#pragma once
#include "../../LawnApp.h"
#include "../Plant.h"
#include "../Board.h"
#include "../TeamPlay.h"
#include "TeamAttackContext.h"
namespace TeamSupportScope {
class PlantSnapshot {
 const TeamProgression::Strike* strike;
 TeamAttackContext::SnapshotScope snapshot;
 DamageStats::RemoteSource remote;
public:
 PlantSnapshot(LawnApp*app,const Plant*plant):strike(TeamPlay::PlantSupport(app,plant->mBoard?plant->mBoard->mPlants.DataArrayGetID(const_cast<Plant*>(plant)):0)),snapshot(app->GetRunState(),strike),remote(app->mDamageStats,strike?strike->caster:0,strike?strike->id:0){}
};
}
