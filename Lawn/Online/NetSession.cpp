#include "NetSession.h"

#include <cstring>

#include "../../Sexy.TodLib/TodDebug.h"

namespace
{

const int	HELLO_PAYLOAD_SIZE		= 6;	// src, dst, u16 version, u16 build
const int	HELLO_ACK_PAYLOAD_SIZE	= 7;	// src, dst, u16 version, u16 build, u8 accepted
const int	START_LEVEL_PAYLOAD_SIZE = 11;	// src, dst, u8 mode, u32 level, i32 seed
const int	START_ACK_PAYLOAD_SIZE	= 2;	// src, dst
const int	SWAP_REQUEST_PAYLOAD_SIZE = 2;	// src, dst
const int	SWAP_REPLY_PAYLOAD_SIZE = 3;	// src, dst, u8 accepted
const int	ESCAPED_ZOMBIE_PAYLOAD_SIZE = 22;	// src, dst, u8 row, u16 type, u8 flags, i32 ×4 血量
const int	HEARTBEAT_PAYLOAD_SIZE	= 6;	// src, dst, u32 tick
const int	BYE_PAYLOAD_SIZE		= 3;	// src, dst, u8 reason

// M2 就两个席位。换位规则按"环上的后一位"写，所以扩到四席位时只要把这个数
// 和 ApplySeatSwap 一起改成按座次置换，规则本身不用动。
const uint8_t	SEAT_COUNT		= 2;

// 席位是个环：1 → 2 → … → N → 1。末位的后一位是首位，人人都有换的对象。
uint8_t NextSeatInRing(uint8_t theSeat)
{
	return (theSeat >= SEAT_COUNT) ? NetProto::SEAT_HOST : (uint8_t)(theSeat + 1);
}

}

NetSession::NetSession()
{
	mRole = Role::NONE;
	mState = State::OFF;
	mLocalSeat = NetProto::SEAT_UNSET;
	mPeerSeat = NetProto::SEAT_UNSET;
	mFramesSincePacket = 0;
	mFramesSinceHeartbeat = 0;
	mHeartbeatTick = 0;
	mConnectPort = NetProto::DEFAULT_PORT;
	mStatusText = "Not connected.";
	mHintText = "Host a game, or type the host's IP and join.";
	mShortStatus = "Connection lost";
	mHasPendingStart = false;
	mHasStartAck = false;
	mSwapRequestPending = false;
	mSwapAskPending = false;
	mNoticeFrames = 0;
}

NetSession::~NetSession()
{
	// 析构不再发 BYE：对端收不到也只是多等 5 秒心跳超时，和崩溃退出一个待遇。
	mLink.Close();
}

// ====================================================================================================
// ★ 对外接口
// ====================================================================================================

bool NetSession::StartHost(uint16_t thePort)
{
	ResetToOff();
	mRole = Role::HOST;
	mLocalSeat = NetProto::SEAT_HOST;
	mPeerSeat = NetProto::SEAT_CLIENT;

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
	mLocalSeat = NetProto::SEAT_CLIENT;
	mPeerSeat = NetProto::SEAT_HOST;
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
	return true;
}

bool NetSession::TakeStartAck()
{
	if (!mHasStartAck) return false;

	mHasStartAck = false;
	return true;
}

