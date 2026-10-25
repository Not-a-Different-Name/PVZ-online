#include <memory>
#include "Lawn/Observer/ObserverState.h"
class CoopPlay;
class TeamProgressionDialog;
struct TeamProgressionUiState;
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

#ifndef __LAWNAPP_H__
#define __LAWNAPP_H__

#include "ConstEnums.h"
#include "Lawn/System/BattleMusicState.h"
#include "Lawn/System/DamageStats.h"
#include "Lawn/Run/TeamProgressionView.h"
#include "Lawn/Online/NetProtocol.h"
#include "SexyAppFramework/SexyApp.h"
#include "PvzpLib/PvzpFoley.h"
#include <memory>

class Board;
class GameSelector;
class ChallengeDefinition;
class SeedChooserScreen;
class AwardScreen;
class CreditScreen;
class PvzpFoley;
class PoolEffect;
class ZenGarden;
class PottedPlant;
class EffectSystem;
class PvzpParticleSystem;
class Reanimation;
class ReanimatorCache;
class ProfileMgr;
class PlayerInfo;
class Music;
class TitleScreen;
class ChallengeScreen;
class StoreScreen;
class AlmanacDialog;
#include "Lawn/Run/RunBuffs.h"
class RunState;
class NetSession;
class RunFlowScreen;
class TypingCheck;

namespace Sexy
{
	class Dialog;
	class Graphics;
	class ButtonWidget;
};

