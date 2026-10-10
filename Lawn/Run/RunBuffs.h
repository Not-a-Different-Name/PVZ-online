#ifndef __RUNBUFFS_H__
#define __RUNBUFFS_H__

#include "../../ConstEnums.h"

// @pvz-online: 闯关（肉鸽）增益目录，两类都在这：
//   ① 全局增益（RUN_BUFF_*，对全部植物/整局生效）
//   ② 单株升级（RUN_UPGRADE_*，只对已拥有的那株植物生效，抽取时过滤）
// 两张表既是文案（三选一屏 R2 用它摆名字和说明），也是数值（R3：每层多少，
// 落点各自的代码位置见 RunBuffDef 数值字段与 LawnApp::RunPlantUpgradeMul/Count）。
//
// 两类共用同一个 id 空间：id < RUN_BUFF_COUNT 是全局，否则 id − RUN_BUFF_COUNT
// 是单株表下标。存储（RunState 的 BuffStack）与检查点格式因此不用区分两类。
// 2026-10-09 批 0 起追加第二 buff 表（docs/06 §8.5）：id ≥ RUN_BUFF_COUNT +
// RUN_PLANT_UPGRADE_COUNT 是第二表下标——同株的第二条词条，id = 两表计数 + 下标。
// 2026-10-03 起条目多了两个维度（方案 docs/06 §2）：层数上限（mMaxStacks，叠满不再
// 进候选）与叠乘模式（mMultiplicative，(1+每层)^层数）——都是表内静态属性，不进检查点。

enum RunBuffId
{
	RUN_BUFF_FIREPOWER,		// 火力强化：全体子弹伤害 +30%（无限）
	RUN_BUFF_ROOTED,		// 扎根：全体植物血量 +75%（无限；2026-10-04 批七由 +150% 下调——只改扎根，单株血量行不动）
	RUN_BUFF_ABUNDANCE,		// 丰饶：产阳光间隔叠乘 ×0.75/层（至多 4 层；2026-10-09 全局批由线性 −15%/层 改叠乘）
	RUN_BUFF_SWIFT,			// 急袭：攻击间隔叠乘 ×0.75/层（无限；2026-10-09 全局批由 ×0.8 上调）
	RUN_BUFF_FASTSEED,		// 速种：种植冷却叠乘 ×0.8/层（无限）
	RUN_BUFF_SKYFALL,		// 天降：夜晚也降阳光 + 降阳光速率 ×2（只可选 1 层；2026-10-06 整条重做、2026-10-09 全局批 ×4 下调、当晚实机反馈 ×3→×2）
	RUN_BUFF_BLAST,			// 爆破：一次性植物伤害 +60%（无限；2026-10-05 批八由 +30% 上调）
	RUN_BUFF_PRECISION,		// 精准：全体伤害 +15%（无限；2026-10-04 批 18 追加为 id 8）
	RUN_BUFF_MIRROR,		// 排山倒海：种下植物时上下相邻空格免费种同款（只可选 1 层；2026-10-09 批 A 追加为 id 8）
	// 2026-10-06（用户指令，见 docs/07 同日条目）：「储备」（原 id 5）删除——全局 9→8 条，
	// 单株 id 由「9 + 下标」全体左移 1 变「8 + 下标」（检查点 v8 迁移读平）；开局阳光改按
	// 上座顺位给（LawnApp::OnlineStartSunBonus，挂点 Board::InitLevel）。
	RUN_BUFF_COUNT
};

