#pragma once
#include "../Board.h"
#include "../Plant.h"
#include "RunPlantTraits.h"
#include "RunState.h"
namespace RunStacking {
inline bool Enabled(const LawnApp* app) {
 if(!app->IsRunMode() || app->mGameMode!=GAMEMODE_ADVENTURE || !app->mRunState)return false;
 // Lawn geometry belongs to the recipient and must not inherit a card's combat snapshot.
 for(const auto& buff:app->mRunState->mBuffs)
  if(buff.mId==RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT+RUN_BUFF2_STACK_FOUR)return buff.mCount>0;
 return false;
}

inline bool Covers(Board* b,Plant* p,int x,int y) {
 const auto seed=p->mSeedType==SEED_IMITATER && p->mImitaterType!=SEED_NONE ? p->mImitaterType : p->mSeedType;
 return !p->mDead && !p->NotOnGround() && p->mRow==y && !RunPlantTraits::RollingBowling(b->mApp,seed) &&
 (p->mPlantCol==x || (seed==SEED_COBCANNON && !RunPlantTraits::CompactCob(b->mApp) && p->mPlantCol+1==x));
}
inline int Count(Board* b,int x,int y) { int count=0;Plant*p=nullptr;while(b->IteratePlants(p))if(Covers(b,p,x,y))++count;return count; }
inline Plant* Top(Board* b,int x,int y) { Plant*top=nullptr;Plant*p=nullptr;while(b->IteratePlants(p))if(Covers(b,p,x,y)&&(!top||p->mRenderOrder>top->mRenderOrder))top=p;return top; }
inline bool Full(Board* b,int x,int y,SeedType seed) {
 if(RunPlantTraits::RollingBowling(b->mApp,seed))return false;
 if(Count(b,x,y)>=4)return true;
 const bool cob=seed==SEED_COBCANNON || (seed==SEED_KERNELPULT && b->mApp->RunPlantUpgradeCount(SEED_KERNELPULT)>0);
 return cob && !RunPlantTraits::CompactCob(b->mApp) && (x+1>=9 || Count(b,x+1,y)>=4);
}
inline int HealthLine(Board* b,Plant* plant) {
 if(!Enabled(b->mApp))return 0;
 int line=0;Plant*p=nullptr;
 while(b->IteratePlants(p))if(p!=plant && Covers(b,p,plant->mPlantCol,plant->mRow) && p->mRenderOrder<plant->mRenderOrder)++line;
 return line;
}

}
