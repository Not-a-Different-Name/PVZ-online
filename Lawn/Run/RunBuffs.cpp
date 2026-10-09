#include "RunBuffs.h"
#include "../ModText.h"
#include <string>

// 说明文案中英各一份（语言批 2026-10-03 起），取用时按 ModText::IsChinese 择一；
// 绘制走宽字符路径（位图字体没有中文字形）。
// 按钮上的名字（mName）各语言同字（英文），仍走位图字体。
// 数值列要和 mDesc 里的百分比对得上，改一边就改另一边；封顶条目的上限后缀
// 由 GetRunChoiceDesc 按 mMaxStacks 机械追加，别手写进 mDesc。
// 结构体尾部省略的字段 = mMaxStacks 0（无限）、mMultiplicative false（线性）。
// 2026-10-03（方案 §三 定案）：全局 8 条整表重标——火力 30%、扎根 50%、丰饶封顶 4、
// 急袭/速种改叠乘 ×0.8、储备 50、天降封顶 4、爆破不变。
// 2026-10-04（用户指令，见 docs/07 批五）：扎根每层 +50% → +150%，与六条单株血量行
// （批二统一 +150%/层）对齐——至此全部血量成长行每层数值统一 1.5。
// 2026-10-04 批 18：追加第 9 条「Precision 所有伤害 +15%」（id 8；挂点 Zombie::TakeDamage
// 总入口，乘性叠在火力基数之上）——全局表 8→9 条，单株 id 全体右移 1（检查点 v6 迁移）。
// 2026-10-04 批七（用户指令，见 docs/07 同日条目）：三处表改动——
//   ① 扎根每层 +150% → +75%（全局血量行；单株血量行保持 +150%/层 不动——用户定案
//      「只改扎根」）；
//   ② 坚果墙/高坚果两行「血量 +150%/层」整条换「巨人砸击时像地刺王一样耐砸（每次
//      -200 血）」（1 层成型）——与批 18 大蒜同机制、伤害不同（大蒜每次 -50）：
//      Zombie.cpp 巨人砸击分支按株给伤害，砸空才被吃掉（坚果 4000 血 = 挨 20 次、
//      高坚果 8000 = 挨 40 次）；
//   ③ 地刺王行「攻击间隔 ×0.75/层（至多 3 层）」整条换「血量 +200%/层（至多 3 层）」，
//      走 HEALTH 通挂点（Plant.cpp:484），无需新代码。
//   （同批还有非表项：末位顺位乘数 ×1→×2，见 Board.cpp 与 docs/03 §5.37；MOD_BUILD 29→30。）
// 2026-10-05 批八（用户指令，见 docs/07 同日条目）：灰烬/一次性族四项——
//   ① 全局「Demolition 爆破」+30% → +60%/层（一次性植物伤害）；
//   ② 樱桃/土豆雷/毁灭菇三行半径 +25% → +50%/层（只动表值与 desc，代码挂点原样；
//      其中毁灭菇那条已在批八b 整条换掉、半径还原 250——见下方批八b 注）；
//   ③ 寒冰菇行 +2 秒/层 → +4 秒/层（Zombie::HitIceTrap 钩子 200→400 帧/层）；
//   ④ 窝瓜行「压击处眩晕 +2 秒/层」整条换「砸击次数 +2/层」——UpdateSquash 改多段砸击
//      （落地还有余额就起身再砸，共 1+2n 次），原 DoSquashDamage 的 ApplyButter 挂点删。
//   （同批 MOD_BUILD 不进位：纯表值与单机表现，无协议影响。）
// 2026-10-05 批八b（用户指令，见 docs/07 同日条目）：毁灭菇行整条换「爆炸造成 50000 伤害」
//   （只可选 1 层）——取代批八 ② 的毁灭菇半径条目。Board::KillAllZombiesInRadius 与
//   Zombie::ApplyBurn 加直伤基数覆写参数（0 = 默认 1800）；覆写只换基数（厚血目标改吃
//   50000、薄血目标照旧烧死保味），全局「爆破」乘数照常叠乘。
// 2026-10-06（用户指令，见 docs/07 同日条目）：「天降」行整条重做——「天上掉阳光间隔
//   −20%/层、至多 4 层」→「夜晚/迷雾关也降阳光 + 降阳光速率 ×4、只可选 1 层」。
//   消费走专口 LawnApp::RunSkySunAtNight / RunSkySunIntervalMul，二值口径：层数 ≥1 即生效、
//   不看叠了几层（旧档这条带 2~4 层的与 1 层同效，不追溯削减——同忧郁菇上限 2→1 先例）；
//   数值列 −0.75×1 层 = ×0.25 只作文档，与 desc 的「×4」互为倒数（不参与 RunBuffMul 的
//   逐层公式——照公式旧档 4 层会出负数）。挂点 = Board.cpp 的 mSunCountDown 两处
//   （InitLevel 初始 / UpdateSunSpawning 重置）与 UpdateSunSpawning 的 StageIsNight 门。
// 2026-10-06（用户指令，见 docs/07 同日条目）：「储备」（全局原 id 5）删除——全局表 9→8 条，
//   单株 id 由「9 + 下标」全体左移 1 变「8 + 下标」（检查点 v8 迁移，RunState::Load）；
//   开局阳光改按上座顺位给（LawnApp::OnlineStartSunBonus，挂点 Board::InitLevel），不再走本表。
// 2026-10-09 权重批（docs/07 同日条目）：两表加稀有度档位（末列，§8.2/§8.7 定稿）；
//   数值对齐八处 + 两条整条重做——丰饶 −20%→−15%、小喷菇 +2→+1 颗/层、大喷菇 ×0.25→×0.5、
//   三线 cap2→cap1、南瓜头/保护伞 cap3→cap2、地刺王 +200% cap3→+150% cap2（血量族统一，
//   覆盖批七③）、×0.75 冷却族五条（辣椒/海蘑菇/磁力菇/吸金磁/加农炮）统一 ×0.8、
//   向日葵/阳光菇/双子整条重做「丰收：25% 概率多产 1 阳光/层 cap2」、土豆雷整条重做
//   「震雷：爆炸眩晕 2 秒/层 cap2」。金盏花/吸金磁 1★ 冻结（钱无用途，§8.3）。
// 2026-10-09 审计批（docs/07 同日条目）：花盆/睡莲血量行删条——不进抽取池（300 血底子
//   ≈ 无感，§8.6 评审同口径先例），表行与挂点保留供存量档生效（id 空间/检查点不动，
//   见 RunPlantUpgradeInPool）；路灯花「照亮 +1 格」2★→1★（雾关外死格不配中档）。
static const RunBuffDef gRunBuffDefs[RUN_BUFF_COUNT] =
{
	// 末列 = 稀有度档位（权重批 2026-10-09 填档，docs/06 §8.2.1/§8.7）：
	// 3★ 火力/丰饶/急袭 · 2★ 扎根/速种/爆破/精准 · 1★ 天降
	{ "Firepower",    "所有子弹伤害 +30%",      "All projectile damage +30%",       0.30f,  0, 0, false, 3 },
	{ "Deep Roots",   "所有植物血量 +75%",      "All plant health +75%",            0.75f,  0, 0, false, 2 },
	{ "Abundance",    "产阳光植物更快 15%",     "Sun plants 15% faster",           -0.15f,  0, 4, false, 3 },
	{ "Swift Strikes","植物攻击间隔逐层 ×0.8",  "Plant attack interval ×0.8/stack",-0.20f,  0, 0, true, 3 },
	{ "Quick Seeds",  "种植冷却逐层 ×0.8",      "Planting cooldown ×0.8/stack",    -0.20f,  0, 0, true, 2 },
	{ "Skyfall",      "夜晚也降阳光，降阳光速率 ×4", "Sky sun also falls at night, 4x drop rate", -0.75f,  0, 1, false, 1 },
	{ "Demolition",   "一次性植物伤害 +60%",    "Instant plant damage +60%",        0.60f,  0, 0, false, 2 },
	{ "Precision",    "所有伤害 +15%",          "All damage +15%",                  0.15f,  0, 0, false, 2 },
};

