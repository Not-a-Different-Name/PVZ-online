#include "RunBuffs.h"

// 文案一律英文：位图字体没有中文字形（同 M2 的所有新界面）。
static const RunBuffDef gRunBuffDefs[RUN_BUFF_COUNT] =
{
	{ "Firepower",    "All shots deal +10% damage" },
	{ "Deep Roots",   "All plants gain +20% health" },
	{ "Abundance",    "Sun plants produce 20% faster" },
	{ "Swift Strikes","Plants attack 10% faster" },
	{ "Quick Seeds",  "Plant recharge is 15% faster" },
	{ "Reserves",     "Start each level with +25 sun" },
	{ "Skyfall",      "Sun falls from the sky 20% faster" },
	{ "Demolition",   "Instant plants deal +30% damage" },
};

const RunBuffDef& GetRunBuffDef(int theId)
{
	if (theId < 0 || theId >= RUN_BUFF_COUNT) return gRunBuffDefs[0];
	return gRunBuffDefs[theId];
}
