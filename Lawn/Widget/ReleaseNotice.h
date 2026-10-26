#pragma once
#include "../ModText.h"
namespace ReleaseNotice {
inline const char* Body() { return ModText::Tr(
        "【团队增益】\n· 新增战前补给、坚固阵线、快速整备\n· 新增阳光接力、末位救援\n· 补给每层50阳光；阵线每层生命+30%\n· 整备每层种子冷却缩短5%\n· 普通增益最多3层，共享与救援最多1层\n· 无尽模式禁用军备共享" "\f"
        "【团队协作】\n· 驰援每层200阳光，并刷新全部种子卡\n· 阳光接力：付100，到账150/175/200\n· 收到的赠送阳光不能再次赠送\n· 末位指漏怪传递链末位\n· 每关补回末位已用小推车，重开不重复补\n· 战前补给每关发放，重开不重复领取" "\f"
        "【樱桃支援】\n· 拥有樱桃且队友允许时，可赠送支援卡\n· 赠送者付一次樱桃费用，独立冷却20秒\n· 队友点击卡片，自行选择种植位置\n· 不占种子栏，使用不再花阳光\n· 每人限持一张，仅本关有效\n· 继承赠送者强化，每层结算一次爆炸" "\f"
        "【联机与观战】\n· 全队统一更新，复制房码或邀请即可加入\n· 同房观战可自动直连，持续更新草坪\n· 画面停留时点刷新，操作间隔5秒\n· 快照本机播放，可能偏离最新战况\n· 电脑：V观战、数字选人、R刷新、B协作\n· 手机：点击对应按钮操作" "\f"
        "【修复】\n· 修复了一些已知问题",
        "[Team buffs]\n· New: Battle supplies, Fortified lines, Quick preparation\n· New: Sun relay and Last-seat rescue\n· Supplies: +50 sun/stack; Fortified lines: +30% health/stack\n· Quick preparation: 5% shorter seed cooldown per stack\n· Max 3 stacks; Shared arsenal and Last-seat rescue: max 1\n· Shared arsenal is disabled in endless" "\f"
        "[Teamwork]\n· Victory relief: 200 sun/stack and a full seed refresh\n· Sun relay: pay 100, deliver 150/175/200\n· Received gift sun cannot be gifted again\n· Last seat means the end of the zombie relay chain\n· Restore spent mowers each stage; no extra retry refresh\n· Battle supplies arrive once per stage, not again on retry" "\f"
        "[Cherry aid]\n· Own Cherry Bomb and get the teammate's consent\n· Giver pays one Cherry cost; separate 20s cooldown\n· Recipient taps the card and chooses where to plant\n· No seed-bank slot or extra sun needed to use it\n· Hold one card per player; expires at stage end\n· Uses giver's upgrades; one blast per stack" "\f"
        "[Playing and watching]\n· Everyone updates; copy a room code or invite to join\n· Same-room direct links can keep lawn pictures updating\n· If the picture stops, refresh; wait 5s between actions\n· Local snapshot playback may differ from live play\n· PC: V to watch, numbers to pick, R refresh, B co-op\n· Mobile: tap the corresponding buttons" "\f"
        "[Fixes]\n· Fixed some known issues"); }
}
