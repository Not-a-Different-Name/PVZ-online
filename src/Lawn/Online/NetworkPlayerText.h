#pragma once
#include <string>

// Display-only translation. Keep transport causes and diagnostic logs intact.
namespace NetworkPlayerText {
inline std::string Transport(const std::string& raw, bool chinese) {
    if(raw.find("The network thread is stuck.")==0 || raw=="Cannot start the network thread." ||
       raw=="Winsock could not start." || raw=="Winsock is not available.")
        return chinese?"联机无法启动，请重启游戏。":"Networking could not start. Restart the game.";
    if(raw=="No address given." || raw=="The server address is invalid." || raw=="Cannot resolve that address.")
        return chinese?"无法连接，请核对邀请后重试。":"Could not connect. Check the invite and try again.";
    if(raw=="Cannot listen on that port.")
        return chinese?"开房失败，请重试；仍失败请重启游戏。":"Could not host. Try again, or restart the game.";
    return chinese?"连接失败，请检查网络后重新联机。":"Connection failed. Check your network and reconnect.";
}
inline std::string Error(const std::string& raw, bool chinese) {
    struct Entry { const char* zh; const char* en; const char* shownZh; const char* shownEn; };
    static const Entry entries[] = {
        {"暂时无法创建房间，请稍后重试。", "Could not create a room. Please retry shortly.", "建房失败，请稍后重试。", "Could not create a room. Try again shortly."},
        {"重连失败——没能回到房间。", "Reconnect failed - could not get back into the room.", "未能归队，请重新加入房间。", "Could not rejoin. Join the room again."},
        {"服务器没有回应。", "The server did not answer.", "连接超时，请稍后重新加入。", "Connection timed out. Try joining again shortly."},
        {"对方发来的数据包有问题。", "A player sent a malformed packet.", "联机数据异常，请重新联机。", "Room data is invalid. Reconnect to play."},
        {"服务器发来的数据包有问题。", "The server sent a malformed packet.", "联机数据异常，请重新加入房间。", "Room data is invalid. Rejoin the room."},
        {"服务器发来的名册有问题。", "The server sent a bad roster.", "房间信息异常，请重新加入。", "Room details are invalid. Rejoin the room."},
        {"对方发来的席位号不合法。", "A player sent a bogus seat number.", "房间信息异常，请重新联机。", "Room details are invalid. Reconnect to play."},
        {"构建版本不一致——请把所有机器更新到同一版本。", "Build mismatch - update every machine to the same build.", "版本不同，请和队友更新到同一版。", "Update to the same game version as your teammates."},
        {"协议版本不一致——所有人必须跑同一版本。", "Version mismatch - all players must run the same build.", "版本不同，请和队友更新到同一版。", "Update to the same game version as your teammates."},
        {"有玩家离开了游戏。", "A player left the game.", "队友已退出，请重新联机。", "Your teammate left. Reconnect to play."},
        {"重连失败：席位对不上。", "Reconnect failed: the seat does not match.", "无法归队，请重新加入房间。", "Could not rejoin. Join the room again."},
        {"重连失败：身份对不上，服务器不认这条归队请求。", "Reconnect failed: the server did not recognize you.", "归队身份无法确认，请重新加入房间。", "Your return identity could not be confirmed. Rejoin the room."},
        {"重连失败：房间或你的席位已经没了。", "Reconnect failed: the room or your seat is gone.", "房间或席位已失效，请重新加入或建房。", "The room or seat expired. Join or create a room."},
        {"重连失败。", "Reconnect failed.", "无法归队，请重新加入房间。", "Could not rejoin. Join the room again."},
        {"本关已开始或正在准备开战，请等房主在关间开放补位。", "Joining is closed. Ask the host to open intermission invitations.", "暂时不能加入，请等房主在关间开放补位。", "Wait for the host to open joining between stages."},
        {"游戏版本不同，请与房主使用同一版本。", "Use the same game version as the host.", "请与房主更新到同一版本。", "Update to the same game version as the host."},
        {"没有这个房间——请核对房间码。", "No such room - check the room code.", "找不到房间，请核对房码或重新复制邀请。", "Room not found. Check the code or copy a fresh invite."},
        {"房间满了。", "That room is full.", "房间已满，请等空位或加入其他房间。", "Room full. Wait for a space or join another room."},
        {"房间码不对——应是 4 位字母或数字。", "Bad room code - it is 4 letters and digits.", "房码应为4位字母或数字，请重新填写。", "Enter a room code with four letters or digits."},
        {"服务器忙——请稍后再试。", "The server is busy - try again later.", "暂时无法加入，请稍后重试。", "Could not join right now. Try again shortly."},
        {"服务器拒绝了该请求。", "The server did not like that request.", "请求未成功，请稍后重试。", "The request failed. Try again shortly."},
        {"房间已关闭。", "The room was closed.", "房间已关闭，请重新加入或建房。", "Room closed. Join or create a room."},
        {"房主离开了——房间已关闭。", "The host left - the room is closed.", "房主已退出，请重新加入或建房。", "The host left. Join or create a room."},
        {"本轮战斗玩家全部离线，重连保留期已结束。请返回联机页面重新开局。", "No combat players remain and reconnect grace has ended. Return to Online and start a new game.", "重连等待已结束，请重新建房开局。", "Reconnect wait ended. Create a room to start again."},
        {"房主已取消本次补位，已保存的进度会保留。", "The host cancelled your admission. Saved progress is retained.", "房主取消了补位，已保存的进度保留。", "The host cancelled joining. Saved progress is kept."},
        {"服务器关闭了房间。", "The server closed the room.", "房间已关闭，请重新加入或建房。", "Room closed. Join or create a room."},
        {"已取消重连。", "Reconnect cancelled.", "已退出重连，可重新加入房间。", "Reconnection cancelled. You can join a room again."},
        {"连接断开。", "Connection lost.", "连接已断开，请重新联机。", "Connection lost. Reconnect to play."},
    };
    for(const auto& entry:entries)if(raw==entry.zh || raw==entry.en)
        return chinese?entry.shownZh:entry.shownEn;
    return Transport(raw,chinese);
}
}
