#pragma once
#include "OnlineSeatDialog.h"
#include "SeedChooserScreen.h"
#include "../Board.h"
#include "widget/WidgetManager.h"
#include "RoomShareUi.h"
#include "PlayerRoomText.h"

// @pvz-online: the existing ready gate stays modal; its presentation no longer hides the lawn.
class OnlineReadyDialog : public LawnDialog {
    LawnApp* app;
    bool down=false;
    bool readyDown=false,inviteDown=false;
    bool cancelTooLate=false;
    RoomShareUi::Transfer transfer;
    int shareDown=-1;
    std::string localIp;
    Rect ShareRect(int index) const {return Rect(18+index*218,Endless()?200:148,198,44);}
    static Rect Invite(){return Rect(486,138,166,40);}
    bool Endless()const{return app->GetRunState() && app->GetRunState()->IsNewEndless();}
    static Rect Ready(){return Rect(486,74,166,46);}
    static Rect Details(){return Rect(486,14,166,46);}
    bool CanToggleReady() const {
        auto* session=RoomPresentation::Session(app);
        if(!session)return false;
        const auto state=session->GetReadyCancelState();
        if(state!=NetSession::ReadyCancelState::NONE)return false;
        return session->IsSeatSeedsReady(session->GetLocalSeat())?
            session->CanCancelSeedsReady():session->CanReadyPreparation();
    }
    void ToggleReady() {
        if(!CanToggleReady())return;
        auto* session=RoomPresentation::Session(app);
        if(session->IsSeatSeedsReady(session->GetLocalSeat()))session->RequestSeedsReadyCancel();
        else app->ToggleOnlineSeedsReady();
        MarkDirty();
    }
public:
    explicit OnlineReadyDialog(LawnApp* a):LawnDialog(a,DIALOG_ONLINE_START,true,"","","",Dialog::BUTTONS_NONE),app(a) {
        Resize(64,92,672,Endless()?252:200);
        localIp=NetLink::GetLocalIPv4Text();
    }
    void Update() override {
        LawnDialog::Update();
        transfer.Update();
        auto* session=RoomPresentation::Session(app);
        if(session) {
            const auto state=session->GetReadyCancelState();
            if(state==NetSession::ReadyCancelState::CONFIRMED) {
                session->ClearReadyCancelResult();
                auto* chooser=app->mSeedChooserScreen.get();
                if(app->mOnlineSeedsHeld && app->mBoard && chooser && app->mBoard->ChooseSeedsOnCurrentLevel()) {
                    app->mOnlineSeedsHeld=false;
                    app->mPreparationEditRequested=false;
                    app->KillDialog(DIALOG_ONLINE_SWAP);
                    app->KillDialog(DIALOG_ONLINE_START);
                    chooser->ResumeSelectionForEditing();
                    app->mWidgetManager->SetFocus(chooser);
                    return;
                }
            } else if(state==NetSession::ReadyCancelState::TOO_LATE) {
                cancelTooLate=true;
                session->ClearReadyCancelResult();
            }
        }
        MarkDirty();
    }
    void Draw(Graphics* g) override {
        SeatUi::Strip(g,Rect(0,0,mWidth,mHeight));
        auto* s=RoomPresentation::Session(app);
        const bool ready=s&&s->IsSeatSeedsReady(s->GetLocalSeat());
        const bool cancelling=s&&s->GetReadyCancelState()==NetSession::ReadyCancelState::PENDING;
        SeatUi::Text(g,RoomPresentation::ReadySummary(s),18,35,450,24,Color(245,230,165));
        SeatUi::Paragraph(g,PlayerRoomText::ReadyHint(ready,cancelling,cancelTooLate,CanToggleReady()),Rect(18,54,450,48),20,Color(220,225,210));
        if(!transfer.notice.empty())SeatUi::Text(g,transfer.notice,18,124,634,16,Color(190,210,225));
        else if(ready&&!cancelling&&!cancelTooLate)SeatUi::Text(g,RoomPresentation::WaitingSeats(s),18,124,634,18,Color(190,210,225));
        SeatUi::Button(g,Details(),ModText::Tr("队友与换位","Seats / swap"),true,down);
        if(Endless() && s) {
            auto* run=app->GetRunState();const bool host=s->GetRole()==NetSession::Role::HOST;
            std::string status=ModText::Tr("房间码 ","Room code ")+s->GetRoomCode()+" · ";
            status+=!s->SupportsAdmission()?ModText::Tr("暂不能邀请，请稍后再试","Invites unavailable; retry later"):
                s->mAdmissionNewMask?ModText::Tr("新队友正在选植物","New teammate choosing plants"):
                s->mGateWanted==1 && (!host || s->mGateAck==s->mGateRequest)?ModText::Tr("允许关间加入","Joining between levels is open"):
                ModText::Tr("暂不允许加入","Joining is closed");
            SeatUi::Text(g,status,18,166,450,18,Color(220,225,210));
            if(host)SeatUi::Button(g,Invite(),s->mAdmissionNewMask?ModText::Tr("取消本次邀请","Cancel invite"):
                run->mAdmission.allow?ModText::Tr("暂停加入","Close joining"):ModText::Tr("允许加入","Open joining"),
                s->SupportsAdmission() && s->GetPreparationPhase()!=PreparationWire::SEAL && s->GetPreparationPhase()!=PreparationWire::GO,inviteDown);
        }
        const char* label=cancelling?ModText::Tr("取消中…","Cancelling…"):
            ready?ModText::Tr("取消准备","Cancel ready"):ModText::Tr("准备","Ready");
        SeatUi::Button(g,Ready(),label,CanToggleReady(),readyDown);
        const char* labels[]={ModText::Tr("复制房号","Copy code"),ModText::Tr("复制邀请","Copy invite"),ModText::Tr("分享","Share")};
        for(int index=0;index<3;++index)SeatUi::Button(g,ShareRect(index),labels[index],s&&!s->GetRoomCode().empty()&&!transfer.Busy(),shareDown==index);
    }
    void MouseDown(int x,int y,int clicks) override {shareDown=-1;for(int index=0;index<3;++index)if(clicks>0&&clicks!=3&&ShareRect(index).Contains(x,y))shareDown=index;down=clicks>0&&Details().Contains(x,y);readyDown=clicks>0&&Ready().Contains(x,y);inviteDown=clicks>0&&Endless()&&Invite().Contains(x,y);MarkDirty();}
    void MouseUp(int x,int y,int clicks) override {
        const int shared=shareDown;shareDown=-1;
        if(shared>=0&&clicks>0&&clicks!=3&&ShareRect(shared).Contains(x,y)){const RoomShareUi::Action actions[]={RoomShareUi::Action::CopyCode,RoomShareUi::Action::CopyInvite,RoomShareUi::Action::Share};transfer.Start(actions[shared],RoomPresentation::Session(app),localIp);down=readyDown=inviteDown=false;return;}
        const bool open=down&&clicks>0&&Details().Contains(x,y);
        const bool toggle=readyDown&&clicks>0&&Ready().Contains(x,y);
        const bool invitation=inviteDown&&clicks>0&&Invite().Contains(x,y);
        down=readyDown=inviteDown=false;MarkDirty();
        if(invitation) {
            auto* s=RoomPresentation::Session(app);auto* run=app->GetRunState();
            if(s && run && s->SupportsAdmission() && s->GetRole()==NetSession::Role::HOST && s->GetPreparationPhase()!=PreparationWire::SEAL && s->GetPreparationPhase()!=PreparationWire::GO) {
                if(s->mAdmissionNewMask)s->CancelPendingAdmissions();
                else {run->mAdmission.allow=!run->mAdmission.allow;app->mAdmissionDirty=true;s->RequestRoomGate(false);}
            }
            return;
        }
        if(toggle){ToggleReady();return;}
        if(open&&!app->GetDialog(DIALOG_ONLINE_SWAP))app->AddDialog(DIALOG_ONLINE_SWAP,new OnlineSeatDialog(app,true));
    }
    void KeyDown(KeyCode key) override {
        if(key==KEYCODE_ESCAPE){ToggleReady();return;}
        if(key==KEYCODE_RETURN&&!app->GetDialog(DIALOG_ONLINE_SWAP))
            app->AddDialog(DIALOG_ONLINE_SWAP,new OnlineSeatDialog(app,true));
    }
};

