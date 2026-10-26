#include "RunState.h"
#include "RunHistory.h"
#include "../TeamPlay.h"
#include "RunBuffs.h"
#include "RunCheckpointIO.h"
#include "EndlessLawn.h"
#include "../System/PlayerInfo.h"
#include "../ModText.h"
#include "RunPortSupport.h"
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include "../LawnCommon.h"
#include <format>
#include "../../PvzpLib/PvzpDebug.h"
#include "misc/Buffer.h"
#include "misc/MTRand.h"
#include "../../SexyAppFramework/SexyAppBase.h"

// @pvz-online: 闯关状态的实体。检查点格式（小端，x86 直写）：
//   u32 magic 'RUN1' + u16 版本 + u16（低字节 = 时长档，高字节 = 出怪难度档）
//   i32 runSeed + i32 levelIndex + u16 failCounts[25]
//   u16 卡池数 + 每株 u16 SeedType
//   u16 buff 数 + 每条 (u16 id, u16 层数)
//   u16 推车记账（v5） + u16 高级选项位包（v10） + u16 无尽场景档（v12）
// v14: adds ladder/buff mode bits and eight u16 zombie stacks + last voted level.
// 版本不符 / 越界一律当"没有检查点"——宁可从头开新局，也不带着半截数据进场。
// v2：一局从 5 关扩到 25 关，failCounts 数组跟着变长——v1 的档一律按"没有"处理。
// v3：加时长档（M4-b 三档时长）——旧版的保留位恒 0，恰好就是"完整版"，所以 v2 的档
//     直接按完整版续；载荷长度一个字节没变。
// v4：加出怪难度档（2026-10-03，房主开局前选的那种）——v3 的高字节保留位恒 0，恰好
//     就是"标准"，所以 v3 的档直接按标准续；载荷长度同样没变。
// v5：加末位推车记账（联机末位每行一台、整局一次性）——追加在载荷末尾；v4 及更老的档
//     读不到这两字节，按"一辆都没用"续。
// v6：批 18（2026-10-04）全局表尾插了第 9 条 Precision——全局 id 0..7 不动，单株 id 由
//     「8 + 下标」全体右移 1 变「9 + 下标」；v5 及更老的档读到 id >= 8 的全部 +1 读平
//     （旧档里 id 8 是第一个单株、不是 Precision，不会撞车）。载荷长度一个字节没变。
// v7：批十（2026-10-05）模式表改版——普通档由每场景第 1/3/5 关改抽 1/5 关（=原快速表）、
//     快速档改抽每场景第 5 关：同一个关序号在新表里指向别的引擎关，非完整档的旧检查点没
//     法安全续，一律当"没有检查点"（完整档的表没动，v2..v6 的完整档照续）。载荷长度没变。
// v8：2026-10-06 全局表删了「储备」（原 id 5）——全局 9→8 条，单株 id 由「9 + 下标」全体
//     左移 1 变「8 + 下标」；v7 及更老的档读到 id 5（储备）的丢弃，id >= 6 的全部 −1 读平
//     （v5 及更老的档链条：先按 v6 把 id >= 8 的 +1，再统一 −1）。载荷长度没变。
// v9：2026-10-09 全局表尾追加「排山倒海」（新 id 8）——全局 8→9 条，单株 id 由「8 + 下标」
//     全体右移 1 变「9 + 下标」；v8 及更老的档读到 id >= 8 的全部 +1 读平（旧档里 id 8 是
//     第一个单株、不是排山倒海，不会撞车）。载荷长度没变。
// v10：2026-10-09 批 C「高级选项」——尾追加一个 u16 位包：bit0..1 = 出怪规模档
//     （RUN_SCALE_*）、bit2..3 = 节奏档（RUN_TEMPO_*）、bit4 = 植物僵尸混入开关。
//     v9 及更老的档读不到，按 标准/标准/关 续（与 v3/v4 的保留位缺省同口径）。
// v11：2026-10-09 第二 buff 批 0（docs/06 §8.5）——新增第二单株表，id 区块
//     RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + 表下标（57..95）。buff 列表仍是
//     通用 id+count、载荷长度没变，但老版本会把第二表 id 当老单株解出错误条目，
//     故抬版本让老构建直接拒档；v10 及更老档没有这类 id，读平即可（迁移链不动）。
// v12：2026-10-10 多人无尽——新增第四档 RUN_MODE_ENDLESS（锁场景无限循环、难度走对数阶
//     曲线）与其场景字段 mEndlessScene（0..4）；载荷尾追加 u16 场景档。v11 及更老的档
//     不可能真是无尽档（mode 3 是 v12 才写的），读到 mode==ENDLESS 的一律拒档（伪造/错位）；
//     其余档读平，场景按 0 续。关数校验按 LevelCountForMode(3)=60000 封口（软上限）。

static const unsigned int RUN_CHECKPOINT_MAGIC = 0x314E5552;	// 'RUN1'
// v19: independent slots, endless rule bits, lineup mask, hashed lawn/retry snapshots,
// snapshot phase and completed zombie ballot rounds. Older saves retain endless options 0.

static const unsigned short RUN_CHECKPOINT_VERSION = 25; // v24 appends seven species traits; v23 stores exactly ten.

static std::vector<unsigned char>& AppendU16(std::vector<unsigned char>& theData, unsigned int theValue)
{
	theData.push_back((unsigned char)(theValue & 0xFF));
	theData.push_back((unsigned char)((theValue >> 8) & 0xFF));
	return theData;
}

static std::vector<unsigned char>& AppendI32(std::vector<unsigned char>& theData, int theValue)
{
	AppendU16(theData, (unsigned int)theValue & 0xFFFF);
	AppendU16(theData, ((unsigned int)theValue >> 16) & 0xFFFF);
	return theData;
}

// 读取游标：越界即失败，调用方统一走"检查点不可用"。
class RunReader
{
public:
	RunReader(const unsigned char* theData, int theLength) : mData(theData), mLength(theLength), mOffset(0) {}

	bool		ReadU16(unsigned int& theValue)
	{
		if (mOffset + 2 > mLength) return false;
		theValue = (unsigned int)mData[mOffset] | ((unsigned int)mData[mOffset + 1] << 8);
		mOffset += 2;
		return true;
	}

	bool		ReadU32(unsigned int& theValue)
	{
		unsigned int aLow = 0, aHigh = 0;
		if (!ReadU16(aLow) || !ReadU16(aHigh)) return false;
		theValue = aLow | (aHigh << 16);
		return true;
	}

	bool		ReadI32(int& theValue)
	{
		unsigned int aLow = 0, aHigh = 0;
		if (!ReadU16(aLow) || !ReadU16(aHigh)) return false;
		theValue = (int)(aLow | (aHigh << 16));
		return true;
	}

    bool ReadBlob(std::vector<unsigned char>& out) {
        int n=0;if(!ReadI32(n)||n<0||n>24*1024*1024||n>mLength-mOffset)return false;
        out.assign(mData+mOffset,mData+mOffset+n);mOffset+=n;unsigned hash=0;return ReadU32(hash) && hash==EndlessRules::Hash(out.data(),out.size());
    }
    void ReadStats(std::vector<unsigned char>& out)
    {
        out.clear();
        int length = 0;
        if (!ReadI32(length) || length < 0 || length > 8192 || length > mLength - mOffset - 4) return;
        const unsigned char* bytes = mData + mOffset;
        mOffset += length;
        unsigned hash = 0;
        if (ReadU32(hash) && hash == EndlessRules::Hash(bytes, static_cast<size_t>(length)))
            out.assign(bytes, bytes + length);
    }
private:
	const unsigned char*	mData;
	int						mLength;
	int						mOffset;
};

RunState::RunState()
{
	memset(mFailCounts, 0, sizeof(mFailCounts));
	StartNew(0);
}

// 无尽档「开局全植物」口径（2026-10-11 用户令，见 BeginStartPicks/StartNew）：把 0..47
// 常规植物补齐进卡池——模仿者不给（与词条候选同口径，它要先指定模仿对象）。StartNew 用它
// 给新局一次铺满；Load 用它把「两株起家」年代存下的旧无尽档在读档时一次补齐（同一个
// 无尽口径，不追溯非无尽档）。
static void FillPoolWithAllPlants(std::vector<SeedType>& thePool)
{
	bool aOwned[NUM_SEEDS_IN_CHOOSER];
	memset(aOwned, 0, sizeof(aOwned));
	for (size_t i = 0; i < thePool.size(); i++)
	{
		if (thePool[i] >= 0 && thePool[i] < NUM_SEEDS_IN_CHOOSER) aOwned[thePool[i]] = true;
	}
	for (int i = 0; i < (int)SeedType::SEED_IMITATER; i++)
	{
		if (!aOwned[i]) thePool.push_back((SeedType)i);
	}
}

void RunState::StartNew(int theRunSeed, int theRunMode, int theRunDiff, int theRunScale, int theRunTempo, int theZombotany, int theEndlessScene, int theBossFlag, int theBuffMult, int theLadder, int theZombieBuff, int theSpeedTier, int theFirstWave, int theEndlessOptions)
{
    mCrowdRule = CrowdRules::CURRENT; // @pvz-online: new runs only; old checkpoints load LEGACY.
	mEndlessOptions = theRunMode == RUN_MODE_ENDLESS && EndlessRules::Valid(theEndlessOptions) ? theEndlessOptions : 0;
    mEndlessFocusMask=0; mEndlessSnapshotLevel=-1; mEndlessSnapshotPhase=0;
    mEndlessVoteRounds=0;mEndlessVoteIds[0]=mEndlessVoteIds[1]=255;
    mEndlessLawn.clear(); mEndlessRetryLawn.clear();
	mRunSeed = theRunSeed;
    mDamageHistory.clear();
    mAdmission=Admission::State{};
    mTeam=TeamProgression::State{};
	mMode = theRunMode;
	mDiff = theRunDiff;
	// 无尽档锁定的场景（0..4）：入口链/线传/读档三处都过了闸，这里再夹一次兜底——
	// 坏场景会让 GetLevel 取出越界引擎关号。
	mEndlessScene = (theEndlessScene >= 0 && theEndlessScene < RUN_SCENE_COUNT) ? theEndlessScene : 0;
	mScale = theRunScale;
	mTempo = theRunTempo;
	mZombotany = theZombotany;
	// 关底 boss 开关：无尽档没有关底，恒记 0（与读档的非无尽口径一致）。
	mBossFlag = theRunMode != RUN_MODE_ENDLESS ? theBossFlag : 0;
	// 增益倍选（2026-10-13）：非法档不在这儿夹——消费全走 BuffMultValueFor（与 mScale/mTempo
	// 同口径：档位合法性归各自的 *For 函数）。
	mBuffMult = theBuffMult;
	// 场景难度阶梯（2026-10-14）：同上，合法性归 LadderPermilleFor。无尽档照记（不吃，但
	// 两端对齐/存档往返必须保真——线传了多少就原样带多少）。
	mLadder = theLadder;
    mSpeedTier=theSpeedTier;
	mFirstWave = theFirstWave >= RUN_FIRSTWAVE_STD && theFirstWave <= RUN_FIRSTWAVE_LONG ? theFirstWave : RUN_FIRSTWAVE_STD;
	// 僵尸 buff（2026-10-14）：本局层数表清零（叠层只在一局内累积），投票记账归哨兵。
	mZombieBuffMode = theZombieBuff;
	memset(mZombieBuffStacks, 0, sizeof(mZombieBuffStacks));
	mZombieVotedLevel = RUN_ZOMBIE_VOTED_NONE;
	mLevelIndex = 0;
    mLoanPlant=mLoanLevel=mLentLevel=-1; mLoanBuffs.clear();
	mRerollVouchers = 0;
	mRerollAwardedLevel = -1;
	mPool.clear();
	if (theRunMode == RUN_MODE_ENDLESS)
	{
		// 无尽档开局就全植物（2026-10-11 用户令「开局全植物」）：0..47 常规植物一次给满。
		// 卡池 48 > 10 格，往后每关都过原版选卡界面挑 10 株上场；CanOfferPlantPick 恒假，
		// 植物三选一屏自然不再发。
		FillPoolWithAllPlants(mPool);
	}
	else
	{
		mPool.push_back(SeedType::SEED_SUNFLOWER);
		mPool.push_back(SeedType::SEED_PEASHOOTER);
	}
	mBuffs.clear();
	memset(mFailCounts, 0, sizeof(mFailCounts));
	mMowerUsedRows = 0;
	mPendingPlantPicks = 0;
	mPendingBuffPicks = 0;
	mChoicesRolled = false;
	mPaidRerollCount = 0;		// 重摇阶梯价：新局从头起算（实际每屏还会在 RollChoices 清零）
	mPickSeenBuffCount = 0;		// 候选去重批：新局无露脸/冷却
	mPickCooldownBuffCount = 0;
	mPickSeenPlantCount = 0;	// 植物屏跨屏冷却（2026-10-16 用户令）：新局无露脸/冷却
	mPickCooldownPlantCount = 0;
	mPickCounter = 0;
	// 本机随机盐（用户 2026-10-03 定案：各玩家的候选不共用一套随机数）。不进检查点、
	// 不随联机命令走——pending 屏本来就不落盘，读档/追赶时重抽的屏用什么盐都合法。
	mPickSalt = (unsigned int)Sexy::Rand() ^ ((unsigned int)Sexy::Rand() << 16);
	mCatchUpLevel = -1;
	for (int i = 0; i < RUN_CHOICES; i++)
	{
		mPlantChoices[i] = SeedType::SEED_NONE;
		mBuffChoices[i] = 0;
	}
}

