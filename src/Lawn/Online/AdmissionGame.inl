#include "../Run/EndlessLawn.h"
#include "../SeedPacket.h"
#include "../Plant.h"
#include "../Widget/SeedChooserScreen.h"
#include "../System/PlayerInfo.h"
#include "misc/Buffer.h"

// @pvz-online: the profile identity is durable; room/round IDs still reject delayed game messages.
void LawnApp::InitializeAdmissionIdentity() {
    if(!mPlayerInfo || !mOnlineSession || mAdmissionIdentityProfile==mPlayerInfo->mId)return;
    const auto path=RunState::GetCheckpointName(mPlayerInfo->mId)+".player-id";
    Sexy::Buffer bytes;uint64_t token=0;
    if(ReadBufferFromFile(path,&bytes,false) && bytes.GetDataLen()==16) {
        const auto* p=(const unsigned char*)bytes.GetDataPtr();const auto* q=p;
        token=Admission::Get(q,8);if(Admission::Get(q,8)!=~token)token=0;
    }
    if(!token) {
        token=Admission::NewId();std::vector<unsigned char> out;Admission::Put(out,token,8);Admission::Put(out,~token,8);
        MkDir(GetAppDataFolder()+"userdata");
        if(!RunCheckpointIO::AtomicWrite(path,out))return;
    }
    mOnlineSession->SetPersistentPlayer(token);mAdmissionIdentityProfile=mPlayerInfo->mId;
}

bool LawnApp::AdmissionBuilding() const {
    return mRunState && mRunState->IsNewEndless() && mRunState->mAdmission.Building();
}

void LawnApp::StartAdmissionFastPicks() {
    if(!mRunState || mRunState->mAdmission.phase!=Admission::PICKS)return;
    mRunState->mAdmission.fast=true;mAdmissionDirty=true;
#ifdef __ANDROID__
    KillRunFlowScreen();
#else
    KillDialog(DIALOG_RUN_PICK);
#endif
}

void LawnApp::BeginAdmissionBuilding() {
    if(!mBoard || !mRunState || mRunState->mAdmission.phase!=Admission::DECK)return;
    mRunState->mAdmission.phase=Admission::BUILD;
    mRunState->mEndlessFocusMask=0;
    for(int i=0;i<mBoard->mSeedBank->mNumPackets;++i) {
        const auto& packet=mBoard->mSeedBank->mSeedPackets[i];
        mRunState->mEndlessFocusMask|=EndlessLawn::LineupSeedBit(packet.mPacketType,packet.mImitaterType);
    }
    KillSeedChooserScreen();KillDialog(DIALOG_ONLINE_START);
    mBoard->mCutScene->mSeedChoosing=false;mBoard->Move(0,0);
    mBoard->mCutScene->PreloadResources();mBoard->mCutScene->PlaceLawnItems();
    mBoard->mEnableGraveStones=true;mBoard->mSeedBank->mCutSceneDarken=0;
    mBoard->ClearAdviceImmediately();
    mBoard->mSeedBank->Move(10,0);mBoard->mSeedBank->mVisible=true;
    mBoard->mMenuButton->mBtnNoDraw=false;mBoard->mShowShovel=true;
    // @pvz-online: construction owns the lower HUD and cannot act on a running battle's panels.
    mDamageStats.mOpen=0;mBoard->mChatPanelOpen=false;mBoard->mSunGiftSelectFrames=0;
    mGameScene=SCENE_PLAYING;mRunIntroHeld=true;mOnlineSeedsHeld=false;
    if(mWidgetManager)mWidgetManager->SetFocus(mBoard);
    mAdmissionDirty=true;
}

void LawnApp::FinishAdmissionBuilding() {
    if(!AdmissionBuilding() || !mBoard || mAdmissionDirty || !IsOnlineGame() || mBoard->mPaused)return;
    auto& state=mRunState->mAdmission;const int budget=state.budget;
    state.phase=Admission::PREPARED;state.budget=0;
    mGameScene=SCENE_LEVEL_INTRO;mRunIntroHeld=true;mOnlineSeedsHeld=true;
    if(!EndlessLawn::Capture(mBoard,true) || !mRunState->Save(mPlayerInfo->mId)) {
        state.phase=Admission::BUILD;state.budget=budget;mGameScene=SCENE_PLAYING;mOnlineSeedsHeld=false;
        mAdmissionDirty=true;return;
    }
    if(!mOnlineSession || !mOnlineSession->IsConnected())return;
    mOnlineSession->BeginPreparation(true);mOnlineSession->SendSeedsReady(false);
    mPreparationRulesApplied=false;mPreparationSaved=false;mPreparationConfiguredRevision=0;
    mPreparationBuiltSeat=mOnlineSession->GetLocalSeat();mPreparationBuiltMultiplier=OnlineSeatMultiplier();
}

