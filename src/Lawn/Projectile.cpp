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

#include "Board.h"
#include "Plant.h"
#include "Zombie.h"
#include "Cutscene.h"
#include "Projectile.h"
#include "Run/RunDotEffects.h"
#include "Run/RunWaveHitPacking.h"
#include "Run/RunBuffs.h"
#include "../LawnApp.h"
#include "../Resources.h"
#include "../GameConstants.h"
#include "../PvzpLib/PvzpFoley.h"
#include "../PvzpLib/PvzpDebug.h"
#include "../PvzpLib/Reanimator.h"
#include "../PvzpLib/Attachment.h"
#include "Widget/AchievementsScreen.h"
#include <algorithm>

constinit const ProjectileDefinition gProjectileDefinition[] = {
	{ .mProjectileType = ProjectileType::PROJECTILE_PEA, .mImageRow = 0, .mDamage = 20 },
	{ .mProjectileType = ProjectileType::PROJECTILE_SNOWPEA, .mImageRow = 0, .mDamage = 20 },
	{ .mProjectileType = ProjectileType::PROJECTILE_CABBAGE, .mImageRow = 0, .mDamage = 40 },
	{ .mProjectileType = ProjectileType::PROJECTILE_MELON, .mImageRow = 0, .mDamage = 80 },
	{ .mProjectileType = ProjectileType::PROJECTILE_PUFF, .mImageRow = 0, .mDamage = 20 },
	{ .mProjectileType = ProjectileType::PROJECTILE_WINTERMELON, .mImageRow = 0, .mDamage = 80 },
	{ .mProjectileType = ProjectileType::PROJECTILE_FIREBALL, .mImageRow = 0, .mDamage = 40 },
	{ .mProjectileType = ProjectileType::PROJECTILE_STAR, .mImageRow = 0, .mDamage = 20 },
	{ .mProjectileType = ProjectileType::PROJECTILE_SPIKE, .mImageRow = 0, .mDamage = 20 },
	{ .mProjectileType = ProjectileType::PROJECTILE_BASKETBALL, .mImageRow = 0, .mDamage = 75 },
	{ .mProjectileType = ProjectileType::PROJECTILE_KERNEL, .mImageRow = 0, .mDamage = 20 },
	{ .mProjectileType = ProjectileType::PROJECTILE_COBBIG, .mImageRow = 0, .mDamage = 300 },
	{ .mProjectileType = ProjectileType::PROJECTILE_BUTTER, .mImageRow = 0, .mDamage = 40 },
	{ .mProjectileType = ProjectileType::PROJECTILE_ZOMBIE_PEA, .mImageRow = 0, .mDamage = 20 }
};

static int KindlingFireballDamage(LawnApp* theApp, ProjectileType theType, int theBaseDamage)
{
	if (theType != ProjectileType::PROJECTILE_FIREBALL) return theBaseDamage;
	return (int)(theBaseDamage * theApp->RunPlantUpgradeMul(SeedType::SEED_TORCHWOOD) + 0.5f);
}

// @pvz-online: use pre-impact body HP, not armor, and cap before any damage multipliers.
int Projectile::GetRunFixedDamageBonus(Zombie* theZombie)
{
    if (!mRunScaredySource || !theZombie || theZombie->IsDeadOrDying() ||
        mApp->RunExtraBuffCount(RUN_BUFF2_HEART_SPORE) == 0)
        return 0;
    return std::min(5000, std::max(0, theZombie->mBodyHealth) / 20);
}


bool Projectile::WasZombiePricklyHit(Zombie* theZombie)
{
	ZombieID aID = mBoard->ZombieGetID(theZombie);
	int aSlots = (mPricklyHitCount < 16) ? mPricklyHitCount : 16;
	for (int i = 0; i < aSlots; i++)
	{
		if (mPricklyHitIds[i] == aID)
		{
			return true;
		}
	}
	return false;
}

void Projectile::RecordPricklyHit(Zombie* theZombie)
{
	mPricklyHitIds[mPricklyHitCount % 16] = mBoard->ZombieGetID(theZombie);
	mPricklyHitCount++;
}
Projectile::Projectile()
{
}

Projectile::~Projectile()
{
	AttachmentDie(mAttachmentID);
}

void Projectile::ProjectileInitialize(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType)
{
	int aGridX = mBoard->PixelToGridXKeepOnBoard(theX, theY);
	mProjectileType = theProjectileType;
	mPosX = theX;
	mPosY = theY;
	mPosZ = 0.0f;
	mVelX = 0.0f;
	mVelY = 0.0f;
	mVelZ = 0.0f;
	mAccZ = 0.0f;
	mShadowY = mBoard->GridToPixelY(aGridX, theRow) + 67.0f;
	mHitTorchwoodGridX = -1;
	mMotionType = ProjectileMotion::MOTION_STRAIGHT;
	mFrame = 0;
	mNumFrames = 1;
	mRow = theRow;
	mCobTargetX = 0.0f;
	mDamageRangeFlags = 0;
	mDead = false;
	mAttachmentID = AttachmentID::ATTACHMENTID_NULL;
	mCobTargetRow = 0;
	mTargetZombieID = ZombieID::ZOMBIEID_NULL;
	// @pvz-online: 单株升级「Prickly」（仙人掌）的穿透标记（见 Projectile.h）；默认 0
	//（无条），Fire 里只有仙人掌的弹会预置 -1（无限）或有限计数。
	mPricklyHitsLeft = 0;
	mPricklyHitCount = 0;
	mRunIceSource = false;
	mRunCactusSource = false;
	mRunScaredySource = false;
	mRunCrystalDamage = 0;
    mRunSourceSeed = SeedType::SEED_NONE;
    mRunHuntHitsLeft = 0;
    mRunLaunchDelay = 0;
    mRunIceWave = false;
    for (int i = 0; i < 1024; ++i) mRunWaveHitIds[i] = ZombieID::ZOMBIEID_NULL;
	mOnHighGround = mBoard->mGridSquareType[aGridX][theRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND;
	if (mBoard->StageHasRoof())
	{
		mShadowY -= 12.0f;
	}
	mRenderOrder = theRenderOrder;
	mRotation = 0.0f;
	mRotationSpeed = 0.0f;
	mWidth = 40;
	mHeight = 40;
	mProjectileAge = 0;
	mClickBackoffCounter = 0;
	mAnimTicksPerFrame = 0;

	if (mProjectileType == ProjectileType::PROJECTILE_CABBAGE || mProjectileType == ProjectileType::PROJECTILE_BUTTER)
	{
		mRotation = -7 * PI / 25;  // DEG_TO_RAD(-50.4f);
		mRotationSpeed = RandRangeFloat(-0.08f, -0.02f);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_MELON || mProjectileType == ProjectileType::PROJECTILE_WINTERMELON)
	{
		mRotation = -2 * PI / 5;  // DEG_TO_RAD(-72.0f);
		mRotationSpeed = RandRangeFloat(-0.08f, -0.02f);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_KERNEL)
	{
		mRotation = 0.0f;
		mRotationSpeed = RandRangeFloat(-0.2f, -0.08f);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_SNOWPEA)
	{
		PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(mPosX + 8.0f, mPosY + 13.0f, 400000, ParticleEffect::PARTICLE_SNOWPEA_TRAIL);
		AttachParticle(mAttachmentID, aParticle, 8.0f, 13.0f);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL)
	{
		PVZP_ASSERT();
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		mWidth = IMAGE_REANIM_COBCANNON_COB->GetWidth();
		mHeight = IMAGE_REANIM_COBCANNON_COB->GetHeight();
		mRotation = PI / 2;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_PUFF)
	{
		PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(mPosX + 13.0f, mPosY + 13.0f, 400000, ParticleEffect::PARTICLE_PUFFSHROOM_TRAIL);
		AttachParticle(mAttachmentID, aParticle, 13.0f, 13.0f);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_BASKETBALL)
	{
		mRotation = RandRangeFloat(0.0f, 2 * PI);
		mRotationSpeed = RandRangeFloat(0.05f, 0.1f);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_STAR)
	{
		mShadowY += 15.0f;
		mRotationSpeed = RandRangeFloat(0.05f, 0.1f);
		if (Rand(2) == 0)
		{
			mRotationSpeed = -mRotationSpeed;
		}
	}

	mAnimCounter = 0;
	mX = (int)mPosX;
	mY = (int)mPosY;
}

Plant* Projectile::FindCollisionTargetPlant()
{
	Rect aProjectileRect = GetProjectileRect();

	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow != mRow)
			continue;

		if (mProjectileType == ProjectileType::PROJECTILE_ZOMBIE_PEA)
		{
			if (aPlant->mSeedType == SeedType::SEED_PUFFSHROOM ||
				aPlant->mSeedType == SeedType::SEED_SUNSHROOM ||
				aPlant->mSeedType == SeedType::SEED_POTATOMINE ||
				aPlant->mSeedType == SeedType::SEED_SPIKEWEED ||
				aPlant->mSeedType == SeedType::SEED_SPIKEROCK ||
				aPlant->mSeedType == SeedType::SEED_LILYPAD)  // zombie peas cannot hit low plants
				continue;
		}

		Rect aPlantRect = aPlant->GetPlantRect();
		if (GetRectOverlap(aProjectileRect, aPlantRect) > 8)
		{
			if (mProjectileType == ProjectileType::PROJECTILE_ZOMBIE_PEA)
			{
				return mBoard->GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_EATING_ORDER);
			}
			else
			{
				return mBoard->GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_CATAPULT_ORDER);
			}
		}
	}

	return nullptr;
}

