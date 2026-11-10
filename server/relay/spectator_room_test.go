package main

import (
	"encoding/binary"
	"fmt"
	"testing"
	"time"
)

func watchFrame(c *testClient, typ uint16) wireFrame {
	c.t.Helper()
	for i := 0; i < 80; i++ {
		f := c.next(2 * time.Second)
		if f.typ == typ {
			return f
		}
	}
	c.t.Fatalf("expected watch frame %#x", typ)
	return wireFrame{}
}
func watchJoinBytes(code, name string, directory bool, credential []byte) []byte {
	p := make([]byte, 25)
	binary.LittleEndian.PutUint16(p, protocolVersion)
	binary.LittleEndian.PutUint16(p[2:], watchRoomBuild)
	copy(p[4:20], name)
	copy(p[20:24], code)
	if directory {
		p[24] = 1
	}
	return append(p, credential...)
}
func watchRoleBytes(op, slot uint8, term, round, request uint32) []byte {
	p := []byte{op, slot}
	p = binary.LittleEndian.AppendUint32(p, term)
	p = binary.LittleEndian.AppendUint32(p, round)
	return binary.LittleEndian.AppendUint32(p, request)
}
func TestWatchRoomFullFightAndIndependentViewers(t *testing.T) {
	s, addr := startTestServer(t, nil)
	var fighters [6]*testClient
	h := newTestClient(t, addr)
	fighters[0] = h
	code := decodeWelcome(t, h.create(1, watchRoomBuild, "host")).code
	for i := 1; i < 6; i++ {
		fighters[i] = newTestClient(t, addr)
		decodeWelcome(t, fighters[i].join(1, watchRoomBuild, fmt.Sprintf("player%d", i), code))
	}
	for _, c := range fighters {
		c.send(msgCliFlow, []byte{1, 0})
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled })
	h.send(msgCliPublish, []byte{1, 0, 1, 0})
	watchFrame(h, msgSrvPublished)
	flowBegin(t, s, code, h, fighters[1])
	list := newTestClient(t, addr)
	list.send(msgCliList, []byte{4, 0, watchRoomBuild, 0, 0, 0})
	page := watchFrame(list, msgSrvDirectory).payload
	if len(page) != 33 || string(page[8:12]) != code || page[30] != 6 || page[31] != 13 || page[32] != 0 {
		t.Fatalf("running directory metadata %x", page)
	}
	var viewers [4]*testClient
	for i := range viewers {
		c := newTestClient(t, addr)
		viewers[i] = c
		c.send(msgCliWatchJoin, watchJoinBytes(code, fmt.Sprintf("watch%d", i), true, nil))
		f := watchFrame(c, msgSrvWatchRoster)
		if f.payload[1] != uint8(7+i) {
			t.Fatalf("spectator route %d", f.payload[1])
		}
	}
	extra := newTestClient(t, addr)
	extra.send(msgCliWatchJoin, watchJoinBytes(code, "extra", true, nil))
	extra.expectReject(rejectRoomFull)
	flowWait(t, s, code, func(r *Room) bool { return r.flowMask() == 0x7e && r.watchers() == 4 })
	// Observer-route frames reach the real source, while spoofed game frames cannot.
	cap := binary.LittleEndian.AppendUint32(nil, observerLocalRevision)
	h.send(0xF023, cap)
	watchFrame(h, 0xF023)
	v := viewers[0]
	v.send(0xF023, cap)
	watchFrame(v, 0xF023)
	begin := obsControl(7, 1, 1, 170, 1, 1)
	binary.LittleEndian.PutUint32(begin[18:], observerLocalRevision)
	v.send(msgCliFlowGame, flowGameBytes(1, 1, 30, begin))
	f := watchFrame(h, msgSrvFlowGame)
	if f.payload[10] != 7 || f.payload[11] != 1 {
		t.Fatal("viewer control reached wrong source")
	}
	accept := obsControl(1, 7, 2, 170, 9, 1)
	binary.LittleEndian.PutUint32(accept[18:], observerLocalRevision)
	h.send(msgCliFlowGame, flowGameBytes(1, 1, 30, accept))
	watchFrame(v, msgSrvFlowGame)
	for _, p := range obsParts(1, 7, 170, 9, 1, 1, obsData()) {
		h.send(msgCliFlowGame, flowGameBytes(1, 1, 31, p))
	}
	if picture := watchFrame(v, msgSrvFlowGame); picture.payload[11] != 7 || binary.LittleEndian.Uint16(picture.payload[8:]) != 31 {
		t.Fatal("viewer missed its first picture")
	}
	v.send(msgCliFlowProgress, flowProgressBytes(1, 1, 5, 1, 0))
	v.send(msgCliFlowGame, flowGameBytes(1, 1, 6, []byte{7, 1, 0}))
	v.send(msgCliWatchRole, watchRoleBytes(watchApply, 7, 1, 2, 1))
	var request uint32
	flowWait(t, s, code, func(r *Room) bool {
		request = r.viewers[0].request
		return request != 0 && !r.flow.lost && r.flow.phase == flowBattle
	})
	h.send(msgCliWatchRole, watchRoleBytes(watchApprove, 7, 1, 2, request))
	flowWait(t, s, code, func(r *Room) bool { return r.viewers[0].approved && r.watchers() == 4 })
	fighters[1].send(msgCliLeaveRoom, nil)
	flowWait(t, s, code, func(r *Room) bool { return r.bySeat[2] == nil && r.nextCombatSeat(1) == 3 })
	for i, c := range fighters {
		if i != 1 {
			c.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 1, 0))
		}
	}
	var generation uint32
	flowWait(t, s, code, func(r *Room) bool { generation = r.flow.generation; return r.flow.phase == flowVerify })
	for i, c := range fighters {
		if i != 1 {
			c.send(msgCliFlowProgress, flowProgressBytes(1, 1, 2, 1, generation))
		}
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowComplete })
	for i, c := range fighters {
		if i != 1 {
			c.send(msgCliFlowProgress, flowProgressBytes(1, 1, 3, 1, generation))
		}
	}
	flowWait(t, s, code, func(r *Room) bool {
		return r.flow.phase == flowIntermission && r.bySeat[2] != nil && r.watchers() == 3 &&
			r.flowMask() == 0x7a && r.watchOrder[5] == 2
	})
	if w := decodeWelcome(t, watchFrame(v, msgSrvWelcome).payload); w.code != code {
		t.Fatal("promotion detached the room")
	}
	h.send(msgCliFlowStart, flowStartBytes(1, 2, 1))
	f = watchFrame(v, msgSrvFlowGame)
	if binary.LittleEndian.Uint32(f.payload[4:]) != 2 || binary.LittleEndian.Uint16(f.payload[8:]) != 3 {
		t.Fatal("promoted player did not receive next round START")
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flowMask() == 0x7e && r.flow.epoch == 2 && r.nextCombatSeat(6) == 2 })
	var viewerCredential [16]byte
	flowWait(t, s, code, func(r *Room) bool {
		viewerCredential = r.bySeat[2].credential
		return r.bySeat[2].watchReserved == 7 && r.freeViewer() == 0
	})
	rejoin := func(c *testClient, credential [16]byte) {
		payload := append(watchJoinBytes(code, "watch0", false, nil)[:24], 2)
		c.send(msgCliSecureRejoin, append(payload, credential[:]...))
	}
	spoof := newTestClient(t, addr)
	badCredential := viewerCredential
	badCredential[0] ^= 1
	rejoin(spoof, badCredential)
	spoof.expectReject(rejectRejoinSeatMismatch)
	flowWait(t, s, code, func(r *Room) bool { return r.bySeat[2].watchReserved == 7 && r.freeViewer() == 0 })
	reconnected := newTestClient(t, addr)
	rejoin(reconnected, viewerCredential)
	if roster := watchFrame(reconnected, msgSrvWatchRoster); roster.payload[1] != 7 {
		t.Fatal("authenticated reconnect did not reclaim its reserved spectator slot")
	}
}

