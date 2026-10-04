#ifndef __NETPROTOCOL_H__
#define __NETPROTOCOL_H__

#include <cstdint>
#include <cstring>

// @pvz-online: 联机协议 v1（M2 直连 + M3 Go 中继共用）。
//
// 帧格式与传输方式无关，两种模式完全一样：
//
//   +0        +2        +4
//   | u16 类型 | u16 长度 | payload（长度个字节）
//
// 两个字段都是小端；长度只算 payload，不含头。带席位的信息，payload 前两字节固定是
// { u8 srcSeat, u8 dstSeat }；席位从 1 开始编号，0 保留表示"未定"。
// 字段逐个按小端读写（Writer/Reader），所以结构体在内存里的对齐/填充与线格式无关。
// 收到的包长度与类型对不上时必须断线——那是串包，不是可以忽略的噪声。
//
// type 分两段（M3 中继）：1..15 是游戏帧——直连时两端互发；中继时客户端发给服务器，
// 服务器只读 payload[1]=dstSeat，把 payload 逐字节转给那个席位的连接。0xF000 起是
// 控制帧，只在中继模式下出现，由服务器生成/消费（建房、名册、保活、换位），不转发。

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
// 15 → 16：选卡等队友（SEEDS_READY）。选完卡不再各按各的 Let's Rock 直接开打，
//        要等到所有上座席位都报过"选好了"才放行——混搭时一边等一边开打，
//        第三关后的不同步就是这么来的，所以两边必须同版本。
// 16 → 17：M3 中继（连服务器组队，最多四人）。会话层从"固定两个席位"改成一张席位表，
//        新增中继传输与控制帧（建房/加入/名册/退房/换位广播/保活）。直连模式的线上字节
//        没变，但中继那一整套旧构建根本没有：混搭时一边能建房一边连不上，必须同版本。
// 17 → 18：闯关扩完整版第一步——一局从 5 关（白天）变 25 关（白天→夜→泳池→迷雾→屋顶），
//        僵尸血量/数量按场景阶梯缩放、种类梯度重排。START_LEVEL 里的关序号口径变了：
//        旧构建的关卡表只有 5 格，收到 5 以上的序号会越界建错关，必须两边同版本。
// 18 → 19：局内快捷聊天（MSG_QUICK_CHAT）：快捷短语 + 植物表情（T/E 键面板）。编号查表在
//        QuickChat.h。旧构建把这帧当没见过的消息静默丢——发的人以为喊了、队友没看见，
//        所以两端要一起更新。
// 19 → 20：闯关三档时长（完整 25 关 / 普通 15 关奖励×2 / 快速 10 关奖励×3）。START_LEVEL
//        的闯关变体加一个模式字节（载荷 17→18 字节）：模式决定关卡表抽行与奖励屏数，
//        混搭时两端按不同关表建场必演岔，必须同版本。旧长度的 START_LEVEL 会被当串包拒掉。
// 20 → 21：闯关/联机局不吃本机档案（系统排查 §5.9）：耙子/坚果包扎术的购买加成、
//        掉落档位升级、选卡器 7 行在联机局一律按"没买/没通关"算。混搭时买了一方的
//        局面明显占便宜（每关多一次耙子秒杀），所以两边同版本。
// 21 → 22：闯关里水生植物解除"只能种水里"的限制（睡莲/缠绕海草/水兵菇/香蒲可种草坪），
//        卡的"需要泳池"灰罩与"需要底座"拾取门槛一并失效（后者是 R6 紫卡直种的配套遗漏）；
//        另修换关时卡槽不刷新、残留旧植物（快速版第 2 关起）。混搭时一边能种一边种不了，
//        卡槽两边显示也不一致，必须两边同版本。
// 22 → 23：席位顺位刷怪乘数（四人 4:3:2:1 / 三人 3:2:1 / 二人 2:1，2026-10-03 定案，
//        取代早先文档里的 8:4:2:1）。协议一个字没动、各客户端只为自己的棋盘缩放，但混搭时
//        同一席位两边的出怪量不同，整个合作的难度口径对不上，必须两边同版本。
// 23 → 24：修正顺位乘数口径——23 误做成"占份"（全队合计 = 单机 1 倍、末位仅 1/10），
//        用户定案改为直接倍乘：末位 = 原量 ×1 不变、往前每位 +1 倍，且每波数量上限同倍
//        放大（四人局 1 号位 20→80、非闯关 50→200）。同一席位两边出怪量不同、两端要同版本。
// 24 → 25：闯关"丰富僵尸种类"（2026-10-03 用户定案）：抽怪权重平铺（普僵/路障不再占半场）、
//        种类名单整体提速一档、出怪点数曲线 +2/波。协议一个字没动、波表由种子两端各算，
//        但公式不同时同一关两边的怪种与数量都对不上，必须两边同版本。
// 25 → 26：开局选卡加厚（2026-10-03 多人实测反馈"开局难度略高"后用户定案）：一局开局从
//        "先挑两株"改为"先挑四株 + 两个增益"（进第 1 关前手里 6 株 + 2 个增益）。协议没动、
//        各玩家选各自的，但混搭时一方开局 2 株一方 6 株 + 2 增益，合作难度口径对不上，
//        必须两边同版本。
// 26 → 27：出怪难度档（2026-10-03 用户定案）：房主开局前在选模式页选全局出怪旋钮
//        轻松 ×0.5 / 标准 ×1.0 / 高压 ×2.0（倍率 2026-10-04 由 ×1.5 上调，字节语义不变）。
//        START_LEVEL 的闯关变体再加一个难度字节
//        （载荷 18→19 字节）：它乘在全队出怪上，混搭时一边半量一边满量，合作难度
//        直接对不上，必须同版本。旧长度的 START_LEVEL 会被当串包拒掉。
// 27 → 28：席位上限 4→6 + 顺位乘数改 2 的幂（2026-10-03 用户定案）：同一份波表按
//        上座顺位直接倍乘（末席 ×1、往前每位翻倍，五六人 16/32 封顶），每波数量上限
//        同倍放大。帧格式没动（宽松过渡：≤4 人房混版照连——两边乘数规则不同、手感
//        不一致；5+ 人房旧包收 WELCOME 会因席位超上限被解析拒绝；MOD_BUILD 不符只
//        提示不拒连，两边最好同版本）。
// 28 → 29：后期难度三件套（2026-10-04 玩家反馈后用户定案）：闯关难度阶梯 ×1.2→×1.5/场景、
//        高压档 ×1.5→×2.0、气球权重降到平铺的 1/4。帧格式与 runDiff 字节语义都没动，
//        但同种子两端各算的波表公式变了——混搭时同一关两边的怪种、数量与僵尸血量都
//        对不上，必须两边同版本（见 docs/03-过程与问题.md §5.36）。
const uint16_t	MOD_BUILD			= 29;