bool Projectile::PeaAboutToHitTorchwood()
{
	if (mMotionType != ProjectileMotion::MOTION_STRAIGHT)
		return false;

	if (mProjectileType != ProjectileType::PROJECTILE_PEA && mProjectileType != ProjectileType::PROJECTILE_SNOWPEA)
		return false;

	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == SeedType::SEED_TORCHWOOD && aPlant->mRow == mRow && !aPlant->NotOnGround() && mHitTorchwoodGridX != aPlant->mPlantCol)
		{
			Rect aPlantAttackRect = aPlant->GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
			Rect aProjectileRect = GetProjectileRect();
			aProjectileRect.mX += 40;

			if (GetRectOverlap(aPlantAttackRect, aProjectileRect) > 10)
			{
				return true;
			}
		}
	}

	return false;
}

Zombie* Projectile::FindCollisionTarget()
{
	if (PeaAboutToHitTorchwood())  // 豌豆即将被火炬树桩吸收：交给原版逻辑，这里不算命中僵尸
		return nullptr;

	Rect aProjectileRect = GetProjectileRect();
	Zombie* aBestZombie = nullptr;
	int aMinX = 0;

	Zombie* aZombie = nullptr;
	while (mBoard->IterateZombies(aZombie))
	{
		if ((aZombie->mZombieType == ZombieType::ZOMBIE_BOSS || aZombie->mRow == mRow) && aZombie->EffectedByDamage((unsigned int)mDamageRangeFlags))
		{
			if (aZombie->mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL && mPosZ >= 45.0f)
			{
				continue;
			}

			if (mProjectileType == ProjectileType::PROJECTILE_STAR && mProjectileAge < 25 && mVelX >= 0.0f && aZombie->mZombieType == ZombieType::ZOMBIE_DIGGER)
			{
				continue;
			}

			Rect aZombieRect = aZombie->GetZombieRect();
			if (GetRectOverlap(aProjectileRect, aZombieRect) > 0)
			{
				// @pvz-online: 穿透弹命中去重（2026-10-10）：环内僵尸（本次穿越已命中过）
				// 不再作候选——弹体命中后不再推位（旧推位会跳过贴身/重叠僵尸）。
				if (mPricklyHitsLeft != 0 && WasZombiePricklyHit(aZombie))
				{
					continue;
				}
				if (aBestZombie == nullptr || aZombie->mX < aMinX)
				{
					aBestZombie = aZombie;
					aMinX = aZombie->mX;
				}
			}
		}
	}

	return aBestZombie;
}

void Projectile::CheckForCollision()
{
	if (mMotionType == ProjectileMotion::MOTION_PUFF && mProjectileAge >= 75)
	{
		Die();
		return;
	}

	if (mPosX > WIDE_BOARD_WIDTH || mPosX + mWidth < 0.0f)
	{
		Die();
		return;
	}

	if (mMotionType == ProjectileMotion::MOTION_HOMING)
	{
		Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
        if (mRunHuntHitsLeft > 0 && (!aZombie || aZombie->IsDeadOrDying() || !aZombie->EffectedByDamage((unsigned int)mDamageRangeFlags)))
        {
            aZombie = FindHuntTarget();
            if (!aZombie) { Die(); return; }
            mTargetZombieID = mBoard->ZombieGetID(aZombie);
        }
		if (aZombie && aZombie->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)))
		{
			Rect aProjectileRect = GetProjectileRect();
			Rect aZombieRect = aZombie->GetZombieRect();
			if (GetRectOverlap(aProjectileRect, aZombieRect) >= 0 && mPosY > aZombieRect.mY&& mPosY < aZombieRect.mY + aZombieRect.mHeight)
			{
				DoImpact(aZombie);
			}
		}
		return;
	}

	if (mProjectileType == ProjectileType::PROJECTILE_STAR && (mPosY > 600.0f || mPosY < 40.0f))
	{
		Die();
		return;
	}

	if ((mProjectileType == ProjectileType::PROJECTILE_PEA || mProjectileType == ProjectileType::PROJECTILE_STAR) && mShadowY - mPosY > 90.0f)
	{
		return;
	}

	if (mMotionType == ProjectileMotion::MOTION_FLOAT_OVER)
	{
		return;
	}

	if (mProjectileType == ProjectileType::PROJECTILE_ZOMBIE_PEA)
	{
		Plant* aPlant = FindCollisionTargetPlant();
		if (aPlant)
		{
			const ProjectileDefinition& aProjectileDef = GetProjectileDef();
			aPlant->mPlantHealth -= aProjectileDef.mDamage;
			aPlant->mEatenFlashCountdown = std::max(aPlant->mEatenFlashCountdown, 25);

			mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
			mApp->AddPvzpParticle(mPosX - 3.0f, mPosY + 17.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PEA_SPLAT);
			Die();
		}
		return;
	}

	Zombie* aZombie = FindCollisionTarget();
	if (aZombie)
	{
		if (aZombie->mOnHighGround && CantHitHighGround())
		{
			return;
		}

		DoImpact(aZombie);
	}
}

bool Projectile::CantHitHighGround()
{
	if (mMotionType == ProjectileMotion::MOTION_BACKWARDS || mMotionType == ProjectileMotion::MOTION_HOMING)
		return false;

	return (
		mProjectileType == ProjectileType::PROJECTILE_PEA ||
		mProjectileType == ProjectileType::PROJECTILE_SNOWPEA ||
		mProjectileType == ProjectileType::PROJECTILE_STAR ||
		mProjectileType == ProjectileType::PROJECTILE_PUFF ||
		mProjectileType == ProjectileType::PROJECTILE_FIREBALL
		) && !mOnHighGround;
}

