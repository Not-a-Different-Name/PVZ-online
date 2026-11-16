package main

// Matchmaking is a bounded control-plane operation. Battles never pass through
// the coordinator, and network calls never hold the room lock.
import (
	"crypto/rand"
	"encoding/binary"
	"net"
	"sort"
	"strconv"
	"sync"
	"time"

	"pvzrelay/internal/catalogwire"
)

const msgMatch = 0xF026
const matchPreset = 1
const matchBuild = 114
const matchLease = 12 * time.Second

type matchReservation struct {
	committed bool
	token     [16]byte
	name      [nameSize]byte
	until     time.Time
}
type matchCoordinator struct {
	once   sync.Once
	slot   chan struct{}
	rateMu sync.Mutex
	rates  map[string]lookupRate
	window time.Time
	count  int
}

func (s *Server) allowMatchRoute(address string) bool {
	m := &s.matching
	m.rateMu.Lock()
	defer m.rateMu.Unlock()
	now := time.Now()
	ip, _, err := net.SplitHostPort(address)
	if err != nil {
		return false
	}
	if now.Sub(m.window) >= time.Second {
		m.window = now
		m.count = 0
	}
	if m.count >= 128 {
		return false
	}
	m.count++
	if m.rates == nil {
		m.rates = make(map[string]lookupRate)
	}
	v, exists := m.rates[ip]
	if !exists && len(m.rates) >= 2048 {
		for k, v := range m.rates {
			if now.Sub(v.window) >= time.Minute {
				delete(m.rates, k)
			}
		}
		if len(m.rates) >= 2048 {
			return false
		}
	}
	if now.Sub(v.window) >= time.Second {
		v = lookupRate{window: now}
	}
	if v.count >= 64 {
		return false
	}
	v.count++
	m.rates[ip] = v
	return true
}

type matchOffer struct {
	node      catalogwire.Node
	code      string
	created   uint64
	score     uint16
	canCreate bool
}

func (r *Room) matchOpen() bool {
	return r.matchPreset == matchPreset && !r.started && !r.flow.started && r.joinOpen && r.flow.phase == flowLobby && !r.sharedPaused
}
func (r *Room) matchReserved(seat int, now time.Time) bool {
	return !r.matchSeats[seat].committed && now.Before(r.matchSeats[seat].until)
}
func (r *Room) matchState() []byte {
	if r.matchPreset == 0 {
		return nil
	}
	active := byte(0)
	if r.matchOpen() && r.host != nil {
		active = 1
	}
	return []byte{1, 6, r.matchMode, active, r.matchPreset}
}
func (s *Server) publishMatch(r *Room, c *Conn) {
	state := r.matchState()
	if state != nil && string(c.matchState) != string(state) {
		c.matchState = append(c.matchState[:0], state...)
		s.enqueue(c, encodeFrame(msgMatch, state))
	}
}
func matchKey(p []byte) (uint16, byte, bool) {
	if len(p) < 5 {
		return 0, 0, false
	}
	build, mode := binary.LittleEndian.Uint16(p[2:]), p[4]
	return build, mode, p[0] == 1 && build >= matchBuild && mode <= 2
}
func matchFailure(op, status byte) []byte { return []byte{1, op, status} }

