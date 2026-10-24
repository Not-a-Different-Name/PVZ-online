#pragma once
#include "RunState.h"
#include "RunCheckpointIO.h"
#include "../../LawnApp.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../Cutscene.h"
#include "../Online/NetSession.h"
#include "../System/SaveGame.h"
#include "../System/PlayerInfo.h"
#include "../Widget/LawnDialog.h"
#include "../ModText.h"
#include "misc/Buffer.h"

namespace EndlessLawn {
// Envelope validates the local serializer ABI and preserves the gameplay random stream.
inline uint32_t Layout() { return 0u; }
inline void Put(std::vector<unsigned char>& out,uint32_t value) { for(int i=0;i<4;++i)out.push_back((unsigned char)(value>>(8*i))); }
inline uint32_t Get(const unsigned char* p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
inline std::vector<unsigned char> Pack(const unsigned char* bytes,size_t count) {
    std::string rng=Sexy::GetRandState();std::vector<unsigned char> out;
    Put(out,0x314c4e45);Put(out,Layout());Put(out,(uint32_t)rng.size());
    out.insert(out.end(),rng.begin(),rng.end());out.insert(out.end(),bytes,bytes+count);return out;
}
inline bool Unpack(const std::vector<unsigned char>& bytes,std::vector<unsigned char>& lawn,std::string& rng) {
    if(bytes.size()<12 || Get(bytes.data())!=0x314c4e45 || Get(bytes.data()+4)!=Layout())return false;
    size_t count=Get(bytes.data()+8);
    if(count==0 || count>20000 || count+12>=bytes.size())return false;
    rng.assign((const char*)bytes.data()+12,count);lawn.assign(bytes.begin()+12+count,bytes.end());return true;
}
inline bool StopAtLimit(LawnApp* app) {
    RunState* run=app->GetRunState();
    if(!run || !run->IsEndless() || run->mLevelIndex<RunState::RUN_ENDLESS_LEVEL_COUNT-1)return false;
    run->mEndlessSnapshotPhase=4;run->mPendingPlantPicks=run->mPendingBuffPicks=0;
    if(!run->Save(app->mPlayerInfo->mId)) {
        app->mRunIntroHeld=true;app->mGameScene=GameScenes::SCENE_LEVEL_INTRO;
        app->LawnMessageBox(Dialogs::DIALOG_MESSAGE,ModText::Tr("保存失败","Save failed"),
            ModText::Tr("已到达当前版本的关数上限，但纪录尚未保存。请检查可用空间后返回菜单重试保存。","The level limit was reached, but the record could not be saved. Check storage and retry returning to the menu."),"OK","",Sexy::Dialog::BUTTONS_FOOTER);
        return true;
    }
    app->DoBackToMain(true);
    app->LawnMessageBox(Dialogs::DIALOG_MESSAGE,ModText::Tr("无尽纪录已保存","Endless record saved"),
        ModText::Tr("已完成当前版本的 60000 关上限，阵型和纪录已保留。可新开一局继续挑战。","You reached this version's 60,000-level limit. Your lawn and record are retained. Start a new run to play again."),"OK","",Sexy::Dialog::BUTTONS_FOOTER);
    return true;
}
inline std::string Scratch(LawnApp* app) {
    return RunState::GetCheckpointName(app->mPlayerInfo->mId,RunState::RUN_MODE_ENDLESS,app->GetRunState()->mEndlessScene)+".board-tmp";
}
// Snapshots are embedded in the atomic RunState transaction, not published as a second save.
inline bool Capture(Board* board,bool baseline=false) {
    if(!board)return true;
    LawnApp* app=board->mApp;RunState* run=app->GetRunState();
    if(!run || !run->IsNewEndless())return true;
    if(app->mBoardResult==BoardResult::BOARDRESULT_LOST || app->mGameScene==GameScenes::SCENE_ZOMBIES_WON) {run->mEndlessSnapshotPhase=5;return true;}
    int phase=run->mAdmission.Building()?1:run->mEndlessSnapshotPhase==4?4:app->mRunIntroHeld?1:app->mGameScene==GameScenes::SCENE_PLAYING?3:2;
    if(app->IsOnlineGame() && phase==3 && !baseline)return true;
    std::string path=Scratch(app);
    if(!LawnSaveGame(board,path))return false;
    Sexy::Buffer buffer;
    bool ok=app->ReadBufferFromFile(path,&buffer,false);
    app->EraseFile(path);
    if(!ok || buffer.GetDataLen()<=0 || buffer.GetDataLen()>24*1024*1024)return false;
    if(run->mEndlessSnapshotLevel!=run->mLevelIndex)run->mEndlessRetryLawn.clear();
    const unsigned char* bytes=(const unsigned char*)buffer.GetDataPtr();
    run->mEndlessLawn=Pack(bytes,buffer.GetDataLen());
    run->mEndlessSnapshotLevel=run->mLevelIndex;run->mEndlessSnapshotPhase=phase;
    if(baseline)run->mEndlessRetryLawn=run->mEndlessLawn;
    return true;
}
// A selected imitater belongs to the plant it will become, including before transformation.
inline uint64_t LineupSeedBit(int seed,int imitated) {
    if(seed==(int)SeedType::SEED_IMITATER)seed=imitated;
    return seed>=0 && seed<48 ? uint64_t(1)<<seed : 0;
}
inline void RememberLineup(Board* board) {
    if(!board)return;
    RunState* run=board->mApp->GetRunState();if(!run || !run->IsNewEndless())return;
    uint64_t mask=0;
    for(int i=0;i<board->mSeedBank->mNumPackets;++i) {
        const auto& packet=board->mSeedBank->mSeedPackets[i];
        mask|=LineupSeedBit(packet.mPacketType,packet.mImitaterType);
    }
    Plant* p=nullptr;
    while(board->IteratePlants(p))if(!p->mDead)mask|=LineupSeedBit(p->mSeedType,p->mImitaterType);
    run->mEndlessFocusMask=mask;
}
// 0 means a new/legacy run has no snapshot. -1 means a promised snapshot could not restore.
inline int Restore(LawnApp* app) {
    RunState* run=app->GetRunState();if(!run || !run->IsNewEndless())return 0;
    if(app->mBoard && app->mBoardResult!=BoardResult::BOARDRESULT_LOST &&
        (app->mBoard->mLevelComplete || app->mBoard->mLevelAwardSpawned))return 0;
    if(run->mEndlessLawn.empty() && run->mAdmission.Pending())return 0;
    if(run->mEndlessLawn.empty()) {
        if(run->IsCatchingUp() && app->mOnlineSession)
            app->mOnlineSession->PostNotice(ModText::Tr("本机没有这一局的草坪；将补领增益并从空草坪加入。","No matching local lawn: catching up buffs and joining with a fresh lawn."));
        return run->mLevelIndex==0?0:-1;
    }
    bool admissionAdvance=run->mAdmission.run && run->mAdmission.phase!=Admission::NONE && run->mEndlessSnapshotLevel<run->mLevelIndex;
    if(run->mEndlessSnapshotLevel!=run->mLevelIndex && !admissionAdvance) {
        if(run->IsCatchingUp() && app->mOnlineSession)
            app->mOnlineSession->PostNotice(ModText::Tr("本机草坪关数与房主不同；将补领增益并从空草坪加入。","Local lawn is from another stage: catching up buffs and joining with a fresh lawn."));
        return run->IsCatchingUp()?0:-1;
    }
    bool retry=run->mEndlessSnapshotPhase==5 || app->mBoardResult==BoardResult::BOARDRESULT_LOST || (app->IsOnlineGame() && run->mEndlessSnapshotPhase==3);
    const auto& bytes=retry?run->mEndlessRetryLawn:run->mEndlessLawn;
    if(bytes.empty())return -1;
    std::string path=Scratch(app);
    std::vector<unsigned char> lawn;std::string rng;
    if(!Unpack(bytes,lawn,rng) || !RunCheckpointIO::AtomicWrite(path,lawn))return -1;
    app->MakeNewBoard();
    bool ok=app->mBoard->LoadGame(path);app->EraseFile(path);
    if(!ok){app->KillBoard();return -1;}
    app->mBoard->mOnlineBoard=app->IsOnlineGame();
    if(admissionAdvance)app->mBoard->InitEndlessRunStage();
    app->mBoard->mPaused=false;
    app->mBoardResult=BoardResult::BOARDRESULT_NONE;
    app->SetOnlineStartOverride(run->GetLevel(),run->GetLevelSeed());
    if(admissionAdvance) {
        app->mGameScene=GameScenes::SCENE_LEVEL_INTRO;app->mRunIntroHeld=true;
        if(app->IsOnlineGame())app->mOnlineSession->SendSeedsReady(false);
    } else if(retry) {
        app->mGameScene=GameScenes::SCENE_LEVEL_INTRO;app->mRunIntroHeld=false;
        if(app->IsOnlineGame())app->mOnlineSession->SendSeedsReady(false);
        app->ShowSeedChooserScreen();app->mBoard->mCutScene->StartLevelIntro();
    } else if(admissionAdvance || run->mEndlessSnapshotPhase==1) {
        app->mGameScene=GameScenes::SCENE_LEVEL_INTRO;app->mRunIntroHeld=true;
    } else if(run->mEndlessSnapshotPhase==2) {
        app->mGameScene=GameScenes::SCENE_LEVEL_INTRO;app->mRunIntroHeld=false;
        app->ShowSeedChooserScreen();app->mBoard->mCutScene->StartLevelIntro();
    } else {
        app->mGameScene=GameScenes::SCENE_PLAYING;app->mRunIntroHeld=false;app->DoPauseDialog();
    }
    if(run->mAdmission.Building()) {app->mGameScene=GameScenes::SCENE_PLAYING;app->mRunIntroHeld=true;app->mBoard->ClearAdviceImmediately();}
    Sexy::RestoreRandState(rng);
    return 1;
}
inline void Enter(LawnApp* app) {
    if(app->GetRunState() && app->GetRunState()->mEndlessSnapshotPhase==4 && StopAtLimit(app))return;
    if(app->TryKeepEndlessLawn())return;
    int restored=Restore(app);
    if(restored==0){app->NewGame();return;}
    if(restored>0)return;
    app->DoBackToMain(true);
    app->LawnMessageBox(Dialogs::DIALOG_MESSAGE,ModText::Tr("无法恢复草坪","Cannot restore lawn"),
        ModText::Tr("草坪存档缺失或不兼容，已保留原存档。请从对应无尽场景重试。","The lawn snapshot is missing or incompatible. Your save is retained."),"OK","",Sexy::Dialog::BUTTONS_FOOTER);
}

}
