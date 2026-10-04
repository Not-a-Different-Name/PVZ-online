#ifndef __ONLINESTARTDIALOG_H__
#define __ONLINESTARTDIALOG_H__

#include "LawnDialog.h"

class LawnApp;
class LawnStoneButton;

// @pvz-online: 联机开局流程的中文框，一张类管几种面孔：
//   等待其他玩家（主机，[取消]） / 是否加入（客户端，[加入][暂不]） / 继续闯关？（主机，[继续][新开一局]） /
//   启动玩法公告（[知道了]，正文多行按 '\n' 分行） / 纯看板（"等待队友选卡"，无按钮）。
// 为什么自己做：位图字体（main.pak 里的 BrianneTod 全系）只到 Latin，画不了汉字。
// 自 2026-10-03 语言批起走 ModText 的宽字符直绘（UTF-8 → UTF-16 → TextOutW，与系统码页
// 脱钩）；此前是 SysFont + Utf8ToAnsi（转本机码页再 TextOutA），已并入 Lawn/ModText，
// 这条路先用 tools/cjk_probe 单独验证过（仓库外，不进版本库）。
// 按钮是石材按钮的原画法（CjkStoneButton，见 CjkStoneButton.h）：中文档标签走 ModText
// 宽字符直绘，英文档回落到原版位图字体的绿字内嵌样式。
class OnlineStartDialog : public LawnDialog
{
public:
	// 按钮按下去之后这框跟谁说：
	//   NOTIFY_NONE          不通知：阻塞框自己从 WaitForResult 收结果（续不续存档），纯看板没有按钮
	//   NOTIFY_INVITE_ANSWER 联机"是否加入"询问框 → LawnApp::OnlineStartPromptAnswer
	//   NOTIFY_WAIT_CANCEL   主机"等待其他玩家"上的取消 → LawnApp::OnlineStartWaitCancelled
	enum Notify
	{
		NOTIFY_NONE,
		NOTIFY_INVITE_ANSWER,
		NOTIFY_WAIT_CANCEL
	};

	// 按钮文案传空指针 = 不摆那个按钮；两个都空 = 纯看板。
	// theDraggable = 整框可鼠标拖动、且不受屏幕边缘回夹（基线 Dialog 自带拖拽，但钳在
	// 屏幕边缘 ±8px 内，框大了会被卡住看不全——只有启动公告框要这个，2026-10-04 用户要求）。
	OnlineStartDialog(LawnApp* theApp, const char* theTitleUtf8, const char* theBodyUtf8,
		const char* theYesUtf8, const char* theNoUtf8, Notify theNotify, bool theDraggable = false);
	virtual ~OnlineStartDialog();

	virtual void			Draw(Graphics* g);
	virtual void			KeyDown(KeyCode theKey);
	virtual void			MouseDrag(int x, int y);
	virtual void			ButtonPress(int theId);
	virtual void			ButtonDepress(int theId);
	virtual void			Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void			AddedToManager(WidgetManager* theWidgetManager);
	virtual void			RemovedFromManager(WidgetManager* theWidgetManager);

private:
	std::string				mTitle;			// UTF-8 原样（绘制走 ModText::WideFromUtf8）
	std::string				mBody;
	int						mTitleY;
	int						mBodyY;
	int						mButtonCount;
	LawnStoneButton*		mButtons[2];
	Notify					mNotify;
	bool					mDraggable;	// 可整框拖动且无边缘回夹（启动公告框才置 true）
};

#endif
