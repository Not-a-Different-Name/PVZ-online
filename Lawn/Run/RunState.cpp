#include "RunState.h"
#include <cstring>
#include "../LawnCommon.h"
#include "../../Sexy.TodLib/TodDebug.h"
#include "misc/Buffer.h"
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