func (s *Server) handleMatch(c *Conn, p []byte) {
	if s.privateHost {
		s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(1, 3)))
		return
	}
	if len(p) < 2 || p[0] != 1 {
		s.dropBadControl(c, msgMatch)
		return
	}
	if c.room.Load() != nil {
		return
	}
	switch p[1] {
	case 1: // route: build, mode, catalog revision, player name
		if len(p) != 29 {
			s.dropBadControl(c, msgMatch)
			return
		}
		build, mode, ok := matchKey(p)
		if !ok {
			s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(1, 4)))
			return
		}
		allowed := s.allowMatchRoute(c.remoteAddr())
		if !allowed {
			s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(1, 2)))
			return
		}
		s.routeMatch(c, p, build, mode)
	case 2: // bounded metadata query, no room mutation
		if len(p) != 13 {
			s.dropBadControl(c, msgMatch)
			return
		}
		build, mode, ok := matchKey(p)
		if !ok {
			s.dropBadControl(c, msgMatch)
			return
		}
		if binary.LittleEndian.Uint64(p[5:]) != s.operations().Catalog.Revision {
			s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(2, 3)))
			return
		}
		s.enqueueAndClose(c, encodeFrame(msgMatch, s.matchInfo(build, mode)))
	case 3: // destination reserves one seat atomically
		if len(p) != 29 {
			s.dropBadControl(c, msgMatch)
			return
		}
		build, mode, ok := matchKey(p)
		if !ok {
			s.dropBadControl(c, msgMatch)
			return
		}
		if binary.LittleEndian.Uint64(p[21:]) != s.operations().Catalog.Revision {
			s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(3, 3)))
			return
		}
		s.enqueueAndClose(c, encodeFrame(msgMatch, s.reserveMatch(build, mode, p[5:21])))
	case 4: // cancellation authority is the unpredictable reservation token
		if len(p) != 22 {
			s.dropBadControl(c, msgMatch)
			return
		}
		s.mu.Lock()
		if r := s.rooms[string(p[2:6])]; r != nil {
			for seat := 1; seat <= maxPlayers; seat++ {
				reservation := r.matchSeats[seat]
				if !time.Now().Before(reservation.until) || string(reservation.token[:]) != string(p[6:]) {
					continue
				}
				if reservation.committed {
					if member := r.bySeat[seat]; member != nil && member.name == reservation.name {
						member.left.Store(true)
						member.room.Store(nil)
						s.removeSharedMember(r, member, peerLeaveQuit)
						member.close()
					} else if r.bySeat[seat] == nil && r.holds[seat].name == reservation.name {
						r.holds[seat] = seatHold{}
						r.roundWaitMask &^= 1 << seat
						r.broadcast(s, nil, encodeFrame(msgSrvPeerLeave, encodePeerLeave(uint8(seat), peerLeaveQuit)))
					}
				}
				r.matchSeats[seat] = matchReservation{}
			}
			if r.empty() {
				delete(s.rooms, r.code)
			} else {
				s.resetFlowBarrier(r)
				s.checkFlowBarrier(r)
				s.publishFlow(r)
			}
		}
		s.mu.Unlock()
		s.enqueueAndClose(c, encodeFrame(msgMatch, []byte{1, 4, 0}))
	case 5: // commit on the actual game connection, once
		if len(p) != 41 {
			s.dropBadControl(c, msgMatch)
			return
		}
		s.commitMatch(c, p)
	default:
		s.dropBadControl(c, msgMatch)
	}
}

