#pragma once
#include "RoomInvite.h"
#include <memory>
#include <set>

namespace RoomInvite {
struct LookupReply { bool found = false; std::uint16_t build = 0; std::uint8_t players = 0, flags = 0; };
inline bool ReadReply(const NetLink::Packet& packet, const std::string& code, LookupReply& output) {
    if (packet.mSize != 15) return false;
    NetProto::Reader reader(packet.mData, packet.mSize);
    if (reader.U16() != RoomDirectory::PAGE || reader.U16() != 11 || reader.U16() != 3) return false;
    const auto found = reader.U8(); std::string echo;
    for (int index = 0; index < 4; ++index) echo += char(reader.U8());
    LookupReply result; result.found = found == 1; result.build = reader.U16(); result.players = reader.U8(); result.flags = reader.U8();
    if (reader.Overflowed() || found > 1 || echo != code || (result.flags & ~15) ||
        (result.found ? !result.build || result.players < 1 || result.players > 6 : result.build || result.players || result.flags)) return false;
    output = result; return true;
}
class UniqueMatch {
public:
    enum Result { Pending, Unique, Missing, Ambiguous, Incomplete };
    explicit UniqueMatch(std::size_t count) : expected(count) {}
    void Add(const RelayNodes::Endpoint& endpoint, bool valid, bool found) {
        if (!seen.insert(endpoint.Text()).second) return;
        ++completed; if (!valid) failed = true;
        if (valid && found) { ++matches; target = endpoint; }
    }
    Result State() const {
        if (completed < expected) return Pending;
        if (!expected || failed) return Incomplete;
        return matches > 1 ? Ambiguous : matches ? Unique : Missing;
    }
    RelayNodes::Endpoint target;
private:
    std::size_t expected = 0, completed = 0, matches = 0;
    bool failed = false;
    std::set<std::string> seen;
};
class AutoRoomLookup {
    struct Query {
        RelayNodes::Endpoint endpoint;
        NetLink link;
        std::chrono::steady_clock::time_point began;
        bool sent = false, done = false;
    };
    RelayNodes::CatalogDiscovery discovery;
    std::vector<RelayNodes::Endpoint> endpoints;
    std::vector<std::unique_ptr<Query>> queries;
    std::unique_ptr<UniqueMatch> matches;
    std::chrono::steady_clock::time_point began;
    std::size_t next = 0;
    Intent intent;
    bool discovering = false;
    std::vector<RelayNodes::Node> fixtureNodes;
    void Begin() {
        const auto& nodes=fixtureNodes.empty()?RelayNodes::Catalog():fixtureNodes;
        if (intent.Routed()) {
            const RelayNodes::Node* node=nullptr;for(const auto& entry:nodes)if(entry.id==intent.node)node=&entry;
            if (!node) { state = INCOMPLETE; loading = false; return; }
            target = node->endpoint; state = READY; loading = false; return;
        }
        for (const auto& node : nodes) {
            if (std::none_of(endpoints.begin(), endpoints.end(), [&](const auto& endpoint) { return RelayNodes::Same(endpoint, node.endpoint); }))
                endpoints.push_back(node.endpoint);
        }
        matches = std::make_unique<UniqueMatch>(endpoints.size());
    }
public:
    enum State { IDLE, LOADING, READY, INVALID, MISSING, AMBIGUOUS, INCOMPLETE } state = IDLE;
    bool loading = false;
    RelayNodes::Endpoint target;
    std::string code;
    void Close() { discovery.Close(); queries.clear(); endpoints.clear(); matches.reset(); fixtureNodes.clear(); next = 0; discovering = loading = false; state = IDLE; }
#ifdef D002_ROOM_TESTS
    bool StartLocalFixture(const std::string& text,const std::vector<RelayNodes::Node>& nodes) {
        Close();for(const auto& node:nodes)if(node.endpoint.host!="127.0.0.1")return false;
        if(nodes.empty()||!Parse(text,intent))return false;
        fixtureNodes=nodes;code=intent.code;state=LOADING;loading=true;began=std::chrono::steady_clock::now();Begin();return true;
    }
#endif
    bool Start(const std::string& text) {
        Close();
        if (!Parse(text, intent)) { state = INVALID; return false; }
        code = intent.code; target = {};
        state = LOADING; loading = true; began = std::chrono::steady_clock::now();
        if (intent.Routed() && RelayNodes::Find(intent.node)) Begin();
        else { discovering = discovery.Start(true); if (!discovering) Begin(); }
        return true;
    }
    void Update() {
        if (!loading) return;
        const auto now = std::chrono::steady_clock::now();
        if (now - began >= std::chrono::seconds(15)) { queries.clear(); discovery.Close(); state = INCOMPLETE; loading = false; return; }
        if (discovering) { discovery.Update(); if (discovery.loading) return; discovering = false; Begin(); if (!loading) return; }
        if (!matches) { state = INCOMPLETE; loading = false; return; }
        std::size_t active = 0;
        for (auto& query : queries) if (!query->done) {
            if (query->link.IsConnected() && !query->sent) {
                std::uint8_t bytes[12]{}; NetProto::Writer writer(bytes, sizeof(bytes));
                writer.U16(RoomDirectory::LIST); writer.U16(8); writer.U16(3); writer.U16(NetProto::MOD_BUILD);
                for (char letter : code) writer.U8(std::uint8_t(letter));
                query->sent = query->link.Send(bytes, sizeof(bytes));
            }
            NetLink::Packet packet;
            if (query->link.Poll(packet)) {
                LookupReply reply; const bool valid = ReadReply(packet, code, reply);
                matches->Add(query->endpoint, valid, reply.found); query->done = true; query->link.Close();
            } else if (query->link.GetState() == NetLink::State::FAILED || now - query->began >= std::chrono::milliseconds(2500)) {
                matches->Add(query->endpoint, false, false); query->done = true; query->link.Close();
            }
            if (!query->done) ++active;
        }
        while (active < 4 && next < endpoints.size()) {
            auto query = std::make_unique<Query>(); query->endpoint = endpoints[next++]; query->began = now;
            if (!query->link.Connect(query->endpoint.host.c_str(), query->endpoint.port)) {
                matches->Add(query->endpoint, false, false); query->done = true;
            } else ++active;
            queries.push_back(std::move(query));
        }
        const auto result = matches->State();
        if (result == UniqueMatch::Pending) return;
        target = matches->target;
        state = result == UniqueMatch::Unique ? READY : result == UniqueMatch::Missing ? MISSING :
            result == UniqueMatch::Ambiguous ? AMBIGUOUS : INCOMPLETE;
        loading = false; queries.clear();
    }
};
}
