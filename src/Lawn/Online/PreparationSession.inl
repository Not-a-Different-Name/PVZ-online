// @pvz-online: one host-coordinated transaction, bound to player identity and round/revision.
// The existing relay remains authoritative for the actual seat exchange.
bool NetSession::BeginPreparation(bool gameManaged) {
    if(!IsConnected() || (!HasOtherSeats() && !SupportsAdmission()) || !mReadyEpoch)return false;
    for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)
        if(IsSeatOccupied(s) && mSeats[s].mBuild!=NetProto::MOD_BUILD)return false;
    if(mPreparation.epoch!=mReadyEpoch)mPreparation=PreparationWire::StateData{};
    mPreparation.epoch=mReadyEpoch;
    mPreparation.local=true;
    mPreparation.gameManaged=gameManaged;
    mCoopPeers[mLocalSeat]=mCoopToken;
    SendPreparation(PreparationWire::REPORT,mHostSeat);
    return true;
}

bool NetSession::ConfirmPreparationConfiguration() {
    if(!IsConnected() || !IsPreparationActive() || mPreparation.phase!=PreparationWire::SEAL)return false;
    mPreparation.configuredRevision=mPreparation.revision;
    return SendPreparation(PreparationWire::ACK,mHostSeat);
}

bool NetSession::CanChangePreparationSeats() const {
    if(!IsConnected() || !mPreparation.local || mPreparation.phase!=PreparationWire::OPEN)return false;
    for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)
        if(IsSeatOccupied(s) && !(mFlowEnabled && (IsSeatOffline(s)||IsWaitingForNextRound(s))) && (IsSeatOffline(s) || !mCoopPeers[s] ||
            !(mPreparation.entered&(1u<<s)) || mSeats[s].mBuild!=NetProto::MOD_BUILD))return false;
    return HasOtherSeats();
}

bool NetSession::SendPreparation(uint8_t op,uint8_t destination,uint64_t target,bool value) {
    if(!IsConnected() || !destination || !mReadyEpoch)return false;
    PreparationWire::Message m;
    m.src=mLocalSeat;m.dst=destination;m.sender=mCoopToken;m.op=op;
    m.epoch=mReadyEpoch;m.revision=mPreparation.revision;m.request=mPreparation.request;
    m.actor=mPreparation.actor;m.target=target?target:mPreparation.target;
    m.phase=mPreparation.phase;m.entered=mPreparation.entered;
    m.ready=(op==PreparationWire::READY || op==PreparationWire::ANSWER)?(value?1:0):mPreparation.ready;
    m.a=mPreparation.a;m.b=mPreparation.b;
    for(int s=1;s<=NetProto::MAX_PLAYERS;++s)m.players[s]=s==mLocalSeat?mCoopToken:mCoopPeers[s];
    if(destination==mLocalSeat) {HandlePreparation(m);return true;}
    uint8_t bytes[PreparationWire::SIZE];PreparationWire::Encode(bytes,m);
    return Dispatch(NetProto::MSG_PREPARATION,bytes,sizeof(bytes),destination);
}

void NetSession::PublishPreparation() {
    // Apply the coordinator's state locally too, before any client can answer.
    for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)if(IsSeatOccupied(s) && !(mFlowEnabled && (IsSeatOffline(s)||IsWaitingForNextRound(s)))) {
        mSeats[s].mSeedsReady=(mPreparation.ready&(1u<<s))!=0;
        mSeats[s].mSeedsReadyEpoch=mReadyEpoch;
        if(s!=mLocalSeat)SendPreparation(PreparationWire::STATE,s);
    }
    mSeedsReadySent=false;UpdateReadyCancelState();
}

