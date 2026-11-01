package main

import (
	"testing"
	"time"
)

func TestAuditControlUncongestedUpload(t *testing.T) {
	s, r, c := localFixture(t)
	s.observerLocked(r, c[2], c[1], 30, localControl(2, 1, 1, 102, 1, 1), false)
	s.observerLocked(r, c[1], c[2], 30, localControl(1, 2, 2, 102, 9, 1), false)
	data := make([]byte, 60000)
	copy(data, obsData())
	parts := obsParts(1, 2, 102, 9, 1, 1, data)
	for _, p := range parts {
		s.observerLocked(r, c[1], c[2], 31, p, false)
	}
	if !r.observer[2].delivered || c[2].observer.bytes == 0 {
		t.Fatal("control normal upload failed")
	}
}
func TestAuditControlUncongestedFanout(t *testing.T) {
	s, r, c := localFixture(t)
	for seat := 2; seat <= 6; seat++ {
		s.observerLocked(r, c[seat], c[1], 30, localControl(uint8(seat), 1, 1, uint32(100+seat), 1, 1), false)
		s.observerLocked(r, c[1], c[seat], 30, localControl(1, uint8(seat), 2, uint32(100+seat), 9, 1), false)
	}
	data := obsData()
	data[0] = 3
	for _, p := range obsParts(1, 2, 102, 9, 1, 1, data) {
		s.observerLocked(r, c[1], c[2], 31, p, false)
	}
	for seat := 2; seat <= 6; seat++ {
		if !r.observer[seat].delivered || c[seat].observer.bytes == 0 {
			t.Fatalf("control viewer%d failed", seat)
		}
	}
}

// Expected behavior tests. Failure is retained as evidence against unchanged code.
func TestAuditSlowModernUploadKeepsProgress(t *testing.T) {
	s, r, c := localFixture(t)
	s.observerLocked(r, c[2], c[1], 30, localControl(2, 1, 1, 102, 1, 1), false)
	s.observerLocked(r, c[1], c[2], 30, localControl(1, 2, 2, 102, 9, 1), false)
	data := make([]byte, 60000)
	copy(data, obsData())
	parts := obsParts(1, 2, 102, 9, 1, 1, data)
	s.observerLocked(r, c[1], c[2], 31, parts[0], false)
	v := r.observer[2]
	if v.next != 1 {
		t.Fatal("fixture did not accept first fragment")
	}
	v.started = time.Now().Add(-4 * time.Second)
	for _, p := range parts[1:] {
		s.observerLocked(r, c[1], c[2], 31, p, false)
	}
	if !v.delivered || c[2].observer.bytes == 0 {
		t.Fatalf("valid modern upload lost after 4s despite ordered remaining fragments: next=%d delivered=%v bytes=%d", v.next, v.delivered, c[2].observer.bytes)
	}
}
func TestAuditFanoutCannotClaimUndeliverableSuccess(t *testing.T) {
	s, r, c := localFixture(t)
	for seat := 2; seat <= 6; seat++ {
		s.observerLocked(r, c[seat], c[1], 30, localControl(uint8(seat), 1, 1, uint32(100+seat), 1, 1), false)
		s.observerLocked(r, c[1], c[seat], 30, localControl(1, uint8(seat), 2, uint32(100+seat), 9, 1), false)
	}
	data := obsData()
	data[0] = 3
	parts := obsParts(1, 2, 102, 9, 1, 1, data)
	cost := 0
	for _, p := range parts {
		cost += len(encodeFrame(31, p))
	}
	// Other rooms own most of the bounded display pool. Allow source assembly,
	// but only two fanout frames. Model pressure without allocating huge buffers.
	s.observerMemory.Store(int64(observerMemoryLimit - 2*cost))
	for _, p := range parts {
		s.observerLocked(r, c[1], c[2], 31, p, false)
	}
	bad := 0
	for seat := 2; seat <= 6; seat++ {
		if r.observer[seat].delivered && c[seat].observer.bytes == 0 {
			bad++
		}
	}
	if bad > 0 {
		t.Fatalf("%d viewers marked delivered although bounded queue accepted no picture", bad)
	}
}

func TestAuditOversizeAssemblyDoesNotCorruptSharedMemoryCounter(t *testing.T) {
	s, r, c := localFixture(t)
	s.observerLocked(r, c[2], c[1], 30, localControl(2, 1, 1, 102, 1, 1), false)
	s.observerLocked(r, c[1], c[2], 30, localControl(1, 2, 2, 102, 9, 1), false)
	data := make([]byte, 304*216)
	copy(data, obsData())
	for _, p := range obsParts(1, 2, 102, 9, 1, 1, data) {
		s.observerLocked(r, c[1], c[2], 31, p, false)
	}
	if s.observerMemory.Load() != 0 {
		t.Fatalf("oversize input changed shared memory counter: %d", s.observerMemory.Load())
	}
}

func TestAuditRoomSweeperPreservesProgressAndExpiresStall(t *testing.T) {
	s, r, c := localFixture(t)
	s.observerLocked(r, c[2], c[1], 30, localControl(2, 1, 1, 102, 1, 1), false)
	s.observerLocked(r, c[1], c[2], 30, localControl(1, 2, 2, 102, 9, 1), false)
	data := make([]byte, 60000)
	copy(data, obsData())
	parts := obsParts(1, 2, 102, 9, 1, 1, data)
	s.observerLocked(r, c[1], c[2], 31, parts[0], false)
	v := r.observer[2]
	now := time.Now()
	v.started = now.Add(-4 * time.Second)
	v.progress = now
	s.sweepRoom(r, now)
	if v.count == 0 || v.next != 1 {
		t.Fatal("periodic sweeper discarded healthy slow assembly")
	}
	v.progress = now.Add(-13 * time.Second)
	s.sweepRoom(r, now)
	if v.count != 0 || s.observerMemory.Load() != 0 {
		t.Fatal("stalled assembly retained memory")
	}
}
