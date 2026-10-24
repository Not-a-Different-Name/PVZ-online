#pragma once
#include "TeamGoals.h"
#include "TeamProgressionView.h"
#include "RunCheckpointIO.h"
#include "../System/PlayerInfo.h"
#include "../ModText.h"
#include "../../LawnApp.h"
#include <fstream>
#include <iterator>

namespace TeamGoalsStorage {
inline const char* Name(int id){
    static const char*zh[]={"雪中送炭","跨场炮手","百草同心","强敌克星","无尽新星","无尽先锋","无尽传奇"};
    static const char*en[]={"A helping hand","Cross-lawn gunner","Shared roots","Species challenger","Endless rising star","Endless vanguard","Endless legend"};
    return id>=0&&id<7?ModText::Tr(zh[id],en[id]):"";
}
inline std::vector<TeamProgressionView::Goal> View(const TeamGoals::Progress&t){
    const uint64_t values[]={t.gifts,t.fires,uint64_t(TeamGoals::Bits(t.shared)),uint64_t(TeamGoals::Bits(t.species)),t.endless,t.endless,t.endless};
    const uint64_t targets[]={10000,10,6,7,10,25,50};
    static const char*zh[]={"主动赠送实际到账阳光，不计胜利驰援。","成功施放跨场火力，空炸也计。","实际种植原本不拥有的不同共享植物。","分别在七类新强化生效时各赢一关。","无尽通过10关。","无尽通过25关。","无尽通过50关。"};
    static const char*en[]={"Sun actually received from manual gifts; relief excluded.","Successfully execute cross-lawn fire; empty blasts count.","Plant different shared plants not personally owned.","Win with each of the seven new enhancements separately.","Clear 10 endless stages.","Clear 25 endless stages.","Clear 50 endless stages."};
    std::vector<TeamProgressionView::Goal> out;
    for(int i=0;i<7;++i){TeamProgressionView::Goal g;g.name=Name(i);g.earned=values[i]>=targets[i];g.progress=std::to_string(std::min(values[i],targets[i]))+"/"+std::to_string(targets[i]);g.progressPermille=g.earned?1000:int(values[i]*1000/targets[i]);g.description=ModText::Tr(zh[i],en[i]);out.push_back(g);}return out;
}
inline std::string Path(LawnApp*app,const char*prefix){return Sexy::GetAppDataFolder()+"userdata/"+prefix+"-"+std::to_string(app->mPlayerInfo->mId)+".dat";}
inline bool Load(LawnApp*app,TeamGoals::Ledger&out){
    if(!app||!app->mPlayerInfo)return false;
    std::ifstream file(Path(app,"team-goals"),std::ios::binary);if(!file){out={};return true;}
    std::vector<unsigned char>bytes((std::istreambuf_iterator<char>(file)),{});return TeamGoals::Decode(bytes,out);
}
inline int Title(LawnApp*app,const TeamGoals::Ledger&ledger){
    if(!app||!app->mPlayerInfo)return 0;
    std::ifstream file(Path(app,"team-title"),std::ios::binary);if(!file)return 0;
    std::vector<unsigned char>b((std::istreambuf_iterator<char>(file)),{});if(b.size()!=12)return 0;
    TeamProgression::Reader r{b};if(r.Get()!=0x31544754)return 0;const int title=int(r.Get());
    if(r.Get()!=TeamGoals::Hash(b.data(),8)||title<0||title>7)return 0;
    return !title||View(ledger.Total())[title-1].earned?title:0;
}
inline int Title(LawnApp*app){TeamGoals::Ledger l;return Load(app,l)?Title(app,l):0;}
inline bool Equip(LawnApp*app,int title){
    TeamGoals::Ledger ledger;if(title<0||title>7||!Load(app,ledger)||(title&&!View(ledger.Total())[title-1].earned))return false;
    TeamProgression::Writer w;w.Put(0x31544754);w.Put(title);w.Put(TeamGoals::Hash(w.data.data(),w.data.size()));return RunCheckpointIO::AtomicWrite(Path(app,"team-title"),w.data);
}
}
