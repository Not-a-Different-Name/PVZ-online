#include "RunBuffs.h"
#include <string>

// 说明文案是中文，只走 RunPickDialog 的 SysFont（GDI）路径——位图字体没有中文字形；
// 按钮上的名字（mName）保持英文，仍走位图字体。
// 数值列要和 mDesc 里的百分比对得上，改一边就改另一边；封顶条目的「，至多 N 层」
// 由 GetRunChoiceDesc 按 mMaxStacks 机械追加，别手写进 mDesc。
// 结构体尾部省略的字段 = mMaxStacks 0（无限）、mMultiplicative false（线性）。
// 2026-10-03（方案 §三 定案）：全局 8 条整表重标——火力 30%、扎根 50%、丰饶封顶 4、
// 急袭/速种改叠乘 ×0.8、储备 50、天降封顶 4、爆破不变。
static const RunBuffDef gRunBuffDefs[RUN_BUFF_COUNT] =
{
	{ "Firepower",    "所有子弹伤害 +30%",        0.30f,  0 },
	{ "Deep Roots",   "所有植物血量 +50%",        0.50f,  0 },
	{ "Abundance",    "产阳光植物更快 20%",      -0.20f,  0, 4 },
	{ "Swift Strikes","植物攻击间隔逐层 ×0.8",    -0.20f,  0, 0, true },
	{ "Quick Seeds",  "种植冷却逐层 ×0.8",       -0.20f,  0, 0, true },
	{ "Reserves",     "每关开局 +50 阳光",        0.00f, 50 },
	{ "Skyfall",      "天降阳光更快 20%",        -0.20f,  0, 4 },
	{ "Demolition",   "一次性植物伤害 +30%",      0.30f,  0 },
};

const RunBuffDef& GetRunBuffDef(int theId)
{
	if (theId < 0 || theId >= RUN_BUFF_COUNT) return gRunBuffDefs[0];
	return gRunBuffDefs[theId];
}

