#ifndef __RUNMODEDIALOG_H__
#define __RUNMODEDIALOG_H__

#include "LawnDialog.h"

// ButtonWidget 不用前向声明：LawnDialog.h → widget/Dialog.h 已在 namespace Sexy 里
// 声明过，这里再来一份全局的会和 using namespace Sexy 撞成二义（C2872）。
class LawnStoneButton;

// @pvz-online: M4-b 选模式页。点大墓碑开新局时先问一句"这一局打多长"：
// 三张卡片横排（画法照 ChallengeScreen 的小游戏卡：石板边框 + 缩略图标 + 卡上名字），
// 点哪张 = 选哪个时长档，底部一枚"取消"石头按钮 = 关弹窗不开局（等待必须有出路）。
// MOD_BUILD 27 起再多一行"出怪难度"（轻松/标准/高压三枚小石头按钮，选中 = 按下态贴图，
// 点了不关弹窗）：房主定全队出怪倍率（乘在顺位乘数上，见 Board::PickZombieWaves）。
// 这一行只在"队伍"里摆（联机主机，判据与 RequestAdventure 同口径）——单机局档位恒为
// 标准，摆出来也是死控件，所以 mShowDiff 为假时整行不做。
// 阻塞式 WaitForResult 收结果：返回值是卡片按钮编号（RunModeDialog_Mode0 = 完整版，
// 往后依序普通/快速）或 Dialog::ID_NO（取消）；换算回档位、读 mDiffSel 在 LawnApp 一侧做
// （读成员安全：WaitForResult 的自动收摊是 SafeDeleteList 延迟删，框架自己也这么读 mResult）。
// 联机不另问：只有主机走到这一页，选完由 START_LEVEL 的模式字节 + 难度字节带动队友。
class RunModeDialog : public LawnDialog
{
public:
	enum
	{
		RunModeDialog_Mode0 = 100,	// 与 RunPickDialog 的选卡同段：避开 Dialog::ID_YES/NO（1000+）
		RunModeDialog_Mode1,
		RunModeDialog_Mode2,
		RunModeDialog_Diff0 = 110,	// 出怪难度三档（值同 RunState::RUN_DIFF_EASY/STD/HIGH 的顺序）
		RunModeDialog_Diff1,
		RunModeDialog_Diff2
	};

	ButtonWidget*		mCardButtons[3];	// 三张模式卡（隐形占位收点击，画在 Dialog::Draw 里）
	ButtonWidget*		mDiffButtons[3];	// 出怪难度三枚（mShowDiff 为假时全 nullptr）；选中 = mInverted
	bool				mShowDiff;			// 联机主机才有这一行
	int					mDiffSel;			// 选中的难度档（值 = RunState::RUN_DIFF_*）：LawnApp 在 WaitForResult 之后读
	LawnStoneButton*	mCancelButton;		// 取消 = 关弹窗不开局
	int					mCancelWidth;		// 构造时按标签量好（石材贴图整段），Resize 直接用
	int					mDiffWidths[3];		// 三枚难度按钮的宽度（等宽，取最长标签量好）
	int					mDiffCaptionY;		// "出怪难度"一行的顶部（ModText 顶对齐口径）
	std::string			mTitle;				// UTF-8 原样（绘制走 ModText::WideFromUtf8）
	std::string			mCardNames[3];		// 卡上名字（UTF-8 原样）
	std::string			mCardDescs[3];		// 卡下说明（UTF-8 原样）
	std::string			mDiffLabels[3];		// 难度按钮标签（UTF-8 原样）
	std::string			mDiffCaption;		// 难度行的小标题（UTF-8 原样）
	int					mTitleY;			// 标题顶部（ModText 顶对齐口径）

public:
	RunModeDialog(LawnApp* theApp, bool theShowDiff);
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
