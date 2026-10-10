// @pvz-online: M3 Go 中继服务器 —— 房间与分发。
// 房间 = 一张席位表（1..6 号位 → 连接）。房主（建房者）随时可能坐在任意席位
// （换过位的话），"房主走了"按连接认，不按席位认——但房主坐哪一席由 hostSeat
// 持久记着（房主掉线进保留期时 bySeat 上没有指针，只靠指针找会错位）。
// 掉线的席位进"保留"（holds）：保留期内名字+构建对得上的人可以 REJOIN 坐回来，
// 过期没回来才广播离开（清扫见 sweepOnce）。
// 并发模型：所有房间/席位状态改动都在 s.mu 下做；socket 写一律走连接的写队列，
// 绝不在这里直接写 socket。
package main

import (
	"math/rand"
	"strings"
	"sync"
	"time"
)

// seatHold 一个掉线席位的保留：谁（名字+构建）在什么时候之前可以坐回来。
type seatHold struct {
	name  [nameSize]byte
	build uint16
	until time.Time
}

type Room struct {
	code     string
	host     *Conn
	hostSeat uint8 // 房主坐哪一席（建房 = 1；换位跟着换；房主重连时重绑）
	bySeat   [maxPlayers + 1]*Conn
	// 席位保留：非体面断开的席位在这儿挂着，等本人 REJOIN 回来。
	// until 零值 = 没保留。hostSeat 的保留过期 = 房主不会回来了，散房。
	holds [maxPlayers + 1]seatHold
}

// roster 按席位升序列名册（含房主在内的所有上座席位）。调用方持 s.mu。
func (r *Room) roster() []rosterEntry {
	out := make([]rosterEntry, 0, maxPlayers)
	for i := 1; i <= maxPlayers; i++ {
		if c := r.bySeat[i]; c != nil {
			out = append(out, rosterEntry{seat: uint8(i), build: c.build, name: c.name})
		}
	}
	return out
}

// holdActive 这个席位的保留还没过期？调用方持 s.mu。
func (r *Room) holdActive(seat uint8, now time.Time) bool {
	h := r.holds[seat]
	return !h.until.IsZero() && now.Before(h.until)
}

// lowestFreeSeat 最小空闲席位；满员返回 0。调用方持 s.mu。
// 未过期的保留席位视为被占（本人要回来坐的）；顺手清掉碰见的过期保留——
// 只是懒清扫，过期保留的"广播离开/散房"由 sweepOnce 负责。
func (r *Room) lowestFreeSeat(now time.Time) uint8 {
	for i := 1; i <= maxPlayers; i++ {
		if r.bySeat[i] != nil {
			continue
		}
		if !r.holds[i].until.IsZero() {
			if now.Before(r.holds[i].until) {
				continue
			}
			r.holds[i] = seatHold{}
		}
		return uint8(i)
	}
	return 0
}

// reclaimSeatFor 找一个"名字+构建对得上"的未过期保留席位（本人换了新进程重进房：
// 普通 JOIN 也认保留位，坐回老席位）。没有就返回 0。调用方持 s.mu。
func (r *Room) reclaimSeatFor(name []byte, build uint16, now time.Time) uint8 {
	for i := 1; i <= maxPlayers; i++ {
		h := r.holds[i]
		if h.until.IsZero() || !now.Before(h.until) || r.bySeat[i] != nil {
			continue
		}
		if h.build == build && string(h.name[:]) == string(name) {
			return uint8(i)
		}
	}
	return 0
}

func (r *Room) broadcast(s *Server, except *Conn, frame []byte) {
	for i := 1; i <= maxPlayers; i++ {
		if c := r.bySeat[i]; c != nil && c != except {
			s.enqueue(c, frame)
		}
	}
}

type Server struct {
	mu    sync.Mutex
	rooms map[string]*Room

	pingInterval time.Duration
	idleTimeout  time.Duration
	holdDuration time.Duration
	logf         func(format string, args ...any)
}

func newServer(logf func(string, ...any)) *Server {
	return &Server{
		rooms:        make(map[string]*Room),
		pingInterval: time.Second,
		idleTimeout:  30 * time.Second,
		holdDuration: 60 * time.Second,
		logf:         logf,
	}
}

// holdGrace 保留秒数（PEER_OFFLINE 载荷，进 u8）。
func (s *Server) holdGrace() uint8 {
	grace := s.holdDuration / time.Second
	if grace > 255 {
		grace = 255
	}
	return uint8(grace)
}

