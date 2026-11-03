#include "Lawn/Observer/ObserverIsolation.h"
#include "../Run/RunSpeciesRuntime.h"
#include "Music.h"
#include "SaveGame.h"
#include "../Board.h"
#include "../Run/RunDotEffects.h"
#include "../Run/RunWaveHitSave.h"
#include <type_traits>
#include <vector>
#include <unordered_set>
#include "../Run/WaveHealthSave.h"
#include "../Run/WaveSpawnRetrySave.h"
#include "../Run/DeferredZombieSpawnSave.h"
#include "../Run/DeferredPlantSetupSave.h"
#include "../Widget/NextWaveControl.h"
#include "../Challenge.h"
#include "../SeedPacket.h"
#include "../../LawnApp.h"
#include "../CursorObject.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "../MessageWidget.h"
#include "../../Sexy.TodLib/Trail.h"
#include "zlib/zlib.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include <algorithm>
#include <vector>
#include <climits>

static const char* FILE_COMPILE_TIME_STRING = "Feb 16 200923:03:38";
static const unsigned int SAVE_FILE_MAGIC_NUMBER = 0xFEEDDEAD;
// @pvz-online: new projectile source state cannot safely read old raw snapshot padding.
static const unsigned int SAVE_FILE_VERSION = 5U; // @pvz-online: Plant/Projectile/Coin purple ability snapshot layout.
static unsigned int SAVE_FILE_DATE = crc32(0, (Bytef*)FILE_COMPILE_TIME_STRING, strlen(FILE_COMPILE_TIME_STRING));  //[0x6AA7EC]

//0x4813D0
void SaveGameContext::SyncBytes(void* theDest, int theReadSize)
{
	int aReadSize = theReadSize;
	if (mReading)
	{
		if ((unsigned long)ByteLeftToRead() < 4)
		{
			mFailed = true;
		}

		aReadSize = mFailed ? 0 : mBuffer.ReadLong();
	}
	else
	{
		mBuffer.WriteLong(theReadSize);
	}

	if (mReading)
	{
		if (aReadSize != theReadSize || ByteLeftToRead() < theReadSize)
		{
			mFailed = true;
		}

		if (mFailed)
		{
			memset(theDest, 0, theReadSize);
		}
		else
		{
			mBuffer.ReadBytes((uchar*)theDest, theReadSize);
		}
	}
	else
	{
		mBuffer.WriteBytes((uchar*)theDest, theReadSize);
	}
}

//0x481470
void SaveGameContext::SyncInt(int& theInt)
{
	if (mReading)
	{
		if ((unsigned long)ByteLeftToRead() < 4)
		{
			mFailed = true;
		}

		theInt = mFailed ? 0 : mBuffer.ReadLong();
	}
	else
	{
		mBuffer.WriteLong(theInt);
	}
}

