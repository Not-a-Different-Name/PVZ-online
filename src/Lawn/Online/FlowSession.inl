// Flow v1 is negotiated with the relay. It does not change direct connections.
bool NetSession::SendFlowProgress(uint8_t op,bool value,uint32_t generation) {
    if(!IsConnected() || !mFlowEnabled || !mReadyEpoch || (IsWaitingForNextRound(mLocalSeat) && !(op==5 && value)))return false;
    uint8_t bytes[14];NetProto::Writer w(bytes,sizeof(bytes));
    w.U32(mFlowTerm);w.U32(mReadyEpoch);w.U8(op);w.U8(value?1:0);w.U32(generation);
    return SendRaw(NetProto::MSG_CLI_FLOW_PROGRESS,bytes,sizeof(bytes));
}
bool NetSession::SendFlowStart(const NetProto::MsgStartLevel& msg) {
    uint8_t bytes[8+START_LEVEL_PAYLOAD_SIZE];NetProto::Writer w(bytes,sizeof(bytes));w.U32(mFlowTerm);w.U32(msg.mReadyEpoch);
    int n=NetProto::EncodeStartLevel(bytes+8,START_LEVEL_PAYLOAD_SIZE,msg);
    return n==START_LEVEL_PAYLOAD_SIZE && SendRaw(NetProto::MSG_CLI_FLOW_START,bytes,sizeof(bytes));
}
bool NetSession::SendFlowLevelDone(bool clear) {
    if(!IsConnected() || IsWaitingForNextRound(mLocalSeat) || mReadyEpoch!=mFlowEpoch)return false;
    if(mFlowDoneEpoch!=mReadyEpoch || mFlowDoneValue!=clear) {
        if(!SendFlowProgress(1,clear))return false;
        mFlowDoneEpoch=mReadyEpoch;mFlowDoneValue=clear;
        mSeats[mLocalSeat].mLevelDone=clear;
    }
    // Called only after the app rechecks its board, BOSSes and queued escapes.
    if(clear && mFlowPhase==3 && mFlowAcked!=mFlowGeneration) {
        if(!SendFlowProgress(2,true,mFlowGeneration))return false;
        mFlowAcked=mFlowGeneration;
    }
    return true;
}
void NetSession::HandleFlowState(const uint8_t* bytes,int size) {
    if(!IsRelay() || !IsConnected() || size!=24)return;
    NetProto::Reader r(bytes,size);
    uint16_t version=r.U16();uint8_t flags=r.U8(),phase=r.U8(),host=r.U8(),online=r.U8(),occupied=r.U8(),waiting=r.U8();
    uint32_t term=r.U32(),epoch=r.U32(),generation=r.U32();
    uint8_t done=r.U8(),acks=r.U8(),applied=r.U8(),reserved=r.U8();
    (void)acks;(void)applied;
    if(version!=1 || (flags&~3) || phase>5 || !term || term<mFlowTerm || host<1 || host>NetProto::MAX_PLAYERS ||
        (online&0x81) || (occupied&0x81) || (online&~occupied) || (waiting&~occupied) ||
        !(online&(1u<<mLocalSeat)) || !(online&(1u<<host)) || reserved)return;
    const bool changed=mFlowTerm && term!=mFlowTerm;
    const bool newHost=mHostSeat!=host;
    // Authority can change after GO but before a slow app consumes it. Only
    // retain a locally witnessed, committed preparation for this exact START.
    // Server battle phase alone never proves that this player has prepared.
    const auto committed=mPreparation;
    uint8_t committedReady=0;
    const bool sameCommittedRound=(changed || newHost) && mFlowEnabled && !mFlowLost &&
        (flags&1) && !(flags&2) && phase==2 && epoch && epoch==mFlowEpoch &&
        epoch==mReadyEpoch && committed.epoch==epoch && committed.local &&
        committed.phase==PreparationWire::GO && mLastStartLevelValid &&
        mLastStartLevel.mReadyEpoch==epoch &&
        (mRole==Role::HOST || mConsumedStartEpoch==epoch) &&
        !(mRoundWaitMask&(1u<<mLocalSeat)) && !(waiting&(1u<<mLocalSeat)) &&
        IsSeatSeedsReady(mLocalSeat) && (committed.ready&(1u<<mLocalSeat)) &&
        committed.participants[mLocalSeat]==mCoopToken;
    if(sameCommittedRound)for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s) {
        const uint8_t bit=uint8_t(1u<<s);
        const uint64_t player=s==mLocalSeat?mCoopToken:mCoopPeers[s];
        if((online&bit) && !(waiting&bit) && !(mRoundWaitMask&bit) &&
            (committed.entered&bit) && (committed.ready&bit) &&
            IsSeatSeedsReady(s) && player && committed.participants[s]==player)
            committedReady|=bit;
    }
    mFlowSupported=true;mFlowEnabled=(flags&1)!=0;mFlowLost=(flags&2)!=0;mFlowTerm=term;mFlowEpoch=epoch;
    mFlowPhase=phase;mFlowGeneration=generation;mRoundWaitMask=waiting;
    if(mFlowStartPending && epoch==mReadyEpoch)mFlowStartPending=false;
    for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s) {
        mSeats[s].mOccupied=(occupied&(1u<<s))!=0;
        mSeats[s].mOffline=(occupied&(1u<<s)) && !(online&(1u<<s));
        if(mSeats[s].mOffline || (waiting&(1u<<s)))mSeats[s].mAwaitingAck=false;
        if(epoch==mReadyEpoch)mSeats[s].mLevelDone=(done&(1u<<s))!=0;
    }
    // A late escape can revoke a clear even when the board kills it within one frame.
    // Ask the app to re-report only after its next real board recheck, never here.
    if(epoch==mReadyEpoch && mFlowDoneEpoch==epoch && mFlowDoneValue &&
        !(done&(1u<<mLocalSeat)))mFlowDoneEpoch=0;
    // The relay's lobby transition cancels a board, not a victory. Preserve
    // a pending LEVEL_EXIT so the app still returns its actual board to menu.
    if(phase==0 && epoch) {
        EndPreparation();ClearSwapState();mFlowDeferred=false;mFlowStartPending=false;
        mHasPendingStart=false;mHasRunGo=false;mAnyAckRejected=false;
        mLastStartLevelValid=false;mGateWanted=-1;mGateAck=0;
        ClearPauseState();ClearLevelDoneState();
    }
    if(waiting&(1u<<mLocalSeat))mFlowDeferred=true;
    else if(phase==0 || phase==5 || (phase==1 && epoch==mReadyEpoch && mConsumedStartEpoch==epoch))mFlowDeferred=false;
    mHostSeat=host;mRole=mLocalSeat==host?Role::HOST:Role::CLIENT;
    if(changed || newHost) {
        mFlowAcked=0;mFlowDoneEpoch=0;mFlowStartPending=false;
        mFlowHostStartRecovery=mRole==Role::HOST && phase==1 && !mPreparation.local;
        mHasRunGo=false;mHasPendingStart=false;mAnyAckRejected=false;
        mPendingZombieVoteOrder.clear();mPendingZombieVoteOpens.clear();mPendingZombieVotes.clear();mPendingZombieVoteResults.clear();
        mAdmissionInbox.clear();ClearSwapState();
        mGateWanted=-1;mGateRequest=mGateAck=0;mGateRetry=0;
        if(mReadyCancelState==ReadyCancelState::PENDING){mReadyCancelTerm=term;mReadyCancelRevision=0;}
        // Forget transaction consent, keep only the actual server-confirmed map.
        bool local=mPreparation.local,managed=mPreparation.gameManaged;
        mPreparation=PreparationWire::StateData{};mPreparation.local=local;
        mPreparation.gameManaged=managed;mPreparation.epoch=mReadyEpoch;
        if(phase>=2)mPreparation.phase=PreparationWire::GO;
        if(committedReady&(1u<<mLocalSeat)) {
            // Keep the final revision so app-side configuration/save remains
            // idempotent; discard all old authority's transaction consent.
            mPreparation.revision=committed.revision;
            mPreparation.configuredRevision=committed.configuredRevision;
            mPreparation.entered=mPreparation.ready=committedReady;
            mPreparation.occupiedMask=committedReady;
            for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)
                if(committedReady&(1u<<s))mPreparation.participants[s]=committed.participants[s];
        }
        if(phase<2)for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)mCoopPeers[s]=s==mLocalSeat?mCoopToken:0;
        for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s) {
            mSeats[s].mSeedsReady=(mPreparation.ready&(1u<<s))!=0;
            if(mSeats[s].mSeedsReady)mSeats[s].mSeedsReadyEpoch=epoch;
            mSeats[s].mAwaitingAck=false;
        }
        if(mFlowHostStartRecovery && mLastStartLevelValid) {
            mPendingStart=mLastStartLevel;mHasPendingStart=true;
            mLastStartLevel.mSrcSeat=mLocalSeat;
            for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)
                if(s!=mLocalSeat && (online&(1u<<s)) && !(waiting&(1u<<s)))ResendLastStartLevel(s);
        }
        PushEvent(EventType::HOST_CHANGED,host);
    }
    UpdateReadyCancelState();
    if(mFlowEnabled && !mFlowHelloSent && mRole==Role::CLIENT) {
        mFlowHelloSent=true;AdmissionWire::Message hello;hello.op=AdmissionWire::HELLO;
        SendAdmission(hello,mHostSeat);
    }
}

