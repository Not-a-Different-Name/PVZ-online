#include "OnlineStartDialog.h"
#include "CjkStoneButton.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "graphics/Graphics.h"
#include "widget/WidgetManager.h"
#include "../ModText.h"

// 两枚按钮之间的间距（构造时定版心、Resize 里摆位共用同一个数）
#define BUTTON_GAP 24

// 正文多行（公告那种）的行距；单行正文时用不到
#define BODY_LINE_GAP 6

// 源码里的中文字面量是 UTF-8（整个仓库都带 /utf-8 编译）。自 2026-10-03 语言批起，
// 中文绘制走 ModText 的宽字符直绘（UTF-8 → UTF-16 → TextOutW，与系统码页脱钩）：
// 字符串一律存 UTF-8 原样、绘制/量宽时现转；原先这里的 Utf8ToAnsi + GetCjkFont
// （SysFont + TextOutA、进程级缓存）已并入 Lawn/ModText。
// 字号仍按"目标像素高"反算点值：ModText::GetFont 会把点值按屏幕 DPI 折算成像素
// （-MulDiv(pt, GetDeviceCaps(LOGPIXELSY), 72)），150% 缩放（144 DPI）下写死的点值
// 会被放大出近 2 倍——公告（9 行正文）的弹窗正是这样被撑到 800×600 之外、"知道了"
// 按钮掉出屏幕。所以这里反着折算，让最终落地的像素高与 DPI 无关。字号只在这两档上用。
// （CjkPointSize / 石材按钮的宽字符画法已提取到 CjkStoneButton.h，联机面板共用一份。）
#define CJK_TITLE_PX 22
#define CJK_BODY_PX 20

// "第 n / m 页"翻页指示行的字号（像素高）。比正文小一号：它是辅助信息，不抢正文。
#define CJK_PAGE_PX 14

// 正文支持 '\n' 手动分行（启动公告那种多行说明；单行文本 = 一行，老面孔不受影响）、
// '\f' 手动分页（多页公告，最多 4 页——2026-10-08 用户要的翻页）。
// '\n' / '\f' 都是 ASCII：UTF-8 字节流里 0x0A / 0x0C 不会出现在多字节序列内部，
// 按字节数行/分页天然安全。
// 手分行而不是自动换行：宽度可控，换行点由文案自己定，不会在词中间断开也不知道弹窗有多宽。
static int CountBodyLines(const std::string& theBody)
{
	int aCount = 1;
	for (size_t i = 0; i < theBody.size(); i++)
	{
		if (theBody[i] == '\n') aCount++;
	}
	return aCount;
}

static int MeasureBodyWidth(ModText::Font* theFont, const std::string& theBody)
{
	int aMaxWidth = 0;
	size_t aStart = 0;
	for (size_t i = 0; i <= theBody.size(); i++)
	{
		if (i == theBody.size() || theBody[i] == '\n')
		{
			// 只在 '\n' 处切字节，不会切进多字节字符；切片仍是完整 UTF-8
			std::wstring aLine = ModText::WideFromUtf8(theBody.substr(aStart, i - aStart).c_str());
			int aWidth = ModText::TextWidth(theFont, aLine);
			if (aWidth > aMaxWidth) aMaxWidth = aWidth;
			aStart = i + 1;
		}
	}
	return aMaxWidth;
}

static int BodyBlockHeight(ModText::Font* theFont, int theLineCount)
{
	int aLineHeight = ModText::LineHeight(theFont);
	return aLineHeight + (theLineCount - 1) * (aLineHeight + BODY_LINE_GAP);
}

