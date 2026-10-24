/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#include "Coin.h"
#include "Plant.h"
#include "Run/RunState.h"
#include "Run/RunDotEffects.h"
#include "Board.h"
#include "Zombie.h"
#include "Cutscene.h"
#include "GridItem.h"
#include "ZenGarden.h"
#include "Challenge.h"
#include "Projectile.h"
#include "SeedPacket.h"
#include "../LawnApp.h"
#include "CursorObject.h"
#include "../GameConstants.h"
#include "System/PlayerInfo.h"
#include "System/ReanimationLawn.h"
#include "../PvzpLib/PvzpFoley.h"
#include "../PvzpLib/PvzpDebug.h"
#include "../PvzpLib/Attachment.h"
#include "../PvzpLib/Reanimator.h"
#include "../PvzpLib/PvzpParticle.h"
#include "../PvzpLib/EffectSystem.h"
#include "../PvzpLib/PvzpStringFile.h"
#include "Widget/AchievementsScreen.h"
#include <algorithm>
#include <format>
#include "Run/RunBuffs.h"

constinit const // @pvz-online: 紫卡保留较长种植等待，但以 30 秒封顶避免拖慢成型（2026-10-16 用户定案）。
PlantDefinition gPlantDefs[SeedType::NUM_SEED_TYPES] = {
	{ .mSeedType = SeedType::SEED_PEASHOOTER,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_PEASHOOTER,    .mPacketIndex = 0,  .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "PEASHOOTER" },
	{ .mSeedType = SeedType::SEED_SUNFLOWER,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SUNFLOWER,     .mPacketIndex = 1,  .mSeedCost = 50,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 2500, .mPlantName = "SUNFLOWER" },
	{ .mSeedType = SeedType::SEED_CHERRYBOMB,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CHERRYBOMB,    .mPacketIndex = 3,  .mSeedCost = 150, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "CHERRY_BOMB" },
	{ .mSeedType = SeedType::SEED_WALLNUT,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_WALLNUT,       .mPacketIndex = 2,  .mSeedCost = 50,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "WALL_NUT" },
	{ .mSeedType = SeedType::SEED_POTATOMINE,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_POTATOMINE,    .mPacketIndex = 37, .mSeedCost = 25,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "POTATO_MINE" },
	{ .mSeedType = SeedType::SEED_SNOWPEA,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SNOWPEA,       .mPacketIndex = 4,  .mSeedCost = 175, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "SNOW_PEA" },
	{ .mSeedType = SeedType::SEED_CHOMPER,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CHOMPER,       .mPacketIndex = 31, .mSeedCost = 150, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "CHOMPER" },
	// @pvz-online: 双发成型费用与 PC 的 2026-10-16 定案同步。
	{ .mSeedType = SeedType::SEED_REPEATER,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_REPEATER,      .mPacketIndex = 5,  .mSeedCost = 150, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "REPEATER" },
	{ .mSeedType = SeedType::SEED_PUFFSHROOM,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_PUFFSHROOM,    .mPacketIndex = 6,  .mSeedCost = 0,   .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "PUFF_SHROOM" },
	{ .mSeedType = SeedType::SEED_SUNSHROOM,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SUNSHROOM,     .mPacketIndex = 7,  .mSeedCost = 0,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 2500, .mPlantName = "SUN_SHROOM" },
	{ .mSeedType = SeedType::SEED_FUMESHROOM,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_FUMESHROOM,    .mPacketIndex = 9,  .mSeedCost = 75,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "FUME_SHROOM" },
	{ .mSeedType = SeedType::SEED_GRAVEBUSTER,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GRAVE_BUSTER,  .mPacketIndex = 40, .mSeedCost = 0,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "GRAVE_BUSTER" },
	{ .mSeedType = SeedType::SEED_HYPNOSHROOM,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_HYPNOSHROOM,   .mPacketIndex = 10, .mSeedCost = 75,  .mRefreshTime = 750,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "HYPNO_SHROOM" },
	{ .mSeedType = SeedType::SEED_SCAREDYSHROOM,     .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SCRAREYSHROOM, .mPacketIndex = 33, .mSeedCost = 25,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "SCAREDY_SHROOM" },
	{ .mSeedType = SeedType::SEED_ICESHROOM,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_ICESHROOM,     .mPacketIndex = 36, .mSeedCost = 75,  .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "ICE_SHROOM" },
	{ .mSeedType = SeedType::SEED_DOOMSHROOM,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_DOOMSHROOM,    .mPacketIndex = 20, .mSeedCost = 125, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "DOOM_SHROOM" },
	{ .mSeedType = SeedType::SEED_LILYPAD,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_LILYPAD,       .mPacketIndex = 19, .mSeedCost = 25,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "LILY_PAD" },
	{ .mSeedType = SeedType::SEED_SQUASH,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SQUASH,        .mPacketIndex = 21, .mSeedCost = 50,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "SQUASH" },
	// @pvz-online: 三道火力费用与 PC 的 2026-10-16 平衡定案对齐。
	{ .mSeedType = SeedType::SEED_THREEPEATER,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_THREEPEATER,   .mPacketIndex = 12, .mSeedCost = 200, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "THREEPEATER" },
	{ .mSeedType = SeedType::SEED_TANGLEKELP,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_TANGLEKELP,    .mPacketIndex = 17, .mSeedCost = 25,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "TANGLE_KELP" },
	{ .mSeedType = SeedType::SEED_JALAPENO,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_JALAPENO,      .mPacketIndex = 11, .mSeedCost = 125, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "JALAPENO" },
	{ .mSeedType = SeedType::SEED_SPIKEWEED,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SPIKEWEED,     .mPacketIndex = 22, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "SPIKEWEED" },
	{ .mSeedType = SeedType::SEED_TORCHWOOD,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_TORCHWOOD,     .mPacketIndex = 29, .mSeedCost = 175, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "TORCHWOOD" },
	{ .mSeedType = SeedType::SEED_TALLNUT,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_TALLNUT,       .mPacketIndex = 28, .mSeedCost = 125, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "TALL_NUT" },
	{ .mSeedType = SeedType::SEED_SEASHROOM,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SEASHROOM,     .mPacketIndex = 39, .mSeedCost = 0,   .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "SEA_SHROOM" },
	{ .mSeedType = SeedType::SEED_PLANTERN,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_PLANTERN,      .mPacketIndex = 38, .mSeedCost = 25,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 2500, .mPlantName = "PLANTERN" },
	{ .mSeedType = SeedType::SEED_CACTUS,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CACTUS,        .mPacketIndex = 15, .mSeedCost = 125, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "CACTUS" },
	{ .mSeedType = SeedType::SEED_BLOVER,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_BLOVER,        .mPacketIndex = 18, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "BLOVER" },
	{ .mSeedType = SeedType::SEED_SPLITPEA,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SPLITPEA,      .mPacketIndex = 32, .mSeedCost = 125, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "SPLIT_PEA" },
	{ .mSeedType = SeedType::SEED_STARFRUIT,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_STARFRUIT,     .mPacketIndex = 30, .mSeedCost = 125, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "STARFRUIT" },
	{ .mSeedType = SeedType::SEED_PUMPKINSHELL,      .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_PUMPKIN,       .mPacketIndex = 25, .mSeedCost = 125, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "PUMPKIN" },
	{ .mSeedType = SeedType::SEED_MAGNETSHROOM,      .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_MAGNETSHROOM,  .mPacketIndex = 35, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "MAGNET_SHROOM" },
	{ .mSeedType = SeedType::SEED_CABBAGEPULT,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CABBAGEPULT,   .mPacketIndex = 13, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 300,  .mPlantName = "CABBAGE_PULT" },
	{ .mSeedType = SeedType::SEED_FLOWERPOT,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_FLOWER_POT,    .mPacketIndex = 33, .mSeedCost = 25,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "FLOWER_POT" },
	{ .mSeedType = SeedType::SEED_KERNELPULT,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_KERNELPULT,    .mPacketIndex = 13, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 300,  .mPlantName = "KERNEL_PULT" },
	{ .mSeedType = SeedType::SEED_INSTANT_COFFEE,    .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_COFFEEBEAN,    .mPacketIndex = 33, .mSeedCost = 75,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "COFFEE_BEAN" },
	{ .mSeedType = SeedType::SEED_GARLIC,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GARLIC,        .mPacketIndex = 8,  .mSeedCost = 50,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "GARLIC" },
	{ .mSeedType = SeedType::SEED_UMBRELLA,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_UMBRELLALEAF,  .mPacketIndex = 23, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "UMBRELLA_LEAF" },
	{ .mSeedType = SeedType::SEED_MARIGOLD,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_MARIGOLD,      .mPacketIndex = 24, .mSeedCost = 50,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 2500, .mPlantName = "MARIGOLD" },
	// @pvz-online: 降低西瓜体系成型成本；闯关冰瓜直种费用同 PC 随底座降价。
	{ .mSeedType = SeedType::SEED_MELONPULT,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_MELONPULT,     .mPacketIndex = 14, .mSeedCost = 225, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 300,  .mPlantName = "MELON_PULT" },
	{ .mSeedType = SeedType::SEED_GATLINGPEA,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GATLINGPEA,    .mPacketIndex = 5,  .mSeedCost = 250, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "GATLING_PEA" },
	{ .mSeedType = SeedType::SEED_TWINSUNFLOWER,     .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_TWIN_SUNFLOWER,.mPacketIndex = 1,  .mSeedCost = 150, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 2500, .mPlantName = "TWIN_SUNFLOWER" },
	{ .mSeedType = SeedType::SEED_GLOOMSHROOM,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GLOOMSHROOM,   .mPacketIndex = 27, .mSeedCost = 150, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 200,  .mPlantName = "GLOOM_SHROOM" },
	{ .mSeedType = SeedType::SEED_CATTAIL,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CATTAIL,       .mPacketIndex = 27, .mSeedCost = 225, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "CATTAIL" },
	{ .mSeedType = SeedType::SEED_WINTERMELON,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_WINTER_MELON,  .mPacketIndex = 27, .mSeedCost = 200, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 300,  .mPlantName = "WINTER_MELON" },
	{ .mSeedType = SeedType::SEED_GOLD_MAGNET,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GOLD_MAGNET,   .mPacketIndex = 27, .mSeedCost = 50,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "GOLD_MAGNET" },
	{ .mSeedType = SeedType::SEED_SPIKEROCK,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SPIKEROCK,     .mPacketIndex = 27, .mSeedCost = 125, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "SPIKEROCK" },
	{ .mSeedType = SeedType::SEED_COBCANNON,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_COBCANNON,     .mPacketIndex = 16, .mSeedCost = 700, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 600,  .mPlantName = "COB_CANNON" },
	{ .mSeedType = SeedType::SEED_IMITATER,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_IMITATER,      .mPacketIndex = 33, .mSeedCost = 0,   .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "IMITATER" },
	{ .mSeedType = SeedType::SEED_EXPLODE_O_NUT,     .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_WALLNUT,       .mPacketIndex = 2,  .mSeedCost = 0,   .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "EXPLODE_O_NUT" },
	{ .mSeedType = SeedType::SEED_GIANT_WALLNUT,     .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_WALLNUT,       .mPacketIndex = 2,  .mSeedCost = 0,   .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "GIANT_WALLNUT" },
	{ .mSeedType = SeedType::SEED_SPROUT,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_ZENGARDEN_SPROUT, .mPacketIndex = 33, .mSeedCost = 0,   .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "SPROUT" },
	{ .mSeedType = SeedType::SEED_LEFTPEATER,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_REPEATER,      .mPacketIndex = 5,  .mSeedCost = 200, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "REPEATER" }
};

static int CobCannonArmFrames(LawnApp* theApp, int theBaseFrames)
{
    return (int)(theBaseFrames * theApp->RunPlantUpgradeMul(SeedType::SEED_COBCANNON) + 0.5f);
}

static bool PlantInLanternLight(LawnApp* theApp, Plant* thePlant)
{
    if (thePlant == nullptr || thePlant->mBoard == nullptr) return false;
    if (theApp->RunPlantBuff2Count(SeedType::SEED_PLANTERN) <= 0) return false;

    int aExtra = theApp->RunPlantUpgradeCount(SeedType::SEED_PLANTERN);
    Plant* aLantern = nullptr;
    while (thePlant->mBoard->IteratePlants(aLantern))
    {
        if (aLantern->mSeedType != SeedType::SEED_PLANTERN) continue;
        int aDistX = aLantern->mPlantCol - thePlant->mPlantCol;
        if (aDistX < 0) aDistX = -aDistX;
        int aDistY = aLantern->mRow - thePlant->mRow;
        if (aDistY < 0) aDistY = -aDistY;
        if (aDistX <= 3 + aExtra && aDistY <= 2 + aExtra && aDistX + aDistY <= 5 + 2 * aExtra)
            return true;
    }
    return false;
}

static float PlantShootSpeed(LawnApp* theApp, SeedType theSeedType, Plant* thePlant)
{
    float aMul = theApp->RunBuffMul(RUN_BUFF_SWIFT) * theApp->RunPlantUpgradeMulKind(theSeedType, RUN_UPGRADE_KIND_RHYTHM)
        * theApp->RunPlantBuff2MulKind(theSeedType, RUN_UPGRADE_KIND_RHYTHM);	// 第二 buff：同 Kind 闸门口（双发「火力压制」×0.7/层）
    // @pvz-online: 第二 buff「Twilight」（路灯花 #25，docs/06 §8.6）：照亮格内植物攻速
    // ×0.75/层——间隔因子 0.75^n 乘进 aMul（值越小越快，与节奏族同向；未抽/不在照亮格恒 1）。
    if (PlantInLanternLight(theApp, thePlant))
    {
        aMul *= theApp->RunPlantBuff2Mul(SeedType::SEED_PLANTERN);
    }
    return 1.0f / aMul;
}

static int PlantShootTicks(int theBaseTicks, float theSpeed)
{
    int aScaled = (int)(theBaseTicks / theSpeed + 0.5f);
    return aScaled < 1 ? 1 : aScaled;
}

static Zombie* FindHomingStarTarget(Board* theBoard, float theCenterX, float theCenterY, int theDamageRangeFlags, Zombie** theExcludes, int theExcludeCount)
{
    Zombie* aBestZombie = nullptr;
    int aHighestWeight = 0;
    Zombie* aZombie = nullptr;
    while (theBoard->IterateZombies(aZombie))
    {
        if (!aZombie->EffectedByDamage(theDamageRangeFlags))
        {
            continue;
        }
        bool aExcluded = false;
        for (int i = 0; i < theExcludeCount; i++)
        {
            if (theExcludes[i] == aZombie)
            {
                aExcluded = true;
                break;
            }
        }
        if (aExcluded)
        {
            continue;
        }

        Rect aZombieRect = aZombie->GetZombieRect();
        int aWeight = -Distance2D(theCenterX, theCenterY, aZombieRect.mX + aZombieRect.mWidth / 2, aZombieRect.mY + aZombieRect.mHeight / 2);
        if (aBestZombie == nullptr || aWeight > aHighestWeight)
        {
            aHighestWeight = aWeight;
            aBestZombie = aZombie;
        }
    }
    return aBestZombie;
}

void Plant::AttachTanglekelpGrab(Zombie* theZombie)
{
    theZombie->PoolSplash(false);

    float aVinesPosX = -13.0f;
    float aVinesPosY = 15.0f;
    if (theZombie->mZombieType == ZombieType::ZOMBIE_SNORKEL)
    {
        aVinesPosX = -43.0f;
        aVinesPosY = 55.0f;
    }
    if (theZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING)
    {
        aVinesPosX = -20.0f;
        aVinesPosY = 37.0f;
    }
    Reanimation* aGrabReanim = theZombie->AddAttachedReanim(aVinesPosX, aVinesPosY, ReanimationType::REANIM_TANGLEKELP);
    if (aGrabReanim)
    {
        aGrabReanim->SetFramesForLayer("anim_grab");
        aGrabReanim->mAnimRate = 24.0f;
        aGrabReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
    }
}

bool Plant::IsTangleKelpTargeting(ZombieID theZombieID)
{
    if (theZombieID == ZombieID::ZOMBIEID_NULL)
    {
        return false;
    }
    if (mTargetZombieID == theZombieID)
    {
        return true;
    }
    for (int i = 0; i < mExtraTanglekelpCount; i++)
    {
        if (mExtraTanglekelpIDs[i] == theZombieID)
        {
            return true;
        }
    }
    return false;
}

int Plant::SpikeweedCycleFrames(LawnApp* theApp, SeedType theSeedType)
{
    // @pvz-online: 生命词条不能改变攻击循环，修复「棘冠」加血反而减攻速。
    return (int)(100 * theApp->RunPlantUpgradeMulKind(theSeedType, RUN_UPGRADE_KIND_RHYTHM) + 0.5f);
}

static int MagnetShroomRechargeFrames(LawnApp* theApp)
{
    return (int)(1500 * theApp->RunPlantUpgradeMul(SeedType::SEED_MAGNETSHROOM) + 0.5f);
}

static SeedType GetUpgradeBaseFor(SeedType theSeedType)
{
    switch (theSeedType)
    {
    case SeedType::SEED_GATLINGPEA:     return SeedType::SEED_REPEATER;
    case SeedType::SEED_WINTERMELON:    return SeedType::SEED_MELONPULT;
    case SeedType::SEED_TWINSUNFLOWER:  return SeedType::SEED_SUNFLOWER;
    case SeedType::SEED_SPIKEROCK:      return SeedType::SEED_SPIKEWEED;
    case SeedType::SEED_COBCANNON:      return SeedType::SEED_KERNELPULT;
    case SeedType::SEED_GOLD_MAGNET:    return SeedType::SEED_MAGNETSHROOM;
    case SeedType::SEED_GLOOMSHROOM:    return SeedType::SEED_FUMESHROOM;
    case SeedType::SEED_CATTAIL:        return SeedType::SEED_LILYPAD;
    default:                            return SeedType::SEED_NONE;
    }
}
Plant::Plant()
{
}

// @pvz-online: 「Nut Bowling」扩展表词条：坚果变保龄球。IsWallnutBowlingLevel 在闯关模式
// 恒 false（它驱动档案关型判定副作用，不能翻），各行为点按本词条单独放行；模仿者解析到本株。
static bool IsBowlingNut(const Plant* thePlant)
{
    SeedType aType = thePlant->mSeedType;
    if (aType == SeedType::SEED_IMITATER && thePlant->mImitaterType != SeedType::SEED_NONE)
        aType = thePlant->mImitaterType;
    return aType == SeedType::SEED_WALLNUT && thePlant->mApp->RunExtraBuffCount(RUN_BUFF2_NUT_BOWLING) > 0;
}

