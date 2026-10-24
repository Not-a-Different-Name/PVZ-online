#ifndef __RUNSTATE_H__
#define __RUNSTATE_H__
#include "AdmissionState.h"
#include "TeamProgressionState.h"

#include "ZombieBuffRules.h"
#include "EndlessRules.h"
#include "CrowdRules.h"
#include <string>
#include <vector>
#include "../../ConstEnums.h"

// @pvz-online: 全流程闯关（肉鸽）的本地状态 + 检查点文件。
//
// 一局分四档（M4-b 定案；批十 2026-10-05 按玩家反馈改版；无尽档 2026-10-10 定案）：
// 三档时长——完整版 = 5 场景 × 5 关 = 25 关；普通版 = 每场景第 1/5 关 = 10 关（即原快速版）；
// 快速版 = 每场景第 5 关 = 5 关（短局取每场景收尾的难关；短一局用更密的奖励屏补内容量，
// 倍乘见 BeginLevelEndPicks）。三档共用同一张 25 关号表（完整版全取、普通/快速抽行，见
// LevelForModeIndex），首关仍是 10 波带旗；场景（白天 → 夜 → 泳池 → 迷雾 → 屋顶）与难度阶梯
// 只看第几关落在哪个场景，三档不差一个字。第四档无尽：进场锁死一个场景（mEndlessScene），
// 在该场景 5 个原型里按关序号 % 5 无限循环，难度走专属对数阶曲线（见 GetDifficultyPermille）。
// 卡池随三选一逐关变大、buff 跟着这一局走——检查点把这两样一起带走。
//
// 检查点写在 userdata/run%d.dat，和 user%d.dat（本机档案进度）完全分开：
// 闯关不推进 mLevel、不发奖杯，退出 / 换档案都不受影响。
// v17保存当前选择阶段与开战检查点；在奖励页返回后可以接着选，已选成长不会丢失。
// 付费重摇以同一份事务日志提交档案和检查点，进程中断后先恢复交易再读取。

class PlayerInfo;
class RunState
{
public:
	// @pvz-online: 时长档（M4-b）。完整版一局 25 关；普通版抽每场景第 1/5 关（10 关，=原快速表）、
	// 快速版抽第 5 关（5 关）——短一局用更密的奖励屏补内容量（倍乘见 BeginLevelEndPicks）。
	// 无尽档（2026-10-10 用户定案）：多人无尽的第四档——锁一个场景、在该场景 5 个原型里无限
	// 循环（关序号 % 5），无奖杯/结算屏（IsComplete 恒假），难度走专属对数阶曲线。
	enum	{ RUN_MODE_FULL = 0, RUN_MODE_NORMAL = 1, RUN_MODE_QUICK = 2, RUN_MODE_ENDLESS = 3 };

	// @pvz-online: 出怪难度档（2026-10-03 用户定案）：房主开局前在选模式页选的全局出怪
	// 旋钮。轻松 ×0.5 / 标准 ×1.0 / 高压 ×2.0（2026-10-04 由 ×1.5 上调）——直接乘在全队的顺位乘数上（乘后向下取整、
	// 保底 1 倍；每波数量上限随同一乘数放大），落点见 Board::PickZombieWaves。
	// 房间级设置：一局定一次，随 START_LEVEL 走、进检查点；单机局同样可选。
	enum	{ RUN_DIFF_EASY = 0, RUN_DIFF_STD = 1, RUN_DIFF_HIGH = 2 };

