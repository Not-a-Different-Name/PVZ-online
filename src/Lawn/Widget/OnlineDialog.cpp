#include "../../SexyAppFramework/Common.h"
#include "MobileCoopDraw.h"
#include "OnlineDialog.h"
#include "RoomDirectoryDialog.h"
#include "RunHistoryDialog.h"
#include "OnlineSeatDialog.h"
#include "OnlineRoomLayout.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../Online/NetSession.h"
#include "../Online/PrivateEndpoint.h"
#include "../Online/LocalHostControl.h"
#include "../ModText.h"
#include "../LawnCommon.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "widget/EditWidget.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "graphics/Graphics.h"
#include <functional>

class RoomCodeEdit : public Sexy::EditWidget {
public:
    std::function<void(const std::string&)> apply;
    std::function<void()> paste;
    RoomCodeEdit(int id,Sexy::EditListener* listener):Sexy::EditWidget(id,listener){}
    void KeyDown(Sexy::KeyCode key) override {
        if(int(key)=='V'&&mWidgetManager&&mWidgetManager->mKeyDown[Sexy::KEYCODE_CONTROL]){if(paste)paste();return;}
        Sexy::EditWidget::KeyDown(key);
    }
    void KeyText(std::string_view text) override {
        if(text.size()>1){if(apply)apply(std::string(text));return;}
        Sexy::EditWidget::KeyText(text);
    }
    void KeyChar(char letter) override {
        if(letter>='a'&&letter<='z')letter=char(letter-'a'+'A');
        if((letter>='A'&&letter<='Z')||(letter>='0'&&letter<='9')||letter=='-')Sexy::EditWidget::KeyChar(letter);
    }
};

