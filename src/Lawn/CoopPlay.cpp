#include "CoopPlay.h"
#include "TeamPlay.h"
#include "../LawnApp.h"
#include "Board.h"
#include "Plant.h"
#include "SeedPacket.h"
#include "Widget/LawnDialog.h"
#include "Widget/SeedChooserScreen.h"
#include "System/PlayerInfo.h"
#include "../Resources.h"
#include "ModText.h"
#include "graphics/Graphics.h"
#include "graphics/Font.h"
#include <algorithm>
#include <climits>
#ifdef __ANDROID__
#include "Widget/MobileCoopDraw.h"
#include "../PvzpLib/PvzpStringFile.h"
#else
#endif

static Board* CoopBoard(LawnApp* app) {
#ifdef __ANDROID__
    return app->mBoard;
#else
    return app->mBoard;
#endif
}
static NetSession* CoopSession(LawnApp* app) {
#ifdef __ANDROID__
    return app->mOnlineSession.get();
#else
    return app->mOnlineSession;
#endif
}
#include "Widget/CoopPlayDialog.h"
static uint32_t CoopAttempt(LawnApp* app){auto*n=CoopSession(app);auto*r=app->GetRunState();return n&&n->IsActive()?std::max(1u,n->GetReadyEpoch()):r?std::max(1u,r->mTeam.attempt):1;}


