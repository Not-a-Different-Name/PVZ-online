#ifndef __QUICKCHAT_H__
#define __QUICKCHAT_H__

// @pvz-online: 局内快捷聊天（快捷短语 + 植物表情）的编号表。
// 线路上只传编号（MSG_QUICK_CHAT 的 u8 id），文字与植物卡图各机本地查这张表。
// **编号 = 线路身份**：改文案、换表情植物随便改（不用抬 MOD_BUILD）；
// 动编号（增删、改顺序）必须两边一起动并抬 MOD_BUILD——旧表把 5 号念成另一句话。
// 另见 NetProtocol.h 里 18 → 19 的版本注释。

#include <cstdint>

#include "../../ConstEnums.h"

namespace QuickChat
{

// id 1..8 = 快捷短语（PHRASES[id - 1]）。UTF-8，绘制侧自行转 ANSI/宽字符。
// 中英各一张表（语言批 2026-10-03）：按**本机语言**择一显示（各机各译，不走协议——
// 编号才是线路身份）；选择在绘制侧做（ModText::Tr），这张头文件保持零依赖。
const int		PHRASE_COUNT		= 8;
const char* const	PHRASES[PHRASE_COUNT] =
{
	"快来帮我",
	"我这边守得住",
	"一大波僵尸来了",
	"我没阳光了",
	"干得好",
	"对不起",
	"我要用樱桃炸弹了",
	"集合到我这"
};
const char* const	PHRASES_EN[PHRASE_COUNT] =
{
	"Help me!",
	"I can hold this side",
	"Huge wave incoming",
	"I'm low on sun",
	"Well done",
	"Sorry",
	"Cherry Bomb incoming",
	"Rally to me"
};

// id 9..16 = 植物表情（EMOTE_SEEDS[id - 9]）。用户钦定的 8 株，顺序即编号顺序，
// 棋盘按 SeedPacketDrawSeed 画同款种子卡图。
const int		EMOTE_COUNT			= 8;
const SeedType	EMOTE_SEEDS[EMOTE_COUNT] =
{
	SEED_POTATOMINE,
	SEED_SUNFLOWER,
	SEED_CATTAIL,
	SEED_CHERRYBOMB,
	SEED_SQUASH,
	SEED_GARLIC,
	SEED_JALAPENO,
	SEED_DOOMSHROOM
};
const char* const	EMOTE_NAMES[EMOTE_COUNT] =
{
	"土豆雷",
	"向日葵",
	"猫尾草",
	"樱桃炸弹",
	"窝瓜",
	"大蒜",
	"火爆辣椒",
	"毁灭菇"
};
const char* const	EMOTE_NAMES_EN[EMOTE_COUNT] =
{
	"Potato Mine",
	"Sunflower",
	"Cattail",
	"Cherry Bomb",
	"Squash",
	"Garlic",
	"Jalapeno",
	"Doom-shroom"
};

const int		TOTAL_COUNT			= PHRASE_COUNT + EMOTE_COUNT;

inline bool IsValidId(int theId)
{
	return theId >= 1 && theId <= TOTAL_COUNT;
}

inline bool IsEmoteId(int theId)
{
	return theId > PHRASE_COUNT;
}

}

#endif
