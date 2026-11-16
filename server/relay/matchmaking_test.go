package main

import (
	"encoding/binary"
	"fmt"
	"net"
	"pvzrelay/internal/catalogwire"
	"strconv"
	"sync"
	"testing"
	"time"
)

func matchNode(id, addr string) catalogwire.Node {
	host, port, _ := net.SplitHostPort(addr)
	n, _ := strconv.Atoi(port)
	return catalogwire.Node{ID: id, Host: host, Port: uint16(n), Weight: 100, Create: true, Directory: true}
}
func matchConfigure(s *Server, id string, nodes []catalogwire.Node) {
	cfg := defaultOperationsConfig()
	cfg.MatchNodeID = id
	s.ops.current.Store(&operationsSnapshot{Config: cfg, Catalog: catalogwire.Document{Revision: 1, ExpiresAt: time.Now().Add(time.Hour), Nodes: nodes}})
}
func matchRouteBytes(build uint16, mode byte, name string) []byte {
	p := []byte{1, 1, byte(build), byte(build >> 8), mode}
	p = binary.LittleEndian.AppendUint64(p, 1)
	return append(p, name16(name)...)
}
func matchCommitBytes(reply []byte, build uint16, name string) []byte {
	p := []byte{1, 5, byte(build), byte(build >> 8), reply[3]}
	p = append(p, reply[6:]...)
	return append(p, name16(name)...)
}
func matchReserveReply(r []byte) []byte { return append([]byte{1, 1, 0, r[3], 1, 0}, r[4:]...) }
func matchFrame(t *testing.T, c *testClient, typ uint16) []byte {
	t.Helper()
	for i := 0; i < 24; i++ {
		f := c.next(time.Second)
		if f.typ == typ {
			return f.payload
		}
	}
	t.Fatalf("missing frame %x", typ)
	return nil
}
func TestMatchConcurrentReservationAndCancellation(t *testing.T) {
	s, _ := startTestServer(t, nil)
	var wg sync.WaitGroup
	replies := make(chan []byte, 24)
	for i := 0; i < 24; i++ {
		wg.Add(1)
		go func(i int) { defer wg.Done(); replies <- s.reserveMatch(114, 2, name16(fmt.Sprint(i))) }(i)
	}
	wg.Wait()
	close(replies)
	codes := map[string]int{}
	tokens := map[string]bool{}
	for p := range replies {
		if len(p) != 24 {
			t.Fatalf("reservation: %x", p)
		}
		codes[string(p[4:8])]++
		if tokens[string(p[8:])] {
			t.Fatal("duplicate ticket")
		}
		tokens[string(p[8:])] = true
	}
	if len(codes) != 4 {
		t.Fatalf("24 simultaneous requests split into %d rooms", len(codes))
	}
	for _, n := range codes {
		if n != 6 {
			t.Fatalf("room count %d", n)
		}
	}
	s.mu.Lock()
	for _, r := range s.rooms {
		for seat := 1; seat <= maxPlayers; seat++ {
			r.matchSeats[seat].until = time.Now().Add(-time.Second)
		}
	}
	s.mu.Unlock()
	s.sweepOnce(time.Now())
	s.mu.Lock()
	defer s.mu.Unlock()
	if len(s.rooms) != 0 {
		t.Fatal("expired empty reservations leaked rooms")
	}
}
func TestMatchTwoNodeRoutingAndPools(t *testing.T) {
	a, addrA := startTestServer(t, nil)
	b, addrB := startTestServer(t, nil)
	nodes := []catalogwire.Node{matchNode("1", addrA), matchNode("2", addrB)}
	matchConfigure(a, "1", nodes)
	matchConfigure(b, "2", nodes)
	// Make node 1 busier. Its coordinator must still assign the lower-load node.
	a.status.mu.Lock()
	a.status.value.Operations.Score = 0.8
	a.status.mu.Unlock()
	var first []byte
	for i := 0; i < 6; i++ {
		query := newTestClient(t, addrA)
		query.send(msgMatch, matchRouteBytes(114, 2, fmt.Sprintf("m%d", i)))
		reply := matchFrame(t, query, msgMatch)
		if len(reply) != 26 || reply[2] != 0 || binary.LittleEndian.Uint16(reply[4:]) != 2 {
			t.Fatalf("route %d: %x", i, reply)
		}
		if i == 0 {
			first = reply
		} else if string(first[6:10]) != string(reply[6:10]) {
			t.Fatal("same pool split across rooms")
		}
		game := newTestClient(t, addrB)
		game.send(msgMatch, matchCommitBytes(reply, 114, fmt.Sprintf("m%d", i)))
		welcome := decodeWelcome(t, matchFrame(t, game, msgSrvWelcome))
		if welcome.code != string(reply[6:10]) {
			t.Fatal("wrong room")
		}
		state := matchFrame(t, game, msgMatch)
		if string(state) != string([]byte{1, 6, 2, 1, 1}) {
			t.Fatalf("bad state %x", state)
		}
	}
	// A distinct run length must create a distinct pool.
	query := newTestClient(t, addrA)
	query.send(msgMatch, matchRouteBytes(114, 0, "full"))
	reply := matchFrame(t, query, msgMatch)
	if len(reply) != 26 || reply[2] != 0 || reply[3] != 0 || string(reply[6:10]) == string(first[6:10]) {
		t.Fatalf("mode not separated %x", reply)
	}
	// Catalog revision mismatches fail without creating another room.
	stale := matchRouteBytes(114, 1, "stale")
	binary.LittleEndian.PutUint64(stale[5:], 2)
	query = newTestClient(t, addrA)
	query.send(msgMatch, stale)
	if reply = matchFrame(t, query, msgMatch); len(reply) != 3 || reply[2] != 3 {
		t.Fatalf("stale catalog %x", reply)
	}
	query = newTestClient(t, addrB)
	query.send(msgMatch, matchRouteBytes(114, 1, "wrong-node"))
	if reply = matchFrame(t, query, msgMatch); len(reply) != 3 || reply[2] != 3 {
		t.Fatalf("non-coordinator %x", reply)
	}
}
func TestMatchCancelBeforeAndAfterCommit(t *testing.T) {
	for _, commit := range []bool{false, true} {
		t.Run(fmt.Sprint(commit), func(t *testing.T) {
			s, addr := startTestServer(t, nil)
			reserved := s.reserveMatch(114, 1, name16("cancel"))
			route := matchReserveReply(reserved)
			if commit {
				game := newTestClient(t, addr)
				game.send(msgMatch, matchCommitBytes(route, 114, "cancel"))
				matchFrame(t, game, msgSrvWelcome)
			}
			cancel := newTestClient(t, addr)
			cancel.send(msgMatch, append([]byte{1, 4}, reserved[4:]...))
			matchFrame(t, cancel, msgMatch)
			s.mu.Lock()
			n := len(s.rooms)
			s.mu.Unlock()
			if n != 0 {
				t.Fatal("cancel left a room or reconnect hold")
			}
		})
	}
}
func TestMatchLobbyJoinGateStartAndHostDeparture(t *testing.T) {
	s, addr := startTestServer(t, nil)
	rsv := s.reserveMatch(114, 2, name16("host"))
	code := string(rsv[4:8])
	h := newTestClient(t, addr)
	h.send(msgMatch, matchCommitBytes(matchReserveReply(rsv), 114, "host"))
	matchFrame(t, h, msgSrvWelcome)
	g := newTestClient(t, addr)
	g.send(msgCliDirectoryJoin, joinPayload(1, 114, "lobby", code))
	matchFrame(t, g, msgSrvWelcome)
	h.send(msgCliFlow, []byte{1, 0})
	g.send(msgCliFlow, []byte{1, 0})
	flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled && r.flowMask() == 6 })
	late := s.reserveMatch(114, 2, name16("late"))
	gate := make([]byte, 13)
	binary.LittleEndian.PutUint32(gate, 1)
	binary.LittleEndian.PutUint32(gate[4:], 1)
	binary.LittleEndian.PutUint32(gate[9:], 1)
	h.send(msgCliGate, gate)
	matchFrame(t, h, msgSrvGate)
	newcomer := newTestClient(t, addr)
	newcomer.send(msgMatch, matchCommitBytes(matchReserveReply(late), 114, "late"))
	if p := matchFrame(t, newcomer, msgSrvReject); p[2] != rejectJoinClosed {
		t.Fatalf("join crossed start seal %x", p)
	}
	body := make([]byte, 56)
	body[0] = 1
	body[1] = 2
	body[11] = 1
	copy(body[18:], []byte{2, 1, 1, 1, 0, 0, 2, 0, 0, 1, 0, 1})
	binary.LittleEndian.PutUint32(body[51:], 1)
	start := make([]byte, 8)
	binary.LittleEndian.PutUint32(start, 1)
	binary.LittleEndian.PutUint32(start[4:], 1)
	start = append(start, body...)
	wrong := append([]byte(nil), start...)
	wrong[8+29] = 0
	s.mu.Lock()
	valid := s.rooms[code].matchStartValid(wrong[8:])
	s.mu.Unlock()
	if valid {
		t.Fatal("18-second preset accepted")
	}
	h.send(msgCliFlowStart, start)
	flowWait(t, s, code, func(r *Room) bool { return r.started && r.flow.phase == flowSetup && !r.matchOpen() })
	watch := newTestClient(t, addr)
	watch.send(msgCliDirectoryJoin, joinPayload(1, 114, "watch", code))
	w := decodeWelcome(t, matchFrame(t, watch, msgSrvWelcome))
	flowWait(t, s, code, func(r *Room) bool { return r.bySeat[w.yourSeat].seatObserver })
	h.send(msgCliLeaveRoom, nil)
	flowWait(t, s, code, func(r *Room) bool { return r.host != nil && r.host.seat == 2 })
}
func TestMatchOfflineNodeFailsClosed(t *testing.T) {
	s, addr := startTestServer(t, nil)
	closed, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	dead := closed.Addr().String()
	closed.Close()
	matchConfigure(s, "1", []catalogwire.Node{matchNode("1", addr), matchNode("2", dead)})
	c := newTestClient(t, addr)
	c.send(msgMatch, matchRouteBytes(114, 2, "offline"))
	p := matchFrame(t, c, msgMatch)
	if len(p) != 3 || p[2] != 1 {
		t.Fatalf("offline accepted %x", p)
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if len(s.rooms) != 0 {
		t.Fatal("offline query created a split pool")
	}
}

func TestMatchCoordinatorBurstDoesNotSplitHalfEmptyRooms(t *testing.T) {
	a, addrA := startTestServer(t, nil)
	b, addrB := startTestServer(t, nil)
	nodes := []catalogwire.Node{matchNode("1", addrA), matchNode("2", addrB)}
	matchConfigure(a, "1", nodes)
	matchConfigure(b, "2", nodes)
	start := time.Now()
	out := make(chan []byte, 32)
	var workers sync.WaitGroup
	for i := 0; i < 32; i++ {
		workers.Add(1)
		go func(i int) {
			defer workers.Done()
			conn, err := net.DialTimeout("tcp", addrA, time.Second)
			if err != nil {
				out <- nil
				return
			}
			defer conn.Close()
			conn.SetDeadline(time.Now().Add(8 * time.Second))
			_, err = conn.Write(encodeFrame(msgMatch, matchRouteBytes(114, 1, fmt.Sprintf("burst-%d", i))))
			if err != nil {
				out <- nil
				return
			}
			typ, p, err := readFrame(conn)
			if err != nil || typ != msgMatch {
				out <- nil
				return
			}
			out <- p
		}(i)
	}
	workers.Wait()
	close(out)
	counts := map[string]int{}
	for p := range out {
		if len(p) != 26 || p[2] != 0 {
			t.Fatalf("burst route failed: %x", p)
		}
		counts[fmt.Sprintf("%d/%s", binary.LittleEndian.Uint16(p[4:]), p[6:10])]++
	}
	full, partial := 0, 0
	for _, count := range counts {
		if count == 6 {
			full++
		} else if count == 2 {
			partial++
		} else {
			t.Fatalf("split pool: %v", counts)
		}
	}
	if full != 5 || partial != 1 {
		t.Fatalf("32 players did not form 5 full and 1 partial room: %v", counts)
	}
	t.Logf("32 actual TCP routes: %v; rooms=%v", time.Since(start), counts)
}

func TestMatchSecondReservationCanBecomeFirstHost(t *testing.T) {
	s, addr := startTestServer(t, nil)
	one := s.reserveMatch(114, 0, name16("one"))
	two := s.reserveMatch(114, 0, name16("two"))
	second := newTestClient(t, addr)
	second.send(msgMatch, matchCommitBytes(matchReserveReply(two), 114, "two"))
	w := decodeWelcome(t, matchFrame(t, second, msgSrvWelcome))
	if w.yourSeat != 2 || w.hostSeat != 2 {
		t.Fatalf("first committed member must be host: %+v", w)
	}
	first := newTestClient(t, addr)
	first.send(msgMatch, matchCommitBytes(matchReserveReply(one), 114, "one"))
	w = decodeWelcome(t, matchFrame(t, first, msgSrvWelcome))
	if w.yourSeat != 1 || w.hostSeat != 2 {
		t.Fatal("late first reservation stole host authority")
	}
	// A stolen public name without its ticket cannot consume a reservation.
	third := s.reserveMatch(114, 0, name16("three"))
	bad := newTestClient(t, addr)
	bad.send(msgMatch, matchCommitBytes(matchReserveReply(third), 114, "impostor"))
	if p := matchFrame(t, bad, msgSrvReject); p[2] != rejectJoinClosed {
		t.Fatal("wrong ticket identity was accepted")
	}
	correct := newTestClient(t, addr)
	correct.send(msgMatch, matchCommitBytes(matchReserveReply(third), 114, "three"))
	matchFrame(t, correct, msgSrvWelcome)
}

func TestMatchCancelReleasesHoldAfterCommitConnectionDrops(t *testing.T) {
	s, addr := startTestServer(t, nil)
	reservation := s.reserveMatch(114, 2, name16("lost-welcome"))
	code := string(reservation[4:8])
	game := newTestClient(t, addr)
	game.send(msgMatch, matchCommitBytes(matchReserveReply(reservation), 114, "lost-welcome"))
	matchFrame(t, game, msgSrvWelcome)
	game.nc.Close()
	flowWait(t, s, code, func(r *Room) bool { return r.holdActive(1, time.Now()) && r.bySeat[1] == nil })
	cancel := newTestClient(t, addr)
	cancel.send(msgMatch, append([]byte{1, 4}, reservation[4:]...))
	matchFrame(t, cancel, msgMatch)
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.rooms[code] != nil {
		t.Fatal("cancel after an uncertain join left a 60-second reconnect hold")
	}
}

func TestMatchCancellationCannotRemoveAnotherPlayerAfterSeatSwap(t *testing.T) {
	s, addr := startTestServer(t, nil)
	hTicket := s.reserveMatch(114, 2, name16("host"))
	gTicket := s.reserveMatch(114, 2, name16("guest"))
	code := string(hTicket[4:8])
	h, g := newTestClient(t, addr), newTestClient(t, addr)
	h.send(msgMatch, matchCommitBytes(matchReserveReply(hTicket), 114, "host"))
	matchFrame(t, h, msgSrvWelcome)
	g.send(msgMatch, matchCommitBytes(matchReserveReply(gTicket), 114, "guest"))
	matchFrame(t, g, msgSrvWelcome)
	h.send(msgCliFlow, []byte{1, 0})
	g.send(msgCliFlow, []byte{1, 0})
	flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled && r.flowMask() == 6 })
	s.mu.Lock()
	host := s.rooms[code].host
	s.mu.Unlock()
	s.handleSwapCommitWithTerm(host, 1, 2, 1, 0, 0)
	flowWait(t, s, code, func(r *Room) bool { return r.hostSeat == 2 && r.bySeat[2] == host })
	c := newTestClient(t, addr)
	c.send(msgMatch, append([]byte{1, 4}, hTicket[4:]...))
	matchFrame(t, c, msgMatch)
	flowWait(t, s, code, func(r *Room) bool {
		return r.bySeat[1] != nil && string(r.bySeat[1].name[:5]) == "guest" && r.bySeat[2] == nil && !r.holdActive(2, time.Now())
	})
	g.send(msgCliLeaveRoom, nil)
	end := time.Now().Add(time.Second)
	for time.Now().Before(end) {
		s.mu.Lock()
		exists := s.rooms[code] != nil
		s.mu.Unlock()
		if !exists {
			return
		}
		time.Sleep(time.Millisecond)
	}
	t.Fatal("normal departure retained a committed cancellation record as a ghost seat")
}
