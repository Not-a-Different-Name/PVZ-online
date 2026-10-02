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

// 单株升级。数值列要和 mDesc 里的百分比对得上，改一边就改另一边。
static const RunPlantUpgradeDef gRunPlantUpgradeDefs[RUN_PLANT_UPGRADE_COUNT] =
{
	{ SeedType::SEED_PEASHOOTER,  "Pea Volley",  "Peashooter fires +1 pea per stack",       0.00f },
	{ SeedType::SEED_SUNFLOWER,   "Rich Bloom",  "Sunflower makes +1 sun per stack",        0.00f },
	{ SeedType::SEED_CHERRYBOMB,  "Wide Blast",  "Cherry Bomb blast +25% wider per stack",  0.25f },
	{ SeedType::SEED_WALLNUT,     "Thick Shell", "Wall-nut gains +25% health per stack",    0.25f },
	{ SeedType::SEED_POTATOMINE,  "Deep Charge", "Potato Mine deals +40% damage per stack", 0.40f },
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