void Projectile::CheckForHighGround()
{
	float aShadowDelta = mShadowY - mPosY;

	if (mProjectileType == ProjectileType::PROJECTILE_PEA ||
		mProjectileType == ProjectileType::PROJECTILE_SNOWPEA ||
		mProjectileType == ProjectileType::PROJECTILE_FIREBALL ||
		mProjectileType == ProjectileType::PROJECTILE_SPIKE ||
		mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		if (aShadowDelta < 28.0f)
		{
			DoImpact(nullptr);
			return;
		}
	}

	if (mProjectileType == ProjectileType::PROJECTILE_PUFF && aShadowDelta < 0.0f)
	{
		DoImpact(nullptr);
		return;
	}

	if (mProjectileType == ProjectileType::PROJECTILE_STAR && aShadowDelta < 23.0f)
	{
		DoImpact(nullptr);
		return;
	}

	if (CantHitHighGround())
	{
		int aGridX = mBoard->PixelToGridXKeepOnBoard(mPosX + 30, mPosY);
		if (mBoard->mGridSquareType[aGridX][mRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND)
		{
			DoImpact(nullptr);
		}
	}
}

bool Projectile::IsSplashDamage(Zombie* theZombie)
{
	if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL && theZombie && theZombie->IsFireResistant())
		return false;

	return
		mProjectileType == ProjectileType::PROJECTILE_MELON ||
		mProjectileType == ProjectileType::PROJECTILE_WINTERMELON ||
		mProjectileType == ProjectileType::PROJECTILE_FIREBALL;
}

unsigned int Projectile::GetDamageFlags(Zombie* theZombie)
{
	unsigned int aDamageFlags = 0U;

	if (IsSplashDamage(theZombie))
	{
		SetBit(aDamageFlags, static_cast<int>(DamageFlags::DAMAGE_HITS_SHIELD_AND_BODY), true);
	}
	else if (mMotionType == ProjectileMotion::MOTION_LOBBED || mMotionType == ProjectileMotion::MOTION_BACKWARDS)
	{
		SetBit(aDamageFlags, static_cast<int>(DamageFlags::DAMAGE_BYPASSES_SHIELD), true);
	}
	else if (mMotionType == ProjectileMotion::MOTION_STAR && mVelX < 0.0f)
	{
		SetBit(aDamageFlags, static_cast<int>(DamageFlags::DAMAGE_BYPASSES_SHIELD), true);
	}

	if (mProjectileType == ProjectileType::PROJECTILE_SNOWPEA || mProjectileType == ProjectileType::PROJECTILE_WINTERMELON)
	{
		SetBit(aDamageFlags, static_cast<int>(DamageFlags::DAMAGE_FREEZE), true);
	}

	return aDamageFlags;
}

bool Projectile::IsZombieHitBySplash(Zombie* theZombie)
{
	Rect aProjectileRect = GetProjectileRect();
	if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL)
	{
		aProjectileRect.mWidth = 100;
	}
    // @pvz-online: only genuine Winter Melon shots gain the full 3x3 burst.
    else if (mRunSourceSeed == SeedType::SEED_WINTERMELON && mApp->RunPlantUpgradeCount(SeedType::SEED_WINTERMELON) > 0)
    {
        aProjectileRect.mX = (int)mPosX - 100;
        aProjectileRect.mWidth = 240;
    }

	int aRowDeviation = theZombie->mRow - mRow;
	Rect aZombieRect = theZombie->GetZombieRect();
	if (theZombie->IsFireResistant() && mProjectileType == ProjectileType::PROJECTILE_FIREBALL)
	{
		return false;
	}

	if (theZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		aRowDeviation = 0;
	}
	if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL)
	{
		// @pvz-online: 第二 buff「烈焰溅射」（火炬树桩 #22，docs/06 §8.6，2★ cap1）：持条时
		// 火弹溅射判定放行上下各一行（基线只有同排）；不持条行为原样。
		bool aFlameSplashBuff = mApp->RunPlantBuff2Count(SeedType::SEED_TORCHWOOD) > 0;
		if (aRowDeviation != 0 && !(aFlameSplashBuff && (aRowDeviation == 1 || aRowDeviation == -1)))
		{
			return false;
		}
	}
	else if (aRowDeviation > 1 || aRowDeviation < -1)
	{
		return false;
	}

	return theZombie->EffectedByDamage((unsigned int)mDamageRangeFlags) && GetRectOverlap(aProjectileRect, aZombieRect) >= 0;
}

void Projectile::DoSplashDamage(Zombie* theZombie)
{
    DamageStats::Source aDamageSource(mApp->mDamageStats, true);
	const ProjectileDefinition& aProjectileDef = GetProjectileDef();

	int aZombiesGetSplashed = 0;
	Zombie* aZombie = nullptr;
	while (mBoard->IterateZombies(aZombie))
	{
		if (aZombie != theZombie && IsZombieHitBySplash(aZombie))
		{
			aZombiesGetSplashed++;
		}
	}

	// @pvz-online: 闯关 buff「火力」：基数先放大，后面的溅射上限/递减换算全从它推，
	// 比例关系保持不变（非闯关局 RunBuffMul 恒为 1.0，取整后与原值一致）。
	int aOriginalDamage = (int)(aProjectileDef.mDamage * mApp->RunBuffMul(RUN_BUFF_FIREPOWER) + 0.5f);
	// @pvz-online: 单株升级「Kindling」（火炬树桩）：火弹再乘单株乘数（helper 注释），
	// 后面的溅射 1/3 递减与上限换算都从放大后的基数推，比例不变。
	aOriginalDamage = KindlingFireballDamage(mApp, mProjectileType, aOriginalDamage);
	// @pvz-online: 「重投」原挂此处，2026-10-10 实机修复挪走——卷心菜弹不是溅射弹
	//（IsSplashDamage 只认西瓜/冰西瓜/火弹），这条路径对卷心菜零执行；现挂 DoImpact
	// 单发分支（见该处注释）。
	bool aColdBurst = mRunSourceSeed == SeedType::SEED_WINTERMELON && mApp->RunPlantUpgradeCount(SeedType::SEED_WINTERMELON) > 0;
    int aSplashDamage = aColdBurst ? aOriginalDamage : aOriginalDamage / 3;
	int aMaxSplashDamageAmount = aSplashDamage * 7;
	if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL)
	{
		aMaxSplashDamageAmount = aOriginalDamage;
	}
	// @pvz-online: 第二 buff「烈焰溅射」（火炬树桩 #22，docs/06 §8.6，2★ cap1）：持条时火弹
	// 溅射每只都吃满原伤 1/3，不再按总上限（原伤）摊薄；不持条走基线摊薄。
	bool aFlameSplashBuff = mProjectileType == ProjectileType::PROJECTILE_FIREBALL && mApp->RunPlantBuff2Count(SeedType::SEED_TORCHWOOD) > 0;
	int aSplashDamageAmount = aSplashDamage * aZombiesGetSplashed;
	if (!aColdBurst && !aFlameSplashBuff && aSplashDamageAmount > aMaxSplashDamageAmount)
	{
		//aSplashDamage *= aMaxSplashDamageAmount / aSplashDamage;
		aSplashDamage = aOriginalDamage * aMaxSplashDamageAmount / (aSplashDamageAmount * 3);
		aSplashDamage = std::max(aSplashDamage, 1);
	}

	aZombie = nullptr;
	while (mBoard->IterateZombies(aZombie))
	{
		if (IsZombieHitBySplash(aZombie))
		{
			unsigned int aDamageFlags = GetDamageFlags(aZombie);
			if (aZombie == theZombie)
			{
				aZombie->TakeDamage(aOriginalDamage, aDamageFlags);
			}
			else
			{
				aZombie->TakeDamage(aSplashDamage, aDamageFlags);
			}
		}
	}
}