struct RunBuffDef
{
	const char*	mName;
	const char*	mDesc;		// 中文说明（UTF-8，宽字符路径绘制）
	const char*	mDescEn;	// 英文说明（同上；语言批 2026-10-03 起按 ModText::IsChinese 择一）
	// 每层的乘数修正：线性条目 = 1 + mPerStackMul × 层数（+0.10 = ×1.10，−0.20 = ×0.80）；
	// 叠乘条目（mMultiplicative）= (1 + mPerStackMul)^层数。0 = 这条不是乘数型。
	// 取用走 LawnApp::RunBuffMul，非闯关局自动是 1.0。
	float		mPerStackMul;
	// 每层的绝对值加成。0 = 不是加成型。取用走 LawnApp::RunBuffAdd——当前无条目使用
	// （原「储备」专用；该条 2026-10-06 删除后留机制备用）。
	int			mPerStackAdd;
	// @pvz-online: 层数上限：叠到这么多层后不再进候选（0 = 无限）。已超限的旧档保留层数、
	// 不追溯削减；过滤在 RunState::RollChoices。
	int			mMaxStacks;
	// @pvz-online: 叠乘模式（方案 §2.3）：true = 层数按 (1+mPerStackMul)^层数 几何叠乘。
	bool		mMultiplicative;
	// @pvz-online: 稀有度档位（docs/06 §8.7，权重批）：1/2/3★；0 = 未定档（中性权重，
	// 管线零行为）。档位基值 1★=12 / 2★=6 / 3★=1（2026-10-09 批 A：其余档翻倍、3★ 不动
	// = 3★ 相对出率减半），全局条再乘 k、经济条再乘 e、1★/2★ 全局条再减半（常量见下）。
	int			mRarity;
};

const RunBuffDef& GetRunBuffDef(int theId);