func validRoomCode(code string) bool {
	if len(code) != roomCodeLen {
		return false
	}
	for i := 0; i < len(code); i++ {
		if !strings.ContainsRune(roomAlphabet, rune(code[i])) {
			return false
		}
	}
	return true
}

// newRoomCode 生成一个没被占用的房间码。调用方持 s.mu。
func (s *Server) newRoomCode() (string, bool) {
	for attempt := 0; attempt < 32; attempt++ {
		b := make([]byte, roomCodeLen)
		for i := range b {
			b[i] = roomAlphabet[rand.Intn(len(roomAlphabet))]
		}
		code := string(b)
		if _, exists := s.rooms[code]; !exists {
			return code, true
		}
	}
	return "", false
}

// handleCreate 建房：发房间码 + 1 号席位，回 WELCOME。已在房间里的连接再来建房直接无视。
func (s *Server) handleCreate(c *Conn, version, build uint16, name []byte) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if c.room.Load() != nil {
		return
	}
	if version != protocolVersion {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectProtocolVersion)))
		return
	}
	code, ok := s.newRoomCode()
	if !ok {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectServerBusy)))
		return
	}
	r := &Room{code: code, host: c, hostSeat: 1}
	r.bySeat[1] = c
	c.build = build
	copy(c.name[:], name)
	c.seat = 1
	c.room.Store(r)
	s.rooms[code] = r
	s.enqueue(c, encodeFrame(msgSrvWelcome, encodeWelcome(1, 1, code, r.roster())))
	s.logf("room %s created by %s (build %d)", code, c.remoteAddr(), build)
}

// handleJoin 加入：码不区分大小写；优先坐回自己的保留位（名字+构建对得上的掉线席位），
// 否则取最小空闲席位（跳过别人的保留位）；回 WELCOME（含全部名册）并广播 PEER_JOIN。
func (s *Server) handleJoin(c *Conn, version, build uint16, name, rawCode []byte) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if c.room.Load() != nil {
		return
	}
	if version != protocolVersion {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectProtocolVersion)))
		return
	}
	code := strings.ToUpper(string(rawCode))
	if !validRoomCode(code) {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectBadCode)))
		return
	}
	r := s.rooms[code]
	if r == nil {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRoomNotFound)))
		return
	}
	now := time.Now()
	seat := r.reclaimSeatFor(name, build, now)
	if seat != 0 {
		r.holds[seat] = seatHold{} // 本人回来了，保留消费掉
	} else {
		seat = r.lowestFreeSeat(now)
	}
	if seat == 0 {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRoomFull)))
		return
	}
	c.build = build
	copy(c.name[:], name)
	c.seat = seat
	c.room.Store(r)
	r.bySeat[seat] = c
	s.enqueue(c, encodeFrame(msgSrvWelcome, encodeWelcome(seat, r.hostSeat, code, r.roster())))
	r.broadcast(s, c, encodeFrame(msgSrvPeerJoin, encodePeerJoin(rosterEntry{seat: seat, build: build, name: c.name})))
	s.logf("room %s: %s joined seat %d (build %d)", code, c.remoteAddr(), seat, build)
}

// handleSwapCommit 换位落实：交换两席的连接映射，然后全房广播（含提交者自己）——
// 所有人只认这条广播执行置换，天然免双重应用。
func (s *Server) handleSwapCommit(c *Conn, a, b uint8) {
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil {
		return
	}
	if a < 1 || a > maxPlayers || b < 1 || b > maxPlayers || a == b {
		return
	}
	ca, cb := r.bySeat[a], r.bySeat[b]
	if ca == nil || cb == nil {
		return
	}
	if c != ca && c != cb {
		s.logf("room %s: seat %d tried to swap seats %d/%d it doesn't own - dropped", r.code, c.seat, a, b)
		return
	}
	r.bySeat[a], r.bySeat[b] = cb, ca
	ca.seat, cb.seat = b, a
	if r.hostSeat == a {
		r.hostSeat = b
	} else if r.hostSeat == b {
		r.hostSeat = a
	}
	r.broadcast(s, nil, encodeFrame(msgSrvSeatSwap, encodeSeatSwap(a, b)))
	s.logf("room %s: seats %d and %d swapped", r.code, a, b)
}

