#include "OnlineStatusWidget.h"
#include "../Online/NetSession.h"
#include "../System/PlayerInfo.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "graphics/Font.h"

namespace
{
	const int	CHIP_PAD_X		= 8;		// 文字到小条边缘
	const int	CHIP_PAD_Y		= 5;
	const int	CHIP_LINES		= 3;		// 标题 / 名字+IP / 状态
}

OnlineStatusWidget::OnlineStatusWidget(LawnApp* theApp)
{
	mApp = theApp;

	// 没开会话（绝大多数时候）就整个藏起来：不画东西的 widget 只要还可见，
	// 它那块矩形照样会把点击吃掉。
	mVisible = false;
	Resize(8, 8, 10, 10);
}

OnlineStatusWidget::~OnlineStatusWidget()
{
}

void OnlineStatusWidget::Update()
{
	Widget::Update();

	NetSession* aSession = mApp->mOnlineSession;
	bool aShow = aSession != nullptr && aSession->IsActive();
	if (aShow != mVisible)
	{
		mVisible = aShow;
		MarkDirty();
		// 刚开会话：顺手把本机 IP 取一次。它要枚举网卡，不是每帧该干的活，
		// 而会话开着这段时间地址不会变。
		if (aShow) mIpText = NetLink::GetLocalIPv4Text();
	}
	if (!aShow) return;

	// 宽度按三行里最宽的那行量出来：状态行有长有短（"Hosting - waiting for player" 最长），
	// 名字那行还看玩家自己叫什么，写死宽度不是勒着字就是留一大块空底。
	int aWidth = FONT_DWARVENTODCRAFT12->StringWidth(_S("CO-OP ONLINE"));
	int anIdentityWidth = FONT_DWARVENTODCRAFT12->StringWidth(GetIdentityLine());
	if (anIdentityWidth > aWidth) aWidth = anIdentityWidth;
	int aStateWidth = FONT_DWARVENTODCRAFT12->StringWidth(GetStateLine());
	if (aStateWidth > aWidth) aWidth = aStateWidth;
	aWidth += CHIP_PAD_X * 2;

	int aHeight = CHIP_PAD_Y * 2 + FONT_DWARVENTODCRAFT12->GetLineSpacing() * CHIP_LINES;
	if (aWidth != mWidth || aHeight != mHeight)
		Resize(mX, mY, aWidth, aHeight);
}

void OnlineStatusWidget::Draw(Graphics* g)
{
	NetSession* aSession = mApp->mOnlineSession;
	if (!aSession || !aSession->IsActive()) return;

	// 半透明黑底 + 一圈细边：主菜单那一角是树和天空，不衬底白字根本读不出来
	g->SetColor(Color(0, 0, 0, 140));
	g->FillRect(0, 0, mWidth, mHeight);
	g->SetColor(Color(255, 255, 255, 60));
	g->DrawRect(0, 0, mWidth, mHeight);

	g->SetFont(FONT_DWARVENTODCRAFT12);
	int aLineY = CHIP_PAD_Y + FONT_DWARVENTODCRAFT12->GetAscent();

	g->SetColor(Color(255, 208, 80));
	g->DrawString(_S("CO-OP ONLINE"), CHIP_PAD_X, aLineY);

	aLineY += FONT_DWARVENTODCRAFT12->GetLineSpacing();
	g->SetColor(Color(205, 230, 255));
	g->DrawString(GetIdentityLine(), CHIP_PAD_X, aLineY);

	aLineY += FONT_DWARVENTODCRAFT12->GetLineSpacing();
	g->SetColor(Color(255, 255, 255));
	g->DrawString(GetStateLine(), CHIP_PAD_X, aLineY);
}

void OnlineStatusWidget::MouseUp(int x, int y, int theClickCount)
{
	(void)x; (void)y; (void)theClickCount;

	// 面板就是断开连接的入口，也是唯一能看到完整状态（本机 IP、失败原因）的地方
	mApp->PlaySample(Sexy::SOUND_TAP);
	mApp->DoOnlineDialog();
}

// 本机是谁、在哪台机器上：名字取自本机存档（建房的那位把它念给队友，队友才好输 IP）。
std::string OnlineStatusWidget::GetIdentityLine()
{
	// mName 是玩家自己在建档时敲的，位图字体画不出来的字符最多是空白——
	// 名字和 IP 谁缺了都还有另一半顶着。
	std::string aName = mApp->mPlayerInfo ? mApp->mPlayerInfo->mName : std::string();
	if (aName.empty()) return mIpText;

	if (mIpText.empty()) return aName;
	return aName + "  " + mIpText;
}

// 一行短状态：详细说法在面板里（那两行 NetSession 的状态/提示足够啰嗦了），
// 这里只回答"我现在是什么身份、卡在哪一步"。
std::string OnlineStatusWidget::GetStateLine()
{
	NetSession* aSession = mApp->mOnlineSession;
	if (!aSession || !aSession->IsActive()) return "";

	// 主机按了关卡、正等队友就位。这时候会话还是 CONNECTED，不单独说一句的话
	// 小条还写着 "pick a level"，看着像压根没点上。
	if (mApp->IsOnlineWaitingStartAck())
		return "Starting - waiting for teammate";

	switch (aSession->GetState())
	{
	case NetSession::State::LISTENING:
		return "Hosting - waiting for player";

	case NetSession::State::CONNECTING:
		{
			// 连不上会一直重试，重试次数得露出来，不然"还在试"看着和"卡死了"一样
			int anAttempts = aSession->GetConnectAttempts();
			std::string aText = "Connecting";
			if (anAttempts > 1)
				aText += " (attempt " + std::to_string((unsigned)anAttempts) + ")";
			return aText;
		}

	case NetSession::State::HANDSHAKING:
		return "Handshaking";

	case NetSession::State::CONNECTED:
		return (aSession->GetRole() == NetSession::Role::HOST)
			? "Host - pick a level"
			: "Client - waiting for host";

	case NetSession::State::DEAD:
		// 死因用会话给的短标签：版本/构建不符这类"要你动手换包"的原因，
		// 在这块小条上不能显示成和"网线掉了"一样的话
		return aSession->GetShortStatus();

	case NetSession::State::OFF:
	default:
		return "";
	}
}
