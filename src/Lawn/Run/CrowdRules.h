#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

// @pvz-online: D005 is a per-run rule; old saves must not opt in merely by updating.
namespace CrowdRules {
enum Rule { LEGACY = 0, ABOVE_80_HALF = 1, CURRENT = ABOVE_80_HALF };
constexpr int THRESHOLD = 80;
constexpr int MAX_WAVE_ENTRIES = 1600;

inline bool Valid(unsigned rule) { return rule == LEGACY || rule == ABOVE_80_HALF; }

inline int TargetCount(int originalCount, int rule) {
    if (originalCount <= 0) return 0;
    return rule == ABOVE_80_HALF && originalCount > THRESHOLD
        ? THRESHOLD + (originalCount - THRESHOLD) / 2 : originalCount;
}

inline uint32_t Mix(uint32_t value) {
    value ^= value >> 16; value *= 0x85ebca6bu;
    value ^= value >> 13; value *= 0xc2b2ae35u;
    return value ^ (value >> 16);
}

// Produce original indices, preserving order, without rerolling types or touching game RNG.
// The caller explicitly supplies flag/scripted indices; invalid plans never partially publish.
inline bool PlanWave(int originalCount, unsigned rule, uint32_t seed,
                     const std::vector<int>& protectedIndices, std::vector<int>& keptIndices) {
    if (!Valid(rule) || originalCount < 0 || originalCount > MAX_WAVE_ENTRIES) return false;
    const int target = TargetCount(originalCount, static_cast<int>(rule));
    if (protectedIndices.size() > static_cast<size_t>(target)) return false;
    std::vector<unsigned char> protectedMask(static_cast<size_t>(originalCount), 0);
    std::vector<int> result;
    result.reserve(static_cast<size_t>(target));
    for (int index : protectedIndices) {
        if (index < 0 || index >= originalCount || protectedMask[index]) return false;
        protectedMask[index] = 1;
        result.push_back(index);
    }
    struct Candidate { uint32_t rank; int index; };
    std::vector<Candidate> candidates;
    for (int index = 0; index < originalCount; ++index)
        if (!protectedMask[index]) candidates.push_back({Mix(seed ^ Mix(static_cast<uint32_t>(index))), index});
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.rank != b.rank ? a.rank < b.rank : a.index < b.index;
    });
    for (size_t i = 0; result.size() < static_cast<size_t>(target); ++i)
        result.push_back(candidates[i].index);
    std::sort(result.begin(), result.end());
    keptIndices.swap(result);
    return true;
}
}
