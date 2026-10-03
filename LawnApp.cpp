#include <corecrt.h>
#include <time.h>
#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/MessageWidget.h"
#include "Lawn/Plant.h"
#include "Lawn/Zombie.h"
#include "Lawn/Cutscene.h"
#include "GameConstants.h"
#include "Lawn/Challenge.h"
#include "Lawn/ZenGarden.h"
#include "Sexy.TodLib/Trail.h"
#include "Lawn/System/Music.h"
#include "Lawn/System/SaveGame.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/Attachment.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/PoolEffect.h"
#include "Lawn/System/ProfileMgr.h"
#include "Lawn/System/PopDRMComm.h"
#include "Lawn/Widget/GameButton.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Lawn/Widget/UserDialog.h"
#include "Lawn/System/TypingCheck.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Lawn/Widget/AwardScreen.h"
#include "Lawn/Widget/TitleScreen.h"
#include "Lawn/Widget/StoreScreen.h"
#include "Lawn/Widget/CheatDialog.h"
#include "Lawn/Widget/OnlineDialog.h"
#include "Lawn/Widget/OnlineStartDialog.h"
#include "Lawn/Widget/RunModeDialog.h"
#include "Lawn/Online/NetSession.h"
#include "Lawn/Run/RunState.h"
#include "Lawn/Run/RunBuffs.h"
#include "Lawn/Widget/RunPickDialog.h"
#include "Lawn/Widget/GameSelector.h"
#include "Lawn/Widget/CreditScreen.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/FilterEffect.h"
#include "graphics/Graphics.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "Lawn/Widget/Almanac.h"
#include "Lawn/Widget/NewUserDialog.h"
#include "Lawn/Widget/ContinueDialog.h"
#include "Lawn/System/ReanimationLawn.h"
#include "Lawn/Widget/ChallengeScreen.h"
#include "Lawn/Widget/NewOptionsDialog.h"
#include "Lawn/Widget/SeedChooserScreen.h"
#include "widget/WidgetManager.h"
#include "misc/ResourceManager.h"

#include "widget/Checkbox.h"
#include "sound/BassMusicInterface.h"
#include "widget/Dialog.h"
#include "SexyAppFramework/resource.h"

bool gIsPartnerBuild = false; // GOTY @Patoke: 0x729659
bool gSlowMo = false;  //0x6A9EAA
bool gFastMo = false;  //0x6A9EAB
LawnApp* gLawnApp = nullptr;  //0x6A9EC0
int gSlowMoCounter = 0;  //0x6A9EC4

//0x44E8A0
bool LawnGetCloseRequest()
{
	if (gLawnApp == nullptr)
		return false;

	return gLawnApp->mCloseRequest;
}

//0x44E8C0
bool LawnHasUsedCheatKeys()
{
	return gLawnApp && gLawnApp->mPlayerInfo && gLawnApp->mPlayerInfo->mHasUsedCheatKeys;
}

//0x44EAA0
// GOTY @Patoke: 0x451D70
LawnApp::LawnApp()
{
	mBoard = nullptr;
	mGameSelector = nullptr;
	mChallengeScreen = nullptr;
	mSeedChooserScreen = nullptr;
	mAwardScreen = nullptr;
	mCreditScreen = nullptr;
	mTitleScreen = nullptr;
	mSoundSystem = nullptr;
	mKonamiCheck = nullptr;
	mMustacheCheck = nullptr;
	mMoustacheCheck = nullptr;
	mSuperMowerCheck = nullptr;
	mSuperMowerCheck2 = nullptr;
	mFutureCheck = nullptr;
	mPinataCheck = nullptr;
	mDanceCheck = nullptr;
	mDaisyCheck = nullptr;
	mSukhbirCheck = nullptr;
	mMustacheMode = false;
	mSuperMowerMode = false;
	mFutureMode = false;
	mPinataMode = false;
	mDanceMode = false;
	mDaisyMode = false;
	mSukhbirMode = false;
	mGameScene = GameScenes::SCENE_LOADING;
	mPoolEffect = nullptr;
	mZenGarden = nullptr;
	mEffectSystem = nullptr;
	mReanimatorCache = nullptr;
	mCloseRequest = false;
	mWidth = BOARD_WIDTH;
	mHeight = BOARD_HEIGHT;
	mFullscreenBits = 32;
	mAppCounter = 0;
	mAppRandSeed = _time64(nullptr);
	mTrialType = TrialType::TRIALTYPE_NONE;
	mDebugTrialLocked = false;
	mMuteSoundsForCutscene = false;
	mMusicVolume = 0.85;
	mSfxVolume = 0.5525;
	mAutoStartLoadingThread = false;
	mDebugKeysEnabled = false;
	mProdName = "PopCap\\PlantsVsZombies";
	std::string aTitleName = "Plants vs. Zombies";
#ifdef _DEBUG
	aTitleName += " BETA ";
	aTitleName += mProductVersion;
#endif
	mTitle = StringToSexyStringFast(aTitleName);
	mCustomCursorsEnabled = false;
	mPlayerInfo = nullptr;
	mLastLevelStats = new LevelStats();
	mFirstTimeGameSelector = true;
	mGameMode = GameMode::GAMEMODE_ADVENTURE;
	mEasyPlantingCheat = false;
	mAutoEnable3D = true;
	Tod_SWTri_AddAllDrawTriFuncs();
	mLoadingZombiesThreadCompleted = true;
	mGamesPlayed = 0;
	mMaxExecutions = 0;
	mMaxPlays = 0;
	mMaxTime = 0;
	mCompletedLoadingThreadTasks = 0;
	mProfileMgr = new ProfileMgr();
	mRegisterResourcesLoaded = false;
	mTodCheatKeys = false;
	mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
	mCrazyDaveBlinkCounter = 0;
	mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveMessageIndex = -1;
	mBigArrowCursor = LoadCursor(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_CURSOR1));
	mDRM = nullptr;
	mOnlineSession = nullptr;
	mHasOnlineStart = false;
	mOnlineStartLevel = 0;
	mOnlineStartSeed = 0;
	mOnlineWasConnected = false;
	mOnlineWaitingStartAck = false;
	mOnlineStartWaitFrames = 0;
	mPauseMenuWasOpen = false;
	mPauseMenuFromPeer = false;
	mOnlineWaitingAdviceOn = false;
	mRunState = nullptr;
	mPendingAdventure = false;
	mShowedStartupAnnounce = false;
	mOnlineRunStartHeld = false;
	mOnlineRunGo = false;
	mRunIntroHeld = false;
	mOnlineStartPromptActive = false;
	mOnlineStartPromptFrames = 0;
	mOnlineSeedsHeld = false;
}

//0x44EDD0、0x44EDF0
LawnApp::~LawnApp()
{
	if (mBoard)
	{
		WriteCurrentUserConfig();
	}

	if (mBoard)
	{
		mBoardResult = BoardResult::BOARDRESULT_QUIT_APP;
		mBoard->TryToSaveGame();
		mWidgetManager->RemoveWidget(mBoard);
		delete mBoard;
		mBoard = nullptr;
	}

	if (mTitleScreen)
	{
		mWidgetManager->RemoveWidget(mTitleScreen);
		delete mTitleScreen;
	}

	delete mSoundSystem;
	delete mMusic;

	// @pvz-online: 联机会话先于其它 UI 收掉——它会等收包线程退出（最多 2 秒）
	delete mOnlineSession;
	mOnlineSession = nullptr;

	// 闯关状态只是内存里这一局的账本，检查点已经在盘上（每关开打时写），直接丢。
	delete mRunState;
	mRunState = nullptr;

	if (mKonamiCheck)
	{
		delete mKonamiCheck;
	}
	if (mMustacheCheck)
	{
		delete mMustacheCheck;
	}
	if (mMoustacheCheck)
	{
		delete mMoustacheCheck;
	}
	if (mSuperMowerCheck)
	{
		delete mSuperMowerCheck;
	}
	if (mSuperMowerCheck2)
	{
		delete mSuperMowerCheck2;
	}
	if (mFutureCheck)
	{
		delete mFutureCheck;
	}
	if (mPinataCheck)
	{
		delete mPinataCheck;
	}
	if (mDanceCheck)
	{
		delete mDanceCheck;
	}
	if (mDaisyCheck)
	{
		delete mDaisyCheck;
	}
	if (mSukhbirCheck)
	{
		delete mSukhbirCheck;
	}

	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		delete mGameSelector;
	}
	if (mChallengeScreen)
	{
		mWidgetManager->RemoveWidget(mChallengeScreen);
		delete mChallengeScreen;
	}
	if (mSeedChooserScreen)
	{
		mWidgetManager->RemoveWidget(mSeedChooserScreen);
		delete mSeedChooserScreen;
	}
	if (mAwardScreen)
	{
		mWidgetManager->RemoveWidget(mAwardScreen);
		delete mAwardScreen;
	}
	if (mCreditScreen)
	{
		mWidgetManager->RemoveWidget(mCreditScreen);
		delete mCreditScreen;
	}

	delete mProfileMgr;
	delete mLastLevelStats;

	mResourceManager->DeleteResources("");
	/*
#ifdef _DEBUG
	BetaSubmit(true);
#endif
	*/
}

//0x44F200
void LawnApp::Shutdown()
{
	if (!mLoadingThreadCompleted)
	{
		mLoadingFailed = true;
		return;
	}

	if (!mShutdown)
	{
		for (int i = 0; i < Dialogs::NUM_DIALOGS; i++)
		{
			KillDialog(i);
		}

		if (mBoard)
		{
			mBoardResult = BoardResult::BOARDRESULT_QUIT_APP;
			mBoard->TryToSaveGame();
			KillBoard();
			WriteCurrentUserConfig();
		}

		ProcessSafeDeleteList();

		if (mPoolEffect)
		{
			mPoolEffect->PoolEffectDispose();
			delete mPoolEffect;
			mPoolEffect = nullptr;
		}

		if (mZenGarden)
		{
			delete mZenGarden;
			mZenGarden = nullptr;
		}

		if (mEffectSystem)
		{
			mEffectSystem->EffectSystemDispose();
			delete mEffectSystem;
			mEffectSystem = nullptr;
		}

		if (mReanimatorCache)
		{
			mReanimatorCache->ReanimatorCacheDispose();
			delete mReanimatorCache;
			mReanimatorCache = nullptr;
		}

		FilterEffectDisposeForApp();
		TodParticleFreeDefinitions();
		ReanimatorFreeDefinitions();
		TrailFreeDefinitions();
		FreeGlobalAllocators();
		UpdateRegisterInfo();
		SexyAppBase::Shutdown();

		if (mDRM)
		{
			delete mDRM;
		}
		mDRM = nullptr;
	}
}

//0x44F380
// GOTY @Patoke : 0x452640
void LawnApp::KillBoard()
{
	FinishModelessDialogs();
	KillSeedChooserScreen();
	if (mBoard)
	{
/*
#ifdef _DEBUG
		BetaRecordLevelStats();
#endif
*/
		mBoard->DisposeBoard();
		mWidgetManager->RemoveWidget(mBoard);
		SafeDeleteWidget(mBoard);
		mBoard = nullptr;
	}

	SetCursor(CURSOR_POINTER);
}

//0x44F410
bool LawnApp::CanPauseNow()
{
	if (mBoard == nullptr)  // 不在关卡内
		return false;

	if (mSeedChooserScreen && mSeedChooserScreen->mMouseVisible)  // 处于选卡界面
		return false;

	if (mBoard->mBoardFadeOutCounter >= 0)  // 退出关卡过程中
		return false;

	if (mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF)  // 存在戴夫
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)  // 处于禅境花园或智慧树
		return false;

	return GetDialogCount() <= 0;  // 不存在对话
}

void LawnApp::GotFocus()
{
}

//0x44F460
void LawnApp::LostFocus()
{
	// @pvz-online: 联机环境里切出去一律不暂停——建房等待、连接中、已连上都算（用户拍板：
	// 这个简化框开的 DIALOG_PAUSED 不参与暂停同步，一弹就是"只停自己"：队友还在打，
	// 僵尸照样往这边漏；哪怕房还空着，跑局中途也可能有人连进来）。要暂停请按 ESC 走
	// 同步的那条路。只有完全没碰过联机面板的单机局保留原版行为。
	if (mOnlineSession != nullptr && mOnlineSession->IsActive()) return;

	if (!mTodCheatKeys && CanPauseNow())
	{
		DoPauseDialog();
	}
}

//0x44F480
void LawnApp::WriteToRegistry()
{
	if (mPlayerInfo)
	{
		RegistryWriteString("CurUser", SexyStringToStringFast(mPlayerInfo->mName));
		mPlayerInfo->SaveDetails();
	}

	SexyAppBase::WriteToRegistry();
}

//0x44F530
void LawnApp::ReadFromRegistry()
{
	SexyApp::ReadFromRegistry();
}

//0x44F540
// GOTY @Patoke: 0x452800
bool LawnApp::WriteCurrentUserConfig()
{
	if (mPlayerInfo)
		mPlayerInfo->SaveDetails();

	return true;
}

// @pvz-online: M1 裁剪下架的玩法范围（见 PreNewGame）。GAMEMODE_CHALLENGE_ICE 与
// GAMEMODE_CHALLENGE_ZEN_GARDEN 虽然落在挑战区间内，但前者是 Ice Demo 的流程、
// 后者是冒险 45 关的奖励教学，都保留可达，故显式排除
static bool IsPvzModeCulled(GameMode theGameMode)
{
	if (theGameMode >= GAMEMODE_SURVIVAL_NORMAL_STAGE_1 && theGameMode <= GAMEMODE_SURVIVAL_HARD_STAGE_5)
		return true;
	if (theGameMode >= GAMEMODE_CHALLENGE_WAR_AND_PEAS && theGameMode <= GAMEMODE_CHALLENGE_SQUIRREL
		&& theGameMode != GAMEMODE_CHALLENGE_ICE && theGameMode != GAMEMODE_CHALLENGE_ZEN_GARDEN)
		return true;
	if (theGameMode >= GAMEMODE_SCARY_POTTER_1 && theGameMode <= GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS)
		return true;
	return false;
}

//0x44F560
// GOTY @Patoke: 0x452820
void LawnApp::PreNewGame(GameMode theGameMode, bool theLookForSavedGame)
{
	//if (NeedRegister())
	//{
	//	ShowGameSelector();
	//	return;
	//}

	// @pvz-online: M1 模式裁剪——拦住已下架的玩法，作为入口隐藏之外的第二道防线
	//（正常流程走不到这里）。用"黑名单"而不是严格白名单，是为了不打断仍然保留的内部流：
	//冒险 45 关的禅境花园教学（AwardScreen 的通关奖励分支 → CHALLENGE_ZEN_GARDEN）、
	//商店 / 智慧树的 TREE_OF_WISDOM 内部跳转，以及 Ice Demo 的 CHALLENGE_ICE。
	if (IsPvzModeCulled(theGameMode))
	{
		TodTrace("PreNewGame: mode %d is culled in M1, ignored", (int)theGameMode);
		return;
	}

	mGameMode = theGameMode;

	// @pvz-online: 会话开着的时候一律按联机规矩来：
	//   能开局（已连上的主机）→ 不读档也不删档，把这一局的关卡和波表种子广播出去，
	//     然后等队友回 START_ACK 再进场（见 UpdateOnlineStart）；
	//   不能开局（没连上 / 客户端）→ 什么都不做。
	// 客户端的关卡是主机说了算，所以客户端在这条路上永远不会自己开出一局来。
	// 掉线的会话按"没有会话"算：这时候点关卡该开一局单机，而不是被一句
	// "联机局不能开局"永远挡在门外（那正是"只能断开连接才恢复"的老毛病）。
	if (mOnlineSession && mOnlineSession->IsActive()
		&& mOnlineSession->GetState() != NetSession::State::DEAD)
	{
		// 上一次开局还在等队友回应：这次的请求按下不表（不然等待期间每触发一次就重发一条
		// START_LEVEL，对面收到一串）。等出头了再说。
		if (mOnlineWaitingStartAck)
		{
			TodTrace("PreNewGame: still waiting for the teammate's answer, ignored");
			return;
		}

		// 棋盘还在就来开新局（暂停菜单里的 Restart Level）：联机不给重开——两边棋盘各跑各的，
		// 重开必然对不上。C 里把菜单上那颗按钮收掉，这儿是兜底。
		if (mBoard != nullptr)
		{
			TodTrace("PreNewGame: online game, restarting a level is not allowed, ignored");
			return;
		}

		if (!IsOnlineStartAllowed())
		{
			// 开不了（没连上 / 客户端）。要是主菜单已经被拆掉（点了开局又赶上队友掉线），
			// 就地补一个回来：拆了菜单又没有棋盘，屏幕上就什么都不剩了。
			if (mGameScene == GameScenes::SCENE_MENU && mGameSelector == nullptr)
			{
				ShowGameSelector();
			}
			TodTrace("PreNewGame: online session cannot start right now, ignored");
			return;
		}

		// 房里就我一个（队友还没来）：没有要通知的人，也就没有 ACK 可等——直接进场。
		// 摆"本机的数要和广播出去的一致"那套覆盖值也免了：没广播，就无所谓一致不一致。
		// 队友之后中途进来会被主机当场拉进这一关（见 UpdateOnlineEvents）。
		if (!WillWaitForStartAck())
		{
			TodLog("[net] no one else in the room - entering the level right away");
			KillDialog(Dialogs::DIALOG_ONLINE);
			KillGameSelector();
			NewGame();
			return;
		}

		// 主场先广播、后进场：队友没收到命令的话，主机一个人开着关跑下去是最糟的结局。
		// 广播要带关卡和种子，可这时候棋盘还没建（要等 ACK 才建）——所以按 Board 的算式
		// 在这儿算一份，设成覆盖值；等 ACK 到了 NewGame()，InitLevel 和 GetLevelRandSeed
		// 读的就是同一份覆盖值，和广播出去的完全一致。
		//
		// 生存模式的棋盘随机数取的是 Rand()（见 Board 构造函数），那种模式下这套算法对不上；
		// 好在生存已经整段裁剪（IsPvzModeCulled），联机局开不出生存来。
		int aLevel = IsAdventureMode() ? mPlayerInfo->mLevel : 0;
		int aSeed = Board::ComputeLevelRandSeed(mAppRandSeed, IsAdventureMode(), mPlayerInfo->mId,
			mPlayerInfo->mFinishedAdventure, aLevel, 0, (int)mGameMode);

		SetOnlineStartOverride(aLevel, aSeed);
		mOnlineWaitingStartAck = true;
		mOnlineStartWaitFrames = 0;
		TodLog("[net] host picked level %d, waiting for the teammate before entering", aLevel);
		mOnlineSession->SendStartLevel((uint8_t)mGameMode, (uint32_t)aLevel, aSeed);
		return;
	}

	if (theLookForSavedGame && TryLoadGame())
		return;

	std::string aFileName = GetSavedGameName(mGameMode, mPlayerInfo->mId);
	EraseFile(aFileName);
	NewGame();
}

// @pvz-online: 联机三连问。没有会话 = 单机，一律走原版行为，联机代码不参与。
bool LawnApp::IsOnlineGame()
{
	return mOnlineSession != nullptr && mOnlineSession->IsConnected();
}

// 谁能开局：单机随便；联机局里只有已经连上的主机能定关卡。
// 没连上（面板上正写着"等人加入 / 正在连"）和客户端都返回 false。
// 例外：掉线（DEAD）的会话已经不联机了，这时候一律放行——让人能接着开单机局。
bool LawnApp::IsOnlineStartAllowed()
{
	if (!mOnlineSession || !mOnlineSession->IsActive()) return true;
	if (mOnlineSession->GetState() == NetSession::State::DEAD) return true;

	return mOnlineSession->IsConnected() && mOnlineSession->GetRole() == NetSession::Role::HOST;
}

// @pvz-online: 我这台是"跟着主机走"的那一头吗（客户端）。全队败之后开不开新局是主机一个人
// 的决定——两边必须进同一关、同一张波表，谁先动手谁就把对面带沟里了——所以要分得出主客：
// 客户端那台的 GAME OVER 框里没有 Try Again，只有一句等主机的话（见 GameOverDialog）。
bool LawnApp::IsOnlineClient()
{
	return mOnlineSession != nullptr && mOnlineSession->IsConnected()
		&& mOnlineSession->GetRole() == NetSession::Role::CLIENT;
}

// 现在点开局会不会走"广播命令、等队友 START_ACK"这条路。会的话主菜单先留着：
// 等待的那几秒里，屏幕上至少得是个能看、等不到还能重试的菜单，而不是一片黑。
// 房里只有我一个（联机房间刚建、队友还没进来）时不算：没有要通知的人，也就没有 ACK 可等，
// 等下去是白等——闯关局的等待本来就不设超时（见 ONLINE_START_WAIT_TIMEOUT_FRAMES 那段），
// 一个人开的房点开局会永久挂起。这时候直接开局，队友中途进来会被当场拉进这一关。
bool LawnApp::WillWaitForStartAck()
{
	return mOnlineSession != nullptr
		&& mOnlineSession->IsConnected()
		&& mOnlineSession->GetRole() == NetSession::Role::HOST
		&& mOnlineSession->HasOtherSeats();
}

void LawnApp::SetOnlineStartOverride(int theLevel, int theSeed)
{
	mHasOnlineStart = true;
	mOnlineStartLevel = theLevel;
	mOnlineStartSeed = theSeed;
}

bool LawnApp::GetOnlineStartOverride(int& theLevel, int& theSeed)
{
	if (!mHasOnlineStart) return false;

	theLevel = mOnlineStartLevel;
	theSeed = mOnlineStartSeed;
	return true;
}

void LawnApp::ClearOnlineStartOverride()
{
	mHasOnlineStart = false;
}

// @pvz-online: 两边进场都从这儿走。每条开局命令只能用一次，用过就清；
// 清掉之后 GetLevelRandSeed 又回到本机自己的算法。
// 主机广播完开局命令之后，队友回 START_ACK 需要的时间上限（600 帧 ≈ 6 秒）。
// 超过就当这次开局作废——人留在主菜单上，比盯着一个点不动的界面强。
// 联机闯关（R5）不用这个表：队友可能正在补发追赶（一屏一屏地选，几十屏都可能），
// 拿"6 秒"去卡它等于把正常的队友判成卡死——闯关的换关等待只认掉线那一条出路。
static const int ONLINE_START_WAIT_TIMEOUT_FRAMES = 600;

