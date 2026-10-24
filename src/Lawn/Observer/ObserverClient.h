#pragma once

#include "ObserverTimeline.h"
#include <array>
#include <bitset>
#include <cstdint>
#include <utility>

namespace Observer {

enum class ViewerRole { Outside, Member, ReconnectWait };
enum class ViewMode { Closed, Selecting, LegacyV1, NormalV2 };
enum class ActionKind { None, Exit, Next, Seat };
enum class Platform { PC, Android };

struct Access {
    std::uint64_t viewerRoom = 0, targetRoom = 0, viewer = 0, target = 0;
    std::uint8_t viewerSeat = 0, targetSeat = 0;
    ViewerRole role = ViewerRole::Outside;
    bool authenticated = false, connected = false, targetConnected = false, targetActive = false;

    bool Allows(const Identity& identity) const {
        return authenticated && connected && targetConnected && targetActive && viewerRoom &&
            viewerRoom == targetRoom && viewer && target && viewer != target &&
            target == identity.player && targetSeat == identity.seat && identity.Valid() &&
            ((role == ViewerRole::Member && viewerSeat >= 1 && viewerSeat <= 6 && viewerSeat != targetSeat) ||
                (role == ViewerRole::ReconnectWait && viewerSeat <= 6));
    }
};

struct Capabilities {
    bool localV2 = false, sourceV2 = false, relayV2 = false;
    bool direct = false, directAgreed = false, legacyRoute = false;
    std::uint32_t localRevision = 0, sourceRevision = 0;

    ViewMode Select() const {
        if (localV2 && sourceV2 && (direct ? directAgreed : relayV2) && localRevision &&
            localRevision == sourceRevision) return ViewMode::NormalV2;
        return legacyRoute ? ViewMode::LegacyV1 : ViewMode::Closed;
    }
};

struct Action {
    ActionKind kind = ActionKind::None;
    unsigned seat = 0;
    std::uint64_t ticket = 0;
    Identity previous;
    EndReason reason = EndReason::None;
};

class TouchSlots {
public:
    int Find(std::int64_t native) const {
        for (std::size_t index = 0; index < slots.size(); ++index)
            if (slots[index].used && slots[index].native == native) return static_cast<int>(index);
        return -1;
    }
    int Down(std::int64_t native) {
        const auto found = Find(native);
        if (found >= 0) return found;
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (!slots[index].used) { slots[index] = {native, true}; return static_cast<int>(index); }
        }
        return -1;
    }
    void Up(std::int64_t native) { const auto found = Find(native); if (found >= 0) slots[found] = {}; }
    void Clear() { slots = {}; }

private:
    struct Slot { std::int64_t native = 0; bool used = false; };
    std::array<Slot, 64> slots{};
};

struct Viewport {
    int x = 0, y = 0, width = 800, height = 600;
    bool Project(int screenX, int screenY, int& boardX, int& boardY) const {
        const auto relativeX = static_cast<std::int64_t>(screenX) - x;
        const auto relativeY = static_cast<std::int64_t>(screenY) - y;
        if (width <= 0 || height <= 0 || relativeX < 0 || relativeY < 0 || relativeX >= width || relativeY >= height) return false;
        boardX = static_cast<int>(relativeX * 800 / width);
        boardY = static_cast<int>(relativeY * 600 / height);
        return true;
    }
};

inline ActionKind ControlAt(int x, int y) {
    if (y >= 8 && y < 54 && x >= 530 && x < 655) return ActionKind::Next;
    if (y >= 8 && y < 54 && x >= 665 && x < 790) return ActionKind::Exit;
    return ActionKind::None;
}

template<typename Frame, typename Renderer>
class Client {
public:
    explicit Client(Renderer& renderer, Platform platform, BufferConfig config = {})
        : timeline(renderer, config), platform(platform) {}

    bool Open(const Access& access, const Capabilities& caps, const Identity& target, std::uint64_t now,
        const std::bitset<256>& heldKeys = {}, const std::bitset<64>& heldPointers = {}, bool holdV = false) {
        if (Active() || action.kind != ActionKind::None) return false;
        action = {};
        if (!Adopt(access, caps, target, now)) return false;
        heldNavigation = heldKeys;
        fence.Open(heldKeys, heldPointers);
        heldToWatch = platform == Platform::PC && holdV;
        textTail = false;
        return true;
    }