const RunBuffDef& GetRunBuffDef(int theId)
{
	if (theId < 0 || theId >= RUN_BUFF_COUNT) return gRunBuffDefs[0];
	return gRunBuffDefs[theId];
}

// 单株升级。数值列要和 mDesc 里的百分比对得上，改一边就改另一边。
// （每层）前面的 \n 是排版用的显式换行，见 RunPickDialog 的中文排版。
// 顺序纪律（方案 §2.1/Q7）：按 SeedType 升序排，前 5 条（SeedType 0..4）永远留在原位；
// 新增条目插在自己的 SeedType 位次上。批 1 的 5 条是追加（SeedType 都大于 4）；批 2 的
// 4 条插进中段（双发 7 / 杨桃 29 / 卷心菜 32 / 机枪 40——射速族共用 Plant.cpp:956 的节奏
// 计数器，全局「急袭」同挂点）；批 3 的 3 条（阳光菇 9 / 金盏花 38 / 双子向日葵 41——
// 产出族，UpdateProductionPlant 各分支加「每层多落一枚」循环）；批 4 的 2 条（火爆辣椒
// 20 / 海蘑菇 24——种植冷却，SeedPacket.cpp:884 同挂点）；批 5 的 3 条（毁灭菇 15 / 窝瓜
// 17 / 三线 18——毁灭菇照樱桃的半径乘法，窝瓜走 DoSquashDamage 里 ApplyButter 眩晕，三线
// 纯表：每道一次 Fire，Plant.cpp:4790 的多发循环按株取数）；批 6 的 2 条（寒冰射手 5 /
// 寒冰菇 14——寒冰射手：雪豆命中后在 Projectile::DoImpact 单体分支把 ApplyChill 挂上的
// 1000 帧按单株乘数放宽（==1000 认出、更长的减速不动）；寒冰菇：Zombie::HitIceTrap 三档
// 冻结各 +200 帧/层，唯一调用者就是 IceZombies）；批 7 的 2 条（小喷菇 8 / 大喷菇 10——射程族，
// 挂点都在 Plant::GetPlantAttackRect 的攻击矩形：小喷菇带层即同 default 支一路铺到板尾（同 800px，
// FindTargetZombie 拿这矩形当开火门，弹道本身无射程上限），大喷菇每层 +80px；同批土豆雷 4 改
// 「Wide Charge」：效果从爆炸直伤 +40% 换成半径 +25%/层——直伤乘数从
// Board::KillAllZombiesInRadius 摘除，半径乘数落 Plant::DoSpecial 土豆雷支）；批 8 的 2 条（地刺 21 /
// 地刺王 46——地刺：命中的僵尸在 Plant::DoRowAreaDamage 里按层数上减速（+300 帧/层，CanBeChilled/
// max 语义同寒冰）；地刺王：Plant::SpikeweedCycleFrames 攻击循环帧数 ×单株乘数，75/69/33 三个命中点
// 同比例缩）；批 9 的 2 条（磁力菇 31 / 吸金磁 45——磁力菇：吸取后充能 1500 帧乘单株乘数
// （MagnetShroomRechargeFrames，吸僵尸装备与吸地面梯子两处共用）；吸金磁：READY 期 1/50
// 起吸门与充能 200..300 帧同步乘，吸取动画本身不动）。表内下标随插入右移：批 4
// 档里 id 16..26、批 5 档里 15/17/18、批 6 档里 15..31、批 7 档里 24..33、批 8 档里 35 的层数会错位到别的植物（批 1/批 2/批 3 档同理；开发期接受，见方案 §六）。
// 修正批 2026-10-03（方案 §2.7）：四处通用挂点此前对所有行无条件消费 mul/count——
// 语义无关的条目会静默生效（双发行每层缩双发血量 25%、寒冰行把雪豆射速拉长、
// 玉米投手够到多发循环就多发玉米、睡莲行把睡莲种植冷却 ×3）。现在条目带 mKind、
// 挂点走 RunPlantUpgradeMulKind/CountKind：只有标签相符的条目才被该挂点消费。
// 本批 14 行打标（血量 6 / 射速节奏 4 / 多发 2 / 种植冷却 2），其余默认 EFFECT。
// 批 10 2026-10-03：投手族 4 条（玉米投手 34 / 西瓜 39 / 冰西瓜 44 / 加农炮 47）——
// 玉米投手：黄油掷点 Sexy::Rand(4)==0 改 < 1+层数（3 层必出）；西瓜：IsZombieHitBySplash
// 的判定矩形宽 ×单株乘数（不动主命中矩形）；冰西瓜：命中后 1000 帧减速按单株乘数放宽
// （溅射 DoSplashDamage 与打抗火僵尸的单发分支两条路同挂）；加农炮：两处 ARMING 倒计时
//（种下 500 / 每发完 3000）走 CobCannonArmFrames 同源取整，充能/开火动画不动。
// 批 11 2026-10-03：计时/产出族 4 条（大嘴花 6 / 墓碑吞噬者 11 / 火炬树桩 22 / 咖啡豆 35）——
// 大嘴花：咬到后的消化倒计时 4000 帧 ×单株乘数（咬/吞动画不动）；墓碑吞噬者：吞掉墓碑
// 成功时额外落 25 阳光/层（真吞到才给，落币法同咖啡豆）；火炬树桩：火弹伤害两个计算点
// （溅射 DoSplashDamage 的基数、打抗火僵尸的单发分支）乘单株乘数（KindlingFireballDamage
// helper）；咖啡豆：闯关里产阳光 4+层（1 层 5 枚 = 125、2 层 6 枚 = 150；
// 2026-10-03 平调三版：基座维持 100、每层 +25——用户定的 100/125/150；
// 种植冷却加长到 12 秒）。
// 批 12 2026-10-03：弹道/索敌族 3 条（仙人掌 26 / 分裂豌豆 28 / 猫尾草 43）——
// 仙人掌：尖刺穿透（弹体 mPricklyHitsLeft 由 Fire 按层数预置、只有仙人掌的弹带；命中扣
// 一点继续飞，并把弹体推到该僵尸身后免重撞——Projectile::DoImpact 末尾）；分裂豌豆：
// 背向弹每次 +1 颗（Fire 的 SECONDARY 分支多发循环，镜像正面 21px 错位）；猫尾草：攻击
// 目标 +1 个/层（FindTargetZombie 加排除参数，同一轮依次找第 2/3 个目标各发一颗追踪刺）。
// 表内下标随插入右移补充：批 9 档里 id 36、批 10 档里 id 14 及以后、批 11 档里 id 30 及以后、
// 批 12 档里 id 30 及以后、批 13 档里 id 20 及以后也会错位到别的植物；批 14 落地后 48 条
// 满编（表下标 == SeedType），此后版本不再有错位。
// 批 13 2026-10-03：控制/减速族 4 条（魅惑菇 12 / 缠绕海草 18 / 三叶草 25 / 大蒜 34）——
// 魅惑菇：被魅惑僵尸咬到的僵尸也倒戈（Zombie::EatZombie 命中点走 StartMindControlled，
// 1 层成型）；缠绕海草：抓取时每层多找 1 只（至多 2 层、1→3 只），额外目标存
// Plant::mExtraTanglekelpIDs，抓取/沉底/清场与主目标同一时点；三叶草：吹风中
// （BlowAwayFliers）全场僵尸减速 500 帧/层；大蒜：被驱赶换道那一刻（UpdateYuckyFace
// 的 170 帧点）减速 500 帧/层。后两者语义照地刺（CanBeChilled / max / 冰音）。
// 批 14 2026-10-03：光环/行为族 3 条（胆小菇 13 / 路灯花 25 / 忧郁菇 42）——本批后满编。
// 胆小菇：抽到后不再缩头（UpdateScaredyShroom 的贴近判定直接作废，恒 READY 照常射击，
// 1 层成型）；路灯花：迷雾关照亮范围每层 +1 格（UpdateFog 传 4+层数，ClearFogAroundPlant
// 的矩形形状随之逐层膨胀——原版 3 横 2 纵切角，膨胀语义为三常量 +1/+1/+2）；
// 忧郁菇：光环范围每层 +1 格（GetPlantAttackRect 的 240×240 矩形与 DoRowAreaDamage
// 的行差限制 ±1 同步外扩，保证纵向也真的多打一行）。
// 2026-10-04 平衡调整（用户指令，见 docs/07 同日条目）三处：
//   ① 小喷菇行「Far Spore 射程变为无限」整条换掉 → 「Spore Volley 每次多发 2 颗/层、无上限」，
//      走 SHOTCOUNT 通挂点（Plant.cpp Fire 多发循环；该株的 2 颗/层换算在循环里做）；
//      原射程挂点（GetPlantAttackRect 小喷菇支的 800px）随之还原为 230px。
//   ② 忧郁菇行层数上限 2 → 1（效果不变：+1 格光环；文案去掉「（每层）」随单层惯例）。
//   ③ 六条血量行（坚果墙/睡莲/高坚果/南瓜头/花盆/保护伞）每层 +50%/+100% 统一改 **+150%/层**。
// 2026-10-04 再调整（用户指令，见 docs/07 同日条目）：杨桃行「Star Rain 射击间隔 ×0.75/层
//   （至多 3 层）」整条更换 → 「Homing Stars 子弹变为追踪弹」（1 层成型、至多 1 层）。
//   星弹出膛后走香蒲刺同款 MOTION_HOMING（转向/命中都在 Projectile 的 homing 分支）；
//   弹种/贴图不变（照旧 PROJECTILE_STAR）、五向散开的初速保留；选敌/挂点在
//   Plant::StarFruitFire（批 12 香蒲多目标同款的距离口径，逐颗排除已锁定目标）。
// 批 18 2026-10-04（用户指令，见 docs/07 同日条目；本批 11 项，9 条行整条替换 + 1 条全局 + 1 项非表）：
//   新增全局「Precision 所有伤害 +15%」（无限；挂点 Zombie::TakeDamage 总入口——总量放大后
//   再走各分支，乘性叠在「火力」基数之上，1.30×1.15≈1.495）；
//   寒冰射手 Frostbite→Blizzard：发射冰西瓜（1 层；Fire 出口弹种换成 PROJECTILE_WINTERMELON，
//   80 伤 + 冰冻 + 溅射，直线飞行走 UpdateNormalMotion 的兜底速度，无需设初速）；
//   双发 Quick Rhythm→Pea Barrage：每次射击多发 2 颗/层（无上限；SHOTCOUNT 通挂点——
//   该株一轮 = 两次 Fire，故每层一轮净增 2 颗，满打满算的「每次 +2」）；
//   大喷菇 Thick Fumes→Fume Rush：攻击间隔 ×0.25（1 层；RHYTHM 通挂点，原射程挂点还原）；
//   卷心菜 Heavy Toss→Free Toss：种植费用变为 0（1 层；GetCost 的 run 块开头短路，卡面/
//   扣费/可用全走此口，模仿者按底座解析）；玉米投手 Buttery→Artillery：种下后变为玉米加农炮
//   （1 层；CanPlantAt 校验加农炮点位、执行器扣费后换 SEED_COBCANNON 走既有加农炮种植路径，
//   点位放不下则拦截提示、不扣阳光；原黄油率挂点还原 Sexy::Rand(4)==0）；
//   大蒜 Pungent→Iron Clove：巨人砸击时像地刺王一样耐砸（1 层；Gargantuar 砸击加分支——
//   每砸 -50 血、不反伤不震，≤0 才被吃掉；原换道减速挂点删）；
//   西瓜 Heavy Melon→Melon Barrage：每次多发 1 个西瓜/层（无上限；投手初速段内循环追加
//   完整弹道参数的同型弹，EFFECT——不进多发通环以免呆弹；原溅射范围挂点换给冰西瓜）；
//   机枪 Rapid Fire→Overclock：攻击间隔 ×0.5（1 层；RHYTHM 通挂点）；
//   冰西瓜 Winter Chill→Deep Splash：溅射半径 +50%/层（无上限；IsZombieHitBySplash 的
//   判定矩形宽 ×单株乘数，主命中不动；原「溅射减速放宽」WidenWinterMelonChill 挂点整体删）；
//   另有非表项：三线射手成本 325→150（gPlantDefs，全模式生效）。
//   id 纪律：全局表 8→9 条使单株 id 全体右移 1——旧检查点 v6 迁移（RunState::Load：
//   aVersion<6 时 id≥8 均 +1）。
// 血量型条目（批 1 的 5 条 + 坚果墙）不用专门挂点——Plant.cpp:484 的通用血量口按
// RUN_UPGRADE_KIND_HEALTH 消费（修正批起，不再对任意行生效），表里加一行标上 Kind
// 就生效（南瓜头护罩血已查证同走 mPlantHealth）。
static const RunPlantUpgradeDef gRunPlantUpgradeDefs[RUN_PLANT_UPGRADE_COUNT] =
{
	// 末列 = 稀有度档位（权重批 2026-10-09 填档，docs/06 §8.2.2/§8.7）。
	// 3★：豌豆0/寒冰射手5/双发7/小喷菇8/大喷菇10/寒冰菇14/毁灭菇15/三线18/玉米投手34/西瓜39/机枪40
	// 1★：墓碑11/卷心菜32/咖啡豆35/金盏花38/吸金磁45（金盏花/吸金磁冻结——钱无用途，§8.3）
	//     + 路灯花25（审计批 2026-10-09 降档：雾关外死格）
	// 审计批 2026-10-09 删条不进池：睡莲16/花盆33（血量行对 300 血底子 ≈ 无感，见
	// RunPlantUpgradeInPool）——表行保留供存量档，池 56→54。
	{ SeedType::SEED_PEASHOOTER,   "Pea Volley",   "豌豆射手每次多发 1 颗\n（每层）",   "Peashooter fires 1 extra pea per shot\n(per stack)", 0.00f, 0, false, RUN_UPGRADE_KIND_SHOTCOUNT, 3 },
	{ SeedType::SEED_SUNFLOWER,    "Harvest",      "每轮 25% 概率多产 1 阳光\n（每层）", "25% chance of 1 extra sun per cycle\n(per stack)",   0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_CHERRYBOMB,   "Wide Blast",   "樱桃炸弹爆炸范围 +50%\n（每层）",   "Cherry Bomb blast radius +50%\n(per stack)",        0.50f, 0, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_WALLNUT,      "Thick Shell",  "巨人砸击时像地刺王一样耐砸\n（每次 -200 血）", "Survives Gargantuar smashes like a Spikerock\n(-200 HP per smash)", 0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_POTATOMINE,   "Seismic Mine", "爆炸眩晕半径内僵尸 2 秒\n（每层）", "Blast stuns zombies in radius 2 sec\n(per stack)",   0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_SNOWPEA,      "Blizzard",     "发射冰西瓜",                        "Fires winter melons",                                0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 3 },
	{ SeedType::SEED_CHOMPER,      "Ravenous",     "咀嚼时间减半",                      "Chew time halved",                                   -0.50f, 1, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_REPEATER,     "Pea Barrage",  "每次射击多发 2 颗\n（每层）",       "Fires 2 extra peas per shot\n(per stack)",           0.00f, 0, false, RUN_UPGRADE_KIND_SHOTCOUNT, 3 },
	{ SeedType::SEED_PUFFSHROOM,   "Spore Volley", "每次多发 1 颗\n（每层）",           "Fires 1 extra spore per shot\n(per stack)",        0.00f, 0, false, RUN_UPGRADE_KIND_SHOTCOUNT, 3 },
	{ SeedType::SEED_SUNSHROOM,    "Bright Cap",   "每轮 25% 概率多产 1 阳光\n（每层）", "25% chance of 1 extra sun per cycle\n(per stack)",   0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_FUMESHROOM,   "Fume Rush",    "攻击间隔 ×0.5",                     "Attack interval ×0.5",                               -0.50f, 1, false, RUN_UPGRADE_KIND_RHYTHM, 3 },
	{ SeedType::SEED_GRAVEBUSTER,  "Quick Dig",    "吞掉墓碑额外产 25 阳光\n（每层）",  "Grave eaten yields +25 sun\n(per stack)",            0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 1 },
	{ SeedType::SEED_HYPNOSHROOM,  "Devotion",     "被魅惑僵尸咬到的僵尸也变友军",      "Zombies bitten by a hypnotized zombie turn friendly", 0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_SCAREDYSHROOM, "Bravery",     "敌人贴近时不再缩头",                "No longer hides when zombies get close",             0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_ICESHROOM,    "Deep Freeze",  "全场冰冻 +4 秒\n（每层）",          "Board freeze +4 sec\n(per stack)",                   0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 3 },
	{ SeedType::SEED_DOOMSHROOM,   "Annihilation", "爆炸造成 50000 伤害",               "Blast deals 50000 damage",                           0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 3 },
	{ SeedType::SEED_LILYPAD,      "Tough Pad",    "血量 +150%\n（每层）",              "Health +150%\n(per stack)",                          1.50f, 2, false, RUN_UPGRADE_KIND_HEALTH, 2 },
	{ SeedType::SEED_SQUASH,       "Heavy Squash", "砸击次数 +2\n（每层）",             "Smashes 2 extra times\n(per stack)",                 0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_THREEPEATER,  "Triple Volley", "每条道多发 1 颗",                  "1 extra pea per lane",                               0.00f, 1, false, RUN_UPGRADE_KIND_SHOTCOUNT, 3 },
	{ SeedType::SEED_TANGLEKELP,   "Entangle",     "多缠 1 只僵尸\n（每层）",           "Grabs 1 extra zombie\n(per stack)",                  0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_JALAPENO,     "Inferno",      "种植冷却逐层 ×0.8\n（每层）",       "Planting cooldown ×0.8/stack",                       -0.20f, 3, true, RUN_UPGRADE_KIND_COOLDOWN, 2 },
	{ SeedType::SEED_SPIKEWEED,    "Barbed Spikes", "扎过的僵尸减速 +3 秒\n（每层）",   "Zombies it pricks slowed +3 sec\n(per stack)",       0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_TORCHWOOD,    "Kindling",     "火弹伤害加成 +50%\n（每层）",       "Fire pea damage +50%\n(per stack)",                  0.50f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_TALLNUT,      "Iron Shell",   "巨人砸击时像地刺王一样耐砸\n（每次 -200 血）", "Survives Gargantuar smashes like a Spikerock\n(-200 HP per smash)", 0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_SEASHROOM,    "Brine Spore",  "种植冷却逐层 ×0.8\n（每层）",       "Planting cooldown ×0.8/stack",                       -0.20f, 3, true, RUN_UPGRADE_KIND_COOLDOWN, 2 },
	{ SeedType::SEED_PLANTERN,     "Lantern Light", "照亮范围 +1 格\n（每层）",         "Illumination radius +1 tile\n(per stack)",           0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 1 },
	{ SeedType::SEED_CACTUS,       "Prickly",      "尖刺穿透 +1 只\n（每层）",          "Spikes pierce +1 zombie\n(per stack)",               0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_BLOVER,       "Gale",         "吹风后全场僵尸减速 5 秒\n（每层）", "Slows all zombies 5 sec after blowing\n(per stack)", 0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_SPLITPEA,     "Backspike",    "背向豌豆每次 +1 颗\n（每层）",      "1 extra backward pea per shot\n(per stack)",         0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_STARFRUIT,    "Homing Stars", "子弹变为追踪弹",                    "Shots become homing",                                0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_PUMPKINSHELL, "Hard Rind",    "血量 +150%\n（每层）",              "Health +150%\n(per stack)",                          1.50f, 2, false, RUN_UPGRADE_KIND_HEALTH, 2 },
	{ SeedType::SEED_MAGNETSHROOM, "Magnet Pull",  "吸取间隔逐层 ×0.8\n（每层）",       "Recharge interval ×0.8/stack",                       -0.20f, 3, true, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_CABBAGEPULT,  "Free Toss",    "种植费用变为 0",                    "Planting cost becomes 0",                            0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 1 },
	{ SeedType::SEED_FLOWERPOT,    "Rich Soil",    "血量 +150%\n（每层）",              "Health +150%\n(per stack)",                          1.50f, 2, false, RUN_UPGRADE_KIND_HEALTH, 2 },
	{ SeedType::SEED_KERNELPULT,   "Artillery",    "种下后变为玉米加农炮",              "Becomes a Cob Cannon when planted",                  0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 3 },
	{ SeedType::SEED_INSTANT_COFFEE,"Rich Roast",  "唤醒产阳光 +25\n（每层）",          "Waking sun +25\n(per stack)",                        0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 1 },
	{ SeedType::SEED_GARLIC,       "Iron Clove",   "巨人砸击时像地刺王一样耐砸",        "Survives Gargantuar smashes like a Spikerock",       0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_UMBRELLA,     "Canopy",       "血量 +150%\n（每层）",              "Health +150%\n(per stack)",                          1.50f, 2, false, RUN_UPGRADE_KIND_HEALTH, 2 },
	{ SeedType::SEED_MARIGOLD,     "Golden Bloom", "每次多产 1 枚\n（每层）",           "1 extra coin per cycle\n(per stack)",                0.00f, 3, false, RUN_UPGRADE_KIND_EFFECT, 1 },
	{ SeedType::SEED_MELONPULT,    "Melon Barrage","每次多发 1 个西瓜\n（每层）",       "Fires 1 extra melon per volley\n(per stack)",        0.00f, 0, false, RUN_UPGRADE_KIND_EFFECT, 3 },
	{ SeedType::SEED_GATLINGPEA,   "Overclock",    "攻击间隔 ×0.5",                     "Attack interval ×0.5",                               -0.50f, 1, false, RUN_UPGRADE_KIND_RHYTHM, 3 },
	{ SeedType::SEED_TWINSUNFLOWER, "Twin Bloom",  "每轮 25% 概率多产 1 阳光\n（每层）", "25% chance of 1 extra sun per cycle\n(per stack)",   0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_GLOOMSHROOM,  "Gloom",        "光环范围 +1 格",                    "Aura radius +1 tile",                                0.00f, 1, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_CATTAIL,      "Quick Claw",   "攻击目标 +1 个\n（每层）",          "Targets +1 zombie\n(per stack)",                     0.00f, 2, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_WINTERMELON,  "Deep Splash",  "溅射半径 +50%\n（每层）",           "Splash radius +50%\n(per stack)",                    0.50f, 0, false, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_GOLD_MAGNET,  "Gilded Pull",  "吸取间隔逐层 ×0.8\n（每层）",       "Recharge interval ×0.8/stack",                       -0.20f, 3, true, RUN_UPGRADE_KIND_EFFECT, 2 },
	{ SeedType::SEED_SPIKEROCK,    "Royal Thorns", "血量 +150%\n（每层）",              "Health +150%\n(per stack)",                          1.50f, 2, false, RUN_UPGRADE_KIND_HEALTH, 2 },
	{ SeedType::SEED_COBCANNON,    "Rapid Reload", "装填时间逐层 ×0.8\n（每层）",       "Reload time ×0.8/stack",                             -0.20f, 3, true, RUN_UPGRADE_KIND_EFFECT, 2 },
};

