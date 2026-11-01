#pragma once
#include "TeamProgressionState.h"
namespace TeamAttackContext {
inline thread_local bool exactBlast=false;
inline thread_local const TeamProgression::Strike* snapshot=nullptr;
inline thread_local const void* snapshotOwner=nullptr;
inline thread_local int sourceSeed=-1;
class SnapshotScope {
 const TeamProgression::Strike* old;const void* owner;
public:SnapshotScope(const void*r,const TeamProgression::Strike*s):old(snapshot),owner(snapshotOwner){snapshot=s;snapshotOwner=r;}
 ~SnapshotScope(){snapshot=old;snapshotOwner=owner;}
};
class SeedScope {int old;public:explicit SeedScope(int seed):old(sourceSeed){sourceSeed=seed;}~SeedScope(){sourceSeed=old;}};
inline int SnapshotCount(int id){if(snapshot)for(const auto&b:snapshot->buffs)if(b.id==id)return b.count;return 0;}

class ExactBlast {
    bool previous;
public: ExactBlast():previous(exactBlast){exactBlast=true;}
    ~ExactBlast(){exactBlast=previous;}
};
inline float BlastMultiplier(float ordinary){return exactBlast?1.0f:ordinary;}
}
