#ifndef __RUNPICKDIALOG_H__
#define __RUNPICKDIALOG_H__

#include "LawnDialog.h"

class RunState;
class LawnStoneButton;

// @pvz-online: 闯关的"三选一"屏（R2）。一屏摆三张卡：
// 植物屏摆三袋种子（原版种子包美术，一眼认得出是哪株），buff 屏摆三条增益（名字在按钮上、
// 一句效果画在各自上方）。归属单一：选谁、加进哪张卡池全由 RunState 说了算，
// 这个类只管画和收点击——按下第 N 张就把序号交回 LawnApp::RunPickChosen，
// 加卡池、叠 buff、进下一关都在那边。
//
// 待选的几张还没选完之前，这一屏必然占着屏幕（LawnApp::UpdateRunPick 每帧看着，
// 发现该选而屏不在就再开一张）——屏被误关不算出口，下一帧它照样弹回来。正经的出路
// 有两条：点一张卡（RunPickChosen），或点「放弃」（Skip，2026-10-03 用户定案：
// 三条都不想要时也得能往前走——欠的屏数照减、什么都不拿）。
class RunPickDialog : public LawnDialog
{
public:
	enum
	{
		RunPickDialog_Choice0 = 100,
		RunPickDialog_Choice1,
		RunPickDialog_Choice2,
		RunPickDialog_Skip
	};

	RunState*			mRun;					// 这一屏为哪一局开
	bool				mPlantPick;				// 这一屏发的是植物（true）还是 buff（false）
	LawnStoneButton*	mChoiceButtons[3];
	LawnStoneButton*	mSkipButton;			// 「放弃」：单独一行在最底下（见 Resize）
	int					mColumnX[3];			// 三列的位置与宽度（Resize 里算好，Draw 直接用）
	int					mColumnWidth;
	int					mAreaTop;				// 卡片区（种子包 / 效果说明）的上沿与高度
	int					mAreaHeight;

public:
	RunPickDialog(LawnApp* theApp, RunState* theRun);
	virtual ~RunPickDialog();

	virtual void		Draw(Graphics* g);
	virtual void		Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void		AddedToManager(WidgetManager* theWidgetManager);
	virtual void		RemovedFromManager(WidgetManager* theWidgetManager);
	virtual void		ButtonDepress(int theId);
	virtual void		KeyDown(KeyCode theKey);
};

#endif