void Plant::PlantInitialize(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType)
{
    mPlantCol = theGridX;
    mRow = theGridY;
    if (mBoard)
    {
        mX = mBoard->GridToPixelX(theGridX, theGridY);
        mY = mBoard->GridToPixelY(theGridX, theGridY);
    }
    mAnimCounter = 0;
    mAnimPing = true;
    mFrame = 0;
    mShootingCounter = 0;
    mShakeOffsetX = 0.0f;
    mShakeOffsetY = 0.0f;
    mFrameLength = RandRangeInt(12, 18);
    mTargetX = -1;
    mTargetY = -1;
    mStartRow = mRow;
    mNumFrames = 5;
    mState = PlantState::STATE_NOTREADY;
    mDead = false;
    mSquished = false;
    mSeedType = theSeedType;
    mImitaterType = theImitaterType;
    mPlantHealth = 300;
    mDoSpecialCountdown = 0;
    mDisappearCountdown = 200;
    mStateCountdown = 0;
    // @pvz-online: 缠绕海草「Entangle」多缠（批 13）：额外目标槽清零——抓取外
    // IsTangleKelpTargeting 会遍历它判"这只正被这株缠着"，不清会读到脏值。
    mExtraTanglekelpCount = 0;
    mExtraTanglekelpIDs[0] = ZombieID::ZOMBIEID_NULL;
    mExtraTanglekelpIDs[1] = ZombieID::ZOMBIEID_NULL;
    // @pvz-online: 窝瓜多砸（批八）——<0 = 首次锁定目标时还没记账；起跳点先清 0
    // （首跳前 PRE_LAUNCH 转移处必写，见 UpdateSquash）。
    mSquashSmashesLeft = -1;
    mSquashFromX = 0.0f;
    mSquashFromY = 0.0f;
    mMineregrowsLeft = 0;	// @pvz-online: legacy field retained; recursive mine network no longer uses a regrowth budget.
    mRunDispatchCursor = 0;
    mRunAliveTicks = 0;
    mRunGarlicCooldown = 0;
    mRunPurpleShots = 0;
    mRunSunCollectTicks = 0;
    mChainBitesLeft = 0;	// @pvz-online: 连锁吞（第二 buff 批 8）——非闯关/非种下路径恒 0
    mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
    mBodyReanimID = ReanimationID::REANIMATIONID_NULL;
    mHeadReanimID = ReanimationID::REANIMATIONID_NULL;
    mHeadReanimID2 = ReanimationID::REANIMATIONID_NULL;
    mHeadReanimID3 = ReanimationID::REANIMATIONID_NULL;
    mBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
    mLightReanimID = ReanimationID::REANIMATIONID_NULL;
    mSleepingReanimID = ReanimationID::REANIMATIONID_NULL;
    mBlinkCountdown = 0;
    mRecentlyEatenCountdown = 0;
    mEatenFlashCountdown = 0;
    mBeghouledFlashCountdown = 0;
    mWidth = 80;
    mHeight = 80;
    memset(mMagnetItems, 0, sizeof(mMagnetItems));
    const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
    mIsAsleep = false;
    mWakeUpCounter = 0;
    mOnBungeeState = PlantOnBungeeState::NOT_ON_BUNGEE;
    mPottedPlantIndex = -1;
    mLaunchRate = aPlantDef.mLaunchRate;
    mSubclass = aPlantDef.mSubClass;
    mRenderOrder = CalcRenderOrder();
    if (IsOnBoard() && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN &&
        (theSeedType == SeedType::SEED_LILYPAD || theSeedType == SeedType::SEED_FLOWERPOT ||
         (theSeedType == SeedType::SEED_IMITATER &&
          (theImitaterType == SeedType::SEED_LILYPAD || theImitaterType == SeedType::SEED_FLOWERPOT))))
    {
        PlantsOnLawn aPlants;
        mBoard->GetPlantsOnLawn(mPlantCol, mRow, &aPlants);
        SeedType aSupportType = theSeedType == SeedType::SEED_IMITATER ? theImitaterType : theSeedType;
        Plant* aOther = aSupportType == SeedType::SEED_LILYPAD ? aPlants.mFlowerPotPlant : aPlants.mLilypadPlant;
        if (aOther)
        {
            // Keep the pair within its existing render band; the later support is exposed first.
            aOther->mRenderOrder = mRenderOrder;
            if (Reanimation* aOtherBody = mApp->ReanimationTryToGet(aOther->mBodyReanimID))
                aOtherBody->mRenderOrder = mRenderOrder + 1;
            mRenderOrder += 2;
        }
    }

    Reanimation* aBodyReanim = nullptr;
    if (aPlantDef.mReanimationType != ReanimationType::REANIM_NONE)
    {
        float aOffsetY = PlantDrawHeightOffset(mBoard, this, mSeedType, mPlantCol, mRow);
        aBodyReanim = mApp->AddReanimation(0.0f, aOffsetY, mRenderOrder + 1, aPlantDef.mReanimationType);
        aBodyReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
        aBodyReanim->mAnimRate = RandRangeFloat(10.0f, 15.0f);

        if (aBodyReanim->TrackExists("anim_idle"))
            aBodyReanim->SetFramesForLayer("anim_idle");

        if ((mApp->IsWallnutBowlingLevel() || IsBowlingNut(this)) && aBodyReanim->TrackExists("_ground"))
        {
            aBodyReanim->SetFramesForLayer("_ground");
            if (mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_EXPLODE_O_NUT)
                aBodyReanim->mAnimRate = RandRangeFloat(12.0f, 18.0f);
            else if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
                aBodyReanim->mAnimRate = RandRangeFloat(6.0f, 10.0f);
        }

        aBodyReanim->mIsAttachment = true;
        mBodyReanimID = mApp->ReanimationGetID(aBodyReanim);
        mBlinkCountdown = 400 + Sexy::Rand(400);
    }

    // @pvz-online: 闯关里蘑菇种下即醒（用户定案）——白天也照常干活；咖啡豆因此改行
    // 产阳光（见 DoSpecial），植物价格在 GetCost 里 +25。
    if (IsNocturnal(mSeedType) && mBoard && !mBoard->StageIsNight() && !mApp->IsRunMode())
        SetSleeping(true);

    if (mLaunchRate > 0)
    {
        // @pvz-online: 闯关 buff「丰饶」：产阳光间隔表内叠乘（2026-10-09 全局批起 ×0.75/层）——
        // 初始等待与每次重置都要乘，所以两处一起改（另一处在产阳光的重置那行）。
        if (MakesSun())
        {
            float aSunIntervalMul = mApp->RunBuffMul(RUN_BUFF_ABUNDANCE);
            // @pvz-online: 经济批 2026-10-09：阳光菇「亮顶」改速度轴——产阳光间隔 ×0.75/层叠乘，
            // 初始等待同乘（口径同丰饶：「间隔型」两处都要乘，漏一处首轮就不缩短）。
            if (mSeedType == SeedType::SEED_SUNSHROOM)
                aSunIntervalMul *= mApp->RunPlantUpgradeMul(SeedType::SEED_SUNSHROOM);
            mLaunchCounter = (int)(RandRangeInt(300, mLaunchRate / 2) * aSunIntervalMul + 0.5f);
            // @pvz-online: 经济批 2026-10-09：向日葵「丰收」重做——「种下立即产 1 次阳光」把
            // 首轮计时压 0（下一次 UpdateProductionPlant 即落币，随后回正常节奏）。
            if (mSeedType == SeedType::SEED_SUNFLOWER && mApp->RunPlantUpgradeCount(SeedType::SEED_SUNFLOWER) > 0)
                mLaunchCounter = 0;
        }
        else
        {
            // @pvz-online: 闯关 buff「急袭」：射手首发等待也缩短（稳态周期那处在 UpdateShooter）。
            // 只认射手——这条文案是"攻击更快"，非射手的 mLaunchCounter 另有用途（产币等）。
            float aShootMul = mSubclass == PlantSubClass::SUBCLASS_SHOOTER ? mApp->RunBuffMul(RUN_BUFF_SWIFT) : 1.0f;
            mLaunchCounter = (int)(RandRangeInt(0, mLaunchRate) * aShootMul + 0.5f);
        }
    }
    else
        mLaunchCounter = 0;

    switch (theSeedType)
    {
    case SeedType::SEED_BLOVER:
    {
        mDoSpecialCountdown = 50;

        if (IsInPlay())
        {
            aBodyReanim->SetFramesForLayer("anim_blow");
            aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
            aBodyReanim->mAnimRate = 20.0f;
        }
        else
        {
            aBodyReanim->SetFramesForLayer("anim_idle");
            aBodyReanim->mAnimRate = 10.0f;
        }

        break;
    }
    case SeedType::SEED_PEASHOOTER:
    case SeedType::SEED_SNOWPEA:
    case SeedType::SEED_REPEATER:
    case SeedType::SEED_LEFTPEATER:
    case SeedType::SEED_GATLINGPEA:
        if (aBodyReanim)
        {
            aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
            Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
            aHeadReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
            aHeadReanim->mAnimRate = aBodyReanim->mAnimRate;
            aHeadReanim->SetFramesForLayer("anim_head_idle");
            mHeadReanimID = mApp->ReanimationGetID(aHeadReanim);

            if (aBodyReanim->TrackExists("anim_stem"))
                aHeadReanim->AttachToAnotherReanimation(aBodyReanim, "anim_stem");
            else if (aBodyReanim->TrackExists("anim_idle"))
                aHeadReanim->AttachToAnotherReanimation(aBodyReanim, "anim_idle");
        }
        break;
    case SeedType::SEED_SPLITPEA:
    {
        PVZP_ASSERT(aBodyReanim);

        aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
        Reanimation* aHeadReanim1 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
        aHeadReanim1->mLoopType = ReanimLoopType::REANIM_LOOP;
        aHeadReanim1->mAnimRate = aBodyReanim->mAnimRate;
        aHeadReanim1->SetFramesForLayer("anim_head_idle");
        aHeadReanim1->AttachToAnotherReanimation(aBodyReanim, "anim_idle");
        mHeadReanimID = mApp->ReanimationGetID(aHeadReanim1);

        Reanimation* aHeadReanim2 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
        aHeadReanim2->mLoopType = ReanimLoopType::REANIM_LOOP;
        aHeadReanim2->mAnimRate = aBodyReanim->mAnimRate;
        aHeadReanim2->SetFramesForLayer("anim_splitpea_idle");
        aHeadReanim2->AttachToAnotherReanimation(aBodyReanim, "anim_idle");
        mHeadReanimID2 = mApp->ReanimationGetID(aHeadReanim2);

        break;
    }
    case SeedType::SEED_THREEPEATER:
    {
        PVZP_ASSERT(aBodyReanim);

        aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
        Reanimation* aHeadReanim1 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
        aHeadReanim1->mLoopType = ReanimLoopType::REANIM_LOOP;
        aHeadReanim1->mAnimRate = aBodyReanim->mAnimRate;
        aHeadReanim1->SetFramesForLayer("anim_head_idle1");
        aHeadReanim1->AttachToAnotherReanimation(aBodyReanim, "anim_head1");
        mHeadReanimID = mApp->ReanimationGetID(aHeadReanim1);

        Reanimation* aHeadReanim2 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
        aHeadReanim2->mLoopType = ReanimLoopType::REANIM_LOOP;
        aHeadReanim2->mAnimRate = aBodyReanim->mAnimRate;
        aHeadReanim2->SetFramesForLayer("anim_head_idle2");
        aHeadReanim2->AttachToAnotherReanimation(aBodyReanim, "anim_head2");
        mHeadReanimID2 = mApp->ReanimationGetID(aHeadReanim2);

        Reanimation* aHeadReanim3 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
        aHeadReanim3->mLoopType = ReanimLoopType::REANIM_LOOP;
        aHeadReanim3->mAnimRate = aBodyReanim->mAnimRate;
        aHeadReanim3->SetFramesForLayer("anim_head_idle3");
        aHeadReanim3->AttachToAnotherReanimation(aBodyReanim, "anim_head3");
        mHeadReanimID3 = mApp->ReanimationGetID(aHeadReanim3);

        break;
    }
    case SeedType::SEED_WALLNUT:
        mPlantHealth = 4000;
        mBlinkCountdown = 1000 + Sexy::Rand(1000);
        break;
    case SeedType::SEED_EXPLODE_O_NUT:
        mPlantHealth = 4000;
        mBlinkCountdown = 1000 + Sexy::Rand(1000);
        aBodyReanim->mColorOverride = Color(255, 64, 64);
        break;
    case SeedType::SEED_GIANT_WALLNUT:
        mPlantHealth = 4000;
        mBlinkCountdown = 1000 + Sexy::Rand(1000);
        break;
    case SeedType::SEED_TALLNUT:
        mPlantHealth = 8000;
        mHeight = 80;
        mBlinkCountdown = 1000 + Sexy::Rand(1000);
        break;
    case SeedType::SEED_GARLIC:
        PVZP_ASSERT(aBodyReanim);
        mPlantHealth = 400;
        aBodyReanim->SetTruncateDisappearingFrames();
        break;
    case SeedType::SEED_GOLD_MAGNET:
        PVZP_ASSERT(aBodyReanim);
        aBodyReanim->SetTruncateDisappearingFrames();
        break;
    case SeedType::SEED_IMITATER:
        PVZP_ASSERT(aBodyReanim);
        aBodyReanim->mAnimRate = RandRangeFloat(25.0f, 30.0f);
        mStateCountdown = 200;
        break;
    case SeedType::SEED_CHERRYBOMB:
    case SeedType::SEED_JALAPENO:
    {
        PVZP_ASSERT(aBodyReanim);

        if (IsInPlay())
        {
            mDoSpecialCountdown = 100;

            aBodyReanim->SetFramesForLayer("anim_explode");
            aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;

            mApp->PlayFoley(FoleyType::FOLEY_REVERSE_EXPLOSION);
        }

        break;
    }
    case SeedType::SEED_POTATOMINE:
    {
        PVZP_ASSERT(aBodyReanim);

        aBodyReanim->mAnimRate = 12.0f;

        if (IsInPlay())
        {
            aBodyReanim->AssignRenderGroupToTrack("anim_glow", RENDER_GROUP_HIDDEN);
            // @pvz-online: quick arming applies to every generation, without the removed seismic stun.
            mStateCountdown = mApp->RunPlantUpgradeCount(SeedType::SEED_POTATOMINE) > 0 ? 375 : 1500;
        }
        else
        {
            aBodyReanim->SetFramesForLayer("anim_armed");
            mState = PlantState::STATE_POTATO_ARMED;
        }

        break;
    }
    case SeedType::SEED_GRAVEBUSTER:
    {
        PVZP_ASSERT(aBodyReanim);

        if (IsInPlay())
        {
            aBodyReanim->SetFramesForLayer("anim_land");
            aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;

            mState = PlantState::STATE_GRAVEBUSTER_LANDING;
            mApp->PlayFoley(FoleyType::FOLEY_GRAVEBUSTERCHOMP);
        }

        break;
    }
    case SeedType::SEED_SUNSHROOM:
    {
        PVZP_ASSERT(aBodyReanim);

        aBodyReanim->mFrameBasePose = 6;

        if (IsInPlay())
        {
            mX += Sexy::Rand(10) - 5;
            mY += Sexy::Rand(10) - 5;
        }
        else if (mIsAsleep)
            aBodyReanim->SetFramesForLayer("anim_bigsleep");
        else
            aBodyReanim->SetFramesForLayer("anim_bigidle");

        mState = PlantState::STATE_SUNSHROOM_SMALL;
        mStateCountdown = 12000;

        break;
    }
    case SeedType::SEED_PUFFSHROOM:
    case SeedType::SEED_SEASHROOM:
        if (IsInPlay())
        {
            mX += Sexy::Rand(10) - 5;
            mY += Sexy::Rand(6) - 3;
        }
        break;
    case SeedType::SEED_PUMPKINSHELL:
    {
        mPlantHealth = 4000;
        mWidth = 120;

        PVZP_ASSERT(aBodyReanim);
        aBodyReanim->AssignRenderGroupToTrack("Pumpkin_back", 1);
        break;
    }
    case SeedType::SEED_CHOMPER:
        if (mApp->RunExtraBuffCount(RUN_BUFF2_DIGEST_HEAL) > 0) mPlantHealth = 3000;
        mState = PlantState::STATE_READY;
        break;
    case SeedType::SEED_PLANTERN:
    {
        mStateCountdown = 50;

        if (!IsOnBoard() || mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
        {
            AddAttachedParticle(mX + 40, mY + 40, (int)RenderLayer::RENDER_LAYER_FOG + 1, ParticleEffect::PARTICLE_LANTERN_SHINE);
        }
        if (IsInPlay())
        {
            mApp->PlaySample(Sexy::SOUND_PLANTERN);
        }

        break;
    }
    case SeedType::SEED_TORCHWOOD:
        break;
    case SeedType::SEED_MARIGOLD:
        PVZP_ASSERT(aBodyReanim);
        aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
        break;
    case SeedType::SEED_CACTUS:
        mState = PlantState::STATE_CACTUS_LOW;
        break;
    case SeedType::SEED_INSTANT_COFFEE:
        mDoSpecialCountdown = 100;
        break;
    case SeedType::SEED_SCAREDYSHROOM:
        mState = PlantState::STATE_READY;
        break;
    case SeedType::SEED_COBCANNON:
        if (IsInPlay())
        {
            mState = PlantState::STATE_COBCANNON_ARMING;
            mStateCountdown = CobCannonArmFrames(mApp, 500);

            PVZP_ASSERT(aBodyReanim);
            aBodyReanim->SetFramesForLayer("anim_unarmed_idle");
        }
        break;
    case SeedType::SEED_KERNELPULT:
        PVZP_ASSERT(aBodyReanim);
        aBodyReanim->AssignRenderGroupToPrefix("Cornpult_butter", RENDER_GROUP_HIDDEN);
        break;
    case SeedType::SEED_MAGNETSHROOM:
        PVZP_ASSERT(aBodyReanim);
        aBodyReanim->SetTruncateDisappearingFrames();
        break;
    case SeedType::SEED_SPIKEROCK:
        mPlantHealth = 450;
        PVZP_ASSERT(aBodyReanim);
        break;
    case SeedType::SEED_SPROUT:
        break;
    case SeedType::SEED_FLOWERPOT:
        if (IsInPlay())
        {
            mState = PlantState::STATE_FLOWERPOT_INVULNERABLE;
            mStateCountdown = 100;
        }
        break;
    case SeedType::SEED_LILYPAD:
        if (IsInPlay())
        {
            mState = PlantState::STATE_LILYPAD_INVULNERABLE;
            mStateCountdown = 100;
        }
        break;
    case SeedType::SEED_TANGLEKELP:
        PVZP_ASSERT(aBodyReanim);
        aBodyReanim->SetTruncateDisappearingFrames();
    default:
        break;
    }

    if ((mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BIG_TIME) &&
        (theSeedType == SeedType::SEED_WALLNUT || theSeedType == SeedType::SEED_SUNFLOWER || theSeedType == SeedType::SEED_MARIGOLD))
    {
        mPlantHealth *= 2;
    }
    // @pvz-online: 闯关 buff「扎根」：全体植物血量 ×(1+75%/层)（2026-10-04 批七由 150% 下调）。
    // 单株升级（睡莲「厚垫」等）在同一行一起取——两乘数相乘。修正批（2026-10-03）改走 Kind 闸门口：
    // 只有血量型条目（Kind=HEALTH：睡莲/南瓜头/花盆/保护伞/地刺王——地刺王 2026-10-04 批七
    // 由攻击间隔族换成血量族；坚果/高坚果批七换成耐砸，已不在血量族）才乘——此前任意行都乘，
    // 双发/寒冰这类非血量条目会把该株血量一起改了。非闯关局都是 1.0，对 300/4000 无损。
    if (theSeedType == SeedType::SEED_GLOOMSHROOM && mApp->RunPlantBuff2Count(theSeedType) > 0)
        mPlantHealth = 1000;
    mPlantHealth = (int)(mPlantHealth * mApp->RunBuffMul(RUN_BUFF_ROOTED) * mApp->RunPlantUpgradeMulKind(theSeedType, RUN_UPGRADE_KIND_HEALTH) + 0.5f);
    // @pvz-online: 第二 buff「血量改写」：忧郁菇耐砸的1000为基础生命，先设基值再乘血量加成。
    // 磐石4000和引路蒜20仍在乘法链后绝对覆盖，不改变其他植物规则。
    // 引路蒜 20 + 被吃全行换道（2★，换道在 Zombie::EatPlant → SwitchLanes）。
    if (mApp->RunPlantBuff2Count(theSeedType) > 0)
    {
        if (theSeedType == SeedType::SEED_SCAREDYSHROOM) mPlantHealth = 4000;
        else if (theSeedType == SeedType::SEED_GARLIC) mPlantHealth = 20;
    }
    // @pvz-online: steel shell halves the final HP after existing health multipliers.
    if (theSeedType == SeedType::SEED_PUMPKINSHELL && mApp->RunExtraBuffCount(RUN_BUFF2_STEEL_PUMPKIN) > 0)
        mPlantHealth = std::max(1, mPlantHealth / 2);
    mPlantMaxHealth = mPlantHealth;

    if (mSeedType != SeedType::SEED_FLOWERPOT && IsOnBoard())
    {
        PVZP_ASSERT(mBoard);
        Plant* aFlowerPot = mBoard->GetFlowerPotAt(mPlantCol, mRow);
        if (aFlowerPot)
            mApp->ReanimationGet(aFlowerPot->mBodyReanimID)->mAnimRate = 0.0f;
    }
}

int Plant::CalcRenderOrder()
{
	PLANT_ORDER anOrder = PLANT_ORDER::PLANT_ORDER_NORMAL;
	RenderLayer aLayer = RenderLayer::RENDER_LAYER_PLANT;

	SeedType aSeedType = mSeedType;
	if (mSeedType == SeedType::SEED_IMITATER && mImitaterType != SeedType::SEED_NONE)
        aSeedType = mImitaterType;

    if (mApp->IsWallnutBowlingLevel() || IsBowlingNut(this))
    {
        aLayer = RenderLayer::RENDER_LAYER_PROJECTILE;
    }
	else if (aSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		anOrder = PLANT_ORDER::PLANT_ORDER_PUMPKIN;
	}
	else if (IsFlying(aSeedType))
	{
		anOrder = PLANT_ORDER::PLANT_ORDER_FLYER;
	}
	else if (aSeedType == SeedType::SEED_FLOWERPOT || (aSeedType == SeedType::SEED_LILYPAD && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN))
	{
		anOrder = PLANT_ORDER::PLANT_ORDER_LILYPAD;
	}

	return Board::MakeRenderOrder(aLayer, mRow, anOrder * 5 - mX + 800);
}

void Plant::SetSleeping(bool theIsAsleep)
{
	if (mIsAsleep == theIsAsleep || NotOnGround())
		return;

	mIsAsleep = theIsAsleep;
	if (theIsAsleep)
	{
		float aPosX = mX + 50.0f;
		float aPosY = mY + 40.0f;
		if (mSeedType == SeedType::SEED_FUMESHROOM)
			aPosX += 12.0f;
		else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
			aPosY -= 20.0f;
		else if (mSeedType == SeedType::SEED_GLOOMSHROOM)
			aPosY -= 12.0f;

		Reanimation* aSleepReanim = mApp->AddReanimation(aPosX, aPosY, mRenderOrder + 2, ReanimationType::REANIM_SLEEPING);
		if (aSleepReanim)
		{
			aSleepReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
			aSleepReanim->mAnimRate = RandRangeFloat(6.0f, 8.0f);
			aSleepReanim->mAnimTime = RandRangeFloat(0.0f, 0.9f);
			mSleepingReanimID = mApp->ReanimationGetID(aSleepReanim);
		}
	}
	else
	{
		mApp->RemoveReanimation(mSleepingReanimID);
		mSleepingReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	if (theIsAsleep)
	{
		if (!IsInPlay() && mSeedType == SeedType::SEED_SUNSHROOM)
		{
			aBodyReanim->SetFramesForLayer("anim_bigsleep");
		}
		else if (aBodyReanim->TrackExists("anim_sleep"))
		{
			float aAnimTime = aBodyReanim->mAnimTime;
			aBodyReanim->StartBlend(20);
			aBodyReanim->SetFramesForLayer("anim_sleep");
			aBodyReanim->mAnimTime = aAnimTime;
		}
		else
		{
			aBodyReanim->mAnimRate = 1.0f;
		}

		EndBlink();
	}
	else
	{
		if (!IsInPlay() && mSeedType == SeedType::SEED_SUNSHROOM)
		{
			aBodyReanim->SetFramesForLayer("anim_bigidle");
		}
		else if (aBodyReanim->TrackExists("anim_idle"))
		{
			float aAnimTime = aBodyReanim->mAnimTime;
			aBodyReanim->StartBlend(20);
			aBodyReanim->SetFramesForLayer("anim_idle");
			aBodyReanim->mAnimTime = aAnimTime;
		}

		if (aBodyReanim->mAnimRate < 2.0f && IsInPlay())
			aBodyReanim->mAnimRate = RandRangeFloat(10.0f, 15.0f);
	}
}

int Plant::GetDamageRangeFlags(PlantWeapon thePlantWeapon)
{
	switch (mSeedType)
	{
	case SeedType::SEED_CACTUS:
		return thePlantWeapon == PlantWeapon::WEAPON_SECONDARY ? 1 : 2;
	case SeedType::SEED_CHERRYBOMB:
	case SeedType::SEED_JALAPENO:
	case SeedType::SEED_COBCANNON:
	case SeedType::SEED_DOOMSHROOM:
		return 127;
	case SeedType::SEED_MELONPULT:
	case SeedType::SEED_CABBAGEPULT:
	case SeedType::SEED_KERNELPULT:
	case SeedType::SEED_WINTERMELON:
		return 13;
	case SeedType::SEED_POTATOMINE:
		return 77;
	case SeedType::SEED_SQUASH:
		return 13;
	case SeedType::SEED_PUFFSHROOM:
	case SeedType::SEED_SEASHROOM:
	case SeedType::SEED_FUMESHROOM:
	case SeedType::SEED_GLOOMSHROOM:
	case SeedType::SEED_CHOMPER:
		return 9;
	case SeedType::SEED_CATTAIL:
		return 11;
	case SeedType::SEED_TANGLEKELP:
		return 5;
	case SeedType::SEED_GIANT_WALLNUT:
		return 17;
	default:
		return 1;
	}
}

bool Plant::IsOnHighGround()
{
	return mBoard && mBoard->mGridSquareType[mPlantCol][mRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND;
}

void Plant::SpikeRockTakeDamage()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

	SpikeweedAttack();

	mPlantHealth -= 50;
	if (mPlantHealth <= 300)
	{
		aBodyReanim->AssignRenderGroupToTrack("bigspike3", RENDER_GROUP_HIDDEN);
	}
	if (mPlantHealth <= 150)
	{
		aBodyReanim->AssignRenderGroupToTrack("bigspike2", RENDER_GROUP_HIDDEN);
	}
	if (mPlantHealth <= 0)
	{
		mApp->PlayFoley(FoleyType::FOLEY_SQUISH);
		Die();
	}
}

bool Plant::IsSpiky()
{
	return mSeedType == SeedType::SEED_SPIKEWEED || mSeedType == SeedType::SEED_SPIKEROCK;
}

// @pvz-online: 「emotional damage」（2026-10-15 用户定案）的条件判定：周围 8 格
//（3×3 减自身）没有其他非承载植物。即时判定口径（用户拍板）：每发
// 雾爆各自查，种上/铲掉的下一发即刻生效；同帧同态下整轮四发结果一致。
static bool GloomIsLonely(Plant* thePlant)
{
    Plant* aPlant = nullptr;
    while (thePlant->mBoard->IteratePlants(aPlant))
    {
        if (aPlant == thePlant || aPlant->mDead) continue;
        if (aPlant->mSeedType == SeedType::SEED_FLOWERPOT || aPlant->mSeedType == SeedType::SEED_LILYPAD) continue;
        if (abs(aPlant->mPlantCol - thePlant->mPlantCol) <= 1 &&
            abs(aPlant->mRow - thePlant->mRow) <= 1)
        {
            return false;
        }
    }
    return true;
}


void Plant::DoRowAreaDamage(int theDamage, unsigned int theDamageFlags)
{
    // @pvz-online: both normal volleys and Continuous Fumes pass this damage entrance once.
    if (mSeedType == SeedType::SEED_FUMESHROOM || mSeedType == SeedType::SEED_GLOOMSHROOM)
        theDamage = static_cast<int>(theDamage * (1.0f + 0.35f * mApp->RunExtraBuffCount(RUN_BUFF2_SHARED_FUMES)) + 0.5f);
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
    Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
    bool anEmoHeal = mSeedType == SEED_GLOOMSHROOM && mApp->RunExtraBuffCount(RUN_BUFF2_EMO_DAMAGE) > 0 && GloomIsLonely(this);

    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        int aDiffY = (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS) ? 0 : (aZombie->mRow - mRow);
        if (mSeedType == SeedType::SEED_GLOOMSHROOM)
        {
            // @pvz-online: 闯关「Gloom」（忧郁菇，批 14）：光环的纵向行差随层数一齐外扩
            // （±(1+层)），与 GetPlantAttackRect 的矩形同步——不然矩形扩了、行差还卡三行。
            int aGloomRows = mApp->RunPlantUpgradeCount(SeedType::SEED_GLOOMSHROOM);
            if (aDiffY < -1 - aGloomRows || aDiffY > 1 + aGloomRows)
                continue;
        }
        else if (aDiffY)
            continue;

        if (aZombie->mOnHighGround == IsOnHighGround() && aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            Rect aZombieRect = aZombie->GetZombieRect();
            if (GetRectOverlap(aAttackRect, aZombieRect) > 0)
            {
                int aDamage = theDamage;
                if ((aZombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI || aZombie->mZombieType == ZombieType::ZOMBIE_CATAPULT) &&
                    (TestBit(theDamageFlags, DamageFlags::DAMAGE_SPIKE)))
                {
                    aDamage = 1800;

                    if (mSeedType == SeedType::SEED_SPIKEROCK)
                    {
                        SpikeRockTakeDamage();
                    }
                    else
                    {
                        Die();
                    }
                }

                aZombie->TakeDamage(aDamage, theDamageFlags);
                mApp->PlayFoley(FoleyType::FOLEY_SPLAT);

                // @pvz-online: 「emotional damage」命中回血：每命中一只回 4% 最大生命
                //（耐砸改版后 1000 血 = 每次 +40），钳满血为止、无每轮封顶。
                if (anEmoHeal)
                {
                    mPlantHealth = std::min(mPlantMaxHealth, mPlantHealth + mPlantMaxHealth / 25);
                }

                // @pvz-online: 单株升级「Barbed Spikes」（地刺，表行 SEED_SPIKEWEED）：扎过的
                // 僵尸减速 +3 秒/层（计数型、至多 2 层）。语义照 Zombie::ApplyChill——CanBeChilled
                // 挡下、不缩短更长的减速（冰道/寒冰菇的 2000 帧不动）、首次挂上出冰音、降速
                // 刷新走 UpdateAnimSpeed。只认地刺本体（地刺王的加成是攻速、不在这）；
                // RunPlantUpgradeCount 非闯关恒 0，加 0 不进这条。
                if (mSeedType == SeedType::SEED_SPIKEWEED)
                {
                    int aSlowFrames = aZombie->SoftControlDuration(300 * mApp->RunPlantUpgradeCount(SeedType::SEED_SPIKEWEED));
                    if (aSlowFrames > 0 && aZombie->CanBeChilled() && aSlowFrames > aZombie->mChilledCounter)
                    {
                        if (aZombie->mChilledCounter == 0)
                        {
                            mApp->PlayFoley(FoleyType::FOLEY_FROZEN);
                        }
                        aZombie->mChilledCounter = aSlowFrames;
                        aZombie->UpdateAnimSpeed();
                    }
                }

                // @pvz-online: 第二 buff「腐臭之息」（大喷菇 #10，docs/06 §8.6）：命中软减速
                // 3 秒（mFetidSlowCounter=300，幅度 20%/层在 ApplyAnimRate 乘）。只认大喷菇
                // 本体（忧郁菇是另一株、词条不共享）；软减速刷到 300 封顶、不缩短已有窗口。
                // @pvz-online: duration resistance also applies to this non-ice slow; hard immunity does not.
                if (mSeedType == SeedType::SEED_FUMESHROOM && mApp->RunPlantBuff2Count(SeedType::SEED_FUMESHROOM) > 0
                    && !aZombie->IsDeadOrDying())
                {
                    int aFetidFrames = aZombie->SoftControlDuration(300);
                    if (aZombie->mFetidSlowCounter < aFetidFrames)
                    {
                        aZombie->mFetidSlowCounter = aFetidFrames;
                        aZombie->UpdateAnimSpeed();
                    }
                }
            }
        }
    }
}

PvzpParticleSystem* Plant::AddAttachedParticle(int thePosX, int thePosY, int theRenderPosition, ParticleEffect theEffect)
{
	PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(mParticleID);
	if (aParticle)
		aParticle->ParticleSystemDie();

	PvzpParticleSystem* aNewParticle = mApp->AddPvzpParticle(thePosX, thePosY, theRenderPosition, theEffect);
	if (aNewParticle)
		mParticleID = mApp->ParticleGetID(aNewParticle);

	return aNewParticle;
}

bool Plant::FindTargetAndFire(int theRow, PlantWeapon thePlantWeapon)
{
    Zombie* aZombie = FindTargetZombie(theRow, thePlantWeapon);
    if (!aZombie && mSeedType == SeedType::SEED_SPLITPEA && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY &&
        mApp->RunExtraBuffCount(RUN_BUFF2_FORWARD_BACK) > 0)
        aZombie = FindTargetZombie(theRow, PlantWeapon::WEAPON_PRIMARY);
    // @pvz-online: 忧郁菇（用户 2026-10-09 令）：无论有没有敌人都一直攻击——光环不设目标门，
    // 空场也照常走 anim_shooting 四连喷（UpdateShooting → Fire → DoRowAreaDamage，
    // 没怪时白喷、无副作用）。
    // @pvz-online: 仙人掌「Tri-Spike」（2026-10-10 用户令「和三线射手相似的行为，包括索敌」）：
    // 调用方已按三行 OR 过门（见 UpdateShooter 仙人掌分支），这里放行空放——本行无怪也照放
    // 主弹+侧弹，同三线「任一行有敌 → 三行都放」口径。
    bool aTriSpikeAlways = mSeedType == SeedType::SEED_CACTUS && mApp->RunPlantBuff2Count(SeedType::SEED_CACTUS) > 0;
    if (aZombie == nullptr && mSeedType != SeedType::SEED_GLOOMSHROOM && !aTriSpikeAlways)
        return false;

    float aSpeed = PlantShootSpeed(mApp, mSeedType, this);

    EndBlink();
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mHeadReanimID);

    if (mSeedType == SeedType::SEED_SPLITPEA && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
    {
        Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);
        aHeadReanim2->StartBlend(20);
        aHeadReanim2->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
        aHeadReanim2->mAnimRate = 35.0f * aSpeed;
        aHeadReanim2->SetFramesForLayer("anim_splitpea_shooting");
        mShootingCounter = PlantShootTicks(26, aSpeed);
    }
    else if (aHeadReanim && aHeadReanim->TrackExists("anim_shooting"))
    {
        aHeadReanim->StartBlend(20);
        aHeadReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
        aHeadReanim->mAnimRate = 35.0f * aSpeed;
        aHeadReanim->SetFramesForLayer("anim_shooting");

        mShootingCounter = PlantShootTicks(33, aSpeed);
        if (mSeedType == SeedType::SEED_REPEATER || mSeedType == SeedType::SEED_SPLITPEA || mSeedType == SeedType::SEED_LEFTPEATER)
        {
            aHeadReanim->mAnimRate = 45.0f * aSpeed;
            mShootingCounter = PlantShootTicks(26, aSpeed);
        }
        else if (mSeedType == SeedType::SEED_GATLINGPEA)
        {
            aHeadReanim->mAnimRate = 38.0f * aSpeed;
            mShootingCounter = PlantShootTicks(100, aSpeed);
        }
    }
    else if (mState == PlantState::STATE_CACTUS_HIGH)
    {
        PlayBodyReanim("anim_shootinghigh", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 35.0f * aSpeed);
        mShootingCounter = PlantShootTicks(23, aSpeed);
    }
    else if (mSeedType == SeedType::SEED_GLOOMSHROOM)
    {
        PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 14.0f * aSpeed);
        mShootingCounter = PlantShootTicks(200, aSpeed);
    }
    else if (mSeedType == SeedType::SEED_CATTAIL)
    {
        PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 30.0f * aSpeed);
        mShootingCounter = PlantShootTicks(50, aSpeed);
    }
    else if (aBodyReanim && aBodyReanim->TrackExists("anim_shooting"))
    {
        PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 35.0f * aSpeed);

        switch (mSeedType)
        {
        case SeedType::SEED_FUMESHROOM:     mShootingCounter = PlantShootTicks(50, aSpeed);  break;
        case SeedType::SEED_PUFFSHROOM:     mShootingCounter = PlantShootTicks(29, aSpeed);  break;
        case SeedType::SEED_SCAREDYSHROOM:  mShootingCounter = PlantShootTicks(25, aSpeed);  break;
        case SeedType::SEED_CABBAGEPULT:    mShootingCounter = PlantShootTicks(32, aSpeed);  break;
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_WINTERMELON:    mShootingCounter = PlantShootTicks(36, aSpeed);  break;
        case SeedType::SEED_KERNELPULT:
        {
            // @pvz-online: 批 18 玉米投手行改为「Artillery 种下变加农炮」——黄油率恢复原版
            // 固定 1/4（原 Buttery 挂点删除）。
            // @pvz-online: 第二 buff「Butter Feast」黄油盛宴（玉米投手 #34，docs/06 §8.6，2★
            // cap2）：持条时黄油率 25% + 25%×层（cap2 封顶 75%）、黄油时长 400 + 100×层 帧
            //（+1 秒/层，见 Projectile 黄油命中处）；未持条保留原版 Rand(4) 抽法，RNG 流
            // 与改前完全一致。
            int aButterFeastStacks = mApp->RunPlantBuff2Count(SeedType::SEED_KERNELPULT);
            bool aMakeButter = aButterFeastStacks > 0 ? (Sexy::Rand(100) < 25 + 25 * aButterFeastStacks) : (Sexy::Rand(4) == 0);
            if (aMakeButter)
            {
                aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
                aBodyReanim->AssignRenderGroupToPrefix("Cornpult_butter", RENDER_GROUP_NORMAL);
                aBodyReanim->AssignRenderGroupToPrefix("Cornpult_kernal", RENDER_GROUP_HIDDEN);
                mState = PlantState::STATE_KERNELPULT_BUTTER;
            }

            mShootingCounter = PlantShootTicks(30, aSpeed);
            break;
        }
        case SeedType::SEED_CACTUS:         mShootingCounter = PlantShootTicks(35, aSpeed);  break;
        default:                            mShootingCounter = PlantShootTicks(29, aSpeed);  break;
        }
    }
    else
        Fire(aZombie, theRow, thePlantWeapon);

    return true;
}

void Plant::LaunchThreepeater()
{
    int rowAbove = mRow - 1;
    int rowBelow = mRow + 1;

    if ((FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY)) ||
        (mBoard->RowCanHaveZombies(rowAbove) && FindTargetZombie(rowAbove, PlantWeapon::WEAPON_PRIMARY)) ||
        (mBoard->RowCanHaveZombies(rowBelow) && FindTargetZombie(rowBelow, PlantWeapon::WEAPON_PRIMARY)))
    {
        float aSpeed = PlantShootSpeed(mApp, mSeedType, this);
        Reanimation* aHeadReanim1 = mApp->ReanimationGet(mHeadReanimID);
        Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);
        Reanimation* aHeadReanim3 = mApp->ReanimationGet(mHeadReanimID3);

        if (mBoard->RowCanHaveZombies(rowBelow))
        {
            aHeadReanim1->StartBlend(10);
            aHeadReanim1->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
            aHeadReanim1->mAnimRate = 20.0f * aSpeed;
            aHeadReanim1->SetFramesForLayer("anim_shooting1");
        }

        aHeadReanim2->StartBlend(10);
        aHeadReanim2->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
        aHeadReanim2->mAnimRate = 20.0f * aSpeed;
        aHeadReanim2->SetFramesForLayer("anim_shooting2");

        if (mBoard->RowCanHaveZombies(rowAbove))
        {
            aHeadReanim3->StartBlend(10);
            aHeadReanim3->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
            aHeadReanim3->mAnimRate = 20.0f * aSpeed;
            aHeadReanim3->SetFramesForLayer("anim_shooting3");
        }

        mShootingCounter = PlantShootTicks(35, aSpeed);
    }
}

bool Plant::FindStarFruitTarget()
{
	if (mRecentlyEatenCountdown > 0)
		return true;

	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
	int aCenterStarX = mX + 40;
	int aCenterStarY = mY + 40;

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		Rect aZombieRect = aZombie->GetZombieRect();
		if (aZombie->EffectedByDamage(aDamageRangeFlags))
		{
			if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS && mPlantCol >= 5)
				return true;

			if (aZombie->mRow == mRow)
			{
				if (aZombieRect.mX + aZombieRect.mWidth < aCenterStarX)
					return true;
			}
			else
			{
				if (aZombie->mZombieType == ZombieType::ZOMBIE_DIGGER)
					aZombieRect.mWidth += 10;

				float aProjectileTime = Distance2D(aCenterStarX, aCenterStarY, aZombieRect.mX + aZombieRect.mWidth / 2, aZombieRect.mY + aZombieRect.mHeight / 2) / 3.33f;
				int aZombieHitX = aZombie->ZombieTargetLeadX(aProjectileTime) - aZombieRect.mWidth / 2;
				if ((aZombieHitX + aZombieRect.mWidth > aCenterStarX) && (aZombieHitX < aCenterStarX))
					return true;

				int aCenterZombieX = aZombieHitX + aZombieRect.mWidth / 2;
				int aCenterZombieY = aZombieRect.mY + aZombieRect.mHeight / 2;
				float angle = RAD_TO_DEG(atan2(aCenterZombieY - aCenterStarY, aCenterZombieX - aCenterStarX));
				if (abs(aZombie->mRow - mRow) < 2)
				{
					if ((angle > 20.0f && angle < 40.0f) || (angle < -25.0f && angle > -45.0f))
						return true;
				}
				else
				{
					if ((angle > 25.0f && angle < 35.0f) || (angle < -28.0f && angle > -38.0f))
						return true;
				}
			}
		}
	}

	return false;
}

void Plant::LaunchStarFruit()
{
    // @pvz-online: 用户 2026-10-09 令：无论有没有敌人都一直攻击——删 FindStarFruitTarget 门，
    // 空场也照常转 anim_shoot 并出五向星弹（StarFruitFire 本就无条件五发，没怪时飞出屏即可）。
    float aSpeed = PlantShootSpeed(mApp, mSeedType, this);
    PlayBodyReanim("anim_shoot", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 28.0f * aSpeed);
    mShootingCounter = PlantShootTicks(40, aSpeed);
}

void Plant::StarFruitFire()
{
    mApp->PlayFoley(FoleyType::FOLEY_THROW);

    // @pvz-online: 单株升级「Homing Stars」（杨桃，表行 SEED_STARFRUIT）：子弹变追踪弹
    //（1 层成型、至多 1 层）。星弹照旧是 PROJECTILE_STAR（贴图/伤害不变）、五向散开的
    // 初速也不动，出膛后走香蒲刺同款的 MOTION_HOMING——转向与命中（只碰锁定目标）都在
    // Projectile 的 homing 分支，这里只管发射时锁定目标：距离近者优先、五颗逐颗排除
    // 已锁定、尽量各打各的；场上不够五只时余星全追最前的那只（追远的不如集火）。目标
    // 中途死掉 → 星弹保持当前速度直飞（同香蒲刺）；一颗都锁不上（目标刚死光）就保持
    // 直线星原样飞完。
    int aHomingStacks = mApp->RunPlantUpgradeCount(SeedType::SEED_STARFRUIT);
    int aHomingFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
    Zombie* aHomingTargets[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };

    float aShootAngleX = cos(DEG_TO_RAD(30.0f)) * 3.33f;
    float aShootAngleY = sin(DEG_TO_RAD(30.0f)) * 3.33f;
    for (int i = 0; i < 5; i++)
    {
        Projectile* aProjectile = mBoard->AddProjectile(mX + 25, mY + 25, mRenderOrder - 1, mRow, ProjectileType::PROJECTILE_STAR);
        if (aProjectile == nullptr) return;
        aProjectile->mDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
        aProjectile->mMotionType = ProjectileMotion::MOTION_STAR;

        switch (i)
        {
        case 0:     aProjectile->mVelX = -3.33f;         aProjectile->mVelY = 0.0f;             break;
        case 1:     aProjectile->mVelX = 0.0f;          aProjectile->mVelY = 3.33f;             break;
        case 2:     aProjectile->mVelX = 0.0f;          aProjectile->mVelY = -3.33f;            break;
        case 3:     aProjectile->mVelX = aShootAngleX;  aProjectile->mVelY = aShootAngleY;      break;
        case 4:     aProjectile->mVelX = aShootAngleX;  aProjectile->mVelY = -aShootAngleY;     break;
        default:    PVZP_ASSERT();                                                               break;
        }

        if (aHomingStacks >= 1)
        {
            Zombie* aTarget = FindHomingStarTarget(mBoard, mX + 40.0f, mY + 40.0f, aHomingFlags, aHomingTargets, i);
            if (aTarget == nullptr && i > 0)
            {
                aTarget = FindHomingStarTarget(mBoard, mX + 40.0f, mY + 40.0f, aHomingFlags, nullptr, 0);
            }
            aHomingTargets[i] = aTarget;
            if (aTarget)
            {
                aProjectile->mMotionType = ProjectileMotion::MOTION_HOMING;
                aProjectile->mTargetZombieID = mBoard->ZombieGetID(aTarget);
            }
        }
    }
}

