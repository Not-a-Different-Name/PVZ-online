package main

import (
	"encoding/binary"
	"math/bits"
)

const (
	finaleBossFlag    = 4
	finaleBuild       = 117
	finaleDamageFrame = 32
	finaleHealthFrame = 33
)

type finaleHealth struct {
	maximum   uint32
	remaining uint32
	// Shared-room identities survive reconnect and seat changes.
	applied map[uint32]uint32
}

func newFinaleHealth(start []byte, players int) finaleHealth {
	if len(start) != eliteWavesStartBytes || start[11] != 1 || start[24]&finaleBossFlag == 0 ||
		start[18] >= 3 || start[19] >= 3 || players < 1 || players > maxPlayers {
		return finaleHealth{}
	}
	mode, difficulty := start[18], start[19]
	if binary.LittleEndian.Uint16(start[16:]) != [...]uint16{25, 10, 5}[mode] {
		return finaleHealth{}
	}
	maximum := [...]uint32{480000, 220000, 100000}[mode] *
		[...]uint32{80, 100, 125}[difficulty] * uint32(4+3*(players-1)) / 400
	return finaleHealth{maximum: maximum, remaining: maximum, applied: make(map[uint32]uint32, players)}
}

func (f *finaleHealth) record(identity, total uint32) bool {
	previous := f.applied[identity]
	if f.remaining == 0 || identity == 0 || total <= previous || total > f.maximum {
		return false
	}
	f.applied[identity] = total
	damage := total - previous
	if damage >= f.remaining {
		f.remaining = 0
	} else {
		f.remaining -= damage
	}
	return true
}

func (s *Server) sendFinaleHealth(r *Room, c *Conn) {
	var body [13]byte
	body[0] = c.seat
	binary.LittleEndian.PutUint32(body[1:], r.finale.maximum)
	binary.LittleEndian.PutUint32(body[5:], r.finale.remaining)
	binary.LittleEndian.PutUint32(body[9:], r.finale.applied[c.watchIdentity])
	s.enqueue(c, flowEnvelope(r, finaleHealthFrame, body[:]))
}

func (s *Server) handleFinaleDamage(r *Room, c *Conn, body []byte) {
	if r.finale.maximum == 0 || len(body) != 6 || body[0] != c.seat || body[1] != c.seat ||
		c.watchIdentity == 0 || r.flowMask()&(1<<c.seat) == 0 {
		return
	}
	total := binary.LittleEndian.Uint32(body[2:])
	if r.flow.phase != flowBattle || r.flow.lost || !r.finale.record(c.watchIdentity, total) {
		s.sendFinaleHealth(r, c)
		return
	}
	for seat, player := range r.bySeat {
		if player != nil && r.flowMask()&(1<<seat) != 0 {
			s.sendFinaleHealth(r, player)
		}
	}
}

func (r *Room) beginFinale(start []byte) {
	r.finale = finaleHealth{}
	if r.sharedBuild != 0 {
		r.finale = newFinaleHealth(start, bits.OnesCount8(r.combatOnlineMask()))
	}
}
