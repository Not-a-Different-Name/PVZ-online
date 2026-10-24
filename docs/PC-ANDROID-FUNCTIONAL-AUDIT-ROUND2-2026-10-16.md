# PC / Android 第二轮功能审计

2026-10-16修复进展：F13–F18及相关F01/F02已完成源码修复与本批定点检查，详见[修复交接](PARITY-ROUND2-FIX-STATUS.md)。下文保留发现时的证据和历史状态；APK/设备状态以修复交接为准。

2026-10-16。承接[第一轮审计](PC-ANDROID-FUNCTIONAL-AUDIT-2026-10-16.md)，只找新增问题和收敛疑点，没有修改游戏代码、安装 APK 或操作玩家存档。本轮没有截图；用源码调用链及六项隔离检查取证，不重复全量测试。

**本轮新增五项有证据的缺陷/持久化缺口，以及一项已确认缓存现象、尚未复现完整玩法后果的风险。它们都涉及双端共同实现，不能作为手机漏迁移独有问题统计。**

## 探查范围与证据边界

依次检查奖励选择中保存、付费重摇的钱包、无尽保留植物、断线席位赠送、借用取消、下一关准备状态；同时追到框架失焦处理，复核第一轮 R03。

- PC 测试使用 `5224048366493432daab49c1238a9062327d9f0c` 的冻结头文件，以及隔离复制的现有 Release 对象文件；对象不是本轮从整份源码重新编译的发行包。具体文件 SHA256 见证据清单。
- Android 源码核对时 HEAD 为 `ed8fb76`。两端工作区还有键位/血量、植物候选等其他工作，未覆盖、回退或代提交。这里的结论不代表这些工作已经构建进用户正在玩的程序。
- 六项检查调用真实 PC 游戏实现，其中赠送与准备状态使用真实 NetSession 和本机 Go 中继；保留植物使用最小棋盘/植物状态夹具，取消使用受控事务状态，不冒充完整游戏场景。
- Android 对应调用链已静态确认；本轮没有在 MuMu 复现这六项。断线赠送用本机中继制造精确的保留席位状态，没有把本机结果说成云端实测或云端稳定性问题。
- 工作区中途有 `RunState` 结构布局改动，最初探针与旧对象不匹配并在结束时崩溃；`results.log` / `results2.log` 均作废。冻结头文件与对象后重新验证，种子/关序号校验正常，六项输出符合预期，进程退出码 **0**。不能引用作废结果。

## 新增问题

| 编号 / 优先级 | 问题与影响 | 证据层级 |
|---|---|---|
| F13 / P1 | **选择阶段付费重摇后，读检查点会失去尚欠奖励。** Save 保存成长结果但没有待选植物/buff数量、当前候选、选择阶段和本屏重摇次数；Resume/Continue不补回 | 真实保存/读档调用：原有4次植物+2次buff待选，重摇保存后新对象读取为0+0；重摇次数1→0。确认第一轮 S01，不是再次统计 F02 的“未保存回旧局” |
| F14 / P2 | **重摇写闯关检查点，却没有同时持久化钱包扣款。** 退出前崩溃/进程被杀时，两份文件可能不是同一笔交易的状态 | 隔离档案先保存 `mCoins=1000`，调用真实付费重摇后内存990、重新读取用户文件仍1000。正常返回菜单会写钱包；未宣称每次正常退出都不扣钱，也不宣称已证明一套可重复刷币流程 |
| F15 / P1 | **无尽保留下来的植物未获得新选的通用生命增益。** 新种植物和旧植物实际最大生命不一致 | 调用真实 RunPickChosen 选择一层 Rooted 后，既有向日葵最大生命仍300；当前规则的 CoopMaxHealth 应为525。两端选择后刷新只处理首次大嘴花/钢南瓜特殊变更及土豆雷计时，没有通用 Rooted/生命升级的同步 |
| F16 / P1 | **向已经断线、席位仍保留的队友送阳光会扣款且提示已发送，实际未送达。** 旧赠送没有到账回执 | 两个真实会话＋原始连接制造断线：席位occupied=1、offline=1；TrySendSunGift返回true，阳光500→400，冷却300；中继同时记录 `dropped 0x13 to empty seat 2`。这条是G/手机赠送入口使用的旧链路，不是有回执的求援响应链路 |
| F17 / P2 | **借用 OFFER 已发送、尚未确认时，CANCEL 不释放借出名额。** 若随后没有匹配的 DECLINE（例如断链导致后续回复未到），本关可能一直不能再次借出 | 受控事务：lentPeer/request匹配、lentCommitted=false、mLentLevel=当前关；处理匹配CANCEL后仍占用。源码中Answer发OFFER即Save占位，只有DECLINE回滚，Update断线清理也不释放。未模拟整条断网/OFFER/CANCEL/重连实战；正常可靠连接下迟到OFFER会触发DECLINE，不能说所有取消都会卡住 |
| F18 / P1风险 | **新 START 不清上一轮准备缓存。** 在新一轮false到达前，房主查询仍认为该席已准备 | 两个真实NetSession：上一轮true→SendStartLevel下一关→继续收发后仍true。已证实缓存现象；完整游戏通常会在NewGame/TryKeepEndlessLawn发false，是否能造成提前开场尚未复现，因此不写“等待门已失效” |

