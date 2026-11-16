#include "DataSync.h"
#include "PlayerInfo.h"
#include "../Run/RunState.h"
#include "../Run/RunCheckpointIO.h"
#include "../LawnCommon.h"
#include "../Widget/ChallengeScreen.h"
#include "../../Sexy.TodLib/TodDebug.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "misc/Buffer.h"
#include "../../SexyAppFramework/SexyAppBase.h"

static int gUserVersion = 12;

PlayerInfo::PlayerInfo()
{
	Reset();
}

//0x468310
void PlayerInfo::SyncSummary(DataSync& theSync)
{
	theSync.SyncString(mName);
	theSync.SyncLong(mUseSeq);
	theSync.SyncLong(mId);
}

//0x468390
// GOTY @Patoke: 0x46BE80
void PlayerInfo::SyncDetails(DataSync& theSync)
{
	if (theSync.GetReader())
	{
		Reset();
	}

	// Larger inventories use a version that older clients reject before reading the records.
	int aVersion = mNumPottedPlants > 200 ? gUserVersion + 1 : gUserVersion;
	theSync.SyncLong(aVersion);
	theSync.SetVersion(aVersion);
	if (aVersion != gUserVersion && aVersion != gUserVersion + 1)
	{
		return;
	}

	theSync.SyncLong(mLevel);
	theSync.SyncLong(mCoins);
	theSync.SyncLong(mFinishedAdventure);
	for (int i = 0; i < 100; i++)
	{
		theSync.SyncLong(mChallengeRecords[i]);
	}
	for (int i = 0; i < 80; i++)
	{
		theSync.SyncLong(mPurchases[i]);
	}
	theSync.SyncLong(mPlayTimeActivePlayer);
	theSync.SyncLong(mPlayTimeInactivePlayer);
	theSync.SyncLong(mHasUsedCheatKeys);
	theSync.SyncLong(mHasWokenStinky);
	theSync.SyncLong(mDidntPurchasePacketUpgrade);
	theSync.SyncLong(mLastStinkyChocolateTime);
	theSync.SyncLong(mStinkyPosX);
	theSync.SyncLong(mStinkyPosY);
	theSync.SyncLong(mHasUnlockedMinigames);
	theSync.SyncLong(mHasUnlockedPuzzleMode);
	theSync.SyncLong(mHasNewMiniGame);
	theSync.SyncLong(mHasNewScaryPotter);
	theSync.SyncLong(mHasNewIZombie);
	theSync.SyncLong(mHasNewSurvival);
	theSync.SyncLong(mHasUnlockedSurvivalMode);
	theSync.SyncLong(mNeedsMessageOnGameSelector);
	theSync.SyncLong(mNeedsMagicTacoReward);
	theSync.SyncLong(mHasSeenStinky);
	theSync.SyncLong(mHasSeenUpsell);
	theSync.SyncLong(mPlaceHolderPlayerStats);
	theSync.SyncLong(mNumPottedPlants);
	
	if (mNumPottedPlants < 0 || mNumPottedPlants > MAX_POTTED_PLANTS ||
		(aVersion == gUserVersion && mNumPottedPlants > 200))
		throw DataReaderException();
	for (int i = 0; i < mNumPottedPlants; i++)
	{
		theSync.SyncBytes(&mPottedPlant[i], sizeof(PottedPlant));
	}

	// @Patoke: implemented
	for (int i = 0; i < 20; i++)
	{
		theSync.SyncBool(mEarnedAchievements[i]);
	}

	for (int i = 0; i < 20; i++)
	{
		theSync.SyncBool(mShownAchievements[i]);
	}

	theSync.SyncBool(mAcceptedZombatarULA);
	theSync.SyncLong(mNumZombatars);
	for (int i = 0; i < mNumZombatars; i++)
	{
		theSync.SyncBytes(&mZombatars[i], sizeof(Zombatar));
	}

	for (int i = 0; i < 20; i++)
	{
		theSync.SyncBool(mMiniGamesCompleted[i]);
	}

	theSync.SyncBool(mShownZombatarDesktopMessage);
}

//0x469400
void PlayerInfo::LoadDetails()
{
    if (!RunCheckpointIO::Recover(RunState::GetCheckpointName(mId),
        GetAppDataFolder() + "userdata/user" + std::to_string(mId) + ".dat")) return;
	try
	{
		Buffer aBuffer;
		std::string aFileName = GetAppDataFolder() + StrFormat("userdata/user%d.dat", mId);
		if (!gSexyAppBase->ReadBufferFromFile(aFileName, &aBuffer, false))
		{
			return;
		}

		DataReader aReader;
		aReader.OpenMemory(aBuffer.GetDataPtr(), aBuffer.GetDataLen(), false);
		DataSync aSync(aReader);
		SyncDetails(aSync);
	}
	catch (DataReaderException&)
	{
		TodTrace("Failed to player data, resetting it\n");
		Reset();
	}
}

