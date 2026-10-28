#pragma once
#include "../ModText.h"
namespace ReleaseNotice {
inline const char* Body() { return ModText::Tr(
        "【无尽新开与继续】\n· 无尽入口可新开一局，也可继续旧局\n· 多局分别保留，列表可翻页选择\n· 继续时回到保存的关卡开局\n· 金币、成就等累计记录照常保留\n· 旧版无尽进度可继续使用" "\f"
        "【无尽联机】\n· 房主选择要继续的无尽局\n· 全队准备后一起进入共同关卡\n· 没有对应进度的队友可补位加入\n· 单人无尽也可邀请队友一起玩\n· 草坪和观战画面显示实际关数" "\f"
        "【观战与重连】\n· 点「观战」查看队友，可刷新或换人\n· 获取画面时仍可操作自己的草坪\n· 等待归队时可观战、打开房间或菜单\n· 关闭房间提示后不会反复弹出\n· 电脑V观战、数字选人、R刷新\n· 手机点对应按钮；刷新间隔5秒" "\f"
        "【P2P观战设置】\n· 选项→帮助→帮助设置→P2P观战\n· 默认关闭，可按需开启\n· 开启后可尝试直连，持续观看队友\n· 关闭后仍可查看和刷新观战画面\n· 队友请统一更新至本版" "\f"
        "【修复】\n· 修复了一些已知问题",
        "[Endless: new or continue]\n· Start a new run or continue a saved one\n· Keep multiple runs; browse the saved-run pages\n· Continue from the saved stage opening\n· Coins and lifetime achievements remain yours\n· Existing endless saves can still be used" "\f"
        "[Endless together]\n· The host chooses the endless run to continue\n· Ready up and enter the shared stage together\n· Teammates without matching progress can join\n· Invite teammates into a solo endless run\n· Your lawn and spectator view show the actual stage" "\f"
        "[Watching and rejoining]\n· Watch a teammate; refresh or switch players\n· Keep playing while a picture is being fetched\n· While waiting, use Watch, Room or Menu\n· Closing the room prompt keeps it closed\n· PC: V to watch, numbers to pick, R to refresh\n· Mobile: tap the buttons; refresh every 5s" "\f"
        "[P2P spectator setting]\n· Options > Help > Help settings > P2P watching\n· Off by default; enable it if you wish\n· When on, try direct links for continuing pictures\n· When off, viewing and refreshing still work\n· All teammates should update to this version" "\f"
        "[Fixes]\n· Fixed some known issues"); }
}
