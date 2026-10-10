#ifndef __RUNOPTIONSDIALOG_H__
#define __RUNOPTIONSDIALOG_H__

#include "LawnDialog.h"

// ButtonWidget 不用前向声明：LawnDialog.h → widget/Dialog.h 已在 namespace Sexy 里
// 声明过，这里再来一份全局的会和 using namespace Sexy 撞成二义（C2872）。
class LawnStoneButton;
class CjkStoneButton;

// @pvz-online: 高级选项（批 C，MOD_BUILD 35）：出怪规模 / 节奏 / 植物僵尸三行开关，由
// RunModeDialog 的「高级选项…」按钮弹出（仅主机那一行摆按钮；单机档位恒标准，同出怪
// 难度行的纪律）。三行都是"小标题 + 一排 CjkStoneButton"，选中 = 按下态贴图，点了只
// 换选中、不关弹窗（照 RunModeDialog 的难度行）。档位含义见 RunState.h 的
// RUN_SCALE_*/RUN_TEMPO_* 注释；植物僵尸开关改出怪名单（见 RunZombieRoster）。
// 第四行「滑动收阳光」（2026-10-10 用户定案）与前不同：本机即时设置，不随开局广播——
// 确定即由打开方落注册表并热改 LawnApp::mSlideCollect，取消不动（同"确定才收值"纪律）。
// 第五行「巨型 Boss」（MOD_BUILD 38，2026-10-10 用户定案）：回到房间级设置口径——随
// START_LEVEL 广播（START_LEVEL 载荷 24→25），无尽档没有关底、该行不摆。
// 阻塞式 WaitForResult 收结果：确定 = RunOptionsDialog_OK、取消 = Dialog::ID_NO；
// 五个选择值由打开方在 WaitForResult 之后读成员带走（取消保留原值）。嵌套阻塞没问题：
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
		RunOptionsDialog_Boss0 = 170	// 巨型 boss 两枚（0 关 / 1 开）
	};

	ButtonWidget*	mScaleButtons[4];
	ButtonWidget*	mTempoButtons[3];
	ButtonWidget*	mZombotanyButtons[2];
	ButtonWidget*	mSlideButtons[2];
	ButtonWidget*	mBossButtons[2];
	int				mScaleSel;			// 选中的规模档（RunState::RUN_SCALE_*）
	int				mTempoSel;			// 选中的节奏档（RunState::RUN_TEMPO_*）
	int				mZombotanySel;		// 植物僵尸开关（0 关 1 开）
	int				mSlideSel;			// 滑动收阳光开关（0 关 1 开）
	int				mBossSel;			// 巨型 boss 开关（0 关 1 开）
	CjkStoneButton*	mOkButton;
	CjkStoneButton*	mCancelButton;
	int				mOkWidth;
	int				mCancelWidth;
	int				mScaleWidths[4];	// 各行按钮等宽（取最长标签量好）
	int				mTempoWidths[3];
	int				mZombotanyWidths[2];
	int				mSlideWidths[2];
	int				mBossWidths[2];
	int				mTitleY;			// 各文字行的顶部（ModText 顶对齐口径）
	int				mScaleCaptionY;
	int				mTempoCaptionY;
	int				mZombotanyCaptionY;
	int				mSlideCaptionY;
	int				mBossCaptionY;
	std::string		mTitle;				// UTF-8 原样（绘制走 ModText::WideFromUtf8）
	std::string		mScaleCaption;
	std::string		mTempoCaption;
	std::string		mZombotanyCaption;
	std::string		mSlideCaption;
	std::string		mBossCaption;
	std::string		mScaleLabels[4];
	std::string		mTempoLabels[3];
	std::string		mZombotanyLabels[2];
	std::string		mSlideLabels[2];
	std::string		mBossLabels[2];

public:
	RunOptionsDialog(LawnApp* theApp, int theScaleSel, int theTempoSel, int theZombotanySel, int theSlideSel, int theBossSel);
	virtual ~RunOptionsDialog();

	virtual void		Draw(Graphics* g);
	virtual void		Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void		AddedToManager(WidgetManager* theWidgetManager);
	virtual void		RemovedFromManager(WidgetManager* theWidgetManager);
	virtual void		ButtonPress(int theId);
	virtual void		ButtonDepress(int theId);
	virtual void		KeyDown(KeyCode theKey);
};

#endif
