#ifndef __BOARD_H__
#define __BOARD_H__

#include "../ConstEnums.h"
#include "../Sexy.TodLib/DataArray.h"
#include "widget/Widget.h"
#include "widget/ButtonListener.h"

#include "Plant.h"
#include "Zombie.h"
#include "Projectile.h"
#include "Coin.h"
#include "LawnMower.h"
#include "GridItem.h"
// @pvz-online: 观战（队友场地查看）要用 NetSession::ViewSnapshot 值成员；这个头链
// （NetSession→NetLink→NetProtocol）全是不含 winsock 的干净接口，进 Board.h 无碍。
#include "Online/NetSession.h"

using namespace Sexy;

#define MAX_GRID_SIZE_X 9
#define MAX_GRID_SIZE_Y 6
// 每波基准上限（原版 50）；联机席位顺位乘数最大 32 倍（2 的幂口径，六人局 1 号位），数组按 32 倍留量
#define WAVE_ZOMBIE_CAP_BASE 50
#define MAX_ZOMBIES_IN_WAVE (WAVE_ZOMBIE_CAP_BASE * 32)
#define MAX_ZOMBIE_WAVES 100
#define MAX_GRAVE_STONES MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y
#define MAX_POOL_GRID_SIZE 10
#define MAX_RENDER_ITEMS 2048
#define PROGRESS_METER_COUNTER 150

class LawnApp;
class CursorObject;
class CursorPreview;
class GameButton;
class MessageWidget;
class SeedBank;
class ToolTipWidget;
class CutScene;
class Challenge;
class Reanimation;
class DataSync;
class TodParticleSystem;
namespace Sexy
{
	class Graphics;
	class ButtonWidget;
	class WidgetManager;
	class Image;
	class MTRand;
}

class HitResult
{
public:
	void*							mObject;
	GameObjectType					mObjectType;
};

class RenderItem
{
public:
	RenderObjectType				mRenderObjectType;
	int								mZPos;
	union
	{
		GameObject*					mGameObject;
		Plant*						mPlant;
		Zombie*						mZombie;
		Coin*						mCoin;
		Projectile*					mProjectile;
		CursorPreview*				mCursorPreview;
		TodParticleSystem*			mParticleSytem;
		Reanimation*				mReanimation;
		GridItem*					mGridItem;
		LawnMower*					mMower;
		BossPart					mBossPart;
		int							mBoardGridY;
	};
};
bool RenderItemSortFunc(const RenderItem& theItem1, const RenderItem& theItem2);

struct ZombiePicker
{
	int								mZombieCount;
	int								mZombiePoints;
	int								mZombieTypeCount[NUM_ZOMBIE_TYPES];
	int								mAllWavesZombieTypeCount[NUM_ZOMBIE_TYPES];
};

/*inline*/ void						ZombiePickerInitForWave(ZombiePicker* theZombiePicker);
/*inline*/ void						ZombiePickerInit(ZombiePicker* theZombiePicker);

struct PlantsOnLawn
{
	Plant*							mUnderPlant;
	Plant*							mPumpkinPlant;
	Plant*							mFlyingPlant;
	Plant*							mNormalPlant;
};

struct BungeeDropGrid
{
	TodWeightedGridArray			mGridArray[MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y];
	int								mGridArrayCount;
};