//0x4695F0
// GOTY @Patoke: 0x46D750
// @pvz-online: check durability before exposing a paid reroll or overwriting a pending transaction.
std::vector<unsigned char> PlayerInfo::EncodeDetails()
{
    DataWriter writer;
    writer.OpenMemory();
    DataSync sync(writer);
    SyncDetails(sync);
    const auto* data = (const unsigned char*)writer.GetDataPtr();
    return std::vector<unsigned char>(data, data + writer.GetDataLen());
}
bool PlayerInfo::SaveDetailsChecked()
{
    MkDir(GetAppDataFolder() + "userdata");
    const std::string path = GetAppDataFolder() + "userdata/user" + std::to_string(mId) + ".dat";
    return RunCheckpointIO::Recover(RunState::GetCheckpointName(mId), path) &&
        RunCheckpointIO::AtomicWrite(path, EncodeDetails());
}
void PlayerInfo::SaveDetails()
{
    SaveDetailsChecked();
}

//0x469810
void PlayerInfo::DeleteUserFiles()
{
	std::string aFilename = GetAppDataFolder() + StrFormat("userdata/user%d.dat", mId);
	gSexyAppBase->EraseFile(aFilename);

	for (int i = 0; i < (int)GameMode::NUM_GAME_MODES; i++)
	{
		std::string aFileName = GetSavedGameName((GameMode)i, mId);
		gSexyAppBase->EraseFile(aFileName);
	}
}

//0x469940
// GOTY @Patoke: 0x46DBF0
void PlayerInfo::Reset()
{
	mLevel = 1;
	mCoins = 0;
	mFinishedAdventure = 0;
	memset(mChallengeRecords, 0, sizeof(mChallengeRecords));
	memset(mPurchases, 0, sizeof(mPurchases));
	mPlayTimeActivePlayer = 0;
	mPlayTimeInactivePlayer = 0;
	mHasUsedCheatKeys = 0;
	mHasWokenStinky = 0;
	mDidntPurchasePacketUpgrade = 0;
	mLastStinkyChocolateTime = 0;
	mStinkyPosX = 0;
	mStinkyPosY = 0;
	mHasUnlockedMinigames = 0;
	mHasUnlockedPuzzleMode = 0;
	mHasNewMiniGame = 0;
	mHasNewScaryPotter = 0;
	mHasNewIZombie = 0;
	mHasNewSurvival = 0;
	mHasUnlockedSurvivalMode = 0;
	mNeedsMessageOnGameSelector = 0;
	mNeedsMagicTacoReward = 0;
	mHasSeenStinky = 0;
	mHasSeenUpsell = 0;
	mAcceptedZombatarULA = false;
	mPlaceHolderPlayerStats = 0;

	for (int i = 0; i < 20; i++)
	{
		mEarnedAchievements[i] = false;
		mShownAchievements[i] = false;
	}

	mNumZombatars = 0;

	memset(mPottedPlant, 0, sizeof(mPottedPlant));
	mNumPottedPlants = 0;
	memset(mMiniGamesCompleted, 0, sizeof(mMiniGamesCompleted));
	mShownZombatarDesktopMessage = false;
}

void PlayerInfo::AddCoins(int theAmount)
{
	mCoins += theAmount;
	if (mCoins > 99999999)
	{
		mCoins = 99999999;
	}
	else if (mCoins < 0)
	{
		mCoins = 0;
	}
}

void PlayerInfo::ResetChallengeRecord(GameMode theGameMode)
{
	int aGameMode = (int)theGameMode - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	TOD_ASSERT(aGameMode >= 0 && aGameMode <= NUM_CHALLENGE_MODES);
	mChallengeRecords[aGameMode] = 0;
}

//0x469A00
void PottedPlant::InitializePottedPlant(SeedType theSeedType)
{
	memset(this, 0, sizeof(PottedPlant));
	mSeedType = theSeedType;
	mDrawVariation = DrawVariation::VARIATION_NORMAL;
	mLastWateredTime = 0;
	mFacing = (FacingDirection)RandRangeInt((int)FacingDirection::FACING_RIGHT, (int)FacingDirection::FACING_LEFT);
	mPlantAge = PottedPlantAge::PLANTAGE_SPROUT;
	mTimesFed = 0;
	mWhichZenGarden = GardenType::GARDEN_MAIN;
	mFeedingsPerGrow = RandRangeInt(3, 5);
	mPlantNeed = PottedPlantNeed::PLANTNEED_NONE;
	mLastNeedFulfilledTime = 0;
	mLastFertilizedTime = 0;
	mLastChocolateTime = 0;
}

void PlayerInfo::DeleteZombatarFromIndex(int* theIndex)
{
	if (*theIndex < 0 || *theIndex > mNumZombatars - 1)
	{
		TodTraceAndLog("Attempting to delete invalid Zombatar at index %d", *theIndex);
		return;
	}

	if (*theIndex == mNumZombatars - 1)
	{
		// if zombatar is at the end of the list, we can just invalidate it
		mNumZombatars = mNumZombatars - 1;
	}
	else
	{
		if (*theIndex > 0)
		{
			// if the zombatar is in the middle of the list, we need to shift the rest of the list down
			memcpy(&mZombatars[*theIndex], &mZombatars[*theIndex + 1], sizeof(Zombatar) * (mNumZombatars - *theIndex));
		}
		else
		{
			// if the zombatar is at the beginning of the list, we can just shift the rest of the list down
			memcpy(mZombatars, &mZombatars[1], sizeof(Zombatar) * mNumZombatars - sizeof(Zombatar));
		}

		--mNumZombatars;
	}
}