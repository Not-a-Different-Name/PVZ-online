#include "RunModeDialog.h"
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

// 卡片尺寸与摆距照抄 ChallengeScreen（卡片 104×115，横向间距 155 → 三张一行 414 宽）
static const int CARD_W = 104;
static const int CARD_H = 115;
static const int CARD_PITCH_X = 155;

// 三张卡的缩略图标（IMAGE_CHALLENGE_THUMBNAILS 的 cel 号，实机看效果可换）：
// 完整版 = 决胜魔音（终局感）、普通版 = 火爆辣椒帽子（中间档）、快速版 = 僵尸快跑（快）
static const int kCardIcons[3] = { 19, 16, 18 };

// 中文/英文字面量（UTF-8）的绘制自 2026-10-03 语言批起改走 ModText 的宽字符直绘
// （UTF-8 → UTF-16 → TextOutW，与系统码页脱钩）；原先这里各有一份 Utf8ToAnsi +
// SysFont(TextOutA) 助手，已并入 Lawn/ModText。文案中英各一份、构造时按
// ModText::Tr 择一存进成员；字符串成员一律存 UTF-8 原样，绘制/量宽时现转宽字符。

// 石材按钮是"左端贴图 + 中段贴图 × n + 右端贴图"平铺画的（见 CjkStoneButton::Draw），
// 宽度必须正好是这三段的和；照抄 OnlineStartDialog 的同一支（含"至少带一个中段"的下限）。
static int StoneButtonWidth(int theWidth, bool theRoundUp)
{
	int aMid = Sexy::IMAGE_BUTTON_MIDDLE->mWidth;
	int aMin = Sexy::IMAGE_BUTTON_LEFT->mWidth + Sexy::IMAGE_BUTTON_RIGHT->mWidth;
	int anExtra = theWidth - aMin;
	if (anExtra < 0)
	{
		anExtra = 0;
	}
	else if (aMid > 0)
	{
		int aRemainder = anExtra % aMid;
		if (aRemainder != 0)
		{
			if (theRoundUp) anExtra += aMid - aRemainder;
			else anExtra -= aRemainder;
		}
	}
	int aWidth = aMin + anExtra;
	int aFloor = aMin + aMid;   // 至少带一个中段：光两块端头拼不成石头
	return aWidth < aFloor ? aFloor : aWidth;
}

// 石材按钮 + 中文标签：照抄 OnlineStartDialog 的内联类（贴图平铺、按下位移、居中）。
class CjkStoneButton : public LawnStoneButton
{
public:
	CjkStoneButton(int theId, ButtonListener* theListener) : LawnStoneButton(nullptr, theId, theListener) { }

	virtual void Draw(Graphics* g)
	{
		if (mBtnNoDraw) return;

		bool aDown = (mIsDown && mIsOver && !mDisabled) ^ mInverted;
		Image* aLeftImage = aDown ? Sexy::IMAGE_BUTTON_DOWN_LEFT : Sexy::IMAGE_BUTTON_LEFT;
		Image* aMiddleImage = aDown ? Sexy::IMAGE_BUTTON_DOWN_MIDDLE : Sexy::IMAGE_BUTTON_MIDDLE;
		Image* aRightImage = aDown ? Sexy::IMAGE_BUTTON_DOWN_RIGHT : Sexy::IMAGE_BUTTON_RIGHT;

		int aFontX = 0;
		int aFontY = 0;
		int aImageX = 0;
		if (aDown)
		{
			aFontX++;
			aFontY++;
			aImageX++;
		}

		int aRepeat = (mWidth - aLeftImage->mWidth - aRightImage->mWidth) / aMiddleImage->mWidth;
		g->DrawImage(aLeftImage, aImageX, 0);
		aImageX += aLeftImage->mWidth;
		while (aRepeat > 0)
		{
			g->DrawImage(aMiddleImage, aImageX, 0);
			aImageX += aMiddleImage->mWidth;
			--aRepeat;
		}
		g->DrawImage(aRightImage, aImageX, 0);

		// 标签是 UTF-8 原样（SetLabel 收的也是原样字节），绘制现转宽字符（顶对齐）
		ModText::Font* aFont = ModText::GetFont(14, false);
		std::wstring aLabel = ModText::WideFromUtf8(mLabel.c_str());
		aFontX += (mWidth - ModText::TextWidth(aFont, aLabel)) / 2;
		aFontY += (mHeight - ModText::LineHeight(aFont)) / 2;
		ModText::DrawTextWide(g, aFont, aFontX, aFontY, aLabel,
			mIsOver ? Color(0x9B, 0xF0, 0x60) : Color(0x2F, 0x6B, 0x2B), g->mClipRect);
	}
};

