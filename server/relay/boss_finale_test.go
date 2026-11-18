package main

import (
	"encoding/binary"
	"testing"
	"time"
)

func finaleTestStart(mode, difficulty byte, index uint16, flags byte) []byte {
	start := make([]byte, eliteWavesStartBytes)
	start[11], start[18], start[19], start[24] = 1, mode, difficulty, flags
	binary.LittleEndian.PutUint16(start[16:], index)
	return start
}

func TestFinaleHealthScaling(t *testing.T) {
	for _, tc := range []struct {
		build            uint16
		mode, difficulty byte
		index            uint16
		players          int
		want             uint32
	}{
		{finaleBuild, 2, 0, 5, 1, 80000},
		{finaleBuild, 2, 1, 5, 2, 175000},
		{finaleBuild, 1, 1, 10, 4, 715000},
		{finaleBuild, 0, 1, 25, 6, 2280000},
		{finaleBuild, 0, 2, 25, 6, 2850000},
		{finaleRebalanceBuild, 2, 0, 5, 1, 480000},
		{finaleRebalanceBuild, 2, 1, 5, 2, 1050000},
		{finaleRebalanceBuild, 1, 1, 10, 4, 1950000},
		{finaleRebalanceBuild, 2, 2, 5, 6, 3562500},
		{finaleRebalanceBuild, 1, 2, 10, 6, 3562500},
		{finaleRebalanceBuild, 0, 1, 25, 6, 2850000},
		{finaleRebalanceBuild, 0, 2, 25, 6, 3562500},
		{finaleRebalanceBuild, 0, 1, 24, 6, 0},
		{finaleRebalanceBuild, 3, 1, 25, 6, 0},
	} {
		f := newFinaleHealth(finaleTestStart(tc.mode, tc.difficulty, tc.index, finaleBossFlag), tc.players, tc.build)
		if f.maximum != tc.want || f.remaining != tc.want {
			t.Fatalf("build=%d mode=%d difficulty=%d index=%d players=%d: got %+v, want %d", tc.build, tc.mode, tc.difficulty, tc.index, tc.players, f, tc.want)
		}
	}
	if f := newFinaleHealth(finaleTestStart(2, 1, 5, 2), 4, finaleRebalanceBuild); f.maximum != 0 {
		t.Fatal("original giant-boss option must not initialize the finale")
	}
}

func TestFinaleHealthDeduplicatesByMember(t *testing.T) {
	f := newFinaleHealth(finaleTestStart(2, 1, 5, finaleBossFlag), 2, finaleBuild)
	if !f.record(101, 5000) || f.record(101, 5000) || f.record(101, 4000) || f.remaining != 170000 {
		t.Fatalf("same member or stale replay did not settle exactly once: %+v", f)
	}
	if !f.record(202, 160000) || !f.record(101, 15000) || f.remaining != 0 || f.record(202, 170000) {
		t.Fatalf("both players did not settle one health pool: %+v", f)
	}
}

func TestFinaleHealthBroadcast(t *testing.T) {
	t.Run("MOD117 quick normal", func(t *testing.T) { testFinaleHealthBroadcast(t, finaleBuild, 2, 1, 5, 175000) })
	t.Run("MOD118 quick normal", func(t *testing.T) { testFinaleHealthBroadcast(t, finaleRebalanceBuild, 2, 1, 5, 1050000) })
	t.Run("MOD118 standard hard", func(t *testing.T) { testFinaleHealthBroadcast(t, finaleRebalanceBuild, 1, 2, 10, 1312500) })
	t.Run("MOD118 full hard", func(t *testing.T) { testFinaleHealthBroadcast(t, finaleRebalanceBuild, 0, 2, 25, 1312500) })
}

func testFinaleHealthBroadcast(t *testing.T, build uint16, mode, difficulty byte, index uint16, maximum uint32) {
	t.Helper()
	s, addr := startTestServer(t, nil)
	host, guest := newTestClient(t, addr), newTestClient(t, addr)
	code := decodeWelcome(t, host.create(1, build, "host")).code
	decodeWelcome(t, guest.join(1, build, "guest", code))
	for _, c := range []*testClient{host, guest} {
		c.send(msgCliFlow, []byte{1, 0})
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flow.enabled && r.flowMask() == 6 })
	body := finaleTestStart(mode, difficulty, index, finaleBossFlag)
	body[0] = 1
	binary.LittleEndian.PutUint32(body[len(body)-5:], 1)
	start := append([]byte{1, 0, 0, 0, 1, 0, 0, 0}, body...)
	host.send(msgCliFlowStart, start)
	flowWait(t, s, code, func(r *Room) bool { return r.flow.epoch == 1 && r.finale.remaining == maximum })
	for _, c := range []*testClient{host, guest} {
		c.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 0, 0))
	}
	flowWait(t, s, code, func(r *Room) bool { return r.flow.phase == flowBattle })

	damage := func(c *testClient, claimed byte, total uint32) {
		var packet [6]byte
		packet[0], packet[1] = claimed, claimed
		binary.LittleEndian.PutUint32(packet[2:], total)
		c.send(msgCliFlowGame, flowGameBytes(1, 1, finaleDamageFrame, packet[:]))
	}
	expect := func(c *testClient, seat byte, remaining, applied uint32) {
		t.Helper()
		for i := 0; i < 40; i++ {
			frame := c.next(time.Second)
			p := frame.payload
			if frame.typ != msgSrvFlowGame || len(p) != 23 || binary.LittleEndian.Uint16(p[8:]) != finaleHealthFrame {
				continue
			}
			if p[10] != seat || binary.LittleEndian.Uint32(p[11:]) != maximum ||
				binary.LittleEndian.Uint32(p[15:]) != remaining || binary.LittleEndian.Uint32(p[19:]) != applied {
				t.Fatalf("seat %d: unexpected finale snapshot %v", seat, p)
			}
			return
		}
		t.Fatal("no health snapshot received")
	}

	damage(guest, 1, 30000)
	damage(guest, 2, 5000)
	expect(host, 1, maximum-5000, 0)
	expect(guest, 2, maximum-5000, 5000)
	guest.send(msgCliFlowProgress, flowProgressBytes(1, 1, 1, 1, 0))
	damage(guest, 2, 5000)
	expect(guest, 2, maximum-5000, 5000)
	flowWait(t, s, code, func(r *Room) bool { return r.flow.done == 0 })
	damage(host, 1, maximum-2000)
	expect(host, 1, 0, maximum-2000)
	expect(guest, 2, 0, 5000)
}
