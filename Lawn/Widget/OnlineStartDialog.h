#ifndef __ONLINESTARTDIALOG_H__
#define __ONLINESTARTDIALOG_H__

#include "LawnDialog.h"

class LawnApp;
class LawnStoneButton;

// @pvz-online: 联机开局流程的中文框，一张类管三种面孔：
//   等待其他玩家（主机，无按钮） / 是否加入（客户端，[加入][暂不]） / 继续闯关？（主机，[继续][重新开始]）。
// 为什么自己做：位图字体（main.pak 里的 BrianneTod 全系）只到 Latin，画不了汉字。
// 这里走 SysFont（GDI）——源码字面量是 UTF-8，转成本机码页（简中 = GBK）的字节，
// 由 TextOutA 画出去；参数照 SysFont::Init。这条路先用 tools/cjk_probe 单独验证过
// （仓库外，不进版本库），探针图里四条文案都清晰可读。
// 按钮是石材按钮的原画法，只是标签字体换掉（原版 DrawStoneButton 把字体写死成位图字体）。
class OnlineStartDialog : public LawnDialog
{
public:
	// theNotifyApp=true：非阻塞（联机询问框），点按钮回 LawnApp::OnlineStartPromptAnswer；
	// false：阻塞（WaitForResult 等返回值，比如"续不续存档"、单按钮的通知）。
	// 按钮文案传空指针 = 不摆那个按钮；两个都空 = 纯看板（"等待其他玩家"那张）。
	OnlineStartDialog(LawnApp* theApp, const char* theTitleUtf8, const char* theBodyUtf8,
		const char* theYesUtf8, const char* theNoUtf8, bool theNotifyApp);
	virtual ~OnlineStartDialog();

	virtual void			Draw(Graphics* g);
	virtual void			KeyDown(KeyCode theKey);
	virtual void			ButtonPress(int theId);
	virtual void			ButtonDepress(int theId);
	virtual void			Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void			AddedToManager(WidgetManager* theWidgetManager);
	virtual void			RemovedFromManager(WidgetManager* theWidgetManager);

private:
	std::string				mTitle;			// 已经转成本机 ANSI（简中 = GBK）的文本
	std::string				mBody;
	int						mTitleY;
	int						mBodyY;
	int						mButtonCount;
	LawnStoneButton*		mButtons[2];
	bool					mNotifyApp;
};

#endif
