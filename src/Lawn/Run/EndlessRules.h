#pragma once
#include <cstdint>
#include <cstddef>

// Shared deterministic rules; bits 4..6 are reserved, zero preserves legacy endless runs.
namespace EndlessRules {
inline uint32_t Hash(const unsigned char* p,size_t n) {uint32_t h=2166136261u;for(size_t i=0;i<n;++i)h=(h^p[i])*16777619u;return h;}
constexpr int DEFAULT_OPTIONS = 0x8d; // new rules, boss every 10, themes and lineup preference.
inline bool Valid(int v) { return v == 0 || ((v & ~0x8f) == 0 && (v & 0x80) && (v & 3) <= 2); }
inline int BossPeriod(int v) { return !(v & 0x80) || !(v & 3) ? 0 : ((v & 3) == 1 ? 10 : 5); }
inline int Growth(int tier,int level) {
    int base=tier==1?1030:tier==2?1050:1000, value=1000;
    for(int i=0;i<level/5 && value<1500 && base!=1000;++i) value=value*base/1000;
    return value>1500?1500:value;
}
inline int SpawnPermille(int seat,int difficulty,int scale) {
    return seat*difficulty*scale/1000;
}
inline int Theme(int options,int seed,int level,int scene) {
    if (!(options & 0x80) || !(options & 4)) return 0;
    // Cycling the seeded order prevents adjacent repeats; four themes on water fields.
    unsigned x=(unsigned)seed; x^=x>>16; x*=0x85ebca6bu; x^=x>>13;
    int count=(scene==2 || scene==3)?4:3;
    return 1+((x%count+level/5)%count);
}
}
