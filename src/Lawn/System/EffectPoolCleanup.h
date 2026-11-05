#pragma once
// Based on PR #3 by moanlixin. Trace both attachment/reanimation edges from real
// owners before retiring anything; unreachable cycles must not keep each other alive.
#include "../BoardInclude.h"
#include "../Run/RunCheckpointIO.h"
#include "../Observer/ObserverIsolation.h"
#ifdef __ANDROID__
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/Trail.h"
#include "../../PvzpLib/PvzpDebug.h"
#else
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/Trail.h"
#endif
#include <unordered_set>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdint>

namespace EffectPoolCleanup {
template<class Pool,class Item> inline bool Next(Pool&pool,Item*&item){
#ifdef __ANDROID__
    return pool.DataArrayIterate(item);
#else
    return pool.IterateNext(item);
#endif
}
inline void Log(const std::string&text){
#ifdef __ANDROID__
    PvzpLogLn("{}",text);
#else
    TodLog("%s",text.c_str());
#endif
}
struct Plan {
    bool valid=true;
    std::unordered_set<unsigned> attachments,reanimations,particles,trails;
    std::vector<unsigned> retireAttachments,retireReanimations;
    bool Empty()const{return retireAttachments.empty()&&retireReanimations.empty();}
};
inline bool Optional(Reanimation* r){return r->mReanimationType==REANIM_ZOMBIE||ReanimationMayOmit(r->mReanimationType);}
inline Plan Inspect(Board* b){
    Plan p;auto&e=*b->mApp->mEffectSystem;auto&ap=e.mAttachmentHolder->mAttachments;auto&rp=e.mReanimationHolder->mReanimations;
    std::vector<std::pair<bool,unsigned>> pending;
    auto holdA=[&](AttachmentID id){unsigned v=unsigned(id);if(v&&ap.DataArrayTryToGet(v)&&p.attachments.insert(v).second)pending.push_back({false,v});};
    auto holdR=[&](ReanimationID id){unsigned v=unsigned(id);if(v&&rp.DataArrayTryToGet(v)&&p.reanimations.insert(v).second)pending.push_back({true,v});};
    auto holdP=[&](ParticleSystemID id){if(id)p.particles.insert(unsigned(id));};
    // Include dead owners still in their pools: they retain their own disposal duty.
    Zombie*z=nullptr;while(Next(b->mZombies,z)){
        holdA(z->mAttachmentID);for(auto id:{z->mBodyReanimID,z->mBossFireBallReanimID,z->mSpecialHeadReanimID,z->mPaperHelmReanimID,z->mZombatarHeadReanimID,z->mMoweredReanimID})holdR(id);
    }
    Projectile*shot=nullptr;while(Next(b->mProjectiles,shot))holdA(shot->mAttachmentID);
    Coin*coin=nullptr;while(Next(b->mCoins,coin))holdA(coin->mAttachmentID);
    Plant*plant=nullptr;while(Next(b->mPlants,plant)){
        for(auto id:{plant->mBodyReanimID,plant->mHeadReanimID,plant->mHeadReanimID2,plant->mHeadReanimID3,plant->mBlinkReanimID,plant->mLightReanimID,plant->mSleepingReanimID})holdR(id);
        holdP(plant->mParticleID);
    }
    LawnMower*mower=nullptr;while(Next(b->mLawnMowers,mower))holdR(mower->mReanimID);
    GridItem*grid=nullptr;while(Next(b->mGridItems,grid)){holdR(grid->mGridItemReanimID);holdP(grid->mGridItemParticleID);}
    for(int row=0;row<MAX_GRID_SIZE_Y;++row){for(auto id:b->mFwooshID[row])holdR(id);holdP(b->mIceParticleID[row]);}
    holdP(b->mTutorialParticleID);holdP(b->mPoolSparklyParticleID);
    holdR(b->mChallenge->mReanimChallenge);for(auto id:b->mChallenge->mReanimClouds)holdR(id);
    holdR(b->mCursorObject->mReanimCursorID);for(auto id:b->mAdvice->mTextReanimID)holdR(id);
    holdR(b->mApp->mCrazyDaveReanimID);holdR(b->mApp->mCrazyDaveBlinkReanimID);
    holdR(b->mCutScene->mZombiesWonReanimID);
    Reanimation*r=nullptr;while(Next(rp,r)){
        if((r->mTrackInstances&&!r->mDefinition)||(r->mDefinition&&r->mDefinition->mTracks.count>0&&!r->mTrackInstances)){p.valid=false;return p;}
        // Preserve every non-whitelisted type even if its owner is outside this board.
        if(!Optional(r))holdR(ReanimationID(rp.DataArrayGetID(r)));
    }
    Attachment*a=nullptr;while(Next(ap,a))if(a->mNumEffects<0||a->mNumEffects>MAX_EFFECTS_PER_ATTACHMENT){p.valid=false;return p;}
    while(!pending.empty()){
        auto node=pending.back();pending.pop_back();
        if(node.first){
            auto*item=rp.DataArrayTryToGet(node.second);if(!item||!item->mDefinition||!item->mTrackInstances)continue;
            for(int i=0;i<item->mDefinition->mTracks.count;++i)holdA(item->mTrackInstances[i].mAttachmentID);
        }else{
            auto*item=ap.DataArrayTryToGet(node.second);if(!item)continue;
            for(int i=0;i<item->mNumEffects;++i){const auto&fx=item->mEffectArray[i];switch(fx.mEffectType){
                case EFFECT_ATTACHMENT:holdA(AttachmentID(fx.mEffectID));break;
                case EFFECT_REANIM:holdR(ReanimationID(fx.mEffectID));break;
                case EFFECT_PARTICLE:p.particles.insert(fx.mEffectID);break;
                case EFFECT_TRAIL:p.trails.insert(fx.mEffectID);break;
                default:break;
            }}
        }
    }
    a=nullptr;while(Next(ap,a)){unsigned id=ap.DataArrayGetID(a);if(!p.attachments.count(id))p.retireAttachments.push_back(id);}
    r=nullptr;while(Next(rp,r)){unsigned id=rp.DataArrayGetID(r);if(Optional(r)&&!p.reanimations.count(id))p.retireReanimations.push_back(id);}
    return p;
}
inline void Apply(Board*b,const Plan&p){
    if(!p.valid||p.Empty())return;
    auto&e=*b->mApp->mEffectSystem;auto&ap=e.mAttachmentHolder->mAttachments;auto&rp=e.mReanimationHolder->mReanimations;
    std::unordered_set<unsigned> particles,trails;
    // Sever edges BEFORE destruction. Recursive AttachmentDie would overflow on
    // an orphan cycle, or destroy an object also held by a live, reachable owner.
    for(unsigned id:p.retireAttachments)if(auto*a=ap.DataArrayTryToGet(id)){
        for(int i=0;i<a->mNumEffects;++i){const auto&fx=a->mEffectArray[i];
            if(fx.mEffectType==EFFECT_PARTICLE&&!p.particles.count(fx.mEffectID))particles.insert(fx.mEffectID);
            if(fx.mEffectType==EFFECT_TRAIL&&!p.trails.count(fx.mEffectID))trails.insert(fx.mEffectID);
        }
        a->mNumEffects=0;a->mDead=true;
    }
    for(unsigned id:p.retireReanimations)if(auto*r=rp.DataArrayTryToGet(id)){
        if(r->mDefinition&&r->mTrackInstances)for(int i=0;i<r->mDefinition->mTracks.count;++i)r->mTrackInstances[i].mAttachmentID=ATTACHMENTID_NULL;
        r->mDead=true;rp.DataArrayFree(r);
    }
    for(unsigned id:p.retireAttachments)if(auto*a=ap.DataArrayTryToGet(id))ap.DataArrayFree(a);
    for(unsigned id:particles)if(auto*item=e.mParticleHolder->mParticleSystems.DataArrayTryToGet(id)){
        // Standalone effects may have direct owners outside this attachment graph.
        if(item->mIsAttachment)item->ParticleSystemDie();
    }
    for(unsigned id:trails)if(auto*item=e.mTrailHolder->mTrails.DataArrayTryToGet(id))if(item->mIsAttachment)item->mDead=true;
}
inline bool PreserveInput(const std::string&path){
    Sexy::Buffer bytes;if(!Sexy::gSexyAppBase->ReadBufferFromFile(path,&bytes,false))return false;
    const int size=bytes.GetDataLen();if(size<=0||size>64*1024*1024)return false;
    const auto*data=bytes.GetDataPtr();uint64_t hash=14695981039346656037ull;
    for(int i=0;i<size;++i)hash=(hash^data[i])*1099511628211ull;
    std::ostringstream suffix;suffix<<std::hex<<hash;
    const std::string backup=path+".effects-before-"+suffix.str()+".bak";
    if(Sexy::gSexyAppBase->FileExists(backup)){
        Sexy::Buffer old;return Sexy::gSexyAppBase->ReadBufferFromFile(backup,&old,false)&&old.GetDataLen()==size&&std::memcmp(old.GetDataPtr(),data,size)==0;
    }
    return RunCheckpointIO::AtomicWrite(backup,std::vector<unsigned char>(data,data+size));
}
inline void AfterLoad(Board*b,const std::string&path){
    if(ObserverIsolation::Active())return;
    const auto p=Inspect(b);
    if(!p.valid){Log("[save] effect cleanup skipped: invalid effect metadata");return;}
    if(p.Empty())return;
    if(!PreserveInput(path)){Log("[save] effect cleanup skipped: original backup unavailable");return;}
    Apply(b,p);
    Log("[save] effect cleanup after validated load: attachments "+std::to_string(p.retireAttachments.size())+
        ", reanimations "+std::to_string(p.retireReanimations.size())+"; original input backed up");
}
}