const uint16_t	DEFAULT_PORT		= 27777;

// 中继服务器端口。云服务器上用的是 97（那边预留/已放行的口）；直连模式仍走
// DEFAULT_PORT，两者互相独立。
const uint16_t	DEFAULT_RELAY_PORT	= 97;

const uint8_t	SEAT_UNSET			= 0;
const uint8_t	SEAT_HOST			= 1;	// 建房方
const uint8_t	SEAT_CLIENT			= 2;	// 加入方

// @pvz-online: 一局的席位上限。直连只用两个（主机 = 1、客户端 = 2），中继最多开六个
// （谁坐几号位由服务器点名册），但名册 UI 按这个数把位子全画出来——上下顺序就是顺位，
// 空着的位子也得看得见。
const uint8_t	MAX_PLAYERS			= 6;

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
	MSG_RUN_GO			= 14,	// H→C：全员都已进场，各席位开始做自己的三选一
	MSG_SEEDS_READY		= 15,	// 双向：我这一轮的选卡状态（1 = 选好了，等其他人）
	MSG_QUICK_CHAT		= 16	// 双向：局内快捷聊天，只传编号（1-8 短语、9-16 植物表情，查表在 QuickChat.h）
};

// ------------------------------------------------------------------------------------------------
// ★ 中继控制帧（服务器 ↔ 客户端；只在 Go 中继模式下出现，游戏帧 1..16 一个字节没改）
// ------------------------------------------------------------------------------------------------

