package main

import (
	"bytes"
	"encoding/binary"
	"math/bits"
	"strings"
	"time"
)

const (
	watchRoomBuild    = 111
	watchRoomLimit    = 4
	watchRouteMax     = maxPlayers + watchRoomLimit
	msgCliWatchJoin   = 0xF02A
	msgCliWatchRole   = 0xF02B
	msgSrvWatchRoster = 0xF012

	watchApply   = 1
	watchCancel  = 2
	watchApprove = 3
	watchReject  = 4
	watchDemote  = 5
)

type roomViewer struct {
	conn                 *Conn
	reserved             *Conn
	request, term, round uint32
	clientRequest        uint32
	approved             bool
}

func (c *Conn) watchRoute() uint8 {
	if c.watchSlot != 0 {
		return c.watchSlot
	}
	return c.seat
}

func (r *Room) watchPeer(route uint8) *Conn {
	if route >= 1 && route <= maxPlayers {
		return r.bySeat[route]
	}
	if route <= maxPlayers || route > watchRouteMax {
		return nil
	}
	return r.viewers[route-maxPlayers-1].conn
}

func (r *Room) watchingRoom() bool {
	if r.host == nil || r.host.build < watchRoomBuild {
		return false
	}
	for _, c := range r.bySeat {
		if c != nil && c.build != r.host.build {
			return false
		}
	}
	return true
}

func (r *Room) watchers() uint8 {
	var n uint8
	for _, viewer := range r.viewers {
		if viewer.conn != nil {
			n++
		}
	}
	return n
}

func (r *Room) viewerSlotsUsed() uint8 {
	var n uint8
	now := time.Now()
	for i, v := range r.viewers {
		if v.conn != nil || v.reserved != nil || now.Before(r.viewerHolds[i].until) {
			n++
		}
	}
	return n
}

func (r *Room) freeViewer() uint8 {
	now := time.Now()
	for i := range r.viewers {
		if r.viewers[i].conn == nil && r.viewers[i].reserved == nil && !now.Before(r.viewerHolds[i].until) {
			return uint8(i + maxPlayers + 1)
		}
	}
	return 0
}

func (r *Room) nextWatchRound() uint32 {
	if !r.flow.started {
		return 1
	}
	if r.flow.epoch == ^uint32(0) {
		return 0
	}
	return r.flow.epoch + 1
}

func (r *Room) updateWatchOrder() {
	online := r.onlineMask()
	kept := r.watchOrder[:0]
	var included uint8
	for _, seat := range r.watchOrder {
		if online&(1<<seat) != 0 {
			kept = append(kept, seat)
			included |= 1 << seat
		}
	}
	for seat := uint8(1); seat <= maxPlayers; seat++ {
		if online&(1<<seat) != 0 && included&(1<<seat) == 0 {
			kept = append(kept, seat)
		}
	}
	r.watchOrder = kept
}

func (r *Room) nextCombatSeat(from uint8) uint8 {
	r.updateWatchOrder()
	found := false
	for _, seat := range r.watchOrder {
		if found && r.roundWaitMask&(1<<seat) == 0 {
			return seat
		}
		if seat == from {
			found = true
		}
	}
	return 0
}

