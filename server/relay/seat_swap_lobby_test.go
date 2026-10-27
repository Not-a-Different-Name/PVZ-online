package main

import (
	"testing"
	"time"
)

func TestFlowLobbySeatSwapBeforeAndAfterGame(t *testing.T) {
	for _, afterGame := range []bool{false, true} {
		name := "fresh"
		if afterGame {
			name = "returned"
		}
		t.Run(name, func(t *testing.T) {
			s, _, code, h, g := flowPair(t)
			if afterGame {
				flowBegin(t, s, code, h, g)
				s.mu.Lock()
				s.returnFlowToMenu(s.rooms[code], s.rooms[code].host)
				s.mu.Unlock()
			}
			s.mu.Lock()
			r := s.rooms[code]
			hc, gc, term, epoch := r.bySeat[1], r.bySeat[2], r.flow.term, r.flow.epoch
			s.mu.Unlock()
			// An approved legacy lobby exchange has no preparation request number.
			s.handleSwapCommitWithTerm(hc, 1, 2, term, epoch, 0)
			s.mu.Lock()
			ok := r.bySeat[2] == hc && r.bySeat[1] == gc && r.host == hc && r.hostSeat == 2 && r.flow.phase == flowLobby
			s.mu.Unlock()
			if !ok {
				t.Fatal("approved lobby exchange was dropped")
			}
			// The same packet cannot toggle the seats back after its sender moved.
			s.handleSwapCommitWithTerm(hc, 1, 2, term, epoch, 0)
			s.mu.Lock()
			ok = r.bySeat[2] == hc && r.hostSeat == 2
			s.mu.Unlock()
			if !ok {
				t.Fatal("duplicate lobby commit toggled the seats back")
			}
			_ = g
		})
	}
}

func TestFlowLobbySeatSwapRejectsInvalidContext(t *testing.T) {
	s, _, code, _, _ := flowPair(t)
	s.mu.Lock()
	r := s.rooms[code]
	hc, gc := r.bySeat[1], r.bySeat[2]
	s.mu.Unlock()
	for _, tag := range [][3]uint32{{0, 0, 0}, {1, 1, 0}, {1, 0, 9}} {
		s.handleSwapCommitWithTerm(hc, 1, 2, tag[0], tag[1], tag[2])
	}
	s.handleSwapCommitWithTerm(gc, 1, 2, 1, 0, 0)
	s.mu.Lock()
	ok := r.bySeat[1] == hc && r.hostSeat == 1
	s.mu.Unlock()
	if !ok {
		t.Fatal("stale or wrong-owner lobby commit applied")
	}
}

func TestFlowReturnedLobbyForwardsSwapConsent(t *testing.T) {
	s, _, code, h, g := flowPair(t)
	flowBegin(t, s, code, h, g)
	s.mu.Lock()
	r := s.rooms[code]
	s.returnFlowToMenu(r, r.host)
	term, epoch := r.flow.term, r.flow.epoch
	s.mu.Unlock()
	request := []byte{1, 2}
	h.send(msgCliFlowGame, flowGameBytes(term, epoch, 10, request))
	// Ignore prior state publications; the request must reach the other client.
	f := g.next(time.Second)
	for f.typ != msgSrvFlowGame || len(f.payload) < 12 || f.payload[8] != 10 {
		f = g.next(time.Second)
	}
	if f.payload[10] != 1 || f.payload[11] != 2 {
		t.Fatal("wrong lobby request")
	}
}
