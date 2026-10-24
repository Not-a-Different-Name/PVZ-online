#include <climits>
#include "CoopPlay.h"
#include "Widget/NextWaveControl.h"
#include "Widget/AdmissionBoard.h"
#include "Widget/CoopHud.h"
#include "Widget/DamageStatsView.h"
// PC Build 46 cooperation rules, with native touch and UTF-8 drawing.
#include "../LawnApp.h"
#include "../PvzpLib/PvzpCommon.h"
#include "../PvzpLib/PvzpStringFile.h"
#include "Board.h"
#include "Coin.h"
#include "Online/NetSession.h"
#include "Online/QuickChat.h"
#include "SeedPacket.h"
#include "Widget/MobileCoopDraw.h"
static const int kSunGiftAmount = 100, kSunGiftTax = 10, kSunGiftSelectFrames = 500,
                 kSunGiftCooldownFrames = 300;

bool Board::QuickChatAvailable()
{
    if (mApp->IsLocalOnlyMode(mApp->mGameMode) || mApp->AdmissionBuilding()) return false;
    // 观看队友场地时不开面板：观看层盖住全屏，面板看不见却还在吞键（T/E 会被这里先接走）
    return mApp->IsOnlineGame() && mApp->mOnlineSession != nullptr && mApp->mOnlineSession->IsConnected() &&
           mApp->mGameScene == GameScenes::SCENE_PLAYING;
}

void Board::CloseQuickChatPanel()
{
    mChatPanelOpen = false;
    mChatPanelTimer = 0;
    // 关面板也吃一次冷却：Esc 关掉后同一下按键的自动重复不许立刻又把面板弹回去
    mChatInputCooldown = QUICK_CHAT_KEY_COOLDOWN;
}

bool Board::HandleQuickChatKey(KeyCode theKey)
{
    if (mChatPanelOpen)
    {
        // 面板开着：所有键一律吞掉（不落到 ESC 取消光标 / SPACE 暂停那条链），冷却中不产生动作
        if (mChatInputCooldown > 0)
            return true;
        if(theKey=='G'&&mApp->IsRunMode()){CloseQuickChatPanel();HandleSunGiftKey(theKey);return true;}
        mChatPanelTimer = QUICK_CHAT_PANEL_TIMEOUT; // 任意键都算"有人操作"，重置超时

        if (theKey == KeyCode::KEYCODE_ESCAPE)
        {
            CloseQuickChatPanel();
            return true;
        }
        if (theKey == 'T')
        {
            if (mChatEmotePage)
            {
                mChatEmotePage = false;
                mChatInputCooldown = QUICK_CHAT_KEY_COOLDOWN;
            }
            else
            {
                CloseQuickChatPanel(); // 已在短语页，再按 T = 关
            }
            return true;
        }
        if (theKey == 'E')
        {
            if (!mChatEmotePage)
            {
                mChatEmotePage = true;
                mChatInputCooldown = QUICK_CHAT_KEY_COOLDOWN;
            }
            else
            {
                CloseQuickChatPanel(); // 已在表情页，再按 E = 关
            }
            return true;
        }
        if (theKey >= '1' && theKey <= '9')
        {
            // 按当前页选发：短语页 1-8 发 1..8，表情页 1-8 发 9..16
            if (theKey - '0' > (mChatEmotePage ? QuickChat::EMOTE_COUNT : QuickChat::PHRASE_COUNT)) return true;
            uint8_t anId = (uint8_t)((theKey - '1') + 1 + (mChatEmotePage ? QuickChat::PHRASE_COUNT : 0));
            CloseQuickChatPanel();
            if (mApp->SendCoopQuickChat(anId))
            {
                PushQuickChatBanner(mApp->mOnlineSession->GetLocalSeat(), anId); // 本地回显（我：…）
            }
            // 发送失败（掉线）就什么都不显示——掉线提示走既有路径
            return true;
        }
        return true;
    }

    // 面板关着：只认 T / E，其余原样放回（RETURN/SPACE/ESC 那条链不受影响）
    if (theKey != 'T' && theKey != 'E')
        return false;
    if (mChatInputCooldown > 0)
        return true; // 冷却中吞掉，防自动重复把面板弹回去
    if (!QuickChatAvailable())
        return false; // 单机 / 过场 / 选卡：不响应（焦点本就不在棋盘）
    mCoopToolsOpen = false;
    mSunGiftSelectFrames=0;
    mChatPanelOpen = true;
    mChatEmotePage = (theKey == 'E');
    mChatPanelTimer = QUICK_CHAT_PANEL_TIMEOUT;
    mChatInputCooldown = QUICK_CHAT_KEY_COOLDOWN;
    return true;
}