void Projectile::UpdateLobMotion()
{
	if (mProjectileType == ProjectileType::PROJECTILE_COBBIG && mPosZ < -700.0f)
	{
		mVelZ = 8.0f;
		mRow = mCobTargetRow;
		mPosX = mCobTargetX;
		int aCobTargetCol = mBoard->PixelToGridXKeepOnBoard(mCobTargetX, 0);
		mPosY = mBoard->GridToPixelY(aCobTargetCol, mCobTargetRow);
		mShadowY = mPosY + 67.0f;
		mRotation = -PI / 2;
	}

	mVelZ += mAccZ;
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_HIGH_GRAVITY)
	{
		mVelZ += mAccZ;
	}
	mPosX += mVelX;
	mPosY += mVelY;
	mPosZ += mVelZ;

	bool isRising = mVelZ < 0.0f;
	if (isRising && (mProjectileType == ProjectileType::PROJECTILE_BASKETBALL || mProjectileType == ProjectileType::PROJECTILE_COBBIG))
	{
		return;
	}
	if (mProjectileAge > 20)
	{
		if (isRising)
		{
			return;
		}

		float aMinCollisionZ = 0.0f;
		if (mProjectileType == ProjectileType::PROJECTILE_BUTTER)
		{
			aMinCollisionZ = -32.0f;
		}
		else if (mProjectileType == ProjectileType::PROJECTILE_BASKETBALL)
		{
			aMinCollisionZ = 60.0f;
		}
		else if (mProjectileType == ProjectileType::PROJECTILE_MELON || mProjectileType == ProjectileType::PROJECTILE_WINTERMELON)
		{
			aMinCollisionZ = -35.0f;
		}
		else if (mProjectileType == ProjectileType::PROJECTILE_CABBAGE || mProjectileType == ProjectileType::PROJECTILE_KERNEL)
		{
			aMinCollisionZ = -30.0f;
		}
		else if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
		{
			aMinCollisionZ = -60.0f;
		}
		if (mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL)
		{
			aMinCollisionZ += 40.0f;
		}

		if (mPosZ <= aMinCollisionZ)
		{
			return;
		}
	}

	Plant* aPlant = nullptr;
	Zombie* aZombie = nullptr;
	if (mProjectileType == ProjectileType::PROJECTILE_BASKETBALL || mProjectileType == ProjectileType::PROJECTILE_ZOMBIE_PEA)
	{
		aPlant = FindCollisionTargetPlant();
	}
	else
	{
		aZombie = FindCollisionTarget();
	}

	float aGroundZ = 80.0f;
	if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		aGroundZ = -40.0f;
	}
	bool hitGround = mPosZ > aGroundZ;
	if (aZombie == nullptr && aPlant == nullptr && !hitGround)
	{
		return;
	}

	if (aPlant)
	{
		Plant* aUmbrellaPlant = mBoard->FindUmbrellaPlant(aPlant->mPlantCol, aPlant->mRow);
		if (aUmbrellaPlant)
		{
			if (aUmbrellaPlant->mState == PlantState::STATE_UMBRELLA_REFLECTING)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
				int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 1);
				mApp->AddPvzpParticle(mPosX + 20.0f, mPosY + 20.0f, aRenderPosition, ParticleEffect::PARTICLE_UMBRELLA_REFLECT);
				Die();
			}
			else if (aUmbrellaPlant->mState != PlantState::STATE_UMBRELLA_TRIGGERED)
			{
				mApp->PlayFoley(FoleyType::FOLEY_UMBRELLA);
				aUmbrellaPlant->DoSpecial();
			}
		}
		else
		{
			aPlant->mPlantHealth -= GetProjectileDef().mDamage;
			aPlant->mEatenFlashCountdown = std::max(aPlant->mEatenFlashCountdown, 25);
			mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
			Die();
		}
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		int aBeforeGargantuarCount = mBoard->GetLiveGargantuarCount();
		mBoard->KillAllZombiesInRadius(mRow, mPosX + 80, mPosY + 40, 115, 1, true, mDamageRangeFlags);
		int aAfterGargantuarCount = mBoard->GetLiveGargantuarCount();
		mBoard->mGargantuarsKillsByCornCob += aBeforeGargantuarCount - aAfterGargantuarCount;
		if (mBoard->mGargantuarsKillsByCornCob >= 2)
			ReportAchievement::GiveAchievement(mApp, PopcornParty, true);

		DoImpact(nullptr);
	}
	else
	{
		DoImpact(aZombie);
	}
}

// @pvz-online: no new projectile/save fields; all copies and torchwood conversions retain the rule.
static bool FollowsRoofSlope(const Projectile* projectile)
{
    return projectile->mBoard->StageHasRoof() &&
        (projectile->mProjectileType == ProjectileType::PROJECTILE_PEA ||
         projectile->mProjectileType == ProjectileType::PROJECTILE_SNOWPEA ||
         projectile->mProjectileType == ProjectileType::PROJECTILE_FIREBALL) &&
        (projectile->mMotionType == ProjectileMotion::MOTION_STRAIGHT ||
         projectile->mMotionType == ProjectileMotion::MOTION_BACKWARDS ||
         projectile->mMotionType == ProjectileMotion::MOTION_THREEPEATER) &&
        projectile->mApp->RunExtraBuffCount(RUN_BUFF2_ROOF_PEAS) > 0;
}

void Projectile::UpdateNormalMotion()
{
    const bool followsRoof = FollowsRoofSlope(this);
    const float oldRoofY = followsRoof ? mBoard->GetPosYBasedOnRow(mPosX, mRow) : 0.0f;
	if (mMotionType == ProjectileMotion::MOTION_BACKWARDS)
	{
		mPosX -= 3.33f;
	}
	else if (mMotionType == ProjectileMotion::MOTION_HOMING)
	{
		Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
		if (aZombie && aZombie->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)))
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			SexyVector2 aTargetCenter(aZombie->ZombieTargetLeadX(0.0f), aZombieRect.mY + aZombieRect.mHeight / 2);
			SexyVector2 aProjectileCenter(mPosX + mWidth / 2, mPosY + mHeight / 2);
			SexyVector2 aToTarget = (aTargetCenter - aProjectileCenter).Normalize();
			SexyVector2 aMotion(mVelX, mVelY);

			aMotion += aToTarget * (0.001f * mProjectileAge);
			aMotion = aMotion.Normalize();
			aMotion *= 2.0f;

			mVelX = aMotion.x;
			mVelY = aMotion.y;
			mRotation = -atan2(mVelY, mVelX);
		}

		mPosY += mVelY;
		mPosX += mVelX;
		mShadowY += mVelY;
		mRow = mBoard->PixelToGridYKeepOnBoard(mPosX, mPosY);
	}
	else if (mMotionType == ProjectileMotion::MOTION_STAR)
	{
		mPosY += mVelY;
		mPosX += mVelX;
		mShadowY += mVelY;

		if (mVelY != 0.0f)
		{
			mRow = mBoard->PixelToGridYKeepOnBoard(mPosX, mPosY);
		}
	}
	else if (mMotionType == ProjectileMotion::MOTION_BEE)
	{
		if (mProjectileAge < 60)
		{
			mPosY -= 0.5f;
		}
		mPosX += 3.33f;
	}
	else if (mMotionType == ProjectileMotion::MOTION_FLOAT_OVER)
	{
		if (mVelZ < 0.0f)
		{
			mVelZ += 0.002f;
			mVelZ = std::min(mVelZ, 0.0f);
			mPosY += mVelZ;
			mRotation = 0.3f - 0.7f * mVelZ * PI * 0.25f;
		}
		mPosX += 0.4f;
	}
	else if (mMotionType == ProjectileMotion::MOTION_BEE_BACKWARDS)
	{
		if (mProjectileAge < 60)
		{
			mPosY -= 0.5f;
		}
		mPosX -= 3.33f;
	}
	else if (mMotionType == ProjectileMotion::MOTION_THREEPEATER)
	{
		mPosX += 3.33f;
		mPosY += mVelY;
		mVelY *= 0.97f;
		mShadowY += mVelY;
	}
	else
	{
		mPosX += 3.33f;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_HIGH_GRAVITY)
	{
		if (mMotionType == ProjectileMotion::MOTION_FLOAT_OVER)
		{
			mVelZ += 0.004f;
		}
		else
		{
			mVelZ += 0.2f;
		}

		mPosY += mVelZ;
	}

    // Move both the pea and its shadow before collision, including on the tick crossing the ridge.
    if (followsRoof) {
        const float slopeChange = mBoard->GetPosYBasedOnRow(mPosX, mRow) - oldRoofY;
        mPosY += slopeChange;
        mShadowY += slopeChange;
    }
	CheckForCollision();
	CheckForHighGround();
}

