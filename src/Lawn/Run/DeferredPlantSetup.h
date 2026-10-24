#pragma once
#include <cstdint>
#include <vector>

// Transient prefix state; saved with an optional, checked trailer, never as raw vector bytes.
struct DeferredPlantSetup {
    enum Kind : int32_t { NONE, ADVENTURE, I_ZOMBIE, SHOVEL, POTTER_COLUMN, BEGHOULED };
    enum Flags : uint32_t { INIT = 1, START = 2, NEXT_I_ZOMBIE = 4 };
    struct Ticket { int32_t seed, x, y, extra; };
    std::vector<Ticket> pending;
    uint32_t next = 0, flags = 0;
    int32_t kind = NONE;
    bool planning = false; // A synchronous planning pass never crosses a save or update boundary.
    bool HasPending() const { return next < pending.size(); }
    bool IsWaiting() const { return flags != 0 || HasPending(); }
    bool Contains(int x, int y) const {
        for (const auto& item : pending) if (item.x == x && item.y == y) return true;
        return false;
    }
    void Begin(Kind value) { pending.clear(); next = 0; kind = value; planning = false; }
    void Finish() { pending.clear(); next = 0; kind = NONE; planning = false; }
};
