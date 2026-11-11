package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"testing"
	"time"
)

func flowWait(t *testing.T, s *Server, code string, predicate func(*Room) bool) {
	t.Helper()
	end := time.Now().Add(2 * time.Second)
	for time.Now().Before(end) {
		s.mu.Lock()
		r := s.rooms[code]
		ok := r != nil && predicate(r)
		s.mu.Unlock()
		if ok {
			return
		}
		time.Sleep(time.Millisecond)
	}
	t.Fatal("flow state did not settle")
}
func flowPair(t *testing.T) (*Server, string, string, *testClient, *testClient) {
	s, addr := startTestServer(t, nil)
	h, g := newTestClient(t, addr), newTestClient(t, addr)
	code := decodeWelcome(t, h.create(1, 83, "host")).code
	decodeWelcome(t, g.join(1, 83, "guest", code))
	h.send(msgCliFlow, []byte{1, 0})
	g.send(msgCliFlow, []byte{1, 0})
	flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled && r.bySeat[1].flow && r.bySeat[2].flow })
	return s, addr, code, h, g
}
func flowStartBytes(term, epoch uint32, seat uint8) []byte {
	p := make([]byte, 53)
	binary.LittleEndian.PutUint32(p, term)
	binary.LittleEndian.PutUint32(p[4:], epoch)
	p[8] = seat
	binary.LittleEndian.PutUint32(p[48:], epoch)
	return p
}
func flowProgressBytes(term, epoch uint32, op, value uint8, generation uint32) []byte {
	p := make([]byte, 14)
	binary.LittleEndian.PutUint32(p, term)
	binary.LittleEndian.PutUint32(p[4:], epoch)
	p[8], p[9] = op, value
	binary.LittleEndian.PutUint32(p[10:], generation)
	return p
}
func flowGameBytes(term, epoch uint32, typ uint16, body []byte) []byte {
	p := make([]byte, 10+len(body))
	binary.LittleEndian.PutUint32(p, term)
	binary.LittleEndian.PutUint32(p[4:], epoch)
	binary.LittleEndian.PutUint16(p[8:], typ)
	copy(p[10:], body)
	return p
}
func flowBegin(t *testing.T, s *Server, code string, h, g *testClient) {
	h.send(msgCliFlowStart, flowStartBytes(1, 1, 1))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.epoch == 1 })
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 0, 0))
	g.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 0, 0))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowBattle })
}