void Plant::UpdateShooter()
{
    // @pvz-online: continuous fumes owns its fixed clock; normal launch/haste must not add shots.
    if (mSeedType == SeedType::SEED_FUMESHROOM && mApp->RunPlantUpgradeCount(mSeedType) > 0)
        return;

    float aSpeed = PlantShootSpeed(mApp, mSeedType, this);

    mLaunchCounter--;
    if (mLaunchCounter <= 0)
    {
        // @pvz-online: 闯关攻速族（全局「急袭」×0.8/层 + 单株射速族 ×0.75/层）现为「动画加速」
        // 实现（用户 2026-10-04 定案，见 PlantShootSpeed）：本行重置值与下面两处 ==25/==50
        // 触发点都按同一速度因子缩放。旧实现只缩这个计数器，叠层高时下一发触发会打断进行中的
        // 多段攻击动画（忧郁菇 4 连/香蒲双发/加特林 4 连/双发第二发被卡掉）；现在动画速率与
        // 全部检查点同比例提速，命中帧与多发间隔比例不变。Kind 闸门保留（2026-10-03 修正批：
        // 只有射速族条目进得来——此前任意行都乘，寒冰射手 +30% 行会把雪豆射速拉长）。
        mLaunchCounter = (int)((mLaunchRate - Sexy::Rand(15)) * mApp->RunBuffMul(RUN_BUFF_SWIFT) * mApp->RunPlantUpgradeMulKind(mSeedType, RUN_UPGRADE_KIND_RHYTHM) * mApp->RunPlantBuff2MulKind(mSeedType, RUN_UPGRADE_KIND_RHYTHM) * (PlantInLanternLight(mApp, this) ? mApp->RunPlantBuff2Mul(SeedType::SEED_PLANTERN) : 1.0f) + 0.5f);	// 第二 buff 批 2：同因子链（火力压制）；批 8 追加「暮光」灯照因子（同因子链，见 PlantShootSpeed）

        if (mSeedType == SeedType::SEED_THREEPEATER)
        {
            LaunchThreepeater();
        }
        else if (mSeedType == SeedType::SEED_STARFRUIT)
        {
            LaunchStarFruit();
        }
        else if (mSeedType == SeedType::SEED_SPLITPEA)
        {
            FindTargetAndFire(mRow, PlantWeapon::WEAPON_SECONDARY);
        }
        else if (mSeedType == SeedType::SEED_CACTUS)
        {
            // @pvz-online: 「Tri-Spike」持条（2026-10-10 用户令「和三线射手相似的行为，包括
            // 索敌」）：索敌改三行 OR——本行/上行/下行任一有敌就开火（照 LaunchThreepeater
            // 的三行判定，RowCanHaveZombies 闸门同款），本行无怪时由 FindTargetAndFire 的
            // 持条豁免空放（Fire 内侧弹照补）；未持条维持只查本行。
            if (mApp->RunPlantBuff2Count(mSeedType) > 0)
            {
                if (mState == PlantState::STATE_CACTUS_HIGH || mState == PlantState::STATE_CACTUS_LOW)
                {
                    PlantWeapon aWeapon = (mState == PlantState::STATE_CACTUS_HIGH) ? PlantWeapon::WEAPON_PRIMARY : PlantWeapon::WEAPON_SECONDARY;
                    int rowAbove = mRow - 1;
                    int rowBelow = mRow + 1;
                    if ((FindTargetZombie(mRow, aWeapon)) ||
                        (mBoard->RowCanHaveZombies(rowAbove) && FindTargetZombie(rowAbove, aWeapon)) ||
                        (mBoard->RowCanHaveZombies(rowBelow) && FindTargetZombie(rowBelow, aWeapon)))
                    {
                        FindTargetAndFire(mRow, aWeapon);
                    }
                }
            }
            else if (mState == PlantState::STATE_CACTUS_HIGH)
            {
                FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
            }
            else if (mState == PlantState::STATE_CACTUS_LOW)
            {
                FindTargetAndFire(mRow, PlantWeapon::WEAPON_SECONDARY);
            }
        }
        else
        {
            FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
        }
    }

    if (mLaunchCounter == PlantShootTicks(50, aSpeed) && mSeedType == SeedType::SEED_CATTAIL)
    {
        FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
    }
    if (mLaunchCounter == PlantShootTicks(25, aSpeed))
    {
        if (mSeedType == SeedType::SEED_REPEATER || mSeedType == SeedType::SEED_LEFTPEATER)
        {
            FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
        }
        else if (mSeedType == SeedType::SEED_SPLITPEA)
        {
            FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
            FindTargetAndFire(mRow, PlantWeapon::WEAPON_SECONDARY);
        }
    }
}

bool Plant::MakesSun()
{
	return mSeedType == SeedType::SEED_SUNFLOWER || mSeedType == SeedType::SEED_TWINSUNFLOWER || mSeedType == SeedType::SEED_SUNSHROOM;
}

void Plant::UpdateProductionPlant()
{
    if (!IsInPlay() || mApp->IsIZombieLevel() || mApp->mGameMode == GameMode::GAMEMODE_UPSELL || mApp->mGameMode == GameMode::GAMEMODE_INTRO)
        return;

    // @pvz-online: 联机等待窗里照常生产（清空 ≠ 结束，见 Board::OnlineWaitingForTeam）
    if (mBoard->HasLevelAwardDropped() && !mBoard->OnlineWaitingForTeam())
        return;

    if (mSeedType == SeedType::SEED_MARIGOLD && mBoard->mCurrentWave == mBoard->mNumWaves)
    {
        if (mState != PlantState::STATE_MARIGOLD_ENDING)
        {
            mState = PlantState::STATE_MARIGOLD_ENDING;
            mStateCountdown = 6000;
        }
        else if (mStateCountdown <= 0)
            return;
    }

    if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND && mBoard->mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_LAST_STAND_ONSLAUGHT)
        return;

    mLaunchCounter--;
    if (mLaunchCounter <= 100)
    {
        int aFlashCountdown = PvzpAnimateCurve(100, 0, mLaunchCounter, 0, 100, PvzpCurves::CURVE_LINEAR);
        mEatenFlashCountdown = std::max(mEatenFlashCountdown, aFlashCountdown);
    }
    if (mLaunchCounter <= 0)
    {
        // @pvz-online: 经济批 2026-10-09：阳光菇「亮顶」产阳光间隔 ×0.75/层叠乘也在周期重置这行
        // 乘（另一处在 PlantInitialize 初始等待；小形态也走本函数，口径一致）。
        float aSunIntervalMul = mApp->RunBuffMul(RUN_BUFF_ABUNDANCE);
        if (mSeedType == SeedType::SEED_SUNSHROOM)
            aSunIntervalMul *= mApp->RunPlantUpgradeMul(SeedType::SEED_SUNSHROOM);
        mLaunchCounter = (int)(RandRangeInt(mLaunchRate - 150, mLaunchRate) * aSunIntervalMul + 0.5f);
        mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);

        if (mSeedType == SeedType::SEED_SUNSHROOM)
        {
            // @pvz-online: 经济批 2026-10-09（用户定案）：丰收重做——概率多产撤掉，改速度轴
            // 「产阳光间隔 ×0.75/层 叠乘」（乘算见本函数开头重置行与 PlantInitialize 初始等待）；
            // 这里只落常规那枚（小阳光阶段落的还是小阳光）。
            CoinType aSunType = (mState == PlantState::STATE_SUNSHROOM_SMALL) ? CoinType::COIN_SMALLSUN : CoinType::COIN_SUN;
            mBoard->AddCoin(mX, mY, aSunType, CoinMotion::COIN_MOTION_FROM_PLANT);
            // @pvz-online: 第二 buff「催长」（阳光菇 #9，docs/06 §8.6）：首产（小阳光 15）落地
            // 后立即长大——第二次产出已是 25 大阳光。清掉长大倒计时，下一帧 UpdateSunShroom
            // 的 SMALL 分支判 0 走长大流程；非闯关词条恒 0 不动。
            if (mState == PlantState::STATE_SUNSHROOM_SMALL && mApp->RunPlantBuff2Count(SeedType::SEED_SUNSHROOM) > 0)
            {
                mStateCountdown = 0;
            }
        }
        else if (mSeedType == SeedType::SEED_SUNFLOWER)
        {
            mBoard->AddCoin(mX, mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
            // @pvz-online: 经济批 2026-10-09（用户定案）：丰收重做——每轮 50% 概率多产 1 阳光
            // （只可选 1 层、每轮掷一次；「种下立即产 1 次」在 PlantInitialize 压首轮计时）。
            if (mApp->RunPlantUpgradeCount(SeedType::SEED_SUNFLOWER) > 0 && (int)Sexy::Rand(100) < 50)
                mBoard->AddCoin(mX, mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
        }
        else if (mSeedType == SeedType::SEED_TWINSUNFLOWER)
        {
            mBoard->AddCoin(mX, mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
            mBoard->AddCoin(mX, mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
            // @pvz-online: 经济批 2026-10-09（用户定案）：Twin Bloom 改加量轴——每轮固定多产
            // 2 阳光/层（至多 2 层），无条件落币（原「每轮掷一次 25%×层」概率口径撤掉）。
            for (int i = 0, aExtra = 2 * mApp->RunPlantUpgradeCount(SeedType::SEED_TWINSUNFLOWER); i < aExtra; i++)
                mBoard->AddCoin(mX, mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
        }
        else if (mSeedType == SeedType::SEED_MARIGOLD)
        {
            // @pvz-online: 第二 buff「富贵」（金盏花 #38，docs/06 §8.6）：+10%/层 概率产
            // 高值币——命中时 25% 钻石 / 75% 金（贴词条「金/钻石币」），未命中走原版
            // 10% 金 / 90% 银。常规枚与「多产」老条的加枚各自独立 roll。
            int aRichChance = 10 * mApp->RunPlantBuff2Count(SeedType::SEED_MARIGOLD);
            CoinType aMarigoldCoin;
            if (aRichChance > 0 && (int)Sexy::Rand(100) < aRichChance)
            {
                aMarigoldCoin = ((int)Sexy::Rand(4) == 0) ? CoinType::COIN_DIAMOND : CoinType::COIN_GOLD;
            }
            else
            {
                aMarigoldCoin = ((int)Sexy::Rand(100) < 10) ? CoinType::COIN_GOLD : CoinType::COIN_SILVER;
            }
            mBoard->AddCoin(mX, mY, aMarigoldCoin, CoinMotion::COIN_MOTION_COIN);
            // @pvz-online: 单株升级「多产」：每层多落一枚（金/银按各自 10% 独立摇）
            for (int i = 0, aExtra = mApp->RunPlantUpgradeCount(SeedType::SEED_MARIGOLD); i < aExtra; i++)
            {
                CoinType aExtraCoin;
                if (aRichChance > 0 && (int)Sexy::Rand(100) < aRichChance)
                {
                    aExtraCoin = ((int)Sexy::Rand(4) == 0) ? CoinType::COIN_DIAMOND : CoinType::COIN_GOLD;
                }
                else
                {
                    aExtraCoin = ((int)Sexy::Rand(100) < 10) ? CoinType::COIN_GOLD : CoinType::COIN_SILVER;
                }
                mBoard->AddCoin(mX, mY, aExtraCoin, CoinMotion::COIN_MOTION_COIN);
            }
        }

        if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BIG_TIME)
        {
            if (mSeedType == SeedType::SEED_SUNFLOWER)
            {
                mBoard->AddCoin(mX, mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
            }
            else if (mSeedType == SeedType::SEED_MARIGOLD)
            {
                mBoard->AddCoin(mX, mY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
            }
        }
    }
}

void Plant::UpdateSunShroom()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mState == PlantState::STATE_SUNSHROOM_SMALL)
	{
		if (mStateCountdown == 0)
		{
			PlayBodyReanim("anim_grow", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 12.0f);
			mState = PlantState::STATE_SUNSHROOM_GROWING;
			mApp->PlayFoley(FoleyType::FOLEY_PLANTGROW);
		}

		UpdateProductionPlant();
	}
	else if (mState == PlantState::STATE_SUNSHROOM_GROWING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			PlayBodyReanim("anim_bigidle", ReanimLoopType::REANIM_LOOP, 10, RandRangeFloat(12.0f, 15.0f));
			mState = PlantState::STATE_SUNSHROOM_BIG;
		}
	}
	else
	{
		UpdateProductionPlant();
	}
}

void Plant::UpdateGraveBuster()
{
    if (mState == PlantState::STATE_GRAVEBUSTER_LANDING)
    {
        if (mApp->ReanimationGet(mBodyReanimID)->mLoopCount > 0)
        {
            PlayBodyReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 10, 12.0f);
            // @pvz-online: 第二 buff 批 2「疾掘」（墓碑吞噬者，docs/06 §8.6 #11）：吞噬速度
            // ×0.5/层（1★）——400 帧吞碑计时按乘数缩，2 层 ×0.25。
            mStateCountdown = (int)(400 * mApp->RunPlantBuff2Mul(SeedType::SEED_GRAVEBUSTER) + 0.5f);
            mState = PlantState::STATE_GRAVEBUSTER_EATING;
            AddAttachedParticle(mX + 40, mY + 40, mRenderOrder + 4, ParticleEffect::PARTICLE_GRAVE_BUSTER);
        }
    }
    else if (mState == PlantState::STATE_GRAVEBUSTER_EATING && mStateCountdown == 0)
    {
        GridItem* aGraveStone = mBoard->GetGraveStoneAt(mPlantCol, mRow);
        if (aGraveStone)
        {
            aGraveStone->GridItemDie();
            mBoard->mGravesCleared++;
            // @pvz-online: 单株升级「Quick Dig」（墓碑吞噬者，表行 SEED_GRAVEBUSTER）：吞掉
            // 墓碑成功额外落 25 阳光/层（计数型、至多 2 层）——真吞到才给，落币法同咖啡豆。
            // 经济批 2026-10-09：本行已删条不进抽取池（用户令删），挂点保留供存量档生效。
            int aSunCoins = mApp->RunPlantUpgradeCount(SeedType::SEED_GRAVEBUSTER);
            for (int i = 0; i < aSunCoins; i++)
            {
                mBoard->AddCoin(mX, mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
            }
        }

        mApp->AddPvzpParticle(mX + 40, mY + 40, mRenderOrder + 4, ParticleEffect::PARTICLE_GRAVE_BUSTER_DIE);
        Die();
        mBoard->DropLootPiece(mX + 40, mY, 12);
    }
}

void Plant::PlayBodyReanim(const char* theTrackName, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate)
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

	if (theBlendTime > 0)
		aBodyReanim->StartBlend(theBlendTime);
	if (theAnimRate > 0.0f)
		aBodyReanim->mAnimRate = theAnimRate;

	aBodyReanim->mLoopType = theLoopType;
	aBodyReanim->mLoopCount = 0;
	aBodyReanim->SetFramesForLayer(theTrackName);
}

void Plant::UpdatePotato()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

	if (mState == PlantState::STATE_NOTREADY)
	{
		if (mStateCountdown == 0)
		{
			mApp->AddPvzpParticle(mX + mWidth / 2, mY + mHeight / 2, mRenderOrder, ParticleEffect::PARTICLE_POTATO_MINE_RISE);
			PlayBodyReanim("anim_rise", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 18.0f);
			mState = PlantState::STATE_POTATO_RISING;
			mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
		}
	}
	else if (mState == PlantState::STATE_POTATO_RISING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			float aRate = RandRangeFloat(12.0f, 15.0f);
			PlayBodyReanim("anim_armed", ReanimLoopType::REANIM_LOOP, 0, aRate);

			auto& holder = mApp->mEffectSystem->mReanimationHolder;
            Reanimation* aLightReanim = holder->mReanimations.DataArrayCanAllocate(holder->mPlantReservation + 1) ?
                mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, GetPlantDefinition(mSeedType).mReanimationType) : nullptr;
			if (aLightReanim)
			{
				aLightReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
				aLightReanim->mAnimRate = aRate - 2.0f;
				aLightReanim->SetFramesForLayer("anim_glow");
				aLightReanim->mFrameCount = 10;
				aLightReanim->ShowOnlyTrack("anim_glow");
				aLightReanim->SetTruncateDisappearingFrames("anim_glow", false);
				mLightReanimID = mApp->ReanimationGetID(aLightReanim);
				aLightReanim->AttachToAnotherReanimation(aBodyReanim, "anim_light");
			}

			mState = PlantState::STATE_POTATO_ARMED;
			mBlinkCountdown = 400 + Sexy::Rand(4000);
		}
	}
	else if (mState == PlantState::STATE_POTATO_ARMED)
	{
		if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
		{
			DoSpecial();
		}
		else
		{
			Reanimation* aLightReanim = mApp->ReanimationTryToGet(mLightReanimID);
			if (aLightReanim)
			{
				aLightReanim->mFrameCount = PvzpAnimateCurve(200, 50, DistanceToClosestZombie(), 10, 3, PvzpCurves::CURVE_LINEAR);
			}
		}
	}
}

void Plant::UpdateTanglekelp()
{
    if (mState != PlantState::STATE_TANGLEKELP_GRABBING)
    {
        Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
        if (aZombie)
        {
            mApp->PlayFoley(FoleyType::FOLEY_FLOOP);
            mState = PlantState::STATE_TANGLEKELP_GRABBING;
            mStateCountdown = 100;
            AttachTanglekelpGrab(aZombie);
            mTargetZombieID = mBoard->ZombieGetID(aZombie);

            // @pvz-online: 单株升级「Entangle」（缠绕海草，批 13）：每层多缠 1 只、至多 2 层
            //（1→3 只）。逐只再调 FindTargetZombie——对海草它会跳过已被任何海草抓着的僵尸
            //（IsTangleKelpTarget 走 IsTangleKelpTargeting，含刚记下的主目标），不会重抓同一只。
            int aExtraCount = mApp->RunPlantUpgradeCount(SeedType::SEED_TANGLEKELP);
            if (aExtraCount > 2)
            {
                aExtraCount = 2;
            }
            for (int i = 0; i < aExtraCount; i++)
            {
                Zombie* aExtraZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
                if (!aExtraZombie)
                {
                    break;
                }
                AttachTanglekelpGrab(aExtraZombie);
                mExtraTanglekelpIDs[mExtraTanglekelpCount] = mBoard->ZombieGetID(aExtraZombie);
                mExtraTanglekelpCount++;
            }
        }
    }
    else
    {
        if (mStateCountdown == 50)
        {
            Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
            if (aZombie)
            {
                aZombie->DragUnder();
                aZombie->PoolSplash(false);
            }
            // @pvz-online: 多缠的额外目标与主目标同时点沉底（批 13）。
            for (int i = 0; i < mExtraTanglekelpCount; i++)
            {
                Zombie* aExtraZombie = mBoard->ZombieTryToGet(mExtraTanglekelpIDs[i]);
                if (aExtraZombie)
                {
                    aExtraZombie->DragUnder();
                    aExtraZombie->PoolSplash(false);
                }
            }
        }

        if (mStateCountdown == 20)
        {
            int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
            Reanimation* aSplashReanim = mApp->AddReanimation(mX - 23, mY + 7, aRenderPosition, ReanimationType::REANIM_SPLASH);
            if (aSplashReanim) aSplashReanim->OverrideScale(1.3f, 1.3f);

            mApp->AddPvzpParticle(mX + 31, mY + 64, aRenderPosition, ParticleEffect::PARTICLE_PLANTING_POOL);
            mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_ENTERING_WATER);
        }

        if (mStateCountdown == 0)
        {
            Die();

            Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
            if (aZombie)
            {
                aZombie->DieWithLoot();
            }
            // @pvz-online: 多缠的额外目标与主目标同时点清场（批 13）。
            for (int i = 0; i < mExtraTanglekelpCount; i++)
            {
                Zombie* aExtraZombie = mBoard->ZombieTryToGet(mExtraTanglekelpIDs[i]);
                if (aExtraZombie)
                {
                    aExtraZombie->DieWithLoot();
                }
            }
        }
    }
}

void Plant::SpikeweedAttack()
{
    PVZP_ASSERT(IsSpiky());

    if (mState != PlantState::STATE_SPIKEWEED_ATTACKING)
    {
        PlayBodyReanim("anim_attack", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 18.0f);
        mApp->PlaySample(SOUND_THROW);

        mState = PlantState::STATE_SPIKEWEED_ATTACKING;
        mStateCountdown = SpikeweedCycleFrames(mApp, mSeedType);
    }
}

void Plant::UpdateSpikeweed()
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (mState == PlantState::STATE_SPIKEWEED_ATTACKING)
    {
        // @pvz-online: 命中点按循环帧数同比例缩（地刺王的攻速加成缩短了循环）；原始比例 =
        // 倒计时从 100 走：地刺 75、地刺王 69/33。
        int aCycle = SpikeweedCycleFrames(mApp, mSeedType);
        if (mStateCountdown == 0)
        {
            mState = PlantState::STATE_NOTREADY;
        }
        else if (mSeedType == SeedType::SEED_SPIKEROCK)
        {
            if (mStateCountdown == aCycle * 69 / 100 || mStateCountdown == aCycle * 33 / 100)
            {
                DoRowAreaDamage(20, 33U);
            }
        }
        else if (mStateCountdown == aCycle * 75 / 100)
        {
            DoRowAreaDamage(20, 33U);
        }

        if (aBodyReanim->mLoopCount > 0)
        {
            PlayIdleAnim(RandRangeFloat(12.0f, 15.0f));
        }
    }
    else if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
    {
        SpikeweedAttack();
    }
}

void Plant::UpdateScaredyShroom()
{
    if (mShootingCounter > 0)
        return;

    bool aHasZombieNearby = false;

    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        Rect aZombieRect = aZombie->GetZombieRect();
        int aDiffY = (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS) ? 0 : (aZombie->mRow - mRow);
        if (!aZombie->mMindControlled && !aZombie->IsDeadOrDying() && aDiffY <= 1 && aDiffY >= -1 && GetCircleRectOverlap(mX, mY + 20.0f, 120, aZombieRect))
        {
            aHasZombieNearby = true;
            break;
        }
    }

    // @pvz-online: 单株升级「Bravery」（胆小菇，批 14）：抽到后不再缩头——贴近判定直接
    // 作废，恒 READY 照常射击（1 层成型；非闯关恒 0，加 0 保持原版缩头）。
    if (mApp->RunPlantUpgradeCount(SeedType::SEED_SCAREDYSHROOM) > 0)
    {
        aHasZombieNearby = false;
    }

    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (mState == PlantState::STATE_READY)
    {
        if (aHasZombieNearby)
        {
            mState = PlantState::STATE_SCAREDYSHROOM_LOWERING;
            PlayBodyReanim("anim_scared", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 10.0f);
        }
    }
    else if (mState == PlantState::STATE_SCAREDYSHROOM_LOWERING)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            mState = PlantState::STATE_SCAREDYSHROOM_SCARED;
            PlayBodyReanim("anim_scaredidle", ReanimLoopType::REANIM_LOOP, 10, 0.0f);
        }
    }
    else if (mState == PlantState::STATE_SCAREDYSHROOM_SCARED)
    {
        if (!aHasZombieNearby)
        {
            mState = PlantState::STATE_SCAREDYSHROOM_RAISING;

            float aAnimRate = RandRangeFloat(7.0f, 12.0f);
            PlayBodyReanim("anim_grow", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, aAnimRate);
        }
    }
    else if (mState == PlantState::STATE_SCAREDYSHROOM_RAISING)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            mState = PlantState::STATE_READY;

            float aAnimRate = RandRangeFloat(10.0f, 15.0f);
            PlayIdleAnim(aAnimRate);
        }
    }

    if (mState != PlantState::STATE_READY)
    {
        mLaunchCounter = mLaunchRate;
    }
}

void Plant::UpdateTorchwood()
{
	Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

	for (Projectile* aProjectile : mBoard->mProjectiles)
	{
		if (aProjectile->mDead)
			continue;
		if ((aProjectile->mRow == mRow) &&
			(aProjectile->mProjectileType == ProjectileType::PROJECTILE_PEA || aProjectile->mProjectileType == ProjectileType::PROJECTILE_SNOWPEA))
		{
			Rect aProjectileRect = aProjectile->GetProjectileRect();
			if (GetRectOverlap(aAttackRect, aProjectileRect) >= 10)
			{
				if (aProjectile->mProjectileType == ProjectileType::PROJECTILE_PEA)
				{
					aProjectile->ConvertToFireball(mPlantCol);
				}
				else if (aProjectile->mProjectileType == ProjectileType::PROJECTILE_SNOWPEA)
				{
					aProjectile->ConvertToPea(mPlantCol);
				}
			}
		}
	}
}

void Plant::DoSquashDamage()
{
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
    Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        if ((aZombie->mRow == mRow || aZombie->mZombieType == ZombieType::ZOMBIE_BOSS) && aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            Rect aZombieRect = aZombie->GetZombieRect();
            if (GetRectOverlap(aAttackRect, aZombieRect) > (aZombie->mZombieType == ZombieType::ZOMBIE_FOOTBALL ? -20 : 0))
            {
                // @pvz-online: 闯关 buff「爆破」：窝瓜压扁伤害 ×(1+60%/层，批八由 30% 上调)。
                aZombie->TakeDamage((int)(1800 * mApp->RunBuffMul(RUN_BUFF_BLAST) + 0.5f), 18U);

                // @pvz-online: 单株升级「重压」的眩晕挂点已于批八（2026-10-05）整体删除——
                // 该行换成「砸击次数 +2/层」，机制移到 UpdateSquash 的多段砸击（落地还有
                // 余额就起身再砸），不再走 ApplyButter。
            }
        }
    }
}

Zombie* Plant::FindSquashTarget()
{
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
    Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

    int aClosestRange = 0;
    Zombie* aClosestZombie = nullptr;

    // @pvz-online: 第二 buff「Leap Shock」飞跃震击（窝瓜 #17，docs/06 §8.6，2★ cap2）：
    // 持条时相邻行（±1）的僵尸也纳入索敌（基线只有本行 + 巨人特例）。
    int aLeapStacks = mApp->RunPlantBuff2Count(SeedType::SEED_SQUASH);

    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        if ((aZombie->mRow == mRow || aZombie->mZombieType == ZombieType::ZOMBIE_BOSS ||
             (aLeapStacks > 0 && (aZombie->mRow - mRow == 1 || aZombie->mRow - mRow == -1))) &&
            aZombie->mHasHead && !aZombie->IsTangleKelpTarget() && aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            Rect aZombieRect = aZombie->GetZombieRect();

            if ((
                    aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT && aZombieRect.mX < mX + 20
                ) || (
                    aZombie->mZombiePhase != ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_POLEVAULTER_IN_VAULT &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_SNORKEL_INTO_POOL &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_INTO_POOL &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_RIDING &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_IN_JUMP &&
                    !aZombie->IsBobsledTeamWithSled()
                ))
            {
                int aRange = -GetRectOverlap(aAttackRect, aZombieRect);
                if (aRange <= (aZombie->mIsEating ? 110 : 70))
                {
                    int aPlantX = aAttackRect.mX;
                    if (aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_POST_VAULT || aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT ||
                        aZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL || aZombie->mZombieType == ZombieType::ZOMBIE_IMP ||
                        aZombie->mZombieType == ZombieType::ZOMBIE_FOOTBALL || mApp->IsScaryPotterLevel())
                    {
                        aPlantX = aAttackRect.mX - 60;
                    }

                    if (aZombie->IsWalkingBackwards() || aZombieRect.mX + aZombieRect.mWidth >= aPlantX)
                    {
                        if (mBoard->ZombieGetID(aZombie) == mTargetZombieID)
                            return aZombie;  // 是锁定的目标僵尸，则直接返回该僵尸

                        if (aClosestZombie == nullptr || aRange < aClosestRange)
                        {
                            aClosestZombie = aZombie;
                            aClosestRange = aRange;
                        }
                    }
                }
            }
        }
    }

    return aClosestZombie;
}

void Plant::UpdateSquash()
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    (void)aBodyReanim; // unused in Release mode
    PVZP_ASSERT(aBodyReanim);

    if (mState == PlantState::STATE_NOTREADY)
    {
        Zombie* aZombie = FindSquashTarget();
        if (aZombie)
        {
            // @pvz-online: 窝瓜多砸（批八）：第一次锁定目标时按层数记余额（每层 +2 次）；
            // 重臂回 NOTREADY 时不再重置——余额用完才消失（未升级恒 0，落地即 Die）。
            // @pvz-online: 第二 buff「Leap Shock」飞跃震击（窝瓜 #17，docs/06 §8.6，2★ cap2）：
            // 总砸击数 = (1+2×老条层) × 2^新层（与老条加算叠乘，只可选 2 层），余额即再 -1；
            // 锁定到相邻行目标时整株改行（后续落点 Y / 溅伤判定 / 水池判定全按 mRow 自动跟随），
            // 巨人特例（跨行锁定）不改行，基线行为原样。
            if (mSquashSmashesLeft < 0)
                mSquashSmashesLeft = (int)((1 + 2 * mApp->RunPlantUpgradeCount(SeedType::SEED_SQUASH)) * mApp->RunPlantBuff2Mul(SeedType::SEED_SQUASH) + 0.5f) - 1;
            if (aZombie->mRow != mRow && aZombie->mZombieType != ZombieType::ZOMBIE_BOSS)
                mRow = aZombie->mRow;
            mTargetZombieID = mBoard->ZombieGetID(aZombie);
            mTargetX = aZombie->ZombieTargetLeadX(0.0f) - mWidth / 2;
            mState = PlantState::STATE_SQUASH_LOOK;
            mStateCountdown = 80;
            PlayBodyReanim(mTargetX < mX ? "anim_lookleft" : "anim_lookright", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 24.0f);
            mApp->PlayFoley(FoleyType::FOLEY_SQUASH_HMM);
        }
    }
    else if (mState == PlantState::STATE_SQUASH_LOOK)
    {
        if (mStateCountdown <= 0)
        {
            PlayBodyReanim("anim_jumpup", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
            mState = PlantState::STATE_SQUASH_PRE_LAUNCH;
            mStateCountdown = 30;
        }
    }
    else if (mState == PlantState::STATE_SQUASH_PRE_LAUNCH)
    {
        if (mStateCountdown <= 0)
        {
            Zombie* aZombie = FindSquashTarget();
            if (aZombie)
            {
                mTargetX = aZombie->ZombieTargetLeadX(30.0f) - mWidth / 2;
                // @pvz-online: 第二 buff「Leap Shock」（窝瓜 #17）：起跳前目标若在相邻行
                //（只有持条才可能选中），跟锁定点一样整株改行——落点/溅伤全随 mRow 走。
                if (aZombie->mRow != mRow && aZombie->mZombieType != ZombieType::ZOMBIE_BOSS)
                    mRow = aZombie->mRow;
            }

            // @pvz-online: 窝瓜多砸（批八）：记下本次起跳点——首跳 = 种植格（此刻 mX/mY
            // 未动过）、后续跳 = 上次落点；RISING 插值起点改用它（原写死种植格）。
            mSquashFromX = mX;
            mSquashFromY = mY;

            mState = PlantState::STATE_SQUASH_RISING;
            mStateCountdown = 50;
            mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
        }
    }
    else
    {
        int aTargetCol = mBoard->PixelToGridXKeepOnBoard(mTargetX, mY);
        int aDestY = mBoard->GridToPixelY(aTargetCol, mRow) + 8;

        if (mState == PlantState::STATE_SQUASH_RISING)
        {
            mX = PvzpAnimateCurve(50, 20, mStateCountdown, mSquashFromX, mTargetX, PvzpCurves::CURVE_EASE_IN_OUT);
            mY = PvzpAnimateCurve(50, 20, mStateCountdown, mSquashFromY, aDestY - 120, PvzpCurves::CURVE_EASE_IN_OUT);

            if (mStateCountdown == 0)
            {
                PlayBodyReanim("anim_jumpdown", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 60.0f);
                mState = PlantState::STATE_SQUASH_FALLING;
                mStateCountdown = 10;
            }
        }
        else if (mState == PlantState::STATE_SQUASH_FALLING)
        {
            mY = PvzpAnimateCurve(10, 0, mStateCountdown, aDestY - 120, aDestY, PvzpCurves::CURVE_EASE_IN_OUT);

            if (mStateCountdown == 5)
            {
                DoSquashDamage();
            }

            if (mStateCountdown == 0)
            {
                if (mBoard->IsPoolSquare(aTargetCol, mRow))
                {
                    mApp->AddReanimation(mX - 11, mY + 20, mRenderOrder + 1, ReanimationType::REANIM_SPLASH);
                    mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
                    mApp->PlaySample(SOUND_ZOMBIESPLASH);

                    Die();
                }
                else
                {
                    mState = PlantState::STATE_SQUASH_DONE_FALLING;
                    mStateCountdown = 100;

                    mBoard->ShakeBoard(1, 4);
                    mApp->PlayFoley(FoleyType::FOLEY_THUMP);
                    float aOffsetY = mBoard->StageHasRoof() ? 69.0f : 80.0f;
                    mApp->AddPvzpParticle(mX + 40, mY + aOffsetY, mRenderOrder + 4, ParticleEffect::PARTICLE_DUST_SQUASH);
                }
            }
        }
        else if (mState == PlantState::STATE_SQUASH_DONE_FALLING)
        {
            if (mStateCountdown == 0)
            {
                // @pvz-online: 窝瓜多砸（批八）：还有余额就起身回 NOTREADY 再找下一只
                // （新索敌按此刻落点算），余额用尽才消失——未升级恒 0，行为与原版一致。
                if (mSquashSmashesLeft > 0)
                {
                    mSquashSmashesLeft--;
                    mState = PlantState::STATE_NOTREADY;
                    mRenderOrder = CalcRenderOrder();
                    PlayBodyReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 30, RandRangeFloat(10.0f, 15.0f));
                }
                else
                {
                    Die();
                }
            }
        }
    }
}

void Plant::UpdateDoomShroom()
{
	if (mIsAsleep || mState == PlantState::STATE_DOINGSPECIAL)
		return;

	mState = PlantState::STATE_DOINGSPECIAL;
	mDoSpecialCountdown = 100;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	PVZP_ASSERT(aBodyReanim);

	aBodyReanim->SetFramesForLayer("anim_explode");
	aBodyReanim->mAnimRate = 23.0f;
	aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
	aBodyReanim->SetShakeOverride("DoomShroom_head1", 1.0f);
	aBodyReanim->SetShakeOverride("DoomShroom_head2", 2.0f);
	aBodyReanim->SetShakeOverride("DoomShroom_head3", 2.0f);
	mApp->PlayFoley(FoleyType::FOLEY_REVERSE_EXPLOSION);
}

void Plant::UpdateIceShroom()
{
	if (!mIsAsleep && mState != PlantState::STATE_DOINGSPECIAL)
	{
		mState = PlantState::STATE_DOINGSPECIAL;
		mDoSpecialCountdown = 100;
	}
}

void Plant::UpdateBlover()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (aBodyReanim->mLoopCount > 0 && aBodyReanim->mLoopType != ReanimLoopType::REANIM_LOOP)
	{
		aBodyReanim->SetFramesForLayer("anim_loop");
		aBodyReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
	}
}

