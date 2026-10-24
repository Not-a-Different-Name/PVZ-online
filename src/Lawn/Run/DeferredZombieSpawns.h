#pragma once
#include <cstdint>
#include <vector>

// Preserve the original selected type, tile and wave while admission waits.
struct DeferredZombieSpawn { int32_t type, x, y, wave, kind; };
struct DeferredZombieSpawns {
    std::vector<DeferredZombieSpawn> pending;
    uint32_t next = 0;
    bool HasPending() const { return next < pending.size(); }
    void Clear() { pending.clear(); next = 0; }
};
