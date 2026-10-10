// @pvz-online: M3 Go 中继服务器测试。
// 测的是"线上跑的字节"：测试客户端自己组装/解析线格式（和游戏端同一套），
// 所以这里钉住的就是客户端真正看到的契约。
package main

import (
	"errors"
	"fmt"
	"log"
	"net"
	"os"
	"testing"
	"time"
)

// ---- 测试客户端：说和游戏端一模一样的线格式 ----

type testClient struct {
	t  *testing.T
	nc net.Conn
}

type wireFrame struct {
	typ     uint16
	payload []byte
}

func newTestClient(t *testing.T, addr string) *testClient {
	t.Helper()
	nc, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatalf("dial %s: %v", addr, err)
	}
	t.Cleanup(func() { nc.Close() })
	return &testClient{t: t, nc: nc}
}

func (c *testClient) send(typ uint16, payload []byte) {
	c.t.Helper()
	if _, err := c.nc.Write(encodeFrame(typ, payload)); err != nil {
		c.t.Fatalf("write %#x: %v", typ, err)
	}
}

// trySend 给后台 goroutine 用（那里不能调 t.Fatalf）
func (c *testClient) trySend(typ uint16, payload []byte) {
	c.nc.Write(encodeFrame(typ, payload))
}

func (c *testClient) next(timeout time.Duration) wireFrame {
	c.t.Helper()
	c.nc.SetReadDeadline(time.Now().Add(timeout))
	typ, payload, err := readFrame(c.nc)
	if err != nil {
		c.t.Fatalf("expected a frame within %v, got %v", timeout, err)
	}
	return wireFrame{typ, payload}
}

func (c *testClient) expectNothing(d time.Duration) {
	c.t.Helper()
	c.nc.SetReadDeadline(time.Now().Add(d))
	typ, _, err := readFrame(c.nc)
	if err == nil {
		c.t.Fatalf("expected no frame, got %#x", typ)
	}
}

// expectClosed 断言连接已被服务器关掉（读到非超时的错误）。
func (c *testClient) expectClosed(d time.Duration) {
	c.t.Helper()
	c.nc.SetReadDeadline(time.Now().Add(d))
	_, _, err := readFrame(c.nc)
	if err == nil {
		c.t.Fatalf("expected the connection to be closed")
	}
	if errors.Is(err, os.ErrDeadlineExceeded) {
		c.t.Fatalf("connection still open after %v", d)
	}
}

func name16(s string) []byte {
	b := make([]byte, nameSize)
	copy(b, s)
	return b
}

func createPayload(version, build uint16, name string) []byte {
	var s buf
	s.u16(version)
	s.u16(build)
	s.raw(name16(name))
	return s.b
}

func joinPayload(version, build uint16, name, rawCode string) []byte {
	var s buf
	s.u16(version)
	s.u16(build)
	s.raw(name16(name))
	s.raw([]byte(rawCode))
	return s.b
}

// rejoinPayload 重连载荷（MOD_BUILD 39）：和 JOIN 一样，尾巴多一个席位号。
func rejoinPayload(version, build uint16, name, code string, seat uint8) []byte {
	var s buf
	s.u16(version)
	s.u16(build)
	s.raw(name16(name))
	s.raw([]byte(code))
	s.u8(seat)
	return s.b
}

func (c *testClient) create(version, build uint16, name string) []byte {
	c.t.Helper()
	c.send(msgCliCreateRoom, createPayload(version, build, name))
	f := c.next(time.Second)
	if f.typ != msgSrvWelcome {
		c.t.Fatalf("create: expected WELCOME, got %#x", f.typ)
	}
	return f.payload
}

func (c *testClient) join(version, build uint16, name, code string) []byte {
	c.t.Helper()
	c.send(msgCliJoinRoom, joinPayload(version, build, name, code))
	f := c.next(time.Second)
	if f.typ != msgSrvWelcome {
		c.t.Fatalf("join: expected WELCOME, got %#x", f.typ)
	}
	return f.payload
}

// rejoin 发 REJOIN 并等 WELCOME（成功路径的便捷封装；拒绝路径自己 send + expectReject）。
func (c *testClient) rejoin(version, build uint16, name, code string, seat uint8) []byte {
	c.t.Helper()
	c.send(msgCliRejoin, rejoinPayload(version, build, name, code, seat))
	f := c.next(time.Second)
	if f.typ != msgSrvWelcome {
		c.t.Fatalf("rejoin: expected WELCOME, got %#x", f.typ)
	}
	return f.payload
}

func (c *testClient) expectReject(wantReason uint8) {
	c.t.Helper()
	f := c.next(time.Second)
	if f.typ != msgSrvReject {
		c.t.Fatalf("expected REJECT, got %#x", f.typ)
	}
	if len(f.payload) != 3 {
		c.t.Fatalf("REJECT payload size = %d, want 3", len(f.payload))
	}
	if got := f.payload[2]; got != wantReason {
		c.t.Fatalf("REJECT reason = %d, want %d", got, wantReason)
	}
}

type welcomeInfo struct {
	yourSeat uint8
	hostSeat uint8
	code     string
	roster   []rosterEntry
}