OnlineStartDialog::OnlineStartDialog(LawnApp* theApp, const char* theTitleUtf8, const char* theBodyUtf8,
	const char* theYesUtf8, const char* theNoUtf8, Notify theNotify, bool theDraggable) : LawnDialog(
		theApp, Dialogs::DIALOG_ONLINE_START, true, _S(""), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
	mNotify = theNotify;
	mDraggable = theDraggable;
	mTitle = (theTitleUtf8 != nullptr) ? theTitleUtf8 : "";
	mTitleY = 0;
	mBodyY = 0;
	mPageY = 0;
	mPageIndex = 0;

	// 正文按 '\f' 拆页（最多 4 页；单页面孔 = 只有第 0 页）
	{
		const std::string aBody = (theBodyUtf8 != nullptr) ? theBodyUtf8 : "";
		mPageCount = 0;
		size_t aStart = 0;
		for (size_t i = 0; i <= aBody.size() && mPageCount < 4; i++)
		{
			if (i == aBody.size() || aBody[i] == '\f')
			{
				mPages[mPageCount++] = aBody.substr(aStart, i - aStart);
				aStart = i + 1;
			}
		}
	}

	mButtonCount = 0;
	mButtons[0] = nullptr;
	mButtons[1] = nullptr;
	mButtons[2] = nullptr;
	if (mPageCount > 1)
	{
		// 多页公告：左端摆"上一页"、右端"下一页"（下面按创建顺序摆整行，
		// 中间留着调用方的主按钮——公告就是"知道了"）
		mButtons[mButtonCount] = new CjkStoneButton(ID_PAGE_PREV, this);
		mButtons[mButtonCount]->SetLabel(ModText::Tr("上一页", "Prev"));
		mButtons[mButtonCount]->mHasAlpha = true;
		mButtons[mButtonCount]->mHasTransparencies = true;
		mButtonCount++;
	}
	if (theYesUtf8 != nullptr && theYesUtf8[0] != '\0')
	{
		mButtons[mButtonCount] = new CjkStoneButton(Dialog::ID_YES, this);
		mButtons[mButtonCount]->SetLabel(theYesUtf8);
		mButtons[mButtonCount]->mHasAlpha = true;
		mButtons[mButtonCount]->mHasTransparencies = true;
		mButtonCount++;
	}
	if (theNoUtf8 != nullptr && theNoUtf8[0] != '\0')
	{
		mButtons[mButtonCount] = new CjkStoneButton(Dialog::ID_NO, this);
		mButtons[mButtonCount]->SetLabel(theNoUtf8);
		mButtons[mButtonCount]->mHasAlpha = true;
		mButtons[mButtonCount]->mHasTransparencies = true;
		mButtonCount++;
	}
	if (mPageCount > 1)
	{
		mButtons[mButtonCount] = new CjkStoneButton(ID_PAGE_NEXT, this);
		mButtons[mButtonCount]->SetLabel(ModText::Tr("下一页", "Next"));
		mButtons[mButtonCount]->mHasAlpha = true;
		mButtons[mButtonCount]->mHasTransparencies = true;
		mButtonCount++;
	}

	mTallBottom = (mButtonCount > 0);
	mVerticalCenterText = false;

	// 版心比最长的一行两侧各宽 40；高度 = 标题 + 间距 + 正文（+ 翻页指示行 + 按钮行），
	// 上下都留白——"弹窗不能挤"就落在这些数字上。CalcSize 会按对话框贴图再取整/加高，
	// 多出来的空隙由 Resize 里的居中吸收。多页时正文块按**最高的一页**定高、宽度取
	// 各页最宽行——翻页时框大小与正文顶部位置都不动，只有正文行数换了。
	ModText::Font* aTitleFont = ModText::GetFont(CjkPointSize(CJK_TITLE_PX), true);
	ModText::Font* aBodyFont = ModText::GetFont(CjkPointSize(CJK_BODY_PX), false);
	int aTextWidth = ModText::TextWidth(aTitleFont, ModText::WideFromUtf8(mTitle.c_str()));
	for (int i = 0; i < mPageCount; i++)
	{
		int aPageWidth = MeasureBodyWidth(aBodyFont, mPages[i]);
		if (aPageWidth > aTextWidth) aTextWidth = aPageWidth;
	}

	int anExtraX = aTextWidth + 80;
	int anExtraY = ModText::LineHeight(aTitleFont) + 14 + BodyBlockHeight(aBodyFont, MaxBodyLineCount()) + 46;
	if (mPageCount > 1)
	{
		// 翻页指示行跟在正文块下（位置见 Resize 的 mPageY），版心把它也算上
		anExtraY += BODY_LINE_GAP + ModText::LineHeight(ModText::GetFont(CjkPointSize(CJK_PAGE_PX), false));
	}
	if (mButtonCount > 0)
	{
		// 版心也得放得下整行按钮：按最长的一条标签定每枚按钮的宽度（两侧各留 16），
		// 取整到石材贴图的整段，整行（含间距）反过来把版心撑够——不够宽的话标签会
		// 贴着按钮边、看着像溢出（见 CjkStoneButtonWidth 与 Resize）。
		int aButtonWidth = 0;
		for (int i = 0; i < mButtonCount; i++)
		{
			int aLabelWidth = ModText::TextWidth(aBodyFont, ModText::WideFromUtf8(mButtons[i]->mLabel.c_str())) + 32;
			if (aLabelWidth > aButtonWidth) aButtonWidth = aLabelWidth;
		}
		aButtonWidth = CjkStoneButtonWidth(aButtonWidth, true);

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
	for (int i = 0; i < 3; i++) delete mButtons[i];
}

// 版心定高与摆位都要"最高的一页"，抽出来两边共用一份口径
int OnlineStartDialog::MaxBodyLineCount() const
{
	int aMax = 1;
	for (int i = 0; i < mPageCount; i++)
	{
		int aLines = CountBodyLines(mPages[i]);
		if (aLines > aMax) aMax = aLines;
	}
	return aMax;
}

void OnlineStartDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);

	ModText::Font* aTitleFont = ModText::GetFont(CjkPointSize(CJK_TITLE_PX), true);
	ModText::Font* aBodyFont = ModText::GetFont(CjkPointSize(CJK_BODY_PX), false);

	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;
	int aButtonY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aButtonY += 5;

	// 文字块（标题 + 一行间隔 + 正文 [+ 翻页指示行]）摆在按钮行以上、垂直居中。
	// 正文块按最高的一页定高——翻页时正文顶部与指示行都不挪窝。
	int aLineCount = MaxBodyLineCount();
	int aTextTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	int aTextBottom = (mButtonCount > 0)
		? aButtonY - 10
		: mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom;
	int aBlockHeight = ModText::LineHeight(aTitleFont) + 14 + BodyBlockHeight(aBodyFont, aLineCount);
	if (mPageCount > 1) aBlockHeight += BODY_LINE_GAP + ModText::LineHeight(ModText::GetFont(CjkPointSize(CJK_PAGE_PX), false));
	int aBlockY = aTextTop + (aTextBottom - aTextTop - aBlockHeight) / 2;
	if (aBlockY < aTextTop) aBlockY = aTextTop;
	mTitleY = aBlockY;							// ModText 顶对齐：存的直接是顶（原来是基线口径）
	mBodyY = aBlockY + ModText::LineHeight(aTitleFont) + 14;
	mPageY = mBodyY + BodyBlockHeight(aBodyFont, aLineCount) + BODY_LINE_GAP;

	if (mButtonCount > 0)
	{
		int aContentWidth = mWidth - mContentInsets.mLeft - mContentInsets.mRight
			- mBackgroundInsets.mLeft - mBackgroundInsets.mRight;

		// 宽度取整到石材贴图的整段（平铺铺满，标签按控件宽居中才等于在石头上居中）；
		// 向下取整保证整行放得进版心（构造时保证过版心至少有这么宽）。
		int anAvailable = (aContentWidth - BUTTON_GAP * (mButtonCount - 1)) / mButtonCount;
		int aButtonWidth = CjkStoneButtonWidth(anAvailable, false);
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
		ModText::Font* aTitleFont = ModText::GetFont(CjkPointSize(CJK_TITLE_PX), true);
		std::wstring aTitle = ModText::WideFromUtf8(mTitle.c_str());
		ModText::DrawTextWide(g, aTitleFont,
			(mWidth - ModText::TextWidth(aTitleFont, aTitle)) / 2, mTitleY,
			aTitle, mColors[Dialog::COLOR_HEADER], g->mClipRect);
	}
	// 正文只画当前页（单页面孔 = 第 0 页，与旧行为逐像素一致）
	const std::string& aBody = mPages[mPageIndex];
	if (!aBody.empty())
	{
		ModText::Font* aBodyFont = ModText::GetFont(CjkPointSize(CJK_BODY_PX), false);
		int aY = mBodyY;
		size_t aStart = 0;
		for (size_t i = 0; i <= aBody.size(); i++)
		{
			if (i == aBody.size() || aBody[i] == '\n')
			{
				std::wstring aLine = ModText::WideFromUtf8(aBody.substr(aStart, i - aStart).c_str());
				ModText::DrawTextWide(g, aBodyFont,
					(mWidth - ModText::TextWidth(aBodyFont, aLine)) / 2, aY,
					aLine, mColors[Dialog::COLOR_LINES], g->mClipRect);
				aY += ModText::LineHeight(aBodyFont) + BODY_LINE_GAP;
				aStart = i + 1;
			}
		}
	}
	if (mPageCount > 1)
	{
		// "第 n / m 页"：中文档两截夹数字，英文档 "Page n / m"
		ModText::Font* aPageFont = ModText::GetFont(CjkPointSize(CJK_PAGE_PX), false);
		std::string aLabel = ModText::Tr("第 ", "Page ");
		aLabel += std::to_string(mPageIndex + 1);
		aLabel += " / ";
		aLabel += std::to_string(mPageCount);
		aLabel += ModText::Tr(" 页", "");
		std::wstring aPageText = ModText::WideFromUtf8(aLabel.c_str());
		ModText::DrawTextWide(g, aPageFont,
			(mWidth - ModText::TextWidth(aPageFont, aPageText)) / 2, mPageY,
			aPageText, mColors[Dialog::COLOR_LINES], g->mClipRect);
	}
}

