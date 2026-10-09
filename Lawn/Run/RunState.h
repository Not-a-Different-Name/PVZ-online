#ifndef __RUNSTATE_H__
#define __RUNSTATE_H__

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
// 写入时机 = 每关的选卡做完、开场之前（LawnApp::UpdateRunPick 的 ①）：盘上的那一关
// 永远是一关"手里牌齐了、能直接开打"的状态。中途退出再续就是从这一关重开——选卡
// 已经进池，不会重抽、更不会丢；退在选卡屏里才回上一关重打（那一屏重新抽）。

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

	static const int	RUN_LEVEL_COUNT		= 25;	// 完整版总关数 = 5 场景 × 5 关（数组/静态表的尺寸上限）
	static const int	RUN_SCENE_COUNT		= 5;	// 白天 → 夜 → 泳池 → 迷雾 → 屋顶
	static const int	RUN_LEVELS_PER_SCENE = RUN_LEVEL_COUNT / RUN_SCENE_COUNT;
	// 无尽档的"总关数"：软上限，实际不可达——u16 线传（START_LEVEL 的关序号）与检查点
	// 校验都按它封口，让 mLevelIndex 永远合法（IsComplete 对无尽档恒假，见下）。
	static const int	RUN_ENDLESS_LEVEL_COUNT	= 60000;
	static const int	RUN_SEED_SLOTS		= 8;	// 种子槽固定 8 格（覆盖原版 mPurchases+6 规则）
	static const int	RUN_POOL_MAX		= 48;	// 卡池上限 = 全部植物
	static const int	RUN_CHOICES			= 3;	// 一屏摆几张卡
	// @pvz-online: mBuffChoices 的空缺哨兵（方案 §2.4 的防御守卫）：候选不足三条时多出来的
	// 格子填它——不是合法 id（全局 9 + 单株 48 + 第二 buff 39 都够不到），选择函数与
	// 屏上按钮一律忽略。
	static const unsigned short RUN_BUFF_CHOICE_NONE = 0xFFFF;

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

	// @pvz-online: "三选一"的待选状态（R2）。只在内存里活着，不进检查点——
	// 选了才写进卡池 / buff 表；检查点只在选卡做完的那一刻落盘（见头注释的写入时机）。
	// 所以退在选卡屏里，这一屏的欠账不恢复：从上一关重打，回到这一关时重新抽。
	// 候选由 runSeed + mPickSalt 推导（用户 2026-10-03 定案）：盐是本机自己随机的，
	// 不随联机命令走——联机里每个玩家看到的候选各不相同，选什么更是各选各的。
	int							mPendingPlantPicks;		// 还欠几株新植物（一屏只选一株，选完减一）
	int							mPendingBuffPicks;		// 还欠几个增益
	SeedType					mPlantChoices[RUN_CHOICES];
	unsigned short				mBuffChoices[RUN_CHOICES];	// 三条候选 id（缺格 = RUN_BUFF_CHOICE_NONE）
	unsigned int				mPickCounter;			// 抽过几次：同一局里每屏的候选都不一样
	unsigned int				mPickSalt;				// 本机随机盐：StartNew/Load 时各生成一次，进程内不变——
														// 同一局里重开同关还是同一组三条（防刷），换进程/换机器就不同
	// @pvz-online: 「换一批」的记账（2026-10-08 玩家反馈）。和上面一样纯内存态、不落检查点：
	// mChoicesRolled = 当前这一屏的三条候选已经抽好（打掉/放弃后复位，下一屏才会重抽——
	// 也让「换一批」重开的那一屏不会被再抽一次）；mPickReRolled = 这一屏的刷新机会用掉没有。
	bool						mChoicesRolled;
	bool						mPickReRolled;

public:
	// @pvz-online 内存态：这一局正在打（含"刚过关、正要进下一关"的空档）。
	// 回主菜单 = LawnApp 把这个对象删掉；检查点留在盘上，续关时重新读出来。
	int							mRunSeed;		// 这一局的种子：每关波表的种子由它推导，重开同一关不变
	int							mMode;			// 时长档（RUN_MODE_*）：决定关卡表抽行与每关后的奖励屏数
	int							mDiff;			// 出怪难度档（RUN_DIFF_*）：全队出怪总旋钮，乘在顺位乘数上
	// @pvz-online: 无尽档锁定的场景（0..4，仅 RUN_MODE_ENDLESS 用；其余档恒 0）。入口链定死，
	// 随 START_LEVEL 广播（MOD_BUILD 37）与检查点 v12 走；三条进路（建档/线传/读档）都验 0..4。
	int							mEndlessScene;
	int							mLevelIndex;	// 0..(关数-1) = 当前（或待打的）关序号；>= 关数 = 已通关（关数见 GetLevelCount）
	std::vector<SeedType>		mPool;			// 这一局的卡池（按加入顺序；起始 = 向日葵 + 豌豆射手）
	std::vector<BuffStack>		mBuffs;			// 这一局拿到的 buff（同名可叠加）
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
	// @pvz-online: 补发追赶的目标关序号（R5）。队友没有检查点 / 检查点落后于主机时，
	// 不是"跳到主机的关"，而是从这一局的起点一屏一屏地把欠下的三选一补齐——
	// 补做的屏与真打过的一模一样（候选由 runSeed + 关序号推导）。开着的时候
	// IsCatchingUp() 为真，UpdateRunPick 每选完一屏就 AdvanceCatchUp 挪一关。
	int							mCatchUpLevel;

