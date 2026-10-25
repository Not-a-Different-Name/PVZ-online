package main

import (
	"bytes"
	"encoding/binary"
	"testing"
	"time"
)

func TestObserverABDisabledIsMOD96WireEquivalent(t *testing.T) {
	trace := func(build uint16) [][]byte {
		s := newServer(func(string, ...any) {})
		cfg := defaultOperationsConfig()
		cfg.PunchEnable = false
		s.ops.current.Store(&operationsSnapshot{Config: cfg})
		r := &Room{code: "AB12"}
		makeC := func(seat uint8) *Conn {
			c := &Conn{srv: s, seat: seat, build: build, send: make(chan outMsg, 64), done: make(chan struct{})}
			c.room.Store(r)
			r.bySeat[seat] = c
			return c
		}
		a, b := makeC(1), makeC(2)
		var out [][]byte
		cap := make([]byte, 4)
		binary.LittleEndian.PutUint32(cap, observerLocalRevision)
		for _, c := range []*Conn{a, b} {
			s.handleObserverCapability(c, cap)
			f := (<-c.send).frame
			if len(f) != 12 {
				t.Fatal("disabled capability changed original 8 byte payload")
			}
			out = append(out, f)
		}
		control := func(src, dst, op byte) []byte {
			p := obsControl(src, dst, op, 777, 9, 1)
			binary.LittleEndian.PutUint32(p[18:], observerLocalRevision)
			return p
		}
		begin := control(2, 1, 1)
		s.forward(b, 30, begin)
		out = append(out, (<-a.send).frame)
		accept := control(1, 2, 2)
		s.forward(a, 30, accept)
		out = append(out, (<-b.send).frame)
		for _, p := range obsParts(1, 2, 777, 9, 1, 1, obsData()) {
			s.forward(a, 31, p)
		}
		for {
			f := b.observer.pop(s)
			if f == nil {
				break
			}
			out = append(out, f)
		}
		s.forward(a, 16, []byte{1, 2, 1})
		out = append(out, (<-b.send).frame)
		stop := control(2, 1, 4)
		s.forward(b, 30, stop)
		out = append(out, (<-a.send).frame)
		req := []byte{punchOpCapable, 2, 0, 0, 0}
		s.handlePunchControl(b, req)
		if len(a.send) != 0 || len(b.send) != 0 || s.punch.capable.Load() != 0 || s.observerMemory.Load() != 0 {
			t.Fatal("off switch left unsolicited control/data or memory")
		}
		return out
	}
	old, new := trace(96), trace(97)
	if len(old) != len(new) {
		t.Fatal("wire frame count changed")
	}
	for i := range old {
		if !bytes.Equal(old[i], new[i]) {
			t.Fatalf("MOD96 wire differs at frame %d", i)
		}
	}
}

func TestObserverABBoundGrantSourceLimitAndRevocation(t *testing.T) {
	s := newServer(func(string, ...any) {})
	r := &Room{code: "AB12"}
	s.punch.hellos = make(map[[16]byte]*helloGrant)
	s.punch.pairs = make(map[[16]byte]*punchPair)
	peers := make([]*Conn, 7)
	for i := 1; i <= 6; i++ {
		c := &Conn{srv: s, seat: uint8(i), build: 97, send: make(chan outMsg, 64), done: make(chan struct{})}
		c.punch.capable = true
		c.punch.revision = 2
		c.punch.endpoint = &punchEndpoint{ip: [4]byte{127, 0, 0, 1}, port: uint16(10000 + i)}
		c.room.Store(r)
		r.bySeat[i] = c
		peers[i] = c
	}
	source := peers[1]
	request := func(i int, watch uint32) { s.punchBeginContext(peers[i], 1, watch, 9, 1) }
	request(2, 102)
	if len(s.punch.pairs) != 0 {
		t.Fatal("unsubscribed pair granted")
	}
	for i := 2; i <= 6; i++ {
		r.observer[i] = &observerSubscription{viewer: peers[i], source: source, watch: uint32(100 + i), stream: 9, epoch: 1}
		request(i, uint32(100+i))
	}
	if len(s.punch.pairs) != 3 {
		t.Fatalf("source grant cap=%d", len(s.punch.pairs))
	}
	for _, p := range s.punch.pairs {
		if p.viewer == nil || p.source != source || p.watch == 0 || p.stream != 9 || p.epoch != 1 {
			t.Fatal("grant identity missing")
		}
	}
	// STOP/target replacement removes stale grants before allocating the next one.
	r.observer[2] = nil
	request(5, 105)
	if len(s.punch.pairs) != 3 {
		t.Fatal("revoked source slot not reusable")
	}
	for _, p := range s.punch.pairs {
		if p.viewer == peers[2] {
			t.Fatal("revoked token retained")
		}
	}
	// A same-seat replacement cannot report success for a previous connection.
	var token [16]byte
	var pair *punchPair
	for key, p := range s.punch.pairs {
		token = key
		pair = p
		break
	}
	c := pair.viewer
	s.punchResultToken(c, pair.sourceSeat, 1, 10, &token)
	s.punchResultToken(source, pair.viewerSeat, 1, 10, &token)
	if !pair.direct {
		t.Fatal("bound bidirectional reports did not agree")
	}
	count := s.punch.opened.Load()
	s.punchResultToken(c, pair.sourceSeat, 1, 10, &token)
	if s.punch.opened.Load() != count {
		t.Fatal("duplicate report inflated success")
	}
	pair.expires = time.Now().Add(-time.Second)
	pair.direct = false
	s.punchResultToken(c, pair.sourceSeat, 1, 10, &token)
	if pair.direct {
		t.Fatal("expired grant accepted result")
	}
	r.roundWaitMask = 1 << 1
	before := len(peers[6].send)
	request(6, 106)
	if len(peers[6].send) != before {
		t.Fatal("waiting source granted")
	}
}