void NetSession::HandlePreparation(const PreparationWire::Message& m) {
    using namespace PreparationWire;
    if(!IsConnected() || m.epoch!=mReadyEpoch || !IsSeatOccupied(m.src))return;
    if(m.dst!=mLocalSeat && !(m.op==STATE && m.players[m.dst]==mCoopToken))return;
    if(mPreparation.epoch!=mReadyEpoch) {
        mPreparation=StateData{};mPreparation.epoch=mReadyEpoch;
    }
    auto& p=mPreparation;
    if(m.op==STATE) {
        // The host identity survives a move to a different seat.
        uint64_t host=mHostSeat==mLocalSeat?mCoopToken:mCoopPeers[mHostSeat];
        if(mRole==Role::HOST || (host?m.sender!=host:m.src!=mHostSeat) || m.revision<p.revision)return;
        if(m.revision==p.revision && m.phase<p.phase)return;
        // A later COMMIT snapshot may already carry the server-confirmed map.
        const bool finalCommit=m.phase==COMMIT && m.a && m.b &&
            m.players[m.a]==m.target && m.players[m.b]==m.actor;
        if(finalCommit && p.appliedRequest!=m.request) {
            p.phase=COMMIT;p.request=m.request;p.a=m.a;p.b=m.b;
            if(p.rejoinRoster) {
                std::swap(mCoopPeers[m.a],mCoopPeers[m.b]);
                std::swap(p.participants[m.a],p.participants[m.b]);
                p.appliedRequest=p.request;
            } else ApplySeatSwap(m.a,m.b);
        }
        // A full roster, rather than replaying a toggle, repairs a missed commit on rejoin.
        // COMMIT carries the old map; later phases carry the authoritative final map.
        if(m.phase!=COMMIT) {
            for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)if(m.players[s]) {
                uint8_t old=FindCoopSeat(m.players[s]);
                if(old && old!=s)ApplySeatSwap(s,old);
            }
        }
        if(m.src!=mHostSeat && m.sender!=mCoopPeers[mHostSeat])return;
        if(m.revision!=p.revision || m.request!=p.request)p.commitSent=false;
        p.revision=m.revision;p.request=m.request;p.actor=m.actor;p.target=m.target;
        p.phase=m.phase;p.entered=m.entered;p.ready=m.ready;p.a=m.a;p.b=m.b;
        for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)if(IsSeatOccupied(s) && !(mFlowEnabled && (IsSeatOffline(s)||IsWaitingForNextRound(s)))) {
            if(m.players[s] && (finalCommit || !(m.phase==COMMIT && p.appliedRequest==p.request)))mCoopPeers[s]=m.players[s];
            mSeats[s].mSeedsReady=(p.ready&(1u<<s))!=0;mSeats[s].mSeedsReadyEpoch=mReadyEpoch;
        }
        // Retain the committed identity map too on clients. REPORT fills this
        // table on the coordinator, while STATE supplies it to participants.
        if(p.phase==GO)for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)
            p.participants[s]=m.players[s];
        ClearSwapState();UpdateReadyCancelState();
        if(p.phase==COMMIT && p.appliedRequest!=p.request) {
            if(mTransport==Transport::DIRECT)ApplySeatSwap(p.a,p.b);
            else if(p.actor==mCoopToken && !p.commitSent) {
                p.commitSent=true;SendSeatSwapCommit(p.a,p.b);
            }
        }
        if((p.phase==COMMIT && p.appliedRequest==p.request) ||
            (p.phase==SEAL && (!p.gameManaged || p.configuredRevision==p.revision)))
            SendPreparation(ACK,mHostSeat);
        return;
    }
    if(mRole!=Role::HOST)return;
    if(mCoopPeers[m.src] && mCoopPeers[m.src]!=m.sender) {
        if(m.op==REPORT && p.phase==COMMIT && p.rejoinRoster && p.appliedRequest!=p.request &&
            ((m.src==p.a && m.sender==p.target)||(m.src==p.b && m.sender==p.actor))) {
            // WELCOME already rebuilt names/local/host seats; only token bookkeeping was stale.
            std::swap(mCoopPeers[p.a],mCoopPeers[p.b]);std::swap(p.participants[p.a],p.participants[p.b]);
            p.appliedRequest=p.request;p.acks|=uint8_t(1u<<mLocalSeat);
        } else return;
    }
    if(m.op==REPORT) {
        mCoopPeers[m.src]=m.sender;
        p.participants[m.src]=m.sender;
        if(p.recovering)p.recoveryReports|=uint8_t(1u<<m.src);
        if(!(p.entered&(1u<<m.src))) {
            p.entered|=uint8_t(1u<<m.src);p.ready=0;
            if(!p.revision)p.revision=1;
        }
        PublishPreparation();return;
    }
    if(m.op==QUERY) {SendPreparation(STATE,m.src);return;}
    if(m.revision!=p.revision)return;
    if(m.op==REQUEST) {
        if(!CanChangePreparationSeats() || !m.target || m.target==m.sender)return;
        uint8_t target=FindCoopSeat(m.target);if(!target || IsSeatOffline(target))return;
        ++p.revision;p.request=p.revision;p.actor=m.sender;p.target=m.target;
        p.a=m.src;p.b=target;p.phase=ASK;p.ready=0;p.ticks=1500;
        PublishPreparation();return;
    }
    if(m.op==ANSWER) {
        if(p.phase!=ASK || m.request!=p.request || m.sender!=p.target)return;
        if(!m.ready) {
            ++p.revision;p.phase=OPEN;p.ready=0;p.request=0;PublishPreparation();return;
        }
        // Recheck both identities and connectivity at the point consent is committed.
        if(FindCoopSeat(p.actor)!=p.a || FindCoopSeat(p.target)!=p.b)return;
        for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)if(IsSeatOccupied(s)&&IsSeatOffline(s)&&!mFlowEnabled)return;
        ++p.revision;p.phase=COMMIT;p.acks=0;p.ready=0;p.commitSent=false;p.ticks=0;
        p.recovering=p.rejoinAlternate=p.rejoinRoster=false;p.recoveryReports=0;
        PublishPreparation();
        if(mTransport==Transport::DIRECT)ApplySeatSwap(p.a,p.b);
        else if(p.actor==mCoopToken) {p.commitSent=true;SendSeatSwapCommit(p.a,p.b);}
        return;
    }
    if(m.op==READY) {
        if(!(p.entered&(1u<<m.src)) || (m.ready ? !CanReadyPreparation() : !(p.phase==OPEN || p.phase==SEAL)))return;
        if(!m.ready && p.phase==SEAL){++p.revision;p.phase=OPEN;p.acks=0;p.configuredRevision=0;}
        if(m.ready)p.ready|=uint8_t(1u<<m.src);else p.ready&=uint8_t(~(1u<<m.src));
        PublishPreparation();return;
    }
    if(m.op==ACK && (p.phase==COMMIT || p.phase==SEAL) && m.request==p.request) {
        for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)
            if(IsSeatOccupied(s) && !(mFlowEnabled && (IsSeatOffline(s)||IsWaitingForNextRound(s))) && m.players[s]!=(s==mLocalSeat?mCoopToken:mCoopPeers[s]))return;
        p.acks|=uint8_t(1u<<m.src);
    }
}

