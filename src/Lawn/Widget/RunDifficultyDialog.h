#pragma once

#include "../../LawnApp.h"
#include "../Online/NetSession.h"
#include "../Run/RunState.h"
#include "../System/PlayerInfo.h"
#include "LawnDialog.h"
#include "OnlineUiDraw.h"

class RunDifficultyDialog : public LawnDialog
{
    int mPressed = -1;
    bool mSaveFailed = false;

    static const char* Label(int diff)
    {
        switch (diff)
        {
        case RunState::RUN_DIFF_EASY: return ModText::Tr("轻松 ×0.5", "Easy x0.5");
        case RunState::RUN_DIFF_HIGH: return ModText::Tr("高压 ×2", "High x2");
        default: return ModText::Tr("标准 ×1", "Standard x1");
        }
    }

    static Rect ChoiceRect(int diff) { return Rect(40 + diff * 176, 154, 164, 46); }

    void Choose(int diff)
    {
        if (!CanChoose(mApp)) return;
        RunState* run = mApp->GetRunState();
        const int previous = run->mDiff;
        run->mDiff = diff;
        run->mNextDifficultyPending = false;
        if (!run->Save(mApp->mPlayerInfo->mId))
        {
            run->mDiff = previous;
            run->mNextDifficultyPending = true;
            mSaveFailed = true;
            MarkDirty();
            return;
        }
        mApp->KillDialog(Dialogs::DIALOG_RUN_DIFFICULTY);
    }

public:
    explicit RunDifficultyDialog(LawnApp* app)
        : LawnDialog(app, Dialogs::DIALOG_RUN_DIFFICULTY, true, "", "", "", Dialog::BUTTONS_NONE)
    {
        LawnApp::CenterDialog(this, 596, 232);
    }

    static bool CanChoose(LawnApp* app)
    {
        const RunState* run = app->GetRunState();
        const auto& session = app->mOnlineSession;
        return run && run->mNextDifficultyPending && !run->IsComplete() &&
            !app->mOnlineWaitingStartAck && !app->mOnlineRunStartHeld &&
            (!session || !session->IsActive() ||
                (session->GetRole() == NetSession::Role::HOST && !session->InWatchRoom() &&
                 !session->IsReconnecting() && session->mMatchMode < 0));
    }

    static bool Hold(LawnApp* app)
    {
        if (!CanChoose(app)) return false;
        if (!app->GetDialog(Dialogs::DIALOG_RUN_DIFFICULTY))
            app->AddDialog(Dialogs::DIALOG_RUN_DIFFICULTY, new RunDifficultyDialog(app));
        return true;
    }

    void Update() override
    {
        LawnDialog::Update();
        if (!CanChoose(mApp))
            mApp->KillDialog(Dialogs::DIALOG_RUN_DIFFICULTY);
    }

    void Draw(Graphics* g) override
    {
        const RunState* run = mApp->GetRunState();
        if (!run) return;
        SeatUi::Panel(g, Rect(0, 0, mWidth, mHeight));
        const auto heading = ModText::Tr("第 ", "Level ") + std::to_string(run->mLevelIndex + 1) +
            ModText::Tr(" 关难度", " difficulty");
        SeatUi::Text(g, heading, 36, 72, 524, 28, Color(245, 230, 165), true);
        const auto current = std::string(ModText::Tr("当前难度  ", "Current  ")) + Label(run->mDiff);
        SeatUi::Text(g, current, 36, 105, 524, 20, Color(225, 225, 210), true);
        if (mSaveFailed)
            SeatUi::Text(g, ModText::Tr("保存失败，请检查存储空间后重试。", "Save failed. Check storage and retry."),
                36, 132, 524, 17, Color(255, 180, 135), true);
        for (int diff = RunState::RUN_DIFF_EASY; diff <= RunState::RUN_DIFF_HIGH; ++diff)
            SeatUi::Button(g, ChoiceRect(diff), Label(diff), true, diff == mPressed, diff == run->mDiff);
    }

    void MouseDown(int x, int y, int clicks) override
    {
        mPressed = -1;
        if (clicks > 0 && clicks != 3)
            for (int diff = RunState::RUN_DIFF_EASY; diff <= RunState::RUN_DIFF_HIGH; ++diff)
                if (ChoiceRect(diff).Contains(x, y)) mPressed = diff;
        MarkDirty();
    }

    void MouseUp(int x, int y, int clicks) override
    {
        const int pressed = mPressed;
        mPressed = -1;
        MarkDirty();
        if (clicks != 3 && pressed >= 0 && ChoiceRect(pressed).Contains(x, y))
            Choose(pressed);
    }

    void KeyDown(KeyCode key) override
    {
        if (key == KEYCODE_ESCAPE || key == KEYCODE_RETURN)
            if (const RunState* run = mApp->GetRunState()) Choose(run->mDiff);
    }
};
