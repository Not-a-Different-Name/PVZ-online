#include "OnlineDialog.h"
#include "GameButton.h"
#include "../LawnCommon.h"
#include "../Online/NetSession.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "graphics/Font.h"
#include "widget/WidgetManager.h"

// LawnCommon.cpp 里那张输入框配色表（CreateEditWidget 用的就是它）
extern int gLawnEditWidgetColors[][4];

namespace
{
	const int	EDIT_HEIGHT		= 28;
	const int	ROW_GAP			= 10;	// 状态行→输入框、输入框→按钮之间的留白
	// 标签那一格：框体美术从输入框 mX 再往左画 8px（DrawEditBox），标签又是在框体之前画的，
	// 所以标签得在 mX-8 之前收尾。15 号字体实测 "Host IP"=67px、"Server"=53px、"Code"=38px，
	// 配上 +4 的起始偏移，宽度至少 79 / 65 / 50 —— 56 和 42 会让字尾被框体切掉（"Host"/"Cod"）。
	const int	LABEL_WIDTH		= 84;	// 输入框左边留给 "Host IP" / "Server" 标签的宽度
	const int	CODE_LABEL_WIDTH = 54;	// 中继那行 "Code" 标签
	const int	CODE_EDIT_WIDTH	= 66;	// 房间码框：4 个字符 + 光标
	const int	COL_GAP			= 8;

	// EditListener::AllowChar 在这个框架里是**注释掉的**（EditListener.h 里那几条虚函数没开，
	// EditWidget::KeyChar 里调用它的那行也注释着），所以"这个输入框收哪些字符"挂不到 listener 上；
	// 每敲一个键必过的只剩 LawnEditWidget::KeyChar 这条虚函数——过滤只能挂在这儿。
	class OnlineEditWidget : public LawnEditWidget
	{
	public:
		enum Filter
		{
			FILTER_HOST,	// 地址：字母数字 + . -（IPv4 / 主机名）
			FILTER_CODE		// 房间码：字母数字，一律折成大写（服务器那边也是这么归一化的）
		};

		OnlineEditWidget(int theId, EditListener* theListener, Dialog* theDialog, Filter theFilter)
			: LawnEditWidget(theId, theListener, theDialog), mFilter(theFilter)
		{
			// 地址和房间码都没有"首字母大写"这回事——地址给大写了就找不着主机
			mAutoCapFirstLetter = false;
		}

		virtual void KeyChar(char theChar)
		{
			if (mFilter == FILTER_CODE && theChar >= 'a' && theChar <= 'z')
				theChar = (char)(theChar - 'a' + 'A');
			if (!Allows(theChar)) return;
			LawnEditWidget::KeyChar(theChar);
		}

	private:
		bool Allows(char theChar) const
		{
			bool anAlnum = (theChar >= '0' && theChar <= '9')
				|| (theChar >= 'A' && theChar <= 'Z')
				|| (theChar >= 'a' && theChar <= 'z');
			if (mFilter == FILTER_CODE) return anAlnum;
			return anAlnum || theChar == '.' || theChar == '-';
		}

		Filter mFilter;
	};

	LawnEditWidget* CreateOnlineEditWidget(int theId, EditListener* theListener, Dialog* theDialog,
		OnlineEditWidget::Filter theFilter)
	{
		OnlineEditWidget* aWidget = new OnlineEditWidget(theId, theListener, theDialog, theFilter);
		// 和 CreateEditWidget 同一套外观（字体/配色/光标闪烁），只是多了字符过滤
		aWidget->SetFont(Sexy::FONT_BRIANNETOD16);
		aWidget->SetColors(gLawnEditWidgetColors, EditWidget::NUM_COLORS);
		aWidget->mBlinkDelay = 14;
		return aWidget;
	}
}

