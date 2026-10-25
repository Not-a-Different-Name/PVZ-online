// @pvz-online: Keep administration local and lightweight so status inspection
// does not expose room secrets or add per-request work to game forwarding.
package main

import (
	_ "embed"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"runtime"
	"sync"
	"sync/atomic"
	"time"
)

const relayRelease = "2026-10-25-observer-direct1"
const defaultAdminAddress = "127.0.0.1:27778"
const connectionLimit = 512
const roomLimit = 256

// These are process-local counters. No player names, room codes or credentials
// are included in the administrative output.
type relayCounters struct {
	accepted, closed, capacityRejected            atomic.Uint64
	inBytes, outBytes, inFrames, outFrames        atomic.Uint64
	forwarded, directoryQueries, queueDrops       atomic.Uint64
	malformed, rateLimited, timedOut, writeErrors atomic.Uint64
}

type processResources struct {
	RSSBytes                 *uint64  `json:"rss_bytes"`
	CPUSeconds               *float64 `json:"cpu_seconds"`
	HostMemoryTotalBytes     *uint64  `json:"host_memory_total_bytes"`
	HostMemoryAvailableBytes *uint64  `json:"host_memory_available_bytes"`
	HostLoad1                *float64 `json:"host_load_1"`
}

type relayStatus struct {
	Operations                operationsStatus `json:"operations"`
	Release                   string           `json:"release"`
	Protocol                  int              `json:"protocol"`
	PID                       int              `json:"pid"`
	StartedAt                 time.Time        `json:"started_at"`
	SampledAt                 time.Time        `json:"sampled_at"`
	UptimeSeconds             float64          `json:"uptime_seconds"`
	SampleSeconds             float64          `json:"sample_seconds"`
	GamePort                  int              `json:"game_port"`
	Connections               int              `json:"connections"`
	Players                   int              `json:"players"`
	AnonymousConnections      int              `json:"anonymous_connections"`
	Rooms                     int              `json:"rooms"`
	PublicWaitingRooms        int              `json:"public_waiting_rooms"`
	StartedRooms              int              `json:"started_rooms"`
	HeldSeats                 int              `json:"held_seats"`
	QueuedFrames              int              `json:"queued_frames"`
	MaxConnectionQueue        int              `json:"max_connection_queue"`
	ConnectionLimit           int              `json:"connection_limit"`
	RoomLimit                 int              `json:"room_limit"`
	Goroutines                int              `json:"goroutines"`
	CPUs                      int              `json:"cpus"`
	GoMaxProcs                int              `json:"go_max_procs"`
	HeapBytes                 uint64           `json:"heap_bytes"`
	GoReservedBytes           uint64           `json:"go_reserved_bytes"`
	Process                   processResources `json:"process"`
	CPUPercentOneCore         *float64         `json:"cpu_percent_one_core"`
	AcceptedTotal             uint64           `json:"accepted_total"`
	ClosedTotal               uint64           `json:"closed_total"`
	CapacityRejectedTotal     uint64           `json:"capacity_rejected_total"`
	ReceivedBytesTotal        uint64           `json:"received_bytes_total"`
	SentBytesTotal            uint64           `json:"sent_bytes_total"`
	ReceivedFramesTotal       uint64           `json:"received_frames_total"`
	SentFramesTotal           uint64           `json:"sent_frames_total"`
	ForwardedFramesTotal      uint64           `json:"forwarded_frames_total"`
	DirectoryQueriesTotal     uint64           `json:"directory_queries_total"`
	QueueDropsTotal           uint64           `json:"queue_drops_total"`
	MalformedTotal            uint64           `json:"malformed_total"`
	RateLimitedTotal          uint64           `json:"rate_limited_total"`
	TimedOutTotal             uint64           `json:"timed_out_total"`
	WriteErrorsTotal          uint64           `json:"write_errors_total"`
	PunchCapableTotal         uint64           `json:"punch_capable_total"`
	PunchHellosTotal          uint64           `json:"punch_hellos_total"`
	PunchOffersTotal          uint64           `json:"punch_offers_total"`
	PunchDirectTotal          uint64           `json:"punch_direct_total"`
	PunchFailedTotal          uint64           `json:"punch_failed_total"`
	ReceivedBytesPerSecond    float64          `json:"received_bytes_per_second"`
	SentBytesPerSecond        float64          `json:"sent_bytes_per_second"`
	ForwardedFramesPerSecond  float64          `json:"forwarded_frames_per_second"`
	DirectoryQueriesPerSecond float64          `json:"directory_queries_per_second"`
}

