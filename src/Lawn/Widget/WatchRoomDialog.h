#pragma once

#include "LawnDialog.h"
#include "OnlineUiDraw.h"
#include "../Online/RoomPresentation.h"
#include "../Observer/ObserverBoard.h"
#include "widget/WidgetManager.h"

namespace WatchRoomUi {
constexpr int DialogId = 3111;

inline bool StartTemporary(Board* board, NetSession* session, uint8_t seat) {
    if(!board || !session)return false;
    if(board->mApp->mGameScene!=GameScenes::SCENE_PLAYING) {
        board->PushCustomBanner(ModText::Tr("进入战斗后可观看队友。","You can watch teammates once battle begins."));
        return false;
    }
    if(!session->CanWatchSeat(seat) || !session->BeginWatch(seat)) {
        board->PushCustomBanner(ModText::Tr("暂时无法观战，请稍后再试。","Cannot watch yet. Try again shortly."));
        return false;
    }
#ifdef __ANDROID__
    board->mMobileWatchOpen=true;
#else
    board->mWatchKeyHeld=true;
    board->mCoopWatchLatched=true;
#endif
    board->mWatchLastSeat=seat;
    board->PushCustomBanner(ModText::Tr("正在加载队友场地…","Loading your teammate's lawn…"));
    return true;
}

inline void RestoreRoom(LawnApp* app, bool open) {
    app->KillDialog(DialogId);
    if(app->mGameScene==GameScenes::SCENE_SPECTATING)app->ShowGameSelector();
    if(open && !app->GetDialog(DIALOG_ONLINE))app->DoOnlineDialog();
}

class Screen final : public LawnDialog {
    LawnApp* app;
    Observer::BoardView view;
    uint8_t target;
    int pressed=0;
    bool picker=false;
    bool searched=false;
    static Sexy::Rect Control(int i) { return Sexy::Rect(358+i*145,551,138,42); }
    static Sexy::Rect Target(int i) { return Sexy::Rect(408+(i%2)*186,397+(i/2)*47,174,42); }
    NetSession* Session() const { return RoomPresentation::Session(app); }

    int Hit(int x,int y) const {
        for(int i=0;i<3;++i)if(Control(i).Contains(x,y))return i+1;
        if(picker)for(int i=0;i<NetProto::MAX_PLAYERS;++i)
            if(Target(i).Contains(x,y))return i+10;
        return 0;
    }

    void Back() {
        if(auto* s=Session())s->EndWatch(true);
        RestoreRoom(app,true);
    }

public:
    Screen(LawnApp* a,uint8_t seat):
        LawnDialog(a,DialogId,true,"","","",Dialog::BUTTONS_NONE),app(a),view(a),target(seat) { Resize(0,0,800,600); }

    void Update() override {
        LawnDialog::Update();
        auto* s=Session();
        if(!s || !s->SharedRoom() || !s->InWatchRoom())return;
        if(!s->IsWatching()) {
            if(s->WatchSourceReady(target) && !searched) {
                searched=s->BeginWatch(target);
            } else {
                for(int i=1;i<=NetProto::MAX_PLAYERS;++i) {
                    const auto next=uint8_t((target+NetProto::MAX_PLAYERS-1+i)%NetProto::MAX_PLAYERS+1);
                    if(next!=target && s->WatchSourceReady(next) && s->BeginWatch(next)) {
                        target=next;searched=true;break;
                    }
                }
                if(!s->WatchSourceReady(target))searched=false;
            }
        }
        view.Update(nullptr,s);
        MarkDirty();
    }

