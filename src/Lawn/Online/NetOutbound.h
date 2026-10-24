#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>
#include <algorithm>
// A single network worker owns all socket writes. UI threads only copy into
// bounded queues. Unsent optional frames can be replaced in their entirety.
class NetOutbound {
public:
 using Bytes=std::vector<uint8_t>;using Frame=std::vector<Bytes>;
 bool Critical(const void*p,int n){if(!p||n<=0||n>270)return false;std::lock_guard<std::mutex> lock(mutex);if(critical.size()>=256||criticalBytes+size_t(n)>64*1024)return false;critical.emplace_back((const uint8_t*)p,(const uint8_t*)p+n);criticalBytes+=n;return true;}
 bool Display(uint8_t viewer,Frame frame){if(viewer<1||viewer>6||frame.empty()||frame.size()>304)return false;size_t size=0;for(auto&b:frame){if(b.empty()||b.size()>270)return false;size+=b.size();}if(size>96*1024)return false;std::lock_guard<std::mutex> lock(mutex);if(at[viewer]>0)return false;optional[viewer]=std::move(frame);at[viewer]=0;return true;}
 bool Pop(Bytes&out,bool&display,uint64_t now){std::lock_guard<std::mutex> lock(mutex);
  if(!critical.empty()){out=std::move(critical.front());critical.pop_front();criticalBytes-=out.size();criticalInFlight=true;display=false;return true;}
  if(now<budgetAt||now-budgetAt>=1000){budgetAt=now;spent.fill(0);total=0;}if(total>=128*1024)return false;
  for(int i=0;i<6;++i){next=uint8_t(next%6+1);auto&f=optional[next];if(spent[next]>=24*1024||at[next]>=f.size())continue;out=std::move(f[at[next]++]);spent[next]+=out.size();total+=out.size();display=true;if(at[next]==f.size()){f.clear();at[next]=0;}return true;}return false;
 }
 void Completed(bool display){if(display)return;std::lock_guard<std::mutex> lock(mutex);criticalInFlight=false;}
 bool HasCritical()const{std::lock_guard<std::mutex> lock(mutex);return !critical.empty()||criticalInFlight;}
 size_t DisplayBytes()const{std::lock_guard<std::mutex> lock(mutex);size_t n=0;for(int s=1;s<=6;++s)for(size_t i=at[s];i<optional[s].size();++i)n+=optional[s][i].size();return n;}
 void ClearDisplay(uint8_t viewer=0){std::lock_guard<std::mutex> lock(mutex);for(int s=1;s<=6;++s)if(!viewer||s==viewer){optional[s].clear();at[s]=0;}}
 void Clear(){std::lock_guard<std::mutex> lock(mutex);critical.clear();criticalBytes=0;criticalInFlight=false;for(auto&f:optional)f.clear();at.fill(0);spent.fill(0);total=0;budgetAt=0;}
private:mutable std::mutex mutex;std::deque<Bytes> critical;size_t criticalBytes=0;bool criticalInFlight=false;
 std::array<Frame,7>optional;std::array<size_t,7>at{};uint8_t next=0;uint64_t budgetAt=0;std::array<size_t,7>spent{};size_t total=0;
};
