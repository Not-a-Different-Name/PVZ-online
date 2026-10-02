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
// 写入时机 = 每关开始时："正在打的这一关"——中途退出再续就是重打这一关。

class RunState
{
public:
	static const int	RUN_LEVEL_COUNT		= 5;
	static const int	RUN_SEED_SLOTS		= 8;	// 种子槽固定 8 格（覆盖原版 mPurchases+6 规则）
	static const int	RUN_POOL_MAX		= 48;	// 卡池上限 = 全部植物

	// buff 的三选一还没做（R2 的屏 + R3 的数值），这里先把数据结构定下来：id + 叠了几层。
	// 检查点格式一次写全，R2/R3 只填内容、不动 IO。
	struct BuffStack
	{
		unsigned short	mId;
		unsigned short	mCount;
	};

public:
	// @pvz-online 内存态：这一局正在打（含"刚过关、正要进下一关"的空档）。
	// 回主菜单 = LawnApp 把这个对象删掉；检查点留在盘上，续关时重新读出来。
	int							mRunSeed;		// 这一局的种子：每关波表的种子由它推导，重开同一关不变
	int							mLevelIndex;	// 0..4 = 当前（或待打的）关序号；>= RUN_LEVEL_COUNT = 已通关
	std::vector<SeedType>		mPool;			// 这一局的卡池（按加入顺序；起始 = 向日葵 + 豌豆射手）
	std::vector<BuffStack>		mBuffs;			// 这一局拿到的 buff（同名可叠加）
	int							mFailCounts[RUN_LEVEL_COUNT];	// 每关失败次数（首版只存不用，平衡阶段再定惩罚）

public:
	RunState();

	// 全新一局：卡池回到两株、失败计数清零、从第 1 关开打。
	void				StartNew(int theRunSeed);

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

	// 当前关：mLevel 值的映射（1/3/5/7/9）与波表种子。序号越界返回 -1 / 0。
	int					GetLevel() const;
	int					GetLevelSeed() const;
	static int			LevelForIndex(int theIndex);
};

#endif
