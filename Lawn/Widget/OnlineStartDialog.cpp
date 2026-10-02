#include "OnlineStartDialog.h"
#include "GameButton.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "graphics/Font.h"
#include "graphics/SysFont.h"
#include "graphics/Graphics.h"
#include "widget/WidgetManager.h"

// 两枚按钮之间的间距（构造时定版心、Resize 里摆位共用同一个数）
#define BUTTON_GAP 24

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

// 中文用的两支字体（16pt 粗 = 标题，14pt = 正文/按钮），进程级缓存、故意不释放：
// 对话框的生命周期比它短，框架里也没有统一的字体属主，谁先析构都拿不准——进程退出时
// 系统回收就完了。charset 跟着系统码页走：简中（CP936）配 GB2312_CHARSET，
// 其他码页退回 ANSI_CHARSET（那时候上面转出来的字节也是那个码页的，仍能对上）。
// 字号只在这两档上用，别的字号直接返回对应的那支（够用就行，不扩）。
static _Font* GetCjkFont(int thePointSize, bool theBold)
{
	static _Font* aTitleFont = nullptr;
	static _Font* aBodyFont = nullptr;
	_Font*& aSlot = theBold ? aTitleFont : aBodyFont;

	if (aSlot == nullptr)
	{
		int aCharset = (GetACP() == 936) ? GB2312_CHARSET : ANSI_CHARSET;
		aSlot = new SysFont(gSexyAppBase, "Microsoft YaHei", thePointSize, aCharset, theBold, false, false);
	}
	return aSlot;
}

// 石材按钮是"左端贴图 + 中段贴图 × n + 右端贴图"平铺画的（见 CjkStoneButton::Draw），
// 宽度必须正好是这三段的和；随手给个宽度的话平铺铺不满，画出来的石头比控件窄一截，
// 标签按控件宽居中就会整体偏右（字越多越显得贴边）。原版 LawnDialog::Resize 也做同款
// 取整："不足中部贴图宽度的部分补充至中部贴图宽度"。
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

// 石材按钮 + 中文标签：原版 DrawStoneButton 把标签字体写死成位图字体（没有汉字），
// 这里照抄它的画法（贴图平铺、按下位移、居中），只把字体和颜色换掉。
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

OnlineStartDialog::OnlineStartDialog(LawnApp* theApp, const char* theTitleUtf8, const char* theBodyUtf8,
	const char* theYesUtf8, const char* theNoUtf8, bool theNotifyApp) : LawnDialog(
		theApp, Dialogs::DIALOG_ONLINE_START, true, _S(""), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
	mNotifyApp = theNotifyApp;
	mTitle = Utf8ToAnsi(theTitleUtf8 != nullptr ? theTitleUtf8 : "");
	mBody = Utf8ToAnsi(theBodyUtf8 != nullptr ? theBodyUtf8 : "");
	mTitleY = 0;
	mBodyY = 0;

	mButtonCount = 0;
	mButtons[0] = nullptr;
	mButtons[1] = nullptr;
	if (theYesUtf8 != nullptr && theYesUtf8[0] != '\0')
	{
		mButtons[mButtonCount] = new CjkStoneButton(Dialog::ID_YES, this);
		mButtons[mButtonCount]->SetLabel(Utf8ToAnsi(theYesUtf8));
		mButtons[mButtonCount]->mHasAlpha = true;
		mButtons[mButtonCount]->mHasTransparencies = true;
		mButtonCount++;
	}
	if (theNoUtf8 != nullptr && theNoUtf8[0] != '\0')
	{
		mButtons[mButtonCount] = new CjkStoneButton(Dialog::ID_NO, this);
		mButtons[mButtonCount]->SetLabel(Utf8ToAnsi(theNoUtf8));
		mButtons[mButtonCount]->mHasAlpha = true;
		mButtons[mButtonCount]->mHasTransparencies = true;
		mButtonCount++;
	}

	mTallBottom = (mButtonCount > 0);
	mVerticalCenterText = false;

	// 版心比最长的一行两侧各宽 40；高度 = 标题 + 间距 + 正文（+ 按钮行），上下都留白——
	// "弹窗不能挤"就落在这些数字上。CalcSize 会按对话框贴图再取整/加高，多出来的空隙
	// 由 Resize 里的居中吸收。
	_Font* aTitleFont = GetCjkFont(16, true);
	_Font* aBodyFont = GetCjkFont(14, false);
	int aTextWidth = aTitleFont->StringWidth(mTitle);
	int aBodyWidth = aBodyFont->StringWidth(mBody);
	if (aBodyWidth > aTextWidth) aTextWidth = aBodyWidth;

	int anExtraX = aTextWidth + 80;
	int anExtraY = aTitleFont->GetHeight() + 14 + aBodyFont->GetHeight() + 46;
	if (mButtonCount > 0)
	{
		// 版心也得放得下整行按钮：按最长的一条标签定每枚按钮的宽度（两侧各留 16），
		// 取整到石材贴图的整段，整行（含间距）反过来把版心撑够——不够宽的话标签会
		// 贴着按钮边、看着像溢出（见 StoneButtonWidth 与 Resize）。
		int aButtonWidth = 0;
		for (int i = 0; i < mButtonCount; i++)
		{
			int aLabelWidth = aBodyFont->StringWidth(mButtons[i]->mLabel) + 32;
			if (aLabelWidth > aButtonWidth) aButtonWidth = aLabelWidth;
		}
		aButtonWidth = StoneButtonWidth(aButtonWidth, true);

		int aButtonsWidth = aButtonWidth * mButtonCount + BUTTON_GAP * (mButtonCount - 1);
		if (aButtonsWidth > anExtraX) anExtraX = aButtonsWidth;
		anExtraY += IMAGE_BUTTON_LEFT->mHeight + 18;
	}

	CalcSize(anExtraX, anExtraY);
	mApp->CenterDialog(this, mWidth, mHeight);
	mClip = false;
}

