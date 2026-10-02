#include "RunBuffs.h"

// 文案一律英文：位图字体没有中文字形（同 M2 的所有新界面）。
// 数值两列要和 mDesc 里的百分比对得上，改一边就改另一边。
static const RunBuffDef gRunBuffDefs[RUN_BUFF_COUNT] =
{
	{ "Firepower",    "All shots deal +10% damage",        0.10f,  0 },
	{ "Deep Roots",   "All plants gain +20% health",       0.20f,  0 },
	{ "Abundance",    "Sun plants produce 20% faster",    -0.20f,  0 },
	{ "Swift Strikes","Plants attack 10% faster",         -0.10f,  0 },
	{ "Quick Seeds",  "Plant recharge is 15% faster",     -0.15f,  0 },
	{ "Reserves",     "Start each level with +25 sun",     0.00f, 25 },
	{ "Skyfall",      "Sun falls from the sky 20% faster",-0.20f,  0 },
	{ "Demolition",   "Instant plants deal +30% damage",   0.30f,  0 },
};

const RunBuffDef& GetRunBuffDef(int theId)
{
	if (theId < 0 || theId >= RUN_BUFF_COUNT) return gRunBuffDefs[0];
	return gRunBuffDefs[theId];
}
