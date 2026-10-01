#ifndef __ONLINESTATUSWIDGET_H__
#define __ONLINESTATUSWIDGET_H__

#include <string>
#include "widget/Widget.h"

class LawnApp;

using namespace Sexy;

// @pvz-online: 组队状态小条——挂在主菜单左侧的一小块常驻 UI。
//
// 为什么要有它：联机面板是模态对话框，它在的时候后面所有牌子一个都点不着；而"组队
// 成功后还要回主菜单点关卡牌开局"是必经的一步——玩家就是被这一点挡住的。所以面板
// 一连上就自动收起（见 LawnApp::UpdateFrames 里那段），"我们组上队了没"改由这个小条
// 来说明。它只占自己那一小块矩形，不挡任何按钮；点它可以随时把面板叫回来（断开连接
// 在面板里）。文案全英文：位图字体没有中文字形。

class OnlineStatusWidget : public Widget
{
public:
	LawnApp*			mApp;

public:
	OnlineStatusWidget(LawnApp* theApp);
	virtual ~OnlineStatusWidget();

	virtual void		Update();
	virtual void		Draw(Graphics* g);
	virtual void		MouseUp(int x, int y, int theClickCount);

private:
	std::string			GetStateLine();
};

#endif
