#include "../CoopPlay.h"
#include "MobileCoopDraw.h"
#include "OnlineUiDraw.h"
#include "RunFlowScreen.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../Run/RunState.h"
#include "../Run/PortVersion.h"
#include "../Run/RunBuffs.h"
#include "../Run/RunPortSupport.h"
#include "../ModText.h"
#include "../System/PlayerInfo.h"
#include "../SeedPacket.h"
#include "../../PvzpLib/PvzpCommon.h"
#include "widget/ButtonWidget.h"
#include "widget/Dialog.h"
#include "graphics/Graphics.h"
#include "graphics/Font.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include <format>

namespace {
class RerollClickButton:public MobileStoneButton {
public:
    RerollClickButton(int id,Sexy::ButtonListener* listener):MobileStoneButton(id,listener){}
    void MouseUp(int x,int y,int button,int clicks) override {
        if(clicks>1){Sexy::Widget::MouseUp(x,y,button,clicks);return;}
        MobileStoneButton::MouseUp(x,y,button,clicks);
    }
};
// Match PC RunModeDialog's challenge cards and icon IDs, with the same click-to-start flow.
class RunModeCard : public Sexy::ButtonWidget {
    int icon;
public:
    RunModeCard(int id, Sexy::ButtonListener* listener, int cell) : ButtonWidget(id,listener),icon(cell) {}
    void Draw(Sexy::Graphics* g) override {
        int offset=mIsDown&&mIsOver?1:0;
        g->DrawImageCel(Sexy::IMAGE_CHALLENGE_THUMBNAILS,13+offset,4+offset,icon);
        g->DrawImage(mIsOver?Sexy::IMAGE_CHALLENGE_WINDOW:Sexy::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT,-6+offset,-2+offset);
        auto* font=ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18,18);
        g->SetFont(font);g->SetColor(Sexy::Color(42,42,90));
        g->DrawString(MobileTextLine(font,mLabel,104),offset+(104-font->StringWidth(mLabel))/2,100+offset);
    }
};

// PC rewards group the seed image, description and owned count in each column.
class RunChoiceCard : public Sexy::ButtonWidget {
    LawnApp* app;
public:
    RunChoiceCard(int id, Sexy::ButtonListener* listener, LawnApp* a) : ButtonWidget(id,listener),app(a) {}
    void Draw(Sexy::Graphics* g) override {
        auto& run=*app->mRunState;
        bool plantPick=run.IsPlantPick();
        int choice=plantPick?static_cast<int>(run.mPlantChoices[mId]):run.mBuffChoices[mId];
        if(mDisabled){DrawMobileStone(g,Sexy::Rect(0,mHeight-48,mWidth,48),mLabel,false,false);return;}
        auto* font=ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18,20);
        if(!plantPick){
            const char* plantName=GetRunChoicePlantName(choice);
            std::string head=plantName?std::string(plantName):ModText::Tr("全局增益","Global bonus");
            int rarity=GetRunChoiceRarity(choice);
            for(int i=0;i<rarity;++i)head+=" ★";
            PvzpDrawStringWrapped(g,head,Sexy::Rect(4,0,mWidth-8,55),font,
                rarity>=3?Sexy::Color(255,200,60):Sexy::Color(205,205,190),DrawStringJustification::DS_ALIGN_CENTER);
            std::string desc=GetRunChoiceDesc(choice);
            Sexy::Rect descRect(6,60,mWidth-12,mHeight-152);
            for(int size=20;size>=14;size-=2) {
                font=ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18,size);
                if(PvzpDrawStringWrappedHelper(g,desc,descRect,font,Sexy::Color::White,DS_ALIGN_LEFT,false)<=descRect.mHeight)break;
            }
            PvzpDrawStringWrapped(g,desc,descRect,font,Sexy::Color(245,230,165),DrawStringJustification::DS_ALIGN_LEFT);
            std::string owned=ModText::Format("层数 {}", "Stacks {}",run.GetBuffCount(choice));
            int cap=GetRunChoiceMaxStacks(choice);
            if(cap>0)owned+=" / "+std::to_string(cap);
            PvzpDrawStringWrapped(g,owned,Sexy::Rect(4,mHeight-84,mWidth-8,30),ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18,20),Sexy::Color(205,205,190),DrawStringJustification::DS_ALIGN_CENTER);
        } else {
            DrawSeedPacket(g,(mWidth-SEED_PACKET_WIDTH)/2,8,(SeedType)choice,SEED_NONE,0,255,false,false);
            PvzpDrawStringWrapped(g,ModText::Tr("加入本局卡池", "Add to this run"),Sexy::Rect(8,126,mWidth-16,80),font,Sexy::Color(205,205,190),DrawStringJustification::DS_ALIGN_CENTER);
        }
        DrawMobileStone(g,Sexy::Rect(0,mHeight-48,mWidth,48),mLabel,mIsDown&&mIsOver,true);
    }
};
}

