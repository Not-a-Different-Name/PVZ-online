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
 DrawScene scene;Identity identity;
 uint64_t instance=0,requestedAt=0,receivedAt=0,sequence=0,capturedAt=0,lastCapture=0;
 bool have=false;
public:
 explicit BoardView(LawnApp*app):scene(app){static std::atomic<uint64_t> ids{0};instance=++ids;}
 void Update(Board*b,NetSession*s){
  const auto now=Milliseconds();s->SetObserverBoard(instance,b->mApp->mGameScene==GameScenes::SCENE_PLAYING);
  if(s->IsWatchingV2()&&s->ObserverAccepted()){
   const uint8_t target=s->GetWatchTargetSeat();
   Identity next{s->ObserverWatch(),s->mCoopPeers[target]?s->mCoopPeers[target]:uint64_t(target),s->ObserverStream(),s->ObserverEpoch(),target};
   if(!(next==identity)){scene.Clear();identity=next;have=false;sequence=0;requestedAt=now;}
   ObserverTransfer::Complete body;
   while(s->TakeObserverDisplay(body)){
    DrawFrame frame;
    if(!DecodeDrawFrame(body.data.data(),body.data.size(),frame)||!(frame.flags&4)||!scene.Prepare(frame)){
     s->EndWatch(true);scene.Clear();identity={};have=false;break;
    }
    sequence=body.sequence;receivedAt=now;capturedAt=requestedAt;have=true;
   }
  }else if(identity.Valid()){scene.Clear();identity={};have=false;sequence=0;}
  if(have)scene.SetPredictionAge(double(now-receivedAt)/1000.0);
  if(now-lastCapture<100||!s->ObserverCanCapture())return;
  lastCapture=now;DrawFrame frame;frame.flags=4;frame.identity={1,1,1,1,s->GetLocalSeat()};frame.sequence=1;
  std::vector<uint8_t> bytes;
  if(CaptureDrawFrame(b,frame)&&EncodeDrawFrame(frame,bytes))s->SendObserverDisplay(bytes,uint32_t(now));
  else{s->ObserverCaptureFailed();fprintf(stderr,"[observer] one-shot snapshot unavailable; no continuous fallback\n");}
 }
 uint64_t VisibleSequence()const{return have?sequence:0;}
 bool Draw(Sexy::Graphics*g,NetSession*s){
  if(!s||!s->IsWatchingV2())return false;
  g->SetColor(Sexy::Color(15,20,18));g->FillRect(0,0,800,600);
  if(have)scene.Draw(g);
  else SeatUi::Text(g,ModText::Tr("正在获取队友快照…","Loading teammate snapshot..."),200,300,440,22,Sexy::Color(245,230,165),true);
  g->SetColor(Sexy::Color(0,0,0,180));g->FillRect(0,0,395,58);
  auto seconds=have?(Milliseconds()-capturedAt)/1000:0;
  auto label=std::string("P")+std::to_string(s->GetWatchTargetSeat())+ModText::Tr(" 本地推演 · "," Local prediction · ")+std::to_string(seconds)+ModText::Tr("秒前快照","s old");
  SeatUi::Text(g,label,10,33,380,18,Sexy::Color(245,230,165),true);
  if(have&&(scene.Flags()&1))SeatUi::Text(g,ModText::Tr("采样时已暂停","Paused at snapshot"),260,330,340,24,Sexy::Color(245,230,165),true);
  return true;
 }
};
}
