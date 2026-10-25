package main

// P2P punch（观战第二期 P2P-a，见 work/plans/OBSERVER-P2P-PHASE2-20261025.md）：
// 中继只做两件事——0xF024 控制面介绍同房观战对（端点互换+一次性配对 token），
// UDP 上应答问候让客户端得知自己的公网映射（迷你 STUN）。探测与后续直连数据
// 全部客户端对客户端，中继不承载观战画面字节。
//
// 锁序：s.mu → punch.mu，任何路径不得反向。punchConnState 字段守卫：
// begins/beginWin 只在 s.mu 下动；capable/endpoint/helloAt/lastPair 只在 punch.mu 下动。
// UDP 监听端口为冷配置：启动时读取，改端口需重启进程。

import (
	"crypto/rand"
	"encoding/binary"
	"errors"
	"net"
	"sync"
	"sync/atomic"
	"time"
)

const (
	punchHelloTokenTTL = 5 * time.Minute
	punchPairTTL       = 120 * time.Second
	punchProbeWindow   = 3 * time.Second
	punchBeginLimit    = 10 // 每连接 10 秒内最多发起配对数
	punchBeginWindow   = 10 * time.Second
	punchHelloPerIPMax = 60 // 每 IP 10 秒内最多问候包（防放大/洪水）
	punchFloodWindow   = 10 * time.Second
)

var errPunchPort = errors.New("punch udp port out of range")

// punchEndpoint 本批只打 IPv4 洞；v6 直连留待后续批次。
type punchEndpoint struct {
	ip   [4]byte
	port uint16
}

type punchConnState struct {
	revision uint32
	capable  bool           // punch.mu：0xF024 握手完成
	endpoint *punchEndpoint // punch.mu：UDP 问候学到的公网映射
	helloAt  time.Time      // punch.mu
	lastPair [16]byte       // punch.mu：最近一次收到的配对 token
	begins   int            // s.mu：限流计数
	beginWin time.Time      // s.mu
}

type helloGrant struct {
	c       *Conn
	expires time.Time
}

type punchPair struct {
	source, viewer             *Conn
	watch, stream, epoch, term uint32
	sourceOK, viewerOK         bool
	token                      [16]byte
	room                       string
	viewerSeat                 uint8
	sourceSeat                 uint8
	direct                     bool // 观看端报告 ok 才置位
	expires                    time.Time
}

type punchHub struct {
	mu         sync.Mutex
	udp        *net.UDPConn
	hellos     map[[16]byte]*helloGrant
	pairs      map[[16]byte]*punchPair
	capable    atomic.Uint64
	hellosSeen atomic.Uint64
	offers     atomic.Uint64
	opened     atomic.Uint64
	failed     atomic.Uint64
}

func (s *Server) startPunchUDP(port int) error {
	// 0 = 系统随机端口，仅供测试；线上端口由运营配置校验（1..65535）。
	if port < 0 || port > 65535 {
		return errPunchPort
	}
	conn, err := net.ListenUDP("udp", &net.UDPAddr{Port: port})
	if err != nil {
		return err
	}
	s.punch.udp = conn
	s.punch.hellos = make(map[[16]byte]*helloGrant)
	s.punch.pairs = make(map[[16]byte]*punchPair)
	go s.punchUDPLoop(conn)
	return nil
}

// punchUDPLoop 只应答问候；探测/数据包是客户端对客户端，落到这里一律丢弃。
// 每 IP 限流防放大：应答 32B 对 32B 请求，1:1 无增益，限流只防洪水。
func (s *Server) punchUDPLoop(conn *net.UDPConn) {
	buf := make([]byte, 1500)
	flood := make(map[[4]byte]int)
	var window time.Time
	for {
		n, addr, err := conn.ReadFromUDP(buf)
		if err != nil {
			return
		}
		now := time.Now()
		if now.Sub(window) >= punchFloodWindow {
			window = now
			flood = make(map[[4]byte]int)
		}
		ip4 := udpIPv4(addr)
		if ip4 == nil {
			continue
		}
		if _, known := flood[*ip4]; !known && len(flood) >= 4096 {
			continue
		}
		flood[*ip4]++
		if flood[*ip4] > punchHelloPerIPMax {
			continue
		}
		s.punchHandleUDP(conn, addr, buf[:n])
	}
}

