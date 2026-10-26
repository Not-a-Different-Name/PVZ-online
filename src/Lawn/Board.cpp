#include "Run/RunSpeciesRuntime.h"
#include "TeamPlay.h"
#include "Run/RunSpeciesRules.h"
#include <memory>
#include <new>
#include "Run/EndlessLawn.h"
#include "Widget/NextWaveControl.h"
#include "CoopPlay.h"
#include "Widget/CoopHud.h"
#include "Widget/DamageStatsView.h"
/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#include <time.h>
#include <algorithm>
#include <SDL.h>
#include <format>
#include "ZenGarden.h"
#include "BoardInclude.h"
#include "Lawn/Observer/ObserverBoard.h"
#include "Widget/AdmissionBoard.h"
#include "Online/NetSession.h"
#include "Online/NetworkLog.h"
#include "ModText.h"
#include "Run/RunState.h"
#include "Run/RunMath.h"
#include "Run/AnimationAdmission.h"
#include "Run/FrostArmorBatch.h"
#include "Run/RunZombieRoster.h"
#include "LawnCommon.h"
#include "System/Music.h"
#include "System/SaveGame.h"
#include "Widget/LawnDialog.h"
#include "System/PlayerInfo.h"
#include "System/PoolEffect.h"
#include "System/TypingCheck.h"
#include "Widget/StoreScreen.h"
#include "Widget/AwardScreen.h"
#include "../PvzpLib/Trail.h"
#include "Widget/ChallengeScreen.h"
#include "../PvzpLib/PvzpDebug.h"
#include "../PvzpLib/PvzpFoley.h"
#include "Widget/SeedChooserScreen.h"
#include "../PvzpLib/Attachment.h"
#include "../PvzpLib/Reanimator.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "../PvzpLib/PvzpParticle.h"
#include "../PvzpLib/EffectSystem.h"
#include "../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "sound/SoundManager.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "sound/SoundInstance.h"

//#define SEXY_PERF_ENABLED
#include "misc/PerfTimer.h"
#include "Widget/AchievementsScreen.h"
#include "Run/GarlicPulse.h"

constexpr const int ZOMBIE_COUNTDOWN_FIRST_WAVE = 1800;
constexpr const int ZOMBIE_COUNTDOWN = 2500;
constexpr const int ZOMBIE_COUNTDOWN_RANGE = 600;
constexpr const int ZOMBIE_COUNTDOWN_BEFORE_FLAG = 4500;
constexpr const int ZOMBIE_COUNTDOWN_BEFORE_REPICK = 5499;
constexpr const int SUN_COUNTDOWN = 425;
constexpr const int SUN_COUNTDOWN_RANGE = 275;
constexpr const int SUN_COUNTDOWN_MAX = 950;
constexpr const int FOG_BLOW_RETURN_TIME = 2000;
constexpr const int FLAG_RAISE_TIME = 100;

bool gShownMoreSunTutorial = false;








Board::Board(LawnApp* theApp)
{
    RunSpeciesRuntime::Reset(this);
	for (auto& wave : mZombiesInWave) std::fill(std::begin(wave), std::end(wave), ZombieType::ZOMBIE_INVALID);
	mApp = theApp;
    NextWave::Reset(this);
	mApp->mBoard = this;

	try { mZombies.DataArrayInitialize(2048U, "zombies"); }
	catch (const std::bad_alloc&) { mZombies.DataArrayInitialize(1024U, "zombies"); PvzpLogLn("[capacity] zombies: using original 1024-slot pool"); }
	mPlants.DataArrayInitialize(1024U, "plants");
	try { mProjectiles.DataArrayInitialize(4096U, "projectiles"); }
	catch (const std::bad_alloc&) { mProjectiles.DataArrayInitialize(2048U, "projectiles"); PvzpLogLn("[capacity] projectiles: using original 2048-slot pool"); }
	mCoins.DataArrayInitialize(1024U, "coins");
	mLawnMowers.DataArrayInitialize(32U, "lawnmowers");
	mGridItems.DataArrayInitialize(128U, "griditems");

	mApp->mEffectSystem->EffectSystemFreeAll();
	mBoardRandSeed = mApp->mAppRandSeed;
	if (mApp->IsSurvivalMode())
	{
		mBoardRandSeed = Rand();
	}
	mCoinBankFadeCount = 0;
	mLevel = 0;
	mCursorObject = std::make_unique<CursorObject>();
	mCursorPreview = std::make_unique<CursorPreview>();
	mSeedBank = std::make_unique<SeedBank>();
	mCutScene = std::make_unique<CutScene>();
	mSpecialGraveStoneX = -1;
	mSpecialGraveStoneY = -1;
	for (int i = 0; i < MAX_GRID_SIZE_X; i++)
	{
		for (int j = 0; j < MAX_GRID_SIZE_Y; j++)
		{
			mGridSquareType[i][j] = GridSquareType::GRIDSQUARE_GRASS;
			mGridCelLook[i][j] = Rand(20);
			mGridCelOffset[i][j][0] = Rand(10) - 5;
			mGridCelOffset[i][j][1] = Rand(10) - 5;
		}

		for (int k = 0; k < MAX_GRID_SIZE_Y + 1; k++)
		{
			mGridCelFog[i][k] = 0;
		}
	}
	mFogOffset = 0.0f;
	mSunCountDown = 0;
	mShakeCounter = 0;
	mShakeAmountX = 0;
	mShakeAmountY = 0;
	mPaused = false;
	mLevelAwardSpawned = false;
	mFlagRaiseCounter = 0;
	mIceTrapCounter = 0;
	mLevelComplete = false;
	mBoardFadeOutCounter = -1;
	mNextSurvivalStageCounter = 0;
	mScoreNextMowerCounter = 0;
	mProgressMeterWidth = 0;
	mPoolSparklyParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mFogBlownCountDown = 0;
	mFwooshCountDown = 0;
	mTimeStopCounter = 0;
	mCobCannonCursorDelayCounter = 0;
	mCobCannonMouseX = 0;
	mCobCannonMouseY = 0;
	mDroppedFirstCoin = false;
	mBonusLawnMowersRemaining = 0;
	mEnableGraveStones = false;
	mHelpIndex = AdviceType::ADVICE_NONE;
	mEffectCounter = 0;
	mDrawCount = 0;
	mRiseFromGraveCounter = 0;
	mFinalWaveSoundCounter = 0;
	mKilledYeti = false;
	mTriggeredLawnMowers = 0;
	mPlayTimeActiveLevel = 0;
	mPlayTimeInactiveLevel = 0;
	mMaxSunPlants = 0;
	mStartDrawTime = 0;
	mIntervalDrawTime = 0;
	mIntervalDrawCountStart = 0;
	mPreloadTime = 0;
	mGameID = mApp->GetNowTime();
	mMinFPS = 1000.0f;
	mGravesCleared = 0;
	mPlantsEaten = 0;
	mPlantsShoveled = 0;
	mPeaShooterUsed = false;
	mCatapultPlantsUsed = false;
	mMushroomAndCoffeeBeansOnly = true;
	mMushroomsUsed = false;
	mLevelCoinsCollected = 0;
	mGargantuarsKillsByCornCob = 0;
	mCoinsCollected = 0;
	mDiamondsCollected = 0;
	mPottedPlantsCollected = 0;
	mChocolateCollected = 0;
	for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
	{
		for (int x = 0; x < 12; x++)
		{
			mFwooshID[y][x] = ReanimationID::REANIMATIONID_NULL;
		}
	}
	mPrevMouseX = -1;
	mPrevMouseY = -1;
	mFinalBossKilled = false;
	mMustacheMode = mApp->mMustacheMode;
	mSuperMowerMode = mApp->mSuperMowerMode;
	mFutureMode = mApp->mFutureMode;
	mPinataMode = mApp->mPinataMode;
	mDanceMode = mApp->mDanceMode;
	mDaisyMode = mApp->mDaisyMode;
	mSukhbirMode = mApp->mSukhbirMode;
	mShowShovel = false;
	mToolTip = std::make_unique<ToolTipWidget>();
	mAdvice = std::make_unique<MessageWidget>(mApp);
	mBackground = BackgroundType::BACKGROUND_1_DAY;
	mMainCounter = 0;
	mBoardUpdateCounter = 0;
	mTutorialState = TutorialState::TUTORIAL_OFF;
	mTutorialTimer = -1;
	mTutorialParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mChallenge = std::make_unique<Challenge>();
	mClip = false;
	mDebugTextMode = DebugTextMode::DEBUG_TEXT_NONE;
	mMenuButton = std::make_unique<GameButton>(0);
	mMenuButton->mDrawStoneButton = true;
	mStoreButton = nullptr;
	mIgnoreMouseUp = false;
#ifdef __ANDROID__
	mTouchSeedDrag = false;
	mTouchSeedDragMoved = false;
	mTouchSeedDragStartX = 0;
	mTouchSeedDragStartY = 0;
#endif

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mMenuButton->SetLabel("[MAIN_MENU_BUTTON]");
		mMenuButton->Resize(628, -10, 163, 46);

		mStoreButton = std::make_unique<GameButton>(1);
		mStoreButton->mButtonImage = IMAGE_ZENSHOPBUTTON;
		mStoreButton->mOverImage = IMAGE_ZENSHOPBUTTON_HIGHLIGHT;
		mStoreButton->mDownImage = IMAGE_ZENSHOPBUTTON_HIGHLIGHT;
		mStoreButton->mParentWidget = this;
		mStoreButton->Resize(678, 33, IMAGE_ZENSHOPBUTTON->mWidth, 40);
	}
	else
	{
		mMenuButton->SetLabel("[MENU_BUTTON]");
		mMenuButton->Resize(681, -10, 117, 46);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		mStoreButton = std::make_unique<GameButton>(1);
		mStoreButton->mDrawStoneButton = true;
		mStoreButton->mBtnNoDraw = true;
		mStoreButton->mDisabled = true;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_UPSELL)
	{
		mMenuButton->SetLabel("[MAIN_MENU_BUTTON]");
		mMenuButton->Resize(628, -10, 163, 46);

		mStoreButton = std::make_unique<GameButton>(1);
		mStoreButton->mDrawStoneButton = true;
		mStoreButton->mBtnNoDraw = true;
		mStoreButton->SetLabel("[GET_FULL_VERSION_BUTTON]");
	}
}

Board::~Board() {
    if(mApp->mOnlineSession)mApp->mOnlineSession->SetObserverBoard(0,false);
    mObserverView.reset();
    RunSpeciesRuntime::Reset(this); NextWave::Reset(this); }

void BoardInitForPlayer()
{
	gShownMoreSunTutorial = false;
}

void Board::DisposeBoard()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
		mApp->mZenGarden->LeaveGarden();
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		mChallenge->TreeOfWisdomLeave();

	mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_RAIN);
	mApp->mZenGarden->mBoard = nullptr;
	mApp->CrazyDaveDie();
	mApp->mEffectSystem->EffectSystemFreeAll();
}

bool Board::AreEnemyZombiesOnScreen()
{
    if (RunSpeciesRuntime::Pending(this)) return true;
    if (mDeferredPlantSetup.IsWaiting()) return true;
    if (mRiseFromGraveCounter > 0 || mDeferredZombieSpawns.HasPending()) return true;
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if ((aZombie->mHasHead || IsRunBossZombie(aZombie) || aZombie->mZombieType == ZombieType::ZOMBIE_BOSS) && !aZombie->IsDeadOrDying() && !aZombie->mMindControlled)
		{
			return true;
		}
	}
	return false;
}

// Bosses remain required enemies even after losing a head or moving off screen.
bool Board::HasLivingBoss()
{
    Zombie* zombie = nullptr;
    while (IterateZombies(zombie))
    {
        if (!zombie->IsDeadOrDying() && !zombie->mMindControlled &&
            (IsRunBossZombie(zombie) || zombie->mZombieType == ZombieType::ZOMBIE_BOSS)) return true;
    }
    return false;
}

int Board::CountZombiesOnScreen()
{
	int aCount = 0;
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mHasHead && !aZombie->IsDeadOrDying() && !aZombie->mMindControlled && aZombie->IsOnBoard())
		{
			aCount++;
		}
	}
	return aCount;
}

int Board::GetLiveGargantuarCount() {
	int aCount = 0;
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mHasHead && !aZombie->IsDeadOrDying() && aZombie->IsOnBoard() && (aZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR))
		{
			aCount++;
		}
	}
	return aCount;
}

int Board::CountUntriggerLawnMowers()
{
	int aCount = 0;
	for (LawnMower* aLawnMower : mLawnMowers)
	{
		if (aLawnMower->mDead)
			continue;
		if (aLawnMower->mMowerState != LawnMowerState::MOWER_TRIGGERED && aLawnMower->mMowerState != LawnMowerState::MOWER_SQUISHED)
		{
			aCount++;
		}
	}
	return aCount;
}

void Board::TryToSaveGame()
{
    if(mApp->GetRunState() && mApp->GetRunState()->IsNewEndless()) {
        mApp->GetRunState()->Save(mApp->mPlayerInfo->mId);return;
    }

	std::string aFileName = GetSavedGameName(mApp->mGameMode, mApp->mPlayerInfo->mId);

	if (NeedSaveGame())
	{
		if (mBoardFadeOutCounter > 0)
		{
			CompleteEndLevelSequenceForSaving();
			return;
		}

		MkDir(GetAppDataPath("userdata"));
		mApp->mMusic->GameMusicPause(true);
		LawnSaveGame(this, aFileName);
		mApp->ClearUpdateBacklog();
		SurvivalSaveScore();
	}
}

bool Board::NeedSaveGame()
{
	if (mOnlineBoard) return false;
	// @pvz-online: checkpoints use their own format; never overwrite adventure mid-level saves.
	if (mApp->mRunState) return false;
	return
		mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ICE &&
		mApp->mGameMode != GameMode::GAMEMODE_UPSELL &&
		mApp->mGameMode != GameMode::GAMEMODE_INTRO &&
		mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN &&
		mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM &&
		mApp->mGameScene == GameScenes::SCENE_PLAYING;
}

void Board::SaveGame(const std::string& theFileName)
{
	LawnSaveGame(this, theFileName);
}

void Board::ResetFPSStats()
{
	int64_t aTickCount = SDL_GetTicks();
	mStartDrawTime = aTickCount;
	mIntervalDrawTime = aTickCount;
	mDrawCount = 1;
	mIntervalDrawCountStart = 1;
}

bool Board::LoadGame(const std::string& theFileName)
{
	if (!LawnLoadGame(this, theFileName))
		return false;

	LoadBackgroundImages();
	mApp->ClearUpdateBacklog();
	ResetFPSStats();
	UpdateLayers();
	mApp->mMusic->RestoreGameMusicAfterLoad();
	return true;
}

GridItem* Board::GetGridItemAt(GridItemType theGridItemType, int theGridX, int theGridY)
{
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridX == theGridX && aGridItem->mGridY == theGridY && aGridItem->mGridItemType == theGridItemType)
		{
			return aGridItem;
		}
	}
	return nullptr;
}

GridItem* Board::GetRake()
{
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_RAKE)
		{
			return aGridItem;
		}
	}
	return nullptr;
}

GridItem* Board::GetCraterAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_CRATER, theGridX, theGridY);
}

GridItem* Board::GetGraveStoneAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_GRAVESTONE, theGridX, theGridY);
}

GridItem* Board::GetLadderAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_LADDER, theGridX, theGridY);
}

GridItem* Board::GetScaryPotAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_SCARY_POT, theGridX, theGridY);
}

GridItem* Board::GetZenToolAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_ZEN_TOOL, theGridX, theGridY);
}

bool Board::CanAddGraveStoneAt(int theGridX, int theGridY)
{
	if (mGridSquareType[theGridX][theGridY] != GridSquareType::GRIDSQUARE_GRASS && mGridSquareType[theGridX][theGridY] != GridSquareType::GRIDSQUARE_HIGH_GROUND)
	{
		return false;
	}

	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridX == theGridX && aGridItem->mGridY == theGridY)
		{
			if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE ||
				aGridItem->mGridItemType == GridItemType::GRIDITEM_CRATER ||
				aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
				return false;
		}
	}
	return true;
}

int Board::MakeRenderOrder(RenderLayer theRenderLayer, int theRow, int theLayerOffset)
{
	return theRow * static_cast<int>(RenderLayer::RENDER_LAYER_ROW_OFFSET) + theRenderLayer + theLayerOffset;
}

GridItem* Board::AddALadder(int theGridX, int theGridY)
{
	GridItem* aLadder = mGridItems.DataArrayAlloc();
	aLadder->mGridItemType = GridItemType::GRIDITEM_LADDER;
	aLadder->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, theGridY, 800);
	aLadder->mGridX = theGridX;
	aLadder->mGridY = theGridY;
	return aLadder;
}

GridItem* Board::AddACrater(int theGridX, int theGridY)
{
	GridItem* aCrater = mGridItems.DataArrayAlloc();
	aCrater->mGridItemType = GridItemType::GRIDITEM_CRATER;
	aCrater->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, theGridY, 1);
	aCrater->mGridX = theGridX;
	aCrater->mGridY = theGridY;
	return aCrater;
}

GridItem* Board::AddAGraveStone(int theGridX, int theGridY)
{
	GridItem* aGraveStone = mGridItems.DataArrayAlloc();
	aGraveStone->mGridItemType = GridItemType::GRIDITEM_GRAVESTONE;
	aGraveStone->mGridItemCounter = -Rand(50);
	aGraveStone->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, theGridY, 3);
	aGraveStone->mGridX = theGridX;
	aGraveStone->mGridY = theGridY;
	return aGraveStone;
}

void Board::AddGraveStones(int theGridX, int theCount, MTRand& theLevelRNG)
{
	PVZP_ASSERT(theCount <= MAX_GRID_SIZE_Y);

	// Clamp theCount to the number of squares that can hold a grave stone, otherwise the loop below would never terminate
	//GridItem* aGridItem = nullptr;
	//bool aAllowGraveStone[MAX_GRID_SIZE_Y] = { false };
	int aGridAllowGraveStonesCount = 0;
	for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
	{
		if (CanAddGraveStoneAt(theGridX, y))
		{
			aGridAllowGraveStonesCount++;
		}
	}
	theCount = std::min(theCount, aGridAllowGraveStonesCount);

	int i = 0;
	while (i < theCount)
	{
		int aGridY = theLevelRNG.Next((unsigned long)MAX_GRID_SIZE_Y);
		//if (aAllowGraveStone[aGridY])
		//{
		//	aAllowGraveStone[aGridY] = false;
		//	GridItem* aGraveStone = AddAGraveStone(theGridX, aGridY);
		//	++i;
		//}
		// re-check each time instead of a cached allowance array, which could go stale if AddAGraveStone() changes
		if (CanAddGraveStoneAt(theGridX, aGridY))
		{
			AddAGraveStone(theGridX, aGridY);
			++i;
		}
	}
}

int Board::GetNumWavesPerFlag()
{
	return (mApp->IsFirstTimeAdventureMode() && mNumWaves < 10) ? mNumWaves : 10;
}

bool Board::IsFlagWave(int theWaveNumber)
{
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
		return false;

	int aWavesPerFlag = GetNumWavesPerFlag();
	return theWaveNumber % aWavesPerFlag == aWavesPerFlag - 1;
}

void ZombiePickerInitForWave(ZombiePicker* theZombiePicker)
{
	theZombiePicker->mZombieCount = 0;
	theZombiePicker->mZombiePoints = 0;
	memset(theZombiePicker->mZombieTypeCount, 0, sizeof(theZombiePicker->mZombieTypeCount));
}

void ZombiePickerInit(ZombiePicker* theZombiePicker)
{
	ZombiePickerInitForWave(theZombiePicker);
	memset(theZombiePicker->mAllWavesZombieTypeCount, 0, sizeof(theZombiePicker->mAllWavesZombieTypeCount));
}

void Board::PutZombieInWave(ZombieType theZombieType, int theWaveNumber, ZombiePicker* theZombiePicker)
{
	PVZP_ASSERT(theWaveNumber < MAX_ZOMBIE_WAVES && theZombiePicker->mZombieCount < MAX_ZOMBIES_IN_WAVE);
	mZombiesInWave[theWaveNumber][theZombiePicker->mZombieCount++] = theZombieType;
	if (theZombiePicker->mZombieCount < MAX_ZOMBIES_IN_WAVE)
	{
		mZombiesInWave[theWaveNumber][theZombiePicker->mZombieCount] = ZombieType::ZOMBIE_INVALID;
	}
	const int value = mApp->mRunState && (theZombieType == ZOMBIE_GARGANTUAR || theZombieType == ZOMBIE_REDEYE_GARGANTUAR) ?
		RunState::RUN_GARGANTUAR_VALUE : GetZombieDefinition(theZombieType).mZombieValue;
	theZombiePicker->mZombiePoints -= value;
	theZombiePicker->mZombieTypeCount[theZombieType]++;
	theZombiePicker->mAllWavesZombieTypeCount[theZombieType]++;
}

void Board::PutInMissingZombies(int theWaveNumber, ZombiePicker* theZombiePicker)
{
	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = static_cast<ZombieType>(static_cast<int>(aZombieType) + 1))
	{
		if (theZombiePicker->mZombieTypeCount[aZombieType] <= 0 && aZombieType != ZombieType::ZOMBIE_YETI && CanZombieSpawnOnLevel(aZombieType, mLevel))
		{
			PutZombieInWave(aZombieType, theWaveNumber, theZombiePicker);
		}
	}
}

void Board::PickZombieWaves()
{
    // @pvz-online: generate every original wave before reducing; preserve all shared RNG calls.
    std::vector<int> crowdProtectedPrefix(MAX_ZOMBIE_WAVES, 0);
	// ====================================================================================================
	// ▲ 设定关卡总波数
	// ====================================================================================================
	if (mApp->IsAdventureMode())
	{
		if (mApp->IsWhackAZombieLevel())
		{
			mNumWaves = 8;
		}
		else
		{
			mNumWaves = gZombieWaves[std::clamp(mLevel - 1, 0, 49)];
			// @pvz-online: 闯关不吃"+10"这一勺：波数固定是波表的原始值，
			// 谁玩、在哪个档案上玩都一样。难度靠关卡号、场景阶梯和怪的种类涨。
			if (!mApp->IsFirstTimeAdventureMode() && !mApp->IsMiniBossLevel() && !mApp->IsRunMode())
			{
				mNumWaves = mNumWaves < 10 ? 20 : mNumWaves + 10;
			}
		}
	}
	else
	{
		GameMode aGameMode = mApp->mGameMode;
		if (mApp->IsSurvivalMode() || aGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
			mNumWaves = GetNumWavesPerSurvivalStage();
		else if (aGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || aGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM || mApp->IsSquirrelLevel())
			mNumWaves = 0;
		else if (aGameMode == GameMode::GAMEMODE_CHALLENGE_WHACK_A_ZOMBIE)
			mNumWaves = 12;
		else if (aGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING || aGameMode == GameMode::GAMEMODE_CHALLENGE_AIR_RAID ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_GRAVE_DANGER || aGameMode == GameMode::GAMEMODE_CHALLENGE_HIGH_GRAVITY ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT || aGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL)
			mNumWaves = 20;
		else if (mApp->IsStormyNightLevel() || mApp->IsLittleTroubleLevel() || mApp->IsBungeeBlitzLevel() ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN || mApp->IsShovelLevel() || aGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS_2 ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING_2 || aGameMode == GameMode::GAMEMODE_CHALLENGE_POGO_PARTY)
			mNumWaves = 30;
		else
			mNumWaves = 40;
	}

	// ====================================================================================================
	// ▲ 一些准备工作
	// ====================================================================================================
	ZombiePicker aZombiePicker;
	ZombiePickerInit(&aZombiePicker);
	ZombieType aIntroZombieType = GetIntroducedZombieType();
	PVZP_ASSERT(mNumWaves <= MAX_ZOMBIE_WAVES);

	// ====================================================================================================
	// ▲ 遍历每一波并填充每波的出怪列表
	// ====================================================================================================
	for (int aWave = 0; aWave < mNumWaves; aWave++)
	{
		ZombiePickerInitForWave(&aZombiePicker);
		mZombiesInWave[aWave][0] = ZombieType::ZOMBIE_INVALID;

		bool aIsFlagWave = IsFlagWave(aWave);
		bool aIsFinalWave = aWave == mNumWaves - 1;

		if (mApp->IsBungeeBlitzLevel() && aIsFlagWave)
		{
			// 蹦极闪电战关卡的每大波固定刷出 5 只蹦极僵尸
			for (int _i = 0; _i < 5; _i++)
				PutZombieInWave(ZombieType::ZOMBIE_BUNGEE, aWave, &aZombiePicker);

			if (!aIsFinalWave)
				continue;
		}

		// ------------------------------------------------------------------------------------------------
		// △ 计算该波的僵尸总点数
		// ------------------------------------------------------------------------------------------------
		int& aZombiePoints = aZombiePicker.mZombiePoints;
		// 根据关卡计算本波的基础僵尸点数
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
		{
			aZombiePoints = (mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage() + aWave + 10) * 2 / 5 + 1;
		}
		else if (mApp->IsSurvivalMode() && mChallenge->mSurvivalStage > 0)
		{
			aZombiePoints = (mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage() + aWave) * 2 / 5 + 1;
		}
		else if (mApp->IsRunMode())
		{
			// 闯关（M4-a）：固定走"通关后"的出怪曲线。绝不挂在 HasFinishedAdventure 上——
			// 两个玩家的档案进度不同会让同一关的波表都不一样（R5 遗留的确定性漏洞）。
			// 2026-10-03（用户定案"丰富僵尸种类、不要减少僵尸数量"）：起步 +2 点/波——
			// 权重平铺后强怪占比上升，这点增幅把总只数抬回并略超原量，前期也能买得起多样怪。
			aZombiePoints = aWave * 2 / 5 + 3;
		}
		else if (mApp->IsAdventureMode() && mApp->HasFinishedAdventure() && mLevel != 5)
		{
			aZombiePoints = aWave * 2 / 5 + 1;
		}
		else
		{
			aZombiePoints = aWave / 3 + 1;
		}

		// 闯关的难度阶梯（M4-a）：数量随场景档同乘 ×1.5/场景（血量在 ZombieInitialize 里缩放）。
		// 乘在旗帜波 ×2.5 之前：旗帜波的总点数也一起吃这个系数。整数乘除，两端逐位一致。
		if (mApp->IsRunMode())
		{
			if(!mApp->GetRunState()->IsNewEndless())aZombiePoints = aZombiePoints * mApp->GetRunState()->GetDifficultyPermille() / 1000;
		}

		// @pvz-online: 席位顺位刷怪乘数（2026-10-03 用户二次定案：改回 2 的幂，取代当日的线性 4:3:2:1）：
		// 末席（最后上座的席位）原量 ×2（2026-10-04 批七由 ×1 抬升）；往前每升一位翻一倍：
		// 四人 8:4:2:2、三人 4:2:2、二人 2:2（末两席同为 ×2）；五六人续 16、32，顶到 32 封顶
		// （用户定）。顺位按"上座席位"排（第 r 位权重 2^(n-r)、末位保底 ×2）；
		// 各客户端只为自己的棋盘缩放，不涉协议。乘在旗帜波 ×2.5 之前。
		// 同一倍率也放大每波数量封顶（下面 aWaveZombieCap）：1 号位 32 倍时 20 → 640 只。
		// 2026-10-06：公式抽到 LawnApp::OnlineSeatMultiplier（顺位开局阳光同用）。
		int aSeatMult = mApp->OnlineSeatMultiplier();

		// @pvz-online: 出怪难度档（2026-10-03 用户定案）：房主开局前选的全局旋钮，
		// 直接乘在顺位乘数上——轻松 ×0.5 / 高压 ×2.0，乘后向下取整、保底 1 倍
		// （末位 2×0.5 保底 1；顺位形状 32:16:8:4:2:2 在高压下翻倍变 64:32:16:8:4:4）。
		// 只在闯关局生效；非闯关局这两个值恒为标准档，乘出来还是原值。
		// MOD_BUILD 37 起这段不再躲在 if (aSeatMult > 1) 里：单机（×1）的无尽局也要吃
		// 难度/规模档（无尽选项页对单机就摆这一行）。普通单机局档位恒为标准（×1 恒等）、
		// 行为逐位不变。
		int aSpawnPermille = aSeatMult * 1000;
        if(mApp->IsRunMode() && mApp->GetRunState()->IsNewEndless())
            aSpawnPermille=EndlessRules::SpawnPermille(aSeatMult,RunState::DiffPermilleFor(mApp->GetRunState()->mDiff),RunState::ScalePermilleFor(mApp->GetRunState()->mScale));
		if (mApp->IsRunMode() && mApp->GetRunState() != nullptr && !mApp->GetRunState()->IsNewEndless())
		{
			aSeatMult = aSeatMult * RunState::DiffPermilleFor(mApp->GetRunState()->mDiff) / 1000;
			if (aSeatMult < 1) aSeatMult = 1;

			// @pvz-online: 高级选项·规模档（批 C，MOD_BUILD 35）：难度档之后再乘一档
			// 出怪规模（少 ×0.5 / 多 ×2 / 海量 ×4），乘后向下取整、保底 1——与难度档
			// 同一条纪律（每波数量封顶在下面 aWaveZombieCap 随同一倍率放大后钳硬顶）。
			aSeatMult = aSeatMult * RunState::ScalePermilleFor(mApp->GetRunState()->mScale) / 1000;
			if (aSeatMult < 1) aSeatMult = 1;
		}

		if(mApp->IsRunMode() && mApp->GetRunState()->IsNewEndless())
            aZombiePoints=std::max(1,(int)((long long)aZombiePoints*mApp->GetRunState()->GetDifficultyPermille()*aSpawnPermille/1000000));
        else aZombiePoints *= aSeatMult;

		// 旗帜波的特殊调整
		if (aIsFlagWave)
		{
			int aPlainZombiesNum = std::min(aZombiePoints, 8);
			aZombiePoints *= 2.5f;

			if (mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS_2)
			{
				for (int _i = 0; _i < aPlainZombiesNum; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_NORMAL, aWave, &aZombiePicker);
				}
				PutZombieInWave(ZombieType::ZOMBIE_FLAG, aWave, &aZombiePicker);
			}
		}

		// 部分关卡的多倍出怪
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
		{
			aZombiePoints *= 6;
		}
		else if (mApp->IsLittleTroubleLevel() || mApp->IsWallnutBowlingLevel())
		{
			aZombiePoints *= 4;
		}
		else if (mApp->IsMiniBossLevel())
		{
			aZombiePoints *= 3;
		}
		else if (mApp->IsStormyNightLevel() && mApp->IsAdventureMode())
		{
			aZombiePoints *= 3;
		}
		else if (mApp->IsShovelLevel() || mApp->IsBungeeBlitzLevel() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL)
		{
			aZombiePoints *= 2;
		}

		// ------------------------------------------------------------------------------------------------
		// △ 向出怪列表中加入固定刷出的僵尸
		// ------------------------------------------------------------------------------------------------
		// 部分新出现的僵尸会在特定波固定刷出
		// @pvz-online: 闯关里"初次登场固定刷一只"也要过名单（M4-a）：原版按 startingLevel==mLevel
		// 找登场怪，闯关抽的是中后段关号，会撞上原版意义上的登场怪（如 L40 的雪人）——
		// 名单里没有它就不许塞。
		if (aIntroZombieType != ZombieType::ZOMBIE_INVALID && aIntroZombieType != ZombieType::ZOMBIE_DUCKY_TUBE &&
			(!mApp->IsRunMode() || CanZombieSpawnOnLevel(aIntroZombieType, mLevel)))
		{
			bool aSpawnIntro = false;
			if ((aIntroZombieType == ZombieType::ZOMBIE_DIGGER || aIntroZombieType == ZombieType::ZOMBIE_BALLOON))
			{
				if (aWave + 1== 7 || aIsFinalWave)
				{
					aSpawnIntro = true;
				}
			}
			else if (aIntroZombieType == ZombieType::ZOMBIE_YETI)
			{
				if (aWave == mNumWaves / 2 && !mApp->mSawYeti)
				{
					aSpawnIntro = true;
				}
			}
			else if (aWave == mNumWaves / 2 || aIsFinalWave)
			{
				aSpawnIntro = true;
			}

			if (aSpawnIntro)
			{
				PutZombieInWave(aIntroZombieType, aWave, &aZombiePicker);
			}
		}

		// 5-10 关卡的最后一波加入一只伽刚特尔
		if (mLevel == 50 && aIsFinalWave)
		{
			PutZombieInWave(ZombieType::ZOMBIE_GARGANTUAR, aWave, &aZombiePicker);
		}
		// 冒险模式关卡的最后一波会出现本关卡可能出现的所有僵尸
		// @pvz-online: 闯关不调它（M4-a 用户定案"关掉"）：名单里的怪本来就全程按权重出，
		// "末波挨个露面"反而把没抽到的高价怪硬塞进最后一波，破坏"点数多出强怪"的节奏。
		if (mApp->IsAdventureMode() && aIsFinalWave && !mApp->IsRunMode())
		{
			PutInMissingZombies(aWave, &aZombiePicker);
		}
		// 柱子关卡的特殊出怪
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
		{
			// 每大波的第 5 小波，固定出现 10 只扶梯僵尸
			if (aWave % 10 == 5)
			{
				for (int _i = 0; _i < 10; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_LADDER, aWave, &aZombiePicker);
				}
			}

			// 每大波的第 8 小波，固定出现 10 只玩偶匣僵尸
			if (aWave % 10 == 8)
			{
				for (int _i = 0; _i < 10; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_JACK_IN_THE_BOX, aWave, &aZombiePicker);
				}
			}

			// 第 19/29 小波，固定出现 3/5 只伽刚特尔
			if (aWave == 19)
			{
				for (int _i = 0; _i < 3; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_GARGANTUAR, aWave, &aZombiePicker);
				}
			}
			if (aWave == 29)
			{
				for (int _i = 0; _i < 5; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_GARGANTUAR, aWave, &aZombiePicker);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------
		// △ 剩余的僵尸点数用于向列表中补充随机僵尸
		// ------------------------------------------------------------------------------------------------
		// @pvz-online: 闯关"数量封顶"（M4-a 定案）：点数不封顶，但一波最多 RUN_WAVE_ZOMBIE_CAP 只；
		// 预算花不完的零头直接作废——富余的点数靠 PickZombieType 的强僵尸优先花在质量上。
		// 联机再按席位顺位乘数同倍放大（2026-10-03 幂次口径：六人局 1 号位 32 倍 → 闯关上限
		// 20×32 = 640 只、高压再 ×2.0 → 1280 只（仍小于数组上限）；非闯关 50×32 = 1600 = 数组上限，正好兜住）；
		// 非闯关基准 = 原版 50，数组上限 MAX_ZOMBIES_IN_WAVE 按基准×32 留量。
		int aWaveZombieCap = (mApp->IsRunMode() ? RunState::RUN_WAVE_ZOMBIE_CAP : WAVE_ZOMBIE_CAP_BASE) * aSeatMult;
		if(mApp->IsRunMode() && mApp->GetRunState()->IsNewEndless())
            aWaveZombieCap=std::max(1,RunState::RUN_WAVE_ZOMBIE_CAP*aSpawnPermille/1000);
        // @pvz-online: 高级选项·规模档（批 C）把顺位乘数顶到 ×128（×32 顺位 × ×4 规模）：
		// 非闯关 50×128 = 6400 会撞 MAX_ZOMBIES_IN_WAVE 的数组上限并触发断言——钳回硬顶
		// 减一。原基准 50×32 本来就正好贴着上限，这里只是把"贴着"变成"钳着"。
		if (aWaveZombieCap > MAX_ZOMBIES_IN_WAVE - 1) aWaveZombieCap = MAX_ZOMBIES_IN_WAVE - 1;
        if(mApp->IsRunMode() && mApp->GetRunState()->IsNewEndless()) {
            // Scripted flag-wave entries must obey the same low-scale cap as random entries.
            if(aZombiePicker.mZombieCount>aWaveZombieCap && mZombiesInWave[aWave][aZombiePicker.mZombieCount-1]==ZombieType::ZOMBIE_FLAG)
                std::swap(mZombiesInWave[aWave][0],mZombiesInWave[aWave][aZombiePicker.mZombieCount-1]);
            while(aZombiePicker.mZombieCount>aWaveZombieCap) {
                int index=--aZombiePicker.mZombieCount;
                ZombieType type=mZombiesInWave[aWave][index];
                if(type>=0 && type<ZombieType::NUM_ZOMBIE_TYPES)--aZombiePicker.mZombieTypeCount[(int)type];
                mZombiesInWave[aWave][index]=ZombieType::ZOMBIE_INVALID;
            }
        }
        crowdProtectedPrefix[aWave] = aZombiePicker.mZombieCount;
		while (aZombiePoints > 0 && aZombiePicker.mZombieCount < aWaveZombieCap)
		{
			ZombieType aZombieType = PickZombieType(aZombiePoints, aWave, &aZombiePicker);
			PutZombieInWave(aZombieType, aWave, &aZombiePicker);
		}

		// @pvz-online: 每波构成落日志（47，取证惯例——「出怪不对」类报障直接对 log.txt
		// 对数，不再靠猜 RNG）。只闯关局记；伴舞不入波表，日志里见不到（落地才翻）。
		if (mApp->IsRunMode())
		{
			char aWaveLog[512];
			int aLeft = aZombiePoints > 0 ? aZombiePoints : 0;
			int aLen = snprintf(aWaveLog, sizeof(aWaveLog), "[wave] lvl=%d wave=%d/%d pts_left=%d picks=%d:",
				mLevel, aWave + 1, mNumWaves, aLeft, aZombiePicker.mZombieCount);
			for (int aZombieType = 0; aZombieType < ZombieType::NUM_ZOMBIE_TYPES && aLen > 0 && aLen < (int)sizeof(aWaveLog) - 1; aZombieType++)
			{
				int aCnt = aZombiePicker.mZombieTypeCount[aZombieType];
				if (aCnt <= 0) continue;
				const ZombieDefinition& aLogDef = GetZombieDefinition((ZombieType)aZombieType);
				char aZName[48];
				int aNameLen = 0;
				while (aLogDef.mZombieName[aNameLen] != '\0' && aNameLen < (int)sizeof(aZName) - 1)
				{
					aZName[aNameLen] = (char)aLogDef.mZombieName[aNameLen];
					aNameLen++;
				}
				aZName[aNameLen] = '\0';
				int aWritten = snprintf(aWaveLog + aLen, sizeof(aWaveLog) - aLen, " %s x%d", aZName, aCnt);
				if (aWritten < 0) break;
				aLen += aWritten;
			}
			NetworkLog("%s", aWaveLog);
		}
	}
    // @pvz-online: one post-generation pass; raw lawn restoration overwrites saved wave tables.
    if (mApp->IsRunMode() && mApp->GetRunState()->UsesCrowdCompression()) {
        for (int wave = 0; wave < mNumWaves; ++wave) {
            int count = 0;
            while (count < MAX_ZOMBIES_IN_WAVE && mZombiesInWave[wave][count] != ZombieType::ZOMBIE_INVALID) ++count;
            if (count <= CrowdRules::THRESHOLD || count >= MAX_ZOMBIES_IN_WAVE) continue;
            std::vector<int> protectedIndices, kept;
            for (int i = 0; i < count; ++i)
                if (i < crowdProtectedPrefix[wave] || mZombiesInWave[wave][i] == ZombieType::ZOMBIE_FLAG) protectedIndices.push_back(i);
            const uint32_t seed = CrowdRules::Mix(static_cast<uint32_t>(mApp->GetRunState()->GetLevelSeed()) ^ static_cast<uint32_t>(wave));
            if (!CrowdRules::PlanWave(count, mApp->GetRunState()->mCrowdRule, seed, protectedIndices, kept)) {
                PvzpLogLn("[crowd] compression rejected wave={} original={}", wave+1, count);
                continue;
            }
            for (size_t i = 0; i < kept.size(); ++i) mZombiesInWave[wave][i] = mZombiesInWave[wave][kept[i]];
            for (size_t i = kept.size(); i < static_cast<size_t>(count); ++i) mZombiesInWave[wave][i] = ZombieType::ZOMBIE_INVALID;
            PvzpLogLn("[crowd] wave={} original={} retained={}", wave+1, count, kept.size());
        }
    }

}

int Board::GetLevelRandSeed()
{
	// @pvz-online: 联机局里两边的波表必须同源，客户端直接用主机广播的那一份——
	// 本机自己算永远对不上（这个式子里有各自的存档 ID 和各自的开局随机数）。
	int anOnlineLevel = 0, anOnlineSeed = 0;
	if (mApp->GetOnlineStartOverride(anOnlineLevel, anOnlineSeed))
		return anOnlineSeed;

	// 非冒险模式才碰 mChallenge（原式就是如此），所以先分流再取生存阶段
	int aSurvivalStage = mApp->IsAdventureMode() ? 0 : mChallenge->mSurvivalStage;
	return ComputeLevelRandSeed(mBoardRandSeed, mApp->IsAdventureMode(), mApp->mPlayerInfo->mId,
		mApp->mPlayerInfo->mFinishedAdventure, mLevel, aSurvivalStage, (int)mApp->mGameMode);
}

void Board::LoadBackgroundImages()
{
	switch (mBackground)
	{
	case BackgroundType::BACKGROUND_1_DAY:
		mLoadedResourceNames.push_back("DelayLoad_Background1");
		if ((mApp->IsAdventureMode() && mLevel <= 4) || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RESODDED)
		{
			mLoadedResourceNames.push_back("DelayLoad_BackgroundUnsodded");
		}
		break;

	case BackgroundType::BACKGROUND_2_NIGHT:
		mLoadedResourceNames.push_back("DelayLoad_Background2");
		break;

	case BackgroundType::BACKGROUND_3_POOL:
		mLoadedResourceNames.push_back("DelayLoad_Background3");
		break;

	case BackgroundType::BACKGROUND_4_FOG:
		mLoadedResourceNames.push_back("DelayLoad_Background4");
		break;

	case BackgroundType::BACKGROUND_5_ROOF:
		mLoadedResourceNames.push_back("DelayLoad_Background5");
		break;

	case BackgroundType::BACKGROUND_6_BOSS:
		mLoadedResourceNames.push_back("DelayLoad_Background6");
		break;

	case BackgroundType::BACKGROUND_GREENHOUSE:
		mLoadedResourceNames.push_back("DelayLoad_GreenHouseGarden");
		mLoadedResourceNames.push_back("DelayLoad_GreenHouseOverlay");
		break;

	case BackgroundType::BACKGROUND_TREEOFWISDOM:
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_TREEOFWISDOM, true);
		break;

	case BackgroundType::BACKGROUND_ZOMBIQUARIUM:
		mLoadedResourceNames.push_back("DelayLoad_Zombiquarium");
		mLoadedResourceNames.push_back("DelayLoad_GreenHouseOverlay");
		break;

	case BackgroundType::BACKGROUND_MUSHROOM_GARDEN:
		mLoadedResourceNames.push_back("DelayLoad_MushroomGarden");
		break;

	default:
		PVZP_ASSERT(false);
		break;
	}

	for (std::string& resource : mLoadedResourceNames)
		PvzpLoadResources(resource.c_str());
}

void Board::PickBackground()
{
	switch (mApp->mGameMode)
	{
	case GameMode::GAMEMODE_ADVENTURE:
		if (mLevel <= 1 * LEVELS_PER_AREA)
		{
			mBackground = BackgroundType::BACKGROUND_1_DAY;
		}
		else if (mLevel <= 2 * LEVELS_PER_AREA)
		{
			mBackground = BackgroundType::BACKGROUND_2_NIGHT;
		}
		else if (mLevel <= 3 * LEVELS_PER_AREA)
		{
			mBackground = BackgroundType::BACKGROUND_3_POOL;
		}
		else if (mApp->IsScaryPotterLevel())
		{
			mBackground = BackgroundType::BACKGROUND_2_NIGHT;
		}
		else if (mLevel <= 4 * LEVELS_PER_AREA)
		{
			mBackground = BackgroundType::BACKGROUND_4_FOG;
		}
		else if (mLevel < FINAL_LEVEL)
		{
			mBackground = BackgroundType::BACKGROUND_5_ROOF;
		}
		else if (mLevel == FINAL_LEVEL)
		{
			mBackground = BackgroundType::BACKGROUND_6_BOSS;
		}
		else
		{
			mBackground = BackgroundType::BACKGROUND_1_DAY;
		}
		break;

	case GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1:
	case GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_1:
	case GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_1:
	case GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS:
	case GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING:
	case GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE:
	case GameMode::GAMEMODE_CHALLENGE_SEEING_STARS:
	case GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING_2:
	case GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_WALLNUT:
	case GameMode::GAMEMODE_CHALLENGE_SUNNY_DAY:
	case GameMode::GAMEMODE_CHALLENGE_RESODDED:
	case GameMode::GAMEMODE_CHALLENGE_BIG_TIME:
	case GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_SUNFLOWER:
	case GameMode::GAMEMODE_CHALLENGE_ICE:
	case GameMode::GAMEMODE_CHALLENGE_SHOVEL:
	case GameMode::GAMEMODE_CHALLENGE_SQUIRREL:
		mBackground = BackgroundType::BACKGROUND_1_DAY;
		break;

	case GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_2:
	case GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_2:
	case GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_2:
	case GameMode::GAMEMODE_CHALLENGE_BEGHOULED:
	case GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST:
	case GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT:
	case GameMode::GAMEMODE_CHALLENGE_WHACK_A_ZOMBIE:
	case GameMode::GAMEMODE_CHALLENGE_GRAVE_DANGER:
	case GameMode::GAMEMODE_SCARY_POTTER_1:
	case GameMode::GAMEMODE_SCARY_POTTER_2:
	case GameMode::GAMEMODE_SCARY_POTTER_3:
	case GameMode::GAMEMODE_SCARY_POTTER_4:
	case GameMode::GAMEMODE_SCARY_POTTER_5:
	case GameMode::GAMEMODE_SCARY_POTTER_6:
	case GameMode::GAMEMODE_SCARY_POTTER_7:
	case GameMode::GAMEMODE_SCARY_POTTER_8:
	case GameMode::GAMEMODE_SCARY_POTTER_9:
	case GameMode::GAMEMODE_SCARY_POTTER_ENDLESS:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_2:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_4:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_5:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_6:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_7:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_8:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_9:
	case GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS:
		mBackground = BackgroundType::BACKGROUND_2_NIGHT;
		break;

	case GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_3:
	case GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_3:
	case GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_3:
	case GameMode::GAMEMODE_CHALLENGE_LITTLE_TROUBLE:
	case GameMode::GAMEMODE_CHALLENGE_BOBSLED_BONANZA:
	case GameMode::GAMEMODE_CHALLENGE_SPEED:
	case GameMode::GAMEMODE_CHALLENGE_LAST_STAND:
	case GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS_2:
	case GameMode::GAMEMODE_UPSELL:
	case GameMode::GAMEMODE_INTRO:
		mBackground = BackgroundType::BACKGROUND_3_POOL;
		break;

	case GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_4:
	case GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_4:
	case GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_4:
	case GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS:
	case GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL:
	case GameMode::GAMEMODE_CHALLENGE_AIR_RAID:
	case GameMode::GAMEMODE_CHALLENGE_STORMY_NIGHT:
		mBackground = BackgroundType::BACKGROUND_4_FOG;
		break;

	case GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_5:
	case GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_5:
	case GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_5:
	case GameMode::GAMEMODE_CHALLENGE_COLUMN:
	case GameMode::GAMEMODE_CHALLENGE_POGO_PARTY:
	case GameMode::GAMEMODE_CHALLENGE_HIGH_GRAVITY:
	case GameMode::GAMEMODE_CHALLENGE_BUNGEE_BLITZ:
		mBackground = BackgroundType::BACKGROUND_5_ROOF;
		break;

	case GameMode::GAMEMODE_CHALLENGE_FINAL_BOSS:
		mBackground = BackgroundType::BACKGROUND_6_BOSS;
		break;

	case GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM:
		mBackground = BackgroundType::BACKGROUND_ZOMBIQUARIUM;
		break;

	case GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN:
		mBackground = BackgroundType::BACKGROUND_GREENHOUSE;
		break;

	case GameMode::GAMEMODE_TREE_OF_WISDOM:
		mBackground = BackgroundType::BACKGROUND_TREEOFWISDOM;
		break;

	default:
		PVZP_ASSERT(false);
		break;
	}
	LoadBackgroundImages();

	if (mBackground == BackgroundType::BACKGROUND_1_DAY || mBackground == BackgroundType::BACKGROUND_GREENHOUSE || mBackground == BackgroundType::BACKGROUND_TREEOFWISDOM)
	{
		mPlantRow[0] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[1] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[2] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[3] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[4] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[5] = PlantRowType::PLANTROW_DIRT;

		if (mApp->IsAdventureMode() && mApp->IsFirstTimeAdventureMode())
		{
			if (mLevel == 1)
			{
				mPlantRow[0] = PlantRowType::PLANTROW_DIRT;
				mPlantRow[1] = PlantRowType::PLANTROW_DIRT;
				mPlantRow[3] = PlantRowType::PLANTROW_DIRT;
				mPlantRow[4] = PlantRowType::PLANTROW_DIRT;
			}
			else if (mLevel == 2 || mLevel == 3)
			{
				mPlantRow[0] = PlantRowType::PLANTROW_DIRT;
				mPlantRow[4] = PlantRowType::PLANTROW_DIRT;
			}
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RESODDED)
		{
			mPlantRow[0] = PlantRowType::PLANTROW_DIRT;
			mPlantRow[4] = PlantRowType::PLANTROW_DIRT;
		}
	}
	else if (mBackground == BackgroundType::BACKGROUND_2_NIGHT)
	{
		mPlantRow[0] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[1] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[2] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[3] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[4] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[5] = PlantRowType::PLANTROW_DIRT;
	}
	else if (mBackground == BackgroundType::BACKGROUND_3_POOL || mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM || mBackground == BackgroundType::BACKGROUND_4_FOG)
	{
		mPlantRow[0] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[1] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[2] = PlantRowType::PLANTROW_POOL;
		mPlantRow[3] = PlantRowType::PLANTROW_POOL;
		mPlantRow[4] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[5] = PlantRowType::PLANTROW_NORMAL;
	}
	else if (mBackground == BackgroundType::BACKGROUND_5_ROOF || mBackground == BackgroundType::BACKGROUND_6_BOSS)
	{
		mPlantRow[0] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[1] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[2] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[3] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[4] = PlantRowType::PLANTROW_NORMAL;
		mPlantRow[5] = PlantRowType::PLANTROW_DIRT;
	}
	else
	{
		PVZP_ASSERT(false);
	}

	for (int x = 0; x < MAX_GRID_SIZE_X; x++)
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
		{
			if (mPlantRow[y] == PlantRowType::PLANTROW_DIRT)
			{
				mGridSquareType[x][y] = GridSquareType::GRIDSQUARE_DIRT;
			}
			else if (mPlantRow[y] == PlantRowType::PLANTROW_POOL && x >= 0 && x <= 8)
			{
				mGridSquareType[x][y] = GridSquareType::GRIDSQUARE_POOL;
			}
			else if (mPlantRow[y] == PlantRowType::PLANTROW_HIGH_GROUND && x >= 4 && x <= 8)
			{
				mGridSquareType[x][y] = GridSquareType::GRIDSQUARE_HIGH_GROUND;
			}
		}
	}

	MTRand aLevelRNG(GetLevelRandSeed());
	if (StageHasGraveStones())
	{
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_GRAVE_DANGER)
		{
			AddGraveStones(6, RandRangeInt(1, 2), aLevelRNG);
			AddGraveStones(7, RandRangeInt(1, 3), aLevelRNG);
			AddGraveStones(8, RandRangeInt(2, 3), aLevelRNG);
		}
		else if (mApp->IsWhackAZombieLevel())
		{
			mChallenge->WhackAZombiePlaceGraves(9);
		}
		else if (mBackground == BackgroundType::BACKGROUND_2_NIGHT)
		{
			if (mApp->IsSurvivalNormal(mApp->mGameMode))
			{
				AddGraveStones(5, 1, aLevelRNG);
				AddGraveStones(6, 1, aLevelRNG);
				AddGraveStones(7, 1, aLevelRNG);
				AddGraveStones(8, 2, aLevelRNG);
			}
			else if (!mApp->IsAdventureMode())
			{
				AddGraveStones(4, 1, aLevelRNG);
				AddGraveStones(5, 1, aLevelRNG);
				AddGraveStones(6, 2, aLevelRNG);
				AddGraveStones(7, 2, aLevelRNG);
				AddGraveStones(8, 3, aLevelRNG);
			}
			else if (mLevel == 11 || mLevel == 12 || mLevel == 13)
			{
				AddGraveStones(6, 1, aLevelRNG);
				AddGraveStones(7, 1, aLevelRNG);
				AddGraveStones(8, 2, aLevelRNG);
			}
			else if (mLevel == 14 || mLevel == 16)
			{
				AddGraveStones(5, 1, aLevelRNG);
				AddGraveStones(6, 1, aLevelRNG);
				AddGraveStones(7, 2, aLevelRNG);
				AddGraveStones(8, 3, aLevelRNG);
			}
			else if (mLevel == 17 || mLevel == 18 || mLevel == 19)
			{
				AddGraveStones(4, 1, aLevelRNG);
				AddGraveStones(5, 2, aLevelRNG);
				AddGraveStones(6, 2, aLevelRNG);
				AddGraveStones(7, 3, aLevelRNG);
				AddGraveStones(8, 3, aLevelRNG);
			}
			else if (mLevel >= 20)
			{
				AddGraveStones(3, 1, aLevelRNG);
				AddGraveStones(4, 2, aLevelRNG);
				AddGraveStones(5, 2, aLevelRNG);
				AddGraveStones(6, 2, aLevelRNG);
				AddGraveStones(7, 3, aLevelRNG);
				AddGraveStones(8, 3, aLevelRNG);
			}
			else
			{
				PVZP_ASSERT(false);
			}
		}
	}
	PickSpecialGraveStone();
}

void Board::InitZombieWavesForLevel(int theForLevel)
{
	if (mApp->IsWhackAZombieLevel() || (mApp->IsWallnutBowlingLevel() && !mApp->IsFirstTimeAdventureMode()))
	{
		mChallenge->InitZombieWaves();
		return;
	}

	for (int aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType++)
	{
		mZombieAllowed[aZombieType] = CanZombieSpawnOnLevel(static_cast<ZombieType>(aZombieType), theForLevel);
	}
}

bool Board::IsZombieWaveDistributionOk()
{
	if (mApp->mRunState || !mApp->IsAdventureMode())
		return true;

	int aZombieTypeCount[ZombieType::NUM_ZOMBIE_TYPES] = { 0 };
	for (int aWave = 0; aWave < mNumWaves; aWave++)
	{
		for (int aIndex = 0; aIndex < MAX_ZOMBIES_IN_WAVE; aIndex++)
		{
			ZombieType aZombieType = mZombiesInWave[aWave][aIndex];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
			{
				break;
			}

			PVZP_ASSERT(aZombieType >= 0 && aZombieType < ZombieType::NUM_ZOMBIE_TYPES);
			aZombieTypeCount[aZombieType]++;
		}
	}

	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = static_cast<ZombieType>(static_cast<int>(aZombieType) + 1))
	{
		if (aZombieType != ZombieType::ZOMBIE_YETI && CanZombieSpawnOnLevel(aZombieType, mLevel) && aZombieTypeCount[aZombieType] == 0)
		{
			PvzpLogLn("Didn't spawn required zombie {}, level {}", GetZombieDefinition(aZombieType).mZombieName, mLevel);
			return false;
		}
	}
	return true;
}

void Board::InitZombieWaves()
{
    NextWave::Reset(this);
	memset(mZombieAllowed, false, sizeof(mZombieAllowed));
	if (mApp->IsAdventureMode())
	{
		InitZombieWavesForLevel(mLevel);
	}
	else
	{
		mChallenge->InitZombieWaves();
	}
	PickZombieWaves();
	PVZP_ASSERT(IsZombieWaveDistributionOk());

	mCurrentWave = 0;
	mTotalSpawnedWaves = 0;
	mApp->mSawYeti = false;
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 2)
	{
		mZombieCountDown = ZOMBIE_COUNTDOWN * 2;
	}
	else if (mApp->IsSurvivalMode() && mChallenge->mSurvivalStage > 0)
	{
		mZombieCountDown = ZOMBIE_COUNTDOWN_RANGE;
	}
	else
	{
		mZombieCountDown = ZOMBIE_COUNTDOWN_FIRST_WAVE;
	}

	if (mApp->IsRunMode() && (!mApp->GetRunState()->IsEndless() || mApp->GetRunState()->IsNewEndless()))
		mZombieCountDown = mApp->GetRunState()->FirstWaveDelayTicks();

	mWaveHealth.start = 0;
    mWaveSpawnRetry = {};
	mLastBungeeWave = 0;
	mProgressMeterWidth = 0;
	mHugeWaveCountDown = 0;
	mLevelAwardSpawned = false;
	mZombieCountDownStart = mZombieCountDown;
	mWaveHealth.next = -1;
    SyncLegacyWaveHealth();
}

void Board::FreezeEffectsForCutscene(bool theFreeze)
{
	for (PvzpParticleSystem* aParticle : mApp->mEffectSystem->mParticleHolder->mParticleSystems)
	{
		if (aParticle->mDead)
			continue;
		if (aParticle->mEffectType == ParticleEffect::PARTICLE_GRAVE_BUSTER)
		{
			aParticle->mDontUpdate = theFreeze;
		}
		else if (aParticle->mEffectType == ParticleEffect::PARTICLE_POOL_SPARKLY && mIceTrapCounter == 0)
		{
			aParticle->mDontUpdate = theFreeze;
		}
	}

	for (Reanimation* aReanim : mApp->mEffectSystem->mReanimationHolder->mReanimations)
	{
		if (aReanim->mDead)
			continue;
		if (aReanim->mReanimationType == ReanimationType::REANIM_SLEEPING)
		{
			aReanim->mAnimRate = theFreeze ? 0.0f : RandRangeFloat(6, 8);
		}
	}
}

void Board::InitSurvivalStage()
{
	RefreshSeedPacketFromCursor();
	mApp->mSoundSystem->GamePause(true);
	FreezeEffectsForCutscene(true);
	mLevelComplete = false;
	InitZombieWaves();
	mApp->mGameScene = GameScenes::SCENE_LEVEL_INTRO;
	mApp->ShowSeedChooserScreen();
	mCutScene->StartLevelIntro();
	mSeedBank->UpdateWidth();

	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		SeedPacket* aPacket = &mSeedBank->mSeedPackets[i];
		aPacket->mX = GetSeedPacketPositionX(i);
		aPacket->mPacketType = SeedType::SEED_NONE;
	}

	if (StageHasFog())
	{
		mFogBlownCountDown = FOG_BLOW_RETURN_TIME;
	}
	for (int j = 0; j < MAX_GRID_SIZE_Y; j++)
	{
		mWaveRowGotLawnMowered[j] = -100;
	}
}

Rect Board::GetShovelButtonRect()
{
	Rect aRect(GetSeedBankExtraWidth() + 456, 0, Sexy::IMAGE_SHOVELBANK->GetWidth(), Sexy::IMAGE_SHOVELBANK->GetHeight());
	if (mApp->IsSlotMachineLevel() || mApp->IsSquirrelLevel())
	{
		aRect.mX = 600;
	}
    if(mApp->IsOnlineGame()&&!mApp->IsLocalOnlyMode(mApp->mGameMode))
        aRect=Rect(604,0,48,Sexy::IMAGE_SEEDBANK->GetHeight());
	return aRect;
}

void Board::GetZenButtonRect(GameObjectType theObjectType, Rect& theRect)
{
	// theRect must be pre-initialized with the shovel button rect; this function only adjusts its X

	theRect.mX = 30;
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		return;
	}

	bool usable = true;
	for (int anObject = GameObjectType::OBJECT_TYPE_WATERING_CAN; anObject <= GameObjectType::OBJECT_TYPE_WHEELBARROW; anObject++)
	{
		if (!CanUseGameObject((GameObjectType)anObject))
		{
			usable = false;
			break;
		}
	}
	if (usable)
	{
		theRect.mX = 0;
	}

	int aShovelWidth = Sexy::IMAGE_SHOVELBANK->GetWidth();
	for (int anObject = GameObjectType::OBJECT_TYPE_WATERING_CAN; anObject < theObjectType; anObject++)
	{
		if (CanUseGameObject((GameObjectType)anObject))
		{
			theRect.mX += aShovelWidth;
		}
	}
	//return theRect;
}

void Board::InitLevel()
{
    NextWave::Reset(this);
    mApp->mDamageStats.BeginLevel(this);
	mOnlineBoard = mApp->IsOnlineGame();
	mRunBossZombieID = ZombieID::ZOMBIEID_NULL;
	mMainCounter = 0;
	mBoardUpdateCounter = 0;
	mEnableGraveStones = false;
	mSodPosition = 0;
	mPrevBoardResult = mApp->mBoardResult;

	GameMode aGameMode = mApp->mGameMode;
	if (aGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM && aGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		mApp->mMusic->StopAllMusic();
	}
    int level = 0, seed = 0;
    if (mApp->GetOnlineStartOverride(level, seed)) mLevel = level;
    else mLevel = mApp->mRunState ? mApp->mRunState->GetLevel() : mApp->IsAdventureMode() ? mApp->mPlayerInfo->mLevel : 0;
	PickBackground();
	InitZombieWaves();
	if (aGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || aGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		mApp->IsScaryPotterLevel() || mApp->IsWhackAZombieLevel())
	{
		mSunMoney = 0;
	}
	else if (aGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		mSunMoney = 5000;
	}
	else if (mApp->IsIZombieLevel())
	{
		mSunMoney = 150;
	}
	else if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
	{
		mSunMoney = 150;
	}
	else
	{
		mSunMoney = 50;
	}

	mSunMoney += mApp->OnlineStartSunBonus();
    mApp->mPreparationBuiltSeat=mApp->mOnlineSession?mApp->mOnlineSession->GetLocalSeat():0;
    mApp->mPreparationBuiltSunBonus=mApp->OnlineStartSunBonus();
    mApp->mPreparationBuiltMultiplier=mApp->OnlineSeatMultiplier();
	if (mApp->IsRunMode()) mSunMoney = std::max(50, mSunMoney);
	memset(mRowPickingArray, 0, sizeof(mRowPickingArray));
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		mWaveRowGotLawnMowered[aRow] = -100;
		mIceMinX[aRow] = BOARD_ICE_START;
		mIceTimer[aRow] = 0;
		mIceParticleID[aRow] = ParticleSystemID::PARTICLESYSTEMID_NULL;
		mRowPickingArray[aRow].mItem = aRow;
	}
	mNumSunsFallen = 0;
	if (!StageIsNight() || mApp->RunSkySunAtNight())
	{
		mSunCountDown = (int)(RandRangeInt(425, 700) * mApp->RunSkySunIntervalMul() + 0.5f);
	}
	memset(mHelpDisplayed, 0, sizeof(mHelpDisplayed));
	mSeedBank->mNumPackets = GetNumSeedsInBank();
	mSeedBank->UpdateWidth();
	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		SeedPacket* aPacket = &mSeedBank->mSeedPackets[i];
		aPacket->mIndex = i;
		aPacket->mX = GetSeedPacketPositionX(i);
		aPacket->mY = 8;
		aPacket->mPacketType = SeedType::SEED_NONE;
	}
	if (mApp->IsSlotMachineLevel())
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 3);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_SUNFLOWER);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_PEASHOOTER);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_SNOWPEA);
	}
	else if (aGameMode == GameMode::GAMEMODE_CHALLENGE_ICE)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 6);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_PEASHOOTER);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_CHERRYBOMB);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_WALLNUT);
		mSeedBank->mSeedPackets[3].SetPacketType(SeedType::SEED_REPEATER);
		mSeedBank->mSeedPackets[4].SetPacketType(SeedType::SEED_SNOWPEA);
		mSeedBank->mSeedPackets[5].SetPacketType(SeedType::SEED_CHOMPER);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 3);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_NORMAL);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_FOOTBALL);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_2)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 3);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_NORMAL);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_SCREEN_DOOR);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 3);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_NORMAL);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_DIGGER);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_4)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 3);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_NORMAL);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_LADDER);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_5)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 4);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_NORMAL);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_BUNGEE);
		mSeedBank->mSeedPackets[3].SetPacketType(SeedType::SEED_ZOMBIE_BALLOON);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_6)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 4);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_NORMAL);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_POLEVAULTER);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
		mSeedBank->mSeedPackets[3].SetPacketType(SeedType::SEED_ZOMBIE_GARGANTUAR);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_7)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 4);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_NORMAL);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_POLEVAULTER);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
		mSeedBank->mSeedPackets[3].SetPacketType(SeedType::SEED_ZOMBIE_DANCER);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_8)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 6);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_IMP);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_TRAFFIC_CONE);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
		mSeedBank->mSeedPackets[3].SetPacketType(SeedType::SEED_ZOMBIE_BUNGEE);
		mSeedBank->mSeedPackets[4].SetPacketType(SeedType::SEED_ZOMBIE_DIGGER);
		mSeedBank->mSeedPackets[5].SetPacketType(SeedType::SEED_ZOMBIE_LADDER);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_9)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 8);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_IMP);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_TRAFFIC_CONE);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_POLEVAULTER);
		mSeedBank->mSeedPackets[3].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
		mSeedBank->mSeedPackets[4].SetPacketType(SeedType::SEED_ZOMBIE_BUNGEE);
		mSeedBank->mSeedPackets[5].SetPacketType(SeedType::SEED_ZOMBIE_DIGGER);
		mSeedBank->mSeedPackets[6].SetPacketType(SeedType::SEED_ZOMBIE_LADDER);
		mSeedBank->mSeedPackets[7].SetPacketType(SeedType::SEED_ZOMBIE_FOOTBALL);
	}
	else if (aGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS)
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 9);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIE_IMP);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIE_TRAFFIC_CONE);
		mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_ZOMBIE_POLEVAULTER);
		mSeedBank->mSeedPackets[3].SetPacketType(SeedType::SEED_ZOMBIE_PAIL);
		mSeedBank->mSeedPackets[4].SetPacketType(SeedType::SEED_ZOMBIE_BUNGEE);
		mSeedBank->mSeedPackets[5].SetPacketType(SeedType::SEED_ZOMBIE_DIGGER);
		mSeedBank->mSeedPackets[6].SetPacketType(SeedType::SEED_ZOMBIE_LADDER);
		mSeedBank->mSeedPackets[7].SetPacketType(SeedType::SEED_ZOMBIE_FOOTBALL);
		mSeedBank->mSeedPackets[8].SetPacketType(SeedType::SEED_ZOMBIE_DANCER);
	}
	else if (mApp->IsScaryPotterLevel())
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 1);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_CHERRYBOMB);
	}
	else if (mApp->IsWhackAZombieLevel())
	{
		PVZP_ASSERT(mSeedBank->mNumPackets == 3);
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_POTATOMINE);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_GRAVEBUSTER);
		mSeedBank->mSeedPackets[2].SetPacketType(mApp->IsAdventureMode() ? SeedType::SEED_CHERRYBOMB : SeedType::SEED_ICESHROOM);
	}
	else if (aGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM) {
		mSeedBank->mSeedPackets[0].SetPacketType(SeedType::SEED_ZOMBIQUARIUM_SNORKLE);
		mSeedBank->mSeedPackets[1].SetPacketType(SeedType::SEED_ZOMBIQUARIUM_TROPHY);
	}
	else if (mApp->mRunState && !ChooseSeedsOnCurrentLevel())
	{
		for (int i = 0; i < mSeedBank->mNumPackets; ++i)
			mSeedBank->mSeedPackets[i].SetPacketType(mApp->mRunState->GetUsablePool()[i]);
	}
	else if (!ChooseSeedsOnCurrentLevel() && !HasConveyorBeltSeedBank())
	{
		mSeedBank->mNumPackets = GetNumSeedsInBank();
		// fill the seed bank with all seed types in order
		for (int i = 0; i < mSeedBank->mNumPackets; i++)
		{
			mSeedBank->mSeedPackets[i].SetPacketType((SeedType)i);
		}
	}
	MarkAllDirty();

	mPaused = false;
	mOutOfMoneyCounter = 0;
	if (StageHasFog())
	{
		mFogBlownCountDown = 200;
		mFogOffset = 1065 - LeftFogColumn() * 80;
	}
	mDeferredPlantSetup.flags |= DeferredPlantSetup::INIT;
	mChallenge->InitLevel();
	CompleteDeferredPlantSetup();
}

Reanimation* Board::CreateRakeReanim(float theRakeX, float theRakeY, int theRenderOrder)
{
	Reanimation* aReanim = mApp->AddReanimation(theRakeX + 20, theRakeY, theRenderOrder, REANIM_RAKE);
	if (!aReanim) return nullptr;
	aReanim->mAnimRate = 0;
	aReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
	aReanim->mIsAttachment = true;
	return aReanim;
}

void Board::PlaceRake()
{
	if (mApp->IsOnlineGame()) return;
	if (mApp->mRunState) return;
	if (!mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_RAKE])
		return;

	int aGridX = 7;
	if (mApp->IsScaryPotterLevel())
	{
		for (GridItem* aGridItem : mGridItems)
		{
			if (aGridItem->mDead)
				continue;
			if (aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT && aGridItem->mGridX <= aGridX && aGridItem->mGridX > 0)
			{
				aGridX = aGridItem->mGridX - 1;
			}
		}
	}
	else
	{
		if (!StageHasZombieWalkInFromRight() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
			mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BOBSLED_BONANZA)
			return;
	}

	int aPickCount = 0;
	PvzpWeightedArray aPickArray[MAX_GRID_SIZE_Y];
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (aRow != 5 && mPlantRow[aRow] == PlantRowType::PLANTROW_NORMAL)
		{
			aPickArray[aPickCount].mWeight = 1;
			aPickArray[aPickCount].mItem = aRow;
			aPickCount++;
		}
	}
	if (aPickCount == 0)
		return;

	int aGridY = PvzpPickFromWeightedArray(aPickArray, aPickCount);
	mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_RAKE]--;
	GridItem* aRake = mGridItems.DataArrayAlloc();
	aRake->mGridItemType = GridItemType::GRIDITEM_RAKE;
	aRake->mGridX = aGridX;
	aRake->mGridY = aGridY;
	aRake->mPosX = GridToPixelX(aGridX, aGridY);
	aRake->mPosY = GridToPixelY(aGridX, aGridY);
	aRake->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, aGridY, 9);
	aRake->mGridItemReanimID = mApp->ReanimationGetID(CreateRakeReanim(aRake->mPosX, aRake->mPosY, 0)); // Lmao gotta pass in the right coords
	aRake->mGridItemState = GridItemState::GRIDITEM_STATE_RAKE_ATTRACTING;
}

void Board::InitLawnMowers()
{
	GameMode aGameMode = mApp->mGameMode;
	// 这里优化一下原版的代码，事先列举一些不创建小推车的关卡
	// @pvz-online: 末位推车（2026-10-03 用户定案）：漏怪链的末位玩家、多人局，每行发一台车——
	// 链到他为止，再漏就是全队败，房前给他留一重兜底。整局一次性：用过的行记在 RunState
	// （检查点 v5），跨关不再补。非末位照旧没有车：他们的漏怪要传给队友，房前不留兜底；
	// 单机闯关也没有（房前不留兜底，早先已拍板）。
	bool aOnlineLastSeat = mApp->IsOnlineGame() && mApp->mOnlineSession->IsLastRelaySeat();
	if ((!aOnlineLastSeat && (mApp->IsOnlineGame() || mApp->IsRunMode())) ||
		aGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || aGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		aGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || aGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		aGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND || aGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM ||
		mApp->IsSquirrelLevel() || mApp->IsIZombieLevel() ||
		// 末位联机不认屋顶清洁车的购买位：全队的兜底不挂在一件商店购买上（车本身照旧
		// 按场景定型——屋顶关是屋顶清洁车型，见 LawnMowerInitialize）
		(!aOnlineLastSeat && StageHasRoof() && !mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_ROOF_CLEANER]))
		return;

	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		// 这一局（闯关）已经用掉的行不再发车——RunState 里跨关、跨检查点记着
		if (aOnlineLastSeat && mApp->IsRunMode() && (mApp->GetRunState()->mMowerUsedRows & (1u << aRow)) != 0)
			continue;

		if ((aGameMode == GameMode::GAMEMODE_CHALLENGE_RESODDED && aRow <= 4) ||
			(mApp->IsAdventureMode() && mLevel == 35) ||   // 这里原版没有对于行的判断，故冒险模式 4-5 关卡有 6 行小推车
			(!mApp->IsScaryPotterLevel() && mPlantRow[aRow] != PlantRowType::PLANTROW_DIRT))  // 除冒险模式 4-5 关卡外的破罐者模式关卡无小推车
		{
			LawnMower* aLawnMower = mLawnMowers.DataArrayAlloc();
			aLawnMower->LawnMowerInitialize(aRow);
			aLawnMower->mVisible = false;
		}
	}
}

bool Board::ChooseSeedsOnCurrentLevel()
{
	if (mApp->mRunState) return mApp->mRunState->GetUsablePool().size() > RunState::RUN_SEED_SLOTS;
	if (mApp->IsChallengeWithoutSeedBank() || HasConveyorBeltSeedBank())
		return false;

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM)
		return false;

	if (mApp->IsIZombieLevel() || mApp->IsSlotMachineLevel())
		return false;

	return (!mApp->IsFirstTimeAdventureMode() || mLevel > 7);
}

void Board::StartLevel()
{
    auto& setup = mDeferredPlantSetup;
    setup.flags |= DeferredPlantSetup::START;
    if ((setup.flags & DeferredPlantSetup::INIT) || !mChallenge->StartLevel()) return;
    setup.flags &= ~DeferredPlantSetup::START;
	if (mApp->mRunState)
		PvzpLogLn("[run] board: index={} engine={} waves={} difficulty={} seed={} plants={} mowers={}",
			mApp->mRunState->mLevelIndex, mLevel, mNumWaves, mApp->mRunState->GetDifficultyPermille(),
			GetLevelRandSeed(), mApp->mRunState->GetUsablePool().size(), CountUntriggerLawnMowers());
	mCoinBankFadeCount = 0;
	mApp->mLastLevelStats->Reset();


	// the unsigned underflow below is intentional
	unsigned int aSurvivalStage = mApp->mGameMode - GAMEMODE_SURVIVAL_ENDLESS_STAGE_1;
	if (aSurvivalStage <= 4) {
		if (GetSurvivalFlagsCompleted() >= 20) {
			ReportAchievement::GiveAchievement(mApp, Immortal, true);
		}
	}

	if (mApp->IsSurvivalMode() && mChallenge->mSurvivalStage > 0)
	{
		FreezeEffectsForCutscene(false);
		mApp->mSoundSystem->GamePause(false);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->mGameMode == GameMode::GAMEMODE_UPSELL ||
		mApp->mGameMode == GameMode::GAMEMODE_INTRO ||
		mApp->IsFinalBossLevel())
		return;

	mApp->mMusic->StartGameMusic();
}

LawnMower* Board::GetBottomLawnMower()
{
	LawnMower* aBottomMower = nullptr;
	for (LawnMower* aLawnMower : mLawnMowers)
	{
		if (aLawnMower->mDead)
			continue;
		if (aLawnMower->mMowerState == LawnMowerState::MOWER_TRIGGERED || aLawnMower->mMowerState == LawnMowerState::MOWER_SQUISHED)
			continue;

		if (aBottomMower == nullptr || aBottomMower->mRow < aLawnMower->mRow)
		{
			aBottomMower = aLawnMower;
		}
	}
	return aBottomMower;
}

void Board::UpdateLevelEndSequence()
{
	if (HasLivingBoss()) return;
	if (mNextSurvivalStageCounter > 0)
	{
		if (!IsScaryPotterDaveTalking())
		{
			mNextSurvivalStageCounter--;
			if (mApp->IsAdventureMode() && mApp->IsScaryPotterLevel() && mNextSurvivalStageCounter == 300)
			{
				mApp->CrazyDaveEnter();
				mApp->CrazyDaveTalkIndex(mChallenge->mSurvivalStage == 0 ? 2700 : 2800);
				mChallenge->PuzzleNextStageClear();
				mNextSurvivalStageCounter = 100;
			}
		}

		if (mNextSurvivalStageCounter == 1 && mApp->IsSurvivalMode())
		{
			TryToSaveGame();
		}

		if (!mNextSurvivalStageCounter)
		{
			if (mApp->IsScaryPotterLevel())
			{
				if (mApp->IsAdventureMode())
					return;

				if (!IsFinalScaryPotterStage())
				{
					mChallenge->PuzzleNextStageClear();
					mChallenge->ScaryPotterPopulate();
				}
			}
			else if (LawnApp::IsEndlessIZombie(mApp->mGameMode))
			{
				mChallenge->PuzzleNextStageClear();
                mDeferredPlantSetup.flags |= DeferredPlantSetup::NEXT_I_ZOMBIE;
                CompleteDeferredPlantSetup();
			}
			else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
			{
				ClearAdvice(AdviceType::ADVICE_NONE);
			}
			else
			{
				mLevelComplete = true;
				RemoveZombiesForRepick();
			}
			return;
		}
	}

	if (mBoardFadeOutCounter < 0)
		return;

	mBoardFadeOutCounter--;
	if (mBoardFadeOutCounter == 0)
	{
		mLevelComplete = true;
		return;
	}
	if (mBoardFadeOutCounter == 300)
	{
		if (!IsSurvivalStageWithRepick() && !(mLevel == 9 || mLevel == 19 || mLevel == 29 || mLevel == 39 || mLevel == 49))
		{
			mApp->PlaySample(Sexy::SOUND_LIGHTFILL);
		}
	}

	if (mScoreNextMowerCounter > 0)
	{
		mScoreNextMowerCounter--;
		if (mScoreNextMowerCounter)
		{
			return;
		}
	}

	if (CanDropLoot() && !IsSurvivalStageWithRepick())
	{
		mScoreNextMowerCounter = 40;
		LawnMower* aLawnMower = GetBottomLawnMower();
		if (aLawnMower)
		{
			AddCoin(aLawnMower->mPosX + 40, aLawnMower->mPosY + 40, CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_LAWNMOWER_COIN);
			SoundInstance* aSoundInstance = mApp->mSoundManager->GetSoundInstance(Sexy::SOUND_POINTS);
			if (aSoundInstance)
			{
				aSoundInstance->Play(false, true);
				float aPitch = std::clamp(6 - CountUntriggerLawnMowers(), 0, 6);
				aSoundInstance->AdjustPitch(aPitch);
			}
			aLawnMower->Die();
		}
	}
}

void Board::CompleteEndLevelSequenceForSaving()
{
	if (CanDropLoot())
	{
		for (LawnMower* aLawnMower : mLawnMowers)
		{
			if (aLawnMower->mDead)
				continue;
			if (aLawnMower->mMowerState != LawnMowerState::MOWER_TRIGGERED && aLawnMower->mMowerState != LawnMowerState::MOWER_SQUISHED)
			{
				int aCoinValue = Coin::GetCoinValue(CoinType::COIN_GOLD);
				mApp->mPlayerInfo->AddCoins(aCoinValue);
				mCoinsCollected += aCoinValue;
			}
		}
	}

	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead)
			continue;
		if (aCoin->mIsBeingCollected)
		{
			aCoin->ScoreCoin();
		}
		else
		{
			aCoin->Die();
		}
	}

	mApp->UpdatePlayerProfileForFinishingLevel();
}

void Board::FadeOutLevel()
{
	if (HasLivingBoss()) return;
	if (mApp->IsOnlineGame()) return;
	if (mApp->mRunState)
	{
		ClearCursor();
		mLevelComplete = true;
		return;
	}
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING)
	{
		RefreshSeedPacketFromCursor();
		mApp->mLastLevelStats->Reset();
		mLevelComplete = true;
	}

	bool aNeedSoundEffect = true;
	if (mApp->IsScaryPotterLevel() && !IsFinalScaryPotterStage())
	{
		aNeedSoundEffect = false;
	}
	else if (IsSurvivalStageWithRepick() || IsLastStandStageWithRepick() || mApp->IsEndlessIZombie(mApp->mGameMode))
	{
		aNeedSoundEffect = false;
	}
	if (aNeedSoundEffect)
	{
		mApp->mMusic->StopAllMusic();
		if (mApp->IsAdventureMode() && mLevel == 50)
		{
			mApp->PlayFoley(FoleyType::FOLEY_FINAL_FANFARE);
		}
		else if (mApp->TrophiesNeedForGoldSunflower() == 1)
		{
			mApp->PlayFoley(FoleyType::FOLEY_FINAL_FANFARE);
		}
		else
		{
			mApp->PlayFoley(FoleyType::FOLEY_WINMUSIC);
		}
	}

	if (mApp->IsScaryPotterLevel() && !IsFinalScaryPotterStage())
	{
		mNextSurvivalStageCounter = 500;
		if (mApp->IsAdventureMode())
		{
			ClearAdvice(AdviceType::ADVICE_NONE);
		}
		else
		{
			mLevelAwardSpawned = true;
			std::string aStreakStr = mApp->IsEndlessScaryPotter(mApp->mGameMode) ? "[ADVICE_MORE_SCARY_POTS]" : "[ADVICE_3_IN_A_ROW]";
			std::string aMessage = PvzpReplaceNumberString(aStreakStr, "{STREAK}", mChallenge->mSurvivalStage + 1);
			PuzzleSaveStreak();
			ClearAdvice(AdviceType::ADVICE_NONE);
			DisplayAdvice(aMessage, MessageStyle::MESSAGE_STYLE_BIG_MIDDLE, AdviceType::ADVICE_NONE);
		}
		return;
	}

	if (mApp->IsEndlessIZombie(mApp->mGameMode))
	{
		mNextSurvivalStageCounter = 500;
		std::string aMessage = PvzpReplaceNumberString("[ADVICE_MORE_IZOMBIE]", "{STREAK}", mChallenge->mSurvivalStage + 1);
		PuzzleSaveStreak();
		ClearAdvice(AdviceType::ADVICE_NONE);
		DisplayAdvice(aMessage, MessageStyle::MESSAGE_STYLE_BIG_MIDDLE, AdviceType::ADVICE_NONE);
		return;
	}

	if (IsLastStandStageWithRepick())
	{
		mNextSurvivalStageCounter = 500;
		mChallenge->LastStandCompletedStage();
		return;
	}

	if (!IsSurvivalStageWithRepick())
	{
		RefreshSeedPacketFromCursor();
		mApp->mLastLevelStats->mUnusedLawnMowers = CountUntriggerLawnMowers();

		mBoardFadeOutCounter = 600;
		if (mLevel == 9 || mLevel == 19 || mLevel == 29 || mLevel == 39 || mLevel == 49)
		{
			mBoardFadeOutCounter = 500;
		}

		if (CanDropLoot())
		{
			mScoreNextMowerCounter = 200;
		}

		for (Coin* aCoin : mCoins)
		{
			if (aCoin->mDead)
				continue;
			aCoin->TryAutoCollectAfterLevelAward();
		}
	}
	else
	{
		PVZP_ASSERT(mApp->IsSurvivalMode());
		mNextSurvivalStageCounter = 500;
		DisplayAdvice("[ADVICE_MORE_ZOMBIES]", MessageStyle::MESSAGE_STYLE_BIG_MIDDLE, AdviceType::ADVICE_NONE);
		mApp->mMusic->FadeOut(500);
		mApp->PlaySample(Sexy::SOUND_HUGE_WAVE);
		for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
		{
			mIceTimer[aRow] = mNextSurvivalStageCounter;
		}
	}

	mApp->SetCursor(CURSOR_POINTER);
}

void Board::DisplayAdvice(std::string_view theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex)
{
	if (theHelpIndex != AdviceType::ADVICE_NONE)
	{
		if (mHelpDisplayed[theHelpIndex])
			return;

		mHelpDisplayed[theHelpIndex] = true;
	}

	mAdvice->SetLabel(theAdvice, theMessageStyle);
	mHelpIndex = theHelpIndex;
}

void Board::DisplayAdviceAgain(std::string_view theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex)
{
	if (theHelpIndex != AdviceType::ADVICE_NONE)
	{
		mHelpDisplayed[theHelpIndex] = false;
	}
	DisplayAdvice(theAdvice, theMessageStyle, theHelpIndex);
}

void Board::ClearAdviceImmediately()
{
	ClearAdvice(AdviceType::ADVICE_NONE);
	mAdvice->mDuration = 0;
}

void Board::ClearAdvice(AdviceType theHelpIndex)
{
	if (theHelpIndex == AdviceType::ADVICE_NONE || theHelpIndex == mHelpIndex)
	{
		mAdvice->ClearLabel();
		mHelpIndex = AdviceType::ADVICE_NONE;
	}
}

Coin* Board::AddCoin(int theX, int theY, CoinType theCoinType, CoinMotion theCoinMotion)
{
	Coin* aCoin = mCoins.DataArrayAlloc();
	aCoin->CoinInitialize(theX, theY, theCoinType, theCoinMotion);
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
	{
		DisplayAdvice("[ADVICE_CLICK_ON_SUN]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_CLICK_ON_SUN);
	}
	return aCoin;
}

bool Board::IsPlantInCursor()
{
	return
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_DUPLICATOR ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW;
}

SeedType Board::GetSeedTypeInCursor()
{
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WHEEELBARROW)
	{
		PottedPlant* aPottedPlant = mApp->mZenGarden->GetPottedPlantInWheelbarrow();
		if (aPottedPlant)
		{
			return aPottedPlant->mSeedType;
		}
	}

	if (!IsPlantInCursor())
	{
		return SeedType::SEED_NONE;
	}
	return mCursorObject->mType == SeedType::SEED_IMITATER ? mCursorObject->mImitaterType : mCursorObject->mType;
}

void Board::RefreshSeedPacketFromCursor()
{
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN)
	{
		mCoins.DataArrayTryToGet(mCursorObject->mCoinID)->DroppedUsableSeed();
	}
	else if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK)
	{
		PVZP_ASSERT(mCursorObject->mSeedBankIndex >= 0 && mCursorObject->mSeedBankIndex < mSeedBank->mNumPackets);
		mSeedBank->mSeedPackets[mCursorObject->mSeedBankIndex].Activate();
	}
	ClearCursor();
}

bool Board::IsPoolSquare(int theGridX, int theGridY)
{
	if (theGridX >= 0 && theGridY >= 0)
	{
		PVZP_ASSERT(theGridX < MAX_GRID_SIZE_X && theGridY < MAX_GRID_SIZE_Y);
		return mGridSquareType[theGridX][theGridY] == GridSquareType::GRIDSQUARE_POOL;
	}
	return false;
}

// Called before spending sun, killing an upgraded plant or consuming the selected packet.
bool Board::PreparePlantAnimations(SeedType seed, unsigned int count)
{
    if (!count || mPlants.mMaxSize == 0 || !mPlants.DataArrayCanAllocate(count) || !mApp || !mApp->mEffectSystem ||
        !mApp->mEffectSystem->mReanimationHolder) return false;
    unsigned int need = GetPlantDefinition(seed).mReanimationType == ReanimationType::REANIM_NONE ? 0U : 1U;
    switch (seed)
    {
    case SeedType::SEED_PEASHOOTER: case SeedType::SEED_SNOWPEA:
    case SeedType::SEED_REPEATER: case SeedType::SEED_LEFTPEATER: case SeedType::SEED_GATLINGPEA: need += 1; break;
    case SeedType::SEED_SPLITPEA: need += 2; break;
    case SeedType::SEED_THREEPEATER: need += 3; break;
    default: break;
    }
    // Sleep indicators are optional, but account for them during synchronous initialization.
    if (Plant::IsNocturnal(seed) && !StageIsNight() && !mApp->IsRunMode()) ++need;
    return mApp->mEffectSystem->mReanimationHolder->MakeRoomForPlant(need * count);
}

Plant* Board::NewPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType)
{
	if (!PreparePlantAnimations(theSeedType)) return nullptr;
	Plant* aPlant = mPlants.DataArrayAlloc();
	aPlant->mIsOnBoard = true;
	aPlant->PlantInitialize(theGridX, theGridY, theSeedType, theImitaterType);
	return aPlant;
}

void Board::DoPlantingEffects(int theGridX, int theGridY, Plant* thePlant)
{
	int aXPos = GridToPixelX(theGridX, theGridY) + 41;
	int aYPos = GridToPixelY(theGridX, theGridY) + 74;
	if (thePlant)
	{
		if (thePlant->mSeedType == SeedType::SEED_LILYPAD)
		{
			aYPos += 15;
		}
		else if (thePlant->mSeedType == SeedType::SEED_FLOWERPOT)
		{
			aYPos += 30;
		}
	}

	if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE)
	{
		mApp->PlayFoley(FoleyType::FOLEY_CERAMIC);
		return;
	}
	if (mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);
		return;
	}
	if (Plant::IsFlying(thePlant->mSeedType))
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT);
		return;
	}

	if (IsPoolSquare(theGridX, theGridY))
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);
		mApp->AddPvzpParticle(aXPos, aYPos, RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_PLANTING_POOL);
	}
	else
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT);
		mApp->AddPvzpParticle(aXPos, aYPos, RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_PLANTING);
	}
}

Plant* Board::AddPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType)
{
	Plant* aPlant = NewPlant(theGridX, theGridY, theSeedType, theImitaterType);
	if (!aPlant) return nullptr;
	DoPlantingEffects(theGridX, theGridY, aPlant);
	mChallenge->PlantAdded(aPlant);
    RunGarlicPulse::OnPlanted(this, aPlant);

	int aSunPlantsCount = CountPlantByType(SeedType::SEED_SUNSHROOM) + CountPlantByType(SeedType::SEED_SUNFLOWER);
	if (aSunPlantsCount > mMaxSunPlants)
	{
		mMaxSunPlants = aSunPlantsCount;  //mMaxSunPlants = max(aSunPlantsCount, mMaxSunPlants);
	}

	if (theSeedType == SeedType::SEED_PEASHOOTER ||
		theSeedType == SeedType::SEED_SNOWPEA ||
		theSeedType == SeedType::SEED_REPEATER ||
		theSeedType == SeedType::SEED_THREEPEATER ||
		theSeedType == SeedType::SEED_SPLITPEA ||
		theSeedType == SeedType::SEED_GATLINGPEA)
	{
		mPeaShooterUsed = true;
	}
	if (theSeedType == SeedType::SEED_CABBAGEPULT ||
		theSeedType == SeedType::SEED_KERNELPULT ||
		theSeedType == SeedType::SEED_MELONPULT ||
		theSeedType == SeedType::SEED_WINTERMELON)
	{
		mCatapultPlantsUsed = true;
	}

	bool aIsFungi = Plant::IsFungus(theSeedType);
	if (!Plant::IsFlying(theSeedType) && !aIsFungi) {
		mMushroomAndCoffeeBeansOnly = false;
	}
	if (aIsFungi) {
		mMushroomsUsed = true;
	}

	return aPlant;
}

Plant* Board::GetPumpkinAt(int theGridX, int theGridY)
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mPlantCol == theGridX && aPlant->mRow == theGridY && !aPlant->NotOnGround() && aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			return aPlant;
		}
	}
	return nullptr;
}

Plant* Board::GetFlowerPotAt(int theGridX, int theGridY)
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mPlantCol == theGridX && aPlant->mRow == theGridY && !aPlant->NotOnGround() && aPlant->mSeedType == SeedType::SEED_FLOWERPOT)
		{
			return aPlant;
		}
	}
	return nullptr;
}

void Board::GetPlantsOnLawn(int theGridX, int theGridY, PlantsOnLawn* thePlantOnLawn)
{
	thePlantOnLawn->mLilypadPlant = nullptr;
	thePlantOnLawn->mFlowerPotPlant = nullptr;
	thePlantOnLawn->mUnderPlant = nullptr;
	thePlantOnLawn->mPumpkinPlant = nullptr;
	thePlantOnLawn->mFlyingPlant = nullptr;
	thePlantOnLawn->mNormalPlant = nullptr;

	if (theGridX < 0 || theGridX >= MAX_GRID_SIZE_X || theGridY < 0 || theGridY >= MAX_GRID_SIZE_Y)
		return;

	if (mApp->IsWallnutBowlingLevel() && !mCutScene->IsInShovelTutorial())
		return;

	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow != theGridY)
		{
			continue;
		}
		SeedType aSeedType = aPlant->mSeedType;
		if (aSeedType == SeedType::SEED_IMITATER && aPlant->mImitaterType != SeedType::SEED_NONE)
		{
			aSeedType = aPlant->mImitaterType;
		}

		if (aSeedType == SeedType::SEED_COBCANNON)
		{
			if (aPlant->mPlantCol < theGridX - 1 || aPlant->mPlantCol > theGridX)
			{
				continue;
			}
		}
		else
		{
			if (aPlant->mPlantCol != theGridX)
			{
				continue;
			}
		}
		if (aPlant->NotOnGround())
		{
			continue;
		}

		if (Plant::IsFlying(aSeedType))
		{
			PVZP_ASSERT(!thePlantOnLawn->mFlyingPlant);
			thePlantOnLawn->mFlyingPlant = aPlant;
		}
		else if (aSeedType == SeedType::SEED_FLOWERPOT || (aSeedType == SeedType::SEED_LILYPAD && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN))
		{
			// Two distinct support plants share a tile; rendering order also identifies the exposed one.
			Plant*& aSupport = aSeedType == SeedType::SEED_LILYPAD ?
				thePlantOnLawn->mLilypadPlant : thePlantOnLawn->mFlowerPotPlant;
			PVZP_ASSERT(!aSupport);
			aSupport = aPlant;
			if (!thePlantOnLawn->mUnderPlant || aPlant->mRenderOrder > thePlantOnLawn->mUnderPlant->mRenderOrder)
				thePlantOnLawn->mUnderPlant = aPlant;
		}
		else if (aSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			PVZP_ASSERT(!thePlantOnLawn->mPumpkinPlant);
			thePlantOnLawn->mPumpkinPlant = aPlant;
		}
		else
		{
			PVZP_ASSERT(!thePlantOnLawn->mNormalPlant);
			thePlantOnLawn->mNormalPlant = aPlant;
		}
	}
}

Plant* Board::GetTopPlantAt(int theGridX, int theGridY, PlantPriority thePriority)
{
	if (theGridX < 0 || theGridX >= MAX_GRID_SIZE_X || theGridY < 0 || theGridY >= MAX_GRID_SIZE_Y)
		return nullptr;

	if (mApp->IsWallnutBowlingLevel() && !mCutScene->IsInShovelTutorial())
		return nullptr;

	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(theGridX, theGridY, &aPlantOnLawn);

	switch (thePriority)
	{
	case PlantPriority::TOPPLANT_EATING_ORDER:
		if (aPlantOnLawn.mPumpkinPlant)							return aPlantOnLawn.mPumpkinPlant;
		else if (aPlantOnLawn.mNormalPlant)						return aPlantOnLawn.mNormalPlant;
		else													return aPlantOnLawn.mUnderPlant;
	case PlantPriority::TOPPLANT_DIGGING_ORDER:
		if (aPlantOnLawn.mNormalPlant)							return aPlantOnLawn.mNormalPlant;
		else													return aPlantOnLawn.mUnderPlant;
	case PlantPriority::TOPPLANT_BUNGEE_ORDER:
	case PlantPriority::TOPPLANT_CATAPULT_ORDER:
	case PlantPriority::TOPPLANT_ANY:
		if (aPlantOnLawn.mFlyingPlant)							return aPlantOnLawn.mFlyingPlant;
		else if (aPlantOnLawn.mNormalPlant)						return aPlantOnLawn.mNormalPlant;
		else if (aPlantOnLawn.mPumpkinPlant)					return aPlantOnLawn.mPumpkinPlant;
		else													return aPlantOnLawn.mUnderPlant;
	case PlantPriority::TOPPLANT_ZEN_TOOL_ORDER:
		if (aPlantOnLawn.mFlyingPlant)							return aPlantOnLawn.mFlyingPlant;
		else if (aPlantOnLawn.mPumpkinPlant)					return aPlantOnLawn.mPumpkinPlant;
		else if (aPlantOnLawn.mNormalPlant)						return aPlantOnLawn.mNormalPlant;
		else													return aPlantOnLawn.mUnderPlant;
	case PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION:			return aPlantOnLawn.mNormalPlant;
	case PlantPriority::TOPPLANT_ONLY_FLYING:					return aPlantOnLawn.mFlyingPlant;
	case PlantPriority::TOPPLANT_ONLY_PUMPKIN:					return aPlantOnLawn.mPumpkinPlant;
	case PlantPriority::TOPPLANT_ONLY_UNDER_PLANT:				return aPlantOnLawn.mUnderPlant;
	default:													PVZP_ASSERT(false);
	}
	unreachable();
}

int Board::CountSunFlowers()
{
	int aCount = 0;
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->MakesSun())
		{
			aCount++;
		}
	}
	return aCount;
}

int Board::CountPlantByType(SeedType theSeedType)
{
	int aCount = 0;
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == theSeedType)
		{
			aCount++;
		}
	}
	return aCount;
}

int Board::CountEmptyPotsOrLilies(SeedType theSeedType)
{
	int aCount = 0;
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == theSeedType && !GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION))
		{
			aCount++;
		}
	}
	return aCount;
}

bool Board::IsValidCobCannonSpotHelper(int theGridX, int theGridY)
{
	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(theGridX, theGridY, &aPlantOnLawn);
	if (aPlantOnLawn.mPumpkinPlant)
		return false;

	if (aPlantOnLawn.mNormalPlant && aPlantOnLawn.mNormalPlant->mSeedType == SeedType::SEED_KERNELPULT)
		return true;

	return mApp->mEasyPlantingCheat && CanPlantAt(theGridX, theGridY, SeedType::SEED_KERNELPULT) == PlantingReason::PLANTING_OK;
}

bool Board::IsValidCobCannonSpot(int theGridX, int theGridY)
{
	if (!IsValidCobCannonSpotHelper(theGridX, theGridY) || !IsValidCobCannonSpotHelper(theGridX + 1, theGridY))
		return false;

	return !GetFlowerPotAt(theGridX, theGridY) == !GetFlowerPotAt(theGridX + 1, theGridY);
}

bool Board::HasValidCobCannonSpot()
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == SeedType::SEED_KERNELPULT && IsValidCobCannonSpot(aPlant->mPlantCol, aPlant->mRow))
		{
			return true;
		}
	}
	return false;
}

Projectile* Board::AddProjectile(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType)
{
	Projectile* aProjectile = mProjectiles.DataArrayTryAlloc();
	if (aProjectile == nullptr)
	{
		// Log at most once per 500 game ticks; never write beyond the pool.
		static int aLastRejectedTick = -500;
		if (mMainCounter < aLastRejectedTick || mMainCounter - aLastRejectedTick >= 500)
		{
			PvzpLogLn("[capacity] projectile allocation refused: {}/{}", mProjectiles.mSize, mProjectiles.mMaxSize);
			aLastRejectedTick = mMainCounter;
		}
		return nullptr;
	}
	aProjectile->ProjectileInitialize(theX, theY, theRenderOrder, theRow, theProjectileType);
	return aProjectile;
}

bool Board::CanZombieSpawnOnLevel(ZombieType theZombieType, int theLevel)
{
	// @pvz-online: 闯关的准入只看名单（RunZombieRoster，按关序号逐格加码，含屋顶剔除）：
	// 不看原版 startingLevel / allowedLevels 表，雪人也不放——名单之外一律免谈。
	// 行/场硬约束（水路只收会水的、雪橇要有冰道、0 行禁巨人）在名单之外照旧自动生效。
	if (gLawnApp->IsRunMode())
	{
		// @pvz-online: 植物僵尸开关（2026-10-09，MOD_BUILD 35 高级选项）：随 RunState 走，
		// 房主设、全员对齐（AlignRunToHost 无条件覆盖）——各客户端建场时在这里读到同值。
		RunState* aRunState = gLawnApp->GetRunState();
		bool aZombotany = aRunState != nullptr && aRunState->mZombotany != 0;
		// @pvz-online: 无尽名单快速解锁（2026-10-10 用户定案"快点解锁"）。无尽锁死一个场景、
		// 原型按 关序号%5 轮转，若按引擎关号查名单，永远只有本场景 5 格。改查"虚拟关号"：
		// 先吃满本场景 5 格，此后每 2 关多解锁一格，到雾场景末格封顶（索引 19，2026-10-12 起；
		// 后续场景的巨人/水怪/投石车等按序渗入）。推导只用 RunState 同步字段
		//（mEndlessScene/mLevelIndex），六席各端逐位一致、无随机。波表与种子仍按原型关
		//（GetLevel 不动），名单只管"允许谁"。
		if (aRunState != nullptr && aRunState->IsEndless())
		{
			int aVirtualIndex = aRunState->mEndlessScene * RunState::RUN_LEVELS_PER_SCENE
				+ (RunState::RUN_LEVELS_PER_SCENE - 1) + aRunState->mLevelIndex / 2;
			// @pvz-online: 封顶 = 屋顶前最后一格（雾 sub5，索引 19；2026-10-12 用户拍板）。
			// 格 20-24 是屋顶场景的剔除名单（无潜水/海豚/冰车/雪橇），无尽单行道穿过它会让
			// 非屋顶卡"越打越丢怪"（雾卡第 3 关起丢 4 种、泳池第 13 关起、夜 23/昼 33）。
			// 封在 19：五卡只增不减；屋顶卡实际种类逐位不变——那 4 种本来就由下面的场景闸
			// 拦下（格 19 经闸 = 现屋顶名单 21 种）。
			const int aEndlessRosterCap = RunState::RUN_LEVEL_COUNT - RunState::RUN_LEVELS_PER_SCENE - 1;
			if (aVirtualIndex > aEndlessRosterCap) aVirtualIndex = aEndlessRosterCap;
			// 场景不相容类型拦在闸外（名单 v2 的"有意剔除"口径照旧）：
			// 水生三样只在有水场景（泳池/雾）放行——无水草坪没有行可落；
			// 冰车/雪橇屋顶不放（RUN_ZOMBIE_ROOF 同口径，2026-10-03 名单 v2 定案）——
			// 2026-10-12 起这行也是屋顶卡唯一的剔除机制（虚拟格封顶 19，不再经过屋顶名单）。
			if (theZombieType == ZombieType::ZOMBIE_DUCKY_TUBE || theZombieType == ZombieType::ZOMBIE_SNORKEL
				|| theZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
			{
				if (aRunState->mEndlessScene != 2 && aRunState->mEndlessScene != 3) return false;
			}
			else if (theZombieType == ZombieType::ZOMBIE_ZAMBONI || theZombieType == ZombieType::ZOMBIE_BOBSLED)
			{
				if (aRunState->mEndlessScene == 4) return false;
			}
			return RunZombieAllowedOnLevel(theZombieType, aVirtualIndex, aZombotany);
		}
		return RunZombieAllowedOnLevel(theZombieType, RunLevelIndexForEngineLevel(theLevel), aZombotany);
	}

	const ZombieDefinition& aZombieDef = GetZombieDefinition(theZombieType);
	if (theZombieType == ZombieType::ZOMBIE_YETI)
	{
		return gLawnApp->CanSpawnYetis();
	}

	if (theLevel < aZombieDef.mStartingLevel || aZombieDef.mPickWeight == 0)
	{
		return false;
	}

	PVZP_ASSERT(gZombieAllowedLevels[theZombieType].mZombieType == theZombieType);
	return gZombieAllowedLevels[theZombieType].mAllowedOnLevel[std::clamp(theLevel - 1, 0, 49)];
}

ZombieType Board::GetIntroducedZombieType()
{
	if (!mApp->IsAdventureMode() || mLevel == 1)
	{
		return ZombieType::ZOMBIE_INVALID;
	}

	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = static_cast<ZombieType>(static_cast<int>(aZombieType) + 1))
	{
		const ZombieDefinition& aZombieDef = GetZombieDefinition(aZombieType);
		if ((aZombieType != ZombieType::ZOMBIE_YETI || mApp->CanSpawnYetis()) && aZombieDef.mStartingLevel == mLevel)
		{
			return aZombieType;
		}
	}
	return ZombieType::ZOMBIE_INVALID;
}

ZombieType Board::PickGraveRisingZombieType()
{
	PvzpWeightedArray aZombieWeightArray[ZombieType::NUM_ZOMBIE_TYPES];
	int aCount = 2;
	aZombieWeightArray[0].mItem = ZombieType::ZOMBIE_NORMAL;
	aZombieWeightArray[0].mWeight = GetZombieDefinition(ZombieType::ZOMBIE_NORMAL).mPickWeight;
	aZombieWeightArray[1].mItem = ZombieType::ZOMBIE_TRAFFIC_CONE;
	aZombieWeightArray[1].mWeight = GetZombieDefinition(ZombieType::ZOMBIE_TRAFFIC_CONE).mPickWeight;
	if (!StageHasGraveStones())
	{
		aZombieWeightArray[2].mItem = ZombieType::ZOMBIE_PAIL;
		aZombieWeightArray[2].mWeight = GetZombieDefinition(ZombieType::ZOMBIE_PAIL).mPickWeight;
		aCount++;
	}

	for (int i = 0; i < aCount; i++)
	{
		ZombieType aZombieType = static_cast<ZombieType>(aZombieWeightArray[i].mItem);
		const ZombieDefinition& aZombieDef = GetZombieDefinition(aZombieType);
		if ((mApp->IsFirstTimeAdventureMode() && mLevel < aZombieDef.mStartingLevel) || (!mZombieAllowed[aZombieType] && aZombieType != ZombieType::ZOMBIE_NORMAL))
		{
			aZombieWeightArray[i].mWeight = 0;
		}
	}

	return (ZombieType)PvzpPickFromWeightedArray(aZombieWeightArray, aCount);
}

static int RunZombieValue(ZombieType theZombieType)
{
	if (theZombieType == ZombieType::ZOMBIE_GARGANTUAR || theZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
	{
		return RunState::RUN_GARGANTUAR_VALUE;
	}
	return GetZombieDefinition(theZombieType).mZombieValue;
}

ZombieType Board::PickZombieType(int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker)
{
	int aPickCount = 0;
	PvzpWeightedArray aZombieWeightArray[ZombieType::NUM_ZOMBIE_TYPES];
	for (int aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType++)
	{
		if (!mZombieAllowed[aZombieType])
			continue;

		const ZombieDefinition& aZombieDef = GetZombieDefinition((ZombieType)aZombieType);

		// ================================================================================================
		// ▲ 将不符合出怪限制或超出剩余点数的僵尸类型排除
		// ================================================================================================
		GameMode aGameMode = mApp->mGameMode;
		// 蹦极僵尸在无尽模式中仅在旗帜波出现
		if (aZombieType == ZombieType::ZOMBIE_BUNGEE && mApp->IsSurvivalEndless(aGameMode))
		{
			if (!IsFlagWave(theWaveIndex))
			{
				continue;
			}
		}
		// @pvz-online: 闯关的出类闸（M4-a，用户定案"再激进一档"）：不走"最早出现波数"，
		// 只看点数买不买得起——"这一关有没有它"已在名单（CanZombieSpawnOnLevel）答过。
		// 巨人系价值压到 4（RunZombieValue），另吃每波合计上限：不设闸的话点数富余的波
		// 会一路连抽巨人，一波能到五六只。
		else if (mApp->IsRunMode())
		{
			if (theZombiePoints < RunZombieValue((ZombieType)aZombieType))
			{
				continue;
			}
			if ((aZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR) &&
				theZombiePicker->mZombieTypeCount[(int)ZombieType::ZOMBIE_GARGANTUAR] + theZombiePicker->mZombieTypeCount[(int)ZombieType::ZOMBIE_REDEYE_GARGANTUAR] >= RunState::RUN_GARGANTUAR_CAP)
			{
				continue;
			}
			// @pvz-online: 舞王每波至多 1 只（47，2026-10-14 实机报障「一进来满屏舞王卡死」）：
			// 舞王 value=5 落在强怪池，权重平铺 1000 后与其他强怪同权——快速第 4 关（舞王
			// 首关）点数富余时一波连抽五六只，每只落地又翻 1+4 伴舞 + 追光特效，人数直接
			// 爆表掉帧。伴舞不走抽取通道、不占 cap，与巨人同闸写法、跨波不限。
			if (aZombieType == ZombieType::ZOMBIE_DANCER &&
				theZombiePicker->mZombieTypeCount[(int)ZombieType::ZOMBIE_DANCER] >= 1)
			{
				continue;
			}
		}
		// 僵尸最早出现的波数的限制（出怪限制）
		else if (aGameMode != GameMode::GAMEMODE_CHALLENGE_POGO_PARTY && aGameMode != GameMode::GAMEMODE_CHALLENGE_BOBSLED_BONANZA && aGameMode != GameMode::GAMEMODE_CHALLENGE_AIR_RAID)
		{
			int aFirstAllowedWave = aZombieDef.mFirstAllowedWave;
			// 无尽模式中，僵尸最早可出现的波数逐渐前移
			if (mApp->IsSurvivalEndless(aGameMode))
			{
				int aFlags = GetSurvivalFlagsCompleted();
				int aAllowedWave = aFirstAllowedWave - PvzpAnimateCurve(18, 50, aFlags, 0, 15, PvzpCurves::CURVE_LINEAR);
				aFirstAllowedWave = std::max(aAllowedWave, 1);
			}
			if (theWaveIndex + 1 < aFirstAllowedWave || theZombiePoints < aZombieDef.mZombieValue)
			{
				continue;
			}
		}

		// ================================================================================================
		// ▲ 生存模式中，根据当前旗帜数等重新计算僵尸的权重
		// ================================================================================================
		int aPickWeight = aZombieDef.mPickWeight;
		if (mApp->IsSurvivalMode())
		{
			int aFlags = GetSurvivalFlagsCompleted();
			// 伽刚特尔和雪橇车僵尸的每波出怪上限
			if (aZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombieType == ZombieType::ZOMBIE_ZAMBONI)
			{
				if (theZombiePicker->mZombieTypeCount[aZombieType] >= PvzpAnimateCurve(10, 50, aFlags, 2, 50, PvzpCurves::CURVE_LINEAR))
				{
					continue;
				}
			}
			// 红眼的旗帜波出怪上限和非旗帜波出怪总和上限
			else if (aZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
			{
				if (IsFlagWave(theWaveIndex))
				{
					if (theZombiePicker->mZombieTypeCount[aZombieType] >= PvzpAnimateCurve(14, 100, aFlags, 1, 50, PvzpCurves::CURVE_LINEAR))
					{
						continue;
					}
				}
				else
				{
					if (theZombiePicker->mAllWavesZombieTypeCount[aZombieType] >= PvzpAnimateCurve(10, 110, aFlags, 1, 50, PvzpCurves::CURVE_LINEAR))
					{
						continue;
					}
					aPickWeight = 1000;
				}
			}
			// 普通僵尸和路障僵尸的权重衰减
			else if (aZombieType == ZombieType::ZOMBIE_NORMAL)
			{
				aPickWeight = PvzpAnimateCurve(10, 50, aFlags, aPickWeight, aPickWeight / 10, PvzpCurves::CURVE_LINEAR);
			}
			else if (aZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE)
			{
				aPickWeight = PvzpAnimateCurve(10, 50, aFlags, aPickWeight, aPickWeight / 4, PvzpCurves::CURVE_LINEAR);
			}
		}
		// @pvz-online: 闯关的权重走平铺表（RunZombieRoster.cpp，2026-10-03 用户定案"丰富僵尸种类"）：
		// 原版权重里普僵/路障各 4000，抽出来半个战场就这两样；平铺后红眼与普通巨人同档
		// 也自动成立（旧的特判一并撤掉）。
		else if (mApp->IsRunMode())
		{
			aPickWeight = RunZombieWeight((ZombieType)aZombieType);
            int theme=mApp->GetRunState()->EndlessTheme();
            ZombieType t=(ZombieType)aZombieType;
            bool favored=(theme==1 && (t==ZombieType::ZOMBIE_PAIL || t==ZombieType::ZOMBIE_DOOR || t==ZombieType::ZOMBIE_FOOTBALL)) ||
                (theme==2 && (t==ZombieType::ZOMBIE_POLEVAULTER || t==ZombieType::ZOMBIE_FOOTBALL || t==ZombieType::ZOMBIE_DOLPHIN_RIDER)) ||
                (theme==3 && (t==ZombieType::ZOMBIE_BALLOON || t==ZombieType::ZOMBIE_BUNGEE)) ||
                (theme==4 && (t==ZombieType::ZOMBIE_DUCKY_TUBE || t==ZombieType::ZOMBIE_SNORKEL || t==ZombieType::ZOMBIE_DOLPHIN_RIDER));
            if(favored)aPickWeight*=IsFlagWave(theWaveIndex) && mApp->GetRunState()->IsEndlessBossLevel()?2:3;
            if ((aZombieType==ZombieType::ZOMBIE_BALLOON || aZombieType==ZombieType::ZOMBIE_BUNGEE) &&
                mApp->GetRunState() && mApp->GetRunState()->GetSceneIndex()<=1) aPickWeight/=2;
		}
		// Only multiply legal, budgeted balloon candidates; never add wave entries.
        if (mApp->IsRunMode() && aZombieType==ZombieType::ZOMBIE_BALLOON) {
            auto* run=mApp->GetRunState();
            if(run) aPickWeight=RunSpeciesRules::ScalePositive(aPickWeight,
                RunSpeciesRules::LinearPermille(RunSpeciesRules::ARMORED_BALLOON,
                    run->ZombieBuffCount(RunSpeciesRules::ARMORED_BALLOON)));
        }
        aZombieWeightArray[aPickCount].mItem = aZombieType;
		aZombieWeightArray[aPickCount].mWeight = aPickWeight;
		aPickCount++;
	}

	// @pvz-online: 闯关"点数多就出强僵尸"（M4-a 定案）：单波剩余点数 >= RUN_HEAVY_POINTS 时，
	// 把价值不足 RUN_HEAVY_VALUE 的普通僵尸权重清零，只从强僵尸里抽——多出来的点数变成
	// 更强的个体而不是人海。名单里没有强僵尸（或强僵尸全抽不起）时保持原样，绝不空抽。
	if (mApp->IsRunMode() && theZombiePoints >= RunState::RUN_HEAVY_POINTS)
	{
		int aHeavyWeight = 0;
		for (int i = 0; i < aPickCount; i++)
		{
			if (GetZombieDefinition((ZombieType)aZombieWeightArray[i].mItem).mZombieValue >= RunState::RUN_HEAVY_VALUE)
				aHeavyWeight += aZombieWeightArray[i].mWeight;
		}
		if (aHeavyWeight > 0)
		{
			for (int i = 0; i < aPickCount; i++)
			{
				if (GetZombieDefinition((ZombieType)aZombieWeightArray[i].mItem).mZombieValue < RunState::RUN_HEAVY_VALUE)
					aZombieWeightArray[i].mWeight = 0;
			}
		}
	}

	// 加权随机地取得一种可能的僵尸类型并返回
	return (ZombieType)PvzpPickFromWeightedArray(aZombieWeightArray, aPickCount);
}

bool Board::IsZombieTypePoolOnly(ZombieType theZombieType)
{
	return (theZombieType == ZombieType::ZOMBIE_SNORKEL || theZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER);
}

bool Board::RowCanHaveZombieType(int theRow, ZombieType theZombieType)
{
	if (!RowCanHaveZombies(theRow))
	{
		return false;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RESODDED && mPlantRow[theRow] == PlantRowType::PLANTROW_DIRT && mCurrentWave < 5)
	{
		return false;
	}
	if (mPlantRow[theRow] == PlantRowType::PLANTROW_POOL && !Zombie::ZombieTypeCanGoInPool(theZombieType) && theZombieType != ZombieType::ZOMBIE_BALLOON)
	{
		return false;
	}
	if (mPlantRow[theRow] == PlantRowType::PLANTROW_HIGH_GROUND && !Zombie::ZombieTypeCanGoOnHighGround(theZombieType))
	{
		return false;
	}

	int aCurrentWave = mCurrentWave;
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		aCurrentWave += mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage();
	}
	if (mPlantRow[theRow] == PlantRowType::PLANTROW_POOL)
	{
		if (aCurrentWave < 5 && !IsZombieTypePoolOnly(theZombieType))
		{
			return false;
		}
	}
	else if (IsZombieTypePoolOnly(theZombieType))
	{
		return false;
	}
	if (theZombieType == ZOMBIE_BOBSLED && !mIceTimer[theRow])
	{
		return false;
	}
	if (theRow == 0 && !mApp->IsSurvivalMode())
	{
		if (theZombieType == ZombieType::ZOMBIE_GARGANTUAR || theZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
		{
			return false;
		}
	}
	if (theZombieType != ZombieType::ZOMBIE_DANCER || StageHasPool())
	{
		return true;
	}
	// Outside pool levels, dancer zombies need rows on both sides to summon backup dancers
	return RowCanHaveZombies(theRow - 1) && RowCanHaveZombies(theRow + 1);
}

int Board::PickRowForNewZombie(ZombieType theZombieType)
{
	// A rake that is attracting a zombie pulls the new zombie into its row
	GridItem* aRake = GetRake();
	if (aRake && aRake->mGridItemState == GridItemState::GRIDITEM_STATE_RAKE_ATTRACTING && RowCanHaveZombieType(aRake->mGridY, theZombieType))
	{
		aRake->mGridItemState = GridItemState::GRIDITEM_STATE_RAKE_WAITING;
		PvzpUpdateSmoothArrayPick(mRowPickingArray, MAX_GRID_SIZE_Y, aRake->mGridY);
		return aRake->mGridY;
	}

	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (!RowCanHaveZombieType(aRow, theZombieType))
		{
			mRowPickingArray[aRow].mWeight = 0;
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT)
		{
			mRowPickingArray[aRow].mWeight = mChallenge->PortalCombatRowSpawnWeight(aRow);
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL && mCurrentWave <= 3 && aRow == 5)
		{
			mRowPickingArray[aRow].mWeight = 0;
		}
		// lawn mower loss protection
		else
		{
			int aWavesMowered = mCurrentWave - mWaveRowGotLawnMowered[aRow];
			if (mApp->IsContinuousChallenge() && mCurrentWave == mNumWaves - 1)
			{
				aWavesMowered = 100;
			}

			if (aWavesMowered <= 1)
			{
				mRowPickingArray[aRow].mWeight = 0.01f;
			}
			else if (aWavesMowered <= 2)
			{
				mRowPickingArray[aRow].mWeight = 0.5f;
			}
			else
			{
				mRowPickingArray[aRow].mWeight = 1.0f;
			}
		}
	}
	return PvzpPickFromSmoothArray(mRowPickingArray, MAX_GRID_SIZE_Y);
}

bool Board::CanAddBobSled()
{
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (mIceTimer[aRow] > 0 && mIceMinX[aRow] < 700)
		{
			return true;
		}
	}
	return false;
}

Zombie* Board::AddZombieInRow(ZombieType theZombieType, int theRow, int theFromWave)
{
    if (!CanAllocateZombieAnimations(theZombieType,theFromWave)) return nullptr;
	// Reserve the whole sled team before constructing its leader; keep the original spare slot.
	const unsigned int aRequired = theZombieType == ZombieType::ZOMBIE_BOBSLED &&
		theFromWave != Zombie::ZOMBIE_WAVE_CUTSCENE && theFromWave != Zombie::ZOMBIE_WAVE_UI ? 4U : 1U;
	if (mZombies.mSize >= mZombies.mMaxSize || aRequired >= mZombies.mMaxSize - mZombies.mSize)
	{
		PvzpLogLn("[capacity] zombie spawn rejected type {} wave {}: {}/{}, need {} plus reserve", (int)theZombieType, theFromWave, mZombies.mSize, mZombies.mMaxSize, aRequired);
		return nullptr;
	}

	if (theZombieType == ZombieType::ZOMBIE_YETI) {
		if (mApp->IsAdventureMode() && mLevel == 40 && theFromWave >= 0)
			ReportAchievement::GiveAchievement(mApp, Zombologist, true);
	}

	bool aVariant = !Rand(5);
	Zombie* aZombie = mZombies.DataArrayAlloc();
	aZombie->ZombieInitialize(theRow, theZombieType, aVariant, nullptr, theFromWave);
	if (theZombieType == ZombieType::ZOMBIE_BOBSLED && aZombie->IsOnBoard())
	{
		for (int _i = 0; _i < 3; _i++)
		{
			mZombies.DataArrayAlloc()->ZombieInitialize(theRow, ZombieType::ZOMBIE_BOBSLED, false, aZombie, theFromWave);
		}
	}
	return aZombie;
}

Zombie* Board::AddZombie(ZombieType theZombieType, int theFromWave)
{
	if (!CanAllocateZombieAnimations(theZombieType,theFromWave)) return nullptr;
	return AddZombieInRow(theZombieType, PickRowForNewZombie(theZombieType), theFromWave);
}

void Board::RemoveAllZombies()
{
    RunSpeciesRuntime::Reset(this);
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (!aZombie->IsDeadOrDying())
		{
			aZombie->DieNoLoot();
		}
	}
}

void Board::RemoveZombiesForRepick()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (!aZombie->IsDeadOrDying() && aZombie->mMindControlled && aZombie->mPosX > 720)
		{
			aZombie->DieNoLoot();
		}
	}
}

void Board::RemoveCutsceneZombies()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE)
		{
			aZombie->DieNoLoot();
		}
	}
}

bool Board::IsIceAt(int theGridX, int theGridY)
{
	PVZP_ASSERT(theGridY >= 0 && theGridY < MAX_GRID_SIZE_Y);
	if (mIceTimer[theGridY] == 0 || mIceMinX[theGridY] > 750)
		return false;

	return theGridX >= PixelToGridXKeepOnBoard(mIceMinX[theGridY] + 12, 0);
}

PlantingReason Board::CanPlantAt(int theGridX, int theGridY, SeedType theSeedType)
{
	// 目标位置不在场地内，则返回“不能种在那里”
	if (theGridX < 0 || theGridX >= MAX_GRID_SIZE_X || theGridY < 0 || theGridY >= MAX_GRID_SIZE_Y)
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}

	// 从关卡玩法上，判断能否种植
	PlantingReason aReason = mChallenge->CanPlantAt(theGridX, theGridY, theSeedType);
	if (aReason != PlantingReason::PLANTING_OK || Challenge::IsZombieSeedType(theSeedType))
	{
		return aReason;
	}

	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(theGridX, theGridY, &aPlantOnLawn);
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		if (aPlantOnLawn.mUnderPlant || aPlantOnLawn.mPumpkinPlant || aPlantOnLawn.mFlyingPlant || aPlantOnLawn.mNormalPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}
		if (mApp->mZenGarden->mGardenType == GARDEN_AQUARIUM && !Plant::IsAquatic(theSeedType))
		{
			return PlantingReason::PLANTING_NOT_ON_WATER;
		}

		return PlantingReason::PLANTING_OK;
	}

	// 墓碑吞噬者只能种植在墓碑上
	bool aHasGrave = GetGraveStoneAt(theGridX, theGridY);
	if (theSeedType == SeedType::SEED_GRAVEBUSTER)
	{
		if (aPlantOnLawn.mNormalPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}

		return aHasGrave ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_ONLY_ON_GRAVES;
	}
	if (theSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		if (aPlantOnLawn.mFlyingPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}

		// @pvz-online: 闯关里蘑菇全醒，没有"睡着的植物"可浇——咖啡豆改行产阳光
		// （见 Plant::DoSpecial），种植条件因此放宽成"下面任意一株普通植物"
		if (mApp->IsRunMode())
		{
			if (!aPlantOnLawn.mNormalPlant ||
				aPlantOnLawn.mNormalPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
			{
				return PlantingReason::PLANTING_NOT_HERE;
			}

			return PlantingReason::PLANTING_OK;
		}

		if (!aPlantOnLawn.mNormalPlant || !aPlantOnLawn.mNormalPlant->mIsAsleep || aPlantOnLawn.mNormalPlant->mWakeUpCounter > 0 ||
			aPlantOnLawn.mNormalPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
		{
			return PlantingReason::PLANTING_NEEDS_SLEEPING;
		}

		return PlantingReason::PLANTING_OK;
	}
	// 非墓碑吞噬者且非飞行植物，则不能种在墓碑上
	if (aHasGrave)
	{
		return Plant::IsFlying(theSeedType) ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_NOT_ON_GRAVE;
	}

	Plant* aUnderPlant = aPlantOnLawn.mUnderPlant;
	const bool aHasLilypad = aPlantOnLawn.mLilypadPlant &&
		aPlantOnLawn.mLilypadPlant->mSeedType == SeedType::SEED_LILYPAD &&
		aPlantOnLawn.mLilypadPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE;
	const bool aHasFlowerPot = aPlantOnLawn.mFlowerPotPlant &&
		aPlantOnLawn.mFlowerPotPlant->mSeedType == SeedType::SEED_FLOWERPOT &&
		aPlantOnLawn.mFlowerPotPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE;
	// 部分情况下的格子中不能种植植物
	if (GetCraterAt(theGridX, theGridY))
	{
		return PlantingReason::PLANTING_NOT_ON_CRATER;
	}
	if (GetScaryPotAt(theGridX, theGridY) || IsIceAt(theGridX, theGridY))
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}
	GridSquareType aGridSquare = mGridSquareType[theGridX][theGridY];
	if (aGridSquare == GridSquareType::GRIDSQUARE_DIRT || aGridSquare == GridSquareType::GRIDSQUARE_NONE)
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}
	// 水生植物只能种在水上
	Plant* aNormalPlant = aPlantOnLawn.mNormalPlant;
	if (theSeedType == SeedType::SEED_LILYPAD || theSeedType == SeedType::SEED_TANGLEKELP || theSeedType == SeedType::SEED_SEASHROOM)
	{
		// A pot supports a lily on land/roofs; a lily supports a pot on water.
		const bool aStackedAquatic = aHasFlowerPot && aHasLilypad;
		if (!IsPoolSquare(theGridX, theGridY) && (!mApp->IsRunMode() || StageHasRoof()) &&
			!(theSeedType == SeedType::SEED_LILYPAD ? aHasFlowerPot : aStackedAquatic))
		{
			return PlantingReason::PLANTING_ONLY_IN_POOL;
		}

		if (theSeedType == SeedType::SEED_LILYPAD)
		{
			return (aNormalPlant || aPlantOnLawn.mLilypadPlant || (aUnderPlant && !aHasFlowerPot) ||
				(aUnderPlant && aPlantOnLawn.mPumpkinPlant)) ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
		}
		return (aNormalPlant || (aUnderPlant && !aStackedAquatic)) ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	if (Plant::IsFlying(theSeedType))
	{
		return aPlantOnLawn.mFlyingPlant ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	// Roof tiles support mines and spikes directly; pots remain optional on roofs.
	const bool aBareRoofPlant = StageHasRoof() && (theSeedType == SeedType::SEED_POTATOMINE ||
		theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK);
	// 地刺/地刺王不能种在水面或非屋顶的底座上。
	if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
	{
		if (aGridSquare == GridSquareType::GRIDSQUARE_POOL || (aUnderPlant && !(aBareRoofPlant && aHasFlowerPot)))
		{
			return PlantingReason::PLANTING_NEEDS_GROUND;
		}
	}
	// 非水生植物不能种在水面上（南瓜头可以种在香蒲上）
	Plant* aPumpkinPlant = aPlantOnLawn.mPumpkinPlant;
	if (aGridSquare == GridSquareType::GRIDSQUARE_POOL && !aHasLilypad && theSeedType != SeedType::SEED_CATTAIL)
	{
		if (!aNormalPlant || aNormalPlant->mSeedType != SeedType::SEED_CATTAIL || theSeedType != SeedType::SEED_PUMPKINSHELL)
		{
			return PlantingReason::PLANTING_NOT_ON_WATER;
		}
	}
	// 花盆的种植条件
	if (theSeedType == SeedType::SEED_FLOWERPOT)
	{
		return (aNormalPlant || aPlantOnLawn.mFlowerPotPlant || aPumpkinPlant || (aUnderPlant && !aHasLilypad)) ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	// 屋顶种植需要花盆
	if (StageHasRoof() && !aHasFlowerPot && !aBareRoofPlant)
	{
		return PlantingReason::PLANTING_NEEDS_POT;
	}
	// 南瓜头的种植条件
	// @pvz-online: 坚果包扎术作为商店永久购买的特例，也适用于本机闯关/无尽。
	bool aAidPurchased = (mApp->IsRunMode() || !mApp->IsOnlineGame()) &&
		mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FIRSTAID] > 0;
	if (theSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		// 不可种植在玉米加农炮上
		if (aNormalPlant && aNormalPlant->mSeedType == SeedType::SEED_COBCANNON)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}
		// 无南瓜头时，可以种植南瓜头
		if (!aPumpkinPlant)
		{
			return PlantingReason::PLANTING_OK;
		}
		// 南瓜头的坚果包扎术
		if (aAidPurchased && aPumpkinPlant->mPlantHealth < aPumpkinPlant->mPlantMaxHealth * 2 / 3 &&
			aPumpkinPlant->mSeedType == SeedType::SEED_PUMPKINSHELL && aPumpkinPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
		{
			return PlantingReason::PLANTING_OK;
		}

		return PlantingReason::PLANTING_NOT_HERE;
	}
	// 土豆地雷只能种在陆地上
	if (aHasLilypad && theSeedType == SeedType::SEED_POTATOMINE)
	{
		return PlantingReason::PLANTING_ONLY_ON_GROUND;
	}

	if (aUnderPlant)
	{
		// 香蒲对底端植物的紫卡升级
		if (theSeedType == SeedType::SEED_CATTAIL)
		{
			if (aNormalPlant)
			{
				return PlantingReason::PLANTING_NOT_HERE;
			}
			if (aHasLilypad && aPlantOnLawn.mLilypadPlant->IsUpgradableTo(theSeedType))
			{
				return PlantingReason::PLANTING_OK;
			}
			// @pvz-online: 闯关里紫卡直接种（不用底座）——香蒲不再要求"先有睡莲才能升"
			if (Plant::IsUpgrade(theSeedType) && !mApp->IsRunMode())
			{
				return PlantingReason::PLANTING_NEEDS_UPGRADE;
			}
		}
		else
		{
			// 模仿中的模仿者不可作为花盆或睡莲
			if (aUnderPlant->mSeedType == SeedType::SEED_IMITATER)
			{
				return PlantingReason::PLANTING_NOT_HERE;
			}
		}
	}

	// 一般紫卡植物的更迭判断
	if (aNormalPlant)
	{
		// 紫卡植物的升级
		if (aNormalPlant->IsUpgradableTo(theSeedType) && aNormalPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
		{
			return PlantingReason::PLANTING_OK;
		}
		// @pvz-online: 闯关里紫卡就是普通植物（直接种），落在地块已占的位置按"占位"处理
		if (Plant::IsUpgrade(theSeedType) && !mApp->IsRunMode())
		{
			return PlantingReason::PLANTING_NEEDS_UPGRADE;
		}

		// 坚果包扎术
		if (((theSeedType == SeedType::SEED_WALLNUT || theSeedType == SeedType::SEED_TALLNUT) && aAidPurchased) ||
			(theSeedType == SeedType::SEED_WALLNUT && mApp->RunExtraBuffCount(RUN_BUFF2_WALLNUT_AID) > 0))
		{
			if (aNormalPlant->mPlantHealth < (mApp->RunExtraBuffCount(RUN_BUFF2_WALLNUT_AID) > 0 &&
				theSeedType == SeedType::SEED_WALLNUT ? aNormalPlant->mPlantMaxHealth : aNormalPlant->mPlantMaxHealth * 2 / 3) &&
				aNormalPlant->mSeedType == theSeedType && aNormalPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
			{
				return PlantingReason::PLANTING_OK;
			}
		}

		return PlantingReason::PLANTING_NOT_HERE;
	}

	// 免费种植模式下紫卡的额外判断（闯关里紫卡直接种、不吃这条）
	if (!mApp->mEasyPlantingCheat && Plant::IsUpgrade(theSeedType) && !mApp->IsRunMode())
	{
		return PlantingReason::PLANTING_NEEDS_UPGRADE;
	}
	// @pvz-online: 闯关里加农炮同样直接种（用户 2026-10-03 定案）——不必先凑两个玉米投手；
	// 它占锚点格 + 右邻格（占位见 GetPlantsOnLawn），所以右邻格也得留出地方：越界或已有
	// 普通/南瓜头/飞行植物都算种不下，免得两株叠在同一格（非闯关照旧要合法炮弹位）。
	if (theSeedType == SeedType::SEED_COBCANNON && !IsValidCobCannonSpot(theGridX, theGridY))
	{
		if (!mApp->IsRunMode())
		{
			return PlantingReason::PLANTING_NEEDS_UPGRADE;
		}
		PlantsOnLawn aRightLawn;
		GetPlantsOnLawn(theGridX + 1, theGridY, &aRightLawn);
		if (theGridX + 1 >= MAX_GRID_SIZE_X ||
			aRightLawn.mNormalPlant || aRightLawn.mPumpkinPlant || aRightLawn.mFlyingPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}
	}
	// @pvz-online: 香蒲同理（用户 2026-10-03 定案）——闯关里草坪也能直接种；非闯关照旧只限泳池
	else if (theSeedType == SeedType::SEED_CATTAIL && aGridSquare != GridSquareType::GRIDSQUARE_POOL && !mApp->IsRunMode())
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}

	// @pvz-online: 批 18 玉米投手「Artillery」——种下即变玉米加农炮（1 层）。加农炮占锚点格 +
	// 右邻格（占位见 GetPlantsOnLawn）：右邻格越界或已有普通/南瓜头/飞行植物都算放不下，拦在
	// 这里——校验在扣费之前，所以是"提示种不下、不扣阳光"（用户拍板 ④）。mEasyPlantingCheat
	// 短路一是照抄上面紫卡的老规矩，二是断递归——cheat 开时 IsValidCobCannonSpotHelper 会递归
	// 调回 CanPlantAt(KERNELPULT)。
	if (theSeedType == SeedType::SEED_KERNELPULT && mApp->IsRunMode() &&
		mApp->RunPlantUpgradeCount(SeedType::SEED_KERNELPULT) > 0 &&
		!(mApp->mEasyPlantingCheat || IsValidCobCannonSpot(theGridX, theGridY)))
	{
		PlantsOnLawn aRightLawn;
		GetPlantsOnLawn(theGridX + 1, theGridY, &aRightLawn);
		if (theGridX + 1 >= MAX_GRID_SIZE_X ||
			aRightLawn.mNormalPlant || aRightLawn.mPumpkinPlant || aRightLawn.mFlyingPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}
	}

	return PlantingReason::PLANTING_OK;
}

void Board::UpdateCursor(const HitResult* theHitResult)
{
	int aMouseX = mApp->mWidgetManager->mLastMouseX - mX;
	int aMouseY = mApp->mWidgetManager->mLastMouseY - mY;
	bool aShowFinger = false;
	bool aShowDrag = false;
	bool aHideCursor = false;

	if (mApp->mSeedChooserScreen && mApp->mSeedChooserScreen->Contains(aMouseX + mX, aMouseY + mY))
		return;

	if (mApp->GetDialogCount() > 0)
		return;

	if (mPaused || mBoardFadeOutCounter >= 0 || mTimeStopCounter > 0 || mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		mApp->SetCursor(Sexy::CURSOR_POINTER);
		return;
	}

	HitResult aLocalHitResult;
	if (theHitResult == nullptr)
	{
		MouseHitTest(aMouseX, aMouseY, &aLocalHitResult);
		theHitResult = &aLocalHitResult;
	}
	switch (theHitResult->mObjectType)
	{
	case GameObjectType::OBJECT_TYPE_MENU_BUTTON:
	case GameObjectType::OBJECT_TYPE_STORE_BUTTON:
	case GameObjectType::OBJECT_TYPE_SHOVEL:
	case GameObjectType::OBJECT_TYPE_WATERING_CAN:
	case GameObjectType::OBJECT_TYPE_FERTILIZER:
	case GameObjectType::OBJECT_TYPE_BUG_SPRAY:
	case GameObjectType::OBJECT_TYPE_PHONOGRAPH:
	case GameObjectType::OBJECT_TYPE_CHOCOLATE:
	case GameObjectType::OBJECT_TYPE_GLOVE:
	case GameObjectType::OBJECT_TYPE_MONEY_SIGN:
	case GameObjectType::OBJECT_TYPE_NEXT_GARDEN:
	case GameObjectType::OBJECT_TYPE_WHEELBARROW:
	case GameObjectType::OBJECT_TYPE_SLOT_MACHINE_HANDLE:
	case GameObjectType::OBJECT_TYPE_TREE_FOOD:
	case GameObjectType::OBJECT_TYPE_STINKY:
	case GameObjectType::OBJECT_TYPE_TREE_OF_WISDOM:
	case GameObjectType::OBJECT_TYPE_COIN:
	case GameObjectType::OBJECT_TYPE_PROJECTILE:
		aShowFinger = true;
		break;

	case GameObjectType::OBJECT_TYPE_SEEDPACKET:
		aShowFinger = ((SeedPacket*)theHitResult->mObject)->CanPickUp();
		break;

	case GameObjectType::OBJECT_TYPE_SCARY_POT:
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL)
		{
			aShowFinger = true;
		}
		else if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
		{
			aHideCursor = true;
		}
		break;

	case GameObjectType::OBJECT_TYPE_PLANT:
		if ((mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST) && !HasLevelAwardDropped())
		{
			aShowFinger = true;
		}
		if (((Plant*)theHitResult->mObject)->mState == PlantState::STATE_COBCANNON_READY)
		{
			aShowFinger = true;
		}
		break;

	default:
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
		{
			aHideCursor = true;
		}
		break;
	}

	if (mChallenge->mBeghouledMouseCapture || aShowDrag)
	{
		mApp->SetCursor(Sexy::CURSOR_DRAGGING);
	}
	else if (aShowFinger)
	{
		mApp->SetCursor(Sexy::CURSOR_HAND);
	}
	else if (aHideCursor)
	{
		mApp->SetCursor(Sexy::CURSOR_NONE);
	}
	else
	{
		mApp->SetCursor(Sexy::CURSOR_POINTER);
	}
}

void Board::MouseMove(int x, int y)
{
    if(mApp->TeamProgressionInputBlocked())return;
	Widget::MouseMove(x, y);
	mChallenge->MouseMove(x, y);
}

void Board::MouseDrag(int x, int y)
{
    if(mApp->TeamProgressionInputBlocked())return;
    if (mCoopTouchCaptured || mMobileWatchOpen) return;
#ifdef __ANDROID__
	// @pvz-online: a small movement threshold keeps tapping a card available.
	if (mTouchSeedDrag &&
		(std::abs(x - mTouchSeedDragStartX) >= 8 || std::abs(y - mTouchSeedDragStartY) >= 8))
	{
		mTouchSeedDragMoved = true;
	}
#endif
	Widget::MouseDrag(x, y);
	mChallenge->MouseMove(x, y);
    if (mApp->mSlideCollect && !mPaused && !mIgnoreMouseUp &&
        mApp->mGameScene == SCENE_PLAYING && mApp->GetDialogCount() == 0 &&
        mCursorObject->mCursorType == CURSOR_TYPE_NORMAL)
    {
        HitResult hit;
        if (MouseHitTest(x, y, &hit) && hit.mObjectType == OBJECT_TYPE_COIN)
        {
            auto* coin = static_cast<Coin*>(hit.mObject);
            if (coin->mBoard == this && !coin->mDead && !coin->mVisualOnly &&
                coin->IsSun() && !coin->mIsBeingCollected)
            {
                coin->Collect();
                UpdateCursor();
            }
        }
    }
}

Zombie* Board::ZombieHitTest(int theMouseX, int theMouseY)
{
	Zombie* aRecord = nullptr;
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead || aZombie->IsDeadOrDying())
			continue;

		// ignore zombotany zombies shown during the level intro
		if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && Zombie::IsZombotany(aZombie->mZombieType))
			continue;

		if (aZombie->GetZombieRect().Contains(theMouseX, theMouseY))
		{
			if (aRecord == nullptr || aZombie->mY > aRecord->mY)
			{
				aRecord = aZombie;
			}
		}
	}

	return aRecord;
}

bool Board::IsPlantInGoldWateringCanRange(int theMouseX, int theMouseY, Plant* thePlant)
{
	/*
	Rect aRect = Rect(theMouseX - 70, theMouseY - 80, 160, 160);
	if (GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ZEN_TOOL_ORDER) == thePlant)
	{
		return aRect.Contains(thePlant->mX + 40, thePlant->mY + 40);
	}
	return false;
	*/

	int aMinX = theMouseX - 70;
	int aMaxX = theMouseX + 90;
	int aMinY = theMouseY - 80;
	int aMaxY = theMouseY + 80;

	if (GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ZEN_TOOL_ORDER) == thePlant)
	{
		return thePlant->mX + 40 >= aMinX && thePlant->mX + 40 < aMaxX && thePlant->mY + 40 >= aMinY && thePlant->mY + 40 < aMaxY;
	}
	return false;
}

void Board::HighlightPlantsForMouse(int theMouseX, int theMouseY, const HitResult* theHitResult)
{
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WATERING_CAN && mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_GOLD_WATERINGCAN])
	{
		for (Plant* aPlant : mPlants)
		{
			if (aPlant->mDead)
				continue;
			if (IsPlantInGoldWateringCanRange(theMouseX, theMouseY, aPlant))
			{
				aPlant->mHighlighted = true;
				Plant* aFlowerPot = GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
				if (aFlowerPot)
				{
					aFlowerPot->mHighlighted = true;
				}
			}
		}
	}
	else
	{
		Plant* aPlant = theHitResult->mObjectType == GameObjectType::OBJECT_TYPE_PLANT ? ToolHitTestHelper(theHitResult) : nullptr;
		if (aPlant)
		{
			aPlant->mHighlighted = true;
			if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
			{
				Plant* aFlowerPot = GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
				if (aFlowerPot)
				{
					aFlowerPot->mHighlighted = true;
				}
			}
		}
	}
}

void Board::UpdateMousePosition()
{
	SeedType aCursorSeedType = GetSeedTypeInCursor();
	int aMouseX = mApp->mWidgetManager->mLastMouseX - mX;
	int aMouseY = mApp->mWidgetManager->mLastMouseY - mY;
	HitResult aHitResult;
	MouseHitTest(aMouseX, aMouseY, &aHitResult);

	UpdateCursor(&aHitResult);
	UpdateToolTip(&aHitResult);
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		aPlant->mHighlighted = false;
	}

	if (mApp->IsScaryPotterLevel())
	{
		for (GridItem* aGridItem : mGridItems)
		{
			if (aGridItem->mDead)
				continue;
			if (aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT)
			{
				aGridItem->mHighlighted = false;
			}
		}

		if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_SCARY_POT)
		{
			GridItem* aScaryPot = (GridItem*)aHitResult.mObject;
			aScaryPot->mHighlighted = true;
			return;
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		GridItem* aStinky = mApp->mZenGarden->GetStinky();
		if (aStinky)
		{
			aStinky->mHighlighted = aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_STINKY;
		}
	}

	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_SHOVEL ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WATERING_CAN ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_FERTILIZER ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_BUG_SPRAY ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PHONOGRAPH ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_CHOCOLATE ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_GLOVE ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_MONEY_SIGN ||
		(mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WHEEELBARROW && !mApp->mZenGarden->GetPottedPlantInWheelbarrow()))
	{
		HighlightPlantsForMouse(aMouseX, aMouseY, &aHitResult);
		return;
	}

	// coffee bean and wall-nut first aid highlighting
	if (aCursorSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		int aGridX = PlantingPixelToGridX(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY, aCursorSeedType);
		int aGridY = PlantingPixelToGridY(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY, aCursorSeedType);

		Plant* aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aPlant && (mApp->IsRunMode() || aPlant->mIsAsleep) && CanPlantAt(aGridX, aGridY, SeedType::SEED_INSTANT_COFFEE) == PlantingReason::PLANTING_OK)
		{
			aPlant->mHighlighted = true;
		}
	}
	else if (aCursorSeedType == SeedType::SEED_WALLNUT || aCursorSeedType == SeedType::SEED_TALLNUT)
	{
		int aGridX = PlantingPixelToGridX(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY, aCursorSeedType);
		int aGridY = PlantingPixelToGridY(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY, aCursorSeedType);

		Plant* aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_PUMPKIN);
		if (aPlant && aPlant->mSeedType == aCursorSeedType && CanPlantAt(aGridX, aGridY, aCursorSeedType) == PlantingReason::PLANTING_OK)
		{
			aPlant->mHighlighted = true;
		}
	}
	else if (aCursorSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		int aGridX = PlantingPixelToGridX(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY, aCursorSeedType);
		int aGridY = PlantingPixelToGridY(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY, aCursorSeedType);

		Plant* aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aPlant && aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL && CanPlantAt(aGridX, aGridY, SeedType::SEED_PUMPKINSHELL) == PlantingReason::PLANTING_OK)
		{
			aPlant->mHighlighted = true;
		}
	}
}

void Board::UpdateToolTip(const HitResult* theHitResult)
{
	if (!mApp->mWidgetManager->mMouseIn || !mApp->mActive || mTimeStopCounter > 0 || mApp->GetDialogCount() > 0 || mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		mToolTip->mVisible = false;
		return;
	}

	int aMouseX = mApp->mWidgetManager->mLastMouseX - mX;
	int aMouseY = mApp->mWidgetManager->mLastMouseY - mY;

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
	{
		if (!mCutScene->mSeedChoosing)
		{
			mToolTip->mVisible = false;
			return;
		}

		if (mSeedBank->ContainsPoint(mWidgetManager->mLastMouseX, mWidgetManager->mLastMouseY) ||
			mApp->mSeedChooserScreen->mAlmanacButton->IsMouseOver() ||
			mApp->mSeedChooserScreen->mStoreButton->IsMouseOver() ||
			mApp->mSeedChooserScreen->mImitaterButton->IsMouseOver())
		{
			mToolTip->mVisible = false;
			return;
		}

		Zombie* aZombie = ZombieHitTest(aMouseX, aMouseY);
		if (aZombie == nullptr || aZombie->mFromWave != Zombie::ZOMBIE_WAVE_CUTSCENE)
		{
			mToolTip->mVisible = false;
			return;
		}

		std::string aZombieName = std::format("[{}]", GetZombieDefinition(aZombie->mZombieType).mZombieName);
		mToolTip->SetTitle(aZombieName);
		if (mApp->CanShowAlmanac() && aZombie->mZombieType != ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
		{
			mToolTip->SetLabel("[CLICK_TO_VIEW]");
		}
		else
		{
			mToolTip->SetLabel("");
		}
		mToolTip->SetWarningText("");

		Rect aRect = aZombie->GetZombieRect();
		mToolTip->mX = aRect.mWidth / 2 + aRect.mX + 5;
		mToolTip->mY = aRect.mHeight + aRect.mY - 10;
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE)
		{
			mToolTip->mY = aZombie->mY;
		}

		mToolTip->mVisible = true;
		mToolTip->mCenter = true;

		mToolTip->mMinLeft = IMAGE_SEEDCHOOSER_BACKGROUND->GetWidth();
		if (mApp->mSeedChooserScreen->mAlmanacButton->mBtnNoDraw && mApp->mSeedChooserScreen->mStoreButton->mBtnNoDraw)
		{
			mToolTip->mMaxBottom = 600;
		}
		else
		{
			mToolTip->mMaxBottom = 570;
		}
		if (!mApp->mSeedChooserScreen->mImitaterButton->mBtnNoDraw)
		{
			mToolTip->CalculateSize();
			if (mX + mToolTip->mX - mToolTip->mWidth / 2 < 524)
			{
				mToolTip->mMaxBottom = 503;
			}
		}

		return;
	}

	if (!CanInteractWithBoardButtons())
	{
		mToolTip->mVisible = false;
		return;
	}

	mToolTip->mMinLeft = 0;
	mToolTip->mMaxBottom = BOARD_HEIGHT;
	mToolTip->SetTitle("");
	mToolTip->SetLabel("");
	mToolTip->SetWarningText("");
	mToolTip->mCenter = false;
	if (mChallenge->UpdateToolTip(aMouseX, aMouseY, theHitResult))
	{
		return;
	}

	HitResult aLocalHitResult;
	if (theHitResult == nullptr)
	{
		MouseHitTest(aMouseX, aMouseY, &aLocalHitResult);
		theHitResult = &aLocalHitResult;
	}

	switch (theHitResult->mObjectType)
	{
	case GameObjectType::OBJECT_TYPE_SHOVEL:
	{
		mToolTip->SetLabel("[SHOVEL_TOOLTIP]");
		Rect aShovelButtonRect = GetShovelButtonRect();
		mToolTip->mX = aShovelButtonRect.mX + 35;
		mToolTip->mY = aShovelButtonRect.mY + 72;
		mToolTip->mCenter = true;
		mToolTip->mVisible = true;
		return;
	}

	case GameObjectType::OBJECT_TYPE_NEXT_GARDEN:
	{
		mToolTip->SetLabel("[NEXT_GARDEN_TOOLTIP]");
		Rect aButtonRect = GetShovelButtonRect();
		mToolTip->mX = 599;
		mToolTip->mY = aButtonRect.mY + 52;
		mToolTip->mCenter = true;
		mToolTip->mVisible = true;
		return;
	}

	case GameObjectType::OBJECT_TYPE_WATERING_CAN:
		mToolTip->SetLabel("[WATERING_CAN_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_FERTILIZER:
		mToolTip->SetLabel("[FERTILIZER_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_BUG_SPRAY:
		mToolTip->SetLabel("[BUG_SPRAY_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_PHONOGRAPH:
		mToolTip->SetLabel("[PHONOGRAPH_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_CHOCOLATE:
		mToolTip->SetLabel("[CHOCOLATE_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_GLOVE:
		mToolTip->SetLabel("[GLOVE_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_MONEY_SIGN:
		mToolTip->SetLabel("[MONEY_SIGN_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_WHEELBARROW:
		mToolTip->SetLabel("[WHEELBARROW_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_TREE_FOOD:
		mToolTip->SetLabel("[TREE_FERTILIZER_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_SEEDPACKET:
		break;
	default:
		mToolTip->mVisible = false;
		return;
	}

	if (theHitResult->mObjectType != GameObjectType::OBJECT_TYPE_SEEDPACKET)
	{
		Rect aButtonRect = GetShovelButtonRect();
		GetZenButtonRect(theHitResult->mObjectType, aButtonRect);
		this->mToolTip->mX = aButtonRect.mX + 35;
		this->mToolTip->mY = aButtonRect.mY + 72;
		this->mToolTip->mCenter = true;
		this->mToolTip->mVisible = true;
		return;
	}

	SeedPacket* aSeedPacket = (SeedPacket*)theHitResult->mObject;
	SeedType aUseSeedType = aSeedPacket->mPacketType;
	if (aSeedPacket->mPacketType == SeedType::SEED_IMITATER && aSeedPacket->mImitaterType != SeedType::SEED_NONE)
	{
		aUseSeedType = aSeedPacket->mImitaterType;
	}

	if (gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST)
	{
		if (aUseSeedType == SeedType::SEED_REPEATER)
		{
			mToolTip->SetLabel("[BEGHOULED_REPEATER_UPGRADE_TOOLTIP]");
		}
		else if (aUseSeedType == SeedType::SEED_FUMESHROOM)
		{
			mToolTip->SetLabel("[BEGHOULED_FUMESHROOM_UPGRADE_TOOLTIP]");
		}
		else if (aUseSeedType == SeedType::SEED_TALLNUT)
		{
			mToolTip->SetLabel("[BEGHOULED_TALLNUT_UPGRADE_TOOLTIP]");
		}
		else if (aUseSeedType == SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE)
		{
			mToolTip->SetLabel("[BEGHOULED_SHUFFLE_TOOLTIP]");
		}
		else if (aUseSeedType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
		{
			mToolTip->SetLabel("[BEGHOULED_CRATER_TOOLTIP]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_SLOT_MACHINE_SUN)
	{
		mToolTip->SetLabel("[SLOT_MACHINE_SUN_TOOLTIP]");
	}
	else if (aUseSeedType == SeedType::SEED_SLOT_MACHINE_DIAMOND)
	{
		mToolTip->SetLabel("[SLOT_MACHINE_DIAMOND_TOOLTIP]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIQUARIUM_SNORKLE)
	{
		mToolTip->SetLabel("[ZOMBIQUARIUM_SNORKEL_TOOLTIP]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIQUARIUM_TROPHY)
	{
		mToolTip->SetLabel("[ZOMBIQUARIUM_TROPHY_TOOLTIP]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_NORMAL)
	{
		mToolTip->SetLabel("[ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_TRAFFIC_CONE)
	{
		mToolTip->SetLabel("[CONEHEAD_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_POLEVAULTER)
	{
		mToolTip->SetLabel("[POLE_VAULTING_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_PAIL)
	{
		mToolTip->SetLabel("[BUCKETHEAD_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_LADDER)
	{
		mToolTip->SetLabel("[LADDER_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_DIGGER)
	{
		mToolTip->SetLabel("[DIGGER_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_BUNGEE)
	{
		mToolTip->SetLabel("[BUNGEE_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_FOOTBALL)
	{
		mToolTip->SetLabel("[FOOTBALL_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_BALLOON)
	{
		mToolTip->SetLabel("[BALLOON_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_SCREEN_DOOR)
	{
		mToolTip->SetLabel("[SCREEN_DOOR_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBONI)
	{
		mToolTip->SetLabel("[ZOMBONI]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_POGO)
	{
		mToolTip->SetLabel("[POGO_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_DANCER)
	{
		mToolTip->SetLabel("[DANCING_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_GARGANTUAR)
	{
		mToolTip->SetLabel("[GARGANTUAR]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_IMP)
	{
		mToolTip->SetLabel("[IMP]");
	}
	else
	{
		mToolTip->SetLabel(Plant::GetNameString(aSeedPacket->mPacketType, aSeedPacket->mImitaterType));
	}

	int aPlantCost = GetCurrentPlantCost(aSeedPacket->mPacketType, aSeedPacket->mImitaterType);
	if (mApp->mEasyPlantingCheat)
	{
		mToolTip->SetWarningText("FREE_PLANTING_CHEAT");
	}
	else if (!aSeedPacket->mActive && (gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST))
	{
		if (aSeedPacket->mPacketType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
		{
			mToolTip->SetWarningText("[BEGHOULED_NO_CRATERS]");
		}
		else
		{
			mToolTip->SetWarningText("[BEGHOULED_SEED_ALREADY_PURCHASED]");
		}
	}
	else if (!aSeedPacket->mActive)
	{
		mToolTip->SetWarningText("[WAITING_FOR_SEED]");
	}
	else if (!CanTakeSunMoney(aPlantCost) && !HasConveyorBeltSeedBank() && !mApp->IsSlotMachineLevel())
	{
		mToolTip->SetWarningText("[NOT_ENOUGH_SUN]");
	}
	else if (aUseSeedType == SeedType::SEED_GATLINGPEA)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_REPEATER]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_WINTERMELON)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_MELONPULT]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_TWINSUNFLOWER)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_SUNFLOWER]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_SPIKEROCK)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_SPIKEWEED]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_COBCANNON)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_KERNELPULTS]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_GOLD_MAGNET)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_MAGNETSHROOM]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_FUMESHROOM]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_CATTAIL)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_LILY_PAD]");
		}
	}

	mToolTip->mX = (SEED_PACKET_WIDTH - mToolTip->mWidth) / 2 + mSeedBank->mX + aSeedPacket->mOffsetX + aSeedPacket->mX;
	mToolTip->mY = mSeedBank->mY + aSeedPacket->mY + 70;
	mToolTip->mVisible = true;
}

void Board::MouseDownCobcannonFire(int x, int y, int theClickCount)
{
    if(mApp->TeamProgressionInputBlocked())return;
	if (theClickCount >= 0 && y >= 80)
	{
		if (mCobCannonCursorDelayCounter > 0 && Distance2D(x, y, mCobCannonMouseX, mCobCannonMouseY) < 100.0f)
		{
			return;  // misclick guard: ignore clicks within 100px of the crosshair while the 30cs fire delay is active
		}

		if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_DUPLICATOR)
		{
			Plant* aCobcannon = mPlants.DataArrayTryToGet(mCursorObject->mCobCannonPlantID);
			if (aCobcannon)
			{
				aCobcannon->CobCannonFire(x, y);
			}
		}
	}
	ClearCursor();
}

void Board::MouseDownWithPlant(int x, int y, int theClickCount)
{
    if(mApp->TeamProgressionInputBlocked())return;
	// 右击鼠标：放下卡牌
	if (theClickCount < 0)
	{
		RefreshSeedPacketFromCursor();
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		return;
	}

	// 我是僵尸模式中，交由 Challenge 处理
	if (mApp->IsIZombieLevel())
	{
		mChallenge->IZombieMouseDownWithZombie(x, y, theClickCount);
		return;
	}

	SeedType aPlantingSeedType = GetSeedTypeInCursor();
	int aGridX = PlantingPixelToGridX(x, y, aPlantingSeedType);
	int aGridY = PlantingPixelToGridY(x, y, aPlantingSeedType);

	// 不在场地内的点击：放下卡牌
	if (aGridX < 0 || aGridX >= MAX_GRID_SIZE_X || aGridY < 0 || aGridY >= MAX_GRID_SIZE_Y)
	{
		RefreshSeedPacketFromCursor();
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		return;
	}

	PlantingReason aReason = CanPlantAt(aGridX, aGridY, aPlantingSeedType);
	if (aReason != PlantingReason::PLANTING_OK)
	{
		// 根据不同的种植原因播放相应的提示字幕
		if (aReason == PlantingReason::PLANTING_ONLY_ON_GRAVES)
		{
			DisplayAdvice(("[ADVICE_GRAVEBUSTERS_ON_GRAVES]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_GRAVEBUSTERS_ON_GRAVES);
		}
		else if (aPlantingSeedType == SeedType::SEED_LILYPAD)
		{
			if (aReason == PlantingReason::PLANTING_ONLY_IN_POOL)
			{
				DisplayAdvice(("[ADVICE_LILYPAD_ON_WATER]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_LILYPAD_ON_WATER);
			}
		}
		else if (aPlantingSeedType == SeedType::SEED_TANGLEKELP)
		{
			if (aReason == PlantingReason::PLANTING_ONLY_IN_POOL)
			{
				DisplayAdvice(("[ADVICE_TANGLEKELP_ON_WATER]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_TANGLEKELP_ON_WATER);
			}
		}
		else if (aPlantingSeedType == SeedType::SEED_SEASHROOM)
		{
			if (aReason == PlantingReason::PLANTING_ONLY_IN_POOL)
			{
				DisplayAdvice(("[ADVICE_SEASHROOM_ON_WATER]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_SEASHROOM_ON_WATER);
			}
		}
		else if (aReason == PlantingReason::PLANTING_ONLY_ON_GROUND)
		{
			DisplayAdvice(("[ADVICE_POTATO_MINE_ON_LILY]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_POTATOE_MINE_ON_LILY);
		}
		else if (aReason == PlantingReason::PLANTING_NOT_PASSED_LINE)
		{
			DisplayAdvice(("[ADVICE_NOT_PASSED_LINE]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NOT_PASSED_LINE);
		}
		else if (aReason == PlantingReason::PLANTING_NEEDS_UPGRADE)
		{
			switch (aPlantingSeedType)
			{
			case SeedType::SEED_GATLINGPEA:
				DisplayAdvice(("[ADVICE_ONLY_ON_REPEATERS]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_ONLY_ON_REPEATERS);
				break;

			case SeedType::SEED_TWINSUNFLOWER:
				DisplayAdvice(("[ADVICE_ONLY_ON_SUNFLOWER]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_ONLY_ON_SUNFLOWER);
				break;

			case SeedType::SEED_GLOOMSHROOM:
				DisplayAdvice(("[ADVICE_ONLY_ON_FUMESHROOM]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_ONLY_ON_FUMESHROOM);
				break;

			case SeedType::SEED_CATTAIL:
				DisplayAdvice(("[ADVICE_ONLY_ON_LILYPAD]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_ONLY_ON_LILYPAD);
				break;

			case SeedType::SEED_WINTERMELON:
				DisplayAdvice(("[ADVICE_ONLY_ON_MELONPULT]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_ONLY_ON_MELONPULT);
				break;

			case SeedType::SEED_GOLD_MAGNET:
				DisplayAdvice(("[ADVICE_ONLY_ON_MAGNETSHROOM]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_ONLY_ON_MAGNETSHROOM);
				break;

			case SeedType::SEED_SPIKEROCK:
				DisplayAdvice(("[ADVICE_ONLY_ON_SPIKEWEED]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_ONLY_ON_SPIKEWEED);
				break;

			case SeedType::SEED_COBCANNON:
				DisplayAdvice(("[ADVICE_ONLY_ON_KERNELPULT]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_ONLY_ON_KERNELPULT);
				break;
			default:
				break;
			}
		}
		else if (aReason == PlantingReason::PLANTING_NOT_ON_ART)
		{
			std::string aSeedName = Plant::GetNameString(mChallenge->GetArtChallengeSeed(aGridX, aGridY), SeedType::SEED_NONE);
			std::string aMessage = PvzpReplaceString(("ADVICE_WRONG_ART_TYPE"), ("{SEED}"), aSeedName);
			DisplayAdvice(aMessage, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_WRONG_ART_TYPE);
		}
		else if (aReason == PlantingReason::PLANTING_NEEDS_POT)
		{
			if (mApp->IsFirstTimeAdventureMode() && mLevel == 41)
			{
				DisplayAdvice(("[ADVICE_PLANT_NEED_POT1]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NEED_POT);
			}
			else
			{
				DisplayAdvice(("[ADVICE_PLANT_NEED_POT2]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NEED_POT);
			}
		}
		else if (aReason == PlantingReason::PLANTING_NOT_ON_GRAVE)
		{
			DisplayAdvice(("[ADVICE_PLANT_NOT_ON_GRAVE]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NOT_ON_GRAVE);
		}
		else if (aReason == PlantingReason::PLANTING_NOT_ON_CRATER)
		{
			if (IsPoolSquare(aGridX, aGridY))
			{
				DisplayAdvice(("[ADVICE_CANT_PLANT_THERE]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_CANT_PLANT_THERE);
			}
			else
			{
				DisplayAdvice(("[ADVICE_PLANT_NOT_ON_CRATER]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NOT_ON_CRATER);
			}
		}
		else if (aReason == PlantingReason::PLANTING_NOT_ON_WATER)
		{
			if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && mApp->mZenGarden->mGardenType == GardenType::GARDEN_AQUARIUM)
			{
				DisplayAdvice(("[ZEN_ONLY_AQUATIC_PLANTS]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_NONE);
			}
			else if (aPlantingSeedType == SeedType::SEED_POTATOMINE)
			{
				DisplayAdvice(("[ADVICE_POTATO_MINE_ON_LILY]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_POTATOE_MINE_ON_LILY);
			}
			else
			{
				DisplayAdvice(("[ADVICE_PLANT_NOT_ON_WATER]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NOT_ON_WATER);
			}
		}
		else if (aReason == PlantingReason::PLANTING_NEEDS_GROUND)
		{
			DisplayAdvice(("[ADVICE_PLANTING_NEEDS_GROUND]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANTING_NEEDS_GROUND);
		}
		else if (aReason == PlantingReason::PLANTING_NEEDS_SLEEPING)
		{
			DisplayAdvice(("[ADVICE_PLANTING_NEED_SLEEPING]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANTING_NEED_SLEEPING);
		}
		else if (aReason == PlantingReason::PLANTING_NOT_HERE &&
			aPlantingSeedType == SeedType::SEED_KERNELPULT && mApp->IsRunMode() &&
			mApp->RunPlantUpgradeCount(SeedType::SEED_KERNELPULT) > 0)
		{
			// @pvz-online: 批 18 玉米投手「Artillery」——通用 NOT_HERE 原本静默（沿用原版习惯），
			// 但这条路径用户要求"提示种不下"（拍板 ④），补上标准「不能种在这里」提示。
			DisplayAdvice(("[ADVICE_CANT_PLANT_THERE]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_CANT_PLANT_THERE);
		}

		// 特定情况下，放下原有手持的植物
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE || mApp->IsWhackAZombieLevel())
		{
			RefreshSeedPacketFromCursor();
			mApp->PlayFoley(FoleyType::FOLEY_DROP);
		}
		// 不可种植的情况至此结束，直接跳转至返回
		return;
	}

	/* 以下为植物类型可以种植的情况 */
	// 清除种植相关的提示字幕
	ClearAdvice(AdviceType::ADVICE_PLANTING_NEED_SLEEPING);
	ClearAdvice(AdviceType::ADVICE_CANT_PLANT_THERE);
	ClearAdvice(AdviceType::ADVICE_PLANTING_NEEDS_GROUND);
	ClearAdvice(AdviceType::ADVICE_PLANT_NOT_ON_WATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_NOT_ON_CRATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_NOT_ON_GRAVE);
	ClearAdvice(AdviceType::ADVICE_PLANT_NEED_POT);
	ClearAdvice(AdviceType::ADVICE_PLANT_WRONG_ART_TYPE);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_LILYPAD);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_MAGNETSHROOM);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_FUMESHROOM);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_KERNELPULT);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_SUNFLOWER);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_SPIKEWEED);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_MELONPULT);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_REPEATERS);
	ClearAdvice(AdviceType::ADVICE_PLANT_NOT_PASSED_LINE);
	ClearAdvice(AdviceType::ADVICE_PLANT_GRAVEBUSTERS_ON_GRAVES);
	ClearAdvice(AdviceType::ADVICE_PLANT_LILYPAD_ON_WATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_TANGLEKELP_ON_WATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_SEASHROOM_ON_WATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_POTATOE_MINE_ON_LILY);
	ClearAdvice(AdviceType::ADVICE_SURVIVE_FLAGS);

    const bool createsPlant = mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK ||
        mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN;
    if (createsPlant)
    {
        SeedType admissionSeed = mCursorObject->mType;
        if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK &&
            aPlantingSeedType == SeedType::SEED_KERNELPULT && mApp->IsRunMode() &&
            mApp->RunPlantUpgradeCount(SeedType::SEED_KERNELPULT) > 0) admissionSeed = SeedType::SEED_COBCANNON;
        unsigned int plantCount = 1;
        if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
        {
            for (int row = 0; row < MAX_GRID_SIZE_Y; ++row)
                if (row != aGridY && CanPlantAt(aGridX, row, aPlantingSeedType) == PlantingReason::PLANTING_OK) ++plantCount;
        }
        else if (mApp->RunMirrorStacks() > 0 &&
            ((aGridY + 1 < MAX_GRID_SIZE_Y && CanPlantAt(aGridX, aGridY + 1, admissionSeed) == PlantingReason::PLANTING_OK) ||
             (aGridY > 0 && CanPlantAt(aGridX, aGridY - 1, admissionSeed) == PlantingReason::PLANTING_OK))) ++plantCount;
        if (!PreparePlantAnimations(admissionSeed, plantCount))
        {
            DisplayAdvice("\u8bf7\u7a0d\u7b49\u7247\u523b\uff0c\u518d\u6b21\u79cd\u690d\u3002", MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_NONE);
            return;
        }
    }


	// 无免费种植、非传送带关卡的卡槽植物，判断阳光是否充足：充足则扣除阳光，不足则退出
	if (!mApp->mEasyPlantingCheat && mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK && !HasConveyorBeltSeedBank())
	{
		if (!TakeSunMoney(GetCurrentPlantCost(aPlantingSeedType, SeedType::SEED_NONE)))
		{
			return;
		}
	}

	bool aKernelToCob = false;
	// @pvz-online: 批 18 玉米投手「Artillery」替换点——扣费之后（按玉米投手 100 扣）把种植类型
	// 整换成加农炮：后面的升级杀锚点、杀右邻格、落地 AddPlant 全按加农炮走，等同闯关里已有的
	// 直接种加农炮路径。只换卡槽种植——手套搬运也读游标类型，不能把搬玉米变成种加农炮。
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK &&
		aPlantingSeedType == SeedType::SEED_KERNELPULT && mApp->IsRunMode() &&
		mApp->RunPlantUpgradeCount(SeedType::SEED_KERNELPULT) > 0)
	{
		aPlantingSeedType = SeedType::SEED_COBCANNON;
		aKernelToCob = true;
	}

	// 升级种植或坚果包扎术等情况时，先将原植物销毁
	bool aIsAwake = false;
	int aWakeUpCounter = 0;
	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(aGridX, aGridY, &aPlantOnLawn);
	Plant* aNormalPlant = aPlantOnLawn.mNormalPlant;
	Plant* aPumpkinPlant = aPlantOnLawn.mPumpkinPlant;
	if (aNormalPlant && aNormalPlant->IsUpgradableTo(aPlantingSeedType))
	{
		if (aPlantingSeedType == SeedType::SEED_GLOOMSHROOM)
		{
			aIsAwake = !aNormalPlant->mIsAsleep;
			aWakeUpCounter = aNormalPlant->mWakeUpCounter;
		}
		aNormalPlant->Die();
	}
	if ((aPlantingSeedType == SeedType::SEED_WALLNUT || aPlantingSeedType == SeedType::SEED_TALLNUT) && aNormalPlant)
	{
		if (aNormalPlant->mSeedType == aPlantingSeedType)
		{
			aNormalPlant->Die();
		}
	}
	if (aPlantingSeedType == SeedType::SEED_PUMPKINSHELL && aPumpkinPlant)
	{
		if (aPumpkinPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			aPumpkinPlant->Die();
		}
	}
	if (aPlantingSeedType == SeedType::SEED_COBCANNON)
	{
		Plant* aRightPlant = GetTopPlantAt(aGridX + 1, aGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aRightPlant)
		{
			aRightPlant->Die();
		}
	}
	if (aPlantingSeedType == SeedType::SEED_CATTAIL)
	{
		if (aPlantOnLawn.mLilypadPlant)
		{
			aPlantOnLawn.mLilypadPlant->Die();
		}
		if (aNormalPlant)
		{
			aNormalPlant->Die();
		}
	}

	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE)
	{
		mApp->mZenGarden->MovePlant(mPlants.DataArrayTryToGet(mCursorObject->mGlovePlantID), aGridX, aGridY);
	}
	else if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW)
	{
		mApp->mZenGarden->MouseDownWithFullWheelBarrow(x, y);
	}
	else if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN)
	{
		if(TeamPlay::UseSupportCard(mApp,uint32_t(mCursorObject->mCoinID),aGridY,aGridX))return;
		AddPlant(aGridX, aGridY, mCursorObject->mType, mCursorObject->mImitaterType);
		Coin* aCoin = mCoins.DataArrayTryToGet(mCursorObject->mCoinID);
		mCursorObject->mCoinID = CoinID::COINID_NULL;
		aCoin->Die();
	}
	else if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK)
	{
		SeedType aSpawnType = mCursorObject->mType;
		SeedType aSpawnImitater = mCursorObject->mImitaterType;
		// @pvz-online: 批 18 玉米投手「Artillery」——换过加农炮时游标仍指着玉米（模仿者是
		// SEED_IMITATER），落地显式改种加农炮本体；模仿游标也种本体——没有加农炮的模仿版。
		if (aKernelToCob)
		{
			aSpawnType = SeedType::SEED_COBCANNON;
			aSpawnImitater = SeedType::SEED_NONE;
		}
		Plant* aPlant = AddPlant(aGridX, aGridY, aSpawnType, aSpawnImitater);
        if(aPlant&&mApp->GetRunState())TeamPlay::NoteSharedPlant(mApp,int(mCursorObject->mType==SEED_IMITATER&&!mApp->GetRunState()->mTeam.shared.Has(SEED_IMITATER)?mCursorObject->mImitaterType:mCursorObject->mType));
		if (aIsAwake)
		{
			aPlant->SetSleeping(false);
		}
		else
		{
			aPlant->mWakeUpCounter = aWakeUpCounter;
		}

		mSeedBank->mSeedPackets[mCursorObject->mSeedBankIndex].WasPlanted();

		// @pvz-online: 批 A「排山倒海」（2026-10-09）——种下后在上下相邻空格免费补种同款，
		// 只落 1 株：先试下格、被占/不可种再试上格，都不行就算了。免阳光、不刷 CD、
		// 不递归（本分支只由玩家点击进）。aSpawnType 用换装后的（Artillery 情境复制株
		// 也是加农炮）；睡眠态随主株。
		if (mApp->RunMirrorStacks() > 0)
		{
			int aMirrorRow = -1;
			if (aGridY + 1 < MAX_GRID_SIZE_Y && CanPlantAt(aGridX, aGridY + 1, aSpawnType) == PlantingReason::PLANTING_OK)
			{
				aMirrorRow = aGridY + 1;
			}
			else if (aGridY - 1 >= 0 && CanPlantAt(aGridX, aGridY - 1, aSpawnType) == PlantingReason::PLANTING_OK)
			{
				aMirrorRow = aGridY - 1;
			}
			if (aMirrorRow >= 0)
			{
				Plant* aMirrorPlant = AddPlant(aGridX, aMirrorRow, aSpawnType, aSpawnImitater);
				if (aIsAwake)
				{
					aMirrorPlant->SetSleeping(false);
				}
				else
				{
					aMirrorPlant->mWakeUpCounter = aWakeUpCounter;
				}
			}
		}
	}
	else
	{
		PVZP_ASSERT(false);
	}

	// 柱子关卡中，一列种植
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
	{
		for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
		{
			if (aRow == aGridY || CanPlantAt(aGridX, aRow, aPlantingSeedType) != PlantingReason::PLANTING_OK)
				continue;

			if (aPlantingSeedType == SeedType::SEED_WALLNUT || aPlantingSeedType == SeedType::SEED_TALLNUT)
			{
				aNormalPlant = GetTopPlantAt(aGridX, aRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
				if (aNormalPlant && aNormalPlant->mSeedType == aPlantingSeedType)
				{
					aNormalPlant->Die();
				}
			}
			if (aPlantingSeedType == SeedType::SEED_PUMPKINSHELL)
			{
				aPumpkinPlant = GetTopPlantAt(aGridX, aRow, PlantPriority::TOPPLANT_ONLY_PUMPKIN);
				if (aPumpkinPlant && aPumpkinPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
				{
					aPumpkinPlant->Die();
				}
			}
			AddPlant(aGridX, aRow, mCursorObject->mType, mCursorObject->mImitaterType);
			// @pvz-online: the run bowling upgrade has the same planting feedback as PC.
			if ((mCursorObject->mType == SeedType::SEED_WALLNUT || mCursorObject->mImitaterType == SeedType::SEED_WALLNUT) &&
				mApp->RunExtraBuffCount(RUN_BUFF2_NUT_BOWLING) > 0)
			{
				mApp->PlaySample(Sexy::SOUND_BOWLING);
			}
		}
	}

	// 设置教程状态相关
	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER)
	{
		SetTutorialState(mPlants.mSize >= 2 ? TutorialState::TUTORIAL_LEVEL_1_COMPLETED : TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER);
	}
	else if (mTutorialState == TutorialState::TUTORIAL_LEVEL_2_PLANT_SUNFLOWER)
	{
		int aSunFlowersCount = CountSunFlowers();
		if (aPlantingSeedType == SeedType::SEED_SUNFLOWER && aSunFlowersCount == 2)
		{
			DisplayAdvice(("[ADVICE_MORE_SUNFLOWERS]"), MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL2, AdviceType::ADVICE_NONE);
			if (!mSeedBank->mSeedPackets[1].CanPickUp())
			{
				SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER);
			}
			else
			{
				SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER);
			}
		}
		else if (aSunFlowersCount >= 3)
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_COMPLETED);
		}
		else if (!mSeedBank->mSeedPackets[1].CanPickUp())
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER);
		}
		else
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER);
		}
	}
	else if (mTutorialState == TutorialState::TUTORIAL_MORESUN_PLANT_SUNFLOWER)
	{
		if (CountSunFlowers() >= 3)
		{
			SetTutorialState(TutorialState::TUTORIAL_MORESUN_COMPLETED);
			DisplayAdvice(("[ADVICE_PLANT_SUNFLOWER5]"), MessageStyle::MESSAGE_STYLE_TUTORIAL_LATER, AdviceType::ADVICE_PLANT_SUNFLOWER5);
			mTutorialTimer = -1;
		}
		else if (!mSeedBank->mSeedPackets[1].CanPickUp())
		{
			SetTutorialState(TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER);
		}
		else
		{
			SetTutorialState(TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER);
		}
	}

	// 保龄球关卡，播放保龄球滚动的音效
	if (mApp->IsWallnutBowlingLevel())
	{
		mApp->PlaySample(Sexy::SOUND_BOWLING);
	}

	// 重置鼠标
	ClearCursor();
}

Plant* Board::ToolHitTestHelper(const HitResult* theHitResult)
{
	Plant* aPlant = (Plant*)theHitResult->mObject;
	return (aPlant->mSeedType != SeedType::SEED_GRAVEBUSTER || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN) ? aPlant : nullptr;
}

Plant* Board::ToolHitTest(int theX, int theY)
{
	HitResult aHitResult;
	MouseHitTest(theX, theY, &aHitResult);
	if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_PLANT)
	{
		return ToolHitTestHelper(&aHitResult);
	}
	return nullptr;
}

void Board::TutorialArrowShow(int theX, int theY)
{
	TutorialArrowRemove();
	PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(theX, theY, MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ParticleEffect::PARTICLE_SEED_PACKET_PICK);
	mTutorialParticleID = mApp->ParticleGetID(aParticle);
}

void Board::TutorialArrowRemove()
{
	mApp->RemoveParticle(mTutorialParticleID);
	mTutorialParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
}

void Board::MouseDownWithTool(int x, int y, int theClickCount, CursorType theCursorType)
{
    if(mApp->TeamProgressionInputBlocked())return;
	if (theClickCount < 0)
	{
		ClearCursor();
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		return;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		mApp->mZenGarden->MouseDownWithTool(x, y, theCursorType);
		return;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mChallenge->TreeOfWisdomTool(x, y);
		return;
	}

	Plant* aPlant = ToolHitTest(x, y);
	if (aPlant == nullptr)
	{
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
	}
	else if (theCursorType == CursorType::CURSOR_TYPE_SHOVEL)
	{
		mApp->PlayFoley(FoleyType::FOLEY_USE_SHOVEL);
		mPlantsShoveled++;
		aPlant->Die();

		if (aPlant->mSeedType == SeedType::SEED_CATTAIL && GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_PUMPKIN))
		{
			NewPlant(aPlant->mPlantCol, aPlant->mRow, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
		}
		if (mTutorialState == TutorialState::TUTORIAL_SHOVEL_DIG || mTutorialState == TutorialState::TUTORIAL_SHOVEL_KEEP_DIGGING)
		{
			SetTutorialState(CountPlantByType(SeedType::SEED_PEASHOOTER) == 0 ? TutorialState::TUTORIAL_SHOVEL_COMPLETED : TutorialState::TUTORIAL_SHOVEL_KEEP_DIGGING);
		}
	}

	ClearCursor();
}

Plant* Board::SpecialPlantHitTest(int x, int y)
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			float aMinDist = GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION) ? 25 : 0;
			float aDistance = Distance2D(x, y, aPlant->mX + 40, aPlant->mY + 40);
			if (aDistance >= aMinDist && aDistance <= 50 && y > aPlant->mY + 25)
			{
				return aPlant;
			}
		}
		else if (Plant::IsFlying(aPlant->mSeedType))
		{
			if (Distance2D(x, y, aPlant->mX + 40, aPlant->mY) < 15)
			{
				return aPlant;
			}
		}
	}
	return nullptr;
}

bool Board::MouseHitTestPlant(int x, int y, HitResult* theHitResult)
{
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_COBCANNON_TARGET || mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
		return false;

	Plant* aPlant;
	aPlant = SpecialPlantHitTest(x, y);
	if (aPlant)
	{
		theHitResult->mObject = aPlant;
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_PLANT;
		return true;
	}

	int aGridX = PixelToGridX(x, y);
	int aGridY = PixelToGridY(x, y);
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ZEN_TOOL_ORDER);
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WATERING_CAN && (!aPlant || !mApp->mZenGarden->PlantCanBeWatered(aPlant)))
		{
			Plant* aTopPlant = GetTopPlantAt(PixelToGridX(x - 30, y - 20), PixelToGridY(x - 30, y - 20), PlantPriority::TOPPLANT_ZEN_TOOL_ORDER);
			if (aTopPlant && mApp->mZenGarden->PlantCanBeWatered(aTopPlant))
			{
				aPlant = aTopPlant;
			}
		}
	}
	else
	{
		aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_DIGGING_ORDER);
		if (aPlant && (aPlant->mSeedType == SeedType::SEED_LILYPAD || aPlant->mSeedType == SeedType::SEED_FLOWERPOT))
		{
			if (GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_PUMPKIN))
			{
				return false;
			}
		}
	}

	if (aPlant == nullptr)
	{
		return false;
	}
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_CHOCOLATE && !mApp->mZenGarden->PlantCanHaveChocolate(aPlant))
	{
		theHitResult->mObject = nullptr;
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
		return false;
	}

	theHitResult->mObject = aPlant;
	theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_PLANT;
	return true;
}

bool Board::MouseHitTest(int x, int y, HitResult* theHitResult)
{
	if (mBoardFadeOutCounter >= 0 || IsScaryPotterDaveTalking())
	{
		theHitResult->mObject = nullptr;
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
		return false;
	}

	if (mMenuButton->IsMouseOver() && CanInteractWithBoardButtons())
	{
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_MENU_BUTTON;
		return true;
	}
	else if (mStoreButton && mStoreButton->IsMouseOver() && CanInteractWithBoardButtons())
	{
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_STORE_BUTTON;
		return true;
	}

	Rect aShovelButtonRect = GetShovelButtonRect();
	if (mSeedBank->MouseHitTest(x, y, theHitResult))
	{
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL ||
			mCursorObject->mCursorType == CursorType::CURSOR_TYPE_COBCANNON_TARGET ||
			mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
			return true;
	}
	if (mShowShovel && aShovelButtonRect.Contains(x, y) && CanInteractWithBoardButtons())
	{
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_SHOVEL;
		return true;
	}

	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL || mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
	{
		Coin* aTopCoin = nullptr;
		for (Coin* aCoin : mCoins)
		{
			if (aCoin->mDead)
				continue;
			HitResult aHitResultCoin;
			if (aCoin->MouseHitTest(x, y, &aHitResultCoin))
			{
				aCoin = (Coin*)aHitResultCoin.mObject;
				if (aTopCoin == nullptr || aCoin->mRenderOrder >= aTopCoin->mRenderOrder)
				{
					theHitResult->mObjectType = aHitResultCoin.mObjectType;
					theHitResult->mObject = aCoin;
					aTopCoin = aCoin;
				}
			}
		}
		if (aTopCoin)
		{
			return true;
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		bool canClick = false;
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_CHOCOLATE && !mApp->mZenGarden->IsStinkyHighOnChocolate())
		{
			canClick = true;
		}
		else if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL && mApp->mZenGarden->IsStinkySleeping())
		{
			canClick = true;
		}

		GridItem* aStinky = mApp->mZenGarden->GetStinky();
		if (canClick && aStinky)
		{
			Rect aStinkyRect(aStinky->mPosX - 6, aStinky->mPosY - 10, 84, 90);
			if (aStinkyRect.Contains(x, y))
			{
				theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_STINKY;
				return true;
			}
		}
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_TREE_FOOD && mChallenge->TreeOfWisdomHitTest(x, y, theHitResult))
		{
			return true;
		}
	}
	if ((mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM) && CanInteractWithBoardButtons())
	{
		for (int i = static_cast<int>(GameObjectType::OBJECT_TYPE_WATERING_CAN); i <= static_cast<int>(GameObjectType::OBJECT_TYPE_NEXT_GARDEN); i++)
		{
			GameObjectType aTool = static_cast<GameObjectType>(i);
			if (CanUseGameObject(aTool) && (aTool != GameObjectType::OBJECT_TYPE_TREE_FOOD || mChallenge->TreeOfWisdomCanFeed()))
			{
				Rect aButtonRect = GetShovelButtonRect();
				if (aTool == GameObjectType::OBJECT_TYPE_NEXT_GARDEN)
				{
					aButtonRect.mX = 564;
				}
				else
				{
					GetZenButtonRect(aTool, aButtonRect);
				}

				if (aButtonRect.Contains(x, y))
				{
					theHitResult->mObjectType = (GameObjectType)aTool;
					return true;
				}
			}
		}
	}

	if (MouseHitTestPlant(x, y, theHitResult))
		return true;

	if (mApp->IsScaryPotterLevel() &&
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL &&
		mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_SCARY_POTTER_MALLETING &&
		mApp->mGameScene == GameScenes::SCENE_PLAYING &&
		mApp->GetDialog(Dialogs::DIALOG_GAME_OVER) == nullptr &&
		mApp->GetDialog(Dialogs::DIALOG_CONTINUE) == nullptr)
	{
		GridItem* aScaryPot = GetGridItemAt(GridItemType::GRIDITEM_SCARY_POT, PixelToGridX(x, y), PixelToGridY(x, y));
		if (aScaryPot)
		{
			theHitResult->mObject = aScaryPot;
			theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_SCARY_POT;
			return true;
		}
	}

	if (mApp->IsSlotMachineLevel())
	{
		Rect aSlotMachineHandleRect = mChallenge->SlotMachineGetHandleRect();
		if (aSlotMachineHandleRect.Contains(x, y) && mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_NORMAL && !HasLevelAwardDropped())
		{
			theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_SLOT_MACHINE_HANDLE;
			return true;
		}
	}

	theHitResult->mObject = nullptr;
	theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
	return false;
}

void Board::PickUpTool(GameObjectType theObjectType)
{
	if (mPaused || (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mCutScene->IsInShovelTutorial()))
		return;

	switch (theObjectType)
	{
	case GameObjectType::OBJECT_TYPE_SHOVEL:
		if (mTutorialState == TutorialState::TUTORIAL_SHOVEL_PICKUP)
		{
			SetTutorialState(TutorialState::TUTORIAL_SHOVEL_DIG);
		}
		mCursorObject->mCursorType = CursorType::CURSOR_TYPE_SHOVEL;
		mApp->PlayFoley(FoleyType::FOLEY_SHOVEL);
		break;

	case GameObjectType::OBJECT_TYPE_WATERING_CAN:
		if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_PICKUP_WATER)
		{
			mTutorialState = TutorialState::TUTORIAL_ZEN_GARDEN_WATER_PLANT;
			DisplayAdvice("[ADVICE_ZEN_GARDEN_WATER_PLANT]", MessageStyle::MESSAGE_STYLE_ZEN_GARDEN_LONG, AdviceType::ADVICE_NONE);
			TutorialArrowRemove();
		}
		mCursorObject->mCursorType = CursorType::CURSOR_TYPE_WATERING_CAN;
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		break;

	case GameObjectType::OBJECT_TYPE_FERTILIZER:
		if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FERTILIZER] > PURCHASE_COUNT_OFFSET)
		{
			mCursorObject->mCursorType = CursorType::CURSOR_TYPE_FERTILIZER;
			mApp->PlayFoley(FoleyType::FOLEY_DROP);
		}
		else
		{
			mApp->PlaySample(Sexy::SOUND_BUZZER);
		}
		break;

	case GameObjectType::OBJECT_TYPE_BUG_SPRAY:
		if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_BUG_SPRAY] > PURCHASE_COUNT_OFFSET)
		{
			mCursorObject->mCursorType = CursorType::CURSOR_TYPE_BUG_SPRAY;
			mApp->PlayFoley(FoleyType::FOLEY_DROP);
		}
		else
		{
			mApp->PlaySample(Sexy::SOUND_BUZZER);
		}
		break;

	case GameObjectType::OBJECT_TYPE_PHONOGRAPH:
		mCursorObject->mCursorType = CursorType::CURSOR_TYPE_PHONOGRAPH;
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		break;

	case GameObjectType::OBJECT_TYPE_CHOCOLATE:
		if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_CHOCOLATE] > PURCHASE_COUNT_OFFSET)
		{
			mCursorObject->mCursorType = CursorType::CURSOR_TYPE_CHOCOLATE;
			mApp->PlayFoley(FoleyType::FOLEY_DROP);
		}
		else
		{
			mApp->PlaySample(Sexy::SOUND_BUZZER);
		}
		break;

	case GameObjectType::OBJECT_TYPE_GLOVE:
		mCursorObject->mCursorType = CursorType::CURSOR_TYPE_GLOVE;
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		break;

	case GameObjectType::OBJECT_TYPE_MONEY_SIGN:
		mCursorObject->mCursorType = CursorType::CURSOR_TYPE_MONEY_SIGN;
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		break;

	case GameObjectType::OBJECT_TYPE_WHEELBARROW:
		mCursorObject->mCursorType = CursorType::CURSOR_TYPE_WHEEELBARROW;
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		break;

	case GameObjectType::OBJECT_TYPE_TREE_FOOD:
		if (mChallenge->TreeOfWisdomCanFeed())
		{
			if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_TREE_FOOD] > PURCHASE_COUNT_OFFSET)
			{
				mCursorObject->mCursorType = CursorType::CURSOR_TYPE_TREE_FOOD;
				mApp->PlayFoley(FoleyType::FOLEY_DROP);
			}
			else
			{
				mApp->PlaySample(Sexy::SOUND_BUZZER);
			}
		}
		break;

	default:
		PVZP_ASSERT(false);
		break;
	}

	mCursorObject->mType = SeedType::SEED_NONE;
}

void Board::MouseDown(int x, int y, int theClickCount)
{
    if(mApp->TeamProgressionInputBlocked())return;
    if (mApp->mOnlineSession && mApp->mOnlineSession->RelayBattleHeld()) { mApp->DoOnlineDialog(); return; }
    if(AdmissionBoard::MouseDown(this,x,y,theClickCount))return;
    if (NextWaveControl::MouseDown(this, x, y, theClickCount)) return;
    if (DamageStatsView::MouseDown(this, x, y, theClickCount)) return;
    if (CoopMouseDown(x, y, theClickCount)) return;
#ifdef __ANDROID__
	mTouchSeedDrag = false;
	mTouchSeedDragMoved = false;
#endif
	UpdateMousePosition();
	Widget::MouseDown(x, y, theClickCount);
	mIgnoreMouseUp = !CanInteractWithBoardButtons();
	if (mTimeStopCounter > 0)
		return;

	HitResult aHitResult;
	MouseHitTest(x, y, &aHitResult);
    if (mDeferredPlantSetup.IsWaiting() && aHitResult.mObjectType != GameObjectType::OBJECT_TYPE_MENU_BUTTON) return;
	if (mChallenge->MouseDown(x, y, theClickCount, &aHitResult))
		return;

	if (mMenuButton->IsMouseOver() && CanInteractWithBoardButtons() && theClickCount > 0)
	{
		mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
	}
	else if (mStoreButton && mStoreButton->IsMouseOver() && CanInteractWithBoardButtons() && theClickCount > 0)
	{
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		{
			mApp->PlaySample(Sexy::SOUND_TAP);
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND || mApp->mGameMode == GameMode::GAMEMODE_UPSELL)
		{
			mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
		}
	}

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && mApp->mSeedChooserScreen)
	{
		mApp->mSeedChooserScreen->CancelLawnView();
	}
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		mCutScene->ZombieWonClick();
		return;
	}
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
	{
		mCutScene->MouseDown(x, y);
	}

	if (mApp->mCheatKeys && !mApp->IsScaryPotterLevel() && mNextSurvivalStageCounter > 0)
	{
		mNextSurvivalStageCounter = 2;
		for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
		{
			mIceTimer[i] = std::min(mIceTimer[i], 2);
		}
	}

	CursorType aCursor = mCursorObject->mCursorType;
	if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_NONE)
	{
		if (aCursor == CURSOR_TYPE_COBCANNON_TARGET)
		{
			MouseDownCobcannonFire(x, y, theClickCount);
			UpdateCursor();
			return;
		}
	}
	else if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_COIN && theClickCount >= 0)
	{
		Coin* aCoin = (Coin*)aHitResult.mObject;
		if (aCoin->mBoard)
		{
			aCoin->MouseDown(x, y, theClickCount);
		}
		UpdateCursor();
		return;
	}

	if (aCursor == CursorType::CURSOR_TYPE_SHOVEL ||
		aCursor == CursorType::CURSOR_TYPE_WATERING_CAN ||
		aCursor == CursorType::CURSOR_TYPE_FERTILIZER ||
		aCursor == CursorType::CURSOR_TYPE_BUG_SPRAY ||
		aCursor == CursorType::CURSOR_TYPE_PHONOGRAPH ||
		aCursor == CursorType::CURSOR_TYPE_CHOCOLATE ||
		aCursor == CursorType::CURSOR_TYPE_GLOVE ||
		aCursor == CursorType::CURSOR_TYPE_MONEY_SIGN ||
		aCursor == CursorType::CURSOR_TYPE_WHEEELBARROW ||
		aCursor == CursorType::CURSOR_TYPE_TREE_FOOD)
	{
		MouseDownWithTool(x, y, theClickCount, aCursor);
	}
	else if (IsPlantInCursor())
	{
		MouseDownWithPlant(x, y, theClickCount);
	}
	else
	{
		switch (aHitResult.mObjectType)
		{
		case GameObjectType::OBJECT_TYPE_SEEDPACKET:
			if (!mPaused)
			{
				((SeedPacket*)aHitResult.mObject)->MouseDown(x, y, theClickCount);
#ifdef __ANDROID__
				// @pvz-online: only start after the packet actually selected a plant (not on cooldown).
				if (theClickCount > 0 && mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK)
				{
					mTouchSeedDrag = true;
					mTouchSeedDragStartX = x;
					mTouchSeedDragStartY = y;
				}
#endif
			}
			break;
		case GameObjectType::OBJECT_TYPE_NEXT_GARDEN:
			if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
			{
				mApp->mZenGarden->GotoNextGarden();
			}
			else if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
			{
				mChallenge->TreeOfWisdomNextGarden();
			}
			mApp->PlaySample(Sexy::SOUND_TAP);
			break;
		case GameObjectType::OBJECT_TYPE_SHOVEL:
		case GameObjectType::OBJECT_TYPE_WATERING_CAN:
		case GameObjectType::OBJECT_TYPE_FERTILIZER:
		case GameObjectType::OBJECT_TYPE_BUG_SPRAY:
		case GameObjectType::OBJECT_TYPE_PHONOGRAPH:
		case GameObjectType::OBJECT_TYPE_CHOCOLATE:
		case GameObjectType::OBJECT_TYPE_GLOVE:
		case GameObjectType::OBJECT_TYPE_MONEY_SIGN:
		case GameObjectType::OBJECT_TYPE_WHEELBARROW:
		case GameObjectType::OBJECT_TYPE_TREE_FOOD:
			PickUpTool(aHitResult.mObjectType);
			break;
		case GameObjectType::OBJECT_TYPE_PLANT:
			((Plant*)aHitResult.mObject)->MouseDown(x, y, theClickCount);
			break;
		default:
			break;
		}
	}

	UpdateCursor();
}

void Board::ClearCursor()
{
    if (mApp && mApp->mEffectSystem && mApp->mEffectSystem->mReanimationHolder)
        mApp->mEffectSystem->mReanimationHolder->mPlantReservation = 4;
#ifdef __ANDROID__
	mTouchSeedDrag = false;
	mTouchSeedDragMoved = false;
#endif
	if (mAdvice->mDuration > 0)
	{
		if (mHelpIndex == AdviceType::ADVICE_PLANT_GRAVEBUSTERS_ON_GRAVES ||
			mHelpIndex == AdviceType::ADVICE_PLANT_LILYPAD_ON_WATER ||
			mHelpIndex == AdviceType::ADVICE_PLANT_TANGLEKELP_ON_WATER ||
			mHelpIndex == AdviceType::ADVICE_PLANT_SEASHROOM_ON_WATER ||
			mHelpIndex == AdviceType::ADVICE_PLANT_POTATOE_MINE_ON_LILY ||
			mHelpIndex == AdviceType::ADVICE_PLANT_WRONG_ART_TYPE ||
			mHelpIndex == AdviceType::ADVICE_PLANT_NEED_POT ||
			mHelpIndex == AdviceType::ADVICE_PLANT_NOT_PASSED_LINE ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_REPEATERS ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_MELONPULT ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_SUNFLOWER ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_SPIKEWEED ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_KERNELPULT)
		{
			ClearAdvice(mHelpIndex);
		}
	}

	mCursorObject->mType = SeedType::SEED_NONE;
	mCursorObject->mCursorType = CursorType::CURSOR_TYPE_NORMAL;
	mCursorObject->mSeedBankIndex = -1;
	mCursorObject->mCoinID = CoinID::COINID_NULL;
	mCursorObject->mDuplicatorPlantID = PlantID::PLANTID_NULL;
	mCursorObject->mCobCannonPlantID = PlantID::PLANTID_NULL;
	mCursorObject->mGlovePlantID = PlantID::PLANTID_NULL;
	mApp->SetCursor(CURSOR_POINTER);
	mChallenge->ClearCursor();

	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER)
	{
		SetTutorialState(TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER);
	}
	else if (mTutorialState == TutorialState::TUTORIAL_LEVEL_2_PLANT_SUNFLOWER || mTutorialState == TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER)
	{
		if (!mSeedBank->mSeedPackets[1].CanPickUp())
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER);
		}
		else
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER);
		}
	}
	else if (mTutorialState == TutorialState::TUTORIAL_MORESUN_PLANT_SUNFLOWER || mTutorialState == TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER)
	{
		if (!mSeedBank->mSeedPackets[1].CanPickUp())
		{
			SetTutorialState(TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER);
		}
		else
		{
			SetTutorialState(TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER);
		}
	}
	else if (mTutorialState == TutorialState::TUTORIAL_SHOVEL_DIG)
	{
		SetTutorialState(TutorialState::TUTORIAL_SHOVEL_PICKUP);
	}
}

bool Board::CanInteractWithBoardButtons()
{
    if(mApp->TeamProgressionInputBlocked())return false;
	// during the process of obtaining a reward, should not interaction with the menu.
	if (mBoardFadeOutCounter >= 0)
		return false;

	if (mPaused || mApp->GetDialogCount() > 0)
		return false;

	if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_NORMAL &&
		mCursorObject->mCursorType != CursorType::CURSOR_TYPE_HAMMER &&
		mCursorObject->mCursorType != CursorType::CURSOR_TYPE_COBCANNON_TARGET)
		return false;

	if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
		return false;

	return mApp->mGameMode == GameMode::GAMEMODE_UPSELL || mApp->mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_OFF;
}

void Board::MouseUp(int x, int y, int theClickCount)
{
    if(mApp->TeamProgressionInputBlocked())return;
    if (mApp->mOnlineSession && mApp->mOnlineSession->RelayBattleHeld()) return;
    if(AdmissionBoard::MouseUp(this,x,y,theClickCount))return;
    if (NextWaveControl::MouseUp(this, x, y, theClickCount)) return;
    if (DamageStatsView::MouseUp(this, x, y, theClickCount)) return;
    if (CoopMouseUp(x, y, theClickCount)) return;
	Widget::MouseUp(x, y, theClickCount);
#ifdef __ANDROID__
	const bool aDropSeed = mTouchSeedDrag && theClickCount > 0 &&
		(mTouchSeedDragMoved || std::abs(x - mTouchSeedDragStartX) >= 8 || std::abs(y - mTouchSeedDragStartY) >= 8);
	mTouchSeedDrag = false;
	mTouchSeedDragMoved = false;
	if (aDropSeed)
	{
		mIgnoreMouseUp = false;
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK)
		{
			// @pvz-online: reuse planting rules on release; interrupted/invalid drops return the seed.
			if (!mPaused && !mApp->mMinimized && mApp->mGameScene == GameScenes::SCENE_PLAYING &&
				mApp->GetDialogCount() == 0 && mTimeStopCounter <= 0)
			{
				MouseDownWithPlant(x, y, 1);
			}
			if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK)
			{
				RefreshSeedPacketFromCursor();
			}
			UpdateCursor();
		}
		return;
	}
#endif
	if (mIgnoreMouseUp)
	{
		mIgnoreMouseUp = false;
		return;
	}

    if (mDeferredPlantSetup.IsWaiting() && !mMenuButton->IsMouseOver()) return;
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED && mChallenge->MouseUp(x, y) && theClickCount > 0)
		return;

	if (CanInteractWithBoardButtons() && theClickCount > 0)
	{
		if (mMenuButton->IsMouseOver() && !mApp->GetDialog(Dialogs::DIALOG_GAME_OVER) && !mApp->GetDialog(Dialogs::DIALOG_LEVEL_COMPLETE))
		{
			mMenuButton->mIsOver = false;
			mMenuButton->mIsDown = false;
			UpdateCursor();
			ClearCursor();
			if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_COMPLETED)
			{
				mApp->FinishZenGardenToturial();
			}
			else if (mApp->mGameMode != GAMEMODE_CHALLENGE_ZEN_GARDEN && mApp->mGameMode != GAMEMODE_TREE_OF_WISDOM && mApp->mGameMode != GAMEMODE_UPSELL)
			{
				mApp->PlaySample(Sexy::SOUND_PAUSE);
				mApp->DoNewOptions(false);
			}
			else
			{
				mApp->mBoardResult = BoardResult::BOARDRESULT_QUIT;
				mApp->DoBackToMain();
			}
		}
		else if(mStoreButton && mStoreButton->IsMouseOver())
		{
			if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
			{
				ClearAdviceImmediately();
				mApp->mZenGarden->OpenStore();
			}
			else if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
			{
				mChallenge->TreeOfWisdomOpenStore();
			}
			else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
			{
				mChallenge->mChallengeState = ChallengeState::STATECHALLENGE_LAST_STAND_ONSLAUGHT;
				mStoreButton->mBtnNoDraw = true;
				mStoreButton->mDisabled = true;
				mZombieCountDown = 10;
				mZombieCountDownStart = 10;
			}
			else if (mApp->mGameMode == GameMode::GAMEMODE_UPSELL)
			{
				mApp->DoBackToMain();
			}
		}
	}
}

void Board::ShowCoinBank(int theDuration)
{
	mCoinBankFadeCount = theDuration;
}

void Board::Pause(bool thePause)
{
    if(thePause) { NextWaveControl::Update(this);NextWaveControl::Suspend(this); }
	if (mPaused == thePause)
		return;

	mPaused = thePause;
	if (thePause && mApp->mPlayerInfo->mCoins > 0)
	{
		ShowCoinBank();
	}

	if (!thePause || mApp->mGameScene != GameScenes::SCENE_LEVEL_INTRO)
	{
		mApp->mSoundSystem->GamePause(thePause);
		mApp->mMusic->GameMusicPause(thePause);
	}

	if (thePause && (NeedSaveGame() || (mApp->GetRunState() && mApp->GetRunState()->IsNewEndless())))
	{
		TryToSaveGame();
	}
}

int Board::GetGraveStonesCount()
{
	int aCount = 0;

	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE)
		{
			aCount++;
		}
	}

	return aCount;
}

void Board::PickSpecialGraveStone()
{
	GridItem* aPicks[MAX_GRAVE_STONES];
	int aPickCount = 0;
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE)
		{
			PVZP_ASSERT(aPickCount < MAX_GRAVE_STONES);
			aPicks[aPickCount] = aGridItem;
			aPickCount++;
		}
	}

	if (aPickCount > 0)
	{
		PvzpPickFromArray(aPicks, aPickCount)->mGridItemState = GridItemState::GRIDITEM_STATE_GRAVESTONE_SPECIAL;
	}
}

void Board::SpawnZombiesFromPool()
{
	if (mIceTrapCounter > 0)
		return;

	int aCount, aZombiePoints;
	if (mLevel == 21 || mLevel == 22 || mLevel == 31 || mLevel == 32)
	{
		aCount = 2;
		aZombiePoints = 3;
	}
	else if (mLevel == 23 || mLevel == 24 || mLevel == 25 || mLevel == 33 || mLevel == 34 || mLevel == 35)
	{
		aCount = 3;
		aZombiePoints = 5;
	}
	else
	{
		aCount = 3;
		aZombiePoints = 7;
	}

	int aGridArrayCount = 0;
	PvzpWeightedGridArray aGridArray[MAX_POOL_GRID_SIZE];
	for (int aGridX = 5; aGridX < MAX_GRID_SIZE_X; aGridX++)
	{
		for (int aGridY = 2; aGridY <= 3; aGridY++)
		{
			aGridArray[aGridArrayCount].mX = aGridX;
			aGridArray[aGridArrayCount].mY = aGridY;
			aGridArray[aGridArrayCount].mWeight = 10000;
			aGridArrayCount++;
			PVZP_ASSERT(aGridArrayCount <= MAX_POOL_GRID_SIZE);
		}
	}

	aGridArrayCount = std::max(aGridArrayCount, 0);
	for (int i = 0; i < aCount; i++)
	{
		PvzpWeightedGridArray* aGrid = PvzpPickFromWeightedGridArray(aGridArray, aGridArrayCount);
		aGrid->mWeight = 0;

		ZombieType aZombieType = PickGraveRisingZombieType();
		mDeferredZombieSpawns.pending.push_back({static_cast<int32_t>(aZombieType), aGrid->mX, aGrid->mY, mCurrentWave, 0});

		aZombiePoints -= GetZombieDefinition(aZombieType).mZombieValue;
		aZombiePoints = std::max(aZombiePoints, 1);
	}
}

void Board::SetupBungeeDrop(BungeeDropGrid* theBungeeDropGrid)
{
	theBungeeDropGrid->mGridArrayCount = 0;
	for (int aGridX = 4; aGridX < MAX_GRID_SIZE_X; aGridX++)
	{
		for (int aGridY = 0; aGridY <= 4; aGridY++)
		{
			int aCount = theBungeeDropGrid->mGridArrayCount;
			theBungeeDropGrid->mGridArray[aCount].mX = aGridX;
			theBungeeDropGrid->mGridArray[aCount].mY = aGridY;
			theBungeeDropGrid->mGridArray[aCount].mWeight = 10000;
			theBungeeDropGrid->mGridArrayCount++;
			PVZP_ASSERT(static_cast<size_t>(theBungeeDropGrid->mGridArrayCount) <= LENGTH(theBungeeDropGrid->mGridArray));
		}
	}
}

bool Board::BungeeDropZombie(BungeeDropGrid* theBungeeDropGrid, ZombieType theZombieType)
{
	// A carrier and its payload must both fit before either is spawned.
	const unsigned int aRequired = theZombieType == ZombieType::ZOMBIE_BOBSLED ? 5U : 2U;
	if (mZombies.mSize >= mZombies.mMaxSize || aRequired >= mZombies.mMaxSize - mZombies.mSize)
		return false;

    if (!mApp->mEffectSystem->mReanimationHolder->mReanimations.DataArrayCanAllocate(
        AnimationAdmission::ZombieGroup(theZombieType,mCurrentWave)+AnimationAdmission::ZombieOne(ZombieType::ZOMBIE_BUNGEE) + mApp->mEffectSystem->mReanimationHolder->mPlantReservation)) return false;

	PvzpWeightedGridArray* aGrid = PvzpPickFromWeightedGridArray(theBungeeDropGrid->mGridArray, theBungeeDropGrid->mGridArrayCount);
	aGrid->mWeight = 1;

	Zombie* aBungeeZombie = AddZombie(ZombieType::ZOMBIE_BUNGEE, mCurrentWave);
	if (aBungeeZombie == nullptr) return false;
	Zombie* aZombie = AddZombie(theZombieType, mCurrentWave);
	if (aZombie == nullptr)
	{
		aBungeeZombie->DieNoLoot();
		return false;
	}

	aBungeeZombie->BungeeDropZombie(aZombie, aGrid->mX, aGrid->mY);
    return true;
}

void Board::SpawnZombiesFromSky()
{
	if (mIceTrapCounter > 0)
		return;

	int aCount, aZombiePoints;
	if (mLevel == 41 || mLevel == 42)
	{
		aCount = 2;
		aZombiePoints = 3;
	}
	else if (mLevel == 43 || mLevel == 44 || mLevel == 45)
	{
		aCount = 3;
		aZombiePoints = 5;
	}
	else
	{
		aCount = 3;
		aZombiePoints = 7;
	}

	BungeeDropGrid aBungeeDropGrid;
	SetupBungeeDrop(&aBungeeDropGrid);
	aCount = std::min(aCount, aBungeeDropGrid.mGridArrayCount);

	if (aBungeeDropGrid.mGridArrayCount == 0 || aCount <= 0)
		return;

	for (int i = 0; i < aCount; i++)
	{
		ZombieType aZombieType = PickGraveRisingZombieType();
		auto* target = PvzpPickFromWeightedGridArray(aBungeeDropGrid.mGridArray, aBungeeDropGrid.mGridArrayCount);
		target->mWeight = 1;
		mDeferredZombieSpawns.pending.push_back({static_cast<int32_t>(aZombieType), target->mX, target->mY, mCurrentWave, 1});
		aZombiePoints -= GetZombieDefinition(aZombieType).mZombieValue;
		aZombiePoints = std::max(aZombiePoints, 1);
	}
}

void Board::DrainDeferredZombieSpawns()
{
    auto& state = mDeferredZombieSpawns;
    while (state.HasPending())
    {
        const auto& ticket = state.pending[state.next];
        const auto type = static_cast<ZombieType>(ticket.type);
        Zombie* zombie = nullptr;
        if (ticket.kind == 1)
        {
            // Bungee carrier plus payload, and one unused zombie slot kept by AddZombie.
            auto& holder = mApp->mEffectSystem->mReanimationHolder;
            if (!mZombies.DataArrayCanAllocate(3) ||
                !holder->mReanimations.DataArrayCanAllocate(2 + holder->mPlantReservation)) break;
            Zombie* carrier = AddZombie(ZombieType::ZOMBIE_BUNGEE, ticket.wave);
            if (!carrier) break;
            zombie = AddZombie(type, ticket.wave);
            if (!zombie) { carrier->DieNoLoot(); break; }
            carrier->BungeeDropZombie(zombie, ticket.x, ticket.y);
        }
        else
        {
            zombie = ticket.kind == 0 ? AddZombieInRow(type, ticket.y, ticket.wave) : AddZombie(type, ticket.wave);
            if (!zombie) break;
            zombie->RiseFromGrave(ticket.x, ticket.y);
        }
        ++state.next;
    }
    if (state.HasPending()) mRiseFromGraveCounter = 1;
    else { state.Clear(); mRiseFromGraveCounter = 0; }
}

void Board::SpawnZombiesFromGraves()
{
    if (mDeferredZombieSpawns.HasPending()) { DrainDeferredZombieSpawns(); return; }
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS_2)
		return;

	if (StageHasRoof())
	{
		SpawnZombiesFromSky();
	}
	else if (StageHasPool())
	{
		SpawnZombiesFromPool();
		DrainDeferredZombieSpawns();
		return;
	}

//	int aZombiePoints = GetGraveStonesCount();
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType != GridItemType::GRIDITEM_GRAVESTONE || aGridItem->mGridItemCounter < 100)
		{
			continue;
		}
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_GRAVE_DANGER && Rand(mNumWaves) > mCurrentWave)
		{
			continue;
		}

		ZombieType aZombieType = PickGraveRisingZombieType();
		mDeferredZombieSpawns.pending.push_back({static_cast<int32_t>(aZombieType), aGridItem->mGridX, aGridItem->mGridY, mCurrentWave, 2});

		/*
		aZombiePoints -= GetZombieDefinition(aZombieType).mZombieValue;
		if (aZombieType < 1)
		{
			aZombiePoints = 1;
		}
		*/
	}

    DrainDeferredZombieSpawns();
}

int64_t Board::TotalZombiesHealthInWave(int theWaveIndex)
{
	int64_t aTotalHealth = 0;
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mFromWave == theWaveIndex && !aZombie->mMindControlled && !aZombie->IsDeadOrDying() &&
			aZombie->mZombieType != ZombieType::ZOMBIE_BUNGEE && aZombie->mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
		{
			// Keep body/helmet/flying at full weight and shield at 20%, truncating
            // once per zombie. Widen before addition; never round the running sum to float.
            const int64_t aWholeHealth = static_cast<int64_t>(aZombie->mBodyHealth) +
                aZombie->mHelmHealth + aZombie->mFlyingHealth;
            aTotalHealth += (aWholeHealth * 5 + aZombie->mShieldHealth) / 5;
		}
	}
	return aTotalHealth;
}

void Board::SpawnZombieWave()
{
    if(mWaveSpawnRetry.wave!=mCurrentWave) {
        mChallenge->SpawnZombieWave();
        mWaveSpawnRetry={mCurrentWave,0,0};
    }
	if (mApp->IsBungeeBlitzLevel())
	{
		BungeeDropGrid aBungeeDropGrid;
		SetupBungeeDrop(&aBungeeDropGrid);
		for (int& i = mWaveSpawnRetry.index; i < MAX_ZOMBIES_IN_WAVE; i++)
		{
			ZombieType aZombieType = mZombiesInWave[mCurrentWave][i];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
				break;

			if (aZombieType == ZombieType::ZOMBIE_BUNGEE || aZombieType == ZombieType::ZOMBIE_ZAMBONI)
			{
				if (!AddZombie(aZombieType, mCurrentWave)) return;
			}
			else
			{
				if (!BungeeDropZombie(&aBungeeDropGrid, aZombieType)) return;
			}
		}
	}
	else
	{
		PVZP_ASSERT(mCurrentWave >= 0 && mCurrentWave < MAX_ZOMBIE_WAVES && mCurrentWave < mNumWaves);
		for (int& i = mWaveSpawnRetry.index; i < MAX_ZOMBIES_IN_WAVE; i++)
		{
			ZombieType aZombieType = mZombiesInWave[mCurrentWave][i];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
				break;

			if (aZombieType == ZombieType::ZOMBIE_BOBSLED && (mWaveSpawnRetry.part>0 || !CanAddBobSled()))
			{
				for (int& part = mWaveSpawnRetry.part; part < MAX_ZOMBIE_FOLLOWERS; part++)
				{
					if (!AddZombie(ZombieType::ZOMBIE_NORMAL, mCurrentWave)) return;  // spawn 4 normal zombies instead of the bobsled team
				}
				mWaveSpawnRetry.part=0;
			}
			else
			{
				if (!AddZombie(aZombieType, mCurrentWave)) return;
			}
		}
	}

	if (mCurrentWave == mNumWaves - 1 && !mApp->IsContinuousChallenge())
	{
		mRiseFromGraveCounter = 200;
	}
	if (IsFlagWave(mCurrentWave))
	{
		mFlagRaiseCounter = FLAG_RAISE_TIME;
		if (!SpawnRunBoss()) return;
	}
	mWaveSpawnRetry={};
	mCurrentWave++;
	mTotalSpawnedWaves++;
}

void Board::UpdateGameObjects()
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		aPlant->Update();
	}

	{
		FrostArmorBatch::Scope aFrostPass(this);
		for (Zombie* aZombie : mZombies)
		{
			if (aZombie->mDead) continue;
			aZombie->Update();
			aFrostPass.AfterUpdate(aZombie);
		}
	}

	for (Projectile* aProjectile : mProjectiles)
	{
		if (aProjectile->mDead)
			continue;
		aProjectile->Update();
	}

	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead)
			continue;
		aCoin->Update();
	}

	for (LawnMower* aLawnMower : mLawnMowers)
	{
		if (aLawnMower->mDead)
			continue;
		aLawnMower->Update();
	}

	mCursorPreview->Update();
	mCursorObject->Update();

	for (int i = 0; i < mSeedBank->mNumPackets; i++)
	{
		mSeedBank->mSeedPackets[i].Update();
	}
    RunSpeciesRuntime::Drain(this);
}

void Board::StopAllZombieSounds()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		aZombie->StopZombieSound();
	}
}

int Board::GetSurvivalFlagsCompleted()
{
	int aWavesPerFlag = GetNumWavesPerFlag();
	int aFlagsCompleted = mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage() / aWavesPerFlag;
	int aCurrentWave = mCurrentWave;
	if (IsFlagWave(aCurrentWave - 1) && mBoardFadeOutCounter < 0 && !mNextSurvivalStageCounter)
	{
		aCurrentWave -= 1;
	}
	return aCurrentWave / aWavesPerFlag + aFlagsCompleted;
}

void Board::SurvivalSaveScore()
{
	if (!mApp->IsSurvivalMode())
		return;

	uint32_t aFlagsCompleted = GetSurvivalFlagsCompleted();
	uint32_t& aFlagsRecord = mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()];
	if (aFlagsCompleted > aFlagsRecord)
	{
		aFlagsRecord = aFlagsCompleted;
		mApp->WriteCurrentUserConfig();
	}
}

void Board::PuzzleSaveStreak()
{
	if (!mApp->IsEndlessIZombie(mApp->mGameMode) && !mApp->IsEndlessScaryPotter(mApp->mGameMode))
		return;

	uint32_t aStreak = mChallenge->mSurvivalStage + 1;
	uint32_t& aRecord = mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()];
	if (aStreak > aRecord)
	{
		aRecord = aStreak;
		mApp->WriteCurrentUserConfig();
	}
}

void Board::ZombiesWon(Zombie* theZombie, bool theFromPeer)
{
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
		return;

	// @pvz-online: theFromPeer = 队友那边先丢的草坪，这条是"照着演一遍"的命令。下面两件事
	// 都要绕开：不许把 GAME_OVER 再回发一遍（回声），也不用问"手上还有没有队友可传"——
	// 全队败是既成事实，两台机器演的是同一件事（见 LawnApp::UpdateOnlineEnd）。
	if (!theFromPeer)
	{
		// @pvz-online: 联机局里的漏怪一律先走传递（见 Zombie::CheckForBoardEdge），
		// 传不成才落到这儿——那只有末席。万一别处绕过传递直接调进来，手上还有队友可传
		// 也不能认输：认了就是把队友也一起判了。
		if (mApp->IsOnlineGame() && mApp->mOnlineSession->GetRelayTargetSeat() != NetProto::SEAT_UNSET)
			return;

		// @pvz-online: 闯关（R4）输一关记一笔失败（进检查点，首版只存不用）。
		// mBoardResult 已经是 LOST 说明这一败记过账了，不再重复。
		if (mApp->IsRunMode() && mApp->mBoardResult != BoardResult::BOARDRESULT_LOST)
		{
			mApp->NoteRunFailure();
		}

		// @pvz-online: 到了这儿就是全队败——最后一名席位漏怪，没有下一家可传。输的不是
		// "谁漏谁出局"，是所有人，所以先把话告诉队友（他们收到也跟着把这一局演完，
		// 演完只有主机点得动重开，见 LawnApp::RetryOnlineLevel），本机再照原版把"房子被吃"演完。
		// mBoardResult 已经输了就不用再报一遍。
		if (mApp->IsOnlineGame() && mApp->mBoardResult != BoardResult::BOARDRESULT_LOST)
		{
			mApp->mOnlineSession->SendGameOver(NetProto::GAMEOVER_ZOMBIES_WON);
		}
	}

	ClearAdvice(AdviceType::ADVICE_NONE);
	mApp->mBoardResult = BoardResult::BOARDRESULT_LOST;

	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (aZombie == theZombie)
			continue;

		if (aZombie->GetZombieRect().mX < -50 ||
			aZombie->mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
			aZombie->mZombiePhase == ZombiePhase::PHASE_DANCER_RISING)
		{
			if ((aZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR) &&
				aZombie->IsDeadOrDying() && aZombie->mPosX < 140)
			{
				aZombie->DieNoLoot();
			}
		}
	}
	SurvivalSaveScore();

	std::string aGameOverMsg;
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM)
	{
		aGameOverMsg = ("[ZOMBIQUARIUM_DEATH_MESSAGE]");
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		std::string aFlagStr = mApp->Pluralize(GetSurvivalFlagsCompleted(), ("[ONE_FLAG]"), ("[COUNT_FLAGS]"));
		aGameOverMsg = PvzpReplaceString(("[LAST_STAND_DEATH_MESSAGE]"), ("{FLAGS}"), aFlagStr);
	}
	else if (mApp->IsEndlessIZombie(mApp->mGameMode) || mApp->IsEndlessScaryPotter(mApp->mGameMode))
	{
		aGameOverMsg = PvzpReplaceNumberString(("[ENDLESS_PUZZLE_DEATH_MESSAGE]"), ("{STREAK}"), mChallenge->mSurvivalStage);
	}
	else if (mApp->IsIZombieLevel())
	{
		aGameOverMsg = ("[I_ZOMBIE_DEATH_MESSAGE]");
	}
	else
	{
		mApp->mGameScene = GameScenes::SCENE_ZOMBIES_WON;
		if (theZombie)  // 原版此处没有对 theZombie 进行空指针判断，但加上判断后便允许绕过僵尸而直接调用游戏失败
		{
			theZombie->WalkIntoHouse();
		}

		ClearAdvice(AdviceType::ADVICE_NONE);
		mCutScene->StartZombiesWon();
		FreezeEffectsForCutscene(true);
		TutorialArrowRemove();
		UpdateCursor();
		return;
	}

	GameOverDialog* aGameOverDialog = new GameOverDialog(aGameOverMsg, true);
	mApp->AddDialog(Dialogs::DIALOG_GAME_OVER, aGameOverDialog);

	mApp->mMusic->StopAllMusic();
	StopAllZombieSounds();
	mApp->PlaySample(Sexy::SOUND_LOSEMUSIC);

	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIES_WON, true);
	Reanimation* aReanim = mApp->AddReanimation(-BOARD_OFFSET, 0, MakeRenderOrder(RenderLayer::RENDER_LAYER_SCREEN_FADE, 0, 0), ReanimationType::REANIM_ZOMBIES_WON);
	if (aReanim)
	{
    	aReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
    	aReanim->GetTrackInstanceByName("fullscreen")->mTrackColor = Color::Black;
    	aReanim->SetFramesForLayer("anim_screen");
	}
}

bool Board::IsFinalScaryPotterStage()
{
	if (!mApp->IsScaryPotterLevel())
		return false;

	if (mApp->IsAdventureMode())
	{
		return mChallenge->mSurvivalStage == 2;
	}

	return !mApp->IsEndlessScaryPotter(mApp->mGameMode);
}

bool Board::IsFinalSurvivalStage()
{
	if (!mApp->IsSurvivalMode())
		return false;

	int aFlags = GetNumWavesPerSurvivalStage() * (mChallenge->mSurvivalStage + 1) / GetNumWavesPerFlag();
	if (mApp->IsSurvivalNormal(mApp->mGameMode))
	{
		return aFlags >= 5;
	}
	if (mApp->IsSurvivalHard(mApp->mGameMode))
	{
		return aFlags >= 10;
	}

	return false;
}

bool Board::IsLastStandFinalStage()
{
	return mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND && mChallenge->mSurvivalStage == LAST_STAND_FLAGS - 1;
}

bool Board::IsSurvivalStageWithRepick()
{
	return mApp->IsSurvivalMode() && !IsFinalSurvivalStage();
}

bool Board::IsLastStandStageWithRepick()
{
	return mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND && !IsLastStandFinalStage();
}

bool Board::HasLevelAwardDropped()
{
	return mLevelAwardSpawned || mNextSurvivalStageCounter > 0 || mBoardFadeOutCounter >= 0;
}

void Board::UpdateSunSpawning()
{
	if ((StageIsNight() && !mApp->RunSkySunAtNight()) ||
		(HasLevelAwardDropped() && !OnlineWaitingForTeam()) ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_UPSELL ||
		mApp->mGameMode == GameMode::GAMEMODE_INTRO ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND ||
		mApp->IsIZombieLevel() ||
		mApp->IsScaryPotterLevel() ||
		mApp->IsSquirrelLevel() ||
		HasConveyorBeltSeedBank() ||
		mTutorialState == TutorialState::TUTORIAL_SLOT_MACHINE_PULL)
		return;

	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER || mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER)
	{
		if (mPlants.mSize == 0)
		{
			return;
		}
	}

	// @pvz-online: 夜晚先建草坪后拿「天降」时计时仍为0；旧档也可能已减成负数。
	// 非正值都视为到期，避免0减成-1后永远错过掉落，并让旧档自动恢复。
	if (mSunCountDown > 0)
		mSunCountDown--;
	if (mSunCountDown > 0)
		return;

	mNumSunsFallen++;
	// @pvz-online: 「天降」间隔减半；旧档多层仍只生效一次，保留原来的掉落速率。
	mSunCountDown = (int)((std::min(SUN_COUNTDOWN_MAX, SUN_COUNTDOWN + mNumSunsFallen * 10) + Rand(SUN_COUNTDOWN_RANGE)) * mApp->RunSkySunIntervalMul() + 0.5f);
	CoinType aSunType = mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_SUNNY_DAY ? CoinType::COIN_LARGESUN : CoinType::COIN_SUN;
	AddCoin(RandRangeInt(100, 649), 60, aSunType, CoinMotion::COIN_MOTION_FROM_SKY);
}

void Board::NextWaveComing()
{
	if (mCurrentWave + 1 == mNumWaves)
	{
		if (!IsSurvivalStageWithRepick() && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_LAST_STAND && !mApp->IsContinuousChallenge())
		{
			mApp->AddReanimation(0, 30, MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0), ReanimationType::REANIM_FINAL_WAVE);
			mFinalWaveSoundCounter = 60;
		}
	}

	if (mCurrentWave == 0)
	{
		mApp->PlaySample(Sexy::SOUND_AWOOGA);
	}
	else if (mApp->IsWhackAZombieLevel() ? (mCurrentWave == mNumWaves - 1) : IsFlagWave(mCurrentWave))
	{
		mApp->PlaySample(Sexy::SOUND_SIREN);
	}
}

void Board::UpdateZombieSpawning()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_UPSELL || mApp->mGameMode == GameMode::GAMEMODE_INTRO)
		return;

	if (mFinalWaveSoundCounter > 0)
	{
		mFinalWaveSoundCounter--;
		if (mFinalWaveSoundCounter == 0)
		{
			mApp->PlaySample(Sexy::SOUND_FINALWAVE);
		}
	}

	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER ||
		mTutorialState == TutorialState::TUTORIAL_SLOT_MACHINE_PULL)
		return;

	if (HasLevelAwardDropped())
		return;

	if (mRiseFromGraveCounter > 0)
	{
		mRiseFromGraveCounter--;
		if (mRiseFromGraveCounter == 0)
		{
			SpawnZombiesFromGraves();
		}
	}

	// A manual request enters the same wave spawn and post-spawn bookkeeping as the timer.
    if (NextWaveControl::TakeRequest(this))
    {
        if(IsFlagWave(mCurrentWave) && mApp->mMusic)mApp->mMusic->StartBurst();
        mHugeWaveCountDown=0;
        ClearAdvice(AdviceType::ADVICE_HUGE_WAVE);
        NextWaveComing();
        mZombieCountDown=1;
    }

    if (mHugeWaveCountDown > 0)
	{
		mHugeWaveCountDown--;
		if (mHugeWaveCountDown == 0)
		{
			ClearAdvice(AdviceType::ADVICE_HUGE_WAVE);
			NextWaveComing();
			mZombieCountDown = 1;
		}
		else
		{
			if (mHugeWaveCountDown == 725)
			{
				mApp->PlaySample(Sexy::SOUND_HUGE_WAVE);
			}
			else
			{
				if (mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_DAY_GRASSWALK ||
					mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES ||
					mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST ||
					mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF)
				{
					if (mHugeWaveCountDown == 400)
					{
						mApp->mMusic->StartBurst();
					}
				}
				else if (mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS)
				{
					if (mHugeWaveCountDown == 700)
					{
						mApp->mMusic->StartBurst();
					}
				}
				return;
			}
		}
	}

	if (mChallenge->UpdateZombieSpawning())
		return;

	if (mCurrentWave == mNumWaves)
	{
		if (IsFinalSurvivalStage())
		{
			return;
		}
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
		{
			return;
		}
		if (!mApp->IsSurvivalMode() && !mApp->IsContinuousChallenge())
		{
			return;
		}
	}

	mZombieCountDown--;
	if (mCurrentWave == mNumWaves && mApp->IsSurvivalMode())
	{
		if (mZombieCountDown == 0)
		{
			FadeOutLevel();
		}
		return;
	}

	if (mZombieCountDown > 200 && mZombieCountDownStart - mZombieCountDown > 400 && TotalZombiesHealthInWave(mCurrentWave - 1) <= mWaveHealth.next)
	{
		mZombieCountDown = 200;
	}
	if (mZombieCountDown == 5)
	{
		if (IsFlagWave(mCurrentWave))
		{
			ClearAdviceImmediately();
			DisplayAdviceAgain(("[ADVICE_HUGE_WAVE]"), MessageStyle::MESSAGE_STYLE_HUGE_WAVE, AdviceType::ADVICE_HUGE_WAVE);
			mHugeWaveCountDown = 750;
			return;
		}
		NextWaveComing();
	}
	if (mZombieCountDown == 0)
	{
        const int aWaveBefore=mCurrentWave;
        SpawnZombieWave();
        if(mCurrentWave==aWaveBefore) {mZombieCountDown=1;return;}
		mWaveHealth.start = TotalZombiesHealthInWave(mCurrentWave - 1);

		if (mCurrentWave == mNumWaves && mApp->IsSurvivalMode())
		{
			mWaveHealth.next = 0;
			mZombieCountDown = ZOMBIE_COUNTDOWN_BEFORE_REPICK + 1;
		}
		else if (IsFlagWave(mCurrentWave) && (mApp->IsWallnutBowlingLevel() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND))
		{
			mWaveHealth.next = 0;
			mZombieCountDown = ZOMBIE_COUNTDOWN_BEFORE_FLAG;
		}
		else
		{
			mWaveHealth.next = static_cast<int64_t>(static_cast<double>(RandRangeFloat(0.5f, 0.65f)) * mWaveHealth.start);
			if (mApp->IsLittleTroubleLevel() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
			{
				mZombieCountDown = 750;
			}
			else
			{
				mZombieCountDown = ZOMBIE_COUNTDOWN + Rand(ZOMBIE_COUNTDOWN_RANGE);
				// @pvz-online: 高级选项·节奏档（批 C，MOD_BUILD 35）：乘在默认波间隔上
				// （快 ×0.6 / 慢 ×1.6），下限 400 帧——别让快档把波距压没了。只在闯关局
				// 生效；抢跑/旗前的特殊倒计时（上面各分支）不参与缩放，闯关局走不到。
				if (mApp->IsRunMode() && mApp->GetRunState() != nullptr)
				{
					mZombieCountDown = mZombieCountDown * RunState::TempoPermilleFor(mApp->GetRunState()->mTempo) / 1000;
					if (mZombieCountDown < 400) mZombieCountDown = 400;
				}
			}
		}
		SyncLegacyWaveHealth();
		mZombieCountDownStart = mZombieCountDown;
	}
}

void Board::UpdateIce()
{
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (mIceTimer[aRow])
		{
			mIceTimer[aRow]--;
			PvzpParticleSystem* aParticleIce = mApp->ParticleTryToGet(mIceParticleID[aRow]);
			if (mIceTimer[aRow] == 0)
			{
				mIceMinX[aRow] = BOARD_ICE_START;
				if (aParticleIce)
				{
					aParticleIce->ParticleSystemDie();
				}
			}
			else
			{
				float aPosX = mIceMinX[aRow];
				float aPosY = GridToPixelY(8, aRow);
				if (aParticleIce)
				{
					aParticleIce->SystemMove(aPosX, aPosY);
				}
				else
				{
					int aRenderPosition = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, aRow, 3);
					aParticleIce = mApp->AddPvzpParticle(aPosX, aPosY, aRenderPosition, ParticleEffect::PARTICLE_ICE_SPARKLE);
					mIceParticleID[aRow] = mApp->ParticleGetID(aParticleIce);
				}
			}

			int anAlpha = std::clamp(mIceTimer[aRow] / 10, 0, 255);
			aParticleIce->OverrideColor(nullptr, Color(255, 255, 255, anAlpha));
		}
	}
}

void Board::UpdateProgressMeter()
{
	if (mApp->IsFinalBossLevel())
	{
		Zombie* aBoss = GetBossZombie();
		if (aBoss && !aBoss->IsDeadOrDying())
		{
			mProgressMeterWidth = aBoss->mBodyMaxHealth > 0 ? static_cast<int>(static_cast<int64_t>(150) * (aBoss->mBodyMaxHealth - aBoss->mBodyHealth) / aBoss->mBodyMaxHealth) : 0;
		}
		else
		{
			mProgressMeterWidth = 150;
		}
	}
	else if (mCurrentWave != 0)
	{
		if (mFlagRaiseCounter > 0)
			mFlagRaiseCounter--;

		int aTotalWidth = 150;  // total meter length distributed across the waves
		int aNumWavesPerFlag = GetNumWavesPerFlag();
		bool aHasFlags = ProgressMeterHasFlags();  // flag waves occupy extra meter length when flags are drawn
		if (aHasFlags)
		{
			aTotalWidth -= 12 * mNumWaves / aNumWavesPerFlag;
		}

		int aWaveLength = aTotalWidth / (mNumWaves - 1);
		int aCurrentWaveLength = (mCurrentWave - 1) * aTotalWidth / (mNumWaves - 1);
		int aNextWaveLength = mCurrentWave * aTotalWidth / (mNumWaves - 1);
		if (aHasFlags)
		{
			int anExtraLength = mCurrentWave / aNumWavesPerFlag * 12;  // add back the flag length of completed flag waves
			aCurrentWaveLength += anExtraLength;
			aNextWaveLength += anExtraLength;
		}

		float aFraction = (mZombieCountDownStart - mZombieCountDown) / static_cast<float>(mZombieCountDownStart);
		if (mWaveHealth.next != -1)
		{
			int64_t aHealthCurrent = TotalZombiesHealthInWave(mCurrentWave - 1);
			// damage that must be dealt to this wave to trigger the next wave
			int64_t aDamageTarget = mWaveHealth.start - mWaveHealth.next;
			if (aDamageTarget < 1)
			{
				aDamageTarget = 1;
			}
			// health fraction = damage dealt to this wave / damage needed to trigger the next wave
			float aHealthFraction = (aDamageTarget - aHealthCurrent + mWaveHealth.next) / static_cast<float>(aDamageTarget);
			aFraction = std::max(aHealthFraction, aFraction);
		}

		int aLength = std::clamp(aCurrentWaveLength + FloatRoundToInt((aNextWaveLength - aCurrentWaveLength) * aFraction), 1, 150);
		int aDelta = aLength - mProgressMeterWidth;
		// adjust every 20cs, or every 5cs when more than a wave length behind
		if ((aDelta > aWaveLength && (mMainCounter % 5 == 0)) || (aDelta > 0 && (mMainCounter % 20 == 0)))
		{
			mProgressMeterWidth++;
		}
	}
}

void Board::UpdateTutorial()
{
	if (mTutorialTimer > 0)
		mTutorialTimer--;

	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER && mTutorialTimer == 0)
	{
		DisplayAdvice("[ADVICE_CLICK_PEASHOOTER]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
		TutorialArrowShow(mSeedBank->mX + mSeedBank->mSeedPackets[0].mX, mSeedBank->mY + mSeedBank->mSeedPackets[0].mY);
		mTutorialTimer = -1;
	}
	else if (mTutorialState == TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_2_PLANT_SUNFLOWER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER)
	{
		if (mTutorialTimer == 0)
		{
			DisplayAdvice("[ADVICE_PLANT_SUNFLOWER2]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL2, AdviceType::ADVICE_NONE);
			mTutorialTimer = -1;
		}
		else if (mZombieCountDown == 750 && mCurrentWave == 0)
		{
			DisplayAdvice("[ADVICE_PLANT_SUNFLOWER3]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL2, AdviceType::ADVICE_NONE);
		}
	}
	else if (mTutorialState == TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER ||
		mTutorialState == TutorialState::TUTORIAL_MORESUN_PLANT_SUNFLOWER ||
		mTutorialState == TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER)
	{
		if (mTutorialTimer == 0)
		{
			DisplayAdvice("[ADVICE_PLANT_SUNFLOWER5]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LATER, AdviceType::ADVICE_PLANT_SUNFLOWER5);
			mTutorialTimer = -1;
		}
	}

	if (mApp->IsFirstTimeAdventureMode() && mLevel >= 3 && mLevel != 5 && mLevel <= 7 && mTutorialState == TutorialState::TUTORIAL_OFF &&
		mCurrentWave >= 5 && !gShownMoreSunTutorial && mSeedBank->mSeedPackets[1].CanPickUp() && CountPlantByType(SeedType::SEED_SUNFLOWER) < 3)
	{
		PVZP_ASSERT(!ChooseSeedsOnCurrentLevel());
		DisplayAdvice("[ADVICE_PLANT_SUNFLOWER4]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LATER_STAY, AdviceType::ADVICE_NONE);
		gShownMoreSunTutorial = true;
		SetTutorialState(TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER);
		mTutorialTimer = 500;
	}
}

void Board::SetTutorialState(TutorialState theTutorialState)
{
	switch (theTutorialState)
	{
	case TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER:
		if (mPlants.mSize == 0)
		{
			float aPosX = mSeedBank->mX + mSeedBank->mSeedPackets[0].mX;
			float aPosY = mSeedBank->mY + mSeedBank->mSeedPackets[0].mY;
			TutorialArrowShow(aPosX, aPosY);
			DisplayAdvice("[ADVICE_CLICK_SEED_PACKET]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
		}
		else
		{
			DisplayAdvice("[ADVICE_ENOUGH_SUN]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
			mTutorialTimer = 400;
		}
		break;

	case TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER:
		mTutorialTimer = -1;
		TutorialArrowRemove();
		if (mPlants.mSize == 0)
		{
			DisplayAdvice("[ADVICE_CLICK_ON_GRASS]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
		}
		else
		{
			ClearAdvice(AdviceType::ADVICE_NONE);
		}
		break;

	case TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER:
		DisplayAdvice("[ADVICE_PLANTED_PEASHOOTER]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
		mSunCountDown = 400;
		break;

	case TutorialState::TUTORIAL_LEVEL_1_COMPLETED:
		DisplayAdvice("[ADVICE_ZOMBIE_ONSLAUGHT]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1, AdviceType::ADVICE_NONE);
		mZombieCountDown = 99;
		mZombieCountDownStart = mZombieCountDown;
		break;

	case TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER:
	case TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER:
		TutorialArrowShow(mSeedBank->mX + mSeedBank->mSeedPackets[1].mX, mSeedBank->mY + mSeedBank->mSeedPackets[1].mY);
		break;

	case TutorialState::TUTORIAL_LEVEL_2_PLANT_SUNFLOWER:
	case TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER:
	case TutorialState::TUTORIAL_MORESUN_PLANT_SUNFLOWER:
	case TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER:
		TutorialArrowRemove();
		break;

	case TutorialState::TUTORIAL_LEVEL_2_COMPLETED:
		if (mCurrentWave == 0)
		{
			mZombieCountDown = 999;
			mZombieCountDownStart = mZombieCountDown;
		}
		break;

	case TutorialState::TUTORIAL_SLOT_MACHINE_PULL:
		DisplayAdvice("[ADVICE_SLOT_MACHINE_PULL]", MessageStyle::MESSAGE_STYLE_SLOT_MACHINE, AdviceType::ADVICE_SLOT_MACHINE_PULL);
		break;

	case TutorialState::TUTORIAL_SLOT_MACHINE_COMPLETED:
		ClearAdvice(AdviceType::ADVICE_SLOT_MACHINE_PULL);
		break;

	case TutorialState::TUTORIAL_SHOVEL_PICKUP:
	{
		DisplayAdvice("[ADVICE_CLICK_SHOVEL]", MessageStyle::MESSAGE_STYLE_HINT_STAY, AdviceType::ADVICE_NONE);
		Rect aShovelButtonRect = GetShovelButtonRect();
		int aPosX = aShovelButtonRect.mX + aShovelButtonRect.mWidth / 2 - 25;
		int aPosY = aShovelButtonRect.mY + aShovelButtonRect.mHeight - 65;
		TutorialArrowShow(aPosX, aPosY);
		break;
	}

	case TutorialState::TUTORIAL_SHOVEL_DIG:
		DisplayAdvice("[ADVICE_CLICK_PLANT]", MessageStyle::MESSAGE_STYLE_HINT_STAY, AdviceType::ADVICE_NONE);
		TutorialArrowRemove();
		break;

	case TutorialState::TUTORIAL_SHOVEL_KEEP_DIGGING:
		DisplayAdvice("[ADVICE_KEEP_DIGGING]", MessageStyle::MESSAGE_STYLE_HINT_STAY, AdviceType::ADVICE_NONE);
		break;

	case TutorialState::TUTORIAL_SHOVEL_COMPLETED:
		ClearAdvice(AdviceType::ADVICE_NONE);
		mCutScene->mCutsceneTime = 1500;
		mCutScene->mCrazyDaveDialogStart = 2410;
		break;

	default:
		break;
	}

	mTutorialState = theTutorialState;
}

void Board::UpdateGame()
{
    if (mApp->mOnlineSession && mApp->mOnlineSession->RelayBattleHeld()) return;
	UpdateGameObjects();
	if (StageHasFog() && mFogBlownCountDown > 0)
	{
		float aMaxFogOffset = 1065.0f - LeftFogColumn() * 80.0f;
		if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		{
			mFogOffset = PvzpAnimateCurveFloat(200, 0, mFogBlownCountDown, aMaxFogOffset, 0, PvzpCurves::CURVE_EASE_OUT);
		}
		else if (mFogBlownCountDown < 2000)
		{
			mFogOffset = PvzpAnimateCurveFloat(2000, 0, mFogBlownCountDown, aMaxFogOffset, 0, PvzpCurves::CURVE_EASE_OUT);
		}
		else if (mFogOffset < aMaxFogOffset)
		{
			mFogOffset = PvzpAnimateCurveFloat(-5, aMaxFogOffset, mFogOffset * 1.1f, 0, aMaxFogOffset, PvzpCurves::CURVE_LINEAR);
		}
	}

	if (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mCutScene->ShouldRunUpsellBoard())
		return;

	mMainCounter++;
	// Periodic high-load evidence survives a later crash without logging every spawn/frame.
	if (mMainCounter % 500 == 0 &&
		(mZombies.mSize >= mZombies.mMaxSize / 2 || mProjectiles.mSize >= mProjectiles.mMaxSize / 2 ||
		 mCoins.mSize >= mCoins.mMaxSize / 2 ||
		 mApp->mEffectSystem->mReanimationHolder->mReanimations.mSize >= 768 ||
		 mApp->mEffectSystem->mAttachmentHolder->mAttachments.mSize >= 768))
	{
		const auto& aReanims = mApp->mEffectSystem->mReanimationHolder->mReanimations;
		const auto& aAttachments = mApp->mEffectSystem->mAttachmentHolder->mAttachments;
		PvzpLogLn("[capacity] wave {} frame {} zombies {}/{} plants {}/{} projectiles {}/{} coins {}/{} reanims {}/{} attachments {}/{}",
			mCurrentWave, mMainCounter, mZombies.mSize, mZombies.mMaxSize, mPlants.mSize, mPlants.mMaxSize,
			mProjectiles.mSize, mProjectiles.mMaxSize, mCoins.mSize, mCoins.mMaxSize,
			aReanims.mSize, aReanims.mMaxSize, aAttachments.mSize, aAttachments.mMaxSize);
	}
	UpdateSunSpawning();
	UpdateZombieSpawning();
	UpdateIce();
	if (mIceTrapCounter > 0)
	{
		mIceTrapCounter--;
		if (mIceTrapCounter == 0)
		{
			PvzpParticleSystem* aPoolSparklyParticle = mApp->ParticleTryToGet(mPoolSparklyParticleID);
			if (aPoolSparklyParticle)
			{
				aPoolSparklyParticle->mDontUpdate = false;
			}
		}
	}

	if (mFogBlownCountDown > 0)
	{
		mFogBlownCountDown--;
	}

	if (mMainCounter == 1 && mApp->IsFirstTimeAdventureMode())
	{
		if (mLevel == 1)
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER);
		}
		else if (mLevel == 2)
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER);
			DisplayAdvice("[ADVICE_PLANT_SUNFLOWER1]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL2, AdviceType::ADVICE_NONE);
			mTutorialTimer = 500;
		}
	}

	UpdateProgressMeter();
}

void Board::Update()
{
    if (mApp->mOnlineSession && mApp->mOnlineSession->RelayBattleHeld()) {
        Widget::Update();MarkDirty();UpdateQuickChat();UpdateBoardWatch();return;
    }
    if(AdmissionBoard::Update(this))return;
    NextWaveControl::Update(this);
    UpdateQuickChat();
    UpdateBoardWatch();
	if (mApp->GetDialog(DIALOG_ONLINE_RECONNECT)) { MarkDirty(); return; }
	PvzpHesitationBracket aHesitation("Board::Update");

	Widget::Update();
	MarkDirty();

	mBoardUpdateCounter++;
    if (mDeferredPlantSetup.IsWaiting())
    {
        if (!mPaused) { mApp->mEffectSystem->Update(); CompleteDeferredPlantSetup(); }
        UpdateMousePosition(); UpdateQuickChat(); UpdateBoardWatch();
        mMenuButton->Update(); mAdvice->Update();
        return;
    }

	mCutScene->Update();
	UpdateMousePosition();
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		mApp->mZenGarden->ZenGardenUpdate();
	}
	if (IsScaryPotterDaveTalking())
	{
		mApp->UpdateCrazyDave();
	}

	if (mPaused)
	{
		mChallenge->Update();
		mCursorPreview->mVisible = false;
		mCursorObject->mVisible = false;
		return;
	}

	bool aDisabled = !CanInteractWithBoardButtons() || mIgnoreMouseUp;
	if (!mMenuButton->mBtnNoDraw)
	{
		mMenuButton->mDisabled = aDisabled;
	}
	mMenuButton->Update();
	if (mStoreButton)
	{
		mStoreButton->mDisabled = aDisabled;
		mStoreButton->Update();
	}

	mApp->mEffectSystem->Update();
	mAdvice->Update();
	UpdateTutorial();

	if (mCobCannonCursorDelayCounter > 0)
	{
		mCobCannonCursorDelayCounter--;
	}
	if (mOutOfMoneyCounter > 0)
	{
		mOutOfMoneyCounter--;
	}
	if (mShakeCounter > 0)
	{
		mShakeCounter--;
		if (mShakeCounter == 0)
		{
			mX = 0;
			mY = 0;
		}
		else
		{
			if (!Rand(3))
			{
				mShakeAmountX = -mShakeAmountX;
			}
			mX = PvzpAnimateCurve(12, 0, mShakeCounter, 0, mShakeAmountX, PvzpCurves::CURVE_BOUNCE);
			mY = PvzpAnimateCurve(12, 0, mShakeCounter, 0, mShakeAmountY, PvzpCurves::CURVE_BOUNCE);
		}
	}
	if (mCoinBankFadeCount > 0 && mApp->GetDialog(Dialogs::DIALOG_PURCHASE_PACKET_SLOT) == nullptr)
	{
		mCoinBankFadeCount--;
	}
	UpdateLayers();

	if (mTimeStopCounter > 0)
		return;

	mEffectCounter++;
	if (StageHasPool() && !mIceTrapCounter && mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON && !mCutScene->IsSurvivalRepick())
	{
		mApp->mPoolEffect->mPoolCounter++;
	}
	if (mBackground == BackgroundType::BACKGROUND_3_POOL && mPoolSparklyParticleID == ParticleSystemID::PARTICLESYSTEMID_NULL)
	{
		int aRenderPosition = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, 2, 0);
		PvzpParticleSystem* aPoolParticle = mApp->AddPvzpParticle(450, 295, aRenderPosition, ParticleEffect::PARTICLE_POOL_SPARKLY);
		mPoolSparklyParticleID = mApp->ParticleGetID(aPoolParticle);
	}

	UpdateGridItems();
	UpdateFwoosh();
	UpdateGame();
	UpdateFog();
	mChallenge->Update();
	UpdateLevelEndSequence();
	mPrevMouseX = mApp->mWidgetManager->mLastMouseX;
	mPrevMouseY = mApp->mWidgetManager->mLastMouseY;
}

void Board::UpdateLayers()
{
	if (mWidgetManager)
	{
		mWidgetManager->MarkAllDirty();

		for (DialogList::iterator anIter = mApp->mDialogList.begin(); anIter != mApp->mDialogList.end(); ++anIter)
		{
			Dialog* aDialog = *anIter;
			mWidgetManager->BringToFront(aDialog);
			aDialog->MarkDirty();
		}
	}
}

bool Board::RowCanHaveZombies(int theRow)
{
	if (theRow < 0 || theRow >= MAX_GRID_SIZE_Y)
		return false;

	return (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RESODDED && theRow <= 4) || mPlantRow[theRow] != PlantRowType::PLANTROW_DIRT;
}

int Board::GetIceZPos(int theRow)
{
	return MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, theRow, 2);
}

void Board::DrawIce(Graphics* g, int theGridY)
{
	int aPosY = GridToPixelY(8, theGridY) + 20;
	int aHeight = Sexy::IMAGE_ICE->GetHeight();
	int aWidth = Sexy::IMAGE_ICE->GetWidth();
	int anAlpha = std::clamp(255 * mIceTimer[theGridY] / 10, 0, 255);
	if (anAlpha < 255)
	{
		g->SetColorizeImages(true);
		g->SetColor(Color(255, 255, 255, anAlpha));
	}

	int aBeginningX = mIceMinX[theGridY] + 13, aDeltaX;
	for (int aPosX = aBeginningX; aPosX < BOARD_WIDTH; aPosX += aDeltaX)
	{
		if (aPosX == aBeginningX)
		{
			aDeltaX = (BOARD_WIDTH - aBeginningX) % aWidth;
			if (!aDeltaX) aDeltaX = aWidth;
		}
		else aDeltaX = aWidth;
		Rect aRepeatSrcRect(aWidth - aDeltaX, 0, aDeltaX, aHeight);
		Rect aRepeatDstRect(aPosX, aPosY, aDeltaX, aHeight);
		g->DrawImage(Sexy::IMAGE_ICE, aRepeatDstRect, aRepeatSrcRect);
	}
	g->DrawImage(Sexy::IMAGE_ICE_CAP, mIceMinX[theGridY], aPosY);
	g->SetColorizeImages(false);
}

void Board::DrawBackdrop(Graphics* g)
{
	Image* aBgImage = nullptr;
	switch (mBackground)
	{
	case BackgroundType::BACKGROUND_1_DAY:				aBgImage = Sexy::IMAGE_BACKGROUND1;						break;
	case BackgroundType::BACKGROUND_2_NIGHT:			aBgImage = Sexy::IMAGE_BACKGROUND2;						break;
	case BackgroundType::BACKGROUND_3_POOL:				aBgImage = Sexy::IMAGE_BACKGROUND3;						break;
	case BackgroundType::BACKGROUND_4_FOG:				aBgImage = Sexy::IMAGE_BACKGROUND4;						break;
	case BackgroundType::BACKGROUND_5_ROOF:				aBgImage = Sexy::IMAGE_BACKGROUND5;						break;
	case BackgroundType::BACKGROUND_6_BOSS:				aBgImage = Sexy::IMAGE_BACKGROUND6BOSS;					break;
	case BackgroundType::BACKGROUND_MUSHROOM_GARDEN:	aBgImage = Sexy::IMAGE_BACKGROUND_MUSHROOMGARDEN;		break;
	case BackgroundType::BACKGROUND_GREENHOUSE:			aBgImage = Sexy::IMAGE_BACKGROUND_GREENHOUSE;			break;
	case BackgroundType::BACKGROUND_ZOMBIQUARIUM:		aBgImage = Sexy::IMAGE_AQUARIUM1;						break;
	case BackgroundType::BACKGROUND_TREEOFWISDOM:		aBgImage = nullptr;										break;
	default:											PVZP_ASSERT(false);											break;
	}

	if (mLevel == 1 && mApp->IsFirstTimeAdventureMode())
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, -BOARD_OFFSET, 0);
		int aWidth = PvzpAnimateCurve(0, 1000, mSodPosition, 0, Sexy::IMAGE_SOD1ROW->GetWidth(), PvzpCurves::CURVE_LINEAR);
		Rect aSrcRect(0, 0, aWidth, Sexy::IMAGE_SOD1ROW->GetHeight());
		g->DrawImage(Sexy::IMAGE_SOD1ROW, 239 - BOARD_OFFSET, 265, aSrcRect);
	}
	else if (((mLevel == 2 || mLevel == 3) && mApp->IsFirstTimeAdventureMode()) || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RESODDED)
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, -BOARD_OFFSET, 0);
		g->DrawImage(Sexy::IMAGE_SOD1ROW, 239 - BOARD_OFFSET, 265);
		int aWidth = PvzpAnimateCurve(0, 1000, mSodPosition, 0, Sexy::IMAGE_SOD3ROW->GetWidth(), PvzpCurves::CURVE_LINEAR);
		Rect aSrcRect(0, 0, aWidth, Sexy::IMAGE_SOD3ROW->GetHeight());
		g->DrawImage(Sexy::IMAGE_SOD3ROW, 235 - BOARD_OFFSET, 149, aSrcRect);
	}
	else if (mLevel == 4 && mApp->IsFirstTimeAdventureMode())
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, -BOARD_OFFSET, 0);
		g->DrawImage(Sexy::IMAGE_SOD3ROW, 235 - BOARD_OFFSET, 149);
		int aWidth = PvzpAnimateCurve(0, 1000, mSodPosition, 0, 773, PvzpCurves::CURVE_LINEAR);
		Rect aSrcRect(232, 0, aWidth, Sexy::IMAGE_BACKGROUND1->GetHeight());
		g->DrawImage(Sexy::IMAGE_BACKGROUND1, 232 - BOARD_OFFSET, 0, aSrcRect);
	}
	else if (aBgImage)
	{
		if (aBgImage == Sexy::IMAGE_BACKGROUND_MUSHROOMGARDEN || aBgImage == Sexy::IMAGE_BACKGROUND_GREENHOUSE || aBgImage == Sexy::IMAGE_AQUARIUM1)
		{
			g->DrawImage(aBgImage, 0, 0);
		}
		else
		{
			g->DrawImage(aBgImage, -BOARD_OFFSET, 0);
		}
	}

	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		DrawHouseDoorBottom(g);
	}
	if (StageHasPool())
	{
		mApp->mPoolEffect->PoolEffectDraw(g, StageIsNight());
	}
	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER)
	{
		Graphics aClipG(*g);
		aClipG.SetColorizeImages(true);
		aClipG.SetColor(GetFlashingColor(mMainCounter, 75));
		aClipG.DrawImage(Sexy::IMAGE_SOD1ROW, 239 - BOARD_OFFSET, 265);
		aClipG.SetColorizeImages(false);
	}
	mChallenge->DrawBackdrop(g);
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && StageHasGraveStones())
	{
		g->DrawImage(Sexy::IMAGE_NIGHT_GRAVE_GRAPHIC, 1092, 40);
	}
}

bool RenderItemSortFunc(const RenderItem& theItem1, const RenderItem& theItem2)
{
	if (theItem1.mZPos == theItem2.mZPos)
	{
		return theItem1.mGameObject < theItem2.mGameObject;
	}

	return theItem1.mZPos < theItem2.mZPos;
}

void Board::AddBossRenderItem(RenderItem* theRenderList, int& theCurRenderItem, Zombie* theBossZombie)
{
	PVZP_ASSERT(theCurRenderItem >= 0);
	int aBackLegRow = 1;
	int aFrontLegRow = 3;
	int aBackArmRow = 4;
	if (theBossZombie->IsDeadOrDying())
	{
		aBackArmRow = 1;
	}
	else if (theBossZombie->mZombiePhase == ZombiePhase::PHASE_BOSS_STOMPING)
	{
		Reanimation* aBossReanim = mApp->ReanimationTryToGet(theBossZombie->mBodyReanimID);
		if (aBossReanim->mAnimTime > 0.25f && aBossReanim->mAnimTime < 0.75f)
		{
			if (theBossZombie->mTargetRow == 1)
			{
				aBackLegRow = 2;
			}
			else if (theBossZombie->mTargetRow == 3)
			{
				aFrontLegRow = 4;
			}
		}
	}

	RenderItem* aItem = &theRenderList[theCurRenderItem];
	aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
	aItem->mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, aBackLegRow, 2);
	aItem->mBossPart = BossPart::BOSS_PART_BACK_LEG;
	theCurRenderItem++;
	aItem = &theRenderList[theCurRenderItem];
	aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
	aItem->mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, aFrontLegRow, 2);
	aItem->mBossPart = BossPart::BOSS_PART_FRONT_LEG;
	theCurRenderItem++;
	aItem = &theRenderList[theCurRenderItem];
	aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
	aItem->mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, 4, 2);
	aItem->mBossPart = BossPart::BOSS_PART_MAIN;
	theCurRenderItem++;
	aItem = &theRenderList[theCurRenderItem];
	aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
	aItem->mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, aBackArmRow, 3);
	aItem->mBossPart = BossPart::BOSS_PART_BACK_ARM;
	theCurRenderItem++;

	Reanimation* aBallReanim = mApp->ReanimationTryToGet(theBossZombie->mBossFireBallReanimID);
	if (aBallReanim)
	{
		RenderItem* aItem = &theRenderList[theCurRenderItem];
		aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
		aItem->mZPos = aBallReanim->mRenderOrder;
		aItem->mBossPart = BossPart::BOSS_PART_FIREBALL;
		theCurRenderItem++;
	}
}

/*
[[maybe_unused]]
static inline void AddGameObjectRenderItem(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	PVZP_ASSERT(theCurRenderItem >= 0);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	theCurRenderItem++;
}
*/

static inline void AddGameObjectRenderItemCursorPreview(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	PVZP_ASSERT(theCurRenderItem >= 0);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mCursorPreview = (CursorPreview*)theGameObject;

	theCurRenderItem++;
}

static inline void AddGameObjectRenderItemPlant(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	PVZP_ASSERT(theCurRenderItem >= 0);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mPlant = (Plant*)theGameObject;

	theCurRenderItem++;
}

static inline void AddGameObjectRenderItemZombie(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	PVZP_ASSERT(theCurRenderItem >= 0);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mZombie = (Zombie*)theGameObject;
	theCurRenderItem++;
}

static inline void AddGameObjectRenderItemProjectile(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	PVZP_ASSERT(theCurRenderItem >= 0);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mProjectile = (Projectile*)theGameObject;
	theCurRenderItem++;
}

static inline void AddGameObjectRenderItemCoin(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	PVZP_ASSERT(theCurRenderItem >= 0);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mCoin = (Coin*)theGameObject;
	theCurRenderItem++;
}

static inline void AddUIRenderItem(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, int thePosZ)
{
	PVZP_ASSERT(theCurRenderItem >= 0);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = thePosZ;
	aRenderItem.mGameObject = nullptr;
	theCurRenderItem++;
}

// @pvz-online: local display preferences stay outside Board snapshots, matching PC H/J.
static bool gShowZombieHealthNumbers = false;
static bool gShowPlantHealthNumbers = false;
bool Board::PlantHealthVisible() const { return gShowPlantHealthNumbers; }
bool Board::ZombieHealthVisible() const { return gShowZombieHealthNumbers; }
void Board::SetPlantHealthVisible(bool visible) { gShowPlantHealthNumbers = visible; }
void Board::SetZombieHealthVisible(bool visible) { gShowZombieHealthNumbers = visible; }

static void DrawHealthNumberOverRect(Graphics* g, int health, const Rect& rect, int line = 0)
{
    const std::string text = std::to_string(health);
    Graphics label(*g);
    label.SetFont(FONT_CONTINUUMBOLD14);
    const int x = rect.mX + rect.mWidth / 2 - FONT_CONTINUUMBOLD14->StringWidth(text) / 2;
    const int y = rect.mY - 4 - line * 16;
    label.SetColor(Color::Black);
    label.DrawString(text, x - 1, y); label.DrawString(text, x + 1, y);
    label.DrawString(text, x, y - 1); label.DrawString(text, x, y + 1);
    label.SetColor(Color::White); label.DrawString(text, x, y);
}

int Board::GetRenderListCapacity() const
{
	// Per-object upper bounds include Boss parts, shadows, overlays and standalone effects.
	return 3 * mPlants.mSize + mCoins.mSize + 5 * mZombies.mSize + 2 * mProjectiles.mSize +
		mLawnMowers.mSize + mApp->mEffectSystem->mParticleHolder->mParticleSystems.mSize +
		mApp->mEffectSystem->mReanimationHolder->mReanimations.mSize + 2 * mGridItems.mSize +
		MAX_GRID_SIZE_Y + 16;
}

int Board::BuildRenderList(RenderItem* theRenderList, int theCapacity)
{
	// Reject undersized buffers before any append; fixed 2048-entry lists cannot hold dense waves.
	if (theRenderList == nullptr || theCapacity < GetRenderListCapacity()) return -1;
	// Clear the entire union, including the upper pointer bits on 64-bit Android.
	std::fill_n(theRenderList, GetRenderListCapacity(), RenderItem{});

	RenderItem* aRenderList = theRenderList;
	int aRenderItemCount = 0;

	{
		for (Plant* aPlant : mPlants)
		{
			if (aPlant->mDead)
				continue;
			if (aPlant->mOnBungeeState == PlantOnBungeeState::NOT_ON_BUNGEE)
			{
				AddGameObjectRenderItemPlant(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_PLANT, aPlant);

				if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && aPlant->mPottedPlantIndex != -1)
				{
					RenderItem& aRenderItem = aRenderList[aRenderItemCount];
					aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_PLANT_OVERLAY;
					aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, 0, mY);
					aRenderItem.mPlant = aPlant;
					aRenderItemCount++;
				}

				if ((aPlant->mSeedType == SeedType::SEED_MAGNETSHROOM || aPlant->mSeedType == SeedType::SEED_GOLD_MAGNET) && aPlant->DrawMagnetItemsOnTop())
				{
					RenderItem& aRenderItem = aRenderList[aRenderItemCount];
					aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_PLANT_MAGNET_ITEMS;
					aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, -1);
					aRenderItem.mPlant = aPlant;
					aRenderItemCount++;
				}
			}
		}
	}
	{
		for (Coin* aCoin : mCoins)
		{
			if (aCoin->mDead)
				continue;
			AddGameObjectRenderItemCoin(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_COIN, aCoin);
		}
	}
	{
		for (Zombie* aZombie : mZombies)
		{
			if (aZombie->mDead)
				continue;
			if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
			{
				AddBossRenderItem(aRenderList, aRenderItemCount, aZombie);
			}
			else
			{
				AddGameObjectRenderItemZombie(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_ZOMBIE, aZombie);

				if (aZombie->HasShadow())
				{
					RenderItem& aRenderItem = aRenderList[aRenderItemCount];
					aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_ZOMBIE_SHADOW;
					aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, aZombie->mRow, 3);
					aRenderItem.mZombie = aZombie;
					aRenderItemCount++;
				}

				if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE)
				{
					RenderItem& aRenderItem = aRenderList[aRenderItemCount];
					aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_ZOMBIE_BUNGEE_TARGET;
					aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, aZombie->mRow, 1);
					aRenderItem.mZombie = aZombie;
					aRenderItemCount++;
				}
			}
		}
	}
	{
		for (Projectile* aProjectile : mProjectiles)
		{
			if (aProjectile->mDead)
				continue;
			AddGameObjectRenderItemProjectile(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_PROJECTILE, aProjectile);

			RenderItem& aRenderItem = aRenderList[aRenderItemCount];
			aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_PROJECTILE_SHADOW;
			aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, aProjectile->mRow, 3);
			aRenderItem.mProjectile = aProjectile;
			aRenderItemCount++;
		}
	}
	{
		for (LawnMower* aLawnMower : mLawnMowers)
		{
			if (aLawnMower->mDead)
				continue;
			RenderItem& aRenderItem = aRenderList[aRenderItemCount];
			aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_MOWER;
			aRenderItem.mZPos = aLawnMower->mRenderOrder;
			aRenderItem.mMower = aLawnMower;
			aRenderItemCount++;
		}
	}
	{
		for (PvzpParticleSystem* aParticle : mApp->mEffectSystem->mParticleHolder->mParticleSystems)
		{
			if (aParticle->mDead)
				continue;
			if (!aParticle->mIsAttachment)
			{
				RenderItem& aRenderItem = aRenderList[aRenderItemCount];
				aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_PARTICLE;
				aRenderItem.mZPos = aParticle->mRenderOrder;
				aRenderItem.mParticleSytem = aParticle;
				aRenderItemCount++;
			}
		}
	}
	{
		for (Reanimation* aReanimation : mApp->mEffectSystem->mReanimationHolder->mReanimations)
		{
			if (aReanimation->mDead)
				continue;
			if (!aReanimation->mIsAttachment)
			{
				RenderItem& aRenderItem = aRenderList[aRenderItemCount];
				aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_REANIMATION;
				aRenderItem.mZPos = aReanimation->mRenderOrder;
				aRenderItem.mReanimation = aReanimation;
				aRenderItemCount++;
			}
		}
	}
	{
		for (GridItem* aGridItem : mGridItems)
		{
			if (aGridItem->mDead)
				continue;
			RenderItem& aRenderItem = aRenderList[aRenderItemCount];
			aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_GRID_ITEM;
			aRenderItem.mZPos = aGridItem->mRenderOrder;
			aRenderItem.mGridItem = aGridItem;
			aRenderItemCount++;

			if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && aGridItem->mGridItemType == GridItemType::GRIDITEM_STINKY)
			{
				RenderItem& aRenderItem = aRenderList[aRenderItemCount];
				aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_GRID_ITEM_OVERLAY;
				aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, 0, aGridItem->mPosY - 30.0f);
				aRenderItem.mGridItem = aGridItem;
				aRenderItemCount++;
			}
		}
	}
	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		if (mIceTimer[i])
		{
			RenderItem& aRenderItem = aRenderList[aRenderItemCount];
			aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_ICE;
			aRenderItem.mBoardGridY = i;
			aRenderItem.mZPos = GetIceZPos(i);
			aRenderItemCount++;
		}
	}
	{
		int aZPos;
		if (mTimeStopCounter > 0)
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0);
		}
		else if (mApp->mGameScene == GameScenes::SCENE_PLAYING || mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 1);
		}
		else if (mCutScene->IsAfterSeedChooser() || mCutScene->IsInShovelTutorial() || mHelpIndex == AdviceType::ADVICE_CLICK_TO_CONTINUE)
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 1);
		}
		else
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0);
		}

		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_BACKDROP, MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 0));
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_BOTTOM_UI, aZPos);
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_COIN_BANK, MakeRenderOrder(RenderLayer::RENDER_LAYER_COIN_BANK, 0, 0));
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_TOP_UI, MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_TOP, 0, 0));
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_SCREEN_FADE, MakeRenderOrder(RenderLayer::RENDER_LAYER_SCREEN_FADE, 0, 0));
	}
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		int aZPos;
		if (StageHasRoof())
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, 0, 4);
		}
		else
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, 3, 2);
		}
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_DOOR_MASK, aZPos);
	}
	if (StageHasFog())
	{
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_FOG, MakeRenderOrder(RenderLayer::RENDER_LAYER_FOG, 0, 0));
	}
	if (mApp->IsStormyNightLevel() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS)
	{
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_STORM, MakeRenderOrder(RenderLayer::RENDER_LAYER_FOG, 0, 3));
	}
	AddGameObjectRenderItemCursorPreview(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_CURSOR_PREVIEW, mCursorPreview.get());

	return aRenderItemCount;
}

void Board::DrawGameObjects(Graphics* g)
{
	const int aCapacity = GetRenderListCapacity();
	// Local ownership also keeps nested resource-loading draws from invalidating this list.
	std::unique_ptr<RenderItem[]> aStorage(new RenderItem[aCapacity]);
	RenderItem* aRenderList = aStorage.get();
	const int aRenderItemCount = BuildRenderList(aRenderList, aCapacity);
	if (aRenderItemCount < 0) return;

	std::sort(aRenderList, aRenderList + aRenderItemCount, RenderItemSortFunc);

	for (int i = 0; i < aRenderItemCount; i++)
	{
		RenderItem& aRenderItem = aRenderList[i];
        Observer::CaptureRenderItem(g, this, aRenderItem);
		switch (aRenderItem.mRenderObjectType)
		{
		case RenderObjectType::RENDER_ITEM_PLANT:
		{
			Plant* aPlant = aRenderItem.mPlant;
			if (aPlant->BeginDraw(g))
			{
				aPlant->Draw(g);
				aPlant->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_PLANT_OVERLAY:
		{
			Plant* aPlant = aRenderItem.mPlant;
			if (aPlant->BeginDraw(g))
			{
				mApp->mZenGarden->DrawPlantOverlay(g, aPlant);
				aPlant->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_PLANT_MAGNET_ITEMS:
		{
			Plant* aPlant = aRenderItem.mPlant;
			if (aPlant->BeginDraw(g))
			{
				aPlant->DrawMagnetItems(g);
				aPlant->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_MOWER:
		{
			LawnMower* aLawnMower = aRenderItem.mMower;
			aLawnMower->Draw(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_ZOMBIE:
		{
			Zombie* aZombie = aRenderItem.mZombie;
			if (aZombie->BeginDraw(g))
			{
				aZombie->Draw(g);
				aZombie->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_ZOMBIE_SHADOW:
		{
			Zombie* aZombie = aRenderItem.mZombie;
			if (aZombie->BeginDraw(g))
			{
				aZombie->DrawShadow(g);
				aZombie->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_ZOMBIE_BUNGEE_TARGET:
		{
			Zombie* aZombie = aRenderItem.mZombie;
			aZombie->DrawBungeeTarget(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_BOSS_PART:
		{
			Zombie* aBossZombie = GetBossZombie();
			if (aBossZombie && aBossZombie->BeginDraw(g))
			{
				aBossZombie->DrawBossPart(g, aRenderItem.mBossPart);
				aBossZombie->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_COIN:
		{
			Coin* aCoin = aRenderItem.mCoin;
			if (aCoin->BeginDraw(g))
			{
				aCoin->Draw(g);
				aCoin->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_PROJECTILE:
		{
			Projectile* aProjectile = aRenderItem.mProjectile;
			if (aProjectile->BeginDraw(g))
			{
				aProjectile->Draw(g);
				aProjectile->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_PROJECTILE_SHADOW:
		{
			Projectile* aProjectile = aRenderItem.mProjectile;
			if (aProjectile->BeginDraw(g))
			{
				aProjectile->DrawShadow(g);
				aProjectile->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_CURSOR_PREVIEW:
		{
			CursorPreview* aCursorPreview = aRenderItem.mCursorPreview;
			if (aCursorPreview->BeginDraw(g))
			{
				aCursorPreview->Draw(g);
				aCursorPreview->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_GRID_ITEM:
		{
			GridItem* aGridItem = aRenderItem.mGridItem;
			aGridItem->DrawGridItem(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_GRID_ITEM_OVERLAY:
		{
			GridItem* aGridItem = aRenderItem.mGridItem;
			aGridItem->DrawGridItemOverlay(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_ICE:
			DrawIce(g, aRenderItem.mBoardGridY);
			break;

		case RenderObjectType::RENDER_ITEM_PARTICLE:
		{
			PvzpParticleSystem* aParticle = aRenderItem.mParticleSytem;
			aParticle->Draw(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_REANIMATION:
		{
			Reanimation* aReanimation = aRenderItem.mReanimation;
			aReanimation->Draw(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_COIN_BANK:
			DrawUICoinBank(g);
			break;

		case RenderObjectType::RENDER_ITEM_BACKDROP:
			DrawBackdrop(g);
			break;

		case RenderObjectType::RENDER_ITEM_DOOR_MASK:
			DrawHouseDoorTop(g);
			break;

		case RenderObjectType::RENDER_ITEM_BOTTOM_UI:
			DrawUIBottom(g);
			break;

		case RenderObjectType::RENDER_ITEM_TOP_UI:
			DrawUITop(g);
			break;

		case RenderObjectType::RENDER_ITEM_FOG:
			DrawFog(g);
			break;

		case RenderObjectType::RENDER_ITEM_STORM:
			mChallenge->DrawWeather(g);
			break;

		case RenderObjectType::RENDER_ITEM_SCREEN_FADE:
			DrawFadeOut(g);
			break;

		default:
			PVZP_ASSERT(false);
			break;
		}
	}

}

bool Board::HasProgressMeter()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		mApp->IsFinalBossLevel() ||
		mApp->IsSlotMachineLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsIZombieLevel())
		return true;

	if (mProgressMeterWidth == 0)
		return false;

	if (mApp->IsContinuousChallenge() ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->IsScaryPotterLevel())
		return false;

	return true;
}

bool Board::ProgressMeterHasFlags()
{
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
		return false;

	if (mApp->IsWhackAZombieLevel() ||
		mApp->IsFinalBossLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		mApp->IsSlotMachineLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsIZombieLevel())
		return false;

	return true;
}

void Board::DrawProgressMeter(Graphics* g)
{
	if (!HasProgressMeter())
		return;

	g->DrawImageCel(Sexy::IMAGE_FLAGMETER, 600, 575, 0);
	int aCelWidth = Sexy::IMAGE_FLAGMETER->GetCelWidth();
	int aCelHeight = Sexy::IMAGE_FLAGMETER->GetCelHeight();
	int aClipWidth = PvzpAnimateCurve(0, PROGRESS_METER_COUNTER, mProgressMeterWidth, 0, 143, PvzpCurves::CURVE_LINEAR);
	Rect aSrcRect(aCelWidth - aClipWidth - 7, aCelHeight, aClipWidth, aCelHeight);
	Rect aDstRect(aCelWidth - aClipWidth + 593, 575, aClipWidth, aCelHeight);
	g->DrawImage(Sexy::IMAGE_FLAGMETER, aDstRect, aSrcRect);

	// Draw mode-specific text or flags on the meter
	int aPosX = aCelWidth / 2 + 600;
	Color aColor(224, 187, 98);
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST)
	{
		std::string aMatchStr = std::format("{}/{} {}", mChallenge->mChallengeScore, 75, PvzpStringTranslate("[MATCHES]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (mApp->IsSquirrelLevel())
	{
		std::string aMatchStr = std::format("{}/{} {}", mChallenge->mChallengeScore, 7, PvzpStringTranslate("[SQUIRRELS]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE)
	{
		int aSunMoney = std::clamp(mSunMoney, 0, 2000);
		std::string aMatchStr = std::format("{}/{} {}", aSunMoney, 2000, PvzpStringTranslate("[SUN]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM)
	{
		int aSunMoney = std::clamp(mSunMoney, 0, 1000);
		std::string aMatchStr = std::format("{}/{} {}", aSunMoney, 1000, PvzpStringTranslate("[SUN]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (mApp->IsIZombieLevel())
	{
		std::string aMatchStr = std::format("{}/{} {}", mChallenge->mChallengeScore, 5, PvzpStringTranslate("[BRAINS]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (ProgressMeterHasFlags())
	{
		int aNumWavesPerFlag = GetNumWavesPerFlag();
		int aNumFlagWaves = mNumWaves / aNumWavesPerFlag;
		int aFlagsPosEnd = 590 + aCelWidth;
		for (int aFlagWave = 1; aFlagWave <= aNumFlagWaves; aFlagWave++)
		{
			int aHeight = 0;
			int aTotalWavesAtFlag = aFlagWave * aNumWavesPerFlag;
			if (aTotalWavesAtFlag < mCurrentWave)
			{
				aHeight = 14;
			}
			else if (aTotalWavesAtFlag == mCurrentWave)
			{
				aHeight = PvzpAnimateCurve(100, 0, mFlagRaiseCounter, 0, 14, PvzpCurves::CURVE_LINEAR);
			}
			int aPosX = PvzpAnimateCurve(0, mNumWaves, aTotalWavesAtFlag, aFlagsPosEnd, 606, PvzpCurves::CURVE_LINEAR);
			g->DrawImageCel(Sexy::IMAGE_FLAGMETERPARTS, aPosX, 571, 1, 0);
			g->DrawImageCel(Sexy::IMAGE_FLAGMETERPARTS, aPosX, 572 - aHeight, 2, 0);
		}
	}

	g->DrawImage(Sexy::IMAGE_FLAGMETERLEVELPROGRESS, 638, 589);
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM ||
		mApp->IsSquirrelLevel() ||
		mApp->IsSlotMachineLevel() ||
		mApp->IsIZombieLevel() ||
		mApp->IsFinalBossLevel())
		return;
	int aHeadProgress = PvzpAnimateCurve(0, 150, mProgressMeterWidth, 0, 135, CURVE_LINEAR);
	g->DrawImageCel(Sexy::IMAGE_FLAGMETERPARTS, aCelWidth - aHeadProgress + 580, 572, 0, 0);
}

void Board::DrawHouseDoorBottom(Graphics* g)
{
	switch (mBackground)
	{
	case BackgroundType::BACKGROUND_1_DAY:		g->DrawImage(Sexy::IMAGE_BACKGROUND1_GAMEOVER_INTERIOR_OVERLAY, -126, 225);		break;
	case BackgroundType::BACKGROUND_2_NIGHT:	g->DrawImage(Sexy::IMAGE_BACKGROUND2_GAMEOVER_INTERIOR_OVERLAY, -125, 196);		break;
	case BackgroundType::BACKGROUND_3_POOL:		g->DrawImage(Sexy::IMAGE_BACKGROUND3_GAMEOVER_INTERIOR_OVERLAY, -171, 241);		break;
	case BackgroundType::BACKGROUND_4_FOG:		g->DrawImage(Sexy::IMAGE_BACKGROUND4_GAMEOVER_INTERIOR_OVERLAY, -172, 246);		break;
	default:																													break;
	}
}

void Board::DrawHouseDoorTop(Graphics* g)
{
	switch (mBackground)
	{
	case BackgroundType::BACKGROUND_1_DAY:		g->DrawImage(Sexy::IMAGE_BACKGROUND1_GAMEOVER_MASK, -130, 202);		break;
	case BackgroundType::BACKGROUND_2_NIGHT:	g->DrawImage(Sexy::IMAGE_BACKGROUND2_GAMEOVER_MASK, -128, 207);		break;
	case BackgroundType::BACKGROUND_3_POOL:		g->DrawImage(Sexy::IMAGE_BACKGROUND3_GAMEOVER_MASK, -172, 234);		break;
	case BackgroundType::BACKGROUND_4_FOG:		g->DrawImage(Sexy::IMAGE_BACKGROUND4_GAMEOVER_MASK, -173, 133);		break;
	case BackgroundType::BACKGROUND_5_ROOF:		g->DrawImage(Sexy::IMAGE_BACKGROUND5_GAMEOVER_MASK, -220, 81);		break;
	case BackgroundType::BACKGROUND_6_BOSS:		g->DrawImage(Sexy::IMAGE_BACKGROUND6_GAMEOVER_MASK, -220, 81);		break;
	default:																										break;
	}
}

void Board::DrawLevel(Graphics* g)
{
	std::string aLevelStr;
	if (mApp->IsAdventureMode())
	{
		aLevelStr = std::string(PvzpStringTranslate("[LEVEL]")) + " " + mApp->GetStageString(mLevel);
	}
	else
	{
		aLevelStr = mApp->GetCurrentChallengeDef().mChallengeName;
		if (mApp->IsSurvivalMode() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
		{
			int aFlags = GetSurvivalFlagsCompleted();
			if (aFlags > 0)
			{
				std::string aFlagStr = mApp->Pluralize(aFlags, "[ONE_FLAG]", "[COUNT_FLAGS]");
				std::string aCompletedStr = PvzpReplaceString("[FLAGS_COMPLETED]", "{FLAGS}", aFlagStr);
				aLevelStr = std::format("{} - {}", PvzpStringTranslate(aLevelStr), aCompletedStr);
			}
		}
		else if (mApp->IsEndlessIZombie(mApp->mGameMode) || mApp->IsEndlessScaryPotter(mApp->mGameMode))
		{
			int aStreak = mChallenge->mSurvivalStage;
			if (mNextSurvivalStageCounter > 0)
			{
				aStreak++;
			}
			if (aStreak > 0)
			{
				std::string aStreakStr = PvzpReplaceNumberString("[ENDLESS_STREAK]", "{STREAK}", aStreak);
				aLevelStr = std::format("{} - {}", PvzpStringTranslate(aLevelStr), aStreakStr);
			}
		}
	}

	int aPosX = 780;
	int aPosY = 595;
	if (HasProgressMeter())
	{
		aPosX = 593;
	}
	if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
	{
		aPosY += PvzpAnimateCurve(50, 0, mChallenge->mChallengeStateCounter, 0, 50, PvzpCurves::CURVE_EASE_IN_OUT);
	}
	PvzpDrawString(g, aLevelStr, aPosX, aPosY, Sexy::FONT_HOUSEOFTERROR16, Color(224, 187, 98), DrawStringJustification::DS_ALIGN_RIGHT);
}

void Board::DrawZenWheelBarrowButton(Graphics* g, int theOffsetY)
{
	Rect aButtonRect = GetShovelButtonRect();
	GetZenButtonRect(GameObjectType::OBJECT_TYPE_WHEELBARROW, aButtonRect);
	PottedPlant* aPlant = mApp->mZenGarden->GetPottedPlantInWheelbarrow();
	if (aPlant && mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW)
	{
		if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
		{
			g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aButtonRect.mX - 7, aButtonRect.mY + theOffsetY - 3);
		}
		else
		{
			g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aButtonRect.mX - 7, aButtonRect.mY + theOffsetY + 4);
		}

		if (aPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
		{
			mApp->mZenGarden->DrawPottedPlant(g, aButtonRect.mX + 23, aButtonRect.mY + theOffsetY - 8, aPlant, 0.6f, true);
		}
		else if (aPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
		{
			mApp->mZenGarden->DrawPottedPlant(g, aButtonRect.mX + 28, aButtonRect.mY + theOffsetY + 2, aPlant, 0.5f, true);
		}
		else
		{
			mApp->mZenGarden->DrawPottedPlant(g, aButtonRect.mX + 34, aButtonRect.mY + theOffsetY + 12, aPlant, 0.4f, true);
		}
	}
	else
	{
		g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aButtonRect.mX - 7, aButtonRect.mY + theOffsetY - 3);
	}
}

void Board::DrawZenButtons(Graphics* g)
{
	int aOffsetY = 0;
	if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
	{
		aOffsetY = PvzpAnimateCurve(50, 0, mChallenge->mChallengeStateCounter, 0, -72, PvzpCurves::CURVE_EASE_IN_OUT);
	}

	for (GameObjectType aTool = GameObjectType::OBJECT_TYPE_WATERING_CAN; aTool <= GameObjectType::OBJECT_TYPE_NEXT_GARDEN; aTool = (GameObjectType)(aTool + 1))
	{
		if (!CanUseGameObject(aTool))
			continue;

		Rect aButtonRect = GetShovelButtonRect();
		if (aTool == GameObjectType::OBJECT_TYPE_NEXT_GARDEN)
		{
			aButtonRect.mX = 564;
			if (!mMenuButton->mBtnNoDraw)
			{
				g->DrawImage(Sexy::IMAGE_ZEN_NEXTGARDEN, aButtonRect.mX + 2, aButtonRect.mY + aOffsetY);
			}
		}
		else
		{
			GetZenButtonRect(aTool, aButtonRect);
			g->DrawImage(Sexy::IMAGE_SHOVELBANK, aButtonRect.mX, aButtonRect.mY + aOffsetY);
			if (static_cast<int>(mCursorObject->mCursorType) == static_cast<int>(CursorType::CURSOR_TYPE_WATERING_CAN) + static_cast<int>(aTool) - 6)
			{
				continue;  // skip drawing a tool currently held by the cursor
			}

			switch (aTool)
			{
			case GameObjectType::OBJECT_TYPE_WATERING_CAN:
				if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_GOLD_WATERINGCAN])
				{
					g->DrawImage(Sexy::IMAGE_WATERINGCANGOLD, aButtonRect.mX - 2, aButtonRect.mY + aOffsetY - 6);
				}
				else
				{
					g->DrawImage(Sexy::IMAGE_WATERINGCAN, aButtonRect.mX - 2, aButtonRect.mY + aOffsetY - 6);
				}
				break;
			case GameObjectType::OBJECT_TYPE_FERTILIZER:
			{
				uint32_t aPurchase = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FERTILIZER];
				int aCharges = aPurchase > PURCHASE_COUNT_OFFSET ? aPurchase - PURCHASE_COUNT_OFFSET : 0;
				if (aCharges == 0)
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(96, 96, 96));
				}
				else if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_FERTILIZE_PLANTS)
				{
					g->SetColorizeImages(true);
					g->SetColor(GetFlashingColor(mMainCounter, 75));
				}
				g->DrawImage(Sexy::IMAGE_FERTILIZER, aButtonRect.mX - 6, aButtonRect.mY + aOffsetY - 7);
				g->SetColorizeImages(false);

				std::string aChargeString = std::format("x{}", aCharges);
				PvzpDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
				break;
			}
			case GameObjectType::OBJECT_TYPE_BUG_SPRAY:
			{
				uint32_t aPurchase = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_BUG_SPRAY];
				int aCharges = aPurchase > PURCHASE_COUNT_OFFSET ? aPurchase - PURCHASE_COUNT_OFFSET : 0;
				if (aCharges == 0)
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(128, 128, 128));
				}
				g->DrawImage(Sexy::IMAGE_BUG_SPRAY, aButtonRect.mX, aButtonRect.mY + aOffsetY - 1);
				g->SetColorizeImages(false);

				std::string aChargeString = std::format("x{}", aCharges);
				PvzpDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
				break;
			}
			case GameObjectType::OBJECT_TYPE_PHONOGRAPH:
				g->DrawImage(Sexy::IMAGE_PHONOGRAPH, aButtonRect.mX + 2, aButtonRect.mY + aOffsetY + 2);
				break;
			case GameObjectType::OBJECT_TYPE_CHOCOLATE:
			{
				uint32_t aPurchase = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_CHOCOLATE];
				int aCharges = aPurchase > PURCHASE_COUNT_OFFSET ? aPurchase - PURCHASE_COUNT_OFFSET : 0;
				if (aCharges == 0)
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(128, 128, 128));
				}
				g->DrawImage(Sexy::IMAGE_CHOCOLATE, aButtonRect.mX + 6, aButtonRect.mY + aOffsetY + 4);
				g->SetColorizeImages(false);

				std::string aChargeString = std::format("x{}", aCharges);
				PvzpDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
				break;
			}
			case GameObjectType::OBJECT_TYPE_GLOVE:
				if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE &&
					mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW)
				{
					g->DrawImage(Sexy::IMAGE_ZEN_GARDENGLOVE, aButtonRect.mX - 6, aButtonRect.mY + aOffsetY - 4);
				}
				break;
			case GameObjectType::OBJECT_TYPE_MONEY_SIGN:
				g->DrawImage(Sexy::IMAGE_ZEN_MONEYSIGN, aButtonRect.mX - 5, aButtonRect.mY + aOffsetY - 4);
				break;
			case GameObjectType::OBJECT_TYPE_WHEELBARROW:
				DrawZenWheelBarrowButton(g, aOffsetY);
				break;
			case GameObjectType::OBJECT_TYPE_TREE_FOOD:
			{
				uint32_t aPurchase = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_TREE_FOOD];
				int aCharges = aPurchase > PURCHASE_COUNT_OFFSET ? aPurchase - PURCHASE_COUNT_OFFSET : 0;
				if (aCharges == 0)
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(128, 128, 128));
				}
				if (!mChallenge->TreeOfWisdomCanFeed())
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(128, 128, 128));
				}
				g->DrawImage(Sexy::IMAGE_TREEFOOD, aButtonRect.mX - 6, aButtonRect.mY + aOffsetY - 7);
				g->SetColorizeImages(false);

				std::string aChargeString = std::format("x{}", aCharges);
				PvzpDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
				break;
			}
			default:
				break;
			}
		}
	}
}

void Board::DrawShovel(Graphics* g)
{
	if (mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		if (mShowShovel)
		{
			Rect aShovelRect = GetShovelButtonRect();
			g->DrawImage(Sexy::IMAGE_SHOVELBANK,aShovelRect,Rect(0,0,Sexy::IMAGE_SHOVELBANK->mWidth,Sexy::IMAGE_SHOVELBANK->mHeight));

			if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_SHOVEL)
			{
				if (mChallenge->mChallengeState == (ChallengeState)15)
				{
					g->SetColorizeImages(true);
					g->SetColor(GetFlashingColor(mMainCounter, 75));
				}
				if(aShovelRect.mWidth==48) {
                    auto* image=Sexy::IMAGE_SHOVEL;float scale=std::min(48.0f/image->mWidth,70.0f/image->mHeight);
                    int width=(int)(image->mWidth*scale),height=(int)(image->mHeight*scale);
                    g->DrawImage(image,Rect(aShovelRect.mX+(48-width)/2,aShovelRect.mY+4,width,height),Rect(0,0,image->mWidth,image->mHeight));
                } else g->DrawImage(Sexy::IMAGE_SHOVEL,aShovelRect.mX-7,aShovelRect.mY-3);
				g->SetColorizeImages(false);
			}
			DrawKeyboardHint(g, -1, aShovelRect.mX + 5, aShovelRect.mY + 14);
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		DrawZenButtons(g);
	}
}

void Board::DrawDebugText(Graphics* g)
{
	std::string aText;

	switch (mDebugTextMode)
	{
	case DebugTextMode::DEBUG_TEXT_NONE:
		break;

	case DebugTextMode::DEBUG_TEXT_ZOMBIE_SPAWN:
	{
		int aTime = mZombieCountDownStart - mZombieCountDown;
		float aCountDownFraction = static_cast<float>(aTime) / static_cast<float>(mZombieCountDownStart);

		aText += std::format("ZOMBIE SPAWNING DEBUG\n");
		aText += std::format("CurrentWave: {} of {}\n", mCurrentWave, mNumWaves);
		aText += std::format("TimeSinseLastSpawn: {} {}\n", aTime, aTime > 400 ? "" : "(too soon)");
		aText += std::format("ZombieCountDown: {}/{} ({:.0f}%)\n", mZombieCountDown, mZombieCountDownStart, aCountDownFraction);

		if (mWaveHealth.next != -1)
		{
			int64_t aTotalHealth = TotalZombiesHealthInWave(mCurrentWave - 1);
			int64_t aHealthRange = std::max<int64_t>(mWaveHealth.start - mWaveHealth.next, 1);
			float aHealthFraction = static_cast<float>(mWaveHealth.next - aTotalHealth + aHealthRange) / static_cast<float>(aHealthRange);
			aText += std::format("ZombieHealth: CurZombieHealth {} trigger {} ({:.0f}%)\n", aTotalHealth, mWaveHealth.next, aHealthFraction * 100);
		}
		else
		{
			aText += std::format("ZombieHealth: before first wave\n");
		}

		if (mHugeWaveCountDown > 0)
		{
			aText += std::format("HugeWaveCountDown: {}\n", mHugeWaveCountDown);
		}

		Zombie* aBossZombie = GetBossZombie();
		if (aBossZombie)
		{
			aText += std::format("\nSpawn: {}\n", aBossZombie->mSummonCounter);
			aText += std::format("Stomp: {}\n", aBossZombie->mBossStompCounter);
			aText += std::format("Bungee: {}\n", aBossZombie->mBossBungeeCounter);
			aText += std::format("Head: {}\n", aBossZombie->mBossHeadCounter);
			aText += std::format("Health: {} of {}\n", aBossZombie->mBodyHealth, aBossZombie->mBodyMaxHealth);
		}

		break;
	}

	case DebugTextMode::DEBUG_TEXT_MUSIC:
	{
		aText += std::format("MUSIC DEBUG\n");
		aText += std::format("CurrentWave: {} of {}\n", mCurrentWave, mNumWaves);

		if (mApp->mMusic->mCurMusicFileMain == MusicFile::MUSIC_FILE_NONE)
		{
			aText += std::format("No music");
		}
		else
		{
			aText += std::format("Music Burst: ");

			if (mApp->mMusic->mMusicBurstState == MusicBurstState::MUSIC_BURST_OFF)
			{
				aText += std::format("Off");
			}
			else if (mApp->mMusic->mMusicBurstState == MusicBurstState::MUSIC_BURST_STARTING)
			{
				aText += std::format("Starting {}/{}", mApp->mMusic->mBurstStateCounter, 400);
			}
			else if (mApp->mMusic->mMusicBurstState == MusicBurstState::MUSIC_BURST_ON)
			{
				aText += std::format("On at least until {}/{}", mApp->mMusic->mBurstStateCounter, 800);
			}
			else if (mApp->mMusic->mMusicBurstState == MusicBurstState::MUSIC_BURST_FINISHING)
			{
				aText += std::format("Finishing {}/{}", mApp->mMusic->mBurstStateCounter, 400);
			}

			if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_OFF)
			{
				aText += std::format(", Drums off");
			}
			else if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_ON_QUEUED)
			{
				aText += std::format(", Drums queued on");
			}
			else if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_ON)
			{
				aText += std::format(", Drums on");
			}
			else if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_OFF_QUEUED)
			{
				aText += std::format(", Drums queued off");
			}
			else if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_FADING)
			{
				aText += std::format(", Drums fading off {}/{}", mApp->mMusic->mDrumsStateCounter, 50);
			}
			aText += std::format("\n");
		}

		break;
	}

	case DebugTextMode::DEBUG_TEXT_MEMORY:
		aText += std::format("MEMORY DEBUG\n");
		aText += std::format("attachments {}\n", mApp->mEffectSystem->mAttachmentHolder->mAttachments.mSize);
		aText += std::format("emitters {}\n", mApp->mEffectSystem->mParticleHolder->mEmitters.mSize);
		aText += std::format("particles {}\n", mApp->mEffectSystem->mParticleHolder->mParticles.mSize);
		aText += std::format("particle systems {}\n", mApp->mEffectSystem->mParticleHolder->mParticleSystems.mSize);
		aText += std::format("trails {}\n", mApp->mEffectSystem->mTrailHolder->mTrails.mSize);
		aText += std::format("reanimation {}\n", mApp->mEffectSystem->mReanimationHolder->mReanimations.mSize);
		aText += std::format("zombies {}\n", mZombies.mSize);
		aText += std::format("plants {}\n", mPlants.mSize);
		aText += std::format("projectiles {}\n", mProjectiles.mSize);
		aText += std::format("coins {}\n", mCoins.mSize);
		aText += std::format("lawn mowers {}\n", mLawnMowers.mSize);
		aText += std::format("grid items {}\n", mGridItems.mSize);
		break;

	case DebugTextMode::DEBUG_TEXT_COLLISION:
		aText += std::format("COLLISION DEBUG\n");
		break;

	default:
		PVZP_ASSERT(false);
		break;
	}

	g->SetFont(FONT_PICO129);
	if (!aText.empty())
	{
		g->SetColor(Color::Black);
		g->DrawStringWordWrapped(aText, 10, 89);
		g->DrawStringWordWrapped(aText, 11, 91);
		g->DrawStringWordWrapped(aText, 9, 90);
		g->DrawStringWordWrapped(aText, 11, 90);
		g->SetColor(Color(255, 255, 255));
		g->DrawStringWordWrapped(aText, 10, 90);
	}
}

void Board::DrawDebugObjectRects(Graphics* g)
{
	if (mDebugTextMode != DebugTextMode::DEBUG_TEXT_COLLISION)
		return;

	{
		for (Plant* aPlant : mPlants)
		{
			if (aPlant->mDead)
				continue;
			Rect aRect = aPlant->GetPlantRect();
			g->SetColor(Color(0, 255, 0));
			g->DrawRect(aRect);

			Rect aAttackRect = aPlant->GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
			if (aAttackRect.mWidth < BOARD_WIDTH)
			{
				g->SetColor(Color(255, 0, 0));
				g->DrawRect(aAttackRect);
			}

			Rect aSecondaryRect = aPlant->GetPlantAttackRect(PlantWeapon::WEAPON_SECONDARY);
			if (aSecondaryRect.mWidth < BOARD_WIDTH)
			{
				g->SetColor(Color(255, 0, 128));
				g->DrawRect(aSecondaryRect);
			}
		}
	}
	{
		for (Zombie* aZombie : mZombies)
		{
			if (aZombie->mDead)
				continue;
			if (!aZombie->IsDeadOrDying())
			{
				Rect aRect = aZombie->GetZombieRect();
				g->SetColor(Color(0, 255, 0));
				g->DrawRect(aRect);

				Rect aAttackRect = aZombie->GetZombieAttackRect();
				g->SetColor(Color(255, 0, 0));
				g->DrawRect(aAttackRect);
			}
		}
	}
	{
		for (LawnMower* aLawnMower : mLawnMowers)
		{
			if (aLawnMower->mDead)
				continue;
			Rect aAttackRect = aLawnMower->GetLawnMowerAttackRect();
			g->SetColor(Color(255, 0, 0));
			g->DrawRect(aAttackRect);
		}
	}
	{
		for (Projectile* aProjectile : mProjectiles)
		{
			if (aProjectile->mDead)
				continue;
			g->SetColor(Color(255, 0, 0));
			Rect aDamageRect = aProjectile->GetProjectileRect();
			g->DrawRect(aDamageRect);
		}
	}
}

void Board::DrawFadeOut(Graphics* g)
{
	if (mBoardFadeOutCounter < 0 || IsSurvivalStageWithRepick())
		return;

	int anAlpha = PvzpAnimateCurve(200, 0, mBoardFadeOutCounter, 0, 255, PvzpCurves::CURVE_LINEAR);
	if (mLevel == 9 || mLevel == 19 || mLevel == 29 || mLevel == 39 || mLevel == 49)
	{
		g->SetColor(Color(0, 0, 0, anAlpha));
	}
	else
	{
		g->SetColor(Color(255, 255, 255, anAlpha));
	}
	g->FillRect(0, 0, mWidth, mHeight);
}

void Board::DrawTopRightUI(Graphics* g)
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		if (mChallenge->mChallengeState == STATECHALLENGE_ZEN_FADING)
		{
			mMenuButton->mY = PvzpAnimateCurve(50, 0, mChallenge->mChallengeStateCounter, -10, -50, PvzpCurves::CURVE_EASE_IN_OUT);
			mStoreButton->mX = PvzpAnimateCurve(50, 0, mChallenge->mChallengeStateCounter, 678, 800, PvzpCurves::CURVE_EASE_IN_OUT);
		}
		else
		{
			mMenuButton->mY = -10;
			mStoreButton->mX = 678;
		}
	}

	if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_COMPLETED)
	{
		g->SetColorizeImages(true);
		g->SetColor(GetFlashingColor(mMainCounter, 75));
	}
	mMenuButton->Draw(g);
	g->SetColorizeImages(false);

	if (mStoreButton && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_VISIT_STORE)
		{
			g->SetColorizeImages(true);
			g->SetColor(GetFlashingColor(mMainCounter, 75));
		}
		mStoreButton->Draw(g);
		g->SetColorizeImages(false);
	}
}

void Board::DrawUIBottom(Graphics* g)
{
	if (mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		int aWaveTime = std::abs(static_cast<int>((mMainCounter / 8) % 22) - 11);
		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		g->DrawImageCel(Sexy::IMAGE_WAVESIDE, 0, 40, aWaveTime);
		g->DrawImageCel(Sexy::IMAGE_WAVECENTER, 160, 40, aWaveTime);
		g->DrawImageCel(Sexy::IMAGE_WAVECENTER, 320, 40, aWaveTime);
		g->DrawImageCel(Sexy::IMAGE_WAVECENTER, 480, 40, aWaveTime);
		//PvzpDrawImageCelScaled(g, Sexy::IMAGE_WAVESIDE, 800, 40, 0, aWaveTime, -1.0f, 1.0f);
		PvzpDrawImageCelScaled(
			g, Sexy::IMAGE_WAVESIDE, 800, 40, aWaveTime % Sexy::IMAGE_WAVESIDE->mNumCols,
			aWaveTime / Sexy::IMAGE_WAVESIDE->mNumCols, -1.0f, 1.0f
		);
		g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
	}

	if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE || mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		g->DrawImage(
			IMAGE_BACKGROUND_GREENHOUSE_OVERLAY,
			Rect(0, 0, BOARD_WIDTH, BOARD_HEIGHT),
			Rect(0, 0, IMAGE_BACKGROUND_GREENHOUSE_OVERLAY->mWidth, IMAGE_BACKGROUND_GREENHOUSE_OVERLAY->mHeight)
		);
		g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
	}

	if (mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON)
	{
		if (mSeedBank->BeginDraw(g))
		{
			mSeedBank->Draw(g);
			mSeedBank->EndDraw(g);
		}

		if (mAdvice->mMessageStyle == MessageStyle::MESSAGE_STYLE_SLOT_MACHINE)
		{
			mAdvice->Draw(g);
		}
	}

	DrawShovel(g);
	if (!StageHasFog())
	{
		DrawTopRightUI(g);
	}
}

void Board::DrawUICoinBank(Graphics* g)
{
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING && mApp->mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_OFF)
		return;

	if (mCoinBankFadeCount <= 0)
		return;

	int aPosX = 57;
	int aPosY = 599 - Sexy::IMAGE_COINBANK->GetHeight();
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF)
	{
		aPosX = 450 - mX;
	}

	g->SetColorizeImages(true);
	int anAlpha = std::clamp(255 * mCoinBankFadeCount / 15, 0, 255);
	g->SetColor(Color(255, 255, 255, anAlpha));
	g->DrawImage(Sexy::IMAGE_COINBANK, aPosX, aPosY);

	g->SetColor(Color(180, 255, 90, anAlpha));
	g->SetFont(Sexy::FONT_CONTINUUMBOLD14);
	std::string aCoinLabel = mApp->GetMoneyString(mApp->mPlayerInfo->mCoins);
	g->DrawString(aCoinLabel, aPosX + 116 - Sexy::FONT_CONTINUUMBOLD14->StringWidth(aCoinLabel), aPosY + 24);
	g->SetColorizeImages(false);
}

void Board::ClearFogAroundPlant(Plant* thePlant, int theSize)
{
	//int aFogFadeOutSpeed = mFogBlownCountDown >= 2000 ? 40 : mFogBlownCountDown > 0 ? 2 : 6;
	int aFogFadeOutSpeed = 6;
	if (mFogBlownCountDown > 0 && mFogBlownCountDown < 2000)
	{
		aFogFadeOutSpeed = 2;
	}
	else if (mFogBlownCountDown > 0)
	{
		aFogFadeOutSpeed = 40;
	}

	int aLeft = LeftFogColumn();
	int aFogOffsetX = (mFogOffset + 50) / 100;
	int aStartX = thePlant->mPlantCol - theSize - aFogOffsetX;
	int aEndX = thePlant->mPlantCol + theSize - aFogOffsetX;
	aStartX = std::max(aStartX, aLeft);
	aEndX = std::min(aEndX, MAX_GRID_SIZE_X - 1);

	int aStartY = thePlant->mRow - theSize;
	int aEndY = thePlant->mRow + theSize;
	aStartY = std::max(aStartY, 0);
	aEndY = std::min(aEndY, MAX_GRID_SIZE_Y);

	for (int x = aStartX; x <= aEndX; x++)
	{
		for (int y = aStartY; y <= aEndY; y++)
		{
			int aDistX = abs(x + aFogOffsetX - thePlant->mPlantCol);
			int aDistY = abs(y - thePlant->mRow);
			if (theSize == 4)
			{
				if (aDistX > 3 || aDistY > 2)
				{
					continue;
				}
				if (aDistX + aDistY == 5)
				{
					continue;
				}
			}
			else if (aDistX + aDistY > theSize)
			{
				continue;
			}

			mGridCelFog[x][y] = std::max(mGridCelFog[x][y] - aFogFadeOutSpeed, 0);
		}
	}
}

void Board::UpdateFog()
{
	if (!StageHasFog())
		return;

	//int aFogFadeInSpeed = mFogBlownCountDown >= 2000 ? 20 : mFogBlownCountDown > 0 ? 1 : 3;
	int aFogFadeInSpeed = 3;
	if (mFogBlownCountDown > 0 && mFogBlownCountDown < 2000)
	{
		aFogFadeInSpeed = 1;
	}
	else if (mFogBlownCountDown > 0)
	{
		aFogFadeInSpeed = 20;
	}

	int aLeft = LeftFogColumn();
	for (int x = aLeft; x < MAX_GRID_SIZE_X; x++)
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y + 1; y++)
		{
			int aFogMax = x == aLeft ? 200 : 255;
			mGridCelFog[x][y] = std::min(mGridCelFog[x][y] + aFogFadeInSpeed, aFogMax);
		}
	}

	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->NotOnGround())
			continue;

		if (aPlant->mSeedType == SeedType::SEED_PLANTERN)
		{
			// @pvz-online: 单株升级「Lantern Light」（路灯花，批 14）：迷雾关照亮范围每层
			// +1 格——半径 4 起、最大 6；形状逐层膨胀在 ClearFogAroundPlant 内。
			ClearFogAroundPlant(aPlant, 4 + mApp->RunPlantUpgradeCount(SeedType::SEED_PLANTERN));
		}
		else if (aPlant->mSeedType == SeedType::SEED_TORCHWOOD)
		{
			ClearFogAroundPlant(aPlant, 1);
		}
	}
}

void Board::DrawFog(Graphics* g)
{
	bool aIs3DAccelerated = mApp->Is3DAccelerated();
	Image* aImageFog = aIs3DAccelerated ? Sexy::IMAGE_FOG : Sexy::IMAGE_FOG_SOFTWARE;
	// the fog animation uses 900- and 500-frame periods; mod by their lcm (4500) to avoid float precision loss on large counters
	constexpr uint32_t FOG_ANIM_PERIOD = 4500;
	float aTime = static_cast<float>(mMainCounter % FOG_ANIM_PERIOD) * PI * 2;
	g->SetColorizeImages(true);
	for (int x = 0; x < MAX_GRID_SIZE_X; x++)
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y + 1; y++)
		{
			int aFadeAmount = mGridCelFog[x][y];
			if (aFadeAmount == 0)
				continue;

			// fog shape of the cell; the extra row 6 reuses row 0's shape
			int aCelLook = mGridCelLook[x][y % MAX_GRID_SIZE_Y];
			int aCelCol = aCelLook % 8;
			float aPosX = x * 80 + mFogOffset - 15;
			float aPosY = y * 85 + 20;
			float aPhaseX = 6 * PI * x / MAX_GRID_SIZE_X;
			float aPhaseY = 6 * PI * y / (MAX_GRID_SIZE_Y + 1);
			float aMotion = 13 + 4 * sin(aTime / 900 + aPhaseY) + 8 * sin(aTime / 500 + aPhaseX);

			int aColorVariant = 255 - aCelLook * 1.5 - aMotion * 1.5;
			int aLightnessVariant = 255 - aCelLook - aMotion;
			if (!aIs3DAccelerated)
			{
				aPosX += 10;
				aPosY += 3;
				aCelCol = aCelLook % Sexy::IMAGE_FOG_SOFTWARE->mNumCols;
				aColorVariant = 255;
				aLightnessVariant = 255;
			}

			g->SetColor(Color(aColorVariant, aColorVariant, aLightnessVariant, aFadeAmount));
			g->DrawImageCel(aImageFog, aPosX, aPosY, aCelCol, 0);

			if (x == MAX_GRID_SIZE_X - 1)
			{
				g->DrawImageCel(aImageFog, aPosX + 80, aPosY, aCelCol, 0);
			}
		}
	}
	g->SetColorizeImages(false);
}

bool Board::IsScaryPotterDaveTalking()
{
	return mApp->IsScaryPotterLevel() && mNextSurvivalStageCounter > 0 && mApp->mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF;
}

void Board::DrawUITop(Graphics* g)
{
	if (StageHasFog())
	{
		DrawTopRightUI(g);
	}

	if (mTimeStopCounter > 0)
	{
		g->SetColor(Color(200, 200, 200, 210));
		g->FillRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
	}

	if (mApp->mGameScene == GameScenes::SCENE_PLAYING || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		DrawProgressMeter(g);
		DrawLevel(g);
	}
	if (mStoreButton && mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		mStoreButton->Draw(g);
	}

	if ((mApp->mGameMode == GameMode::GAMEMODE_UPSELL || mApp->mGameMode == GameMode::GAMEMODE_INTRO) && mCutScene->mUpsellHideBoard)
	{
		g->SetColor(Color(0, 0, 0));
		g->FillRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_UPSELL)
	{
		mCutScene->DrawUpsell(g);
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_INTRO)
	{
		mCutScene->DrawIntro(g);
	}

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		IsScaryPotterDaveTalking())
	{
		Graphics aScreenSpace(*g);
		aScreenSpace.mTransX -= mX;
		aScreenSpace.mTransY -= mY;
		mApp->DrawCrazyDave(&aScreenSpace);
	}

	if (mAdvice->mMessageStyle != MessageStyle::MESSAGE_STYLE_SLOT_MACHINE)
	{
		mAdvice->Draw(g);
	}

	if (mTimeStopCounter == 0 && mCursorObject->BeginDraw(g))
	{
		mCursorObject->Draw(g);
		mCursorObject->EndDraw(g);
	}

	mToolTip->Draw(g);
	DrawDebugText(g);
	DrawDebugObjectRects(g);
}

// Full observation owns the lawn surface; local child buttons must not
// repaint over the remote picture. Keep the normal modal and clipping rules.
void Board::DrawAll(Sexy::ModalFlags* flags,Sexy::Graphics* g){
 if(mApp->mOnlineSession&&mApp->mOnlineSession->IsWatchingV2()&&mMobileWatchOpen){
  if(mPriority>mWidgetManager->mMinDeferredOverlayPriority)mWidgetManager->FlushDeferredOverlayWidgets(mPriority);
  Sexy::AutoModalFlags scoped(flags,mWidgetFlagsMod);
  Sexy::Graphics clipped(*g);
  if(mClip&&(flags->GetFlags()&Sexy::WIDGETFLAGS_CLIP))clipped.ClipRect(0,0,mWidth,mHeight);
  if(flags->GetFlags()&Sexy::WIDGETFLAGS_DRAW)Draw(&clipped);
  return;
 }
 Sexy::Widget::DrawAll(flags,g);
}

void Board::Draw(Graphics* g)
{
	if (mApp->GetDialog(Dialogs::DIALOG_STORE) || mApp->GetDialog(Dialogs::DIALOG_ALMANAC))
		return;

	g->SetLinearBlend(true);

	if (mDrawCount && mCutScene->mPreloaded)
	{
		int64_t aTickCount = SDL_GetTicks();
		int64_t aIntervalDraws = mDrawCount - mIntervalDrawCountStart;
		int64_t aInterval = aTickCount - mIntervalDrawTime;
		if (aInterval > 10000)
		{
			float aIntervalFPS = (aIntervalDraws * 1000 + 500) / aInterval;
			if (mMinFPS > aIntervalFPS)
			{
				mMinFPS = aIntervalFPS;
			}
			mIntervalDrawCountStart = mDrawCount;
			mIntervalDrawTime = aTickCount;
		}
	}
	else
	{
		ResetFPSStats();
	}

	mDrawCount++;
	DrawGameObjects(g);
    if (gShowZombieHealthNumbers)
        for (Zombie* zombie : mZombies) {
            if (zombie->mDead || zombie->IsDeadOrDying()) continue;
            const Rect rect = zombie->GetZombieRect(); int line = 0;
            DrawHealthNumberOverRect(g, zombie->mBodyHealth, rect, line++);
            if (zombie->mHelmHealth > 0) DrawHealthNumberOverRect(g, zombie->mHelmHealth, rect, line++);
            if (zombie->mShieldHealth > 0) DrawHealthNumberOverRect(g, zombie->mShieldHealth, rect, line++);
            if (zombie->mFlyingHealth > 0) DrawHealthNumberOverRect(g, zombie->mFlyingHealth, rect, line++);
        }
    if (gShowPlantHealthNumbers)
        for (Plant* plant : mPlants)
            if (!plant->mDead) DrawHealthNumberOverRect(g, plant->mPlantHealth, plant->GetPlantRect());
    DrawRelayedZombieMarkers(g);
    DrawCoopUI(g);
    if (mMobileWatchOpen) DrawMobileWatch(g);
}

void Board::SetMustacheMode(bool theEnableMustache)
{
	mApp->PlayFoley(FoleyType::FOLEY_POLEVAULT);
	mMustacheMode = theEnableMustache;
	mApp->mMustacheMode = theEnableMustache;

	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		aZombie->EnableMustache(theEnableMustache);
	}
}

void Board::SetFutureMode(bool theEnableFuture)
{
	mApp->PlaySample(Sexy::SOUND_BOING);
	mFutureMode = theEnableFuture;
	mApp->mFutureMode = theEnableFuture;

	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		aZombie->EnableFuture(theEnableFuture);
	}
}

void Board::SetPinataMode(bool theEnablePinata)
{
	mApp->PlayFoley(FoleyType::FOLEY_JUICY);
	mPinataMode = theEnablePinata;
	mApp->mPinataMode = theEnablePinata;
}

void Board::SetDanceMode(bool theEnableDance)
{
	mApp->PlayFoley(FoleyType::FOLEY_DANCER);
	mDanceMode = theEnableDance;
	mApp->mDanceMode = theEnableDance;

	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		aZombie->EnableDance();
	}
}

void Board::SetSuperMowerMode(bool theEnableSuperMower)
{
	mApp->PlayFoley(FoleyType::FOLEY_ZAMBONI);
	mSuperMowerMode = theEnableSuperMower;
	mApp->mSuperMowerMode = theEnableSuperMower;

	for (LawnMower* aLawnMower : mLawnMowers)
	{
		if (aLawnMower->mDead)
			continue;
		aLawnMower->EnableSuperMower(theEnableSuperMower);
	}
}

void Board::SetDaisyMode(bool theEnableDaisy)
{
	mApp->PlaySample(SOUND_LOADINGBAR_FLOWER);
	mDaisyMode = theEnableDaisy;
	mApp->mDaisyMode = theEnableDaisy;
}

void Board::SetSukhbirMode(bool theEnableSukhbir)
{
	mApp->PlaySample(Sexy::SOUND_SUKHBIR);
	mSukhbirMode = theEnableSukhbir;
	mApp->mSukhbirMode = theEnableSukhbir;
}

void Board::DoTypingCheck(KeyCode theKey)
{
	if (mApp->mKonamiCheck->Check(theKey))
	{
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		return;
	}
	if (mApp->mMustacheCheck->Check(theKey) || mApp->mMoustacheCheck->Check(theKey))
	{
		SetMustacheMode(!mMustacheMode);
		ReportAchievement::GiveAchievement(mApp, MustacheMode, true);
		return;
	}
	if (mApp->mSuperMowerCheck->Check(theKey) || mApp->mSuperMowerCheck2->Check(theKey))
	{
		SetSuperMowerMode(!mSuperMowerMode);
		return;
	}
	if (mApp->mFutureCheck->Check(theKey))
	{
		SetFutureMode(!mFutureMode);
		return;
	}
	if (mApp->mPinataCheck->Check(theKey))
	{
		if (mApp->CanDoPinataMode())
		{
			SetPinataMode(!mPinataMode);
			return;
		}
		else
		{
			if (mApp->mGameScene == GameScenes::SCENE_PLAYING)
			{
				DisplayAdvice("[CANT_USE_CODE]", MessageStyle::MESSAGE_STYLE_BIG_MIDDLE_FAST, AdviceType::ADVICE_NONE);
			}
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}
	if (mApp->mDanceCheck->Check(theKey))
	{
		if (mApp->CanDoDanceMode())
		{
			SetDanceMode(!mDanceMode);
			return;
		}
		else
		{
			if (mApp->mGameScene == GameScenes::SCENE_PLAYING)
			{
				DisplayAdvice("[CANT_USE_CODE]", MessageStyle::MESSAGE_STYLE_BIG_MIDDLE_FAST, AdviceType::ADVICE_NONE);
			}
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}
	if (mApp->mDaisyCheck->Check(theKey))
	{
		if (mApp->CanDoDaisyMode())
		{
			SetDaisyMode(!mDaisyMode);
			return;
		}
		else
		{
			if (mApp->mGameScene == GameScenes::SCENE_PLAYING)
			{
				DisplayAdvice("[CANT_USE_CODE]", MessageStyle::MESSAGE_STYLE_BIG_MIDDLE_FAST, AdviceType::ADVICE_NONE);
			}
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}
	if (mApp->mSukhbirCheck->Check(theKey))
	{
		SetSukhbirMode(!mSukhbirMode);
		return;
	}
}

void Board::KeyUp(KeyCode theKey) { if(mApp->TeamProgressionInputBlocked())return;NextWaveControl::KeyUp(this,theKey); }

void Board::KeyDown(KeyCode theKey)
{
    if(mApp->TeamProgressionInputBlocked())return;
    if (mApp->mOnlineSession && mApp->mOnlineSession->RelayBattleHeld()) { mApp->DoOnlineDialog(); return; }
    if(NextWaveControl::KeyDown(this,theKey))return;
    // @pvz-online: use uppercase VK letters; lowercase ASCII aliases collide with F5/F7/NUMPAD2.
    if((theKey=='B') && mApp->IsOnlineGame()){mCoopToolsOpen=false;CloseQuickChatPanel();mSunGiftSelectFrames=0;mApp->ShowCoopPlay();return;}
    // Cooperation gets first choice of normalized digits.
    if (theKey >= KEYCODE_NUMPAD0 && theKey <= KEYCODE_NUMPAD9)
        theKey = static_cast<KeyCode>('0' + theKey - KEYCODE_NUMPAD0);
    if (((theKey >= '0' && theKey <= '9') || theKey == 'Q' ||
        theKey == 'H' || theKey == 'J') && mWidgetManager->mIsKeyRepeat)
        return; // A held panel choice must not become a plant after the panel closes.
    if (mMobileWatchOpen) {
        if (theKey == KEYCODE_ESCAPE || theKey == 'V') StopMobileWatch();
        else if (theKey == KEYCODE_RIGHT) CycleMobileWatch();
        return;
    }
    if (theKey == 'V') { if (QuickChatAvailable()) { CycleMobileWatch(); return; } }
    if (HandleQuickChatKey(theKey) || HandleSunGiftKey(theKey)) return;
    if (mCoopToolsOpen) { if(theKey==KEYCODE_ESCAPE)mCoopToolsOpen=false; return; }
    if (HandlePlantKeyboardKey(theKey)) return;
    if ((theKey == 'H' || theKey == 'J') && mApp->mGameScene == GameScenes::SCENE_PLAYING &&
        mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN &&
        mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM) {
        mKeyboardControlsUsed = true;
        if (theKey == 'H') gShowZombieHealthNumbers = !gShowZombieHealthNumbers;
        else gShowPlantHealthNumbers = !gShowPlantHealthNumbers;
        MarkDirty(); return;
    }
	DoTypingCheck(theKey);

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO &&
		mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN &&
		mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mCutScene->KeyDown(theKey);
	}
	else if (theKey == KeyCode::KEYCODE_RETURN || theKey == KeyCode::KEYCODE_SPACE)
	{
		if (IsScaryPotterDaveTalking() && mApp->mCrazyDaveMessageIndex != -1)
		{
			mChallenge->AdvanceCrazyDaveDialog();
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		{
			mApp->mZenGarden->AdvanceCrazyDaveDialog();
		}
		else if (theKey == KeyCode::KEYCODE_SPACE && mApp->CanPauseNow())
		{
			mApp->PlaySample(Sexy::SOUND_PAUSE);
			mApp->DoPauseDialog();
		}
	}
	else if (theKey == KeyCode::KEYCODE_ESCAPE)
	{
		if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_NORMAL)
		{
			RefreshSeedPacketFromCursor();
		}
		else if (CanInteractWithBoardButtons() && mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON)
		{
			mApp->DoNewOptions(false);
		}
	}
}

static void PvzpCrash()
{
	PVZP_ASSERT(false, "Crash{}", "!!!!");
}

void Board::KeyChar(char theChar)
{
    if(mApp->TeamProgressionInputBlocked())return;
    if(mApp->IsRunMode() && (theChar=='n' || theChar=='N'))return;
    // @pvz-online: H/J must not also reach the debug zombie-spawn character handlers.
    if ((theChar == 'h' || theChar == 'H' || theChar == 'j' || theChar == 'J') &&
        mApp->mGameScene == GameScenes::SCENE_PLAYING &&
        mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN &&
        mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM) return;
    // SDL sends KeyChar after KeyDown too; reserve battle inputs even with debug enabled.
    if (HasPlantKeyboardControls() &&
        ((theChar >= '0' && theChar <= '9') || theChar == 'q' || theChar == 'Q')) return;
	if (!mApp->mDebugKeysEnabled)
		return;

	PvzpLogLn("Board cheat key '{}'", theChar);

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		if (theChar == 'm')
		{
			if (!mApp->mZenGarden->IsZenGardenFull(true))
			{
				PottedPlant aPottedPlant;
				aPottedPlant.InitializePottedPlant(SeedType::SEED_MARIGOLD);
				aPottedPlant.mDrawVariation = static_cast<DrawVariation>(RandRangeInt(static_cast<int>(DrawVariation::VARIATION_MARIGOLD_WHITE), static_cast<int>(DrawVariation::VARIATION_MARIGOLD_LIGHT_GREEN)));
				mApp->mZenGarden->AddPottedPlant(&aPottedPlant);
			}
			return;
		}

		if (theChar == '+')
		{
			if (!mApp->mZenGarden->IsZenGardenFull(true))
			{
				PottedPlant aPottedPlant;
				aPottedPlant.InitializePottedPlant(mApp->mZenGarden->PickRandomSeedType());
				mApp->mZenGarden->AddPottedPlant(&aPottedPlant);
			}
			return;
		}

		if (theChar == 'a')
		{
			if (!mApp->mZenGarden->IsZenGardenFull(true))
			{
				PottedPlant aPottedPlant;
				aPottedPlant.InitializePottedPlant(mApp->mZenGarden->PickRandomSeedType());
				aPottedPlant.mPlantAge = PottedPlantAge::PLANTAGE_FULL;
				mApp->mZenGarden->AddPottedPlant(&aPottedPlant);
			}
			return;
		}

		if (theChar == 'f')
		{
			for (Plant* aPlant : mPlants)
			{
				if (aPlant->mDead)
					continue;
				if (GetZenToolAt(aPlant->mPlantCol, aPlant->mRow) == nullptr && aPlant->mPottedPlantIndex >= 0)
				{
					PottedPlant* aPottedPlant = mApp->mZenGarden->PottedPlantFromIndex(aPlant->mPottedPlantIndex);
					PottedPlantNeed aNeed = mApp->mZenGarden->GetPlantsNeed(aPottedPlant);
					if (aNeed == PottedPlantNeed::PLANTNEED_WATER)
					{
						aPlant->mHighlighted = true;
						mApp->mZenGarden->MouseDownWithFeedingTool(aPlant->mX, aPlant->mY, CursorType::CURSOR_TYPE_WATERING_CAN);
						return;
					}
					else if (aNeed == PottedPlantNeed::PLANTNEED_FERTILIZER)
					{
						aPlant->mHighlighted = true;
						if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FERTILIZER] <= PURCHASE_COUNT_OFFSET)
						{
							mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FERTILIZER] = PURCHASE_COUNT_OFFSET + 1;
						}
						mApp->mZenGarden->MouseDownWithFeedingTool(aPlant->mX, aPlant->mY, CursorType::CURSOR_TYPE_FERTILIZER);
						return;
					}
					else if (aNeed == PottedPlantNeed::PLANTNEED_BUGSPRAY)
					{
						aPlant->mHighlighted = true;
						if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_BUG_SPRAY] <= PURCHASE_COUNT_OFFSET)
						{
							mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_BUG_SPRAY] = PURCHASE_COUNT_OFFSET + 1;
						}
						mApp->mZenGarden->MouseDownWithFeedingTool(aPlant->mX, aPlant->mY, CursorType::CURSOR_TYPE_BUG_SPRAY);
						return;
					}
					else if (aNeed == PottedPlantNeed::PLANTNEED_PHONOGRAPH)
					{
						aPlant->mHighlighted = true;
						mApp->mZenGarden->MouseDownWithFeedingTool(aPlant->mX, aPlant->mY, CursorType::CURSOR_TYPE_PHONOGRAPH);
						return;
					}
				}
			}
			return;
		}

		if (theChar == 'r')
		{
			for (Plant* aPlant : mPlants)
			{
				if (aPlant->mDead)
					continue;
				if (aPlant->mPottedPlantIndex >= 0)
				{
					PVZP_ASSERT(aPlant->mPottedPlantIndex < mApp->mPlayerInfo->mNumPottedPlants);
					PottedPlant* aPottedPlant = &mApp->mPlayerInfo->mPottedPlant[aPlant->mPottedPlantIndex];
					mApp->mZenGarden->ResetPlantTimers(aPottedPlant);
				}
			}
			return;
		}

		if (theChar == 's')
		{
			if (mApp->mZenGarden->IsStinkySleeping())
			{
				mApp->mZenGarden->WakeStinky();
			}
			else
			{
				mApp->mZenGarden->ResetStinkyTimers();
			}
			return;
		}

		if (theChar == 'c')
		{
			if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_CHOCOLATE] < PURCHASE_COUNT_OFFSET)
			{
				mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_CHOCOLATE] = PURCHASE_COUNT_OFFSET + 1;
			}
			else
			{
				mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_CHOCOLATE]++;
			}
			return;
		}

		if (theChar == ']')
		{
			PottedPlant* aPottedPlant = mApp->mZenGarden->GetPottedPlantInWheelbarrow();
			if (aPottedPlant)
			{
				aPottedPlant->mSeedType = static_cast<SeedType>(static_cast<int>(aPottedPlant->mSeedType) + 1);
				if (aPottedPlant->mSeedType == SeedType::SEED_GATLINGPEA)
				{
					aPottedPlant->mSeedType = SeedType::SEED_PEASHOOTER;
				}
				if (aPottedPlant->mSeedType == SeedType::SEED_FLOWERPOT)
				{
					aPottedPlant->mSeedType = SeedType::SEED_KERNELPULT;
				}
			}
			return;
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		if (theChar == 'f')
		{
			if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_TREE_FOOD] <= PURCHASE_COUNT_OFFSET)
			{
				mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_TREE_FOOD] = PURCHASE_COUNT_OFFSET + 1;
			}
			mChallenge->TreeOfWisdomFertilize();
		}
		else if (theChar == 'g')
		{
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == 'b')
		{
			mChallenge->mChallengeStateCounter = 1;
		}
		else if (theChar == '0')
		{
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = 0;
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == '1')
		{
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = 9;
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == '2')
		{
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = 19;
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == '3')
		{
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = 29;
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == '4')
		{
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = 39;
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == '5')
		{
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = 49;
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == '6')
		{
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = 98;
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == '7')
		{
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = 498;
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == '8')
		{
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = 998;
			mChallenge->TreeOfWisdomGrow();
		}

		return;
	}

	if (theChar == '<')
	{
		mApp->DoNewOptions(false);
	}
	else if (theChar == 'l')
	{
		mApp->DoCheatDialog();
	}
	else if (theChar == '#')
	{
		if (mApp->IsSurvivalMode())
		{
			if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
			{
				return;
			}

			mCurrentWave = mNumWaves;
			mChallenge->mSurvivalStage += 5;
			RemoveAllZombies();
			FadeOutLevel();
		}
	}
	else if (theChar == '!')
	{
		mApp->mBoardResult = BoardResult::BOARDRESULT_CHEAT;
		if (IsLastStandStageWithRepick())
		{
			if (mNextSurvivalStageCounter == 0)
			{
				mCurrentWave = mNumWaves;
				RemoveAllZombies();
				FadeOutLevel();
			}
		}
		else if ((mApp->IsScaryPotterLevel() && !IsFinalScaryPotterStage()) || mApp->IsEndlessIZombie(mApp->mGameMode))
		{
			if (mNextSurvivalStageCounter == 0)
			{
				RemoveAllZombies();
				FadeOutLevel();
			}
		}
		else if (mApp->IsSurvivalMode())
		{
			if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
			{
				return;
			}

			mCurrentWave = mNumWaves;
			//if (!IsSurvivalStageWithRepick())
			//{
				RemoveAllZombies();
			//}
			FadeOutLevel();
		}
		else
		{
			RemoveAllZombies();
			FadeOutLevel();
			mBoardFadeOutCounter = 200;
		}
	}
	else if (theChar == '+')
	{
		mApp->mBoardResult = BoardResult::BOARDRESULT_CHEAT;
		if (IsLastStandStageWithRepick())
		{
			if (mNextSurvivalStageCounter == 0)
			{
				mCurrentWave = mNumWaves;
				RemoveAllZombies();
				FadeOutLevel();
			}
		}
		else if ((mApp->IsScaryPotterLevel() && !IsFinalScaryPotterStage()) || mApp->IsEndlessIZombie(mApp->mGameMode))
		{
			if (mNextSurvivalStageCounter == 0)
			{
				RemoveAllZombies();
				FadeOutLevel();
			}
		}
		else if (mApp->IsSurvivalEndless(mApp->mGameMode))
		{
			if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
			{
				return;
			}

			mCurrentWave = mNumWaves;
			RemoveAllZombies();
			FadeOutLevel();
		}
		else if (mApp->IsSurvivalMode())
		{
			mChallenge->mSurvivalStage = 5;
			RemoveAllZombies();
			FadeOutLevel();
			mBoardFadeOutCounter = 200;
		}
		else
		{
			RemoveAllZombies();
			FadeOutLevel();
			mBoardFadeOutCounter = 200;
		}
	}
	else if (theChar == '8')
	{
		mApp->mEasyPlantingCheat = !mApp->mEasyPlantingCheat;
	}
	else if (theChar == '7')
	{
		mApp->ToggleSlowMo();
	}
	else if (theChar == '6')
	{
		mApp->ToggleFastMo();
	}
	else if (theChar == 'z')
	{
		mDebugTextMode = static_cast<DebugTextMode>(static_cast<int>(mDebugTextMode) + 1);
		if (mDebugTextMode > DebugTextMode::DEBUG_TEXT_COLLISION)
		{
			mDebugTextMode = DebugTextMode::DEBUG_TEXT_NONE;
		}
	}

	if (mApp->mGameScene != GameScenes::SCENE_PLAYING)
	{
		return;
	}

	Zombie* aBossZombie = GetBossZombie();
	if (aBossZombie && !aBossZombie->IsDeadOrDying())
	{
		if (theChar == 'b')
		{
			aBossZombie->mBossBungeeCounter = 0;
			return;
		}
		if (theChar == 'u')
		{
			aBossZombie->mSummonCounter = 0;
			return;
		}
		if (theChar == 's')
		{
			aBossZombie->mBossStompCounter = 0;
			return;
		}
		if (theChar == 'r')
		{
			aBossZombie->BossRVAttack();
			return;
		}
		if (theChar == 'h')
		{
			aBossZombie->mBossHeadCounter = 0;
			return;
		}
		if (theChar == 'd')
		{
			aBossZombie->TakeDamage(10000, 0U);
			return;
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS_2)
	{
		if (theChar == 'w')
		{
			AddZombie(ZombieType::ZOMBIE_WALLNUT_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
		if (theChar == 't')
		{
			AddZombie(ZombieType::ZOMBIE_TALLNUT_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
		if (theChar == 'j')
		{
			AddZombie(ZombieType::ZOMBIE_JALAPENO_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
		if (theChar == 'g')
		{
			AddZombie(ZombieType::ZOMBIE_GATLING_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
		if (theChar == 's')
		{
			AddZombie(ZombieType::ZOMBIE_SQUASH_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
	}

	if (theChar == 'q')
	{
		if (mApp->IsSurvivalEndless(mApp->mGameMode))
		{
			mApp->mEasyPlantingCheat = true;
			for (int y = 0; y < MAX_GRID_SIZE_X; y++)
			{
				for (int x = 0; x < MAX_GRID_SIZE_Y; x++)
				{
					if (CanPlantAt(x, y, SeedType::SEED_LILYPAD) == PlantingReason::PLANTING_OK)
					{
						AddPlant(x, y, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
					}
					if (CanPlantAt(x, y, SeedType::SEED_PUMPKINSHELL) == PlantingReason::PLANTING_OK)
					{
						if (x <= 6 || IsPoolSquare(x, y))
						{
							AddPlant(x, y, SeedType::SEED_PUMPKINSHELL, SeedType::SEED_NONE);
						}
					}
					if (CanPlantAt(x, y, SeedType::SEED_GATLINGPEA) == PlantingReason::PLANTING_OK)
					{
						if (x < 5)
						{
							AddPlant(x, y, SeedType::SEED_GATLINGPEA, SeedType::SEED_NONE);
						}
						else if (x == 5)
						{
							AddPlant(x, y, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
						}
						else if (x == 6)
						{
							AddPlant(x, y, SeedType::SEED_SPLITPEA, SeedType::SEED_NONE);
						}
						else if (y == 2 || y == 3)
						{
							AddPlant(x, y, SeedType::SEED_GLOOMSHROOM, SeedType::SEED_NONE);
							if (CanPlantAt(x, y, SeedType::SEED_INSTANT_COFFEE) == PlantingReason::PLANTING_OK)
							{
								AddPlant(x, y, SeedType::SEED_INSTANT_COFFEE, SeedType::SEED_NONE);
							}
						}
					}
				}
			}
		}
		else if (mApp->IsIZombieLevel())
		{
			mApp->mEasyPlantingCheat = true;
			for (int i = 0; i < 5; i++)
			{
				mChallenge->IZombiePlaceZombie(ZombieType::ZOMBIE_FOOTBALL, 6, i);
			}
		}
		else
		{
			mApp->mEasyPlantingCheat = true;
			for (int y = 0; y < MAX_GRID_SIZE_Y; ++y)
			{
				for (int x = 0; x < MAX_GRID_SIZE_X; ++x)
				{
					if (StageHasRoof() && CanPlantAt(x, y, SeedType::SEED_FLOWERPOT) == PlantingReason::PLANTING_OK)
					{
						AddPlant(x, y, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
					}
					if (CanPlantAt(x, y, SeedType::SEED_LILYPAD) == PlantingReason::PLANTING_OK)
					{
						AddPlant(x, y, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
					}
					if (CanPlantAt(x, y, SeedType::SEED_THREEPEATER) == PlantingReason::PLANTING_OK)
					{
						AddPlant(x, y, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
					}
				}
			}

			if (!mChallenge->UpdateZombieSpawning())
			{
				int aWavesRemaining = std::min(mNumWaves - mCurrentWave, 20);
				while (aWavesRemaining)
				{
					SpawnZombieWave();
					aWavesRemaining--;
				}
			}

			if (mApp->IsScaryPotterLevel())
			{
				for (GridItem* aGridItem : mGridItems)
				{
					if (aGridItem->mDead)
						continue;
					if (aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT)
					{
						mChallenge->ScaryPotterOpenPot(aGridItem);
					}
				}
			}
		}

		return;
	}

	if (theChar == 'O')
	{
		mApp->mEasyPlantingCheat = true;
		for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
		{
			for (int x = 0; x < 3; x++)
			{
				if (CanPlantAt(x, y, SeedType::SEED_FLOWERPOT) == PlantingReason::PLANTING_OK)
				{
					AddPlant(x, y, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
				}
			}
		}
		return;
	}

	if (theChar == '?' || theChar == '/')
	{
		if (mHugeWaveCountDown > 0)
		{
			mHugeWaveCountDown = 1;
		}
		else
		{
			mZombieCountDown = 6;
		}
		return;
	}

	if (theChar == 'b')
	{
		AddZombie(ZombieType::ZOMBIE_BUNGEE, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'o')
	{
		AddZombie(ZombieType::ZOMBIE_FOOTBALL, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 's')
	{
		AddZombie(ZombieType::ZOMBIE_DOOR, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'L')
	{
		AddZombie(ZombieType::ZOMBIE_LADDER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'y')
	{
		AddZombie(ZombieType::ZOMBIE_YETI, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'a')
	{
		AddZombie(ZombieType::ZOMBIE_FLAG, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'w')
	{
		AddZombie(ZombieType::ZOMBIE_NEWSPAPER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'F')
	{
		AddZombie(ZombieType::ZOMBIE_BALLOON, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'n')
	{
		if (StageHasPool())
		{
			AddZombie(ZombieType::ZOMBIE_SNORKEL, Zombie::ZOMBIE_WAVE_DEBUG);
		}
	}
	if (theChar == 'c')
	{
		AddZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'm')
	{
		AddZombie(ZombieType::ZOMBIE_DANCER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'h')
	{
		AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	//if (theChar == 'H'
	//{
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	return;
	//}
	if (theChar == 'D')
	{
		AddZombie(ZombieType::ZOMBIE_DIGGER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'p')
	{
		AddZombie(ZombieType::ZOMBIE_POLEVAULTER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'P')
	{
		AddZombie(ZombieType::ZOMBIE_POGO, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'R')
	{
		if (StageHasPool())
		{
			AddZombie(ZombieType::ZOMBIE_DOLPHIN_RIDER, Zombie::ZOMBIE_WAVE_DEBUG);
		}
		return;
	}
	else if(theChar == 'j')
	{
		AddZombie(ZombieType::ZOMBIE_JACK_IN_THE_BOX, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'g')
	{
		AddZombie(ZombieType::ZOMBIE_GARGANTUAR, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'G')
	{
		AddZombie(ZombieType::ZOMBIE_REDEYE_GARGANTUAR, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'i')
	{
		AddZombie(ZombieType::ZOMBIE_ZAMBONI, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'C')
	{
		AddZombie(ZombieType::ZOMBIE_CATAPULT, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == '1')
	{
		Plant* aPlant = GetTopPlantAt(0, 0, PlantPriority::TOPPLANT_ANY);
		if (aPlant)
		{
			aPlant->Die();
			mChallenge->ZombieAtePlant(aPlant);
			return;
		}
	}
	if (theChar == 'B')
	{
		mFogBlownCountDown = 2200;
		return;
	}
	if (theChar == 't')
	{
		if (!CanAddBobSled())
		{
			int aRow = Rand(5);
			int aPos = 400;
			if (StageHasPool())
			{
				aRow = Rand(2);
			}
			else if (StageHasRoof())
			{
				aPos = 500;
			}
			mIceTimer[aRow] = 3000;
			mIceMinX[aRow] = aPos;
		}

		AddZombie(ZombieType::ZOMBIE_BOBSLED, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == 'r')
	{
		SpawnZombiesFromGraves();
		return;
	}
	if (theChar == '0')
	{
		AddSunMoney(100);
		mApp->PlaySample(SOUND_BUTTONCLICK);
		return;
	}
	if (theChar == '9')
	{
		AddSunMoney(999999);
		mApp->PlaySample(SOUND_BUTTONCLICK);
		return;
	}
	if (theChar == '$')
	{
		mApp->mPlayerInfo->AddCoins(100);
		mApp->PlaySample(SOUND_BUTTONCLICK);
		ShowCoinBank();
		return;
	}
	if (theChar == '-')
	{
		mSunMoney -= 100;
		mSunMoney = std::max(mSunMoney, 0);
		return;
	}

	if (theChar == '%')
	{
		mApp->SwitchScreenMode(mApp->mIsWindowed, !mApp->Is3DAccelerated(), false);
	}
	if (theChar == 'M')
	{
		mApp->mMusic->mBurstOverride -= 2 - (mApp->mMusic->mBurstOverride != 1);
		return;
	}

	if (theChar == '\3' && mApp->mCtrlDown && mApp->mCheatKeys)
	{
		PvzpCrash();

		if (mHugeWaveCountDown > 0)
		{
			mHugeWaveCountDown = 1;
		}
		else
		{
			mZombieCountDown = 6;
		}
	}
}

void Board::AddSunMoney(int theAmount)
{
    mSunMoney=int(std::clamp<int64_t>(int64_t(mSunMoney)+theAmount,0,mApp->IsRunMode()?INT_MAX:9990));
	if (mSunMoney >= 8000)
		ReportAchievement::GiveAchievement(mApp, SunnyDays, true);
}

int Board::CountSunBeingCollected()
{
	int aCount = 0;
	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead || aCoin->mVisualOnly)
			continue;
		if (aCoin->mIsBeingCollected && aCoin->IsSun())
		{
			aCount += aCoin->GetSunValue();
		}
	}
	return aCount;
}

int Board::CountCoinsBeingCollected()
{
	int aCount = 0;
	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead)
			continue;
		if (aCoin->mIsBeingCollected && aCoin->IsMoney())
		{
			aCount += aCoin->GetCoinValue(aCoin->mType);
		}
	}
	return aCount;
}

bool Board::TakeSunMoney(int theAmount)
{
    if(mApp->AdmissionBuilding()) {
        auto& state=mApp->GetRunState()->mAdmission;
        if(theAmount<0 || theAmount>state.budget)return false;
        state.budget-=theAmount;mApp->mAdmissionDirty=true;return true;
    }
	if (CanTakeSunMoney(theAmount))
	{
		mSunMoney -= theAmount;
        TeamPlay::SpendRestrictedSun(mApp,theAmount);
		return true;
	}

	mApp->PlaySample(Sexy::SOUND_BUZZER);
	mOutOfMoneyCounter = 70;
	return false;
}

bool Board::CanTakeSunMoney(int theAmount)
{
    if(mApp->AdmissionBuilding())return theAmount>=0 && theAmount<=mApp->GetRunState()->mAdmission.budget;
	return theAmount>=0 && int64_t(theAmount) <= int64_t(mSunMoney) + CountSunBeingCollected();
}

void Board::ProcessDeleteQueue()
{
	{
		for (Plant* aPlant : mPlants)
		{
			if (aPlant->mDead)
			{
				mPlants.DataArrayFree(aPlant);
			}
		}
	}
	{
		for (Zombie* aZombie : mZombies)
		{
			if (aZombie->mDead)
			{
				mZombies.DataArrayFree(aZombie);
			}
		}
	}
	{
		for (Projectile* aProjectile : mProjectiles)
		{
			if (aProjectile->mDead)
			{
				mProjectiles.DataArrayFree(aProjectile);
			}
		}
	}
	{
		for (Coin* aCoin : mCoins)
		{
			if (aCoin->mDead)
			{
				mCoins.DataArrayFree(aCoin);
			}
		}
	}
	{
		for (LawnMower* aLawnMower : mLawnMowers)
		{
			if (aLawnMower->mDead)
			{
				mLawnMowers.DataArrayFree(aLawnMower);
			}
		}
	}
	{
		for (GridItem* aGridItem : mGridItems)
		{
			if (aGridItem->mDead)
			{
				mGridItems.DataArrayFree(aGridItem);
			}
		}
	}
}

bool Board::HasConveyorBeltSeedBank()
{
	if (mApp->mRunState) return false;
	return
		mApp->IsFinalBossLevel() ||
		mApp->IsMiniBossLevel() ||
		mApp->IsShovelLevel() ||
		mApp->IsWallnutBowlingLevel() ||
		mApp->IsLittleTroubleLevel() ||
		mApp->IsStormyNightLevel() ||
		mApp->IsBungeeBlitzLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL;
}

int Board::GetNumSeedsInBank()
{
	if (mApp->mRunState) return std::min(RunState::RUN_SEED_SLOTS, (int)mApp->mRunState->GetUsablePool().size());
	if (mApp->IsScaryPotterLevel())
	{
		return 1;
	}
	if (mApp->IsWhackAZombieLevel())
	{
		return 3;
	}
	if (mApp->IsChallengeWithoutSeedBank())
	{
		return 0;
	}
	if (HasConveyorBeltSeedBank())
	{
		return 10;
	}
	if (mApp->IsSlotMachineLevel())
	{
		return 3;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE)
	{
		return 6;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST)
	{
		return 0;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM)
	{
		return 2;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 || mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_2 ||
		mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3 || mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_4)
	{
		return 3;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_5 || mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_6 ||
		mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_7)
	{
		return 4;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_8)
	{
		return 6;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_9)
	{
		return 8;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS)
	{
		return 9;
	}

	int aNumSeeds = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] + 6;
	int aSeedsAvailable = mApp->GetSeedsAvailable();
	return std::min(aNumSeeds, aSeedsAvailable);
}

bool Board::StageIsNight()
{
	return
		mBackground == BackgroundType::BACKGROUND_2_NIGHT ||
		mBackground == BackgroundType::BACKGROUND_4_FOG ||
		mBackground == BackgroundType::BACKGROUND_6_BOSS ||
		mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
		mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM;
}

bool Board::StageHasGraveStones()
{
	if (mApp->IsWallnutBowlingLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_POGO_PARTY ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND ||
		mApp->IsIZombieLevel() ||
		mApp->IsScaryPotterLevel())
		return false;

	return mBackground == BackgroundType::BACKGROUND_2_NIGHT;
}

bool Board::StageHasRoof()
{
	return (mBackground == BackgroundType::BACKGROUND_5_ROOF || mBackground == BackgroundType::BACKGROUND_6_BOSS);
}

bool Board::StageHasPool()
{
	return (mBackground == BackgroundType::BACKGROUND_3_POOL || mBackground == BackgroundType::BACKGROUND_4_FOG);
}

bool Board::StageHas6Rows()
{
	return (mBackground == BackgroundType::BACKGROUND_3_POOL || mBackground == BackgroundType::BACKGROUND_4_FOG);
}

bool Board::StageHasZombieWalkInFromRight()
{
	if (mApp->IsWhackAZombieLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM ||
		mApp->IsFinalBossLevel() ||
		mApp->IsIZombieLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsScaryPotterLevel())
		return false;

	return true;
}

bool Board::StageHasFog()
{
	return !mApp->IsStormyNightLevel() && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL && mBackground == BackgroundType::BACKGROUND_4_FOG;
}

bool Board::StageIsDayWithoutPool() {
	return mBackground == BackgroundType::BACKGROUND_1_DAY;
}

bool Board::StageIsDayWithPool() {
	return mBackground == BackgroundType::BACKGROUND_3_POOL;
}

int Board::LeftFogColumn()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_AIR_RAID)		return 6;
	if (!mApp->IsAdventureMode())										return 5;
	if (mLevel == 31)													return 6;
	if (mLevel >= 32 && mLevel <= 36)									return 5;
	if (mLevel >= 37 && mLevel <= 40)									return 4;
	PVZP_ASSERT(false);

	unreachable();
}

int Board::GetSeedPacketPositionX(int theIndex)
{
	if (mApp->IsSlotMachineLevel())			return theIndex * 59 + 247;
	if (HasConveyorBeltSeedBank())			return theIndex * 50 + 91;

	if (mSeedBank->mNumPackets <= 7)		return theIndex * 59 + 85;
	else if (mSeedBank->mNumPackets == 8)	return theIndex * 54 + 81;
	else if (mSeedBank->mNumPackets == 9)	return theIndex * 52 + 80;
	else									return theIndex * 51 + 79;
}

int Board::GetSeedBankExtraWidth()
{
	int aNumPackets = mSeedBank->mNumPackets;
	return aNumPackets <= 6 ? 0 : aNumPackets == 7 ? 60 : aNumPackets == 8 ? 76 : aNumPackets == 9 ? 112 : 153;
}

void Board::OffsetYForPlanting(int& theY, SeedType theSeedType)
{
	if (Plant::IsFlying(theSeedType) || theSeedType == SeedType::SEED_GRAVEBUSTER)
	{
		theY += 15;
	}
	if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
	{
		theY -= 15;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && mBackground == BackgroundType::BACKGROUND_GREENHOUSE)
	{
		theY -= 25;
	}
}

int Board::PlantingPixelToGridX(int theX, int theY, SeedType theSeedType)
{
	OffsetYForPlanting(theY, theSeedType);
	return PixelToGridX(theX, theY);
}

int Board::PlantingPixelToGridY(int theX, int theY, SeedType theSeedType)
{
	OffsetYForPlanting(theY, theSeedType);

	int aGridY = PixelToGridY(theX, theY);
	if (theSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		int aGridX = PixelToGridX(theX, theY);

		// @pvz-online: 闯关里咖啡豆认任意植物（蘑菇全醒），吸附跟着放宽（原来只吸睡着的）
		Plant* aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aPlant && (mApp->IsRunMode() || aPlant->mIsAsleep))
		{
			return aGridY;
		}

		int aGridYDown = PixelToGridY(theX, theY + 30);
		if (aGridYDown != aGridY)
		{
			Plant* aPlantDown = GetTopPlantAt(aGridX, aGridYDown, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
			if (aPlantDown && (mApp->IsRunMode() || aPlantDown->mIsAsleep))
			{
				return aGridYDown;
			}
		}

		int aGridYUp = PixelToGridY(theX, theY - 50);
		if (aGridYUp != aGridY)
		{
			Plant* aPlantUp = GetTopPlantAt(aGridX, aGridYUp, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
			if (aPlantUp && (mApp->IsRunMode() || aPlantUp->mIsAsleep))
			{
				return aGridYUp;
			}
		}
	}
	return aGridY;
}

int Board::PixelToGridX(int theX, int theY)
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE ||
			mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
			mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
		{
			return mApp->mZenGarden->PixelToGridX(theX, theY);
		}
	}

	if (theX < LAWN_XMIN)
		return -1;

	return std::clamp((theX - LAWN_XMIN) / 80, 0, MAX_GRID_SIZE_X - 1);
}

int Board::PixelToGridXKeepOnBoard(int theX, int theY)
{
	int aGridX = PixelToGridX(theX, theY);
	return std::max(aGridX, 0);
}

int Board::PixelToGridY(int theX, int theY)
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE ||
			mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
			mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
		{
			return mApp->mZenGarden->PixelToGridY(theX, theY);
		}
	}

	int aGridX = PixelToGridX(theX, theY);
	if (aGridX == -1 || theY < LAWN_YMIN)
		return -1;

	if (StageHasRoof())
	{
		if (aGridX < 5)
		{
			theY -= (4 - aGridX) * 20;
		}
		return std::clamp((theY - LAWN_YMIN) / 85, 0, MAX_GRID_SIZE_Y - 2);
	}
	else if (StageHasPool())
	{
		return std::clamp((theY - LAWN_YMIN) / 85, 0, MAX_GRID_SIZE_Y - 1);
	}
	else
	{
		return std::clamp((theY - LAWN_YMIN) / 100, 0, MAX_GRID_SIZE_Y - 2);
	}
}

int Board::PixelToGridYKeepOnBoard(int theX, int theY)
{
	int aGridY = PixelToGridY(std::max(theX, 80), theY);
	return std::max(aGridY, 0);
}

int Board::GridToPixelX(int theGridX, int theGridY)
{
	PVZP_ASSERT(theGridX >= 0 && theGridX < MAX_GRID_SIZE_X);
	PVZP_ASSERT(theGridY >= 0 && theGridY < MAX_GRID_SIZE_Y);
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE ||
			mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
			mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
		{
			return mApp->mZenGarden->GridToPixelX(theGridX, theGridY);
		}
	}

	return theGridX * 80 + LAWN_XMIN;
}

float Board::GetPosYBasedOnRow(float thePosX, int theRow)
{
	if (StageHasRoof())
	{
		float aSlopeOffset = 0.0f;
		if (thePosX < 440.0f)
		{
			aSlopeOffset = (440.0f - thePosX) * 0.25f;
		}

		return GridToPixelY(8, theRow) + aSlopeOffset;
	}

	return GridToPixelY(0, theRow);
}

int Board::GridToPixelY(int theGridX, int theGridY)
{
	PVZP_ASSERT(theGridX >= 0 && theGridX < MAX_GRID_SIZE_X);
	PVZP_ASSERT(theGridY >= 0 && theGridY < MAX_GRID_SIZE_Y);
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE ||
			mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
			mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
		{
			return mApp->mZenGarden->GridToPixelY(theGridX, theGridY);
		}
	}

	int aY;
	if (StageHasRoof())
	{
		int aSlopeOffset;
		if (theGridX < 5)
		{
			aSlopeOffset = (5 - theGridX) * 20;
		}
		else
		{
			aSlopeOffset = 0;
		}
		aY = theGridY * 85 + aSlopeOffset + LAWN_YMIN - 10;
	}
	else if (StageHasPool())
	{
		aY = theGridY * 85 + LAWN_YMIN;
	}
	else
	{
		aY = theGridY * 100 + LAWN_YMIN;
	}

	if (theGridX != -1 && mGridSquareType[theGridX][theGridY] == GridSquareType::GRIDSQUARE_HIGH_GROUND)
	{
		aY -= HIGH_GROUND_HEIGHT;
	}

	return aY;
}

ZombieID Board::ZombieGetID(Zombie* theZombie)
{
	return static_cast<ZombieID>(mZombies.DataArrayGetID(theZombie));
}

Zombie* Board::ZombieGet(ZombieID theZombieID)
{
	return mZombies.DataArrayGet(static_cast<unsigned int>(theZombieID));
}

Zombie* Board::ZombieTryToGet(ZombieID theZombieID)
{
	return mZombies.DataArrayTryToGet(static_cast<unsigned int>(theZombieID));
}

int GetRectOverlap(const Rect& rect1, const Rect& rect2)
{
	return std::min(rect1.mX + rect1.mWidth, rect2.mX + rect2.mWidth) -
		std::max(rect1.mX, rect2.mX);
}

bool GetCircleRectOverlap(int theCircleX, int theCircleY, int theRadius, const Rect& theRect)
{
	int aNearX = std::clamp(theCircleX, theRect.mX, theRect.mX + theRect.mWidth);
	int aNearY = std::clamp(theCircleY, theRect.mY, theRect.mY + theRect.mHeight);
	int dx = theCircleX - aNearX;
	int dy = theCircleY - aNearY;
		return static_cast<double>(dx) * dx + static_cast<double>(dy) * dy <= static_cast<double>(theRadius) * theRadius;
}

void Board::KillAllPlantsInRadius(int theX, int theY, int theRadius)
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (GetCircleRectOverlap(theX, theY, theRadius, aPlant->GetPlantRect()))
		{
			mPlantsEaten++;
			aPlant->Die();
		}
	}
}

unsigned int Board::SeedNotRecommendedForLevel(SeedType theSeedType)
{
	unsigned int aNotRec = 0;
	if (Plant::IsNocturnal(theSeedType) && !StageIsNight() && !mApp->mRunState)
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NOCTURNAL, true);
	}
	if (theSeedType == SeedType::SEED_INSTANT_COFFEE && StageIsNight() && !mApp->IsRunMode())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_AT_NIGHT, true);
	}
	if (theSeedType == SeedType::SEED_GRAVEBUSTER && !StageHasGraveStones())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_GRAVES, true);
	}
	if (theSeedType == SeedType::SEED_PLANTERN && !StageHasFog())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_FOG, true);
	}
	if (theSeedType == SeedType::SEED_FLOWERPOT && !StageHasRoof())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_ROOF, true);
	}
	if (!StageHasPool() && Plant::IsAquatic(theSeedType) && !mApp->mRunState)
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_POOL, true);
	}
	return aNotRec;
}

int Board::CountCoinByType(CoinType theCoinType)
{
	int aCount = 0;

	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead)
			continue;
		if (aCoin->mType == theCoinType)
		{
			aCount++;
		}
	}

	return aCount;
}

int Board::GetGraveStoneCount()
{
	int aCount = 0;

	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE)
		{
			aCount++;
		}
	}

	return aCount;
}

void Board::DropLootPiece(int thePosX, int thePosY, int theDropFactor)
{
	if (mApp->IsFirstTimeAdventureMode())
	{
		if (mLevel == 22 && mCurrentWave > 5 && !mApp->mPlayerInfo->mHasUnlockedMinigames && CountCoinByType(CoinType::COIN_PRESENT_MINIGAMES) == 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_ART_CHALLENGE);
			AddCoin(thePosX, thePosY, CoinType::COIN_PRESENT_MINIGAMES, CoinMotion::COIN_MOTION_COIN);
			return;
		}
		if (mLevel == 36 && mCurrentWave > 5 && !mApp->mPlayerInfo->mHasUnlockedPuzzleMode && CountCoinByType(CoinType::COIN_PRESENT_PUZZLE_MODE) == 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_ART_CHALLENGE);
			AddCoin(thePosX, thePosY, CoinType::COIN_PRESENT_PUZZLE_MODE, CoinMotion::COIN_MOTION_COIN);
			return;
		}
	}

	int aDropHit = Rand(30000);
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 11 && !mDroppedFirstCoin && mCurrentWave > 5)
	{
		aDropHit = 1000;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
	{
		aDropHit *= 5;
	}

	if (mApp->IsWhackAZombieLevel())
	{
		int aSunChanceMin = 2500;
		int aSunChanceMax = mSunMoney > 500 ? 2800 : mSunMoney > 350 ? 3100 : mSunMoney > 200 ? 3700 : 5000;
		if (aDropHit >= aSunChanceMin * theDropFactor && aDropHit <= aSunChanceMax * theDropFactor)
		{
			mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
			AddCoin(thePosX - 20, thePosY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
			AddCoin(thePosX - 40, thePosY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
			AddCoin(thePosX - 60, thePosY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
			return;
		}
	}

	if (mTotalSpawnedWaves > 70)
		return;

	int aPottedPlantChance;
	if (!mApp->mZenGarden->CanDropPottedPlantLoot())
	{
		aPottedPlantChance = 0;
	}
	else if (mApp->IsAdventureMode() && !mApp->IsFirstTimeAdventureMode())
	{
		aPottedPlantChance = 24;
	}
	else
	{
		aPottedPlantChance = mApp->IsSurvivalEndless(mApp->mGameMode) ? 3 : 12;
	}

	int aChocolateChance = aPottedPlantChance;
	if (mApp->mZenGarden->CanDropChocolate())
	{
		if (mApp->IsAdventureMode() && !mApp->IsFirstTimeAdventureMode())
		{
			aChocolateChance = aPottedPlantChance + 72;
		}
		else
		{
			aChocolateChance = aPottedPlantChance + (mApp->IsSurvivalEndless(mApp->mGameMode) ? 9 : 36);
		}
	}

	int aDiamondChance = aChocolateChance + 14;
	int aGoldChance = aChocolateChance + 250;
	int aSilverChance = aChocolateChance + 2500;

	CoinType aCoinType;
	if (aDropHit < aPottedPlantChance * theDropFactor)
	{
		aCoinType = CoinType::COIN_PRESENT_PLANT;
	}
	else if (aDropHit < aChocolateChance * theDropFactor)
	{
		aCoinType = CoinType::COIN_CHOCOLATE;
	}
	else if (aDropHit < aDiamondChance * theDropFactor)
	{
		aCoinType = (mApp->IsOnlineGame() || mApp->IsRunMode() || mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] < 1) ? CoinType::COIN_GOLD : CoinType::COIN_DIAMOND;
	}
	else if (aDropHit < aGoldChance * theDropFactor)
	{
		aCoinType = CoinType::COIN_GOLD;
	}
	else if (aDropHit < aSilverChance * theDropFactor)
	{
		aCoinType = CoinType::COIN_SILVER;
	}
	else return;

	if (mApp->IsWallnutBowlingLevel() && Coin::IsMoney(aCoinType))
		return;

	if (mApp->IsFirstTimeAdventureMode() && mLevel == 11)
	{
		int aMoney = Coin::GetCoinValue(CoinType::COIN_GOLD) * mLawnMowers.mSize;
		int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);
		aMoney += mApp->mPlayerInfo->mCoins + CountCoinsBeingCollected();
		if (Coin::GetCoinValue(aCoinType) + aMoney >= aCost)
		{
			return;
		}
	}

	mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
	AddCoin(thePosX - 40, thePosY, aCoinType, CoinMotion::COIN_MOTION_COIN);
	mDroppedFirstCoin = true;
}

bool Board::CanDropLoot()
{
	return !mCutScene->ShouldRunUpsellBoard() && (!mApp->IsFirstTimeAdventureMode() || mLevel >= 11);
}

bool Board::BungeeIsTargetingCell(int theGridX, int theGridY)
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (!aZombie->IsDeadOrDying() && aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mRow == theGridY && aZombie->mTargetCol == theGridX)
		{
			return true;
		}
	}
	return false;
}

Zombie* Board::GetBossZombie()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
		{
			return aZombie;
		}
	}
	return nullptr;
}

Plant* Board::FindUmbrellaPlant(int theGridX, int theGridY)
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == SeedType::SEED_UMBRELLA && !aPlant->NotOnGround() && GridInRange(theGridX, theGridY, aPlant->mPlantCol, aPlant->mRow, 1, 1))
		{
			return aPlant;
		}
	}
	return nullptr;
}

void Board::DoFwoosh(int theRow)
{
	int aRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, theRow, 1);
	for (int i = 0; i < 12; i++)
	{
		Reanimation* aOriReanim = mApp->ReanimationTryToGet(mFwooshID[theRow][i]);
		if (aOriReanim)
		{
			aOriReanim->ReanimationDie();
		}

		float aPosX = 750.0f * i / 11.0f + 10.0f;
		float aPosY = GetPosYBasedOnRow(aPosX + 10.0f, theRow) - 10.0f;
		Reanimation* aFwoosh = mApp->AddReanimation(aPosX, aPosY, aRenderOrder, ReanimationType::REANIM_JALAPENO_FIRE);
		if (!aFwoosh)
		{
			mFwooshID[theRow][i] = ReanimationID::REANIMATIONID_NULL;
			continue;
		}
		aFwoosh->SetFramesForLayer("anim_flame");
		aFwoosh->mLoopType = ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME;
		aFwoosh->mAnimRate *= RandRangeFloat(0.7f, 1.3f);

		float aScale = RandRangeFloat(0.9f, 1.1f);
		float aFlip = Rand(2) ? 1.0f : -1.0f;
		aFwoosh->OverrideScale(aScale * aFlip, 1);

		mFwooshID[theRow][i] = mApp->ReanimationGetID(aFwoosh);
	}
	mFwooshCountDown = 100;
}

void Board::UpdateFwoosh()
{
	if (mFwooshCountDown == 0)
		return;

	int aFwooshRemaining = PvzpAnimateCurve(50, 0, --mFwooshCountDown, 12, 0, PvzpCurves::CURVE_LINEAR);
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		for (int i = 0; i < 12 - aFwooshRemaining; i++)
		{
			Reanimation* aFwoosh = mApp->ReanimationTryToGet(mFwooshID[aRow][i]);
			if (aFwoosh)
			{
				aFwoosh->SetFramesForLayer("anim_done");
				aFwoosh->mAnimRate = 15;
				aFwoosh->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME;
			}
			mFwooshID[aRow][i] = ReanimationID::REANIMATIONID_NULL;
		}
	}
}

void Board::UpdateGridItems()
{
	const bool advanceGraves = mEnableGraveStones || (mApp->IsRunMode() && mApp->mGameScene == GameScenes::SCENE_PLAYING);
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (advanceGraves && aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE && aGridItem->mGridItemCounter < 100)
		{
			aGridItem->mGridItemCounter++;
		}

		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_CRATER && mApp->mGameScene == GameScenes::SCENE_PLAYING)
		{
			if (aGridItem->mGridItemCounter > 0)
			{
				aGridItem->mGridItemCounter--;
			}
			if (aGridItem->mGridItemCounter == 0)
			{
				aGridItem->GridItemDie();
			}
		}
		aGridItem->Update();
	}
}

bool Board::PlantingRequirementsMet(SeedType theSeedType)
{
    if (mApp->IsRunMode()) return true;
	switch (theSeedType)
	{
	case SeedType::SEED_GATLINGPEA:			return CountPlantByType(SeedType::SEED_REPEATER);
	case SeedType::SEED_TWINSUNFLOWER:		return CountPlantByType(SeedType::SEED_SUNFLOWER);
	case SeedType::SEED_GLOOMSHROOM:		return CountPlantByType(SeedType::SEED_FUMESHROOM);
	case SeedType::SEED_CATTAIL:			return CountEmptyPotsOrLilies(SeedType::SEED_LILYPAD);
	case SeedType::SEED_WINTERMELON:		return CountPlantByType(SeedType::SEED_MELONPULT);
	case SeedType::SEED_GOLD_MAGNET:		return CountPlantByType(SeedType::SEED_MAGNETSHROOM);
	case SeedType::SEED_SPIKEROCK:			return CountPlantByType(SeedType::SEED_SPIKEWEED);
	case SeedType::SEED_COBCANNON:			return HasValidCobCannonSpot();
	default:								return true;
	}
}

int Board::KillAllZombiesInRadius(int theRow, int theX, int theY, int theRadius, int theRowRange, bool theBurn, int theDamageRangeFlags, int theDirectDamage, bool theExpandRadius)
{
	Zombie* aZombie = nullptr;
	int aKilledZombies = 0; // @Patoke: implemented this
	while (IterateZombies(aZombie))
	{
		if (aZombie->EffectedByDamage(theDamageRangeFlags))
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			int aRowDist = aZombie->mRow - theRow;
			if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
			{
				aRowDist = 0;
			}

			if ((theExpandRadius || (aRowDist <= theRowRange && aRowDist >= -theRowRange)) && GetCircleRectOverlap(theX, theY, theRadius, aZombieRect))
			{
				if (theBurn)
				{
					// @pvz-online: theDirectDamage 透传给 ApplyBurn（0 = 默认）；毁灭菇单株升级
					//「Annihilation」的 6000 直伤覆写走这条（批八b；2026-10-09 夜 50000→6000）。
					aZombie->ApplyBurn(theDirectDamage);
				}
				else
				{
					// @pvz-online: 闯关 buff「爆破」：爆炸直伤 ×(1+60%/层)。走 theBurn=false
					// 这条的只有土豆雷；燃烧那条（樱桃/辣椒/末日菇）在 Zombie::ApplyBurn 里加。
					// 单株升级「宽装药」只加半径（见 Plant::DoSpecial 土豆雷支），这里不再乘单株。
					int aBlastBase = theDirectDamage > 0 ? theDirectDamage : 1800;
					aZombie->TakeDamage((int)(aBlastBase * mApp->RunBuffMul(RUN_BUFF_BLAST) + 0.5f), 18U);
				}

				aKilledZombies++;
			}
		}
	}

	int aGridX = PixelToGridXKeepOnBoard(theX, theY);
	int aGridY = PixelToGridYKeepOnBoard(theX, theY);
	GridItem* aGridItem = nullptr;
	while (IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
		{
			Rect aLadderRect(GridToPixelX(aGridItem->mGridX, aGridItem->mGridY),
				GridToPixelY(aGridItem->mGridX, aGridItem->mGridY), 80, 80);
			if (theExpandRadius ? GetCircleRectOverlap(theX, theY, theRadius, aLadderRect) :
				GridInRange(aGridItem->mGridX, aGridItem->mGridY, aGridX, aGridY, theRowRange, theRowRange))
			{
				aGridItem->GridItemDie();
			}
		}
	}

	return aKilledZombies;
}

int Board::GetNumWavesPerSurvivalStage()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND || mApp->IsSurvivalNormal(mApp->mGameMode))
	{
		return 10;
	}
	else if (mApp->IsSurvivalHard(mApp->mGameMode) || mApp->IsSurvivalEndless(mApp->mGameMode))
	{
		return 20;
	}

	PVZP_ASSERT(false);

	unreachable();
}

void Board::RemoveParticleByType(ParticleEffect theEffectType)
{
	for (PvzpParticleSystem* aParticle : mApp->mEffectSystem->mParticleHolder->mParticleSystems)
	{
		if (aParticle->mDead)
			continue;
		if (aParticle->mEffectType == theEffectType)
		{
			aParticle->ParticleSystemDie();
		}
	}
}

bool Board::PlantUsesAcceleratedPricing(SeedType theSeedType)
{
	return Plant::IsUpgrade(theSeedType) && mApp->IsSurvivalEndless(mApp->mGameMode);
}

int Board::GetCurrentPlantCost(SeedType theSeedType, SeedType theImitaterType)
{
	int aCost = Plant::GetCost(theSeedType, theImitaterType);
	if (PlantUsesAcceleratedPricing(theSeedType))
	{
		aCost += CountPlantByType(theSeedType) * 50;
	}
	return aCost;
}

bool Board::CanUseGameObject(GameObjectType theGameObject)
{
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		return (theGameObject == GameObjectType::OBJECT_TYPE_TREE_FOOD) || (theGameObject == GameObjectType::OBJECT_TYPE_NEXT_GARDEN);
	}
	if (mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		return false;
	}

	switch (theGameObject)
	{
	case GameObjectType::OBJECT_TYPE_WATERING_CAN:
		return true;
	case GameObjectType::OBJECT_TYPE_NEXT_GARDEN:
		return
			mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_MUSHROOM_GARDEN] ||
			mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_AQUARIUM_GARDEN] ||
			mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_TREE_OF_WISDOM];
	case GameObjectType::OBJECT_TYPE_FERTILIZER:
		return mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FERTILIZER] > 0;
	case GameObjectType::OBJECT_TYPE_BUG_SPRAY:
		return mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_BUG_SPRAY] > 0;
	case GameObjectType::OBJECT_TYPE_PHONOGRAPH:
		return  mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PHONOGRAPH] > 0;
	case GameObjectType::OBJECT_TYPE_CHOCOLATE:
		return mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_CHOCOLATE] > 0;
	case GameObjectType::OBJECT_TYPE_WHEELBARROW:
		return mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_WHEEL_BARROW] > 0;
	case GameObjectType::OBJECT_TYPE_GLOVE:
		return mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_GARDENING_GLOVE] > 0;
	case GameObjectType::OBJECT_TYPE_MONEY_SIGN:
		return mApp->CanShowZenGarden(); // @pvz-online: selling is part of the garden economy.
	case GameObjectType::OBJECT_TYPE_TREE_FOOD:
		return false;
	default:
		PVZP_ASSERT(false);
		unreachable();
	}
}

void Board::ShakeBoard(int theShakeAmountX, int theShakeAmountY)
{
	mShakeCounter = 12;
	mShakeAmountX = theShakeAmountX;
	mShakeAmountY = theShakeAmountY;
}

LawnMower* Board::FindLawnMowerInRow(int theRow)
{
	for (LawnMower* aLawnMower : mLawnMowers)
	{
		if (aLawnMower->mDead)
			continue;
		if (aLawnMower->mRow == theRow)
		{
			return aLawnMower;
		}
	}
	return nullptr;
}

Zombie* Board::GetWinningZombie()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mFromWave == Zombie::ZOMBIE_WAVE_WINNER)
		{
			return aZombie;
		}
	}
	return nullptr;
}

int Board::CountZombieByType(ZombieType theZombieType)
{
	int aCount = 0;

	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mZombieType == theZombieType)
		{
			aCount++;
		}
	}

	return aCount;
}

int Board::NumberZombiesInWave(int theWaveIndex)
{
	PVZP_ASSERT(theWaveIndex >= 0 && theWaveIndex < MAX_ZOMBIE_WAVES && theWaveIndex < mNumWaves);

	for (int i = 0; i < MAX_ZOMBIES_IN_WAVE; i++)
	{
		if (mZombiesInWave[theWaveIndex][i] == ZombieType::ZOMBIE_INVALID)
		{
			return i;
		}
	}

	PVZP_ASSERT(false);
	return 0;
}

bool Board::IsZombieTypeSpawnedOnly(ZombieType theZombieType)
{
	// @pvz-online: 桶钢门/路障报纸/桶报纸/铁门植物头五种只走闯关名单投放——生存模式的随机给怪池不能抽到它们。
	return PlantZombies::Family(theZombieType)>=0 || (theZombieType == ZombieType::ZOMBIE_BACKUP_DANCER || theZombieType == ZombieType::ZOMBIE_BOBSLED || theZombieType == ZombieType::ZOMBIE_IMP || theZombieType == ZombieType::ZOMBIE_PAIL_DOOR || theZombieType == ZombieType::ZOMBIE_CONE_NEWSPAPER || theZombieType == ZombieType::ZOMBIE_PAIL_NEWSPAPER ||
		theZombieType == ZombieType::ZOMBIE_PEA_HEAD_DOOR || theZombieType == ZombieType::ZOMBIE_WALLNUT_HEAD_DOOR || theZombieType == ZombieType::ZOMBIE_JALAPENO_HEAD_DOOR || theZombieType == ZombieType::ZOMBIE_GATLING_HEAD_DOOR || theZombieType == ZombieType::ZOMBIE_SQUASH_HEAD_DOOR);
}

static int RunBossHpPermille(ZombieType type)
{
	if (type == ZombieType::ZOMBIE_REDEYE_GARGANTUAR) return 2000;
	if (type == ZombieType::ZOMBIE_FOOTBALL) return 4500;
	if (type == ZombieType::ZOMBIE_ZAMBONI) return 7500;
	return 1000;
}

bool Board::SpawnRunBoss()
{
	RunState* aRun = mApp->GetRunState();
	if (aRun == nullptr) return true;
	int aBossTier = aRun->IsEndless() ? (aRun->IsEndlessBossLevel()?1:0) : ((aRun->mBossFlag > 0 && aRun->mBossFlag <= 2) ? aRun->mBossFlag : 0);	// 消费端钳（2026-10-16 起三档）
	if (aBossTier <= 0) return true;
	if (aRun->IsEndless() && !aRun->IsEndlessBossLevel()) return true;		// 无尽档没有关底
	// 末关（档 1/2 都出）或场景 3 末关（仅档 2；2026-10-20 起快速第 3 关同样出场）。
	bool aFinalLevel = aRun->GetPlayingLevelIndex() + 1 >= aRun->GetLevelCount();
	bool aScene3Final = aBossTier >= 2
		&& aRun->GetPlayingLevelIndex() + 1 == RunState::LevelsPerScene(aRun->mMode) * 3;
	if (!aFinalLevel && !aScene3Final && !aRun->IsEndlessBossLevel()) return true;
	if (mRunBossZombieID != ZombieID::ZOMBIEID_NULL) return true;	// 一关一只（旗波钩子每旗都会来问）
	// 仅一号位：联机恒出在 1 号位玩家的棋盘（2026-10-12 用户令由「房主棋盘」改为
	// 「一号位棋盘」——换位后房主可能不在 1 号位，boss 固定跟席位 1 走，不跟房主；
	// 单机恒有）。1 号位空席（人离房释放了）时本关不刷。

	if (mApp->IsOnlineGame() && mApp->mOnlineSession->GetLocalSeat() != NetProto::SEAT_HOST) return true;
    if(!mZombies.DataArrayCanAllocate(2) || !CanAllocateZombieAnimations(ZombieType::ZOMBIE_NORMAL,mCurrentWave)) return false;
	int aRows[MAX_GRID_SIZE_Y];
	int aRowCount = 0;
	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		if (RowCanHaveZombies(i) && mPlantRow[i] != PlantRowType::PLANTROW_POOL)
		{
			aRows[aRowCount++] = i;
		}
	}
	if (aRowCount == 0) return true;

	static const ZombieType bossTypes[] = {
		ZombieType::ZOMBIE_REDEYE_GARGANTUAR, ZombieType::ZOMBIE_FOOTBALL, ZombieType::ZOMBIE_ZAMBONI
	};
	MTRand bossRandom((unsigned)aRun->GetLevelSeed()^0x424f5353u);
    int typeIndex=aRun->IsNewEndless()?(int)bossRandom.Next((unsigned long)(StageHasRoof()?2:3)):Rand(3);
    int rowIndex=aRun->IsNewEndless()?(int)bossRandom.Next((unsigned long)aRowCount):Rand(aRowCount);
    ZombieType aBossType = bossTypes[typeIndex];
    Zombie* aBoss = [&]() { RunSpeciesRules::SpawnScope context(RunSpeciesRules::SpawnKind::RUN_BOSS); return AddZombieInRow(aBossType, aRows[rowIndex], mCurrentWave); }();
	if (aBoss == nullptr) return false;

	aBoss->mScaleZombie = 2.0f;
    aBoss->mBossMode = Zombie::RUN_BOSS_MARKER;
	int aMult = mApp->OnlineSeatMultiplier() * RunBossHpPermille(aBossType);
	aBoss->mBodyHealth = RunMath::ScaleHealth(aBoss->mBodyHealth, aMult, 1000);
	aBoss->mBodyMaxHealth = aBoss->mBodyHealth;
	if (aBossType == ZombieType::ZOMBIE_FOOTBALL)
	{
		aBoss->mHelmHealth = RunMath::ScaleHealth(aBoss->mHelmHealth, aMult, 1000);
		aBoss->mHelmMaxHealth = aBoss->mHelmHealth;
	}
	aBoss->UpdateAnimSpeed();
	mRunBossZombieID = ZombieGetID(aBoss);
    return true;
}

bool Board::IsRunBossZombie(Zombie* theZombie)
{
	// @pvz-online: a board can receive more than one boss; a single spawn ID cannot identify all of them.
	return theZombie != nullptr &&
		((theZombie->mBossMode == Zombie::RUN_BOSS_MARKER &&
		  (theZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR || theZombie->mZombieType == ZombieType::ZOMBIE_FOOTBALL ||
		   theZombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI)) ||
		 (mRunBossZombieID != ZombieID::ZOMBIEID_NULL && mRunBossZombieID == ZombieGetID(theZombie)));
}

void Board::InitEndlessRunStage()
{
	RunState* aRun = mApp->GetRunState();
	if (aRun == nullptr) return;
    mApp->mDamageStats.BeginLevel(this);

	// 关序号 / 种子的覆盖值跟着新一关换：选卡候选、墓碑这类"关级随机"读的就是它
	// （建新场时这一步在 EnterRunLevel，这条路上没人替它做）。
	mApp->SetOnlineStartOverride(aRun->GetLevel(),aRun->GetLevelSeed());
    mApp->mAppRandSeed = aRun->GetLevelSeed();
	mLevel = aRun->GetLevel();

	RefreshSeedPacketFromCursor();
	mRunBossZombieID = ZombieID::ZOMBIEID_NULL;
    mLevelComplete = false;
	// 联机局的草坪在等待期一直开着：棋盘上可能留着队友漏过来、还没走完的怪。
	// 新一关各就各位，这些上一关的尾巴不能接着走——它们会啃新关的植物，
	// 摸到房子那一边还会把推车误碰掉。单机局的怪在过关时已经清完，这儿是空转。
	RemoveAllZombies();
	InitZombieWaves();
	mBoardFadeOutCounter = -1;	// 单机过关的白幕倒计时在这一刻才真正落定

	if (StageHasFog())
	{
		mFogBlownCountDown = FOG_BLOW_RETURN_TIME;
	}
	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		mWaveRowGotLawnMowered[i] = -100;
	}
}

void Board::FillSeedBankFromRunPool()
{
	RunState* aRun = mApp->GetRunState();
	mSeedBank->mNumPackets = GetNumSeedsInBank();
	for (int i = 0; i < mSeedBank->mNumPackets; i++)
	{
		mSeedBank->mSeedPackets[i].SetPacketType(aRun->GetUsablePool()[i]);
	}
	mSeedBank->UpdateWidth();
}

bool Board::TryRelayEscapedZombie(Zombie* theZombie)
{
	if (!mApp->IsOnlineGame()) return false;

	NetProto::MsgEscapedZombie aMsg;
	aMsg.mRow = (uint8_t)theZombie->mRow;
	aMsg.mZombieType = (uint16_t)theZombie->mZombieType;
	aMsg.mFlags = IsRunBossZombie(theZombie) ? NetProto::ESCAPED_FLAG_RUN_BOSS : 0;
	aMsg.mBodyHealth = theZombie->mBodyHealth;
	aMsg.mHelmHealth = theZombie->mHelmHealth;
	aMsg.mShieldHealth = theZombie->mShieldHealth;
	aMsg.mFlyingHealth = theZombie->mFlyingHealth;
	aMsg.mBodyMaxHealth = theZombie->mBodyMaxHealth;
	aMsg.mHelmMaxHealth = theZombie->mHelmMaxHealth;
	aMsg.mShieldMaxHealth = theZombie->mShieldMaxHealth;
	aMsg.mFlyingMaxHealth = theZombie->mFlyingMaxHealth;
    RunSpeciesRuntime::ExportRelay(theZombie,aMsg);

	// 发不出去（末席没有下一席位，或者队友刚好断了、socket 坏了）就当没传成：
	// 宁可这边判负，也不能让这只怪凭空消失——那等于把惩罚取消了。
	if (!mApp->mOnlineSession->SendEscapedZombie(aMsg)) return false;

	theZombie->DieNoLoot();
	return true;
}

// @pvz-online: 接力怪入场来源合并（2026-10-15，docs/09 §五）：只记账不开写——同一来源
// 同一行的怪在合并窗内并成一批，窗口到期才推一条横幅，高密度漏怪不逐只刷屏。
void Board::NoteRelayedZombie(int theSrcSeat, int theRow)
{
	if (theSrcSeat <= 0 || theSrcSeat > NetProto::MAX_PLAYERS) return;
	if (theRow < 0 || theRow >= MAX_GRID_SIZE_Y) return;

	for (int i = 0; i < mRelayBatchCount; i++)
	{
		if (mRelayBatches[i].mSrcSeat == theSrcSeat && mRelayBatches[i].mRow == theRow)
		{
			mRelayBatches[i].mCount++;
			mRelayBatches[i].mFrames = RELAY_BATCH_FRAMES;	// 窗内续批：窗口重算，连传也连并
			return;
		}
	}
	if (mRelayBatchCount >= RELAY_BATCH_MAX)
	{
		for (int i = 1; i < mRelayBatchCount; i++) mRelayBatches[i - 1] = mRelayBatches[i];	// 满了丢最旧
		mRelayBatchCount--;
	}
	mRelayBatches[mRelayBatchCount].mSrcSeat = theSrcSeat;
	mRelayBatches[mRelayBatchCount].mRow = theRow;
	mRelayBatches[mRelayBatchCount].mCount = 1;
	mRelayBatches[mRelayBatchCount].mFrames = RELAY_BATCH_FRAMES;
	mRelayBatchCount++;
}

void Board::UpdateRelayBatches()
{
	for (int i = 0; i < mRelayBatchCount; i++) mRelayBatches[i].mFrames--;
	int aWrite = 0;
	for (int i = 0; i < mRelayBatchCount; i++)
	{
		if (mRelayBatches[i].mFrames > 0)
		{
			if (aWrite != i) mRelayBatches[aWrite] = mRelayBatches[i];
			aWrite++;
			continue;
		}
		// 到期：推一条合并横幅。行号按人类的 1..5 报（棋盘内部 0 起算）
		std::string aNameUtf8;
		if (mApp->mOnlineSession != nullptr) aNameUtf8 = mApp->mOnlineSession->GetSeatName((uint8_t)mRelayBatches[i].mSrcSeat);
		if (aNameUtf8.empty()) aNameUtf8 = "P" + std::to_string((unsigned)mRelayBatches[i].mSrcSeat);
		char aBuf[128];
		snprintf(aBuf, sizeof(aBuf),
			ModText::Tr("%s 的僵尸从第 %d 行过来 x%d", "%s's zombies came on row %d x%d"),
			aNameUtf8.c_str(), mRelayBatches[i].mRow + 1, mRelayBatches[i].mCount);
		PushCustomBanner(aBuf);
	}
	mRelayBatchCount = aWrite;
}

Zombie* Board::AddRelayedZombie(int theRow, ZombieType theZombieType, int theBodyHealth, int theHelmHealth, int theShieldHealth, int theFlyingHealth,
    uint8_t theFlags, int theBodyMaxHealth, int theHelmMaxHealth, int theShieldMaxHealth, int theFlyingMaxHealth, int theSrcSeat)
{
	// 行号和类型是网络来的（对面包长已经校验过，但内容没有）：行号越界会直接写穿
	// mPlantRow / mGridSquareType，类型越界会在初始化里撞到没有分支的 switch。
	if (theRow < 0 || theRow >= MAX_GRID_SIZE_Y) return nullptr;
	if (theZombieType < 0 || theZombieType >= ZombieType::NUM_ZOMBIE_TYPES) return nullptr;
	// 已经没血的怪就是已经死了的怪，别把尸体摆上来（摆上来 UpdatePlaying 的血量断言也会炸）。
	if (theBodyHealth <= 0) return nullptr;
	if (theFlags & ~NetProto::ESCAPED_FLAG_RUN_BOSS) return nullptr;
	const bool aRunBoss = (theFlags & NetProto::ESCAPED_FLAG_RUN_BOSS) != 0;
	if (aRunBoss && theZombieType != ZombieType::ZOMBIE_REDEYE_GARGANTUAR && theZombieType != ZombieType::ZOMBIE_FOOTBALL &&
		theZombieType != ZombieType::ZOMBIE_ZAMBONI) return nullptr;

	Zombie* aZombie = [&]() { RunSpeciesRules::SpawnScope context(RunSpeciesRules::SpawnKind::RELAY); return AddZombieInRow(theZombieType, theRow, mCurrentWave); }();
	if (aZombie == nullptr) return nullptr;		// 场上满了（AddZombieInRow 里那声 "Too many zombies!!"）

	MarkRelayedZombie(aZombie);
	NoteRelayedZombie(theSrcSeat, theRow);
	aZombie->mBodyHealth = theBodyHealth;
	aZombie->mHelmHealth = theHelmHealth;
	aZombie->mShieldHealth = theShieldHealth;
	aZombie->mFlyingHealth = theFlyingHealth;
	// @pvz-online: do not infer source maximum HP from the receiving seat's freshly initialized zombie.
	aZombie->mBodyMaxHealth = std::max(theBodyHealth, theBodyMaxHealth > 0 ? theBodyMaxHealth : aZombie->mBodyMaxHealth);
	if (theHelmMaxHealth > 0) aZombie->mHelmMaxHealth = std::max(theHelmHealth, theHelmMaxHealth);
	if (theShieldMaxHealth > 0) aZombie->mShieldMaxHealth = std::max(theShieldHealth, theShieldMaxHealth);
	if (theFlyingMaxHealth > 0) aZombie->mFlyingMaxHealth = std::max(theFlyingHealth, theFlyingMaxHealth);
	if (aRunBoss)
	{
		aZombie->mBossMode = Zombie::RUN_BOSS_MARKER;
		aZombie->mScaleZombie = 2.0f;
		aZombie->UpdateAnimSpeed();
	}

	// 对面丢掉的那部分要跟着丢：本机是刚初始化的新怪，帽子/铁门/报纸都还在身上。
	// 走游戏自己的掉装备函数、带上"不留尸体"标志——状态对了，粒子音效那些不重放。
	// （只剩多少血那种"半残"不用管：伤害值已经覆写，下一击时游戏自己会接上。）
	const unsigned int aQuietFlags = 1 << (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY;
	if (aZombie->mHelmHealth == 0) aZombie->DropHelm(aQuietFlags);
	if (aZombie->mShieldHealth == 0) aZombie->DropShield(aQuietFlags);

    aZombie->RefreshPlantHat();
	return aZombie;
}

bool Board::OnlineWaitingForTeam()
{
	return mApp->IsOnlineGame() && mLevelAwardSpawned
		&& mApp->mOnlineSession != nullptr && mApp->mOnlineSession->HasOtherSeats()
		&& !mApp->mOnlineSession->IsPeerLevelDone();
}

void Board::NoteMowerConsumed(int theRow)
{
	if (!mApp->IsOnlineGame() || !mApp->IsRunMode()) return;

	mApp->GetRunState()->mMowerUsedRows |= (1u << theRow);
	NetworkLog("[run] the mower on row %d is spent - no more this run", theRow);
}

int Board::ComputeLevelRandSeed(int theBoardRandSeed, bool theAdventureMode, int thePlayerId,
	int theFinishedAdventure, int theLevel, int theSurvivalStage, int theGameMode)
{
	int aRndSeed = thePlayerId + theBoardRandSeed;
	if (theAdventureMode)
	{
		aRndSeed += theFinishedAdventure * 101 + theLevel;
	}
	else
	{
		aRndSeed += theSurvivalStage * 101 + theGameMode;
	}
	return aRndSeed;
}

// @pvz-online: pooled IDs include generations; stale marks cannot attach to reused slots.
void Board::MarkRelayedZombie(Zombie* zombie)
{
    if (!zombie) return;
    for (auto it=mRelayedZombieIds.begin(); it!=mRelayedZombieIds.end(); )
        if (!mZombies.DataArrayTryToGet(*it)) it=mRelayedZombieIds.erase(it); else ++it;
    mRelayedZombieIds.insert((unsigned int)mZombies.DataArrayGetID(zombie));
}
bool Board::IsRelayedZombie(Zombie* zombie)
{
    return zombie && mRelayedZombieIds.count((unsigned int)mZombies.DataArrayGetID(zombie)) != 0;
}

// @pvz-online: markers belong to the final overlay, not behind the zombie's reanimation.
// Use its live body rectangle so jumping, balloon and giant units keep the marker attached.
void Board::DrawRelayedZombieMarkers(Graphics* g)
{
    Graphics marker(*g);
    Zombie* zombie = nullptr;
    while (IterateZombies(zombie))
    {
        if (!zombie->mVisible || zombie->mDead || zombie->IsDeadOrDying() ||
            zombie->mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED ||
            !IsRelayedZombie(zombie)) continue;
        Rect body = zombie->GetZombieRect();
        int x = body.mX + body.mWidth / 2 - 9;
        int y = body.mY - 24;
        if (x + 18 <= 0 || x >= BOARD_WIDTH || y + 18 <= 0 || y >= BOARD_HEIGHT) continue;
        marker.SetColor(Color(35, 30, 20, 230)); marker.FillRect(x, y, 18, 18);
        marker.SetColor(Color(255, 215, 70));
        marker.FillRect(x + 7, y + 2, 4, 8);
        marker.FillRect(x + 3, y + 8, 12, 3);
        marker.FillRect(x + 5, y + 11, 8, 3);
        marker.FillRect(x + 7, y + 14, 4, 2);
    }
}

// Legacy fields retain their old offsets and width for raw saves and old portable tags.
// New saves also persist the exact prefix fields in optional compatible data.
void Board::SyncLegacyWaveHealth()
{
    mZombieHealthWaveStart = static_cast<int>(std::min<int64_t>(mWaveHealth.start, INT_MAX));
    mZombieHealthToNextWave = static_cast<int>(std::min<int64_t>(mWaveHealth.next, INT_MAX));
}

void Board::RestoreLegacyWaveHealth()
{
    mWaveHealth.start = mZombieHealthWaveStart;
    mWaveHealth.next = mZombieHealthToNextWave;
}

bool Board::CanAllocateZombieAnimations(ZombieType type,int wave)
{
    if(PlantZombies::Armor(type) && (!mApp || !mApp->mEffectSystem ||
        !mApp->mEffectSystem->mAttachmentHolder ||
        !mApp->mEffectSystem->mAttachmentHolder->mAttachments.DataArrayCanAllocate(2))) return false;
    return mApp && mApp->mEffectSystem && mApp->mEffectSystem->mReanimationHolder &&
        mApp->mEffectSystem->mReanimationHolder->mReanimations.DataArrayCanAllocate(
            AnimationAdmission::ZombieGroup(type,wave) + mApp->mEffectSystem->mReanimationHolder->mPlantReservation);
}

// Planned cells remain authoritative while only missing actors are admitted.
bool Board::DrainDeferredPlantSetup()
{
    auto& state = mDeferredPlantSetup;
    while (state.HasPending())
    {
        const auto& item = state.pending[state.next];
        Plant* plant = state.kind == DeferredPlantSetup::SHOVEL ?
            AddPlant(item.x, item.y, static_cast<SeedType>(item.seed), SeedType::SEED_NONE) :
            NewPlant(item.x, item.y, static_cast<SeedType>(item.seed), SeedType::SEED_NONE);
        if (!plant) return false;
        if (state.kind == DeferredPlantSetup::I_ZOMBIE) mChallenge->IZombieSetupPlant(plant);
        else if (state.kind == DeferredPlantSetup::BEGHOULED) plant->mY = item.extra;
        else if (state.kind == DeferredPlantSetup::POTTER_COLUMN && item.seed == SeedType::SEED_POTATOMINE)
            plant->mStateCountdown = 10;
        ++state.next;
    }
    return true;
}

bool Board::CompleteDeferredPlantSetup()
{
    auto& state = mDeferredPlantSetup;
    if (state.flags & DeferredPlantSetup::INIT)
    {
        if (!mChallenge->CompleteInitPlants()) return false;
        state.flags &= ~DeferredPlantSetup::INIT;
    }
    if (state.flags & DeferredPlantSetup::NEXT_I_ZOMBIE)
    {
        if (!mChallenge->IZombieInitLevel()) return false;
        state.flags &= ~DeferredPlantSetup::NEXT_I_ZOMBIE;
    }
    if (state.flags & DeferredPlantSetup::START) StartLevel();
    if (state.flags) return false;
    if (state.HasPending())
    {
        if (state.kind == DeferredPlantSetup::SHOVEL) mChallenge->ShovelAddWallnuts();
        else DrainDeferredPlantSetup();
    }
    return !state.IsWaiting();
}