// 单株升级。数值列要和 mDesc 里的百分比对得上，改一边就改另一边。
// （每层）前面的 \n 是排版用的显式换行，见 RunPickDialog 的中文排版。
// 顺序纪律（方案 §2.1/Q7）：按 SeedType 升序排，前 5 条（SeedType 0..4）永远留在原位；
// 新增条目插在自己的 SeedType 位次上。批 1 的 5 条是追加（SeedType 都大于 4）；批 2 的
// 4 条插进中段（双发 7 / 杨桃 29 / 卷心菜 32 / 机枪 40——射速族共用 Plant.cpp:956 的节奏
// 计数器，全局「急袭」同挂点）；批 3 的 3 条（阳光菇 9 / 金盏花 38 / 双子向日葵 41——
// 产出族，UpdateProductionPlant 各分支加「每层多落一枚」循环）；批 4 的 2 条（火爆辣椒
// 20 / 海蘑菇 24——种植冷却，SeedPacket.cpp:884 同挂点）；批 5 的 3 条（毁灭菇 15 / 窝瓜
// 17 / 三线 18——毁灭菇照樱桃的半径乘法，窝瓜走 DoSquashDamage 里 ApplyButter 眩晕，三线
// 纯表：每道一次 Fire，Plant.cpp:4790 的多发循环按株取数）；批 6 的 2 条（寒冰射手 5 /
// 寒冰菇 14——寒冰射手：雪豆命中后在 Projectile::DoImpact 单体分支把 ApplyChill 挂上的
// 1000 帧按单株乘数放宽（==1000 认出、更长的减速不动）；寒冰菇：Zombie::HitIceTrap 三档
// 冻结各 +200 帧/层，唯一调用者就是 IceZombies）；批 7 的 2 条（小喷菇 8 / 大喷菇 10——射程族，
// 挂点都在 Plant::GetPlantAttackRect 的攻击矩形：小喷菇带层即同 default 支一路铺到板尾（同 800px，
// FindTargetZombie 拿这矩形当开火门，弹道本身无射程上限），大喷菇每层 +80px；同批土豆雷 4 改
// 「Wide Charge」：效果从爆炸直伤 +40% 换成半径 +25%/层——直伤乘数从
// Board::KillAllZombiesInRadius 摘除，半径乘数落 Plant::DoSpecial 土豆雷支）。表内下标随插入右移：批 4
// 档里 id 16..26、批 5 档里 15/17/18、批 6 档里 15..31 的层数会错位到别的植物（批 1/批 2/批 3 档同理；开发期接受，见方案 §六）。
// 血量型条目（批 1 的 5 条 + 坚果墙）不用挂点——Plant.cpp:484 对任意株统一乘
// RunPlantUpgradeMul(mSeedType)，表里加一行就生效（南瓜头护罩血已查证同走 mPlantHealth）。
static const RunPlantUpgradeDef gRunPlantUpgradeDefs[RUN_PLANT_UPGRADE_COUNT] =
{
	{ SeedType::SEED_PEASHOOTER,   "Pea Volley",   "豌豆射手每次多发 1 颗\n（每层）",   0.00f },
	{ SeedType::SEED_SUNFLOWER,    "Rich Bloom",   "向日葵每次多产 1 阳光\n（每层）",   0.00f, 3 },
	{ SeedType::SEED_CHERRYBOMB,   "Wide Blast",   "樱桃炸弹爆炸范围 +25%\n（每层）",   0.25f },
	{ SeedType::SEED_WALLNUT,      "Thick Shell",  "坚果墙血量 +50%\n（每层）",         0.50f },
	{ SeedType::SEED_POTATOMINE,   "Wide Charge",  "土豆雷爆炸范围 +25%\n（每层）",     0.25f },
	{ SeedType::SEED_SNOWPEA,      "Frostbite",    "命中减速时长 +30%\n（每层）",       0.30f, 3 },
	{ SeedType::SEED_REPEATER,     "Quick Rhythm", "射击间隔逐层 ×0.75\n（每层）",      -0.25f, 3, true },
	{ SeedType::SEED_PUFFSHROOM,   "Far Spore",    "射程变为无限",                     0.00f, 1 },
	{ SeedType::SEED_SUNSHROOM,    "Bright Cap",   "每次多产 1 阳光\n（每层）",         0.00f, 3 },
	{ SeedType::SEED_FUMESHROOM,   "Thick Fumes",  "雾气射程 +1 格\n（每层）",          0.00f, 2 },
	{ SeedType::SEED_ICESHROOM,    "Deep Freeze",  "全场冰冻 +2 秒\n（每层）",          0.00f, 2 },
	{ SeedType::SEED_DOOMSHROOM,   "Annihilation", "爆炸范围 +25%\n（每层）",           0.25f, 2 },
	{ SeedType::SEED_LILYPAD,      "Tough Pad",    "血量 +100%\n（每层）",              1.00f, 2 },
	{ SeedType::SEED_SQUASH,       "Heavy Squash", "压击处僵尸眩晕 +2 秒\n（每层）",    0.00f, 2 },
	{ SeedType::SEED_THREEPEATER,  "Triple Volley", "每条道多发 1 颗\n（每层）",         0.00f, 2 },
	{ SeedType::SEED_JALAPENO,     "Inferno",      "种植冷却逐层 ×0.75\n（每层）",      -0.25f, 3, true },
	{ SeedType::SEED_TALLNUT,      "Iron Shell",   "血量 +50%\n（每层）",               0.50f, 3 },
	{ SeedType::SEED_SEASHROOM,    "Brine Spore",  "种植冷却逐层 ×0.75\n（每层）",      -0.25f, 3, true },
	{ SeedType::SEED_STARFRUIT,    "Star Rain",    "射击间隔逐层 ×0.75\n（每层）",      -0.25f, 3, true },
	{ SeedType::SEED_PUMPKINSHELL, "Hard Rind",    "血量 +50%\n（每层）",               0.50f, 3 },
	{ SeedType::SEED_CABBAGEPULT,  "Heavy Toss",   "投掷间隔逐层 ×0.75\n（每层）",      -0.25f, 3, true },
	{ SeedType::SEED_FLOWERPOT,    "Rich Soil",    "血量 +100%\n（每层）",              1.00f, 2 },
	{ SeedType::SEED_UMBRELLA,     "Canopy",       "血量 +50%\n（每层）",               0.50f, 3 },
	{ SeedType::SEED_MARIGOLD,     "Golden Bloom", "每次多产 1 枚\n（每层）",           0.00f, 3 },
	{ SeedType::SEED_GATLINGPEA,   "Rapid Fire",   "射击间隔逐层 ×0.75\n（每层）",      -0.25f, 3, true },
	{ SeedType::SEED_TWINSUNFLOWER, "Twin Bloom",  "每次多产 1 阳光\n（每层）",         0.00f, 3 },
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

// 封顶条目的说明尾部机械追加「，至多 N 层」（方案 §2.5）：基础文案只管效果，
// 上限只在 mMaxStacks 一处维护。返回静态缓冲——屏上取到就画，别存指针。
const char* GetRunChoiceDesc(int theId)
{
	const char* aDesc;
	int aMaxStacks;
	if (theId >= RUN_BUFF_COUNT)
	{
		const RunPlantUpgradeDef& aDef = GetRunPlantUpgradeDef(theId - RUN_BUFF_COUNT);
		aDesc = aDef.mDesc;
		aMaxStacks = aDef.mMaxStacks;
	}
	else
	{
		const RunBuffDef& aDef = GetRunBuffDef(theId);
		aDesc = aDef.mDesc;
		aMaxStacks = aDef.mMaxStacks;
	}
	if (aMaxStacks <= 0) return aDesc;

	static std::string sCappedDesc;
	sCappedDesc = aDesc;
	sCappedDesc += "，至多 ";
	sCappedDesc += std::to_string(aMaxStacks);
	sCappedDesc += " 层";
	return sCappedDesc.c_str();
}

// 这条条目封顶几层（0 = 无限）。抽取过滤与屏上「已有 x/N」都走它。
int GetRunChoiceMaxStacks(int theId)
{
	if (theId >= RUN_BUFF_COUNT) return GetRunPlantUpgradeDef(theId - RUN_BUFF_COUNT).mMaxStacks;
	return GetRunBuffDef(theId).mMaxStacks;
}

// 单株升级列顶那行【植物名】（2026-10-03 玩家反馈）：按钮名字是英文位图字体塞不下中文，
// 说明文案批 2 起多数也不含植物名（「射击间隔逐层 ×0.75」这种）——不单独标出来，
// 屏上分不清这条 buff 是哪株的。按 SeedType 查（表里一株最多一条）；新批次往表里
// 插行时这里同步加名字。全局增益（或漏了名字）→ NULL，屏上不画标题行。
static const struct { SeedType mPlant; const char* mName; } gRunPlantUpgradeZhNames[] =
{
	{ SeedType::SEED_PEASHOOTER,   "豌豆射手" },
	{ SeedType::SEED_SUNFLOWER,    "向日葵" },
	{ SeedType::SEED_CHERRYBOMB,   "樱桃炸弹" },
	{ SeedType::SEED_WALLNUT,      "坚果墙" },
	{ SeedType::SEED_POTATOMINE,   "土豆雷" },
	{ SeedType::SEED_SNOWPEA,      "寒冰射手" },
	{ SeedType::SEED_REPEATER,     "双发射手" },
	{ SeedType::SEED_PUFFSHROOM,   "小喷菇" },
	{ SeedType::SEED_SUNSHROOM,    "阳光菇" },
	{ SeedType::SEED_FUMESHROOM,   "大喷菇" },
	{ SeedType::SEED_ICESHROOM,    "寒冰菇" },
	{ SeedType::SEED_DOOMSHROOM,   "毁灭菇" },
	{ SeedType::SEED_LILYPAD,      "睡莲" },
	{ SeedType::SEED_SQUASH,       "窝瓜" },
	{ SeedType::SEED_THREEPEATER,  "三线射手" },
	{ SeedType::SEED_JALAPENO,     "火爆辣椒" },
	{ SeedType::SEED_TALLNUT,      "高坚果" },
	{ SeedType::SEED_SEASHROOM,    "海蘑菇" },
	{ SeedType::SEED_STARFRUIT,    "杨桃" },
	{ SeedType::SEED_PUMPKINSHELL, "南瓜头" },
	{ SeedType::SEED_CABBAGEPULT,  "卷心菜投手" },
	{ SeedType::SEED_FLOWERPOT,    "花盆" },
	{ SeedType::SEED_UMBRELLA,     "保护伞" },
	{ SeedType::SEED_MARIGOLD,     "金盏花" },
	{ SeedType::SEED_GATLINGPEA,   "机枪射手" },
	{ SeedType::SEED_TWINSUNFLOWER,"双子向日葵" },
};

const char* GetRunChoicePlantName(int theId)
{
	if (theId < RUN_BUFF_COUNT) return NULL;
	SeedType aPlant = GetRunPlantUpgradeDef(theId - RUN_BUFF_COUNT).mPlant;
	for (int i = 0; i < (int)(sizeof(gRunPlantUpgradeZhNames) / sizeof(gRunPlantUpgradeZhNames[0])); i++)
	{
		if (gRunPlantUpgradeZhNames[i].mPlant == aPlant) return gRunPlantUpgradeZhNames[i].mName;
	}
	return NULL;
}