    bool CompleteSelection(std::uint64_t ticket, const Access& access, const Capabilities& caps,
        const Identity& target, std::uint64_t now) {
        if (mode != ViewMode::Selecting || !ticket || ticket != selectionTicket ||
            target.subscription == previousSubscription || now < lastNow) return false;
        if (!Adopt(access, caps, target, now)) return false;
        action = {};
        return true;
    }

    bool Receive(const Frame& frame, std::uint64_t sourceMs, std::uint64_t now) {
        if (mode != ViewMode::NormalV2 || now < lastNow) return false;
        lastNow = now;
        const bool accepted = timeline.Accept(frame, sourceMs, now);
        SyncEnd();
        return accepted;
    }

    bool LegacyReady(const Identity& target, std::uint64_t sequence, std::uint64_t now) {
        if (mode != ViewMode::LegacyV1 || now < lastNow || !(target == identity) ||
            !sequence || sequence <= legacySequence) return false;
        if (now - lastLegacy >= Timeline<Frame, Renderer>::TimeoutMs) { Close(EndReason::Timeout); return false; }
        lastNow = lastLegacy = now;
        legacySequence = sequence;
        return true;
    }

    bool Congestion(const Identity& target, bool pause, std::uint64_t now) {
        if (mode != ViewMode::NormalV2 || now < lastNow) return false;
        lastNow = now;
        const bool accepted = timeline.Congestion(target, pause, now);
        SyncEnd();
        return accepted;
    }

    void Tick(std::uint64_t now, const Access& current, bool sceneAllows, bool focused, bool sameStream) {
        if (!Active() || now < lastNow) return;
        lastNow = now;
        if (!current.connected) { Close(EndReason::Disconnected); return; }
        if (current.viewerRoom != viewerRoom || current.viewer != viewerPlayer) { Close(EndReason::ContextChanged); return; }
        if (!sceneAllows || !focused) { Close(EndReason::ContextChanged); return; }
        if (!sameStream || !current.Allows(identity)) { Close(EndReason::TargetLeft); return; }
        if (mode == ViewMode::NormalV2) { timeline.Tick(now); SyncEnd(); }
        else if (mode == ViewMode::LegacyV1 && now - lastLegacy >= Timeline<Frame, Renderer>::TimeoutMs)
            Close(EndReason::Timeout);
        else if (mode == ViewMode::Selecting && now - selectionAt >= Timeline<Frame, Renderer>::TimeoutMs)
            Close(EndReason::Timeout);
    }

    void Close(EndReason reason = EndReason::User) {
        const bool wasActive = Active();
        const auto previous = identity;
        if (wasActive) textTail = true;
        timeline.End(reason);
        previousSubscription = identity.subscription ? identity.subscription : previousSubscription;
        identity = {};
        viewerRoom = viewerPlayer = 0;
        mode = ViewMode::Closed;
        legacySequence = 0;
        fence.Close();
        presses = {};
        if (wasActive) action = {ActionKind::Exit, 0, 0, previous, reason};
        ended = reason;
    }

    bool Key(Input input, int code = 0) {
        const bool wasActive = Active();
        const bool consumed = fence.Consume(input, code);
        if (input == Input::Character) return consumed || textTail;
        if (input != Input::KeyDown && input != Input::KeyUp) return consumed;
        if (!consumed && input == Input::KeyDown) textTail = false;
        if (code < 0 || code >= 256) return consumed;
        const bool repeat = heldNavigation.test(code);
        if (input == Input::KeyUp) heldNavigation.reset(code);
        else heldNavigation.set(code);
        if (!wasActive) return consumed;
        if ((input == Input::KeyDown && code == 27 && !repeat) ||
            (input == Input::KeyUp && code == 'V' && heldToWatch)) {
            Close();
        } else if (platform == Platform::PC && input == Input::KeyDown && !repeat && code >= '1' && code <= '6') {
            Select(ActionKind::Seat, static_cast<unsigned>(code - '0'));
        }
        return true;
    }

