#include "RunState.h"
#include "RunBuffs.h"
#include <cstring>
#include "../LawnCommon.h"
#include "../../GameConstants.h"
#include "../../Sexy.TodLib/TodDebug.h"
#include "misc/Buffer.h"
#include "misc/MTRand.h"
#include "../../SexyAppFramework/SexyAppBase.h"

// @pvz-online: 闯关状态的实体。检查点格式（小端，x86 直写）：
//   u32 magic 'RUN1' + u16 版本 + u16（低字节 = 时长档，高字节 = 出怪难度档）
//   i32 runSeed + i32 levelIndex + u16 failCounts[25]
//   u16 卡池数 + 每株 u16 SeedType
//   u16 buff 数 + 每条 (u16 id, u16 层数)
//   u16 推车记账（v5） + u16 高级选项位包（v10） + u16 无尽场景档（v12）
// 版本不符 / 越界一律当"没有检查点"——宁可从头开新局，也不带着半截数据进场。
// v2：一局从 5 关扩到 25 关，failCounts 数组跟着变长——v1 的档一律按"没有"处理。
// v3：加时长档（M4-b 三档时长）——旧版的保留位恒 0，恰好就是"完整版"，所以 v2 的档
//     直接按完整版续；载荷长度一个字节没变。
// v4：加出怪难度档（2026-10-03，房主开局前选的那种）——v3 的高字节保留位恒 0，恰好
//     就是"标准"，所以 v3 的档直接按标准续；载荷长度同样没变。
// v5：加末位推车记账（联机末位每行一台、整局一次性）——追加在载荷末尾；v4 及更老的档
//     读不到这两字节，按"一辆都没用"续。
// v6：批 18（2026-10-04）全局表尾插了第 9 条 Precision——全局 id 0..7 不动，单株 id 由
//     「8 + 下标」全体右移 1 变「9 + 下标」；v5 及更老的档读到 id >= 8 的全部 +1 读平
//     （旧档里 id 8 是第一个单株、不是 Precision，不会撞车）。载荷长度一个字节没变。
// v7：批十（2026-10-05）模式表改版——普通档由每场景第 1/3/5 关改抽 1/5 关（=原快速表）、
//     快速档改抽每场景第 5 关：同一个关序号在新表里指向别的引擎关，非完整档的旧检查点没
//     法安全续，一律当"没有检查点"（完整档的表没动，v2..v6 的完整档照续）。载荷长度没变。
// v8：2026-10-06 全局表删了「储备」（原 id 5）——全局 9→8 条，单株 id 由「9 + 下标」全体
//     左移 1 变「8 + 下标」；v7 及更老的档读到 id 5（储备）的丢弃，id >= 6 的全部 −1 读平
//     （v5 及更老的档链条：先按 v6 把 id >= 8 的 +1，再统一 −1）。载荷长度没变。
// v9：2026-10-09 全局表尾追加「排山倒海」（新 id 8）——全局 8→9 条，单株 id 由「8 + 下标」
//     全体右移 1 变「9 + 下标」；v8 及更老的档读到 id >= 8 的全部 +1 读平（旧档里 id 8 是
//     第一个单株、不是排山倒海，不会撞车）。载荷长度没变。
// v10：2026-10-09 批 C「高级选项」——尾追加一个 u16 位包：bit0..1 = 出怪规模档
//     （RUN_SCALE_*）、bit2..3 = 节奏档（RUN_TEMPO_*）、bit4 = 植物僵尸混入开关。
//     v9 及更老的档读不到，按 标准/标准/关 续（与 v3/v4 的保留位缺省同口径）。
// v11：2026-10-09 第二 buff 批 0（docs/06 §8.5）——新增第二单株表，id 区块
//     RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + 表下标（57..95）。buff 列表仍是
//     通用 id+count、载荷长度没变，但老版本会把第二表 id 当老单株解出错误条目，
//     故抬版本让老构建直接拒档；v10 及更老档没有这类 id，读平即可（迁移链不动）。
// v12：2026-10-10 多人无尽——新增第四档 RUN_MODE_ENDLESS（锁场景无限循环、难度走对数阶
//     曲线）与其场景字段 mEndlessScene（0..4）；载荷尾追加 u16 场景档。v11 及更老的档
//     不可能真是无尽档（mode 3 是 v12 才写的），读到 mode==ENDLESS 的一律拒档（伪造/错位）；
//     其余档读平，场景按 0 续。关数校验按 LevelCountForMode(3)=60000 封口（软上限）。

static const unsigned int RUN_CHECKPOINT_MAGIC = 0x314E5552;	// 'RUN1'
static const unsigned short RUN_CHECKPOINT_VERSION = 12;

static std::vector<unsigned char>& AppendU16(std::vector<unsigned char>& theData, unsigned int theValue)
{
	theData.push_back((unsigned char)(theValue & 0xFF));
	theData.push_back((unsigned char)((theValue >> 8) & 0xFF));
	return theData;
}

static std::vector<unsigned char>& AppendI32(std::vector<unsigned char>& theData, int theValue)
{
	AppendU16(theData, (unsigned int)theValue & 0xFFFF);
	AppendU16(theData, ((unsigned int)theValue >> 16) & 0xFFFF);
	return theData;
}

// 读取游标：越界即失败，调用方统一走"检查点不可用"。
class RunReader
{
public:
	RunReader(const unsigned char* theData, int theLength) : mData(theData), mLength(theLength), mOffset(0) {}

	bool		ReadU16(unsigned int& theValue)
	{
		if (mOffset + 2 > mLength) return false;
		theValue = (unsigned int)mData[mOffset] | ((unsigned int)mData[mOffset + 1] << 8);
		mOffset += 2;
		return true;
	}

	bool		ReadU32(unsigned int& theValue)
	{
		unsigned int aLow = 0, aHigh = 0;
		if (!ReadU16(aLow) || !ReadU16(aHigh)) return false;
		theValue = aLow | (aHigh << 16);
		return true;
	}

	bool		ReadI32(int& theValue)
	{
		unsigned int aLow = 0, aHigh = 0;
		if (!ReadU16(aLow) || !ReadU16(aHigh)) return false;
		theValue = (int)(aLow | (aHigh << 16));
		return true;
	}

private:
	const unsigned char*	mData;
	int						mLength;
	int						mOffset;
};

RunState::RunState()
{
	memset(mFailCounts, 0, sizeof(mFailCounts));
	StartNew(0);
}