void Plant::UpdateFlowerPot()
{
	if (mState == PlantState::STATE_FLOWERPOT_INVULNERABLE && mStateCountdown == 0)
		mState = PlantState::STATE_NOTREADY;
}

void Plant::UpdateLilypad()
{
	if (mState == PlantState::STATE_LILYPAD_INVULNERABLE && mStateCountdown == 0)
		mState = PlantState::STATE_NOTREADY;
}

void Plant::UpdateCoffeeBean()
{
	if (mState == PlantState::STATE_DOINGSPECIAL)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			Die();
		}
	}
}

void Plant::UpdateUmbrella()
{
	if (mState == PlantState::STATE_UMBRELLA_TRIGGERED)
	{
		if (mStateCountdown == 0)
		{
			mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, mRow + 1, 0);
			mState = PlantState::STATE_UMBRELLA_REFLECTING;
		}
	}
	else if (mState == PlantState::STATE_UMBRELLA_REFLECTING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			PlayIdleAnim(0.0f);
			mState = PlantState::STATE_NOTREADY;
			mRenderOrder = CalcRenderOrder();
		}
	}
}

void Plant::UpdateCobCannon()
{
	if (mState == PlantState::STATE_COBCANNON_ARMING)
	{
		if (mStateCountdown == 0)
		{
			mState = PlantState::STATE_COBCANNON_LOADING;
			PlayBodyReanim("anim_charge", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
		}
	}
	else if (mState == PlantState::STATE_COBCANNON_LOADING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.5f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_SHOOP);
		}
		if (aBodyReanim->mLoopCount > 0)
		{
			mState = PlantState::STATE_COBCANNON_READY;
			PlayIdleAnim(12.0f);
		}
	}
	else if (mState == PlantState::STATE_COBCANNON_READY)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		ReanimatorTrackInstance* aCobTrack = aBodyReanim->GetTrackInstanceByName("CobCannon_cob");
		aCobTrack->mTrackColor = GetFlashingColor(mBoard->mMainCounter, 75);
	}
	else if (mState == PlantState::STATE_COBCANNON_FIRING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.48f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_COB_LAUNCH);
		}
	}
}

void Plant::UpdateCactus()
{
	if (mShootingCounter > 0)
		return;

	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mState == PlantState::STATE_CACTUS_RISING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			mState = PlantState::STATE_CACTUS_HIGH;
			PlayBodyReanim("anim_idlehigh", ReanimLoopType::REANIM_LOOP, 20, 0.0f);
			if (mApp->IsIZombieLevel())
			{
				aBodyReanim->mAnimRate = 0;
			}

			mLaunchCounter = 1;
		}
	}
	else if (mState == PlantState::STATE_CACTUS_HIGH)
	{
		if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY) == nullptr)
		{
			mState = PlantState::STATE_CACTUS_LOWERING;
			PlayBodyReanim("anim_lower", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aBodyReanim->mDefinition->mFPS);
		}
	}
	else if (mState == PlantState::STATE_CACTUS_LOWERING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			mState = PlantState::STATE_CACTUS_LOW;
			PlayIdleAnim(0.0f);
		}
	}
	else if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
	{
		mState = PlantState::STATE_CACTUS_RISING;
		PlayBodyReanim("anim_rise", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aBodyReanim->mDefinition->mFPS);
		mApp->PlayFoley(FoleyType::FOLEY_PLANTGROW);
	}
}

void Plant::UpdateChomper()
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (mState == PlantState::STATE_READY)
    {
        if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
        {
            PlayBodyReanim("anim_bite", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
            mState = PlantState::STATE_CHOMPER_BITING;
            mStateCountdown = 70;
            // @pvz-online: 第二 buff「Chain Bite」连锁吞（大嘴花 #6，docs/06 §8.6，2★ cap2）：
            // 每层追加2次追咬，最多5口；非闯关不增加追咬。
            mChainBitesLeft = 2 * mApp->RunPlantBuff2Count(SeedType::SEED_CHOMPER);
        }
    }
    else if (mState == PlantState::STATE_CHOMPER_BITING)
    {
        if (mStateCountdown == 0)
        {
            mApp->PlayFoley(FoleyType::FOLEY_BIGCHOMP);

            Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
            bool doBite = false;
            if (aZombie)
            {
                // @pvz-online: 关底 boss 同巨人口径——大嘴花吞不下，只咬 40 伤/口
                //（MOD_BUILD 39 批，2026-10-11 用户定案「吞食免疫」；连锁吞 buff
                // 重播的也是这条 BITTING 路径，一并继承）。
                if (aZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
                    aZombie->mZombieType == ZombieType::ZOMBIE_BOSS || mBoard->IsRunBossZombie(aZombie))
                {
                    doBite = true;
                }
            }
            bool doMiss = false;
            if (aZombie == nullptr)
            {
                doMiss = true;
            }
            else if (!aZombie->IsImmobilizied())
            {
                if (aZombie->IsBouncingPogo() ||
                    aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
                {
                    doMiss = true;
                }
            }

            if (doBite)
            {
                mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
                const bool aGiantBite = mApp->RunExtraBuffCount(RUN_BUFF2_GIANT_BITE) > 0;
                aZombie->TakeDamage(aGiantBite ? 10000 : 40, 0U);
                if (aGiantBite)
                {
                    // A lethal heavy bite still owes digestion, even with chains left.
                    if (aZombie->IsDeadOrDying()) mChainBitesLeft = 0;
                    mState = PlantState::STATE_CHOMPER_BITING_GOT_ONE;
                }
                else
                    mState = PlantState::STATE_CHOMPER_BITING_MISSED;
            }
            else if (doMiss)
            {
                mState = PlantState::STATE_CHOMPER_BITING_MISSED;
            }
            else
            {
                aZombie->DieWithLoot();
                mState = PlantState::STATE_CHOMPER_BITING_GOT_ONE;
            }
        }
    }
    else if (mState == PlantState::STATE_CHOMPER_BITING_GOT_ONE)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            // @pvz-online: 第二 buff「Chain Bite」连锁吞（大嘴花 #6）：本口咬死目标后还有追咬
            // 次数、且正前方又有可咬僵尸，就重播一次咬合（与 READY 处同款，不嚼不消化）；
            // 次数耗尽或前方没目标再照原路进消化。
            if (mChainBitesLeft > 0 && FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
            {
                --mChainBitesLeft;
                PlayBodyReanim("anim_bite", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
                mState = PlantState::STATE_CHOMPER_BITING;
                mStateCountdown = 70;
            }
            else
            {
                mChainBitesLeft = 0;
                PlayBodyReanim("anim_chew", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
                if (mApp->IsIZombieLevel())
                {
                    aBodyReanim->mAnimRate = 0;
                }

                mState = PlantState::STATE_CHOMPER_DIGESTING;
                // @pvz-online: 单株升级「Ravenous」（大嘴花，表行 SEED_CHOMPER）：咀嚼（消化）
                // 时间 ×0.2（40秒→8秒），只可选1层；噬巨每轮或击杀后同样进入消化。
                mStateCountdown = (int)(4000 * mApp->RunPlantUpgradeMul(SeedType::SEED_CHOMPER) + 0.5f);
            }
        }
    }
    else if (mState == PlantState::STATE_CHOMPER_DIGESTING)
    {
        if (mStateCountdown == 0)
        {
            // @pvz-online: heal only at completed digestion, not on each chained bite.
            if (mApp->RunExtraBuffCount(RUN_BUFF2_DIGEST_HEAL) > 0)
                mPlantHealth += std::min(mPlantMaxHealth - mPlantHealth, (int)((long long)mPlantMaxHealth * 30 / 100));
            PlayBodyReanim("anim_swallow", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
            mState = PlantState::STATE_CHOMPER_SWALLOWING;
        }
    }
    else if ((mState == PlantState::STATE_CHOMPER_SWALLOWING || mState == PlantState::STATE_CHOMPER_BITING_MISSED) && aBodyReanim->mLoopCount > 0)
    {
        PlayIdleAnim(aBodyReanim->mDefinition->mFPS);
        mState = PlantState::STATE_READY;
    }
}

MagnetItem* Plant::GetFreeMagnetItem()
{
	if (mSeedType == SeedType::SEED_GOLD_MAGNET)
	{
		for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
		{
			if (mMagnetItems[i].mItemType == MagnetItemType::MAGNET_ITEM_NONE)
			{
				return &mMagnetItems[i];
			}
		}

		return nullptr;
	}

	return &mMagnetItems[0];
}

void Plant::MagnetShroomAttactItem(Zombie* theZombie)
{
    mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
    mStateCountdown = MagnetShroomRechargeFrames(mApp);
    PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
    mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);

    MagnetItem* aMagnetItem = GetFreeMagnetItem();
    if (PlantZombies::Armor(theZombie->mZombieType)==2 && theZombie->HasPlantHat())
    {
        const int damage=theZombie->PlantHatDamageIndex();
        theZombie->GetTrackPosition("anim_head1",aMagnetItem->mPosX,aMagnetItem->mPosY);
        const int family=PlantZombies::Family(theZombie->mZombieType);
        aMagnetItem->mPosX+=PlantZombies::HatOffsetX(family)-IMAGE_REANIM_ZOMBIE_BUCKET1->GetWidth()/2;
        aMagnetItem->mPosY+=PlantZombies::HatOffsetY(family)-IMAGE_REANIM_ZOMBIE_BUCKET1->GetHeight()/2;
        theZombie->DropPlantHat(1u<<(int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY);
        aMagnetItem->mDestOffsetX=RandRangeFloat(-10.0f,10.0f)+25.0f;
        aMagnetItem->mDestOffsetY=RandRangeFloat(-10.0f,10.0f)+20.0f;
        aMagnetItem->mItemType=(MagnetItemType)((int)MagnetItemType::MAGNET_ITEM_PAIL_1+damage);
    }
    else if (theZombie->mHelmType == HelmType::HELMTYPE_PAIL)
    {
        int aDamageIndex = theZombie->GetHelmDamageIndex();

        theZombie->mHelmHealth = 0;
        theZombie->mHelmType = HelmType::HELMTYPE_NONE;
        theZombie->GetTrackPosition("anim_bucket", aMagnetItem->mPosX, aMagnetItem->mPosY);
        theZombie->ReanimShowPrefix("anim_bucket", RENDER_GROUP_HIDDEN);
        theZombie->ReanimShowPrefix("anim_hair", RENDER_GROUP_NORMAL);

        aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_BUCKET1->GetWidth() / 2;
        aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_BUCKET1->GetHeight() / 2;
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 25.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
        aMagnetItem->mItemType = (MagnetItemType)((int)MagnetItemType::MAGNET_ITEM_PAIL_1 + aDamageIndex);
    }
    else if (theZombie->mHelmType == HelmType::HELMTYPE_FOOTBALL)
    {
        int aDamageIndex = theZombie->GetHelmDamageIndex();

        theZombie->mHelmHealth = 0;
        theZombie->mHelmType = HelmType::HELMTYPE_NONE;
        theZombie->GetTrackPosition("zombie_football_helmet", aMagnetItem->mPosX, aMagnetItem->mPosY);
        theZombie->ReanimShowPrefix("zombie_football_helmet", RENDER_GROUP_HIDDEN);
        theZombie->ReanimShowPrefix("anim_hair", RENDER_GROUP_NORMAL);

        aMagnetItem->mPosX += 37.0f;
        aMagnetItem->mPosY -= 60.0f;
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
        aMagnetItem->mItemType = (MagnetItemType)((int)MagnetItemType::MAGNET_ITEM_FOOTBALL_HELMET_1 + aDamageIndex);
    }
    else if (theZombie->mShieldType == ShieldType::SHIELDTYPE_DOOR)
    {
        int aDamageIndex = theZombie->GetShieldDamageIndex();

        theZombie->DetachShield();
        theZombie->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
        if (!theZombie->mIsEating)
        {
            PVZP_ASSERT(theZombie->mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL);
            theZombie->StartWalkAnim(0);
        }
        theZombie->GetTrackPosition("anim_screendoor", aMagnetItem->mPosX, aMagnetItem->mPosY);

        aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_SCREENDOOR1->GetWidth() / 2;
        aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_SCREENDOOR1->GetHeight() / 2;
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 30.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
        aMagnetItem->mItemType = (MagnetItemType)((int)MagnetItemType::MAGNET_ITEM_DOOR_1 + aDamageIndex);
    }
    else if (theZombie->mShieldType == ShieldType::SHIELDTYPE_LADDER)
    {
        int aDamageIndex = theZombie->GetShieldDamageIndex();

        theZombie->DetachShield();

        aMagnetItem->mPosX = theZombie->mPosX + 31.0f;
        aMagnetItem->mPosY = theZombie->mPosY + 20.0f;
        aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_LADDER_5->GetWidth() / 2;
        aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_LADDER_5->GetHeight() / 2;
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 30.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
        aMagnetItem->mItemType = (MagnetItemType)((int)MagnetItemType::MAGNET_ITEM_LADDER_1 + aDamageIndex);
    }
    else if (theZombie->mZombieType == ZombieType::ZOMBIE_POGO)
    {
        theZombie->PogoBreak(16U);
        // ZombieDrawPosition aDrawPos;
        // theZombie->GetDrawPos(aDrawPos);
        theZombie->GetTrackPosition("Zombie_pogo_stick", aMagnetItem->mPosX, aMagnetItem->mPosY);

        aMagnetItem->mPosX += 40.0f - IMAGE_REANIM_ZOMBIE_LADDER_5->GetWidth() / 2;
        aMagnetItem->mPosY += 84.0f - IMAGE_REANIM_ZOMBIE_LADDER_5->GetHeight() / 2;
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 30.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
        aMagnetItem->mItemType = theZombie->mHasArm ? MagnetItemType::MAGNET_ITEM_POGO_1 : MagnetItemType::MAGNET_ITEM_POGO_3;
    }
    else if (theZombie->mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING)
    {
        theZombie->StopZombieSound();
        theZombie->PickRandomSpeed();
        theZombie->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
        theZombie->ReanimShowPrefix("Zombie_jackbox_box", RENDER_GROUP_HIDDEN);
        theZombie->ReanimShowPrefix("Zombie_jackbox_handle", RENDER_GROUP_HIDDEN);
        theZombie->GetTrackPosition("Zombie_jackbox_box", aMagnetItem->mPosX, aMagnetItem->mPosY);

        aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_JACKBOX_BOX->GetWidth() / 2;
        aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_JACKBOX_BOX->GetHeight() / 2;
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 15.0f;
        aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_JACK_IN_THE_BOX;
    }
    else if (theZombie->mZombieType == ZombieType::ZOMBIE_DIGGER)
    {
        theZombie->DiggerLoseAxe();
        theZombie->GetTrackPosition("Zombie_digger_pickaxe", aMagnetItem->mPosX, aMagnetItem->mPosY);

        aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_DIGGER_PICKAXE->GetWidth() / 2;
        aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_DIGGER_PICKAXE->GetHeight() / 2;
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 45.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 15.0f;
        aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_PICK_AXE;
    }

    // @pvz-online: 第二 buff「Magnet Throw」卸甲飞掷（磁力菇 #31，docs/06 §8.6，2★ cap2）：
    // 被吸走防具的僵尸就地吃 1200×层 直伤（「防具掷回」的落点）——挂在吸取函数末尾，
    // 六个分支（桶/橄榄球盔/纱门/梯子/跳杆/弹簧盒/镐）吸到什么就砸什么；伤害标志 0
    // 走常规结算（同荆棘之壁）。吸取目标必是活体僵尸，无需再挡死亡态。
    int aMagnetThrowStacks = mApp->RunPlantBuff2Count(SeedType::SEED_MAGNETSHROOM);
    if (aMagnetThrowStacks > 0)
    {
        theZombie->TakeDamage(1200 * aMagnetThrowStacks, 0U);
    }
}

bool Plant::DrawMagnetItemsOnTop()
{
	if (mSeedType == SeedType::SEED_GOLD_MAGNET)
	{
		for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
		{
			if (mMagnetItems[i].mItemType != MagnetItemType::MAGNET_ITEM_NONE)
			{
				return true;
			}
		}

		return false;
	}

	if (mSeedType == SeedType::SEED_MAGNETSHROOM)
	{
		for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
		{
			MagnetItem* aMagnetItem = &mMagnetItems[i];
			if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
			{
				SexyVector2 aVectorToPlant(mX + aMagnetItem->mDestOffsetX - aMagnetItem->mPosX, mY + aMagnetItem->mDestOffsetY - aMagnetItem->mPosY);
				if (aVectorToPlant.Magnitude() > 20.0f)
				{
					return true;
				}
			}
		}

		return false;
	}

	return false;
}

void Plant::UpdateMagnetShroom()
{
    for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
    {
        MagnetItem* aMagnetItem = &mMagnetItems[i];
        if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
        {
            SexyVector2 aVectorToPlant(mX + aMagnetItem->mDestOffsetX - aMagnetItem->mPosX, mY + aMagnetItem->mDestOffsetY - aMagnetItem->mPosY);
            if (aVectorToPlant.Magnitude() > 20.0f)
            {
                aMagnetItem->mPosX += aVectorToPlant.x * 0.05f;
                aMagnetItem->mPosY += aVectorToPlant.y * 0.05f;
            }
        }
    }

    if (mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
    {
        if (mStateCountdown == 0)
        {
            mState = PlantState::STATE_READY;

            float aAnimRate = RandRangeFloat(10.0f, 15.0f);
            PlayBodyReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 30, aAnimRate);
            if (mApp->IsIZombieLevel())
            {
                Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
                aBodyReanim->mAnimRate = 0.0f;
            }

            mMagnetItems[0].mItemType = MagnetItemType::MAGNET_ITEM_NONE;
        }
    }
    else if (mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
        if (aBodyReanim->mLoopCount > 0)
        {
            PlayBodyReanim("anim_nonactive_idle2", ReanimLoopType::REANIM_LOOP, 20, 2.0f);
            if (mApp->IsIZombieLevel())
            {
                aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
                aBodyReanim->mAnimRate = 0.0f;
            }

            mState = PlantState::STATE_MAGNETSHROOM_CHARGING;
        }
    }
    else
    {
        float aClosestDistance = 0.0f;
        Zombie* aClosestZombie = nullptr;

        Zombie* aZombie = nullptr;
        while (mBoard->IterateZombies(aZombie))
        {
            int aDiffY = aZombie->mRow - mRow;
            Rect aZombieRect = aZombie->GetZombieRect();

            if (aZombie->mMindControlled)
                continue;

            if (!aZombie->mHasHead)
                continue;

            if (aZombie->mZombieHeight != ZombieHeight::HEIGHT_ZOMBIE_NORMAL || aZombie->mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE)
                continue;

            if (aZombie->IsDeadOrDying())
                continue;

            // @pvz-online: 关底 boss 免疫磁力菇（MOD_BUILD 39 批，2026-10-11 用户定案
            //「不能吸」）——橄榄球 boss 的头盔吸不走；普通橄榄球/铁桶/纱门照吸（下面
            // 的装备名单不动）。本扫描是 MagnetShroomAttactItem 的唯一入口，挡在这里
            // 即覆盖吸取与「卸甲飞掷」buff 两段。
            if (mBoard->IsRunBossZombie(aZombie))
                continue;

            if (aZombieRect.mX > BOARD_WIDTH || aDiffY > 2 || aDiffY < -2)
                continue;

            if (aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
                aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED ||
                aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING ||
                aZombie->mZombieType == ZombieType::ZOMBIE_POGO)
            {
                if (!aZombie->mHasObject)
                    continue;
            }
            else if (!((PlantZombies::Armor(aZombie->mZombieType)==2 && aZombie->HasPlantHat()) ||
                aZombie->mHelmType == HelmType::HELMTYPE_PAIL ||
                aZombie->mHelmType == HelmType::HELMTYPE_FOOTBALL ||
                aZombie->mShieldType == ShieldType::SHIELDTYPE_DOOR ||
                aZombie->mShieldType == ShieldType::SHIELDTYPE_LADDER ||
                aZombie->mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING))
                continue;

            int aRadius = aZombie->mIsEating ? 320 : 270;
            if (GetCircleRectOverlap(mX, mY + 20, aRadius, aZombieRect))
            {
                float aDistance = Distance2D(mX, mY, aZombieRect.mX, aZombieRect.mY);
                aDistance += abs(aDiffY) * 80.0f;

                if (aClosestZombie == nullptr || aDistance < aClosestDistance)
                {
                    aClosestZombie = aZombie;
                    aClosestDistance = aDistance;
                }
            }
        }

        if (aClosestZombie)
        {
            MagnetShroomAttactItem(aClosestZombie);
            return;
        }

        ////////////////////

        float aClosestLadderDist = 0.0f;
        GridItem* aClosestLadder = nullptr;

        GridItem* aGridItem = nullptr;
        while (mBoard->IterateGridItems(aGridItem))
        {
            if (aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
            {
                int aDiffX = abs(aGridItem->mGridX - mPlantCol);
                int aDiffY = abs(aGridItem->mGridY - mRow);
                int aSquareDistance = std::max(aDiffX, aDiffY);
                if (aSquareDistance <= 2)
                {
                    float aDistance = aSquareDistance + aDiffY * 0.05f;
                    if (aClosestLadder == nullptr || aDistance < aClosestLadderDist)
                    {
                        aClosestLadder = aGridItem;
                        aClosestLadderDist = aDistance;
                    }
                }
            }
        }

        if (aClosestLadder)
        {
            mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
            mStateCountdown = MagnetShroomRechargeFrames(mApp);
            PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
            mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);

            aClosestLadder->GridItemDie();

            MagnetItem* aMagnetItem = GetFreeMagnetItem();
            aMagnetItem->mPosX = mBoard->GridToPixelX(aClosestLadder->mGridX, aClosestLadder->mGridY) + 40;
            aMagnetItem->mPosY = mBoard->GridToPixelY(aClosestLadder->mGridX, aClosestLadder->mGridY);
            aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 10.0f;
            aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
            aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_LADDER_PLACED;
        }
    }
}

Coin* Plant::FindGoldMagnetTarget()
{
	Coin* aClosestCoin = nullptr;
	float aClosestDistance = 0.0f;

	for (Coin* aCoin : mBoard->mCoins)
	{
		if (aCoin->mDead)
			continue;
		if (aCoin->IsMoney() && aCoin->mCoinMotion != CoinMotion::COIN_MOTION_FROM_PRESENT && !aCoin->mIsBeingCollected && aCoin->mCoinAge >= 50)
		{
			float aDistance = Distance2D(mX + mWidth / 2, mY + mHeight / 2, aCoin->mPosX + aCoin->mWidth / 2, aCoin->mPosY + aCoin->mHeight / 2);
			if (aClosestCoin == nullptr || aDistance < aClosestDistance)
			{
				aClosestCoin = aCoin;
				aClosestDistance = aDistance;
			}
		}
	}

	return aClosestCoin;
}

void Plant::GoldMagnetFindTargets()
{
	if (GetFreeMagnetItem() == nullptr)
	{
		PVZP_ASSERT(false);
		return;
	}

	for (;;)
	{
		MagnetItem* aMagnetItem = GetFreeMagnetItem();
		if (aMagnetItem == nullptr)
			break;

		Coin* aCoin = FindGoldMagnetTarget();
		if (aCoin == nullptr)
			break;

		aMagnetItem->mPosX = aCoin->mPosX + 15.0f;
		aMagnetItem->mPosY = aCoin->mPosY + 15.0f;
		aMagnetItem->mDestOffsetX = RandRangeFloat(20.0f, 40.0f);
		aMagnetItem->mDestOffsetY = RandRangeFloat(-20.0f, 0.0f) + 20.0f;

		switch (aCoin->mType)
		{
		case CoinType::COIN_SILVER:     aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_SILVER_COIN;   break;
		case CoinType::COIN_GOLD:       aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_GOLD_COIN;     break;
		case CoinType::COIN_DIAMOND:    aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_DIAMOND;       break;
		default:                        PVZP_ASSERT(false);                                                       return;
		}

		aCoin->Die();
	}
}

bool Plant::IsAGoldMagnetAboutToSuck()
{
	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (!aPlant->NotOnGround() && aPlant->mSeedType == SeedType::SEED_GOLD_MAGNET && aPlant->mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
		{
			Reanimation* aBodyReanim = mApp->ReanimationGet(aPlant->mBodyReanimID);
			if (aBodyReanim->mAnimTime < 0.5f)
			{
				return true;
			}
		}
	}

	return false;
}

void Plant::UpdateGoldMagnetShroom()
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

    // @pvz-online: 单株升级「Gilded Pull」（吸金磁，表行 SEED_GOLD_MAGNET）：吸取间隔 ×0.75/层
    // （乘算、至多 3 层）。间隔由两段机械等待组成，同步缩——READY 期每帧 1/50 的起吸门与
    // 吸完后的充能 200..300 帧；吸取动画本身不动。非闯关局取用口恒 1.0，原样。
    float aIntervalMul = mApp->RunPlantUpgradeMul(SeedType::SEED_GOLD_MAGNET);

    bool aIsSuckingCoin = false;
    for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
    {
        MagnetItem* aMagnetItem = &mMagnetItems[i];
        if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
        {
            SexyVector2 aVectorToPlant(mX + aMagnetItem->mDestOffsetX - aMagnetItem->mPosX, mY + aMagnetItem->mDestOffsetY - aMagnetItem->mPosY);
            float aDistance = aVectorToPlant.Magnitude();
            if (aDistance < 20.0f)
            {
                CoinType aCoinType;
                switch (aMagnetItem->mItemType)
                {
                case MagnetItemType::MAGNET_ITEM_SILVER_COIN:   aCoinType = CoinType::COIN_SILVER;      break;
                case MagnetItemType::MAGNET_ITEM_GOLD_COIN:     aCoinType = CoinType::COIN_GOLD;        break;
                case MagnetItemType::MAGNET_ITEM_DIAMOND:       aCoinType = CoinType::COIN_DIAMOND;     break;
                default:                                        PVZP_ASSERT();                           return;
                }

                // @pvz-online: transmute silver deterministically and award sun once per absorbed coin.
                bool aGoldTouch = mApp->RunPlantBuff2Count(SeedType::SEED_GOLD_MAGNET) > 0;
                if (aGoldTouch && aCoinType == CoinType::COIN_SILVER) aCoinType = CoinType::COIN_GOLD;
                if (aGoldTouch) mBoard->AddSunMoney(10);

                int aValue = Coin::GetCoinValue(aCoinType);
                mApp->mPlayerInfo->AddCoins(aValue);
                mBoard->mCoinsCollected += aValue;
                mApp->PlayFoley(FoleyType::FOLEY_COIN);

                aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_NONE;
            }
            else
            {
                float aSpeed = PvzpAnimateCurveFloatTime(30.0f, 0.0f, aDistance, 0.02f, 0.05f, PvzpCurves::CURVE_LINEAR);
                aMagnetItem->mPosX += aVectorToPlant.x * aSpeed;
                aMagnetItem->mPosY += aVectorToPlant.y * aSpeed;

                aIsSuckingCoin = true;
            }
        }
    }

    if (mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
    {
        if (mStateCountdown == 0)
        {
            mState = PlantState::STATE_READY;
        }
    }
    else if (mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
    {
        if (aBodyReanim->ShouldTriggerTimedEvent(0.4f))
        {
            mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);
            GoldMagnetFindTargets();
        }

        if (aBodyReanim->mLoopCount > 0 && !aIsSuckingCoin)
        {
            PlayIdleAnim(14.0f);
            mState = PlantState::STATE_MAGNETSHROOM_CHARGING;
            mStateCountdown = (int)(RandRangeInt(200, 300) * aIntervalMul + 0.5f);
        }
    }
    else if (!IsAGoldMagnetAboutToSuck() && Sexy::Rand((int)(50 * aIntervalMul + 0.5f)) == 0 && FindGoldMagnetTarget())
    {
        mBoard->ShowCoinBank();
        mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
        PlayBodyReanim("anim_attract", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
    }
}

void Plant::RemoveEffects()
{
	mApp->RemoveParticle(mParticleID);
	mApp->RemoveReanimation(mBodyReanimID);
	mApp->RemoveReanimation(mHeadReanimID);
	mApp->RemoveReanimation(mHeadReanimID2);
	mApp->RemoveReanimation(mHeadReanimID3);
	mApp->RemoveReanimation(mLightReanimID);
	mApp->RemoveReanimation(mBlinkReanimID);
	mApp->RemoveReanimation(mSleepingReanimID);
}

void Plant::Squish()
{
	if (NotOnGround())
		return;

	if (!mIsAsleep)
	{
		if (mSeedType == SeedType::SEED_CHERRYBOMB || mSeedType == SeedType::SEED_JALAPENO ||
			mSeedType == SeedType::SEED_DOOMSHROOM || mSeedType == SeedType::SEED_ICESHROOM)
		{
			DoSpecial();
			return;
		}
		else if (mSeedType == SeedType::SEED_POTATOMINE && mState != PlantState::STATE_NOTREADY)
		{
			DoSpecial();
			return;
		}
	}

	if (mSeedType == SeedType::SEED_SQUASH && mState != PlantState::STATE_NOTREADY)
		return;

	mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, mRow, 8);
	mSquished = true;
	mDisappearCountdown = 500;
	mApp->PlayFoley(FoleyType::FOLEY_SQUISH);
	RemoveEffects();

	GridItem* aLadder = mBoard->GetLadderAt(mPlantCol, mRow);
	if (aLadder)
	{
		aLadder->GridItemDie();
	}

	if (mApp->IsIZombieLevel())
	{
		mBoard->mChallenge->IZombiePlantDropRemainingSun(this);
	}
}

void Plant::UpdateBowling()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim && aBodyReanim->TrackExists("_ground"))
	{
		float aSpeed = aBodyReanim->GetTrackVelocity("_ground");
		if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
		{
			aSpeed *= 2;
		}

		mX -= aSpeed;
		if (mX > 800)
			Die();
	}

	if (mState == PlantState::STATE_BOWLING_UP)
	{
		mY -= 2;
	}
	else if (mState == PlantState::STATE_BOWLING_DOWN)
	{
		mY += 2;
	}
	int aDistToGrid = mBoard->GridToPixelY(0, mRow) - mY;
	if (aDistToGrid < -2 || aDistToGrid > 2)
		return;

	PlantState aNewState = mState;
	if (mState == PlantState::STATE_BOWLING_UP && mRow <= 0)
	{
		aNewState = PlantState::STATE_BOWLING_DOWN;
	}
	else if (mState == PlantState::STATE_BOWLING_DOWN && mRow >= 4)
	{
		aNewState = PlantState::STATE_BOWLING_UP;
	}

	Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
	if (aZombie)
	{
		int aPosX = mX + mWidth / 2;
		int aPosY = mY + mHeight / 2;

		if (mSeedType == SeedType::SEED_EXPLODE_O_NUT)
		{
			mApp->PlayFoley(FoleyType::FOLEY_CHERRYBOMB);
			mApp->PlaySample(SOUND_BOWLINGIMPACT2);

			int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY) | 32U;
			mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, 90, 1, true, aDamageRangeFlags);
			mApp->AddPvzpParticle(aPosX, aPosY, static_cast<int>(RenderLayer::RENDER_LAYER_TOP), ParticleEffect::PARTICLE_POWIE);
			mBoard->ShakeBoard(3, -4);

			Die();

			return;
		}

		mApp->PlayFoley(FoleyType::FOLEY_BOWLINGIMPACT);
		mBoard->ShakeBoard(1, -2);

		if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
		{
			aZombie->TakeDamage(1800, 0U);
		}
		else if (aZombie->mShieldType == ShieldType::SHIELDTYPE_DOOR && mState != PlantState::STATE_NOTREADY)
		{
			aZombie->TakeDamage(1800, 0U);
		}
		else if (aZombie->mShieldType != ShieldType::SHIELDTYPE_NONE)
		{
			aZombie->TakeShieldDamage(400, 0U);
		}
		else if (aZombie->mHelmType != HelmType::HELMTYPE_NONE)
		{
			if (aZombie->mHelmType == HelmType::HELMTYPE_PAIL)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
			}
			else if (aZombie->mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE)
			{
				mApp->PlayFoley(FoleyType::FOLEY_PLASTIC_HIT);
			}

			aZombie->TakeHelmDamage(900, 0U);
		}
		else
		{
			aZombie->TakeDamage(1800, 0U);
		}

		if ((!mApp->IsFirstTimeAdventureMode() || mApp->mPlayerInfo->GetLevel() > 10) && mSeedType == SeedType::SEED_WALLNUT)
		{
			mLaunchCounter++;
			if (mLaunchCounter == 2)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
				mBoard->AddCoin(aPosX, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
			}
			else if (mLaunchCounter == 3)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
				mBoard->AddCoin(aPosX - 5.0f, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
				mBoard->AddCoin(aPosX + 5.0f, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
			}
			else if (mLaunchCounter == 4)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
				mBoard->AddCoin(aPosX - 10.0f, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
				mBoard->AddCoin(aPosX, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
				mBoard->AddCoin(aPosX + 10.0f, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
			}
			else if (mLaunchCounter >= 5)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
				mBoard->AddCoin(aPosX, aPosY, CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_COIN);
				ReportAchievement::GiveAchievement(mApp, RollSomeHeads, true);
			}
		}

		if (mSeedType != SeedType::SEED_GIANT_WALLNUT)
		{
			if (mRow == 4 || mState == PlantState::STATE_BOWLING_DOWN)
			{
				aNewState = PlantState::STATE_BOWLING_UP;
			}
			else if (mRow == 0 || mState == PlantState::STATE_BOWLING_UP)
			{
				aNewState = PlantState::STATE_BOWLING_DOWN;
			}
			else
			{
				aNewState = Sexy::Rand(2) ? PlantState::STATE_BOWLING_UP : PlantState::STATE_BOWLING_DOWN;
			}
		}
	}

	if (aNewState == PlantState::STATE_BOWLING_UP)
	{
		mRow--;
		mState = PlantState::STATE_BOWLING_UP;
		mRenderOrder = CalcRenderOrder();
	}
	else if (aNewState == PlantState::STATE_BOWLING_DOWN)
	{
		mState = PlantState::STATE_BOWLING_DOWN;
		mRenderOrder = CalcRenderOrder();
		mRow++;
	}
}