    bool Pointer(Input input, int pointer, int x, int y, bool primary = true) {
        const bool wasActive = Active();
        const bool consumed = fence.Consume(input, pointer);
        if (!consumed || pointer < 0 || pointer >= 64) return consumed;
        auto& press = presses[pointer];
        if (input == Input::PointerDown) {
            press = {};
            if (wasActive && primary) press = {ControlAt(x, y), selectionTicket, true};
        } else if (input == Input::PointerUp) {
            const auto saved = press;
            press = {};
            if (wasActive && primary && saved.down && saved.ticket == selectionTicket &&
                saved.kind != ActionKind::None && saved.kind == ControlAt(x, y)) {
                if (saved.kind == ActionKind::Exit) Close();
                else Select(ActionKind::Next, 0);
            }
        }
        return true;
    }

    bool Other(Input input) {
        if (input == Input::Cancel) { presses = {}; heldNavigation.reset(); }
        return fence.Consume(input);
    }
    void Select(ActionKind kind, unsigned seat) {
        if (!Active() || (kind != ActionKind::Next && kind != ActionKind::Seat) ||
            (kind == ActionKind::Seat && (seat < 1 || seat > 6))) return;
        previousSubscription = identity.subscription ? identity.subscription : previousSubscription;
        timeline.End(EndReason::ContextChanged);
        legacySequence = 0;
        mode = ViewMode::Selecting;
        selectionAt = lastNow;
        ++selectionTicket;
        if (!selectionTicket) ++selectionTicket;
        presses = {};
        action = {kind, seat, selectionTicket, identity, EndReason::ContextChanged};
    }

    bool Active() const { return mode != ViewMode::Closed; }
    bool HasPicture() const { return mode == ViewMode::NormalV2 ? timeline.GetSequence() != 0 :
        mode == ViewMode::LegacyV1 && legacySequence != 0; }
    ViewMode Mode() const { return mode; }
    Status DisplayStatus() const { return !Active() ? Status::Closed : HasPicture() ? Status::Showing : Status::Waiting; }
    Platform GetPlatform() const { return platform; }
    bool HoldToWatch() const { return heldToWatch; }
    EndReason GetEndReason() const { return ended; }
    const Identity& Target() const { return identity; }
    std::uint64_t LegacySequence() const { return legacySequence; }
    const Timeline<Frame, Renderer>& Buffer() const { return timeline; }
    Action TakeAction() { return std::exchange(action, Action{}); }

private:
    bool Adopt(const Access& access, const Capabilities& caps, const Identity& target, std::uint64_t now) {
        const auto selected = caps.Select();
        if (!access.Allows(target) || selected == ViewMode::Closed || target.subscription == previousSubscription ||
            (Active() && (access.viewerRoom != viewerRoom || access.viewer != viewerPlayer))) return false;
        if (!timeline.Begin(target, now)) return false;
        identity = target;
        viewerRoom = access.viewerRoom;
        viewerPlayer = access.viewer;
        mode = selected;
        lastNow = lastLegacy = now;
        legacySequence = 0;
        ended = EndReason::None;
        return true;
    }
    void SyncEnd() {
        if (mode == ViewMode::NormalV2 && timeline.GetStatus() == Status::Closed) Close(timeline.GetEndReason());
    }
    struct Press { ActionKind kind = ActionKind::None; std::uint64_t ticket = 0; bool down = false; };
    Timeline<Frame, Renderer> timeline;
    Platform platform;
    InputFence fence;
    std::bitset<256> heldNavigation;
    std::array<Press, 64> presses{};
    Identity identity;
    ViewMode mode = ViewMode::Closed;
    EndReason ended = EndReason::None;
    std::uint64_t previousSubscription = 0, selectionTicket = 0, selectionAt = 0;
    std::uint64_t viewerRoom = 0, viewerPlayer = 0;
    std::uint64_t legacySequence = 0, lastLegacy = 0, lastNow = 0;
    bool heldToWatch = false, textTail = false;
    Action action;
};

}
