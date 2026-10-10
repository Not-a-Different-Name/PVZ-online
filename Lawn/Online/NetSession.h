#ifndef __NETSESSION_H__
#define __NETSESSION_H__

#include <cstdint>
#include <string>
#include <vector>
#include "NetLink.h"

// @pvz-online: M2/M3 会话层——"谁建房、谁坐哪个席位、握手、心跳、掉线"这套规矩收在这里，
// 游戏代码只问状态、只收事件，不碰 socket。
//
// 只有主线程用这个类（Update / Start* / Send* / PollEvent 全在主循环里调）；
// 收包线程只往 NetLink 的队列里塞字节。
//
// 席位：直连是固定的两个（主机 = 1，客户端 = 2）；M3 中继由服务器点名册，最多六个。
// 所以"谁是队友"记在一张**席位表**里（mSeats），不再由一个对端字段代表——判胜、
// 漏怪接力、换位环、开局应答全按表走，写死"就是那两个人"的地方一处不剩。

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

	enum class Transport
	{
		DIRECT,			// 一人 Listen、一人 Connect 的直连（M2 起就有）
		RELAY			// 两边都连中继服务器，身份由服务器名册说了算（M3）
	};

	enum class EventType
	{
		NONE,
		CONNECTED,		// 握手完成，可以开局了
		DISCONNECTED,	// 掉线（不是我们自己关的）
		PEER_JOINED,	// 中继：有人进了房（mSeat 说清是谁）。直连不会有——那个人就是 CONNECTED
		PEER_LEFT		// 中继：有人走了（mSeat 说清是谁）。房主走 = ROOM_CLOSED，不是这一条
	};

	struct Event
	{
		EventType	mType;
		uint8_t		mSeat;		// 跟这事有关的席位（PEER_JOINED/PEER_LEFT 用；别的类型是 SEAT_UNSET）
	};

	// 观战结束的原因（棋盘取走 TakeWatchEnded 后据此决定要不要给玩家一句提示）。
	enum class WatchEnd : uint8_t
	{
		NONE = 0,
		USER,			// 我自己松的手/按 ESC（棋盘自己知道，不用提示）
		TIMEOUT,		// 3 秒没有快照——对面没响应（旧构建 / 他那边卡了）——提示
		TARGET_LEFT,	// 被看的人走了/掉线——提示
		SILENT			// 其他安静收场（换位后目标成了自己、关卡换代、对面主动停推）
	};

	// 一份快照解析后的样子（纯展示数据：接收端照它画，不建任何实体）。
	// 数组定长给满协议上限——发送端已在源头截断，这里不可能更多。
	struct ViewSnapshot
	{
		struct Zombie
		{
			uint16_t	mType;
			uint8_t		mRow;
			int			mX;			// 已经换算回像素坐标（协议上是 x+512）
			uint8_t		mHP;		// 0..100 的体血百分比
			uint8_t		mFlags;		// NetProto::SNAPSHOT_ZFLAG_*
		};
		struct Plant
		{
			uint8_t		mSeedType;
			uint8_t		mRow;
			uint8_t		mCol;
			uint8_t		mHP;
		};

		uint8_t		mFlags;			// NetProto::SNAPSHOT_FLAG_*（暂停/清完/截断）
		uint8_t		mWave;
		uint8_t		mWaveTotal;
		uint8_t		mRows;			// 这一关的行数（5/6），画背景按它来
		uint8_t		mMowers;		// bit r = 第 r 行推车还在
		int			mZombieCount;
		Zombie		mZombies[NetProto::SNAPSHOT_ZOMBIE_MAX];
		int			mPlantCount;
		Plant		mPlants[NetProto::SNAPSHOT_PLANT_MAX];
	};

	// 帧计数按主循环固定 10ms 一拍折算：100 帧 ≈ 1 秒
	static const int	HEARTBEAT_FRAMES			= 100;
	static const int	TIMEOUT_FRAMES				= 500;
	static const int	NOTICE_FRAMES				= 400;	// 即时说明挂多久（≈4 秒）
	// 中继握手掐表：发了 CREATE/JOIN 之后 10 秒没等到 WELCOME 就判死。
	// 直连那边**故意不掐表**（TCP 通了就一直等 HELLO），这条只走中继。
	static const int	RELAY_HANDSHAKE_FRAMES		= 1000;
	// 观战（队友场地查看）的三个计时，同样 10ms 一拍：
	static const int	WATCH_KEEPALIVE_FRAMES		= 200;	// 观看者每 2 秒一条保活
	static const int	WATCH_SNAPSHOT_FRAMES		= 300;	// 观看者 3 秒没有快照 = 对面没响应，观看收场
	static const int	WATCH_KEEPER_FRAMES			= 600;	// 被看方 6 秒收不到保活 = 观看者没了，停推