void RunState::StartNew(int theRunSeed, int theRunMode, int theRunDiff, int theRunScale, int theRunTempo, int theZombotany, int theEndlessScene)
{
	mRunSeed = theRunSeed;
	mMode = theRunMode;
	mDiff = theRunDiff;
	// 无尽档锁定的场景（0..4）：入口链/线传/读档三处都过了闸，这里再夹一次兜底——
	// 坏场景会让 GetLevel 取出越界引擎关号。
	mEndlessScene = (theEndlessScene >= 0 && theEndlessScene < RUN_SCENE_COUNT) ? theEndlessScene : 0;
	mScale = theRunScale;
	mTempo = theRunTempo;
	mZombotany = theZombotany;
	mLevelIndex = 0;
	mPool.clear();
	mPool.push_back(SeedType::SEED_SUNFLOWER);
	mPool.push_back(SeedType::SEED_PEASHOOTER);
	mBuffs.clear();
	memset(mFailCounts, 0, sizeof(mFailCounts));
	mMowerUsedRows = 0;
	mPendingPlantPicks = 0;
	mPendingBuffPicks = 0;
	mChoicesRolled = false;
	mPickReRolled = false;
	mPickSeenBuffCount = 0;		// 候选去重批：新局无露脸/冷却
	mPickCooldownBuffCount = 0;
	mPickCounter = 0;
	// 本机随机盐（用户 2026-10-03 定案：各玩家的候选不共用一套随机数）。不进检查点、
	// 不随联机命令走——pending 屏本来就不落盘，读档/追赶时重抽的屏用什么盐都合法。
	mPickSalt = (unsigned int)Sexy::Rand() ^ ((unsigned int)Sexy::Rand() << 16);
	mCatchUpLevel = -1;
	for (int i = 0; i < RUN_CHOICES; i++)
	{
		mPlantChoices[i] = SeedType::SEED_NONE;
		mBuffChoices[i] = 0;
	}
}

// 一局的两处选卡口：开局先挑四株 + 两个增益（进第 1 关前手里就有 6 株；2026-10-03 多人
// 实测反馈"开局难度略高"后用户定案加厚，原来只挑两株）。批十 2026-10-05：快速版 5 关全是
// 每场景收尾的难关，开局再多给两株两增益（4+2 株、2+2 增益）。每过一关的奖励屏见
// BeginLevelEndPicks。屏的先后由 IsPlantPick 决定：先植物屏、再增益屏。
// 卡池拿满 48 株后植物屏没得抽——那之后只发增益屏。
void RunState::BeginStartPicks()
{
	// 新一批屏从"还没抽"开始（抽干作废过之后又会发新屏，别被上一轮的抽好标志挡住重抽）。
	mChoicesRolled = false;
	mPickReRolled = false;
	// 候选去重批：冷却集只在一批屏内滚动（上一屏 → 下一屏），换批即清。
	mPickSeenBuffCount = 0;
	mPickCooldownBuffCount = 0;
	int aExtra = (mMode == RUN_MODE_QUICK) ? 2 : 0;
	mPendingPlantPicks = CanOfferPlantPick() ? 4 + aExtra : 0;
	mPendingBuffPicks = 2 + aExtra;
	TodLog("[run] start picks: %d plant(s) + %d buff(s) (mode %d)", mPendingPlantPicks, mPendingBuffPicks, mMode);
}

void RunState::BeginLevelEndPicks()
{
	// 新一批屏从"还没抽"开始（同 BeginStartPicks；也覆盖补发追赶 AdvanceCatchUp 的路径）。
	mChoicesRolled = false;
	mPickReRolled = false;
	// 候选去重批：冷却集只在一批屏内滚动（上一屏 → 下一屏），换批即清（同 BeginStartPicks）。
	mPickSeenBuffCount = 0;
	mPickCooldownBuffCount = 0;

	// 每关后的植物奖励数（2026-10-09 定案）：完整 2 株/关不变、普通 6→3、快速 10→4——
	// 大幅收紧让植物保持稀缺（一局拿不满 48 株），单株增益候选池不被一口气铺宽。
	// 植物候选抽干时自动只发增益屏（见 CanOfferPlantPick）。
	static const int aPlants[] = { 2, 3, 4 };
	int aPerLevel = (mMode >= RUN_MODE_FULL && mMode <= RUN_MODE_QUICK) ? aPlants[mMode] : 2;
	mPendingPlantPicks = CanOfferPlantPick() ? aPerLevel : 0;
	// 增益每关收尾的发屏数（2026-10-08 玩家反馈批定案）：普通档（10 关）每关固定 3；
	// 快速档（5 关）逐关 3、4、5、5（= min(关序号 + 2, 5)——它的收尾点恰好 4 个：最后一关
	// 打完直接亮奖杯屏，调用点自己挡掉末点的屏）；完整档保持批十曲线 min(关序号 + 2, 7)。
	// 调用点此刻 mLevelIndex 已经指向下一关，所以"下一关收尾该给的数"直接按它算。
	// 无尽档固定 2 条/关（2026-10-10 用户定案）：难度走对数曲线逐关自涨，屏数不随关序号堆。
	int aBuffs;
	if (mMode == RUN_MODE_ENDLESS)
	{
		aBuffs = 2;
	}
	else if (mMode == RUN_MODE_NORMAL)
	{
		aBuffs = 3;
	}
	else
	{
		aBuffs = mLevelIndex + 2;
		int aCap = (mMode == RUN_MODE_QUICK) ? 5 : 7;
		if (aBuffs > aCap) aBuffs = aCap;
	}
	mPendingBuffPicks = aBuffs;
	TodLog("[run] level end picks: %d plant(s) + %d buff(s) (level %d, mode %d)", mPendingPlantPicks, mPendingBuffPicks, mLevelIndex, mMode);
}

bool RunState::CanOfferPlantPick() const
{
	// 候选口径与 RollChoices 的植物屏一致：0..47 常规植物、排除模仿者。
	for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; i++)
	{
		if (i == (int)SeedType::SEED_IMITATER) continue;
		if (!HasPlant((SeedType)i)) return true;
	}
	return false;
}

