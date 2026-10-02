#include "NetSession.h"

#include <cstring>
#include <utility>

#include "../../Sexy.TodLib/TodDebug.h"

namespace
{

const int	HELLO_PAYLOAD_SIZE		= 6 + NetProto::NAME_SIZE;		// src, dst, u16 version, u16 build, 名字
const int	HELLO_ACK_PAYLOAD_SIZE	= 7 + NetProto::NAME_SIZE;		// src, dst, u16 version, u16 build, u8 accepted, 名字
const int	START_LEVEL_PAYLOAD_SIZE = 17;	// src, dst, u8 mode, u32 level, i32 seed, u8 isRun, i32 runSeed, u8 runLevelIndex
const int	START_ACK_PAYLOAD_SIZE	= 3;	// src, dst, u8 accepted
const int	RUN_GO_PAYLOAD_SIZE		= 2;	// src, dst
const int	SEEDS_READY_PAYLOAD_SIZE = 3;	// src, dst, u8 ready
const int	SWAP_REQUEST_PAYLOAD_SIZE = 2;	// src, dst
const int	SWAP_REPLY_PAYLOAD_SIZE = 3;	// src, dst, u8 accepted
const int	ESCAPED_ZOMBIE_PAYLOAD_SIZE = 22;	// src, dst, u8 row, u16 type, u8 flags, i32 ×4 血量
const int	HEARTBEAT_PAYLOAD_SIZE	= 6;	// src, dst, u32 tick
const int	BYE_PAYLOAD_SIZE		= 3;	// src, dst, u8 reason
const int	LEVEL_EXIT_PAYLOAD_SIZE	= 3;	// src, dst, u8 reason
const int	PAUSE_PAYLOAD_SIZE		= 3;	// src, dst, u8 paused
const int	LEVEL_DONE_PAYLOAD_SIZE	= 3;	// src, dst, u8 done
const int	GAME_OVER_PAYLOAD_SIZE	= 3;	// src, dst, u8 reason

// 名字只留可打印 ASCII：位图字体没有别的字形，画出来只能是空白或乱码；何况这是对面
// 发来的东西，控制字符更不能原样进绘制。剔掉而不是截断——"Alice玩家" 至少还认得出 Alice。
std::string SanitizeName(const char* theName, int theMaxBytes)
{
	std::string aResult;
	if (!theName) return aResult;

	for (int i = 0; i < theMaxBytes && theName[i]; i++)
	{
		unsigned char aChar = (unsigned char)theName[i];
		if (aChar >= 32 && aChar < 127) aResult += (char)aChar;
	}
	return aResult;
}

}

NetSession::NetSession()
{
	mRole = Role::NONE;
	mTransport = Transport::DIRECT;
	mState = State::OFF;
	mLocalSeat = NetProto::SEAT_UNSET;
	ClearSeatTable();
	mFramesSincePacket = 0;
	mFramesSinceHeartbeat = 0;
	mHeartbeatTick = 0;
	mConnectPort = NetProto::DEFAULT_PORT;
	mStatusText = "Not connected.";
	mHintText = "Host a game, or type the host's IP and join.";
	mShortStatus = "Connection lost";
	mHasPendingStart = false;
	mHasRunGo = false;
	mHasPendingLevelExit = false;
	mAllDoneTaken = false;
	mHasPendingGameOver = false;
	mSharedPaused = false;
	mPauseCameFromPeer = false;
	mHasPendingPause = false;
	mPendingPauseValue = false;
	mSwapRequestSeat = NetProto::SEAT_UNSET;
	mSwapAskSeat = NetProto::SEAT_UNSET;
	mNoticeFrames = 0;
}

NetSession::~NetSession()
{
	// 析构不再发 BYE：对端收不到也只是多等 5 秒心跳超时，和崩溃退出一个待遇。
	mLink.Close();
}

// 席位表整个回到"一个人都没有"。名册和每席位的记账是一体的，一起清才不会有
// "人走了但状态还挂着"的残影。
void NetSession::ClearSeatTable()
{
	for (int aSeat = 0; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		mSeats[aSeat].mOccupied = false;
		mSeats[aSeat].mBuild = 0;
		mSeats[aSeat].mName.clear();
		mSeats[aSeat].mLevelDone = false;
		mSeats[aSeat].mSeedsReady = false;
		mSeats[aSeat].mAckState = 0;
		mSeats[aSeat].mAwaitingAck = false;
	}
}

uint8_t NetSession::DirectPeerSeat() const
{
	// 直连就两个固定席位：主机 1、客户端 2。这个映射跟连没连上无关——
	// 客户端 TCP 一通就要发 HELLO，那时名册上还没有人。
	if (mLocalSeat == NetProto::SEAT_HOST) return NetProto::SEAT_CLIENT;
	if (mLocalSeat == NetProto::SEAT_CLIENT) return NetProto::SEAT_HOST;
	return NetProto::SEAT_UNSET;
}

// ====================================================================================================
// ★ 对外接口
// ====================================================================================================

bool NetSession::StartHost(uint16_t thePort)
{
	ResetToOff();
	mRole = Role::HOST;
	mTransport = Transport::DIRECT;
	mLocalSeat = NetProto::SEAT_HOST;

	if (!mLink.Listen(thePort))
	{
		mState = State::DEAD;
		mStatusText = mLink.GetLastError();
		mHintText.clear();
		PushEvent(EventType::DISCONNECTED);
		return false;
	}

	mState = State::LISTENING;
	mHintText = "Your IP: " + NetLink::GetLocalIPv4Text() + "   Port: " + std::to_string((unsigned)thePort);
	UpdateStatusText();
	return true;
}

bool NetSession::StartJoin(const char* theHost, uint16_t thePort)
{
	ResetToOff();
	mRole = Role::CLIENT;
	mTransport = Transport::DIRECT;
	mLocalSeat = NetProto::SEAT_CLIENT;
	mConnectHost = (theHost && theHost[0]) ? theHost : "127.0.0.1";
	mConnectPort = thePort;

	if (!mLink.Connect(mConnectHost.c_str(), thePort))
	{
		mState = State::DEAD;
		mStatusText = mLink.GetLastError();
		mHintText.clear();
		PushEvent(EventType::DISCONNECTED);
		return false;
	}

	mState = State::CONNECTING;
	mHintText = "Port " + std::to_string((unsigned)thePort);
	UpdateStatusText();
	return true;
}

void NetSession::SetLocalName(const char* theName)
{
	// 截到定长字段放得下的长度：线上字段就这么大，留着更长的名字只会让两边看到的不一样
	mLocalName = SanitizeName(theName, NetProto::NAME_SIZE - 1);
}

std::string NetSession::GetSeatName(uint8_t theSeat) const
{
	if (theSeat == NetProto::SEAT_UNSET || theSeat > NetProto::MAX_PLAYERS) return std::string();
	if (theSeat == mLocalSeat) return mLocalName;
	return mSeats[theSeat].mName;
}