OnlineDialog::OnlineDialog(LawnApp* theApp) :
	LawnDialog(theApp, Dialogs::DIALOG_ONLINE, true, _S("CO-OP ONLINE"), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
	mApp = theApp;
	mVerticalCenterText = false;

	mHostButton = MakeButton(OnlineDialog::OnlineDialog_Host, this, _S("Host"));
	mJoinButton = MakeButton(OnlineDialog::OnlineDialog_Join, this, _S("Join"));
	mCloseButton = MakeButton(OnlineDialog::OnlineDialog_Close, this, _S("Close"));
	mDisconnectButton = MakeButton(OnlineDialog::OnlineDialog_Disconnect, this, _S("Disconnect"));
	mSwapButton = MakeButton(OnlineDialog::OnlineDialog_Swap, this, _S("Swap"));
	mAcceptButton = MakeButton(OnlineDialog::OnlineDialog_Accept, this, _S("Accept"));
	mRejectButton = MakeButton(OnlineDialog::OnlineDialog_Reject, this, _S("Reject"));
	mCreateRoomButton = MakeButton(OnlineDialog::OnlineDialog_CreateRoom, this, _S("Create Room"));
	mJoinRoomButton = MakeButton(OnlineDialog::OnlineDialog_JoinRoom, this, _S("Join Room"));

	// 直连那格的过滤和 Server 同一套（原来挂的 AllowChar 是死钩子，从来没被调过）
	mIpEditWidget = CreateOnlineEditWidget(OnlineDialog::OnlineDialog_IpEdit, this, this,
		OnlineEditWidget::FILTER_HOST);
	mIpEditWidget->mMaxChars = 15;			// "255.255.255.255"
	mIpEditWidget->SetText(_S("127.0.0.1"), true);

	mServerEditWidget = CreateOnlineEditWidget(OnlineDialog::OnlineDialog_ServerEdit, this, this,
		OnlineEditWidget::FILTER_HOST);
	mServerEditWidget->mMaxChars = 15;
	mServerEditWidget->SetText(_S("127.0.0.1"), true);	// P4 部署后默认填云服务器地址

	mCodeEditWidget = CreateOnlineEditWidget(OnlineDialog::OnlineDialog_CodeEdit, this, this,
		OnlineEditWidget::FILTER_CODE);
	mCodeEditWidget->mMaxChars = NetProto::ROOM_CODE_LEN;

	mStatusLine = "Not connected.";
	mHintLine = "";
	mLocalIpLoaded = false;

	// 比 CheatDialog 大一圈：两行状态 + 两行输入框 + 两排按钮（底排直连 / 上排中继）。
	// 面板高度由字体/按钮美术的实际高度推出来，不硬写常数：改字号或换按钮图都不会再互相压。
	//
	// 末尾减的两项是 CalcSize 自己还会加上去的：标题那一截（-ascentPadding + headerHeight + spaceAfterHeader）
	// 和固定的 mButtonHeight（24——标准 Dialog 按钮的高度；本面板的按钮是底下那排 LawnStoneButton 自己排的）。
	// 它们加、我们减，一加一减才刚好等于排版真正需要的高度——不减的话面板会高出约 90px，
	// 而按钮钉在面板底、输入框排在顶，多出来的那些全挤在 Server 行和 Create Room 那排之间。
	// 输入框只算两行：Server 和 Code 是同一行并排（见 Resize 里 aServerY 那一处）。
	int aBandTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	int aHeaderAllowance = -mHeaderFont->GetAscentPadding() + mHeaderFont->GetHeight() + mSpaceAfterHeader;
	int aHeight = (GetEditY() - aBandTop)
		+ EDIT_HEIGHT * 2 + ROW_GAP					// Host IP / Server+Code 两行输入框
		+ ROW_GAP + IMAGE_BUTTON_LEFT->mHeight		// 中继那排：Create Room / Join Room
		+ ROW_GAP + IMAGE_BUTTON_LEFT->mHeight		// 底排：Host / Join / Close（及叠加键）
		- 2
		- aHeaderAllowance - mButtonHeight;
	CalcSize(400, aHeight);		// 比直连时代宽一圈：Server+Code 一行两栏放得下
}

OnlineDialog::~OnlineDialog()
{
	delete mHostButton;
	delete mJoinButton;
	delete mCloseButton;
	delete mDisconnectButton;
	delete mSwapButton;
	delete mAcceptButton;
	delete mRejectButton;
	delete mCreateRoomButton;
	delete mJoinRoomButton;
	delete mIpEditWidget;
	delete mServerEditWidget;
	delete mCodeEditWidget;
}

void OnlineDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);

	int aLeft = mContentInsets.mLeft + mBackgroundInsets.mLeft;
	int anInnerWidth = mWidth - mContentInsets.mLeft - mContentInsets.mRight - mBackgroundInsets.mLeft - mBackgroundInsets.mRight;

	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;
	int aButtonY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	int aButtonGap = 8;
	int aButtonWidth = (anInnerWidth - aButtonGap * 2) / 3;

	mHostButton->Resize(aLeft, aButtonY, aButtonWidth, aButtonHeight);
	mJoinButton->Resize(aLeft + aButtonWidth + aButtonGap, aButtonY, aButtonWidth, aButtonHeight);
	mCloseButton->Resize(aLeft + (aButtonWidth + aButtonGap) * 2, aButtonY, aButtonWidth, aButtonHeight);
	// 连接中它顶掉 Host 那一格：同一时刻 Host 本来就是灰的，占着地方也是白占
	mDisconnectButton->Resize(aLeft, aButtonY, aButtonWidth, aButtonHeight);
	// Swap 占中间那格：会话活着的时候 Join 藏着，正好空出来
	mSwapButton->Resize(aLeft + aButtonWidth + aButtonGap, aButtonY, aButtonWidth, aButtonHeight);
	// 对面问换位时这两把键顶上前两格（Disconnect / Swap 这时都藏着）
	mAcceptButton->Resize(aLeft, aButtonY, aButtonWidth, aButtonHeight);
	mRejectButton->Resize(aLeft + aButtonWidth + aButtonGap, aButtonY, aButtonWidth, aButtonHeight);

	// 中继那排紧贴底排上方，两栏分：按钮行和它上面那行 Server/Code 输入框对齐
	int aRelayButtonY = aButtonY - ROW_GAP - aButtonHeight;
	int aHalfWidth = (anInnerWidth - aButtonGap) / 2;
	mCreateRoomButton->Resize(aLeft, aRelayButtonY, aHalfWidth, aButtonHeight);
	mJoinRoomButton->Resize(aLeft + aHalfWidth + aButtonGap, aRelayButtonY, aHalfWidth, aButtonHeight);

	int anEditWidth = anInnerWidth - LABEL_WIDTH;
	if (anEditWidth > 300) anEditWidth = 300;
	mIpEditWidget->Resize(aLeft + LABEL_WIDTH, GetEditY(), anEditWidth, EDIT_HEIGHT);

	// 中继那行：Server 框吃掉剩下的宽度，Code 框固定短一条挂在右边（房间码就 4 格）
	int aServerY = GetEditY() + EDIT_HEIGHT + ROW_GAP;
	int aServerWidth = anEditWidth - COL_GAP - CODE_LABEL_WIDTH - CODE_EDIT_WIDTH;
	mServerEditWidget->Resize(aLeft + LABEL_WIDTH, aServerY, aServerWidth, EDIT_HEIGHT);
	mCodeEditWidget->Resize(aLeft + LABEL_WIDTH + aServerWidth + COL_GAP + CODE_LABEL_WIDTH,
		aServerY, CODE_EDIT_WIDTH, EDIT_HEIGHT);
}

void OnlineDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	AddWidget(mHostButton);
	AddWidget(mJoinButton);
	AddWidget(mCloseButton);
	AddWidget(mDisconnectButton);
	AddWidget(mSwapButton);
	AddWidget(mAcceptButton);
	AddWidget(mRejectButton);
	AddWidget(mCreateRoomButton);
	AddWidget(mJoinRoomButton);
	AddWidget(mIpEditWidget);
	AddWidget(mServerEditWidget);
	AddWidget(mCodeEditWidget);
	theWidgetManager->SetFocus(mIpEditWidget);
}

void OnlineDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	RemoveWidget(mHostButton);
	RemoveWidget(mJoinButton);
	RemoveWidget(mCloseButton);
	RemoveWidget(mDisconnectButton);
	RemoveWidget(mSwapButton);
	RemoveWidget(mAcceptButton);
	RemoveWidget(mRejectButton);
	RemoveWidget(mCreateRoomButton);
	RemoveWidget(mJoinRoomButton);
	RemoveWidget(mIpEditWidget);
	RemoveWidget(mServerEditWidget);
	RemoveWidget(mCodeEditWidget);
}