type statusCache struct {
	mu          sync.Mutex
	value       relayStatus
	history     []trendSample
	previousCPU *float64
}

// Meter actual application-stream bytes, including partial writes/invalid input.
// TCP/IP headers, ACKs, retransmissions and kernel socket buffers are excluded.
type meteredConn struct {
	net.Conn
	stats *relayCounters
}

func (c *meteredConn) Read(p []byte) (int, error) {
	n, err := c.Conn.Read(p)
	c.stats.inBytes.Add(uint64(n))
	return n, err
}
func (c *meteredConn) Write(p []byte) (int, error) {
	n, err := c.Conn.Write(p)
	c.stats.outBytes.Add(uint64(n))
	return n, err
}

func (s *Server) sampleStatus(now time.Time) relayStatus {
	s.status.mu.Lock()
	defer s.status.mu.Unlock()
	previous := s.status.value
	if !previous.SampledAt.IsZero() && now.Sub(previous.SampledAt) < time.Second {
		return previous
	}
	v := relayStatus{Release: relayRelease, Protocol: protocolVersion, PID: os.Getpid(), StartedAt: s.startedAt,
		SampledAt: now, UptimeSeconds: now.Sub(s.startedAt).Seconds(), GamePort: s.gamePort,
		ConnectionLimit: connectionLimit, RoomLimit: roomLimit, CPUs: runtime.NumCPU(), GoMaxProcs: runtime.GOMAXPROCS(0)}
	s.connections.Range(func(key, value any) bool {
		c := key.(*Conn)
		v.Connections++
		if c.room.Load() == nil {
			v.AnonymousConnections++
		}
		n := len(c.send)
		v.QueuedFrames += n
		if n > v.MaxConnectionQueue {
			v.MaxConnectionQueue = n
		}
		return true
	})
	s.mu.Lock()
	v.Rooms = len(s.rooms)
	for _, r := range s.rooms {
		if r.started {
			v.StartedRooms++
		}
		if r.public && !r.started && r.host != nil && !r.host.left.Load() && (!r.gateManaged || r.joinOpen) {
			v.PublicWaitingRooms++
		}
		for seat := 1; seat <= maxPlayers; seat++ {
			if c := r.bySeat[seat]; c != nil {
				select {
				case <-c.done:
				default:
					v.Players++
				}
			} else if r.holdActive(uint8(seat), now) {
				v.HeldSeats++
			}
		}
	}
	s.mu.Unlock()
	var memory runtime.MemStats
	runtime.ReadMemStats(&memory)
	v.HeapBytes = memory.HeapAlloc
	v.GoReservedBytes = memory.Sys
	v.Goroutines = runtime.NumGoroutine()
	v.Process = readProcessResources()
	c := &s.stats
	v.AcceptedTotal = c.accepted.Load()
	v.ClosedTotal = c.closed.Load()
	v.CapacityRejectedTotal = c.capacityRejected.Load()
	v.ReceivedBytesTotal = c.inBytes.Load()
	v.SentBytesTotal = c.outBytes.Load()
	v.ReceivedFramesTotal = c.inFrames.Load()
	v.SentFramesTotal = c.outFrames.Load()
	v.ForwardedFramesTotal = c.forwarded.Load()
	v.DirectoryQueriesTotal = c.directoryQueries.Load()
	v.QueueDropsTotal = c.queueDrops.Load()
	v.MalformedTotal = c.malformed.Load()
	v.RateLimitedTotal = c.rateLimited.Load()
	v.TimedOutTotal = c.timedOut.Load()
	v.WriteErrorsTotal = c.writeErrors.Load()
	v.PunchCapableTotal = s.punch.capable.Load()
	v.PunchHellosTotal = s.punch.hellosSeen.Load()
	v.PunchOffersTotal = s.punch.offers.Load()
	v.PunchDirectTotal = s.punch.opened.Load()
	v.PunchFailedTotal = s.punch.failed.Load()
	if !previous.SampledAt.IsZero() {
		v.SampleSeconds = now.Sub(previous.SampledAt).Seconds()
		v.ReceivedBytesPerSecond = float64(v.ReceivedBytesTotal-previous.ReceivedBytesTotal) / v.SampleSeconds
		v.SentBytesPerSecond = float64(v.SentBytesTotal-previous.SentBytesTotal) / v.SampleSeconds
		v.ForwardedFramesPerSecond = float64(v.ForwardedFramesTotal-previous.ForwardedFramesTotal) / v.SampleSeconds
		v.DirectoryQueriesPerSecond = float64(v.DirectoryQueriesTotal-previous.DirectoryQueriesTotal) / v.SampleSeconds
		if v.Process.CPUSeconds != nil && s.status.previousCPU != nil {
			percent := 100 * (*v.Process.CPUSeconds - *s.status.previousCPU) / v.SampleSeconds
			if percent < 0 {
				percent = 0
			}
			v.CPUPercentOneCore = &percent
		}
	}
	v.Operations = s.summarizeOperations(v)
	s.observerBusy.Store(v.Operations.Busy)
	s.status.previousCPU = v.Process.CPUSeconds
	s.status.value = v
	return v
}

