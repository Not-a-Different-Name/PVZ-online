#pragma once
#include "RelayNodes.h"
#include "../ModText.h"
#include <array>

namespace Matchmaking {
constexpr uint16_t TYPE=0xF026;
constexpr uint8_t PRESET=1;
inline const char* ModeName(int mode) {
    static const char* zh[]={"完整闯关","标准闯关","快速闯关"};
    static const char* en[]={"Full run","Standard run","Quick run"};
    return ModText::Tr(zh[mode>=0&&mode<3?mode:0],en[mode>=0&&mode<3?mode:0]);
}
// Short independent control connection; never replaces an established team.
class Client {
private:
    RelayNodes::CatalogDiscovery discovery;
    NetLink link;
    RelayNodes::Endpoint coordinator;
    std::chrono::steady_clock::time_point began{},retry{};
    std::string name;
    bool sent=false;
    void Fail(const char* zh,const char* en){link.Close();discovery.Close();state=FAILED;error=ModText::Tr(zh,en);}
    bool Send(const std::vector<uint8_t>& payload){uint8_t bytes[NetProto::MAX_PACKET];NetProto::Writer w(bytes,sizeof(bytes));w.U16(TYPE);w.U16(uint16_t(payload.size()));for(auto b:payload)w.U8(b);return !w.Overflowed()&&link.Send(bytes,w.Size());}
    void Route() {
        const RelayNodes::Node* first=nullptr;
        for(const auto& n:RelayNodes::Catalog())if(n.create&&n.directory&&(!first||std::stoul(n.id)<std::stoul(first->id)))first=&n;
        if(!first||!RelayNodes::CatalogRevision()){Fail("匹配清单暂不可用，请稍后重试。","Matchmaking directory is unavailable. Please retry later.");return;}
        coordinator=first->endpoint;sent=false;state=ROUTING;retry={};
        if(!link.Connect(coordinator.host.c_str(),coordinator.port))Fail("无法连接匹配服务，请重试。","Could not connect to matchmaking. Please retry.");
    }
public:
    enum State {IDLE,DISCOVERING,ROUTING,READY,CANCELLING,FAILED} state=IDLE;
    RelayNodes::Endpoint target;
    std::string code,error;
    std::array<uint8_t,16> ticket{};
    int mode=2;
    bool Start(int value,const std::string& player) {
        if(state!=IDLE&&state!=FAILED)return false;
        if(value<0||value>2)return false;
        mode=value;name=player;error.clear();code.clear();ticket={};link.Close();began=std::chrono::steady_clock::now();sent=false;
        state=DISCOVERING;if(!discovery.Start(true))Route();return state!=FAILED;
    }
    void Cancel() {
        discovery.Close();link.Close();sent=false;
        if(code.empty()){state=IDLE;return;}
        state=CANCELLING;retry=std::chrono::steady_clock::now();
        if(!link.Connect(target.host.c_str(),target.port))state=IDLE;
    }
    void Update() {
        const auto now=std::chrono::steady_clock::now();
        if(state==DISCOVERING){discovery.Update();if(!discovery.loading)Route();return;}
        if(state==CANCELLING) {
            if(link.IsConnected()&&!sent){std::vector<uint8_t> p{1,4};p.insert(p.end(),code.begin(),code.end());p.insert(p.end(),ticket.begin(),ticket.end());sent=Send(p);}
            NetLink::Packet packet;
            if(link.Poll(packet)||link.GetState()==NetLink::State::FAILED||now-retry>std::chrono::seconds(2)){link.Close();state=IDLE;}
            return;
        }
        if(state!=ROUTING)return;
        if(now-began>std::chrono::seconds(10)){Fail("匹配超时，请重试；已有对局不受影响。","Matchmaking timed out. Please retry.");return;}
        if(retry!=std::chrono::steady_clock::time_point{}) {
            if(now<retry)return;retry={};sent=false;
            if(!link.Connect(coordinator.host.c_str(),coordinator.port)){Fail("匹配服务连接失败，请重试。","Matchmaking connection failed. Please retry.");return;}
        }
        if(link.IsConnected()&&!sent) {
            uint8_t p[29]{};NetProto::Writer w(p,sizeof(p));w.U8(1);w.U8(1);w.U16(NetProto::MOD_BUILD);w.U8(uint8_t(mode));
            const uint64_t revision=RelayNodes::CatalogRevision();w.U32(uint32_t(revision));w.U32(uint32_t(revision>>32));
            char fixed[NetProto::NAME_SIZE];NetProto::SetName(fixed,name.c_str());for(char c:fixed)w.U8(uint8_t(c));
            if(!Send(std::vector<uint8_t>(p,p+sizeof(p)))){Fail("匹配请求未完成，请重试。","Matchmaking request failed. Please retry.");return;}sent=true;
        }
        NetLink::Packet packet;
        while(link.Poll(packet)) {
            const auto* p=packet.mData;
            if(packet.mSize<7||p[0]!=uint8_t(TYPE)||p[1]!=uint8_t(TYPE>>8)||p[2]!=packet.mSize-4||p[3]||p[4]!=1||p[5]!=1){Fail("匹配服务返回异常，请重试。","Invalid matchmaking reply. Please retry.");return;}
            if(p[6]==2 && packet.mSize==7){link.Close();sent=false;retry=now+std::chrono::milliseconds(250+int((now-began).count()%250));return;}
            if(p[6]!=0){Fail("匹配服务暂不可用，可能正在更新或部分节点离线，请稍后重试。","Matchmaking is unavailable. A node may be updating or offline. Please retry later.");return;}
            if(packet.mSize!=30||p[7]!=mode){Fail("匹配房间信息异常，请重试。","Invalid matching room. Please retry.");return;}
            const auto id=std::to_string(unsigned(p[8])|(unsigned(p[9])<<8));const auto* node=RelayNodes::Find(id);
            if(!node||!node->create||!node->directory){Fail("匹配目标不在服务器清单中，请重试。","Matchmaking destination is unavailable. Please retry.");return;}
            target=node->endpoint;code.assign(reinterpret_cast<const char*>(p+10),4);
            for(char c:code)if(std::string("ABCDEFGHJKLMNPQRSTUVWXYZ23456789").find(c)==std::string::npos){Fail("匹配房间码异常，请重试。","Invalid matching room code. Please retry.");return;}
            std::copy(p+14,p+30,ticket.begin());state=READY;link.Close();return;
        }
        if(link.GetState()==NetLink::State::FAILED)Fail("匹配连接中断，或服务器尚未支持匹配，请稍后重试。","Connection lost or matchmaking is not supported yet. Please retry later.");
    }
};
}
