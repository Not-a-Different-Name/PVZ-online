#ifndef __NETPROTOCOL_H__
#define __NETPROTOCOL_H__

#include <cstdint>

// @pvz-online: M2 联机协议 v1。
//
// 与传输方式无关：现在走 TCP 直连，M3 换成 Go 中继时只换 NetLink，本文件不动。
//
//   +0        +2        +4
//   | u16 类型 | u16 长度 | payload（长度个字节）
//
// 两个字段都是小端；长度只算 payload，不含头。带席位的信息，payload 前两字节固定是
// { u8 srcSeat, u8 dstSeat }；席位从 1 开始编号，0 保留表示"未定"。
// 字段逐个按小端读写（Writer/Reader），所以结构体在内存里的对齐/填充与线格式无关。
// 收到的包长度与类型对不上时必须断线——那是串包，不是可以忽略的噪声。

namespace NetProto
{

const uint16_t	PROTOCOL_VERSION	= 1;
const uint16_t	DEFAULT_PORT		= 27777;

const uint8_t	SEAT_UNSET			= 0;
const uint8_t	SEAT_HOST			= 1;	// 建房方
const uint8_t	SEAT_CLIENT			= 2;	// 加入方

const int		HEADER_SIZE			= 4;
const int		MAX_PAYLOAD			= 256;
const int		MAX_PACKET			= HEADER_SIZE + MAX_PAYLOAD;

enum MessageType : uint16_t
{
	MSG_HELLO			= 1,	// C→H：协议版本 + 我要坐的席位
	MSG_HELLO_ACK		= 2,	// H→C：接受 / 拒绝
	MSG_START_LEVEL		= 3,	// H→C：模式、关卡、波表种子
	MSG_LEVEL_DONE		= 4,	// 双向：我这边清完了
	MSG_ESCAPED_ZOMBIE	= 5,	// 双向：漏怪传递（血量是传递那一刻的当前值）
	MSG_GAME_OVER		= 6,	// 双向：全队败北
	MSG_HEARTBEAT		= 7,	// 双向：1 秒一次，5 秒收不到判掉线
	MSG_BYE				= 8		// 双向：主动离开
};

enum ByeReason : uint8_t
{
	BYE_QUIT			= 0,
	BYE_TIMEOUT			= 1,
	BYE_PROTOCOL		= 2
};

enum GameOverReason : uint8_t
{
	GAMEOVER_ZOMBIES_WON	= 0,
	GAMEOVER_QUIT			= 1
};

// ====================================================================================================
// ★ 序列化
// ====================================================================================================

class Writer
{
public:
	Writer(uint8_t* theBuffer, int theCapacity) :
		mBuffer(theBuffer), mCapacity(theCapacity), mSize(0), mOverflow(false) { }

	void			U8(uint8_t theValue)
	{
		if (mSize + 1 > mCapacity) { mOverflow = true; return; }
		mBuffer[mSize++] = theValue;
	}
	void			U16(uint16_t theValue) { U8((uint8_t)(theValue & 0xFF)); U8((uint8_t)(theValue >> 8)); }
	void			U32(uint32_t theValue) { U16((uint16_t)(theValue & 0xFFFF)); U16((uint16_t)(theValue >> 16)); }
	void			I32(int32_t theValue) { U32((uint32_t)theValue); }
	void			Bytes(const void* theData, int theCount)
	{
		const uint8_t* aBytes = (const uint8_t*)theData;
		for (int i = 0; i < theCount; i++) U8(aBytes[i]);
	}

	int				Size() const { return mSize; }
	bool			Overflowed() const { return mOverflow; }

private:
	uint8_t*		mBuffer;
	int				mCapacity;
	int				mSize;
	bool			mOverflow;
};

class Reader
{
public:
	Reader(const uint8_t* theData, int theSize) :
		mData(theData), mSize(theSize), mPos(0), mOverflow(false) { }

