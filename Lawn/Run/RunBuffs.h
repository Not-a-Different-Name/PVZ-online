#ifndef __RUNBUFFS_H__
#define __RUNBUFFS_H__

// @pvz-online: 闯关（肉鸽）全局增益的目录。
//
// 这张表既是文案（三选一屏 R2 用它摆名字和说明），也是数值（R3：每层多少，
// 落点各自的代码位置见 RunBuffDef 两个数值字段的注释）。
// 另一类"单株升级"（只对已拥有的植物出）是 R3 的另一张小表，不在这个文件里。

enum RunBuffId
{
	RUN_BUFF_FIREPOWER,		// 火力强化：全体子弹伤害 +10%
	RUN_BUFF_ROOTED,		// 扎根：全体植物血量 +20%
	RUN_BUFF_ABUNDANCE,		// 丰饶：产阳光间隔 −20%
	RUN_BUFF_SWIFT,			// 急袭：攻击间隔 −10%
	RUN_BUFF_FASTSEED,		// 速种：种植冷却 −15%
	RUN_BUFF_RESERVE,		// 储备：每关开局阳光 +25
	RUN_BUFF_SKYFALL,		// 天降：天上掉阳光间隔 −20%
	RUN_BUFF_BLAST,			// 爆破：一次性植物伤害 +30%
	RUN_BUFF_COUNT
};

struct RunBuffDef
{
	const char*	mName;
	const char*	mDesc;
	// 每层的乘数修正：最终乘数 = 1 + mPerStackMul × 层数（+0.10 = ×1.10，−0.20 = ×0.80）。
	// 0 = 这条不是乘数型。取用走 LawnApp::RunBuffMul，非闯关局自动是 1.0。
	float		mPerStackMul;
	// 每层的绝对值加成（储备 +25 阳光）。0 = 不是加成型。取用走 LawnApp::RunBuffAdd。
	int			mPerStackAdd;
};

const RunBuffDef& GetRunBuffDef(int theId);

#endif
