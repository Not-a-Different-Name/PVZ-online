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

static const unsigned int RUN_CHECKPOINT_MAGIC = 0x314E5552;	// 'RUN1'
static const unsigned short RUN_CHECKPOINT_VERSION = 6;

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

void RunState::StartNew(int theRunSeed, int theRunMode, int theRunDiff)
{
	mRunSeed = theRunSeed;
	mMode = theRunMode;
	mDiff = theRunDiff;
	mLevelIndex = 0;
	mPool.clear();
	mPool.push_back(SeedType::SEED_SUNFLOWER);
	mPool.push_back(SeedType::SEED_PEASHOOTER);
	mBuffs.clear();
	memset(mFailCounts, 0, sizeof(mFailCounts));
	mMowerUsedRows = 0;
	mPendingPlantPicks = 0;
	mPendingBuffPicks = 0;
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

// 一局的两处选卡口：开局先挑四株 + 两个增益（手里有 6 株才进第 1 关；2026-10-03 多人
// 实测反馈"开局难度略高"后用户定案加厚，原来只挑两株），每过一关再挑两株 + 一个增益。
// 屏的先后由 IsPlantPick 决定：先四屏植物、再两屏增益。
// 卡池拿满 48 株后植物屏没得抽——那之后只发增益屏。
void RunState::BeginStartPicks()
{
	mPendingPlantPicks = CanOfferPlantPick() ? 4 : 0;
	mPendingBuffPicks = 2;
}

void RunState::BeginLevelEndPicks()
{
	// 每关后的奖励屏按时长档倍乘（M4-b 定案）：完整 ×1、普通 ×2、快速 ×3——"短一局"
	// 用更密的奖励补内容量。植物候选抽干时自动只发增益屏（见 CanOfferPlantPick）。
	static const int aMul[] = { 1, 2, 3 };
	int aTimes = (mMode >= RUN_MODE_FULL && mMode <= RUN_MODE_QUICK) ? aMul[mMode] : 1;
	mPendingPlantPicks = CanOfferPlantPick() ? 2 * aTimes : 0;
	mPendingBuffPicks = 1 * aTimes;
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
void RunState::RollChoices()
{
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
		// 增益：全局 8 条 + 单株升级混池抽 3 条互不重复。单株的只收"卡池里已经有这株"的
		// （设计文档：只对已拥有的植物出）。同名跨屏可以再来（叠层，见 BuffStack）；
		// 但叠到 mMaxStacks 的条目不再进候选（0 = 无限，方案 §2.2）——到顶就抽不中你。
		int aCandidates[RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT];
		int aCount = 0;
		for (int i = 0; i < RUN_BUFF_COUNT; i++)
		{
			int aCap = GetRunChoiceMaxStacks(i);
			if (aCap > 0 && GetBuffCount(i) >= aCap) continue;
			aCandidates[aCount++] = i;
		}
		for (int i = 0; i < RUN_PLANT_UPGRADE_COUNT; i++)
		{
			int aId = RUN_BUFF_COUNT + i;
			if (!HasPlant(GetRunPlantUpgradeDef(i).mPlant)) continue;
			int aCap = GetRunChoiceMaxStacks(aId);
			if (aCap > 0 && GetBuffCount(aId) >= aCap) continue;
			aCandidates[aCount++] = aId;
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

		for (int i = 0; i < RUN_CHOICES; i++)
		{
			if (aCount <= 0)
			{
				mBuffChoices[i] = RUN_BUFF_CHOICE_NONE;
				continue;
			}
			int aPick = (int)aRNG.Next((unsigned long)aCount);
			mBuffChoices[i] = (unsigned short)aCandidates[aPick];
			aCandidates[aPick] = aCandidates[--aCount];
		}
	}
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
			return;
		}
	}

	BuffStack aStack;
	aStack.mId = aId;
	aStack.mCount = 1;
	mBuffs.push_back(aStack);
	mPendingBuffPicks--;
}