// 控制帧的 type 下限：≥ 这个值的都是控制帧，服务器自己处理、不转发。
const uint16_t	SRV_FRAME_BASE		= 0xF000;

// 房间码：线上就 4 个字符（服务器从 A-Z2-9 里挑，去掉 I/O/0/1 这些看岔了会读错的）；
// 结构体里多留一个字节存结束符，当 C 串用。
const int		ROOM_CODE_LEN		= 4;
const int		ROOM_CODE_SIZE		= ROOM_CODE_LEN + 1;

enum ControlType : uint16_t
{
	MSG_SRV_WELCOME		= 0xF001,	// S→C 进房成功：你的席位 + 全量名册（含各席构建代次）
	MSG_SRV_REJECT		= 0xF002,	// S→C 建房/加入被拒（原因见 RejectReason）
	MSG_SRV_PEER_JOIN	= 0xF003,	// S→C 有人进来了（广播给房内其他人；新人自己的名册在 WELCOME 里）
	MSG_SRV_PEER_LEAVE	= 0xF004,	// S→C 有人走了（主动退 / 超时 / 被踢）
	MSG_SRV_ROOM_CLOSED	= 0xF005,	// S→C 房间解散（房主走了 / 服务器收摊）
	MSG_SRV_SEAT_SWAP	= 0xF006,	// S→C 全房广播：这两个席位已互换（换位的唯一权威）
	MSG_SRV_PING		= 0xF007,	// S→C 每秒一次保活（房里没人说话时唯一的入站流量）
	MSG_CLI_CREATE_ROOM	= 0xF011,	// C→S 建房
	MSG_CLI_JOIN_ROOM	= 0xF012,	// C→S 按房间码加入
	MSG_CLI_SWAP_COMMIT	= 0xF013,	// C→S 换位已谈妥，请服务器落实并广播
	MSG_CLI_PONG		= 0xF014,	// C→S 回应 SRV_PING
	MSG_CLI_LEAVE_ROOM	= 0xF015	// C→S 主动退房（比直接断 TCP 语义清楚，服务器能立刻广播）
};

enum RejectReason : uint8_t
{
	REJECT_PROTOCOL_VERSION	= 0,	// 客户端协议版本和服务器对不上
	REJECT_ROOM_NOT_FOUND	= 1,	// 没这个房间码
	REJECT_ROOM_FULL		= 2,	// 席位都有人了
	REJECT_BAD_CODE			= 3,	// 房间码格式不对
	REJECT_SERVER_BUSY		= 4,	// 服务器到上限了（常规负载碰不到，留给以后）
	REJECT_BAD_REQUEST		= 5		// 包本身不像话（长度/字段越界）
};

enum RoomClosedReason : uint8_t
{
	ROOM_CLOSED_HOST_LEFT	= 0,	// 房主的连接断了/退了——房间跟着散
	ROOM_CLOSED_SERVER		= 1		// 服务器收摊
};

enum PeerLeaveReason : uint8_t
{
	PEER_LEAVE_QUIT			= 0,	// 主动退房（LEAVE_ROOM 或关客户端）
	PEER_LEAVE_TIMEOUT		= 1,	// 10 秒没收到它的任何数据
	PEER_LEAVE_KICKED		= 2		// 被服务器请出去（床位冲突之类的兜底）
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
//                u8 isRun, i32 runSeed, u8 runLevelIndex, u8 runMode, u8 runDiff }
// levelSeed 是主机 GetLevelRandSeed() 的完整返回值（它含主机存档 ID，客户端必须整体覆盖）。
// 闯关局（isRun=1）多带"这一局是谁的局、打到第几关"：队友拿它对上自己的检查点，
// 没检查点 / 对不上就从这一局的起点摆起、把欠下的三选一补回来（补做的屏和真打过的一模一样，
// 候选由 runSeed + 关序号推导）。runMode 是闯关的时长档（RunState::RUN_MODE_*，M4-b）：
// 它决定关卡表抽行与每关后的奖励屏数，队友必须按同一个档建局。runDiff 是房主选的出怪
// 难度档（RunState::RUN_DIFF_*，轻松/标准/高压）：乘在全队出怪上，队友按同一个档建局；
// 对不上同样按"检查点不匹配"重建。单关局 isRun=0：runSeed/runLevelIndex/runMode 全是 0，
// runDiff 记标准档（1，单关局不参与难度缩放），其余语义一字不变。
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
	uint8_t			mRunMode;
	uint8_t			mRunDiff;
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
// M3 扩到六个席位若真出现乱序需要，再补序号。
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
// 中继模式下会话层对每个上座席位各发一帧（不是协议广播）。
struct MsgRunGo
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
};

