#ifndef __RUNBUFFS_H__
#define __RUNBUFFS_H__

// @pvz-online: 闯关（肉鸽）全局增益的目录。
//
// 三选一屏（R2）只用这里的名字和说明文字；效果真正落到棋盘上是 R3 的活儿——
// 到时候在各自的效果点读 RunState::GetBuffCount(id) 就行，这张目录本身不用动。
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
};

const RunBuffDef& GetRunBuffDef(int theId);

#endif