const RunPlantUpgradeDef& GetRunPlantUpgradeDef(int theIndex)
{
	if (theIndex < 0 || theIndex >= RUN_PLANT_UPGRADE_COUNT) return gRunPlantUpgradeDefs[0];
	return gRunPlantUpgradeDefs[theIndex];
}

int RunPlantUpgradeIndexFor(SeedType thePlant)
{
	for (int i = 0; i < RUN_PLANT_UPGRADE_COUNT; i++)
	{
		if (gRunPlantUpgradeDefs[i].mPlant == thePlant) return i;
	}
	return -1;
}

const char* GetRunChoiceName(int theId)
{
	if (theId >= RUN_BUFF_COUNT) return GetRunPlantUpgradeDef(theId - RUN_BUFF_COUNT).mName;
	return GetRunBuffDef(theId).mName;
}

// 封顶条目的说明尾部机械追加语言相称的上限后缀（方案 §2.5）：基础文案只管效果，
// 上限只在 mMaxStacks 一处维护。返回静态缓冲——屏上取到就画，别存指针。
const char* GetRunChoiceDesc(int theId)
{
	const char* aDesc;
	int aMaxStacks;
	if (theId >= RUN_BUFF_COUNT)
	{
		const RunPlantUpgradeDef& aDef = GetRunPlantUpgradeDef(theId - RUN_BUFF_COUNT);
		aDesc = ModText::IsChinese() ? aDef.mDesc : aDef.mDescEn;
		aMaxStacks = aDef.mMaxStacks;
	}
	else
	{
		const RunBuffDef& aDef = GetRunBuffDef(theId);
		aDesc = ModText::IsChinese() ? aDef.mDesc : aDef.mDescEn;
		aMaxStacks = aDef.mMaxStacks;
	}
	if (aMaxStacks <= 0) return aDesc;

	static std::string sCappedDesc;
	sCappedDesc = aDesc;
	if (ModText::IsChinese())
	{
		sCappedDesc += "，至多 ";
		sCappedDesc += std::to_string(aMaxStacks);
		sCappedDesc += " 层";
	}
	else
	{
		sCappedDesc += ", up to ";
		sCappedDesc += std::to_string(aMaxStacks);
		sCappedDesc += " stacks";
	}
	return sCappedDesc.c_str();
}