void OnlineDialog::Update()
{
	LawnDialog::Update();

	NetSession* aSession = mApp->mOnlineSession;
	// 死了的会话（掉线 / 版本不符被拒）按"没连着"对待：Host/Join 直接给回来，
	// 玩家不用先按一下 Disconnect 才能重开。死因不会因此丢掉——状态行照旧读
	// GetStatusText()，小条也还把短标签挂着，直到下一次 Start 把会话复位。
	// （ResetToOff 现在会把 NetLink 收干净，所以从这里重开局是通的，见 T19。）
	bool anActive = aSession && aSession->IsActive()
		&& aSession->GetState() != NetSession::State::DEAD;

	// 面板是遥控器，不显示连接细节：状态行每帧跟着会话走
	if (aSession)
	{
		mStatusLine = aSession->GetStatusText();
		mHintLine = aSession->GetHintText();
	}
	else
	{
		mStatusLine = "Not connected.";
		mHintLine = "Host or join a team, then click Adventure.";
	}

	// 中继：房间码一回来就写进 Code 框。房主那格是自己没有的（由服务器生成），
	// 面板上是"我在哪个房间"最直白的一处；按码加入的人也能看到自己的码归一化后的样子。
	// 只在码真的换了的时候写一次——每帧都写会把玩家正在框里敲的字擦掉。
	if (aSession && aSession->IsRelay())
	{
		const std::string& aCode = aSession->GetRoomCode();
		if (!aCode.empty() && aCode != mShownRoomCode)
		{
			mShownRoomCode = aCode;
			mCodeEditWidget->SetText(aCode.c_str(), true);
		}
	}

	// 主机按了关卡、正等队友就位：会话状态还是 CONNECTED，得单独说一句在等什么，
	// 不然状态行还写着 "Pick a level from the menu"，看着像那一下没点上。
	// 说法不点"那一个队友"：四席位时等的是所有人（ACK 全部到齐才进场，见 START_ACK）。
	if (mApp->IsOnlineWaitingStartAck())
		mStatusLine = "Starting - waiting for everyone to get ready.";

	mHostButton->SetDisabled(anActive);
	mJoinButton->SetDisabled(anActive);
	// 中继这两把键没有"顶掉它们"的叠加键，就连着会话一直摆着、只是灰掉：
	// 面板的排面不会因为连上而跳一下。
	mCreateRoomButton->SetDisabled(anActive);
	mJoinRoomButton->SetDisabled(anActive);
	// 会话活着的时候：Host/Join 让位给 Disconnect，Close 就只是关面板
	mHostButton->mVisible = !anActive;
	mJoinButton->mVisible = !anActive;

	// 对面问换位时前两格换成同意/拒绝：这个问题得玩家自己回答。面板是模态的
	// （别处点不着），但主循环照跑——等答复不会把心跳等断掉。
	bool anAsk = aSession && aSession->HasIncomingSwapRequest();
	mDisconnectButton->mVisible = anActive && !anAsk;
	mSwapButton->mVisible = anActive && !anAsk;
	mAcceptButton->mVisible = anAsk;
	mRejectButton->mVisible = anAsk;

	// 换位置只在自己这局开局前有意义：棋盘一开（或主机已在等队友就位），
	// 顺位就定下了，按钮灰掉免得中途换了位置两边都糊涂。
	// 已经有一条请求在谈（我等的 / 等我的）时也不许再发——一次只谈一件事。
	bool aCanSwap = aSession && aSession->IsConnected() && !anAsk
		&& !aSession->IsSwapRequestPending()
		&& mApp->mBoard == nullptr && !mApp->IsOnlineWaitingStartAck();
	mSwapButton->SetDisabled(!aCanSwap);
}

void OnlineDialog::Draw(Graphics* g)
{
	LawnDialog::Draw(g);

	g->SetFont(mLinesFont);
	g->SetColor(mColors[Dialog::COLOR_LINES]);

	int aLineY = GetStatusBaseline();
	WriteCenteredLine(g, aLineY, mStatusLine);
	WriteCenteredLine(g, aLineY + mLinesFont->GetLineSpacing(), mHintLine);

	DrawRoomBlock(g);

	g->SetColor(mColors[Dialog::COLOR_LINES]);		// 块里按行换过色，标签的颜色得放回来
	g->DrawString(_S("Host IP"), mIpEditWidget->mX - LABEL_WIDTH + 4, mIpEditWidget->mY + mLinesFont->GetAscent());
	DrawEditBox(g, mIpEditWidget);

	g->DrawString(_S("Server"), mServerEditWidget->mX - LABEL_WIDTH + 4, mServerEditWidget->mY + mLinesFont->GetAscent());
	DrawEditBox(g, mServerEditWidget);

	g->DrawString(_S("Code"), mCodeEditWidget->mX - CODE_LABEL_WIDTH + 4, mCodeEditWidget->mY + mLinesFont->GetAscent());
	DrawEditBox(g, mCodeEditWidget);
}

