#include "TeamPlay.h"
#include "../LawnApp.h"
#include "Board.h"
#include "Zombie.h"
#include "Plant.h"
#include "SeedPacket.h"
#include "CoopPlay.h"
#include "System/PlayerInfo.h"
#include "System/SaveGame.h"
#include "Online/RoomPresentation.h"
#include "Run/RunState.h"
#include "Run/RunCheckpointIO.h"
#include "Run/TeamAttackContext.h"
#include "Run/RunDotEffects.h"
#include "Run/RunMath.h"
#include "Run/TeamGoalsStorage.h"
#include "ModText.h"
#include "../Resources.h"
#include "misc/Buffer.h"
#include <algorithm>
#include <climits>
#include <memory>
#include <fstream>
#include <iterator>

using namespace TeamProgression;
using namespace TeamProgressionView;
namespace {
std::map<LawnApp*,std::unique_ptr<TeamPlay>> controllers;
const char* Names(int id) {
    switch(id){case ARSENAL:return ModText::Tr("军备共享","Shared arsenal");case RELIEF:return ModText::Tr("胜利驰援","Victory relief");case FIRE:return ModText::Tr("跨场火力","Cross-lawn firepower");default:return "";}
}
std::string Description(int id) {
    switch(id){
    case ARSENAL:return ModText::Tr("共享全队本局获得的植物与模仿者资格，不含个人增益和临时借卡。队友离开仍保留。最多1层。", "Share plants and Imitater earned this run, excluding personal buffs and loans. Keep them after teammates leave. Max 1 stack.");
    case RELIEF:return ModText::Tr("每关首人通关时，仍在战斗的队友每层获得1000阳光，全部种子卡刷新一次。每关只触发一次。", "The first clear each level gives fighting teammates 1,000 sun per stack and refreshes all seed packets once. Once per level.");
    case FIRE:return ModText::Tr("拥有樱桃且队友允许支援时可施放。每层爆炸一枚，只付一次本人樱桃费用。独立冷却20秒，使用本人的伤害与增益。", "With Cherry Bomb and the teammate's permission, fire one explosion per stack. Pay your Cherry cost once. Separate 20-second cooldown; your damage and buffs apply.");default:return "";}
}
bool Same(const Context&a,const Context&b){return a.run==b.run&&a.player==b.player&&a.level==b.level&&a.attempt==b.attempt&&a.term==b.term&&a.revision==b.revision;}
int AddSun(int current,int add){return int(std::min<int64_t>(INT_MAX,int64_t(current)+add));}
uint64_t PlantMask(const RunState&r){uint64_t mask=0;for(auto p:r.mPool)if(p>=0&&p<=SEED_IMITATER)mask|=uint64_t(1)<<int(p);return mask;}
}
TeamPlay& TeamPlay::For(LawnApp*app){auto&c=controllers[app];if(!c)c=std::make_unique<TeamPlay>(app);return *c;}
void TeamPlay::Detach(LawnApp*app){TeamPlayUi::Install(app,nullptr);controllers.erase(app);}
NetSession* TeamPlay::Session()const{return RoomPresentation::Session(app);}
bool TeamPlay::Host()const{auto*n=Session();return !n||!n->IsActive()||(n->IsConnected()&&n->IsHostSeat());}
uint32_t TeamPlay::Attempt()const {
    auto*r=app->GetRunState();auto*s=Session();
    return s&&s->IsActive()?std::max(1u,s->GetReadyEpoch()):r?uint32_t(1+r->mFailCounts[r->mLevelIndex%RunState::RUN_LEVEL_COUNT]):1;
}
TeamWire::Message TeamPlay::Message(uint8_t op)const {
    TeamWire::Message m;m.op=op;auto*r=app->GetRunState();if(!r)return m;
    m.mode=uint8_t(r->mMode);m.seed=uint32_t(r->mRunSeed);m.level=r->mLevelIndex;m.attempt=Attempt();
    m.sender=r->mTeam.player;m.run=r->mTeam.shared.run;m.revision=r->mTeam.shared.revision;return m;
}
bool TeamPlay::Send(TeamWire::Message m,uint64_t target) {
    auto*s=Session();if(!s||!app->IsOnlineGame()||!s->IsConnected())return false;
    const uint8_t seat=target?s->FindCoopSeat(target):0;
    if(target&&(!seat||s->IsSeatOffline(seat)))return false;
    return s->SendTeam(m,seat);
}
bool TeamPlay::Capture(std::vector<unsigned char>&bytes)const {
    if(!app->mBoard)return false;
    const auto rng=Sexy::GetRandState();
    const std::string path=GetAppDataFolder()+"userdata/team-transaction-"+std::to_string(app->mPlayerInfo->mId)+".tmp";
    const bool saved=LawnSaveGame(app->mBoard,path);Sexy::RestoreRandState(rng);
    Sexy::Buffer b;const bool ok=saved&&app->ReadBufferFromFile(path,&b,false);app->EraseFile(path);
    if(!ok||b.GetDataLen()<=0||b.GetDataLen()>24*1024*1024)return false;
    const auto*data=reinterpret_cast<const unsigned char*>(b.GetDataPtr());bytes.assign(data,data+b.GetDataLen());return true;
}
bool TeamPlay::Restore(const std::vector<unsigned char>&bytes) {
    const std::string path=GetAppDataFolder()+"userdata/team-rollback-"+std::to_string(app->mPlayerInfo->mId)+".tmp";
    if(bytes.empty()||!RunCheckpointIO::AtomicWrite(path,bytes))return false;
    bool ok=LawnLoadGame(app->mBoard,path);app->EraseFile(path);return ok;
}
// Only an established, current battle may replace the recoverable lawn. Never
// create a controller or consume its pending restore from a save callback.
bool TeamPlay::PrepareCheckpoint(LawnApp* app,std::vector<unsigned char>& previous,bool& captured) {
    captured=false;
    auto it=controllers.find(app);if(it==controllers.end())return true;
    auto&c=*it->second;auto*r=app->GetRunState();
    if(!c.BattleCheckpointReady())return true;
    std::vector<unsigned char> current;
    if(!c.Capture(current))return false;
    previous.swap(r->mTeam.lawn);r->mTeam.lawn.swap(current);captured=true;return true;
}
bool TeamPlay::BattleCheckpointReady()const {
    auto*r=app->GetRunState();
    return r&&observed==r&&r->mTeam.player&&r->mTeam.shared.run&&
        r->mTeam.localLevel==r->mLevelIndex&&r->mTeam.attempt==Attempt()&&
        app->mBoard&&app->mGameScene==SCENE_PLAYING&&!app->mRunIntroHeld&&
        !app->AdmissionBuilding()&&!recoverLawn;
}
bool TeamPlay::Save(bool lawn) {
    auto*r=app->GetRunState();if(!r||!app->mPlayerInfo)return false;
    if(lawn&&!BattleCheckpointReady()){notice=ModText::Tr("正在恢复草坪，支援暂缓","Restoring the lawn; support is on hold");return false;}
    // RunState commits the current lawn with the matching counters and receipts.
    if(!r->Save(app->mPlayerInfo->mId)){notice=ModText::Tr("保存失败，请检查可用空间","Save failed; check available storage");return false;}
    dirty=false;saveTicks=0;return true;
}
std::vector<Member> TeamPlay::Members()const {
    std::vector<Member> result;auto*r=app->GetRunState();auto*s=Session();if(!r)return result;
    for(const auto&pair:peers){const auto&m=pair.second;const auto seat=s?s->FindCoopSeat(m.player):0;
        if(m.level==uint32_t(r->mLevelIndex)&&m.attempt==Attempt()&&
            (m.player==r->mTeam.player||(seat&&!s->IsSeatOffline(seat)&&!s->IsWaitingForNextRound(seat))))result.push_back(m);}
    return result;
}
void TeamPlay::BeginContext() {
    auto*r=app->GetRunState();if(!r)return;auto&s=r->mTeam;auto*n=Session();
    if(observed!=r||!s.player){observed=r;peers.clear();peerAge.clear();wantsVote=false;baselineCaptured=false;reportedClear=false;quote=0;selected=0;shownBallotRevision=0;reportTicks=0;voteTicks=0;notice.clear();recoverLawn=!s.lawn.empty()&&s.localLevel==r->mLevelIndex&&s.attempt==Attempt();++viewRevision;}
    if(!s.player)s.player=app->IsOnlineGame()&&n?n->mCoopToken:Admission::NewId();
    if(app->IsOnlineGame()&&n&&n->mCoopToken!=s.player)n->SetPersistentPlayer(s.player);
    if(Host()&&!s.shared.run){s.shared.run=r->mAdmission.run?r->mAdmission.run:Admission::NewId();dirty=true;}
    if(s.localLevel!=r->mLevelIndex||s.attempt!=Attempt()) {
        const bool newLevel=s.localLevel!=r->mLevelIndex;
        s.localLevel=r->mLevelIndex;s.attempt=Attempt();s.lawn.clear();
        if((s.outgoing.phase==RESERVED||s.outgoing.phase==COMMITTED)&&
           (s.outgoing.level!=uint32_t(s.localLevel)||s.outgoing.attempt!=s.attempt))s.outgoing.phase=REJECTED;
        for(auto&receipt:s.receipts)if((receipt.strike.phase==ACCEPTED||receipt.strike.phase==COMMITTED)&&
           (receipt.strike.level!=uint32_t(s.localLevel)||receipt.strike.attempt!=s.attempt))receipt.strike.phase=REJECTED;
        if(newLevel){s.baselinePackets.clear();s.baselineSun=0;}
        wantsVote=false;baselineCaptured=false;recoverLawn=false;reportedClear=false;quote=0;++viewRevision;dirty=true;
        // Retry restores the start-of-battle sun/cards; issued relief remains a level-level event.
        if(s.reliefReceivedLevel==s.localLevel&&s.reliefReceivedAttempt!=s.attempt){s.reliefReceivedLevel=-1;s.reliefReceivedAttempt=0;}
    }
    if(Host()&&s.shared.authority!=s.player){s.shared.authority=s.player;++s.shared.term;++s.shared.revision;dirty=true;Broadcast();}
}
void TeamPlay::Report() {
    auto*r=app->GetRunState();if(!r)return;Member m;m.player=r->mTeam.player;m.plants=PlantMask(*r);m.level=r->mLevelIndex;m.attempt=Attempt();
    m.allow=r->mTeam.allow;m.ready=wantsVote;m.rewardsDone=app->mRunIntroHeld&&!r->HasPendingPick()&&!r->IsCatchingUp();m.rows=app->mBoard&&app->mBoard->StageHasPool()?6:5;m.title=uint8_t(TeamGoalsStorage::Title(app));
    m.battle=app->mBoardResult==BOARDRESULT_LOST?FAILED:app->mGameScene==SCENE_PLAYING&&!app->mRunIntroHeld?FIGHTING:PREPARING;
    NetProto::MsgEscapedZombie relay;const bool pendingRelay=Session()&&Session()->PeekPendingEscapedZombie(relay);
    if(m.battle==FIGHTING&&app->mBoard&&!pendingRelay&&!app->mBoard->AreEnemyZombiesOnScreen()&&(app->mBoard->mLevelComplete||app->mBoard->mLevelAwardSpawned)&&
       (!Session()||!app->IsOnlineGame()||Session()->IsLocalLevelDone()))m.battle=CLEARED;
    peers[m.player]=m;peerAge[m.player]=0;auto e=Message(TeamWire::REPORT);e.member=m;Send(e);
    if(Host()&&r->mTeam.shared.stacks[ARSENAL]) {
        const uint64_t before=r->mTeam.shared.plants;
        for(const auto&entry:Members())r->mTeam.shared.plants|=entry.plants;
        if(before!=r->mTeam.shared.plants){++r->mTeam.shared.revision;dirty=true;Broadcast();}
    }
}
void TeamPlay::Broadcast(){auto*r=app->GetRunState();if(!r||!Host())return;auto m=Message(TeamWire::SHARED);m.shared=r->mTeam.shared;
    for(int i=0;i<RunState::RUN_ZOMBIE_BUFF_COUNT;++i)m.zombieStacks[i]=r->mZombieBuffStacks[i];
    m.zombieVoted=r->mZombieVotedLevel;m.zombieRounds=uint8_t(app->mZombieVoteRoundsDone);Send(m);}
