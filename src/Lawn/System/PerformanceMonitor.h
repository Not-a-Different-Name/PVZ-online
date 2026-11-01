#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#ifdef __ANDROID__
#include <unistd.h>
#else
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <psapi.h>
#endif

// Local opt-in diagnostics. Disabled scopes do not read the clock or touch disk.
// The writer owns no game pointers and snapshots a fixed-size sample window.
namespace PerformanceMonitor {
using Clock=std::chrono::steady_clock;
enum Phase {AppUpdate,AppDraw,FrameInterval,ObserverCapture,ObserverRestore,ObserverAdvance,ObserverDraw,PhaseCount};
inline const char* Name(Phase p){static const char* n[]={"app_update","app_draw","frame_interval","observer_capture","observer_restore","observer_advance","observer_draw"};return n[p];}
struct Samples {
 uint64_t count=0,total=0,maximum=0,over16=0,over33=0,over50=0,over100=0;
 std::array<uint32_t,512> recent{};int plants=0,zombies=0,bullets=0,scene=0,watching=0,animations=0,particles=0,trails=0,attachments=0;
 void Add(uint64_t us){recent[count%recent.size()]=uint32_t(std::min<uint64_t>(us,UINT32_MAX));++count;total+=us;maximum=std::max(maximum,us);over16+=us>16667;over33+=us>33333;over50+=us>50000;over100+=us>100000;}
};
inline uint64_t ResidentBytes(){
#ifdef __ANDROID__
 unsigned long pages=0,unused=0;std::ifstream in("/proc/self/statm");in>>unused>>pages;return uint64_t(pages)*sysconf(_SC_PAGESIZE);
#else
 PROCESS_MEMORY_COUNTERS p{};p.cb=sizeof(p);return GetProcessMemoryInfo(GetCurrentProcess(),&p,sizeof(p))?p.WorkingSetSize:0;
#endif
}
struct State {
 std::atomic<bool> enabled{false};std::atomic<uint64_t> skipped{0};bool configured=false,stop=false;std::mutex mutex;std::condition_variable wake;std::thread writer;std::string path;
 std::array<Samples,PhaseCount> samples{};Clock::time_point lastPresented{};int scene=0,watching=0;
 ~State(){if(writer.joinable()){{std::lock_guard<std::mutex> lock(mutex);stop=true;}wake.notify_one();writer.join();}}
 void Flush()noexcept{try{
  std::array<Samples,PhaseCount> snapshot;{std::lock_guard<std::mutex> lock(mutex);snapshot=samples;for(auto&s:samples)s=Samples{};}
  if(std::none_of(snapshot.begin(),snapshot.end(),[](const Samples&s){return s.count!=0;}))return;
  const char* columns="monotonic_ms,phase,count,mean_ms,p50_ms,p95_ms,p99_ms,max_ms,over16,over33,over50,over100,rss_bytes,plants,zombies,bullets,scene,watching,animations,particles,trails,attachments,skipped_samples";
  std::error_code ec;bool rotate=std::filesystem::file_size(path,ec)>1024*1024&&!ec;
  if(!rotate&&std::filesystem::exists(path,ec)&&std::filesystem::file_size(path,ec)>0){std::array<char,512> first{};std::ifstream in(path);in.getline(first.data(),first.size());rotate=std::string(first.data())!=columns;}
  if(rotate){const auto previous=path+".previous";std::filesystem::remove(previous,ec);std::filesystem::rename(path,previous,ec);if(ec)return;}
  bool header=!std::filesystem::exists(path,ec)||std::filesystem::file_size(path,ec)==0;
  std::ofstream out(path,std::ios::app);if(!out)return;
  if(header)out<<columns<<'\n';
  auto now=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();const auto rss=ResidentBytes();const auto omitted=skipped.exchange(0,std::memory_order_relaxed);
  for(int p=0;p<PhaseCount;++p){auto&s=snapshot[p];if(!s.count)continue;size_t n=size_t(std::min<uint64_t>(s.count,s.recent.size()));std::sort(s.recent.begin(),s.recent.begin()+n);auto quantile=[&](double q){return s.recent[std::min(n-1,size_t(n*q))]/1000.0;};out<<now<<','<<Name(Phase(p))<<','<<s.count<<','<<double(s.total)/s.count/1000<<','<<quantile(.5)<<','<<quantile(.95)<<','<<quantile(.99)<<','<<s.maximum/1000.0<<','<<s.over16<<','<<s.over33<<','<<s.over50<<','<<s.over100<<','<<rss<<','<<s.plants<<','<<s.zombies<<','<<s.bullets<<','<<s.scene<<','<<s.watching<<','<<s.animations<<','<<s.particles<<','<<s.trails<<','<<s.attachments<<','<<omitted<<'\n';}
 }catch(...){}}
};
inline State& Get(){static State state;return state;}
inline bool Enabled(){return Get().enabled.load(std::memory_order_relaxed);}
inline void Shutdown()noexcept{auto&s=Get();s.enabled.store(false,std::memory_order_relaxed);try{if(s.writer.joinable()){{std::lock_guard<std::mutex> lock(s.mutex);s.stop=true;}s.wake.notify_one();s.writer.join();}s.configured=false;s.lastPresented=Clock::time_point{};}catch(...){}}
inline void Configure(const std::string& folder)noexcept{auto&s=Get();if(s.configured)return;s.configured=true;try{
 const char* env=std::getenv("PVZ_PERF_MONITOR");std::ifstream flag(folder+"performance-monitor.flag");if((!env||std::string(env)!="1")&&!flag)return;
 std::filesystem::create_directories(folder+"performance");s.path=folder+"performance/performance.csv";{std::lock_guard<std::mutex> lock(s.mutex);s.stop=false;s.lastPresented=Clock::time_point{};s.scene=s.watching=0;for(auto&window:s.samples)window=Samples{};}
 s.writer=std::thread([&s]{std::unique_lock<std::mutex> lock(s.mutex);for(;;){bool done=s.wake.wait_for(lock,std::chrono::seconds(5),[&s]{return s.stop;});lock.unlock();s.Flush();if(done)return;lock.lock();}});s.enabled.store(true,std::memory_order_relaxed);
 }catch(...){s.enabled.store(false,std::memory_order_relaxed);}}
inline void Context(int scene,bool watching){if(!Enabled())return;auto&s=Get();std::unique_lock<std::mutex> lock(s.mutex,std::try_to_lock);if(!lock.owns_lock()){s.skipped.fetch_add(1,std::memory_order_relaxed);return;}s.scene=scene;s.watching=watching;}
inline void Record(Phase phase,uint64_t us,int plants=0,int zombies=0,int bullets=0,int animations=0,int particles=0,int trails=0,int attachments=0){auto&s=Get();std::unique_lock<std::mutex> lock(s.mutex,std::try_to_lock);if(!lock.owns_lock()){s.skipped.fetch_add(1,std::memory_order_relaxed);return;}auto&p=s.samples[phase];p.Add(us);p.plants=plants;p.zombies=zombies;p.bullets=bullets;p.scene=s.scene;p.watching=s.watching;p.animations=animations;p.particles=particles;p.trails=trails;p.attachments=attachments;}
inline void Presented(){if(!Enabled())return;auto&s=Get();auto now=Clock::now();if(s.lastPresented!=Clock::time_point{})Record(FrameInterval,std::chrono::duration_cast<std::chrono::microseconds>(now-s.lastPresented).count());s.lastPresented=now;}
struct Scope {
 Phase phase;bool enabled;Clock::time_point start;int plants=0,zombies=0,bullets=0,animations=0,particles=0,trails=0,attachments=0;
 explicit Scope(Phase p):phase(p),enabled(Enabled()){if(enabled)start=Clock::now();}
 ~Scope(){if(enabled)Record(phase,std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count(),plants,zombies,bullets,animations,particles,trails,attachments);}
 void Objects(int p,int z,int b){plants=p;zombies=z;bullets=b;}
 void Effects(int a,int p,int t,int attached){animations=a;particles=p;trails=t;attachments=attached;}
 void Cancel(){enabled=false;}
};
}
