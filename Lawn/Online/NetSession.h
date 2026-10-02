#ifndef __NETSESSION_H__
#define __NETSESSION_H__

#include <cstdint>
#include <string>
#include <vector>
#include "NetLink.h"

// @pvz-online: M2 会话层——"谁建房、谁坐哪个席位、握手、心跳、掉线"这套规矩收在这里，
// 游戏代码只问状态、只收事件，不碰 socket。
//
// 只有主线程用这个类（Update / Start* / Send* / PollEvent 全在主循环里调）；
// 收包线程只往 NetLink 的队列里塞字节。
//
// 席位：主机 = 1，客户端 = 2（M3 再扩到四个）。

class NetSession
{
public:
	enum class Role
	{
		NONE,
		HOST,
		CLIENT
	};

	enum class State
	{
		OFF,
		LISTENING,		// 主机：端口开着，等队友连进来
		CONNECTING,		// 客户端：正在连
		HANDSHAKING,	// TCP 通了，等握手结果
		CONNECTED,
		DEAD			// 断了或失败了，原因在 GetStatusText() 里
	};

	enum class EventType
	{
		NONE,
		CONNECTED,		// 握手完成，可以开局了
		DISCONNECTED	// 掉线（不是我们自己关的）
	};

	struct Event
	{
		EventType	mType;
	};

	// 帧计数按主循环固定 10ms 一拍折算：100 帧 ≈ 1 秒
	static const int	HEARTBEAT_FRAMES			= 100;
	static const int	TIMEOUT_FRAMES				= 500;
	static const int	NOTICE_FRAMES				= 400;	// 即时说明挂多久（≈4 秒）

public:
	NetSession();
	~NetSession();

	bool			StartHost(uint16_t thePort = NetProto::DEFAULT_PORT);
	bool			StartJoin(const char* theHost, uint16_t thePort = NetProto::DEFAULT_PORT);

	// 本机玩家名：握手时报给对面，名册 UI 靠它显示"谁坐在几号位"。
	// 名字是"这台机器是谁"，跟某一局无关——StartHost 第一件事就是 ResetToOff，
	// 所以它不能放在那里面清（清了就变成"每次开局都得重设"，忘一次名册就空了）。
	void			SetLocalName(const char* theName);

	// 名册：某个席位上的人叫什么。空席位返回空串。
	// 席位号从 1 数到 NetProto::MAX_PLAYERS，UI 从上往下照着画就是顺位顺序。
	std::string		GetSeatName(uint8_t theSeat) const;
	// 这个席位上现在有没有人。自己那席只要有身份就算有人（建房/加入那一刻起），
	// 对面那席要真连上才算——还在等人进来的时候那个位子是空的。
	bool			IsSeatOccupied(uint8_t theSeat) const;

	// 主动收摊（先给对方发 BYE）。之后可以重新 StartHost / StartJoin。
	void			Close();

	void			Update();			// 主线程每帧调一次

	// 主机开局：把这一局的模式、关卡、波表种子告诉队友。没连上时返回 false。
	// 联机闯关（R5）再多带三个数：是不是闯关局、局种子、关序号——队友据此对齐进度
	// （见 LawnApp::AlignRunToHost）；单关局用默认值，线格式上这三格是 0。
	bool			SendStartLevel(uint8_t theGameMode, uint32_t theLevel, int32_t theLevelSeed,
						bool theIsRun = false, int32_t theRunSeed = 0, uint8_t theRunLevelIndex = 0);

	// 客户端侧：取出主机发来的开局命令（同时清掉）。没有就返回 false。
	// 单槽而不是队列：开局命令只有"最新那条"有意义，堆着旧的开局命令没有用处。
	bool			TakePendingStartLevel(NetProto::MsgStartLevel& theMsg);

	// 客户端侧：告诉主机"开局命令收到、我进场了"（theAccepted=false 是"现在不行"，
	// 比如人还在关卡里）。只在真的准备进场时发 true——不在主菜单（命令接不了）就别发 true，
	// 否则主机会一个人开着关跑下去。
	void			SendStartAck(bool theAccepted = true);

	// 主机侧：队友对开局命令的回应到了没有（取一次就清）。theAccepted 是"进场 / 现在不行"。
	bool			TakeStartAck(bool& theAccepted);

	// 我离开这一局了（回主菜单）。对面收到会跟着退——不然一边在关卡里、一边在菜单上，
	// 退的那边再点关卡开局时还会撞上"对面不在菜单、开局命令被丢掉"的卡死。
	bool			SendLevelExit(uint8_t theReason = NetProto::EXIT_QUIT_TO_MENU);

	// 对面退出了这一局没有（取一次就清）。
	bool			TakePendingLevelExit(NetProto::MsgLevelExit& theMsg);

	// 我这块草坪清干净了没有（theDone=1 清完了 / 0 又来了怪）。全队判胜靠这条：
	// **所有上座席位**都报着"清完了"，这一关才算过。棋盘每帧都会问一次，所以这里
	// 按"上次发出去的值"去重，值没变就不发。真的发出去了才返回 true。
	bool			SendLevelDone(bool theDone);

