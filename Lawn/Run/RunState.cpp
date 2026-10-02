#include "RunState.h"
#include "RunBuffs.h"
#include <cstring>
#include "../LawnCommon.h"
#include "../../Sexy.TodLib/TodDebug.h"
#include "misc/Buffer.h"
#include "misc/MTRand.h"
#include "../../SexyAppFramework/SexyAppBase.h"

// @pvz-online: 闯关状态的实体。检查点格式（小端，x86 直写）：
//   u32 magic 'RUN1' + u16 版本 + u16 保留
//   i32 runSeed + i32 levelIndex + u16 failCounts[5]
//   u16 卡池数 + 每株 u16 SeedType
//   u16 buff 数 + 每条 (u16 id, u16 层数)
// 版本不符 / 越界一律当"没有检查点"——宁可从头开新局，也不带着半截数据进场。

static const unsigned int RUN_CHECKPOINT_MAGIC = 0x314E5552;	// 'RUN1'
static const unsigned short RUN_CHECKPOINT_VERSION = 1;

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

void RunState::StartNew(int theRunSeed)
{
	mRunSeed = theRunSeed;
	mLevelIndex = 0;
	mPool.clear();
	mPool.push_back(SeedType::SEED_SUNFLOWER);
	mPool.push_back(SeedType::SEED_PEASHOOTER);
	mBuffs.clear();
	memset(mFailCounts, 0, sizeof(mFailCounts));
	mPendingPlantPicks = 0;
	mPendingBuffPicks = 0;
	mPickCounter = 0;
	for (int i = 0; i < RUN_CHOICES; i++)
	{
		mPlantChoices[i] = SeedType::SEED_NONE;
		mBuffChoices[i] = 0;
	}
}

// 一局的两处选卡口：开局先挑两株（手里有 4 株才进第 1 关），每过一关再挑两株 + 一个增益。
void RunState::BeginStartPicks()
{
	mPendingPlantPicks = 2;
	mPendingBuffPicks = 0;
}

void RunState::BeginLevelEndPicks()
{
	mPendingPlantPicks = 2;
	mPendingBuffPicks = 1;
}

// 抽一屏的三条候选。种子挂上"这一屏是第几次抽"（mPickCounter），所以同一局里
// 两屏不会抽出同一组；又因为一切都是 runSeed 推出来的，失败重打这一关时
// 抽出来的还是同一组三条——重开不会变成"刷候选"。
void RunState::RollChoices()
{
	unsigned int aSeed = (unsigned int)mRunSeed
		^ (0x9E3779B9u * (unsigned int)(mLevelIndex + 1))
		^ (0x85EBCA6Bu * (mPickCounter + 1));
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
		// （设计文档：只对已拥有的植物出）。全局 8 条是保底，池子恒 ≥ 8 条。
		// 同名跨屏可以再来（叠层，见 BuffStack）。
		int aCandidates[RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT];
		int aCount = 0;
		for (int i = 0; i < RUN_BUFF_COUNT; i++) aCandidates[aCount++] = i;
		for (int i = 0; i < RUN_PLANT_UPGRADE_COUNT; i++)
		{
			if (HasPlant(GetRunPlantUpgradeDef(i).mPlant)) aCandidates[aCount++] = RUN_BUFF_COUNT + i;
		}

		for (int i = 0; i < RUN_CHOICES; i++)
		{
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
}

void RunState::TakeBuffChoice(int theIndex)
{
	if (theIndex < 0 || theIndex >= RUN_CHOICES || mPendingBuffPicks <= 0) return;

	unsigned short aId = mBuffChoices[theIndex];
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
	static const int aLevels[RUN_LEVEL_COUNT] = { 1, 3, 5, 7, 9 };
	if (theIndex < 0 || theIndex >= RUN_LEVEL_COUNT) return -1;
	return aLevels[theIndex];
}

int RunState::GetLevel() const
{
	return LevelForIndex(mLevelIndex);
}

int RunState::GetLevelSeed() const
{
	// 由局种子 + 关序号推导：同一局里每关不同、重开同一关（失败重试）完全一样。
	unsigned int aSeed = (unsigned int)mRunSeed ^ (0x9E3779B9u * (unsigned int)(mLevelIndex + 1));
	aSeed ^= aSeed >> 16;
	aSeed *= 0x85EBCA6Bu;
	aSeed ^= aSeed >> 13;
	return (int)aSeed;
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
	AppendU16(aData, 0);
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
	if (aMagic != RUN_CHECKPOINT_MAGIC || aVersion != RUN_CHECKPOINT_VERSION)
	{
		TodLog("[run] checkpoint magic/version mismatch, ignored");
		return false;
	}

	int aRunSeed = 0, aLevelIndex = 0;
	if (!aReader.ReadI32(aRunSeed) || !aReader.ReadI32(aLevelIndex))
	{
		return false;
	}
	if (aLevelIndex < 0 || aLevelIndex > RUN_LEVEL_COUNT)
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
		aBuffs.push_back(aStack);
	}

	mRunSeed = aRunSeed;
	mLevelIndex = aLevelIndex;
	memcpy(mFailCounts, aFailCounts, sizeof(mFailCounts));
	mPool = aPool;
	mBuffs = aBuffs;
	return true;
}
