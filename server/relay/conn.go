// @pvz-online: M3 Go 中继服务器 —— 单条连接。
// 每条连接固定三件事在跑：
//   - 读循环（run 所在 goroutine）→ srv.handle，收任何数据都刷新 lastActive；
//   - 写循环（另一个 goroutine）→ 这条连接唯一的写手（并发写同一 socket 会把帧字节搅碎）；
//   - 每 pingInterval 一拍：进过房的连接发 SRV_PING，并检查闲死（idleTimeout 没收到任何数据就断）。
package main

import (
	"net"
	"sync"
	"sync/atomic"
	"time"
)

// outMsg 写队列里的一条：要写的帧；closeAfter = 写完这帧就关连接
// （解散房间时的 ROOM_CLOSED 必须真的发出去，不能一 enqueue 就 close——那会把帧连同 socket 一起丢掉）。
type outMsg struct {
	frame      []byte
	closeAfter bool
}

type Conn struct {
	srv       *Server
	nc        net.Conn
	send      chan outMsg
	done      chan struct{}
	closeOnce sync.Once

	room atomic.Pointer[Room] // 所属房间（未进房为 nil）
	seat uint8                // 当前席位（1..6；未分配为 0）—— 持 srv.mu 改
	// seat/build/name 都由 srv.mu 保护（读席位在 forward/handle 里都持锁）
	build uint16
	name  [nameSize]byte

	timedOut   atomic.Bool  // 是不是闲死踢的（决定 PEER_LEAVE 的原因）
	left       atomic.Bool  // 收到过 LEAVE_ROOM（体面退房：不设席位保留，直接广播离开）
	replaced   atomic.Bool  // 被重连的快路径静默替换（席位已归新连接，退场时什么都不广播）
	lastActive atomic.Int64 // 最近收到它的任何数据的时间（unix nano）
}

func newConn(s *Server, nc net.Conn) *Conn {
	c := &Conn{
		srv:  s,
		nc:   nc,
		send: make(chan outMsg, 64),
		done: make(chan struct{}),
	}
	c.lastActive.Store(time.Now().UnixNano())
	return c
}

func (c *Conn) remoteAddr() string { return c.nc.RemoteAddr().String() }

func (c *Conn) close() {
	c.closeOnce.Do(func() {
		close(c.done)
		c.nc.Close()
	})
}

func (c *Conn) run() {
	go c.writeLoop()
	c.readLoop()
	reason := uint8(peerLeaveQuit)
	if c.timedOut.Load() {
		reason = peerLeaveTimeout
	}
	c.srv.removeConn(c, reason)
}

func (c *Conn) readLoop() {
	for {
		typ, payload, err := readFrame(c.nc)
		if err != nil {
			if err == errMalformed {
				c.srv.logf("malformed frame from %s - dropping the connection", c.remoteAddr())
			}
			return
		}
		c.lastActive.Store(time.Now().UnixNano())
		c.srv.handle(c, typ, payload)
	}
}

func (c *Conn) writeLoop() {
	ticker := time.NewTicker(c.srv.pingInterval)
	defer ticker.Stop()
	for {
		select {
		case <-c.done:
			return
		case m := <-c.send:
			if len(m.frame) > 0 && !c.write(m.frame) {
				return
			}
			if m.closeAfter {
				c.close()
				return
			}
		case <-ticker.C:
			if c.room.Load() != nil {
				if time.Since(time.Unix(0, c.lastActive.Load())) > c.srv.idleTimeout {
					c.srv.logf("%s timed out (no data for %v)", c.remoteAddr(), c.srv.idleTimeout)
					c.timedOut.Store(true)
					c.close()
					return
				}
				if !c.write(encodeFrame(msgSrvPing, nil)) {
					return
				}
			}
		}
	}
}

// write 写一帧，带 10 秒写超时（对面不收也不许把写循环挂死）。写失败即断连。
func (c *Conn) write(frame []byte) bool {
	c.nc.SetWriteDeadline(time.Now().Add(10 * time.Second))
	if _, err := c.nc.Write(frame); err != nil {
		c.close()
		return false
	}
	return true
}

// handle 收帧分发：控制帧服务器自己处理；游戏帧走转发。
func (s *Server) handle(c *Conn, typ uint16, payload []byte) {
	if typ >= srvFrameBase {
		switch typ {
		case msgCliCreateRoom:
			version, build, name, ok := decodeCreateRoom(payload)
			if !ok {
				s.dropBadControl(c, typ)
				return
			}
			s.handleCreate(c, version, build, name)
		case msgCliJoinRoom:
			version, build, name, code, ok := decodeJoinRoom(payload)
			if !ok {
				s.dropBadControl(c, typ)
				return
			}
			s.handleJoin(c, version, build, name, code)
		case msgCliRejoin:
			version, build, name, code, seat, ok := decodeRejoin(payload)
			if !ok {
				s.dropBadControl(c, typ)
				return
			}
			s.handleRejoin(c, version, build, name, code, seat)
		case msgCliSwapCommit:
			a, b, ok := decodeSwapCommit(payload)
			if !ok {
				s.dropBadControl(c, typ)
				return
			}
			s.handleSwapCommit(c, a, b)
		case msgCliPong:
			if len(payload) != 0 {
				s.dropBadControl(c, typ)
				return
			}
			// lastActive 已由读循环刷新，这里什么都不用做
		case msgCliLeaveRoom:
			if len(payload) != 0 {
				s.dropBadControl(c, typ)
				return
			}
			// 体面退房：不设席位保留。被打过标记的连接（已断/已替换）再来这条是无害空转。
			c.left.Store(true)
			s.removeConn(c, peerLeaveQuit)
		default:
			s.logf("unknown control frame %#x from %s - dropped", typ, c.remoteAddr())
		}
		return
	}
	// 游戏帧：至少要有 src + dst 两个字节，否则断线
	if len(payload) < 2 {
		s.logf("malformed game frame %#x from %s - dropping the connection", typ, c.remoteAddr())
		c.close()
		return
	}
	s.forward(c, typ, payload)
}

func (s *Server) dropBadControl(c *Conn, typ uint16) {
	s.logf("bad control frame %#x from %s - dropping the connection", typ, c.remoteAddr())
	c.close()
}