func TestFlowStartSnapshotReplay(t *testing.T) {
	for _, size := range []int{45, 46, 53, 54, 56} {
		t.Run(fmt.Sprint(size), func(t *testing.T) {
			var s *Server
			var code string
			var h, g *testClient
			if size == eliteWavesStartBytes {
				var addr string
				s, addr = startTestServer(t, nil)
				h, g = newTestClient(t, addr), newTestClient(t, addr)
				code = decodeWelcome(t, h.create(1, eliteWavesBuild, "host")).code
				decodeWelcome(t, g.join(1, eliteWavesBuild, "guest", code))
				h.send(msgCliFlow, []byte{1, 0})
				g.send(msgCliFlow, []byte{1, 0})
				flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled && r.flowMask() == 6 })
			} else {
				s, _, code, h, g = flowPair(t)
			}
			body := make([]byte, size)
			body[0], body[1] = 1, 2
			if size >= 46 {
				body[40] = 7 // Cost surcharge stack must not be mistaken for epoch.
			}
			if size == eliteWavesStartBytes {
				body[size-7], body[size-6] = 1, 1
			}
			binary.LittleEndian.PutUint32(body[size-5:], 1)
			p := make([]byte, 8+size)
			binary.LittleEndian.PutUint32(p, 1)
			binary.LittleEndian.PutUint32(p[4:], 1)
			copy(p[8:], body)
			h.send(msgCliFlowStart, p)
			flowWait(t, s, code, func(r *Room) bool { return r.flow.epoch == 1 })
			readStart := func() {
				for i := 0; i < 12; i++ {
					f := g.next(time.Second)
					if f.typ == msgSrvFlowGame && len(f.payload) >= 10 && binary.LittleEndian.Uint16(f.payload[8:]) == 3 {
						if !bytes.Equal(f.payload[10:], body) {
							t.Fatalf("START snapshot changed: %x", f.payload)
						}
						return
					}
				}
				t.Fatal("START snapshot missing")
			}
			readStart()
			h.send(msgCliFlowGame, flowGameBytes(1, 1, 3, body))
			readStart()
		})
	}
}
func flowPrepare(t *testing.T, s *Server, code string, h, g *testClient) uint32 {
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 1, 0))
	g.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 1, 0))
	var gen uint32
	flowWait(t, s, code, func(r *Room) bool { gen = r.flow.generation; return r.flow.phase == flowVerify })
	return gen
}
func TestFlowNoEarlyAdvanceOrMissingApplication(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	flowBegin(t, s, code, h, g)
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 1, 0))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.done&(1<<1) != 0 && r.flow.phase == flowBattle })
	h.send(msgCliFlowStart, flowStartBytes(1, 2, 1))
	gen := flowPrepare(t, s, code, h, g)
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 2, 1, gen))
	g.send(msgCliFlowProgress, flowProgressBytes(1, 1, 2, 1, gen))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowComplete && r.flow.epoch == 1 })
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 3, 1, gen))
	h.send(msgCliFlowStart, flowStartBytes(1, 2, 1))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.applied == 1<<1 && r.flow.epoch == 1 })
	g.send(msgCliFlowProgress, flowProgressBytes(1, 1, 3, 1, gen))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowIntermission })
	h.send(msgCliFlowStart, flowStartBytes(1, 2, 1))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.epoch == 2 && r.flow.phase == flowSetup })
}
func TestFlowEscapeInvalidatesAckGeneration(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	flowBegin(t, s, code, h, g)
	gen := flowPrepare(t, s, code, h, g)
	g.send(msgCliFlowProgress, flowProgressBytes(1, 1, 2, 1, gen))
	h.send(msgCliFlowGame, flowGameBytes(1, 1, 5, []byte{1, 2, 0}))
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 2, 1, gen))
	flowWait(t, s, code, func(r *Room) bool {
		return r.flow.phase == flowBattle && r.flow.done&(1<<2) == 0 && r.flow.ack == 0 && r.flow.generation > gen
	})
}
func TestFlowHostElectionHoldAndSoloCompletion(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	flowBegin(t, s, code, h, g)
	h.nc.Close()
	flowWait(t, s, code, func(r *Room) bool {
		return r.hostSeat == 2 && r.host == r.bySeat[2] && r.flow.term == 2 && r.holdActive(1, time.Now())
	})
	g.send(msgCliFlowProgress, flowProgressBytes(2, 1, 1, 1, 0))
	var gen uint32
	flowWait(t, s, code, func(r *Room) bool { gen = r.flow.generation; return r.flow.phase == flowVerify })
	g.send(msgCliFlowProgress, flowProgressBytes(2, 1, 2, 1, gen))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowComplete })
	g.send(msgCliFlowProgress, flowProgressBytes(2, 1, 3, 1, gen))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowIntermission })
	g.send(msgCliFlowStart, flowStartBytes(2, 2, 2))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.epoch == 2 && r.flow.phase == flowSetup })
	s.mu.Lock()
	deadline := s.rooms[code].holds[1].until
	s.mu.Unlock()
	s.sweepOnce(deadline.Add(time.Second))
	flowWait(t, s, code, func(r *Room) bool {
		return r.hostSeat == 2 && !r.holdActive(1, time.Now()) && r.roundWaitMask&(1<<1) == 0
	})
}
func TestFlowRejectsStaleRoundAuthorityAndSpoofedSeats(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	flowBegin(t, s, code, h, g)
	tests := []struct {
		name        string
		term, epoch uint32
		body        []byte
	}{
		{"old term", 0, 1, []byte{1, 2}}, {"old round", 1, 0, []byte{1, 2}},
		{"future round", 1, 2, []byte{1, 2}}, {"wrong src", 1, 1, []byte{2, 1}},
		{"bad dst", 1, 1, []byte{1, 7}}, {"self dst", 1, 1, []byte{1, 1}},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) { h.send(msgCliFlowGame, flowGameBytes(tt.term, tt.epoch, 14, tt.body)) })
	}
	g.send(msgCliFlowGame, flowGameBytes(1, 1, 14, []byte{2, 1})) // guest cannot send host GO.
	time.Sleep(25 * time.Millisecond)
	if got := s.stats.forwarded.Load(); got != 1 {
		t.Fatalf("unauthorized gameplay was forwarded: %d (only START expected)", got)
	}
}
func TestFlowFullSizeSnapshotsAndEnvelopeBound(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	flowBegin(t, s, code, h, g)
	body := bytes.Repeat([]byte{0xA5}, 256)
	body[0], body[1] = 1, 2
	h.send(msgCliFlowGame, flowGameBytes(1, 1, 18, body))
	for n := 0; n < 30; n++ {
		f := g.next(time.Second)
		if f.typ == msgSrvFlowGame && binary.LittleEndian.Uint16(f.payload[8:]) == 18 {
			if len(f.payload) != 266 || !bytes.Equal(f.payload[10:], body) {
				t.Fatal("snapshot changed")
			}
			return
		}
	}
	t.Fatal("full snapshot was not delivered")
}
func TestFlowPlainFramesCannotBypassTerm(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	flowBegin(t, s, code, h, g)
	h.send(14, []byte{1, 2})
	h.send(5, []byte{1, 2, 0})
	time.Sleep(20 * time.Millisecond)
	if s.stats.forwarded.Load() != 1 {
		t.Fatal("plain gameplay bypassed flow authority")
	}
}