// 键盘一律不认：LawnDialog::KeyDown 会把空格/回车当成"点了 Yes"，而这几种框上
// 误触一下的代价太大（把开局确认掉、把存档选择点掉）。RunPickDialog 同一条纪律。
void OnlineStartDialog::KeyDown(KeyCode theKey)
{
	(void)theKey;
}

// 拖动：Dialog 基类本来就带（MouseDown 记锚点、MouseUp 收摊），但 MouseDrag 会把框
// 钳在屏幕边缘 ±8px 内——公告框大了，被边缘卡住就看不全。这里给 draggable 的框去掉钳制：
// 不设界，框可以拖到屏幕外，想看哪块拖哪块（2026-10-04 用户要求）。
// 锚点沿用基类那一套（mDragMouseX/Y = 光标在框内的落点），不回夹时无需更新——公式里
// x = 绝对坐标 - mX，代入后 aNew = 绝对坐标 - 锚点，光标恒钉在原落点上。
void OnlineStartDialog::MouseDrag(int x, int y)
{
	if (!mDraggable || !mDragging)
	{
		LawnDialog::MouseDrag(x, y);   // 未按下的经过 / 普通框：走 Dialog 的钳制版
		return;
	}
	Move(mX + x - mDragMouseX, mY + y - mDragMouseY);
}

