// Observer v2: isolated display subscription, authenticated room routing.
bool NetSession::ObserverControl(uint8_t dst,ObserverWireV2::ControlOp op,uint32_t watch,uint32_t stream){
 if(!IsConnected()||dst<1||dst>6||dst==mLocalSeat||!IsSeatOccupied(dst)||IsSeatOffline(dst))return false;
 ObserverWireV2::Control c;c.op=op;c.watchId=watch;c.boardStream=stream;c.roundEpoch=ObserverEpoch();c.features=ObserverRevision;c.budgetKbps=24*1024;
 std::vector<uint8_t> p;if(!ObserverWireV2::EncodeControl(c,p))return false;p.insert(p.begin(),{mLocalSeat,dst});return Dispatch(30,p.data(),int(p.size()),dst);
}
void NetSession::ClearObserverV2(){
 for(auto&v:mObserverViewers)v={};mLink.ClearObserverOutput();mObserverAssembler.Context(0,0,0,0,0);mObserverInbox.clear();
 if(mObserverWatch)ClearWatchState(WatchEnd::SILENT);
}
void NetSession::SetObserverBoard(uint64_t instance,bool active){
 const bool changed=instance!=mObserverBoard;
 if(mObserverBoardActive&&(!active||changed)){
  for(uint8_t seat=1;seat<=6;++seat){auto&v=mObserverViewers[seat];if(v.watch)ObserverControl(seat,ObserverWireV2::ControlOp::STOP,v.watch,v.stream);}
 }
 if((changed&&mObserverBoard)||(mObserverBoardActive&&!active))ClearObserverV2();
 if(changed){mObserverBoard=instance;++mObserverLocalStream;if(!mObserverLocalStream)++mObserverLocalStream;}
 mObserverBoardActive=active;
}