void NetSession::PreparationSeatSwapApplied(uint8_t a,uint8_t b) {
    using namespace PreparationWire;
    auto& p=mPreparation;
    if(p.phase!=COMMIT || !((p.a==a && p.b==b)||(p.a==b && p.b==a)))return;
    std::swap(p.participants[a],p.participants[b]);
    p.appliedRequest=p.request;p.ready=0;
    for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)mSeats[s].mSeedsReady=false;
    if(mRole==Role::HOST)p.acks|=uint8_t(1u<<mLocalSeat);
    else SendPreparation(ACK,mHostSeat);
}

void NetSession::UpdatePreparation() {
    using namespace PreparationWire;
    auto& p=mPreparation;
    if(!IsConnected() || !p.epoch || p.epoch!=mReadyEpoch || p.phase==GO)return;
    if(p.local && ++p.reportTicks>=100) {
        p.reportTicks=0;
        SendPreparation(REPORT,mHostSeat);
        if(mRole!=Role::HOST)SendPreparation(QUERY,mHostSeat);
    }
    if(mRole!=Role::HOST)return;
    uint8_t occupied=0;bool complete=true;
    for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)if(IsSeatOccupied(s) && !(mFlowEnabled && (IsSeatOffline(s)||IsWaitingForNextRound(s)))) {
        occupied|=uint8_t(1u<<s);
        if(p.participants[s] && p.participants[s]!=mCoopPeers[s]) {
            p.entered&=uint8_t(~(1u<<s));p.ready=0;p.participants[s]=0;
        }
        if(IsSeatOffline(s) || !(p.entered&(1u<<s)) || !mCoopPeers[s] ||
            mSeats[s].mBuild!=NetProto::MOD_BUILD)complete=false;
    } else {
        p.entered&=uint8_t(~(1u<<s));p.ready&=uint8_t(~(1u<<s));p.participants[s]=0;
    }
    if(p.occupiedMask && p.occupiedMask!=occupied && p.phase!=COMMIT) {
        ++p.revision;p.phase=WAIT;p.ready=0;p.request=0;PublishPreparation();
    }
    p.occupiedMask=occupied;
    if(mAdmissionNewMask&occupied)complete=false;
    if(p.phase==ASK && (!complete || !IsSeatOccupied(p.a) || !IsSeatOccupied(p.b) || --p.ticks<=0)) {
        ++p.revision;p.request=0;p.phase=complete?OPEN:WAIT;p.ready=0;
        PublishPreparation();return;
    }
    if(p.phase==WAIT && p.local && complete) {
        ++p.revision;p.phase=OPEN;p.ready=0;PublishPreparation();return;
    }
    if(p.phase==OPEN && !complete) {
        ++p.revision;p.phase=WAIT;p.ready=0;PublishPreparation();return;
    }
    // A join accepted immediately before the relay closed the gate must not be
    // required to ACK a seat transaction it never entered. Finish its original
    // participants first, then admit the newcomer through WAIT.
    const uint8_t commitMask=occupied&p.entered;
    bool commitComplete=true;
    for(uint8_t s=1;s<=NetProto::MAX_PLAYERS;++s)if((commitMask&(1u<<s)) && (IsSeatOffline(s)||!mCoopPeers[s]))commitComplete=false;
    if(p.phase==COMMIT && p.appliedRequest==p.request && commitComplete && (p.acks&commitMask)==commitMask) {
        ++p.revision;p.phase=complete?OPEN:WAIT;p.ready=0;p.request=0;p.recovering=false;ClearSwapState();PublishPreparation();return;
    }
    if(p.phase==COMMIT && p.appliedRequest!=p.request) {
        // Query the relay's actual roster after an ambiguous send, never resend a seat toggle.
        if(!p.recovering && ++p.ticks>=300 && mTransport==Transport::RELAY) {
            p.recovering=true;p.recoveryReports=0;EnterReconnecting();return;
        }
        if(p.recovering && p.rejoinRoster && commitComplete && (p.recoveryReports&commitMask)==commitMask) {
            ++p.revision;p.phase=complete?OPEN:WAIT;p.ready=0;p.request=0;p.recovering=false;
            PublishPreparation();return;
        }
    }
    if(p.phase==OPEN && complete && (p.ready&occupied)==occupied) {
        if(!RequestRoomGate(false))return;
        p.phase=SEAL;p.acks=p.gameManaged?0:uint8_t(1u<<mLocalSeat);PublishPreparation();return;
    }
    if(p.phase==SEAL && !complete) {
        ++p.revision;p.phase=WAIT;p.ready=0;PublishPreparation();return;
    }
    if(p.phase==SEAL && complete && (p.acks&occupied)==occupied) {
        if(!RequestRoomGate(false))return;
        p.phase=GO;PublishPreparation();
    }
}