// 一局的两处选卡口：开局先挑四株 + 两个增益（进第 1 关前手里就有 6 株；2026-10-03 多人
// 实测反馈"开局难度略高"后用户定案加厚，原来只挑两株）。批十 2026-10-05：快速版 5 关全是
// 每场景收尾的难关，开局再多给两株两增益（4+2 株、2+2 增益）。2026-10-11 无尽档（用户令
// 「开局全植物，第一关前选四buff，之后每关选2」）：开局增益同样 2+2；植物屏因卡池开局
// 即满（StartNew 给全 48 株）自动归零，这一档开局的四次选卡全是增益。
// 每过一关的奖励屏见 BeginLevelEndPicks。屏的先后由 IsPlantPick 决定：先植物屏、再增益屏。
// 卡池拿满 48 株后植物屏没得抽——那之后只发增益屏。
void RunState::BeginStartPicks()
{
	// 新一批屏从"还没抽"开始（抽干作废过之后又会发新屏，别被上一轮的抽好标志挡住重抽）。
	mChoicesRolled = false;
	// 候选去重批：冷却集只在一批屏内滚动（上一屏 → 下一屏），换批即清。
	mPickSeenBuffCount = 0;
	mPickCooldownBuffCount = 0;
	mPickSeenPlantCount = 0;
	mPickCooldownPlantCount = 0;
	int aExtra = (mMode == RUN_MODE_QUICK || mMode == RUN_MODE_ENDLESS) ? 2 : 0;
	mPendingPlantPicks = CanOfferPlantPick() ? 4 + aExtra : 0;
	// 增益倍选（2026-10-13 用户令）：发放的 buff 屏数按倍率翻倍（只乘 buff 屏，植物屏不动）。
	mPendingBuffPicks = (2 + aExtra) * BuffMultValueFor(mBuffMult);
	PvzpLogLn("[run] start picks: {} plant(s) + {} buff(s) (mode {}, buff mult x{})", mPendingPlantPicks, mPendingBuffPicks, mMode, BuffMultValueFor(mBuffMult));
}

void RunState::BeginLevelEndPicks()
{
	// 新一批屏从"还没抽"开始（同 BeginStartPicks；也覆盖补发追赶 AdvanceCatchUp 的路径）。
	mChoicesRolled = false;
	// 候选去重批：冷却集只在一批屏内滚动（上一屏 → 下一屏），换批即清（同 BeginStartPicks）。
	mPickSeenBuffCount = 0;
	mPickCooldownBuffCount = 0;
	mPickSeenPlantCount = 0;
	mPickCooldownPlantCount = 0;

	// 每关后的植物奖励数（2026-10-09 定案）：完整 2 株/关不变、普通 6→3、快速 10→4——
	// 大幅收紧让植物保持稀缺（一局拿不满 48 株），单株增益候选池不被一口气铺宽。
	// 植物候选抽干时自动只发增益屏（见 CanOfferPlantPick）——无尽档开局池就满，这笔恒不发。
	static const int aPlants[] = { 2, 3, 4 };
	int aPerLevel = (mMode >= RUN_MODE_FULL && mMode <= RUN_MODE_QUICK) ? aPlants[mMode] : 2;
	mPendingPlantPicks = CanOfferPlantPick() ? aPerLevel : 0;
	// 增益每关收尾的发屏数（2026-10-08 玩家反馈批定案，2026-10-12 用户令收紧完整档）：普通档
	// （10 关）每关固定 3；快速档（5 关）逐关 3、4、5、5（= min(关序号 + 2, 5)——它的收尾点
	// 恰好 4 个：最后一关打完直接亮奖杯屏，调用点自己挡掉末点的屏）；完整档（25 关）改
	// **每关固定 1**（2026-10-12 用户令「25 关模式每关只给选一次 buff」——原批十曲线
	// min(关序号 + 2, 7) 逐关递增作废：25 关长局里增益堆得太滥）。
	// 调用点此刻 mLevelIndex 已经指向下一关，所以"下一关收尾该给的数"直接按它算。
	// 无尽档固定 2 条/关（2026-10-10 用户定案）：难度曲线逐关自涨，屏数不随关序号堆。
	int aBuffs;
	if (mMode == RUN_MODE_ENDLESS)
	{
		aBuffs = 2;
        int p=EndlessBossPeriod();
        if(p && mLevelIndex>0 && mLevelIndex%p==0) ++aBuffs;
	}
	else if (mMode == RUN_MODE_NORMAL)
	{
		aBuffs = 3;
	}
	else if (mMode == RUN_MODE_FULL)
	{
		aBuffs = (mLevelIndex>=1 && mLevelIndex<=3) ? 2 : 1;
	}
	else	// RUN_MODE_QUICK：3、4、5、5
	{
		aBuffs = mLevelIndex + 2;
		if (aBuffs > 5) aBuffs = 5;
	}
	mPendingBuffPicks = aBuffs * BuffMultValueFor(mBuffMult);	// 增益倍选（2026-10-13）：屏数乘倍
	PvzpLogLn("[run] level end picks: {} plant(s) + {} buff(s) (level {}, mode {}, buff mult x{})", mPendingPlantPicks, mPendingBuffPicks, mLevelIndex, mMode, BuffMultValueFor(mBuffMult));
}

bool RunState::CanOfferPlantPick() const
{
    for (int seed = 0; seed < SEED_IMITATER; ++seed)
        if (RunPlantImplemented((SeedType)seed) && !HasPlant((SeedType)seed)) return true;
    return false;
}

// @pvz-online: §8.6.1 矛盾对跨屏互斥（2026-10-10 用户拍板：从「同屏去重」扩为「持有即
// 排除」）：同株两条机制互斥（一条把弹/株改型，另一条依赖原弹/原株）——只拦同屏不够，
// 跨屏先后双持会出两种坏局：①雪豆改冰瓜弹后「寒冰贯通」仍按株型给弹标无限穿透（弹型
// 无关）→ 溅射+穿透叠加、远超单条设计强度；②玉米变加农炮后「黄油盛宴」掷点无落点 →
// 静默无效（卷心菜型死挂点）。口径：这两对里任一条已持有（>0 层）→ 另一条从候选整体
// 剔除、本局不再出现（双向；对侧双持的存量旧档两条都被拦，不再往上叠）。双向判 = 这条
// 所属株若在「对侧表」有互斥条且其已持有，返回 true。清单见 docs/06 §8.6.1。
// @pvz-online: exclusions compare actual IDs; a new same-plant buff must not inherit unrelated conflicts.
static bool RunChoiceBlockedByCounterpart(const RunState* theRun, int theId)
{
    const int base = RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT;
    if (theId == RUN_BUFF_COUNT + RUN_UPGRADE_SNOWPEA)
        return theRun->GetBuffCount(base + RUN_BUFF2_FROSTPIERCE) > 0;
    if (theId == base + RUN_BUFF2_FROSTPIERCE)
        return theRun->GetBuffCount(RUN_BUFF_COUNT + RUN_UPGRADE_SNOWPEA) > 0;
    if (theId == RUN_BUFF_COUNT + RUN_UPGRADE_KERNELPULT)
        return theRun->GetBuffCount(base + RUN_BUFF2_BUTTERFEAST) > 0 ||
            theRun->GetBuffCount(base + RUN_BUFF2_BUTTER_SPLASH) > 0 ||
            theRun->GetBuffCount(base + RUN_BUFF2_BUTTER_HEAVY) > 0;
    if (theId == base + RUN_BUFF2_BUTTERFEAST || theId == base + RUN_BUFF2_BUTTER_SPLASH || theId == base + RUN_BUFF2_BUTTER_HEAVY)
        return theRun->GetBuffCount(RUN_BUFF_COUNT + RUN_UPGRADE_KERNELPULT) > 0;
    // @pvz-online: 「Nut Bowling」（2026-10-15）三角互斥：老表「耐砸」/扩展表「荆棘之壁」
    // 把坚果往站桩更硬推，保龄球把坚果变成滚撞击杀——口径相反，两两不可共存。
    if (theId == RUN_BUFF_COUNT + RUN_UPGRADE_WALLNUT || theId == base + RUN_BUFF2_THORNS)
        return theRun->GetBuffCount(base + RUN_BUFF2_NUT_BOWLING) > 0;
    if (theId == base + RUN_BUFF2_NUT_BOWLING)
        return theRun->GetBuffCount(RUN_BUFF_COUNT + RUN_UPGRADE_WALLNUT) > 0 ||
            theRun->GetBuffCount(base + RUN_BUFF2_THORNS) > 0;
    return false;
}

