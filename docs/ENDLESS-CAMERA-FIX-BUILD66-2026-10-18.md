# Build66 无尽视角偏移修复与验收

后续：2026-10-18已按用户“编译”要求生成双端完整测试包并完成隔离首次启动，未覆盖现用安装。见[编译与完整包记录](PLAYER-FULL-BUILD66-2026-10-18.md)。下文“未发包”为修复验收时的历史状态。

日期：2026-10-18。用户报告无尽视角偏移后不返回，并提供偏向房屋、卡槽/菜单消失的截图。当前工作区MOD66已复现同类故障并完成双端修复；未发包或覆盖现用安装。

## 已复现根因

PC真实资源游戏窗口，实际六席位联机/两席位复现路径：开局选卡→进入战斗→模拟本关清空并由每席报完成→实际全队过关结算→消耗奖励屏→进入下一关。

旧流程在全队结算后设置了冻结标记，却保留SCENE_PLAYING。UpdateRunPick把旧草坪误当成已经进入下一关的新草坪，绕过START/ACK/RUN_GO，直接放开开场。StartLevelIntro把棋盘移到(220,0)，隐藏卡槽/菜单；CutScene::Update发现游戏仍是战斗场景，不推进开场，所以视角永久留在房屋侧。

[修复前运行日志](../../tools/audit/endless-camera66-before.log)记录：`AFTER_TEAM_CLEAR scene=3 held=1`，随后`STALLED_OR_CHOOSER scene=3 board=220,0 time=0 choosing=0 held=0 chooser=1`。scene=3为战斗，2为开场。这与用户截图的房屋偏移及缺失顶栏现象一致，未声称取得了用户截图时的运行日志或确认其他未知触发方式。

## 修复内容

PC `LawnApp.cpp`、Android `src/Lawn/OnlineGame.cpp`各三处局部修改：

1. 联机无尽结算进入开场场景并冻结旧草坪，奖励与保存不再留在战斗场景。
2. 旧关完成标记、等待ACK或等待RUN_GO期间，先走下一关联机握手。握手完成后才在保留的草坪上处理奖励、选卡与关间准备，防止沿用上一关准备轮次。
3. ReleaseRunIntro明确恢复开场场景，再启动镜头回移，保证场景和计时状态一致。

未用每帧强制坐标归零来遮盖异常；镜头继续走原生动画。未改平衡、资源、协议编码、MOD66编号或存档实体布局。植物阵型、生命、阳光、选卡及个人小车记录继续保留。

改前文件在`tools/audit/endless-camera66-before/`，本轮独立差异在[源码差异](../../tools/audit/endless-camera66-source.diff)。保留关间换位及此前工作区修改，未提交/推送。

## 验收

| 项目 | 结果 |
|---|---|
| PC x86 Release | [编译链接通过](../../tools/audit/endless-camera66-pc-build.log) |
| Android API24 arm64 | [编译链接通过](../../tools/audit/endless-camera66-android-build.log) |
| 房主端实际PC引擎＋6个网络会话 | 连续从第1关到第3关，**24项，0失败**；实际消费4屏奖励、每个续关仅一次新开局命令、选卡场景正确、开战镜头(0,0)、10张卡、植物原位置/123生命、777阳光均保留。[日志](../../tools/audit/endless-camera66-after.log) |
| 非房主端实际PC引擎＋6个网络会话 | 连续两次续关，**24项，0失败**；每关接收一次新开局命令并重新准备，奖励、镜头回位、卡槽及原植物/生命/阳光保留均通过。[日志](../../tools/audit/endless-camera66-client.log) |
| 原换位/存档回归 | **22项，0失败**，[日志](../../tools/audit/endless-camera66-preparation-regression.log) |
| 既有引擎修复回归 | **27项，0失败**，[日志](../../tools/audit/endless-camera66-engine-regression.log) |
| 图像与运行状态 | 修复前/后均由规定的grab脚本采集800×600客户区1:1；背景采样匹配测得220像素水平位移，修复后镜头回到原点、卡槽回到y=0。[像素日志](../../tools/audit/endless-camera66-pixels.log) |

[修复前画面](../../tools/audit/endless-camera66-before.png) · [房主修复后画面](../../tools/audit/endless-camera66-after.png) · [非房主修复后画面](../../tools/audit/endless-camera66-client.png) · [前采集记录](../../tools/audit/endless-camera66-before-capture.log) · [房主后采集记录](../../tools/audit/endless-camera66-after-capture.log) · [非房主后采集记录](../../tools/audit/endless-camera66-client-capture.log)。截图未进行移动窗口或缩放补偿。测试程序为隔离夹具，使用实际游戏资源与引擎；完成关卡通过清空敌人并发送真实会话完成消息触发，并非真人完整打完每一波。

## 交付边界

专项覆盖无尽联机续关卡住的已复现原因，不等同全场景、六名真人长局或Android真机触控验收。测试档案及数据位于`tools/audit/`隔离目录，玩家现用安装/存档未覆盖，未安装新APK。以后发包仍按用户要求提供双端完整可用包并保档。
