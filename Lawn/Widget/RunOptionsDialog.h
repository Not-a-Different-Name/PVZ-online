#ifndef __RUNOPTIONSDIALOG_H__
#define __RUNOPTIONSDIALOG_H__

#include "LawnDialog.h"

// ButtonWidget 不用前向声明：LawnDialog.h → widget/Dialog.h 已在 namespace Sexy 里
// 声明过，这里再来一份全局的会和 using namespace Sexy 撞成二义（C2872）。
class LawnStoneButton;
class CjkStoneButton;

// @pvz-online: 高级选项（批 C，MOD_BUILD 35）：出怪规模 / 节奏 / 植物僵尸三行开关，由
// RunModeDialog 的「高级选项…」按钮弹出（仅主机那一行摆按钮；单机档位恒标准，同出怪
// 难度行的纪律）。各行都是"小标题 + 一排 CjkStoneButton"，选中 = 按下态贴图，点了只
// 换选中、不关弹窗（照 RunModeDialog 的难度行）。档位含义见 RunState.h 的
// RUN_SCALE_*/RUN_TEMPO_* 注释；植物僵尸开关改出怪名单（见 RunZombieRoster）。
// 第四行「滑动收阳光」（2026-10-10 用户定案）与前不同：本机即时设置，不随开局广播——
// 确定即由打开方落注册表并热改 LawnApp::mSlideCollect，取消不动（同"确定才收值"纪律）。
// 第五行「巨型 Boss」（MOD_BUILD 38，2026-10-10 用户定案）：回到房间级设置口径——随
// START_LEVEL 广播；无尽档没有关底，该值在无尽档恒按关处理（StartNew/读档都归 0）。
// 无尽档不吃阶梯、**「场景难度」行在无尽档整行不摆**（2026-10-14 用户裁决；
// mShowLadderRow 为假时该行永远不上场）。僵尸 buff 行
// （RunState::mZombieBuffMode 0/1/2）无尽档照摆——无尽是五关一选、照吃。
// 页切换只挪控件：当前页的行摆进行槽位，其余整行 Resize 去屏外（画不着、点不着）——
// 选中态存在各按钮的 mInverted 上，翻回来原样还在。
// 阻塞式 WaitForResult 收结果：确定 = RunOptionsDialog_OK、取消 = Dialog::ID_NO；
// 各项选择值由打开方在 WaitForResult 之后读成员带走（取消保留原值）。嵌套阻塞没问题：
// WaitForResult 泵的就是主循环（见 Dialog::WaitForResult），父弹窗只是继续等自己的结果。
class RunOptionsDialog : public LawnDialog
{
public:
	enum
	{
		RunOptionsDialog_OK = 120,		// 与 RunPickDialog 的选卡同段：避开 Dialog::ID_YES/NO（1000+）
		RunOptionsDialog_Scale0 = 130,	// 出怪规模四枚（值 = RunState::RUN_SCALE_* 的顺序）
		RunOptionsDialog_Tempo0 = 140,	// 出怪节奏三枚（值 = RunState::RUN_TEMPO_* 的顺序）
		RunOptionsDialog_Zombotany0 = 150,	// 植物僵尸两枚（0 关 / 1 开）
		RunOptionsDialog_Slide0 = 160,	// 滑动收阳光两枚（0 关 / 1 开；本机即时设置）
		RunOptionsDialog_Boss0 = 170,	// 巨型 boss 两枚（0 关 / 1 开）
		RunOptionsDialog_BuffMult0 = 180,	// 增益倍选三枚（值 = RunState::RUN_BUFFMULT_* 的顺序；MOD_BUILD 43）
		RunOptionsDialog_PrevPage = 190,	// 上一页（翻页批：多页环绕）
		RunOptionsDialog_NextPage = 191,	// 下一页
		RunOptionsDialog_Ladder0 = 200,		// 场景难度五枚（MOD_BUILD 45）：UI 顺序 ×1/×1.2/×1.33/×1.5/×2
											// ——与枚举顺序不同（STD=0 是第三枚），值映射见 .cpp 的 kLadderUiValues
		RunOptionsDialog_ZombieBuff0 = 210,	// 僵尸 buff 三枚（MOD_BUILD 45）：0 关 / 1 / 2（值 = 顺序）
		RunOptionsDialog_Speed0 = 220,		// 移速成长三枚（2026-10-16）：0 无 / ×1.03 / ×1.05（值 = 顺序）
		RunOptionsDialog_FirstWave0 = 230,
		RunOptionsDialog_Theme0 = 240,
        RunOptionsDialog_Focus0 = 250,
        RunOptionsDialog_DamageScope0 = 260,
        RunOptionsDialog_DamageTotal0 = 270,
        RunOptionsDialog_DamageBoss0 = 280,
		RunOptionsDialog_Finale0 = 290,
        RUN_OPTIONS_ROW_COUNT = 16,
		RUN_OPTIONS_ROWS_PER_PAGE = 3,
		RUN_OPTIONS_PAGE_COUNT = 5
	};

