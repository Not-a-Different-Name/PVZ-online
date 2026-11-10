#pragma once

#include "LawnDialog.h"
#include "OnlineUiDraw.h"
#include "../Online/RoomPresentation.h"
#include "../Observer/ObserverBoard.h"

namespace WatchRoomUi {
constexpr int DialogId = 3111;

class Panel final : public LawnDialog {
    struct Action {
        int op = 0;
        uint8_t slot = 0;
        uint32_t request = 0;
        bool operator==(const Action&) const = default;
    };
    LawnApp* app;
    Observer::BoardView view;
    const bool spectator;
    Action pressed{};
    bool leaveConfirm = false;

    static Sexy::Rect Target(int i) { return Sexy::Rect(12+i*130,470,126,42); }
    static Sexy::Rect Bottom(int i) { return Sexy::Rect(12+i*195,545,184,43); }
    static Sexy::Rect Row(int i) { return Sexy::Rect(44,133+i*78,712,66); }
    static Sexy::Rect Decision(int i,int col) { return Sexy::Rect(484+col*128,145+i*78,120,42); }
    NetSession* Session() const { return RoomPresentation::Session(app); }

    Action Hit(int x,int y) const {
        auto* s=Session();
        if(Bottom(3).Contains(x,y))return {100};
        if(!s || !s->SupportsWatchRoom() || spectator!=s->InWatchRoom())return {};
        if(spectator) {
            for(uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat)
                if(Target(seat-1).Contains(x,y) && s->CanWatchSeat(seat))return {101,seat};
            if(Bottom(0).Contains(x,y)) {
                const auto& me=s->WatchViewers()[s->WatchSlot()];
                return me.state ? Action{WatchRoom::Cancel,s->WatchSlot(),me.request} : Action{WatchRoom::Apply};
            }
            if(Bottom(1).Contains(x,y) && s->CanRefreshObserver())
                return {101,s->GetWatchTargetSeat()};
            return {};
        }
        if(Bottom(0).Contains(x,y) && s->CanMoveToWatchRoom())return {WatchRoom::Demote};
        if(s->IsHostSeat())for(uint8_t id=WatchRoom::FirstViewer;id<=WatchRoom::LastViewer;++id) {
            const auto& viewer=s->WatchViewers()[id];
            if(!viewer.state)continue;
            if(viewer.state==1 && Decision(id-WatchRoom::FirstViewer,0).Contains(x,y))return {WatchRoom::Approve,id,viewer.request};
            if(Decision(id-WatchRoom::FirstViewer,1).Contains(x,y))return {WatchRoom::Reject,id,viewer.request};
        }
        return {};
    }

