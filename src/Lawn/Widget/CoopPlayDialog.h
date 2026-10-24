#pragma once
#include "LawnDialog.h"
#include "OnlineUiDraw.h"
#include "../Online/RoomPresentation.h"
#include "../CoopPlay.h"
#include "../Board.h"
#include "../SeedPacket.h"
#include <set>

// @pvz-online: target, plant and Send share one page; optional details preserve both selections.
class CoopDialog : public LawnDialog {
    CoopPlay* coop;
    uint64_t peer=0;
    int plant=-1,plantPage=0,detailPage=0,down=-1;
    bool details=false,wasLoanActive=false;
    static Rect Hide(){return Rect(674,42,78,44);}
    static Rect Send(){return Rect(560,508,192,46);}
    static Rect Cancel(){return Rect(348,508,192,46);}
    static Rect Detail(){return Rect(48,508,192,46);}
    static Rect Prev(){return Rect(48,440,88,44);}
    static Rect Next(){return Rect(256,440,88,44);}
    static Rect Card(int i){return Rect(48+(i%4)*178,212+(i/4)*112,166,104);}
    static Rect Peer(int i){return Rect(48+i*142,150,134,46);}
    NetSession* Session() const{return RoomPresentation::Session(coop->app);}
    bool LoanActive() const {
        auto* run=coop->app->GetRunState();return run&&run->mLoanLevel==run->mLevelIndex&&run->mLoanPlant>=0;
    }
    int SeatAt(int index) const {
        auto* s=Session();if(!s)return 0;
        for(int seat=1;seat<=NetProto::MAX_PLAYERS;++seat)
            if(s->IsSeatOccupied((uint8_t)seat)&&seat!=s->GetLocalSeat())if(index--==0)return seat;
        return 0;
    }
    bool CanBorrow() const {
        auto* s=Session();int seat=s?s->FindCoopSeat(peer):0;
        return coop->Preparing()&&!coop->loanWaiting&&coop->pendingCancels.empty()&&!LoanActive()
            &&seat&&s->IsSeatOccupied((uint8_t)seat)&&!s->IsSeatOffline((uint8_t)seat);
    }
    std::vector<std::string> Traits() const {
        std::vector<std::string> lines;auto* run=coop->app->GetRunState();if(!run)return lines;
        const int seed=coop->hasIncoming?coop->incoming.plant:run->mLoanPlant;
        std::set<int> ids;
        for(const auto& b:run->mBuffs)if(b.mCount&&GetRunChoicePlant(b.mId)==(SeedType)seed)ids.insert(b.mId);
        if(!coop->hasIncoming)for(const auto& b:run->mLoanBuffs)if(b.mCount)ids.insert(b.mId);
        for(int id:ids){std::string line=GetRunChoiceDisplayName(id);
            if(coop->hasIncoming)line+=" x"+std::to_string(run->GetBuffCount(id));
            else line+=ModText::Tr(" · 自己 "," · own ")+std::to_string(run->GetBuffCount(id))+
                ModText::Tr(" → 生效 "," -> active ")+std::to_string(run->GetEffectiveBuffCount(id));
            lines.push_back(line);
        }
        return lines;
    }
    std::string Status() const {
        auto* s=Session();
        if(coop->hasIncoming)return ModText::Tr("队友想借植物，请同意或拒绝。", "A teammate wants a plant. Accept or decline.");
        if(!coop->pendingCancels.empty())return ModText::Tr("等待队友确认取消，再申请新借用。", "Wait for the teammate to confirm cancellation before borrowing again.");
        if(coop->loanWaiting){int seat=s?s->FindCoopSeat(coop->pendingLoan.target):0;
            return "P"+std::to_string(seat)+ModText::Tr(" · 等待答复：", " · awaiting reply: ")+GetRunPlantName((SeedType)coop->pendingLoan.plant);}
        if(coop->giftWaiting)return ModText::Tr("已付100阳光，预计到账90；等待确认。", "100 sun paid; expected delivery 90. Awaiting confirmation.");
        if(!coop->notice.empty())return coop->notice;
        return coop->Preparing()?ModText::Tr("选队友、选植物，然后发送申请。","Choose a teammate and plant, then send the request."):
            ModText::Tr("奖励或选卡时可申请借用。", "Request a loan during rewards or seed selection.");
    }
    int Hit(int x,int y) const {
        if(Hide().Contains(x,y))return 90;
        if(coop->hasIncoming) {
            if(Send().Contains(x,y))return 80;
            if(Cancel().Contains(x,y))return 81;
        } else {
            if(Detail().Contains(x,y)&&LoanActive())return 92;
            if(Cancel().Contains(x,y)&&coop->loanWaiting)return 93;
            if(Cancel().Contains(x,y)&&LoanActive()&&coop->Preparing()&&coop->pendingCancels.empty())return 95;
            if(!details&&Send().Contains(x,y)&&plant>=0&&CanBorrow())return 94;
            if(!details) {
                for(int i=0;i<5;++i)if(SeatAt(i)&&Peer(i).Contains(x,y))return 100+i;
                if(CanBorrow())for(int i=0;i<8;++i)if(Card(i).Contains(x,y))return 200+i;
            }
        }
        if(Prev().Contains(x,y))return 82;
        if(Next().Contains(x,y))return 83;
        return -1;
    }
public:
    explicit CoopDialog(CoopPlay* c):LawnDialog(c->app,DIALOG_COOP_PLAY,true,"","","",Dialog::BUTTONS_NONE),coop(c) {
        Resize(0,0,800,600);details=LoanActive();
    }
    void Update() override {LawnDialog::Update();bool active=LoanActive();if(active&&!wasLoanActive){details=true;detailPage=0;}wasLoanActive=active;MarkDirty();}
    void Draw(Graphics* g) override {
        SeatUi::Panel(g,Rect(20,20,760,560));
        SeatUi::Text(g,ModText::Tr("向队友借植物", "Borrow a plant"),48,78,610,27,Color(245,230,165));
        SeatUi::Button(g,Hide(),ModText::Tr("收起","Hide"),true,down==90);
        SeatUi::Strip(g,Rect(48,96,704,44));
        SeatUi::Paragraph(g,Status(),Rect(58,99,684,40),17,Color(235,225,185));
        auto* s=Session();auto* run=coop->app->GetRunState();
        if(coop->hasIncoming||details) {
            const int seed=coop->hasIncoming?coop->incoming.plant:run?run->mLoanPlant:-1;
            std::string who=coop->hasIncoming?(s?s->GetSeatName(coop->incoming.src):""):coop->mLoanLenderName;
            SeatUi::Text(g,who,48,179,704,22,Color(190,215,225));
            SeatUi::Text(g,seed>=0?GetRunPlantName((SeedType)seed):ModText::Tr("借用已结束","Loan ended"),48,218,704,24,Color(245,230,165));
            auto lines=Traits();int pages=std::max(1,((int)lines.size()+3)/4);detailPage=std::clamp(detailPage,0,pages-1);
            if(lines.empty())SeatUi::Text(g,ModText::Tr("没有专属词条","No plant-specific traits"),48,270,704,21,Color(210,220,225));
            for(int i=detailPage*4;i<(int)lines.size()&&i<(detailPage+1)*4;++i)
                SeatUi::Paragraph(g,lines[i],Rect(48,240+(i%4)*46,704,44),18,Color(220,225,210));
            SeatUi::Text(g,std::to_string(detailPage+1)+" / "+std::to_string(pages),144,469,104,20,Color(245,230,165),true);
            SeatUi::Text(g,coop->hasIncoming?ModText::Tr("借出后你仍可使用，本关有效。","You keep your plant; loan lasts this level."):
                ModText::Tr("临时词条仅本关有效，永久层数不变。","Temporary traits expire after this level."),48,497,704,17,Color(190,215,225));
        } else {
            for(int i=0;i<5;++i){int seat=SeatAt(i);if(!seat)break;Rect box=Peer(i);auto token=s->mCoopPeers[seat];
                bool enabled=token&&!s->IsSeatOffline((uint8_t)seat)&&!coop->loanWaiting&&coop->pendingCancels.empty();
                SeatUi::Button(g,box,"",enabled,token&&token==peer);
                SeatUi::Text(g,"P"+std::to_string(seat)+" "+s->GetSeatName((uint8_t)seat),box.mX+7,box.mY+21,120,17,Color(245,230,165),true);
                SeatUi::Text(g,s->IsSeatOffline((uint8_t)seat)?ModText::Tr("离线","Offline"):!token?ModText::Tr("请稍等", "Please wait"):token==peer?ModText::Tr("已选中","Selected"):ModText::Tr("选择队友","Select"),box.mX+7,box.mY+40,120,15,Color(190,215,225),true);
            }
            if(!SeatAt(0))SeatUi::Text(g,ModText::Tr("暂无可协作队友","No teammates available"),48,180,704,22,Color(190,215,225));
            for(int i=0;i<8;++i){Rect box=Card(i);int seed=plantPage*8+i;
                SeatUi::Strip(g,box);if(seed==plant){Graphics border(*g);border.SetColor(Color(240,215,110));border.DrawRect(box);}
                DrawSeedPacket(g,(float)(box.mX+57),(float)(box.mY+3),(SeedType)seed,SEED_NONE,0,CanBorrow()?255:140,true,false);
                SeatUi::Text(g,GetRunPlantName((SeedType)seed),box.mX+6,box.mY+97,154,17,Color(245,230,165),true);
            }
            SeatUi::Text(g,std::to_string(plantPage+1)+" / 6",144,469,104,20,Color(245,230,165),true);
            SeatUi::Text(g,plant<0?ModText::Tr("请选择植物", "Choose a plant"):GetRunPlantName((SeedType)plant),360,469,392,20,Color(245,230,165));
            SeatUi::Text(g,ModText::Tr("每关限借一株；队友须拥有并同意借出。", "One loan per level; the owner must have the plant and approve."),48,497,704,17,Color(190,215,225));
        }
        SeatUi::Button(g,Prev(),ModText::Tr("上页","Prev"),true,down==82);
        SeatUi::Button(g,Next(),ModText::Tr("下页","Next"),true,down==83);
        if(coop->hasIncoming){SeatUi::Button(g,Cancel(),ModText::Tr("拒绝","Decline"),true,down==81);SeatUi::Button(g,Send(),ModText::Tr("同意申请","Accept"),true,down==80);}
        else {
            if(LoanActive())SeatUi::Button(g,Detail(),details?ModText::Tr("返回选择","Back"):ModText::Tr("植物详情","Loan details"),true,down==92);
            if(coop->loanWaiting)SeatUi::Button(g,Cancel(),ModText::Tr("取消申请","Cancel request"),true,down==93);
            else if(LoanActive())SeatUi::Button(g,Cancel(),ModText::Tr("取消使用","Return plant"),coop->Preparing()&&coop->pendingCancels.empty(),down==95);
            if(!details)SeatUi::Button(g,Send(),coop->loanWaiting?ModText::Tr("等待答复","Waiting"):ModText::Tr("发送申请","Send request"),plant>=0&&CanBorrow(),down==94);
        }
    }
    void MouseDown(int x,int y,int clicks) override {down=clicks>0?Hit(x,y):-1;MarkDirty();}
    void MouseUp(int x,int y,int clicks) override {
        int action=Hit(x,y),pressed=down;down=-1;MarkDirty();if(clicks<=0||pressed<0||action!=pressed)return;
        if(action==90){coop->Hide();return;}
        if(action==80||action==81){coop->Answer(action==80);detailPage=0;return;}
        if(action==92){details=!details;detailPage=0;return;}
        if(action==93){coop->CancelLoanRequest();return;}
        if(action==95){coop->ReturnLoan();if(!LoanActive()){details=false;detailPage=0;wasLoanActive=false;}return;}
        if(action==94){if(plant>=0&&CanBorrow())coop->Ask(peer,(SeedType)plant);return;}
        if(action==82||action==83){int delta=action==82?-1:1;
            if(coop->hasIncoming||details){int pages=std::max(1,((int)Traits().size()+3)/4);detailPage=std::clamp(detailPage+delta,0,pages-1);}
            else plantPage=(plantPage+6+delta)%6;return;}
        if(action>=100&&action<105){int seat=SeatAt(action-100);auto* s=Session();if(seat&&s->mCoopPeers[seat]&&!s->IsSeatOffline((uint8_t)seat)&&!coop->loanWaiting&&coop->pendingCancels.empty())peer=s->mCoopPeers[seat];return;}
        if(action>=200&&action<208&&CanBorrow())plant=plantPage*8+action-200;
    }
    void KeyDown(KeyCode key) override {if(key==KEYCODE_ESCAPE)coop->Hide();}
};
