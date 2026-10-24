#pragma once
#include "widget/Widget.h"
#include "widget/ButtonListener.h"
#include <vector>

class LawnApp;
class RunState;
namespace Sexy { class ButtonWidget; }
class RunFlowScreen : public Sexy::Widget, public Sexy::ButtonListener
{
    LawnApp* mApp;
    bool mModePage;
    bool mEndlessPage=false;
    int mEndlessOptions=141;
    std::string mCheckpointLine;
    bool mAdvanced = false;
    int mAdvancedPage = 0;
    std::vector<Sexy::ButtonWidget*> mModeButtons;
    void RefreshModePage();
    void RefreshDamageLabels();
    void RefreshRerollLabels();
    RunState* mPickRun=nullptr;
    unsigned int mExpectedCounter=0;
    int mLockedChoice=-1;
    bool mSubmitted=false;
    Sexy::ButtonWidget* mRefreshButton=nullptr;
    Sexy::ButtonWidget* mLockButtons[3]{};
    int mBuffMult = 0;
    Sexy::ButtonWidget* mBuffMultButton = nullptr;
    Sexy::ButtonWidget* mLadderButton = nullptr;
    Sexy::ButtonWidget* mZombieBuffButton = nullptr;
    Sexy::ButtonWidget* mSpeedButton=nullptr;
    int mSpeedTier=0;
    int mFirstWave=0;
    Sexy::ButtonWidget* mFirstWaveButton=nullptr;
    int mDifficulty = 1, mScale = 1, mTempo = 1, mZombotany = 0, mEndlessScene = 0, mBossFlag = 2, mLadder = 0, mZombieBuff = 1;
    Sexy::ButtonWidget* mOptionButtons[6]{};
    void RefreshOptionLabels();
public:
    explicit RunFlowScreen(LawnApp* app, bool modePage, int endlessScene=-1);
    ~RunFlowScreen() override;
    void Draw(Sexy::Graphics* g) override;
    void ButtonDepress(int id) override;
    void ButtonPress(int) override {}
    void ButtonDownTick(int) override {}
    void ButtonMouseEnter(int) override {}
    void ButtonMouseLeave(int) override {}
    void ButtonMouseMove(int, int, int) override {}
};
