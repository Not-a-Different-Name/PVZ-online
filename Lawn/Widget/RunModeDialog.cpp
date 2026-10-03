#include "RunModeDialog.h"
#include "GameButton.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "../Run/RunState.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "graphics/Font.h"
#include "graphics/SysFont.h"
#include "graphics/Graphics.h"
#include "widget/WidgetManager.h"

// 卡片尺寸与摆距照抄 ChallengeScreen（卡片 104×115，横向间距 155 → 三张一行 414 宽）
static const int CARD_W = 104;
static const int CARD_H = 115;
static const int CARD_PITCH_X = 155;

// 三张卡的缩略图标（IMAGE_CHALLENGE_THUMBNAILS 的 cel 号，实机看效果可换）：
// 完整版 = 决胜魔音（终局感）、普通版 = 火爆辣椒帽子（中间档）、快速版 = 僵尸快跑（快）
static const int kCardIcons[3] = { 19, 16, 18 };

// 源码里的中文字面量是 UTF-8（整个仓库都带 /utf-8 编译）；SysFont 的 DrawString 走 TextOutA，
// 字节按系统码页解释——简中 Windows 上就是 GBK。所以在这儿做一次转换，两边就对上了。
static std::string Utf8ToAnsi(const char* theText)
{
	int aWideLength = MultiByteToWideChar(CP_UTF8, 0, theText, -1, nullptr, 0);
	if (aWideLength <= 0) return std::string();

	std::wstring aWide((size_t)aWideLength, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, theText, -1, &aWide[0], aWideLength);

	int anAnsiLength = WideCharToMultiByte(CP_ACP, 0, aWide.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (anAnsiLength <= 0) return std::string();

	std::string anAnsi((size_t)anAnsiLength, '\0');
	WideCharToMultiByte(CP_ACP, 0, aWide.c_str(), -1, &anAnsi[0], anAnsiLength, nullptr, nullptr);
	if (!anAnsi.empty() && anAnsi.back() == '\0') anAnsi.pop_back();
	return anAnsi;
}

// 中文用的几支字体（16 粗 = 弹窗标题，14 粗 = 卡上名字，14 细 = 按钮标签，12 细 = 卡下说明），
// 进程级缓存、故意不释放——理由同 OnlineStartDialog：对话框的生命周期比它短，框架里没有
// 统一的字体属主，谁先析构都拿不准，进程退出时系统回收就完了。charset 跟着系统码页走：
// 简中（CP936）配 GB2312_CHARSET，其他码页退回 ANSI_CHARSET（转出来的字节也是那个码页的）。
// 没登记的规格一律并进 14 细那支（字号只在表里这几档上用，够用就行）。
static _Font* GetCjkFont(int thePointSize, bool theBold)
{
	struct FontSlot { int mPointSize; bool mBold; _Font* mFont; };
	static FontSlot aSlots[] = {
		{ 16, true, nullptr }, { 14, true, nullptr }, { 14, false, nullptr }, { 12, false, nullptr },
	};
	_Font** aSlot = &aSlots[2].mFont;	// 兜底：14 细
	for (FontSlot& aEntry : aSlots)
	{
		if (aEntry.mPointSize == thePointSize && aEntry.mBold == theBold)
		{
			aSlot = &aEntry.mFont;
			break;
		}
	}
	if (*aSlot == nullptr)
	{
		int aCharset = (GetACP() == 936) ? GB2312_CHARSET : ANSI_CHARSET;
		*aSlot = new SysFont(gSexyAppBase, "Microsoft YaHei", thePointSize, aCharset, theBold, false, false);
	}
	return *aSlot;
}

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

		_Font* aFont = GetCjkFont(14, false);
		g->SetFont(aFont);
		g->SetColor(mIsOver ? Color(0x9B, 0xF0, 0x60) : Color(0x2F, 0x6B, 0x2B));
		aFontX += (mWidth - aFont->StringWidth(mLabel)) / 2;
		aFontY += (mHeight - aFont->GetHeight()) / 2 + aFont->GetAscent();
		g->DrawString(mLabel, aFontX, aFontY);
	}
};

