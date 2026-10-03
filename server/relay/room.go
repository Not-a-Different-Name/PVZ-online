// @pvz-online: M3 Go 中继服务器 —— 房间与分发。
// 房间 = 一张席位表（1..6 号位 → 连接）。房主（建房者）随时可能坐在任意席位
// （换过位的话），"房主走了"按连接认，不按席位认。
// 并发模型：所有房间/席位状态改动都在 s.mu 下做；socket 写一律走连接的写队列，
// 绝不在这里直接写 socket。
package main

import (
	"math/rand"
	"strings"
	"sync"
	"time"
)

type Room struct {
	code   string
	host   *Conn
	bySeat [maxPlayers + 1]*Conn
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

// hostSeat 房主当前坐在哪个席位（换过位就不是 1 了）。调用方持 s.mu。
func (r *Room) hostSeat() uint8 {
	for i := 1; i <= maxPlayers; i++ {
		if r.bySeat[i] == r.host {
			return uint8(i)
		}
	}
	return 1
}

// lowestFreeSeat 最小空闲席位；满员返回 0。调用方持 s.mu。
func (r *Room) lowestFreeSeat() uint8 {
	for i := 1; i <= maxPlayers; i++ {
		if r.bySeat[i] == nil {
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
	logf         func(format string, args ...any)
}

func newServer(logf func(string, ...any)) *Server {
	return &Server{
		rooms:        make(map[string]*Room),
		pingInterval: time.Second,
		idleTimeout:  10 * time.Second,
		logf:         logf,
	}
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
	r := &Room{code: code, host: c}
	r.bySeat[1] = c
	c.build = build
	copy(c.name[:], name)
	c.seat = 1
	c.room.Store(r)
	s.rooms[code] = r
	s.enqueue(c, encodeFrame(msgSrvWelcome, encodeWelcome(1, 1, code, r.roster())))
	s.logf("room %s created by %s (build %d)", code, c.remoteAddr(), build)
}

// handleJoin 加入：码不区分大小写；取最小空闲席位；回 WELCOME（含全部名册）并广播 PEER_JOIN。
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
	seat := r.lowestFreeSeat()
	if seat == 0 {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRoomFull)))
		return
	}
	c.build = build
	copy(c.name[:], name)
	c.seat = seat
	c.room.Store(r)
	r.bySeat[seat] = c
	s.enqueue(c, encodeFrame(msgSrvWelcome, encodeWelcome(seat, r.hostSeat(), code, r.roster())))
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
	r.broadcast(s, nil, encodeFrame(msgSrvSeatSwap, encodeSeatSwap(a, b)))
	s.logf("room %s: seats %d and %d swapped", r.code, a, b)
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

// removeConn 连接断了/退房：清席位、广播离开；房主走了就解散整房。
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
	if r.host == c {
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
	s.logf("room %s: seat %d left (reason %d)", r.code, seat, reason)
	r.broadcast(s, c, encodeFrame(msgSrvPeerLeave, encodePeerLeave(seat, reason)))
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