	ButtonWidget*	mScaleButtons[4];
	ButtonWidget*	mTempoButtons[3];
	ButtonWidget*	mZombotanyButtons[2];
	ButtonWidget*	mSlideButtons[2];
	ButtonWidget*	mBossButtons[3];
	ButtonWidget*	mBuffMultButtons[3];
	ButtonWidget*	mSpeedButtons[3]; // 第 7 行（2026-10-16）：移速成长 0/×1.03/×1.05
	ButtonWidget*	mFirstWaveButtons[3];
	ButtonWidget*	mZombieBuffButtons[3];	// 第 8 行（MOD_BUILD 45）
	ButtonWidget*	mLadderButtons[5];		// 第 9 行（MOD_BUILD 45；无尽档不摆）
	int				mScaleSel;			// 选中的规模档（RunState::RUN_SCALE_*）
	int				mTempoSel;			// 选中的节奏档（RunState::RUN_TEMPO_*）
	int				mZombotanySel;		// 植物僵尸开关（0 关 1 开）
	int				mSlideSel;			// 滑动收阳光开关（0 关 1 开）
	int				mBossSel;			// 巨型 boss 档（0 关 / 1 仅末关 / 2 末关+场景3，默认 2）
	int				mBuffMultSel;		// 增益倍选档（RunState::RUN_BUFFMULT_*）
	int				mSpeedSel; // 移速成长档（0 无 / 1 ×1.03 / 2 ×1.05）
	int				mFirstWaveSel;
	int				mZombieBuffSel;		// 僵尸 buff 档（0 关 / 1 / 2）
	int				mLadderSel;			// 场景难度阶梯档（RunState::RUN_LADDER_*；存"值"不存槽位）
	int mEndlessOptions;
    int mThemeSel, mFocusSel;
    int mDamageScopeSel, mDamageTotalSel, mDamageBossSel;
    ButtonWidget* mDamageScopeButtons[2];
    ButtonWidget* mDamageTotalButtons[2];
    ButtonWidget* mDamageBossButtons[2];
    int mDamageScopeWidths[2], mDamageTotalWidths[2], mDamageBossWidths[2];
    std::string mDamageScopeLabels[2], mDamageTotalLabels[2], mDamageBossLabels[2];
    std::string mDamageScopeCaption, mDamageTotalCaption, mDamageBossCaption;
    int mFinaleSel;
    ButtonWidget* mFinaleButtons[2];
    int mFinaleWidths[2];
    std::string mFinaleLabels[2], mFinaleCaption;
    ButtonWidget* mThemeButtons[2];ButtonWidget* mFocusButtons[2];
    int mThemeWidths[2],mFocusWidths[2];
    std::string mThemeLabels[2],mFocusLabels[2],mThemeCaption,mFocusCaption;
    int RowPage(int row) const;
    int RowSlot(int row) const;
	bool			mShowLadderRow;		// 无尽档不摆「场景难度」行（2026-10-14 用户裁决：无尽不吃阶梯）
	int				mPage;
	CjkStoneButton*	mOkButton;
	CjkStoneButton*	mCancelButton;
	CjkStoneButton*	mPrevButton;
	CjkStoneButton*	mNextButton;
	int				mOkWidth;
	int				mCancelWidth;
	int				mPrevWidth;
	int				mNextWidth;
	int				mPageTextX;			// 翻页行里页码文字的左上角 / 量好的宽（Layout 用）
	int				mPageTextY;
	int				mPageTextW;
	int				mScaleWidths[4];	// 各行按钮等宽（取最长标签量好）
	int				mTempoWidths[3];
	int				mZombotanyWidths[2];
	int				mSlideWidths[2];
	int				mBossWidths[3];
	int				mBuffMultWidths[3];
	int				mSpeedWidths[3];
	int				mFirstWaveWidths[3];
	int				mZombieBuffWidths[3];
	int				mLadderWidths[5];
	int				mTitleY;			// 标题顶部（ModText 顶对齐口径）
	int				mCaptionYs[RUN_OPTIONS_ROW_COUNT];	// 各行小标题顶部（行序同 mRows 表；非当前页 = 屏外）
	std::string		mTitle;				// UTF-8 原样（绘制走 ModText::WideFromUtf8）
	std::string		mScaleCaption;
	std::string		mTempoCaption;
	std::string		mZombotanyCaption;
	std::string		mSlideCaption;
	std::string		mBossCaption;
	std::string		mBuffMultCaption;
	std::string		mSpeedCaption;
	std::string		mFirstWaveCaption;
	std::string		mZombieBuffCaption;
	std::string		mLadderCaption;
	std::string		mScaleLabels[4];
	std::string		mTempoLabels[3];
	std::string		mZombotanyLabels[2];
	std::string		mSlideLabels[2];
	std::string		mBossLabels[3];
	std::string		mBuffMultLabels[3];
	std::string		mSpeedLabels[3];
	std::string		mFirstWaveLabels[3];
	std::string		mZombieBuffLabels[3];
	std::string		mLadderLabels[5];

public:
	RunOptionsDialog(LawnApp* theApp, int theScaleSel, int theTempoSel, int theZombotanySel, int theSlideSel, int theBossSel, int theBuffMultSel, int theLadderSel, int theZombieBuffSel, int theSpeedSel, int theFirstWaveSel, bool theEndless, int theEndlessOptions=141);
	virtual ~RunOptionsDialog();

	virtual void		Draw(Graphics* g);
	virtual void		Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void		AddedToManager(WidgetManager* theWidgetManager);
	virtual void		RemovedFromManager(WidgetManager* theWidgetManager);
	virtual void		ButtonPress(int theId);
	virtual void		ButtonDepress(int theId);
	virtual void		KeyDown(KeyCode theKey);

private:
	// 摆一次全部控件：一页三行的行槽位（当前页上场、其余挪去屏外）+ 翻页行 + 确定行。
	// Resize 与翻页都走它——构造期首次摆位由 CalcSize 触发的 Resize 完成（此时按钮已建好）。
	void				PlaceRows();
};

#endif
