#include "NetSession.h"

#include <cstring>

#include "../../Sexy.TodLib/TodDebug.h"

namespace
{

const int	HELLO_PAYLOAD_SIZE		= 6;	// src, dst, u16 version, u16 build
const int	HELLO_ACK_PAYLOAD_SIZE	= 7;	// src, dst, u16 version, u16 build, u8 accepted
const int	START_LEVEL_PAYLOAD_SIZE = 11;	// src, dst, u8 mode, u32 level, i32 seed
const int	START_ACK_PAYLOAD_SIZE	= 2;	// src, dst
const int	ESCAPED_ZOMBIE_PAYLOAD_SIZE = 22;	// src, dst, u8 row, u16 type, u8 flags, i32 ×4 血量
const int	HEARTBEAT_PAYLOAD_SIZE	= 6;	// src, dst, u32 tick
const int	BYE_PAYLOAD_SIZE		= 3;	// src, dst, u8 reason

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

uint8_t NetSession::GetRelayTargetSeat() const
{
	// M2 只有两个席位，队形就是一环：1 → 2。客户端是末席——它漏怪就是全队败，
	// 没有可传的人（这条规则由调用方处理：收到 SEAT_UNSET 就走原版判负）。
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
		// 关卡由主机定：面板是双方唯一共用的提示位，就把各自的下一步写清楚，
		// 免得客户端点了冒险按钮却什么反馈都没有（局面板照旧不冻心跳）。
		mStatusText = (mRole == Role::HOST)
			? "Connected. Pick a level from the menu."
			: "Connected. Waiting for the host to pick a level.";
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