bool CoopPlay::Preparing() const {
    auto* run=app->GetRunState();
    return app->IsOnlineGame() && run && app->mGameScene==SCENE_LEVEL_INTRO &&
        (app->mRunIntroHeld || app->mOnlineSeedsHeld || run->HasPendingPick() || app->mSeedChooserScreen!=nullptr);
}
bool CoopPlay::Blocking() const{return app->GetDialog(DIALOG_COOP_PLAY)!=nullptr || hasIncoming || loanWaiting;}
void CoopPlay::Show(){auto* session=CoopSession(app);if(session&&session->IsConnected()&&app->GetRunState()&&!app->GetDialog(DIALOG_COOP_PLAY))app->AddDialog(DIALOG_COOP_PLAY,new CoopDialog(this));}
CoopWire::Event CoopPlay::Message(uint8_t op) {
    CoopWire::Event e;e.op=op;
    if(auto*run=app->GetRunState()){serial=std::max(serial,run->mTeam.coopSerial);e.run=(uint32_t)run->mRunSeed;e.level=run->mLevelIndex;}
    e.serial=serial==UINT32_MAX?0:++serial;e.attempt=CoopAttempt(app);return e;
}
bool CoopPlay::Send(CoopWire::Event event,uint64_t target) {
    auto* session=CoopSession(app);if(!session)return false;
    if(target){uint8_t seat=session->FindCoopSeat(target);if(!seat||session->IsSeatOffline(seat))return false;event.target=target;return session->SendCoop(event,seat);}
    return session->SendCoop(event);
}
bool CoopPlay::RequestSun() {
    auto* b=CoopBoard(app);if(!b||(app->mGameScene!=SCENE_PLAYING || app->AdmissionBuilding()) || requestCooldown>0){notice=ModText::Tr("战斗中可求援；请等待请求冷却结束","Request sun during battle after its cooldown");return false;}
    auto e=Message(CoopWire::SUN_REQUEST);if(!Send(e)){notice=ModText::Tr("求援未发送","Request could not be sent");return false;}
    requestId=e.serial;requestGrace=0;requestTicks=1000;requestCooldown=1500;notice=ModText::Tr("已求援：十秒内可响应一份阳光","Sun requested for 10 seconds");return true;
}
bool CoopPlay::SendGift(uint64_t token,uint32_t request,bool proactive) {
    auto*b=CoopBoard(app);auto*session=CoopSession(app);auto*run=app->GetRunState();
    const uint8_t seat=session?session->FindCoopSeat(token):0;
    if(!run||!b||!seat||session->IsSeatOffline(seat)||app->mGameScene!=SCENE_PLAYING||app->mRunIntroHeld||app->AdmissionBuilding()||
       run->mTeam.gift.phase==TeamProgression::RESERVED||run->mTeam.giftCooldown>0||sunCooldown>0||b->mSunGiftCooldownFrames>0||b->mSunMoney-std::min(b->mSunMoney,run->mTeam.restrictedSun)<100){
        notice=ModText::Tr("暂不能赠送：队友离线、上次未确认、阳光不足或冷却中", "Cannot give sun: teammate offline, previous gift pending, low sun or cooldown");return false;
    }
    gift=Message(CoopWire::SUN_GIFT);if(!gift.serial)return false;gift.request=request;gift.target=token;gift.result=proactive?1:0;
    const auto before=run->mTeam;const int sun=b->mSunMoney,oldCooldown=sunCooldown,oldBoardCooldown=b->mSunGiftCooldownFrames;
    auto&g=run->mTeam.gift;g={};g.peer=token;g.id=gift.serial;g.run=gift.run;g.level=gift.level;g.attempt=gift.attempt;g.request=request;g.phase=TeamProgression::RESERVED;g.proactive=proactive;
    run->mTeam.coopSerial=gift.serial;run->mTeam.giftCooldown=sunCooldown=b->mSunGiftCooldownFrames=300;b->mSunMoney-=100;
    if(!TeamPlay::SaveCoopTransaction(app)){run->mTeam=before;b->mSunMoney=sun;sunCooldown=oldCooldown;b->mSunGiftCooldownFrames=oldBoardCooldown;notice=ModText::Tr("无法保存赠送，未扣阳光","Could not save the gift; no sun spent");return false;}
    giftWaiting=true;giftTicks=0;
    Send(gift,token); // A lost send or acknowledgment is retried with the durable identity.
    notice=ModText::Tr("赠送待确认","Gift awaiting confirmation");return true;
}
bool CoopPlay::Aid(uint64_t token) {
    auto it=requests.find(token);
    if(it==requests.end()){notice=ModText::Tr("请求已结束","Request ended");return false;}
    return SendGift(token,it->second.serial,false);
}
bool CoopPlay::GiveSun(uint8_t seat) {
    auto* session=CoopSession(app);
    if(!session || seat==session->GetLocalSeat() || !session->IsSeatOccupied(seat) || session->IsSeatOffline(seat)) {
        notice=ModText::Tr("队友不在线，未扣阳光","Teammate offline; no sun spent");return false;
    }
    if(session->GetSeatBuild(seat)<55) {
        notice=ModText::Tr("请让队友更新游戏后再试", "Ask the teammate to update the game, then retry");return false;
    }
    if(!session->mCoopPeers[seat]) {
        Send(Message(CoopWire::HELLO));
        notice=ModText::Tr("请等队友连接就绪后再试", "Wait for the teammate to connect, then retry");return false;
    }
    return SendGift(session->mCoopPeers[seat],0,true);
}
void CoopPlay::SunReceived() {
    if(!requestId)return;
    auto e=Message(CoopWire::SUN_DONE);e.request=requestId;Send(e);requestTicks=0;requestGrace=100;
}
bool CoopPlay::Ask(uint64_t token,SeedType plant) {
    auto* run=app->GetRunState();
    if(!Preparing()||!run||loanWaiting||!pendingCancels.empty()||run->mLoanLevel==run->mLevelIndex){notice=ModText::Tr("请在准备阶段申请，每关限借入一株","Borrow during preparation; one plant per level");return false;}
    auto e=Message(CoopWire::ASK);e.plant=(uint8_t)plant;e.target=token;
    if(!Send(e,token)){notice=ModText::Tr("借用申请未发送，请检查队友连接","Loan request not sent; check the teammate connection");return false;}
    pendingLoan=e;loanWaiting=true;loanTicks=1200;notice=ModText::Tr("借用申请已发出，等待队友确认","Loan requested; waiting for approval");return true;
}
void CoopPlay::Answer(bool accept) {
    if(!hasIncoming)return;
    auto request=incoming;hasIncoming=false;
    auto* run=app->GetRunState();bool valid=accept&&Preparing()&&run&&run->HasPlant((SeedType)request.plant)&&run->mLentLevel!=run->mLevelIndex;
    auto e=Message(valid?CoopWire::OFFER:CoopWire::DECLINE);e.request=request.serial;e.plant=request.plant;
    if(valid) {
        for(const auto& buff:run->mBuffs)if(GetRunChoicePlant(buff.mId)==(SeedType)e.plant) {
            if(e.count>=CoopWire::MAX_TRAITS){e.op=CoopWire::DECLINE;valid=false;break;}
            e.ids[e.count]=buff.mId;e.stacks[e.count++]=buff.mCount;
        }
    }
    bool sent=Send(e,request.sender);
    if(sent&&valid){lentPeer=request.sender;lentRequest=request.serial;lentCommitted=false;run->mLentLevel=run->mLevelIndex;run->Save(app->mPlayerInfo->mId);}
    notice=!sent?ModText::Tr("答复未送达，请检查连接","Reply not delivered; check the connection"):valid?ModText::Tr("已同意，本关仍可正常使用自己的植物","Approved; your own plant remains usable"):ModText::Tr("已拒绝或当前不能借出","Declined or unavailable");
}
// @pvz-online: cancellation only releases an uncommitted matching offer; never a confirmed loan.
bool CoopPlay::ReleasePendingLend(uint64_t peer,uint32_t request) {
    auto* run=app->GetRunState();
    if(!run || lentCommitted || peer!=lentPeer || request!=lentRequest || run->mLentLevel!=run->mLevelIndex)return false;
    int level=run->mLentLevel;run->mLentLevel=-1;
    if(!run->Save(app->mPlayerInfo->mId)){run->mLentLevel=level;return false;}
    lentPeer=0;lentRequest=0;return true;
}
void CoopPlay::CancelLoanRequest() {
    if(!loanWaiting)return;
    auto e=Message(CoopWire::CANCEL);e.request=pendingLoan.serial;e.target=pendingLoan.target;
    e.run=pendingLoan.run;e.level=pendingLoan.level;
    pendingCancels[{e.target,e.request}]=e;
    Send(e,e.target);loanWaiting=false;notice=ModText::Tr("正在确认取消借用","Confirming loan cancellation");
}
void CoopPlay::ReturnLoan() {
    auto* run=app->GetRunState();
    if(!Preparing()||!run||run->mLoanLevel!=run->mLevelIndex||run->mLoanPlant<0||!pendingCancels.empty())return;
    auto* session=CoopSession(app);
    if(!session)return;
    for(int seat=1;seat<=NetProto::MAX_PLAYERS;++seat) {
        if(session->IsSeatOccupied((uint8_t)seat)&&session->GetSeatBuild((uint8_t)seat)<59) {
            notice=ModText::Tr("请全队更新游戏后再取消借用", "Everyone must update the game before returning a loan");return;
        }
    }
    int plant=run->mLoanPlant,level=run->mLoanLevel;
    auto buffs=run->mLoanBuffs;
    run->mLoanPlant=run->mLoanLevel=-1;run->mLoanBuffs.clear();
    bool saved=run->Save(app->mPlayerInfo->mId);
    run->mLoanPlant=plant;run->mLoanLevel=level;run->mLoanBuffs=buffs;
    if(!saved){notice=ModText::Tr("保存失败，未取消借用","Save failed; loan unchanged");return;}
    ExpireLoan();
    if(!run->HasPlant((SeedType)plant)) {
        auto* chooser=app->mSeedChooserScreen.get();
        if(chooser) {
            for(int seed=0;seed<NUM_SEEDS_IN_CHOOSER;++seed)chooser->LandFlyingSeed(chooser->mChosenSeeds[seed]);
            for(int seed=0;seed<NUM_SEEDS_IN_CHOOSER;++seed) {
                auto& chosen=chooser->mChosenSeeds[seed];
                if(chosen.mSeedState==SEED_IN_BANK && (seed==plant || chosen.mImitaterType==(SeedType)plant)) {
                    chooser->ClickedSeedInBank(chosen);
                    for(int other=0;other<NUM_SEEDS_IN_CHOOSER;++other)chooser->LandFlyingSeed(chooser->mChosenSeeds[other]);
                }
            }
        }
        if(auto* board=CoopBoard(app)) {
            auto* bank=board->mSeedBank.get();
            int write=0,before=bank->mNumPackets;
            for(int index=0;index<before;++index) {
                auto& packet=bank->mSeedPackets[index];
                if(packet.mPacketType==(SeedType)plant || packet.mImitaterType==(SeedType)plant)continue;
                if(write!=index)bank->mSeedPackets[write]=packet;
                bank->mSeedPackets[write].mIndex=write;
                ++write;
            }
            for(int index=write;index<before;++index) {
                auto& packet=bank->mSeedPackets[index];
                packet.mPacketType=packet.mImitaterType=SEED_NONE;
                packet.mRefreshing=packet.mActive=false;
                packet.mRefreshCounter=packet.mRefreshTime=0;
            }
            bank->UpdateWidth();
            if(chooser)chooser->EnableStartButton(chooser->mSeedsInBank==bank->mNumPackets);
        }
    }
    if(pendingLoan.serial && pendingLoan.run==(uint32_t)run->mRunSeed && pendingLoan.level==(uint32_t)run->mLevelIndex && pendingLoan.target) {
        auto event=Message(CoopWire::RETURN_LOAN);
        event.request=pendingLoan.serial;event.target=pendingLoan.target;
        pendingCancels[{event.target,event.request}]=event;
        Send(event,event.target);
        notice=ModText::Tr("已取消借用，等待队友确认后可重新申请","Loan removed; request another after confirmation");
    } else notice=ModText::Tr("已取消借用，可重新申请","Loan removed; you can request another");
}