void Projectile::UpdateMotion()
{
	if (mAnimTicksPerFrame > 0)
	{
		mAnimCounter = (mAnimCounter + 1) % (mNumFrames * mAnimTicksPerFrame);
		mFrame = mAnimCounter / mAnimTicksPerFrame;
	}

	int aOldRow = mRow;
	float aOldY = mBoard->GetPosYBasedOnRow(mPosX, mRow);
	if (mMotionType == ProjectileMotion::MOTION_LOBBED)
	{
		UpdateLobMotion();
	}
	else
	{
		UpdateNormalMotion();
	}

	float aSlopeHeightChange = mBoard->GetPosYBasedOnRow(mPosX, aOldRow) - aOldY;
	if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		aSlopeHeightChange = 0.0f;  // Fix The Roof Offset Bug of Corn Cob
	}
	if (mMotionType == ProjectileMotion::MOTION_FLOAT_OVER)
	{
		mPosY += aSlopeHeightChange;
	}
	if (mMotionType == ProjectileMotion::MOTION_LOBBED)
	{
		mPosY += aSlopeHeightChange;
		mPosZ -= aSlopeHeightChange;
	}
	if (!FollowsRoofSlope(this)) mShadowY += aSlopeHeightChange; // Roof peas already moved before collision.
	mX = static_cast<int>(mPosX);
	mY = static_cast<int>(mPosY + mPosZ);
}

void Projectile::PlayImpactSound(Zombie* theZombie)
{
	bool aPlayHelmSound = true;
	bool aPlaySplatSound = true;
	if (mProjectileType == ProjectileType::PROJECTILE_KERNEL)
	{
		mApp->PlayFoley(FoleyType::FOLEY_KERNEL_SPLAT);
		aPlayHelmSound = false;
		aPlaySplatSound = false;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_BUTTER)
	{
		mApp->PlayFoley(FoleyType::FOLEY_BUTTER);
		aPlaySplatSound = false;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL && IsSplashDamage(theZombie))
	{
		mApp->PlayFoley(FoleyType::FOLEY_IGNITE);
		aPlayHelmSound = false;
		aPlaySplatSound = false;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_MELON || mProjectileType == ProjectileType::PROJECTILE_WINTERMELON)
	{
		mApp->PlayFoley(FoleyType::FOLEY_MELONIMPACT);
		aPlaySplatSound = false;
	}

	if (aPlayHelmSound && theZombie)
	{
		if (theZombie->mHelmType == HELMTYPE_PAIL)
		{
			mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
			aPlaySplatSound = false;
		}
		else if (theZombie->mHelmType == HELMTYPE_TRAFFIC_CONE || theZombie->mHelmType == HELMTYPE_DIGGER || theZombie->mHelmType == HELMTYPE_FOOTBALL)
		{
			mApp->PlayFoley(FoleyType::FOLEY_PLASTIC_HIT);
		}
	}

	if (aPlaySplatSound)
	{
		mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
	}
}

// @pvz-online: hunter retargeting is local, respects target immunities and the hit ID history.
Zombie* Projectile::FindHuntTarget()
{
    Zombie* best = nullptr;
    float bestDistance = 300.0f * 300.0f;
    Zombie* z = nullptr;
    while (mBoard->IterateZombies(z))
    {
        if (z->IsDeadOrDying() || WasZombiePricklyHit(z) || !z->EffectedByDamage((unsigned int)mDamageRangeFlags)) continue;
        Rect rect = z->GetZombieRect();
        float dx = rect.mX + rect.mWidth / 2 - (mPosX + 20);
        float dy = rect.mY + rect.mHeight / 2 - (mPosY + 20);
        float distance = dx * dx + dy * dy;
        if (distance < bestDistance) { bestDistance = distance; best = z; }
    }
    return best;
}

void Projectile::UpdatePurpleWave()
{
    DamageStats::Source aDamageSource(mApp->mDamageStats, true);
    ++mProjectileAge;
    mPosX += 6.0f;
    mX = (int)mPosX;
    mY = (int)mPosY;
    Zombie* z = nullptr;
    while (mBoard->IterateZombies(z))
    {
        if (z->IsDeadOrDying() || abs(z->mRow - mRow) > 1 || !z->EffectedByDamage((unsigned int)mDamageRangeFlags)) continue;
        unsigned int id = (unsigned int)mBoard->ZombieGetID(z);
        unsigned int slot = id & 0xFFFFU;
        if (slot >= RunWaveHitPacking::Slots || RunWaveHitPacking::Has(mRunWaveHitIds, id)) continue;
        Rect rect = z->GetZombieRect();
        if (rect.mX > mPosX + 35 || rect.mX + rect.mWidth < mPosX - 35) continue;
        RunWaveHitPacking::Record(mRunWaveHitIds, id);
        if (z->CanBeFrozen())
        {
            z->mIceTrapCounter = std::max(z->mIceTrapCounter, 100);
            z->UpdateAnimSpeed();
        }
        z->TakeDamage((int)(240 * mApp->RunBuffMul(RUN_BUFF_FIREPOWER) + 0.5f), (unsigned int)DamageFlags::DAMAGE_HITS_SHIELD_AND_BODY);
    }
    if (mPosX > 850 || mProjectileAge > 150) Die();
}

