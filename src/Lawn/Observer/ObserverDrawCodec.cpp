#include "ObserverDrawScene.h"
#include <cstring>
#include <limits>
#ifdef __ANDROID__
#include <zlib.h>
#else
#include "zlib/zlib.h"
#endif
namespace Observer {
namespace {
constexpr size_t RawLimit=128*1024,EncodedLimit=64*1024;
struct Writer{std::vector<uint8_t> b;bool ok=true;
 void Put(uint64_t n,int size=4){if(b.size()+size>RawLimit){ok=false;return;}for(int i=0;i<size;++i)b.push_back(uint8_t(n>>(i*8)));}
 void Float32(float f){uint32_t n;memcpy(&n,&f,4);Put(n);}
 void Float(double f){uint64_t n;static_assert(sizeof(f)==sizeof(n));memcpy(&n,&f,8);Put(n,8);}
};
struct Reader{const std::vector<uint8_t>&b;size_t at=0;bool ok=true;
 uint64_t Get(int size=4){if(!ok||size_t(size)>b.size()-at){ok=false;return 0;}uint64_t n=0;for(int i=0;i<size;++i)n|=uint64_t(b[at++])<<(i*8);return n;}
 float Float32(){uint32_t n=uint32_t(Get());float f;memcpy(&f,&n,4);return f;}
 double Float(){uint64_t n=Get(8);double f;memcpy(&f,&n,8);return f;}
};
bool Pack(const std::vector<uint8_t>&in,std::vector<uint8_t>&out){
 if(in.empty()||in.size()>RawLimit)return false;
 out.resize(compressBound(uLong(in.size()))+5);out[0]=2;
 for(int i=0;i<4;++i)out[i+1]=uint8_t(in.size()>>(i*8));
 uLongf count=uLongf(out.size()-5);int status=compress2(out.data()+5,&count,in.data(),uLong(in.size()),Z_BEST_SPEED);
 if(status!=Z_OK||count+5>EncodedLimit){out.clear();return false;}out.resize(count+5);return true;
}
bool Unpack(const uint8_t*p,size_t n,std::vector<uint8_t>&out){
 if(!p||n<6||n>EncodedLimit||p[0]!=2)return false;uint32_t wanted=0;for(int i=0;i<4;++i)wanted|=uint32_t(p[i+1])<<(i*8);
 if(!wanted||wanted>RawLimit)return false;out.resize(wanted);
 z_stream z{};z.next_in=const_cast<Bytef*>(p+5);z.avail_in=uInt(n-5);z.next_out=out.data();z.avail_out=wanted;
 if(inflateInit(&z)!=Z_OK)return false;int status=inflate(&z,Z_FINISH);
 bool valid=status==Z_STREAM_END&&z.total_in==n-5&&z.total_out==wanted;inflateEnd(&z);
 if(!valid)out.clear();return valid;
}

}
bool EncodeDrawFrame(const DrawFrame&f,std::vector<uint8_t>&out)try{
 if(f.assets.size()>512||f.commands.empty()||f.commands.size()>8192)return false;
 Writer w;w.Put(0x31445750);w.Put(2,2);w.Put(f.flags,1);w.Put(0,1);w.Put(f.sequence,8);w.Put(f.wave,2);w.Put(f.total,2);w.Put(f.assets.size(),2);w.Put(f.commands.size(),2);
 for(auto&k:f.assets){w.Put(k.kind,1);w.Put(k.id);w.Put(k.item,2);w.Put(k.filters,2);}
 for(auto&d:f.commands){if(d.vertices.size()>16384||d.text.size()>1024)return false;
  w.Put(uint8_t(d.op),1);w.Put(d.mode,1);w.Put(d.flags,1);w.Put(0,1);w.Put(d.asset,2);w.Put(d.color);for(auto n:d.src)w.Put(uint32_t(n));for(auto n:d.dst)w.Put(uint32_t(n));for(auto n:d.clip)w.Put(uint32_t(n));for(auto n:d.values)w.Float(n);
  w.Put(d.vertices.size(),2);w.Put(d.text.size(),2);
  for(auto&v:d.vertices){w.Float32(v.x);w.Float32(v.y);w.Float32(v.u);w.Float32(v.v);w.Put(v.color);}
  for(auto c:d.text)w.Put(uint8_t(c),1);if(!w.ok)return false;
 }return w.ok&&Pack(w.b,out);
}catch(const std::bad_alloc&){out.clear();return false;}
bool DecodeDrawFrame(const uint8_t*p,size_t n,DrawFrame&out)try{
 std::vector<uint8_t> raw;if(!Unpack(p,n,raw))return false;Reader r{raw};DrawFrame f;
 if(r.Get()!=0x31445750||r.Get(2)!=2)return false;f.flags=uint8_t(r.Get(1));if(r.Get(1))return false;
 f.sequence=r.Get(8);f.wave=uint16_t(r.Get(2));f.total=uint16_t(r.Get(2));size_t assets=r.Get(2),commands=r.Get(2);
 if(!r.ok||assets>512||!commands||commands>8192||assets*9+commands*126>raw.size()-r.at)return false;
 f.assets.reserve(assets);f.commands.reserve(commands);
 for(size_t j=0;j<assets;++j){AssetKey k;k.kind=uint8_t(r.Get(1));k.id=uint32_t(r.Get());k.item=uint16_t(r.Get(2));k.filters=uint16_t(r.Get(2));f.assets.push_back(k);}
 size_t totalVertices=0,totalText=0;
 for(size_t j=0;j<commands;++j){DrawCommand d;d.op=DrawOp(r.Get(1));d.mode=uint8_t(r.Get(1));d.flags=uint8_t(r.Get(1));if(r.Get(1))return false;d.asset=uint16_t(r.Get(2));d.color=uint32_t(r.Get());
  for(auto&v:d.src)v=int32_t(r.Get());for(auto&v:d.dst)v=int32_t(r.Get());for(auto&v:d.clip)v=int32_t(r.Get());for(auto&v:d.values)v=r.Float();
  size_t vertices=r.Get(2),text=r.Get(2);totalVertices+=vertices;totalText+=text;
  if(!r.ok||totalVertices>16384||text>1024||totalText>4096||vertices*20+text>raw.size()-r.at)return false;
  d.vertices.resize(vertices);for(auto&v:d.vertices){v.x=r.Float32();v.y=r.Float32();v.u=r.Float32();v.v=r.Float32();v.color=uint32_t(r.Get());}
  d.text.reserve(text);for(size_t k=0;k<text;++k)d.text.push_back(char(r.Get(1)));f.commands.push_back(std::move(d));
 }
 if(!r.ok||r.at!=raw.size())return false;out=std::move(f);return true;
}catch(const std::bad_alloc&){return false;}
}
