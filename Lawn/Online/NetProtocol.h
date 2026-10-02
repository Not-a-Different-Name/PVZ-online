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
// 7 → 8：开局确认分两态（START_ACK 带 accepted：队友在关卡里时明确回绝，
//        主机不再对着黑屏空等到天荒地老）。
// 8 → 9：暂停同步（一边暂停，两边都暂停；任一方都能暂停也能继续）。
// 9 → 10：漏怪传递落地（收的那边真把僵尸建到棋盘上）。协议一个字没动，但旧包
//        收到漏怪只会丢进日志——两边必须同版本，所以照样抬。
// 10 → 11：全队判胜/判负（LEVEL_DONE 谁清完了、GAME_OVER 全队败）。这两条消息
//        枚举里一直有、但从来没上过线：旧构建收到会当成没见过的消息直接断线。
// 11 → 12：构建不一致不再拒连（只提示、照常玩——见 NetSession 握手处），
//        判死的会话不再被 Update 每帧复活。两条都是握手/生命周期行为，
//        要两边都更新才有意义，所以照样抬。
// 12 → 13：全队败流程（两边都演"吃脑子"，之后只有主机能点 Try Again 整队重来同一关）。
// 13 → 14：联机闯关（R5）——开局命令带闯关上下文（局种子 + 关序号），队友拿来自我对齐、
//        补发追赶；主机换关 / 整队重来也走同一条"广播 + 等 START_ACK"。包长变了，
//        旧构建收到会被当串包拒掉，两边必须同版本。
// 14 → 15：闯关 R6 的四条规则改动（蘑菇全醒 + 售价 +25、咖啡豆改产 100 阳光、
//        升级植物直接种 + 价 = 原价 + 基础价、全队进草坪后才给三选一）。协议本身
//        只加了一条 RUN_GO，但四条要一起生效，所以共用这一次代次。
const uint16_t	MOD_BUILD			= 15;

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
	MSG_PAUSE			= 12,	// 双向：我暂停了 / 我继续了
	MSG_LEVEL_EXIT		= 13,	// 双向：我离开这一局、回主菜单了
	MSG_RUN_GO			= 14	// H→C：全员都已进场，各席位开始做自己的三选一
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

// START_LEVEL：{ srcSeat, dstSeat, u8 gameMode, u32 level, i32 levelSeed,
//                u8 isRun, i32 runSeed, u8 runLevelIndex }
// levelSeed 是主机 GetLevelRandSeed() 的完整返回值（它含主机存档 ID，客户端必须整体覆盖）。
// 闯关局（isRun=1）多带"这一局是谁的局、打到第几关"：队友拿它对上自己的检查点，
// 没检查点 / 对不上就从这一局的起点摆起、把欠下的三选一补回来（补做的屏和真打过的一模一样，
// 候选由 runSeed + 关序号推导）。单关局这三格全是 0，老语义一字不变。
struct MsgStartLevel
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mGameMode;
	uint32_t		mLevel;
	int32_t			mLevelSeed;
	uint8_t			mIsRun;
	int32_t			mRunSeed;
	uint8_t			mRunLevelIndex;
};

// LEVEL_DONE：{ srcSeat, dstSeat, u8 done }（1 = 我这块草坪清完了，0 = 又不清净了）
// 两态而不是"报一次就完"：漏怪本来就是往下一席位那块草坪送的，队友那儿漏过来的怪一落地，
// 收的那一方就又不算清完了——末席尤其明显，它清完还能再吃一记漏怪。一个位就能让各家的
// "谁清完了"始终是当下的事实，不用再为"撤回"开一条消息。
struct MsgLevelDone
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mDone;
};

// START_ACK：{ srcSeat, dstSeat, u8 accepted }
// 客户端说"开局命令收到，我这就进场"（accepted=1）；或者"现在不行"（accepted=0，
// 比如人还在关卡里、接不了这条命令）。主机收到才进场——两边进场只差一个单程；
// 回绝的话主机当场把这次开局作废，不至于对着没有界面的屏幕干等。
struct MsgStartAck
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mAccepted;
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

// PAUSE：{ srcSeat, dstSeat, u8 paused }（1 = 我暂停了，0 = 我继续了）
// 共识模型：任一方都能暂停、也能继续，不搞"请求/同意"。载荷只有两个状态，没带序号——
// 两个席位、TCP 保序，加上发送侧"状态没变就不发"这一条，就足以让两边收敛到同一个值。
// M3 扩到四个席位若真出现乱序需要，再补序号。
struct MsgPause
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mPaused;
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

// RUN_GO：{ srcSeat, dstSeat }
// 闯关 R6 的新时序里，主机收到全队的 START_ACK（"命令收到、我进场了"）之后广播这一条，
// 意思是"全员都到草坪上了，各自做自己的三选一"。客户端收到才把选项屏放出来——
// 先有草坪、后有选项，选项不再是盖在菜单/上一关残局上。
struct MsgRunGo
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
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
	aWriter.U8(theMsg.mIsRun);
	aWriter.I32(theMsg.mRunSeed);
	aWriter.U8(theMsg.mRunLevelIndex);
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
	theMsg.mIsRun = aReader.U8();
	theMsg.mRunSeed = aReader.I32();
	theMsg.mRunLevelIndex = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeLevelDone(uint8_t* theBuffer, int theCapacity, const MsgLevelDone& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mDone);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeLevelDone(const uint8_t* theData, int theSize, MsgLevelDone& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mDone = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeGameOver(uint8_t* theBuffer, int theCapacity, const MsgGameOver& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mReason);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeGameOver(const uint8_t* theData, int theSize, MsgGameOver& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mReason = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeStartAck(uint8_t* theBuffer, int theCapacity, const MsgStartAck& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mAccepted);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeStartAck(const uint8_t* theData, int theSize, MsgStartAck& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mAccepted = aReader.U8();
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

inline int EncodePause(uint8_t* theBuffer, int theCapacity, const MsgPause& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mPaused);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodePause(const uint8_t* theData, int theSize, MsgPause& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mPaused = aReader.U8();
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

inline int EncodeRunGo(uint8_t* theBuffer, int theCapacity, const MsgRunGo& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeRunGo(const uint8_t* theData, int theSize, MsgRunGo& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	return !aReader.Overflowed();
}

}

#endif