RunFlowScreen::RunFlowScreen(LawnApp* app, bool modePage, int endlessScene) : mApp(app), mModePage(modePage), mEndlessPage(endlessScene>=0)
{
    mApp->mDamageStats.LoadSettings();
    Resize(0, 0, 800, 600);
    if(endlessScene>=0 && endlessScene<5)mEndlessScene=endlessScene;
    auto addButton = [&](int id, const std::string& text, int x, int y, int width, int height) {
        MobileStoneButton* button = id==4?static_cast<MobileStoneButton*>(new RerollClickButton(id,this)):new MobileStoneButton(id, this);
        button->mLabel = text;
        button->SetFont(ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18, 28));
        button->SetLabelColor(Sexy::Color(245, 230, 165));
        button->SetLabelHiliteColor(Sexy::Color::White);
        button->SetBkgColor(Sexy::Color(37, 83, 55));
        button->Resize(x, y, width, height);
        AddWidget(button);
        if(mModePage)mModeButtons.push_back(button);
        return button;
    };
    if(!mModePage && app->IsOnlineGame())addButton(30,ModText::Tr("队友互助","Teamwork"),570,65,190,42);
    if (mModePage)
    {
        PvzpLoadResources("DelayLoad_ChallengeScreen");
        const int icons[]={19,16,18};
        if(!mEndlessPage)for(int i=0;i<3;++i){
            auto* card=new RunModeCard(10+i,this,icons[i]);
            const char* labels[]={ModText::Tr("完整25关","Full 25"),ModText::Tr("普通10关","Normal 10"),ModText::Tr("快速5关","Quick 5")};
            card->mLabel=labels[i];card->Resize(174+i*174,160,104,115);AddWidget(card);mModeButtons.push_back(card);
        }
        addButton(15, mEndlessPage?ModText::Tr("开始无尽", "Start endless"):ModText::Tr("无尽", "Endless"), 300, 292, 200, 48);
        for(int i=0;i<3;++i)addButton(40+i,ModText::Tr(i==0?"轻松 x0.5":i==1?"标准 x1":"高压 x2",i==0?"Easy x0.5":i==1?"Standard x1":"High x2"),122+i*190,400,176,48);
        for (int i = 0; i < 6; ++i)
            mOptionButtons[i] = addButton(20 + i, "", 64 + (i % 2) * 344, 154 + (i / 2) * 74, 328, 54);
        RefreshOptionLabels();
        addButton(34,ModText::Tr("阶段主题：开","Stage themes: on"),64,188,328,64);
        addButton(35,ModText::Tr("候选偏好：阵容","Buff preference: lineup"),408,188,328,64);
        addButton(36, "", 64, 188, 328, 64);
        addButton(37, "", 408, 188, 328, 64);
        addButton(38, "", 64, 298, 328, 64);
        RefreshDamageLabels();
        mZombieBuffButton = addButton(27, ModText::Tr("僵尸强化：1", "Zombie boosts: 1"), 64, 376, 328, 54);
        const char* ladders[] = { "1.33", "1", "1.2", "1.5", "2" };
        mLadderButton = addButton(26, ModText::Format("场景阶梯：x{}", "Scene ladder: x{}", ladders[mLadder]), 408, 376, 328, 54);
        mBuffMultButton = addButton(14, ModText::Format("增益倍选：x{}", "Buff picks: x{}", RunState::BuffMultValueFor(mBuffMult)), 64, 450, 328, 44);
        mSpeedButton=addButton(29,ModText::Tr(mEndlessPage?"移速成长（每五关）：无":"移速成长（每场景）：无", "Speed growth: none"),408,450,328,44);
        mFirstWaveButton=addButton(33,ModText::Tr("首波等待：30 s", "First wave: 30 sec"),64,450,328,44);
        addButton(31, ModText::Tr("上一页", "Previous"), 64, 448, 164, 48);
        addButton(32, ModText::Tr("下一页", "Next"), 572, 448, 164, 48);
        addButton(13, mEndlessPage?ModText::Tr("继续无尽", "Continue endless"):ModText::Tr("继续闯关", "Continue run"), 310, 510, 180, 48)->SetDisabled(!RunState::HasCheckpoint(app->mPlayerInfo->mId,mEndlessPage?RunState::RUN_MODE_ENDLESS:-1,mEndlessScene));
        addButton(28,ModText::Tr("更多设置", "More settings"),90,510,180,48);
    }
    else
    {
        RunState& run = *app->mRunState;
        if (!run.ChoicesRolled()) run.RollChoices();
        mPickRun=&run;mExpectedCounter=run.mPickCounter;
        for (int i = 0; i < RunState::RUN_CHOICES; ++i)
        {
            const bool plant = run.IsPlantPick();
            const bool valid = plant ? run.mPlantChoices[i] != SEED_NONE : run.mBuffChoices[i] != RunState::RUN_BUFF_CHOICE_NONE;
            const char* label = !valid ? ModText::Tr("不可用", "Unavailable") : plant ? GetRunPlantName(run.mPlantChoices[i]) : GetRunChoiceName(run.mBuffChoices[i]);
            auto* card=new RunChoiceCard(i,this,app);card->mLabel=label?label:"Plant";
            card->Resize(56+i*240,198,208,290);card->SetDisabled(!valid);AddWidget(card);
            mLockButtons[i]=addButton(150+i,"",56+i*240,164,208,30);
        }
        // @pvz-online: the top corners leave the wallet and current-stage strip unobstructed.
        if(run.mAdmission.phase==Admission::PICKS)addButton(60,ModText::Tr("快速完成","Quick catch-up"),56,65,208,42);
        mRefreshButton=addButton(4,"",56,518,240,48);
        RefreshRerollLabels();
        addButton(5, ModText::Tr("跳过本次", "Skip this pick"), 568, 518, 176, 48);
    }
    addButton(9, ModText::Tr("返回主菜单", "Back to menu"), mModePage?530:312,mModePage?510:518,208,48);
    if(mModePage)RefreshModePage();
}

