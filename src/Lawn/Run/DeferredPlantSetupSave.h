#pragma once
#include "WaveHealthSave.h"

namespace DeferredPlantSetupSave {
inline std::vector<unsigned char> SaveTrailer(const Board* board) {
    const auto& state=board->mDeferredPlantSetup;
    const uint32_t count=state.HasPending()?static_cast<uint32_t>(state.pending.size()-state.next):0;
    std::vector<unsigned char> out{'P','V','Z','P','S','P','0','1'};
    WaveHealthSave::WriteLE(out,28+16*count,4);
    WaveHealthSave::WriteLE(out,state.flags,4);
    WaveHealthSave::WriteLE(out,state.kind,4);
    WaveHealthSave::WriteLE(out,count,4);
    for(uint32_t i=0;i<count;++i) {
        const auto& item=state.pending[state.next+i];
        for(int32_t value:{item.seed,item.x,item.y,item.extra}) WaveHealthSave::WriteLE(out,static_cast<uint32_t>(value),4);
    }
    WaveHealthSave::WriteLE(out,WaveHealthSave::Checksum(out.data(),out.size()),4);
    return out;
}
inline bool LoadTrailer(Board* board,const unsigned char* data,size_t size) {
    auto& state=board->mDeferredPlantSetup;state={};
    if(!data || size<8)return true;
    for(size_t pos=size-8;;--pos) {
        if(std::memcmp(data+pos,"PVZPSP01",8)==0) {
            if(size-pos<28)return false;
            const auto length=WaveHealthSave::ReadLE(data+pos+8,4),flags=WaveHealthSave::ReadLE(data+pos+12,4);
            const auto kind=WaveHealthSave::ReadLE(data+pos+16,4),count=WaveHealthSave::ReadLE(data+pos+20,4);
            if(count>MAX_GRID_SIZE_X*MAX_GRID_SIZE_Y || flags>7 || kind>DeferredPlantSetup::BEGHOULED ||
                (count && kind==DeferredPlantSetup::NONE) || length!=28+count*16 || length>size-pos ||
                WaveHealthSave::ReadLE(data+pos+length-4,4)!=WaveHealthSave::Checksum(data+pos,length-4))return false;
            std::vector<DeferredPlantSetup::Ticket> restored;restored.reserve(static_cast<size_t>(count));
            for(uint32_t i=0;i<count;++i) {
                const auto* p=data+pos+24+i*16;
                DeferredPlantSetup::Ticket item{static_cast<int32_t>(WaveHealthSave::ReadLE(p,4)),static_cast<int32_t>(WaveHealthSave::ReadLE(p+4,4)),
                    static_cast<int32_t>(WaveHealthSave::ReadLE(p+8,4)),static_cast<int32_t>(WaveHealthSave::ReadLE(p+12,4))};
                if(item.seed<0 || item.seed>SeedType::SEED_IMITATER || item.x<0 || item.x>=MAX_GRID_SIZE_X ||
                    item.y<0 || item.y>=MAX_GRID_SIZE_Y || item.extra < -1000 || item.extra>1000)return false;
                for(const auto& previous:restored)if(previous.x==item.x && previous.y==item.y)return false;
                restored.push_back(item);
            }
            state.pending.swap(restored);state.flags=static_cast<uint32_t>(flags);state.kind=static_cast<int32_t>(kind);
            return true;
        }
        if(pos==0)break;
    }
    return true;
}
}