bool NetSession::HasObserverWatchers()const{if(!mObserverBoardActive||!IsConnected()||IsWaitingForNextRound(mLocalSeat))return false;for(auto&v:mObserverViewers)if(v.watch)return true;return false;}
void NetSession::ObserverV2Tick(){
 const auto now=ObserverNow();
 if(mObserverEpoch!=ObserverEpoch()||mObserverTerm!=mFlowTerm){ClearObserverV2();mObserverEpoch=ObserverEpoch();mObserverTerm=mFlowTerm;}
 if(IsRelay()&&!mObserverRelay&&(now-mObserverQueryAt>=2000)){uint8_t p[4];ObserverWireV2::Put32(p,ObserverRevision);SendRaw(0xF023,p,4);mObserverQueryAt=now;}
 if(mObserverWatch){
  if(!IsSeatOccupied(mWatchTargetSeat)||IsSeatOffline(mWatchTargetSeat)||IsWaitingForNextRound(mWatchTargetSeat)||
    (!mObserverReceived&&now-mObserverPendingAt>90000)){
   EndWatch(true);mWatchEndPending=WatchEnd::TIMEOUT;
  }
 }
 // The authenticated gameplay connection already supplies liveness. No observer
 // keepalive or periodic picture is sent after a successful one-shot transfer.
 for(uint8_t seat=1;seat<=6;++seat){auto&v=mObserverViewers[seat];if(v.watch&&(!IsSeatOccupied(seat)||IsSeatOffline(seat))){v={};mLink.ClearObserverOutput(seat);}}

}
void NetSession::HandleObserver(uint16_t type,const uint8_t*p,int n){
 if(!IsConnected()||n<2||p[0]<1||p[0]>6||p[0]==mLocalSeat||p[1]!=mLocalSeat||!IsSeatOccupied(p[0])||IsSeatOffline(p[0])||GetSeatBuild(p[0])<96||
    (mTransport==Transport::DIRECT&&p[0]!=DirectPeerSeat())||(IsRelay()&&!mObserverRelay))return;
 const uint8_t src=p[0];const auto now=ObserverNow();
 if(type==31){if(!mObserverWatch||!mObserverAccepted||src!=mWatchTargetSeat)return;ObserverTransfer::Complete complete;
  const auto progress=mObserverAssembler.Progress();if(mObserverAssembler.Accept(p,n,now,complete)){mObserverLast=now;mObserverReceived=true;if(mObserverInbox.size()>=2)mObserverInbox.pop_front();mObserverInbox.push_back(std::move(complete));}if(mObserverAssembler.Progress()!=progress)mObserverLast=now;return;}
 ObserverWireV2::Control c;if(!ObserverWireV2::DecodeControl(p+2,n-2,c)||c.roundEpoch!=ObserverEpoch()||c.features!=ObserverRevision||c.flags||c.budgetKbps!=24*1024)return;
 using Op=ObserverWireV2::ControlOp;
 if(c.op==Op::BEGIN){
  if(!mObserverBoardActive||IsWaitingForNextRound(mLocalSeat)){ObserverControl(src,Op::BUSY,c.watchId,c.boardStream);return;}
  mObserverViewers[src]={c.watchId,mObserverLocalStream,now,true};mLink.ClearObserverOutput(src);
  ObserverControl(src,Op::ACCEPT,c.watchId,mObserverLocalStream);return;
 }
 if(c.op==Op::KEEPALIVE){auto&v=mObserverViewers[src];if(v.watch==c.watchId&&v.stream==c.boardStream){v.touched=now;ObserverControl(src,Op::ACCEPT,v.watch,v.stream);}return;}
 if(c.op==Op::STOP){auto&v=mObserverViewers[src];if(v.watch==c.watchId&&v.stream==c.boardStream){v={};mLink.ClearObserverOutput(src);}if(src==mWatchTargetSeat&&c.watchId==mObserverWatch)ClearWatchState(WatchEnd::SILENT);return;}
 if(src!=mWatchTargetSeat||c.watchId!=mObserverWatch)return;
 if(c.op==Op::ACCEPT){if(mObserverAccepted&&c.boardStream!=mObserverRemoteStream){EndWatch(true);return;}mObserverRemoteStream=c.boardStream;mObserverAccepted=true;mObserverAssembler.Context(src,mLocalSeat,mObserverWatch,c.boardStream,ObserverEpoch());}
 if(c.op==Op::BUSY||c.op==Op::UNSUPPORTED){EndWatch(true);mWatchEndPending=WatchEnd::TIMEOUT;}
}
bool NetSession::SendObserverDisplay(const std::vector<uint8_t>&body,uint32_t tick){
 if(!ObserverCanCapture())return false;++mObserverSeq;if(!mObserverSeq)++mObserverSeq;bool sent=false;
 for(uint8_t s=1;s<=6;++s){auto&v=mObserverViewers[s];if(!v.watch||!v.pending)continue;std::vector<std::vector<uint8_t>> parts;
  if(!ObserverTransfer::Fragment(mLocalSeat,s,v.watch,v.stream,mObserverSeq,tick,ObserverEpoch(),body,parts))return false;
  for(auto&part:parts){std::vector<uint8_t> packet;const bool flow=mFlowEnabled&&IsRelay();packet.resize(4+(flow?10:0)+part.size());ObserverWireV2::Put16(packet.data(),flow?NetProto::MSG_CLI_FLOW_GAME:31);ObserverWireV2::Put16(packet.data()+2,uint16_t(packet.size()-4));size_t at=4;
   if(flow){ObserverWireV2::Put32(packet.data()+at,mFlowTerm);ObserverWireV2::Put32(packet.data()+at+4,mReadyEpoch);ObserverWireV2::Put16(packet.data()+at+8,31);at+=10;}std::copy(part.begin(),part.end(),packet.begin()+at);part=std::move(packet);}
  sent=mLink.OfferObserver(s,std::move(parts))||sent;
  if(sent){
   if(IsRelay()){for(auto&viewer:mObserverViewers)viewer.pending=false;break;}
   v.pending=false;
  }
 }return sent;
}
bool NetSession::TakeObserverDisplay(ObserverTransfer::Complete&out){if(mObserverInbox.empty())return false;out=std::move(mObserverInbox.front());mObserverInbox.pop_front();return true;}

void NetSession::ObserverCaptureFailed(){
 for(uint8_t seat=1;seat<=6;++seat){auto&v=mObserverViewers[seat];if(v.watch&&v.pending){ObserverControl(seat,ObserverWireV2::ControlOp::BUSY,v.watch,v.stream);v.pending=false;}}
}

uint32_t NetSession::ObserverRequestCooldown()const{auto now=ObserverNow();return mObserverRequestAt&&now-mObserverRequestAt<5000?uint32_t(5000-(now-mObserverRequestAt)):0;}
