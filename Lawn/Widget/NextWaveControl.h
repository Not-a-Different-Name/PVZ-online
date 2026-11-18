#pragma once
#include "../../LawnApp.h"
#include "../Run/RunState.h"
#include "OnlineUiDraw.h"
#include "CoopHud.h"
#include "../Run/NextWaveState.h"
#include "../CursorObject.h"
#include "widget/WidgetManager.h"

namespace NextWaveControl {
using namespace Sexy;
inline Rect Button() { return Rect(500,550,78,46); }
inline bool BattleRunning(Board* b) {
    auto* app=b->mApp;
    return app->IsRunMode() && app->mGameScene==SCENE_PLAYING && !app->mRunIntroHeld &&
        !b->mPaused && app->mActive && !app->mMinimized && app->GetDialogCount()==0 &&
        app->mBoardResult==BOARDRESULT_NONE && b->mBoardFadeOutCounter<0 && b->mTimeStopCounter<=0 &&
        !b->HasLevelAwardDropped();
}
inline bool Visible(Board* b) {
    if(b->IsRunBossFinale() || b->mApp->AdmissionBuilding() || !b->mApp->IsRunMode() || b->mApp->mGameScene!=SCENE_PLAYING || CoopHud::Watching(b) ||
        b->mChatPanelOpen || b->mSunGiftSelectFrames>0 || b->mApp->mDamageStats.mOpen)return false;
#ifdef __ANDROID__
    if(b->mCoopToolsOpen)return false;
#endif
    return true;
}
inline void Update(Board* b) {
    auto& state=NextWave::For(b);
    const bool running=BattleRunning(b);
    state.Tick(NextWave::Now(),running);
    if(!running)state.Suspend();
    if(!b->mApp->mActive || b->mApp->mMinimized)state.keyHeld=false;
}
inline void Suspend(Board* b) { if(b)NextWave::For(b).Suspend(); }
inline bool CanRequest(Board* b) {
    const auto& state=NextWave::For(b);
    return Visible(b) && BattleRunning(b) && b->mCurrentWave>=0 && b->mCurrentWave<b->mNumWaves &&
        state.remainingMs==0 && state.pendingWave<0 &&
        b->mCursorObject->mCursorType==CURSOR_TYPE_NORMAL && b->mApp->mCrazyDaveState==CRAZY_DAVE_OFF;
}
inline bool Request(Board* b,int wave) {
    Update(b);
    if(!CanRequest(b) || wave!=b->mCurrentWave)return false;
    NextWave::For(b).pendingWave=wave;return true;
}
inline bool TakeRequest(Board* b) {
    auto& state=NextWave::For(b);int wave=state.pendingWave;state.pendingWave=-1;
    if(wave<0 || wave!=b->mCurrentWave || !CanRequest(b))return false;
    state.remainingMs=NextWave::COOLDOWN_MS;
    state.lastTick=NextWave::Now();state.running=true;
    return true;
}
inline bool MouseDown(Board* b,int x,int y,int clicks) {
    if(clicks<=0 || !Visible(b) || !Button().Contains(x,y))return false;
    Update(b);
    auto& state=NextWave::For(b);state.captured=true;
    state.pressedWave=CanRequest(b)?b->mCurrentWave:-1;
    b->mIgnoreMouseUp=true;
#ifdef __ANDROID__
    b->mTouchSeedDrag=false;b->mTouchSeedDragMoved=false;
#endif
    return true;
}
inline bool MouseUp(Board* b,int x,int y,int clicks) {
    auto& state=NextWave::For(b);
    if(!state.captured)return false;
    int wave=state.pressedWave;state.captured=false;state.pressedWave=-1;
    b->mIgnoreMouseUp=false;
    if(clicks>0 && Button().Contains(x,y) && wave>=0)Request(b,wave);
    return true;
}
inline bool KeyDown(Board* b,KeyCode key) {
    if(key!='N' || !b->mApp->IsRunMode())return false;
    auto& state=NextWave::For(b);
    bool held=state.keyHeld;state.keyHeld=true;
    if(!held && !b->mWidgetManager->mIsKeyRepeat && !b->mWidgetManager->mKeyDown[KEYCODE_CONTROL] &&
        !b->mWidgetManager->mKeyDown[KEYCODE_MENU])Request(b,b->mCurrentWave);
    return true;
}
inline void KeyUp(Board* b,KeyCode key) { if(key=='N')NextWave::For(b).keyHeld=false; }
inline std::vector<unsigned char> SaveTrailer(Board* b) { Update(b);return NextWave::Trailer(NextWave::For(b).remainingMs); }
inline void LoadTrailer(Board* b,const unsigned char* data,size_t size) {
    NextWave::Reset(b);NextWave::For(b).remainingMs=NextWave::ReadTrailer(data,size);
}
inline void Draw(Board* b,Graphics* g) {
    if(!Visible(b))return;
    const auto& state=NextWave::For(b);bool enabled=CanRequest(b);
    auto box=Button();
    SeatUi::Button(g,box,"",enabled,state.captured && enabled);
    std::string title=b->mCurrentWave>=b->mNumWaves?ModText::Tr("全部出场","All out"):
        state.remainingMs>0?std::string(ModText::Tr("冷却 ","Wait "))+std::to_string((state.remainingMs+999)/1000)+ModText::Tr("秒","s"):
        b->mCurrentWave==0?ModText::Tr("开始出怪","Start"):ModText::Tr("下一波","Next");
    std::string detail;
    if(b->mCurrentWave<b->mNumWaves) {
        bool boss=b->mApp->GetRunState()->IsEndless()?b->mApp->GetRunState()->IsEndlessBossLevel():false;
        // Finite bosses use their original milestone/seat checks in SpawnRunBoss.
        auto* run=b->mApp->GetRunState();
        if(!run->IsEndless())boss=run->GiantBossTier()>0 && (run->GetPlayingLevelIndex()+1==RunState::LevelCountForMode(run->mMode) ||
            (run->GiantBossTier()>=2 && run->GetPlayingLevelIndex()+1==RunState::LevelsPerScene(run->mMode)*3));
        boss=boss && b->IsFlagWave(b->mCurrentWave) && b->mRunBossZombieID==ZombieID::ZOMBIEID_NULL &&
            (!b->mApp->IsOnlineGame() || b->mApp->mOnlineSession->GetLocalSeat()==NetProto::SEAT_HOST);
        detail=(boss?"BOSS ":"")+std::to_string(b->mCurrentWave+1)+"/"+std::to_string(b->mNumWaves);
        if(!boss)detail+=" "+std::to_string((std::max(0,b->mHugeWaveCountDown>0?b->mHugeWaveCountDown:b->mZombieCountDown)+99)/100)+"s";
    }
#ifndef __ANDROID__
    if(state.remainingMs==0 && b->mCurrentWave>0 && b->mCurrentWave<b->mNumWaves)title+=" N";
#endif
    Color ink=enabled?Color(245,230,165):Color(175,175,165);
    SeatUi::Text(g,title,box.mX+3,box.mY+19,box.mWidth-6,14,ink,true);
    SeatUi::Text(g,detail,box.mX+3,box.mY+37,box.mWidth-6,11,ink,true);
}
}
