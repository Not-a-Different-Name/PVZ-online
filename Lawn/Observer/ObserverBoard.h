#pragma once
#include "ObserverDrawScene.h"
#include "World.h"
#include "../Online/NetSession.h"
#include "../Board.h"
#include "../../LawnApp.h"
#include "../Widget/OnlineUiDraw.h"
#include <chrono>
#include <atomic>
#include <cstdio>
namespace Observer {
inline uint64_t Milliseconds(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
class BoardView {
 DrawScene scene;LawnApp* app;std::unique_ptr<ObserverWorld::World> world;bool worldVisible=false;uint64_t lastAdvance=0;size_t frameBytes=0;
 Identity requested,visible;
 uint64_t instance=0,receivedAt=0,serial=0,lastCapture=0,captureInterval=500,lastDirectAt=0;
 bool have=false;
 void Clear(){world.reset();worldVisible=false;lastAdvance=0;scene.Clear();requested=visible={};have=false;lastDirectAt=0;}
public:
 explicit BoardView(LawnApp*app):scene(app),app(app){static std::atomic<uint64_t> ids{0};instance=++ids;}
 bool CanDraw(NetSession*s)const{
  return s&&s->IsWatchingV2()&&have&&visible.Valid()&&s->CanWatchSeat(visible.seat)&&visible.generation==s->ObserverEpoch()&&
   (!s->mCoopPeers[visible.seat]||visible.player==s->mCoopPeers[visible.seat])&&
   visible.seat!=s->GetLocalSeat()&&(!s->mCoopToken||visible.player!=s->mCoopToken);
 }
 void Update(Board*b,NetSession*s){
  const auto now=Milliseconds();if(b)s->SetObserverBoard(instance,b->mApp->mGameScene==GameScenes::SCENE_PLAYING);
  if(!s->IsWatchingV2())Clear();
  else if(!s->CanWatchSeat(s->GetWatchTargetSeat())){s->EndWatch(true);Clear();}
  else if(s->ObserverAccepted()){
   const uint8_t target=s->GetWatchTargetSeat();
   Identity next{s->ObserverWatch(),s->mCoopPeers[target]?s->mCoopPeers[target]:uint64_t(target),s->ObserverStream(),s->ObserverEpoch(),target};
   if(!(next==requested)){
    // Keep the last valid teammate picture while a refresh or switch is prepared.
    if(visible.Valid()&&visible.generation!=next.generation)Clear();
    requested=next;lastDirectAt=0;
   }
   ObserverTransfer::Complete body;
   while(s->TakeObserverDisplay(body)){
    if(body.src!=target||body.dst!=s->ObserverRoute()||body.watch!=next.subscription||body.stream!=next.run||body.epoch!=next.generation)continue;
    // A late one-shot reply must not rewind a working direct stream.
    if(!body.direct&&lastDirectAt&&now-lastDirectAt<5000)continue;
    if(!body.data.empty()&&body.data[0]==5){
     if(s->GetSeatBuild(target)<NetSession::ObserverWorldBuild)continue;
     if(!world)world=std::make_unique<ObserverWorld::World>(app);
     if(!world->Restore(body.data))continue;
     // Room spectators may never have loaded this scene's delayed resources.
     world->Inspect()->LoadBackgroundImages();
     worldVisible=true;scene.Clear();lastAdvance=now;
    }else{
     DrawFrame frame;if(!DecodeDrawFrame(body.data.data(),body.data.size(),frame)||!(frame.flags&4))continue;
     frame.identity=next;frame.sequence=body.sequence;if(!scene.Prepare(frame))continue;
     worldVisible=false;world.reset();
    }
    frameBytes=body.data.size();s->ObserverDisplayReady(body.watch);
    visible=next;receivedAt=now;have=true;++serial;
    if(body.direct)lastDirectAt=now;
   }
  }
  if(have){if(worldVisible&&world){world->Advance(unsigned(std::min<uint64_t>(now-lastAdvance,200)));lastAdvance=now;}else scene.SetPredictionAge(double(now-receivedAt)/1000.0);}
  if(!b||!s->ObserverCanCapture()||(!s->ObserverNeedsInitialCapture()&&now-lastCapture<captureInterval))return;
  lastCapture=now;DrawFrame frame;frame.flags=4;frame.identity={1,1,1,1,s->GetLocalSeat()};frame.sequence=1;
  std::vector<uint8_t> bytes;const auto start=Milliseconds();
  const bool captured=s->ObserverNeedsWorldSnapshot()?ObserverWorld::Capture(b,bytes):(CaptureDrawFrame(b,frame)&&EncodeDrawFrame(frame,bytes));
  if(captured)s->SendObserverDisplay(bytes,uint32_t(now));
  else{s->ObserverCaptureFailed();fprintf(stderr,"[observer] source capture rejected\n");}
  captureInterval=std::clamp<uint64_t>((Milliseconds()-start)*100,500,2000);
 }
 const Board* LocalBattle()const{return worldVisible&&world?world->Inspect():nullptr;}
 size_t FrameBytes()const{return frameBytes;}
 uint64_t VisibleSequence()const{return have?serial:0;}
 bool Draw(Sexy::Graphics*g,NetSession*s){
  if(!CanDraw(s))return false;
  if(worldVisible&&world)world->Draw(g);else{scene.SetPredictionAge(double(Milliseconds()-receivedAt)/1000.0);scene.Draw(g);}
  g->SetColor(Sexy::Color(0,0,0,150));g->FillRect(8,551,334,42);
  auto name=s->GetSeatName(visible.seat);if(name.empty())name="P"+std::to_string(visible.seat);
  SeatUi::Text(g,ModText::Tr("正在观看：","Watching: ")+name,14,577,322,18,Sexy::Color(245,230,165),true);
  return true;
 }
};
}