class Board : public Widget, public ButtonListener
{
public:
	LawnApp*						mApp;													//+0x8C
	DataArray<Zombie>				mZombies;												//+0x90
	DataArray<Plant>				mPlants;												//+0xAC
	DataArray<Projectile>			mProjectiles;											//+0xC8
	DataArray<Coin>					mCoins;													//+0xE4
	DataArray<LawnMower>			mLawnMowers;											//+0x100
	DataArray<GridItem>				mGridItems;												//+0x11C
	CursorObject*					mCursorObject;											//+0x138
	CursorPreview*					mCursorPreview;											//+0x13C
	MessageWidget*					mAdvice;												//+0x140
	SeedBank*						mSeedBank;												//+0x144
	GameButton*						mMenuButton;											//+0x148
	GameButton*						mStoreButton;											//+0x14C
	bool							mIgnoreMouseUp;											//+0x150
	ToolTipWidget*					mToolTip;												//+0x154
	_Font*							mDebugFont;												//+0x158
	CutScene*						mCutScene;												//+0x15C
	Challenge*						mChallenge;												//+0x160
	bool							mPaused;												//+0x164
	GridSquareType					mGridSquareType[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];		//+0x168
	int								mGridCelLook[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];			//+0x240
	int								mGridCelOffset[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y][2];	//+0x318
	int								mGridCelFog[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y + 1];		//+0x4C8
	bool							mEnableGraveStones;										//+0x5C4
	int								mSpecialGraveStoneX;									//+0x5C8
	int								mSpecialGraveStoneY;									//+0x5CC
	float							mFogOffset;												//+0x5D0
	int								mFogBlownCountDown;										//+0x5D4
	PlantRowType					mPlantRow[MAX_GRID_SIZE_Y];								//+0x5D8
	int								mWaveRowGotLawnMowered[MAX_GRID_SIZE_Y];				//+0x5F0
	int								mBonusLawnMowersRemaining;								//+0x608
	int								mIceMinX[MAX_GRID_SIZE_Y];								//+0x60C
	int								mIceTimer[MAX_GRID_SIZE_Y];								//+0x624
	ParticleSystemID				mIceParticleID[MAX_GRID_SIZE_Y];						//+0x63C
	TodSmoothArray					mRowPickingArray[MAX_GRID_SIZE_Y];						//+0x654
	ZombieType						mZombiesInWave[MAX_ZOMBIE_WAVES][MAX_ZOMBIES_IN_WAVE];	//+0x6B4
	bool							mZombieAllowed[100];									//+0x54D4
	int								mSunCountDown;											//+0x5538
	int								mNumSunsFallen;											//+0x553C
	int								mShakeCounter;											//+0x5540
	int								mShakeAmountX;											//+0x5544
	int								mShakeAmountY;											//+0x5548
	BackgroundType					mBackground;											//+0x554C
	int								mLevel;													//+0x5550
	int								mSodPosition;											//+0x5554
	int								mPrevMouseX;											//+0x5558
	int								mPrevMouseY;											//+0x555C
	int								mSunMoney;												//+0x5560
	int								mNumWaves;												//+0x5564
	int								mMainCounter;											//+0x5568
	int								mEffectCounter;											//+0x556C
	int								mDrawCount;												//+0x5570
	int								mRiseFromGraveCounter;									//+0x5574
	int								mOutOfMoneyCounter;										//+0x5578
	int								mCurrentWave;											//+0x557C
	int								mTotalSpawnedWaves;										//+0x5580
	TutorialState					mTutorialState;											//+0x5584
	ParticleSystemID				mTutorialParticleID;									//+0x5588
	int								mTutorialTimer;											//+0x558C
	int								mLastBungeeWave;										//+0x5590
	int								mZombieHealthToNextWave;								//+0x5594
	int								mZombieHealthWaveStart;									//+0x5598
	int								mZombieCountDown;										//+0x559C
	int								mZombieCountDownStart;									//+0x55A0
	int								mHugeWaveCountDown;										//+0x55A4
	bool							mHelpDisplayed[NUM_ADVICE_TYPES];						//+0x55A8
	AdviceType						mHelpIndex;												//+0x55EC
	bool							mFinalBossKilled;										//+0x55F0
	bool							mShowShovel;											//+0x55F1
	int								mCoinBankFadeCount;										//+0x55F4
	DebugTextMode					mDebugTextMode;											//+0x55F8
	bool							mLevelComplete;											//+0x55FC
	int								mBoardFadeOutCounter;									//+0x5600
	int								mNextSurvivalStageCounter;								//+0x5604
	int								mScoreNextMowerCounter;									//+0x5608
	bool							mLevelAwardSpawned;										//+0x560C
	int								mProgressMeterWidth;									//+0x5610
	int								mFlagRaiseCounter;										//+0x5614
	int								mIceTrapCounter;										//+0x5618
	int								mBoardRandSeed;											//+0x561C
	ParticleSystemID				mPoolSparklyParticleID;									//+0x5620
	ReanimationID					mFwooshID[MAX_GRID_SIZE_Y][12];							//+0x5624
	int								mFwooshCountDown;										//+0x5744
	int								mTimeStopCounter;										//+0x5748
	bool							mDroppedFirstCoin;										//+0x574C
	int								mFinalWaveSoundCounter;									//+0x5750
	int								mCobCannonCursorDelayCounter;							//+0x5754
	int								mCobCannonMouseX;										//+0x5758
	int								mCobCannonMouseY;										//+0x575C
	bool							mKilledYeti;											//+0x5760
	bool							mMustacheMode;											//+0x5761
	bool							mSuperMowerMode;										//+0x5762
	bool							mFutureMode;											//+0x5763
	bool							mPinataMode;											//+0x5764
	bool							mDanceMode;												//+0x5765
	bool							mDaisyMode;												//+0x5766
	bool							mSukhbirMode;											//+0x5767
	BoardResult						mPrevBoardResult;										//+0x5768
	bool							mPeaShooterUsed;										//+GOTY @Patoke: 0x5784
	bool							mCatapultPlantsUsed;									//+GOTY @Patoke: 0x5785
	int								mLevelCoinsCollected;									//+GOTY @Patoke: 0x5788
	int								mGargantuarsKillsByCornCob;								//+GOTY @Patoke: 0x578C
	bool							mMushroomAndCoffeeBeansOnly;							//+GOTY @Patoke: 0x5790
	bool							mMushroomsUsed;											//+GOTY @Patoke: 0x5791
	int								mTriggeredLawnMowers;									//+0x576C
	int								mPlayTimeActiveLevel;									//+0x5770
	int								mPlayTimeInactiveLevel;									//+0x5774
	int								mMaxSunPlants;											//+0x5778
	DWORD							mStartDrawTime;											//+0x577C
	DWORD							mIntervalDrawTime;										//+0x5780
	int								mIntervalDrawCountStart;								//+0x5784
	float							mMinFPS;												//+0x5788
	int								mPreloadTime;											//+0x578C
	intptr_t						mGameID;												//+0x5790
	int								mGravesCleared;											//+0x5794
	int								mPlantsEaten;											//+0x5798
	int								mPlantsShoveled;										//+0x579C
	int								mCoinsCollected;										//+0x57A0 GOTY @Patoke: 0x57C8
	int								mDiamondsCollected;										//+0x57A4 GOTY @Patoke: 0x57CC
	int								mPottedPlantsCollected;									//+0x57A8
	int								mChocolateCollected;									//+0x57AC

public:
	Board(LawnApp* theApp);
	virtual ~Board();