	// 我这边清完了没有 / 其他席位是不是都清完了（棋盘上那句"等队友"看这个）。
	bool			IsLocalLevelDone() const;
	bool			IsPeerLevelDone() const;

	// 全队都清完了——这一关对所有人结束了，该回主菜单。取一次就清：
	// 主循环每帧都会问，收摊只能收一次。
	bool			TakeAllLevelsDone();

	// 全队败（末尾席位漏怪）：输的是**全队**，不是"谁漏谁出局"。对面收到就一起收摊。
	// reason 见 NetProto::GameOverReason。
	bool			SendGameOver(uint8_t theReason);

	// 对面报的全队败（取一次就清）。
	bool			TakePendingGameOver(NetProto::MsgGameOver& theMsg);

	// 我暂停了 / 我继续了（共识模型：任一方都能按，任一方也都能继续）。
	// 与两边已经认可的状态相同时**不发**——这一条同时干掉了所有回声：收端为了队友
	// 把菜单弹出来时，本机的发送检测器也会看到"菜单开了"，但那时状态已经是对的，不会回发。
	// 发送成功（状态真的变了）返回 true。
	bool			SendPauseState(bool thePaused);

	// 对面那边暂停/继续了（取一次就清）。只对**收到**的改动返回 true，本机自己发的不会走这里。
	bool			TakePauseState(bool& thePaused);

	// 现在这个暂停是队友按的（菜单上据此注明"队友暂停了"）。
	// 双方同时按的时候谁也不抢这个署名——只有"收到 0→1"才置真。
	bool			IsPausedByPeer() const { return mSharedPaused && mPauseCameFromPeer; }

	// 给玩家看的即时说明（几秒后自己消失）。握手/换位之外的地方也要能写一句，
	// 所以把它开出来——UI 重建时说明不会跟着丢。
	void			PostNotice(const char* theText);

	// 这一局结束了（回主菜单）：丢掉只对"当前这一局"有意义的收包队列。
	// 不清的话，上一局收到的漏怪/开局命令会砸到下一局的棋盘上。
	void			DiscardLevelPackets();

	// 开始前换位置：跟"后一位"换（末位的后一位是首位），对面同意才真的换。
	// 这一步只是把请求发出去，本机不动；同意/拒绝由对面定，见下面几条。
	// 没连上、已经有一个请求在等回话、或对面正问着我，都返回 false（按不动）。
	bool			SwapSeats();

	// 我发出的请求还在等对面回话（面板据此灰掉 Swap 键，也挡住重复请求）。
	bool			IsSwapRequestPending() const { return mSwapRequestPending; }

	// 对面正问我换不换（主循环看到就把面板叫出来，等玩家按同意/拒绝）。
	// 一次请求一直挂着为真，直到 AnswerSwapRequest 作答。
	bool			HasIncomingSwapRequest() const { return mSwapAskPending; }

	// 回答对面的换位请求。同意则两边各自执行同一次换位（本机当场生效，
	// 对面收到 REPLY 后跟着换）；拒绝则什么都不发生，对面只收到一句说明。
	void			AnswerSwapRequest(bool theAccept);

	// 一小句给玩家看的即时说明（"对面拒绝了"、"换过去了，你现在是 P3"），
	// 过几秒自己消失。没有就返回空串。
	const std::string&	GetNoticeText() const { return mNoticeText; }

	// 我方棋盘上的漏怪该传给谁。M2 是两席位：1 → 2；2 号位是末席，
	// 没有下一席位（返回 SEAT_UNSET）——末席漏怪就是全队败，没得传。
	// 只看席位号、不看谁建的房：换过位置后方向跟着换。
	uint8_t			GetRelayTargetSeat() const;

	// 把一只漏怪交出去（席位由会话层填好）。末席、没连上、编码失败都返回 false——
	// 返回 false 就是"没传成"，调用方要按原版判负处理，绝不能悄悄让怪消失。
	bool			SendEscapedZombie(const NetProto::MsgEscapedZombie& theMsg);

	// 收下的漏怪（先进先出，一只都不许丢）。没有就返回 false。
	bool			TakePendingEscapedZombie(NetProto::MsgEscapedZombie& theMsg);

	State			GetState() const { return mState; }
	Role			GetRole() const { return mRole; }
	bool			IsConnected() const { return mState == State::CONNECTED; }
	bool			IsActive() const { return mState != State::OFF; }
	uint8_t			GetLocalSeat() const { return mLocalSeat; }
	uint8_t			GetPeerSeat() const { return mPeerSeat; }

