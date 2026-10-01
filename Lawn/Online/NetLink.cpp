#include "NetLink.h"

// <winsock2.h> 必须排在所有 <windows.h> 之前。这个文件是工程里唯一同时碰两者的翻译单元：
// 其余代码只 include NetLink.h（pimpl，没有 winsock 类型），HTTPTransfer.cpp 用的
// <winsock.h> v1 在另一个 TU 里，互不影响。
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <cstring>
#include <string>
#include <vector>

#include "../../Sexy.TodLib/TodDebug.h"

namespace
{

const int	CONNECT_TIMEOUT_MS	= 5000;		// 连不上的话最多等这么久
const int	IO_SLICE_MS			= 100;		// select / recv 的分片，决定停线程的延迟上限
const int	MAX_QUEUED_PACKETS	= 256;
const int	SEND_RETRY_LIMIT	= 50;		// × IO_SLICE_MS = 发送最多重试 5 秒
const DWORD	THREAD_EXIT_WAIT_MS	= 2000;

void FormatSocketError(char* theBuffer, int theCapacity, const char* thePrefix, int theError)
{
	_snprintf_s(theBuffer, theCapacity, _TRUNCATE, "%s (WSA %d)", thePrefix, theError);
}

unsigned __stdcall LinkThreadProc(void* theArg);

}

// ====================================================================================================
// ★ Impl
// ====================================================================================================

struct NetLink::Impl
{
	SOCKET				mListenSocket;
	SOCKET				mSocket;			// 已连接的那个；发布之后只有 Close() 能关它
	HANDLE				mThread;
	volatile LONG		mStop;
	volatile LONG		mState;
	CRITICAL_SECTION	mQueueLock;
	std::vector<Packet>	mQueue;
	char				mHost[64];
	uint16_t			mPort;
	char				mLastError[160];
	bool				mWsaReady;

	State	GetStateValue() const { return (State)mState; }
	void	SetStateValue(State theState) { InterlockedExchange(&mState, (LONG)theState); }

	void	Fail(const char* theText)
	{
		strncpy_s(mLastError, theText, _TRUNCATE);
		SetStateValue(State::FAILED);
		TodLog("[net] link failed: %s", mLastError);
	}

	void	FailWithError(const char* thePrefix, int theError)
	{
		FormatSocketError(mLastError, (int)sizeof(mLastError), thePrefix, theError);
		SetStateValue(State::FAILED);
		TodLog("[net] link failed: %s", mLastError);
	}

	void	SetupConnectedSocket(SOCKET theSocket)
	{
		// accept() 在 Windows 上会继承监听 socket 的非阻塞标志，connect 前也设过非阻塞，
		// 所以这里必须显式改回阻塞 + 100ms 超时，否则收包线程会空转。
		u_long aNonBlocking = 0;
		ioctlsocket(theSocket, FIONBIO, &aNonBlocking);

		int aNoDelay = 1;
		setsockopt(theSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&aNoDelay, sizeof(aNoDelay));

		int aTimeout = IO_SLICE_MS;
		setsockopt(theSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&aTimeout, sizeof(aTimeout));
		setsockopt(theSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&aTimeout, sizeof(aTimeout));
	}

	bool	StartThread()
	{
		InterlockedExchange(&mStop, 0);
		unsigned aThreadId = 0;
		mThread = (HANDLE)_beginthreadex(nullptr, 0, &LinkThreadProc, this, 0, &aThreadId);
		if (!mThread)
		{
			Fail("Cannot start the network thread.");
			return false;
		}
		return true;
	}

	void	RunThread()
	{
		for (;;)
		{
			if (mStop) break;

			State aState = GetStateValue();
			if (aState == State::LISTENING)			DoAccept();
			else if (aState == State::CONNECTING)	DoConnect();
			else if (aState == State::CONNECTED)	DoReceive();
			else									break;		// FAILED / IDLE：交回主线程
		}
	}