func decodeWelcome(t *testing.T, payload []byte) welcomeInfo {
	t.Helper()
	r := &reader{b: payload}
	if v, _ := r.u16(); v != protocolVersion {
		t.Fatalf("WELCOME version = %d, want %d", v, protocolVersion)
	}
	var w welcomeInfo
	w.yourSeat, _ = r.u8()
	w.hostSeat, _ = r.u8()
	count, _ := r.u8()
	if count > maxPlayers {
		t.Fatalf("WELCOME seatCount = %d > %d", count, maxPlayers)
	}
	rawCode, _ := r.raw(roomCodeLen)
	w.code = string(rawCode)
	for i := 0; i < int(count); i++ {
		var e rosterEntry
		e.seat, _ = r.u8()
		e.build, _ = r.u16()
		rawName, _ := r.raw(nameSize)
		copy(e.name[:], rawName)
		w.roster = append(w.roster, e)
	}
	if !r.done() {
		t.Fatalf("WELCOME: %d trailing bytes", len(payload)-r.off)
	}
	return w
}

func decodePeerJoin(t *testing.T, payload []byte) (uint8, uint16, string) {
	t.Helper()
	r := &reader{b: payload}
	seat, _ := r.u8()
	build, _ := r.u16()
	rawName, _ := r.raw(nameSize)
	if !r.done() {
		t.Fatalf("PEER_JOIN: %d trailing bytes", len(payload)-r.off)
	}
	return seat, build, cstr(rawName)
}

func cstr(b []byte) string {
	n := 0
	for n < len(b) && b[n] != 0 {
		n++
	}
	return string(b[:n])
}

func nameOf(e rosterEntry) string { return cstr(e.name[:]) }

// gameFrame 造一个游戏帧载荷：前两字节 src/dst，后面随便跟几个字节当"事件体"。
func gameFrame(src, dst uint8, extra ...byte) []byte {
	out := []byte{src, dst}
	return append(out, extra...)
}

// pump 后台读流：PING 回 PONG（保持自己活着），其他帧送到 out。
// 这就是正式客户端在保活上的行为。stop 关闭后退出。
func (c *testClient) pump(out chan<- wireFrame) (stop func()) {
	stopCh := make(chan struct{})
	go func() {
		defer close(out)
		for {
			select {
			case <-stopCh:
				return
			default:
			}
			c.nc.SetReadDeadline(time.Now().Add(20 * time.Millisecond))
			typ, payload, err := readFrame(c.nc)
			if err != nil {
				continue
			}
			if typ == msgSrvPing {
				c.trySend(msgCliPong, nil)
				continue
			}
			select {
			case out <- wireFrame{typ, payload}:
			default:
			}
		}
	}()
	return func() { close(stopCh) }
}

// ---- 测试服务器 ----

// startTestServer 起一台跑在 127.0.0.1 随机端口上的服务器。
// 默认把保活参数调成"一小时"——大多数用例不希望 PING 掺进读流里。
func startTestServer(t *testing.T, tune func(*Server)) (*Server, string) {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen: %v", err)
	}
	t.Cleanup(func() { ln.Close() })
	// 服务器日志走 stderr：用例结束后服务器 goroutine 还可能收尾打日志，
	// 走 t.Logf 会炸 "log in goroutine after test has completed"。
	srv := newServer(log.New(os.Stderr, "relay: ", 0).Printf)
	srv.pingInterval = time.Hour
	srv.idleTimeout = time.Hour
	if tune != nil {
		tune(srv)
	}
	go func() {
		for {
			nc, err := ln.Accept()
			if err != nil {
				return
			}
			go newConn(srv, nc).run()
		}
	}()
	return srv, ln.Addr().String()
}

// ---- 用例 ----

func TestCreateRoomWelcome(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	if w.yourSeat != 1 || w.hostSeat != 1 {
		t.Fatalf("host seat/hostSeat = %d/%d, want 1/1", w.yourSeat, w.hostSeat)
	}
	if !validRoomCode(w.code) {
		t.Fatalf("room code %q is not in the alphabet", w.code)
	}
	if len(w.roster) != 1 || w.roster[0].seat != 1 || w.roster[0].build != 16 || nameOf(w.roster[0]) != "host" {
		t.Fatalf("roster = %+v, want one seat-1 entry", w.roster)
	}
}

func TestJoinRosterAndBroadcast(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	hw := decodeWelcome(t, host.create(protocolVersion, 16, "host"))

	c2 := newTestClient(t, addr)
	w := decodeWelcome(t, c2.join(protocolVersion, 99, "guest", hw.code))
	if w.yourSeat != 2 || w.hostSeat != 1 {
		t.Fatalf("joiner seat/hostSeat = %d/%d, want 2/1", w.yourSeat, w.hostSeat)
	}
	if len(w.roster) != 2 {
		t.Fatalf("joiner roster size = %d, want 2", len(w.roster))
	}
	if w.roster[0].seat != 1 || nameOf(w.roster[0]) != "host" || w.roster[0].build != 16 {
		t.Fatalf("roster[0] = %+v, want seat-1 host build 16", w.roster[0])
	}
	if w.roster[1].seat != 2 || nameOf(w.roster[1]) != "guest" || w.roster[1].build != 99 {
		t.Fatalf("roster[1] = %+v, want seat-2 guest build 99", w.roster[1])
	}

	f := host.next(time.Second)
	if f.typ != msgSrvPeerJoin {
		t.Fatalf("host got %#x, want PEER_JOIN", f.typ)
	}
	seat, build, name := decodePeerJoin(t, f.payload)
	if seat != 2 || build != 99 || name != "guest" {
		t.Fatalf("PEER_JOIN = seat %d build %d name %q", seat, build, name)
	}
}