// Returns true only for an authenticated, matching room offer; normal continue never grants resources.
void LawnApp::ApplyAdmissionOffer(int targetIndex) {
    if(!mRunState || !mOnlineSession || !mRunState->IsNewEndless())return;
    const auto& offer=mOnlineSession->mAdmissionOffer;
    if(!mOnlineSession->HasAdmissionOffer(targetIndex) || offer.player!=mOnlineSession->mCoopToken || offer.level!=uint32_t(targetIndex) || !offer.run)return;
    auto& state=mRunState->mAdmission;
    if(state.run==offer.run && state.player==offer.player) {
        if(state.phase==Admission::NONE){state.phase=Admission::PLAYED;state.joined=0;mAdmissionDirty=true;}
        int previous=mRunState->mLevelIndex;
        if(targetIndex>previous) {
            int delta=Admission::PicksBefore(targetIndex,RunState::BuffMultValueFor(mRunState->mBuffMult),mRunState->EndlessBossPeriod())-
                Admission::PicksBefore(previous,RunState::BuffMultValueFor(mRunState->mBuffMult),mRunState->EndlessBossPeriod());
            mRunState->mPendingBuffPicks+=std::max(0,delta);
            mRunState->mLevelIndex=targetIndex;mRunState->mCatchUpLevel=-1;
            if(state.Pending())state.phase=Admission::PICKS;
            mAdmissionDirty=true;
        }
        return;
    }
    state=Admission::State{};state.run=offer.run;state.player=offer.player;
    state.joined=int(offer.joined);state.totalBudget=state.budget=int(offer.budget);state.phase=Admission::PICKS;
    mRunState->mLevelIndex=targetIndex;mRunState->mCatchUpLevel=-1;
    mRunState->mPendingPlantPicks=0;
    mRunState->mPendingBuffPicks=Admission::PicksBefore(targetIndex,RunState::BuffMultValueFor(mRunState->mBuffMult),mRunState->EndlessBossPeriod());
    mRunState->mChoicesRolled=false;mRunState->mPickCounter=0;
    // Candidate generation is fixed for this player's admission, including across process restarts.
    mRunState->mPickSalt=uint32_t(offer.run)^uint32_t(offer.player)^uint32_t(offer.player>>32);
    mRunState->mEndlessLawn.clear();mRunState->mEndlessRetryLawn.clear();mRunState->mDamageHistory.clear();
    mRunState->mEndlessSnapshotLevel=-1;mRunState->mEndlessSnapshotPhase=0;
    mDamageStats.Reset();mAdmissionDirty=true;
}

void LawnApp::SendAdmissionStart(uint8_t seat) {
    auto* run=GetRunState();
    mOnlineSession->SendStartLevel((uint8_t)GAMEMODE_ADVENTURE,(uint32_t)run->GetLevel(),run->GetLevelSeed(),
        true,run->mRunSeed,(uint16_t)run->mLevelIndex,seat,(uint8_t)run->mMode,(uint8_t)run->mDiff,
        (uint8_t)run->mScale,(uint8_t)run->mTempo,(uint8_t)run->mZombotany,(uint8_t)run->mEndlessScene,(uint8_t)run->mBossFlag,
        (uint8_t)run->mBuffMult,(uint8_t)run->mLadder,(uint8_t)run->mZombieBuffMode,run->mZombieBuffStacks,
        (uint8_t)run->mSpeedTier,(uint8_t)run->mFirstWave,(uint8_t)run->mEndlessOptions,(uint8_t)run->mCrowdRule);
}

