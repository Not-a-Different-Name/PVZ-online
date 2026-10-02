#ifndef __GAMESELECTOR_H__
#define __GAMESELECTOR_H__

#include "../../ConstEnums.h"
#include "widget/Widget.h"
#include "widget/ButtonListener.h"
#include "AchievementsWidget.h"
#include "GameButton.h"

class LawnApp;
class ToolTipWidget;
class OnlineStatusWidget;
namespace Sexy
{
    class DialogButton;
}

using namespace Sexy;

enum SelectorAnimState
{
    SELECTOR_OPEN,
    SELECTOR_NEW_USER,
    SELECTOR_SHOW_SIGN,
    SELECTOR_IDLE
};

class GameSelector : public Widget, public ButtonListener
{
private:
    enum
    {
        GameSelector_Adventure = 100,
        GameSelector_Minigame = 101,
        GameSelector_Puzzle = 102,
        GameSelector_Options = 103,
        GameSelector_Help = 104,
        GameSelector_Quit = 105,
        GameSelector_ChangeUser = 106,
        GameSelector_Store = 107,
        GameSelector_Almanac = 108,
        GameSelector_ZenGarden = 109,
        GameSelector_Survival = 110,
        GameSelector_Zombatar = 111, // @Patoke: add stuff after 110
        GameSelector_AchievementsBack = 112,
        GameSelector_Achievements = 113,
        GameSelector_QuickPlay = 114,
        GameSelector_PvP = 115, // @pvz-online: M1 PVP 占位（无 ButtonDepress 分支）
        GameSelector_Online = 116 // @pvz-online: M2 联机面板
    };

public:
    LawnApp*                    mApp;                       //+0x8C
    NewLawnButton*              mAdventureButton;           //+0x90
    NewLawnButton*              mMinigameButton;            //+0x94
    NewLawnButton*              mPuzzleButton;              //+0x98
    NewLawnButton*              mOptionsButton;             //+0x9C
    NewLawnButton*              mQuitButton;                //+0xA0
    NewLawnButton*              mHelpButton;                //+0xA4
    NewLawnButton*              mStoreButton;               //+0xA8
    NewLawnButton*              mAlmanacButton;             //+0xAC
    NewLawnButton*              mZenGardenButton;           //+0xB0
    NewLawnButton*              mSurvivalButton;            //+0xB4
    NewLawnButton*              mChangeUserButton;          //+0xB8
    NewLawnButton*              mZombatarButton;             //+GOTY @Patoke: 0xC0
    NewLawnButton*              mAchievementsButton;        //+GOTY @Patoke: 0xC4
    NewLawnButton*              mQuickPlayButton;           //+GOTY @Patoke: 0xC8
    NewLawnButton*              mPvpButton;                 //+@pvz-online: M1 PVP 灰置占位
    NewLawnButton*              mOnlineButton;              //+@pvz-online: M2 联机入口
    // @pvz-online: 组队状态小条。是主菜单的子控件，跟着菜单一起滑进滑出；
    // 没有会话时它把自己藏起来（既不画也不吃点击）。
    OnlineStatusWidget*         mOnlineStatus;
    Widget*                     mOverlayWidget;             //+0xBC
    bool                        mStartingGame;              //+0xC0
    int                         mStartingGameCounter;       //+0xC4
    bool                        mMinigamesLocked;           //+0xC8
    bool                        mPuzzleLocked;              //+0xC9
    bool                        mSurvivalLocked;            //+0xCA
    bool                        mShowStartButton;           //+0xCB
    ParticleSystemID            mTrophyParticleID;          //+0xCC
    ReanimationID               mSelectorReanimID;          //+0xD0
    ReanimationID               mCloudReanimID[6];          //+0xD4
    int                         mCloudCounter[6];           //+0xEC
    ReanimationID               mFlowerReanimID[3];         //+0x104
    ReanimationID               mLeafReanimID;              //+0x110
    ReanimationID               mHandReanimID;              //+0x114
    int                         mLeafCounter;               //+0x118
    SelectorAnimState           mSelectorState;             //+0x11C
    int                         mLevel;                     //+0x120
    bool                        mLoading;                   //+0x124
    ToolTipWidget*              mToolTip;                   //+0x128
    bool                        mHasTrophy;                 //+0x12C
    bool                        mUnlockSelectorCheat;       //+0x12D
    int                         mSlideCounter;              //+GOTY @Patoke: 0x154
    int                         mStartX;                    //+GOTY @Patoke: 0x158
    int                         mStartY;                    //+GOTY @Patoke: 0x15C
    int                         mDestX;                     //+GOTY @Patoke: 0x160
    int                         mDestY;                     //+GOTY @Patoke: 0x164
    //ZombatarScreen*           mZombatarScreen;            //+GOTY @Patoke: 0x168
    AchievementsWidget*       mAchievementsWidget;        //+GOTY @Patoke: 0x16C
    // @pvz-online: the menu buttons are siblings of the achievements page, not children,
    // so they have to be hidden explicitly while the page is up (see Update).
    bool                        mMenuHiddenForAchievements;

public:
    GameSelector(LawnApp* theApp);
    virtual ~GameSelector();

    void                        SyncProfile(bool theShowLoading);
    virtual void                Draw(Graphics* g);
    virtual void                DrawOverlay(Graphics* g);
    virtual void                Update();
    virtual void                AddedToManager(WidgetManager* theWidgetManager);
    virtual void                RemovedFromManager(WidgetManager* theWidgetManager);
    virtual void                OrderInManagerChanged();
    virtual void                ButtonMouseEnter(int theId);
    virtual void                ButtonPress(int theId);
    virtual void                ButtonDepress(int theId);
    virtual void                ButtonDownTick(int){}
    virtual void                ButtonMouseLeave(int){}
    virtual void                ButtonMouseMove(int, int, int){}
    virtual void                KeyDown(KeyCode theKey);
    virtual void                KeyChar(char theChar);
    virtual void                MouseDown(int x, int y, int theClickCount);
    void                        TrackButton(DialogButton* theButton, const char* theTrackName, float theOffsetX, float theOffsetY);
    void                        SyncButtons();
    void                        AddTrophySparkle();
    void                        ClickedAdventure();
    // @pvz-online: 把菜单上的入口一律按下去 / 放开。开局转场那 4.5 秒和"广播换关命令、
    // 等队友回话"那段等待都用它——等待期间菜单留着当看板，但不能再点出别的页面
    // （等待中开出的模态框会横跨到入场之后）。放开的时机：selector 重建（ShowGameSelector）。
    void                        SetMenuButtonsDisabled(bool theDisabled);
    void                        UpdateTooltip();
    /*inline*/ bool             ShouldDoZenTuturialBeforeAdventure();
    void                        AddPreviewProfiles();
    // @Patoke: implement functions
    /*inline*/ void             SlideTo(int theX, int theY);
    void                        ShowAchievementsWidget(); // @Patoke: unofficial name
};

class GameSelectorOverlay : public Widget
{
public:
    GameSelector*               mParent;                    //+0x88

public:
    GameSelectorOverlay(GameSelector* theGameSelector);
    virtual ~GameSelectorOverlay() { }

    virtual void Draw(Graphics* g);
};

#endif