void LawnApp::UpdateOnlineStart()
{
	if (!mOnlineSession) return;

	// 主机侧：开局命令已经广播出去了，等队友的 START_ACK。等待期间主菜单留着
	// （GameSelector::Update 会保住它），所以每条出路都得让菜单重新可用：
	// 要么进场，要么把这次开局作废、人还好好待着。
	if (mOnlineWaitingStartAck)
	{
		mOnlineStartWaitFrames++;

		// ① 队友没了：ACK 和掉线挤在同一帧的话，宁可这一次开局作废，
		// 也不能一个人开着关跑下去。
		if (!mOnlineSession->IsConnected())
		{
			mOnlineWaitingStartAck = false;
			KillDialog(Dialogs::DIALOG_ONLINE_START);
			ClearOnlineStartOverride();
			ShowGameSelector();
			TodTrace("online start: teammate left before entering, start dropped");
			return;
		}

		// @pvz-online: 菜单上的等待（起第一关）挂一块"等待其他玩家"的看板框；换关的等待
		// 棋盘还在（上一关的草坪当背景，见下面那句 advice），不叠框。每帧自检：框不在了
		// 就再摆一张——和 RunPickDialog 同一条纪律，被谁误关都不会卡住等待。
		if (mBoard == nullptr && GetDialog(Dialogs::DIALOG_ONLINE_START) == nullptr)
		{
			OnlineStartDialog* aWaitDialog = new OnlineStartDialog(this,
				"等待其他玩家", "邀请已发出，等待确认…", "取消", nullptr,
				OnlineStartDialog::NOTIFY_WAIT_CANCEL);
			CenterDialog(aWaitDialog, aWaitDialog->mWidth, aWaitDialog->mHeight);
			AddDialog(Dialogs::DIALOG_ONLINE_START, aWaitDialog);
		}

		// @pvz-online: 联机闯关（R5）的换关等待：上一关的棋盘留在屏幕上当背景（UpdateRunPick
		// 特意不拆），挂一句等待说明，别让队友看着一块不动的草坪以为卡住了。等待期间每帧
		// 看看说明还在不在、够不够长，自己给自己续期（清理由是换关成功后棋盘一换就没了）。
		if (IsRunMode() && mBoard != nullptr
			&& (!mBoard->mAdvice->IsBeingDisplayed() || mBoard->mAdvice->mDuration < 50))
		{
			mBoard->DisplayAdvice(_S("Waiting for your teammate..."),
				MessageStyle::MESSAGE_STYLE_BIG_MIDDLE, AdviceType::ADVICE_NONE);
		}

		// 收下这一轮的 ACK。记账在会话层按席位（四个人里谁答了、谁还没答，那儿看得清），
		// 这儿只是把它们取走——留着的话下一帧会当成"迟到的 ACK"。
		bool aDrain = false;
		while (mOnlineSession->TakeStartAck(aDrain)) { }

		// ② 有人回了"现在不行"（他人还在关卡里 / 在询问框上点了"暂不"）：这次开局不作数，
		// 说清楚为什么，菜单原样可用。这条记号是粘着的（取不走）——后到的好消息也翻不了案，
		// 本来就得全队都点头才能进。
		if (mOnlineSession->AnyStartAckRejected())
		{
			mOnlineWaitingStartAck = false;
			KillDialog(Dialogs::DIALOG_ONLINE_START);
			ClearOnlineStartOverride();
			ShowGameSelector();
			TodLog("online start: a teammate turned it down");
			OnlineStartDialog* aDialog = new OnlineStartDialog(this,
				"队友暂不加入", "他可能还在别的关卡里，可以稍后再试。", "知道了", nullptr,
				OnlineStartDialog::NOTIFY_NONE);
			CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
			AddDialog(Dialogs::DIALOG_ONLINE_START, aDialog);
			aDialog->WaitForResult();   // 阻塞框，泵主循环——联机心跳不受影响
			return;
		}

		// ③ 该答的都答了：人齐，进场。按**发命令那一刻上座的席位**记账——等待中有人走了，
		// 他那一格跟着作废、不再堵着；中途新进来的人没收到过这条命令，不算数。
		// （原来只认"收到的那一条 ACK"：两个人的时候就是对面，四个人时会一个人先进去。）
		if (mOnlineSession->AreAllStartAcksIn())
		{
			mOnlineWaitingStartAck = false;
			KillDialog(Dialogs::DIALOG_ONLINE_START);
			TodTrace("online start: the whole team is in, entering the level");
			KillDialog(Dialogs::DIALOG_ONLINE);
			KillGameSelector();
			NewGame();
			// @pvz-online: 联机闯关（R6）：全队都回话了才算人齐——广播放行，各席位
			// 在自己的新草坪上开始做三选一（自己那块草坪上面那句 NewGame 已经建好，
			// 欠下的选项屏由 UpdateRunPick 摆上去）。单关局没有选项要等，不发。
			if (IsRunMode()) mOnlineSession->SendRunGo();
			return;
		}

		// ④ 等太久了：队友可能卡住了、或者这条命令根本没送到。作废，菜单留给玩家重试。
		// 闯关局不走这条（见上面常量那两句注释），所以那一头的等待只认掉线。
		if (!IsRunMode() && mOnlineStartWaitFrames > ONLINE_START_WAIT_TIMEOUT_FRAMES)
		{
			mOnlineWaitingStartAck = false;
			KillDialog(Dialogs::DIALOG_ONLINE_START);
			ClearOnlineStartOverride();
			ShowGameSelector();
			TodLog("online start: no answer from the teammate in time, start dropped");
			OnlineStartDialog* aDialog = new OnlineStartDialog(this,
				"没有等到回应", "队友没有及时确认，可以再试一次。", "知道了", nullptr,
				OnlineStartDialog::NOTIFY_NONE);
			CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
			AddDialog(Dialogs::DIALOG_ONLINE_START, aDialog);
			aDialog->WaitForResult();
		}
		return;
	}

	// 不在等待时的迟到 ACK：没有哪个等待方认领它，清掉。留着的话下一次等待会被它污染——
	// 点名那一帧到下一帧之间只有"正在打"这一个状态，没人接的 ACK 会让主机以为队友
	// 已经就位，一个人开着关跑下去。
	bool aStaleAck = false;
	if (mOnlineSession->TakeStartAck(aStaleAck))
	{
		// @pvz-online: 闯关（R6）有一个不在等待态发出的点名：队友中途连进来、被主机
		// 当场拉进这一关（见 UpdateOnlineEvents）。他的 ACK 到这儿没人认领，但放行还得给——
		// 他正站在草坪上等 RUN_GO，不给就永远等下去。
		// M3 扩到四个席位：这里同样要按席位记账再决定放行（见 NetSession::SendRunGo）。
		if (aStaleAck && IsRunMode() && mBoard != nullptr
			&& mOnlineSession->GetRole() == NetSession::Role::HOST)
		{
			TodLog("[run] a pulled-in teammate answered - releasing their picks");
			mOnlineSession->SendRunGo();
		}
		else
		{
			TodLog("[net] dropped a start ack nobody was waiting for (accepted=%d)", (int)aStaleAck);
		}
	}

	// @pvz-online: 联机闯关（R6）客户端侧：主机放行了吗（RUN_GO）。只认"命令已经收下、
	// 正寄存着"的那一次；没人等着的放行丢掉（比如自己刚回绝了这次开局）。
	if (mOnlineSession->TakeRunGo())
	{
		if (mOnlineRunStartHeld)
		{
			TodLog("[run] the host says go - the new lawn is clear to pick");
			mOnlineRunGo = true;
		}
		else
		{
			TodLog("[run] dropped a run-go nobody was waiting for");
		}
	}

	// 客户端侧：主机开局了，跟着开自己那块棋盘。
	NetProto::MsgStartLevel aStart;

	// @pvz-online: 询问框挂着的这段时间也在计时：非闯关局给它和主机那把尺一样长的时间
	// （同一个常量），到点两边一起收——主机那边超时作废，这边把框撤掉顺手回一句"不去"。
	// 不撤的话，晚点的"确认"会让本机一个人进场（主机早就不等了），那块草坪谁也结束不了。
	// 闯关局和主机一样不设时限：那一头等多久都算正常（队友可能在补发追赶）。
	if (mOnlineStartPromptActive)
	{
		mOnlineStartPromptFrames++;
		if (!mOnlineStartPromptMsg.mIsRun && mOnlineStartPromptFrames > ONLINE_START_WAIT_TIMEOUT_FRAMES)
		{
			TodLog("[net] the start invite timed out unanswered, it is dropped");
			DismissOnlineStartPrompt(true);
		}
	}

	if (!mOnlineSession->TakePendingStartLevel(aStart)) return;

	// @pvz-online: 联机闯关（R5/R6）：先无条件对齐主机的进度，命令本身寄存住。ACK 从
	// "我选完了、进场了"改成"命令收到、我这就进场"（R6）：先让主机知道人齐、由主机广播
	// RUN_GO（见上面主机侧），自己这块新草坪建好、屏摆上，才在草坪上做欠下的三选一
	// （补发追赶一屏一屏地选，见 UpdateRunPick）。选项跟着 RUN_GO 走，不再盖在
	// 菜单 / 上一关的残局上。
	if (aStart.mIsRun)
	{
		if (mGameScene != GameScenes::SCENE_MENU && !IsRunMode())
		{
			// 人还在别的局里（既不在菜单、手里也没有闯关局）：这条命令接不了。回一句
			// "现在不行"，主机当场收摊，不至于干等。
			TodTrace("run start refused: scene %d, no run in progress", (int)mGameScene);
			mOnlineSession->SendStartAck(false);
			return;
		}

		// @pvz-online: 人停在主菜单上的（起第一关 / 中途被拉进来）先看一眼"是否加入"，
		// 命令寄存在询问框里，点了"加入"才对齐进度（见 OnlineStartPromptAnswer）。
		// 人已经在闯关里被叫去换关的不问：那是正常推进，问了反而把流程卡住。
		if (mGameScene == GameScenes::SCENE_MENU && mBoard == nullptr)
		{
			ShowOnlineStartPrompt(aStart);
			return;
		}

		TodLog("[run] the host calls us into level index %u (run seed %d)",
			(unsigned)aStart.mRunLevelIndex, (int)aStart.mRunSeed);
		AlignRunToHost((int)aStart.mRunSeed, (int)aStart.mRunLevelIndex, (int)aStart.mRunMode);
		mOnlineRunStartHeld = true;
		mOnlineRunGo = false;		// 这条是新命令：上一次的放行作废
		mOnlineSession->SendStartAck();
		return;
	}

	// 整队重来：队友刚输了、主机点了 Try Again。本机还停在吃脑子的残局上（棋盘还在，
	// 但已经判负），把残局拆掉照样进新局——这跟"人还在关卡里打"是两回事，
	// 回绝的话主机的重开就白按了（见 LawnApp::RetryOnlineLevel）。
	if (mGameScene != GameScenes::SCENE_MENU && mBoard != nullptr
		&& mBoardResult == BoardResult::BOARDRESULT_LOST)
	{
		TodLog("[net] the host asked for a retry - clearing the lost level");
		KillDialog(Dialogs::DIALOG_GAME_OVER);
		KillBoard();
	}
	else if (mGameScene != GameScenes::SCENE_MENU)
	{
		// 我在关卡里（比如上一局的退关还没走到这儿），这条开局命令接不了。
		// 不能装没看见：主机在那边等 ACK，一直等不到就只能超时作废。回一句"现在不行"，
		// 主机当场就能说清楚是队友忙，而不是干等六秒。
		TodTrace("online start refused: scene %d is not the menu", (int)mGameScene);
		mOnlineSession->SendStartAck(false);
		return;
	}

	// @pvz-online: 开局流程改成"房主开始 → 其他人确认"：菜单上的人先看询问框，点了"加入"
	// 才走下面那条进场路（见 OnlineStartPromptAnswer → EnterOnlineStart）。走到这儿的
	// 只剩一种客人：停在吃脑子残局上、主机点了整队重来的人——那个不问，重开是整队的决定。
	if (mGameScene == GameScenes::SCENE_MENU && mBoard == nullptr)
	{
		ShowOnlineStartPrompt(aStart);
		return;
	}

	EnterOnlineStart(aStart);
}

// @pvz-online: 主机等待框上按了"取消"：这次开局作废，人回主菜单。和"没有等到回应"
// 那条路一个收法（清记号、撤框、覆盖值作废、菜单交还），只是这次是玩家自己按的。
// 菜单不在等待里被拆（WillWaitForStartAck 为真时 GameSelector::Update 会保住它），
// 所以 ShowGameSelector 会把旧的拆掉重摆一个——上面被按死的按钮跟着一起活过来。
void LawnApp::OnlineStartWaitCancelled()
{
	if (!mOnlineWaitingStartAck) return;	// 早撤了（已经进场 / 这次已经作废）：这一下不算数

	mOnlineWaitingStartAck = false;
	KillDialog(Dialogs::DIALOG_ONLINE_START);
	ClearOnlineStartOverride();
	ShowGameSelector();
	TodLog("online start: the host cancelled the wait");
}

// @pvz-online: 客户端进场的收口（主机的开局命令已经受理、该点都点过了）：摆好覆盖值、
// 拆菜单、回 ACK、建棋盘。两条来路——菜单上的询问框点了"加入"，还是人停在吃脑子的
// 残局上（整队重来）——都汇到这儿，顺序和以前一模一样。
void LawnApp::EnterOnlineStart(const NetProto::MsgStartLevel& theMsg)
{
	KillDialog(Dialogs::DIALOG_ONLINE);
	KillGameSelector();
	SetOnlineStartOverride((int)theMsg.mLevel, (int)theMsg.mLevelSeed);
	mGameMode = (GameMode)theMsg.mGameMode;
	TodTrace("online start: mode %d level %u seed %d",
		(int)mGameMode, (unsigned)theMsg.mLevel, (int)theMsg.mLevelSeed);
	// ACK 就是"我进关了"这句话，先把它发出去，主机才会跟着进
	mOnlineSession->SendStartAck();
	NewGame();
}

// @pvz-online: 客户端在菜单上收到主机的开局命令 → 先摆一张"是否加入"的询问框（联机开局
// 流程的客户端一半：房主开始 → 其他人显示是否开始）。命令连内容一起寄存进成员变量，
// 玩家点"加入"才走对齐/进场（见 OnlineStartPromptAnswer），点"暂不"回一句 ACK(false)。
// 菜单按钮照着主机等待那套按死：框只盖住屏幕中间那块，不按死的话角落还能点进别的页面，
// 进场时会拖着一堆没拆干净的界面。
void LawnApp::ShowOnlineStartPrompt(const NetProto::MsgStartLevel& theMsg)
{
	mOnlineStartPromptMsg = theMsg;
	mOnlineStartPromptFrames = 0;
	if (mOnlineStartPromptActive) return;   // 框已经挂着（主机又发了一条）：换个记号就行

	mOnlineStartPromptActive = true;
	TodLog("[net] the host starts a level - the invite is on screen");
	if (mGameSelector) mGameSelector->SetMenuButtonsDisabled(true);

	OnlineStartDialog* aDialog = new OnlineStartDialog(this, "房主开始了游戏", "是否加入？", "加入", "暂不",
		OnlineStartDialog::NOTIFY_INVITE_ANSWER);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_ONLINE_START, aDialog);
}

// 询问框上点了按钮（theAccepted = 加入）。框可能已经被撤了（超时/掉线），所以先认标记。
void LawnApp::OnlineStartPromptAnswer(bool theAccepted)
{
	if (!mOnlineStartPromptActive) return;
	mOnlineStartPromptActive = false;
	KillDialog(Dialogs::DIALOG_ONLINE_START);
	if (mGameSelector) mGameSelector->SetMenuButtonsDisabled(false);

	if (!theAccepted)
	{
		TodLog("[net] the start invite was turned down");
		if (mOnlineSession) mOnlineSession->SendStartAck(false);
		return;
	}

	if (mOnlineSession == nullptr) return;

	NetProto::MsgStartLevel aMsg = mOnlineStartPromptMsg;
	if (aMsg.mIsRun)
	{
		// 闯关：对齐进度 + 命令寄存 + 马上回 ACK（"我这就进场"），草坪与选项由
		// 主机的 RUN_GO 放行（见 UpdateOnlineStart / UpdateRunPick）
		TodLog("[run] the host calls us into level index %u (run seed %d, answered)",
			(unsigned)aMsg.mRunLevelIndex, (int)aMsg.mRunSeed);
		AlignRunToHost((int)aMsg.mRunSeed, (int)aMsg.mRunLevelIndex, (int)aMsg.mRunMode);
		mOnlineRunStartHeld = true;
		mOnlineRunGo = false;
		mOnlineSession->SendStartAck();
		return;
	}

	EnterOnlineStart(aMsg);
}

// 撤掉询问框。theSendAck = 顺带回一句"我不去"：超时要回（主机若还在等，当场就知道；
// 早作废了就只在日志里留一行），掉线不用（会话已经没了）。
void LawnApp::DismissOnlineStartPrompt(bool theSendAck)
{
	if (!mOnlineStartPromptActive) return;
	mOnlineStartPromptActive = false;
	KillDialog(Dialogs::DIALOG_ONLINE_START);
	if (mGameSelector) mGameSelector->SetMenuButtonsDisabled(false);
	if (theSendAck && mOnlineSession) mOnlineSession->SendStartAck(false);
}

// @pvz-online: 选卡等队友（SEEDS_READY）。挂在选卡界面关屏的门口（SeedChooserScreen::
// CloseSeedChooser 第一行）：返回 true = 这次关屏被拦下（等待框已挂上），false = 照常开打。
//
// 为什么要有这道门：Let's Rock 一按就开打，两边各按各的——选得快的那台能领先一整段，
// 越到后面卡池越大、选项越多，差得越明显（第三关以后才露出来就是因为它）。
//
// 为什么是"报状态"而不是"报事件"：这一关不用选卡的机器（卡池 ≤8 之类）根本走不到这儿，
// 它得在自己草坪建好时主动报一次"我这一轮的状态"。各家都在新一轮开始时报"还没选"、
// 真选好了再报"选好了"；收的人只读对面当下的状态，不挑时机清位——清位就是丢消息
// （对面报得早、本地清得晚，两边就会互相干等）。见 NetProtocol.h 的 SEEDS_READY 一段。
//
// 等待框：一张"你选好了、等队友"的看板。每帧自检会再摆（被谁误关都不会把等待卡住），
// 所以挂之前先看有没有，别叠两张。
static void EnsureSeedsWaitDialog(LawnApp* theApp)
{
	if (theApp->GetDialog(Dialogs::DIALOG_ONLINE_START) != nullptr) return;

	OnlineStartDialog* aDialog = new OnlineStartDialog(theApp,
		"等待队友选卡", "你已经选好植物了，队友选完就开打。", nullptr, nullptr,
		OnlineStartDialog::NOTIFY_NONE);
	theApp->CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	theApp->AddDialog(Dialogs::DIALOG_ONLINE_START, aDialog);
}

bool LawnApp::TryHoldSeedChooserForTeammates()
{
	if (!IsOnlineGame()) return false;			// 单机 / 掉了线：原版节奏，不等

	if (mOnlineSeedsHeld) return true;			// 已经拦下过一次：继续等（放行走 UpdateOnlineSeeds）

	mOnlineSession->SendSeedsReady(true);
	if (mOnlineSession->IsPeerSeedsReady())
	{
		TodLog("[net] the teammate picked their plants too - starting right away");
		return false;
	}

	mOnlineSeedsHeld = true;
	TodLog("[net] plants picked - holding the start until the whole team is ready");
	EnsureSeedsWaitDialog(this);
	return true;
}

// 门开着的时候每帧看一眼：对面也选好了就撤框、重新走一遍关屏（这一遍门会放行）。
// 掉线（会话没了 / 断了）也放行——等的人不会来了，后面本来就是单机收场，
// 别把选卡界面锁死在"等待队友"上。
void LawnApp::UpdateOnlineSeeds()
{
	if (!mOnlineSeedsHeld) return;

	bool aConnected = IsOnlineGame();
	if (aConnected && !mOnlineSession->IsPeerSeedsReady())
	{
		EnsureSeedsWaitDialog(this);			// 框被误关了再摆一张
		return;
	}

	TodLog(aConnected ? "[net] the whole team is ready - here we go"
		: "[net] the teammate is gone - the start is no longer held");
	KillDialog(Dialogs::DIALOG_ONLINE_START);
	mOnlineSeedsHeld = false;
	// @pvz-online: 两道门共用这一个开合（R7）。卡池>8 拦的是选卡界面的关屏（Let's Rock），
	// 放行 = 再走一遍关屏（这一遍门会放行）。卡池≤8 的关卡没有选卡界面（"选卡"是草坪上
	// 的三选一屏，门在 UpdateRunPick 的 ①），放行 = 解冻 + 开场——和那道门口的收口对上。
	if (mBoard != nullptr && mBoard->ChooseSeedsOnCurrentLevel())
	{
		if (mSeedChooserScreen != nullptr) mSeedChooserScreen->CloseSeedChooser();
	}
	else if (mBoard != nullptr && mRunIntroHeld)
	{
		mRunIntroHeld = false;
		mBoard->mCutScene->StartLevelIntro();
	}
}

// @pvz-online: 队友传过来的漏怪。收包链里不建僵尸（要动棋盘、加载美术），这里每帧
// 把队列取空——棋盘不在（主菜单、换关的空档）就直接丢掉：迟到的怪绝不能等下一关
// 的棋盘建好了再冒出来。
//
// 收到就交给 Board::AddRelayedZombie：同类型的怪从本棋盘右侧重新走进来，带着它
// 对面挨打后剩下的血。每只都打一行日志——玩家在两边看到的必须是同一只怪。
void LawnApp::UpdateOnlineRelay()
{
	if (!mOnlineSession) return;

	NetProto::MsgEscapedZombie aMsg;
	while (mOnlineSession->TakePendingEscapedZombie(aMsg))
	{
		if (mBoard == nullptr)
		{
			TodLog("[net] dropped a relayed zombie: no board to put it on (row %u type %u)",
				(unsigned)aMsg.mRow, (unsigned)aMsg.mZombieType);
			continue;
		}

		Zombie* aZombie = mBoard->AddRelayedZombie((int)aMsg.mRow, (ZombieType)aMsg.mZombieType,
			(int)aMsg.mBodyHealth, (int)aMsg.mHelmHealth, (int)aMsg.mShieldHealth, (int)aMsg.mFlyingHealth);
		if (aZombie == nullptr)
		{
			// 满场或包内容越界。掉一只怪等于把队友的惩罚取消了，但除了日志没有别的办法。
			TodLog("[net] could not put the relayed zombie on the board: row %u type %u hp %d",
				(unsigned)aMsg.mRow, (unsigned)aMsg.mZombieType, (int)aMsg.mBodyHealth);
			continue;
		}

		TodLog("[net] relayed zombie is on the board: row %u type %u hp %d/%d/%d/%d",
			(unsigned)aMsg.mRow, (unsigned)aMsg.mZombieType,
			(int)aMsg.mBodyHealth, (int)aMsg.mHelmHealth,
			(int)aMsg.mShieldHealth, (int)aMsg.mFlyingHealth);
	}
}

// @pvz-online: 队友退关（回主菜单）了没有。收到就跟着退——这一局对两边一起结束，会话留着，
// 两人都在菜单上，主机直接点关卡就能开下一局。这同时堵上了"退出方自己再开局会卡死"：
// 任何一边退关，另一边必定跟着回菜单，主机再开局时对面一定在菜单上等着接开局命令。
void LawnApp::UpdateOnlineLevelExit()
{
	if (!mOnlineSession) return;

	NetProto::MsgLevelExit aMsg;
	if (!mOnlineSession->TakePendingLevelExit(aMsg)) return;

	if (mBoard != nullptr)
	{
		TodLog("[net] a teammate left the level - going back to the main menu too");
		// 复用 DoBackToMain 的五步（停音乐/写配置/关暂停框/拆棋盘/回菜单）；
		// false 是因为"我要退"这句话对面已经先说了，不用回话。
		DoBackToMain(false);
		LawnMessageBox(Dialogs::DIALOG_MESSAGE, "Teammate left",
			"A teammate left the level.\nBack to the main menu.", "OK", "", Dialog::BUTTONS_FOOTER);
		return;
	}

	// 已经在菜单上（比如刚退完，或还没进关）：只写一句即时说明，界面不动。
	TodLog("[net] a teammate left the level (already in the menu)");
	mOnlineSession->PostNotice("A teammate left the level.");
}

