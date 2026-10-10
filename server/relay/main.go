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
)

func main() {
	port := flag.Int("port", 27777, "TCP listen port")
	flag.Parse()

	logger := log.New(os.Stdout, "", log.LstdFlags)
	srv := newServer(func(format string, args ...any) { logger.Printf(format, args...) })
	go srv.sweepLoop()

	ln, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		logger.Fatalf("listen on :%d failed: %v", *port, err)
	}
	logger.Printf("pvz relay listening on :%d (protocol v%d, max %d players, idle %v, hold %v)",
		*port, protocolVersion, maxPlayers, srv.idleTimeout, srv.holdDuration)

	for {
		nc, err := ln.Accept()
		if err != nil {
			logger.Printf("accept failed: %v", err)
			continue
		}
		c := newConn(srv, nc)
		logger.Printf("connection from %s", c.remoteAddr())
		go c.run()
	}
}
