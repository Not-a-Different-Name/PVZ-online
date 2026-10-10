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
#include "graphics/Graphics.h"
#include "../ModText.h"
#include <cstdio>
#include <vector>

// ── 中文描述这一档 ──────────────────────────────────────────────────────
// 引擎自带的字体全是位图字体，没有中文字形，所以这一屏的中文说明走 ModText 的宽字符
// 直绘（UTF-8 → UTF-16 → TextOutW，与系统码页脱钩；2026-10-03 语言批起）。
// 原先这里的转码（RunPickAnsiFromUtf8）与字体缓存（RunPickCjkFont、13pt）已并入
// Lawn/ModText；断行系统一并宽字符化——不再有 CP936 双字节的字节口径，一个 wchar_t
// 就是一个单元。

static ModText::Font* RunPickCjkFont() { return ModText::GetFont(13, false); }

// 折行时的最小单元：中文一字一个；空格独立成单元（英文在空格处断行）；英文单词
// 不打散——「0.75/stack」「20%」整着走，「×0.75」的 × 归到后一个词里一起走
// （2026-10-03 玩家截图里「0.75」被拆成两行是旧口径的毛病）。
// 2026-10-04：旧口径把"连续 ASCII"当一个单元——纯英文句子整句都是 ASCII，成了一句
// 放不下的巨型单元、被硬放进位（wrap 里"一个单元比整列还宽：硬放"），英文档的说明
// 就是这么横穿邻列叠在一起的。
static int RunPickBreakUnit(const std::wstring& theText, int theIndex)
{
	wchar_t aChar = theText[theIndex];
	if (aChar == L' ') return 1;
	if (aChar >= 0x80 && aChar != 0x00D7) return 1;	// ×（0xD7）例外：随后面的词走
	int anEnd = theIndex;
	while (anEnd < (int)theText.size())
	{
		wchar_t aCh = theText[anEnd];
		if (aCh == L' ') break;
		if (aCh >= 0x80 && aCh != 0x00D7) break;
		anEnd++;
	}
	return anEnd - theIndex;
}

// 不能起行的字（收尾标点）；断行时把它们留在上一行
static bool RunPickNoLineStart(const std::wstring& theText, int theIndex)
{
	static const wchar_t* aPuncts[] = { L"、",L"，",L"。",L"！",L"？",L"：",L"；",L"）",L"】",L"》",L"”",L"’",L"%",L"…" };
	std::wstring aUnit = theText.substr(theIndex, 1);
	for (int i = 0; i < (int)(sizeof(aPuncts) / sizeof(aPuncts[0])); i++)
	{
		if (aUnit == aPuncts[i]) return true;
	}
	return false;
}

// 一条描述按显式 \n 拆行后的最大行宽——列宽就按它定，不让任何一条被硬折
static int RunPickDescWidth(ModText::Font* theFont, const char* theUtf8)
{
	std::wstring aText = ModText::WideFromUtf8(theUtf8);
	int aMax = 0;
	int aStart = 0;
	while (true)
	{
		int aBreak = (int)aText.find(L'\n', aStart);
		if (aBreak < 0) aBreak = (int)aText.size();
		int aWidth = ModText::TextWidth(theFont, aText.substr(aStart, aBreak - aStart));
		if (aWidth > aMax) aMax = aWidth;
		if (aBreak >= (int)aText.size()) break;
		aStart = aBreak + 1;
	}
	return aMax;
}

