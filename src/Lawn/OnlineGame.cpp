#include "Lawn/Run/EndlessSaves.h"
#include "Run/RunSpeciesRuntime.h"
#include "TeamPlay.h"
#include "Run/EndlessLawn.h"
#include "CoopPlay.h"
// Game session flow from pinned PC Build 45 (0e08297), with native ownership/UI adapters.
#include "../LawnApp.h"
#include "Run/RunHistory.h"
#include "Board.h"
#include "Zombie.h"
#include "Cutscene.h"
#include "SeedPacket.h"
#include "LawnCommon.h"
#include "Run/RunState.h"
#include "Online/NetSession.h"
#include "Online/NetworkLog.h"
#include "Widget/OnlineDialog.h"
#include "Widget/ZombieVoteDialog.h"
#include "Widget/OnlineStartDialog.h"
#include "Widget/OnlineReadyDialog.h"
#include "Widget/GameSelector.h"
#include "Widget/SeedChooserScreen.h"
#include "Widget/RunFlowScreen.h"
#include "MessageWidget.h"
#include "Widget/NewOptionsDialog.h"
#include "System/PlayerInfo.h"
#include "System/Music.h"
#include "ModText.h"
#include "../PvzpLib/PvzpDebug.h"
#include "widget/WidgetManager.h"
static const int ONLINE_START_WAIT_TIMEOUT_FRAMES = 600;
static void ShowOnlineNotice(LawnApp* app, const char* title, const char* body)
{
    app->KillDialog(DIALOG_ONLINE_START);
    auto* dialog = new OnlineStartDialog(app, title, body, ModText::Tr("知道了", "OK"), nullptr, OnlineStartDialog::NOTIFY_NONE);
    app->CenterDialog(dialog, dialog->mWidth, dialog->mHeight);
    app->AddDialog(DIALOG_ONLINE_START, dialog);
    dialog->WaitForResult();
}

bool LawnApp::IsOnlineGame()
{
	return mOnlineSession != nullptr && mOnlineSession->IsConnected();
}

bool LawnApp::IsOnlineStartAllowed()
{
	if (!mOnlineSession || !mOnlineSession->IsActive()) return true;
	if (mOnlineSession->GetState() == NetSession::State::DEAD) return !mOnlineSession->IsRelay();

    return mOnlineSession->CanHostStart();
}

bool LawnApp::IsOnlineClient()
{
	return mOnlineSession != nullptr && mOnlineSession->IsConnected()
		&& mOnlineSession->GetRole() == NetSession::Role::CLIENT;
}

bool LawnApp::WillWaitForStartAck()
{
	return mOnlineSession != nullptr
		&& mOnlineSession->IsConnected()
		&& mOnlineSession->GetRole() == NetSession::Role::HOST
		&& (mOnlineSession->HasOtherSeats() || mOnlineSession->HasRoundFlow());
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

#include "Online/StartScene.inl"

void LawnApp::UpdateOnlineStart()
{
	if (!mOnlineSession || mOnlineSession->IsReconnecting()) return;
    if(mOnlineWaitingStartAck && mOnlineSession->GetRole()!=NetSession::Role::HOST) {
        mOnlineWaitingStartAck=false;KillDialog(Dialogs::DIALOG_ONLINE_START);ClearOnlineStartOverride();
    }

	// 主机侧：开局命令已经广播出去了，等队友的 START_ACK。等待期间主菜单留着
	// （GameSelector::Update 会保住它），所以每条出路都得让菜单重新可用：
	// 要么进场，要么把这次开局作废、人还好好待着。
	if (mOnlineWaitingStartAck)
	{
		mOnlineStartWaitFrames++;

		// ① 队友没了：ACK 和掉线挤在同一帧的话，宁可这一次开局作废，
		// 也不能一个人开着关跑下去。MOD_BUILD 39 的例外：**闯关局里主机自己在重连**
		// ——队友们都还在（席位名册服务器保留着），这一局不能当"人没了"作废，全队等
		// 他连回来接着开局；非闯关局（还没进关，在菜单等第一关的 ACK）保持作废：
		// 那条路本来就有 6 秒超时兜底，别把人晾在菜单上干等一个可能回不来的人。
		if (!mOnlineSession->IsConnected()
			&& !(mOnlineSession->IsReconnecting() && IsRunMode()))
		{
			mOnlineWaitingStartAck = false;
			mOnlineSession->AbortFlowStartAttempt();	// 开局作废：中继房间收回大厅，别让它卡在 setup 相
			KillDialog(Dialogs::DIALOG_ONLINE_START);
			ClearOnlineStartOverride();
			ShowGameSelector();
			NetworkLog("online start: teammate left before entering, start dropped");
			return;
		}

		// ①b 房间模式：服务器还连着、人走光了——没有谁能回这条 ACK 了，再等就是死等
		//（闯关的换关等待不设超时，棋盘在时也不挂取消框）。闯关局不能按上面那样"作废
		// 回菜单"（ShowGameSelector 会把这一局的内存态一并丢掉）：撤掉这次等待，下一帧
		// 按单人的路自己进场（棋盘在 → UpdateRunPick 的点名门；不在 → 它那条收口）。
		// 非闯关的等待不用这条：④ 的超时会作废它。
		if (mOnlineSession->IsConnected() && IsRunMode() && !mOnlineSession->HasOtherSeats() && !mOnlineSession->HasRoundFlow())
		{
			mOnlineWaitingStartAck = false;
			mOnlineSession->AbortFlowStartAttempt();	// 房都空了：一样把房间收回大厅
			KillDialog(Dialogs::DIALOG_ONLINE_START);
			ClearOnlineStartOverride();
			NetworkLog("[net] the room emptied while waiting for a start ack - going in solo");
			return;
		}

		// @pvz-online: 菜单上的等待（起第一关）挂一块"等待其他玩家"的看板框；换关的等待
		// 棋盘还在（上一关的草坪当背景，见下面那句 advice），不叠框。每帧自检：框不在了
		// 就再摆一张——和 RunPickDialog 同一条纪律，被谁误关都不会卡住等待。
		if (mBoard == nullptr && GetDialog(Dialogs::DIALOG_ONLINE_START) == nullptr)
		{
			OnlineStartDialog* aWaitDialog = new OnlineStartDialog(this,
				ModText::Tr("等待其他玩家", "Waiting for players"),
				ModText::Tr("邀请已发出，等待确认…", "Invites sent, waiting for confirmation..."),
				ModText::Tr("取消", "Cancel"), nullptr,
				OnlineStartDialog::NOTIFY_WAIT_CANCEL);
			CenterDialog(aWaitDialog, aWaitDialog->mWidth, aWaitDialog->mHeight);
			AddDialog(Dialogs::DIALOG_ONLINE_START, aWaitDialog);
		}

        // Mobile: keep the lawn unobstructed while START acknowledgements arrive.

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
			// 拒绝=这次开局作废：把中继房间收回大厅，
			// 已点「加入」进场的队友也一并拉回来
			mOnlineSession->AbortFlowStartAttempt();
			KillDialog(Dialogs::DIALOG_ONLINE_START);
			ClearOnlineStartOverride();
			ShowGameSelector();
			NetworkLog("online start: a teammate turned it down");
			OnlineStartDialog* aDialog = new OnlineStartDialog(this,
				ModText::Tr("队友暂不加入", "Teammate declined"),
				ModText::Tr("等队友准备好后，再开始游戏。", "Start again when your teammate is ready."),
				ModText::Tr("知道了", "OK"), nullptr,
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
			NetworkLog("online start: the whole team is in, entering the level");
			KillDialog(Dialogs::DIALOG_ONLINE);
			KillGameSelector();
			// @pvz-online: 无尽局原地续关（无尽续草坪批）：全队在等这条 ACK 的换关里，
			// 本机的草坪是"刚打赢的那块"——留着当下一关的地，别建新场（房里没别人时
			// 走的是 EnterRunLevel 的收口，那儿挂的是同一句判断）。
			EndlessLawn::Enter(this);
			// @pvz-online: 联机闯关（R6）：全队都回话了才算人齐——广播放行，各席位
			// 在自己的草坪上开始做三选一（新建的草坪上面那句 NewGame 已经建好，无尽局
			// 留下的草坪由 TryKeepEndlessLawn 压住开场，欠下的选项屏都由 UpdateRunPick
			// 摆上去）。单关局没有选项要等，不发。
			if (IsRunMode()) mOnlineSession->SendRunGo();
			return;
		}

		// ④ 等太久了：队友可能卡住了、或者这条命令根本没送到。作废，菜单留给玩家重试。
		// 闯关局不走这条（见上面常量那两句注释），所以那一头的等待只认掉线。
		if (!IsRunMode() && mOnlineStartWaitFrames > ONLINE_START_WAIT_TIMEOUT_FRAMES)
		{
			mOnlineWaitingStartAck = false;
			mOnlineSession->AbortFlowStartAttempt();	// 超时同拒绝：房间收回大厅，开局键随时能再按
			KillDialog(Dialogs::DIALOG_ONLINE_START);
			ClearOnlineStartOverride();
			ShowGameSelector();
			NetworkLog("online start: no answer from the teammate in time, start dropped");
			OnlineStartDialog* aDialog = new OnlineStartDialog(this,
				ModText::Tr("没有等到回应", "No answer"),
				ModText::Tr("等队友准备好后，再开始游戏。", "Start again when your teammate is ready."),
				ModText::Tr("知道了", "OK"), nullptr,
				OnlineStartDialog::NOTIFY_NONE);
			CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
			AddDialog(Dialogs::DIALOG_ONLINE_START, aDialog);
			aDialog->WaitForResult();
		}
		return;
	}

	// @pvz-online 2026-10-25：孤儿 setup 相自救。放弃开局的五条路里有一条是断线——
	// 那一下「回菜单」注定发不出去；重连归队后房间还停在 setup 相，而中继对 setup 相
	// 没有服务端自救，开始键从此不亮。人已回到菜单、手上又没有任何在途开局时，就认作
	// 这种孤儿，每 200 帧重发一次 op5 把房间收回大厅（点齐 ACK 进场的那扇窗有棋盘
	// 挡着，进不来这条）。发得冗余无害：房间已在大厅时 op5 是空操作。
	if (mOnlineSession->IsConnected() && !mOnlineSession->IsReconnecting() &&
		mOnlineSession->GetRole() == NetSession::Role::HOST &&
		mGameScene == GameScenes::SCENE_MENU && mBoard == nullptr &&
		mOnlineSession->IsOrphanFlowSetup())
	{
		if (++mOrphanSetupTicks >= 200)
		{
			mOrphanSetupTicks = 0;
			mOnlineSession->RecoverOrphanFlowSetup();
		}
	}
	else
	{
		mOrphanSetupTicks = 0;
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
			NetworkLog("[run] a pulled-in teammate answered - releasing their picks");
			mOnlineSession->SendRunGo();
		}
		else
		{
			NetworkLog("[net] dropped a start ack nobody was waiting for (accepted=%d)", (int)aStaleAck);
		}
	}

	// @pvz-online: 联机闯关（R6）客户端侧：主机放行了吗（RUN_GO）。只认"命令已经收下、
	// 正寄存着"的那一次；没人等着的放行丢掉（比如自己刚回绝了这次开局）。
	if (mOnlineSession->TakeRunGo())
	{
		if (mOnlineRunStartHeld)
		{
			NetworkLog("[run] the host says go - the new lawn is clear to pick");
			mOnlineRunGo = true;
		}
		else
		{
			NetworkLog("[run] dropped a run-go nobody was waiting for");
		}
	}

	// 客户端侧：主机开局了，跟着开自己那块棋盘。
	NetProto::MsgStartLevel aStart;

	// @pvz-online 2026-10-25：房主放弃了那条开局（中继已把房间收回大厅）——还挂着的
	// "是否加入"当场收掉；不收的话，晚一步点"加入"只会一个人进场。闯关命令在下面
	// 那条超时里被特意放过（补发追赶可能要几十屏），这条是它唯一的收口。
	if (mOnlineStartPromptActive && mOnlineSession->StartAttemptRolledBack())
	{
		NetworkLog("[net] the start attempt was rolled back - the invite is closed");
		DismissOnlineStartPrompt(false);
	}

	// @pvz-online: 询问框挂着的这段时间也在计时：非闯关局给它和主机那把尺一样长的时间
	// （同一个常量），到点两边一起收——主机那边超时作废，这边把框撤掉顺手回一句"不去"。
	// 不撤的话，晚点的"确认"会让本机一个人进场（主机早就不等了），那块草坪谁也结束不了。
	// 闯关局和主机一样不设时限：那一头等多久都算正常（队友可能在补发追赶）。
	if (mOnlineStartPromptActive)
	{
		mOnlineStartPromptFrames++;
		if (!mOnlineStartPromptMsg.mIsRun && mOnlineStartPromptFrames > ONLINE_START_WAIT_TIMEOUT_FRAMES)
		{
			NetworkLog("[net] the start invite timed out unanswered, it is dropped");
			DismissOnlineStartPrompt(true);
		}
	}

    const bool preparationReplay=mOnlineSession->IsPendingPreparationReplay();
	if (!mOnlineSession->TakePendingStartLevel(aStart)) return;
    // A requeued START can repair authority/ACK state without replacing an
    // already accepted lawn and its personal choices. Real retries/new epochs
    // and waiting seats still pass through the normal start transaction.
    if(preparationReplay && mBoard && IsRunMode() && aStart.mIsRun &&
        mBoardResult!=BoardResult::BOARDRESULT_LOST && mGameScene==GameScenes::SCENE_LEVEL_INTRO &&
        (mRunIntroHeld || mOnlineSeedsHeld || mSeedChooserScreen) &&
        mRunState->mRunSeed==aStart.mRunSeed && mRunState->mMode==aStart.mRunMode &&
        mRunState->mDiff==aStart.mRunDiff && mRunState->mLevelIndex==aStart.mRunLevelIndex &&
        mRunState->mEndlessScene==aStart.mRunEndlessScene && mBoard->mLevel==int(aStart.mLevel) &&
        mRunState->GetLevelSeed()==aStart.mLevelSeed &&
        (aStart.mRunMode!=RunState::RUN_MODE_ENDLESS ||
         mRunState->mAdmission.run==mOnlineSession->mLastEndlessStartContext.run)) {
        mOnlineSession->FinishRoundRejoin();mOnlineSession->SendStartAck();return;
    }
    if(mOnlineSession->NeedsRoundRejoin() && !mOnlineSession->IsWaitingForNextRound(mOnlineSession->GetLocalSeat())) {
        KillBoard();mGameScene=GameScenes::SCENE_MENU;
        mOnlineSession->FinishRoundRejoin();
        mOnlineRunStartHeld=false;mOnlineRunGo=false;mRunIntroHeld=false;
    }

    if (IsOnlineStartLobby())
    {
        ShowOnlineStartPrompt(aStart);
        return;
    }

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
			NetworkLog("run start refused: scene %d, no run in progress", (int)mGameScene);
			mOnlineSession->SendStartAck(false);
			return;
		}

		// Existing run lawns advance automatically; menu/garden invitations were handled above.

		NetworkLog("[run] the host calls us into level index %u (run seed %d)",
			(unsigned)aStart.mRunLevelIndex, (int)aStart.mRunSeed);
		if(!AlignRunToHost((int)aStart.mRunSeed, (int)aStart.mRunLevelIndex, (int)aStart.mRunMode, (int)aStart.mRunDiff,
			(int)aStart.mRunScale, (int)aStart.mRunTempo, (int)aStart.mRunZombotany,
			(int)aStart.mRunEndlessScene, (int)aStart.mRunBossFlag, (int)aStart.mRunBuffMult,
			(int)aStart.mRunLadder, (int)aStart.mRunZombieBuff, aStart.mZombieStacks,aStart.mRunSpeedTier, aStart.mRunFirstWave,aStart.mRunEndlessOptions, aStart.mRunCrowdRule)){mOnlineSession->SendStartAck(false);mOnlineSession->PostNotice(ModText::Tr("接入失败，请检查空间后重试。","Could not join. Check storage and retry."));return;}
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
		NetworkLog("[net] the host asked for a retry - clearing the lost level");
		KillDialog(Dialogs::DIALOG_GAME_OVER);
		KillBoard();
	}
	else if (mGameScene != GameScenes::SCENE_MENU)
	{
		// 我在关卡里（比如上一局的退关还没走到这儿），这条开局命令接不了。
		// 不能装没看见：主机在那边等 ACK，一直等不到就只能超时作废。回一句"现在不行"，
		// 主机当场就能说清楚是队友忙，而不是干等六秒。
		NetworkLog("online start refused: scene %d is not the menu", (int)mGameScene);
		mOnlineSession->SendStartAck(false);
		return;
	}

	// @pvz-online: 开局流程改成"房主开始 → 其他人确认"：菜单上的人先看询问框，点了"加入"
	// 才走下面那条进场路（见 OnlineStartPromptAnswer → EnterOnlineStart）。走到这儿的
	// 只剩一种客人：停在吃脑子残局上、主机点了整队重来的人——那个不问，重开是整队的决定。

	EnterOnlineStart(aStart);
}