// @pvz-online: 暂停同步。规则（已拍板）：任一方都能暂停，也任一方都能继续。
//
// 本机"我暂停了/我继续了"不挂钩子，看状态：暂停菜单（DIALOG_NEWOPTIONS）开着没有，
// 与上一帧比。这样 ESC、右上角 Menu、选卡界面的 Menu、Back to Game、Main Menu……
// 所有开合路径一网打尽，不用在六处调用点各加一行，也不会漏。
// 刻意不参与同步的是 DoPauseDialog 那个简化框（现在就剩空格键开的——联机环境下
// 切出去不再开它，见 LostFocus）：自己切出去不该把队友强按进暂停菜单。
void LawnApp::UpdateOnlinePause()
{
	if (!mOnlineSession) return;

	// ① 队友按了暂停 / 继续
	bool aPaused = false;
	if (mOnlineSession->TakePauseState(aPaused))
	{
		if (aPaused)
		{
			// 绝不直写 mBoard->mPaused：走 DoNewOptions 自己的模态链，让 ModalOpen 去停棋盘
			// （音效/音乐也跟着停）。已经有暂停框就只记状态，不叠第二个。
			if (mBoard != nullptr && GetDialog(Dialogs::DIALOG_NEWOPTIONS) == nullptr)
			{
				DoNewOptions(false);
				// 这一步是替队友做的，不算本机操作：下一段的检测器不该把它当成"我按的"
				mPauseMenuWasOpen = true;
				mPauseMenuFromPeer = true;
			}
		}
		else if (GetDialog(Dialogs::DIALOG_NEWOPTIONS) != nullptr)
		{
			// 任一方都能继续：不管这菜单是谁开的都收掉
			KillNewOptionsDialog();
		}
	}

	// ② 本机自己开/关了暂停菜单 → 告诉队友（SendPauseState 自己会做去重，没连上会返回 false）
	bool aNowOpen = mBoard != nullptr && GetDialog(Dialogs::DIALOG_NEWOPTIONS) != nullptr;
	if (aNowOpen != mPauseMenuWasOpen)
	{
		mPauseMenuWasOpen = aNowOpen;
		mPauseMenuFromPeer = false;
		mOnlineSession->SendPauseState(aNowOpen);
	}

	// ③ 队友掉线了：他那张"替队友弹的"暂停菜单没人能解（玩家自己没按过），收掉；
	// 玩家自己按出来的暂停菜单不动——那是他的操作，掉不掉线都该留着。
	if (mPauseMenuFromPeer && !mOnlineSession->IsConnected()
		&& GetDialog(Dialogs::DIALOG_NEWOPTIONS) != nullptr)
	{
		TodLog("[net] the teammate vanished while the pause menu was theirs - closing it");
		KillNewOptionsDialog();
		mPauseMenuWasOpen = false;
		mPauseMenuFromPeer = false;
	}
}

// @pvz-online: 会话事件的收口。以前没人取 PollEvent，事件在队列里越堆越多，掉线这件事
// 就只写在状态行上。
void LawnApp::UpdateOnlineEvents()
{
	if (!mOnlineSession) return;

	NetSession::Event anEvent;
	while (mOnlineSession->PollEvent(anEvent))
	{
		switch (anEvent.mType)
		{
		case NetSession::EventType::CONNECTED:
			TodLog("[net] the teammate is here - you can pick a level now");
			// @pvz-online: 联机闯关（R5）：本机正跑着一局、队友才连进来——立刻把他拉进
			// 当前这一关（他没检查点/对不上就走补发追赶）。不拉的话，这一关的"全队判胜"
			// 永远凑不齐：一个连上了却没棋盘的席位不会报"我清完了"。
			if (IsRunMode() && mBoard != nullptr
				&& mOnlineSession->GetRole() == NetSession::Role::HOST)
			{
				TodLog("[run] a teammate joined mid-level - pulling them into level %d (index %d)",
					mRunState->GetLevel(), mRunState->mLevelIndex);
				mOnlineSession->SendStartLevel((uint8_t)GameMode::GAMEMODE_ADVENTURE,
					(uint32_t)mRunState->GetLevel(), mRunState->GetLevelSeed(),
					true, mRunState->mRunSeed, (uint8_t)mRunState->mLevelIndex, (uint8_t)mRunState->mMode);
			}
			break;

		case NetSession::EventType::DISCONNECTED:
			TodLog("[net] connection lost: %s", mOnlineSession->GetStatusText().c_str());
			// 询问框挂着的客户端（人还在主菜单上）：对面没了，这张框也就没有意义了。
			// 不回 ACK——会话都没了。主机那边等待中的看板框由 UpdateOnlineStart 自己收
			// （它每帧查 IsConnected）。
			DismissOnlineStartPrompt(false);
			// @pvz-online: 闯关的"等放行"空档里掉线（命令收下了、RUN_GO 还没来）：放行
			// 永远等不到了，寄存作废。棋盘在的话下面整队收摊（ShowGameSelector 再清一遍）；
			// 人还在菜单上的就此放手——等不来放行，照旧按单机那条路进场（mRunState 不动）。
			mOnlineRunStartHeld = false;
			mOnlineRunGo = false;
			// 局中掉线：这一局打不下去了。让人留在一盘打不完的棋盘上比收摊更糟——
			// 漏怪传不出去（怪走到房子直接算输），"全队过关/全队败"又都得有对面才算数。
			// 所以照 M2 定的规矩收摊：提示一句 + 回主菜单。会话死在谁身上都不挡单机
			// （见 IsOnlineStartAllowed），玩家想自己开一局随时可以。
			// 顺序要紧：先退干净再弹框——弹框是阻塞的（WaitForResult 会泵主循环），
			// 退到一半的状态会在这期间被别的更新碰到；反面例子见 DoBackToMain 的注释。
			if (mBoard != nullptr)
			{
				std::string aReason = mOnlineSession->GetStatusText();
				DoBackToMain(false);
				LawnMessageBox(Dialogs::DIALOG_MESSAGE, "Disconnected",
					(aReason + "\nBack to the main menu.").c_str(), "OK", "", Dialog::BUTTONS_FOOTER);
			}
			break;

		case NetSession::EventType::PEER_JOINED:
			// @pvz-online: 中继：有人进了房（mSeat 说清是谁）——直连没有这条，那边的
			// 第一个人就是 CONNECTED。本机在棋盘上跑着闯关局、又是主机：当场把他拉进
			// 当前这一关（他没检查点/对不上就走补发追赶）。不拉的话这一关的"全队判胜"
			// 永远凑不齐——一个连上了却没棋盘的席位不会报"我清完了"。人还在菜单上就
			// 什么都不做（名册小条自己会显示他）。
			TodLog("[net] seat %u joined the room", (unsigned)anEvent.mSeat);
			if (IsRunMode() && mBoard != nullptr
				&& mOnlineSession->GetRole() == NetSession::Role::HOST)
			{
				TodLog("[run] seat %u joined mid-level - pulling them into level %d (index %d)",
					(unsigned)anEvent.mSeat, mRunState->GetLevel(), mRunState->mLevelIndex);
				mOnlineSession->SendStartLevel((uint8_t)GameMode::GAMEMODE_ADVENTURE,
					(uint32_t)mRunState->GetLevel(), mRunState->GetLevelSeed(),
					true, mRunState->mRunSeed, (uint8_t)mRunState->mLevelIndex,
					anEvent.mSeat, (uint8_t)mRunState->mMode);
			}
			break;

		case NetSession::EventType::PEER_LEFT:
			// @pvz-online: 中继：有人走了（服务器还在、我也还在；房主走 = ROOM_CLOSED，
			// 那条是 DISCONNECTED 的死路）。人还在棋盘上就挂一句说明接着打——少一个人
			// 不是这一局的死刑，判胜按"现在的上座席位"算，他那一格已经跟着回收；
			// 人还在菜单上就什么都不做。
			TodLog("[net] seat %u left the room", (unsigned)anEvent.mSeat);
			if (mBoard != nullptr)
			{
				mOnlineSession->PostNotice("A teammate left the room.");
			}
			break;

		default:
			break;
		}
	}
}

// @pvz-online: 这一关对全队结束了没有。三条出路都汇在这儿：
//   ① 我这块草坪清干净了 → 告诉队友（会话层按"上次发出去的值"去重，每帧问也只发一次）；
//   ② 所有席位都清完了 → 两边各自回主菜单（不发奖杯、不写档）；
//   ③ 队友报的全队败（末席漏怪）→ 跟着收摊。
//
// "清干净了"看的是棋盘自己的判据 mLevelAwardSpawned（波次打完、场上没怪，和原版掉过关
// 种子包是同一个条件），**并且**眼前真的一只怪都没有——队友那儿漏过来的怪一落地，
// 我这句"清完了"就得当场撤回，不然会赢在一只还在走的僵尸上。
void LawnApp::UpdateOnlineEnd()
{
	if (!mOnlineSession) return;

	// ①
	if (mBoard != nullptr)
	{
		// @pvz-online: 联机闯关（R5）的换关空档：点名还在飞的时候（主机等 ACK / 客户端把
		// 命令寄存着），屏幕上这块是**上一关**的棋盘——它当然清完了，但这一关的记账已经
		// 翻篇（SendStartLevel 清过"谁清完了"），再报一次会把两边都带成"全队清完"，
		// 白送一关。等新棋盘建起来（点名落地）这条通道自己就恢复了。
		bool aHandoff = mOnlineWaitingStartAck || mOnlineRunStartHeld;
		if (!aHandoff)
		{
			bool aClear = mBoard->mLevelAwardSpawned && !mBoard->AreEnemyZombiesOnScreen();
			mOnlineSession->SendLevelDone(aClear);

			// 单方先清完：棋盘上挂一句"等队友们"，别让人以为卡住了。这条消息自己会过期
			// （15 秒），所以在快到期时续一次，让等待期间一直看得见。
			bool aWaiting = aClear && !mOnlineSession->IsPeerLevelDone();
			if (aWaiting && (!mBoard->mAdvice->IsBeingDisplayed() || mBoard->mAdvice->mDuration < 50))
			{
				mBoard->DisplayAdvice(_S("Waiting for the teammates..."),
					MessageStyle::MESSAGE_STYLE_BIG_MIDDLE, AdviceType::ADVICE_NONE);
				mOnlineWaitingAdviceOn = true;
			}
			else if (!aWaiting && mOnlineWaitingAdviceOn)
			{
				mBoard->ClearAdvice(AdviceType::ADVICE_NONE);
				mOnlineWaitingAdviceOn = false;
			}
		}
	}

	// ② 全队清完
	if (mOnlineSession->TakeAllLevelsDone())
	{
		mOnlineWaitingAdviceOn = false;
		if (mBoard != nullptr)
		{
			// @pvz-online: 联机闯关（R5）：全队过了这一关，走的不是"回主菜单"那条路——
			// 回菜单会把 mRunState 删掉、这一局就没了。两边各自领本关的过关奖（各选各的），
			// 棋盘留着当换关的背景；选完由 UpdateRunPick 走"主机点名 / 队友等点名"。
			// 打完第 25 关的收尾（删检查点 + 回菜单 + 提示）两边各自做，做的是一样的。
			if (IsRunMode())
			{
				TodLog("[net] every lawn is clear - the run moves on");
				mRunState->AdvanceLevel();
				if (mRunState->IsComplete())
				{
					TodLog("[run] the run is complete");
					RunState::DeleteCheckpoint(mPlayerInfo->mId);
					ShowGameSelector();
					LawnMessageBox(Dialogs::DIALOG_MESSAGE, "Run complete",
						StrFormat("You made it through all %d levels!\nClick ADVENTURE for a new run.", mRunState->GetLevelCount()).c_str(),
						"OK", "", Dialog::BUTTONS_FOOTER);
				}
				else
				{
					mRunState->BeginLevelEndPicks();
				}
			}
			else
			{
				TodLog("[net] every lawn is clear - back to the main menu");
				DoBackToMain(false);
				LawnMessageBox(Dialogs::DIALOG_MESSAGE, "Level complete",
					"All lawns are clear - the level is over for the whole team.\nBack to the main menu.",
					"OK", "", Dialog::BUTTONS_FOOTER);
			}
		}
		return;
	}

	// ③ 队友那边的末席漏了怪（或是他主动收摊）
	NetProto::MsgGameOver aOver;
	if (mOnlineSession->TakePendingGameOver(aOver))
	{
		mOnlineWaitingAdviceOn = false;
		if (mBoard != nullptr)
		{
			// @pvz-online: 全队败得让两台机器看到同一件事——本机也把"吃脑子"演一遍，不再直接
			// 踹回主菜单（从前队友那边只看得到一句"他那边漏了"的弹框，和主机看到的对不上）。
			// theFromPeer=true：这是照着演，别再回发 GAME_OVER，也别吃"还有队友可传"那条早退。
			// 演完弹的 GAME OVER 里只有主机点得动 Try Again（见 GameOverDialog / RetryOnlineLevel）。
			TodLog("[net] the team lost (reason %u) - playing the lost-level ending here too",
				(unsigned)aOver.mReason);
			mBoard->ZombiesWon(nullptr, true);
		}
		else
		{
			TodLog("[net] the team lost (reason %u) while in the menu - nothing to do",
				(unsigned)aOver.mReason);
		}
	}
}

// @pvz-online: 全队败之后主机按了 Try Again——整队重来同一关。
// 关卡号和种子照抄刚输掉那一局（棋盘还在，读得到），所以两边重来的是同一张波表。
// 做法是"退回主菜单 + 再走一遍开局流程"：菜单上那条 START_ACK 链路是现成验证过的，
// 而队友此刻还停在吃脑子的画面上——他那头会把这条开局命令当成"重开"接住
// （见 UpdateOnlineStart 里那段）。退关那句不广播：这不是"有人要走"，是整队重来。
void LawnApp::RetryOnlineLevel()
{
	if (mOnlineSession == nullptr || mBoard == nullptr) return;
	if (mOnlineSession->GetRole() != NetSession::Role::HOST) return;

	// @pvz-online: 联机闯关（R5）：整队重来本关，这一局不能丢——走"回主菜单再开局"那条路
	// 会把 mRunState 删掉（ShowGameSelector），队友收到的就是一条没有闯关上下文的开局命令。
	// 所以闯关局直接在原地重发"本关、同波表种子"的换关命令，吃脑子的残局留着当背景，
	// 队友那边走的是同一条寄存 → 收口 → 回 ACK 的路（他的棋盘也是残局，会被拆掉）。
	if (IsRunMode())
	{
		int aRunLevel = mRunState->GetLevel();
		int aRunSeed = mRunState->GetLevelSeed();
		TodLog("[net] the host retries run level %d (index %d) - calling the team back in",
			aRunLevel, mRunState->mLevelIndex);
		SetOnlineStartOverride(aRunLevel, aRunSeed);
		mOnlineWaitingStartAck = true;
		mOnlineStartWaitFrames = 0;
		mOnlineSession->SendStartLevel((uint8_t)GameMode::GAMEMODE_ADVENTURE, (uint32_t)aRunLevel, aRunSeed,
			true, mRunState->mRunSeed, (uint8_t)mRunState->mLevelIndex, (uint8_t)mRunState->mMode);
		return;
	}

	int aLevel = mBoard->mLevel;
	int aSeed = mBoard->GetLevelRandSeed();
	uint8_t aMode = (uint8_t)mGameMode;

	DoBackToMain(false);
	SetOnlineStartOverride(aLevel, aSeed);
	mOnlineWaitingStartAck = true;
	mOnlineStartWaitFrames = 0;
	TodLog("[net] the host retried level %d - asking the team to come back in", aLevel);
	mOnlineSession->SendStartLevel(aMode, (uint32_t)aLevel, aSeed);
}

// @pvz-online: 闯关（肉鸽）。入口是主菜单主位那块烤字 ADVENTURE 的大墓碑（见 RequestAdventure；
// 第三槽的 PUZZLE 石板是原版战役入口，不吃这条路），流水线本身
// 和联机无关：一局 = 按固定顺序打 25 关（5 场景 × 5 关），每关的关卡号 + 波表种子喂给现成的覆盖通道
// （SetOnlineStartOverride）。R5 的联机闯关复用同一套。
//
// 按下入口只记一个请求（mPendingAdventure），真动手全在主循环：开局要拆面板、拆主菜单、
// 建棋盘，还可能先弹一个"续不续"的询问框（WaitForResult，只能从主循环里调）——
// 这套活在按钮自己的 Update 里干迟早出事（联机开局同理，见 UpdateOnlineStart）。

// 一局的种子：挂钟时间混帧计数——同一秒里连开两局也要开出不同的波表。
static int MakeRunSeed(int theAppCounter)
{
	return (int)((unsigned int)_time64(nullptr) ^ ((unsigned int)theAppCounter * 2654435761u));
}

// 入口（主位大墓碑）按下：冒险 = 组队闯关，所以没队伍的先把组队面板叫出来，队伍在手才谈开局。
// 返回 true = 这一下已经受理（开了面板 / 排进队列），调用方不用再干什么；
// 返回 false = 落回组队面板——客户端（主机的局还没来）、队友正进来、会话正断，
// 这几种局面都由面板的状态行说清卡在哪。
bool LawnApp::RequestAdventure()
{
	NetSession* aSession = mOnlineSession;
	bool aTeam = aSession != nullptr && aSession->IsActive()
		&& aSession->GetState() != NetSession::State::DEAD;

	if (!aTeam)
	{
		TodTrace("adventure: no team yet, opening the team panel");
		DoOnlineDialog();
		return true;
	}

	// 主机，队伍就我一个：单人闯关的起点。（只剩一个人的队伍也是一支队伍。）
	if (aSession->GetState() == NetSession::State::LISTENING)
	{
		TodTrace("adventure: a team of one, the run is queued");
		mPendingAdventure = true;
		return true;
	}

	// @pvz-online: 队友连着的主机（R5）：队伍齐了，开局还是由主机定——真正的开局在主循环里
	// （要拆面板、拆菜单、建棋盘，还可能先弹一个"续不续"的询问框），这里只排队。
	// 队友由开局命令一路拉进同一关（EnterRunLevel 广播 + 等 START_ACK），不会再出
	// "棋盘去等一个没进关的队友"那种谁也结束不了的局。
	// 客户端那台不排队：点这块牌还是落回老流程（弹面板说清在等主机选关）。
	if (aSession->GetState() == NetSession::State::CONNECTED
		&& aSession->GetRole() == NetSession::Role::HOST)
	{
		// 正在等队友确认上一个开局命令的时候再点不加事——那一下是在等回话，不是新请求。
		if (mOnlineWaitingStartAck)
		{
			TodTrace("adventure: already waiting for the teammate, the click is dropped");
			return true;
		}
		TodTrace("adventure: the team is here, the run is queued");
		mPendingAdventure = true;
		return true;
	}

	return false;
}

// 主循环里消费上面那个请求。队伍在这两帧之间可能已经变了（有人正好连进来、
// 或者会话刚断），所以条件再核一遍：必须是"主机 + 队伍里没有会漏掉的人"。
// 队友连着的局面（R5）是合法的：上面的入口已经把命令广播出去，队友会被拉进同一关；
// 不核的话会开出"只有我在打"的棋盘去等一个没进关的队友，谁也结束不了这一关。
void LawnApp::UpdateAdventureRequest()
{
	if (!mPendingAdventure) return;
	mPendingAdventure = false;

	NetSession* aSession = mOnlineSession;
	bool aTeamed = aSession == nullptr
		|| (aSession->IsActive() && aSession->GetState() == NetSession::State::LISTENING)
		|| (aSession->IsActive() && aSession->GetState() == NetSession::State::CONNECTED
			&& aSession->GetRole() == NetSession::Role::HOST);
	if (!aTeamed)
	{
		TodTrace("adventure: the team changed before the run could start, dropped");
		return;
	}

	// 盘上有打到一半的检查点：先问一句续不续。打完的那一局收尾时检查点就删了
	// （见 UpdateRunEnd），所以这儿问的一定是"还有得打"的那一局。
	// @pvz-online: 这一问换成中文框（OnlineStartDialog）——"房主开始 → 是否继续存档"，
	// 开局流程的第一问。阻塞式（WaitForResult 泵主循环），联机等待照常跑。
	if (RunState::HasCheckpoint(mPlayerInfo->mId))
	{
		OnlineStartDialog* aDialog = new OnlineStartDialog(this,
			"继续闯关？", "有一局没有打完。", "继续", "新开一局", OnlineStartDialog::NOTIFY_NONE);
		CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
		AddDialog(Dialogs::DIALOG_ONLINE_START, aDialog);
		if (aDialog->WaitForResult() == Dialog::ID_YES)
		{
			ContinueRun();
			return;
		}
	}

	// 新局（无检查点，或上面选了"新开一局"）：先选时长档再开局。同样阻塞式；返回值是
	// 卡片按钮编号（Mode0 = 完整版，依序普通/快速），取消 = 关弹窗不开局。
	// 联机不另问：只有主机走到这儿，选完由 START_LEVEL 的模式字节带动队友对齐。
	RunModeDialog* aModeDialog = new RunModeDialog(this);
	CenterDialog(aModeDialog, aModeDialog->mWidth, aModeDialog->mHeight);
	AddDialog(Dialogs::DIALOG_ONLINE_START, aModeDialog);
	int aModeResult = aModeDialog->WaitForResult();
	if (aModeResult < RunModeDialog::RunModeDialog_Mode0 || aModeResult > RunModeDialog::RunModeDialog_Mode2)
	{
		TodTrace("adventure: the mode picker was cancelled, no run starts");
		return;
	}
	StartRun(aModeResult - RunModeDialog::RunModeDialog_Mode0);
}

// @pvz-online: 进入游戏后的玩法公告（2026-10-03 用户要的）：启动后第一次落到主菜单时弹一次，
// 简要说明联机玩法与功能（正文按 OnlineStartDialog 的 '\n' 手动分行）。每进程只弹一次；
// 阻塞式（WaitForResult 泵主循环）——这时候会话要么还没建、要么在后台自己跑心跳，不受影响。
void LawnApp::UpdateStartupAnnounce()
{
	if (mShowedStartupAnnounce) return;
	if (mGameSelector == nullptr) return;	// 还停在标题屏 / 加载中，不是"进游戏"

	mShowedStartupAnnounce = true;

	OnlineStartDialog* aDialog = new OnlineStartDialog(this, "欢迎来到 PvZ 联机合作版",
		"· 2~4 人合作各守一块草坪，漏掉的僵尸\n"
		"  传给下一位队友（保留血量）。\n"
		"· 最后一位漏怪 = 全队失败；联机局没有除草机。\n"
		"· 出怪量按席位顺位递减（1 号位最多）。\n"
		"· 主位大墓碑 = 组队 / 加入房间；队友连着时\n"
		"  主机可发起「组队闯关」（25 关跨场景连打）。\n"
		"· 第三槽 PUZZLE 石板 = 一起打单关 / 原版战役。\n"
		"· 局内：ESC 暂停；T / E 短语与表情（数字键选）。\n"
		"· 左上小条 = 名册与换位；回主菜单 = 一起退关。",
		"知道了", nullptr, OnlineStartDialog::NOTIFY_NONE);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_ONLINE_START, aDialog);
	aDialog->WaitForResult();
}