func TestJoinFailures(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))

	c := newTestClient(t, addr)

	c.send(msgCliJoinRoom, joinPayload(protocolVersion, 16, "x", "ZZZZ"))
	c.expectReject(rejectRoomNotFound)

	c.send(msgCliJoinRoom, joinPayload(protocolVersion, 16, "x", "IO01")) // 不在字母表里
	c.expectReject(rejectBadCode)

	c.send(msgCliJoinRoom, joinPayload(protocolVersion+1, 16, "x", w.code))
	c.expectReject(rejectProtocolVersion)

	// 填满全部席位（host 占 1）
	for i := 0; i < maxPlayers-1; i++ {
		newTestClient(t, addr).join(protocolVersion, 16, "fill", w.code)
	}
	// 满员后再来一位进不来
	cLate := newTestClient(t, addr)
	cLate.send(msgCliJoinRoom, joinPayload(protocolVersion, 16, "late", w.code))
	cLate.expectReject(rejectRoomFull)
}

// 4 席以上：第 5、6 位能正常进（席位 5、6），WELCOME 名册逐位齐全，
// 跨第 6 席位的转发双向都走得通，满员（6）后第 7 位被拒。
func TestFiveAndSixSeatJoinAndRouting(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))

	clients := []*testClient{host}
	for i := 2; i <= maxPlayers; i++ {
		name := fmt.Sprintf("guest%d", i)
		c := newTestClient(t, addr)
		cw := decodeWelcome(t, c.join(protocolVersion, 16, name, w.code))
		if cw.yourSeat != uint8(i) {
			t.Fatalf("joiner %d seat = %d, want %d", i, cw.yourSeat, i)
		}
		if len(cw.roster) != i {
			t.Fatalf("roster size after join %d = %d, want %d", i, len(cw.roster), i)
		}
		if last := cw.roster[len(cw.roster)-1]; last.seat != uint8(i) || nameOf(last) != name {
			t.Fatalf("roster last entry = %+v, want seat %d %s", last, i, name)
		}
		clients = append(clients, c)
	}

	// 前面每位都收到过后来的 PEER_JOIN（host 依次收满 5 条）
	for i := 2; i <= maxPlayers; i++ {
		f := host.next(time.Second)
		if f.typ != msgSrvPeerJoin {
			t.Fatalf("host got %#x, want PEER_JOIN", f.typ)
		}
		seat, _, name := decodePeerJoin(t, f.payload)
		if seat != uint8(i) || name != fmt.Sprintf("guest%d", i) {
			t.Fatalf("PEER_JOIN = seat %d name %q, want seat %d guest%d", seat, name, i, i)
		}
	}

	// 满员后再来一位进不来
	cLate := newTestClient(t, addr)
	cLate.send(msgCliJoinRoom, joinPayload(protocolVersion, 16, "late", w.code))
	cLate.expectReject(rejectRoomFull)

	// 第 5/6 席位不是摆设：2 → 6、6 → 1 各走一帧
	c2, c6 := clients[1], clients[maxPlayers-1]
	c2.send(5, gameFrame(2, 6, 0x66))
	if f := c6.next(time.Second); f.typ != 5 || f.payload[2] != 0x66 {
		t.Fatalf("seat 6 got %#x %x, want game frame with 0x66", f.typ, f.payload)
	}
	c6.send(5, gameFrame(6, 1, 0x77))
	if f := host.next(time.Second); f.typ != 5 || f.payload[2] != 0x77 {
		t.Fatalf("host got %#x %x from seat 6, want game frame with 0x77", f.typ, f.payload)
	}
}

func TestJoinCodeCaseInsensitive(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))

	lower := []byte(w.code)
	changed := false
	for i := range lower {
		if lower[i] >= 'A' && lower[i] <= 'Z' {
			lower[i] += 'a' - 'A'
			changed = true
		}
	}
	if !changed {
		t.Skipf("generated code %q has no letters to lower", w.code)
	}
	c2 := newTestClient(t, addr)
	got := decodeWelcome(t, c2.join(protocolVersion, 16, "guest", string(lower)))
	if got.yourSeat != 2 || got.code != w.code {
		t.Fatalf("join with lowercase code: seat %d code %q, want 2/%q", got.yourSeat, got.code, w.code)
	}
}