void Plant::UpdateAbilities()
{
    if (!IsInPlay())
        return;

    if (mState == PlantState::STATE_DOINGSPECIAL || mSquished)
    {
        mDisappearCountdown--;
        if (mDisappearCountdown < 0)
        {
            Die();
            return;
        }
    }

    if (mWakeUpCounter > 0)
    {
        mWakeUpCounter--;
        if (mWakeUpCounter == 60)
        {
            mApp->PlayFoley(FoleyType::FOLEY_WAKEUP);
        }
        if (mWakeUpCounter == 0)
        {
            SetSleeping(false);
        }
    }

    if (mIsAsleep || mSquished || mOnBungeeState != PlantOnBungeeState::NOT_ON_BUNGEE)
        return;

    UpdatePurpleBuffs();
    UpdateShooting();

    if (mStateCountdown > 0)
        mStateCountdown--;

    if (mApp->IsWallnutBowlingLevel() || IsBowlingNut(this))
    {
        UpdateBowling();
        return;
    }

    if (mSeedType == SeedType::SEED_SQUASH)                                                     UpdateSquash();
    else if (mSeedType == SeedType::SEED_DOOMSHROOM)                                            UpdateDoomShroom();
    else if (mSeedType == SeedType::SEED_ICESHROOM)                                             UpdateIceShroom();
    else if (mSeedType == SeedType::SEED_CHOMPER)                                               UpdateChomper();
    else if (mSeedType == SeedType::SEED_BLOVER)                                                UpdateBlover();
    else if (mSeedType == SeedType::SEED_FLOWERPOT)                                             UpdateFlowerPot();
    else if (mSeedType == SeedType::SEED_LILYPAD)                                               UpdateLilypad();
    else if (mSeedType == SeedType::SEED_IMITATER)                                              UpdateImitater();
    else if (mSeedType == SeedType::SEED_INSTANT_COFFEE)                                        UpdateCoffeeBean();
    else if (mSeedType == SeedType::SEED_UMBRELLA)                                              UpdateUmbrella();
    else if (mSeedType == SeedType::SEED_COBCANNON)                                             UpdateCobCannon();
    else if (mSeedType == SeedType::SEED_CACTUS)                                                UpdateCactus();
    else if (mSeedType == SeedType::SEED_MAGNETSHROOM)                                          UpdateMagnetShroom();
    else if (mSeedType == SeedType::SEED_GOLD_MAGNET)                                           UpdateGoldMagnetShroom();
    else if (mSeedType == SeedType::SEED_SUNSHROOM)                                             UpdateSunShroom();
    else if (MakesSun() || mSeedType == SeedType::SEED_MARIGOLD)                                UpdateProductionPlant();
    else if (mSeedType == SeedType::SEED_GRAVEBUSTER)                                           UpdateGraveBuster();
    else if (mSeedType == SeedType::SEED_TORCHWOOD)                                             UpdateTorchwood();
    else if (mSeedType == SeedType::SEED_POTATOMINE)                                            UpdatePotato();
    else if (mSeedType == SeedType::SEED_SPIKEWEED || mSeedType == SeedType::SEED_SPIKEROCK)    UpdateSpikeweed();
    else if (mSeedType == SeedType::SEED_TANGLEKELP)                                            UpdateTanglekelp();
    else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)                                         UpdateScaredyShroom();
    else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
    {
        // @pvz-online: 第二 buff「Regrow」再生（南瓜头 #30，docs/06 §8.6，2★ cap2）：每 1500 帧
        //（15 秒）回复 10%×层 最大血量，不超上限。计时器借 mLaunchCounter——南瓜头该字段
        // 全程闲置（PlantInitialize 里 mLaunchRate 为 0 走 else 置 0，全代码无第二处使用）。
        // 未抽条恒 0，整段不进。
        int aRegrowStacks = mApp->RunPlantBuff2Count(SeedType::SEED_PUMPKINSHELL);
        if (aRegrowStacks > 0 && ++mLaunchCounter >= 1500)
        {
            mLaunchCounter = 0;
            if (mPlantHealth < mPlantMaxHealth)
            {
                int aHealTarget = mPlantHealth + (mPlantMaxHealth / 10) * aRegrowStacks;
                mPlantHealth = aHealTarget > mPlantMaxHealth ? mPlantMaxHealth : aHealTarget;
            }
        }
    }

    if (mSubclass == PlantSubClass::SUBCLASS_SHOOTER)
    {
        UpdateShooter();
    }
    if (mDoSpecialCountdown > 0)
    {
        mDoSpecialCountdown--;
        if (mDoSpecialCountdown == 0)
        {
            DoSpecial();
        }
    }
}

bool Plant::IsPartOfUpgradableTo(SeedType theUpgradedType)
{
	if (theUpgradedType == SeedType::SEED_COBCANNON && mSeedType == SeedType::SEED_KERNELPULT)
	{
		return mBoard->IsValidCobCannonSpot(mPlantCol, mRow) || mBoard->IsValidCobCannonSpot(mPlantCol - 1, mRow);
	}

	return IsUpgradableTo(theUpgradedType);
}

bool Plant::IsUpgradableTo(SeedType theUpgradedType)
{
	if (theUpgradedType == SeedType::SEED_GATLINGPEA && mSeedType == SeedType::SEED_REPEATER)
	{
		return true;
	}
	if (theUpgradedType == SeedType::SEED_WINTERMELON && mSeedType == SeedType::SEED_MELONPULT)
	{
		return true;
	}
	if (theUpgradedType == SeedType::SEED_TWINSUNFLOWER && mSeedType == SeedType::SEED_SUNFLOWER)
	{
		return true;
	}
	if (theUpgradedType == SeedType::SEED_SPIKEROCK && mSeedType == SeedType::SEED_SPIKEWEED)
	{
		return true;
	}
	if (theUpgradedType == SeedType::SEED_COBCANNON && mSeedType == SeedType::SEED_KERNELPULT)
	{
		return mBoard->IsValidCobCannonSpot(mPlantCol, mRow);
	}
	if (theUpgradedType == SeedType::SEED_GOLD_MAGNET && mSeedType == SeedType::SEED_MAGNETSHROOM)
	{
		return true;
	}
	if (theUpgradedType == SeedType::SEED_GLOOMSHROOM && mSeedType == SeedType::SEED_FUMESHROOM)
	{
		return true;
	}
	if (theUpgradedType == SeedType::SEED_CATTAIL && mSeedType == SeedType::SEED_LILYPAD)
	{
		Plant* aPlant = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		return aPlant == nullptr || aPlant->mSeedType != SeedType::SEED_CATTAIL;
	}
	return false;
}

void Plant::UpdateReanimColor()
{
	if (!IsOnBoard())
		return;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	SeedType aSeedType = mBoard->GetSeedTypeInCursor();
	Color aColorOverride;

	bool isOnGlove = false;
	if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE)
	{
		Plant* aPlant = mBoard->mPlants.DataArrayTryToGet(static_cast<unsigned int>(mBoard->mCursorObject->mGlovePlantID));
		if (aPlant && aPlant->mPlantCol == mPlantCol && aPlant->mRow == mRow)
		{
			isOnGlove = true;
		}
	}

	if (isOnGlove)
	{
		aColorOverride = Color(128, 128, 128);
	}
	else if (IsPartOfUpgradableTo(aSeedType) && mBoard->CanPlantAt(mPlantCol, mRow, aSeedType) == PLANTING_OK)
	{
		aColorOverride = GetFlashingColor(mBoard->mMainCounter, 90);
	}
	else if (aSeedType == SeedType::SEED_COBCANNON && mSeedType == SeedType::SEED_KERNELPULT && mBoard->CanPlantAt(mPlantCol - 1, mRow, aSeedType) == PLANTING_OK)
	{
		aColorOverride = GetFlashingColor(mBoard->mMainCounter, 90);
	}
	else if (mSeedType == SeedType::SEED_EXPLODE_O_NUT)
	{
		aColorOverride = Color(255, 64, 64);
	}
	else
	{
		aColorOverride = Color(255, 255, 255);
	}

	aBodyReanim->mColorOverride = aColorOverride;

	if (mHighlighted)
	{
		aBodyReanim->mExtraAdditiveColor = Color(255, 255, 255, 196);
		aBodyReanim->mEnableExtraAdditiveDraw = true;
		if (mImitaterType == SeedType::SEED_IMITATER)
		{
			aBodyReanim->mExtraAdditiveColor = Color(255, 255, 255, 92);
		}
	}
	else if (mBeghouledFlashCountdown > 0)
	{
		int anAlpha = PvzpAnimateCurve(50, 0, mBeghouledFlashCountdown % 50, 1, 128, PvzpCurves::CURVE_BOUNCE);
		aBodyReanim->mExtraAdditiveColor = Color(255, 255, 255, anAlpha);
		aBodyReanim->mEnableExtraAdditiveDraw = true;
	}
	else if (mEatenFlashCountdown > 0)
	{
		int aGrayness = std::clamp(mEatenFlashCountdown * 3, 0, mImitaterType == SeedType::SEED_IMITATER ? 128 : 255);
		aBodyReanim->mExtraAdditiveColor = Color(aGrayness, aGrayness, aGrayness);
		aBodyReanim->mEnableExtraAdditiveDraw = true;
	}
	else
	{
		aBodyReanim->mEnableExtraAdditiveDraw = false;
	}

	if (mBeghouledFlashCountdown > 0)
	{
		int anAlpha = PvzpAnimateCurve(50, 0, mBeghouledFlashCountdown % 50, 1, 128, PvzpCurves::CURVE_BOUNCE);
		aBodyReanim->mExtraOverlayColor = Color(255, 255, 255, anAlpha);
		aBodyReanim->mEnableExtraOverlayDraw = true;
	}
	else
	{
		aBodyReanim->mEnableExtraOverlayDraw = false;
	}

	aBodyReanim->PropogateColorToAttachments();
}

bool Plant::IsOnBoard()
{
	if (!mIsOnBoard)
		return false;

	PVZP_ASSERT(mBoard);
	return true;
}

bool Plant::IsInPlay()
{
	return IsOnBoard() && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM;
}

void Plant::UpdateReanim()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	UpdateReanimColor();

	float aOffsetX = mShakeOffsetX;
	float aOffsetY = PlantDrawHeightOffset(mBoard, this, mSeedType, mPlantCol, mRow);
	float aScaleX = 1.0f, aScaleY = 1.0f;
	if ((mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BIG_TIME) &&
		(mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_SUNFLOWER || mSeedType == SeedType::SEED_MARIGOLD))
	{
		aScaleX = 1.5f;
		aScaleY = 1.5f;
		aOffsetX -= 20.0f;
		aOffsetY -= 40.0f;
	}
	if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
	{
		aScaleX = 2.0f;
		aScaleY = 2.0f;
		aOffsetX -= 76.0f;
		aOffsetY -= 64.0f;
	}
	if (mSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		aScaleX = 0.8f;
		aScaleY = 0.8f;
		aOffsetX += 12.0f;
		aOffsetY += 10.0f;
	}
	if (mSeedType == SeedType::SEED_POTATOMINE)
	{
		aScaleX = 0.8f;
		aScaleY = 0.8f;
		aOffsetX += 12.0f;
		aOffsetY += 12.0f;
	}
	if (mState == PlantState::STATE_GRAVEBUSTER_EATING)
	{
		aOffsetY += PvzpAnimateCurveFloat(400, 0, mStateCountdown, 0.0f, 30.0f, PvzpCurves::CURVE_LINEAR);
	}
	if (mWakeUpCounter > 0)
	{
		float aScaleFactor = PvzpAnimateCurveFloat(70, 0, mWakeUpCounter, 1.0f, 0.8f, PvzpCurves::CURVE_EASE_SIN_WAVE);
		aScaleY *= aScaleFactor;
		aOffsetY += 80.0f - 80.0f * aScaleFactor;
	}

	aBodyReanim->Update();

	if (mSeedType == SeedType::SEED_LEFTPEATER)
	{
		aOffsetX += 80.0f * aScaleX;
		aScaleX *= -1.0f;
	}

	if (mPottedPlantIndex != -1)
	{
		PottedPlant* aPottedPlant = &mApp->mPlayerInfo->mPottedPlant[mPottedPlantIndex];

		if (aPottedPlant->mFacing == PottedPlant::FacingDirection::FACING_LEFT)
		{
			aOffsetX += 80.0f * aScaleX;
			aScaleX *= -1.0f;
		}

		float aOffsetXStart, aOffsetXEnd;
		float aOffsetYStart, aOffsetYEnd;
		float aScaleStart, aScaleEnd;
		if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
		{
			aOffsetXStart = 20.0f;
			aOffsetXEnd = 20.0f;
			aOffsetYStart = 40.0f;
			aOffsetYEnd = 40.0f;
			aScaleStart = 0.5f;
			aScaleEnd = 0.5f;
		}
		else if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
		{
			aOffsetXStart = 20.0f;
			aOffsetXEnd = 10.0f;
			aOffsetYStart = 40.0f;
			aOffsetYEnd = 20.0f;
			aScaleStart = 0.5f;
			aScaleEnd = 0.75f;
		}
		else
		{
			aOffsetXStart = 10.0f;
			aOffsetXEnd = 0.0f;
			aOffsetYStart = 20.0f;
			aOffsetYEnd = 0.0f;
			aScaleStart = 0.75f;
			aScaleEnd = 1.0f;
		}

		float aAnimatedOffsetX = PvzpAnimateCurveFloat(100, 0, mStateCountdown, aOffsetXStart, aOffsetXEnd, PvzpCurves::CURVE_LINEAR);
		float aAnimatedOffsetY = PvzpAnimateCurveFloat(100, 0, mStateCountdown, aOffsetYStart, aOffsetYEnd, PvzpCurves::CURVE_LINEAR);
		float aAnimatedScale = PvzpAnimateCurveFloat(100, 0, mStateCountdown, aScaleStart, aScaleEnd, PvzpCurves::CURVE_LINEAR);

		aOffsetX += aAnimatedOffsetX * aScaleX;
		aOffsetY += aAnimatedOffsetY * aScaleY;
		aScaleX *= aAnimatedScale;
		aScaleY *= aAnimatedScale;
		aOffsetX += mApp->mZenGarden->ZenPlantOffsetX(aPottedPlant);
		aOffsetY += mApp->mZenGarden->PlantPottedDrawHeightOffset(mSeedType, aScaleY);
	}

	aBodyReanim->SetPosition(aOffsetX, aOffsetY);
	aBodyReanim->OverrideScale(aScaleX, aScaleY);
}

void Plant::Update()
{
    DamageStats::Source aDamageSource(mApp->mDamageStats, true);
	bool doUpdate = false;
	if (IsOnBoard() && mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && mApp->IsWallnutBowlingLevel())
		doUpdate = true;
	else if (IsOnBoard() && mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
		doUpdate = true;
	else if (IsOnBoard() && mBoard->mCutScene->ShouldRunUpsellBoard())
		doUpdate = true;
	else if (!IsOnBoard() || mApp->mGameScene == GameScenes::SCENE_PLAYING)
		doUpdate = true;

	if (doUpdate)
    {
        // @pvz-online: gameplay time only; menus and pauses cannot grow the reroll fund.
        if (IsOnBoard() && mApp->mGameScene == GameScenes::SCENE_PLAYING && !mDead)
        {
            if (mRunAliveTicks < 6000) ++mRunAliveTicks;
            if (mRunGarlicCooldown > 0) --mRunGarlicCooldown;
        }
        UpdateAbilities();
        Animate();

		if (mPlantHealth < 0)
			Die();

		UpdateReanim();
	}
}

bool Plant::NotOnGround()
{
	if (mSeedType == SeedType::SEED_SQUASH)
	{
		if (mState == PlantState::STATE_SQUASH_RISING || mState == PlantState::STATE_SQUASH_FALLING || mState == PlantState::STATE_SQUASH_DONE_FALLING)
			return true;
	}

	return mSquished || mOnBungeeState == PlantOnBungeeState::RISING_WITH_BUNGEE || mDead;
}

Reanimation* Plant::AttachBlinkAnim(Reanimation* theReanimBody)
{
	const PlantDefinition& aPlantDef = GetPlantDefinition(mSeedType);
	LawnApp* aApp = (LawnApp*)gSexyAppBase;
	Reanimation* aAnimToAttach = theReanimBody;
	const char* aTrackToPlay = "anim_blink";
	const char* aTrackToAttach = nullptr;

	if (mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_TALLNUT ||
		mSeedType == SeedType::SEED_EXPLODE_O_NUT || mSeedType == SeedType::SEED_GIANT_WALLNUT)
	{
		int aHit = Rand(10);
		if (aHit < 1 && theReanimBody->TrackExists("anim_blink_twitch"))
		{
			aTrackToPlay = "anim_blink_twitch";
		}
		else
		{
			aTrackToPlay = aHit < 7 ? "anim_blink_twice" : "anim_blink_thrice";
		}
	}
	else if (mSeedType == SeedType::SEED_THREEPEATER)
	{
		int aHit = Rand(3);
		if (aHit == 0)
		{
			aTrackToPlay = "anim_blink1";
			aTrackToAttach = "anim_face1";
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_head1");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
		else if (aHit == 1)
		{
			aTrackToPlay = "anim_blink2";
			aTrackToAttach = "anim_face2";
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_head2");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
		else
		{
			aTrackToPlay = "anim_blink3";
			aTrackToAttach = "anim_face3";
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_head3");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
	}
	else if (mSeedType == SeedType::SEED_SPLITPEA)
	{
		if (Rand(2) == 0)
		{
			aTrackToPlay = "anim_blink";
			aTrackToAttach = "anim_face";
			aAnimToAttach = mApp->ReanimationTryToGet(mHeadReanimID);
		}
		else
		{
			aTrackToPlay = "anim_blink2";
			aTrackToAttach = "anim_face2";
			aAnimToAttach = mApp->ReanimationTryToGet(mHeadReanimID2);
		}
	}
	else if (mSeedType == SeedType::SEED_TWINSUNFLOWER)
	{
		if (Rand(2) == 0)
		{
			aTrackToPlay = "anim_blink";
			aTrackToAttach = "anim_face";
		}
		else
		{
			aTrackToPlay = "anim_blink2";
			aTrackToAttach = "anim_face2";
		}
	}
	else if (mSeedType == SeedType::SEED_PEASHOOTER || mSeedType == SeedType::SEED_SNOWPEA || mSeedType == SeedType::SEED_REPEATER || mSeedType == SeedType::SEED_LEFTPEATER || mSeedType == SeedType::SEED_GATLINGPEA)
	{
		if (theReanimBody->TrackExists("anim_stem"))
		{
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_stem");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
		else if (theReanimBody->TrackExists("anim_idle"))
		{
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_idle");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
	}

	if (aAnimToAttach == nullptr)
	{
		PvzpLogLn("Missing head anim");
		return nullptr;
	}

	if (!theReanimBody->TrackExists(aTrackToPlay))
		return nullptr;

    auto& holder = aApp->mEffectSystem->mReanimationHolder;
    if (!holder->mReanimations.DataArrayCanAllocate(holder->mPlantReservation + 1)) return nullptr;
	Reanimation* aBlinkReanim = aApp->mEffectSystem->mReanimationHolder->AllocReanimation(0.0f, 0.0f, 0, aPlantDef.mReanimationType);
    if (!aBlinkReanim) return nullptr;
	aBlinkReanim->SetFramesForLayer(aTrackToPlay);
	aBlinkReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD;
	aBlinkReanim->mAnimRate = 15.0f;
	aBlinkReanim->mColorOverride = theReanimBody->mColorOverride;

	if (aTrackToAttach && aAnimToAttach->TrackExists(aTrackToAttach))
	{
		aBlinkReanim->AttachToAnotherReanimation(aAnimToAttach, aTrackToAttach);
	}
	else if (aAnimToAttach->TrackExists("anim_face"))
	{
		aBlinkReanim->AttachToAnotherReanimation(aAnimToAttach, "anim_face");
	}
	else if (aAnimToAttach->TrackExists("anim_idle"))
	{
		aBlinkReanim->AttachToAnotherReanimation(aAnimToAttach, "anim_idle");
	}
	else
	{
		PvzpLogLn("Missing anim_idle for blink");
	}

	aBlinkReanim->mFilterEffect = theReanimBody->mFilterEffect;
	return aBlinkReanim;
}

void Plant::DoBlink()
{
	mBlinkCountdown = 400 + Rand(400);

	if (NotOnGround() || mShootingCounter != 0)
		return;

	if (mSeedType == SeedType::SEED_POTATOMINE && mState != PlantState::STATE_POTATO_ARMED)
		return;

	if (mState == PlantState::STATE_CACTUS_RISING || mState == PlantState::STATE_CACTUS_HIGH || mState == PlantState::STATE_CACTUS_LOWERING ||
		mState == PlantState::STATE_MAGNETSHROOM_SUCKING || mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
		return;

	EndBlink();
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	if ((mSeedType == SeedType::SEED_TALLNUT && aBodyReanim->GetImageOverride("anim_idle") == IMAGE_REANIM_TALLNUT_CRACKED2) ||
		(mSeedType == SeedType::SEED_GARLIC && aBodyReanim->GetImageOverride("anim_face") == IMAGE_REANIM_GARLIC_BODY3))
		return;

	if (mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_TALLNUT ||
		mSeedType == SeedType::SEED_EXPLODE_O_NUT || mSeedType == SeedType::SEED_GIANT_WALLNUT)
	{
		mBlinkCountdown = 1000 + Rand(1000);
	}

	Reanimation* aBlinkReanim = AttachBlinkAnim(aBodyReanim);
	if (aBlinkReanim)
	{
		mBlinkReanimID = mApp->ReanimationGetID(aBlinkReanim);
	}
	aBodyReanim->AssignRenderGroupToPrefix("anim_eye", RENDER_GROUP_HIDDEN);
}

void Plant::EndBlink()
{
	if (mBlinkReanimID != ReanimationID::REANIMATIONID_NULL)
	{
		mApp->RemoveReanimation(mBlinkReanimID);
		mBlinkReanimID = ReanimationID::REANIMATIONID_NULL;

		Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
		if (aBodyReanim)
		{
			aBodyReanim->AssignRenderGroupToPrefix("anim_eye", RENDER_GROUP_NORMAL);
		}
	}
}

void Plant::UpdateBlink()
{
	if (mBlinkReanimID != ReanimationID::REANIMATIONID_NULL)
	{
		Reanimation* aBlinkReanim = mApp->ReanimationTryToGet(mBlinkReanimID);
		if (aBlinkReanim == nullptr || aBlinkReanim->mLoopCount > 0)
		{
			EndBlink();
		}
	}

	if (mIsAsleep)
		return;

	if (mBlinkCountdown > 0)
	{
		mBlinkCountdown--;
		if (mBlinkCountdown == 0)
		{
			DoBlink();
		}
	}
}

void Plant::AnimateNuts()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	Image* aCracked1;
	Image* aCracked2;
	const char* aTrackToOverride;
	if (mSeedType == SeedType::SEED_WALLNUT)
	{
		aCracked1 = IMAGE_REANIM_WALLNUT_CRACKED1;
		aCracked2 = IMAGE_REANIM_WALLNUT_CRACKED2;
		aTrackToOverride = "anim_face";
	}
	else if (mSeedType == SeedType::SEED_TALLNUT)
	{
		aCracked1 = IMAGE_REANIM_TALLNUT_CRACKED1;
		aCracked2 = IMAGE_REANIM_TALLNUT_CRACKED2;
		aTrackToOverride = "anim_idle";
	}
	else return;

	int aPosX = mX + 40;
	int aPosY = mY + 10;
	if (mSeedType == SeedType::SEED_TALLNUT)
	{
		aPosY -= 32;
	}

	Image* aImageOverride = aBodyReanim->GetImageOverride(aTrackToOverride);
	if (mPlantHealth < mPlantMaxHealth / 3)
	{
		if (aImageOverride != aCracked2)
		{
			aBodyReanim->SetImageOverride(aTrackToOverride, aCracked2);
			mApp->AddPvzpParticle(aPosX, aPosY, mRenderOrder + 4, ParticleEffect::PARTICLE_WALLNUT_EAT_LARGE);
		}
	}
	else if (mPlantHealth < mPlantMaxHealth * 2 / 3)
	{
		if (aImageOverride != aCracked1)
		{
			aBodyReanim->SetImageOverride(aTrackToOverride, aCracked1);
			mApp->AddPvzpParticle(aPosX, aPosY, mRenderOrder + 4, ParticleEffect::PARTICLE_WALLNUT_EAT_LARGE);
		}
	}
	else
	{
		aBodyReanim->SetImageOverride(aTrackToOverride, nullptr);
	}

	if (IsInPlay() && !mApp->IsIZombieLevel())
	{
		if (mRecentlyEatenCountdown > 0)
		{
			aBodyReanim->mAnimRate = 0.1f;
			return;
		}

		if (aBodyReanim->mAnimRate < 1.0f && mOnBungeeState != PlantOnBungeeState::RISING_WITH_BUNGEE)
		{
			aBodyReanim->mAnimRate = RandRangeFloat(10.0f, 15.0f);
		}
	}
}

void Plant::AnimateGarlic()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	Image* aImageOverride = aBodyReanim->GetImageOverride("anim_face");

	if (mPlantHealth < mPlantMaxHealth / 3)
	{
		if (aImageOverride != IMAGE_REANIM_GARLIC_BODY3)
		{
			aBodyReanim->SetImageOverride("anim_face", IMAGE_REANIM_GARLIC_BODY3);
			aBodyReanim->AssignRenderGroupToPrefix("Garlic_stem", RENDER_GROUP_HIDDEN);
		}
	}
	else if (mPlantHealth < mPlantMaxHealth * 2 / 3)
	{
		if (aImageOverride != IMAGE_REANIM_GARLIC_BODY2)
		{
			aBodyReanim->SetImageOverride("anim_face", IMAGE_REANIM_GARLIC_BODY2);
		}
	}
	else
	{
		aBodyReanim->SetImageOverride("anim_face", nullptr);
	}
}

void Plant::AnimatePumpkin()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	Image* aImageOverride = aBodyReanim->GetImageOverride("Pumpkin_front");

	if (mPlantHealth < mPlantMaxHealth / 3)
	{
		if (aImageOverride != IMAGE_REANIM_PUMPKIN_DAMAGE3)
			aBodyReanim->SetImageOverride("Pumpkin_front", IMAGE_REANIM_PUMPKIN_DAMAGE3);
	}
	else if (mPlantHealth < mPlantMaxHealth * 2 / 3)
	{
		if (aImageOverride != IMAGE_REANIM_PUMPKIN_DAMAGE1)
			aBodyReanim->SetImageOverride("Pumpkin_front", IMAGE_REANIM_PUMPKIN_DAMAGE1);
	}
	else
	{
		aBodyReanim->SetImageOverride("Pumpkin_front", nullptr);
	}
}

void Plant::UpdateShooting()
{
    // @pvz-online: 100 simulation ticks/sec gives exactly five hits, independent of all attack speed.
    if (mSeedType == SeedType::SEED_FUMESHROOM && mApp->RunPlantUpgradeCount(mSeedType) > 0)
    {
        if (NotOnGround())
            return;
        if (mLaunchCounter != -1)
        {
            mLaunchCounter = -1; // Cancel a pending normal shot when this buff is acquired.
            mShootingCounter = 20;
        }
        Reanimation* aBody = mApp->ReanimationTryToGet(mBodyReanimID);
        if (aBody)
        {
            int aStart, aCount;
            aBody->GetFramesForLayer("anim_shooting", aStart, aCount);
            // Restore the loop after waking; never restart it for each damage pulse.
            if (aBody->mFrameStart != aStart || aBody->mLoopType != ReanimLoopType::REANIM_LOOP)
                PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_LOOP, 0, 35.0f);
            aBody->mAnimRate = 35.0f;
        }
        if (--mShootingCounter <= 0)
        {
            mShootingCounter = 20;
            Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
            if (aBody)
            {
                int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
                AddAttachedParticle(mX + 85, mY + 31, aRenderPosition, ParticleEffect::PARTICLE_FUMECLOUD);
            }
        }
        return;
    }

    if (NotOnGround() || mShootingCounter == 0)
        return;

    mShootingCounter--;

    // @pvz-online: 命中/粒子检查点随「动画加速」同比例缩放（见 PlantShootSpeed 注释）。
    // 非攻速植物 aSpeed==1，取整后与原常量逐值相同，行为零变化。
    float aSpeed = PlantShootSpeed(mApp, mSeedType, this);

    if (mSeedType == SeedType::SEED_FUMESHROOM && mShootingCounter == PlantShootTicks(15, aSpeed))
    {
        int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
        AddAttachedParticle(mX + 85, mY + 31, aRenderPosition, ParticleEffect::PARTICLE_FUMECLOUD);
    }

    if (mSeedType == SeedType::SEED_GLOOMSHROOM)
    {
        if (mShootingCounter == PlantShootTicks(136, aSpeed) || mShootingCounter == PlantShootTicks(108, aSpeed) || mShootingCounter == PlantShootTicks(80, aSpeed) || mShootingCounter == PlantShootTicks(52, aSpeed))
        {
            int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
            AddAttachedParticle(mX + 40, mY + 40, aRenderPosition, ParticleEffect::PARTICLE_GLOOMCLOUD);
        }
        if (mShootingCounter == PlantShootTicks(126, aSpeed) || mShootingCounter == PlantShootTicks(98, aSpeed) || mShootingCounter == PlantShootTicks(70, aSpeed) || mShootingCounter == PlantShootTicks(42, aSpeed))
        {
            Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
        }
    }
    else if (mSeedType == SeedType::SEED_GATLINGPEA)
    {
        if (mShootingCounter == PlantShootTicks(18, aSpeed) || mShootingCounter == PlantShootTicks(35, aSpeed) || mShootingCounter == PlantShootTicks(51, aSpeed) || mShootingCounter == PlantShootTicks(68, aSpeed))
        {
            Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
        }
    }
    else if (mSeedType == SeedType::SEED_CATTAIL)
    {
        if (mShootingCounter == PlantShootTicks(19, aSpeed))
        {
            Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
            if (aZombie)
            {
                Fire(aZombie, mRow, PlantWeapon::WEAPON_PRIMARY);

                // @pvz-online: 单株升级「Quick Claw」（猫尾草，表行 SEED_CATTAIL）：攻击目标
                // +1 个/层（计数型、至多 2 层）——同一瞬间依次找第 2、第 3 个目标（排除已选），
                // 各发一颗追踪刺；目标不足时补打主目标。
                Zombie* aPrevZombie = aZombie;
                Zombie* aPrevZombie2 = nullptr;
                for (int i = 0, aExtra = mApp->RunPlantUpgradeCount(SeedType::SEED_CATTAIL); i < aExtra; i++)
                {
                    Zombie* aExtraZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY, aPrevZombie, aPrevZombie2);
                    if (!aExtraZombie)
                    {
                        aExtraZombie = aZombie;
                    }
                    Fire(aExtraZombie, mRow, PlantWeapon::WEAPON_PRIMARY);
                    aPrevZombie2 = aPrevZombie;
                    aPrevZombie = aExtraZombie;
                }
            }
        }
    }
    else if (mShootingCounter == 1)
    {
        if (mSeedType == SeedType::SEED_THREEPEATER)
        {
            int rowAbove = mRow - 1;
            int rowBelow = mRow + 1;
            Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);
            Reanimation* aHeadReanim3 = mApp->ReanimationGet(mHeadReanimID3);
            Reanimation* aHeadReanim1 = mApp->ReanimationGet(mHeadReanimID);

            if (aHeadReanim1->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(nullptr, rowBelow, PlantWeapon::WEAPON_PRIMARY);
            }
            if (aHeadReanim2->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
            }
            if (aHeadReanim3->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(nullptr, rowAbove, PlantWeapon::WEAPON_PRIMARY);
            }
        }
        else if (mSeedType == SeedType::SEED_SPLITPEA)
        {
            Reanimation* aHeadBackReanim = mApp->ReanimationTryToGet(mHeadReanimID2);
            Reanimation* aHeadFrontReanim = mApp->ReanimationTryToGet(mHeadReanimID);
            if (aHeadFrontReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
            }
            if (aHeadBackReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(nullptr, mRow, PlantWeapon::WEAPON_SECONDARY);
            }
        }
        else if (mState == PlantState::STATE_CACTUS_LOW)
        {
            Fire(nullptr, mRow, PlantWeapon::WEAPON_SECONDARY);
        }
        else if (mSeedType == SeedType::SEED_CABBAGEPULT || mSeedType == SeedType::SEED_KERNELPULT || mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON)
        {
            PlantWeapon aPlantWeapon = PlantWeapon::WEAPON_PRIMARY;
            if (mState == PlantState::STATE_KERNELPULT_BUTTER)
            {
                Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
                aBodyReanim->AssignRenderGroupToPrefix("Cornpult_butter", RENDER_GROUP_HIDDEN);
                aBodyReanim->AssignRenderGroupToPrefix("Cornpult_kernal", RENDER_GROUP_NORMAL);
                mState = PlantState::STATE_NOTREADY;
                aPlantWeapon = PlantWeapon::WEAPON_SECONDARY;
            }

            Zombie* aZombie = FindTargetZombie(mRow, aPlantWeapon);
            Fire(aZombie, mRow, aPlantWeapon);
        }
        else
        {
            Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
        }

        return;
    }

    if (mShootingCounter != 0)
        return;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mHeadReanimID);
    if (mSeedType == SeedType::SEED_THREEPEATER)
    {
        Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);
        Reanimation* aHeadReanim3 = mApp->ReanimationGet(mHeadReanimID3);

        if (aHeadReanim2->mLoopCount > 0)
        {
            if (aHeadReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                aHeadReanim->StartBlend(20);
                aHeadReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
                aHeadReanim->SetFramesForLayer("anim_head_idle1");
                aHeadReanim->mAnimRate = aBodyReanim->mAnimRate;
                aHeadReanim->mAnimTime = aBodyReanim->mAnimTime;
            }

            aHeadReanim2->StartBlend(20);
            aHeadReanim2->mLoopType = ReanimLoopType::REANIM_LOOP;
            aHeadReanim2->SetFramesForLayer("anim_head_idle2");
            aHeadReanim2->mAnimRate = aBodyReanim->mAnimRate;
            aHeadReanim2->mAnimTime = aBodyReanim->mAnimTime;

            if (aHeadReanim3->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                aHeadReanim3->StartBlend(20);
                aHeadReanim3->mLoopType = ReanimLoopType::REANIM_LOOP;
                aHeadReanim3->SetFramesForLayer("anim_head_idle3");
                aHeadReanim3->mAnimRate = aBodyReanim->mAnimRate;
                aHeadReanim3->mAnimTime = aBodyReanim->mAnimTime;
            }

            return;
        }
    }
    else if (mSeedType == SeedType::SEED_SPLITPEA)
    {
        Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);

        if (aHeadReanim->mLoopCount > 0)
        {
            aHeadReanim->StartBlend(20);
            aHeadReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
            aHeadReanim->SetFramesForLayer("anim_head_idle");
            aHeadReanim->mAnimRate = aBodyReanim->mAnimRate;
            aHeadReanim->mAnimTime = aBodyReanim->mAnimTime;
        }

        if (aHeadReanim2->mLoopCount > 0)
        {
            aHeadReanim2->StartBlend(20);
            aHeadReanim2->mLoopType = ReanimLoopType::REANIM_LOOP;
            aHeadReanim2->SetFramesForLayer("anim_splitpea_idle");
            aHeadReanim2->mAnimRate = aBodyReanim->mAnimRate;
            aHeadReanim2->mAnimTime = aBodyReanim->mAnimTime;
        }

        return;
    }
    else if (mState == PlantState::STATE_CACTUS_HIGH)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            PlayBodyReanim("anim_idlehigh", ReanimLoopType::REANIM_LOOP, 20, 0.0f);

            aBodyReanim->mAnimRate = aBodyReanim->mDefinition->mFPS;
            if (mApp->IsIZombieLevel())
            {
                aBodyReanim->mAnimRate = 0.0f;
            }

            return;
        }
    }
    else if (aHeadReanim)
    {
        if (aHeadReanim->mLoopCount > 0)
        {
            aHeadReanim->StartBlend(20);
            aHeadReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
            aHeadReanim->SetFramesForLayer("anim_head_idle");
            aHeadReanim->mAnimRate = aBodyReanim->mAnimRate;
            aHeadReanim->mAnimTime = aBodyReanim->mAnimTime;
            return;
        }
    }
    else if (mSeedType == SeedType::SEED_COBCANNON)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            mState = PlantState::STATE_COBCANNON_ARMING;
            mStateCountdown = CobCannonArmFrames(mApp, 3000);
            PlayBodyReanim("anim_unarmed_idle", ReanimLoopType::REANIM_LOOP, 20, aBodyReanim->mDefinition->mFPS);
            return;
        }
    }
    else if (aBodyReanim && aBodyReanim->mLoopCount > 0)
    {
        PlayIdleAnim(aBodyReanim->mDefinition->mFPS);
        return;
    }

    mShootingCounter = 1;
}