	// @pvz-online: 高级选项（批 C，2026-10-09 用户定案）：房主在选模式页「高级选项…」定的
	// 房间级设置。出怪规模 ×0.5/×1/×2/×4——乘在顺位乘数链之后（乘后向下取整、保底 1，
	// 每波数量上限随同一倍率放大后再钳回硬顶）；节奏 ×0.6/×1/×1.6——只乘波间隔倒计时。
	// 千分比取数走 ScalePermilleFor / TempoPermilleFor（非法档按标准）。随 START_LEVEL 走、
	// 进检查点（v10）；单机局同样可选。
	enum	{ RUN_SCALE_HALF = 0, RUN_SCALE_STD = 1, RUN_SCALE_DOUBLE = 2, RUN_SCALE_QUAD = 3 };
	enum	{ RUN_TEMPO_FAST = 0, RUN_TEMPO_STD = 1, RUN_TEMPO_SLOW = 2 };
	// @pvz-online: 增益倍选（2026-10-13 用户令「每次选 buff 1 2 3 倍」）：房主在高级选项里
	// 选的倍数——每屏照旧三选一拿 1 条，但发放的三选一屏数 ×1/×2/×3（拿到的总数翻倍，
	// 代价是连点更多屏）。0-based 索引，倍率值走 BuffMultValueFor（非法按 1 倍）。
	enum	{ RUN_BUFFMULT_X1 = 0, RUN_BUFFMULT_X2 = 1, RUN_BUFFMULT_X3 = 2 };
	// @pvz-online: 场景难度阶梯五档（2026-10-14 用户令「阶梯应当可调，做 1 1.2 1.33 1.5 2
	// 五档，放在高级选项」）：乘在血量与出怪点数上的逐场景阶梯因子。RUN_LADDER_STD = 0 是
	// 现状 ×1.33 档、也是 sentinel——老档/老协议读 0 恰好是现状，不静默变难度。五张固定
	// 千分比表见 LadderPermilleFor；**无尽档不吃阶梯**（上批定案的无尽专属曲线保持独立，
	// 高级选项在无尽档也不摆「场景难度」行）。
	enum	{ RUN_LADDER_STD = 0, RUN_LADDER_X1 = 1, RUN_LADDER_X12 = 2, RUN_LADDER_X15 = 3, RUN_LADDER_X2 = 4 };
	// @pvz-online: 僵尸移速成长档（2026-10-16 用户令「移速成长做成可选项放到高级选项，默认无，
	// 有 1.03 1.05 两挡」+「每场景」复利）：乘在 ZombieInitialize 的移速上（迅捷之后、独立
	// 叠乘），每进一个新场景复利一次（场景 1 恒 ×1）——千分比表见 SpeedGrowthPermilleFor，
	// 两端各算、逐位一致。位包 bit12..13 持久（老档读 0 = 无成长，安全降级不抬版本）。
	enum	{ RUN_SPEED_NONE = 0, RUN_SPEED_X103 = 1, RUN_SPEED_X105 = 2 };
	enum	{ RUN_FIRSTWAVE_STD = 0, RUN_FIRSTWAVE_VANILLA = 1, RUN_FIRSTWAVE_LONG = 2 };
	// @pvz-online: 僵尸强化（2026-10-14 用户令「到下一场景选给僵尸 0/1/2 个 buff；作用在
	// 无尽则五关一选」）：独立于植物词条表的第二条强化线——每进一个新场景全队投票叠层，
	// 层数累积、无总上限（255 封顶防溢出，各效果数值饱和）。池子 9 条固定 id（即
	// mZombieBuffStacks 下标，随投票/协议/存档逐位一致）；每场景发几条 = mZombieBuffMode
	// （高级选项「僵尸 buff」0/1/2，0 = 不开）。
	enum
	{
		ZOMBIE_BUFF_SWIFT = 0,		// 迅捷：移速 +30%/层
		ZOMBIE_BUFF_IRON = 1,		// 铁壁：生命 ×1.5/层
		ZOMBIE_BUFF_HEARTGUARD = 2,	// 护心（56 改版：最高一层）：首次大额伤害封顶 200；只挡一次
		ZOMBIE_BUFF_RAVENOUS = 3,	// 狂食：每口啃咬 ×2/层
		ZOMBIE_BUFF_REGEN = 4,		// 再生：每秒回 0.5% 上限血/层
		ZOMBIE_BUFF_STEADFAST = 5,	// 免硬控：冰冻/黄油/击退/换道/魅惑；最高一层
		ZOMBIE_BUFF_GRAVEKEEPER = 6,	// 墓碑者：巨人砸碎植物→原地生成墓碑
		ZOMBIE_BUFF_GHOSTWALK = 7,	// 无视阻挡：普通僵尸直接走过植物不停下啃咬
		ZOMBIE_BUFF_PLANT_TAX = 9,    // 加价：所有植物最终种植费用 +25 阳光/层
		ZOMBIE_BUFF_SOFTRESIST = 8,
        ZOMBIE_BUFF_GIANT_CHAIN = RunSpeciesRules::GIANT_CHAIN,
        ZOMBIE_BUFF_JACK_DEATH = RunSpeciesRules::JACK_DEATH,
        ZOMBIE_BUFF_ARMORED_BALLOON = RunSpeciesRules::ARMORED_BALLOON,
        ZOMBIE_BUFF_FAST_ZAMBONI = RunSpeciesRules::FAST_ZAMBONI,
        ZOMBIE_BUFF_HEAVY_FOOTBALL = RunSpeciesRules::HEAVY_FOOTBALL,
        ZOMBIE_BUFF_ALL_ARMOR = RunSpeciesRules::ALL_ARMOR,
        ZOMBIE_BUFF_DIGGER_AMBUSH = RunSpeciesRules::DIGGER_AMBUSH,
	// 软控缩时：减速持续时间减半；最高一层
	};
	static const int	RUN_ZOMBIE_BUFF_COUNT	= RunZombieBuffRules::COUNT;	// 池子大小（= 上面枚举条目数）
	static const int	RUN_ZOMBIE_VOTE_CHOICES	= 3;	// 每次开票的候选数（三选一口径，2026-10-14 用户令）
	static const int	RUN_ZOMBIE_VOTED_NONE	= 0xFFFF;	// mZombieVotedLevel 的"没投过"哨兵