//0x4814C0
void SaveGameContext::SyncReanimationDef(ReanimatorDefinition*& theDefinition)
{
	if (mReading)
	{
		int aReanimType;
		SyncInt(aReanimType);
		if (aReanimType == (int)ReanimationType::REANIM_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aReanimType >= 0 && aReanimType < (int)ReanimationType::NUM_REANIMS)
		{
			ReanimatorEnsureDefinitionLoaded((ReanimationType)aReanimType, true);
			theDefinition = &gReanimatorDefArray[aReanimType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aReanimType = (int)ReanimationType::REANIM_NONE;
		for (int i = 0; i < (int)ReanimationType::NUM_REANIMS; i++)
		{
			ReanimatorDefinition* aDef = &gReanimatorDefArray[i];
			if (theDefinition == aDef)
			{
				aReanimType = i;
				break;
			}
		}
		SyncInt(aReanimType);
	}
}

//0x481560
void SaveGameContext::SyncParticleDef(TodParticleDefinition*& theDefinition)
{
	if (mReading)
	{
		int aParticleType;
		SyncInt(aParticleType);
		if (aParticleType == (int)ParticleEffect::PARTICLE_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aParticleType >= 0 && aParticleType < (int)ParticleEffect::NUM_PARTICLES)
		{
			theDefinition = &gParticleDefArray[aParticleType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aParticleType = (int)ParticleEffect::PARTICLE_NONE;
		for (int i = 0; i < (int)ParticleEffect::NUM_PARTICLES; i++)
		{
			TodParticleDefinition* aDef = &gParticleDefArray[i];
			if (theDefinition == aDef)
			{
				aParticleType = i;
				break;
			}
		}
		SyncInt(aParticleType);
	}
}

//0x4815F0
void SaveGameContext::SyncTrailDef(TrailDefinition*& theDefinition)
{
	if (mReading)
	{
		int aTrailType;
		SyncInt(aTrailType);
		if (aTrailType == TrailType::TRAIL_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aTrailType >= 0 && aTrailType < TrailType::NUM_TRAILS)
		{
			theDefinition = &gTrailDefArray[aTrailType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aTrailType = TrailType::TRAIL_NONE;
		for (int i = 0; i < TrailType::NUM_TRAILS; i++)
		{
			TrailDefinition* aDef = &gTrailDefArray[i];
			if (theDefinition == aDef)
			{
				aTrailType = i;
				break;
			}
		}
		SyncInt(aTrailType);
	}
}

//0x481690
void SaveGameContext::SyncImage(Image*& theImage)
{
	if (mReading)
	{
		ResourceId aResID;
		SyncInt((int&)aResID);
		if (aResID == Sexy::ResourceId::RESOURCE_ID_MAX)
		{
			theImage = nullptr;
		}
		else
		{
			theImage = GetImageById(aResID);
		}
	}
	else
	{
		ResourceId aResID;
		if (theImage != nullptr)
		{
			aResID = GetIdByImage(theImage);
		}
		else
		{
			aResID = Sexy::ResourceId::RESOURCE_ID_MAX;
		}
		SyncInt((int&)aResID);
	}
}

//0x481710
void SyncDataIDList(TodList<unsigned int>* theDataIDList, SaveGameContext& theContext, TodAllocator* theAllocator)
{
	try
	{
		if (theContext.mReading)
		{
			if (theDataIDList)
			{
				theDataIDList->mHead = nullptr;
				theDataIDList->mTail = nullptr;
				theDataIDList->mSize = 0;
				theDataIDList->SetAllocator(theAllocator);
			}

			int aCount;
			theContext.SyncInt(aCount);
			for (int i = 0; i < aCount; i++)
			{
				unsigned int aDataID;
				theContext.SyncBytes(&aDataID, sizeof(aDataID));
				theDataIDList->AddTail(aDataID);
			}
		}
		else
		{
			int aCount = theDataIDList->mSize;
			theContext.SyncInt(aCount);
			for (TodListNode<unsigned int>* aNode = theDataIDList->mHead; aNode != nullptr; aNode = aNode->mNext)
			{
				unsigned int aDataID = aNode->mValue;
				theContext.SyncBytes(&aDataID, sizeof(aDataID));
			}
		}
	}
	catch (std::exception&)
	{
		return;
	}
}

//0x4817C0
void SyncParticleEmitter(TodParticleSystem* theParticleSystem, TodParticleEmitter* theParticleEmitter, SaveGameContext& theContext)
{
	int aEmitterDefIndex = 0;
	if (theContext.mReading)
	{
		theContext.SyncInt(aEmitterDefIndex);
		theParticleEmitter->mParticleSystem = theParticleSystem;
		theParticleEmitter->mEmitterDef = &theParticleSystem->mParticleDef->mEmitterDefs[aEmitterDefIndex];
	}
	else
	{
		aEmitterDefIndex = ((intptr_t)theParticleEmitter->mEmitterDef - (intptr_t)theParticleSystem->mParticleDef->mEmitterDefs) / sizeof(TodEmitterDefinition);
		theContext.SyncInt(aEmitterDefIndex);
	}

	theContext.SyncImage(theParticleEmitter->mImageOverride);
	SyncDataIDList((TodList<unsigned int>*)&theParticleEmitter->mParticleList, theContext, &theParticleSystem->mParticleHolder->mParticleListNodeAllocator);
	for (TodListNode<ParticleID>* aNode = theParticleEmitter->mParticleList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		TodParticle* aParticle = theParticleSystem->mParticleHolder->mParticles.DataArrayGet((unsigned int)aNode->mValue);
		if (theContext.mReading)
		{
			aParticle->mParticleEmitter = theParticleEmitter;
		}
	}
}

//0x481880
void SyncParticleSystem(Board* theBoard, TodParticleSystem* theParticleSystem, SaveGameContext& theContext)
{
	theContext.SyncParticleDef(theParticleSystem->mParticleDef);
	if (theContext.mReading)
	{
		theParticleSystem->mParticleHolder = theBoard->mApp->mEffectSystem->mParticleHolder;
	}

	SyncDataIDList((TodList<unsigned int>*)&theParticleSystem->mEmitterList, theContext, &theParticleSystem->mParticleHolder->mEmitterListNodeAllocator);
	for (TodListNode<ParticleEmitterID>* aNode = theParticleSystem->mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		TodParticleEmitter* aEmitter = theParticleSystem->mParticleHolder->mEmitters.DataArrayGet((unsigned int)aNode->mValue);
		SyncParticleEmitter(theParticleSystem, aEmitter, theContext);
	}
}

//0x4818F0
void SyncReanimation(Board* theBoard, Reanimation* theReanimation, SaveGameContext& theContext)
{
	theContext.SyncReanimationDef(theReanimation->mDefinition);
	if (theContext.mReading)
	{
		theReanimation->mReanimationHolder = theBoard->mApp->mEffectSystem->mReanimationHolder;
	}

	if (theReanimation->mDefinition->mTracks.count != 0)
	{
		int aSize = theReanimation->mDefinition->mTracks.count * sizeof(ReanimatorTrackInstance);
		if (theContext.mReading)
		{
			theReanimation->mTrackInstances = (ReanimatorTrackInstance*)FindGlobalAllocator(aSize)->Calloc(aSize);
		}
		theContext.SyncBytes(theReanimation->mTrackInstances, aSize);

		for (int aTrackIndex = 0; aTrackIndex < theReanimation->mDefinition->mTracks.count; aTrackIndex++)
		{
			ReanimatorTrackInstance& aTrackInstance = theReanimation->mTrackInstances[aTrackIndex];
			theContext.SyncImage(aTrackInstance.mImageOverride);

			if (theContext.mReading)
			{
				aTrackInstance.mBlendTransform.mText = "";
				TOD_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mImage == nullptr);
			}
			else
			{
				TOD_ASSERT(aTrackInstance.mBlendTransform.mText[0] == NULL);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mImage == nullptr);
			}
		}
	}
}

void SyncTrail(Board* theBoard, Trail* theTrail, SaveGameContext& theContext)
{
	theContext.SyncTrailDef(theTrail->mDefinition);
	if (theContext.mReading)
	{
		theTrail->mTrailHolder = theBoard->mApp->mEffectSystem->mTrailHolder;
	}
}

template <typename T> inline static void SyncDataArray(SaveGameContext& theContext, DataArray<T>& theDataArray)
{
	if (theContext.mFailed) return;
	unsigned int aFree = theDataArray.mFreeListHead, aUsed = theDataArray.mMaxUsedCount, aSize = theDataArray.mSize;
	theContext.SyncUint(aFree);
	theContext.SyncUint(aUsed);
	theContext.SyncUint(aSize);
	if (theContext.mFailed || aUsed > theDataArray.mMaxSize || aSize > aUsed || aFree > aUsed)
	{ theContext.mFailed = true; return; }
	const size_t aBytes = size_t(aUsed) * sizeof(*theDataArray.mBlock);
	if (aBytes > INT_MAX || (theContext.mReading &&
		(theContext.ByteLeftToRead() < 4 || aBytes > size_t(theContext.ByteLeftToRead() - 4))))
	{ theContext.mFailed = true; return; }
	if constexpr (std::is_same_v<T, Projectile>) {
        if (!theContext.mReading) {
            // SyncBytes prefixes the entire pool once. Keep that exact framing.
            theContext.mBuffer.WriteLong(static_cast<int>(aBytes));
            for (unsigned int i = 0; i < aUsed; ++i) {
                const auto& slot = theDataArray.mBlock[i];
                const auto* raw = reinterpret_cast<const unsigned char*>(&slot);
                if ((slot.mID & DATA_ARRAY_KEY_MASK) && slot.mItem.mRunIceWave) {
                    std::array<ZombieID, RunWaveHitPacking::LegacySlots> legacy{};
                    for (unsigned int index = 0; index < legacy.size(); ++index)
                        legacy[index] = static_cast<ZombieID>(RunWaveHitPacking::LegacyId(slot.mItem.mRunWaveHitIds, index));
                    const size_t offset = reinterpret_cast<const unsigned char*>(slot.mItem.mRunWaveHitIds) - raw;
                    theContext.mBuffer.WriteBytes(raw, static_cast<int>(offset));
                    theContext.mBuffer.WriteBytes(reinterpret_cast<const unsigned char*>(legacy.data()), sizeof(legacy));
                    theContext.mBuffer.WriteBytes(raw + offset + sizeof(legacy), static_cast<int>(sizeof(slot) - offset - sizeof(legacy)));
                } else theContext.mBuffer.WriteBytes(raw, sizeof(slot));
            }
        } else theContext.SyncBytes(theDataArray.mBlock, static_cast<int>(aBytes));
    } else theContext.SyncBytes(theDataArray.mBlock, static_cast<int>(aBytes));
	if (theContext.mReading)
	{
		theDataArray.mFreeListHead = aFree;
		theDataArray.mMaxUsedCount = aUsed;
		theDataArray.mSize = aSize;
	}
	if (theContext.mFailed || !theDataArray.DataArrayValidateState())
	{
		theContext.mFailed = true;
		// Never walk untrusted IDs during cleanup of a failed load.
		if (theContext.mReading) theDataArray.mFreeListHead = theDataArray.mMaxUsedCount = theDataArray.mSize = 0;
	}
}

template <typename T>
static void PreflightSavedPool(SaveGameContext& theContext, const DataArray<T>& theDestination)
{
	if (theContext.mFailed) return;
	DataArray<T> aScratch;
	aScratch.DataArrayInitialize(theDestination.mMaxSize, "save-preflight");
	SyncDataArray(theContext, aScratch);
	// These are serialized bytes, not live constructed engine objects.
	aScratch.mFreeListHead = aScratch.mMaxUsedCount = aScratch.mSize = 0;
}

static bool PreflightSavedPools(const SaveGameContext& theSource, Board* theBoard)
{
	SaveGameContext aScan;
	aScan.mReading = true;
	aScan.mFailed = false;
	aScan.mBuffer = theSource.mBuffer;
	const size_t aOffset = size_t(&theBoard->mPaused) - size_t(theBoard);
	std::vector<unsigned char> aBoardBytes(sizeof(Board) - aOffset);
	aScan.SyncBytes(aBoardBytes.data(), static_cast<int>(aBoardBytes.size()));
	PreflightSavedPool(aScan, theBoard->mZombies);
	PreflightSavedPool(aScan, theBoard->mPlants);
	PreflightSavedPool(aScan, theBoard->mProjectiles);
	PreflightSavedPool(aScan, theBoard->mCoins);
	PreflightSavedPool(aScan, theBoard->mLawnMowers);
	PreflightSavedPool(aScan, theBoard->mGridItems);
	PreflightSavedPool(aScan, theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems);
	PreflightSavedPool(aScan, theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters);
	PreflightSavedPool(aScan, theBoard->mApp->mEffectSystem->mParticleHolder->mParticles);
	PreflightSavedPool(aScan, theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations);
	PreflightSavedPool(aScan, theBoard->mApp->mEffectSystem->mTrailHolder->mTrails);
	PreflightSavedPool(aScan, theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments);
	return !aScan.mFailed;
}

// @pvz-online: 读档后修剪无主 attachment。Attachment 只在持有者驱动的 Update() 里才会发现效果
// 已失效并把自己标死；持有者消失时若漏调 AttachmentDie，它就再也不被 Update、永不回收，并随
// SyncDataArray 整池写进存档（实测历史存档 98.5%~100% 的存活 attachment 无主，挂的粒子早已不存在，
// 池可顶满 4096 导致新僵尸挂不上任何特效）。
// 可达集合：僵尸/子弹/金币的 mAttachmentID（池遍历含已死仍在池中的）+ 存活 reanim 各轨道的
// mAttachmentID 为根，再沿「可达 attachment」里的 EFFECT_ATTACHMENT 向下展开——只从可达者展开，
// 互相引用的孤儿不会彼此保活。不可达的存活 attachment 调 AttachmentDie()：效果一并死亡、自身 mDead，
// 下一帧由 EffectSystem::ProcessDeleteQueue 回收。必须先于 reanim 修剪：它会让挂件 reanim 死亡。
static void TrimOrphanAttachmentsAfterLoad(Board* theBoard)
{
	auto& aAttachPool = theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments;
	std::unordered_set<unsigned int> aReachable;
	std::vector<unsigned int> aPending;
	unsigned int aRootOwner = 0, aRootTrack = 0;
	auto aRoot = [&](AttachmentID theId, unsigned int& theCounter)
	{
		if (theId == AttachmentID::ATTACHMENTID_NULL) return;
		Attachment* aAttachment = aAttachPool.DataArrayTryToGet((unsigned int)theId);
		if (aAttachment == nullptr || aAttachment->mDead) return;
		if (aReachable.insert((unsigned int)theId).second) { aPending.push_back((unsigned int)theId); ++theCounter; }
	};

	Zombie* aZombie = nullptr;
	while (theBoard->mZombies.IterateNext(aZombie))
		aRoot(aZombie->mAttachmentID, aRootOwner);
	Projectile* aProjectile = nullptr;
	while (theBoard->mProjectiles.IterateNext(aProjectile))
		aRoot(aProjectile->mAttachmentID, aRootOwner);
	Coin* aCoin = nullptr;
	while (theBoard->mCoins.IterateNext(aCoin))
		aRoot(aCoin->mAttachmentID, aRootOwner);
	Reanimation* aReanim = nullptr;
	while (theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations.IterateNext(aReanim))
	{
		if (aReanim->mDead || aReanim->mTrackInstances == nullptr || aReanim->mDefinition == nullptr) continue;
		for (int aTrack = 0; aTrack < aReanim->mDefinition->mTracks.count; ++aTrack)
			aRoot(aReanim->mTrackInstances[aTrack].mAttachmentID, aRootTrack);
	}

	while (!aPending.empty())
	{
		Attachment* aParent = aAttachPool.DataArrayTryToGet(aPending.back());
		aPending.pop_back();
		if (aParent == nullptr) continue;
		for (int i = 0; i < aParent->mNumEffects && i < MAX_EFFECTS_PER_ATTACHMENT; ++i)
		{
			if (aParent->mEffectArray[i].mEffectType != EffectType::EFFECT_ATTACHMENT) continue;
			const unsigned int aChildId = aParent->mEffectArray[i].mEffectID;
			Attachment* aChild = aAttachPool.DataArrayTryToGet(aChildId);
			if (aChild != nullptr && !aChild->mDead && aReachable.insert(aChildId).second)
				aPending.push_back(aChildId);
		}
	}

	std::vector<Attachment*> aOrphans;
	unsigned int aAlive = 0;
	Attachment* aAttachment = nullptr;
	while (aAttachPool.IterateNext(aAttachment))
	{
		if (aAttachment->mDead) continue;
		++aAlive;
		if (!aReachable.count(aAttachPool.DataArrayGetID(aAttachment)))
			aOrphans.push_back(aAttachment);
	}
	for (Attachment* aOrphan : aOrphans)
		aOrphan->AttachmentDie();
	TodLog("[save] attachment trim after load: pool %u/%u, alive %u, roots owner %u track %u, reachable %u, trimmed %u",
		aAttachPool.mSize, aAttachPool.mMaxSize, aAlive, aRootOwner, aRootTrack,
		(unsigned int)aReachable.size(), (unsigned int)aOrphans.size());
}

// @pvz-online: 读档后修剪残留 reanim。存档用 SyncDataArray 整块序列化 reanim 池，历史累积的
// 无主对象会随之长期留存。调用时 SyncReanimation 已完成（mDefinition 与 mTrackInstances 已重新
// 绑定，~Reanimation 会调 ReanimationDie 并遍历 tracks），持有者字段也已全部读回。
// 判据：类型属于 MayOmit 白名单（金币/阳光/溅射等装饰类）或 REANIM_ZOMBIE（历史版本残留的
// 僵尸 body），**且**不在全局持有集合里。持有集合收集所有读档后有效的 ReanimationID 字段，
// 外加存活 attachment 里的 EFFECT_REANIM——漏掉任何一个字段都会误删（如睡眠蘑菇的 Zzz、
// 割草机卷走中的僵尸、报纸路障的独立帽子）。遍历池用 IterateNext，已死但仍在池中的持有者也算。
static void TrimOrphanReanimationsAfterLoad(Board* theBoard)
{
	std::unordered_set<unsigned int> aHeld;
	auto aHold = [&aHeld](ReanimationID theId) { if (theId != ReanimationID::REANIMATIONID_NULL) aHeld.insert((unsigned int)theId); };

	Zombie* aZombie = nullptr;
	while (theBoard->mZombies.IterateNext(aZombie))
	{
		aHold(aZombie->mBodyReanimID);
		aHold(aZombie->mBossFireBallReanimID);
		aHold(aZombie->mSpecialHeadReanimID);
		aHold(aZombie->mPaperHelmReanimID);
		aHold(aZombie->mZombatarHeadReanimID);
		aHold(aZombie->mMoweredReanimID);
	}
	// 只为日志对比旧判据：旧判据白名单只看 !mIsAttachment，僵尸只看「存活」僵尸（IterateZombies
	// 跳过 mDead）的字段。新判据把已死仍在池中的僵尸也算持有者——它们下一帧清理时自己会 RemoveReanimation。
	std::unordered_set<unsigned int> aHeldByLiveZombies;
	aZombie = nullptr;
	while (theBoard->IterateZombies(aZombie))
		for (ReanimationID aId : { aZombie->mBodyReanimID, aZombie->mBossFireBallReanimID, aZombie->mSpecialHeadReanimID,
			aZombie->mPaperHelmReanimID, aZombie->mZombatarHeadReanimID, aZombie->mMoweredReanimID })
			if (aId != ReanimationID::REANIMATIONID_NULL) aHeldByLiveZombies.insert((unsigned int)aId);
	Plant* aPlant = nullptr;
	while (theBoard->mPlants.IterateNext(aPlant))
	{
		aHold(aPlant->mBodyReanimID);
		aHold(aPlant->mHeadReanimID);
		aHold(aPlant->mHeadReanimID2);
		aHold(aPlant->mHeadReanimID3);
		aHold(aPlant->mBlinkReanimID);
		aHold(aPlant->mLightReanimID);
		aHold(aPlant->mSleepingReanimID);
	}
	LawnMower* aLawnMower = nullptr;
	while (theBoard->mLawnMowers.IterateNext(aLawnMower))
		aHold(aLawnMower->mReanimID);
	GridItem* aGridItem = nullptr;
	while (theBoard->mGridItems.IterateNext(aGridItem))
		aHold(aGridItem->mGridItemReanimID);
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; ++aRow)
		for (int i = 0; i < 12; ++i)
			aHold(theBoard->mFwooshID[aRow][i]);
	aHold(theBoard->mChallenge->mReanimChallenge);
	for (int i = 0; i < 6; ++i)
		aHold(theBoard->mChallenge->mReanimClouds[i]);
	aHold(theBoard->mCursorObject->mReanimCursorID);
	for (int i = 0; i < MAX_MESSAGE_LENGTH; ++i)
		aHold(theBoard->mAdvice->mTextReanimID[i]);
	aHold(theBoard->mApp->mCrazyDaveReanimID);
	aHold(theBoard->mApp->mCrazyDaveBlinkReanimID);
	Attachment* aAttachment = nullptr;
	while (theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments.IterateNext(aAttachment))
	{
		if (aAttachment->mDead) continue;
		for (int i = 0; i < aAttachment->mNumEffects && i < MAX_EFFECTS_PER_ATTACHMENT; ++i)
			if (aAttachment->mEffectArray[i].mEffectType == EffectType::EFFECT_REANIM)
				aHeld.insert(aAttachment->mEffectArray[i].mEffectID);
	}

	auto& aReanimPool = theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations;
	std::vector<Reanimation*> aTrimList;
	unsigned int aPoolCount = 0, aTrimZombie = 0, aKeptOldRuleWouldTrim = 0, aOldRuleTotal = 0;
	// 「旧判据会删、新判据保留」按类型拆开：S1 点名的两类误伤（割草卷走、睡眠 Zzz）单列，
	// 读档时刻难以人为制造，靠这行日志在实际存档里留证。
	unsigned int aKeptZombie = 0, aKeptMowered = 0, aKeptSleeping = 0, aKeptOther = 0;
	Reanimation* aPoolItem = nullptr;
	while (aReanimPool.IterateNext(aPoolItem))
	{
		++aPoolCount;
		const bool aZombieType = aPoolItem->mReanimationType == ReanimationType::REANIM_ZOMBIE;
		if (!aZombieType && !ReanimationMayOmit(aPoolItem->mReanimationType)) continue;
		const unsigned int aId = aReanimPool.DataArrayGetID(aPoolItem);
		const bool aOldRuleWouldTrim = aZombieType ? !aHeldByLiveZombies.count(aId) : !aPoolItem->mIsAttachment;
		if (aOldRuleWouldTrim) ++aOldRuleTotal;
		if (aHeld.count(aId))
		{
			if (aOldRuleWouldTrim)
			{
				++aKeptOldRuleWouldTrim;
				if (aZombieType) ++aKeptZombie;
				else if (aPoolItem->mReanimationType == ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE) ++aKeptMowered;
				else if (aPoolItem->mReanimationType == ReanimationType::REANIM_SLEEPING) ++aKeptSleeping;
				else ++aKeptOther;
			}
			continue;
		}
		aTrimList.push_back(aPoolItem);
		if (aZombieType) ++aTrimZombie;
	}
	for (Reanimation* aTrim : aTrimList)
		aReanimPool.DataArrayFree(aTrim);
	TodLog("[save] reanim trim after load: pool %u, trimmed %u (zombie %u), held set %u, old rule would trim %u (kept now %u: zombie %u mowered %u sleeping %u other %u)",
		aPoolCount, (unsigned int)aTrimList.size(), aTrimZombie, (unsigned int)aHeld.size(), aOldRuleTotal, aKeptOldRuleWouldTrim,
		aKeptZombie, aKeptMowered, aKeptSleeping, aKeptOther);
}

//0x4819D0
void SyncBoard(SaveGameContext& theContext, Board* theBoard)
{
	if (theContext.mReading) theBoard->mRelayedZombieIds.clear();
	// TODO test if gives sane results
	size_t offset = size_t(&theBoard->mPaused) - size_t(theBoard);
	theContext.SyncBytes(&theBoard->mPaused, sizeof(Board) - offset);

	SyncDataArray(theContext, theBoard->mZombies);
	if (theContext.mFailed) return;													//0x482190
	SyncDataArray(theContext, theBoard->mPlants);
	if (theContext.mFailed) return;													//0x482280
	SyncDataArray(theContext, theBoard->mProjectiles);
	if (theContext.mFailed) return;												//0x482370
	SyncDataArray(theContext, theBoard->mCoins);
	if (theContext.mFailed) return;													//0x482460
	SyncDataArray(theContext, theBoard->mLawnMowers);
	if (theContext.mFailed) return;												//0x482550
	SyncDataArray(theContext, theBoard->mGridItems);
	if (theContext.mFailed) return;												//0x482650
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems);
	if (theContext.mFailed) return;	//0x482740
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters);
	if (theContext.mFailed) return;			//0x482830
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticles);
	if (theContext.mFailed) return;			//0x482920
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations);
	if (theContext.mFailed) return;	//0x482920
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mTrailHolder->mTrails);
	if (theContext.mFailed) return;				//0x482650
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments);
	if (theContext.mFailed) return;		//0x482A10

	{
		TodParticleSystem* aParticle = nullptr;
		while (theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(aParticle))
		{
			SyncParticleSystem(theBoard, aParticle, theContext);
		}
	}
	{
		Reanimation* aReanimation = nullptr;
		while (theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations.IterateNext(aReanimation))
		{
			SyncReanimation(theBoard, aReanimation, theContext);
		}
	}
	{
		Trail* aTrail = nullptr;
		while (theBoard->mApp->mEffectSystem->mTrailHolder->mTrails.IterateNext(aTrail))
		{
			SyncTrail(theBoard, aTrail, theContext);
		}
	}

	theContext.SyncBytes(theBoard->mCursorObject, sizeof(CursorObject));
	theContext.SyncBytes(theBoard->mCursorPreview, sizeof(CursorPreview));
	theContext.SyncBytes(theBoard->mAdvice, sizeof(MessageWidget));
	theContext.SyncBytes(theBoard->mSeedBank, sizeof(SeedBank));
	theContext.SyncBytes(theBoard->mChallenge, sizeof(Challenge));
	theContext.SyncBytes(theBoard->mApp->mMusic, sizeof(Music));

	// @pvz-online: 修剪必须在 Challenge / CursorObject / MessageWidget 读回之后——它们的
	// ReanimationID 字段是持有集合的一部分，读回前还是读档前的旧值。
	if (theContext.mReading && !theContext.mFailed)
	{
		TrimOrphanAttachmentsAfterLoad(theBoard);
		TrimOrphanReanimationsAfterLoad(theBoard);
	}

	if (theContext.mReading)
	{
		if ((unsigned long)theContext.ByteLeftToRead() < 4)
		{
			theContext.mFailed = true;
		}

		if (theContext.mFailed || (unsigned int)theContext.mBuffer.ReadLong() != SAVE_FILE_MAGIC_NUMBER)
		{
			theContext.mFailed = true;
		}
	}
	else
	{
		theContext.mBuffer.WriteLong(SAVE_FILE_MAGIC_NUMBER);
	}

	// @pvz-online: optional tail preserves relay identity without changing raw entity layouts.
	// Old saves end at the preceding magic; old readers ignore this appended tail.
	const unsigned int aRelayTag = 0x31594c52; // RLY1
	if (theContext.mReading)
	{
		if (theContext.mFailed || theContext.ByteLeftToRead() == 0) return;
		if (theContext.ByteLeftToRead() < 8 || (unsigned int)theContext.mBuffer.ReadLong() != aRelayTag)
		{ theContext.mFailed = true; return; }
		unsigned int aCount = (unsigned int)theContext.mBuffer.ReadLong();
		if (aCount > theBoard->mZombies.mMaxSize || aCount > (unsigned int)theContext.ByteLeftToRead() / 4)
		{ theContext.mFailed = true; return; }
		for (unsigned int i = 0; i < aCount; ++i)
		{
			unsigned int aId = (unsigned int)theContext.mBuffer.ReadLong();
			if (!theBoard->mZombies.DataArrayTryToGet(aId) || !theBoard->mRelayedZombieIds.insert(aId).second)
			{ theContext.mFailed = true; return; }
		}
	}
	else
	{
		std::vector<unsigned int> aIds;
		for (auto aId : theBoard->mRelayedZombieIds)
			if (theBoard->mZombies.DataArrayTryToGet(aId)) aIds.push_back(aId);
		std::sort(aIds.begin(), aIds.end());
		theContext.mBuffer.WriteLong(aRelayTag);
		theContext.mBuffer.WriteLong((int)aIds.size());
		for (auto aId : aIds) theContext.mBuffer.WriteLong(aId);
	}
}

