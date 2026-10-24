#pragma once
#include <cstdint>

// PC legacy Board is four-byte aligned. Widen the transient prefix without
// raising Board's alignment or changing the length of its serialized suffix.
#pragma pack(push,4)
struct WaveHealthState {
    int64_t start=0;
    int64_t next=-1;
};
#pragma pack(pop)
static_assert(sizeof(WaveHealthState)==16 && alignof(WaveHealthState)==4);