	static const int	RUN_LEVEL_COUNT		= 25;	// 完整版总关数 = 5 场景 × 5 关（数组/静态表的尺寸上限）
	static const int	RUN_SCENE_COUNT		= 5;	// 白天 → 夜 → 泳池 → 迷雾 → 屋顶
	static const int	RUN_LEVELS_PER_SCENE = RUN_LEVEL_COUNT / RUN_SCENE_COUNT;
	// 无尽档的"总关数"：软上限，实际不可达——u16 线传（START_LEVEL 的关序号）与检查点
	// 校验都按它封口，让 mLevelIndex 永远合法（IsComplete 对无尽档恒假，见下）。
	static const int	RUN_ENDLESS_LEVEL_COUNT	= 60000;
	static const int	RUN_SEED_SLOTS		= 10;	// 种子槽固定 10 格（= SEEDBANK_MAX 上限；覆盖原版 mPurchases+6 规则；2026-10-11 8→10，闯关+无尽）
	static const int	RUN_POOL_MAX		= 48;	// 卡池上限 = 全部植物
	static const int	RUN_CHOICES			= 3;	// 一屏摆几张卡
	// @pvz-online: mBuffChoices 的空缺哨兵（方案 §2.4 的防御守卫）：候选不足三条时多出来的
	// 格子填它——不是合法 id（全局 9 + 单株 48 + 第二 buff 39 都够不到），选择函数与
	// 屏上按钮一律忽略。
	static const unsigned short RUN_BUFF_CHOICE_NONE = 0xFFFF;
	// @pvz-online: 三选一「重摇」底价（花园批 2026-10-13 用户拍板的三选一金币出路；同日
	// 用户令改阶梯价；2026-10-14 用户令「第一次也要花金币」取消免费「换一批」——本常量
	// 也是第一次的价）：同一屏里每摇一次 ×10（100 → 1000 → 10000…公式不设上限），
	// 进新的三选一屏即回到底价重新起算（见 GetPaidRerollCost）。
	// 2026-10-14 二次定案：**币面价 10**（不是 100）——钱数全局按 GetMoneyString 的
	// 币×10 口径显示，按钮上要让玩家看到「100」起、阶梯 100/1000/10000，币就得是
	// 10/100/1000（旧底价 100 币在按钮上显示 $1,000，玩家实机报「从1000开始、有 500
	// 多币也点不动」——被 100 币的置灰守卫挡住，2026-10-14 实锤）。钱包上限 99999999 币
	// （2026-10-22 用户「上限加三位数」）让阶梯能走到第 6 步 10000000 币；第 7 步
	// 100000000 币超出上限，自然买不起。
	// 扣存档钱包 mCoins，与商店同源；对齐金僵尸掉落的 100。纯本地下注，不进协议——联机
	// 双端各花各的钱、各摇各的屏。
	static const int	RUN_PAID_REROLL_COST	= 10;

	// @pvz-online: 出怪编排旋钮（M4-a，用户定案）：点数不封顶、数量封顶；点数多就出强僵尸。
	// 用在 Board::PickZombieWaves / Board::PickZombieType 的闯关分支，调平衡只动这组数。
	static const int	RUN_WAVE_ZOMBIE_CAP	= 20;	// 每波僵尸数量上限基准（含旗帜波预放的普通+旗帜）；联机按席位顺位乘数放大，见 Board::PickZombieWaves
	static const int	RUN_HEAVY_POINTS	= 16;	// 单波剩余点数到此为止：接下来只抽"强僵尸"
	static const int	RUN_HEAVY_VALUE		= 4;	// "强僵尸"的价值门槛（铁桶/铁门/橄榄球/巨人等）
	static const int	RUN_GARGANTUAR_VALUE	= 4;	// 巨人系（普通/红眼）点数值 10→4：点数一到 4 就可能抽中
	static const int	RUN_GARGANTUAR_CAP		= 2;	// 巨人系每波合计上限：压价后不设闸，点数富余的波会连抽巨人

	// buff 的三选一屏是 R2 的活儿、数值落地是 R3 的：数据类型一层就该定死，id + 叠了几层。
	struct BuffStack
	{
		unsigned short	mId;
		unsigned short	mCount;
	};