func TestWatchRoomPrivateReconnectAndReusedSlot(t *testing.T) {
	s, addr := startTestServer(t, nil)
	h, g := newTestClient(t, addr), newTestClient(t, addr)
	code := decodeWelcome(t, h.create(1, watchRoomBuild, "host")).code
	decodeWelcome(t, g.join(1, watchRoomBuild, "guest", code))
	credential := admissionCredential(t, g)
	h.send(msgCliFlow, []byte{1, 0})
	g.send(msgCliFlow, []byte{1, 0})
	flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled })
	flowBegin(t, s, code, h, g)
	p := newTestClient(t, addr)
	p.send(msgCliWatchJoin, watchJoinBytes(code, "public", true, nil))
	p.expectReject(rejectJoinClosed)
	list := newTestClient(t, addr)
	list.send(msgCliList, []byte{4, 0, watchRoomBuild, 0, 0, 0})
	if page := watchFrame(list, msgSrvDirectory).payload; len(page) != 8 || page[7] != 0 {
		t.Fatal("private room leaked to public directory")
	}
	g.nc.Close()
	flowWait(t, s, code, func(r *Room) bool { return r.holdActive(2, time.Now()) })
	r := newTestClient(t, addr)
	r.send(msgCliSecureRejoin, append(append(watchJoinBytes(code, "guest", false, nil)[:24], 2), credential...))
	if got := watchFrame(r, msgSrvWatchRoster).payload[1]; got != 7 {
		t.Fatalf("authenticated reconnect occupied route %d", got)
	}
	flowWait(t, s, code, func(r *Room) bool { return r.bySeat[2] == nil && !r.holdActive(2, time.Now()) && r.flowMask() == 2 })
	r.send(msgCliWatchRole, watchRoleBytes(watchApply, 7, 1, 2, 1))
	var old uint32
	flowWait(t, s, code, func(r *Room) bool { old = r.viewers[0].request; return old != 0 })
	r.send(msgCliLeaveRoom, nil)
	flowWait(t, s, code, func(r *Room) bool { return r.watchers() == 0 })
	newcomer := newTestClient(t, addr)
	newcomer.send(msgCliWatchJoin, watchJoinBytes(code, "replacement", false, nil))
	watchFrame(newcomer, msgSrvWatchRoster)
	newcomer.send(msgCliWatchRole, watchRoleBytes(watchApply, 7, 1, 2, 1))
	flowWait(t, s, code, func(r *Room) bool { return r.viewers[0].request > old })
	h.send(msgCliWatchRole, watchRoleBytes(watchApprove, 7, 1, 2, old))
	// The next frame on the same link confirms the delayed approval was processed.
	h.send(msgCliPublish, []byte{1, 0, 0, 1})
	watchFrame(h, msgSrvPublished)
	flowWait(t, s, code, func(r *Room) bool { return !r.viewers[0].approved })
}