func (s *Server) matchRoom(build uint16, mode byte, now time.Time) *Room {
	var selected *Room
	for _, r := range s.rooms {
		if r.matchOpen() && r.sharedBuild == build && r.matchMode == mode && r.lowestFreeSeat(now) != 0 &&
			(r.host != nil || r.connectedPlayers(true) == 0 && !r.sharedPaused) &&
			(selected == nil || r.matchCreated.Before(selected.matchCreated) || r.matchCreated.Equal(selected.matchCreated) && r.code < selected.code) {
			selected = r
		}
	}
	return selected
}
func (s *Server) matchInfo(build uint16, mode byte) []byte {
	s.status.mu.Lock()
	load := s.status.value.Operations.Score
	s.status.mu.Unlock()
	s.mu.Lock()
	defer s.mu.Unlock()
	var out buf
	out.u8(1)
	out.u8(2)
	out.u8(0)
	canCreate := s.operations().Config.AcceptNewRooms && len(s.rooms) < roomLimit
	if canCreate {
		out.u8(1)
	} else {
		out.u8(0)
	}
	if r := s.matchRoom(build, mode, time.Now()); r != nil {
		out.raw([]byte(r.code))
		out.b = binary.LittleEndian.AppendUint64(out.b, uint64(r.matchCreated.UnixMilli()))
	} else {
		out.raw(make([]byte, 12))
	}
	// Load is sampled independently. Do not walk gameplay objects or sample CPU here.
	base := float64(len(s.rooms)) / roomLimit
	if load > base {
		base = load
	}
	base = base * 100 / float64(s.operations().Config.Weight)
	if base > 60 {
		base = 60
	}
	out.u16(uint16(base * 1000))
	out.b = binary.LittleEndian.AppendUint64(out.b, s.operations().Catalog.Revision)
	return out.b
}
func (s *Server) reserveMatch(build uint16, mode byte, name []byte) []byte {
	s.mu.Lock()
	defer s.mu.Unlock()
	now := time.Now()
	r := s.matchRoom(build, mode, now)
	if r == nil {
		if !s.operations().Config.AcceptNewRooms || len(s.rooms) >= roomLimit {
			return matchFailure(3, 1)
		}
		code, ok := s.newRoomCode()
		if !ok {
			return matchFailure(3, 1)
		}
		r = &Room{code: code, sharedBuild: build, public: true, gateManaged: true, joinOpen: true, matchPreset: matchPreset, matchMode: mode, matchCreated: now}
		s.rooms[code] = r
	}
	seat := r.lowestFreeSeat(now)
	if seat == 0 {
		return matchFailure(3, 1)
	}
	var reservation matchReservation
	if _, err := rand.Read(reservation.token[:]); err != nil {
		if r.empty() {
			delete(s.rooms, r.code)
		}
		return matchFailure(3, 1)
	}
	copy(reservation.name[:], name)
	reservation.until = now.Add(matchLease)
	r.matchSeats[seat] = reservation
	out := []byte{1, 3, 0, mode}
	out = append(out, []byte(r.code)...)
	return append(out, reservation.token[:]...)
}
func (s *Server) commitMatch(c *Conn, p []byte) {
	build, mode, ok := matchKey(p)
	s.mu.Lock()
	defer s.mu.Unlock()
	r := s.rooms[string(p[5:9])]
	if !ok || r == nil || !r.matchOpen() || r.sharedBuild != build || r.matchMode != mode {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectJoinClosed)))
		return
	}
	seat := uint8(0)
	for i := 1; i <= maxPlayers; i++ {
		v := r.matchSeats[i]
		if r.matchReserved(i, time.Now()) && string(v.token[:]) == string(p[9:25]) && string(v.name[:]) == string(p[25:]) {
			seat = uint8(i)
			break
		}
	}
	if seat == 0 {
		s.enqueue(c, encodeFrame(msgSrvReject, encodeReject(rejectJoinClosed)))
		return
	}
	r.matchSeats[seat].committed = true
	c.seat, c.build = seat, build
	copy(c.name[:], p[25:])
	c.room.Store(r)
	r.bySeat[seat] = c
	if r.host == nil {
		r.host = c
		r.hostSeat = seat
	}
	s.welcomeShared(r, c)
	s.issueCredential(c)
	r.broadcast(s, c, encodeFrame(msgSrvPeerJoin, encodePeerJoin(rosterEntry{seat: seat, build: build, name: c.name})))
	s.publishFlow(r)
}