bool NetSession::IsSeatOccupied(uint8_t theSeat) const
{
	if (theSeat == NetProto::SEAT_UNSET || theSeat > NetProto::MAX_PLAYERS) return false;
	if (theSeat == mLocalSeat) return true;		// 建房/加入那一刻起，自己那席就有人了
	// 队友那席要真连上才算：还在等人进来的时候，名册上那个位子该是空的
	return mSeats[theSeat].mOccupied && IsConnected();
}

bool NetSession::HasOtherSeats() const
{
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (aSeat == mLocalSeat) continue;
		if (IsSeatOccupied(aSeat)) return true;
	}
	return false;
}

int NetSession::GetOccupiedSeatCount() const
{
	int aCount = 0;
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
		if (IsSeatOccupied(aSeat)) aCount++;
	return aCount;
}

uint8_t NetSession::NextOccupiedSeatInRing(uint8_t theSeat) const
{
	if (theSeat < 1 || theSeat > NetProto::MAX_PLAYERS) return NetProto::SEAT_UNSET;

	// 从后一位数起，绕一圈找第一个上座席位（跳过自己）。人都在的时候就是
	// "后一位"；中间空了谁，环自己把洞跳过去。
	for (int anOffset = 1; anOffset < NetProto::MAX_PLAYERS; anOffset++)
	{
		uint8_t aSeat = (uint8_t)(((theSeat - 1 + anOffset) % NetProto::MAX_PLAYERS) + 1);
		if (aSeat != mLocalSeat && IsSeatOccupied(aSeat)) return aSeat;
	}
	return NetProto::SEAT_UNSET;
}

uint8_t NetSession::NextOccupiedSeat(uint8_t theSeat) const
{
	if (theSeat < 1 || theSeat > NetProto::MAX_PLAYERS) return NetProto::SEAT_UNSET;

	// 顺着往后找，**不绕回**：链的末端就是"没有下一家"（末席漏怪 = 全队败）。
	for (uint8_t aSeat = (uint8_t)(theSeat + 1); aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (aSeat != mLocalSeat && IsSeatOccupied(aSeat)) return aSeat;
	}
	return NetProto::SEAT_UNSET;
}

// 直连兼容 getter（名册请用 GetSeatName 逐格问）：直连只有一个对端。没连上、对面还没报
// 构建号时是 0——跟 M2 一样（那时这个值只有握手收下之后才有内容）。
uint16_t NetSession::GetPeerBuild() const
{
	uint8_t aPeer = DirectPeerSeat();
	if (aPeer == NetProto::SEAT_UNSET || !IsSeatOccupied(aPeer)) return 0;
	return mSeats[aPeer].mBuild;
}

// 有席位的人报的构建代次跟本机不一样。直连就是那一个对端；M3 中继扩成整张名册
// （每人报一次，谁不配套都算）。没连上就没什么好提示的，返回假。
bool NetSession::IsBuildDifferent() const
{
	if (mState != State::CONNECTED) return false;

	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (aSeat == mLocalSeat || !IsSeatOccupied(aSeat)) continue;

		uint16_t aBuild = mSeats[aSeat].mBuild;
		if (aBuild != 0 && aBuild != NetProto::MOD_BUILD) return true;
	}
	return false;
}

void NetSession::Close()
{
	if (mState != State::OFF && mState != State::DEAD && mLink.IsConnected())
	{
		SendBye(NetProto::BYE_QUIT);
	}

	mLink.Close();
	ResetToOff();
	TodLog("[net] session closed");
}

void NetSession::Update()
{
	if (mState == State::OFF) return;

	// 死了就是死了，别再泵这个会话。SetDead 不动 NetLink，判死时链路往往还挂着
	// CONNECTED——继续往下走的话，下面"链路通了、还没握手"那段每帧都成立，会把
	// DEAD 顶回 HANDSHAKING：主机每帧打一行 "a peer connected, waiting for HELLO"，
	// 客户端每帧重发一次 HELLO，收到的包又判死一次，状态行就在死因和握手文案之间
	// 反复跳。（2026-10-02 实机：两侧构建版本不一致，日志被这两行刷了上千遍，
	// 两边版本一致时任何判死路径同样会中招。）
	// 复活只走玩家的手：面板上的 Host/Join → ResetToOff → 新会话。
	if (mState == State::DEAD) return;

	// 先收包再收尸：对端临关之前发的东西（比如 BYE）就在队列里，
	// 先判 link FAILED 的话，死因会被"连接被对面关了"这个笼统说法盖掉。
	NetLink::Packet aPacket;
	while (mLink.Poll(aPacket))
	{
		mFramesSincePacket = 0;
		HandlePacket(aPacket);
		if (mState == State::DEAD) return;
	}

	NetLink::State aLinkState = mLink.GetState();
	if (aLinkState == NetLink::State::FAILED)
	{
		SetDead(mLink.GetLastError());
		return;
	}

	if (aLinkState == NetLink::State::CONNECTED && mState != State::CONNECTED && mState != State::HANDSHAKING)
	{
		mState = State::HANDSHAKING;
		mFramesSincePacket = 0;
		if (mRole == Role::CLIENT)
		{
			// TCP 一通，客户端先报家门；主机那头只管等着收 HELLO
			SendHello();
		}
		else
		{
			TodLog("[net] a peer connected, waiting for HELLO");
		}
	}

	mFramesSincePacket++;

	// 握手阶段不掐表：TCP 通了就一直等对方的 HELLO，等多久都行——对端真走了 socket 层会报，
	// 玩家不想等了面板上的 Disconnect 也是现成的。这里原先有个 3 秒上限，已按需求取消。
	if (mState == State::CONNECTED)
	{
		if (++mFramesSinceHeartbeat >= HEARTBEAT_FRAMES)
		{
			mFramesSinceHeartbeat = 0;
			SendHeartbeat();
		}
		if (mFramesSincePacket > TIMEOUT_FRAMES)
		{
			SetDead("Connection timed out.");
			return;
		}
	}

	// 即时说明按帧倒计时，到点自己消失（状态行每帧重算，不用另外触发重画）
	if (mNoticeFrames > 0 && --mNoticeFrames == 0)
		mNoticeText.clear();

	UpdateStatusText();
}

bool NetSession::PollEvent(Event& theEvent)
{
	if (mEvents.empty()) return false;

	theEvent = mEvents.front();
	mEvents.erase(mEvents.begin());
	return true;
}

bool NetSession::TakePendingStartLevel(NetProto::MsgStartLevel& theMsg)
{
	if (!mHasPendingStart) return false;

	theMsg = mPendingStart;
	mHasPendingStart = false;
	ClearPauseState();		// 这就进场了：上一局的暂停态不带进新棋盘
	ClearLevelDoneState();	// "谁清完了"同理
	return true;
}

// 按席位记账、取一条清一条（先进先出）。M2 只有一个队友，看上去和"那一条"没差别；
// 四席位时才知道是谁答的、谁还没答——P3 的放行判定就靠这张表。
bool NetSession::TakeStartAck(bool& theAccepted)
{
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (mSeats[aSeat].mAckState == 0) continue;

		theAccepted = mSeats[aSeat].mAckState == 1;
		mSeats[aSeat].mAckState = 0;
		return true;
	}
	return false;
}

bool NetSession::TakeRunGo()
{
	if (!mHasRunGo) return false;

	mHasRunGo = false;
	return true;
}

