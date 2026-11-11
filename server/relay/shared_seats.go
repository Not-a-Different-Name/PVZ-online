package main

import (
	"bytes"
	"encoding/binary"
	"time"
)

const (
	sharedSeatBuild    = 112
	sharedSetRole      = 6
	sharedSource       = 7
	sharedFight        = 1
	sharedWatch        = 2
	msgSrvSharedResume = 0xF013
)

func (r *Room) combatOnlineMask() uint8 {
	mask := r.onlineMask()
	for _, c := range r.bySeat {
		if c != nil && c.seatObserver {
			mask &^= 1 << c.seat
		}
	}
	return mask
}

func (r *Room) combatOccupiedMask(now time.Time) uint8 {
	mask := r.combatOnlineMask()
	for seat := uint8(1); seat <= maxPlayers; seat++ {
		if r.holdActive(seat, now) && (r.sharedBuild == 0 || !r.flow.started && !r.holds[seat].seatObserver) {
			mask |= 1 << seat
		}
	}
	return mask
}

func (r *Room) sharedPreparationOpen() bool {
	return r.flow.phase == flowSetup && r.flow.prepPhase < 2 && !r.flow.prepCommitted && !r.flow.lost
}

func (s *Server) publishSharedRoster(r *Room) {
	if r.flow.term == 0 {
		r.flow.term = 1
	}
	r.updateWatchOrder()
	var body buf
	body.u8(2)
	body.u8(0) // recipient's real seat
	host := uint8(0)
	if r.host != nil {
		host = r.hostSeat
	}
	body.u8(host)
	body.u8(r.flow.phase)
	flags := uint8(0)
	for i, value := range []bool{r.public, r.started, r.flow.enabled, r.flow.lost, r.sharedPaused, r.sharedPreparationOpen()} {
		if value {
			flags |= 1 << i
		}
	}
	body.u8(flags)
	var sources uint8
	if !r.flow.lost && !r.sharedPaused && (r.flow.phase == flowSetup || r.flow.phase == flowBattle || r.flow.phase == flowVerify) {
		for _, c := range r.bySeat {
			if c != nil && c.watchSource && !c.seatObserver && !c.left.Load() && !c.replaced.Load() {
				sources |= 1 << c.seat
			}
		}
		sources &^= r.flow.done | r.roundWaitMask
	}
	body.u8(sources)
	body.u32(r.flow.term)
	body.u32(r.flow.epoch)
	body.raw([]byte(r.code))
	body.u8(0)
	var seen uint8
	appendMember := func(seat uint8) {
		c := r.bySeat[seat]
		h := r.holds[seat]
		if c == nil && !r.holdActive(seat, time.Now()) {
			return
		}
		build, identity, name := h.build, h.watchIdentity, h.name
		role := uint8(1)
		if c != nil {
			if c.watchIdentity == 0 {
				r.watchIdentity++
				if r.watchIdentity == 0 {
					r.watchIdentity++
				}
				c.watchIdentity = r.watchIdentity
			}
			build, identity, name = c.build, c.watchIdentity, c.name
			role = c.sharedRole << 2
			if c.seatObserver {
				role |= 2
			}
		} else if h.seatObserver {
			role |= 2
		}
		body.u8(seat)
		body.u16(build)
		body.u32(identity)
		body.u8(role)
		body.raw(name[:])
		body.b[18]++
		seen |= 1 << seat
	}
	for _, seat := range r.watchOrder {
		if r.roundWaitMask&(1<<seat) == 0 {
			appendMember(seat)
		}
	}
	for _, seat := range r.watchOrder {
		if seen&(1<<seat) == 0 {
			appendMember(seat)
		}
	}
	for seat := uint8(1); seat <= maxPlayers; seat++ {
		if seen&(1<<seat) == 0 {
			appendMember(seat)
		}
	}
	for _, c := range r.bySeat {
		if c == nil || c.left.Load() || c.replaced.Load() {
			continue
		}
		body.b[1] = c.seat
		if !bytes.Equal(c.watchRoster, body.b) {
			c.watchRoster = append(c.watchRoster[:0], body.b...)
			s.enqueue(c, encodeFrame(msgSrvWatchRoster, body.b))
		}
	}
}

func (s *Server) joinSharedRoom(r *Room, c *Conn, build uint16, name []byte, directory bool) {
	if build != r.sharedBuild {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectBuildMismatch)))
		return
	}
	if directory && !r.public {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectJoinClosed)))
		return
	}
	seat := r.lowestFreeSeat(time.Now())
	if seat == 0 {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRoomFull)))
		return
	}
	c.seat, c.build = seat, build
	copy(c.name[:], name)
	c.seatObserver = r.started || r.sharedPaused || !r.joinOpen
	c.room.Store(r)
	r.bySeat[seat] = c
	s.welcomeShared(r, c)
	s.issueCredential(c)
	if !c.seatObserver {
		r.broadcast(s, c, encodeFrame(msgSrvPeerJoin, encodePeerJoin(rosterEntry{seat: seat, build: build, name: c.name})))
	}
	s.publishFlow(r)
}

