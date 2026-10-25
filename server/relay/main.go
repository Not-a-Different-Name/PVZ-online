// @pvz-online: M3 Go 中继服务器入口。
// 用法：pvz-relay [-port 27777]
// 生产部署（阿里云 + 宝塔/Supervisor）见 server/relay/README.md。
package main

import (
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func main() {
	port := flag.Int("port", 27777, "TCP listen port")
	private := flag.Bool("private", false, "player-hosted six-seat relay; no public directory or admin listener")
	readyFile := flag.String("ready-file", "", "local helper readiness file, written only after successful bind")
	admin := flag.String("admin", defaultAdminAddress, "local-only status HTTP address; empty disables status")
	configPath := flag.String("config", "", "operations JSON config; validated changes reload within one second")
	status := flag.Bool("status", false, "print running relay status and exit (does not start a server)")
	statusJSON := flag.Bool("status-json", false, "print running relay status as JSON and exit")
	version := flag.Bool("version", false, "print relay release and exit")
	flag.Parse()
	if *version {
		fmt.Println(relayRelease)
		return
	}
	if *status || *statusJSON {
		if err := printStatus(*admin, *statusJSON, os.Stdout); err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		return
	}

	logger := log.New(os.Stdout, "", log.LstdFlags)
	if *private {
		host, err := startHostedRelay(fmt.Sprintf(":%d", *port), true, logger.Printf)
		if err != nil {
			logger.Fatal(err)
		}
		defer host.Close()
		if *readyFile != "" {
			if err := os.WriteFile(*readyFile, []byte(fmt.Sprint(host.server.gamePort)), 0600); err != nil {
				logger.Fatal(err)
			}
			defer os.Remove(*readyFile)
		}
		logger.Printf("player relay listening on %s", host.listener.Addr())
		stopped := make(chan os.Signal, 1)
		signal.Notify(stopped, os.Interrupt, syscall.SIGTERM)
		defer signal.Stop(stopped)
		ticker := time.NewTicker(100 * time.Millisecond)
		defer ticker.Stop()
		for {
			select {
			case <-stopped:
				return
			case <-ticker.C:
				if *readyFile != "" {
					if bytes, err := os.ReadFile(*readyFile); err == nil && string(bytes) == "stop" {
						return
					}
				}
			}
		}
	}
	srv := newServer(func(format string, args ...any) { logger.Printf(format, args...) })
	srv.gamePort = *port
	if *configPath != "" {
		if err := srv.reloadOperations(*configPath); err != nil {
			logger.Fatalf("config: %v", err)
		}
	}
	srv.startSampling(*configPath)
	defer srv.stopSampling()
	go srv.sweepLoop()
	// P2P 打洞监听是冷配置：端口取启动时的运营配置，热改不重绑，重启生效。
	if ops := srv.operations().Config; ops.PunchEnable {
		if err := srv.startPunchUDP(ops.PunchUDPPort); err != nil {
			logger.Fatalf("punch udp: %v", err)
		}
		go srv.punchSweepLoop()
		logger.Printf("punch udp listening on :%d", ops.PunchUDPPort)
	}

	ln, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		logger.Fatalf("listen on :%d failed: %v", *port, err)
	}
	if *admin != "" {
		if _, err := srv.startStatus(*admin); err != nil {
			logger.Fatalf("status listener failed: %v", err)
		}
		logger.Printf("local status: http://%s/ | release %s", *admin, relayRelease)
	}
	logger.Printf("pvz relay listening on :%d (protocol v%d, max %d players, idle %v, hold %v)",
		*port, protocolVersion, maxPlayers, srv.idleTimeout, srv.holdDuration)

	connections := make(chan struct{}, connectionLimit)
	for {
		nc, err := ln.Accept()
		if err != nil {
			logger.Printf("accept failed: %v", err)
			continue
		}
		select {
		case connections <- struct{}{}:
			c := newConn(srv, nc)
			go func() { defer func() { <-connections }(); c.run() }()
		default:
			srv.stats.capacityRejected.Add(1)
			nc.Close()
		}
	}
}
