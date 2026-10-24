#pragma once
#include "../Online/RoomPresentation.h"

namespace PlayerRoomText {
struct View {
    NetSession::State connection=NetSession::State::OFF;
    bool connected=false,reconnecting=false,waitingRound=false,swapRequest=false;
    bool startAllowed=false,needsPlayers=false,inGame=false,startPending=false;
    bool mismatch=false,host=false,retryLevel=false;
};
inline bool DifferentBuild(const NetSession* session) {
    if(!session)return false;
    if(session->IsBuildDifferent())return true;
    for(std::uint8_t seat=1;seat<=NetProto::MAX_PLAYERS;++seat)if(session->IsSeatOccupied(seat)) {
        const auto build=RoomPresentation::SeatBuild(session,seat);
        if(build&&build!=NetProto::MOD_BUILD)return true;
    }
    return false;
}
inline std::string VersionBadge(std::uint16_t build) {
    if(!build)return ModText::Tr("版本待确认","Checking build");
    if(build==NetProto::MOD_BUILD)return {};
    return "MOD"+std::to_string(build);
}
inline View Capture(LawnApp* app,bool startAllowed,bool needsPlayers) {
    View view;
    auto* session=RoomPresentation::Session(app);
    if(!session)return view;
    view.connection=session->GetState();
    view.connected=session->IsConnected();view.reconnecting=session->IsReconnecting();
    view.waitingRound=session->IsWaitingForNextRound(session->GetLocalSeat());
    view.swapRequest=session->HasIncomingSwapRequest();view.startAllowed=startAllowed;
    view.needsPlayers=needsPlayers;view.inGame=app->mBoard!=nullptr;
    view.startPending=app->mOnlineWaitingStartAck;view.mismatch=DifferentBuild(session);
    view.host=session->IsHostSeat();view.retryLevel=app->mBoard&&(app->mBoardResult==BoardResult::BOARDRESULT_LOST||app->mGameScene==GameScenes::SCENE_ZOMBIES_WON);return view;
}
inline const char* Title(const View& view) {
    if(view.connection==NetSession::State::DEAD)return ModText::Tr("连接中断，退出后重试","Disconnected: leave and retry");
    if(view.connection==NetSession::State::OFF)return ModText::Tr("尚未加入房间","Not in a room");
    if(view.connection==NetSession::State::CONNECTING)return ModText::Tr("正在连接","Connecting");
    if(view.connection==NetSession::State::HANDSHAKING)return ModText::Tr("正在加入房间","Joining room");
    if(view.connection==NetSession::State::RECONNECTING)return ModText::Tr("正在重连","Reconnecting");
    if(view.mismatch)return ModText::Tr("先统一游戏版本","Update before joining");
    if(view.swapRequest)return ModText::Tr("队友申请换位","Swap requested");
    if(view.reconnecting)return ModText::Tr("正在重连","Reconnecting");
    if(view.waitingRound)return ModText::Tr("已归队，等下一关","Rejoined: next level");
    if(view.needsPlayers)return ModText::Tr("邀请队友","Invite a teammate");
    if(view.startPending)return ModText::Tr("正在进入游戏","Starting game");
    if(view.startAllowed)return ModText::Tr("可以开始了","Ready to start");
    if(view.inGame&&view.connected)return ModText::Tr("继续本局","Continue playing");
    if(view.connected)return ModText::Tr("等待房主开始","Waiting for the host");
    return ModText::Tr("等待队友加入","Waiting for teammates");
}
inline std::string Hint(const View& view,const std::string& fallback) {
    if(view.connection==NetSession::State::DEAD)return fallback.empty()?ModText::Tr("先离开房间，再用「粘贴加入」重试。","Leave this room, then use Paste & join."):fallback;
    if(view.connection==NetSession::State::OFF)return ModText::Tr("先选择游戏方式，再创建或加入房间。","Choose how to play, then create or join a room.");
    if(view.connection==NetSession::State::CONNECTING||view.connection==NetSession::State::HANDSHAKING)return ModText::Tr("请稍等，暂时不用重复点击。","Please wait. No need to click again.");
    if(view.connection==NetSession::State::RECONNECTING)return ModText::Tr("请稍等，连接恢复后继续本局。","Please wait. Play resumes when reconnected.");
    if(view.mismatch)return ModText::Tr("请全队更新到同一版本，再重新加入房间。","Update to the same game version, then rejoin.");
    if(view.swapRequest)return ModText::Tr("点「队友」查看并答复换位申请。","Open Seats to answer the swap request.");
    if(view.reconnecting)return ModText::Tr("请稍等，连接恢复后继续本局。","Please wait. Play resumes when reconnected.");
    if(view.waitingRound)return ModText::Tr("等下一关开始，不用重新加入。","Wait for the next level. No need to join again.");
    if(view.needsPlayers)return ModText::Tr("点「复制邀请」发给朋友；至少2人才能开始。","Copy invite and send it to a friend. At least 2 players are needed.");
    if(view.startPending)return ModText::Tr("等队友进入游戏。","Wait for your teammates to enter.");
    if(view.startAllowed)return view.retryLevel?ModText::Tr("点「开始游戏」重试本关。","Choose Start game to retry this level."):ModText::Tr("点「开始游戏」选择玩法。","Choose Start game to pick a mode.");
    if(view.inGame&&view.connected)return ModText::Tr("点「返回」回到游戏。","Choose Back to return to the game.");
    if(view.connected&&!view.host)return ModText::Tr("等房主选择玩法。","Wait for the host to choose a mode.");
    return fallback;
}
inline const char* ReadyHint(bool ready,bool cancelling,bool tooLate,bool canToggle) {
    if(tooLate)return ModText::Tr("已经开局，不能再换植物。","The game has started. Plants can no longer be changed.");
    if(cancelling)return ModText::Tr("正在取消准备，请稍等。","Cancelling ready. Please wait.");
    if(!canToggle)return ModText::Tr("正在进入下一步，请稍等。","Please wait while the game gets ready.");
    if(ready)return ModText::Tr("想换植物？点「取消准备」。","To change plants, choose Cancel ready.");
    return ModText::Tr("选好植物后点「准备」。","Choose your plants, then select Ready.");
}
inline const char* StartLabel(NetSession::State connection,bool needsPlayers) {
    if(connection==NetSession::State::OFF||connection==NetSession::State::DEAD)return ModText::Tr("已断开","Offline");
    if(connection==NetSession::State::RECONNECTING)return ModText::Tr("重连中","Retrying");
    if(connection==NetSession::State::CONNECTING||connection==NetSession::State::HANDSHAKING)return ModText::Tr("连接中","Connecting");
    return needsPlayers?ModText::Tr("至少2人开局","Need 2"):ModText::Tr("开始游戏","Start game");
}
}