	void	DoAccept()
	{
		fd_set aRead;
		FD_ZERO(&aRead);
		FD_SET(mListenSocket, &aRead);
		timeval aTimeout;
		aTimeout.tv_sec = 0;
		aTimeout.tv_usec = IO_SLICE_MS * 1000;

		int aReady = select(0, &aRead, nullptr, nullptr, &aTimeout);
		if (aReady <= 0) return;						// 0 = 超时，继续等

		SOCKET aSocket = accept(mListenSocket, nullptr, nullptr);
		if (aSocket == INVALID_SOCKET) return;

		// 两个席位只需要一条连接，接上就把监听口关掉（这个 socket 还没发布出去，
		// 由本线程关闭是安全的；主线程此时只会在 Close() 里等一下再动它）。
		closesocket(mListenSocket);
		mListenSocket = INVALID_SOCKET;

		mSocket = aSocket;
		SetupConnectedSocket(mSocket);
		SetStateValue(State::CONNECTED);
		TodLog("[net] accepted a peer");
	}

	void	DoConnect()
	{
		char aPortText[16];
		_snprintf_s(aPortText, (int)sizeof(aPortText), _TRUNCATE, "%u", (unsigned)mPort);

		addrinfo aHints;
		memset(&aHints, 0, sizeof(aHints));
		aHints.ai_family = AF_INET;
		aHints.ai_socktype = SOCK_STREAM;
		aHints.ai_protocol = IPPROTO_TCP;

		addrinfo* aResult = nullptr;
		int aLookupError = getaddrinfo(mHost, aPortText, &aHints, &aResult);
		if (aLookupError != 0 || aResult == nullptr)
		{
			Fail("Cannot resolve that address.");
			return;
		}

		SOCKET aSocket = socket(aResult->ai_family, aResult->ai_socktype, aResult->ai_protocol);
		if (aSocket == INVALID_SOCKET)
		{
			freeaddrinfo(aResult);
			Fail("Cannot create a socket.");
			return;
		}

		u_long aNonBlocking = 1;
		ioctlsocket(aSocket, FIONBIO, &aNonBlocking);

		int aConnectResult = connect(aSocket, aResult->ai_addr, (int)aResult->ai_addrlen);
		int aConnectError = (aConnectResult == SOCKET_ERROR) ? WSAGetLastError() : 0;
		freeaddrinfo(aResult);

		if (aConnectError != 0 && aConnectError != WSAEWOULDBLOCK &&
			aConnectError != WSAEINPROGRESS && aConnectError != WSAEALREADY)
		{
			closesocket(aSocket);
			FailWithError("Cannot connect to the host", aConnectError);
			return;
		}

		// 可写 = 连接有结果（成功或失败，靠 SO_ERROR 分辨）。按 100ms 分片等，好响应停止位。
		DWORD aStart = GetTickCount();
		for (;;)
		{
			if (mStop) { closesocket(aSocket); return; }

			fd_set aWrite;
			FD_ZERO(&aWrite);
			FD_SET(aSocket, &aWrite);
			timeval aTimeout;
			aTimeout.tv_sec = 0;
			aTimeout.tv_usec = IO_SLICE_MS * 1000;

			int aReady = select(0, nullptr, &aWrite, nullptr, &aTimeout);
			if (aReady == SOCKET_ERROR)
			{
				int anError = WSAGetLastError();
				closesocket(aSocket);
				FailWithError("Cannot connect to the host", anError);
				return;
			}
			if (aReady > 0) break;

			if (GetTickCount() - aStart > (DWORD)CONNECT_TIMEOUT_MS)
			{
				closesocket(aSocket);
				Fail("Timed out connecting to the host.");
				return;
			}
		}

		int aSoError = 0;
		int aSoErrorSize = (int)sizeof(aSoError);
		getsockopt(aSocket, SOL_SOCKET, SO_ERROR, (char*)&aSoError, &aSoErrorSize);
		if (aSoError != 0)
		{
			closesocket(aSocket);
			FailWithError("Cannot connect to the host", aSoError);
			return;
		}

		mSocket = aSocket;
		SetupConnectedSocket(mSocket);
		SetStateValue(State::CONNECTED);
		TodLog("[net] connected to %s:%u", mHost, (unsigned)mPort);
	}