void Board::PushQuickChatBanner(uint8_t theSeat, uint8_t theId)
{
    // 横幅窗口 3 条，满了丢最旧保最新（与会话层收包队列同一条策略）
    if (mChatBannerCount >= QUICK_CHAT_BANNER_MAX)
    {
        for (int i = 1; i < mChatBannerCount; i++)
            mChatBanners[i - 1] = mChatBanners[i];
        mChatBannerCount--;
    }
    mChatBanners[mChatBannerCount].mSeat = theSeat;
    mChatBanners[mChatBannerCount].mId = theId;
    mChatBanners[mChatBannerCount].mFrames = QUICK_CHAT_BANNER_FRAMES;
    mChatBanners[mChatBannerCount].mCustom[0] = 0;
    mChatBannerCount++;
}

void Board::PushCustomBanner(const char *theTextUtf8)
{
    if (mChatBannerCount >= QUICK_CHAT_BANNER_MAX)
    {
        for (int i = 1; i < mChatBannerCount; i++)
            mChatBanners[i - 1] = mChatBanners[i];
        mChatBannerCount--;
    }
    mChatBanners[mChatBannerCount].mSeat = 0;
    mChatBanners[mChatBannerCount].mId = 0;
    mChatBanners[mChatBannerCount].mFrames = QUICK_CHAT_BANNER_FRAMES;
    snprintf(mChatBanners[mChatBannerCount].mCustom, sizeof(mChatBanners[mChatBannerCount].mCustom), "%s",
             theTextUtf8 ? theTextUtf8 : "");
    mChatBannerCount++;
}

void Board::UpdateQuickChat()
{
    UpdateRelayBatches();	// @pvz-online: 接力来源合并批的倒计时与到期横幅（2026-10-15）

    // 发阳光选择态倒计时：超时自动退出（等待必须有出路）
    if (mSunGiftSelectFrames > 0)
        mSunGiftSelectFrames--;

    // 先收：队友的喊话不因我按了暂停就丢——横幅会冻着，继续了再轮播
    if (mApp->mOnlineSession != nullptr)
    {
        NetProto::MsgQuickChat aMsg;
        while (mApp->mOnlineSession->TakePendingQuickChat(aMsg))
        {
            PushQuickChatBanner(aMsg.mSrcSeat, aMsg.mId);
        }
    }

    if (!QuickChatAvailable())
    {
        if(!CoopHud::Watching(this)){mCoopTouchCaptured=false;mCoopTouchAction=-1;mCoopTouchPeerLow=mCoopTouchPeerHigh=0;}

        // 掉线/收摊/不在局里：横幅与冷却一并清掉，别让上一局的话漂进新局面
        // （会话层收包队列由 DiscardLevelPackets/会话复位负责）
        if (mChatPanelOpen)
            CloseQuickChatPanel();
        mCoopToolsOpen = false;
        mChatBannerCount = 0;
        mChatInputCooldown = 0;
        mSunGiftSelectFrames = 0;
        return;
    }

    if (mChatInputCooldown > 0)
        mChatInputCooldown--;
    if (mSunGiftCooldownFrames > 0)
        mSunGiftCooldownFrames--;
    if (mPaused || !mApp->mActive || mApp->mMinimized) {
        mCoopToolsOpen = false;
        mCoopTouchCaptured=false;mCoopTouchAction=-1;mCoopTouchPeerLow=mCoopTouchPeerHigh=0;
        mSunGiftSelectFrames = 0;
    }

    if (mChatPanelOpen)
    {
        if (mPaused || !mApp->mActive || mApp->mMinimized)
        {
            CloseQuickChatPanel(); // 暂停菜单要弹出来了，面板压在底下没意义
        }
        else
        {
            mChatPanelTimer--;
            if (mChatPanelTimer <= 0)
                CloseQuickChatPanel(); // ≈10 秒无操作自动关（退路之一）
        }
    }

    // 横幅各自计时（2026-10-11 起三条同屏）：全体一起走，到期的原位摘除、保持顺序；
    // 暂停时冻着不推进
    if (!mPaused && mChatBannerCount > 0)
    {
        for (int i = 0; i < mChatBannerCount; i++)
            mChatBanners[i].mFrames--;
        int aWrite = 0;
        for (int i = 0; i < mChatBannerCount; i++)
        {
            if (mChatBanners[i].mFrames > 0)
            {
                if (aWrite != i)
                    mChatBanners[aWrite] = mChatBanners[i];
                aWrite++;
            }
        }
        mChatBannerCount = aWrite;
    }
}