OnlineDialog::OnlineDialog(LawnApp* app)
    : Dialog(nullptr, nullptr, DIALOG_ONLINE, true, "", "", "", BUTTONS_NONE), mApp(app)
{
    Resize(0, 0, 760, 560);
    mLocalIp = NetLink::GetLocalIPv4Text();
    auto edit = [&](int id, int x, int y, int width, const std::string& text, int max) {
        std::unique_ptr<Sexy::EditWidget> e;
        if(id==32){auto code=std::make_unique<RoomCodeEdit>(id,this);code->apply=[this](const std::string& text){ApplyInvite(text);};code->paste=[this](){ButtonDepress(15);};e=std::move(code);}else e=std::make_unique<Sexy::EditWidget>(id,this);
        e->SetFont(ModText::FontFor(Sexy::FONT_BRIANNETOD16, 22));
        e->SetColors({Sexy::Color(0,0,0,0), Sexy::Color(0,0,0,0), Sexy::Color(240,240,255), Sexy::Color::White, Sexy::Color::Black});
        e->mMaxChars = max; e->SetText(text); e->Resize(x, y, width, 36);
        AddWidget(e.get()); return e;
    };
    // Match PC: direct host and relay server are independent; session defaults
    // select their respective ports instead of sharing an editable direct port.
    mAddress = edit(30, 148, 382, 292, app->mOnlineAddress, 253);
    mCode = edit(32, 580, 228, 128, app->mOnlineRoomCode, 10);
    Sexy::MkDir(Sexy::GetAppDataFolder()+"userdata");
    RelayNodes::SetCatalogCachePath(Sexy::GetAppDataFolder()+"userdata/relay-nodes.bin");
    auto button = [&](int id, const char* text, int x, int y, int width) {
        auto b = std::make_unique<MobileStoneButton>(id, this);
        b->mLabel = text; b->SetFont(ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18, 24));
        b->SetLabelColor(Sexy::Color(245, 230, 165)); b->SetLabelHiliteColor(Sexy::Color::White); b->SetBkgColor(Sexy::Color(37, 83, 55));
        b->Resize(x, y, width, 46); AddWidget(b.get()); mButtons.push_back(std::move(b));
    };
    auto place=[&](int id,const char* label,Sexy::Rect r){button(id,label,r.mX,r.mY,r.mWidth);mButtons.back()->Resize(r);};
    using namespace OnlineRoomLayout;
    if(RoomPresentation::Active(app->mOnlineSession.get()))mPage=app->mOnlineSession->IsRelay()&&!app->mOnlineSession->IsPrivateRoom()?SERVER:LOCAL;
    place(1,ModText::Tr("开六人房","Host six"),DirectHost());
    place(2,ModText::Tr("加入","Join"),DirectJoin());
    place(3,ModText::Tr("创建房间","Create room"),Create());
    place(4,ModText::Tr("加入房间","Join room"),Join());
    place(5,ModText::Tr("断开连接","Disconnect"),Disconnect());
    place(8,ModText::Tr("队友","Seats"),Seats());
    place(6,ModText::Tr("返回","Back"),Close());
    place(7,ModText::Tr("开始游戏","Start game"),Start());
    place(9,ModText::Tr("公开房间","Public rooms"),Sexy::Rect(40,480,260,46));
    place(10,ModText::Tr("最近记录","Recent runs"),Sexy::Rect(552,34,168,42));
    place(11,ModText::Tr("公开房间：开","Public: on"),Sexy::Rect(480,440,232,32));
    place(12,ModText::Tr("单人与局域网游戏","Solo / LAN game"),LocalPage());
    place(13,ModText::Tr("在线房间","Online rooms"),ServerPage());
    place(15,ModText::Tr("粘贴加入","Paste & join"),Paste());
    place(16,ModText::Tr("复制房号","Copy code"),CopyCode());
    place(17,ModText::Tr("复制邀请","Copy invite"),CopyInvite());
    place(18,ModText::Tr("分享","Share"),Share());
    place(14,ModText::Tr("单人游戏","Play solo"),Solo());
    const auto codeBox=Code();
    mCode->Resize(codeBox.mX,codeBox.mY,codeBox.mWidth,codeBox.mHeight);
    Update();
}
OnlineDialog::~OnlineDialog()
{
    mCreation.Cancel(*mApp->mOnlineSession);
    RemoveWidget(mAddress.get()); RemoveWidget(mCode.get());
    for (auto& b : mButtons) RemoveWidget(b.get());
}
void OnlineDialog::Update()
{
    Dialog::Update(); const auto& session = *mApp->mOnlineSession;
    if(mStartingLocalHost) {
        ++mLocalHostTicks;
        if(const auto port=LocalHostControl::Port()) {
            mStartingLocalHost=false;mShownRoomCode.clear();mApp->mOnlineSession->StartPrivateRoomHost("127.0.0.1",port);
        } else if(mLocalHostTicks>1500||(mLocalHostTicks>40&&!LocalHostControl::Starting())) {
            mStartingLocalHost=false;mInputError=ModText::Tr("开房失败：","Could not host: ")+LocalHostControl::Error();LocalHostControl::Stop();
        }
    }
    mTransfer.Update();const auto pasted=mTransfer.TakePaste();if(!pasted.empty()){ApplyInvite(pasted);if(mInputError.empty())ButtonDepress(mPage==OnlineRoomLayout::LOCAL?2:4);}
    RoomInvite::Intent typed;if(mPage==OnlineRoomLayout::SERVER&&mCode->mString!=mPastedCode&&RoomInvite::Parse(mCode->mString,typed))ApplyInvite(mCode->mString);
    mAllocator.Update();
    if(mAllocator.state==RelayNodes::Allocator::READY){
        const auto candidates=mAllocator.candidates;mAllocator.Cancel();
        if(mPage==OnlineRoomLayout::SERVER&&!RoomPresentation::Active(&session))mCreation.Start(*mApp->mOnlineSession,candidates);
    }else if(mAllocator.state==RelayNodes::Allocator::UNAVAILABLE){
        mAllocator.Cancel();mInputError=ModText::Tr("建房失败，请再点「创建房间」。","Could not create a room. Choose Create room to retry.");
    }
    mCreation.Update(*mApp->mOnlineSession);mJoinLookup.Update();
    if(!mPendingJoinCode.empty()&&!mJoinLookup.loading){
        mPendingJoinCode.clear();
        if(mJoinLookup.state==RoomInvite::AutoRoomLookup::READY)mApp->mOnlineSession->StartRoomJoin(mJoinLookup.target.host.c_str(),mJoinLookup.code.c_str(),mJoinLookup.target.port);
        else mInputError=RoomShareUi::LookupError(mJoinLookup.state);
    }
    const bool active=RoomPresentation::Active(&session);
    const bool allocating=(mAllocator.state==RelayNodes::Allocator::LOADING||mCreation.Active()||mJoinLookup.loading||mStartingLocalHost||mTransfer.Busy());
    const bool local=!active&&mPage==OnlineRoomLayout::LOCAL;
    const bool server=!active&&mPage==OnlineRoomLayout::SERVER;
    for (auto& b : mButtons) {
        if(b->mId==15){b->SetVisible(local||server);b->SetDisabled(allocating||mTransfer.Busy());b->Resize(local?Sexy::Rect(536,442,176,38):OnlineRoomLayout::Paste());}
        if(b->mId>=16&&b->mId<=18){b->SetVisible(active&&session.IsRelay()&&!session.GetRoomCode().empty());b->SetDisabled(mTransfer.Busy());}
        if(b->mId==3)b->mLabel=allocating?ModText::Tr("正在连接…","Connecting..."):ModText::Tr("创建房间","Create room");
        if(b->mId==9){b->SetVisible(server);b->SetDisabled(!server||allocating);}
        if(b->mId==12||b->mId==13)b->SetVisible(!active);
        if(b->mId==14){
            b->SetVisible(local||(active&&session.IsRelay()&&!session.IsPrivateRoom()&&!mApp->mBoard));
            b->Resize(active?Sexy::Rect(40,440,420,32):OnlineRoomLayout::Solo());
            b->mLabel=mConfirmSolo?ModText::Tr("确认退房并单人开始","Confirm: leave and play solo"):
                active?ModText::Tr("返回单人游戏","Switch to solo"):ModText::Tr("单人游戏","Play solo");
        }
        if(b->mId==10)b->mLabel=active&&session.IsPrivateRoom()?ModText::Tr("复制邀请","Copy invite"):ModText::Tr("最近记录","Recent runs");
        if(b->mId==11){b->SetVisible(active&&session.IsRelay()&&!session.IsPrivateRoom()&&session.IsHostSeat()&&!session.mDirectoryStarted);
            b->mLabel=!session.mDirectoryConfirmed?ModText::Tr("正在更新…","Updating..."):
                session.mDirectoryPublic?ModText::Tr("公开房间：开","Public: on"):ModText::Tr("公开房间：关","Public: off");}
        if(b->mId>=1&&b->mId<=4){const bool visible=b->mId<=2?local:server;b->SetVisible(visible);b->SetDisabled(!visible||allocating);}
        if(b->mId==5||b->mId==8||b->mId==7)b->SetVisible(active);
        if(b->mId==5){const bool localHost=LocalHostControl::Alive()||mStartingLocalHost;
            b->SetVisible(active||localHost);b->mLabel=localHost?ModText::Tr("结束托管并离开","End & leave"):
                mApp->mBoard?ModText::Tr("保存并退房","Save & leave"):ModText::Tr("退出房间","Leave room");}
        if(b->mId==6)b->Resize(active?OnlineRoomLayout::Close():OnlineRoomLayout::Start());
        if(b->mId==7){b->SetDisabled(!OnlineRoomLayout::CanStart(mApp));
            b->mLabel=PlayerRoomText::StartLabel(session.GetState(),OnlineRoomLayout::NeedsPlayers(&session));}
    }
    for(auto* edit:{mAddress.get(),mCode.get()}) {
        const bool visible=edit==mAddress.get()?local:(server||local);
        if(!visible&&edit->mHasFocus)mApp->mWidgetManager->SetFocus(this);
        edit->SetDisabled(!visible||allocating);edit->SetVisible(visible);
    }
    const auto addressBox=OnlineRoomLayout::Address();mAddress->Resize(addressBox.mX,addressBox.mY,addressBox.mWidth,addressBox.mHeight);
    const auto codeBox=local?OnlineRoomLayout::PrivateCode():OnlineRoomLayout::Code();mCode->Resize(codeBox.mX,codeBox.mY,codeBox.mWidth,codeBox.mHeight);
    const auto& code=session.GetRoomCode();
    if(session.IsRelay()&&!code.empty()&&code!=mShownRoomCode){mCode->SetText(code);mShownRoomCode=code;}
    MarkDirty();
}
void OnlineDialog::Draw(Sexy::Graphics* g)
{
    auto* s=mApp->mOnlineSession.get();
    std::string detail=!mInputError.empty()?mInputError:s->GetStatusText();
    if(s->IsConnected()&&s->HasIncomingSwapRequest())detail=ModText::Tr("点「队友」答复换位申请。","Open Seats to answer the swap request.");
    else if(s->IsConnected()&&mApp->mOnlineWaitingStartAck)detail=RoomPresentation::WaitingSeats(s,true);
    else if(s->IsConnected()&&!s->IsHostSeat())detail=ModText::Tr("等房主选择玩法。","Wait for the host to choose a mode.");
    OnlineRoomLayout::Draw(g,mApp,detail,mLocalIp,mPage);
    if(!mTransfer.notice.empty())SeatUi::Text(g,mTransfer.notice,40,550,680,16,Sexy::Color(245,230,165),true);
    if(s->IsPrivateRoom()&&!s->GetRoomCode().empty())SeatUi::Text(g,PrivateEndpoint::Invite(s->GetRelayHost()=="127.0.0.1"?mLocalIp:s->GetRelayHost(),s->GetRelayPort(),s->GetRoomCode()),40,450,420,16,Sexy::Color(210,225,195));
    if(!mInputError.empty()&&!RoomPresentation::Active(s))SeatUi::Text(g,mInputError,48,465,664,15,Sexy::Color(255,175,120));
    if((mAllocator.state==RelayNodes::Allocator::LOADING||mCreation.Active()||mJoinLookup.loading))SeatUi::Text(g,ModText::Tr("正在连接，请稍等…","Connecting. Please wait..."),48,465,664,15,Sexy::Color(210,220,195));
    if(!RoomPresentation::Active(s)) {
        for(auto* edit:{mAddress.get(),mCode.get()}) {
            if(!edit->mVisible)continue;
            DrawEditBox(g,edit);
            if(edit->mHasFocus){g->SetColor(Sexy::Color(245,230,165));g->DrawRect(edit->mX-9,edit->mY-5,edit->mWidth+18,edit->mHeight+10);}
        }
    }
}