bool NetSession::IsPeerSeedsReady()
{
	if (!IsConnected()) return false;

	bool aHasPeer = false;
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (aSeat == mLocalSeat || !IsSeatOccupied(aSeat)) continue;

		aHasPeer = true;
		if (!mSeats[aSeat].mSeedsReady) return false;
	}
	return aHasPeer;		// 自己报的不算"对面"；一个队友都没有也不叫"都选好了"
}

bool NetSession::TakePendingLevelExit(NetProto::MsgLevelExit& theMsg)
{
	if (!mHasPendingLevelExit) return false;

	theMsg = mPendingLevelExit;
	mHasPendingLevelExit = false;
	return true;
}

// ====================================================================================================
// ★ 全队判胜 / 全队败
// ====================================================================================================

// 我这边草坪清干净了没有。棋盘每帧都会问一次，所以去重放在这里：跟上次发出去的值一样就不发。
bool NetSession::SendLevelDone(bool theDone)
{
	if (mRole == Role::NONE || !IsConnected()) return false;
	if (mSeats[mLocalSeat].mLevelDone == theDone) return false;		// 状态没变，队友那边本来就是对的

	NetProto::MsgLevelDone aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mDone = theDone ? 1 : 0;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeLevelDone(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	// 发出去了才记账：没发出去（socket 坏了）下一帧还要再试
	if (!Dispatch(NetProto::MSG_LEVEL_DONE, aPayload, aSize)) return false;

	mSeats[mLocalSeat].mLevelDone = theDone;
	TodLog("[net] told the teammates my lawn is %s (seat %u)", theDone ? "clear" : "busy again",
		(unsigned)mLocalSeat);
	return true;
}

bool NetSession::IsLocalLevelDone() const
{
	return mLocalSeat != NetProto::SEAT_UNSET && mSeats[mLocalSeat].mLevelDone;
}

// 其他上座席位是不是都清完了。一个队友都没有 → false：单机里没人陪你判胜，
// 棋盘上那句"等队友"挂在单机上也会显得莫名其妙。
bool NetSession::IsPeerLevelDone() const
{
	if (!IsConnected()) return false;

	bool aHasPeer = false;
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (aSeat == mLocalSeat || !IsSeatOccupied(aSeat)) continue;

		aHasPeer = true;
		if (!mSeats[aSeat].mLevelDone) return false;
	}
	return aHasPeer;
}

// 上座席位都报了"清完了"才算过。人数从 2 到 4 都是这一条，判胜不用跟着席位数量重写。
bool NetSession::AreAllSeatsDone() const
{
	if (!IsConnected()) return false;

	int aOccupied = 0;
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (!IsSeatOccupied(aSeat)) continue;

		aOccupied++;
		if (!mSeats[aSeat].mLevelDone) return false;
	}

	// 一个人不算"全队"：自己跟自己判胜没有意义，也防住"会话还在但队友已经掉了"的边角
	return aOccupied >= 2;
}

bool NetSession::TakeAllLevelsDone()
{
	if (mAllDoneTaken || !AreAllSeatsDone()) return false;

	mAllDoneTaken = true;
	TodLog("[net] every lawn is clear - this level is over for the whole team");
	return true;
}

