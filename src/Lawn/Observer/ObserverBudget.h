#pragma once

#include <cstdint>

namespace Observer {

enum class CaptureDecision { Wait, Capture, Pause };

struct BattlePressure {
    bool criticalPending = false;
    bool writable = true;
    bool congested = false;
};

class CapturePacer {
public:
    CaptureDecision Poll(std::uint64_t now, const BattlePressure& pressure) {
        if (clocked && now < lastClock) return CaptureDecision::Wait;
        clocked = true;
        lastClock = now;
        if (pressure.criticalPending || !pressure.writable) return CaptureDecision::Pause;
        const std::uint64_t interval = pressure.congested ? 1000 : 200;
        if (started && now - lastCapture < interval) return CaptureDecision::Wait;
        started = true;
        lastCapture = now;
        return CaptureDecision::Capture;
    }
    void Clear() { *this = CapturePacer(); }

private:
    bool started = false, clocked = false;
    std::uint64_t lastCapture = 0, lastClock = 0;
};

}
