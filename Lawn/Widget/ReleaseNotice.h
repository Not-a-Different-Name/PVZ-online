#pragma once
#include "../ModText.h"
namespace ReleaseNotice {
inline const char* Body() { return ModText::Tr(
        "【队友操作】\n· 点队友名字：送阳光、送樱桃、观战\n· 名单旁可求阳光、开关接收樱桃\n· 选卡时点名字可直接借植物\n· 借用申请直接点同意或拒绝" "\f"
        "【开局与重连】\n· 修复慢选队友被提前跳过的问题\n· 短暂掉线保留本轮选择与投票\n· 修复同轮重复开始提示和重复选增益\n· 完成本人选择即可投票，没有倒计时" "\f"
        "【无尽存档】\n· 开始新无尽与继续存档分别进入\n· 多局独立保存，可翻页选择\n· 联机由房主选局，全队准备后进入\n· 缺少对应进度的队友可以补位" "\f"
        "【查看与观战】\n· 增益→全队增益查看名称、层数和效果\n· 点队友名字观战，可刷新或换人\n· 电脑V观战、R刷新，手机点按钮\n· P2P观战默认关闭，可在帮助设置开启" "\f"
        "【更新与联机】\n· 请全队统一更新至MOD104\n· 游戏自动选服，只需四位房间码\n· 安卓覆盖安装，不要先卸载\n· 修复了一些已知问题",
        "[Teammate actions]\n· Tap a name to give sun, send Cherry or watch\n· Request sun or toggle Cherry receiving beside the list\n· Borrow plants directly during seed selection\n· Accept or decline loan requests in the small strip" "\f"
        "[Starting and reconnecting]\n· Slow choosers are no longer skipped early\n· Brief disconnects keep choices and vote eligibility\n· Fixed repeated start prompts and reward selection\n· Vote after your choices, without a countdown" "\f"
        "[Endless saves]\n· Start new or continue an existing run\n· Keep multiple runs and browse saved pages\n· The host chooses a run; everyone readies up\n· Teammates without matching progress can join" "\f"
        "[Buffs and watching]\n· Buffs > Team buffs shows earned effects and stacks\n· Tap a teammate name to watch, refresh or switch\n· PC: V to watch and R to refresh; mobile: tap buttons\n· P2P watching is off by default in Help settings" "\f"
        "[Updating and joining]\n· Everyone should update to MOD104\n· Automatic server choice; share the four-character room code\n· Android: install over the existing app\n· Fixed some known issues"); }
}