void RunFlowScreen::RefreshModePage()
{
    for(auto* b:mModeButtons){
        bool advanced=(b->mId>=20&&b->mId<=27)||b->mId==14||b->mId==29||(b->mId>=33&&b->mId<=38);
        const std::vector<int> optionIds = mEndlessPage?std::vector<int>{20,21,22,33,24,29,27,23,14,34,35,25,36,37,38}:std::vector<int>{20,21,22,23,24,14,29,27,26,33,-1,-1,36,37,38};
        int optionIndex = -1;
        for (int index = 0; index < (int)optionIds.size(); ++index)
            if (b->mId == optionIds[index]) optionIndex = index;
        bool navigation = b->mId == 31 || b->mId == 32;
        bool visible = b->mId == 9 || b->mId == 28 ||
            (navigation ? mAdvanced : advanced ? mAdvanced && optionIndex>=0 && optionIndex / 4 == mAdvancedPage : !mAdvanced);
        b->SetVisible(visible);
        if (advanced && optionIndex >= 0)
            b->Resize(64 + (optionIndex % 2) * 344, 188 + ((optionIndex % 4) / 2) * 110, 328, 64);
        if(b->mId>=40&&b->mId<=42)b->mInverted=(b->mId-40==mDifficulty);
        if(b->mId==28)b->mLabel=mAdvanced?ModText::Tr("返回模式","Back"):ModText::Tr("更多设置", "More settings");
    }
    MarkDirty();
}

RunFlowScreen::~RunFlowScreen() { RemoveAllWidgets(true); }

void RunFlowScreen::RefreshRerollLabels()
{
    if(!mPickRun)return;
    const int cost=mPickRun->GetRerollPurchaseCost(mLockedChoice);
    mRefreshButton->mLabel=cost>0?ModText::Tr("重摇 -","Reroll -")+LawnApp::GetMoneyString(cost):ModText::Tr("使用重摇券","Use voucher");
    mRefreshButton->SetDisabled(!mApp->mPlayerInfo || mApp->mPlayerInfo->mCoins<cost);
    for(int index=0;index<3;++index) {
        mLockButtons[index]->mLabel=mLockedChoice==index?ModText::Tr("取消锁定","Unlock"):ModText::Tr("锁定","Lock");
        mLockButtons[index]->SetDisabled(!mPickRun->CanLockChoice(index));
    }
}