//0x481CE0
void FixBoardAfterLoad(Board* theBoard)
{
	{
		Plant* aPlant = nullptr;
		while (theBoard->mPlants.IterateNext(aPlant))
		{
			aPlant->mApp = theBoard->mApp;
			aPlant->mBoard = theBoard;
			if (!aPlant->mDead && aPlant->mSeedType == SeedType::SEED_GLOOMSHROOM &&
				theBoard->mApp->IsRunMode() && theBoard->mApp->RunPlantBuff2Count(SeedType::SEED_GLOOMSHROOM) > 0)
				aPlant->RefreshRunMaxHealth();
		}
	}
	{
		Zombie* aZombie = nullptr;
		while (theBoard->mZombies.IterateNext(aZombie))
		{
			aZombie->mApp = theBoard->mApp;
			aZombie->mBoard = theBoard;
		}
	}
	{
		Projectile* aProjectile = nullptr;
		while (theBoard->mProjectiles.IterateNext(aProjectile))
		{
			aProjectile->mApp = theBoard->mApp;
			aProjectile->mBoard = theBoard;
		}
	}
	{
		Coin* aCoin = nullptr;
		while (theBoard->mCoins.IterateNext(aCoin))
		{
			aCoin->mApp = theBoard->mApp;
			aCoin->mBoard = theBoard;
		}
	}
	{
		LawnMower* aLawnMower = nullptr;
		while (theBoard->mLawnMowers.IterateNext(aLawnMower))
		{
			aLawnMower->mApp = theBoard->mApp;
			aLawnMower->mBoard = theBoard;
			if (!aLawnMower->mDead && theBoard->mApp->IsRunMode() && !theBoard->StageHasRoof() &&
				theBoard->mPlantRow[aLawnMower->mRow] == PlantRowType::PLANTROW_POOL &&
				aLawnMower->mMowerType == LawnMowerType::LAWNMOWER_LAWN &&
				(aLawnMower->mMowerState == LawnMowerState::MOWER_READY ||
				 aLawnMower->mMowerState == LawnMowerState::MOWER_ROLLING_IN))
			{
				theBoard->mApp->RemoveReanimation(aLawnMower->mReanimID);
				aLawnMower->mMowerType = LawnMowerType::LAWNMOWER_POOL;
				Reanimation* aMowerReanim = theBoard->mApp->AddReanimation(0.0f, 18.0f,
					aLawnMower->mRenderOrder, ReanimationType::REANIM_POOL_CLEANER);
				aMowerReanim->mAnimRate = 0.0f;
				aMowerReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
				aMowerReanim->mIsAttachment = true;
				aMowerReanim->OverrideScale(0.8f, 0.8f);
				aMowerReanim->SetFramesForLayer("anim_land");
				aMowerReanim->SetTruncateDisappearingFrames(nullptr, false);
				aLawnMower->mReanimID = theBoard->mApp->ReanimationGetID(aMowerReanim);
			}
		}
	}
	{
		GridItem* aGridItem = nullptr;
		while (theBoard->mGridItems.IterateNext(aGridItem))
		{
			aGridItem->mApp = theBoard->mApp;
			aGridItem->mBoard = theBoard;
		}
	}

	theBoard->mAdvice->mApp = theBoard->mApp;
	theBoard->mCursorObject->mApp = theBoard->mApp;
	theBoard->mCursorObject->mBoard = theBoard;
	theBoard->mCursorPreview->mApp = theBoard->mApp;
	theBoard->mCursorPreview->mBoard = theBoard;
	theBoard->mSeedBank->mApp = theBoard->mApp;
	theBoard->mSeedBank->mBoard = theBoard;
	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		theBoard->mSeedBank->mSeedPackets[i].mApp = theBoard->mApp;
		theBoard->mSeedBank->mSeedPackets[i].mBoard = theBoard;
	}
	theBoard->mChallenge->mApp = theBoard->mApp;
	theBoard->mChallenge->mBoard = theBoard;
	theBoard->mApp->mMusic->mApp = theBoard->mApp;
	theBoard->mApp->mMusic->mMusicInterface = theBoard->mApp->mMusicInterface;
}

