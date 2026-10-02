#include "RunBuffs.h"

// 说明文案是中文，只走 RunPickDialog 的 SysFont（GDI）路径——位图字体没有中文字形；
// 按钮上的名字（mName）保持英文，仍走位图字体。
// 数值两列要和 mDesc 里的百分比对得上，改一边就改另一边。
static const RunBuffDef gRunBuffDefs[RUN_BUFF_COUNT] =
{
	{ "Firepower",    "所有子弹伤害 +10%",        0.10f,  0 },
	{ "Deep Roots",   "所有植物血量 +20%",        0.20f,  0 },
	{ "Abundance",    "产阳光植物更快 20%",      -0.20f,  0 },
	{ "Swift Strikes","植物攻击速度 +10%",       -0.10f,  0 },
	{ "Quick Seeds",  "种植冷却缩短 15%",        -0.15f,  0 },
	{ "Reserves",     "每关开局 +25 阳光",        0.00f, 25 },
	{ "Skyfall",      "天降阳光更快 20%",        -0.20f,  0 },
	{ "Demolition",   "一次性植物伤害 +30%",      0.30f,  0 },
};

const RunBuffDef& GetRunBuffDef(int theId)
{
	if (theId < 0 || theId >= RUN_BUFF_COUNT) return gRunBuffDefs[0];
	return gRunBuffDefs[theId];
}

// 单株升级。数值列要和 mDesc 里的百分比对得上，改一边就改另一边。
// （每层）前面的 \n 是排版用的显式换行，见 RunPickDialog 的中文排版。
static const RunPlantUpgradeDef gRunPlantUpgradeDefs[RUN_PLANT_UPGRADE_COUNT] =
{
	{ SeedType::SEED_PEASHOOTER,  "Pea Volley",  "豌豆射手每次多发 1 颗\n（每层）",        0.00f },
	{ SeedType::SEED_SUNFLOWER,   "Rich Bloom",  "向日葵每次多产 1 阳光\n（每层）",        0.00f },
	{ SeedType::SEED_CHERRYBOMB,  "Wide Blast",  "樱桃炸弹爆炸范围 +25%\n（每层）",        0.25f },
	{ SeedType::SEED_WALLNUT,     "Thick Shell", "坚果墙血量 +25%\n（每层）",              0.25f },
	{ SeedType::SEED_POTATOMINE,  "Deep Charge", "土豆雷伤害 +40%\n（每层）",              0.40f },
};

const RunPlantUpgradeDef& GetRunPlantUpgradeDef(int theIndex)
{
	if (theIndex < 0 || theIndex >= RUN_PLANT_UPGRADE_COUNT) return gRunPlantUpgradeDefs[0];
	return gRunPlantUpgradeDefs[theIndex];
}

int RunPlantUpgradeIndexFor(SeedType thePlant)
{
	for (int i = 0; i < RUN_PLANT_UPGRADE_COUNT; i++)
	{
		if (gRunPlantUpgradeDefs[i].mPlant == thePlant) return i;
	}
	return -1;
}

const char* GetRunChoiceName(int theId)
{
	if (theId >= RUN_BUFF_COUNT) return GetRunPlantUpgradeDef(theId - RUN_BUFF_COUNT).mName;
	return GetRunBuffDef(theId).mName;
}

const char* GetRunChoiceDesc(int theId)
{
	if (theId >= RUN_BUFF_COUNT) return GetRunPlantUpgradeDef(theId - RUN_BUFF_COUNT).mDesc;
	return GetRunBuffDef(theId).mDesc;
}
