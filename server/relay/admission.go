package main

// Admission is serialized with joins and seat changes by Server.mu. A host must
// receive the closed-gate acknowledgement before sealing a preparation round.
import (
	"crypto/rand"
	"crypto/subtle"
	"encoding/binary"
	"time"
)

const (
	msgSrvGate            = 0xF00A
	msgSrvCredential      = 0xF00B
	msgCliGate            = 0xF017
	msgCliSecureRejoin    = 0xF018
	msgCliCancelAdmission = 0xF019
	rejectJoinClosed      = 8
	rejectBuildMismatch   = 9
	secureBuild           = 67
)

func (s *Server) issueCredential(c *Conn) bool {
	if c.build < secureBuild {
		return true
	}
	if c.credential == [16]byte{} {
		if _, err := rand.Read(c.credential[:]); err != nil {
			c.close()
			return false
		}
	}
	s.enqueue(c, encodeFrame(msgSrvCredential, c.credential[:]))
	return true
}

func (s *Server) handleGate(c *Conn, p []byte) {
	if (len(p) != 9 && len(p) != 13) || p[8] > 1 {
		s.dropBadControl(c, msgCliGate)
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil || r.host != c || c.build < secureBuild {
		return
	}
	if r.flow.enabled && (len(p) != 13 || binary.LittleEndian.Uint32(p[9:]) != r.flow.term) {
		return
	}
	request := binary.LittleEndian.Uint32(p)
	epoch := binary.LittleEndian.Uint32(p[4:])
	if request == 0 || epoch == 0 || request < r.gateRequest {
		return
	}
	if request == r.gateRequest && (epoch != r.gateEpoch || (p[8] != 0) != r.joinOpen) {
		return
	}
	r.gateManaged, r.gateRequest, r.gateEpoch, r.joinOpen = true, request, epoch, p[8] != 0
	response := append([]byte(nil), p[:9]...)
	mask := r.combatOccupiedMask(time.Now())
	response = append(response, mask)
	s.enqueue(c, encodeFrame(msgSrvGate, response))
}

func (s *Server) handleSecureRejoin(c *Conn, p []byte) {
	if len(p) != 2+2+nameSize+roomCodeLen+1+16 {
		s.dropBadControl(c, msgCliSecureRejoin)
		return
	}
	version, build, name, code, seat, ok := decodeRejoin(p[:len(p)-16])
	if !ok {
		s.dropBadControl(c, msgCliSecureRejoin)
		return
	}
	copy(c.credential[:], p[len(p)-16:])
	s.handleRejoin(c, version, build, name, code, seat)
}

func sameCredential(a, b [16]byte) bool {
	return a != [16]byte{} && subtle.ConstantTimeCompare(a[:], b[:]) == 1
}

// The coordinator can release an unfinished admission without consuming a held seat.
func (s *Server) handleCancelAdmission(c *Conn, payload []byte) {
	if len(payload) != 1 || payload[0] < 1 || payload[0] > maxPlayers {
		s.dropBadControl(c, msgCliCancelAdmission)
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	room := c.room.Load()
	if room == nil || room.host != c || c.build < secureBuild || payload[0] == room.hostSeat {
		return
	}
	seat := payload[0]
	target := room.bySeat[seat]
	if room.sharedBuild != 0 {
		if target != nil && target.watchJoinRound == room.flow.epoch && !target.seatObserver && !target.watchDeadline.IsZero() {
			target.sharedRole = sharedWatch
			target.watchDeadline = time.Time{}
			s.applySharedRoles(room)
			s.publishFlow(room)
		}
		return
	}
	room.joinOpen = false
	room.holds[seat] = seatHold{}
	if target != nil {
		target.left.Store(true)
		target.room.Store(nil)
		room.bySeat[seat] = nil
		s.enqueueAndClose(target, encodeFrame(msgSrvRoomClosed, encodeRoomClosed(2)))
	}
	room.broadcast(s, nil, encodeFrame(msgSrvPeerLeave, encodePeerLeave(seat, peerLeaveQuit)))
}