void LawnApp::StartRun(int theRunMode)
{
	delete mRunState;
	mRunState = new RunState();
	mRunState->StartNew(MakeRunSeed(mAppCounter), theRunMode);
	TodLog("[run] a new run starts (seed %d, mode %d)", mRunState->mRunSeed, mRunState->mMode);
	// 手里的两株不够开局：先挑两株（两次三选一），选完 RunPickChosen 才进第 1 关。
	mRunState->BeginStartPicks();
}

void LawnApp::ContinueRun()
{
	delete mRunState;
	mRunState = new RunState();

	// 读不出检查点（没有 / 版本不符 / 内容坏了）或者那一局其实已经打完了，都当"从头开一局"：
	// 玩家选的是续，但给出去的东西必须永远是一条能走的路。
	if (!mRunState->Load(mPlayerInfo->mId) || mRunState->IsComplete())
	{
		mRunState->StartNew(MakeRunSeed(mAppCounter));
		TodLog("[run] no usable checkpoint, a new run starts instead");
	}
	else
	{
		TodLog("[run] continuing at level %d (seed %d)", mRunState->mLevelIndex, mRunState->mRunSeed);
	}
	EnterRunLevel();
}

// 进（下一）关：照联机客户端开局的同一套顺序——先摆好覆盖值再拆 UI、建棋盘，
// 这样 InitLevel / GetLevelRandSeed 读到的一定是这一关的关卡号和种子。
// 检查点不在这儿写：要等这一关的选卡做完（池子齐了）才落盘，见 UpdateRunPick 的 ①。
void LawnApp::EnterRunLevel()
{
	int aLevel = mRunState->GetLevel();
	int aSeed = mRunState->GetLevelSeed();

	mGameMode = GameMode::GAMEMODE_ADVENTURE;
	SetOnlineStartOverride(aLevel, aSeed);
	TodTrace("run: entering level %d (index %d, seed %d)", aLevel, mRunState->mLevelIndex, aSeed);

	// @pvz-online: 队友连着的时候（R5），换关命令由主机广播、等 START_ACK——和单关联机
	// 开局同一条链路；主菜单照旧留着当等待的看板，并且把上面的入口全按下去
	// （等待里开出的模态框会横跨到入场之后）。客户端的"进关"是主机点名点出来的，
	// 走的是下面那条路（见 UpdateRunPick 的收口）。
	if (WillWaitForStartAck())
	{
		TodLog("[run] the host calls the team into level %d (index %d)",
			aLevel, mRunState->mLevelIndex);
		mOnlineWaitingStartAck = true;
		mOnlineStartWaitFrames = 0;
		if (mGameSelector) mGameSelector->SetMenuButtonsDisabled(true);
		mOnlineSession->SendStartLevel((uint8_t)mGameMode, (uint32_t)aLevel, aSeed,
			true, mRunState->mRunSeed, (uint8_t)mRunState->mLevelIndex, (uint8_t)mRunState->mMode);
		return;
	}

	KillDialog(Dialogs::DIALOG_ONLINE);
	KillGameSelector();
	// @pvz-online: 客户端这条"进关"是主机点名 + RUN_GO 放行点出来的（R6；命令收到时
	// 就已经回过 ACK 了，那儿的含义是"我这就进场"）。走到这一步，寄存的使命结束：
	// 欠下的三选一挪到新草坪上做（NewGame 会压住开场，见 UpdateRunPick）。整队重来时
	// 人还停在"吃脑子"的残局上，顺手把残局拆掉。单机局 IsOnlineClient 为假。
	if (IsOnlineClient())
	{
		KillDialog(Dialogs::DIALOG_GAME_OVER);
	}
	mOnlineRunStartHeld = false;
	mOnlineRunGo = false;
	NewGame();
}

// @pvz-online: 联机闯关（R5）：把本机进度对齐到主机点名的那一关。规则（§5.1 定案）：
// 队友无条件对齐房主进度——手里这局对不上（换了局种子 / 时长档不同）、或者本机居然超了主机，
// 就丢掉重来；盘上的检查点能用（同一局种子、同一档、序号不超过主机）就接着走，否则从这一局
// 的起点摆一局、把欠下的三选一补上。补做的屏和真打过的一模一样：候选由 runSeed + 关序号
// 推导，各抽各的。时长档（theRunMode，M4-b）必须和主机同一个档：档决定关卡表与奖励屏数，
// 档不对的检查点续了也是错的关表。
void LawnApp::AlignRunToHost(int theRunSeed, int theTargetIndex, int theRunMode)
{
	// 时长档来自对端（构建代次不同只提示、不拒连）：非法值按完整版处理，别让它把
	// 越界模式一路带进关卡表。
	if (theRunMode < RunState::RUN_MODE_FULL || theRunMode > RunState::RUN_MODE_QUICK)
	{
		TodLog("[run] the host named an unknown run mode %d - treating it as the full run", theRunMode);
		theRunMode = RunState::RUN_MODE_FULL;
	}

	// 关序号来自对端（构建代次不同只提示、不拒连）：越界就按第 1 关处理——
	// 旧构建的关卡表只有 5 格，混搭时别让它把越界值一路带进 LevelForIndex。
	if (theTargetIndex < 0 || theTargetIndex >= RunState::LevelCountForMode(theRunMode))
	{
		TodLog("[run] the host named an out-of-range level index %d - treating it as the first level",
			theTargetIndex);
		theTargetIndex = 0;
	}

	if (mRunState != nullptr
		&& (mRunState->mRunSeed != theRunSeed || mRunState->mMode != theRunMode
			|| mRunState->mLevelIndex > theTargetIndex))
	{
		TodLog("[run] the local run does not match the host (seed %d vs %d, mode %d vs %d, index %d vs %d) - rebuilding",
			mRunState->mRunSeed, theRunSeed, mRunState->mMode, theRunMode, mRunState->mLevelIndex, theTargetIndex);
		delete mRunState;
		mRunState = nullptr;
	}

	if (mRunState == nullptr)
	{
		mRunState = new RunState();
		if (!mRunState->Load(mPlayerInfo->mId)
			|| mRunState->mRunSeed != theRunSeed
			|| mRunState->mMode != theRunMode
			|| mRunState->mLevelIndex > theTargetIndex)
		{
			mRunState->StartNew(theRunSeed, theRunMode);
			mRunState->BeginStartPicks();
			TodLog("[run] aligning to the host: a fresh run at seed %d (mode %d), catching up to index %d",
				theRunSeed, theRunMode, theTargetIndex);
		}
		else
		{
			TodLog("[run] aligning to the host: reusing the local checkpoint at index %d",
				mRunState->mLevelIndex);
		}
	}

	if (mRunState->mLevelIndex < theTargetIndex)
	{
		mRunState->BeginCatchUp(theTargetIndex);
	}
}

// 这一关的收摊（CheckForGameEnd 的闯关分支）：闯关不写档、不发奖杯，过一关就是
// "关序号 +1"，还有剩余关卡就接着进下一关；25 关打完就把检查点删掉、回主菜单——
// 这一局已经结束了，下次点冒险该开的是新的一局，而不是再问一遍"续不续"。
void LawnApp::UpdateRunEnd()
{
	KillBoard();
	mRunState->AdvanceLevel();

	if (mRunState->IsComplete())
	{
		TodLog("[run] the run is complete");
		RunState::DeleteCheckpoint(mPlayerInfo->mId);
		ShowGameSelector();
		LawnMessageBox(Dialogs::DIALOG_MESSAGE, "Run complete",
			StrFormat("You made it through all %d levels!\nClick ADVENTURE for a new run.", mRunState->GetLevelCount()).c_str(),
			"OK", "", Dialog::BUTTONS_FOOTER);
	}
	else
	{
		// 过关奖：两株新植物 + 一个增益（三屏，各选一张），选完才进下一关。
		mRunState->BeginLevelEndPicks();
	}
}

// 三选一屏：该选而屏不在就开一张；卡都选完了就在这儿把下一关开起来。
// 候选也在这儿现抽——屏什么时候被开出来、上一屏选的是哪张，都由 RunState 的计数说了算，
// 所以这一屏重开多少次都是同一组三条。
//
// 进关卡这类"拆主菜单、建棋盘"的活儿一律留在这个主循环函数里干，不在按钮回调里干
// （和 UpdateAdventureRequest 同一条纪律）。
//
// @pvz-online: 联机闯关（R5/R6）让"选完就进关"分成几种走法：单机照旧；主机广播换关命令、
// 等队友 START_ACK；客户端等主机点名 + RUN_GO 放行。R6 起选项一律摆在新草坪上（见 NewGame
// 的压开场）：先由 NewGame 把这一关的草坪建起来，人站上去、屏盖上去，做完全部三选一
// （含补发追赶）才放开选卡与开场。联机局换关不拆棋盘：上一关的草坪留在那儿当背景
// （打赢的完胜场面、打输的吃脑子场面各自都说得通），等待提示挂在上面。
void LawnApp::UpdateRunPick()
{
	if (mRunState == nullptr) return;

	// ① 新草坪已经建好、开场压着（R6 起联机与单机闯关都走这条）：三选一屏就摆在草坪上，
	// 补发追赶一屏接一屏地补。全都清完才按最新的卡池重填卡槽、放开选卡与开场——
	// 和 NewGame 的收尾两行同款。草坪没建起来之前一张屏都不摆，这正是 R6 要的：
	// 选项不再盖在主菜单 / 上一关的残局上。
	if (mRunIntroHeld && mBoard != nullptr)
	{
		if (mRunState->HasPendingPick())
		{
			if (GetDialog(Dialogs::DIALOG_RUN_PICK) != nullptr) return;
			mRunState->RollChoices();
			// 抽干兜底（方案 §2.4）：增益池一条不剩时 RollChoices 把欠的屏作废了——
			// 那一刻三条全是哨兵、没得点，屏别开（开了没出口）；下一帧走下面的收尾。
			if (!mRunState->HasPendingPick()) return;
			RunPickDialog* aDialog = new RunPickDialog(this, mRunState);
			CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
			AddDialog(Dialogs::DIALOG_RUN_PICK, aDialog);
			return;
		}

		if (mRunState->IsCatchingUp())
		{
			mRunState->AdvanceCatchUp();
			TodLog("[run] caught up one level (now at index %d)", mRunState->mLevelIndex);
			return;
		}

		// @pvz-online: 卡池≤8 的开场门拦下之后的重入（等队友的那几帧）：下面这些事
		// 都是"只做一次"的（选卡界面已经建过，再建一次会撞断言），一件都别再碰；
		// 放行由 UpdateOnlineSeeds 负责，它每帧都在跑。
		if (mOnlineSeedsHeld) return;

		TodLog("[run] the picks are done - the chooser and the intro can start");
		// 卡槽在 InitLevel 建场时按"当时"的卡池填过，刚刚这几屏的新植物要重填一次
		// （卡池 ≤8 的关卡全程不开选卡界面，不重填这一关新选的植物就赶不上）。
		if (!mBoard->ChooseSeedsOnCurrentLevel())
		{
			mBoard->FillSeedBankFromRunPool();
		}
		else
		{
			// @pvz-online: 卡池是刚做完这几屏才越过 8 的（建场时还 ≤8，卡槽已按当时的池子
			// 填过并定好了格数）：这一关要开选卡界面，而界面的出槽数、开始按钮、Let's Rock
			// 的回填循环全按 mSeedBank->mNumPackets 走——先把它按"本关选 8 株"的口径重定、
			// 包袋清空，回到原版选卡关开局的状态；不然界面只有建场时那几格，选完的植物也
			// 回填不进去（快速版第 2 关"植物栏不刷新、旧卡卡在上面"的根因）。
			mBoard->mSeedBank->mNumPackets = mBoard->GetNumSeedsInBank();
			for (int i = 0; i < SEEDBANK_MAX; i++)
			{
				mBoard->mSeedBank->mSeedPackets[i].mPacketType = SeedType::SEED_NONE;
			}
			mBoard->mSeedBank->UpdateWidth();
		}
		// @pvz-online: 检查点写在这一刻（不是进关前）：盘上的"正在打的这一关"= 这一关的
		// 选卡全进了池、能直接开打的状态。进关前写的那一版留不住这几株——续关读回来
		// 池子缺选卡、选择屏又不再出现，这就是"从主界面继续丢掉初始两株"的根因。
		mRunState->Save(mPlayerInfo->mId);
		ShowSeedChooserScreen();

		// @pvz-online: "都确定才开场"（R7）。卡池≤8 的关卡全场不开选卡界面，按不了
		// Let's Rock、也就没有那道关屏门（TryHoldSeedChooserForTeammates）——这道门口
		// 是它的替身，等的东西一样：刚在 ShowSeedChooserScreen 里报过"我选好了"，
		// 队友没齐就等。等待期草坪继续冻着（mRunIntroHeld 保持 true，CutScene::Update
		// 一步不走）；齐了由 UpdateOnlineSeeds 撤框、解冻、开场。
		if (!mBoard->ChooseSeedsOnCurrentLevel() && IsOnlineGame()
			&& mOnlineSession->HasOtherSeats() && !mOnlineSession->IsPeerSeedsReady())
		{
			mOnlineSeedsHeld = true;
			TodLog("[net] no seed chooser on this level - holding the intro until the whole team is ready");
			EnsureSeedsWaitDialog(this);
			return;
		}

		mRunIntroHeld = false;
		mBoard->mCutScene->StartLevelIntro();
		return;
	}

	if (mBoard != nullptr)
	{
		// 棋盘还在的联机局只有一种"空档"：本关在我这儿已经结束了——打赢了等下一关，
		// 打输了等整队重来。草坪还在打的时候什么都不该做，否则会把刚开起来的这一关当场
		// 再点一遍名。
		if (!IsOnlineGame()) return;
		bool aMyLevelOver = mBoard->mLevelAwardSpawned || mBoardResult == BoardResult::BOARDRESULT_LOST;

		if (mOnlineSession->GetRole() == NetSession::Role::HOST)
		{
			// 主机只在一件事上点名：全队都报了清完，进下一关。光自己清完不算——队友还在打，
			// 点早了会把他拉进一关他还没打完的进度里。点名由 mOnlineWaitingStartAck 把关，
			// 点不重。自己欠的三选一不在这儿做（R6）：人各进各的新草坪，那几屏在 ① 做。
			if (!mOnlineWaitingStartAck && mOnlineSession->IsLocalLevelDone()
				&& mOnlineSession->IsPeerLevelDone())
			{
				EnterRunLevel();
			}
		}
		else if (mOnlineRunStartHeld && mOnlineRunGo && aMyLevelOver)
		{
			// 客户端：命令到了（held）、主机也放行了（RUN_GO）、自己的草坪打完了
			// （或输了等着重来）：收口进场。
			EnterRunLevel();
		}
		else if (aMyLevelOver
			&& (!mBoard->mAdvice->IsBeingDisplayed() || mBoard->mAdvice->mDuration < 50))
		{
			// 本关打完、还进不了下一关的这段：客户端是在等主机点名 / 放行，主机自己是在等
			// 队友把这一关打完 / 回话（主机点名的那两个前提都在别处挂着提示，这里只是兜底）。
			bool aHostRole = mOnlineSession->GetRole() == NetSession::Role::HOST;
			mBoard->DisplayAdvice(aHostRole ? _S("Waiting for the teammate...") : _S("Waiting for the host..."),
				MessageStyle::MESSAGE_STYLE_BIG_MIDDLE, AdviceType::ADVICE_NONE);
		}
		return;
	}

	// 棋盘不在：单机 / 主机在这儿进关；客户端要等主机的 RUN_GO 放行——放行一到就进场
	// （草坪先建好，欠下的三选一由 ① 摆上去）。
	// 会话断了的客户端不在此列（IsOnlineClient 为假）：等不来放行，照旧按单机那条路进场。
	if (IsOnlineClient())
	{
		if (mOnlineRunGo) EnterRunLevel();
		return;
	}
	// @pvz-online: 点名已经发出去了（主机在等 START_ACK）就别再点——这条路口每帧都经过，
	// 不拦的话等待期每一帧都会再广播一次 START_LEVEL：队友那头"房主开始了游戏 / 是否加入"
	// 的框会一次次重新武装（答过的又弹出来，再答一次又是一个迟到的 ACK）。
	// 等 ACK 的收口在 UpdateOnlineStart ③：人齐在那儿直接进场，不经过这里。
	if (mOnlineWaitingStartAck) return;
	EnterRunLevel();
}

// 玩家点了第 theIndex 张卡：收进局里（卡池 / buff 表），然后把屏关掉——还欠哪一屏、
// 什么时候进关卡，都由 UpdateRunPick 下一帧看着办。
void LawnApp::RunPickChosen(int theIndex)
{
	RunState* aRun = mRunState;
	if (aRun == nullptr) return;

	KillDialog(Dialogs::DIALOG_RUN_PICK);
	if (aRun->IsPlantPick())
	{
		aRun->TakePlantChoice(theIndex);
	}
	else
	{
		aRun->TakeBuffChoice(theIndex);
	}
}

// R4：输一关就记账——失败次数进检查点（首版只存不用）。紧跟着写盘是刻意的：
// 玩家接下来可能直接点 Main Menu 走人，那一笔也得在盘上。
// "本关重开"（Retry Level）不改任何状态：关序号没动，检查点里记的就是这一关。
void LawnApp::RunNoteFailure()
{
	if (mRunState == nullptr) return;

	mRunState->NoteLevelFailed();
	mRunState->Save(mPlayerInfo->mId);
	TodLog("[run] level %d failed (attempt %d)", mRunState->mLevelIndex,
		mRunState->mFailCounts[mRunState->mLevelIndex]);
}

// R3 的 buff 数值：线性条目 = 1 + 每层修正 × 层数；叠乘条目（方案 §2.3，急袭/速种）=
// (1 + 每层修正)^层数——每层在已有结果上再乘一次（整数次连乘，不用 powf，负底也稳）。
// 没拿到 / 不在闯关 = 1.0；加成型 = 每层加成 × 层数。下限保护：表里数字写歪也不至于把
// 间隔压成 0。非闯关局自动中性值，落点不需要判 mRunState。
float LawnApp::RunBuffMul(int theBuffId) const
{
	if (mRunState == nullptr) return 1.0f;

	const RunBuffDef& aDef = GetRunBuffDef(theBuffId);
	int aStacks = mRunState->GetBuffCount(theBuffId);
	float aMul = 1.0f;
	if (aDef.mMultiplicative)
	{
		for (int i = 0; i < aStacks; i++) aMul *= 1.0f + aDef.mPerStackMul;
	}
	else
	{
		aMul = 1.0f + aDef.mPerStackMul * (float)aStacks;
	}
	return aMul < 0.1f ? 0.1f : aMul;
}

int LawnApp::RunBuffAdd(int theBuffId) const
{
	if (mRunState == nullptr) return 0;
	return GetRunBuffDef(theBuffId).mPerStackAdd * mRunState->GetBuffCount(theBuffId);
}

// 单株升级和全局 buff 共用存储：单株的层数就存在 BuffStack 里，
// id = RUN_BUFF_COUNT + 表内下标（检查点格式因此不用区分两类）。叠乘口径同 RunBuffMul。
float LawnApp::RunPlantUpgradeMul(SeedType thePlant) const
{
	if (mRunState == nullptr) return 1.0f;

	int aIndex = RunPlantUpgradeIndexFor(thePlant);
	if (aIndex < 0) return 1.0f;

	const RunPlantUpgradeDef& aDef = GetRunPlantUpgradeDef(aIndex);
	int aStacks = mRunState->GetBuffCount(RUN_BUFF_COUNT + aIndex);
	float aMul = 1.0f;
	if (aDef.mMultiplicative)
	{
		for (int i = 0; i < aStacks; i++) aMul *= 1.0f + aDef.mPerStackMul;
	}
	else
	{
		aMul = 1.0f + aDef.mPerStackMul * (float)aStacks;
	}
	return aMul < 0.1f ? 0.1f : aMul;
}

int LawnApp::RunPlantUpgradeCount(SeedType thePlant) const
{
	if (mRunState == nullptr) return 0;

	int aIndex = RunPlantUpgradeIndexFor(thePlant);
	if (aIndex < 0) return 0;
	return mRunState->GetBuffCount(RUN_BUFF_COUNT + aIndex);
}

//0x44F5F0
// GOTY @Patoke: 0x4528B0
void LawnApp::MakeNewBoard()
{
	KillBoard();
	mBoard = new Board(this);
	mBoard->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mBoard);
	mWidgetManager->BringToBack(mBoard);
	mWidgetManager->SetFocus(mBoard);
}

//0x44F6B0
// GOTY @Patoke: 0x452970
void LawnApp::StartPlaying()
{
	KillSeedChooserScreen();
	mBoard->StartLevel();
	mGameScene = GameScenes::SCENE_PLAYING;
}

//0x44F700
bool LawnApp::SaveFileExists()
{
	std::string aFileName = GetSavedGameName(GameMode::GAMEMODE_ADVENTURE, mPlayerInfo->mId);
	return this->FileExists(aFileName);
}

//0x44F7A0
// GOTY @Patoke: 0x452A50
bool LawnApp::TryLoadGame()
{
	std::string aSaveName = GetSavedGameName(mGameMode, mPlayerInfo->mId);
	mMusic->StopAllMusic();

	if (this->FileExists(aSaveName))
	{
		MakeNewBoard();
		if (mBoard->LoadGame(aSaveName))
		{
			mFirstTimeGameSelector = false;
			DoContinueDialog();
			return true;
		}

		KillBoard();
	}

	return false;
}

//0x44F890
// GOTY @Patoke: 0x452B30
void LawnApp::NewGame()
{
	mFirstTimeGameSelector = false;

	MakeNewBoard();
	mBoard->InitLevel();
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mGameScene = GameScenes::SCENE_LEVEL_INTRO;

	// @pvz-online: 选卡等队友（SEEDS_READY）：新一轮从"草坪刚建好"这一刻算起，先报
	// "我还没选好"。报在这儿而不是选卡界面打开时：这一关欠着三选一（含补发追赶）的机器
	// 可能过很久才轮到选卡，中间这段不能让对面以为"他早就选好了"（那会直接放行、白等一场）。
	// 真选好了（按 Let's Rock / 这一关不用选卡）再报 1，见 TryHoldSeedChooserForTeammates
	// 与 ShowSeedChooserScreen。
	if (IsOnlineGame()) mOnlineSession->SendSeedsReady(false);

	// @pvz-online: 联机闯关（R6）：草坪先建、选项后做——这一关还欠着三选一（含补发追赶）
	// 的时候，把"选卡 + 开场"压住：人先站在自己的新草坪上，选项屏盖在草坪上做完，
	// 才由 UpdateRunPick 放开（见那边的 ①）。单关局 / 非闯关一条路都不变。
	mRunIntroHeld = false;
	if (IsRunMode() && (mRunState->HasPendingPick() || mRunState->IsCatchingUp()))
	{
		mRunIntroHeld = true;
		TodLog("[run] the lawn is up before the picks - the intro is held");
		return;
	}

	ShowSeedChooserScreen();
	mBoard->mCutScene->StartLevelIntro();
}