// 抽一屏的三条候选。种子挂上"这一屏是第几次抽"（mPickCounter），所以同一局里
// 两屏不会抽出同一组；runSeed 之外还混了本机随机盐 mPickSalt（用户定案：联机里
// 每个玩家的候选各不相同）。盐在进程内稳定，所以失败重打这一关时抽出来的
// 还是同一组三条——重开不会变成"刷候选"；换进程/换机器才换盐。
void RunState::RollChoices(bool theReroll)
{
	// 这一屏的候选从这一刻就算抽好了（LawnApp::UpdateRunPick 的"还没抽才抽"守卫看它）；
	// 每抽一次刷新机会回满——「换一批」自己会把它用掉（见 RerollChoices）。
	mChoicesRolled = true;
	mPickReRolled = false;

	unsigned int aSeed = (unsigned int)mRunSeed
		^ (0x9E3779B9u * (unsigned int)(mLevelIndex + 1))
		^ (0x85EBCA6Bu * (mPickCounter + 1))
		^ (0xC2B2AE3Du * mPickSalt);
	if (IsPlantPick()) aSeed ^= 0x5BF03635u;
	mPickCounter++;

	Sexy::MTRand aRNG(aSeed);

	if (IsPlantPick())
	{
		// 候选 = 全部 48 种常规植物（SEED_PEASHOOTER..SEED_COBCANNON）里、卡池还没有的。
		// 模仿者（SEED_IMITATER）不进候选：它要先指定模仿对象，那道选择在选卡界面里才有。
		bool aOwned[NUM_SEEDS_IN_CHOOSER];
		memset(aOwned, 0, sizeof(aOwned));
		for (size_t i = 0; i < mPool.size(); i++)
		{
			if (mPool[i] >= 0 && mPool[i] < NUM_SEEDS_IN_CHOOSER) aOwned[mPool[i]] = true;
		}

		SeedType aCandidates[NUM_SEEDS_IN_CHOOSER];
		int aCount = 0;
		for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; i++)
		{
			if (i != (int)SeedType::SEED_IMITATER && !aOwned[i]) aCandidates[aCount++] = (SeedType)i;
		}

		// 候选去重批（2026-10-10）：「换一批」把本屏刚摆的三株从候选摘掉——池子够时重抽
		// 必出全新三株；保底口径与增益屏一致（摘到只剩 min(池子, 3) 条为止，别让屏变秃）。
		// 植物屏没有跨屏冷却：植物一次性入池，没"刷掉的株反复来"这回事。
		if (theReroll)
		{
			int aFloor = (aCount < RUN_CHOICES) ? aCount : RUN_CHOICES;
			for (int i = 0; i < RUN_CHOICES && aCount > aFloor; i++)
			{
				SeedType aOld = mPlantChoices[i];
				if (aOld == SeedType::SEED_NONE) continue;
				for (int j = 0; j < aCount; j++)
				{
					if (aCandidates[j] == aOld)
					{
						aCandidates[j] = aCandidates[aCount - 1];
						aCount--;
						break;
					}
				}
			}
		}

		for (int i = 0; i < RUN_CHOICES; i++)
		{
			if (aCount <= 0)
			{
				mPlantChoices[i] = SeedType::SEED_NONE;
				continue;
			}
			int aPick = (int)aRNG.Next((unsigned long)aCount);
			mPlantChoices[i] = aCandidates[aPick];
			aCandidates[aPick] = aCandidates[--aCount];		// 抽走的换到队尾，保证三条互不重复
		}
	}
	else
	{
		// 候选去重批：本屏"露过脸"记账从零起（theReroll 时不清——「换一批」之后，
		// 被刷掉的那批也算本屏露过脸，一并计入下一个冷却集）。
		if (!theReroll) mPickSeenBuffCount = 0;
		// 增益：全局 9 条 + 单株升级 + 第二 buff（docs/06 §8.5，同株第二条词条）混池抽
		// 3 条互不重复，全部按权重（§8.7：档位基值 1★12/2★6/3★1，全局条 ×k、经济条 ×e、
		// 1★/2★ 全局条再减半）。单株两表
		// 的只收"卡池里已经有这株"的（设计文档：只对已拥有的植物出；老表花盆/睡莲/墓碑
		// 删条不进池见 RunPlantUpgradeInPool，第二表删条与未落消费端条目见 RunPlantBuff2InPool）。
		// 同名跨屏可以再来（叠层，见 BuffStack）；但叠到 mMaxStacks 的条目不再进候选
		//（0 = 无限，方案 §2.2）——到顶就抽不中你。
		// （2026-10-09 权重批撤保底：旧「第 1 格保底一条全局」随权重落地撤除——保底是
		// 无权重时代防全局被挤成小概率的手段，§8.7 定案后由 k 与真实候选池密度保证，
		// docs/06 §8.4 步骤 3。）
		int aCandidates[RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_PLANT_BUFF2_COUNT];
		int aWeights[RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_PLANT_BUFF2_COUNT];	// 同下标抽取权重（§8.7）
		int aCount = 0;
		int aTotalW = 0;
		for (int i = 0; i < RUN_BUFF_COUNT; i++)
		{
			int aCap = GetRunChoiceMaxStacks(i);
			if (aCap > 0 && GetBuffCount(i) >= aCap) continue;
			aCandidates[aCount] = i;
			aWeights[aCount] = GetRunChoiceWeight(i);
			aTotalW += aWeights[aCount];
			aCount++;
		}
		for (int i = 0; i < RUN_PLANT_UPGRADE_COUNT; i++)
		{
			int aId = RUN_BUFF_COUNT + i;
			if (!RunPlantUpgradeInPool(GetRunPlantUpgradeDef(i).mPlant)) continue;
			if (!HasPlant(GetRunPlantUpgradeDef(i).mPlant)) continue;
			int aCap = GetRunChoiceMaxStacks(aId);
			if (aCap > 0 && GetBuffCount(aId) >= aCap) continue;
			aCandidates[aCount] = aId;
			aWeights[aCount] = GetRunChoiceWeight(aId);
			aTotalW += aWeights[aCount];
			aCount++;
		}
		// 第二 buff（同株第二条词条，docs/06 §8.5）：进池开关/删条/未落消费端全在
		// RunPlantBuff2InPool（批 0 全 false，纯结构批）；拥有过滤与封顶过滤同老表。
		for (int i = 0; i < RUN_PLANT_BUFF2_COUNT; i++)
		{
			int aId = RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + i;
			if (!RunPlantBuff2InPool(i)) continue;
			if (!HasPlant(GetRunPlantBuff2Def(i).mPlant)) continue;
			int aCap = GetRunChoiceMaxStacks(aId);
			if (aCap > 0 && GetBuffCount(aId) >= aCap) continue;
			aCandidates[aCount] = aId;
			aWeights[aCount] = GetRunChoiceWeight(aId);
			aTotalW += aWeights[aCount];
			aCount++;
		}

		// 候选去重批（2026-10-10，用户令「刷掉的 buff 别反复出现」）：「换一批」要刷掉的本屏
		// 现三条（theReroll）+ 上一屏露过脸没拿的（mPickCooldownBuffs，含上一屏被刷掉的那批）
		// 从候选里摘掉；先摘现三条再摘冷却，摘到只剩 min(池子, 3) 条为止——池子小时自动放宽，
		// 宁可又见到重复条目，也不让三选一屏变秃（哨兵格）。
		{
			unsigned short aExclude[RUN_CHOICES * 3];
			int aExcludeCount = 0;
			if (theReroll)
			{
				for (int i = 0; i < RUN_CHOICES; i++)
				{
					if (mBuffChoices[i] != RUN_BUFF_CHOICE_NONE) aExclude[aExcludeCount++] = mBuffChoices[i];
				}
			}
			for (int i = 0; i < mPickCooldownBuffCount; i++)
			{
				aExclude[aExcludeCount++] = mPickCooldownBuffs[i];
			}
			int aFloor = (aCount < RUN_CHOICES) ? aCount : RUN_CHOICES;
			for (int aE = 0; aE < aExcludeCount && aCount > aFloor; aE++)
			{
				for (int i = 0; i < aCount; i++)
				{
					if (aCandidates[i] == (int)aExclude[aE])
					{
						aTotalW -= aWeights[i];
						aCandidates[i] = aCandidates[aCount - 1];
						aWeights[i] = aWeights[aCount - 1];
						aCount--;
						break;
					}
				}
			}
		}

		// 防御守卫（方案 §2.4）：无限条目兜底，池子正常恒 ≥ 3 条；真抽干时缺格填哨兵、
		// 一条不剩就把这次欠的增益屏作废——空池进 MTRand::Next(0) 是整数除零，直接崩。
		if (aCount <= 0)
		{
			for (int i = 0; i < RUN_CHOICES; i++) mBuffChoices[i] = RUN_BUFF_CHOICE_NONE;
			mPendingBuffPicks = 0;
			TodLog("[run] the buff pool is dry - the owed buff picks are voided");
			return;
		}

		// 加权无放回抽三格。抽中单株条（老表/第二表都算）后把同株的其余条目从候选
		// 摘除（§8.5/§8.6.1，第二 buff 批 0 落地）：一屏三张里同一株至多出现 1 条，
		// 矛盾对（寒冰射手「冰西瓜化」↔「寒冰贯通」、玉米投手「加农炮转化」↔「黄油盛宴」）
		// 都是同株两条，被这条规则一并互斥。跨屏不拦：老条+新条可以先后都拿（存量层叠
		// 各自按 cap 管）。全局条 GetRunChoicePlant 返 SEED_NONE，不触发。
		for (int aSlot = 0; aSlot < RUN_CHOICES; aSlot++)
		{
			if (aCount <= 0)
			{
				mBuffChoices[aSlot] = RUN_BUFF_CHOICE_NONE;
				continue;
			}
			int aPickW = (int)aRNG.Next((unsigned long)aTotalW);
			int aPick = 0;
			for (int i = 0; i < aCount; i++)
			{
				aPickW -= aWeights[i];
				if (aPickW < 0) { aPick = i; break; }
			}
			mBuffChoices[aSlot] = (unsigned short)aCandidates[aPick];
			SeedType aChoicePlant = GetRunChoicePlant(mBuffChoices[aSlot]);
			aTotalW -= aWeights[aPick];
			aCandidates[aPick] = aCandidates[aCount - 1];
			aWeights[aPick] = aWeights[aCount - 1];
			aCount--;
			if (aChoicePlant != SeedType::SEED_NONE)
			{
				for (int i = aCount - 1; i >= 0; i--)
				{
					if (GetRunChoicePlant(aCandidates[i]) == aChoicePlant)
					{
						aTotalW -= aWeights[i];
						aCandidates[i] = aCandidates[aCount - 1];
						aWeights[i] = aWeights[aCount - 1];
						aCount--;
					}
				}
			}
		}

		// 候选去重批的记账：本屏露过脸的全部（抽的当下就记，含「换一批」两批；去重合并
		// 防溢）。消费时由 NoteBuffScreenConsumed 减去拿走的，生成下一屏冷却集。
		for (int i = 0; i < RUN_CHOICES; i++)
		{
			if (mBuffChoices[i] == RUN_BUFF_CHOICE_NONE) continue;
			bool aDup = false;
			for (int j = 0; j < mPickSeenBuffCount; j++)
			{
				if (mPickSeenBuffs[j] == mBuffChoices[i]) { aDup = true; break; }
			}
			if (!aDup && mPickSeenBuffCount < RUN_CHOICES * 2)
			{
				mPickSeenBuffs[mPickSeenBuffCount++] = mBuffChoices[i];
			}
		}
	}
}