//0x4572E0 的同一支音效（LawnDialog::ButtonPress），听着还是原版的石头按钮
void OnlineStartDialog::ButtonPress(int theId)
{
	(void)theId;
	mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
}

void OnlineStartDialog::ButtonDepress(int theId)
{
	// 翻页按钮：只换页，不设 mResult（设了 WaitForResult 就收摊、框就关了）。
	// 环绕：末页的"下一页"回第 1 页、第 1 页的"上一页"去末页——两枚按钮永远可按，
	// 不用给按钮做禁用态（CjkStoneButton 没有禁用态的画法）。
	if (theId == ID_PAGE_PREV || theId == ID_PAGE_NEXT)
	{
		if (mPageCount > 1)
		{
			mPageIndex = (mPageIndex + ((theId == ID_PAGE_NEXT) ? 1 : mPageCount - 1)) % mPageCount;
			MarkDirty();
		}
		return;
	}
	if (theId != Dialog::ID_YES && theId != Dialog::ID_NO) return;

	// 不调 Dialog::ButtonDepress：那条路会把结果转成 2000+/3000+ 的标准对话框编号发给
	// LawnApp::ButtonDepress——这套框的故事只在 LawnApp 的两个入口里，不走那套路由。
	mResult = theId;
	switch (mNotify)
	{
	case NOTIFY_INVITE_ANSWER:
		// 联机询问框：把结果交回主循环侧（撤框、回 ACK / 续进场都在那儿）
		mApp->OnlineStartPromptAnswer(theId == Dialog::ID_YES);
		break;

	case NOTIFY_WAIT_CANCEL:
		// 主机等队友的看板：取消这次开局（撤框、清覆盖值、菜单交还给玩家）
		mApp->OnlineStartWaitCancelled();
		break;

	default:
		break;	// 阻塞那些（"续不续存档"、单按钮通知）由 WaitForResult 自己收摊。
	}
}