RunModeDialog::RunModeDialog(LawnApp* theApp, bool theShowDiff) : LawnDialog(
	theApp, Dialogs::DIALOG_ONLINE_START, true, _S(""), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
	// 卡片边框和缩略图标在 ChallengeScreen 的延迟资源组里，不加载就是空指针
	TodLoadResources("DelayLoad_ChallengeScreen");

	mTitle = Utf8ToAnsi("选择闯关模式");
	mCardNames[0] = Utf8ToAnsi("完整版");
	mCardNames[1] = Utf8ToAnsi("普通版");
	mCardNames[2] = Utf8ToAnsi("快速版");
	mCardDescs[0] = Utf8ToAnsi("25 关 · 标准奖励");
	mCardDescs[1] = Utf8ToAnsi("15 关 · 奖励×2");
	mCardDescs[2] = Utf8ToAnsi("10 关 · 奖励×3");
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
		mDiffCaption = Utf8ToAnsi("出怪难度（全队倍率）");
		mDiffLabels[0] = Utf8ToAnsi("轻松 ×0.5");
		mDiffLabels[1] = Utf8ToAnsi("标准 ×1");
		mDiffLabels[2] = Utf8ToAnsi("高压 ×1.5");

		// 三枚等宽（取最长标签量的），石门贴图平铺对宽度有整段要求（见 StoneButtonWidth）
		_Font* aDiffFont = GetCjkFont(14, false);
		int aLabelMax = 0;
		for (int i = 0; i < 3; i++)
		{
			int aWidth = aDiffFont->StringWidth(mDiffLabels[i]);
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
	mCancelButton->SetLabel(Utf8ToAnsi("取消"));
	mCancelButton->mHasAlpha = true;
	mCancelButton->mHasTransparencies = true;
	mCancelWidth = StoneButtonWidth(GetCjkFont(14, false)->StringWidth(mCancelButton->mLabel) + 32, true);

	mTallBottom = true;
	mVerticalCenterText = false;

	// 版心：宽 = 三张一行的宽度（边框两侧各探出几像素，留 16 兜住）；高 = 标题 + 间隔
	// + 卡片（连边框）+ （难度行）+ 按钮。CalcSize 会按对话框贴图再取整/加高，多出来的
	// 空隙由 Resize 里"卡片贴顶、按钮贴底"吸收。
	_Font* aTitleFont = GetCjkFont(16, true);
	int anExtraX = aTitleFont->StringWidth(mTitle) + 80;
	int aRowWidth = CARD_PITCH_X * 2 + CARD_W + 16;
	if (aRowWidth > anExtraX) anExtraX = aRowWidth;
	int anExtraY = aTitleFont->GetHeight() + 18		// 标题 + 与卡片的间隔
		+ CARD_H + 14								// 卡片 + 边框上下探出
		+ IMAGE_BUTTON_LEFT->mHeight + 18;			// 按钮行 + 与卡片的间隔
	if (mShowDiff)
	{
		// 难度行：卡下说明（12 细，基线在卡底 +19）之下再塞一行——小标题 + 间隔 + 按钮
		anExtraY += GetCjkFont(12, false)->GetHeight() + 6 + IMAGE_BUTTON_LEFT->mHeight + 8;
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

	_Font* aTitleFont = GetCjkFont(16, true);

	// 标题贴顶；三张卡片横排在标题下面（一行整体在版心里左右居中）
	int aTitleTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	mTitleY = aTitleTop + aTitleFont->GetAscent();
	int aCardsY = aTitleTop + aTitleFont->GetHeight() + 18;
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
		_Font* aCaptionFont = GetCjkFont(12, false);
		int aCaptionTop = aCardsY + CARD_H + 34;
		mDiffCaptionY = aCaptionTop + aCaptionFont->GetAscent();
		int aDiffY = aCaptionTop + aCaptionFont->GetHeight() + 6;
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
		_Font* aTitleFont = GetCjkFont(16, true);
		g->SetFont(aTitleFont);
		g->SetColor(mColors[Dialog::COLOR_HEADER]);
		g->DrawString(mTitle, (mWidth - aTitleFont->StringWidth(mTitle)) / 2, mTitleY);
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
		_Font* aNameFont = GetCjkFont(14, true);
		g->SetFont(aNameFont);
		g->SetColor(aTextColor);
		g->DrawString(mCardNames[i],
			aPosX + (CARD_W - aNameFont->StringWidth(mCardNames[i])) / 2, aPosY + 100);

		_Font* aDescFont = GetCjkFont(12, false);
		g->SetFont(aDescFont);
		g->SetColor(Color(96, 72, 40));
		g->DrawString(mCardDescs[i],
			aPosX + (CARD_W - aDescFont->StringWidth(mCardDescs[i])) / 2, aPosY + 134);
	}

	// 难度行的小标题（三枚按钮自己画自己，走控件那套）：
	// 卡下说明之下、居中，"多出来的是全队倍率"这层意思写在标题里。
	if (mShowDiff)
	{
		_Font* aCaptionFont = GetCjkFont(12, false);
		g->SetFont(aCaptionFont);
		g->SetColor(Color(96, 72, 40));
		g->DrawString(mDiffCaption, (mWidth - aCaptionFont->StringWidth(mDiffCaption)) / 2, mDiffCaptionY);
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
