package main

import (
	"encoding/binary"
	"testing"
	"time"
)

func TestObserverMutualControlDirections(t *testing.T) {
	for _, modern := range []bool{false, true} {
		for _, sameWatch := range []bool{false, true} {
			s, r, c := localFixture(t)
			control := func(src, dst, op byte, watch, stream uint32) []byte {
				if modern {
					return localControl(src, dst, op, watch, stream, 1)
				}
				return obsControl(src, dst, op, watch, stream, 1)
			}
			if !modern {
				for _, p := range c {
					if p != nil {
						p.observerRevision = observerRevision
					}
				}
			}
			first, second := uint32(101), uint32(202)
			if sameWatch {
				second = first
			}
			// Both seats concurrently view the other's real lawn.
			s.observerLocked(r, c[1], c[2], 30, control(1, 2, 1, first, 1), false)
			s.observerLocked(r, c[2], c[1], 30, control(2, 1, 1, second, 1), false)
			a, b := r.observer[1], r.observer[2]
			if a == nil || b == nil {
				t.Fatal("mutual BEGIN missing")
			}
			s.observerLocked(r, c[2], c[1], 30, control(2, 1, 2, first, 7), false)
			s.observerLocked(r, c[1], c[2], 30, control(1, 2, 2, second, 9), false)
			if a.stream != 7 || b.stream != 9 || len(c[1].send) != 2 || len(c[2].send) != 2 {
				t.Fatalf("modern=%v sameWatch=%v mutual ACCEPT misrouted: streams=%d/%d queues=%d/%d", modern, sameWatch, a.stream, b.stream, len(c[1].send), len(c[2].send))
			}
			// Producer BUSY/UNSUPPORTED belong to the opposite seat's subscription.
			for _, op := range []byte{5, 6} {
				before := len(c[1].send)
				s.observerLocked(r, c[2], c[1], 30, control(2, 1, op, first, 7), false)
				if len(c[1].send) != before+1 {
					t.Fatalf("producer op %d not delivered", op)
				}
			}
			before := len(c[1].send)
			s.observerLocked(r, c[2], c[1], 30, control(2, 1, 2, 99999, 7), false)
			if len(c[1].send) != before {
				t.Fatal("foreign watch accepted")
			}
			// Even a colliding watch ID cannot select the producer slot for KEEPALIVE.
			old := time.Now().Add(-time.Second)
			a.touched = old
			b.touched = old
			s.observerLocked(r, c[1], c[2], 30, control(1, 2, 3, first, 7), false)
			if !a.touched.After(old) || !b.touched.Equal(old) {
				t.Fatal("KEEPALIVE touched reverse subscription")
			}
			// Producer stop has distinct stream identity; reverse direction survives.
			s.observerLocked(r, c[2], c[1], 30, control(2, 1, 4, first, 7), false)
			if r.observer[1] != nil || r.observer[2] != b {
				t.Fatal("producer STOP cleared wrong direction")
			}
			s.observerLocked(r, c[2], c[1], 30, control(2, 1, 4, second, 9), false)
			if r.observer[2] != nil {
				t.Fatal("viewer STOP did not clear remaining direction")
			}
			if s.observerMemory.Load() != 0 {
				t.Fatal("mutual cleanup leaked")
			}
		}
	}
}

func TestObserverMutualActualTCP(t *testing.T) {
	_, addr := startTestServer(t, nil)
	a, b := newTestClient(t, addr), newTestClient(t, addr)
	code := decodeWelcome(t, a.create(1, 106, "mutual-a")).code
	b.join(1, 106, "mutual-b", code)
	a.next(time.Second)
	capReq := make([]byte, 4)
	binary.LittleEndian.PutUint32(capReq, observerLocalRevision)
	for _, c := range []*testClient{a, b} {
		c.send(0xF023, capReq)
		obsNext(c, 0xF023)
	}
	a.send(30, localControl(1, 2, 1, 101, 1, 1))
	obsNext(b, 30)
	b.send(30, localControl(2, 1, 1, 202, 1, 1))
	obsNext(a, 30)
	b.send(30, localControl(2, 1, 2, 101, 7, 1))
	f := obsNext(a, 30)
	if f.payload[3] != 2 || binary.LittleEndian.Uint32(f.payload[6:]) != 101 {
		t.Fatal("first ACCEPT")
	}
	a.send(30, localControl(1, 2, 2, 202, 9, 1))
	f = obsNext(b, 30)
	if f.payload[3] != 2 || binary.LittleEndian.Uint32(f.payload[6:]) != 202 {
		t.Fatal("reverse ACCEPT")
	}
	for _, way := range []struct {
		source, viewer *testClient
		src, dst       byte
		watch, stream  uint32
	}{{b, a, 2, 1, 101, 7}, {a, b, 1, 2, 202, 9}} {
		parts := obsParts(way.src, way.dst, way.watch, way.stream, 1, 1, obsData())
		for _, p := range parts {
			way.source.send(31, p)
		}
		for range parts {
			if f := obsNext(way.viewer, 31); f.payload[0] != way.src || f.payload[1] != way.dst {
				t.Fatal("snapshot routing")
			}
		}
	}
	a.send(30, localControl(1, 2, 4, 101, 7, 1))
	obsNext(b, 30)
	a.send(16, []byte{1, 2, 1})
	if obsNext(b, 16).typ != 16 {
		t.Fatal("battle blocked after stop")
	}
}