func (s *Server) welcomeShared(r *Room, c *Conn) {
	roster := r.roster()
	if c.seatObserver {
		roster = append(roster, rosterEntry{seat: c.seat, build: c.build, name: c.name})
	}
	s.enqueue(c, encodeFrame(msgSrvWelcome, encodeWelcome(c.seat, r.hostSeat, r.code, roster)))
}

func (s *Server) handleSharedRole(r *Room, c *Conn, p []byte) {
	if r.bySeat[c.seat] != c || c.left.Load() || c.replaced.Load() ||
		binary.LittleEndian.Uint32(p[2:]) != r.flow.term {
		return
	}
	epoch := binary.LittleEndian.Uint32(p[6:])
	switch p[0] {
	case sharedSource:
		if epoch == r.flow.epoch && !c.seatObserver && p[1] <= 1 && c.watchSource != (p[1] == 1) {
			c.watchSource = p[1] == 1
			s.publishSharedRoster(r)
		}
	case sharedSetRole:
		request := binary.LittleEndian.Uint32(p[10:])
		if epoch > r.flow.epoch || r.flow.epoch-epoch > 1 || p[1] > sharedWatch || request <= c.sharedRequest || (p[1] == sharedWatch && r.host == c) {
			return
		}
		c.sharedRequest = request
		c.sharedRole = p[1]
		if c.sharedRole == sharedFight && !c.seatObserver || c.sharedRole == sharedWatch && c.seatObserver {
			c.sharedRole = 0
		}
		if r.sharedPaused || r.flow.phase == flowLobby || r.flow.phase == flowIntermission || r.sharedPreparationOpen() {
			s.applySharedRoles(r)
		}
		s.publishFlow(r)
	}
}

func (s *Server) applySharedRoles(r *Room) {
	for _, c := range r.bySeat {
		if c == nil || c.sharedRole == 0 || c.left.Load() || c.replaced.Load() {
			continue
		}
		s.applySharedRole(r, c, c.sharedRole == sharedWatch)
	}
	r.updateWatchOrder()
	if r.host == nil && s.electFlowHost(r) {
		s.resumeSharedRoom(r)
	}
}

func (s *Server) applySharedRole(r *Room, c *Conn, observer bool) {
	c.sharedRole = 0
	if c == r.host && observer || c.seatObserver == observer {
		return
	}
	c.seatObserver, c.watchSource = observer, false
	c.watchJoinRound, c.watchDeadline = 0, time.Time{}
	s.clearObserverConn(r, c)
	s.punchDrop(r, c)
	bit := uint8(1 << c.seat)
	r.roundWaitMask &^= bit
	r.flow.done &^= bit
	r.flow.ack &^= bit
	r.flow.applied &^= bit
	if observer {
		r.broadcast(s, c, encodeFrame(msgSrvPeerLeave, encodePeerLeave(c.seat, peerLeaveQuit)))
		return
	}
	if r.flow.phase == flowIntermission {
		r.roundWaitMask |= bit
		c.watchJoinRound = r.flow.epoch + 1
	} else if r.flow.lost {
		c.watchJoinRound = r.flow.epoch + 1
	}
	s.welcomeShared(r, c)
	r.broadcast(s, c, encodeFrame(msgSrvPeerJoin, encodePeerJoin(rosterEntry{seat: c.seat, build: c.build, name: c.name})))
	if r.sharedPreparationOpen() && len(r.sharedStart) != 0 {
		c.watchJoinRound = r.flow.epoch
		c.watchDeadline = time.Now().Add(120 * time.Second)
		s.sendSharedStart(r, c)
	}
}

func (s *Server) sendSharedStart(r *Room, c *Conn) {
	p := append([]byte(nil), r.sharedStart...)
	p[0], p[1] = r.hostSeat, c.seat
	if len(r.sharedEndless) != 0 {
		context := append([]byte(nil), r.sharedEndless...)
		context[0], context[1] = r.hostSeat, c.seat
		s.enqueue(c, flowEnvelope(r, 26, context))
	}
	s.enqueue(c, flowEnvelope(r, 3, p))
}