//0x44F8E0
// GOTY @Patoke: 0x452B80
void LawnApp::ShowGameSelector()
{
	KillBoard();
	// @pvz-online: 回主菜单 = 这次在关卡里的运行结束了。闯关状态（卡池 / buff / 关序号）
	// 就在这儿丢——检查点每关开打时已经写进 userdata/run%d.dat，CONTINUE RUN 再读回来。
	// 只有"正在关卡里打"的那些时候 IsRunMode() 才为真。
	delete mRunState;
	mRunState = nullptr;
	// @pvz-online: 回主菜单就把联机开局参数扔掉。它在整局里都得留着（选卡界面也会用
	// GetLevelRandSeed 抽植物），所以只能在这个"一局已经结束"的点上清。
	ClearOnlineStartOverride();
	// 只对"当前这一局"有意义的收包队列（开局命令/漏怪/退关）同理：留着的话，
	// 上一局的怪会砸到下一局的棋盘上。
	if (mOnlineSession) mOnlineSession->DiscardLevelPackets();
	// @pvz-online: 客户端寄存的"主机点名"也作废——那条命令是给上一局的，回菜单就没有下一关了。
	// 随它一起的还有 R6 的放行标记与压开场标记（棋盘上面那句已经拆了，它们只剩残留价值）。
	mOnlineRunStartHeld = false;
	mOnlineRunGo = false;
	mRunIntroHeld = false;
	// @pvz-online: 选卡等队友（SEEDS_READY）的等待残留同理——留着的话下一局的门口会带着
	// 上一局的等待框（框本身也一起撤掉：等待态没了，框就没主了）。
	mOnlineSeedsHeld = false;
	KillDialog(Dialogs::DIALOG_ONLINE_START);
	//UpdateRegisterInfo();
	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		SafeDeleteWidget(mGameSelector);
	}

	mGameScene = GameScenes::SCENE_MENU;
	TodTrace("ShowGameSelector: constructing GameSelector");
	mGameSelector = new GameSelector(this);
	TodTrace("ShowGameSelector: GameSelector constructed %p", (void*)mGameSelector);
	mGameSelector->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mGameSelector);
	mWidgetManager->BringToBack(mGameSelector);
	mWidgetManager->SetFocus(mGameSelector);
	TodTrace("ShowGameSelector: added to manager");

	//if (NeedRegister())
	//{
	//	DoNeedRegisterDialog();
	//}
}

//0x44F9E0
// GOTY @Patoke: 0x452C70
void LawnApp::KillGameSelector()
{
	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		SafeDeleteWidget(mGameSelector);
		mGameSelector = nullptr;
	}
}

//0x44FA20
// GOTY @Patoke: 0x452CB0
void LawnApp::ShowAwardScreen(AwardType theAwardType, bool theShowAchievements)
{
	mGameScene = GameScenes::SCENE_AWARD;
	mAwardScreen = new AwardScreen(this, theAwardType, theShowAchievements);
	mAwardScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mAwardScreen);
	mWidgetManager->BringToBack(mAwardScreen);
	mWidgetManager->SetFocus(mAwardScreen);
}

//0x44FAF0
// GOTY @Patoke: 0x452D80
void LawnApp::KillAwardScreen()
{
	if (mAwardScreen)
	{
		mWidgetManager->RemoveWidget(mAwardScreen);
		SafeDeleteWidget(mAwardScreen);
		mAwardScreen = nullptr;
	}
}

//0x44FB30
// GOTY @Patoke: 0x452DC0
void LawnApp::ShowCreditScreen()
{
	mCreditScreen = new CreditScreen(this);
	mCreditScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mCreditScreen);
	mWidgetManager->BringToBack(mCreditScreen);
	mWidgetManager->SetFocus(mCreditScreen);
}

//0x44FBF0
void LawnApp::KillCreditScreen()
{
	if (mCreditScreen)
	{
		mWidgetManager->RemoveWidget(mCreditScreen);
		SafeDeleteWidget(mCreditScreen);
		mCreditScreen = nullptr;
	}
}

//0x44FC30
// GOTY @Patoke: 0x452EC0
void LawnApp::ShowChallengeScreen(ChallengePage thePage)
{
	mGameScene = GameScenes::SCENE_CHALLENGE;
	mChallengeScreen = new ChallengeScreen(this, thePage);
	mChallengeScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mChallengeScreen);
	mWidgetManager->BringToBack(mChallengeScreen);
	mWidgetManager->SetFocus(mChallengeScreen);
}

//0x44FD00
void LawnApp::KillChallengeScreen()
{
	if (mChallengeScreen)
	{
		mWidgetManager->RemoveWidget(mChallengeScreen);
		SafeDeleteWidget(mChallengeScreen);
		mChallengeScreen = nullptr;
	}
}

//0x44FD40
// GOTY @Patoke: 0x452FD0
StoreScreen* LawnApp::ShowStoreScreen()
{
	//FinishModelessDialogs();
	TOD_ASSERT(!GetDialog((int)Dialogs::DIALOG_STORE));

	StoreScreen* aStoreScreen = new StoreScreen(this);
	AddDialog(aStoreScreen);
	mWidgetManager->SetFocus(aStoreScreen);

	return aStoreScreen;
}

void LawnApp::KillStoreScreen()
{
	if (GetDialog(Dialogs::DIALOG_STORE))
	{
		KillDialog(Dialogs::DIALOG_STORE);
		ClearUpdateBacklog(false);
	}
}

//0x44FDC0
// GOTY @Patoke: 0x453050
void LawnApp::ShowSeedChooserScreen()
{
	TOD_ASSERT(mSeedChooserScreen == nullptr);

	// @pvz-online: 选卡等队友（SEEDS_READY）：新一轮的选卡界面要开了，本机这一轮的等待
	// 残留先清掉，同时把本机这一轮的状态报准——这一关根本不用选卡的机器（卡池 ≤8 之类
	// 全场不开选卡界面）直接算"选好了"：它走不到 Let's Rock，不主动报就会让队友干等。
	mOnlineSeedsHeld = false;
	if (IsOnlineGame() && mBoard != nullptr)
	{
		mOnlineSession->SendSeedsReady(!mBoard->ChooseSeedsOnCurrentLevel());
	}

	mSeedChooserScreen = new SeedChooserScreen();
	mSeedChooserScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mSeedChooserScreen);
	mWidgetManager->BringToBack(mSeedChooserScreen);
}

//0x44FE70
void LawnApp::KillSeedChooserScreen()
{
	if (mSeedChooserScreen)
	{
		mWidgetManager->RemoveWidget(mSeedChooserScreen);
		SafeDeleteWidget(mSeedChooserScreen);
		mSeedChooserScreen = nullptr;
	}
}

void LawnApp::EndLevel()
{
	KillBoard();
	if (IsAdventureMode())
	{
		NewGame();
	}

	mFirstTimeGameSelector = true;

	MakeNewBoard();
	mBoard->InitLevel();
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mGameScene = GameScenes::SCENE_LEVEL_INTRO;
	ShowSeedChooserScreen();
	mBoard->mCutScene->StartLevelIntro();
}

//0x44FEB0
void LawnApp::DoBackToMain(bool theNotifyOnline)
{
	// @pvz-online: 退关要告诉对面——不然一边在关卡里、一边在菜单上；退的那边再点关卡
	// 开局时会撞上"对面不在菜单、开局命令被丢掉"的卡死（见 UpdateOnlineLevelExit）。
	// theNotifyOnline=false 是"收到对面的退关、我跟着退"，别回声。
	if (theNotifyOnline && IsOnlineGame() && mBoard)
	{
		mOnlineSession->SendLevelExit(NetProto::EXIT_QUIT_TO_MENU);
	}

	mMusic->StopAllMusic();
	mSoundSystem->CancelPausedFoley();
	WriteCurrentUserConfig();
	KillNewOptionsDialog();
	KillBoard();
	ShowGameSelector();
}

//0x44FF00
void LawnApp::DoConfirmBackToMain()
{
	LawnDialog* aDialog = (LawnDialog*)DoDialog(
		Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN, 
		true, 
		_S("Leave Game?"/*"[LEAVE_GAME]"*/),
		_S("Do you want to return\nto the main menu?\n\nYour game will be saved."/*"[LEAVE_GAME_HEADER]"*/), 
		"", 
		Dialog::BUTTONS_YES_NO
	);

	aDialog->mLawnYesButton->mLabel = TodStringTranslate("[LEAVE_BUTTON]");
	aDialog->mLawnNoButton->mLabel = TodStringTranslate("[DIALOG_BUTTON_CANCEL]");
	//aDialog->CalcSize(0, 0);
}

//0x4500D0
// GOTY @Patoke: 0x453360
void LawnApp::DoNewOptions(bool theFromGameSelector)
{
	//FinishModelessDialogs();

	NewOptionsDialog* aDialog = new NewOptionsDialog(this, theFromGameSelector);
	CenterDialog(aDialog, IMAGE_OPTIONS_MENUBACK->mWidth, IMAGE_OPTIONS_MENUBACK->mHeight);
	AddDialog(Dialogs::DIALOG_NEWOPTIONS, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

//0x450180
// GOTY @Patoke: 0x453410
AlmanacDialog* LawnApp::DoAlmanacDialog(SeedType theSeedType, ZombieType theZombieType)
{
	PerfTimer mTimer;
	mTimer.Start();

	//FinishModelessDialogs();

	AlmanacDialog* aDialog = new AlmanacDialog(this);
	AddDialog(Dialogs::DIALOG_ALMANAC, aDialog);
	mWidgetManager->SetFocus(aDialog);

	if (theSeedType != SeedType::SEED_NONE)
	{
		aDialog->ShowPlant(theSeedType);
	}
	else if (theZombieType != ZombieType::ZOMBIE_INVALID)
	{
		aDialog->ShowZombie(theZombieType);
	}

	int aDuration = mTimer.GetDuration();
	TodTrace("almanac load time: %d ms", aDuration);

	return aDialog;
}

//0x450220
// GOTY @Patoke: 0x453590
void LawnApp::DoContinueDialog()
{
	ContinueDialog* aDialog = new ContinueDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CONTINUE, aDialog);
}

//0x4502C0
void LawnApp::DoPauseDialog()
{
	mBoard->Pause(true);
	//FinishModelessDialogs();

	LawnDialog* aDialog = (LawnDialog*)DoDialog(
		Dialogs::DIALOG_PAUSED,
		true,
		_S("Resume Game"/*"[RESUME_GAME]"*/),
		_S("Click to resume game"), 
		_S("GAME PAUSED"/*"[GAME_PAUSED]"*/), 
		Dialog::BUTTONS_FOOTER
	);

	aDialog->mReanimation->AddReanimation(72.0f, 42.0f, ReanimationType::REANIM_ZOMBIE_NEWSPAPER);
	aDialog->mSpaceAfterHeader = 155;
	aDialog->CalcSize(0, 10);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
}

//0x4504B0
// GOTY @Patoke: 0x4538A0
int LawnApp::LawnMessageBox(int theDialogId, const SexyChar* theHeaderName, const SexyChar* theLinesName, const SexyChar* theButton1Name, const SexyChar* theButton2Name, int theButtonMode)
{
	Widget* aOldFocus = mWidgetManager->mFocusWidget;

	LawnDialog* aDialog = (LawnDialog*)DoDialog(theDialogId, true, theHeaderName, theLinesName, theButton1Name, theButtonMode);
	if (aDialog->mYesButton)
	{
		aDialog->mYesButton->mLabel = TodStringTranslate(theButton1Name);
	}
	if (aDialog->mNoButton)
	{
		aDialog->mNoButton->mLabel = TodStringTranslate(theButton2Name);
	}
	//aDialog->CalcSize(0, 0);

	mWidgetManager->SetFocus(aDialog);
	int aResult = aDialog->WaitForResult(true);
	mWidgetManager->SetFocus(aOldFocus);

	return aResult;
}

//0x450770
Dialog* LawnApp::DoDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	SexyString aHeader = TodStringTranslate(theDialogHeader);
	SexyString aLines = TodStringTranslate(theDialogLines);
	SexyString aFooter = TodStringTranslate(theDialogFooter);

	Dialog* aDialog = SexyAppBase::DoDialog(theDialogId, isModal, aHeader, aLines, aFooter, theButtonMode);
	if (mWidgetManager->mFocusWidget == nullptr)
	{
		mWidgetManager->mFocusWidget = aDialog;
	}

	return aDialog;
}

Dialog* LawnApp::DoDialogDelay(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	LawnDialog* aDialog = (LawnDialog*)SexyAppBase::DoDialog(theDialogId, isModal, theDialogHeader, theDialogLines, theDialogFooter, theButtonMode);
	aDialog->SetButtonDelay(30);
	return aDialog;
}

//0x450880
// GOTY @Patoke: 0x453C60
void LawnApp::DoUserDialog()
{
	KillDialog(Dialogs::DIALOG_USERDIALOG);

	UserDialog* aDialog = new UserDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_USERDIALOG, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

//0x450930
void LawnApp::FinishUserDialog(bool isYes)
{
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (aUserDialog)
	{
		if (isYes)
		{
			PlayerInfo* aProfile = mProfileMgr->GetProfile(StringToSexyStringFast(aUserDialog->GetSelName()));
			if (aProfile)
			{
				mPlayerInfo = aProfile;
				mWidgetManager->MarkAllDirty();

				if (mGameSelector)
				{
					mGameSelector->SyncProfile(true);
				}
			}
		}

		KillDialog(Dialogs::DIALOG_USERDIALOG);
	}
}

//0x450A10
// GOTY @Patoke: 0x453DE0
void LawnApp::DoCreateUserDialog()
{
	KillDialog(Dialogs::DIALOG_CREATEUSER);

	NewUserDialog* aDialog = new NewUserDialog(this, false);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CREATEUSER, aDialog);
}

//0x450AC0
void LawnApp::FinishCreateUserDialog(bool isYes)
{
	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(Dialogs::DIALOG_CREATEUSER);
	if (aNewUserDialog == nullptr)
		return;

	SexyString aName = aNewUserDialog->GetName();

	if (isYes && aName.empty())
	{
		DoDialog(
			Dialogs::DIALOG_CREATEUSERERROR,
			true,
			_S("Enter Your Name"),
			_S("Please enter your name to create a new user profile for storing high score data and game progress"),
			_S("OK"),
			Dialog::BUTTONS_FOOTER
		);
	}
	else if (mPlayerInfo == nullptr && (!isYes || aName.empty()))
	{
		DoDialog(
			Dialogs::DIALOG_CREATEUSERERROR,
			true,
			_S("Enter Your Name"/*"[ENTER_YOUR_NAME]"*/),
			_S("Please enter your name to create a new user profile for storing high score data and game progress"/*"[ENTER_NEW_USER]"*/),
			_S("OK"/*"[DIALOG_BUTTON_OK]"*/),
			Dialog::BUTTONS_FOOTER
		);
	}
	else if (!isYes)
	{
		KillDialog(Dialogs::DIALOG_CREATEUSER);
	}
	else
	{
		PlayerInfo* aProfile = mProfileMgr->AddProfile(aName);
		if (aProfile == nullptr)
		{
			DoDialog(
				Dialogs::DIALOG_CREATEUSERERROR,
				true,
				_S("Name Conflict"/*"[NAME_CONFLICT]"*/),
				_S("The name you entered is already being used.  Please enter a unique player name"/*"[ENTER_UNIQUE_PLAYER_NAME]"*/),
				_S("OK"/*"[DIALOG_BUTTON_OK]"*/),
				Dialog::BUTTONS_FOOTER
			);
		}
		else
		{
			mProfileMgr->Save();
			mPlayerInfo = aProfile;

			KillDialog(Dialogs::DIALOG_USERDIALOG);
			KillDialog(Dialogs::DIALOG_CREATEUSER);
			mWidgetManager->MarkAllDirty();

			if (mGameSelector)
			{
				mGameSelector->SyncProfile(true);
			}
		}
	}
}

//0x450E20
// GOTY @Patoke: 0x4541F0
void LawnApp::DoConfirmDeleteUserDialog(const SexyString& theName)
{
	KillDialog(Dialogs::DIALOG_CONFIRMDELETEUSER);
	DoDialog(
		Dialogs::DIALOG_CONFIRMDELETEUSER, 
		true, 
		_S("Are You Sure"/*"[ARE_YOU_SURE]"*/), 
		// StrFormat(TodStringTranslate(_S("[DELETE_USER_WARNING]")).c_str(), StringToSexyStringFast(theName))
		// @Patoke: didn't access this as 'const char*'
		StrFormat(_S("This will permanently remove '%s' from the player roster!")/**/, theName.c_str()),
		_S(""), 
		Dialog::BUTTONS_YES_NO
	);
}

//0x450F40
void LawnApp::FinishConfirmDeleteUserDialog(bool isYes)
{
	KillDialog(Dialogs::DIALOG_CONFIRMDELETEUSER);
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (aUserDialog == nullptr)
		return;

	mWidgetManager->SetFocus(aUserDialog);

	if (!isYes)
		return;

	SexyString aCurName = mPlayerInfo ? mPlayerInfo->mName : _S("");
	SexyString aName = aUserDialog->GetSelName();
	if (aName == aCurName)
	{
		mPlayerInfo = nullptr;
	}

	mProfileMgr->DeleteProfile(aName);
	aUserDialog->FinishDeleteUser();
	if (mPlayerInfo == nullptr)
	{
		mPlayerInfo = mProfileMgr->GetProfile(aUserDialog->GetSelName());
		if (mPlayerInfo == nullptr)
		{
			mPlayerInfo = mProfileMgr->GetAnyProfile();
		}
	}

	mProfileMgr->Save();
	if (mPlayerInfo == nullptr)
	{
		DoCreateUserDialog();
	}

	mWidgetManager->MarkAllDirty();
	if (mGameSelector != nullptr)
	{
		mGameSelector->SyncProfile(true);
	}
}

//0x451180
// GOTY @Patoke: 0x454560
void LawnApp::DoRenameUserDialog(const SexyString& theName)
{
	KillDialog(Dialogs::DIALOG_RENAMEUSER);

	NewUserDialog* aDialog = new NewUserDialog(this, true);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	aDialog->SetName(theName);
	AddDialog(Dialogs::DIALOG_RENAMEUSER, aDialog);
}

//0x451260
void LawnApp::FinishRenameUserDialog(bool isYes)
{
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (!isYes)
	{
		KillDialog(Dialogs::DIALOG_RENAMEUSER);
		mWidgetManager->SetFocus(aUserDialog);
		return;
	}

	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(Dialogs::DIALOG_RENAMEUSER);
	if (aUserDialog == nullptr || aNewUserDialog == nullptr)
		return;

	SexyString anOldName = aUserDialog->GetSelName();
	SexyString aNewName = aNewUserDialog->GetName();
	if (aNewName.empty())
		return;
	
	bool isCurrentUser = mProfileMgr->GetProfile(anOldName) == mPlayerInfo;
	if (!mProfileMgr->RenameProfile(anOldName, aNewName))
	{
		DoDialog(
			Dialogs::DIALOG_RENAMEUSERERROR,
			true,
			_S("Name Conflict"/*"[NAME_CONFLICT]"*/),
			_S("The name you entered is already being used.  Please enter a unique player name"/*"[ENTER_UNIQUE_PLAYER_NAME]"*/),
			_S("OK"/*"[DIALOG_BUTTON_OK]"*/),
			Dialog::BUTTONS_FOOTER
		);
		return;
	}

	mProfileMgr->Save();
	if (isCurrentUser)
	{
		mPlayerInfo = mProfileMgr->GetProfile(aNewName);
	}

	aUserDialog->FinishRenameUser(aNewName);
	mWidgetManager->MarkAllDirty();
	KillDialog(Dialogs::DIALOG_RENAMEUSER);
	mWidgetManager->SetFocus(aUserDialog);
}

//0x451490
void LawnApp::FinishNameError(int theId)
{
	KillDialog(theId);

	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(theId == Dialogs::DIALOG_CREATEUSERERROR ? Dialogs::DIALOG_CREATEUSER : Dialogs::DIALOG_RENAMEUSER);
	if (aNewUserDialog)
	{
		mWidgetManager->SetFocus(aNewUserDialog->mNameEditWidget);
	}
}

//0x4514D0
void LawnApp::FinishRestartConfirmDialog()
{
	mSawYeti = mBoard->mKilledYeti;

	KillDialog(Dialogs::DIALOG_CONTINUE);
	KillDialog(Dialogs::DIALOG_RESTARTCONFIRM);
	KillBoard();

	PreNewGame(mGameMode, false);
}

void LawnApp::DoCheatDialog()
{
	KillDialog(Dialogs::DIALOG_CHEAT);

	CheatDialog* aDialog = new CheatDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CHEAT, aDialog);
}

// @pvz-online: M2 联机面板。会话本身活在 mOnlineSession 里，关掉面板不会掐连接——
// 建房之后还要回主菜单点关卡才能开局。
void LawnApp::DoOnlineDialog()
{
	if (!mOnlineSession)
	{
		mOnlineSession = new NetSession();
		// 名字取本机档案里的玩家名：握手里报给对面，小条名册上"谁坐在几号位"就是它。
		// 会话只在这里新建，名字设一次就够——它不随每局收摊清掉（见 SetLocalName）。
		if (mPlayerInfo) mOnlineSession->SetLocalName(mPlayerInfo->mName.c_str());
	}

	KillDialog(Dialogs::DIALOG_ONLINE);

	OnlineDialog* aDialog = new OnlineDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_ONLINE, aDialog);
}

void LawnApp::FinishCheatDialog(bool isYes)
{
	CheatDialog* aCheatDialog = (CheatDialog*)GetDialog(Dialogs::DIALOG_CHEAT);
	if (aCheatDialog == nullptr)
		return;

	if (isYes && !aCheatDialog->ApplyCheat())
		return;

	KillDialog(Dialogs::DIALOG_CHEAT);
	if (isYes)
	{
		mMusic->StopAllMusic();
		mBoardResult = BoardResult::BOARDRESULT_CHEAT;
		PreNewGame(mGameMode, false);
	}
}

void LawnApp::FinishTimesUpDialog()
{
	KillDialog(Dialogs::DIALOG_TIMESUP);
}

// GOTY @Patoke: 0x5282E0
void LawnApp::DoConfirmSellDialog(const SexyString& theMessage)
{
	Dialog* aConfirmDialog = DoDialog(Dialogs::DIALOG_ZEN_SELL, true, _S("[ZEN_SELL_HEADER]"), theMessage, _S(""), Dialog::BUTTONS_YES_NO);
	aConfirmDialog->mYesButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_YES]"));
	aConfirmDialog->mNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_NO]"));
}

void LawnApp::DoConfirmPurchaseDialog(const SexyString& theMessage)
{
	LawnDialog* aComfirmDialog = (LawnDialog*)DoDialog(Dialogs::DIALOG_STORE_PURCHASE, true, _S("买下这个物品？"), theMessage, _S(""), Dialog::BUTTONS_YES_NO);
	aComfirmDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_YES]"));
	aComfirmDialog->mLawnNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_NO]"));
}

//0x451580
Dialog* LawnApp::NewDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	LawnDialog* aDialog = new LawnDialog(
		this, 
		theDialogId, 
		isModal, 
		SexyStringToStringFast(theDialogHeader), 
		SexyStringToStringFast(theDialogLines), 
		SexyStringToStringFast(theDialogFooter), 
		theButtonMode
	);

	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	return aDialog;
}

