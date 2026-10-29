# PvZ Online — 多人合作 PVE

当前完整玩家版：**MOD104**。支持 Windows 电脑与 Android，最多6人，各自守自己的草坪，漏怪向下一位队友传递。电脑与手机可以一起玩。

## 下载与开始

在[最新完整版本](https://github.com/Not-a-Different-Name/PVZ-online/releases/latest)下载 PC.zip 或完整安卓 APK。电脑完整解压后运行“启动游戏.bat”；安卓直接覆盖安装，勿先卸载，以保留存档。联机请全队使用同版。

游戏自动选择官方服务器。创建房间后分享四位房间码，队友可复制、粘贴加入；无需选择服务器或输入前缀。也支持最多6人的自建联机。

## 当前玩法和操作

- 闯关与无尽合作、个人增益、僵尸强化投票、全队增益及队友互助。
- 常驻队友名单：点名字送阳光、送樱桃卡或观战；选卡时直接借植物，收到申请可同意或拒绝。求阳光和接收樱桃开关在名单旁。
- 本人完成前置选择即可参加全队投票，无倒计时。局内“增益→全队增益”查看名称、层数和效果。
- 无尽“开始新无尽／继续存档”分别进入，多局各自保留。联机房主选局，全队准备后开战；缺档队友按补位规则加入。
- 观战可刷新、换人；电脑 V 观战、R 刷新，手机点按钮。默认关闭P2P，可在“选项→帮助→帮助设置”开启。关掉P2P后仍能查看与刷新画面；后续战况需要新画面才会更新。
- 金币可用于重摇锁定候选、外观名牌等，合作成就与收藏不提供永久战斗属性。

MOD104 修复了无尽开局慢选／短重连导致队友提前跳过、重复开始提示和重新选择增益的问题，并简化队友操作。详见[版本说明](https://github.com/Not-a-Different-Name/PVZ-online/releases/tag/mod104-full-20261029)。

## 源码与构建

PC源码在[pvz-online分支](https://github.com/Not-a-Different-Name/PVZ-online/tree/pvz-online)，安卓源码在[android-port分支](https://github.com/Not-a-Different-Name/PVZ-online/tree/android-port)。正式源码从固定输入构建并提供 SHA256；本地开发树包含未提交更改，不能用旧HEAD代表完整产物。

PC 使用 VS2022/MSVC x86、CMake/Ninja Release；Android 使用 SDK36/min24、NDK28.2.13676358、arm64、CMake3.22.1、Gradle8.12。Source-Dependencies.zip 提供可公开构建依赖，第三方许可证保留。更新本地程序前保存并正常退出游戏，备份存档后替换；不要强制结束正在进行的对局。

游戏素材不提交到源码分支；完整玩家包在 Releases 单独提供。项目基于 Patoke/re-plants-vs-zombies 与 PvZ-Portable，保留各自版权与许可证，PopCap 原作权利属于原权利人。

## 待核查事项

此前玩家的原始冰瓜闪退及前台异常掉线尚未取得完整根因证据，不能将局部修复称为全部解决。BOSS按玩家增益总星数调整血量及每关BOSS档位仍需完整数值对照与验收；PVP仍为规划。已发布功能与历史讨论稿冲突时，以最新版本说明与交付记录为准。模拟器测试不能代替物理手机及六人跨家庭长局。

[支持作者](https://afdian.com/a/pvzol) · [B站主页](https://space.bilibili.com/491197058)