bool Board::TrySendSunGift(uint8_t theTargetSeat)
{
    if (!mApp->IsOnlineGame() || !mApp->IsRunMode() || !mApp->mOnlineSession) return false;
    // @pvz-online: a successful relay write is not a delivery receipt; share the idempotent gift path.
    if (!mApp->mCoopPlay) mApp->mCoopPlay=std::make_unique<CoopPlay>(mApp);
    const bool sent=mApp->mCoopPlay->GiveSun(theTargetSeat);
    PushCustomBanner(mApp->mCoopPlay->notice.c_str());
    return sent;
}

bool Board::HandleSunGiftKey(KeyCode theKey)
{
    if (!mApp->IsOnlineGame() || !mApp->IsRunMode())
        return false;
    if (mApp->mOnlineSession == nullptr)
        return false;
    bool aSelecting = mSunGiftSelectFrames > 0;

    if (theKey == 'G')
    {
        if (aSelecting)
        {
            mSunGiftSelectFrames = 0;
            PushCustomBanner(ModText::Tr("已取消发阳光", "Sun gift cancelled"));
            return true;
        }
        mSunGiftSelectFrames = kSunGiftSelectFrames;
        PushCustomBanner(
            ModText::Tr("发阳光：按 1-6 选队友（100，到账 90）", "Sun gift: press 1-6 to pick a teammate"));
        return true;
    }

    if (!aSelecting)
        return false;
    if (theKey == KEYCODE_ESCAPE)
    {
        mSunGiftSelectFrames = 0;
        return true;
    }
    if (theKey >= '1' && theKey <= '6')
    {
        uint8_t aSeat = (uint8_t)(theKey - '0');
        if (aSeat == mApp->mOnlineSession->GetLocalSeat())
        {
            PushCustomBanner(ModText::Tr("请选择其他队友", "Choose another teammate"));
            return true;
        }
        if (!mApp->mOnlineSession->IsSeatOccupied(aSeat))
        {
            PushCustomBanner(ModText::Tr("换一位在线队友", "Choose an online teammate"));
            return true;
        }
        TrySendSunGift(aSeat);
        mSunGiftSelectFrames = 0; // 成败都退出选择态（冷却/钱不足的横幅已给原因）
        return true;
    }
    return false; // 其他键不吞、选择态继续（超时自动退）
}