// 抽一屏的三条候选。种子挂上"这一屏是第几次抽"（mPickCounter），所以同一局里
// 两屏不会抽出同一组；runSeed 之外还混了本机随机盐 mPickSalt（用户定案：联机里
// 每个玩家的候选各不相同）。盐在进程内稳定，所以失败重打这一关时抽出来的
// 还是同一组三条——重开不会变成"刷候选"；换进程/换机器才换盐。
void RunState::RollChoices(bool theReroll, int lockedChoice)
{
	const bool locked = theReroll && CanLockChoice(lockedChoice);
	const SeedType lockedPlant = locked && IsPlantPick() ? mPlantChoices[lockedChoice] : SeedType::SEED_NONE;
	const int lockedBuff = locked && !IsPlantPick() ? mBuffChoices[lockedChoice] : RUN_BUFF_CHOICE_NONE;
	const uint64_t lockedPlants = lockedBuff != RUN_BUFF_CHOICE_NONE ? GetRunChoicePlantMask(lockedBuff) : 0;
	// 这一屏的候选从这一刻就算抽好了（LawnApp::UpdateRunPick 的"还没抽才抽"守卫看它）。
	mChoicesRolled = true;
	// 「重摇」阶梯价按屏起算（花园批同日用户令，拍板"每屏重置"）：非重抽 = 这是一屏新的
	// 候选，价格回底价；重抽都在同一屏里，计数照旧累着。
	if (!theReroll) mPaidRerollCount = 0;

	unsigned int aSeed = (unsigned int)mRunSeed
		^ (0x9E3779B9u * (unsigned int)(mLevelIndex + 1))
		^ (0x85EBCA6Bu * (mPickCounter + 1))
		^ (0xC2B2AE3Du * mPickSalt);
	if (IsPlantPick()) aSeed ^= 0x5BF03635u;
	mPickCounter++;

	Sexy::MTRand aRNG(aSeed);

	if (IsPlantPick())
	{
		// 候选 = 全部 48 种常规植物（SEED_PEASHOOTER..SEED_COBCANNON）里、卡池还没有的。
		// 模仿者（SEED_IMITATER）不进候选：它要先指定模仿对象，那道选择在选卡界面里才有。
		bool aOwned[NUM_SEEDS_IN_CHOOSER];
		memset(aOwned, 0, sizeof(aOwned));
		for (size_t i = 0; i < mPool.size(); i++)
		{
			if (mPool[i] >= 0 && mPool[i] < NUM_SEEDS_IN_CHOOSER) aOwned[mPool[i]] = true;
		}

		SeedType aCandidates[NUM_SEEDS_IN_CHOOSER];
		int aWeights[NUM_SEEDS_IN_CHOOSER];				// 同下标抽取权重
		int aCount = 0;
		int aTotalW = 0;
		for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; i++)
		{
			if (i != (int)SeedType::SEED_IMITATER && !aOwned[i] && i != lockedPlant)
			{
				aCandidates[aCount] = (SeedType)i;
				// 花盆/荷叶出率 ×5（2026-10-14 用户令）：权重 5 对其余 1——同一池里这两株的
				// 抽中概率是普通候选的五倍（三格整体按权重无放回抽，见下）。
				aWeights[aCount] = (i == (int)SeedType::SEED_FLOWERPOT || i == (int)SeedType::SEED_LILYPAD) ? 5 : 1;
				aTotalW += aWeights[aCount];
				aCount++;
			}
		}

		// 候选去重批（2026-10-10 同屏排除 + 2026-10-16 用户令跨屏冷却，同 PC 2eb8646）：
		// 「重摇」要刷掉的本屏现三株（theReroll）+ 上一屏露过脸没拿的（mPickCooldownPlants，
		// 含被刷掉的那批）从候选里摘掉；摘到只剩 min(池子, 3) 条为止——池子小时自动放宽，
		// 宁可又见到旧株，也不让三选一屏变秃（哨兵格）。
		{
			SeedType aExclude[RUN_CHOICES * 3];
			int aExcludeCount = 0;
			if (theReroll)
			{
				for (int i = 0; i < RUN_CHOICES; i++)
				{
					if (mPlantChoices[i] != SeedType::SEED_NONE) aExclude[aExcludeCount++] = mPlantChoices[i];
				}
			}
			for (int i = 0; i < mPickCooldownPlantCount; i++)
			{
				aExclude[aExcludeCount++] = (SeedType)mPickCooldownPlants[i];
			}
			int aFloor = std::min(aCount, RUN_CHOICES - (locked ? 1 : 0));
			for (int aE = 0; aE < aExcludeCount && aCount > aFloor; aE++)
			{
				for (int j = 0; j < aCount; j++)
				{
					if (aCandidates[j] == aExclude[aE])
					{
						aTotalW -= aWeights[j];
						aCandidates[j] = aCandidates[aCount - 1];
						aWeights[j] = aWeights[aCount - 1];
						aCount--;
						break;
					}
				}
			}
		}

		// 加权无放回抽三格（与增益屏同一套算法）：
		for (int i = 0; i < RUN_CHOICES; i++)
		{
			if (locked && i == lockedChoice) continue;
			if (aCount <= 0)
			{
				mPlantChoices[i] = SeedType::SEED_NONE;
				continue;
			}
			int aPickW = (int)aRNG.Next((unsigned long)aTotalW);
			int aPick = 0;
			for (int j = 0; j < aCount; j++)
			{
				aPickW -= aWeights[j];
				if (aPickW < 0) { aPick = j; break; }
			}
			mPlantChoices[i] = aCandidates[aPick];
			aTotalW -= aWeights[aPick];
			aCandidates[aPick] = aCandidates[--aCount];		// 抽走的换到队尾，保证三条互不重复
			aWeights[aPick] = aWeights[aCount];
		}

		// 候选去重批的记账（植物屏同款，2026-10-16 用户令）：本屏露过脸的全部（含「重摇」
		// 刷掉的那批；theReroll 时不清零）抽的当下就记，消费时由 NotePlantScreenConsumed
		// 生成下一屏冷却集。
		if (!theReroll) mPickSeenPlantCount = 0;
		for (int i = 0; i < RUN_CHOICES; i++)
		{
			if (mPlantChoices[i] == SeedType::SEED_NONE) continue;
			bool aDup = false;
			for (int j = 0; j < mPickSeenPlantCount; j++)
			{
				if (mPickSeenPlants[j] == (unsigned short)mPlantChoices[i]) { aDup = true; break; }
			}
			if (!aDup && mPickSeenPlantCount < RUN_CHOICES * 2)
			{
				mPickSeenPlants[mPickSeenPlantCount++] = (unsigned short)mPlantChoices[i];
			}
		}
	}
	else
	{
		// 候选去重批：本屏"露过脸"记账从零起（theReroll 时不清——「重摇」之后，
		// 被刷掉的那批也算本屏露过脸，一并计入下一个冷却集）。
		if (!theReroll) mPickSeenBuffCount = 0;
		// 增益：全局 9 条 + 单株升级 + 第二 buff（docs/06 §8.5，同株第二条词条）混池抽
		// 3 条互不重复，全部按权重（§8.7：档位基值 1★12/2★6/3★1，全局条 ×k、经济条 ×e、
		// 1★/2★ 全局条再减半）。单株两表
		// 的只收"卡池里已经有这株"的（设计文档：只对已拥有的植物出；老表花盆/睡莲/墓碑
		// 删条不进池见 RunPlantUpgradeInPool，第二表删条与未落消费端条目见 RunPlantBuff2InPool）。
		// 同名跨屏可以再来（叠层，见 BuffStack）；但叠到 mMaxStacks 的条目不再进候选
		//（0 = 无限，方案 §2.2）——到顶就抽不中你。
		// （2026-10-09 权重批撤保底：旧「第 1 格保底一条全局」随权重落地撤除——保底是
		// 无权重时代防全局被挤成小概率的手段，§8.7 定案后由 k 与真实候选池密度保证，
		// docs/06 §8.4 步骤 3。）
		int aCandidates[RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_PLANT_BUFF2_COUNT];
		int aWeights[RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_PLANT_BUFF2_COUNT];	// 同下标抽取权重（§8.7）
		int aCount = 0;
		int aTotalW = 0;
		for (int i = 0; i < RUN_BUFF_COUNT; i++)
		{
			int aCap = GetRunChoiceMaxStacks(i);
			if (aCap > 0 && GetBuffCount(i) >= aCap) continue;
			if (i == lockedBuff || (GetRunChoicePlantMask(i) & lockedPlants) != 0) continue;
			aCandidates[aCount] = i;
			aWeights[aCount] = GetChoiceWeight(i);
			aTotalW += aWeights[aCount];
			aCount++;
		}
		for (int i = 0; i < RUN_PLANT_UPGRADE_COUNT; i++)
		{
			int aId = RUN_BUFF_COUNT + i;
			if (aId == lockedBuff || (GetRunChoicePlantMask(aId) & lockedPlants) != 0) continue;
			if (!RunPlantUpgradeInPool(GetRunPlantUpgradeDef(i).mPlant)) continue;
			if (!HasPlant(GetRunPlantUpgradeDef(i).mPlant)) continue;
			int aCap = GetRunChoiceMaxStacks(aId);
			if (aCap > 0 && GetBuffCount(aId) >= aCap) continue;
			if (RunChoiceBlockedByCounterpart(this, aId)) continue;
			aCandidates[aCount] = aId;
			aWeights[aCount] = GetChoiceWeight(aId);
			aTotalW += aWeights[aCount];
			aCount++;
		}
		// 第二 buff（同株第二条词条，docs/06 §8.5）：进池开关/删条/未落消费端全在
		// RunPlantBuff2InPool（批 0 全 false，纯结构批）；拥有过滤与封顶过滤同老表。
		for (int i = 0; i < RUN_PLANT_BUFF2_COUNT; i++)
		{
			int aId = RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + i;
			if (aId == lockedBuff || (GetRunChoicePlantMask(aId) & lockedPlants) != 0) continue;
			if (!RunPlantBuff2InPool(i)) continue;
            // @pvz-online: family traits appear once if any permanent pool member is eligible.
            const uint64_t mask = GetRunChoicePlantMask(aId);
            bool owned = false;
            for (SeedType plant : mPool)
                if (plant >= 0 && plant < 64 && (mask & (uint64_t(1) << plant))) { owned = true; break; }
            if (!owned) continue;
			int aCap = GetRunChoiceMaxStacks(aId);
			if (aCap > 0 && GetBuffCount(aId) >= aCap) continue;
			if (RunChoiceBlockedByCounterpart(this, aId)) continue;
			aCandidates[aCount] = aId;
			aWeights[aCount] = GetChoiceWeight(aId);
			aTotalW += aWeights[aCount];
			aCount++;
		}

		// 候选去重批（2026-10-10，用户令「刷掉的 buff 别反复出现」）：「重摇」要刷掉的本屏
		// 现三条（theReroll）+ 上一屏露过脸没拿的（mPickCooldownBuffs，含上一屏被刷掉的那批）
		// 从候选里摘掉；先摘现三条再摘冷却，摘到只剩 min(池子, 3) 条为止——池子小时自动放宽，
		// 宁可又见到重复条目，也不让三选一屏变秃（哨兵格）。
		{
			unsigned short aExclude[RUN_CHOICES * 3];
			int aExcludeCount = 0;
			if (theReroll)
			{
				for (int i = 0; i < RUN_CHOICES; i++)
				{
					if (mBuffChoices[i] != RUN_BUFF_CHOICE_NONE) aExclude[aExcludeCount++] = mBuffChoices[i];
				}
			}
			for (int i = 0; i < mPickCooldownBuffCount; i++)
			{
				aExclude[aExcludeCount++] = mPickCooldownBuffs[i];
			}
			int aFloor = std::min(aCount, RUN_CHOICES - (locked ? 1 : 0));
			for (int aE = 0; aE < aExcludeCount && aCount > aFloor; aE++)
			{
				for (int i = 0; i < aCount; i++)
				{
					if (aCandidates[i] == (int)aExclude[aE])
					{
						aTotalW -= aWeights[i];
						aCandidates[i] = aCandidates[aCount - 1];
						aWeights[i] = aWeights[aCount - 1];
						aCount--;
						break;
					}
				}
			}
		}

		// 防御守卫（方案 §2.4）：无限条目兜底，池子正常恒 ≥ 3 条；真抽干时缺格填哨兵、
		// 一条不剩就把这次欠的增益屏作废——空池进 MTRand::Next(0) 是整数除零，直接崩。
		if (aCount <= 0 && !locked)
		{
			for (int i = 0; i < RUN_CHOICES; i++) mBuffChoices[i] = RUN_BUFF_CHOICE_NONE;
			mPendingBuffPicks = 0;
			PvzpLogLn("[run] the buff pool is dry - the owed buff picks are voided");
			return;
		}

		// 加权无放回抽三格。抽中单株条（老表/第二表都算）后把同株的其余条目从候选
		// 摘除（§8.5/§8.6.1，第二 buff 批 0 落地）：一屏三张里同一株至多出现 1 条。
		// §8.6.1 矛盾对自 2026-10-10（用户拍板）从「同屏去重」扩为「持有即排除」——
		// 跨屏先后也不让双持，由候选构建期的 RunChoiceBlockedByCounterpart 拦；其余
		// 同株老条+新条跨屏不拦（存量层叠各自按 cap 管）。全局条 GetRunChoicePlant 返
		// SEED_NONE，不触发。
		for (int aSlot = 0; aSlot < RUN_CHOICES; aSlot++)
		{
			if (locked && aSlot == lockedChoice) continue;
			if (aCount <= 0)
			{
				mBuffChoices[aSlot] = RUN_BUFF_CHOICE_NONE;
				continue;
			}
			int aDrawTotal=aTotalW;
            bool aFocus=false;
            if(aSlot==0 && HasLineupPreference()) {
                int sum=0;for(int j=0;j<aCount;++j)if((GetRunChoicePlantMask(aCandidates[j]) & mEndlessFocusMask) != 0)sum+=aWeights[j];
                if(sum>0){aDrawTotal=sum;aFocus=true;}
            }
            int aPickW = (int)aRNG.Next((unsigned long)aDrawTotal);
			int aPick = 0;
			for (int i = 0; i < aCount; i++)
			{
				if(aFocus && (GetRunChoicePlantMask(aCandidates[i]) & mEndlessFocusMask) == 0)continue;
				aPickW -= aWeights[i];
				if (aPickW < 0) { aPick = i; break; }
			}
			mBuffChoices[aSlot] = (unsigned short)aCandidates[aPick];
			const uint64_t aChoicePlants = GetRunChoicePlantMask(mBuffChoices[aSlot]);
			aTotalW -= aWeights[aPick];
			aCandidates[aPick] = aCandidates[aCount - 1];
			aWeights[aPick] = aWeights[aCount - 1];
			aCount--;
			if (aChoicePlants != 0)
			{
				for (int i = aCount - 1; i >= 0; i--)
				{
					if ((GetRunChoicePlantMask(aCandidates[i]) & aChoicePlants) != 0)
					{
						aTotalW -= aWeights[i];
						aCandidates[i] = aCandidates[aCount - 1];
						aWeights[i] = aWeights[aCount - 1];
						aCount--;
					}
				}
			}
		}

		// 候选去重批的记账：本屏露过脸的全部（抽的当下就记，含「重摇」历次刷掉的几批；去重合并
		// 防溢）。消费时由 NoteBuffScreenConsumed 减去拿走的，生成下一屏冷却集。
		for (int i = 0; i < RUN_CHOICES; i++)
		{
			if (mBuffChoices[i] == RUN_BUFF_CHOICE_NONE) continue;
			bool aDup = false;
			for (int j = 0; j < mPickSeenBuffCount; j++)
			{
				if (mPickSeenBuffs[j] == mBuffChoices[i]) { aDup = true; break; }
			}
			if (!aDup && mPickSeenBuffCount < RUN_CHOICES * 2)
			{
				mPickSeenBuffs[mPickSeenBuffCount++] = mBuffChoices[i];
			}
		}
	}
}

void RunState::TakePlantChoice(int theIndex)
{
	if (theIndex < 0 || theIndex >= RUN_CHOICES || mPendingPlantPicks <= 0) return;

	SeedType aSeed = mPlantChoices[theIndex];
	if (aSeed != SeedType::SEED_NONE && mPool.size() < (size_t)RUN_POOL_MAX)
	{
		mPool.push_back(aSeed);
		PvzpLogLn("run: plant {} joins the pool ({} seeds)", (int)aSeed, (int)mPool.size());
	}
	mPendingPlantPicks--;
	// 这一屏消费掉了：下一屏（还有欠的话）得重新抽——见 LawnApp::UpdateRunPick 的"还没抽才抽"守卫。
	mChoicesRolled = false;
	NotePlantScreenConsumed(aSeed);	// 去重批（2026-10-16 植物屏同款）：本屏露过脸没拿的进下一屏冷却
	// 刚拿到最后一株没到手的植物：本次欠的植物屏到此为止（再选就没候选了）。
	if (mPendingPlantPicks > 0 && !CanOfferPlantPick()) mPendingPlantPicks = 0;
}

void RunState::TakeBuffChoice(int theIndex)
{
	if (theIndex < 0 || theIndex >= RUN_CHOICES || mPendingBuffPicks <= 0) return;

	unsigned short aId = mBuffChoices[theIndex];
	// 空缺格（防御，方案 §2.4）：屏上按钮本就画成不可点；真点到这里也不欠账、不加层。
	if (aId == RUN_BUFF_CHOICE_NONE) return;
	for (size_t i = 0; i < mBuffs.size(); i++)
	{
		if (mBuffs[i].mId == aId)
		{
			mBuffs[i].mCount++;
			mPendingBuffPicks--;
			mChoicesRolled = false;		// 这一屏消费掉了，下一屏重新抽（同 TakePlantChoice）
			NoteBuffScreenConsumed(aId);	// 去重批：本屏露过脸没拿的进下一屏冷却
			return;
		}
	}

	BuffStack aStack;
	aStack.mId = aId;
	aStack.mCount = 1;
	mBuffs.push_back(aStack);
	mPendingBuffPicks--;
	mChoicesRolled = false;		// 这一屏消费掉了，下一屏重新抽（同 TakePlantChoice）
	NoteBuffScreenConsumed(aId);	// 去重批：本屏露过脸没拿的进下一屏冷却
}

