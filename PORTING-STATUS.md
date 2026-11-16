# Android 移植进度

## 2026-11-12 MOD113 正式交付

- 当前正式版为 MOD113，APK 为 arm64、versionCode 113。MuMu 3 保档覆盖完成，公告和主菜单已验。2026-11-12 复核已安装 APK 的 SHA256 与正式附件一致。 [完整包](https://github.com/Not-a-Different-Name/PVZ-online/releases/tag/mod113-full-20261112)
- Android 完整原生回归 696 项、双端本地中继 28 项、两服 PC＋MuMu 公网各 5 项通过。覆盖公开房间、参战和观众共用六席、掉线后观战、关间角色切换和回房。
- Android 对应源码分支为同一 PvZ Online 仓库的 `android-port`。以下为早期移植历史。

## 2026-10-18 Build68 当前工作区（本地验证通过，未打包/部署）

公共房间大厅和本地最近50局查询已实现，联机页新增“公开房间”“最近记录”。大厅仅供开局前入房，无尽关间补位继续使用房间码；记录按玩家档案隔离，支持筛选、详情与备份恢复。PC/Android原生构建、Go竞态测试、36项新功能检查、27项无尽补位回归及六人准备流程回归通过；Android ARM64原生会话联测通过。未安装新APK、未部署公网、未打包，完整手机UI触控及长局仍待验，onlineGameReady=false。详见[实现与验收](docs/LOBBY-HISTORY-BUILD68-2026-10-18.md)，本条优先于下方历史状态。

## 2026-10-18 Build67 当前工作区（本地验证通过，未打包/部署）

无尽关间按房间码补位、增益补齐、独立额度布阵、队伍独立检查点21、门禁确认和随机重连凭据已同步PC与中继；联机67、协议1。API24 arm64原生构建、5PC+1Android原生网络会话补位联测通过；PC引擎存档27项与准备流程50项、实际界面及布阵退出恢复/开战通过。未安装新APK或覆盖现用存档，手机真机UI触控和完整长局仍待验，onlineGameReady=false；公共大厅与最近记录查询未包含。详见[实现与验收](docs/ENDLESS-ADMISSION-BUILD67-2026-10-18.md)，本条优先于下方历史状态。

## 2026-10-17 Build60 旧反馈续修（仅源码）

补齐聊天小按钮原艺术字及窄按钮字宽、首波缺字、手机五场景无尽入口与旧奖杯提示、旧局隐形墓碑出土。仅源码和记录，未编译/测试/验收/安装，不动实际存档及原资源，onlineGameReady仍false。详见[续修报告](docs/FEEDBACK-FOLLOWUP-BUILD60-2026-10-17.md)。

## 2026-10-16 Build60 当前工作区（仅源码）

首波18/30/60秒、默认30秒已接入原生设置/存档/开局同步/重连与倒计时，限有限闯关；樱桃/焦土及毁灭菇范围加成包含完整纵向圆形范围。MOD_BUILD60、协议1、检查点17，兼容覆盖层见docs/compatibility/updates/build60-firstwave-range-working-tree.json。继承Build59未发布修复，未编译/测试/验收/打包/安装，onlineGameReady仍false；此条不能当成手机运行包已更新。详见[本批报告](docs/FIRST-WAVE-RANGE-AUDIT-BUILD60-2026-10-16.md)。

## 2026-10-16 Build58 当前修复

双端天降夜晚/迷雾非正倒计时修复已同步，26项定点检查通过；当前规则Build58、协议1、关间检查点17，工作树覆盖层见`docs/compatibility/updates/build58-skyfall-working-tree.json`。此条优先于下方历史Build52/57。默认交付带资源和汉化的完整包；构建及产物见PC[修复记录](../re-plants-vs-zombies/docs/SKYFALL-FIX-BUILD58-2026-10-16.md)。

## 2026-10-15 Build52 检查点（优先于下方历史）

固定PC来源 `d30f30a5e75f6b9f65471882a0505ae21a277f61` / Build52。已实现关间借用专属buff取最高、到期恢复与v16检查点，以及战斗阳光求援、回执重发去重。保留此前Build51词条、观战和石材UI迁入。双端编译与21项核心检查通过，Android对原生会话验证借入及求援到账/重复不再加账。完整双端实战和观战仍未验，`onlineGameReady=false`保留。

借用/求援本轮已交付；任意换位和移动UI剩余项待后续。产物、用户存档恢复和验证边界见 [持续交接](docs/COOP-ROLLOUT-STATUS.md)。下方历史中的旧版本及未实现说明只代表当时状态。

## 基线

- Portable 上游：wszqkzqk/PvZ-Portable，提交 `141c1476dd462ac3526f796e303177616f4de6bc`。
- 固定源码包 SHA256：`A3966F829E4F05FE7A1D67EBC7ECD575F84EF9DCD8E9A8418E171E7A0DD12AA4`。
- Windows 初期来源：`7a756a2db2ef88b10a08ec4d5c34900818a4995c`；完整 buff 来源更新为固定提交 `61c37d625ccb3cb7cecaed2422798e8ec5746cbf`（MOD_BUILD 44），现同步至 `0e08297fb047cda41b735d7be092a2bdf499ca90`（Build 45）。不读取 Windows 未提交修改。
- 此工程来自固定提交压缩包，没有上游 Git 历史；本地初始化提交 `ac3e1e5` 是开发基线，不能误称为上游提交。共享仓库为 `https://github.com/Not-a-Different-Name/PVZ-online-android`，整合分支 `android-port`，保留基线和拖放修复历史。

## 工具链（2026-10-13）

- JDK：CLion 自带 JetBrains Runtime 21.0.8。
- Android SDK：项目父目录 `android-sdk/`，platform 36、build tools 36.0.0、platform-tools。
- NDK：28.2.13676358；native API 24，ABI arm64-v8a。
- CMake/Ninja：CLion 自带；SDK 也已安装 CMake 3.22.1。
- Gradle：8.12，项目父目录 `android-build-tools/`。
- SDL2：2.32.10，已生成并安装 ARM64 `libSDL2.so`。
- vcpkg ports：上游锁定提交 `e8709ee8703328fe121eab33cda73a406a7626b7`，本地压缩包展开；Git registry fetch 遇到连接重置，改用此固定版本的本地 ports。

## 当前状态

**2026-10-16校准更新：当前共同规则为Build57，源码含未提交覆盖层；109项功能与113条增益目录的最新核对见[发布前校准](docs/RELEASE-CALIBRATION-2026-10-16.md)。本轮四处补修已进入候选APK，双端构建/15项定点通过，尚未安装及整局验收，onlineGameReady仍false。下段Build46/旧APK为历史状态，不能作当前发布依据。**

当前同步至固定 PC Build 46 / `8bfc67ea9c85be37cac0a182c3416f3add2c1640`：血量计算、首领接力、16 位投票流程与重摇币面口径已迁入；手机战斗新增消息、表情、送阳光三个入口，已有房间/开局/选卡屏障保留。源码 UI 非原版风格清单已记录，整体美术替换尚未执行。最新 APK 为 `pvz-online-keyboard-arm64.apk`，28,167,140 字节，已安装 MuMu，v2 签名与 16 KB 对齐通过；新增 1–9/0 选植物与 Q 铲子，交付和有限验收见 [快捷键结果](docs/KEYBOARD-CONTROLS-STATUS.md)。协作与受控投票代表检查见 [协作结果](docs/MOBILE-COOP-STATUS.md)，后续界面替换见 [UI 盘点](docs/UI-STYLE-AUDIT.md)。PC 快捷键尚未同步；完整 PC 战斗/跨关仍待验收，onlineGameReady=false。

### 批次 C 交付记录（历史）

### 2026-10-14 · 防具变体、高级单机选项与同步准备

- 八种防具变体 34–41 的实际行为、定义/缓存和完整出怪名单已迁入，计数和缓存专用编号与固定 PC 一致。高级选项、无尽场景/保留草坪和关底首领规则已接入；完整边界见 [本轮结果](docs/VARIANTS-OPTIONS-STATUS.md)。
- 保留旧 v4 的 100×50 波表块，额外容量及新状态使用独立可选块。旧冒险档读取/重新保存、CRC 和新增槽位默认值检查通过；最终五个用户文件已恢复且哈希一致。
- MuMu 检查包含中文入口、选项检查点、报纸防具组合外观与实际投放、无尽夜晚 48 株选卡；没有完整验收所有变体战斗、首领、无尽跨关或双端联网。
- 协议/会话和 POSIX 传输已编译；游戏侧联机仍未接入，onlineGameReady=false。根据用户要求提供固定来源、差异审计和构建版本检查；PC Build 45 已列为候选，当前 APK 仍明确使用 Build 44。
- 当前 APK 为仓库外 pvz-online-variants-options-arm64.apk，28,061,904 字节；Native/Apk、v2 签名、16 KB zipalign 通过并已安装 MuMu。包哈希与有限检查见本轮结果。

### 2026-10-14 · 完整 buff 已迁入

- 96 条目录逐条对应消费者，92 条按 PC 规则可抽取，3 条退池效果保留存量，初阳沿 PC 停用；植物池开放 48 株，咖啡豆/升级植物/加农炮转化规则已接入。
- 按固定 PC Build 44 对齐 Kind、倍率、取整、费用/冷却/发射间隔、倍选和金币重摇；闯关金币正常进入冒险档案，取代批次 A 的金币屏蔽。
- 新实体状态使用可选 v4 扩展块，旧 Android 冒险 v4 可读取；RunState v13 增加倍选选项位。PC Windows 原生局内档互读不在承诺范围。
- MuMu 代表检查包含倍选/真实钱包扣费保存、日夜生产、复制/咖啡豆、连发/贯穿、窝瓜入口、两格炮、大喷菇命中/小喷菇死亡以及旧 v4 与非零效果计时读写；没有穷举全部 buff 组合或重新跑完整关卡。五个用户文件已恢复且 SHA256 一致。
- 最终 Native/Apk、v2 签名、16 KB zipalign 通过，APK 已更新安装 MuMu。包与哈希见 [完整结果](docs/BUFF-PORT-STATUS.md)，逐条状态见 [迁移矩阵](docs/compatibility/buff-port-matrix.csv)。
- 剩余：新增僵尸、联机闭环、高级选项/boss/无尽、原版风格 mod UI、完整平衡、SAF 与真实手机验收。

以下批次 A 及更早内容为历史记录，其中“39 株/只开放三条 buff/金币屏蔽”等已由本轮取代。

### 批次 A：五场景战斗基础与重打（历史）

- 批次 A 已实施：固定 PC 来源的现有类型出怪表、波数、抽怪权重、预算与本体/防具场景血量；巨人价值/数量限制与每波 20 只上限。PC 新增僵尸 34–41 仍过滤，保留原编号；三种时长模式按实际引擎关号查同一张表。
- 基础植物池从 28 扩到 39；蘑菇白天清醒、非屋顶陆地可种水生植物。卡池最多 10 时全部自动带入，超过 10 时选卡。咖啡豆、升级植物和其特殊规则仍未迁入。
- 单机无推车/耙子/急救购买加成；失败独立记账，重打保持关号、卡池、增益与种子。花园掉落不进入原版档案。未实现的高级选项检查点会明确拒绝。
- 定点验证覆盖一次白天真实 10 波胜利→奖励页、失败重打、夜晚向日葵、泳池睡莲+清醒小喷菇、屋顶新花盆+清醒小喷菇，以及迷雾载入。胜利使用增强现有增益的仓库外检查点，不代表标准档平衡或完整 25 关验收。原版 user1.dat、users.dat、game1_0.v4 和语言设置哈希保持一致；用户原检查点已恢复。
- 当前 APK：仓库外 `android-artifacts/pvz-online-run-rules-arm64.apk`，27,920,560 字节，SHA256 `1B23C09B682CC5C1214B042F37191F50C959C0ED81685D52485BEE2470E32A67`，增量 Native/Apk 构建、v2 签名、16 KB zipalign 通过，已安装 MuMu Android 15。下一批按完整 buff 迁移计划实施。
- 未完成：全部增益和特殊植物规则、新增僵尸、联机、原版风格 mod UI、完整五场景平衡、SAF 用户操作及真实 ARM 手机验收。

### 上一批次：单机闯关与语言

- 独立推进；同伴任务 A 的枚举报告已完成，见 docs/compatibility/enum-audit.md。SeedType 实际编号一致；PC 僵尸 34–41 尚需迁入行为与缓存。RunZombieRoster 只作为参考，不编译。
- RunState/RunBuffs 来自固定 Windows 提交 7a756a2，适配 Portable API。最小闯关入口、25/10/5 关、开局四植物/两增益、换关奖励与独立 android_run*.dat 检查点已接入。
- 只开放火力/扎根/速种，实际挂到弹体伤害、植物血量和种子冷却。植物抽取池也限制为当前已实现的 28 种；未迁入的增益不会作为空效果出现在选择中。
- MuMu 已进入闯关并种植；用临时受控结算覆盖两次奖励→下一关，检查点为 v13、levelIndex=2、植物池 10、扎根 4 层。原版 user1.dat 前后 SHA256 均为 10CD4360C11E2CD54FA440A1B05892FE836BE552C71B59C7258D086F1E95719F。临时入口已从最终源码移除；未等待完整战斗胜利，不宣称已完成平衡验证。
- 语言前移到当前阶段，范围按全部界面。自动/中文/English 保存后重启统一应用；原版菜单、图鉴和提示通过玩家自己的 pvz_zh.pak/LawnStrings_zh.txt 接入，新增页面与资源管理页双语。复用 PC 位图字体，内置 Noto CJK 补齐新文案和缺字，许可与来源见 docs/LOCALIZATION.md。
- MuMu 自动模式识别中文，已看到中文标题、主菜单、选项与保存提示；通过语言按钮保存 English 后重启，标题/主菜单图片和闯关入口均恢复英文。成就页翻译键另作适配。
- 最终包恢复第 3 关检查点，中文闯关模式页、选卡页、图鉴目录与豌豆射手长文均显示正常；成就页和资源管理页中文显示正常。恢复后原版 user1.dat 哈希仍相同。SAF 复制交互与全部图鉴条目未穷举。
- 当前包为仓库外 `android-artifacts/pvz-online-run-language-arm64.apk`，27,914,172 字节，SHA256 `41D798AEEB5AC0D1C3D6EA059161F1FE1B38ECD7052B26A2F179A433E16C70D4`；增量原生编译/Gradle 打包通过，开发签名 v2 验证与 16 KB zipalign 检查通过，已更新安装到 MuMu。APK 只内置开源字体，没有原版或 PC 汉化素材。
- 未完成项：PC 出怪/难度/全部增益、推车跨关规则、联机、SAF 用户操作与真实 ARM 手机性能。中文资源检查只覆盖代表性页面，没有逐条遍历全部原版对话。

### 底座记录

- SDK 安装成功，Gradle 能启动，SDL2 编译成功。
- 图像/音频依赖、游戏 native 库与 APK 已构建成功。API 24 使用独立安装目录 `android-native-api24-deps/`，不要误用首次 API 28 的产物。
- 底座包：父目录 `android-artifacts/pvz-online-base-arm64.apk`，14,179,626 字节，SHA256 `03B8E9BBE64BAAC13AE767D7A8C6F59EB3E4916671BC84A0ED0E744E1180DFC6`。开发签名 v2 验证通过，APK 16 KB zipalign 检查通过；运行记录见下。
- Gradle 工程配置已通过。开发包名 `io.github.pvzonline.android`，名称 `PvZ Online Dev`，与社区原版隔离；Java namespace 暂保留底座命名。
- 可重复构建脚本：父目录 `build-android.ps1`，阶段 `Dependencies` / `Native` / `Apk` / `All`。
- MuMu 已升级为 Android 15，9 GB 内存，实例 2；ADB 127.0.0.1:16448，ABI x86_64/arm64-v8a/x86。已安装 ARM64 包、看到资源管理页、通过 ADB 复制本机正版资源、进入主菜单和第一关并观察到已种植的豌豆。OpenSL ES 音频流已启动；没有通过听音确认声音。未检验 SAF 交互导入全流程，也没有把模拟器结果当作真实 ARM 手机性能结论。
- 用户报告种子拖动松手不种植，已实现 Android 拖放修复；增量编译、APK 打包和更新安装通过。在 MuMu 的 1-2 关用向日葵验证：拖到无效土地区域松手取消，阳光保持 50，无悬浮植物；随后拖到草坪松手直接种下，阳光变为 0，无需第二次点击。点选再点格子仍沿用原路径（此轮未另做交互回归）。
- 修复包：仓库外 `android-artifacts/pvz-online-touch-arm64.apk`，14,180,106 字节，SHA256 `B1DC726EFA64BD403A132460A2D51B5055F961F4ED8A4B7ADB0D7BF9E60940B8`。保存了有效/无效落点截图，未将游戏画面上传共享仓库。
- Android 已迁到独立 PVZ-online-android 仓库；现有 PVZ-online/android-port 保留为旧临时副本，后续不再向它同步。任务书见 docs/COLLABORATION.md。
- 未改 Windows 游戏代码、未部署服务器、未打包正版素材。

## 差异地图初稿

| 本项目模块 | Portable 对应/动作 |
|---|---|
| Lawn/Online/NetProtocol.h | 纯固定宽度小端编解码，优先保持协议 |
| Lawn/Online/NetLink.cpp | Winsock/线程/锁改跨平台；保留对外接口与超时语义 |
| Lawn/Online/NetSession | 核对引擎接口、帧计时及 Android 生命周期 |
| Lawn/Run | 迁入 src/Lawn/Run；CMake 当前 GLOB 未包含此子目录，需显式加源文件 |
| Sexy.TodLib | Portable 改为 src/PvzpLib，Tod 类型/函数需要适配，禁止整目录覆盖 |
| Board/Plant/Zombie/Projectile | 按功能挂点迁入，保留 Portable 平台与保存修复 |
| ModText | GDI 路径不可复用；核对 Portable 字体能力后适配 |
| GameSelector/联机对话框 | 加入口、软键盘和触屏按钮；先不改逻辑画布 |

阶段顺序与验收预算见父目录 `ANDROID-PORT-PLAN.md`。