func TestFlowSixPlayersMigrationAndCompletion(t *testing.T) {
	s, addr := startTestServer(t, nil)
	clients := make([]*testClient, 6)
	clients[0] = newTestClient(t, addr)
	code := decodeWelcome(t, clients[0].create(1, 83, "host")).code
	for i := 1; i < 6; i++ {
		clients[i] = newTestClient(t, addr)
		decodeWelcome(t, clients[i].join(1, 83, "peer", code))
	}
	for _, c := range clients {
		c.send(msgCliFlow, []byte{1, 0})
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled && r.bySeat[6].flow })
	clients[0].send(msgCliFlowStart, flowStartBytes(1, 1, 1))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowSetup })
	for i, c := range clients {
		value := uint8(1)
		if i == 5 {
			value = 0
		}
		c.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, value, 0))
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowBattle && r.flow.done == 0x3e })
	clients[0].nc.Close()
	flowWait(t, s, code, func(r *Room) bool { return r.hostSeat == 2 && r.flow.term == 2 })
	clients[2].nc.Close()
	flowWait(t, s, code, func(r *Room) bool { return r.flowMask() == 0x74 })
	clients[5].send(msgCliFlowProgress, flowProgressBytes(2, 1, 1, 1, 0))
	var gen uint32
	flowWait(t, s, code, func(r *Room) bool { gen = r.flow.generation; return r.flow.phase == flowVerify })
	for _, i := range []int{1, 3, 4, 5} {
		clients[i].send(msgCliFlowProgress, flowProgressBytes(2, 1, 2, 1, gen))
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowComplete })
	for _, i := range []int{1, 3, 4} {
		clients[i].send(msgCliFlowProgress, flowProgressBytes(2, 1, 3, 1, gen))
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flow.applied == 0x34 && r.flow.phase == flowComplete })
	clients[5].send(msgCliFlowProgress, flowProgressBytes(2, 1, 3, 1, gen))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowIntermission })
	clients[1].send(msgCliFlowStart, flowStartBytes(2, 2, 2))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.epoch == 2 && r.flow.phase == flowSetup })
}
func TestFlowSeatCommitBoundToTermRoundAndRequest(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	h.send(msgCliFlowStart, flowStartBytes(1, 1, 1))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowSetup })
	body := make([]byte, 92)
	body[0], body[1], body[2] = 1, 2, 6
	binary.LittleEndian.PutUint32(body[3:], 1)
	binary.LittleEndian.PutUint32(body[7:], 10)
	binary.LittleEndian.PutUint32(body[11:], 9)
	body[39], body[42], body[43] = 3, 1, 2
	h.send(msgCliFlowGame, flowGameBytes(1, 1, 25, body))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.prepRequest == 9 && r.flow.prepPhase == 3 })
	s.mu.Lock()
	hc, gc := s.rooms[code].bySeat[1], s.rooms[code].bySeat[2]
	s.mu.Unlock()
	for _, tag := range [][3]uint32{{0, 1, 9}, {1, 0, 9}, {1, 1, 8}} {
		s.handleSwapCommitWithTerm(hc, 1, 2, tag[0], tag[1], tag[2])
	}
	s.mu.Lock()
	unchanged := s.rooms[code].bySeat[1] == hc
	s.mu.Unlock()
	if !unchanged {
		t.Fatal("stale swap committed")
	}
	s.handleSwapCommitWithTerm(hc, 1, 2, 1, 1, 9)
	s.handleSwapCommitWithTerm(hc, 1, 2, 1, 1, 9)
	s.mu.Lock()
	r := s.rooms[code]
	swappedOnce := r.bySeat[1] == gc && r.bySeat[2] == hc && r.hostSeat == 2
	s.mu.Unlock()
	if !swappedOnce {
		t.Fatal("swap was rejected or replayed twice")
	}
	_ = g
}
func TestFlowLossDoesNotBecomeVictoryAndAllowsSoloRetry(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	flowBegin(t, s, code, h, g)
	g.nc.Close()
	flowWait(t, s, code, func(r *Room) bool { return r.flowMask() == 1<<1 })
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 4, 1, 0))
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 1, 0))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.lost && r.flow.phase == flowBattle && r.flow.done == 1<<1 })
	h.send(msgCliFlowStart, flowStartBytes(1, 2, 1))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.epoch == 2 && !r.flow.lost && r.flow.phase == flowSetup })
}
func TestFlowOnlyWaitingPlayersCannotAutoWin(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	flowBegin(t, s, code, h, g)
	s.mu.Lock()
	r := s.rooms[code]
	r.roundWaitMask = 1 << 2
	s.mu.Unlock()
	h.nc.Close()
	flowWait(t, s, code, func(r *Room) bool { return r.hostSeat == 2 && r.flowMask() == 0 && r.flow.phase == flowBattle })
	g.send(msgCliFlowProgress, flowProgressBytes(2, 1, 1, 1, 0))
	g.send(msgCliFlowStart, flowStartBytes(2, 2, 2))
	time.Sleep(20 * time.Millisecond)
	flowWait(t, s, code, func(r *Room) bool { return r.flow.epoch == 1 && r.flow.phase == flowBattle })
}

