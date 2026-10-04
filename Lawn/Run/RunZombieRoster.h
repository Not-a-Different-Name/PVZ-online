#ifndef __RUNZOMBIEROSTER_H__
#define __RUNZOMBIEROSTER_H__

#include "../../ConstEnums.h"

// @pvz-online: 闯关的僵尸种类梯度（M4-a 定"更激进档"，2026-10-03 按实机反馈再提速一档——
// "希望丰富僵尸种类，让眼花缭乱的僵尸都出现"）。
//
// 一关能出哪些类型只由闯关关序号（完整版口径，0..24）决定：五个场景各分五个子关，
// 名单随手关推进逐格加码（白天基本怪 → 夜里上巨人 → 泳池/雾加水路专属 → 屋顶重武器
// 全开）。普通/快速档（M4-b）的关都是同一张 25 关表的子集——引擎关号反查回来的还是
// 完整版序号，所以同一引擎关在三档里遭遇同样的怪。
// 这是 Board::CanZombieSpawnOnLevel 的闯关分支查的表——名单只回答"这个子关允许
// 哪些类型"；行/场硬约束（水路只收会下水的、雪橇要有冰道、0 行禁巨人、舞王要
// 左右两行空地）在名单之外照旧自动生效。名单本体在 RunZombieRoster.cpp。
bool RunZombieAllowedOnLevel(ZombieType theZombieType, int theRunLevelIndex);

// @pvz-online: 闯关的抽怪权重（2026-10-03 用户定案"丰富僵尸种类"）：原版 pickWeight 里
// 普僵/路障各 4000、其余 1000-3500，抽出来近半个战场都是这两样。这里对名单内的类型给
// "平铺"权重（同权；2026-10-04 起气球单独降到 1/4），种类分布立刻散开；权重为 0 的
// 特殊类型（鸭子圈/旗帜/伴舞）保持 0。只在闯关分支替代 aZombieDef.mPickWeight 用，
// 非闯关局一字不动。
int RunZombieWeight(ZombieType theZombieType);

// 从引擎关号（mLevel）反推闯关关序号：CanZombieSpawnOnLevel 拿到的是 mLevel，
// 而名单按关序号编排，这里换算一次。表外关号（旧构建混搭等）一律当第 1 关。
int RunLevelIndexForEngineLevel(int theLevel);

#endif
