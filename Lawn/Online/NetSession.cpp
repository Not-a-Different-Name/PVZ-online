#include "NetSession.h"

#include <cstring>
#include <utility>

#include "../../Sexy.TodLib/TodDebug.h"
#include "QuickChat.h"

namespace
{

// 控制帧的起始编号（见 NetProtocol.h 的 ControlType）：≥ 这个数的是"服务器→客户端"的
// 控制帧，由会话层自己处理，绝不转发、也绝不落进游戏消息的 switch。游戏帧是 1..16。
const uint16_t	CONTROL_TYPE_BASE		= 0xF000;

const int	HELLO_PAYLOAD_SIZE		= 6 + NetProto::NAME_SIZE;		// src, dst, u16 version, u16 build, 名字
const int	HELLO_ACK_PAYLOAD_SIZE	= 7 + NetProto::NAME_SIZE;		// src, dst, u16 version, u16 build, u8 accepted, 名字
const int	START_LEVEL_PAYLOAD_SIZE = 25;	// src, dst, u8 mode, u32 level, i32 seed, u8 isRun, i32 runSeed, u16 runLevelIndex, u8 runMode, u8 runDiff, u8 runScale, u8 runTempo, u8 runZombotany, u8 runEndlessScene, u8 runBossFlag
const int	START_ACK_PAYLOAD_SIZE	= 3;	// src, dst, u8 accepted
const int	RUN_GO_PAYLOAD_SIZE		= 2;	// src, dst
const int	SEEDS_READY_PAYLOAD_SIZE = 3;	// src, dst, u8 ready
const int	SWAP_REQUEST_PAYLOAD_SIZE = 2;	// src, dst
const int	SWAP_REPLY_PAYLOAD_SIZE = 3;	// src, dst, u8 accepted
const int	ESCAPED_ZOMBIE_PAYLOAD_SIZE = 22;	// src, dst, u8 row, u16 type, u8 flags, i32 ×4 血量
const int	SEND_SUN_PAYLOAD_SIZE	= 4;	// src, dst, u16 amount（发阳光到账额，MOD_BUILD 38）
const int	HEARTBEAT_PAYLOAD_SIZE	= 6;	// src, dst, u32 tick
const int	BYE_PAYLOAD_SIZE		= 3;	// src, dst, u8 reason
const int	LEVEL_EXIT_PAYLOAD_SIZE	= 3;	// src, dst, u8 reason
const int	PAUSE_PAYLOAD_SIZE		= 3;	// src, dst, u8 paused
const int	LEVEL_DONE_PAYLOAD_SIZE	= 3;	// src, dst, u8 done
const int	GAME_OVER_PAYLOAD_SIZE	= 3;	// src, dst, u8 reason
const int	QUICK_CHAT_PAYLOAD_SIZE	= 3;	// src, dst, u8 id
const int	BOARD_WATCH_PAYLOAD_SIZE = 3;	// src, dst, u8 op（快照是变长帧，长度校验按 NetProtocol.h 的常量算）

// 收到的快捷聊天最多攒几条：喊话是时间敏感信息，攒一堆旧的全堆给棋盘没意义；
// 满了丢最旧、保最新（TakePendingQuickChat 处同款注释）。棋盘每帧都会来取，常态下攒不满。
const size_t	MAX_PENDING_QUICK_CHATS	= 8;

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

// 把拼好的快照数据段解析成展示模型（格式见 NetProtocol.h 的 SNAPSHOT_* 注释）。
// 任何结构对不上都返回 false——调用方保留上一份画面、等下一份自愈，绝不画半份垃圾。
bool ParseViewSnapshot(const uint8_t* theBody, int theSize, NetSession::ViewSnapshot& theSnapshot)
{
	if (theSize < NetProto::SNAPSHOT_HEADER_SIZE) return false;

	NetProto::Reader aReader(theBody, theSize);
	theSnapshot.mFlags = aReader.U8();
	uint8_t aZombieCount = aReader.U8();
	uint8_t aPlantCount = aReader.U8();
	theSnapshot.mMowers = aReader.U8();
	theSnapshot.mWave = aReader.U8();
	theSnapshot.mWaveTotal = aReader.U8();
	theSnapshot.mRows = aReader.U8();
	if (aReader.Overflowed()) return false;

	if (aZombieCount > NetProto::SNAPSHOT_ZOMBIE_MAX || aPlantCount > NetProto::SNAPSHOT_PLANT_MAX)
		return false;
	int aExpected = NetProto::SNAPSHOT_HEADER_SIZE
		+ aZombieCount * NetProto::SNAPSHOT_ZOMBIE_SIZE
		+ aPlantCount * NetProto::SNAPSHOT_PLANT_SIZE;
	if (theSize != aExpected) return false;

	theSnapshot.mZombieCount = aZombieCount;
	for (int i = 0; i < aZombieCount; i++)
	{
		theSnapshot.mZombies[i].mType = aReader.U16();
		theSnapshot.mZombies[i].mRow = aReader.U8();
		theSnapshot.mZombies[i].mX = (int)aReader.U16() - 512;	// 线上是 x+512（棋盘坐标可为负）
		theSnapshot.mZombies[i].mHP = aReader.U8();
		theSnapshot.mZombies[i].mFlags = aReader.U8();
	}
	theSnapshot.mPlantCount = aPlantCount;
	for (int i = 0; i < aPlantCount; i++)
	{
		theSnapshot.mPlants[i].mSeedType = aReader.U8();
		theSnapshot.mPlants[i].mRow = aReader.U8();
		theSnapshot.mPlants[i].mCol = aReader.U8();
		theSnapshot.mPlants[i].mHP = aReader.U8();
	}
	return !aReader.Overflowed();
}

}

NetSession::NetSession()
{
	mRole = Role::NONE;
	mTransport = Transport::DIRECT;
	mState = State::OFF;
	mLocalSeat = NetProto::SEAT_UNSET;
	mHostSeat = NetProto::SEAT_UNSET;
	ClearSeatTable();
	mFramesSincePacket = 0;
	mFramesSinceHeartbeat = 0;
	mHeartbeatTick = 0;
	mConnectPort = NetProto::DEFAULT_PORT;
	mFramesSinceRoomRequest = 0;
	mReconnectAttempts = 0;
	mFramesSinceReconnectStart = 0;
	mFramesSinceRejoinSent = 0;
	mRejoinSent = false;
	mLastStartLevelValid = false;
	mTextChinese = false;			// 默认英文；上层每帧注入（见 SetTextChinese）
	mStatusText = NetText("未连接。", "Not connected.");
	mHintText = NetText("开一间房，或输入主机 IP 加入。", "Host a game, or type the host's IP and join.");
	mShortStatus = NetText("连接断开", "Connection lost");
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
	mSwapCommitHandled = false;
	mAnyAckRejected = false;
	mNoticeFrames = 0;
	mWatchTargetSeat = NetProto::SEAT_UNSET;
	mFramesSinceWatchKeepalive = 0;
	mFramesSinceWatchSnapshot = 0;
	mWatchAssemblySeq = 0;
	mWatchAssemblyParts = 0;
	mWatchAssemblySize = 0;
	mHasViewSnapshot = false;
	mWatchEndPending = WatchEnd::NONE;
	mBoardWatcherMask = 0;
	mSnapshotSeq = 0;
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
		mSeats[aSeat].mOffline = false;
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
	mHostSeat = NetProto::SEAT_HOST;

	if (!mLink.Listen(thePort))
	{
		mState = State::DEAD;
		mStatusText = mLink.GetLastError();
		mHintText.clear();
		PushEvent(EventType::DISCONNECTED);
		return false;
	}

	mState = State::LISTENING;
	mHintText = NetText("本机 IP：", "Your IP: ") + NetLink::GetLocalIPv4Text()
		+ NetText("   端口：", "   Port: ") + std::to_string((unsigned)thePort);
	UpdateStatusText();
	return true;
}

bool NetSession::StartJoin(const char* theHost, uint16_t thePort)
{
	ResetToOff();
	mRole = Role::CLIENT;
	mTransport = Transport::DIRECT;
	mLocalSeat = NetProto::SEAT_CLIENT;
	mHostSeat = NetProto::SEAT_HOST;		// 直连的对端就是主机，固定 1 号位
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
	mHintText = NetText("端口 ", "Port ") + std::to_string((unsigned)thePort);
	UpdateStatusText();
	return true;
}

// 中继的两种开局长得几乎一样：都是 Connect 到服务器，区别只在链路一通之后发哪一条
// （CREATE_ROOM 还是带房间码的 JOIN_ROOM）。席位、房主、名册全由服务器在 WELCOME 里点，
// 所以本地席位号这会儿还不知道（SEAT_UNSET），先记下角色和地址。
bool NetSession::StartRoomHost(const char* theServer, uint16_t thePort)
{
	ResetToOff();
	mRole = Role::HOST;
	mTransport = Transport::RELAY;
	mLocalSeat = NetProto::SEAT_UNSET;
	mConnectHost = (theServer && theServer[0]) ? theServer : "127.0.0.1";
	mConnectPort = thePort;
	mRoomCode.clear();

	if (!mLink.Connect(mConnectHost.c_str(), thePort))
	{
		mState = State::DEAD;
		mStatusText = mLink.GetLastError();
		mHintText.clear();
		PushEvent(EventType::DISCONNECTED);
		return false;
	}

	mState = State::CONNECTING;
	mHintText = NetText("服务器 ", "Server ") + mConnectHost + ":" + std::to_string((unsigned)thePort);
	UpdateStatusText();
	return true;
}

bool NetSession::StartRoomJoin(const char* theServer, const char* theRoomCode, uint16_t thePort)
{
	ResetToOff();
	mRole = Role::CLIENT;
	mTransport = Transport::RELAY;
	mLocalSeat = NetProto::SEAT_UNSET;
	mConnectHost = (theServer && theServer[0]) ? theServer : "127.0.0.1";
	mConnectPort = thePort;

	// 房间码要往定长字段里放：先按"只有合法字符、大写、够长"整一遍，
	// 不到 4 位就是玩家还没输完——那也别急着连服务器，白跑一趟还得等它回绝。
	mRoomCode.clear();
	const char* aCode = theRoomCode ? theRoomCode : "";
	for (int i = 0; i < NetProto::ROOM_CODE_LEN && aCode[i]; i++)
	{
		char aChar = aCode[i];
		if (aChar >= 'a' && aChar <= 'z') aChar = (char)(aChar - 'a' + 'A');
		mRoomCode += aChar;
	}
	if ((int)mRoomCode.size() != NetProto::ROOM_CODE_LEN)
	{
		mState = State::DEAD;
		mStatusText = NetText("房间码不对——应是 4 位字母或数字。", "Bad room code - it is 4 letters and digits.");
		mShortStatus = NetText("房间码不对", "Bad room code");
		mHintText.clear();
		PushEvent(EventType::DISCONNECTED);
		return false;
	}

	if (!mLink.Connect(mConnectHost.c_str(), thePort))
	{
		mState = State::DEAD;
		mStatusText = mLink.GetLastError();
		mHintText.clear();
		PushEvent(EventType::DISCONNECTED);
		return false;
	}

	mState = State::CONNECTING;
	mHintText = NetText("服务器 ", "Server ") + mConnectHost + ":" + std::to_string((unsigned)thePort)
		+ NetText("   房间 ", "   Room ") + mRoomCode;
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
	// 队友那席要真连上才算：还在等人进来的时候，名册上那个位子该是空的。
	// 重连期是例外（MOD_BUILD 39）：名册还挂在服务器上（席位保留着），本机对"谁还在这房里"
	// 的记忆照旧——不然这一瞬间全表为空，漏怪链的末席、换位环、各等待门全当房里没人，
	// 棋盘只要跑出一帧就是误判。
	return mSeats[theSeat].mOccupied && (IsConnected() || mState == State::RECONNECTING);
}

// 掉线保留期查询（MOD_BUILD 39）：名册上的人还在，但本人掉线了、正在自动重连。
// 本机自己那格不标 offline（本机是在回自己的席位，不叫"掉线的人"）。
bool NetSession::IsSeatOffline(uint8_t theSeat) const
{
	if (theSeat == NetProto::SEAT_UNSET || theSeat > NetProto::MAX_PLAYERS) return false;
	return mSeats[theSeat].mOffline;
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

	// 从后一位数起，绕一圈找第一个上座席位。人都在的时候就是"后一位"；
	// 中间空了谁，环自己把洞跳过去。
	// 不该拿"我坐哪"去筛：这是"从 theSeat 往后看，环上下一个是谁"——谁问都一样。
	// （起点不在环上时才会绕回自己，而 SwapSeats 只从自己这席问，走的是别人那一圈。）
	for (int anOffset = 1; anOffset < NetProto::MAX_PLAYERS; anOffset++)
	{
		uint8_t aSeat = (uint8_t)(((theSeat - 1 + anOffset) % NetProto::MAX_PLAYERS) + 1);
		if (IsSeatOccupied(aSeat)) return aSeat;
	}
	return NetProto::SEAT_UNSET;
}