func TestGameFrameRouting(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second) // 吃掉 PEER_JOIN

	// 1 → 2：原样到达
	want := gameFrame(1, 2, 0xAA, 0xBB, 0xCC)
	host.send(5, want)
	f := c2.next(time.Second)
	if f.typ != 5 || string(f.payload) != string(want) {
		t.Fatalf("c2 got %#x %x, want 5 %x", f.typ, f.payload, want)
	}

	// 2 → 1：原样到达
	want = gameFrame(2, 1, 0x01)
	c2.send(5, want)
	f = host.next(time.Second)
	if f.typ != 5 || string(f.payload) != string(want) {
		t.Fatalf("host got %#x %x, want 5 %x", f.typ, f.payload, want)
	}

	// 冒名：src 写成 3（不是自己席位）→ 丢
	host.send(5, gameFrame(3, 2, 0xEE))
	c2.expectNothing(150 * time.Millisecond)

	// 发往空席位 3 → 丢（不炸、不断连）
	host.send(5, gameFrame(1, 3, 0xEE))
	c2.expectNothing(150 * time.Millisecond)

	// 发给自己 → 丢
	host.send(5, gameFrame(1, 1, 0xEE))
	c2.expectNothing(150 * time.Millisecond)

	// 被丢过帧之后连接照常活着
	host.send(5, gameFrame(1, 2, 0x77))
	f = c2.next(time.Second)
	if f.payload[2] != 0x77 {
		t.Fatalf("connection died after dropped frames; got %x", f.payload)
	}
}

func TestGameFrameBeforeJoinDropped(t *testing.T) {
	_, addr := startTestServer(t, nil)
	c := newTestClient(t, addr)
	c.send(3, gameFrame(1, 2, 0xEE)) // 没进房就发包 = 丢，但不断连
	decodeWelcome(t, c.create(protocolVersion, 16, "host"))
}

func TestSeatSwapViaServer(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)

	// 1 号发起：提交 1/2 置换
	host.send(msgCliSwapCommit, []byte{1, 2})

	for _, c := range []*testClient{host, c2} {
		f := c.next(time.Second)
		if f.typ != msgSrvSeatSwap || len(f.payload) != 2 || f.payload[0] != 1 || f.payload[1] != 2 {
			t.Fatalf("expected SEAT_SWAP 1/2, got %#x %x", f.typ, f.payload)
		}
	}

	// 置换后台：host 在 2 号位、c2 在 1 号位。c2 用新席位发 1→2，host 收得到
	c2.send(5, gameFrame(1, 2, 0x42))
	f := host.next(time.Second)
	if f.typ != 5 || f.payload[2] != 0x42 {
		t.Fatalf("after swap, host got %#x %x", f.typ, f.payload)
	}

	// 用旧席位当 src → 丢
	c2.send(5, gameFrame(2, 1, 0xEE))
	host.expectNothing(150 * time.Millisecond)

	// 第三方提交一对不含自己的置换 → 忽略（不广播）
	c3 := newTestClient(t, addr)
	decodeWelcome(t, c3.join(protocolVersion, 16, "third", w.code))
	host.next(time.Second) // 吃 c3 的 PEER_JOIN
	c2.next(time.Second)
	c3.send(msgCliSwapCommit, []byte{1, 2})
	for _, c := range []*testClient{host, c2, c3} {
		c.expectNothing(150 * time.Millisecond)
	}
}

func TestPeerLeaveAndSeatReuse(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)
	c3 := newTestClient(t, addr)
	decodeWelcome(t, c3.join(protocolVersion, 16, "guest3", w.code))
	host.next(time.Second)
	c2.next(time.Second)

	// 体面退房（LEAVE_ROOM）：立即广播离开、不设保留，席位随即可复用
	c3.send(msgCliLeaveRoom, nil)

	for _, c := range []*testClient{host, c2} {
		f := c.next(2 * time.Second)
		if f.typ != msgSrvPeerLeave || f.payload[0] != 3 || f.payload[1] != peerLeaveQuit {
			t.Fatalf("expected PEER_LEAVE seat 3 quit, got %#x %x", f.typ, f.payload)
		}
	}

	// 3 号席位回收复用
	c4 := newTestClient(t, addr)
	got := decodeWelcome(t, c4.join(protocolVersion, 16, "guest4", w.code))
	if got.yourSeat != 3 {
		t.Fatalf("recycled seat = %d, want 3", got.yourSeat)
	}
	if len(got.roster) != 3 {
		t.Fatalf("roster after recycle = %d, want 3", len(got.roster))
	}
}

func TestLeaveRoomMessage(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)

	// 非房主发 LEAVE_ROOM：别人收到 PEER_LEAVE(quit)，自己连接被断
	c2.send(msgCliLeaveRoom, nil)
	f := host.next(time.Second)
	if f.typ != msgSrvPeerLeave || f.payload[0] != 2 || f.payload[1] != peerLeaveQuit {
		t.Fatalf("expected PEER_LEAVE seat 2 quit, got %#x %x", f.typ, f.payload)
	}
	c2.expectClosed(time.Second)
}

func TestHostLeaveClosesRoom(t *testing.T) {
	srv, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)

	// 房主体面退房（LEAVE_ROOM）：散整房（掉线那条路现在进保留期，见 TestHostDropHoldsRoomForRejoin）
	host.send(msgCliLeaveRoom, nil)

	f := c2.next(2 * time.Second)
	if f.typ != msgSrvRoomClosed || len(f.payload) != 1 || f.payload[0] != roomClosedHostLeft {
		t.Fatalf("expected ROOM_CLOSED hostLeft, got %#x %x", f.typ, f.payload)
	}
	c2.expectClosed(time.Second)

	// 房间没了：老码再进 = 查无此房
	c3 := newTestClient(t, addr)
	c3.send(msgCliJoinRoom, joinPayload(protocolVersion, 16, "x", w.code))
	c3.expectReject(rejectRoomNotFound)

	srv.mu.Lock()
	roomCount := len(srv.rooms)
	srv.mu.Unlock()
	if roomCount != 0 {
		t.Fatalf("rooms left = %d, want 0", roomCount)
	}
}

