#ifndef __RUNMODEDIALOG_H__
#define __RUNMODEDIALOG_H__

#include "LawnDialog.h"

// ButtonWidget 不用前向声明：LawnDialog.h → widget/Dialog.h 已在 namespace Sexy 里
// 声明过，这里再来一份全局的会和 using namespace Sexy 撞成二义（C2872）。
class LawnStoneButton;

// @pvz-online: M4-b 选模式页。点大墓碑开新局时先问一句"这一局打多长"：
// 三张卡片横排（画法照 ChallengeScreen 的小游戏卡：石板边框 + 缩略图标 + 卡上名字），
// 点哪张 = 选哪个时长档，底部一枚"取消"石头按钮 = 关弹窗不开局（等待必须有出路）。
// 阻塞式 WaitForResult 收结果：返回值是卡片按钮编号（RunModeDialog_Mode0 = 完整版，
// 往后依序普通/快速）或 Dialog::ID_NO（取消）；换算回档位在 LawnApp 一侧做。
// 联机不另问：只有主机走到这一页，选完由 START_LEVEL 的模式字节带动队友。
class RunModeDialog : public LawnDialog
{
public:
	enum
	{
		RunModeDialog_Mode0 = 100,	// 与 RunPickDialog 的选卡同段：避开 Dialog::ID_YES/NO（1000+）
		RunModeDialog_Mode1,
		RunModeDialog_Mode2
	};

	ButtonWidget*		mCardButtons[3];	// 三张模式卡（隐形占位收点击，画在 Dialog::Draw 里）
	LawnStoneButton*	mCancelButton;		// 取消 = 关弹窗不开局
	int					mCancelWidth;		// 构造时按标签量好（石材贴图整段），Resize 直接用
	std::string			mTitle;				// 已转 ANSI（GDI 直接画）
	std::string			mCardNames[3];		// 卡上名字（已转 ANSI）
	std::string			mCardDescs[3];		// 卡下说明（已转 ANSI）
	int					mTitleY;

public:
	RunModeDialog(LawnApp* theApp);
	virtual ~RunModeDialog();

	virtual void		Draw(Graphics* g);
	virtual void		Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void		AddedToManager(WidgetManager* theWidgetManager);
	virtual void		RemovedFromManager(WidgetManager* theWidgetManager);
	virtual void		ButtonPress(int theId);
	virtual void		ButtonDepress(int theId);
	virtual void		KeyDown(KeyCode theKey);
};

#endif