// @pvz-online: 「换一批」（2026-10-08 玩家反馈定案；2026-10-10 去重批补排除）：
// 把这一屏的三条候选重抽一遍。走的就是 RollChoices(true)——mPickCounter 再推一步之外，
// 本屏现三条（增益屏连同上一屏没拿的冷却集）在抽之前被整体摘出候选，所以池子够时重抽
// 的三条必然跟刚作废的那组全不同；池子不够时按"保底 3 条"自动放宽。（旧注释声称"必然≠"，
// 但旧实现只换种子、重抽跟作废组可能重合——去重批把它变成真的。）每屏限一次：用过的屏
// （mPickReRolled）再点不动（屏上的按钮在重开时同时置为不可点）；下一屏 RollChoices 会把机会回满。
void RunState::RerollChoices()
{
	if (mPickReRolled) return;
	RollChoices(true);
	mPickReRolled = true;
	TodLog("[run] the choices were refreshed (pick counter %u)", mPickCounter);
}

void RunState::TakePlantChoice(int theIndex)
{
	if (theIndex < 0 || theIndex >= RUN_CHOICES || mPendingPlantPicks <= 0) return;

	SeedType aSeed = mPlantChoices[theIndex];
	if (aSeed != SeedType::SEED_NONE && mPool.size() < (size_t)RUN_POOL_MAX)
	{
		mPool.push_back(aSeed);
		TodTrace("run: plant %d joins the pool (%d seeds)", (int)aSeed, (int)mPool.size());
	}
	mPendingPlantPicks--;
	// 这一屏消费掉了：下一屏（还有欠的话）得重新抽——见 LawnApp::UpdateRunPick 的"还没抽才抽"守卫。
	mChoicesRolled = false;
	// 刚拿到最后一株没到手的植物：本次欠的植物屏到此为止（再选就没候选了）。
	if (mPendingPlantPicks > 0 && !CanOfferPlantPick()) mPendingPlantPicks = 0;
}