	// @pvz-online: "三选一"的待选状态（R2）。v17起完整保存选择阶段——
	// 选择后写入卡池 / buff 表；未选次数、当前候选和本屏价格一起保存。
	// 候选由 runSeed + mPickSalt 推导（用户 2026-10-03 定案）：盐是本机自己随机的，
	// 不随联机命令走——联机里每个玩家看到的候选各不相同，选什么更是各选各的。
	int							mPendingPlantPicks;		// 还欠几株新植物（一屏只选一株，选完减一）
	int							mPendingBuffPicks;		// 还欠几个增益
	SeedType					mPlantChoices[RUN_CHOICES];
	unsigned short				mBuffChoices[RUN_CHOICES];	// 三条候选 id（缺格 = RUN_BUFF_CHOICE_NONE）
	unsigned int				mPickCounter;			// 抽过几次：同一局里每屏的候选都不一样
	unsigned int				mPickSalt;				// 本机随机盐：StartNew/Load 时各生成一次，进程内不变——
														// 同一局里重开同关还是同一组三条（防刷），换进程/换机器就不同
	// @pvz-online: 候选抽好没抽好的记账（2026-10-08 玩家反馈）。和上面一样纯内存态、
	// v17持久化：mChoicesRolled = 当前这一屏的三条候选已经抽好（打掉/放弃后复位，
	// 下一屏才会重抽——也让重摇重开的那一屏不会被再抽一次）。
	bool						mChoicesRolled;
	// @pvz-online: 「重摇」阶梯价的记账（花园批 2026-10-13；2026-10-14 起第一次也花金币）：
	// 本屏已经花金币重摇过几次——当前价 = 底价 × 10^本数（见 GetPaidRerollCost）。纯内存、
	// 不落检查点；新的一屏（RollChoices 非重抽路径）清零。
	int							mPaidRerollCount;
	// @pvz-online: run-local vouchers persist across checkpoints; clear credit is awarded once per index.
	int mRerollVouchers;
	int mRerollAwardedLevel;
	// @pvz-online: 候选去重批的记账（2026-10-10 用户定案「同屏排除+一屏冷却」，一屏/一批
	// 生灭、v17随检查点保存）。mPickSeenBuffs = 本屏露过脸的全部增益 id（含「重摇」
	// 刷掉的那批，最多 3+3，抽的当下就记）；mPickCooldownBuffs = 上一屏露过脸又没拿的
	// （= 本屏记账减去拿走的那条）——下一屏抽候选时整体摘掉，屏池不够时按"保底 3 条"
	// 自动放宽（见 RollChoices）；拿走的不进冷却，跨屏叠层照旧。植物屏同款记账见下方
	// mPickSeenPlants（2026-10-16 用户令扩入，口径一致）。
	unsigned short				mPickSeenBuffs[RUN_CHOICES * 2];
	int							mPickSeenBuffCount;
	unsigned short				mPickCooldownBuffs[RUN_CHOICES * 2];
	int							mPickCooldownBuffCount;
	// 植物屏跨屏冷却（2026-10-16 用户令）：跳过/「重摇」刷掉的株，下一屏不再出现；拿走的
	// 株已入卡池、owned 过滤天然挡住，冷却里减不减无行为差（照抄增益屏口径保持一致）。
	// v17随检查点保存；换批（BeginStartPicks/BeginLevelEndPicks）即清。
	unsigned short				mPickSeenPlants[RUN_CHOICES * 2];
	int							mPickSeenPlantCount;
	unsigned short				mPickCooldownPlants[RUN_CHOICES * 2];
	int							mPickCooldownPlantCount;

public:
	// @pvz-online 内存态：这一局正在打（含"刚过关、正要进下一关"的空档）。
	// 回主菜单 = LawnApp 把这个对象删掉；检查点留在盘上，续关时重新读出来。
	int							mRunSeed;		// 这一局的种子：每关波表的种子由它推导，重开同一关不变
	int							mMode;			// 时长档（RUN_MODE_*）：决定关卡表抽行与每关后的奖励屏数
	int							mDiff;			// 出怪难度档（RUN_DIFF_*）：全队出怪总旋钮，乘在顺位乘数上
	// @pvz-online: 无尽档锁定的场景（0..4，仅 RUN_MODE_ENDLESS 用；其余档恒 0）。入口链定死，
	// 随 START_LEVEL 广播（MOD_BUILD 37）与检查点 v12 走；三条进路（建档/线传/读档）都验 0..4。
	int							mEndlessScene;
    int mEndlessOptions = 0;
    int mEndlessVoteRounds = 0;
    mutable std::vector<unsigned char> mDamageHistory;
    Admission::State mAdmission;
    TeamProgression::State mTeam;
    unsigned char mEndlessVoteIds[2] = {255,255};
    uint64_t mEndlessFocusMask = 0;
    int mEndlessSnapshotLevel = -1;
    int mEndlessSnapshotPhase = 0; // 1 rewards, 2 loadout, 3 playing, 4 record at version limit, 5 failed (retry baseline).
    std::vector<unsigned char> mEndlessLawn, mEndlessRetryLawn;
    bool IsNewEndless() const { return IsEndless() && (mEndlessOptions & 0x80); }
    int EndlessBossPeriod() const { return IsNewEndless()?EndlessRules::BossPeriod(mEndlessOptions):0; }
    bool IsEndlessBossLevel() const { int p=EndlessBossPeriod(); return p && (GetPlayingLevelIndex()+1)%p==0; }
    int EndlessTheme() const { return IsNewEndless()?EndlessRules::Theme(mEndlessOptions,mRunSeed,GetPlayingLevelIndex(),mEndlessScene):0; }
    bool HasLineupPreference() const { return IsNewEndless() && (mEndlessOptions & 8); }
    bool PrefersPlant(SeedType p) const { return p>=0 && p<48 && ((mEndlessFocusMask>>(int)p)&1); }
    static const char* EndlessThemeName(int theme);
    bool HasZombieBuffChoices() const { for(int i=0;i<RUN_ZOMBIE_BUFF_COUNT;++i)if(CanAddZombieBuffStack(i))return true;return false; }

	int							mLevelIndex;	// 0..(关数-1) = 当前（或待打的）关序号；>= 关数 = 已通关（关数见 GetLevelCount）
	std::vector<SeedType>		mPool;			// 这一局的卡池（按加入顺序；起始 = 向日葵 + 豌豆射手；无尽档起始 = 全部 48 株，2026-10-11）
	std::vector<BuffStack>		mBuffs;			// 这一局拿到的 buff（同名可叠加）