//0x481FE0
// GOTY @Patoke: 0x48CBC0
bool LawnLoadGame(Board* theBoard, const std::string& theFilePath)
{
	SaveGameContext aContext;
	aContext.mFailed = false;
	aContext.mReading = true;
	if (!gSexyAppBase->ReadBufferFromFile(theFilePath, &aContext.mBuffer, false))
	{
		return false;
	}

	SaveFileHeader aHeader;
	aContext.SyncBytes(&aHeader, sizeof(aHeader));
	if (aHeader.mMagicNumber != SAVE_FILE_MAGIC_NUMBER || aHeader.mBuildVersion != SAVE_FILE_VERSION || aHeader.mBuildDate != SAVE_FILE_DATE)
	{
		return false;
	}
	// Reject damaged pool metadata before replacing any part of the live board.
	if (!PreflightSavedPools(aContext, theBoard)) return false;

    RunSpeciesRuntime::Reset(theBoard);
	SyncBoard(aContext, theBoard);
	if (aContext.mFailed)
	{
		return false;
	}

	const size_t waveHitBaseEnd = static_cast<size_t>(aContext.mBuffer.mReadBitPos / 8);
	TodTrace("Loaded save game");
	FixBoardAfterLoad(theBoard);
	if (!WaveHealthSave::LoadTrailer(theBoard, aContext.mBuffer.GetDataPtr(), static_cast<size_t>(aContext.mBuffer.GetDataLen()))) return false;
	if (!WaveSpawnRetrySave::LoadTrailer(theBoard, aContext.mBuffer.GetDataPtr(), static_cast<size_t>(aContext.mBuffer.GetDataLen()))) return false;
	if (!DeferredZombieSpawnSave::LoadTrailer(theBoard, aContext.mBuffer.GetDataPtr(), static_cast<size_t>(aContext.mBuffer.GetDataLen()))) return false;
	if (!DeferredPlantSetupSave::LoadTrailer(theBoard, aContext.mBuffer.GetDataPtr(), static_cast<size_t>(aContext.mBuffer.GetDataLen()))) return false;
	NextWaveControl::LoadTrailer(theBoard, aContext.mBuffer.GetDataPtr(), static_cast<size_t>(aContext.mBuffer.GetDataLen()));
	theBoard->mApp->mDamageStats.LoadSaveTrailer(theBoard, aContext.mBuffer.GetDataPtr(), static_cast<size_t>(aContext.mBuffer.GetDataLen()));
	if (!RunDotEffects::LoadTrailer(theBoard, aContext.mBuffer.GetDataPtr(), static_cast<size_t>(aContext.mBuffer.GetDataLen()))) return false;
	if (!RunWaveHitSave::LoadTrailer(theBoard, aContext.mBuffer.GetDataPtr(), static_cast<size_t>(aContext.mBuffer.GetDataLen()), waveHitBaseEnd)) return false;
    if (!RunSpeciesRuntime::LoadTrailer(theBoard,aContext.mBuffer.GetDataPtr(),static_cast<size_t>(aContext.mBuffer.GetDataLen()))) return false;
	theBoard->mApp->mGameScene = GameScenes::SCENE_PLAYING;
	return true;
}