// @pvz-online: 「放弃」的记账（2026-10-03 用户定案）：不落货、只消账。先后与
// IsPlantPick 一致——植物屏没清完时放弃的必是植物屏（UpdateRunPick 一屏一屏地摆）。
void RunState::SkipPendingPick()
{
	if (mPendingPlantPicks > 0)
	{
		mPendingPlantPicks--;
		mChoicesRolled = false;		// 这一屏消费掉了，下一屏重新抽（同 TakePlantChoice）
		NotePlantScreenConsumed(SeedType::SEED_NONE);	// 去重批：放弃 = 本屏露过脸的全部进冷却
		PvzpLogLn("[run] a plant pick was skipped ({} still owed)", mPendingPlantPicks);
	}
	else if (mPendingBuffPicks > 0)
	{
		mPendingBuffPicks--;
		mChoicesRolled = false;		// 这一屏消费掉了，下一屏重新抽（同 TakePlantChoice）
		NoteBuffScreenConsumed(RUN_BUFF_CHOICE_NONE);	// 去重批：放弃 = 本屏露过脸的全部进冷却
		PvzpLogLn("[run] a buff pick was skipped ({} still owed)", mPendingBuffPicks);
	}
}

// @pvz-online: 候选去重批的回执（2026-10-10）：「本屏露过脸的全部」减去拿走的那条 = 下一屏
// 的冷却集。拿走的不进冷却（下一屏还能再来叠层——跨屏叠层是老设计，去重批只治"没拿的
// 反复出现"）；放弃（theTakenId = RUN_BUFF_CHOICE_NONE）则整批进冷却。
void RunState::NoteBuffScreenConsumed(unsigned short theTakenId)
{
	int aCount = 0;
	for (int i = 0; i < mPickSeenBuffCount; i++)
	{
		if (mPickSeenBuffs[i] != theTakenId) mPickCooldownBuffs[aCount++] = mPickSeenBuffs[i];
	}
	mPickCooldownBuffCount = aCount;
}

// @pvz-online: 植物屏同款回执（2026-10-16 用户令，同 PC 2eb8646）：「本屏露过脸的全部」
// 减去拿走的那株 = 下一屏的冷却集。拿走的株已入卡池、候选 owned 过滤天然挡住（冷却里
// 减不减无行为差，照抄增益屏口径）；放弃（theTakenSeed = SEED_NONE）则整批进冷却。
void RunState::NotePlantScreenConsumed(SeedType theTakenSeed)
{
	int aCount = 0;
	for (int i = 0; i < mPickSeenPlantCount; i++)
	{
		if (mPickSeenPlants[i] != (unsigned short)theTakenSeed) mPickCooldownPlants[aCount++] = mPickSeenPlants[i];
	}
	mPickCooldownPlantCount = aCount;
}

// @pvz-online: replaced purple effects have one level, including restored/borrowed legacy stacks.
static int ClampPurpleReplacementStacks(int id, int count)
{
    int base = RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT;
    if (id == RUN_BUFF_COUNT + RUN_UPGRADE_WINTERMELON || id == base + RUN_BUFF2_DEEPFREEZE || id == base + RUN_BUFF2_GOLDTOUCH)
        return std::min(count, 1);
    return count;
}

// @pvz-online: scale all weights equally so a 3-star combination can have a fractional weight.
int RunState::GetChoiceWeight(int theId) const
{
    const int cob = RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_BUFF2_TWINCOB;
    const int conversion = RUN_BUFF_COUNT + RUN_UPGRADE_KERNELPULT;
    int weight = GetRunChoiceWeight(theId) * 16;
    if (theId == cob) return GetBuffCount(conversion) > 0 ? 1 : 4;
    if (theId == conversion && GetBuffCount(cob) > 0) weight /= 4;
    return weight;
}

int RunState::GetBuffCount(int theBuffId) const
{
	for (size_t i = 0; i < mBuffs.size(); i++)
	{
		if (mBuffs[i].mId == (unsigned short)theBuffId) return (int)mBuffs[i].mCount;
	}
	return 0;
}

bool RunState::HasPlant(SeedType theSeedType) const
{
	for (size_t i = 0; i < mPool.size(); i++)
	{
		if (mPool[i] == theSeedType) return true;
	}
	return false;
}

int RunState::LevelForIndex(int theIndex)
{
	// 25 个引擎关号（M4-a 定案，2026-10-03 用户要求整组后移：首关不再用 1-1 教学关）。
	// 编排规则：场景内难度只升不降；首关就取到 10 波关（10 波一旗是现成机制，末波
	// 自带旗帜波，不用改旗机制）。第 50 关（5-10）是僵王 boss 关，绝不能进表；
	// 5-8 只有 20 波，顶不上场景 5 的后段（用它会 30→20 回落），故取 5-2/3/4/7/9。
	static const int aLevels[RUN_LEVEL_COUNT] = {
		4,  6,  7,  9,  10,	// 场景 1 白天：10/10/20/20/20 波
		13, 15, 17, 19, 20,	// 场景 2 夜：  10/10/20/20/20 波
		23, 25, 27, 29, 30,	// 场景 3 泳池：20/20/30/30/30 波
		33, 35, 37, 39, 40,	// 场景 4 雾：  10/20/20/20/20 波
		42, 43, 44, 47, 49,	// 场景 5 屋顶：20/20/30/30/30 波
	};
	if (theIndex < 0 || theIndex >= RUN_LEVEL_COUNT) return -1;
	return aLevels[theIndex];
}

// 时长档的每场景关数（完整 5 / 普通 2 / 快速 1）。模式非法按完整版——检查点、联机包
// 里来的值都过这道闸，越界值永远到不了下面的表。无尽档取完整版值（5），仅作静态表尺寸用。
int RunState::LevelsPerScene(int theRunMode)
{
	static const int aPerScene[] = { 5, 2, 1 };
	if (theRunMode < RUN_MODE_FULL || theRunMode > RUN_MODE_QUICK) return aPerScene[RUN_MODE_FULL];
	return aPerScene[theRunMode];
}

// 无尽档没有"关数"这回事（软上限 RUN_ENDLESS_LEVEL_COUNT，见头文件）；其余档 = 场景数 × 每场景关数。
int RunState::LevelCountForMode(int theRunMode)
{
	if (theRunMode == RUN_MODE_ENDLESS) return RUN_ENDLESS_LEVEL_COUNT;
	return RUN_SCENE_COUNT * LevelsPerScene(theRunMode);
}

// 出怪难度档的千分比表（2026-10-03 用户定案：轻松 ×0.5 / 标准 ×1.0；高压 2026-10-04
// 按玩家反馈由 ×1.5 上调 ×2.0，见 docs/07 批六）。
int RunState::DiffPermilleFor(int theRunDiff)
{
	static const int aPermille[] = { 500, 1000, 2000 };
	if (theRunDiff < RUN_DIFF_EASY || theRunDiff > RUN_DIFF_HIGH) return aPermille[RUN_DIFF_STD];
	return aPermille[theRunDiff];
}

// 高级选项两个倍率档的千分比（批 C，2026-10-09 用户定案）：规模 ×0.5/×1/×2/×4——乘在
// 顺位乘数链之后；节奏 ×0.6/×1/×1.6——只乘波间隔倒计时。档位非法按标准 1000。
int RunState::ScalePermilleFor(int theRunScale)
{
	static const int aPermille[] = { 500, 1000, 2000, 4000 };
	if (theRunScale < RUN_SCALE_HALF || theRunScale > RUN_SCALE_QUAD) return aPermille[RUN_SCALE_STD];
	return aPermille[theRunScale];
}

int RunState::TempoPermilleFor(int theRunTempo)
{
	static const int aPermille[] = { 600, 1000, 1600 };
	if (theRunTempo < RUN_TEMPO_FAST || theRunTempo > RUN_TEMPO_SLOW) return aPermille[RUN_TEMPO_STD];
	return aPermille[theRunTempo];
}

// 按时长档从同一张 25 关表里抽行（批十 2026-10-05 按玩家反馈改版）：普通版每场景取
// 第 1/5 关（10 关，即原快速表）、快速版取第 5 关（5 关，每场景收尾的难关）。
// 抽出来的还是这张表里的引擎关号——波数、出怪、种类名单都按引擎关走，
// 所以 RunLevelIndexForEngineLevel 的完整版反查在三档里都命中。
int RunState::LevelForModeIndex(int theRunMode, int theIndex)
{
	static const int aSubs[RUN_MODE_QUICK + 1][RUN_LEVELS_PER_SCENE] = {
		{ 0, 1, 2, 3, 4 },	// 完整版：全取
		{ 0, 4, 0, 0, 0 },	// 普通版：每场景第 1/5 关（=原快速表）
		{ 4, 0, 0, 0, 0 },	// 快速版：每场景第 5 关（引擎关 10/20/30/40/49）
	};
	if (theRunMode < RUN_MODE_FULL || theRunMode > RUN_MODE_QUICK) theRunMode = RUN_MODE_FULL;
	int aPerScene = LevelsPerScene(theRunMode);
	if (theIndex < 0 || theIndex >= LevelCountForMode(theRunMode)) return -1;
	return LevelForIndex((theIndex / aPerScene) * RUN_LEVELS_PER_SCENE + aSubs[theRunMode][theIndex % aPerScene]);
}

// @pvz-online: 补发追赶期间（R6）：人先站到"要追到的那一关"的草坪上，再在草坪上把
// 欠下的三选一补完（见 UpdateRunPick）——所以正在打的这一关就是目标关，关卡号、
// 波表种子和难度阶梯都得按它算，不然先进草坪的那一下会建错关。
int RunState::GetPlayingLevelIndex() const
{
	return IsCatchingUp() ? mCatchUpLevel : mLevelIndex;
}

// 场景档 0..4。mLevelIndex 会短暂停在"已通关"（== 关数）这种空档上：
// 夹进范围里，别让越界值把难度表读穿。每场景关数按时长档（5/2/1）。
// 无尽档的场景进场就锁死了（建档/线传/读档三处都验过 0..4），不随关序号走。
int RunState::GetSceneIndex() const
{
	if (mMode == RUN_MODE_ENDLESS) return mEndlessScene;
	int aIndex = GetPlayingLevelIndex();
	if (aIndex < 0) aIndex = 0;
	if (aIndex > GetLevelCount() - 1) aIndex = GetLevelCount() - 1;
	return aIndex / LevelsPerScene(mMode);
}

// @pvz-online: 无尽档的难度乘数（2026-10-12 用户定案，整个替换原「场景基准 + 600·ln(1+关序号)」
// 对数阶增量」；血量和出怪点数同走本乘数，场景基准不再叠加）：
//   乘数 = min(1.1^(n-1), (1+ln n)²)，n = 关序号（1 起：aLevel=0 是第 1 关）。
// 前 32 关走指数支 1.1^(n-1)（×1.1/关）；约第 33 关两曲线交叉后走对数支 (1+ln n)²，后期趋缓。
// 全程整数千分比乘除——指数递推 ×1100/1000 逐层截尾，ln 用 t = floor(log2 n)、frac = 小数部分
// 千分比、693·t + 693·frac/1000（ln2 ≈ 0.693）的近似；联机两端逐位一致。
// 指数支涨过 1000000 千分比即停算（此时对数支恒更低，结果无差）。
// 样例（n → 乘数千分比）：1→1000、2→1100、10→2356、20→6105、32→19149、33→20124、100→30769。
static int EndlessDifficultyPermille(int theLevelIndex)
{
	int aN = theLevelIndex + 1;			// 关序号 n（1 起）
	if (aN <= 1) return 1000;			// n=1：两支都是 ×1.0

	// 指数支：1.1^(n-1) 的千分比。
	int aPow = 1000;
	for (int i = 1; i < aN; i++)
	{
		if (aPow >= 1000000) break;
		aPow = aPow * 1100 / 1000;
	}

	// 对数支：(1 + ln n)² 的千分比。
	int aT = 0;
	while ((1u << (aT + 1)) <= (unsigned int)aN) aT++;
	unsigned int aPow2 = 1u << aT;
	int aFrac = (int)((((unsigned int)aN - aPow2) * 1000) / aPow2);
	int aLnMilli = 693 * aT + 693 * aFrac / 1000;
	int aBase = 1000 + aLnMilli;
	int aSq = aBase * aBase / 1000;

	return (aPow < aSq) ? aPow : aSq;
}

// 难度阶梯（M4-a，用户定案）：每过一个场景血量与数量同乘 ×1.33 → 1.0/1.33/1.77/2.35/3.13
//（2026-10-04 按玩家反馈"后期难度不足"由 ×1.2 上调、2026-10-05 回调为 ×1.33，见 docs/07 批六/批九；截尾口径同旧表）。
// 写成整数千分比表：两边全靠整数乘除，逐位一致——浮点乘的 0.000001 之差就可能让同一只
// 僵尸在两台机器上一个剩 1 点血、一个已经死了。
// 无尽档：min(1.1^(n-1), (1+ln n)²) 全量乘数（见 EndlessDifficultyPermille），不再叠场景基准。
int RunState::GetDifficultyPermille() const
{
	if (mMode == RUN_MODE_ENDLESS)
	{
		int aLevel = GetPlayingLevelIndex();
		if (aLevel < 0) aLevel = 0;
		return EndlessDifficultyPermille(aLevel);
	}
	return LadderPermilleFor(mLadder, GetSceneIndex());
}

