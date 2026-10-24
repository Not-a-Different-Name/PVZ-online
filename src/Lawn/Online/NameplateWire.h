#pragma once
#include <cstddef>
#include <cstdint>

namespace NameplateWire {
constexpr uint16_t TYPE=29;
constexpr size_t SIZE=4;
struct Message {uint8_t src=0,dst=0,id=0;};
inline bool Decode(const uint8_t* data,size_t size,Message& message)
{
    if(!data || size!=SIZE || data[0]<1 || data[0]>6 || data[1]<1 || data[1]>6 ||
        data[0]==data[1] || data[2]!=1 || data[3]>3)return false;
    message={data[0],data[1],data[3]};return true;
}
inline void Encode(uint8_t* data,uint8_t id)
{
    data[0]=data[1]=0;data[2]=1;data[3]=id;
}
}
