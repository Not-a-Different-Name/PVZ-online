#ifndef __ONLINEDIALOG_H__
#define __ONLINEDIALOG_H__

#include "LawnDialog.h"
#include "widget/EditListener.h"

class LawnApp;
class LawnEditWidget;
class LawnStoneButton;

// @pvz-online: 组队页面（从主位那块 ADVENTURE 大墓碑、主菜单左上角的小状态条进来）
// ——建房 / 按 IP 加入 / 关面板，外加两行状态和一个房间信息块（房间码、我坐哪一席、
// 谁是房主、P1..P4 名册）。文案全英文：位图字体没有中文字形。
//
// 面板只是"遥控器"：组队本身活在 LawnApp::mOnlineSession 里，关掉面板队伍照旧，
// 这样才能先建房、再回主菜单点入口开局（主菜单左上角的小状态条随时能把面板叫回来）。
//
// 闯关的入口在主位大墓碑上：没队伍时点它先把这个面板叫出来，队伍在手（一个人的队伍
// 也算）再点就开一局闯关（LawnApp::RequestAdventure）。第三槽那块 PUZZLE 石板是原版
// 战役入口，不参与组队闯关；队友连着时点它还是原来的单关联机流程（R5 才把队友拉进闯关）。

class OnlineDialog : public LawnDialog, public EditListener
{
public:
	enum
	{
		OnlineDialog_Host		= 1200,
		OnlineDialog_Join		= 1201,
		OnlineDialog_Close		= 1202,
		OnlineDialog_Disconnect	= 1203,
		OnlineDialog_Swap		= 1204,
		OnlineDialog_Accept		= 1205,
		OnlineDialog_Reject		= 1206,
		OnlineDialog_CreateRoom	= 1207,
		OnlineDialog_JoinRoom	= 1208,
		OnlineDialog_IpEdit		= 1210,		// 直连：主机地址
		OnlineDialog_ServerEdit	= 1211,		// 中继：服务器地址
		OnlineDialog_CodeEdit	= 1212		// 中继：房间码
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
	// @pvz-online: 开局前换位（和后一位换）。和 Disconnect 一样只在会话活着时露面、
	// 占中间那一格——那时 Join 本来就藏着，两把键不会抢位子。
	LawnStoneButton*	mSwapButton;
	// @pvz-online: 对面发来换位请求时顶上前两格，问玩家同不同意。面板是模态的
	// （别处点不着），但主循环照跑——等答复不会把心跳等断掉。
	LawnStoneButton*	mAcceptButton;
	LawnStoneButton*	mRejectButton;
	// @pvz-online: M3 中继那一排——Server/Code 两个输入框 + 建房/按码加入两把键，
	// 和底排的直连 Host/Join 并存（直连模式保留）。默认地址先填本机，P4 上线后换云 IP。
	LawnEditWidget*		mServerEditWidget;
	LawnEditWidget*		mCodeEditWidget;
	LawnStoneButton*	mCreateRoomButton;
	LawnStoneButton*	mJoinRoomButton;

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

private:
	int					GetStatusBaseline();
	// @pvz-online: 房间信息块（标题 + 四个席位名册）就排在两行状态和输入框之间，见 .cpp。
	// 地盘是钉死的五行——没连上时标题说"没房间"、名册留空，面板不因为这个跳高度。
	int					GetRoomHeaderBaseline();
	void				DrawRoomBlock(Graphics* g);
	std::string			GetRoomHeaderLine();
	std::string			GetRoomSeatLine(int theSeat);
	int					GetEditY();
	void				StartHost();
	void				StartJoin();
	void				StartRoomHost();
	void				StartRoomJoin();
	std::string			GetIpText();
	std::string			GetServerText();
	std::string			GetRoomCodeText();

	std::string			mStatusLine;
	std::string			mHintLine;
	// 已经写进 Code 框的那个房间码：只在码真的换了的时候写一次，
	// 不然玩家在框里敲字会被每帧擦掉。
	std::string			mShownRoomCode;
	// 直连房主要念给队友的自己那条地址；枚举网卡不是每帧该干的活，算一次存着。
	std::string			mLocalIpText;
	bool				mLocalIpLoaded;
};

#endif
