# MOD110 CLI（JSON Lines，协议 1）

供脚本或 AI 操作的 Windows 客户端，复用 MOD110 R1 的真实原生游戏对象，并包含失败重开阳光恢复顺序及重复建场修复。版本编号仍为 MOD110。它不是另写的战斗模拟器。

## 启动

解压完整包，在终端运行（无需另装 Python）：

```powershell
.\pvz-cli.exe
```

源码入口也可用 Python 3.10+ 启动：

```powershell
python .\pvz-cli.py
```

启动后输出一行 `{"event":"ready","ok":true,"state":{...},"backup":...}`。此后每行输入一个 JSON 对象，每个请求恰有一个响应。可传 `id`，响应原样带回。stdout 只写 JSON；诊断写数据目录的 `cli-engine.log`。启动及单次命令默认超时 180 秒，可用 `--timeout` 调整。

```json
{"id":1,"cmd":"new_run","mode":0,"difficulty":1}
{"id":2,"cmd":"step","frames":10}
{"id":3,"cmd":"state"}
{"id":4,"cmd":"choice","index":0}
{"id":5,"cmd":"step","frames":1000}
{"id":6,"cmd":"save"}
{"id":7,"cmd":"quit"}
```

前面的选项只是格式示例；每一步应根据返回状态决定。`example_agent.py` 给出可直接运行的状态驱动示例。

## 与图形版共用数据

默认使用包内 `cli-data`，可指定任意原生数据根目录：

```powershell
.\pvz-cli.exe --data-root "D:\PvZData"
# 上述目录中放 userdata 子目录，而不是直接指向 userdata。
```

使用当前电脑正式 MOD110 数据：

```powershell
.\pvz-cli.exe --shared-data
# 等同于 --data-root "$env:ProgramData\PlantsVsZombies"
```

此时沿用原 `users.dat`、`user*.dat`、`run*.dat`、新无尽独立槽及备份格式；不把原生存档转换成 JSON。CLI 和图形版退出后可以轮流使用同一份数据。每次启动都会先将现有 `userdata` 备份到数据根目录的 `cli-backups/时间-进程号/userdata`，ready 响应给出备份路径。保留所有原档案，默认载入原生列表中的首个档案，可用 `profile` 按 ID 切换。

正式数据模式持有与正常图形版相同的进程互斥量：已有游戏运行时拒绝启动；CLI 运行时正常图形版也不能再打开。另有按数据目录加的 CLI 写锁。不要用调试多开、其他修改版或外部存档编辑器绕过互斥。其他历史版本是否兼容，仍由其原生加载器及存档版本决定；不保证旧版能读取新版存档。安卓原生草坪快照也不能直接当作 PC 草坪载入。

`saves` 显示当前档案的无尽槽。`continue` 的 `slot` 可传十进制字符串，避免 JavaScript 的 64 位整数精度损失；省略/0 表示所选场景最近的可用槽。`saves`/续档沿用图形版的旧档迁移与备份逻辑。`save` 对普通闯关保存检查点，对新无尽保存原生草坪；普通闯关不会因此获得原版没有的中途草坪续玩。

## 命令

索引均从 **0** 开始。只接受表中字段及可选 `id`，布尔值不能代替整数。

| cmd | 字段 | 行为 |
|---|---|---|
| `state` | 无 | 查询完整状态，不推进时间 |
| `new_run` | `mode=0..3`，`difficulty=0..2`，`scene=0..4` | 按原入口新开完整版/普通/快速/无尽；默认 0/1/0 |
| `continue` | `mode`、`scene`、`slot=0` | 按原入口继续当前档案的检查点/无尽槽 |
| `saves` | 无 | 枚举当前档案可用无尽槽 |
| `profile` | `profile_id` | 菜单中切换原生档案；列表在 `state.profiles` |
| `choice` | `index=0..2` 或 `-1` | 实际三选一；-1 放弃当前屏 |
| `seed` | `seed=0..47` | 实际选卡/取消；仅在可交互选卡页生效 |
| `start` | 无 | 十卡选满且飞行动画结束后按原“开始”入口 |
| `step` | `frames=0..10000`，`delay_ms=0..10` | 推进指定应用帧；每帧可附加真实等待 |
| `plant` | `slot=0..9`、`row=0..5`、`col=0..8` | 通过真实卡槽和草坪输入种植，执行费用/冷却/场景规则 |
| `collect` | `coin_id` | 实际点击当前掉落物；ID 来自 `board.coins` |
| `click` | `x=0..799`、`y=0..599`、`button=1/-1` | 游戏内左/右点击，可操作模态框、推车/工具/团队入口 |
| `key` | `key=1..254` | 一次游戏内按下/抬起，使用原 Win32 虚拟键码 |
| `host` | `port=27888` | 原协议直连房主 |
| `join` | `server="127.0.0.1"`、`port=27888` | 原协议直连加入 |
| `room_host` | `server="127.0.0.1"`、`port=97` | 在指定中继创建房间 |
| `room_join` | `server`、`port=97`、`room` | 加入指定中继房间 |
| `disconnect` | 无 | 菜单中断开网络 |
| `save` | 无 | 调用原生保存入口 |
| `menu` | 无 | 按正常退关链返回主菜单 |
| `screenshot` | 无 | 输出当前原生画面的 PNG 至数据目录 `screenshots/帧号.png` |
| `quit` | 无 | 正常关闭、释放数据锁；不等同于 `save`，重要进度应先保存 |

