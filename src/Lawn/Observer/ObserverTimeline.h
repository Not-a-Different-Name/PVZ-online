#pragma once

#include "ObserverState.h"
#include <algorithm>
#include <cstddef>
#include <deque>
#include <limits>
#include <new>

namespace Observer {

struct BufferConfig {
    std::uint64_t delayMs = 1500;
    std::size_t maxFrames = 12;
    std::size_t maxBytes = 8 * 1024 * 1024;
};

struct SourceStamp {
    std::uint64_t sequence = 0;
    std::uint64_t milliseconds = 0;
};

class SourceClock {
public:
    bool Peek(std::uint32_t sequence, std::uint32_t tick, SourceStamp& result) const {
        if (!started) { result = {1, tick}; return true; }
        const auto advance = static_cast<std::uint32_t>(sequence - lastSequence);
        const auto elapsed = static_cast<std::uint32_t>(tick - lastTick);
        if (!advance || advance >= 0x80000000u || elapsed >= 0x80000000u ||
            stamp.sequence > std::numeric_limits<std::uint64_t>::max() - advance ||
            stamp.milliseconds > std::numeric_limits<std::uint64_t>::max() - elapsed) return false;
        result = {stamp.sequence + advance, stamp.milliseconds + elapsed};
        return true;
    }
    bool Commit(std::uint32_t sequence, std::uint32_t tick, const SourceStamp& accepted) {
        SourceStamp expected;
        if (!Peek(sequence, tick, expected) || expected.sequence != accepted.sequence ||
            expected.milliseconds != accepted.milliseconds) return false;
        lastSequence = sequence;
        lastTick = tick;
        stamp = accepted;
        started = true;
        return true;
    }
    void Clear() { *this = SourceClock(); }

private:
    bool started = false;
    std::uint32_t lastSequence = 0, lastTick = 0;
    SourceStamp stamp;
};

template<typename Frame, typename Renderer>
class Timeline {
public:
    static constexpr std::uint64_t TimeoutMs = 3000;
    static constexpr std::uint64_t RetryMs = 100;

    explicit Timeline(Renderer& renderer, BufferConfig config = {}) : renderer(renderer), config(config) {
        this->config.delayMs = std::clamp<std::uint64_t>(config.delayMs, 1000, 2000);
        this->config.maxFrames = std::clamp<std::size_t>(config.maxFrames, 2, 32);
        this->config.maxBytes = std::clamp<std::size_t>(config.maxBytes, 1024, 8 * 1024 * 1024);
    }
    ~Timeline() { renderer.Clear(); }
    Timeline(const Timeline&) = delete;
    Timeline& operator=(const Timeline&) = delete;

    bool Begin(const Identity& next, std::uint64_t now) {
        End(EndReason::ContextChanged);
        if (!next.Valid()) return false;
        identity = next;
        clock = lastTraffic = now;
        status = Status::Waiting;
        reason = EndReason::None;
        return true;
    }

    bool Accept(const Frame& frame, std::uint64_t sourceMs, std::uint64_t now) {
        if (!Advance(now) || paused || !(frame.identity == identity) || !frame.sequence ||
            frame.sequence <= accepted || (anchored && sourceMs < lastSource)) return false;
        if (anchored) {
            const auto sourceElapsed = sourceMs - sourceAnchor;
            const auto localElapsed = now - localAnchor;
            if (sourceElapsed > localElapsed && sourceElapsed - localElapsed > 2000) return false;
        }
        const auto cost = renderer.Bytes(frame);
        if (cost > config.maxBytes / 2 || !renderer.Validate(frame)) return false;
        try {
            Entry next{frame, sourceMs, 0};
            next.bytes = renderer.Bytes(next.frame);
            if (next.bytes > config.maxBytes / 2) return false;
            queue.push_back(std::move(next));
        } catch (const std::bad_alloc&) { return false; }
        bytes += queue.back().bytes;
        while (queue.size() > config.maxFrames || bytes > config.maxBytes) {
            bytes -= queue[1].bytes;
            queue.erase(queue.begin() + 1);
            ++dropped;
        }
        if (!anchored) {
            anchored = true;
            sourceAnchor = sourceMs;
            localAnchor = now;
        }
        lastSource = sourceMs;
        accepted = frame.sequence;
        lastTraffic = now;
        return true;
    }

    bool Tick(std::uint64_t now) {
        if (!Advance(now) || paused || !anchored || queue.empty() || now < retryAt ||
            now - localAnchor < config.delayMs) return false;
        const auto playable = now - localAnchor - config.delayMs;
        std::size_t count = 0;
        while (count < queue.size() && queue[count].sourceMs - sourceAnchor <= playable) ++count;
        if (!count) return false;
        const auto& next = queue[count - 1];
        if (!renderer.Prepare(next.frame)) {
            retryAt = now > std::numeric_limits<std::uint64_t>::max() - RetryMs ? now : now + RetryMs;
            return false;
        }
        visible = next.frame.sequence;
        status = Status::Showing;
        retryAt = 0;
        while (count--) {
            bytes -= queue.front().bytes;
            queue.pop_front();
        }
        return true;
    }

    bool Congestion(const Identity& target, bool stop, std::uint64_t now) {
        if (!Advance(now) || !(target == identity)) return false;
        if (paused != stop) {
            queue.clear(); bytes = 0;
            anchored = false;
            retryAt = 0;
        }
        if (stop || paused != stop) lastTraffic = now;
        paused = stop;
        return true;
    }

    void End(EndReason ended = EndReason::User) {
        renderer.Clear();
        queue.clear(); bytes = 0;
        identity = {};
        clock = lastTraffic = localAnchor = sourceAnchor = lastSource = accepted = visible = retryAt = 0;
        paused = anchored = false;
        status = Status::Closed;
        reason = ended;
    }

    Status GetStatus() const { return status; }
    EndReason GetEndReason() const { return reason; }
    const Identity& GetIdentity() const { return identity; }
    std::uint64_t GetSequence() const { return visible; }
    std::uint64_t AcceptedSequence() const { return accepted; }
    std::size_t Queued() const { return queue.size(); }
    std::size_t QueuedBytes() const { return bytes; }
    std::uint64_t Dropped() const { return dropped; }
    std::uint64_t Delay() const { return config.delayMs; }
    bool Paused() const { return paused; }
    bool Buffering() const { return anchored && visible == 0 && !paused; }

private:
    bool Advance(std::uint64_t now) {
        if (status == Status::Closed || now < clock) return false;
        clock = now;
        if (now - lastTraffic >= TimeoutMs) { End(EndReason::Timeout); return false; }
        return true;
    }
    struct Entry { Frame frame; std::uint64_t sourceMs; std::size_t bytes; };
    Renderer& renderer;
    BufferConfig config;
    std::deque<Entry> queue;
    Identity identity;
    std::size_t bytes = 0;
    std::uint64_t clock = 0, lastTraffic = 0, localAnchor = 0, sourceAnchor = 0, lastSource = 0;
    std::uint64_t accepted = 0, visible = 0, retryAt = 0, dropped = 0;
    bool paused = false, anchored = false;
    Status status = Status::Closed;
    EndReason reason = EndReason::None;
};

}
