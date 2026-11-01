#pragma once
#include "AdmissionState.h"
#include <array>
#include <limits>
#include <algorithm>

// Domain state is independent of Board/Zombie's legacy save layout.
namespace TeamProgression {
constexpr uint64_t PLANTS=(uint64_t(1)<<49)-1;
enum Buff : uint8_t { ARSENAL=0, RELIEF=1, FIRE=2, SUPPLY=3, FORTRESS=4, READINESS=5, RELAY=6, RESCUE=7, REPAIR=8, BORROW=9, GROUP_SUN=10, ICE_AID=11, FIRE_AID=12, ARMOR_AID=13, BUFFS=14 };
inline int MaxStacks(int id){return id==ARSENAL||id>=RESCUE?1:3;}
inline int ReliefSun(int stacks){return 200*std::min(3,stacks);}
inline int RelaySun(int stacks){return stacks?125+25*std::min(3,stacks):90;}
enum Battle : uint8_t { PREPARING=0, FIGHTING=1, CLEARED=2, FAILED=3 };
enum Transfer : uint8_t { EMPTY=0, RESERVED, ACCEPTED, COMMITTED, EXECUTED, REJECTED, DELIVERED };
struct Member {
    uint64_t player=0,plants=0;
    uint32_t level=0,attempt=0;
    uint8_t battle=PREPARING,rows=5,title=0;
    bool ready=false,allow=false,rewardsDone=false,hasSupportCard=false;
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
struct SupportBuff { uint16_t id=0,count=0; };
struct Strike {
    uint64_t id=0,caster=0,target=0;
    uint32_t level=0,attempt=0;
    uint16_t bursts=0;
    uint8_t phase=EMPTY,row=0,column=0;
    int cost=0,radius=115,damage=1800,scorch=0,scorchMultiplier=1000;
    uint64_t allDamage=0,bossDamage=0;
    uint8_t seed=2; // Cherry / Ice-shroom / Jalapeno / Pumpkin.
    std::vector<SupportBuff> buffs;
    bool Valid()const {
        return (seed==2||seed==14||seed==20||seed==30)&&buffs.size()<=16&&phase<=DELIVERED && (!phase || (id&&caster&&target&&caster!=target&&level<60000&&attempt&&
            bursts>=1&&bursts<=255&&row<6&&column<9&&cost>=0&&radius>=1&&radius<=1000000&&
            damage>=1&&scorch>=0&&scorch<=65535&&scorchMultiplier>=1&&bossDamage<=allDamage));
    }
};
struct AidFreeze {uint32_t zombie=0;bool armor=false;};
struct AidPlant { uint32_t plant=0; Strike strike; };
struct Receipt {uint64_t player=0,sequence=0;Strike strike;};
struct Gift {
    uint64_t peer=0;
    uint32_t id=0,run=0,level=0,attempt=0,request=0;
    uint8_t phase=EMPTY,actual=0;
    bool proactive=false;
    bool Valid()const{return phase<=REJECTED&&actual<=200&&(!phase||(peer&&id&&level<60000&&attempt));}
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
    bool Begin(int level,const std::vector<Member>& members,bool endless=false,uint32_t random=0) {
        if(ballot.open || votedLevel==level)return false;
        Ballot b;b.level=level;b.revision=++revision;
        bool useful=false;uint64_t combined=plants;
        for(const auto&m:members)combined|=m.plants;
        for(const auto&m:members)if((combined&~m.plants&PLANTS)!=0)useful=true;
        std::vector<uint8_t> candidates;
        for(int id=0;id<BUFFS;++id)if(stacks[id]<MaxStacks(id)&&
            (id!=ARSENAL||(!endless&&useful))&&
            (members.size()>1||(id!=ARSENAL&&id!=RELIEF&&id!=FIRE&&id!=RELAY&&id!=RESCUE&&id<REPAIR)))candidates.push_back(uint8_t(id));
        // Select without replacement using a private RNG, never the battle RNG.
        auto next=[&](){random=random*1664525u+1013904223u;return random;};
        for(size_t i=candidates.size();i>1;--i)std::swap(candidates[i-1],candidates[next()%i]);
        auto fresh=std::find_if(candidates.begin(),candidates.end(),[&](int id){return stacks[id]==0;});
        if(fresh!=candidates.end())std::iter_swap(candidates.begin(),fresh);
        int count=std::min(3,int(candidates.size()));
        for(int i=0;i<count;++i)b.choices[i]=candidates[i];
        for(size_t i=0;i<members.size()&&i<6;++i)b.players[i]=members[i].player;
        b.open=count>0;ballot=b;
        if(!count)votedLevel=level;
        return true;
    }
    bool Resolve(uint32_t random) {
        if(!ballot.open)return false;const int picked=ballot.Winner(random);
        if(picked>=0 && stacks[picked]<MaxStacks(picked))++stacks[picked];
        votedLevel=ballot.level;ballot.open=false;++revision;return true;
    }
};
struct State {
    Shared shared;
    uint64_t player=0,serial=0;
    int localLevel=-1,reliefReceivedLevel=-1;
    uint32_t attempt=0,reliefReceivedAttempt=0;
    bool allow=true;
    int cooldown=0,baselineSun=0,baselineCooldown=0;
    std::vector<unsigned char> baselinePackets;
    uint32_t supportCoin=0;
    int suppliedLevel=-1,rescuedLevel=-1,restrictedSun=0;
    Strike outgoing;
    std::vector<Strike> sentCards;
    std::vector<Receipt> receipts;
    std::vector<unsigned char> lawn;
    uint64_t giftSun=0,supportCount=0,sharedUsed=0,speciesWon=0;
    uint32_t endlessCleared=0;
    uint32_t coopSerial=0;
    int giftCooldown=0;
    Gift gift;
    std::vector<Gift> giftReceipts;
    std::vector<Gift> groupGifts;
    int repairCooldown=0;
    std::vector<AidPlant> aidPlants;
    std::vector<AidFreeze> aidFreezes;
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
inline void WriteStrike(Writer&w,const Strike&s,bool extended=true) {
    w.Put(s.id,8);w.Put(s.caster,8);w.Put(s.target,8);w.Put(s.level);w.Put(s.attempt);w.Put(s.bursts,2);
    w.Put(s.phase,1);w.Put(s.row,1);w.Put(s.column,1);w.Put(s.cost);w.Put(s.radius);w.Put(s.damage);w.Put(s.scorch);w.Put(s.scorchMultiplier);
    w.Put(s.allDamage,8);w.Put(s.bossDamage,8);
    if(extended){w.Put(s.seed,1);w.Put(s.buffs.size(),1);for(const auto&b:s.buffs){w.Put(b.id,2);w.Put(b.count,2);}}
}
inline bool ReadStrike(Reader&r,Strike&s,bool extended=true) {
    s.id=r.Get(8);s.caster=r.Get(8);s.target=r.Get(8);s.level=r.Get();s.attempt=r.Get();s.bursts=uint16_t(r.Get(2));
    s.phase=uint8_t(r.Get(1));s.row=uint8_t(r.Get(1));s.column=uint8_t(r.Get(1));s.cost=int32_t(r.Get());
    s.radius=int32_t(r.Get());s.damage=int32_t(r.Get());s.scorch=int32_t(r.Get());s.scorchMultiplier=int32_t(r.Get());s.allDamage=r.Get(8);s.bossDamage=r.Get(8);
    if(extended){s.seed=uint8_t(r.Get(1));auto n=r.Get(1);if(n>16)return false;for(size_t i=0;i<n;++i){SupportBuff b{uint16_t(r.Get(2)),uint16_t(r.Get(2))};if(!b.count||b.id>=1024)return false;for(const auto&old:s.buffs)if(old.id==b.id)return false;s.buffs.push_back(b);}}
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
inline bool ReadShared(Reader&r,Shared&s,bool legacy=false,int savedBuffs=BUFFS) {
    s.run=r.Get(8);s.authority=r.Get(8);s.plants=r.Get(8);s.term=r.Get();s.revision=r.Get();
    for(int i=0;i<(legacy?3:savedBuffs);++i)s.stacks[i]=uint8_t(r.Get(1));s.votedLevel=int32_t(r.Get());s.reliefLevel=int32_t(r.Get());s.reliefAttempt=r.Get();
    for(auto&p:s.reliefPlayers)p=r.Get(8);
    auto&b=s.ballot;b.level=int32_t(r.Get());b.revision=r.Get();auto open=r.Get(1);b.open=open!=0;
    for(auto&c:b.choices)c=uint8_t(r.Get(1));for(auto&p:b.players)p=r.Get(8);for(auto&v:b.votes)v=int8_t(r.Get(1));
    if(!r.ok||open>1||(s.plants&~PLANTS)||s.stacks[ARSENAL]>1||s.votedLevel < -1||s.votedLevel>=60000||
        s.reliefLevel < -1||s.reliefLevel>=60000||b.level < -1||b.level>=60000)return false;
    for(int i=0;i<BUFFS;++i){if(legacy)s.stacks[i]=uint8_t(std::min(int(s.stacks[i]),MaxStacks(i)));else if(s.stacks[i]>MaxStacks(i))return false;}
    bool gap=false;for(size_t i=0;i<3;++i){if(b.choices[i]==255)gap=true;else if(gap||b.choices[i]>=BUFFS)return false;for(size_t j=0;j<i;++j)if(b.choices[i]<BUFFS&&b.choices[i]==b.choices[j])return false;}
    for(size_t i=0;i<6;++i){if(b.votes[i] < -2||b.votes[i]>=b.Count())return false;for(size_t j=0;j<i;++j)if(b.players[i]&&b.players[i]==b.players[j])return false;}
    return !b.open||(s.run&&s.authority&&b.Count()&&b.level>=0);
}
inline std::vector<unsigned char> Encode(const State&s) {
    Writer w;w.Put(0x33504d54);WriteShared(w,s.shared);w.Put(s.player,8);w.Put(s.serial,8);
    w.Put(s.localLevel);w.Put(s.attempt);w.Put(s.reliefReceivedLevel);w.Put(s.reliefReceivedAttempt);w.Put(s.allow,1);
    w.Put(s.cooldown);w.Put(s.baselineSun);w.Put(s.baselineCooldown);w.Blob(s.baselinePackets);WriteStrike(w,s.outgoing);
    w.Put(s.receipts.size(),2);
    for(const auto&x:s.receipts){w.Put(x.player,8);w.Put(x.sequence,8);WriteStrike(w,x.strike);}
    w.Put(s.giftSun,8);w.Put(s.supportCount,8);w.Put(s.sharedUsed,8);w.Put(s.speciesWon,8);w.Put(s.endlessCleared);
    w.Put(s.coopSerial);w.Put(s.giftCooldown);WriteGift(w,s.gift);w.Put(s.giftReceipts.size(),2);for(const auto&g:s.giftReceipts)WriteGift(w,g);
    w.Blob(s.lawn);w.Put(s.supportCoin);w.Put(s.suppliedLevel);w.Put(s.rescuedLevel);w.Put(s.restrictedSun);w.Put(s.sentCards.size(),2);for(const auto&card:s.sentCards)WriteStrike(w,card);
    w.Put(s.repairCooldown);w.Put(s.groupGifts.size(),1);for(const auto&g:s.groupGifts)WriteGift(w,g);
    w.Put(s.aidPlants.size(),2);for(const auto&a:s.aidPlants){w.Put(a.plant);WriteStrike(w,a.strike);}
    w.Put(s.aidFreezes.size(),2);for(const auto&f:s.aidFreezes){w.Put(f.zombie);w.Put(f.armor,1);}
    return w.data;
}
inline bool Decode(const std::vector<unsigned char>&bytes,State&out) {
    Reader r{bytes};State s;const auto version=r.Get();const bool legacy=version==0x31504d54;const bool extended=version==0x33504d54;
    if((!legacy&&!extended&&version!=0x32504d54)||!ReadShared(r,s.shared,legacy,extended?BUFFS:8))return false;
    s.player=r.Get(8);s.serial=r.Get(8);s.localLevel=int32_t(r.Get());s.attempt=r.Get();s.reliefReceivedLevel=int32_t(r.Get());s.reliefReceivedAttempt=r.Get();
    auto allow=r.Get(1);s.allow=allow!=0;s.cooldown=int32_t(r.Get());s.baselineSun=int32_t(r.Get());s.baselineCooldown=int32_t(r.Get());
    if(!r.Blob(s.baselinePackets,4096)||s.baselinePackets.size()%14||!ReadStrike(r,s.outgoing,extended))return false;
    const auto count=r.Get(2);if(count>Admission::MAX_PLAYERS_HISTORY)return false;s.receipts.resize(size_t(count));
    for(auto&x:s.receipts){x.player=r.Get(8);x.sequence=r.Get(8);if(!x.player||!ReadStrike(r,x.strike,extended))return false;}
    s.giftSun=r.Get(8);s.supportCount=r.Get(8);s.sharedUsed=r.Get(8);s.speciesWon=r.Get(8);s.endlessCleared=r.Get();
    s.coopSerial=r.Get();s.giftCooldown=int32_t(r.Get());if(!ReadGift(r,s.gift))return false;
    const auto gifts=r.Get(2);if(gifts>Admission::MAX_PLAYERS_HISTORY)return false;s.giftReceipts.resize(size_t(gifts));
    for(size_t i=0;i<s.giftReceipts.size();++i){if(!ReadGift(r,s.giftReceipts[i])||s.giftReceipts[i].phase<EXECUTED)return false;for(size_t j=0;j<i;++j)if(s.giftReceipts[i].peer==s.giftReceipts[j].peer)return false;}
    if(!r.Blob(s.lawn,24*1024*1024))return false;
    if(!legacy){s.supportCoin=uint32_t(r.Get());s.suppliedLevel=int32_t(r.Get());s.rescuedLevel=int32_t(r.Get());s.restrictedSun=int32_t(r.Get());
        auto cards=r.Get(2);if(cards>Admission::MAX_PLAYERS_HISTORY)return false;s.sentCards.resize(size_t(cards));
        for(size_t i=0;i<s.sentCards.size();++i){auto&card=s.sentCards[i];if(!ReadStrike(r,card,extended)||card.caster!=s.player||(card.phase!=DELIVERED&&card.phase!=EXECUTED))return false;for(size_t j=0;j<i;++j)if(card.id==s.sentCards[j].id)return false;}
    }
    if(extended){s.repairCooldown=int32_t(r.Get());auto n=r.Get(1);if(n>5||s.repairCooldown<0||s.repairCooldown>3000)return false;
        for(size_t i=0;i<n;++i){Gift g;if(!ReadGift(r,g))return false;for(const auto&old:s.groupGifts)if(old.peer==g.peer)return false;s.groupGifts.push_back(g);}
        auto plants=r.Get(2);if(plants>2048)return false;for(size_t i=0;i<plants;++i){AidPlant a;a.plant=uint32_t(r.Get());if(!a.plant||!ReadStrike(r,a.strike)||a.strike.phase!=EXECUTED)return false;for(const auto&old:s.aidPlants)if(old.plant==a.plant)return false;s.aidPlants.push_back(a);}
        auto freezes=r.Get(2);if(freezes>2048)return false;for(size_t i=0;i<freezes;++i){AidFreeze f;f.zombie=uint32_t(r.Get());auto armor=r.Get(1);f.armor=armor!=0;if(!f.zombie||armor>1)return false;for(const auto&old:s.aidFreezes)if(old.zombie==f.zombie)return false;s.aidFreezes.push_back(f);}}
    if(s.suppliedLevel < -1||s.suppliedLevel>=60000||s.rescuedLevel < -1||s.rescuedLevel>=60000||s.restrictedSun<0)return false;
    if(!r.ok||r.at!=bytes.size()||allow>1||s.localLevel < -1||s.localLevel>=60000||
       s.reliefReceivedLevel < -1||s.reliefReceivedLevel>=60000||s.giftCooldown<0||s.giftCooldown>6000||s.cooldown<0||s.cooldown>2000||s.baselineCooldown<0||s.baselineCooldown>2000||s.baselineSun<0||(s.sharedUsed&~PLANTS)||(s.speciesWon&~uint64_t(127))||s.endlessCleared>60000)return false;
    for(size_t i=0;i<s.receipts.size();++i)for(size_t j=0;j<i;++j)if(s.receipts[i].player==s.receipts[j].player)return false;
    if(legacy){s.outgoing.bursts=uint16_t(std::min(3,int(s.outgoing.bursts)));for(auto&receipt:s.receipts)receipt.strike.bursts=uint16_t(std::min(3,int(receipt.strike.bursts)));}
    out=std::move(s);return true;
}
}