	uint8_t			U8()
	{
		if (mPos + 1 > mSize) { mOverflow = true; return 0; }
		return mData[mPos++];
	}
	uint16_t		U16() { uint16_t aLow = U8(); uint16_t aHigh = U8(); return (uint16_t)(aLow | (aHigh << 8)); }
	uint32_t		U32() { uint32_t aLow = U16(); uint32_t aHigh = U16(); return aLow | (aHigh << 16); }
	int32_t			I32() { return (int32_t)U32(); }

	int				Remain() const { return mSize - mPos; }
	bool			Overflowed() const { return mOverflow; }

private:
	const uint8_t*	mData;
	int				mSize;
	int				mPos;
	bool			mOverflow;
};

// ====================================================================================================
// ★ 载荷（v1 全集；编解码函数按需在用到的那一步补上）
// ====================================================================================================

// HELLO：{ srcSeat=2, dstSeat=1, u16 version }
struct MsgHello
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint16_t		mVersion;
};

// HELLO_ACK：{ srcSeat=1, dstSeat=2, u16 version, u8 accepted }
struct MsgHelloAck
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint16_t		mVersion;
	uint8_t			mAccepted;
};

// START_LEVEL：{ srcSeat, dstSeat, u8 gameMode, u32 level, i32 levelSeed }
// levelSeed 是主机 GetLevelRandSeed() 的完整返回值（它含主机存档 ID，客户端必须整体覆盖）。
struct MsgStartLevel
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mGameMode;
	uint32_t		mLevel;
	int32_t			mLevelSeed;
};

// LEVEL_DONE：{ srcSeat, dstSeat }
struct MsgLevelDone
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
};

// ESCAPED_ZOMBIE：{ srcSeat, dstSeat, u8 row, u16 zombieType, u8 flags, i32 ×4 当前血量 }
// 只传"当前值"。上限/入场动画等由接收方按 zombieType 重新初始化后覆写。
struct MsgEscapedZombie
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mRow;
	uint16_t		mZombieType;
	uint8_t			mFlags;
	int32_t			mBodyHealth;
	int32_t			mHelmHealth;
	int32_t			mShieldHealth;
	int32_t			mFlyingHealth;
};

// GAME_OVER：{ srcSeat, dstSeat, u8 reason }
struct MsgGameOver
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mReason;
};

// HEARTBEAT：{ srcSeat, dstSeat, u32 tick }
struct MsgHeartbeat
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint32_t		mTick;
};

// BYE：{ srcSeat, dstSeat, u8 reason }
struct MsgBye
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mReason;
};

// ====================================================================================================
// ★ 编解码（用到哪条加哪条）
// ====================================================================================================

inline int EncodeStartLevel(uint8_t* theBuffer, int theCapacity, const MsgStartLevel& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mGameMode);
	aWriter.U32(theMsg.mLevel);
	aWriter.I32(theMsg.mLevelSeed);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeStartLevel(const uint8_t* theData, int theSize, MsgStartLevel& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mGameMode = aReader.U8();
	theMsg.mLevel = aReader.U32();
	theMsg.mLevelSeed = aReader.I32();
	return !aReader.Overflowed();
}

inline int EncodeHello(uint8_t* theBuffer, int theCapacity, const MsgHello& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U16(theMsg.mVersion);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeHello(const uint8_t* theData, int theSize, MsgHello& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mVersion = aReader.U16();
	return !aReader.Overflowed();
}

inline int EncodeHelloAck(uint8_t* theBuffer, int theCapacity, const MsgHelloAck& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U16(theMsg.mVersion);
	aWriter.U8(theMsg.mAccepted);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeHelloAck(const uint8_t* theData, int theSize, MsgHelloAck& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mVersion = aReader.U16();
	theMsg.mAccepted = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeHeartbeat(uint8_t* theBuffer, int theCapacity, const MsgHeartbeat& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U32(theMsg.mTick);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeHeartbeat(const uint8_t* theData, int theSize, MsgHeartbeat& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mTick = aReader.U32();
	return !aReader.Overflowed();
}

inline int EncodeBye(uint8_t* theBuffer, int theCapacity, const MsgBye& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mReason);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeBye(const uint8_t* theData, int theSize, MsgBye& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mReason = aReader.U8();
	return !aReader.Overflowed();
}

}

#endif
