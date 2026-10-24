package main

import (
	"encoding/binary"
	"hash/crc32"
	"sync"
	"time"
)

const observerRevision = 0x9402
const observerLocalRevision = 0x9603
const observerNodeBytesPerSecond = 12500
const observerMemoryLimit = 8 * 1024 * 1024

// Display output has its own replaceable bounded queue. It never enters send,
// closes a gameplay connection, or writes from a second goroutine.
type observerOutput struct {
	mu           sync.Mutex
	frames       [][]byte
	pending      [][]byte
	pendingBytes int
	at, bytes    int
	window       time.Time
	spent        int
}

func (o *observerOutput) clear(s *Server) {
	o.mu.Lock()
	defer o.mu.Unlock()
	s.observerMemory.Add(-int64(o.bytes + o.pendingBytes))
	o.pending = nil
	o.pendingBytes = 0
	o.frames = nil
	o.at = 0
	o.bytes = 0
}
func (o *observerOutput) offer(s *Server, frames [][]byte) {
	n := 0
	for _, f := range frames {
		n += len(f)
	}
	o.mu.Lock()
	defer o.mu.Unlock()
	// Finish a partially written full frame; only its successor is replaceable.
	if o.at > 0 && o.at < len(o.frames) {
		s.observerMemory.Add(-int64(o.pendingBytes))
		o.pending = nil
		o.pendingBytes = 0
		if s.observerMemory.Add(int64(n)) > observerMemoryLimit {
			s.observerMemory.Add(-int64(n))
			return
		}
		o.pending = frames
		o.pendingBytes = n
		return
	}
	s.observerMemory.Add(-int64(o.bytes))
	o.frames = nil
	o.bytes = 0
	o.at = 0
	if s.observerMemory.Add(int64(n)) > observerMemoryLimit {
		s.observerMemory.Add(-int64(n))
		return
	}
	o.frames = frames
	o.bytes = n
}
func (o *observerOutput) pop(s *Server) []byte {
	o.mu.Lock()
	defer o.mu.Unlock()
	now := time.Now()
	if now.Sub(o.window) >= time.Second {
		o.window = now
		o.spent = 0
	}
	if o.at >= len(o.frames) || o.spent >= 24*1024 {
		return nil
	}
	f := o.frames[o.at]
	if !s.observerBudget(len(f) + 80) {
		return nil
	}
	o.at++
	o.spent += len(f)
	o.bytes -= len(f)
	s.observerMemory.Add(-int64(len(f)))
	if o.at == len(o.frames) {
		o.frames = o.pending
		o.bytes = o.pendingBytes
		o.pending = nil
		o.pendingBytes = 0
		o.at = 0
	}
	return f
}

type observerSubscription struct {
	delivered                      bool
	source, viewer                 *Conn
	watch, stream, term, epoch     uint32
	touched, started, rateAt       time.Time
	spent                          int
	sequence, last, tick, crc, raw uint32
	next, count                    uint16
	data                           []byte
	frames                         [][]byte
	memory                         int
}

func (s *Server) clearObserverAssembly(v *observerSubscription) {
	s.observerMemory.Add(-int64(v.memory))
	v.memory = 0
	v.data = nil
	v.frames = nil
	v.next = 0
	v.count = 0
}
func (s *Server) clearObserverConn(r *Room, c *Conn) {
	for i, v := range r.observer {
		if v != nil && (v.source == c || v.viewer == c) {
			s.clearObserverAssembly(v)
			v.viewer.observer.clear(s)
			r.observer[i] = nil
		}
	}
}
func (s *Server) handleObserverCapability(c *Conn, p []byte) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if len(p) != 4 || (binary.LittleEndian.Uint32(p) != observerRevision && binary.LittleEndian.Uint32(p) != observerLocalRevision) || c.room.Load() == nil || c.build < 94 {
		return
	}
	c.observerCap = true
	c.observerRevision = binary.LittleEndian.Uint32(p)
	out := make([]byte, 8)
	binary.LittleEndian.PutUint32(out, c.observerRevision)
	binary.LittleEndian.PutUint32(out[4:], 24*1024)
	s.enqueue(c, encodeFrame(0xF023, out))
}