//0x451630
bool LawnApp::KillNewOptionsDialog()
{
	NewOptionsDialog* aNewOptionsDialog = (NewOptionsDialog*)GetDialog(Dialogs::DIALOG_NEWOPTIONS);
	if (aNewOptionsDialog == nullptr)
		return false;

	bool wantWindowed = !aNewOptionsDialog->mFullscreenCheckbox->IsChecked();
	bool want3D = aNewOptionsDialog->mHardwareAccelerationCheckbox->IsChecked();
	SwitchScreenMode(wantWindowed, want3D, false);

	KillDialog(Dialogs::DIALOG_NEWOPTIONS);
	ClearUpdateBacklog();
	return true;
}

//0x4516C0
bool LawnApp::KillAlmanacDialog()
{
	if (GetDialog(Dialogs::DIALOG_ALMANAC))
	{
		KillDialog(Dialogs::DIALOG_ALMANAC);
		ClearUpdateBacklog(false);
		return true;
	}

	return false;
}

//0x4516F0
bool LawnApp::NeedPauseGame()
{
	if (mDialogList.size() == 0)
		return false;

	if (mDialogList.size() == 1 && mDialogList.front()->mId != Dialogs::DIALOG_NEW_GAME)
	{
		int anId = mDialogList.front()->mId;
		if (anId == Dialogs::DIALOG_CHOOSER_WARNING || anId == Dialogs::DIALOG_PURCHASE_PACKET_SLOT || anId == Dialogs::DIALOG_IMITATER)
		{
			return false;
		}
	}

	return (mBoard == nullptr || mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN) && (mBoard == nullptr || mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM);
}

//0x451780
void LawnApp::ModalOpen()
{
	if (mBoard && NeedPauseGame())
	{
		mBoard->Pause(true);
	}
}

void LawnApp::ModalClose()
{
	if (mBoard && !NeedPauseGame())
	{
	mBoard->Pause(false);
	}
}

//0x451800
bool LawnApp::KillDialog(int theDialogId)
{
	if (SexyAppBase::KillDialog(theDialogId))
	{
		if (mDialogMap.size() == 0)
		{
			if (mBoard)
			{
				mWidgetManager->SetFocus(mBoard);
			}
			else if (mGameSelector)
			{
				mWidgetManager->SetFocus(mGameSelector);
			}
		}

		if (mBoard && !NeedPauseGame())
		{
			mBoard->Pause(false);
		}

		return true;
	}

	return false;
}

//0x451870
void LawnApp::ShowResourceError(bool doExit)
{
	SexyAppBase::ShowResourceError(doExit);
}

/*
void BetaSubmitFunc()
{
	if (gLawnApp)
	{
		gLawnApp->BetaSubmit(false);
	}
}
*/

//0x451880
// GOTY @Patoke: 0x454C60
void LawnApp::Init()
{
	DoParseCmdLine();
	if (!mTodCheatKeys)
	{
		mOnlyAllowOneCopyToRun = true;
	}

	// GOTY @Patoke: 0x60C590
	//if (!gSexyCache->Connected() &&
	//	gLawnApp->mTodCheatKeys &&
	//	MessageBox(gLawnApp->mHWnd, _S("Start SexyCache now?"), _S("SexyCache"), MB_YESNO) == IDYES &&
	//	WinExec("SexyCache.exe", SW_MINIMIZE) >= 32)
	//{
	//  // GOTY @Patoke: 0x60C490
	//	gSexyCache = SexyCache();
	//}
	//if (gSexyCache->Connected() && !gLawnApp->mTodCheatKeys)
	//{
	//  // GOTY @Patoke: 0x60C5B0
	//	gSexyCache->Disconnect();
	//}

	mSessionID = _time64(nullptr);
	mPlayTimeActiveSession = 0;
	mPlayTimeInactiveSession = 0;
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mSawYeti = false;

	SexyApp::Init();
	// @Patoke: horrible debug checks, breaks the whole exe in release mode
//#ifdef _DEBUG
	TodAssertInitForApp();
	TodLog("session id: %u", mSessionID);
//#endif

	if (!mResourceManager->ParseResourcesFile("properties\\resources.xml"))
	{
		ShowResourceError(true);
		return;
	}

	if (!TodLoadResources("Init"))
	{
		return;
	}

	PerfTimer mTimer;
	mTimer.Start();

	mProfileMgr->Load();

	std::string aCurUser;
	if (mPlayerInfo == nullptr && RegistryReadString("CurUser", &aCurUser))
	{
		mPlayerInfo = mProfileMgr->GetProfile(StringToSexyStringFast(aCurUser));
	}
	if (mPlayerInfo == nullptr)
	{
		mPlayerInfo = mProfileMgr->GetAnyProfile();
	}

	mMaxExecutions = GetInteger("MaxExecutions", 0);
	mMaxPlays = GetInteger("MaxPlays", 0);
	mMaxTime = GetInteger("MaxTime", 60);

	mTitleScreen = new TitleScreen(this);
	mTitleScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mTitleScreen);
	mWidgetManager->SetFocus(mTitleScreen);

#ifdef _DEBUG
	int aDuration = mTimer.GetDuration();
	TodTrace("loading: 'profiles' %d ms", aDuration);
#endif
	mTimer.Start();

	mMusic = new Music();
	mSoundSystem = new TodFoley();
	mEffectSystem = new EffectSystem();
	mEffectSystem->EffectSystemInitialize();

	mKonamiCheck = new TypingCheck();
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_UP);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_UP);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_DOWN);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_DOWN);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_LEFT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_RIGHT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_LEFT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_RIGHT);
	mKonamiCheck->AddChar('b');
	mKonamiCheck->AddChar('a');
	mMustacheCheck = new TypingCheck("mustache");
	mMoustacheCheck = new TypingCheck("moustache");
	mSuperMowerCheck = new TypingCheck("trickedout");
	mSuperMowerCheck2 = new TypingCheck("tricked out");
	mFutureCheck = new TypingCheck("future");
	mPinataCheck = new TypingCheck("pinata");
	mDanceCheck = new TypingCheck("dance");
	mDaisyCheck = new TypingCheck("daisies");
	mSukhbirCheck = new TypingCheck("sukhbir");

#ifdef _DEBUG
	aDuration = mTimer.GetDuration();
	TodTrace("loading: 'system' %d ms", aDuration);
#endif
	mTimer.Start();

	ReanimatorLoadDefinitions(gLawnReanimationArray, ReanimationType::NUM_REANIMS);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LOADBAR_SPROUT, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LOADBAR_ZOMBIEHEAD, true);

#ifdef _DEBUG
	aDuration = mTimer.GetDuration();
	TodTrace("loading: 'loaderbar' %d ms", aDuration);
#endif
	mTimer.Start();
}

//0x4522A0
bool LawnApp::ChangeDirHook(const char* /*theIntendedPath*/)
{
	return false;
}

//0x4522B0
void LawnApp::Start()
{
	if (mLoadingFailed)
		return;

	SexyAppBase::Start();
}

//0x4522C0
bool LawnApp::DebugKeyDown(int theKey)
{
	return SexyAppBase::DebugKeyDown(theKey);
}

//0x4522E0
void LawnApp::HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue)
{
	if (theParamName == "-tod")
	{
#ifdef _DEBUG
		mTodCheatKeys = true;
		mDebugKeysEnabled = true;
#endif
	}
	else
	{
		SexyApp::HandleCmdLineParam(theParamName, theParamValue);
	}
}

//0x452310
// GOTY @Patoke: 0x41E420
bool LawnApp::UpdatePlayerProfileForFinishingLevel()
{
	// @pvz-online: 联机局不写档。这里是"过关推进存档"的唯一闸口——CheckForGameEnd 和
	// Board::CompleteEndLevelSequenceForSaving 都汇到这儿——所以在这儿早退最省事、也最不漏。
	// 联机这一局不推进 mLevel、不发奖杯，两边各打各的、打完各回各的菜单（成长留 M4）。
	// 闯关局同理：一局 25 关全靠检查点记着，mPlayerInfo 一个字都不动。
	if (IsOnlineGame() || IsRunMode()) return false;

	bool aUnlockedNewChallenge = false;

	if (IsAdventureMode())
	{
		if (mBoard->mLevel == FINAL_LEVEL)
		{
			mPlayerInfo->SetLevel(1);  // 存档回到第 1-1 关
			mPlayerInfo->mFinishedAdventure++;  // 完成冒险模式周目数增加 1 次
			if (mPlayerInfo->mFinishedAdventure == 1)
			{
				mPlayerInfo->mNeedsMessageOnGameSelector = 1;
			}
		}
		else
		{
			mPlayerInfo->SetLevel(mBoard->mLevel + 1);  // 存档进入下一关
		}

		if (!HasFinishedAdventure() && mBoard->mLevel == 34)
		{
			mPlayerInfo->mNeedsMagicTacoReward = 1;
		}
	}
	else if (IsSurvivalMode())
	{
		if (mBoard->IsFinalSurvivalStage())
		{
			aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
			mBoard->SurvivalSaveScore();

			if (aUnlockedNewChallenge && HasFinishedAdventure())
			{
				int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
				if (aNumTrophies != 8 && aNumTrophies != 9)
				{
					mPlayerInfo->mHasNewSurvival = true;
				}
			}
		}
	}
	else if (IsPuzzleMode())
	{
		aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
		mPlayerInfo->mChallengeRecords[GetCurrentChallengeIndex()]++;

		if (!HasFinishedAdventure() && (mGameMode == GameMode::GAMEMODE_SCARY_POTTER_3 || mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3))
		{
			aUnlockedNewChallenge = false;
		}

		if (aUnlockedNewChallenge)
		{
			if (IsScaryPotterLevel())
			{
				mPlayerInfo->mHasNewScaryPotter = 1;
			}
			else
			{
				mPlayerInfo->mHasNewIZombie = 1;
			}
		}
	}
	else
	{
		aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
		mPlayerInfo->mChallengeRecords[GetCurrentChallengeIndex()]++;

		if (aUnlockedNewChallenge && HasFinishedAdventure())
		{
			int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
			if (aNumTrophies <= 17)
			{
				mPlayerInfo->mHasNewMiniGame = 1;
			}
		}
	}

	WriteCurrentUserConfig();

	return aUnlockedNewChallenge;
}

//0x4524F0
// GOTY @Patoke: 0x4558E0
void LawnApp::CheckForGameEnd()
{
	if (mBoard == nullptr || !mBoard->mLevelComplete)
		return;

	// @pvz-online: 联机局不在这儿收摊。下面那套是单机的"过关"：发奖杯、进下一关、推存档。
	// 联机里一块棋盘打完只说明"我这块草坪清了"，全队判胜要等所有席位都报过，由
	// UpdateOnlineEnd 统一收。联机局的 mLevelComplete 本来就是冷的（FadeOutLevel 早退了）。
	if (IsOnlineGame())
		return;

	// @pvz-online: 闯关同理，但收摊的活儿是另一套：不发奖杯、不推存档、不铺奖励屏，
	// 只是关序号 +1 再进下一关（打完了就回主菜单，见 UpdateRunEnd）。
	if (IsRunMode())
	{
		UpdateRunEnd();
		return;
	}

	bool aGotPostGameAchievements = mBoard->CheckForPostGameAchievements();
	bool aUnlockedNewChallenge = UpdatePlayerProfileForFinishingLevel();

	if (IsAdventureMode())
	{
		int aLevel = mBoard->mLevel;
		KillBoard();

		if (IsFirstTimeAdventureMode() && aLevel < 50)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
		}
		else if (aLevel == FINAL_LEVEL)
		{
			if (mPlayerInfo->mFinishedAdventure == 1)
			{
				ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
			}
			else
			{
				ShowAwardScreen(AwardType::AWARD_CREDITS_ZOMBIENOTE, true);
			}
		}
		else if (aLevel == 9 || aLevel == 19 || aLevel == 29 || aLevel == 39 || aLevel == 49)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
		}
		else
		{
			PreNewGame(mGameMode, false);
		}
	}
	else if (IsSurvivalMode())
	{
		if (mBoard->IsFinalSurvivalStage())
		{
			KillBoard();

			if (aUnlockedNewChallenge && HasFinishedAdventure())
			{
				ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
			}
			else
			{
				ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
			}
		}
		else
		{
			mBoard->mChallenge->mSurvivalStage++;
			KillGameSelector();
			mBoard->InitSurvivalStage();
		}
	}
	else if (IsPuzzleMode())
	{
		KillBoard();

		if (aUnlockedNewChallenge)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
		}
		else
		{
			ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_PUZZLE);
		}
	}
	else
	{
		KillBoard();

		if (aUnlockedNewChallenge && HasFinishedAdventure())
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
		}
		else
		{
			ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
		}
	}
}

void LawnApp::UpdatePlayTimeStats()
{
	static int aLastTime = -1;

	int aTickCount = GetTickCount();
	int aSession = (aTickCount - aLastTime) / 1000;

	if (mPlayerInfo && !mPlayerInfo->mHasUsedCheatKeys && !mDebugKeysEnabled && mTodCheatKeys)
	{
		mPlayerInfo->mHasUsedCheatKeys = 1;
	}

	if (aLastTime == -1)
	{
		aLastTime = aTickCount;
		return;
	}

	if (aSession > 0)
	{
		aLastTime = aTickCount;

		if ((mBoard == nullptr || !mBoard->mPaused) && mHasFocus && mLastTimerTime - mLastUserInputTick <= 10000)
		{
			mPlayTimeActiveSession += aSession;

			if (mBoard)
			{
				mBoard->mPlayTimeActiveLevel += aSession;
			}

			if (mPlayerInfo)
			{
				mPlayerInfo->mPlayTimeActivePlayer += aSession;
			}
		}
		else
		{
			mPlayTimeInactiveSession += aSession;

			if (mBoard)
			{
				mBoard->mPlayTimeInactiveLevel += aSession;
			}

			if (mPlayerInfo)
			{
				mPlayerInfo->mPlayTimeInactivePlayer += aSession;
			}
		}
	}
}

//0x452650
void LawnApp::UpdateFrames()
{
	// @pvz-online: 联机会话按真实帧推进，故意放在 aUpdateCount 循环外——心跳和超时算的是
	// 挂钟时间，不该跟着 slow/fast-mo 一起变快变慢。
	if (mOnlineSession)
	{
		mOnlineSession->Update();

		// @pvz-online: 组队成功就把联机面板收起来。面板是模态对话框，它在的时候后面
		// 所有牌子一个都点不着，而"组队成功后还要回主菜单点关卡牌开局"是必经的一步——
		// 玩家就是被这一点挡住的。状态收摊后由主菜单左侧的小状态条接着说
		// （OnlineStatusWidget），面板随时点小状态条能再叫回来。
		//
		// 只在"刚变成已连接"那一下收，不是连着就一直收：玩家自己点开面板看状态时
		// 不能被它按回去（那样面板就永远打不开了）。在这里关而不是在面板自己的
		// Update 里关：KillDialog 是当场 delete，不能在自己 Update 的中途把自己删掉。
		bool aConnected = mOnlineSession->IsConnected();
		if (aConnected && !mOnlineWasConnected)
			KillDialog(Dialogs::DIALOG_ONLINE);
		mOnlineWasConnected = aConnected;

		// @pvz-online: 对面问换位：把面板叫出来让玩家按同意/拒绝（面板开着别的地方
		// 点不着，但主循环照跑——等答复不会把心跳等断）。正在关卡里就直接回绝：
		// 顺位开局那一下就定死了，不该在棋盘上被问这事。
		if (mOnlineSession->HasIncomingSwapRequest())
		{
			if (mBoard != nullptr)
				mOnlineSession->AnswerSwapRequest(false);
			else if (GetDialog(Dialogs::DIALOG_ONLINE) == nullptr)
				DoOnlineDialog();
		}
	}
	// 开局要换场景、动一堆 UI，所以也放在循环外、widget 更新之前——收包链里干了迟早出事。
	UpdateOnlineStart();
	UpdateOnlineRelay();
	UpdateOnlineLevelExit();
	UpdateOnlinePause();
	UpdateOnlineEvents();
	UpdateOnlineEnd();
	// 选卡等队友（SEEDS_READY）：门口拦下的那次等对面也选好，齐了撤框、重新关屏。
	UpdateOnlineSeeds();
	// 闯关入口的请求（"冒险"牌只记账，动手在这儿）——和联机开局同一套路数。
	UpdateAdventureRequest();
	// 闯关的三选一屏（刚开局、刚过完一关）：不动 UI 和棋盘，只是把屏开出来等玩家点。
	UpdateRunPick();
	// 玩法公告：第一次落到主菜单时弹一次（进程内一次，联机开局之前先看得到）。
	UpdateStartupAnnounce();

	if ((!mActive || mMinimized) && mBoard)
	{
		mBoard->ResetFPSStats();
	}

#ifdef _DEBUG
	UpdatePlayTimeStats();
#endif

	int aUpdateCount = 1;
	if (gSlowMo)
	{
		++gSlowMoCounter;
		if (gSlowMoCounter < 4)
		{
			aUpdateCount = 0;
		}
		else
		{
			gSlowMoCounter = 0;
		}
	}
	else if (gFastMo)
	{
		aUpdateCount = 20;
	}

	for (int i = 0; i < aUpdateCount; i++)
	{
		mAppCounter++;
		
		if (mBoard)
		{
			mBoard->ProcessDeleteQueue();
		}

		SexyApp::UpdateFrames();

		mMusic->MusicUpdate();
		if (mLoadingThreadCompleted && mEffectSystem)
		{
			mEffectSystem->ProcessDeleteQueue();
		}

		CheckForGameEnd();
	}
}

void LawnApp::ToggleSlowMo()
{
	gSlowMoCounter = 0;
	gSlowMo = !gSlowMo;
	gFastMo = false;
}

void LawnApp::ToggleFastMo()
{
	gSlowMo = false;
	gFastMo = !gFastMo;
}

//0x452740
void LawnApp::LoadGroup(const char* theGroupName, int theGroupAveMsToLoad)
{
	PerfTimer aTimer;
	aTimer.Start();

	mResourceManager->StartLoadResources(theGroupName);
	while (!mShutdown && !mCloseRequest && !mLoadingFailed && TodLoadNextResource())
	{
		mCompletedLoadingThreadTasks += theGroupAveMsToLoad;
	}

	if (mShutdown || mCloseRequest)
		return;

	if (mResourceManager->HadError() || !ExtractResourcesByName(mResourceManager, theGroupName))
	{
		ShowResourceError();
		mLoadingFailed = true;
	}

	//int aTotalGroupWeight = mResourceManager->GetNumResources(theGroupName) * theGroupAveMsToLoad;
	//int aGroupTime = max(aTimer.GetDuration(), 0.0);
	//TraceLoadGroup(theGroupName, aGroupTime, aTotalGroupWeight, theGroupAveMsToLoad);
}

//0x4528E0
void LawnApp::LoadingThreadProc()
{
	if (!TodLoadResources("LoaderBar"))
		return;

	TodStringListLoad("Properties\\LawnStrings.txt");

	if (mTitleScreen)
	{
		mTitleScreen->mLoaderScreenIsLoaded = true;
	}

	const char* groups[] = { "LoadingFonts", "LoadingImages", "LoadingSounds" };
	int group_ave_ms_to_load[] = { 54, 9, 54 };
	for (int i = 0; i < 3; i++)
	{
		mNumLoadingThreadTasks += mResourceManager->GetNumResources(groups[i]) * group_ave_ms_to_load[i];
	}
	mNumLoadingThreadTasks += 636;
	mNumLoadingThreadTasks += GetNumPreloadingTasks();
	mNumLoadingThreadTasks += mMusic->GetNumLoadingTasks();

	PerfTimer aTimer;
	aTimer.Start();

	TodHesitationTrace("start loading");
	TodHesitationBracket aHesitationResources("Resources");
	TodHesitationTrace("loading thread start");

	LoadGroup("LoadingImages", 9);
	LoadGroup("LoadingFonts", 54);
	if (mLoadingFailed || mShutdown || mCloseRequest)
		return;

	aHesitationResources.EndBracket();
	TodTrace("loading '%s' %d ms", "resources", (int)aTimer.GetDuration());

	mMusic->MusicInit();
	// aDuration goes unused
	//int aDuration = max(aTimer.GetDuration(), 0.0);
	aTimer.Start();

	mPoolEffect = new PoolEffect();
	mPoolEffect->PoolEffectInitialize();
	mZenGarden = new ZenGarden();
	mReanimatorCache = new ReanimatorCache();
	mReanimatorCache->ReanimatorCacheInitialize();
	TodFoleyInitialize(gLawnFoleyParamArray, LENGTH(gLawnFoleyParamArray));

	TodTrace("loading '%s' %d ms", "stuff", (int)aTimer.GetDuration());
	aTimer.Start();

	TrailLoadDefinitions(gLawnTrailArray, LENGTH(gLawnTrailArray));
	TodTrace("loading '%s' %d ms", "trail", (int)aTimer.GetDuration());
	aTimer.Start();
	TodHesitationTrace("trail");
	
	TodParticleLoadDefinitions(gLawnParticleArray, LENGTH(gLawnParticleArray));
	//aDuration = max(aTimer.GetDuration(), 0.0);
	aTimer.Start();

	PreloadForUser();
	if (mLoadingFailed || mShutdown || mCloseRequest)
		return;

	//aDuration = max(aTimer.GetDuration(), 0.0);
	aTimer.Start();

	GetNumPreloadingTasks();
	aTimer.Start();
	LoadGroup("LoadingSounds", 54);
	TodTrace("loading '%s' %d ms", "sounds", (int)aTimer.GetDuration());
	TodHesitationTrace("finished loading");
}

//0x452C60
void LawnApp::FastLoad(GameMode theGameMode)
{
	if (!mShutdown)
	{
		mWidgetManager->RemoveWidget(mTitleScreen);
		SafeDeleteWidget(mTitleScreen);
		mTitleScreen = nullptr;

		PreNewGame(theGameMode, false);
	}
}

void LawnApp::LoadingThreadCompleted()
{
}

//0x452CB0
// GOTY @Patoke: 0x456150
void LawnApp::LoadingCompleted()
{
	TodTrace("LawnApp::LoadingCompleted enter (titleScreen=%p)", (void*)mTitleScreen);
	mWidgetManager->RemoveWidget(mTitleScreen);
	SafeDeleteWidget(mTitleScreen);
	mTitleScreen = nullptr;

	mResourceManager->DeleteImage("IMAGE_TITLESCREEN");

	TodTrace("LawnApp::LoadingCompleted -> ShowGameSelector");
	ShowGameSelector();
	TodTrace("LawnApp::LoadingCompleted done");
}

//0x452D80
void LawnApp::URLOpenFailed(const std::string& theURL)
{
	SexyAppBase::URLOpenFailed(theURL);
	KillDialog(Dialogs::DIALOG_OPENURL_WAIT);
	CopyToClipboard(theURL);

	std::string aString = 
		"Please open the following URL in your browser\n\n" + 
		theURL + 
		"\n\nFor your convenience, this URL has already been copied to your clipboard.";

	DoDialog(Dialogs::DIALOG_OPENURL_WAIT, true, _S("Open Browser"), _S("OK"), StringToSexyStringFast(aString), Dialog::BUTTONS_FOOTER);
}