// @pvz-online: 房间信息块——"我现在在哪个房间"一眼看全：房间码（中继）/ 主机地址（直连）、
// 我坐第几席、谁是房主，下面是 P1..P4 名册。名册和小状态条那份是同一套说法
// （自己那行 (you)、空位 --）：面板是模态的、盖着小条看不见，所以这里再写一份。
void OnlineDialog::DrawRoomBlock(Graphics* g)
{
	NetSession* aSession = mApp->mOnlineSession;
	bool anActive = aSession != nullptr && aSession->IsActive();

	int aLeft = mContentInsets.mLeft + mBackgroundInsets.mLeft + 4;	// 和 "Host IP" 那些标签同一个起点
	int aLineHeight = mLinesFont->GetLineSpacing();
	int aLineY = GetRoomHeaderBaseline();

	g->SetColor(mColors[Dialog::COLOR_LINES]);
	g->DrawString(GetRoomHeaderLine(), aLeft, aLineY);
	if (!anActive)
		return;		// 没房间：名册那四行的地盘空着（位置钉死，面板不会因为连上而跳）

	for (int aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		aLineY += aLineHeight;
		if (aSession->GetLocalSeat() == aSeat)
			g->SetColor(Color(255, 255, 255, 255));			// 自己那行最亮
		else if (aSession->IsSeatOccupied((uint8_t)aSeat))
			g->SetColor(mColors[Dialog::COLOR_LINES]);		// 队友：和状态行同色
		else
			g->SetColor(Color(122, 112, 96, 255));			// 空位压暗
		g->DrawString(GetRoomSeatLine(aSeat), aLeft, aLineY);
	}
}

// 块的第一行：**房间码**是"我在哪个房间"的唯一凭据——房主要念给朋友、队友要核对进对没有，
// 所以摆在最显眼处。直连没有房间码，"房间"就是主机那台机器：房主顺带念出自己的地址
// （面板是模态的，盖着主菜单那个小条，看不见）。
std::string OnlineDialog::GetRoomHeaderLine()
{
	NetSession* aSession = mApp->mOnlineSession;
	std::string aText = "Room ----";	// 没会话 / 中继还没等到服务器点名，都是这一句
	if (!aSession || !aSession->IsActive())
		return aText;

	uint8_t aSeat = aSession->GetLocalSeat();
	if (aSeat == NetProto::SEAT_UNSET)
		return aText;

	if (aSession->IsRelay())
	{
		std::string aCode = aSession->GetRoomCode();
		if (!aCode.empty())
			aText = "Room " + aCode;
	}
	else
	{
		// 枚举网卡不是每帧该干的活：算一次存着（会话开着这段时间地址不会变）
		if (!mLocalIpLoaded)
		{
			mLocalIpText = NetLink::GetLocalIPv4Text();
			mLocalIpLoaded = true;
		}
		aText = "Direct";
		if (aSession->IsHostSeat() && !mLocalIpText.empty())
			aText += " " + mLocalIpText;
	}

	aText += " - you are P" + std::to_string((int)aSeat);
	if (aSession->IsHostSeat())
		aText += " (host)";
	return aText;
}

// 名册的一行，和小状态条同一套说法：空位就是 --（别看名字那栏是空的就以为是没画出来），
// 名字一个能画的字形都没有时直说 (no name)。多一个 (host)——面板上得看出房主是谁。
std::string OnlineDialog::GetRoomSeatLine(int theSeat)
{
	NetSession* aSession = mApp->mOnlineSession;
	std::string aText = "P" + std::to_string(theSeat);
	if (!aSession) return aText + "  --";

	bool aMine = (aSession->GetLocalSeat() == theSeat);
	if (!aSession->IsSeatOccupied((uint8_t)theSeat))
		return aText + "  --";

	std::string aName = aSession->GetSeatName((uint8_t)theSeat);
	if (aName.empty() && !aMine)
		aName = "(no name)";
	if (!aName.empty())
		aText += "  " + aName;

	std::string aTag;
	if (aSession->GetHostSeat() == theSeat && aSession->GetHostSeat() != NetProto::SEAT_UNSET)
		aTag = "host";
	if (aMine)
		aTag = aTag.empty() ? "you" : aTag + ", you";
	if (!aTag.empty())
		aText += " (" + aTag + ")";
	return aText;
}

// 标题栏下沿第一行正文的基线。LawnDialog::Draw 画完标题后正是用这套算式继续排正文的
// （见 LawnDialog.cpp 里 aFontY 那几行）；这里原来直接拿 DIALOG_HEADER_OFFSET 当基线用，
// 那正好是标题自己的基线，于是两行状态就压在标题上了。
int OnlineDialog::GetStatusBaseline()
{
	return mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET
		- mHeaderFont->GetAscentPadding() + mHeaderFont->GetHeight() + mSpaceAfterHeader;
}