// 断行的唯一实现（显式 \n 分段 + 按列宽折行）：绘制与面积测算共用同一份——
// 2026-10-03 的文本重叠就是两边各算各的、算的比画的少两行算出来的。
static void RunPickWrapLines(ModText::Font* theFont, const std::wstring& theText, int theWidth, std::vector<std::wstring>& theLines)
{
	if (theWidth <= 0) theWidth = 1;
	int aStart = 0;
	while (true)
	{
		int aBreak = (int)theText.find(L'\n', aStart);
		if (aBreak < 0) aBreak = (int)theText.size();
		std::wstring aPara = theText.substr(aStart, aBreak - aStart);
		int aPos = 0;
		while (aPos < (int)aPara.size())
		{
			int aFit = aPos;
			int aCursor = aPos;
			while (aCursor < (int)aPara.size())
			{
				int aNext = aCursor + RunPickBreakUnit(aPara, aCursor);
				if (aFit > aPos && ModText::TextWidth(theFont, aPara.substr(aPos, aNext - aPos)) > theWidth) break;
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
static int RunPickCountLines(ModText::Font* theFont, const char* theUtf8, int theWidth)
{
	std::vector<std::wstring> aLines;
	RunPickWrapLines(theFont, ModText::WideFromUtf8(theUtf8), theWidth, aLines);
	return (int)aLines.size() > 0 ? (int)aLines.size() : 1;
}

// 中文没有空格，WriteWordWrapped 那套按词断行的排版用不了：自己量着宽度断
// （行内的显式 \n 先拆段）。断点不会超列宽，所以文字不会再压到按钮上；
// 整块垂直居中、逐行水平居中。
static void RunPickDrawCjkLines(Graphics* g, ModText::Font* theFont, const Rect& theRect,
	const char* theUtf8, const Sexy::Color& theColor)
{
	if (theRect.mWidth <= 0) return;

	std::vector<std::wstring> aLines;
	RunPickWrapLines(theFont, ModText::WideFromUtf8(theUtf8), theRect.mWidth, aLines);

	int aLineHeight = ModText::LineHeight(theFont) + 3;
	int aY = theRect.mY + (theRect.mHeight - (int)aLines.size() * aLineHeight) / 2;
	if (aY < theRect.mY) aY = theRect.mY;
	for (int i = 0; i < (int)aLines.size(); i++)
	{
		int aX = theRect.mX + (theRect.mWidth - ModText::TextWidth(theFont, aLines[i])) / 2;
		if (aX < theRect.mX) aX = theRect.mX;
		// 原 DrawString 是基线口径（y + Ascent 才是顶），这里收顶对齐：
		// 行的顶就是 aY + i * aLineHeight
		ModText::DrawTextWide(g, theFont, aX, aY + i * aLineHeight, aLines[i], theColor, g->mClipRect);
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
			// @pvz-online: 空缺格（玩家闪退修复 2026-10-11）：植物候选抽干时 RollChoices
			// 按屏格数缺格填 SEED_NONE——空位不取名。名字走 Plant::GetNameString，
			// SEED_NONE（-1）会读穿植物定义表（GetPlantDefinition 裸下标，Release 断言
			// 为空）→ 野指针崩。2026-10-10 玩家「深夜泳池第三关」闪退的根因。
			if (theRun->mPlantChoices[i] != SeedType::SEED_NONE)
			{
				aLabel = Plant::GetNameString(theRun->mPlantChoices[i]);
			}
		}
		else if (theRun->mBuffChoices[i] != RunState::RUN_BUFF_CHOICE_NONE)
		{
			aLabel = SexyString(GetRunChoiceName(theRun->mBuffChoices[i]));
		}
		mChoiceButtons[i] = MakeButton(RunPickDialog_Choice0 + i, this, aLabel);
		// 空缺格（防御，方案 §2.4）：空着不画、不可点——置灰兜底；逻辑层 TakePlantChoice /
		// TakeBuffChoice 还会再忽略一次哨兵，双保险。植物屏与增益屏同口径（闪退修复
		// 2026-10-11 前只有增益屏这一道）。
		bool aEmptySlot = mPlantPick
			? (theRun->mPlantChoices[i] == SeedType::SEED_NONE)
			: (theRun->mBuffChoices[i] == RunState::RUN_BUFF_CHOICE_NONE);
		if (aEmptySlot)
		{
			mChoiceButtons[i]->mDisabled = true;
		}
		// 3★ 金色字（权重批 2026-10-09，docs/06 §8.7）：增益屏非空格按稀有度档位染色。
		// 未定档（0）恒为白，零行为。植物屏不参与染色——mBuffChoices 还是上一批增益屏
		// 的残留值，拿来查会把植物卡错染成金。
		else if (!mPlantPick && GetRunChoiceRarity(theRun->mBuffChoices[i]) == 3)
		{
			mChoiceButtons[i]->mGoldLabel = true;
		}
	}
	// 「放弃」（Skip，2026-10-03 用户定案）：三条都不想要时的出路。按钮文案和卡名/标题
	// 一样走位图字体（没有汉字字形），所以是英文；位置在最下面一行、和「换一批」并排（见 Resize）。
	mSkipButton = MakeButton(RunPickDialog_Skip, this, _S("Skip"));
	// 「换一批」（Refresh，2026-10-08 玩家反馈定案）：这一屏的候选重抽一次，每屏限一次。
	// 机会已经用掉时置为不可点——框架的 Widget::mDisabled，点在派发层就被拦下（和本作
	// 其它置灰石钮同口径：不变暗、只是按不动）；重开这一屏时按当时的记账重算
	// （不可点状态跟着 mPickReRolled 走，见 RunState::RerollChoices）。
	mRefreshButton = MakeButton(RunPickDialog_Refresh, this, _S("Refresh"));
	if (theRun->mPickReRolled) mRefreshButton->mDisabled = true;

	mTallBottom = true;
	mVerticalCenterText = false;
	// 尺寸由内容反推，且两个驱动按屏分家（2026-10-04 用户反馈"NEW PLANT 屏过大"）：
	//  植物屏 列宽只按按钮上的植物名（标签，位图字体）与种子包定，面积只要放下包；
	//  增益屏 列宽按说明的显式行宽，面积按"最多的那条说明在当前列宽下折几行"——
	//         RunPickCountLines 走的就是绘制端同一份断行，不再按显式 \n 行数估。
	// 之前两屏共用一套驱动：植物屏根本不放那些说明，却被它们的折行数顶着长到
	// CalcSize 顶档——弹窗比画布 800 还宽，两侧石头边框被画布切掉，包和按钮之间
	// 空着百来像素的死区。以后改文案不用回来调数字。
	ModText::Font* aCjkFont = RunPickCjkFont();
	int aTargetColumn = 0;
	if (mPlantPick)
	{
		aTargetColumn = SEED_PACKET_WIDTH + 16;
		for (int i = 0; i < 3; i++)
		{
			// 标签用绘制端同款字体量（GameButton.cpp 的 DrawStoneButton）——宽就是列宽
			int aLabelWidth = Sexy::FONT_DWARVENTODCRAFT18GREENINSET->StringWidth(mChoiceButtons[i]->mLabel) + 16;
			if (aLabelWidth > aTargetColumn) aTargetColumn = aLabelWidth;
		}
	}
	else
	{
		// 全池静态测量（增益屏列宽按全池最长说明）：三段 id 都测，第二表里未进池的
		// 条目跳过（不会上屏，不参与定宽）。
		for (int i = 0; i < RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_PLANT_BUFF2_COUNT; i++)
		{
			if (i >= RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT && !RunPlantBuff2InPool(i - RUN_BUFF_COUNT - RUN_PLANT_UPGRADE_COUNT)) continue;
			int aWidth = RunPickDescWidth(aCjkFont, GetRunChoiceDesc(i));
			if (aWidth > aTargetColumn) aTargetColumn = aWidth;
		}
		aTargetColumn += 12; // 两侧各留一点白
		// 列宽目标封顶：英文说明是长句（"Sky sun falls 20% faster, up to 4 stacks"），
		// 按显式行宽推目标会一路把弹窗顶着长到整屏宽——封顶后交给折行处理
		// （2026-10-04 用户反馈：英文档这个框大到快出屏）。
		if (aTargetColumn > 220) aTargetColumn = 220;
	}
	int aFitLineHeight = ModText::LineHeight(aCjkFont) + 3;
	// 底部是两行按钮（三张卡一行、最底「放弃」单独一行）：最小高与上限高都比原来多让
	// 出一行（按钮高 + 6，与 Resize 里三张卡那一行上移的量是同一笔账）。卡片区口径不变。
	int aExtraHeight = IMAGE_BUTTON_LEFT->mHeight + 6;
	// 起手宽度：植物屏三列只为放包与名字，不必按增益屏的 430 起步（360 起，两屏的
	// 标题与边框内缩量会把总宽再抬到比画布窄一截的位置）。
	int aSizeX = mPlantPick ? 360 : 430;
	int aSizeY = 150 + aExtraHeight;
	CalcSize(aSizeX, aSizeY);
	// 宽的上限从 740 收到 580（2026-10-04）：说明改按词折行后，不再需要为一句长文
	// 把弹窗拉满整屏——到 580 就收手，多出来的行交给折行。
	// 宽、高各自达标即各自停长：两个条件绑在同一个 i 上时，高不达标会把宽也一级一级
	// 抬到顶档（植物屏被增益说明的折行数拖成整屏宽，正是这条路）。
	for (int i = 1; i <= 6; i++)
	{
		int aNeededArea;
		if (mPlantPick)
		{
			// 面积里只有种子包（名字在按钮上）——包高 + 上下留白
			aNeededArea = SEED_PACKET_HEIGHT + 12;
		}
		else
		{
			int aMaxLines = 1;
			for (int j = 0; j < RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_PLANT_BUFF2_COUNT; j++)
			{
				if (j >= RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT && !RunPlantBuff2InPool(j - RUN_BUFF_COUNT - RUN_PLANT_UPGRADE_COUNT)) continue;
				int aLines = RunPickCountLines(aCjkFont, GetRunChoiceDesc(j), mColumnWidth - 4);
				if (aLines > aMaxLines) aMaxLines = aLines;
			}
			aNeededArea = (aMaxLines + 2) * aFitLineHeight + 6;
		}
		bool aWidthDone = mColumnWidth >= aTargetColumn || mWidth >= 580;
		bool aHeightDone = mAreaHeight >= aNeededArea || mHeight >= 460 + aExtraHeight;
		if (aWidthDone && aHeightDone) break;
		if (!aWidthDone) aSizeX += 20;
		if (!aHeightDone) aSizeY += 20;
		CalcSize(aSizeX, aSizeY);
	}
	mApp->CenterDialog(this, mWidth, mHeight);
	mClip = false;
}

RunPickDialog::~RunPickDialog()
{
	for (int i = 0; i < 3; i++) delete mChoiceButtons[i];
	delete mSkipButton;
	delete mRefreshButton;
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

	// 最底下那一行放两个按钮（2026-10-08 起：「换一批」在左、「放弃」在右，整行居中）：
	// 三张卡那一行整体上移一行（按钮高 + 6），卡片区的上沿不动、下沿从卡片行反推——
	// 和构造函数里多让的 aExtraHeight 是同一笔账（行数没变，高度账原样）。
	int aSkipY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aSkipY += 5;
	int aButtonY = aSkipY - aButtonHeight - 6;
	mAreaHeight = aButtonY - mAreaTop - 6;
	if (mAreaHeight < 0) mAreaHeight = 0;

	for (int i = 0; i < 3; i++)
	{
		mChoiceButtons[i]->Resize(mColumnX[i], aButtonY, mColumnWidth, aButtonHeight);
	}
	int aFootCenter = (mColumnX[0] + mColumnX[2] + mColumnWidth) / 2;
	int aFootWidth = 150;
	mRefreshButton->Resize(aFootCenter - 8 - aFootWidth, aSkipY, aFootWidth, aButtonHeight);
	mSkipButton->Resize(aFootCenter + 8, aSkipY, aFootWidth, aButtonHeight);
}

void RunPickDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	for (int i = 0; i < 3; i++) AddWidget(mChoiceButtons[i]);
	AddWidget(mSkipButton);
	AddWidget(mRefreshButton);
}

void RunPickDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	for (int i = 0; i < 3; i++) RemoveWidget(mChoiceButtons[i]);
	RemoveWidget(mSkipButton);
	RemoveWidget(mRefreshButton);
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
			// 空缺格（玩家闪退修复 2026-10-11）：植物候选抽干时缺格填 SEED_NONE，空着不画——
			// DrawSeedPacket 对 SEED_NONE（-1）会走种子包路径的查表/升级判断（Release 断言为空）。
			// 与增益屏下面那道哨兵护栏同口径。
			if (mRun->mPlantChoices[i] == SeedType::SEED_NONE) continue;
			DrawSeedPacket(g, (float)(mColumnX[i] + (mColumnWidth - SEED_PACKET_WIDTH) / 2), (float)mAreaTop,
				mRun->mPlantChoices[i], SeedType::SEED_NONE, 0.0f, 255, false, false);
		}
		else
		{
			unsigned short aBuffId = mRun->mBuffChoices[i];
			// 空缺格（防御，方案 §2.4）：空着不画——这一列什么都不出现
			if (aBuffId == RunState::RUN_BUFF_CHOICE_NONE) continue;

			// 效果说明贴着各自的按钮画：三列各说各的，不用让人去猜哪句话配哪个名字。
			// 中文走 ModText 宽字符直绘，按列宽断行、逐行居中。
			ModText::Font* aFont = RunPickCjkFont();
			int aLineHeight = ModText::LineHeight(aFont) + 3;
			// 列顶一行：单株升级的【植物名】（2026-10-03 玩家反馈——按钮名字是英文位图
			// 字体、说明文案多数也不含植物名，不标出来分不清这条 buff 是哪株的）+ 稀有度
			// 星标（2026-10-09 晚实机反馈：全档可见，3★ 金色、1★/2★ 灰色，同图鉴
			// Almanac::DrawRarityStars 口径；旧口径「三选一屏只标 3★」作废）。全局条
			// 没有植物名，就只画星。
			const char* aPlantName = GetRunChoicePlantName(aBuffId);
			std::wstring aHead;
			if (aPlantName != NULL)
			{
				// 括号也随语言：【植物名】 / [Plant Name]
				std::string aHeadUtf8 = ModText::IsChinese()
					? std::string("【") + aPlantName + "】"
					: std::string("[") + aPlantName + "]";
				aHead = ModText::WideFromUtf8(aHeadUtf8.c_str());
			}
			int aRarity = GetRunChoiceRarity(aBuffId);
			std::wstring aStars = (aRarity > 0)
				? std::wstring((size_t)aRarity, (wchar_t)0x2605) : std::wstring();
			// 说明区：顶边整行让给名字/星标行、底边整行让给「已有 x/N」。列高在构造时
			// 按真实折行数算过（预算的 +2 行就是这两行），正常放得下；真到尺寸上限也
			// 只会往下压「已有」一行，2026-10-03 那种标题/说明/已有三头叠字不会再出现。
			Rect aRect(mColumnX[i] + 2, mAreaTop + aLineHeight, mColumnWidth - 4,
				mAreaHeight - 2 * aLineHeight - 4);
			{
				int aHeadW = aHead.empty() ? 0 : ModText::TextWidth(aFont, aHead);
				int aStarsW = aStars.empty() ? 0 : ModText::TextWidth(aFont, aStars);
				int aTopX = mColumnX[i] + (mColumnWidth - aHeadW - aStarsW
					- ((aHeadW > 0 && aStarsW > 0) ? 6 : 0)) / 2;
				if (aTopX < mColumnX[i]) aTopX = mColumnX[i];
				if (aHeadW > 0)
				{
					ModText::DrawTextWide(g, aFont, aTopX, mAreaTop, aHead,
						mColors[Dialog::COLOR_LINES], g->mClipRect);
					aTopX += aHeadW + 6;
				}
				if (aStarsW > 0)
				{
					ModText::DrawTextWide(g, aFont, aTopX, mAreaTop, aStars,
						aRarity >= 3 ? Sexy::Color(255, 200, 60) : Sexy::Color(150, 140, 130), g->mClipRect);
				}
			}
			RunPickDrawCjkLines(g, aFont, aRect, GetRunChoiceDesc(aBuffId), mColors[Dialog::COLOR_LINES]);

			// 「已有 x/N」：封顶条目带 /N；无限条目只报已有层数。画在卡片区底边、逐列居中。
			int aOwned = mRun->GetBuffCount(aBuffId);
			int aCap = GetRunChoiceMaxStacks(aBuffId);
			char aCountUtf8[64];
			if (aCap > 0) snprintf(aCountUtf8, sizeof(aCountUtf8), ModText::Tr("已有 %d/%d", "Owned %d/%d"), aOwned, aCap);
			else snprintf(aCountUtf8, sizeof(aCountUtf8), ModText::Tr("已有 %d", "Owned %d"), aOwned);
			std::wstring aCountText = ModText::WideFromUtf8(aCountUtf8);
			int aCountY = mAreaTop + mAreaHeight - aLineHeight;
			if (aCountY < mAreaTop) aCountY = mAreaTop;
			int aCountX = mColumnX[i] + (mColumnWidth - ModText::TextWidth(aFont, aCountText)) / 2;
			ModText::DrawTextWide(g, aFont, aCountX, aCountY, aCountText,
				mColors[Dialog::COLOR_LINES], g->mClipRect);
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
	else if (theId == RunPickDialog_Refresh)
	{
		mApp->RunPickRefreshed();
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
