#pragma once
#include <cstdint>
#include <utility>
#include <array>
#include <cstring>
#include <functional>
#include <limits>
namespace Sexy {class Image;}
#include <vector>
#include <string>
#include <type_traits>
#include <cmath>
#include <stdexcept>
namespace ObserverWorld {
// Explicit little-endian values; no native structs, pointers, padding or save ABI.
class Wire {
 const std::vector<uint8_t>* input=nullptr;size_t at=0;
public:
 std::vector<uint8_t> bytes;bool reading=false,ok=true;
 std::function<void(Sexy::Image*&)> image;
 void Value(Sexy::Image*&v){if(image)image(v);else ok=false;}
 void Value(std::vector<unsigned char>&v){uint32_t n=uint32_t(v.size());Value(n);if(!ok||n>1024*1024){ok=false;return;}if(reading){if(n>input->size()-at){ok=false;return;}v.assign(input->begin()+at,input->begin()+at+n);at+=n;}else{if(bytes.size()+n>8*1024*1024){ok=false;return;}bytes.insert(bytes.end(),v.begin(),v.end());}}
 template<class T,size_t N,size_t M>void Waves(T(&v)[N][M],int&waves){uint32_t rows=reading?0:uint32_t(waves);Value(rows);if(!ok||rows>N){ok=false;return;}if(reading){for(auto&row:v)for(auto&x:row)x=T(-1);waves=int(rows);}for(unsigned row=0;row<rows;++row){uint32_t n=0;if(!reading)while(n<M&&int(v[row][n])!=-1)++n;Value(n);if(!ok||n>M){ok=false;return;}for(uint32_t i=0;i<n&&ok;++i){uint32_t type=reading?0:uint32_t(v[row][i]);Bits32(type);if(type>=60){ok=false;return;}if(reading)v[row][i]=T(type);}}}
 void Bits32(uint32_t&v){uint64_t n=v;Bits(n,1);if(reading)v=uint32_t(n);}
 template<class T,size_t N>void Sparse(T(&v)[N]){uint32_t n=0;if(!reading)for(auto x:v)if(x!=T{})++n;Value(n);if(!ok||n>N){ok=false;return;}if(reading){for(auto&x:v)x=T{};uint32_t last=0;for(uint32_t j=0;j<n&&ok;++j){uint32_t at=0;Value(at);if(at>=N||(j&&at<=last)){ok=false;return;}last=at;Value(v[at]);}}else for(uint32_t at=0;at<N;++at)if(v[at]!=T{}){Value(at);Value(v[at]);}}
 explicit Wire()=default;explicit Wire(const std::vector<uint8_t>&b):input(&b),reading(true){}
 size_t Position()const{return at;}
 bool End()const{return ok&&(!reading||at==input->size());}
 void Bits(uint64_t&n,unsigned count){if(!ok)return;if(reading){if(count>input->size()-at){ok=false;return;}n=0;for(unsigned i=0;i<count;++i)n|=uint64_t((*input)[at++])<<(i*8);}else{if(bytes.size()+count>8*1024*1024){ok=false;return;}for(unsigned i=0;i<count;++i)bytes.push_back(uint8_t(n>>(i*8)));}}
 // Field wire types are explicit; Android unsigned clocks must not change zigzag coding.
 template<class W,class T>void Scalar(T&v){static_assert(std::is_arithmetic_v<W>&&std::is_arithmetic_v<T>);W n{};if(!reading){if constexpr(std::is_integral_v<W>&&std::is_integral_v<T>&&!std::is_same_v<W,bool>&&!std::is_same_v<T,bool>){if(!std::in_range<W>(v)){ok=false;return;}}n=static_cast<W>(v);}Value(n);if(reading&&ok){if constexpr(std::is_integral_v<W>&&std::is_integral_v<T>&&!std::is_same_v<W,bool>&&!std::is_same_v<T,bool>){if(!std::in_range<T>(n)){ok=false;return;}}v=static_cast<T>(n);}}
 template<class W,class T,size_t N>void Scalar(T(&v)[N]){for(auto&x:v)Scalar<W>(x);}
 template<class W,class T,size_t N>void Scalar(std::array<T,N>&v){for(auto&x:v)Scalar<W>(x);}
 template<class T>void Value(T&v){if(!ok)return;
  if constexpr(std::is_enum_v<T>){int32_t n=int32_t(v);Value(n);if(reading)v=T(n);}
  else if constexpr(std::is_integral_v<T>){
   uint64_t n=0;
   if(!reading){if constexpr(std::is_signed_v<T>)n=(uint64_t(int64_t(v))<<1)^uint64_t(-int64_t(v<0));else n=uint64_t(v);}
   if(reading){for(unsigned i=0;i<10;++i){uint64_t byte=0;Bits(byte,1);if(!ok)return;if(i==9&&(byte&254)){ok=false;return;}n|=(byte&127)<<(7*i);if(!(byte&128)){if(i&&!(byte&127)){ok=false;return;}break;}if(i==9){ok=false;return;}}}
   else{do{uint64_t byte=n&127;n>>=7;if(n)byte|=128;Bits(byte,1);}while(n&&ok);return;}
   if constexpr(std::is_signed_v<T>){int64_t value=int64_t(n>>1)^-int64_t(n&1);if(value<std::numeric_limits<T>::min()||value>std::numeric_limits<T>::max()){ok=false;return;}v=T(value);}
   else{if(n>std::numeric_limits<T>::max()){ok=false;return;}v=T(n);}
  }
  else if constexpr(std::is_floating_point_v<T>){uint64_t n=0;static_assert(sizeof(T)==4||sizeof(T)==8);if(!reading){std::memcpy(&n,&v,sizeof(T));if constexpr(sizeof(T)==4){if(std::isfinite(v)&&std::abs(v)<=1000000&&uint32_t(n&0x7f800000u)!=0)n=uint32_t((uint32_t(n)+4096u)&0xffffe000u);}}Bits(n,sizeof(T));if(reading)std::memcpy(&v,&n,sizeof(T));if(!std::isfinite(v))ok=false;}
  else Fields(*this,v);
 }
 template<class T,size_t N>void Value(T(&a)[N]){for(auto&v:a)Value(v);}
 template<class T,size_t N>void Value(std::array<T,N>&a){for(auto&v:a)Value(v);}
 template<class T>void Value(std::vector<T>&v){uint32_t n=uint32_t(v.size());Value(n);if(n>4096){ok=false;return;}if(reading)v.resize(n);for(auto&x:v)Value(x);}
 void Value(std::string&v){uint32_t n=uint32_t(v.size());Value(n);if(n>20000){ok=false;return;}if(reading){if(n>input->size()-at){ok=false;return;}v.assign(reinterpret_cast<const char*>(input->data()+at),n);at+=n;}else{if(bytes.size()+n>8*1024*1024){ok=false;return;}bytes.insert(bytes.end(),v.begin(),v.end());}}
};
}
