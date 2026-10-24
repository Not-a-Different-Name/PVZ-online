#include "NameplateShop.h"
#include "NameplateDraw.h"
#include "../System/NameplateCatalog.h"
#include "../System/NameplateStorage.h"
#include "../Run/TeamGoalsStorage.h"
#include "../../LawnApp.h"

using namespace Sexy;

NameplateShop::NameplateShop(LawnApp* app):mApp(app),mPlayer(app->mPlayerInfo),mProfile(mPlayer?mPlayer->mId:0)
{
    Resize(0,0,800,600);
    if(mPlayer)mPreviewTitle=TeamGoalsStorage::Title(mApp);
}
bool NameplateShop::Valid() const
{
    return !mClosed && mPlayer && mApp->mPlayerInfo==mPlayer && mPlayer->mId==mProfile;
}
int NameplateShop::Hit(int x,int y) const
{
    if(!Valid())return -1;
    if(mConfirmation) {
        if(Rect(64,526,320,44).Contains(x,y))return 10;
        if(Rect(416,526,320,44).Contains(x,y))return 11;
        return -1;
    }
    for(int id=1;id<=3;++id)if(Rect(610,126+(id-1)*120,130,48).Contains(x,y))return id;
    if(Rect(64,526,200,44).Contains(x,y))return 0;
    if(Rect(560,526,176,44).Contains(x,y))return 9;
    return -1;
}
void NameplateShop::Activate(int action)
{
    if(!Valid())return;
    if(action==9){mClosed=true;return;}
    if(action==11){mConfirmation=0;mStatus.clear();return;}
    if(action>=1 && action<=3 && !NameplateCatalog::Owned(*mPlayer,action)) {
        if(mPlayer->mCoins<NameplateCatalog::Price(action)){mStatus=ModText::Tr("金币不足，未购买。","Not enough coins. Nothing was purchased.");return;}
        mConfirmation=action;
        mStatus=ModText::Tr("确认购买：","Confirm purchase: ")+std::string(NameplateCatalog::Name(action))+" -"+LawnApp::GetMoneyString(NameplateCatalog::Price(action));
        return;
    }
    if(!NameplateStorage::Ready(GetAppDataFolder()+"userdata",mProfile)) {
        mStatus=ModText::Tr("本次未扣款。请先继续原闯关存档，再更换名牌。", "Nothing charged. Continue the original run save before changing nameplates.");
        MarkDirty();return;
    }
    const auto save=[](PlayerInfo& player){return player.SaveDetailsChecked();};
    NameplateCatalog::Result result;
    if(action==10 && mConfirmation)result=NameplateCatalog::Buy(*mPlayer,mConfirmation,save);
    else if(action>=0 && action<=3)result=NameplateCatalog::Equip(*mPlayer,action,save);
    else return;
    if(result==NameplateCatalog::Result::SAVED || result==NameplateCatalog::Result::ALREADY_OWNED) {
        mStatus=action==10?ModText::Tr("购买成功，点“装备”使用。", "Purchased. Choose Equip to use it."):ModText::Tr("名牌已保存。","Nameplate saved.");
        mConfirmation=0;
    } else if(result==NameplateCatalog::Result::INSUFFICIENT)mStatus=ModText::Tr("金币不足，未购买。","Not enough coins. Nothing was purchased.");
    else mStatus=ModText::Tr("未保存，未扣款；请检查可用空间。","Not saved or charged. Check available storage.");
    MarkDirty();
}
void NameplateShop::Draw(Graphics* graphics)
{
    SeatUi::Panel(graphics,Rect(20,20,760,560));
    if(!Valid())return;
    SeatUi::Text(graphics,ModText::Tr("名牌收藏","Nameplate collection"),52,64,696,28,Color(245,230,165),true);
    SeatUi::Text(graphics,ModText::Tr("纯外观 · 永久保留 · 不增加属性","Cosmetic only / Permanent / No combat bonuses"),52,96,696,18,Color(205,205,190),true);
    std::string preview=mPlayer->mName.empty()?"Player":mPlayer->mName;
    if(mPreviewTitle)preview+=" · "+std::string(TeamGoalsStorage::Name(mPreviewTitle-1));
    for(int id=1;id<=3;++id) {
        const int top=120+(id-1)*120;
        NameplateDraw::Draw(graphics,Rect(52,top,350,62),id,preview);
        SeatUi::Text(graphics,NameplateCatalog::Name(id),424,top+22,174,20,Color(245,230,165));
        SeatUi::Text(graphics,LawnApp::GetMoneyString(NameplateCatalog::Price(id)),424,top+50,174,18,Color(205,205,190));
        const bool owned=NameplateCatalog::Owned(*mPlayer,id);
        const bool equipped=NameplateCatalog::Equipped(*mPlayer)==id;
        const bool enabled=!mConfirmation && !equipped && (owned || mPlayer->mCoins>=NameplateCatalog::Price(id));
        SeatUi::Button(graphics,Rect(610,top+6,130,48),equipped?ModText::Tr("已装备","Equipped"):owned?ModText::Tr("装备","Equip"):ModText::Tr("购买","Buy"),enabled,mPressed==id);
    }
    const std::string balance=ModText::Tr("余额：","Balance: ")+LawnApp::GetMoneyString(mPlayer->mCoins);
    SeatUi::Text(graphics,balance,52,478,696,20,Color(180,255,90));
    SeatUi::Text(graphics,mStatus,52,510,696,18,Color(245,230,165),true);
    if(mConfirmation) {
        SeatUi::Button(graphics,Rect(64,526,320,44),ModText::Tr("确认购买","Confirm purchase"),true,mPressed==10);
        SeatUi::Button(graphics,Rect(416,526,320,44),ModText::Tr("取消","Cancel"),true,mPressed==11);
    } else {
        SeatUi::Button(graphics,Rect(64,526,200,44),ModText::Tr("卸下名牌","Unequip"),NameplateCatalog::Equipped(*mPlayer)!=0,mPressed==0);
        SeatUi::Button(graphics,Rect(560,526,176,44),ModText::Tr("返回商店","Back to shop"),true,mPressed==9);
    }
}
void NameplateShop::Update()
{
    Widget::Update();
    if(!Valid())mClosed=true;
}
void NameplateShop::MouseDown(int x,int y,int clicks)
{
    mPressed=clicks==1?Hit(x,y):-1;MarkDirty();
}
void NameplateShop::MouseUp(int x,int y,int clicks)
{
    const int pressed=mPressed;mPressed=-1;
    if(clicks==1 && pressed>=0 && Hit(x,y)==pressed)Activate(pressed);
    MarkDirty();
}
void NameplateShop::KeyDown(KeyCode key)
{
    if(key==KEYCODE_ESCAPE){if(mConfirmation)mConfirmation=0;else mClosed=true;MarkDirty();}
}