func TestSwapHostThenHostLeaveStillClosesRoom(t *testing.T) {
	// 房主换到 2 号位之后走人，房间照样要散（房主身份按连接认，不按席位认）
	srv, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)

	host.send(msgCliSwapCommit, []byte{1, 2})
	host.next(time.Second)
	c2.next(time.Second)

	host.send(msgCliLeaveRoom, nil)
	f := c2.next(2 * time.Second)
	if f.typ != msgSrvRoomClosed || f.payload[0] != roomClosedHostLeft {
		t.Fatalf("expected ROOM_CLOSED after swapped-host left, got %#x %x", f.typ, f.payload)
	}

	srv.mu.Lock()
	defer srv.mu.Unlock()
	if len(srv.rooms) != 0 {
		t.Fatalf("rooms left = %d, want 0", len(srv.rooms))
	}
}

func TestPing(t *testing.T) {
	_, addr := startTestServer(t, func(s *Server) { s.pingInterval = 30 * time.Millisecond })
	host := newTestClient(t, addr)
	decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	f := host.next(time.Second)
	if f.typ != msgSrvPing || len(f.payload) != 0 {
		t.Fatalf("expected PING, got %#x %x", f.typ, f.payload)
	}
}

func TestIdleTimeoutDropsTimeoutPeer(t *testing.T) {
	// 观察者回 PONG 保自己活着；发呆的那位什么都不回 → 到点被踢。
	// 两段式（MOD_BUILD 39）：先 PEER_OFFLINE（席位进保留期），保留过期没回来才 PEER_LEAVE(timeout)。
	srv, addr := startTestServer(t, func(s *Server) {
		s.pingInterval = 30 * time.Millisecond
		s.idleTimeout = 200 * time.Millisecond
	})
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))

	frames := make(chan wireFrame, 16)
	stop := host.pump(frames)
	defer stop()

	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "sleeper", w.code))
	// c2 从此一言不发

	deadline := time.After(3 * time.Second)
	offlineSeen := false
	for !offlineSeen {
		select {
		case f := <-frames:
			if f.typ == msgSrvPeerLeave {
				t.Fatalf("got PEER_LEAVE before PEER_OFFLINE: %x", f.payload)
			}
			if f.typ != msgSrvPeerOffline {
				continue
			}
			if f.payload[0] != 2 {
				t.Fatalf("PEER_OFFLINE = seat %d, want seat 2", f.payload[0])
			}
			if f.payload[1] != srv.holdGrace() {
				t.Fatalf("PEER_OFFLINE grace = %d, want %d", f.payload[1], srv.holdGrace())
			}
			offlineSeen = true
		case <-deadline:
			t.Fatal("sleeper never went offline for idleness")
		}
	}

	// 保留过期（时间拨到未来，直接扫）：广播 PEER_LEAVE(timeout)
	srv.sweepOnce(time.Now().Add(2 * srv.holdDuration))

	deadline = time.After(2 * time.Second)
	for {
		select {
		case f := <-frames:
			if f.typ != msgSrvPeerLeave {
				continue
			}
			if f.payload[0] != 2 || f.payload[1] != peerLeaveTimeout {
				t.Fatalf("PEER_LEAVE = seat %d reason %d, want seat 2 reason timeout", f.payload[0], f.payload[1])
			}
			return
		case <-deadline:
			t.Fatal("expired hold never produced PEER_LEAVE")
		}
	}
}

func TestMalformedFramesDropConnection(t *testing.T) {
	t.Run("oversized length", func(t *testing.T) {
		_, addr := startTestServer(t, nil)
		c := newTestClient(t, addr)
		c.nc.Write([]byte{0x03, 0x00, 0x2C, 0x01}) // len=300 > 256
		c.expectClosed(time.Second)
	})
	t.Run("game frame shorter than src+dst", func(t *testing.T) {
		_, addr := startTestServer(t, nil)
		c := newTestClient(t, addr)
		c.send(5, []byte{0x01}) // 只有 src
		c.expectClosed(time.Second)
	})
	t.Run("bad control payload size", func(t *testing.T) {
		_, addr := startTestServer(t, nil)
		c := newTestClient(t, addr)
		c.send(msgCliCreateRoom, []byte{0x01, 0x00}) // 只有 version 两字节
		c.expectClosed(time.Second)
	})
	t.Run("truncated join code", func(t *testing.T) {
		_, addr := startTestServer(t, nil)
		host := newTestClient(t, addr)
		decodeWelcome(t, host.create(protocolVersion, 16, "host"))
		c := newTestClient(t, addr)
		var s buf
		s.u16(protocolVersion)
		s.u16(16)
		s.raw(name16("x"))
		s.raw([]byte("ABC")) // 码只有 3 字节 → 载荷长度对不上
		c.send(msgCliJoinRoom, s.b)
		c.expectClosed(time.Second)
	})
}