void OnlineDialog::ButtonDepress(int id)
{
    if(id==15){if(!RoomPresentation::Active(mApp->mOnlineSession.get())&&!mJoinLookup.loading)mTransfer.Start(RoomShareUi::Action::Paste,mApp->mOnlineSession.get());return;}
    if(id>=16&&id<=18){const RoomShareUi::Action actions[]={RoomShareUi::Action::CopyCode,RoomShareUi::Action::CopyInvite,RoomShareUi::Action::Share};mTransfer.Start(actions[id-16],mApp->mOnlineSession.get(),mLocalIp);return;}
    // Remember editable fields between openings, including after a failed connection.
    mApp->mOnlineAddress = mAddress->mString; mApp->mOnlineRoomCode = mCode->mString;
    auto* app = mApp;
    if(id==12||id==13) {
        if(RoomPresentation::Active(app->mOnlineSession.get()))return;
        mAllocator.Cancel();mCreation.Cancel(*mApp->mOnlineSession);mJoinLookup.Close();mPendingJoinCode.clear();mInputError.clear();mTransfer.Cancel();mPastedText.clear();mPastedCode.clear();
        mPage=id==12?OnlineRoomLayout::LOCAL:OnlineRoomLayout::SERVER;
        mConfirmSolo=false;Update();app->mWidgetManager->SetFocus(this);return;
    }
    if(id==14) {
        if(app->mBoard||(!RoomPresentation::Active(app->mOnlineSession.get())&&mPage!=OnlineRoomLayout::LOCAL))return;
        if(RoomPresentation::Active(app->mOnlineSession.get())&&!mConfirmSolo){mConfirmSolo=true;Update();return;}
        app->mOnlineSession->Close();const bool retry=OnlineRoomLayout::HasLostBoard(app);app->KillDialog(DIALOG_ONLINE);
        if(retry)app->RetryOnlineLevel();else app->ShowRunMenu();return;
    }
    mConfirmSolo=false;
    if(id==9){if(!RoomPresentation::Active(app->mOnlineSession.get())&&mPage==OnlineRoomLayout::SERVER&&(mAllocator.state!=RelayNodes::Allocator::LOADING&&!mCreation.Active()&&!mJoinLookup.loading))
        app->AddDialog(DIALOG_ROOM_DIRECTORY,new RoomDirectoryDialog(app));
        return;}
    if(id==10){
        auto& session=*app->mOnlineSession;
        if(session.IsPrivateRoom()&&session.IsConnected())mTransfer.Start(RoomShareUi::Action::CopyInvite,&session,mLocalIp);
        else app->AddDialog(DIALOG_RUN_HISTORY,new RunHistoryDialog(app));return;
    }
    if(id==11){
        auto& s=*app->mOnlineSession;if(s.IsRelay()&&!s.IsPrivateRoom()&&s.IsHostSeat()&&!s.mDirectoryStarted)s.SetDirectoryPublic(!s.mDirectoryPublic);return;}
    if (id == 6) { app->KillDialog(DIALOG_ONLINE); return; }
    if (id == 7) {
        if(!OnlineRoomLayout::CanStart(app))return;
        app->KillDialog(DIALOG_ONLINE);app->ShowRunMenu();return;
    }
    if (id == 5) {
        LeaveRoom();return;
    }
    if (id == 8)
    {
        // 2026-10-15：换位改开选人框（任选上座队友 + 换后预览）；请求/答复/超时在 NetSession。
        if (RoomPresentation::Active(app->mOnlineSession.get()))
            app->AddDialog(DIALOG_ONLINE_SWAP, new OnlineSeatDialog(app));
        return;
    }
    if (id < 1 || id > 4) return;
    if((id<=2&&mPage!=OnlineRoomLayout::LOCAL)||(id>=3&&mPage!=OnlineRoomLayout::SERVER))return;
    if (RoomPresentation::Active(app->mOnlineSession.get())||(mAllocator.state==RelayNodes::Allocator::LOADING||mCreation.Active()||mJoinLookup.loading)) return;
    mInputError.clear(); auto& session = *app->mOnlineSession;
    const std::string host = mAddress->mString.empty() ? "127.0.0.1" : mAddress->mString;
    if(id==1||id==2){
        if(mStartingLocalHost)return;
        PrivateEndpoint::Address target;
        if(!PrivateEndpoint::Parse(host,id==2?mCode->mString:"",target)||(id==2&&target.code.empty())){
            mInputError=ModText::Tr("填写地址:端口及四位房间码，或粘贴完整邀请。","Enter host:port and a four-character code, or paste an invite.");return;
        }
        mShownRoomCode.clear();
        if(id==1){
            std::string error;
            if(!LocalHostControl::Start(target.port,error)){mInputError=ModText::Tr("开房失败：","Could not host: ")+error;return;}
            mStartingLocalHost=true;mLocalHostTicks=0;Update();
        } else session.StartPrivateRoomJoin(target.host.c_str(),target.code.c_str(),target.port);
    }
    if (id == 3) {mShownRoomCode.clear();mAllocator.Start();Update();}
    if (id == 4) {
        mShownRoomCode.clear();const auto input=EffectiveInvite();
        if(!mJoinLookup.Start(input)){mInputError=RoomShareUi::LookupError(mJoinLookup.state);return;}
        mPendingJoinCode=input;
    }
    // Stop the IME when leaving the edit fields without closing the live room panel.
    app->mWidgetManager->SetFocus(this);
}
void OnlineDialog::LeaveRoom()
{
    auto* app=mApp;
    const bool stopHost=LocalHostControl::Alive()||mStartingLocalHost;
    const int page=stopHost||app->mOnlineSession->IsPrivateRoom()?OnlineRoomLayout::LOCAL:OnlineRoomLayout::SERVER;
    if(app->mBoard){app->DoBackToMain(false);if(app->mBoard)return;}
    app->mOnlineSession->Close();
    if(stopHost)LocalHostControl::Stop();
    app->mOnlineWaitingStartAck=false;
    app->DismissOnlineStartPrompt(false);app->ClearOnlineStartOverride();
    auto* dialog=static_cast<OnlineDialog*>(app->GetDialog(DIALOG_ONLINE));
    if(dialog!=this){app->DoOnlineDialog();dialog=static_cast<OnlineDialog*>(app->GetDialog(DIALOG_ONLINE));}
    if(!dialog)return;
    dialog->mStartingLocalHost=false;dialog->mLocalHostTicks=0;
    dialog->mAllocator.Cancel();dialog->mCreation.Cancel(*app->mOnlineSession);
    dialog->mJoinLookup.Close();dialog->mPendingJoinCode.clear();
    dialog->mTransfer.Cancel();dialog->mPastedText.clear();dialog->mPastedCode.clear();
    dialog->mShownRoomCode.clear();dialog->mCode->SetText("");app->mOnlineRoomCode.clear();
    dialog->mInputError.clear();dialog->mConfirmSolo=false;dialog->mPage=page;
    dialog->Update();app->mWidgetManager->SetFocus(dialog);
}
void OnlineDialog::EditWidgetText(int id, const std::string&)
{
    // PC Enter flow: host IP joins directly; server/code joins the relay room.
    ButtonDepress(id == 30 || mPage==OnlineRoomLayout::LOCAL ? 2 : 4);
}
void OnlineDialog::KeyDown(Sexy::KeyCode key)
{
    if (key == Sexy::KEYCODE_ESCAPE) ButtonDepress(6);
}

std::string OnlineDialog::EffectiveInvite(){return mCode->mString==mPastedCode&&!mPastedText.empty()?mPastedText:mCode->mString;}
void OnlineDialog::ApplyInvite(const std::string& input){
    if(mPage==OnlineRoomLayout::LOCAL){PrivateEndpoint::Address address;if(PrivateEndpoint::Parse(input,"",address)&&!address.code.empty()){mAddress->SetText(address.host+":"+std::to_string(address.port));mCode->SetText(address.code);mInputError.clear();return;}}
    RoomInvite::Intent parsed;if(!RoomInvite::Parse(input,parsed)){mInputError=ModText::Tr("没读到邀请，请先复制队友发来的邀请。","No invite found. Copy your teammate's invite first.");return;}
    mPastedText=input;mPastedCode=parsed.code;mCode->SetText(parsed.code);mInputError.clear();
}
