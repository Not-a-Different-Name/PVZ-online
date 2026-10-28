// @pvz-online: only the coordinator publishes the relay gate; its ACK orders joins before GO.
bool NetSession::RequestRoomGate(bool open) {
    if(mTransport!=Transport::RELAY || mRole!=Role::HOST || !mHasCredential || !IsConnected())return true;
    if(mGateWanted!=int(open) || mGateEpoch!=(mReadyEpoch?mReadyEpoch:1)) {
        mGateWanted=int(open);mGateEpoch=mReadyEpoch?mReadyEpoch:1;++mGateRequest;
        uint8_t bytes[13];const int size=mFlowEnabled?13:9;NetProto::Writer w(bytes,size);w.U32(mGateRequest);w.U32(mGateEpoch);w.U8(open?1:0);if(mFlowEnabled)w.U32(mFlowTerm);
        if(!SendRaw(0xF017,bytes,size))return false;
    }
    if(mGateAck!=mGateRequest && ++mGateRetry>=100) {
        mGateRetry=0;uint8_t bytes[13];const int size=mFlowEnabled?13:9;NetProto::Writer w(bytes,size);w.U32(mGateRequest);w.U32(mGateEpoch);w.U8(open?1:0);if(mFlowEnabled)w.U32(mFlowTerm);SendRaw(0xF017,bytes,size);
    }
    return mGateAck==mGateRequest;
}

bool NetSession::SendAdmission(AdmissionWire::Message m,uint8_t destination) {
    if(!IsConnected())return false;
    m.src=mLocalSeat;m.dst=destination;m.player=m.player?m.player:mCoopToken;
    uint8_t bytes[AdmissionWire::SIZE];AdmissionWire::Encode(bytes,m);
    return Dispatch(AdmissionWire::TYPE,bytes,sizeof(bytes),destination);
}

void NetSession::ResetAdmissionNetwork() {
    mAdmissionInbox.clear();mAdmissionOffer=AdmissionWire::Message{};
    mEndlessStartContext=mLastEndlessStartContext=AdmissionWire::Message{};
    mEndlessStartRun=0;mEndlessStartingResume=mEndlessStartingLegacy=mEndlessNeedsAdmission=false;

    mHasCredential=false;for(auto& n:mCredential)n=0;
    mGateWanted=-1;mGateEpoch=mGateRequest=mGateAck=0;mAdmissionNewMask=mAdmissionGrantMask=0;
    mAdmissionValuesEpoch=0;for(int& n:mAdmissionValues)n=-1;
}

void NetSession::CancelPendingAdmissions() {
    if(mTransport!=Transport::RELAY || !SupportsAdmission() || mRole!=Role::HOST)return;
    RequestRoomGate(false);
    for(uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat)if(mAdmissionNewMask&(1u<<seat))SendRaw(0xF019,&seat,1);
}
