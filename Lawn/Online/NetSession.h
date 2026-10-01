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

public:
	NetSession();
	~NetSession();

	bool			StartHost(uint16_t thePort = NetProto::DEFAULT_PORT);
	bool			StartJoin(const char* theHost, uint16_t thePort = NetProto::DEFAULT_PORT);

	// 主动收摊（先给对方发 BYE）。之后可以重新 StartHost / StartJoin。
	void			Close();

	void			Update();			// 主线程每帧调一次

	// 主机开局：把这一局的模式、关卡、波表种子告诉队友。没连上时返回 false。
	bool			SendStartLevel(uint8_t theGameMode, uint32_t theLevel, int32_t theLevelSeed);

	// 客户端侧：取出主机发来的开局命令（同时清掉）。没有就返回 false。
	// 单槽而不是队列：开局命令只有"最新那条"有意义，堆着旧的开局命令没有用处。
	bool			TakePendingStartLevel(NetProto::MsgStartLevel& theMsg);

	State			GetState() const { return mState; }
	Role			GetRole() const { return mRole; }
	bool			IsConnected() const { return mState == State::CONNECTED; }
	bool			IsActive() const { return mState != State::OFF; }
	uint8_t			GetLocalSeat() const { return mLocalSeat; }
	uint8_t			GetPeerSeat() const { return mPeerSeat; }

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

	NetSession(const NetSession&);
	NetSession& operator=(const NetSession&);
};

#endif
