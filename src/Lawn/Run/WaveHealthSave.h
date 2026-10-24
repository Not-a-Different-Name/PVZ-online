#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

// Optional, checksummed data leaves legacy Board bytes and portable field tags intact.
// Board is supplied by the including translation unit; this header also supports isolated tests.
namespace WaveHealthSave {
inline uint64_t ReadLE(const unsigned char* p, unsigned bytes) {
    uint64_t value=0;
    for(unsigned i=0;i<bytes;++i)value|=uint64_t(p[i])<<(i*8);
    return value;
}
inline void WriteLE(std::vector<unsigned char>& out,uint64_t value,unsigned bytes) {
    for(unsigned i=0;i<bytes;++i)out.push_back(static_cast<unsigned char>(value>>(i*8)));
}
inline uint32_t Checksum(const unsigned char* data,size_t size) {
    uint32_t hash=2166136261u;
    for(size_t i=0;i<size;++i)hash=(hash^data[i])*16777619u;
    return hash;
}
inline std::vector<unsigned char> SaveTrailer(const Board* board) {
    std::vector<unsigned char> out{'P','V','Z','W','H','P','0','1'};
    WriteLE(out,32,4);
    WriteLE(out,static_cast<uint64_t>(board->mWaveHealth.start),8);
    WriteLE(out,static_cast<uint64_t>(board->mWaveHealth.next),8);
    WriteLE(out,Checksum(out.data(),out.size()),4);
    return out;
}
inline bool LoadTrailer(Board* board,const unsigned char* data,size_t size) {
    board->RestoreLegacyWaveHealth();
    if(!data || size<8)return true;
    for(size_t pos=size-8;;--pos) {
        if(std::memcmp(data+pos,"PVZWHP01",8)==0) {
            if(size-pos<32 || ReadLE(data+pos+8,4)!=32 ||
                ReadLE(data+pos+28,4)!=Checksum(data+pos,28))return false;
            const uint64_t start=ReadLE(data+pos+12,8),trigger=ReadLE(data+pos+20,8);
            // Keep subsequent differences and conversions inside signed arithmetic range.
            if(start>static_cast<uint64_t>(std::numeric_limits<int64_t>::max()/4) ||
                (trigger!=UINT64_MAX && trigger>start))return false;
            board->mWaveHealth.start=static_cast<int64_t>(start);
            board->mWaveHealth.next=trigger==UINT64_MAX?-1:static_cast<int64_t>(trigger);
            board->SyncLegacyWaveHealth();
            return true;
        }
        if(pos==0)break;
    }
    return true;
}
}