// Same reconnect pause/cancel semantics, with a live compact status instead of a large notice.
class OnlineReconnectDialog : public LawnDialog {
    LawnApp* app;
    bool down=false;
    static Rect Cancel(){return Rect(486,14,166,46);}
public:
    explicit OnlineReconnectDialog(LawnApp* a):LawnDialog(a,DIALOG_ONLINE_RECONNECT,true,"","","",Dialog::BUTTONS_NONE),app(a) {
        Resize(64,92,672,140);
    }
    void Update() override {LawnDialog::Update();MarkDirty();}
    void Draw(Graphics* g) override {
        auto* s=RoomPresentation::Session(app);if(!s)return;
        SeatUi::Strip(g,Rect(0,0,mWidth,mHeight));
        SeatUi::Text(g,ModText::Tr("正在重连 · 房间 ","Reconnecting · Room ")+s->GetRoomCode(),18,35,450,24,Color(245,230,165));
        const auto progress=ModText::Tr("自动重连还剩 ","Reconnecting: ")+std::to_string(s->GetReconnectSecondsLeft())+ModText::Tr("秒","s left");
        SeatUi::Text(g,progress,18,80,634,22,Color(220,225,210));
        SeatUi::Text(g,ModText::Tr("恢复连接后继续；点「返回主菜单」会停止重连。","Play resumes when connected. Leave game stops reconnecting."),18,124,634,18,Color(190,210,225));
        SeatUi::Button(g,Cancel(),ModText::Tr("返回主菜单","Leave game"),true,down);
    }
    void MouseDown(int x,int y,int clicks) override {down=clicks>0&&Cancel().Contains(x,y);MarkDirty();}
    void MouseUp(int x,int y,int clicks) override {
        bool cancel=down&&clicks>0&&Cancel().Contains(x,y);down=false;MarkDirty();
        if(cancel)app->OnlineReconnectCancelled();
    }
    void KeyDown(KeyCode key) override {if(key==KEYCODE_ESCAPE)app->OnlineReconnectCancelled();}
};
