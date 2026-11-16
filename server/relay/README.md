# PvZ Online 中继服务器（server/relay）

> 2026-10-20 首批运维版源码已接入：负载监测趋势、自动分流所需的容量 v2、签名动态服务器清单及配置即时生效。完整配置、加服、兼容与首次部署边界见 [OPERATIONS.md](OPERATIONS.md)。是否已经安装到公网，以运行中的 `/status` 版本为准。

> Go 写的房间 + 事件中继：2–6 个客户端各自连上来，服务器**按席位转发游戏帧**。
> 零依赖（只用标准库）、单进程、状态全在内存（无数据库、无落盘）。
> 线格式的单一事实源是客户端的 `../../Lawn/Online/NetProtocol.h`——本目录 `proto.go`
> 是它的 Go 镜像，两边由 Go 的 `golden_test.go` 与客户端侧 `tools/nettest/proto_golden`
> 用同一组黄金向量对钉。
> 服务器按 `src`/`dst` 路由；另核验开局者、在线人数和等候下一关的席位，不模拟草坪。

## 单人入口与服务器房间分离（2026-10-19）

容量扩展版 `2026-10-19-room-min2-rooms256` 将房间上限提高到 256，保留 512 连接与每房 6 人限制。目录查询 v2 可枚举全部 256 间；旧目录 v1 仍只返回前 128 间并正常结束。新版 PC / Android 大厅遇到旧服务器会回退到 v1；游戏 Build 不变。完整行为、验证与上线限制见[256 房间记录](../../docs/ROOMS256-2026-10-19.md)。下文 128 房压测和旧版本说明为历史记录。

版本 `2026-10-19-room-min2`：允许一人建立等待大厅，但 START_LEVEL 和 RUN_GO 只允许房主在至少两名实际在线玩家时发送。离线保留席、主动离开及已被替换的连接不计人数。等候下一关的席位不计当前战斗人数，但可参加新的 START_LEVEL，避免下一关无法开局。消息27由房主同步下一关等候名单，新开局清空名单；换位同步调整名单。

PC/Android MOD81 提供独立的“单人与局域网游戏”和“服务器房间”页面。客户端亦做开局限制，失去最后一名队友时保留并冻结草坪；失败连接可明确退出，不自动改为单人。服务端规则需要部署本次中继程序才能在公网生效，客户端构建不会自动替换公网服务器。

## 本机状态版（2026-10-18，独立于客户端Build）

运行参数仍可沿用`-port 97`。新增默认本机状态地址`127.0.0.1:27778`，不需要新增公网端口；使用`-admin ''`可以关闭，非回环监听地址会拒绝启动。

```bash
cd /www/server/pvz-relay
./pvz-relay -status
./pvz-relay -status-json
watch -n 2 './pvz-relay -status'
```

`-status`读取正在运行的新服务后退出，不会启动第二个游戏中继。`-version`打印磁盘程序版本，当前`2026-10-19-room-min2-rooms256`；真正运行版本以`-status`输出为准。仅本机HTTP提供`/`状态页、`/status` JSON、`/healthz`进程存活检查。网页每5秒刷新；统计缓存每秒采样。`/healthz`表示状态HTTP进程存活，不替代公网97端口或联机功能检查。

状态含连接/玩家/房间、公开待开局和已开局房间、保留席、CPU（100%=一核）、Linux当前RSS、Go堆、整机可用内存、游戏流收发速率/总量、大厅查询、转发、队列满/超时/格式错误等。只统计中继应用流，不含IP/TCP头、ACK、重传或本机状态HTTP；重启清零。状态按秒采样，瞬时变更可能等约1秒再显示；Go保留内存不是RSS。非Linux开发机没有OS资源时字段为null，不能据此当零占用。

状态不提供房主名字、房间码、IP或随机凭据；无数据库、文件历史或第三方监控依赖。两个Linux架构的完整程序、上传与回退说明位于工作区根`dist/PvZ-Relay-Status-20261018/`；服务器可直接上传替换，游戏客户端无需因这项改动升Build。当前本机测试不等于公网已部署。

真实Linux程序短测覆盖6/60/120/300/480玩家模拟通信、观战及查询，连接512和房间128的保护阈值也已检查。压测是本地Ryzen单核绑核传输，不能当成阿里云CPU或公网实战保证。完整方法、数值和用户实际2核/1.6GiB配置估计见[负载记录](../../docs/RELAY-STATUS-LOAD-2026-10-18.md)。

## Build68更新（2026-10-18，本地已验，尚未部署）

新增`directory.go`：F01A/F00D公开状态及确认、F01B/F00C分页目录、F01C目录加入。同锁重查公开/未开局/门禁/版本/空位，START_LEVEL转发前撤下列表。仅显示开局前公开房间；关间补位继续用原F012房间码，不公开到大厅。目录6条/页，载荷≤152字节、含帧头≤156字节，回应后关闭查询连接；无数据库或后台轮询。历史记录仅在客户端保存。

