#pragma once
#include <cstdint>
namespace CoopWire {
// A session-local identity survives seat swaps and reconnects of this session object.
enum Op : uint8_t { HELLO=0, SUN_REQUEST, SUN_GIFT, SUN_RESULT, SUN_DONE, OFFER, ACCEPT, DECLINE, CANCEL, ASK, RETURN_LOAN, RETURN_RESULT };
constexpr int MAX_TRAITS=16;
struct Event {
    uint8_t src=0,dst=0,op=0,plant=0,count=0,result=0;
    uint32_t run=0,level=0,serial=0,request=0,attempt=0;
    uint64_t sender=0,target=0;
    uint16_t ids[MAX_TRAITS]{},stacks[MAX_TRAITS]{};
};
inline int Encode(uint8_t* b,const Event& e) {
    int at=0; auto put=[&](uint64_t v,int n){for(int i=0;i<n;++i)b[at++]=(uint8_t)(v>>(8*i));};
    if(e.count>MAX_TRAITS)return 0;
    put(e.src,1);put(e.dst,1);put(e.op,1);put(e.plant,1);put(e.count,1);put(e.result,1);
    put(e.run,4);put(e.level,4);put(e.serial,4);put(e.request,4);put(e.sender,8);put(e.target,8);put(e.attempt,4);
    for(int i=0;i<e.count;++i){put(e.ids[i],2);put(e.stacks[i],2);}return at;
}
inline bool Decode(const uint8_t* b,int size,Event& e) {
    if(size<42)return false;int at=0;
    auto get=[&](int n){uint64_t v=0;for(int i=0;i<n;++i)v|=(uint64_t)b[at++]<<(8*i);return v;};
    e.src=get(1);e.dst=get(1);e.op=get(1);e.plant=get(1);e.count=get(1);e.result=get(1);
    if(e.op>RETURN_RESULT || e.result>1 || e.count>MAX_TRAITS || size!=42+e.count*4)return false;
    e.run=get(4);e.level=get(4);e.serial=get(4);e.request=get(4);e.sender=get(8);e.target=get(8);e.attempt=get(4);
    if(!e.sender)return false;
    for(int i=0;i<e.count;++i){e.ids[i]=get(2);e.stacks[i]=get(2);if(!e.stacks[i])return false;
        for(int j=0;j<i;++j)if(e.ids[i]==e.ids[j])return false;}
    return true;
}
}