    // @pvz-online: temporary effects never enter the permanent growth/candidate table.
    int mLoanPlant=-1, mLoanLevel=-1, mLentLevel=-1;
    std::vector<BuffStack> mLoanBuffs;
    int GetEffectiveBuffCount(int id) const;
    bool ValidateLoan(SeedType plant,const std::vector<BuffStack>& buffs) const;
    std::vector<SeedType> GetUsablePool() const;
    bool HasUsablePlant(SeedType plant) const;
	int							mFailCounts[RUN_LEVEL_COUNT];	// 每关失败次数（首版只存不用，平衡阶段再定惩罚）
	// @pvz-online: 末位推车的"整局一次性"记账：bit = 行号。联机闯关里末位玩家的哪几行推车
	// 已经用了（触发或被压）；用过的行跨关不再补。随检查点持久（v5），联机两端各记各的。
	unsigned int				mMowerUsedRows;
	// @pvz-online: 高级选项三字段（批 C）：出怪规模档 / 节奏档 / 植物僵尸混入开关（0 关 1 开）。
	// 房间级设置，随 START_LEVEL 广播（MOD_BUILD 35）、随检查点 v10 持久；联机各端无条件
	// 按房主的值对齐（不参与重建判等）。开关的出怪消费见 RunZombieRoster。
	int							mScale;			// RUN_SCALE_*：出怪规模倍率档
	int							mTempo;			// RUN_TEMPO_*：波间隔倍率档
	int							mZombotany;		// 植物僵尸混入开关
	// @pvz-online: 关底巨型 boss 档（MOD_BUILD 38，2026-10-10 用户定案；2026-10-16 用户令升
	// 三档）：0 = 关 / 1 = 仅末关 / 2 = 末关 + 场景 3 末关（快速档场景 3 跳过；无尽档没有
	// 关底、恒不刷）。旧语义的"开"= 档 1（老检查点/老协议读 0/1 自然成立）。房间级设置，
	// 随 START_LEVEL 广播、随检查点 v13 u16 持久（原样放宽 0/1/2，不抬版本）；联机各端
	// 无条件按房主的值对齐（不参与重建判等）。消费端钳在 SpawnRunBoss。
	int							mBossFlag;
	// @pvz-online: 增益倍选（2026-10-13 用户令）：1/2/3 倍的索引（RUN_BUFFMULT_*）。房间级
	// 设置，随 START_LEVEL 广播（MOD_BUILD 43）、随检查点 v10 位包 bit5..6 持久（老档读 0
	// = ×1，安全降级；不抬检查点版本）；联机各端无条件按房主的值对齐（不参与重建判等）。
	// 消费见 BeginStartPicks / BeginLevelEndPicks（只乘 buff 屏数）与 BuffMultValueFor。
	int							mBuffMult;
	// @pvz-online: 场景难度阶梯档（2026-10-14 用户令）：RUN_LADDER_*，五张固定千分比表见
	// LadderPermilleFor。房间级设置，随 START_LEVEL 广播、随检查点位包 bit7..9 持久（v14
	// 前的老档读 0 = 现状标准档，安全降级、不抬版本）；联机各端无条件按房主的值对齐。
	int							mLadder;
	// @pvz-online: 僵尸移速成长档（2026-10-16 用户令）：RUN_SPEED_*，千分比表见
	// SpeedGrowthPermilleFor（每进一个新场景复利一次）。房间级设置，随 START_LEVEL 广播、
	// 随检查点位包 bit12..13 持久（老档读 0 = 无成长，安全降级不抬版本）；联机各端无条件
	// 按房主的值对齐。消费端钳（档非法按无成长）。
	int							mSpeedTier;
	int							mFirstWave;
	// @pvz-online: 僵尸强化的两个字段（2026-10-14 用户令）：mZombieBuffMode = 高级选项
	// 「僵尸 buff」档 0/1/2（每进一个新场景投票叠几条，0 = 不开）；mZombieBuffStacks[9] =
	// 池子 9 条的当前层数（投中 +1、255 封顶，累积不重置）。房间级，随 START_LEVEL 广播
	// （层数快照随 mLastStartLevel 自愈重连）、随检查点 v14 持久；mZombieVotedLevel =
	// 上一张票投在第几关（RUN_ZOMBIE_VOTED_NONE = 没投过；重试同一关不重开票）。
	int							mZombieBuffMode;
	unsigned char				mZombieBuffStacks[RUN_ZOMBIE_BUFF_COUNT];
	unsigned short				mZombieVotedLevel;
	// @pvz-online: 补发追赶的目标关序号（R5）。队友没有检查点 / 检查点落后于主机时，
	// 不是"跳到主机的关"，而是从这一局的起点一屏一屏地把欠下的三选一补齐——
	// 补做的屏与真打过的一模一样（候选由 runSeed + 关序号推导）。开着的时候
	// IsCatchingUp() 为真，UpdateRunPick 每选完一屏就 AdvanceCatchUp 挪一关。
	int							mCatchUpLevel;

public:
	RunState();

	// 全新一局：卡池回到两株、失败计数清零、从第 1 关开打。
	// 无尽档（theRunMode == RUN_MODE_ENDLESS）：theEndlessScene 锁场景（0..4，越界钳 0）；
	// 卡池直接给满全部 48 株常规植物（2026-10-11 用户令「开局全植物」——模仿者照旧不给）。
	// theBuffMult（2026-10-13）：增益倍选索引（RUN_BUFFMULT_*，非法按 ×1——消费端钳）。
	// theLadder（2026-10-14）：场景难度阶梯档（RUN_LADDER_*，非法按现状档）。
	// theZombieBuff（2026-10-14）：僵尸 buff 档 0/1/2（每场景投票叠几条；层数表跟随本局清零）。
	// theSpeedTier（2026-10-16）：僵尸移速成长档（RUN_SPEED_*，非法按无成长——消费端钳）。
	// theBossFlag 同日起三档 0 关 / 1 仅末关 / 2 末关+场景 3（默认 2）。
	void				StartNew(int theRunSeed, int theRunMode = RUN_MODE_FULL, int theRunDiff = RUN_DIFF_STD,
							int theRunScale = RUN_SCALE_STD, int theRunTempo = RUN_TEMPO_STD, int theZombotany = 0,
							int theEndlessScene = 0, int theBossFlag = 2, int theBuffMult = 0,
							int theLadder = RUN_LADDER_STD, int theZombieBuff = 1,
							int theSpeedTier = RUN_SPEED_NONE, int theFirstWave = RUN_FIRSTWAVE_STD, int theEndlessOptions = EndlessRules::DEFAULT_OPTIONS);

	// 时长档的关数口径：每场景关数（5/2/1）与总关数（25/10/5）。模式非法按完整版。
	// 无尽档的 LevelCountForMode = RUN_ENDLESS_LEVEL_COUNT（软上限）；LevelsPerScene 对
	// 无尽档返回完整版值（5），只作静态表尺寸用——无尽档的关卡映射在 GetLevel 另有分支。
	static int			LevelsPerScene(int theRunMode);
	static int			LevelCountForMode(int theRunMode);
	int					GetLevelCount() const { return LevelCountForMode(mMode); }