void Projectile::DoImpact(Zombie* theZombie)
{
    DamageStats::Source aDamageSource(mApp->mDamageStats, true);
	PlayImpactSound(theZombie);

	// @pvz-online: split only original Snow Pea shots; piercing collision dedup prevents repeats.
	if (mRunIceSource && mApp->RunExtraBuffCount(RUN_BUFF2_ICE_CRYSTAL) > 0 &&
		(theZombie || mProjectileType == ProjectileType::PROJECTILE_WINTERMELON))
	{
		int aCol = mBoard->PixelToGridXKeepOnBoard(mPosX, mPosY);
		for (int r = mRow - 1; r <= mRow + 1; r += 2)
		{
			if (!mBoard->RowCanHaveZombies(r)) continue;
			int y = mBoard->GridToPixelY(aCol, r) + 25;
			Projectile* aCrystal = mBoard->AddProjectile((int)mPosX, y, mRenderOrder, r, ProjectileType::PROJECTILE_SNOWPEA);
			if (aCrystal == nullptr) continue;
			aCrystal->mDamageRangeFlags = 13;
			aCrystal->mRunCrystalDamage = std::max(1, GetProjectileDef().mDamage / 2);
		}
	}
	if (theZombie && mRunCactusSource && mApp->RunExtraBuffCount(RUN_BUFF2_CACTUS_SLOW) > 0 && theZombie->CanBeChilled())
		theZombie->mRunBarbedCounter = std::max(theZombie->mRunBarbedCounter, theZombie->SoftControlDuration(300));


	if (IsSplashDamage(theZombie))
	{
		if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL && theZombie)
		{
			theZombie->RemoveColdEffects();
		}

		DoSplashDamage(theZombie);
	}
	else if (theZombie)
	{
		unsigned int aDamageFlags = GetDamageFlags(theZombie);
		// @pvz-online: 闯关 buff「火力」：单体命中伤害按倍率放大（溅射那条同上，在 DoSplashDamage 里）。
		int aDamage = (int)((mRunCrystalDamage > 0 ? mRunCrystalDamage : GetProjectileDef().mDamage) * mApp->RunBuffMul(RUN_BUFF_FIREPOWER) + 0.5f);
		// @pvz-online: 单株升级「Kindling」（火炬树桩）：火弹打抗火僵尸（投石车/冰车等
		// IsSplashDamage 为假）走这条单发路径，同样乘（helper 注释）。
		aDamage = KindlingFireballDamage(mApp, mProjectileType, aDamage);
		// @pvz-online: 第二 buff 批 2「重投」（卷心菜投手，docs/06 §8.6 #32）：单发伤害
		// +50%/层（1★，cap3）。2026-10-10 实机修复（用户报「伤害 buff 不生效」）：旧挂点
		// 在 DoSplashDamage，而卷心菜弹非溅射弹（IsSplashDamage 只认西瓜/冰西瓜/火弹），
		// 那份乘算对卷心菜从零执行；挪到本条单发分支尾（乘序=火力→火炬→重投，与旧口径一致）。
		if (mProjectileType == ProjectileType::PROJECTILE_CABBAGE)
		{
			aDamage = (int)(aDamage * mApp->RunPlantBuff2Mul(SeedType::SEED_CABBAGEPULT) + 0.5f);
		}
		theZombie->TakeDamage(aDamage, aDamageFlags, GetRunFixedDamageBonus(theZombie));
	}

	float aLastPosX = mPosX - mVelX;
	float aLastPosY = mPosY + mPosZ - mVelY - mVelZ;
	ParticleEffect aEffect = ParticleEffect::PARTICLE_NONE;
	float aSplatPosX = mPosX + 12.0f;
	float aSplatPosY = mPosY + 12.0f;
	if (mProjectileType == ProjectileType::PROJECTILE_MELON)
	{
		mApp->AddPvzpParticle(aLastPosX + 30.0f, aLastPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_MELONSPLASH);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_WINTERMELON)
	{
		mApp->AddPvzpParticle(aLastPosX + 30.0f, aLastPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_WINTERMELON);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		int aRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, mCobTargetRow, 2);
		mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 40.0f, aRenderOrder, ParticleEffect::PARTICLE_BLASTMARK);
		mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 40.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_POPCORNSPLASH);
		mApp->PlaySample(SOUND_DOOMSHROOM);
		mBoard->ShakeBoard(3, -4);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_PEA)
	{
		aSplatPosX -= 15.0f;
		aEffect = ParticleEffect::PARTICLE_PEA_SPLAT;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_SNOWPEA)
	{
		aSplatPosX -= 15.0f;
		aEffect = ParticleEffect::PARTICLE_SNOWPEA_SPLAT;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL)
	{
		if (IsSplashDamage(theZombie))
		{
			Reanimation* aFireReanim = mApp->AddReanimation(mPosX + 38.0f, mPosY - 20.0f, mRenderOrder + 1, ReanimationType::REANIM_JALAPENO_FIRE);
			if (aFireReanim)
			{
				aFireReanim->mAnimTime = 0.25f;
				aFireReanim->mAnimRate = 24.0f;
				aFireReanim->OverrideScale(0.7f, 0.4f);
			}
		}
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_STAR)
	{
		aEffect = ParticleEffect::PARTICLE_STAR_SPLAT;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_PUFF)
	{
		aSplatPosX -= 20.0f;
		aEffect = ParticleEffect::PARTICLE_PUFF_SPLAT;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_CABBAGE)
	{
		aSplatPosX = aLastPosX - 38.0f;
		aSplatPosY = aLastPosY + 23.0f;
		aEffect = ParticleEffect::PARTICLE_CABBAGE_SPLAT;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_BUTTER)
	{
		aSplatPosX = aLastPosX - 20.0f;
		aSplatPosY = aLastPosY + 63.0f;
		aEffect = ParticleEffect::PARTICLE_BUTTER_SPLAT;

		if (theZombie)
		{
			// @pvz-online: 第二 buff「Butter Feast」黄油盛宴（玉米投手 #34，docs/06 §8.6，2★
			// cap2）：黄油时长 400 + 100×层 帧（+1 秒/层；命中率的 +25%/层 在投手发射端起骰，
			// 见 Plant::FindTargetAndFire 的 KERNELPULT 块）。未持条时 +0，与原版默认一致。
			RunDotEffects::ButterHit(theZombie, 400 + 100 * mApp->RunPlantBuff2Count(SeedType::SEED_KERNELPULT),
                800 + 200 * mApp->RunPlantBuff2Count(SeedType::SEED_KERNELPULT));
			// @pvz-online: extra butter is a status event, never another projectile or recursive splash.
			if (mApp->RunExtraBuffCount(RUN_BUFF2_BUTTER_SPLASH) > 0)
			{
				Zombie* aTargets[3] = {};
				float aDistances[3] = { 1e9f, 1e9f, 1e9f };
				Rect aMainRect = theZombie->GetZombieRect();
				float aCenter = aMainRect.mX + aMainRect.mWidth / 2.0f;
				Zombie* aOther = nullptr;
				while (mBoard->IterateZombies(aOther))
				{
					if (aOther == theZombie || !aOther->mHasHead || aOther->IsFlying() || aOther->IsTangleKelpTarget() ||
						aOther->IsBobsledTeamWithSled() || (!aOther->CanBeFrozen() && !(mApp->RunExtraBuffCount(RUN_BUFF2_BUTTER_HEAVY) > 0 && RunDotEffects::CanCoat(aOther))) || !aOther->EffectedByDamage(mDamageRangeFlags)) continue;
					Rect aRect = aOther->GetZombieRect();
					float dx = fabsf(aRect.mX + aRect.mWidth / 2.0f - aCenter);
					if (abs(aOther->mRow - theZombie->mRow) > 1 || dx > 80.0f) continue;
					float d = dx + 80.0f * abs(aOther->mRow - theZombie->mRow);
					for (int i = 0; i < 3; ++i) if (d < aDistances[i])
					{
						for (int j = 2; j > i; --j) { aTargets[j] = aTargets[j - 1]; aDistances[j] = aDistances[j - 1]; }
						aTargets[i] = aOther; aDistances[i] = d; break;
					}
				}
				// @pvz-online: a short splash must not shorten a longer direct butter stun already in progress.
				for (int i = 0; i < 3; ++i)
					if (aTargets[i]) RunDotEffects::ButterHit(aTargets[i], 200, 200);
			}

		}
	}

	if (aEffect != ParticleEffect::PARTICLE_NONE)
	{
		if (theZombie)
		{
			float aPosX = aSplatPosX + 52.0f - theZombie->mX;
			float aPosY = aSplatPosY - theZombie->mY;
			if (theZombie->mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL || theZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL)
			{
				aPosY += 60.0f;
			}
			if (mMotionType == ProjectileMotion::MOTION_BACKWARDS)
			{
				aPosX -= 80.0f;
			}
			else if (mPosX > theZombie->mX + 40 && mMotionType != ProjectileMotion::MOTION_LOBBED)
			{
				aPosX -= 60.0f;
			}

			aPosY = std::clamp(aPosY, 20.0f, 100.0f);
			theZombie->AddAttachedParticle(aPosX, aPosY, aEffect);
		}
		else
		{
			mApp->AddPvzpParticle(aSplatPosX, aSplatPosY, mRenderOrder + 1, aEffect);
		}
	}

	// @pvz-online: 单株升级「Prickly」（仙人掌，表行 SEED_CACTUS）：尖刺穿透。三态：
	// 0 = 无条（原版行为）、-1 = 无限穿透（2026-10-09 晚实机反馈起；旧 +1 只/层、cap 2
	// 的计数型已废）、>0 = 有限计数（兼容保留）。mPricklyHitsLeft 在 Fire 里预置；
	// 有限计数扣完才 Die。2026-10-10 起：命中去重走 16 槽 ID 环（RecordPricklyHit 记账、
	// FindCollisionTarget 跳环内僵尸），弹体位置不再动——旧版「推到命中者右缘 +30px」
	// 的绝对传送会把弹跳过贴身/重叠的下一只（弹与它背向而行永不再相遇，实机反馈
	// 「穿透弹无法同时命中靠近的僵尸」），已废。直射弹的阴影 Y 本来就不跟弹体走，不动。
    // @pvz-online: one larger needle hits up to three distinct enemies; never duplicates itself.
    if (mRunHuntHitsLeft > 0 && theZombie)
    {
        RecordPricklyHit(theZombie);
        --mRunHuntHitsLeft;
        Zombie* next = mRunHuntHitsLeft > 0 ? FindHuntTarget() : nullptr;
        if (next) mTargetZombieID = mBoard->ZombieGetID(next);
        else Die();
        return;
    }
	if (mPricklyHitsLeft != 0 && theZombie)
	{
		if (mPricklyHitsLeft > 0) mPricklyHitsLeft--;
		RecordPricklyHit(theZombie);
		return;
	}

	// @pvz-online: 第二 buff「滚爆」（西瓜投手 #39，docs/06 §8.6）：落地后向前滚 1 格
	//（80px）并再炸一次——二次爆走直伤通道（KillAllZombiesInRadius 半径 50，伤害 = 弹
	// 基数 ×火力放大，无溅射递减），两个落点各放一片西瓜溅射粒子。只认 MELON 本体弹
	//（冰西瓜不走这条）；词条未抽取时短路。
	// 2026-10-10 实机修复（报「后面爆的一次没伤害」）两处：①判定旗标必须用弹体自己的
	// mDamageRangeFlags——旧写 GetDamageFlags() 塞来的是 TakeDamage 空间的位（西瓜 = 2，
	// 位 1），进 EffectedByDamage 等于只点亮 DAMAGES_FLYING、缺 DAMAGES_GROUND（位 0），
	// 收尾判定 TestBit(GROUND) 恒假 → 地面僵尸全被过滤，只有气球挨得着；土豆雷 77 /
	// 亡语 9 / 加农炮 127 都带位 0，唯独这处没有。②圆心 Y 从粒子锚（上一帧弹顶）挪到
	// 弹体身体中心（+mHeight/2 = +20）——旧值离被命中者矩形顶只剩 ~3.5px 裕量，中心化
	// 后 ~30px，与溅射粒子画在身体中心的视觉一致。
	if (mProjectileType == ProjectileType::PROJECTILE_MELON && mRunSourceSeed == SeedType::SEED_MELONPULT && mApp->RunPlantBuff2Count(SeedType::SEED_MELONPULT) > 0)
	{
		int aRollX = (int)(mPosX + 80.0f);
		int aRollY = (int)(mPosY + mPosZ) + mHeight / 2;
		int aRollFlags = mDamageRangeFlags;
		int aRollDamage = (int)((mRunCrystalDamage > 0 ? mRunCrystalDamage : GetProjectileDef().mDamage) * mApp->RunBuffMul(RUN_BUFF_FIREPOWER) + 0.5f);
		mBoard->KillAllZombiesInRadius(mRow, aRollX, aRollY, 50, 0, false, aRollFlags, aRollDamage);
		mApp->AddPvzpParticle(aRollX + 30.0f, mPosY + mPosZ - mVelY - mVelZ + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_MELONSPLASH);
	}

	Die();
}