void CoopPlay::Capture(std::vector<int>& health,std::vector<float>& attack,std::vector<int>& refresh) {
    auto* b=CoopBoard(app);if(!b)return;
    Plant* p=nullptr;while(b->IteratePlants(p)){health.push_back(p->CoopMaxHealth());attack.push_back(p->CoopAttackPeriod());}
    for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto& card=b->mSeedBank->mSeedPackets[i];
        if(card.mPacketType==SEED_NONE){refresh.push_back(1);continue;}
        refresh.push_back(std::max(1,(int)(Plant::GetRefreshTime(card.mPacketType,card.mImitaterType)*app->RunBuffMul(RUN_BUFF_FASTSEED)*app->RunPlantUpgradeMulKind(card.mPacketType,RUN_UPGRADE_KIND_COOLDOWN)*app->RunCoffeeBeanRefreshMul(card.mPacketType,card.mImitaterType))));}
    if(app->mSeedChooserScreen)for(int seed=0;seed<48;++seed)
        refresh.push_back(std::max(1,(int)(Plant::GetRefreshTime((SeedType)seed,SEED_NONE)*app->RunBuffMul(RUN_BUFF_FASTSEED)*app->RunPlantUpgradeMulKind((SeedType)seed,RUN_UPGRADE_KIND_COOLDOWN)*app->RunCoffeeBeanRefreshMul((SeedType)seed,SEED_NONE))));
}
void CoopPlay::Rescale(const std::vector<int>& health,const std::vector<float>& attack,const std::vector<int>& refresh) {
    auto* b=CoopBoard(app);if(!b)return;Plant* p=nullptr;size_t i=0;
    while(b->IteratePlants(p)&&i<health.size()) {
        int expected=p->CoopMaxHealth();if(expected!=health[i]) {
            int maximum=(int)std::min((long long)INT_MAX,(long long)p->mPlantMaxHealth*expected/std::max(1,health[i]));maximum=std::max(1,maximum);
            p->mPlantHealth=(int)((long long)std::max(0,p->mPlantHealth)*maximum/std::max(1,p->mPlantMaxHealth));p->mPlantMaxHealth=maximum;
        }
        float period=p->CoopAttackPeriod();if(p->mLaunchCounter>0 && attack[i]>0 && period!=attack[i])p->mLaunchCounter=std::max(0,(int)(p->mLaunchCounter*(double)period/attack[i]));++i;
    }
    for(int j=0;j<b->mSeedBank->mNumPackets&&j<(int)refresh.size();++j) {
        auto& c=b->mSeedBank->mSeedPackets[j];if(c.mPacketType==SEED_NONE)continue;
        int period=std::max(1,(int)(Plant::GetRefreshTime(c.mPacketType,c.mImitaterType)*app->RunBuffMul(RUN_BUFF_FASTSEED)*app->RunPlantUpgradeMulKind(c.mPacketType,RUN_UPGRADE_KIND_COOLDOWN)*app->RunCoffeeBeanRefreshMul(c.mPacketType,c.mImitaterType)));
        if(c.mRefreshing && period!=refresh[j]){c.mRefreshCounter=(int)((long long)c.mRefreshCounter*period/refresh[j]);c.mRefreshTime=period;}
    }
    if(app->mSeedChooserScreen)for(int seed=0;seed<48;++seed) {
        auto& chosen=app->mSeedChooserScreen->mChosenSeeds[seed];size_t index=b->mSeedBank->mNumPackets+seed;
        int period=std::max(1,(int)(Plant::GetRefreshTime((SeedType)seed,SEED_NONE)*app->RunBuffMul(RUN_BUFF_FASTSEED)*app->RunPlantUpgradeMulKind((SeedType)seed,RUN_UPGRADE_KIND_COOLDOWN)*app->RunCoffeeBeanRefreshMul((SeedType)seed,SEED_NONE)));
        if(chosen.mRefreshing && index<refresh.size() && refresh[index]>0 && period!=refresh[index])
            chosen.mRefreshCounter=(int)((long long)chosen.mRefreshCounter*period/refresh[index]);
    }
}
void CoopPlay::ApplyLoan(const CoopWire::Event& e) {
    auto* run=app->GetRunState();if(!run)return;
    auto* session=CoopSession(app);
    mLoanLenderName=session?session->GetSeatName(e.src):std::string();
    if(mLoanLenderName.empty())mLoanLenderName="P"+std::to_string((unsigned)e.src);
    std::vector<RunState::BuffStack> buffs;for(int i=0;i<e.count;++i){RunState::BuffStack b;b.mId=e.ids[i];b.mCount=e.stacks[i];buffs.push_back(b);}
    if(!run->ValidateLoan((SeedType)e.plant,buffs)){notice=ModText::Tr("无法借用：增益冲突或版本不兼容", "Cannot borrow: conflicting buffs or incompatible versions");auto reply=Message(CoopWire::DECLINE);reply.request=e.request;Send(reply,e.sender);return;}
    pendingLoan=e;pendingLoan.serial=e.request;pendingLoan.target=e.sender;
    std::vector<int> hp,refresh;std::vector<float> attack;Capture(hp,attack,refresh);
    auto* currentBoard=CoopBoard(app);
    const bool choseSeeds=currentBoard&&currentBoard->ChooseSeedsOnCurrentLevel();
    run->mLoanPlant=e.plant;run->mLoanLevel=run->mLevelIndex;run->mLoanBuffs=buffs;Rescale(hp,attack,refresh);
    if(auto* board=CoopBoard(app)) {
        const auto pool=run->GetUsablePool();
        if(pool.size()<=RunState::RUN_SEED_SLOTS) {
            const int before=board->mSeedBank->mNumPackets;
            for(int index=before;index<(int)pool.size();++index)
                board->mSeedBank->mSeedPackets[index].SetPacketType(pool[index]);
            board->mSeedBank->mNumPackets=(int)pool.size();
            board->mSeedBank->UpdateWidth();
        } else if(!choseSeeds) {
            app->mOnlineSeedsHeld=false;
            session->SendSeedsReady(false);
            app->KillDialog(DIALOG_ONLINE_START);
            if(!app->mSeedChooserScreen)app->ShowSeedChooserScreen();
            if(app->mRunIntroHeld)app->ReleaseRunIntro();
        }
        if(app->mSeedChooserScreen) {
            app->mSeedChooserScreen->UpdateAfterPurchase();
            app->mSeedChooserScreen->MarkDirty();
        }
    }
    run->Save(app->mPlayerInfo->mId);notice=ModText::Tr("借用成功：同种植物各词条取双方最高层","Borrowed: same-plant traits use the higher stacks");
    auto reply=Message(CoopWire::ACCEPT);reply.request=e.request;reply.plant=e.plant;Send(reply,e.sender);
}
void CoopPlay::ExpireLoan() {
    auto* run=app->GetRunState();if(!run || run->mLoanLevel!=run->mLevelIndex)return;
    std::vector<int> hp,refresh;std::vector<float> attack;Capture(hp,attack,refresh);
    run->mLoanPlant=run->mLoanLevel=-1;run->mLoanBuffs.clear();Rescale(hp,attack,refresh);
    mLoanLenderName.clear();
}
void CoopPlay::Handle(const CoopWire::Event& e) {
    auto* session=CoopSession(app);auto* run=app->GetRunState();auto* board=CoopBoard(app);
    if(e.op==CoopWire::HELLO)return;
    if(e.op==CoopWire::SUN_RESULT) {
        if(!run)return;auto&g=run->mTeam.gift;
        if(g.phase!=TeamProgression::RESERVED||e.sender!=g.peer||e.request!=g.id||e.run!=g.run||e.level!=g.level||e.attempt!=g.attempt||e.plant>200)return;
        const auto before=run->mTeam;const int sun=board?board->mSunMoney:0,oldCooldown=sunCooldown,oldBoardCooldown=board?board->mSunGiftCooldownFrames:0;
        const bool refunded=!e.result&&board&&(uint32_t)run->mRunSeed==g.run&&(uint32_t)run->mLevelIndex==g.level&&CoopAttempt(app)==g.attempt;
        g.phase=e.result?TeamProgression::EXECUTED:TeamProgression::REJECTED;g.actual=e.result?e.plant:0;
        if(e.result)run->mTeam.giftSun=TeamGoals::Add(run->mTeam.giftSun,std::min(90,int(e.plant)));
        if(refunded){board->mSunMoney=int(std::min<int64_t>(INT_MAX,int64_t(sun)+100));run->mTeam.giftCooldown=sunCooldown=board->mSunGiftCooldownFrames=0;}
        if(!TeamPlay::SaveCoopTransaction(app,refunded)){run->mTeam=before;sunCooldown=oldCooldown;if(board){board->mSunMoney=sun;board->mSunGiftCooldownFrames=oldBoardCooldown;}return;}
            giftWaiting=false;notice=e.result?(ModText::Tr("队友已收到阳光：","Sun received by teammate: ")+std::to_string(e.plant)):
            refunded?ModText::Tr("未到账，阳光已退回","Not delivered; sun returned"):ModText::Tr("未到账，原关卡已结束","Not delivered; original level ended");
        if(board)board->PushCustomBanner(notice.c_str());return;
    }
    if(e.op==CoopWire::SUN_GIFT) {
        if(!run||!board||!e.sender||!e.serial||!e.attempt)return;
        auto reply=Message(CoopWire::SUN_RESULT);reply.request=e.serial;reply.run=e.run;reply.level=e.level;reply.attempt=e.attempt;
        auto&records=run->mTeam.giftReceipts;
        auto previous=std::find_if(records.begin(),records.end(),[&](const auto&g){return g.peer==e.sender;});
        if(previous!=records.end()&&e.serial<=previous->id){
            if(e.serial==previous->id&&e.run==previous->run&&e.level==previous->level&&e.attempt==previous->attempt){reply.result=previous->phase==TeamProgression::EXECUTED;reply.plant=previous->actual;}
            else reply.result=1; // An older identity was already settled; never grant it again.
            Send(reply,e.sender);return;
        }
        if(previous==records.end()&&records.size()>=Admission::MAX_PLAYERS_HISTORY){Send(reply,e.sender);return;}
        const auto before=run->mTeam;const int sun=board->mSunMoney;
        reply.result=app->mGameScene==SCENE_PLAYING&&!app->mRunIntroHeld&&!app->AdmissionBuilding()&&e.run==(uint32_t)run->mRunSeed&&e.level==(uint32_t)run->mLevelIndex&&e.attempt==CoopAttempt(app)&&
            ((e.result==1&&e.request==0)||(e.result==0&&e.request==requestId&&(requestTicks>0||requestGrace>0)));
        if(reply.result){
            const int amount=TeamProgression::RelaySun(run->mTeam.shared.stacks[TeamProgression::RELAY]);
            board->mSunMoney=int(std::min<int64_t>(INT_MAX,int64_t(sun)+amount));reply.plant=uint8_t(board->mSunMoney-sun);
            run->mTeam.restrictedSun=int(std::min<int64_t>(INT_MAX,int64_t(std::min(sun,run->mTeam.restrictedSun))+reply.plant));
        }
        TeamProgression::Gift record;record.peer=e.sender;record.id=e.serial;record.run=e.run;record.level=e.level;record.attempt=e.attempt;record.request=e.request;record.phase=reply.result?TeamProgression::EXECUTED:TeamProgression::REJECTED;record.actual=reply.plant;
        if(previous==records.end())records.push_back(record);else *previous=record;
        if(!TeamPlay::SaveCoopTransaction(app)){run->mTeam=before;board->mSunMoney=sun;return;}
        if(reply.result){board->mSunMoney=sun;board->GiveSunGift(reply.plant,e.src);}
        Send(reply,e.sender);return;
    }
    if(!run||e.run!=(uint32_t)run->mRunSeed||e.level!=(uint32_t)run->mLevelIndex)return;
    if(e.op==CoopWire::SUN_REQUEST) {
        if((app->mGameScene==SCENE_PLAYING && !app->AdmissionBuilding())){requests[e.sender]=e;requestLife[e.sender]=1000;
            if(board)board->PushCustomBanner((session->GetSeatName(e.src)+ModText::Tr("请求阳光 · 点击赠送响应"," requests sun · tap Give to respond")).c_str());}
    } else if(e.op==CoopWire::SUN_DONE) {
        auto it=requests.find(e.sender);if(it!=requests.end()&&it->second.serial==e.request){requests.erase(it);requestLife.erase(e.sender);}
    } else if(e.op==CoopWire::RETURN_LOAN) {
        auto key=std::make_pair(e.sender,e.serial);
        auto previous=receipts.find(key);
        if(previous!=receipts.end()){Send(previous->second,e.sender);return;}
        bool matching=e.sender==lentPeer && e.request==lentRequest && run->mLentLevel==run->mLevelIndex;
        bool committed=lentCommitted;
        if(matching&&Preparing()) {
            lentCommitted=false;
            if(!ReleasePendingLend(e.sender,e.request)){lentCommitted=committed;return;}
        }
        auto reply=Message(CoopWire::RETURN_RESULT);reply.request=e.request;
        reply.result=!matching||Preparing();
        if(receipts.size()>=128)receipts.erase(receipts.begin());
        receipts[key]=reply;Send(reply,e.sender);
    } else if(e.op==CoopWire::RETURN_RESULT) {
        auto pending=pendingCancels.find({e.sender,e.request});
        if(pending!=pendingCancels.end()&&pending->second.op==CoopWire::RETURN_LOAN) {
            pendingCancels.erase(pending);
            notice=e.result?ModText::Tr("已取消借用，可以重新申请","Loan returned; you can request another"):
                ModText::Tr("借用已取消；队友已开战，本关借出名额不退回","Loan removed; teammate already started, lending slot stays used");
        }
    } else if(e.op==CoopWire::ASK) {
        if(!Preparing()||hasIncoming||!run->HasPlant((SeedType)e.plant)||run->mLentLevel==run->mLevelIndex) {
            auto reply=Message(CoopWire::DECLINE);reply.request=e.serial;Send(reply,e.sender);return;
        }
        incoming=e;hasIncoming=true;incomingTicks=1200;Show();
    } else if(e.op==CoopWire::OFFER) {
        if(loanWaiting&&Preparing()&&e.sender==pendingLoan.target&&e.request==pendingLoan.serial&&e.plant==pendingLoan.plant) {
            loanWaiting=false;ApplyLoan(e);
        } else {auto reply=Message(CoopWire::DECLINE);reply.request=e.request;Send(reply,e.sender);}
    } else if(e.op==CoopWire::ACCEPT) {
        auto pending=pendingCancels.find({e.sender,e.request});
        if(pending!=pendingCancels.end()&&pending->second.op==CoopWire::CANCEL) {
            pendingCancels.erase(pending);notice=ModText::Tr("借用已生效，请打开借用详情查看", "Loan active. Open Loan details to view it.");
        }
        if(e.sender==lentPeer && e.request==lentRequest){lentCommitted=true;notice=ModText::Tr("队友已确认借入","Teammate confirmed the loan");}
    } else if(e.op==CoopWire::DECLINE) {
        auto pending=pendingCancels.find({e.sender,e.request});
        if(pending!=pendingCancels.end()&&pending->second.op==CoopWire::CANCEL) {
            pendingCancels.erase(pending);notice=ModText::Tr("取消借用已确认","Loan cancellation confirmed");
        }
        ReleasePendingLend(e.sender,e.request);
        if(loanWaiting&&e.sender==pendingLoan.target&&e.request==pendingLoan.serial){loanWaiting=false;notice=ModText::Tr("队友拒绝或当前无法借出","Teammate declined or cannot lend now");}
    } else if(e.op==CoopWire::CANCEL) {
        if(hasIncoming&&e.sender==incoming.sender&&e.request==incoming.serial)hasIncoming=false;
        bool committed=e.sender==lentPeer && e.request==lentRequest && lentCommitted;
        bool pending=e.sender==lentPeer && e.request==lentRequest && !lentCommitted && run->mLentLevel==run->mLevelIndex;
        bool released=ReleasePendingLend(e.sender,e.request);
        if(pending && !released)return; // storage failure: let the requester retry cancellation.
        auto reply=Message(committed?CoopWire::ACCEPT:CoopWire::DECLINE);reply.request=e.request;Send(reply,e.sender);
    }
}
void CoopPlay::Update() {
    auto* session=CoopSession(app);auto* run=app->GetRunState();
    if(run){sunCooldown=run->mTeam.giftCooldown;if(auto*b=CoopBoard(app))b->mSunGiftCooldownFrames=sunCooldown;}
    giftWaiting=run&&run->mTeam.gift.phase==TeamProgression::RESERVED;
    if(giftWaiting){const auto&g=run->mTeam.gift;gift={};gift.op=CoopWire::SUN_GIFT;gift.serial=g.id;gift.target=g.peer;gift.run=g.run;gift.level=g.level;gift.attempt=g.attempt;gift.request=g.request;gift.result=g.proactive?1:0;}
    if(!run || !session||!session->IsConnected()) {
        if(!run){giftWaiting=false;pendingCancels.clear();}
        else CancelLoanRequest();
        hasIncoming=loanWaiting=false;requests.clear();requestLife.clear();requestTicks=0;requestGrace=0;
        if(app->GetDialog(DIALOG_COOP_PLAY))app->KillDialog(DIALOG_COOP_PLAY);return;
    }
    if(observedToken!=session->mCoopToken){observedToken=session->mCoopToken;pendingCancels.clear();receipts.clear();giftHigh.clear();requestId=0;requestGrace=0;}
    if((app->mGameScene!=SCENE_PLAYING || app->AdmissionBuilding())){requests.clear();requestLife.clear();requestId=0;requestTicks=requestGrace=0;}
    if(--helloTicks<=0){helloTicks=100;Send(Message(CoopWire::HELLO));
        for(const auto& item:pendingCancels)Send(item.second,item.second.target);
    }
    if(run && (observedRun!=run->mRunSeed||observedLevel!=run->mLevelIndex)) {
        observedRun=run->mRunSeed;observedLevel=run->mLevelIndex;hasIncoming=loanWaiting=false;
        pendingCancels.clear();lentPeer=0;lentRequest=0;lentCommitted=false;requests.clear();requestLife.clear();requestTicks=0;requestId=0;requestGrace=0;mLoanLenderName.clear();
        if(app->GetDialog(DIALOG_COOP_PLAY))app->KillDialog(DIALOG_COOP_PLAY);
    }
    if(requestGrace>0)--requestGrace;
    if(requestCooldown>0)--requestCooldown;if(sunCooldown>0)--sunCooldown;
    run->mTeam.giftCooldown=sunCooldown;if(auto*b=CoopBoard(app))b->mSunGiftCooldownFrames=sunCooldown;
    if(requestTicks>0 && --requestTicks==0){auto e=Message(CoopWire::SUN_DONE);e.request=requestId;Send(e);}
    for(auto it=requestLife.begin();it!=requestLife.end();) {
        if(--it->second<=0||!session->FindCoopSeat(it->first)){requests.erase(it->first);it=requestLife.erase(it);}else ++it;
    }
    if(hasIncoming && (--incomingTicks<=0||!Preparing()))Answer(false);
    if(loanWaiting && (--loanTicks<=0||!Preparing())){CancelLoanRequest();notice=ModText::Tr("借用申请已结束","Loan request ended");}
    if(giftWaiting && ++giftTicks%100==0)Send(gift,gift.target); // retry same identity; no refund on uncertainty
    CoopWire::Event event;while(session->TakeCoop(event))Handle(event);
}