OnlineStartDialog::~OnlineStartDialog()
{
	for (int i = 0; i < 2; i++) delete mButtons[i];
}

void OnlineStartDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);

	_Font* aTitleFont = GetCjkFont(16, true);
	_Font* aBodyFont = GetCjkFont(14, false);

	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;
	int aButtonY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aButtonY += 5;

	// 文字块（标题 + 一行间隔 + 正文）摆在按钮行以上、垂直居中
	int aTextTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	int aTextBottom = (mButtonCount > 0)
		? aButtonY - 10
		: mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom;
	int aBlockHeight = aTitleFont->GetHeight() + 14 + aBodyFont->GetHeight();
	int aBlockY = aTextTop + (aTextBottom - aTextTop - aBlockHeight) / 2;
	if (aBlockY < aTextTop) aBlockY = aTextTop;
	mTitleY = aBlockY + aTitleFont->GetAscent();
	mBodyY = aBlockY + aTitleFont->GetHeight() + 14 + aBodyFont->GetAscent();

	if (mButtonCount > 0)
	{
		int aContentWidth = mWidth - mContentInsets.mLeft - mContentInsets.mRight
			- mBackgroundInsets.mLeft - mBackgroundInsets.mRight;

		// 宽度取整到石材贴图的整段（平铺铺满，标签按控件宽居中才等于在石头上居中）；
		// 向下取整保证整行放得进版心（构造时保证过版心至少有这么宽）。
		int anAvailable = (aContentWidth - BUTTON_GAP * (mButtonCount - 1)) / mButtonCount;
		int aButtonWidth = StoneButtonWidth(anAvailable, false);
		int aButtonsWidth = aButtonWidth * mButtonCount + BUTTON_GAP * (mButtonCount - 1);

		// 版心比整行宽时（CalcSize 会把对话框再撑大）多出来的空白左右对半分
		int aLeft = mContentInsets.mLeft + mBackgroundInsets.mLeft + (aContentWidth - aButtonsWidth) / 2;
		for (int i = 0; i < mButtonCount; i++)
		{
			mButtons[i]->Resize(aLeft + i * (aButtonWidth + BUTTON_GAP), aButtonY, aButtonWidth, aButtonHeight);
		}
	}
}

void OnlineStartDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	for (int i = 0; i < mButtonCount; i++) AddWidget(mButtons[i]);
}

void OnlineStartDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	for (int i = 0; i < mButtonCount; i++) RemoveWidget(mButtons[i]);
}

void OnlineStartDialog::Draw(Graphics* g)
{
	LawnDialog::Draw(g);   // 底板。header / lines 都是空的，只出框。

	if (!mTitle.empty())
	{
		g->SetFont(GetCjkFont(16, true));
		g->SetColor(mColors[Dialog::COLOR_HEADER]);
		WriteCenteredLine(g, mTitleY, mTitle);
	}
	if (!mBody.empty())
	{
		g->SetFont(GetCjkFont(14, false));
		g->SetColor(mColors[Dialog::COLOR_LINES]);
		WriteCenteredLine(g, mBodyY, mBody);
	}
}

// 键盘一律不认：LawnDialog::KeyDown 会把空格/回车当成"点了 Yes"，而这几种框上
// 误触一下的代价太大（把开局确认掉、把存档选择点掉）。RunPickDialog 同一条纪律。
void OnlineStartDialog::KeyDown(KeyCode theKey)
{
	(void)theKey;
}

//0x4572E0 的同一支音效（LawnDialog::ButtonPress），听着还是原版的石头按钮
void OnlineStartDialog::ButtonPress(int theId)
{
	(void)theId;
	mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
}

void OnlineStartDialog::ButtonDepress(int theId)
{
	if (theId != Dialog::ID_YES && theId != Dialog::ID_NO) return;

	// 不调 Dialog::ButtonDepress：那条路会把结果转成 2000+/3000+ 的标准对话框编号发给
	// LawnApp::ButtonDepress——这套框的故事只在 LawnApp 的两个入口里，不走那套路由。
	mResult = theId;
	if (mNotifyApp)
	{
		// 联机询问框：把结果交回主循环侧（撤框、回 ACK / 续进场都在那儿）
		mApp->OnlineStartPromptAnswer(theId == Dialog::ID_YES);
	}
	// 阻塞那些（"续不续存档"、单按钮通知）由 WaitForResult 自己收摊。
}
