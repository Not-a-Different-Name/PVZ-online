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
	const int	CHIP_LINES		= 6;		// 标题 / P1..P4 / 状态
	const int	TITLE_GAP		= 12;		// 标题和后面那截 IP 之间的空当
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

	// 宽度按六行里最宽的那行量出来：状态行有长有短（"Hosting - waiting for player" 最长），
	// 名字那行还看玩家自己叫什么，写死宽度不是勒着字就是留一大块空底。
	int aWidth = FONT_DWARVENTODCRAFT12->StringWidth(GetTitleLine());
	std::string aTitleIp = GetTitleIpText();
	if (!aTitleIp.empty())
		aWidth += TITLE_GAP + FONT_DWARVENTODCRAFT12->StringWidth(aTitleIp);
	for (int aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		int aSeatWidth = FONT_DWARVENTODCRAFT12->StringWidth(GetSeatLine(aSeat));
		if (aSeatWidth > aWidth) aWidth = aSeatWidth;
	}
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
	int aLineHeight = FONT_DWARVENTODCRAFT12->GetLineSpacing();

	std::string aTitle = GetTitleLine();
	g->SetColor(Color(255, 208, 80));
	g->DrawString(aTitle, CHIP_PAD_X, aLineY);

	// 主机名后面挂着本机 IP：队友要输的就是它，念的时候得看得见
	std::string aTitleIp = GetTitleIpText();
	if (!aTitleIp.empty())
	{
		g->SetColor(Color(160, 200, 255));
		g->DrawString(aTitleIp,
			CHIP_PAD_X + FONT_DWARVENTODCRAFT12->StringWidth(aTitle) + TITLE_GAP, aLineY);
	}

	// 名册：四个位子画满，从上到下就是顺位。自己在最亮那行，空位压暗——
	// 一眼看得出"我在几号位、后面还有没有人"。
	for (int aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		aLineY += aLineHeight;
		if (aSession->GetLocalSeat() == aSeat)
			g->SetColor(Color(255, 255, 255));
		else if (aSession->IsSeatOccupied((uint8_t)aSeat))
			g->SetColor(Color(205, 230, 255));
		else
			g->SetColor(Color(150, 150, 150));
		g->DrawString(GetSeatLine(aSeat), CHIP_PAD_X, aLineY);
	}

	aLineY += aLineHeight;
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

std::string OnlineStatusWidget::GetTitleLine()
{
	return "CO-OP ONLINE";
}

// 本机 IP：只有主机需要它——队友要输进 Join 框里的就是这一串，主机得念得出来。
// 客户端念自己的地址没有用，那行就空着（空着不占宽度）。
std::string OnlineStatusWidget::GetTitleIpText()
{
	NetSession* aSession = mApp->mOnlineSession;
	if (!aSession || aSession->GetRole() != NetSession::Role::HOST) return "";
	return mIpText;
}

// 名册的一行。位子空着就说空着（"--"），别看名字那栏是空的就以为是没画出来。
std::string OnlineStatusWidget::GetSeatLine(int theSeat)
{
	NetSession* aSession = mApp->mOnlineSession;
	std::string aText = "P" + std::to_string(theSeat);
	if (!aSession) return aText + "  --";

	bool aMine = (aSession->GetLocalSeat() == theSeat);
	if (!aSession->IsSeatOccupied((uint8_t)theSeat))
		return aText + "  --";

	std::string aName = aSession->GetSeatName((uint8_t)theSeat);
	// 名字一个能画的字形都没有（比如玩家建档时敲的是中文）时，自己那行还有 (you)
	// 顶着，队友那行就得直说没名字，免得看着像个占了位子又不说话的鬼影。
	if (aName.empty() && !aMine)
		aName = "(no name)";
	if (!aName.empty())
		aText += "  " + aName;
	if (aMine)
		aText += " (you)";
	return aText;
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

	// 换位这件事有来有回，两种"等"得分开说：对面问我（面板会自动叫出来，
	// 但玩家也能把它关掉，关了就靠这行提醒），还是我在等对面回话。
	if (aSession->HasIncomingSwapRequest())
		return "Swap request - open the panel";
	if (aSession->IsSwapRequestPending())
		return "Swap asked - waiting";

	// 刚发生的事（换成了 / 被拒绝了）优先占几秒
	if (!aSession->GetNoticeText().empty())
		return aSession->GetNoticeText();

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
