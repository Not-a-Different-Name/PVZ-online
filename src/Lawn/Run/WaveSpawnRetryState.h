#pragma once
#include <cstdint>

// Four-byte prefix metadata does not change the legacy raw-save suffix alignment.
struct WaveSpawnRetryState {
    int32_t wave=-1,index=0,part=0;
};
static_assert(sizeof(WaveSpawnRetryState)==12 && alignof(WaveSpawnRetryState)==4);
