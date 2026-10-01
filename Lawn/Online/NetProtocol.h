#ifndef __NETPROTOCOL_H__
#define __NETPROTOCOL_H__

#include <cstdint>
#include <cstring>

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

// @pvz-online: mod 构建代次。协议号只管"包怎么编"，它管"两边行为配不配套"——两边都按
// PROTOCOL_VERSION=1 通信也可能一边是旧包（比如 C1 的包能正常连上，却不会跟着主机开局）。
// 凡是改动"需要两台机器一起更新"的东西（新增消息、开局/漏怪等行为）就 +1：
// 忘了 +1 的后果是新旧包互相认成同版，故障会以最难查的方式出现在棋盘上。
// 1 → 2：开局同步（主机广播开局命令后等客户端 START_ACK 才进场）。
// 2 → 3：漏怪传递（ESCAPED_ZOMBIE 开始真的发、真的收）。
// 3 → 4：位置交换（SWAP_SEATS）。
// 4 → 5：换位改成"和后一位换、对面同意才生效"（SWAP_REQUEST + SWAP_REPLY）。
// 5 → 6：握手里互报玩家名（名册 UI 要显示每个席位上是谁）。
// 6 → 7：退关同步（一方退回主菜单，对面跟着退，不再一边在关卡里一边在菜单上）。
const uint16_t	MOD_BUILD			= 7;

const uint16_t	DEFAULT_PORT		= 27777;

const uint8_t	SEAT_UNSET			= 0;
const uint8_t	SEAT_HOST			= 1;	// 建房方
const uint8_t	SEAT_CLIENT			= 2;	// 加入方

// @pvz-online: 一局的席位上限。M2 实际只开两个席位（见 NetSession.cpp 的 SEAT_COUNT），
// 但名册 UI 按这个数把位子全画出来——上下顺序就是顺位，空着的位子也得看得见。
const uint8_t	MAX_PLAYERS			= 4;

// 名字字段定长：这样"长度对不上 = 对面是别的构建版"那条检测还是准的（见 NetSession 收包），
// 改名长短不会把包长带得忽长忽短。位图字体只有 ASCII 字形，名字也够用。
const int		NAME_SIZE			= 16;

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
	MSG_BYE				= 8,	// 双向：主动离开
	MSG_START_ACK		= 9,	// C→H：开局命令收到并已进场（主机等这条才进）
	MSG_SWAP_REQUEST	= 10,	// 双向：请求和我后一位（末位的后一位是首位）交换位置
	MSG_SWAP_REPLY		= 11,	// 双向：对换位请求的回答；同意的话两边各自换
	// 12 留给 MSG_PAUSE（暂停同步，下一笔提交加；先用着 13 不打乱号段）
	MSG_LEVEL_EXIT		= 13	// 双向：我离开这一局、回主菜单了
};

// LEVEL_EXIT 的 reason。0 是唯一的常规值（回主菜单）；其它留给以后
// （重开、超时解散之类）——先定一个字段，省得将来加一种"离开"又要改包长。
enum LevelExitReason : uint8_t
{
	EXIT_QUIT_TO_MENU	= 0
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
	// 读一段原始字节（定长字段）。读不够时 U8 会置溢出位并给 0，所以目标先被补成 0，
	// 不越界地把字段填满——长度不对由调用方按"构建不符"拦下。
	void			Bytes(void* theDest, int theCount)
	{
		uint8_t* aDest = (uint8_t*)theDest;
		for (int i = 0; i < theCount; i++) aDest[i] = U8();
	}

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

// 把名字装进定长字段：截到 NAME_SIZE-1 个字符，剩下的补 0——补 0 对面才能当 C 串读。
inline void SetName(char* theDest, const char* theSource)
{
	memset(theDest, 0, NAME_SIZE);
	if (!theSource) return;
	for (int i = 0; i < NAME_SIZE - 1 && theSource[i]; i++) theDest[i] = theSource[i];
}

// HELLO：{ srcSeat=2, dstSeat=1, u16 version, u16 build, char name[16] }
struct MsgHello
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint16_t		mVersion;
	uint16_t		mBuild;		// MOD_BUILD；旧包没有这两个字节也没有名字，长度就不一样
	char			mName[NAME_SIZE];
};

// HELLO_ACK：{ srcSeat=1, dstSeat=2, u16 version, u16 build, u8 accepted, char name[16] }
struct MsgHelloAck
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint16_t		mVersion;
	uint16_t		mBuild;
	uint8_t			mAccepted;
	char			mName[NAME_SIZE];
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