bool TeamPlay::HoldPersonalRewards() {
    auto*r=app->GetRunState();if(!r)return false;BeginContext();Report();
    auto*n=Session();if(n&&n->IsActive()&&!n->IsConnected())return true;
    if(!app->IsOnlineGame())return false;if(!n||!n->IsConnected())return true;
    for(uint8_t seat=1;seat<=6;++seat)if(n->IsSeatOccupied(seat)&&!n->IsSeatOffline(seat)&&!n->IsWaitingForNextRound(seat)){
        const auto id=seat==n->GetLocalSeat()?r->mTeam.player:n->mCoopPeers[seat];auto it=peers.find(id);
        if(!id||it==peers.end()||!it->second.rewardsDone||it->second.level!=uint32_t(r->mLevelIndex)||it->second.attempt!=Attempt())return true;
    }
    return false;
}
bool TeamPlay::HoldPreparation() {
    auto*r=app->GetRunState();if(!r)return false;BeginContext();
    if(auto*n=Session();n&&n->IsActive()&&!n->IsConnected())return true;
    auto&s=r->mTeam.shared;
    const int previous=r->mLevelIndex?r->LevelForModeIndex(r->mMode,r->mLevelIndex-1):r->GetLevel();
    const bool due=s.Due(r->mLevelIndex,r->GetSceneIndex(),(previous-1)/10,r->IsEndless());
    if(!s.run||(!Host()&&!s.ballot.open&&s.votedLevel!=r->mLevelIndex&&due)){wantsVote=true;Report();page=Page::VOTE;return true;}
    if(!due&&!s.ballot.open)return false;
    wantsVote=true;Report();
    if(Host()&&!s.ballot.open&&s.votedLevel!=r->mLevelIndex) {
        auto members=Members();bool all=!members.empty();auto*n=Session();
        if(app->IsOnlineGame()&&n)for(uint8_t seat=1;seat<=6;++seat)if(n->IsSeatOccupied(seat)&&!n->IsSeatOffline(seat)&&!n->IsWaitingForNextRound(seat)){
            auto id=seat==n->GetLocalSeat()?r->mTeam.player:n->mCoopPeers[seat];auto it=peers.find(id);if(!id||it==peers.end()||!it->second.ready||it->second.level!=uint32_t(r->mLevelIndex)||it->second.attempt!=Attempt())all=false;}
        for(const auto&m:members)if(!m.ready)all=false;
        if(all){s.Begin(r->mLevelIndex,members);voteTicks=0;dirty=true;if(!Save()){s.ballot.open=false;return true;}Broadcast();}
    }
    if(s.votedLevel==r->mLevelIndex&&!s.ballot.open){wantsVote=false;return false;}
    page=Page::VOTE;
    if(s.ballot.open&&shownBallotRevision!=s.ballot.revision){
        TeamPlayUi::Show(app,Page::VOTE);
        if(app->mTeamProgressionDialog)shownBallotRevision=s.ballot.revision;
    }
    return true;
}
void TeamPlay::PrepareBattle() {
    auto*r=app->GetRunState();if(!r||!app->mBoard)return;BeginContext();
    // Weights selected after personal rewards must affect this stage, not the next one.
    const auto rng=Sexy::GetRandState();Sexy::SRand(uint32_t(r->GetLevelSeed()));
    app->mBoard->InitZombieWaves();Sexy::RestoreRandState(rng);
}
void TeamPlay::GrantRelief() {
    auto*r=app->GetRunState();auto*b=app->mBoard;if(!r||!b)return;auto&s=r->mTeam;const auto&g=s.shared;
    if(g.reliefLevel!=r->mLevelIndex||g.reliefAttempt!=Attempt()||!g.stacks[RELIEF]||
       (s.reliefReceivedLevel==r->mLevelIndex&&s.reliefReceivedAttempt==Attempt()))return;
    if(std::find(g.reliefPlayers.begin(),g.reliefPlayers.end(),s.player)==g.reliefPlayers.end())return;
    if(!BattleCheckpointReady()||app->mBoardResult==BOARDRESULT_LOST||b->mLevelAwardSpawned||(Session()&&app->IsOnlineGame()&&Session()->IsLocalLevelDone()))return;
    const auto before=s;std::vector<unsigned char> lawn;if(!Capture(lawn))return;
    b->mSunMoney=AddSun(b->mSunMoney,1000*int(g.stacks[RELIEF]));
    for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto&p=b->mSeedBank->mSeedPackets[i];if(p.mPacketType!=SEED_NONE){p.mRefreshCounter=0;p.mRefreshing=false;p.Activate();}}
    s.reliefReceivedLevel=r->mLevelIndex;s.reliefReceivedAttempt=Attempt();
    if(!Save(true)){s=before;if(!Restore(lawn))b->mPaused=true;return;}
    b->PushCustomBanner(ModText::Tr("胜利驰援：阳光到账，种子卡已刷新","Victory relief: sun received; seed packets refreshed"));
}
int TeamPlay::CherryCost()const{return app->mBoard?app->mBoard->GetCurrentPlantCost(SEED_CHERRYBOMB,SEED_NONE):INT_MAX;}
bool TeamPlay::CanSupport(uint64_t target,int row,int col,std::string&why)const {
    auto*r=app->GetRunState();auto*n=Session();
    if(!r||!app->mBoard||!n||!app->IsOnlineGame()||!n->IsConnected()||n->IsReconnecting()){why=ModText::Tr("需要在线队友","An online teammate is required");return false;}
    const auto&s=r->mTeam;auto found=peers.find(target);uint8_t seat=n->FindCoopSeat(target);
    if(!s.shared.stacks[FIRE]||(!r->HasPlant(SEED_CHERRYBOMB)&&!s.shared.Has(SEED_CHERRYBOMB))){why=ModText::Tr("需要跨场火力与樱桃","Cross-lawn firepower and Cherry Bomb are required");return false;}
    if(app->mGameScene!=SCENE_PLAYING||app->mRunIntroHeld||app->mBoardResult==BOARDRESULT_LOST||!seat||n->IsSeatOffline(seat)||n->IsWaitingForNextRound(seat)||target==s.player||found==peers.end()||found->second.level!=uint32_t(r->mLevelIndex)||found->second.attempt!=Attempt()||found->second.battle!=FIGHTING||!found->second.allow){why=ModText::Tr("队友须在本关战斗并允许支援","The teammate must be fighting this stage and allow support");return false;}
    if(s.cooldown>0||(s.outgoing.phase>=RESERVED&&s.outgoing.phase<=COMMITTED)){why=ModText::Tr("请等冷却结束或上次支援确认", "Wait for cooldown or the previous support confirmation");return false;}
    if(row<0||row>=found->second.rows||col<0||col>=9){why=ModText::Tr("请先选择队友草坪上的有效格位","Select a valid cell on your teammate's lawn");return false;}
    if(app->mBoard->mSunMoney<CherryCost()){why=ModText::Tr("阳光不足","Not enough sun");return false;}
    why=ModText::Tr("确认后扣除一次樱桃费用","Confirmation spends one Cherry Bomb cost");return true;
}
void TeamPlay::StartSupport(uint64_t target,int row,int col) {
    auto*r=app->GetRunState();std::string why;if(!CanSupport(target,row,col,why)){notice=why;return;}
    auto&s=r->mTeam;const auto before=s;auto*b=app->mBoard;const int sun=b->mSunMoney;
    Strike strike;strike.id=++s.serial;strike.caster=s.player;strike.target=target;strike.level=r->mLevelIndex;strike.attempt=Attempt();
    strike.bursts=s.shared.stacks[FIRE];strike.phase=RESERVED;strike.row=uint8_t(row);strike.column=uint8_t(col);strike.cost=CherryCost();
    strike.radius=int(std::clamp<double>(115.0*app->RunPlantUpgradeMul(SEED_CHERRYBOMB)+0.5,1,1000000));
    strike.damage=int(std::clamp<double>(1800.0*app->RunBuffMul(RUN_BUFF_BLAST)*app->RunBuffMul(RUN_BUFF_PRECISION)+0.5,1,INT_MAX));strike.scorch=std::clamp(app->RunPlantBuff2Count(SEED_CHERRYBOMB),0,65535);
    strike.scorchMultiplier=int(std::clamp<double>(1000.0*app->RunBuffMul(RUN_BUFF_PRECISION)+0.5,1,INT_MAX));
    s.outgoing=strike;b->mSunMoney-=strike.cost;
    if(!Save(true)){s=before;b->mSunMoney=sun;return;}
    Reply(strike,TeamWire::OFFER);quote=0;++viewRevision;notice=ModText::Tr("已扣阳光，等待队友确认支援", "Sun paid; waiting for the teammate to confirm support");
}
void TeamPlay::Reply(const Strike&s,uint8_t op){auto m=Message(op);m.strike=s;Send(m,s.caster==m.sender?s.target:s.caster);}
void TeamPlay::Execute(Receipt&receipt) {
    auto*r=app->GetRunState();auto*b=app->mBoard;auto&s=receipt.strike;
    if(!r||!b||s.phase!=COMMITTED)return;
    if(s.level!=uint32_t(r->mLevelIndex)||s.attempt!=Attempt()||!r->mTeam.allow||app->mBoardResult==BOARDRESULT_LOST||app->mGameScene!=SCENE_PLAYING){const auto before=s;s.phase=REJECTED;if(Save())Reply(s,TeamWire::RESULT);else s=before;return;}
    const auto previous=r->mTeam;std::vector<unsigned char> lawn;if(!Capture(lawn))return;
    s.phase=EXECUTED; // Mark before damage can cause chained deaths or reentrant callbacks.
    const int x=b->GridToPixelX(s.column,s.row)+40,y=b->GridToPixelY(s.column,s.row)+40;
    {
        TeamAttackContext::ExactBlast exact;
        DamageStats::RemoteSource source(app->mDamageStats,s.caster,s.id);
        for(unsigned burst=0;burst<s.bursts;++burst){
            b->KillAllZombiesInRadius(s.row,x,y,s.radius,1,true,127,s.damage,s.radius>115);
            if(s.scorch){Zombie*z=nullptr;while(b->IterateZombies(z))if(RunDotEffects::Alive(z)&&z->EffectedByDamage(127)&&
                (s.radius>115||z->mZombieType==ZOMBIE_BOSS||std::abs(z->mRow-int(s.row))<=1)&&GetCircleRectOverlap(x,y,s.radius,z->GetZombieRect()))RunDotEffects::RemoteScorch(z,s.caster,s.id,s.scorch,s.scorchMultiplier);}
        }
    }
    app->PlayFoley(FOLEY_CHERRYBOMB);
#ifdef __ANDROID__
    app->AddPvzpParticle(x,y,RENDER_LAYER_TOP,PARTICLE_POWIE);
#else
    app->AddTodParticle(x,y,RENDER_LAYER_TOP,PARTICLE_POWIE);
#endif
    b->ShakeBoard(3,-4);
    if(!Save(true)){r->mTeam=previous;if(!Restore(lawn))b->mPaused=true;return;}
    Reply(s,TeamWire::RESULT);
}
void TeamPlay::Handle(const TeamWire::Message&m) {
    auto*r=app->GetRunState();if(!r||m.seed!=uint32_t(r->mRunSeed)||m.mode!=r->mMode)return;auto&s=r->mTeam;auto*n=Session();
    if(m.op==TeamWire::SHARED) {
        if(!n||m.src!=n->GetHostSeat()||m.shared.authority!=m.sender||!m.run)return;
        if(s.shared.run&&s.shared.run!=m.run)return;
        if(m.shared.term<s.shared.term||(m.shared.term==s.shared.term&&m.shared.revision<s.shared.revision))return;
        if(s.shared.ballot.open!=m.shared.ballot.open||s.shared.ballot.revision!=m.shared.ballot.revision)++viewRevision;
        if(s.shared.ballot.open&&!m.shared.ballot.open&&page==Page::VOTE){TeamPlayUi::Close(app);page=Page::TEAM_EFFECTS;}
        s.shared=m.shared;
        const int incomingVote=m.zombieVoted==65535?-1:m.zombieVoted,localVote=r->mZombieVotedLevel==65535?-1:r->mZombieVotedLevel;
        if(m.level==uint32_t(r->mLevelIndex)&&m.attempt==Attempt()&&incomingVote>=localVote&&
           (incomingVote!=localVote||m.zombieRounds>=app->mZombieVoteRoundsDone)){
            for(int i=0;i<RunState::RUN_ZOMBIE_BUFF_COUNT;++i)r->mZombieBuffStacks[i]=RunSpeciesRules::ClampedStacks(i,m.zombieStacks[i]);
            r->mZombieVotedLevel=m.zombieVoted;app->mZombieVoteRoundsDone=m.zombieRounds;r->mEndlessVoteRounds=m.zombieRounds;
        }
        dirty=true;GrantRelief();return;
    }
    if(m.op==TeamWire::REPORT){
        if(m.run&&s.shared.run&&m.run!=s.shared.run)return;
        peers[m.sender]=m.member;peerAge[m.sender]=0;
        if(Host()){
            if(s.shared.stacks[ARSENAL]&&(m.member.plants&~s.shared.plants)){s.shared.plants|=m.member.plants;++s.shared.revision;dirty=true;}
            if(!m.run||m.revision!=s.shared.revision)Broadcast();
        }
        return;
    }
    if(m.run!=s.shared.run||!m.run)return;
    if(m.op==TeamWire::VOTE){if(Host()&&m.level==uint32_t(r->mLevelIndex)&&m.attempt==Attempt()&&m.revision==s.shared.ballot.revision&&s.shared.ballot.Vote(m.sender,m.choice)){dirty=true;Broadcast();}return;}
    const auto&incoming=m.strike;
    if(incoming.target==s.player&&incoming.caster==m.sender){
        const auto before=s;
        auto*receipt=s.FindReceipt(m.sender);if(!receipt)return;
        if(incoming.id<receipt->sequence)return;
        if(incoming.id==receipt->sequence){
            if(m.op==TeamWire::COMMIT&&receipt->strike.phase==ACCEPTED){receipt->strike.phase=COMMITTED;if(Save())Execute(*receipt);else s=before;}
            else Reply(receipt->strike,receipt->strike.phase==ACCEPTED?TeamWire::ACCEPT:TeamWire::RESULT);
            return;
        }
        if(m.op!=TeamWire::OFFER||incoming.phase!=RESERVED)return;
        if(receipt->strike.phase==ACCEPTED||receipt->strike.phase==COMMITTED)return;
        receipt->sequence=incoming.id;receipt->strike=incoming;auto&t=receipt->strike;
        const auto peer=peers.find(m.sender);
        const bool valid=app->mBoard&&app->mGameScene==SCENE_PLAYING&&!app->mRunIntroHeld&&app->mBoardResult!=BOARDRESULT_LOST&&s.allow&&
            t.level==uint32_t(r->mLevelIndex)&&t.attempt==Attempt()&&m.level==t.level&&m.attempt==t.attempt&&
            t.bursts==s.shared.stacks[FIRE]&&s.shared.stacks[FIRE]>0&&peer!=peers.end()&&
            ((peer->second.plants&(uint64_t(1)<<SEED_CHERRYBOMB))||s.shared.Has(SEED_CHERRYBOMB))&&
            t.row<((app->mBoard->StageHasPool())?6:5);
        t.phase=valid?ACCEPTED:REJECTED;
        if(Save())Reply(t,valid?TeamWire::ACCEPT:TeamWire::RESULT);else s=before;return;
    }
    auto&out=s.outgoing;
    if(incoming.caster!=s.player||incoming.target!=m.sender||incoming.id!=out.id||out.caster!=s.player||incoming.level!=out.level||incoming.attempt!=out.attempt)return;
    if(m.op==TeamWire::ACCEPT&&(out.phase==RESERVED||out.phase==COMMITTED)){
        const auto previous=s;if(out.phase==RESERVED)s.cooldown=2000;
        out.phase=COMMITTED;if(Save())Reply(out,TeamWire::COMMIT);else s=previous;return;
    }
    if(m.op==TeamWire::RESULT&&incoming.phase==REJECTED&&(out.phase==RESERVED||out.phase==COMMITTED)) {
        const auto previous=s;int sun=app->mBoard?app->mBoard->mSunMoney:0;
        out.phase=REJECTED;
        if(app->mBoard&&out.level==uint32_t(r->mLevelIndex)&&out.attempt==Attempt())app->mBoard->mSunMoney=AddSun(sun,out.cost);
        if(!Save(true)){s=previous;if(app->mBoard)app->mBoard->mSunMoney=sun;return;}
        notice=ModText::Tr("支援未施放，阳光已退回", "Support was not used; sun refunded");return;
    }
    if(m.op==TeamWire::RESULT&&incoming.phase==EXECUTED&&(out.phase==RESERVED||out.phase==COMMITTED||out.phase==EXECUTED)) {
        if(incoming.allDamage<out.allDamage||incoming.bossDamage<out.bossDamage)return;
        const auto before=s;const auto previousStats=app->mDamageStats.SaveSnapshot();
        const uint64_t all=incoming.allDamage-out.allDamage,boss=incoming.bossDamage-out.bossDamage;
        if(boss>all)return;
        if(out.phase!=EXECUTED)s.supportCount=TeamGoals::Add(s.supportCount,1);
        out.phase=EXECUTED;out.allDamage=incoming.allDamage;out.bossDamage=incoming.bossDamage;
        app->mDamageStats.AddSupportDamage(all,boss);dirty=true;
        if(!Save(true)){s=before;if(app->mBoard&&!previousStats.empty())app->mDamageStats.LoadSnapshot(app->mBoard,previousStats.data(),previousStats.size());return;}
        notice=ModText::Tr("跨场火力已施放","Cross-lawn support executed");
    }
}
void TeamPlay::NoteDamage(LawnApp*app,uint64_t caster,uint64_t strike,int amount,bool boss) {
    auto*r=app->GetRunState();if(!r||amount<=0)return;
    for(auto&receipt:r->mTeam.receipts)if(receipt.player==caster&&receipt.strike.id==strike&&receipt.strike.phase==EXECUTED){
        auto&s=receipt.strike;const uint64_t add=std::min<uint64_t>(uint64_t(amount),DamageStatsWire::LIMIT-s.allDamage);
        s.allDamage+=add;if(boss)s.bossDamage+=add;TeamPlay::For(app).dirty=true;return;
    }
}
bool TeamPlay::SyncGoals(){
    auto*r=app->GetRunState();if(!r||!app->mPlayerInfo)return false;
    const auto path=GetAppDataFolder()+"userdata/team-goals-"+std::to_string(app->mPlayerInfo->mId)+".dat";
    if(goalsProfile!=app->mPlayerInfo->mId){
        goalsProfile=app->mPlayerInfo->mId;goals={};goalsPending=false;goalsValid=true;
        std::ifstream in(path,std::ios::binary);if(in){std::vector<unsigned char>bytes((std::istreambuf_iterator<char>(in)),{});goalsValid=TeamGoals::Decode(bytes,goals);}
        if(!goalsValid){notice=ModText::Tr("成长记录无法读取，已保留原文件","Growth records could not be read; the original file is retained");return false;}
    }
    if(!goalsValid)return false;
    goalsPending=goals.Merge(r->mTeam)||goalsPending;
    if(goalsPending){if(RunCheckpointIO::AtomicWrite(path,TeamGoals::Encode(goals)))goalsPending=false;else notice=ModText::Tr("成长记录尚未保存，请检查空间","Growth records are not saved yet; check storage");}
    return !goalsPending;
}
bool TeamPlay::NoteGift(LawnApp*app,int actualSun){
    auto*r=app->GetRunState();if(!r||actualSun<=0)return false;auto&c=For(app);c.BeginContext();const auto before=r->mTeam.giftSun;
    r->mTeam.giftSun=TeamGoals::Add(before,uint64_t(actualSun));if(!c.Save(true)){r->mTeam.giftSun=before;return false;}c.SyncGoals();return true;
}
bool TeamPlay::SaveCoopTransaction(LawnApp*app,bool lawn){auto&c=For(app);c.BeginContext();if(!c.Save(lawn))return false;c.SyncGoals();return true;}
bool TeamPlay::NoteVictory(LawnApp*app){
    auto*r=app->GetRunState();if(!r)return true;auto&c=For(app);c.BeginContext();const auto before=r->mTeam;
    for(int i=10;i<17;++i)if(r->mZombieBuffStacks[i])r->mTeam.speciesWon|=uint64_t(1)<<(i-10);
    if(r->IsEndless())r->mTeam.endlessCleared=std::max<uint32_t>(r->mTeam.endlessCleared,r->mLevelIndex+1);
    if(!c.Save()){r->mTeam=before;return false;}return c.SyncGoals();
}
void TeamPlay::NoteSharedPlant(LawnApp*app,int plant){
    auto*r=app->GetRunState();if(!r||plant<0||plant>SEED_IMITATER||r->HasPlant(SeedType(plant))||!r->mTeam.shared.Has(plant))return;
    auto&c=For(app);const auto bit=uint64_t(1)<<plant;if(r->mTeam.sharedUsed&bit)return;r->mTeam.sharedUsed|=bit;c.dirty=true;
}
void TeamPlay::Update() {
    auto*r=app->GetRunState();if(!r){observed=nullptr;TeamPlayUi::Install(app,nullptr);return;}
    BeginContext();TeamPlayUi::Install(app,this);auto&s=r->mTeam;auto*n=Session();
    if(s.cooldown>0&&app->mGameScene==SCENE_PLAYING&&app->mBoard&&!app->mBoard->mPaused)--s.cooldown;
    if(app->mGameScene==SCENE_PLAYING&&app->mBoard&&!app->mRunIntroHeld&&!baselineCaptured){
        if(recoverLawn){
            if(!Restore(s.lawn)){app->mBoard->mPaused=true;notice=ModText::Tr("草坪恢复失败，已保留存档","Lawn recovery failed; the save has been retained");return;}
            recoverLawn=false;baselineCaptured=true;
        }
        if(!baselineCaptured){
        baselineCaptured=true;
        if(s.baselinePackets.empty()||s.localLevel!=r->mLevelIndex){
            s.baselineSun=app->mBoard->mSunMoney;s.baselineCooldown=s.cooldown;s.baselinePackets.clear();
            for(int i=0;i<app->mBoard->mSeedBank->mNumPackets;++i){const auto&p=app->mBoard->mSeedBank->mSeedPackets[i];
                Admission::Put(s.baselinePackets,p.mPacketType);Admission::Put(s.baselinePackets,p.mRefreshCounter);Admission::Put(s.baselinePackets,p.mRefreshTime);Admission::Put(s.baselinePackets,p.mActive,1);Admission::Put(s.baselinePackets,p.mRefreshing,1);}
            dirty=true;
        } else if(s.baselinePackets.size()%14==0) {
            app->mBoard->mSunMoney=s.baselineSun;s.cooldown=s.baselineCooldown;
            for(int i=0;i<app->mBoard->mSeedBank->mNumPackets;++i){auto&packet=app->mBoard->mSeedBank->mSeedPackets[i];
                for(size_t offset=0;offset<s.baselinePackets.size();offset+=14){const auto*p=s.baselinePackets.data()+offset;
                    const int plant=int32_t(Admission::Get(p));int counter=int32_t(Admission::Get(p)),time=int32_t(Admission::Get(p));const bool active=Admission::Get(p,1)!=0,refreshing=Admission::Get(p,1)!=0;
                    if(plant==packet.mPacketType){packet.mRefreshCounter=counter;packet.mRefreshTime=time;packet.mActive=active;packet.mRefreshing=refreshing;break;}
                }
            }
            dirty=true;
        }
        }
    }
    TeamWire::Message m;if(n)while(n->TakeTeam(m))Handle(m);
    if(--reportTicks<=0){reportTicks=100;Report();if(Host())Broadcast();
        if(s.outgoing.phase==RESERVED)Reply(s.outgoing,TeamWire::OFFER);
        if(s.outgoing.phase==COMMITTED)Reply(s.outgoing,TeamWire::COMMIT);
        for(const auto&receipt:s.receipts)if(receipt.strike.phase==EXECUTED)Reply(receipt.strike,TeamWire::RESULT);
    }
    if(Host()&&s.shared.ballot.open){
        auto&b=s.shared.ballot;
        for(size_t i=0;i<6;++i)if(b.players[i]&&n&&app->IsOnlineGame()&&!n->FindCoopSeat(b.players[i])){b.players[i]=0;b.votes[i]=-1;}
        if(b.Complete()||++voteTicks>=3000){const auto prior=s.shared;s.shared.Resolve(uint32_t(Admission::NewId()));dirty=true;
            if(Save()){Broadcast();++viewRevision;if(page==Page::VOTE)TeamPlayUi::Close(app);page=Page::TEAM_EFFECTS;Report();}
            else s.shared=prior;
        }
    }
    if(Host()&&s.shared.stacks[RELIEF]&&s.shared.reliefLevel!=r->mLevelIndex){
        const auto members=Members();bool clear=false;for(const auto&p:members)if(p.battle==CLEARED)clear=true;
        if(clear){const auto before=s.shared;s.shared.reliefLevel=r->mLevelIndex;s.shared.reliefAttempt=Attempt();s.shared.reliefPlayers.fill(0);size_t at=0;
            for(const auto&p:members)if(p.battle==FIGHTING&&at<6&&(!n||!app->IsOnlineGame()||!n->IsSeatLevelDone(n->FindCoopSeat(p.player))))s.shared.reliefPlayers[at++]=p.player;
            ++s.shared.revision;if(Save()){Broadcast();GrantRelief();}
            else s.shared=before;
        }
    }
    GrantRelief();
    for(auto&receipt:s.receipts)if(receipt.strike.phase==COMMITTED)Execute(receipt);
    if(dirty&&++saveTicks>=100)Save();
    if(!dirty&&reportTicks==100)SyncGoals();
}
Snapshot TeamPlay::Read()const {
    Snapshot out;auto*r=app->GetRunState();if(!r)return out;const auto&s=r->mTeam;auto*n=Session();
    out.context={s.shared.run,s.player,uint32_t(r->mLevelIndex),Attempt(),s.shared.term,viewRevision};out.page=page;
    out.phase=s.shared.ballot.open?Phase::VOTING:app->mBoardResult==BOARDRESULT_LOST?Phase::FAILED:app->mGameScene==SCENE_PLAYING?Phase::PLAYING:Phase::PREPARING;
    if(!s.shared.run||(n&&n->IsReconnecting()))out.phase=Phase::RECOVERING;
    out.canAct=s.shared.run&&(!app->IsOnlineGame()||(n&&n->IsConnected()));out.status=notice;
    if(out.status.empty())out.status=s.shared.ballot.open&&page==Page::VOTE?ModText::Tr("选择一项全队增益，30秒后结算；弃权不计票。", "Choose one team buff. Voting ends after 30 seconds; abstentions do not count."):ModText::Tr("点标签查看增益、与队友协作或查看目标。", "Choose a tab to view buffs, help teammates or check goals.");
    if(s.shared.ballot.open)for(auto id:s.shared.ballot.choices)if(id<BUFFS){Choice c;c.id=id;c.name=Names(id);c.description=Description(id);c.stacks=s.shared.stacks[id];c.enabled=true;
        for(size_t i=0;i<6;++i)if(s.shared.ballot.players[i]==s.player&&s.shared.ballot.votes[i]!=-2)c.enabled=false;out.choices.push_back(c);}
    for(int id=0;id<BUFFS;++id)if(s.shared.stacks[id]){Effect e;e.id=id;e.stacks=s.shared.stacks[id];e.name=Names(id);e.description=Description(id);e.source=ModText::Tr("全队共享","Shared by the team");out.teamEffects.push_back(e);}
    for(const auto&b:r->mBuffs){Effect e;e.id=b.mId;e.stacks=b.mCount;e.name=GetRunChoiceDisplayName(b.mId);e.description=GetRunChoiceDesc(b.mId);e.source=ModText::Tr("本人获得","Your personal buff");out.personalEffects.push_back(e);}
    for(uint8_t seat=1;seat<=6;++seat){const bool local=!app->IsOnlineGame()?seat==1:n&&seat==n->GetLocalSeat();if(!local&&(!n||!n->IsSeatOccupied(seat)))continue;
        Peer p;p.seat=seat;p.player=local?s.player:n->mCoopPeers[seat];p.name=local&&!n?ModText::Tr("我","Me"):n->GetSeatName(seat);p.online=local||!n->IsSeatOffline(seat);
        auto found=peers.find(p.player);p.eligible=found!=peers.end()&&found->second.level==uint32_t(r->mLevelIndex)&&found->second.attempt==Attempt();
        if(found!=peers.end()){if(found->second.title)p.name+=" · "+std::string(TeamGoalsStorage::Name(found->second.title-1));p.allowSupport=found->second.allow;p.phase=found->second.battle==FIGHTING?ModText::Tr("战斗中","Fighting"):found->second.battle==CLEARED?ModText::Tr("已通关","Cleared"):found->second.battle==FAILED?ModText::Tr("本关失败","Defeated"):ModText::Tr("准备中","Preparing");}
        p.voteStatus=ModText::Tr("未参与本轮","Not in this vote");for(size_t i=0;i<6;++i)if(s.shared.ballot.players[i]==p.player)p.voteStatus=s.shared.ballot.votes[i]==-2?ModText::Tr("未投票","Not voted"):s.shared.ballot.votes[i]==-1?ModText::Tr("已弃权","Abstained"):ModText::Tr("已投票","Voted");out.peers.push_back(p);}
    out.support.target=selected;out.support.quoteId=quote;out.support.row=quoteRow;out.support.column=quoteColumn;out.support.cost=CherryCost();out.support.cooldownSeconds=(s.cooldown+99)/100;
    out.support.pending=s.outgoing.phase>=RESERVED&&s.outgoing.phase<=COMMITTED;
    out.support.enabled=CanSupport(selected,quoteRow,quoteColumn,out.support.reason);
    out.support.canConfirm=out.support.enabled&&quote&&quoteCost==CherryCost();
    out.briefTitle=ModText::Tr("本局合作记录","Teamwork this run");out.briefSummary=ModText::Tr("跨场支援完成：","Cross-lawn supports completed: ")+std::to_string(s.supportCount);
    if(const auto*stats=app->mDamageStats.Local())out.damageSummary=ModText::Tr("本人有效伤害：","Your effective damage: ")+std::to_string(stats->mRunDamage.mAll);
    auto total=goals.Total();
    auto goal=[&](const char*zh,const char*en,uint64_t value,uint64_t target,const char*rewardZh,const char*rewardEn){Goal g;g.name=ModText::Tr(zh,en);g.progress=std::to_string(std::min(value,target))+"/"+std::to_string(target);g.progressPermille=int(std::min<uint64_t>(1000,value>=target?1000:value*1000/target));g.earned=value>=target;g.description=ModText::Tr(rewardZh,rewardEn);out.goals.push_back(g);};
    goal("雪中送炭","A helping hand",total.gifts,10000,"队友实际收到的赠送阳光；解锁同名称号。","Gift 10,000 sun actually received by teammates. Unlocks the matching title.");
    goal("跨场炮手","Cross-lawn gunner",total.fires,10,"成功施放跨场火力；解锁同名称号。","Successfully execute cross-lawn support. Unlocks the matching title.");
    goal("百草同心","Shared roots",TeamGoals::Bits(total.shared),6,"种过6种原本不拥有的共享植物；解锁同名称号。","Plant 6 different shared plants you did not personally own. Unlocks the matching title.");
    goal("强敌克星","Species challenger",TeamGoals::Bits(total.species),7,"分别在7类新强化生效时赢过一关；解锁同名称号。","Win a stage with each of the 7 new enemy enhancements active. Unlocks the matching title.");
    goal("无尽新星","Endless rising star",total.endless,10,"无尽通过10关；解锁同名称号。","Clear 10 endless stages. Unlocks the matching title.");
    goal("无尽先锋","Endless vanguard",total.endless,25,"无尽通过25关；解锁同名称号。","Clear 25 endless stages. Unlocks the matching title.");
    goal("无尽传奇","Endless legend",total.endless,50,"无尽通过50关；解锁同名称号。","Clear 50 endless stages. Unlocks the matching title.");
    return out;
}
void TeamPlay::Submit(const Intent&i) {
    auto*r=app->GetRunState();if(!r||!Same(i.context,Read().context))return;auto&s=r->mTeam;
    if(i.action==Action::CLOSE)return;
    if(i.action==Action::SELECT_PAGE&&i.value>=0&&i.value<=int(Page::GOALS)){page=Page(i.value);quote=0;++viewRevision;return;}
    if(i.action==Action::SELECT_PEER){selected=i.target;quote=0;quoteRow=quoteColumn=-1;++viewRevision;return;}
    if(i.action==Action::VOTE){auto m=Message(TeamWire::VOTE);m.choice=i.value;m.revision=s.shared.ballot.revision;
        if(Host()){if(s.shared.ballot.Vote(s.player,i.value)){dirty=true;Broadcast();}}
        else if(auto*n=Session())Send(m,n->mCoopPeers[n->GetHostSeat()]);return;}
    if(i.action==Action::SUPPORT_PERMISSION){const bool before=s.allow;s.allow=i.value!=0;if(!Save())s.allow=before;else{Report();++viewRevision;}return;}
    if(i.action==Action::SELECT_CELL){quote=0;quoteRow=i.row;quoteColumn=i.column;++viewRevision;return;}
    if(i.action==Action::SUPPORT_PREVIEW){std::string why;quoteRow=i.row;quoteColumn=i.column;selected=i.target;
        if(CanSupport(selected,quoteRow,quoteColumn,why)){quote=Admission::NewId();quoteCost=CherryCost();}else{quote=0;notice=why;}++viewRevision;return;}
    if(i.action==Action::SUPPORT_CONFIRM){if(i.quoteId&&i.quoteId==quote&&i.target==selected&&i.row==quoteRow&&i.column==quoteColumn&&quoteCost==CherryCost())StartSupport(selected,quoteRow,quoteColumn);return;}
    if(i.action==Action::SUN_REQUEST&&app->mCoopPlay)app->mCoopPlay->RequestSun();
    if(i.action==Action::SUN_GIFT&&app->mCoopPlay)app->mCoopPlay->SendGift(i.target,0,true);
    if(i.action==Action::LOAN&&app->mCoopPlay)app->mCoopPlay->Show();
    if(i.action==Action::WATCH&&Session()&&app->mBoard){auto seat=Session()->FindCoopSeat(i.target);if(seat&&Session()->BeginWatch(seat)){
        TeamPlayUi::Close(app);
#ifdef __ANDROID__
        app->mBoard->mMobileWatchOpen=true;
#else
        app->mBoard->mWatchKeyHeld=true;app->mBoard->mWatchLastSeat=seat;app->mBoard->mCoopWatchLatched=true;
#endif
    }}
}
