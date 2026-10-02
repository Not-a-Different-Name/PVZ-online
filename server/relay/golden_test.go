package main

import (
	"bytes"
	"encoding/hex"
	"testing"
)

// 黄金字节：下面这些十六进制串，和 C++ 端 tools/nettest/proto_golden.cpp 里的是**同一份**。
// 谁单方面改了字段顺序、字节序或类型编号，谁自己的测试先红——这就是两边编解码的合同。
// 改协议时：两边一起改，向量一起更新。
var goldenVectors = map[string]string{
	"cli_create_room": "11f0140001001700416c6963650000000000000000000000",
	"cli_join_room":   "12f0180001001700426f6200000000000000000000000000374b5134",
	"cli_swap_commit": "13f002000103",
	"cli_pong":        "14f00000",
	"cli_leave_room":  "15f00000",
	"srv_welcome": "01f042000100030103374b5134011700486f7374000000000000000000000000" +
		"021600426f62000000000000000000000000000317004361726f6c0000000000000000000000",
	"srv_reject":      "02f00300010002",
	"srv_peer_join":   "03f0130004150044617665000000000000000000000000",
	"srv_peer_leave":  "04f002000201",
	"srv_room_closed": "05f0010000",
	"srv_seat_swap":   "06f002000204",
	"srv_ping":        "07f00000",
}

func goldenBytes(t *testing.T, name string) []byte {
	t.Helper()
	raw, ok := goldenVectors[name]
	if !ok {
		t.Fatalf("no golden vector %q", name)
	}
	b, err := hex.DecodeString(raw)
	if err != nil {
		t.Fatalf("golden %s: bad hex: %v", name, err)
	}
	return b
}

func nm16(s string) [16]byte {
	var b [16]byte
	copy(b[:], s)
	return b
}

func TestGoldenVectors(t *testing.T) {
	roster := []rosterEntry{
		{1, 23, nm16("Host")},
		{2, 22, nm16("Bob")},
		{3, 23, nm16("Carol")},
	}

	createName := nm16("Alice")
	var create buf
	create.u16(1)
	create.u16(23)
	create.raw(createName[:])

	joinName := nm16("Bob")
	var join buf
	join.u16(1)
	join.u16(23)
	join.raw(joinName[:])
	join.raw([]byte("7KQ4"))

	cases := []struct {
		name  string
		frame []byte
	}{
		{"cli_create_room", encodeFrame(msgCliCreateRoom, create.b)},
		{"cli_join_room", encodeFrame(msgCliJoinRoom, join.b)},
		{"cli_swap_commit", encodeFrame(msgCliSwapCommit, []byte{1, 3})},
		{"cli_pong", encodeFrame(msgCliPong, nil)},
		{"cli_leave_room", encodeFrame(msgCliLeaveRoom, nil)},
		{"srv_welcome", encodeFrame(msgSrvWelcome, encodeWelcome(3, 1, "7KQ4", roster))},
		{"srv_reject", encodeFrame(msgSrvReject, encodeReject(2))},
		{"srv_peer_join", encodeFrame(msgSrvPeerJoin, encodePeerJoin(rosterEntry{4, 21, nm16("Dave")}))},
		{"srv_peer_leave", encodeFrame(msgSrvPeerLeave, encodePeerLeave(2, 1))},
		{"srv_room_closed", encodeFrame(msgSrvRoomClosed, encodeRoomClosed(0))},
		{"srv_seat_swap", encodeFrame(msgSrvSeatSwap, encodeSeatSwap(2, 4))},
		{"srv_ping", encodeFrame(msgSrvPing, nil)},
	}
	for _, c := range cases {
		if want := goldenBytes(t, c.name); !bytes.Equal(c.frame, want) {
			t.Errorf("%s:\n got %x\nwant %x", c.name, c.frame, want)
		}
	}

	// 解码侧：同样拿黄金帧喂回服务器自己的解码器
	_, createPayload, _ := readFrame(bytes.NewReader(goldenBytes(t, "cli_create_room")))
	version, build, name, ok := decodeCreateRoom(createPayload)
	if !ok || version != 1 || build != 23 || cstr(name) != "Alice" {
		t.Fatalf("decode create: ok=%v version=%d build=%d name=%q", ok, version, build, cstr(name))
	}

	_, joinPayload, _ := readFrame(bytes.NewReader(goldenBytes(t, "cli_join_room")))
	version, build, name, code, ok := decodeJoinRoom(joinPayload)
	if !ok || version != 1 || build != 23 || cstr(name) != "Bob" || string(code) != "7KQ4" {
		t.Fatalf("decode join: ok=%v version=%d build=%d name=%q code=%q", ok, version, build, cstr(name), code)
	}

	_, swapPayload, _ := readFrame(bytes.NewReader(goldenBytes(t, "cli_swap_commit")))
	a, b, ok := decodeSwapCommit(swapPayload)
	if !ok || a != 1 || b != 3 {
		t.Fatalf("decode swap commit: ok=%v a=%d b=%d", ok, a, b)
	}
}
