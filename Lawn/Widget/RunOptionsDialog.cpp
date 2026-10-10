#include "RunOptionsDialog.h"
#include "CjkStoneButton.h"
#include "GameButton.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "../Run/RunState.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "graphics/Font.h"
#include "graphics/Graphics.h"
#include "widget/WidgetManager.h"
#include "../ModText.h"

// 行内按钮间距 / 底部两枚的间距（RunModeDialog 的难度行与底行各用一套，这里照抄）。
static const int kGapX = 12;
static const int kBottomGapX = 20;

// 小标题居中直绘（RunModeDialog 的难度行同款颜色）。
static void DrawRowCaption(Graphics* g, ModText::Font* theFont, const std::string& theText, int theY, int theWidth)
{
	std::wstring aWide = ModText::WideFromUtf8(theText.c_str());
	ModText::DrawTextWide(g, theFont, (theWidth - ModText::TextWidth(theFont, aWide)) / 2, theY,
		aWide, Color(96, 72, 40), g->mClipRect);
}

RunOptionsDialog::RunOptionsDialog(LawnApp* theApp, int theScaleSel, int theTempoSel, int theZombotanySel, int theSlideSel, int theBossSel)
	: LawnDialog(theApp, Dialogs::DIALOG_RUN_OPTIONS, true, _S(""), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
	mTitle = ModText::Tr("高级选项", "Advanced Options");

	// 传入的当前值先钳一遍（与 AlignRunToHost 同纪律）：非法档按 标准/标准/关。
	mScaleSel = theScaleSel;
	mTempoSel = theTempoSel;
	mZombotanySel = theZombotanySel;
	if (mScaleSel < RunState::RUN_SCALE_HALF || mScaleSel > RunState::RUN_SCALE_QUAD) mScaleSel = RunState::RUN_SCALE_STD;
	if (mTempoSel < RunState::RUN_TEMPO_FAST || mTempoSel > RunState::RUN_TEMPO_SLOW) mTempoSel = RunState::RUN_TEMPO_STD;
	if (mZombotanySel != 0 && mZombotanySel != 1) mZombotanySel = 0;
	mSlideSel = theSlideSel;
	if (mSlideSel != 0 && mSlideSel != 1) mSlideSel = 1;	// 非法档归开（同"默认开"口径）
	mBossSel = theBossSel;
	if (mBossSel != 0 && mBossSel != 1) mBossSel = 0;

	mScaleCaption = ModText::Tr("出怪规模（全队倍率）", "Zombie scale (team multiplier)");
	mScaleLabels[0] = ModText::Tr("少 ×0.5", "Few ×0.5");
	mScaleLabels[1] = ModText::Tr("标准 ×1", "Standard ×1");
	mScaleLabels[2] = ModText::Tr("多 ×2", "Many ×2");
	mScaleLabels[3] = ModText::Tr("海量 ×4", "Swarm ×4");

	mTempoCaption = ModText::Tr("出怪节奏（波间隔）", "Spawn tempo (wave interval)");
	mTempoLabels[0] = ModText::Tr("快 ×0.6", "Fast ×0.6");
	mTempoLabels[1] = ModText::Tr("标准 ×1", "Standard ×1");
	mTempoLabels[2] = ModText::Tr("慢 ×1.6", "Slow ×1.6");

	mZombotanyCaption = ModText::Tr("植物僵尸（实验玩法）", "ZomBotany (experimental)");
	mZombotanyLabels[0] = ModText::Tr("关闭", "Off");
	mZombotanyLabels[1] = ModText::Tr("开启", "On");

	mSlideCaption = ModText::Tr("滑动收阳光（仅本机，全局生效）", "Slide-collect sun (local, all modes)");
	mSlideLabels[0] = ModText::Tr("关闭", "Off");
	mSlideLabels[1] = ModText::Tr("开启", "On");

	mBossCaption = ModText::Tr("巨型 Boss（闯关关底）", "Giant Boss (run finale)");
	mBossLabels[0] = ModText::Tr("关闭", "Off");
	mBossLabels[1] = ModText::Tr("开启", "On");

	mTitleY = mScaleCaptionY = mTempoCaptionY = mZombotanyCaptionY = 0;

	// 各行按钮等宽（取最长标签量的）；石门贴图平铺对宽度有整段要求（见 CjkStoneButtonWidth）。
	// 量宽用按钮实际画标签的那档字号（CjkPointSize(CJK_BUTTON_LABEL_PX)），量画同一份。
	ModText::Font* aLabelFont = ModText::GetFont(CjkPointSize(CJK_BUTTON_LABEL_PX), false);
	static const int kRowCounts[5] = { 4, 3, 2, 2, 2 };
	const std::string* aLabelSets[5] = { mScaleLabels, mTempoLabels, mZombotanyLabels, mSlideLabels, mBossLabels };
	int* aWidthSets[5] = { mScaleWidths, mTempoWidths, mZombotanyWidths, mSlideWidths, mBossWidths };
	for (int aRow = 0; aRow < 5; aRow++)
	{
		int aLabelMax = 0;
		for (int i = 0; i < kRowCounts[aRow]; i++)
		{
			int aWidth = ModText::TextWidth(aLabelFont, ModText::WideFromUtf8(aLabelSets[aRow][i].c_str()));
			if (aWidth > aLabelMax) aLabelMax = aWidth;
		}
		int aButtonWidth = CjkStoneButtonWidth(aLabelMax + 26, true);
		for (int i = 0; i < kRowCounts[aRow]; i++) aWidthSets[aRow][i] = aButtonWidth;
	}

	for (int i = 0; i < 4; i++)
	{
		CjkStoneButton* aButton = new CjkStoneButton(RunOptionsDialog_Scale0 + i, this);
		aButton->SetLabel(mScaleLabels[i]);
		aButton->mHasAlpha = true;
		aButton->mHasTransparencies = true;
		aButton->mInverted = (i == mScaleSel);
		mScaleButtons[i] = aButton;
	}
	for (int i = 0; i < 3; i++)
	{
		CjkStoneButton* aButton = new CjkStoneButton(RunOptionsDialog_Tempo0 + i, this);
		aButton->SetLabel(mTempoLabels[i]);
		aButton->mHasAlpha = true;
		aButton->mHasTransparencies = true;
		aButton->mInverted = (i == mTempoSel);
		mTempoButtons[i] = aButton;
	}
	for (int i = 0; i < 2; i++)
	{
		CjkStoneButton* aButton = new CjkStoneButton(RunOptionsDialog_Zombotany0 + i, this);
		aButton->SetLabel(mZombotanyLabels[i]);
		aButton->mHasAlpha = true;
		aButton->mHasTransparencies = true;
		aButton->mInverted = (i == mZombotanySel);
		mZombotanyButtons[i] = aButton;
	}
	for (int i = 0; i < 2; i++)
	{
		CjkStoneButton* aButton = new CjkStoneButton(RunOptionsDialog_Slide0 + i, this);
		aButton->SetLabel(mSlideLabels[i]);
		aButton->mHasAlpha = true;
		aButton->mHasTransparencies = true;
		aButton->mInverted = (i == mSlideSel);
		mSlideButtons[i] = aButton;
	}
	for (int i = 0; i < 2; i++)
	{
		CjkStoneButton* aButton = new CjkStoneButton(RunOptionsDialog_Boss0 + i, this);
		aButton->SetLabel(mBossLabels[i]);
		aButton->mHasAlpha = true;
		aButton->mHasTransparencies = true;
		aButton->mInverted = (i == mBossSel);
		mBossButtons[i] = aButton;
	}

	mOkButton = new CjkStoneButton(RunOptionsDialog_OK, this);
	mOkButton->SetLabel(ModText::Tr("确定", "OK"));
	mOkButton->mHasAlpha = true;
	mOkButton->mHasTransparencies = true;
	mOkWidth = CjkStoneButtonWidth(ModText::TextWidth(ModText::GetFont(CjkPointSize(CJK_BUTTON_LABEL_PX), false),
		ModText::WideFromUtf8(mOkButton->mLabel.c_str())) + 32, true);

	mCancelButton = new CjkStoneButton(Dialog::ID_NO, this);
	mCancelButton->SetLabel(ModText::Tr("取消", "Cancel"));
	mCancelButton->mHasAlpha = true;
	mCancelButton->mHasTransparencies = true;
	mCancelWidth = CjkStoneButtonWidth(ModText::TextWidth(ModText::GetFont(CjkPointSize(CJK_BUTTON_LABEL_PX), false),
		ModText::WideFromUtf8(mCancelButton->mLabel.c_str())) + 32, true);

	mTallBottom = true;
	mVerticalCenterText = false;

	// 版心：宽 = 标题与五行按钮总宽的最大者；高 = 标题 + 间隔 + 五行（小标题 + 按钮）
	// + 确定行。CalcSize 会按对话框贴图再取整/加高，多出来的空隙由 Resize 里
	// "首行贴顶、确定行贴底"吸收。
	ModText::Font* aTitleFont = ModText::GetFont(16, true);
	ModText::Font* aCaptionFont = ModText::GetFont(12, false);
	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;
	int anExtraX = ModText::TextWidth(aTitleFont, ModText::WideFromUtf8(mTitle.c_str())) + 80;
	int aRowWidths[5] = {
		mScaleWidths[0] * 4 + kGapX * 3,
		mTempoWidths[0] * 3 + kGapX * 2,
		mZombotanyWidths[0] * 2 + kGapX,
		mSlideWidths[0] * 2 + kGapX,
		mBossWidths[0] * 2 + kGapX,
	};
	for (int i = 0; i < 5; i++) if (aRowWidths[i] > anExtraX) anExtraX = aRowWidths[i];
	int aRowHeight = ModText::LineHeight(aCaptionFont) + 6 + aButtonHeight + 8;
	int anExtraY = ModText::LineHeight(aTitleFont) + 18		// 标题 + 与首行的间隔
		+ aRowHeight * 5
		+ aButtonHeight + 18;								// 确定行 + 与末行的间隔

	CalcSize(anExtraX, anExtraY);
	mApp->CenterDialog(this, mWidth, mHeight);
	mClip = false;
}