void LawnApp::OnlineStartWaitCancelled()
{
	if (!mOnlineWaitingStartAck) return;	// 早撤了（已经进场 / 这次已经作废）：这一下不算数

	mOnlineWaitingStartAck = false;
	mOnlineSession->AbortFlowStartAttempt();	// 玩家自己取消：同一条收法，房间收回大厅
	KillDialog(Dialogs::DIALOG_ONLINE_START);
	ClearOnlineStartOverride();
	ShowGameSelector();
	NetworkLog("online start: the host cancelled the wait");
}

void LawnApp::OnlineReconnectCancelled()
{
	if (mOnlineSession == nullptr) return;
	mOnlineSession->AbortReconnect();
}

void LawnApp::EnterOnlineStart(const NetProto::MsgStartLevel& theMsg)
{
    LeaveLocalSceneForOnlineStart();
	KillDialog(Dialogs::DIALOG_ONLINE);
	KillGameSelector();
	SetOnlineStartOverride((int)theMsg.mLevel, (int)theMsg.mLevelSeed);
	mGameMode = (GameMode)theMsg.mGameMode;
	NetworkLog("online start: mode %d level %u seed %d",
		(int)mGameMode, (unsigned)theMsg.mLevel, (int)theMsg.mLevelSeed);
	// ACK 就是"我进关了"这句话，先把它发出去，主机才会跟着进
	mOnlineSession->SendStartAck();
	NewGame();
}

void LawnApp::ShowOnlineStartPrompt(const NetProto::MsgStartLevel& theMsg)
{
	mOnlineStartPromptMsg = theMsg;
	mOnlineStartPromptFrames = 0;
	if (mOnlineStartPromptActive) return;   // 框已经挂着（主机又发了一条）：换个记号就行

	mOnlineStartPromptActive = true;
	NetworkLog("[net] the host starts a level - the invite is on screen");
	if (mGameSelector) mGameSelector->SetMenuButtonsDisabled(true);

	OnlineStartDialog* aDialog = new OnlineStartDialog(this,
		ModText::Tr("房主开始了游戏", "The host started a game"),
		ModText::Tr("是否加入？", "Join?"),
		ModText::Tr("加入", "Join"), ModText::Tr("暂不", "Not now"),
		OnlineStartDialog::NOTIFY_INVITE_ANSWER);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_ONLINE_START, aDialog);
}

void LawnApp::OnlineStartPromptAnswer(bool theAccepted)
{
	if (!mOnlineStartPromptActive) return;
	mOnlineStartPromptActive = false;
	KillDialog(Dialogs::DIALOG_ONLINE_START);
	if (mGameSelector) mGameSelector->SetMenuButtonsDisabled(false);

	if (!theAccepted)
	{
		NetworkLog("[net] the start invite was turned down");
		if (mOnlineSession) mOnlineSession->SendStartAck(false);
		return;
	}

	if (mOnlineSession == nullptr) return;

	LeaveLocalSceneForOnlineStart();
	NetProto::MsgStartLevel aMsg = mOnlineStartPromptMsg;
	if (aMsg.mIsRun)
	{
		// 闯关：对齐进度 + 命令寄存 + 马上回 ACK（"我这就进场"），草坪与选项由
		// 主机的 RUN_GO 放行（见 UpdateOnlineStart / UpdateRunPick）
		NetworkLog("[run] the host calls us into level index %u (run seed %d, answered)",
			(unsigned)aMsg.mRunLevelIndex, (int)aMsg.mRunSeed);
		if(!AlignRunToHost((int)aMsg.mRunSeed, (int)aMsg.mRunLevelIndex, (int)aMsg.mRunMode, (int)aMsg.mRunDiff,
			(int)aMsg.mRunScale, (int)aMsg.mRunTempo, (int)aMsg.mRunZombotany,
			(int)aMsg.mRunEndlessScene, (int)aMsg.mRunBossFlag, (int)aMsg.mRunBuffMult,
			(int)aMsg.mRunLadder, (int)aMsg.mRunZombieBuff, aMsg.mZombieStacks,aMsg.mRunSpeedTier, aMsg.mRunFirstWave,aMsg.mRunEndlessOptions, aMsg.mRunCrowdRule)){mOnlineSession->SendStartAck(false);mOnlineSession->PostNotice(ModText::Tr("接入失败，请检查空间后重试。","Could not join. Check storage and retry."));return;}
		mOnlineRunStartHeld = true;
		mOnlineRunGo = false;
		mOnlineSession->SendStartAck();
		return;
	}

	EnterOnlineStart(aMsg);
}

void LawnApp::DismissOnlineStartPrompt(bool theSendAck)
{
	if (!mOnlineStartPromptActive) return;
	mOnlineStartPromptActive = false;
	KillDialog(Dialogs::DIALOG_ONLINE_START);
	if (mGameSelector) mGameSelector->SetMenuButtonsDisabled(false);
	if (theSendAck && mOnlineSession) mOnlineSession->SendStartAck(false);
}

static void EnsureSeedsWaitDialog(LawnApp* theApp)
{
    // @pvz-online: the seat page owns the modal surface until the player returns to waiting.
    if(theApp->GetDialog(Dialogs::DIALOG_ONLINE_SWAP))return;
    if (theApp->GetDialog(Dialogs::DIALOG_ONLINE_START) != nullptr) return;
    // @pvz-online: compact live roster; the existing UpdateOnlineSeeds owns release.
    theApp->AddDialog(Dialogs::DIALOG_ONLINE_START, new OnlineReadyDialog(theApp));
}

static void EnsureOnlineReconnectDialog(LawnApp* theApp)
{
    if(theApp->GetDialog(Dialogs::DIALOG_ONLINE_RECONNECT))return;
    // @pvz-online: display live reconnect progress without rebuilding or enlarging the dialog.
    theApp->AddDialog(Dialogs::DIALOG_ONLINE_RECONNECT,new OnlineReconnectDialog(theApp));
}

bool LawnApp::TryHoldSeedChooserForTeammates()
{
    if (IsLocalOnlyMode(mGameMode)) return false;
	if (!IsOnlineGame()) return false;			// 单机 / 掉了线：原版节奏，不等

	if (mOnlineSeedsHeld) return true;			// 已经拦下过一次：继续等（放行走 UpdateOnlineSeeds）

    if(mRunState && mRunState->IsNewEndless() && mRunState->mLevelIndex>0 && mOnlineSession->SupportsAdmission() &&
        mOnlineSession->IsPreparationActive() && mOnlineSession->GetPreparationPhase()!=PreparationWire::GO) {
        mOnlineSeedsHeld=true;EnsureSeedsWaitDialog(this);return true;
    }
	mOnlineSession->SendSeedsReady(true);
	// 房里就我一个：没有人可等，原版节奏直接开打（拦下来等的是不存在的队友）
	if (!mOnlineSession->HasOtherSeats() && !mOnlineSession->IsPreparationActive()) return false;
	if (mOnlineSession->IsPeerSeedsReady() && FinalizePreparationSeats())
	{
		NetworkLog("[net] the teammate picked their plants too - starting right away");
		return false;
	}

	mOnlineSeedsHeld = true;
	NetworkLog("[net] plants picked - holding the start until the whole team is ready");
	EnsureSeedsWaitDialog(this);
	return true;
}

void LawnApp::ToggleOnlineSeedsReady()
{
    if (!mOnlineSeedsHeld || !IsOnlineGame() || !mBoard) return;
    const bool ready=mOnlineSession->IsSeatSeedsReady(mOnlineSession->GetLocalSeat());
    if(ready) {
        // The waiting dialog restores the existing chooser only after authority confirms.
        mOnlineSession->RequestSeedsReadyCancel();
        return;
    }
    if(mOnlineSession->CanReadyPreparation())mOnlineSession->SendSeedsReady(true);
}

void LawnApp::UpdateOnlineSeeds()
{
    if (mBoard && IsLocalOnlyMode(mGameMode)) return; // @pvz-online: local garden/tree only.
    if(mCoopPlay && mCoopPlay->Blocking())return;
	if (!mOnlineSeedsHeld) return;
    if(AdmissionBuilding())return;

	// @pvz-online MOD_BUILD 39：重连期断的是我自己的线，不是"队友没了"——放行条件
	// （IsOnlineGame）在这期间天然为假，不加这条约束会把等待门当成"等的人不会来了"
	// 误放行（选卡关屏当场开打，"都选好才开打"就废了）。等待框先收掉（免得和重连框
	// 叠着摆），门保持堵着；归队后每帧自检会把框再摆回来（EnsureSeedsWaitDialog）。
	if (mOnlineSession->IsReconnecting())
	{
		KillDialog(Dialogs::DIALOG_ONLINE_START);
		return;
	}

    if(IsOnlineGame()) {
        const auto cancel=mOnlineSession->GetReadyCancelState();
        if(cancel==NetSession::ReadyCancelState::PENDING || cancel==NetSession::ReadyCancelState::CONFIRMED) {
            EnsureSeedsWaitDialog(this);return;
        }
    }

    // @pvz-online: do not release any lawn until every player has applied and saved final seat rules.
    if(IsOnlineGame() && mOnlineSession->IsPreparationActive() &&
        mOnlineSession->GetPreparationPhase()==PreparationWire::SEAL) {
        if(FinalizePreparationSeats())mOnlineSession->ConfirmPreparationConfiguration();
        EnsureSeedsWaitDialog(this);return;
    }
    if (IsOnlineGame() && !mOnlineSession->IsSeatSeedsReady(mOnlineSession->GetLocalSeat())) {
        EnsureSeedsWaitDialog(this);
        return;
    }
	bool aConnected = IsOnlineGame();
	bool aSomebodyToWait = aConnected && mOnlineSession->HasOtherSeats()
		&& !mOnlineSession->IsPeerSeedsReady();
	if (aSomebodyToWait)
	{
		EnsureSeedsWaitDialog(this);			// 框被误关了再摆一张
		return;
	}

	if (!aConnected)
	{
		NetworkLog("[net] the teammate is gone - the start is no longer held");
	}
	else if (!mOnlineSession->HasOtherSeats())
	{
		NetworkLog("[net] the room is empty - nobody left to wait for, the start is no longer held");
	}
	else
	{
		NetworkLog("[net] the whole team is ready - here we go");
	}
	if(!FinalizePreparationSeats())return;
    KillDialog(Dialogs::DIALOG_ONLINE_SWAP);
	KillDialog(Dialogs::DIALOG_ONLINE_START);
	mOnlineSeedsHeld = false;
	// @pvz-online: 两道门共用这一个开合（R7）。卡池>10 拦的是选卡界面的关屏（Let's Rock），
	// 放行 = 再走一遍关屏（这一遍门会放行）。卡池≤10 的关卡没有选卡界面（"选卡"是草坪上
	// 的三选一屏，门在 UpdateRunPick 的 ①），放行 = 解冻 + 开场——和那道门口的收口对上。
	if (mBoard != nullptr && mBoard->ChooseSeedsOnCurrentLevel() && mSeedChooserScreen != nullptr)
	{
		mSeedChooserScreen->CloseSeedChooser();
	}
	else if (mBoard != nullptr && mRunIntroHeld)
	{
		// @pvz-online: 放开走统一收口（无尽局在这儿就地续排下一关的波次；见函数注释）。
		ReleaseRunIntro();
	}
}

