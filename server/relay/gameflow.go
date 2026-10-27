package main

// Opt-in flow v1. Old rooms keep their original behavior; a running flow room
// never accepts unversioned gameplay. Every envelope binds the host term and
// round, including snapshots, so a delayed old host/round cannot advance play.
import (
	"encoding/binary"
	"time"
)

const (
	msgCliFlow         = 0xF01F
	msgCliFlowGame     = 0xF020
	msgCliFlowProgress = 0xF021
	msgCliFlowStart    = 0xF022
	msgSrvFlow         = 0xF010
	msgSrvFlowGame     = 0xF011
	flowLobby          = 0
	flowSetup          = 1
	flowBattle         = 2
	flowVerify         = 3
	flowComplete       = 4
	flowIntermission   = 5
)

type roundFlow struct {
	enabled, started          bool
	term, epoch, generation   uint32
	phase, done, ack, applied uint8
	prepRevision, prepRequest uint32
	prepPhase, prepA, prepB   uint8
	prepCommitted             bool
	lost                      bool
	// Fixed when the last current combatant disappears; waiting reconnects do
	// not extend this window or gain permission to skip/restart the round.
	recoveryUntil time.Time
}

func (r *Room) updateFlowRecovery(now time.Time, grace time.Duration) {
	if !r.flow.enabled || !r.flow.started || (r.flow.phase != flowBattle && r.flow.phase != flowVerify) || r.flowMask() != 0 {
		r.flow.recoveryUntil = time.Time{}
		return
	}
	if !r.flow.recoveryUntil.IsZero() {
		return
	}
	r.flow.recoveryUntil = now
	for seat := 1; seat <= maxPlayers; seat++ {
		if until := r.holds[seat].until; until.After(r.flow.recoveryUntil) {
			r.flow.recoveryUntil = until
		}
	}
	if !r.flow.recoveryUntil.After(now) {
		// A live-link replacement can exhaust combat without creating a hold.
		// It still gets one grace window, fixed here and never extended.
		r.flow.recoveryUntil = now.Add(grace)
	}
}

