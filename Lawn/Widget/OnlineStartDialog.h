#ifndef __ONLINESTARTDIALOG_H__
#define __ONLINESTARTDIALOG_H__

#include "LawnDialog.h"
#include "../../ConstEnums.h"	// Dialogs::（默认的 theDialogId 参数）

class LawnApp;
class LawnStoneButton;

// @pvz-online: 联机开局流程的中文框，一张类管几种面孔：
//   等待其他玩家（主机，[取消]） / 是否加入（客户端，[加入][暂不]） / 继续闯关？（主机，[继续][新开一局]） /
//   启动玩法公告（[知道了]，正文多行按 '\n' 分行、多页按 '\f' 分页） / 纯看板（"等待队友选卡"，无按钮） /
//   掉线重连（[取消并退出]，正文每帧热替换，MOD_BUILD 39）。
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
	//   NOTIFY_RECONNECT_CANCEL  掉线重连框上的「取消并退出」 → LawnApp::OnlineReconnectCancelled
	enum Notify
	{
		NOTIFY_NONE,
		NOTIFY_INVITE_ANSWER,
		NOTIFY_WAIT_CANCEL,
		NOTIFY_RECONNECT_CANCEL
	};

	// 公告翻页按钮（2026-10-08 用户要的）：按下只换页、不设 mResult——设了
	// WaitForResult 就收摊、框就关了。值不与 Dialog::ID_YES/ID_NO（1000/1001）撞。
	enum
	{
		ID_PAGE_PREV = 4100,
		ID_PAGE_NEXT = 4101
	};

	// 按钮文案传空指针 = 不摆那个按钮；两个都空 = 纯看板。
	// 正文里出现 '\f'（0x0C）时按它分页（最多 4 页）：自动在中间两枚按钮的外侧摆
	// [上一页] / [下一页]（环绕——末页的"下一页"回第 1 页），正文块下方居中画
	// "第 n / m 页"。版心按"最宽的一行 + 最高的一页"定，翻页时框大小与正文位置都不动。
	// theDraggable = 整框可鼠标拖动、且不受屏幕边缘回夹（基线 Dialog 自带拖拽，但钳在
	// 屏幕边缘 ±8px 内，框大了会被卡住看不全——只有启动公告框要这个，2026-10-04 用户要求）。
	// theDialogId = 这框注册到哪个 Dialog id 名下。默认就是它一直用的 DIALOG_ONLINE_START；
	// 掉线重连框（MOD_BUILD 39）传自己的 DIALOG_ONLINE_RECONNECT——不然会跟"等待其他玩家"
	// 那套每帧自检（GetDialog(DIALOG_ONLINE_START)）互相踩：自检会以为框没摆、叠着建。
	OnlineStartDialog(LawnApp* theApp, const char* theTitleUtf8, const char* theBodyUtf8,
		const char* theYesUtf8, const char* theNoUtf8, Notify theNotify, bool theDraggable = false,
		int theDialogId = Dialogs::DIALOG_ONLINE_START);
	virtual ~OnlineStartDialog();

	// 单页面孔的正文热替换（掉线重连框每帧刷新"第 n 次/剩 N 秒"用）。只换文本不重算尺寸：
	// 框大小是构造时按换行后的正文定死的，跟着数字每帧重排会让框抖。多页面孔不走这条路。
	void					SetBody(const char* theBodyUtf8);

	virtual void			Draw(Graphics* g);
	virtual void			KeyDown(KeyCode theKey);
	virtual void			MouseDrag(int x, int y);
	virtual void			ButtonPress(int theId);
	virtual void			ButtonDepress(int theId);
	virtual void			Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void			AddedToManager(WidgetManager* theWidgetManager);
	virtual void			RemovedFromManager(WidgetManager* theWidgetManager);

private:
	int						MaxBodyLineCount() const;	// 各页行数的最大值（版心与摆位共用）

	std::string				mTitle;			// UTF-8 原样（绘制走 ModText::WideFromUtf8）
	std::string				mPages[4];		// 每页正文 UTF-8 原样（'\f' 分页；单页面孔 = 只有 [0]）
	int						mPageCount;
	int						mPageIndex;
	int						mTitleY;
	int						mBodyY;
	int						mPageY;			// "第 n / m 页"指示行的顶（单页时不用）
	int						mButtonCount;
	LawnStoneButton*		mButtons[3];
	Notify					mNotify;
	bool					mDraggable;	// 可整框拖动且无边缘回夹（启动公告框才置 true）
};

#endif