void LawnApp::UpdateOnlineRelay()
{
    if (mBoard && IsLocalOnlyMode(mGameMode)) return; // @pvz-online: local garden/tree only.
	if (!mOnlineSession) return;

	NetProto::MsgEscapedZombie aMsg;
	while (mOnlineSession->PeekPendingEscapedZombie(aMsg))
	{
		if (mBoard == nullptr)
		{
			mOnlineSession->TakePendingEscapedZombie(aMsg);
			NetworkLog("[net] dropped a relayed zombie: no board to put it on (row %u type %u)",
				(unsigned)aMsg.mRow, (unsigned)aMsg.mZombieType);
			continue;
		}

        // Match AddRelayedZombie's content checks before distinguishing invalid data
        // from temporary pool pressure. Invalid packets must not block the FIFO.
        const auto type=static_cast<ZombieType>(aMsg.mZombieType);
        const bool boss=(aMsg.mFlags & NetProto::ESCAPED_FLAG_RUN_BOSS)!=0;
        if(aMsg.mRow>=MAX_GRID_SIZE_Y || type<0 || type>=ZombieType::NUM_ZOMBIE_TYPES ||
            aMsg.mBodyHealth<=0 || (aMsg.mFlags & ~NetProto::ESCAPED_FLAG_RUN_BOSS) ||
            (boss && type!=ZombieType::ZOMBIE_REDEYE_GARGANTUAR &&
                type!=ZombieType::ZOMBIE_FOOTBALL && type!=ZombieType::ZOMBIE_ZAMBONI)) {
            mOnlineSession->TakePendingEscapedZombie(aMsg);
            NetworkLog("[net] rejected invalid relayed zombie: row %u type %u hp %d",
                (unsigned)aMsg.mRow,(unsigned)aMsg.mZombieType,(int)aMsg.mBodyHealth);
            continue;
        }

		Zombie* aZombie = mBoard->AddRelayedZombie((int)aMsg.mRow, (ZombieType)aMsg.mZombieType,
			(int)aMsg.mBodyHealth, (int)aMsg.mHelmHealth, (int)aMsg.mShieldHealth, (int)aMsg.mFlyingHealth,
            aMsg.mFlags, aMsg.mBodyMaxHealth, aMsg.mHelmMaxHealth, aMsg.mShieldMaxHealth, aMsg.mFlyingMaxHealth, (int)aMsg.mSrcSeat);
        if (aZombie == nullptr)break; // Leave the head queued; never lose a transferred zombie.
        RunSpeciesRuntime::ImportRelay(aZombie,aMsg);
        mOnlineSession->TakePendingEscapedZombie(aMsg); // Commit only after successful spawn.

		NetworkLog("[net] relayed zombie is on the board: row %u type %u hp %d/%d/%d/%d",
			(unsigned)aMsg.mRow, (unsigned)aMsg.mZombieType,
			(int)aMsg.mBodyHealth, (int)aMsg.mHelmHealth,
			(int)aMsg.mShieldHealth, (int)aMsg.mFlyingHealth);
	}

	// @pvz-online: 发阳光（MOD_BUILD 38）——队友发来的到账额直接入账（封顶 9990）+
	// 系统横幅；棋盘不在（换关空档）就丢弃，不跨关补账。
	NetProto::MsgSendSun aSunMsg;
	while (mOnlineSession->TakePendingSunGift(aSunMsg))
	{
		if (mBoard == nullptr)
		{
			NetworkLog("[net] dropped a sun gift: no board (+%u from seat %u)",
				(unsigned)aSunMsg.mAmount, (unsigned)aSunMsg.mSrcSeat);
			continue;
		}
		mBoard->GiveSunGift((int)aSunMsg.mAmount, aSunMsg.mSrcSeat);
	}
}

void LawnApp::UpdateOnlineLevelExit()
{
	if (!mOnlineSession) return;

	NetProto::MsgLevelExit aMsg;
	if (!mOnlineSession->TakePendingLevelExit(aMsg)) return;
    if (mBoard && IsLocalOnlyMode(mGameMode)) return; // @pvz-online: consume, do not eject local garden.

	if (mBoard != nullptr)
	{
		NetworkLog("[net] a teammate left the level - going back to the main menu too");
		// 复用 DoBackToMain 的五步（停音乐/写配置/关暂停框/拆棋盘/回菜单）；
		// false 是因为"我要退"这句话对面已经先说了，不用回话。
		DoBackToMain(false);
		ShowOnlineNotice(this, ModText::Tr("队友离开了关卡", "Teammate left"), ModText::Tr("已回到主菜单，连接仍然保留。", "Back to the main menu. The connection is kept."));
		return;
	}

	// 已经在菜单上（比如刚退完，或还没进关）：只写一句即时说明，界面不动。
	// （即时说明显示在主菜单小状态条/联机面板上，语言批：跟着 UI 语言走。）
	NetworkLog("[net] a teammate left the level (already in the menu)");
	mOnlineSession->PostNotice(ModText::Tr("队友离开了关卡。", "A teammate left the level."));
}

void LawnApp::UpdateOnlinePause()
{
    if (mBoard && IsLocalOnlyMode(mGameMode)) return; // @pvz-online: local garden/tree only.
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
		// A waiting spectator's local menu must not pause the fighting team.
        if (!mOnlineSession->IsWaitingForNextRound(mOnlineSession->GetLocalSeat()) && !mOnlineSession->NeedsRoundRejoin())
            mOnlineSession->SendPauseState(aNowOpen);
	}

	// ③ 队友掉线了：他那张"替队友弹的"暂停菜单没人能解（玩家自己没按过），收掉；
	// 玩家自己按出来的暂停菜单不动——那是他的操作，掉不掉线都该留着。
	if (mPauseMenuFromPeer && !mOnlineSession->IsConnected()
		&& GetDialog(Dialogs::DIALOG_NEWOPTIONS) != nullptr)
	{
		NetworkLog("[net] the teammate vanished while the pause menu was theirs - closing it");
		KillNewOptionsDialog();
		mPauseMenuWasOpen = false;
		mPauseMenuFromPeer = false;
	}
}