public:
	NetSession();
	~NetSession();

	bool			StartHost(uint16_t thePort = NetProto::DEFAULT_PORT);
	bool			StartJoin(const char* theHost, uint16_t thePort = NetProto::DEFAULT_PORT);

	// ---- M3 中继：连同一台服务器，建房 / 按房间码加入 ----
	// 直连那两个 Start* 保持原样并存：一个 Listen、一个 Connect，不经过服务器。
	// 中继这边两边都是 Connect 到服务器，身份（坐第几席、谁是房主）由服务器在 WELCOME 里点。
	// 房主断开 = 房间解散（服务器广播 ROOM_CLOSED 给其他人）。
	bool			StartRoomHost(const char* theServer, uint16_t thePort = NetProto::DEFAULT_RELAY_PORT);
	bool			StartRoomJoin(const char* theServer, const char* theRoomCode,
						uint16_t thePort = NetProto::DEFAULT_RELAY_PORT);
	bool			IsRelay() const { return mTransport == Transport::RELAY; }

	// 中继：本房房间码（4 字符，房主念给朋友的那串）。直连时空串。
	const std::string&	GetRoomCode() const { return mRoomCode; }
	// 中继：房主坐哪一席（服务器定的，不一定是 1）。直连固定 1。
	// 开局应答、给主机的话都发给它。
	uint8_t			GetHostSeat() const { return mHostSeat; }
	bool			IsHostSeat() const { return mLocalSeat != NetProto::SEAT_UNSET && mLocalSeat == mHostSeat; }

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

	// 除我之外还有没有上座席位。一个人开局（独处）时判"要不要等队友"就靠它：
	// 凭空等一个不存在的队友，闯关局会永久挂起。
	bool			HasOtherSeats() const;
	int				GetOccupiedSeatCount() const;

	// 席位是个环（换位）：1 → 2 → … → N → 1，末位的后一位是首位，人人都有换的对象。
	// 只看**上座**席位、跳过空位——中间有人走了，环自己把洞跳过去。
	uint8_t			NextOccupiedSeatInRing(uint8_t theSeat) const;
	// 席位也是条链（漏怪接力）：1 → 2 → … → N，**不绕回**首位——链的末端就是规则下限，
	// 末席漏怪是全队败。同样只认上座席位。
	uint8_t			NextOccupiedSeat(uint8_t theSeat) const;

	// 主动收摊（先给对方发 BYE）。之后可以重新 StartHost / StartJoin。
	void			Close();

	void			Update();			// 主线程每帧调一次

	// 主机开局：把这一局的模式、关卡、波表种子告诉队友。没连上时返回 false。
	// 联机闯关（R5）再多带三个数：是不是闯关局、局种子、关序号——队友据此对齐进度
	// （见 LawnApp::AlignRunToHost）；单关局用默认值，线格式上这三格是 0。
	// theRunMode（M4-b）是闯关的时长档（RunState::RUN_MODE_*）：它决定关卡表抽行与
	// 奖励屏数，队友必须按同一个档建局；单关局默认 0。
	// theRunDiff（MOD_BUILD 27）是房主选的出怪难度档（RunState::RUN_DIFF_*）：乘在全队
	// 出怪上（见 Board::PickZombieWaves），队友必须按同一个档建局；单关局默认标准 1。
	// theRunScale/theRunTempo/theRunZombotany（MOD_BUILD 35）是房主「高级选项」定的出怪
	// 规模档 / 节奏档 / 植物僵尸混入开关（RunState::RUN_SCALE_*/RUN_TEMPO_*）：随开局
	// 广播，队友按同一组建局；单关局默认 标准/标准/关。
	// theRunLevelIndex 由 u8 改 u16（MOD_BUILD 37）：无尽档的关序号会过 255；theRunEndlessScene
	// （MOD_BUILD 37）是无尽档锁定的场景 0..4，队友按同一场景建局；单关局/非无尽档默认 0。
	// theRunBossFlag（MOD_BUILD 38）= 闯关关底巨型 boss 开关（仅第一席生效）；默认 0。
	// theTargetSeat 默认 SEAT_UNSET = 发给所有队友；中途拉一个人进关必须点名单发——
	// 扇出会把已经在打的人重新点名一遍（那会重建棋盘）。
	bool			SendStartLevel(uint8_t theGameMode, uint32_t theLevel, int32_t theLevelSeed,
						bool theIsRun = false, int32_t theRunSeed = 0, uint16_t theRunLevelIndex = 0,
						uint8_t theTargetSeat = NetProto::SEAT_UNSET, uint8_t theRunMode = 0,
						uint8_t theRunDiff = 1, uint8_t theRunScale = 1,
						uint8_t theRunTempo = 1, uint8_t theRunZombotany = 0,
						uint8_t theRunEndlessScene = 0, uint8_t theRunBossFlag = 0);

	// 客户端侧：取出主机发来的开局命令（同时清掉）。没有就返回 false。
	// 单槽而不是队列：开局命令只有"最新那条"有意义，堆着旧的开局命令没有用处。
	bool			TakePendingStartLevel(NetProto::MsgStartLevel& theMsg);

	// 客户端侧：告诉主机"开局命令收到、我进场了"（theAccepted=false 是"现在不行"，
	// 比如人还在关卡里）。只在真的准备进场时发 true——不在主菜单（命令接不了）就别发 true，
	// 否则主机会一个人开着关跑下去。
	void			SendStartAck(bool theAccepted = true);

	// 主机侧：某个队友对开局命令的回应（按席位记账，取一条清一条，先进先出）。
	// theAccepted 是"进场 / 现在不行"。M2 只有两个席位，所以看上去和"那一条"没差别。
	bool			TakeStartAck(bool& theAccepted);

	// 主机侧：还在等谁回答吗（一个在等的都没有 = 齐了，可以往下走）。
	// 按**发命令那一刻上座的席位**记账：等待中有人走了，他那一格跟着作废、不再堵着；
	// 等待中新来的人不算数（他本来就没收到这条命令，等他就是等到天荒地老）。
	bool			AreAllStartAcksIn() const;

	// 主机侧：有人回了"现在不行"。粘着的一条——TakeStartAck 是取一条清一条的，
	// 细节会被取走，这条留着做"这次开不下去"的判断；下一次开局/回主菜单才清。
	bool			AnyStartAckRejected() const { return mAnyAckRejected; }

	// 主机侧：全员都进场了——广播"各席位开始做自己的三选一"（闯关 R6 新时序的最后一环：
	// 命令 → 各席位先进场并回 ACK → 收齐后 RUN_GO → 各席位在新草坪上放选项屏）。
	// 收齐的判定按**所有上座席位**记账，不再写死"就是那两个人"。
	bool			SendRunGo();

	// 客户端侧：主机放行了吗（取一次就清）。拿到才把选项屏放出来。
	bool			TakeRunGo();

	// 双向：报"我这一轮的选卡状态"（theReady=1 选好了 / 0 还在选）。选完卡不能一个人
	// 先开打——两边各按各的 Let's Rock，开打时间就对不上；谁先选完谁先报，等所有人到齐。
	// 是个状态不是事件，所以同一轮里重复报同一个值会被去重（和 SendLevelDone 同款）。
	bool			SendSeedsReady(bool theReady = true);

	// 其他上座席位这一轮都选好了没有（各席位报的是状态，本地只在开局/回菜单时重置）。
	// 单对端（直连）时就是"对面那一个"，语义与 M2 完全一致。
	bool			IsPeerSeedsReady();

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

	// 开始前换位置：跟"环上的后一位"换（末位的后一位是首位），对面同意才真的换。
	// 这一步只是把请求发出去，本机不动；同意/拒绝由对面定，见下面几条。
	// 没连上、已经有一个请求在等回话、或对面正问着我，都返回 false（按不动）。
	bool			SwapSeats();

	// 我发出的请求还在等对面回话（面板据此灰掉 Swap 键，也挡住重复请求）。
	bool			IsSwapRequestPending() const { return mSwapRequestSeat != NetProto::SEAT_UNSET; }

	// 对面正问我换不换（主循环看到就把面板叫出来，等玩家按同意/拒绝）。
	// 一次请求一直挂着为真，直到 AnswerSwapRequest 作答。
	bool			HasIncomingSwapRequest() const { return mSwapAskSeat != NetProto::SEAT_UNSET; }

	// 回答对面的换位请求。同意则两边各自执行同一次换位（本机当场生效，
	// 对面收到 REPLY 后跟着换）；拒绝则什么都不发生，对面只收到一句说明。
	void			AnswerSwapRequest(bool theAccept);

	// 一小句给玩家看的即时说明（"对面拒绝了"、"换过去了，你现在是 P3"），
	// 过几秒自己消失。没有就返回空串。
	const std::string&	GetNoticeText() const { return mNoticeText; }

	// 我方棋盘上的漏怪该传给谁：直连是"另一个固定席位"（1 ↔ 2），中继是**链上的
	// 下一个上座席位**（1 → 2 → 3 → 4，跳空洞）。
	// 返回 SEAT_UNSET = 没有下一家：末席漏怪就是全队败，没得传。
	// 只看席位号、不看谁建的房：换过位置后方向跟着换。
	uint8_t			GetRelayTargetSeat() const;

	// @pvz-online: 本机是不是漏怪链的末位（漏怪不再往下传、再漏就是全队败的那一家）。
	// 末位推车（每行一台、整局一次性）只给这一家发。单人房不算——一个席位谈不上"末位"，
	// 漏怪直接判负，推车给不给都一样。
	bool			IsLastRelaySeat() const;

	// 把一只漏怪交出去（席位由会话层填好）。末席、没连上、编码失败都返回 false——
	// 返回 false 就是"没传成"，调用方要按原版判负处理，绝不能悄悄让怪消失。
	bool			SendEscapedZombie(const NetProto::MsgEscapedZombie& theMsg);

	// 收下的漏怪（先进先出，一只都不许丢）。没有就返回 false。
	bool			TakePendingEscapedZombie(NetProto::MsgEscapedZombie& theMsg);

	// @pvz-online: 发阳光给队友（MOD_BUILD 38）：定向发往环上下一个上座席位（**环绕**，
	// 末席发给首位——与漏怪链的"末席即终点"不同，发阳光人人都能发），amount = 到账额
	// （发送端已扣档位与税）。单人房无人可发、没连上都返回 false——调用方按"没发出去"处理。
	bool			SendSunGift(uint16_t theAmount);

	// 收到的阳光（先进先出）。没有就返回 false。
	bool			TakePendingSunGift(NetProto::MsgSendSun& theMsg);

	// 双向：发一条局内快捷聊天（编号查 QuickChat.h：1-8 短语、9-16 植物表情）。
	// 沿用 Dispatch 默认扇出——所有其他上座席位各收一份，天然按最多六人泛化。
	// 没连上、编号非法、单人房无人可发都返回 false（调用方当"没发出去"处理）。
	bool			SendQuickChat(uint8_t theId);

	// 收到的快捷聊天（先进先出）。队列满时丢最旧、保最新——喊话是时间敏感信息，
	// 背压时最新一条最有用。没有就返回 false。
	bool			TakePendingQuickChat(NetProto::MsgQuickChat& theMsg);

	// ---- 观战（队友场地查看）----
	// 观看端：开始看某席位（按住 V 那一刻调一次）。发 BEGIN 并开始收快照；每 2 秒自动补
	// 一条保活，3 秒收不到快照就收场（WatchEnd::TIMEOUT）。没连上/席位越界/空位/
	// 是我自己，都返回 false（看不成）。
	bool			BeginWatch(uint8_t theSeat);
	// 观看端：结束观看（松开 V / ESC / 掉线兜底）。theNotify=true 会给被看方发 END
	//（对面立刻停推；不发的兜底是被看方 6 秒收不到保活自己停）。
	void			EndWatch(bool theNotify = true);
	bool			IsWatching() const { return mWatchTargetSeat != NetProto::SEAT_UNSET; }
	uint8_t			GetWatchTargetSeat() const { return mWatchTargetSeat; }
	// 观看结束的原因（取一次就清；还在看 / 没看过都是 NONE）。USER 和 SILENT 不用提示，
	// TIMEOUT 提示"对方没有响应"、TARGET_LEFT 提示"对方离开了"——棋盘自己择串。
	WatchEnd		TakeWatchEnded();
	// 观看端：最新一份快照到了没有（取一次就清；棋盘每帧问一次）。快照是"最新覆盖"的流，
	// 只留一份不排队——过期的战场定格没有展示价值，丢了就丢了。
	bool			TakeViewSnapshot(ViewSnapshot& theSnapshot);

	// 被看端：现在有人在看我吗（棋盘据此决定这一帧要不要打包快照）。
	bool			HasBoardWatchers() const { return mBoardWatcherMask != 0; }
	// 被看端：观众席位列表（棋盘按它逐个发快照——快照是单播，一个观众一份）。
	// 返回写进 theSeats 的个数（最多 theMax 个）。
	int				GetBoardWatcherSeats(uint8_t* theSeats, int theMax) const;
	// 被看端：推一份快照给一个观众（会话层负责分片与 seq，不解析内容）。
	// theBody/theSize 是快照数据段（格式见 NetProtocol.h 的 SNAPSHOT_* 注释），
	// 超过 SNAPSHOT_MAX_PARTS×250 字节的会被拒——那是调用方该裁的账。
	bool			SendBoardSnapshot(uint8_t theViewerSeat, const uint8_t* theBody, int theSize);

	State			GetState() const { return mState; }
	Role			GetRole() const { return mRole; }
	Transport		GetTransport() const { return mTransport; }
	bool			IsConnected() const { return mState == State::CONNECTED; }
	bool			IsActive() const { return mState != State::OFF; }
	uint8_t			GetLocalSeat() const { return mLocalSeat; }
	// 直连兼容 getter：直连就是固定的另一个席位（还没连上也算——HELLO 得先发出去）。
	// 多席位名册请用 IsSeatOccupied/GetSeatName 逐格问，别用这个。
	uint8_t			GetPeerSeat() const { return DirectPeerSeat(); }

	// 对端的构建代次（握手时报的，连上才有效）。
	uint16_t		GetPeerBuild() const;
	// 有席位的构建代次和本机不一样：包还是同一套，但行为可能不配套（比如漏怪在某代才真的
	// 落地）。**从 MOD_BUILD 12 起这不再拒绝握手**——照常连、UI 上挂一句提醒，
	// 玩家真撞上不对劲自己会去更新，比"整场连不上"实用（用户 2026-10-02 拍板）。
	bool			IsBuildDifferent() const;

	const std::string&	GetStatusText() const { return mStatusText; }
	const std::string&	GetHintText() const { return mHintText; }
	// 死因的一行短标签（主菜单小状态条那种一行宽的地方用）。没给短标签就是默认的
	// "Connection lost"——版本/构建不符这类要玩家动手的原因得自带短标签，
	// 否则在小条上看起来和"网线掉了"一模一样。
	const std::string&	GetShortStatus() const { return mShortStatus; }
	// 已经试到第几轮连接。连不上不再有上限，所以这是"还在试"和"卡死了"的唯一区别，
	// 状态行和主菜单小状态条都要用。
	int				GetConnectAttempts() const { return mLink.GetConnectAttempts(); }

	// @pvz-online 语言批（2026-10-03）：状态/提示/即时说明按哪种语言生成。这一层要能
	// 脱开框架单测（tools/nettest 直接编译本 .cpp、只链 ws2_32），不能引用 ModText——
	// 所以语言由上层注入：LawnApp 每帧在 Update 之前把当前语言判一下塞进来。从没注入过
	// = 英文（单测的默认，断言全按英文写的）。只影响文案生成，不动协议、不动状态机。
	void			SetTextChinese(bool theChinese) { mTextChinese = theChinese; }
	bool			IsTextChinese() const { return mTextChinese; }

	bool			PollEvent(Event& theEvent);

