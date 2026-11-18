#include "RunOptionsDialog.h"
#include "CjkStoneButton.h"
#include "GameButton.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "../Run/RunState.h"
#include "../Online/NetSession.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "graphics/Font.h"
#include "graphics/Graphics.h"
#include "widget/WidgetManager.h"
#include "../ModText.h"
#include <string>

// 行内按钮间距 / 底部两枚的间距（RunModeDialog 的难度行与底行各用一套，这里照抄）。
static const int kGapX = 12;
static const int kBottomGapX = 20;

static const int kRowCounts[RunOptionsDialog::RUN_OPTIONS_ROW_COUNT] = { 4, 3, 2, 2, 3, 3, 3, 3, 5, 3, 2, 2, 2, 2, 2, 2 };
static const int kRowPages[RunOptionsDialog::RUN_OPTIONS_ROW_COUNT] = { 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, -1, -1, 4, 4, 4, 3 };
static const int kRowSlots[RunOptionsDialog::RUN_OPTIONS_ROW_COUNT] = { 0, 1, 2, 0, 0, 1, 2, 0, 1, 2, 0, 0, 0, 1, 2, 1 };

static std::string SectionTitle(int page)
{
	const char* names[] = {
		ModText::Tr("出怪设置", "Zombie settings"),
		ModText::Tr("成长与首领", "Growth and bosses"),
		ModText::Tr("场景与强化", "Scenes and boosts"),
		ModText::Tr("本机操作", "Local controls"),
        ModText::Tr("伤害统计", "Damage statistics")
	};
	return names[page];
}

// 「场景难度」行的槽位号（行序末位）。无尽档不吃阶梯，该行整行不摆（2026-10-14 用户
// 裁决）——mShowLadderRow 为假时它永远不上场（Resize 去屏外，画不着、点不着）。
static const int kRowLadder = 8;
static const int kRowFirstWave = 9;
static const int kFirstWaveUiValues[3] = { RunState::RUN_FIRSTWAVE_VANILLA, RunState::RUN_FIRSTWAVE_STD, RunState::RUN_FIRSTWAVE_LONG };

static int FirstWaveUiIndex(int theValue)
{
	for (int anIndex = 0; anIndex < 3; ++anIndex)
		if (kFirstWaveUiValues[anIndex] == theValue) return anIndex;
	return 1;
}

// 「场景难度」行的按钮顺序与枚举顺序不同：UI 按倍率从小到大摆（用户 2026-10-14 令
// 「做 1 1.2 1.33 1.5 2 五档」），枚举里 RUN_LADDER_STD（= 现状 ×1.33）是 0（sentinel
// 保老档不静默变难度）。value ↔ 槽位 的桥就这一张表：按钮 i 的值 = kLadderUiValues[i]。
static const int kLadderUiValues[5] = {
	RunState::RUN_LADDER_X1, RunState::RUN_LADDER_X12, RunState::RUN_LADDER_STD,
	RunState::RUN_LADDER_X15, RunState::RUN_LADDER_X2
};
static int LadderUiIndex(int theValue)
{
	for (int i = 0; i < 5; i++)
	{
		if (kLadderUiValues[i] == theValue) return i;
	}
	return 2;	// 非法档归「×1.33」槽位（= STD，与 LadderPermilleFor 的兜底同口径）
}

// 小标题居中直绘（RunModeDialog 的难度行同款颜色）。
static void DrawRowCaption(Graphics* g, ModText::Font* theFont, const std::string& theText, int theY, int theWidth)
{
	std::wstring aWide = ModText::WideFromUtf8(theText.c_str());
	ModText::DrawTextWide(g, theFont, (theWidth - ModText::TextWidth(theFont, aWide)) / 2, theY,
		aWide, Color(220, 215, 190), g->mClipRect);
}

// 翻页行中间那句「第 n / m 页」（中英各一版；两处调用：构造量宽、Draw 现画）。
static std::string MakePageText(int thePage)
{
	return ModText::Tr("第 ", "Page ") + std::to_string(thePage + 1) + " / "
		+ std::to_string(RunOptionsDialog::RUN_OPTIONS_PAGE_COUNT) + ModText::Tr(" 页", "");
}

