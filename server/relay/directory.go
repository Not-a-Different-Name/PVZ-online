package main

// The directory contains only live room metadata; no polling goroutine, database,
// gameplay state or retained browser subscription is needed on the small relay.
import (
	"encoding/binary"
	"sort"
	"time"
)

const (
	msgCliPublish        = 0xF01A
	msgCliList           = 0xF01B
	msgCliDirectoryJoin  = 0xF01C
	msgSrvDirectory      = 0xF00C
	msgSrvPublished      = 0xF00D
	directoryPageSize    = 6
	legacyDirectoryLimit = 128
)

func (s *Server) handlePublish(c *Conn, p []byte) {
	if len(p) != 4 || binary.LittleEndian.Uint16(p) != 1 || p[2] > 1 || p[3] > 1 {
		s.dropBadControl(c, msgCliPublish)
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	r := c.room.Load()
	if r == nil || r.host != c || c.build < 68 {
		return
	}
	r.public = p[2] != 0
	if !r.flow.started {
		r.started = p[3] != 0
	}
	s.publishWatchRoster(r)
	s.enqueue(c, encodeFrame(msgSrvPublished, []byte{1, 0, p[2], p[3]}))
}

func (s *Server) handleList(c *Conn, p []byte) {
	// v3 is an exact-code read-only lookup, not an enumeration page.
	if len(p) >= 2 && binary.LittleEndian.Uint16(p) == 3 {
		s.handleRoomLookup(c, p)
		return
	}
	if len(p) != 6 {
		s.dropBadControl(c, msgCliList)
		return
	}
	version := binary.LittleEndian.Uint16(p)
	limit := roomLimit
	if version == 1 {
		// Older browsers reject page offsets beyond 126. Keep their last page
		// bounded while version 2 browsers can enumerate the entire directory.
		limit = legacyDirectoryLimit
	}
	offset := int(binary.LittleEndian.Uint16(p[4:]))
	if (version != 1 && version != 2 && version != 4) || offset >= limit || offset%directoryPageSize != 0 {
		s.dropBadControl(c, msgCliList)
		return
	}
	s.stats.directoryQueries.Add(1)
	s.mu.Lock()
	defer s.mu.Unlock()
	if c.room.Load() != nil {
		return
	}
	build := binary.LittleEndian.Uint16(p[2:])
	now := time.Now()
	codes := make([]string, 0, len(s.rooms))
	for code, r := range s.rooms {
		if r.public && r.host != nil && !r.host.left.Load() && (version == 4 || (!r.started && (!r.gateManaged || r.joinOpen))) {
			codes = append(codes, code)
		}
	}
	sort.Strings(codes)
	if len(codes) > limit {
		codes = codes[:limit]
	}
	end := offset + directoryPageSize
	if end > len(codes) {
		end = len(codes)
	}
	count := end - offset
	if count < 0 {
		count = 0
	}
	var out buf
	out.u16(version)
	out.u8(0)
	out.u16(uint16(offset))
	if end < len(codes) {
		out.u16(uint16(end))
	} else {
		out.u16(65535)
	}
	out.u8(uint8(count))
	for i := offset; i < end; i++ {
		r := s.rooms[codes[i]]
		players := byte(0)
		for seat := 1; seat <= maxPlayers; seat++ {
			if r.bySeat[seat] != nil || r.holdActive(uint8(seat), now) {
				players++
			}
		}
		flags := byte(0)
		if players >= maxPlayers {
			flags |= 1
		}
		if r.host.build != build {
			flags |= 2
		}
		out.raw([]byte(r.code))
		out.raw(r.host.name[:])
		out.u16(r.host.build)
		out.u8(players)
		if version == 4 {
			if r.started || (r.gateManaged && !r.joinOpen) {
				flags |= 4
			}
			if r.watchingRoom() {
				flags |= 8
			}
		}
		out.u8(flags)
		if version == 4 {
			out.u8(r.viewerSlotsUsed())
		}
	}
	// A single bounded response ends the browser connection, including on old servers' empty pages.
	s.enqueueAndClose(c, encodeFrame(msgSrvDirectory, out.b))
}
