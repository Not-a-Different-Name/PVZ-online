#pragma once
#include "widget/Widget.h"
#include <string>

class LawnApp;
class PlayerInfo;
class NameplateShop:public Sexy::Widget
{
    LawnApp* mApp;
    PlayerInfo* mPlayer;
    unsigned long mProfile;
    int mPressed=-1;
    int mConfirmation=0;
    int mPreviewTitle=0;
    bool mClosed=false;
    std::string mStatus;
    int Hit(int x,int y) const;
    bool Valid() const;
    void Activate(int action);
public:
    explicit NameplateShop(LawnApp* app);
    bool Closed() const {return mClosed;}
    void Draw(Sexy::Graphics* graphics) override;
    void Update() override;
    void MouseDown(int x,int y,int clicks) override;
    void MouseUp(int x,int y,int clicks) override;
    void KeyDown(Sexy::KeyCode key) override;
};
