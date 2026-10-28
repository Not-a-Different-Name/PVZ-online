// Observer v2: isolated display subscription, authenticated room routing.
bool NetSession::ObserverControl(uint8_t dst,ObserverWireV2::ControlOp op,uint32_t watch,uint32_t stream){
 if(!IsConnected()||dst<1||dst>6||dst==mLocalSeat||!IsSeatOccupied(dst)||IsSeatOffline(dst))return false;
 ObserverWireV2::Control c;c.op=op;c.watchId=watch;c.boardStream=stream;c.roundEpoch=ObserverEpoch();c.features=ObserverRevision;c.budgetKbps=24*1024;
 std::vector<uint8_t> p;if(!ObserverWireV2::EncodeControl(c,p))return false;p.insert(p.begin(),{mLocalSeat,dst});return Dispatch(30,p.data(),int(p.size()),dst);
}
void NetSession::ClearObserverV2(){
 mPunchLink.ClearPeers();mPunchAttemptWatch=mPunchFailedWatch=0;mPunchAttempts=0;mObserverDirectAt=0;
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
 if(IsRelay()&&((!mObserverRelay&&now-mObserverQueryAt>=2000)||(mObserverP2PEnabled&&mPunchAdvertised&&now-mObserverQueryAt>=25000))){uint8_t p[4];ObserverWireV2::Put32(p,ObserverRevision);SendRaw(0xF023,p,4);mObserverQueryAt=now;}
 // P2P 打洞（P2P-a）：能力查询每 2 秒直到中继 ACK；链上之后每拍泵问候/探测，结果回给中继。
 TickObserverDirect(now);
 if(mObserverWatch){
  if(!CanWatchSeat(mWatchTargetSeat)||
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
 if(c.op==Op::STOP){auto&v=mObserverViewers[src];if(v.watch==c.watchId&&v.stream==c.boardStream){mPunchLink.Cancel(DirectContext(src,true));v={};mLink.ClearObserverOutput(src);}if(src==mWatchTargetSeat&&c.watchId==mObserverWatch)ClearWatchState(WatchEnd::SILENT);return;}
 if(src!=mWatchTargetSeat||c.watchId!=mObserverWatch)return;
 if(c.op==Op::ACCEPT){if(mObserverAccepted&&c.boardStream!=mObserverRemoteStream){EndWatch(true);return;}mObserverRemoteStream=c.boardStream;mObserverAccepted=true;mObserverAssembler.Context(src,mLocalSeat,mObserverWatch,c.boardStream,ObserverEpoch());}
 if((c.op==Op::BUSY||c.op==Op::UNSUPPORTED)&&!mObserverReceived){EndWatch(true);mWatchEndPending=WatchEnd::TIMEOUT;}
}
bool NetSession::SendObserverDisplay(const std::vector<uint8_t>&body,uint32_t tick){
 if(!ObserverCanCapture())return false;
 bool directSent=false;const auto now=ObserverNow();
 for(uint8_t seat=1;seat<=6;++seat){auto c=DirectContext(seat,true);
  if(c.Valid()&&mPunchLink.Writable(c)&&now-mDirectSentAt[seat]>=mPunchLink.Interval(c)){
   if(mPunchLink.Queue(c,body,tick,now)){mDirectSentAt[seat]=now;directSent=true;}
  }
 }
 if(!mLink.ObserverWritable())return directSent;
 ++mObserverSeq;if(!mObserverSeq)++mObserverSeq;bool sent=false;
 for(uint8_t s=1;s<=6;++s){auto&v=mObserverViewers[s];if(!v.watch||!v.pending)continue;std::vector<std::vector<uint8_t>> parts;
  if(!ObserverTransfer::Fragment(mLocalSeat,s,v.watch,v.stream,mObserverSeq,tick,ObserverEpoch(),body,parts))return false;
  for(auto&part:parts){std::vector<uint8_t> packet;const bool flow=mFlowEnabled&&IsRelay();packet.resize(4+(flow?10:0)+part.size());ObserverWireV2::Put16(packet.data(),flow?NetProto::MSG_CLI_FLOW_GAME:31);ObserverWireV2::Put16(packet.data()+2,uint16_t(packet.size()-4));size_t at=4;
   if(flow){ObserverWireV2::Put32(packet.data()+at,mFlowTerm);ObserverWireV2::Put32(packet.data()+at+4,mReadyEpoch);ObserverWireV2::Put16(packet.data()+at+8,31);at+=10;}std::copy(part.begin(),part.end(),packet.begin()+at);part=std::move(packet);}
  sent=mLink.OfferObserver(s,std::move(parts))||sent;
  if(sent){
   if(IsRelay()){for(auto&viewer:mObserverViewers)viewer.pending=false;break;}
   v.pending=false;
  }
 }return sent||directSent;
}
bool NetSession::TakeObserverDisplay(ObserverTransfer::Complete&out){if(mObserverInbox.empty())return false;out=std::move(mObserverInbox.front());mObserverInbox.pop_front();return true;}

void NetSession::ObserverCaptureFailed(){
 for(uint8_t seat=1;seat<=6;++seat){auto&v=mObserverViewers[seat];if(v.watch&&v.pending){ObserverControl(seat,ObserverWireV2::ControlOp::BUSY,v.watch,v.stream);v.pending=false;}}
}

uint32_t NetSession::ObserverRequestCooldown()const{auto now=ObserverNow();return mObserverRequestAt&&now-mObserverRequestAt<5000?uint32_t(5000-(now-mObserverRequestAt)):0;}

// @pvz-online P2P-a：0xF024 打洞控制面（中继→客户端方向）。op1 ACK 带问候 token 与中继
// 打洞 UDP 端口；op2 OFFER 带对方公网端点与一次性配对 token，双方各起一个探测。
// 任何畸形/不允许的情形一律静默——观战永远有 MOD96 快照档兜底。

ObserverDirect::Context NetSession::DirectContext(uint8_t peer,bool producer)const{
 if(!mObserverP2PEnabled||peer<1||peer>6)return {};
 if(producer){const auto&v=mObserverViewers[peer];return {v.watch,v.stream,ObserverEpoch(),mFlowTerm,mLocalSeat,peer};}
 return {mObserverWatch,mObserverRemoteStream,ObserverEpoch(),mFlowTerm,peer,mLocalSeat};
}
bool NetSession::IsObserverDirect()const{return mObserverP2PEnabled&&mObserverWatch&&mObserverAccepted&&mPunchFailedWatch!=mObserverWatch&&mPunchLink.Ready(DirectContext(mWatchTargetSeat,false));}
uint64_t NetSession::ObserverDirectInterval()const{return IsObserverDirect()?mPunchLink.Interval(DirectContext(mWatchTargetSeat,false)):2000;}
bool NetSession::ObserverCanCapture()const{
 if(!HasObserverWatchers())return false;auto now=ObserverNow();
 if(mLink.ObserverWritable())for(auto&v:mObserverViewers)if(v.watch&&v.pending)return true;
 for(uint8_t peer=1;peer<=6;++peer){auto c=DirectContext(peer,true);if(c.Valid()&&mPunchLink.Writable(c)&&now-mDirectSentAt[peer]>=mPunchLink.Interval(c))return true;}
 return false;
}
void NetSession::TickObserverDirect(uint64_t now){
 if(!mObserverP2PEnabled)return;
 if(IsRelay()&&mPunchAdvertised&&(!mPunchRelay||(mObserverWatch&&now-mPunchRenewAt>=120000))&&now-mPunchQueryAt>=2000){
  uint8_t p[5];p[0]=ObserverDirect::OP_CAPABLE;ObserverWireV2::Put32(p+1,ObserverDirect::REVISION);SendRaw(ObserverDirect::CONTROL_TYPE,p,5);mPunchQueryAt=now;
 }
 // LAN tokens have the same 120-second lifetime as relay offers. Renew before
 // expiry even when the subscription and UDP channel are already established.
 if(mTransport==Transport::DIRECT&&mObserverWatch&&mObserverAccepted&&GetSeatBuild(mWatchTargetSeat)>=97&&mPunchFailedWatch!=mObserverWatch&&(mPunchAttemptWatch!=mObserverWatch||now-mPunchAttemptAt>=90000)){
  mPunchAttemptWatch=mObserverWatch;mPunchAttemptAt=now;
  if(mPunchLink.Open()){uint8_t p[35]{};p[0]=4;ObserverDirect::Put16(p+1,mPunchLink.LocalPort());
   if(ObserverDirect::DirectLink::Token(p+3)){
    auto c=DirectContext(mWatchTargetSeat,false);ObserverDirect::Put32(p+19,c.watch);ObserverDirect::Put32(p+23,c.stream);ObserverDirect::Put32(p+27,c.epoch);ObserverDirect::Put32(p+31,c.term);
    SendRaw(ObserverDirect::CONTROL_TYPE,p,sizeof(p));mPunchRelay=true;
   }
  }
 }
 if(!mPunchRelay)return;
 std::vector<ObserverDirect::DirectLink::Result> events;mPunchLink.Pump(now,&events);
 for(auto&e:events)if(IsRelay()){uint8_t p[21];ObserverDirect::PutResult(p,e.peerSeat,e.ok,e.rttMs);std::memcpy(p+5,e.token,16);SendRaw(ObserverDirect::CONTROL_TYPE,p,21);}
 if(mObserverWatch&&mObserverAccepted&&mPunchLink.HasEndpoint()&&mPunchFailedWatch!=mObserverWatch){
  if(mPunchAttemptWatch!=mObserverWatch){mPunchAttemptWatch=mObserverWatch;mPunchAttempts=0;mPunchAttemptAt=0;}
  if(IsRelay()&&(!IsObserverDirect()||now-mPunchAttemptAt>=90000)&&mPunchAttempts<3&&now-mPunchAttemptAt>=1000&&GetSeatBuild(mWatchTargetSeat)>=97){
   uint8_t p[14];p[0]=ObserverDirect::OP_BEGIN;p[1]=mWatchTargetSeat;ObserverWireV2::Put32(p+2,mObserverWatch);ObserverWireV2::Put32(p+6,mObserverRemoteStream);ObserverWireV2::Put32(p+10,ObserverEpoch());SendRaw(ObserverDirect::CONTROL_TYPE,p,sizeof(p));++mPunchAttempts;mPunchAttemptAt=now;
  }
  if(IsObserverDirect()&&!mObserverDirectAt)mObserverDirectAt=now;
  if(mObserverDirectAt&&now-mObserverDirectAt>6000){mPunchLink.Cancel(DirectContext(mWatchTargetSeat,false));mPunchFailedWatch=mObserverWatch;}
 }
 ObserverDirect::Frame frame;
 while(mPunchLink.Take(frame)){
  auto c=DirectContext(mWatchTargetSeat,false);
  if(!mObserverWatch||!mObserverAccepted||!(frame.context==c)||mPunchFailedWatch==mObserverWatch||!mPunchLink.Ready(c))continue;
  mObserverDirectAt=mObserverLast=now;mObserverReceived=true;mPunchAttempts=0;
  ObserverTransfer::Complete out{c.src,c.dst,c.watch,c.stream,frame.sequence,frame.tick,c.epoch,ObserverWireV2::Get32(frame.data.data()+1),std::move(frame.data),true};
  if(mObserverInbox.size()>=2)mObserverInbox.pop_front();mObserverInbox.push_back(std::move(out));
 }
}
void NetSession::HandlePunchControl(const uint8_t*p,int n){
 if(!mObserverP2PEnabled||!IsConnected()||!p||n<1)return;
 if(mTransport==Transport::DIRECT){
  const uint8_t peer=DirectPeerSeat();if(n!=35||p[0]!=4||GetSeatBuild(peer)<97||!ObserverDirect::Get16(p+1))return;
  ObserverDirect::Context c{ObserverDirect::Get32(p+19),ObserverDirect::Get32(p+23),ObserverDirect::Get32(p+27),ObserverDirect::Get32(p+31),0,0};bool producer=false;
  if(c.epoch!=ObserverEpoch()||c.term!=mFlowTerm)return;
  if(mObserverWatch&&mObserverAccepted&&c.watch==mObserverWatch&&c.stream==mObserverRemoteStream){c.src=peer;c.dst=mLocalSeat;}
  else{auto&v=mObserverViewers[peer];if(!mObserverBoardActive||v.watch!=c.watch||v.stream!=c.stream)return;producer=true;c.src=mLocalSeat;c.dst=peer;}
  if(!mPunchLink.Open())return;
  if(mPunchLink.StartLanProbe(peer,mLink.GetPeerIPv4Text().c_str(),ObserverDirect::Get16(p+1),p+3,ObserverNow(),c,producer)){
   mPunchRelay=true;if(producer){std::vector<uint8_t> reply(p,p+n);ObserverDirect::Put16(reply.data()+1,mPunchLink.LocalPort());SendRaw(ObserverDirect::CONTROL_TYPE,reply.data(),int(reply.size()));}
  }return;
 }
 if(!mPunchAdvertised)return;
 if(p[0]==ObserverDirect::OP_CAPABLE){ObserverDirect::CapableAck ack;
  if(!ObserverDirect::ParseCapableAck(p,n,&ack)||!mPunchLink.Open())return;
  if(mPunchLink.StartHello(GetRelayHost().c_str(),ack.udpPort,ack.token,ObserverNow())){mPunchRelay=true;mPunchRenewAt=ObserverNow();}return;
 }
 if(p[0]!=ObserverDirect::OP_BEGIN||n!=ObserverDirect::BOUND_OFFER_SIZE)return;
 ObserverDirect::Offer offer;if(!ObserverDirect::ParseOffer(p,n,&offer)||offer.peerSeat==mLocalSeat||!IsSeatOccupied(offer.peerSeat)||IsSeatOffline(offer.peerSeat)||GetSeatBuild(offer.peerSeat)<97)return;
 auto c=offer.context;if(c.epoch!=ObserverEpoch()||c.term!=mFlowTerm)return;
 bool producer=false;
 if(mObserverWatch&&mObserverAccepted&&offer.peerSeat==mWatchTargetSeat&&c.watch==mObserverWatch&&c.stream==mObserverRemoteStream){c.src=offer.peerSeat;c.dst=mLocalSeat;}
 else{auto&v=mObserverViewers[offer.peerSeat];if(!mObserverBoardActive||IsWaitingForNextRound(mLocalSeat)||!v.watch||v.watch!=c.watch||v.stream!=c.stream)return;c.src=mLocalSeat;c.dst=offer.peerSeat;producer=true;}
 if(!mPunchLink.Open())return;mPunchLink.StartProbe(offer.peerSeat,offer.ip,offer.port,offer.token,ObserverNow(),&c,producer,offer.ttl);
}

void NetSession::SetObserverP2PEnabled(bool enabled) {
    if(mObserverP2PEnabled==enabled)return;
    // Stop only UDP peers. Keep the accepted one-shot subscription and picture:
    // recreating BEGIN here would hit the relay's existing five-second limit.
    if(!enabled){
        if(mObserverWatch)mPunchLink.Cancel(DirectContext(mWatchTargetSeat,false));
        for(uint8_t seat=1;seat<=6;++seat)
            if(mObserverViewers[seat].watch)mPunchLink.Cancel(DirectContext(seat,true));
    }
    mObserverP2PEnabled=enabled;mPunchLink.Close();mPunchRelay=false;
    mPunchQueryAt=mPunchAttemptAt=mPunchRenewAt=mObserverDirectAt=0;
    mPunchAttemptWatch=mPunchFailedWatch=0;mPunchAttempts=0;
    for(auto& at:mDirectSentAt)at=0;
    mObserverInbox.erase(std::remove_if(mObserverInbox.begin(),mObserverInbox.end(),[](const ObserverTransfer::Complete& frame){return frame.direct;}),mObserverInbox.end());
}
