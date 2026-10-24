#pragma once

#include "ObserverClient.h"
#include "ObserverOverlay.h"
#include "ObserverReanimScene.h"
#include "widget/Widget.h"
#include <string>

namespace Observer {

using SceneClient = Client<Frame, ReanimScene>;

class LegacyPainter {
public:
    virtual ~LegacyPainter() = default;
    virtual void Draw(Sexy::Graphics* graphics, const Identity& identity, std::uint64_t sequence) = 0;
};

class Surface : public Sexy::Widget {
public:
    Surface(SceneClient& client, ReanimScene& scene) : client(client), scene(scene) {
        Resize(0, 0, 800, 600);
        mWantsFocus = true;
        mHasTransparencies = false;
    }
    bool SetName(const Identity& target, const std::string& name) {
        if (!(target == client.Target()) || !client.Active()) return false;
        targetName = BoundedName(name);
        nameIdentity = target;
        return true;
    }
    void SetLegacyPainter(LegacyPainter* painter) { legacy = painter; }

    void Draw(Sexy::Graphics* graphics) override {
        if (!graphics || !client.Active()) return;
        Sexy::Graphics surface(*graphics);
        surface.ClipRect(0, 0, 800, 600);
        surface.SetColor(Sexy::Color(25, 30, 25));
        surface.FillRect(0, 0, 800, 600);
        const bool mobile = client.GetPlatform() == Platform::Android;
        if (client.HasPicture()) {
            if (client.Mode() == ViewMode::NormalV2) scene.Draw(&surface);
            else if (client.Mode() == ViewMode::LegacyV1 && legacy)
                legacy->Draw(&surface, client.Target(), client.LegacySequence());
        }
        const std::string name = nameIdentity == client.Target() && client.Mode() != ViewMode::Selecting ? targetName :
            ModText::Tr("队友", "Teammate");
        Observer::DrawOverlay(&surface, client.DisplayStatus(), name, scene.Wave(), scene.Total(), scene.Flags(), mobile, client.HoldToWatch());
        const char* label = nullptr;
        if (client.Mode() == ViewMode::LegacyV1)
            label = ModText::Tr("兼容简图 · 非完整关卡画面", "Legacy overview · Not a full scene");
        else if (client.Mode() == ViewMode::Selecting)
            label = ModText::Tr("正在切换队友画面…", "Switching teammate view...");
        else if (client.Buffer().Paused())
            label = ModText::Tr("观战暂缓 · 优先保障对局", "View paused · Battle traffic takes priority");
        else if (!client.HasPicture())
            label = ModText::Tr("正在缓冲队友画面（1–2秒）", "Buffering teammate view (1–2 seconds)");
        if (label) {
            surface.SetColor(Sexy::Color(0, 0, 0, 180));
            surface.FillRect(110, 75, 580, 40);
            SeatUi::Text(&surface, label, 120, 103, 560, 20, Sexy::Color(245, 230, 165), true);
        }
    }

#ifdef __ANDROID__
    void KeyChar(char) override { client.Key(Input::Character); }
    void KeyText(std::string_view) override { client.Key(Input::Character); }
#else
    void KeyChar(SexyChar) override { client.Key(Input::Character); }
#endif
    void KeyDown(Sexy::KeyCode key) override { client.Key(Input::KeyDown, static_cast<int>(key)); }
    void KeyUp(Sexy::KeyCode key) override { client.Key(Input::KeyUp, static_cast<int>(key)); }
    void MouseDown(int x, int y, int clicks) override { client.Pointer(Input::PointerDown, PointerOf(clicks), x, y, clicks >= 0 && clicks != 3); }
    void MouseDown(int x, int y, int button, int clicks) override { client.Pointer(Input::PointerDown, button, x, y, button == 0); }
    void MouseUp(int x, int y) override { client.Pointer(Input::PointerUp, 0, x, y); }
    void MouseUp(int x, int y, int clicks) override { client.Pointer(Input::PointerUp, PointerOf(clicks), x, y, clicks >= 0 && clicks != 3); }
    void MouseUp(int x, int y, int button, int clicks) override { client.Pointer(Input::PointerUp, button, x, y, button == 0); }
    void MouseMove(int x, int y) override { client.Pointer(Input::PointerMove, 0, x, y); }
    void MouseDrag(int x, int y) override { client.Pointer(Input::PointerMove, 0, x, y); }
    void MouseWheel(int) override { client.Other(Input::Wheel); }

    bool TouchDown(std::int64_t native, int x, int y) {
        return client.Pointer(Input::PointerDown, touches.Down(native), x, y);
    }
    bool TouchMove(std::int64_t native, int x, int y) {
        return client.Pointer(Input::PointerMove, touches.Find(native), x, y);
    }
    bool TouchUp(std::int64_t native, int x, int y) {
        const auto slot = touches.Find(native);
        const bool consumed = client.Pointer(Input::PointerUp, slot, x, y);
        touches.Up(native);
        return consumed;
    }

private:
    static int PointerOf(int clicks) { return clicks < 0 ? 1 : clicks == 3 ? 2 : 0; }
    SceneClient& client;
    ReanimScene& scene;
    TouchSlots touches;
    LegacyPainter* legacy = nullptr;
    std::string targetName;
    Identity nameIdentity;
};

}
