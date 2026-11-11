package main

import (
	"encoding/binary"
	"fmt"
	"testing"
	"time"
)

func sharedRoleFrame(c *testClient, role uint8, term, epoch, request uint32) {
	c.send(msgCliWatchRole, watchRoleBytes(sharedSetRole, role, term, epoch, request))
}

func sharedStartFrame(t *testing.T, c *testClient, epoch uint32) []byte {
	t.Helper()
	for i := 0; i < 80; i++ {
		frame := c.next(time.Second)
		if frame.typ == msgSrvFlowGame && len(frame.payload) > 10 &&
			binary.LittleEndian.Uint16(frame.payload[8:]) == 3 && binary.LittleEndian.Uint32(frame.payload[4:]) == epoch {
			return frame.payload[10:]
		}
	}
	t.Fatal("expected START for promoted seat")
	return nil
}

func TestSharedSixSeatsAndNoApproval(t *testing.T) {
	s, addr := startTestServer(t, nil)
	h, g := newTestClient(t, addr), newTestClient(t, addr)
	code := decodeWelcome(t, h.create(1, sharedSeatBuild, "host")).code
	decodeWelcome(t, g.join(1, sharedSeatBuild, "guest", code))
	h.send(msgCliFlow, []byte{1, 0})
	g.send(msgCliFlow, []byte{1, 0})
	flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled && r.combatOnlineMask() == 6 })
	flowBegin(t, s, code, h, g)
	var viewers [4]*testClient
	for i := range viewers {
		v := newTestClient(t, addr)
		viewers[i] = v
		if seat := decodeWelcome(t, v.join(1, sharedSeatBuild, fmt.Sprintf("viewer%d", i), code)).yourSeat; seat != uint8(i+3) {
			t.Fatalf("spectator did not get real seat: %d", seat)
		}
		v.send(msgCliFlow, []byte{1, 0})
	}
	flowWait(t, s, code, func(r *Room) bool { return r.onlineMask() == 126 && r.flowMask() == 6 && r.watchers() == 0 })
	extra := newTestClient(t, addr)
	join := watchJoinBytes(code, "seventh", false, nil)[:24]
	binary.LittleEndian.PutUint16(join[2:], sharedSeatBuild)
	extra.send(msgCliJoinRoom, join)
	extra.expectReject(rejectRoomFull)
	v := viewers[0]
	var identity uint32
	flowWait(t, s, code, func(r *Room) bool { identity = r.bySeat[3].watchIdentity; return identity != 0 })
	h.send(msgCliWatchRole, watchRoleBytes(sharedSource, 1, 1, 1, 0))
	flowWait(t, s, code, func(r *Room) bool { return r.bySeat[1].watchSource })
	v.send(msgCliFlowProgress, flowProgressBytes(1, 1, 4, 1, 0))
	v.send(msgCliFlowProgress, flowProgressBytes(1, 1, 5, 1, 0))
	sharedRoleFrame(v, sharedFight, 1, 1, 1)
	sharedRoleFrame(g, sharedWatch, 1, 1, 1)
	sharedRoleFrame(h, sharedWatch, 1, 1, 1)
	flowWait(t, s, code, func(r *Room) bool {
		return r.bySeat[3].sharedRole == sharedFight && r.bySeat[2].sharedRole == sharedWatch &&
			r.bySeat[1].sharedRole == 0 && !r.flow.lost && r.flow.phase == flowBattle && r.flowMask() == 6
	})
	for _, c := range []*testClient{h, g} {
		c.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 1, 0))
	}
	var generation uint32
	flowWait(t, s, code, func(r *Room) bool { generation = r.flow.generation; return r.flow.phase == flowVerify })
	for _, c := range []*testClient{h, g} {
		c.send(msgCliFlowProgress, flowProgressBytes(1, 1, 2, 1, generation))
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowComplete })
	for _, c := range []*testClient{h, g} {
		c.send(msgCliFlowProgress, flowProgressBytes(1, 1, 3, 1, generation))
	}
	flowWait(t, s, code, func(r *Room) bool {
		return r.flow.phase == flowIntermission && r.bySeat[2].seatObserver && !r.bySeat[3].seatObserver &&
			r.bySeat[3].watchIdentity == identity && r.onlineMask() == 126 && r.nextCombatSeat(1) == 0
	})
	h.send(msgCliFlowStart, flowStartBytes(1, 2, 1))
	if start := sharedStartFrame(t, v, 2); start[1] != 3 {
		t.Fatal("START routed to the wrong seat")
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flowMask() == 10 && r.nextCombatSeat(1) == 3 })
}