func (r *Room) onlineMask() uint8 {
	var m uint8
	for seat := 1; seat <= maxPlayers; seat++ {
		if c := r.bySeat[seat]; c != nil && !c.left.Load() && !c.replaced.Load() {
			m |= 1 << seat
		}
	}
	return m
}
func (r *Room) flowMask() uint8 { return r.onlineMask() &^ r.roundWaitMask }
func (s *Server) publishFlow(r *Room) {
	if r.flow.term == 0 {
		r.flow.term = 1
	}
	occupied := r.onlineMask()
	for seat := 1; seat <= maxPlayers; seat++ {
		if r.holdActive(uint8(seat), time.Now()) {
			occupied |= 1 << seat
		}
	}
	enabled := true
	for seat := 1; seat <= maxPlayers; seat++ {
		if c := r.bySeat[seat]; c != nil && !c.flow {
			enabled = false
		}
	}
	if !r.flow.started && !r.started {
		r.flow.enabled = enabled
	}
	p := make([]byte, 24)
	binary.LittleEndian.PutUint16(p, 1)
	if r.flow.enabled {
		p[2] = 1
	}
	if r.flow.lost {
		p[2] |= 2
	}
	p[3], p[4], p[5], p[6], p[7] = r.flow.phase, r.hostSeat, r.onlineMask(), occupied, r.roundWaitMask
	binary.LittleEndian.PutUint32(p[8:], r.flow.term)
	binary.LittleEndian.PutUint32(p[12:], r.flow.epoch)
	binary.LittleEndian.PutUint32(p[16:], r.flow.generation)
	p[20], p[21], p[22] = r.flow.done, r.flow.ack, r.flow.applied
	for seat := 1; seat <= maxPlayers; seat++ {
		if c := r.bySeat[seat]; c != nil && c.flow {
			s.enqueue(c, encodeFrame(msgSrvFlow, p))
		}
	}
}
func (s *Server) handleFlow(c *Conn, p []byte) {
	if len(p) != 2 || binary.LittleEndian.Uint16(p) != 1 {
		s.dropBadControl(c, msgCliFlow)
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil {
		return
	}
	c.flow = true
	s.publishFlow(r)
}
func flowEnvelope(r *Room, typ uint16, p []byte) []byte {
	out := make([]byte, 10+len(p))
	binary.LittleEndian.PutUint32(out, r.flow.term)
	binary.LittleEndian.PutUint32(out[4:], r.flow.epoch)
	binary.LittleEndian.PutUint16(out[8:], typ)
	copy(out[10:], p)
	return encodeFrame(msgSrvFlowGame, out)
}
func (s *Server) resetFlowBarrier(r *Room) {
	r.flow.generation++
	if r.flow.generation == 0 {
		r.flow.generation = 1
	}
	r.flow.ack = 0
	if r.flow.phase == flowVerify {
		r.flow.phase = flowBattle
	}
}
func (s *Server) checkFlowBarrier(r *Room) {
	r.updateFlowRecovery(time.Now(), s.holdDuration)
	mask := r.flowMask()
	if mask == 0 || r.flow.lost {
		return
	}
	if r.flow.phase == flowBattle && r.flow.done&mask == mask {
		s.resetFlowBarrier(r)
		r.flow.phase = flowVerify
	}
	if r.flow.phase == flowVerify && r.flow.ack&mask == mask {
		r.flow.phase = flowComplete
		r.flow.applied = 0
	}
	if r.flow.phase == flowComplete && r.flow.applied&mask == mask {
		r.flow.phase = flowIntermission
	}
}
func (s *Server) handleFlowProgress(c *Conn, p []byte) {
	if len(p) != 14 {
		s.dropBadControl(c, msgCliFlowProgress)
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil || !c.flow || !r.flow.started || binary.LittleEndian.Uint32(p) != r.flow.term || binary.LittleEndian.Uint32(p[4:]) != r.flow.epoch {
		return
	}
	// A return-to-menu abort is independent of combat participation. In
	// particular a sole remaining/waiting seat must be able to recover a room.
	if p[8] == 5 && p[9] == 1 {
		s.returnFlowToMenu(r, c)
		return
	}
	if r.flowMask()&(1<<c.seat) == 0 {
		return
	}
	bit := uint8(1 << c.seat)
	op, value := p[8], p[9]
	if value > 1 {
		return
	}
	switch op {
	case 1: // Local board rechecked clear/busy. SETUP ends on actual board play.
		if r.flow.phase == flowSetup {
			r.flow.phase = flowBattle
		}
		if r.flow.phase != flowBattle && r.flow.phase != flowVerify {
			return
		}
		if value == 1 {
			r.flow.done |= bit
		} else {
			r.flow.done &^= bit
			s.resetFlowBarrier(r)
		}
	case 2: // No completion until every active board revalidates this generation.
		if r.flow.phase != flowVerify || value != 1 || binary.LittleEndian.Uint32(p[10:]) != r.flow.generation {
			return
		}
		r.flow.ack |= bit
	case 3: // Each app consumed completion before the host may start the next round.
		if r.flow.phase != flowComplete || value != 1 || binary.LittleEndian.Uint32(p[10:]) != r.flow.generation {
			return
		}
		r.flow.applied |= bit
	case 4: // A losing lawn also reports directly when it is the sole survivor.
		if value != 1 || r.flow.phase >= flowComplete {
			return
		}
		r.flow.lost = true
	default:
		return
	}
	s.checkFlowBarrier(r)
	s.publishFlow(r)
}
func (s *Server) handleFlowStart(c *Conn, p []byte) {
	if len(p) != 53 && len(p) != 54 && len(p) != 61 {
		s.dropBadControl(c, msgCliFlowStart)
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil || !c.flow || !r.flow.enabled || r.host != c || binary.LittleEndian.Uint32(p) != r.flow.term {
		return
	}
	for seat := 1; seat <= maxPlayers; seat++ {
		if member := r.bySeat[seat]; member != nil && !member.flow {
			return
		}
	}
	epoch := binary.LittleEndian.Uint32(p[4:])
	snapshot := append([]byte(nil), p[8:]...)
	if epoch == 0 || snapshot[0] != c.seat || binary.LittleEndian.Uint32(snapshot[len(snapshot)-5:len(snapshot)-1]) != epoch {
		return
	}
	// Nine-, ten- and seventeen-stack snapshots retain epoch + crowd rule at the tail.
	if !r.flow.started {
		if epoch != 1 || r.connectedPlayers(true) < 2 {
			return
		}
	} else if epoch != r.flow.epoch+1 || (r.flow.phase != flowLobby && r.flow.phase != flowIntermission && !r.flow.lost) || (r.flow.phase != flowLobby && !r.flow.lost && r.flow.applied&r.flowMask() != r.flowMask()) || (r.flow.phase == flowLobby && r.connectedPlayers(true) < 2) {
		return
	}
	r.flow.lost = false
	r.flow.recoveryUntil = time.Time{}
	r.flow.prepRevision = 0
	r.flow.prepRequest = 0
	r.flow.prepPhase = 0
	r.flow.prepCommitted = false
	r.flow.started = true
	r.flow.epoch = epoch
	r.flow.phase = flowSetup
	r.flow.done = 0
	r.flow.ack = 0
	r.flow.applied = 0
	r.roundWaitMask = 0
	r.started = true
	s.resetFlowBarrier(r)
	s.publishFlow(r)
	for seat := 1; seat <= maxPlayers; seat++ {
		if t := r.bySeat[seat]; t != nil && t != c && t.flow {
			snapshot[1] = uint8(seat)
			if s.enqueue(t, flowEnvelope(r, 3, snapshot)) {
				s.stats.forwarded.Add(1)
			}
		}
	}
}
func (s *Server) handleFlowGame(c *Conn, p []byte) {
	if len(p) < 12 || len(p) > 266 {
		s.dropBadControl(c, msgCliFlowGame)
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil || !c.flow || !r.flow.enabled || binary.LittleEndian.Uint32(p) != r.flow.term || binary.LittleEndian.Uint32(p[4:]) != r.flow.epoch {
		return
	}
	typ := binary.LittleEndian.Uint16(p[8:])
	body := p[10:]
	if typ >= srvFrameBase || len(body) > 256 || body[0] != c.seat || body[1] < 1 || body[1] > maxPlayers || body[1] == c.seat {
		return
	}
	dst := body[1]
	// Compatibility: an existing MOD90 LEVEL_EXIT must also reset the relay
	// barrier, not merely move the clients to menus while leaving phase=BATTLE.
	if typ == 13 && len(body) == 3 && body[2] == 0 && r.flow.started {
		s.returnFlowToMenu(r, c)
		return
	}
	// Legacy lobby seat toggles cannot arrive after a round started. Current
	// preparation transactions use type25 and revision-bound consent instead.
	if r.flow.phase != flowLobby && (typ == 10 || typ == 11) {
		return
	}
	if typ == 29 && (len(body) != 4 || body[2] != 1 || body[3] > 3) {
		return
	}
	t := r.bySeat[dst]
	if t == nil || !t.flow {
		return
	}
	if typ == 30 || typ == 31 {
		s.observerLocked(r, c, t, typ, body, true)
		return
	}
	if typ == 3 { // Same-round targeted replay only; new rounds use FlowStart.
		if c != r.host || (len(body) != 45 && len(body) != 46 && len(body) != 53) || binary.LittleEndian.Uint32(body[len(body)-5:]) != r.flow.epoch {
			return
		}
	}
	if (typ == 14 || typ == 20 || typ == 22 || typ == 27 || (typ == 25 && len(body) == 92 && body[2] == 6)) && c != r.host {
		return
	}
	if r.roundWaitMask&(1<<c.seat) != 0 && typ != 16 && typ != 17 && typ != 25 && typ != 23 && typ != 24 && typ != 26 && typ != 28 && typ != 29 {
		return
	}
	// Late combat after the distributed clear barrier is never delivered.
	if (typ == 4 || typ == 5 || typ == 6 || typ == 12 || typ == 13 || typ == 19) && r.flow.phase >= flowComplete {
		return
	}
	if typ == 6 {
		r.flow.lost = true
	}
	if typ == 25 && len(body) == 92 && body[2] == 6 && c == r.host {
		rev, request := binary.LittleEndian.Uint32(body[7:]), binary.LittleEndian.Uint32(body[11:])
		if binary.LittleEndian.Uint32(body[3:]) != r.flow.epoch || rev < r.flow.prepRevision {
			return
		}
		if request != r.flow.prepRequest {
			r.flow.prepCommitted = false
		}
		r.flow.prepRevision = rev
		r.flow.prepRequest = request
		r.flow.prepPhase = body[39]
		r.flow.prepA = body[42]
		r.flow.prepB = body[43]
	}
	if typ == 5 { // Incoming escaped zombie invalidates recipient's previous clear.
		if r.roundWaitMask&(1<<dst) != 0 {
			return
		}
		r.flow.done &^= 1 << dst
		s.resetFlowBarrier(r)
	}
	if s.enqueue(t, flowEnvelope(r, typ, body)) {
		s.stats.forwarded.Add(1)
	}
	if typ == 5 || typ == 6 {
		s.publishFlow(r)
	}
}

// Cancelling a board is not a clear, victory, or loss. Keep epoch monotonic,
// reopen the room lobby and invalidate all previous battle/preparation state.
func (s *Server) returnFlowToMenu(r *Room, source *Conn) {
	if r.flow.phase == flowLobby {
		return
	}
	r.flow.phase = flowLobby
	r.started = false
	r.flow.lost = false
	r.flow.done, r.flow.ack, r.flow.applied = 0, 0, 0
	r.flow.recoveryUntil = time.Time{}
	r.flow.prepRevision, r.flow.prepRequest = 0, 0
	r.flow.prepPhase, r.flow.prepA, r.flow.prepB = 0, 0, 0
	r.flow.prepCommitted = false
	r.roundWaitMask = 0
	r.joinOpen = true
	for seat := 1; seat <= maxPlayers; seat++ {
		if t := r.bySeat[seat]; t != nil && t != source && t.flow {
			s.enqueue(t, flowEnvelope(r, 13, []byte{source.seat, uint8(seat), 0}))
		}
	}
	s.publishFlow(r)
}
func (s *Server) electFlowHost(r *Room) bool {
	for _, mask := range []uint8{r.flowMask(), r.onlineMask()} {
		for seat := 1; seat <= maxPlayers; seat++ {
			if mask&(1<<seat) != 0 {
				r.gateRequest = 0
				r.gateEpoch = 0
				r.flow.prepRevision = 0
				r.flow.prepRequest = 0
				r.flow.prepPhase = 0
				r.flow.prepCommitted = false
				r.host = r.bySeat[seat]
				r.hostSeat = uint8(seat)
				r.flow.term++
				s.resetFlowBarrier(r)
				s.checkFlowBarrier(r)
				return true
			}
		}
	}
	return false
}