// 这条条目封顶几层（0 = 无限）。抽取过滤与屏上「已有 x/N」都走它。
int GetRunChoiceMaxStacks(int theId)
{
	if (theId >= RUN_BUFF_COUNT) return GetRunPlantUpgradeDef(theId - RUN_BUFF_COUNT).mMaxStacks;
	return GetRunBuffDef(theId).mMaxStacks;
}

// 弱词条删条（审计批 2026-10-09，docs/07 同日条目）：花盆/睡莲的血量行对 300 血底子 ≈ 无感
// （§8.6 第二 buff 评审同口径先例：这两株的新条已删）。表行与挂点保留——老档里已叠的
// 层数照常生效、id 空间与检查点一字不动——只是不再进抽取池。
bool RunPlantUpgradeInPool(SeedType thePlant)
{
	return thePlant != SeedType::SEED_FLOWERPOT && thePlant != SeedType::SEED_LILYPAD;
}

// 稀有度档位（权重批 2026-10-09，docs/06 §8.7）：全局/单株两类都查 mRarity。
// 两表已全量填档（75efd06，3★14/2★36/1★6，分布见 §8.2）。
int GetRunChoiceRarity(int theId)
{
	if (theId < 0) return 0;
	if (theId >= RUN_BUFF_COUNT)
	{
		int aIdx = theId - RUN_BUFF_COUNT;
		if (aIdx >= RUN_PLANT_UPGRADE_COUNT) return 0;
		return GetRunPlantUpgradeDef(aIdx).mRarity;
	}
	return GetRunBuffDef(theId).mRarity;
}

