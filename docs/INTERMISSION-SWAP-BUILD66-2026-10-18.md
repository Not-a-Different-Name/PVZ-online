# Build66 关间换位：实现与验收

日期：2026-10-18。用户授权“开始实现且你来验收”。PC与Android源码已落地，本轮专项验收通过；未打包、发布或覆盖现用MOD63，真实玩家存档未修改。

## 玩家流程与规则

1. 全队过关，各自领完奖励并完成投票，进入下一关准备。第一关仍沿用大厅准备，战斗中不能换位。
2. 打开“席位与换位”，选择在线队友并申请，对方15秒内同意；拒绝、超时或离开即撤销。全队同一时间只处理一个申请，可多次互换。
3. 申请及换位使全队取消准备，卡牌保留。每人确认最终席位后重新准备。
4. 各端应用最终席位职责并保存，所有端确认成功后才放行。保存失败留在准备阶段，重试成功再继续。队友全部退出后，剩余玩家可独自完成准备。

草坪、植物生命/位置、阳光余额（无尽）、金币、增益、植物池、借用、伤害归属与房主身份跟随玩家。出怪倍数、接力顺序、末位小车职责跟随席位；BOSS固定1号位，不随房主移动。六人倍数仍为32/16/8/4/2/2，初始阳光加成为500/300/200/100/0/0；未改平衡数值。

闯关只替换已经计入的席位初始阳光差额一次；无尽不增减已有阳光。最终末位按玩家个人已消耗行记录恢复可用小车；移除非末位未用小车不计消耗、不发额外奖励，换回末位也不补个人已消耗行。席位或人数导致倍数变化时重新生成本关出怪，保存和恢复随机状态，不重建个人草坪、不重复发奖。

## 实现位置与兼容

- 双端 `Lawn/Online/PreparationWire.h`、`PreparationSession.inl`、`PreparationGame.inl`：固定字节编码、事务状态机和最终配置/保存。三个共享文件逐字节对齐。
- `NetSession`：稳定玩家令牌、关次、修订/请求号、名册确认、准备状态和重连恢复。状态依次为等待奖励、开放、待同意、提交、保存确认、开局。同修订的迟到状态不能把开局退回保存确认。
- `LawnApp`/Android `OnlineGame.cpp`、`Board`：接奖励结束与选卡等待、保留卡组、按最终席位调整出怪/阳光/小车、保存失败阻止开局。人数变化清除旧准备并重新确认。
- `OnlineReadyDialog`、`OnlineSeatDialog`、`RoomPresentation`：关间入口、申请者信息、15秒同意/拒绝、职责预览。修复等待框盖住换位页的问题。
- 双端联机MOD_BUILD=66；新消息25为92字节，协议仍1、开局44字节、检查点20。全体同版本才能打开新关间准备。未增加Board/Plant等原始序列化实体成员，保存格式不变。中继沿用既有交换协议，无服务端源码修改或部署。

实际中继只执行一次交换；遇到不确定提交，通过重连后的权威名册确认结果，不重复发送席位翻转。恢复沿用既有重连身份机制，本轮不是重连认证安全审计。

## 已完成验收

| 项目 | 结果与证据 |
|---|---|
| PC x86 Release | 编译/链接成功，[日志](../../tools/audit/preparation66-pc-build.log) |
| Android API24 arm64 | 原生编译/链接成功，[日志](../../tools/audit/preparation66-android-build.log)；来源版本检查通过 |
| 实际NetSession网络专项 | **50项，0失败**：[日志](../../tools/audit/preparation66-net-probe.log)。包含六人/双人直连、奖励门槛、双方同意、全队取消准备、房主换到末位、非房主互换、拒绝/超时/迟到/重复、跨关隔离、断线重连、漏提交/漏交换通知、过时保存确认状态、主动离开及仅剩一人 |
| 实际PC草坪与保存 | **22项，0失败**：[日志](../../tools/audit/preparation66-engine-probe.log)。验证闯关阳光差额仅一次、无尽777阳光保留、植物生命/位置/卡片冷却/随机状态、小车个人消耗、再回末位不补车、保存重载、保存失败阻止放行并可重试 |
| 实际Go中继 | **39项，0失败**：[日志](../../tools/audit/preparation66-live-net-probe.log)；`go test ./...`通过，[日志](../../tools/audit/preparation66-relay-go.log) |
| 5PC+1Android跨端 | **通过**：[PC](../../tools/audit/preparation66-cross-probe.log)、[Android](../../tools/audit/preparation66-android-peer.log)。PC与Android实际原生网络对象，经Go中继，房主由1换到6、Android由6换到1；Android延迟100帧配置确认，所有PC保持等待，确认后全队放行 |
| 既有修复回归 | 网络37项、PC引擎27项、植物加强27项通过：[网络](../../tools/audit/preparation66-regression-net.log)、[引擎](../../tools/audit/preparation66-regression-engine.log)、[强化](../../tools/audit/preparation66-regression-buffs.log) |
| PC真实资源界面 | 六人选择与待同意界面，均800×600原始客户区1:1；见下方截图和像素记录 |

网络专项使用实际C++会话，隔离中继注入断线/丢通知。草坪专项使用实际引擎与独立测试存档，其中小车美术定义为测试桩；界面截图另由真实资源游戏窗口取得。跨端是MuMu Android15 x86_64环境执行ARM64原生网络程序，未安装新APK，也不等同真机或Android完整游戏画面验收。

## 界面证据

截图均通过项目规定的 `tools/grab.ps1`，未缩放客户区。运行夹具使用单独测试档案66066。

- [六人选择页](../../tools/audit/preparation66-full-window.png)：本机金色框、目标蓝框、六席位倍数/阳光、接力顺序与房主标识清楚可见；等待框不覆盖页面。
- [收到申请页](../../tools/audit/preparation66-incoming.png)：显示申请者、同意/拒绝及重新准备提示。
- [像素检查](../../tools/audit/preparation66-ui-pixels.log)、[选择页采集](../../tools/audit/preparation66-full-window-capture.log)、[申请页采集](../../tools/audit/preparation66-incoming-capture.log)。六卡分别位于x=48/408、y=182/282/382，344×90；操作按钮在y=528、高46，均在800×600边界内。

## 验收边界与交接

本轮验收覆盖新增关间换位的网络状态、实际草坪保存与PC界面；尚未覆盖物理Android设备触控、六人真人完整长局、不同手机显示比例与APK首次安装。因此不把Android整体联机标记为已就绪，`onlineGameReady=false`保留。

未制作发布包、修改运行安装、卸载或清除原应用数据。以后发包仍必须PC完整可用包与Android完整APK，带资源/中文/依赖并保留存档；既有MOD63仍为当前安装。此次工作基于已有脏工作区，保留此前修改；改前文件在 `tools/audit/preparation66-before/`，本轮独立差异记录在 `preparation66-own-changes.diff`。本轮未提交或推送。