void RunState::TakeBuffChoice(int theIndex)
{
	if (theIndex < 0 || theIndex >= RUN_CHOICES || mPendingBuffPicks <= 0) return;

	unsigned short aId = mBuffChoices[theIndex];
	// 空缺格（防御，方案 §2.4）：屏上按钮本就画成不可点；真点到这里也不欠账、不加层。
	if (aId == RUN_BUFF_CHOICE_NONE) return;
	for (size_t i = 0; i < mBuffs.size(); i++)
	{
		if (mBuffs[i].mId == aId)
		{
			mBuffs[i].mCount++;
			mPendingBuffPicks--;
			mChoicesRolled = false;		// 这一屏消费掉了，下一屏重新抽（同 TakePlantChoice）
			NoteBuffScreenConsumed(aId);	// 去重批：本屏露过脸没拿的进下一屏冷却
			return;
		}
	}

	BuffStack aStack;
	aStack.mId = aId;
	aStack.mCount = 1;
	mBuffs.push_back(aStack);
	mPendingBuffPicks--;
	mChoicesRolled = false;		// 这一屏消费掉了，下一屏重新抽（同 TakePlantChoice）
	NoteBuffScreenConsumed(aId);	// 去重批：本屏露过脸没拿的进下一屏冷却
}

// @pvz-online: 「放弃」的记账（2026-10-03 用户定案）：不落货、只消账。先后与
// IsPlantPick 一致——植物屏没清完时放弃的必是植物屏（UpdateRunPick 一屏一屏地摆）。
void RunState::SkipPendingPick()
{
	if (mPendingPlantPicks > 0)
	{
		mPendingPlantPicks--;
		mChoicesRolled = false;		// 这一屏消费掉了，下一屏重新抽（同 TakePlantChoice）
		TodLog("[run] a plant pick was skipped (%d still owed)", mPendingPlantPicks);
	}
	else if (mPendingBuffPicks > 0)
	{
		mPendingBuffPicks--;
		mChoicesRolled = false;		// 这一屏消费掉了，下一屏重新抽（同 TakePlantChoice）
		NoteBuffScreenConsumed(RUN_BUFF_CHOICE_NONE);	// 去重批：放弃 = 本屏露过脸的全部进冷却
		TodLog("[run] a buff pick was skipped (%d still owed)", mPendingBuffPicks);
	}
}

// @pvz-online: 候选去重批的回执（2026-10-10）：「本屏露过脸的全部」减去拿走的那条 = 下一屏
// 的冷却集。拿走的不进冷却（下一屏还能再来叠层——跨屏叠层是老设计，去重批只治"没拿的
// 反复出现"）；放弃（theTakenId = RUN_BUFF_CHOICE_NONE）则整批进冷却。
void RunState::NoteBuffScreenConsumed(unsigned short theTakenId)
{
	int aCount = 0;
	for (int i = 0; i < mPickSeenBuffCount; i++)
	{
		if (mPickSeenBuffs[i] != theTakenId) mPickCooldownBuffs[aCount++] = mPickSeenBuffs[i];
	}
	mPickCooldownBuffCount = aCount;
}

int RunState::GetBuffCount(int theBuffId) const
{
	for (size_t i = 0; i < mBuffs.size(); i++)
	{
		if (mBuffs[i].mId == (unsigned short)theBuffId) return (int)mBuffs[i].mCount;
	}
	return 0;
}

bool RunState::HasPlant(SeedType theSeedType) const
{
	for (size_t i = 0; i < mPool.size(); i++)
	{
		if (mPool[i] == theSeedType) return true;
	}
	return false;
}

int RunState::LevelForIndex(int theIndex)
{
	// 25 个引擎关号（M4-a 定案，2026-10-03 用户要求整组后移：首关不再用 1-1 教学关）。
	// 编排规则：场景内难度只升不降；首关就取到 10 波关（10 波一旗是现成机制，末波
	// 自带旗帜波，不用改旗机制）。第 50 关（5-10）是僵王 boss 关，绝不能进表；
	// 5-8 只有 20 波，顶不上场景 5 的后段（用它会 30→20 回落），故取 5-2/3/4/7/9。
	static const int aLevels[RUN_LEVEL_COUNT] = {
		4,  6,  7,  9,  10,	// 场景 1 白天：10/10/20/20/20 波
		13, 15, 17, 19, 20,	// 场景 2 夜：  10/10/20/20/20 波
		23, 25, 27, 29, 30,	// 场景 3 泳池：20/20/30/30/30 波
		33, 35, 37, 39, 40,	// 场景 4 雾：  10/20/20/20/20 波
		42, 43, 44, 47, 49,	// 场景 5 屋顶：20/20/30/30/30 波
	};
	if (theIndex < 0 || theIndex >= RUN_LEVEL_COUNT) return -1;
	return aLevels[theIndex];
}

// 时长档的每场景关数（完整 5 / 普通 2 / 快速 1）。模式非法按完整版——检查点、联机包
// 里来的值都过这道闸，越界值永远到不了下面的表。无尽档取完整版值（5），仅作静态表尺寸用。
int RunState::LevelsPerScene(int theRunMode)
{
	static const int aPerScene[] = { 5, 2, 1 };
	if (theRunMode < RUN_MODE_FULL || theRunMode > RUN_MODE_QUICK) return aPerScene[RUN_MODE_FULL];
	return aPerScene[theRunMode];
}

// 无尽档没有"关数"这回事（软上限 RUN_ENDLESS_LEVEL_COUNT，见头文件）；其余档 = 场景数 × 每场景关数。
int RunState::LevelCountForMode(int theRunMode)
{
	if (theRunMode == RUN_MODE_ENDLESS) return RUN_ENDLESS_LEVEL_COUNT;
	return RUN_SCENE_COUNT * LevelsPerScene(theRunMode);
}

// 出怪难度档的千分比表（2026-10-03 用户定案：轻松 ×0.5 / 标准 ×1.0；高压 2026-10-04
// 按玩家反馈由 ×1.5 上调 ×2.0，见 docs/07 批六）。
int RunState::DiffPermilleFor(int theRunDiff)
{
	static const int aPermille[] = { 500, 1000, 2000 };
	if (theRunDiff < RUN_DIFF_EASY || theRunDiff > RUN_DIFF_HIGH) return aPermille[RUN_DIFF_STD];
	return aPermille[theRunDiff];
}

