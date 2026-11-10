bool NetSession::WatchRoleAction(uint8_t op,uint8_t slot,uint32_t request) {
    if(!mWatchRoom || !IsConnected() || !mFlowTerm)return false;
    if(op==WatchRoom::Apply || op==WatchRoom::Cancel)slot=mWatchSlot;
    if(op==WatchRoom::Demote) {
        if(!CanMoveToWatchRoom())return false;
        slot=0;
    } else if(slot<WatchRoom::FirstViewer || slot>WatchRoom::LastViewer)return false;
    if(op==WatchRoom::Apply || op==WatchRoom::Demote) {
        mWatchRequest=std::max(mWatchRequest, mWatchViewers[mWatchSlot].request);
        request=++mWatchRequest;
    }
    if(!request)return false;
    uint8_t bytes[14];NetProto::Writer w(bytes,sizeof(bytes));
    w.U8(op);w.U8(slot);w.U32(mFlowTerm);w.U32(mFlowEpoch+1);w.U32(request);
    return SendRaw(WatchRoom::Role,bytes,w.Size());
}

bool NetSession::SendWatchRoomRejoin() {
    if(!mHasCredential || mRoomCode.size()!=NetProto::ROOM_CODE_LEN)return false;
    NetProto::MsgCliJoinRoom join={};
    join.mVersion=NetProto::PROTOCOL_VERSION;join.mBuild=NetProto::MOD_BUILD;
    NetProto::SetName(join.mName,mLocalName.c_str());
    memcpy(join.mRoomCode,mRoomCode.data(),NetProto::ROOM_CODE_LEN);
    uint8_t bytes[64];
    int n=NetProto::EncodeCliJoinRoom(bytes,sizeof(bytes),join);
    if(n<=0)return false;
    bytes[n++]=0;memcpy(bytes+n,mCredential,16);
    return SendRaw(WatchRoom::Join,bytes,n+16);
}

void NetSession::HandleWatchRoomRoster(const uint8_t* bytes,int size) {
    if(!IsRelay() || (mState!=State::HANDSHAKING && mState!=State::RECONNECTING && mState!=State::CONNECTED) || size<21)return;
    NetProto::Reader r(bytes,size);
    const auto version=r.U8(), slot=r.U8(), host=r.U8(), phase=r.U8(), flags=r.U8(), online=r.U8(), waiting=r.U8();
    const auto term=r.U32(),epoch=r.U32();
    char code[5]{};r.Bytes(code,4);
    const auto count=r.U8();
    if(version!=1 || (slot!=0 && (slot<WatchRoom::FirstViewer || slot>WatchRoom::LastViewer)) ||
        host<1 || host>NetProto::MAX_PLAYERS || phase>5 || (flags&128) || ((flags>>3)&7)>4 || !term || term<mFlowTerm ||
        (online&0x81) || (waiting&~online) || count>NetProto::MAX_PLAYERS ||
        (!mRoomCode.empty() && mRoomCode!=code))return;
    decltype(mWatchViewers) viewers{};
    std::array<std::string,NetProto::MAX_PLAYERS+1> names;
    std::array<uint16_t,NetProto::MAX_PLAYERS+1> builds{};
    std::array<uint32_t,NetProto::MAX_PLAYERS+1> identities{};
    std::array<uint8_t,NetProto::MAX_PLAYERS> order{};
    uint8_t seen=0;
    for(int i=0;i<count;++i) {
        const auto seat=r.U8();const auto build=r.U16();const auto identity=r.U32();
        char name[NetProto::NAME_SIZE]{};r.Bytes(name,sizeof(name));
        if(seat<1 || seat>NetProto::MAX_PLAYERS || (seen&(1u<<seat)) || !build || !identity)return;
        seen|=1u<<seat;names[seat]=SanitizeName(name,sizeof(name));builds[seat]=build;order[i]=seat;identities[seat]=identity;
    }
    const auto n=r.U8();
    if(seen!=online || n>4 || size!=21+count*23+n*22)return;
    uint16_t seenViewers=0;
    for(int i=0;i<n;++i) {
        const auto id=r.U8(),state=r.U8();const auto request=r.U32();
        char name[NetProto::NAME_SIZE]{};r.Bytes(name,sizeof(name));
        if(id<WatchRoom::FirstViewer || id>WatchRoom::LastViewer || (seenViewers&(1u<<id)) ||
            state>2 || (state && !request))return;
        seenViewers|=1u<<id;viewers[id]={state,request,SanitizeName(name,sizeof(name))};
    }
    if(r.Overflowed() || n>((flags>>3)&7) || (slot && !(seenViewers&(1u<<slot))))return;
    if(!slot && (!IsConnected() || mWatchSlot || !mLocalSeat))return;
    mWatchRoom=true;mWatchViewers=std::move(viewers);mWatchViewerMask=seenViewers;mWatchOrder=order;mWatchSlotsUsed=(flags>>3)&7;mWatchPrivate=!(flags&1);
    if(!slot)return; // Combat roster and barrier retain the original authoritative packet.
    const bool entering=!IsConnected() || !mWatchSlot;
    if(!entering && mWatchTargetSeat && mWatchPeerIdentity[mWatchTargetSeat]!=identities[mWatchTargetSeat])EndWatch(false);
    mWatchPeerIdentity=identities;
    if(entering) {
        EndWatch(false);EndPreparation();DiscardLevelPackets();
        ClearSeatTable();ClearSwapState();
        mFlowDeferred=false;mFlowStartPending=false;mFlowHelloSent=false;
        mLocalSeat=0;mRole=Role::CLIENT;mRoomCode=code;
    }
    mWatchSlot=slot;mHostSeat=host;mFlowEnabled=(flags&64)!=0;mFlowTerm=term;mFlowEpoch=mReadyEpoch=epoch;
    mFlowPhase=phase;mRoundWaitMask=waiting;
    for(uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat) {
        auto& item=mSeats[seat];item.mOccupied=(online&(1u<<seat))!=0;item.mOffline=false;
        item.mBuild=builds[seat];item.mName=std::move(names[seat]);
        mCoopPeers[seat]=identities[seat];
    }
    if(entering) {
        mState=State::CONNECTED;
        mFramesSincePacket=mFramesSinceHeartbeat=0;
        mFramesSinceReconnectStart=0;mRejoinSent=false;
        PushEvent(EventType::WATCH_ROOM_ENTERED);
    }
}