void Plant::Animate()
{
	if ((mSeedType == SeedType::SEED_CHERRYBOMB || mSeedType == SeedType::SEED_JALAPENO) && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		mShakeOffsetX = RandRangeFloat(-1.0f, 1.0f);
		mShakeOffsetY = RandRangeFloat(-1.0f, 1.0f);
	}

	if (mRecentlyEatenCountdown > 0)
	{
		mRecentlyEatenCountdown--;
	}
	if (mEatenFlashCountdown > 0)
	{
		mEatenFlashCountdown--;
	}
	if (mBeghouledFlashCountdown > 0)
	{
		mBeghouledFlashCountdown--;
	}

	if (mSquished)
	{
		mFrame = 0;
		return;
	}

	if (mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_TALLNUT)
	{
		AnimateNuts();
	}
	else if (mSeedType == SeedType::SEED_GARLIC)
	{
		AnimateGarlic();
	}
	else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		AnimatePumpkin();
	}

	UpdateBlink();

	if (mAnimPing)
	{
		if (mAnimCounter < mFrameLength * mNumFrames - 1)
		{
			mAnimCounter++;
		}
		else
		{
			mAnimPing = false;
			mAnimCounter -= mFrameLength;
		}
	}
	else if (mAnimCounter > 0)
	{
		mAnimCounter--;
	}
	else
	{
		mAnimPing = true;
		mAnimCounter += mFrameLength;
	}
	mFrame = mAnimCounter / mFrameLength;
}

float PlantFlowerPotHeightOffset(SeedType theSeedType, float theFlowerPotScale)
{
	float aHeightOffset = -5.0f * theFlowerPotScale;
	float aScaleOffsetFix = 0.0f;

	switch (theSeedType)
	{
	case SeedType::SEED_CHOMPER:
	case SeedType::SEED_PLANTERN:
		aHeightOffset -= 5.0f;
		break;
	case SeedType::SEED_SCAREDYSHROOM:
		aHeightOffset += 5.0f;
		aScaleOffsetFix -= 8.0f;
		break;
	case SeedType::SEED_SUNSHROOM:
	case SeedType::SEED_PUFFSHROOM:
		aScaleOffsetFix -= 4.0f;
		break;
	case SeedType::SEED_HYPNOSHROOM:
	case SeedType::SEED_MAGNETSHROOM:
	case SeedType::SEED_PEASHOOTER:
	case SeedType::SEED_REPEATER:
	case SeedType::SEED_LEFTPEATER:
	case SeedType::SEED_SNOWPEA:
	case SeedType::SEED_THREEPEATER:
	case SeedType::SEED_SUNFLOWER:
	case SeedType::SEED_MARIGOLD:
	case SeedType::SEED_CABBAGEPULT:
	case SeedType::SEED_MELONPULT:
	case SeedType::SEED_TANGLEKELP:
	case SeedType::SEED_BLOVER:
	case SeedType::SEED_SPIKEWEED:
		aScaleOffsetFix -= 8.0f;
		break;
	case SeedType::SEED_SEASHROOM:
	case SeedType::SEED_POTATOMINE:
		aScaleOffsetFix -= 4.0f;
		break;
	case SeedType::SEED_LILYPAD:
		aScaleOffsetFix -= 16.0f;
		break;
	case SeedType::SEED_INSTANT_COFFEE:
		aScaleOffsetFix -= 20.0f;
		break;
	default:
		break;
	}

	return aHeightOffset + (theFlowerPotScale * aScaleOffsetFix - aScaleOffsetFix);
}

float PlantDrawHeightOffset(Board* theBoard, Plant* thePlant, SeedType theSeedType, int theCol, int theRow)
{
	float aHeightOffset = 0.0f;
	Plant* aFlowerPot = theBoard ? theBoard->GetFlowerPotAt(theCol, theRow) : nullptr;

	bool doFloating = false;
	if (Plant::IsFlying(theSeedType))
	{
		doFloating = false;
	}
	else if (theBoard == nullptr)
	{
		if (Plant::IsAquatic(theSeedType))
		{
			doFloating = true;
		}
	}
	else if (theBoard->IsPoolSquare(theCol, theRow))
	{
		doFloating = true;
	}
	else if (thePlant != nullptr && theBoard->mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		doFloating = true;
	}

	if (doFloating)
	{
		uint32_t aCounter = theBoard ? theBoard->mMainCounter : gLawnApp->mAppCounter;

		float aPos = theRow * PI + theCol * 0.25f * PI;
		float aTime = static_cast<float>(aCounter % 200) * (2.0f * PI / 200.0f);
		float aFloatingHeight = sin(aPos + aTime) * 2.0f;
		aHeightOffset += aFloatingHeight;
	}

	if (theBoard && (thePlant == nullptr || !thePlant->mSquished))
	{
		Plant* aPot = aFlowerPot;
		bool aPotBelow = true;
        if (thePlant && theSeedType == SeedType::SEED_LILYPAD && aPot)
            aPotBelow = aPot->mRenderOrder < thePlant->mRenderOrder;
        if (aPot && !aPot->mSquished && theSeedType != SeedType::SEED_FLOWERPOT && aPotBelow)
		{
			aHeightOffset += PlantFlowerPotHeightOffset(theSeedType, 1.0f);
		}
	}

	if (theSeedType == SeedType::SEED_FLOWERPOT)
    {
        aHeightOffset += 26.0f;
        if (theBoard && thePlant && theBoard->mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
        {
            PlantsOnLawn aPlants;
            theBoard->GetPlantsOnLawn(theCol, theRow, &aPlants);
            if (aPlants.mLilypadPlant && aPlants.mLilypadPlant->mRenderOrder < thePlant->mRenderOrder)
                aHeightOffset -= 5.0f;
        }
    }
	else if (theSeedType == SeedType::SEED_LILYPAD)
	{
		aHeightOffset += 25.0f;
	}
	else if (theSeedType == SeedType::SEED_STARFRUIT)
	{
		aHeightOffset += 10.0f;
	}
	else if (theSeedType == SeedType::SEED_TANGLEKELP)
	{
		aHeightOffset += 24.0f;
	}
	else if (theSeedType == SeedType::SEED_SEASHROOM)
	{
		aHeightOffset += 28.0f;
	}
	else if (theSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		aHeightOffset -= 20.0f;
	}
	//else if (Plant::IsFlying(theSeedType))
	//{
	//    aHeightOffset -= 30.0f;
	//}
	else if (theSeedType == SeedType::SEED_CACTUS)
	{
		return aHeightOffset;
	}
	else if (theSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		aHeightOffset += 15.0f;
	}
	else if (theSeedType == SeedType::SEED_PUFFSHROOM)
	{
		aHeightOffset += 5.0f;
	}
	else if (theSeedType == SeedType::SEED_SCAREDYSHROOM)
	{
		aHeightOffset -= 14.0f;
	}
	else if (theSeedType == SeedType::SEED_GRAVEBUSTER)
	{
		aHeightOffset -= 40.0f;
	}
	else if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
	{
		int aBottomRow = 4;
		if (theBoard && theBoard->StageHas6Rows())
		{
			aBottomRow = 5;
		}

		if (theSeedType == SeedType::SEED_SPIKEROCK)
		{
			aHeightOffset += 6.0f;
		}

		if (aFlowerPot && gLawnApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
		{
			aHeightOffset += 5.0f;
		}
		else if (theBoard && theBoard->StageHasRoof())
		{
			aHeightOffset += 15.0f;
		}
		else if (theBoard && theBoard->IsPoolSquare(theCol, theRow))
		{
			aHeightOffset += 0.0f;
		}
		else if (theRow == aBottomRow && theCol >= 7 && theBoard->StageHas6Rows())
		{
			aHeightOffset += 1.0f;
		}
		else if (theRow == aBottomRow && theCol < 7)
		{
			aHeightOffset += 12.0f;
		}
		else
		{
			aHeightOffset += 15.0f;
		}
	}

	return aHeightOffset;
}

void Plant::GetPeaHeadOffset(int& theOffsetX, int& theOffsetY)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

	int aTrackIndex = 0;
	if (aBodyReanim->TrackExists("anim_stem"))
	{
		aTrackIndex = aBodyReanim->FindTrackIndex("anim_stem");
	}
	else if(aBodyReanim->TrackExists("anim_idle"))
	{
		aTrackIndex = aBodyReanim->FindTrackIndex("anim_idle");
	}

	ReanimatorTransform aTransform;
	aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);
	theOffsetX = aTransform.mTransX;
	theOffsetY = aTransform.mTransY;
}

void Plant::DrawMagnetItems(Graphics* g)
{
	float aOffsetX = 0.0f;
	float aOffsetY = PlantDrawHeightOffset(mBoard, this, mSeedType, mPlantCol, mRow);

	for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
	{
		MagnetItem* aMagnetItem = &mMagnetItems[i];
		if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
		{
			int aCelRow = 0, aCelCol = 0;
			Image* aImage = nullptr;
			float aScale = 0.8f;

			if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_PAIL_1)
			{
				aImage = IMAGE_REANIM_ZOMBIE_BUCKET1;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_PAIL_2)
			{
				aImage = IMAGE_REANIM_ZOMBIE_BUCKET2;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_PAIL_3)
			{
				aImage = IMAGE_REANIM_ZOMBIE_BUCKET3;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_FOOTBALL_HELMET_1)
			{
				aImage = IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_FOOTBALL_HELMET_2)
			{
				aImage = IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET2;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_FOOTBALL_HELMET_3)
			{
				aImage = IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET3;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_DOOR_1)
			{
				aImage = IMAGE_REANIM_ZOMBIE_SCREENDOOR1;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_DOOR_2)
			{
				aImage = IMAGE_REANIM_ZOMBIE_SCREENDOOR2;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_DOOR_3)
			{
				aImage = IMAGE_REANIM_ZOMBIE_SCREENDOOR3;
			}
			else if (aMagnetItem->mItemType >= MagnetItemType::MAGNET_ITEM_POGO_1 && aMagnetItem->mItemType <= MagnetItemType::MAGNET_ITEM_POGO_3)
			{
				aCelCol = static_cast<int>(aMagnetItem->mItemType) - static_cast<int>(MagnetItemType::MAGNET_ITEM_POGO_1);
				aImage = IMAGE_ZOMBIEPOGO;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_LADDER_1)
			{
				aImage = IMAGE_REANIM_ZOMBIE_LADDER_1;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_LADDER_2)
			{
				aImage = IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE1;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_LADDER_3)
			{
				aImage = IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE2;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_LADDER_PLACED)
			{
				aImage = IMAGE_REANIM_ZOMBIE_LADDER_5;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_JACK_IN_THE_BOX)
			{
				aImage = IMAGE_REANIM_ZOMBIE_JACKBOX_BOX;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_PICK_AXE)
			{
				aImage = IMAGE_REANIM_ZOMBIE_DIGGER_PICKAXE;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_SILVER_COIN)
			{
				aScale = 1.0f;
				aImage = IMAGE_REANIM_COIN_SILVER_DOLLAR;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_GOLD_COIN)
			{
				aScale = 1.0f;
				aImage = IMAGE_REANIM_COIN_GOLD_DOLLAR;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_DIAMOND)
			{
				aScale = 1.0f;
				aImage = IMAGE_REANIM_DIAMOND;
			}
			else
			{
				PVZP_ASSERT(false);
			}

			if (aScale == 1.0f)
			{
				g->DrawImageCel(aImage, aMagnetItem->mPosX - mX + aOffsetX, aMagnetItem->mPosY - mY + aOffsetY, aCelCol, aCelRow);
			}
			else
			{
				PvzpDrawImageCelScaledF(g, aImage, aMagnetItem->mPosX - mX + aOffsetX, aMagnetItem->mPosY - mY + aOffsetY, aCelCol, aCelRow, aScale, aScale);
			}
		}
	}
}

Image* Plant::GetImage(SeedType theSeedType)
{
	Image** aImages = GetPlantDefinition(theSeedType).mPlantImage;
	return aImages ? aImages[0] : nullptr;
}

void Plant::DrawShadow(Sexy::Graphics* g, float theOffsetX, float theOffsetY)
{
	if (mSeedType == SeedType::SEED_LILYPAD || mSeedType == SeedType::SEED_STARFRUIT || mSeedType == SeedType::SEED_TANGLEKELP ||
		mSeedType == SeedType::SEED_SEASHROOM || mSeedType == SeedType::SEED_COBCANNON || mSeedType == SeedType::SEED_SPIKEWEED ||
		mSeedType == SeedType::SEED_SPIKEROCK || mSeedType == SeedType::SEED_GRAVEBUSTER || mSeedType == SeedType::SEED_CATTAIL ||
		mOnBungeeState == PlantOnBungeeState::RISING_WITH_BUNGEE)
		return;

	if (IsOnBoard() && mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && mApp->mZenGarden->mGardenType == GardenType::GARDEN_MAIN)
		return;

	int aShadowType = 0;
	float aShadowOffsetX = -3.0f;
	float aShadowOffsetY = 51.0f;
	float aScale = 1.0f;
	if (mBoard && mBoard->StageIsNight())
	{
		aShadowType = 1;
	}

	if (mSeedType == SeedType::SEED_SQUASH)
	{
		if (mBoard)
		{
			aShadowOffsetY += mBoard->GridToPixelY(mPlantCol, mRow) - mY;
		}
		aShadowOffsetY += 5.0f;
	}
	else if (mSeedType == SeedType::SEED_PUFFSHROOM)
	{
		aScale = 0.5f;
		aShadowOffsetY = 42.0f;
	}
	else if (mSeedType == SeedType::SEED_SUNSHROOM)
	{
		aShadowOffsetY = 42.0f;
		if (mState == PlantState::STATE_SUNSHROOM_SMALL)
		{
			aScale = 0.5f;
		}
		else if (mState == PlantState::STATE_SUNSHROOM_GROWING)
		{
			Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
			aScale = 0.5f + 0.5f * aBodyReanim->mAnimTime;
		}
	}
	else if (mSeedType == SeedType::SEED_UMBRELLA)
	{
		aScale = 0.5f;
		aShadowOffsetX = -7.0f;
		aShadowOffsetY = 52.0f;
	}
	else if (mSeedType == SeedType::SEED_FUMESHROOM || mSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		aScale = 1.3f;
		aShadowOffsetY = 47.0f;
	}
	else if (mSeedType == SeedType::SEED_CABBAGEPULT || mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON)
	{
		aShadowOffsetY = 47.0f;
	}
	else if (mSeedType == SeedType::SEED_KERNELPULT)
	{
		aShadowOffsetX = 0.0f;
		aShadowOffsetY = 47.0f;
	}
	else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
	{
		aShadowOffsetX = -9.0f;
		aShadowOffsetY = 55.0f;
	}
	else if (mSeedType == SeedType::SEED_CHOMPER)
	{
		aShadowOffsetX = -21.0f;
		aShadowOffsetY = 57.0f;
	}
	else if (mSeedType == SeedType::SEED_FLOWERPOT)
	{
		aShadowOffsetX = -4.0f;
		aShadowOffsetY = 46.0f;
	}
	else if (mSeedType == SeedType::SEED_TALLNUT)
	{
		aShadowOffsetY = 54.0f;
		aScale = 1.3f;
	}
	else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		aShadowOffsetY = 46.0f;
		aScale = 1.4f;
	}
	else if (mSeedType == SeedType::SEED_CACTUS)
	{
		aShadowOffsetX = -8.0f;
		aShadowOffsetY = 50.0f;
	}
	else if (mSeedType == SeedType::SEED_PLANTERN)
	{
		aShadowOffsetY = 57.0f;
	}
	else if (mSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		aShadowOffsetY = 71.0f;
	}
	else if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
	{
		aShadowOffsetX = -33.0f;
		aShadowOffsetY = 56.0f;
		aScale = 1.7f;
	}

	if (Plant::IsFlying(mSeedType))
	{
		aShadowOffsetY += 10.0f;
		if (mBoard && (mBoard->GetTopPlantAt(mPlantCol, mRow, TOPPLANT_ONLY_NORMAL_POSITION) || mBoard->GetTopPlantAt(mPlantCol, mRow, TOPPLANT_ONLY_PUMPKIN)))
			return;
	}

	if (aShadowType == 0)
	{
		PvzpDrawImageCelCenterScaledF(g, IMAGE_PLANTSHADOW, theOffsetX + aShadowOffsetX, theOffsetY + aShadowOffsetY, 0, aScale, aScale);
	}
	else
	{
		PvzpDrawImageCelCenterScaledF(g, IMAGE_PLANTSHADOW2, theOffsetX + aShadowOffsetX, theOffsetY + aShadowOffsetY, 0, aScale, aScale);
	}
}

void Plant::Draw(Graphics* g)
{
	float aOffsetX = 0.0f;
	float aOffsetY = PlantDrawHeightOffset(mBoard, this, mSeedType, mPlantCol, mRow);
	if (Plant::IsFlying(mSeedType) && mSquished)
	{
		aOffsetY += 30.0f;
	}

	int aImageIndex = mFrame;
	Image* aPlantImage = Plant::GetImage(mSeedType);

	if (mSquished)
	{
		if (mSeedType == SeedType::SEED_FLOWERPOT)
		{
			aOffsetY -= 15.0f;
		}
		if (mSeedType == SeedType::SEED_INSTANT_COFFEE)
		{
			aOffsetY -= 20.0f;
		}

		g->SetScale(1.0f, 0.25f, 0.0f, 0.0f);
		DrawSeedType(g, mSeedType, mImitaterType, DrawVariation::VARIATION_NORMAL, aOffsetX, 60.0f + aOffsetY);
		g->SetScale(1.0f, 1.0f, 0.0f, 0.0f);
	}
	else
	{
		bool aDrawPumpkinBack = false;
		Plant* aPumpkin = nullptr;

		if (IsOnBoard())
		{
			aPumpkin = mBoard->GetPumpkinAt(mPlantCol, mRow);
			if (aPumpkin)
			{
				Plant* aPlantInPumpkin = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
				if (aPlantInPumpkin)
				{
					if (aPlantInPumpkin->mRenderOrder > aPumpkin->mRenderOrder || aPlantInPumpkin->mOnBungeeState == GETTING_GRABBED_BY_BUNGEE)
					{
						aPlantInPumpkin = nullptr;
					}
				}

				if (aPlantInPumpkin == this)
				{
					aDrawPumpkinBack = true;
				}
				if (aPlantInPumpkin == nullptr && mSeedType == SeedType::SEED_PUMPKINSHELL)
				{
					aDrawPumpkinBack = true;
				}
			}
			else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
			{
				aDrawPumpkinBack = true;
				aPumpkin = this;
			}
		}
		else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			aDrawPumpkinBack = true;
			aPumpkin = this;
		}

		DrawShadow(g, aOffsetX, aOffsetY);

		if (Plant::IsFlying(mSeedType))
		{
			uint32_t aCounter = IsOnBoard() ? mBoard->mMainCounter : mApp->mAppCounter;

			float aTime = static_cast<float>(fmod((mRow * 97.0 + mPlantCol * 61.0 + static_cast<double>(aCounter)) * 0.03, 2.0 * PI));
			float aWave = sin(aTime) * 2.0f;
			aOffsetY += aWave;
		}

		if (aDrawPumpkinBack)
		{
			Reanimation* aPumpkinReanim = mApp->ReanimationGet(aPumpkin->mBodyReanimID);
			Graphics aPumpkinGraphics(*g);
			aPumpkinGraphics.mTransX += aPumpkin->mX - mX;
			aPumpkinGraphics.mTransY += aPumpkin->mY - mY;
			aPumpkinReanim->DrawRenderGroup(&aPumpkinGraphics, 1);
		}

		aOffsetX += mShakeOffsetX;
		aOffsetY += mShakeOffsetY;
		if (IsInPlay() && mApp->IsIZombieLevel())
		{
			mBoard->mChallenge->IZombieDrawPlant(g, this);
		}
		else if (mBodyReanimID != ReanimationID::REANIMATIONID_NULL)
		{
			Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
			if (aBodyReanim)
			{
				if (!mApp->Is3DAccelerated() && mSeedType == SeedType::SEED_FLOWERPOT && IsOnBoard() &&
					aBodyReanim->mAnimRate == 0.0f && aBodyReanim->IsAnimPlaying("anim_idle"))
				{
					mApp->mReanimatorCache->DrawCachedPlant(g, aOffsetX, aOffsetY, mSeedType, DrawVariation::VARIATION_NORMAL);
				}
				else
				{
					aBodyReanim->Draw(g);
				}
			}
		}
		else
		{
			SeedType aSeedType = SeedType::SEED_NONE;
			if (mBoard)
			{
				aSeedType = mBoard->GetSeedTypeInCursor();
			}

			if (IsPartOfUpgradableTo(aSeedType) && mBoard->CanPlantAt(mPlantCol, mRow, aSeedType) == PlantingReason::PLANTING_OK)
			{
				g->SetColorizeImages(true);
				g->SetColor(GetFlashingColor(mBoard->mMainCounter, 90));
			}
			else if (aSeedType == SeedType::SEED_COBCANNON && mBoard->CanPlantAt(mPlantCol - 1, mRow, aSeedType) == PlantingReason::PLANTING_OK)
			{
				g->SetColorizeImages(true);
				g->SetColor(GetFlashingColor(mBoard->mMainCounter, 90));
			}
			else if (mBoard && mBoard->mTutorialState == TutorialState::TUTORIAL_SHOVEL_DIG)
			{
				g->SetColorizeImages(true);
				g->SetColor(GetFlashingColor(mBoard->mMainCounter, 90));
			}

			PvzpDrawImageCelF(g, aPlantImage, aOffsetX, aOffsetY, aImageIndex, 0);
			g->SetColorizeImages(false);
			if (mHighlighted)
			{
				g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
				g->SetColorizeImages(true);
				g->SetColor(Color(255, 255, 255, 196));
				PvzpDrawImageCelF(g, aPlantImage, aOffsetX, aOffsetY, aImageIndex, 0);
				g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				g->SetColorizeImages(false);
			}
			else if (mEatenFlashCountdown > 0)
			{
				g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
				g->SetColorizeImages(true);
				g->SetColor(Color(255, 255, 255, std::clamp(mEatenFlashCountdown * 3, 0, 255)));
				PvzpDrawImageCelF(g, aPlantImage, aOffsetX, aOffsetY, aImageIndex, 0);
				g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				g->SetColorizeImages(false);
			}
		}

		if (mSeedType == SeedType::SEED_MAGNETSHROOM && !DrawMagnetItemsOnTop())
		{
			DrawMagnetItems(g);
		}
	}
}

void Plant::DrawSeedType(Graphics* g, SeedType theSeedType, SeedType theImitaterType, DrawVariation theDrawVariation, float thePosX, float thePosY)
{
	Graphics aSeedG(*g);
	int aCelRow = 0;
	int aCelCol = 2;
	float aOffsetX = 0.0f;
	float aOffsetY = 0.0f;
	SeedType aSeedType = theSeedType;
	DrawVariation aDrawVariation = theDrawVariation;

	if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
	{
		aSeedType = theImitaterType;
		aDrawVariation = DrawVariation::VARIATION_IMITATER;
		if (theImitaterType == SeedType::SEED_HYPNOSHROOM || theImitaterType == SeedType::SEED_SQUASH || theImitaterType == SeedType::SEED_POTATOMINE ||
			theImitaterType == SeedType::SEED_GARLIC || theImitaterType == SeedType::SEED_LILYPAD)
			aDrawVariation = DrawVariation::VARIATION_IMITATER_LESS;
	}
	else if (theDrawVariation == DrawVariation::VARIATION_NORMAL && theSeedType == SeedType::SEED_TANGLEKELP)
	{
		aDrawVariation = DrawVariation::VARIATION_AQUARIUM;
	}

	if (((LawnApp*)gSexyAppBase)->mGameMode == GameMode::GAMEMODE_CHALLENGE_BIG_TIME &&
		(aSeedType == SeedType::SEED_WALLNUT || aSeedType == SeedType::SEED_SUNFLOWER || aSeedType == SeedType::SEED_MARIGOLD))
	{
		aSeedG.mScaleX *= 1.5f;
		aSeedG.mScaleY *= 1.5f;
		aOffsetX = -20.0f;
		aOffsetY = -40.0f;
	}
	if (aSeedType == SeedType::SEED_LEFTPEATER)
	{
		aOffsetX += aSeedG.mScaleX * 80.0f;
		aSeedG.mScaleX *= -1.0f;
	}

	if (Challenge::IsZombieSeedType(aSeedType))
	{
		ZombieType aZombieType = Challenge::IZombieSeedTypeToZombieType(aSeedType);
		if (aZombieType == ZombieType::ZOMBIE_DANCER)
		{
			aSeedG.mScaleX *= 0.8f;
			aSeedG.mScaleY *= 0.8f;
			aOffsetX = 20.0f;
			aOffsetY = 42.0f;
		}
		gLawnApp->mReanimatorCache->DrawCachedZombie(&aSeedG, thePosX + aOffsetX, thePosY + aOffsetY, aZombieType);
	}
	else
	{
		const PlantDefinition& aPlantDef = GetPlantDefinition(aSeedType);

		if (aSeedType == SeedType::SEED_GIANT_WALLNUT)
		{
			aSeedG.mScaleX *= 1.4f;
			aSeedG.mScaleY *= 1.4f;
			PvzpDrawImageScaledF(&aSeedG, IMAGE_REANIM_WALLNUT_BODY, thePosX - 53.0f, thePosY - 56.0f, aSeedG.mScaleX, aSeedG.mScaleY);
		}
		else if (aPlantDef.mReanimationType != ReanimationType::REANIM_NONE)
		{
			gLawnApp->mReanimatorCache->DrawCachedPlant(&aSeedG, thePosX + aOffsetX, thePosY + aOffsetY, aSeedType, aDrawVariation);
		}
		else
		{
			if (aSeedType == SeedType::SEED_KERNELPULT)
			{
				aCelRow = 2;
			}
			else if (aSeedType == SeedType::SEED_TWINSUNFLOWER)
			{
				aCelRow = 1;
			}

			Image* aPlantImage = Plant::GetImage(aSeedType);
			if (aPlantImage->mNumCols <= 2)
			{
				aCelCol = aPlantImage->mNumCols - 1;
			}

			PvzpDrawImageCelScaledF(&aSeedG, aPlantImage, thePosX + aOffsetX, thePosY + aOffsetY, aCelCol, aCelRow, aSeedG.mScaleX, aSeedG.mScaleY);
		}
	}
}

void Plant::MouseDown(int x, int y, int theClickCount)
{
    if(mApp->AdmissionBuilding())return;
	if (theClickCount < 0)
		return;

	if (mState == PlantState::STATE_COBCANNON_READY)
	{
		mBoard->ClearCursor();
		mBoard->mCursorObject->mType = SeedType::SEED_NONE;
		mBoard->mCursorObject->mCursorType = CursorType::CURSOR_TYPE_COBCANNON_TARGET;
		mBoard->mCursorObject->mSeedBankIndex = -1;
		mBoard->mCursorObject->mCoinID = CoinID::COINID_NULL;
		mBoard->mCursorObject->mCobCannonPlantID = (PlantID)mBoard->mPlants.DataArrayGetID(this);
		mBoard->mCobCannonCursorDelayCounter = 30;
		mBoard->mCobCannonMouseX = x;
		mBoard->mCobCannonMouseY = y;
	}
}

void Plant::IceZombies()
{
	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		aZombie->HitIceTrap();
	}

	mBoard->mIceTrapCounter = 300;
	PvzpParticleSystem* aPoolSparklyParticle = mApp->ParticleTryToGet(mBoard->mPoolSparklyParticleID);
	if (aPoolSparklyParticle)
	{
		aPoolSparklyParticle->mDontUpdate = false;
	}

	Zombie* aBossZombie = mBoard->GetBossZombie();
	if (aBossZombie)
	{
		aBossZombie->BossDestroyFireball();
	}
}

void Plant::BurnRow(int theRow)
{
	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if ((aZombie->mZombieType == ZombieType::ZOMBIE_BOSS || aZombie->mRow == theRow) && aZombie->EffectedByDamage(aDamageRangeFlags))
		{
			aZombie->RemoveColdEffects();
			aZombie->ApplyBurn();
            if (mApp->RunExtraBuffCount(RUN_BUFF2_EMBERS) > 0) RunDotEffects::Ember(aZombie);
        }
	}

	for (GridItem* aGridItem : mBoard->mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridY == theRow && aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
		{
			aGridItem->GridItemDie();
		}
	}

	Zombie* aBossZombie = mBoard->GetBossZombie();
	if (aBossZombie && aBossZombie->mFireballRow == theRow)
	{
		aBossZombie->BossDestroyIceballInRow();
	}
}