// 无尽档的关卡映射：锁定场景内的 5 个原型循环——关序号 % 5 取行（场景锁死，不再随序号换景）。
int RunState::GetLevel() const
{
	if (mMode == RUN_MODE_ENDLESS)
	{
		int aIndex = GetPlayingLevelIndex();
		if (aIndex < 0) return -1;
		return LevelForIndex(mEndlessScene * RUN_LEVELS_PER_SCENE + aIndex % RUN_LEVELS_PER_SCENE);
	}
	return LevelForModeIndex(mMode, GetPlayingLevelIndex());
}

int RunState::GetLevelSeed() const
{
	// 由局种子 + 关序号推导：同一局里每关不同、重开同一关（失败重试）完全一样。
	int aIndex = GetPlayingLevelIndex();
	unsigned int aSeed = (unsigned int)mRunSeed ^ (0x9E3779B9u * (unsigned int)(aIndex + 1));
	aSeed ^= aSeed >> 16;
	aSeed *= 0x85EBCA6Bu;
	aSeed ^= aSeed >> 13;
	return (int)aSeed;
}

// 本关失败一次。序号越界只会出现在"已通关 / 空局"这种不该有人报失败的时候，直接不理。
// 无尽档的关序号会一直往上走：计数按 25 取模落格（首版只存不再惩罚，循环叠用无妨）。
void RunState::NoteLevelFailed()
{
	if (mLevelIndex < 0 || mLevelIndex >= GetLevelCount()) return;
	mFailCounts[mLevelIndex % RUN_LEVEL_COUNT]++;
}

std::string RunState::GetCheckpointName(int theProfileId, int theMode, int theScene, uint64_t theTeam)
{
	return GetAppDataFolder() + "userdata/android_run" + std::to_string(theProfileId) +
        (theMode==RUN_MODE_ENDLESS ? "_endless"+std::to_string(theScene) : "_finite") + (theTeam?"_team"+std::to_string(theTeam):"") + ".dat";
}

std::string RunState::CheckpointSummary(int theProfileId, int theMode, int theScene)
{
    RunState saved;
    if(!saved.Load(theProfileId,theMode,theScene))return {};
    std::string text=ModText::Tr("存档：第 ","Saved: level ")+std::to_string(saved.mLevelIndex+1)+ModText::Tr(" 关","");
    if(saved.IsEndless() && !saved.IsNewEndless())text+=ModText::Tr("（旧规则，仅进度）"," (legacy, progress only)");
    const auto path=GetCheckpointName(theProfileId,saved.mMode,saved.mEndlessScene);
    std::tm local{};
#ifdef _WIN32
    struct _stat64 info{};
    if(_stat64(path.c_str(),&info)!=0 || localtime_s(&local,&info.st_mtime)!=0)return text;
#else
    struct stat info{};
    if(stat(path.c_str(),&info)!=0 || !localtime_r(&info.st_mtime,&local))return text;
#endif
    char date[32]{};
    if(std::strftime(date,sizeof(date),"%Y-%m-%d %H:%M",&local))text+=std::string("\n")+ModText::Tr("保存于 ","Saved at ")+date;
    return text;
}

bool RunState::HasCheckpoint(int theProfileId, int theMode, int theScene)
{
    RunState temp; return temp.Load(theProfileId,theMode,theScene);
}

void RunState::DeleteCheckpoint(int theProfileId)
{
	if (gSexyAppBase && RunCheckpointIO::Recover(GetCheckpointName(theProfileId),
        GetAppDataFolder()+"userdata/user"+std::to_string(theProfileId)+".dat"))
        {
            gSexyAppBase->EraseFile(GetCheckpointName(theProfileId));
            gSexyAppBase->EraseFile(GetCheckpointName(theProfileId)+".bak");
        }
}

// @pvz-online: 探读（2026-10-10）：完整做一遍 Load 才回填，失败一律返回 false——
// 调用方（两处「续不续」问句）靠它分辨盘上这份档属于哪一档，别再自己看文件在不在。
bool RunState::PeekCheckpoint(int theProfileId, int& theRunMode, int& theEndlessScene, int theMode, int theScene)
{
	RunState aTemp;
	if (!aTemp.Load(theProfileId,theMode,theScene)) return false;
	theRunMode = aTemp.mMode;
	theEndlessScene = aTemp.mEndlessScene;
	return true;
}

std::vector<unsigned char> RunState::EncodeCheckpoint() const
{
	std::vector<unsigned char> aData;
	AppendI32(aData, (int)RUN_CHECKPOINT_MAGIC);
	AppendU16(aData, RUN_CHECKPOINT_VERSION);
	AppendU16(aData, (unsigned int)((mMode & 0xFF) | ((mDiff & 0xFF) << 8)));	// 低字节 = 时长档，高字节 = 出怪难度档
	AppendI32(aData, mRunSeed);
	AppendI32(aData, mLevelIndex);
	for (int i = 0; i < RUN_LEVEL_COUNT; i++)
	{
		AppendU16(aData, (unsigned int)mFailCounts[i]);
	}

	AppendU16(aData, (unsigned int)mPool.size());
	for (size_t i = 0; i < mPool.size(); i++)
	{
		AppendU16(aData, (unsigned int)mPool[i]);
	}

	AppendU16(aData, (unsigned int)mBuffs.size());
	for (size_t i = 0; i < mBuffs.size(); i++)
	{
		AppendU16(aData, mBuffs[i].mId);
		AppendU16(aData, mBuffs[i].mCount);
	}

	AppendU16(aData, mMowerUsedRows);	// v5：末位推车记账（bit = 行号）
	// v10：高级选项位包（bit5..6 = 增益倍选，2026-10-13 启用——老版本读 0 = ×1，安全降级，
	// 不抬版本；消费端 BuffMultValueFor 钳非法档）。v14 启用 bit7..9 = 场景难度阶梯档
	// （老档读 0 = 现状标准档）、bit10..11 = 僵尸 buff 档（老档读 0 = 不开）；bit12..15 仍空闲。
	AppendU16(aData, (unsigned int)((mScale & 3) | ((mTempo & 3) << 2) | ((mZombotany & 1) << 4) | ((mBuffMult & 3) << 5)
		| ((mLadder & 7) << 7) | ((mZombieBuffMode & 3) << 10) | ((mSpeedTier & 3) << 12) | ((mFirstWave & 3) << 14)));
	// v11（第二 buff 批 0）：buff 列表格式不变（BuffStack 通用 id+count），但 mBuffs 里
	// 从此可能出现第二表 id（57..95）。老版本读到会当老单株解出错误条目，故抬版本号让
	// 老构建直接拒档；v10 及更老档没有这类 id，读平即可，不需要迁移链。
	AppendU16(aData, (unsigned int)mEndlessScene);	// v12：无尽档锁定的场景（非无尽档恒 0）
	AppendU16(aData, (unsigned int)mBossFlag);	// v13：关底巨型 boss 开关（无尽档恒 0）
	// v18：僵尸强化——9 条层数（u16×9，值域 0..255）与"上一票投在第几关"（0xFFFF = 没投过）。
	for (int i = 0; i < RUN_ZOMBIE_BUFF_COUNT; i++)
	{
		AppendU16(aData, (unsigned int)mZombieBuffStacks[i]);
	}
	AppendU16(aData, (unsigned int)mZombieVotedLevel);
	AppendU16(aData, (unsigned int)mRerollVouchers);
	AppendI32(aData, mRerollAwardedLevel);
    AppendI32(aData,mLoanPlant);AppendI32(aData,mLoanLevel);AppendI32(aData,mLentLevel);
    AppendU16(aData,(unsigned)mLoanBuffs.size());
    for(const auto& buff:mLoanBuffs){AppendU16(aData,buff.mId);AppendU16(aData,buff.mCount);}

    // @pvz-online: stage snapshots must preserve owed rewards, prices and candidate exclusion sets.
    AppendI32(aData, mPendingPlantPicks); AppendI32(aData, mPendingBuffPicks);
    for (int i=0;i<RUN_CHOICES;++i) { AppendI32(aData, (int)mPlantChoices[i]); AppendU16(aData, mBuffChoices[i]); }
    AppendI32(aData, (int)mPickCounter); AppendI32(aData, (int)mPickSalt);
    AppendU16(aData, mChoicesRolled ? 1 : 0); AppendI32(aData, mPaidRerollCount); AppendI32(aData, mCatchUpLevel);
    auto writeSet=[&](const unsigned short* values,int count) {
        AppendU16(aData,(unsigned)count); for(int i=0;i<count;++i)AppendU16(aData,values[i]);
    };
    writeSet(mPickSeenBuffs,mPickSeenBuffCount); writeSet(mPickCooldownBuffs,mPickCooldownBuffCount);
    writeSet(mPickSeenPlants,mPickSeenPlantCount); writeSet(mPickCooldownPlants,mPickCooldownPlantCount);
    AppendU16(aData,mEndlessOptions);
    AppendI32(aData,(int)mEndlessFocusMask);AppendI32(aData,(int)(mEndlessFocusMask>>32));
    AppendI32(aData,mEndlessSnapshotLevel);AppendU16(aData,mEndlessSnapshotPhase);
    for(const auto* bytes : {&mEndlessLawn,&mEndlessRetryLawn}) {
        AppendI32(aData,(int)bytes->size());aData.insert(aData.end(),bytes->begin(),bytes->end());
        AppendI32(aData,(int)EndlessRules::Hash(bytes->data(),bytes->size()));
    }

    AppendU16(aData,mEndlessVoteRounds);AppendU16(aData,mEndlessVoteIds[0]);AppendU16(aData,mEndlessVoteIds[1]);
    AppendI32(aData,static_cast<int>(mDamageHistory.size()));
    aData.insert(aData.end(),mDamageHistory.begin(),mDamageHistory.end());
    AppendI32(aData,static_cast<int>(EndlessRules::Hash(mDamageHistory.data(),mDamageHistory.size())));
    const auto admission=Admission::Encode(mAdmission);
    AppendI32(aData,static_cast<int>(admission.size()));aData.insert(aData.end(),admission.begin(),admission.end());
    AppendI32(aData,static_cast<int>(EndlessRules::Hash(admission.data(),admission.size())));
    AppendU16(aData, static_cast<unsigned int>(mCrowdRule)); // @pvz-online: v22, validated on load.
    const auto team=TeamProgression::Encode(mTeam);
    AppendI32(aData,static_cast<int>(team.size()));aData.insert(aData.end(),team.begin(),team.end());
    AppendI32(aData,static_cast<int>(EndlessRules::Hash(team.data(),team.size())));
    return aData;
}

bool RunState::Save(int theProfileId) const
{
    const auto previousHistory = mDamageHistory;
    if(gLawnApp && gLawnApp->GetRunState()==this) { gLawnApp->mDamageStats.Flush(); gLawnApp->mDamageStats.CaptureCheckpoint(*this); }
    if(gLawnApp && gLawnApp->GetRunState()==this && !EndlessLawn::Capture(gLawnApp->mBoard)) { mDamageHistory = previousHistory; return false; }
    std::vector<unsigned char> previousTeamLawn;bool capturedTeam=false;
    if(gLawnApp && gLawnApp->GetRunState()==this && !TeamPlay::PrepareCheckpoint(gLawnApp,previousTeamLawn,capturedTeam)) { mDamageHistory=previousHistory;return false; }
    MkDir(GetAppDataFolder() + "userdata");
    const auto path=GetCheckpointName(theProfileId,mMode,mEndlessScene,mAdmission.phase!=Admission::NONE?mAdmission.run:0);
    const auto user=GetAppDataFolder()+"userdata/user"+std::to_string(theProfileId)+".dat";
    const bool saved = RunCheckpointIO::Recover(path,user) && RunCheckpointIO::AtomicWrite(path,EncodeCheckpoint());
    if (!saved) { mDamageHistory = previousHistory; if(capturedTeam)gLawnApp->GetRunState()->mTeam.lawn.swap(previousTeamLawn); }
    if(saved && gLawnApp && gLawnApp->GetRunState()==this)RunHistory::Capture(gLawnApp);
    return saved;
}
bool RunState::SaveWithPlayer(PlayerInfo& player) const
{
    const auto previousHistory = mDamageHistory;
    if(gLawnApp && gLawnApp->GetRunState()==this) { gLawnApp->mDamageStats.Flush(); gLawnApp->mDamageStats.CaptureCheckpoint(*this); }
    if(gLawnApp && gLawnApp->GetRunState()==this && !EndlessLawn::Capture(gLawnApp->mBoard)) { mDamageHistory = previousHistory; return false; }
    std::vector<unsigned char> previousTeamLawn;bool capturedTeam=false;
    if(gLawnApp && gLawnApp->GetRunState()==this && !TeamPlay::PrepareCheckpoint(gLawnApp,previousTeamLawn,capturedTeam)) { mDamageHistory=previousHistory;return false; }
    MkDir(GetAppDataFolder() + "userdata");
    const bool saved = RunCheckpointIO::Commit(GetCheckpointName(player.mId,mMode,mEndlessScene,mAdmission.phase!=Admission::NONE?mAdmission.run:0),
        GetAppDataFolder()+"userdata/user"+std::to_string(player.mId)+".dat",EncodeCheckpoint(),player.EncodeDetails());
    if (!saved) { mDamageHistory = previousHistory; if(capturedTeam)gLawnApp->GetRunState()->mTeam.lawn.swap(previousTeamLawn); }
    if(saved && gLawnApp && gLawnApp->GetRunState()==this)RunHistory::Capture(gLawnApp);
    return saved;
}

