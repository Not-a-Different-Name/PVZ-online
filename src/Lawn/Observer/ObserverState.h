#pragma once

#include <bitset>
#include <cstdint>
#include <utility>

namespace Observer {

struct Identity {
    std::uint64_t subscription = 0;
    std::uint64_t player = 0;
    std::uint64_t run = 0;
    std::uint32_t generation = 0;
    std::uint8_t seat = 0;

    bool Valid() const {
        return subscription && player && run && generation && seat >= 1 && seat <= 6;
    }
    bool operator==(const Identity& other) const {
        return subscription == other.subscription && player == other.player && run == other.run &&
            generation == other.generation && seat == other.seat;
    }
};

enum class Status { Closed, Waiting, Showing };
enum class EndReason { None, User, Timeout, TargetLeft, ContextChanged, Disconnected };

template<typename Frame, typename Renderer>
class ViewState {
public:
    static constexpr std::uint64_t TimeoutMs = 3000;

    explicit ViewState(Renderer& renderer) : renderer(renderer) {}
    ~ViewState() { renderer.Clear(); }
    ViewState(const ViewState&) = delete;
    ViewState& operator=(const ViewState&) = delete;

    bool Begin(const Identity& next, std::uint64_t now) {
        End(EndReason::ContextChanged);
        if (!next.Valid()) return false;
        identity = next;
        started = lastValid = clock = now;
        status = Status::Waiting;
        reason = EndReason::None;
        return true;
    }

    bool Accept(const Frame& frame, std::uint64_t now) {
        if (status == Status::Closed || now < clock) return false;
        Tick(now);
        if (status == Status::Closed || !(frame.identity == identity) || !frame.sequence ||
            frame.sequence <= sequence || !renderer.Prepare(frame)) return false;
        sequence = frame.sequence;
        lastValid = now;
        status = Status::Showing;
        return true;
    }

    void Tick(std::uint64_t now) {
        if (status == Status::Closed || now < clock) return;
        clock = now;
        if (now - lastValid >= TimeoutMs) End(EndReason::Timeout);
    }

    void End(EndReason ended = EndReason::User) {
        renderer.Clear();
        identity = {};
        sequence = started = lastValid = clock = 0;
        status = Status::Closed;
        reason = ended;
    }

    void Context(bool connected, bool playing, bool focused, bool sameTarget) {
        if (status == Status::Closed) return;
        if (!connected) End(EndReason::Disconnected);
        else if (!sameTarget) End(EndReason::TargetLeft);
        else if (!playing || !focused) End(EndReason::ContextChanged);
    }

    Status GetStatus() const { return status; }
    EndReason GetEndReason() const { return reason; }
    const Identity& GetIdentity() const { return identity; }
    std::uint64_t GetSequence() const { return sequence; }
    std::uint64_t Age(std::uint64_t now) const {
        return status == Status::Closed || now < lastValid ? 0 : now - lastValid;
    }

private:
    Renderer& renderer;
    Identity identity;
    std::uint64_t sequence = 0, started = 0, lastValid = 0, clock = 0;
    Status status = Status::Closed;
    EndReason reason = EndReason::None;
};

enum class Input { KeyDown, KeyUp, Character, PointerDown, PointerUp, PointerMove, Wheel, Cancel };

class InputFence {
public:
    void Open(const std::bitset<256>& heldKeys = {}, const std::bitset<64>& heldPointers = {}) {
        watching = true;
        keys |= heldKeys;
        pointers |= heldPointers;
    }
    void Close() { watching = false; }
    bool Active() const { return watching; }

    bool Consume(Input input, int code = 0) {
        if (input == Input::Cancel) {
            const bool consumed = watching || keys.any() || pointers.any();
            keys.reset();
            pointers.reset();
            return consumed;
        }
        if (input == Input::KeyDown || input == Input::KeyUp) {
            if (code < 0 || code >= 256) return watching;
            const bool consumed = watching || keys.test(code);
            if (input == Input::KeyUp) keys.reset(code);
            else if (consumed) keys.set(code);
            return consumed;
        }
        if (input == Input::PointerDown || input == Input::PointerUp || input == Input::PointerMove) {
            if (code < 0 || code >= 64) return true;
            const bool consumed = watching || (input==Input::PointerMove?pointers.any():pointers.test(code));
            if (input == Input::PointerUp) pointers.reset(code);
            else if (input == Input::PointerDown && consumed) pointers.set(code);
            return consumed;
        }
        return watching || (input == Input::Character && keys.any());
    }

private:
    bool watching = false;
    std::bitset<256> keys;
    std::bitset<64> pointers;
};

}
