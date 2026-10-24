#pragma once
#include "../ModText.h"
namespace ReleaseNotice {
inline const char* Body() { return ModText::Tr(
        "【MOD94：看见队友的真实草坪】\n· 观战显示植物、僵尸、子弹和特效\n· 可在战斗中随时观看房内队友\n· 画面约延迟1.5秒\n· 网络繁忙时优先保障战斗" "\f"
        "【观看与返回】\n· 电脑按住V观看，数字键切换队友\n· 点击观战按钮可持续观看\n· 手机点击观战按钮选择队友\n· 再点返回，或按Esc退出观看" "\f"
        "【安装与游玩】\n· 全体队友请统一更新\n· 保留全队增益、名牌和成就\n· 电脑完整解压后运行启动游戏.bat\n· 安卓覆盖安装，请勿先卸载",
        "[MOD94: your teammate's real lawn]\n· See plants, zombies, shots and effects\n· Watch teammates during a battle\n· Picture has about a 1.5s buffer\n· Combat gets priority on busy networks" "\f"
        "[Watch and return]\n· PC: hold V, use number keys to switch\n· Click Watch to keep watching\n· Mobile: tap Watch and pick a teammate\n· Tap Back or press Esc to leave" "\f"
        "[Install and play]\n· Update all teammates together\n· Team buffs, nameplates and goals stay\n· PC: extract all, run the launch BAT\n· Android: install over; do not uninstall"); }
}