void Projectile::Update()
{
    DamageStats::Source aDamageSource(mApp->mDamageStats, true);
    // @pvz-online: delayed salvos use the normal cob trajectory once released.
    if (mRunLaunchDelay > 0)
    {
        if (mApp->mGameScene == GameScenes::SCENE_PLAYING) --mRunLaunchDelay;
        return;
    }
    if (mRunIceWave)
    {
        if (mApp->mGameScene == GameScenes::SCENE_PLAYING) UpdatePurpleWave();
        return;
    }
	mProjectileAge++;
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mBoard->mCutScene->ShouldRunUpsellBoard())
		return;

	int aTime = 20;
	if (mProjectileType == ProjectileType::PROJECTILE_PEA ||
		mProjectileType == ProjectileType::PROJECTILE_SNOWPEA ||
		mProjectileType == ProjectileType::PROJECTILE_CABBAGE ||
		mProjectileType == ProjectileType::PROJECTILE_MELON ||
		mProjectileType == ProjectileType::PROJECTILE_WINTERMELON ||
		mProjectileType == ProjectileType::PROJECTILE_KERNEL ||
		mProjectileType == ProjectileType::PROJECTILE_BUTTER ||
		mProjectileType == ProjectileType::PROJECTILE_COBBIG ||
		mProjectileType == ProjectileType::PROJECTILE_ZOMBIE_PEA ||
		mProjectileType == ProjectileType::PROJECTILE_SPIKE)
	{
		aTime = 0;
	}
	if (mProjectileAge > aTime)
	{
		mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, mRow, 0);
	}

	if (mClickBackoffCounter > 0)
	{
		mClickBackoffCounter--;
	}
	mRotation += mRotationSpeed;

	UpdateMotion();
	AttachmentUpdateAndMove(mAttachmentID, mPosX, mPosY + mPosZ);
}