//0x4820D0
bool LawnSaveGame(Board* theBoard, const std::string& theFilePath)
{
    if(ObserverIsolation::Active()){return false;}

	theBoard->SyncLegacyWaveHealth();
	SaveGameContext aContext;
	aContext.mFailed = false;
	aContext.mReading = false;

	SaveFileHeader aHeader;
	aHeader.mMagicNumber = SAVE_FILE_MAGIC_NUMBER;
	aHeader.mBuildVersion = SAVE_FILE_VERSION;
	aHeader.mBuildDate = SAVE_FILE_DATE;

	aContext.SyncBytes(&aHeader, sizeof(aHeader));
	SyncBoard(aContext, theBoard);
	if (aContext.mFailed) return false;
	const auto waveHits=RunWaveHitSave::SaveTrailer(theBoard);
	aContext.mBuffer.WriteBytes(waveHits.data(),static_cast<int>(waveHits.size()));
	const auto retry=WaveSpawnRetrySave::SaveTrailer(theBoard);
	aContext.mBuffer.WriteBytes(retry.data(),static_cast<int>(retry.size()));
	const auto auxiliary=DeferredZombieSpawnSave::SaveTrailer(theBoard);
	aContext.mBuffer.WriteBytes(auxiliary.data(),static_cast<int>(auxiliary.size()));
	const auto plants=DeferredPlantSetupSave::SaveTrailer(theBoard);
	aContext.mBuffer.WriteBytes(plants.data(),static_cast<int>(plants.size()));
	const auto health=WaveHealthSave::SaveTrailer(theBoard);
	aContext.mBuffer.WriteBytes(health.data(),static_cast<int>(health.size()));
	const auto dots=RunDotEffects::SaveTrailer(theBoard);
	aContext.mBuffer.WriteBytes(dots.data(),static_cast<int>(dots.size()));
	const auto nextWave=NextWaveControl::SaveTrailer(theBoard);
	aContext.mBuffer.WriteBytes(nextWave.data(),static_cast<int>(nextWave.size()));
	const auto stats = theBoard->mApp->mDamageStats.SaveTrailer();
	if (!stats.empty()) aContext.mBuffer.WriteBytes(stats.data(), static_cast<int>(stats.size()));
    const auto species=RunSpeciesRuntime::SaveTrailer(theBoard);
    aContext.mBuffer.WriteBytes(species.data(),static_cast<int>(species.size()));
	return gSexyAppBase->WriteBufferToFile(theFilePath, &aContext.mBuffer);
}