	void							DisposeBoard();
	int								CountSunBeingCollected();
	void							DrawGameObjects(Graphics* g);
	void							ClearCursor();
	/*inline*/ bool					AreEnemyZombiesOnScreen();
	LawnMower*						FindLawnMowerInRow(int theRow);
//  inline bool						SyncState(DataSync& theDataSync) { /* 未发现 */return true; }
	/*inline*/ void					SaveGame(const std::string& theFileName);
	bool							LoadGame(const std::string& theFileName);
	void							InitLevel();
	void							DisplayAdvice(const SexyString& theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex);
	void							StartLevel();
	Plant*							AddPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
	Projectile*						AddProjectile(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType);
	Coin*							AddCoin(int theX, int theY, CoinType theCoinType, CoinMotion theCoinMotion);
	void							RefreshSeedPacketFromCursor();
	ZombieType						PickGraveRisingZombieType();
	ZombieType						PickZombieType(int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker);
	int								PickRowForNewZombie(ZombieType theZombieType);
	/*inline*/ Zombie*				AddZombie(ZombieType theZombieType, int theFromWave);
	void							SpawnZombieWave();
	void							RemoveAllZombies();
	void							RemoveCutsceneZombies();
	void							SpawnZombiesFromGraves();
	PlantingReason					CanPlantAt(int theGridX, int theGridY, SeedType theSeedType);
	virtual void					MouseMove(int x, int y);
	virtual void					MouseDrag(int x, int y);
	virtual void					MouseDown(int x, int y, int theClickCount);
	virtual void					MouseUp(int x, int y, int theClickCount);
	virtual void					KeyChar(SexyChar theChar);
	virtual void					KeyUp(KeyCode theKey);
	virtual void					KeyDown(KeyCode theKey);
	virtual void					Update();
	void							UpdateLayers();
	virtual void					Draw(Graphics* g);
	void							DrawBackdrop(Graphics* g);
	virtual void					ButtonPress  	(int){}
	virtual void					ButtonDepress	(int){}
	virtual void					ButtonDownTick	(int){}
	virtual void					ButtonMouseEnter(int){}
	virtual void					ButtonMouseLeave(int){}
	virtual void					ButtonMouseMove(int, int, int){}
	/*inline*/ void					AddSunMoney(int theAmount);
	bool							TakeSunMoney(int theAmount);
	/*inline*/ bool					CanTakeSunMoney(int theAmount);
	/*inline*/ void					Pause(bool thePause);
	inline bool						MakeEasyZombieType() { /* 未发现 */return false; }
	void							TryToSaveGame();
	/*inline*/ bool					NeedSaveGame();
	/*inline*/ bool					RowCanHaveZombies(int theRow);
	void							ProcessDeleteQueue();
	bool							ChooseSeedsOnCurrentLevel();
	int								GetNumSeedsInBank();
	// @pvz-online: 闯关的卡槽按卡池重填（卡池 ≤8 时才有人调）。InitLevel 建场时一次、
	// 三选一做完放开开场前再一次（见 LawnApp::UpdateRunPick）——不重填的话，卡池 ≤8 的
	// 关卡不开选卡界面，这一关新选的植物赶不上。
	void							FillSeedBankFromRunPool();
	/*inline*/ bool					StageIsNight();
	/*inline*/ bool					StageHasPool();
	/*inline*/ bool					StageHas6Rows();
	/*inline*/ bool					StageHasFog();
	/*inline*/ bool					StageIsDayWithoutPool();
	/*inline*/ bool					StageIsDayWithPool();
	bool							StageHasGraveStones();
	int								PixelToGridX(int theX, int theY);
	int								PixelToGridY(int theX, int theY);
	/*inline*/ int					GridToPixelX(int theGridX, int theGridY);
	int								GridToPixelY(int theGridX, int theGridY);
	/*inline*/ int					PixelToGridXKeepOnBoard(int theX, int theY);
	/*inline*/ int					PixelToGridYKeepOnBoard(int theX, int theY);
	void							UpdateGameObjects();
	bool							MouseHitTest(int x, int y, HitResult* theHitResult);
	void							MouseDownWithPlant(int x, int y, int theClickCount);
	void							MouseDownWithTool(int x, int y, int theClickCount, CursorType theCursorType);
//	inline void						MouseDownNormal(int x, int y, int theClickCount) { /* 未发现 */; }
	bool							CanInteractWithBoardButtons();
	void							DrawProgressMeter(Graphics* g);
	void							UpdateToolTip();
	Plant*							GetTopPlantAt(int theGridX, int theGridY, PlantPriority thePriority);
	void							GetPlantsOnLawn(int theGridX, int theGridY, PlantsOnLawn* thePlantOnLawn);
	/*inline*/ int					CountSunFlowers();
	int								GetSeedPacketPositionX(int theIndex);
	void							AddGraveStones(int theGridX, int theCount, MTRand& theLevelRNG);
	int								GetGraveStoneCount();
	void							ZombiesWon(Zombie* theZombie = nullptr, bool theFromPeer = false);
	// @pvz-online: 漏怪传递。僵尸走到房子前先问这里：传成了就从本棋盘消失（不算漏），
	// 返回 false 才走原版判负。单机、队友没了、末席（没人可传）都是 false。
	bool							TryRelayEscapedZombie(Zombie* theZombie);
	// @pvz-online: 末位推车的"整局一次性"记账（联机闯关）：推车被消耗（触发或被压）时
	// 由 LawnMower 调进来，行号记进 RunState、随检查点持久——用过的行跨关不再补。
	void							NoteMowerConsumed(int theRow);
	// @pvz-online: 漏怪传递的接收侧。队友那儿漏过来的僵尸在本棋盘右侧按原类型重新生成，
	// 只把"还剩多少血"照搬过来。行号/类型是网络来的，越界就不收（返回 nullptr）。
	Zombie*							AddRelayedZombie(int theRow, ZombieType theZombieType, int theBodyHealth, int theHelmHealth, int theShieldHealth, int theFlyingHealth);
	void							DrawLevel(Graphics* g);
	void							DrawShovel(Graphics* g);
	void							UpdateZombieSpawning();
	void							UpdateSunSpawning();
	/*inline*/ void					ClearAdvice(AdviceType theHelpIndex);
	bool							RowCanHaveZombieType(int theRow, ZombieType theZombieType);
	/*inline*/ int					NumberZombiesInWave(int theWaveIndex);
	int								TotalZombiesHealthInWave(int theWaveIndex);
	void							DrawDebugText(Graphics* g);
	void							DrawUICoinBank(Graphics* g);
	/*inline*/ void					ShowCoinBank(int theDuration = 1000);
	void							FadeOutLevel();
	void							DrawFadeOut(Graphics* g);
	void							DrawIce(Graphics* g, int theGridY);
	bool							IsIceAt(int theGridX, int theGridY);
	/*inline*/ ZombieID				ZombieGetID(Zombie* theZombie);
	/*inline*/ Zombie*				ZombieGet(ZombieID theZombieID);
	/*inline*/ Zombie*				ZombieTryToGet(ZombieID theZombieID);
	void							DrawDebugObjectRects(Graphics* g);
	void							UpdateIce();
	/*inline*/ int					GetIceZPos(int theRow);
	/*inline*/ bool					CanAddBobSled();
	/*inline*/ void					ShakeBoard(int theShakeAmountX, int theShakeAmountY);
	int								CountUntriggerLawnMowers();
	bool							IterateZombies(Zombie*& theZombie);
	bool							IteratePlants(Plant*& thePlant);
	bool							IterateProjectiles(Projectile*& theProjectile);
	bool							IterateCoins(Coin*& theCoin);
	bool							IterateLawnMowers(LawnMower*& theLawnMower);
	bool							IterateParticles(TodParticleSystem*& theParticle);
	bool							IterateReanimations(Reanimation*& theReanimation);
	bool							IterateGridItems(GridItem*& theGridItem);
	/*inline*/ Zombie*				AddZombieInRow(ZombieType theZombieType, int theRow, int theFromWave);
	/*inline*/ bool					IsPoolSquare(int theGridX, int theGridY);
	void							PickZombieWaves();
	void							StopAllZombieSounds();
	/*inline*/ bool					HasLevelAwardDropped();
	// @pvz-online: 联机"清完等队友"窗口（2026-10-03 用户定案）：本席位已清完、有队友还没清完。
	// 这个窗口不算"本关已结束"——消费点见 Board.cpp 实现处的清单。
	bool							OnlineWaitingForTeam();
	void							UpdateProgressMeter();
	void							DrawUIBottom(Graphics* g);
	void							DrawUITop(Graphics* g);
	Zombie*							ZombieHitTest(int theMouseX, int theMouseY);
	void							KillAllPlantsInRadius(int theX, int theY, int theRadius);
	Plant*							GetPumpkinAt(int theGridX, int theGridY);
	Plant*							GetFlowerPotAt(int theGridX, int theGridY);
	static bool						CanZombieSpawnOnLevel(ZombieType theZombieType, int theLevel);
	bool							IsZombieWaveDistributionOk();
	void							PickBackground();
	void							InitZombieWaves();
	void							InitSurvivalStage();
	// @pvz-online: 无尽局原地续关（无尽续草坪批，MOD_BUILD 37）：过关不清草坪——老棋盘
	// 直接当下一关的地，这一步只把"下一关"的排上（波次表、雾、推车记账）。原版生存的
	// InitSurvivalStage 是兄弟函数：那边连选卡带开场都重来，这边的选卡 / 开场由
	// LawnApp 一侧在放开开场之前收尾（见 LawnApp::ReleaseRunIntro）。
	void							InitEndlessRunStage();
	static /*inline*/ int			MakeRenderOrder(RenderLayer theRenderLayer, int theRow, int theLayerOffset);
	void							UpdateGame();
	void							InitZombieWavesForLevel(int theForLevel);
	unsigned int					SeedNotRecommendedForLevel(SeedType theSeedType);
	void							DrawTopRightUI(Graphics* g);
	void							DrawFog(Graphics* g);
	void							UpdateFog();
	/*inline*/ int					LeftFogColumn();
	static /*inline*/ bool			IsZombieTypePoolOnly(ZombieType theZombieType);
	void							DropLootPiece(int thePosX, int thePosY, int theDropFactor);
	void							UpdateLevelEndSequence();
	LawnMower*						GetBottomLawnMower();
	bool							CanDropLoot();
	ZombieType						GetIntroducedZombieType();
	void							PickSpecialGraveStone();
	float							GetPosYBasedOnRow(float thePosX, int theRow);
	void							NextWaveComing();
	bool							BungeeIsTargetingCell(int theGridX, int theGridY);
	/*inline*/ int					PlantingPixelToGridX(int theX, int theY, SeedType theSeedType);
	/*inline*/ int					PlantingPixelToGridY(int theX, int theY, SeedType theSeedType);
	Plant*							FindUmbrellaPlant(int theGridX, int theGridY);
	void							SetTutorialState(TutorialState theTutorialState);
	void							DoFwoosh(int theRow);
	void							UpdateFwoosh();
	Plant*							SpecialPlantHitTest(int x, int y);
	void							UpdateMousePosition();
	/*inline*/ Plant*				ToolHitTestHelper(HitResult* theHitResult);
	/*inline*/ Plant*				ToolHitTest(int theX, int theY);
	bool							CanAddGraveStoneAt(int theGridX, int theGridY);
	void							UpdateGridItems();
	/*inline*/ GridItem*			AddAGraveStone(int theGridX, int theGridY);
	int								GetSurvivalFlagsCompleted();
	bool							HasProgressMeter();
	void							UpdateCursor();
	void							UpdateTutorial();
	SeedType						GetSeedTypeInCursor();
	/*inline*/ int					CountPlantByType(SeedType theSeedType);
	bool							PlantingRequirementsMet(SeedType theSeedType);
	bool							HasValidCobCannonSpot();
	bool							IsValidCobCannonSpot(int theGridX, int theGridY);
	bool							IsValidCobCannonSpotHelper(int theGridX, int theGridY);
	void							MouseDownCobcannonFire(int x, int y, int theClickCount);
	int								KillAllZombiesInRadius(int theRow, int theX, int theY, int theRadius, int theRowRange, bool theBurn, int theDamageRangeFlags, int theDirectDamage = 0); // @Patoke: modified function prototype
	// @pvz-online: theDirectDamage = 爆炸直伤基数覆写（0 = 默认 1800×「爆破」乘数）——毁灭菇单株升级「Annihilation」经此传 50000（批八b）。
	/*inline*/ int					GetSeedBankExtraWidth();
	bool							IsFlagWave(int theWaveNumber);
	void							DrawHouseDoorTop(Graphics* g);
	void							DrawHouseDoorBottom(Graphics* g);
	Zombie*							GetBossZombie();
	bool							HasConveyorBeltSeedBank();
	/*inline*/ bool					StageHasRoof();
	void							SpawnZombiesFromPool();
	void							SpawnZombiesFromSky();
	void							PickUpTool(GameObjectType theObjectType);
	void							TutorialArrowShow(int theX, int theY);
	void							TutorialArrowRemove();
	int								CountCoinsBeingCollected();
	void							BungeeDropZombie(BungeeDropGrid* theBungeeDropGrid, ZombieType theZombieType);
	void							SetupBungeeDrop(BungeeDropGrid* theBungeeDropGrid);
	/*inline*/ void					PutZombieInWave(ZombieType theZombieType, int theWaveNumber, ZombiePicker* theZombiePicker);
	/*inline*/ void					PutInMissingZombies(int theWaveNumber, ZombiePicker* theZombiePicker);
	Rect							GetShovelButtonRect();
	void							GetZenButtonRect(GameObjectType theObjectType, Rect& theRect);
	Plant*							NewPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
	void							DoPlantingEffects(int theGridX, int theGridY, Plant* thePlant);
	bool							IsFinalSurvivalStage();
	void							SurvivalSaveScore();
	int								CountZombiesOnScreen();
	int								GetLiveGargantuarCount(); // @Patoke: implemented
	/*inline*/ int					GetNumWavesPerSurvivalStage();
	int								GetLevelRandSeed();
	// @pvz-online: 波表种子的算式本身。开局同步要在建棋盘之前就把种子广播出去
	//（主机还得等队友确认才建棋盘），所以算式得能脱离棋盘实例算——两处走同一份，
	// 免得哪天改了这里忘了那里（theBoardRandSeed 见构造函数：平时就是 mAppRandSeed）。
	static int						ComputeLevelRandSeed(int theBoardRandSeed, bool theAdventureMode,
										int thePlayerId, int theFinishedAdventure, int theLevel,
										int theSurvivalStage, int theGameMode);
	void							AddBossRenderItem(RenderItem* theRenderList, int& theCurRenderItem, Zombie* theBossZombie);
	/*inline*/ GridItem*			GetCraterAt(int theGridX, int theGridY);
	/*inline*/ GridItem*			GetGraveStoneAt(int theGridX, int theGridY);
	/*inline*/ GridItem*			GetLadderAt(int theGridX, int theGridY);
	/*inline*/ GridItem*			AddALadder(int theGridX, int theGridY);
	/*inline*/ GridItem*			AddACrater(int theGridX, int theGridY);
	void							InitLawnMowers();
	/*inline*/ bool					IsPlantInCursor();
	void							HighlightPlantsForMouse(int theMouseX, int theMouseY);
	void							ClearFogAroundPlant(Plant* thePlant, int theSize);
	/*inline*/ void					RemoveParticleByType(ParticleEffect theEffectType);
	/*inline*/ GridItem*			GetScaryPotAt(int theGridX, int theGridY);
	void							PuzzleSaveStreak();
	/*inline*/ void					ClearAdviceImmediately();
	/*inline*/ bool					IsFinalScaryPotterStage();
	/*inline*/ void					DisplayAdviceAgain(const SexyString& theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex);
	GridItem*						GetSquirrelAt(int theGridX, int theGridY);
	GridItem*						GetZenToolAt(int theGridX, int theGridY);
	bool							IsPlantInGoldWateringCanRange(int theMouseX, int theMouseY, Plant* thePlant);
	bool							StageHasZombieWalkInFromRight();
	void							PlaceRake();
	GridItem*						GetRake();
	/*inline*/ bool					IsScaryPotterDaveTalking();
	/*inline*/ Zombie*				GetWinningZombie();
	/*inline*/ void					ResetFPSStats();
	int								CountEmptyPotsOrLilies(SeedType theSeedType);
	GridItem*						GetGridItemAt(GridItemType theGridItemType, int theGridX, int theGridY);
	bool							ProgressMeterHasFlags();
	/*inline*/ bool					IsLastStandFinalStage();
	/*inline*/ int					GetNumWavesPerFlag();
	int								GetCurrentPlantCost(SeedType theSeedType, SeedType theImitaterType);
	/*inline*/ bool					PlantUsesAcceleratedPricing(SeedType theSeedType);
	void							FreezeEffectsForCutscene(bool theFreeze);
	void							LoadBackgroundImages();
	bool							CanUseGameObject(GameObjectType theGameObject);
	void							SetMustacheMode(bool theEnableMustache);
	int								CountCoinByType(CoinType theCoinType);
	void							SetSuperMowerMode(bool theEnableSuperMower);
	void							DrawZenWheelBarrowButton(Graphics* g, int theOffsetY);
	void							DrawZenButtons(Graphics* g);
	/*inline*/ void					OffsetYForPlanting(int& theY, SeedType theSeedType);
	void							SetDanceMode(bool theEnableDance);
	void							SetFutureMode(bool theEnableFuture);
	void							SetPinataMode(bool theEnablePinata);
	void							SetDaisyMode(bool theEnableDaisy);
	void							SetSukhbirMode(bool theEnableSukhbir);
	bool							MouseHitTestPlant(int x, int y, HitResult* theHitResult);
	