// Role snapshots use an independent namespace. Combat packets still accept 1–6.
func (s *Server) publishWatchRoster(r *Room) {
	if !r.watchingRoom() && r.watchers() == 0 {
		return
	}
	var body buf
	body.u8(1)
	body.u8(0) // Per-connection observer slot. Zero means active.
	body.u8(r.hostSeat)
	body.u8(r.flow.phase)
	var flags uint8
	if r.public {
		flags |= 1
	}
	if r.started {
		flags |= 2
	}
	if r.gateManaged && !r.joinOpen {
		flags |= 4
	}
	flags |= r.viewerSlotsUsed() << 3
	if r.flow.enabled && r.flow.started {
		flags |= 64
	}
	body.u8(flags)
	body.u8(r.onlineMask())
	body.u8(r.roundWaitMask & r.onlineMask())
	body.u32(r.flow.term)
	body.u32(r.flow.epoch)
	body.raw([]byte(r.code))
	body.u8(uint8(bits.OnesCount8(r.onlineMask())))
	r.updateWatchOrder()
	for _, seat := range r.watchOrder {
		c := r.bySeat[seat]
		body.u8(seat)
		body.u16(c.build)
		if c.watchIdentity == 0 {
			r.watchIdentity++
			if r.watchIdentity == 0 {
				r.watchIdentity++
			}
			c.watchIdentity = r.watchIdentity
		}
		body.u32(c.watchIdentity)
		body.raw(c.name[:])
	}
	body.u8(r.watchers())
	for _, viewer := range r.viewers {
		if viewer.conn == nil {
			continue
		}
		body.u8(viewer.conn.watchSlot)
		state := uint8(0)
		if viewer.term == r.flow.term && viewer.round == r.nextWatchRound() && viewer.request != 0 {
			state = 1
			if viewer.approved {
				state = 2
			}
		}
		body.u8(state)
		body.u32(viewer.request)
		body.raw(viewer.conn.name[:])
	}
	for route := uint8(1); route <= watchRouteMax; route++ {
		c := r.watchPeer(route)
		if c == nil || c.build < watchRoomBuild || c.left.Load() || c.replaced.Load() {
			continue
		}
		body.b[1] = c.watchSlot
		if bytes.Equal(c.watchRoster, body.b) {
			continue
		}
		c.watchRoster = append(c.watchRoster[:0], body.b...)
		s.enqueue(c, encodeFrame(msgSrvWatchRoster, body.b))
	}
}

func (s *Server) addRoomViewer(r *Room, c *Conn, slot uint8) {
	c.seat = 0
	c.watchSlot = slot
	c.flow = true
	c.watchRoster = nil
	c.room.Store(r)
	r.viewers[slot-maxPlayers-1] = roomViewer{conn: c}
	r.viewerHolds[slot-maxPlayers-1] = seatHold{}
	s.issueCredential(c)
	s.publishWatchRoster(r)
}

// Code joins may watch private rooms. Directory joins must be explicitly public.
// An optional credential resumes an existing viewer or an authenticated active hold.
func (s *Server) handleWatchJoin(c *Conn, p []byte) {
	const joinBytes = 2 + 2 + nameSize + roomCodeLen
	if (len(p) != joinBytes+1 && len(p) != joinBytes+17) || p[joinBytes] > 1 {
		s.dropBadControl(c, msgCliWatchJoin)
		return
	}
	version, build, name, codeBytes, ok := decodeJoinRoom(p[:joinBytes])
	if !ok {
		s.dropBadControl(c, msgCliWatchJoin)
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if c.room.Load() != nil {
		return
	}
	reject := func(reason uint8) { s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(reason))) }
	if version != protocolVersion {
		reject(rejectProtocolVersion)
		return
	}
	r := s.rooms[strings.ToUpper(string(codeBytes))]
	if r == nil {
		reject(rejectRoomNotFound)
		return
	}
	if build < watchRoomBuild || !r.watchingRoom() || build != r.host.build {
		reject(rejectBuildMismatch)
		return
	}
	if p[joinBytes] == 1 && !r.public {
		reject(rejectJoinClosed)
		return
	}
	c.build = build
	copy(c.name[:], name)
	if len(p) == joinBytes+17 {
		copy(c.credential[:], p[joinBytes+1:])
		for _, v := range r.viewers {
			if v.conn != nil && sameCredential(v.conn.credential, c.credential) && v.conn.build == build {
				old := v.conn
				s.clearObserverConn(r, old)
				old.replaced.Store(true)
				old.room.Store(nil)
				old.close()
				copy(c.name[:], old.name[:])
				s.addRoomViewer(r, c, old.watchSlot)
				return
			}
		}
		for i, h := range r.viewerHolds {
			if time.Now().Before(h.until) && h.build == build && sameCredential(h.credential, c.credential) {
				c.name = h.name
				s.addRoomViewer(r, c, uint8(i+maxPlayers+1))
				return
			}
		}
		reject(rejectRejoinUnavailable)
		return
	}
	slot := r.freeViewer()
	if slot == 0 {
		reject(rejectRoomFull)
		return
	}
	s.addRoomViewer(r, c, slot)
}