bool NetSession::SwapSeats()
{
	// 一次只谈一件事：要么我在等回话，要么对面正问我——都不许再发一条
	if (!IsConnected() || mSwapRequestPending || mSwapAskPending) return false;

	NetProto::MsgSwapRequest aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NextSeatInRing(mLocalSeat);

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeSwapRequest(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	// 只是把请求发出去，本机先不动：换不换由对面点头，点头了才两边一起换。
	// 发不出去当然也谈不上等回话。
	if (!SendRaw(NetProto::MSG_SWAP_REQUEST, aPayload, aSize)) return false;

	mSwapRequestPending = true;
	TodLog("[net] asked seat %u to swap positions", (unsigned)aMsg.mDstSeat);
	return true;
}

void NetSession::AnswerSwapRequest(bool theAccept)
{
	if (!mSwapAskPending) return;			// 没有待答的问句：多半是重复点击，忽略

	mSwapAskPending = false;
	SendSwapReply(theAccept);

	if (theAccept)
	{
		// 两边执行的是同一次换位，所以各自算各自的就一致了，不用再对一次账
		ApplySeatSwap();
		TodLog("[net] accepted the swap");
	}
	else
	{
		TodLog("[net] declined the swap");
	}
}

void NetSession::ApplySeatSwap()
{
	// 和"后一位"对调：本机换成那个席位，对面换成我原来的席位。两席位的时候
	// 就是 1 ↔ 2；四席位时这条要按请求里带的两个座次来换（见 NextSeatInRing）。
	uint8_t aMySeat = mLocalSeat;
	mLocalSeat = mPeerSeat;
	mPeerSeat = aMySeat;

	SetNotice(mLocalSeat == NetProto::SEAT_HOST
		? "Swapped - you are now P1."
		: "Swapped - you are now P2.");
	TodLog("[net] positions swapped - local seat %u, peer seat %u",
		(unsigned)mLocalSeat, (unsigned)mPeerSeat);
}

void NetSession::SetNotice(const char* theText, int theFrames)
{
	mNoticeText = (theText && theText[0]) ? theText : "";
	mNoticeFrames = mNoticeText.empty() ? 0 : theFrames;
}

uint8_t NetSession::GetRelayTargetSeat() const
{
	// M2 只有两个席位，队形就是一环：1 → 2。2 号位是末席——它漏怪就是全队败，
	// 没有可传的人（这条规则由调用方处理：收到 SEAT_UNSET 就走原版判负）。
	// 只看席位号，不看谁建的房：开始前换过位置的话，方向跟着换。
	return (mLocalSeat == NetProto::SEAT_HOST) ? NetProto::SEAT_CLIENT : NetProto::SEAT_UNSET;
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
	return SendRaw(NetProto::MSG_ESCAPED_ZOMBIE, aPayload, aSize);
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
	mRole = Role::NONE;
	mState = State::OFF;
	mLocalSeat = NetProto::SEAT_UNSET;
	mPeerSeat = NetProto::SEAT_UNSET;
	mFramesSincePacket = 0;
	mFramesSinceHeartbeat = 0;
	mHeartbeatTick = 0;
	mConnectHost.clear();
	mConnectPort = NetProto::DEFAULT_PORT;
	mStatusText = "Not connected.";
	mHintText = "Host a game, or type the host's IP and join.";
	mEvents.clear();
	mHasPendingStart = false;
	mHasStartAck = false;
	mPendingEscapedZombies.clear();
	mSwapRequestPending = false;
	mSwapAskPending = false;
	mNoticeText.clear();
	mNoticeFrames = 0;
}

void NetSession::SetConnected()
{
	mState = State::CONNECTED;
	mFramesSincePacket = 0;
	mFramesSinceHeartbeat = 0;
	TodLog("[net] handshake complete - local seat %u, peer seat %u",
		(unsigned)mLocalSeat, (unsigned)mPeerSeat);
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
	mSwapRequestPending = false;
	mSwapAskPending = false;
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
		mStatusText = "Waiting for the other player...";
		break;
	case State::CONNECTING:
		{
			int anAttempts = GetConnectAttempts();
			mStatusText = "Connecting to " + mConnectHost + "...";
			// 连不上会一直重试（没有时间上限了），所以重试次数得露出来，
			// 不然"还在试"看起来和"卡死了"一模一样。
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
			if (mSwapAskPending)
				mStatusText = "The teammate wants to swap positions - Accept or Reject.";
			else if (mSwapRequestPending)
				mStatusText = "Swap asked - waiting for the teammate to answer.";
			else
				mStatusText = (mRole == Role::HOST)
					? "Connected. Pick a level from the menu."
					: "Connected. Waiting for the host to pick a level.";

			// 提示行借来写清"我是几号位、漏怪往哪走"：连上之后 IP 已经没用了
			// （输入框里还留着），而位置是开局前要拿主意的事（面板里的 Swap）。
			// 有即时说明（刚换完 / 被拒绝）时先让说明占着，几秒后自己回到席位那行。
			if (mNoticeFrames > 0)
				mHintText = mNoticeText;
			else if (mLocalSeat == NetProto::SEAT_HOST)
				mHintText = "You are P1 - your leaks pass to your teammate.";
			else
				mHintText = "You are P2 - you take your teammate's leaks.";
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
		SetDead("The other player sent a malformed packet.");
		return;
	}

	uint16_t aType = (uint16_t)(thePacket.mData[0] | (thePacket.mData[1] << 8));
	uint16_t aPayloadSize = (uint16_t)(thePacket.mData[2] | (thePacket.mData[3] << 8));
	if (NetProto::HEADER_SIZE + aPayloadSize != thePacket.mSize)
	{
		SetDead("The other player sent a malformed packet.");
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
				SendHelloAck(false);
				SetDead("Build mismatch - update both machines to the same build.", "Build mismatch");
				return;
			}

			NetProto::MsgHello aMsg;
			if (!NetProto::DecodeHello(aPayload, aPayloadSize, aMsg))
			{
				SetDead("The other player sent a malformed packet.");
				return;
			}

			if (aMsg.mVersion != NetProto::PROTOCOL_VERSION)
			{
				SendHelloAck(false);
				SetDead("Version mismatch - both players must run the same build.", "Build mismatch");
				return;
			}
			if (aMsg.mBuild != NetProto::MOD_BUILD)
			{
				SendHelloAck(false);
				SetDead("Build mismatch - update both machines to the same build.", "Build mismatch");
				return;
			}

			mPeerSeat = aMsg.mSrcSeat;
			SendHelloAck(true);
			SetConnected();
		}
		break;

	case NetProto::MSG_HELLO_ACK:
		{
			if (mRole != Role::CLIENT) return;

			if (aPayloadSize != HELLO_ACK_PAYLOAD_SIZE)
			{
				SetDead("Build mismatch - update both machines to the same build.", "Build mismatch");
				return;
			}

			NetProto::MsgHelloAck aMsg;
			if (!NetProto::DecodeHelloAck(aPayload, aPayloadSize, aMsg))
			{
				SetDead("The other player sent a malformed packet.");
				return;
			}

			if (!aMsg.mAccepted)
			{
				SetDead("The other player refused - update both machines.", "Build mismatch");
				return;
			}
			if (aMsg.mVersion != NetProto::PROTOCOL_VERSION || aMsg.mBuild != NetProto::MOD_BUILD)
			{
				SetDead("Build mismatch - update both machines to the same build.", "Build mismatch");
				return;
			}
			SetConnected();
		}
		break;

	case NetProto::MSG_START_LEVEL:
		{
			NetProto::MsgStartLevel aMsg;
			if (aPayloadSize != START_LEVEL_PAYLOAD_SIZE || !NetProto::DecodeStartLevel(aPayload, aPayloadSize, aMsg))
			{
				SetDead("The other player sent a malformed packet.");
				return;
			}
			if (mRole != Role::CLIENT) return;		// 只有客户端听主机的

			// 不在收包链里直接开局：开局要动游戏场景和一堆 UI，那是主循环的活。
			// 这里只把命令存下来，LawnApp 每帧 TakePendingStartLevel 取走；
			// 它真进场的时候才回 START_ACK——没进场的命令不能让主机先进去。
			mPendingStart = aMsg;
			mHasPendingStart = true;
			TodLog("[net] host started: mode %u level %u seed %d",
				(unsigned)aMsg.mGameMode, (unsigned)aMsg.mLevel, (int)aMsg.mLevelSeed);
		}
		break;

	case NetProto::MSG_HEARTBEAT:
		{
			NetProto::MsgHeartbeat aMsg;
			if (aPayloadSize != HEARTBEAT_PAYLOAD_SIZE || !NetProto::DecodeHeartbeat(aPayload, aPayloadSize, aMsg))
			{
				SetDead("The other player sent a malformed packet.");
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
				SetDead("The other player sent a malformed packet.");
				return;
			}
			SetDead("The other player left the game.");
		}
		break;

	case NetProto::MSG_ESCAPED_ZOMBIE:
		{
			NetProto::MsgEscapedZombie aMsg;
			if (aPayloadSize != ESCAPED_ZOMBIE_PAYLOAD_SIZE || !NetProto::DecodeEscapedZombie(aPayload, aPayloadSize, aMsg))
			{
				SetDead("The other player sent a malformed packet.");
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
				SetDead("The other player sent a malformed packet.");
				return;
			}
			// 存下来就走：开局要换场景、动一堆 UI，那是主循环的活。
			mHasStartAck = true;
			TodLog("[net] the teammate is ready, the host may enter the level");
		}
		break;

	case NetProto::MSG_SWAP_REQUEST:
		{
			// 没连上就谈不上换位置（也顺手把迟到的这类包挡在外面）
			if (!IsConnected()) return;

			NetProto::MsgSwapRequest aMsg;
			if (aPayloadSize != SWAP_REQUEST_PAYLOAD_SIZE || !NetProto::DecodeSwapRequest(aPayload, aPayloadSize, aMsg))
			{
				SetDead("The other player sent a malformed packet.");
				return;
			}

			if (mSwapRequestPending)
			{
				// 两边同时按：互相要的是同一件事（我跟后一位换 = 他跟我换），直接成交，
				// 别让双方都傻等对方点头。各自撤掉自己那条请求，回话到对面也会被无视。
				SendSwapReply(true);
				mSwapRequestPending = false;
				ApplySeatSwap();
				TodLog("[net] both sides asked at once - swapping");
			}
			else if (mSwapAskPending)
			{
				// 已经有一条待答的问句挂在玩家面前了：一次只谈一件事，多出来的按拒绝回
				SendSwapReply(false);
			}
			else
			{
				// 主循环看到 HasIncomingSwapRequest 就把面板叫出来问玩家（面板不冻心跳）
				mSwapAskPending = true;
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
				SetDead("The other player sent a malformed packet.");
				return;
			}

			// 不是我等的回话（迟到的、或对面那条请求已经被我这边成交掉了）：当它没到
			if (!mSwapRequestPending) return;

			mSwapRequestPending = false;
			if (aMsg.mAccepted)
			{
				ApplySeatSwap();
				TodLog("[net] the swap went through");
			}
			else
			{
				SetNotice("The teammate declined the swap.");
				TodLog("[net] the teammate declined the swap");
			}
		}
		break;

	default:
		// LEVEL_DONE / GAME_OVER 是后面的步骤的事，这一版还没接，
		// 直接忽略（长度合法性已经在头上查过了）。
		break;
	}
}