bool RunState::Load(int theProfileId, int theMode, int theScene, bool theBackup, uint64_t theTeam)
{
    std::string aLoadPath=GetCheckpointName(theProfileId,theMode,theScene,theTeam)+(theBackup?".bak":"");
    if (!gSexyAppBase) return false;
    if(!theTeam && !theBackup && !gSexyAppBase->FileExists(aLoadPath) && !gSexyAppBase->FileExists(aLoadPath+".txn"))
        {
            aLoadPath=GetAppDataFolder()+"userdata/android_run"+std::to_string(theProfileId)+".dat";
            if(gSexyAppBase->FileExists(aLoadPath+".migrated"))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
        }

    if (!RunCheckpointIO::Recover(aLoadPath,
        GetAppDataFolder()+"userdata/user"+std::to_string(theProfileId)+".dat")) return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	Buffer aBuffer;
	if (!gSexyAppBase->ReadBufferFromFile(aLoadPath, &aBuffer, false))
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}

	RunReader aReader((const unsigned char*)aBuffer.GetDataPtr(), aBuffer.GetDataLen());
	unsigned int aMagic = 0, aVersion = 0, aReserved = 0;
	if (!aReader.ReadU32(aMagic) || !aReader.ReadU16(aVersion) || !aReader.ReadU16(aReserved))
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}
	// v3 才有时长档、v4 才有难度档、v5 才有推车记账（v6 与 v5 的保留位含义相同）。老版本
	// 占的保留位恒 0，恰好是各自默认档——v2 的档按完整版续、v3 的档按标准难度续，都不作废；
	// v5 及更老的档没有推车字段，按"一辆都没用"续。再往前的版本一律当"没有检查点"。
	// v7 起模式表改版（见文件头 v7 条目）：6 及更老的非完整档关序号对不上新表，读完模式
	// 字段后直接作废；完整档表没动，照续。
	// v8 起删了「储备」（见文件头 v8 条目）：旧档 buff id 按下面 buff 循环里的链迁移。
	// v10 才有高级选项位包；v9 及更老的档读不到，按 标准/标准/关 续。
	// v11 起buff 列表可能有第二表 id（57..95，第二 buff 批 0）；v10 及更老档天然没有，
	// buff 循环与迁移链照旧（迁移条件全是 aVersion < N，对新 id 不触发）。
	// v12 起才有无尽档与场景字段：v11 及更老档读到 mode==ENDLESS 一律拒档（伪造/错位）；
	// 非无尽档的场景恒 0。
	// v13 才有关底 boss 开关；v14 才有僵尸强化字段（位包 bit7..11 与尾追加 8×u16+u16）。
	int aMode = RUN_MODE_FULL;
	int aDiff = RUN_DIFF_STD;
	if (aMagic != RUN_CHECKPOINT_MAGIC || (aVersion < 2 || aVersion > RUN_CHECKPOINT_VERSION))
	{
		PvzpLogLn("[run] checkpoint magic/version mismatch, ignored");
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}
	if (aVersion >= 3)
	{
		aMode = (int)(aReserved & 0xFF);
		if (aMode < RUN_MODE_FULL || aMode > RUN_MODE_ENDLESS)
		{
			return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		}
		if (aMode == RUN_MODE_ENDLESS && aVersion < 12)
		{
			PvzpLogLn("[run] pre-v12 checkpoint claims the endless mode, ignored");
			return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		}
		if (aVersion < 7 && aMode != RUN_MODE_FULL)
		{
			PvzpLogLn("[run] pre-v7 checkpoint with the old mode table, ignored");
			return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		}
	}
	if (aVersion >= 4)
	{
		aDiff = (int)((aReserved >> 8) & 0xFF);
		if (aDiff < RUN_DIFF_EASY || aDiff > RUN_DIFF_HIGH)
		{
			return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		}
	}

	int aRunSeed = 0, aLevelIndex = 0;
	if (!aReader.ReadI32(aRunSeed) || !aReader.ReadI32(aLevelIndex))
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}
	if (aLevelIndex < 0 || aLevelIndex > LevelCountForMode(aMode))
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}

	int aFailCounts[RUN_LEVEL_COUNT] = { 0 };
	for (int i = 0; i < RUN_LEVEL_COUNT; i++)
	{
		unsigned int aCount = 0;
		if (!aReader.ReadU16(aCount)) return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		aFailCounts[i] = (int)aCount;
	}

	unsigned int aPoolCount = 0;
	if (!aReader.ReadU16(aPoolCount) || aPoolCount > RUN_POOL_MAX)
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}
	std::vector<SeedType> aPool;
	for (unsigned int i = 0; i < aPoolCount; i++)
	{
		unsigned int aSeed = 0;
		if (!aReader.ReadU16(aSeed) || aSeed >= (unsigned int)SeedType::NUM_SEED_TYPES)
		{
			return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		}
		aPool.push_back((SeedType)aSeed);
	}

	unsigned int aBuffCount = 0;
	if (!aReader.ReadU16(aBuffCount) || aBuffCount > RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_PLANT_BUFF2_COUNT)
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}
	std::vector<BuffStack> aBuffs;
	for (unsigned int i = 0; i < aBuffCount; i++)
	{
		unsigned int aId = 0, aCount = 0;
		if (!aReader.ReadU16(aId) || !aReader.ReadU16(aCount))
		{
			return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		}
		BuffStack aStack;
		aStack.mId = (unsigned short)aId;
		aStack.mCount = (unsigned short)aCount;
		// @pvz-online: 批 18——Precision 全局插在表尾（新 id 8），单株 id 由「8 + 下标」全体
		// 右移 1 变「9 + 下标」；v5 及更老的档里 id >= 8 的其实都是单株，读进来先 +1 读平。
		// 全局 0..7 不动；旧档的 id 8 是"第一个单株"，不是 Precision，不会撞车。
		if (aVersion < 6 && aStack.mId >= 8)
		{
			aStack.mId = (unsigned short)(aStack.mId + 1);
		}
		// @pvz-online: v8——「储备」（原 id 5）删除：全局 9→8 条，单株 id 由「9 + 下标」全体
		// 左移 1 变「8 + 下标」。链在 v6 的 +1 之后（v5 档：id 8 →+1= 9 →−1= 8 回位）。
		// id 5 各代次都是储备本体，丢弃不读；id >= 6 一律 −1（含 8→7 Precision、9..56 → 8..55）。
		if (aVersion < 8)
		{
			if (aStack.mId == 5)
			{
				continue;
			}
			if (aStack.mId >= 6)
			{
				aStack.mId = (unsigned short)(aStack.mId - 1);
			}
		}
		// @pvz-online: v9——全局表尾追加「排山倒海」（新 id 8）：单株 id 由「8 + 下标」全体
		// 右移 1 变「9 + 下标」。链在 v6/v8 之后（v5 档：8 →+1= 9 →−1= 8 →+1= 9 回位）。
		// 旧档的 id 8 是"第一个单株"，不是排山倒海，不会撞车。
		if (aVersion < 9 && aStack.mId >= 8)
		{
			aStack.mId = (unsigned short)(aStack.mId + 1);
		}
		// @pvz-online: old mine entries become the replacement choices; cap1 prevents misleading legacy stacks.
		if (aStack.mId >= RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_PLANT_BUFF2_COUNT || aStack.mCount == 0)
			return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		if (aStack.mId == RUN_BUFF_COUNT + RUN_UPGRADE_POTATOMINE ||
			aStack.mId == RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + RUN_BUFF2_MINEREGROW)
			aStack.mCount = 1;
        aStack.mCount = (unsigned short)ClampPurpleReplacementStacks(aStack.mId, aStack.mCount);
		for (const BuffStack& aPrevious : aBuffs) if (aPrevious.mId == aStack.mId) return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		aBuffs.push_back(aStack);
	}

	// v5 才有推车记账；v4 及更老的档读不到，按"一辆都没用"续。
	unsigned int aMowerUsedRows = 0;
	if (aVersion >= 5 && !aReader.ReadU16(aMowerUsedRows))
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}

	// v10 才有高级选项位包；v9 及更老的档读不到，按 标准/标准/关 续。位掩码天然合法
	//（bit7..11 = 阶梯档/僵尸 buff 档，v14 启用；老档恒 0 = 现状/不开）。
	unsigned int aOptions = 0;
	if (aVersion >= 10 && !aReader.ReadU16(aOptions))
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}

	// v12 才有场景字段。无尽档的场景必须真落在 0..4——GetLevel 的取模救不了坏场景
	//（坏场景会取出越界引擎关号），只能在这里拒档；非无尽档的场景恒 0。
	unsigned int aEndlessScene = 0;
	if (aVersion >= 12 && !aReader.ReadU16(aEndlessScene))
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}
	if (aMode == RUN_MODE_ENDLESS)
	{
		if (aEndlessScene >= (unsigned int)RUN_SCENE_COUNT)
		{
			PvzpLogLn("[run] endless checkpoint with a bad scene, ignored");
			return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		}
	}
	else
	{
		aEndlessScene = 0;
	}

	// v13 才有关底 boss 开关；v12 及更老的档读不到，按关。旧值不校验语义（0/1），
	// 无尽档读进来也按 0（与 StartNew 同口径）。
	unsigned int aBossFlag = 0;
	if (aVersion >= 13 && !aReader.ReadU16(aBossFlag))
	{
		return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	}
	if (aMode == RUN_MODE_ENDLESS) aBossFlag = 0;

	// v14 才有僵尸强化字段：8 条层数（u16×8，正常不过 255——读到更大的当伪造/错位，
	// 钳到 255，饱和口径与运行时一致）+ "上一票投在第几关"（0xFFFF = 没投过）。
	// v13 及更老的档读不到，按"没强过、没投过"续。
	unsigned char aZombieStacks[RUN_ZOMBIE_BUFF_COUNT] = { 0 };
	unsigned int aZombieVotedLevel = RUN_ZOMBIE_VOTED_NONE;
	if (aVersion >= 14)
	{
		const int aSavedCount = aVersion >= 24 ? RUN_ZOMBIE_BUFF_COUNT : aVersion >= 23 ? RunZombieBuffRules::PRE_SPECIES_COUNT : aVersion >= 18 ? RunZombieBuffRules::PRE_TAX_COUNT : RunZombieBuffRules::LEGACY_COUNT;
		for (int i = 0; i < aSavedCount; i++)
		{
			unsigned int aStack = 0;
			if (!aReader.ReadU16(aStack))
			{
				return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
			}
			if (aStack > 255) aStack = 255;
			aZombieStacks[i] = (unsigned char)aStack;
		}
		if (!aReader.ReadU16(aZombieVotedLevel))
		{
			return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
		}
	}

    // @pvz-online: old full immunity becomes both capped effects without losing run progress.
    if (aVersion < 18 && aZombieStacks[ZOMBIE_BUFF_STEADFAST] > 0)
        aZombieStacks[ZOMBIE_BUFF_SOFTRESIST] = 1;
    aZombieStacks[ZOMBIE_BUFF_STEADFAST] = aZombieStacks[ZOMBIE_BUFF_STEADFAST] ? 1 : 0;
    aZombieStacks[ZOMBIE_BUFF_SOFTRESIST] = aZombieStacks[ZOMBIE_BUFF_SOFTRESIST] ? 1 : 0;

	unsigned int aVouchers = 0;
	int aAwardedLevel = -1;
	if (aVersion >= 15 && (!aReader.ReadU16(aVouchers) || aVouchers > 2 ||
		!aReader.ReadI32(aAwardedLevel) || aAwardedLevel < -1 || aAwardedLevel > aLevelIndex)) return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
	mRerollVouchers = (int)aVouchers;
	mRerollAwardedLevel = aAwardedLevel;

    mLoanPlant=mLoanLevel=mLentLevel=-1; mLoanBuffs.clear();
    if(aVersion>=16) {
        unsigned count=0;
        if(!aReader.ReadI32(mLoanPlant)||!aReader.ReadI32(mLoanLevel)||!aReader.ReadI32(mLentLevel)||
            !aReader.ReadU16(count)||count>16||mLoanPlant < -1 || mLoanPlant>=48 ||
            mLoanLevel < -1 || mLoanLevel>aLevelIndex || mLentLevel < -1 || mLentLevel>aLevelIndex)return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
        for(unsigned i=0;i<count;++i){unsigned id=0,stacks=0;if(!aReader.ReadU16(id)||!aReader.ReadU16(stacks)||!stacks)return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
            if(id<RUN_BUFF_COUNT || id>=RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT+RUN_PLANT_BUFF2_COUNT || GetRunChoicePlant((int)id)!=(SeedType)mLoanPlant)return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
            for(const auto& b:mLoanBuffs)if(b.mId==id)return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
            BuffStack b;b.mId=(unsigned short)id;b.mCount=(unsigned short)ClampPurpleReplacementStacks((int)id,(int)stacks);mLoanBuffs.push_back(b);}
        if(mLoanPlant<0 && (count || mLoanLevel>=0))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
    }
    RunState picks;
    if(aVersion>=17) {
        int seed=0; unsigned value=0;
        if(!aReader.ReadI32(picks.mPendingPlantPicks)||!aReader.ReadI32(picks.mPendingBuffPicks)||
            picks.mPendingPlantPicks<0||picks.mPendingPlantPicks>10000||picks.mPendingBuffPicks<0||picks.mPendingBuffPicks>400000)return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
        for(int i=0;i<RUN_CHOICES;++i) {
            if(!aReader.ReadI32(seed)||seed<(int)SEED_NONE||seed>=RUN_POOL_MAX||!aReader.ReadU16(value)||
                (value!=RUN_BUFF_CHOICE_NONE&&value>=RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT+RUN_PLANT_BUFF2_COUNT))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
            picks.mPlantChoices[i]=(SeedType)seed; picks.mBuffChoices[i]=(unsigned short)value;
        }
        if(!aReader.ReadU32(picks.mPickCounter)||!aReader.ReadU32(picks.mPickSalt)||!aReader.ReadU16(value)||value>1)return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
        picks.mChoicesRolled=value!=0;
        if(!aReader.ReadI32(picks.mPaidRerollCount)||picks.mPaidRerollCount<0||picks.mPaidRerollCount>1000000||
            !aReader.ReadI32(picks.mCatchUpLevel)||picks.mCatchUpLevel < -1||picks.mCatchUpLevel>LevelCountForMode(aMode))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
        auto readSet=[&](unsigned short* values,int& count,unsigned limit) {
            unsigned n=0,id=0;if(!aReader.ReadU16(n)||n>RUN_CHOICES*2)return false;
            count=(int)n;for(unsigned i=0;i<n;++i){if(!aReader.ReadU16(id)||id>=limit)return false;values[i]=(unsigned short)id;}return true;
        };
        const unsigned buffLimit=RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT+RUN_PLANT_BUFF2_COUNT;
        if(!readSet(picks.mPickSeenBuffs,picks.mPickSeenBuffCount,buffLimit)||
            !readSet(picks.mPickCooldownBuffs,picks.mPickCooldownBuffCount,buffLimit)||
            !readSet(picks.mPickSeenPlants,picks.mPickSeenPlantCount,RUN_POOL_MAX)||
            !readSet(picks.mPickCooldownPlants,picks.mPickCooldownPlantCount,RUN_POOL_MAX))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
    }

    mEndlessOptions=0;mEndlessFocusMask=0;mEndlessSnapshotLevel=-1;mEndlessSnapshotPhase=0;
    mEndlessLawn.clear();mEndlessRetryLawn.clear();
    mEndlessVoteRounds=0;mEndlessVoteIds[0]=mEndlessVoteIds[1]=255;
    if(aVersion>=19) {
        unsigned opt=0,lo=0,hi=0,phase=0;
        if(!aReader.ReadU16(opt)||!EndlessRules::Valid((int)opt)||
            !aReader.ReadU32(lo)||!aReader.ReadU32(hi)||(hi&0xffff0000u)||
            !aReader.ReadI32(mEndlessSnapshotLevel)||mEndlessSnapshotLevel < -1||mEndlessSnapshotLevel>aLevelIndex||
            !aReader.ReadU16(phase)||phase>5||!aReader.ReadBlob(mEndlessLawn)||!aReader.ReadBlob(mEndlessRetryLawn))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
        mEndlessOptions=(int)opt;mEndlessFocusMask=(uint64_t(hi)<<32)|lo;mEndlessSnapshotPhase=(int)phase;
        unsigned rounds=0,first=0,second=0;
        if(!aReader.ReadU16(rounds)||rounds>2||!aReader.ReadU16(first)||!aReader.ReadU16(second)||
            (first!=255&&first>=RUN_ZOMBIE_BUFF_COUNT)||(second!=255&&second>=RUN_ZOMBIE_BUFF_COUNT))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
        mEndlessVoteRounds=(int)rounds;mEndlessVoteIds[0]=(unsigned char)first;mEndlessVoteIds[1]=(unsigned char)second;
    }
    mDamageHistory.clear();
    if(aVersion>=21) { if(!aReader.ReadBlob(mDamageHistory))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam); }
    else if(aVersion>=20) aReader.ReadStats(mDamageHistory);
    mAdmission=Admission::State{};
    if(aVersion>=21) {std::vector<unsigned char> bytes;if(!aReader.ReadBlob(bytes)||!Admission::Decode(bytes,mAdmission))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);}
    // @pvz-online: constructor/StartNew defaults cannot silently upgrade an older saved run.
    unsigned int crowdRule = CrowdRules::LEGACY;
    if (aVersion >= 22 && (!aReader.ReadU16(crowdRule) || !CrowdRules::Valid(crowdRule)))
        return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
    mTeam=TeamProgression::State{};
    if(aVersion>=25) {std::vector<unsigned char> team;if(!aReader.ReadBlob(team)||!TeamProgression::Decode(team,mTeam))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);}
    // @pvz-online 2026-10-26：一张票不跨进程。MOD98 的单人误开票把 open 票写进了存档，
    // 以后再进团队房续档时，那张没有原会话时钟与名册的旧票会被当成真票结算（单人局凭空
    // 得增益、跨关跨模式串流）。读档一律关票——已得的层数原样保留，下一次准备按现役
    // 名册重新开票。
    if(mTeam.shared.ballot.open)mTeam.shared.ballot.open=false;
    if(theTeam && (mAdmission.run!=theTeam || mAdmission.phase==Admission::NONE))return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);
    if(theMode==RUN_MODE_ENDLESS ? (aMode!=RUN_MODE_ENDLESS || (int)aEndlessScene!=theScene) : aMode==RUN_MODE_ENDLESS)return !theBackup && Load(theProfileId,theMode,theScene,true,theTeam);

    mCrowdRule = static_cast<int>(crowdRule);
	mRunSeed = aRunSeed;
	mMode = aMode;
	mDiff = aDiff;
	mEndlessScene = (int)aEndlessScene;
	mScale = (int)(aOptions & 3);
	mTempo = (int)((aOptions >> 2) & 3);
	mZombotany = (int)((aOptions >> 4) & 1);
	// 增益倍选（2026-10-13，位包 bit5..6）：v13 及更老的档该两位恒 0 = ×1；读到非法档（3）
	// 也不拒档——BuffMultValueFor 消费时按 ×1 处理（与其他档同一套纪律）。
	mBuffMult = (int)((aOptions >> 5) & 3);
	// 场景难度阶梯（2026-10-14，位包 bit7..9）：v13 及更老的档恒 0 = 现状标准档（不静默
	// 变难度）；读到非法档（5..7）也不拒档——LadderPermilleFor 消费时按 STD 处理。
	mLadder = (int)((aOptions >> 7) & 7);
    mSpeedTier=(int)((aOptions>>12)&3);
	mFirstWave = (int)((aOptions >> 14) & 3);
	if (mFirstWave > RUN_FIRSTWAVE_LONG) mFirstWave = RUN_FIRSTWAVE_STD;
	// 僵尸 buff 档（2026-10-14，位包 bit10..11）：v13 及更老的档恒 0 = 不开；非法档（3）
	// 消费端 GetZombieBuffPerScene 按 0 处理。
	mZombieBuffMode = (int)((aOptions >> 10) & 3);
	memcpy(mZombieBuffStacks, aZombieStacks, sizeof(mZombieBuffStacks));
	mZombieVotedLevel = (unsigned short)aZombieVotedLevel;
	mBossFlag = (int)aBossFlag;
	mLevelIndex = aLevelIndex;
	memcpy(mFailCounts, aFailCounts, sizeof(mFailCounts));
	mMowerUsedRows = aMowerUsedRows;
	mPool = aPool;
	// 无尽档读档补齐全植物（2026-10-11 开局全植物批）：老档是「两株起家」年代存的，
	// 续档时一次补齐到与开新局同一口径（非无尽档不追溯，池子原样）。
	if (mMode == RUN_MODE_ENDLESS) FillPoolWithAllPlants(mPool);
	mBuffs = aBuffs;
    mPendingPlantPicks=picks.mPendingPlantPicks; mPendingBuffPicks=picks.mPendingBuffPicks;
    std::memcpy(mPlantChoices,picks.mPlantChoices,sizeof(mPlantChoices));
    std::memcpy(mBuffChoices,picks.mBuffChoices,sizeof(mBuffChoices));
    mPickCounter=picks.mPickCounter; mPickSalt=picks.mPickSalt; mChoicesRolled=picks.mChoicesRolled;
    mPaidRerollCount=picks.mPaidRerollCount; mCatchUpLevel=picks.mCatchUpLevel;
    auto copySet=[](unsigned short* dst,int& count,const unsigned short* src,int n) {
        count=n;std::memcpy(dst,src,n*sizeof(*dst));
    };
    copySet(mPickSeenBuffs,mPickSeenBuffCount,picks.mPickSeenBuffs,picks.mPickSeenBuffCount);
    copySet(mPickCooldownBuffs,mPickCooldownBuffCount,picks.mPickCooldownBuffs,picks.mPickCooldownBuffCount);
    copySet(mPickSeenPlants,mPickSeenPlantCount,picks.mPickSeenPlants,picks.mPickSeenPlantCount);
    copySet(mPickCooldownPlants,mPickCooldownPlantCount,picks.mPickCooldownPlants,picks.mPickCooldownPlantCount);

    const std::string legacy=GetAppDataFolder()+"userdata/android_run"+std::to_string(theProfileId)+".dat";
    if(aLoadPath==legacy) {
        const auto* raw=(const unsigned char*)aBuffer.GetDataPtr();
        std::vector<unsigned char> backup(raw,raw+aBuffer.GetDataLen());
        if(!RunCheckpointIO::AtomicWrite(legacy+".legacy-backup",backup) ||
            !RunCheckpointIO::AtomicWrite(GetCheckpointName(theProfileId,mMode,mEndlessScene),EncodeCheckpoint()) ||
            !RunCheckpointIO::AtomicWrite(legacy+".migrated",std::vector<unsigned char>{1}))return false;
    }

    if(theBackup) {
        // Quarantine the damaged head before publishing the verified backup, preserving .bak.
        std::string head=GetCheckpointName(theProfileId,theMode,theScene,theTeam);
        if(gSexyAppBase->FileExists(head)) {
#ifdef _WIN32
            if(!MoveFileExA(head.c_str(),(head+".rejected").c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))return false;
#else
            if(std::rename(head.c_str(),(head+".rejected").c_str())!=0)return false;
#endif
        }
        const auto* raw=(const unsigned char*)aBuffer.GetDataPtr();
        if(!RunCheckpointIO::AtomicWrite(head,std::vector<unsigned char>(raw,raw+aBuffer.GetDataLen())))return false;
    }

	return true;
}

