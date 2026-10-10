#include "OnlineStatusWidget.h"
#include "CjkStoneButton.h"
#include "../ModText.h"
#include "../Online/NetSession.h"
#include "../System/PlayerInfo.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "graphics/Font.h"

namespace
{
	const int	CHIP_PAD_X		= 8;		// 文字到小条边缘
	const int	CHIP_PAD_Y		= 5;
	const int	CHIP_LINES		= NetProto::MAX_PLAYERS + 2;	// 标题 + 每席位一行 + 状态
	const int	TITLE_GAP		= 12;		// 标题和后面那截 IP 之间的空当

	// @pvz-online 语言批（2026-10-03）：中文档整条走 ModText 宽字符直绘（位图字体没有
	// 汉字字形），英文档保持原位图字体原样。量宽/行距/首行基线三个量度都按当前语言取，
	// Update 的排布与 Draw 的落笔共用这几个函数——两边各算各的迟早对不上。
	// 字号取 12：与小条位图字体（DwarvenTodCraft12）的名义字号同档，行距也接近，
	// 切语言时小条不会明显跳高跳矮。
	#define CHIP_CJK_PX 12

	ModText::Font* ChipCjkFont() { return ModText::GetFont(CjkPointSize(CHIP_CJK_PX), false); }

	int ChipTextWidth(const std::string& theText)
	{
		if (!ModText::IsChinese()) return FONT_DWARVENTODCRAFT12->StringWidth(theText);
		return ModText::TextWidth(ChipCjkFont(), ModText::WideFromUtf8(theText.c_str()));
	}

	int ChipLineStep()
	{
		if (!ModText::IsChinese()) return FONT_DWARVENTODCRAFT12->GetLineSpacing();
		return ModText::LineHeight(ChipCjkFont());
	}

	int ChipFirstBaseline()
	{
		if (!ModText::IsChinese()) return CHIP_PAD_Y + FONT_DWARVENTODCRAFT12->GetAscent();
		return CHIP_PAD_Y + ModText::Ascent(ChipCjkFont());
	}

	// theBaseline 是行基线：两种画法对到同一条线上（位图字体按自己的基线落笔，宽字符
	// 用 顶 = 基线 - Ascent 换算，ModText 的顶对齐口径）。颜色由调用方给。
	void ChipDrawText(Graphics* g, const std::string& theText, int theX, int theBaseline, const Color& theColor)
	{
		g->SetColor(theColor);
		if (!ModText::IsChinese())
		{
			g->SetFont(FONT_DWARVENTODCRAFT12);
			g->DrawString(theText, theX, theBaseline);
			return;
		}
		ModText::Font* aFont = ChipCjkFont();
		ModText::DrawTextWide(g, aFont, theX, theBaseline - ModText::Ascent(aFont),
			ModText::WideFromUtf8(theText.c_str()), theColor, g->mClipRect);
	}
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
	// （语言批：量宽走 ChipTextWidth，中文档按宽字符量。）
	int aWidth = ChipTextWidth(GetTitleLine());
	std::string aTitleTag = GetTitleTagText();
	if (!aTitleTag.empty())
		aWidth += TITLE_GAP + ChipTextWidth(aTitleTag);
	for (int aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		int aSeatWidth = ChipTextWidth(GetSeatLine(aSeat));
		if (aSeatWidth > aWidth) aWidth = aSeatWidth;
	}
	int aStateWidth = ChipTextWidth(GetStateLine());
	if (aStateWidth > aWidth) aWidth = aStateWidth;
	aWidth += CHIP_PAD_X * 2;

	int aHeight = CHIP_PAD_Y * 2 + ChipLineStep() * CHIP_LINES;
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

	int aLineY = ChipFirstBaseline();		// 首行基线（语言批：两种字体各自的基线口径）
	int aLineHeight = ChipLineStep();

	std::string aTitle = GetTitleLine();
	ChipDrawText(g, aTitle, CHIP_PAD_X, aLineY, Color(255, 208, 80));

	// 标题后面挂一串字：中继挂房间码（念给朋友 / 核对进对了没有），直连挂主机 IP
	std::string aTitleTag = GetTitleTagText();
	if (!aTitleTag.empty())
	{
		ChipDrawText(g, aTitleTag, CHIP_PAD_X + ChipTextWidth(aTitle) + TITLE_GAP, aLineY,
			Color(160, 200, 255));
	}

	// 名册：位子全部画满，从上到下就是顺位。自己在最亮那行，空位压暗——
	// 一眼看得出"我在几号位、后面还有没有人"。
	for (int aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		aLineY += aLineHeight;
		Color aColor;
		if (aSession->GetLocalSeat() == aSeat)
			aColor = Color(255, 255, 255);
		else if (aSession->IsSeatOccupied((uint8_t)aSeat))
			aColor = Color(205, 230, 255);
		else
			aColor = Color(150, 150, 150);
		ChipDrawText(g, GetSeatLine(aSeat), CHIP_PAD_X, aLineY, aColor);
	}

	aLineY += aLineHeight;
	ChipDrawText(g, GetStateLine(), CHIP_PAD_X, aLineY, Color(255, 255, 255));
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
	// 语言批：标题跟着 UI 语言走（中文档是小条唯一的"这是什么"说明）
	return ModText::Tr("联机合作", "CO-OP ONLINE");
}