// START_ACK：{ srcSeat, dstSeat }
// 客户端说"开局命令收到了，我这就进场"。主机收到才进——两边进场只差一个单程，
// 也顺手挡住了"客户端当时不在主菜单、命令被丢掉，主机一个人开着关跑下去"。
struct MsgStartAck
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
};

// SWAP_REQUEST：{ srcSeat, dstSeat }（dst = 请求者的后一位，末位的后一位是首位）
// 席位是个环：1 → 2 → … → N → 1，换位就是"跟后面的那一位换"，人人都有换的对象。
// 请求不带别的参数：跟谁换由规则定死，不由请求者挑。
struct MsgSwapRequest
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
};

// SWAP_REPLY：{ srcSeat, dstSeat, u8 accepted }（src = 应对方，dst = 请求者）
// 同意才真的换：两边各自执行同一次"和后一位换"，结果一致。
// 拒绝就什么都不发生，请求方只收到一句说明。
struct MsgSwapReply
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mAccepted;
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

// LEVEL_EXIT：{ srcSeat, dstSeat, u8 reason }
// "我这一局不打了、回主菜单了"。对面收到也回主菜单（这一局对两人一起结束），
// 会话本身留着——两人都在菜单上，主机直接点关卡就能开下一局。
struct MsgLevelExit
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

inline int EncodeStartAck(uint8_t* theBuffer, int theCapacity, const MsgStartAck& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeStartAck(const uint8_t* theData, int theSize, MsgStartAck& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeSwapRequest(uint8_t* theBuffer, int theCapacity, const MsgSwapRequest& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeSwapRequest(const uint8_t* theData, int theSize, MsgSwapRequest& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeSwapReply(uint8_t* theBuffer, int theCapacity, const MsgSwapReply& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mAccepted);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeSwapReply(const uint8_t* theData, int theSize, MsgSwapReply& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mAccepted = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeEscapedZombie(uint8_t* theBuffer, int theCapacity, const MsgEscapedZombie& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mRow);
	aWriter.U16(theMsg.mZombieType);
	aWriter.U8(theMsg.mFlags);
	aWriter.I32(theMsg.mBodyHealth);
	aWriter.I32(theMsg.mHelmHealth);
	aWriter.I32(theMsg.mShieldHealth);
	aWriter.I32(theMsg.mFlyingHealth);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeEscapedZombie(const uint8_t* theData, int theSize, MsgEscapedZombie& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mRow = aReader.U8();
	theMsg.mZombieType = aReader.U16();
	theMsg.mFlags = aReader.U8();
	theMsg.mBodyHealth = aReader.I32();
	theMsg.mHelmHealth = aReader.I32();
	theMsg.mShieldHealth = aReader.I32();
	theMsg.mFlyingHealth = aReader.I32();
	return !aReader.Overflowed();
}

inline int EncodeHello(uint8_t* theBuffer, int theCapacity, const MsgHello& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U16(theMsg.mVersion);
	aWriter.U16(theMsg.mBuild);
	aWriter.Bytes(theMsg.mName, NAME_SIZE);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeHello(const uint8_t* theData, int theSize, MsgHello& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mVersion = aReader.U16();
	theMsg.mBuild = aReader.U16();
	aReader.Bytes(theMsg.mName, NAME_SIZE);
	// 对面写满了 16 个字节没留结束符也不能读出去：这里自己封口，解码出来的名字永远是 C 串
	theMsg.mName[NAME_SIZE - 1] = 0;
	return !aReader.Overflowed();
}

inline int EncodeHelloAck(uint8_t* theBuffer, int theCapacity, const MsgHelloAck& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U16(theMsg.mVersion);
	aWriter.U16(theMsg.mBuild);
	aWriter.U8(theMsg.mAccepted);
	aWriter.Bytes(theMsg.mName, NAME_SIZE);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeHelloAck(const uint8_t* theData, int theSize, MsgHelloAck& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mVersion = aReader.U16();
	theMsg.mBuild = aReader.U16();
	theMsg.mAccepted = aReader.U8();
	aReader.Bytes(theMsg.mName, NAME_SIZE);
	theMsg.mName[NAME_SIZE - 1] = 0;
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

inline int EncodeLevelExit(uint8_t* theBuffer, int theCapacity, const MsgLevelExit& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mReason);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeLevelExit(const uint8_t* theData, int theSize, MsgLevelExit& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mReason = aReader.U8();
	return !aReader.Overflowed();
}

}

#endif
