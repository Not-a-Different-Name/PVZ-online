#ifndef __LAWNAPP_H__
#define __LAWNAPP_H__

#include "ConstEnums.h"
#include "SexyAppFramework/SexyApp.h"
#include "Sexy.TodLib/TodFoley.h"

class Board;
class GameSelector;
class ChallengeDefinition;
class SeedChooserScreen;
class AwardScreen;
class CreditScreen;
class TodFoley;
class PoolEffect;
class ZenGarden;
class PottedPlant;
class EffectSystem;
class TodParticleSystem;
class Reanimation;
class ReanimatorCache;
class ProfileMgr;
class PlayerInfo;
class Music;
class TitleScreen;
class PopDRMComm;
class ChallengeScreen;
class StoreScreen;
class AlmanacDialog;
class TypingCheck;
class NetSession;

namespace Sexy
{
	class Dialog;
	class Graphics;
	class ButtonWidget;
};

enum FoleyType;

using namespace Sexy;

typedef std::list<ButtonWidget*> ButtonList;
typedef std::list<Image*> ImageList;

class LevelStats
{
public:
	int								mUnusedLawnMowers;

public:
	LevelStats() { Reset(); }
	inline void						Reset() { mUnusedLawnMowers = 0; }
};

class LawnApp : public SexyApp
{
public:
	Board*							mBoard;											//+0x768
	TitleScreen*					mTitleScreen;									//+0x76C
	GameSelector*					mGameSelector;									//+0x770
	SeedChooserScreen*				mSeedChooserScreen;								//+0x774
	AwardScreen*					mAwardScreen;									//+0x778
	CreditScreen*					mCreditScreen;									//+0x77C
	ChallengeScreen*				mChallengeScreen;								//+0x780
	TodFoley*						mSoundSystem;									//+0x784
	ButtonList						mControlButtonList;								//+0x788
	ImageList						mCreatedImageList;								//+0x794
	std::string						mReferId;										//+0x7A0
	std::string						mRegisterLink;									//+0x7BC
	std::string						mMod;											//+0x7D8
	bool							mRegisterResourcesLoaded;						//+0x7F4
	bool							mTodCheatKeys;									//+0x7F5
	GameMode						mGameMode;										//+0x7F8
	GameScenes						mGameScene;										//+0x7FC
	bool							mLoadingZombiesThreadCompleted;					//+0x800
	bool							mFirstTimeGameSelector;							//+0x801
	int								mGamesPlayed;									//+0x804
	int								mMaxExecutions;									//+0x808
	int								mMaxPlays;										//+0x80C
	int								mMaxTime;										//+0x810
	bool							mEasyPlantingCheat;								//+0x814
	PoolEffect*						mPoolEffect;									//+0x818
	ZenGarden*						mZenGarden;										//+0x81C
	EffectSystem*					mEffectSystem;									//+0x820
	ReanimatorCache*				mReanimatorCache;								//+0x824
	ProfileMgr*						mProfileMgr;									//+0x828
	PlayerInfo*						mPlayerInfo;									//+0x82C
	LevelStats*						mLastLevelStats;								//+0x830
	bool							mCloseRequest;									//+0x834
	int								mAppCounter;									//+0x838
	Music*							mMusic;											//+0x83C
	ReanimationID					mCrazyDaveReanimID;								//+0x840
	CrazyDaveState					mCrazyDaveState;								//+0x844
	int								mCrazyDaveBlinkCounter;							//+0x848
	ReanimationID					mCrazyDaveBlinkReanimID;						//+0x84C
	int								mCrazyDaveMessageIndex;							//+0x850
	SexyString						mCrazyDaveMessageText;							//+0x854
	int								mAppRandSeed;									//+0x870
	HICON							mBigArrowCursor;								//+0x874
	PopDRMComm*						mDRM;											//+0x878
	intptr_t						mSessionID;										//+0x87C
	int								mPlayTimeActiveSession;							//+0x880
	int								mPlayTimeInactiveSession;						//+0x884
	BoardResult						mBoardResult;									//+0x888
	bool							mSawYeti;										//+0x88C
	TypingCheck*					mKonamiCheck;									//+0x890
	TypingCheck*					mMustacheCheck;									//+0x894
	TypingCheck*					mMoustacheCheck;								//+0x898
	TypingCheck*					mSuperMowerCheck;								//+0x89C
	TypingCheck*					mSuperMowerCheck2;								//+0x8A0
	TypingCheck*					mFutureCheck;									//+0x8A4
	TypingCheck*					mPinataCheck;									//+0x8A8
	TypingCheck*					mDanceCheck;									//+0x8AC
	TypingCheck*					mDaisyCheck;									//+0x8B0
	TypingCheck*					mSukhbirCheck;									//+0x8B4
	bool							mMustacheMode;									//+0x8B8
	bool							mSuperMowerMode;								//+0x8B9
	bool							mFutureMode;									//+0x8BA
	bool							mPinataMode;									//+0x8BB
	bool							mDanceMode;										//+0x8BC
	bool							mDaisyMode;										//+0x8BD
	bool							mSukhbirMode;									//+0x8BE
	TrialType						mTrialType;										//+0x8C0
	bool							mDebugTrialLocked;								//+0x8C4
	bool							mMuteSoundsForCutscene;							//+0x8C5
	// @pvz-online: M2 联机会话。放在类尾：上面的 //+0x… 是反编译出来的偏移，不扰动它们。
	// 只有 DoOnlineDialog 会按需 new，连着的是哪台机器都记在 NetSession 里。
	NetSession*						mOnlineSession;
	// @pvz-online: 联机开局参数（主机广播来的关卡 + 波表种子）。只有客户端用得上：
	// Board 建棋盘时来取，取完就清；全程不碰 mPlayerInfo，客户端自己的存档进度不动。
	bool							mHasOnlineStart;
	int								mOnlineStartLevel;
	int								mOnlineStartSeed;
	// 上一帧是不是已连接：联机面板只在"刚连上"那一下自动收起，之后玩家再点开就留在
	// 屏幕上（不然面板一开就被按回去，等于打不开）。
	bool							mOnlineWasConnected;
	// 主机广播完开局命令、正等队友 START_ACK 的那段时间。这期间玩家还留在主菜单上
	// （参数已经按覆盖值定好），ACK 一到才 NewGame()——两边进场只差一个单程。
	bool							mOnlineWaitingStartAck;

public:
	LawnApp();
	virtual ~LawnApp();