public:
	RunState();

	// 全新一局：卡池回到两株、失败计数清零、从第 1 关开打。
	// 无尽档（theRunMode == RUN_MODE_ENDLESS）：theEndlessScene 锁场景（0..4，越界钳 0）。
	void				StartNew(int theRunSeed, int theRunMode = RUN_MODE_FULL, int theRunDiff = RUN_DIFF_STD,
							int theRunScale = RUN_SCALE_STD, int theRunTempo = RUN_TEMPO_STD, int theZombotany = 0,
							int theEndlessScene = 0);

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

	// 该选植物 / 该选 buff 了（一局开始时先挑四株 + 两个增益——进第 1 关前手里就有 6 株；
	// 快速版开局加厚成六株 + 四个增益，批十 2026-10-05）。每过一关再挑一批：植物按时长档
	// 倍乘（完整 2 / 普通 6 / 快速 10 株），增益每关收尾的发屏数按时长档定（2026-10-08
	// 玩家反馈批：普通固定 3、快速 3/4/5/5、完整保持 3→7 封顶，见 BeginLevelEndPicks）。
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
	// 增益屏的第 1 格保底一条全局增益（批十 2026-10-05；全局全封顶时自然让位）。
	// 每抽一次 mPickCounter 推一步——「换一批」重抽也走这里，所以重抽的三条必然≠刚才那三条。
	void				RollChoices();
	// @pvz-online: 当前屏的候选抽没抽（LawnApp::UpdateRunPick 靠它别把「换一批」重开的屏再抽一遍）。
	bool				ChoicesRolled() const { return mChoicesRolled; }
	// @pvz-online: 「换一批」（2026-10-08 玩家反馈定案）：这一屏重抽一次，玩家每屏多一次
	// 刷新机会——每屏限一次，用过的屏再点也没效果（屏上的按钮同时置为不可点）。
	void				RerollChoices();
	// 玩家点了第 theIndex 张卡：植物进卡池、buff 叠一层，各欠的数减一。
	void				TakePlantChoice(int theIndex);
	void				TakeBuffChoice(int theIndex);
	// @pvz-online: 玩家点了「放弃」（2026-10-03 用户定案）：这一屏不选也不要——欠的屏数
	// 照减（先植物后增益，与 IsPlantPick 的先后一致），卡池 / buff 表原样不动。
	void				SkipPendingPick();
	// 这一局拿到某个 buff 的层数（R3 的数值层按它算加成）。
	int					GetBuffCount(int theBuffId) const;
	// 这株植物在不在这局的卡池里。选卡界面（卡池 > 8 格才弹）靠它决定哪些袋子
	// 画得出来、点得动——见 LawnApp::SeedTypeAvailable 的闯关分支。
	bool				HasPlant(SeedType theSeedType) const;

	// 检查点读写。Load 失败（文件不在 / 版本不符 / 内容越界）返回 false，对象保持"空局"。
	bool				Load(int theProfileId);
	bool				Save(int theProfileId) const;
	static bool			HasCheckpoint(int theProfileId);
	// @pvz-online: 探读检查点（2026-10-10）：完整走一遍 Load（含全部校验），成功才回填
	// 时长档与无尽场景，返回 true。两处「续不续」的问句先用它甄别——HasCheckpoint 只看
	// 文件在不在，而 ContinueRun 的兜底是 StartNew 完整档，探错会悄悄开成普通局。
	static bool			PeekCheckpoint(int theProfileId, int& theRunMode, int& theEndlessScene);
	// 一局打完了就删掉：那份检查点代表的那一局已经结束，留着只会让下次开局多问一句
	// "续不续"（续了也是从头开）。
	static void			DeleteCheckpoint(int theProfileId);
	static std::string	GetCheckpointName(int theProfileId);

	// 无尽档没有"打完"这回事（2026-10-10 用户定案：无奖杯/结算屏，关序号只涨不封）；其余档
	// 在关序号走到关数时收场。
	bool				IsComplete() const { return mMode != RUN_MODE_ENDLESS && mLevelIndex >= GetLevelCount(); }
	// @pvz-online: 无尽档判定（无尽续草坪批）：草坪原地续关的几个分叉口都问这一句。
	bool				IsEndless() const { return mMode == RUN_MODE_ENDLESS; }
	void				AdvanceLevel() { mLevelIndex++; }

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
	// GetLevelSeed 一致（追赶期间 = 目标关）；场景档 0..4；难度 = 千分比表
	// {1000,1330,1768,2352,3129}——每过一个场景血量与数量同乘 ×1.33（2026-10-05 由 ×1.5 回调）。
	// 无尽档：场景恒为 mEndlessScene，难度 = 该场景基准 + 600·ln(1+关序号) 的对数阶增量。
	// 全整数运算，联机两端逐位一致（见 GetDifficultyPermille 实现处的说明）。
	int					GetPlayingLevelIndex() const;
	int					GetSceneIndex() const;
	int					GetDifficultyPermille() const;
};

#endif