void CoopPlay::DrawEntry(Graphics* g,int x,int y,int w,int h,bool flat)
{
    auto* s=CoopSession(app);if(!s||!s->IsConnected()||!app->GetRunState())return;
    // Pending requests remain discoverable after Hide; hiding is not network cancellation.
    const char* label=hasIncoming?ModText::Tr("植物申请","Loan request"):
        !pendingCancels.empty()?ModText::Tr("取消待确认","Cancelling"):
        loanWaiting?ModText::Tr("等待答复","Loan pending"):
        giftWaiting?ModText::Tr("阳光待确认","Gift pending"):ModText::Tr("队友互助","Teamwork");
    if(flat){
        SeatUi::Strip(g,Rect(x,y,w,h));
        SeatUi::Text(g,label,x+6,y+(h+17)/2-2,w-12,17,Color(245,230,165),true);
    }else SeatUi::Button(g,Rect(x,y,w,h),label,true);
}

bool LawnApp::SendCoopQuickChat(uint8_t id)
{
    if(!mCoopPlay)mCoopPlay=std::make_unique<CoopPlay>(this);
    if(id==9 && !mCoopPlay->RequestSun()) {
        if(auto* b=CoopBoard(this))b->PushCustomBanner(mCoopPlay->notice.c_str());return false;
    }
    auto* session=CoopSession(this);return session && session->SendQuickChat(id);
}

void CoopPlay::Close()
{
    if(hasIncoming)Answer(false);
    if(loanWaiting){CancelLoanRequest();}
    app->KillDialog(DIALOG_COOP_PLAY);
}

void CoopPlay::Hide(){app->KillDialog(DIALOG_COOP_PLAY);}
