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
#include "graphics/SysFont.h"
#include <cstdio>
#include <vector>

// ── 中文描述这一档 ──────────────────────────────────────────────────────
// 引擎自带的字体全是位图字体，没有中文字形，所以这一屏的中文说明改走 GDI（SysFont）。
// 源文件按 UTF-8 编译（根 CMakeLists 的 /utf-8），而 TextOutA 认的是系统 ANSI
// （简体中文机器上是 CP936）：画之前先转一次码；转不出来（构建配置换了）就原样当 ANSI。
static std::string RunPickAnsiFromUtf8(const char* theUtf8)
{
	std::string aText(theUtf8 != NULL ? theUtf8 : "");
	if (aText.empty()) return aText;

	int aWideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, aText.c_str(), (int)aText.size(), NULL, 0);
	if (aWideLen <= 0) return aText;
	std::vector<wchar_t> aWide(aWideLen);
	MultiByteToWideChar(CP_UTF8, 0, aText.c_str(), (int)aText.size(), &aWide[0], aWideLen);

	int anAnsiLen = WideCharToMultiByte(CP_ACP, 0, &aWide[0], aWideLen, NULL, 0, NULL, NULL);
	if (anAnsiLen <= 0) return aText;
	std::string anAnsi(anAnsiLen, '\0');
	WideCharToMultiByte(CP_ACP, 0, &aWide[0], aWideLen, &anAnsi[0], anAnsiLen, NULL, NULL);
	return anAnsi;
}

// 进程共用的中文字体：雅黑 → 黑体 → 宋体；都找不到也照样建（GDI 会替一支能画的）。
// 字号和 OnlineStartDialog 的正文一档（13pt）。charset 跟着系统码页走（同那边）：
// 简中（CP936）配 GB2312_CHARSET，其他码页退回 ANSI_CHARSET——上面的转码也是按
// 系统码页转的，两边始终对得上。
static SysFont* RunPickCjkFont()
{
	static SysFont* sFont = NULL;
	static bool sTried = false;
	if (!sTried)
	{
		sTried = true;
		const char* aFace = "Microsoft YaHei";
		if (GetFileAttributesA("C:\\Windows\\Fonts\\msyh.ttc") == INVALID_FILE_ATTRIBUTES)
		{
			aFace = (GetFileAttributesA("C:\\Windows\\Fonts\\simhei.ttf") != INVALID_FILE_ATTRIBUTES) ? "SimHei" : "SimSun";
		}
		int aCharset = (GetACP() == 936) ? GB2312_CHARSET : ANSI_CHARSET;
		sFont = new SysFont(gSexyAppBase, aFace, 13, aCharset);
	}
	return sFont;
}

// CP936 里一个字的字节数：ASCII 一字节，其余两字节
static int RunPickAnsiUnit(const std::string& theText, int theIndex)
{
	if (((unsigned char)theText[theIndex]) < 0x80) return 1;
	return (theIndex + 1 < (int)theText.size()) ? 2 : 1;
}

// 不能起行的字（收尾标点）；断行时把它们留在上一行
static bool RunPickNoLineStart(const std::string& theText, int theIndex)
{
	static const char* aPuncts[] = { "、","，","。","！","？","：","；","）","】","》","”","’","%","…" };
	int aLen = RunPickAnsiUnit(theText, theIndex);
	std::string aUnit = theText.substr(theIndex, aLen);
	for (int i = 0; i < (int)(sizeof(aPuncts) / sizeof(aPuncts[0])); i++)
	{
		if (aUnit == aPuncts[i]) return true;
	}
	return false;
}

// 一条描述按显式 \n 拆行后的最大行宽——列宽就按它定，不让任何一条被硬折
static int RunPickDescWidth(SysFont* theFont, const char* theUtf8)
{
	std::string aText = RunPickAnsiFromUtf8(theUtf8);
	int aMax = 0;
	int aStart = 0;
	while (true)
	{
		int aBreak = (int)aText.find('\n', aStart);
		if (aBreak < 0) aBreak = (int)aText.size();
		int aWidth = theFont->StringWidth(aText.substr(aStart, aBreak - aStart));
		if (aWidth > aMax) aMax = aWidth;
		if (aBreak >= (int)aText.size()) break;
		aStart = aBreak + 1;
	}
	return aMax;
}