RunOptionsDialog::RunOptionsDialog(LawnApp* theApp, int theScaleSel, int theTempoSel, int theZombotanySel, int theSlideSel, int theBossSel, int theBuffMultSel, int theLadderSel, int theZombieBuffSel, int theSpeedSel, int theFirstWaveSel, bool theEndless, int theEndlessOptions)
	: LawnDialog(theApp, Dialogs::DIALOG_RUN_OPTIONS, true, _S(""), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
	mTitle = theEndless ? ModText::Tr("无尽高级选项", "Endless options") : ModText::Tr("闯关高级选项", "Run options");
    mApp->mDamageStats.LoadSettings();
    mDamageScopeSel = mApp->mDamageStats.WholeRun() ? 1 : 0;
    mDamageTotalSel = mApp->mDamageStats.ShowTotal() ? 1 : 0;
    mDamageBossSel = mApp->mDamageStats.ShowBoss() ? 1 : 0;
    mDamageScopeCaption = ModText::Tr("统计范围（仅本机）", "Statistics scope (local)");
    mDamageTotalCaption = ModText::Tr("总伤害统计（包含BOSS）", "Total damage (includes bosses)");
    mDamageBossCaption = ModText::Tr("BOSS伤害统计（独立显示）", "Boss damage (separate display)");
    mDamageScopeLabels[0] = ModText::Tr("本关", "Level");
    mDamageScopeLabels[1] = ModText::Tr("本轮", "Run");
    for (int index = 0; index < 2; ++index)
        mDamageTotalLabels[index] = mDamageBossLabels[index] = index ? ModText::Tr("开启", "On") : ModText::Tr("关闭", "Off");
    mEndlessOptions=theEndlessOptions;
    mThemeSel=(theEndlessOptions&4)?1:0;mFocusSel=(theEndlessOptions&8)?1:0;
    mThemeCaption=ModText::Tr("阶段主题（每五关轮换）","Stage themes (every 5 levels)");
    mFocusCaption=ModText::Tr("增益候选偏好","Buff candidate preference");
    mThemeLabels[0]=ModText::Tr("关闭","Off");mThemeLabels[1]=ModText::Tr("开启","On");
    mFocusLabels[0]=ModText::Tr("自由随机","Random");mFocusLabels[1]=ModText::Tr("阵容偏好","Lineup");

	// 传入的当前值先钳一遍（与 AlignRunToHost 同纪律）：非法档按 标准/标准/关。
	mScaleSel = theScaleSel;
	mTempoSel = theTempoSel;
	mZombotanySel = theZombotanySel;
	if (mScaleSel < RunState::RUN_SCALE_HALF || mScaleSel > RunState::RUN_SCALE_QUAD) mScaleSel = RunState::RUN_SCALE_STD;
	if (mTempoSel < RunState::RUN_TEMPO_FAST || mTempoSel > RunState::RUN_TEMPO_SLOW) mTempoSel = RunState::RUN_TEMPO_STD;
	if (mZombotanySel != 0 && mZombotanySel != 1) mZombotanySel = 0;
	mSlideSel = theSlideSel;
	if (mSlideSel != 0 && mSlideSel != 1) mSlideSel = 1;	// 非法档归开（同"默认开"口径）
	mBossSel = theEndless ? (theEndlessOptions&3) : (theBossSel & 3);
    mFinaleSel = !theEndless && (theBossSel & RunState::BOSS_FINALE);
    mFinaleCaption = ModText::Tr("僵王终章（追加黑夜屋顶）", "Zomboss finale (night roof)");
    mFinaleLabels[0] = ModText::Tr("关闭", "Off");
    mFinaleLabels[1] = ModText::Tr("开启", "On");
	if (mBossSel < 0 || mBossSel > 2) mBossSel = 0;	// 非法档归关（2026-10-16 起三档）
	mBuffMultSel = theBuffMultSel;
	if (mBuffMultSel < RunState::RUN_BUFFMULT_X1 || mBuffMultSel > RunState::RUN_BUFFMULT_X3) mBuffMultSel = RunState::RUN_BUFFMULT_X1;
	mZombieBuffSel = theZombieBuffSel;
	if (mZombieBuffSel < 0 || mZombieBuffSel > 2) mZombieBuffSel = 0;
	mSpeedSel = theSpeedSel;
	mFirstWaveSel = theFirstWaveSel >= RunState::RUN_FIRSTWAVE_STD && theFirstWaveSel <= RunState::RUN_FIRSTWAVE_LONG ? theFirstWaveSel : RunState::RUN_FIRSTWAVE_STD;
	if (mSpeedSel < 0 || mSpeedSel > 2) mSpeedSel = 0;	// 移速成长：非法档归无（2026-10-16）
	mLadderSel = theLadderSel;
	if (mLadderSel < RunState::RUN_LADDER_STD || mLadderSel > RunState::RUN_LADDER_X2) mLadderSel = RunState::RUN_LADDER_STD;
	mShowLadderRow = !theEndless;	// 无尽档不吃阶梯 → 整行不摆（2026-10-14 用户裁决）

	mScaleCaption = ModText::Tr("全队出怪数量", "Team zombie count");
	mScaleLabels[0] = ModText::Tr("少 x0.5", "Few x0.5");
	mScaleLabels[1] = ModText::Tr("标准 x1", "Standard x1");
	mScaleLabels[2] = ModText::Tr("多 x2", "Many x2");
	mScaleLabels[3] = ModText::Tr("海量 x4", "Swarm x4");

	mTempoCaption = ModText::Tr("波次间隔", "Wave interval");
	mTempoLabels[0] = ModText::Tr("快 x0.6", "Fast x0.6");
	mTempoLabels[1] = ModText::Tr("标准 x1", "Standard x1");
	mTempoLabels[2] = ModText::Tr("慢 x1.6", "Slow x1.6");

	mZombotanyCaption = ModText::Tr("植物僵尸", "ZomBotany");
	mZombotanyLabels[0] = ModText::Tr("关闭", "Off");
	mZombotanyLabels[1] = ModText::Tr("开启", "On");

	mSlideCaption = ModText::Tr("滑动收阳光（本机所有模式）", "Slide-collect sun (all local modes)");
	mSlideLabels[0] = ModText::Tr("关闭", "Off");
	mSlideLabels[1] = ModText::Tr("开启", "On");

	mBossCaption = ModText::Tr("巨型BOSS出场", "Giant boss timing");
	mBossLabels[0] = ModText::Tr("关闭", "Off");
	mBossLabels[1] = ModText::Tr("仅末关", "Final only");
	mBossLabels[2] = ModText::Tr("末关+场景3", "Final + Scene 3");

    if(theEndless) {
        mBossCaption=ModText::Tr("周期 BOSS（胜利额外选一次增益）","Boss cycle (extra buff on victory)");
        mBossLabels[1]=ModText::Tr("每十关","Every 10");mBossLabels[2]=ModText::Tr("每五关","Every 5");
    }

	// 第六行「增益倍选」（2026-10-13 用户令，MOD_BUILD 43）：×1/×2/×3——每屏照旧三选一
	// 拿 1 条，但发放的三选一屏数乘倍（拿到的总数翻倍，代价是连点更多屏）。
	mBuffMultCaption = ModText::Tr("增益选择次数", "Number of buff picks");
	mBuffMultLabels[0] = ModText::Tr("x1", "x1");
	mBuffMultLabels[1] = ModText::Tr("x2", "x2");
	mBuffMultLabels[2] = ModText::Tr("x3", "x3");

	// 第七行「移速成长」（2026-10-16 用户令）：0/×1.03/×1.05——僵尸移速每进一个新场景
	// 复利一档（场景 1 恒 ×1，场景 5 ≈ ×1.12/×1.21）。0 = 无成长。消费在 ZombieInitialize
	//（与迅捷 buff 独立叠乘）。
	mSpeedCaption = ModText::Tr("僵尸移速成长（每新场景）", "Zombie speed growth (per scene)");
    if(theEndless)mSpeedCaption=ModText::Tr("移速成长（每五关，最高1.5倍）","Speed per 5 levels (cap x1.5)");
	mSpeedLabels[0] = ModText::Tr("关闭", "Off");
	mSpeedLabels[1] = ModText::Tr("x1.03", "x1.03");
	mSpeedLabels[2] = ModText::Tr("x1.05", "x1.05");

	// 第八行「僵尸 buff」（2026-10-14 用户令，MOD_BUILD 45）：0/1/2 = 每进一个新场景给
	// 僵尸叠几条强化（全队投票选具体哪几条；无尽档五关一选）。0 = 整条机制不开。
	// MOD_BUILD 56 起档 2 = 同一场景连开两屏、每屏三选一取 1 条（两屏制，见 LawnApp）。
	mZombieBuffCaption = theEndless?ModText::Tr("僵尸强化（每五关）","Zombie boosts (every 5 levels)"):ModText::Tr("僵尸强化（每新场景）", "Zombie boosts (per new scene)");
	mZombieBuffLabels[0] = ModText::Tr("关闭", "Off");
	mZombieBuffLabels[1] = ModText::Tr("1 条", "1 buff");
	mZombieBuffLabels[2] = ModText::Tr("2 条", "2 buffs");

	// 第九行「场景难度」（2026-10-14 用户令，MOD_BUILD 45）：阶梯倍率乘在僵尸血量与
	// 出怪点数上；×1.33 = 现状（老档/默认；枚举值是 sentinel 0）。无尽档不吃阶梯，
	// 该行整行不摆（mShowLadderRow，构造参数 theEndless）。
	mLadderCaption = ModText::Tr("场景难度阶梯（血量+出怪）", "Scene difficulty ladder (HP+spawns)");
	mLadderLabels[0] = ModText::Tr("x1", "x1");
	mLadderLabels[1] = ModText::Tr("x1.2", "x1.2");
	mLadderLabels[2] = ModText::Tr("x1.33", "x1.33");
	mLadderLabels[3] = ModText::Tr("x1.5", "x1.5");
	mLadderLabels[4] = ModText::Tr("x2", "x2");

	mFirstWaveCaption = ModText::Tr("首波等待时间", "First wave delay");
	mFirstWaveLabels[0] = ModText::Tr("18 s", "18 sec");
	mFirstWaveLabels[1] = ModText::Tr("30 s", "30 sec");
	mFirstWaveLabels[2] = ModText::Tr("60 s", "60 sec");

	mTitleY = 0;
	for (int i = 0; i < RUN_OPTIONS_ROW_COUNT; i++) mCaptionYs[i] = 0;

	// 各行按钮等宽（取最长标签量的）；石门贴图平铺对宽度有整段要求（见 CjkStoneButtonWidth）。
	// 量宽用按钮实际画标签的那档字号（CjkPointSize(CJK_BUTTON_LABEL_PX)），量画同一份。
	const std::string* aLabelSets[RUN_OPTIONS_ROW_COUNT] = {
		mScaleLabels, mTempoLabels, mZombotanyLabels, mSlideLabels, mBossLabels, mBuffMultLabels,
		mSpeedLabels, mZombieBuffLabels, mLadderLabels, mFirstWaveLabels, mThemeLabels, mFocusLabels,
        mDamageScopeLabels, mDamageTotalLabels, mDamageBossLabels, mFinaleLabels
	};
	ButtonWidget** aSlots[RUN_OPTIONS_ROW_COUNT] = {
		mScaleButtons, mTempoButtons, mZombotanyButtons, mSlideButtons, mBossButtons, mBuffMultButtons,
		mSpeedButtons, mZombieBuffButtons, mLadderButtons, mFirstWaveButtons, mThemeButtons, mFocusButtons,
        mDamageScopeButtons, mDamageTotalButtons, mDamageBossButtons, mFinaleButtons
	};
	int* aWidthSets[RUN_OPTIONS_ROW_COUNT] = {
		mScaleWidths, mTempoWidths, mZombotanyWidths, mSlideWidths, mBossWidths, mBuffMultWidths,
		mSpeedWidths, mZombieBuffWidths, mLadderWidths, mFirstWaveWidths, mThemeWidths, mFocusWidths,
        mDamageScopeWidths, mDamageTotalWidths, mDamageBossWidths, mFinaleWidths
	};
	static const int kRowIdBase[RUN_OPTIONS_ROW_COUNT] = {
		RunOptionsDialog_Scale0, RunOptionsDialog_Tempo0, RunOptionsDialog_Zombotany0,
		RunOptionsDialog_Slide0, RunOptionsDialog_Boss0, RunOptionsDialog_BuffMult0,
		RunOptionsDialog_Speed0, RunOptionsDialog_ZombieBuff0, RunOptionsDialog_Ladder0, RunOptionsDialog_FirstWave0, RunOptionsDialog_Theme0, RunOptionsDialog_Focus0,
        RunOptionsDialog_DamageScope0, RunOptionsDialog_DamageTotal0, RunOptionsDialog_DamageBoss0, RunOptionsDialog_Finale0
	};
	// 每行"选中槽位号"：多数行 = 值本身（枚举顺序 = 按钮顺序）；「场景难度」行例外
	// （UI 顺序与枚举顺序不同，走 kLadderUiValues 映射）。
	const int aSelIndex[RUN_OPTIONS_ROW_COUNT] = {
		mScaleSel, mTempoSel, mZombotanySel, mSlideSel, mBossSel, mBuffMultSel,
		mSpeedSel, mZombieBuffSel, LadderUiIndex(mLadderSel), FirstWaveUiIndex(mFirstWaveSel), mThemeSel, mFocusSel,
        mDamageScopeSel, mDamageTotalSel, mDamageBossSel, mFinaleSel
	};
	for (int aRow = 0; aRow < RUN_OPTIONS_ROW_COUNT; aRow++)
	{
		int aLabelMax = 0;
		for (int i = 0; i < kRowCounts[aRow]; i++)
		{
			int aWidth = CjkStoneButtonLabelWidth(aLabelSets[aRow][i]);
			if (aWidth > aLabelMax) aLabelMax = aWidth;
		}
		int aButtonWidth = CjkStoneButtonWidth(aLabelMax + 26, true);
		for (int i = 0; i < kRowCounts[aRow]; i++)
		{
			aWidthSets[aRow][i] = aButtonWidth;

			CjkStoneButton* aButton = new CjkStoneButton(kRowIdBase[aRow] + i, this);
			aButton->SetLabel(aLabelSets[aRow][i]);
			aButton->mHasAlpha = true;
			aButton->mHasTransparencies = true;
			aButton->mInverted = (i == aSelIndex[aRow]);
			aSlots[aRow][i] = aButton;
		}
	}

	mOkButton = new CjkStoneButton(RunOptionsDialog_OK, this);
	mOkButton->SetLabel(ModText::Tr("确定", "OK"));
	mOkButton->mHasAlpha = true;
	mOkButton->mHasTransparencies = true;
	mOkWidth = CjkStoneButtonWidth(CjkStoneButtonLabelWidth(mOkButton->mLabel) + 32, true);

	mCancelButton = new CjkStoneButton(Dialog::ID_NO, this);
	mCancelButton->SetLabel(ModText::Tr("取消", "Cancel"));
	mCancelButton->mHasAlpha = true;
	mCancelButton->mHasTransparencies = true;
	mCancelWidth = CjkStoneButtonWidth(CjkStoneButtonLabelWidth(mCancelButton->mLabel) + 32, true);

	// 翻页行两枚（上一页 / 下一页，环绕翻页）+ 中间的「第 n / m 页」文字（宽度先量好，摆位留位）。
	mPrevButton = new CjkStoneButton(RunOptionsDialog_PrevPage, this);
	mPrevButton->SetLabel(ModText::Tr("上一页", "Prev"));
	mPrevButton->mHasAlpha = true;
	mPrevButton->mHasTransparencies = true;
	mPrevWidth = CjkStoneButtonWidth(CjkStoneButtonLabelWidth(mPrevButton->mLabel) + 26, true);

	mNextButton = new CjkStoneButton(RunOptionsDialog_NextPage, this);
	mNextButton->SetLabel(ModText::Tr("下一页", "Next"));
	mNextButton->mHasAlpha = true;
	mNextButton->mHasTransparencies = true;
	mNextWidth = CjkStoneButtonWidth(CjkStoneButtonLabelWidth(mNextButton->mLabel) + 26, true);

	mPageTextW = ModText::TextWidth(ModText::GetFont(CjkPointSize(20), false), ModText::WideFromUtf8(MakePageText(0).c_str()));
	mPageTextX = mPageTextY = 0;

	mPage = 0;

	mTallBottom = true;
	mVerticalCenterText = false;

	// 版心（翻页批后按"一页"算）：宽 = 标题与八行按钮总宽的最大者；高 = 标题 + 间隔 + 三行
	// （小标题 + 按钮）+ 翻页行 + 确定行。CalcSize 会按对话框贴图再取整/加高，多出来的
	// 空隙由 PlaceRows 里"首行贴顶、翻页/确定贴底"吸收。
	ModText::Font* aTitleFont = ModText::GetFont(CjkPointSize(28), true);
	ModText::Font* aCaptionFont = ModText::GetFont(CjkPointSize(20), false);
	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;
	int anExtraX = ModText::TextWidth(aTitleFont, ModText::WideFromUtf8(mTitle.c_str())) + 80;
	for (int aRow = 0; aRow < RUN_OPTIONS_ROW_COUNT; aRow++)
	{
		int aRowWidth = 0;
		for (int i = 0; i < kRowCounts[aRow]; i++)
		{
			if (i > 0) aRowWidth += kGapX;
			aRowWidth += aWidthSets[aRow][i];
		}
		if (aRowWidth > anExtraX) anExtraX = aRowWidth;
	}
	int aRowHeight = ModText::LineHeight(aCaptionFont) + 6 + aButtonHeight + 8;
	int anExtraY = ModText::LineHeight(aTitleFont) + 18		// 标题 + 与首行的间隔
		+ aRowHeight * RUN_OPTIONS_ROWS_PER_PAGE			// 一页三行
		+ aButtonHeight + 12								// 翻页行 + 间隔
		+ aButtonHeight + 18;								// 确定行 + 与上一行的间隔

	CalcSize(anExtraX, anExtraY);
	// 2026-10-13 实机实测：CalcSize 的高度按对话框中段贴图块向上取整——算式上的微调会被
	// 取整吃掉（实测总高 614，超 800×600 屏高上下各 7px）。这里按「合法高度 = 上贴图 +
	// 下贴图 + 隔头 + k×中贴图块高」从取整结果往下逐块收，收到屏高以内（留 8px 边）。
	{
		int aBottomHeight = (mTallBottom ? IMAGE_DIALOG_BIGBOTTOMLEFT : IMAGE_DIALOG_BOTTOMLEFT)->mHeight;
		int aImageHeight = IMAGE_DIALOG_TOPLEFT->mHeight + aBottomHeight + DIALOG_HEADER_OFFSET;
		int aCenterHeight = IMAGE_DIALOG_CENTERLEFT->mHeight;
		while (mHeight > mApp->mHeight - 8 && mHeight - aCenterHeight >= aImageHeight)
			mHeight -= aCenterHeight;
	}
	mApp->CenterDialog(this, mWidth, mHeight);
	mClip = false;
}