	// 对面的构建代次（握手时报的，连上才有效）。
	uint16_t		GetPeerBuild() const { return mPeerBuild; }
	// 两边的构建代次不一样：包还是同一套，但行为可能不配套（比如漏怪在某代才真的
	// 落地）。**从 MOD_BUILD 12 起这不再拒绝握手**——照常连、UI 上挂一句提醒，
	// 玩家真撞上不对劲自己会去更新，比"整场连不上"实用（用户 2026-10-02 拍板）。
	bool			IsBuildDifferent() const
	{
		return mState == State::CONNECTED && mPeerBuild != 0 && mPeerBuild != NetProto::MOD_BUILD;
	}

	const std::string&	GetStatusText() const { return mStatusText; }
	const std::string&	GetHintText() const { return mHintText; }
	// 死因的一行短标签（主菜单小状态条那种一行宽的地方用）。没给短标签就是默认的
	// "Connection lost"——版本/构建不符这类要玩家动手的原因得自带短标签，
	// 否则在小条上看起来和"网线掉了"一模一样。
	const std::string&	GetShortStatus() const { return mShortStatus; }
	// 已经试到第几轮连接。连不上不再有上限，所以这是"还在试"和"卡死了"的唯一区别，
	// 状态行和主菜单小状态条都要用。
	int				GetConnectAttempts() const { return mLink.GetConnectAttempts(); }

	bool			PollEvent(Event& theEvent);

private:
	void			ResetToOff();
	void			SetConnected();
	void			SetDead(const char* theReason, const char* theShortReason = nullptr);
	void			UpdateStatusText();
	void			HandlePacket(const NetLink::Packet& thePacket);
	void			PushEvent(EventType theType);
	// 执行一次换位（本机 + 记在心里的对端席位）。请求被同意时两边各调一次。
	void			ApplySeatSwap();
	// 给对面回话。同意与否都由调用方定；这里只负责发。
	void			SendSwapReply(bool theAccepted);
	// 写一条几秒后自动消失的即时说明（"对面拒绝了"这类）。
	void			SetNotice(const char* theText, int theFrames = NOTICE_FRAMES);
	// 把"两边暂停到哪了"整个忘掉（新一局开始、掉线、收摊时用）。
	void			ClearPauseState();
	// 把"谁清完了"整个忘掉（新一局开始、掉线、收摊时用）。
	void			ClearLevelDoneState();
	// 上座席位是不是都报了"清完了"。一个队友都没有的空局返回 false——
	// 单机里没人陪你判胜。
	bool			AreAllSeatsDone() const;

	bool			SendRaw(uint16_t theType, const uint8_t* thePayload, int thePayloadSize);
	void			SendHello();
	void			SendHelloAck(bool theAccepted);
	void			SendHeartbeat();
	void			SendBye(uint8_t theReason);

private:
	NetLink				mLink;
	Role				mRole;
	State				mState;
	uint8_t				mLocalSeat;
	uint8_t				mPeerSeat;
	uint16_t			mPeerBuild;			// 对面报的构建代次（握手时记下，连上期间读）
	std::string			mLocalName;			// 不随 ResetToOff 清（见 SetLocalName）
	std::string			mPeerName;			// 对面报的名字，连接作废时跟着一起清
	int					mFramesSincePacket;
	int					mFramesSinceHeartbeat;
	uint32_t			mHeartbeatTick;
	std::string			mConnectHost;
	uint16_t			mConnectPort;
	std::string			mStatusText;
	std::string			mHintText;
	std::string			mShortStatus;
	std::vector<Event>	mEvents;
	bool				mHasPendingStart;
	NetProto::MsgStartLevel	mPendingStart;
	bool				mHasStartAck;
	bool				mStartAckAccepted;	// 上一条 START_ACK 是"进场"还是"现在不行"
	bool				mHasPendingLevelExit;
	NetProto::MsgLevelExit	mPendingLevelExit;
	// 各席位"草坪清干净了没有"，按下标=席位号（0 号位不用）。用数组而不是两个布尔：
	// M2 只有两个席位，M3 是四个，判胜的写法不该跟着席位数量重写一遍。
	// 自己那一格同时是"上次发出去的值"，发送去重就靠它。
	bool				mSeatDone[NetProto::MAX_PLAYERS + 1];
	bool				mAllDoneTaken;		// "全队都清完了"已经收过摊了
	bool				mHasPendingGameOver;
	NetProto::MsgGameOver	mPendingGameOver;
	// 两边共同认可的暂停态（同时也是发送去重的依据），以及"这次暂停是谁按的"。
	bool				mSharedPaused;
	bool				mPauseCameFromPeer;
	bool				mHasPendingPause;
	bool				mPendingPauseValue;
	std::vector<NetProto::MsgEscapedZombie>	mPendingEscapedZombies;
	bool				mSwapRequestPending;	// 我发出的换位请求在等回话
	bool				mSwapAskPending;		// 对面的换位请求在等我作答
	std::string			mNoticeText;			// 即时说明（几秒后自己消失）
	int					mNoticeFrames;

	NetSession(const NetSession&);
	NetSession& operator=(const NetSession&);
};

#endif