using namespace Sexy;

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
    Observer::InputFence mObserverInput;bool mObserverTextTail=false;
    bool FilterObserverInput(int,int=0,int=0,int=0)override;
    std::unique_ptr<CoopPlay> mCoopPlay;
    BattleMusicState mBattleMusic;
    DamageStats mDamageStats;
    bool SendCoopQuickChat(uint8_t id);
    void UpdateCoopPlay();void ShowCoopPlay();void ExpireCoopLoan();

    int RunExtraBuffCount(RunPlantBuff2Id index) const;
    void AwardRunRerollVoucher();
    void RefreshRunChoicePlants(int id, int before);

	Board*							mBoard;
	// @pvz-online: run progress is independent of the original adventure profile.
	std::unique_ptr<RunState> mRunState;
	std::unique_ptr<RunFlowScreen> mRunFlowScreen;

    std::unique_ptr<NetSession> mOnlineSession;
    bool mSlideCollect = true;
    bool mHasOnlineStart = false, mOnlineWasConnected = false, mOnlineWaitingStartAck = false;
    int mOnlineStartLevel = 0, mOnlineStartSeed = 0, mOnlineStartWaitFrames = 0;
    // 孤儿 setup 相自救的节拍器：房主停在菜单、房间却停在 setup 相（放弃时的 op5 丢了）
    // 时，按这个计数每 200 帧重发一次「回菜单」。
    int mOrphanSetupTicks = 0;
    bool mOnlineRunStartHeld = false, mOnlineRunGo = false, mRunIntroHeld = false;
    bool mOnlineStartPromptActive = false, mOnlineSeedsHeld = false, mOnlineWaitingAdviceOn = false;
    bool mShowedStartupAnnounce = false;
    bool mPauseMenuWasOpen = false, mPauseMenuFromPeer = false;
    NetProto::MsgStartLevel mOnlineStartPromptMsg{};
    int mOnlineStartPromptFrames = 0;
    std::string mOnlineAddress = "127.0.0.1", mOnlineServer = "101.200.131.182", mOnlineRoomCode;
    bool IsLocalOnlyMode(GameMode mode) const;
    bool IsOnlineStartLobby() const;
    void LeaveLocalSceneForOnlineStart();
    bool mZombieVoteActive = false;
    int mZombieVoteLevel = -1, mZombieVoteMyChoice = -1, mZombieVoteFrames = 0;
    unsigned char mZombieVoteCandidates[3]{};
    int mZombieVoteVotes[3]{}, mZombieVoteAbstains = 0, mZombieVoteVotedSeats = 0;
    unsigned int mZombieVoteSeatMask = 0;
    int mZombieVoteRoundsDone = 0;   // 本场景已结的票数（两屏制 MOD_BUILD 56：档 2 结第一票后仍 <2，路口再开第二屏；重开游戏归 0——配合 mZombieVotedLevel 区分"恢复"与"待第二屏"）
    int mZombieVoteOpenRound=0; // Round at OPEN; RESULT and SHARED can arrive on different queues.
    unsigned char mZombieVoteBannerIds[2]{};  // 本场景结果横幅累积（每屏各记 1 条，全结后合成一条横幅推）
    int mZombieVoteBannerCount = 0;
    std::string mZombieVotePendingBanner;
    bool TryOpenZombieVote();
    void UpdateZombieVote();
    void ZombieVoteChosen(int choice);
    std::string ZombieVoteStatusText();
    void ZombieVoteOpenLocal(int level, const unsigned char* candidates);
    void ZombieVoteRegister(int seat, int choice);
    unsigned int ZombieVoteRequiredSeatMask();
    int ZombieVoteExpectedVoters();
    void ZombieVoteResolve();
    void ZombieVoteCloseUI();
    void ResetZombieVote();
    void DisconnectOnline();
    void AlignRunToHost(int seed, int index, int mode, int difficulty, int scale, int tempo,
        int zombotany, int scene, int boss, int buffMult, int ladder, int zombieBuff, const uint8_t* stacks,int speedTier=0, int firstWave=0, int endlessOptions=0, int crowdRule=0);
    bool IsOnlineGame();
    bool IsOnlineStartAllowed();
    bool IsOnlineClient();
    bool WillWaitForStartAck();
    void SetOnlineStartOverride(int theLevel, int theSeed);
    bool GetOnlineStartOverride(int& theLevel, int& theSeed);
    void ClearOnlineStartOverride();
    void UpdateOnlineStart();
    void OnlineStartWaitCancelled();
    void OnlineReconnectCancelled();
    void EnterOnlineStart(const NetProto::MsgStartLevel& theMsg);
    void ShowOnlineStartPrompt(const NetProto::MsgStartLevel& theMsg);
    void OnlineStartPromptAnswer(bool theAccepted);
    void DismissOnlineStartPrompt(bool theSendAck);
    bool TryHoldSeedChooserForTeammates();
    bool FinalizePreparationSeats();
    int mAdmissionIdentityProfile=-1,mAdmissionReportTicks=100;
    bool mAdmissionDirty=false,mAdmissionPressed=false;
    void InitializeAdmissionIdentity();
    void UpdateEndlessAdmission();
    void ApplyAdmissionOffer(int targetIndex);
    void SendAdmissionStart(uint8_t seat);
    bool AdmissionBuilding() const;
    void BeginAdmissionBuilding();
    void FinishAdmissionBuilding();
    void StartAdmissionFastPicks();
    // @pvz-online: preparation metadata lives outside the raw-serialized Board layout.
    int mPreparationBuiltSeat=0,mPreparationBuiltSunBonus=0,mPreparationBuiltMultiplier=0;
    bool mPreparationRulesApplied=false,mPreparationSaved=false;
    bool mPreparationEditRequested=false;
    uint32_t mPreparationConfiguredRevision=0;
    void ToggleOnlineSeedsReady();
    void UpdateOnlineSeeds();
    void UpdateOnlineRelay();
    void UpdateOnlineLevelExit();
    void UpdateOnlinePause();
    void UpdateOnlineEvents();
    void UpdateOnlineEnd();
    void RetryOnlineLevel();
    void EnterRunLevel();
    bool TryKeepEndlessLawn();
    void ReleaseRunIntro();
    void UpdateRunPick();
    int OnlineSeatMultiplier();
    int OnlineSeatMultiplierForSeat(uint8_t theSeat);   // @pvz-online: 按席位号查同一条顺位曲线（换位预览用），2026-10-15
    int OnlineStartSunBonus();
    int OnlineStartSunBonusForSeat(uint8_t theSeat);    // @pvz-online: 换位预览用
    void DoOnlineDialog();

	void ShowRunMenu(int endlessScene=-1);
	void StartRun(int mode, int buffMult = 0, int difficulty = 1, int scale = 1, int tempo = 1, int zombotany = 0, int endlessScene = 0, int bossFlag = 2, int ladder = 0, int zombieBuff = 1,int speedTier=0, int firstWave=0, int endlessOptions=141);
	bool RunPickPaidReroll(int lockedChoice, unsigned int expectedCounter);
	void ResumeRun(int mode=-1, int scene=0);
	void ShowRunPicks();
	void StartRunLevel();
	void RetryRunLevel();
	void NoteRunFailure();
	void KillRunFlowScreen();
	float RunBuffMul(int id) const;
    bool RunSkySunAtNight() const;
    float RunSkySunIntervalMul() const;
    int RunMirrorStacks() const;
    int RunBuffAdd(int theBuffId) const;
    float RunPlantUpgradeMul(SeedType thePlant) const;
    int RunPlantUpgradeCount(SeedType thePlant) const;
    float RunPlantUpgradeMulKind(SeedType thePlant, RunPlantUpgradeKind theKind) const;
    int RunPlantUpgradeCountKind(SeedType thePlant, RunPlantUpgradeKind theKind) const;
    float RunPlantBuff2Mul(SeedType thePlant) const;
    int RunPlantBuff2Count(SeedType thePlant) const;
    float RunPlantBuff2MulKind(SeedType thePlant, RunPlantUpgradeKind theKind) const;
    int RunPlantBuff2CountKind(SeedType thePlant, RunPlantUpgradeKind theKind) const;
    float RunCoffeeBeanRefreshMul(SeedType thePlant, SeedType theImitaterType) const;
    inline bool IsRunMode() const { return mRunState != nullptr; }
    inline RunState* GetRunState() const { return mRunState.get(); }
	std::unique_ptr<TitleScreen>	mTitleScreen;
	std::unique_ptr<GameSelector>	mGameSelector;
	std::unique_ptr<SeedChooserScreen>	mSeedChooserScreen;
	std::unique_ptr<AwardScreen>	mAwardScreen;
	std::unique_ptr<CreditScreen>	mCreditScreen;
	std::unique_ptr<ChallengeScreen>	mChallengeScreen;
	std::unique_ptr<PvzpFoley>		mSoundSystem;
	std::string						mReferId;
	std::string						mRegisterLink;
	std::string						mMod;
	bool							mRegisterResourcesLoaded;
	bool							mCheatKeys;
	GameMode						mGameMode;
	GameScenes						mGameScene;
	bool							mLoadingZombiesThreadCompleted;
	bool							mFirstTimeGameSelector;
	int								mGamesPlayed;
	int								mMaxExecutions;
	int								mMaxPlays;
	int								mMaxTime;
	bool							mEasyPlantingCheat;
	std::unique_ptr<PoolEffect>		mPoolEffect;
	std::unique_ptr<ZenGarden>		mZenGarden;
	std::unique_ptr<EffectSystem>	mEffectSystem;
	std::unique_ptr<ReanimatorCache>	mReanimatorCache;
	std::unique_ptr<ProfileMgr>		mProfileMgr;
	PlayerInfo*						mPlayerInfo;
	std::unique_ptr<LevelStats>		mLastLevelStats;
	std::atomic<bool>					mCloseRequest;
	uint32_t						mAppCounter;
	std::unique_ptr<Music>			mMusic;
	ReanimationID					mCrazyDaveReanimID;
	CrazyDaveState					mCrazyDaveState;
	int								mCrazyDaveBlinkCounter;
	ReanimationID					mCrazyDaveBlinkReanimID;
	int								mCrazyDaveMessageIndex;
	std::string						mCrazyDaveMessageText;
	int								mAppRandSeed;
	intptr_t						mSessionID;
	int								mPlayTimeActiveSession;
	int								mPlayTimeInactiveSession;
	BoardResult						mBoardResult;
	bool							mSawYeti;
	std::unique_ptr<TypingCheck>	mKonamiCheck;
	std::unique_ptr<TypingCheck>	mMustacheCheck;
	std::unique_ptr<TypingCheck>	mMoustacheCheck;
	std::unique_ptr<TypingCheck>	mSuperMowerCheck;
	std::unique_ptr<TypingCheck>	mSuperMowerCheck2;
	std::unique_ptr<TypingCheck>	mFutureCheck;
	std::unique_ptr<TypingCheck>	mPinataCheck;
	std::unique_ptr<TypingCheck>	mDanceCheck;
	std::unique_ptr<TypingCheck>	mDaisyCheck;
	std::unique_ptr<TypingCheck>	mSukhbirCheck;
	bool							mMustacheMode;
	bool							mSuperMowerMode;
	bool							mFutureMode;
	bool							mPinataMode;
	bool							mDanceMode;
	bool							mDaisyMode;
	bool							mSukhbirMode;
	TrialType						mTrialType;
	bool							mDebugTrialLocked;
	bool							mMuteSoundsForCutscene;