`state.scene`：1 菜单、2 开场/准备、3 战斗；其他值沿用 MOD110 原枚举。`run` 包含当前关、卡池、持有增益和欠选次数；`choice` 包含当前三张实际候选；`chooser.interactive` 才表示能够点选卡片。小卡池可能自动填槽，存在过场选卡对象也不代表需要操作。

`board` 提供实际阳光、波次、暂停/通关、卡槽费用/冷却、植物、僵尸和掉落物 ID。`dialogs` 提供对话框，`controls` 提供当前可见按钮和团队自绘控件的屏幕矩形。请选择 `enabled=true` 的控件，必要时先 `step` 等待原按钮延迟/动画，再点击矩形中心。`ok:true` 表示输入请求已交给原入口；是否产生目标结果仍应检查返回状态（例如被模态框拦住的坐标点击）。拒绝的语义操作可能已走过一帧，不能假设错误响应具有回滚语义。

## 同步对话框与联机时钟

原游戏部分警告/商店会在输入回调内同步等待。CLI 此时返回 `{"ok":true,"pending":true,"state":...}`，脚本下一条可以查状态、推进按钮延迟或点击确认/取消。原调用会在真实对话框解决后继续，不产生额外无请求响应；退出/输入 EOF 也会解除等待。不会替 AI 自动确认消费或选择。

初始化完成后默认由请求驱动：`state` 和空闲不会推进模拟，`step` 每次执行一个原应用帧（正常速度名义 10ms），普通输入命令末尾再执行一帧。原游戏的随机盐、挂钟、网络重连计时不保证跨进程逐比特确定。

联机仍使用原协议和真实网络线程；脚本必须持续驱动 `step`。建议两端各自循环 `{"cmd":"step","frames":100,"delay_ms":10}`，避免空闲导致心跳超时，也避免模拟速度远超队友。只在需要时显式建房/加入，不自动接入官方服。

## 平台与验收边界

目前为 Windows 10/11 桌面端。原引擎需要 Win32 窗口、DirectDraw 与资源，CLI 将其置于从不切到前台的独立桌面，首次草坪绘制在内存中完成；它不是已移除所有图形依赖的 Linux 服务器程序。音乐保留原 BASS 对象、音量为零，避免原占位音频路径的空对象访问。

完整包包含原生引擎、封装后的命令行入口、运行库和完整资源。原源码未修改协议、存档格式或玩法规则；GUI/MuMu 正式客户端不因本包自动更新。

构建与验证记录见仓库 `work/cli-mod110-20261103/REPORT.md`。保留早期编译、无声音频崩溃、测试时序/夹具及独立槽问题记录；不将受控 CLI 验证宣传为物理手机、公网多人自然长局或全部旧故障根治。

## 开发构建

在已有 MOD110 原生构建依赖（VS2022 x86、CMake/Ninja、相邻 third_party）可用的开发环境中，用单独构建目录和 `CMAKE_PROJECT_INCLUDE` 接入 `cli.cmake`。它仅替换 LawnProject 的 main.cpp 和控制台入口属性，不改变默认图形版构建：

```powershell
cmake -S re-plants-vs-zombies -B build-cli -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PROJECT_INCLUDE="$PWD/tools/cli/cli.cmake"
cmake --build build-cli --target LawnProject
```

`build.py` 是本工作区对固定 R1 对象、当前 TeamPlay/LawnApp 修复及 CLI 入口的增量构建取证工具，依赖记录中的历史构建缓存；不能当作全新检出的通用构建脚本。`package.py` 将 Python 前端封装为独立 exe，并按白名单复制完整资源。交付记录区分实际增量链接与全量 clean 构建。