Go竞态检测及目录专项、PC与Android原生网络检查通过；容量上限仍沿用67，不等同实战承载验收。完整行为与未验边界见[Build68记录](../../docs/LOBBY-HISTORY-BUILD68-2026-10-18.md)。需更新中继及68客户端；本批未部署公网。

## Build67更新（2026-10-18，本地已验，尚未部署）

新增 `admission.go`：无尽准备期房主门禁/确认、16字节随机重连凭据、取消未完成补位。门禁与JOIN同锁排序；关闭后仍允许原席位凭据重连。Build67房间只接收同构建新人，旧中继无法启用新补位流程。游戏消息26仍是不解析的透传负载。

控制帧新增F017/F00A（门禁请求9字节/确认10字节）、F00B（凭据16字节）、F018（REJOIN25+16字节）、F019（取消席位1字节），拒绝原因8为关门、9为构建不匹配。Build67的REJOIN必须带随机凭据，不能凭同名抢占；凭据仅保存在进程内存，服务器重启会散房。

面向1GB实例增加128房、512连接上限；未入房连接总寿命10秒、控制帧每秒32条，沿用64帧发送队列和256字节负载。无需数据库或草坪持久化。上限用于约束内存，尚未测满容量。`go test -race ./...`及5PC+1Android原生网络补位本地检查通过；公网服务器本批未改。更新仍按下方原有部署流程备份旧程序并维护重启，重启会清空当时房间。

客户端/状态协议与验收范围见 [`../../docs/ENDLESS-ADMISSION-BUILD67-2026-10-18.md`](../../docs/ENDLESS-ADMISSION-BUILD67-2026-10-18.md)。

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

**2026-10-18 Build68上传说明**：新版Linux amd64/arm64程序、校验文件和完整宝塔操作步骤已准备在工作区根`dist/PvZ-Relay-Build68-20261018/`。优先按其中`宝塔上传与更新说明.md`操作：上传有独立名称的新文件，校验后备份旧程序，使用新文件改名替换，再重启原服务。首次安装和回退步骤也在该说明中；公网未由本地构建自动更新。

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
- **升级**：上传为独立的新文件，核对校验和；将旧`pvz-relay`备份为带时间的文件，再用新文件改名替换原路径、`systemctl restart pvz-relay`。不要直接覆盖运行中的文件。回滚也使用备份复制到临时文件、改名替换后再restart。升级和回退同样会踢掉所有房间。
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

- **无账号登录**：新队员使用房间码或公开目录，受公开/开局/门禁/版本/容量检查；安全版本原席位重连还须匹配随机凭据。每连接控制帧32次/秒，连接总上限512；没有全局游戏帧吞吐限速。
- **无 TLS**：裸 TCP 明文——流过的只有游戏事件（位置/波次/名册），没有账号凭据类数据。
- **无优雅退出**：`stop` 即退，没有落盘状态可丢。
- **单进程单机**：本机Linux状态版空载约7MiB当前RSS，480模拟玩家密集通信短测约16MiB。RSS不含内核套接字和日志等开销；实际容量还受公网带宽及整机其他任务影响，不沿用旧版1–2MB说法。
- 控制帧载荷不合法（长度/字段越界）**断连**处理；未知控制帧记日志后丢弃。


## MOD114 fixed-preset matchmaking

The relay release `2026-11-16-matchmaking2` adds control type `0xF026` and directory v5. Directory v1/v2/v4 and older gameplay builds retain their existing layouts. The START frame remains 56 bytes for MOD113+; matching requires MOD114+ and the same build in each room.

Add `"match_node_id": "1"` (or that machine's actual numeric catalog ID) to operations.json and increment its revision. Keep the signed node catalog, endpoints, service identity, bandwidth settings and local admin listener. Every create/directory node participating in matching must run this release and use the same signed catalog revision. The lowest numeric enabled create/directory ID is the coordinator. Appending more catalog nodes requires no client patch. Coordinator reassignment is an operator catalog change, not a client-side automatic fallback.

The coordinator queries bounded metadata and reserves one destination seat with an unpredictable 12-second token. Network work never holds the room mutex; commit/cancel/gate/START are serialized with ordinary joins. Requests wait at most two seconds for the coordinator, then return a retryable busy result. Mode/build/preset pools remain separate. A missing node or catalog disagreement fails matching closed; ordinary rooms and code/lobby joins continue independently. Temporarily retire an unavailable node from matching using the signed create/directory flags before expanding or maintaining the cluster. Do not rewrite an existing catalog ID to a new endpoint.

Matching uses process-local bounded room state and rate maps; no database, gameplay proxy, or polling daemon is added. Restarting the relay still disconnects that node's players. Back up both binary and operations.json before switching; rollback restores both, including the prior config revision and removal of the new optional field.
