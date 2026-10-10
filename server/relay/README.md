# PvZ Online 中继服务器（server/relay）

> Go 写的房间 + 事件中继：2–6 个客户端各自连上来，服务器**按席位转发游戏帧**。
> 零依赖（只用标准库）、单进程、状态全在内存（无数据库、无落盘）。
> 线格式的单一事实源是客户端的 `../../Lawn/Online/NetProtocol.h`——本目录 `proto.go`
> 是它的 Go 镜像，两边由 Go 的 `golden_test.go` 与客户端侧 `tools/nettest/proto_golden`
> 用同一组黄金向量对钉。
> 服务器只读每帧的 `src`/`dst` 两个字节做路由，**不解析游戏帧内容**——玩法语义全在客户端。

## 目录

| 文件 | 职责 |
|---|---|
| `main.go` | 入口：`-port` 参数、监听、accept 循环、过期保留清扫循环 |
| `room.go` | 房间与分发：建房 / 加入 / 重连受理 / 席位保留与清扫 / 换位落实 / 转发校验 / 退房回收 / 散房 |
| `conn.go` | 单连接：读循环、写循环（唯一写手）、1s PING 与闲死判定 |
| `proto.go` | 线格式常量与全部编解码 |
| `relay_test.go` / `golden_test.go` | 服务端契约测试 / 协议黄金向量 |

## 构建

需要 Go 1.21+。Linux 服务器上直接构建：

```
cd server/relay
go build -trimpath -ldflags="-s -w" -o pvz-relay .
```

Windows 开发机交叉编译 Linux 版（产物可拷到任何 x86-64 发行版）：

```
GOOS=linux GOARCH=amd64 CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o pvz-relay .
```

跑测试：`go test ./...`（契约 + 黄金向量都在里面）。

## 部署（阿里云 + 宝塔，2026-10-03 实际走通的流程）

1. **落盘** `/www/server/pvz-relay/pvz-relay`（宝塔面板 → 文件 → 上传 → 权限改 **755**）。
   - **别放 `/www/wwwroot`**：那里是站点目录，nginx 会把二进制当静态文件供人下载。
   - 宝塔"上传"默认权限 644，不改 755 服务起不来（systemd 报 203/EXEC）。
   - 上传后在服务器上 `sha256sum pvz-relay`，与开发机构建产物的哈希核对一致再往下走。
2. **试跑**：`./pvz-relay -port 97` → 看到 `pvz relay listening on :97 ...` 即成功，
   **Ctrl+C 停掉再装服务**（不停掉的话 97 被占，systemd 起不来）。
3. **装 systemd 常驻**（整段粘贴）：

   ```bash
   cat > /etc/systemd/system/pvz-relay.service <<'EOF'
   [Unit]
   Description=PvZ Online Relay Server
   After=network-online.target
   Wants=network-online.target

   [Service]
   Type=simple
   WorkingDirectory=/www/server/pvz-relay
   ExecStart=/www/server/pvz-relay/pvz-relay -port 97
   Restart=always
   RestartSec=3
   LimitNOFILE=8192

   [Install]
   WantedBy=multi-user.target
   EOF

   systemctl daemon-reload
   systemctl enable --now pvz-relay
   ```

4. **防火墙三层，缺一不可**：

   | 层 | 操作 |
   |---|---|
   | 系统 | 先看谁在管：`ufw status` / `firewall-cmd --list-ports`。ufw：`ufw allow 97/tcp`；firewalld：`firewall-cmd --permanent --add-port=97/tcp && firewall-cmd --reload`。**ufw 按顺序匹配——若有显式 `97/tcp DENY` 规则要先 `ufw delete <编号>` 再放行** |
   | 宝塔 | 面板 → 安全 → 放行端口 `97`，协议 TCP |
   | 阿里云 | 控制台 → ECS → 这台实例 → 安全组 → 入方向 → 手动添加 TCP `97/97`，源 `0.0.0.0/0`——**最常见的漏项**：前两层开了这层没开照样连不上 |

5. **验证**（开发机侧，工具在 `tools/nettest/`，仓库外）：
   `Test-NetConnection <云IP> -Port 97` 通 → `fake_server_smoke.exe --host <云IP> --port 97`
   → `REAL RELAY: ALL PASS (0 failures)`。

   注：97 是特权端口（<1024），systemd 以 root 跑没问题；将来若想降权跑普通用户，
   需换 >1024 的口或给 `CAP_NET_BIND_SERVICE`。

