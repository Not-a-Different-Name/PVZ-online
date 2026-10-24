#pragma once
#include "TeamProgressionState.h"
#include <map>
#include <algorithm>
namespace TeamGoals {
struct Progress {uint64_t gifts=0,fires=0,shared=0,species=0,endless=0;};
inline uint64_t Add(uint64_t a,uint64_t b){return b>UINT64_MAX-a?UINT64_MAX:a+b;}
inline int Bits(uint64_t n){int k=0;for(;n;n&=n-1)++k;return k;}
struct Ledger {
    std::map<std::pair<uint64_t,uint64_t>,Progress> runs;
    Progress Total()const {Progress t;for(const auto&entry:runs){const auto&p=entry.second;t.gifts=Add(t.gifts,p.gifts);t.fires=Add(t.fires,p.fires);t.shared|=p.shared;t.species|=p.species;t.endless=std::max(t.endless,p.endless);}return t;}
    bool Merge(const TeamProgression::State&s){
        if(!s.player||!s.shared.run)return false;
        auto&v=runs[{s.shared.run,s.player}];const auto before=v;
        v.gifts=std::max(v.gifts,s.giftSun);v.fires=std::max(v.fires,s.supportCount);v.shared|=s.sharedUsed;v.species|=s.speciesWon;v.endless=std::max<uint64_t>(v.endless,s.endlessCleared);
        return before.gifts!=v.gifts||before.fires!=v.fires||before.shared!=v.shared||before.species!=v.species||before.endless!=v.endless;
    }
};
inline uint32_t Hash(const unsigned char*p,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;++i)h=(h^p[i])*16777619u;return h;}
inline std::vector<unsigned char> Encode(const Ledger&l){TeamProgression::Writer w;w.Put(0x314c4754);w.Put(l.runs.size());for(const auto&e:l.runs){w.Put(e.first.first,8);w.Put(e.first.second,8);for(auto n:{e.second.gifts,e.second.fires,e.second.shared,e.second.species,e.second.endless})w.Put(n,8);}w.Put(Hash(w.data.data(),w.data.size()));return w.data;}
inline bool Decode(const std::vector<unsigned char>&bytes,Ledger&out){
    TeamProgression::Reader r{bytes};Ledger l;if(r.Get()!=0x314c4754)return false;const auto n=r.Get();if(n>100000||bytes.size()!=12+56*n)return false;
    for(size_t i=0;i<n;++i){const auto run=r.Get(8),player=r.Get(8);Progress p;p.gifts=r.Get(8);p.fires=r.Get(8);p.shared=r.Get(8);p.species=r.Get(8);p.endless=r.Get(8);
        if(!run||!player||(p.shared&~TeamProgression::PLANTS)||(p.species&~uint64_t(127))||p.endless>60000||!l.runs.emplace(std::make_pair(run,player),p).second)return false;}
    const auto checksum=r.Get();if(!r.ok||r.at!=bytes.size()||checksum!=Hash(bytes.data(),bytes.size()-4))return false;out=std::move(l);return true;
}
}