// @pvz-online: 「放弃」的记账（2026-10-03 用户定案）：不落货、只消账。先后与
// IsPlantPick 一致——植物屏没清完时放弃的必是植物屏（UpdateRunPick 一屏一屏地摆）。
void RunState::SkipPendingPick()
{
	if (mPendingPlantPicks > 0)
	{
		mPendingPlantPicks--;
		TodLog("[run] a plant pick was skipped (%d still owed)", mPendingPlantPicks);
	}
	else if (mPendingBuffPicks > 0)
	{
		mPendingBuffPicks--;
		TodLog("[run] a buff pick was skipped (%d still owed)", mPendingBuffPicks);
	}
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

// 时长档的每场景关数（完整 5 / 普通 3 / 快速 2）。模式非法按完整版——检查点、联机包
// 里来的值都过这道闸，越界值永远到不了下面的表。
int RunState::LevelsPerScene(int theRunMode)
{
	static const int aPerScene[] = { 5, 3, 2 };
	if (theRunMode < RUN_MODE_FULL || theRunMode > RUN_MODE_QUICK) return aPerScene[RUN_MODE_FULL];
	return aPerScene[theRunMode];
}

int RunState::LevelCountForMode(int theRunMode)
{
	return RUN_SCENE_COUNT * LevelsPerScene(theRunMode);
}

// 出怪难度档的千分比表（2026-10-03 用户定案：轻松 ×0.5 / 标准 ×1.0 / 高压 ×1.5）。
int RunState::DiffPermilleFor(int theRunDiff)
{
	static const int aPermille[] = { 500, 1000, 1500 };
	if (theRunDiff < RUN_DIFF_EASY || theRunDiff > RUN_DIFF_HIGH) return aPermille[RUN_DIFF_STD];
	return aPermille[theRunDiff];
}

// 按时长档从同一张 25 关表里抽行：普通版每场景取第 1/3/5 关、快速版取第 1/5 关
// （M4-b 定案）。抽出来的还是这张表里的引擎关号——波数、出怪、种类名单都按引擎关走，
// 所以 RunLevelIndexForEngineLevel 的完整版反查在三档里都命中。
int RunState::LevelForModeIndex(int theRunMode, int theIndex)
{
	static const int aSubs[RUN_MODE_QUICK + 1][RUN_LEVELS_PER_SCENE] = {
		{ 0, 1, 2, 3, 4 },	// 完整版：全取
		{ 0, 2, 4, 0, 0 },	// 普通版：每场景第 1/3/5 关
		{ 0, 4, 0, 0, 0 },	// 快速版：每场景第 1/5 关
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
// 夹进范围里，别让越界值把难度表读穿。每场景关数按时长档（5/3/2）。
int RunState::GetSceneIndex() const
{
	int aIndex = GetPlayingLevelIndex();
	if (aIndex < 0) aIndex = 0;
	if (aIndex > GetLevelCount() - 1) aIndex = GetLevelCount() - 1;
	return aIndex / LevelsPerScene(mMode);
}

// 难度阶梯（M4-a，用户定案）：每过一个场景血量与数量同乘 ×1.2 → 1.0/1.2/1.44/1.73/2.07。
// 写成整数千分比表：两边全靠整数乘除，逐位一致——浮点乘的 0.000001 之差就可能让同一只
// 僵尸在两台机器上一个剩 1 点血、一个已经死了。
int RunState::GetDifficultyPermille() const
{
	static const int aPermille[RUN_SCENE_COUNT] = { 1000, 1200, 1440, 1728, 2073 };
	return aPermille[GetSceneIndex()];
}

int RunState::GetLevel() const
{
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
void RunState::NoteLevelFailed()
{
	if (mLevelIndex < 0 || mLevelIndex >= GetLevelCount()) return;
	mFailCounts[mLevelIndex]++;
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
	int aMode = RUN_MODE_FULL;
	int aDiff = RUN_DIFF_STD;
	if (aMagic != RUN_CHECKPOINT_MAGIC || (aVersion != RUN_CHECKPOINT_VERSION && aVersion != 5 && aVersion != 4 && aVersion != 3 && aVersion != 2))
	{
		TodLog("[run] checkpoint magic/version mismatch, ignored");
		return false;
	}
	if (aVersion >= 3)
	{
		aMode = (int)(aReserved & 0xFF);
		if (aMode < RUN_MODE_FULL || aMode > RUN_MODE_QUICK)
		{
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
		aBuffs.push_back(aStack);
	}

	// v5 才有推车记账；v4 及更老的档读不到，按"一辆都没用"续。
	unsigned int aMowerUsedRows = 0;
	if (aVersion >= 5 && !aReader.ReadU16(aMowerUsedRows))
	{
		return false;
	}

	mRunSeed = aRunSeed;
	mMode = aMode;
	mDiff = aDiff;
	mLevelIndex = aLevelIndex;
	memcpy(mFailCounts, aFailCounts, sizeof(mFailCounts));
	mMowerUsedRows = aMowerUsedRows;
	mPool = aPool;
	mBuffs = aBuffs;
	// 检查点里没有"补发追赶"这回事（它只活在联机对齐的那一刻），读进来一律清掉。
	mCatchUpLevel = -1;
	// 读档也换一次盐（与 StartNew 同口径）：待选屏不进检查点，读档后重抽用新盐。
	mPickSalt = (unsigned int)Sexy::Rand() ^ ((unsigned int)Sexy::Rand() << 16);
	return true;
}
