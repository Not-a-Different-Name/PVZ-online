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

bool NetSession::SendSharedRole(uint8_t op,uint8_t value,uint32_t request) {
    if(!SharedRoom() || !mSharedTerm)return false;
    uint8_t bytes[14];NetProto::Writer writer(bytes,sizeof(bytes));
    writer.U8(op);writer.U8(value);writer.U32(mSharedTerm);writer.U32(mSharedEpoch);writer.U32(request);
    return SendRaw(WatchRoom::Role,bytes,writer.Size());
}

bool NetSession::SetRoomRole(uint8_t role) {
    if(role>WatchRoom::Watch || (role==WatchRoom::Watch && IsHostSeat()))return false;
    return SendSharedRole(WatchRoom::SetRole,role,++mWatchRequest);
}

void NetSession::HandleSharedRoster(const uint8_t* bytes,int size) {
    if(!IsRelay() || !IsConnected() || size<19)return;
    NetProto::Reader reader(bytes,size);
    const auto version=reader.U8(), local=reader.U8(), host=reader.U8(), phase=reader.U8();
    const auto flags=reader.U8(), sources=reader.U8();
    const auto term=reader.U32(), epoch=reader.U32();
    char code[5]{};reader.Bytes(code,4);
    const auto count=reader.U8();
    if(version!=2 || local<1 || local>NetProto::MAX_PLAYERS || local!=mLocalSeat || host>NetProto::MAX_PLAYERS ||
        phase>5 || (flags&~63) || (sources&0x81) || !term || term<mSharedTerm ||
        (term==mSharedTerm && epoch<mSharedEpoch) || mRoomCode!=code || count>NetProto::MAX_PLAYERS || size!=19+count*24)return;
    decltype(mSharedMembers) members{};
    decltype(mSharedOrder) order{};
    decltype(mWatchOrder) combat{};
    size_t combatCount=0;
    uint8_t readySources=0;
    for(int i=0;i<count;++i) {
        const auto seat=reader.U8();
        const auto build=reader.U16();
        const auto identity=reader.U32();
        const auto state=reader.U8();
        char name[NetProto::NAME_SIZE]{};reader.Bytes(name,sizeof(name));
        if(seat<1 || seat>NetProto::MAX_PLAYERS || members[seat].identity || build!=NetProto::MOD_BUILD || !identity || (state>>2)>2)return;
        auto& member=members[seat];
        member={SanitizeName(name,sizeof(name)),identity,build,state};
        order[i]=seat;
        if(!member.Offline() && !member.Spectator()) {
            combat[combatCount++]=seat;
            readySources|=1u<<seat;
        }
    }
    if(reader.Overflowed() || (sources&~readySources) || !members[local].identity || members[local].Offline() ||
        (host && (!(readySources&(1u<<host)))) || (!host && !(flags&16)))return;
    const bool wasObserver=InWatchRoom();
    const bool observer=members[local].Spectator();
    const auto target=GetWatchTargetSeat();
    if(target && ((mSharedMembers[target].identity && members[target].identity!=mSharedMembers[target].identity) ||
        ((mSharedSources&(1u<<target)) && !(sources&(1u<<target))))) {
        EndWatch(true);mWatchEndPending=WatchEnd::TARGET_LEFT;
    }
    if(mSharedTerm!=term || mSharedEpoch!=epoch)mSharedSourceSent=2;
    mSharedRoom=mWatchRoom=true;
    mSharedTerm=term;mSharedEpoch=epoch;mSharedFlags=flags;mSharedSources=sources;
    mSharedMembers=std::move(members);mSharedOrder=order;mWatchOrder=combat;mWatchPrivate=!(flags&1);
    if(!observer) {
        mWatchSlot=0;
        if(wasObserver && phase!=0)mWatchPromotion=phase==1?epoch:epoch+1;
        return;
    }
    if(!wasObserver) {
        EndWatch(true);EndPreparation();DiscardLevelPackets();ClearSwapState();
        mFlowDeferred=mFlowStartPending=false;
        mSharedRoomEntry=true;
        PushEvent(EventType::WATCH_ROOM_ENTERED);
    }
    mWatchSlot=local;mHostSeat=host;mRole=Role::CLIENT;
    mFlowEnabled=(flags&4)!=0;mFlowLost=(flags&8)!=0;
    mFlowTerm=term;mFlowEpoch=mReadyEpoch=epoch;mFlowPhase=phase;
    mSeats[local].mOccupied=false;
    for(uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat) {
        if(seat==local)continue;
        const auto& member=mSharedMembers[seat];
        auto& entry=mSeats[seat];
        entry.mOccupied=member.identity && !member.Spectator();
        entry.mOffline=member.Offline();entry.mName=member.name;entry.mBuild=member.build;
        mCoopPeers[seat]=member.identity;
    }
}

void NetSession::HandleSharedRecovery(const uint8_t* bytes,int size) {
    if(!SharedRoom() || InWatchRoom() || !IsHostSeat() ||
        (size!=START_LEVEL_PAYLOAD_SIZE && size!=START_LEVEL_PAYLOAD_SIZE+AdmissionWire::SIZE))return;
    NetProto::MsgStartLevel start{};
    if(!NetProto::DecodeStartLevel(bytes,START_LEVEL_PAYLOAD_SIZE,start) || start.mReadyEpoch!=mSharedEpoch)return;
    mEndlessStartContext={};
    if(size>START_LEVEL_PAYLOAD_SIZE) {
        if(!AdmissionWire::Decode(bytes+START_LEVEL_PAYLOAD_SIZE,AdmissionWire::SIZE,mEndlessStartContext))return;
        mLastEndlessStartContext=mEndlessStartContext;
        // The successor is now the authority. It can load its own matching
        // checkpoint or catch up, without waiting for an offer from itself.
        mEndlessStartContext.budget&=~512u;
    }
    AdoptReadyEpoch(start.mReadyEpoch);
    mLastStartLevel=mSharedRecoveryStart=start;
    mLastStartLevel.mSrcSeat=mLocalSeat;
    mLastStartLevelValid=mSharedRecovery=true;
}

bool NetSession::TakeSharedRecovery(NetProto::MsgStartLevel& start) {
    if(!mSharedRecovery)return false;
    mSharedRecovery=false;start=mSharedRecoveryStart;return true;
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
    if(size>0 && bytes[0]==2){HandleSharedRoster(bytes,size);return;}
    if(NetProto::MOD_BUILD>=112 && size>0 && bytes[0]==1) {
        SetDead(NetText("这个中继尚未更新到六席观战版本。","This relay has not updated to shared spectator seats.").c_str());
        return;
    }
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
