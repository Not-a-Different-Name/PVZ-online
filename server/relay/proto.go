// @pvz-online: M3 Go 中继服务器 —— 线格式与游戏端 NetProtocol.h 完全一致：
// [u16 type][u16 len][payload]，小端，len 只算 payload（不含 4 字节头）。
// type < 0xF000 是游戏帧（服务器只读 payload[1]=dstSeat 逐字节转发）；
// 0xF000 起是控制帧（本文件的 encode/decode 管），服务器生成/消费、不转发。
//
// 改这里 = 改协议：任何一处动了都要和 C++ 端 NetProtocol.h 一起动，否则混搭出怪毛病。
package main

import (
	"encoding/binary"
	"errors"
	"io"
)

const (
	headerSize      = 4
	maxPayload      = 256
	protocolVersion = 1
	maxPlayers      = 4
	nameSize        = 16
	roomCodeLen     = 4

	srvFrameBase = 0xF000
)

// 控制帧类型（与 C++ 端 NetProto::ControlType 一一对应）
const (
	msgSrvWelcome    = 0xF001
	msgSrvReject     = 0xF002
	msgSrvPeerJoin   = 0xF003
	msgSrvPeerLeave  = 0xF004
	msgSrvRoomClosed = 0xF005
	msgSrvSeatSwap   = 0xF006
	msgSrvPing       = 0xF007

	msgCliCreateRoom = 0xF011
	msgCliJoinRoom   = 0xF012
	msgCliSwapCommit = 0xF013
	msgCliPong       = 0xF014
	msgCliLeaveRoom  = 0xF015
)

// REJECT 原因（与 NetProto::RejectReason 对齐）
const (
	rejectProtocolVersion = 0
	rejectRoomNotFound    = 1
	rejectRoomFull        = 2
	rejectBadCode         = 3
	rejectServerBusy      = 4
	rejectBadRequest      = 5
)

// ROOM_CLOSED 原因（与 NetProto::RoomClosedReason 对齐）
const (
	roomClosedHostLeft = 0
	roomClosedServer   = 1
)

// PEER_LEAVE 原因（与 NetProto::PeerLeaveReason 对齐）
const (
	peerLeaveQuit    = 0
	peerLeaveTimeout = 1
	peerLeaveKicked  = 2
)

// 房间码字母表：去掉 I/O/0/1 这些看岔了会读错的
const roomAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"

var errMalformed = errors.New("malformed frame")

// readFrame 读一整帧。帧头坏 / 载荷超长返回 errMalformed，调用方负责断连。
func readFrame(r io.Reader) (uint16, []byte, error) {
	var hdr [headerSize]byte
	if _, err := io.ReadFull(r, hdr[:]); err != nil {
		return 0, nil, err
	}
	typ := binary.LittleEndian.Uint16(hdr[0:2])
	n := binary.LittleEndian.Uint16(hdr[2:4])
	if int(n) > maxPayload {
		return 0, nil, errMalformed
	}
	payload := make([]byte, n)
	if n > 0 {
		if _, err := io.ReadFull(r, payload); err != nil {
			return 0, nil, err
		}
	}
	return typ, payload, nil
}

func encodeFrame(typ uint16, payload []byte) []byte {
	out := make([]byte, headerSize+len(payload))
	binary.LittleEndian.PutUint16(out[0:2], typ)
	binary.LittleEndian.PutUint16(out[2:4], uint16(len(payload)))
	copy(out[headerSize:], payload)
	return out
}

// ---- 小端读写（对应 C++ 的 Writer/Reader）----

type buf struct{ b []byte }

func (s *buf) u8(v uint8)   { s.b = append(s.b, v) }
func (s *buf) u16(v uint16) { s.b = binary.LittleEndian.AppendUint16(s.b, v) }
func (s *buf) raw(v []byte) { s.b = append(s.b, v...) }

type reader struct {
	b   []byte
	off int
}

func (r *reader) u8() (uint8, bool) {
	if r.off+1 > len(r.b) {
		return 0, false
	}
	v := r.b[r.off]
	r.off++
	return v, true
}

func (r *reader) u16() (uint16, bool) {
	if r.off+2 > len(r.b) {
		return 0, false
	}
	v := binary.LittleEndian.Uint16(r.b[r.off:])
	r.off += 2
	return v, true
}

func (r *reader) raw(n int) ([]byte, bool) {
	if r.off+n > len(r.b) {
		return nil, false
	}
	v := r.b[r.off : r.off+n]
	r.off += n
	return v, true
}

func (r *reader) done() bool { return r.off == len(r.b) }

// ---- 控制帧编码（服务器 → 客户端）----

type rosterEntry struct {
	seat  uint8
	build uint16
	name  [nameSize]byte
}

// WELCOME：{ u16 version, u8 yourSeat, u8 hostSeat, u8 seatCount, 4B code,
//
//	seatCount × { u8 seat, u16 build, 16B name } }
func encodeWelcome(yourSeat, hostSeat uint8, code string, roster []rosterEntry) []byte {
	var s buf
	s.u16(protocolVersion)
	s.u8(yourSeat)
	s.u8(hostSeat)
	s.u8(uint8(len(roster)))
	s.raw([]byte(code))
	for _, e := range roster {
		s.u8(e.seat)
		s.u16(e.build)
		s.raw(e.name[:])
	}
	return s.b
}

// REJECT：{ u16 version, u8 reason }
func encodeReject(reason uint8) []byte {
	var s buf
	s.u16(protocolVersion)
	s.u8(reason)
	return s.b
}

// PEER_JOIN：{ u8 seat, u16 build, 16B name }
func encodePeerJoin(e rosterEntry) []byte {
	var s buf
	s.u8(e.seat)
	s.u16(e.build)
	s.raw(e.name[:])
	return s.b
}

// PEER_LEAVE：{ u8 seat, u8 reason }
func encodePeerLeave(seat, reason uint8) []byte {
	var s buf
	s.u8(seat)
	s.u8(reason)
	return s.b
}

// ROOM_CLOSED：{ u8 reason }
func encodeRoomClosed(reason uint8) []byte {
	var s buf
	s.u8(reason)
	return s.b
}

// SEAT_SWAP：{ u8 seatA, u8 seatB }
func encodeSeatSwap(a, b uint8) []byte {
	var s buf
	s.u8(a)
	s.u8(b)
	return s.b
}

// ---- 控制帧解码（客户端 → 服务器）。载荷长度必须恰好对上，多一个少一个都算坏 ----

// CREATE_ROOM：{ u16 version, u16 build, 16B name }
func decodeCreateRoom(payload []byte) (version, build uint16, name []byte, ok bool) {
	if len(payload) != 2+2+nameSize {
		return 0, 0, nil, false
	}
	r := &reader{b: payload}
	version, _ = r.u16()
	build, _ = r.u16()
	name, _ = r.raw(nameSize)
	return version, build, name, true
}

// JOIN_ROOM：{ u16 version, u16 build, 16B name, 4B code }
func decodeJoinRoom(payload []byte) (version, build uint16, name, code []byte, ok bool) {
	if len(payload) != 2+2+nameSize+roomCodeLen {
		return 0, 0, nil, nil, false
	}
	r := &reader{b: payload}
	version, _ = r.u16()
	build, _ = r.u16()
	name, _ = r.raw(nameSize)
	code, _ = r.raw(roomCodeLen)
	return version, build, name, code, true
}

// SWAP_COMMIT：{ u8 seatA, u8 seatB }
func decodeSwapCommit(payload []byte) (a, b uint8, ok bool) {
	if len(payload) != 2 {
		return 0, 0, false
	}
	return payload[0], payload[1], true
}
