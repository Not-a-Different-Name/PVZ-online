#pragma once
#include "RunState.h"
#include "RunSpeciesRules.h"

// Stateless species consumers: no Zombie/Board layout or save ABI changes.
namespace RunSpeciesEffects {
inline int Count(Zombie* z,int id) {
    if(!z || !z->mApp || !z->mBoard || !z->mApp->IsRunMode() ||
       !z->IsOnBoard() || z->mZombieType==ZombieType::ZOMBIE_BOSS ||
       z->mBoard->IsRunBossZombie(z)) return 0;
    auto* run=z->mApp->GetRunState();
    return run ? RunSpeciesRules::ClampedStacks(id,run->ZombieBuffCount(id)) : 0;
}
inline bool Fresh(Zombie* z) {
    return RunSpeciesRules::ApplyFreshTraits() && z && z->mApp && z->mBoard &&
        z->mApp->IsRunMode() && z->IsOnBoard() && z->mZombieType!=ZombieType::ZOMBIE_BOSS;
}
inline void ApplyInitialHealth(Zombie* z) {
    if(!Fresh(z))return;
    using namespace RunSpeciesRules;
    if(z->mZombieType==ZombieType::ZOMBIE_ZAMBONI)
        z->mBodyHealth=ScalePositive(z->mBodyHealth,ZamboniHealthPermille(Count(z,FAST_ZAMBONI)));
    if(z->mZombieType==ZombieType::ZOMBIE_POGO)
        z->mBodyHealth=ScalePositive(z->mBodyHealth,LinearPermille(POGO_ASSAULT,Count(z,POGO_ASSAULT)));
    if(z->mZombieType==ZombieType::ZOMBIE_FOOTBALL) {
        const int p=LinearPermille(HEAVY_FOOTBALL,Count(z,HEAVY_FOOTBALL));
        z->mBodyHealth=ScalePositive(z->mBodyHealth,p);
        z->mHelmHealth=ScalePositive(z->mHelmHealth,p);
        z->mShieldHealth=ScalePositive(z->mShieldHealth,p);
        z->mFlyingHealth=ScalePositive(z->mFlyingHealth,p);
    }
    const int p=LinearPermille(ALL_ARMOR,Count(z,ALL_ARMOR));
    z->mHelmHealth=ScalePositive(z->mHelmHealth,p);
    z->mShieldHealth=ScalePositive(z->mShieldHealth,p);
    z->mFlyingHealth=ScalePositive(z->mFlyingHealth,p);
    if (Count(z, ELITE_WAVES) > 0) {
        z->mBodyHealth = ScalePositive(z->mBodyHealth, 3000);
        z->mHelmHealth = ScalePositive(z->mHelmHealth, 3000);
        z->mShieldHealth = ScalePositive(z->mShieldHealth, 3000);
        z->mFlyingHealth = ScalePositive(z->mFlyingHealth, 3000);
    }
    if (z->mZombieType == ZombieType::ZOMBIE_NORMAL && Count(z, BASIC_TOUGHNESS) > 0)
        z->mBodyHealth = ScalePositive(z->mBodyHealth, 3000);
}
inline int Attack(Zombie* z,int damage) {
    const int id=z->mZombieType==ZombieType::ZOMBIE_FOOTBALL ? RunSpeciesRules::HEAVY_FOOTBALL :
        z->mZombieType==ZombieType::ZOMBIE_DIGGER ? RunSpeciesRules::DIGGER_AMBUSH : -1;
    return id<0 ? damage : RunSpeciesRules::ScalePositive(damage,RunSpeciesRules::LinearPermille(id,Count(z,id)));
}
inline float WalkFactor(Zombie* z) {
    const int id=z->mZombieType==ZombieType::ZOMBIE_ZAMBONI ? RunSpeciesRules::FAST_ZAMBONI :
        z->mZombieType==ZombieType::ZOMBIE_JACK_IN_THE_BOX ? RunSpeciesRules::JACK_DEATH :
        z->mZombieType==ZombieType::ZOMBIE_POGO ? RunSpeciesRules::POGO_ASSAULT : -1;
    return id<0 ? 1.0f : RunSpeciesRules::LinearPermille(id,Count(z,id))/1000.0f;
}
inline float VisualScale(Zombie* z) {
    return z->mScaleZombie * (z->mZombieType==ZombieType::ZOMBIE_FOOTBALL &&
        Count(z,RunSpeciesRules::HEAVY_FOOTBALL)>0 ? 1.5f : 1.0f);
}
}