// One per-node budget is consumed at write time, including conservative TCP/IP
// overhead. A large snapshot is paced across windows, never dropped because its
// total size exceeds one second's allowance. Gameplay retains the priority queue.
func (s *Server) observerBudget(n int) bool {
	s.observerBudgetMu.Lock()
	defer s.observerBudgetMu.Unlock()
	if s.observerBusy.Load() {
		return false
	}
	now := time.Now()
	if now.Sub(s.observerBudgetAt) >= time.Second {
		s.observerBudgetAt = now
		s.observerBudgetBytes = 0
	}
	limit := observerNodeBytesPerSecond
	if mbps := s.operations().Config.SendBudgetMbps; mbps > 0 {
		scaled := int(mbps * 1000000 / 8 * .05)
		if scaled < limit {
			limit = scaled
		}
	}
	if s.observerBudgetBytes+n > limit {
		return false
	}
	s.observerBudgetBytes += n
	return true
}
func observerExpired(v *observerSubscription, now time.Time) bool {
	if v.source.observerRevision == observerLocalRevision && v.viewer.observerRevision == observerLocalRevision {
		return false
	}
	return now.Sub(v.touched) > 4*time.Second
}

func observerNewer(a, b uint32) bool { return a != b && uint32(a-b) < 0x80000000 }
func (s *Server) observerLocked(r *Room, c, t *Conn, typ uint16, p []byte, flow bool) {
	now := time.Now()
	select {
	case <-c.done:
		return
	default:
	}
	select {
	case <-t.done:
		return
	default:
	}
	if !c.observerCap || !t.observerCap || c.build < 94 || t.build < 94 || r.bySeat[c.seat] != c || r.bySeat[t.seat] != t || c.left.Load() || t.left.Load() || c.replaced.Load() || t.replaced.Load() {
		return
	}
	term, epoch := uint32(0), uint32(1)
	if flow {
		term = r.flow.term
		epoch = r.flow.epoch
	}
	for i, v := range r.observer {
		if v != nil && (observerExpired(v, now) || v.term != term || v.epoch != epoch || r.bySeat[v.source.seat] != v.source || r.bySeat[v.viewer.seat] != v.viewer) {
			s.clearObserverAssembly(v)
			v.viewer.observer.clear(s)
			r.observer[i] = nil
		}
	}
	envelope := func(b []byte) []byte {
		if flow {
			return flowEnvelope(r, typ, b)
		}
		return encodeFrame(typ, b)
	}
	if typ == 30 {
		if len(p) != 30 || p[2] != 2 || p[3] < 1 || p[3] > 6 || p[4] != 0 || p[5] != 0 || (binary.LittleEndian.Uint32(p[18:]) != observerRevision && binary.LittleEndian.Uint32(p[18:]) != observerLocalRevision) || binary.LittleEndian.Uint32(p[22:]) != 24*1024 || binary.LittleEndian.Uint32(p[14:]) != epoch {
			return
		}
		modern := binary.LittleEndian.Uint32(p[18:]) == observerLocalRevision
		if modern && (c.observerRevision != observerLocalRevision || t.observerRevision != observerLocalRevision) {
			return
		}
		delay := binary.LittleEndian.Uint32(p[26:])
		if delay < 1000 || delay > 2000 {
			return
		}
		watch, stream := binary.LittleEndian.Uint32(p[6:]), binary.LittleEndian.Uint32(p[10:])
		if watch == 0 || stream == 0 {
			return
		}
		if p[3] == 1 { // Viewer -> source; a reconnect wait seat can watch, never produce.
			if r.roundWaitMask&(1<<t.seat) != 0 {
				return
			}
			if modern && !c.observerRequestAt.IsZero() && now.Sub(c.observerRequestAt) < 5*time.Second {
				reply := append([]byte(nil), p...)
				reply[0], reply[1], reply[3] = p[1], p[0], 6
				s.enqueue(c, envelope(reply))
				return
			}
			if modern {
				c.observerRequestAt = now
			}
			old := r.observer[c.seat]
			if old != nil {
				s.clearObserverAssembly(old)
				c.observer.clear(s)
			}
			r.observer[c.seat] = &observerSubscription{source: t, viewer: c, watch: watch, stream: stream, term: term, epoch: epoch, touched: now}
			s.enqueue(t, envelope(p))
			return
		}
		v := r.observer[c.seat]
		fromViewer := v != nil && v.source == t && v.viewer == c
		if !fromViewer {
			v = r.observer[t.seat]
		}
		if v == nil || v.watch != watch || (!fromViewer && (v.source != c || v.viewer != t)) {
			return
		}
		switch p[3] {
		case 2:
			if fromViewer {
				return
			}
			if v.stream != 1 && v.stream != stream {
				return
			}
			v.stream = stream
		case 3:
			if !fromViewer || v.stream != stream {
				return
			}
			v.touched = now
		case 4:
			if v.stream != stream {
				return
			}
			s.clearObserverAssembly(v)
			v.viewer.observer.clear(s)
			r.observer[v.viewer.seat] = nil
		case 5, 6:
			if fromViewer {
				return
			}
		default:
			return
		}
		s.enqueue(t, envelope(p))
		return
	}
	if len(p) < 41 || len(p) > 256 || p[2] != 2 || p[3] != 1 || r.roundWaitMask&(1<<c.seat) != 0 {
		return
	}
	v := r.observer[t.seat]
	if v == nil || v.source != c || v.viewer != t || binary.LittleEndian.Uint32(p[4:]) != v.watch || binary.LittleEndian.Uint32(p[8:]) != v.stream || binary.LittleEndian.Uint32(p[20:]) != epoch {
		return
	}
	seq, tick := binary.LittleEndian.Uint32(p[12:]), binary.LittleEndian.Uint32(p[16:])
	part, count := binary.LittleEndian.Uint16(p[24:]), binary.LittleEndian.Uint16(p[26:])
	raw, crc := binary.LittleEndian.Uint32(p[32:]), binary.LittleEndian.Uint32(p[36:])
	if v.delivered && c.observerRevision == observerLocalRevision {
		return
	}
	if seq == 0 || !observerNewer(seq, v.last) || count == 0 || count > 304 || part >= count || raw == 0 || raw > 1024*1024 || (c.observerRevision != observerLocalRevision && raw > 128*1024) || binary.LittleEndian.Uint32(p[28:]) != uint32(len(p)-40) {
		return
	}
	if now.Sub(v.started) > 3*time.Second {
		s.clearObserverAssembly(v)
	}
	if part == 0 {
		if v.count != 0 && !observerNewer(seq, v.sequence) {
			return
		}
		s.clearObserverAssembly(v)
		v.sequence = seq
		v.tick = tick
		v.count = count
		v.raw = raw
		v.crc = crc
		v.started = now
	}
	if v.count == 0 || part != v.next || count != v.count || seq != v.sequence || tick != v.tick || raw != v.raw || crc != v.crc {
		s.clearObserverAssembly(v)
		return
	}
	frame := envelope(p)
	cost := len(p) - 40 + len(frame)
	if len(v.data)+len(p)-40 > 64*1024 || s.observerMemory.Add(int64(cost)) > observerMemoryLimit {
		s.observerMemory.Add(-int64(cost))
		s.clearObserverAssembly(v)
		return
	}
	v.memory += cost
	v.data = append(v.data, p[40:]...)
	v.frames = append(v.frames, frame)
	v.next++
	if v.next != v.count {
		return
	}
	if len(v.data) < 5 || binary.LittleEndian.Uint32(v.data[1:]) != raw || crc32.ChecksumIEEE(v.data) != crc {
		s.clearObserverAssembly(v)
		return
	}
	if now.Sub(v.rateAt) >= time.Second {
		v.rateAt = now
		v.spent = 0
	}
	n := 0
	for _, f := range v.frames {
		n += len(f)
	}
	v.last = seq
	frames := v.frames
	s.clearObserverAssembly(v)
	if v.spent+n > 96*1024 || len(t.send) > 16 || s.observerBusy.Load() {
		return
	}
	v.spent += n
	if c.observerRevision == observerLocalRevision {
		// A single authenticated source upload serves all currently pending viewers.
		// Only viewer-specific routing/watch identifiers are rewritten; CRC'd content
		// is immutable and still sent separately to each recipient.
		for _, target := range r.observer {
			if target == nil || target.source != c || target.delivered || target.viewer.observerRevision != observerLocalRevision || target.stream != v.stream || target.term != term || target.epoch != epoch {
				continue
			}
			target.delivered = true
			target.last = seq
			out := make([][]byte, 0, len(frames))
			for _, original := range frames {
				f := append([]byte(nil), original...)
				at := 4
				if flow {
					at += 10
				}
				f[at+1] = target.viewer.seat
				binary.LittleEndian.PutUint32(f[at+4:], target.watch)
				out = append(out, f)
			}
			target.viewer.observer.offer(s, out)
			select {
			case <-target.viewer.done:
				target.viewer.observer.clear(s)
			default:
			}
		}
		return
	}
	t.observer.offer(s, frames)
	select {
	case <-t.done:
		t.observer.clear(s)
	default:
	}
}
