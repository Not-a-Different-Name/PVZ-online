#ifndef __RUNSTATE_H__
#define __RUNSTATE_H__

#include <string>
#include <vector>
#include "../../ConstEnums.h"

// @pvz-online: 全流程闯关（肉鸽）的本地状态 + 检查点文件。
//
// 一局 = 5 关：玩家看到的是白天第 2/4/6/8/10 关，喂给引擎的 mLevel 是 1/3/5/7/9
// （场景与难度只由关卡号推出，所以"第几关"就是这一串序号）。
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
	static const int	RUN_LEVEL_COUNT		= 5;
	static const int	RUN_SEED_SLOTS		= 8;	// 种子槽固定 8 格（覆盖原版 mPurchases+6 规则）
	static const int	RUN_POOL_MAX		= 48;	// 卡池上限 = 全部植物
	static const int	RUN_CHOICES			= 3;	// 一屏摆几张卡

	// buff 的三选一屏是 R2 的活儿、数值落地是 R3 的：数据类型一层就该定死，id + 叠了几层。
	struct BuffStack
	{
		unsigned short	mId;
		unsigned short	mCount;
	};

	// @pvz-online: "三选一"的待选状态（R2）。只在内存里活着，不进检查点——
	// 选了才写进卡池 / buff 表；检查点只在选卡做完的那一刻落盘（见头注释的写入时机）。
	// 所以退在选卡屏里，这一屏的欠账不恢复：从上一关重打，回到这一关时重新抽。
	// 候选由 runSeed 推导，抽出来还是同一组三条，玩家不会因此占便宜也不会吃亏。
	int							mPendingPlantPicks;		// 还欠几株新植物（一屏只选一株，选完减一）
	int							mPendingBuffPicks;		// 还欠几个增益
	SeedType					mPlantChoices[RUN_CHOICES];
	unsigned short				mBuffChoices[RUN_CHOICES];
	unsigned int				mPickCounter;			// 抽过几次：同一局里每屏的候选都不一样

public:
	// @pvz-online 内存态：这一局正在打（含"刚过关、正要进下一关"的空档）。
	// 回主菜单 = LawnApp 把这个对象删掉；检查点留在盘上，续关时重新读出来。
	int							mRunSeed;		// 这一局的种子：每关波表的种子由它推导，重开同一关不变
	int							mLevelIndex;	// 0..4 = 当前（或待打的）关序号；>= RUN_LEVEL_COUNT = 已通关
	std::vector<SeedType>		mPool;			// 这一局的卡池（按加入顺序；起始 = 向日葵 + 豌豆射手）
	std::vector<BuffStack>		mBuffs;			// 这一局拿到的 buff（同名可叠加）
	int							mFailCounts[RUN_LEVEL_COUNT];	// 每关失败次数（首版只存不用，平衡阶段再定惩罚）
	// @pvz-online: 补发追赶的目标关序号（R5）。队友没有检查点 / 检查点落后于主机时，
	// 不是"跳到主机的关"，而是从这一局的起点一屏一屏地把欠下的三选一补齐——
	// 补做的屏与真打过的一模一样（候选由 runSeed + 关序号推导）。开着的时候
	// IsCatchingUp() 为真，UpdateRunPick 每选完一屏就 AdvanceCatchUp 挪一关。
	int							mCatchUpLevel;

public:
	RunState();

	// 全新一局：卡池回到两株、失败计数清零、从第 1 关开打。
	void				StartNew(int theRunSeed);

	// 该选植物 / 该选 buff 了（一局开始时先挑两株——进第 1 关前手里就有 4 株；
	// 每过一关再挑两株 + 一个增益）。只负责"欠几屏"，候选由 RollChoices 现抽。
	void				BeginStartPicks();
	void				BeginLevelEndPicks();
	bool				HasPendingPick() const { return mPendingPlantPicks > 0 || mPendingBuffPicks > 0; }
	// 这一屏发的是植物（true）还是 buff（false）。
	bool				IsPlantPick() const { return mPendingPlantPicks > 0; }
	// 抽当前这一屏的三条候选，摆在 mPlantChoices / mBuffChoices 里等玩家点。
	void				RollChoices();
	// 玩家点了第 theIndex 张卡：植物进卡池、buff 叠一层，各欠的数减一。
	void				TakePlantChoice(int theIndex);
	void				TakeBuffChoice(int theIndex);
	// 这一局拿到某个 buff 的层数（R3 的数值层按它算加成）。
	int					GetBuffCount(int theBuffId) const;
	// 这株植物在不在这局的卡池里。选卡界面（卡池 > 8 格才弹）靠它决定哪些袋子
	// 画得出来、点得动——见 LawnApp::SeedTypeAvailable 的闯关分支。
	bool				HasPlant(SeedType theSeedType) const;

	// 检查点读写。Load 失败（文件不在 / 版本不符 / 内容越界）返回 false，对象保持"空局"。
	bool				Load(int theProfileId);
	bool				Save(int theProfileId) const;
	static bool			HasCheckpoint(int theProfileId);
	// 一局打完了就删掉：那份检查点代表的那一局已经结束，留着只会让下次开局多问一句
	// "续不续"（续了也是从头开）。
	static void			DeleteCheckpoint(int theProfileId);
	static std::string	GetCheckpointName(int theProfileId);

	bool				IsComplete() const { return mLevelIndex >= RUN_LEVEL_COUNT; }
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

	// 当前关：mLevel 值的映射（1/3/5/7/9）与波表种子。序号越界返回 -1 / 0。
	int					GetLevel() const;
	int					GetLevelSeed() const;
	static int			LevelForIndex(int theIndex);
};

#endif
