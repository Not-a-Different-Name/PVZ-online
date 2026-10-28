// @pvz-online: PC snapshot wire format; viewing never creates remote simulation entities.
#include "Board.h"
#include "Observer/ObserverBoard.h"
#include "../LawnApp.h"
#include "../PvzpLib/Reanimator.h"
#include "System/ReanimationLawn.h"
#include "Run/RunMath.h"
#include "Widget/MobileCoopDraw.h"
#include "Widget/CoopHud.h"
#include <algorithm>

int Board::BuildBoardSnapshot(uint8_t* theBuffer, int theCapacity)
{
	const int aMaxSize = NetProto::SNAPSHOT_HEADER_SIZE
		+ NetProto::SNAPSHOT_ZOMBIE_MAX * NetProto::SNAPSHOT_ZOMBIE_SIZE
		+ NetProto::SNAPSHOT_PLANT_MAX * NetProto::SNAPSHOT_PLANT_SIZE;
	if (theCapacity < aMaxSize) return 0;

	bool aTruncated = false;

	Zombie* aZombiePicks[NetProto::SNAPSHOT_ZOMBIE_MAX];
	int aZombieCount = 0;
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (aZombie->mDead) continue;
		if (aZombieCount < NetProto::SNAPSHOT_ZOMBIE_MAX)
		{
			aZombiePicks[aZombieCount++] = aZombie;
			continue;
		}
		aTruncated = true;
		int aWorst = 0;
		for (int i = 1; i < NetProto::SNAPSHOT_ZOMBIE_MAX; i++)
		{
			if (aZombiePicks[i]->mX > aZombiePicks[aWorst]->mX) aWorst = i;
		}
		if (aZombie->mX < aZombiePicks[aWorst]->mX) aZombiePicks[aWorst] = aZombie;
	}

	Plant* aPlantPicks[NetProto::SNAPSHOT_PLANT_MAX];
	int aPlantCount = 0;
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->mDead) continue;
		if (aPlantCount >= NetProto::SNAPSHOT_PLANT_MAX)
		{
			aTruncated = true;
			break;
		}
		aPlantPicks[aPlantCount++] = aPlant;
	}

	// 推车位图：bit r = 第 r 行的兜底还在（待命或正滚入；冲出去过/被压扁就是没了）
	uint8_t aMowers = 0;
	LawnMower* aMower = nullptr;
	while (mLawnMowers.DataArrayIterate(aMower))
	{
		if (aMower->mMowerState == LawnMowerState::MOWER_READY
			|| aMower->mMowerState == LawnMowerState::MOWER_ROLLING_IN)
		{
			aMowers |= (uint8_t)(1 << aMower->mRow);
		}
	}

	int anAt = 0;
	theBuffer[anAt++] = (uint8_t)((mPaused ? NetProto::SNAPSHOT_FLAG_PAUSED : 0)
		| (mLevelAwardSpawned ? NetProto::SNAPSHOT_FLAG_COMPLETE : 0)
		| (aTruncated ? NetProto::SNAPSHOT_FLAG_TRUNCATED : 0));
	theBuffer[anAt++] = (uint8_t)aZombieCount;
	theBuffer[anAt++] = (uint8_t)aPlantCount;
	theBuffer[anAt++] = aMowers;
	theBuffer[anAt++] = (uint8_t)mCurrentWave;
	theBuffer[anAt++] = (uint8_t)mNumWaves;
	theBuffer[anAt++] = (uint8_t)(StageHas6Rows() ? 6 : 5);

	for (int i = 0; i < aZombieCount; i++)
	{
		Zombie* aOne = aZombiePicks[i];
		int aX = aOne->mX + 512;	// 线上是 x+512（棋盘坐标可为负——入场口在屏外）
		if (aX < 0) aX = 0;
		if (aX > 0xFFFF) aX = 0xFFFF;
		// hp 按体血算（协议口径）；头盔/护盾/气球只传"在不在"，具体甲种观看端不画
		int aHP = aOne->mBodyMaxHealth > 0 ? RunMath::ScaleHealth(aOne->mBodyHealth, 100, aOne->mBodyMaxHealth) : 100;
		if (aHP < 0) aHP = 0;
		if (aHP > 100) aHP = 100;
		uint8_t aFlags = 0;
		if (aOne->mHelmType != HelmType::HELMTYPE_NONE) aFlags |= NetProto::SNAPSHOT_ZFLAG_HELMET;
		if (aOne->mShieldType != ShieldType::SHIELDTYPE_NONE) aFlags |= NetProto::SNAPSHOT_ZFLAG_SHIELD;
		if (PlantZombies::Armor(aOne->mZombieType)) {
            if(aOne->HasPlantHat())aFlags|=NetProto::SNAPSHOT_ZFLAG_PLANT_HAT;
        } else if (aOne->mFlyingHealth > 0 && aOne->mFlyingMaxHealth > 0) aFlags |= NetProto::SNAPSHOT_ZFLAG_FLYING;
		if (IsRelayedZombie(aOne)) aFlags |= NetProto::SNAPSHOT_ZFLAG_RELAYED;

		uint16_t aType = (uint16_t)aOne->mZombieType;
		theBuffer[anAt++] = (uint8_t)(aType & 0xFF);
		theBuffer[anAt++] = (uint8_t)(aType >> 8);
		theBuffer[anAt++] = (uint8_t)aOne->mRow;
		theBuffer[anAt++] = (uint8_t)(aX & 0xFF);
		theBuffer[anAt++] = (uint8_t)(aX >> 8);
		theBuffer[anAt++] = (uint8_t)aHP;
		theBuffer[anAt++] = aFlags;
	}

	for (int i = 0; i < aPlantCount; i++)
	{
		Plant* aOne = aPlantPicks[i];
		int aHP = aOne->mPlantMaxHealth > 0 ? aOne->mPlantHealth * 100 / aOne->mPlantMaxHealth : 100;
		if (aHP < 0) aHP = 0;
		if (aHP > 100) aHP = 100;
		theBuffer[anAt++] = (uint8_t)aOne->mSeedType;
		theBuffer[anAt++] = (uint8_t)aOne->mRow;
		theBuffer[anAt++] = (uint8_t)aOne->mPlantCol;
		theBuffer[anAt++] = (uint8_t)aHP;
	}

	return anAt;
}


