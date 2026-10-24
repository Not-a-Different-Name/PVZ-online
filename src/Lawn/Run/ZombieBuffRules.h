#pragma once

#include <cstdint>
#include "RunSpeciesRules.h"

// @pvz-online: one count keeps checkpoint, vote and wire arrays in agreement.
namespace RunZombieBuffRules
{
constexpr int COUNT = RunSpeciesRules::COUNT;
constexpr int PRE_SPECIES_COUNT = RunSpeciesRules::PRE_SPECIES_COUNT;
constexpr int PRE_TAX_COUNT = 9;
constexpr int LEGACY_COUNT = 8;

inline int RegenPerSecond(int maxBodyHealth, int stacks)
{
    if (maxBodyHealth <= 0 || stacks <= 0) return 0;
    const int layers = stacks > 50 ? 50 : stacks;
    const int64_t proportional = static_cast<int64_t>(maxBodyHealth) * (5 * layers) / 1000;
    const int cap = 500 * layers;
    const int amount = proportional < cap ? static_cast<int>(proportional) : cap;
    return amount > 0 ? amount : 1;
}

inline int SoftDuration(int frames, bool resistant)
{
    // Round upward so a positive control window never becomes immunity.
    return resistant && frames > 0 ? frames / 2 + frames % 2 : frames;
}
}