// SEEDS_READY：{ srcSeat, dstSeat, u8 ready }
// "我这一轮的选卡是什么状态"：1 = 选好了（按下 Let's Rock），0 = 还在选（新一轮刚开始）。
// 两态而不是"报一次就完"，理由和 LEVEL_DONE 一样——它是个状态、不是一串事件：
// 各家在**新一轮开始时各报一次当下状态**（要选卡的报 0，这一关不用选卡的直接报 1），
// 按下 Let's Rock 再报一次 1。收的人只管读"对面现在是什么状态"，不用在本地挑时机清位——
// 清位就是丢消息（对面报得早、清得晚，两边就互相干等）。
// 谁先选完谁先报；收到的人若自己也选完了就放行，没选完就继续选。
// 所有上座席位都报"选好了"才开打——少一个人开打，两家从第一秒就不在一个时间线上。
// 中继模式下会话层对每个上座席位各发一帧（同 RUN_GO，不是协议广播）。
struct MsgSeedsReady
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mReady;
};

// QUICK_CHAT：{ srcSeat, dstSeat, u8 id }
// 局内快捷聊天：id 1..8 = 快捷短语、9..16 = 植物表情，线路上只传编号，文字与图片各机
// 本地查表（QuickChat.h）。收发都是事件不是状态，一次一发、不去重、按到达顺序显示。
// 前两个字节由 Dispatch 盖戳，发送方只填 id。
struct MsgQuickChat
{
	uint8_t			mSrcSeat;
	uint8_t			mDstSeat;
	uint8_t			mId;
};

// ====================================================================================================
// ★ 中继控制帧载荷
// ====================================================================================================

// WELCOME：{ u16 version, u8 yourSeat, u8 hostSeat, u8 seatCount, char roomCode[4],
//            seatCount × { u8 seat, u16 build, char name[16] } }
// 名册里**含自己那一席**（服务器视角的全房名单）；客户端填表时自己那格用本地名字，
// 不信名单里的副本——名字只有本机说了算（和直连握手同一条规矩）。
struct MsgSrvWelcome
{
	struct Seat
	{
		uint8_t		mSeat;
		uint16_t	mBuild;
		char		mName[NAME_SIZE];
	};

	uint16_t	mVersion;		// 服务器报的协议版本（对不上 REJECT，能走到这儿就是对的）
	uint8_t		mYourSeat;
	uint8_t		mHostSeat;		// 房主坐哪一席（房主的连接断开 = 房间解散）
	uint8_t		mSeatCount;		// 名册里有几席（≤ MAX_PLAYERS）
	char		mRoomCode[ROOM_CODE_SIZE];
	Seat		mSeats[MAX_PLAYERS];
};

// REJECT：{ u16 version, u8 reason }（reason 见 RejectReason）
struct MsgSrvReject
{
	uint16_t	mVersion;
	uint8_t		mReason;
};

// PEER_JOIN：{ u8 seat, u16 build, char name[16] }
struct MsgSrvPeerJoin
{
	uint8_t		mSeat;
	uint16_t	mBuild;
	char		mName[NAME_SIZE];
};

// PEER_LEAVE：{ u8 seat, u8 reason }（reason 见 PeerLeaveReason）
struct MsgSrvPeerLeave
{
	uint8_t		mSeat;
	uint8_t		mReason;
};