	// 返回 true = 读满；false = 连接已不可用（错误/停止位，状态已置好）
	bool	RecvExact(uint8_t* theBuffer, int theCount)
	{
		int aGot = 0;
		while (aGot < theCount)
		{
			if (mStop) return false;

			int aRead = recv(mSocket, (char*)theBuffer + aGot, theCount - aGot, 0);
			if (aRead > 0)
			{
				aGot += aRead;
				continue;
			}
			if (aRead == 0)
			{
				Fail("The other player disconnected.");
				return false;
			}

			int anError = WSAGetLastError();
			if (anError == WSAETIMEDOUT) continue;					// 只是暂时没数据
			if (anError == WSAEWOULDBLOCK) { Sleep(1); continue; }
			if (anError == WSAENOTSOCK || anError == WSAEINTR) return false;	// 正在 Close()
			FailWithError("Connection lost", anError);
			return false;
		}
		return true;
	}

	void	DoReceive()
	{
		uint8_t aHeader[NetProto::HEADER_SIZE];
		if (!RecvExact(aHeader, NetProto::HEADER_SIZE)) return;

		uint16_t aPayloadSize = (uint16_t)(aHeader[2] | (aHeader[3] << 8));
		if (aPayloadSize > NetProto::MAX_PAYLOAD)
		{
			Fail("The other player sent a malformed packet.");
			return;
		}

		Packet aPacket;
		memcpy(aPacket.mData, aHeader, NetProto::HEADER_SIZE);
		aPacket.mSize = NetProto::HEADER_SIZE;

		if (aPayloadSize > 0)
		{
			if (!RecvExact(aPacket.mData + NetProto::HEADER_SIZE, aPayloadSize)) return;
			aPacket.mSize += aPayloadSize;
		}

		PushPacket(aPacket);
	}

	void	PushPacket(const Packet& thePacket)
	{
		EnterCriticalSection(&mQueueLock);
		if ((int)mQueue.size() >= MAX_QUEUED_PACKETS)
		{
			LeaveCriticalSection(&mQueueLock);
			Fail("Too many unread packets - the connection is stalled.");
			return;
		}
		mQueue.push_back(thePacket);
		LeaveCriticalSection(&mQueueLock);
	}
};

namespace
{

unsigned __stdcall LinkThreadProc(void* theArg)
{
	((NetLink::Impl*)theArg)->RunThread();
	return 0;
}

}

// ====================================================================================================
// ★ NetLink
// ====================================================================================================

NetLink::NetLink()
{
	mImpl = new Impl();
	mImpl->mListenSocket = INVALID_SOCKET;
	mImpl->mSocket = INVALID_SOCKET;
	mImpl->mThread = nullptr;
	mImpl->mStop = 0;
	mImpl->mState = (LONG)State::IDLE;
	mImpl->mHost[0] = '\0';
	mImpl->mPort = 0;
	mImpl->mLastError[0] = '\0';
	mImpl->mWsaReady = false;
	InitializeCriticalSection(&mImpl->mQueueLock);

	WSADATA aData;
	if (WSAStartup(MAKEWORD(2, 2), &aData) == 0)
	{
		mImpl->mWsaReady = true;
	}
	else
	{
		mImpl->Fail("Winsock could not start.");
	}
}

NetLink::~NetLink()
{
	if (!mImpl) return;

	Close();
	DeleteCriticalSection(&mImpl->mQueueLock);
	if (mImpl->mWsaReady) WSACleanup();

	delete mImpl;
	mImpl = nullptr;
}