// ── 单株升级（R3 第二张表）────────────────────────────────────────────
// 只对"卡池里已有这株植物"的玩家出（抽取时过滤，见 RunState::RollChoices）。
// 落点取用走 LawnApp::RunPlantUpgradeMul / RunPlantUpgradeCount——和全局 buff
// 一样，非闯关局自动是中性值，落点不需要判 mRunState。
// @pvz-online: 表按 SeedType 升序维护（方案 docs/06 §2.1 / Q7）——单株 id = RUN_BUFF_COUNT
// + 表内下标，所以前 5 条（SeedType 0..4）必须永远留在原位：老检查点里的 id 直接按它解读。
// （2026-10-04 批 18 起 RUN_BUFF_COUNT 8→9，单株 id 全体右移 1；旧档按 v6 迁移读平。
// 2026-10-06 起 RUN_BUFF_COUNT 9→8——储备删除，单株 id 全体左移 1；旧档按 v8 迁移读平。
// 2026-10-09 起 RUN_BUFF_COUNT 8→9——全局表尾追加「排山倒海」，单株 id 全体右移 1；
// 旧档按 v9 迁移读平。）
// 新增条目插在自己的 SeedType 位次上：批 1 的 5 条是追加（SeedType 都大于 4），批 2/批 3/
// 批 4/批 5/批 6/批 7 的 16 条、批 8 的地刺 1 条、批 9 的磁系 2 条、批 10 的投手族 4 条、
// 批 11 的计时/产出族 4 条、批 12 的弹道/索敌族 3 条与批 13 的控制/减速族 4 条插进中段——
// 每插一批，更早批次档里单株 id 的层数就会错位到别的植物（开发期接受，见方案 §六）；
// 满编 48 条后表下标 == SeedType。
// 2026-10-04 批 18：整条替换 9 行（寒冰射手/双发/大喷菇/卷心菜/玉米投手/大蒜/西瓜/机枪/冰西瓜，
// 表下标 == SeedType 不动，仅整行换语义）；全局表 8→9 条（追加「Precision」为 id 8）——
// 单株 id 全体右移 1，旧检查点按 v6 迁移（RunState::Load：aVersion<6 时 id≥8 均 +1）。
enum RunPlantUpgradeId
{
	RUN_UPGRADE_PEASHOOTER,		// 豌豆射手：每次多打 1 发（每层）
	RUN_UPGRADE_SUNFLOWER,		// 向日葵：丰收——种下立即产 1 次阳光 + 每轮 50% 概率多产 1 阳光（只可选 1 层；2026-10-09 经济批重做）
	RUN_UPGRADE_CHERRYBOMB,		// 樱桃炸弹：爆炸半径 +50%/层（2026-10-05 批八由 +25% 上调）
	RUN_UPGRADE_WALLNUT,		// 坚果墙：巨人砸击时像地刺王一样耐砸（每次 -200 血；只可选 1 层；2026-10-04 批七由血量族整条换掉）
	RUN_UPGRADE_POTATOMINE,		// 土豆雷：震雷——爆炸眩晕半径内僵尸 2 秒/层（至多 2 层；2026-10-09 权重批由半径族整条换掉）
	RUN_UPGRADE_SNOWPEA,		// 寒冰射手：发射冰西瓜（只可选 1 层；2026-10-04 批 18 由减速时长族整条换掉）
	RUN_UPGRADE_CHOMPER,		// 大嘴花：咀嚼时间减半（只可选 1 层）
	RUN_UPGRADE_REPEATER,		// 双发：每次射击多发 2 颗（无上限；2026-10-04 批 18 由射击间隔族整条换掉）
	RUN_UPGRADE_PUFFSHROOM,		// 小喷菇：每次多发 2 颗/层（无上限；2026-10-04 由「射程变为无限」改，射程挂点已还原）
	RUN_UPGRADE_SUNSHROOM,		// 阳光菇：亮顶——产阳光间隔 ×0.75/层 叠乘（至多 2 层；2026-10-09 经济批由概率多产改速度轴）
	RUN_UPGRADE_FUMESHROOM,		// 大喷菇：攻击间隔 ×0.5（只可选 1 层；2026-10-04 批 18 由射程族整条换掉，2026-10-09 权重批 ×0.25→×0.5）
	RUN_UPGRADE_GRAVEBUSTER,	// 墓碑吞噬者：吞掉墓碑额外产 25 阳光（每层，至多 2 层；经济批 2026-10-09 删条不进池，表行保留供存量档）
	RUN_UPGRADE_HYPNOSHROOM,	// 魅惑菇：被魅惑僵尸咬到的僵尸也变友军（只可选 1 层）
	RUN_UPGRADE_SCAREDYSHROOM,	// 胆小菇：敌人贴近时不再缩头（只可选 1 层）
	RUN_UPGRADE_ICESHROOM,		// 寒冰菇：全场冰冻 +4 秒/层（至多 2 层；2026-10-05 批八由 +2 秒上调）
	RUN_UPGRADE_DOOMSHROOM,		// 毁灭菇：爆炸直伤 50000（只可选 1 层；2026-10-05 批八b 由半径族整条换掉）
	RUN_UPGRADE_LILYPAD,		// 睡莲：血量 +150%/层（至多 2 层；2026-10-09 审计批删条不进池，表行保留供存量档）
	RUN_UPGRADE_SQUASH,			// 窝瓜：砸击次数 +2/层（至多 2 层；2026-10-05 批八由眩晕族整条换掉，UpdateSquash 多段砸击）
	RUN_UPGRADE_THREEPEATER,	// 三线射手：每条道多发 1 颗（只可选 1 层；2026-10-09 权重批 cap2→cap1）
	RUN_UPGRADE_TANGLEKELP,		// 缠绕海草：每层多缠 1 只僵尸（至多 2 层）
	RUN_UPGRADE_JALAPENO,		// 火爆辣椒：种植冷却 ×0.8/层（至多 3 层；2026-10-09 权重批 ×0.75→×0.8）
	RUN_UPGRADE_SPIKEWEED,		// 地刺：扎过的僵尸减速 +3 秒/层（至多 2 层）
	RUN_UPGRADE_TORCHWOOD,		// 火炬树桩：火弹伤害 +50%/层（至多 2 层）
	RUN_UPGRADE_TALLNUT,		// 高坚果：巨人砸击时像地刺王一样耐砸（每次 -200 血；只可选 1 层；2026-10-04 批七由血量族整条换掉）
	RUN_UPGRADE_SEASHROOM,		// 海蘑菇：种植冷却 ×0.8/层（至多 3 层；2026-10-09 权重批 ×0.75→×0.8）
	RUN_UPGRADE_PLANTERN,		// 路灯花：照亮范围 +1 格/层（至多 2 层；2026-10-09 审计批 2★→1★）
	RUN_UPGRADE_CACTUS,			// 仙人掌：尖刺穿透 +1 只/层（至多 2 层）
	RUN_UPGRADE_BLOVER,			// 三叶草：吹风后全场僵尸减速 +5 秒/层（至多 2 层）
	RUN_UPGRADE_SPLITPEA,		// 分裂豌豆：背向豌豆每次 +1 颗/层（至多 2 层）
	RUN_UPGRADE_STARFRUIT,		// 杨桃：子弹变为追踪弹（只可选 1 层；2026-10-04 由射速族整条换掉）
	RUN_UPGRADE_PUMPKINSHELL,	// 南瓜头：血量 +150%/层（至多 2 层；2026-10-09 权重批 cap3→cap2）
	RUN_UPGRADE_MAGNETSHROOM,	// 磁力菇：吸取间隔 ×0.8/层（至多 3 层；2026-10-09 权重批 ×0.75→×0.8）
	RUN_UPGRADE_CABBAGEPULT,	// 卷心菜投手：种植费用变为 0（只可选 1 层；2026-10-04 批 18 由投掷间隔族整条换掉）
	RUN_UPGRADE_FLOWERPOT,		// 花盆：血量 +150%/层（至多 2 层；2026-10-09 审计批删条不进池，表行保留供存量档）
	RUN_UPGRADE_KERNELPULT,		// 玉米投手：种下后变成玉米加农炮（只可选 1 层；2026-10-04 批 18 由黄油率族整条换掉）
	RUN_UPGRADE_INSTANT_COFFEE,	// 咖啡豆：唤醒产阳光 +25/层（至多 2 层）
	RUN_UPGRADE_GARLIC,			// 大蒜：巨人砸击时像地刺王一样耐砸（只可选 1 层；2026-10-04 批 18 由减速族整条换掉）
	RUN_UPGRADE_UMBRELLA,		// 保护伞：血量 +150%/层（至多 2 层；2026-10-09 权重批 cap3→cap2）
	RUN_UPGRADE_MARIGOLD,		// 金盏花：每次多产 1 枚（每层，至多 3 层）
	RUN_UPGRADE_MELONPULT,		// 西瓜投手：每次多发 1 个西瓜（无上限；2026-10-04 批 18 由溅射范围族整条换掉）
	RUN_UPGRADE_GATLINGPEA,		// 机枪射手：攻击间隔 ×0.5（只可选 1 层；2026-10-04 批 18 由射击间隔族整条换掉）
	RUN_UPGRADE_TWINSUNFLOWER,	// 双子向日葵：绽放——每轮固定多产 2 阳光/层（至多 2 层；2026-10-09 经济批由概率多产改加量轴）
	RUN_UPGRADE_GLOOMSHROOM,	// 忧郁菇：光环范围 +1 格（只可选 1 层；2026-10-04 上限 2→1）
	RUN_UPGRADE_CATTAIL,		// 猫尾草：攻击目标 +1 个/层（至多 2 层）
	RUN_UPGRADE_WINTERMELON,	// 冰西瓜：溅射半径 +50%/层（无上限；2026-10-04 批 18 由减速时长族整条换掉）
	RUN_UPGRADE_GOLDMAGNET,		// 吸金磁：吸取间隔 ×0.8/层（至多 3 层；2026-10-09 权重批 ×0.75→×0.8）
	RUN_UPGRADE_SPIKEROCK,		// 地刺王：血量 +150%/层（至多 2 层；2026-10-04 批七换血量 +200% cap3，2026-10-09 权重批血量族统一回调）
	RUN_UPGRADE_COBCANNON,		// 玉米加农炮：装填时间 ×0.8/层（至多 3 层；2026-10-09 权重批 ×0.75→×0.8）
	RUN_PLANT_UPGRADE_COUNT
};