	// 出怪难度档的千分比（500/1000/1500）。档位非法按标准 1000——盘上的档、线传的字节
	// 都从这儿过一道，读数的地方不用再操心合法性。
	static int			DiffPermilleFor(int theRunDiff);

	// 高级选项两个倍率档的千分比（批 C）：规模 {500,1000,2000,4000}、节奏 {600,1000,1600}。
	// 档位非法按标准 1000——与 DiffPermilleFor 同一套纪律，消费点不用再操心合法性。
	static int			ScalePermilleFor(int theRunScale);
	static int			TempoPermilleFor(int theRunTempo);

	// 增益倍选的倍率值（1/2/3）。档位非法按 1——与上面同一套纪律，消费点不用再操心合法性。
	static int			BuffMultValueFor(int theBuffMult);

	// 场景难度阶梯五档的千分比表（2026-10-14 用户令）：五档 × 五场景固定字面量、截尾
	// （两端逐位一致，不用运行时浮点）。STD 档表 = 2026-10-05 定案的现状表一字不差。
	// 档位/场景非法按 STD/场景 0——与上面同一套纪律，消费点不用再操心合法性。
	static int			LadderPermilleFor(int theLadder, int theScene);

	// 僵尸移速成长档的千分比（2026-10-16 用户令）：按"进过的场景数"复利——场景序号 0..4
	// 对应 ×1 / ×1.03 / ×1.03² / ×1.03³ / ×1.03⁴（×1.05 档同理，≈×1.12 / ≈×1.21 收尾），
	// 整数迭代截尾、两端逐位一致；档非法按无成长（1000）。消费点 = ZombieInitialize
	// （乘在迅捷之后、独立叠乘）。
	int					FirstWaveDelayTicks() const;
	static int			SpeedGrowthPermilleFor(int theSpeedTier, int theSceneIndex);
	// 本局现取：档 × 当前场景一次算齐（Zombie.cpp 只问这一句）。
	int					ZombieSpeedGrowthPermille() const
	{
		return IsNewEndless() ? EndlessRules::Growth(mSpeedTier, GetPlayingLevelIndex()) : SpeedGrowthPermilleFor(mSpeedTier, GetSceneIndex());
	}

	// @pvz-online: 僵尸强化的三个取数口（2026-10-14 用户令）。每场景发放档 0/1/2 非法按 0
	// （= 不开）；层数与叠层口（投票投中 +1、255 封顶）。全部走"消费端钳"纪律：消费点
	// 拿到的是已经钳好的值。
	int					GetZombieBuffPerScene() const
	{
		return (mZombieBuffMode >= 0 && mZombieBuffMode <= 2) ? mZombieBuffMode : 0;
	}
	int					ZombieBuffCount(int theId) const
	{
		if (theId < 0 || theId >= RUN_ZOMBIE_BUFF_COUNT) return 0;
		return (int)mZombieBuffStacks[theId];
	}
	bool                CanAddZombieBuffStack(int theId) const
	{
		if (theId < 0 || theId >= RUN_ZOMBIE_BUFF_COUNT) return false;
        if (ZombieBuffCount(theId) >= 255) return false;
        if (theId >= RunSpeciesRules::PRE_SPECIES_COUNT && ZombieBuffCount(theId) >= RunSpeciesRules::MaxStacks(theId)) return false;
        if (theId == ZOMBIE_BUFF_RAVENOUS && ZombieBuffCount(ZOMBIE_BUFF_GHOSTWALK) > 0) return false;
        if (IsNewEndless()) {
            if(theId==ZOMBIE_BUFF_SWIFT && ZombieBuffSpeedPermille()*ZombieSpeedGrowthPermille()>=4000000)return false;
            const int caps[RUN_ZOMBIE_BUFF_COUNT]={10,12,1,5,50,1,1,1,1,255,2,255,1,255,255,255,255};
            if(ZombieBuffCount(theId)>=caps[theId])return false;
        }
		return (theId != ZOMBIE_BUFF_STEADFAST && theId != ZOMBIE_BUFF_SOFTRESIST &&
            theId != ZOMBIE_BUFF_GHOSTWALK && theId != ZOMBIE_BUFF_GRAVEKEEPER &&
            theId != ZOMBIE_BUFF_HEARTGUARD) || ZombieBuffCount(theId) == 0;
	}
	void				AddZombieBuffStack(int theId)
	{
		if (!CanAddZombieBuffStack(theId)) return;
		if (mZombieBuffStacks[theId] < 255) mZombieBuffStacks[theId]++;
	}
	// 各条效果的消费端换算（整数千分比/点值，饱和封顶；层 0 = 无效果——护心/再生返回 0
	// 即"不生效"）。免硬控/软控缩时/墓碑者/无视阻挡是 ≥1 判定，直接走 ZombieBuffCount。
	// 数值出处与调整史见 docs/07。
	int					ZombieBuffSpeedPermille() const;		// 迅捷：1000+300×层，封顶 4000（×4）
	int					ZombieBuffHealthPermille() const;		// 铁壁：1.5^层 千分比（逐层截尾），封顶 100000（×100）
	int					ZombieBuffSingleHitCap() const;			// 护心（56 最高一层）：≥1 层恒 200；0 = 不封顶
	int					ZombieBuffBitePermille() const;			// 狂食：2^层 千分比，封顶 32000（×32）
	int					ZombieBuffRegenPermillePerSec() const;	// 再生：5×层（每秒回血的千分比），封顶 250

