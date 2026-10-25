package main

import (
	"encoding/binary"
	"net"
	"testing"
	"time"
)

// punchTestServer 起一个带打洞 UDP（随机端口）的测试中继。
func punchTestServer(t *testing.T) (*Server, string, int) {
	t.Helper()
	s, addr := startTestServer(t, nil)
	if err := s.startPunchUDP(0); err != nil {
		t.Fatalf("start punch udp: %v", err)
	}
	t.Cleanup(func() { s.punch.udp.Close() })
	return s, addr, s.punch.udp.LocalAddr().(*net.UDPAddr).Port
}

func punchCapableReq() []byte {
	p := make([]byte, 5)
	p[0] = punchOpCapable
	binary.LittleEndian.PutUint32(p[1:], punchRevision)
	return p
}

// readPunch 读到下一条指定 op 的 0xF024 帧（跳过路线上的其他帧）。
func readPunch(c *testClient, op byte, size int) []byte {
	c.t.Helper()
	for i := 0; i < 30; i++ {
		f := c.next(time.Second)
		if f.typ != msgCliPunch {
			continue
		}
		if len(f.payload) != size || f.payload[0] != op {
			c.t.Fatalf("bad punch reply op=%d len=%d", f.payload[0], len(f.payload))
		}
		return f.payload
	}
	c.t.Fatal("punch reply absent")
	return nil
}

// punchHello 用独立 UDP socket 发问候并等应答，返回应答包与本地端点。
func punchHello(t *testing.T, port int, token []byte) ([]byte, *net.UDPAddr) {
	t.Helper()
	sock, err := net.DialUDP("udp", nil, &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: port})
	if err != nil {
		t.Fatalf("dial udp: %v", err)
	}
	defer sock.Close()
	pkt := make([]byte, punchPacketSize)
	pkt[0], pkt[1], pkt[2], pkt[3] = punchMagic0, punchMagic1, punchMagic2, punchMagic3
	pkt[4] = punchUDPHello
	copy(pkt[5:21], token)
	if _, err := sock.Write(pkt); err != nil {
		t.Fatalf("send hello: %v", err)
	}
	buf := make([]byte, 1500)
	sock.SetReadDeadline(time.Now().Add(2 * time.Second))
	n, _, err := sock.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("hello ack: %v", err)
	}
	return buf[:n], sock.LocalAddr().(*net.UDPAddr)
}

func punchOffer(c *testClient, wantPeerSeat uint8) ([16]byte, *net.UDPAddr) {
	c.t.Helper()
	p := readPunch(c, punchOpBegin, 26)
	if p[1] != wantPeerSeat {
		c.t.Fatalf("offer peerSeat=%d want %d", p[1], wantPeerSeat)
	}
	var token [16]byte
	copy(token[:], p[8:])
	ep := &net.UDPAddr{IP: net.IP(p[2:6]), Port: int(binary.LittleEndian.Uint16(p[6:]))}
	return token, ep
}

func TestPunchCapableUDPHelloAndOffer(t *testing.T) {
	s, tcpAddr, udpPort := punchTestServer(t)
	h, g := newTestClient(t, tcpAddr), newTestClient(t, tcpAddr)
	code := decodeWelcome(t, h.create(1, 96, "source")).code
	g.join(1, 96, "viewer", code)

	// 能力握手：ACK 必须带回端口与问候 token；随后真发 UDP 问候，中继记录端点。
	for _, c := range []*testClient{h, g} {
		c.send(msgCliPunch, punchCapableReq())
		ack := readPunch(c, punchOpCapable, 24)
		if binary.LittleEndian.Uint32(ack[1:]) != punchRevision || ack[7] != 1 {
			t.Fatal("capable ack malformed")
		}
		var token [16]byte
		copy(token[:], ack[8:])
		helloAck, local := punchHello(t, udpPort, token[:])
		if len(helloAck) != punchPacketSize || helloAck[4] != punchUDPHelloAck || string(helloAck[5:21]) != string(token[:]) {
			t.Fatal("hello ack malformed")
		}
		if string(helloAck[25:29]) != string([]byte{127, 0, 0, 1}) || binary.LittleEndian.Uint16(helloAck[29:]) != uint16(local.Port) {
			t.Fatalf("ack endpoint != local %v", local)
		}
	}

	// 配对：两侧各收到对方公网端点与同一 token。
	g.send(msgCliPunch, []byte{punchOpBegin, 1})
	gToken, offer2h := punchOffer(g, 1)
	hToken, offer2g := punchOffer(h, 2)
	if gToken != hToken {
		t.Fatal("pair token mismatch")
	}
	if offer2h.Port == 0 || offer2g.Port == 0 || (offer2h.IP.Equal(offer2g.IP) && offer2h.Port == offer2g.Port) {
		t.Fatalf("offer endpoints wrong: %v %v", offer2h, offer2g)
	}

	// 双方回报 ok → 配对记直连；计数进状态页。
	g.send(msgCliPunch, []byte{punchOpResult, 1, 1, 12, 0})
	h.send(msgCliPunch, []byte{punchOpResult, 2, 1, 15, 0})
	deadline := time.Now().Add(time.Second)
	for time.Now().Before(deadline) && s.punch.opened.Load() < 2 {
		time.Sleep(time.Millisecond)
	}
	if s.punch.opened.Load() != 2 || s.punch.offers.Load() != 2 || s.punch.capable.Load() != 2 || s.punch.hellosSeen.Load() != 2 {
		t.Fatalf("counters capable=%d hellos=%d offers=%d opened=%d", s.punch.capable.Load(), s.punch.hellosSeen.Load(), s.punch.offers.Load(), s.punch.opened.Load())
	}
	s.punch.mu.Lock()
	var pair *punchPair
	for _, p := range s.punch.pairs {
		pair = p
	}
	s.punch.mu.Unlock()
	if pair == nil || !pair.direct || pair.viewerSeat != 2 || pair.sourceSeat != 1 {
		t.Fatal("pair not marked direct")
	}
}