// 中文没有空格，WriteWordWrapped 那套按词断行的排版用不了：自己量着宽度断
// （行内的显式 \n 先拆段）。断点不会超列宽，所以文字不会再压到按钮上；
// 整块垂直居中、逐行水平居中。
static void RunPickDrawCjkLines(Graphics* g, SysFont* theFont, const Rect& theRect, const char* theUtf8)
{
	if (theFont == NULL || theRect.mWidth <= 0) return;
	std::string aText = RunPickAnsiFromUtf8(theUtf8);

	std::vector<std::string> aLines;
	int aStart = 0;
	while (true)
	{
		int aBreak = (int)aText.find('\n', aStart);
		if (aBreak < 0) aBreak = (int)aText.size();
		std::string aPara = aText.substr(aStart, aBreak - aStart);
		int aPos = 0;
		while (aPos < (int)aPara.size())
		{
			int aFit = aPos;
			int aCursor = aPos;
			while (aCursor < (int)aPara.size())
			{
				int aNext = aCursor + RunPickAnsiUnit(aPara, aCursor);
				if (aFit > aPos && theFont->StringWidth(aPara.substr(aPos, aNext - aPos)) > theRect.mWidth) break;
				aFit = aNext;
				aCursor = aNext;
			}
			if (aFit == aPos) aFit = aPos + RunPickAnsiUnit(aPara, aPos); // 一个字比整列还宽：硬放
			while (aFit < (int)aPara.size() && RunPickNoLineStart(aPara, aFit))
			{
				aFit += RunPickAnsiUnit(aPara, aFit);
			}
			aLines.push_back(aPara.substr(aPos, aFit - aPos));
			aPos = aFit;
		}
		if (aBreak >= (int)aText.size()) break;
		aStart = aBreak + 1;
	}

	int aLineHeight = theFont->GetHeight() + 3;
	int aY = theRect.mY + (theRect.mHeight - (int)aLines.size() * aLineHeight) / 2;
	if (aY < theRect.mY) aY = theRect.mY;
	for (int i = 0; i < (int)aLines.size(); i++)
	{
		int aX = theRect.mX + (theRect.mWidth - theFont->StringWidth(aLines[i])) / 2;
		if (aX < theRect.mX) aX = theRect.mX;
		g->DrawString(aLines[i], aX, aY + theFont->GetAscent() + i * aLineHeight);
	}
}

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
		SexyString aLabel;
		if (mPlantPick)
		{
			aLabel = Plant::GetNameString(theRun->mPlantChoices[i]);
		}
		else if (theRun->mBuffChoices[i] != RunState::RUN_BUFF_CHOICE_NONE)
		{
			aLabel = SexyString(GetRunChoiceName(theRun->mBuffChoices[i]));
		}
		mChoiceButtons[i] = MakeButton(RunPickDialog_Choice0 + i, this, aLabel);
		// 空缺格（防御，方案 §2.4）：空着不画、不可点——置灰兜底；逻辑层 TakeBuffChoice
		// 还会再忽略一次哨兵，双保险。
		if (!mPlantPick && theRun->mBuffChoices[i] == RunState::RUN_BUFF_CHOICE_NONE)
		{
			mChoiceButtons[i]->mDisabled = true;
		}
	}

	mTallBottom = true;
	mVerticalCenterText = false;
	// 尺寸由文案反推：量出全部条目里最宽的一行（三列按它对齐）和最多行数（列高按
	// 描述行数 + 标题行 + 「已有」行推）——植物屏和增益屏于是同一个尺寸，以后改文案
	// 也不用回来调数字。CalcSize 只吃"额外宽高"、标题宽度它自己会加，所以算完从
	// Resize 拿回真实布局，还不够就再要一点（不依赖任何贴图尺寸）。
	SysFont* aCjkFont = RunPickCjkFont();
	int aTargetColumn = 0;
	int aMaxDescLines = 1;
	for (int i = 0; i < RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT; i++)
	{
		const char* aDesc = GetRunChoiceDesc(i);
		int aWidth = RunPickDescWidth(aCjkFont, aDesc);
		if (aWidth > aTargetColumn) aTargetColumn = aWidth;
		int aLines = 1;
		for (const char* aCursor = aDesc; *aCursor != '\0'; aCursor++)
		{
			if (*aCursor == '\n') aLines++;
		}
		if (aLines > aMaxDescLines) aMaxDescLines = aLines;
	}
	aTargetColumn += 12; // 两侧各留一点白
	int aFitLineHeight = (aCjkFont != NULL ? aCjkFont->GetHeight() : mLinesFont->GetHeight()) + 3;
	// 列高：描述行数 + 标题行（单株的【植物名】）+「已有 x/N」行 + 一点余量
	int aTargetArea = (aMaxDescLines + 2) * aFitLineHeight + 4;
	CalcSize(430, 150);
	for (int i = 1; i <= 6 && (mColumnWidth < aTargetColumn || mAreaHeight < aTargetArea) && mWidth < 740 && mHeight < 460; i++)
	{
		CalcSize(430 + i * 30, 150 + i * 20);
	}
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
			unsigned short aBuffId = mRun->mBuffChoices[i];
			// 空缺格（防御，方案 §2.4）：空着不画——这一列什么都不出现
			if (aBuffId == RunState::RUN_BUFF_CHOICE_NONE) continue;

			// 效果说明贴着各自的按钮画：三列各说各的，不用让人去猜哪句话配哪个名字。
			// 中文走 SysFont（位图字体没有中文字形），按列宽断行、逐行居中。
			SysFont* aFont = RunPickCjkFont();
			int aLineHeight = (aFont != NULL ? aFont->GetHeight() : mLinesFont->GetHeight()) + 3;
			// 单株升级在列顶加一行【植物名】（2026-10-03 玩家反馈）：按钮名字是英文位图
			// 字体、说明文案多数也不含植物名——不标出来分不清这条 buff 是哪株的。
			const char* aPlantName = GetRunChoicePlantName(aBuffId);
			bool aHasHead = (aFont != NULL && aPlantName != NULL);
			// 说明区从底边让出一行给「已有 x/N」（方案 §2.5 定案 Q6：说明带「至多 N 层」+
			// 这里报已有层数），顶边也让一行给标题行，免得它们和说明叠在一起。
			Rect aRect(mColumnX[i] + 2, mAreaTop, mColumnWidth - 4,
				mAreaHeight - aLineHeight - 4 - (aHasHead ? aLineHeight : 0));
			if (aFont != NULL)
			{
				g->SetFont(aFont);
				g->SetColor(mColors[Dialog::COLOR_LINES]);
				if (aHasHead)
				{
					std::string aHead = RunPickAnsiFromUtf8((std::string("【") + aPlantName + "】").c_str());
					int aHeadX = mColumnX[i] + (mColumnWidth - aFont->StringWidth(aHead)) / 2;
					if (aHeadX < mColumnX[i]) aHeadX = mColumnX[i];
					g->DrawString(aHead, aHeadX, mAreaTop + aFont->GetAscent());
				}
				RunPickDrawCjkLines(g, aFont, aRect, GetRunChoiceDesc(aBuffId));

				// 「已有 x/N」：封顶条目带 /N；无限条目只报已有层数。画在卡片区底边、逐列居中。
				int aOwned = mRun->GetBuffCount(aBuffId);
				int aCap = GetRunChoiceMaxStacks(aBuffId);
				char aCountUtf8[64];
				if (aCap > 0) snprintf(aCountUtf8, sizeof(aCountUtf8), "已有 %d/%d", aOwned, aCap);
				else snprintf(aCountUtf8, sizeof(aCountUtf8), "已有 %d", aOwned);
				std::string aCountText = RunPickAnsiFromUtf8(aCountUtf8);
				int aCountY = mAreaTop + mAreaHeight - aLineHeight;
				if (aCountY < mAreaTop) aCountY = mAreaTop;
				int aCountX = mColumnX[i] + (mColumnWidth - aFont->StringWidth(aCountText)) / 2;
				g->DrawString(aCountText, aCountX, aCountY + aFont->GetAscent());
			}
			else
			{
				// 连系统字体都建不出来时的兜底：照旧走位图字体（中文会缺字形，但不崩）
				g->SetFont(mLinesFont);
				WriteWordWrapped(g, aRect, SexyString(GetRunChoiceDesc(aBuffId)),
					mLinesFont->GetLineSpacing() + mLineSpacingOffset, mTextAlign);
			}
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
