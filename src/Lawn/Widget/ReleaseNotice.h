#pragma once
#include "../ModText.h"
namespace ReleaseNotice {
inline const char* Body() { return ModText::Tr(
        "【开始联机】\n· 房主点「开始游戏」，队友选「加入」进场\n· 队友选「暂不」后，房主可随时重新开始\n· 复制房码或邀请，好友粘贴即可加入\n· 全队回到菜单后，可在原房直接重开" "\f"
        "【观战】\n· 同房观战可自动直连，持续更新草坪画面\n· 画面停留时，点「刷新」获取新快照\n· 再次进入、换人或刷新需间隔 5 秒\n· 快照会在本机播放，可能偏离最新战况\n· 电脑按 V 观战或切换，数字键选择队友\n· 按 R 刷新；点「返回」或按 Esc 退出" "\f"
        "【快捷操作】\n· 房内点「观战」，查看队友的草坪\n· H / J 显示或收起僵尸 / 植物血量\n· T 或 E 打开快捷消息面板\n· G 进入送阳光，数字 1–6 选择队友\n· B 打开协作面板" "\f"
        "【成长与收藏】\n· 全队增益：军备共享、胜利驰援、跨场火力\n· 僵尸强化：七种新强化登场\n· 铜叶、银叶、金叶名牌可供收藏展示\n· 查看成就、称号，展示自己的成长\n· 三选一重摇时，可锁定一张候选" "\f"
        "【修复】\n· 修复了一些已知问题",
        "[Playing together]\n· Host picks Start; teammates pick Join\n· If a teammate picks Not now, start again anytime\n· Copy the room code or invite; friends paste to join\n· Back at the menu? Restart in the same room" "\f"
        "[Watching]\n· Same-room direct links can keep pictures updating\n· If the picture stays still, click Refresh\n· Wait 5s between entering, switching or refreshing\n· Local snapshot playback may differ from live play\n· PC: V to watch or switch; number keys pick a teammate\n· R to refresh; Back or Esc to leave" "\f"
        "[Quick controls]\n· Click Watch in the room to view a teammate\n· H / J: toggle zombie / plant health numbers\n· T or E: open quick messages\n· G: send sun; then 1–6 to pick a teammate\n· B: open the co-op panel" "\f"
        "[Growth and collection]\n· Team buffs: Armory share, Victory reinforce,\n  and Cross-field firepower\n· Seven new zombie enhancements\n· Collect Bronze, Silver and Gold nameplates\n· View achievements and titles; show your progress\n· Lock one candidate when rerolling picks" "\f"
        "[Fixes]\n· Fixed some known issues"); }
}