	bool							KillNewOptionsDialog();
	virtual void					GotFocus();
	virtual void					LostFocus();
	virtual void					InitHook();
	virtual void					WriteToRegistry();
	virtual void					ReadFromRegistry();
	virtual void					LoadingThreadProc();
	virtual void					LoadingCompleted();
	virtual void					LoadingThreadCompleted();
	virtual void					URLOpenFailed(const std::string& theURL);
	virtual void					URLOpenSucceeded(const std::string& theURL);
	virtual bool					OpenURL(const std::string& theURL, bool shutdownOnOpen);
	virtual bool					DebugKeyDown(int theKey);
	virtual void					HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue);
	void							ConfirmQuit();
	void							ConfirmCheckForUpdates() { ; }
	void							CheckForUpdates() { ; }
	void							DoUserDialog();
	void							FinishUserDialog(bool isYes);
	void							DoCreateUserDialog();
	void							DoCheatDialog();
	void							DoOnlineDialog(); // @pvz-online: M2 联机面板
	void							FinishCheatDialog(bool isYes);
	void							FinishCreateUserDialog(bool isYes);
	void							DoConfirmDeleteUserDialog(const SexyString& theName);
	void							FinishConfirmDeleteUserDialog(bool isYes);
	void							DoRenameUserDialog(const SexyString& theName);
	void							FinishRenameUserDialog(bool isYes);
	void							FinishNameError(int theId);
	void							FinishRestartConfirmDialog();
	void							DoConfirmSellDialog(const SexyString& theMessage);
	void							DoConfirmPurchaseDialog(const SexyString& theMessage);
	void							FinishTimesUpDialog();
	void							KillBoard();
	void							MakeNewBoard();
	void							StartPlaying();
	bool							TryLoadGame();
	void							NewGame();
	void							PreNewGame(GameMode theGameMode, bool theLookForSavedGame);
	// @pvz-online: 联机相关。IsOnlineGame = 双方已连上（这局就是联机局）；
	// IsOnlineStartAllowed = 现在能不能开局（单机永远可以；联机只有已连上的主机可以）。
	bool							IsOnlineGame();
	bool							IsOnlineStartAllowed();
	void							SetOnlineStartOverride(int theLevel, int theSeed);
	bool							GetOnlineStartOverride(int& theLevel, int& theSeed);
	void							ClearOnlineStartOverride();
	// 主机正等队友确认进场（面板/小条据此显示"等队友就位"）。
	bool							IsOnlineWaitingStartAck() const { return mOnlineWaitingStartAck; }
	void							UpdateOnlineStart();
	void							UpdateOnlineRelay();
	void							ShowGameSelector();
	void							KillGameSelector();
	void							ShowAwardScreen(AwardType theAwardType, bool theShowAchievements); // @Patoke: add argument
	void							KillAwardScreen();
	void							ShowSeedChooserScreen();
	void							KillSeedChooserScreen();
	void							DoHighScoreDialog();
	void							DoBackToMain();
	void							DoConfirmBackToMain();
	void							DoNewOptions(bool theFromGameSelector);
	void							DoRegister();
	void							DoRegisterError();
	bool							CanDoRegisterDialog();
	/*inline*/ bool					WriteCurrentUserConfig();
	void							DoNeedRegisterDialog();
	void							DoContinueDialog();
	void							DoPauseDialog();
	void							FinishModelessDialogs();
	virtual Dialog*					DoDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	virtual Dialog*					DoDialogDelay(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	virtual void					Shutdown();
	virtual void					Init();
	virtual void					Start();
	virtual Dialog*					NewDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	virtual bool					KillDialog(int theDialogId);
	virtual void					ModalOpen();
	virtual void					ModalClose();
	virtual void					PreDisplayHook();
	virtual bool					ChangeDirHook(const char* theIntendedPath);
	virtual bool					NeedRegister();
	virtual void					UpdateRegisterInfo();
	virtual void					ButtonPress(int theId);
	virtual void					ButtonDepress(int theId);
	virtual void					ButtonDownTick(int theId);
	virtual void					ButtonMouseEnter(int theId);
	virtual void					ButtonMouseLeave(int theId);
	virtual void					ButtonMouseMove(int theId, int theX, int theY);
	virtual void					UpdateFrames();
	virtual bool					UpdateApp();
	/*inline*/ bool					IsAdventureMode();
	/*inline*/ bool					IsSurvivalMode();
	bool							IsContinuousChallenge();
	/*inline*/ bool					IsArtChallenge();
	bool							NeedPauseGame();
	virtual void					ShowResourceError(bool doExit = false);
	void							ToggleSlowMo();
	void							ToggleFastMo();
	void							PlayFoley(FoleyType theFoleyType);
	void							PlayFoleyPitch(FoleyType theFoleyType, float thePitch);
	void							PlaySample(int theSoundNum);
	void							FastLoad(GameMode theGameMode);
	static SexyString				GetStageString(int theLevel);
	/*inline*/ void					KillChallengeScreen();
	void							ShowChallengeScreen(ChallengePage thePage);
	ChallengeDefinition&			GetCurrentChallengeDef();
	void							CheckForGameEnd();
	virtual void					CloseRequestAsync();
	/*inline*/ bool					IsChallengeWithoutSeedBank();
	AlmanacDialog*					DoAlmanacDialog(SeedType theSeedType = SeedType::SEED_NONE, ZombieType theZombieType = ZombieType::ZOMBIE_INVALID);
	bool							KillAlmanacDialog();
	int								GetSeedsAvailable();
	Reanimation*					AddReanimation(float theX, float theY, int theRenderOrder, ReanimationType theReanimationType);
	TodParticleSystem*				AddTodParticle(float theX, float theY, int theRenderOrder, ParticleEffect theEffect);
	/*inline*/ ParticleSystemID		ParticleGetID(TodParticleSystem* theParticle);
	/*inline*/ TodParticleSystem*	ParticleGet(ParticleSystemID theParticleID);
	/*inline*/ TodParticleSystem*	ParticleTryToGet(ParticleSystemID theParticleID);
	/*inline*/ ReanimationID		ReanimationGetID(Reanimation* theReanimation);
	/*inline*/ Reanimation*			ReanimationGet(ReanimationID theReanimationID);
	/*inline*/ Reanimation*			ReanimationTryToGet(ReanimationID theReanimationID);
	void							RemoveReanimation(ReanimationID theReanimationID);
	void							RemoveParticle(ParticleSystemID theParticleID);
	StoreScreen*					ShowStoreScreen();
	void							KillStoreScreen();
	bool							HasSeedType(SeedType theSeedType);
	/*inline*/ bool					SeedTypeAvailable(SeedType theSeedType);
	/*inline*/ void					EndLevel();
	inline bool						IsIceDemo() { return false; }
	/*inline*/ bool					IsShovelLevel();
	/*inline*/ bool					IsWallnutBowlingLevel();
	/*inline*/ bool					IsMiniBossLevel();
	/*inline*/ bool					IsSlotMachineLevel();
	/*inline*/ bool					IsLittleTroubleLevel();
	/*inline*/ bool					IsStormyNightLevel();
	/*inline*/ bool					IsFinalBossLevel();
	/*inline*/ bool					IsBungeeBlitzLevel();
	static /*inline*/ SeedType		GetAwardSeedForLevel(int theLevel);
	SexyString						GetCrazyDaveText(int theMessageIndex);
	/*inline*/ bool					CanShowAlmanac();
	/*inline*/ bool					IsNight();
	/*inline*/ bool					CanShowStore();
	/*inline*/ bool					HasBeatenChallenge(GameMode theGameMode);
	PottedPlant*					GetPottedPlantByIndex(int thePottedPlantIndex);
	static /*inline*/ bool			IsSurvivalNormal(GameMode theGameMode);
	static /*inline*/ bool			IsSurvivalHard(GameMode theGameMode);
	static /*inline*/ bool			IsSurvivalEndless(GameMode theGameMode);
	/*inline*/ bool					HasFinishedAdventure();
	/*inline*/ bool					IsFirstTimeAdventureMode();
	/*inline*/ bool					CanSpawnYetis();
	void							CrazyDaveEnter();
	void							UpdateCrazyDave();
	void							CrazyDaveTalkIndex(int theMessageIndex);
	void							CrazyDaveTalkMessage(const SexyString& theMessage);
	void							CrazyDaveLeave();
	void							DrawCrazyDave(Graphics* g);
	void							CrazyDaveDie();
	void							CrazyDaveStopTalking();
	void							PreloadForUser();
	int								GetNumPreloadingTasks();
	int								LawnMessageBox(int theDialogId, const SexyChar* theHeaderName, const SexyChar* theLinesName, const SexyChar* theButton1Name, const SexyChar* theButton2Name, int theButtonMode);
	virtual void					EnforceCursor();
	void							ShowCreditScreen();
	void							KillCreditScreen();
	static SexyString				Pluralize(int theCount, const SexyChar* theSingular, const SexyChar* thePlural);
	int								GetNumTrophies(ChallengePage thePage);
	/*inline*/ bool					EarnedGoldTrophy();
	inline bool						IsRegistered() { return false; }
	inline bool						IsExpired() { return false; }
	inline bool						IsDRMConnected() { return false; }
	/*inline*/ bool					IsScaryPotterLevel();
	static /*inline*/ bool			IsEndlessScaryPotter(GameMode theGameMode);
	/*inline*/ bool					IsSquirrelLevel();
	/*inline*/ bool					IsIZombieLevel();
	/*inline*/ bool					CanShowZenGarden();
	static SexyString				GetMoneyString(int theAmount);
	bool							AdvanceCrazyDaveText();
	/*inline*/ bool					IsWhackAZombieLevel();
	void							UpdatePlayTimeStats();
	void							BetaAddFile(std::list<std::string>& theUploadFileList, std::string theFileName, std::string theShortName);
	bool							CanPauseNow();
	/*inline*/ bool					IsPuzzleMode();
	/*inline*/ bool					IsChallengeMode();
	static /*inline*/ bool			IsEndlessIZombie(GameMode theGameMode);
	void							CrazyDaveDoneHanding();
	inline SexyString				GetCurrentLevelName() { return _S("Unknown"); }
	/*inline*/ int					TrophiesNeedForGoldSunflower();
	/*inline*/ int					GetCurrentChallengeIndex();
	void							LoadGroup(const char* theGroupName, int theGroupAveMsToLoad);
//	void							TraceLoadGroup(const char* theGroupName, int theGroupTime, int theTotalGroupWeigth, int theTaskWeight);
	void							CrazyDaveStopSound();
	/*inline*/ bool					IsTrialStageLocked();
	/*inline*/ void					FinishZenGardenToturial();
	bool							UpdatePlayerProfileForFinishingLevel();
	bool							SaveFileExists();
	/*inline*/ bool					CanDoPinataMode();
	/*inline*/ bool					CanDoDanceMode();
	/*inline*/ bool					CanDoDaisyMode();
	virtual void					SwitchScreenMode(bool wantWindowed, bool is3d, bool force = false);
	static /*inline*/ void			CenterDialog(Dialog* theDialog, int theWidth, int theHeight);
};

SexyString							LawnGetCurrentLevelName();
bool								LawnGetCloseRequest();
bool								LawnHasUsedCheatKeys();
void								BetaSubmitFunc();

extern bool (*gAppCloseRequest)();				//[0x69E6A0]
extern bool (*gAppHasUsedCheatKeys)();			//[0x69E6A4]
extern SexyString (*gGetCurrentLevelName)();

extern bool gIsPartnerBuild;
extern bool gFastMo;  //0x6A9EAB
extern bool gSlowMo;  //0x6A9EAA
extern LawnApp* gLawnApp;  //0x6A9EC0
extern int gSlowMoCounter;  //0x6A9EC4


#endif	// __LAWNAPP_H__