void LawnApp::UpdateOnlineEvents()
{
	if (!mOnlineSession) return;

	// @pvz-online MOD_BUILD 39：重连框每帧自检（摆框、刷新"第 n 次/剩 N 秒"；见
	// EnsureOnlineReconnectDialog）。放事件循环前面：进重连的当帧这张框就得到位。
	if (mOnlineSession->IsReconnecting())
		EnsureOnlineReconnectDialog(this);

	NetSession::Event anEvent;
	while (mOnlineSession->PollEvent(anEvent))
	{
		switch (anEvent.mType)
		{
		case NetSession::EventType::CONNECTED:
            // A fresh join during battle waits for the next stage, without replaying waves.
            if(IsRunMode() && mBoard && mGameScene==GameScenes::SCENE_PLAYING &&
                !mRunIntroHeld && !mOnlineRunStartHeld && !mOnlineWaitingStartAck &&
                mRunState->GetLevel()==mBoard->mLevel &&
                mOnlineSession->WaitForNextRound(mOnlineSession->GetPeerSeat()))break;

			NetworkLog("[net] the teammate is here - you can pick a level now");
			// @pvz-online: 联机闯关（R5）：本机正跑着一局、队友才连进来——立刻把他拉进
			// 当前这一关（他没检查点/对不上就走补发追赶）。不拉的话，这一关的"全队判胜"
			// 永远凑不齐：一个连上了却没棋盘的席位不会报"我清完了"。
			if (IsRunMode() && mBoard != nullptr
				&& mOnlineSession->GetRole() == NetSession::Role::HOST)
			{
				NetworkLog("[run] a teammate joined mid-level - pulling them into level %d (index %d)",
					mRunState->GetLevel(), mRunState->mLevelIndex);
				// 拉人 = 广播：第 7 参显式 SEAT_UNSET（别再让 mMode 挤上 theTargetSeat——
				// 那是 M4-b 加 mode 字节时留下的传参错位，普通/快速档会把命令发给错误席位）。
				mOnlineSession->SendStartLevel((uint8_t)GameMode::GAMEMODE_ADVENTURE,
					(uint32_t)mRunState->GetLevel(), mRunState->GetLevelSeed(),
					true, mRunState->mRunSeed, (uint16_t)mRunState->mLevelIndex,
					NetProto::SEAT_UNSET, (uint8_t)mRunState->mMode, (uint8_t)mRunState->mDiff,
					(uint8_t)mRunState->mScale, (uint8_t)mRunState->mTempo, (uint8_t)mRunState->mZombotany,
					(uint8_t)mRunState->mEndlessScene, (uint8_t)mRunState->mBossFlag,
					(uint8_t)mRunState->mBuffMult, (uint8_t)mRunState->mLadder,
					(uint8_t)mRunState->mZombieBuffMode, mRunState->mZombieBuffStacks,(uint8_t)mRunState->mSpeedTier, (uint8_t)mRunState->mFirstWave, (uint8_t)mRunState->mEndlessOptions, (uint8_t)mRunState->mCrowdRule);
			}
			break;

		case NetSession::EventType::DISCONNECTED:
			NetworkLog("[net] connection lost: %s", mOnlineSession->GetDiagnosticStatusText().c_str());
			// @pvz-online MOD_BUILD 39：重连框（在手的话）先收——重连失败/被取消都会走到
			// 这条收摊老路上来，框不能留在屏幕上盖着后面的"连接断开"提示。
			KillDialog(Dialogs::DIALOG_ONLINE_RECONNECT);
			// 询问框挂着的客户端（人还在主菜单上）：对面没了，这张框也就没有意义了。
			// 不回 ACK——会话都没了。主机那边等待中的看板框由 UpdateOnlineStart 自己收
			// （它每帧查 IsConnected）。
			DismissOnlineStartPrompt(false);
            if(mOnlineSession->IsRelay() && mBoard) {
                DoOnlineDialog();
                break;
            }
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
				// @pvz-online 语言批：中文档死因也是中文，位图字体的 LawnMessageBox 画不了
				// 汉字（会成一串方块）——换成联机自家的中文框，阻塞口径与消息框一致。
				if (ModText::IsChinese())
				{
					OnlineStartDialog* aDialog = new OnlineStartDialog(this,
						ModText::Tr("连接断开", "Disconnected"),
						(aReason + ModText::Tr("\n回到主菜单。", "\nBack to the main menu.")).c_str(),
						ModText::Tr("知道了", "OK"), nullptr, OnlineStartDialog::NOTIFY_NONE);
					CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
					AddDialog(Dialogs::DIALOG_ONLINE_START, aDialog);
					aDialog->WaitForResult();
				}
				else
				{
					LawnMessageBox(Dialogs::DIALOG_MESSAGE, "Disconnected",
						(aReason + "\nBack to the main menu.").c_str(), "OK", "", Dialog::BUTTONS_FOOTER);
				}
			}
			break;

		case NetSession::EventType::PEER_JOINED:
            // A fresh join during battle waits for the next stage, without replaying waves.
            if(IsRunMode() && mBoard && mGameScene==GameScenes::SCENE_PLAYING &&
                !mRunIntroHeld && !mOnlineRunStartHeld && !mOnlineWaitingStartAck &&
                mRunState->GetLevel()==mBoard->mLevel &&
                mOnlineSession->WaitForNextRound(anEvent.mSeat))break;

            if(mRunState && mRunState->IsNewEndless() && mRunState->mLevelIndex>0 && mOnlineSession->SupportsAdmission())break;
			// @pvz-online: 中继：有人进了房（mSeat 说清是谁）——直连没有这条，那边的
			// 第一个人就是 CONNECTED。本机在棋盘上跑着闯关局、又是主机：当场把他拉进
			// 当前这一关（他没检查点/对不上就走补发追赶）。不拉的话这一关的"全队判胜"
			// 永远凑不齐——一个连上了却没棋盘的席位不会报"我清完了"。人还在菜单上就
			// 什么都不做（名册小条自己会显示他）。
			NetworkLog("[net] seat %u joined the room", (unsigned)anEvent.mSeat);
			if (IsRunMode() && mBoard != nullptr
				&& mOnlineSession->GetRole() == NetSession::Role::HOST)
			{
				NetworkLog("[run] seat %u joined mid-level - pulling them into level %d (index %d)",
					(unsigned)anEvent.mSeat, mRunState->GetLevel(), mRunState->mLevelIndex);
				mOnlineSession->SendStartLevel((uint8_t)GameMode::GAMEMODE_ADVENTURE,
					(uint32_t)mRunState->GetLevel(), mRunState->GetLevelSeed(),
					true, mRunState->mRunSeed, (uint16_t)mRunState->mLevelIndex,
					anEvent.mSeat, (uint8_t)mRunState->mMode, (uint8_t)mRunState->mDiff,
					(uint8_t)mRunState->mScale, (uint8_t)mRunState->mTempo, (uint8_t)mRunState->mZombotany,
					(uint8_t)mRunState->mEndlessScene, (uint8_t)mRunState->mBossFlag,
					(uint8_t)mRunState->mBuffMult, (uint8_t)mRunState->mLadder,
					(uint8_t)mRunState->mZombieBuffMode, mRunState->mZombieBuffStacks,(uint8_t)mRunState->mSpeedTier, (uint8_t)mRunState->mFirstWave, (uint8_t)mRunState->mEndlessOptions, (uint8_t)mRunState->mCrowdRule);
			}
			break;

		case NetSession::EventType::PEER_LEFT:
			// @pvz-online: 中继：有人走了（服务器还在、我也还在；房主走 = ROOM_CLOSED，
			// 那条是 DISCONNECTED 的死路）。人还在棋盘上就挂一句说明接着打——少一个人
			// 不是这一局的死刑，判胜按"现在的上座席位"算，他那一格已经跟着回收；
			// 人还在菜单上就什么都不做。
			NetworkLog("[net] seat %u left the room", (unsigned)anEvent.mSeat);
			if (mBoard != nullptr)
			{
				mOnlineSession->PostNotice(ModText::Tr("队友离开了房间。", "A teammate left the room."));
			}
			break;

		case NetSession::EventType::RECONNECTING:
			NetworkLog("[net] link lost - reconnecting to room %s, our seat is held",
				mOnlineSession->GetRoomCode().c_str());
			// 询问框挂着的客户端（人还在主菜单上、还没答"是否加入"）：这一刻起 ACK 也发不
			// 出去，"是否加入"答了也没用——先收掉，免得和重连框叠着摆。归队后主机补发的
			// START_LEVEL 会让它原样再摆出来（见会话层 PEER_BACK 的 awaiting 重发），
			// 回答流程一步不少。
			//
			// "等 ACK"的作废不在这儿做：UpdateOnlineStart ① 每帧先跑（本帧会话层已经切到
			// 重连态），非闯关局的等待当场就按"队友没了"那条作废；闯关局的等待由 ① 的
			// 新门特意留到重连结束——全队等主机自己回来，不算白等。
			DismissOnlineStartPrompt(false);
			break;

		case NetSession::EventType::RECONNECTED:
			// 我自己回来了：收框（棋盘冻结随之解除）、留一句说明。不重跑任何开局链——
			// 剧本、棋盘、记忆全是原样保留的那一套；我这边"已选卡/已清完"的记忆会话层
			// 已清掉、归队时会重新报一遍，队友那边的等待门跟着重开。
			NetworkLog("[net] reconnected - we are back in room %s", mOnlineSession->GetRoomCode().c_str());
			KillDialog(Dialogs::DIALOG_ONLINE_RECONNECT);
			mOnlineSession->PostNotice(ModText::Tr("已重新连接。", "Reconnected."));
			break;

		case NetSession::EventType::HOST_CHANGED:
            DismissOnlineStartPrompt(false);
            ZombieVoteCloseUI();mZombieVoteFrames=0;mZombieVoteMyChoice=-1;mZombieVoteSeatMask=0;mZombieVoteVotedSeats=0;
            if(mOnlineSession->NeedsHostStartRecovery()) {
                mOnlineWaitingStartAck=true;mOnlineStartWaitFrames=0;
                mOnlineRunStartHeld=false;mOnlineRunGo=true;
                mOnlineSession->FinishHostStartRecovery();
            } else if(mOnlineSession->GetRole()!=NetSession::Role::HOST) {
                mOnlineWaitingStartAck=false;
            }
            mOnlineSession->PostNotice(ModText::Tr("房主已交接，队伍继续。","A new host has taken over."));
            break;

        case NetSession::EventType::PEER_OFFLINE:
			{
				// 队友掉线、席位进了保留期：给个说法。等待门不动（他那格还占着）——全队
				// 一起等他回来；保留期走完服务器会广播 PEER_LEAVE，那时才清格放行。
				NetworkLog("[net] seat %u lost the link - its seat is held", (unsigned)anEvent.mSeat);
				std::string aName = mOnlineSession->GetSeatName(anEvent.mSeat);
				if (aName.empty()) aName = "P" + std::to_string((unsigned)anEvent.mSeat);
				mOnlineSession->PostNotice((aName + ModText::Tr(" 掉线了，正在重连……", " lost the link - reconnecting...")).c_str());
			}
			break;

		case NetSession::EventType::PEER_BACK:
			{
				// 掉线的那位回来了（会话层只在这席位此前确实标过 offline 时才推这条——
				// 快路径的静默替换不打扰任何人）。补状态全在会话层里做，这儿只提示一句。
				NetworkLog("[net] seat %u is back", (unsigned)anEvent.mSeat);
				std::string aName = mOnlineSession->GetSeatName(anEvent.mSeat);
				if (aName.empty()) aName = "P" + std::to_string((unsigned)anEvent.mSeat);
				mOnlineSession->PostNotice((aName + ModText::Tr(" 已重新连接。", " is back.")).c_str());
			}
			break;

		default:
			break;
		}
	}
}