func TestFlowLegacyRoundNeverChangesModeMidBattle(t *testing.T) {
	s, addr := startTestServer(t, nil)
	h, g := newTestClient(t, addr), newTestClient(t, addr)
	code := decodeWelcome(t, h.create(1, 83, "host")).code
	decodeWelcome(t, g.join(1, 83, "guest", code))
	h.send(msgCliFlow, []byte{1, 0})
	flowWait(t, s, code, func(r *Room) bool { return r.bySeat[1].flow && !r.flow.enabled })
	body := make([]byte, 45)
	body[0], body[1] = 1, 2
	h.send(3, body)
	flowWait(t, s, code, func(r *Room) bool { return r.started && !r.flow.started })
	g.send(msgCliFlow, []byte{1, 0})
	flowWait(t, s, code, func(r *Room) bool { return r.bySeat[2].flow && !r.flow.enabled })
	h.send(14, []byte{1, 2})
	flowWait(t, s, code, func(r *Room) bool { return s.stats.forwarded.Load() == 2 })
}

func TestFlowEmptyRoomRejoinElectsOnlineAuthorityWithoutWinning(t *testing.T) {
	s, addr, code, h, g := flowPair(t)
	credential := admissionCredential(t, h)
	flowBegin(t, s, code, h, g)
	h.nc.Close()
	flowWait(t, s, code, func(r *Room) bool { return r.hostSeat == 2 && r.flow.term == 2 })
	g.nc.Close()
	flowWait(t, s, code, func(r *Room) bool { return r.host == nil && r.onlineMask() == 0 })
	back := newTestClient(t, addr)
	back.send(msgCliSecureRejoin, append(rejoinPayload(1, 83, "host", code, 1), credential...))
	w := decodeWelcome(t, back.next(time.Second).payload)
	if w.hostSeat != 1 {
		t.Fatal("rejoin did not select the online authority")
	}
	flowWait(t, s, code, func(r *Room) bool {
		return r.host == r.bySeat[1] && r.hostSeat == 1 && r.flow.term == 3 && r.flowMask() == 0 && r.flow.epoch == 1 && r.flow.phase == flowBattle
	})
}
