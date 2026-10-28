#pragma once
#include "ObserverDrawScene.h"
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
 DrawScene scene;
 Identity requested,visible;
 uint64_t instance=0,receivedAt=0,serial=0,lastCapture=0,captureInterval=500,lastDirectAt=0;
 bool have=false;
 void Clear(){scene.Clear();requested=visible={};have=false;lastDirectAt=0;}
public:
 explicit BoardView(LawnApp*app):scene(app){static std::atomic<uint64_t> ids{0};instance=++ids;}
 bool CanDraw(NetSession*s)const{
  return s&&s->IsWatchingV2()&&have&&visible.Valid()&&s->CanWatchSeat(visible.seat)&&visible.generation==s->ObserverEpoch()&&
   (!s->mCoopPeers[visible.seat]||visible.player==s->mCoopPeers[visible.seat])&&
   visible.seat!=s->GetLocalSeat()&&(!s->mCoopToken||visible.player!=s->mCoopToken);
 }
 void Update(Board*b,NetSession*s){
  const auto now=Milliseconds();s->SetObserverBoard(instance,b->mApp->mGameScene==GameScenes::SCENE_PLAYING);
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
    if(body.src!=target||body.dst!=s->GetLocalSeat()||body.watch!=next.subscription||body.stream!=next.run||body.epoch!=next.generation)continue;
    // A late one-shot reply must not rewind a working direct stream.
    if(!body.direct&&lastDirectAt&&now-lastDirectAt<5000)continue;
    DrawFrame frame;
    if(!DecodeDrawFrame(body.data.data(),body.data.size(),frame)||!(frame.flags&4))continue;
    frame.identity=next;frame.sequence=body.sequence;
    // Prepare is transactional: a rejected frame cannot discard the visible one.
    if(!scene.Prepare(frame))continue;
    visible=next;receivedAt=now;have=true;++serial;
    if(body.direct)lastDirectAt=now;
   }
  }
  if(have)scene.SetPredictionAge(double(now-receivedAt)/1000.0);
  if(!s->ObserverCanCapture()||(!s->ObserverNeedsInitialCapture()&&now-lastCapture<captureInterval))return;
  lastCapture=now;DrawFrame frame;frame.flags=4;frame.identity={1,1,1,1,s->GetLocalSeat()};frame.sequence=1;
  std::vector<uint8_t> bytes;const auto start=Milliseconds();
  if(CaptureDrawFrame(b,frame)&&EncodeDrawFrame(frame,bytes))s->SendObserverDisplay(bytes,uint32_t(now));
  else{s->ObserverCaptureFailed();fprintf(stderr,"[observer] source capture rejected\n");}
  captureInterval=std::clamp<uint64_t>((Milliseconds()-start)*100,500,2000);
 }
 uint64_t VisibleSequence()const{return have?serial:0;}
 bool Draw(Sexy::Graphics*g,NetSession*s){
  if(!CanDraw(s))return false;
  scene.SetPredictionAge(double(Milliseconds()-receivedAt)/1000.0);scene.Draw(g);
  g->SetColor(Sexy::Color(0,0,0,150));g->FillRect(0,0,395,58);
  auto name=s->GetSeatName(visible.seat);if(name.empty())name="P"+std::to_string(visible.seat);
  SeatUi::Text(g,ModText::Tr("正在观看：","Watching: ")+name,10,33,380,18,Sexy::Color(245,230,165),true);
  return true;
 }
};
}
