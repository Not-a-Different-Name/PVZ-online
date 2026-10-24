#pragma once
#include "AdmissionState.h"
#include <array>
#include <limits>

// Domain state is independent of Board/Zombie's legacy save layout.
namespace TeamProgression {
constexpr uint64_t PLANTS=(uint64_t(1)<<49)-1;
enum Buff : uint8_t { ARSENAL=0, RELIEF=1, FIRE=2, BUFFS=3 };
enum Battle : uint8_t { PREPARING=0, FIGHTING=1, CLEARED=2, FAILED=3 };
enum Transfer : uint8_t { EMPTY=0, RESERVED, ACCEPTED, COMMITTED, EXECUTED, REJECTED };
struct Member {
    uint64_t player=0,plants=0;
    uint32_t level=0,attempt=0;
    uint8_t battle=PREPARING,rows=5,title=0;
    bool ready=false,allow=false,rewardsDone=false;
};
struct Ballot {
    int level=-1;
    uint32_t revision=0;
    bool open=false;
    std::array<uint8_t,3> choices{{255,255,255}};
    std::array<uint64_t,6> players{};
    std::array<int8_t,6> votes{{-2,-2,-2,-2,-2,-2}};
    int Count()const {int n=0;for(auto c:choices)if(c<BUFFS)++n;return n;}
    bool Vote(uint64_t player,int choice) {
        if(!open || choice < -1 || choice>=Count())return false;
        for(size_t i=0;i<players.size();++i)if(players[i]==player && player && votes[i]==-2){votes[i]=int8_t(choice);return true;}
        return false;
    }
    bool Complete()const {for(size_t i=0;i<players.size();++i)if(players[i]&&votes[i]==-2)return false;return true;}
    int Winner(uint32_t random)const {
        int scores[3]{},best=-1,n=0,ids[3]{};
        for(auto v:votes)if(v>=0&&v<Count())++scores[v];
        for(int i=0;i<Count();++i) {
            if(scores[i]>best){best=scores[i];n=0;}
            if(scores[i]==best)ids[n++]=i;
        }
        return n?choices[ids[random%uint32_t(n)]]:-1;
    }
};
struct Strike {
    uint64_t id=0,caster=0,target=0;
    uint32_t level=0,attempt=0;
    uint16_t bursts=0;
    uint8_t phase=EMPTY,row=0,column=0;
    int cost=0,radius=115,damage=1800,scorch=0,scorchMultiplier=1000;
    uint64_t allDamage=0,bossDamage=0;
    bool Valid()const {
        return phase<=REJECTED && (!phase || (id&&caster&&target&&caster!=target&&level<60000&&attempt&&
            bursts>=1&&bursts<=255&&row<6&&column<9&&cost>=0&&radius>=1&&radius<=1000000&&
            damage>=1&&scorch>=0&&scorch<=65535&&scorchMultiplier>=1&&bossDamage<=allDamage));
    }
};
struct Receipt {uint64_t player=0,sequence=0;Strike strike;};
struct Gift {
    uint64_t peer=0;
    uint32_t id=0,run=0,level=0,attempt=0,request=0;
    uint8_t phase=EMPTY,actual=0;
    bool proactive=false;
    bool Valid()const{return phase<=REJECTED&&actual<=90&&(!phase||(peer&&id&&level<60000&&attempt));}
};
struct Shared {
    uint64_t run=0,authority=0,plants=0;
    uint32_t term=0,revision=0;
    std::array<uint8_t,BUFFS> stacks{};
    int votedLevel=-1,reliefLevel=-1;
    uint32_t reliefAttempt=0;
    std::array<uint64_t,6> reliefPlayers{};
    Ballot ballot;
    bool Has(int plant)const{return plant>=0&&plant<49&&stacks[ARSENAL]&&((plants>>plant)&1);}
    bool Due(int level,int scene,int previousScene,bool endless)const {
        return level>=0&&level<60000&&votedLevel!=level&&(level==0||(endless?level%5==0:scene!=previousScene));
    }
    bool Begin(int level,const std::vector<Member>& members) {
        if(ballot.open || votedLevel==level)return false;
        Ballot b;b.level=level;b.revision=++revision;
        bool useful=false;uint64_t combined=plants;
        for(const auto&m:members)combined|=m.plants;
        for(const auto&m:members)if((combined&~m.plants&PLANTS)!=0)useful=true;
        int count=0;
        if(!stacks[ARSENAL]&&useful)b.choices[count++]=ARSENAL;
        if(stacks[RELIEF]<255)b.choices[count++]=RELIEF;
        if(stacks[FIRE]<255)b.choices[count++]=FIRE;
        for(size_t i=0;i<members.size()&&i<6;++i)b.players[i]=members[i].player;
        b.open=count>0;ballot=b;
        if(!count)votedLevel=level;
        return true;
    }
    bool Resolve(uint32_t random) {
        if(!ballot.open)return false;const int picked=ballot.Winner(random);
        if(picked>=0 && stacks[picked]<(picked==ARSENAL?1:255))++stacks[picked];
        votedLevel=ballot.level;ballot.open=false;++revision;return true;
    }
};
struct State {
    Shared shared;
    uint64_t player=0,serial=0;
    int localLevel=-1,reliefReceivedLevel=-1;
    uint32_t attempt=0,reliefReceivedAttempt=0;
    bool allow=false;
    int cooldown=0,baselineSun=0,baselineCooldown=0;
    std::vector<unsigned char> baselinePackets;
    Strike outgoing;
    std::vector<Receipt> receipts;
    std::vector<unsigned char> lawn;
    uint64_t giftSun=0,supportCount=0,sharedUsed=0,speciesWon=0;
    uint32_t endlessCleared=0;
    uint32_t coopSerial=0;
    int giftCooldown=0;
    Gift gift;
    std::vector<Gift> giftReceipts;
    Receipt* FindReceipt(uint64_t id) {
        for(auto&r:receipts)if(r.player==id&&id)return &r;
        if(id&&receipts.size()<Admission::MAX_PLAYERS_HISTORY){receipts.push_back(Receipt{});receipts.back().player=id;return &receipts.back();}
        return nullptr;
    }
};
struct Writer {
    std::vector<unsigned char> data;
    void Put(uint64_t n,int size=4){Admission::Put(data,n,size);}
    void Blob(const std::vector<unsigned char>& b){Put(b.size());data.insert(data.end(),b.begin(),b.end());}
};
struct Reader {
    const std::vector<unsigned char>& data;size_t at=0;bool ok=true;
    uint64_t Get(int n=4){if(!ok||size_t(n)>data.size()-at){ok=false;return 0;}const auto*p=data.data()+at;at+=n;return Admission::Get(p,n);}
    bool Blob(std::vector<unsigned char>& b,size_t max){size_t n=Get();if(!ok||n>max||n>data.size()-at){ok=false;return false;}b.assign(data.begin()+at,data.begin()+at+n);at+=n;return true;}
};
inline void WriteStrike(Writer&w,const Strike&s) {
    w.Put(s.id,8);w.Put(s.caster,8);w.Put(s.target,8);w.Put(s.level);w.Put(s.attempt);w.Put(s.bursts,2);
    w.Put(s.phase,1);w.Put(s.row,1);w.Put(s.column,1);w.Put(s.cost);w.Put(s.radius);w.Put(s.damage);w.Put(s.scorch);w.Put(s.scorchMultiplier);
    w.Put(s.allDamage,8);w.Put(s.bossDamage,8);
}
inline bool ReadStrike(Reader&r,Strike&s) {
    s.id=r.Get(8);s.caster=r.Get(8);s.target=r.Get(8);s.level=r.Get();s.attempt=r.Get();s.bursts=uint16_t(r.Get(2));
    s.phase=uint8_t(r.Get(1));s.row=uint8_t(r.Get(1));s.column=uint8_t(r.Get(1));s.cost=int32_t(r.Get());
    s.radius=int32_t(r.Get());s.damage=int32_t(r.Get());s.scorch=int32_t(r.Get());s.scorchMultiplier=int32_t(r.Get());s.allDamage=r.Get(8);s.bossDamage=r.Get(8);
    return r.ok&&s.Valid();
}
inline void WriteGift(Writer&w,const Gift&g){w.Put(g.peer,8);w.Put(g.id);w.Put(g.run);w.Put(g.level);w.Put(g.attempt);w.Put(g.request);w.Put(g.phase,1);w.Put(g.actual,1);w.Put(g.proactive,1);}
inline bool ReadGift(Reader&r,Gift&g){g.peer=r.Get(8);g.id=r.Get();g.run=r.Get();g.level=r.Get();g.attempt=r.Get();g.request=r.Get();g.phase=uint8_t(r.Get(1));g.actual=uint8_t(r.Get(1));auto proactive=r.Get(1);g.proactive=proactive!=0;return r.ok&&proactive<=1&&g.Valid();}
inline void WriteShared(Writer&w,const Shared&s) {
    w.Put(s.run,8);w.Put(s.authority,8);w.Put(s.plants,8);w.Put(s.term);w.Put(s.revision);
    for(auto v:s.stacks)w.Put(v,1);w.Put(s.votedLevel);w.Put(s.reliefLevel);w.Put(s.reliefAttempt);
    for(auto p:s.reliefPlayers)w.Put(p,8);
    const auto&b=s.ballot;w.Put(b.level);w.Put(b.revision);w.Put(b.open,1);
    for(auto c:b.choices)w.Put(c,1);for(auto p:b.players)w.Put(p,8);for(auto v:b.votes)w.Put(uint8_t(v),1);
}
inline bool ReadShared(Reader&r,Shared&s) {
    s.run=r.Get(8);s.authority=r.Get(8);s.plants=r.Get(8);s.term=r.Get();s.revision=r.Get();
    for(auto&v:s.stacks)v=uint8_t(r.Get(1));s.votedLevel=int32_t(r.Get());s.reliefLevel=int32_t(r.Get());s.reliefAttempt=r.Get();
    for(auto&p:s.reliefPlayers)p=r.Get(8);
    auto&b=s.ballot;b.level=int32_t(r.Get());b.revision=r.Get();auto open=r.Get(1);b.open=open!=0;
    for(auto&c:b.choices)c=uint8_t(r.Get(1));for(auto&p:b.players)p=r.Get(8);for(auto&v:b.votes)v=int8_t(r.Get(1));
    if(!r.ok||open>1||(s.plants&~PLANTS)||s.stacks[ARSENAL]>1||s.votedLevel < -1||s.votedLevel>=60000||
        s.reliefLevel < -1||s.reliefLevel>=60000||b.level < -1||b.level>=60000)return false;
    bool gap=false;for(size_t i=0;i<3;++i){if(b.choices[i]==255)gap=true;else if(gap||b.choices[i]>=BUFFS)return false;for(size_t j=0;j<i;++j)if(b.choices[i]<BUFFS&&b.choices[i]==b.choices[j])return false;}
    for(size_t i=0;i<6;++i){if(b.votes[i] < -2||b.votes[i]>=b.Count())return false;for(size_t j=0;j<i;++j)if(b.players[i]&&b.players[i]==b.players[j])return false;}
    return !b.open||(s.run&&s.authority&&b.Count()&&b.level>=0);
}
inline std::vector<unsigned char> Encode(const State&s) {
    Writer w;w.Put(0x31504d54);WriteShared(w,s.shared);w.Put(s.player,8);w.Put(s.serial,8);
    w.Put(s.localLevel);w.Put(s.attempt);w.Put(s.reliefReceivedLevel);w.Put(s.reliefReceivedAttempt);w.Put(s.allow,1);
    w.Put(s.cooldown);w.Put(s.baselineSun);w.Put(s.baselineCooldown);w.Blob(s.baselinePackets);WriteStrike(w,s.outgoing);
    w.Put(s.receipts.size(),2);
    for(const auto&x:s.receipts){w.Put(x.player,8);w.Put(x.sequence,8);WriteStrike(w,x.strike);}
    w.Put(s.giftSun,8);w.Put(s.supportCount,8);w.Put(s.sharedUsed,8);w.Put(s.speciesWon,8);w.Put(s.endlessCleared);
    w.Put(s.coopSerial);w.Put(s.giftCooldown);WriteGift(w,s.gift);w.Put(s.giftReceipts.size(),2);for(const auto&g:s.giftReceipts)WriteGift(w,g);
    w.Blob(s.lawn);return w.data;
}
inline bool Decode(const std::vector<unsigned char>&bytes,State&out) {
    Reader r{bytes};State s;if(r.Get()!=0x31504d54||!ReadShared(r,s.shared))return false;
    s.player=r.Get(8);s.serial=r.Get(8);s.localLevel=int32_t(r.Get());s.attempt=r.Get();s.reliefReceivedLevel=int32_t(r.Get());s.reliefReceivedAttempt=r.Get();
    auto allow=r.Get(1);s.allow=allow!=0;s.cooldown=int32_t(r.Get());s.baselineSun=int32_t(r.Get());s.baselineCooldown=int32_t(r.Get());
    if(!r.Blob(s.baselinePackets,4096)||s.baselinePackets.size()%14||!ReadStrike(r,s.outgoing))return false;
    const auto count=r.Get(2);if(count>Admission::MAX_PLAYERS_HISTORY)return false;s.receipts.resize(size_t(count));
    for(auto&x:s.receipts){x.player=r.Get(8);x.sequence=r.Get(8);if(!x.player||!ReadStrike(r,x.strike))return false;}
    s.giftSun=r.Get(8);s.supportCount=r.Get(8);s.sharedUsed=r.Get(8);s.speciesWon=r.Get(8);s.endlessCleared=r.Get();
    s.coopSerial=r.Get();s.giftCooldown=int32_t(r.Get());if(!ReadGift(r,s.gift))return false;
    const auto gifts=r.Get(2);if(gifts>Admission::MAX_PLAYERS_HISTORY)return false;s.giftReceipts.resize(size_t(gifts));
    for(size_t i=0;i<s.giftReceipts.size();++i){if(!ReadGift(r,s.giftReceipts[i])||s.giftReceipts[i].phase<EXECUTED)return false;for(size_t j=0;j<i;++j)if(s.giftReceipts[i].peer==s.giftReceipts[j].peer)return false;}
    if(!r.Blob(s.lawn,24*1024*1024)||!r.ok||r.at!=bytes.size()||allow>1||s.localLevel < -1||s.localLevel>=60000||
       s.reliefReceivedLevel < -1||s.reliefReceivedLevel>=60000||s.giftCooldown<0||s.giftCooldown>300||s.cooldown<0||s.cooldown>2000||s.baselineCooldown<0||s.baselineCooldown>2000||s.baselineSun<0||(s.sharedUsed&~PLANTS)||(s.speciesWon&~uint64_t(127))||s.endlessCleared>60000)return false;
    for(size_t i=0;i<s.receipts.size();++i)for(size_t j=0;j<i;++j)if(s.receipts[i].player==s.receipts[j].player)return false;
    out=std::move(s);return true;
}
}