// ROOM_CLOSED：{ u8 reason }（reason 见 RoomClosedReason）
struct MsgSrvRoomClosed
{
	uint8_t		mReason;
};

// SEAT_SWAP / SWAP_COMMIT 共用形状：{ u8 seatA, u8 seatB }
// 服务器交换"席位→连接"映射后向全房广播 SEAT_SWAP，所有人（含请求方）只认这条广播
// 执行置换——单一权威，天然免掉"两边各自换一次"的双重应用问题。
struct MsgSeatSwap
{
	uint8_t		mSeatA;
	uint8_t		mSeatB;
};

// CREATE_ROOM：{ u16 version, u16 build, char name[16] }
struct MsgCliCreateRoom
{
	uint16_t	mVersion;
	uint16_t	mBuild;
	char		mName[NAME_SIZE];
};

// JOIN_ROOM：{ u16 version, u16 build, char name[16], char roomCode[4] }
struct MsgCliJoinRoom
{
	uint16_t	mVersion;
	uint16_t	mBuild;
	char		mName[NAME_SIZE];
	char		mRoomCode[ROOM_CODE_SIZE];
};

// PING / PONG / LEAVE_ROOM 的载荷是空的，不设结构体——帧头 type 本身把话说完了。

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
	aWriter.U8(theMsg.mRunMode);
	aWriter.U8(theMsg.mRunDiff);
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
	theMsg.mRunMode = aReader.U8();
	theMsg.mRunDiff = aReader.U8();
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