// All writes are serialized by Server.mu. A request is tied to an identity,
// authority term and target round, even when a viewer slot is immediately reused.
func (s *Server) handleWatchRole(c *Conn, p []byte) {
	if len(p) != 14 {
		s.dropBadControl(c, msgCliWatchRole)
		return
	}
	op, slot := p[0], p[1]
	term, round, request := binary.LittleEndian.Uint32(p[2:]), binary.LittleEndian.Uint32(p[6:]), binary.LittleEndian.Uint32(p[10:])
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil || c.build < watchRoomBuild || !r.watchingRoom() || c.left.Load() || c.replaced.Load() ||
		r.watchPeer(c.watchRoute()) != c || term != r.flow.term || round != r.nextWatchRound() || request == 0 {
		return
	}
	if op == watchDemote {
		openPreparation := r.flow.phase == flowSetup && r.flow.prepPhase < 2 && !r.flow.lost
		if c.seat == 0 || (r.flow.phase != flowIntermission && r.flow.phase != flowLobby && !openPreparation) {
			return
		}
		// Keep an authority and at least one live battle participant.
		if bits.OnesCount8(r.flowMask()) < 2 {
			return
		}
		slot = r.freeViewer()
		if slot == 0 {
			return
		}
		s.detachCombatForWatch(r, c)
		s.addRoomViewer(r, c, slot)
		s.publishFlow(r)
		return
	}
	if slot <= maxPlayers || slot > watchRouteMax {
		return
	}
	v := &r.viewers[slot-maxPlayers-1]
	if v.conn == nil {
		return
	}
	switch op {
	case watchApply:
		if v.conn != c || request <= v.clientRequest {
			return
		}
		r.viewerRequestID++
		if r.viewerRequestID == 0 {
			r.viewerRequestID++
		}
		v.clientRequest = request
		v.request, v.term, v.round, v.approved = r.viewerRequestID, term, round, false
	case watchCancel:
		if v.conn != c || request != v.request {
			return
		}
		v.term, v.approved = 0, false
	case watchApprove, watchReject:
		if r.host != c || c.seat == 0 || v.request != request || v.term != term || v.round != round {
			return
		}
		v.approved = op == watchApprove
		if !v.approved {
			v.term = 0
		}
		if v.approved && (r.flow.phase == flowLobby || r.flow.phase == flowIntermission) {
			s.promoteRoomViewers(r, round)
			s.publishFlow(r)
		}
	default:
		return
	}
	s.publishWatchRoster(r)
}

func (s *Server) detachCombatForWatch(r *Room, c *Conn) {
	seat := c.seat
	s.clearObserverConn(r, c)
	s.punchDrop(r, c)
	r.bySeat[seat] = nil
	r.holds[seat] = seatHold{}
	r.roundWaitMask &^= 1 << seat
	r.flow.done &^= 1 << seat
	r.flow.ack &^= 1 << seat
	r.flow.applied &^= 1 << seat
	c.seat = 0
	r.broadcast(s, nil, encodeFrame(msgSrvPeerLeave, encodePeerLeave(seat, peerLeaveQuit)))
	if r.host == c {
		r.host = nil
		s.electFlowHost(r)
	}
	s.resetFlowBarrier(r)
	s.checkFlowBarrier(r)
}

func (s *Server) promoteRoomViewers(r *Room, round uint32) {
	for i := range r.viewers {
		v := &r.viewers[i]
		c := v.conn
		if c == nil || c.left.Load() || c.replaced.Load() || !v.approved || v.term != r.flow.term || v.round != round {
			continue
		}
		seat := r.lowestFreeSeat(time.Now())
		if seat == 0 {
			break
		}
		s.clearObserverConn(r, c)
		*v = roomViewer{}
		c.watchSlot, c.seat = 0, seat
		c.watchRoster = nil
		r.bySeat[seat] = c
		if r.flow.started && r.flow.phase != flowLobby {
			r.roundWaitMask |= 1 << seat
			c.watchReserved = uint8(i + maxPlayers + 1)
			c.watchJoinRound = round
			r.viewers[i].reserved = c
		}
		// WELCOME precedes the new round and is the sole combat-seat commit.
		s.enqueue(c, encodeFrame(msgSrvWelcome, encodeWelcome(seat, r.hostSeat, r.code, r.roster())))
		r.broadcast(s, c, encodeFrame(msgSrvPeerJoin, encodePeerJoin(rosterEntry{seat: seat, build: c.build, name: c.name})))
	}
}