func TestSharedRejoinAndPausedRecovery(t *testing.T) {
	s, addr := startTestServer(t, nil)
	h, g := newTestClient(t, addr), newTestClient(t, addr)
	code := decodeWelcome(t, h.create(1, sharedSeatBuild, "host")).code
	decodeWelcome(t, g.join(1, sharedSeatBuild, "guest", code))
	credential := admissionCredential(t, g)
	h.send(msgCliFlow, []byte{1, 0})
	g.send(msgCliFlow, []byte{1, 0})
	var identity uint32
	flowWait(t, s, code, func(r *Room) bool { identity = r.bySeat[2].watchIdentity; return r.flow.enabled && identity != 0 })
	flowBegin(t, s, code, h, g)
	g.nc.Close()
	flowWait(t, s, code, func(r *Room) bool { return r.holdActive(2, time.Now()) && r.flowMask() == 2 })
	reconnected := newTestClient(t, addr)
	p := append(watchJoinBytes(code, "guest", false, nil)[:24], 2)
	binary.LittleEndian.PutUint16(p[2:], sharedSeatBuild)
	reconnected.send(msgCliSecureRejoin, append(p, credential...))
	if seat := decodeWelcome(t, watchFrame(reconnected, msgSrvWelcome).payload).yourSeat; seat != 2 {
		t.Fatal("reconnected member moved seats")
	}
	flowWait(t, s, code, func(r *Room) bool {
		return r.bySeat[2] != nil && r.bySeat[2].seatObserver && r.bySeat[2].watchIdentity == identity
	})
	h.send(msgCliLeaveRoom, nil)
	flowWait(t, s, code, func(r *Room) bool { return r.sharedPaused && r.host == nil && r.flowMask() == 0 && r.flow.epoch == 1 })
	sharedRoleFrame(reconnected, sharedFight, 1, 1, 1)
	flowWait(t, s, code, func(r *Room) bool {
		return !r.sharedPaused && r.hostSeat == 2 && r.flow.phase == flowSetup && r.flow.epoch == 2 && r.flow.term == 2 && r.flowMask() == 4
	})
	if frame := watchFrame(reconnected, msgSrvSharedResume).payload; binary.LittleEndian.Uint32(frame[len(frame)-5:]) != 2 {
		t.Fatal("resume did not bind the restarted epoch")
	}
}