// ====================================================================================================
// ★ 发包
// ====================================================================================================

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
	aMsg.mDstSeat = mPeerSeat;
	aMsg.mVersion = NetProto::PROTOCOL_VERSION;
	aMsg.mBuild = NetProto::MOD_BUILD;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeHello(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) SendRaw(NetProto::MSG_HELLO, aPayload, aSize);
}

void NetSession::SendHelloAck(bool theAccepted)
{
	NetProto::MsgHelloAck aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = mPeerSeat;
	aMsg.mVersion = NetProto::PROTOCOL_VERSION;
	aMsg.mBuild = NetProto::MOD_BUILD;
	aMsg.mAccepted = theAccepted ? 1 : 0;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeHelloAck(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) SendRaw(NetProto::MSG_HELLO_ACK, aPayload, aSize);
}

bool NetSession::SendStartLevel(uint8_t theGameMode, uint32_t theLevel, int32_t theLevelSeed)
{
	if (mRole != Role::HOST || !IsConnected()) return false;

	NetProto::MsgStartLevel aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = mPeerSeat;
	aMsg.mGameMode = theGameMode;
	aMsg.mLevel = theLevel;
	aMsg.mLevelSeed = theLevelSeed;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeStartLevel(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	TodLog("[net] telling the client to start: mode %u level %u seed %d",
		(unsigned)theGameMode, (unsigned)theLevel, (int)theLevelSeed);
	return SendRaw(NetProto::MSG_START_LEVEL, aPayload, aSize);
}

void NetSession::SendStartAck()
{
	if (mRole != Role::CLIENT || !IsConnected()) return;

	NetProto::MsgStartAck aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = mPeerSeat;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeStartAck(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0)
	{
		TodLog("[net] telling the host I am entering the level");
		SendRaw(NetProto::MSG_START_ACK, aPayload, aSize);
	}
}

void NetSession::SendSwapReply(bool theAccepted)
{
	NetProto::MsgSwapReply aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = mPeerSeat;
	aMsg.mAccepted = theAccepted ? 1 : 0;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeSwapReply(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) SendRaw(NetProto::MSG_SWAP_REPLY, aPayload, aSize);
}

void NetSession::SendHeartbeat()
{
	NetProto::MsgHeartbeat aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = mPeerSeat;
	aMsg.mTick = ++mHeartbeatTick;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeHeartbeat(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) SendRaw(NetProto::MSG_HEARTBEAT, aPayload, aSize);
}

void NetSession::SendBye(uint8_t theReason)
{
	NetProto::MsgBye aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = mPeerSeat;
	aMsg.mReason = theReason;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeBye(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) SendRaw(NetProto::MSG_BYE, aPayload, aSize);
}
