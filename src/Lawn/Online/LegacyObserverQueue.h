#pragma once
#include <cstdint>
#include <cstddef>

// Shared transport policy only: no game state, pointers or protocol changes.
namespace LegacyObserverQueue {
inline bool IsSnapshot(const uint8_t* bytes, std::size_t size) {
    if(!bytes || size<4 || size!=4+std::size_t(bytes[2]|(uint16_t(bytes[3])<<8)))return false;
    const uint16_t type=bytes[0]|(uint16_t(bytes[1])<<8);
    const uint8_t* body=bytes+4;std::size_t count=size-4;
    if(type==31)return count>=40&&count<=256;
    if(type==0xF011&&count>=50&&(body[8]|(uint16_t(body[9])<<8))==31)return true;
    if(type==0xF011) {
        if(count<16 || (body[8]|(uint16_t(body[9])<<8))!=18)return false;
        body+=10;count-=10;
    } else if(type!=18)return false;
    return count>=6 && count<=256 && body[0]>=1 && body[0]<=6 && body[1]>=1 && body[1]<=6 && body[0]!=body[1] && body[5]>=1 && body[5]<=8 && body[4]<body[5];
}
constexpr std::size_t ReceiveSnapshotBudget=224; // 32 of the existing 256 slots remain for gameplay/control.
}
