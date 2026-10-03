# PvZ Online（合作 PVE 联机改版）— 项目指南

基于 Patoke/re-plants-vs-zombies（PvZ GOTY 逆向重制，CC0）开发四人合作联机 mod。
上游 README 说明编码风格（m/the/a 前缀、@Contributor 注释标记）。

> **动手前必读**：`docs/规范.md`（项目约束）与 `docs/README.md`（文档索引）。
> 换电脑：根目录 `../TRANSFER.md` → `docs/01-转移与重建.md`。

## 构建（Windows + MSVC x86）

```
cmd /c <pvz-online 根>\build-msvc.bat           # 增量构建（本机：C:\Users\18611\Desktop\pvz-online）
cmd /c <pvz-online 根>\build-msvc.bat reconfig  # 删除 build-x86 重新配置
```

- 产物：`build-x86/SexyAppFramework.exe`（目标名固定，勿改）
- 覆盖 `runtime/` 里的 exe **前必须先杀进程**（`taskkill //F //IM SexyAppFramework.exe`），
  否则报 `Device or resource busy`；**必须以 `runtime/` 为工作目录启动**
- **`runtime/` 里我们自己构建的 exe 只保留最新版**（`SexyAppFramework.exe`）；
  临时诊断副本（`SexyAppFramework_bXX.exe`）用完立刻删（`docs/规范.md` §8）
- VS2022 Community 自带 CMake/Ninja，勿用 `cmake` 裸命令（不在 PATH）
- 工作分支 `pvz-online`，勿直接提交到 main；基线提交 `11950d5`
- 在线仓库 `git@github.com:Not-a-Different-Name/PVZ-online.git`
  （<https://github.com/Not-a-Different-Name/PVZ-online>）：推送/拉取都用它的
  **`pvz-online` 分支**（机器 2 的同步源）；`origin` 是上游 `Patoke/re-plants-vs-zombies`，**别动**
- 构建链已迁移化（8c4204e）：脚本自动探测 VS2022，`imagelib/CMakeLists.txt` 用
  `${THIRD_PARTY_DIR}` 相对引用本地依赖仓库——**换机器不再需要改任何路径**，
  但 `third_party/` 必须与 `re-plants-vs-zombies/` 并列（见 `docs/01-转移与重建.md`）
- 一次只改一处、一次构建只验一处；见 `docs/规范.md`

### 本仓库对上游的构建补丁（保持最小化）

1. 根 `CMakeLists.txt`：`windres` 仅在非 MSVC 设置（MSVC 用 rc.exe）；MSVC 加 `/utf-8`
   （源码 UTF-8 无 BOM 含中文注释，默认 GBK 码页会误解析出连锁语法错误）
2. `SexyAppFramework/CMakeLists.txt`：`-static*` 链接参数仅 GCC 系
3. `SexyAppFramework/misc/SEHCatcher.cpp`：x64 寄存器引用加 `#ifdef _WIN64` 分支
   （x86 用 Eip/Ebp；`ImageHelpWalk` 在 x86 返回空，dbghelp 指针 typedef 仅 x64 兼容）
4. `SexyAppFramework/imagelib/CMakeLists.txt`：三个 ExternalProject 的
   GIT_REPOSITORY 指向本地 `${THIRD_PARTY_DIR}/{zlib,libpng,libjpeg-turbo}`
   （`THIRD_PARTY_DIR` = 仓库根上两级的 `third_party/`；这三个是预下载自
   codeload.github.com 并 git init+tag 的本地仓库，绕开 github.com 443 不稳）；
   MSVC 下额外传 `-DWITH_SIMD=0`（无 NASM）
   - 升级依赖版本时改这里 + 重新准备 third_party 下对应本地仓库（需含同名 tag）
   - 注意：仓库内文件编码混杂（多数 UTF-8，个别如 Cutscene.h 是 GBK），
     `/utf-8` 下 GBK 文件只产生 C4828 警告，无害

## 运行资源（重要）

- `main.pak` **是在运行时真的被加载的**：`SexyAppFramework/SexyAppBase.cpp:6118`
  `gPakInterface->AddPakFile("main.pak");`
  （**更正**：本文档此前写「已被上游注释掉」，是错的。）
- **散装文件只是回退**：`paklib/PakInterface.cpp:194-220`（`FOpen` 先查 pak 索引，
  查不到才 `fopen`）。所以资源优先走 pak，散装只在缺 pak 时兜底。
- 用户已安装 **Steam 正版 PvZ GOTY**，素材解包在 `C:\Users\18611\Desktop\pvz-online\runtime\`，
  **仅供本机测试**；**PopCap 资源绝不随 mod 分发、绝不上传**（见 `docs/规范.md` §6）。
- 看原版素材用 `tools/pakx.pl`（格式：整文件字节 XOR `0xF7`，magic `0xBAC04AC0`，
  条目 = flags/nameWidth/name/size/FILETIME，数据区紧跟结束标记；**文件名是反斜杠**）。

## 代码地图（M1 裁剪/改造入口）

- `LawnApp.cpp/h` — 全局状态机（mScreen）与各界面切换
- `Lawn/Board.cpp` — 棋盘模拟核心（漏怪/入场逻辑的接入点）
- `Lawn/Zombie.cpp`、`Lawn/Plant.cpp` — 战斗实体
- `Lawn/Widget/GameSelector.cpp` — 主菜单（裁剪模式入口处）
- `ConstEnums.h` — GameMode 枚举（生存×15、挑战×22+、解谜等待裁）
- `Lawn/System/PlayerInfo.cpp` + `ProfileMgr.cpp` — 用户进度（本地存档改造点）
- `Lawn/Online/` — 联机层（`NetProtocol.h` 协议 / `NetLink` 唯一 socket 层 / `NetSession` 会话状态机）；
  玩法与协议速查见 `docs/04-联机-M2.md`
- `SexyAppFramework/paklib/PakInterface.cpp` — 资源读取（FOpen pak→散装回退）

## 联机架构（已定）

- 棋盘主权分离：各客户端权威模拟自己棋盘，跨棋盘只传离散事件（漏怪传递等）
- 服务器：Go 房间+事件中继，无状态（1G 内存云服务器）
- 合作 PVE 规则：自然刷怪按**上座顺位**直接倍乘（末位 = 原量 ×1、往前每位 +1 倍——
  四人 4:3:2:1、三人 3:2:1、二人 2:1；每波数量上限同倍放大；2026-10-03 用户定案，
  取代早先文档的 8:4:2:1，落地见 `docs/03-过程与问题.md` §5.13）；
  无除草机；漏怪传下一席位地图（保留剩余血量、
  同行右侧入场）；席位 4 漏怪=全队败；成长=过关肉鸽三选一（本地存档）
- PVP 模式（击杀传播）在合作 PVE 完成后再设计，事件协议预留扩展

## 里程碑

M0 编译跑通 → M1 裁剪+关卡流+存档 → M2 双人局域网（漏怪传递）→ M3 四人+云部署 → M4 成长+平衡

## 网络注意

本机访问 github.com:443 时通时断；api.github.com / codeload.github.com 通常可用。
克隆/下载失败先换 codeload tarball + curl，或重试。
