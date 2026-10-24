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

// id 1..9 = 快捷短语（PHRASES[id - 1]）。UTF-8，绘制侧自行转 ANSI/宽字符。
// 中英各一张表（语言批 2026-10-03）：按**本机语言**择一显示（各机各译，不走协议——
// 编号才是线路身份）；选择在绘制侧做（ModText::Tr），这张头文件保持零依赖。
const int		PHRASE_COUNT		= 9;
const char* const	PHRASES[PHRASE_COUNT] =
{
	"快点",
	"我已启动",
	"我很阳光^_^",
	"干得漂亮",
	"！？区区？！",
	"那我的魔法猫咪这一块谁补给我啊",
	"你要自己来取，你知道规则的",
	"蹦！蹦！",
	"给我阳光口牙"
};
const char* const	PHRASES_EN[PHRASE_COUNT] =
{
	"Hurry up!",
	"Powered up",
	"Feeling sunny ^_^",
	"Well played",
	"?!Maggot?!",
	"Then who's covering my magical cat?!",
	"Come get it yourself, you know the rules",
	"Bungee! Bungee!",
	"Gimme sun!"
};

// id 10..17 = 植物表情（EMOTE_SEEDS[id - PHRASE_COUNT]）。用户钦定的 8 株，顺序即编号顺序，
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