// handleRejoin 重连受理（MOD_BUILD 39）。两种情形：
//   - 快路径（旧连接还挂着，多半是抖动）：名字+构建对得上就静默换掉旧连接，
//     不发 PEER_OFFLINE——队友全程无感；广播 PEER_BACK 让大家把状态补给它。
//   - 保留路径（席位在保留期内）：核对名字+构建，消费保留，回 WELCOME，广播 PEER_BACK。
//
// 拒绝分两种：名字/构建对不上 = REJOIN_SEAT_MISMATCH（防顶号）；
// 房间没了/席位既没人也没保留 = REJOIN_UNAVAILABLE。
func (s *Server) handleRejoin(c *Conn, version, build uint16, name, rawCode []byte, seat uint8) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if c.room.Load() != nil {
		return
	}
	if version != protocolVersion {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectProtocolVersion)))
		return
	}
	code := strings.ToUpper(string(rawCode))
	if !validRoomCode(code) {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectBadCode)))
		return
	}
	r := s.rooms[code]
	if r == nil || seat < 1 || seat > maxPlayers {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRejoinUnavailable)))
		return
	}
	now := time.Now()
	old := r.bySeat[seat]
	fast := old != nil
	if fast {
		if old.build != build || string(old.name[:]) != string(name) {
			s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRejoinSeatMismatch)))
			s.logf("room %s: rejoin seat %d rejected (identity mismatch on live seat)", r.code, seat)
			return
		}
		// 静默替换：旧连接退场时什么都不广播（removeConn 看 replaced；room 置空后
		// 它连 removeConn 的主体都进不来）。
		old.replaced.Store(true)
		old.room.Store(nil)
		old.close()
	} else {
		if !r.holdActive(seat, now) {
			s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRejoinUnavailable)))
			return
		}
		h := r.holds[seat]
		if h.build != build || string(h.name[:]) != string(name) {
			s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRejoinSeatMismatch)))
			s.logf("room %s: rejoin seat %d rejected (identity mismatch on held seat)", r.code, seat)
			return
		}
		r.holds[seat] = seatHold{} // 保留消费掉
	}
	c.build = build
	copy(c.name[:], name)
	c.seat = seat
	c.room.Store(r)
	r.bySeat[seat] = c
	if seat == r.hostSeat {
		r.host = c // 房主归位（快慢路径都可能：房主掉线也进保留期）
	}
	s.enqueue(c, encodeFrame(msgSrvWelcome, encodeWelcome(seat, r.hostSeat, code, r.roster())))
	r.broadcast(s, c, encodeFrame(msgSrvPeerBack, encodePeerBack(rosterEntry{seat: seat, build: build, name: c.name})))
	if fast {
		s.logf("room %s: seat %d reconnected on a live link (old conn replaced)", r.code, seat)
	} else {
		s.logf("room %s: seat %d reconnected from hold", r.code, seat)
	}
}

// forward 游戏帧：校验来源席位（防冒名）后逐字节转给 dst 席位；不合法就丢。
func (s *Server) forward(c *Conn, typ uint16, payload []byte) {
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil {
		return
	}
	src := payload[0]
	if src != c.seat {
		s.logf("dropped %#x with bogus src %d (connection sits at seat %d)", typ, src, c.seat)
		return
	}
	dst := payload[1]
	if dst < 1 || dst > maxPlayers || dst == src {
		s.logf("dropped %#x to bad dst %d", typ, dst)
		return
	}
	t := r.bySeat[dst]
	if t == nil {
		s.logf("dropped %#x to empty seat %d", typ, dst)
		return
	}
	s.enqueue(t, encodeFrame(typ, payload))
}

// removeConn 连接断了/退房：清席位，然后按"怎么断的"分流（MOD_BUILD 39 起）：
//   - replaced（被重连静默换掉）：什么都不广播，席位已归新连接；
//   - left（收到过 LEAVE_ROOM，体面退房）：不设保留——房主散房、其他人广播 PEER_LEAVE；
//   - 其余（掉线/闲死踢/进程崩）：席位进保留期，广播 PEER_OFFLINE；房主同理
//     （房主指针清空，保留过期还没回来才散房，见 sweepRoom）。
//
// 可重入安全：连接已不在房里时直接返回。
func (s *Server) removeConn(c *Conn, reason uint8) {
	c.close()
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil {
		return
	}
	c.room.Store(nil)
	seat := c.seat
	if seat >= 1 && seat <= maxPlayers && r.bySeat[seat] == c {
		r.bySeat[seat] = nil
	}

	if c.replaced.Load() {
		s.logf("room %s: seat %d replaced connection cleaned up silently", r.code, seat)
		return
	}
	graceful := c.left.Load()

	if r.host == c {
		if !graceful {
			// 房主掉线：保留席位，等重连；房间暂时由服务器兜着开
			r.host = nil
			if seat >= 1 && seat <= maxPlayers {
				r.holds[seat] = seatHold{name: c.name, build: c.build, until: time.Now().Add(s.holdDuration)}
				s.logf("room %s: host (seat %d) dropped - holding the room %v for a rejoin", r.code, seat, s.holdDuration)
				r.broadcast(s, nil, encodeFrame(msgSrvPeerOffline, encodePeerOffline(seat, s.holdGrace())))
			}
			return
		}
		s.logf("room %s closed (host left)", r.code)
		for i := 1; i <= maxPlayers; i++ {
			if o := r.bySeat[i]; o != nil {
				o.room.Store(nil)
				s.enqueueAndClose(o, encodeFrame(msgSrvRoomClosed, encodeRoomClosed(roomClosedHostLeft)))
			}
		}
		delete(s.rooms, r.code)
		return
	}

	if graceful {
		s.logf("room %s: seat %d left (reason %d)", r.code, seat, reason)
		r.broadcast(s, c, encodeFrame(msgSrvPeerLeave, encodePeerLeave(seat, reason)))
		return
	}
	if seat >= 1 && seat <= maxPlayers {
		r.holds[seat] = seatHold{name: c.name, build: c.build, until: time.Now().Add(s.holdDuration)}
	}
	s.logf("room %s: seat %d dropped (reason %d) - held %v for a rejoin", r.code, seat, reason, s.holdDuration)
	r.broadcast(s, c, encodeFrame(msgSrvPeerOffline, encodePeerOffline(seat, s.holdGrace())))
}