func TestSharedPreparationBoundaryAndRetry(t *testing.T) {
	s, addr := startTestServer(t, nil)
	h, g := newTestClient(t, addr), newTestClient(t, addr)
	code := decodeWelcome(t, h.create(1, sharedSeatBuild, "host")).code
	decodeWelcome(t, g.join(1, sharedSeatBuild, "guest", code))
	h.send(msgCliFlow, []byte{1, 0})
	g.send(msgCliFlow, []byte{1, 0})
	flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled })
	h.send(msgCliPublish, []byte{1, 0, 1, 0})
	watchFrame(h, msgSrvPublished)
	h.send(msgCliFlowStart, flowStartBytes(1, 1, 1))
	flowWait(t, s, code, func(r *Room) bool { return r.sharedPreparationOpen() })
	directory := newTestClient(t, addr)
	directory.send(msgCliList, []byte{4, 0, sharedSeatBuild, 0, 0, 0})
	page := watchFrame(directory, msgSrvDirectory).payload
	if len(page) != 33 || string(page[8:12]) != code || page[30] != 2 || page[31]&12 != 12 {
		t.Fatalf("ongoing room with a shared seat available missing from directory: %x", page)
	}
	early := newTestClient(t, addr)
	decodeWelcome(t, early.join(1, sharedSeatBuild, "early", code))
	early.send(msgCliFlow, []byte{1, 0})
	sharedRoleFrame(early, sharedFight, 1, 1, 1)
	if start := sharedStartFrame(t, early, 1); start[1] != 3 {
		t.Fatal("open preparation did not replay the current START to the new fighter")
	}
	prep := make([]byte, 92)
	prep[0], prep[1], prep[2], prep[39] = 1, 2, 6, 4
	binary.LittleEndian.PutUint32(prep[3:], 1)
	binary.LittleEndian.PutUint32(prep[7:], 1)
	h.send(msgCliFlowGame, flowGameBytes(1, 1, 25, prep))
	flowWait(t, s, code, func(r *Room) bool { return !r.sharedPreparationOpen() && r.flow.prepPhase == 4 })
	late := newTestClient(t, addr)
	decodeWelcome(t, late.join(1, sharedSeatBuild, "late", code))
	late.send(msgCliFlow, []byte{1, 0})
	sharedRoleFrame(late, sharedFight, 1, 1, 1)
	flowWait(t, s, code, func(r *Room) bool { return r.bySeat[4].seatObserver && r.bySeat[4].sharedRole == sharedFight })
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 4, 1, 0))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.lost })
	h.send(msgCliFlowStart, flowStartBytes(1, 2, 1))
	if start := sharedStartFrame(t, late, 2); start[1] != 4 {
		t.Fatal("retry did not apply queued role at safe boundary")
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flowMask() == 30 && !r.flow.lost && !r.bySeat[4].seatObserver })
}

func TestSharedReturnToMenuKeepsRoomAndMembership(t *testing.T) {
	s, addr := startTestServer(t, func(s *Server) { s.privateHost = true })
	h, g := newTestClient(t, addr), newTestClient(t, addr)
	code := decodeWelcome(t, h.create(1, sharedSeatBuild, "host")).code
	decodeWelcome(t, g.join(1, sharedSeatBuild, "guest", code))
	h.send(msgCliFlow, []byte{1, 0})
	g.send(msgCliFlow, []byte{1, 0})
	var identity uint32
	flowWait(t, s, code, func(r *Room) bool {
		identity = r.bySeat[2].watchIdentity
		return r.flow.enabled && identity != 0
	})
	flowBegin(t, s, code, h, g)

	g.send(msgCliFlowProgress, flowProgressBytes(1, 1, 5, 1, 0))
	flowWait(t, s, code, func(r *Room) bool {
		return r.host == r.bySeat[1] && r.flow.phase == flowBattle && r.flowMask() == 2 &&
			r.bySeat[2].seatObserver && r.bySeat[2].watchIdentity == identity
	})
	// The remaining fighter can retry a failed stage, then retire that round
	// without touching the room or promoting the viewer.
	h.send(msgCliFlowProgress, flowProgressBytes(1, 1, 4, 1, 0))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.lost })
	h.send(msgCliFlowStart, flowStartBytes(1, 2, 1))
	flowWait(t, s, code, func(r *Room) bool { return r.flow.epoch == 2 && r.flow.phase == flowSetup && !r.flow.lost })
	h.send(msgCliFlowProgress, flowProgressBytes(1, 2, 5, 1, 0))
	flowWait(t, s, code, func(r *Room) bool {
		return r.host == r.bySeat[1] && r.flow.phase == flowLobby && !r.started && r.joinOpen &&
			r.bySeat[2].seatObserver && r.bySeat[2].watchIdentity == identity && len(r.sharedStart) == 0
	})
	sharedRoleFrame(g, sharedFight, 1, 2, 1)
	flowWait(t, s, code, func(r *Room) bool {
		return r.host == r.bySeat[1] && r.combatOnlineMask() == 6 &&
			!r.bySeat[2].seatObserver && r.bySeat[2].watchIdentity == identity
	})
	h.send(msgCliFlowStart, flowStartBytes(1, 3, 1))
	if start := sharedStartFrame(t, g, 3); start[1] != 2 {
		t.Fatal("new round did not reach the same guest seat")
	}
}