void RunFlowScreen::RefreshDamageLabels()
{
    const auto& stats = mApp->mDamageStats;
    for (auto* button : mModeButtons)
    {
        if (button->mId == 36) button->mLabel = ModText::Format("统计：{}", "Statistics: {}", stats.WholeRun() ? ModText::Tr("本轮", "Run") : ModText::Tr("本关", "Level"));
        if (button->mId == 37) button->mLabel = ModText::Format("总伤害：{}", "Total damage: {}", stats.ShowTotal() ? ModText::Tr("开", "On") : ModText::Tr("关", "Off"));
        if (button->mId == 38) button->mLabel = ModText::Format("BOSS伤害：{}", "Boss damage: {}", stats.ShowBoss() ? ModText::Tr("开", "On") : ModText::Tr("关", "Off"));
    }
    MarkDirty();
}

void RunFlowScreen::RefreshOptionLabels()
{
    const char* difficulties[] = { ModText::Tr("轻松", "Easy"), ModText::Tr("标准", "Standard"), ModText::Tr("高压", "High") };
    const char* scales[] = { ModText::Tr("少量", "Half"), ModText::Tr("标准", "Standard"), ModText::Tr("多量", "Double"), ModText::Tr("海量", "Quadruple") };
    const char* tempos[] = { ModText::Tr("快", "Fast"), ModText::Tr("标准", "Standard"), ModText::Tr("慢", "Slow") };
    const char* scenes[] = { ModText::Tr("白天", "Day"), ModText::Tr("夜晚", "Night"), ModText::Tr("泳池", "Pool"), ModText::Tr("迷雾", "Fog"), ModText::Tr("屋顶", "Roof") };
    mOptionButtons[0]->mLabel = ModText::Format("难度：{}", "Difficulty: {}", difficulties[mDifficulty]);
    mOptionButtons[1]->mLabel = ModText::Format("规模：{}", "Scale: {}", scales[mScale]);
    mOptionButtons[2]->mLabel = ModText::Format("快慢：{}", "Tempo: {}", tempos[mTempo]);
    mOptionButtons[3]->mLabel = ModText::Format("植物僵尸：{}", "Zombotany: {}", mZombotany ? ModText::Tr("开", "On") : ModText::Tr("关", "Off"));
    mOptionButtons[4]->mLabel = ModText::Format("关底头目：{}", "Final boss: {}", mBossFlag==2?ModText::Tr("末关+场景3", "Final + Scene 3"):mBossFlag==1?ModText::Tr("仅末关","Final"):ModText::Tr("关","Off"));
    if(mEndlessPage) {
        const char* cycles[]={ModText::Tr("关","Off"),ModText::Tr("每十关","Every 10"),ModText::Tr("每五关","Every 5")};
        mOptionButtons[4]->mLabel=ModText::Format("周期 BOSS：{}","Boss cycle: {}",cycles[mEndlessOptions&3]);
    }
    mCheckpointLine=RunState::CheckpointSummary(mApp->mPlayerInfo->mId,mEndlessPage?RunState::RUN_MODE_ENDLESS:-1,mEndlessScene);
    for(auto* button:mModeButtons)if(button->mId==13)button->SetDisabled(mCheckpointLine.empty());
    mOptionButtons[5]->mLabel = ModText::Format("无尽场景：{}", "Endless scene: {}", scenes[mEndlessScene]);
    MarkDirty();
}

