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
// 阻塞式 WaitForResult 收结果：确定 = RunOptionsDialog_OK、取消 = Dialog::ID_NO；
// 三个选择值由打开方在 WaitForResult 之后读成员带走（取消保留原值）。嵌套阻塞没问题：
// WaitForResult 泵的就是主循环（见 Dialog::WaitForResult），父弹窗只是继续等自己的结果。
class RunOptionsDialog : public LawnDialog
{
public:
	enum
	{
		RunOptionsDialog_OK = 120,		// 与 RunPickDialog 的选卡同段：避开 Dialog::ID_YES/NO（1000+）
		RunOptionsDialog_Scale0 = 130,	// 出怪规模四枚（值 = RunState::RUN_SCALE_* 的顺序）
		RunOptionsDialog_Tempo0 = 140,	// 出怪节奏三枚（值 = RunState::RUN_TEMPO_* 的顺序）
		RunOptionsDialog_Zombotany0 = 150	// 植物僵尸两枚（0 关 / 1 开）
	};

	ButtonWidget*	mScaleButtons[4];
	ButtonWidget*	mTempoButtons[3];
	ButtonWidget*	mZombotanyButtons[2];
	int				mScaleSel;			// 选中的规模档（RunState::RUN_SCALE_*）
	int				mTempoSel;			// 选中的节奏档（RunState::RUN_TEMPO_*）
	int				mZombotanySel;		// 植物僵尸开关（0 关 1 开）
	CjkStoneButton*	mOkButton;
	CjkStoneButton*	mCancelButton;
	int				mOkWidth;
	int				mCancelWidth;
	int				mScaleWidths[4];	// 各行按钮等宽（取最长标签量好）
	int				mTempoWidths[3];
	int				mZombotanyWidths[2];
	int				mTitleY;			// 各文字行的顶部（ModText 顶对齐口径）
	int				mScaleCaptionY;
	int				mTempoCaptionY;
	int				mZombotanyCaptionY;
	std::string		mTitle;				// UTF-8 原样（绘制走 ModText::WideFromUtf8）
	std::string		mScaleCaption;
	std::string		mTempoCaption;
	std::string		mZombotanyCaption;
	std::string		mScaleLabels[4];
	std::string		mTempoLabels[3];
	std::string		mZombotanyLabels[2];

public:
	RunOptionsDialog(LawnApp* theApp, int theScaleSel, int theTempoSel, int theZombotanySel);
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
