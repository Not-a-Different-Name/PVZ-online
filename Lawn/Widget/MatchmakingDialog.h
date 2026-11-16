#pragma once
#include "LawnDialog.h"
#include "OnlineUiDraw.h"
#include "../Online/Matchmaking.h"
#include "../Online/RoomPresentation.h"
#include <fstream>

class MatchmakingDialog : public LawnDialog {
    LawnApp* app;
    Matchmaking::Client client;
    int mode=2,down=-1;
    bool joining=false,handedOff=false,cancelling=false,readOnly=false,cleaning=false;
    std::chrono::steady_clock::time_point began{},joinBegan{};
    static Rect Choice(int i){return Rect(48+i*228,112,208,54);}
    static Rect Back(){return Rect(48,478,208,48);}
    static Rect Start(){return Rect(280,478,432,48);}
    bool Busy()const{return joining||client.state==Matchmaking::Client::DISCOVERING||client.state==Matchmaking::Client::ROUTING||client.state==Matchmaking::Client::CANCELLING;}
    int Hit(int x,int y)const {
        if(Back().Contains(x,y))return 3;
        if(Start().Contains(x,y)&&!Busy()&&!cancelling)return 4;
        if(!Busy()&&!cancelling&&!readOnly)for(int i=0;i<3;++i)if(Choice(i).Contains(x,y))return i;
        return -1;
    }
    void Cancel() {
        if(cancelling)return;
        cancelling=true;
        if(joining){RoomPresentation::Session(app)->Close();joining=false;}
        client.Cancel();
        if(client.state==Matchmaking::Client::IDLE)app->KillDialog(DIALOG_MATCHMAKING);
    }
public:
    explicit MatchmakingDialog(LawnApp* a):LawnDialog(a,DIALOG_MATCHMAKING,true,"","","",Dialog::BUTTONS_NONE),app(a) {
        Resize(0,0,760,560);std::ifstream file(Sexy::GetAppDataFolder()+"userdata/matching-mode.txt");int stored=2;if(file>>stored && stored>=0&&stored<=2)mode=stored;
        auto* session=RoomPresentation::Session(app);readOnly=RoomPresentation::Active(session)&&session->mMatchMode>=0;if(readOnly)mode=session->mMatchMode;
    }
    ~MatchmakingDialog() override {if(joining&&!handedOff)RoomPresentation::Session(app)->Close();}
    void Update() override {
        LawnDialog::Update();client.Update();
        if(cancelling){if(client.state==Matchmaking::Client::IDLE)app->KillDialog(DIALOG_MATCHMAKING);return;}
        auto* session=RoomPresentation::Session(app);
        if(cleaning){if(client.state==Matchmaking::Client::IDLE){cleaning=false;client.state=Matchmaking::Client::FAILED;}MarkDirty();return;}
        if(client.state==Matchmaking::Client::READY&&!joining) {
            joining=session->StartMatchedRoom(client.target.host.c_str(),client.target.port,client.code,mode,client.ticket.data());joinBegan=std::chrono::steady_clock::now();
            if(!joining){session->Close();client.error=ModText::Tr("入房连接失败，请重试。","Could not join the room. Please retry.");client.Cancel();cleaning=client.state==Matchmaking::Client::CANCELLING;if(!cleaning)client.state=Matchmaking::Client::FAILED;}
        }
        if(joining&&session->IsConnected()&&session->mMatchMode==mode){handedOff=true;joining=false;app->KillDialog(DIALOG_MATCHMAKING);return;}
        if(joining&&(session->GetState()==NetSession::State::DEAD||std::chrono::steady_clock::now()-joinBegan>std::chrono::seconds(8))) {
            session->Close();joining=false;
            client.error=ModText::Tr("房间已开始、席位已过期或网络中断，请重新匹配。","The room started, the seat expired, or connection failed. Please match again.");
            client.Cancel();cleaning=client.state==Matchmaking::Client::CANCELLING;if(!cleaning)client.state=Matchmaking::Client::FAILED;
        }
        MarkDirty();
    }
    void Draw(Graphics* g) override {
        SeatUi::Panel(g,Rect(0,0,760,560));
        SeatUi::Text(g,ModText::Tr("一键匹配","Find a team"),48,66,664,30,Color(245,230,165),true);
        SeatUi::Text(g,ModText::Tr("先选档位；同档同版本组队，优先加入等待中的队伍。","Choose a run length. Join a waiting team with the same build."),48,94,664,17,Color(205,220,225),true);
        const int choices[]={2,1,0};
        for(int i=0;i<3;++i){auto r=Choice(i);SeatUi::Button(g,r,Matchmaking::ModeName(choices[i]),!Busy()&&!cancelling&&!readOnly,down==i);if(mode==choices[i]){g->SetColor(Color(245,210,110));g->DrawRect(r.mX+2,r.mY+2,r.mWidth-5,r.mHeight-5);}}
        const char* zh[]={"快速 5 关／标准 10 关／完整 25 关","难度 ×1 · 数量 ×1 · 波次间隔 ×1 · 首波 30 秒","增益选择 ×1 · 植物僵尸关 · 移速成长关","每新场景选 1 条僵尸强化 · 场景难度阶梯 ×1.33","BOSS：快速仅末关；标准／完整为场景3及末关","至少 2 人，由房主手动开始，最多 6 席。"};
        const char* en[]={"Quick: 5 levels / Standard: 10 / Full: 25","Difficulty x1 / Count x1 / Interval x1 / First wave 30s","Buff picks x1 / ZomBotany off / Speed growth off","1 zombie buff per scene / Scene scaling x1.33","Bosses: Quick final; Standard/Full scene 3 and final","Host starts with 2+ players. Up to 6 seats."};
        for(int i=0;i<6;++i)SeatUi::Text(g,ModText::Tr(zh[i],en[i]),48,211+i*31,664,18,Color(225,225,205),true);
        std::string status=client.error;
        if(Busy()&&!cancelling){const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-began).count();status=std::string(ModText::Tr("匹配中… ","Matching... "))+std::to_string(seconds)+ModText::Tr(" 秒","s");}
        if(cancelling)status=ModText::Tr("正在取消…","Cancelling...");
        SeatUi::Paragraph(g,status,Rect(48,405,664,60),18,client.state==Matchmaking::Client::FAILED?Color(255,175,120):Color(245,230,165));
        SeatUi::Button(g,Back(),Busy()?ModText::Tr("取消匹配","Cancel matching"):ModText::Tr("返回","Back"),!cancelling,down==3);
        SeatUi::Button(g,Start(),readOnly?ModText::Tr("返回房间","Back to room"):Busy()?ModText::Tr("匹配中…","Matching..."):ModText::Tr("一键匹配","Find a team"),!Busy()&&!cancelling,down==4);
    }
    void MouseDown(int x,int y,int clicks)override{down=clicks>0?Hit(x,y):-1;MarkDirty();}
    void MouseUp(int x,int y,int clicks)override {
        const int hit=Hit(x,y),pressed=down;down=-1;if(clicks<=0||hit<0||hit!=pressed)return;
        if(hit==3){Cancel();return;}
        if(hit==4&&readOnly){app->KillDialog(DIALOG_MATCHMAKING);return;}
        if(hit<3){const int choices[]={2,1,0};mode=choices[hit];return;}
        if(hit==4&&!RoomPresentation::Active(RoomPresentation::Session(app))) {
            Sexy::MkDir(Sexy::GetAppDataFolder()+"userdata");std::ofstream(Sexy::GetAppDataFolder()+"userdata/matching-mode.txt")<<mode;
            began=std::chrono::steady_clock::now();client.Start(mode,RoomPresentation::Session(app)->GetLocalName());
        }
    }
    void KeyDown(KeyCode key)override{if(key==KEYCODE_ESCAPE)Cancel();}
};