func TestRoomCodesUniqueAndWellFormed(t *testing.T) {
	_, addr := startTestServer(t, nil)
	seen := map[string]bool{}
	for i := 0; i < 16; i++ {
		c := newTestClient(t, addr)
		w := decodeWelcome(t, c.create(protocolVersion, 16, "h"))
		if !validRoomCode(w.code) {
			t.Fatalf("code %q out of alphabet", w.code)
		}
		if seen[w.code] {
			t.Fatalf("duplicate room code %q", w.code)
		}
		seen[w.code] = true
	}
}

func TestUnknownControlFrameIgnored(t *testing.T) {
	_, addr := startTestServer(t, func(s *Server) { s.pingInterval = 30 * time.Millisecond })
	c := newTestClient(t, addr)
	decodeWelcome(t, c.create(protocolVersion, 16, "host"))
	c.send(0xF0FF, nil) // 未知控制帧：丢，不断连
	f := c.next(time.Second)
	if f.typ != msgSrvPing {
		t.Fatalf("connection did not survive the unknown frame; got %#x", f.typ)
	}
}

// ---- MOD_BUILD 39：重连（REJOIN）与席位保留 ----

func expectPeerOffline(t *testing.T, f wireFrame, wantSeat, wantGrace uint8) {
	t.Helper()
	if f.typ != msgSrvPeerOffline {
		t.Fatalf("expected PEER_OFFLINE, got %#x %x", f.typ, f.payload)
	}
	if len(f.payload) != 2 || f.payload[0] != wantSeat || f.payload[1] != wantGrace {
		t.Fatalf("PEER_OFFLINE = %x, want seat %d grace %d", f.payload, wantSeat, wantGrace)
	}
}

func expectPeerLeave(t *testing.T, f wireFrame, wantSeat, wantReason uint8) {
	t.Helper()
	if f.typ != msgSrvPeerLeave {
		t.Fatalf("expected PEER_LEAVE, got %#x %x", f.typ, f.payload)
	}
	if len(f.payload) != 2 || f.payload[0] != wantSeat || f.payload[1] != wantReason {
		t.Fatalf("PEER_LEAVE = %x, want seat %d reason %d", f.payload, wantSeat, wantReason)
	}
}

// 快路径：旧连接还挂着时 REJOIN（网络抖动自愈）——静默换掉，队友只见 PEER_BACK 不见 OFFLINE。
func TestRejoinFastPathReplacesLiveConn(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second) // 吃 PEER_JOIN

	c2new := newTestClient(t, addr)
	got := decodeWelcome(t, c2new.rejoin(protocolVersion, 16, "guest", w.code, 2))
	if got.yourSeat != 2 || got.hostSeat != 1 || len(got.roster) != 2 {
		t.Fatalf("fast-path WELCOME = seat %d host %d roster %d, want 2/1/2", got.yourSeat, got.hostSeat, len(got.roster))
	}

	// 队友看到的第一帧必须是 PEER_BACK（不能有 OFFLINE 混进来）
	f := host.next(time.Second)
	if f.typ != msgSrvPeerBack {
		t.Fatalf("host got %#x, want PEER_BACK", f.typ)
	}
	if seat, build, name := decodePeerJoin(t, f.payload); seat != 2 || build != 16 || name != "guest" {
		t.Fatalf("PEER_BACK = seat %d build %d name %q", seat, build, name)
	}

	// 旧连接被静默关掉
	c2.expectClosed(time.Second)

	// 新连接直接干活：2 → 1 和 1 → 2 都通
	c2new.send(5, gameFrame(2, 1, 0x99))
	if f := host.next(time.Second); f.typ != 5 || f.payload[2] != 0x99 {
		t.Fatalf("host got %#x %x after rejoin, want game frame", f.typ, f.payload)
	}
	host.send(5, gameFrame(1, 2, 0x98))
	if f := c2new.next(time.Second); f.typ != 5 || f.payload[2] != 0x98 {
		t.Fatalf("rejoined conn got %#x %x, want game frame", f.typ, f.payload)
	}
}

// 保留路径：闲死踢掉后席位进保留，本人 REJOIN 坐回原席。
func TestRejoinFromHoldAfterTimeoutKick(t *testing.T) {
	_, addr := startTestServer(t, func(s *Server) {
		s.pingInterval = 30 * time.Millisecond
		s.idleTimeout = 200 * time.Millisecond
		s.holdDuration = 5 * time.Second
	})
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))

	frames := make(chan wireFrame, 16)
	stop := host.pump(frames) // 观察者自己 PONG 保活
	defer stop()

	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "sleeper", w.code))
	// c2 从此一言不发 → 被踢 → 席位保留

	deadline := time.After(3 * time.Second)
	for {
		select {
		case f := <-frames:
			if f.typ == msgSrvPeerOffline {
				expectPeerOffline(t, f, 2, 5)
				goto offline
			}
		case <-deadline:
			t.Fatal("sleeper never went offline")
		}
	}
offline:

	// 本人回来（新连接 REJOIN）→ WELCOME 坐回 2 号席，队友收到 PEER_BACK
	c2new := newTestClient(t, addr)
	got := decodeWelcome(t, c2new.rejoin(protocolVersion, 16, "sleeper", w.code, 2))
	if got.yourSeat != 2 {
		t.Fatalf("rejoined seat = %d, want 2", got.yourSeat)
	}
	deadline = time.After(2 * time.Second)
	for {
		select {
		case f := <-frames:
			if f.typ == msgSrvPeerBack {
				if seat, build, name := decodePeerJoin(t, f.payload); seat != 2 || build != 16 || name != "sleeper" {
					t.Fatalf("PEER_BACK = seat %d build %d name %q", seat, build, name)
				}
				return
			}
		case <-deadline:
			t.Fatal("no PEER_BACK after rejoin from hold")
		}
	}
}

