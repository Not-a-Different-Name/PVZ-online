#pragma once
#include "WaveHealthSave.h"

namespace DeferredZombieSpawnSave {
constexpr uint32_t MaxTickets = 2048;
inline std::vector<unsigned char> SaveTrailer(const Board* board) {
    const auto& state = board->mDeferredZombieSpawns;
    const uint32_t count = state.HasPending() ? static_cast<uint32_t>(state.pending.size() - state.next) : 0;
    std::vector<unsigned char> out{'P','V','Z','A','S','P','0','1'};
    WaveHealthSave::WriteLE(out,20 + 20 * count,4);
    WaveHealthSave::WriteLE(out,count,4);
    for (uint32_t i=0;i<count;++i) {
        const auto& ticket=state.pending[state.next+i];
        for (int32_t value : {ticket.type,ticket.x,ticket.y,ticket.wave,ticket.kind})
            WaveHealthSave::WriteLE(out,static_cast<uint32_t>(value),4);
    }
    WaveHealthSave::WriteLE(out,WaveHealthSave::Checksum(out.data(),out.size()),4);
    return out;
}
inline bool LoadTrailer(Board* board,const unsigned char* data,size_t size) {
    auto& state=board->mDeferredZombieSpawns;state.Clear();
    if (!data || size<8) return true;
    for (size_t pos=size-8;;--pos) {
        if (std::memcmp(data+pos,"PVZASP01",8)==0) {
            if (size-pos<20) return false;
            const auto length=WaveHealthSave::ReadLE(data+pos+8,4);
            const auto count=WaveHealthSave::ReadLE(data+pos+12,4);
            if (count>MaxTickets || length!=20+count*20 || length>size-pos ||
                WaveHealthSave::ReadLE(data+pos+length-4,4)!=WaveHealthSave::Checksum(data+pos,length-4)) return false;
            std::vector<DeferredZombieSpawn> restored;restored.reserve(static_cast<size_t>(count));
            for (uint32_t i=0;i<count;++i) {
                const auto* p=data+pos+16+i*20;
                DeferredZombieSpawn ticket{static_cast<int32_t>(WaveHealthSave::ReadLE(p,4)),static_cast<int32_t>(WaveHealthSave::ReadLE(p+4,4)),
                    static_cast<int32_t>(WaveHealthSave::ReadLE(p+8,4)),static_cast<int32_t>(WaveHealthSave::ReadLE(p+12,4)),static_cast<int32_t>(WaveHealthSave::ReadLE(p+16,4))};
                if ((ticket.type!=ZombieType::ZOMBIE_NORMAL && ticket.type!=ZombieType::ZOMBIE_TRAFFIC_CONE && ticket.type!=ZombieType::ZOMBIE_PAIL) ||
                    ticket.x<0 || ticket.x>=MAX_GRID_SIZE_X || ticket.y<0 || ticket.y>=MAX_GRID_SIZE_Y ||
                    ticket.wave<0 || ticket.wave>MAX_ZOMBIE_WAVES || ticket.kind<0 || ticket.kind>2) return false;
                restored.push_back(ticket);
            }
            state.pending.swap(restored);
            if (state.HasPending()) board->mRiseFromGraveCounter=1;
            return true;
        }
        if (pos==0) break;
    }
    return true;
}
}
