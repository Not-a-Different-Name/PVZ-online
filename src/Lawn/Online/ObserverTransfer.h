#pragma once
#include "ObserverWireV2.h"
#include <array>
#include <cstdint>
#include <vector>
namespace ObserverTransfer {
inline uint32_t Crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(int j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
inline bool Newer(uint32_t a,uint32_t b){return a!=b&&uint32_t(a-b)<0x80000000u;}
struct Complete {uint8_t src=0,dst=0;uint32_t watch=0,stream=0,sequence=0,tick=0,epoch=0,rawBytes=0;std::vector<uint8_t> data;};
inline bool Fragment(uint8_t src,uint8_t dst,uint32_t watch,uint32_t stream,uint32_t seq,uint32_t tick,uint32_t epoch,const std::vector<uint8_t>&data,std::vector<std::vector<uint8_t>>&out){
 out.clear();if(data.size()<5||data.size()>ObserverWireV2::MAX_ENCODED||!epoch)return false;
 uint32_t raw=ObserverWireV2::Get32(data.data()+1);if(!raw||raw>ObserverWireV2::MAX_RAW)return false;
 const int count=int((data.size()+ObserverWireV2::FRAME_DATA-1)/ObserverWireV2::FRAME_DATA);const uint32_t crc=Crc(data.data(),data.size());
 for(int i=0;i<count;++i){ObserverWireV2::Frame f;f.src=src;f.dst=dst;f.flags=1;f.watchId=watch;f.boardStream=stream;f.frameSeq=seq;f.sourceTick=tick;f.roundEpoch=epoch;f.partIndex=uint16_t(i);f.partCount=uint16_t(count);size_t at=size_t(i)*ObserverWireV2::FRAME_DATA;f.data=data.data()+at;f.dataSize=int(std::min(size_t(ObserverWireV2::FRAME_DATA),data.size()-at));f.encodedBytes=f.dataSize;f.rawBytes=raw;f.crc32=crc;std::vector<uint8_t> packet;if(!ObserverWireV2::EncodeFrame(f,packet)){out.clear();return false;}out.push_back(std::move(packet));}return true;
}
class Assembler {
public:
 uint64_t Progress()const{return progress;}
 void Reset(){base={};buffer.clear();startedAt=0;next=0;active=false;}
 void Context(uint8_t source,uint8_t dest,uint32_t watch,uint32_t stream,uint32_t epoch){if(src!=source||dst!=dest||watchId!=watch||streamId!=stream||round!=epoch){Reset();last=0;}src=source;dst=dest;watchId=watch;streamId=stream;round=epoch;}
 bool Accept(const uint8_t*p,int size,uint64_t now,Complete&complete){
  ObserverWireV2::Frame f;if(!ObserverWireV2::DecodeFrame(p,size,f)||f.src>6||f.dst>6||f.flags!=1||f.src!=src||f.dst!=dst||f.watchId!=watchId||f.boardStream!=streamId||f.roundEpoch!=round||!Newer(f.frameSeq,last))return false;
  if(active&&(now<startedAt||now-startedAt>3000))Reset();
  if(f.partIndex==0){if(active&&!Newer(f.frameSeq,base.frameSeq))return false;Reset();base=f;base.data=nullptr;active=true;startedAt=now;buffer.reserve(std::min(size_t(f.partCount)*ObserverWireV2::FRAME_DATA,size_t(ObserverWireV2::MAX_ENCODED)));}
  if(!active||f.partIndex!=next||f.frameSeq!=base.frameSeq||f.partCount!=base.partCount||f.sourceTick!=base.sourceTick||f.rawBytes!=base.rawBytes||f.crc32!=base.crc32||f.flags!=base.flags){Reset();return false;}
  if(buffer.size()+size_t(f.dataSize)>ObserverWireV2::MAX_ENCODED){Reset();return false;}
  buffer.insert(buffer.end(),f.data,f.data+f.dataSize);++next;++progress;if(next!=base.partCount)return false;
  if(buffer.size()<5||Crc(buffer.data(),buffer.size())!=base.crc32||ObserverWireV2::Get32(buffer.data()+1)!=base.rawBytes){Reset();return false;}
  complete={src,dst,watchId,streamId,base.frameSeq,base.sourceTick,round,base.rawBytes,std::move(buffer)};last=base.frameSeq;Reset();return true;
 }
private:uint8_t src=0,dst=0;uint32_t watchId=0,streamId=0,round=0,last=0;bool active=false;uint64_t startedAt=0,progress=0;uint16_t next=0;ObserverWireV2::Frame base;std::vector<uint8_t> buffer;
};
}