## 运维

- 状态 / 日志：`systemctl status pvz-relay`、`journalctl -u pvz-relay -f`
  （建房、加入、换位、超时踢人、连接来自哪，全在日志里；成功有日志、被拒的帧只记丢弃原因）
- 重启：`systemctl restart pvz-relay`——**所有房间清空**（状态在内存里），挑没人的时候做
- **升级**：先 `cp pvz-relay pvz-relay.bak`，再上传新文件、`systemctl restart pvz-relay`；
  回滚 = 换回 `.bak` 再 restart。升级同样会踢掉所有房间。
- **端口被占起不来**（日志 `bind: address already in use`）：`ss -lntp | grep ':97 '` 看占用者，
  常见是"试跑没停掉的进程"：`pkill -f pvz-relay` 后 `systemctl start pvz-relay`。

## 行为常量（改这里要同步客户端）

| 常量 | 值 | 位置 | 客户端侧对应 |
|---|---|---|---|
| 监听端口 | `-port`，默认 27777；云上 97 | `main.go` | `NetProtocol.h`：`DEFAULT_RELAY_PORT`（97）/ 直连 `DEFAULT_PORT`（27777） |
| 协议版本 | 1 | `proto.go` | `PROTOCOL_VERSION`——对不上回 `REJECT_PROTOCOL_VERSION` |
| 席位上限 | 6 | `proto.go` | `MAX_PLAYERS` |
| 房间码 | 4 字符，字母表 `ABCDEFGHJKLMNPQRSTUVWXYZ23456789`（无 I/O/0/1，32 个），加入时不区分大小写、服务器保证在服内唯一 | `proto.go` | `ROOM_CODE_LEN` |
| PING / 闲死 | 进过房的连接每 1s 发 `SRV_PING`；**30s** 收不到它的任何数据即断，席位进保留（广播 `PEER_OFFLINE`） | `conn.go` / `room.go` | 客户端回 `PONG` |
| 席位保留 | 非体面断开（掉线/超时）的席位保留 **60s**：本人 `REJOIN`（名字+构建对得上）坐回原席；普通 `JOIN` 跳过保留位，但名字+构建对得上也可以坐回（换进程重进房）；保留过期 = 广播 `PEER_LEAVE`(timeout)；房主的保留过期 = 散房 | `room.go` | 重连窗口 45s（客户端先放弃，不会撞上已释放的席位） |
| REJOIN 受理 | 快路径（旧连接还挂着）：静默换掉旧连接，只广播 `PEER_BACK`，队友无感；保留路径：核对名字+构建、消费保留、回 `WELCOME`、广播 `PEER_BACK`。身份对不上回 `REJECT_REJOIN_SEAT_MISMATCH`(6)；房间没了/席位已释放回 `REJECT_REJOIN_UNAVAILABLE`(7) | `room.go` | `MSG_CLI_REJOIN` = `0xF016` |
| 写超时 / 写队列 | 单帧写 10s 超时；写队列 64 帧满 = 客户端卡死，断掉 | `conn.go` / `room.go` | — |
| 转发校验 | 游戏帧 `payload[0]`（src）必须等于连接的当前席位（防冒名），`payload[1]`（dst）必须在 1..6 且非自己、非空位；不合法丢帧不断线（保留期的席位是"空位"，丢帧是正确行为） | `room.go` | 线格式见 `NetProtocol.h` 头部注释 |

## 已知限制（v1 有意为之）

- **无鉴权**：房间码即门票（32⁴ ≈ 100 万种，服务器保证在服内唯一）；连接与猜码无速率限制。
- **无 TLS**：裸 TCP 明文——流过的只有游戏事件（位置/波次/名册），没有账号凭据类数据。
- **无优雅退出**：`stop` 即退，没有落盘状态可丢。
- **单进程单机**：实测常驻约 1–2 MB（`systemctl status` 的 Memory 行），1G 内存机器富余。
- 控制帧载荷不合法（长度/字段越界）**断连**处理；未知控制帧记日志后丢弃。
