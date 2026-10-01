#include "OnlineDialog.h"
#include "GameButton.h"
#include "../LawnCommon.h"
#include "../Online/NetSession.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "graphics/Font.h"
#include "widget/WidgetManager.h"

namespace
{
	const int	EDIT_HEIGHT		= 28;
	const int	ROW_GAP			= 10;	// 状态行→输入框、输入框→按钮之间的留白
	const int	LABEL_WIDTH		= 56;	// 输入框左边留给 "Host IP" 标签的宽度
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

	mIpEditWidget = CreateEditWidget(OnlineDialog::OnlineDialog_IpEdit, this, this);
	mIpEditWidget->mMaxChars = 15;			// "255.255.255.255"
	mIpEditWidget->SetText(_S("127.0.0.1"), true);

	mStatusLine = "Not connected.";
	mHintLine = "";

	// 比 CheatDialog 大一圈：两行状态 + 输入框 + 三个按钮。
	// 面板高度由字体/按钮美术的实际高度推出来，不硬写常数：改字号或换按钮图都不会再互相压。
	int aBandTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	int aHeight = (GetEditY() - aBandTop) + EDIT_HEIGHT + ROW_GAP + IMAGE_BUTTON_LEFT->mHeight - 2;
	CalcSize(320, aHeight);
}

OnlineDialog::~OnlineDialog()
{
	delete mHostButton;
	delete mJoinButton;
	delete mCloseButton;
	delete mDisconnectButton;
	delete mSwapButton;
	delete mIpEditWidget;
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

	int anEditWidth = anInnerWidth - LABEL_WIDTH;
	if (anEditWidth > 300) anEditWidth = 300;
	mIpEditWidget->Resize(aLeft + LABEL_WIDTH, GetEditY(), anEditWidth, EDIT_HEIGHT);
}

void OnlineDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	AddWidget(mHostButton);
	AddWidget(mJoinButton);
	AddWidget(mCloseButton);
	AddWidget(mDisconnectButton);
	AddWidget(mSwapButton);
	AddWidget(mIpEditWidget);
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
	RemoveWidget(mIpEditWidget);
}

void OnlineDialog::Update()
{
	LawnDialog::Update();

	NetSession* aSession = mApp->mOnlineSession;
	bool anActive = aSession && aSession->IsActive();

	// 面板是遥控器，不显示连接细节：状态行每帧跟着会话走
	if (aSession)
	{
		mStatusLine = aSession->GetStatusText();
		mHintLine = aSession->GetHintText();
	}
	else
	{
		mStatusLine = "Not connected.";
		mHintLine = "Host a game, or type the host's IP and join.";
	}

	// 主机按了关卡、正等队友就位：会话状态还是 CONNECTED，得单独说一句在等什么，
	// 不然状态行还写着 "Pick a level from the menu"，看着像那一下没点上。
	if (mApp->IsOnlineWaitingStartAck())
		mStatusLine = "Starting - waiting for the teammate to get ready.";

	mHostButton->SetDisabled(anActive);
	mJoinButton->SetDisabled(anActive);
	// 会话活着的时候：Host/Join 让位给 Disconnect，Close 就只是关面板
	mHostButton->mVisible = !anActive;
	mJoinButton->mVisible = !anActive;
	mDisconnectButton->mVisible = anActive;
	mSwapButton->mVisible = anActive;

	// 换位置只在自己这局开局前有意义：棋盘一开（或主机已在等队友就位），
	// 顺位就定下了，按钮灰掉免得中途换了位置两边都糊涂。
	bool aCanSwap = aSession && aSession->IsConnected()
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

	g->DrawString(_S("Host IP"), mIpEditWidget->mX - LABEL_WIDTH + 4, mIpEditWidget->mY + mLinesFont->GetAscent());
	DrawEditBox(g, mIpEditWidget);
}

// 标题栏下沿第一行正文的基线。LawnDialog::Draw 画完标题后正是用这套算式继续排正文的
// （见 LawnDialog.cpp 里 aFontY 那几行）；这里原来直接拿 DIALOG_HEADER_OFFSET 当基线用，
// 那正好是标题自己的基线，于是两行状态就压在标题上了。
int OnlineDialog::GetStatusBaseline()
{
	return mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET
		- mHeaderFont->GetAscentPadding() + mHeaderFont->GetHeight() + mSpaceAfterHeader;
}

// 两行状态的正下方再让开 ROW_GAP，就是输入框的上沿。
// Draw / Resize / 构造里的面板高度全走这一个式子，免得三处各算各的又算岔。
int OnlineDialog::GetEditY()
{
	return GetStatusBaseline() + mLinesFont->GetLineSpacing() * 2 - mLinesFont->GetAscent() + ROW_GAP;
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
		// 发不出去就谁都不换（会话内部先发后换），面板这边不用管结果：
		// 换成了下一帧的提示行就是新的。
		if (mApp->mOnlineSession)
		{
			mApp->mOnlineSession->SwapSeats();
		}
		break;
	}
}

void OnlineDialog::EditWidgetText(int theId, const SexyString& theString)
{
	(void)theId;(void)theString;
	StartJoin();
}

bool OnlineDialog::AllowChar(int theId, SexyChar theChar)
{
	(void)theId;
	return sexyisdigit(theChar) || theChar == _S('.');
}

std::string OnlineDialog::GetIpText()
{
	std::string anIp = mIpEditWidget->mString;
	if (anIp.empty()) anIp = "127.0.0.1";
	return anIp;
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
