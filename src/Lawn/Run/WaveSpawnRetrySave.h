#pragma once
#include "WaveHealthSave.h"

namespace WaveSpawnRetrySave {
inline std::vector<unsigned char> SaveTrailer(const Board* board) {
    std::vector<unsigned char> out{'P','V','Z','W','S','P','0','1'};
    WaveHealthSave::WriteLE(out,28,4);
    WaveHealthSave::WriteLE(out,static_cast<uint32_t>(board->mWaveSpawnRetry.wave),4);
    WaveHealthSave::WriteLE(out,board->mWaveSpawnRetry.index,4);
    WaveHealthSave::WriteLE(out,board->mWaveSpawnRetry.part,4);
    WaveHealthSave::WriteLE(out,WaveHealthSave::Checksum(out.data(),out.size()),4);
    return out;
}
inline bool LoadTrailer(Board* board,const unsigned char* data,size_t size) {
    board->mWaveSpawnRetry={};
    if(!data || size<8)return true;
    for(size_t pos=size-8;;--pos) {
        if(std::memcmp(data+pos,"PVZWSP01",8)==0) {
            if(size-pos<28 || WaveHealthSave::ReadLE(data+pos+8,4)!=28 ||
                WaveHealthSave::ReadLE(data+pos+24,4)!=WaveHealthSave::Checksum(data+pos,24))return false;
            auto wave=WaveHealthSave::ReadLE(data+pos+12,4);
            auto index=WaveHealthSave::ReadLE(data+pos+16,4),part=WaveHealthSave::ReadLE(data+pos+20,4);
            if(wave==UINT32_MAX)return index==0 && part==0;
            if(wave>=MAX_ZOMBIE_WAVES || wave!=static_cast<unsigned>(board->mCurrentWave) ||
                index>MAX_ZOMBIES_IN_WAVE || part>=MAX_ZOMBIE_FOLLOWERS)return false;
            if(part && (index>=MAX_ZOMBIES_IN_WAVE ||
                board->mZombiesInWave[wave][index]!=ZombieType::ZOMBIE_BOBSLED))return false;
            board->mWaveSpawnRetry={static_cast<int32_t>(wave),static_cast<int32_t>(index),static_cast<int32_t>(part)};
            return true;
        }
        if(pos==0)break;
    }
    return true;
}
}