void RunFlowScreen::Draw(Sexy::Graphics* g)
{
    DrawMobilePanel(g,Sexy::Rect(20,20,760,560));
    auto* titleFont=ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT24,30);
    std::string title=mModePage?(mAdvanced?(mEndlessPage?ModText::Tr("无尽高级选项","Endless options"):ModText::Tr("闯关高级选项","Run options")):(mEndlessPage?ModText::Tr("无尽准备","Endless setup"):ModText::Tr("闯关模式","Roguelike Run"))):
        mApp->mRunState->IsPlantPick()?ModText::Tr("选择植物","Choose a plant"):ModText::Tr("选择增益","Choose a bonus");
    g->SetFont(titleFont);g->SetColor(Sexy::Color(245,230,165));
    g->DrawString(title,(800-titleFont->StringWidth(title))/2,90);
    auto* font=ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18,20);
    if(mModePage){
        auto text=mAdvanced?ModText::Tr("点按选项切换；设置完成后点“返回模式”。", "Tap an option to change it, then choose Back."):
            mEndlessPage?ModText::Tr("选好场景后开始或继续；各场景分别存档。", "Choose a scene, then start or continue. Each scene has its own save."):ModText::Tr("先选难度，需要时点“更多设置”，再点模式卡开始。", "Choose difficulty, adjust More settings if needed, then choose a mode card.");
        PvzpDrawStringWrapped(g,text,Sexy::Rect(80,104,640,46),font,Sexy::Color(205,205,190),DrawStringJustification::DS_ALIGN_CENTER);
        if(!mAdvanced && mEndlessPage) {
            PvzpDrawStringWrapped(g,mOptionButtons[5]->mLabel,Sexy::Rect(80,185,640,40),font,Sexy::Color(245,230,165),DS_ALIGN_CENTER);
            PvzpDrawStringWrapped(g,mCheckpointLine.empty()?ModText::Tr("保留阵型 · 全植物 · 十卡槽 · 五关一阶段","Keep your lawn / All plants / 10 slots / 5-level stages"):mCheckpointLine,Sexy::Rect(80,228,640,54),font,Sexy::Color(205,205,190),DS_ALIGN_CENTER);
        }
        if(!mAdvanced)PvzpDrawStringWrapped(g,ModText::Tr("出怪难度","Zombie difficulty"),Sexy::Rect(150,362,500,28),font,Sexy::Color(205,205,190),DrawStringJustification::DS_ALIGN_CENTER);
        else {
            if(mEndlessPage && mAdvancedPage != 3)PvzpDrawStringWrapped(g,ModText::Tr("固定场景 · 保留阵型 · 每五关进入新阶段","Fixed field / Keep lineup / New stage every 5 levels"),Sexy::Rect(80,380,640,58),font,Sexy::Color(205,205,190),DS_ALIGN_CENTER);
            const char* groups[] = {ModText::Tr("基础设置 · 点按切换档位", "Basics · tap to change"),
                ModText::Tr("成长与首领 · 点按切换档位", "Growth and bosses · tap to change"),
                ModText::Tr("场景设置 · 点按切换档位", "Scenes · tap to change"), ModText::Tr("伤害统计 · 仅本机显示", "Damage statistics / Local display")};
            const char* endlessGroups[]={ModText::Tr("准备与出怪","Preparation and spawns"),ModText::Tr("阶段挑战与强化","Stage challenges and boosts"),ModText::Tr("阵容、增益与场景","Lineup, buffs and scene"),ModText::Tr("伤害统计 · 仅本机显示", "Damage statistics / Local display")};
            PvzpDrawStringWrapped(g, mEndlessPage?endlessGroups[mAdvancedPage]:groups[mAdvancedPage], Sexy::Rect(80,150,640,30),font,Sexy::Color(205,205,190),DS_ALIGN_CENTER);
            if (mAdvancedPage == 2 && !mEndlessPage) PvzpDrawStringWrapped(g,ModText::Tr("场景阶梯提高血量和出怪数量；BOSS按所选时机出现。", "Scene growth increases HP and zombie count. Bosses follow your selected timing."),Sexy::Rect(80,380,640,58),font,Sexy::Color(205,205,190),DS_ALIGN_CENTER);
            if (mAdvancedPage == 3) PvzpDrawStringWrapped(g, ModText::Tr("总统计包含BOSS伤害；两个显示开关互不影响。", "Total includes boss damage; display switches are independent."), Sexy::Rect(80,380,640,58),font,Sexy::Color(205,205,190),DS_ALIGN_CENTER);
            PvzpDrawStringWrapped(g, ModText::Format("第 {} / 4 页", "Page {} / 4", mAdvancedPage + 1),
                Sexy::Rect(240,456,320,32), font, Sexy::Color(245,230,165), DS_ALIGN_CENTER);
        }
    }else{
        const auto& run=*mApp->mRunState;
        SeatUi::Strip(g, Sexy::Rect(48,112,704,46));
        auto state=run.IsEndless()?ModText::Format("无尽第 {} 关","Endless level {}",run.mLevelIndex+1):ModText::Format("第 {} / {} 关","Level {} / {}",run.mLevelIndex+1,run.GetLevelCount());
        if(run.mAdmission.phase==Admission::PICKS)state=ModText::Format("加入第 {} 关 · 还需选择 {} 次", "Joining level {} / {} picks left",run.mLevelIndex+1,run.mPendingBuffPicks);
        g->DrawImage(Sexy::IMAGE_COINBANK,64,118);
        g->SetFont(Sexy::FONT_CONTINUUMBOLD14);g->SetColor(Sexy::Color(180,255,90));
        auto money=LawnApp::GetMoneyString(mApp->mPlayerInfo->mCoins);
        g->DrawString(money,180-Sexy::FONT_CONTINUUMBOLD14->StringWidth(money),142);
        PvzpDrawStringWrapped(g,state,Sexy::Rect(222,118,514,32),font,Sexy::Color(205,205,190),DrawStringJustification::DS_ALIGN_CENTER);
        const bool voucher = run.mRerollVouchers > 0;
        std::string cost = voucher ? ModText::Tr("重摇：使用1张券", "Reroll: use 1 voucher")
            : ModText::Tr("本次重摇：", "Reroll cost: ") + LawnApp::GetMoneyString(run.GetPaidRerollCost());
        if(mLockedChoice>=0)cost+=ModText::Tr(" + 锁定 "," + lock ")+LawnApp::GetMoneyString(100);
        if (mApp->mPlayerInfo->mCoins < run.GetRerollPurchaseCost(mLockedChoice))
            cost += ModText::Tr(" · 金币不足", " / Not enough coins");
        PvzpDrawStringWrapped(g,cost,Sexy::Rect(56,492,688,24),ModText::FontFor(Sexy::FONT_BRIANNETOD16,18),
            Sexy::Color(245,230,165),DS_ALIGN_CENTER);
    }
}