inline int EncodeSeedsReady(uint8_t* theBuffer, int theCapacity, const MsgSeedsReady& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mReady);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeSeedsReady(const uint8_t* theData, int theSize, MsgSeedsReady& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mReady = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeQuickChat(uint8_t* theBuffer, int theCapacity, const MsgQuickChat& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSrcSeat);
	aWriter.U8(theMsg.mDstSeat);
	aWriter.U8(theMsg.mId);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeQuickChat(const uint8_t* theData, int theSize, MsgQuickChat& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSrcSeat = aReader.U8();
	theMsg.mDstSeat = aReader.U8();
	theMsg.mId = aReader.U8();
	return !aReader.Overflowed();
}

// ---- 中继控制帧 ----

inline int EncodeSrvWelcome(uint8_t* theBuffer, int theCapacity, const MsgSrvWelcome& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U16(theMsg.mVersion);
	aWriter.U8(theMsg.mYourSeat);
	aWriter.U8(theMsg.mHostSeat);
	aWriter.U8(theMsg.mSeatCount);
	aWriter.Bytes(theMsg.mRoomCode, ROOM_CODE_LEN);
	int aCount = theMsg.mSeatCount;
	if (aCount > MAX_PLAYERS) aCount = MAX_PLAYERS;
	for (int i = 0; i < aCount; i++)
	{
		aWriter.U8(theMsg.mSeats[i].mSeat);
		aWriter.U16(theMsg.mSeats[i].mBuild);
		aWriter.Bytes(theMsg.mSeats[i].mName, NAME_SIZE);
	}
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeSrvWelcome(const uint8_t* theData, int theSize, MsgSrvWelcome& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mVersion = aReader.U16();
	theMsg.mYourSeat = aReader.U8();
	theMsg.mHostSeat = aReader.U8();
	theMsg.mSeatCount = aReader.U8();
	aReader.Bytes(theMsg.mRoomCode, ROOM_CODE_LEN);
	theMsg.mRoomCode[ROOM_CODE_LEN] = 0;
	// 名册比席位上限还大 = 不是我们的包，直接拒——顺带挡住越界写
	if (aReader.Overflowed() || theMsg.mSeatCount > MAX_PLAYERS) return false;
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		theMsg.mSeats[i].mSeat = 0;
		theMsg.mSeats[i].mBuild = 0;
		memset(theMsg.mSeats[i].mName, 0, NAME_SIZE);
	}
	for (int i = 0; i < theMsg.mSeatCount; i++)
	{
		theMsg.mSeats[i].mSeat = aReader.U8();
		theMsg.mSeats[i].mBuild = aReader.U16();
		aReader.Bytes(theMsg.mSeats[i].mName, NAME_SIZE);
		theMsg.mSeats[i].mName[NAME_SIZE - 1] = 0;
	}
	return !aReader.Overflowed();
}

inline int EncodeSrvReject(uint8_t* theBuffer, int theCapacity, const MsgSrvReject& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U16(theMsg.mVersion);
	aWriter.U8(theMsg.mReason);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeSrvReject(const uint8_t* theData, int theSize, MsgSrvReject& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mVersion = aReader.U16();
	theMsg.mReason = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeSrvPeerJoin(uint8_t* theBuffer, int theCapacity, const MsgSrvPeerJoin& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSeat);
	aWriter.U16(theMsg.mBuild);
	aWriter.Bytes(theMsg.mName, NAME_SIZE);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeSrvPeerJoin(const uint8_t* theData, int theSize, MsgSrvPeerJoin& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSeat = aReader.U8();
	theMsg.mBuild = aReader.U16();
	aReader.Bytes(theMsg.mName, NAME_SIZE);
	theMsg.mName[NAME_SIZE - 1] = 0;
	return !aReader.Overflowed();
}

inline int EncodeSrvPeerLeave(uint8_t* theBuffer, int theCapacity, const MsgSrvPeerLeave& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSeat);
	aWriter.U8(theMsg.mReason);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeSrvPeerLeave(const uint8_t* theData, int theSize, MsgSrvPeerLeave& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSeat = aReader.U8();
	theMsg.mReason = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeSrvRoomClosed(uint8_t* theBuffer, int theCapacity, const MsgSrvRoomClosed& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mReason);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeSrvRoomClosed(const uint8_t* theData, int theSize, MsgSrvRoomClosed& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mReason = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeSeatSwap(uint8_t* theBuffer, int theCapacity, const MsgSeatSwap& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U8(theMsg.mSeatA);
	aWriter.U8(theMsg.mSeatB);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeSeatSwap(const uint8_t* theData, int theSize, MsgSeatSwap& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mSeatA = aReader.U8();
	theMsg.mSeatB = aReader.U8();
	return !aReader.Overflowed();
}

inline int EncodeCliCreateRoom(uint8_t* theBuffer, int theCapacity, const MsgCliCreateRoom& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U16(theMsg.mVersion);
	aWriter.U16(theMsg.mBuild);
	aWriter.Bytes(theMsg.mName, NAME_SIZE);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeCliCreateRoom(const uint8_t* theData, int theSize, MsgCliCreateRoom& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mVersion = aReader.U16();
	theMsg.mBuild = aReader.U16();
	aReader.Bytes(theMsg.mName, NAME_SIZE);
	theMsg.mName[NAME_SIZE - 1] = 0;
	return !aReader.Overflowed();
}

inline int EncodeCliJoinRoom(uint8_t* theBuffer, int theCapacity, const MsgCliJoinRoom& theMsg)
{
	Writer aWriter(theBuffer, theCapacity);
	aWriter.U16(theMsg.mVersion);
	aWriter.U16(theMsg.mBuild);
	aWriter.Bytes(theMsg.mName, NAME_SIZE);
	aWriter.Bytes(theMsg.mRoomCode, ROOM_CODE_LEN);
	return aWriter.Overflowed() ? -1 : aWriter.Size();
}

inline bool DecodeCliJoinRoom(const uint8_t* theData, int theSize, MsgCliJoinRoom& theMsg)
{
	Reader aReader(theData, theSize);
	theMsg.mVersion = aReader.U16();
	theMsg.mBuild = aReader.U16();
	aReader.Bytes(theMsg.mName, NAME_SIZE);
	theMsg.mName[NAME_SIZE - 1] = 0;
	aReader.Bytes(theMsg.mRoomCode, ROOM_CODE_LEN);
	theMsg.mRoomCode[ROOM_CODE_LEN] = 0;
	return !aReader.Overflowed();
}

}

#endif