//0x452EE0
void LawnApp::URLOpenSucceeded(const std::string& theURL)
{
	SexyAppBase::URLOpenSucceeded(theURL);
	KillDialog(Dialogs::DIALOG_OPENURL_WAIT);
}

//0x452F00
bool LawnApp::OpenURL(const std::string& theURL, bool shutdownOnOpen)
{
	DoDialog(
		Dialogs::DIALOG_OPENURL_WAIT, 
		true, 
		_S("Opening Browser"), 
		_S("Opening Browser"), 
		_S(""), 
		Dialog::BUTTONS_NONE
	);

	DrawDirtyStuff();

	return SexyAppBase::OpenURL(theURL, shutdownOnOpen);
}

//0x453040
// GOTY @Patoke: 0x4564F0
void LawnApp::ConfirmQuit()
{
	SexyString aBody = TodStringTranslate(_S("[QUIT_MESSAGE]"));
	SexyString aHeader = TodStringTranslate(_S("[QUIT_HEADER]"));
	LawnDialog* aDialog = (LawnDialog*)DoDialog(Dialogs::DIALOG_QUIT, true, aHeader, aBody, _S(""), Dialog::BUTTONS_OK_CANCEL);
	aDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[QUIT_BUTTON]"));
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
}

//0x4531D0
void LawnApp::PreDisplayHook()
{
	SexyApp::PreDisplayHook();
}


void LawnApp::ButtonPress(int) {}
void LawnApp::ButtonDownTick(int) {}
void LawnApp::ButtonMouseEnter(int) {}
void LawnApp::ButtonMouseLeave(int) {}
void LawnApp::ButtonMouseMove(int, int, int) {}

//0x4531E0
// GOTY @Patoke: 0x456690
void LawnApp::ButtonDepress(int theId)
{
	if (theId % 10000 >= 2000 && theId % 10000 < 3000)  // 按钮编号 theId ∈ [2000, 3000) 时，表示按下 theId - 2000 编号的对话中的“是”按钮
	{
		switch (theId - 2000)
		{
		case Dialogs::DIALOG_NEW_GAME:
			KillDialog(Dialogs::DIALOG_NEW_GAME);
			ShowGameSelector();
			return;

		case Dialogs::DIALOG_NEWOPTIONS:
			KillNewOptionsDialog();
			return;

		case Dialogs::DIALOG_PREGAME_NAG:
			DoRegister();
			return;

		case Dialogs::DIALOG_LOAD_GAME:
			return;

		case Dialogs::DIALOG_CONFIRM_UPDATE_CHECK:
			KillDialog(Dialogs::DIALOG_CONFIRM_UPDATE_CHECK);
			CheckForUpdates();
			return;

		case Dialogs::DIALOG_QUIT:
			KillDialog(Dialogs::DIALOG_QUIT);
			SendMessage(mHWnd, WM_CLOSE, 0, 0);
			return;

		case Dialogs::DIALOG_NAG:
			KillDialog(Dialogs::DIALOG_NAG);
			DoRegister();
			return;

		case Dialogs::DIALOG_INFO:
			KillDialog(Dialogs::DIALOG_INFO);
			return;

		case Dialogs::DIALOG_PAUSED:
			KillDialog(Dialogs::DIALOG_PAUSED);
			return;

		case Dialogs::DIALOG_NO_MORE_MONEY:
			KillDialog(Dialogs::DIALOG_NO_MORE_MONEY);
			mBoard->AddSunMoney(100);
			return;

		case Dialogs::DIALOG_BONUS:
			KillDialog(Dialogs::DIALOG_BONUS);
			return;

		case Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN:
			KillDialog(Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN);
			mBoardResult = BoardResult::BOARDRESULT_QUIT;
			mBoard->TryToSaveGame();
			DoBackToMain();
			return;

		case Dialogs::DIALOG_USERDIALOG:
			FinishUserDialog(true);
			return;

		case Dialogs::DIALOG_CREATEUSER:
			FinishCreateUserDialog(true);
			return;

		case Dialogs::DIALOG_CONFIRMDELETEUSER:
			FinishConfirmDeleteUserDialog(true);
			return;

		case Dialogs::DIALOG_RENAMEUSER:
			FinishRenameUserDialog(true);
			return;

		case Dialogs::DIALOG_CREATEUSERERROR:
		case Dialogs::DIALOG_RENAMEUSERERROR:
			FinishNameError(theId - 2000);
			return;

		case Dialogs::DIALOG_CHEAT:
			FinishCheatDialog(true);
			return;

		case Dialogs::DIALOG_RESTARTCONFIRM:
			FinishRestartConfirmDialog();
			return;

		case Dialogs::DIALOG_TIMESUP:
			FinishTimesUpDialog();
			return;

		// @Patoke todo: implement this
		case Dialogs::DIALOG_DELETEZOMBATAR:
			// DeleteZombatar();
			return;

		// @Patoke todo: implement this
		case Dialogs::DIALOG_ZOMBATAR_TOS:
			// SetHasDisplayedZombatarTOS();
			return;

		case 20008:
			KillDialog(20008);
			KillDialog(Dialogs::DIALOG_CHECKING_UPDATES);
			return;

		default:
			KillDialog(theId - 2000);
			return;
		}
	}

	if (theId % 10000 >= 3000 && theId < 4000)  // 按钮编号 theId ∈ [3000, 4000) 时，表示按下 theId - 3000 编号的对话中的“否”按钮
	{
		switch (theId - 3000)
		{
		case Dialogs::DIALOG_PREGAME_NAG:
			KillDialog(Dialogs::DIALOG_PREGAME_NAG);
			Shutdown();
			return;

		case Dialogs::DIALOG_LOAD_GAME:
			KillDialog(Dialogs::DIALOG_LOAD_GAME);
			return;

		case Dialogs::DIALOG_USERDIALOG:
			FinishUserDialog(false);
			return;

		case Dialogs::DIALOG_CREATEUSER:
			FinishCreateUserDialog(false);
			return;

		case Dialogs::DIALOG_CONFIRMDELETEUSER:
			FinishConfirmDeleteUserDialog(false);
			return;

		case Dialogs::DIALOG_RENAMEUSER:
			FinishRenameUserDialog(false);
			return;

		case Dialogs::DIALOG_CHEAT:
			FinishCheatDialog(false);
			return;

		case Dialogs::DIALOG_TIMESUP:
			FinishTimesUpDialog();
			return;

		case 10008:
			KillDialog(10008);
			KillDialog(Dialogs::DIALOG_CHECKING_UPDATES);
			return;

		default:
			KillDialog(theId - 3000);
			return;
		}
	}
}

// GOTY @Patoke: 0x4535CD
void LawnApp::CenterDialog(Dialog* theDialog, int theWidth, int theHeight)
{
	theDialog->Resize((BOARD_WIDTH - theWidth) / 2, (BOARD_HEIGHT - theHeight) / 2, theWidth, theHeight);
}

//0x453630
// GOTY @Patoke: 0x456B00
void LawnApp::PlayFoley(FoleyType theFoleyType)
{
	if (!mMuteSoundsForCutscene)
	{
		mSoundSystem->PlayFoley(theFoleyType);
	}
}

//0x453650
void LawnApp::PlayFoleyPitch(FoleyType theFoleyType, float thePitch)
{
	if (!mMuteSoundsForCutscene)
	{
		mSoundSystem->PlayFoleyPitch(theFoleyType, thePitch);
	}
}

//0x453670
SexyString LawnApp::GetStageString(int theLevel)
{
	int aArea = ClampInt((theLevel - 1) / LEVELS_PER_AREA + 1, 1, ADVENTURE_AREAS + 1);
	int aSub = theLevel - (aArea - 1) * LEVELS_PER_AREA;
	return StrFormat("%d-%d", aArea, aSub);
}

bool LawnApp::IsAdventureMode()
{
	return mGameMode == GameMode::GAMEMODE_ADVENTURE;
}

//0x4536D0
bool LawnApp::IsSurvivalMode()
{
	return mGameMode >= GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1 && mGameMode <= GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_5;
}

//0x4536F0
bool LawnApp::IsPuzzleMode()
{
	return
		(mGameMode >= GameMode::GAMEMODE_SCARY_POTTER_1 && mGameMode <= GameMode::GAMEMODE_SCARY_POTTER_ENDLESS) ||
		(mGameMode >= GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 && mGameMode <= GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS);
}

//0x453710
bool LawnApp::IsChallengeMode()
{
	return !IsAdventureMode() && !IsPuzzleMode() && !IsSurvivalMode();
}

bool LawnApp::IsSurvivalNormal(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

bool LawnApp::IsSurvivalHard(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

bool LawnApp::IsSurvivalEndless(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

bool LawnApp::IsEndlessScaryPotter(GameMode theGameMode)
{
	return theGameMode == GameMode::GAMEMODE_SCARY_POTTER_ENDLESS;
}

bool LawnApp::IsEndlessIZombie(GameMode theGameMode)
{
	return theGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS;
}

//0x453740
bool LawnApp::IsContinuousChallenge()
{
	return 
		IsArtChallenge() || 
		IsSlotMachineLevel() || 
		IsFinalBossLevel() || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || 
		mGameMode == GameMode::GAMEMODE_UPSELL || 
		mGameMode == GameMode::GAMEMODE_INTRO || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST;
}

bool LawnApp::IsArtChallenge()
{
	if (mBoard == nullptr)
		return false;

	return 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_WALLNUT || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_SUNFLOWER || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_SEEING_STARS;
}

//0x4537B0
bool LawnApp::IsSquirrelLevel()
{
	return mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SQUIRREL;
}

//0x4537D0
bool LawnApp::IsIZombieLevel()
{
	if (mBoard == nullptr)
		return false;

	return
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_2 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_4 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_5 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_6 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_7 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_8 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_9 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS;
}

//0x453820
bool LawnApp::IsShovelLevel()
{
	return mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SHOVEL;
}

//0x453840
// GOTY @Patoke: 0x456D10
// @pvz-online: 下面这一簇"档案关型"判定（1-5 保龄球 / 2-5 打僵尸 / 3-5 小僵尸 / 4-5 打罐子 /
// 4-10 雾夜 / 5-5 蹦极 / 10·20·30 迷你 boss / 50 僵王）都直接读 mPlayerInfo->mLevel——那是
// 本机档案进度，联机两端可以不一样。闯关借的是 GAMEMODE_ADVENTURE，档案停在哪一关都不许让
// 这些判定为真：点数倍率、传送带种子栏、boss 音乐全挂在这上面，两端档案不同就是直接不同步。
// 所以闯关里一律返回 false；原版战役（非闯关）行为一字不变。
bool LawnApp::IsWallnutBowlingLevel()
{
	if (IsRunMode())
		return false;	// @pvz-online: 闯关不算任何"档案关型"

	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING || mGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING_2)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 5;
}

//0x453870
bool LawnApp::IsSlotMachineLevel()
{
	return (mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE);
}

//0x453890
bool LawnApp::IsWhackAZombieLevel()
{
	if (IsRunMode())
		return false;	// @pvz-online: 闯关不算任何"档案关型"

	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_WHACK_A_ZOMBIE)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 15;
}

//0x4538C0
bool LawnApp::IsLittleTroubleLevel()
{
	if (IsRunMode())
		return false;	// @pvz-online: 闯关不算任何"档案关型"

	return (mBoard && (mGameMode == GameMode::GAMEMODE_CHALLENGE_LITTLE_TROUBLE || (mGameMode == GameMode::GAMEMODE_ADVENTURE && mPlayerInfo->mLevel == 25)));
}

//0x4538F0
bool LawnApp::IsScaryPotterLevel()
{
	if (IsRunMode())
		return false;	// @pvz-online: 闯关不算任何"档案关型"

	if (mGameMode >= GameMode::GAMEMODE_SCARY_POTTER_1 && mGameMode <= GameMode::GAMEMODE_SCARY_POTTER_9)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 35;
}

//0x453920
bool LawnApp::IsStormyNightLevel()
{
	if (IsRunMode())
		return false;	// @pvz-online: 闯关不算任何"档案关型"

	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_STORMY_NIGHT)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 40;
}

//0x453950
bool LawnApp::IsBungeeBlitzLevel()
{
	if (IsRunMode())
		return false;	// @pvz-online: 闯关不算任何"档案关型"

	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_BUNGEE_BLITZ)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 45;
}

//0x453980
bool LawnApp::IsMiniBossLevel()
{
	if (IsRunMode())
		return false;	// @pvz-online: 闯关不算任何"档案关型"

	if (mBoard == nullptr)
		return false;

	return
		(IsAdventureMode() && mPlayerInfo->mLevel == 10) ||
		(IsAdventureMode() && mPlayerInfo->mLevel == 20) ||
		(IsAdventureMode() && mPlayerInfo->mLevel == 30);
}

//0x4539D0
bool LawnApp::IsFinalBossLevel()
{
	if (IsRunMode())
		return false;	// @pvz-online: 闯关不算任何"档案关型"

	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_FINAL_BOSS)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 50;
}

//0x453A00
bool LawnApp::IsChallengeWithoutSeedBank()
{
	return 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS || 
		mGameMode == GameMode::GAMEMODE_UPSELL || 
		mGameMode == GameMode::GAMEMODE_INTRO || 
		IsWhackAZombieLevel() || 
		IsSquirrelLevel() || 
		IsScaryPotterLevel() || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || 
		mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM;
}

bool LawnApp::IsNight()
{
	if (IsIceDemo() || mPlayerInfo == nullptr)
		return false;

	return (mPlayerInfo->mLevel >= 11 && mPlayerInfo->mLevel <= 20) || (mPlayerInfo->mLevel >= 31 && mPlayerInfo->mLevel <= 40) || mPlayerInfo->mLevel == 50;
}

int LawnApp::GetCurrentChallengeIndex()
{
	return (int)mGameMode - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
}

ChallengeDefinition& LawnApp::GetCurrentChallengeDef()
{
	return GetChallengeDefinition(GetCurrentChallengeIndex());
}

PottedPlant* LawnApp::GetPottedPlantByIndex(int thePottedPlantIndex)
{
	TOD_ASSERT(thePottedPlantIndex >= 0 && thePottedPlantIndex < mPlayerInfo->mNumPottedPlants);
	return &mPlayerInfo->mPottedPlant[thePottedPlantIndex];
}

//0x453A50
bool LawnApp::UpdateApp()
{
	if (mCloseRequest)
	{
		Shutdown();
		return false;
	}

	//if (mLoadingThreadCompleted)
	//{
	//	LoadingThreadCompleted();
	//}

	bool updated = SexyAppBase::UpdateApp();

	//if (mLoadingThreadCompleted && !mExitToTop)
	//{
	//	CheckForUpdates();
	//}

	return updated;
}

//0x453A70
void LawnApp::CloseRequestAsync()
{
	mDeferredMessages.clear();
	mExitToTop = true;
	mCloseRequest = true;
}

//0x453A90
SeedType LawnApp::GetAwardSeedForLevel(int theLevel)
{
	int aArea = (theLevel - 1) / LEVELS_PER_AREA + 1;
	int aSub = (theLevel - 1) % LEVELS_PER_AREA + 1;
	int aSeedsHasGot = (aArea - 1) * 8 + aSub;  // 一般来说，每大关可以获得 8 种植物，每小关可以获得 1 种植物
	if (aSub >= 10)
	{
		aSeedsHasGot -= 2;  // 到达第 10 小关时，本大关中有 2 小关的奖励不是新植物
	}
	else if (aSub >= 5)
	{
		aSeedsHasGot -= 1;  // 到达第 5 小关时，本大关中有 1 小关的奖励不是新植物
	}
	if (aSeedsHasGot > 40)
	{
		aSeedsHasGot = 40;
	}
	
	return (SeedType)aSeedsHasGot;
}

//0x453AC0
int LawnApp::GetSeedsAvailable()
{
	int aLevel = mPlayerInfo->GetLevel();
	if (HasFinishedAdventure() || aLevel > 50)
	{
		return 49;
	}

	SeedType aSeedTypeMax = GetAwardSeedForLevel(aLevel);
	return std::min(NUM_SEEDS_IN_CHOOSER, aSeedTypeMax);
}

//0x453B20
// GOTY @Patoke: 0x456FE0
bool LawnApp::HasSeedType(SeedType theSeedType)
{
	if (IsTrialStageLocked() && theSeedType >= SeedType::SEED_JALAPENO)
		return false;

	/*  优化
	if (theSeedType >= SeedType::SEED_TWINSUNFLOWER && theSeedType <= SeedType::SEED_IMITATER)
		return mPlayerInfo->mPurchases[theSeedType - SeedType::SEED_GATLINGPEA];
	*/

	if (theSeedType == SeedType::SEED_TWINSUNFLOWER)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_TWINSUNFLOWER] > 0;
	}
	if (theSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_GLOOMSHROOM] > 0;
	}
	if (theSeedType == SeedType::SEED_CATTAIL)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_CATTAIL] > 0;
	}
	if (theSeedType == SeedType::SEED_WINTERMELON)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_WINTERMELON] > 0;
	}
	if (theSeedType == SeedType::SEED_GOLD_MAGNET)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_GOLD_MAGNET] > 0;
	}
	if (theSeedType == SeedType::SEED_SPIKEROCK)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_SPIKEROCK] > 0;
	}
	if (theSeedType == SeedType::SEED_COBCANNON)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_COBCANNON] > 0;
	}
	if (theSeedType == SeedType::SEED_IMITATER)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_IMITATER] > 0;
	}

	return theSeedType < GetSeedsAvailable();
}

bool LawnApp::SeedTypeAvailable(SeedType theSeedType)
{
	// @pvz-online: 闯关（肉鸽）：能用哪些植物只看这一局的卡池——卡池是三选一攒出来的，
	// 和本机档案解锁到哪儿无关（§5.1 定案）。选卡界面（第 4、5 关卡池 > 8 格时才弹）
	// 里每一处"画不画、点不点"的判定都走这里，一处收口；局中的图鉴也跟着只显示卡池。
	if (IsRunMode())
	{
		return mRunState->HasPlant(theSeedType);
	}

	return (theSeedType == SeedType::SEED_GATLINGPEA && mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_GATLINGPEA]) || HasSeedType(theSeedType);
}

//0x453C30
Reanimation* LawnApp::AddReanimation(float theX, float theY, int theRenderOrder, ReanimationType theReanimationType)
{
	return mEffectSystem->mReanimationHolder->AllocReanimation(theX, theY, theRenderOrder, theReanimationType);
}

//0x453C80
TodParticleSystem* LawnApp::AddTodParticle(float theX, float theY, int theRenderOrder, ParticleEffect theEffect)
{
	return mEffectSystem->mParticleHolder->AllocParticleSystem(theX, theY, theRenderOrder, theEffect);
}

ParticleSystemID LawnApp::ParticleGetID(TodParticleSystem* theParticle)
{
	return (ParticleSystemID)mEffectSystem->mParticleHolder->mParticleSystems.DataArrayGetID(theParticle);
}

ReanimationID LawnApp::ReanimationGetID(Reanimation* theReanimation)
{
	return (ReanimationID)mEffectSystem->mReanimationHolder->mReanimations.DataArrayGetID(theReanimation);
}

TodParticleSystem* LawnApp::ParticleGet(ParticleSystemID theParticleID)
{
	return mEffectSystem->mParticleHolder->mParticleSystems.DataArrayGet((unsigned int)theParticleID);
}

TodParticleSystem* LawnApp::ParticleTryToGet(ParticleSystemID theParticleID)
{
	return mEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet((unsigned int)theParticleID);
}

// GOTY @Patoke: 0x464B0F
Reanimation* LawnApp::ReanimationGet(ReanimationID theReanimationID)
{
	return mEffectSystem->mReanimationHolder->mReanimations.DataArrayGet((unsigned int)theReanimationID);
}

//0x453CB0
Reanimation* LawnApp::ReanimationTryToGet(ReanimationID theReanimationID)
{
	return mEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet((unsigned int)theReanimationID);
}

//0x453CF0
void LawnApp::RemoveReanimation(ReanimationID theReanimationID)
{
	Reanimation* aReanim = ReanimationTryToGet(theReanimationID);
	if (aReanim)
	{
		aReanim->ReanimationDie();
	}
}

void LawnApp::RemoveParticle(ParticleSystemID theParticleID)
{
	TodParticleSystem* aParticle = ParticleTryToGet(theParticleID);
	if (aParticle)
	{
		aParticle->ParticleSystemDie();
	}
}

//0x453D20
bool LawnApp::AdvanceCrazyDaveText()
{
	SexyString aMessageName = StrFormat(_S("[CRAZY_DAVE_%d]"), mCrazyDaveMessageIndex + 1);
	if (!TodStringListExists(aMessageName))
	{
		return false;
	}

	CrazyDaveTalkIndex(mCrazyDaveMessageIndex + 1);
	return true;
}

//0x453DC0
SexyString LawnApp::GetCrazyDaveText(int theMessageIndex)
{
	SexyString aMessage = StrFormat(_S("[CRAZY_DAVE_%d]"), theMessageIndex);
	aMessage = TodReplaceString(aMessage, _S("{PLAYER_NAME}"), mPlayerInfo->mName);
	aMessage = TodReplaceString(aMessage, _S("{MONEY}"), GetMoneyString(mPlayerInfo->mCoins));
	int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);
	aMessage = TodReplaceString(aMessage, _S("{UPGRADE_COST}"), GetMoneyString(aCost));
	return aMessage;
}

//0x454070
bool LawnApp::CanShowAlmanac()
{
	if (IsIceDemo())
		return false;

	if (mPlayerInfo == nullptr)
		return false;

	// @pvz-online: 图鉴直接解锁（用户 2026-10-03 拍板）——不再随档案进度锁
	// （原版条件 HasFinishedAdventure() || mLevel >= 15）。图鉴内部的条目揭示逻辑照旧。
	return true;
}

//0x454090
bool LawnApp::CanShowStore()
{
	if (IsIceDemo())
		return false;

	if (mPlayerInfo == nullptr)
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mHasSeenUpsell || mPlayerInfo->mLevel >= 25;
}

//0x4540C0
bool LawnApp::CanShowZenGarden()
{
	if (mPlayerInfo == nullptr)
		return false;

	if (IsTrialStageLocked())
		return false;

	// @pvz-online: 禅境花园直接解锁（用户 2026-10-03 拍板）——不再随档案进度锁
	// （原版条件 HasFinishedAdventure() || mLevel >= 45）。花园内的购买/种植规则照旧。
	return true;
}

bool LawnApp::CanSpawnYetis()
{
	const ZombieDefinition& aZombieDef = GetZombieDefinition(ZombieType::ZOMBIE_YETI);
	return HasFinishedAdventure() && (mPlayerInfo->mFinishedAdventure >= 2 || mPlayerInfo->mLevel >= aZombieDef.mStartingLevel);
}

//0x454120
bool LawnApp::HasBeatenChallenge(GameMode theGameMode)
{
	if (mPlayerInfo == nullptr)
		return false;

	int aChallengeIndex = theGameMode - GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	TOD_ASSERT(aChallengeIndex >= 0 && aChallengeIndex < NUM_CHALLENGE_MODES);
	if (IsSurvivalNormal(theGameMode))
	{
		return mPlayerInfo->mChallengeRecords[aChallengeIndex] >= SURVIVAL_NORMAL_FLAGS;
	}
	if (IsSurvivalHard(theGameMode))
	{
		return mPlayerInfo->mChallengeRecords[aChallengeIndex] >= SURVIVAL_HARD_FLAGS;
	}
	if (IsSurvivalEndless(theGameMode) || IsEndlessScaryPotter(theGameMode) || IsEndlessIZombie(theGameMode))
	{
		return false;
	}
	return mPlayerInfo->mChallengeRecords[aChallengeIndex] > 0;
}