// 保留过期：清扫器广播 PEER_LEAVE(timeout)，席位随即可被新 JOIN 拿走；此时再 REJOIN 被拒。
func TestHoldExpiryReleasesSeat(t *testing.T) {
	srv, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)

	c2.nc.Close() // 非体面断开 → 保留期
	expectPeerOffline(t, host.next(2*time.Second), 2, srv.holdGrace())

	srv.sweepOnce(time.Now().Add(2 * srv.holdDuration))
	expectPeerLeave(t, host.next(2*time.Second), 2, peerLeaveTimeout)

	// 席位已释放：老身份 REJOIN 被拒（REJOIN_UNAVAILABLE），新手能正常坐下
	later := newTestClient(t, addr)
	later.send(msgCliRejoin, rejoinPayload(protocolVersion, 16, "guest", w.code, 2))
	later.expectReject(rejectRejoinUnavailable)

	got := decodeWelcome(t, later.join(protocolVersion, 16, "someone", w.code))
	if got.yourSeat != 2 {
		t.Fatalf("seat after hold expiry = %d, want 2", got.yourSeat)
	}
}

// 普通 JOIN 跳过别人的保留位；名字+构建对得上则坐回自己的保留位（换进程重进房）。
func TestJoinSkipsAndReclaimsHeldSeats(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)
	c3 := newTestClient(t, addr)
	decodeWelcome(t, c3.join(protocolVersion, 16, "guest3", w.code))
	host.next(time.Second)
	c2.next(time.Second)

	c2.nc.Close() // 2 号席位进保留
	for _, c := range []*testClient{host, c3} {
		f := c.next(2 * time.Second)
		if f.typ != msgSrvPeerOffline || f.payload[0] != 2 {
			t.Fatalf("expected PEER_OFFLINE seat 2, got %#x %x", f.typ, f.payload)
		}
	}

	// 陌生人：跳过保留的 2 号位，坐 4 号
	stranger := newTestClient(t, addr)
	got := decodeWelcome(t, stranger.join(protocolVersion, 16, "stranger", w.code))
	if got.yourSeat != 4 {
		t.Fatalf("stranger seat = %d, want 4 (held seat 2 must be skipped)", got.yourSeat)
	}

	// 本人换进程重进房：名字+构建对得上 → 坐回 2 号
	back := newTestClient(t, addr)
	got = decodeWelcome(t, back.join(protocolVersion, 16, "guest", w.code))
	if got.yourSeat != 2 {
		t.Fatalf("reclaim seat = %d, want 2", got.yourSeat)
	}
}

// 房主掉线：房间进保留不散；保留过期还没回来才散房。
func TestHostDropHoldsRoomThenExpires(t *testing.T) {
	srv, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)

	host.nc.Close()
	expectPeerOffline(t, c2.next(2*time.Second), 1, srv.holdGrace())

	// 房间还活着（只是房主位空着）
	srv.mu.Lock()
	roomCount := len(srv.rooms)
	srv.mu.Unlock()
	if roomCount != 1 {
		t.Fatalf("rooms after host drop = %d, want 1 (room must survive the hold)", roomCount)
	}

	srv.sweepOnce(time.Now().Add(2 * srv.holdDuration))
	f := c2.next(2 * time.Second)
	if f.typ != msgSrvRoomClosed || f.payload[0] != roomClosedHostLeft {
		t.Fatalf("expected ROOM_CLOSED after host hold expired, got %#x %x", f.typ, f.payload)
	}
	c2.expectClosed(time.Second)

	srv.mu.Lock()
	roomCount = len(srv.rooms)
	srv.mu.Unlock()
	if roomCount != 0 {
		t.Fatalf("rooms left = %d, want 0", roomCount)
	}
}

// 房主重连归位 + 换位后 hostSeat 跟着走（hostSeat 不许靠指针现找）。
func TestHostRejoinAndSwapTracking(t *testing.T) {
	srv, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)

	host.nc.Close()
	expectPeerOffline(t, c2.next(2*time.Second), 1, srv.holdGrace())

	// 房主重连：WELCOME 点明 hostSeat 仍是 1；队友收到 PEER_BACK
	hostNew := newTestClient(t, addr)
	got := decodeWelcome(t, hostNew.rejoin(protocolVersion, 16, "host", w.code, 1))
	if got.yourSeat != 1 || got.hostSeat != 1 {
		t.Fatalf("host rejoin WELCOME = seat %d hostSeat %d, want 1/1", got.yourSeat, got.hostSeat)
	}
	if f := c2.next(2 * time.Second); f.typ != msgSrvPeerBack {
		t.Fatalf("c2 got %#x, want PEER_BACK", f.typ)
	}

	// 房主换到 2 号位：hostSeat 跟着换
	hostNew.send(msgCliSwapCommit, []byte{1, 2})
	hostNew.next(time.Second)
	c2.next(time.Second)

	// 再掉线：保留的是 2 号位（房主现在坐那儿）
	hostNew.nc.Close()
	expectPeerOffline(t, c2.next(2*time.Second), 2, srv.holdGrace())

	// 过期不回来：散房
	srv.sweepOnce(time.Now().Add(2 * srv.holdDuration))
	f := c2.next(2 * time.Second)
	if f.typ != msgSrvRoomClosed {
		t.Fatalf("expected ROOM_CLOSED, got %#x %x", f.typ, f.payload)
	}
}

