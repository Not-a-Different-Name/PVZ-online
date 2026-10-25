#pragma once
#include "../ModText.h"
namespace ReleaseNotice {
inline const char* Body() { return ModText::Tr(
        "【MOD97：观战连接优化】\n· 同房观战可自动建立玩家间直连\n· 连接顺畅时持续更新队友画面\n· 连接中断后继续快照与本机播放\n· 直连双方可见彼此IP\n· 不影响任何人的真实战斗" "\f"
        "【观看与刷新】\n· 电脑按住V，数字键切换队友\n· 点击观战按钮可持续观看\n· 点击刷新获取新的快照\n· 换人或刷新请间隔5秒\n· 点击返回或按Esc退出" "\f"
        "【安装与游玩】\n· 全体队友请统一更新\n· 保留全队增益、名牌和成就\n· 电脑完整解压后运行启动游戏.bat\n· 安卓覆盖安装，请勿先卸载",
        "[MOD97: spectator connections]\n· Same-room viewers can connect directly\n· Good connections update teammate pictures\n· On loss, local snapshot playback continues\n· Direct peers can see each other's IP\n· Your real battles remain unaffected" "\f"
        "[Watch and refresh]\n· PC: hold V; number keys switch teammates\n· Click Watch to keep watching\n· Tap Refresh to get a new snapshot\n· Wait 5s between new requests\n· Tap Back or press Esc to return" "\f"
        "[Install and play]\n· Update all teammates together\n· Team buffs, nameplates and goals stay\n· PC: extract all, run the launch BAT\n· Android: install over; do not uninstall"); }
}