void Board::GiveSunGift(int theAmount, uint8_t theFromSeat)
{
    mSunMoney = int(std::clamp<int64_t>(int64_t(mSunMoney)+theAmount,0,mApp->IsRunMode()?INT_MAX:9990));
    if(theAmount>0 && mApp->mCoopPlay)mApp->mCoopPlay->SunReceived();
    // @pvz-online: 收方横幅带发送者名字（2026-10-13 用户令「显示 XXX 送你阳光」）；
    // 席位名口径同聊天/掉线横幅，空名兜底 P<席位>。
    std::string aNameUtf8;
    if (mApp->mOnlineSession != nullptr)
        aNameUtf8 = mApp->mOnlineSession->GetSeatName(theFromSeat);
    if (aNameUtf8.empty())
        aNameUtf8 = "P" + std::to_string((unsigned)theFromSeat);
    PushCustomBanner((aNameUtf8 + ModText::Tr("送你阳光", " sent you sun")).c_str());
    // 天降演出：三枚面额币从天上落到场上（50+25+15 的视觉示意，落地自动飞向计数器）。
    // mVisualOnly 让它们收集时不二次入账、也不算进「在途阳光」种植预算——金额以横幅为准。
    int aBaseX = RandRangeInt(100, 529);
    Coin *aCoin = AddCoin(aBaseX, 60, CoinType::COIN_LARGESUN, CoinMotion::COIN_MOTION_FROM_SKY);
    if (aCoin != nullptr)
        aCoin->mVisualOnly = true;
    aCoin = AddCoin(aBaseX + 45, 60, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_SKY);
    if (aCoin != nullptr)
        aCoin->mVisualOnly = true;
    aCoin = AddCoin(aBaseX + 90, 60, CoinType::COIN_SMALLSUN, CoinMotion::COIN_MOTION_FROM_SKY);
    if (aCoin != nullptr)
        aCoin->mVisualOnly = true;
}
int Board::CoopHit(int x,int y){return CoopHud::Hit(this,x,y);}
bool Board::CoopMouseDown(int x,int y,int clicks) {
    if((!QuickChatAvailable()&&!CoopHud::Watching(this))||mPaused||!mApp->mActive||mApp->mMinimized||clicks<=0)return false;
    int hit=CoopHit(x,y);if(hit<0)return false;
    // Keep a late release out of lawn input even if focus loss clears HUD capture first.
    mIgnoreMouseUp=true;
    mCoopTouchCaptured=true;mCoopTouchAction=hit;mCoopTouchPeerLow=mCoopTouchPeerHigh=0;
    if((hit>=101&&hit<=106)||(hit>=301&&hit<=306)) {
        auto* s=RoomPresentation::Session(mApp);int seat=hit-(hit>=301?300:100);auto token=s->mCoopPeers[seat];mCoopTouchPeerLow=(uint32_t)token;mCoopTouchPeerHigh=(uint32_t)(token>>32);
    }
#ifdef __ANDROID__
    mTouchSeedDrag=false;mTouchSeedDragMoved=false;
#endif
    return true;
}
bool Board::CoopMouseUp(int x,int y,int clicks) {
    if(!mCoopTouchCaptured)return false;
    int action=mCoopTouchAction;uint64_t peer=((uint64_t)mCoopTouchPeerHigh<<32)|mCoopTouchPeerLow;
    mCoopTouchCaptured=false;mCoopTouchAction=-1;mCoopTouchPeerLow=mCoopTouchPeerHigh=0;
    if(clicks<=0||mPaused||!mApp->mActive||mApp->mMinimized||(!QuickChatAvailable()&&!CoopHud::Watching(this))||CoopHit(x,y)!=action)return true;
    if((action>=101&&action<=106)||(action>=301&&action<=306)) {
        auto* s=RoomPresentation::Session(mApp);int seat=action-(action>=301?300:100);
        if(!peer||s->mCoopPeers[seat]!=peer)return true;
    }
    CoopAction(action);return true;
}
void Board::CoopAction(int action){CoopHud::Action(this,action);}
void Board::DrawCoopUI(Graphics* g){NextWaveControl::Draw(this,g);AdmissionBoard::Draw(this,g);DamageStatsView::Draw(this,g);CoopHud::Draw(this,g);}