bool NetSession::CanCancelSeedsReady() const {
 return IsConnected() && IsPreparationActive() && !IsWaitingForNextRound(mLocalSeat) &&
  (mPreparation.phase==PreparationWire::OPEN || mPreparation.phase==PreparationWire::SEAL);
}
bool NetSession::RequestSeedsReadyCancel() {
 if(mReadyCancelState==ReadyCancelState::PENDING)return false;
 if(!CanCancelSeedsReady()){mReadyCancelState=ReadyCancelState::TOO_LATE;return false;}
 mReadyCancelState=ReadyCancelState::PENDING;mReadyCancelEpoch=mReadyEpoch;
 mReadyCancelTerm=mFlowTerm;mReadyCancelRevision=mPreparation.revision;
 if(!SendSeedsReady(false)){mReadyCancelState=ReadyCancelState::NONE;return false;}
 UpdateReadyCancelState();return true;
}
void NetSession::UpdateReadyCancelState() {
 if(mReadyCancelState!=ReadyCancelState::PENDING)return;
 if(mReadyEpoch!=mReadyCancelEpoch || mPreparation.phase==PreparationWire::GO || (mFlowEnabled && mFlowEpoch==mReadyCancelEpoch && mFlowPhase>=2)){mReadyCancelState=ReadyCancelState::TOO_LATE;return;}
 if(mFlowTerm!=mReadyCancelTerm)return;
 if(mPreparation.phase==PreparationWire::OPEN && mPreparation.revision>=mReadyCancelRevision &&
  !(mPreparation.ready&(1u<<mLocalSeat)))mReadyCancelState=ReadyCancelState::CONFIRMED;
}