void Projectile::Draw(Graphics* g)
{
    if (mRunLaunchDelay > 0) return;
    if (mRunIceWave)
    {
        // @pvz-online: bounded geometry makes a visible three-row ice front without particle spam.
        Color old = g->GetColor();
        int col = mBoard->PixelToGridXKeepOnBoard(mPosX, mPosY);
        for (int row = mRow - 1; row <= mRow + 1; ++row)
        {
            if (!mBoard->RowCanHaveZombies(row)) continue;
            int y = mBoard->GridToPixelY(col, row) + 55 - mY;
            g->SetColor(Color(80, 175, 255, 130));
            g->FillRect(-45, y - 30, 65, 45);
            g->SetColor(Color(205, 245, 255, 230));
            for (int i = 0; i < 4; ++i)
            {
                int x = -35 + i * 16;
                int h = 22 + (i % 2) * 18;
                g->DrawLine(x, y, x + 8, y - h);
                g->DrawLine(x + 8, y - h, x + 16, y);
            }
        }
        g->SetColor(old);
        return;
    }

	const ProjectileDefinition& aProjectileDef = GetProjectileDef();

	Image* aImage = nullptr;
	float aScale = 1.0f;
	switch (mProjectileType)
	{
	case ProjectileType::PROJECTILE_COBBIG:
		aImage = IMAGE_REANIM_COBCANNON_COB;
		aScale = 0.9f;
		break;
	case ProjectileType::PROJECTILE_PEA:
	case ProjectileType::PROJECTILE_ZOMBIE_PEA:
		aImage = IMAGE_PROJECTILEPEA;
		break;
	case ProjectileType::PROJECTILE_SNOWPEA:
		aImage = IMAGE_PROJECTILESNOWPEA;
		break;
	case ProjectileType::PROJECTILE_FIREBALL:
		aImage = !FindReanimAttachment(mAttachmentID) ? IMAGE_PROJECTILEPEA : nullptr;
		break;
	case ProjectileType::PROJECTILE_SPIKE:
		aImage = IMAGE_PROJECTILECACTUS;
		break;
	case ProjectileType::PROJECTILE_STAR:
		aImage = IMAGE_PROJECTILE_STAR;
		break;
	case ProjectileType::PROJECTILE_PUFF:
		aImage = IMAGE_PUFFSHROOM_PUFF1;
		aScale = PvzpAnimateCurveFloat(0, 30, mProjectileAge, 0.3f, 1.0f, PvzpCurves::CURVE_LINEAR);
		break;
	case ProjectileType::PROJECTILE_BASKETBALL:
		aImage = IMAGE_REANIM_ZOMBIE_CATAPULT_BASKETBALL;
		aScale = 1.1f;
		break;
	case ProjectileType::PROJECTILE_CABBAGE:
		aImage = IMAGE_REANIM_CABBAGEPULT_CABBAGE;
		aScale = 1.0f;
		break;
	case ProjectileType::PROJECTILE_KERNEL:
		aImage = IMAGE_REANIM_CORNPULT_KERNAL;
		aScale = 0.95f;
		break;
	case ProjectileType::PROJECTILE_BUTTER:
		aImage = IMAGE_REANIM_CORNPULT_BUTTER;
		aScale = 0.8f;
		break;
	case ProjectileType::PROJECTILE_MELON:
		aImage = IMAGE_REANIM_MELONPULT_MELON;
		aScale = 1.0f;
		break;
	case ProjectileType::PROJECTILE_WINTERMELON:
		aImage = IMAGE_REANIM_WINTERMELON_PROJECTILE;
		aScale = 1.0f;
		break;
	default:
		PVZP_ASSERT(false);
		break;
	}

    if (mRunHuntHitsLeft > 0) aScale *= 1.7f;
	bool aMirror = false;
	if (mMotionType == ProjectileMotion::MOTION_BEE_BACKWARDS)
	{
		aMirror = true;
	}

	if (aImage)
	{
		PVZP_ASSERT(aProjectileDef.mImageRow < aImage->mNumRows);
		PVZP_ASSERT(mFrame < aImage->mNumCols);

		int aCelWidth = aImage->GetCelWidth();
		int aCelHeight = aImage->GetCelHeight();
		Rect aSrcRect(aCelWidth * mFrame, aCelHeight * aProjectileDef.mImageRow, aCelWidth, aCelHeight);
		if (FloatApproxEqual(mRotation, 0.0f) && FloatApproxEqual(aScale, 1.0f))
		{
			Rect aDestRect(0, 0, aCelWidth, aCelHeight);
			g->DrawImageMirror(aImage, aDestRect, aSrcRect, aMirror);
		}
		else
		{
			float aOffsetX = mPosX + aCelWidth * 0.5f;
			float aOffsetY = mPosZ + mPosY + aCelHeight * 0.5f;
			SexyTransform2D aTransform;
			PvzpScaleRotateTransformMatrix(aTransform, aOffsetX + mBoard->mX, aOffsetY + mBoard->mY, mRotation, aScale, aScale);
			PvzpBltMatrix(g, aImage, aTransform, g->mClipRect, Color::White, g->mDrawMode, aSrcRect);
		}
	}

	if (mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
	{
		Graphics theParticleGraphics(*g);
		MakeParentGraphicsFrame(&theParticleGraphics);
		AttachmentDraw(mAttachmentID, &theParticleGraphics, false);
	}
}

void Projectile::DrawShadow(Graphics* g)
{
    if (mRunLaunchDelay > 0 || mRunIceWave) return;
	int aCelCol = 0;
	float aScale = 1.0f;
	float aStretch = 1.0f;
	float aOffsetX = mPosX - mX;
	float aOffsetY = mPosY - mY;

	int aGridX = mBoard->PixelToGridXKeepOnBoard(mX, mY);
	bool isHighGround = false;
	if (mBoard->mGridSquareType[aGridX][mRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND)
	{
		isHighGround = true;
	}
	if (mOnHighGround && !isHighGround)
	{
		aOffsetY += HIGH_GROUND_HEIGHT;
	}
	else if (!mOnHighGround && isHighGround)
	{
		aOffsetY -= HIGH_GROUND_HEIGHT;
	}

	if (mBoard->StageIsNight())
	{
		aCelCol = 1;
	}

	switch (mProjectileType)
	{
	case ProjectileType::PROJECTILE_PEA:
	case ProjectileType::PROJECTILE_ZOMBIE_PEA:
		aOffsetX += 3.0f;
		break;

	case ProjectileType::PROJECTILE_SNOWPEA:
		aOffsetX += -1.0f;
		aScale = 1.3f;
		break;

	case ProjectileType::PROJECTILE_STAR:
		aOffsetX += 7.0f;
		break;

	case ProjectileType::PROJECTILE_CABBAGE:
	case ProjectileType::PROJECTILE_KERNEL:
	case ProjectileType::PROJECTILE_BUTTER:
	case ProjectileType::PROJECTILE_MELON:
	case ProjectileType::PROJECTILE_WINTERMELON:
		aOffsetX += 3.0f;
		aOffsetY += 10.0f;
		aScale = 1.6f;
		break;

	case ProjectileType::PROJECTILE_PUFF:
		return;

	case ProjectileType::PROJECTILE_COBBIG:
		aScale = 1.0f;
		aStretch = 3.0f;
		aOffsetX += 57.0f;
		break;

	case ProjectileType::PROJECTILE_FIREBALL:
		aScale = 1.4f;
		break;
	default:
		break;
	}

	if (mMotionType == ProjectileMotion::MOTION_LOBBED)
	{
		float aHeight = std::clamp(-mPosZ, 0.0f, 200.0f);
		aScale *= 200.0f / (aHeight + 200.0f);
	}

	PvzpDrawImageCelScaledF(g, IMAGE_PEA_SHADOWS, aOffsetX, (mShadowY - mPosY + aOffsetY), aCelCol, 0, aScale * aStretch, aScale);
}

void Projectile::Die()
{
	mDead = true;

	if (mProjectileType == ProjectileType::PROJECTILE_PUFF || mProjectileType == ProjectileType::PROJECTILE_SNOWPEA)
	{
		AttachmentCrossFade(mAttachmentID, "FadeOut");
		AttachmentDetach(mAttachmentID);
	}
	else
	{
		AttachmentDie(mAttachmentID);
	}
}

Rect Projectile::GetProjectileRect()
{
	if (mProjectileType == ProjectileType::PROJECTILE_PEA ||
		mProjectileType == ProjectileType::PROJECTILE_SNOWPEA ||
		mProjectileType == ProjectileType::PROJECTILE_ZOMBIE_PEA)
	{
		return Rect(mX - 15, mY, mWidth + 15, mHeight);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		return Rect(mX + mWidth / 2 - 115, mY + mHeight / 2 - 115, 230, 230);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_MELON || mProjectileType == ProjectileType::PROJECTILE_WINTERMELON)
	{
		return Rect(mX + 20, mY, 60, mHeight);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL)
	{
		return Rect(mX, mY, mWidth - 10, mHeight);
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_SPIKE)
	{
		return Rect(mX - 25, mY, mWidth + 25, mHeight);
	}
	else
	{
		return Rect(mX, mY, mWidth, mHeight);
	}
}

void Projectile::ConvertToFireball(int theGridX)
{
	if (mHitTorchwoodGridX == theGridX)
		return;

	mProjectileType = ProjectileType::PROJECTILE_FIREBALL;
	mHitTorchwoodGridX = theGridX;
	mApp->PlayFoley(FoleyType::FOLEY_FIREPEA);

	float aOffsetX = -25.0f;
	float aOffsetY = -25.0f;
	Reanimation* aFirePeaReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FIRE_PEA);
	if (!aFirePeaReanim) return; // Damage/type conversion above still takes effect.
	if (mMotionType == ProjectileMotion::MOTION_BACKWARDS)
	{
		aFirePeaReanim->OverrideScale(-1.0f, 1.0f);
		aOffsetX += 80.0f;
	}

	aFirePeaReanim->SetPosition(mPosX + aOffsetX, mPosY + aOffsetY);
	aFirePeaReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
	aFirePeaReanim->mAnimRate = RandRangeFloat(50.0f, 80.0f);
	AttachReanim(mAttachmentID, aFirePeaReanim, aOffsetX, aOffsetY);
}

void Projectile::ConvertToPea(int theGridX)
{
	if (mHitTorchwoodGridX == theGridX)
		return;

	AttachmentDie(mAttachmentID);
	mProjectileType = ProjectileType::PROJECTILE_PEA;
	mHitTorchwoodGridX = theGridX;
	mApp->PlayFoley(FoleyType::FOLEY_THROW);
}

const ProjectileDefinition& Projectile::GetProjectileDef()
{
	const ProjectileDefinition& aProjectileDef = gProjectileDefinition[mProjectileType];
	PVZP_ASSERT(aProjectileDef.mProjectileType == mProjectileType);

	return aProjectileDef;
}