func (s *Server) punchHandleUDP(conn *net.UDPConn, addr *net.UDPAddr, p []byte) {
	if !s.operations().Config.PunchEnable {
		return
	}
	if len(p) != punchPacketSize || p[0] != punchMagic0 || p[1] != punchMagic1 || p[2] != punchMagic2 || p[3] != punchMagic3 {
		return
	}
	if p[4] != punchUDPHello {
		return
	}
	var token [16]byte
	copy(token[:], p[5:21])
	ip4 := udpIPv4(addr)
	if ip4 == nil {
		return
	}
	s.punch.mu.Lock()
	grant := s.punch.hellos[token]
	if grant == nil || time.Now().After(grant.expires) || grant.c.left.Load() || grant.c.replaced.Load() {
		s.punch.mu.Unlock()
		return
	}
	endpoint := &punchEndpoint{ip: *ip4, port: uint16(addr.Port)}
	grant.c.punch.endpoint = endpoint
	grant.c.punch.helloAt = time.Now()
	s.punch.hellosSeen.Add(1)
	port := uint16(addr.Port)
	s.punch.mu.Unlock()

	ack := make([]byte, punchPacketSize)
	ack[0], ack[1], ack[2], ack[3] = punchMagic0, punchMagic1, punchMagic2, punchMagic3
	ack[4] = punchUDPHelloAck
	copy(ack[5:21], token[:])
	copy(ack[25:29], ip4[:])
	binary.LittleEndian.PutUint16(ack[29:], port)
	conn.WriteToUDP(ack, addr)
}

// punchCapable 只要求已进房（dispatch 保证）；每个连接重复发送会重发问候 token。
func (s *Server) punchCapable(c *Conn) {
	if !s.operations().Config.PunchEnable || c.room.Load() == nil || s.punch.udp == nil {
		return
	}
	var token [16]byte
	if _, err := rand.Read(token[:]); err != nil {
		return
	}
	s.punch.mu.Lock()
	if s.punch.hellos == nil {
		s.punch.hellos = make(map[[16]byte]*helloGrant)
	}
	for key, g := range s.punch.hellos {
		if g.c == c {
			delete(s.punch.hellos, key)
		}
	}
	s.punch.hellos[token] = &helloGrant{c: c, expires: time.Now().Add(punchHelloTokenTTL)}
	c.punch.capable = true
	port := 0
	if s.punch.udp != nil {
		port = s.punch.udp.LocalAddr().(*net.UDPAddr).Port
	}
	s.punch.mu.Unlock()
	s.punch.capable.Add(1)
	out := make([]byte, 24)
	out[0] = punchOpCapable
	binary.LittleEndian.PutUint32(out[1:], c.punch.revision)
	binary.LittleEndian.PutUint16(out[5:], uint16(port))
	out[7] = 1 // flags bit0：UDP 问候通道在听
	copy(out[8:], token[:])
	s.enqueue(c, encodeFrame(msgCliPunch, out))
}

// handlePunchControl 0xF024 控制面入口。坏长度/坏版本一律静默且回 malformed
// 计数——不给探测方任何回显，客户端 3 秒探测窗自己超时回落快照档。
func (s *Server) handlePunchControl(c *Conn, payload []byte) {
	if !s.operations().Config.PunchEnable {
		return
	}
	if c.build < 96 || len(payload) < 1 {
		s.stats.malformed.Add(1)
		return
	}
	switch payload[0] {
	case punchOpCapable:
		if len(payload) != 5 || (binary.LittleEndian.Uint32(payload[1:5]) != punchRevision && binary.LittleEndian.Uint32(payload[1:5]) != 2) {
			s.stats.malformed.Add(1)
			return
		}
		s.punch.mu.Lock()
		c.punch.revision = binary.LittleEndian.Uint32(payload[1:5])
		s.punch.mu.Unlock()
		s.punchCapable(c)
	case punchOpBegin:
		if len(payload) == 14 {
			s.punchBeginContext(c, payload[1], binary.LittleEndian.Uint32(payload[2:]), binary.LittleEndian.Uint32(payload[6:]), binary.LittleEndian.Uint32(payload[10:]))
			return
		}
		if len(payload) != 2 {
			s.stats.malformed.Add(1)
			return
		}
		s.punchBegin(c, payload[1])
	case punchOpResult:
		if len(payload) == 21 {
			var token [16]byte
			copy(token[:], payload[5:])
			s.punchResultToken(c, payload[1], payload[2], binary.LittleEndian.Uint16(payload[3:]), &token)
			return
		}
		if len(payload) != 5 {
			s.stats.malformed.Add(1)
			return
		}
		s.punchResult(c, payload[1], payload[2], binary.LittleEndian.Uint16(payload[3:5]))
	default:
		s.stats.malformed.Add(1)
	}
}

