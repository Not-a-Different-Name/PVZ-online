#include "RunModeDialog.h"
#include "CjkStoneButton.h"
#include "RunOptionsDialog.h"
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

// 石材按钮（平铺宽度取整 CjkStoneButtonWidth + 宽字符标签画法）：与联机弹窗/联机面板
// 共用一份，见 CjkStoneButton.h/.cpp——原先这里内联的一份已并入。

// （内联的 CjkStoneButton 类已提取到 CjkStoneButton.h，联机弹窗/面板/本弹窗共用一份。）

RunModeDialog::RunModeDialog(LawnApp* theApp, bool theShowDiff, int theEndlessScene) : LawnDialog(
	theApp, Dialogs::DIALOG_ONLINE_START, true, _S(""), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
	// 卡片边框和缩略图标在 ChallengeScreen 的延迟资源组里，不加载就是空指针
	TodLoadResources("DelayLoad_ChallengeScreen");

	// 无尽变体（MOD_BUILD 37）：theEndlessScene >= 0 才成立；越界值当普通页处理。
	mEndlessScene = (theEndlessScene >= 0 && theEndlessScene < RunState::RUN_SCENE_COUNT) ? theEndlessScene : -1;
	bool aEndless = mEndlessScene >= 0;

	if (aEndless)
	{
		mTitle = ModText::Tr("无尽设置", "Endless Settings");
		static const char* kSceneNamesZh[RunState::RUN_SCENE_COUNT] = { "白天", "夜晚", "泳池", "浓雾", "屋顶" };
		static const char* kSceneNamesEn[RunState::RUN_SCENE_COUNT] = { "Day", "Night", "Pool", "Fog", "Roof" };
		mSceneLine = ModText::Tr("环境：", "Scene: ");
		mSceneLine += ModText::Tr(kSceneNamesZh[mEndlessScene], kSceneNamesEn[mEndlessScene]);
	}
	else
	{
		mTitle = ModText::Tr("选择闯关模式", "Choose a Run Mode");
	}
	mCardNames[0] = ModText::Tr("完整版", "Full");
	mCardNames[1] = ModText::Tr("普通版", "Normal");
	mCardNames[2] = ModText::Tr("快速版", "Quick");
	// 卡下说明（几关/奖励倍率）2026-10-04 按用户要求整行删去：英文档那几句本来就
	// 横着相撞，删干净反而清爽（档位含义靠卡名 + 文档）。
	mTitleY = 0;
	mSceneY = 0;

	for (int i = 0; i < 3; i++)
	{
		mCardButtons[i] = new ButtonWidget(RunModeDialog_Mode0 + i, this);
		mCardButtons[i]->mDoFinger = true;
		mCardButtons[i]->mFrameNoDraw = true;	// 画在 Dialog::Draw 里（照 ChallengeScreen）；无尽变体整行不摆
	}

	// 出怪难度行（MOD_BUILD 27）：联机主机才摆（单机档位恒为标准，摆了也是死控件）。
	// 选中 = mInverted（CjkStoneButton::Draw 里 XOR 成按下态贴图）；点了只换选中、不关弹窗。
	// 无尽变体（MOD_BUILD 37）例外：单机也摆——无尽局的难度/规模/节奏同样吃这一行。
	mShowDiff = theShowDiff || aEndless;
	mDiffSel = RunState::RUN_DIFF_STD;
	mScaleSel = RunState::RUN_SCALE_STD;
	mTempoSel = RunState::RUN_TEMPO_STD;
	mZombotanySel = 0;
	mOptionsButton = nullptr;
	mOptionsWidth = 0;
	mDiffCaptionY = 0;
	for (int i = 0; i < 3; i++) mDiffButtons[i] = nullptr;
	if (mShowDiff)
	{
		mDiffCaption = ModText::Tr("出怪难度（全队倍率）", "Zombie difficulty (team multiplier)");
		mDiffLabels[0] = ModText::Tr("轻松 ×0.5", "Easy ×0.5");
		mDiffLabels[1] = ModText::Tr("标准 ×1", "Standard ×1");
		mDiffLabels[2] = ModText::Tr("高压 ×2.0", "High ×2.0");

		// 三枚等宽（取最长标签量的），石门贴图平铺对宽度有整段要求（见 CjkStoneButtonWidth）。
		// 量宽用按钮实际画标签的那档字号（CjkPointSize(CJK_BUTTON_LABEL_PX)），量画同一份。
		ModText::Font* aDiffFont = ModText::GetFont(CjkPointSize(CJK_BUTTON_LABEL_PX), false);
		int aLabelMax = 0;
		for (int i = 0; i < 3; i++)
		{
			int aWidth = ModText::TextWidth(aDiffFont, ModText::WideFromUtf8(mDiffLabels[i].c_str()));
			if (aWidth > aLabelMax) aLabelMax = aWidth;
		}
		int aDiffWidth = CjkStoneButtonWidth(aLabelMax + 26, true);
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

		// 「高级选项…」（MOD_BUILD 35）：与取消并排贴底，点开 RunOptionsDialog 回填三值。
		CjkStoneButton* aOptions = new CjkStoneButton(RunModeDialog_Options, this);
		aOptions->SetLabel(ModText::Tr("高级选项…", "Advanced Options..."));
		aOptions->mHasAlpha = true;
		aOptions->mHasTransparencies = true;
		mOptionsButton = aOptions;
		mOptionsWidth = CjkStoneButtonWidth(ModText::TextWidth(ModText::GetFont(CjkPointSize(CJK_BUTTON_LABEL_PX), false),
			ModText::WideFromUtf8(aOptions->mLabel.c_str())) + 32, true);
	}

	mCancelButton = new CjkStoneButton(Dialog::ID_NO, this);
	mCancelButton->SetLabel(ModText::Tr("取消", "Cancel"));
	mCancelButton->mHasAlpha = true;
	mCancelButton->mHasTransparencies = true;
	mCancelWidth = CjkStoneButtonWidth(ModText::TextWidth(ModText::GetFont(CjkPointSize(CJK_BUTTON_LABEL_PX), false),
		ModText::WideFromUtf8(mCancelButton->mLabel.c_str())) + 32, true);

	// 无尽变体（MOD_BUILD 37）：底行多一枚「开始」——卡片行撤了，结果 id 换成它；
	// 取消照旧（等待必须有出路）。
	mStartButton = nullptr;
	mStartWidth = 0;
	if (aEndless)
	{
		CjkStoneButton* aStart = new CjkStoneButton(RunModeDialog_Start, this);
		aStart->SetLabel(ModText::Tr("开始", "Start"));
		aStart->mHasAlpha = true;
		aStart->mHasTransparencies = true;
		mStartButton = aStart;
		mStartWidth = CjkStoneButtonWidth(ModText::TextWidth(ModText::GetFont(CjkPointSize(CJK_BUTTON_LABEL_PX), false),
			ModText::WideFromUtf8(aStart->mLabel.c_str())) + 32, true);
	}

	mTallBottom = true;
	mVerticalCenterText = false;

	// 版心：宽 = 三张一行的宽度（边框两侧各探出几像素，留 16 兜住）；高 = 标题 + 间隔
	// + 卡片（连边框）+ （难度行）+ 按钮。CalcSize 会按对话框贴图再取整/加高，多出来的
	// 空隙由 Resize 里"卡片贴顶、按钮贴底"吸收。
	ModText::Font* aTitleFont = ModText::GetFont(16, true);
	int anExtraX = ModText::TextWidth(aTitleFont, ModText::WideFromUtf8(mTitle.c_str())) + 80;
	if (aEndless)
	{
		// 无尽变体没有卡片行（标题/环境名都比卡片行窄）：宽度改按最宽的一条横排兜底
		// ——底行（高级选项… + 开始 + 取消）与难度行（三枚等宽）里取大的。
		int aBottomW = mOptionsWidth + mStartWidth + mCancelWidth + 20 * 2;
		int aDiffW = mDiffWidths[0] + mDiffWidths[1] + mDiffWidths[2] + 20 * 2;
		int aMinW = (aBottomW > aDiffW ? aBottomW : aDiffW) + 16;
		if (aMinW > anExtraX) anExtraX = aMinW;
	}
	else
	{
		int aRowWidth = CARD_PITCH_X * 2 + CARD_W + 16;
		if (aRowWidth > anExtraX) anExtraX = aRowWidth;
	}
	int anExtraY = ModText::LineHeight(aTitleFont) + 18	// 标题 + 与下一行的间隔
		+ (aEndless ? ModText::LineHeight(ModText::GetFont(14, true)) + 14 : CARD_H + 14)
		+ IMAGE_BUTTON_LEFT->mHeight + 18;			// 按钮行 + 与上一行的间隔
	if (mShowDiff)
	{
		// 难度行：卡片之下一行——小标题 + 间隔 + 按钮
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
	delete mStartButton;
	delete mOptionsButton;
}

void RunModeDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);

	ModText::Font* aTitleFont = ModText::GetFont(16, true);

	// 标题贴顶；三张卡片横排在标题下面（一行整体在版心里左右居中）。
	// 无尽变体（MOD_BUILD 37）：卡片整行撤掉，环境名一行顶在卡片原位。
	int aTitleTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	mTitleY = aTitleTop;						// ModText 顶对齐：存的直接是顶
	int aCardsY = aTitleTop + ModText::LineHeight(aTitleFont) + 18;
	int aRowH;
	if (mEndlessScene < 0)
	{
		aRowH = CARD_H;
		int aRowWidth = CARD_PITCH_X * 2 + CARD_W;
		int aStartX = (mWidth - aRowWidth) / 2;
		for (int i = 0; i < 3; i++)
		{
			mCardButtons[i]->Resize(aStartX + i * CARD_PITCH_X, aCardsY, CARD_W, CARD_H);
		}
	}
	else
	{
		aRowH = ModText::LineHeight(ModText::GetFont(14, true));
		mSceneY = aCardsY;
	}

	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;

	// 出怪难度行：行区之下、取消之上，三枚等宽横排居中
	if (mShowDiff)
	{
		ModText::Font* aCaptionFont = ModText::GetFont(12, false);
		int aCaptionTop = aCardsY + aRowH + 34;
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

	// 底部按钮贴底（OnlineStartDialog 同一条算式）：「高级选项…」+ 取消并排居中（主机），
	// 或只有取消居中（单机）；无尽变体三枚并排：「高级选项…」+「开始」+ 取消。
	int aButtonY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aButtonY += 5;
	static const int aBottomGapX = 20;
	if (mStartButton)
	{
		int aTotalW = mOptionsWidth + mStartWidth + mCancelWidth + aBottomGapX * 2;
		int aLeft = (mWidth - aTotalW) / 2;
		mOptionsButton->Resize(aLeft, aButtonY, mOptionsWidth, aButtonHeight);
		mStartButton->Resize(aLeft + mOptionsWidth + aBottomGapX, aButtonY, mStartWidth, aButtonHeight);
		mCancelButton->Resize(aLeft + mOptionsWidth + mStartWidth + aBottomGapX * 2, aButtonY, mCancelWidth, aButtonHeight);
	}
	else if (mOptionsButton)
	{
		int aTotalW = mOptionsWidth + mCancelWidth + aBottomGapX;
		int aLeft = (mWidth - aTotalW) / 2;
		mOptionsButton->Resize(aLeft, aButtonY, mOptionsWidth, aButtonHeight);
		mCancelButton->Resize(aLeft + mOptionsWidth + aBottomGapX, aButtonY, mCancelWidth, aButtonHeight);
	}
	else
	{
		mCancelButton->Resize((mWidth - mCancelWidth) / 2, aButtonY, mCancelWidth, aButtonHeight);
	}
}

void RunModeDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	// 无尽变体：三张占位卡整行不摆（摆上去会吃点击、抢不到画）。
	if (mEndlessScene < 0)
	{
		for (int i = 0; i < 3; i++) AddWidget(mCardButtons[i]);
	}
	for (int i = 0; i < 3; i++) if (mDiffButtons[i]) AddWidget(mDiffButtons[i]);
	AddWidget(mCancelButton);
	if (mStartButton) AddWidget(mStartButton);
	if (mOptionsButton) AddWidget(mOptionsButton);
}

void RunModeDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	if (mEndlessScene < 0)
	{
		for (int i = 0; i < 3; i++) RemoveWidget(mCardButtons[i]);
	}
	for (int i = 0; i < 3; i++) if (mDiffButtons[i]) RemoveWidget(mDiffButtons[i]);
	RemoveWidget(mCancelButton);
	if (mStartButton) RemoveWidget(mStartButton);
	if (mOptionsButton) RemoveWidget(mOptionsButton);
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
	// 边框（悬停换高亮），卡上名字 14 粗（GDI 中文）。卡下说明 2026-10-04 已删。
	// 无尽变体（MOD_BUILD 37）整行不画，改画环境名一行。
	if (mEndlessScene < 0)
	{
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
		}
	}
	else if (!mSceneLine.empty())
	{
		// 无尽变体：环境名一行（14 粗、居中）顶在卡片原位。
		ModText::Font* aSceneFont = ModText::GetFont(14, true);
		std::wstring aScene = ModText::WideFromUtf8(mSceneLine.c_str());
		ModText::DrawTextWide(g, aSceneFont,
			(mWidth - ModText::TextWidth(aSceneFont, aScene)) / 2, mSceneY,
			aScene, Color(42, 42, 90), g->mClipRect);
	}

	// 难度行的小标题（三枚按钮自己画自己，走控件那套）：
	// 卡片区之下、居中，"多出来的是全队倍率"这层意思写在标题里。
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
	// 无尽变体（MOD_BUILD 37）的「开始」：确认开局。时长档恒为无尽，由 LawnApp 一侧的
	// StartRun 带走（连同 mDiffSel/mScaleSel/mTempoSel/mZombotanySel）。
	if (theId == RunModeDialog_Start)
	{
		mResult = RunModeDialog_Start;
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
	// 「高级选项…」：嵌套阻塞开 RunOptionsDialog（见那边的头注释——WaitForResult 泵的就
	// 是主循环）。确定才收值，取消保留原选择；不关本弹窗，选完模式一并带走。
	if (theId == RunModeDialog_Options)
	{
		RunOptionsDialog* aDialog = new RunOptionsDialog(mApp, mScaleSel, mTempoSel, mZombotanySel);
		mApp->CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
		mApp->AddDialog(Dialogs::DIALOG_RUN_OPTIONS, aDialog);
		if (aDialog->WaitForResult() == RunOptionsDialog::RunOptionsDialog_OK)
		{
			mScaleSel = aDialog->mScaleSel;
			mTempoSel = aDialog->mTempoSel;
			mZombotanySel = aDialog->mZombotanySel;
		}
		return;
	}
}
