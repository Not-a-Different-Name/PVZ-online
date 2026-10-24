#pragma once

// MOD91+ observer v2 wire primitives.  This header is deliberately independent
// of Board/Plant/Zombie objects: observer frames are presentation data only.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ObserverWireV2 {
static constexpr std::uint8_t SCHEMA = 2;
static constexpr std::uint8_t CONTROL_TYPE = 30;
static constexpr std::uint8_t FRAME_TYPE = 31;
static constexpr int MAX_PAYLOAD = 256;
static constexpr int FRAME_HEADER = 40;
static constexpr int FRAME_DATA = MAX_PAYLOAD - FRAME_HEADER;
static constexpr int MAX_PARTS = 304;
static constexpr std::uint32_t MAX_ENCODED = 64U * 1024U;
static constexpr std::uint32_t MAX_RAW = 128U * 1024U;

enum class ControlOp : std::uint8_t { BEGIN=1, ACCEPT=2, KEEPALIVE=3, STOP=4, UNSUPPORTED=5, BUSY=6 };

struct Control {
    std::uint8_t schema = SCHEMA;
    ControlOp op = ControlOp::BEGIN;
    std::uint8_t flags = 0;
    std::uint32_t watchId = 0, boardStream = 0, roundEpoch = 0;
    std::uint32_t features = 0, budgetKbps = 0, playDelayMs = 1500;
};

inline void Put16(std::uint8_t* p, std::uint16_t v) { p[0]=(std::uint8_t)v; p[1]=(std::uint8_t)(v>>8); }
inline void Put32(std::uint8_t* p, std::uint32_t v) { for (int i=0;i<4;i++) p[i]=(std::uint8_t)(v>>(i*8)); }
inline std::uint16_t Get16(const std::uint8_t* p) { return (std::uint16_t)p[0] | ((std::uint16_t)p[1]<<8); }
inline std::uint32_t Get32(const std::uint8_t* p) { std::uint32_t v=0; for (int i=0;i<4;i++) v|=(std::uint32_t)p[i]<<(i*8); return v; }

inline bool EncodeControl(const Control& c, std::vector<std::uint8_t>& out) {
    if (c.schema != SCHEMA || c.op < ControlOp::BEGIN || c.op > ControlOp::BUSY || !c.watchId || !c.boardStream) return false;
    out.assign(28, 0); out[0]=c.schema; out[1]=(std::uint8_t)c.op; out[2]=c.flags;
    Put32(out.data()+4,c.watchId); Put32(out.data()+8,c.boardStream); Put32(out.data()+12,c.roundEpoch);
    Put32(out.data()+16,c.features); Put32(out.data()+20,c.budgetKbps); Put32(out.data()+24,c.playDelayMs);
    return true;
}
inline bool DecodeControl(const std::uint8_t* p, int n, Control& c) {
    if (!p || n != 28) return false; Control x; x.schema=p[0]; x.op=(ControlOp)p[1]; x.flags=p[2];
    x.watchId=Get32(p+4); x.boardStream=Get32(p+8); x.roundEpoch=Get32(p+12); x.features=Get32(p+16); x.budgetKbps=Get32(p+20); x.playDelayMs=Get32(p+24);
    if (x.schema!=SCHEMA || x.op<ControlOp::BEGIN || x.op>ControlOp::BUSY || !x.watchId || !x.boardStream || x.playDelayMs<1000 || x.playDelayMs>2000) return false;
    c=x; return true;
}

struct Frame {
    std::uint8_t src=0,dst=0,schema=SCHEMA,flags=0; std::uint32_t watchId=0,boardStream=0,frameSeq=0,sourceTick=0,roundEpoch=0;
    std::uint16_t partIndex=0,partCount=0; std::uint32_t encodedBytes=0,rawBytes=0,crc32=0; const std::uint8_t* data=nullptr; int dataSize=0;
};
inline bool Validate(const Frame& f) {
    return f.schema==SCHEMA && f.src && f.dst && f.src!=f.dst && f.watchId && f.boardStream && f.frameSeq && f.partCount && f.partCount<=MAX_PARTS && f.partIndex<f.partCount && f.data && f.dataSize>0 && f.dataSize<=FRAME_DATA && f.encodedBytes==(std::uint32_t)f.dataSize && f.rawBytes && f.rawBytes<=MAX_RAW && f.encodedBytes<=MAX_ENCODED;
}
inline bool EncodeFrame(const Frame& f, std::vector<std::uint8_t>& out) {
    if (!Validate(f)) return false; out.assign(FRAME_HEADER+f.dataSize,0); auto* p=out.data(); p[0]=f.src;p[1]=f.dst;p[2]=f.schema;p[3]=f.flags;
    Put32(p+4,f.watchId);Put32(p+8,f.boardStream);Put32(p+12,f.frameSeq);Put32(p+16,f.sourceTick);Put32(p+20,f.roundEpoch);Put16(p+24,f.partIndex);Put16(p+26,f.partCount);Put32(p+28,f.encodedBytes);Put32(p+32,f.rawBytes);Put32(p+36,f.crc32);std::memcpy(p+FRAME_HEADER,f.data,(std::size_t)f.dataSize);return true;
}
inline bool DecodeFrame(const std::uint8_t* p, int n, Frame& f) {
    if (!p || n<FRAME_HEADER || n>MAX_PAYLOAD) return false; Frame x; x.src=p[0];x.dst=p[1];x.schema=p[2];x.flags=p[3];x.watchId=Get32(p+4);x.boardStream=Get32(p+8);x.frameSeq=Get32(p+12);x.sourceTick=Get32(p+16);x.roundEpoch=Get32(p+20);x.partIndex=Get16(p+24);x.partCount=Get16(p+26);x.encodedBytes=Get32(p+28);x.rawBytes=Get32(p+32);x.crc32=Get32(p+36);x.data=p+FRAME_HEADER;x.dataSize=n-FRAME_HEADER; if(!Validate(x))return false; if(x.encodedBytes!=(std::uint32_t)x.dataSize)return false; f=x; return true;
}
}
