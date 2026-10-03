#ifndef __ONLINESTATUSWIDGET_H__
#define __ONLINESTATUSWIDGET_H__

#include <string>
#include "widget/Widget.h"

class LawnApp;

using namespace Sexy;

// @pvz-online: 组队状态小条——挂在主菜单左侧的一小块常驻 UI。
//
// 为什么要有它：联机面板是模态对话框，它在的时候后面所有牌子一个都点不着；而"组队
// 成功后还要回主菜单开局"是必经的一步——玩家就是被这一点挡住的。所以面板
// 一连上就自动收起（见 LawnApp::UpdateFrames 里那段），"我们组上队了没"改由这个小条
// 来说明。它只占自己那一小块矩形，不挡任何按钮；点它可以随时把面板叫回来（断开连接
// 在面板里）。文案自语言批（2026-10-03）起双语：中文档走 ModText 宽字符直绘（位图字体
// 没有汉字字形），英文档保持原位图字体——见 .cpp 里以 Chip 开头的几个量度/绘制助手。
//
// 行数按席位上限算（见 .cpp CHIP_LINES）：标题（+ 队友要的那串字）/ 每席位一行 / 状态行。
// 席位位子是按上限画满的——一局最多六人，从上到下就是顺位；自己那行标 (you)，
// 空位压暗。空位也画出来，才看得出"后面还有位子、也看得出谁排在我后头"。
// 标题后缀那串字：直连时是主机的 IP（队友要输进 Join 框），中继时是房间码
// （房主念给朋友、其他人核对）。

class OnlineStatusWidget : public Widget
{
public:
	LawnApp*			mApp;
	// 本机 IP：会话开始可见时算一次就存着——它要枚举网卡，不是每帧该干的活，
	// 而会话开着这段时间地址也不会变。
	std::string			mIpText;

public:
	OnlineStatusWidget(LawnApp* theApp);
	virtual ~OnlineStatusWidget();

	virtual void		Update();
	virtual void		Draw(Graphics* g);
	virtual void		MouseUp(int x, int y, int theClickCount);

private:
	std::string			GetTitleLine();
	// 标题后面挂的那串字：中继 = 房间码（房主念给朋友）；直连 = 主机念自己的 IP，
	// 客户端这行留空（念自己的地址没有用）。
	std::string			GetTitleTagText();
	// 一个席位一行："P2  Bob" / "P1  Alice (you)" / 空位 "P3  --"。
	std::string			GetSeatLine(int theSeat);
	std::string			GetStateLine();
};

#endif