bool NetSession::SendGameOver(uint8_t theReason)
{
	if (mRole == Role::NONE || !IsConnected()) return false;

	NetProto::MsgGameOver aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mReason = theReason;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeGameOver(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	TodLog("[net] telling the teammates we lost (reason %u)", (unsigned)theReason);
	return Dispatch(NetProto::MSG_GAME_OVER, aPayload, aSize);
}

bool NetSession::TakePendingGameOver(NetProto::MsgGameOver& theMsg)
{
	if (!mHasPendingGameOver) return false;

	theMsg = mPendingGameOver;
	mHasPendingGameOver = false;
	return true;
}

bool NetSession::SendPauseState(bool thePaused)
{
	if (mRole == Role::NONE || !IsConnected()) return false;

	// 状态没变就不发。这一条是暂停同步能收敛的全部秘密：收端为了队友弹菜单、
	// 关菜单时本机检测器都会看到界面变了，但那时两边认可的状态已经是对的，不会回发。
	if (thePaused == mSharedPaused) return false;

	NetProto::MsgPause aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mPaused = thePaused ? 1 : 0;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodePause(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	if (!Dispatch(NetProto::MSG_PAUSE, aPayload, aSize)) return false;

	mSharedPaused = thePaused;
	mPauseCameFromPeer = false;		// 是我按的，署名归我
	TodLog("[net] told the teammate I %s", thePaused ? "paused" : "resumed");
	return true;
}

bool NetSession::TakePauseState(bool& thePaused)
{
	if (!mHasPendingPause) return false;

	mHasPendingPause = false;
	thePaused = mPendingPauseValue;
	return true;
}

bool NetSession::SwapSeats()
{
	// 一次只谈一件事：要么我在等回话，要么有人正问我——都不许再发一条
	if (!IsConnected() || IsSwapRequestPending() || HasIncomingSwapRequest()) return false;

	// 跟"环上的后一位"换：四席位时 2 号位换的是 3 号位，不是"永远跟主机换"。
	uint8_t aTarget = NextOccupiedSeatInRing(mLocalSeat);
	if (aTarget == NetProto::SEAT_UNSET) return false;		// 没别人可换

	NetProto::MsgSwapRequest aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = aTarget;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeSwapRequest(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	// 只是把请求发出去，本机先不动：换不换由对面点头，点头了才两边一起换。
	// 发不出去当然也谈不上等回话。
	if (!Dispatch(NetProto::MSG_SWAP_REQUEST, aPayload, aSize, aTarget)) return false;

	mSwapRequestSeat = aTarget;
	TodLog("[net] asked seat %u to swap positions", (unsigned)aTarget);
	return true;
}

void NetSession::AnswerSwapRequest(bool theAccept)
{
	if (!HasIncomingSwapRequest()) return;		// 没有待答的问句：多半是重复点击，忽略

	uint8_t aAsker = mSwapAskSeat;
	mSwapAskSeat = NetProto::SEAT_UNSET;
	SendSwapReply(theAccept, aAsker);

	if (theAccept)
	{
		// 两边执行的是同一次换位，所以各自算各自的就一致了，不用再对一次账
		ApplySeatSwap(mLocalSeat, aAsker);
		TodLog("[net] accepted the swap");
	}
	else
	{
		TodLog("[net] declined the swap");
	}
}

void NetSession::ApplySeatSwap(uint8_t theSeatA, uint8_t theSeatB)
{
	if (theSeatA == theSeatB) return;
	if (theSeatA < 1 || theSeatA > NetProto::MAX_PLAYERS) return;
	if (theSeatB < 1 || theSeatB > NetProto::MAX_PLAYERS) return;

	// 两格的内容对调：谁坐在几号位变了，人和他这一局记的状态（名字/构建/判胜/选卡）
	// 一起跟着走——「我报过什么」是我这个人的事，不是我原先那个座位的事。
	std::swap(mSeats[theSeatA], mSeats[theSeatB]);
	if (mLocalSeat == theSeatA) mLocalSeat = theSeatB;
	else if (mLocalSeat == theSeatB) mLocalSeat = theSeatA;

	SetNotice(("Swapped - you are now P" + std::to_string((unsigned)mLocalSeat) + ".").c_str());
	TodLog("[net] positions swapped - local seat %u", (unsigned)mLocalSeat);
}

void NetSession::SetNotice(const char* theText, int theFrames)
{
	mNoticeText = (theText && theText[0]) ? theText : "";
	mNoticeFrames = mNoticeText.empty() ? 0 : theFrames;
}

void NetSession::PostNotice(const char* theText)
{
	SetNotice(theText, NOTICE_FRAMES);
}

// 这一局结束了：开局命令、漏怪、退关这些只对"当前这一局"有意义的东西全部作废。
// 不清的话，上一局的怪会凭空出现在下一局的棋盘上（迟到包砸到新棋盘是最难查的一类）。
void NetSession::DiscardLevelPackets()
{
	mHasPendingStart = false;
	mHasRunGo = false;
	mHasPendingLevelExit = false;
	mPendingEscapedZombies.clear();
	// 选卡状态和开局应答也是"这一局"的（含自己那格"上次发出去的值"）：回主菜单、
	// 掉线、收摊都得清，否则下一局会被上一局的记账挡住。
	for (int aSeat = 0; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		mSeats[aSeat].mSeedsReady = false;
		mSeats[aSeat].mAckState = 0;
		mSeats[aSeat].mAwaitingAck = false;
	}
	ClearPauseState();
	ClearLevelDoneState();
}

// 新的一局从头开始：上一局"暂停着"不该带进来（不然下一局一进场两边就都是停着的）。
void NetSession::ClearPauseState()
{
	mSharedPaused = false;
	mPauseCameFromPeer = false;
	mHasPendingPause = false;
	mPendingPauseValue = false;
}

// 同理：上一局谁清完了是上一局的事，新棋盘一律从头记；全队败的通知也一并作废。
void NetSession::ClearLevelDoneState()
{
	for (int aSeat = 0; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		mSeats[aSeat].mLevelDone = false;
	}
	mAllDoneTaken = false;
	mHasPendingGameOver = false;
}

uint8_t NetSession::GetRelayTargetSeat() const
{
	// 漏怪按顺位往下传：1 → 2 → 3 → 4。**末尾席位没有下一家**——它漏怪就是全队败，
	// 调用方收到 SEAT_UNSET 就走原版判负。
	// 注意这条链和换位那个环不是一回事：换位是环（末位的后一位是首位），漏怪是链，
	// 链的末端就是这条规则的下限。所以这里不套 NextOccupiedSeatInRing。
	if (mLocalSeat == NetProto::SEAT_UNSET) return NetProto::SEAT_UNSET;

	// 直连是两个固定席位，跟"名册上现在有谁"无关：1 号位的下一家永远是 2 号位。
	// （中继没那么固定——谁走了链上就少一格，得按上座席位现算。）
	if (mTransport == Transport::DIRECT)
	{
		return (mLocalSeat == NetProto::SEAT_HOST) ? NetProto::SEAT_CLIENT : NetProto::SEAT_UNSET;
	}

	return NextOccupiedSeat(mLocalSeat);
}

bool NetSession::SendEscapedZombie(const NetProto::MsgEscapedZombie& theMsg)
{
	uint8_t aTarget = GetRelayTargetSeat();
	if (aTarget == NetProto::SEAT_UNSET || !IsConnected()) return false;

	NetProto::MsgEscapedZombie aMsg = theMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = aTarget;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeEscapedZombie(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	TodLog("[net] passing a zombie on: row %u type %u hp %d/%d/%d/%d",
		(unsigned)aMsg.mRow, (unsigned)aMsg.mZombieType,
		(int)aMsg.mBodyHealth, (int)aMsg.mHelmHealth,
		(int)aMsg.mShieldHealth, (int)aMsg.mFlyingHealth);
	return Dispatch(NetProto::MSG_ESCAPED_ZOMBIE, aPayload, aSize, aTarget);
}

bool NetSession::TakePendingEscapedZombie(NetProto::MsgEscapedZombie& theMsg)
{
	if (mPendingEscapedZombies.empty()) return false;

	// 先进先出：漏怪是"又来了几只"的事件，顺序不能乱（队列里一只都不许丢）
	theMsg = mPendingEscapedZombies.front();
	mPendingEscapedZombies.erase(mPendingEscapedZombies.begin());
	return true;
}

// ====================================================================================================
// ★ 状态与文案
// ====================================================================================================

void NetSession::ResetToOff()
{
	// 收摊要把传输层一起收干净。NetLink 只认"上一个收包线程还在，就不给开新的"，
	// 而"对面拔线 / 心跳超时"判死这条路谁都没关过它——只有玩家按 Disconnect 才走 Close()。
	// 不收的话，掉线之后直接再点 Host/Join 会当场失败，而且状态行还挂着上一条死因，
	// 看着像"点了没反应"。Close 幂等，Close() 里重复调到这儿也无所谓。
	mLink.Close();

	mRole = Role::NONE;
	mTransport = Transport::DIRECT;
	mState = State::OFF;
	mLocalSeat = NetProto::SEAT_UNSET;
	ClearSeatTable();			// 名册和每席位的记账跟着这一局作废；自己的名字留着（见 SetLocalName）
	mFramesSincePacket = 0;
	mFramesSinceHeartbeat = 0;
	mHeartbeatTick = 0;
	mConnectHost.clear();
	mConnectPort = NetProto::DEFAULT_PORT;
	mStatusText = "Not connected.";
	mHintText = "Host a game, or type the host's IP and join.";
	mEvents.clear();
	DiscardLevelPackets();
	mSwapRequestSeat = NetProto::SEAT_UNSET;
	mSwapAskSeat = NetProto::SEAT_UNSET;
	mNoticeText.clear();
	mNoticeFrames = 0;
}

void NetSession::SetConnected()
{
	mState = State::CONNECTED;
	mFramesSincePacket = 0;
	mFramesSinceHeartbeat = 0;
	uint8_t aPeer = GetPeerSeat();
	TodLog("[net] handshake complete - local seat %u (%s) build %u, peer seat %u (%s) build %u",
		(unsigned)mLocalSeat, mLocalName.c_str(), (unsigned)NetProto::MOD_BUILD,
		(unsigned)aPeer, GetSeatName(aPeer).c_str(), (unsigned)GetPeerBuild());
	// 两边构建代次不一样：连还是要连的（12 起不再拒绝），但得提示一句——行为不配套
	// 的毛病（"漏怪没落地"那类）看起来都像"游戏坏了"，有这行才知道该去更新哪边。
	if (IsBuildDifferent())
	{
		TodLog("[net] builds differ - local %u, peer %u; playing anyway",
			(unsigned)NetProto::MOD_BUILD, (unsigned)GetPeerBuild());
		SetNotice("Builds differ - update both machines if things break.");
	}
	PushEvent(EventType::CONNECTED);
}

void NetSession::SetDead(const char* theReason, const char* theShortReason)
{
	if (mState == State::DEAD) return;

	mState = State::DEAD;
	mStatusText = (theReason && theReason[0]) ? theReason : "Connection lost.";
	mShortStatus = (theShortReason && theShortReason[0]) ? theShortReason : "Connection lost";
	mHintText.clear();
	// 挂着的换位请求跟着连接一起作废：断了就没得换了，面板上那个问句也得收掉
	mSwapRequestSeat = NetProto::SEAT_UNSET;
	mSwapAskSeat = NetProto::SEAT_UNSET;
	// 一局的收包队列同理：连都没了，上一局的开局命令/漏怪/退关再送到棋盘上是纯乱子
	DiscardLevelPackets();
	mNoticeText.clear();
	mNoticeFrames = 0;
	TodLog("[net] session dead: %s", mStatusText.c_str());
	PushEvent(EventType::DISCONNECTED);
}

void NetSession::UpdateStatusText()
{
	switch (mState)
	{
	case State::OFF:
		mStatusText = "Not connected.";
		break;
	case State::LISTENING:
		// 不写"另一个玩家"：最多四个席位，主机等的是"人"，不是那一个特定的人。
		mStatusText = "Waiting for players to join...";
		break;
	case State::CONNECTING:
		{
			int anAttempts = GetConnectAttempts();
			mStatusText = "Connecting to " + mConnectHost + "...";
			// 连不上会一直重试（没有时间上限了），所以重试次数得露出来，
			// 不然"还在试"和"卡死了"看起来一模一样。
			if (anAttempts > 1)
			{
				mStatusText += " (attempt " + std::to_string((unsigned)anAttempts) + ")";
			}
		}
		break;
	case State::HANDSHAKING:
		mStatusText = "Connected. Shaking hands...";
		break;
	case State::CONNECTED:
		{
			// 关卡由主机定：面板是双方唯一共用的提示位，就把各自的下一步写清楚，
			// 免得客户端点了冒险按钮却什么反馈都没有（局面板照旧不冻心跳）。
			// 换位这件事谁先谁后不一样，所以状态行得按"谁在等谁"分开写。
			if (HasIncomingSwapRequest())
				mStatusText = "The teammate wants to swap positions - Accept or Reject.";
			else if (IsSwapRequestPending())
				mStatusText = "Swap asked - waiting for the teammate to answer.";
			else if (IsBuildDifferent())
			{
				// 构建代次不同：连得上、能玩（12 起不再拒绝，见握手处），这句是常驻提醒。
				// 即时说明几秒就没了，而"这俩不是一套"得一直看得见——真撞上不配套的行为
				// 时，这行就是"该去更新了"的凭据。小条上也有一份短的，见 OnlineStatusWidget。
				std::string aBuilds = "Builds differ (you " + std::to_string((unsigned)NetProto::MOD_BUILD)
					+ " / peer " + std::to_string((unsigned)GetPeerBuild()) + ").";
				mStatusText = (mRole == Role::HOST)
					? aBuilds + " Pick a level."
					: aBuilds + " Waiting for the host.";
			}
			else
				// 连上之后该点哪块牌子，按当前设计是"看情况"的：想一起打单关走 PUZZLE，
				// 组队闯关要等 R5——所以这儿不说牌子名，只说"主机来挑"。
				mStatusText = (mRole == Role::HOST)
					? "Connected. Pick a level from the menu."
					: "Connected. Waiting for the host to pick a level.";

			// 提示行借来写清"我是几号位、漏怪往哪走"：连上之后 IP 已经没用了
			// （输入框里还留着），而位置是开局前要拿主意的事（面板里的 Swap）。
			// 有即时说明（刚换完 / 被拒绝）时先让说明占着，几秒后自己回到席位那行。
			if (mNoticeFrames > 0)
			{
				mHintText = mNoticeText;
			}
			else
			{
				// 席位号与"漏怪往哪走"都按当下的席位算：四席位时 2、3 号位的怪是要往后
				// 接着传的，写死"你是 P2、你接队友的漏怪"就把中间席位说成了末席。
				uint8_t aNext = GetRelayTargetSeat();
				mHintText = "You are P" + std::to_string((unsigned)mLocalSeat);
				mHintText += (aNext == NetProto::SEAT_UNSET)
					? " - the last seat: a leak here loses the game."
					: " - your leaks pass on to P" + std::to_string((unsigned)aNext) + ".";
			}
		}
		break;
	case State::DEAD:
	default:
		break;			// 死因由 SetDead 写死了，别覆盖
	}
}

void NetSession::PushEvent(EventType theType)
{
	Event anEvent;
	anEvent.mType = theType;
	mEvents.push_back(anEvent);
}

// ====================================================================================================
// ★ 收包
// ====================================================================================================

void NetSession::HandlePacket(const NetLink::Packet& thePacket)
{
	if (thePacket.mSize < NetProto::HEADER_SIZE)
	{
		SetDead("A player sent a malformed packet.");
		return;
	}

	uint16_t aType = (uint16_t)(thePacket.mData[0] | (thePacket.mData[1] << 8));
	uint16_t aPayloadSize = (uint16_t)(thePacket.mData[2] | (thePacket.mData[3] << 8));
	if (NetProto::HEADER_SIZE + aPayloadSize != thePacket.mSize)
	{
		SetDead("A player sent a malformed packet.");
		return;
	}

	const uint8_t* aPayload = thePacket.mData + NetProto::HEADER_SIZE;

	switch (aType)
	{
	case NetProto::MSG_HELLO:
		{
			if (mRole != Role::HOST) return;		// 只有主机收 HELLO

			// 长度对不上 HELLO 只有一个解释：对面是别的构建版。旧包（HELLO 是 4 字节、
			// 没有 build 号）正好落在这儿——这正是"另一边跑着上个版本"的样子，
			// 要给出能照做的提示，不能报成含糊的坏包。
			if (aPayloadSize != HELLO_PAYLOAD_SIZE)
			{
				SendHelloAck(false, DirectPeerSeat());
				SetDead("Build mismatch - update every machine to the same build.", "Build mismatch");
				return;
			}

			NetProto::MsgHello aMsg;
			if (!NetProto::DecodeHello(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}

			if (aMsg.mVersion != NetProto::PROTOCOL_VERSION)
			{
				SendHelloAck(false, DirectPeerSeat());
				SetDead("Version mismatch - all players must run the same build.", "Version mismatch");
				return;
			}
			// 构建代次不一样照样连（用户 2026-10-02 拍板：只提示，不拒人）。包是同一套，
			// 编解码对得上；不配套的只是行为，UI 上挂一句提醒，真撞上不对劲玩家自己会更新。
			// 拒绝只留给 PROTOCOL_VERSION 和 HELLO 长度——那两种连包都读不出来。

			// 席位号是包里的一个字节，会进席位表、再拿去索引记账——越界就是写穿。
			// 合法席位只有 1..4，且不会是本机自己。
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS
				|| aMsg.mSrcSeat == mLocalSeat)
			{
				SetDead("A player sent a bogus seat number.");
				return;
			}

			mSeats[aMsg.mSrcSeat].mOccupied = true;
			mSeats[aMsg.mSrcSeat].mBuild = aMsg.mBuild;
			mSeats[aMsg.mSrcSeat].mName = SanitizeName(aMsg.mName, NetProto::NAME_SIZE);
			SendHelloAck(true, aMsg.mSrcSeat);
			SetConnected();
		}
		break;

	case NetProto::MSG_HELLO_ACK:
		{
			if (mRole != Role::CLIENT) return;

			if (aPayloadSize != HELLO_ACK_PAYLOAD_SIZE)
			{
				SetDead("Build mismatch - update every machine to the same build.", "Build mismatch");
				return;
			}

			NetProto::MsgHelloAck aMsg;
			if (!NetProto::DecodeHelloAck(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}

			if (!aMsg.mAccepted)
			{
				// 主机拒接的原因只剩两种：协议版本对不上，或者它读不出我们的 HELLO（旧构建）。
				// ACK 里带着主机的版本号，正好能分辨——提示得指得出该更新哪边，别一律怪"构建"。
				if (aMsg.mVersion != NetProto::PROTOCOL_VERSION)
					SetDead("Version mismatch - all players must run the same build.", "Version mismatch");
				else
					SetDead("Build mismatch - update every machine to the same build.", "Build mismatch");
				return;
			}
			if (aMsg.mVersion != NetProto::PROTOCOL_VERSION)
			{
				SetDead("Version mismatch - all players must run the same build.", "Version mismatch");
				return;
			}
			// 构建代次不一样不再拒绝（同主机侧，见 MSG_HELLO 那段）：记下、提示、照常连。

			// 席位号要过和主机侧同样的筛：以前这里是"对面固定 1 号位"的写死假设，
			// 现在改听主机自报，也得防它报个越界或本机的席位（理由见 MSG_HELLO 那段）。
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS
				|| aMsg.mSrcSeat == mLocalSeat)
			{
				SetDead("A player sent a bogus seat number.");
				return;
			}

			mSeats[aMsg.mSrcSeat].mOccupied = true;
			mSeats[aMsg.mSrcSeat].mBuild = aMsg.mBuild;
			mSeats[aMsg.mSrcSeat].mName = SanitizeName(aMsg.mName, NetProto::NAME_SIZE);
			SetConnected();
		}
		break;

	case NetProto::MSG_START_LEVEL:
		{
			NetProto::MsgStartLevel aMsg;
			if (aPayloadSize != START_LEVEL_PAYLOAD_SIZE || !NetProto::DecodeStartLevel(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}
			if (mRole != Role::CLIENT) return;		// 只有客户端听主机的

			// 不在收包链里直接开局：开局要动游戏场景和一堆 UI，那是主循环的活。
			// 这里只把命令存下来，LawnApp 每帧 TakePendingStartLevel 取走；
			// 它真进场的时候才回 START_ACK——没进场的命令不能让主机先进去。
			mPendingStart = aMsg;
			mHasPendingStart = true;
			if (aMsg.mIsRun)
			{
				TodLog("[net] host started the run: level %u seed %d (run seed %d, level index %u)",
					(unsigned)aMsg.mLevel, (int)aMsg.mLevelSeed, (int)aMsg.mRunSeed, (unsigned)aMsg.mRunLevelIndex);
			}
			else
			{
				TodLog("[net] host started: mode %u level %u seed %d",
					(unsigned)aMsg.mGameMode, (unsigned)aMsg.mLevel, (int)aMsg.mLevelSeed);
			}
		}
		break;

	case NetProto::MSG_HEARTBEAT:
		{
			NetProto::MsgHeartbeat aMsg;
			if (aPayloadSize != HEARTBEAT_PAYLOAD_SIZE || !NetProto::DecodeHeartbeat(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}
			// 收到就是活着——mFramesSincePacket 已经在调用处清零了
		}
		break;

	case NetProto::MSG_BYE:
		{
			NetProto::MsgBye aMsg;
			if (aPayloadSize != BYE_PAYLOAD_SIZE || !NetProto::DecodeBye(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}
			SetDead("A player left the game.");
		}
		break;

	case NetProto::MSG_ESCAPED_ZOMBIE:
		{
			NetProto::MsgEscapedZombie aMsg;
			if (aPayloadSize != ESCAPED_ZOMBIE_PAYLOAD_SIZE || !NetProto::DecodeEscapedZombie(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}

			// 跟开局命令一样：收包链里不建僵尸（那要动棋盘、加载美术），
			// 只排队；LawnApp 每帧取走。队列不清空的话迟到的怪会在下一关冒出来。
			mPendingEscapedZombies.push_back(aMsg);
			TodLog("[net] the teammate passed a zombie: row %u type %u hp %d/%d/%d/%d",
				(unsigned)aMsg.mRow, (unsigned)aMsg.mZombieType,
				(int)aMsg.mBodyHealth, (int)aMsg.mHelmHealth,
				(int)aMsg.mShieldHealth, (int)aMsg.mFlyingHealth);
		}
		break;

	case NetProto::MSG_START_ACK:
		{
			if (mRole != Role::HOST) return;		// 只有主机在等这条

			NetProto::MsgStartAck aMsg;
			if (aPayloadSize != START_ACK_PAYLOAD_SIZE || !NetProto::DecodeStartAck(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}
			// 席位号越界就丢掉这一条（同 LEVEL_DONE）：它是包里的一个字节，
			// 直接拿去索引席位表就是写穿。
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS || aMsg.mSrcSeat == mLocalSeat)
			{
				TodLog("[net] threw away a START_ACK with a bogus seat (%u)", (unsigned)aMsg.mSrcSeat);
				break;
			}

			// 存到他那格里就走：开局要换场景、动一堆 UI，那是主循环的活。
			mSeats[aMsg.mSrcSeat].mAckState = aMsg.mAccepted ? 1 : 2;
			mSeats[aMsg.mSrcSeat].mAwaitingAck = false;
			TodLog("[net] seat %u answered the start: %s", (unsigned)aMsg.mSrcSeat,
				aMsg.mAccepted ? "in" : "not now");
		}
		break;

	case NetProto::MSG_SWAP_REQUEST:
		{
			// 没连上就谈不上换位置（也顺手把迟到的这类包挡在外面）
			if (!IsConnected()) return;

			NetProto::MsgSwapRequest aMsg;
			if (aPayloadSize != SWAP_REQUEST_PAYLOAD_SIZE || !NetProto::DecodeSwapRequest(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}
			// 问话的人得是个真坐在席位上的席位号——乱报的丢掉这一条，不至于断线
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS
				|| aMsg.mSrcSeat == mLocalSeat || !IsSeatOccupied(aMsg.mSrcSeat))
			{
				TodLog("[net] threw away a SWAP_REQUEST from seat %u", (unsigned)aMsg.mSrcSeat);
				break;
			}

			if (IsSwapRequestPending() && mSwapRequestSeat == aMsg.mSrcSeat)
			{
				// 两边同时按：互相要的是同一件事（我跟后一位换 = 他跟我换），直接成交，
				// 别让双方都傻等对方点头。各自撤掉自己那条请求，回话到对面也会被无视。
				SendSwapReply(true, aMsg.mSrcSeat);
				mSwapRequestSeat = NetProto::SEAT_UNSET;
				ApplySeatSwap(mLocalSeat, aMsg.mSrcSeat);
				TodLog("[net] both sides asked at once - swapping");
			}
			else if (HasIncomingSwapRequest())
			{
				// 已经有一条待答的问句挂在玩家面前了：一次只谈一件事，多出来的按拒绝回
				SendSwapReply(false, aMsg.mSrcSeat);
			}
			else
			{
				// 主循环看到 HasIncomingSwapRequest 就把面板叫出来问玩家（面板不冻心跳）
				mSwapAskSeat = aMsg.mSrcSeat;
				TodLog("[net] seat %u asks to swap positions", (unsigned)aMsg.mSrcSeat);
			}
		}
		break;

	case NetProto::MSG_SWAP_REPLY:
		{
			if (!IsConnected()) return;

			NetProto::MsgSwapReply aMsg;
			if (aPayloadSize != SWAP_REPLY_PAYLOAD_SIZE || !NetProto::DecodeSwapReply(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}

			// 不是我等的回话（迟到的、或对面那条请求已经被我这边成交掉了）：当它没到
			if (!IsSwapRequestPending()) return;
			if (aMsg.mSrcSeat != mSwapRequestSeat) return;		// 也不是我问的那个人回的

			uint8_t aSeat = mSwapRequestSeat;
			mSwapRequestSeat = NetProto::SEAT_UNSET;
			if (aMsg.mAccepted)
			{
				ApplySeatSwap(mLocalSeat, aSeat);
				TodLog("[net] the swap went through");
			}
			else
			{
				SetNotice("The teammate declined the swap.");
				TodLog("[net] the teammate declined the swap");
			}
		}
		break;

	case NetProto::MSG_LEVEL_DONE:
		{
			if (!IsConnected()) return;

			NetProto::MsgLevelDone aMsg;
			if (aPayloadSize != LEVEL_DONE_PAYLOAD_SIZE || !NetProto::DecodeLevelDone(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}

			// 席位号越界就丢掉这一条：它是包里的一个字节，直接拿去当数组下标会写穿。
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS)
			{
				TodLog("[net] threw away a LEVEL_DONE with a bogus seat (%u)", (unsigned)aMsg.mSrcSeat);
				break;
			}

			mSeats[aMsg.mSrcSeat].mLevelDone = aMsg.mDone != 0;
			// 有人又不清净了，"全队过关"要重新攒——不然那句已经收过的摊会挡住下一次
			if (!aMsg.mDone) mAllDoneTaken = false;
			TodLog("[net] seat %u says its lawn is %s", (unsigned)aMsg.mSrcSeat,
				aMsg.mDone ? "clear" : "busy again");
		}
		break;

	case NetProto::MSG_GAME_OVER:
		{
			if (!IsConnected()) return;

			NetProto::MsgGameOver aMsg;
			if (aPayloadSize != GAME_OVER_PAYLOAD_SIZE || !NetProto::DecodeGameOver(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}

			mPendingGameOver = aMsg;
			mHasPendingGameOver = true;
			TodLog("[net] the teammate says the team lost (reason %u)", (unsigned)aMsg.mReason);
		}
		break;

	case NetProto::MSG_PAUSE:
		{
			if (!IsConnected()) return;

			NetProto::MsgPause aMsg;
			if (aPayloadSize != PAUSE_PAYLOAD_SIZE || !NetProto::DecodePause(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}

			bool aPaused = aMsg.mPaused != 0;
			// 只有"收到 0→1"才把署名记给队友：两人同时按下时两边都已经在暂停里，
			// 谁都不该显示"是队友按的"。
			if (aPaused && !mSharedPaused) mPauseCameFromPeer = true;
			else if (!aPaused) mPauseCameFromPeer = false;

			mSharedPaused = aPaused;
			mPendingPauseValue = aPaused;
			mHasPendingPause = true;
			TodLog("[net] the teammate %s", aPaused ? "paused the game" : "resumed the game");
		}
		break;

	case NetProto::MSG_LEVEL_EXIT:
		{
			if (!IsConnected()) return;

			NetProto::MsgLevelExit aMsg;
			if (aPayloadSize != LEVEL_EXIT_PAYLOAD_SIZE || !NetProto::DecodeLevelExit(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}

			// 单槽：连着退两次只当一次（"这一局结束了"是个状态，不是一串事件）
			mPendingLevelExit = aMsg;
			mHasPendingLevelExit = true;
			TodLog("[net] the teammate left the level (reason %u)", (unsigned)aMsg.mReason);
		}
		break;

	case NetProto::MSG_RUN_GO:
		{
			if (mRole != Role::CLIENT) return;		// 只有客户端在等这条

			NetProto::MsgRunGo aMsg;
			if (aPayloadSize != RUN_GO_PAYLOAD_SIZE || !NetProto::DecodeRunGo(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}
			// 单槽：放行是个状态（"可以开始选了"），连着收到两回只当一回
			mHasRunGo = true;
			TodLog("[net] the host says go: the whole team is on the lawn");
		}
		break;

	case NetProto::MSG_SEEDS_READY:
		{
			// 双向：谁先选完谁先报，所有席位都收
			NetProto::MsgSeedsReady aMsg;
			if (aPayloadSize != SEEDS_READY_PAYLOAD_SIZE || !NetProto::DecodeSeedsReady(aPayload, aPayloadSize, aMsg))
			{
				SetDead("A player sent a malformed packet.");
				return;
			}
			// 席位号越界就丢掉这一条（同 LEVEL_DONE 的理由）
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS)
			{
				TodLog("[net] threw away a SEEDS_READY with a bogus seat (%u)", (unsigned)aMsg.mSrcSeat);
				break;
			}
			// 收下的是个状态（"他这一轮选好了没有"），后到的覆盖先到的——对面新一轮开始
			// 会再报一次 0，本地不用挑时机清（清位就是丢消息，见 NetProtocol.h 那段）。
			mSeats[aMsg.mSrcSeat].mSeedsReady = aMsg.mReady != 0;
			TodLog("[net] seat %u %s", (unsigned)aMsg.mSrcSeat,
				(aMsg.mReady != 0) ? "picked their plants" : "is picking plants");
		}
		break;

	default:
		// 没见过的消息类型：长度合法性已经在头上查过，帧长（头里的 len）还对得上，
		// 所以丢掉这一条就行，不必断线。跨版本不是靠这儿挡的——构建代次不同现在照连
		//（见 MSG_HELLO 那段），这里只防串包和将来加消息时的中间态。
		break;
	}
}

// ====================================================================================================
// ★ 发包
// ====================================================================================================

// 把一帧交给该收的人。直连只有一个对端——行为与 M2 逐字节一致：目标就是那一个，
// 载荷里的 dst 也还是写它。（四席位的扇出/点名单发在中继那边，见 NetSession.h。）
bool NetSession::Dispatch(uint16_t theType, uint8_t* thePayload, int theSize, uint8_t theTarget)
{
	uint8_t aDst = DirectPeerSeat();
	if (aDst == NetProto::SEAT_UNSET) return false;
	if (theTarget != NetProto::SEAT_UNSET && theTarget != aDst) return false;

	// 带席位号的载荷（前两字节 src/dst）在这儿统一盖戳：调用方只报"发给谁"，
	// 别自己写 dst，省得两处各写一份、哪天对不上。
	if (theSize >= 2 && thePayload)
	{
		thePayload[0] = mLocalSeat;
		thePayload[1] = aDst;
	}
	return SendRaw(theType, thePayload, theSize);
}

bool NetSession::SendRaw(uint16_t theType, const uint8_t* thePayload, int thePayloadSize)
{
	if (thePayloadSize < 0 || thePayloadSize > NetProto::MAX_PAYLOAD) return false;

	uint8_t aPacket[NetProto::MAX_PACKET];
	aPacket[0] = (uint8_t)(theType & 0xFF);
	aPacket[1] = (uint8_t)(theType >> 8);
	aPacket[2] = (uint8_t)(thePayloadSize & 0xFF);
	aPacket[3] = (uint8_t)(thePayloadSize >> 8);
	if (thePayloadSize > 0 && thePayload)
	{
		memcpy(aPacket + NetProto::HEADER_SIZE, thePayload, thePayloadSize);
	}

	// 发不出去不用在这儿改状态：NetLink 已经置成 FAILED，下一帧 Update 会统一收尸
	return mLink.Send(aPacket, NetProto::HEADER_SIZE + thePayloadSize);
}

void NetSession::SendHello()
{
	NetProto::MsgHello aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = DirectPeerSeat();		// 还没握手，名册是空的，直连的另一个席位是固定的
	aMsg.mVersion = NetProto::PROTOCOL_VERSION;
	aMsg.mBuild = NetProto::MOD_BUILD;
	NetProto::SetName(aMsg.mName, mLocalName.c_str());

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeHello(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) SendRaw(NetProto::MSG_HELLO, aPayload, aSize);
}

void NetSession::SendHelloAck(bool theAccepted, uint8_t theTarget)
{
	NetProto::MsgHelloAck aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = theTarget;
	aMsg.mVersion = NetProto::PROTOCOL_VERSION;
	aMsg.mBuild = NetProto::MOD_BUILD;
	aMsg.mAccepted = theAccepted ? 1 : 0;
	NetProto::SetName(aMsg.mName, mLocalName.c_str());

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeHelloAck(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) SendRaw(NetProto::MSG_HELLO_ACK, aPayload, aSize);
}

bool NetSession::SendStartLevel(uint8_t theGameMode, uint32_t theLevel, int32_t theLevelSeed,
	bool theIsRun, int32_t theRunSeed, uint8_t theRunLevelIndex, uint8_t theTargetSeat)
{
	if (mRole != Role::HOST || !IsConnected()) return false;

	NetProto::MsgStartLevel aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mGameMode = theGameMode;
	aMsg.mLevel = theLevel;
	aMsg.mLevelSeed = theLevelSeed;
	aMsg.mIsRun = theIsRun ? 1 : 0;
	aMsg.mRunSeed = theRunSeed;
	aMsg.mRunLevelIndex = theRunLevelIndex;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeStartLevel(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	if (theIsRun)
	{
		TodLog("[net] telling the client to enter run level %u (run seed %d, level index %u)",
			(unsigned)theLevel, (int)theRunSeed, (unsigned)theRunLevelIndex);
	}
	else
	{
		TodLog("[net] telling the client to start: mode %u level %u seed %d",
			(unsigned)theGameMode, (unsigned)theLevel, (int)theLevelSeed);
	}
	ClearPauseState();		// 新的一局：暂停态从头开始记
	ClearLevelDoneState();	// "谁清完了"同理

	if (!Dispatch(NetProto::MSG_START_LEVEL, aPayload, aSize, theTargetSeat)) return false;

	// 发出去了才记"在等谁回答"：收齐应答的判定（P3 的放行）得按席位看，
	// 单槽记账看不出四个人里是谁还没答。
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (aSeat == mLocalSeat || !IsSeatOccupied(aSeat)) continue;
		if (theTargetSeat != NetProto::SEAT_UNSET && aSeat != theTargetSeat) continue;
		mSeats[aSeat].mAwaitingAck = true;
	}
	return true;
}

void NetSession::SendStartAck(bool theAccepted)
{
	if (mRole != Role::CLIENT || !IsConnected()) return;

	NetProto::MsgStartAck aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mAccepted = theAccepted ? 1 : 0;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeStartAck(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0)
	{
		TodLog(theAccepted ? "[net] telling the host I am entering the level"
			: "[net] telling the host I cannot enter the level right now");
		Dispatch(NetProto::MSG_START_ACK, aPayload, aSize);
	}
}

bool NetSession::SendRunGo()
{
	if (mRole != Role::HOST || !IsConnected()) return false;

	NetProto::MsgRunGo aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeRunGo(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	TodLog("[net] telling the team to start their picks");
	return Dispatch(NetProto::MSG_RUN_GO, aPayload, aSize);
}

// 双向：报"我这一轮的选卡状态"。是个状态不是事件——同一轮里重复报同一个值就不发
// （和 SendLevelDone 同款去重）；新一轮开始调用方会报一次当下的状态（要选卡的报 0，
// 不用选卡的报 1），所以去重不会把该报的漏掉。
bool NetSession::SendSeedsReady(bool theReady)
{
	if (mRole == Role::NONE || !IsConnected()) return false;
	if (mSeats[mLocalSeat].mSeedsReady == theReady) return false;		// 状态没变，队友本来就是对的

	NetProto::MsgSeedsReady aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mReady = theReady ? 1 : 0;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeSeedsReady(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	mSeats[mLocalSeat].mSeedsReady = theReady;
	TodLog(theReady ? "[net] told the team my plants are picked"
		: "[net] told the team I am still picking plants");
	return Dispatch(NetProto::MSG_SEEDS_READY, aPayload, aSize);
}

bool NetSession::SendLevelExit(uint8_t theReason)
{
	if (mRole == Role::NONE || !IsConnected()) return false;

	NetProto::MsgLevelExit aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mReason = theReason;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeLevelExit(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	TodLog("[net] telling the teammate I left the level (reason %u)", (unsigned)theReason);
	return Dispatch(NetProto::MSG_LEVEL_EXIT, aPayload, aSize);
}

void NetSession::SendSwapReply(bool theAccepted, uint8_t theTarget)
{
	NetProto::MsgSwapReply aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = theTarget;
	aMsg.mAccepted = theAccepted ? 1 : 0;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeSwapReply(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) Dispatch(NetProto::MSG_SWAP_REPLY, aPayload, aSize, theTarget);
}

void NetSession::SendHeartbeat()
{
	NetProto::MsgHeartbeat aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mTick = ++mHeartbeatTick;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeHeartbeat(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) Dispatch(NetProto::MSG_HEARTBEAT, aPayload, aSize);
}

void NetSession::SendBye(uint8_t theReason)
{
	NetProto::MsgBye aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mReason = theReason;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeBye(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) Dispatch(NetProto::MSG_BYE, aPayload, aSize);
}