uint8_t NetSession::NextOccupiedSeat(uint8_t theSeat) const
{
	if (theSeat < 1 || theSeat > NetProto::MAX_PLAYERS) return NetProto::SEAT_UNSET;

	// 顺着往后找，**不绕回**：链的末端就是"没有下一家"（末席漏怪 = 全队败）。
	// 同环上那条：问的是"从 theSeat 往后是谁"，跟谁在问无关。
	for (uint8_t aSeat = (uint8_t)(theSeat + 1); aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (IsSeatOccupied(aSeat)) return aSeat;
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
		// 中继：退房递 LEAVE_ROOM，服务器立刻广播名册变动并回收席位（比直接断 TCP 语义清楚）。
		// 直连还是那句 BYE。两条都是"打个招呼再走"，不指望对方一定收得到——收不到也就是
		// 对面多等一次心跳超时，和崩溃退出一个待遇。
		if (mTransport == Transport::RELAY) SendRaw(NetProto::MSG_CLI_LEAVE_ROOM, nullptr, 0);
		else SendBye(NetProto::BYE_QUIT);
	}

	mLink.Close();
	ResetToOff();
	TodLog("[net] session closed");
}

// @pvz-online MOD_BUILD 39：重连框上的「取消并退出」。
// 只在中继重连期有意义：链路恰好活着就尽力打一声招呼（服务器立刻回收席位、队友立刻
// 看到人走了，不必干等保留期走完）；然后必须 Close——重连期的链路可能挂着一个正在
// 重试的连接线程，而 SetDead 是不动链路的（历史上由 ResetToOff/Close 负责收），
// 不收它，下次开局会被"上一个线程还在"当面挡住。收场借用 SetDead 的老路：
// DISCONNECTED 事件 → 上层收摊回主菜单（和"重连窗口耗尽"同一个出口）。
void NetSession::AbortReconnect()
{
	if (mState != State::RECONNECTING) return;

	if (mLink.IsConnected()) SendRaw(NetProto::MSG_CLI_LEAVE_ROOM, nullptr, 0);
	TodLog("[net] the player cancelled the reconnect and left the room");
	mLink.Close();
	SetDead(NetText("已取消重连。", "Reconnect cancelled.").c_str(),
		NetText("已取消重连", "Reconnect cancelled").c_str());
}

// @pvz-online MOD_BUILD 39：掉线自动重连的总闸门。中继下链路死了或沉默超时都不判死，
// 从这里进重连：置态、清计时、关掉旧链路按原地址重开，之后由 Update 的重连段每秒推进
// （链路通→发 REJOIN→等 WELCOME；发出去没人理→重开再试）。总窗口耗尽才真判死。
// 重连期内链路又死会再次走到这儿：那时只管把链路再开一次——置态、总窗口、事件通知
// 都不重来，不然玩家看到的倒计时会一遍遍从 45 秒重新开始。
// 两件事绝不碰：① 不走 Close()/SendBye——那是"体面退房"，服务器会立刻回收席位，
// 重连的意义（位子还留着）就没了；② 不清开局记账（pause/leveldone/awaitingAck）——
// 回来时全队还等着这些记忆接着跑。
void NetSession::EnterReconnecting()
{
	if (mState != State::RECONNECTING)
	{
		TodLog("[net] link lost in room %s (seat %u) - trying to reconnect",
			mRoomCode.c_str(), (unsigned)mLocalSeat);
		mState = State::RECONNECTING;
		mReconnectAttempts = 0;
		mFramesSinceReconnectStart = 0;
		ClearWatchState(WatchEnd::SILENT);	// 观看/被观看的即时报文已经发不出去了，就地收场
		ClearSwapState();
		PushEvent(EventType::RECONNECTING);
	}
	ReopenLink();
}

// 关掉旧链路、按原地址重开一条。FAILED/IDLE 时旧收包线程已经退出，Close 是即时的；
// CONNECTED 时是我们主动换一条（REJOIN 总等不来 WELCOME，多半对面还是旧服务器）。
// Connect 之后链路自己会每秒重试直到连上、或被我们再关掉，这里不用管"一时连不上"。
void NetSession::ReopenLink()
{
	mLink.Close();
	mRejoinSent = false;
	mFramesSinceRejoinSent = 0;
	if (mLink.Connect(mConnectHost.c_str(), mConnectPort)) mReconnectAttempts++;
	// Connect 失败是罕见的线程句柄泄漏场景：就当这一轮没成，Update 下一帧接着看
}