void RunState::PaidReroll(int lockedChoice)
{
	RollChoices(true, lockedChoice);
	mPaidRerollCount++;
	PvzpLogLn("[run] the choices were rerolled for coins: #{} this screen, next price {} (pick counter {})", mPaidRerollCount, GetPaidRerollCost(), mPickCounter);
}

int RunState::GetPaidRerollCost() const
{
	int aCost = RUN_PAID_REROLL_COST;
	int aSteps = (mPaidRerollCount > 7) ? 7 : mPaidRerollCount;
	for (int i = 0; i < aSteps; i++) aCost *= 10;
	return aCost;
}

bool RunState::CanLockChoice(int index) const
{
    return HasPendingPick() && mChoicesRolled && index >= 0 && index < RUN_CHOICES &&
        (IsPlantPick() ? mPlantChoices[index] != SeedType::SEED_NONE : mBuffChoices[index] != RUN_BUFF_CHOICE_NONE);
}

int RunState::GetRerollPurchaseCost(int lockedChoice) const
{
    return (mRerollVouchers > 0 ? 0 : GetPaidRerollCost()) + (CanLockChoice(lockedChoice) ? 100 : 0);
}

int RunState::BuffMultValueFor(int theBuffMult)
{
	static const int aMult[] = { 1, 2, 3 };
	if (theBuffMult < RUN_BUFFMULT_X1 || theBuffMult > RUN_BUFFMULT_X3) return aMult[RUN_BUFFMULT_X1];
	return aMult[theBuffMult];
}

int RunState::LadderPermilleFor(int theLadder, int theScene)
{
	static const int aTables[5][RUN_SCENE_COUNT] = {
		{ 1000, 1330, 1768, 2352, 3129 },	// RUN_LADDER_STD（×1.33^场景 = 现状）
		{ 1000, 1000, 1000, 1000, 1000 },	// RUN_LADDER_X1
		{ 1000, 1200, 1440, 1728, 2073 },	// RUN_LADDER_X12
		{ 1000, 1500, 2250, 3375, 5062 },	// RUN_LADDER_X15
		{ 1000, 2000, 4000, 8000, 16000 },	// RUN_LADDER_X2
	};
	if (theLadder < RUN_LADDER_STD || theLadder > RUN_LADDER_X2) theLadder = RUN_LADDER_STD;
	if (theScene < 0 || theScene >= RUN_SCENE_COUNT) theScene = 0;
	return aTables[theLadder][theScene];
}

int RunState::FirstWaveDelayTicks() const
{
	if (mFirstWave == RUN_FIRSTWAVE_VANILLA) return 1800;
	if (mFirstWave == RUN_FIRSTWAVE_LONG) return 6000;
	return 3000;
}

