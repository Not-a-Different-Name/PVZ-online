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

// 折行时的最小单元：ASCII 连续段算一个整体——「×0.75」「20%」这种不许在中间断开
// （2026-10-03 玩家截图里「0.75」被拆成两行），中文照旧一字一个。
static int RunPickBreakUnit(const std::string& theText, int theIndex)
{
	if (((unsigned char)theText[theIndex]) >= 0x80) return RunPickAnsiUnit(theText, theIndex);
	int anEnd = theIndex;
	while (anEnd < (int)theText.size() && ((unsigned char)theText[anEnd]) < 0x80) anEnd++;
	return anEnd - theIndex;
}

// 断行的唯一实现（显式 \n 分段 + 按列宽折行）：绘制与面积测算共用同一份——
// 2026-10-03 的文本重叠就是两边各算各的、算的比画的少两行算出来的。
static void RunPickWrapLines(SysFont* theFont, const std::string& theText, int theWidth, std::vector<std::string>& theLines)
{
	if (theWidth <= 0) theWidth = 1;
	int aStart = 0;
	while (true)
	{
		int aBreak = (int)theText.find('\n', aStart);
		if (aBreak < 0) aBreak = (int)theText.size();
		std::string aPara = theText.substr(aStart, aBreak - aStart);
		int aPos = 0;
		while (aPos < (int)aPara.size())
		{
			int aFit = aPos;
			int aCursor = aPos;
			while (aCursor < (int)aPara.size())
			{
				int aNext = aCursor + RunPickBreakUnit(aPara, aCursor);
				if (aFit > aPos && theFont->StringWidth(aPara.substr(aPos, aNext - aPos)) > theWidth) break;
				aFit = aNext;
				aCursor = aNext;
			}
			if (aFit == aPos) aFit = aPos + RunPickBreakUnit(aPara, aPos); // 一个单元比整列还宽：硬放
			while (aFit < (int)aPara.size() && RunPickNoLineStart(aPara, aFit))
			{
				aFit += RunPickBreakUnit(aPara, aFit);
			}
			theLines.push_back(aPara.substr(aPos, aFit - aPos));
			aPos = aFit;
		}
		if (aBreak >= (int)theText.size()) break;
		aStart = aBreak + 1;
	}
}

// 这条说明在给定列宽下画出来是几行——对话框的面积按它算（显式 \n 与折行一格不落）。
static int RunPickCountLines(SysFont* theFont, const char* theUtf8, int theWidth)
{
	if (theFont == NULL) return 1;
	std::vector<std::string> aLines;
	RunPickWrapLines(theFont, RunPickAnsiFromUtf8(theUtf8), theWidth, aLines);
	return (int)aLines.size() > 0 ? (int)aLines.size() : 1;
}

// 中文没有空格，WriteWordWrapped 那套按词断行的排版用不了：自己量着宽度断
// （行内的显式 \n 先拆段）。断点不会超列宽，所以文字不会再压到按钮上；
// 整块垂直居中、逐行水平居中。
static void RunPickDrawCjkLines(Graphics* g, SysFont* theFont, const Rect& theRect, const char* theUtf8)
{
	if (theFont == NULL || theRect.mWidth <= 0) return;

	std::vector<std::string> aLines;
	RunPickWrapLines(theFont, RunPickAnsiFromUtf8(theUtf8), theRect.mWidth, aLines);

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
	// 「放弃」（Skip，2026-10-03 用户定案）：三条都不想要时的出路。按钮文案和卡名/标题
	// 一样走位图字体（没有汉字字形），所以是英文；位置在最下面单独一行（见 Resize）。
	mSkipButton = MakeButton(RunPickDialog_Skip, this, _S("Skip"));

	mTallBottom = true;
	mVerticalCenterText = false;
	// 尺寸由文案反推：列宽照最宽的一行（三列按它对齐），列高照"最多的那条说明在当前
	// 列宽下画出来有几行"——RunPickCountLines 走的就是绘制端同一份断行，不再按显式 \n
	// 行数估（2026-10-03 玩家截图：文案折行后比估算多两行，说明压住了【植物名】和
	// 「已有 x/N」）。再加标题行与「已有」行。宽不够就长一轮、高不够也长一轮，两个都
	// 够了才收手；到顶了也收手（剩下的靠绘制端断行兜住，宁可折行不再叠字）。以后改
	// 文案不用回来调数字。
	SysFont* aCjkFont = RunPickCjkFont();
	int aTargetColumn = 0;
	for (int i = 0; i < RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT; i++)
	{
		int aWidth = RunPickDescWidth(aCjkFont, GetRunChoiceDesc(i));
		if (aWidth > aTargetColumn) aTargetColumn = aWidth;
	}
	aTargetColumn += 12; // 两侧各留一点白
	int aFitLineHeight = (aCjkFont != NULL ? aCjkFont->GetHeight() : mLinesFont->GetHeight()) + 3;
	// 底部是两行按钮（三张卡一行、最底「放弃」单独一行）：最小高与上限高都比原来多让
	// 出一行（按钮高 + 6，与 Resize 里三张卡那一行上移的量是同一笔账）。卡片区口径不变。
	int aExtraHeight = IMAGE_BUTTON_LEFT->mHeight + 6;
	CalcSize(430, 150 + aExtraHeight);
	for (int i = 1; i <= 8; i++)
	{
		int aMaxLines = 1;
		for (int j = 0; j < RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT; j++)
		{
			const char* aDesc = GetRunChoiceDesc(j);
			int aLines;
			if (aCjkFont != NULL)
			{
				aLines = RunPickCountLines(aCjkFont, aDesc, mColumnWidth - 4);
			}
			else
			{
				aLines = 1;
				for (const char* aCursor = aDesc; *aCursor != '\0'; aCursor++)
				{
					if (*aCursor == '\n') aLines++;
				}
			}
			if (aLines > aMaxLines) aMaxLines = aLines;
		}
		int aNeededArea = (aMaxLines + 2) * aFitLineHeight + 6;
		bool aWidthDone = mColumnWidth >= aTargetColumn || mWidth >= 740;
		bool aHeightDone = mAreaHeight >= aNeededArea || mHeight >= 460 + aExtraHeight;
		if (aWidthDone && aHeightDone) break;
		CalcSize(430 + i * 30, 150 + i * 20 + aExtraHeight);
	}
	mApp->CenterDialog(this, mWidth, mHeight);
	mClip = false;
}

