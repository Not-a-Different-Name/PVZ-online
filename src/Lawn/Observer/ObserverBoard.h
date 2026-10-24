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
 DrawScene scene;Timeline<DrawFrame,DrawScene> timeline;SourceClock clock;Identity identity;
 uint64_t captureAt=0;uint64_t instance=0;uint32_t failures=0;uint64_t interval=500;bool stopped=false,timelineStarted=false;
public:
 explicit BoardView(LawnApp* app):scene(app),timeline(scene){static std::atomic<uint64_t> ids{0};instance=++ids;}
 void Update(Board*b,NetSession*s){
  const auto now=Milliseconds();
  s->SetObserverBoard(instance,
    b->mApp->mGameScene==GameScenes::SCENE_PLAYING);
  if(s->IsWatchingV2()&&s->ObserverAccepted()){
   const uint8_t target=s->GetWatchTargetSeat();
   Identity next{ s->ObserverWatch(),s->mCoopPeers[target]?s->mCoopPeers[target]:uint64_t(target),s->ObserverStream(),s->ObserverEpoch(),target};
   if(!(next==identity)){timeline.End();timelineStarted=false;clock.Clear();identity=next;}
   ObserverTransfer::Complete body;
   while(s->TakeObserverDisplay(body)){
    SourceStamp stamp;DrawFrame frame;
    if(clock.Peek(body.sequence,body.tick,stamp)&&DecodeDrawFrame(body.data.data(),body.data.size(),frame)){
     frame.identity=identity;frame.sequence=stamp.sequence;
     if(!timelineStarted){timeline.Begin(identity,now);timelineStarted=true;}
     if(timeline.Accept(frame,stamp.milliseconds,now))clock.Commit(body.sequence,body.tick,stamp);
    }
   }
   if(timeline.AcceptedSequence())timeline.Tick(now);
   if(timelineStarted&&timeline.GetStatus()==Status::Closed){s->EndWatch(true);identity={};}
  }else if(identity.Valid()){timeline.End();timelineStarted=false;identity={};clock.Clear();}
  if(now<captureAt||now-captureAt<interval||!s->ObserverCanCapture())return;
  captureAt=now;
  DrawFrame frame;frame.identity={1,1,1,1,s->GetLocalSeat()};frame.sequence=1;
  std::vector<uint8_t> bytes;
  const auto started=Milliseconds();
  if(CaptureDrawFrame(b,frame)&&EncodeDrawFrame(frame,bytes)){interval=std::clamp<uint64_t>((Milliseconds()-started)*100,500,2000);failures=0;stopped=false;s->SendObserverDisplay(bytes,uint32_t(now));}
  else{++failures;if(!stopped){fprintf(stderr,"[observer] normal frame exceeded display budget or used an unavailable local resource; source paused\n");stopped=true;}}
 }
 uint64_t VisibleSequence()const{return timeline.GetSequence();}
 bool Draw(Sexy::Graphics*g,NetSession*s){
  if(!s||!s->IsWatchingV2())return false;
  g->SetColor(Sexy::Color(15,20,18));g->FillRect(0,0,800,600);
  if(timeline.GetSequence())scene.Draw(g);
  else SeatUi::Text(g,ModText::Tr("正在缓冲队友画面…","Buffering teammate view..."),220,300,400,22,Sexy::Color(245,230,165),true);
  g->SetColor(Sexy::Color(0,0,0,180));g->FillRect(0,0,520,58);
  SeatUi::Text(g,ModText::Tr("正在观看 P","Watching P")+std::to_string(s->GetWatchTargetSeat())+ModText::Tr(" · 延迟约1.5秒"," · 1.5s buffer"),12,34,500,20,Sexy::Color(245,230,165),true);
  if(scene.Flags()&1)SeatUi::Text(g,ModText::Tr("对方已暂停","They paused"),300,330,300,24,Sexy::Color(245,230,165),true);
  return true;
 }
};
}