func punchOfferBody(peerSeat uint8, e *punchEndpoint, token *[16]byte) []byte {
	out := make([]byte, 26) // op1 + seat1 + ip4 + port2 + token16 + ttl2
	out[0] = punchOpBegin
	out[1] = peerSeat
	copy(out[2:], e.ip[:])
	binary.LittleEndian.PutUint16(out[6:], e.port)
	copy(out[8:], token[:])
	binary.LittleEndian.PutUint16(out[24:], uint16(punchPairTTL/time.Second))
	return out
}

// punchBegin 同房鉴权在 s.mu 下做；配对 token 签发在 punch.mu 下做。两侧端点
// 未知/未握手一律静默回绝——客户端 3 秒探测窗自己超时回落，不给攻击方回显。
func (s *Server) punchBegin(c *Conn, targetSeat uint8) { s.punchBeginContext(c, targetSeat, 0, 0, 0) }
func (s *Server) punchBeginContext(c *Conn, targetSeat uint8, watch, stream, epoch uint32) {
	if targetSeat < 1 || targetSeat > maxPlayers || targetSeat == c.seat {
		return
	}
	r := c.room.Load()
	if r == nil {
		return
	}
	now := time.Now()
	s.mu.Lock()
	var source *Conn
	if r.bySeat[c.seat] == c {
		source = r.bySeat[targetSeat]
	}
	if source == nil || source == c || r.bySeat[source.seat] != source ||
		c.left.Load() || c.replaced.Load() || source.left.Load() || source.replaced.Load() ||
		r.roundWaitMask&(1<<source.seat) != 0 { // 重连等待席可以被看不能发起直连配对
		s.mu.Unlock()
		return
	}
	if watch != 0 {
		v := r.observer[c.seat]
		if c.build < 97 || source.build < 97 || v == nil || v.viewer != c || v.source != source || v.watch != watch || v.stream != stream || v.epoch != epoch {
			s.mu.Unlock()
			return
		}
	}
	if now.Sub(c.punch.beginWin) > punchBeginWindow {
		c.punch.beginWin = now
		c.punch.begins = 0
	}
	c.punch.begins++
	if c.punch.begins > punchBeginLimit {
		s.mu.Unlock()
		return
	}
	s.punch.mu.Lock()
	vEnd, sEnd := c.punch.endpoint, source.punch.endpoint
	if !c.punch.capable || !source.punch.capable || vEnd == nil || sEnd == nil {
		s.punch.mu.Unlock()
		s.mu.Unlock()
		return
	}
	if watch != 0 && (c.punch.revision != 2 || source.punch.revision != 2) {
		s.punch.mu.Unlock()
		s.mu.Unlock()
		return
	}
	count := 0
	for tok, p := range s.punch.pairs {
		stale := false
		if p.room == r.code && p.watch != 0 {
			v := r.observer[p.viewerSeat]
			stale = v == nil || v.viewer != p.viewer || v.source != p.source || v.watch != p.watch || v.stream != p.stream || v.epoch != p.epoch || v.term != p.term
		}
		if stale || now.After(p.expires) || (p.viewer == c && p.watch == watch) {
			delete(s.punch.pairs, tok)
			continue
		}
		if p.source == source && p.watch != 0 {
			count++
		}
	}
	if watch != 0 && (count >= 3 || len(s.punch.pairs) >= 2048) {
		s.punch.mu.Unlock()
		s.mu.Unlock()
		return
	}
	pair := &punchPair{source: source, viewer: c, watch: watch, stream: stream, epoch: epoch, term: r.flow.term, room: r.code, viewerSeat: c.seat, sourceSeat: source.seat, expires: now.Add(punchPairTTL)}
	if _, err := rand.Read(pair.token[:]); err != nil {
		s.punch.mu.Unlock()
		s.mu.Unlock()
		return
	}
	s.punch.pairs[pair.token] = pair
	c.punch.lastPair = pair.token
	source.punch.lastPair = pair.token
	s.punch.mu.Unlock()
	s.mu.Unlock()
	s.punch.offers.Add(2)
	offer := func(seat uint8, e *punchEndpoint) []byte {
		out := punchOfferBody(seat, e, &pair.token)
		if watch != 0 {
			out = binary.LittleEndian.AppendUint32(out, watch)
			out = binary.LittleEndian.AppendUint32(out, stream)
			out = binary.LittleEndian.AppendUint32(out, epoch)
			out = binary.LittleEndian.AppendUint32(out, pair.term)
		}
		return out
	}
	s.enqueue(c, encodeFrame(msgCliPunch, offer(source.seat, sEnd)))
	s.enqueue(source, encodeFrame(msgCliPunch, offer(c.seat, vEnd)))
}