int RunState::SpeedGrowthPermilleFor(int theSpeedTier, int theSceneIndex)
{
	int aBase = (theSpeedTier == RUN_SPEED_X105) ? 1050 : (theSpeedTier == RUN_SPEED_X103 ? 1030 : 1000);
	if (aBase == 1000) return 1000;
	int aScene = (theSceneIndex >= 0 && theSceneIndex < RUN_SCENE_COUNT) ? theSceneIndex : 0;
	int aPermille = 1000;
	for (int i = 0; i < aScene; i++)
	{
		aPermille = aPermille * aBase / 1000;	// 逐场景截尾
	}
	return aPermille;
}

int RunState::ZombieBuffSpeedPermille() const
{
	int aPermille = 1000 + 300 * ZombieBuffCount(ZOMBIE_BUFF_SWIFT);
	return (aPermille > 4000) ? 4000 : aPermille;	// 封顶 ×4（层 10 起）
}

int RunState::ZombieBuffHealthPermille() const
{
	int aPermille = 1000;
	int aLayers = ZombieBuffCount(ZOMBIE_BUFF_IRON);
	for (int i = 0; i < aLayers && aPermille < 100000; i++)
	{
		aPermille = aPermille * 1500 / 1000;		// ×1.5/层，逐层截尾
		if (aPermille > 100000) aPermille = 100000;	// 封顶 ×100（层 12 起）
	}
	return aPermille;
}

int RunState::ZombieBuffSingleHitCap() const
{
	// 护心（MOD_BUILD 56 改版，2026-10-16 用户令「最高一层」，与 PC 同批同口径）：投 ≥1 层
	// 恒 200 封顶，多层不再递减——原 250−50×层、底 50 的递减在高层数下把「只挡一次」的
	// 一次性盾叠成常驻低盾，层数收益只剩数字。层数照记（协议/检查点字段不动），效果按一层封。
	int aLayers = ZombieBuffCount(ZOMBIE_BUFF_HEARTGUARD);
	if (aLayers <= 0) return 0;					// 0 = 不封顶
	return 200;
}

int RunState::ZombieBuffBitePermille() const
{
	int aPermille = 1000;
	int aLayers = ZombieBuffCount(ZOMBIE_BUFF_RAVENOUS);
	for (int i = 0; i < aLayers && aPermille < 32000; i++)
	{
		aPermille *= 2;							// ×2/层
		if (aPermille > 32000) aPermille = 32000;	// 封顶 ×32（层 5 起）
	}
	return aPermille;
}

int RunState::ZombieBuffRegenPermillePerSec() const
{
	int aPermille = 5 * ZombieBuffCount(ZOMBIE_BUFF_REGEN);	// 0.5%/层 = 5‰
	return (aPermille > 250) ? 250 : aPermille;				// 封顶 25%/s（层 50 起）
}

void RunState::RollZombieVoteChoices(int theLevelIndex, unsigned char theOut[RUN_ZOMBIE_VOTE_CHOICES]) const
{
	unsigned int aSeed = (unsigned int)mRunSeed
		^ (0x9E3779B9u * (unsigned int)(theLevelIndex + 1))
		^ 0x27D4EB2Fu;
	Sexy::MTRand aRNG(aSeed);
	int aPool[RUN_ZOMBIE_BUFF_COUNT];
	int aPoolCount = 0;
	for (int i = 0; i < RUN_ZOMBIE_BUFF_COUNT; i++)
		if (CanAddZombieBuffStack(i)) aPool[aPoolCount++] = i;
	for (int i = aPoolCount - 1; i > 0; i--)
	{
		int j = (int)aRNG.Next((unsigned long)(i + 1));
		int aTmp = aPool[i];
		aPool[i] = aPool[j];
		aPool[j] = aTmp;
	}
	for (int i = 0; i < RUN_ZOMBIE_VOTE_CHOICES; i++) theOut[i] = i<aPoolCount ? (unsigned char)aPool[i] : 0xff;
}

// 池子 9 条的展示文案（投票框与结果横幅共用）。中文名/说明走宽字符直绘（GDI，无位图
// 字库缺字风险）；英文名上石材按钮的位图字体路径，保持 ASCII。
namespace
{
struct ZombieBuffText
{
	const char*	mName;		// 英文名（ASCII；按钮位图字体标签用）
	const char*	mNameZh;	// 中文名（宽字符直绘：投票框顶行与结果横幅）
	const char*	mDesc;		// 中文说明（UTF-8，宽字符直绘）
	const char*	mDescEn;	// 英文说明
};
}

static const ZombieBuffText kZombieBuffTexts[RunState::RUN_ZOMBIE_BUFF_COUNT] = {
	{ "Swift", "迅捷", "僵尸移动速度 +30%/层（至多 ×4）", "Zombie walk speed +30% per stack (up to x4)" },
	{ "Iron", "铁壁", "僵尸生命值 ×1.5/层（至多 ×100）", "Zombie health x1.5 per stack (up to x100)" },
	{ "Heartguard", "护心", "首次受到的大额伤害封顶 200；只挡一次（最高1层）", "First big hit capped at 200; blocks once (max 1 stack)" },
	{ "Ravenous", "狂食", "每口啃咬伤害 ×2/层（至多 ×32）", "Bite damage x2 per stack (up to x32)" },
	{ "Regen", "再生", "每秒回复本体生命上限0.5%/层，每层最高500（总比例至多25%）", "Regen 0.5% max body HP/sec/stack, capped at 500 per stack (up to 25%)" },
	{ "Steadfast", "免硬控", "免疫冰冻、黄油定身、击退、换道与魅惑；仍可减速（最高1层）", "Immune to freeze, butter, knockback, lane change and charm; slows still work (max 1 stack)" },
	{ "Gravekeeper", "墓碑者", "巨人砸碎植物时，原地留下墓碑（最高1层）", "Gargantuars leave a gravestone where they crush a plant (max 1 stack)" },
	{ "Ghostwalk", "无视阻挡", "普通僵尸不再停下啃咬，直接走过植物；巨人及橄榄球BOSS除外（最高1层）", "Walk through plants instead of chewing; excludes Gargantuars and Football bosses (max 1 stack)" },
	{ "Soft Resist", "软控缩时", "减速持续时间缩短50%，幅度不变；与BOSS抗性不叠加（最高1层）", "Slow duration -50%; strength unchanged; does not stack with boss resistance (max 1 stack)" },
    { "Inflation", "加价", "所有植物最终种植费用 +25 阳光/层，包括原本免费和被增益降到免费的植物", "Final planting cost +25 sun per stack for every plant, including normally free and buff-discounted plants" },
    { "Giant Lineage", "巨人血统", "巨人改投普通巨人；二层后代也可继续投巨人（最多2层）", "Throw Gargantuars; at stack 2 every child can throw another (max 2)" },
    { "Explosive Farewell", "死亡惊喜", "小丑移速每层+100%；战斗死亡按原范围爆炸一次", "Jack-in-the-box walk speed +100%/stack; explodes once on combat death" },
    { "Armored Balloons", "装甲气球", "气球权重×5，气球基础耐久1100；三叶草仍可吹走（最多1层）", "Balloon pick weight x5; balloon base HP 1100; Blover still works (max 1)" },
    { "Fast Zamboni", "极速冰车", "冰车移速每层+100%，本体生命每层+50%", "Zamboni speed +100% and body HP +50% per stack" },
    { "Heavy Football", "重装橄榄", "橄榄本体、防具、攻击每层+200%；体型放大1.5倍", "Football body/armor/attack +200% per stack; visual size x1.5" },
    { "Reinforced Armor", "全面重甲", "所有僵尸防具耐久每层+100%，不增加本体生命", "All zombie armor HP +100% per stack; no body HP increase" },
    { "Digger Ambush", "突袭矿工", "矿工落地立即寻敌，攻击伤害每层+100%", "Diggers seek targets on landing; attack damage +100% per stack" },
};

static const ZombieBuffText& ZombieBuffTextAt(int theId)
{
	if (theId < 0 || theId >= RunState::RUN_ZOMBIE_BUFF_COUNT) theId = 0;
	return kZombieBuffTexts[theId];
}

const char* GetZombieBuffName(int theId)
{
	return ZombieBuffTextAt(theId).mName;
}

const char* GetZombieBuffDisplayName(int theId)
{
	const ZombieBuffText& aText = ZombieBuffTextAt(theId);
	return ModText::IsChinese() ? aText.mNameZh : aText.mName;
}

const char* GetZombieBuffDesc(int theId)
{
	const ZombieBuffText& aText = ZombieBuffTextAt(theId);
	return ModText::IsChinese() ? aText.mDesc : aText.mDescEn;
}

int RunState::GetEffectiveBuffCount(int id) const
{
    int value=GetBuffCount(id);
    int base=RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT;
    if(id==RUN_BUFF_COUNT+RUN_UPGRADE_SNOWPEA && GetBuffCount(base+RUN_BUFF2_FROSTPIERCE)>0)return value;
    if(id==base+RUN_BUFF2_FROSTPIERCE && GetBuffCount(RUN_BUFF_COUNT+RUN_UPGRADE_SNOWPEA)>0)return value;
    if(id==RUN_BUFF_COUNT+RUN_UPGRADE_KERNELPULT && (GetBuffCount(base+RUN_BUFF2_BUTTERFEAST)>0 || GetBuffCount(base+RUN_BUFF2_BUTTER_SPLASH)>0 || GetBuffCount(base+RUN_BUFF2_BUTTER_HEAVY)>0))return value;
    if((id==base+RUN_BUFF2_BUTTERFEAST || id==base+RUN_BUFF2_BUTTER_SPLASH || id==base+RUN_BUFF2_BUTTER_HEAVY) && GetBuffCount(RUN_BUFF_COUNT+RUN_UPGRADE_KERNELPULT)>0)return value;
    if((id==RUN_BUFF_COUNT+RUN_UPGRADE_WALLNUT || id==base+RUN_BUFF2_THORNS) && GetBuffCount(base+RUN_BUFF2_NUT_BOWLING)>0)return value;
    if(id==base+RUN_BUFF2_NUT_BOWLING && (GetBuffCount(RUN_BUFF_COUNT+RUN_UPGRADE_WALLNUT)>0 || GetBuffCount(base+RUN_BUFF2_THORNS)>0))return value;

    if(mLoanLevel==mLevelIndex && mLoanPlant>=0 && id>=RUN_BUFF_COUNT && GetRunChoicePlant(id)==(SeedType)mLoanPlant)
        for(const auto& b:mLoanBuffs)if(b.mId==id)value=std::max(value,(int)b.mCount);
    return ClampPurpleReplacementStacks(id,value);
}
bool RunState::HasUsablePlant(SeedType plant) const
{
    return HasPlant(plant) || (!IsEndless()&&mTeam.shared.Has((int)plant)) || (mLoanLevel==mLevelIndex && mLoanPlant==(int)plant);
}
std::vector<SeedType> RunState::GetUsablePool() const
{
    auto result=mPool;
    for(int p=0;p<48;++p)if((!IsEndless()&&mTeam.shared.Has(p))&&!HasPlant((SeedType)p))result.push_back((SeedType)p);
    if(mLoanLevel==mLevelIndex && mLoanPlant>=0 && !HasPlant((SeedType)mLoanPlant) && !(!IsEndless()&&mTeam.shared.Has(mLoanPlant)))result.push_back((SeedType)mLoanPlant);
    return result;
}
bool RunState::ValidateLoan(SeedType plant,const std::vector<BuffStack>& buffs) const
{
    if(plant<SEED_PEASHOOTER || plant>SEED_COBCANNON || buffs.size()>16)return false;
    for(size_t i=0;i<buffs.size();++i) {
        const auto& b=buffs[i];
        if(b.mId<RUN_BUFF_COUNT || b.mId>=RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT+RUN_PLANT_BUFF2_COUNT ||
            !b.mCount || GetRunChoicePlant(b.mId)!=plant)return false;
        for(size_t j=0;j<i;++j)if(buffs[j].mId==b.mId)return false;
    }
    auto has=[&](int id){int count=GetBuffCount(id);for(const auto& b:buffs)if(b.mId==id)count=std::max(count,(int)b.mCount);return count>0;};
    int base=RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT;
    if(plant==SEED_SNOWPEA && has(RUN_BUFF_COUNT+RUN_UPGRADE_SNOWPEA) && has(base+RUN_BUFF2_FROSTPIERCE))return false;
    if(plant==SEED_KERNELPULT && has(RUN_BUFF_COUNT+RUN_UPGRADE_KERNELPULT) &&
        (has(base+RUN_BUFF2_BUTTERFEAST)||has(base+RUN_BUFF2_BUTTER_SPLASH)||has(base+RUN_BUFF2_BUTTER_HEAVY)))return false;
    if(plant==SEED_WALLNUT && has(base+RUN_BUFF2_NUT_BOWLING) &&
        (has(RUN_BUFF_COUNT+RUN_UPGRADE_WALLNUT)||has(base+RUN_BUFF2_THORNS)))return false;
    return true;
}

const char* RunState::EndlessThemeName(int theme) {
    switch(theme) {
    case 1:return ModText::Tr("装甲队", "Armored ranks");
    case 2:return ModText::Tr("快速队", "Fast ranks");
    case 3:return ModText::Tr("空袭队", "Air raid");
    case 4:return ModText::Tr("水路队", "Pool assault");
    default:return ModText::Tr("自由出怪", "Mixed ranks");
    }
}
