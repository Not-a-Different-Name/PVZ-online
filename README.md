# PvZ Online — 多人合作 PVE 联机改版

> 基于 [Patoke/re-plants-vs-zombies](https://github.com/Patoke/re-plants-vs-zombies)（《植物大战僵尸》GOTY 版
> 逆向重制工程，原工程以 CC0 发布）开发的**联机 mod**。
>
> **本仓库只包含代码，不含任何 PopCap 游戏资源。** 运行本 mod 需要你**自备正版
> PvZ GOTY 的游戏文件**（见下方[合规声明](#️-合规声明)）。

引擎为 2005 年的 SexyAppFramework；上游正在推进引擎现代化（C++23、以 GLFW 替换旧渲染后端，均在进行中）。
本项目当前以 **x86 / MSVC / Windows 窗口化** 为唯一目标平台。

---

## 玩法定位

把原版 PvZ 改造成**多人合作 PVE**（最多 **6 人**）：每位玩家拥有**自己的一块棋盘**，
阵亡漏怪不会立刻结束，而是**传送给下一位玩家的棋盘**继续进攻；
末位（最后一个上座席位）漏怪才算全队失败。过关后全队获得肉鸽式成长（三选一）。

### 三模式规划

| 模式 | 主菜单入口 | 内容 | 状态 |
|---|---|---|---|
| **关卡模式联机** | Adventure（复用） | 原版冒险流程跨场景连打，5 种环境（前院昼 / 前院夜 / 泳池昼 / 泳池雾夜 / 屋顶昼）；开局选时长档与出怪难度，高级选项含出怪规模 / 节奏 / 植物僵尸混入 / **巨型 Boss（闯关关底）**；局内 T/E 快捷聊天、V 观战、G 发阳光 | ✅ 已启用 |
| **无尽联机** | Survival（只留 Endless） | 走闯关肉鸽骨架的无尽档（MOD_BUILD 37）：每张 Endless 卡锁定一个环境（与上同 5 环境），关卡在该环境 5 个原型内无限循环；难度 = 环境基准 + 对数阶曲线（前期猛涨、后期趋缓）；无通关、全队败可重打、中途退出可续档 | ✅ 已启用 |
| **PVP（互相送怪）** | 灰置占位按钮 | 击杀传播玩法，待设计 | 占位已上线 |

M1 已**隐藏**小游戏、解谜、生存 Normal/Hard、商店、禅境花园、僵尸工坊入口；
**保留**图鉴、成就、换用户、选项、退出、帮助。

### 联机架构（已定，M2 起实现）

- **棋盘主权分离**：每个客户端权威模拟自己的棋盘，跨棋盘只传离散事件（漏怪传递等）
- **服务器无状态**：Go 写的房间 + 事件中继，可跑在 1G 内存的小型云服务器上
- **刷怪分布**：各席位自然刷怪按上座顺位直接倍乘（**末两席同为原量 ×2**、再往前每位**翻倍**——
  二人 2:2、三人 4:2:2、四人 8:4:2:2、五六人续 16 / 32、**封顶 ×32**；每波数量上限同倍放大；
  2026-10-03 二次定案，2026-10-04 批七把末位 ×1→×2，口径以 `docs/04-联机-M2.md` §一为准）；
  末位每行一台推车兜底（整局一次性、用掉不补；非末位无除草机）；漏怪保留剩余血量、从同行右侧入场

---

## 当前状态（2026-10）

- ✅ **M0–M2**：编译跑通、模式裁剪、双人局域网联机全功能（同关同步、漏怪传递、
  双清判胜/全队败、暂停与退关同步、换位与名册）——两机实机验收通过（2026-10-02）
- ✅ **M3 中继上公网**：Go 房间 + 事件中继跑在云服务器（按房间码加入、最多 6 席位），
  客户端默认指向云端；操作与协议见 [`docs/05-联机-M3.md`](docs/05-联机-M3.md)
- ✅ **M4 成长 + 平衡全链落地**：闯关肉鸽（完整 25 关 / 普通 10 关 / 快速 5 关三档时长、
  场景难度阶梯、过关三选一 + 单株/全局 buff 池、检查点续档）、出怪难度旋钮与高级选项
  （出怪规模 / 节奏 / 植物僵尸混入 / **巨型 Boss 关底**）、**多人无尽**（锁环境无限循环 +
  对数难度曲线 + 续档）、观战（按住 V 看队友场地）、局内快捷聊天（T/E）、
  **发阳光**（按 G 再按 1-6 直选队友，对方场地天降阳光）
- ⏭ **下一步**：实机验收清单消化（`docs/03-过程与问题.md` §5.44–§5.69）+ 发布链推送

最新一批 = **阶段④⑤：关底巨型 boss + 发阳光**（`MOD_BUILD = 38`，
见 `docs/03-过程与问题.md` §5.69）。

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
