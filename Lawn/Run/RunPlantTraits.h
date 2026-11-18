#pragma once

#include "../../LawnApp.h"
#include <algorithm>

namespace RunPlantTraits {
inline bool CompactCob(const LawnApp* app) { return app->RunExtraBuffCount(RUN_BUFF2_COB_COMPACT) > 0; }
inline bool BowlingCabbage(const LawnApp* app) { return app->RunExtraBuffCount(RUN_BUFF2_CABBAGE_BOWL) > 0; }
inline bool AllBowling(const LawnApp* app, SeedType seed)
{
    if (seed < SEED_PEASHOOTER || seed >= SEED_IMITATER || app->mGameMode != GAMEMODE_ADVENTURE ||
        app->RunExtraBuffCount(RUN_BUFF2_ALL_BOWLING) == 0)
        return false;
    switch (seed)
    {
    case SEED_CHERRYBOMB:
    case SEED_POTATOMINE:
    case SEED_SQUASH:
    case SEED_JALAPENO:
    case SEED_DOOMSHROOM:
    case SEED_ICESHROOM:
    case SEED_TANGLEKELP:
    case SEED_BLOVER:
        return false;
    default:
        return true;
    }
}
inline bool ExplosiveSeaShroomBowling(const LawnApp* app, SeedType seed)
{
    return seed == SEED_SEASHROOM && app->mGameMode == GAMEMODE_ADVENTURE &&
        app->RunExtraBuffCount(RUN_BUFF2_SEASHROOM_EXPLOSIVE_BOWLING) > 0;
}
inline bool RollingBowling(const LawnApp* app, SeedType seed)
{
    return AllBowling(app, seed) ||
        (seed == SEED_EXPLODE_O_NUT && app->mGameMode == GAMEMODE_ADVENTURE) ||
        (seed == SEED_WALLNUT && app->RunExtraBuffCount(RUN_BUFF2_NUT_BOWLING) > 0) ||
        (seed == SEED_CABBAGEPULT && BowlingCabbage(app));
}
inline float CattailFactor(int streak)
{
    constexpr float factor[] = {1.0f, 1.15f, 1.3225f, 1.520875f, 1.74900625f,
        2.01135719f, 2.31306077f, 2.66001988f, 3.0f};
    return factor[std::clamp(streak, 0, 8)];
}
}
