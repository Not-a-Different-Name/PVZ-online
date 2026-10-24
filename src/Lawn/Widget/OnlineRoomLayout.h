#pragma once
#include "OnlineUiDraw.h"
#include "PlayerRoomText.h"
#include "NameplateDraw.h"
#include "NameplatePresentation.h"
#include "../Online/RoomPresentation.h"

namespace OnlineRoomLayout {
using namespace Sexy;
enum Page { CHOOSE, LOCAL, SERVER };
inline Rect LocalPage(){return Rect(40,88,328,54);}
inline Rect ServerPage(){return Rect(392,88,328,54);}
inline Rect Solo(){return Rect(48,250,664,50);}
inline Rect Server(){return Rect(148,284,292,36);}
inline Rect Code(){return Rect(200,276,340,44);}
inline Rect Paste(){return Rect(560,276,152,46);}
inline Rect CopyCode(){return Rect(264,194,148,44);}
inline Rect CopyInvite(){return Rect(424,194,176,44);}
inline Rect Share(){return Rect(612,194,100,44);}
inline Rect Address(){return Rect(148,350,564,36);}
inline Rect PrivateCode(){return Rect(148,400,180,36);}
inline Rect Create(){return Rect(48,340,320,46);}
inline Rect Join(){return Rect(392,340,320,46);}
inline Rect DirectHost(){return Rect(344,394,176,46);}
inline Rect DirectJoin(){return Rect(536,394,176,46);}
inline Rect Seats(){return Rect(40,480,160,46);}
inline Rect Disconnect(){return Rect(216,480,160,46);}
inline Rect Close(){return Rect(392,480,152,46);}
inline Rect Start(){return Rect(560,480,160,46);}
inline bool HasLostBoard(const LawnApp* app) {
    return app && app->mBoard && (app->mBoardResult==BoardResult::BOARDRESULT_LOST ||
        app->mGameScene==GameScenes::SCENE_ZOMBIES_WON);
}
inline bool CanStart(LawnApp* app) {
    auto* session=RoomPresentation::Session(app);
    return session && session->CanHostStart() && (app->IsOnlineStartLobby() || HasLostBoard(app)) && !app->mOnlineWaitingStartAck;
}
inline bool NeedsPlayers(const NetSession* session) {
    return session && session->IsRelay() && session->IsActive() && !session->CanHostStart() && session->GetConnectedPlayerCount(true)<2;
}
inline void Draw(Graphics* g,LawnApp* app,const std::string& detail,const std::string& localIp,int page=CHOOSE) {
    SeatUi::Panel(g,Rect(0,0,760,560));
    auto* s=RoomPresentation::Session(app);const bool active=RoomPresentation::Active(s);
    SeatUi::Text(g,active?(s->IsPrivateRoom()?ModText::Tr("局域网／自建房间","LAN / private room"):s->IsRelay()?ModText::Tr("在线房间","Online room"):ModText::Tr("局域网房间","LAN room")):
        ModText::Tr("选择游戏方式","Choose how to play"),40,64,488,28,Color(245,230,165));
    if(!active) {
        SeatUi::Strip(g,Rect(40,156,680,70));
        const std::string explanation=page==LOCAL?
            ModText::Tr("自己玩点「单人游戏」。和朋友玩可开房，或粘贴局域网邀请。","Choose Play solo, or host a room for friends on your network."):
            page==SERVER?ModText::Tr("开房后点「复制邀请」发给朋友；收到邀请可点「粘贴加入」。","Create a room and Copy invite for friends, or use Paste & join."):
            ModText::Tr("自己玩选「单人与局域网游戏」；和远方朋友玩选「在线房间」。","Play alone or on the same network with Solo / LAN game. Otherwise choose Online rooms.");
        SeatUi::Paragraph(g,explanation,Rect(56,166,648,52),19,Color(235,230,210));
        if(page==SERVER) {
            SeatUi::Text(g,ModText::Tr("至少2人才能开始","At least 2 players to start"),48,260,664,18,Color(210,220,195));
            SeatUi::Text(g,ModText::Tr("房间码","Code"),48,310,136,20,Color(220,225,210));
            SeatUi::Text(g,ModText::Tr("也可填写四位房码，再点「加入房间」。","Or enter a four-character code and choose Join room."),48,420,664,18,Color(190,210,225));
        } else if(page==LOCAL) {
            
            SeatUi::Text(g,ModText::Tr("局域网／自建 · 最多六人","LAN / private - up to six players"),48,332,664,20,Color(245,230,165));
            SeatUi::Text(g,ModText::Tr("地址:端口","Host:port"),48,378,96,18,Color(220,225,210));
            SeatUi::Text(g,ModText::Tr("房间码","Code"),48,425,88,18,Color(220,225,210));
            SeatUi::Text(g,ModText::Tr("本机地址：","Local IP: ")+localIp,48,448,420,15,Color(190,210,225));
        }
        if(!detail.empty() && s && s->GetState()!=NetSession::State::OFF)
            SeatUi::Text(g,detail,48,465,664,15,Color(255,175,120));
        return;
    }
    SeatUi::Strip(g,Rect(40,92,680,84));
    const auto view=PlayerRoomText::Capture(app,CanStart(app),NeedsPlayers(s));
    SeatUi::Text(g,PlayerRoomText::Title(view),56,121,412,23,view.mismatch?Color(255,175,120):Color(245,230,165));
    if(view.mismatch)SeatUi::Text(g,ModText::Tr("本机 MOD","Your build: MOD")+std::to_string(NetProto::MOD_BUILD),488,121,216,14,Color(255,175,120));
    SeatUi::Paragraph(g,PlayerRoomText::Hint(view,detail),Rect(56,130,648,42),17,view.mismatch?Color(255,175,120):Color(210,220,225));
    if(active) {
        SeatUi::Strip(g,Rect(40,188,680,54));
        std::string caption=s->GetRoomCode();
        if(caption.empty())caption=s->IsRelay()?ModText::Tr("等待房号","Preparing room"):ModText::Tr("局域网","LAN");
        if(s->GetOccupiedSeatCount())caption+="  |  "+std::to_string(s->GetOccupiedSeatCount())+"/6";
        SeatUi::Text(g,caption,54,222,190,20,Color(245,230,165));
        for(int i=0;i<NetProto::MAX_PLAYERS;++i) {
            const int seat=i+1;Rect box(40+(i%2)*352,254+(i/2)*62,328,56);
            SeatUi::Strip(g,box);bool occupied=s->IsSeatOccupied((uint8_t)seat);
            const int nameplate=NameplatePresentation::ForSeat(app,static_cast<uint8_t>(seat));
            if(occupied && nameplate)NameplateDraw::Draw(g,Rect(box.mX+4,box.mY+1,320,28),nameplate,RoomPresentation::SeatName(s,seat));
            else SeatUi::Text(g,RoomPresentation::SeatName(s,seat),box.mX+10,box.mY+23,308,20,
                occupied?Color(245,230,165):Color(170,170,165));
            if(occupied)SeatUi::Text(g,RoomPresentation::SeatProgress(app,seat),
                box.mX+10,box.mY+47,occupied?176:308,18,s->IsSeatOffline((uint8_t)seat)?Color(255,175,120):Color(190,215,225));
            if(occupied){const auto build=RoomPresentation::SeatBuild(s,seat);const auto badge=PlayerRoomText::VersionBadge(build);if(!badge.empty())SeatUi::Text(g,badge,box.mX+196,box.mY+47,122,14,build?Color(255,175,120):Color(145,160,165));}
        }
        if(s->IsConnected()&&s->GetOccupiedSeatCount()>1&&(!s->IsRelay()||app->mBoard))SeatUi::Text(g,RoomPresentation::Order(s),40,450,420,18,Color(215,215,195));
    }
}
}
