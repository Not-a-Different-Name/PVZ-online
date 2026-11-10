#pragma once
#include "NetLink.h"
#include <chrono>
#include <vector>

// @pvz-online: browsing uses a short independent connection so it cannot replace a live team.
namespace RoomDirectory {
constexpr uint16_t PUBLISH=0xF01A, LIST=0xF01B, JOIN=0xF01C, PAGE=0xF00C, ACK=0xF00D;
constexpr int PAGE_SIZE=6;
constexpr int LEGACY_ROOM_LIMIT=128, ROOM_LIMIT=256;
struct Entry {
    std::string code,host;
    uint16_t build=0;
    uint8_t players=0,flags=0,viewers=0;
    bool Playable() const { return !(flags&7) && build==NetProto::MOD_BUILD && players<NetProto::MAX_PLAYERS; }
    bool Watchable() const { return (flags&8) && build==NetProto::MOD_BUILD && viewers<4; }
    bool Joinable() const { return Playable() || Watchable(); }
};
inline bool Decode(const uint8_t* p,int size,uint16_t expected,std::vector<Entry>& entries,int& next) {
    if(size<8)return false;
    NetProto::Reader r(p,size);
    const int version=r.U16();
    if((version!=1 && version!=2 && version!=4) || r.U8()!=0 || r.U16()!=expected)return false;
    const int limit=version==1?LEGACY_ROOM_LIMIT:ROOM_LIMIT;
    const int n=r.U16(),count=r.U8();
    if(expected>=limit || expected%PAGE_SIZE || count>PAGE_SIZE || expected+count>limit || size!=8+count*(version==4?25:24) ||
       (n!=65535 && (count!=PAGE_SIZE || n!=expected+PAGE_SIZE || n>=limit)))return false;
    std::vector<Entry> decoded;
    for(int i=0;i<count;++i) {
        Entry e;char code[5]{},name[17]{};
        for(int j=0;j<4;++j)code[j]=char(r.U8());
        for(int j=0;j<16;++j)name[j]=char(r.U8());
        e.code=code;e.host=name;e.build=r.U16();e.players=r.U8();e.flags=r.U8();if(version==4)e.viewers=r.U8();
        if(e.code.size()!=4 || e.players<1 || e.players>6 || e.flags>(version==4?15:3) || e.viewers>4)return false;
        for(char c:e.code)if(std::string("ABCDEFGHJKLMNPQRSTUVWXYZ23456789").find(c)==std::string::npos)return false;
        // Names arrive from other players; never render control bytes as UI commands.
        for(char& c:e.host)if(static_cast<unsigned char>(c)<32 || c==127)c=' ';
        decoded.push_back(e);
    }
    entries.swap(decoded);next=n==65535?-1:n;return true;
}
class Client {
    NetLink link;
    std::chrono::steady_clock::time_point began{},last{};
    bool sent=false;
    uint16_t offset=0,version=4,serverPort=NetProto::DEFAULT_RELAY_PORT;
    std::string serverHost;
    bool RetryLegacy() {
        if(version==1 || (version==2 && offset>=LEGACY_ROOM_LIMIT) || std::chrono::steady_clock::now()-began>=std::chrono::seconds(5))return false;
        link.Close();version=version==4?2:1;sent=false;
        if(!link.Connect(serverHost.c_str(),serverPort))state=UNAVAILABLE;
        return true;
    }
public:
    enum State { IDLE,LOADING,READY,UNAVAILABLE } state=IDLE;
    std::vector<Entry> entries;
    int next=-1;
    bool CanRefresh() const { return state!=LOADING && std::chrono::steady_clock::now()-last>=std::chrono::seconds(2); }
    bool Refresh(const std::string& server,int page=0,uint16_t port=NetProto::DEFAULT_RELAY_PORT) {
        if(!CanRefresh() || page<0 || page>=ROOM_LIMIT || page%PAGE_SIZE)return false;
        if(serverHost!=server || serverPort!=port)version=4;
        if(version==1 && page>=LEGACY_ROOM_LIMIT)return false;
        serverHost=server;serverPort=port;
        link.Close();entries.clear();next=-1;offset=uint16_t(page);sent=false;
        began=last=std::chrono::steady_clock::now();state=LOADING;
        if(!link.Connect(server.c_str(),port))state=UNAVAILABLE;
        return true;
    }
    void Close() { link.Close();if(state==LOADING)state=IDLE; }
    void Update() {
        if(state!=LOADING)return;
        if(link.IsConnected()&&!sent) {
            uint8_t frame[10]{};NetProto::Writer w(frame,sizeof(frame));
            w.U16(LIST);w.U16(6);w.U16(version);w.U16(NetProto::MOD_BUILD);w.U16(offset);
            if(!link.Send(frame,10)){if(RetryLegacy())return;state=UNAVAILABLE;link.Close();return;}sent=true;
        }
        NetLink::Packet packet;
        while(link.Poll(packet)) {
            if(packet.mSize<4)continue;
            NetProto::Reader r(packet.mData,packet.mSize);int type=r.U16(),length=r.U16();
            if(type==PAGE && length==packet.mSize-4 && length>=2 &&
               (packet.mData[4]|(packet.mData[5]<<8))==version && Decode(packet.mData+4,length,offset,entries,next)) {
                state=READY;link.Close();return;
            }
            state=UNAVAILABLE;link.Close();return;
        }
        if(link.GetState()==NetLink::State::FAILED) {
            if(RetryLegacy())return;
            state=UNAVAILABLE;link.Close();
        } else if(std::chrono::steady_clock::now()-began>std::chrono::seconds(5)) {
            state=UNAVAILABLE;link.Close();
        }
    }
};
}