RunOptionsDialog::~RunOptionsDialog()
{
	for (int i = 0; i < 4; i++) delete mScaleButtons[i];
	for (int i = 0; i < 3; i++) delete mTempoButtons[i];
	for (int i = 0; i < 2; i++) delete mZombotanyButtons[i];
	for (int i = 0; i < 2; i++) delete mSlideButtons[i];
	for (int i = 0; i < 2; i++) delete mBossButtons[i];
	delete mOkButton;
	delete mCancelButton;
}

void RunOptionsDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);

	ModText::Font* aTitleFont = ModText::GetFont(16, true);
	ModText::Font* aCaptionFont = ModText::GetFont(12, false);
	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;

	int aTitleTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	mTitleY = aTitleTop;						// ModText 顶对齐：存的直接是顶

	// 四行从上往下排：小标题贴行顶、按钮横排居中（各自等宽）
	int aRowTop = aTitleTop + ModText::LineHeight(aTitleFont) + 18;
	int aButtonY = aRowTop + ModText::LineHeight(aCaptionFont) + 6;
	int aX = (mWidth - (mScaleWidths[0] * 4 + kGapX * 3)) / 2;
	mScaleCaptionY = aRowTop;
	for (int i = 0; i < 4; i++)
	{
		mScaleButtons[i]->Resize(aX, aButtonY, mScaleWidths[i], aButtonHeight);
		aX += mScaleWidths[i] + kGapX;
	}
	aRowTop += ModText::LineHeight(aCaptionFont) + 6 + aButtonHeight + 8;

	aButtonY = aRowTop + ModText::LineHeight(aCaptionFont) + 6;
	aX = (mWidth - (mTempoWidths[0] * 3 + kGapX * 2)) / 2;
	mTempoCaptionY = aRowTop;
	for (int i = 0; i < 3; i++)
	{
		mTempoButtons[i]->Resize(aX, aButtonY, mTempoWidths[i], aButtonHeight);
		aX += mTempoWidths[i] + kGapX;
	}
	aRowTop += ModText::LineHeight(aCaptionFont) + 6 + aButtonHeight + 8;

	aButtonY = aRowTop + ModText::LineHeight(aCaptionFont) + 6;
	aX = (mWidth - (mZombotanyWidths[0] * 2 + kGapX)) / 2;
	mZombotanyCaptionY = aRowTop;
	for (int i = 0; i < 2; i++)
	{
		mZombotanyButtons[i]->Resize(aX, aButtonY, mZombotanyWidths[i], aButtonHeight);
		aX += mZombotanyWidths[i] + kGapX;
	}
	aRowTop += ModText::LineHeight(aCaptionFont) + 6 + aButtonHeight + 8;

	aButtonY = aRowTop + ModText::LineHeight(aCaptionFont) + 6;
	aX = (mWidth - (mSlideWidths[0] * 2 + kGapX)) / 2;
	mSlideCaptionY = aRowTop;
	for (int i = 0; i < 2; i++)
	{
		mSlideButtons[i]->Resize(aX, aButtonY, mSlideWidths[i], aButtonHeight);
		aX += mSlideWidths[i] + kGapX;
	}
	aRowTop += ModText::LineHeight(aCaptionFont) + 6 + aButtonHeight + 8;

	aButtonY = aRowTop + ModText::LineHeight(aCaptionFont) + 6;
	aX = (mWidth - (mBossWidths[0] * 2 + kGapX)) / 2;
	mBossCaptionY = aRowTop;
	for (int i = 0; i < 2; i++)
	{
		mBossButtons[i]->Resize(aX, aButtonY, mBossWidths[i], aButtonHeight);
		aX += mBossWidths[i] + kGapX;
	}

	// 确定行贴底，两枚并排居中（RunModeDialog 底行同一条算式）
	int aBottomY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aBottomY += 5;
	int aTotalW = mOkWidth + mCancelWidth + kBottomGapX;
	int aLeft = (mWidth - aTotalW) / 2;
	mOkButton->Resize(aLeft, aBottomY, mOkWidth, aButtonHeight);
	mCancelButton->Resize(aLeft + mOkWidth + kBottomGapX, aBottomY, mCancelWidth, aButtonHeight);
}

void RunOptionsDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	for (int i = 0; i < 4; i++) AddWidget(mScaleButtons[i]);
	for (int i = 0; i < 3; i++) AddWidget(mTempoButtons[i]);
	for (int i = 0; i < 2; i++) AddWidget(mZombotanyButtons[i]);
	for (int i = 0; i < 2; i++) AddWidget(mSlideButtons[i]);
	for (int i = 0; i < 2; i++) AddWidget(mBossButtons[i]);
	AddWidget(mOkButton);
	AddWidget(mCancelButton);
}

void RunOptionsDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	for (int i = 0; i < 4; i++) RemoveWidget(mScaleButtons[i]);
	for (int i = 0; i < 3; i++) RemoveWidget(mTempoButtons[i]);
	for (int i = 0; i < 2; i++) RemoveWidget(mZombotanyButtons[i]);
	for (int i = 0; i < 2; i++) RemoveWidget(mSlideButtons[i]);
	for (int i = 0; i < 2; i++) RemoveWidget(mBossButtons[i]);
	RemoveWidget(mOkButton);
	RemoveWidget(mCancelButton);
}

void RunOptionsDialog::Draw(Graphics* g)
{
	LawnDialog::Draw(g);   // 底板。header / lines 都是空的，只出框。

	if (!mTitle.empty())
	{
		ModText::Font* aTitleFont = ModText::GetFont(16, true);
		std::wstring aTitle = ModText::WideFromUtf8(mTitle.c_str());
		ModText::DrawTextWide(g, aTitleFont,
			(mWidth - ModText::TextWidth(aTitleFont, aTitle)) / 2, mTitleY,
			aTitle, mColors[Dialog::COLOR_HEADER], g->mClipRect);
	}

	// 五行小标题（各行的按钮自己画自己，走控件那套）。
	ModText::Font* aCaptionFont = ModText::GetFont(12, false);
	DrawRowCaption(g, aCaptionFont, mScaleCaption, mScaleCaptionY, mWidth);
	DrawRowCaption(g, aCaptionFont, mTempoCaption, mTempoCaptionY, mWidth);
	DrawRowCaption(g, aCaptionFont, mZombotanyCaption, mZombotanyCaptionY, mWidth);
	DrawRowCaption(g, aCaptionFont, mSlideCaption, mSlideCaptionY, mWidth);
	DrawRowCaption(g, aCaptionFont, mBossCaption, mBossCaptionY, mWidth);
}