### 源码定位

定位以下面文件及函数为准，其他工作仍可能移动行号。

- **F13/F14**：PC `LawnApp.cpp::RunPickPaidReroll`、`ContinueRun`，Android `src/Lawn/Run/RunFlow.cpp:56/114`；两端 `RunState::Save/Load`、`PlayerInfo::AddCoins/SaveDetails`。Android Load末尾还明确清 `mPaidRerollCount`。探针中的钱数为存档内部 `mCoins` 单位，不能直接当界面显示金额。
- **F15**：PC `LawnApp.cpp::RunPickChosen`，Android `src/Lawn/Run/RunFlow.cpp:317` 的 `RefreshRunChoicePlants`；两端 `Plant::PlantInitialize/CoopMaxHealth`。Android `src/Lawn/OnlineGame.cpp::TryKeepEndlessLawn/ReleaseRunIntro` 明确保留实体；这里只确认Rooted例子，其他初始化型词条列为后续定点项，未宣称逐条复现。
- **F16**：PC `Board.cpp::TrySendSunGift`，Android `src/Lawn/BoardCoop.cpp:208`；两端 `Lawn/Online/NetSession.cpp::SendSunGift/Dispatch`；PC `server/relay/room.go:323` 的forward只找bySeat，断线保留另存在holds，不缓存赠送。客户端只检查occupied，未排除offline；向中继写成功不等于对方收到。
- **F17**：PC `Lawn/CoopPlay.cpp::Answer/Handle/Update`，Android `src/Lawn/CoopPlay.cpp:255/326/368`。CANCEL只处理尚未回答的incoming，OFFER后的保留不在此分支；mLentLevel还被写入检查点。修复必须区分未提交与已确认，不能把所有CANCEL无条件回滚，否则可能重复出借。
- **F18**：两端 `Lawn/Online/NetSession.cpp::SendStartLevel/SendSeedsReady`、`NetProtocol.h::MsgSeedsReady`；Android `src/Lawn/OnlineGame.cpp::UpdateRunPick/UpdateOnlineSeeds/TryKeepEndlessLawn`。SEEDS_READY只有席位＋布尔值，没有局/关或准备代次。新关初始化与重连补发START有不同语义，修复不可混为一次清零。

## 排除、收敛与已有工作