// @pvz-online: 单株条目的「语义标签」（修正批 2026-10-03）。背景：几个通用挂点
//（血量 Plant.cpp:484、射速节奏 :976、多发循环 :4856、种植冷却 SeedPacket.cpp:885）
// 直接拿该株的 RunPlantUpgradeMul/Count——**任何**行都会在这些口生效，于是一条语义
// 无关的条目会静默乘进去（例：双发 ×0.75 行会把双发血量每层也缩 25%；寒冰射手 +30%
// 行会把雪豆射速拉长）。现在挂点改走 RunPlantUpgradeMulKind/CountKind：只有 mKind
// 对上号的条目才生效，其余自动中性。新条目按效果选 Kind；专门挂点（磁力菇充能、
// 雪豆减速时长这类只被一处消费的）用 EFFECT + 通用 Mul/Count 即可。
enum RunPlantUpgradeKind
{
	RUN_UPGRADE_KIND_EFFECT = 0,	// 专项效果：各自挂点用通用 RunPlantUpgradeMul/Count
	RUN_UPGRADE_KIND_HEALTH,		// 血量型：Plant.cpp:484 通用血量挂点
	RUN_UPGRADE_KIND_RHYTHM,		// 射速节奏：Plant.cpp:976 UpdateShooter
	RUN_UPGRADE_KIND_SHOTCOUNT,		// 每次多发：Plant.cpp:4856 Fire 多发循环
	RUN_UPGRADE_KIND_COOLDOWN,		// 种植冷却：SeedPacket.cpp:885
};

