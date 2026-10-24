/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#include "../Board.h"
#include "../ModText.h"
#include "widget/ButtonWidget.h"
#include "GameButton.h"
#include "../Cutscene.h"
#include "AlmanacDialog.h"
#include "../LawnCommon.h"
#include "../../LawnApp.h"
#include "../System/Music.h"
#include "../../Resources.h"
#include "NewOptionsDialog.h"
#ifdef __ANDROID__
#include <SDL.h>
#endif
#include "OnlineUiDraw.h"
#include "../CoopPlay.h"
#include "../Online/NetSession.h"
#include "widget/WidgetManager.h"
#include "../../ConstEnums.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "widget/Slider.h"
#include "widget/Checkbox.h"
#include "../../PvzpLib/PvzpStringFile.h"

using namespace Sexy;

namespace
{
class OptionsHelpButton final : public LawnStoneButton
{
public:
    OptionsHelpButton(int id, ButtonListener* listener) : LawnStoneButton(nullptr, id, listener)
    {
        SetLabel(ModText::Tr("帮助", "Help"));
        mHasAlpha = true;
        mHasTransparencies = true;
    }

    void Draw(Graphics* graphics) override
    {
        if (mBtnNoDraw) return;
        const bool pressed = (mIsDown && mIsOver && !mDisabled) ^ mInverted;
        SeatUi::Button(graphics, Rect(0, 0, mWidth, mHeight), mLabel.c_str(),
            !mDisabled, pressed, mIsOver && !mDisabled);
    }
};
}

NewOptionsDialog::NewOptionsDialog(LawnApp* theApp, bool theFromGameSelector) :
	Dialog(nullptr, nullptr, Dialogs::DIALOG_NEWOPTIONS, true, "Options", "", "", Dialog::BUTTONS_NONE)
{
	mApp = theApp;
	mFromGameSelector = theFromGameSelector;
	SetButtonTextColor(Color(255, 255, 100));
	mAlmanacButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Almanac, this, "[VIEW_ALMANAC_BUTTON]");
	mRestartButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Restart, this, "[RESTART_LEVEL_BUTTON]");
	mBackToMainButton = MakeButton(NewOptionsDialog::NewOptionsDialog_MainMenu, this, "[MAIN_MENU_BUTTON]");
    mHelpButton = std::make_unique<OptionsHelpButton>(NewOptionsDialog_Help, this);
    mBilibiliButton = MakeButton(NewOptionsDialog_Bilibili, this, ModText::Tr("B站主页", "Bilibili"));
    mAfdianButton = MakeButton(NewOptionsDialog_Afdian, this, ModText::Tr("爱发电", "Afdian"));
    mBilibiliButton->SetVisible(mFromGameSelector);
    mAfdianButton->SetVisible(mFromGameSelector);

	mBackToGameButton = MakeNewButton(
		Dialog::ID_OK,
		this,
		"[BACK_TO_GAME]",
		nullptr,
		IMAGE_OPTIONS_BACKTOGAMEBUTTON0,
		IMAGE_OPTIONS_BACKTOGAMEBUTTON0,
		IMAGE_OPTIONS_BACKTOGAMEBUTTON2
	);
	mBackToGameButton->mTranslateX = 0;
	mBackToGameButton->mTranslateY = 0;
	mBackToGameButton->mTextOffsetX = -2;
	mBackToGameButton->mTextOffsetY = -5;
	mBackToGameButton->mTextDownOffsetX = 0;
	mBackToGameButton->mTextDownOffsetY = 1;
	mBackToGameButton->SetFont(FONT_DWARVENTODCRAFT36GREENINSET);
	mBackToGameButton->SetLabelColor(Color::White);
	mBackToGameButton->SetLabelHiliteColor(Color::White);
	mBackToGameButton->mHiliteFont = FONT_DWARVENTODCRAFT36BRIGHTGREENINSET;

	mMusicVolumeSlider = std::make_unique<Slider>(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_MusicVolume, this);
	double aMusicVolume = theApp->GetMusicVolume();
	aMusicVolume = std::max(0.0, std::min(1.0, aMusicVolume));
	mMusicVolumeSlider->SetValue(aMusicVolume);

	mSfxVolumeSlider = std::make_unique<Slider>(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_SoundVolume, this);
	mSfxVolumeSlider->SetValue(theApp->GetSfxVolume() / 0.65);

	mFullscreenCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_Fullscreen, this, !theApp->mIsWindowed);
	mHardwareAccelerationCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_HardwareAcceleration, this, theApp->Is3DAccelerated());
#ifdef __ANDROID__
	// Android always renders through GLES.
	mHardwareAccelerationCheckbox->SetVisible(false);
