#pragma once
#include "../Online/RoomClipboard.h"
#include "../Online/RoomInvite.h"
#include "../Online/AutoRoomLookup.h"
#include "../Online/PrivateEndpoint.h"
#include "../Online/NetSession.h"
#include "../ModText.h"
#include <utility>

namespace RoomShareUi {
enum class Action { CopyCode, CopyInvite, Share, Paste };
inline const char* FailureNotice(Action action) {
    if(action==Action::Paste)return ModText::Tr("没读到邀请，请复制后再点「粘贴加入」。","Could not read an invite. Copy one, then use Paste & join.");
    if(action==Action::CopyCode)return ModText::Tr("复制失败，请再点「复制房号」。","Copy failed. Choose Copy code to retry.");
    if(action==Action::CopyInvite)return ModText::Tr("复制失败，请再点「复制邀请」。","Copy failed. Choose Copy invite to retry.");
#ifdef __ANDROID__
    return ModText::Tr("没打开分享，请再点「分享」。","Could not open sharing. Choose Share to retry.");
#else
    return ModText::Tr("邀请没能复制，请再点「分享」。","Invite was not copied. Choose Share to retry.");
#endif
}
class Transfer {
    RoomClipboard::Job job;
    Action action = Action::CopyCode;
    bool reported = true;
    std::string pasted;
public:
    std::string notice;
    bool Busy() const { return job.state == RoomClipboard::State::Pending; }
    void Cancel(){job.Cancel();reported=true;pasted.clear();notice.clear();}
    bool Start(Action next, const NetSession* session, const std::string& localIp = {}) {
        if (Busy()) return false;
        std::string text;
        if (next != Action::Paste) {
            if (!session || session->GetRoomCode().empty()) return false;
            text = session->GetRoomCode();
            if (next != Action::CopyCode) {
                text = session->IsPrivateRoom() ? PrivateEndpoint::Invite(session->GetRelayHost() == "127.0.0.1" ? localIp :
                    session->GetRelayHost(), session->GetRelayPort(), text) :
                    RoomInvite::Share({session->GetRelayHost(), session->GetRelayPort()}, text);
                if (text.empty()) { notice = ModText::Tr("邀请还没生成，请稍后再点一次。", "Invite not ready. Try again shortly."); return false; }
            }
        }
        action = next; reported = false; notice.clear(); pasted.clear();
        return job.Start(next == Action::Paste ? RoomClipboard::Kind::Read : next == Action::Share ?
            RoomClipboard::Kind::Share : RoomClipboard::Kind::Copy, text);
    }
    void Update() {
        job.Poll();
        if (reported || Busy()) return;
        reported = true;
        if (job.state != RoomClipboard::State::Success) {
            notice = FailureNotice(action); return;
        }
        if (action == Action::Paste) pasted = job.text;
        else if (action == Action::Share) {
#ifdef __ANDROID__
            notice = ModText::Tr("选择要发送给的队友。", "Choose who to send the invite to.");
#else
            notice = ModText::Tr("邀请已复制，粘贴发给队友。", "Invite copied. Send it to your teammate.");
#endif
        } else notice = action == Action::CopyCode ? ModText::Tr("房号已复制，可发给队友。", "Room code copied. Send it to your teammate.") :
            ModText::Tr("邀请已复制，让队友点「粘贴加入」。", "Invite copied. Ask your teammate to use Paste & join.");
    }
    std::string TakePaste() { return std::exchange(pasted, std::string{}); }
};
inline const char* LookupError(RoomInvite::AutoRoomLookup::State state) {
    using Lookup = RoomInvite::AutoRoomLookup;
    return state == Lookup::AMBIGUOUS ? ModText::Tr("房号重复，请让房主「复制邀请」，再点「粘贴加入」。", "Code duplicated. Ask the host to Copy invite; use Paste & join.") :
        state == Lookup::MISSING ? ModText::Tr("没找到房间，请核对四位房号。", "Room not found. Check the four-character code.") :
        state == Lookup::INVALID ? ModText::Tr("填写四位房号，或点「粘贴加入」。", "Enter four characters, or choose Paste & join.") :
        ModText::Tr("暂时连不上，请重试「加入房间」或用「粘贴加入」。", "Could not connect. Retry Join room, or use Paste & join.");
}
}
