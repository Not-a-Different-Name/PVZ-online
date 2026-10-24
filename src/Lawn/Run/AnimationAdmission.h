#pragma once
#include "../../ConstEnums.h"

namespace AnimationAdmission {
// Synchronous construction: body plus any mandatory head/flag/helmet/driver.
// AttachShield changes body layers without allocating another instance.
inline unsigned ZombieOne(ZombieType type) {
    if(PlantZombies::Armor(type))return 3;
    if(PlantZombies::Family(type)>=0)return 2;
    switch(type) {
    case ZombieType::ZOMBIE_ZOMBATAR:return 3;
    case ZombieType::ZOMBIE_FLAG:case ZombieType::ZOMBIE_BALLOON:
    case ZombieType::ZOMBIE_CONE_NEWSPAPER:case ZombieType::ZOMBIE_PAIL_NEWSPAPER:
    case ZombieType::ZOMBIE_PEA_HEAD:case ZombieType::ZOMBIE_PEA_HEAD_DOOR:
    case ZombieType::ZOMBIE_WALLNUT_HEAD:case ZombieType::ZOMBIE_WALLNUT_HEAD_DOOR:
    case ZombieType::ZOMBIE_TALLNUT_HEAD:
    case ZombieType::ZOMBIE_JALAPENO_HEAD:case ZombieType::ZOMBIE_JALAPENO_HEAD_DOOR:
    case ZombieType::ZOMBIE_GATLING_HEAD:case ZombieType::ZOMBIE_GATLING_HEAD_DOOR:
    case ZombieType::ZOMBIE_SQUASH_HEAD:case ZombieType::ZOMBIE_SQUASH_HEAD_DOOR:
    case ZombieType::ZOMBIE_BOSS:return 2;
    default:return 1;
    }
}
// Reserve the entire sled before creating its leader; deferred attachers retry separately.
inline unsigned ZombieGroup(ZombieType type,int wave) {
    const bool team=type==ZombieType::ZOMBIE_BOBSLED &&
        wave!=Zombie::ZOMBIE_WAVE_CUTSCENE && wave!=Zombie::ZOMBIE_WAVE_UI;
    return (team?4U:1U)*ZombieOne(type);
}
}