bool NetLink::Listen(uint16_t thePort)
{
	Impl* anImpl = mImpl;
	if (!anImpl || anImpl->mThread) return false;
	if (!anImpl->mWsaReady) { anImpl->Fail("Winsock is not available."); return false; }

	SOCKET aSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (aSocket == INVALID_SOCKET) { anImpl->Fail("Cannot create a socket."); return false; }

	int aReuse = 1;
	setsockopt(aSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&aReuse, sizeof(aReuse));

	sockaddr_in anAddress;
	memset(&anAddress, 0, sizeof(anAddress));
	anAddress.sin_family = AF_INET;
	anAddress.sin_addr.s_addr = INADDR_ANY;
	anAddress.sin_port = htons(thePort);

	if (bind(aSocket, (sockaddr*)&anAddress, sizeof(anAddress)) == SOCKET_ERROR)
	{
		int anError = WSAGetLastError();
		closesocket(aSocket);
		FormatSocketError(anImpl->mLastError, (int)sizeof(anImpl->mLastError),
			anError == WSAEADDRINUSE ? "That port is already in use" : "Cannot host on that port", anError);
		anImpl->SetStateValue(State::FAILED);
		TodLog("[net] listen failed: %s", anImpl->mLastError);
		return false;
	}

	if (listen(aSocket, 1) == SOCKET_ERROR)
	{
		closesocket(aSocket);
		anImpl->Fail("Cannot listen on that port.");
		return false;
	}

	u_long aNonBlocking = 1;
	ioctlsocket(aSocket, FIONBIO, &aNonBlocking);

	anImpl->mListenSocket = aSocket;
	anImpl->mPort = thePort;
	anImpl->mLastError[0] = '\0';
	anImpl->SetStateValue(State::LISTENING);
	TodLog("[net] listening on port %u", (unsigned)thePort);
	return anImpl->StartThread();
}

bool NetLink::Connect(const char* theHost, uint16_t thePort)
{
	Impl* anImpl = mImpl;
	if (!anImpl || anImpl->mThread) return false;
	if (!anImpl->mWsaReady) { anImpl->Fail("Winsock is not available."); return false; }
	if (!theHost || !theHost[0]) { anImpl->Fail("No address given."); return false; }

	strncpy_s(anImpl->mHost, theHost, _TRUNCATE);
	anImpl->mPort = thePort;
	anImpl->mLastError[0] = '\0';
	anImpl->SetStateValue(State::CONNECTING);
	return anImpl->StartThread();
}

void NetLink::Close()
{
	Impl* anImpl = mImpl;
	if (!anImpl) return;

	InterlockedExchange(&anImpl->mStop, 1);

	if (anImpl->mThread)
	{
		DWORD aWait = WaitForSingleObject(anImpl->mThread, THREAD_EXIT_WAIT_MS);
		if (aWait != WAIT_OBJECT_0)
		{
			// 所有阻塞调用都是 100ms 分片的，正常不可能走到这里
			TodLog("[net] warning: receive thread did not exit in time");
		}
		CloseHandle(anImpl->mThread);
		anImpl->mThread = nullptr;
	}

	if (anImpl->mListenSocket != INVALID_SOCKET)
	{
		closesocket(anImpl->mListenSocket);
		anImpl->mListenSocket = INVALID_SOCKET;
	}
	if (anImpl->mSocket != INVALID_SOCKET)
	{
		closesocket(anImpl->mSocket);
		anImpl->mSocket = INVALID_SOCKET;
	}

	ClearQueue();
	anImpl->SetStateValue(State::IDLE);
}

NetLink::State NetLink::GetState() const
{
	return mImpl ? mImpl->GetStateValue() : State::FAILED;
}

bool NetLink::IsConnected() const
{
	return GetState() == State::CONNECTED;
}