void RunFlowScreen::ButtonDepress(int id)
{
    if (mModePage && id >= 36 && id <= 38)
    {
        const int setting = mApp->mDamageStats.mSetting ^ (1 << (id - 36));
        if (!mApp->mDamageStats.SaveSettings(setting))
            mApp->LawnMessageBox(Dialogs::DIALOG_MESSAGE, ModText::Tr("保存失败", "Save failed"),
                ModText::Tr("统计设置未保存，请检查可用存储。", "Statistics settings were not saved. Check storage."), "OK", "", Sexy::Dialog::BUTTONS_FOOTER);
        RefreshDamageLabels();
        return;
    }
    if(mModePage && (id==34 || id==35)) {
        int bit=id==34?4:8;mEndlessOptions^=bit;
        for(auto* b:mModeButtons)if(b->mId==id)b->mLabel=id==34?
            ModText::Format("阶段主题：{}","Stage themes: {}",(mEndlessOptions&bit)?ModText::Tr("开","On"):ModText::Tr("关","Off")):
            ModText::Format("候选偏好：{}","Buff preference: {}",(mEndlessOptions&bit)?ModText::Tr("阵容","Lineup"):ModText::Tr("随机","Random"));
        MarkDirty();return;
    }

    if(id==30){mApp->ShowCoopPlay();return;}
    if(id==60){mApp->StartAdmissionFastPicks();return;}
    if(mApp->mCoopPlay && mApp->mCoopPlay->Blocking())return;
    if (id == 9) {
        if (!mModePage && mApp->mRunState) {
            if (!mApp->mRunState->SaveWithPlayer(*mApp->mPlayerInfo)) {
                mApp->LawnMessageBox(Dialogs::DIALOG_MESSAGE,ModText::Tr("保存失败","Save failed"),
                    ModText::Tr("进度尚未保存，请检查存储后再返回。","Progress was not saved. Check storage before leaving."),
                    "OK","",Sexy::Dialog::BUTTONS_FOOTER);return;
            }
            if (mApp->mCoopPlay) mApp->mCoopPlay->Close();
            mApp->DoBackToMain(true);
        } else mApp->ShowGameSelector();
        return;
    }
    if (mModePage)
    {
        if(id==28){mAdvanced=!mAdvanced;RefreshModePage();return;}
        if (id == 31 || id == 32) {
            mAdvancedPage = (mAdvancedPage + (id == 31 ? 3 : 1)) % 4;
            RefreshModePage();
            return;
        }
        if(id==33){
            mFirstWave = mFirstWave == RunState::RUN_FIRSTWAVE_STD ? RunState::RUN_FIRSTWAVE_LONG :
                mFirstWave == RunState::RUN_FIRSTWAVE_LONG ? RunState::RUN_FIRSTWAVE_VANILLA : RunState::RUN_FIRSTWAVE_STD;
            const int delays[]={30,18,60};
            mFirstWaveButton->mLabel=ModText::Format("首波等待：{} s","First wave: {} sec",delays[mFirstWave]);
            MarkDirty();return;
        }
        if(id==29){mSpeedTier=(mSpeedTier+1)%3;
            const char* tiers[]={ModText::Tr("无","none"),"x1.03","x1.05"};
            mSpeedButton->mLabel=ModText::Format(mEndlessPage?"移速成长（每五关）：{}":"移速成长（每场景）：{}","Speed growth: {}",tiers[mSpeedTier]);return;}
        if(id>=40&&id<=42){mDifficulty=id-40;RefreshOptionLabels();RefreshModePage();return;}
        if (id >= 20 && id <= 25)
        {
            if (id == 20) mDifficulty = (mDifficulty + 1) % 3;
            if (id == 21) mScale = (mScale + 1) % 4;
            if (id == 22) mTempo = (mTempo + 1) % 3;
            if (id == 23) mZombotany ^= 1;
            if (id == 24) {if(mEndlessPage)mEndlessOptions=(mEndlessOptions&~3)|(((mEndlessOptions&3)+1)%3);else mBossFlag=(mBossFlag+1)%3;}
            if (id == 25) mEndlessScene = (mEndlessScene + 1) % 5;
            RefreshOptionLabels();
            RefreshModePage();
            return;
        }
        if (id == 27)
        {
            mZombieBuff = (mZombieBuff + 1) % 3;
            mZombieBuffButton->mLabel = ModText::Format("僵尸强化：{}", "Zombie boosts: {}", mZombieBuff ? std::to_string(mZombieBuff) : ModText::Tr("关", "off"));
            MarkDirty(); return;
        }
        if (id == 26)
        {
            mLadder = (mLadder + 1) % 5;
            const char* ladders[] = { "1.33", "1", "1.2", "1.5", "2" };
            mLadderButton->mLabel = ModText::Format("场景阶梯：x{}", "Scene ladder: x{}", ladders[mLadder]);
            MarkDirty();
            return;
        }
        if (id == 14)
        {
            mBuffMult = (mBuffMult + 1) % 3;
            mBuffMultButton->mLabel = ModText::Format("增益倍选：x{}", "Buff picks: x{}", RunState::BuffMultValueFor(mBuffMult));
            MarkDirty();
            return;
        }
        if (id == 13) mApp->ResumeRun(mEndlessPage?RunState::RUN_MODE_ENDLESS:-1,mEndlessScene);
        else if(id==15 && !mEndlessPage){mApp->ShowRunMenu(mEndlessScene);return;}
        else if ((id >= 10 && id <= 12) || id == 15)
            mApp->StartRun(id == 15 ? RunState::RUN_MODE_ENDLESS : id - 10, mBuffMult,
                mDifficulty, mScale, mTempo, mZombotany, mEndlessScene, mBossFlag, mLadder, mZombieBuff,mSpeedTier, mFirstWave,mEndlessPage?mEndlessOptions:0);
        return;
    }
    if(mSubmitted || !mPickRun || mApp->mRunState.get()!=mPickRun || mPickRun->mPickCounter!=mExpectedCounter)return;
    RunState& run = *mPickRun;
    if(id>=150 && id<153) {
        const int index=id-150;
        if(!run.CanLockChoice(index))return;
        mLockedChoice=mLockedChoice==index?-1:index;RefreshRerollLabels();MarkDirty();return;
    }
    if (id == 4) {
        mSubmitted=true;
        if(!mApp->RunPickPaidReroll(mLockedChoice,mExpectedCounter)){mSubmitted=false;return;}
    }
    else if (id == 5) run.SkipPendingPick();
    else if (id >= 0 && id < RunState::RUN_CHOICES)
    {
        if (run.IsPlantPick()) run.TakePlantChoice(id);
        else {
            int choice=run.mBuffChoices[id], before=run.GetEffectiveBuffCount(choice);
            run.TakeBuffChoice(id);
            mApp->RefreshRunChoicePlants(choice,before);
        }
    }
    mApp->ShowRunPicks();
}