    void Draw(Sexy::Graphics* g) override {
        using namespace Sexy;
        auto* s=Session();
        const Color cream(245,230,165), light(205,220,225);
        g->SetColor(Color(0,0,0));g->FillRect(0,0,800,600);
        if(!s || !view.Draw(g,s)) {
            const auto title=ModText::Tr("队伍观战","Team spectator");
            std::string status=ModText::Tr("等待队友进入战斗，可切换队友。","Waiting for a teammate to enter battle. You can switch targets.");
            if(s && !s->IsConnected())status=s->GetStatusText();
            else if(s && s->SharedRoomPaused())status=ModText::Tr("暂无参战成员，房间已暂停。","The room is paused until a player returns to combat.");
            else if(s && s->IsWatching())status=ModText::Tr("正在加载队友场地…","Loading your teammate's lawn…");
            else if(s && s->WatchSourceReady(target))status=ModText::Tr("场地暂未收到，可刷新或切换队友。","The lawn did not arrive. Refresh or switch targets.");
            SeatUi::Text(g,title,24,199,752,30,cream,true);
            SeatUi::Text(g,status,30,260,740,20,light,true);
        }
        if(picker && s) {
            SeatUi::Strip(g,Rect(397,386,395,155));
            for(int i=0;i<NetProto::MAX_PLAYERS;++i) {
                const auto seat=uint8_t(i+1);
                const auto& member=s->RoomMembers()[seat];
                const auto label=member.identity?SeatUi::Fit(member.name,135,18):"P"+std::to_string(seat);
                SeatUi::Button(g,Target(i),label.c_str(),s->WatchSourceReady(seat),target==seat);
            }
        }
        SeatUi::Button(g,Control(0),ModText::Tr("切换队友","Switch target"),s&&s->IsConnected());
        SeatUi::Button(g,Control(1),ModText::Tr("刷新","Refresh"),s&&(s->CanRefreshObserver()||(!s->IsWatching()&&s->WatchSourceReady(target))));
        SeatUi::Button(g,Control(2),ModText::Tr("返回房间","Back to room"),true);
    }

    void MouseDown(int x,int y,int clicks) override { pressed=clicks>0?Hit(x,y):0; }
    void MouseUp(int x,int y,int clicks) override {
        const int action=pressed;pressed=0;
        if(clicks<=0 || !action || Hit(x,y)!=action)return;
        if(action==3) { Back();return; }
        auto* s=Session();
        if(!s || !s->IsConnected())return;
        if(action==1)picker=!picker;
        if(action==2 && (s->CanRefreshObserver() || (!s->IsWatching() && s->WatchSourceReady(target))))
            searched=s->BeginWatch(target);
        if(action>=10) {
            const auto seat=uint8_t(action-9);
            if(s->WatchSourceReady(seat)) {
                if(seat==target && s->IsWatching()) {picker=false;return;}
                if(s->BeginWatch(seat)) {target=seat;searched=true;picker=false;}
            }
        }
    }
    void KeyDown(Sexy::KeyCode key) override { if(key==Sexy::KEYCODE_ESCAPE)Back(); }
};

inline void Watch(LawnApp* app,uint8_t seat) {
    auto* s=RoomPresentation::Session(app);
    if(!s || !s->CanWatchSeat(seat))return;
    if(app->mBoard && !s->InWatchRoom()) {
        if(StartTemporary(app->mBoard,s,seat))app->KillDialog(DIALOG_ONLINE);
        return;
    }
    if(!s->InWatchRoom() || app->GetDialog(DialogId))return;
    app->KillDialog(DIALOG_ONLINE);
    if(app->mGameSelector)app->KillGameSelector();
    app->mGameScene=GameScenes::SCENE_SPECTATING;
    auto* screen=new Screen(app,seat);
    app->AddDialog(DialogId,screen);
    app->mWidgetManager->SetFocus(screen);
}

inline void Open(LawnApp* app) {
    if(!app->GetDialog(DIALOG_ONLINE))app->DoOnlineDialog();
}

inline void Update(LawnApp* app) {
    auto* s=RoomPresentation::Session(app);
    if(!s)return;
    if(app->GetDialog(DialogId) && !s->IsReconnecting() && (!s->InWatchRoom() || !s->SharedRoomStarted())) {
        app->KillDialog(DIALOG_ONLINE_RECONNECT);
        RestoreRoom(app,!s->IsConnected() || !s->SharedRoomStarted());
    }
    if(!s->TakeSpectatorEntry())return;
    if(app->mBoard) {
        app->mBoardResult=BOARDRESULT_NONE;
        app->KillBoard();
        app->ShowGameSelector();
    }
    app->mOnlineWaitingStartAck=false;
    app->mOnlineRunStartHeld=false;
    app->mOnlineRunGo=false;
    app->mRunIntroHeld=false;
    app->mOnlineSeedsHeld=false;
    app->KillDialog(DIALOG_ONLINE_RECONNECT);
    app->KillDialog(DIALOG_ONLINE_SWAP);
    app->KillDialog(DIALOG_ONLINE_START);
    Open(app);
}
}
