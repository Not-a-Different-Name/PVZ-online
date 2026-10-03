# PvZ Online — 四人合作 PVE 联机改版

> 基于 [Patoke/re-plants-vs-zombies](https://github.com/Patoke/re-plants-vs-zombies)（《植物大战僵尸》GOTY 版
> 逆向重制工程，原工程以 CC0 发布）开发的**联机 mod**。
>
> **本仓库只包含代码，不含任何 PopCap 游戏资源。** 运行本 mod 需要你**自备正版
> PvZ GOTY 的游戏文件**（见下方[合规声明](#️-合规声明)）。

引擎为 2005 年的 SexyAppFramework；上游正在推进引擎现代化（C++23、以 GLFW 替换旧渲染后端，均在进行中）。
本项目当前以 **x86 / MSVC / Windows 窗口化** 为唯一目标平台。

---

## 玩法定位

把原版 PvZ 改造成 **4 人合作 PVE**：每位玩家拥有**自己的一块棋盘**，
阵亡漏怪不会立刻结束，而是**传送给下一位玩家的棋盘**继续进攻；
4 号位漏怪才算全队失败。过关后全队获得肉鸽式成长（三选一）。

### 三模式规划

| 模式 | 主菜单入口 | 内容 | 状态 |
|---|---|---|---|
| **关卡模式联机** | Adventure（复用） | 原版冒险流程，5 种环境（前院昼 / 前院夜 / 泳池昼 / 泳池雾夜 / 屋顶昼，含 5-10 Boss 夜战） | ✅ 已启用 |
| **无尽联机** | Survival（只留 Endless） | `SURVIVAL_ENDLESS_STAGE_1..5`（与上同 5 环境） | ✅ 已启用 |
| **PVP（互相送怪）** | 灰置占位按钮 | 击杀传播玩法，待设计 | 占位已上线 |

M1 已**隐藏**小游戏、解谜、生存 Normal/Hard、商店、禅境花园、僵尸工坊入口；
**保留**图鉴、成就、换用户、选项、退出、帮助。

### 联机架构（已定，M2 起实现）

- **棋盘主权分离**：每个客户端权威模拟自己的棋盘，跨棋盘只传离散事件（漏怪传递等）
- **服务器无状态**：Go 写的房间 + 事件中继，可跑在 1G 内存的小型云服务器上
- **刷怪分布**：各席位自然刷怪按上座顺位乘数递减（四人 4:3:2:1、三人 3:2:1、二人 2:1；
  2026-10-03 定案，取代早先的 8:4:2:1）；无除草机；漏怪保留剩余血量、从同行右侧入场

---

## 当前状态（2026-10）

- ✅ **M0 编译跑通**，主菜单三处缺陷已修复并实机验收：
  - 背景被 Quick-Play 场景覆盖（`65d1ecb`）
  - 成就页重构为顶层控件（`88e1f34`）
  - 主菜单滑动错位（`2007637`）
- ✅ 菜单阶段的历史堆断言（`_CrtIsValidHeapPointer`）在当前构建下**不复现，已结案**
  （结案方法与代码审查见 `docs/03-过程与问题.md` §4.4）
- ✅ **M1 模式裁剪已完成并实机验收**：主菜单只留冒险 / 生存 / PVP 占位三个入口
  （`7c4f230` 入口裁剪、`0a9d2f3` 生存解锁、`eaa5695` PVP 灰置占位）
- ✅ **M2 双人局域网联机已完成，两机实机验收通过**（2026-10-02，用户本人实机）：
  TCP 直连、同关同步、漏怪传递、双清判胜/全队败、不写档、暂停与退关同步、换位与名册
  （玩法与协议速查见 `docs/04-联机-M2.md`）
- ⏭ **下一步：全流程闯关（肉鸽）首版**（设计见 `docs/03-过程与问题.md` §5.1）

---

## 构建

**前置**：Visual Studio 2022（勾选「使用 C++ 的桌面开发」工作负载）。
无需单独装 CMake/Ninja（用 VS 自带的）。

**目录要求**：`third_party/`（zlib / libpng / libjpeg-turbo 三个本地依赖仓库）
必须与 `re-plants-vs-zombies/` **并列**存放。

```bat
:: 1. 构建（首次或改了 CMakeLists 时加 reconfig）
cmd /c <pvz-online 根>\build-msvc.bat

:: 2. 覆盖 exe 前先杀进程，否则报 Device or resource busy
taskkill /F /IM SexyAppFramework.exe
copy re-plants-vs-zombies\build-x86\SexyAppFramework.exe runtime\

:: 3. 必须以 runtime\ 为工作目录启动（靠它找 properties\resources.xml 与 main.pak）
cd runtime
SexyAppFramework.exe
```

产物固定为 `re-plants-vs-zombies/build-x86/SexyAppFramework.exe`（目标名勿改）。
更多细节（换机器、依赖准备、法律边界）见 [`docs/01-转移与重建.md`](docs/01-转移与重建.md)。

---

## 仓库结构

| 路径 | 内容 |
|---|---|
| `SexyAppFramework/` | 引擎（上游代码）+ 本项目的构建补丁（`imagelib/CMakeLists.txt` 等，见 `CLAUDE.md`） |
| `Lawn/` | 游戏逻辑（棋盘 `Board`、实体、界面 `Widget/GameSelector` 等） |
| `docs/` | **交接文档**（转移重建 / 技术总结 / 问题结论 / 项目规范）——动手前先读 `docs/规范.md` |
| `CLAUDE.md` | 项目指南（构建命令、代码地图、联机架构摘要） |
| `.gitignore` | 已排除 `build-x86/`、`../runtime/`（游戏资源） |

> 上游原版 README（英文，含引擎路线图与贡献规范）见
> [Patoke/re-plants-vs-zombies](https://github.com/Patoke/re-plants-vs-zombies)。
> 本项目遵循上游的编码风格与 `@Contributor` 注释标记规范；本项目自己的改动标 `@pvz-online`。

---

## ⚖️ 合规声明

- 本项目**不包含、不分发任何 PopCap 游戏资源**（美术 / 音频 / `main.pak` 等）。
  发布物**只有代码**。
- 运行需要你自己**合法购买**的《植物大战僵尸 GOTY 版》——从
  [Steam](https://store.steampowered.com/app/3590/Plants_vs_Zombies_GOTY_Edition/) 购买后，
  把游戏文件放到本地的 `runtime/` 目录（该目录已在 `.gitignore` 中排除，**永远不会入库**）。
- 请勿以任何形式上传、分享或重新分发游戏资源文件。
- 上游重制工程为 CC0；本项目同样**不宽恕盗版**。

---

## 致谢

- [@Patoke](https://github.com/Patoke) 与 [re-plants-vs-zombies](https://github.com/Patoke/re-plants-vs-zombies)
  全体贡献者——本项目的全部基础
- [@rspforhp](https://github.com/rspforhp)（0.9.9 版反编译）、[@ruslan831](https://github.com/ruslan831)（存档）
- GLFW 团队（上游引擎的现代渲染路线）
- PopCap——创造了 PvZ，并公开了 SexyAppFramework

---

## 已知问题

- **仅测试过窗口化 800×600**（DPI 感知已修）；不要强制全屏。
- 主菜单三条修复之外的旧 UI 缺陷尚未逐一体检（见 `docs/03-过程与问题.md` 待办）。
- 关卡内对局尚未做过堆检查长跑（菜单阶段已结案）。
- 排查用的运行时插桩（`PVZ_*` 环境变量）已于 2026-10-02 全部摘除；`PVZ_TRACE=1`
  可把引擎 `TodTrace` 输出镜像到 stderr（平时无需设置）。