// 标题后缀：中继挂房间码（两边都挂——房主要念得出来，队友要核对进对没进对）；
// 直连才是老板子：主机念自己的 IP（队友要输进 Join 框的就是它），客户端那格留空。
std::string OnlineStatusWidget::GetTitleTagText()
{
	NetSession* aSession = mApp->mOnlineSession;
	if (!aSession) return "";
	if (aSession->IsRelay())
	{
		std::string aCode = aSession->GetRoomCode();
		if (aCode.empty()) return "";
		return ModText::Tr("房间 ", "Room ") + aCode;
	}
	if (aSession->GetRole() != NetSession::Role::HOST) return "";
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
	// （语言批：名字是玩家数据原样，只有这两处标签跟着 UI 语言走。）
	if (aName.empty() && !aMine)
		aName = ModText::Tr("（无名）", "(no name)");
	if (!aName.empty())
		aText += "  " + aName;
	// MOD_BUILD 39：掉线保留期的席位当场注明——名册上人还占着位（各等待门照旧堵着），
	// 不标一句的话队友那行看着和在线没两样。
	if (aSession->IsSeatOffline((uint8_t)theSeat))
		aText += ModText::Tr("（掉线中）", " (offline)");
	if (aMine)
		aText += ModText::Tr("（你）", " (you)");
	return aText;
}

// 一行短状态：详细说法在面板里（那两行 NetSession 的状态/提示足够啰嗦了），
// 这里只回答"我现在是什么身份、卡在哪一步"。
std::string OnlineStatusWidget::GetStateLine()
{
	NetSession* aSession = mApp->mOnlineSession;
	if (!aSession || !aSession->IsActive()) return "";

	// MOD_BUILD 39：重连最优先——小条这一行只回答"卡在哪"。重连期别的一切状态
	// （等 ACK、换位、刚发生的事）都得让位：人先要回到房里，才谈得上下一步。
	if (aSession->IsReconnecting())
		return ModText::Tr("正在重连……（第 ", "Reconnecting... (attempt ")
			+ std::to_string(aSession->GetReconnectAttempt())
			+ ModText::Tr(" 次）", ")");

	// 主机按了关卡、正等队友就位。这时候会话还是 CONNECTED，不单独说一句的话
	// 小条还写着 "pick a level"，看着像压根没点上。六席位时等的是所有还没到的人。
	if (mApp->IsOnlineWaitingStartAck())
		return ModText::Tr("开始中——等待队友就位", "Starting - waiting for players");

	// 换位这件事有来有回，两种"等"得分开说：对面问我（面板会自动叫出来，
	// 但玩家也能把它关掉，关了就靠这行提醒），还是我在等对面回话。
	if (aSession->HasIncomingSwapRequest())
		return ModText::Tr("有人请求换位——打开面板", "Swap request - open the panel");
	if (aSession->IsSwapRequestPending())
		return ModText::Tr("换位请求已发出——等待中", "Swap asked - waiting");

	// 刚发生的事（换成了 / 被拒绝了）优先占几秒
	if (!aSession->GetNoticeText().empty())
		return aSession->GetNoticeText();

	switch (aSession->GetState())
	{
	case NetSession::State::LISTENING:
		// 队伍里只有我一个人也能开局（单人闯关），所以这行的重点不是"等人"，
		// 而是"下一步点哪"——主位那块烤字 ADVENTURE 的大墓碑就是闯关入口。
		return ModText::Tr("已建房——点大墓碑开始闯关", "Hosting - click Adventure to start");

	case NetSession::State::CONNECTING:
		{
			// 连不上会一直重试，重试次数得露出来，不然"还在试"看着和"卡死了"一样。
			// 中继下连的是服务器不是对面那台机器，说法得区分开（失败原因也完全是两码事）。
			int anAttempts = aSession->GetConnectAttempts();
			std::string aText = aSession->IsRelay()
				? ModText::Tr("正在连接服务器", "Connecting to server")
				: ModText::Tr("正在连接", "Connecting");
			if (anAttempts > 1)
				aText += ModText::Tr("（第 ", " (attempt ")
					+ std::to_string((unsigned)anAttempts) + ModText::Tr(" 次重试）", ")");
			return aText;
		}

	case NetSession::State::HANDSHAKING:
		// 中继的握手是等服务器点名（建房 / 加入的回音），不是和对面互通姓名
		if (aSession->IsRelay())
			return (aSession->GetRole() == NetSession::Role::HOST)
				? ModText::Tr("正在创建房间", "Creating room")
				: ModText::Tr("正在加入房间", "Joining room");
		return ModText::Tr("握手中", "Handshaking");

	case NetSession::State::CONNECTED:
		// 构建代次不同也能玩（12 起不再拒连，见 NetSession 握手处），但得一直看得见——
		// 等真撞上不配套的行为再想起来"该更新了"就晚了
		if (aSession->IsBuildDifferent())
			return ModText::Tr("构建版本不同——已连接", "Builds differ - connected");
		return (aSession->GetRole() == NetSession::Role::HOST)
			? ModText::Tr("主机——请选关卡", "Host - pick a level")
			: ModText::Tr("客户端——等待主机", "Client - waiting for host");

	case NetSession::State::DEAD:
		// 死因用会话给的短标签：版本/构建不符这类"要你动手换包"的原因，
		// 在这块小条上不能显示成和"网线掉了"一样的话
		return aSession->GetShortStatus();

	case NetSession::State::OFF:
	default:
		return "";
	}
}
