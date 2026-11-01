#pragma once

// 100 simulation ticks per second. Reuse Gatling's otherwise idle mRunPurpleShots:
// 0 ready, 1..200 next barrage tick, -800..-1 recovery. No object/save layout change.
#include <cmath>

namespace RunGatlingBarrage {
constexpr int Duration = 200;
constexpr int Shots = 150;
constexpr int Recovery = 800;
constexpr int Chance = 10;
constexpr float HalfAngleTangent = 0.57735026919f;
inline float HalfAngleDegrees() { return 30.0f; }
inline int ShotsThrough(int tick) { return tick * Shots / Duration; }
inline float AngleDegrees(int shot, float jitter) {
    // Half the rounds stay near the center; the rest sweep the whole fan three times.
    static constexpr float sweep[] = {-14, -9, -3, 3, 9, 14, 9, 3, -3, -9};
    const float normalizedAngle = shot % 2 == 0 ? jitter / 5.0f : (sweep[(shot / 2) % 10] + jitter) / 15.0f;
    return normalizedAngle * HalfAngleDegrees();
}
}
