#ifndef __NETLINK_H__
#define __NETLINK_H__

#include <cstdint>
#include <string>
#include "NetProtocol.h"

// @pvz-online: M2 传输层——winsock2 的最小封装。
//
// 这个头文件刻意不含任何 winsock 类型（实现藏在 Impl 里，走 pimpl）：工程里
// HTTPTransfer.cpp 用的是 winsock v1（<winsock.h>），两个版本不能进同一个翻译单元，
// 所以只有 NetLink.cpp 允许 include <winsock2.h>，别的代码只见这个干净接口。
//
// 线程模型
//   主线程：Listen/Connect/Send/Poll/Close（外加 GetState 轮询）
//   收包线程：accept / connect / recv，只做两件事——把整包塞进队列、把状态改成
//   CONNECTED 或 FAILED。
// 谁关 socket：只有 Close()（以及析构）关已经发布出去的 mListenSocket/mSocket，而且
// 它先置停止位、等线程退出、再关。收包线程只关它自己刚创建、还没发布给主线程的
// 临时 socket。这样两个线程永远不同时碰同一个句柄。
// 所有阻塞调用都带 100ms 超时（select 分片 + SO_RCVTIMEO），所以线程最多 200ms 就能退出。

class NetLink
{
public:
	enum class State
	{
		IDLE,
		LISTENING,		// 主机：已 bind+listen，等 accept
		CONNECTING,		// 客户端：正在连
		CONNECTED,
		FAILED			// 连不上 / 对端关了 / 包坏了——具体原因看 GetLastError()
	};

	struct Packet
	{
		uint8_t		mData[NetProto::MAX_PACKET];	// 4 字节头 + 最多 256 字节载荷
		int			mSize;
	};

	struct Impl;									// 实现细节，外部不要碰

public:
	NetLink();
	~NetLink();

	bool			Listen(uint16_t thePort);
	// 连不上不会放弃：一轮失败歇 1 秒就再来，直到连上或者 Close()。面板上的 Disconnect
	// 是唯一的中止方式（对调用方来说，这就是"取消连接等待时限"）。
	bool			Connect(const char* theHost, uint16_t thePort);
	void			Close();						// 收摊；之后可以重新 Listen/Connect

	State			GetState() const;
	bool			IsConnected() const;

	// 客户端已经试到第几次连接（从 1 起）。Connect() 之前恒为 1。
	int				GetConnectAttempts() const;

	// 主线程调用。false = 没发出去（连接多半已经不可用了）。
	bool			Send(const void* theData, int theSize);

	// 主线程调用：取出一整包（含 4 字节头）。队列空返回 false。
	bool			Poll(Packet& thePacket);
	void			ClearQueue();

	const char*		GetLastError() const;

	// 主机在面板上念给队友听的自己的局域网地址；取不到就返回 "127.0.0.1"。
	static std::string	GetLocalIPv4Text();

private:
	Impl*			mImpl;

	NetLink(const NetLink&);
	NetLink& operator=(const NetLink&);
};

#endif