	/*inline*/ Reanimation*			CreateRakeReanim(float theRakeX, float theRakeY, int theRenderOrder);
	void							CompleteEndLevelSequenceForSaving();
	void							RemoveZombiesForRepick();
	int								GetGraveStonesCount();
	/*inline*/ bool					IsSurvivalStageWithRepick();
	/*inline*/ bool					IsLastStandStageWithRepick();
	void							DoTypingCheck(KeyCode theKey);
	int								CountZombieByType(ZombieType theZombieType);
	bool							CheckForPostGameAchievements();
	static /*inline*/ bool			IsZombieTypeSpawnedOnly(ZombieType theZombieType);

	// @pvz-online: 局内快捷聊天（T 短语 / E 表情面板 + 顶部横幅）。非模态、纯棋盘自绘：
	// 模态对话框会暂停棋盘并清焦点，所以聊天必须零焦点改动——KeyDown 钩子 + DrawUITop 手画。
	bool							HandleQuickChatKey(KeyCode theKey);
	void							UpdateQuickChat();
	void							DrawQuickChat(Graphics* g);
	void							PushQuickChatBanner(uint8_t theSeat, uint8_t theId);
	void							PushCustomBanner(const char* theTextUtf8);	// id 0 系统提示横幅（发阳光等本机反馈）
	void							CloseQuickChatPanel();
	/*inline*/ bool					QuickChatAvailable();