void LawnApp::UpdateEndlessAdmission() {
    if(!mOnlineSession)return;
    auto& session=*mOnlineSession;
    auto* run=GetRunState();
    const bool endless=run && run->IsNewEndless();
    if(endless && session.IsConnected() && session.GetRole()==NetSession::Role::HOST && !run->mAdmission.run) {
        run->mAdmission.run=Admission::NewId();run->mAdmission.player=session.mCoopToken;run->mAdmission.history.push_back({session.mCoopToken,0,0});mAdmissionDirty=true;
    }
    // Process OFFER before UpdateOnlineStart can consume its corresponding START_LEVEL.
    auto inbox=std::move(session.mAdmissionInbox);session.mAdmissionInbox.clear();
    for(const auto& m:inbox) {
        if(m.op==AdmissionWire::OFFER) {session.mAdmissionOffer=m;continue;}
        if(m.op==AdmissionWire::CONTEXT) {
            if(endless && m.epoch==session.GetReadyEpoch() && m.level==uint32_t(run->mLevelIndex) && m.run) {
                if(!run->mAdmission.run){run->mAdmission.run=m.run;run->mAdmission.player=session.mCoopToken;mAdmissionDirty=true;}
                if(run->mAdmission.run==m.run){session.mAdmissionNewMask=uint8_t(m.budget)&session.GetOccupiedMask();session.mGateWanted=m.joined?1:0;}
            }
            continue;
        }
        if(!endless || !session.IsConnected() || session.GetRole()!=NetSession::Role::HOST)continue;
        if(m.op==AdmissionWire::VALUE && m.run==run->mAdmission.run && m.epoch==session.GetReadyEpoch()) {
            if(session.mCoopPeers[m.src] && session.mCoopPeers[m.src]!=m.player)continue;
            bool registered=false;for(const auto& e:run->mAdmission.history)if(e.player==m.player)registered=true;
            if(!registered && run->mAdmission.history.size()<Admission::MAX_PLAYERS_HISTORY && m.joined==Admission::PLAYED) {
                run->mAdmission.history.push_back({m.player,0,0});mAdmissionDirty=true;
            }
            session.mAdmissionValues[m.src]=int(m.budget);
            if(m.joined>=Admission::PREPARED)session.mAdmissionNewMask&=uint8_t(~(1u<<m.src));
            continue;
        }
        if(m.op!=AdmissionWire::HELLO || !session.SupportsAdmission() || !mBoard || run->mLevelIndex==0 ||
            !session.IsPreparationActive() || (session.GetPreparationPhase()!=PreparationWire::OPEN && session.GetPreparationPhase()!=PreparationWire::WAIT))continue;
        std::vector<int> values;
        for(uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat)
            if(seat!=m.src && session.IsSeatOccupied(seat) && !session.IsSeatOffline(seat) && !(session.mAdmissionNewMask&(1u<<seat)) && session.mAdmissionValues[seat]>=0)
                values.push_back(session.mAdmissionValues[seat]);
        auto& state=run->mAdmission;
        Admission::Entry* entry=nullptr;
        for(auto& e:state.history)if(e.player==m.player){entry=&e;break;}
        if(!entry) {
            if(state.history.size()>=Admission::MAX_PLAYERS_HISTORY)continue;
            state.history.push_back({m.player,run->mLevelIndex,Admission::Median(values)});entry=&state.history.back();
        }
        // Publish neither a grant nor a start until the durable receipt exists.
        if(!run->Save(mPlayerInfo->mId))continue;
        session.mAdmissionNewMask|=uint8_t(1u<<m.src);
        AdmissionWire::Message offer;offer.op=AdmissionWire::OFFER;offer.epoch=session.GetReadyEpoch();
        offer.run=state.run;offer.player=m.player;offer.level=run->mLevelIndex;offer.joined=entry->joined;offer.budget=entry->budget;
        session.SendAdmission(offer,m.src);SendAdmissionStart(m.src);
    }
    if(session.IsConnected() && session.GetRole()==NetSession::Role::CLIENT && !session.mAdmissionOffer.run && (!session.HasReadyRound() || !endless || !run->mAdmission.run) && ++mAdmissionReportTicks>=100) {
        mAdmissionReportTicks=0;AdmissionWire::Message hello;hello.op=AdmissionWire::HELLO;session.SendAdmission(hello,session.GetHostSeat());
    }
    if(!endless || !mBoard)return;
    auto& state=run->mAdmission;
    if(state.phase==Admission::PICKS && !run->HasPendingPick()) {state.phase=Admission::DECK;state.fast=false;mAdmissionDirty=true;}
    if(state.fast && state.phase==Admission::PICKS && !mAdmissionDirty) {
        for(int i=0;i<32 && run->HasPendingPick();++i) {
            if(!run->ChoicesRolled())run->RollChoices();
            if(!run->HasPendingPick())break;
            int valid[3],count=0;for(int c=0;c<3;++c)if(run->mBuffChoices[c]!=RunState::RUN_BUFF_CHOICE_NONE)valid[count++]=c;
            if(!count){run->SkipPendingPick();continue;}
            uint32_t index=uint32_t(state.player)^run->mPickCounter*0x9e3779b9u;
            run->TakeBuffChoice(valid[index%count]);
        }
        mAdmissionDirty=true;
    }
    if(state.Building()) {mGameScene=SCENE_PLAYING;mRunIntroHeld=true;}
    if(mAdmissionDirty && mPlayerInfo) {
        if(EndlessLawn::Capture(mBoard) && run->Save(mPlayerInfo->mId))mAdmissionDirty=false;
        else {state.fast=false;session.PostNotice(ModText::Tr("补位进度保存失败，请检查空间。","Admission save failed. Check storage."));}
    }
    if(!session.IsConnected() || !session.HasReadyRound())return;
    const uint32_t epoch=session.GetReadyEpoch();
    if(session.mAdmissionValuesEpoch!=epoch) {session.mAdmissionValuesEpoch=epoch;for(int& n:session.mAdmissionValues)n=-1;mAdmissionReportTicks=100;}
    if(++mAdmissionReportTicks>=100) {
        mAdmissionReportTicks=0;int value=0;Plant* plant=nullptr;
        while(mBoard->IteratePlants(plant))if(!plant->mDead && !plant->mSquished && plant->mIsOnBoard && plant->mSeedType>=0 && plant->mSeedType<48 && plant->mSeedType!=SEED_IMITATER && plant->mSeedType!=SEED_CHERRYBOMB && plant->mSeedType!=SEED_JALAPENO && plant->mSeedType!=SEED_DOOMSHROOM && plant->mSeedType!=SEED_ICESHROOM && plant->mSeedType!=SEED_GRAVEBUSTER && plant->mSeedType!=SEED_INSTANT_COFFEE)
            value=std::min(Admission::MAX_BUDGET,value+Plant::GetCost(plant->mSeedType));
        session.mAdmissionValues[session.GetLocalSeat()]=value;
        if(session.GetRole()==NetSession::Role::HOST) {
            for(uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat)if(seat!=session.GetLocalSeat() && session.IsSeatOccupied(seat)) {
                AdmissionWire::Message context;context.op=AdmissionWire::CONTEXT;context.run=state.run;context.level=run->mLevelIndex;context.epoch=epoch;context.budget=session.mAdmissionNewMask;context.joined=session.mGateWanted==1 && session.mGateAck==session.mGateRequest?1:0;
                session.SendAdmission(context,seat);
            }
        } else if(state.run) {
            AdmissionWire::Message report;report.op=AdmissionWire::VALUE;report.run=state.run;report.level=run->mLevelIndex;report.epoch=epoch;report.budget=value;
            report.joined=state.phase==Admission::NONE?Admission::PLAYED:state.phase;session.SendAdmission(report,session.GetHostSeat());
        }
    }
    if(session.GetRole()==NetSession::Role::HOST && session.SupportsAdmission()) {
        session.mAdmissionNewMask&=session.GetOccupiedMask();
        bool ready=true,values=true;
        for(uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat)if(session.IsSeatOccupied(seat)) {
            if(!session.IsSeatSeedsReady(seat))ready=false;
            if(session.mAdmissionValues[seat]<0 || session.IsSeatOffline(seat))values=false;
        }
        bool open=state.allow && state.history.size()<Admission::MAX_PLAYERS_HISTORY && run->mLevelIndex>0 && session.IsPreparationActive() && session.GetPreparationPhase()==PreparationWire::OPEN &&
            !ready && values && !session.mAdmissionNewMask && !mAdmissionDirty && !(mCoopPlay && mCoopPlay->Blocking());
        session.RequestRoomGate(open);
    }
}
