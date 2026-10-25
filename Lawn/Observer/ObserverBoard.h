#pragma once
#include "ObserverDrawScene.h"
#include "ObserverTimeline.h"
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
 DrawScene scene,liveScene;Timeline<DrawFrame,DrawScene> timeline;SourceClock sourceClock;Identity identity;
 uint64_t anchorAt=0,anchorSource=0,visibleLive=0,liveReceived=0,liveAgeAt=0,liveCaptureAt=0,captureInterval=500;
 bool directView=false;

 uint64_t instance=0,requestedAt=0,receivedAt=0,sequence=0,capturedAt=0,lastCapture=0;
 bool have=false;
public:
 explicit BoardView(LawnApp*app):scene(app),liveScene(app),timeline(liveScene,BufferConfig{1500,12,8*1024*1024,6000}){static std::atomic<uint64_t> ids{0};instance=++ids;}
 void Update(Board*b,NetSession*s){
  const auto now=Milliseconds();s->SetObserverBoard(instance,b->mApp->mGameScene==GameScenes::SCENE_PLAYING);
  if(s->IsWatchingV2()&&s->ObserverAccepted()){
   const uint8_t target=s->GetWatchTargetSeat();
   Identity next{s->ObserverWatch(),s->mCoopPeers[target]?s->mCoopPeers[target]:uint64_t(target),s->ObserverStream(),s->ObserverEpoch(),target};
   if(!(next==identity)){
    bool refresh=identity.Valid()&&identity.player==next.player&&identity.seat==next.seat&&identity.run==next.run&&identity.generation==next.generation;
    if(!refresh){scene.Clear();have=false;sequence=0;}
    timeline.End();sourceClock.Clear();directView=false;anchorAt=anchorSource=visibleLive=0;identity=next;requestedAt=now;
   }
   ObserverTransfer::Complete body;
   while(s->TakeObserverDisplay(body)){
    DrawFrame frame;
    if(body.direct){
     if(!DecodeDrawFrame(body.data.data(),body.data.size(),frame)||!(frame.flags&4)){s->StopObserverDirect();continue;}
     if(timeline.GetStatus()==Status::Closed){timeline.Begin(identity,now);sourceClock.Clear();anchorAt=anchorSource=visibleLive=0;}
     SourceStamp stamp;
     if(sourceClock.Peek(body.sequence,body.tick,stamp)){
      frame.identity=identity;frame.sequence=stamp.sequence;
      if(timeline.Accept(frame,stamp.milliseconds,now)){
       if(!anchorAt){anchorAt=now;anchorSource=stamp.milliseconds;}
       sourceClock.Commit(body.sequence,body.tick,stamp);liveReceived=now;
      }
     }
    }else if(!directView){
     if(!DecodeDrawFrame(body.data.data(),body.data.size(),frame)||!(frame.flags&4)||!scene.Prepare(frame)){s->EndWatch(true);scene.Clear();identity={};have=false;break;}
     sequence=body.sequence;receivedAt=now;capturedAt=requestedAt;have=true;
    }
   }
  }else if(identity.Valid()){scene.Clear();timeline.End();sourceClock.Clear();identity={};have=false;sequence=0;directView=false;}
  if(identity.Valid()){
   if(directView&&now-liveReceived>5000){
    if(scene.Prepare(liveScene.CurrentFrame())){have=true;sequence=visibleLive;capturedAt=liveCaptureAt;receivedAt=liveCaptureAt;}
    directView=false;timeline.End();sourceClock.Clear();s->StopObserverDirect();
   }
   timeline.Tick(now);
   if(timeline.GetSequence()&&timeline.GetSequence()!=visibleLive){
    visibleLive=timeline.GetSequence();directView=true;liveAgeAt=now;
    liveCaptureAt=std::min(now,anchorAt+timeline.VisibleSourceMilliseconds()-anchorSource);
   }
  }
  if(have)scene.SetPredictionAge(double(now-receivedAt)/1000.0);
  if(now-lastCapture<captureInterval||!s->ObserverCanCapture())return;
  lastCapture=now;DrawFrame frame;frame.flags=4;frame.identity={1,1,1,1,s->GetLocalSeat()};frame.sequence=1;
  std::vector<uint8_t> bytes;
  const auto start=Milliseconds();
  if(CaptureDrawFrame(b,frame)&&EncodeDrawFrame(frame,bytes))s->SendObserverDisplay(bytes,uint32_t(now));
  else{s->ObserverCaptureFailed();fprintf(stderr,"[observer] one-shot snapshot unavailable; no continuous fallback\n");}
  captureInterval=std::clamp<uint64_t>((Milliseconds()-start)*100,500,2000);
 }
 uint64_t VisibleSequence()const{return directView?visibleLive:have?sequence:0;}
 bool Draw(Sexy::Graphics*g,NetSession*s){
  if(!s||!s->IsWatchingV2())return false;
  g->SetColor(Sexy::Color(15,20,18));g->FillRect(0,0,800,600);
  if(directView){liveScene.SetPredictionAge(double(Milliseconds()-liveCaptureAt)/1000.0);liveScene.Draw(g);}
  else if(have)scene.Draw(g);
  else SeatUi::Text(g,ModText::Tr("正在获取队友快照…","Loading teammate snapshot..."),200,300,440,22,Sexy::Color(245,230,165),true);
  g->SetColor(Sexy::Color(0,0,0,180));g->FillRect(0,0,395,58);
  auto seconds=directView?(Milliseconds()-liveCaptureAt)/1000:have?(Milliseconds()-capturedAt)/1000:0;
  auto label=std::string("P")+std::to_string(s->GetWatchTargetSeat())+ModText::Tr(" 本地推演 · "," Local prediction · ")+std::to_string(seconds)+ModText::Tr("秒前快照","s old");
  SeatUi::Text(g,label,10,33,380,18,Sexy::Color(245,230,165),true);
  if(have&&(scene.Flags()&1))SeatUi::Text(g,ModText::Tr("采样时已暂停","Paused at snapshot"),260,330,340,24,Sexy::Color(245,230,165),true);
  return true;
 }
};
}
