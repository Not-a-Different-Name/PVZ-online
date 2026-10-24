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
        PushCustomBanner(end == NetSession::WatchEnd::TIMEOUT
            ? ModText::Tr("画面暂不可用，请换一位队友", "View unavailable. Choose another teammate.")
            : ModText::Tr("队友已离开，请换一位", "Teammate left. Choose another."));
        StopMobileWatch();
    }
    if (mMobileWatchOpen && (!mApp->mActive || mApp->mMinimized || mPaused ||
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
        if (seat == session->GetLocalSeat() || !session->IsSeatOccupied(seat)) continue;
        if(session->ObserverRequestCooldown()){PushCustomBanner(ModText::Tr("稍等几秒再获取快照", "Wait a few seconds before requesting another snapshot"));return;}
        mHasRemoteSnapshot = false;
        mMobileWatchOpen = session->BeginWatch(seat);
        CloseQuickChatPanel(); mSunGiftSelectFrames = 0; mCoopToolsOpen=false;
        return;
    }
    PushCustomBanner(ModText::Tr("没有可观看的队友", "No teammate to watch"));
}
static void MobileWatchHealth(Graphics* g, int x, int y, int hp)
{
    g->SetColor(Color(0,0,0,190)); g->FillRect(x-1,y-1,42,6);
    g->SetColor(hp > 30 ? Color(90,210,90) : Color(230,90,70));
    g->FillRect(x,y,40*hp/100,4);
}
void Board::DrawMobileWatch(Graphics* g)
{
    if(mObserverView&&mObserverView->Draw(g,mApp->mOnlineSession.get())){CoopHud::WatchControls(g);return;}
    // Reuse the scene backdrop; only cached snapshot entities are drawn above it.
    DrawBackdrop(g);
    const auto& snap = mRemoteSnapshot;
    if (mHasRemoteSnapshot)
    {
        int rows=std::clamp((int)snap.mRows,1,MAX_GRID_SIZE_Y);
        for(int row=0;row<rows;++row) {
            if(!(snap.mMowers & (1<<row))) continue;
            LawnMowerType type=StageHasRoof() ? LAWNMOWER_ROOF :
                (StageHasPool() && mPlantRow[row]==PLANTROW_POOL ? LAWNMOWER_POOL : LAWNMOWER_LAWN);
            float y=GetPosYBasedOnRow(19,row)+42;
            if(type==LAWNMOWER_ROOF) y-=40;
            else if(type==LAWNMOWER_POOL) y-=33;
            mApp->mReanimatorCache->DrawCachedMower(g,-15,y,type);
        }
        for (int i=0; i<snap.mPlantCount; ++i)
        {
            const auto& p=snap.mPlants[i];
            if (p.mRow>=MAX_GRID_SIZE_Y || p.mCol>=MAX_GRID_SIZE_X || p.mSeedType>=NUM_SEED_TYPES) continue;
            int x=GridToPixelX(p.mCol,p.mRow), y=GridToPixelY(p.mCol,p.mRow);
            mApp->mReanimatorCache->DrawCachedPlant(g,x,y,(SeedType)p.mSeedType,DrawVariation::VARIATION_NORMAL);
            if(p.mHP<100) MobileWatchHealth(g,x+20,y+5,p.mHP);
        }
        for(int i=0;i<snap.mZombieCount;++i)
        {
            const auto& z=snap.mZombies[i];
            if(z.mRow>=MAX_GRID_SIZE_Y || z.mType>=NUM_CACHED_ZOMBIE_TYPES) continue;
            float y=GetPosYBasedOnRow(z.mX+40,z.mRow)-70;
            if(z.mType==ZOMBIE_BALLOON) y-=30;
            if(z.mType==ZOMBIE_GARGANTUAR || z.mType==ZOMBIE_REDEYE_GARGANTUAR) y-=20;
            ZombieType viewType=(ZombieType)z.mType;
            const int family=PlantZombies::Family(viewType);
            if(family>=0)viewType=PlantZombies::Form(family,
                (z.mFlags&NetProto::SNAPSHOT_ZFLAG_PLANT_HAT)?PlantZombies::Armor(viewType):0,
                (z.mFlags&NetProto::SNAPSHOT_ZFLAG_SHIELD)!=0);
            mApp->mReanimatorCache->DrawCachedZombie(g,z.mX-40,y,viewType);
            if(z.mHP<100) MobileWatchHealth(g,z.mX-20,y,z.mHP);
            if(z.mFlags & (NetProto::SNAPSHOT_ZFLAG_HELMET | NetProto::SNAPSHOT_ZFLAG_SHIELD)) {
                g->SetColor(Color(140,200,255)); g->FillRect(z.mX-27,y,4,4);
            }
            // @pvz-online: 接力怪金色小箭头（观看端按缓存图定位，本地叠加层使用实体矩形）
            if(z.mFlags & NetProto::SNAPSHOT_ZFLAG_RELAYED) {
                int aAX=z.mX+38, aAY=(int)y-9;
                g->SetColor(Color(35,30,20,230)); g->FillRect(aAX,aAY,18,18);
                g->SetColor(Color(255,215,70));
                g->FillRect(aAX+7,aAY+2,4,8);
                g->FillRect(aAX+3,aAY+8,12,3);
                g->FillRect(aAX+5,aAY+11,8,3);
                g->FillRect(aAX+7,aAY+14,4,2);
            }
        }
    }
    DrawMobileStrip(g,Rect(0,0,800,64));
    auto* session=mApp->mOnlineSession.get();
    std::string title=ModText::Tr("正在观看：", "Watching: ")+session->GetSeatName(session->GetWatchTargetSeat());
    if(mHasRemoteSnapshot) title += "  " + std::to_string(snap.mWave)+"/"+std::to_string(snap.mWaveTotal);
    else title += ModText::Tr("  等待画面…", "  Waiting for snapshot...");
    g->SetFont(ModText::FontFor(FONT_DWARVENTODCRAFT18,22));
    {
        Graphics text(*g); text.SetClipRect(0,0,520,64);
        text.SetColor(Color(245,230,165)); text.DrawString(title,14,38);
    }
    DrawMobileStone(g,Rect(530,8,125,46),ModText::Tr("换人", "Next"));
    DrawMobileStone(g,Rect(665,8,125,46),ModText::Tr("返回", "Back"));
    g->SetFont(ModText::FontFor(FONT_DWARVENTODCRAFT18,20));
    g->SetColor(Color(255,230,160));
    if(mHasRemoteSnapshot && (snap.mFlags & NetProto::SNAPSHOT_FLAG_TRUNCATED))
        g->DrawString(ModText::Tr("密集怪群：仅显示靠近房屋的目标", "Crowded lawn: showing targets nearest the house"),120,585);
    if(mHasRemoteSnapshot && (snap.mFlags & NetProto::SNAPSHOT_FLAG_PAUSED))
        g->DrawString(ModText::Tr("对方已暂停", "Teammate paused"),320,300);
    else if(mHasRemoteSnapshot && (snap.mFlags & NetProto::SNAPSHOT_FLAG_COMPLETE))
        g->DrawString(ModText::Tr("对方已清场", "Teammate cleared"),320,300);
}