// 高级选项两个倍率档的千分比（批 C，2026-10-09 用户定案）：规模 ×0.5/×1/×2/×4——乘在
// 顺位乘数链之后；节奏 ×0.6/×1/×1.6——只乘波间隔倒计时。档位非法按标准 1000。
int RunState::ScalePermilleFor(int theRunScale)
{
	static const int aPermille[] = { 500, 1000, 2000, 4000 };
	if (theRunScale < RUN_SCALE_HALF || theRunScale > RUN_SCALE_QUAD) return aPermille[RUN_SCALE_STD];
	return aPermille[theRunScale];
}

int RunState::TempoPermilleFor(int theRunTempo)
{
	static const int aPermille[] = { 600, 1000, 1600 };
	if (theRunTempo < RUN_TEMPO_FAST || theRunTempo > RUN_TEMPO_SLOW) return aPermille[RUN_TEMPO_STD];
	return aPermille[theRunTempo];
}

// 按时长档从同一张 25 关表里抽行（批十 2026-10-05 按玩家反馈改版）：普通版每场景取
// 第 1/5 关（10 关，即原快速表）、快速版取第 5 关（5 关，每场景收尾的难关）。
// 抽出来的还是这张表里的引擎关号——波数、出怪、种类名单都按引擎关走，
// 所以 RunLevelIndexForEngineLevel 的完整版反查在三档里都命中。
int RunState::LevelForModeIndex(int theRunMode, int theIndex)
{
	static const int aSubs[RUN_MODE_QUICK + 1][RUN_LEVELS_PER_SCENE] = {
		{ 0, 1, 2, 3, 4 },	// 完整版：全取
		{ 0, 4, 0, 0, 0 },	// 普通版：每场景第 1/5 关（=原快速表）
		{ 4, 0, 0, 0, 0 },	// 快速版：每场景第 5 关（引擎关 10/20/30/40/49）
	};
	if (theRunMode < RUN_MODE_FULL || theRunMode > RUN_MODE_QUICK) theRunMode = RUN_MODE_FULL;
	int aPerScene = LevelsPerScene(theRunMode);
	if (theIndex < 0 || theIndex >= LevelCountForMode(theRunMode)) return -1;
	return LevelForIndex((theIndex / aPerScene) * RUN_LEVELS_PER_SCENE + aSubs[theRunMode][theIndex % aPerScene]);
}

// @pvz-online: 补发追赶期间（R6）：人先站到"要追到的那一关"的草坪上，再在草坪上把
// 欠下的三选一补完（见 UpdateRunPick）——所以正在打的这一关就是目标关，关卡号、
// 波表种子和难度阶梯都得按它算，不然先进草坪的那一下会建错关。
int RunState::GetPlayingLevelIndex() const
{
	return IsCatchingUp() ? mCatchUpLevel : mLevelIndex;
}

// 场景档 0..4。mLevelIndex 会短暂停在"已通关"（== 关数）这种空档上：
// 夹进范围里，别让越界值把难度表读穿。每场景关数按时长档（5/2/1）。
// 无尽档的场景进场就锁死了（建档/线传/读档三处都验过 0..4），不随关序号走。
int RunState::GetSceneIndex() const
{
	if (mMode == RUN_MODE_ENDLESS) return mEndlessScene;
	int aIndex = GetPlayingLevelIndex();
	if (aIndex < 0) aIndex = 0;
	if (aIndex > GetLevelCount() - 1) aIndex = GetLevelCount() - 1;
	return aIndex / LevelsPerScene(mMode);
}

// @pvz-online: 无尽档的难度增量（2026-10-10 用户定案"对数阶曲线"，前期涨得动、后期趋缓）：
// 600·ln(1 + 关序号) 的千分比。ln 用整数近似——t = floor(log2(n+1))、frac = 小数部分千分比，
// lnMilli = 693·t + 693·frac/1000（ln2 ≈ 0.693）；全程整数乘除，联机两端逐位一致。
// 样例（n → 增量）：0→0、1→415、3→831、7→1247、15→1663、24→1896、100→2734、255→3326、
// 1023→4158；n=59999（软上限）→ 6582。初值待实机验收调，见 docs/07。
static int EndlessGrowthPermille(int theLevelIndex)
{
	if (theLevelIndex <= 0) return 0;
	unsigned int aN = (unsigned int)theLevelIndex + 1;	// n+1
	int aT = 0;
	while ((1u << (aT + 1)) <= aN) aT++;
	unsigned int aPow = 1u << aT;
	int aFrac = (int)(((aN - aPow) * 1000) / aPow);
	int aLnMilli = 693 * aT + 693 * aFrac / 1000;
	return 600 * aLnMilli / 1000;
}

// 难度阶梯（M4-a，用户定案）：每过一个场景血量与数量同乘 ×1.33 → 1.0/1.33/1.77/2.35/3.13
//（2026-10-04 按玩家反馈"后期难度不足"由 ×1.2 上调、2026-10-05 回调为 ×1.33，见 docs/07 批六/批九；截尾口径同旧表）。
// 写成整数千分比表：两边全靠整数乘除，逐位一致——浮点乘的 0.000001 之差就可能让同一只
// 僵尸在两台机器上一个剩 1 点血、一个已经死了。
// 无尽档：场景基准 + 对数阶增量（见 EndlessGrowthPermille）。
int RunState::GetDifficultyPermille() const
{
	static const int aPermille[RUN_SCENE_COUNT] = { 1000, 1330, 1768, 2352, 3129 };
	if (mMode == RUN_MODE_ENDLESS)
	{
		int aLevel = GetPlayingLevelIndex();
		if (aLevel < 0) aLevel = 0;
		return aPermille[mEndlessScene] + EndlessGrowthPermille(aLevel);
	}
	return aPermille[GetSceneIndex()];
}

// 无尽档的关卡映射：锁定场景内的 5 个原型循环——关序号 % 5 取行（场景锁死，不再随序号换景）。
int RunState::GetLevel() const
{
	if (mMode == RUN_MODE_ENDLESS)
	{
		int aIndex = GetPlayingLevelIndex();
		if (aIndex < 0) return -1;
		return LevelForIndex(mEndlessScene * RUN_LEVELS_PER_SCENE + aIndex % RUN_LEVELS_PER_SCENE);
	}
	return LevelForModeIndex(mMode, GetPlayingLevelIndex());
}

