#ifndef __RUNBUFFS_H__
#define __RUNBUFFS_H__

#include "../../ConstEnums.h"

// @pvz-online: 闯关（肉鸽）增益目录，两类都在这：
//   ① 全局增益（RUN_BUFF_*，对全部植物/整局生效）
//   ② 单株升级（RUN_UPGRADE_*，只对已拥有的那株植物生效，抽取时过滤）
// 两张表既是文案（三选一屏 R2 用它摆名字和说明），也是数值（R3：每层多少，
// 落点各自的代码位置见 RunBuffDef 数值字段与 LawnApp::RunPlantUpgradeMul/Count）。
//
// 两类共用同一个 id 空间：id < RUN_BUFF_COUNT 是全局，否则 id − RUN_BUFF_COUNT
// 是单株表下标。存储（RunState 的 BuffStack）与检查点格式因此不用区分两类。
// 2026-10-03 起条目多了两个维度（方案 docs/06 §2）：层数上限（mMaxStacks，叠满不再
// 进候选）与叠乘模式（mMultiplicative，(1+每层)^层数）——都是表内静态属性，不进检查点。

enum RunBuffId
{
	RUN_BUFF_FIREPOWER,		// 火力强化：全体子弹伤害 +30%（无限）
	RUN_BUFF_ROOTED,		// 扎根：全体植物血量 +50%（无限）
	RUN_BUFF_ABUNDANCE,		// 丰饶：产阳光间隔 −20%（至多 4 层）
	RUN_BUFF_SWIFT,			// 急袭：攻击间隔叠乘 ×0.8/层（无限）
	RUN_BUFF_FASTSEED,		// 速种：种植冷却叠乘 ×0.8/层（无限）
	RUN_BUFF_RESERVE,		// 储备：每关开局阳光 +50（无限）
	RUN_BUFF_SKYFALL,		// 天降：天上掉阳光间隔 −20%（至多 4 层）
	RUN_BUFF_BLAST,			// 爆破：一次性植物伤害 +30%（无限）
	RUN_BUFF_COUNT
};

struct RunBuffDef
{
	const char*	mName;
	const char*	mDesc;
	// 每层的乘数修正：线性条目 = 1 + mPerStackMul × 层数（+0.10 = ×1.10，−0.20 = ×0.80）；
	// 叠乘条目（mMultiplicative）= (1 + mPerStackMul)^层数。0 = 这条不是乘数型。
	// 取用走 LawnApp::RunBuffMul，非闯关局自动是 1.0。
	float		mPerStackMul;
	// 每层的绝对值加成（储备 +50 阳光）。0 = 不是加成型。取用走 LawnApp::RunBuffAdd。
	int			mPerStackAdd;
	// @pvz-online: 层数上限：叠到这么多层后不再进候选（0 = 无限）。已超限的旧档保留层数、
	// 不追溯削减；过滤在 RunState::RollChoices。
	int			mMaxStacks;
	// @pvz-online: 叠乘模式（方案 §2.3）：true = 层数按 (1+mPerStackMul)^层数 几何叠乘。
	bool		mMultiplicative;
};

const RunBuffDef& GetRunBuffDef(int theId);

// ── 单株升级（R3 第二张表）────────────────────────────────────────────
// 只对"卡池里已有这株植物"的玩家出（抽取时过滤，见 RunState::RollChoices）。
// 落点取用走 LawnApp::RunPlantUpgradeMul / RunPlantUpgradeCount——和全局 buff
// 一样，非闯关局自动是中性值，落点不需要判 mRunState。
enum RunPlantUpgradeId
{
	RUN_UPGRADE_PEASHOOTER,		// 豌豆射手：每次多打 1 发（每层）
	RUN_UPGRADE_SUNFLOWER,		// 向日葵：每次多产 1 阳光（每层）
	RUN_UPGRADE_CHERRYBOMB,		// 樱桃炸弹：爆炸半径 +25%/层
	RUN_UPGRADE_WALLNUT,		// 坚果墙：血量 +25%/层
	RUN_UPGRADE_POTATOMINE,		// 土豆雷：爆炸伤害 +40%/层
	RUN_PLANT_UPGRADE_COUNT
};

struct RunPlantUpgradeDef
{
	SeedType	mPlant;			// 这门升级挂在哪种植物上（一株最多一条）
	const char*	mName;
	const char*	mDesc;
	// 同 RunBuffDef.mPerStackMul；纯计数型（+1 发 / +1 阳光）填 0，
	// 那种效果按层数直接取整，走 RunPlantUpgradeCount。
	float		mPerStackMul;
	// @pvz-online: 同 RunBuffDef.mMaxStacks（0 = 无限）与 mMultiplicative。
	int			mMaxStacks;
	bool		mMultiplicative;
};

const RunPlantUpgradeDef& GetRunPlantUpgradeDef(int theIndex);
// 这株植物在单株表里的下标；表里没有这株 → −1。
int RunPlantUpgradeIndexFor(SeedType thePlant);

// 三选一屏统一取文案：两类 id 都能查，屏上不用分支。
const char* GetRunChoiceName(int theId);
// 封顶条目（mMaxStacks > 0）的返回值尾部带「，至多 N 层」（机械追加，见实现处）。
// 返回进程内静态缓冲——取到就画，别存指针。
const char* GetRunChoiceDesc(int theId);
// 这条条目封顶几层（0 = 无限）；抽取过滤（RunState::RollChoices）与屏上「已有 x/N」用它。
int GetRunChoiceMaxStacks(int theId);

#endif