void LawnApp::UpdateOnlineEnd()
{
    if (mOnlineSession && mOnlineSession->RelayBattleHeld()) return;
    if (mBoard && IsLocalOnlyMode(mGameMode)) return; // @pvz-online: local garden/tree only.
	if (!mOnlineSession || mOnlineSession->IsReconnecting()) return;
    const bool canComplete = mBoard && mGameScene == GameScenes::SCENE_PLAYING &&
        !mOnlineWaitingStartAck && !mOnlineRunStartHeld && !mRunIntroHeld &&
        (!IsRunMode() || (!mRunState->HasPendingPick() && !mRunState->IsCatchingUp() &&
            mRunState->GetLevel() == mBoard->mLevel));

    NetProto::MsgEscapedZombie pendingRelay;
    const bool hasPendingRelay=mOnlineSession->PeekPendingEscapedZombie(pendingRelay);
    // A delayed team-complete message cannot overrule the current local board.
    const bool localClear = canComplete && !hasPendingRelay && mBoard->mLevelAwardSpawned &&
        !mBoard->AreEnemyZombiesOnScreen();

	// ①
	if (canComplete)
	{
		// @pvz-online: 联机闯关（R5）的换关空档：点名还在飞的时候（主机等 ACK / 客户端把
		// 命令寄存着），屏幕上这块是**上一关**的棋盘——它当然清完了，但这一关的记账已经
		// 翻篇（SendStartLevel 清过"谁清完了"），再报一次会把两边都带成"全队清完"，
		// 白送一关。等新棋盘建起来（点名落地）这条通道自己就恢复了。
		bool aHandoff = mOnlineWaitingStartAck || mOnlineRunStartHeld;
		if (!aHandoff)
		{
			bool aClear = localClear;
			mOnlineSession->SendLevelDone(aClear);

            // Completion is still reported above; mobile waits without a lawn banner.
		}
	}

	// ② 全队清完
	if (localClear && mOnlineSession->TakeAllLevelsDone())
	{
		// 等到齐了：那句"等队友们"当场撤掉——棋盘还要当换关的背景 / 三选一屏的地，
		// 别让它挂在那儿过期。
		if (mOnlineWaitingAdviceOn && mBoard != nullptr)
		{
			mBoard->ClearAdvice(AdviceType::ADVICE_NONE);
		}
		mOnlineWaitingAdviceOn = false;
		if (mBoard != nullptr)
		{
			// @pvz-online: 联机闯关（R5）：全队过了这一关，走的不是"回主菜单"那条路——
			// 回菜单会把 mRunState 删掉、这一局就没了。两边各自领本关的过关奖（各选各的），
			// 棋盘留着当换关的背景；选完由 UpdateRunPick 走"主机点名 / 队友等点名"。
			// 打完最后一关的收尾（删检查点 + 亮通关奖杯屏，按钮回菜单）两边各自做，做的是一样的。
			if (IsRunMode())
			{
				NetworkLog("[net] every lawn is clear - the run moves on");
                if(!TeamPlay::NoteVictory(this))return;
				AwardRunRerollVoucher();
        ExpireCoopLoan();
        if(EndlessLawn::StopAtLimit(this))return;
        EndlessLawn::RememberLineup(mBoard);
        mRunState->AdvanceLevel();
				if (mRunState->IsComplete())
				{
					NetworkLog("[run] the run is complete");
					RunHistory::Capture(this,RunHistory::COMPLETE);
                    RunState::DeleteCheckpoint(mPlayerInfo->mId);
					// 通关收尾（批十）：亮奖杯屏，回主菜单交给屏上的按钮——mRunState 那一刻
					// 才由 ShowGameSelector 收掉，在那之前它得留着（屏要把关数报出来）。
					// 这局已经结束：棋盘不再当换关背景，和其它收尾一样拆掉。
					KillBoard();
					ShowAwardScreen(AWARD_RUN_COMPLETE, false);
				}
				else
				{
					mRunState->BeginLevelEndPicks();
                // @pvz-online: freeze the old lawn during rewards; a playing scene cannot run the next intro.
                if(mRunState->IsEndless() && mBoard) {
                    mRunIntroHeld=true;mGameScene=GameScenes::SCENE_LEVEL_INTRO;
                }
                mRunState->Save(mPlayerInfo->mId);
				}
			}
			else
			{
				NetworkLog("[net] every lawn is clear - back to the main menu");
				DoBackToMain(false);
				ShowOnlineNotice(this, ModText::Tr("全队过关", "Level complete"), ModText::Tr("所有草坪已清空，返回主菜单。", "All lawns are clear. Back to the main menu."));
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
			NetworkLog("[net] the team lost (reason %u) - playing the lost-level ending here too",
				(unsigned)aOver.mReason);
			mBoard->ZombiesWon(nullptr, true);
		}
		else
		{
			NetworkLog("[net] the team lost (reason %u) while in the menu - nothing to do",
				(unsigned)aOver.mReason);
		}
	}
}

void LawnApp::RetryOnlineLevel()
{
	if (mOnlineSession == nullptr || mBoard == nullptr) return;
	if (mOnlineSession->GetRole() != NetSession::Role::HOST) return;
    if (mOnlineWaitingStartAck || !mOnlineSession->CanHostStart()) return;
    KillDialog(Dialogs::DIALOG_GAME_OVER);

	// @pvz-online: 联机闯关（R5）：整队重来本关，这一局不能丢——走"回主菜单再开局"那条路
	// 会把 mRunState 删掉（ShowGameSelector），队友收到的就是一条没有闯关上下文的开局命令。
	// 所以闯关局直接在原地重发"本关、同波表种子"的换关命令，吃脑子的残局留着当背景，
	// 队友那边走的是同一条寄存 → 收口 → 回 ACK 的路（他的棋盘也是残局，会被拆掉）。
	if (IsRunMode())
	{
		int aRunLevel = mRunState->GetLevel();
		int aRunSeed = mRunState->GetLevelSeed();
		// 房里没别人时没有能回 ACK 的人：等下去挂死（闯关不设超时、残局在时也不挂取消框）。
		// 撤掉"点名等 ACK"，按单人的路直接重开这一关（EnterRunLevel 自己会落单人分支）。
		if (!WillWaitForStartAck())
		{
			NetworkLog("[net] the host retries run level %d (index %d) - nobody else in the room, restarting directly",
				aRunLevel, mRunState->mLevelIndex);
			EnterRunLevel();
			return;
		}
		NetworkLog("[net] the host retries run level %d (index %d) - calling the team back in",
			aRunLevel, mRunState->mLevelIndex);
		SetOnlineStartOverride(aRunLevel, aRunSeed);
		mOnlineWaitingStartAck = true;
		mOnlineStartWaitFrames = 0;
		// 同 UPDATE 拉人：广播要显式 SEAT_UNSET（见 UpdateOnlineEvents 的拉人注释）。
		mOnlineSession->SendStartLevel((uint8_t)GameMode::GAMEMODE_ADVENTURE, (uint32_t)aRunLevel, aRunSeed,
			true, mRunState->mRunSeed, (uint16_t)mRunState->mLevelIndex,
			NetProto::SEAT_UNSET, (uint8_t)mRunState->mMode, (uint8_t)mRunState->mDiff,
			(uint8_t)mRunState->mScale, (uint8_t)mRunState->mTempo, (uint8_t)mRunState->mZombotany,
			(uint8_t)mRunState->mEndlessScene, (uint8_t)mRunState->mBossFlag,
			(uint8_t)mRunState->mBuffMult, (uint8_t)mRunState->mLadder,
			(uint8_t)mRunState->mZombieBuffMode, mRunState->mZombieBuffStacks,(uint8_t)mRunState->mSpeedTier, (uint8_t)mRunState->mFirstWave, (uint8_t)mRunState->mEndlessOptions, (uint8_t)mRunState->mCrowdRule);
		return;
	}

	int aLevel = mBoard->mLevel;
	int aSeed = mBoard->GetLevelRandSeed();
	uint8_t aMode = (uint8_t)mGameMode;

	DoBackToMain(false);
	SetOnlineStartOverride(aLevel, aSeed);
	// 同上：房里没别人时没有 ACK 可等——菜单已经回来了，直接重开这一关
	//（和 PreNewGame 的单人分支同一条路）。
	if (!WillWaitForStartAck())
	{
		KillDialog(Dialogs::DIALOG_ONLINE);
		KillGameSelector();
		NewGame();
		return;
	}
	mOnlineWaitingStartAck = true;
	mOnlineStartWaitFrames = 0;
	NetworkLog("[net] the host retried level %d - asking the team to come back in", aLevel);
	mOnlineSession->SendStartLevel(aMode, (uint32_t)aLevel, aSeed);
}

void LawnApp::EnterRunLevel()
{
    if (mOnlineSession && mOnlineSession->IsRelay() &&
        mOnlineSession->GetRole() == NetSession::Role::HOST && !mOnlineSession->CanHostStart())
    { DoOnlineDialog(); return; }
	KillRunFlowScreen();
    if(mOnlineSession && mOnlineSession->GetRole()==NetSession::Role::HOST && mRunState->IsEndless()) {
        InitializeAdmissionIdentity();if(!mRunState->mAdmission.run)mRunState->mAdmission.run=Admission::NewId();
        mRunState->mAdmission.player=mOnlineSession->mCoopToken;mRunState->mTeam.player=mOnlineSession->mCoopToken;mOnlineSession->mEndlessStartRun=mRunState->mAdmission.run;
        if(mRunState->mAdmission.history.empty())mRunState->mAdmission.history.push_back({mOnlineSession->mCoopToken,0,0});
    }

	int aLevel = mRunState->GetLevel();
	int aSeed = mRunState->GetLevelSeed();

	mGameMode = GameMode::GAMEMODE_ADVENTURE;
	SetOnlineStartOverride(aLevel, aSeed);
	NetworkLog("run: entering level %d (index %d, seed %d)", aLevel, mRunState->mLevelIndex, aSeed);

	// @pvz-online: 队友连着的时候（R5），换关命令由主机广播、等 START_ACK——和单关联机
	// 开局同一条链路；主菜单照旧留着当等待的看板，并且把上面的入口全按下去
	// （等待里开出的模态框会横跨到入场之后）。客户端的"进关"是主机点名点出来的，
	// 走的是下面那条路（见 UpdateRunPick 的收口）。
	if (WillWaitForStartAck())
	{
		NetworkLog("[run] the host calls the team into level %d (index %d)",
			aLevel, mRunState->mLevelIndex);
		mOnlineWaitingStartAck = true;
		mOnlineStartWaitFrames = 0;
		if (mGameSelector) mGameSelector->SetMenuButtonsDisabled(true);
		mOnlineSession->SendStartLevel((uint8_t)mGameMode, (uint32_t)aLevel, aSeed,
			true, mRunState->mRunSeed, (uint16_t)mRunState->mLevelIndex,
			NetProto::SEAT_UNSET, (uint8_t)mRunState->mMode, (uint8_t)mRunState->mDiff,
			(uint8_t)mRunState->mScale, (uint8_t)mRunState->mTempo, (uint8_t)mRunState->mZombotany,
			(uint8_t)mRunState->mEndlessScene, (uint8_t)mRunState->mBossFlag,
			(uint8_t)mRunState->mBuffMult, (uint8_t)mRunState->mLadder,
			(uint8_t)mRunState->mZombieBuffMode, mRunState->mZombieBuffStacks,(uint8_t)mRunState->mSpeedTier, (uint8_t)mRunState->mFirstWave, (uint8_t)mRunState->mEndlessOptions, (uint8_t)mRunState->mCrowdRule);
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
	// @pvz-online: 走到这儿的联机主机只有"房里没别人"一种（WillWait 为假，广播那条在上面
	// 就返回了）：没有 SendStartLevel / TakePendingStartLevel 那两步的判胜复位，得自己翻篇——
	// 不然上一关"全队清完"的一次性闩（mAllDoneTaken）会把这一关的收摊永远挡住。
	// 客户端不清：它的复位在收命令那步（TakePendingStartLevel），各席的选卡记账还要留给
	// RUN_GO 用。
	if (mOnlineSession != nullptr && mOnlineSession->GetRole() == NetSession::Role::HOST)
	{
		mOnlineSession->DiscardLevelPackets();
        if(mRunState->IsNewEndless() && mOnlineSession->SupportsAdmission())SendAdmissionStart(NetProto::SEAT_UNSET);
	}
	// @pvz-online: 无尽局原地续关（无尽续草坪批）：这块草坪刚打赢（单机 mLevelComplete /
	// 联机 mLevelAwardSpawned），留着当地——下一关的波次与三选一收尾都在放开开场那一刻。
	EndlessLawn::Enter(this);
}

bool LawnApp::AlignRunToHost(int theRunSeed, int theTargetIndex, int theRunMode, int theRunDiff,
	int theRunScale, int theRunTempo, int theZombotany, int theEndlessScene, int theBossFlag,
	int theBuffMult, int theLadder, int theZombieBuff, const uint8_t* theZombieStacks,int theSpeedTier, int theFirstWave, int theEndlessOptions, int theCrowdRule)
{
	// 时长档来自对端（构建代次不同只提示、不拒连）：非法值按完整版处理，别让它把
	// 越界模式一路带进关卡表。无尽档（3）是合法档，得放行——钳成完整版会让客户端
	// 对着无尽局建出普通局，两边必岔。
	if (theRunMode < RunState::RUN_MODE_FULL || theRunMode > RunState::RUN_MODE_ENDLESS)
	{
		NetworkLog("[run] the host named an unknown run mode %d - treating it as the full run", theRunMode);
		theRunMode = RunState::RUN_MODE_FULL;
	}

	// 无尽场景同理（MOD_BUILD 37）：非法值按 0（白天）；非无尽档一律归 0（与检查点读档同口径）。
	if (theEndlessScene < 0 || theEndlessScene >= RunState::RUN_SCENE_COUNT)
	{
		NetworkLog("[run] the host named an unknown endless scene %d - treating it as the day scene", theEndlessScene);
		theEndlessScene = 0;
	}
	if (theRunMode != RunState::RUN_MODE_ENDLESS) theEndlessScene = 0;

	// 出怪难度档同理（MOD_BUILD 27）：非法值按标准，别让它一路带进出怪算式。
	if (theRunDiff < RunState::RUN_DIFF_EASY || theRunDiff > RunState::RUN_DIFF_HIGH)
	{
		NetworkLog("[run] the host named an unknown run difficulty %d - treating it as standard", theRunDiff);
		theRunDiff = RunState::RUN_DIFF_STD;
	}

	// 高级选项三参数同理（MOD_BUILD 35）：非法值按 标准/标准/关。
	if (theRunScale < RunState::RUN_SCALE_HALF || theRunScale > RunState::RUN_SCALE_QUAD)
	{
		NetworkLog("[run] the host named an unknown run scale %d - treating it as standard", theRunScale);
		theRunScale = RunState::RUN_SCALE_STD;
	}
	if (theRunTempo < RunState::RUN_TEMPO_FAST || theRunTempo > RunState::RUN_TEMPO_SLOW)
	{
		NetworkLog("[run] the host named an unknown run tempo %d - treating it as standard", theRunTempo);
		theRunTempo = RunState::RUN_TEMPO_STD;
	}
	if (theZombotany != 0 && theZombotany != 1)
	{
		NetworkLog("[run] the host named an unknown zombotany switch %d - treating it as off", theZombotany);
		theZombotany = 0;
	}

	// 关底 boss 开关同理（MOD_BUILD 38）：非法值按关。
	if (theBossFlag<0 || theBossFlag>2)
	{
		NetworkLog("[run] the host named an unknown boss switch %d - treating it as off", theBossFlag);
		theBossFlag = 0;
	}

	// 增益倍选同理（MOD_BUILD 43）：非法值按 ×1，别让它把越界倍数一路带进发屏算式。
	if (theBuffMult < RunState::RUN_BUFFMULT_X1 || theBuffMult > RunState::RUN_BUFFMULT_X3)
	{
		NetworkLog("[run] the host named an unknown buff pick multiplier %d - treating it as x1", theBuffMult);
		theBuffMult = RunState::RUN_BUFFMULT_X1;
	}

	// 场景难度阶梯同理（MOD_BUILD 45）：非法值按现状档（×1.33），别让它把越界档一路带进
	// 千分比查表。僵尸 buff 档同款（非法值按关）。
	if (theLadder < RunState::RUN_LADDER_STD || theLadder > RunState::RUN_LADDER_X2)
	{
		NetworkLog("[run] the host named an unknown run ladder %d - treating it as the default ladder", theLadder);
		theLadder = RunState::RUN_LADDER_STD;
	}
	if (theZombieBuff < 0 || theZombieBuff > 2)
	{
		NetworkLog("[run] the host named an unknown zombie buff mode %d - treating it as off", theZombieBuff);
		theZombieBuff = 0;
	}

	// 关序号来自对端（构建代次不同只提示、不拒连）：越界就按第 1 关处理——
	// 旧构建的关卡表只有 5 格，混搭时别让它把越界值一路带进 LevelForIndex。
	if (theTargetIndex < 0 || theTargetIndex >= RunState::LevelCountForMode(theRunMode))
	{
		NetworkLog("[run] the host named an out-of-range level index %d - treating it as the first level",
			theTargetIndex);
		theTargetIndex = 0;
	}

    const auto& context=mOnlineSession->mEndlessStartContext;
    const bool paired=theRunMode==RunState::RUN_MODE_ENDLESS && context.run && context.epoch==mOnlineSession->GetReadyEpoch() && context.level==uint32_t(theTargetIndex);
    mOnlineSession->mEndlessNeedsAdmission=false;
    if(paired && (context.budget&512)) {
        RunState restored;bool matched=EndlessSaves::Match(mPlayerInfo->mId,theEndlessScene,context.run,theRunSeed,theTargetIndex,(context.budget&1024)!=0,restored);
        if(matched && restored.mAdmission.player && restored.mAdmission.player!=mOnlineSession->mCoopToken)matched=false;
        if(matched && restored.IsNewEndless() && !restored.mEndlessLawn.empty()) {
            std::vector<unsigned char> native;std::string random;
            const auto& bytes=restored.mEndlessSnapshotPhase==3?restored.mEndlessRetryLawn:restored.mEndlessLawn;
            matched=EndlessLawn::Unpack(bytes,native,random);
        }
        if(matched && restored.IsNewEndless() && restored.mEndlessLawn.empty() && theTargetIndex>0)matched=false;
        RunState laterHead;const bool ahead=laterHead.Load(mPlayerInfo->mId,RunState::RUN_MODE_ENDLESS,theEndlessScene,false,context.run) && laterHead.mLevelIndex>theTargetIndex;
        if((!matched || ahead) && !EndlessSaves::PreserveUnmatched(mPlayerInfo->mId,theEndlessScene,context.run))return false;
        KillRunFlowScreen(); SetTeamProgressionController(nullptr);mRunState.reset();
        if(matched){mRunState=std::make_unique<RunState>(std::move(restored)); mRunState->mAdmission.player=mOnlineSession->mCoopToken;mRunState->mTeam.player=mOnlineSession->mCoopToken;}
        else mOnlineSession->mEndlessNeedsAdmission=true;
    } else if(paired && mRunState && mRunState->mAdmission.run!=context.run) {
        KillRunFlowScreen(); SetTeamProgressionController(nullptr);mRunState.reset();
    }

	if (mRunState != nullptr
		&& ((mOnlineSession->HasAdmissionOffer(theTargetIndex) && mRunState->mAdmission.run!=mOnlineSession->mAdmissionOffer.run) || mRunState->mRunSeed != theRunSeed || mRunState->mMode != theRunMode
			|| mRunState->mDiff != theRunDiff
			|| mRunState->mEndlessScene != theEndlessScene
			|| mRunState->mLevelIndex > theTargetIndex))
	{
		NetworkLog("[run] the local run does not match the host (seed %d vs %d, mode %d vs %d, diff %d vs %d, scene %d vs %d, index %d vs %d) - rebuilding",
			mRunState->mRunSeed, theRunSeed, mRunState->mMode, theRunMode, mRunState->mDiff, theRunDiff,
			mRunState->mEndlessScene, theEndlessScene, mRunState->mLevelIndex, theTargetIndex);
        KillRunFlowScreen(); // Rebuild labels and actions for the replacement run.
        SetTeamProgressionController(nullptr);
		mRunState.reset();
		mRunState = nullptr;
	}

    bool initializeStartPicks = false;
	if (mRunState == nullptr)
	{
		mRunState = std::make_unique<RunState>();
        bool loaded=mRunState->Load(mPlayerInfo->mId,theRunMode,theEndlessScene,false,mOnlineSession->HasAdmissionOffer(theTargetIndex)?mOnlineSession->mAdmissionOffer.run:(paired?context.run:0));
        if(!loaded && mOnlineSession->HasAdmissionOffer(theTargetIndex)) {
            loaded=mRunState->Load(mPlayerInfo->mId,theRunMode,theEndlessScene);
            if(loaded && (mRunState->mAdmission.run!=mOnlineSession->mAdmissionOffer.run || mRunState->mAdmission.player!=mOnlineSession->mCoopToken))loaded=false;
        }
        if(loaded && paired && !mOnlineSession->mEndlessNeedsAdmission && (mRunState->mRunSeed!=theRunSeed || mRunState->mLevelIndex>theTargetIndex) && !EndlessSaves::PreserveUnmatched(mPlayerInfo->mId,theEndlessScene,context.run))return false;

		if (mOnlineSession->mEndlessNeedsAdmission || !loaded
			|| mRunState->mRunSeed != theRunSeed
			|| mRunState->mMode != theRunMode
			|| mRunState->mDiff != theRunDiff
			|| mRunState->mEndlessScene != theEndlessScene
			|| mRunState->mLevelIndex > theTargetIndex)
		{
			mRunState->StartNew(theRunSeed, theRunMode, theRunDiff,
				RunState::RUN_SCALE_STD, RunState::RUN_TEMPO_STD, 0, theEndlessScene);
            initializeStartPicks = true;
			NetworkLog("[run] aligning to the host: a fresh run at seed %d (mode %d, diff %d, scene %d), catching up to index %d",
				theRunSeed, theRunMode, theRunDiff, theEndlessScene, theTargetIndex);
		}
		else
		{
			NetworkLog("[run] aligning to the host: reusing the local checkpoint at index %d",
				mRunState->mLevelIndex);
		}
	}

	// 高级选项三字段（批 C）是房间级设置、房主权威：不管检查点续没续上，对齐完一律按
	// 房主的值覆盖（不参与上面的重建判等——改这三样不该把队友手里的整局作废）。
	// 增益倍选（MOD_BUILD 43）同口径：房间级设置、只覆盖不参与重建判等。
	// 场景难度阶梯 + 僵尸 buff 档（MOD_BUILD 45）同理（前者也无尽档不生效，照存）。
	// 9 条强化层数是"本局进度"而非设置：权威快照在主机（投票结果只驱动 UI、层数只信
	// START_LEVEL），一律按这条命令的 8 字节覆盖——中途加入/追赶/重试不走样。
	mRunState->mScale = theRunScale;
	mRunState->mTempo = theRunTempo;
	mRunState->mZombotany = theZombotany;
	mRunState->mBossFlag = mRunState->mMode != RunState::RUN_MODE_ENDLESS ? theBossFlag : 0;
    mRunState->mSpeedTier=theSpeedTier>=0 && theSpeedTier<=2 ? theSpeedTier : 0;
	mRunState->mFirstWave = theFirstWave >= RunState::RUN_FIRSTWAVE_STD && theFirstWave <= RunState::RUN_FIRSTWAVE_LONG ? theFirstWave : RunState::RUN_FIRSTWAVE_STD;
	mRunState->mEndlessOptions = EndlessRules::Valid(theEndlessOptions) && mRunState->IsEndless() ? theEndlessOptions : 0;
    // @pvz-online: retain local progress but never restore a lawn created under another crowd rule.
    const int crowdRule = CrowdRules::Valid(static_cast<unsigned>(theCrowdRule)) ? theCrowdRule : CrowdRules::LEGACY;
    if (mRunState->mCrowdRule != crowdRule) {
        mRunState->mEndlessLawn.clear(); mRunState->mEndlessRetryLawn.clear();
        mRunState->mEndlessSnapshotLevel = -1; mRunState->mEndlessSnapshotPhase = 0;
    }
    mRunState->mCrowdRule = crowdRule;
	mRunState->mBuffMult = theBuffMult;
	mRunState->mLadder = theLadder;
	mRunState->mZombieBuffMode = theZombieBuff;
	for (int i = 0; i < RunState::RUN_ZOMBIE_BUFF_COUNT; i++)
		mRunState->mZombieBuffStacks[i] = theZombieStacks ? theZombieStacks[i] : 0;

    if(paired) {
        mRunState->mAdmission.run=context.run;mRunState->mAdmission.player=mOnlineSession->mCoopToken;
        if(mOnlineSession->mEndlessNeedsAdmission){mRunState->mAdmission.phase=Admission::PICKS;mRunState->mAdmission.joined=theTargetIndex;mRunState->mLevelIndex=theTargetIndex;mRunState->mCatchUpLevel=-1;mRunState->mPendingPlantPicks=mRunState->mPendingBuffPicks=0;mRunState->mEndlessLawn.clear();mRunState->mEndlessRetryLawn.clear();mRunState->mEndlessSnapshotLevel=-1;mRunState->mEndlessSnapshotPhase=0;}
    }
    ApplyAdmissionOffer(theTargetIndex);
    if(initializeStartPicks && mRunState->mAdmission.phase==Admission::NONE && mRunState->mLevelIndex==0)
        mRunState->BeginStartPicks();
	if (mRunState->mLevelIndex < theTargetIndex)
	{
		mRunState->BeginCatchUp(theTargetIndex);
	}
    return true;
}

bool LawnApp::TryKeepEndlessLawn()
{
	if (mRunState == nullptr || !mRunState->IsEndless() || mBoard == nullptr) return false;
	if (mBoardResult == BoardResult::BOARDRESULT_LOST) return false;
	if (!mBoard->mLevelComplete && !mBoard->mLevelAwardSpawned) return false;

	// 草坪接着用：本关的胜负记号翻篇，先报"我还没选好"（和 NewGame 一处口径）。
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mGameScene = GameScenes::SCENE_LEVEL_INTRO;
	if (IsOnlineGame()) mOnlineSession->SendSeedsReady(false);

	// Every stage crosses the common reward/vote gate, even when personal pools are capped.
	mRunIntroHeld = true;
	return true;
}

void LawnApp::ReleaseRunIntro()
{
    if(mOnlineSession && mOnlineSession->mEndlessNeedsAdmission)return;
    // @pvz-online: admission already consumed its chooser; restarting that intro dereferences it.
    if(mBoard && mRunState && mRunState->IsNewEndless() && mRunState->mAdmission.phase>=Admission::PREPARED && !mSeedChooserScreen) {
        mRunIntroHeld=false;mBoard->mCutScene->mSeedChoosing=false;
        mBoard->RemoveCutsceneZombies();mBoard->mMenuButton->mBtnNoDraw=false;
        mBoard->mShowShovel=true;mBoard->Move(0,0);mBoard->mSeedBank->Move(10,0);
        mBoard->mSeedBank->mCutSceneDarken=0;StartPlaying();return;
    }
	if (mBoard != nullptr && mRunState != nullptr && mRunState->IsEndless()
		&& mBoardResult != BoardResult::BOARDRESULT_LOST
		&& (mBoard->mLevelComplete || mBoard->mLevelAwardSpawned))
	{
		mBoard->InitEndlessRunStage();
	}
// @pvz-online: restarting an intro requires its scene as well as a cleared hold flag.
    mGameScene = GameScenes::SCENE_LEVEL_INTRO;
	mRunIntroHeld = false;
	mBoard->mCutScene->StartLevelIntro();
}

void LawnApp::UpdateRunPick()
{
    if(mOnlineSession && mOnlineSession->mEndlessNeedsAdmission)return;
	if (mRunState == nullptr || (mOnlineSession && mOnlineSession->IsReconnecting())) return;
    if(mRunState->mAdmission.fast || AdmissionBuilding())return;

	// @pvz-online: 通关收尾期（批十：奖杯屏挂着、mRunState 还没被屏上按钮后的
	// ShowGameSelector 收掉）什么都不做——尤其别走底下"棋盘不在就开下一关"那条：
	// 关序号已经越界（== 关数），一开就是坏局。
	if (mRunState->IsComplete() || mRunState->mEndlessSnapshotPhase==4) return;

	// ① 新草坪已经建好、开场压着（R6 起联机与单机闯关都走这条）：三选一屏就摆在草坪上，
	// 补发追赶一屏接一屏地补。全都清完才按最新的卡池重填卡槽、放开选卡与开场——
	// 和 NewGame 的收尾两行同款。草坪没建起来之前一张屏都不摆，这正是 R6 要的：
	// 选项不再盖在主菜单 / 上一关的残局上。
	// @pvz-online: a retained cleared lawn must finish START/ACK/RUN_GO before reward/intro release.
    // The completion latch resets when the next command is sent/received; waiting flags cover its flight.
    const bool waitingEndlessHandoff = IsOnlineGame() && mRunState->IsEndless() &&
        (mOnlineSession->IsLocalLevelDone() || mOnlineWaitingStartAck || mOnlineRunStartHeld);
    if (mRunIntroHeld && mBoard != nullptr && !waitingEndlessHandoff)
	{
        
		if (mRunState->HasPendingPick())
		{
			if (mRunFlowScreen) return;
			// @pvz-online: 候选"抽好了就用"：「重摇」在 PaidReroll 里已经把这一屏重抽过了
			// （mChoicesRolled 记着）——这儿再抽一遍会把重抽出来的三条冲掉、还白推 mPickCounter。
			if (!mRunState->ChoicesRolled()) mRunState->RollChoices();
			// 抽干兜底（方案 §2.4）：增益池一条不剩时 RollChoices 把欠的屏作废了——
			// 那一刻三条全是哨兵、没得点，屏别开（开了没出口）；下一帧走下面的收尾。
			if (!mRunState->HasPendingPick()) return;
			ShowRunPicks();
			return;
		}

		if (mRunState->IsCatchingUp())
		{
			mRunState->AdvanceCatchUp();
			NetworkLog("[run] caught up one level (now at index %d)", mRunState->mLevelIndex);
			return;
		}

		// @pvz-online: 卡池≤10 的开场门拦下之后的重入（等队友的那几帧）：下面这些事
		// 都是"只做一次"的（选卡界面已经建过，再建一次会撞断言），一件都别再碰；
		// 放行由 UpdateOnlineSeeds 负责，它每帧都在跑。
        if(TeamPlay::For(this).HoldPersonalRewards())return;
        if(TryOpenZombieVote())return;
        if(TeamPlay::For(this).HoldPreparation())return;
		if (mOnlineSeedsHeld) return;

		NetworkLog("[run] the picks are done - the chooser and the intro can start");
		// 卡槽在 InitLevel 建场时按"当时"的卡池填过，刚刚这几屏的新植物要重填一次
		// （卡池 ≤10 的关卡全程不开选卡界面，不重填这一关新选的植物就赶不上）。
		if (!mBoard->ChooseSeedsOnCurrentLevel())
		{
			mBoard->FillSeedBankFromRunPool();
		}
		else
		{
			// @pvz-online: 卡池是刚做完这几屏才越过 10 的（建场时还 ≤10，卡槽已按当时的池子
			// 填过并定好了格数）：这一关要开选卡界面，而界面的出槽数、开始按钮、Let's Rock
			// 的回填循环全按 mSeedBank->mNumPackets 走——先把它按"本关选 10 株"的口径重定、
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

		// @pvz-online: "都确定才开场"（R7）。卡池≤10 的关卡全场不开选卡界面，按不了
		// Let's Rock、也就没有那道关屏门（TryHoldSeedChooserForTeammates）——这道门口
		// 是它的替身，等的东西一样：刚在 ShowSeedChooserScreen 里报过"我选好了"，
		// 队友没齐就等。等待期草坪继续冻着（mRunIntroHeld 保持 true，CutScene::Update
		// 一步不走）；齐了由 UpdateOnlineSeeds 撤框、解冻、开场。
		if (!mBoard->ChooseSeedsOnCurrentLevel() && IsOnlineGame()
			&& (mOnlineSession->HasOtherSeats() || mOnlineSession->IsPreparationActive()) && !mOnlineSession->IsPeerSeedsReady())
		{
			mOnlineSeedsHeld = true;
			NetworkLog("[net] no seed chooser on this level - holding the intro until the whole team is ready");
			EnsureSeedsWaitDialog(this);
			return;
		}

		// @pvz-online: 放开走统一收口（无尽局在这儿就地续排下一关的波次；见函数注释）。
		ReleaseRunIntro();
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
			// 房里没别人时（一个人开的房 / 队友都走了）没有要等的人：自己清完就点名——
			// 单人房不通这条的话，这一局到这儿就没人动了（2026-10-03 实机复现）。
			// A loss waits for the host retry action; only a completed round advances automatically.
			if (!mOnlineWaitingStartAck && mOnlineSession->HasCompletedRound())
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
        // Mobile: waiting for the host/other lawns never replaces gameplay advice.

		return;
	}

	// 棋盘不在：单机 / 主机在这儿进关；客户端要等主机的 RUN_GO 放行——放行一到就进场
	// （草坪先建好，欠下的三选一由 ① 摆上去）。
	// 会话断了的客户端不在此列（IsOnlineClient 为假）：等不来放行，照旧按单机那条路进场。
	if (IsOnlineClient())
	{
		// @pvz-online: the client must wait for RUN_GO even when no old board remains.
		if (mOnlineRunGo)
		{
			
			EnterRunLevel();
		}
		return;
	}
	// @pvz-online: 点名已经发出去了（主机在等 START_ACK）就别再点——这条路口每帧都经过，
	// 不拦的话等待期每一帧都会再广播一次 START_LEVEL：队友那头"房主开始了游戏 / 是否加入"
	// 的框会一次次重新武装（答过的又弹出来，再答一次又是一个迟到的 ACK）。
	// 等 ACK 的收口在 UpdateOnlineStart ③：人齐在那儿直接进场，不经过这里。
	if (mOnlineWaitingStartAck) return;
	
            EnterRunLevel();
}

int LawnApp::OnlineSeatMultiplier()
{
	if (!IsOnlineGame() || mOnlineSession == nullptr) return 1;
	return OnlineSeatMultiplierForSeat(mOnlineSession->GetLocalSeat());
}

// @pvz-online: 按席位号取同一条顺位曲线（大厅换位预览用，2026-10-15）：换位不改变
// 上座席位集合，各席位号的乘数/阳光档不变——预览=直接查目标席位号的档。
int LawnApp::OnlineSeatMultiplierForSeat(uint8_t theSeat)
{
	if (!IsOnlineGame() || mOnlineSession == nullptr) return 1;

	int aSeatCount = mOnlineSession->GetOccupiedSeatCount();
	if (aSeatCount < 2) return 1;

	uint8_t aMySeat = theSeat;
	int aRank = 0;
	for (uint8_t aSeat = 1; aSeat <= aMySeat; aSeat++)
	{
		if (mOnlineSession->IsSeatOccupied(aSeat))
			aRank++;
	}
	int aSeatMult = 1 << (aSeatCount - aRank);	// 第 r 位 = 2^(n-r)：1 号位 2^(n-1)
	if (aSeatMult < 2) aSeatMult = 2;		// 末席保底 ×2（批七：末位出量翻倍，其余席位不动）
	if (aSeatMult > 32) aSeatMult = 32;		// 用户定封顶 32（六席顶正好 32）
	return aSeatMult;
}

static int SeatStartSunForMult(int theSeatMult)
{
	if (theSeatMult >= 32) return 500;
	if (theSeatMult >= 16) return 300;
	if (theSeatMult >= 8) return 200;
	if (theSeatMult >= 4) return 100;
	return 0;
}

int LawnApp::OnlineStartSunBonus()
{
	return SeatStartSunForMult(OnlineSeatMultiplier());
}

int LawnApp::OnlineStartSunBonusForSeat(uint8_t theSeat)
{
	return SeatStartSunForMult(OnlineSeatMultiplierForSeat(theSeat));
}

void LawnApp::DoOnlineDialog()
{
	if (!mOnlineSession)
	{
		mOnlineSession = std::make_unique<NetSession>();
		// 名字取本机档案里的玩家名：握手里报给对面，小条名册上"谁坐在几号位"就是它。
		// 会话只在这里新建，名字设一次就够——它不随每局收摊清掉（见 SetLocalName）。
		if (mPlayerInfo) mOnlineSession->SetLocalName(mPlayerInfo->mName.c_str());
	}

	InitializeAdmissionIdentity();
	KillDialog(Dialogs::DIALOG_ONLINE);

	OnlineDialog* aDialog = new OnlineDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_ONLINE, aDialog);
    mWidgetManager->SetFocus(aDialog);
}

static const int ZOMBIE_VOTE_TIMEOUT_FRAMES = 3000;

static std::string ZombieVoteBannerText(int theCount, const unsigned char* theIds)
{
	// @pvz-online: keep this UI consumer bounded even if an internal caller bypasses wire validation.
	if (theCount > 2) theCount = 2;
	std::string aText = ModText::Tr("本场景僵尸强化：", "Zombie boosts: ");
	if (theCount <= 0)
	{
		aText += ModText::Tr("无", "none");
		return aText;
	}
	for (int i = 0; i < theCount; i++)
	{
		if (i > 0) aText += ModText::Tr("、", ", ");
		aText += GetZombieBuffDisplayName(theIds[i]);
	}
	return aText;
}

bool LawnApp::IsLocalOnlyMode(GameMode theGameMode) const
{
	return theGameMode == GAMEMODE_CHALLENGE_ZEN_GARDEN
		|| theGameMode == GAMEMODE_TREE_OF_WISDOM
		|| theGameMode == GAMEMODE_INTRO
		|| theGameMode == GAMEMODE_CHALLENGE_ICE;
}

bool LawnApp::TryOpenZombieVote()
{
	// 票已经在开：谁走到"该进新场景"的路口都得先等这张票落地——房主的 START_LEVEL 要等
	// 票结清才发（层数在里头）；客户端也靠这一句挡住"结果还没到就先进了关"。
	if (mZombieVoteActive) return true;

	// 只有房主 / 单机开票；客户端收 OPEN 弹框（UpdateZombieVote），不在这儿开。
	if(mRunState==nullptr)return false;
    if(IsOnlineClient()) {
        const int level=mRunState->GetPlayingLevelIndex(),period=RunState::LevelsPerScene(mRunState->mMode);
        return level>0&&period>0&&level%period==0&&mRunState->GetZombieBuffPerScene()>0&&
            ((int)mRunState->mZombieVotedLevel!=level||(mZombieVoteRoundsDone>0&&mZombieVoteRoundsDone<mRunState->GetZombieBuffPerScene()));
    }
	// 追赶期不开（补做的是过去的关，票是"下一场景"的事；追上了自然轮到）。
	if (mRunState->IsCatchingUp()) return false;
	// 没开这一档（高级选项「僵尸 buff」默认 0）就不开票——没这回事，一帧账都不记。
	if (mRunState->GetZombieBuffPerScene() <= 0) return false;

	// 只在"该进的新场景"开票：关序号是每场景关数的整倍数（完整 5 / 普通 2 / 快速 1；
	// 无尽档越界回落 5）。第 1 关（序号 0）不开——那是开局，不是新场景。
	int aLevel = mRunState->GetPlayingLevelIndex();
	int aPerScene = RunState::LevelsPerScene(mRunState->mMode);
	if (aLevel <= 0 || aPerScene <= 0 || aLevel % aPerScene != 0) return false;
	// 两屏制（MOD_BUILD 56，与 PC 同批同口径）：档位 = 本场景票数。mZombieVotedLevel ==
	// aLevel 时分三种态：roundsDone <= 0 → 检查点/重开恢复，本场景已投完，不重投；
	// roundsDone >= 档位 → 全结；中间态 → 还差屏，继续开下一屏。mZombieVotedLevel !=
	// aLevel = 新场景首票，轮次与横幅累积归零。
	if ((int)mRunState->mZombieVotedLevel == aLevel)
	{
        if(mZombieVoteRoundsDone==0) {
            mZombieVoteRoundsDone=mRunState->mEndlessVoteRounds;
            mZombieVoteBannerCount=0;
            for(int i=0;i<2;++i)if(mRunState->mEndlessVoteIds[i]<RunState::RUN_ZOMBIE_BUFF_COUNT)
                mZombieVoteBannerIds[mZombieVoteBannerCount++]=mRunState->mEndlessVoteIds[i];
        }
		if (mZombieVoteRoundsDone <= 0 || mZombieVoteRoundsDone >= mRunState->GetZombieBuffPerScene())
			return false;
	}
	else
	{
		mZombieVoteRoundsDone = 0;
		mZombieVoteBannerCount = 0;
		mZombieVoteBannerIds[0] = 0;
		mZombieVoteBannerIds[1] = 0;
	}

	unsigned char aCandidates[RunState::RUN_ZOMBIE_VOTE_CHOICES];
	mRunState->RollZombieVoteChoices(aLevel, aCandidates);
        if(!mRunState->HasZombieBuffChoices()) {
            mRunState->mZombieVotedLevel=(unsigned short)aLevel;
            mZombieVoteRoundsDone=mRunState->GetZombieBuffPerScene();
            mRunState->mEndlessVoteRounds=mZombieVoteRoundsDone;
            if(mBoard)mBoard->PushCustomBanner(ModText::Tr("僵尸强化已全部达到上限，本次跳过。","All zombie boosts are capped; this ballot is skipped."));
            mRunState->Save(mPlayerInfo->mId); return false;
        }
	// 两屏制第二屏：候选剔除本场景已入的条目（延续单屏「取前 N 个不同候选」的语义——
	// 档 2 = 两条不同强化，不叠同条）。替换从"全池减已入"里按固定顺位取，不再开 RNG：
	// 候选本就随 OPEN 广播，两端不需要各自复算这一步。
	if (mZombieVoteBannerCount > 0)
	{
		unsigned char aLeft[RunState::RUN_ZOMBIE_BUFF_COUNT];
		int aLeftCount = 0;
		for (int i = 0; i < RunState::RUN_ZOMBIE_BUFF_COUNT; i++)
		{
			bool aIn = false;
			for (int b = 0; b < mZombieVoteBannerCount; b++)
				if (mZombieVoteBannerIds[b] == (unsigned char)i) { aIn = true; break; }
			if (!aIn && mRunState->CanAddZombieBuffStack(i)) aLeft[aLeftCount++] = (unsigned char)i;
		}
		for (int c = 0; c < RunState::RUN_ZOMBIE_VOTE_CHOICES; c++)
		{
			bool aDup = false;
			for (int b = 0; b < mZombieVoteBannerCount; b++)
				if (mZombieVoteBannerIds[b] == aCandidates[c]) { aDup = true; break; }
			if (!aDup) continue;
            aCandidates[c]=0xff;
			for (int l = 0; l < aLeftCount; l++)
			{
				bool aTaken = false;
				for (int c2 = 0; c2 < RunState::RUN_ZOMBIE_VOTE_CHOICES; c2++)
					if (c2 != c && aCandidates[c2] == aLeft[l]) { aTaken = true; break; }
				if (!aTaken) { aCandidates[c] = aLeft[l]; break; }
			}
		}
	}
        int compact=0;for(int c=0;c<3;++c)if(aCandidates[c]<RunState::RUN_ZOMBIE_BUFF_COUNT)aCandidates[compact++]=aCandidates[c];
        while(compact<3)aCandidates[compact++]=0xff;
        if(aCandidates[0]==0xff && aCandidates[1]==0xff && aCandidates[2]==0xff) {
            mRunState->mZombieVotedLevel=(unsigned short)aLevel;
            mZombieVoteRoundsDone=mRunState->GetZombieBuffPerScene();
            mRunState->mEndlessVoteRounds=mZombieVoteRoundsDone;
            mRunState->Save(mPlayerInfo->mId);return false;
        }

	// 房主广播 OPEN（候选以房主这份为准——两端同种子同算法本可以各推各的，但广播一下就
	// 不再要求"算法逐位一致"）；单机跳过协议直接弹框。
	if (IsOnlineGame() && mOnlineSession->GetRole() == NetSession::Role::HOST)
	{
		mOnlineSession->SendZombieVoteOpen((uint16_t)aLevel, aCandidates);
	}
	ZombieVoteOpenLocal(aLevel, aCandidates);
	return true;
}

void LawnApp::ZombieVoteOpenLocal(int theLevel, const unsigned char* theCandidates)
{
	// 重入防御：同一轮重复 Open 直接忽略（OPEN 重发 / 迟到都别把已经投过的一票刷掉）。
	if (mZombieVoteActive) return;

	mZombieVoteOpenRound=mZombieVoteRoundsDone;
	mZombieVoteActive = true;
	mZombieVoteLevel = theLevel;
	for (int i = 0; i < RunState::RUN_ZOMBIE_VOTE_CHOICES; i++)
	{
		mZombieVoteCandidates[i] = theCandidates[i];
		mZombieVoteVotes[i] = 0;
	}
	mZombieVoteAbstains = 0;
	mZombieVoteVotedSeats = 0;
	mZombieVoteSeatMask = 0;
	mZombieVoteMyChoice = -1;
	mZombieVoteFrames = 0;

	if (GetDialog(Dialogs::DIALOG_ZOMBIE_VOTE) == nullptr)
	{
		ZombieVoteDialog* aDialog = new ZombieVoteDialog(this);
		CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
        AddDialog(Dialogs::DIALOG_ZOMBIE_VOTE, aDialog);
	}
	NetworkLog("[vote] opened for scene %d: candidates %d %d %d", theLevel,
		(int)mZombieVoteCandidates[0], (int)mZombieVoteCandidates[1], (int)mZombieVoteCandidates[2]);
}

void LawnApp::ZombieVoteRegister(int theSeat, int theChoice)
{
    if (theChoice < 0 || theChoice > 3 || (theChoice < 3 && (!mRunState || !mRunState->CanAddZombieBuffStack(mZombieVoteCandidates[theChoice])))) return;
	if (theSeat < 0 || theSeat >= 32) return;
	unsigned int aBit = 1u << theSeat;
	if (mZombieVoteSeatMask & aBit) return;		// 一席一票（重发 / 第二次点击不重计）
	mZombieVoteSeatMask |= aBit;
	mZombieVoteVotedSeats++;
	if (theChoice >= 0 && theChoice < RunState::RUN_ZOMBIE_VOTE_CHOICES) mZombieVoteVotes[theChoice]++;
	else mZombieVoteAbstains++;
}

unsigned int LawnApp::ZombieVoteRequiredSeatMask()
{
	// 单机：就本端一票。
	const unsigned int aLocalBit = 1u << (mOnlineSession ? mOnlineSession->GetLocalSeat() : 0);
	if (!IsOnlineGame()) return aLocalBit;

	// 房主：应投的席位 = 已上座且不在掉线宽限里的。掉线宽限席按弃权收口（不拖满 30 秒），
	// 中途退房的席位同理——名册一变，UpdateZombieVote 的每帧检查就会让这一票立刻结算。
	unsigned int aMask = 0;
	for (int i = 1; i <= NetProto::MAX_PLAYERS; i++)
	{
		if (mOnlineSession->IsSeatOccupied((uint8_t)i) && !mOnlineSession->IsSeatOffline((uint8_t)i)) aMask |= 1u << i;
	}
	return aMask ? aMask : aLocalBit;
}

int LawnApp::ZombieVoteExpectedVoters()
{
	unsigned int aMask = ZombieVoteRequiredSeatMask();
	int aCount = 0;
	for (; aMask; aMask &= aMask - 1) aCount++;
	return aCount;
}

void LawnApp::ZombieVoteResolve()
{
	if (!mZombieVoteActive || mRunState == nullptr) return;
    bool anyCandidate=false;
    for(int i=0;i<RunState::RUN_ZOMBIE_VOTE_CHOICES;++i){
        if(!mRunState->CanAddZombieBuffStack(mZombieVoteCandidates[i])){mZombieVoteCandidates[i]=255;mZombieVoteVotes[i]=0;}
        else anyCandidate=true;
    }
    if(!anyCandidate){ZombieVoteCloseUI();TryOpenZombieVote();return;}

	// 两屏制（MOD_BUILD 56，与 PC 同批同口径）：每屏一票一条件——原「按票数取前 aWant 个
	// 不同候选」的档位拆到屏上：档 1 一屏一条、档 2 同场景两屏各一条。
	unsigned char aPicked[RunState::RUN_ZOMBIE_VOTE_CHOICES];
	int aPickedCount = 0;
	int aTotalVotes = 0;
	for (int i = 0; i < RunState::RUN_ZOMBIE_VOTE_CHOICES; i++) aTotalVotes += mZombieVoteVotes[i];

	if (aTotalVotes > 0)
	{
		// 有票：票数最多的候选 1 条；票数相同按候选序（候选本就是种子洗过的，"种子序"
		// 就是下标序）。
		int aBest = -1;
		for (int i = 0; i < RunState::RUN_ZOMBIE_VOTE_CHOICES; i++)
		{
			if (mZombieVoteVotes[i] <= 0 || mZombieVoteCandidates[i]>=RunState::RUN_ZOMBIE_BUFF_COUNT) continue;
			if (aBest < 0 || mZombieVoteVotes[i] > mZombieVoteVotes[aBest]) aBest = i;
		}
		if (aBest >= 0) aPicked[aPickedCount++] = mZombieVoteCandidates[aBest];
	}
	if (aPickedCount <= 0 && mZombieVoteAbstains > 0)
	{
		// PC Build 47 per-screen carry-over: abstaining still grants one random candidate.
		// 两屏制下每屏独立结算：档 2 全弃权 = 两屏各随机 1 条。
		aPicked[aPickedCount++] = mZombieVoteCandidates[([&](){int ids[3],n=0;for(int i=0;i<3;++i)if(mZombieVoteCandidates[i]<RunState::RUN_ZOMBIE_BUFF_COUNT)ids[n++]=i;return ids[Rand(n)];})()];
	}
	if (aPickedCount <= 0)
	{
		// 全队一个人都没表态（纯超时）：种子兜底取候选首位——票开了就得有结果，不能因为
		// 没人理这事儿把全队钉住（等待必须有出路）。
		for(int i=0;i<RunState::RUN_ZOMBIE_VOTE_CHOICES;++i)if(mZombieVoteCandidates[i]<RunState::RUN_ZOMBIE_BUFF_COUNT){aPicked[aPickedCount++]=mZombieVoteCandidates[i];break;}
	}

	mRunState->AddZombieBuffStack(aPicked[0]);
	// 结果入横幅累积（两屏两条合一推一条）；轮数推进 + 记已投关号（重试同关不重开）。
	if (mZombieVoteBannerCount < 2) mZombieVoteBannerIds[mZombieVoteBannerCount++] = aPicked[0];
	mZombieVoteRoundsDone++;
	mRunState->mZombieVotedLevel = (unsigned short)mZombieVoteLevel;
    {
        mRunState->mEndlessVoteRounds=mZombieVoteRoundsDone;
        for(int i=0;i<2;++i)mRunState->mEndlessVoteIds[i]=i<mZombieVoteBannerCount?mZombieVoteBannerIds[i]:255;
        mRunState->Save(mPlayerInfo->mId);
    }

	// RESULT 广播（房主）：先于下一张 OPEN / START_LEVEL 发——公示与层数快照不拧巴。
	if (IsOnlineGame() && mOnlineSession->GetRole() == NetSession::Role::HOST)
	{
		uint8_t aIds[2] = { 0, 0 };
		aIds[0] = aPicked[0];
		mOnlineSession->SendZombieVoteResult((uint16_t)mZombieVoteLevel, (uint8_t)aPickedCount, aIds);
	}

	// 结果横幅：每屏结算都重设为累积文本（第二屏结算后自然含两条）；横幅仍在草坪开打后
	// 才推（UpdateZombieVote 每帧看）——投票期间 mZombieVoteActive 挡着，不会提前出。
	mZombieVotePendingBanner = ZombieVoteBannerText(mZombieVoteBannerCount, mZombieVoteBannerIds);
	NetworkLog("[vote] resolved round %d/%d for scene %d: %d boost(s) - %s", mZombieVoteRoundsDone,
		mRunState->GetZombieBuffPerScene(), mZombieVoteLevel, aPickedCount,
		mZombieVotePendingBanner.c_str());
	ZombieVoteCloseUI();

	// 两屏制：还有没结的屏就在同一空档连开（房主/单机；客户端等下一张 OPEN）。
	if (mZombieVoteRoundsDone < mRunState->GetZombieBuffPerScene())
		TryOpenZombieVote();
}

void LawnApp::ZombieVoteCloseUI()
{
	KillDialog(Dialogs::DIALOG_ZOMBIE_VOTE);
	mZombieVoteActive = false;
}

void LawnApp::ResetZombieVote()
{
	// @pvz-online: an exit can interrupt a vote before RESULT; clear its UI and every per-run record.
	ZombieVoteCloseUI();
	mZombieVoteLevel = -1;
	mZombieVoteMyChoice = -1;
	mZombieVoteFrames = 0;
	mZombieVoteAbstains = 0;
	mZombieVoteVotedSeats = 0;
	mZombieVoteSeatMask = 0;
	mZombieVoteRoundsDone = 0;
	mZombieVoteBannerCount = 0;
	mZombieVoteBannerIds[0] = 0;
	mZombieVoteBannerIds[1] = 0;
	for (int i = 0; i < RunState::RUN_ZOMBIE_VOTE_CHOICES; i++)
	{
		mZombieVoteCandidates[i] = 0;
		mZombieVoteVotes[i] = 0;
	}
	mZombieVotePendingBanner.clear();
}

void LawnApp::ZombieVoteChosen(int theChoice)
{
	if (!mZombieVoteActive) return;
    if(theChoice<0 || theChoice>3 || (theChoice<3 && (!mRunState || !mRunState->CanAddZombieBuffStack(mZombieVoteCandidates[theChoice]))))return;
	// 一席一票：本端已经投过就忽略（投票框点过就把按钮锁上了，这里是双保险）。
	if (mZombieVoteMyChoice >= 0) return;
	mZombieVoteMyChoice = theChoice;

	if (IsOnlineClient())
	{
		mOnlineSession->SendZombieVote((uint16_t)mZombieVoteLevel, (uint8_t)theChoice);
	}
	else
	{
		ZombieVoteRegister(mOnlineSession != nullptr ? (int)mOnlineSession->GetLocalSeat() : 0, theChoice);
	}
}

std::string LawnApp::ZombieVoteStatusText()
{
	int aSeconds = (ZOMBIE_VOTE_TIMEOUT_FRAMES - mZombieVoteFrames + 99) / 100;
	if (aSeconds < 0) aSeconds = 0;

	char aBuf[128];
	if (IsOnlineClient())
	{
		snprintf(aBuf, sizeof(aBuf), ModText::Tr("还剩 %d 秒 · %s", "%ds left - %s"), aSeconds,
			mZombieVoteMyChoice >= 0 ? ModText::Tr("已投", "voted") : ModText::Tr("点一项投票", "choose a card"));
	}
	else
	{
		unsigned int aReceived = mZombieVoteSeatMask & ZombieVoteRequiredSeatMask();
		int aCurrentVoted = 0;
		for (; aReceived; aReceived &= aReceived - 1) aCurrentVoted++;
		snprintf(aBuf, sizeof(aBuf), ModText::Tr("还剩 %d 秒 · 已投 %d/%d", "%ds left - voted %d/%d"),
			aSeconds, aCurrentVoted, ZombieVoteExpectedVoters());
	}
	return std::string(aBuf);
}

void LawnApp::UpdateZombieVote()
{
    if (mOnlineSession && mOnlineSession->IsReconnecting()) return;
	// ① 客户端：收 OPEN（弹框）与 RESULT（公示 + 关框）。层数不在这儿动——等 START_LEVEL。
	if (IsOnlineClient() && mOnlineSession != nullptr)
	{
        for (;;) {
            NetProto::MsgZombieVoteOpen opening;
            NetProto::MsgZombieVoteResult result;
            if (mOnlineSession->TakePendingZombieVoteOpen(opening)) {
                if (mRunState && !mZombieVoteActive) {
                    if (mZombieVoteLevel != (int)opening.mSceneIndex &&
                        (int)mRunState->mZombieVotedLevel != (int)opening.mSceneIndex) {
                        mZombieVoteRoundsDone = 0;
                        mZombieVoteBannerCount = 0;
                    }
                    if (mZombieVoteRoundsDone < mRunState->GetZombieBuffPerScene())
                        ZombieVoteOpenLocal((int)opening.mSceneIndex, opening.mCandidates);
                }
            } else if (mOnlineSession->TakePendingZombieVoteResult(result)) {
                if (!mRunState || !mZombieVoteActive || mZombieVoteLevel != (int)result.mSceneIndex)
                    continue;
                // Commit the same authoritative RESULT locally before a host can
                // disappear. A SHARED snapshot may already contain this round.
                if(mZombieVoteRoundsDone<=mZombieVoteOpenRound)
                    for(unsigned int index=0;index<result.mCount;++index)mRunState->AddZombieBuffStack(result.mIds[index]);
                mRunState->mZombieVotedLevel = result.mSceneIndex;
                for (unsigned int index = 0; index < result.mCount && mZombieVoteBannerCount < 2; ++index)
                    mZombieVoteBannerIds[mZombieVoteBannerCount++] = result.mIds[index];
                mZombieVotePendingBanner = ZombieVoteBannerText(mZombieVoteBannerCount, mZombieVoteBannerIds);
                mZombieVoteRoundsDone=std::max(mZombieVoteRoundsDone,mZombieVoteOpenRound+1);
                mRunState->mEndlessVoteRounds=mZombieVoteRoundsDone;
                for(int i=0;i<2;++i)mRunState->mEndlessVoteIds[i]=i<mZombieVoteBannerCount?mZombieVoteBannerIds[i]:255;
                if(mPlayerInfo)mRunState->Save(mPlayerInfo->mId);
                ZombieVoteCloseUI();
            } else break;
        }
	}
	// 房主：收队友的票记账（本机这一票在 ZombieVoteChosen 里直接记，不走网络）。
	else if (mOnlineSession != nullptr)
	{
		NetProto::MsgZombieVote aVote;
		while (mOnlineSession->TakePendingZombieVote(aVote))
		{
			if (!mZombieVoteActive || (int)aVote.mSceneIndex != mZombieVoteLevel) continue;
			ZombieVoteRegister((int)aVote.mSrcSeat, (int)aVote.mChoice);
		}
	}

	if (!mZombieVoteActive)
	{
		// ② 结果横幅：等草坪真的开打（板子在、开场放开、场景已是游玩）再推——押后见上面。
		if (!mZombieVotePendingBanner.empty() && mBoard != nullptr && !mRunIntroHeld
			&& mGameScene == GameScenes::SCENE_PLAYING)
		{
			mBoard->PushCustomBanner(mZombieVotePendingBanner.c_str());
			mZombieVotePendingBanner.clear();
		}
		return;
	}

	mZombieVoteFrames++;

	// ③ 收口。房主 / 单机每帧看"应投的都投了没有"——这么写一并覆盖"掉线宽限席按弃权"
	// 与"开到一半有人退房"：名册一动，下面这个等式就成立；超时到点也走同一条结算。
	if (!IsOnlineClient())
	{
		// @pvz-online: a departed voter's count must not stand in for a still-online player's missing vote.
		const unsigned int aRequired = ZombieVoteRequiredSeatMask();
		if ((mZombieVoteSeatMask & aRequired) == aRequired
			|| mZombieVoteFrames > ZOMBIE_VOTE_TIMEOUT_FRAMES)
		{
			ZombieVoteResolve();
		}
	}
	// 客户端只留一条本地兜底：RESULT 迟迟不来（房主掉线 / 丢包）也别把框和"等票"的路口
	// 永远钉住——到两倍超时就把框撤了（层数反正只信 START_LEVEL，撤框不伤数值）。
	else if (mZombieVoteFrames > ZOMBIE_VOTE_TIMEOUT_FRAMES * 2)
	{
		NetworkLog("[vote] no result from the host - closing the vote dialog locally");
		ZombieVoteCloseUI();
	}
}

#include "LawnMower.h"
#include "Online/PreparationGame.inl"

#include "Online/AdmissionGame.inl"