// 链路通了，向服务器报"我是原来坐 n 号位的人"。服务器核房间码 + 姓名 + 构建，对上了
// 就把席位还回来（回 WELCOME，名册照旧）。发完就掐表：5 秒没等到 WELCOME 当没收到，
// 重开链路再来一轮。
bool NetSession::SendRejoin()
{
	if (mTransport != Transport::RELAY) return false;
	if (mLocalSeat == NetProto::SEAT_UNSET) return false;
	if ((int)mRoomCode.size() != NetProto::ROOM_CODE_LEN) return false;

	NetProto::MsgCliRejoin aMsg = {};
	aMsg.mVersion = NetProto::PROTOCOL_VERSION;
	aMsg.mBuild = NetProto::MOD_BUILD;
	NetProto::SetName(aMsg.mName, mLocalName.c_str());
	memcpy(aMsg.mRoomCode, mRoomCode.data(), NetProto::ROOM_CODE_LEN);
	aMsg.mSeat = mLocalSeat;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeCliRejoin(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	mRejoinSent = true;
	mFramesSinceRejoinSent = 0;
	TodLog("[net] reconnecting: asking to rejoin room %s seat %u (attempt %d)",
		mRoomCode.c_str(), (unsigned)mLocalSeat, mReconnectAttempts);
	return SendRaw(NetProto::MSG_CLI_REJOIN, aPayload, aSize);
}

// 重连归队时补发"最后一条开局命令"——给那些在等开局 ACK、却因为对方掉线被服务器
// 把帧丢了的人。不复用 SendStartLevel：那家伙带 ClearPause/ClearLevelDone/mAnyAckRejected
// 一堆副作用，重发一条旧命令不该把全队判胜记忆清掉。
// theTargetSeat 给 SEAT_UNSET = 发给所有仍在等的人。
bool NetSession::ResendLastStartLevel(uint8_t theTargetSeat)
{
	if (mRole != Role::HOST || !mLastStartLevelValid) return false;

	NetProto::MsgStartLevel aMsg = mLastStartLevel;
	aMsg.mSrcSeat = mLocalSeat;				// 换过席位也按现席位报
	aMsg.mDstSeat = NetProto::SEAT_UNSET;	// 由 Dispatch 按目标填

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeStartLevel(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;
	if (!Dispatch(NetProto::MSG_START_LEVEL, aPayload, aSize, theTargetSeat)) return false;

	// 跟 SendStartLevel 一样：发出去了才记"在等谁回答"（只勾这次真发到的那些）
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (aSeat == mLocalSeat || !IsSeatOccupied(aSeat)) continue;
		if (theTargetSeat != NetProto::SEAT_UNSET && aSeat != theTargetSeat) continue;
		mSeats[aSeat].mAwaitingAck = true;
	}
	if (theTargetSeat == NetProto::SEAT_UNSET)
		TodLog("[net] resent the pending start command to whoever is still waiting");
	else
		TodLog("[net] resent the pending start command to seat %u", (unsigned)theTargetSeat);
	return true;
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
		// @pvz-online MOD_BUILD 39：中继里已经坐进名册的人掉线不判死——服务器把席位
		// 保留着，重开链路自动回去。直连没有第三方兜底、也没有"席位"可保留；开户/第一次
		// 握手就没成的人（还没入座）没得回——这两种照旧判死走老路。重连期内再次失败
		// 也走这儿：EnterReconnecting 认得出这不是第一回，不会重置总窗口。
		if (mTransport == Transport::RELAY && mLocalSeat != NetProto::SEAT_UNSET
			&& (mState == State::CONNECTED || mState == State::RECONNECTING))
		{
			EnterReconnecting();
			// 提前返回就轮不到末尾那次 UpdateStatusText：不补这一下，进重连的第一帧
			// 状态行还挂着掉线前的旧文案（测试/UI 都会先看到"重连了"却没见"正在重连"）。
			UpdateStatusText();
			return;
		}
		SetDead(mLink.GetLastError());
		return;
	}

	// 重连期的链路接通不走下面的"握手"分支：那边会重发 CREATE_ROOM/JOIN_ROOM，
	// 把刚接上的席位又当成"新进房"办。RECONNECTING 的链路一通走对口的 REJOIN
	//（见下面重连那一段），这里必须把它排除。
	if (aLinkState == NetLink::State::CONNECTED && mState != State::CONNECTED && mState != State::HANDSHAKING
		&& mState != State::RECONNECTING)
	{
		mState = State::HANDSHAKING;
		mFramesSincePacket = 0;
		if (mTransport == Transport::RELAY)
		{
			// 中继：链路一通就把建房/加入的请求递上去，然后掐表等 WELCOME。
			// 换了台服务器、地址错、端口被防火墙拦——服务器不应答就是不应答，
			// 不能让玩家对着"正在连接"干等（直连那边刻意不掐表，见 HANDSHAKING 那段）。
			mFramesSinceRoomRequest = 0;
			SendRoomRequest();
		}
		else if (mRole == Role::CLIENT)
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

	// 中继握手掐表：只有"请求发出去了、WELCOME 还没来"这段算数
	if (mState == State::HANDSHAKING && mTransport == Transport::RELAY)
	{
		if (++mFramesSinceRoomRequest > RELAY_HANDSHAKE_FRAMES)
		{
			SetDead(NetText("服务器没有回应。", "The server did not answer.").c_str(),
				NetText("服务器超时", "Server timeout").c_str());
			return;
		}
	}

	// 握手阶段不掐表：TCP 通了就一直等对方的 HELLO，等多久都行——对端真走了 socket 层会报，
	// 玩家不想等了面板上的 Disconnect 也是现成的。这里原先有个 3 秒上限，已按需求取消。
	if (mState == State::CONNECTED)
	{
		// 中继那边的保活是反过来的：服务器每秒 PING、我们回 PONG（见 HandleControlFrame）。
		// 独处的房主根本没有入站游戏包，全靠这条续命；自己再发心跳只是多余的流量。
		if (mTransport == Transport::DIRECT && ++mFramesSinceHeartbeat >= HEARTBEAT_FRAMES)
		{
			mFramesSinceHeartbeat = 0;
			SendHeartbeat();
		}
		// @pvz-online MOD_BUILD 39：中继的沉默判死放宽到 15 秒（网络抖几秒不该清场），
		// 真超了也不判死——进自动重连，席位在服务器上保留着。直连没有可保留的名分，
		// 照旧 5 秒判死走老路。
		if (mTransport == Transport::RELAY)
		{
			if (mFramesSincePacket > RECONNECT_SILENCE_FRAMES)
			{
				EnterReconnecting();
				UpdateStatusText();	// 同上：沉默进重连的那一帧也要就地刷新状态行
				return;
			}
		}
		else if (mFramesSincePacket > TIMEOUT_FRAMES)
		{
			SetDead(NetText("连接超时。", "Connection timed out.").c_str());
			return;
		}

		// ---- 观战：观看端补保活/掐超时，被看端删掉哑巴观众 ----
		if (IsWatching())
		{
			if (!IsSeatOccupied(mWatchTargetSeat))
			{
				// 看的人走了（中继 PEER_LEAVE / 直连掉线前的名册）：收场，棋盘给一句提示
				ClearWatchState(WatchEnd::TARGET_LEFT);
			}
			else if (mWatchTargetSeat == mLocalSeat)
			{
				// 换位把那个位子换成了我自己：看自己没有意义，安静收场
				ClearWatchState(WatchEnd::SILENT);
			}
			else
			{
				if (++mFramesSinceWatchKeepalive >= WATCH_KEEPALIVE_FRAMES)
				{
					mFramesSinceWatchKeepalive = 0;
					SendBoardWatchMsg(mWatchTargetSeat, NetProto::WATCH_KEEPALIVE);
				}
				if (++mFramesSinceWatchSnapshot >= WATCH_SNAPSHOT_FRAMES)
				{
					// 3 秒一份快照都没有：对面是旧构建（把快照帧当没见过的帧丢了）或者卡住了。
					// 先补一条 END 让对面别白推，再收场——棋盘据此提示"没有响应"。
					TodLog("[net] no snapshot for 3s - stop watching seat %u", (unsigned)mWatchTargetSeat);
					SendBoardWatchMsg(mWatchTargetSeat, NetProto::WATCH_END);
					ClearWatchState(WatchEnd::TIMEOUT);
				}
			}
		}
		for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
		{
			if (!(mBoardWatcherMask & (1u << aSeat))) continue;
			// 6 秒没说话：观看者掉线时 END 到不了，只能靠这个兜底（和心跳超时同理）
			if (++mBoardWatcherFrames[aSeat] > WATCH_KEEPER_FRAMES)
			{
				mBoardWatcherMask &= (uint8_t)~(1u << aSeat);
				TodLog("[net] watcher seat %u went quiet - stop sending snapshots", (unsigned)aSeat);
			}
		}
	}

	// @pvz-online MOD_BUILD 39：重连期。这段只干三件事：链路死了重开、链路一通发 REJOIN、
	// 发了没人理也重开再试；总窗口（45 秒）耗尽才真判死。链路还在 CONNECTING 时不打扰——
	// NetLink 自己在按秒重试。
	if (mState == State::RECONNECTING)
	{
		if (++mFramesSinceReconnectStart > RECONNECT_WINDOW_FRAMES)
		{
			SetDead(NetText("重连失败——没能回到房间。", "Reconnect failed - could not get back into the room.").c_str(),
				NetText("重连失败", "Reconnect failed").c_str());
			return;
		}

		if (aLinkState == NetLink::State::CONNECTED)
		{
			if (!mRejoinSent)
			{
				SendRejoin();
			}
			else if (++mFramesSinceRejoinSent > REJOIN_WAIT_FRAMES)
			{
				// 链路活着、服务器却迟迟不认这条 REJOIN：多半对面还是旧服务器（它把没见过的
				// 帧静默丢了，不会应也不会踢）。重开一次链路再试——重开也救不了就等窗口耗尽。
				TodLog("[net] rejoin got no WELCOME for 5s - reopening the link and trying again");
				ReopenLink();
			}
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
// 六席位时才知道是谁答的、谁还没答——P3 的放行判定就靠这张表。
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

// 还在等谁回答吗。等的是"发命令那一刻上座的席位"：等待中有人走了，他那一格连着
// mAwaitingAck 一起作废，不再堵着；等待中新进来的人没收到过这条命令，不算数。
bool NetSession::AreAllStartAcksIn() const
{
	if (!IsConnected()) return false;

	int aOthers = 0;
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (aSeat == mLocalSeat || !IsSeatOccupied(aSeat)) continue;

		if (mSeats[aSeat].mAwaitingAck) return false;
		aOthers++;
	}
	// 一个人开局（独处）不算"收齐"：这条是给"等队友"用的，没队友就别等（坑 3）。
	return aOthers > 0;
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

	// 发出去了才记账：没发出去（socket 坏了）下一帧还要再试。但"没有别的席位"不算
	// 没发出去——单人房里这份记账就是判胜（TakeAllLevelsDone）的全部依据，记不上，
	// 通关后就永远停在"等队友"上（2026-10-03 实机复现）。
	if (!Dispatch(NetProto::MSG_LEVEL_DONE, aPayload, aSize) && HasOtherSeats()) return false;

	mSeats[mLocalSeat].mLevelDone = theDone;
	TodLog("[net] told the teammates my lawn is %s (seat %u)", theDone ? "clear" : "busy again",
		(unsigned)mLocalSeat);
	return true;
}

// 单发一条判胜给某个席位，**不记账也不去重**：专供"补发给中途进来的人"。
// （我早就报过一轮了，去重记忆会把这次补发直接吞掉——而新人那边什么都没收到过。）
bool NetSession::SendLevelDoneTo(uint8_t theTarget, bool theDone)
{
	if (mRole == Role::NONE || !IsConnected()) return false;
	if (theTarget == NetProto::SEAT_UNSET || theTarget == mLocalSeat) return false;

	NetProto::MsgLevelDone aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = theTarget;
	aMsg.mDone = theDone ? 1 : 0;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeLevelDone(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;
	return Dispatch(NetProto::MSG_LEVEL_DONE, aPayload, aSize, theTarget);
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

	// @pvz-online: 一个席位也算"全队"。房间模式让"连着但就我一个人"成了合法状态
	//（一个人开的房直接开局、或队友都走了）——这时候没有别人可等，自己清完就是全队清完。
	// 旧版这里硬要 aOccupied >= 2，单人房通关后 TakeAllLevelsDone 永远不成立，
	// 关卡就死在"Waiting for the teammates..."上（2026-10-03 实机复现）。
	return aOccupied >= 1;
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

	// 跟"环上的后一位"换：六席位时 2 号位换的是 3 号位，不是"永远跟主机换"。
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
		if (mTransport == Transport::RELAY)
		{
			// 中继下我不能自己先换：服务器是唯一权威，等它的 SEAT_SWAP 广播（坑 6）。
			// 问我的那一方收到"同意"之后会去提请 COMMIT。
			TodLog("[net] accepted the swap - the other side will ask the server");
		}
		else
		{
			// 直连：两边执行的是同一次换位，各自算各自的就一致了，不用再对一次账
			ApplySeatSwap(mLocalSeat, aAsker);
			TodLog("[net] accepted the swap");
		}
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
	// 观众表同一条道理："谁在看我"跟着人走——他换到几号位，快照就得发给几号位。
	// （观看端的目标是"几号位那个场地"，反过来不跟人走：换过去看的就是新坐那位，见 Update 的观战段。）
	{
		bool aBitA = (mBoardWatcherMask & (1u << theSeatA)) != 0;
		bool aBitB = (mBoardWatcherMask & (1u << theSeatB)) != 0;
		mBoardWatcherMask &= (uint8_t)~((1u << theSeatA) | (1u << theSeatB));
		if (aBitA) mBoardWatcherMask |= (uint8_t)(1u << theSeatB);
		if (aBitB) mBoardWatcherMask |= (uint8_t)(1u << theSeatA);
		std::swap(mBoardWatcherFrames[theSeatA], mBoardWatcherFrames[theSeatB]);
	}
	// 房主也是个"人"不是个座位：换位把他挪到别处，找他的地址（开局应答的去处）得跟着走。
	// 不跟着走的话，房主换完位就再也收不到 ACK，开局会一直卡在"等队友"上。
	if (mHostSeat == theSeatA) mHostSeat = theSeatB;
	else if (mHostSeat == theSeatB) mHostSeat = theSeatA;

	SetNotice((NetText("已换位——你现在是 P", "Swapped - you are now P")
		+ std::to_string((unsigned)mLocalSeat) + NetText("。", ".")).c_str());
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
	mPendingQuickChats.clear();
	mPendingSunGifts.clear();
	// 观战也是"这一局"的事：先跟还在看我场地的观众逐个道别（他们那边立刻收场、不用等
	// 6 秒超时兜底），再清观众表；我自己的观看状态整段作废——半份快照和上一关的定格
	// 都不跨局（下一关按住 V 重新点播即可）。
	if (IsConnected() && mBoardWatcherMask)
	{
		uint8_t aWatchers[NetProto::MAX_PLAYERS];
		int aCount = GetBoardWatcherSeats(aWatchers, NetProto::MAX_PLAYERS);
		for (int i = 0; i < aCount; i++) SendBoardWatchMsg(aWatchers[i], NetProto::WATCH_END);
	}
	mBoardWatcherMask = 0;
	ClearWatchState(WatchEnd::SILENT);
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

bool NetSession::IsLastRelaySeat() const
{
	// 链没有下一家 = 本机是末位。再看一眼人数：单人房（只有自己坐席）不算——
	// 那是"没人可传"，不是"排在队尾"，推车不往那儿发。
	return GetRelayTargetSeat() == NetProto::SEAT_UNSET && GetOccupiedSeatCount() >= 2;
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

bool NetSession::SendSunGift(uint16_t theAmount, uint8_t theTargetSeat)
{
	// @pvz-online: 发阳光（MOD_BUILD 38）——定向发给按键直选的席位（本地 G + 1-6 选人）。
	// 目标必须是上座、非自己的席位；与漏怪链无关，末席也能发。
	if (theTargetSeat == NetProto::SEAT_UNSET || theTargetSeat == mLocalSeat
		|| !IsSeatOccupied(theTargetSeat) || !IsConnected()) return false;

	NetProto::MsgSendSun aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = theTargetSeat;
	aMsg.mAmount = theAmount;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeSendSun(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	TodLog("[net] sending sun to seat %u: +%u", (unsigned)theTargetSeat, (unsigned)theAmount);
	return Dispatch(NetProto::MSG_SEND_SUN, aPayload, aSize, theTargetSeat);
}

bool NetSession::TakePendingSunGift(NetProto::MsgSendSun& theMsg)
{
	if (mPendingSunGifts.empty()) return false;

	theMsg = mPendingSunGifts.front();
	mPendingSunGifts.erase(mPendingSunGifts.begin());
	return true;
}

bool NetSession::SendQuickChat(uint8_t theId)
{
	// 编号是线路身份，越界一律不发——坏编号在接收端是"丢弃"待遇，发出去只会骗队友
	if (mRole == Role::NONE || !IsConnected()) return false;
	if (!QuickChat::IsValidId(theId)) return false;

	NetProto::MsgQuickChat aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mId = theId;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeQuickChat(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	TodLog("[net] seat %u sends a quick chat (id %u)", (unsigned)mLocalSeat, (unsigned)theId);
	// 默认扇出：所有其他上座席位各一份；单人房没人可发时 Dispatch 返回 false
	return Dispatch(NetProto::MSG_QUICK_CHAT, aPayload, aSize);
}

bool NetSession::TakePendingQuickChat(NetProto::MsgQuickChat& theMsg)
{
	if (mPendingQuickChats.empty()) return false;

	// 先进先出：聊天按到达顺序显示；满员挤掉的旧条在入队时就丢了，这里只管往外递
	theMsg = mPendingQuickChats.front();
	mPendingQuickChats.erase(mPendingQuickChats.begin());
	return true;
}

// ====================================================================================================
// ★ 观战（队友场地查看）
// ====================================================================================================

bool NetSession::SendBoardWatchMsg(uint8_t theSeat, uint8_t theOp)
{
	NetProto::MsgBoardWatch aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = theSeat;
	aMsg.mOp = theOp;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeBoardWatch(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	// 单播：看谁只打扰谁。中继空席位会被服务器丢弃、直连只有一个对端，都由 Dispatch 管。
	return Dispatch(NetProto::MSG_BOARD_WATCH, aPayload, aSize, theSeat);
}

bool NetSession::BeginWatch(uint8_t theSeat)
{
	if (!IsConnected()) return false;
	// 席位得是个真上座的号、而且不能是自己——看自己场地没有意义（屏幕本来就在）
	if (theSeat < 1 || theSeat > NetProto::MAX_PLAYERS
		|| theSeat == mLocalSeat || !IsSeatOccupied(theSeat))
		return false;

	// 换目标（按住 V 期间按 1-6 切人就走这条路）：先跟旧目标打声招呼，别让他白推
	if (IsWatching() && mWatchTargetSeat != theSeat)
		SendBoardWatchMsg(mWatchTargetSeat, NetProto::WATCH_END);

	mWatchTargetSeat = theSeat;
	mFramesSinceWatchKeepalive = 0;
	mFramesSinceWatchSnapshot = 0;
	mWatchAssemblyParts = 0;
	mWatchAssemblySize = 0;
	mWatchEndPending = WatchEnd::NONE;

	if (!SendBoardWatchMsg(theSeat, NetProto::WATCH_BEGIN))
	{
		// 发都没发出去（对面恰好在上一帧走了）：别干等 3 秒超时，直接收场
		ClearWatchState(WatchEnd::TARGET_LEFT);
		return false;
	}
	TodLog("[net] watching seat %u's board", (unsigned)theSeat);
	return true;
}

void NetSession::EndWatch(bool theNotify)
{
	if (!IsWatching()) return;

	if (theNotify) SendBoardWatchMsg(mWatchTargetSeat, NetProto::WATCH_END);
	ClearWatchState(WatchEnd::USER);
}

void NetSession::ClearWatchState(WatchEnd theReason)
{
	bool aWasWatching = IsWatching();
	mWatchTargetSeat = NetProto::SEAT_UNSET;
	mFramesSinceWatchKeepalive = 0;
	mFramesSinceWatchSnapshot = 0;
	mWatchAssemblyParts = 0;
	mWatchAssemblySize = 0;
	mHasViewSnapshot = false;
	// "为什么结束"只在真的在看、且有原因可留时挂出去（棋盘取一次就清）
	if (aWasWatching && theReason != WatchEnd::NONE)
		mWatchEndPending = theReason;
}

NetSession::WatchEnd NetSession::TakeWatchEnded()
{
	WatchEnd aResult = mWatchEndPending;
	mWatchEndPending = WatchEnd::NONE;
	return aResult;
}

bool NetSession::TakeViewSnapshot(ViewSnapshot& theSnapshot)
{
	if (!mHasViewSnapshot) return false;

	theSnapshot = mLastSnapshot;
	mHasViewSnapshot = false;
	return true;
}

int NetSession::GetBoardWatcherSeats(uint8_t* theSeats, int theMax) const
{
	int aCount = 0;
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS && aCount < theMax; aSeat++)
	{
		if (mBoardWatcherMask & (1u << aSeat)) theSeats[aCount++] = aSeat;
	}
	return aCount;
}

bool NetSession::SendBoardSnapshot(uint8_t theViewerSeat, const uint8_t* theBody, int theSize)
{
	if (!IsConnected()) return false;
	if (!theBody || theSize < 0) return false;
	if (theViewerSeat < 1 || theViewerSeat > NetProto::MAX_PLAYERS) return false;
	if (!(mBoardWatcherMask & (1u << theViewerSeat))) return false;	// 没人在看（或已经走了）就别发
	if (theSize > NetProto::SNAPSHOT_MAX_PARTS * NetProto::SNAPSHOT_PART_PAYLOAD) return false;

	int aCount = (theSize + NetProto::SNAPSHOT_PART_PAYLOAD - 1) / NetProto::SNAPSHOT_PART_PAYLOAD;
	if (aCount < 1) aCount = 1;		// 空数据也至少发一片（正常情况下头就有 7 字节，到不了这儿）

	// 代次每"份" +1。两个观众各调一次会各 +1：seq 只在同一条观看流里比较，
	// 观众端只认相等/不等，跳号无碍（而且跳号还顺手把"两代拼一起"挡死）。
	mSnapshotSeq++;

	for (int aPart = 0; aPart < aCount; aPart++)
	{
		int aOffset = aPart * NetProto::SNAPSHOT_PART_PAYLOAD;
		int aDataSize = theSize - aOffset;
		if (aDataSize > NetProto::SNAPSHOT_PART_PAYLOAD) aDataSize = NetProto::SNAPSHOT_PART_PAYLOAD;

		NetProto::MsgBoardSnapshot aMsg;
		memset(&aMsg, 0, sizeof(aMsg));
		aMsg.mSrcSeat = mLocalSeat;
		aMsg.mDstSeat = theViewerSeat;
		aMsg.mSeq = mSnapshotSeq;
		aMsg.mPart = (uint8_t)aPart;
		aMsg.mCount = (uint8_t)aCount;
		if (aDataSize > 0) memcpy(aMsg.mData, theBody + aOffset, aDataSize);

		uint8_t aPayload[NetProto::MAX_PAYLOAD];
		int aPayloadSize = NetProto::EncodeBoardSnapshot(aPayload, (int)sizeof(aPayload), aMsg, aDataSize);
		if (aPayloadSize <= 0) return false;

		if (!Dispatch(NetProto::MSG_BOARD_SNAPSHOT, aPayload, aPayloadSize, theViewerSeat))
			return false;	// 中间断了：这一份没发完，观众端把半份作废、等下一份自愈
	}
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
	mHostSeat = NetProto::SEAT_UNSET;
	mRoomCode.clear();
	ClearSeatTable();			// 名册和每席位的记账跟着这一局作废；自己的名字留着（见 SetLocalName）
	mFramesSincePacket = 0;
	mFramesSinceHeartbeat = 0;
	mHeartbeatTick = 0;
	mConnectHost.clear();
	mConnectPort = NetProto::DEFAULT_PORT;
	mFramesSinceRoomRequest = 0;
	// 重连记账跟着这一局作废：下一条链路是新会话，不该背着上局的尝试数/等待窗口
	mReconnectAttempts = 0;
	mFramesSinceReconnectStart = 0;
	mFramesSinceRejoinSent = 0;
	mRejoinSent = false;
	mLastStartLevelValid = false;
	mStatusText = NetText("未连接。", "Not connected.");
	mHintText = NetText("开一间房，或输入主机 IP 加入。", "Host a game, or type the host's IP and join.");
	mEvents.clear();
	DiscardLevelPackets();
	ClearSwapState();
	mNoticeText.clear();
	mNoticeFrames = 0;
}

void NetSession::SetConnected(bool theWasReconnect)
{
	mState = State::CONNECTED;
	mFramesSincePacket = 0;
	mFramesSinceHeartbeat = 0;

	// @pvz-online MOD_BUILD 39：重连归队走这条早退——不重跑下面那套首连的动静。
	// 尤其不能推 CONNECTED：上层拿它当"有新客人"办（拉菜单、开局的那些动作），
	// 归队的人只该看到一句"回来了"，游戏场景原地不动。
	if (theWasReconnect)
	{
		mRejoinSent = false;
		mFramesSinceReconnectStart = 0;		// 窗口收工；下一次掉线从头起算
		TodLog("[net] reconnected to room %s - back in seat %u (%d player(s) in the room)",
			mRoomCode.c_str(), (unsigned)mLocalSeat, GetOccupiedSeatCount());
		PushEvent(EventType::RECONNECTED);
		return;
	}

	if (mTransport == Transport::RELAY)
	{
		// 中继：名册是服务器给的，日志里把"我坐几号位、房主是谁、房里有几个人"一次说清
		TodLog("[net] joined room %s - local seat %u (build %u), host seat %u, %d player(s)",
			mRoomCode.c_str(), (unsigned)mLocalSeat, (unsigned)NetProto::MOD_BUILD,
			(unsigned)mHostSeat, GetOccupiedSeatCount());
	}
	else
	{
		uint8_t aPeer = GetPeerSeat();
		TodLog("[net] handshake complete - local seat %u (%s) build %u, peer seat %u (%s) build %u",
			(unsigned)mLocalSeat, mLocalName.c_str(), (unsigned)NetProto::MOD_BUILD,
			(unsigned)aPeer, GetSeatName(aPeer).c_str(), (unsigned)GetPeerBuild());
	}
	// 两边构建代次不一样：连还是要连的（12 起不再拒绝），但得提示一句——行为不配套
	// 的毛病（"漏怪没落地"那类）看起来都像"游戏坏了"，有这行才知道该去更新哪边。
	if (IsBuildDifferent())
	{
		TodLog("[net] builds differ - local %u, peer %u; playing anyway",
			(unsigned)NetProto::MOD_BUILD, (unsigned)GetPeerBuild());
		SetNotice(NetText("两边构建版本不同——出怪行为对不上时请把两边更新到同一版本。",
			"Builds differ - update both machines if things break.").c_str());
	}
	PushEvent(EventType::CONNECTED);
}

void NetSession::SetDead(const char* theReason, const char* theShortReason)
{
	if (mState == State::DEAD) return;

	mState = State::DEAD;
	mStatusText = (theReason && theReason[0]) ? theReason : NetText("连接断开。", "Connection lost.");
	mShortStatus = (theShortReason && theShortReason[0]) ? theShortReason : NetText("连接断开", "Connection lost");
	mHintText.clear();
	// 挂着的换位请求跟着连接一起作废：断了就没得换了，面板上那个问句也得收掉
	ClearSwapState();
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
		mStatusText = NetText("未连接。", "Not connected.");
		break;
	case State::LISTENING:
		// 不写"另一个玩家"：最多六个席位，主机等的是"人"，不是那一个特定的人。
		mStatusText = NetText("等待队友加入……", "Waiting for players to join...");
		break;
	case State::CONNECTING:
		{
			int anAttempts = GetConnectAttempts();
			mStatusText = NetText("正在连接 ", "Connecting to ") + mConnectHost + NetText("……", "...");
			// 连不上会一直重试（没有时间上限了），所以重试次数得露出来，
			// 不然"还在试"和"卡死了"看起来一模一样。
			if (anAttempts > 1)
			{
				mStatusText += NetText("（第 ", " (attempt ") + std::to_string((unsigned)anAttempts) + NetText(" 次重试）", ")");
			}
		}
		break;
	case State::HANDSHAKING:
		// 中继这会儿是"TCP 通了，等服务器点名册"（WELCOME），直连是等对方的 HELLO/HELLO_ACK
		mStatusText = (mTransport == Transport::RELAY)
			? NetText("正在联系服务器……", "Contacting the server...")
			: NetText("已连接，握手中……", "Connected. Shaking hands...");
		break;
	case State::CONNECTED:
		{
			// 关卡由主机定：面板是双方唯一共用的提示位，就把各自的下一步写清楚，
			// 免得客户端点了冒险按钮却什么反馈都没有（局面板照旧不冻心跳）。
			// 换位这件事谁先谁后不一样，所以状态行得按"谁在等谁"分开写。
			if (HasIncomingSwapRequest())
				mStatusText = NetText("队友想和你换位——请点同意或拒绝。", "The teammate wants to swap positions - Accept or Reject.");
			else if (IsSwapRequestPending())
				mStatusText = NetText("换位请求已发出——等待队友答复。", "Swap asked - waiting for the teammate to answer.");
			else if (IsBuildDifferent())
			{
				// 构建代次不同：连得上、能玩（12 起不再拒绝，见握手处），这句是常驻提醒。
				// 即时说明几秒就没了，而"这俩不是一套"得一直看得见——真撞上不配套的行为
				// 时，这行就是"该去更新了"的凭据。小条上也有一份短的，见 OnlineStatusWidget。
				std::string aBuilds = NetText("两边构建版本不同（本机 ", "Builds differ (you ")
					+ std::to_string((unsigned)NetProto::MOD_BUILD)
					+ NetText(" / 对方 ", " / peer ") + std::to_string((unsigned)GetPeerBuild())
					+ NetText("）。", ").");
				mStatusText = (mRole == Role::HOST)
					? aBuilds + NetText(" 请选关卡。", " Pick a level.")
					: aBuilds + NetText(" 等待主机选关。", " Waiting for the host.");
			}
			else
				// 连上之后该点哪块牌子，按当前设计是"看情况"的：想一起打单关走 PUZZLE，
				// 组队闯关要等 R5——所以这儿不说牌子名，只说"主机来挑"。
				mStatusText = (mRole == Role::HOST)
					? NetText("已连接。请在菜单里选一关。", "Connected. Pick a level from the menu.")
					: NetText("已连接。等待主机选关。", "Connected. Waiting for the host to pick a level.");

			// 提示行借来写清"我是几号位、漏怪往哪走"：连上之后 IP 已经没用了
			// （输入框里还留着），而位置是开局前要拿主意的事（面板里的 Swap）。
			// 有即时说明（刚换完 / 被拒绝）时先让说明占着，几秒后自己回到席位那行。
			if (mNoticeFrames > 0)
			{
				mHintText = mNoticeText;
			}
			else
			{
				// 席位号与"漏怪往哪走"都按当下的席位算：六席位时 2、3 号位的怪是要往后
				// 接着传的，写死"你是 P2、你接队友的漏怪"就把中间席位说成了末席。
				// 中继把房间码也带上：房主要念给朋友，其他人核对一下自己进对了房。
				uint8_t aNext = GetRelayTargetSeat();
				mHintText = (mTransport == Transport::RELAY)
					? (NetText("房间 ", "Room ") + mRoomCode + NetText("——", " - ")) : std::string();
				mHintText += NetText("你是 P", "You are P") + std::to_string((unsigned)mLocalSeat);
				mHintText += (aNext == NetProto::SEAT_UNSET)
					? NetText("——末位：这里漏怪会直接判全队失败。", " - the last seat: a leak here loses the game.")
					: (NetText("——你的漏怪往后传给 P", " - your leaks pass on to P")
						+ std::to_string((unsigned)aNext) + NetText("。", "."));
			}
		}
		break;
	case State::RECONNECTING:
		{
			// @pvz-online MOD_BUILD 39：重连期状态行。剩余秒数按总窗口倒着数——数到 0
			// 就是"重连失败"那条路，玩家心里有数就不会对着"正在重连"干瞪眼。
			int aLeftSec = (RECONNECT_WINDOW_FRAMES - mFramesSinceReconnectStart) / 100;
			if (aLeftSec < 0) aLeftSec = 0;
			unsigned anAttempt = (mReconnectAttempts < 1) ? 1u : (unsigned)mReconnectAttempts;
			mStatusText = NetText("掉线了——正在重连（第 ", "Lost the link - reconnecting (attempt ")
				+ std::to_string(anAttempt)
				+ NetText(" 次，还剩 ", ", ") + std::to_string((unsigned)aLeftSec) + NetText(" 秒）……", "s left)...");
			mHintText = NetText("房间 ", "Room ") + mRoomCode
				+ NetText("——你的席位保留着，连上就能接着打。", " - your seat is being held; you can continue once connected.");
		}
		break;
	case State::DEAD:
	default:
		break;			// 死因由 SetDead 写死了，别覆盖
	}
}

void NetSession::PushEvent(EventType theType, uint8_t theSeat)
{
	Event anEvent;
	anEvent.mType = theType;
	anEvent.mSeat = theSeat;
	mEvents.push_back(anEvent);
}

// 换位那几条挂起状态一起清：请求、问句、已经发出的 COMMIT。换成了、被拒了、断了、收摊了
// 都走这儿——漏掉任何一条，面板上就会挂着一个永远等不到回话的"等待中"。
void NetSession::ClearSwapState()
{
	mSwapRequestSeat = NetProto::SEAT_UNSET;
	mSwapAskSeat = NetProto::SEAT_UNSET;
	mSwapCommitHandled = false;
}

// ====================================================================================================
// ★ 收包
// ====================================================================================================

void NetSession::HandlePacket(const NetLink::Packet& thePacket)
{
	if (thePacket.mSize < NetProto::HEADER_SIZE)
	{
		SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
		return;
	}

	uint16_t aType = (uint16_t)(thePacket.mData[0] | (thePacket.mData[1] << 8));
	uint16_t aPayloadSize = (uint16_t)(thePacket.mData[2] | (thePacket.mData[3] << 8));
	if (NetProto::HEADER_SIZE + aPayloadSize != thePacket.mSize)
	{
		SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
		return;
	}

	const uint8_t* aPayload = thePacket.mData + NetProto::HEADER_SIZE;

	// 服务器控制帧（≥0xF000）不落进游戏消息那个 switch：它说的是名册和路由，整段交给
	// HandleControlFrame。直连模式下这些帧根本不该出现（对端是另一个玩家，不是服务器），
	// 真收到就丢掉——它不是这条链路该有的东西。
	if (aType >= CONTROL_TYPE_BASE)
	{
		HandleControlFrame(aType, aPayload, aPayloadSize);
		return;
	}

	switch (aType)
	{
	case NetProto::MSG_HELLO:
		{
			// 中继下身份是服务器点的：HELLO 这套"自己报家门"既用不上也不认（坑 7）。
			// 直连路径逐字不动，见下面那几段。
			if (mTransport == Transport::RELAY) return;
			if (mRole != Role::HOST) return;		// 只有主机收 HELLO

			// 长度对不上 HELLO 只有一个解释：对面是别的构建版。旧包（HELLO 是 4 字节、
			// 没有 build 号）正好落在这儿——这正是"另一边跑着上个版本"的样子，
			// 要给出能照做的提示，不能报成含糊的坏包。
			if (aPayloadSize != HELLO_PAYLOAD_SIZE)
			{
				SendHelloAck(false, DirectPeerSeat());
				SetDead(NetText("构建版本不一致——请把所有机器更新到同一版本。", "Build mismatch - update every machine to the same build.").c_str(), NetText("构建版本不一致", "Build mismatch").c_str());
				return;
			}

			NetProto::MsgHello aMsg;
			if (!NetProto::DecodeHello(aPayload, aPayloadSize, aMsg))
			{
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}

			if (aMsg.mVersion != NetProto::PROTOCOL_VERSION)
			{
				SendHelloAck(false, DirectPeerSeat());
				SetDead(NetText("协议版本不一致——所有人必须跑同一版本。", "Version mismatch - all players must run the same build.").c_str(), NetText("协议版本不一致", "Version mismatch").c_str());
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
				SetDead(NetText("对方发来的席位号不合法。", "A player sent a bogus seat number.").c_str());
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
			if (mTransport == Transport::RELAY) return;		// 同 HELLO：中继不认这套
			if (mRole != Role::CLIENT) return;

			if (aPayloadSize != HELLO_ACK_PAYLOAD_SIZE)
			{
				SetDead(NetText("构建版本不一致——请把所有机器更新到同一版本。", "Build mismatch - update every machine to the same build.").c_str(), NetText("构建版本不一致", "Build mismatch").c_str());
				return;
			}

			NetProto::MsgHelloAck aMsg;
			if (!NetProto::DecodeHelloAck(aPayload, aPayloadSize, aMsg))
			{
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}

			if (!aMsg.mAccepted)
			{
				// 主机拒接的原因只剩两种：协议版本对不上，或者它读不出我们的 HELLO（旧构建）。
				// ACK 里带着主机的版本号，正好能分辨——提示得指得出该更新哪边，别一律怪"构建"。
				if (aMsg.mVersion != NetProto::PROTOCOL_VERSION)
					SetDead(NetText("协议版本不一致——所有人必须跑同一版本。", "Version mismatch - all players must run the same build.").c_str(), NetText("协议版本不一致", "Version mismatch").c_str());
				else
					SetDead(NetText("构建版本不一致——请把所有机器更新到同一版本。", "Build mismatch - update every machine to the same build.").c_str(), NetText("构建版本不一致", "Build mismatch").c_str());
				return;
			}
			if (aMsg.mVersion != NetProto::PROTOCOL_VERSION)
			{
				SetDead(NetText("协议版本不一致——所有人必须跑同一版本。", "Version mismatch - all players must run the same build.").c_str(), NetText("协议版本不一致", "Version mismatch").c_str());
				return;
			}
			// 构建代次不一样不再拒绝（同主机侧，见 MSG_HELLO 那段）：记下、提示、照常连。

			// 席位号要过和主机侧同样的筛：以前这里是"对面固定 1 号位"的写死假设，
			// 现在改听主机自报，也得防它报个越界或本机的席位（理由见 MSG_HELLO 那段）。
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS
				|| aMsg.mSrcSeat == mLocalSeat)
			{
				SetDead(NetText("对方发来的席位号不合法。", "A player sent a bogus seat number.").c_str());
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}
			// 收到就是活着——mFramesSincePacket 已经在调用处清零了
		}
		break;

	case NetProto::MSG_BYE:
		{
			// 中继下"谁走了"只认服务器的 PEER_LEAVE/ROOM_CLOSED（坑 7）：别人手里那条
			// 游戏帧通道传不了这种话，真收到说明对面是个老构建，丢掉别理。
			if (mTransport == Transport::RELAY) return;

			NetProto::MsgBye aMsg;
			if (aPayloadSize != BYE_PAYLOAD_SIZE || !NetProto::DecodeBye(aPayload, aPayloadSize, aMsg))
			{
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}
			SetDead(NetText("有玩家离开了游戏。", "A player left the game.").c_str());
		}
		break;

	case NetProto::MSG_ESCAPED_ZOMBIE:
		{
			NetProto::MsgEscapedZombie aMsg;
			if (aPayloadSize != ESCAPED_ZOMBIE_PAYLOAD_SIZE || !NetProto::DecodeEscapedZombie(aPayload, aPayloadSize, aMsg))
			{
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
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

	case NetProto::MSG_SEND_SUN:
		{
			NetProto::MsgSendSun aMsg;
			if (aPayloadSize != SEND_SUN_PAYLOAD_SIZE || !NetProto::DecodeSendSun(aPayload, aPayloadSize, aMsg))
			{
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}

			// 同漏怪口径：收包链不碰棋盘，只排队；LawnApp 每帧取走入账。
			mPendingSunGifts.push_back(aMsg);
			TodLog("[net] sun gift from seat %u: +%u",
				(unsigned)aMsg.mSrcSeat, (unsigned)aMsg.mAmount);
		}
		break;

	case NetProto::MSG_START_ACK:
		{
			if (mRole != Role::HOST) return;		// 只有主机在等这条

			NetProto::MsgStartAck aMsg;
			if (aPayloadSize != START_ACK_PAYLOAD_SIZE || !NetProto::DecodeStartAck(aPayload, aPayloadSize, aMsg))
			{
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}
			// 席位号得是个真坐在席位上的号：越界 / 本机 / 空位都丢掉这一条——
			// 它是包里的一个字节，直接拿去索引席位表就是写穿。
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS
				|| aMsg.mSrcSeat == mLocalSeat || !IsSeatOccupied(aMsg.mSrcSeat))
			{
				TodLog("[net] threw away a START_ACK with a bogus seat (%u)", (unsigned)aMsg.mSrcSeat);
				break;
			}

			// 存到他那格里就走：开局要换场景、动一堆 UI，那是主循环的活。
			mSeats[aMsg.mSrcSeat].mAckState = aMsg.mAccepted ? 1 : 2;
			mSeats[aMsg.mSrcSeat].mAwaitingAck = false;
			if (!aMsg.mAccepted) mAnyAckRejected = true;	// 粘着：取 ACK 的人取走了细节，这条留着
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
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
				if (mTransport == Transport::RELAY)
				{
					// 中继下换位由服务器落实：两边各提请一次会换两次（等于换回去），
					// 所以只让席位号小的一方发 COMMIT，另一边锁着面板等广播（坑 6）。
					uint8_t aOther = aMsg.mSrcSeat;
					ClearSwapState();
					mSwapRequestSeat = aOther;		// 广播落地之前面板一直灰着
					mSwapCommitHandled = true;		// 不管谁发，都已经有人接手了
					if (mLocalSeat < aOther) SendSeatSwapCommit(mLocalSeat, aOther);
					TodLog("[net] both sides asked at once - %s", (mLocalSeat < aOther)
						? "asking the server to swap" : "waiting for the server");
				}
				else
				{
					mSwapRequestSeat = NetProto::SEAT_UNSET;
					ApplySeatSwap(mLocalSeat, aMsg.mSrcSeat);
					TodLog("[net] both sides asked at once - swapping");
				}
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}

			// 不是我等的回话（迟到的、或对面那条请求已经被我这边成交掉了）：当它没到
			if (!IsSwapRequestPending()) return;
			if (aMsg.mSrcSeat != mSwapRequestSeat) return;		// 也不是我问的那个人回的

			uint8_t aSeat = mSwapRequestSeat;
			mSwapRequestSeat = NetProto::SEAT_UNSET;
			if (aMsg.mAccepted)
			{
				if (mTransport == Transport::RELAY)
				{
					// 服务器是唯一权威：把 COMMIT 递上去，自己先不换——SEAT_SWAP 广播到了才换。
					// 两边同时按的那条路已经有人接手了（mSwapCommitHandled），别发第二条。
					mSwapRequestSeat = aSeat;		// 广播落地之前面板继续灰着
					if (!mSwapCommitHandled)
					{
						SendSeatSwapCommit(mLocalSeat, aSeat);
						mSwapCommitHandled = true;
						TodLog("[net] the swap was accepted - asking the server to confirm");
					}
					break;
				}
				ApplySeatSwap(mLocalSeat, aSeat);
				TodLog("[net] the swap went through");
			}
			else
			{
				SetNotice(NetText("队友拒绝了换位。", "The teammate declined the swap.").c_str());
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}

			// 席位号得是个真坐在席位上的号：越界 / 本机 / 空位都丢掉这一条——
			// 它是包里的一个字节，直接拿去当数组下标会写穿。
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS
				|| aMsg.mSrcSeat == mLocalSeat || !IsSeatOccupied(aMsg.mSrcSeat))
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
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
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}
			// 席位号得是个真坐在席位上的号：越界 / 本机 / 空位都丢掉这一条（同 LEVEL_DONE 的理由）
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS
				|| aMsg.mSrcSeat == mLocalSeat || !IsSeatOccupied(aMsg.mSrcSeat))
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

	case NetProto::MSG_QUICK_CHAT:
		{
			// 双向：局内快捷聊天（编号查 QuickChat.h）。事件不是状态，不去重、按到达顺序入队
			NetProto::MsgQuickChat aMsg;
			if (aPayloadSize != QUICK_CHAT_PAYLOAD_SIZE || !NetProto::DecodeQuickChat(aPayload, aPayloadSize, aMsg))
			{
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}
			// 席位号越界/空位/本机、或编号越界就丢掉这一条、不断线（防御坏包与构建混搭，
			// 同 SEEDS_READY 的理由）
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS
				|| aMsg.mSrcSeat == mLocalSeat || !IsSeatOccupied(aMsg.mSrcSeat))
			{
				TodLog("[net] threw away a QUICK_CHAT with a bogus seat (%u)", (unsigned)aMsg.mSrcSeat);
				break;
			}
			if (!QuickChat::IsValidId(aMsg.mId))
			{
				TodLog("[net] threw away a QUICK_CHAT with a bogus id (%u)", (unsigned)aMsg.mId);
				break;
			}
			// 满了丢最旧、保最新：喊话是时间敏感信息，背压时最新一条最有用
			if (mPendingQuickChats.size() >= MAX_PENDING_QUICK_CHATS)
			{
				mPendingQuickChats.erase(mPendingQuickChats.begin());
				TodLog("[net] quick chat queue full, dropped the oldest one");
			}
			mPendingQuickChats.push_back(aMsg);
			TodLog("[net] seat %u says something (id %u)", (unsigned)aMsg.mSrcSeat, (unsigned)aMsg.mId);
		}
		break;

	case NetProto::MSG_BOARD_WATCH:
		{
			// 观战点播/保活/收播（op 见 WatchOp）。**被看端和观看端的收件都在这里**：
			// BEGIN/KEEPALIVE 落在被看端的观众表上；END 落在被看端（停推）或观看端
			//（被看方离开关卡时的"别看了"，观看起来是对方主动停推）。
			NetProto::MsgBoardWatch aMsg;
			if (aPayloadSize != BOARD_WATCH_PAYLOAD_SIZE || !NetProto::DecodeBoardWatch(aPayload, aPayloadSize, aMsg))
			{
				SetDead(NetText("对方发来的数据包有问题。", "A player sent a malformed packet.").c_str());
				return;
			}
			// 席位号得是个真坐在席位上的号（同 QUICK_CHAT 的理由：防坏包与构建混搭）
			if (aMsg.mSrcSeat < 1 || aMsg.mSrcSeat > NetProto::MAX_PLAYERS
				|| aMsg.mSrcSeat == mLocalSeat || !IsSeatOccupied(aMsg.mSrcSeat))
			{
				TodLog("[net] threw away a BOARD_WATCH with a bogus seat (%u)", (unsigned)aMsg.mSrcSeat);
				break;
			}

			if (aMsg.mOp == NetProto::WATCH_BEGIN || aMsg.mOp == NetProto::WATCH_KEEPALIVE)
			{
				// BEGIN 建表，KEEPALIVE 续命。KEEPALIVE 顺带兜底补位：万一 BEGIN 恰好丢了
				// （极边缘），别让观看者对着这边干等到超时——收到保活就算他还在看。
				if (!(mBoardWatcherMask & (1u << aMsg.mSrcSeat)))
				{
					mBoardWatcherMask |= (uint8_t)(1u << aMsg.mSrcSeat);
					TodLog("[net] seat %u starts watching my board", (unsigned)aMsg.mSrcSeat);
				}
				mBoardWatcherFrames[aMsg.mSrcSeat] = 0;
			}
			else if (aMsg.mOp == NetProto::WATCH_END)
			{
				// 停止观看他：先把我看他这条停掉……
				if (IsWatching() && aMsg.mSrcSeat == mWatchTargetSeat)
				{
					TodLog("[net] seat %u stopped sharing", (unsigned)aMsg.mSrcSeat);
					ClearWatchState(WatchEnd::SILENT);
				}
				// ……再把他看我这条停掉（两个方向互不相干，可能同时成立）
				if (mBoardWatcherMask & (1u << aMsg.mSrcSeat))
				{
					mBoardWatcherMask &= (uint8_t)~(1u << aMsg.mSrcSeat);
					TodLog("[net] seat %u stops watching my board", (unsigned)aMsg.mSrcSeat);
				}
			}
			else
			{
				TodLog("[net] threw away a BOARD_WATCH with a bogus op (%u)", (unsigned)aMsg.mOp);
			}
		}
		break;

	case NetProto::MSG_BOARD_SNAPSHOT:
		{
			// 观战快照（分片到达）。这一族全部**宽松处置**：坏帧丢帧、半份作废、等下一份自愈。
			// 理由是它和其他帧不同——快照是显示层的附加流，长度又天然变长（最后一片不满），
			// "长度对不上"在这里分不出串包还是正常短尾；为一条画不出来的帧把整局判死不值。
			NetProto::MsgBoardSnapshot aMsg;
			if (!NetProto::DecodeBoardSnapshot(aPayload, aPayloadSize, aMsg))
			{
				TodLog("[net] threw away a malformed BOARD_SNAPSHOT");
				break;
			}
			if (!IsWatching() || aMsg.mSrcSeat != mWatchTargetSeat)
			{
				// 不是正在看的人发的（刚停了/换人了）：丢
				TodLog("[net] threw away a BOARD_SNAPSHOT from seat %u (not watching)", (unsigned)aMsg.mSrcSeat);
				break;
			}
			if (aMsg.mCount < 1 || aMsg.mCount > (uint8_t)NetProto::SNAPSHOT_MAX_PARTS || aMsg.mPart >= aMsg.mCount)
			{
				TodLog("[net] threw away a BOARD_SNAPSHOT with bad part info (%u/%u)",
					(unsigned)aMsg.mPart, (unsigned)aMsg.mCount);
				break;
			}

			if (aMsg.mSeq != mWatchAssemblySeq)
			{
				// 换代了：只认新代的第一片，中间的片在这半份上作废——绝不把两代字节拼一起
				if (aMsg.mPart != 0)
				{
					TodLog("[net] a BOARD_SNAPSHOT part %u of a new generation arrived mid-assembly - dropped",
						(unsigned)aMsg.mPart);
					break;
				}
				mWatchAssemblySeq = aMsg.mSeq;
				mWatchAssemblyParts = 0;
				mWatchAssemblySize = 0;
			}
			if (aMsg.mPart != (uint8_t)mWatchAssemblyParts)
			{
				// 片序断了（TCP 有序，只能是前面有片被丢过）：这半份作废，等下一份
				TodLog("[net] a BOARD_SNAPSHOT part gap (%u vs expected %d) - dropped the half",
					(unsigned)aMsg.mPart, mWatchAssemblyParts);
				break;
			}

			int aDataSize = aPayloadSize - NetProto::SNAPSHOT_PART_HEADER;
			memcpy(mWatchAssemblyBuf + mWatchAssemblySize, aMsg.mData, aDataSize);
			mWatchAssemblySize += aDataSize;
			mWatchAssemblyParts++;

			if (mWatchAssemblyParts == aMsg.mCount)
			{
				// 拼齐：解析成展示模型。解析不过就保留上一份画面、这份作废（等下一份自愈）。
				ViewSnapshot aSnapshot;
				memset(&aSnapshot, 0, sizeof(aSnapshot));
				if (ParseViewSnapshot(mWatchAssemblyBuf, mWatchAssemblySize, aSnapshot))
				{
					mLastSnapshot = aSnapshot;
					mHasViewSnapshot = true;
					mFramesSinceWatchSnapshot = 0;
				}
				else
				{
					TodLog("[net] a BOARD_SNAPSHOT did not parse (%d bytes) - kept the previous frame",
						mWatchAssemblySize);
				}
				mWatchAssemblyParts = 0;
				mWatchAssemblySize = 0;
			}
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
// ★ 服务器控制帧（中继）
// ====================================================================================================

// 服务器说的话整段收在这儿：WELCOME 点名册、PEER_JOIN/LEAVE 维护名册、ROOM_CLOSED 收摊、
// SEAT_SWAP 落实换位、PING 保活。这里只动会话层的状态，**不碰游戏**——名册变了要让游戏知道，
// 走的是事件队列（PEER_JOINED/PEER_LEFT），由主循环取走，跟别的包一个规矩。
void NetSession::HandleControlFrame(uint16_t theType, const uint8_t* thePayload, int theSize)
{
	switch (theType)
	{
	case NetProto::MSG_SRV_WELCOME:
		{
			if (mTransport != Transport::RELAY) return;		// 直连不该收到这些
			// 一份名册只认第一次（重复的丢掉）；@pvz-online MOD_BUILD 39：重连归队等来的
			// 那份也受理——整段重连期都停在 RECONNECTING，直到这里才收工。
			if (mState != State::HANDSHAKING && mState != State::RECONNECTING) return;
			bool aWasReconnect = (mState == State::RECONNECTING);

			NetProto::MsgSrvWelcome aMsg = {};
			if (!NetProto::DecodeSrvWelcome(thePayload, theSize, aMsg))
			{
				SetDead(NetText("服务器发来的名册有问题。", "The server sent a bad roster.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}
			if (aMsg.mVersion != NetProto::PROTOCOL_VERSION)
			{
				SetDead(NetText("协议版本不一致——所有人必须跑同一版本。", "Version mismatch - all players must run the same build.").c_str(), NetText("协议版本不一致", "Version mismatch").c_str());
				return;
			}
			// @pvz-online MOD_BUILD 39：归队的人拿到的席位必须还是原来那个——对不上说明
			// 服务器给错了位（或房间码撞上别人的房），接着打下去就是两个人坐同一个位子。
			if (aWasReconnect && aMsg.mYourSeat != mLocalSeat)
			{
				SetDead(NetText("重连失败：席位对不上。", "Reconnect failed: the seat does not match.").c_str(),
					NetText("重连失败", "Reconnect failed").c_str());
				return;
			}

			// 名册是**外面的数据**：席位号要拿去索引席位表，一个越界就是写穿。
			// 席位号必须都合法、互不重复，而且里头得有我自己——少了我，我在这房里就没位置。
			if (aMsg.mYourSeat < 1 || aMsg.mYourSeat > NetProto::MAX_PLAYERS ||
				aMsg.mHostSeat < 1 || aMsg.mHostSeat > NetProto::MAX_PLAYERS ||
				aMsg.mSeatCount < 1 || aMsg.mSeatCount > NetProto::MAX_PLAYERS)
			{
				SetDead(NetText("服务器发来的名册有问题。", "The server sent a bad roster.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}
			bool aSawSelf = false;
			bool aSawSeat[NetProto::MAX_PLAYERS + 1] = {};
			for (int i = 0; i < aMsg.mSeatCount; i++)
			{
				uint8_t aSeat = aMsg.mSeats[i].mSeat;
				if (aSeat < 1 || aSeat > NetProto::MAX_PLAYERS || aSawSeat[aSeat])
				{
					SetDead(NetText("服务器发来的名册有问题。", "The server sent a bad roster.").c_str(), NetText("服务器错误", "Server error").c_str());
					return;
				}
				aSawSeat[aSeat] = true;
				if (aSeat == aMsg.mYourSeat) aSawSelf = true;
			}
			if (!aSawSelf)
			{
				SetDead(NetText("服务器发来的名册有问题。", "The server sent a bad roster.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}

			// @pvz-online MOD_BUILD 39：归队前记下"还在等谁回开局 ACK"。ClearSeatTable 会
			// 把这份记账一起清掉，而回头 UpdateOnlineStart 的"全答齐了"判定看到"全空"会
			// 当场假通过开跑。重建名册后按席位补回，并给还没答的人重发一条开局命令——
			// 他多半根本没收到（帧丢在我掉线的那段），或答了但帧丢在半路。
			bool aWasAwaitingAck[NetProto::MAX_PLAYERS + 1] = {};
			for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
				aWasAwaitingAck[aSeat] = mSeats[aSeat].mAwaitingAck;

			// @pvz-online MOD_BUILD 39：本机"选好了"也一样要留住。它是个状态不是事件——
			// 报过一次（Let's Rock / 这一关不用选卡）就不会再报，去重记忆随名册重建归零后
			// 没有任何一帧会再触发它，队友的等待门（IsPeerSeedsReady）就干等一个永远不来的
			// "选好了"。这里连同记忆一起恢复，重建完名册立刻补报一遍。
			bool aWasSeedsReady = aWasReconnect && mSeats[mLocalSeat].mSeedsReady;

			ClearSeatTable();
			for (int i = 0; i < aMsg.mSeatCount; i++)
			{
				uint8_t aSeat = aMsg.mSeats[i].mSeat;
				mSeats[aSeat].mOccupied = true;
				mSeats[aSeat].mBuild = aMsg.mSeats[i].mBuild;
				mSeats[aSeat].mName = SanitizeName(aMsg.mSeats[i].mName, NetProto::NAME_SIZE);
			}
			// 自己那一格：名字只有本机说了算，名册里的副本不采信（跟直连握手同一条规矩）。
			// 构建代次用本机的——它就该是自己的值。
			mSeats[aMsg.mYourSeat].mName = mLocalName;
			mSeats[aMsg.mYourSeat].mBuild = NetProto::MOD_BUILD;

			mLocalSeat = aMsg.mYourSeat;
			mHostSeat = aMsg.mHostSeat;
			mRoomCode = aMsg.mRoomCode;
			// 谁是房主由服务器点（不一定是 1 号位）：房主管开局和选关，这条不能猜。
			mRole = (mLocalSeat == mHostSeat) ? Role::HOST : Role::CLIENT;

			// 归队的人：把"还在等谁开场"补回去（见上面的快照）。只补问得着的席位——
			// 掉线期间真走了的人，这份等待就该作废。
			if (aWasReconnect)
			{
				for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
				{
					if (aSeat != mLocalSeat && aWasAwaitingAck[aSeat]) ResendLastStartLevel(aSeat);
				}
				// 选卡状态同理（见上面的快照）：恢复本机那份记忆，再朝所有队友补报一遍。
				// 没报过就不补——乱补会让队友提前开打。
				if (aWasSeedsReady)
				{
					mSeats[mLocalSeat].mSeedsReady = true;
					NetProto::MsgSeedsReady aReady = {};
					aReady.mSrcSeat = mLocalSeat;
					aReady.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
					aReady.mReady = 1;
					uint8_t aPayload[NetProto::MAX_PAYLOAD];
					int aSize = NetProto::EncodeSeedsReady(aPayload, (int)sizeof(aPayload), aReady);
					if (aSize > 0) Dispatch(NetProto::MSG_SEEDS_READY, aPayload, aSize);
					TodLog("[net] told the team again that my plants were already picked (reconnect)");
				}
			}
			SetConnected(aWasReconnect);
		}
		return;

	case NetProto::MSG_SRV_REJECT:
		{
			if (mTransport != Transport::RELAY) return;

			NetProto::MsgSrvReject aMsg = {};
			if (!NetProto::DecodeSrvReject(thePayload, theSize, aMsg))
			{
				SetDead(NetText("服务器拒绝了该请求。", "The server did not like that request.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}

			// @pvz-online MOD_BUILD 39：重连期被回绝——这不是普通"加入失败"：人是从局里
			// 掉出来的，文案得说清"回不去了"，然后走判死老路（上层收摊回主菜单）。
			if (mState == State::RECONNECTING)
			{
				switch (aMsg.mReason)
				{
				case NetProto::REJECT_REJOIN_SEAT_MISMATCH:
					SetDead(NetText("重连失败：身份对不上，服务器不认这条归队请求。",
						"Reconnect failed: the server did not recognize you.").c_str(),
						NetText("重连失败", "Reconnect failed").c_str());
					break;
				case NetProto::REJECT_REJOIN_UNAVAILABLE:
					SetDead(NetText("重连失败：房间或你的席位已经没了。",
						"Reconnect failed: the room or your seat is gone.").c_str(),
						NetText("重连失败", "Reconnect failed").c_str());
					break;
				default:
					SetDead(NetText("重连失败。", "Reconnect failed.").c_str(),
						NetText("重连失败", "Reconnect failed").c_str());
					break;
				}
				TodLog("[net] the server turned the rejoin down (reason %u)", (unsigned)aMsg.mReason);
				return;
			}

			// 每种回绝都得给玩家指一条能走的路："不知道"和"没这个房"要做的事不一样。
			switch (aMsg.mReason)
			{
			case NetProto::REJECT_PROTOCOL_VERSION:
				SetDead(NetText("协议版本不一致——所有人必须跑同一版本。", "Version mismatch - all players must run the same build.").c_str(), NetText("协议版本不一致", "Version mismatch").c_str());
				break;
			case NetProto::REJECT_ROOM_NOT_FOUND:
				SetDead(NetText("没有这个房间——请核对房间码。", "No such room - check the room code.").c_str(), NetText("房间不存在", "Room not found").c_str());
				break;
			case NetProto::REJECT_ROOM_FULL:
				SetDead(NetText("房间满了。", "That room is full.").c_str(), NetText("房间已满", "Room full").c_str());
				break;
			case NetProto::REJECT_BAD_CODE:
				SetDead(NetText("房间码不对——应是 4 位字母或数字。", "Bad room code - it is 4 letters and digits.").c_str(), NetText("房间码不对", "Bad room code").c_str());
				break;
			case NetProto::REJECT_SERVER_BUSY:
				SetDead(NetText("服务器忙——请稍后再试。", "The server is busy - try again later.").c_str(), NetText("服务器繁忙", "Server busy").c_str());
				break;
			default:
				SetDead(NetText("服务器拒绝了该请求。", "The server did not like that request.").c_str(), NetText("服务器拒绝", "Server said no").c_str());
				break;
			}
			TodLog("[net] the server turned us down (reason %u)", (unsigned)aMsg.mReason);
		}
		return;

	case NetProto::MSG_SRV_PEER_JOIN:
		{
			if (mTransport != Transport::RELAY) return;
			if (!IsConnected()) return;

			NetProto::MsgSrvPeerJoin aMsg = {};
			if (!NetProto::DecodeSrvPeerJoin(thePayload, theSize, aMsg))
			{
				SetDead(NetText("服务器发来的数据包有问题。", "The server sent a malformed packet.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}
			if (aMsg.mSeat < 1 || aMsg.mSeat > NetProto::MAX_PLAYERS || aMsg.mSeat == mLocalSeat)
			{
				TodLog("[net] threw away a PEER_JOIN with a bad seat (%u)", (unsigned)aMsg.mSeat);
				return;
			}

			mSeats[aMsg.mSeat].mOccupied = true;
			mSeats[aMsg.mSeat].mBuild = aMsg.mBuild;
			mSeats[aMsg.mSeat].mName = SanitizeName(aMsg.mName, NetProto::NAME_SIZE);
			mSeats[aMsg.mSeat].mOffline = false;	// @pvz-online MOD_BUILD 39：新来的人当然在线
			TodLog("[net] seat %u joined: %s (build %u)", (unsigned)aMsg.mSeat,
				mSeats[aMsg.mSeat].mName.c_str(), (unsigned)aMsg.mBuild);

			// 补发一条我现在的判胜状态（坑 2）：LEVEL_DONE 是"值没变不发"的状态包，
			// 在我这边它早就报过了，新人那边却什么都没收到——不补，他的棋盘会一直等
			// 一个"永远等不来"的队友清完，判胜就永远凑不齐。
			if (mSeats[mLocalSeat].mLevelDone) SendLevelDoneTo(aMsg.mSeat, true);

			PushEvent(EventType::PEER_JOINED, aMsg.mSeat);
		}
		return;

	case NetProto::MSG_SRV_PEER_LEAVE:
		{
			if (mTransport != Transport::RELAY) return;
			if (!IsConnected()) return;

			NetProto::MsgSrvPeerLeave aMsg = {};
			if (!NetProto::DecodeSrvPeerLeave(thePayload, theSize, aMsg))
			{
				SetDead(NetText("服务器发来的数据包有问题。", "The server sent a malformed packet.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}
			if (aMsg.mSeat < 1 || aMsg.mSeat > NetProto::MAX_PLAYERS || aMsg.mSeat == mLocalSeat)
			{
				TodLog("[net] threw away a PEER_LEAVE with a bad seat (%u)", (unsigned)aMsg.mSeat);
				return;
			}

			// 整格清掉：人走了，他报过的判胜/选卡/开局应答全都不算数了。
			// 顺带解开"在等他回话"的等待——不然主机永远收不齐开着局（坑 4）。
			mSeats[aMsg.mSeat].mOccupied = false;
			mSeats[aMsg.mSeat].mBuild = 0;
			mSeats[aMsg.mSeat].mName.clear();
			mSeats[aMsg.mSeat].mLevelDone = false;
			mSeats[aMsg.mSeat].mSeedsReady = false;
			mSeats[aMsg.mSeat].mAckState = 0;
			mSeats[aMsg.mSeat].mAwaitingAck = false;
			mSeats[aMsg.mSeat].mOffline = false;	// 保留期到点了，掉线的人算是走了
			// 挂在他身上的换位请求也跟着作废
			if (mSwapRequestSeat == aMsg.mSeat || mSwapAskSeat == aMsg.mSeat) ClearSwapState();
			TodLog("[net] seat %u left (reason %u) - %d player(s) left", (unsigned)aMsg.mSeat,
				(unsigned)aMsg.mReason, GetOccupiedSeatCount());

			PushEvent(EventType::PEER_LEFT, aMsg.mSeat);
		}
		return;

	// @pvz-online MOD_BUILD 39：有人掉线了、席位进保留期。席位照旧算"有人的"（各等待门
	// 全都不动，全队等他），只是名册上标一句"掉线中"，UI 才看得出和"稳稳坐着"的区别。
	// 保留期拖到过期，服务器会补一条 PEER_LEAVE 走上面那条清格路——等待跟着解锁。
	case NetProto::MSG_SRV_PEER_OFFLINE:
		{
			if (mTransport != Transport::RELAY) return;
			if (!IsConnected()) return;

			NetProto::MsgSrvPeerOffline aMsg = {};
			if (!NetProto::DecodeSrvPeerOffline(thePayload, theSize, aMsg))
			{
				SetDead(NetText("服务器发来的数据包有问题。", "The server sent a malformed packet.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}
			if (aMsg.mSeat < 1 || aMsg.mSeat > NetProto::MAX_PLAYERS || aMsg.mSeat == mLocalSeat)
			{
				TodLog("[net] threw away a PEER_OFFLINE with a bad seat (%u)", (unsigned)aMsg.mSeat);
				return;
			}

			mSeats[aMsg.mSeat].mOffline = true;
			TodLog("[net] seat %u dropped out - the server holds its seat for %u more second(s)",
				(unsigned)aMsg.mSeat, (unsigned)aMsg.mGraceSec);
			PushEvent(EventType::PEER_OFFLINE, aMsg.mSeat);
		}
		return;

	// @pvz-online MOD_BUILD 39：掉线的人回来了（重新接上，名册照旧）。
	case NetProto::MSG_SRV_PEER_BACK:
		{
			if (mTransport != Transport::RELAY) return;
			if (!IsConnected()) return;

			NetProto::MsgSrvPeerBack aMsg = {};
			if (!NetProto::DecodeSrvPeerBack(thePayload, theSize, aMsg))
			{
				SetDead(NetText("服务器发来的数据包有问题。", "The server sent a malformed packet.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}
			if (aMsg.mSeat < 1 || aMsg.mSeat > NetProto::MAX_PLAYERS || aMsg.mSeat == mLocalSeat)
			{
				TodLog("[net] threw away a PEER_BACK with a bad seat (%u)", (unsigned)aMsg.mSeat);
				return;
			}

			// 名册残余里这一格什么样都照常补全（万一 PEER_JOIN 之前丢了，这里顺手救回来）
			bool aWasOffline = mSeats[aMsg.mSeat].mOffline;
			mSeats[aMsg.mSeat].mOccupied = true;
			mSeats[aMsg.mSeat].mBuild = aMsg.mBuild;
			mSeats[aMsg.mSeat].mName = SanitizeName(aMsg.mName, NetProto::NAME_SIZE);
			mSeats[aMsg.mSeat].mOffline = false;
			TodLog("[net] seat %u is back: %s (build %u)", (unsigned)aMsg.mSeat,
				mSeats[aMsg.mSeat].mName.c_str(), (unsigned)aMsg.mBuild);

			// 他归队时名册是从服务器的 WELCOME 重建的——"我的状态"在他那边全回到初始值，
			// 而我这边的去重记忆（值没变不发）会把下一次重报吞掉。两条状态点名补给他
			// （和 PEER_JOIN 补判胜同一个理，选卡那条 PEER_JOIN 没补，归队场景靠得住它）。
			if (mSeats[mLocalSeat].mLevelDone) SendLevelDoneTo(aMsg.mSeat, true);
			if (mSeats[mLocalSeat].mSeedsReady)
			{
				NetProto::MsgSeedsReady aReady = {};
				aReady.mSrcSeat = mLocalSeat;
				aReady.mDstSeat = aMsg.mSeat;
				aReady.mReady = 1;
				uint8_t aPayload[NetProto::MAX_PAYLOAD];
				int aSize = NetProto::EncodeSeedsReady(aPayload, (int)sizeof(aPayload), aReady);
				if (aSize > 0) Dispatch(NetProto::MSG_SEEDS_READY, aPayload, aSize, aMsg.mSeat);
			}

			// 主机还在等他回开局 ACK：重发一条开局命令——他多半根本没收到，或答了帧丢在半路。
			if (mSeats[aMsg.mSeat].mAwaitingAck) ResendLastStartLevel(aMsg.mSeat);

			// 只有"大家看见他掉线过"才值得报一句；快路径的静默替换对别人本来就不存在，
			// 这个事件就是给 UI 用的（LawnApp 侧提示"X 已重新连接"）。
			if (aWasOffline) PushEvent(EventType::PEER_BACK, aMsg.mSeat);
		}
		return;

	case NetProto::MSG_SRV_ROOM_CLOSED:
		{
			if (mTransport != Transport::RELAY) return;

			NetProto::MsgSrvRoomClosed aMsg = {};
			if (!NetProto::DecodeSrvRoomClosed(thePayload, theSize, aMsg))
			{
				SetDead(NetText("房间已关闭。", "The room was closed.").c_str(), NetText("房间已关闭", "Room closed").c_str());
				return;
			}

			if (aMsg.mReason == NetProto::ROOM_CLOSED_HOST_LEFT)
				SetDead(NetText("房主离开了——房间已关闭。", "The host left - the room is closed.").c_str(), NetText("房主离开", "Host left").c_str());
			else
				SetDead(NetText("服务器关闭了房间。", "The server closed the room.").c_str(), NetText("房间已关闭", "Room closed").c_str());
		}
		return;

	case NetProto::MSG_SRV_SEAT_SWAP:
		{
			if (mTransport != Transport::RELAY) return;
			if (!IsConnected()) return;

			NetProto::MsgSeatSwap aMsg = {};
			if (!NetProto::DecodeSeatSwap(thePayload, theSize, aMsg))
			{
				SetDead(NetText("服务器发来的数据包有问题。", "The server sent a malformed packet.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}
			if (aMsg.mSeatA < 1 || aMsg.mSeatA > NetProto::MAX_PLAYERS ||
				aMsg.mSeatB < 1 || aMsg.mSeatB > NetProto::MAX_PLAYERS || aMsg.mSeatA == aMsg.mSeatB)
			{
				TodLog("[net] threw away a SEAT_SWAP with bad seats (%u/%u)",
					(unsigned)aMsg.mSeatA, (unsigned)aMsg.mSeatB);
				return;
			}

			// 全房广播 = 唯一权威：照它换，不照"谁问过谁"换。两边同时按的那种也只换这一次。
			bool aMine = (mSwapRequestSeat == aMsg.mSeatA || mSwapRequestSeat == aMsg.mSeatB ||
				mSwapAskSeat == aMsg.mSeatA || mSwapAskSeat == aMsg.mSeatB);
			ApplySeatSwap(aMsg.mSeatA, aMsg.mSeatB);
			if (aMine) ClearSwapState();		// 我这边等的那件事落地了，面板可以解锁
		}
		return;

	case NetProto::MSG_SRV_PING:
		{
			if (mTransport != Transport::RELAY) return;

			// 空载荷。收到什么别的话说明对面不是我们的服务器，别答理。
			if (theSize != 0)
			{
				SetDead(NetText("服务器发来的数据包有问题。", "The server sent a malformed packet.").c_str(), NetText("服务器错误", "Server error").c_str());
				return;
			}
			// 回一句就够：服务器靠它知道我还在，本地靠它刷新"对面还活着"的计时
			// （Update 里任何入站帧都会重置，独处的房主全靠这一条）。
			SendRaw(NetProto::MSG_CLI_PONG, nullptr, 0);
		}
		return;

	default:
		// 没见过的控制帧（包括客户端→服务器的那半边）：丢掉。跨版本不靠这儿挡——
		// 协议版本对不上服务器自己会 REJECT。
		return;
	}
}

// ====================================================================================================
// ★ 发包
// ====================================================================================================



// 把一帧交给该收的人。直连只有一个对端——行为与 M2 逐字节一致：目标就是那一个，
// 载荷里的 dst 也还是写它。（多席位的扇出/点名单发在中继那边，见 NetSession.h。）
bool NetSession::Dispatch(uint16_t theType, uint8_t* thePayload, int theSize, uint8_t theTarget)
{
	// 直连只有一个对端——行为与 M2 逐字节一致：目标就是那一个，载荷里的 dst 也还是写它。
	if (mTransport == Transport::DIRECT)
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

	// 中继：一条帧只去一个席位（服务器只管照 dst 转发，没有"广播"这回事），所以
	// "发给所有队友"在这儿就是**每个上座席位各发一条**——收件人拿到的那条和点名单发
	// 一模一样，谁也看不出自己是不是被扇出来的。theTarget 给了就只发那一个。
	// 载荷里带席位号的那几种（前两字节 src/dst）**每条都要重写 dst**：一份载荷循环发多次，
	// 上一轮的收件人要是留在了字节里，下一个人就会把自己的帧送给前一个人。
	bool aAnySent = false;
	for (uint8_t aSeat = 1; aSeat <= NetProto::MAX_PLAYERS; aSeat++)
	{
		if (aSeat == mLocalSeat || !IsSeatOccupied(aSeat)) continue;
		if (theTarget != NetProto::SEAT_UNSET && theTarget != aSeat) continue;

		if (theSize >= 2 && thePayload)
		{
			thePayload[0] = mLocalSeat;
			thePayload[1] = aSeat;
		}
		if (SendRaw(theType, thePayload, theSize)) aAnySent = true;
	}
	return aAnySent;
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

// 中继：把建房/加入的请求递上去。链路一通就发这一次（Update 里），之后全听服务器的。
// 席位、房主、名册都从这里之后才开始有。
bool NetSession::SendRoomRequest()
{
	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = 0;
	uint16_t aType = NetProto::MSG_CLI_CREATE_ROOM;

	if (mRole == Role::HOST)
	{
		NetProto::MsgCliCreateRoom aMsg = {};
		aMsg.mVersion = NetProto::PROTOCOL_VERSION;
		aMsg.mBuild = NetProto::MOD_BUILD;
		NetProto::SetName(aMsg.mName, mLocalName.c_str());
		aSize = NetProto::EncodeCliCreateRoom(aPayload, (int)sizeof(aPayload), aMsg);
		TodLog("[net] asking the server to open a room (%s)", mConnectHost.c_str());
	}
	else
	{
		NetProto::MsgCliJoinRoom aMsg = {};
		aMsg.mVersion = NetProto::PROTOCOL_VERSION;
		aMsg.mBuild = NetProto::MOD_BUILD;
		NetProto::SetName(aMsg.mName, mLocalName.c_str());
		memcpy(aMsg.mRoomCode, mRoomCode.data(), NetProto::ROOM_CODE_LEN);
		aSize = NetProto::EncodeCliJoinRoom(aPayload, (int)sizeof(aPayload), aMsg);
		aType = NetProto::MSG_CLI_JOIN_ROOM;
		TodLog("[net] asking the server for room %s", mRoomCode.c_str());
	}

	if (aSize <= 0) return false;
	return SendRaw(aType, aPayload, aSize);
}

// 中继：换位谈妥了，请服务器落实并向全房广播。自己**先不换**——服务器是唯一权威，
// 广播到了才换（两边各换一次就成了换回来）。
bool NetSession::SendSeatSwapCommit(uint8_t theSeatA, uint8_t theSeatB)
{
	NetProto::MsgSeatSwap aMsg;
	aMsg.mSeatA = theSeatA;
	aMsg.mSeatB = theSeatB;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeSeatSwap(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	TodLog("[net] asking the server to swap seats %u and %u", (unsigned)theSeatA, (unsigned)theSeatB);
	return SendRaw(NetProto::MSG_CLI_SWAP_COMMIT, aPayload, aSize);
}

void NetSession::SendHello()
{
	if (mTransport == Transport::RELAY) return;		// 中继的身份由服务器点，不发 HELLO

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
	bool theIsRun, int32_t theRunSeed, uint16_t theRunLevelIndex, uint8_t theTargetSeat, uint8_t theRunMode,
	uint8_t theRunDiff, uint8_t theRunScale, uint8_t theRunTempo, uint8_t theRunZombotany,
	uint8_t theRunEndlessScene, uint8_t theRunBossFlag)
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
	aMsg.mRunMode = theRunMode;
	aMsg.mRunDiff = theRunDiff;
	aMsg.mRunScale = theRunScale;
	aMsg.mRunTempo = theRunTempo;
	aMsg.mRunZombotany = theRunZombotany;
	aMsg.mRunEndlessScene = theRunEndlessScene;
	aMsg.mRunBossFlag = theRunBossFlag;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeStartLevel(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	if (theIsRun)
	{
		TodLog("[net] telling the client to enter run level %u (run seed %d, level index %u, mode %u, diff %u, scale %u, tempo %u, zombotany %u, endless scene %u)",
			(unsigned)theLevel, (int)theRunSeed, (unsigned)theRunLevelIndex, (unsigned)theRunMode, (unsigned)theRunDiff,
			(unsigned)theRunScale, (unsigned)theRunTempo, (unsigned)theRunZombotany, (unsigned)theRunEndlessScene);
	}
	else
	{
		TodLog("[net] telling the client to start: mode %u level %u seed %d",
			(unsigned)theGameMode, (unsigned)theLevel, (int)theLevelSeed);
	}
	ClearPauseState();		// 新的一局：暂停态从头开始记
	ClearLevelDoneState();	// "谁清完了"同理

	if (!Dispatch(NetProto::MSG_START_LEVEL, aPayload, aSize, theTargetSeat)) return false;

	// @pvz-online MOD_BUILD 39：这份命令留个底——重连归队/队友回来时要照原样补发一条
	//（见 ResendLastStartLevel），那一头不能带"新一局"的副作用。
	mLastStartLevel = aMsg;
	mLastStartLevelValid = true;

	// 新的一条命令：上一轮"有人说不行"的记账跟着作废，别让它把这一次也毙了。
	mAnyAckRejected = false;

	// 发出去了才记"在等谁回答"：收齐应答的判定（P3 的放行）得按席位看，
	// 单槽记账看不出六个人里是谁还没答。
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
		// 中继下这条只能落到主机那一席（多席位时 Dispatch 默认是扇出，会把"我进场了"
		// 播给不相干的人）；直连的对端本来就是主机，目标保持原样的默认值。
		Dispatch(NetProto::MSG_START_ACK, aPayload, aSize,
			(mTransport == Transport::RELAY) ? mHostSeat : NetProto::SEAT_UNSET);
	}
}

bool NetSession::SendRunGo(uint8_t theTargetSeat)
{
	if (mRole != Role::HOST || !IsConnected()) return false;

	NetProto::MsgRunGo aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeRunGo(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize <= 0) return false;

	TodLog("[net] telling the team to start their picks");
	return Dispatch(NetProto::MSG_RUN_GO, aPayload, aSize, theTargetSeat);
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
	if (mTransport == Transport::RELAY) return;		// 中继的保活是服务器 PING、我们回 PONG

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
	if (mTransport == Transport::RELAY) return;		// 中继退房走 LEAVE_ROOM，见 Close()

	NetProto::MsgBye aMsg;
	aMsg.mSrcSeat = mLocalSeat;
	aMsg.mDstSeat = NetProto::SEAT_UNSET;		// 由 Dispatch 按目标填
	aMsg.mReason = theReason;

	uint8_t aPayload[NetProto::MAX_PAYLOAD];
	int aSize = NetProto::EncodeBye(aPayload, (int)sizeof(aPayload), aMsg);
	if (aSize > 0) Dispatch(NetProto::MSG_BYE, aPayload, aSize);
}