	// 这一关的投票候选（2026-10-14 用户令）：池子 9 条按 (runSeed, 关序号) 哈希洗牌取前
	// RUN_ZOMBIE_VOTE_CHOICES 条——房主抽、随 OPEN 消息广播，单机自抽；各端同一把种子推出
	// 同一组，客户端不自己抽（收 OPEN 回填）。写进 theOut（长度 RUN_ZOMBIE_VOTE_CHOICES）。
	void				RollZombieVoteChoices(int theLevelIndex, unsigned char theOut[RUN_ZOMBIE_VOTE_CHOICES]) const;

	// 该选植物 / 该选 buff 了（一局开始时先挑四株 + 两个增益——进第 1 关前手里就有 6 株；
	// 快速版开局加厚成六株 + 四个增益，批十 2026-10-05；无尽档开局只发四个增益，2026-10-11
	// ——卡池开局即满、植物屏自动归零）。每过一关再挑一批：植物按时长档
	// 倍乘（完整 2 / 普通 6 / 快速 10 株），增益每关收尾的发屏数按时长档定（2026-10-08
	// 玩家反馈批：普通固定 3、快速 3/4/5/5、完整保持 3→7 封顶、无尽固定 2）。
	// 只负责"欠几屏"，候选由 RollChoices 现抽。
	// 卡池拿满 48 株时植物屏没得抽，这两处会自动少发/不发植物屏（见 CanOfferPlantPick）。
	void				BeginStartPicks();
	void				BeginLevelEndPicks();
	// 卡池里还有没到手的植物（种子屏才有候选）。玩家最多能拿到 48 株，关数一多每关的
	// 植物奖（2/6/10 株）必然把候选抽干——抽干了就不再发植物屏，只发增益屏。
	bool				CanOfferPlantPick() const;
	bool				HasPendingPick() const { return mPendingPlantPicks > 0 || mPendingBuffPicks > 0; }
	// 这一屏发的是植物（true）还是 buff（false）。
	bool				IsPlantPick() const { return mPendingPlantPicks > 0; }
	// 抽当前这一屏的三条候选，摆在 mPlantChoices / mBuffChoices 里等玩家点。
	// 增益屏无保底（2026-10-09 权重批撤「第 1 格保底全局」，全局密度由 §8.7 权重体系保证）。
	// theReroll = true（「重摇」）时：本屏现三条先从候选整体摘掉——池子够时重抽必出全新
	// 三条；增益屏另摘上一屏没拿的（mPickCooldownBuffs，含上一屏被刷掉的那批）；两者都在
	// "池子不够"时按保底 min(池子, 3) 条自动放宽（宁可还有重复，不让三选一屏变秃）。
	void				RollChoices(bool theReroll = false, int lockedChoice = -1);
	// @pvz-online: 当前屏的候选抽没抽（LawnApp::UpdateRunPick 靠它别把「重摇」重开的屏再抽一遍）。
	bool				ChoicesRolled() const { return mChoicesRolled; }
	// @pvz-online: 「重摇」（花园批 2026-10-13；2026-10-14 用户令起取消免费「换一批」——
	// 第一次也花金币）：花金币把这屏的候选重抽一批——同一条 RollChoices(true) 重抽链
	// （摘本屏现三条），扣币在 LawnApp::RunPickPaidReroll 那边，这里只管重抽；顺便把本屏
	// 的重摇计数 +1（阶梯价，见下）。
	void				PaidReroll(int lockedChoice = -1);
	bool CanLockChoice(int index) const;
	int GetRerollPurchaseCost(int lockedChoice = -1) const;
	// @pvz-online: 这一屏当前的重摇价（花园批同日用户令：「每次*10，从100开始 100 1000 10000
	// 这样，不设上限」+ 拍板每屏重置）= 底价 × 10^本屏已花币重摇次数。LawnApp 扣费与
	// RunPickDialog 标价/置灰共用这一个出口，保证三处永远同价。
	int					GetPaidRerollCost() const;
	// 玩家点了第 theIndex 张卡：植物进卡池、buff 叠一层，各欠的数减一。
	void				TakePlantChoice(int theIndex);
	void				TakeBuffChoice(int theIndex);
	// @pvz-online: 玩家点了「放弃」（2026-10-03 用户定案）：这一屏不选也不要——欠的屏数
	// 照减（先植物后增益，与 IsPlantPick 的先后一致），卡池 / buff 表原样不动。
	void				SkipPendingPick();
	// @pvz-online: 去重批的回执（2026-10-10）：这一屏增益消费掉了——把本屏露过脸的全部
	// （含「重摇」刷掉的那批）减去拿走的那条，记成下一屏的冷却集。放弃传哨兵 = 整批进冷却；
	// 拿走的那条不进冷却（下一屏还能再来叠层）。
	void				NoteBuffScreenConsumed(unsigned short theTakenId);
	// @pvz-online: 植物屏同款回执（2026-10-16 用户令）：本屏露过脸的全部减去拿走的那株 =
	// 下一屏冷却集；放弃传 SEED_NONE = 整批进冷却。
	void				NotePlantScreenConsumed(SeedType theTakenSeed);
	// 这一局拿到某个 buff 的层数（R3 的数值层按它算加成）。
	int					GetBuffCount(int theBuffId) const;
    int GetChoiceWeight(int theId) const;
	// 这株植物在不在这局的卡池里。选卡界面（卡池 > 10 格才弹）靠它决定哪些袋子
	// 画得出来、点得动——见 LawnApp::SeedTypeAvailable 的闯关分支。
	bool				HasPlant(SeedType theSeedType) const;

