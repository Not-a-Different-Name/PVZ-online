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
#include "../Zombie.h"
#include "GameButton.h"
#include "../../LawnApp.h"
#include "ContinueDialog.h"
#include "../../Resources.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "graphics/Font.h"
#include "../ModText.h"
#include <algorithm>

ContinueDialog::ContinueDialog(LawnApp* theApp) : LawnDialog(
	theApp,
	Dialogs::DIALOG_CONTINUE,
	true,
	theApp->GetString("CONTINUE_GAME_HEADER", "CONTINUE GAME?"),
	"",
	"[DIALOG_BUTTON_CANCEL]",
	Dialog::BUTTONS_FOOTER)
{
	if (theApp->IsAdventureMode())
	{
		mDialogLines = ModText::Tr("可以继续；重新开始会放弃本关进度。", "Continue, or restart and discard this level's progress.");
		mContinueButton = MakeButton(ContinueDialog::ContinueDialog_Continue, this, "[CONTINUE_BUTTON]");
		mNewGameButton = MakeButton(ContinueDialog::ContinueDialog_NewGame, this, "[RESTART_BUTTON]");
	}
	else
	{
		mDialogLines = ModText::Tr("可以继续；新游戏会替换当前进度。", "Continue, or start a new game and replace this progress.");
		mContinueButton = MakeButton(ContinueDialog::ContinueDialog_Continue, this, "[CONTINUE_BUTTON]");
		mNewGameButton = MakeButton(ContinueDialog::ContinueDialog_NewGame, this, "[NEW_GAME_BUTTON]");
	}

	mTallBottom = true;
	CalcSize(10, 60);
}

ContinueDialog::~ContinueDialog() = default;

int ContinueDialog::GetPreferredHeight(int theWidth)
{
	return LawnDialog::GetPreferredHeight(theWidth) + 40;
}

void ContinueDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	int aBtnWidth = IMAGE_BUTTON_LEFT->mWidth + IMAGE_BUTTON_MIDDLE->mWidth * 3 + IMAGE_BUTTON_RIGHT->mWidth;
	const int aLabelWidth = std::max(FONT_DWARVENTODCRAFT18GREENINSET->StringWidth(mContinueButton->mLabel),
		FONT_DWARVENTODCRAFT18GREENINSET->StringWidth(mNewGameButton->mLabel)) + 24;
	while (aBtnWidth < aLabelWidth) aBtnWidth += IMAGE_BUTTON_MIDDLE->mWidth;
	const int aButtonGap = 12;
	const int aRowWidth = aBtnWidth * 2 + aButtonGap;
	const int aMinWidth = aRowWidth + mContentInsets.mLeft + mContentInsets.mRight +
		mBackgroundInsets.mLeft + mBackgroundInsets.mRight;
	if (theWidth < aMinWidth)
	{
		const int aFrameWidth = IMAGE_DIALOG_TOPLEFT->mWidth + IMAGE_DIALOG_TOPRIGHT->mWidth;
		const int aTileWidth = IMAGE_DIALOG_TOPMIDDLE->mWidth;
		theWidth = aFrameWidth + (aMinWidth - aFrameWidth + aTileWidth - 1) / aTileWidth * aTileWidth;
	}
	LawnDialog::Resize(theX, theY, theWidth, theHeight);
	const int aBtnHeight = mLawnYesButton->mHeight;
	const int aRowX = (mWidth - aRowWidth) / 2;
	const int aRowY = mLawnYesButton->mY - aBtnHeight - 8;
	mContinueButton->Resize(aRowX, aRowY, aBtnWidth, aBtnHeight);
	mNewGameButton->Resize(aRowX + aBtnWidth + aButtonGap, aRowY, aBtnWidth, aBtnHeight);
}

void ContinueDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	AddWidget(mContinueButton.get());
	AddWidget(mNewGameButton.get());
}

void ContinueDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	RemoveWidget(mContinueButton.get());
	RemoveWidget(mNewGameButton.get());
}

void ContinueDialog::KeyDown(KeyCode theKey)
{
	if (theKey == KeyCode::KEYCODE_ESCAPE)
	{
		ButtonDepress(Dialog::ID_FOOTER);
		return;
	}

	if (theKey == KeyCode::KEYCODE_RETURN || theKey == KeyCode::KEYCODE_SPACE)
	{
		ButtonDepress(ContinueDialog::ContinueDialog_Continue);
		return;
	}

	LawnDialog::KeyDown(theKey);
}

void ContinueDialog::RestartLoopingSounds()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS || mApp->IsStormyNightLevel())
	{
		mApp->PlayFoley(FoleyType::FOLEY_RAIN);
	}

	for (Zombie* aZombie : mApp->mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mPlayingSong)
		{
			aZombie->StartZombieSound();
		}
	}
}

void ContinueDialog::ButtonDepress(int theId)
{
	if (theId == ContinueDialog::ContinueDialog_Continue)
	{
		RestartLoopingSounds();
		mApp->KillDialog(mId);
	}
	else if (theId == ContinueDialog::ContinueDialog_NewGame)
	{
		if (mApp->IsAdventureMode())
		{
			LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(
				Dialogs::DIALOG_RESTARTCONFIRM,
				true,
				"[RESTART_LEVEL_HEADER]",
				"[RESTART_LEVEL_BODY]",
				"",
				Dialog::BUTTONS_OK_CANCEL
			);
			aDialog->mLawnYesButton->mLabel = PvzpStringTranslate("[RESTART_BUTTON]");
			//aDialog->CalcSize(0, 0);
		}
		else
		{
			LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(
				Dialogs::DIALOG_RESTARTCONFIRM,
				true,
				mApp->GetString("NEW_GAME_HEADER", "New Game?"),
				mApp->GetString("NEW_GAME", "Are you sure that you want to start a new game?"),
				"",
				Dialog::BUTTONS_OK_CANCEL
			);
			aDialog->mLawnYesButton->mLabel = PvzpStringTranslate("[NEW_GAME_BUTTON]");
			//aDialog->CalcSize(0, 0);
		}
	}
	else
	{
		mApp->KillDialog(mId);
		mApp->mBoardResult = BoardResult::BOARDRESULT_QUIT;
		if (mApp->IsAdventureMode())
		{
			mApp->ShowGameSelector();
		}
		else if (mApp->IsSurvivalMode())
		{
			mApp->KillBoard();
			mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
		}
		else if (mApp->IsPuzzleMode())
		{
			mApp->KillBoard();
			mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_PUZZLE);
		}
		else
		{
			mApp->KillBoard();
			mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
		}
	}
}