func TestPunchGatingAndCleanup(t *testing.T) {
	s, _, _ := punchTestServer(t)
	s.punch.mu.Lock()
	s.punch.hellos = make(map[[16]byte]*helloGrant)
	s.punch.pairs = make(map[[16]byte]*punchPair)
	s.punch.mu.Unlock()
	r := &Room{}
	a := &Conn{srv: s, seat: 1, build: 96, send: make(chan outMsg, 64), done: make(chan struct{})}
	b := &Conn{srv: s, seat: 2, build: 96, send: make(chan outMsg, 64), done: make(chan struct{})}
	a.room.Store(r)
	b.room.Store(r)
	r.bySeat[1] = a
	r.bySeat[2] = b

	// 计数前的门（坏席位/指自己/等待席当源）不耗预算，之后恰好 punchBeginLimit 次放行。
	// 注意：能力/端点门在计数之后，用独立连接验证，不占用 b 的预算。
	// handlePunchControl 内部会取 s.mu，这里不能持锁调用。
	a.punch.capable = true
	b.punch.capable = true
	a.punch.endpoint = &punchEndpoint{ip: [4]byte{1, 2, 3, 4}, port: 100}
	b.punch.endpoint = &punchEndpoint{ip: [4]byte{5, 6, 7, 8}, port: 200}
	s.mu.Lock()
	r.roundWaitMask = 1 << 1 // 源席 a 在等待席：拒绝
	s.mu.Unlock()
	s.handlePunchControl(b, []byte{punchOpBegin, 1})
	if len(b.send) != 0 {
		t.Fatal("waiting source accepted")
	}
	s.mu.Lock()
	r.roundWaitMask = 0
	s.mu.Unlock()
	for i := 0; i < punchBeginLimit; i++ {
		s.handlePunchControl(b, []byte{punchOpBegin, 1})
	}
	if len(b.send) != punchBeginLimit || len(a.send) != punchBeginLimit {
		t.Fatalf("offers %d/%d", len(b.send), len(a.send))
	}
	s.handlePunchControl(b, []byte{punchOpBegin, 1})
	if len(b.send) != punchBeginLimit {
		t.Fatal("begin rate limit bypassed")
	}

	// 能力门在计数后：未握手连接发起 → 计数但无 offer。坏席位/指自己先于计数，一并静默。
	c3 := &Conn{srv: s, seat: 3, build: 96, send: make(chan outMsg, 64), done: make(chan struct{})}
	c3.room.Store(r)
	r.bySeat[3] = c3
	s.handlePunchControl(c3, []byte{punchOpBegin, 0})
	s.handlePunchControl(c3, []byte{punchOpBegin, 7})
	s.handlePunchControl(c3, []byte{punchOpBegin, 3}) // 指自己
	s.handlePunchControl(c3, []byte{punchOpBegin, 1})
	if len(c3.send) != 0 {
		t.Fatal("offer without capable handshake")
	}
	// 端点门：握手过但没有 UDP 问候端点 → 仍无 offer。
	s.handlePunchControl(c3, punchCapableReq())
	if f := <-c3.send; len(f.frame) == 0 {
		t.Fatal("ack missing")
	}
	s.handlePunchControl(c3, []byte{punchOpBegin, 1})
	if len(c3.send) != 0 {
		t.Fatal("offer without hello endpoint")
	}
	r.bySeat[3] = nil

	// 退场清理：问候与配对随连接消失（给 a 补一次真实握手产生问候授权）。
	s.handlePunchControl(a, punchCapableReq())
	var grantForA bool
	s.punch.mu.Lock()
	for _, g := range s.punch.hellos {
		if g.c == a {
			grantForA = true
		}
	}
	s.punch.mu.Unlock()
	if !grantForA {
		t.Fatal("hello grant missing before drop")
	}
	s.mu.Lock()
	s.punchDrop(r, a)
	s.mu.Unlock()
	s.punch.mu.Lock()
	defer s.punch.mu.Unlock()
	for _, g := range s.punch.hellos {
		if g.c == a {
			t.Fatal("hello grant survived drop")
		}
	}
	if len(s.punch.pairs) != 0 {
		t.Fatal("pairs survived drop")
	}
}

func TestPunchUDPRejectsGarbage(t *testing.T) {
	s, _, udpPort := punchTestServer(t)
	sock, err := net.DialUDP("udp", nil, &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: udpPort})
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	defer sock.Close()
	bad := [][]byte{
		make([]byte, 16),              // 短包
		make([]byte, punchPacketSize), // 错 magic
		{punchMagic0, punchMagic1, punchMagic2, punchMagic3, punchUDPProbe}, // 探测包落中继
	}
	pkt := make([]byte, punchPacketSize)
	pkt[0], pkt[1], pkt[2], pkt[3] = punchMagic0, punchMagic1, punchMagic2, punchMagic3
	pkt[4] = punchUDPHello
	copy(pkt[5:21], "unknown-token-xxxx") // 未签发 token
	bad = append(bad, pkt)
	for _, p := range bad {
		sock.Write(p)
	}
	sock.SetReadDeadline(time.Now().Add(150 * time.Millisecond))
	buf := make([]byte, 1500)
	if n, _, err := sock.ReadFromUDP(buf); err == nil {
		t.Fatalf("garbage answered with %d bytes", n)
	}
	if s.punch.hellosSeen.Load() != 0 {
		t.Fatal("garbage counted as hello")
	}
}