	// 检查点读写。Load 失败（文件不在 / 版本不符 / 内容越界）返回 false，对象保持"空局"。
	bool				Load(int theProfileId, int theMode = -1, int theScene = 0, bool theBackup=false, uint64_t theTeam=0);
	bool				Save(int theProfileId) const;
    bool SaveWithPlayer(PlayerInfo& player) const;
    std::vector<unsigned char> EncodeCheckpoint() const;
	static std::string CheckpointSummary(int theProfileId, int theMode = -1, int theScene = 0);
	static bool			HasCheckpoint(int theProfileId, int theMode = -1, int theScene = 0);
	// @pvz-online: 探读检查点（2026-10-10）：完整走一遍 Load（含全部校验），成功才回填
	// 时长档与无尽场景，返回 true。两处「续不续」的问句先用它甄别——HasCheckpoint 只看
	// 文件在不在，而 ContinueRun 的兜底是 StartNew 完整档，探错会悄悄开成普通局。
	static bool			PeekCheckpoint(int theProfileId, int& theRunMode, int& theEndlessScene, int theMode = -1, int theScene = 0);
	// 一局打完了就删掉：那份检查点代表的那一局已经结束，留着只会让下次开局多问一句
	// "续不续"（续了也是从头开）。
	static void			DeleteCheckpoint(int theProfileId);
	static std::string	GetCheckpointName(int theProfileId, int theMode = -1, int theScene = 0, uint64_t theTeam=0);

	// 无尽档没有"打完"这回事（2026-10-10 用户定案：无奖杯/结算屏，关序号只涨不封）；其余档
	// 在关序号走到关数时收场。
	bool				IsComplete() const { return mMode != RUN_MODE_ENDLESS && mLevelIndex >= GetLevelCount(); }
	// @pvz-online: 无尽档判定（无尽续草坪批）：草坪原地续关的几个分叉口都问这一句。
	bool				IsEndless() const { return mMode == RUN_MODE_ENDLESS; }
	void				AdvanceLevel() { if(!IsEndless() || mLevelIndex<RUN_ENDLESS_LEVEL_COUNT-1) ++mLevelIndex; }

	// @pvz-online: 补发追赶（R5）。开始补：目标关序号存下，先把当前这关的奖励屏选完
	// （HasPendingPick 那套），选完由 AdvanceCatchUp 挪一关、再摆下一屏，直到追平。
	void				BeginCatchUp(int theTargetIndex) { mCatchUpLevel = theTargetIndex; }
	bool				IsCatchingUp() const { return mLevelIndex < mCatchUpLevel; }
	void				AdvanceCatchUp()
	{
		if (!IsCatchingUp()) return;
		mLevelIndex++;
		BeginLevelEndPicks();
	}

	// 本关失败一次：mFailCounts 对应格 +1（R4）。只记账，怎么用留平衡阶段；
	// 落盘由调用方（LawnApp::RunNoteFailure）紧跟一句 Save 完成。
	void				NoteLevelFailed();

	// 当前关：mLevel 值的映射（关号表见 LevelForIndex）与波表种子。序号越界返回 -1 / 0。
	// 无尽档 = 锁定场景内的 5 原型循环（GetLevel/GetLevelSeed 都按关序号 % 5 取行）。
	int					GetLevel() const;
	int					GetLevelSeed() const;
	// 完整版 25 关号表（表内序号 → 引擎关号）。普通/快速档的关号都是它的子集，所以
	// RunLevelIndexForEngineLevel 按"完整版口径"反查仍命中——波数、种类名单永远跟引擎关走。
	static int			LevelForIndex(int theIndex);
	// 按时长档抽行的关号表：普通版取每场景第 1/5 关（=原快速表）、快速版取每场景第 5 关（批十改版）。
	static int			LevelForModeIndex(int theRunMode, int theIndex);

	// @pvz-online: 难度阶梯（M4-a）取用口。正在打的那一关的序号，口径与 GetLevel /
	// GetLevelSeed 一致（追赶期间 = 目标关）；场景档 0..4；难度 = mLadder 档的千分比表
	// （五档可调，2026-10-14 用户令，表见 LadderPermilleFor）——现档每过一个场景血量与
	// 数量同乘 ×1.33，其余四档 ×1/×1.2/×1.5/×2。
	// 无尽档：min(1.1^(n-1), (1+ln n)²) 全量乘数（2026-10-12 用户定案，见 EndlessDifficultyPermille；
	// 不吃阶梯档）。
	// 全整数运算，联机两端逐位一致（见 GetDifficultyPermille 实现处的说明）。
	int					GetPlayingLevelIndex() const;
	int					GetSceneIndex() const;
	int					GetDifficultyPermille() const;
    // @pvz-online: only StartNew opts in; loading missing/old markers restores legacy rules.
    int mCrowdRule = CrowdRules::LEGACY;
    bool UsesCrowdCompression() const { return mCrowdRule == CrowdRules::ABOVE_80_HALF; }
};

// 池子 9 条的展示文案（投票框与结果横幅共用，2026-10-14）：名字按当前语言择一（中文档
// 中文名、英文档英文名），说明一句、UTF-8。
// 分工照 GetRunChoiceName / GetRunChoiceDisplayName 的老规矩：GetZombieBuffName 恒 ASCII
// （石材按钮的位图字体回退路径只认 Latin）；显示名走宽字符直绘（CjkStoneButton / ModText）。
const char* GetZombieBuffName(int theId);
const char* GetZombieBuffDisplayName(int theId);
const char* GetZombieBuffDesc(int theId);

#endif