// 两行状态的正下方：先让开 ROW_GAP，再排房间信息块（标题一行 + 四个席位四行）。
// 块的下面是输入框，同样让开 ROW_GAP。
// Draw / Resize / 构造里的面板高度全走这两个式子，免得三处各算各的又算岔。
int OnlineDialog::GetRoomHeaderBaseline()
{
	return GetStatusBaseline() + mLinesFont->GetLineSpacing() * 2 + ROW_GAP;
}

int OnlineDialog::GetEditY()
{
	const int aRoomBlockLines = 5;		// 标题 + P1..P4

	return GetRoomHeaderBaseline() + mLinesFont->GetLineSpacing() * (aRoomBlockLines - 1)
		- mLinesFont->GetAscent() + ROW_GAP;
}

void OnlineDialog::ButtonDepress(int theId)
{
	switch (theId)
	{
	case OnlineDialog::OnlineDialog_Host:
		StartHost();
		break;

	case OnlineDialog::OnlineDialog_Join:
		StartJoin();
		break;

	case OnlineDialog::OnlineDialog_Close:
		// 关面板不动连接：连接活在会话里，面板只是遥控器
		mApp->KillDialog(Dialogs::DIALOG_ONLINE);
		break;

	case OnlineDialog::OnlineDialog_Disconnect:
		if (mApp->mOnlineSession)
		{
			mApp->mOnlineSession->Close();
		}
		break;

	case OnlineDialog::OnlineDialog_Swap:
		// 只是把请求发出去：换不换由对面点头，点头了才两边一起换。
		// 发不出去（没连上、已经有一条在谈）就什么都没发生，按钮下一帧照样灰着。
		if (mApp->mOnlineSession)
		{
			mApp->mOnlineSession->SwapSeats();
		}
		break;

	case OnlineDialog::OnlineDialog_Accept:
		if (mApp->mOnlineSession)
		{
			mApp->mOnlineSession->AnswerSwapRequest(true);
		}
		break;

	case OnlineDialog::OnlineDialog_Reject:
		if (mApp->mOnlineSession)
		{
			mApp->mOnlineSession->AnswerSwapRequest(false);
		}
		break;

	case OnlineDialog::OnlineDialog_CreateRoom:
		StartRoomHost();
		break;

	case OnlineDialog::OnlineDialog_JoinRoom:
		StartRoomJoin();
		break;
	}
}

// 回车 = 这一格对应的那把键：地址框回车走直连 Join（老行为），房间码框回车走 Join Room。
// 服务器地址框回车也走 Join Room——输完地址顺手回车是最顺的动作，此时码多半也填好了。
void OnlineDialog::EditWidgetText(int theId, const SexyString& theString)
{
	(void)theString;
	switch (theId)
	{
	case OnlineDialog::OnlineDialog_ServerEdit:
	case OnlineDialog::OnlineDialog_CodeEdit:
		StartRoomJoin();
		break;

	case OnlineDialog::OnlineDialog_IpEdit:
	default:
		StartJoin();
		break;
	}
}

std::string OnlineDialog::GetIpText()
{
	std::string anIp = mIpEditWidget->mString;
	if (anIp.empty()) anIp = "127.0.0.1";
	return anIp;
}

std::string OnlineDialog::GetServerText()
{
	std::string aServer = mServerEditWidget->mString;
	if (aServer.empty()) aServer = "127.0.0.1";
	return aServer;
}

std::string OnlineDialog::GetRoomCodeText()
{
	return mCodeEditWidget->mString;
}

void OnlineDialog::StartHost()
{
	if (!mApp->mOnlineSession) return;
	mApp->mOnlineSession->StartHost();
}

void OnlineDialog::StartJoin()
{
	if (!mApp->mOnlineSession) return;
	mApp->mOnlineSession->StartJoin(GetIpText().c_str());
}

void OnlineDialog::StartRoomHost()
{
	if (!mApp->mOnlineSession) return;
	mApp->mOnlineSession->StartRoomHost(GetServerText().c_str());
}

void OnlineDialog::StartRoomJoin()
{
	if (!mApp->mOnlineSession) return;
	// 码不到 4 位会话自己会回绝并写明原因（状态行照出来），这里不用先拦一道
	mApp->mOnlineSession->StartRoomJoin(GetServerText().c_str(), GetRoomCodeText().c_str());
}
