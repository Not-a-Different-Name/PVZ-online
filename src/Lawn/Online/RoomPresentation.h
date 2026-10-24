#pragma once
#include "NetSession.h"
#include "RelayNodes.h"
#include "PrivateEndpoint.h"
#include "../../LawnApp.h"
#include "../ModText.h"
#include <string>

// @pvz-online: presentation only; all roles, relay targets and tiers come from the live session.
namespace RoomPresentation {
inline NetSession* Session(LawnApp* app) {
#ifdef __ANDROID__
    return app->mOnlineSession.get();
#else
    return app->mOnlineSession;
#endif
}
inline bool Active(const NetSession* s){return s&&s->IsActive()&&(s->GetState()!=NetSession::State::DEAD||s->IsRelay());}
inline std::string State(const NetSession* s) {
    if(!s)return ModText::Tr("创建房间或加入队友","Create or join a room");
    switch(s->GetState()) {
    case NetSession::State::LISTENING:return ModText::Tr("请邀请队友加入","Invite teammates to join");
    case NetSession::State::CONNECTING:return ModText::Tr("正在加入，请稍候","Joining, please wait");
    case NetSession::State::HANDSHAKING:return ModText::Tr("正在加入，请稍候","Joining, please wait");
    case NetSession::State::CONNECTED:if(s->IsWaitingForNextRound(s->GetLocalSeat()))return ModText::Tr("请等下一关一起出战","Wait for the next stage");return s->IsBuildDifferent()?ModText::Tr("版本不同，请统一版本","Update to the same version"):ModText::Tr("","");
    case NetSession::State::RECONNECTING:return ModText::Tr("掉线后正在自动重连","Reconnecting automatically");
    case NetSession::State::DEAD:return ModText::Tr("请重新联机","Reconnect to play");
    default:return ModText::Tr("创建房间或加入队友","Create or join a room");
    }
}
inline std::string Identity(const NetSession* s) {
    if(!Active(s))return ModText::Tr("尚未加入房间","No room joined");
    std::string text=s->IsRelay()?ModText::Tr("房间 ","Room ")+s->GetRoomCode():ModText::Tr("局域网直连","Direct LAN");
    text+="  |  "+std::to_string(s->GetOccupiedSeatCount())+"/"+std::to_string(NetProto::MAX_PLAYERS)+ModText::Tr("席"," seats");
    if(s->GetLocalSeat())text+=ModText::Tr("  |  你 P","  |  You P")+std::to_string(s->GetLocalSeat());
    if(s->GetHostSeat())text+=ModText::Tr("  |  房主 P","  |  Host P")+std::to_string(s->GetHostSeat());
    return text;
}
// @pvz-online: the room header follows the actual host; every seat owns its build label.
inline uint16_t SeatBuild(const NetSession* s,int seat) {
    if(!s||!s->IsSeatOccupied((uint8_t)seat))return 0;
    return seat==s->GetLocalSeat()?NetProto::MOD_BUILD:s->GetSeatBuild((uint8_t)seat);
}
inline std::string BuildLabel(uint16_t build) {
    return build?"MOD"+std::to_string(build):ModText::Tr("待确认","Pending");
}
inline std::string HostVersion(const NetSession* s) {
    return ModText::Tr("房主版本 ","Host version ")+BuildLabel(SeatBuild(s,s?s->GetHostSeat():0));
}
inline std::string SeatVersion(const NetSession* s,int seat) {
    return BuildLabel(SeatBuild(s,seat));
}
inline std::string ReadySummary(const NetSession* s) {
    int ready=0,total=0;
    if(s)for(int seat=1;seat<=NetProto::MAX_PLAYERS;++seat)if(s->IsSeatOccupied((uint8_t)seat)) {
        ++total;if(s->IsSeatSeedsReady((uint8_t)seat))++ready;
    }
    return ModText::Tr("已准备 ","Ready ")+std::to_string(ready)+"/"+std::to_string(total);
}
inline std::string WaitingSeats(const NetSession* s,bool ack=false) {
    std::string seats;
    if(s)for(int seat=1;seat<=NetProto::MAX_PLAYERS;++seat) {
        if(!s->IsSeatOccupied((uint8_t)seat))continue;
        if(ack?!s->IsSeatAwaitingStartAck((uint8_t)seat):s->IsSeatSeedsReady((uint8_t)seat))continue;
        if(!seats.empty())seats+=", ";seats+="P"+std::to_string(seat);
        if(s->IsSeatOffline((uint8_t)seat))seats+=ModText::Tr("(断线)","(offline)");
    }
    return seats.empty()?ModText::Tr("请稍候","Please wait"):
        ModText::Tr("等待 ","Waiting: ")+seats;
}
inline bool ShowingPreparation(LawnApp* app) {
    return app->mBoard && (app->mOnlineSeedsHeld || app->mRunIntroHeld);
}
inline std::string SeatProgress(LawnApp* app,int seat) {
    auto* s=Session(app);
    if(!s||!s->IsSeatOccupied((uint8_t)seat))return ModText::Tr("空席","Empty");
    if(s->IsWaitingForNextRound((uint8_t)seat))return ModText::Tr("等待下一关","Next stage");
    if(s->IsSeatOffline((uint8_t)seat))return ModText::Tr("断线保留","Held offline");
    if(s->GetState()==NetSession::State::RECONNECTING)return ModText::Tr("重新连接中","Reconnecting");
    if(app->mOnlineWaitingStartAck && s->IsHostSeat())
        return s->IsSeatAwaitingStartAck((uint8_t)seat)?ModText::Tr("待确认开局","Confirm start"):ModText::Tr("已加入","Joined");
    if(ShowingPreparation(app))return s->IsSeatSeedsReady((uint8_t)seat)?
        ModText::Tr("已准备","Ready"):ModText::Tr("未准备","Not ready");
    return ModText::Tr("在线","Online");
}
inline std::string Summary(LawnApp* app) {
    auto* s=Session(app);
    if(!s||!s->IsConnected())return State(s);
    if(s->IsBuildDifferent())return State(s);
    if(app->mOnlineSeedsHeld)return ReadySummary(s)+" · "+WaitingSeats(s);
    if(app->mOnlineWaitingStartAck)return ModText::Tr("请等队友确认开局","Waiting for start replies");
    if(s->HasIncomingSwapRequest())return ModText::Tr("请打开房间处理换位","Open room to answer swap");
    if(s->IsSwapRequestPending())return ModText::Tr("请等队友答复换位","Waiting for swap reply");
    return State(s);
}
inline std::string CompactRoster(const NetSession* s) {
    if(!Active(s))return ModText::Tr("点击创建或加入房间","Open to create or join");
    if(!s->GetLocalSeat())return ModText::Tr("正在加入，请稍候","Joining, please wait");
    return ModText::Tr("你 P","You P")+std::to_string(s->GetLocalSeat())+ModText::Tr(" · 房主 P"," · Host P")+std::to_string(s->GetHostSeat())+
        " · "+std::to_string(s->GetOccupiedSeatCount())+"/6"+ModText::Tr("人 · 点此查看"," · Open room");
}
inline std::string SeatName(const NetSession* s,int seat) {
    if(!Active(s)||!s->IsSeatOccupied((uint8_t)seat))return "P"+std::to_string(seat)+ModText::Tr("  空席","  Empty");
    std::string text="P"+std::to_string(seat)+"  "+s->GetSeatName((uint8_t)seat);
    if(seat==s->GetLocalSeat())text+=ModText::Tr(" [你]"," [You]");
    if(seat==s->GetHostSeat())text+=ModText::Tr(" [房主]"," [Host]");
    if(s->IsSeatOffline((uint8_t)seat))text+=ModText::Tr(" [断线保留]"," [Held offline]");
    return text;
}
inline std::string Order(const NetSession* s) {
    if(!Active(s))return ModText::Tr("加入房间后显示接力顺序","Join a room to see relay order");
    std::string line=ModText::Tr("漏怪接力：","Leak relay: ");int count=0;
    for(int seat=1;seat<=NetProto::MAX_PLAYERS;++seat)if(s->IsSeatOccupied((uint8_t)seat)) {
        if(count++)line+=ModText::Tr(" → "," -> ");line+="P"+std::to_string(seat);
    }
    if(count==0)return ModText::Tr("席位分配后显示接力顺序","Relay order appears once seats are assigned");
    if(count==1)return ModText::Tr("邀请队友加入接力","Invite teammates for relay");
    return line+ModText::Tr("（末席漏怪，全队失败）"," (last-seat leak loses the run)");
}
// @pvz-online: explain why preparation is held instead of showing an inert ready button.
inline std::string PreparationStatus(NetSession* s) {
    if(!s || !s->IsPreparationActive())return ReadySummary(s);
    switch(s->GetPreparationPhase()) {
    case PreparationWire::WAIT:return ModText::Tr("请等队友选完奖励并归队","Wait for rewards and teammates");
    case PreparationWire::ASK:return ModText::Tr("换位待答复，之后请重新准备","Swap pending; ready again after");
    case PreparationWire::COMMIT:return ModText::Tr("正在换位，请稍候","Swapping seats, please wait");
    case PreparationWire::SEAL:return ModText::Tr("准备开战，请稍候","Starting, please wait");
    case PreparationWire::GO:return ModText::Tr("准备开战","Starting");
    default:return ModText::Tr("","")+ReadySummary(s);
    }
}
inline bool CanSwap(LawnApp* app,int seat) {
    auto* s=Session(app);
    return s&&s->IsConnected()&&(!app->mBoard||s->CanChangePreparationSeats())&&!app->mOnlineWaitingStartAck&&
        !s->IsSwapRequestPending()&&!s->HasIncomingSwapRequest()&&
        seat!=s->GetLocalSeat()&&s->IsSeatOccupied((uint8_t)seat)&&!s->IsSeatOffline((uint8_t)seat);
}
}