func localAdminAddress(address string) error {
	host, port, err := net.SplitHostPort(address)
	if err != nil || port == "" {
		return errors.New("status address must be a loopback IP and port, for example 127.0.0.1:27778")
	}
	ip := net.ParseIP(host)
	if ip == nil || !ip.IsLoopback() {
		return errors.New("status listener is local-only; use 127.0.0.1 or ::1")
	}
	return nil
}

//go:embed status.html
var statusPage string

func (s *Server) statusHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("Content-Security-Policy", "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'")
		if r.Method != http.MethodGet {
			w.Header().Set("Allow", "GET")
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		switch r.URL.Path {
		case "/":
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
			io.WriteString(w, statusPage)
		case "/status":
			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(s.sampleStatus(time.Now()))
		case "/history":
			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(s.historySnapshot())
		case "/healthz":
			w.Header().Set("Content-Type", "text/plain; charset=utf-8")
			io.WriteString(w, "ok\n")
		default:
			http.NotFound(w, r)
		}
	})
}

func (s *Server) startStatus(address string) (*http.Server, error) {
	if err := localAdminAddress(address); err != nil {
		return nil, err
	}
	listener, err := net.Listen("tcp", address)
	if err != nil {
		return nil, err
	}
	server := &http.Server{Handler: s.statusHandler(), ReadHeaderTimeout: 2 * time.Second, ReadTimeout: 3 * time.Second,
		WriteTimeout: 3 * time.Second, IdleTimeout: 5 * time.Second, MaxHeaderBytes: 4096}
	s.sampleStatus(time.Now())
	done := make(chan struct{})
	go func() {
		defer close(done)
		if err := server.Serve(listener); err != nil && err != http.ErrServerClosed {
			s.logf("status HTTP stopped: %v", err)
		}
	}()
	s.startSampling("")
	return server, nil
}

