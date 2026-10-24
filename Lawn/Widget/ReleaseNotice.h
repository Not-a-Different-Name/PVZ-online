#pragma once
#include "../ModText.h"
namespace ReleaseNotice {
inline const char* Body() { return ModText::Tr(
        "【MOD96：更省流量的观战】\n· 进入时只获取一次队友快照\n· 之后由本机播放移动与动画\n· 画面会逐渐偏离实际战况\n· 不影响你和队友的真实战斗" "\f"
        "【观看与刷新】\n· 电脑按住V，数字键切换队友\n· 点击观战按钮可持续观看\n· 点击刷新获取新的快照\n· 换人或刷新请间隔5秒\n· 点击返回或按Esc退出" "\f"
        "【安装与游玩】\n· 全体队友请统一更新\n· 保留全队增益、名牌和成就\n· 电脑完整解压后运行启动游戏.bat\n· 安卓覆盖安装，请勿先卸载",
        "[MOD96: lower-data spectating]\n· Get one teammate snapshot on entry\n· Your device plays movement and animation\n· The picture gradually drifts from reality\n· Your real battles remain unaffected" "\f"
        "[Watch and refresh]\n· PC: hold V; number keys switch teammates\n· Click Watch to keep watching\n· Tap Refresh to get a new snapshot\n· Wait 5s between new requests\n· Tap Back or press Esc to return" "\f"
        "[Install and play]\n· Update all teammates together\n· Team buffs, nameplates and goals stay\n· PC: extract all, run the launch BAT\n· Android: install over; do not uninstall"); }
}