//0x454170
bool LawnApp::HasFinishedAdventure()
{
	return mPlayerInfo && mPlayerInfo->mFinishedAdventure > 0;
}

//0x454190
bool LawnApp::IsFirstTimeAdventureMode()
{
	// @pvz-online: 闯关模式一律按"老兵"算，和本机档案打到哪儿无关——这样才有：
	// 没有教学提示、没有草地铺设、开局 50 阳光、波次不加 10（见 PickZombieWaves 的闯关分支）；
	// 每位玩家的闯关内容因此完全一致。
	return IsAdventureMode() && !HasFinishedAdventure() && !IsRunMode();
}

//0x4541B0
void LawnApp::CrazyDaveEnter()
{
	TOD_ASSERT(mCrazyDaveState == CRAZY_DAVE_OFF);
	TOD_ASSERT(!ReanimationTryToGet(mCrazyDaveReanimID));

	Reanimation* aCrazyDaveReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
	aCrazyDaveReanim->mIsAttachment = true;
	aCrazyDaveReanim->SetBasePoseFromAnim("anim_idle_handing");
	mCrazyDaveReanimID = ReanimationGetID(aCrazyDaveReanim);
	aCrazyDaveReanim->PlayReanim("anim_enter", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);

	mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_ENTERING;
	mCrazyDaveMessageIndex = -1;
	mCrazyDaveMessageText.clear();
	mCrazyDaveBlinkCounter = RandRangeInt(400, 800);

	if (mGameScene == GameScenes::SCENE_LEVEL_INTRO && IsStormyNightLevel())
	{
		aCrazyDaveReanim->mColorOverride = Color(64, 64, 64);
	}
}

//0x4542F0
void LawnApp::CrazyDaveDie()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim)
	{
		aCrazyDaveReanim->ReanimationDie();

		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
		mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
		mCrazyDaveMessageIndex = -1;
		mCrazyDaveMessageText.clear();

		CrazyDaveStopSound();
	}
}

//0x454350
void LawnApp::CrazyDaveLeave()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim)
	{
		if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
		{
			CrazyDaveDoneHanding();
		}

		aCrazyDaveReanim->PlayReanim("anim_leave", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
		aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);

		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_LEAVING;
		mCrazyDaveMessageIndex = -1;
		mCrazyDaveMessageText.clear();

		CrazyDaveStopSound();
	}
}

//0x454430
void LawnApp::CrazyDaveTalkIndex(int theMessageIndex)
{
	mCrazyDaveMessageIndex = theMessageIndex;
	SexyString aMessageText = GetCrazyDaveText(theMessageIndex);
	CrazyDaveTalkMessage(aMessageText);
}

//0x4544A0
void LawnApp::CrazyDaveDoneHanding()
{
	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);
	ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
	AttachmentDie(aHandTrackInstance->mAttachmentID);

	TodTrace("DoneHanding");
}

//0x454520
void LawnApp::CrazyDaveStopSound()
{
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_EXTRA_LONG);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_CRAZY);
}

//0x454570
void LawnApp::CrazyDaveTalkMessage(const SexyString& theMessage)
{
	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);

	bool doHanding = false;
	if (theMessage.find(_S("{HANDING}")) != SexyString::npos)
	{
		doHanding = true;
	}
	if ((mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING) && !doHanding)
	{
		CrazyDaveDoneHanding();
	}

	bool doSound = true;
	if (theMessage.find(_S("{NO_SOUND}")) != SexyString::npos)
	{
		doSound = false;
	}
	else
	{
		CrazyDaveStopSound();
	}

	int aWordsCount = 0;
	bool isControlWord = false;
	for (size_t i = 0; i < theMessage.size(); i++)
	{
		if (theMessage[i] == _S('{'))
		{
			isControlWord = true;
		}
		else if (theMessage[i] == _S('}'))
		{
			isControlWord = false;
		}
		else if (!isControlWord)
		{
			aWordsCount++;
		}
	}

	aCrazyDaveReanim->SetImageOverride(_S("Dave_mouths"), nullptr);

	if (mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_TALKING || doSound)
	{
		if (doHanding)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			if (doSound)
			{
				if (theMessage.find(_S("{SHORT_SOUND}")) != SexyString::npos)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
				}
				else if (theMessage.find(_S("{SCREAM}")) != SexyString::npos)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM);
				}
				else
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
				}
			}
			
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHAKE}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_crazy", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_CRAZY);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find(_S("{SCREAM}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_smalltalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find(_S("{SCREAM2}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_mediumtalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM_2);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_WALLNUT}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aWallnutReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_WALLNUT);
			aWallnutReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
			TodTrace("Handed");

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachEffect* aAttachEffect = AttachReanim(aHandTrackInstance->mAttachmentID, aWallnutReanim, 100.0f, 393.0f);
			aAttachEffect->mOffset.m00 = 1.2f;
			aAttachEffect->mOffset.m11 = 1.2f;

			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM_2);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_HAMMER}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aHammerReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_HAMMER);
			aHammerReanim->PlayReanim("anim_whack_zombie", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aHammerReanim->mAnimTime = 1.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachEffect* aAttachEffect = AttachReanim(aHandTrackInstance->mAttachmentID, aHammerReanim, 62.0f, 445.0f);
			aAttachEffect->mOffset.m00 = 1.5f;
			aAttachEffect->mOffset.m11 = 1.5f;

			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_FERTILIZER}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aFertilizerReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZENGARDEN_FERTILIZER);
			aFertilizerReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aFertilizerReanim->mAnimRate = 0.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aFertilizerReanim, 102.0f, 412.0f);
			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_TREE_FOOD}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aTreeFoodReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_TREEOFWISDOM_TREEFOOD);
			aTreeFoodReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aTreeFoodReanim->mAnimRate = 0.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aTreeFoodReanim, 102.0f, 412.0f);
			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_MONEYBAG}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aMoneyBagReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZENGARDEN_FERTILIZER);
			aMoneyBagReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aMoneyBagReanim->mAnimRate = 0.0f;
			aMoneyBagReanim->SetImageOverride("bag", IMAGE_MONEYBAG);

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aMoneyBagReanim, 90.0f, 405.0f);
			aCrazyDaveReanim->Update();
			/*
			v16 = Reanimation::GetTrackInstanceByName(v3, "Dave_handinghand");
			theAnimRate = 405.0;
			v17 = 90.0;
			*/
			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else
		{
			if (aWordsCount < 23)
			{
				aCrazyDaveReanim->PlayReanim("anim_smalltalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
			else if (aWordsCount < 52)
			{
				aCrazyDaveReanim->PlayReanim("anim_mediumtalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
			else
			{
				aCrazyDaveReanim->PlayReanim("anim_blahblah", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_EXTRA_LONG);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
		}
	}

	mCrazyDaveMessageText = theMessage;
}

//0x454ED0
void LawnApp::CrazyDaveStopTalking()
{
	bool aDoneHanding = true;
	if (mGameMode == GameMode::GAMEMODE_UPSELL)
	{
		aDoneHanding = false;
	}
	if (aDoneHanding && mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		CrazyDaveDoneHanding();
	}

	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);
	aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);
	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING && !aDoneHanding)
	{
		aCrazyDaveReanim->PlayReanim("anim_idle_handing", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		aCrazyDaveReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
	}

	mCrazyDaveMessageIndex = -1;
	mCrazyDaveMessageText.clear();
	CrazyDaveStopSound();
}

//0x455040
void LawnApp::UpdateCrazyDave()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim == nullptr)
		return;

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_ENTERING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING)
	{
		if (aCrazyDaveReanim->mLoopCount > 0)
		{
			aCrazyDaveReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
		}
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		if (aCrazyDaveReanim->mLoopCount > 0)
		{
			aCrazyDaveReanim->PlayReanim("anim_idle_handing", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
		}
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_LEAVING && aCrazyDaveReanim->mLoopCount > 0)
	{
		CrazyDaveDie();
	}

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_IDLING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		if (mCrazyDaveMessageText.find("{MOUTH_BIG_SMILE}") != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH1);
		}
		else if (mCrazyDaveMessageText.find("{MOUTH_SMALL_SMILE}") != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH5);
		}
		else if (mCrazyDaveMessageText.find("{MOUTH_BIG_OH}") != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH4);
		}
		else if (mCrazyDaveMessageText.find("{MOUTH_SMALL_OH}") != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH6);
		}
	}

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_IDLING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING || 
		mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		mCrazyDaveBlinkCounter--;
		if (mCrazyDaveBlinkCounter <= 0)
		{
			mCrazyDaveBlinkCounter = RandRangeInt(400, 800);
			Reanimation* aBlinkReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
			aBlinkReanim->SetFramesForLayer("anim_blink");
			aBlinkReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD;
			aBlinkReanim->mAnimRate = 15.0f;
			aBlinkReanim->AttachToAnotherReanimation(aCrazyDaveReanim, "Dave_head");
			aBlinkReanim->mColorOverride = aCrazyDaveReanim->mColorOverride;
			aCrazyDaveReanim->AssignRenderGroupToTrack("Dave_eye", RENDER_GROUP_HIDDEN);
			mCrazyDaveBlinkReanimID = ReanimationGetID(aBlinkReanim);
		}
	}

	Reanimation* aBlinkReanim = ReanimationTryToGet(mCrazyDaveBlinkReanimID);
	if (aBlinkReanim && aBlinkReanim->mLoopCount > 0)
	{
		aCrazyDaveReanim->AssignRenderGroupToTrack("Dave_eye", RENDER_GROUP_NORMAL);
		RemoveReanimation(mCrazyDaveBlinkReanimID);
		mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	aCrazyDaveReanim->Update();
}

//0x4552F0
void LawnApp::DrawCrazyDave(Graphics* g)
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim == nullptr)
		return;

	if (mCrazyDaveMessageText.size())
	{
		Image* aBubbleImage = IMAGE_STORE_SPEECHBUBBLE2;
		int aPosX = 285;
		int aPosY = 20;
		if (GetDialog(Dialogs::DIALOG_STORE))
		{
			aBubbleImage = IMAGE_STORE_SPEECHBUBBLE;
			aPosX -= 180;
			aPosY -= 78;
		}
		else if (mGameMode == GameMode::GAMEMODE_UPSELL)
		{
			aPosX += 130;
			aPosY += 70;
		}
		g->DrawImage(aBubbleImage, aPosX, aPosY);

		SexyString aBubbleText = mCrazyDaveMessageText;
		Rect aRect(aPosX + 25, aPosY + 6, 233, 144);
		if (aBubbleText.find(_S("{SHAKE}")) != SexyString::npos)
		{
			aBubbleText = TodReplaceString(aBubbleText, _S("{SHAKE}"), _S(""));
			aRect.mX += rand() % 2;
			aRect.mY += rand() % 2;
		}

		bool clickToContinue = true;
		if (mGameMode == GameMode::GAMEMODE_UPSELL)
		{
			clickToContinue = false;
		}
		else if (aBubbleText.find(_S("{NO_CLICK}")) != SexyString::npos)
		{
			aBubbleText = TodReplaceString(aBubbleText, _S("{NO_CLICK}"), _S(""));
			clickToContinue = false;
		}

		TodDrawStringWrapped(g, aBubbleText, aRect, FONT_BRIANNETOD16, Color::Black, DrawStringJustification::DS_ALIGN_CENTER_VERTICAL_MIDDLE);
		if (clickToContinue)
		{
			TodDrawString(g, _S("click to continue"), aPosX + 139, aPosY + 140, FONT_PICO129, Color::Black, DrawStringJustification::DS_ALIGN_CENTER);
		}
	}

	aCrazyDaveReanim->Draw(g);
}

//0x455670
int LawnApp::GetNumPreloadingTasks()
{
	int aTaskCount = 10;
	if (mPlayerInfo)
	{
		for (SeedType i = SeedType::SEED_PEASHOOTER; i < SeedType::NUM_SEED_TYPES; i = (SeedType)((int)i + 1))
		{
			if (SeedTypeAvailable(i) || HasFinishedAdventure())
			{
				aTaskCount++;
			}
		}

		for (ZombieType i = ZombieType::ZOMBIE_NORMAL; i < ZombieType::NUM_ZOMBIE_TYPES;i = (ZombieType)((int)i + 1))
		{
			if (HasFinishedAdventure() || mPlayerInfo->mLevel >= GetZombieDefinition(i).mStartingLevel)
			{
				if (i != ZombieType::ZOMBIE_BOSS &&
					i != ZombieType::ZOMBIE_CATAPULT &&
					i != ZombieType::ZOMBIE_GARGANTUAR &&
					i != ZombieType::ZOMBIE_DIGGER &&
					i != ZombieType::ZOMBIE_ZAMBONI)
				{
					aTaskCount++;
				}
			}
		}
	}
	return aTaskCount * 68;
}

//0x455720
void LawnApp::PreloadForUser()
{
	int aNumTasks = mNumLoadingThreadTasks + GetNumPreloadingTasks();
	if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
	{
		TodTrace("preload canceled\n");
		mNumLoadingThreadTasks = aNumTasks;
		return;
	}

	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PUFF, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_READYSETPLANT, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_FINAL_WAVE, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SUN, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_TEXT_FADE_ON, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_NEWSPAPER, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SELECTOR_SCREEN, true);
	mCompletedLoadingThreadTasks += 340;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_HAND, true);
	mCompletedLoadingThreadTasks += 68;

	if (mPlayerInfo)
	{
		for (SeedType i = SeedType::SEED_PEASHOOTER; i < SeedType::NUM_SEED_TYPES; i = (SeedType)((int)i + 1))
		{
			if (SeedTypeAvailable(i) || HasFinishedAdventure())
			{
				Plant::PreloadPlantResources(i);
				if (mCompletedLoadingThreadTasks < aNumTasks)
				{
					mCompletedLoadingThreadTasks += 68;
				}

				if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
				{
					TodTrace("preload canceled\n");
					mNumLoadingThreadTasks = aNumTasks;
					return;
				}

				if (mShutdown || mCloseRequest)
				{
					return;
				}
			}
		}

		for (ZombieType i = ZombieType::ZOMBIE_NORMAL; i < ZombieType::NUM_ZOMBIE_TYPES;i = (ZombieType)((int)i + 1))
		{
			if (HasFinishedAdventure() || mPlayerInfo->mLevel >= GetZombieDefinition(i).mStartingLevel)
			{
				continue;
			}
			if (i == ZombieType::ZOMBIE_BOSS || i == ZombieType::ZOMBIE_CATAPULT || i == ZombieType::ZOMBIE_GARGANTUAR ||
				i == ZombieType::ZOMBIE_DIGGER || i == ZombieType::ZOMBIE_ZAMBONI)
			{
				continue;
			}

			Zombie::PreloadZombieResources(i);
			if (mCompletedLoadingThreadTasks < aNumTasks)
			{
				mCompletedLoadingThreadTasks += 68;
			}

			if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
			{
				TodTrace("preload canceled\n");
				mNumLoadingThreadTasks = aNumTasks;
				return;
			}

			if (mShutdown || mCloseRequest)
			{
				return;
			}
		}
	}

	if (mCompletedLoadingThreadTasks != aNumTasks)
	{
		TodTrace("num preload tasks wasn't calculated correctly");
		mCompletedLoadingThreadTasks = aNumTasks;
	}
}

//0x455930
void LawnApp::EnforceCursor()
{
	if (mSEHOccured || !mMouseIn)
	{
		::SetCursor(LoadCursor(NULL, IDC_ARROW));
		return;
	}

	if (mOverrideCursor)
	{
		::SetCursor(mOverrideCursor);
		return;
	}

	switch (mCursorNum)
	{
	case CURSOR_POINTER:
		::SetCursor(LoadCursor(GetModuleHandle(NULL), MAKEINTRESOURCE(IDC_CURSOR1)));
		return;

	case CURSOR_HAND:
		::SetCursor(mHandCursor);
		return;

	case CURSOR_TEXT:
		::SetCursor(LoadCursor(NULL, IDC_IBEAM));
		return;

	case CURSOR_DRAGGING:
		::SetCursor(mDraggingCursor);
		return;

	case CURSOR_CIRCLE_SLASH:
		::SetCursor(LoadCursor(NULL, IDC_NO));
		return;

	case CURSOR_SIZEALL:
		::SetCursor(LoadCursor(NULL, IDC_SIZEALL));
		return;

	case CURSOR_SIZENESW:
		::SetCursor(LoadCursor(NULL, IDC_SIZENESW));
		return;

	case CURSOR_SIZENS:
		::SetCursor(LoadCursor(NULL, IDC_SIZENS));
		return;

	case CURSOR_SIZENWSE:
		::SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
		return;

	case CURSOR_SIZEWE:
		::SetCursor(LoadCursor(NULL, IDC_SIZEWE));
		return;

	case CURSOR_WAIT:
		::SetCursor(LoadCursor(NULL, IDC_WAIT));
		return;

	case CURSOR_CUSTOM:
		::SetCursor(NULL);
		return;

	case CURSOR_NONE:
		::SetCursor(NULL);
		return;

	default:
		::SetCursor(LoadCursor(NULL, IDC_ARROW));
		return;
	}
}

//0x455AA0
SexyString LawnApp::Pluralize(int theCount, const SexyChar* theSingular, const SexyChar* thePlural)
{
	if (theCount == 1)
	{
		return TodReplaceNumberString(theSingular, _S("{COUNT}"), theCount);
	}

	return TodReplaceNumberString(thePlural, _S("{COUNT}"), theCount);
}

//0x455BA0
int LawnApp::GetNumTrophies(ChallengePage thePage)
{
	int aNumTrophies = 0;

	for (int i = 0; i < NUM_CHALLENGE_MODES; i++)
	{
		const ChallengeDefinition& aDef = GetChallengeDefinition(i);
		if (aDef.mPage == thePage && HasBeatenChallenge(aDef.mChallengeMode))
		{
			aNumTrophies++;
		}
	}

	return aNumTrophies;
}

//0x455C20
int LawnApp::TrophiesNeedForGoldSunflower()
{
	return 48 - GetNumTrophies(CHALLENGE_PAGE_SURVIVAL) - GetNumTrophies(CHALLENGE_PAGE_CHALLENGE) - GetNumTrophies(CHALLENGE_PAGE_PUZZLE);
}

//0x455C50
// GOTY @Patoke: 0x459190
bool LawnApp::EarnedGoldTrophy()
{
	return HasFinishedAdventure() && TrophiesNeedForGoldSunflower() <= 0;
}

void LawnApp::FinishZenGardenToturial()
{
	mBoardResult = BoardResult::BOARDRESULT_WON;
	KillBoard();
	PreNewGame(GameMode::GAMEMODE_ADVENTURE, false);
}

//0x455C90
bool LawnApp::IsTrialStageLocked()
{
	if (mDebugTrialLocked)
		return true;

	if (mDRM && mDRM->QueryData())
		return false;

	return mTrialType == TrialType::TRIALTYPE_STAGELOCKED;
}

//0x455CC0
void LawnApp::InitHook()
{
#ifdef _DEBUG
	mDRM = nullptr;
#else
	mDRM = new PopDRMComm();
	mDRM->DoIPC();
	if (sexystricmp(GetString("MarketingMode", _S("")).c_str(), _S("StageLocked")) == 0)
	{
		mTrialType = TrialType::TRIALTYPE_STAGELOCKED;
		mDRM->EnableLocking();
	}
	else
	{
		mTrialType = TrialType::TRIALTYPE_NONE;
	}
#endif
}

//0x455E10
SexyString LawnApp::GetMoneyString(int theAmount)
{
	int aValue = theAmount * 10;
	if (aValue > 999999)
	{
		return StrFormat(_S("$%d,%03d,%03d"), aValue / 1000000, (aValue - aValue / 1000000 * 1000000) / 1000, aValue - aValue / 1000 * 1000);
	}
	else if (aValue > 9999)
	{
		return StrFormat(_S("$%d,%03d"), aValue / 1000, aValue - aValue / 1000 * 1000);
	}
	else
	{
		return StrFormat(_S("$%d"), aValue);
	}
}

//0x455EE0
SexyString LawnGetCurrentLevelName()
{
	if (gLawnApp == nullptr)
	{
		return _S("Before App");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_LOADING)
	{
		return _S("Game Loading");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_MENU)
	{
		return _S("Game Selector");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_AWARD)
	{
		return _S("Award Screen");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_CHALLENGE)
	{
		return _S("Challenge Screen");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_CREDIT)
	{
		return _S("Credits");
	}
	if (gLawnApp->mBoard == nullptr)
	{
		return _S("Not Playing");
	}

	if (gLawnApp->IsFirstTimeAdventureMode())
	{
		return gLawnApp->GetStageString(gLawnApp->mBoard->mLevel);
	}
	if (gLawnApp->IsAdventureMode())
	{
		return StrFormat(_S("F%d"), gLawnApp->GetStageString(gLawnApp->mBoard->mLevel).c_str());
	}

	return gLawnApp->GetCurrentChallengeDef().mChallengeName;
}

//0x456060
bool LawnApp::CanDoPinataMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1] >= 1000;
}

//0x456080
bool LawnApp::CanDoDanceMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1] >= 500;
}

//0x4560A0
bool LawnApp::CanDoDaisyMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1] >= 100;
}

//0x4560C0
void LawnApp::PlaySample(int theSoundNum)
{
	if (!mMuteSoundsForCutscene)
	{
		SexyAppBase::PlaySample(theSoundNum);
	}
}

//0x4560E0
void LawnApp::SwitchScreenMode(bool wantWindowed, bool is3d, bool force)
{
	SexyAppBase::SwitchScreenMode(wantWindowed, is3d, force);

	NewOptionsDialog* aNewOptionsDialog = (NewOptionsDialog*)GetDialog(Dialogs::DIALOG_NEWOPTIONS);
	if (aNewOptionsDialog)
	{
		aNewOptionsDialog->mFullscreenCheckbox->SetChecked(!mIsWindowed);
	}
}

/* #################################################################################################### */
/*
void LawnApp::BetaSubmit(bool theAskForComments)
{

}

void LawnApp::BetaRecordLevelStats()
{

}

void LawnApp::BetaAddFile(std::list<std::string>& theUploadFileList, std::string theFileName, std::string theShortName)
{

}

void LawnApp::TraceLoadGroup(const char* theGroupName, int theGroupTime, int theTotalGroupWeigth, int theTaskWeight)
{

}
*/

/* #################################################################################################### */

void LawnApp::DoHighScoreDialog()
{

}

void LawnApp::DoRegister()
{

}

void LawnApp::DoRegisterError()
{

}

bool LawnApp::CanDoRegisterDialog()
{
	return false;
}

void LawnApp::DoNeedRegisterDialog()
{

}

void LawnApp::FinishModelessDialogs()
{

}

bool LawnApp::NeedRegister()
{
	return false;
}

void LawnApp::UpdateRegisterInfo()
{

}