RunOptionsDialog::~RunOptionsDialog()
{
	ButtonWidget** aSlots[RUN_OPTIONS_ROW_COUNT] = {
		mScaleButtons, mTempoButtons, mZombotanyButtons, mSlideButtons, mBossButtons, mBuffMultButtons,
		mSpeedButtons, mZombieBuffButtons, mLadderButtons, mFirstWaveButtons, mThemeButtons, mFocusButtons,
        mDamageScopeButtons, mDamageTotalButtons, mDamageBossButtons, mFinaleButtons
	};
	for (int aRow = 0; aRow < RUN_OPTIONS_ROW_COUNT; aRow++)
	{
		for (int i = 0; i < kRowCounts[aRow]; i++) delete aSlots[aRow][i];
	}
	delete mOkButton;
	delete mCancelButton;
	delete mPrevButton;
	delete mNextButton;
}

// 一次摆好全部控件（Resize 与翻页共用）。一页三行：行槽位固定（页只决定谁上场）——
// 当前页的行摆进槽位，其余整行挪去负坐标（画不着、点不着；选中态留在各按钮的 mInverted 上，
// 翻回来原样在）。翻页行与确定行都贴底算，中部的多余空隙吸收在末行与翻页行之间。
// 无尽档的「场景难度」行永远不上场（mShowLadderRow = 假，同"挪去屏外"处理）。
void RunOptionsDialog::PlaceRows()
{
	ModText::Font* aTitleFont = ModText::GetFont(CjkPointSize(28), true);
	ModText::Font* aCaptionFont = ModText::GetFont(CjkPointSize(20), false);
	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;

	ButtonWidget** aSlots[RUN_OPTIONS_ROW_COUNT] = {
		mScaleButtons, mTempoButtons, mZombotanyButtons, mSlideButtons, mBossButtons, mBuffMultButtons,
		mSpeedButtons, mZombieBuffButtons, mLadderButtons, mFirstWaveButtons, mThemeButtons, mFocusButtons,
        mDamageScopeButtons, mDamageTotalButtons, mDamageBossButtons, mFinaleButtons
	};
	int* aWidthSets[RUN_OPTIONS_ROW_COUNT] = {
		mScaleWidths, mTempoWidths, mZombotanyWidths, mSlideWidths, mBossWidths, mBuffMultWidths,
		mSpeedWidths, mZombieBuffWidths, mLadderWidths, mFirstWaveWidths, mThemeWidths, mFocusWidths,
        mDamageScopeWidths, mDamageTotalWidths, mDamageBossWidths, mFinaleWidths
	};

	int aTitleTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	mTitleY = aTitleTop;						// ModText 顶对齐：存的直接是顶

	int aRowTop = aTitleTop + ModText::LineHeight(aTitleFont) + 18;
	int aRowHeight = ModText::LineHeight(aCaptionFont) + 6 + aButtonHeight + 8;
	for (int aRow = 0; aRow < RUN_OPTIONS_ROW_COUNT; aRow++)
	{
		int aRowY = aRowTop + RowSlot(aRow) * aRowHeight;
		bool aOnPage = (RowPage(aRow) == mPage)
			&& RowPage(aRow)>=0;
		mCaptionYs[aRow] = aOnPage ? aRowY : -2000;
		int aButtonY = aOnPage ? aRowY + ModText::LineHeight(aCaptionFont) + 6 : -2000;

		int aRowWidth = 0;
		for (int i = 0; i < kRowCounts[aRow]; i++)
		{
			if (i > 0) aRowWidth += kGapX;
			aRowWidth += aWidthSets[aRow][i];
		}
		int aX = (mWidth - aRowWidth) / 2;
		for (int i = 0; i < kRowCounts[aRow]; i++)
		{
			aSlots[aRow][i]->Resize(aX, aButtonY, aWidthSets[aRow][i], aButtonHeight);
			aX += aWidthSets[aRow][i] + kGapX;
		}
	}

	// 确定行贴底，两枚并排居中（原算式）；翻页行贴在确定行上面一行。
	int aBottomY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aBottomY += 5;
	int aTotalW = mOkWidth + mCancelWidth + kBottomGapX;
	int aLeft = (mWidth - aTotalW) / 2;
	mOkButton->Resize(aLeft, aBottomY, mOkWidth, aButtonHeight);
	mCancelButton->Resize(aLeft + mOkWidth + kBottomGapX, aBottomY, mCancelWidth, aButtonHeight);

	// 翻页行：上一页 [第 n / m 页] 下一页，三件套横排居中（页码文字是 Draw 画的，这里只留位）。
	int aNavY = aBottomY - 12 - aButtonHeight;
	int aNavTotalW = mPrevWidth + kGapX + mPageTextW + kGapX + mNextWidth;
	int aNavLeft = (mWidth - aNavTotalW) / 2;
	mPrevButton->Resize(aNavLeft, aNavY, mPrevWidth, aButtonHeight);
	mPageTextX = aNavLeft + mPrevWidth + kGapX;
	mPageTextY = aNavY + (aButtonHeight - ModText::LineHeight(aCaptionFont)) / 2;
	mNextButton->Resize(mPageTextX + mPageTextW + kGapX, aNavY, mNextWidth, aButtonHeight);
}

void RunOptionsDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);
	PlaceRows();
}

void RunOptionsDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	ButtonWidget** aSlots[RUN_OPTIONS_ROW_COUNT] = {
		mScaleButtons, mTempoButtons, mZombotanyButtons, mSlideButtons, mBossButtons, mBuffMultButtons,
		mSpeedButtons, mZombieBuffButtons, mLadderButtons, mFirstWaveButtons, mThemeButtons, mFocusButtons,
        mDamageScopeButtons, mDamageTotalButtons, mDamageBossButtons, mFinaleButtons
	};
	for (int aRow = 0; aRow < RUN_OPTIONS_ROW_COUNT; aRow++)
	{
		for (int i = 0; i < kRowCounts[aRow]; i++) AddWidget(aSlots[aRow][i]);
	}
	AddWidget(mOkButton);
	AddWidget(mCancelButton);
	AddWidget(mPrevButton);
	AddWidget(mNextButton);
}

void RunOptionsDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	ButtonWidget** aSlots[RUN_OPTIONS_ROW_COUNT] = {
		mScaleButtons, mTempoButtons, mZombotanyButtons, mSlideButtons, mBossButtons, mBuffMultButtons,
		mSpeedButtons, mZombieBuffButtons, mLadderButtons, mFirstWaveButtons, mThemeButtons, mFocusButtons,
        mDamageScopeButtons, mDamageTotalButtons, mDamageBossButtons, mFinaleButtons
	};
	for (int aRow = 0; aRow < RUN_OPTIONS_ROW_COUNT; aRow++)
	{
		for (int i = 0; i < kRowCounts[aRow]; i++) RemoveWidget(aSlots[aRow][i]);
	}
	RemoveWidget(mOkButton);
	RemoveWidget(mCancelButton);
	RemoveWidget(mPrevButton);
	RemoveWidget(mNextButton);
}