RunPickDialog::~RunPickDialog()
{
	for (int i = 0; i < 3; i++) delete mChoiceButtons[i];
	delete mSkipButton;
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

	// 卡片区 = 说明那行以下、三张卡那一行以上。跟着同一个公式算，标题换行也不错位。
	int aAreaTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	if (mDialogHeader.size() > 0) aAreaTop += mHeaderFont->GetHeight() + mSpaceAfterHeader;
	aAreaTop += mLinesFont->GetHeight() + 10;
	mAreaTop = aAreaTop;

	// 最底下单独一行放「放弃」（Skip）：三张卡那一行整体上移一行（按钮高 + 6），卡片区的
	// 上沿不动、下沿从卡片行反推——和构造函数里多让的 aExtraHeight 是同一笔账。
	int aSkipY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aSkipY += 5;
	int aButtonY = aSkipY - aButtonHeight - 6;
	mAreaHeight = aButtonY - mAreaTop - 6;
	if (mAreaHeight < 0) mAreaHeight = 0;

	for (int i = 0; i < 3; i++)
	{
		mChoiceButtons[i]->Resize(mColumnX[i], aButtonY, mColumnWidth, aButtonHeight);
	}
	int aSkipWidth = 150;
	mSkipButton->Resize((mColumnX[0] + mColumnX[2] + mColumnWidth) / 2 - aSkipWidth / 2, aSkipY, aSkipWidth, aButtonHeight);
}

void RunPickDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	for (int i = 0; i < 3; i++) AddWidget(mChoiceButtons[i]);
	AddWidget(mSkipButton);
}

void RunPickDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	for (int i = 0; i < 3; i++) RemoveWidget(mChoiceButtons[i]);
	RemoveWidget(mSkipButton);
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
			// 说明区：顶边整行让给【植物名】（aRect 往下挪一行再居中——让出的行不参与
			// 居中，说明就不会往上顶到标题上）、底边整行让给「已有 x/N」。列高在构造时
			// 按真实折行数算过，正常放得下；真到尺寸上限也只会往下压「已有」一行，
			// 2026-10-03 那种标题/说明/已有三头叠字不会再出现。
			Rect aRect(mColumnX[i] + 2, mAreaTop + (aHasHead ? aLineHeight : 0), mColumnWidth - 4,
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
	else if (theId == RunPickDialog_Skip)
	{
		mApp->RunPickSkipped();
	}
}

// 键盘一律不认。按空格/回车会走 LawnDialog::KeyDown 那条"当成点了 Yes"的老路，
// 而这一屏没有 Yes——屏被关掉的话待选的卡还在，下一帧它照样会弹回来，
// 玩家只会觉得"按了没反应"。要么点一张卡、要么点「放弃」（放弃要的是玩家明确的
// 一次点击，不给键盘顺手带过的机会），键盘在这儿什么也别做。
void RunPickDialog::KeyDown(KeyCode theKey)
{
	(void)theKey;
}