// sweepLoop 周期性结算过期保留（main 里起一条；测试直接调 sweepOnce，不开这个）。
func (s *Server) sweepLoop() {
	ticker := time.NewTicker(1500 * time.Millisecond)
	defer ticker.Stop()
	for range ticker.C {
		s.sweepOnce(time.Now())
	}
}

// sweepOnce 清扫一遍全部房间：过期的保留结算掉（广播离开 / 房主没回来就散房）、
// 没人也没保留的房间删掉。纯函数（只吃 now），测试直接调，不依赖真实时钟。
func (s *Server) sweepOnce(now time.Time) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for code, r := range s.rooms {
		if s.sweepRoom(r, now) {
			delete(s.rooms, code)
		}
	}
}

// sweepRoom 结算一个房间里过期的保留；返回 true = 这个房间该删了。调用方持 s.mu。
func (s *Server) sweepRoom(r *Room, now time.Time) bool {
	for seat := 1; seat <= maxPlayers; seat++ {
		h := r.holds[seat]
		if h.until.IsZero() || now.Before(h.until) {
			continue
		}
		r.holds[seat] = seatHold{}
		if r.bySeat[seat] != nil {
			continue // 席位已被新人占用（join 的正常回收），保留作废就行
		}
		if uint8(seat) == r.hostSeat && r.host == nil {
			// 房主保留过期还没回来：房间散伙
			s.logf("room %s closed (host never came back from the rejoin hold)", r.code)
			for i := 1; i <= maxPlayers; i++ {
				if o := r.bySeat[i]; o != nil {
					o.room.Store(nil)
					s.enqueueAndClose(o, encodeFrame(msgSrvRoomClosed, encodeRoomClosed(roomClosedHostLeft)))
				}
			}
			return true
		}
		s.logf("room %s: seat %d rejoin window expired - releasing the seat", r.code, seat)
		r.broadcast(s, nil, encodeFrame(msgSrvPeerLeave, encodePeerLeave(uint8(seat), peerLeaveTimeout)))
	}
	return r.empty()
}

// empty 房间里既没有连接、也没有任何保留（该删了）。
func (r *Room) empty() bool {
	if r.host != nil {
		return false
	}
	for i := 1; i <= maxPlayers; i++ {
		if r.bySeat[i] != nil || !r.holds[i].until.IsZero() {
			return false
		}
	}
	return true
}

// enqueue 投一帧进连接的写队列（缓冲队列是每连接唯一的写手，帧才不会交错）。
// 队列满 = 这客户端卡死了：断它。
func (s *Server) enqueue(c *Conn, frame []byte) {
	select {
	case <-c.done:
	case c.send <- outMsg{frame: frame}:
	default:
		s.logf("write queue full for %s - dropping the connection", c.remoteAddr())
		c.close()
	}
}

// enqueueAndClose 投一帧并让写循环写完它再关（ROOM_CLOSED 这类"遗言"帧专用）。
func (s *Server) enqueueAndClose(c *Conn, frame []byte) {
	select {
	case <-c.done:
	case c.send <- outMsg{frame: frame, closeAfter: true}:
	default:
		s.logf("write queue full for %s - dropping the connection", c.remoteAddr())
		c.close()
	}
}