	// 横幅只存来源席位与编号（1..8 短语、9..16 表情），文字/卡图渲染时查 QuickChat.h；
	// id 0 = 本机系统提示（mCustom 直存 UTF-8 文案，不走协议、不拼席位名前缀）
	struct QuickChatBanner
	{
		uint8_t						mSeat;
		uint8_t						mId;
		int							mFrames;
		char						mCustom[48];
	};
	enum
	{
		QUICK_CHAT_BANNER_MAX		= 8,
		QUICK_CHAT_BANNER_FRAMES	= 300,	// 每条横幅 ≈3 秒，多条依次轮播
		QUICK_CHAT_PANEL_TIMEOUT	= 1000,	// 面板 ≈10 秒无操作自动关（超时也是退路之一）
		QUICK_CHAT_KEY_COOLDOWN		= 18	// 开/关/切/发送后短暂吞键，吸住 WM_KEYDOWN 的自动重复
	};
	QuickChatBanner					mChatBanners[QUICK_CHAT_BANNER_MAX];
	int								mChatBannerCount;
	bool							mChatPanelOpen;
	bool							mChatEmotePage;		// 面板当前页：0 = 短语、1 = 表情
	int								mChatPanelTimer;
	int								mChatInputCooldown;

	// @pvz-online: 观战（队友场地查看）——被看端每 ~15Hz 打包战场快照逐观众单播；
	// 观看端按住 V 时整窗画队友场地（批③），自己的棋盘照跑不暂停。
	// 挂在 Update 的 mPaused 早退之前：被看方暂停时也要推（快照带 PAUSED 位）。
	void							UpdateBoardWatch();
	int								BuildBoardSnapshot(uint8_t* theBuffer, int theCapacity);
	bool							HandleBoardWatchKey(KeyCode theKey);	// KeyDown 钩子；true = 吞键
	bool							IsWatchingView();						// V 按着 + 订阅在 + 局内
	uint8_t							PickDefaultWatchSeat();
	void							DrawBoardWatch(Graphics* g);			// 全屏观看层（只读快照）
	void							DrawWatchNotice(Graphics* g);			// 结束提示（超时/离开）