    void Leave() {
        if(!spectator) { app->KillDialog(DialogId); return; }
        if(!leaveConfirm) {leaveConfirm=true;MarkDirty();return;}
        if(auto* s=Session())s->Close();
        app->KillDialog(DialogId);
    }

public:
    Panel(LawnApp* a,bool observer):
        LawnDialog(a,DialogId,true,"","","",Dialog::BUTTONS_NONE),app(a),view(a),spectator(observer) {
        Resize(0,0,800,600);
    }
    bool IsSpectatorPanel() const { return spectator; }
    void Update() override {
        LawnDialog::Update();
        if(auto* s=Session(); spectator && s && s->InWatchRoom() && s->IsConnected()) {
            if(!s->IsWatchingV2())for(uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat)
                if(s->CanWatchSeat(seat) && s->BeginWatch(seat))break;
            view.Update(nullptr,s);
        }
        MarkDirty();
    }
    void Draw(Sexy::Graphics* g) override {
        using namespace Sexy;
        auto* s=Session();
        const bool connected=s&&s->SupportsWatchRoom();
        const Color cream(245,230,165),light(205,220,225);
        if(spectator) {
            g->SetColor(Color(0,0,0));
            g->FillRect(0,0,800,600);
            if(!connected || !view.Draw(g,s)) {
                SeatUi::Panel(g,Rect(0,0,800,600));
                SeatUi::Text(g,ModText::Tr("队伍观战","Team spectator"),36,86,728,30,cream,true);
                const std::string status=!connected&&s?s->GetStatusText():
                    s&&s->IsWatchingV2()?ModText::Tr("正在取队友画面…","Loading your teammate's lawn…"):
                    ModText::Tr("等待队友开场","Waiting for teammates to start");
                SeatUi::Text(g,status,40,190,720,22,light,true);
            }
            SeatUi::Strip(g,Rect(0,463,800,137));
            for(uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat) {
                const bool online=connected&&s->CanWatchSeat(seat);
                const auto name=online?SeatUi::Fit(s->GetSeatName(seat),100,18):"P"+std::to_string(seat);
                SeatUi::Button(g,Target(seat-1),name.c_str(),
                    online,connected&&s->GetWatchTargetSeat()==seat);
            }
            const uint8_t state=connected?s->WatchViewers()[s->WatchSlot()].state:0;
            const auto label=state==2?ModText::Tr("已批准，等待关间","Approved for next stage"):
                state==1?ModText::Tr("等待房主批准","Awaiting host approval"):
                ModText::Tr("观战中 · 自动刷新 5 秒","Spectating · refresh every 5s");
            SeatUi::Text(g,label,12,533,776,17,light,true);
            SeatUi::Button(g,Bottom(0),state?ModText::Tr("撤回申请","Cancel request"):ModText::Tr("申请参战","Request to play"),connected);
            const auto cool=connected?s->ObserverRequestCooldown():0;
            const auto refresh=ModText::Tr("刷新画面","Refresh")+(cool?" "+std::to_string((cool+999)/1000)+"s":"");
            SeatUi::Button(g,Bottom(1),refresh.c_str(),connected&&s->CanRefreshObserver());
        } else {
            SeatUi::Panel(g,Rect(0,0,800,600));
            SeatUi::Text(g,ModText::Tr("观众与参战申请","Spectators and requests"),40,74,720,29,cream,true);
            SeatUi::Text(g,ModText::Tr("观众单独计位。批准后在关间加入。","Spectators have separate slots. Approvals take effect between stages."),44,109,712,17,light,true);
            for(uint8_t id=WatchRoom::FirstViewer;id<=WatchRoom::LastViewer;++id) {
                const int i=id-WatchRoom::FirstViewer;
                const auto box=Row(i);SeatUi::Strip(g,box);
                const auto viewer=connected?s->WatchViewers()[id]:WatchRoom::Viewer{};
                const auto name=viewer.name.empty()?ModText::Tr("空观众位","Open spectator slot"):viewer.name;
                SeatUi::Text(g,name,box.mX+14,box.mY+25,400,22,cream);
                if(!viewer.name.empty())SeatUi::Text(g,viewer.state==2?ModText::Tr("已批准","Approved"):
                    viewer.state==1?ModText::Tr("申请参战","Wants to play"):ModText::Tr("观战中","Spectating"),box.mX+14,box.mY+53,400,17,light);
                if(connected && s->IsHostSeat() && viewer.state) {
                    SeatUi::Button(g,Decision(i,0),ModText::Tr("批准","Approve"),viewer.state==1);
                    SeatUi::Button(g,Decision(i,1),ModText::Tr("拒绝","Decline"),true);
                }
            }
            SeatUi::Button(g,Bottom(0),ModText::Tr("转为观战","Spectate"),connected&&s->CanMoveToWatchRoom());
        }
        SeatUi::Button(g,Bottom(3),spectator?(leaveConfirm?ModText::Tr("确认离开","Confirm leave"):ModText::Tr("离开房间","Leave room")):ModText::Tr("返回","Back"),true);
    }
    void MouseDown(int x,int y,int clicks) override {pressed=clicks>0?Hit(x,y):Action{};}
    void MouseUp(int x,int y,int clicks) override {
        const auto action=pressed;pressed={};
        if(clicks<=0 || !action.op || !(Hit(x,y)==action))return;
        if(action.op==100) {Leave();return;}
        auto* s=Session();if(!s)return;
        leaveConfirm=false;
        if(action.op==101)s->BeginWatch(action.slot);
        else s->WatchRoleAction(uint8_t(action.op),action.slot,action.request);
    }
    void KeyDown(Sexy::KeyCode key) override {
        if(key==Sexy::KEYCODE_ESCAPE)Leave();
    }
};

inline void Open(LawnApp* app) {
    auto* s=RoomPresentation::Session(app);
    if(s && s->SupportsWatchRoom() && !app->GetDialog(DialogId))
        app->AddDialog(DialogId,new Panel(app,s->InWatchRoom()));
}
inline void Update(LawnApp* app) {
    auto* s=RoomPresentation::Session(app);
    if(auto* d=dynamic_cast<Panel*>(app->GetDialog(DialogId));d && d->IsSpectatorPanel() && (!s || !s->InWatchRoom())) {
        app->KillDialog(DialogId);
        return;
    }
    if(!s || !s->InWatchRoom() || !s->IsConnected())return;
    if(auto* d=dynamic_cast<Panel*>(app->GetDialog(DialogId));d && !d->IsSpectatorPanel())app->KillDialog(DialogId);
    if(app->GetDialog(DialogId))return;
    // A lost live lawn is abandoned from the last durable checkpoint. It must
    // not write unfinished battle state over the checkpoint used on admission.
    if(app->mBoard) {
        app->mBoardResult=BOARDRESULT_NONE;
        app->KillBoard();
        app->ShowGameSelector();
    }
    app->KillDialog(DIALOG_ONLINE_RECONNECT);
    app->KillDialog(DIALOG_ONLINE);
    app->KillDialog(DIALOG_ONLINE_SWAP);
    app->KillDialog(DIALOG_ONLINE_START);
    Open(app);
}
}