// 抽取权重：未定档恒 1（零行为）；定档后 = 档位基值 1★6/2★3/3★1，全局条再乘 k
// （千分比四舍五入）。RunState::RollChoices 按它做同屏加权无放回抽取。
int GetRunChoiceWeight(int theId)
{
	static const int aBase[4] = { 1, 6, 3, 1 };		// 未标 / 1★ / 2★ / 3★
	int aRarity = GetRunChoiceRarity(theId);
	if (aRarity < 0 || aRarity > 3) aRarity = 0;
	int aW = aBase[aRarity];
	if (aRarity > 0 && theId >= 0 && theId < RUN_BUFF_COUNT)
		aW = (aW * RUN_GLOBAL_WEIGHT_K_PERMILLE + 500) / 1000;
	if (aW < 1) aW = 1;
	return aW;
}

// 单株升级列顶那行【植物名】（2026-10-03 玩家反馈）：按钮名字是英文位图字体塞不下中文，
// 说明文案批 2 起多数也不含植物名（「射击间隔逐层 ×0.75」这种）——不单独标出来，
// 屏上分不清这条 buff 是哪株的。按 SeedType 查（表里一株最多一条）；新批次往表里
// 插行时这里同步加名字。全局增益（或漏了名字）→ NULL，屏上不画标题行。
// 语言批 2026-10-03：中/英各一列，按当前语言择一（英文名用官方名，与位图字体按钮/
// 种子卡上的名字对得上）。词条查看器同一张表。
static const struct { SeedType mPlant; const char* mZh; const char* mEn; } gRunPlantUpgradeNames[] =
{
	{ SeedType::SEED_PEASHOOTER,   "豌豆射手",  "Peashooter" },
	{ SeedType::SEED_SUNFLOWER,    "向日葵",    "Sunflower" },
	{ SeedType::SEED_CHERRYBOMB,   "樱桃炸弹",  "Cherry Bomb" },
	{ SeedType::SEED_WALLNUT,      "坚果墙",    "Wall-nut" },
	{ SeedType::SEED_POTATOMINE,   "土豆雷",    "Potato Mine" },
	{ SeedType::SEED_SNOWPEA,      "寒冰射手",  "Snow Pea" },
	{ SeedType::SEED_CHOMPER,      "大嘴花",    "Chomper" },
	{ SeedType::SEED_REPEATER,     "双发射手",  "Repeater" },
	{ SeedType::SEED_PUFFSHROOM,   "小喷菇",    "Puff-shroom" },
	{ SeedType::SEED_SUNSHROOM,    "阳光菇",    "Sun-shroom" },
	{ SeedType::SEED_FUMESHROOM,   "大喷菇",    "Fume-shroom" },
	{ SeedType::SEED_GRAVEBUSTER,  "墓碑吞噬者","Grave Buster" },
	{ SeedType::SEED_HYPNOSHROOM,  "魅惑菇",    "Hypno-shroom" },
	{ SeedType::SEED_SCAREDYSHROOM,"胆小菇",    "Scaredy-shroom" },
	{ SeedType::SEED_ICESHROOM,    "寒冰菇",    "Ice-shroom" },
	{ SeedType::SEED_DOOMSHROOM,   "毁灭菇",    "Doom-shroom" },
	{ SeedType::SEED_LILYPAD,      "睡莲",      "Lily Pad" },
	{ SeedType::SEED_SQUASH,       "窝瓜",      "Squash" },
	{ SeedType::SEED_THREEPEATER,  "三线射手",  "Threepeater" },
	{ SeedType::SEED_TANGLEKELP,   "缠绕海草",  "Tangle Kelp" },
	{ SeedType::SEED_JALAPENO,     "火爆辣椒",  "Jalapeno" },
	{ SeedType::SEED_SPIKEWEED,    "地刺",      "Spikeweed" },
	{ SeedType::SEED_TORCHWOOD,    "火炬树桩",  "Torchwood" },
	{ SeedType::SEED_TALLNUT,      "高坚果",    "Tall-nut" },
	{ SeedType::SEED_SEASHROOM,    "海蘑菇",    "Sea-shroom" },
	{ SeedType::SEED_PLANTERN,     "路灯花",    "Plantern" },
	{ SeedType::SEED_CACTUS,       "仙人掌",    "Cactus" },
	{ SeedType::SEED_BLOVER,       "三叶草",    "Blover" },
	{ SeedType::SEED_SPLITPEA,     "分裂豌豆",  "Split Pea" },
	{ SeedType::SEED_STARFRUIT,    "杨桃",      "Starfruit" },
	{ SeedType::SEED_PUMPKINSHELL, "南瓜头",    "Pumpkin" },
	{ SeedType::SEED_MAGNETSHROOM, "磁力菇",    "Magnet-shroom" },
	{ SeedType::SEED_CABBAGEPULT,  "卷心菜投手","Cabbage-pult" },
	{ SeedType::SEED_FLOWERPOT,    "花盆",      "Flower Pot" },
	{ SeedType::SEED_KERNELPULT,   "玉米投手",  "Kernel-pult" },
	{ SeedType::SEED_INSTANT_COFFEE,"咖啡豆",   "Coffee Bean" },
	{ SeedType::SEED_GARLIC,       "大蒜",      "Garlic" },
	{ SeedType::SEED_UMBRELLA,     "保护伞",    "Umbrella Leaf" },
	{ SeedType::SEED_MARIGOLD,     "金盏花",    "Marigold" },
	{ SeedType::SEED_MELONPULT,    "西瓜投手",  "Melon-pult" },
	{ SeedType::SEED_GATLINGPEA,   "机枪射手",  "Gatling Pea" },
	{ SeedType::SEED_TWINSUNFLOWER,"双子向日葵","Twin Sunflower" },
	{ SeedType::SEED_GLOOMSHROOM,  "忧郁菇",    "Gloom-shroom" },
	{ SeedType::SEED_CATTAIL,      "猫尾草",    "Cattail" },
	{ SeedType::SEED_WINTERMELON,  "冰西瓜",    "Winter Melon" },
	{ SeedType::SEED_GOLD_MAGNET,  "吸金磁",    "Gold Magnet" },
	{ SeedType::SEED_SPIKEROCK,    "地刺王",    "Spikerock" },
	{ SeedType::SEED_COBCANNON,    "玉米加农炮","Cob Cannon" },
};

// 按 SeedType 直查植物显示名（局内词条查看器用：那边手里是表行 + 层数，没有 id）。
const char* GetRunPlantName(SeedType thePlant)
{
	bool aChinese = ModText::IsChinese();
	for (int i = 0; i < (int)(sizeof(gRunPlantUpgradeNames) / sizeof(gRunPlantUpgradeNames[0])); i++)
	{
		if (gRunPlantUpgradeNames[i].mPlant == thePlant)
			return aChinese ? gRunPlantUpgradeNames[i].mZh : gRunPlantUpgradeNames[i].mEn;
	}
	return NULL;
}

const char* GetRunChoicePlantName(int theId)
{
	if (theId < RUN_BUFF_COUNT) return NULL;
	return GetRunPlantName(GetRunPlantUpgradeDef(theId - RUN_BUFF_COUNT).mPlant);
}
