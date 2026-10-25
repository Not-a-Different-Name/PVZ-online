#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>

namespace ObserverDirect {
constexpr uint16_t CONTROL_TYPE=0xF024;
constexpr uint32_t REVISION=2;
constexpr int PACKET_SIZE=32,ACK_SIZE=24,OFFER_SIZE=26,BOUND_OFFER_SIZE=42;
constexpr int PROBE_INTERVAL_MS=150,PROBE_MAX_SENDS=20,HELLO_INTERVAL_MS=500,HELLO_MAX_SENDS=6,MAX_PROBES=4;
constexpr size_t DATAGRAM_SIZE=1200,DATA_HEADER=64,DATA_BYTES=DATAGRAM_SIZE-DATA_HEADER,MAX_FRAME=65536;
enum UDPOp:uint8_t{UDP_HELLO=1,UDP_HELLO_ACK=2,UDP_PROBE=3,UDP_PROBE_ACK=4,UDP_PING=5,UDP_PONG=6,UDP_DATA=7,UDP_FRAME_ACK=8,UDP_STOP=9};
enum CtrlOp:uint8_t{OP_CAPABLE=1,OP_BEGIN=2,OP_RESULT=3};
inline void Put16(uint8_t*p,uint16_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);}
inline void Put32(uint8_t*p,uint32_t v){for(int i=0;i<4;++i)p[i]=uint8_t(v>>(i*8));}
inline uint16_t Get16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t Get32(const uint8_t*p){uint32_t v=0;for(int i=0;i<4;++i)v|=uint32_t(p[i])<<(i*8);return v;}
inline void PutPacket(uint8_t*p,UDPOp op,const uint8_t*token,uint32_t seq,uint32_t ip,uint16_t port){std::memset(p,0,PACKET_SIZE);std::memcpy(p,"PZP2",4);p[4]=op;if(token)std::memcpy(p+5,token,16);Put32(p+21,seq);Put32(p+25,ip);Put16(p+29,port);}
struct Packet{bool ok=false;UDPOp op=UDP_HELLO;uint8_t token[16]{};uint32_t seq=0,ip=0;uint16_t port=0;};
inline Packet ParsePacket(const uint8_t*p,int n){Packet r;if(!p||n!=32||std::memcmp(p,"PZP2",4)||p[4]<1||p[4]>9||p[4]==UDP_DATA||p[31])return r;r.ok=true;r.op=UDPOp(p[4]);std::memcpy(r.token,p+5,16);r.seq=Get32(p+21);r.ip=Get32(p+25);r.port=Get16(p+29);return r;}
struct CapableAck{bool ok=false;uint16_t udpPort=0,flags=0;uint8_t token[16]{};};
inline bool ParseCapableAck(const uint8_t*p,int n,CapableAck*out){if(!p||!out||n!=24||p[0]!=1||!(p[7]&1)||Get32(p+1)!=REVISION||!Get16(p+5))return false;out->ok=true;out->udpPort=Get16(p+5);out->flags=p[7];std::memcpy(out->token,p+8,16);return true;}
struct Context{uint32_t watch=0,stream=0,epoch=0,term=0;uint8_t src=0,dst=0;bool Valid()const{return watch&&stream&&epoch&&src>=1&&src<=6&&dst>=1&&dst<=6&&src!=dst;}bool operator==(const Context&o)const{return watch==o.watch&&stream==o.stream&&epoch==o.epoch&&term==o.term&&src==o.src&&dst==o.dst;}};
struct Offer{bool ok=false;uint8_t peerSeat=0;uint32_t ip=0;uint16_t port=0,ttl=0;uint8_t token[16]{};Context context;};
inline bool ParseOffer(const uint8_t*p,int n,Offer*out){if(!p||!out||(n!=26&&n!=42)||p[0]!=2||p[1]<1||p[1]>6||!Get32(p+2)||!Get16(p+6)||!Get16(p+24)||Get16(p+24)>120)return false;out->ok=true;out->peerSeat=p[1];out->ip=Get32(p+2);out->port=Get16(p+6);out->ttl=Get16(p+24);std::memcpy(out->token,p+8,16);if(n==42){out->context.watch=Get32(p+26);out->context.stream=Get32(p+30);out->context.epoch=Get32(p+34);out->context.term=Get32(p+38);}return true;}
inline void PutResult(uint8_t*p,uint8_t seat,bool ok,uint32_t rtt){p[0]=3;p[1]=seat;p[2]=ok?1:0;Put16(p+3,uint16_t(std::min(rtt,60000u)));}
struct Frame{Context context;uint32_t sequence=0,tick=0;std::vector<uint8_t> data;};
class DirectLink{
public:
 struct Result{uint8_t peerSeat=0;bool ok=false;uint32_t rttMs=0;uint8_t token[16]{};};
 DirectLink();~DirectLink();DirectLink(const DirectLink&)=delete;DirectLink&operator=(const DirectLink&)=delete;
 bool Open();bool Opened()const;void Close();void ClearPeers();void Cancel(uint32_t watch);void Cancel(const Context&);
 bool StartHello(const char*,uint16_t,const uint8_t*,uint64_t);
 uint16_t LocalPort()const;static bool Token(uint8_t*);
 bool StartLanProbe(uint8_t,const char*,uint16_t,const uint8_t*,uint64_t,const Context&,bool);
 bool HasEndpoint()const;uint32_t PublicIp()const;uint16_t PublicPort()const;
 bool StartProbe(uint8_t,uint32_t,uint16_t,const uint8_t*,uint64_t,const Context*context=nullptr,bool producer=false,uint16_t ttl=120);
 void Pump(uint64_t,std::vector<Result>*);
 bool Ready(const Context&)const;bool Writable(const Context&)const;uint64_t Interval(const Context&)const;
 bool Queue(const Context&,const std::vector<uint8_t>&,uint32_t tick,uint64_t now);
 bool Take(Frame&);size_t ActiveProducers()const;
private:struct State;std::unique_ptr<State> state;
};
}
