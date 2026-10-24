#include "RunSpeciesRuntime.h"
#include "../../LawnApp.h"
#include "../Board.h"
#include "../Zombie.h"
#include "RunSpeciesEffects.h"
#include "../Challenge.h"
#include "../System/DamageStats.h"
#ifdef __ANDROID__
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/PvzpFoley.h"
#else
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodFoley.h"
#endif
#include "../Online/NetProtocol.h"
#include "../../Resources.h"
#include <unordered_map>
#include <map>
#include <deque>
#include <cstring>
#include <algorithm>

namespace RunSpeciesRuntime {
// Both original platform implementations use these exact constants.
constexpr float kThrownGravity=0.05f;
constexpr int kJackZombieRadius=115,kJackPlantRadius=90;
struct Blast { int row,x,y; bool mind; };
struct Runtime { std::map<unsigned,unsigned> states; std::deque<Blast> blasts; bool draining=false; };
static std::unordered_map<Board*,Runtime> tables;
static unsigned Id(Zombie* z) { return z->mBoard->mZombies.DataArrayGetID(z); }
static bool Giant(Zombie* z) { return z && (z->mZombieType==ZOMBIE_GARGANTUAR || z->mZombieType==ZOMBIE_REDEYE_GARGANTUAR); }
void Reset(Board* b) { tables.erase(b); }
unsigned Flags(Zombie* z) {
    if(!z || !z->mBoard)return 0;
    auto t=tables.find(z->mBoard);if(t==tables.end())return 0;
    auto s=t->second.states.find(Id(z));return s==t->second.states.end()?0:s->second;
}
void SetFlags(Zombie* z,unsigned f) {
    if(!z || !z->mBoard)return;
    if(f)tables[z->mBoard].states[Id(z)]=f;else { auto t=tables.find(z->mBoard);if(t!=tables.end())t->second.states.erase(Id(z)); }
}
bool CanThrow(Zombie* z) { return z && !z->IsDeadOrDying() && !(Flags(z)&(THROWN_USED|NO_THROW)); }
bool GiantReplacement(Zombie* z) { return Giant(z) && RunSpeciesEffects::Count(z,RunSpeciesRules::GIANT_CHAIN)>0; }
void ChildThrown(Zombie* parent,Zombie* child) {
    SetFlags(parent,Flags(parent)|THROWN_USED);
    if(!Giant(child))return;
    const bool noThrow=RunSpeciesEffects::Count(parent,RunSpeciesRules::GIANT_CHAIN)<2;
    SetFlags(child,FLYING_GIANT|(noThrow?NO_THROW:0));
    child->mHasObject=!noThrow;
    if(noThrow) { child->ReanimShowPrefix("Zombie_imp",RENDER_GROUP_HIDDEN);child->ReanimShowTrack("Zombie_gargantuar_whiterope",RENDER_GROUP_HIDDEN); }
    if(parent->mMindControlled && !child->mMindControlled) {
        // Allegiance is inherited, rather than a new incoming control attack.
        const bool immune=child->mControlImmune;child->mControlImmune=false;
        child->StartMindControlled();child->mControlImmune=immune;
    }
    if(parent->mBoard->IsRelayedZombie(parent))parent->mBoard->MarkRelayedZombie(child);
}
bool UpdateFlight(Zombie* z) {
    if(!(Flags(z)&FLYING_GIANT))return false;
    if(z->IsDeadOrDying())return false;
    z->mVelZ-=kThrownGravity;z->mAltitude+=z->mVelZ;z->mPosX-=z->mVelX;
    const float dy=z->GetPosYBasedOnRow(z->mRow)-z->mPosY;z->mPosY+=dy;z->mAltitude+=dy;
    if(z->mAltitude<=0) {
        z->mAltitude=0;SetFlags(z,Flags(z)&~FLYING_GIANT);
        z->mZombiePhase=PHASE_ZOMBIE_NORMAL;z->PickRandomSpeed();z->StartWalkAnim(0);
    }
    return true;
}
bool CommitBlast(Zombie* z,bool active) {
    if(!z || z->mDead || (!active && z->IsDeadOrDying()) || z->mZombieType!=ZOMBIE_JACK_IN_THE_BOX ||
        (!active && RunSpeciesEffects::Count(z,RunSpeciesRules::JACK_DEATH)<=0))return false;
    if(Flags(z)&BLAST_COMMITTED)return false;
    SetFlags(z,Flags(z)|BLAST_COMMITTED);
    tables[z->mBoard].blasts.push_back({z->mRow,z->mX+z->mWidth/2,z->mY+z->mHeight/2,z->mMindControlled});return true;
}
void CombatDeath(Zombie* z) { CommitBlast(z); }
unsigned Pending(Board* b) { auto t=tables.find(b);return t==tables.end()?0:static_cast<unsigned>(t->second.blasts.size())+(t->second.draining?1u:0u); }
unsigned Drain(Board* b) {
    auto it=tables.find(b);if(it==tables.end() || it->second.draining)return 0;
    it->second.draining=true;unsigned done=0;
    while(done<BLASTS_PER_FRAME && !it->second.blasts.empty()) {
        const Blast s=it->second.blasts.front();it->second.blasts.pop_front();++done;
        DamageStats::Source source(b->mApp->mDamageStats,s.mind);
        b->mApp->PlayFoley(FOLEY_EXPLOSION);
        b->KillAllZombiesInRadius(s.row,s.x,s.y,kJackZombieRadius,1,true,s.mind?127:255);
        if(!s.mind)b->KillAllPlantsInRadius(s.x,s.y,kJackPlantRadius);
#ifdef __ANDROID__
        b->mApp->AddPvzpParticle(s.x,s.y,Board::MakeRenderOrder(RENDER_LAYER_TOP,0,0),PARTICLE_JACKEXPLODE);
#else
        b->mApp->AddTodParticle(s.x,s.y,Board::MakeRenderOrder(RENDER_LAYER_TOP,0,0),PARTICLE_JACKEXPLODE);
#endif
        b->ShakeBoard(4,-6);
        if(b->mApp->IsScaryPotterLevel())b->mChallenge->ScaryPotterJackExplode(s.x,s.y);
        // Look up again: callbacks can insert other board entries and rehash tables.
        it=tables.find(b);if(it==tables.end())return done;
    }
    it->second.draining=false;
    for(auto s=it->second.states.begin();s!=it->second.states.end();)
        if(!b->mZombies.DataArrayTryToGet(s->first))s=it->second.states.erase(s);else ++s;
    // Death's normal TrySpawnLevelAward was held while a blast was pending.
    // Its source corpse may already have been freed on a prior frame. Settle the
    // same run-only completion branch after every final blast, without using it.
    if(done && it->second.blasts.empty() && b->mApp->IsRunMode() &&
        !b->HasLevelAwardDropped() && !b->mLevelComplete && !b->mApp->IsContinuousChallenge() &&
        b->mCurrentWave>=b->mNumWaves && !b->AreEnemyZombiesOnScreen()) {
        b->mLevelAwardSpawned=true;b->mApp->mBoardResult=BOARDRESULT_WON;
        if(!b->IsSurvivalStageWithRepick())b->RemoveAllZombies();
        b->FadeOutLevel();
    }
    return done;
}
void ExportRelay(Zombie* z,NetProto::MsgEscapedZombie& m) {
    m.mSpeciesFlags=static_cast<unsigned char>(Flags(z));
    if(Giant(z) && !z->mHasObject)m.mSpeciesFlags|=THROWN_USED;
    m.mSpeciesMind=z->mMindControlled?1:0;m.mSpeciesPhase=static_cast<int>(z->mZombiePhase);
    m.mSpeciesAltitude=z->mAltitude;m.mSpeciesVelX=z->mVelX;m.mSpeciesVelZ=z->mVelZ;m.mSpeciesScale=z->mScaleZombie;
}
bool ImportRelay(Zombie* z,const NetProto::MsgEscapedZombie& m) {
    if(!z || !NetProto::ValidSpeciesRelay(m))return false;
    SetFlags(z,m.mSpeciesFlags);
    // Relay already carries final HP. Restoring allegiance must not reapply hypno health.
    if(m.mSpeciesMind && !z->mMindControlled) { z->mMindControlled=true;z->mLastPortalX=-1;z->UpdateAnimSpeed(); }
    if(Giant(z)) {
        if(m.mSpeciesFlags&(THROWN_USED|NO_THROW)) {
            z->mHasObject=false;z->ReanimShowPrefix("Zombie_imp",RENDER_GROUP_HIDDEN);z->ReanimShowTrack("Zombie_gargantuar_whiterope",RENDER_GROUP_HIDDEN);
        }
        z->mScaleZombie=m.mSpeciesScale;
        if(m.mSpeciesFlags&FLYING_GIANT) {
            z->mZombiePhase=PHASE_IMP_GETTING_THROWN;z->mAltitude=m.mSpeciesAltitude;
            z->mVelX=m.mSpeciesVelX;z->mVelZ=m.mSpeciesVelZ;
            z->PlayZombieReanim("anim_walk",REANIM_LOOP,0,0);
        }
    }
    return true;
}
static void Put(std::vector<unsigned char>& out,unsigned v) { for(int i=0;i<4;++i)out.push_back(static_cast<unsigned char>(v>>(i*8))); }
static unsigned Get(const unsigned char* p) { return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24); }
static unsigned Hash(const unsigned char* p,size_t n) { unsigned h=2166136261u;for(size_t i=0;i<n;++i)h=(h^p[i])*16777619u;return h; }
std::vector<unsigned char> SaveTrailer(Board* b) {
    auto& t=tables[b];std::vector<unsigned char> out={'P','V','Z','S','P','R','0','1'};
    auto live=[&](unsigned id) { auto*z=b->mZombies.DataArrayTryToGet(id);return z&&!z->IsDeadOrDying(); };
    unsigned count=0;for(const auto& s:t.states)if(live(s.first))++count;
    Put(out,24+8*count+16*static_cast<unsigned>(t.blasts.size()));Put(out,count);Put(out,static_cast<unsigned>(t.blasts.size()));
    for(const auto& s:t.states)if(live(s.first)){Put(out,s.first);Put(out,s.second);}
    for(const auto& s:t.blasts){Put(out,s.row);Put(out,s.x);Put(out,s.y);Put(out,s.mind?1:0);}
    Put(out,Hash(out.data(),out.size()));return out;
}
bool LoadTrailer(Board* b,const unsigned char* data,size_t n) {
    Runtime restored;
    if(n>=6)for(size_t pos=n-6;;--pos) {
        if(!std::memcmp(data+pos,"PVZSPR",6)) {
            const auto* p=data+pos;if(n-pos<24 || std::memcmp(p,"PVZSPR01",8))return false;
            const unsigned bytes=Get(p+8),count=Get(p+12),q=Get(p+16);
            if(count>b->mZombies.mMaxSize || bytes!=24ull+8ull*count+16ull*q || bytes>n-pos || Get(p+bytes-4)!=Hash(p,bytes-4))return false;
            const auto* r=p+20;
            for(unsigned i=0;i<count;++i,r+=8) {
                const unsigned id=Get(r),flags=Get(r+4);auto*z=b->mZombies.DataArrayTryToGet(id);
                if(!z || !flags || (flags&~15u) || !restored.states.emplace(id,flags).second ||
                    ((flags&7) && !Giant(z)) || ((flags&8) && z->mZombieType!=ZOMBIE_JACK_IN_THE_BOX) ||
                    ((flags&4) && z->mZombiePhase!=PHASE_IMP_GETTING_THROWN))return false;
            }
            for(unsigned i=0;i<q;++i,r+=16) {
                const int row=static_cast<int>(Get(r)),x=static_cast<int>(Get(r+4)),y=static_cast<int>(Get(r+8));const unsigned mind=Get(r+12);
                if(row<0 || row>=MAX_GRID_SIZE_Y || x< -4096 || x>4096 || y< -4096 || y>4096 || mind>1)return false;
                restored.blasts.push_back({row,x,y,mind!=0});
            }
            tables[b]=std::move(restored);return true;
        }
        if(pos==0)break;
    }
    Reset(b);return true;
}
}