1. **第一轮R03证据不足，下调疑点。** Android `SexyAppFramework/SexyAppBase.cpp:1962` 的RehupFocus先调用LostFocus，再调用WidgetManager::DoMouseUps；LawnApp会在可暂停时弹暂停框。因此“LawnApp没显式清拖动字段”不能直接推出切后台卡植物/幽灵种植。特殊系统中断仍可代表性实机检查，但不基于此立即改手势。
2. **未发现临时互斥词条必然覆盖新永久路线。** GetEffectiveBuffCount有永久路线优先的冲突屏蔽，不能只看mLoanBuffs没清就报错。
3. **不判定借入植物一定在手动选卡页消失。** HasSeedType等走有效卡池，不是简单只读永久池；完整>10卡借用仍在第一轮R01的待验范围。
4. **F06有并行工作，不重复修改。** 当前另一批工作已加入双端H/J与Android己方血量数字；其交接明确手机普通触摸开关尚缺，且未安装新包。见[键位/血量交接](KEYBOARD-HEALTH-FIX-STATUS.md)。不把第一轮“没有绘制源码”的历史状态当当前事实，也不因此将手机入口验收标完。

## 修复依赖与最小验收

1. **先合并F01/F02/F13/F14的存档方案。** 明确保存的是开战前检查点还是可恢复的选择阶段；重摇发生中途就不能生成“已无待选”的档。若恢复选择阶段，需要保存待选量、候选/去重状态、当前重摇价格及有效阶段；钱包与券、成长检查点应有一致的提交/失败处理。双端同批兼容，不能只在手机按钮后加Save。最小验收是一笔重摇后读档、一次写入失败或中断边界，不扫描全部随机种子。
2. **F16先挡已知offline目标，再处理送达语义。** 触摸选择页与网络层都拒绝已离线席位；仅挡offline还不能解决发送瞬间断线。若复用有回执的赠送方案，保留主动赠送与求援的不同资格规则、100扣款/90到账、去重与不确定时不重复退款，不改PC数值。最小验收：离线不扣款；成功到账一次；重复回执不重复结算。
3. **F17补借用事务收口。** CANCEL、DECLINE、超时、断线、退出统一区分待确认与已提交，并保存真实名额状态。先厘清ACCEPT丢失时如何确认结果，不能靠盲目超时回滚制造第二次借用。最小验收是一笔成功借用和一笔OFFER后取消/掉线。
4. **F15统一处理保留实体的规则变化。** 在选词条前后计算最大生命并按既有比例策略更新，避免把受伤植物直接回满；保留大嘴花/钢南瓜特殊规则，避免重复乘算。最小验收：无尽保留植物受伤状态下选Rooted，旧株/新株最大生命一致，当前生命按既定比例变化。
5. **F18先补真实阶段场景再定协议。** 选一条无待选续关/低卡池开局，用延迟的false检查放行是否越界；修复应区分“下一轮清理”与“同轮重连补发”，必要时加入准备代次，PC/Android同时迁移。现有正常等待路径不得退化。

本轮不因为新增问题调整成长数值，不把共享缺陷修成双端不同规则；这些项加入第一轮修复队列，未宣称已修。

## 证据与清理

仓库外目录：`../android-artifacts/parity-audit-round2/`（相对于Android仓库）。

- `probe.cpp`、`probe.rsp`、`build-probe.bat`：本轮六项定点探针；不加入发行工程或日常全量测试。
- `results-frozen.log`：唯一采用的本轮运行结果，含SANITY种子130013/关序号0、六项结果及退出码0。
- `relay-frozen.log`：有效运行对应房间CVD7，含断线保留、丢弃0x13与房主关闭。
- `baseline-manifest.json`、`object-manifest.json`：源码观察时的HEAD、关键文件及隔离对象SHA256；`pc-baseline/`为冻结头文件/源码快照，不能当当前工作区覆盖回来。
- `data/userdata/`仅含独立测试档案974。未调用ADB、未读取或更改手机用户文件，也未关闭玩家PC游戏。
- 本机58999中继只用于本轮，检查结束后按已记录PID和可执行路径核对并关闭；不影响用户云中继或其他端口。