void Board::StopMobileWatch()
{
    if (mApp->mOnlineSession && mApp->mOnlineSession->IsWatching())
        mApp->mOnlineSession->EndWatch(true);
    mMobileWatchOpen = false;
    mHasRemoteSnapshot = false;
}
void Board::UpdateBoardWatch()
{
    if(mApp->IsOnlineGame()&&mApp->mOnlineSession&&mApp->mOnlineSession->IsConnected()) {
        if(!mObserverView)mObserverView=std::make_unique<Observer::BoardView>(mApp);
        mObserverView->Update(this,
mApp->mOnlineSession.get());
    }else mObserverView.reset();

    auto* session = mApp->mOnlineSession.get();
    if (!mApp->IsOnlineGame() || !session || !session->IsConnected())
    {
        StopMobileWatch(); mWatchSnapshotTicker = 0; return;
    }
    if (session->TakeViewSnapshot(mRemoteSnapshot)) mHasRemoteSnapshot = true;
    auto end = session->TakeWatchEnded();
    if (end == NetSession::WatchEnd::TIMEOUT || end == NetSession::WatchEnd::TARGET_LEFT)
    {
        StopMobileWatch();
    }
    if (mMobileWatchOpen && (!mApp->mActive || mApp->mMinimized || CoopHud::PauseStopsWatch(this) ||
        mApp->mGameScene != GameScenes::SCENE_PLAYING || !session->IsWatching())) StopMobileWatch();
    if (!session->HasBoardWatchers()) { mWatchSnapshotTicker = 0; return; }
    if (mWatchSnapshotTicker > 0) { --mWatchSnapshotTicker; return; }
    mWatchSnapshotTicker = 7;
    uint8_t buffer[NetProto::SNAPSHOT_MAX_PARTS * NetProto::SNAPSHOT_PART_PAYLOAD];
    int size = BuildBoardSnapshot(buffer, sizeof(buffer));
    uint8_t seats[NetProto::MAX_PLAYERS];
    int count = session->GetBoardWatcherSeats(seats, NetProto::MAX_PLAYERS);
    if (size > 0) for (int i=0; i<count; ++i) session->SendBoardSnapshot(seats[i], buffer, size);
}
void Board::CycleMobileWatch()
{
    auto* session = mApp->mOnlineSession.get();
    if (!session || !session->IsConnected()) return;
    uint8_t previous = session->IsWatching() ? session->GetWatchTargetSeat() : session->GetLocalSeat();
    for (int i=1; i<=NetProto::MAX_PLAYERS; ++i)
    {
        uint8_t seat = (previous - 1 + i) % NetProto::MAX_PLAYERS + 1;
        if (!session->CanWatchSeat(seat)) continue;
        if(session->ObserverRequestCooldown())return;
        mHasRemoteSnapshot = false;
        mMobileWatchOpen = session->BeginWatch(seat);
        CloseQuickChatPanel(); mSunGiftSelectFrames = 0; mCoopToolsOpen=false;
        return;
    }
    StopMobileWatch();
}
bool Board::IsWatchingView()
{
    auto* s=mApp->mOnlineSession.get();
    return mMobileWatchOpen&&mApp->mGameScene==GameScenes::SCENE_PLAYING&&mObserverView&&mObserverView->CanDraw(s);
}
void Board::DrawMobileWatch(Graphics* g)
{
    if(mObserverView&&mObserverView->Draw(g,mApp->mOnlineSession.get()))CoopHud::WatchControls(g,this);
}
