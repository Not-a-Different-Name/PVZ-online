package main

import (
	"bytes"
	"encoding/binary"
	"testing"
	"time"
)

func localControl(src, dst, op byte, watch, stream, epoch uint32) []byte {
	p := obsControl(src, dst, op, watch, stream, epoch)
	binary.LittleEndian.PutUint32(p[18:], observerLocalRevision)
	return p
}
func localFixture(t *testing.T) (*Server, *Room, []*Conn) {
	t.Helper()
	s := newServer(func(string, ...any) {})
	r := &Room{}
	all := make([]*Conn, 7)
	for seat := 1; seat <= 6; seat++ {
		c := &Conn{srv: s, seat: uint8(seat), build: 95, observerCap: true, observerRevision: observerLocalRevision, send: make(chan outMsg, 64), done: make(chan struct{})}
		c.room.Store(r)
		r.bySeat[seat] = c
		all[seat] = c
	}
	return s, r, all
}
func TestObserverLocalSingleUploadFiveViewersAndNoPeriodicState(t *testing.T) {
	s, r, c := localFixture(t)
	for seat := 2; seat <= 6; seat++ {
		s.observerLocked(r, c[seat], c[1], 30, localControl(uint8(seat), 1, 1, uint32(100+seat), 1, 1), false)
		s.observerLocked(r, c[1], c[seat], 30, localControl(1, uint8(seat), 2, uint32(100+seat), 9, 1), false)
	}
	data := obsData()
	data[0] = 3
	parts := obsParts(1, 2, 102, 9, 1, 1, data)
	for _, p := range parts {
		s.observerLocked(r, c[1], c[2], 31, p, false)
	}
	for seat := 2; seat <= 6; seat++ {
		if !r.observer[seat].delivered {
			t.Fatalf("viewer%d was not served by the one upload", seat)
		}
		var body []byte
		for _, f := range c[seat].observer.frames {
			p := f[4:]
			if p[1] != uint8(seat) || binary.LittleEndian.Uint32(p[4:]) != uint32(100+seat) {
				t.Fatal("wrong authenticated routing")
			}
			body = append(body, p[40:]...)
		}
		if !bytes.Equal(body, data) {
			t.Fatal("fanout changed immutable snapshot")
		}
	}
	// Once delivered, later source frames cannot become an unsolicited stream.
	before := s.observerMemory.Load()
	for _, p := range obsParts(1, 2, 102, 9, 1, 2, data) {
		s.observerLocked(r, c[1], c[2], 31, p, false)
	}
	if s.observerMemory.Load() != before {
		t.Fatal("periodic state accepted")
	}
	for seat := 2; seat <= 6; seat++ {
		r.observer[seat].touched = time.Now().Add(-24 * time.Hour)
	}
	s.sweepRoom(r, time.Now())
	for seat := 2; seat <= 6; seat++ {
		if r.observer[seat] == nil {
			t.Fatal("valid one-shot view expired due to no frames")
		}
	}
	s.clearObserverConn(r, c[1])
	if s.observerMemory.Load() != 0 {
		t.Fatal("source departure leaked snapshot memory")
	}
}
func TestObserverLocalRefreshCooldownCannotBeResetByStopOrSwitch(t *testing.T) {
	s, r, c := localFixture(t)
	s.observerLocked(r, c[2], c[1], 30, localControl(2, 1, 1, 102, 1, 1), false)
	s.observerLocked(r, c[1], c[2], 30, localControl(1, 2, 2, 102, 9, 1), false)
	s.observerLocked(r, c[2], c[1], 30, localControl(2, 1, 4, 102, 9, 1), false)
	if r.observer[2] != nil {
		t.Fatal("STOP retained subscription")
	}
	s.observerLocked(r, c[2], c[3], 30, localControl(2, 3, 1, 103, 1, 1), false)
	if r.observer[2] != nil {
		t.Fatal("target switch bypassed cooldown")
	}
	c[2].observerRequestAt = time.Now().Add(-6 * time.Second)
	s.observerLocked(r, c[2], c[3], 30, localControl(2, 3, 1, 104, 1, 1), false)
	if r.observer[2] == nil || r.observer[2].source != c[3] {
		t.Fatal("valid refresh blocked after cooldown")
	}
}
func TestObserverLocalNodeBudgetCountsFragmentsAndTransportOverhead(t *testing.T) {
	s, r, c := localFixture(t)
	_ = r
	frames := make([][]byte, 250)
	for i := range frames {
		frames[i] = make([]byte, 260)
	}
	c[2].observer.offer(s, frames)
	sent := 0
	for i := 0; i < 1000; i++ {
		f := c[2].observer.pop(s)
		if f == nil {
			break
		}
		sent += len(f) + 80
	}
	if sent > 12500 || sent < 12000 {
		t.Fatalf("node budget %d", sent)
	}
	if c[2].observer.bytes == 0 {
		t.Fatal("large snapshot must be paced instead of discarded")
	}
	s.observerBudgetMu.Lock()
	s.observerBudgetAt = time.Now().Add(-2 * time.Second)
	s.observerBudgetMu.Unlock()
	if c[2].observer.pop(s) == nil {
		t.Fatal("snapshot did not resume next window")
	}
	c[2].observer.clear(s)
	if s.observerMemory.Load() != 0 {
		t.Fatal("paced output leaked")
	}
}
