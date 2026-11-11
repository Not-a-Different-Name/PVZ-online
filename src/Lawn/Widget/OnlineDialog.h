#pragma once
#include "widget/Dialog.h"
#include "widget/EditListener.h"
#include <memory>
#include "../Online/RoomCreateAttempt.h"
#include "../Online/AutoRoomLookup.h"
#include "RoomShareUi.h"
#include "OnlineRoomLayout.h"
#include <vector>
class LawnApp;
namespace Sexy { class EditWidget; class ButtonWidget; }
class OnlineDialog : public Sexy::Dialog, public Sexy::EditListener
{
public:
    explicit OnlineDialog(LawnApp* app);
    ~OnlineDialog() override;
    void Draw(Sexy::Graphics* g) override;
    void Update() override;
    void ButtonDepress(int id) override;
    void EditWidgetText(int id, const std::string&) override;
    void KeyDown(Sexy::KeyCode key) override;
private:
    void LeaveRoom();
    void MouseDown(int x,int y,int clicks) override;
    void MouseUp(int x,int y,int clicks) override;
    LawnApp* mApp;
    std::unique_ptr<Sexy::EditWidget> mAddress, mCode;
    std::vector<std::unique_ptr<Sexy::ButtonWidget>> mButtons;
    RelayNodes::Allocator mAllocator;
    RelayNodes::RoomCreateAttempt mCreation;
    RoomInvite::AutoRoomLookup mJoinLookup;
    RoomShareUi::Transfer mTransfer;
    std::string mPastedText,mPastedCode;
    void ApplyInvite(const std::string& text);
    std::string EffectiveInvite();
    std::string mPendingJoinCode;
    std::string mInputError;
    std::string mLocalIp;
    std::string mShownRoomCode;
    int mPage=0;
    OnlineRoomLayout::Selection mRoomSelection;
    bool mConfirmSolo=false;
    bool mStartingLocalHost=false;
    int mLocalHostTicks=0;
};
