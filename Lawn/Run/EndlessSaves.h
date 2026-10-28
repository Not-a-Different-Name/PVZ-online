#pragma once
#include "RunState.h"
#include "RunCheckpointIO.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <chrono>

// Independent run slots and a durable start-of-level record. Native lawns never cross devices.
namespace EndlessSaves {
struct Record { uint64_t id=0;int scene=0,level=0;int64_t modified=0;bool joint=false; };
inline std::vector<unsigned char> Read(const std::string& path) {
    std::ifstream f(std::filesystem::path(path),std::ios::binary|std::ios::ate);
    if(!f)return {};auto size=f.tellg();if(size<=0||size>64*1024*1024)return {};
    std::vector<unsigned char> b(static_cast<size_t>(size));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),size))return {};return b;
}
inline bool Exists(const std::string& path) {std::error_code ec;return std::filesystem::is_regular_file(std::filesystem::path(path),ec);}
inline std::string Path(int profile,const RunState& run) {return RunState::GetCheckpointName(profile,RunState::RUN_MODE_ENDLESS,run.mEndlessScene,run.mAdmission.run);}
inline bool ArchiveLegacy(int profile,int scene) {
    const auto alias=RunState::GetCheckpointName(profile,RunState::RUN_MODE_ENDLESS,scene);
    if(!Exists(alias)&&!Exists(alias+".bak"))return true;
    RunState saved;if(!saved.Load(profile,RunState::RUN_MODE_ENDLESS,scene))return true; // damaged originals remain untouched by this migration
    uint64_t id=saved.mAdmission.run;
    if(!id){auto data=Read(alias+".slot-id");if(data.size()==8){const auto* p=data.data();id=Admission::Get(p,8);}}
    if(!id)id=Admission::NewId();
    const auto path=RunState::GetCheckpointName(profile,RunState::RUN_MODE_ENDLESS,scene,id);
    if(!Exists(path)&&!Exists(path+".bak")) {
        saved.mAdmission.run=id;
        if(!RunCheckpointIO::AtomicWrite(path,saved.EncodeCheckpoint()))return false;
    }
    std::vector<unsigned char> data;Admission::Put(data,id,8);
    return RunCheckpointIO::AtomicWrite(alias+".slot-id",data);
}
inline void CarryProgress(int profile,RunState& restored) {
    RunState head;
    if(!head.Load(profile,RunState::RUN_MODE_ENDLESS,restored.mEndlessScene,false,restored.mAdmission.run) ||
       head.mRunSeed!=restored.mRunSeed || (head.mTeam.player && restored.mTeam.player && head.mTeam.player!=restored.mTeam.player))return;
    for(int i=0;i<RunState::RUN_LEVEL_COUNT;++i)restored.mFailCounts[i]=std::max(restored.mFailCounts[i],head.mFailCounts[i]);
    auto& a=restored.mTeam;const auto& b=head.mTeam;
    a.giftSun=std::max(a.giftSun,b.giftSun);a.supportCount=std::max(a.supportCount,b.supportCount);
    a.sharedUsed|=b.sharedUsed;a.speciesWon|=b.speciesWon;a.endlessCleared=std::max(a.endlessCleared,b.endlessCleared);
    a.serial=std::max(a.serial,b.serial);a.coopSerial=std::max(a.coopSerial,b.coopSerial);
}
inline bool Load(int profile,int scene,uint64_t id,bool online,RunState& out) {
    const auto path=RunState::GetCheckpointName(profile,RunState::RUN_MODE_ENDLESS,scene,id);
    const bool joint=online&&(Exists(path+".joint")||Exists(path+".joint.bak"));
    if(!id || !out.Load(profile,RunState::RUN_MODE_ENDLESS,scene,false,id,joint?".joint":"") || out.mAdmission.run!=id)return false;
    if(joint)CarryProgress(profile,out);
    return true;
}
inline std::vector<Record> List(int profile,bool online,bool* migrated=nullptr) {
    bool okay=true;for(int scene=0;scene<5;++scene)okay=ArchiveLegacy(profile,scene)&&okay;if(migrated)*migrated=okay;
    std::vector<Record> records;
    auto folder=std::filesystem::path(RunState::GetCheckpointName(profile)).parent_path();std::error_code ec;
    std::filesystem::directory_iterator it(folder,ec),end;
    for(;!ec&&it!=end;it.increment(ec)) {
        auto name=it->path().filename().string();
        if(name.size()>4&&name.substr(name.size()-4)==".bak")name.resize(name.size()-4);
        for(int scene=0;scene<5;++scene) {
            auto prefix=std::filesystem::path(RunState::GetCheckpointName(profile,RunState::RUN_MODE_ENDLESS,scene)).stem().string()+"_team";
            if(name.size()<=prefix.size()+4||name.compare(0,prefix.size(),prefix)!=0||name.substr(name.size()-4)!=".dat")continue;
            auto digits=name.substr(prefix.size(),name.size()-prefix.size()-4);if(digits.size()>20||digits.empty()||digits.find_first_not_of("0123456789")!=std::string::npos)continue;
            uint64_t id=0;for(char c:digits){if(id>(UINT64_MAX-uint64_t(c-'0'))/10){id=0;break;}id=id*10+uint64_t(c-'0');}
            if(std::any_of(records.begin(),records.end(),[&](const Record& item){return item.id==id&&item.scene==scene;}))continue;
            RunState run;if(!Load(profile,scene,id,online,run))continue;
            const auto head=RunState::GetCheckpointName(profile,RunState::RUN_MODE_ENDLESS,scene,id);
            const bool joint=online&&Exists(head+".joint");
            auto stamp=std::filesystem::last_write_time(std::filesystem::path(head+(joint?".joint":"")),ec);if(ec)break;
            auto date=std::chrono::time_point_cast<std::chrono::seconds>(stamp-std::filesystem::file_time_type::clock::now()+std::chrono::system_clock::now()).time_since_epoch().count();
            records.push_back({id,scene,run.GetPlayingLevelIndex(),date,joint});
        }
    }
    std::sort(records.begin(),records.end(),[](const Record&a,const Record&b){return a.modified!=b.modified?a.modified>b.modified:a.id>b.id;});return records;
}
inline bool Stage(int profile,const RunState& run,const std::vector<unsigned char>& key) {
    if(!run.mAdmission.run)return false;
    auto data=run.EncodeCheckpoint();auto identity=key;Admission::Put(identity,EndlessRules::Hash(data.data(),data.size()));
    const auto path=Path(profile,run);
    return RunCheckpointIO::AtomicWrite(path+".prepared",data)&&RunCheckpointIO::AtomicWrite(path+".prepared-key",identity);
}
inline bool Commit(int profile,const RunState& run,const std::vector<unsigned char>& key) {
    auto path=Path(profile,run);auto data=Read(path+".prepared");auto identity=key;Admission::Put(identity,EndlessRules::Hash(data.data(),data.size()));
    if(data.empty()||Read(path+".prepared-key")!=identity)return false;
    // Retry the original baseline; never rotate its predecessor away on same-level retries.
    RunState previous;if(previous.Load(profile,RunState::RUN_MODE_ENDLESS,run.mEndlessScene,false,run.mAdmission.run,".joint")&&previous.mLevelIndex==run.mLevelIndex&&previous.mAdmission.run==run.mAdmission.run)return true;
    if(Exists(path+".joint") && !RunCheckpointIO::AtomicWrite(path+".joint.bak",Read(path+".joint")))return false;
    return RunCheckpointIO::AtomicWrite(path+".joint",data);
}
inline bool PreserveUnmatched(int profile,int scene,uint64_t id) {
    auto path=RunState::GetCheckpointName(profile,RunState::RUN_MODE_ENDLESS,scene,id);
    if(!Exists(path))return true;
    RunState previous;
    if(previous.Load(profile,RunState::RUN_MODE_ENDLESS,scene,false,id)) {
        previous.mAdmission.run=Admission::NewId();
        return RunCheckpointIO::AtomicWrite(Path(profile,previous),previous.EncodeCheckpoint());
    }
    auto data=Read(path);return !data.empty() && RunCheckpointIO::AtomicWrite(path+".retained-"+std::to_string(Admission::NewId()),data);
}
inline bool Match(int profile,int scene,uint64_t id,int seed,int level,bool legacy,RunState& out) {
    for(bool backup:{false,true}) {
        RunState saved;
        if(saved.Load(profile,RunState::RUN_MODE_ENDLESS,scene,false,id,backup?".joint.bak":".joint")&&saved.mAdmission.run==id&&saved.mRunSeed==seed&&saved.mLevelIndex==level){CarryProgress(profile,saved);out=std::move(saved);return true;}
    }
    if(!legacy)return false;
    RunState saved;
    bool loaded=saved.Load(profile,RunState::RUN_MODE_ENDLESS,scene,false,id);
    if(!loaded)loaded=saved.Load(profile,RunState::RUN_MODE_ENDLESS,scene);
    if(!loaded||saved.mRunSeed!=seed||saved.mLevelIndex!=level||saved.mEndlessScene!=scene)return false;
    if(saved.mAdmission.run&&saved.mAdmission.run!=id)return false;
    saved.mAdmission.run=id;out=std::move(saved);return true;
}
}
