#pragma once
#include <filesystem>
#include <string>
#include <system_error>

namespace NameplateStorage {
inline bool Ready(const std::string& directory,int profile)
{
    std::error_code error;
    if(!std::filesystem::exists(directory,error))return !error;
    if(error)return false;
    const std::string prefix="run"+std::to_string(profile)+"_";
    std::filesystem::directory_iterator entry(directory,error),end;
    if(error)return false;
    while(entry!=end) {
        const std::string name=entry->path().filename().string();
        if(name.compare(0,prefix.size(),prefix)==0 && name.size()>=8 && name.compare(name.size()-8,8,".dat.txn")==0)return false;
        entry.increment(error);
        if(error)return false;
    }
    return true;
}
}