func matchRPC(node catalogwire.Node, p []byte) ([]byte, bool) {
	conn, err := net.DialTimeout("tcp", net.JoinHostPort(node.Host, strconv.Itoa(int(node.Port))), 500*time.Millisecond)
	if err != nil {
		return nil, false
	}
	defer conn.Close()
	conn.SetDeadline(time.Now().Add(900 * time.Millisecond))
	frame := encodeFrame(msgMatch, p)
	if n, err := conn.Write(frame); err != nil || n != len(frame) {
		return nil, false
	}
	typ, body, err := readFrame(conn)
	return body, err == nil && typ == msgMatch && len(body) >= 3 && body[0] == 1 && body[1] == p[1]
}
func (s *Server) routeMatch(c *Conn, p []byte, build uint16, mode byte) {
	// One coordinator for a catalog generation. Uncertainty never creates a second pool.
	ops := s.operations()
	nodes := make([]catalogwire.Node, 0, len(ops.Catalog.Nodes))
	for _, n := range ops.Catalog.Nodes {
		if n.Directory && n.Create {
			nodes = append(nodes, n)
		}
	}
	sort.Slice(nodes, func(i, j int) bool {
		a, _ := strconv.Atoi(nodes[i].ID)
		b, _ := strconv.Atoi(nodes[j].ID)
		return a < b
	})
	self := ops.Config.MatchNodeID
	if len(nodes) == 0 || self == "" || self != nodes[0].ID || !time.Now().Before(ops.Catalog.ExpiresAt) || binary.LittleEndian.Uint64(p[5:13]) != ops.Catalog.Revision {
		s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(1, 3)))
		return
	}
	s.matching.once.Do(func() { s.matching.slot = make(chan struct{}, 1) })
	wait := time.NewTimer(2 * time.Second)
	defer wait.Stop()
	select {
	case s.matching.slot <- struct{}{}:
		defer func() { <-s.matching.slot }()
	case <-c.done:
		return
	case <-wait.C:
		s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(1, 2)))
		return
	}
	if s.operations() != ops {
		s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(1, 3)))
		return
	}
	query := []byte{1, 2, byte(build), byte(build >> 8), mode}
	query = binary.LittleEndian.AppendUint64(query, ops.Catalog.Revision)
	// Bounded concurrent queries, all joined before returning; no background retry loop.
	results := make(chan matchOffer, len(nodes))
	semaphore := make(chan struct{}, 8)
	var workers sync.WaitGroup
	queryDeadline := time.Now().Add(2 * time.Second)
	for _, node := range nodes {
		workers.Add(1)
		go func(n catalogwire.Node) {
			defer workers.Done()
			semaphore <- struct{}{}
			defer func() { <-semaphore }()
			if time.Now().After(queryDeadline) {
				return
			}
			var info []byte
			ok := true
			if n.ID == self {
				info = s.matchInfo(build, mode)
			} else {
				info, ok = matchRPC(n, query)
			}
			if !ok || len(info) != 26 || info[2] != 0 || binary.LittleEndian.Uint64(info[18:]) != ops.Catalog.Revision {
				return
			}
			o := matchOffer{node: n, canCreate: info[3] == 1, score: binary.LittleEndian.Uint16(info[16:])}
			if validRoomCode(string(info[4:8])) {
				o.code = string(info[4:8])
				o.created = binary.LittleEndian.Uint64(info[8:16])
			}
			results <- o
		}(node)
	}
	workers.Wait()
	close(results)
	if s.operations() != ops {
		s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(1, 3)))
		return
	}
	var offers []matchOffer
	for o := range results {
		offers = append(offers, o)
	}
	// A missing node might hold an existing pool. Do not silently build elsewhere.
	if len(offers) != len(nodes) {
		s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(1, 1)))
		return
	}
	sort.SliceStable(offers, func(i, j int) bool {
		a, b := offers[i], offers[j]
		if (a.code != "") != (b.code != "") {
			return a.code != ""
		}
		if a.code != "" && a.created != b.created {
			return a.created < b.created
		}
		return a.score < b.score
	})
	for _, o := range offers {
		if o.code == "" && !o.canCreate {
			continue
		}
		select {
		case <-c.done:
			return
		default:
		}
		request := []byte{1, 3, byte(build), byte(build >> 8), mode}
		request = append(request, p[13:]...)
		request = binary.LittleEndian.AppendUint64(request, ops.Catalog.Revision)
		var reply []byte
		ok := true
		if o.node.ID == self {
			reply = s.reserveMatch(build, mode, p[13:])
		} else {
			reply, ok = matchRPC(o.node, request)
		}
		// A lost reservation reply is ambiguous. Its lease expires; never reserve twice.
		if !ok {
			break
		}
		if len(reply) != 24 || reply[2] != 0 {
			continue
		}
		id, _ := strconv.Atoi(o.node.ID)
		response := []byte{1, 1, 0, mode, byte(id), byte(id >> 8)}
		response = append(response, reply[4:]...)
		s.enqueueAndClose(c, encodeFrame(msgMatch, response))
		return
	}
	s.enqueueAndClose(c, encodeFrame(msgMatch, matchFailure(1, 1)))
}

// START fields are fixed even for a modified host. The seed and earned buffs remain dynamic.
func (r *Room) matchStartValid(p []byte) bool {
	if r.matchPreset == 0 {
		return true
	}
	if len(p) != eliteWavesStartBytes || p[11] != 1 {
		return false
	}
	want := []byte{r.matchMode, 1, 1, 1, 0, 0, 2, 0, 0, 1, 0, 1}
	return string(p[18:30]) == string(want)
}