struct RunPlantUpgradeDef
{
	SeedType	mPlant;			// 这门升级挂在哪种植物上（一株最多一条）
	const char*	mName;
	const char*	mDesc;			// 中文说明（UTF-8）
	const char*	mDescEn;		// 英文说明（UTF-8；同上按当前语言择一）
	// 同 RunBuffDef.mPerStackMul；纯计数型（+1 发 / +1 阳光）填 0，
	// 那种效果按层数直接取整，走 RunPlantUpgradeCount。
	float		mPerStackMul;
	// @pvz-online: 同 RunBuffDef.mMaxStacks（0 = 无限）与 mMultiplicative。
	int			mMaxStacks;
	bool		mMultiplicative;
	// @pvz-online: 语义标签（修正批）：只有 Kind 与挂点相符的条目才被该挂点消费。
	// 省略 = RUN_UPGRADE_KIND_EFFECT。
	RunPlantUpgradeKind	mKind;
	// @pvz-online: 稀有度档位（docs/06 §8.7）：1/2/3★；0 = 未定档（中性权重）。尾部省略 = 0。
	int			mRarity;
};

const RunPlantUpgradeDef& GetRunPlantUpgradeDef(int theIndex);
// 这株植物在单株表里的下标；表里没有这株 → −1。
int RunPlantUpgradeIndexFor(SeedType thePlant);

// 三选一屏统一取文案：两类 id 都能查，屏上不用分支。mName 各语言同字（英文名，
// 位图字体按钮用），说明按当前语言择一。
const char* GetRunChoiceName(int theId);
// 封顶条目（mMaxStacks > 0）的返回值尾部带语言相称的上限后缀（中文「，至多 N 层」/
// 英文 ", up to N stacks"，机械追加，见实现处）。
// 返回进程内静态缓冲——取到就画，别存指针。
const char* GetRunChoiceDesc(int theId);
// 这条条目封顶几层（0 = 无限）；抽取过滤（RunState::RollChoices）与屏上「已有 x/N」用它。
int GetRunChoiceMaxStacks(int theId);
// 这株的单株条目是否进抽取池（审计批 2026-10-09 弱词条删条：花盆/睡莲不进池；经济批
// 同日追加墓碑吞噬者。表行保留供存量档生效）。RunState::RollChoices 过滤用。
bool RunPlantUpgradeInPool(SeedType thePlant);

// @pvz-online: 稀有度/权重（权重批 2026-10-09，docs/06 §8.7）：这条条目的稀有度档位
// （0 = 未定档 / 1 / 2 / 3★）与抽取权重。未定档条目权重恒 1；两表已全量填档（75efd06），
// 档位基值 1★=12 / 2★=6 / 3★=1（批 A 翻倍口径）生效，全局条再乘 k、经济条再乘 e、
// 1★/2★ 全局条最后减半（三个独立旋钮）。同屏加权无放回、抽中即从候选摘除。
enum { RUN_GLOBAL_WEIGHT_K_PERMILLE = 2500 };	// 全局条权重系数 k，千分比（初值 2.5）
enum { RUN_ECON_WEIGHT_K_PERMILLE = 3000 };		// 经济条权重系数 e，千分比（经济权重批 2026-10-09 用户定案 e=3；
												// 作用清单 11 条见 RunBuffs.cpp::IsEconomyRunChoice，
												// 批 9 同日补第二表「免植」/「遗泽」）
enum { RUN_GLOBAL_LOWTIER_HALF_PERMILLE = 500 };// 1★/2★ 全局条最终减半，千分比（2026-10-10 用户令
												// 「1 2星全局buff权重降低至原来一半」；3★ 全局不动）
int GetRunChoiceRarity(int theId);
int GetRunChoiceWeight(int theId);

// 单株升级「这是哪株的」显示名（三选一屏列顶标题行 / 词条查看器用；按当前语言：
// 中文名 / 英文官方名）；全局增益 / 表里漏了名字 → NULL。
const char* GetRunChoicePlantName(int theId);
// 同一张名字表按 SeedType 直查（局内词条查看器手里只有 SeedType + 层数）；表里没有 → NULL。
const char* GetRunPlantName(SeedType thePlant);