#endif

	if (mApp->IsOnlineGame()) mRestartButton->SetVisible(false);
	if (mFromGameSelector)
	{
		mRestartButton->SetVisible(false);
		mBackToGameButton->SetLabel("[DIALOG_BUTTON_OK]");
		if (mApp->HasFinishedAdventure() && !mApp->IsTrialStageLocked())
		{
			mBackToMainButton->SetLabel("[CREDITS]");
		}
		else
		{
			mBackToMainButton->SetVisible(false);
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mRestartButton->SetVisible(false);
	}
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && !mApp->mBoard->mCutScene->IsSurvivalRepick())
	{
		mRestartButton->SetVisible(false);
	}
	if (!mApp->CanShowAlmanac() ||
		mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mFromGameSelector)
	{
		mAlmanacButton->SetVisible(false);
	}
}

NewOptionsDialog::~NewOptionsDialog() = default;

int NewOptionsDialog::GetPreferredHeight([[maybe_unused]] int theWidth)
{
	return IMAGE_OPTIONS_MENUBACK->mWidth;
}

void NewOptionsDialog::AddedToManager(Sexy::WidgetManager* theWidgetManager)
{
	Dialog::AddedToManager(theWidgetManager);
	AddWidget(mAlmanacButton.get());
	AddWidget(mRestartButton.get());
	AddWidget(mBackToMainButton.get());
	AddWidget(mMusicVolumeSlider.get());
	AddWidget(mSfxVolumeSlider.get());
	AddWidget(mHardwareAccelerationCheckbox.get());
	AddWidget(mFullscreenCheckbox.get());
	AddWidget(mBackToGameButton.get());
    AddWidget(mHelpButton.get());
    AddWidget(mBilibiliButton.get());
    AddWidget(mAfdianButton.get());
}

void NewOptionsDialog::RemovedFromManager(Sexy::WidgetManager* theWidgetManager)
{
    if (mApp->mMusic) mApp->mMusic->StopMusicPreview();
	Dialog::RemovedFromManager(theWidgetManager);
	RemoveWidget(mAlmanacButton.get());
	RemoveWidget(mMusicVolumeSlider.get());
	RemoveWidget(mSfxVolumeSlider.get());
	RemoveWidget(mFullscreenCheckbox.get());
	RemoveWidget(mHardwareAccelerationCheckbox.get());
	RemoveWidget(mBackToMainButton.get());
	RemoveWidget(mBackToGameButton.get());
	RemoveWidget(mRestartButton.get());
    RemoveWidget(mHelpButton.get());
    RemoveWidget(mBilibiliButton.get());
    RemoveWidget(mAfdianButton.get());
}

void NewOptionsDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
    if (mRunInfoOpen || mMusicOpen || mRunSettingsOpen) { Dialog::Resize(theX, theY, theWidth, theHeight); return; }
	Dialog::Resize(theX, theY, theWidth, theHeight);
    if (!mFromGameSelector) {
        mMusicVolumeSlider->Resize(300, 112, 200, 40);
        mSfxVolumeSlider->Resize(300, 156, 200, 40);
        mHardwareAccelerationCheckbox->Resize(224, 202, 46, 45);
        mFullscreenCheckbox->Resize(498, 202, 46, 45);
#ifdef __ANDROID__
        mFullscreenCheckbox->mX = 342;
#endif
        mAlmanacButton->Resize(54, 326, 234, 46);
        mRestartButton->Resize(312, 326, 234, 46);
        if (mAlmanacButton->mVisible != mRestartButton->mVisible) {
            mAlmanacButton->mX = 183;
            mRestartButton->mX = 183;
        }
        mBackToMainButton->Resize(54, 386, 492, 46);
        mBackToGameButton->Resize((mWidth - IMAGE_OPTIONS_BACKTOGAMEBUTTON0->mWidth) / 2,
            438, IMAGE_OPTIONS_BACKTOGAMEBUTTON0->mWidth, IMAGE_OPTIONS_BACKTOGAMEBUTTON0->mHeight);
        const Rect help = RunInfoEntryRect();
        mHelpButton->Resize(help.mX, help.mY, help.mWidth, help.mHeight);
        return;
    }
	mMusicVolumeSlider->Resize(199, 116, 135, 40);
	mSfxVolumeSlider->Resize(199, 143, 135, 40);
	mHardwareAccelerationCheckbox->Resize(283, 175, 46, 45);
	mFullscreenCheckbox->Resize(284, 206, 46, 45);
	mAlmanacButton->Resize(107, 241, 209, 46);
	mRestartButton->Resize(mAlmanacButton->mX, mAlmanacButton->mY + 43, 209, 46);
	mBackToMainButton->Resize(mRestartButton->mX, mRestartButton->mY + 43, 209, 46);
	mBackToGameButton->Resize(30, 381, mBackToGameButton->mWidth, mBackToGameButton->mHeight);
    mBilibiliButton->Resize(76, 272, 132, 42);
    mAfdianButton->Resize(218, 272, 132, 42);
    const Rect help = RunInfoEntryRect();
    mHelpButton->Resize(help.mX, help.mY, help.mWidth, help.mHeight);

	if (mFromGameSelector)
	{
		mMusicVolumeSlider->mY += 5;
		mSfxVolumeSlider->mY += 10;
		mHardwareAccelerationCheckbox->mY += 15;
		mFullscreenCheckbox->mY += 20;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mAlmanacButton->mY += 43;
	}
}