int RunState::GetLevelSeed() const
{
	// 由局种子 + 关序号推导：同一局里每关不同、重开同一关（失败重试）完全一样。
	int aIndex = GetPlayingLevelIndex();
	unsigned int aSeed = (unsigned int)mRunSeed ^ (0x9E3779B9u * (unsigned int)(aIndex + 1));
	aSeed ^= aSeed >> 16;
	aSeed *= 0x85EBCA6Bu;
	aSeed ^= aSeed >> 13;
	return (int)aSeed;
}

// 本关失败一次。序号越界只会出现在"已通关 / 空局"这种不该有人报失败的时候，直接不理。
// 无尽档的关序号会一直往上走：计数按 25 取模落格（首版只存不再惩罚，循环叠用无妨）。
void RunState::NoteLevelFailed()
{
	if (mLevelIndex < 0 || mLevelIndex >= GetLevelCount()) return;
	mFailCounts[mLevelIndex % RUN_LEVEL_COUNT]++;
}

std::string RunState::GetCheckpointName(int theProfileId)
{
	return GetAppDataFolder() + StrFormat("userdata/run%d.dat", theProfileId);
}

bool RunState::HasCheckpoint(int theProfileId)
{
	return gSexyAppBase && gSexyAppBase->FileExists(GetCheckpointName(theProfileId));
}

void RunState::DeleteCheckpoint(int theProfileId)
{
	if (gSexyAppBase) gSexyAppBase->EraseFile(GetCheckpointName(theProfileId));
}

// @pvz-online: 探读（2026-10-10）：完整做一遍 Load 才回填，失败一律返回 false——
// 调用方（两处「续不续」问句）靠它分辨盘上这份档属于哪一档，别再自己看文件在不在。
bool RunState::PeekCheckpoint(int theProfileId, int& theRunMode, int& theEndlessScene)
{
	RunState aTemp;
	if (!aTemp.Load(theProfileId)) return false;
	theRunMode = aTemp.mMode;
	theEndlessScene = aTemp.mEndlessScene;
	return true;
}

bool RunState::Save(int theProfileId) const
{
	std::vector<unsigned char> aData;
	AppendI32(aData, (int)RUN_CHECKPOINT_MAGIC);
	AppendU16(aData, RUN_CHECKPOINT_VERSION);
	AppendU16(aData, (unsigned int)((mMode & 0xFF) | ((mDiff & 0xFF) << 8)));	// 低字节 = 时长档，高字节 = 出怪难度档
	AppendI32(aData, mRunSeed);
	AppendI32(aData, mLevelIndex);
	for (int i = 0; i < RUN_LEVEL_COUNT; i++)
	{
		AppendU16(aData, (unsigned int)mFailCounts[i]);
	}

	AppendU16(aData, (unsigned int)mPool.size());
	for (size_t i = 0; i < mPool.size(); i++)
	{
		AppendU16(aData, (unsigned int)mPool[i]);
	}

	AppendU16(aData, (unsigned int)mBuffs.size());
	for (size_t i = 0; i < mBuffs.size(); i++)
	{
		AppendU16(aData, mBuffs[i].mId);
		AppendU16(aData, mBuffs[i].mCount);
	}

	AppendU16(aData, mMowerUsedRows);	// v5：末位推车记账（bit = 行号）
	AppendU16(aData, (unsigned int)((mScale & 3) | ((mTempo & 3) << 2) | ((mZombotany & 1) << 4)));	// v10：高级选项位包
	// v11（第二 buff 批 0）：buff 列表格式不变（BuffStack 通用 id+count），但 mBuffs 里
	// 从此可能出现第二表 id（57..95）。老版本读到会当老单株解出错误条目，故抬版本号让
	// 老构建直接拒档；v10 及更老档没有这类 id，读平即可，不需要迁移链。
	AppendU16(aData, (unsigned int)mEndlessScene);	// v12：无尽档锁定的场景（非无尽档恒 0）

	MkDir(GetAppDataFolder() + "userdata");
	if (!gSexyAppBase->WriteBytesToFile(GetCheckpointName(theProfileId), aData.data(), (unsigned long)aData.size()))
	{
		TodLog("[run] could not write the checkpoint file");
		return false;
	}
	return true;
}