// ── 第二 buff 表（docs/06 §8.5-§8.7 评审终稿，2026-10-09 批 0 结构）────────────
// 每株植物的第二条单株词条（39 条有效 + 9 株删条不进表）。表按 SeedType 升序紧凑
// 维护（删条株不占行——与老表「表行保留供存量档」不同：第二表没有历史存量，直接删）。
// id = RUN_BUFF_COUNT + RUN_PLANT_UPGRADE_COUNT + 表内下标（老 id 0..56 一字不动，
// 老检查点天然读平；新 id 只在新构建出现，检查点 v11 起才可能有）。
// 复用 RunPlantUpgradeDef 结构（字段语义相同）；mKind 同样只标能吃通用挂点的条目，
// 专属挂点条目用 EFFECT + 各自落地批的专属取用。
enum RunPlantBuff2Id
{
	RUN_BUFF2_PIERCE,		// #0 豌豆射手：穿刺豌豆——豌豆无限穿透（cap1，2★）
	RUN_BUFF2_FIRSTLIGHT,	// #1 向日葵：初阳——种下立即产 1 次阳光（cap1，2★；⚠与老条「丰收」同轴，暂不进池待定案）
	RUN_BUFF2_SCORCHED,		// #2 樱桃炸弹：焦土——被炸者每秒 300 伤燃烧 3 秒，火随人（cap2，2★）
	RUN_BUFF2_THORNS,		// #3 坚果墙：荆棘之壁——啃食者每秒受 40 伤（cap2，2★）
	RUN_BUFF2_MINEREGROW,	// #4 土豆雷：自动补雷——引爆后 3 秒原位重埋，每层多补 1 次（cap2，2★）
	RUN_BUFF2_FROSTPIERCE,	// #5 寒冰射手：寒冰贯通——雪豆无限穿透（cap1，2★；与老条「冰西瓜化」同株互斥，2026-10-10 起跨屏「持有即排除」）
	RUN_BUFF2_CHAINBITE,	// #6 大嘴花：连锁吞——吞完范围内仍有僵尸立即再咬，每层多连锁 1 次（cap2，2★）
	RUN_BUFF2_SUPPRESS,		// #7 双发射手：火力压制——攻击间隔 ×0.7/层（cap2，2★；走通用 RHYTHM 挂点）
	RUN_BUFF2_DEATHSPORE,	// #8 小喷菇：亡语孢子——被啃掉时原地爆炸 900 伤/层（cap2，2★）
	RUN_BUFF2_BOOSTGROW,	// #9 阳光菇：催长——种下后第二次产阳光前长大（首产 15、次产起 25）（cap1，2★）
	RUN_BUFF2_FETID,		// #10 大喷菇：腐臭之息——命中减速 20% 持续 3 秒（cap2，2★）
	RUN_BUFF2_SWIFTDIG,		// #11 墓碑吞噬者：疾掘——吞噬速度 ×0.5（cap2，1★）
	RUN_BUFF2_FRAGRANCE,	// #12 魅惑菇：迷魂香——被魅惑僵尸伤害 ×2/层（cap2，2★）
	RUN_BUFF2_BEDROCK,		// #13 胆小菇：磐石——血量变为 4000（cap1，1★）
	RUN_BUFF2_FROSTARMOR,	// #14 寒冰菇：霜甲——冰冻结束后全场减速 35% 持续 20 秒（cap1，3★）
	RUN_BUFF2_DOOMRADIUS,	// #15 毁灭菇：灭世半径——爆炸半径 +50%/层（cap2，2★）
	RUN_BUFF2_LEAP,			// #17 窝瓜：飞跃震击——砸击数 ×2/层 + 解锁上下行跳砸（cap2，2★）
	RUN_BUFF2_TRIDENT,		// #18 三线射手：贯穿——三线子弹无限穿透（cap1，2★）
	RUN_BUFF2_CHAINBURN,	// #20 火爆辣椒：连烧——爆炸次数 +1，间隔 1 秒（cap1，2★）
	RUN_BUFF2_OPENWOUND,	// #21 地刺：伤口加深——被扎者受全伤 +30%（cap2，2★）
	RUN_BUFF2_FLAMESPLASH,	// #22 火炬树桩：烈焰溅射——过火子弹带小溅射（原伤 1/3）（cap1，2★）
	RUN_BUFF2_DEATHBRINE,	// #24 海蘑菇：亡语孢子——被啃掉时原地爆炸 900 伤/层（cap2，2★）
	RUN_BUFF2_TWILIGHT,		// #25 路灯花：暮光——照亮格内植物攻速 ×0.75（cap2，2★）
	RUN_BUFF2_TRISPIKE,		// #26 仙人掌：三向尖刺——改为三行发射（本行 ±1）（cap1，2★）
	RUN_BUFF2_GALE,			// #27 三叶草：狂风——吹风把全场僵尸推离 1 格/层（cap2，2★）
	RUN_BUFF2_FREEPLANT,	// #28 分裂豌豆：免植——种下 0 费（cap1，1★）
	RUN_BUFF2_REGROW,		// #30 南瓜头：再生——每 15 秒回复 10% 血（cap2，2★）
	RUN_BUFF2_MAGNETTHROW,	// #31 磁力菇：卸甲飞掷——吸到的防具掷回，1200 伤/层（cap2，2★）
	RUN_BUFF2_HEAVYTOSS,	// #32 卷心菜投手：重投——伤害 +50%（cap3，1★）
	RUN_BUFF2_BUTTERFEAST,	// #34 玉米投手：黄油盛宴——黄油率 +25% 且时长 +1 秒/层（cap2，2★；与老条「加农炮转化」同株互斥，2026-10-10 起跨屏「持有即排除」）
	RUN_BUFF2_GUIDE,		// #36 大蒜：引路蒜——血量仅 20，被吃时全行僵尸换道（cap1，2★；矛盾设计原型）
	RUN_BUFF2_FORTUNE,		// #38 金盏花：富贵——+10%/层 概率产金/钻石币（cap3，1★；产币端可实现照落，钱用途待定）
	RUN_BUFF2_ROLLING,		// #39 西瓜投手：滚爆——落地后向前滚 1 格并再炸一次（cap1，1★）
	RUN_BUFF2_FIREPEA,		// #40 机枪射手：火豆——子弹变火豆：伤害翻倍（原版火豆 40 伤，评审文案「+50%」低估，docs/07）、命中解除冰缓（cap1，2★）
	RUN_BUFF2_LEGACY,		// #41 双子向日葵：遗泽——被吃掉时产 1 次阳光（cap1，2★）
	RUN_BUFF2_ROCKFLESH,	// #42 忧郁菇：耐砸——血量变为 1000、每次被击固定扣 200（cap1，1★）
	RUN_BUFF2_DEEPFREEZE,	// #44 冰西瓜：冰封——+20%/层 概率冻结命中者 2 秒（cap3，3★）
	RUN_BUFF2_GOLDTOUCH,	// #45 吸金磁：金石——+25%/层 概率银币升金币（cap3，1★；钱用途待定）
	RUN_BUFF2_TWINCOB,		// #47 玉米加农炮：双联装——每轮多发 1 枚（cap1，1★）
	RUN_PLANT_BUFF2_COUNT
};

// 第二表的表行查询（结构同老表）；下标越界 → gRunPlantBuff2Defs[0]。
const RunPlantUpgradeDef& GetRunPlantBuff2Def(int theIndex);
// 这株植物在第二表里的下标；表里没有（含 9 株删条）→ −1。
int RunPlantBuff2IndexFor(SeedType thePlant);
// 第二表条目是否进抽取池：9 株删条天然不进；其余条目在各自消费端落地批才翻真
//（未落消费端的条目不出——「抽到了没效果」比「抽不到」更伤）。RunState::RollChoices 过滤用。
bool RunPlantBuff2InPool(int theIndex);

// id → 所属植物：全局增益 → SEED_NONE；老单株/第二表条目 → 查各自表的 mPlant。
// 同屏同株去重（一屏三张里同一株的老/新条至多出现 1 条）在 RunState::RollChoices
// 抽中后摘除时用；§8.6.1 矛盾对另有跨屏「持有即排除」（候选构建期，2026-10-10 起）。
SeedType GetRunChoicePlant(int theId);

#endif