private:
	// 一个席位上的人和他在这一局里的状态（0 号位不用）。
	// 自己那格的两条状态身兼两职：既是本地记的值，也是"上次发出去的值"——发送去重
	// 就靠它。换位时整格跟着人走，所以"我报过什么"换过位置也不会丢。
	struct SeatInfo
	{
		bool			mOccupied;		// 有没有人：队友要真连上（中继：名册上有）才算
		uint16_t		mBuild;			// 他报的构建代次（0 = 还不知道）
		std::string		mName;
		bool			mLevelDone;		// 报的"草坪清完了"
		bool			mSeedsReady;	// 报的"这一轮选卡选好了"
		uint8_t			mAckState;		// 主机侧收到的开局回答：0 还没 / 1 进场 / 2 现在不行
		bool			mAwaitingAck;	// 主机侧：正在等他这一条
	};

	void			ResetToOff();
	void			SetConnected();
	void			SetDead(const char* theReason, const char* theShortReason = nullptr);
	void			UpdateStatusText();
	void			HandlePacket(const NetLink::Packet& thePacket);
	void			PushEvent(EventType theType, uint8_t theSeat = NetProto::SEAT_UNSET);
	void			ClearSeatTable();
	// 服务器控制帧（type ≥ 0xF000）整段收在这儿：只有中继模式会收到，管的是名册和路由，
	// 不碰游戏状态（名册变动走事件队列通知主循环）。
	void			HandleControlFrame(uint16_t theType, const uint8_t* thePayload, int theSize);
	// 中继：把 CREATE_ROOM / JOIN_ROOM 发出去（链路一通就发，只发一次）。
	bool			SendRoomRequest();
	// 中继：换位谈妥了，请服务器落实并广播（服务器是唯一权威，自己不许先换）。
	bool			SendSeatSwapCommit(uint8_t theSeatA, uint8_t theSeatB);
	// 单发一条"我这边清完了"给某个席位，不走去重记账（补发给中途进来的人用）。
	bool			SendLevelDoneTo(uint8_t theTarget, bool theDone);
	// 换位那几条挂起状态整个清掉（换成了、断了、收摊了）。
	void			ClearSwapState();
	// 直连：对端席位是固定的另一个（1 ↔ 2）。跟连没连上无关——HELLO 就得在这时候发出去。
	// 中继不用这个（那边席位由服务器点名）。
	uint8_t			DirectPeerSeat() const;
	// 执行一次换位：两格的内容对调（谁在几号位变了，人和他记的状态一起走）。
	// 直连时两边各自调一次（本机 + 那个对端）；中继时所有人收到服务器的广播各调一次。
	void			ApplySeatSwap(uint8_t theSeatA, uint8_t theSeatB);
	// 给对面回话。同意与否由调用方定；theTarget 是问我的那个席位。
	void			SendSwapReply(bool theAccepted, uint8_t theTarget);
	// 写一条几秒后自动消失的即时说明（"对面拒绝了"这类）。
	void			SetNotice(const char* theText, int theFrames = NOTICE_FRAMES);
	// 把"两边暂停到哪了"整个忘掉（新一局开始、掉线、收摊时用）。
	void			ClearPauseState();
	// 把"谁清完了"整个忘掉（新一局开始、掉线、收摊时用）。
	void			ClearLevelDoneState();
	// 观看端状态整段清掉（收摊/判死/关卡换代/看完了），并给棋盘留一句"为什么结束"
	// （theReason）。观众表（被看端）不在这里清：收摊/判死走 DiscardLevelPackets 一起清，
	// 那里的道别 END 是先发后清的。
	void			ClearWatchState(WatchEnd theReason);
	// 给某席位发一条 BOARD_WATCH（op 由调用方定；dst 就是 theSeat）。
	bool			SendBoardWatchMsg(uint8_t theSeat, uint8_t theOp);
	// 上座席位是不是都报了"清完了"。一个队友都没有的空局返回 false——
	// 单机里没人陪你判胜。
	bool			AreAllSeatsDone() const;

	// 把一帧交给该收的人。theTarget 默认 SEAT_UNSET = 所有队友（中继扇出；
	// 直连就那一个对端，行为与 M2 逐字节一致）。带席位号的载荷（前两字节 src/dst）
	// 会被这里逐目标改写 dst——别在调用方那边把 dst 写死。
	bool			Dispatch(uint16_t theType, uint8_t* thePayload, int theSize,
						uint8_t theTarget = NetProto::SEAT_UNSET);
	// 组帧直发链路（不管路由）。帧字节的构成全在这儿，改线格式只看这一个函数。
	bool			SendRaw(uint16_t theType, const uint8_t* thePayload, int thePayloadSize);
	void			SendHello();
	void			SendHelloAck(bool theAccepted, uint8_t theTarget);
	void			SendHeartbeat();
	void			SendBye(uint8_t theReason);