bool RunState::Load(int theProfileId)
{
	Buffer aBuffer;
	if (!gSexyAppBase->ReadBufferFromFile(GetCheckpointName(theProfileId), &aBuffer, false))
	{
		return false;
	}

	RunReader aReader((const unsigned char*)aBuffer.GetDataPtr(), aBuffer.GetDataLen());
	unsigned int aMagic = 0, aVersion = 0, aReserved = 0;
	if (!aReader.ReadU32(aMagic) || !aReader.ReadU16(aVersion) || !aReader.ReadU16(aReserved))
	{
		return false;
	}
	// v3 才有时长档、v4 才有难度档、v5 才有推车记账（v6 与 v5 的保留位含义相同）。老版本
	// 占的保留位恒 0，恰好是各自默认档——v2 的档按完整版续、v3 的档按标准难度续，都不作废；
	// v5 及更老的档没有推车字段，按"一辆都没用"续。再往前的版本一律当"没有检查点"。
	// v7 起模式表改版（见文件头 v7 条目）：6 及更老的非完整档关序号对不上新表，读完模式
	// 字段后直接作废；完整档表没动，照续。
	// v8 起删了「储备」（见文件头 v8 条目）：旧档 buff id 按下面 buff 循环里的链迁移。
	// v10 才有高级选项位包；v9 及更老的档读不到，按 标准/标准/关 续。
	// v11 起buff 列表可能有第二表 id（57..95，第二 buff 批 0）；v10 及更老档天然没有，
	// buff 循环与迁移链照旧（迁移条件全是 aVersion < N，对新 id 不触发）。
	// v12 起才有无尽档与场景字段：v11 及更老档读到 mode==ENDLESS 一律拒档（伪造/错位）；
	// 非无尽档的场景恒 0。
	int aMode = RUN_MODE_FULL;
	int aDiff = RUN_DIFF_STD;
	if (aMagic != RUN_CHECKPOINT_MAGIC || (aVersion != RUN_CHECKPOINT_VERSION && aVersion != 11 && aVersion != 10 && aVersion != 9 && aVersion != 8 && aVersion != 7 && aVersion != 6 && aVersion != 5 && aVersion != 4 && aVersion != 3 && aVersion != 2))
	{
		TodLog("[run] checkpoint magic/version mismatch, ignored");
		return false;
	}
	if (aVersion >= 3)
	{
		aMode = (int)(aReserved & 0xFF);
		if (aMode < RUN_MODE_FULL || aMode > RUN_MODE_ENDLESS)
		{
			return false;
		}
		if (aMode == RUN_MODE_ENDLESS && aVersion < 12)
		{
			TodLog("[run] pre-v12 checkpoint claims the endless mode, ignored");
			return false;
		}
		if (aVersion < 7 && aMode != RUN_MODE_FULL)
		{
			TodLog("[run] pre-v7 checkpoint with the old mode table, ignored");
			return false;
		}
	}
	if (aVersion >= 4)
	{
		aDiff = (int)((aReserved >> 8) & 0xFF);
		if (aDiff < RUN_DIFF_EASY || aDiff > RUN_DIFF_HIGH)
		{
			return false;
		}
	}

	int aRunSeed = 0, aLevelIndex = 0;
	if (!aReader.ReadI32(aRunSeed) || !aReader.ReadI32(aLevelIndex))
	{
		return false;
	}
	if (aLevelIndex < 0 || aLevelIndex > LevelCountForMode(aMode))
	{
		return false;
	}

	int aFailCounts[RUN_LEVEL_COUNT] = { 0 };
	for (int i = 0; i < RUN_LEVEL_COUNT; i++)
	{
		unsigned int aCount = 0;
		if (!aReader.ReadU16(aCount)) return false;
		aFailCounts[i] = (int)aCount;
	}

	unsigned int aPoolCount = 0;
	if (!aReader.ReadU16(aPoolCount) || aPoolCount > RUN_POOL_MAX)
	{
		return false;
	}
	std::vector<SeedType> aPool;
	for (unsigned int i = 0; i < aPoolCount; i++)
	{
		unsigned int aSeed = 0;
		if (!aReader.ReadU16(aSeed) || aSeed >= (unsigned int)SeedType::NUM_SEED_TYPES)
		{
			return false;
		}
		aPool.push_back((SeedType)aSeed);
	}

	unsigned int aBuffCount = 0;
	if (!aReader.ReadU16(aBuffCount) || aBuffCount > 64)
	{
		return false;
	}
	std::vector<BuffStack> aBuffs;
	for (unsigned int i = 0; i < aBuffCount; i++)
	{
		unsigned int aId = 0, aCount = 0;
		if (!aReader.ReadU16(aId) || !aReader.ReadU16(aCount))
		{
			return false;
		}
		BuffStack aStack;
		aStack.mId = (unsigned short)aId;
		aStack.mCount = (unsigned short)aCount;
		// @pvz-online: 批 18——Precision 全局插在表尾（新 id 8），单株 id 由「8 + 下标」全体
		// 右移 1 变「9 + 下标」；v5 及更老的档里 id >= 8 的其实都是单株，读进来先 +1 读平。
		// 全局 0..7 不动；旧档的 id 8 是"第一个单株"，不是 Precision，不会撞车。
		if (aVersion < 6 && aStack.mId >= 8)
		{
			aStack.mId = (unsigned short)(aStack.mId + 1);
		}
		// @pvz-online: v8——「储备」（原 id 5）删除：全局 9→8 条，单株 id 由「9 + 下标」全体
		// 左移 1 变「8 + 下标」。链在 v6 的 +1 之后（v5 档：id 8 →+1= 9 →−1= 8 回位）。
		// id 5 各代次都是储备本体，丢弃不读；id >= 6 一律 −1（含 8→7 Precision、9..56 → 8..55）。
		if (aVersion < 8)
		{
			if (aStack.mId == 5)
			{
				continue;
			}
			if (aStack.mId >= 6)
			{
				aStack.mId = (unsigned short)(aStack.mId - 1);
			}
		}
		// @pvz-online: v9——全局表尾追加「排山倒海」（新 id 8）：单株 id 由「8 + 下标」全体
		// 右移 1 变「9 + 下标」。链在 v6/v8 之后（v5 档：8 →+1= 9 →−1= 8 →+1= 9 回位）。
		// 旧档的 id 8 是"第一个单株"，不是排山倒海，不会撞车。
		if (aVersion < 9 && aStack.mId >= 8)
		{
			aStack.mId = (unsigned short)(aStack.mId + 1);
		}
		aBuffs.push_back(aStack);
	}

	// v5 才有推车记账；v4 及更老的档读不到，按"一辆都没用"续。
	unsigned int aMowerUsedRows = 0;
	if (aVersion >= 5 && !aReader.ReadU16(aMowerUsedRows))
	{
		return false;
	}

	// v10 才有高级选项位包；v9 及更老的档读不到，按 标准/标准/关 续。位掩码天然合法。
	unsigned int aOptions = 0;
	if (aVersion >= 10 && !aReader.ReadU16(aOptions))
	{
		return false;
	}

	// v12 才有场景字段。无尽档的场景必须真落在 0..4——GetLevel 的取模救不了坏场景
	//（坏场景会取出越界引擎关号），只能在这里拒档；非无尽档的场景恒 0。
	unsigned int aEndlessScene = 0;
	if (aVersion >= 12 && !aReader.ReadU16(aEndlessScene))
	{
		return false;
	}
	if (aMode == RUN_MODE_ENDLESS)
	{
		if (aEndlessScene >= (unsigned int)RUN_SCENE_COUNT)
		{
			TodLog("[run] endless checkpoint with a bad scene, ignored");
			return false;
		}
	}
	else
	{
		aEndlessScene = 0;
	}

	mRunSeed = aRunSeed;
	mMode = aMode;
	mDiff = aDiff;
	mEndlessScene = (int)aEndlessScene;
	mScale = (int)(aOptions & 3);
	mTempo = (int)((aOptions >> 2) & 3);
	mZombotany = (int)((aOptions >> 4) & 1);
	mLevelIndex = aLevelIndex;
	memcpy(mFailCounts, aFailCounts, sizeof(mFailCounts));
	mMowerUsedRows = aMowerUsedRows;
	mPool = aPool;
	mBuffs = aBuffs;
	// 检查点里没有"补发追赶"这回事（它只活在联机对齐的那一刻），读进来一律清掉。
	mCatchUpLevel = -1;
	// 读档也换一次盐（与 StartNew 同口径）：待选屏不进检查点，读档后重抽用新盐。
	mPickSalt = (unsigned int)Sexy::Rand() ^ ((unsigned int)Sexy::Rand() << 16);
	// 候选去重批的内存记账同样不进检查点，读档清零（与 StartNew 同口径）。
	mPickSeenBuffCount = 0;
	mPickCooldownBuffCount = 0;
	return true;
}
