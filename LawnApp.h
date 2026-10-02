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
class RunState;

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
	// 这次等待已经持续了多少帧。等太久（对面卡了/忙）就当这次开局作废，人留在菜单上，
	// 总好过盯着一个点不动的界面。
	int								mOnlineStartWaitFrames;
	// 上一帧那个暂停菜单（DIALOG_NEWOPTIONS）开着没有 + 它是不是"替队友弹的"。
	// 联机同步靠这两个：跟上一帧比就知道本机刚才是暂停了还是继续了（不用在
	// 每个开合入口挂钩子）；掉线时要收掉的是"替队友弹的那张"，玩家自己按的不动。
	bool							mPauseMenuWasOpen;
	bool							mPauseMenuFromPeer;
	// 棋盘上那句"等队友们"是不是我们挂上去的。是的话状态一变要由我们收掉——
	// 不记这一笔就会把别人（戴夫、波次提示）的 advice 一起擦掉。
	bool							mOnlineWaitingAdviceOn;
	// @pvz-online: 闯关（肉鸽）模式的状态。只在"闯关的一关正在打（含刚点完入口、
	// 正要进场）"时非空——回主菜单即删，检查点留在 userdata/run%d.dat，续关时重新读。
	// IsRunMode() 就认这个指针。
	RunState*						mRunState;
	// @pvz-online: 闯关入口（主位那块 ADVENTURE 大墓碑）被按下了。按下时只摆一个标记，
	// 真开局全在主循环：要拆面板、拆主菜单、建棋盘，还可能先弹一个"续不续"的询问框
	// （询问框是 WaitForResult，只能从主循环里调）。见 LawnApp::RequestAdventure。
	bool							mPendingAdventure;

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
	// 我这台是"跟着主机走"的那一头吗（客户端）——全队败后开不开新局由主机一个人定，
	// 所以 GameOverDialog 得先分得出主客（见 LawnApp::RetryOnlineLevel）。
	bool							IsOnlineClient();
	void							SetOnlineStartOverride(int theLevel, int theSeed);
	bool							GetOnlineStartOverride(int& theLevel, int& theSeed);
	void							ClearOnlineStartOverride();
	// 主机正等队友确认进场（面板/小条据此显示"等队友就位"）。
	bool							IsOnlineWaitingStartAck() const { return mOnlineWaitingStartAck; }
	// 现在点开局，会不会走"广播命令、等队友 START_ACK"那条路（= 已连上的主机）。
	// 会的话主菜单得先留着不能拆——不然等待的那几秒屏幕上什么都没有（见 GameSelector::Update）。
	bool							WillWaitForStartAck();
	void							UpdateOnlineStart();
	void							UpdateOnlineRelay();
	// @pvz-online: 队友退关了我这边跟着退。theNotifyOnline=false 用于"是我先退的/我是被通知的"，
	// 免得两边互相回话形成回声。
	void							UpdateOnlineLevelExit();
	// @pvz-online: 队友暂停/继续了，本机跟着弹/收暂停菜单；本机自己开了关了也告诉队友。
	// 任一方都能暂停、也任一方都能继续（共识模型，不搞请求/同意）。
	void							UpdateOnlinePause();
	// @pvz-online: 会话事件（连上了 / 掉线了）的收口。以前没人取，事件在队列里越堆越多。
	void							UpdateOnlineEvents();
	// @pvz-online: 这一关对全队结束没有——报"我清完了"、全队清完就一起回菜单、
	// 队友那边报了全队败就跟着收摊。棋盘侧的收口都在这儿（见函数上的注释）。
	void							UpdateOnlineEnd();
	// @pvz-online: 全队败之后主机按了 Try Again——整队重来同一关（同关卡号、同波表种子）。
	// 客户端那台没有这个入口，只等着跟进来（见 UpdateOnlineStart 里那段）。
	void							RetryOnlineLevel();
	// @pvz-online: 闯关（肉鸽）。入口是主菜单主位那块烤字 ADVENTURE 的大墓碑（第三槽的
	// PUZZLE 石板是原版战役入口，不走这条路）：没队伍先把组队面板叫出来，
	// 队伍在手（主菜单左上角的小状态条随时能把面板叫回来）再由主机起闯关；
	// 队友连着的时候还走原来的单关联机流程，R5 才把队友拉进闯关——同一套关卡流水线。
	// IsRunMode = 现在这一局是闯关局——棋盘侧据此关掉小推车与续玩存档、种子栏按卡池填、
	// "首次冒险"的特殊待遇一律不算。
	bool							IsRunMode() const { return mRunState != nullptr; }
	RunState*						GetRunState() { return mRunState; }
	// "冒险"牌按下：true = 已经受理（开面板或排队等开局），false = 落回原来的单关联机流程。
	bool							RequestAdventure();
	void							UpdateAdventureRequest();
	void							StartRun();
	void							ContinueRun();
	void							EnterRunLevel();
	void							UpdateRunEnd();
	// @pvz-online: 闯关的三选一屏（R2）。该选而屏不在（刚开局、刚过完一关、或者屏被谁关掉了）
	// 就开一张；玩家点了卡由 RunPickChosen 接着办：把卡收进局里，选够了就进下一关。
	// 屏和关卡互斥——棋盘在的时候这一屏不该出现（换关的空档里 mBoard 一定是空的）。
	void							UpdateRunPick();
	void							RunPickChosen(int theIndex);
	// @pvz-online: R4 闯关输一关：失败计数 +1 并立刻写检查点（首版只存不用，
	// 惩罚留平衡阶段）。由 Board::ZombiesWon 在判负的那一下调。
	void							RunNoteFailure();
	// @pvz-online: R3 的 buff 数值取用口。非闯关局 / 没拿到这条 = 中性值（乘数 1.0、
	// 加数 0），调用点写一行就够，不用自己判 mRunState。数值本体在 RunBuffs.cpp 的表里。
	float							RunBuffMul(int theBuffId) const;
	int								RunBuffAdd(int theBuffId) const;
	// @pvz-online: R3 单株升级的取用口——按"植物"问（落点手里只有植物类型）。
	// 表里没有这株 / 没拿到 / 不在闯关 = 中性值，同全局 buff。
	float							RunPlantUpgradeMul(SeedType thePlant) const;
	int								RunPlantUpgradeCount(SeedType thePlant) const;
	void							ShowGameSelector();
	void							KillGameSelector();
	void							ShowAwardScreen(AwardType theAwardType, bool theShowAchievements); // @Patoke: add argument
	void							KillAwardScreen();
	void							ShowSeedChooserScreen();
	void							KillSeedChooserScreen();
	void							DoHighScoreDialog();
	void							DoBackToMain(bool theNotifyOnline = true);
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