RunModeDialog::RunModeDialog(LawnApp* theApp, bool theShowDiff) : LawnDialog(
	theApp, Dialogs::DIALOG_ONLINE_START, true, _S(""), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
	// 卡片边框和缩略图标在 ChallengeScreen 的延迟资源组里，不加载就是空指针
	TodLoadResources("DelayLoad_ChallengeScreen");

	mTitle = ModText::Tr("选择闯关模式", "Choose a Run Mode");
	mCardNames[0] = ModText::Tr("完整版", "Full");
	mCardNames[1] = ModText::Tr("普通版", "Normal");
	mCardNames[2] = ModText::Tr("快速版", "Quick");
	mCardDescs[0] = ModText::Tr("25 关 · 标准奖励", "25 levels · normal rewards");
	mCardDescs[1] = ModText::Tr("15 关 · 奖励×2", "15 levels · rewards ×2");
	mCardDescs[2] = ModText::Tr("10 关 · 奖励×3", "10 levels · rewards ×3");
	mTitleY = 0;

	for (int i = 0; i < 3; i++)
	{
		mCardButtons[i] = new ButtonWidget(RunModeDialog_Mode0 + i, this);
		mCardButtons[i]->mDoFinger = true;
		mCardButtons[i]->mFrameNoDraw = true;	// 画在 Dialog::Draw 里（照 ChallengeScreen）
	}

	// 出怪难度行（MOD_BUILD 27）：联机主机才摆（单机档位恒为标准，摆了也是死控件）。
	// 选中 = mInverted（CjkStoneButton::Draw 里 XOR 成按下态贴图）；点了只换选中、不关弹窗。
	mShowDiff = theShowDiff;
	mDiffSel = RunState::RUN_DIFF_STD;
	mDiffCaptionY = 0;
	for (int i = 0; i < 3; i++) mDiffButtons[i] = nullptr;
	if (mShowDiff)
	{
		mDiffCaption = ModText::Tr("出怪难度（全队倍率）", "Zombie difficulty (team multiplier)");
		mDiffLabels[0] = ModText::Tr("轻松 ×0.5", "Easy ×0.5");
		mDiffLabels[1] = ModText::Tr("标准 ×1", "Standard ×1");
		mDiffLabels[2] = ModText::Tr("高压 ×1.5", "High ×1.5");

		// 三枚等宽（取最长标签量的），石门贴图平铺对宽度有整段要求（见 StoneButtonWidth）
		ModText::Font* aDiffFont = ModText::GetFont(14, false);
		int aLabelMax = 0;
		for (int i = 0; i < 3; i++)
		{
			int aWidth = ModText::TextWidth(aDiffFont, ModText::WideFromUtf8(mDiffLabels[i].c_str()));
			if (aWidth > aLabelMax) aLabelMax = aWidth;
		}
		int aDiffWidth = StoneButtonWidth(aLabelMax + 26, true);
		for (int i = 0; i < 3; i++)
		{
			mDiffWidths[i] = aDiffWidth;
			CjkStoneButton* aButton = new CjkStoneButton(RunModeDialog_Diff0 + i, this);
			aButton->SetLabel(mDiffLabels[i]);
			aButton->mHasAlpha = true;
			aButton->mHasTransparencies = true;
			aButton->mInverted = (i == mDiffSel);	// 默认选中"标准"
			mDiffButtons[i] = aButton;
		}
	}

	mCancelButton = new CjkStoneButton(Dialog::ID_NO, this);
	mCancelButton->SetLabel(ModText::Tr("取消", "Cancel"));
	mCancelButton->mHasAlpha = true;
	mCancelButton->mHasTransparencies = true;
	mCancelWidth = StoneButtonWidth(ModText::TextWidth(ModText::GetFont(14, false),
		ModText::WideFromUtf8(mCancelButton->mLabel.c_str())) + 32, true);

	mTallBottom = true;
	mVerticalCenterText = false;

	// 版心：宽 = 三张一行的宽度（边框两侧各探出几像素，留 16 兜住）；高 = 标题 + 间隔
	// + 卡片（连边框）+ （难度行）+ 按钮。CalcSize 会按对话框贴图再取整/加高，多出来的
	// 空隙由 Resize 里"卡片贴顶、按钮贴底"吸收。
	ModText::Font* aTitleFont = ModText::GetFont(16, true);
	int anExtraX = ModText::TextWidth(aTitleFont, ModText::WideFromUtf8(mTitle.c_str())) + 80;
	int aRowWidth = CARD_PITCH_X * 2 + CARD_W + 16;
	if (aRowWidth > anExtraX) anExtraX = aRowWidth;
	int anExtraY = ModText::LineHeight(aTitleFont) + 18	// 标题 + 与卡片的间隔
		+ CARD_H + 14								// 卡片 + 边框上下探出
		+ IMAGE_BUTTON_LEFT->mHeight + 18;			// 按钮行 + 与卡片的间隔
	if (mShowDiff)
	{
		// 难度行：卡下说明（12 细，基线在卡底 +19）之下再塞一行——小标题 + 间隔 + 按钮
		anExtraY += ModText::LineHeight(ModText::GetFont(12, false)) + 6 + IMAGE_BUTTON_LEFT->mHeight + 8;
	}

	CalcSize(anExtraX, anExtraY);
	mApp->CenterDialog(this, mWidth, mHeight);
	mClip = false;
}

