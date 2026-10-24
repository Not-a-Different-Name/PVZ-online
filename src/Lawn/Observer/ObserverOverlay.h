#pragma once
#include "ObserverState.h"
#include "Lawn/Widget/OnlineUiDraw.h"

namespace Observer {

enum class Control { None, Next, Return };
inline Sexy::Rect NextButton() { return Sexy::Rect(530, 8, 125, 46); }
inline Sexy::Rect ReturnButton() { return Sexy::Rect(665, 8, 125, 46); }

inline Control HitControl(int x, int y) {
    if (NextButton().Contains(x, y)) return Control::Next;
    if (ReturnButton().Contains(x, y)) return Control::Return;
    return Control::None;
}

inline std::string BoundedName(const std::string& target) {
    if (target.size() <= 128) return target;
    std::size_t end = 128;
    while (end && (static_cast<unsigned char>(target[end]) & 0xc0) == 0x80) --end;
    return target.substr(0, end);
}

inline void DrawOverlay(Sexy::Graphics* graphics, Status status, const std::string& target,
    unsigned wave = 0, unsigned total = 0, std::uint8_t flags = 0,
#ifdef __ANDROID__
    bool mobile = true, bool heldToWatch = true) {
#else
    bool mobile = false, bool heldToWatch = true) {
#endif
    using namespace Sexy;
    if (!graphics || status == Status::Closed) return;
    Graphics overlay(*graphics);
    if (status == Status::Waiting) {
        overlay.SetColor(Color(25, 30, 25));
        overlay.FillRect(0, 0, 800, 600);
        SeatUi::Text(&overlay, ModText::Tr("等待队友画面…", "Waiting for teammate snapshot..."), 80, 290, 640, 22, Color(245, 230, 165), true);
    }
    SeatUi::Strip(&overlay, Rect(0, 0, 800, 64));
    std::string name = BoundedName(target);
    std::string title = ModText::Tr("正在观看：", "Watching: ") + name;
    if (status == Status::Showing && total) title += "  " + std::to_string(wave) + "/" + std::to_string(total);
    SeatUi::Text(&overlay, title, 14, 28, 500, 20, Color(245, 230, 165));
    const char* hint = mobile ? ModText::Tr("只读观看 · 点击返回退出", "Read-only view · Back to return") : heldToWatch ?
        ModText::Tr("只读观看 · 1-6换人 · 松开V或Esc返回", "Read-only · 1-6 switch · release V / Esc to return") :
        ModText::Tr("只读观看 · 1-6换人 · Esc或返回退出", "Read-only · 1-6 switch · Esc / Back to return");
    SeatUi::Text(&overlay, hint, 14, 52, 500, 16, Color(215, 210, 185));
    SeatUi::Button(&overlay, NextButton(), ModText::Tr("换人", "Next"), true);
    SeatUi::Button(&overlay, ReturnButton(), ModText::Tr("返回", "Back"), true);
    if (status == Status::Showing && (flags & 3)) {
        const char* label = (flags & 1) ? ModText::Tr("对方已暂停", "Teammate paused") : ModText::Tr("对方已清场", "Teammate cleared");
        overlay.SetColor(Color(0, 0, 0, 170));
        overlay.FillRect(210, 265, 380, 44);
        SeatUi::Text(&overlay, label, 220, 295, 360, 22, Color(245, 230, 165), true);
    }
}

}