void Plant::BlowAwayFliers()
{
    // @pvz-online: 单株升级「Gale」（三叶草，批 13）：吹风后全场僵尸减速 5 秒/层（至多 2 层）。
    // 语义照地刺（DoRowAreaDamage）：CanBeChilled 挡下、不缩短更长的减速（冰道/寒冰菇的
    // 2000 帧不动）、首次挂上出冰音、降速刷新走 UpdateAnimSpeed；计数型、非闯关恒 0。
    int aSlowFrames = 500 * mApp->RunPlantUpgradeCount(SeedType::SEED_BLOVER);
    // @pvz-online: 第二 buff「Gale Force」狂风（三叶草 #27，docs/06 §8.6，2★ cap2）：持条时
    // 吹风把全场僵尸推离 1 格×层（+80px/层，网格宽 80px；mPosX 为浮点可直接加）。
    // 正在啃植物的（mIsEating，啃食位置是锁存的，推走会边啃空气边掉血）与被吹走的
    //（mBlowingAway，飞行单位本来就在离场）不推；死亡动画中的由外层 IsDeadOrDying 挡。
    int aGaleForceStacks = mApp->RunPlantBuff2Count(SeedType::SEED_BLOVER);
    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        if (!aZombie->IsDeadOrDying())
        {
            // Verified as a pure function, safe to remove
            // Rect aZombieRect = aZombie->GetZombieRect();
            if (aZombie->IsFlying())
            {
                aZombie->mBlowingAway = true;
            }
            // @pvz-online: 关底 boss 免疫狂风推离（MOD_BUILD 39 批，2026-10-11 用户定案
            //「免疫狂风」）——推离是位移控制，关底 boss 都不吃；减速 5 秒（下面 aSlowFrames
            // 段）照常受 CanBeChilled 管：三种关底 boss 均吃减速，持续时间减半。
            // @pvz-online: hard immunity blocks knockback; soft duration resistance is separate below.
            if (aGaleForceStacks > 0 && !aZombie->mIsEating && !aZombie->mBlowingAway
                && !mBoard->IsRunBossZombie(aZombie) && !aZombie->mControlImmune)
            {
                Rect aBodyRect = aZombie->GetZombieRect();
                float aRightLimit = (float)(WIDE_BOARD_WIDTH - (aBodyRect.mX - aZombie->mX + aBodyRect.mWidth));
                if (aZombie->mPosX < aRightLimit)
                {
                    aZombie->mPosX = std::min(aZombie->mPosX + 80.0f * aGaleForceStacks, aRightLimit);
                    aZombie->mX = (int)aZombie->mPosX;
                }
            }
            int aTargetSlowFrames = aZombie->SoftControlDuration(aSlowFrames);
            if (aTargetSlowFrames > 0 && aZombie->CanBeChilled() && aTargetSlowFrames > aZombie->mChilledCounter)
            {
                if (aZombie->mChilledCounter == 0)
                {
                    mApp->PlayFoley(FoleyType::FOLEY_FROZEN);
                }
                aZombie->mChilledCounter = aTargetSlowFrames;
                aZombie->UpdateAnimSpeed();
            }
        }
    }

    mApp->PlaySample(SOUND_BLOVER);
    mBoard->mFogBlownCountDown = 4000;
}

void Plant::KillAllPlantsNearDoom()
{
	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow == mRow && aPlant->mPlantCol == mPlantCol)
		{
			aPlant->Die();
		}
	}
}

void Plant::DoSpecial()
{
    DamageStats::Source aDamageSource(mApp->mDamageStats, true);
    int aPosX = mX + mWidth / 2;
    int aPosY = mY + mHeight / 2;
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);

    switch (mSeedType)
    {
    case SeedType::SEED_BLOVER:
    {
        if (mState != PlantState::STATE_DOINGSPECIAL)
        {
            mState = PlantState::STATE_DOINGSPECIAL;
            BlowAwayFliers();
        }
        break;
    }
    case SeedType::SEED_CHERRYBOMB:
    {
        mApp->PlayFoley(FoleyType::FOLEY_CHERRYBOMB);
        mApp->PlayFoley(FoleyType::FOLEY_JUICY);

        // @pvz-online: 单株升级「扩爆」：半径 ×(1+50%/层)（RunBuffs 单株表；批八由 25% 上调）
        int aRadius = (int)(115 * mApp->RunPlantUpgradeMul(SeedType::SEED_CHERRYBOMB) + 0.5f);
        bool aExpandedBlast = aRadius > 115;
        if (mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, aRadius, 1, true, aDamageRangeFlags, 0, aExpandedBlast) >= 10)
            ReportAchievement::GiveAchievement(mApp, Explodonator, true);

        // @pvz-online: 第二 buff「焦土」（docs/06 §8.6 #2）：被炸中的僵尸继续燃烧——每秒
        // 300×层、共 3 秒（Update 递减块扣血）。半径判定照 KillAllZombiesInRadius 的口径
        // （无扩爆沿用上下各一行；有扩爆按完整圆形；Boss 视作同行）；厚血目标爆炸秒不掉、
        // 焦土接力；僵尸已死则不挂（mScorchedCounter 挡在 EffectedByDamage 后）。
        int aScorchStacks = mApp->RunPlantBuff2Count(SeedType::SEED_CHERRYBOMB);
        if (aScorchStacks > 0)
        {
            Zombie* aScorchZombie = nullptr;
            while (mBoard->IterateZombies(aScorchZombie))
            {
                if (!aScorchZombie->EffectedByDamage(aDamageRangeFlags))
                    continue;
                int aRowDist = aScorchZombie->mRow - mRow;
                if (aScorchZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
                    aRowDist = 0;
                if (!aExpandedBlast && (aRowDist < -1 || aRowDist > 1))
                    continue;
                Rect aScorchRect = aScorchZombie->GetZombieRect();
                if (GetCircleRectOverlap(aPosX, aPosY, aRadius, aScorchRect))
                {
                    RunDotEffects::Scorch(aScorchZombie);
                }
            }
        }

        mApp->AddPvzpParticle(aPosX, aPosY, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_POWIE);
        mBoard->ShakeBoard(3, -4);

        Die();
        break;
    }
    case SeedType::SEED_DOOMSHROOM:
    {
        mApp->PlaySample(SOUND_DOOMSHROOM);

        // @pvz-online: 单株升级「Annihilation」（批八b）：爆炸直伤 6000（只可选 1 层；
        // 2026-10-09 夜由 50000 下调——保留「无条件清场」质变、砍掉无意义超杀数）——
        // 覆写经 Board::KillAllZombiesInRadius → Zombie::ApplyBurn 传下（厚血目标吃 6000、
        // 薄血目标照旧烧死保味）；半径回原版 250（批八的半径 +50% 条目已由本条整行取代）。
        int aDirectDamage = 0;
        if (mApp->RunPlantUpgradeCount(SeedType::SEED_DOOMSHROOM) > 0)
        {
            aDirectDamage = 6000;
        }
        // @pvz-online: 第二 buff「灭世半径」（docs/06 §8.6 #15）：爆炸半径 +50%/层
        // （250 → 375/500）；波及格不变（KillAllPlantsNearDoom 只炸自身格）。
        int aDoomRadius = (int)(250 * mApp->RunPlantBuff2Mul(SeedType::SEED_DOOMSHROOM) + 0.5f);
        mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, aDoomRadius, 3, true, aDamageRangeFlags, aDirectDamage, aDoomRadius > 250);
        KillAllPlantsNearDoom();

        mApp->AddPvzpParticle(aPosX, aPosY, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_DOOM);
        // @pvz-online: clean blast keeps normal plant/base destruction, but leaves no blocking crater.
        if (mApp->RunExtraBuffCount(RUN_BUFF2_NO_CRATER) == 0)
            mBoard->AddACrater(mPlantCol, mRow)->mGridItemCounter = 18000;
        mBoard->ShakeBoard(3, -4);

        Die();
        break;
    }
    case SeedType::SEED_JALAPENO:
    {
        mApp->PlayFoley(FoleyType::FOLEY_JALAPENO_IGNITE);
        mApp->PlayFoley(FoleyType::FOLEY_JUICY);

        mBoard->DoFwoosh(mRow);
        mBoard->ShakeBoard(3, -4);

        BurnRow(mRow);
        mBoard->mIceTimer[mRow] = 20;

        // @pvz-online: 第二 buff「连烧」（docs/06 §8.6 #20）：爆炸次数 +1（间隔 1 秒）——
        // 首爆后不消失，重置自爆倒计时再烧一轮；STATE_DOINGSPECIAL 做「已连烧过」标记
        //（辣椒原本不受 mState 管辖，该值在本株只有这里读），第二轮走完才真正 Die。
        if (mApp->RunPlantBuff2Count(SeedType::SEED_JALAPENO) > 0 && mState != PlantState::STATE_DOINGSPECIAL)
        {
            mState = PlantState::STATE_DOINGSPECIAL;
            mDoSpecialCountdown = 100;
            break;
        }

        Die();
        break;
    }
    case SeedType::SEED_UMBRELLA:
    {
        if (mState != PlantState::STATE_UMBRELLA_TRIGGERED && mState != PlantState::STATE_UMBRELLA_REFLECTING)
        {
            mState = PlantState::STATE_UMBRELLA_TRIGGERED;
            mStateCountdown = 5;

            PlayBodyReanim("anim_block", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 22.0f);
        }

        break;
    }
    case SeedType::SEED_ICESHROOM:
    {
        mApp->PlayFoley(FoleyType::FOLEY_FROZEN);
        IceZombies();
        mApp->AddPvzpParticle(aPosX, aPosY, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_ICE_TRAP);

        Die();
        break;
    }
    case SeedType::SEED_POTATOMINE:
    {
        aPosX = mX + mWidth / 2 - 20;
        aPosY = mY + mHeight / 2;

        mApp->PlaySample(SOUND_POTATO_MINE);
        // @pvz-online: seismic stun is removed; descendants keep ordinary mine damage.
        if (mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, 60, 0, false, aDamageRangeFlags) >= 1)
            ReportAchievement::GiveAchievement(mApp, Spudow, true);

        int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
        mApp->AddPvzpParticle(aPosX + 20.0f, aPosY, aRenderPosition, ParticleEffect::PARTICLE_POTATO_MINE);
        mBoard->ShakeBoard(3, -4);

        // @pvz-online: free the parent square before choosing unique legal child positions.
        int aMineCol = mPlantCol, aMineRow = mRow;
        bool aSpreadMines = mApp->RunPlantBuff2Count(SeedType::SEED_POTATOMINE) > 0;
        Die();
        if (aSpreadMines)
        {
            int aCols[9], aRows[9], aCount = 0;
            for (int r = aMineRow - 1; r <= aMineRow + 1; ++r)
                for (int c = aMineCol - 1; c <= aMineCol + 1; ++c)
                    if (c >= 0 && c < MAX_GRID_SIZE_X && mBoard->RowCanHaveZombies(r) &&
                        mBoard->CanPlantAt(c, r, SeedType::SEED_POTATOMINE) == PlantingReason::PLANTING_OK)
                    {
                        aCols[aCount] = c;
                        aRows[aCount++] = r;
                    }
            for (int i = 0, n = std::min(3, aCount); i < n && mBoard->mPlants.mSize < mBoard->mPlants.mMaxSize; ++i)
            {
                int aPick = Rand(aCount);
                mBoard->AddPlant(aCols[aPick], aRows[aPick], SeedType::SEED_POTATOMINE, SeedType::SEED_NONE);
                --aCount;
                aCols[aPick] = aCols[aCount];
                aRows[aPick] = aRows[aCount];
            }
        }
        break;
    }
    case SeedType::SEED_INSTANT_COFFEE:
    {
        if (mApp->IsRunMode())
        {
            // @pvz-online: 闯关里蘑菇不会再睡，"唤醒"永远没有对象——咖啡豆改产阳光：
            // 四枚 25 = 100（2026-10-03 平调二版：先试过三枚=75，用户定"未增强的不能太废"，
            // 基座回 100；同批种植冷却加长到 12 秒），和向日葵同一套落币方式
            // @pvz-online: 单株升级「Rich Roast」（咖啡豆，表行 SEED_INSTANT_COFFEE）：
            // 产阳光每层 +25（+1 枚）——4+层：1 层 5 枚 = 125、2 层 6 枚 = 150
            //（2026-10-03 三版终值：用户定 100/125/150；二版曾按 ×(1+层) = 200/300 过陡）。
            // 冷却侧 ×1.6（SeedPacket::WasPlanted × LawnApp::RunCoffeeBeanRefreshMul）。
            int aSunCoinCount = 4 + mApp->RunPlantUpgradeCount(SeedType::SEED_INSTANT_COFFEE);
            for (int i = 0; i < aSunCoinCount; i++)
            {
                mBoard->AddCoin(mX, mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
            }

            mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
        }
        else
        {
            Plant* aPlant = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
            if (aPlant && aPlant->mIsAsleep)
            {
                aPlant->mWakeUpCounter = 100;
            }
        }

        mState = PlantState::STATE_DOINGSPECIAL;
        PlayBodyReanim("anim_crumble", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 22.0f);
        mApp->PlayFoley(FoleyType::FOLEY_COFFEE);

        break;
    }
    default:
        break;
    }
}

void Plant::ImitaterMorph()
{
    if (!mBoard->PreparePlantAnimations(mImitaterType)) return;
	Die();
	Plant* aPlant = mBoard->AddPlant(mPlantCol, mRow, mImitaterType, SeedType::SEED_IMITATER);
    if (!aPlant) return;

	FilterEffect aFilter = FilterEffect::FILTER_EFFECT_WASHED_OUT;
	if (mImitaterType == SeedType::SEED_HYPNOSHROOM || mImitaterType == SeedType::SEED_SQUASH || mImitaterType == SeedType::SEED_POTATOMINE ||
		mImitaterType == SeedType::SEED_GARLIC || mImitaterType == SeedType::SEED_LILYPAD)
		aFilter = FilterEffect::FILTER_EFFECT_LESS_WASHED_OUT;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(aPlant->mBodyReanimID);
	if (aBodyReanim)
	{
		aBodyReanim->mFilterEffect = aFilter;
	}
	Reanimation* aHeadReanim = mApp->ReanimationTryToGet(aPlant->mHeadReanimID);
	if (aHeadReanim)
	{
		aHeadReanim->mFilterEffect = aFilter;
	}Reanimation* aHeadReanim2 = mApp->ReanimationTryToGet(aPlant->mHeadReanimID2);
	if (aHeadReanim2)
	{
		aHeadReanim2->mFilterEffect = aFilter;
	}Reanimation* aHeadReanim3 = mApp->ReanimationTryToGet(aPlant->mHeadReanimID3);
	if (aHeadReanim3)
	{
		aHeadReanim3->mFilterEffect = aFilter;
	}
}

void Plant::UpdateImitater()
{
	if (mState != PlantState::STATE_IMITATER_MORPHING)
	{
		if (mStateCountdown == 0)
		{
			mState = PlantState::STATE_IMITATER_MORPHING;
			PlayBodyReanim("anim_explode", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 26.0f);
		}
	}
	else
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.8f))
		{
			mApp->AddPvzpParticle(mX + 40, mY + 40, static_cast<int>(RenderLayer::RENDER_LAYER_TOP), ParticleEffect::PARTICLE_IMITATER_MORPH);
		}
		if (aBodyReanim->mLoopCount > 0)
		{
			ImitaterMorph();
		}
	}
}

void Plant::CobCannonFire(int theTargetX, int theTargetY)
{
	PVZP_ASSERT(mState == PlantState::STATE_COBCANNON_READY);

	mState = PlantState::STATE_COBCANNON_FIRING;
	mShootingCounter = 206;
	PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);

	mTargetX = theTargetX - 47.0f;
	mTargetY = theTargetY;

	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("CobCannon_Cob");
	aTrackInstance->mTrackColor = Color::White;
}

void Plant::Fire(Zombie* theTargetZombie, int theRow, PlantWeapon thePlantWeapon)
{
    // @pvz-online: route existing volleys, rather than manufacturing extra shots.
    if (mSeedType == SeedType::SEED_THREEPEATER && mApp->RunExtraBuffCount(RUN_BUFF2_DISPATCH) > 0 &&
        !FindTargetZombie(theRow, PlantWeapon::WEAPON_PRIMARY))
    {
        int aActiveRows[3], aCount = 0;
        for (int r = mRow - 1; r <= mRow + 1; ++r)
            if (mBoard->RowCanHaveZombies(r) && FindTargetZombie(r, PlantWeapon::WEAPON_PRIMARY))
                aActiveRows[aCount++] = r;
        if (aCount > 0)
        {
            theRow = aActiveRows[mRunDispatchCursor % aCount];
            mRunDispatchCursor = (mRunDispatchCursor + 1) % aCount;
        }
    }
    bool aBackFiresForward = mSeedType == SeedType::SEED_SPLITPEA &&
        thePlantWeapon == PlantWeapon::WEAPON_SECONDARY && mApp->RunExtraBuffCount(RUN_BUFF2_FORWARD_BACK) > 0 &&
        !FindTargetZombie(mRow, PlantWeapon::WEAPON_SECONDARY);

    if (mSeedType == SeedType::SEED_FUMESHROOM)
    {
        DoRowAreaDamage(20, 2U);
        mApp->PlayFoley(FoleyType::FOLEY_FUME);
        return;
    }
    if (mSeedType == SeedType::SEED_GLOOMSHROOM)
    {
        // @pvz-online: 「emotional damage」（2026-10-15）：孤高（周围 8 格无植物）时
        // 每发雾爆 20 → 100（×5，cap1 质变条；2026-10-17 加强）；有植物或没词条走原值。
        int aGloomDamage = 20;
        if (mApp->RunExtraBuffCount(RUN_BUFF2_EMO_DAMAGE) > 0 && GloomIsLonely(this))
        {
            aGloomDamage = 100;
        }
        DoRowAreaDamage(aGloomDamage, 2U);
        return;
    }
    if (mSeedType == SeedType::SEED_STARFRUIT)
    {
        StarFruitFire();
        return;
    }

    ProjectileType aProjectileType;
    switch (mSeedType)
    {
    case SeedType::SEED_PEASHOOTER:
    case SeedType::SEED_REPEATER:
    case SeedType::SEED_THREEPEATER:
    case SeedType::SEED_SPLITPEA:
    case SeedType::SEED_GATLINGPEA:
    case SeedType::SEED_LEFTPEATER:
        aProjectileType = ProjectileType::PROJECTILE_PEA;
        break;
    case SeedType::SEED_SNOWPEA:
        aProjectileType = ProjectileType::PROJECTILE_SNOWPEA;
        break;
    case SeedType::SEED_PUFFSHROOM:
    case SeedType::SEED_SCAREDYSHROOM:
    case SeedType::SEED_SEASHROOM:
        aProjectileType = ProjectileType::PROJECTILE_PUFF;
        break;
    case SeedType::SEED_CACTUS:
    case SeedType::SEED_CATTAIL:
        aProjectileType = ProjectileType::PROJECTILE_SPIKE;
        break;
    case SeedType::SEED_CABBAGEPULT:
        aProjectileType = ProjectileType::PROJECTILE_CABBAGE;
        break;
    case SeedType::SEED_KERNELPULT:
        aProjectileType = ProjectileType::PROJECTILE_KERNEL;
        break;
    case SeedType::SEED_MELONPULT:
        aProjectileType = ProjectileType::PROJECTILE_MELON;
        break;
    case SeedType::SEED_WINTERMELON:
        aProjectileType = ProjectileType::PROJECTILE_WINTERMELON;
        break;
    case SeedType::SEED_COBCANNON:
        aProjectileType = ProjectileType::PROJECTILE_COBBIG;
        break;
    default:
        PVZP_ASSERT();
        break;
    }
    if (mSeedType == SeedType::SEED_KERNELPULT && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
    {
        aProjectileType = ProjectileType::PROJECTILE_BUTTER;
    }

    // @pvz-online: 批 18 寒冰射手「Blizzard」——发射冰西瓜（1 层）：出口弹种整换成冰瓜弹
    // （80 伤 + 冰冻 + 溅射全按弹种自带；枪口雪粒子与 FOLEY_SNOW_PEA_SPARKLES 保留；
    // 直线飞行吃 UpdateNormalMotion 的兜底速度，无需设初速）。
    if (mSeedType == SeedType::SEED_SNOWPEA && mApp->RunPlantUpgradeCount(SeedType::SEED_SNOWPEA) > 0)
    {
        aProjectileType = ProjectileType::PROJECTILE_WINTERMELON;
    }

    mApp->PlayFoley(FoleyType::FOLEY_THROW);
    if (mSeedType == SeedType::SEED_SNOWPEA || mSeedType == SeedType::SEED_WINTERMELON)
    {
        mApp->PlayFoley(FoleyType::FOLEY_SNOW_PEA_SPARKLES);
    }
    else if (mSeedType == SeedType::SEED_PUFFSHROOM || mSeedType == SeedType::SEED_SCAREDYSHROOM || mSeedType == SeedType::SEED_SEASHROOM)
    {
        mApp->PlayFoley(FoleyType::FOLEY_PUFF);
    }

    int aOriginX, aOriginY;
    if (mSeedType == SeedType::SEED_PUFFSHROOM)
    {
        aOriginX = mX + 40;
        aOriginY = mY + 40;
    }
    else if (mSeedType == SeedType::SEED_SEASHROOM)
    {
        aOriginX = mX + 45;
        aOriginY = mY + 63;
    }
    else if (mSeedType == SeedType::SEED_CABBAGEPULT)
    {
        aOriginX = mX + 5;
        aOriginY = mY - 12;
    }
    else if (mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON)
    {
        aOriginX = mX + 25;
        aOriginY = mY - 46;
    }
    else if (mSeedType == SeedType::SEED_CATTAIL)
    {
        aOriginX = mX + 20;
        aOriginY = mY - 3;
    }
    else if (mSeedType == SeedType::SEED_KERNELPULT && thePlantWeapon == PlantWeapon::WEAPON_PRIMARY)
    {
        aOriginX = mX + 19;
        aOriginY = mY - 37;
    }
    else if (mSeedType == SeedType::SEED_KERNELPULT && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
    {
        aOriginX = mX + 12;
        aOriginY = mY - 56;
    }
    else if (mSeedType == SeedType::SEED_PEASHOOTER || mSeedType == SeedType::SEED_SNOWPEA || mSeedType == SeedType::SEED_REPEATER)
    {
        int aOffsetX, aOffsetY;
        GetPeaHeadOffset(aOffsetX, aOffsetY);
        aOriginX = mX + aOffsetX + 24;
        aOriginY = mY + aOffsetY - 33;
    }
    else if (mSeedType == SeedType::SEED_LEFTPEATER)
    {
        int aOffsetX, aOffsetY;
        GetPeaHeadOffset(aOffsetX, aOffsetY);
        aOriginX = mX + aOffsetX + 27;
        aOriginY = mY + aOffsetY - 33;
    }
    else if (mSeedType == SeedType::SEED_GATLINGPEA)
    {
        int aOffsetX, aOffsetY;
        GetPeaHeadOffset(aOffsetX, aOffsetY);
        aOriginX = mX + aOffsetX + 34;
        aOriginY = mY + aOffsetY - 33;
    }
    else if (mSeedType == SeedType::SEED_SPLITPEA)
    {
        int aOffsetX, aOffsetY;
        GetPeaHeadOffset(aOffsetX, aOffsetY);
        aOriginY = mY + aOffsetY - 33;

        if (thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
        {
            aOriginX = mX + aOffsetX + (aBackFiresForward ? 24 : -64);
        }
        else
        {
            aOriginX = mX + aOffsetX + 24;
        }
    }
    else if (mSeedType == SeedType::SEED_THREEPEATER)
    {
        aOriginX = mX + 45;
        aOriginY = mY + 10;
    }
    else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
    {
        aOriginX = mX + 29;
        aOriginY = mY + 21;
    }
    else if (mSeedType == SeedType::SEED_CACTUS)
    {
        if (thePlantWeapon == PlantWeapon::WEAPON_PRIMARY)
        {
            aOriginX = mX + 93;
            aOriginY = mY - 50;
        }
        else
        {
            aOriginX = mX + 70;
            aOriginY = mY + 23;
        }
    }
    else if (mSeedType == SeedType::SEED_COBCANNON)
    {
        aOriginX = mX - 44;
        aOriginY = mY - 184;
    }
    else
    {
        aOriginX = mX + 10;
        aOriginY = mY + 5;
    }
    if (mBoard->GetFlowerPotAt(mPlantCol, mRow))
    {
        aOriginY -= 5;
    }

    if (mSeedType == SeedType::SEED_SNOWPEA)
    {
        int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, mRow, 1);
        mApp->AddPvzpParticle(aOriginX + 8, aOriginY + 13, aRenderPosition, ParticleEffect::PARTICLE_SNOWPEA_PUFF);
    }
    else if (mSeedType == SeedType::SEED_PUFFSHROOM)
    {
        int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, mRow, 1);
        mApp->AddPvzpParticle(aOriginX + 18, aOriginY + 13, aRenderPosition, ParticleEffect::PARTICLE_PUFFSHROOM_MUZZLE);
    }
    else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
    {
        int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, mRow, 1);
        mApp->AddPvzpParticle(aOriginX + 27, aOriginY + 13, aRenderPosition, ParticleEffect::PARTICLE_PUFFSHROOM_MUZZLE);
    }

    Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder - 1, theRow, aProjectileType);
    if (aProjectile == nullptr) return;
    aProjectile->mDamageRangeFlags = GetDamageRangeFlags(thePlantWeapon);
    aProjectile->mRunSourceSeed = mSeedType;
    aProjectile->mRunIceSource = mSeedType == SeedType::SEED_SNOWPEA;
    aProjectile->mRunCactusSource = mSeedType == SeedType::SEED_CACTUS;
    aProjectile->mRunScaredySource = mSeedType == SeedType::SEED_SCAREDYSHROOM;
    if (mSeedType == SeedType::SEED_WINTERMELON && mApp->RunPlantBuff2Count(mSeedType) > 0 && (mRunPurpleShots = (mRunPurpleShots + 1) % 3) == 0 &&
        mBoard->mProjectiles.mSize < mBoard->mProjectiles.mMaxSize)
    {
        Projectile* wave = mBoard->AddProjectile(mX + 40, mY + 30, mRenderOrder + 1, mRow, ProjectileType::PROJECTILE_WINTERMELON);
        if (wave)
        {
            wave->mRunIceWave = true;
            wave->mRunSourceSeed = mSeedType;
            wave->mDamageRangeFlags = aProjectile->mDamageRangeFlags;
        }
    }


    // @pvz-online: 单株升级「Prickly」（仙人掌，表行 SEED_CACTUS）：有该条就给尖刺预置
    // 无限穿透标记 -1（2026-10-09 晚实机反馈：由 +1 只/层、cap 2 改为无限穿透、cap 1）——
    // 只有仙人掌的弹带这个数（猫尾草的同型尖刺是追踪弹，不受影响）；无条与原版一致。
    // 命中后的判定/扣点在 Projectile::DoImpact 末尾（-1 永不扣）。
    if (mSeedType == SeedType::SEED_CACTUS
        && mApp->RunPlantUpgradeCount(SeedType::SEED_CACTUS) > 0)
    {
        aProjectile->mPricklyHitsLeft = -1;
    }

    // @pvz-online: 第二 buff 批 1「穿透族」（docs/06 §8.6 #0/#5/#18）：豌豆「穿刺豌豆」/
    // 寒冰射手「寒冰贯通」（与老条「冰西瓜化」同株互斥，抽取端管）/ 三线射手「贯穿」——
    // 直射弹带无限穿透标记 -1（复用仙人掌 Prickly 的标记与 DoImpact 推过语义，弹型无关）。
    // 评审口径：基弱株穿透类直接无限穿透、cap 收 1。
    if ((mSeedType == SeedType::SEED_PEASHOOTER || mSeedType == SeedType::SEED_SNOWPEA ||
        mSeedType == SeedType::SEED_THREEPEATER)
        && mApp->RunPlantBuff2Count(mSeedType) > 0)
    {
        aProjectile->mPricklyHitsLeft = -1;
    }

    // @pvz-online: each Gatling bullet independently becomes a straight melon; no extra shot.
    if (mSeedType == SeedType::SEED_GATLINGPEA && mApp->RunPlantBuff2Count(mSeedType) > 0 && Sexy::Rand(100) < 25)
        aProjectile->mProjectileType = ProjectileType::PROJECTILE_MELON;

    // @pvz-online: 第二 buff「Tri-Spike」三向尖刺（仙人掌 #26，docs/06 §8.6，2★ cap1）：持条时
    // 本行上下各补一发同型尖刺——弹体从仙人掌出发、走三线射手同款斜向运动（MOTION_THREEPEATER）
    // 飘向相邻行；该行放不了僵尸（池面/出界/屋顶边列）就不补（RowCanHaveZombies 闸门，同三线
    // 选敌）。伤害标志与穿透标记照抄主弹——高射态补高刺（可打气球）、低射态补地刺，老条
    // 「Prickly」无限穿透随主弹一起复制。
    // 2026-10-10 实机修复（用户报「仍然只打两发」；§5.67 曾误判为索敌问题）：影子补偿原照抄
    // 三线的固定 ±80，但三线炮口贴在行线上（mY+offset−33），仙人掌低射炮口在行线下 23px
    //（mY+23）、前院行距却是 100（GridToPixelY：前院 row*100、泳池/屋顶 row*85）——上发出生
    // 影差 = 67+80−100−23 = 24 < 28，撞上 CheckForHighGround 的「贴地」闸门（Projectile.cpp，
    // SPIKE 在列；本意是屋顶斜坡弹撞地）在第一个更新帧就被 DoImpact(nullptr) 收掉，上发永远
    // 飞不出去（下发 64、主弹 44 都活——正是「只打两发/只向下分裂」的观感）。现按实际行距
    // 补偿（同列 ΔGridToPixelY，屋顶斜坡项自消）：两发出生影差恒等于主弹（低射 44 / 高射
    // 117），任何地形都成活、落点影子线与相邻行对齐；弹速维持三线同款不动。
    if (mSeedType == SeedType::SEED_CACTUS && mApp->RunPlantBuff2Count(SeedType::SEED_CACTUS) > 0)
    {
        for (int aSideRow = mRow - 1; aSideRow <= mRow + 1; aSideRow += 2)
        {
            if (!mBoard->RowCanHaveZombies(aSideRow))
            {
                continue;
            }
            Projectile* aSideProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder - 1, aSideRow, aProjectileType);
            if (aSideProjectile == nullptr) continue;
            aSideProjectile->mDamageRangeFlags = aProjectile->mDamageRangeFlags;
            aSideProjectile->mRunCactusSource = true;
            aSideProjectile->mPricklyHitsLeft = aProjectile->mPricklyHitsLeft;
            aSideProjectile->mMotionType = ProjectileMotion::MOTION_THREEPEATER;
            aSideProjectile->mVelY = (aSideRow < mRow) ? -3.0f : 3.0f;
            aSideProjectile->mShadowY += (float)(mBoard->GridToPixelY(mPlantCol, mRow) - mBoard->GridToPixelY(mPlantCol, aSideRow));
        }
    }

    // @pvz-online: 单株升级「多发」：每层多打一发（RunBuffs 单株表）。
    // 表里进得了这段的是直射豌豆系（豌豆、三线——三线每道调一次 Fire，所以是每道各 +1 颗，
    // 不是每轮 +3）与小喷菇——多出来的子弹照主子弹的
    // 默认直线运动走，出发位置逐发后错一点，看得出是一排子弹而不是一发重影。
    // 修正批（2026-10-03）改走 Kind 闸门口（Kind=SHOTCOUNT）：只有多发型条目进得来——
    // 此前任意带层行都会给该株多发（批 10 的黄油行差点让玉米投手多发玉米）。
    // （2026-10-09 数值对齐：小喷菇 +2 颗/层的 ×2 换算删除，回 +1 颗/层，docs/07 同日条目；
    //  2026-10-09 夜实机反馈又改回 +2 颗/层——见下方的层数 ×2 特判。）
    int aExtraShots = mApp->RunPlantUpgradeCountKind(mSeedType, RUN_UPGRADE_KIND_SHOTCOUNT);
    // 小喷菇「Spore Volley」按「+2 颗/层」口径：层数 ×2。它每轮只调一次 Fire，取不到双发
    // 那样「每轮两次 Fire 各跑一遍多发」的自然 2×（双发行 desc 的「+2 颗/层」即由此而来，
    // 无需特判）；豌豆/三线仍 +1 颗/层，不受影响。
    if (mSeedType == SeedType::SEED_PUFFSHROOM)
    {
        aExtraShots *= 2;
    }
    for (int i = 0; i < aExtraShots; i++)
    {
        // Unbounded Threepeater stacks stay near the muzzle instead of spawning beyond the board.
        const int extraOffset = 21 * (mSeedType == SeedType::SEED_THREEPEATER ? 1 + i % 4 : i + 1);
        Projectile* aExtraProjectile = mBoard->AddProjectile(aOriginX + extraOffset, aOriginY, mRenderOrder - 1, theRow, aProjectileType);
        if (aExtraProjectile == nullptr) break; // No slot can free during this volley; retain unlimited trait stacks.
        aExtraProjectile->mDamageRangeFlags = aProjectile->mDamageRangeFlags;
        if (mBoard->StageHasRoof() && mApp->RunExtraBuffCount(RUN_BUFF2_ROOF_PEAS) > 0 &&
            (aProjectileType == ProjectileType::PROJECTILE_PEA || aProjectileType == ProjectileType::PROJECTILE_SNOWPEA || aProjectileType == ProjectileType::PROJECTILE_FIREBALL)) {
            const float slope = mBoard->GetPosYBasedOnRow(aExtraProjectile->mPosX, theRow) - mBoard->GetPosYBasedOnRow(aProjectile->mPosX, theRow);
            aExtraProjectile->mPosY += slope;
            aExtraProjectile->mY = static_cast<int>(aExtraProjectile->mPosY);
            aExtraProjectile->mShadowY = aProjectile->mShadowY + slope;
        }
        aExtraProjectile->mRunSourceSeed = mSeedType;
        aExtraProjectile->mRunIceSource = aProjectile->mRunIceSource;
        aExtraProjectile->mRunCactusSource = aProjectile->mRunCactusSource;
        aExtraProjectile->mPricklyHitsLeft = aProjectile->mPricklyHitsLeft;	// 穿透族：多发复制的子弹同标记（第二 buff 批 1）
        // @pvz-online: 三线多发补斜向（2026-10-10）：三线斜向三件套挂在 Fire 尾部（下段
        // SEED_THREEPEATER 分支），多发弹生成时还没挂——旧版相邻行多发弹是 MOTION_STRAIGHT
        // + mRow=相邻行的「叠画在本行、谁也打不着」废弹（实机反馈「三线加子弹数只加中间的」）。
        // 相邻行的多发弹补与主弹逐参数一致的斜向运动（同 TRISPIKE 侧弹写法）。
        if (mSeedType == SeedType::SEED_THREEPEATER && theRow != mRow)
        {
            aExtraProjectile->mMotionType = ProjectileMotion::MOTION_THREEPEATER;
            aExtraProjectile->mVelY = (theRow < mRow) ? -3.0f : 3.0f;
            aExtraProjectile->mShadowY += (float)(mBoard->GridToPixelY(mPlantCol, mRow) - mBoard->GridToPixelY(mPlantCol, theRow));
        }
    }

    if (mSeedType == SeedType::SEED_CABBAGEPULT || mSeedType == SeedType::SEED_KERNELPULT ||
        mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON)
    {
        float aRangeX, aRangeY;
        if (theTargetZombie)
        {
            Rect aZombieRect = theTargetZombie->GetZombieRect();
            aRangeX = theTargetZombie->ZombieTargetLeadX(50.0f) - aOriginX - 30.0f;
            aRangeY = aZombieRect.mY - aOriginY;

            if (theTargetZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING)
            {
                aRangeX -= 60.0f;
            }
            if (theTargetZombie->mZombieType == ZombieType::ZOMBIE_POGO && theTargetZombie->mHasObject)
            {
                aRangeX -= 60.0f;
            }
            if (theTargetZombie->mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
            {
                aRangeX -= 40.0f;
            }
            if (theTargetZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
            {
                aRangeY = mBoard->GridToPixelY(8, mRow) - aOriginY;
            }
        }
        else
        {
            aRangeX = 700.0f - aOriginX;
            aRangeY = 0.0f;
        }
        if (aRangeX < 40.0f)
        {
            aRangeX = 40.0f;
        }

        aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
        aProjectile->mVelX = aRangeX / 120.0f;
        aProjectile->mVelY = 0.0f;
        aProjectile->mVelZ = aRangeY / 120.0f - 7.0f;
        aProjectile->mAccZ = 0.115f;

        // @pvz-online: 批 18 西瓜「Melon Barrage」——每次多发 1 个西瓜/层（无上限；用户定案）。
        // 额外瓜照主弹的完整参数出发（同速同弧线、位置向右后错开 21px），落点因此与主弹
        // 基本重合。行上留 EFFECT 不进多发通环：那边按直射弹写法补弹，投手弹会变呆弹。
        if (mSeedType == SeedType::SEED_MELONPULT)
        {
            for (int i = 0, aExtra = mApp->RunPlantUpgradeCount(SeedType::SEED_MELONPULT); i < aExtra; i++)
            {
                Projectile* aExtraProjectile = mBoard->AddProjectile(aOriginX + 21 * (i + 1), aOriginY, mRenderOrder - 1, theRow, aProjectileType);
                if (aExtraProjectile == nullptr) break;
                aExtraProjectile->mDamageRangeFlags = aProjectile->mDamageRangeFlags;
                aExtraProjectile->mRunSourceSeed = mSeedType;
                aExtraProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
                aExtraProjectile->mVelX = aRangeX / 120.0f;
                aExtraProjectile->mVelY = 0.0f;
                aExtraProjectile->mVelZ = aRangeY / 120.0f - 7.0f;
                aExtraProjectile->mAccZ = 0.115f;
            }
        }
    }
    else if (mSeedType == SeedType::SEED_THREEPEATER)
    {
        if (theRow < mRow)
        {
            aProjectile->mMotionType = ProjectileMotion::MOTION_THREEPEATER;
            aProjectile->mVelY = -3.0f;
            aProjectile->mShadowY += (float)(mBoard->GridToPixelY(mPlantCol, mRow) - mBoard->GridToPixelY(mPlantCol, theRow));
        }
        else if (theRow > mRow)
        {
            aProjectile->mMotionType = ProjectileMotion::MOTION_THREEPEATER;
            aProjectile->mVelY = 3.0f;
            aProjectile->mShadowY += (float)(mBoard->GridToPixelY(mPlantCol, mRow) - mBoard->GridToPixelY(mPlantCol, theRow));
        }
    }
    else if (mSeedType == SeedType::SEED_PUFFSHROOM || mSeedType == SeedType::SEED_SEASHROOM)
    {
        aProjectile->mMotionType = ProjectileMotion::MOTION_PUFF;
    }
    else if (mSeedType == SeedType::SEED_SPLITPEA && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
    {
        aProjectile->mMotionType = (aBackFiresForward ? ProjectileMotion::MOTION_STRAIGHT : ProjectileMotion::MOTION_BACKWARDS);

        // @pvz-online: 单株升级「Backspike」（分裂豌豆，表行 SEED_SPLITPEA）：背向豌豆每次
        // +1 颗/层（计数型、至多 2 层）——镜像正面多发循环的写法，多出的弹向左后错开、
        // 同样走 MOTION_BACKWARDS。正面的多发循环只认 SHOTCOUNT 标签，与本条互不影响。
        for (int i = 0, aExtra = mApp->RunPlantUpgradeCount(SeedType::SEED_SPLITPEA); i < aExtra; i++)
        {
            Projectile* aExtraProjectile = mBoard->AddProjectile(aOriginX + (aBackFiresForward ? 21 : -21) * (i + 1), aOriginY, mRenderOrder - 1, theRow, aProjectileType);
            if (aExtraProjectile == nullptr) continue;
            aExtraProjectile->mDamageRangeFlags = aProjectile->mDamageRangeFlags;
            aExtraProjectile->mRunSourceSeed = mSeedType;
            aExtraProjectile->mMotionType = (aBackFiresForward ? ProjectileMotion::MOTION_STRAIGHT : ProjectileMotion::MOTION_BACKWARDS);
        }
    }
    else if (mSeedType == SeedType::SEED_LEFTPEATER)
    {
        aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
    }
    else if (mSeedType == SeedType::SEED_CATTAIL)
    {
        aProjectile->mVelX = 2.0f;
        aProjectile->mMotionType = ProjectileMotion::MOTION_HOMING;
        aProjectile->mTargetZombieID = mBoard->ZombieGetID(theTargetZombie);
        if (mApp->RunExtraBuffCount(RUN_BUFF2_HUNT) > 0 && (mRunPurpleShots = (mRunPurpleShots + 1) % 4) == 0)
        {
            aProjectile->mRunHuntHitsLeft = 3;
            aProjectile->mRunCrystalDamage = 80;
        }
    }
    else if (mSeedType == SeedType::SEED_COBCANNON)
    {
        aProjectile->mVelX = 0.001f;
        aProjectile->mDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
        aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
        aProjectile->mVelY = 0.0f;
        aProjectile->mAccZ = 0.0f;
        aProjectile->mVelZ = -8.0f;
        aProjectile->mCobTargetX = mTargetX - 40;
        aProjectile->mCobTargetRow = mBoard->PixelToGridYKeepOnBoard(mTargetX, mTargetY);

        // @pvz-online: five ordinary cob blasts, staggered by 0.2 sec; replaces Twin Cob.
        if (mApp->RunPlantBuff2Count(SeedType::SEED_COBCANNON) > 0)
        {
            const int dx[4] = { 0, 0, -80, 80 };
            const int dr[4] = { -1, 1, 0, 0 };
            for (int i = 0; i < 4 && mBoard->mProjectiles.mSize < mBoard->mProjectiles.mMaxSize; ++i)
            {
                int row = aProjectile->mCobTargetRow + dr[i];
                if (!mBoard->RowCanHaveZombies(row)) continue;
                Projectile* p = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder - 1, theRow, aProjectileType);
                if (p == nullptr) continue;
                p->mVelX = 0.001f;
                p->mDamageRangeFlags = aProjectile->mDamageRangeFlags;
                p->mMotionType = ProjectileMotion::MOTION_LOBBED;
                p->mVelZ = -8.0f;
                p->mCobTargetX = std::max(0.0f, std::min(760.0f, aProjectile->mCobTargetX + dx[i]));
                p->mCobTargetRow = row;
                p->mRunLaunchDelay = (i + 1) * 20;
                p->mRunSourceSeed = mSeedType;
            }
        }
    }
}

// @pvz-online: native collection animation, no duplicate coins or stacked amplification.
void Plant::UpdatePurpleBuffs()
{
    if (mSeedType != SeedType::SEED_TWINSUNFLOWER || !mApp->IsRunMode()) return;
    if (++mRunSunCollectTicks < 500) return;
    mRunSunCollectTicks = 0;
    Coin* coin = nullptr;
    while (mBoard->IterateCoins(coin))
    {
        if (!coin->IsSun() || coin->mDead || coin->mVisualOnly || coin->mIsBeingCollected) continue;
        if (mApp->RunPlantBuff2Count(mSeedType) > 0) coin->mRunSunBonus = coin->GetSunValue() / 5;
        coin->Collect();
    }
}

Zombie* Plant::FindTargetZombie(int theRow, PlantWeapon thePlantWeapon, Zombie* theExclude1, Zombie* theExclude2)
{
    int aDamageRangeFlags = GetDamageRangeFlags(thePlantWeapon);
    Rect aAttackRect = GetPlantAttackRect(thePlantWeapon);
    const int aChomperChains = mSeedType == SeedType::SEED_CHOMPER ?
        2 * mApp->RunPlantBuff2Count(SeedType::SEED_CHOMPER) : 0;
    const bool aChomperChainRange = aChomperChains > 0 &&
        ((mState == PlantState::STATE_CHOMPER_BITING_GOT_ONE && mChainBitesLeft > 0) ||
         (mState == PlantState::STATE_CHOMPER_BITING && mChainBitesLeft < aChomperChains));
    if (aChomperChainRange) aAttackRect.mWidth = 160;
    int aHighestWeight = 0;
    Zombie* aBestZombie = nullptr;

    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        // @pvz-online: 单株升级「Quick Claw」（猫尾草）多目标选敌时排除已选中的（默认无排除）。
        if (aZombie == theExclude1 || aZombie == theExclude2)
        {
            continue;
        }

        int aRowDeviation = aZombie->mRow - theRow;
        if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
        {
            aRowDeviation = 0;
        }

        if (!aZombie->mHasHead || aZombie->IsTangleKelpTarget())
        {
            if (mSeedType == SeedType::SEED_POTATOMINE || mSeedType == SeedType::SEED_CHOMPER || mSeedType == SeedType::SEED_TANGLEKELP)
            {
                continue;
            }
        }

        bool needPortalCheck = false;
        if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT)
        {
            if (mSeedType == SeedType::SEED_PEASHOOTER || mSeedType == SeedType::SEED_CACTUS || mSeedType == SeedType::SEED_REPEATER)
            {
                needPortalCheck = true;
            }
        }

        if (mSeedType != SeedType::SEED_CATTAIL)
        {
            if (mSeedType == SeedType::SEED_GLOOMSHROOM)
            {
                if (aRowDeviation < -1 || aRowDeviation > 1)
                {
                    continue;
                }
            }
            else if (needPortalCheck)
            {
                if (!mBoard->mChallenge->CanTargetZombieWithPortals(this, aZombie))
                {
                    continue;
                }
            }
            else if (aRowDeviation)
            {
                continue;
            }
        }

        if (aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            int aExtraRange = 0;

            if (mSeedType == SeedType::SEED_CHOMPER)
            {
                if (aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING)
                {
                    aAttackRect.mX += 20;
                    aAttackRect.mWidth -= 20;
                }

                if (aZombie->mZombiePhase == ZombiePhase::PHASE_POGO_BOUNCING || (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mTargetCol == mPlantCol))
                {
                    continue;
                }

                if (!aChomperChainRange && (aZombie->mIsEating || mState == PlantState::STATE_CHOMPER_BITING))
                {
                    aExtraRange = 60;
                }
            }

            if (mSeedType == SeedType::SEED_POTATOMINE)
            {
                if ((aZombie->mZombieType == ZombieType::ZOMBIE_POGO && aZombie->mHasObject) ||
                    aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
                {
                    continue;
                }

                if (aZombie->mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
                {
                    aAttackRect.mX += 40;
                    aAttackRect.mWidth -= 40;  // 原版经典土豆地雷 Bug 及“四撑杆引雷”的原理
                }

                if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mTargetCol != mPlantCol)
                {
                    continue;
                }

                if (aZombie->mIsEating)
                {
                    aExtraRange = 30;
                }
            }

            if ((mSeedType == SeedType::SEED_EXPLODE_O_NUT && aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT) ||
                (mSeedType == SeedType::SEED_TANGLEKELP && !aZombie->mInPool))
            {
                continue;
            }

            Rect aZombieRect = aZombie->GetZombieRect();
            if (!needPortalCheck && GetRectOverlap(aAttackRect, aZombieRect) < -aExtraRange)
            {
                continue;
            }

            ////////////////////

            int aWeight = -aZombieRect.mX;
            if (mSeedType == SeedType::SEED_CATTAIL)
            {
                aWeight = -Distance2D(mX + 40.0f, mY + 40.0f, aZombieRect.mX + aZombieRect.mWidth / 2, aZombieRect.mY + aZombieRect.mHeight / 2);
                if (aZombie->IsFlying())
                {
                    aWeight += 10000;  // 优先攻击飞行单位
                }
            }

            if (aBestZombie == nullptr || aWeight > aHighestWeight)
            {
                aHighestWeight = aWeight;
                aBestZombie = aZombie;
            }
        }
    }

    return aBestZombie;
}