RunModeDialog::~RunModeDialog()
{
	for (int i = 0; i < 3; i++) delete mCardButtons[i];
	for (int i = 0; i < 3; i++) delete mDiffButtons[i];
	delete mCancelButton;
}

void RunModeDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);

	ModText::Font* aTitleFont = ModText::GetFont(16, true);

	// 标题贴顶；三张卡片横排在标题下面（一行整体在版心里左右居中）
	int aTitleTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	mTitleY = aTitleTop;						// ModText 顶对齐：存的直接是顶
	int aCardsY = aTitleTop + ModText::LineHeight(aTitleFont) + 18;
	int aRowWidth = CARD_PITCH_X * 2 + CARD_W;
	int aStartX = (mWidth - aRowWidth) / 2;
	for (int i = 0; i < 3; i++)
	{
		mCardButtons[i]->Resize(aStartX + i * CARD_PITCH_X, aCardsY, CARD_W, CARD_H);
	}

	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;

	// 出怪难度行：卡下说明（12 细，基线在卡底 +19）之下、取消之上，三枚等宽横排居中
	if (mShowDiff)
	{
		ModText::Font* aCaptionFont = ModText::GetFont(12, false);
		int aCaptionTop = aCardsY + CARD_H + 34;
		mDiffCaptionY = aCaptionTop;			// 同上：顶对齐
		int aDiffY = aCaptionTop + ModText::LineHeight(aCaptionFont) + 6;
		static const int aGapX = 20;
		int aTotalW = mDiffWidths[0] + mDiffWidths[1] + mDiffWidths[2] + aGapX * 2;
		int aDiffX = (mWidth - aTotalW) / 2;
		for (int i = 0; i < 3; i++)
		{
			mDiffButtons[i]->Resize(aDiffX, aDiffY, mDiffWidths[i], aButtonHeight);
			aDiffX += mDiffWidths[i] + aGapX;
		}
	}

	// 取消按钮贴底（ OnlineStartDialog 同一条算式）
	int aButtonY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aButtonY += 5;
	mCancelButton->Resize((mWidth - mCancelWidth) / 2, aButtonY, mCancelWidth, aButtonHeight);
}

void RunModeDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	for (int i = 0; i < 3; i++) AddWidget(mCardButtons[i]);
	for (int i = 0; i < 3; i++) if (mDiffButtons[i]) AddWidget(mDiffButtons[i]);
	AddWidget(mCancelButton);
}

void RunModeDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	for (int i = 0; i < 3; i++) RemoveWidget(mCardButtons[i]);
	for (int i = 0; i < 3; i++) if (mDiffButtons[i]) RemoveWidget(mDiffButtons[i]);
	RemoveWidget(mCancelButton);
}

