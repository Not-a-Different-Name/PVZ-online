#include "RunPickDialog.h"
#include "GameButton.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../Run/RunState.h"
#include "../Run/RunBuffs.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "graphics/ImageFont.h"

RunPickDialog::RunPickDialog(LawnApp* theApp, RunState* theRun) : LawnDialog(
	theApp,
	Dialogs::DIALOG_RUN_PICK,
	true,
	theRun->IsPlantPick() ? _S("NEW PLANT") : _S("NEW BOOST"),
	_S(""),
	_S(""),
	Dialog::BUTTONS_NONE)
{
	mRun = theRun;
	mPlantPick = theRun->IsPlantPick();
	mAreaTop = 0;
	mAreaHeight = 0;
	mColumnWidth = 0;
	for (int i = 0; i < 3; i++)
	{
		mColumnX[i] = 0;
		// 按钮上就是这株植物 / 这条增益的名字——名字和卡面对得上，玩家才知道自己点的是哪张
		SexyString aLabel = mPlantPick
			? Plant::GetNameString(theRun->mPlantChoices[i])
			: SexyString(GetRunBuffDef(theRun->mBuffChoices[i]).mName);
		mChoiceButtons[i] = MakeButton(RunPickDialog_Choice0 + i, this, aLabel);
	}

	mTallBottom = true;
	mVerticalCenterText = false;
	CalcSize(300, 70);
	mApp->CenterDialog(this, mWidth, mHeight);
	mClip = false;
}

RunPickDialog::~RunPickDialog()
{
	for (int i = 0; i < 3; i++) delete mChoiceButtons[i];
}

void RunPickDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);

	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;
	int aLeft = mContentInsets.mLeft + mBackgroundInsets.mLeft;
	int aContentWidth = mWidth - mContentInsets.mLeft - mContentInsets.mRight - mBackgroundInsets.mLeft - mBackgroundInsets.mRight;
	int aGap = 16;
	mColumnWidth = (aContentWidth - aGap * 2) / 3;
	for (int i = 0; i < 3; i++)
	{
		mColumnX[i] = aLeft + i * (mColumnWidth + aGap);
	}

	// 卡片区 = 说明那行以下、按钮那一行以上。跟着同一个公式算，标题换行也不错位。
	int aAreaTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	if (mDialogHeader.size() > 0) aAreaTop += mHeaderFont->GetHeight() + mSpaceAfterHeader;
	aAreaTop += mLinesFont->GetHeight() + 10;
	mAreaTop = aAreaTop;

	int aButtonY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aButtonY += 5;
	mAreaHeight = aButtonY - mAreaTop - 6;
	if (mAreaHeight < 0) mAreaHeight = 0;

	for (int i = 0; i < 3; i++)
	{
		mChoiceButtons[i]->Resize(mColumnX[i], aButtonY, mColumnWidth, aButtonHeight);
	}
}

void RunPickDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	for (int i = 0; i < 3; i++) AddWidget(mChoiceButtons[i]);
}

void RunPickDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	for (int i = 0; i < 3; i++) RemoveWidget(mChoiceButtons[i]);
}

void RunPickDialog::Draw(Graphics* g)
{
	LawnDialog::Draw(g);

	// 标题下面那行说明。这一屏的三张卡是画上去的（种子包 / 效果说明），不是文本，
	// 所以 Dialog 自己的正文留空，排版在这里现算——和 LawnDialog::Draw 用同一套边距。
	int aFontY = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	if (mDialogHeader.size() > 0)
	{
		int aOffsetY = aFontY - mHeaderFont->GetAscentPadding() + mHeaderFont->GetAscent();
		aFontY = aOffsetY - mHeaderFont->GetAscent() + mHeaderFont->GetHeight() + mSpaceAfterHeader;
	}
	g->SetFont(mLinesFont);
	g->SetColor(mColors[Dialog::COLOR_LINES]);
	WriteCenteredLine(g, aFontY, mPlantPick ? _S("Pick one to join your run.") : _S("Pick one to power up your run."));

	for (int i = 0; i < 3; i++)
	{
		if (mPlantPick)
		{
			DrawSeedPacket(g, (float)(mColumnX[i] + (mColumnWidth - SEED_PACKET_WIDTH) / 2), (float)mAreaTop,
				mRun->mPlantChoices[i], SeedType::SEED_NONE, 0.0f, 255, false, false);
		}
		else
		{
			// 效果说明贴着各自的按钮画：三列各说各的，不用让人去猜哪句话配哪个名字
			Rect aRect(mColumnX[i], mAreaTop, mColumnWidth, mAreaHeight);
			WriteWordWrapped(g, aRect, SexyString(GetRunBuffDef(mRun->mBuffChoices[i]).mDesc),
				mLinesFont->GetLineSpacing() + mLineSpacingOffset, mTextAlign);
		}
	}
}

void RunPickDialog::ButtonDepress(int theId)
{
	if (theId >= RunPickDialog_Choice0 && theId <= RunPickDialog_Choice2)
	{
		mApp->RunPickChosen(theId - RunPickDialog_Choice0);
	}
}

// 键盘一律不认。按空格/回车会走 LawnDialog::KeyDown 那条"当成点了 Yes"的老路，
// 而这一屏没有 Yes——屏被关掉的话待选的卡还在，下一帧它照样会弹回来，
// 玩家只会觉得"按了没反应"。要么点一张卡，要么键盘在这儿什么也别做。
void RunPickDialog::KeyDown(KeyCode theKey)
{
	(void)theKey;
}