	NetSession::ViewSnapshot		mRemoteSnapshot;		// 最新一份队友战场定格（观看端）
	bool							mHasRemoteSnapshot;
	NetSession::WatchEnd			mWatchEndSeen;			// 待展示的结束原因（TIMEOUT/TARGET_LEFT；展示完清回 NONE）
	int								mWatchNoticeTimer;		// 结束提示残留帧数（~2 秒）
	bool							mWatchKeyHeld;			// 本机 V 按住中（结束观看也要等松手再按）
	bool							mWatchRearmBlocked;		// ESC 退场后压住重进，直到 V 真的松手（防自动重复）
	uint8_t							mWatchLastSeat;			// 上次看的席位（再按 V 还看他；SEAT_UNSET = 无）
	int								mWatchSnapshotTicker;	// 被看端发送节拍（每 7 帧 ≈ 15Hz 一份）

	// @pvz-online: 关底巨型 boss（MOD_BUILD 38，2026-10-10 用户定案）：闯关最后一关开始时
	// 在第一席棋盘刷一只巨型红眼巨人（全控制免疫、砸击无视耐砸），本 ID 用来在各处
	// 识别它（ZOMBIEID_NULL = 本关没有）。每关 InitLevel 重置；漏怪照普通传递口径接力。
	ZombieID						mRunBossZombieID;
	bool							IsRunBossZombie(Zombie* theZombie);
	void							SpawnRunBoss();

	// @pvz-online: 发阳光（MOD_BUILD 38，2026-10-10 用户改键交互）：按 G 进选择态，
	// 1-6 直选队友席位定向发一档阳光（到账额 = 档位 - 税）；收款方场上天降对应
	// 面额的演出阳光（纯视觉，钱直接入账）。选择态 5 秒无操作自动退出（等待有出路）。
	int								mSunGiftCooldownFrames;
	int								mSunGiftSelectFrames;	// 发阳光选择态剩余帧（0 = 不在选择态）
	bool							HandleSunGiftKey(KeyCode theKey);	// KeyDown 钩子；true = 吞键
	bool							TrySendSunGift(uint8_t theTargetSeat);
	void							GiveSunGift(int theAmount, uint8_t theFromSeat);	// LawnApp 每帧消费队列时调
};
extern bool gShownMoreSunTutorial;

int									GetRectOverlap(const Rect& rect1, const Rect& rect2);
bool								GetCircleRectOverlap(int theCircleX, int theCircleY, int theRadius, const Rect& theRect);
/*inline*/ void						BoardInitForPlayer();

#endif // __BOARD_H__