private:
	NetLink				mLink;
	Role				mRole;
	Transport			mTransport;
	State				mState;
	uint8_t				mLocalSeat;
	uint8_t				mHostSeat;			// 房主坐哪一席（直连固定 1；中继由服务器在 WELCOME 里点）
	std::string			mRoomCode;			// 中继：本房房间码（直连是空的）
	// 席位表：下标 = 席位号，1..NetProto::MAX_PLAYERS。"我"由 mLocalSeat 指认，
	// 表里不另存标记。空席位 mOccupied=false，其余字段保持中性值。
	SeatInfo			mSeats[NetProto::MAX_PLAYERS + 1];
	std::string			mLocalName;			// 不随 ResetToOff 清（见 SetLocalName）
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
	bool				mHasRunGo;			// 主机放行了没有（客户端侧收下的 RUN_GO，取一次就清）
	bool				mHasPendingLevelExit;
	NetProto::MsgLevelExit	mPendingLevelExit;
	bool				mAllDoneTaken;		// "全队都清完了"已经收过摊了
	bool				mHasPendingGameOver;
	NetProto::MsgGameOver	mPendingGameOver;
	// 两边共同认可的暂停态（同时也是发送去重的依据），以及"这次暂停是谁按的"。
	bool				mSharedPaused;
	bool				mPauseCameFromPeer;
	bool				mHasPendingPause;
	bool				mPendingPauseValue;
	std::vector<NetProto::MsgEscapedZombie>	mPendingEscapedZombies;
	// 收到的局内快捷聊天（收包在会话层、显示在棋盘，跨层不建 UI，理由同漏怪队列）。
	std::vector<NetProto::MsgQuickChat>	mPendingQuickChats;
	// @pvz-online: 收到的发阳光（MOD_BUILD 38）。跨层不碰棋盘，理由同漏怪队列。
	std::vector<NetProto::MsgSendSun>	mPendingSunGifts;
	// ---- 观战（队友场地查看）----
	// 观看端：我在看谁（SEAT_UNSET = 没在看）、两个计时、分片装配缓冲。
	uint8_t				mWatchTargetSeat;
	int					mFramesSinceWatchKeepalive;
	int					mFramesSinceWatchSnapshot;
	uint16_t			mWatchAssemblySeq;		// 正在拼的那一代（与来片 seq 不等就作废重来）
	int					mWatchAssemblyParts;	// 已按序收到的片数
	int					mWatchAssemblySize;		// 已收字节数
	uint8_t				mWatchAssemblyBuf[NetProto::SNAPSHOT_MAX_PARTS * NetProto::SNAPSHOT_PART_PAYLOAD];
	bool				mHasViewSnapshot;		// 有还没被棋盘取走的新快照
	ViewSnapshot		mLastSnapshot;
	WatchEnd			mWatchEndPending;		// 还没被取走的结束原因（NONE = 没有）
	// 被看端：谁在看我的场地（bit = 席位号，bit 1..6）+ 每人上次说话距今帧数
	//（6 秒没声就删——观看者掉线时 END 到不了，只能靠这个兜底）。
	uint8_t				mBoardWatcherMask;
	int					mBoardWatcherFrames[NetProto::MAX_PLAYERS + 1];
	uint16_t			mSnapshotSeq;			// 快照代次（每发一份 +1，u16 回绕）
	uint8_t				mSwapRequestSeat;	// 我发出的换位请求发给了谁（SEAT_UNSET = 没在等）
	uint8_t				mSwapAskSeat;		// 哪个席位正问我换不换（SEAT_UNSET = 没有）
	bool				mSwapCommitHandled;	// 中继：COMMIT 已经有人接手（我发的，或对面会发），等广播
	bool				mAnyAckRejected;	// 收到过"现在不行"（粘着，直到下一次开局）
	int					mFramesSinceRoomRequest;	// 中继：发了 CREATE/JOIN 之后等了多少帧（10 秒判死）
	std::string			mNoticeText;			// 即时说明（几秒后自己消失）
	int					mNoticeFrames;
	bool				mTextChinese;			// 文案语言（上层每帧注入，见 SetTextChinese）

	// 双语择串：按 mTextChinese 在两条 UTF-8（静态存储）里选一条。类似 ModText::Tr，
	// 但本层不能引用 ModText（单测不链那半边）——这就是本层自己的那份。
	std::string			NetText(const char* theZh, const char* theEn) const
	{
		return mTextChinese ? std::string(theZh) : std::string(theEn);
	}

	NetSession(const NetSession&);
	NetSession& operator=(const NetSession&);
};

#endif