void RunOptionsDialog::Draw(Graphics* g)
{
	LawnDialog::Draw(g);   // 底板。header / lines 都是空的，只出框。

	if (!mTitle.empty())
	{
		ModText::Font* aTitleFont = ModText::GetFont(CjkPointSize(28), true);
		const char* endlessTitles[]={ModText::Tr("无尽 · 出怪设置","Endless / Zombies"),ModText::Tr("无尽 · 阶段挑战","Endless / Challenges"),ModText::Tr("无尽 · 阵容与增益","Endless / Lineup"),ModText::Tr("无尽 · 准备与操作","Endless / Preparation"),ModText::Tr("伤害统计", "Damage statistics")};
        std::wstring aTitle = ModText::WideFromUtf8((mShowLadderRow?SectionTitle(mPage):std::string(endlessTitles[mPage])).c_str());
		ModText::DrawTextWide(g, aTitleFont,
			(mWidth - ModText::TextWidth(aTitleFont, aTitle)) / 2, mTitleY,
			aTitle, mColors[Dialog::COLOR_HEADER], g->mClipRect);
	}

	// 当前页三行的小标题（各行的按钮自己画自己，走控件那套；非当前页的行已被挪去屏外）。
	ModText::Font* aCaptionFont = ModText::GetFont(CjkPointSize(20), false);
	const std::string* aCaptions[RUN_OPTIONS_ROW_COUNT] = {
		&mScaleCaption, &mTempoCaption, &mZombotanyCaption, &mSlideCaption, &mBossCaption, &mBuffMultCaption,
		&mSpeedCaption, &mZombieBuffCaption, &mLadderCaption, &mFirstWaveCaption, &mThemeCaption, &mFocusCaption,
        &mDamageScopeCaption, &mDamageTotalCaption, &mDamageBossCaption, &mFinaleCaption
	};
	for (int aRow = 0; aRow < RUN_OPTIONS_ROW_COUNT; aRow++)
	{
		if (RowPage(aRow) != mPage) continue;
		if (RowPage(aRow)<0) continue;	// 无尽档不摆（同 PlaceRows）
		DrawRowCaption(g, aCaptionFont, *aCaptions[aRow], mCaptionYs[aRow], mWidth);
	}

	// 翻页行中间的「第 n / m 页」。
	std::wstring aPage = ModText::WideFromUtf8(MakePageText(mPage).c_str());
	ModText::DrawTextWide(g, aCaptionFont, mPageTextX, mPageTextY,
		aPage, Color(220, 215, 190), g->mClipRect);
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
    if (theId == RunOptionsDialog_Finale0 || theId == RunOptionsDialog_Finale0 + 1) {
        mFinaleSel = theId - RunOptionsDialog_Finale0;
        for (int i = 0; i < 2; ++i) mFinaleButtons[i]->mInverted = (i == mFinaleSel);
        return;
    }
    const int damageBases[] = {RunOptionsDialog_DamageScope0, RunOptionsDialog_DamageTotal0, RunOptionsDialog_DamageBoss0};
    int* damageSelections[] = {&mDamageScopeSel, &mDamageTotalSel, &mDamageBossSel};
    ButtonWidget** damageButtons[] = {mDamageScopeButtons, mDamageTotalButtons, mDamageBossButtons};
    for (int row = 0; row < 3; ++row)
        if (theId >= damageBases[row] && theId < damageBases[row] + 2)
        {
            *damageSelections[row] = theId - damageBases[row];
            for (int index = 0; index < 2; ++index) damageButtons[row][index]->mInverted = index == *damageSelections[row];
            return;
        }
    if(theId>=RunOptionsDialog_Theme0 && theId<RunOptionsDialog_Theme0+2) {
        mThemeSel=theId-RunOptionsDialog_Theme0;
        mEndlessOptions=(mEndlessOptions&~4)|(mThemeSel?4:0);
        for(int i=0;i<2;++i)mThemeButtons[i]->mInverted=i==mThemeSel;
        return;
    }
    if(theId>=RunOptionsDialog_Focus0 && theId<RunOptionsDialog_Focus0+2) {
        mFocusSel=theId-RunOptionsDialog_Focus0;
        mEndlessOptions=(mEndlessOptions&~8)|(mFocusSel?8:0);
        for(int i=0;i<2;++i)mFocusButtons[i]->mInverted=i==mFocusSel;
        return;
    }

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
	if (theId >= RunOptionsDialog_Boss0 && theId <= RunOptionsDialog_Boss0 + 2)
	{
		mBossSel = theId - RunOptionsDialog_Boss0;
        if(!mShowLadderRow)mEndlessOptions=(mEndlessOptions&~3)|mBossSel;
		for (int i = 0; i < 3; i++) mBossButtons[i]->mInverted = (i == mBossSel);
		return;
	}
	if (theId >= RunOptionsDialog_BuffMult0 && theId <= RunOptionsDialog_BuffMult0 + 2)
	{
		mBuffMultSel = theId - RunOptionsDialog_BuffMult0;
		for (int i = 0; i < 3; i++) mBuffMultButtons[i]->mInverted = (i == mBuffMultSel);
		return;
	}
	if (theId >= RunOptionsDialog_Speed0 && theId <= RunOptionsDialog_Speed0 + 2)
	{
		mSpeedSel = theId - RunOptionsDialog_Speed0;
		for (int i = 0; i < 3; i++) mSpeedButtons[i]->mInverted = (i == mSpeedSel);
		return;
	}
	if (theId >= RunOptionsDialog_FirstWave0 && theId <= RunOptionsDialog_FirstWave0 + 2)
	{
		mFirstWaveSel = kFirstWaveUiValues[theId - RunOptionsDialog_FirstWave0];
		for (int anIndex = 0; anIndex < 3; ++anIndex)
			mFirstWaveButtons[anIndex]->mInverted = kFirstWaveUiValues[anIndex] == mFirstWaveSel;
		return;
	}
	if (theId >= RunOptionsDialog_ZombieBuff0 && theId <= RunOptionsDialog_ZombieBuff0 + 2)
	{
		mZombieBuffSel = theId - RunOptionsDialog_ZombieBuff0;
		for (int i = 0; i < 3; i++) mZombieBuffButtons[i]->mInverted = (i == mZombieBuffSel);
		return;
	}
	// 「场景难度」：按钮槽位 ≠ 枚举值（UI 按倍率从小到大，枚举 STD=0 是第三枚），
	// 经 kLadderUiValues 换算；按下态也按"值相等"刷新。
	if (theId >= RunOptionsDialog_Ladder0 && theId <= RunOptionsDialog_Ladder0 + 4)
	{
		mLadderSel = kLadderUiValues[theId - RunOptionsDialog_Ladder0];
		for (int i = 0; i < 5; i++) mLadderButtons[i]->mInverted = (kLadderUiValues[i] == mLadderSel);
		return;
	}
	// 翻页（2026-10-13）：三页环绕翻——翻完重摆一次（当前页行上场、其余挪去屏外）。
	if (theId == RunOptionsDialog_PrevPage || theId == RunOptionsDialog_NextPage)
	{
		if (theId == RunOptionsDialog_PrevPage)
			mPage = (mPage + RUN_OPTIONS_PAGE_COUNT - 1) % RUN_OPTIONS_PAGE_COUNT;
		else
			mPage = (mPage + 1) % RUN_OPTIONS_PAGE_COUNT;
		PlaceRows();
		return;
	}
	if (theId == RunOptionsDialog_OK)
	{
        if (!mApp->mDamageStats.SaveSettings(mDamageScopeSel | (mDamageTotalSel << 1) | (mDamageBossSel << 2)))
        {
            mApp->LawnMessageBox(Dialogs::DIALOG_MESSAGE, ModText::Tr("保存失败", "Save failed"),
                ModText::Tr("统计设置未保存，请检查可用存储。", "Statistics settings were not saved. Check storage."), "OK", "", Dialog::BUTTONS_FOOTER);
            return;
        }
		mResult = RunOptionsDialog_OK;	// 确定 = 收结果（WaitForResult 自己收摊）
		return;
	}
	if (theId == Dialog::ID_NO)
	{
		mResult = Dialog::ID_NO;		// 取消 = 关弹窗不改任何值
		return;
	}
}

int RunOptionsDialog::RowPage(int row) const {
    if (row == 15 && (mApp->mOnlineSession && !mApp->mOnlineSession->SupportsBossFinale())) return -1;
    if(mShowLadderRow)return kRowPages[row];
    static const int pages[]={0,0,0,3,1,2,1,1,-1,3,2,2,4,4,4,-1};return pages[row];
}
int RunOptionsDialog::RowSlot(int row) const {
    if(mShowLadderRow)return kRowSlots[row];
    static const int slots[]={0,1,2,1,0,0,1,2,0,0,1,2,0,1,2,1};return slots[row];
}
