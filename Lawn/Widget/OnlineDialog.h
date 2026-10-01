#ifndef __ONLINEDIALOG_H__
#define __ONLINEDIALOG_H__

#include "LawnDialog.h"
#include "widget/EditListener.h"

class LawnApp;
class LawnEditWidget;
class LawnStoneButton;

// @pvz-online: M2 联机面板——建房 / 按 IP 加入 / 关面板，外加两行状态。
// 文案全英文：位图字体没有中文字形。
//
// 面板只是"遥控器"：连接本身活在 LawnApp::mOnlineSession 里，关掉面板连接照旧，
// 这样才能先建房、再回主菜单点关卡开局。

class OnlineDialog : public LawnDialog, public EditListener
{
public:
	enum
	{
		OnlineDialog_Host		= 1200,
		OnlineDialog_Join		= 1201,
		OnlineDialog_Close		= 1202,
		OnlineDialog_Disconnect	= 1203,
		OnlineDialog_IpEdit		= 1210
	};

public:
	LawnApp*			mApp;					//+0x170
	LawnStoneButton*	mHostButton;
	LawnStoneButton*	mJoinButton;
	LawnStoneButton*	mCloseButton;
	LawnEditWidget*		mIpEditWidget;
	// @pvz-online: 断开连接单独一个键。连接中时它顶掉 Host 那一格（Host/Join 这时本来
	// 就是灰的），Close 则永远只是"关面板"——不然想看一眼状态就得把连接断掉，
	// 而组队成功后玩家多半就是从小状态条点进来瞄一眼的。
	LawnStoneButton*	mDisconnectButton;

public:
	OnlineDialog(LawnApp* theApp);
	virtual ~OnlineDialog();

	virtual void		Update();
	virtual void		Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void		AddedToManager(WidgetManager* theWidgetManager);
	virtual void		RemovedFromManager(WidgetManager* theWidgetManager);
	virtual void		Draw(Graphics* g);
	virtual void		ButtonDepress(int theId);
	virtual void		EditWidgetText(int theId, const SexyString& theString);
	virtual bool		AllowChar(int theId, SexyChar theChar);

private:
	int					GetStatusBaseline();
	int					GetEditY();
	void				StartHost();
	void				StartJoin();
	std::string			GetIpText();

	std::string			mStatusLine;
	std::string			mHintLine;
};

#endif
