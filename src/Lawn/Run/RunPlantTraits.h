#pragma once

#include "../../LawnApp.h"
#include <algorithm>

namespace RunPlantTraits {
inline bool CompactCob(const LawnApp* app) { return app->RunExtraBuffCount(RUN_BUFF2_COB_COMPACT) > 0; }
inline bool BowlingCabbage(const LawnApp* app) { return app->RunExtraBuffCount(RUN_BUFF2_CABBAGE_BOWL) > 0; }
inline float CattailFactor(int streak)
{
    constexpr float factor[] = {1.0f, 1.15f, 1.3225f, 1.520875f, 1.74900625f,
        2.01135719f, 2.31306077f, 2.66001988f, 3.0f};
    return factor[std::clamp(streak, 0, 8)];
}
}