// 键盘一律不认（同 OnlineStartDialog / RunModeDialog 的纪律）。
void RunOptionsDialog::KeyDown(KeyCode theKey)
{
	(void)theKey;
}

// 0x4572E0 的同一支音效（LawnDialog::ButtonPress），听着还是原版的石头按钮
void RunOptionsDialog::ButtonPress(int theId)
{
	(void)theId;
	mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
}

void RunOptionsDialog::ButtonDepress(int theId)
{
	// 不调 Dialog::ButtonDepress（RunModeDialog 同款纪律）：结果由打开方在 WaitForResult
	// 里收，不走 2000+/3000+ 的标准对话框路由。
	if (theId >= RunOptionsDialog_Scale0 && theId <= RunOptionsDialog_Scale0 + 3)
	{
		mScaleSel = theId - RunOptionsDialog_Scale0;
		for (int i = 0; i < 4; i++) mScaleButtons[i]->mInverted = (i == mScaleSel);
		return;
	}
	if (theId >= RunOptionsDialog_Tempo0 && theId <= RunOptionsDialog_Tempo0 + 2)
	{
		mTempoSel = theId - RunOptionsDialog_Tempo0;
		for (int i = 0; i < 3; i++) mTempoButtons[i]->mInverted = (i == mTempoSel);
		return;
	}
	if (theId >= RunOptionsDialog_Zombotany0 && theId <= RunOptionsDialog_Zombotany0 + 1)
	{
		mZombotanySel = theId - RunOptionsDialog_Zombotany0;
		for (int i = 0; i < 2; i++) mZombotanyButtons[i]->mInverted = (i == mZombotanySel);
		return;
	}
	if (theId >= RunOptionsDialog_Slide0 && theId <= RunOptionsDialog_Slide0 + 1)
	{
		mSlideSel = theId - RunOptionsDialog_Slide0;
		for (int i = 0; i < 2; i++) mSlideButtons[i]->mInverted = (i == mSlideSel);
		return;
	}
	if (theId >= RunOptionsDialog_Boss0 && theId <= RunOptionsDialog_Boss0 + 1)
	{
		mBossSel = theId - RunOptionsDialog_Boss0;
		for (int i = 0; i < 2; i++) mBossButtons[i]->mInverted = (i == mBossSel);
		return;
	}
	if (theId == RunOptionsDialog_OK)
	{
		mResult = RunOptionsDialog_OK;	// 确定 = 收结果（WaitForResult 自己收摊）
		return;
	}
	if (theId == Dialog::ID_NO)
	{
		mResult = Dialog::ID_NO;		// 取消 = 关弹窗不改任何值
		return;
	}
}
