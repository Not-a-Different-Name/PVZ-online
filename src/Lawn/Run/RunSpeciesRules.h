#pragma once
#include <cstdint>
#include <limits>

// Appended IDs: keep the original 0..9 checkpoint and vote meanings intact.
namespace RunSpeciesRules {
enum Id : int {
    GIANT_CHAIN = 10, JACK_DEATH = 11, ARMORED_BALLOON = 12,
    FAST_ZAMBONI = 13, HEAVY_FOOTBALL = 14, ALL_ARMOR = 15, DIGGER_AMBUSH = 16
};
constexpr int COUNT = 17;
constexpr int PRE_SPECIES_COUNT = 10;
inline int MaxStacks(int id) {
    return id==GIANT_CHAIN ? 2 : id==ARMORED_BALLOON ? 1 : 255;
}
inline int ClampedStacks(int id, int stacks) {
    return stacks<0 ? 0 : stacks>MaxStacks(id) ? MaxStacks(id) : stacks;
}
inline int LinearPermille(int id, int stacks) {
    stacks=ClampedStacks(id,stacks);
    switch(id) {
    case ARMORED_BALLOON: return 1000+4000*stacks;
    case HEAVY_FOOTBALL: return 1000+2000*stacks;
    case JACK_DEATH: case FAST_ZAMBONI: case ALL_ARMOR: case DIGGER_AMBUSH:
        return 1000+1000*stacks;
    default: return 1000;
    }
}
inline int ZamboniHealthPermille(int stacks) {
    return 1000+500*ClampedStacks(FAST_ZAMBONI,stacks);
}
inline int ScalePositive(int value, int permille) {
    if(value<=0 || permille==1000)return value;
    const int64_t scaled=int64_t(value)*permille/1000;
    return scaled>(std::numeric_limits<int>::max)() ? (std::numeric_limits<int>::max)() : int(scaled);
}

// Some Board paths assign the BOSS marker only after ZombieInitialize.
// Scope the constructor instead of dividing saturated health back afterwards.
enum class SpawnKind { NORMAL, RUN_BOSS, RELAY, PREVIEW };
inline thread_local SpawnKind currentSpawnKind=SpawnKind::NORMAL;
class SpawnScope {
    SpawnKind previous;
public:
    explicit SpawnScope(SpawnKind kind):previous(currentSpawnKind){currentSpawnKind=kind;}
    ~SpawnScope(){currentSpawnKind=previous;}
    SpawnScope(const SpawnScope&)=delete;
    SpawnScope& operator=(const SpawnScope&)=delete;
};
inline bool ApplyFreshTraits(){return currentSpawnKind==SpawnKind::NORMAL;}
}