public:
    TeamProgressionView::Controller* mTeamProgressionController=nullptr;
    TeamProgressionDialog* mTeamProgressionDialog=nullptr;
    std::shared_ptr<TeamProgressionUiState> mTeamProgressionUi;
    bool mTeamProgressionInputFence=false;
    void SetTeamProgressionController(TeamProgressionView::Controller* controller);
    void ShowTeamProgression(TeamProgressionView::Page page);
    bool TeamProgressionAvailable() const;
    bool TeamProgressionInputBlocked() const;
    void UpdateTeamProgressionUi();
    void CloseTeamProgressionUi(bool restoreFocus=true);
    void ShowCoopPlayLegacy();
	LawnApp();
	~LawnApp() override;

	bool							KillNewOptionsDialog();
	void							GotFocus() override;
	void							LostFocus() override;
	void							InitHook() override;
	void							WriteToRegistry() override;
	void							ReadFromRegistry() override;
	void							LoadingThreadProc() override;
	virtual void					LoadingCompleted();
	void							LoadingThreadCompleted() override;
	void							URLOpenFailed(const std::string& theURL) override;
	void							URLOpenSucceeded(const std::string& theURL) override;
	bool							OpenURL(const std::string& theURL, bool shutdownOnOpen) override;
	bool							DebugKeyDown(int theKey) override;
	void							HandleCmdLineParam(std::string_view theParamName, std::string_view theParamValue) override;
	void							ConfirmQuit();
	void							ConfirmCheckForUpdates() { ; }
	void							CheckForUpdates() { ; }
	void							DoUserDialog();
	void							FinishUserDialog(bool isYes);
	void							DoCreateUserDialog();
	void							DoCheatDialog();
	void							FinishCheatDialog(bool isYes);
	void							FinishCreateUserDialog(bool isYes);
	std::string						GetFormattedString(std::string_view theComponentId, std::string_view theDefault, ...);
	void							DoConfirmDeleteUserDialog(const std::string& theName);
	void							FinishConfirmDeleteUserDialog(bool isYes);
	void							DoRenameUserDialog(const std::string& theName);
	void							FinishRenameUserDialog(bool isYes);
	void							FinishNameError(int theId);
	void							FinishRestartConfirmDialog();
	void							DoConfirmSellDialog(const std::string& theMessage);
	void							FinishTimesUpDialog();
	void							KillBoard();
	void							MakeNewBoard();
	void							StartPlaying();
	bool							TryLoadGame();
	void							NewGame();
	void							PreNewGame(GameMode theGameMode, bool theLookForSavedGame);
	void							ShowGameSelector();
	void							KillGameSelector();
	void							ShowAwardScreen(AwardType theAwardType, bool theShowAchievements);
	void							KillAwardScreen();
	void							ShowSeedChooserScreen();
	void							KillSeedChooserScreen();
	void							DoHighScoreDialog();
	void							DoBackToMain(bool notifyOnline = true);
	void							DoConfirmBackToMain();
	void							DoNewOptions(bool theFromGameSelector);
	void							ShowZombatarTOS();
	void							DoRegister();
	void							DoRegisterError();
	bool							CanDoRegisterDialog();
	bool					WriteCurrentUserConfig();
	void							DoNeedRegisterDialog();
	void							DoContinueDialog();
	void							DoPauseDialog();
	void							FinishModelessDialogs();
	Dialog*							DoDialog(int theDialogId, bool isModal, const std::string& theDialogHeader, const std::string& theDialogLines, const std::string& theDialogFooter, int theButtonMode) override;
	virtual Dialog*					DoDialogDelay(int theDialogId, bool isModal, const std::string& theDialogHeader, const std::string& theDialogLines, const std::string& theDialogFooter, int theButtonMode);
	void							Shutdown() override;
	void							ShutdownHook() override;
	void							Init() override;
	void							Start() override;
	Dialog*							NewDialog(int theDialogId, bool isModal, const std::string& theDialogHeader, const std::string& theDialogLines, const std::string& theDialogFooter, int theButtonMode) override;
	bool							KillDialog(int theDialogId) override;
	void							ModalOpen() override;
	void							ModalClose() override;
	void							PreDisplayHook() override;
	bool							ChangeDirHook(const char* theIntendedPath) override;
	virtual bool					NeedRegister();
	virtual void					UpdateRegisterInfo();
	void							ButtonPress(int theId) override;
	void							ButtonDepress(int theId) override;
	void							ButtonDownTick(int theId) override;
	void							ButtonMouseEnter(int theId) override;
	void							ButtonMouseLeave(int theId) override;
	void							ButtonMouseMove(int theId, int theX, int theY) override;
	void							UpdateFrames() override;
	bool							UpdateAppStep(bool* updated) override;
	bool							UpdateApp() override;
	bool					IsAdventureMode();
	bool					IsSurvivalMode();
	bool							IsContinuousChallenge();
	bool					IsArtChallenge();
	bool							NeedPauseGame();
	virtual void					ShowResourceError(bool doExit = false);
	void							ToggleSlowMo();
	void							ToggleFastMo();
	void							PlayFoley(FoleyType theFoleyType);
	void							PlayFoleyPitch(FoleyType theFoleyType, float thePitch);
	void							PlaySample(intptr_t theSoundNum) override;
	void							FastLoad(GameMode theGameMode);
	static std::string				GetStageString(int theLevel);
	void					KillChallengeScreen();
	void							ShowChallengeScreen(ChallengePage thePage);
	const ChallengeDefinition&			GetCurrentChallengeDef();
	void							CheckForGameEnd();
	void							CloseRequestAsync() override;
	bool					IsChallengeWithoutSeedBank();
	AlmanacDialog*					DoAlmanacDialog(SeedType theSeedType = SeedType::SEED_NONE, ZombieType theZombieType = ZombieType::ZOMBIE_INVALID);
	bool							KillAlmanacDialog();
	int								GetSeedsAvailable();
	Reanimation*					AddReanimation(float theX, float theY, int theRenderOrder, ReanimationType theReanimationType);
	PvzpParticleSystem*				AddPvzpParticle(float theX, float theY, int theRenderOrder, ParticleEffect theEffect);
	ParticleSystemID		ParticleGetID(PvzpParticleSystem* theParticle);
	PvzpParticleSystem*	ParticleGet(ParticleSystemID theParticleID);
	PvzpParticleSystem*	ParticleTryToGet(ParticleSystemID theParticleID);
	ReanimationID		ReanimationGetID(Reanimation* theReanimation);
	Reanimation*			ReanimationGet(ReanimationID theReanimationID);
	Reanimation*			ReanimationTryToGet(ReanimationID theReanimationID);
	void							RemoveReanimation(ReanimationID theReanimationID);
	void							RemoveParticle(ParticleSystemID theParticleID);
	StoreScreen*					ShowStoreScreen();
	void							KillStoreScreen();
	bool							HasSeedType(SeedType theSeedType);
	void					EndLevel();
	inline bool						IsIceDemo() { return false; }
	bool					IsShovelLevel();
	bool					IsWallnutBowlingLevel();
	bool					IsMiniBossLevel();
	bool					IsSlotMachineLevel();
	bool					IsLittleTroubleLevel();
	bool					IsStormyNightLevel();
	bool					IsFinalBossLevel();
	bool					IsBungeeBlitzLevel();
	static SeedType		GetAwardSeedForLevel(int theLevel);
	std::string						GetCrazyDaveText(int theMessageIndex);
	bool					CanShowAlmanac();
	bool					IsNight();
	bool					CanShowStore();
	bool					HasBeatenChallenge(GameMode theGameMode);
	PottedPlant*					GetPottedPlantByIndex(int thePottedPlantIndex);
	static bool			IsSurvivalNormal(GameMode theGameMode);
	static bool			IsSurvivalHard(GameMode theGameMode);
	static bool			IsSurvivalEndless(GameMode theGameMode);
	bool					HasFinishedAdventure();
	bool					IsFirstTimeAdventureMode();
	bool					CanSpawnYetis();
	void							CrazyDaveEnter();
	void							UpdateCrazyDave();
	void							CrazyDaveTalkIndex(int theMessageIndex);
	void							CrazyDaveTalkMessage(const std::string& theMessage);
	void							CrazyDaveLeave();
	void							DrawCrazyDave(Graphics* g);
	void							CrazyDaveDie();
	void							CrazyDaveStopTalking();
	void							PreloadForUser();
	int								GetNumPreloadingTasks();
	int								LawnMessageBox(int theDialogId, const char* theHeaderName, const char* theLinesName, const char* theButton1Name, const char* theButton2Name, int theButtonMode);
	void							ShowCreditScreen();
	void							KillCreditScreen();
	static std::string				Pluralize(int theCount, const char* theSingular, const char* thePlural);
	int								GetNumTrophies(ChallengePage thePage);
	bool					EarnedGoldTrophy();
	inline bool						IsRegistered() { return false; }
	inline bool						IsExpired() { return false; }
	inline bool						IsDRMConnected() { return false; }
	bool					IsScaryPotterLevel();
	static bool			IsEndlessScaryPotter(GameMode theGameMode);
	bool					IsSquirrelLevel();
	bool					IsIZombieLevel();
	bool					CanShowZenGarden();
	static std::string				GetMoneyString(int theAmount);
	bool							AdvanceCrazyDaveText();
	bool					IsWhackAZombieLevel();
	void							UpdatePlayTimeStats();
	void							BetaAddFile(std::list<std::string>& theUploadFileList, std::string theFileName, std::string theShortName);
	bool							CanPauseNow();
	bool					IsPuzzleMode();
	bool					IsChallengeMode();
	static bool			IsEndlessIZombie(GameMode theGameMode);
	void							CrazyDaveDoneHanding();
	inline std::string				GetCurrentLevelName() { return "Unknown"; }
	int					TrophiesNeedForGoldSunflower();
	int					GetCurrentChallengeIndex();
	void							LoadGroup(const char* theGroupName, int theGroupAveMsToLoad);
	void							CrazyDaveStopSound();
	bool					IsTrialStageLocked();
	void					FinishZenGardenToturial();
	bool							UpdatePlayerProfileForFinishingLevel();
	bool							SaveFileExists();
	bool					CanDoPinataMode();
	bool					CanDoDanceMode();
	bool					CanDoDaisyMode();
	void							SwitchScreenMode(bool wantWindowed, bool is3d, bool force = false) override;
	static void			CenterDialog(Dialog* theDialog, int theWidth, int theHeight);
};

std::string							LawnGetCurrentLevelName();
bool								LawnGetCloseRequest();
bool								LawnHasUsedCheatKeys();
void								BetaSubmitFunc();

extern bool gIsPartnerBuild;
extern bool gFastMo;
extern bool gSlowMo;
extern LawnApp* gLawnApp;
extern int gSlowMoCounter;


#endif	// __LAWNAPP_H__