bool NetLink::Send(const void* theData, int theSize)
{
	Impl* anImpl = mImpl;
	if (!anImpl || anImpl->GetStateValue() != State::CONNECTED) return false;
	if (!theData || theSize <= 0 || theSize > NetProto::MAX_PACKET) return false;

	const char* aBytes = (const char*)theData;
	int aSent = 0;
	int aRetries = 0;
	while (aSent < theSize)
	{
		int aWrote = send(anImpl->mSocket, aBytes + aSent, theSize - aSent, 0);
		if (aWrote > 0)
		{
			aSent += aWrote;
			aRetries = 0;
			continue;
		}

		int anError = WSAGetLastError();
		if (anError == WSAETIMEDOUT || anError == WSAEWOULDBLOCK)
		{
			if (++aRetries > SEND_RETRY_LIMIT)
			{
				anImpl->Fail("The connection stalled while sending.");
				return false;
			}
			continue;
		}

		anImpl->FailWithError("Connection lost while sending", anError);
		return false;
	}
	return true;
}

bool NetLink::Poll(Packet& thePacket)
{
	Impl* anImpl = mImpl;
	if (!anImpl) return false;

	bool aGot = false;
	EnterCriticalSection(&anImpl->mQueueLock);
	if (!anImpl->mQueue.empty())
	{
		thePacket = anImpl->mQueue.front();
		anImpl->mQueue.erase(anImpl->mQueue.begin());
		aGot = true;
	}
	LeaveCriticalSection(&anImpl->mQueueLock);
	return aGot;
}

void NetLink::ClearQueue()
{
	Impl* anImpl = mImpl;
	if (!anImpl) return;

	EnterCriticalSection(&anImpl->mQueueLock);
	anImpl->mQueue.clear();
	LeaveCriticalSection(&anImpl->mQueueLock);
}

const char* NetLink::GetLastError() const
{
	if (!mImpl || !mImpl->mLastError[0]) return "Connection lost.";
	return mImpl->mLastError;
}

std::string NetLink::GetLocalIPv4Text()
{
	WSADATA aData;
	if (WSAStartup(MAKEWORD(2, 2), &aData) != 0) return "127.0.0.1";

	char aName[256];
	aName[0] = '\0';
	if (gethostname(aName, (int)sizeof(aName)) != 0)
	{
		WSACleanup();
		return "127.0.0.1";
	}

	addrinfo aHints;
	memset(&aHints, 0, sizeof(aHints));
	aHints.ai_family = AF_INET;
	aHints.ai_socktype = SOCK_STREAM;

	addrinfo* aResult = nullptr;
	if (getaddrinfo(aName, nullptr, &aHints, &aResult) != 0 || aResult == nullptr)
	{
		WSACleanup();
		return "127.0.0.1";
	}

	std::string aPreferred;		// 192.168.x.x / 10.x.x.x 这类真局域网地址
	std::string aFallback;		// 其它（虚拟网卡、169.254 之类）

	for (addrinfo* anItr = aResult; anItr; anItr = anItr->ai_next)
	{
		if (!anItr->ai_addr) continue;

		uint32_t aHost = ntohl(((sockaddr_in*)anItr->ai_addr)->sin_addr.s_addr);
		if (aHost == 0 || (aHost >> 24) == 127) continue;

		char aText[32];
		_snprintf_s(aText, (int)sizeof(aText), _TRUNCATE, "%u.%u.%u.%u",
			(unsigned)((aHost >> 24) & 0xFF), (unsigned)((aHost >> 16) & 0xFF),
			(unsigned)((aHost >> 8) & 0xFF), (unsigned)(aHost & 0xFF));

		if ((aHost >> 24) == 192 || (aHost >> 24) == 10) { aPreferred = aText; break; }
		if (aFallback.empty()) aFallback = aText;
	}

	freeaddrinfo(aResult);
	WSACleanup();

	if (!aPreferred.empty()) return aPreferred;
	if (!aFallback.empty()) return aFallback;
	return "127.0.0.1";
}
