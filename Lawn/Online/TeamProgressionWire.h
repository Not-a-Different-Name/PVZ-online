#pragma once
#include "../Run/TeamProgressionState.h"
#include <cstring>
namespace TeamWire {
constexpr uint16_t TYPE=28;
enum Op : uint8_t { REPORT=1, SHARED, VOTE, OFFER, ACCEPT, COMMIT, RESULT, QUERY, CANCEL };
struct Message {
    uint8_t src=0,dst=0,op=REPORT,mode=0;
    uint32_t seed=0,level=0,attempt=0,revision=0;
    uint64_t sender=0,run=0;
    TeamProgression::Member member;
    TeamProgression::Shared shared;
    TeamProgression::Strike strike;
    int choice=-1;
    uint8_t zombieStacks[17]{};
    uint16_t zombieVoted=65535;
    uint8_t zombieRounds=0;
};
inline std::vector<unsigned char> Encode(const Message&m) {
    TeamProgression::Writer w;w.Put(m.src,1);w.Put(m.dst,1);w.Put(m.op,1);w.Put(m.mode,1);
    w.Put(m.seed);w.Put(m.level);w.Put(m.attempt);w.Put(m.revision);w.Put(m.sender,8);w.Put(m.run,8);
    if(m.op==REPORT) {const auto&p=m.member;w.Put(p.plants,8);w.Put(p.battle,1);w.Put(p.ready,1);w.Put(p.allow,1);w.Put(p.rewardsDone,1);w.Put(p.rows,1);w.Put(p.title,1);w.Put(p.hasSupportCard,1);}
    else if(m.op==SHARED){TeamProgression::WriteShared(w,m.shared);for(auto n:m.zombieStacks)w.Put(n,1);w.Put(m.zombieVoted,2);w.Put(m.zombieRounds,1);}
    else if(m.op==VOTE)w.Put(uint8_t(m.choice),1);
    else TeamProgression::WriteStrike(w,m.strike);
    return w.data.size()<=256?w.data:std::vector<unsigned char>{};
}
inline bool Decode(const uint8_t*bytes,int count,Message&out) {
    if(count<36||count>256)return false;std::vector<unsigned char> data(bytes,bytes+count);
    TeamProgression::Reader r{data};Message m;
    m.src=uint8_t(r.Get(1));m.dst=uint8_t(r.Get(1));m.op=uint8_t(r.Get(1));m.mode=uint8_t(r.Get(1));
    m.seed=r.Get();m.level=r.Get();m.attempt=r.Get();m.revision=r.Get();m.sender=r.Get(8);m.run=r.Get(8);
    if(m.op==REPORT) {
        auto&p=m.member;p.player=m.sender;p.level=m.level;p.attempt=m.attempt;p.plants=r.Get(8);p.battle=uint8_t(r.Get(1));
        auto ready=r.Get(1),allow=r.Get(1),rewards=r.Get(1);p.rows=uint8_t(r.Get(1));p.title=uint8_t(r.Get(1));auto card=r.Get(1);p.hasSupportCard=card!=0;if(card>1||(p.plants&~TeamProgression::PLANTS)||p.battle>TeamProgression::FAILED||ready>1||allow>1||rewards>1||p.rows<1||p.rows>6||p.title>7)return false;
        p.ready=ready!=0;p.allow=allow!=0;p.rewardsDone=rewards!=0;
    } else if(m.op==SHARED){if(!TeamProgression::ReadShared(r,m.shared)||m.shared.run!=m.run)return false;
        for(auto&n:m.zombieStacks)n=uint8_t(r.Get(1));m.zombieVoted=uint16_t(r.Get(2));m.zombieRounds=uint8_t(r.Get(1));if(m.zombieRounds>2)return false;}
    else if(m.op==VOTE)m.choice=int8_t(r.Get(1));
    else if(!TeamProgression::ReadStrike(r,m.strike))return false;
    if(!r.ok||r.at!=data.size()||m.op<REPORT||m.op>CANCEL||m.mode>3||!m.sender||m.src<1||m.src>6||m.dst<1||m.dst>6||m.level>=60000||!m.attempt||m.choice < -1||m.choice>2)return false;
    out=m;return true;
}
}