func printStatus(address string, raw bool, out io.Writer) error {
	if err := localAdminAddress(address); err != nil {
		return err
	}
	client := &http.Client{Timeout: 3 * time.Second, Transport: &http.Transport{Proxy: nil, DisableKeepAlives: true},
		CheckRedirect: func(req *http.Request, via []*http.Request) error { return http.ErrUseLastResponse }}
	response, err := client.Get("http://" + address + "/status")
	if err != nil {
		return fmt.Errorf("无法读取本机状态；请确认新版服务已启动：%w", err)
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return fmt.Errorf("status HTTP: %s", response.Status)
	}
	var v relayStatus
	if err = json.NewDecoder(io.LimitReader(response.Body, 64*1024)).Decode(&v); err != nil {
		return err
	}
	if raw {
		encoder := json.NewEncoder(out)
		encoder.SetIndent("", "  ")
		return encoder.Encode(v)
	}
	fmt.Fprintf(out, "PvZ 中继服务 %s · PID %d · TCP %d\n运行 %s；采样 %s（约每秒更新）\n", v.Release, v.PID, v.GamePort,
		time.Duration(v.UptimeSeconds*float64(time.Second)).Round(time.Second), v.SampledAt.Local().Format("15:04:05"))
	fmt.Fprintf(out, "连接 %d/%d（房外 %d）  玩家 %d  房间 %d/%d\n公开待开局 %d  已开局 %d  掉线保留席 %d\n",
		v.Connections, v.ConnectionLimit, v.AnonymousConnections, v.Players, v.Rooms, v.RoomLimit, v.PublicWaitingRooms, v.StartedRooms, v.HeldSeats)
	cpu := "采样中/不支持"
	if v.CPUPercentOneCore != nil {
		cpu = fmt.Sprintf("%.1f%%", *v.CPUPercentOneCore)
	}
	rss := "不支持"
	if v.Process.RSSBytes != nil {
		rss = fmt.Sprintf("%.1f MiB", float64(*v.Process.RSSBytes)/1048576)
	}
	fmt.Fprintf(out, "进程 CPU %s（100%%=占满1核）  实际驻留内存 %s  Go堆 %.1f MiB\n", cpu, rss, float64(v.HeapBytes)/1048576)
	if v.Process.HostMemoryAvailableBytes != nil && v.Process.HostMemoryTotalBytes != nil {
		fmt.Fprintf(out, "整机可用内存 %.0f / %.0f MiB\n", float64(*v.Process.HostMemoryAvailableBytes)/1048576, float64(*v.Process.HostMemoryTotalBytes)/1048576)
	}
	fmt.Fprintf(out, "接收 %.1f KiB/s  发送 %.1f KiB/s（%.3f Mbps）  转发 %.0f帧/s  大厅查询 %.1f次/s\n",
		v.ReceivedBytesPerSecond/1024, v.SentBytesPerSecond/1024, v.SentBytesPerSecond*8/1000000, v.ForwardedFramesPerSecond, v.DirectoryQueriesPerSecond)
	fmt.Fprintf(out, "累计收/发 %.2f / %.2f MiB  累计连接 %d  容量拒绝 %d\n队列 %d帧，单连接最高 %d/64；队列满断开 %d\n超时断开 %d  写失败 %d  格式错误 %d  控制频率超限 %d\n",
		float64(v.ReceivedBytesTotal)/1048576, float64(v.SentBytesTotal)/1048576, v.AcceptedTotal, v.CapacityRejectedTotal,
		v.QueuedFrames, v.MaxConnectionQueue, v.QueueDropsTotal, v.TimedOutTotal, v.WriteErrorsTotal, v.MalformedTotal, v.RateLimitedTotal)
	fmt.Fprintf(out, "P2P打洞：握手 %d  问候 %d  配对 %d  直连成功 %d  失败 %d\n",
		v.PunchCapableTotal, v.PunchHellosTotal, v.PunchOffersTotal, v.PunchDirectTotal, v.PunchFailedTotal)
	fmt.Fprintln(out, "流量只含中继TCP数据，不含IP/TCP头、重传；退出/重启后累计值清零。")
	return nil
}