func (s *Server) releaseWatchReservation(r *Room, c *Conn) {
	if c.watchReserved <= maxPlayers || c.watchReserved > watchRouteMax {
		return
	}
	v := &r.viewers[c.watchReserved-maxPlayers-1]
	if v.reserved == c {
		v.reserved = nil
	}
	c.watchReserved, c.watchJoinRound = 0, 0
	c.watchDeadline = time.Time{}
}

func (s *Server) settleWatchAdmissions(r *Room, now time.Time) {
	changed := false
	for _, c := range r.bySeat {
		if c == nil || c.watchReserved == 0 {
			continue
		}
		if c.watchJoinRound != r.flow.epoch {
			continue
		}
		if r.flow.prepPhase == 5 || r.flow.phase == flowBattle || r.flow.phase == flowVerify {
			s.releaseWatchReservation(r, c)
			changed = true
			continue
		}
		if r.flow.phase != flowSetup || c.watchDeadline.IsZero() || now.Before(c.watchDeadline) {
			continue
		}
		slot := c.watchReserved
		s.releaseWatchReservation(r, c)
		s.detachCombatForWatch(r, c)
		s.addRoomViewer(r, c, slot)
		changed = true
	}
	if changed {
		s.publishFlow(r)
	}
}

func (s *Server) removeRoomViewer(r *Room, c *Conn) {
	slot := c.watchSlot
	if r.watchPeer(slot) != c {
		return
	}
	r.viewers[slot-maxPlayers-1] = roomViewer{}
	if !c.left.Load() {
		r.viewerHolds[slot-maxPlayers-1] = seatHold{credential: c.credential, name: c.name, build: c.build, until: time.Now().Add(s.holdDuration)}
	}
	s.publishWatchRoster(r)
}

func (s *Server) closeRoomViewers(r *Room, reason uint8) {
	for i, v := range r.viewers {
		if v.conn == nil {
			continue
		}
		s.clearObserverConn(r, v.conn)
		v.conn.room.Store(nil)
		s.enqueueAndClose(v.conn, encodeFrame(msgSrvRoomClosed, encodeRoomClosed(reason)))
		r.viewers[i] = roomViewer{}
	}
}

func (s *Server) rejoinCombatAsViewer(r *Room, c, old *Conn, seat uint8, name []byte, build uint16, now time.Time) bool {
	h := r.holds[seat]
	if old != nil {
		h = seatHold{credential: old.credential, name: old.name, build: old.build, until: now.Add(s.holdDuration)}
	}
	if !now.Before(h.until) || h.build != build || string(h.name[:]) != string(name) || !sameCredential(h.credential, c.credential) {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRejoinSeatMismatch)))
		return true
	}
	slot := r.freeViewer()
	if old != nil && old.watchReserved != 0 {
		slot = old.watchReserved
	}
	if slot == 0 {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectRoomFull)))
		return true
	}
	if old != nil {
		s.releaseWatchReservation(r, old)
		s.detachCombatForWatch(r, old)
		old.replaced.Store(true)
		old.room.Store(nil)
		old.close()
	} else {
		r.holds[seat] = seatHold{}
		r.roundWaitMask &^= 1 << seat
		r.broadcast(s, nil, encodeFrame(msgSrvPeerLeave, encodePeerLeave(seat, peerLeaveQuit)))
	}
	c.build, c.name = build, h.name
	s.addRoomViewer(r, c, slot)
	s.publishFlow(r)
	return true
}

func (s *Server) routeObserver(r *Room, c *Conn, typ uint16, p []byte, flow bool) {
	if len(p) < 2 || c.watchRoute() == 0 || p[0] != c.watchRoute() || p[0] == p[1] ||
		r.watchPeer(p[0]) != c {
		return
	}
	t := r.watchPeer(p[1])
	if t == nil || (flow && !t.flow) {
		return
	}
	if (c.watchSlot != 0 || t.watchSlot != 0) && (!r.watchingRoom() || c.build != t.build) {
		return
	}
	s.observerLocked(r, c, t, typ, p, flow)
}