func (s *Server) resumeSharedRoom(r *Room) {
	if !r.sharedPaused || r.host == nil {
		return
	}
	r.sharedPaused = false
	r.flow.recoveryUntil = time.Time{}
	if !r.started || len(r.sharedStart) == 0 {
		return
	}
	// A live lawn with no remaining owner cannot continue. Re-enter its last
	// accepted START through the normal preparation and checkpoint path.
	r.flow.epoch++
	r.flow.phase, r.flow.lost = flowSetup, false
	r.flow.prepPhase, r.flow.done, r.flow.applied = 0, 0, 0
	r.roundWaitMask = 0
	s.resetFlowBarrier(r)
	binary.LittleEndian.PutUint32(r.sharedStart[len(r.sharedStart)-5:], r.flow.epoch)
	payload := append([]byte(nil), r.sharedStart...)
	if len(r.sharedEndless) != 0 {
		binary.LittleEndian.PutUint32(r.sharedEndless[3:], r.flow.epoch)
		r.sharedEndless[0], r.sharedEndless[1] = r.hostSeat, r.hostSeat
		payload = append(payload, r.sharedEndless...)
	}
	s.publishFlow(r)
	s.enqueue(r.host, encodeFrame(msgSrvSharedResume, payload))
	for _, c := range r.bySeat {
		if c != nil && c != r.host && !c.seatObserver {
			s.sendSharedStart(r, c)
		}
	}
}

func (s *Server) rejoinSharedRoom(r *Room, c, old *Conn, seat uint8, name []byte, build uint16, now time.Time) {
	h := r.holds[seat]
	if old != nil {
		h = seatHold{credential: old.credential, name: old.name, build: old.build, flow: old.flow,
			watchIdentity: old.watchIdentity, seatObserver: old.seatObserver, until: now.Add(s.holdDuration)}
	}
	if h.build != build || build != r.sharedBuild || !now.Before(h.until) || !bytes.Equal(h.name[:], name) || !sameCredential(h.credential, c.credential) {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRejoinSeatMismatch)))
		return
	}
	returningHost := old != nil && old == r.host || r.host == nil && seat == r.hostSeat
	if old != nil {
		s.clearObserverConn(r, old)
		s.punchDrop(r, old)
		old.replaced.Store(true)
		old.room.Store(nil)
		old.close()
	}
	r.holds[seat] = seatHold{}
	c.seat, c.build, c.name, c.flow, c.watchIdentity = seat, build, h.name, h.flow, h.watchIdentity
	c.seatObserver = !returningHost && (h.seatObserver || r.started)
	c.room.Store(r)
	r.bySeat[seat] = c
	if returningHost {
		r.host = c
	}
	bit := uint8(1 << seat)
	r.roundWaitMask &^= bit
	r.flow.done &^= bit
	r.flow.ack &^= bit
	r.flow.applied &^= bit
	s.resetFlowBarrier(r)
	s.welcomeShared(r, c)
	s.issueCredential(c)
	if c.seatObserver && !h.seatObserver {
		r.broadcast(s, c, encodeFrame(msgSrvPeerLeave, encodePeerLeave(seat, peerLeaveQuit)))
	} else if !c.seatObserver {
		r.broadcast(s, c, encodeFrame(msgSrvPeerBack, encodePeerBack(rosterEntry{seat: seat, build: c.build, name: c.name})))
	}
	s.checkFlowBarrier(r)
	s.publishFlow(r)
	if returningHost {
		s.resumeSharedRoom(r)
	}
}

func (s *Server) settleSharedAdmissions(r *Room, now time.Time) {
	changed := false
	for _, c := range r.bySeat {
		if c == nil || c.watchDeadline.IsZero() {
			continue
		}
		if c.watchJoinRound != r.flow.epoch || r.flow.phase != flowSetup || r.flow.prepPhase == 5 {
			c.watchDeadline = time.Time{}
			continue
		}
		if c != r.host && !now.Before(c.watchDeadline) {
			c.watchDeadline = time.Time{}
			c.sharedRole = sharedWatch
			changed = true
		}
	}
	if changed {
		s.applySharedRoles(r)
		s.publishFlow(r)
	}
}

func (s *Server) removeSharedMember(r *Room, c *Conn, reason uint8) {
	seat := c.seat
	if r.bySeat[seat] != c {
		return
	}
	r.bySeat[seat] = nil
	if !c.left.Load() {
		r.holds[seat] = seatHold{flow: c.flow, credential: c.credential, name: c.name, build: c.build,
			until: time.Now().Add(s.holdDuration), watchIdentity: c.watchIdentity, seatObserver: c.seatObserver}
	}
	bit := uint8(1 << seat)
	r.roundWaitMask &^= bit
	r.flow.done &^= bit
	r.flow.ack &^= bit
	r.flow.applied &^= bit
	if r.host == c {
		r.host = nil
		s.electFlowHost(r)
		r.sharedPaused = r.host == nil
	}
	if !c.seatObserver {
		r.broadcast(s, nil, encodeFrame(msgSrvPeerLeave, encodePeerLeave(seat, reason)))
	}
	if r.empty() {
		delete(s.rooms, r.code)
		return
	}
	s.resetFlowBarrier(r)
	s.checkFlowBarrier(r)
	s.publishFlow(r)
}