int Plant::DistanceToClosestZombie()
{
	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
	Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
	int aClosestDistance = 1000;

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mRow == mRow && aZombie->EffectedByDamage(aDamageRangeFlags))
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			int aDistance = -GetRectOverlap(aAttackRect, aZombieRect);
			if (aDistance < aClosestDistance)
			{
				aClosestDistance = std::max(aDistance, 0);
			}
		}
	}

	return aClosestDistance;
}

void Plant::Die()
{
    DamageStats::Source aDamageSource(mApp->mDamageStats, true);
	if (IsOnBoard() && mSeedType == SeedType::SEED_TANGLEKELP)
	{
		Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
		if (aZombie)
		{
			aZombie->DieWithLoot();
		}
	}

	mDead = true;
	RemoveEffects();

	if (!Plant::IsFlying(mSeedType) && IsOnBoard())
	{
		GridItem* aLadder = mBoard->GetLadderAt(mPlantCol, mRow);
		if (aLadder)
		{
			aLadder->GridItemDie();
		}
	}

	if (IsOnBoard())
	{
		Plant* aTopPlant = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_BUNGEE_ORDER);
		Plant* aFlowerPot = mBoard->GetFlowerPotAt(mPlantCol, mRow);
		if (aFlowerPot && aTopPlant == aFlowerPot)
		{
			Reanimation* aPotReanim = mApp->ReanimationGet(aFlowerPot->mBodyReanimID);
			aPotReanim->mAnimRate = RandRangeFloat(10.0f, 15.0f);
		}
	}
}

const PlantDefinition& GetPlantDefinition(SeedType theSeedType)
{
	PVZP_ASSERT(gPlantDefs[theSeedType].mSeedType == theSeedType);
	PVZP_ASSERT(theSeedType >= 0 && theSeedType < static_cast<int>(SeedType::NUM_SEED_TYPES));

	return gPlantDefs[theSeedType];
}

int Plant::GetCost(SeedType theSeedType, SeedType theImitaterType)
{
    const int baseCost = [&]() -> int
    {
    if (gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST)
    {
        if (theSeedType == SeedType::SEED_REPEATER)
        {
            return 1000;
        }
        else if (theSeedType == SeedType::SEED_FUMESHROOM)
        {
            return 500;
        }
        else if (theSeedType == SeedType::SEED_TALLNUT)
        {
            return 250;
        }
        else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE)
        {
            return 100;
        }
        else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
        {
            return 200;
        }
    }

    switch (theSeedType)
    {
    case SeedType::SEED_SLOT_MACHINE_SUN:           return 0;
    case SeedType::SEED_SLOT_MACHINE_DIAMOND:       return 0;
    case SeedType::SEED_ZOMBIQUARIUM_SNORKLE:       return 100;
    case SeedType::SEED_ZOMBIQUARIUM_TROPHY:        return 1000;
    case SeedType::SEED_ZOMBIE_NORMAL:              return 50;
    case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:        return 75;
    case SeedType::SEED_ZOMBIE_POLEVAULTER:         return 75;
    case SeedType::SEED_ZOMBIE_PAIL:                return 125;
    case SeedType::SEED_ZOMBIE_LADDER:              return 150;
    case SeedType::SEED_ZOMBIE_DIGGER:              return 125;
    case SeedType::SEED_ZOMBIE_BUNGEE:              return 125;
    case SeedType::SEED_ZOMBIE_FOOTBALL:            return 175;
    case SeedType::SEED_ZOMBIE_BALLOON:             return 150;
    case SeedType::SEED_ZOMBIE_SCREEN_DOOR:         return 100;
    case SeedType::SEED_ZOMBONI:                    return 175;
    case SeedType::SEED_ZOMBIE_POGO:                return 200;
    case SeedType::SEED_ZOMBIE_DANCER:              return 350;
    case SeedType::SEED_ZOMBIE_GARGANTUAR:          return 300;
    case SeedType::SEED_ZOMBIE_IMP:                 return 50;
    default:
    {
        SeedType aCostSeedType = theSeedType;
        if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
        {
            aCostSeedType = theImitaterType;
        }

        // @pvz-online: 闯关里蘑菇全醒（见构造函数），售价 +25 阳光（用户定案）。
        // 卡面显示、扣费、可用判定全走这一个入口，所以自动一致；模仿者按被模仿的那株算。
        int aCost = GetPlantDefinition(aCostSeedType).mSeedCost;
        if (gLawnApp->IsRunMode())
        {
            // @pvz-online: 批 18 卷心菜「Free Toss」——种植费用变为 0（1 层）。卡面显示、
            // 扣费、可用判定全走 GetCost 这一个口，所以自动一致；模仿者上面已解析到本株。
            if (aCostSeedType == SeedType::SEED_CABBAGEPULT && gLawnApp->RunPlantUpgradeCount(SeedType::SEED_CABBAGEPULT) > 0)
            {
                return 0;
            }

            // @pvz-online: 第二 buff 批 2「免植」（分裂豌豆，docs/06 §8.6 #28）：种下 0 费
            //（1★，费用轴新原型同 Free Toss 先例）。卡面显示、扣费、可用判定同走 GetCost。
            if (aCostSeedType == SeedType::SEED_SPLITPEA && gLawnApp->RunPlantBuff2Count(SeedType::SEED_SPLITPEA) > 0)
            {
                return 0;
            }

            // @pvz-online: 「Nut Bowling」（扩展表尾追加，2026-10-15）：坚果费用变 0（保龄球
            // 质变词条的费用轴，同卷心菜/分裂豌豆先例；卡面/扣费/可用判定同走 GetCost）。
            if (aCostSeedType == SeedType::SEED_WALLNUT && gLawnApp->RunExtraBuffCount(RUN_BUFF2_NUT_BOWLING) > 0)
            {
                return 0;
            }

            // @pvz-online: 闯关里紫卡直接种（见 Board::CanPlantAt），售价 = 紫卡原价
            // + 基础植物原价（用户定案清单）；两笔都按原版取值，不再叠加蘑菇 +25。
            SeedType aBaseType = GetUpgradeBaseFor(aCostSeedType);
            if (aBaseType != SeedType::SEED_NONE)
            {
                return aCost + GetPlantDefinition(aBaseType).mSeedCost;
            }

            // @pvz-online: 阳光菇 0 费（用户 2026-10-04 定案）——连蘑菇 +25 加价一起豁免，
            // 所有模式（战役/挑战/闯关）实际都 0 费；其余蘑菇照旧 +25。
            if (IsNocturnal(aCostSeedType) && aCostSeedType != SeedType::SEED_SUNSHROOM)
            {
                aCost += 25;
            }
        }
        return aCost;
    }
    }

    }();
    SeedType plantType = theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE ? theImitaterType : theSeedType;
    if (gLawnApp->IsRunMode() && plantType >= SeedType::SEED_PEASHOOTER && plantType <= SeedType::SEED_IMITATER)
        return baseCost + 25 * gLawnApp->mRunState->ZombieBuffCount(RunState::ZOMBIE_BUFF_PLANT_TAX);
    return baseCost;
}

std::string Plant::GetNameString(SeedType theSeedType, SeedType theImitaterType)
{
	const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
	std::string aName = std::format("[{}]", aPlantDef.mPlantName);
	std::string aTranslatedName(PvzpStringTranslate(aName));

	if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
	{
		const PlantDefinition& aImitaterDef = GetPlantDefinition(theImitaterType);
		std::string aImitaterName = std::format("[{}]", aImitaterDef.mPlantName);
		std::string aTranslatedImitaterName(PvzpStringTranslate(aImitaterName));
		return std::format("{} {}", aTranslatedName, aTranslatedImitaterName);
	}

	return aTranslatedName;
}

std::string Plant::GetToolTip(SeedType theSeedType)
{
	const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
	std::string aToolTip = std::format("[{}_TOOLTIP]", aPlantDef.mPlantName);
	return std::string(PvzpStringTranslate(aToolTip));
}

int Plant::GetRefreshTime(SeedType theSeedType, SeedType theImitaterType)
{
    if (Challenge::IsZombieSeedType(theSeedType))
    {
        return 0;
    }

    // @pvz-online: 「Devotion」（魅惑菇老表 #12）负面：拥有该词条时魅惑菇卡冷却 ×6
    // （2026-10-11 晚恢复词条时加，750cs→4500cs）——模仿者复制的魅惑菇同乘。
    SeedType aRealType = theSeedType;
    if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
    {
        aRealType = theImitaterType;
    }
    int aRefreshTime = GetPlantDefinition(aRealType).mRefreshTime;
    if (aRealType == SeedType::SEED_HYPNOSHROOM && gLawnApp->RunPlantUpgradeCount(SeedType::SEED_HYPNOSHROOM) > 0)
    {
        aRefreshTime *= 6;
    }
    // @pvz-online: 「Nut Bowling」：坚果冷却改 3 秒（300cs，2026-10-19）。速种
    // （RUN_BUFF_FASTSEED）在 SeedPacket 侧随后再乘，属预期叠加。
    if (aRealType == SeedType::SEED_WALLNUT && gLawnApp->RunExtraBuffCount(RUN_BUFF2_NUT_BOWLING) > 0)
    {
        aRefreshTime = 300;
    }
    return aRefreshTime;
}

bool Plant::IsNocturnal(SeedType theSeedtype)
{
	return
		theSeedtype == SeedType::SEED_PUFFSHROOM ||
		theSeedtype == SeedType::SEED_SEASHROOM ||
		theSeedtype == SeedType::SEED_SUNSHROOM ||
		theSeedtype == SeedType::SEED_FUMESHROOM ||
		theSeedtype == SeedType::SEED_HYPNOSHROOM ||
		theSeedtype == SeedType::SEED_DOOMSHROOM ||
		theSeedtype == SeedType::SEED_ICESHROOM ||
		theSeedtype == SeedType::SEED_MAGNETSHROOM ||
		theSeedtype == SeedType::SEED_SCAREDYSHROOM ||
		theSeedtype == SeedType::SEED_GLOOMSHROOM;
}

bool Plant::IsFungus(SeedType theSeedtype)
{
	return
		theSeedtype == SeedType::SEED_PUFFSHROOM ||
		theSeedtype == SeedType::SEED_SUNSHROOM ||
		theSeedtype == SeedType::SEED_FUMESHROOM ||
		theSeedtype == SeedType::SEED_HYPNOSHROOM ||
		theSeedtype == SeedType::SEED_SCAREDYSHROOM ||
		theSeedtype == SeedType::SEED_ICESHROOM ||
		theSeedtype == SeedType::SEED_DOOMSHROOM ||
		theSeedtype == SeedType::SEED_SEASHROOM ||
		theSeedtype == SeedType::SEED_MAGNETSHROOM ||
		theSeedtype == SeedType::SEED_GLOOMSHROOM;
}

bool Plant::IsAquatic(SeedType theSeedType)
{
	return
		theSeedType == SeedType::SEED_LILYPAD ||
		theSeedType == SeedType::SEED_TANGLEKELP ||
		theSeedType == SeedType::SEED_SEASHROOM ||
		theSeedType == SeedType::SEED_CATTAIL;
}

bool Plant::IsFlying(SeedType theSeedtype)
{
	return theSeedtype == SeedType::SEED_INSTANT_COFFEE;
}

bool Plant::IsUpgrade(SeedType theSeedtype)
{
	return
		theSeedtype == SeedType::SEED_GATLINGPEA ||
		theSeedtype == SeedType::SEED_WINTERMELON ||
		theSeedtype == SeedType::SEED_TWINSUNFLOWER ||
		theSeedtype == SeedType::SEED_SPIKEROCK ||
		theSeedtype == SeedType::SEED_COBCANNON ||
		theSeedtype == SeedType::SEED_GOLD_MAGNET ||
		theSeedtype == SeedType::SEED_GLOOMSHROOM ||
		theSeedtype == SeedType::SEED_CATTAIL;
}

Rect Plant::GetPlantRect()
{
	Rect aRect;
	if (mSeedType == SeedType::SEED_TALLNUT)
	{
		aRect = Rect(mX + 10, mY, mWidth, mHeight);
	}
	else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		aRect = Rect(mX, mY, mWidth - 20, mHeight);
	}
	else if (mSeedType == SeedType::SEED_COBCANNON)
	{
		aRect = Rect(mX, mY, 140, 80);
	}
	else
	{
		aRect = Rect(mX + 10, mY, mWidth - 20, mHeight);
	}

	return aRect;
}

Rect Plant::GetPlantAttackRect(PlantWeapon thePlantWeapon)
{
    Rect aRect;
    if (mApp->IsWallnutBowlingLevel() || IsBowlingNut(this))
    {
        aRect = Rect(mX, mY, mWidth - 20, mHeight);
    }
    else if (thePlantWeapon == PlantWeapon::WEAPON_SECONDARY && mSeedType == SeedType::SEED_SPLITPEA)
    {
        aRect = Rect(0, mY, mX + 16, mHeight);
    }
    else switch (mSeedType)
    {
    case SeedType::SEED_LEFTPEATER:     aRect = Rect(0,             mY,             mX,                 mHeight);               break;
    case SeedType::SEED_SQUASH:         aRect = Rect(mX + 20,       mY,             mWidth - 35,        mHeight);               break;
    case SeedType::SEED_CHOMPER:        aRect = Rect(mX + 80,       mY,             40,                 mHeight);               break;
    case SeedType::SEED_SPIKEWEED:
    case SeedType::SEED_SPIKEROCK:      aRect = Rect(mX + 20,       mY,             mWidth - 50,        mHeight);               break;
    case SeedType::SEED_POTATOMINE:     aRect = Rect(mX,            mY,             mWidth - 25,        mHeight);               break;
    case SeedType::SEED_TORCHWOOD:      aRect = Rect(mX + 50,       mY,             30,                 mHeight);               break;
    case SeedType::SEED_PUFFSHROOM:
    case SeedType::SEED_SEASHROOM:      aRect = Rect(mX + 60,       mY,             230,                mHeight);               break;
    // @pvz-online: continuous fumes keeps the original 340px range; only its firing clock changes.
    case SeedType::SEED_FUMESHROOM:     aRect = Rect(mX + 60,       mY,             340,                mHeight);               break;
    // @pvz-online: 闯关「Gloom」（忧郁菇，批 14）：光环范围每层 +1 格——240×240（3 格）
    // 逐层 +160px、中心不动（n 层 = 3+2n 格宽）；纵向行差在 DoRowAreaDamage 同步外扩。
    case SeedType::SEED_GLOOMSHROOM:
    {
        int aGloomExtra = mApp->RunPlantUpgradeCount(SeedType::SEED_GLOOMSHROOM);
        aRect = Rect(mX - 80 - 80 * aGloomExtra, mY - 80 - 80 * aGloomExtra, 240 + 160 * aGloomExtra, 240 + 160 * aGloomExtra);
        break;
    }
    case SeedType::SEED_TANGLEKELP:     aRect = Rect(mX,            mY,             mWidth,             mHeight);               break;
    case SeedType::SEED_CATTAIL:        aRect = Rect(-BOARD_WIDTH,  -BOARD_HEIGHT,  BOARD_WIDTH * 2,    BOARD_HEIGHT * 2);      break;
    default:                            aRect = Rect(mX + 60,       mY,             BOARD_WIDTH,        mHeight);               break;
    }

    return aRect;
}

void Plant::PreloadPlantResources(SeedType theSeedType)
{
	const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
	if (aPlantDef.mReanimationType != ReanimationType::REANIM_NONE)
	{
		ReanimatorEnsureDefinitionLoaded(aPlantDef.mReanimationType, true);
	}

	if (theSeedType == SeedType::SEED_CHERRYBOMB)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CHARRED, true);
	}
	else if (theSeedType == SeedType::SEED_JALAPENO)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_JALAPENO_FIRE, true);
	}
	else if (theSeedType == SeedType::SEED_TORCHWOOD)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_FIRE_PEA, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_JALAPENO_FIRE, true);
	}
	else if (Plant::IsNocturnal(theSeedType))
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SLEEPING, true);
	}
}

void Plant::PlayIdleAnim(float theRate)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim)
	{
		PlayBodyReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, theRate);
		if (mApp->IsIZombieLevel())
		{
			aBodyReanim->mAnimRate = 0.0f;
		}
	}
}

// @pvz-online: retained lawns use the same current health rule as freshly planted entities.
void Plant::RefreshRunMaxHealth()
{
    const int maximum = CoopMaxHealth();
    if (maximum == mPlantMaxHealth) return;
    mPlantHealth = (int)((long long)std::max(0, mPlantHealth) * maximum / std::max(1, mPlantMaxHealth));
    mPlantMaxHealth = maximum;
}

int Plant::CoopMaxHealth()
{
    int base=300;
    if(mSeedType==SEED_WALLNUT || mSeedType==SEED_PUMPKINSHELL)base=4000;
    else if(mSeedType==SEED_TALLNUT)base=8000;
    else if(mSeedType==SEED_GARLIC)base=400;
    else if(mSeedType==SEED_SPIKEROCK)base=450;
    else if(mSeedType==SEED_CHOMPER && mApp->RunExtraBuffCount(RUN_BUFF2_DIGEST_HEAL)>0)base=3000;
    else if(mSeedType==SEED_GLOOMSHROOM && mApp->RunPlantBuff2Count(mSeedType)>0)base=1000;
    double value=base*(double)mApp->RunBuffMul(RUN_BUFF_ROOTED)*mApp->RunPlantUpgradeMulKind(mSeedType,RUN_UPGRADE_KIND_HEALTH);
    value=std::max(1.0,std::min((double)2147483647,value+0.5));
    if(mApp->RunPlantBuff2Count(mSeedType)>0) {
        if(mSeedType==SEED_SCAREDYSHROOM)value=4000;
        else if(mSeedType==SEED_GARLIC)value=20;
    }
    if(mSeedType==SEED_PUMPKINSHELL && mApp->RunExtraBuffCount(RUN_BUFF2_STEEL_PUMPKIN)>0)value=std::max(1.0,value/2);
    return (int)value;
}
float Plant::CoopAttackPeriod()
{
    if(mSeedType==SEED_FUMESHROOM && mApp->RunPlantUpgradeCount(mSeedType)>0)return 20.0f;
    return std::max(1,mLaunchRate)/PlantShootSpeed(mApp,mSeedType,this);
}
