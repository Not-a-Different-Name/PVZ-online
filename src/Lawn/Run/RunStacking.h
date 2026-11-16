#pragma once
#include "../Board.h"
#include "../Plant.h"
#include "RunPlantTraits.h"
namespace RunStacking {
inline bool Enabled(const LawnApp* app) { return app->IsRunMode() && app->mGameMode==GAMEMODE_ADVENTURE && app->RunExtraBuffCount(RUN_BUFF2_STACK_FOUR)>0; }
inline bool Covers(Board* b,Plant* p,int x,int y) {
 return !p->mDead && !p->NotOnGround() && p->mRow==y && !RunPlantTraits::RollingBowling(b->mApp,p->mSeedType) &&
 (p->mPlantCol==x || (p->mSeedType==SEED_COBCANNON && !RunPlantTraits::CompactCob(b->mApp) && p->mPlantCol+1==x));
}
inline int Count(Board* b,int x,int y) { int count=0;Plant*p=nullptr;while(b->IteratePlants(p))if(Covers(b,p,x,y))++count;return count; }
inline Plant* Top(Board* b,int x,int y) { Plant*top=nullptr;Plant*p=nullptr;while(b->IteratePlants(p))if(Covers(b,p,x,y)&&(!top||p->mRenderOrder>top->mRenderOrder))top=p;return top; }
inline bool Full(Board* b,int x,int y,SeedType seed) {
 if(Count(b,x,y)>=4)return true;
 const bool cob=seed==SEED_COBCANNON || (seed==SEED_KERNELPULT && b->mApp->RunPlantUpgradeCount(SEED_KERNELPULT)>0);
 return cob && !RunPlantTraits::CompactCob(b->mApp) && (x+1>=9 || Count(b,x+1,y)>=4);
}
}