void NewOptionsDialog::Draw(Sexy::Graphics* g)
{
    if (mMusicOpen) { mMusicView.Draw(g, mRunInfoDown); return; }
    if (mRunSettingsOpen) {
        mRunSettingsView.Draw(g, mApp->GetRunState(), mApp->IsOnlineGame(), mRunInfoDown);
        return;
    }
    if (mRunInfoOpen) { DrawRunInfo(g); return; }
    if (!mFromGameSelector) {
        g->DrawImage(IMAGE_OPTIONS_MENUBACK, Rect(0, 0, mWidth, mHeight),
            Rect(0, 0, IMAGE_OPTIONS_MENUBACK->mWidth, IMAGE_OPTIONS_MENUBACK->mHeight));
        PvzpDrawString(g, ModText::Tr("音乐音量", "Music Volume"), 274, 138,
            FONT_DWARVENTODCRAFT18, Color(107, 109, 145), DS_ALIGN_RIGHT);
        PvzpDrawString(g, ModText::Tr("音效音量", "Sound Volume"), 274, 182,
            FONT_DWARVENTODCRAFT18, Color(107, 109, 145), DS_ALIGN_RIGHT);
#ifndef __ANDROID__
        PvzpDrawString(g, ModText::Tr("3D 加速", "3D"), 212, 228,
            FONT_DWARVENTODCRAFT18, Color(107, 109, 145), DS_ALIGN_RIGHT);
#endif
#ifdef __ANDROID__
        const int fullscreenLabelX = 330;
#else
        const int fullscreenLabelX = 486;
#endif
        PvzpDrawString(g, ModText::Tr("全屏", "Full Screen"), fullscreenLabelX, 228,
            FONT_DWARVENTODCRAFT18, Color(107, 109, 145), DS_ALIGN_RIGHT);
        SeatUi::Button(g, MusicEntryRect(), ModText::Tr("音乐", "Music"), true, mMusicEntryDown);
        if (mApp->IsRunMode())
            SeatUi::Button(g, RunSettingsEntryRect(), ModText::Tr("开局设定", "Setup"), true, mRunSettingsEntryDown);
        if (mApp->mOnlineSession && mApp->mOnlineSession->IsPausedByPeer())
            PvzpDrawString(g, ModText::Tr("队友暂停了游戏", "Teammate paused the game"), mWidth / 2, 96,
                FONT_DWARVENTODCRAFT18, Color(255, 220, 100), DS_ALIGN_CENTER);
        else if (mApp->mBoard && mApp->mBoard->mKeyboardControlsUsed && mApp->mBoard->HasPlantKeyboardControls())
            PvzpDrawString(g, ModText::Tr("1-9/0 选卡  Q 铲子  H/J 血量", "1-9/0 Cards  Q Shovel  H/J Health"),
                mWidth / 2, 96, ModText::FontFor(FONT_DWARVENTODCRAFT18, 16),
                Color(205, 205, 190), DS_ALIGN_CENTER);
        return;
    }
	g->DrawImage(IMAGE_OPTIONS_MENUBACK, 0, 0);

	int aSfxOffset = 0;
	int a3DAccelOffset = 0;
	int aFullScreenOffset = 0;
	if (mFromGameSelector)
	{
		aSfxOffset = 10;
		a3DAccelOffset = 15;
		aFullScreenOffset = 20;
	}
	Sexy::Color aTextColor(107, 109, 145);

	int aSliderLabelsX = mApp->GetInteger("OPTION_DLG_SLIDER_LABELS_OFFSET_X", 186);
	int aCheckboxLabelsX = mApp->GetInteger("OPTION_DLG_CHECKBOX_LABELS_OFFSET_X", 274);
	float aFontScale = static_cast<float>(mApp->GetDouble("OPTION_DLG_LABEL_FONT_SCALE", 1.0));
	if (aFontScale != 1.0f)
		g->SetScale(aFontScale, aFontScale, 0.0f, 0.0f);
	PvzpDrawString(g, mApp->GetString("OPTIONS_SOUNDFX", "Sound FX"), aSliderLabelsX, 167 + aSfxOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
#ifndef __ANDROID__
	PvzpDrawString(g, mApp->GetString("OPTIONS_3D_ACCELERATION", "3D Acceleration"), aCheckboxLabelsX, 197 + a3DAccelOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
#endif
	PvzpDrawString(g, mApp->GetString("OPTIONS_FULL_SCREEN", "Full Screen"), aCheckboxLabelsX, 229 + aFullScreenOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
	if (aFontScale != 1.0f)
		g->SetScale(1.0f, 1.0f, 0.0f, 0.0f);
    SeatUi::Button(g, MusicEntryRect(), ModText::Tr("音乐", "Music"), true, mMusicEntryDown);
    if (mApp->IsRunMode())
        SeatUi::Button(g, RunSettingsEntryRect(), ModText::Tr("开局设定", "Setup"), true, mRunSettingsEntryDown);
    if (!mFromGameSelector && mApp->mBoard && mApp->mBoard->mKeyboardControlsUsed &&
        mApp->mBoard->HasPlantKeyboardControls())
    {
        PvzpDrawString(g, ModText::Tr("1-9/0 选卡  Q 铲子  H/J 血量", "1-9/0 Cards  Q Shovel  H/J Health"),
            mWidth / 2, 106, ModText::FontFor(FONT_DWARVENTODCRAFT18, 16),
            aTextColor, DrawStringJustification::DS_ALIGN_CENTER);
    }
}

void NewOptionsDialog::SliderVal(int theId, double theVal)
{
	switch (theId)
	{
	case NewOptionsDialog::NewOptionsDialog_MusicVolume:
		mApp->SetMusicVolume(theVal);
		mApp->mSoundSystem->RehookupSoundWithMusicVolume();
		break;

	case NewOptionsDialog::NewOptionsDialog_SoundVolume:
		mApp->SetSfxVolume(theVal * 0.65);
		mApp->mSoundSystem->RehookupSoundWithMusicVolume();
		if (!mSfxVolumeSlider->mDragging)
		{
			mApp->PlaySample(SOUND_BUTTONCLICK);
		}
		break;
	}
}

void NewOptionsDialog::CheckboxChecked(int theId, bool checked)
{
	switch (theId)
	{
	case NewOptionsDialog::NewOptionsDialog_Fullscreen:
		if (!checked && mApp->mForceFullscreen)
		{
			mApp->DoDialog(
				Dialogs::DIALOG_COLORDEPTH_EXP,
				true,
				mApp->GetString("NO_WINDOWED_MODE", "No Windowed Mode"),
				ModText::Tr("暂不能切换窗口，请保持全屏。", "Windowed mode is unavailable. Keep Full Screen on."),
				"[DIALOG_BUTTON_OK]",
				Dialog::BUTTONS_FOOTER
			);

			mFullscreenCheckbox->SetChecked(true, false);
		}
		break;

	case NewOptionsDialog::NewOptionsDialog_HardwareAcceleration:
		if (checked)
		{
			if (!mApp->Is3DAccelerationSupported())
			{
				mHardwareAccelerationCheckbox->SetChecked(false, false);
				mApp->DoDialog(
					Dialogs::DIALOG_INFO,
					true,
					mApp->GetString("NOT_SUPPORTED", "Not Supported"),
					ModText::Tr("此设备不支持3D加速，请保持关闭。", "This device cannot use 3D. Leave it off."),
					"[DIALOG_BUTTON_OK]",
					Dialog::BUTTONS_FOOTER
				);
			}
			else if (!mApp->Is3DAccelerationRecommended())
			{
				mApp->DoDialog(
					Dialogs::DIALOG_INFO,
					true,
					mApp->GetString("DIALOG_WARNING", "Warning"),
					ModText::Tr("如画面变卡，请关闭3D加速。", "Turn off 3D if performance gets worse."),
					"[DIALOG_BUTTON_OK]",
					Dialog::BUTTONS_FOOTER
				);
			}
		}
		break;
	}
}

void NewOptionsDialog::KeyDown(Sexy::KeyCode theKey)
{
    if (mWidgetManager && mWidgetManager->mIsKeyRepeat &&
        (theKey == KEYCODE_ESCAPE || theKey == KEYCODE_SPACE || theKey == KEYCODE_RETURN)) return;
    if (mMusicOpen) {
        if (theKey == KeyCode::KEYCODE_ESCAPE) CloseRunInfo();
        else if (theKey == KeyCode::KEYCODE_PRIOR || theKey == KeyCode::KEYCODE_LEFT ||
            theKey == KeyCode::KEYCODE_NEXT || theKey == KeyCode::KEYCODE_RIGHT) {
            mMusicView.Flip(theKey == KeyCode::KEYCODE_NEXT || theKey == KeyCode::KEYCODE_RIGHT ? 1 : -1);
            MarkDirty();
        }
        return;
    }
    if (mRunSettingsOpen) {
        if (theKey == KeyCode::KEYCODE_ESCAPE) CloseRunInfo();
        else if (theKey == KeyCode::KEYCODE_PRIOR || theKey == KeyCode::KEYCODE_LEFT ||
            theKey == KeyCode::KEYCODE_NEXT || theKey == KeyCode::KEYCODE_RIGHT) {
            mRunSettingsView.Flip(theKey == KeyCode::KEYCODE_NEXT || theKey == KeyCode::KEYCODE_RIGHT ? 1 : -1,
                mApp->GetRunState());
            MarkDirty();
        }
        return;
    }
    if (mRunInfoOpen) {
        if (theKey == KeyCode::KEYCODE_PRIOR || theKey == KeyCode::KEYCODE_LEFT ||
            theKey == KeyCode::KEYCODE_NEXT || theKey == KeyCode::KEYCODE_RIGHT)
            RunInfoFlipPage(theKey == KeyCode::KEYCODE_NEXT || theKey == KeyCode::KEYCODE_RIGHT ? 1 : -1);
        else if (theKey == KeyCode::KEYCODE_ESCAPE) CloseRunInfo();
        return;
    }
	if (mApp->mBoard)
	{
		mApp->mBoard->DoTypingCheck(theKey);
	}

	if (theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN)
	{
		Dialog::ButtonDepress(Dialog::ID_OK);
	}
	else if (theKey == KeyCode::KEYCODE_ESCAPE)
	{
		Dialog::ButtonDepress(Dialog::ID_CANCEL);
	}
}

void NewOptionsDialog::ButtonPress([[maybe_unused]] int theId)
{
	mApp->PlaySample(SOUND_GRAVEBUTTON);
}

void NewOptionsDialog::ButtonDepress(int theId)
{
    if (mMusicOpen || mRunInfoOpen || mRunSettingsOpen) return;
    if (mFromGameSelector && (theId == NewOptionsDialog_Bilibili || theId == NewOptionsDialog_Afdian)) {
        OpenCreatorLink(theId == NewOptionsDialog_Bilibili ? 510 : 511);
        return;
    }
    if (theId == NewOptionsDialog_Help) { OpenRunInfo(); return; }
	Dialog::ButtonDepress(theId);

	switch (theId)
	{
	case NewOptionsDialog::NewOptionsDialog_Almanac:
	{
		AlmanacDialog* aDialog = mApp->DoAlmanacDialog(SeedType::SEED_NONE, ZombieType::ZOMBIE_INVALID);
		aDialog->WaitForResult(true);
		break;
	}

	case NewOptionsDialog::NewOptionsDialog_MainMenu:
	{
		if (mFromGameSelector)
		{
			mApp->KillNewOptionsDialog();
			mApp->KillGameSelector();
			mApp->ShowAwardScreen(AwardType::AWARD_CREDITS_ZOMBIENOTE, false);
		}
		else if (mApp->mBoard && mApp->mBoard->NeedSaveGame())
		{
			mApp->DoConfirmBackToMain();
		}
		else if (mApp->mBoard && mApp->mBoard->mCutScene && mApp->mBoard->mCutScene->IsSurvivalRepick())
		{
			mApp->DoConfirmBackToMain();
		}
		else
		{
			mApp->mBoardResult = BoardResult::BOARDRESULT_QUIT;
			mApp->DoBackToMain();
		}
		break;
	}

	case NewOptionsDialog::NewOptionsDialog_Restart:
	{
		if (mApp->mBoard)
		{
			std::string aDialogTitle;
			std::string aDialogMessage;
			if (mApp->IsPuzzleMode())
			{
				aDialogTitle = "[RESTART_PUZZLE_HEADER]";
				aDialogMessage = "[RESTART_PUZZLE_BODY]";
			}
			else if (mApp->IsChallengeMode())
			{
				aDialogTitle = "[RESTART_CHALLENGE_HEADER]";
				aDialogMessage = "[RESTART_CHALLENGE_BODY]";
			}
			else if (mApp->IsSurvivalMode())
			{
				aDialogTitle = "[RESTART_SURVIVAL_HEADER]";
				aDialogMessage = "[RESTART_SURVIVAL_BODY]";
			}
			else
			{
				aDialogTitle = "[RESTART_LEVEL_HEADER]";
				aDialogMessage = "[RESTART_LEVEL_BODY]";
			}

			LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(Dialogs::DIALOG_CONFIRM_RESTART, true, aDialogTitle, aDialogMessage, "", Dialog::BUTTONS_YES_NO);
			aDialog->mLawnYesButton->mLabel = mApp->GetString("RESTART_LABEL", "RESTART");
			aDialog->mLawnNoButton->mLabel = PvzpStringTranslate("[DIALOG_BUTTON_CANCEL]");

			if (aDialog->WaitForResult(true) == Dialog::ID_YES)
			{
				mApp->mMusic->StopAllMusic();
				mApp->mSoundSystem->CancelPausedFoley();
				mApp->KillNewOptionsDialog();
				mApp->mBoardResult = BoardResult::BOARDRESULT_RESTART;
				mApp->mSawYeti = mApp->mBoard->mKilledYeti;
				mApp->PreNewGame(mApp->mGameMode, false);
			}
		}
		break;
	}

	case NewOptionsDialog::NewOptionsDialog_Update:
		mApp->CheckForUpdates();
		break;
	}
}


bool NewOptionsDialog::RunInfoAvailable() { return true; }
Sexy::Rect NewOptionsDialog::RunInfoEntryRect() {
    if (!mFromGameSelector) return Rect(mApp->IsRunMode() ? 390 : 312, 264, 156, 46);
    return Rect(ModText::IsChinese() ? 96 : 50, 226, 76, 34);
}
void NewOptionsDialog::RunInfoFlipPage(int delta) {
    mRunInfoView.Flip(delta, mApp->IsRunMode() ? mApp->GetRunState() : nullptr);
    MarkDirty();
}
void NewOptionsDialog::MouseDown(int x, int y, int theClickCount) {
    if (mRunSettingsOpen) {
        mRunInfoDown = theClickCount > 0 ? mRunSettingsView.Hit(x, y, mApp->GetRunState()) : -1;
        MarkDirty();
        return;
    }
    if (mMusicOpen) {
        mRunInfoDown = theClickCount > 0 ? mMusicView.Hit(x, y) : -1;
        MarkDirty();
        return;
    }
    if (mRunInfoOpen) {
        mRunInfoDown = theClickCount > 0 ? mRunInfoView.Hit(x, y,
            mApp->IsRunMode() ? mApp->GetRunState() : nullptr, mApp->mBoard != nullptr, mApp->IsOnlineGame()) : -1;
        MarkDirty();
        return;
    }
    if (theClickCount > 0 && mApp->IsRunMode() && RunSettingsEntryRect().Contains(x, y)) {
        mRunSettingsEntryDown = true;
        MarkDirty();
        return;
    }
    if (theClickCount > 0 && MusicEntryRect().Contains(x, y)) {
        mMusicEntryDown = true;
        MarkDirty();
        return;
    }
    Dialog::MouseDown(x, y, theClickCount);
}
void NewOptionsDialog::MouseUp(int x, int y, int theClickCount) {
    if (mRunSettingsOpen) {
        const int action = mRunInfoDown;
        mRunInfoDown = -1;
        if (theClickCount > 0 && action >= 0 && action == mRunSettingsView.Hit(x, y, mApp->GetRunState())) {
            if (action == 100) CloseRunInfo();
            else mRunSettingsView.Act(action, mApp->GetRunState());
        }
        MarkDirty();
        return;
    }
    if (mMusicOpen) {
        const int action = mRunInfoDown;
        mRunInfoDown = -1;
        if (theClickCount > 0 && action >= 0 && action == mMusicView.Hit(x, y)) MusicAction(action);
        MarkDirty();
        return;
    }
    if (!mRunInfoOpen) {
        bool music = mMusicEntryDown && theClickCount > 0 && MusicEntryRect().Contains(x, y);
        const bool settings = mRunSettingsEntryDown && theClickCount > 0 && RunSettingsEntryRect().Contains(x, y);
        const bool entryPressed = mMusicEntryDown || mRunSettingsEntryDown;
        mMusicEntryDown = false;
        mRunSettingsEntryDown = false;
        MarkDirty();
        if (settings) OpenRunSettings();
        else if (music) OpenMusic();
        else if (!entryPressed) Dialog::MouseUp(x, y, theClickCount);
        return;
    }
    auto* run = mApp->IsRunMode() ? mApp->GetRunState() : nullptr;
    int action = mRunInfoDown;
    mRunInfoDown = -1;
    if (theClickCount <= 0 || action < 0 ||
        action != mRunInfoView.Hit(x, y, run, mApp->mBoard != nullptr, mApp->IsOnlineGame())) return;
    if (action == 100) { CloseRunInfo(); return; }
    if (RunInfoView::CreatorUrl(action)) {
        OpenCreatorLink(action);
        return;
    }
    mRunInfoView.Act(action, run);
    if (action == 500) {
#ifdef __ANDROID__
        if (!ModText::SetLanguageSetting((ModText::GetLanguageSetting() + 1) % 3))
            mApp->LawnMessageBox(DIALOG_MESSAGE, ModText::Tr("保存失败", "Save failed"),
                ModText::Tr("语言设置未保存，请重试。", "Language was not saved. Please retry."), "OK", "", Dialog::BUTTONS_FOOTER);
#else
        ModText::SetLanguageSetting((ModText::GetLanguageSetting() + 1) % 3);
#endif
    }
    if (action == 501 && mApp->mBoard) mApp->mBoard->SetPlantHealthVisible(!mApp->mBoard->PlantHealthVisible());
    if (action == 502 && mApp->mBoard) mApp->mBoard->SetZombieHealthVisible(!mApp->mBoard->ZombieHealthVisible());
    if (action == 503) {
        bool next = !mApp->mSlideCollect;
#ifdef __ANDROID__
        bool saved = ModText::SaveSlideCollect(next);
#else
        bool saved = ModText::RegWriteDword("ModSlideCollect", next ? 1 : 0);
#endif
        if (saved) mApp->mSlideCollect = next;
        else mApp->LawnMessageBox(DIALOG_MESSAGE, ModText::Tr("保存失败", "Save failed"),
            ModText::Tr("划取设置未保存，请重试。", "Collection setting was not saved. Retry."), "OK", "", Dialog::BUTTONS_FOOTER);
    }
    if (action == 504) { CloseRunInfo(); mApp->ShowCoopPlay(); return; }
    MarkDirty();
}
void NewOptionsDialog::OpenCreatorLink(int action) {
    const char* url = RunInfoView::CreatorUrl(action);
    if (!url) return;
#ifdef __ANDROID__
    const bool opened = SDL_OpenURL(url) == 0;
#else
    const bool opened = mApp->OpenURL(url, false);
#endif
    if (!opened) {
        const std::string message = std::string(ModText::Tr("无法打开浏览器，请访问：\n", "Could not open a browser. Visit:\n")) + url;
        mApp->LawnMessageBox(DIALOG_MESSAGE, ModText::Tr("打开链接", "Open link"),
            message.c_str(), "OK", "", Dialog::BUTTONS_FOOTER);
    }
}
void NewOptionsDialog::LostFocus() {
    mMusicEntryDown = false;
    mRunSettingsEntryDown = false;
    if (mMusicOpen && mApp->mMusic) {
        mApp->mMusic->StopMusicPreview();
        mMusicView.mStatus.clear();
        mMusicView.mError = false;
    }
    mRunInfoDown = -1;
    Dialog::LostFocus();
}
void NewOptionsDialog::MouseWheel(int delta) {
    if (mRunSettingsOpen) {
        if (delta) mRunSettingsView.Flip(delta < 0 ? 1 : -1, mApp->GetRunState());
        MarkDirty();
        return;
    }
    if (mMusicOpen) { if (delta) mMusicView.Flip(delta < 0 ? 1 : -1); MarkDirty(); return; }
    if (mRunInfoOpen) { if (delta) RunInfoFlipPage(delta < 0 ? 1 : -1); return; }
    Dialog::MouseWheel(delta);
}
void NewOptionsDialog::DrawRunInfo(Sexy::Graphics* graphics) {
#ifdef __ANDROID__
    std::string language = ModText::SettingName();
#else
    const char* languages[] = {ModText::Tr("自动", "Auto"), ModText::Tr("中文", "Chinese"), "English"};
    std::string language = languages[ModText::GetLanguageSetting()];
#endif
    mRunInfoView.Draw(graphics, mApp->IsRunMode() ? mApp->GetRunState() : nullptr,
        mApp->mBoard != nullptr, mApp->IsOnlineGame(),
        mApp->mBoard && mApp->mBoard->PlantHealthVisible(),
        mApp->mBoard && mApp->mBoard->ZombieHealthVisible(), mApp->mSlideCollect, language, mRunInfoDown);
}

Sexy::Rect NewOptionsDialog::MusicEntryRect() {
    if (!mFromGameSelector) return Rect(mApp->IsRunMode() ? 54 : 132, 264, 156, 46);
    return Rect(94, 117, 96, 34);
}
void NewOptionsDialog::OpenMusic() {
    if (!mApp->mMusic || mMusicOpen || mRunInfoOpen || mRunSettingsOpen) return;
    mApp->mMusic->LoadBattleMusicSettings();
    mMusicView = MusicSelectionView();
    mMusicView.mTune = mApp->mBattleMusic.mSelectedTune;
    mMusicView.mBossMusic = mApp->mBattleMusic.mBossMusic;
    mMusicView.mPage = mMusicView.mTune > 6 ? 1 : 0;
    OpenOptionsPage(true);
}
void NewOptionsDialog::MusicAction(int action) {
    if (action == 100) { CloseRunInfo(); return; }
    if (action == 201) {
        const bool playing = mApp->mMusic->StartMusicPreview(static_cast<MusicTune>(mMusicView.mTune));
        mMusicView.mError = !playing;
        mMusicView.mStatus = playing ? ModText::Tr("试听中；点「应用」保存，返回不保存。", "Previewing. Apply saves; Back does not.") :
            ModText::Tr("请调高「音乐音量」，再点「试听」。", "Raise Music Volume, then try Preview.");
    } else if (action == 202) {
        const bool saved = mApp->mMusic->SaveBattleMusicSettings(mMusicView.mTune, mMusicView.mBossMusic);
        mMusicView.mError = !saved;
        mMusicView.mStatus = saved ? ModText::Tr("已保存，只改变本机战斗配乐。", "Saved for battles on this device only.") :
            ModText::Tr("保存失败，原设置未更改。", "Save failed. Previous settings are unchanged.");
    } else {
        mMusicView.mStatus.clear();
        mMusicView.mError = false;
        if (action == 200 || (action >= 300 && action < 306)) {
            mApp->mMusic->StopMusicPreview();
            mMusicView.mTune = action == 200 ? 0 : mMusicView.mPage * 6 + action - 299;
        }
        if (action == 203) mMusicView.mBossMusic = !mMusicView.mBossMusic;
        if (action == 204 || action == 205) mMusicView.Flip(action == 204 ? -1 : 1);
    }
}
void NewOptionsDialog::OpenRunInfo() {
    OpenOptionsPage(false);
}
Sexy::Rect NewOptionsDialog::RunSettingsEntryRect() {
    if (!mFromGameSelector) return Rect(222, 264, 156, 46);
    return Rect(96, 185, 112, 34);
}
void NewOptionsDialog::OpenRunSettings() {
    if (mApp->IsRunMode()) OpenOptionsPage(false, true);
}
void NewOptionsDialog::OpenOptionsPage(bool music, bool settings) {
    if (mRunInfoOpen || mMusicOpen || mRunSettingsOpen) return;
    Sexy::Widget* widgets[] = {mMusicVolumeSlider.get(), mSfxVolumeSlider.get(), mFullscreenCheckbox.get(), mHardwareAccelerationCheckbox.get(), mAlmanacButton.get(), mRestartButton.get(), mBackToMainButton.get(), mBackToGameButton.get(), mHelpButton.get(), mBilibiliButton.get(), mAfdianButton.get()};
    for (int index = 0; index < 11; ++index) {
        mRunInfoWidgetVis[index] = widgets[index]->mVisible;
        widgets[index]->SetVisible(false);
    }
    mOptionsRect = Sexy::Rect(mX, mY, mWidth, mHeight);
    mRunInfoOpen = !music && !settings;
    mMusicOpen = music;
    mRunSettingsOpen = settings;
    mMusicEntryDown = false;
    mRunSettingsEntryDown = false;
    mRunInfoDown = -1;
    mRunInfoView = RunInfoView();
    mRunSettingsView = RunSettingsView();
    Dialog::Resize(20, 20, 760, 560);
    if (mWidgetManager) mWidgetManager->SetFocus(this);
    MarkDirty();
}
void NewOptionsDialog::CloseRunInfo() {
    if (!mRunInfoOpen && !mMusicOpen && !mRunSettingsOpen) return;
    if (mMusicOpen && mApp->mMusic) mApp->mMusic->StopMusicPreview();
    Sexy::Widget* widgets[] = {mMusicVolumeSlider.get(), mSfxVolumeSlider.get(), mFullscreenCheckbox.get(), mHardwareAccelerationCheckbox.get(), mAlmanacButton.get(), mRestartButton.get(), mBackToMainButton.get(), mBackToGameButton.get(), mHelpButton.get(), mBilibiliButton.get(), mAfdianButton.get()};
    for (int index = 0; index < 11; ++index)
        widgets[index]->SetVisible(mRunInfoWidgetVis[index]);
    mRunInfoOpen = false;
    mMusicOpen = false;
    mRunSettingsOpen = false;
    mRunInfoDown = -1;
    Dialog::Resize(mOptionsRect.mX, mOptionsRect.mY, mOptionsRect.mWidth, mOptionsRect.mHeight);
    MarkDirty();
}