void RunModeDialog::Draw(Graphics* g)
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

	// 三张卡片，画法照 ChallengeScreen::DrawButton：按下 +1/+1 位移，缩略图标，
	// 边框（悬停换高亮），卡上名字 14 粗、卡下说明 12 细（GDI 中文）。
	for (int i = 0; i < 3; i++)
	{
		ButtonWidget* aButton = mCardButtons[i];
		int aPosX = aButton->mX;
		int aPosY = aButton->mY;
		if (aButton->mIsDown)
		{
			aPosX++;
			aPosY++;
		}

		bool aHighLight = aButton->mIsOver;
		g->DrawImageCel(Sexy::IMAGE_CHALLENGE_THUMBNAILS, aPosX + 13, aPosY + 4, kCardIcons[i]);
		// 原版这里两个贴图名是互换的（WINDOW 是高亮框、WINDOW_HIGHLIGHT 是普通框），照抄
		g->DrawImage(aHighLight ? Sexy::IMAGE_CHALLENGE_WINDOW : Sexy::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT,
			aPosX - 6, aPosY - 2);

		Color aTextColor = aHighLight ? Color(250, 40, 40) : Color(42, 42, 90);
		ModText::Font* aNameFont = ModText::GetFont(14, true);
		std::wstring aName = ModText::WideFromUtf8(mCardNames[i].c_str());
		ModText::DrawTextWide(g, aNameFont,
			aPosX + (CARD_W - ModText::TextWidth(aNameFont, aName)) / 2,
			aPosY + 100 - ModText::Ascent(aNameFont),	// 原来是 DrawString 基线，换算成顶
			aName, aTextColor, g->mClipRect);

		ModText::Font* aDescFont = ModText::GetFont(12, false);
		std::wstring aDesc = ModText::WideFromUtf8(mCardDescs[i].c_str());
		ModText::DrawTextWide(g, aDescFont,
			aPosX + (CARD_W - ModText::TextWidth(aDescFont, aDesc)) / 2,
			aPosY + 134 - ModText::Ascent(aDescFont),
			aDesc, Color(96, 72, 40), g->mClipRect);
	}

	// 难度行的小标题（三枚按钮自己画自己，走控件那套）：
	// 卡下说明之下、居中，"多出来的是全队倍率"这层意思写在标题里。
	if (mShowDiff)
	{
		ModText::Font* aCaptionFont = ModText::GetFont(12, false);
		std::wstring aCaption = ModText::WideFromUtf8(mDiffCaption.c_str());
		ModText::DrawTextWide(g, aCaptionFont,
			(mWidth - ModText::TextWidth(aCaptionFont, aCaption)) / 2, mDiffCaptionY,
			aCaption, Color(96, 72, 40), g->mClipRect);
	}
}

// 键盘一律不认（同 OnlineStartDialog / RunPickDialog 的纪律）：LawnDialog::KeyDown 会把
// 空格/回车当成"点了 Yes"，这页上误触一下就是开出一条时长随机的局。
void RunModeDialog::KeyDown(KeyCode theKey)
{
	(void)theKey;
}

//0x4572E0 的同一支音效（LawnDialog::ButtonPress），听着还是原版的石头按钮
void RunModeDialog::ButtonPress(int theId)
{
	(void)theId;
	mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
}

void RunModeDialog::ButtonDepress(int theId)
{
	// 不调 Dialog::ButtonDepress：那条路会把结果转成 2000+/3000+ 的标准对话框编号发给
	// LawnApp::ButtonDepress——结果只在 LawnApp::UpdateAdventureRequest 的 WaitForResult
	// 里收，不走那套路由。
	if (theId >= RunModeDialog_Mode0 && theId <= RunModeDialog_Mode2)
	{
		mResult = theId;
		return;
	}
	// 出怪难度三枚：只换选中（按下态贴图），不关弹窗、不出结果——难度由 LawnApp 在
	// WaitForResult 之后读 mDiffSel 带走。
	if (theId >= RunModeDialog_Diff0 && theId <= RunModeDialog_Diff2)
	{
		mDiffSel = theId - RunModeDialog_Diff0;
		for (int i = 0; i < 3; i++) mDiffButtons[i]->mInverted = (i == mDiffSel);
		return;
	}
	if (theId == Dialog::ID_NO)
	{
		mResult = Dialog::ID_NO;	// 取消 = 关弹窗不开局（WaitForResult 自己收摊）
		return;
	}
}