// @pvz-online 2026-10-25：房主放弃这次开局（队友拒绝 / 超时 / 自己取消）。
// FLOW_START 被中继收下的那一刻房间就进了 flowSetup 相——成员答不答与它无关；
// 人都回了菜单就再没人退关，returnFlowToMenu 永远不来，房间卡在 1 相，
// CanHostStart 只认大厅/终场相，开局键从此按不亮（2026-10-25 玩家报障）。
// op5「回菜单」在大厅相是空操作（returnFlowToMenu 见 lobby 直接 return），
// 所以每次放弃都发一发不会误伤；应答记账一并作废，防迟到的 ACK 或断线补发
// 把这条死掉的开局又复活（PEER_BACK 的 ResendLastStartLevel 只认 mAwaitingAck）。
void NetSession::AbortFlowStartAttempt() {
	mFlowStartPending = false;
	mAnyAckRejected = false;
	for (uint8_t seat = 1; seat <= NetProto::MAX_PLAYERS; ++seat) {
		mSeats[seat].mAwaitingAck = false;
		mSeats[seat].mAckState = 0;
	}
	// 直连房间没有 flow 相（mFlowEnabled 恒假）天然跳过；断线时发不出去也不影响本地收摊。
	if (mFlowEnabled && IsConnected() && SendFlowProgress(5, true))
		NetworkLog("[net] the start attempt was aborted - the room goes back to the lobby");
}

// 成员侧：中继已把房间收回大厅（flow 回到 lobby 相）而本机刚好消费过那一条 START——
// 房主放弃了这次开局，还挂着的「是否加入」该收掉了，再点「加入」只会一个人进场。
bool NetSession::StartAttemptRolledBack() const {
	return mFlowEnabled && mConsumedStartEpoch && mFlowPhase == 0 && mFlowEpoch == mConsumedStartEpoch;
}

// 主机侧：房间停在 setup 相、本机却没有任何在途的开局——上次放弃发出的 op5 丢了。
// 已进场的那扇窗（点齐 ACK → 建棋盘 → 首个 op1 之前）不会误入：那时候应用侧有棋盘，
// 自救的调用门（在菜单、无棋盘）把它挡在外面；迁移接任的相 1 被恢复记号排除。
bool NetSession::IsOrphanFlowSetup() const {
	return mFlowEnabled && mFlowPhase == 1 && mReadyEpoch && mFlowEpoch == mReadyEpoch &&
		!mFlowStartPending && !mFlowHostStartRecovery && !mHasPendingStart;
}

void NetSession::RecoverOrphanFlowSetup() {
	if (mFlowEnabled && IsConnected() && SendFlowProgress(5, true))
		NetworkLog("[net] an orphaned setup was reclaimed - the room goes back to the lobby");
}