// punchResult 客户端回报探测结果：观看端 ok 才把配对记为直连（保守——任何一方
// 失败都留在快照档）。计数进 status 页，跑出真实打通率分布。
func (s *Server) punchResult(c *Conn, targetSeat, okFlag uint8, rttMs uint16) {
	s.punchResultToken(c, targetSeat, okFlag, rttMs, nil)
}
func (s *Server) punchResultToken(c *Conn, targetSeat, okFlag uint8, rttMs uint16, token *[16]byte) {
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil || r.bySeat[c.seat] != c {
		return
	}
	s.punch.mu.Lock()
	defer s.punch.mu.Unlock()
	key := c.punch.lastPair
	if token != nil {
		key = *token
	}
	p := s.punch.pairs[key]
	if p == nil || !time.Now().Before(p.expires) || p.room != r.code {
		return
	}
	viewer := p.viewerSeat == c.seat && p.sourceSeat == targetSeat
	source := p.sourceSeat == c.seat && p.viewerSeat == targetSeat
	if !viewer && !source {
		return
	}
	if p.viewer != nil && p.viewer != c && p.source != c {
		return
	}
	if p.watch != 0 {
		v := r.observer[p.viewerSeat]
		if v == nil || v.watch != p.watch || v.stream != p.stream || v.epoch != p.epoch || v.term != p.term {
			return
		}
	}
	if okFlag == 1 && rttMs <= 800 {
		if viewer && !p.viewerOK {
			s.punch.opened.Add(1)
			p.viewerOK = true
		}
		if source && !p.sourceOK {
			s.punch.opened.Add(1)
			p.sourceOK = true
		}
		p.direct = p.viewerOK && p.sourceOK
	} else {
		s.punch.failed.Add(1)
		p.direct = false
	}
}

// punchDrop 连接退场清理；removeConn 持 s.mu 调用（锁序 s.mu → punch.mu）。
func (s *Server) punchDrop(r *Room, c *Conn) {
	seat := c.seat
	code := r.code
	s.punch.mu.Lock()
	defer s.punch.mu.Unlock()
	c.punch.capable = false
	c.punch.endpoint = nil
	for tok, g := range s.punch.hellos {
		if g.c == c {
			delete(s.punch.hellos, tok)
		}
	}
	for tok, p := range s.punch.pairs {
		if p.room == code && (p.viewerSeat == seat || p.sourceSeat == seat) {
			delete(s.punch.pairs, tok)
		}
	}
}

func (s *Server) punchSweepLoop() {
	t := time.NewTicker(30 * time.Second)
	defer t.Stop()
	for range t.C {
		now := time.Now()
		s.punch.mu.Lock()
		for tok, g := range s.punch.hellos {
			if now.After(g.expires) {
				delete(s.punch.hellos, tok)
			}
		}
		for tok, p := range s.punch.pairs {
			if now.After(p.expires) {
				delete(s.punch.pairs, tok)
			}
		}
		s.punch.mu.Unlock()
	}
}

func udpIPv4(addr *net.UDPAddr) *[4]byte {
	v4 := addr.IP.To4()
	if v4 == nil || len(v4) != 4 {
		return nil
	}
	var out [4]byte
	copy(out[:], v4)
	return &out
}
