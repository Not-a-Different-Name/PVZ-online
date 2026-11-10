#pragma once

#include <cstdint>
#include <string>

namespace WatchRoom {
constexpr uint16_t Join = 0xF02A;
constexpr uint16_t Role = 0xF02B;
constexpr uint16_t Roster = 0xF012;
constexpr uint8_t FirstViewer = 7;
constexpr uint8_t LastViewer = 10;
constexpr uint8_t Apply = 1, Cancel = 2, Approve = 3, Reject = 4, Demote = 5;
struct Viewer {
    uint8_t state = 0;
    uint32_t request = 0;
    std::string name;
};
}