// 身份核对：名字/构建对不上就拒绝；被拒不影响原连接/保留。
func TestRejoinIdentityMismatchRejected(t *testing.T) {
	srv, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)

	// 快路径冒名：旧连接还活着，名字对不上 → 拒；原连接毫发无损
	impostor := newTestClient(t, addr)
	impostor.send(msgCliRejoin, rejoinPayload(protocolVersion, 16, "notguest", w.code, 2))
	impostor.expectReject(rejectRejoinSeatMismatch)
	c2.send(5, gameFrame(2, 1, 0x11))
	if f := host.next(time.Second); f.typ != 5 || f.payload[2] != 0x11 {
		t.Fatalf("original conn disturbed by rejected impostor: got %#x %x", f.typ, f.payload)
	}

	// 保留路径：构建对不上 → 拒；名字对不上 → 拒
	c2.nc.Close()
	expectPeerOffline(t, host.next(2*time.Second), 2, srv.holdGrace())
	impostor.send(msgCliRejoin, rejoinPayload(protocolVersion, 99, "guest", w.code, 2))
	impostor.expectReject(rejectRejoinSeatMismatch)
	impostor.send(msgCliRejoin, rejoinPayload(protocolVersion, 16, "notguest", w.code, 2))
	impostor.expectReject(rejectRejoinSeatMismatch)

	// 本人（名字+构建都对）仍能坐回去——拒绝没有把保留弄丢
	back := newTestClient(t, addr)
	got := decodeWelcome(t, back.rejoin(protocolVersion, 16, "guest", w.code, 2))
	if got.yourSeat != 2 {
		t.Fatalf("legit rejoin after rejects = seat %d, want 2", got.yourSeat)
	}
}

// REJOIN 的不可用类拒绝：没房、席位已释放、席位号非法、版本不对。
func TestRejoinUnavailableCases(t *testing.T) {
	_, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))

	c := newTestClient(t, addr)
	c.send(msgCliRejoin, rejoinPayload(protocolVersion, 16, "x", "ZZZZ", 1))
	c.expectReject(rejectRejoinUnavailable)

	c.send(msgCliRejoin, rejoinPayload(protocolVersion, 16, "x", "IO01", 1)) // 码不在字母表
	c.expectReject(rejectBadCode)

	c.send(msgCliRejoin, rejoinPayload(protocolVersion+1, 16, "x", w.code, 1))
	c.expectReject(rejectProtocolVersion)

	c.send(msgCliRejoin, rejoinPayload(protocolVersion, 16, "x", w.code, 0))
	c.expectReject(rejectRejoinUnavailable)
	c.send(msgCliRejoin, rejoinPayload(protocolVersion, 16, "x", w.code, maxPlayers+1))
	c.expectReject(rejectRejoinUnavailable)

	// 体面退房的席位不设保留（MOD_BUILD 39 的 G9 场景）：走了就是走了
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)
	c2.send(msgCliLeaveRoom, nil)
	c2.expectClosed(time.Second)
	expectPeerLeave(t, host.next(2*time.Second), 2, peerLeaveQuit)

	later := newTestClient(t, addr)
	later.send(msgCliRejoin, rejoinPayload(protocolVersion, 16, "guest", w.code, 2))
	later.expectReject(rejectRejoinUnavailable)
}

// 全员掉线的空房：保留过期后清扫器把房间整个删掉。
func TestAllDropRoomSweptAway(t *testing.T) {
	srv, addr := startTestServer(t, nil)
	host := newTestClient(t, addr)
	w := decodeWelcome(t, host.create(protocolVersion, 16, "host"))
	c2 := newTestClient(t, addr)
	decodeWelcome(t, c2.join(protocolVersion, 16, "guest", w.code))
	host.next(time.Second)

	// c2 先掉（host 收到 OFFLINE 即证明服务器处理完了）
	c2.nc.Close()
	expectPeerOffline(t, host.next(2*time.Second), 2, srv.holdGrace())

	// host 再掉（没人能收到它的 OFFLINE 了，轮询服务器状态等它处理完）
	host.nc.Close()
	deadline := time.Now().Add(2 * time.Second)
	for {
		srv.mu.Lock()
		r := srv.rooms[w.code]
		done := r != nil && r.host == nil && r.bySeat[1] == nil && r.bySeat[2] == nil &&
			!r.holds[1].until.IsZero() && !r.holds[2].until.IsZero()
		srv.mu.Unlock()
		if done {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("server never processed the host drop")
		}
		time.Sleep(5 * time.Millisecond)
	}

	srv.sweepOnce(time.Now().Add(2 * srv.holdDuration))
	srv.mu.Lock()
	roomCount := len(srv.rooms)
	srv.mu.Unlock()
	if roomCount != 0 {
		t.Fatalf("rooms after sweep = %d, want 0", roomCount)
	}
}
