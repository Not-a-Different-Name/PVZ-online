#include "../CoopPlay.h"
#include "RunPickDialog.h"
#include "GameButton.h"
#include "CjkStoneButton.h"
#include "RunChoiceLayout.h"
#include "OnlineUiDraw.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../Run/RunState.h"
#include "../Run/RunBuffs.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "graphics/ImageFont.h"
#include "graphics/Graphics.h"
#include "../ModText.h"
#include "../System/PlayerInfo.h"
#include <cstdio>
#include <map>
#include <array>
#include <memory>
#include <tuple>
#include "graphics/MemoryImage.h"

// ── 中文描述这一档 ──────────────────────────────────────────────────────
// 引擎自带的字体全是位图字体，没有中文字形，所以这一屏的中文说明走 ModText 的宽字符
// 直绘（UTF-8 → UTF-16 → TextOutW，与系统码页脱钩；2026-10-03 语言批起）。
// 原先这里的转码（RunPickAnsiFromUtf8）与字体缓存（RunPickCjkFont、13pt）已并入
// Lawn/ModText；断行系统一并宽字符化——不再有 CP936 双字节的字节口径，一个 wchar_t
// 就是一个单元。
// 2026-10-14：断行/整块绘制五件套（含断单元与"不能起行"两表）整体搬去 ModText
// （WrapLines / WrapLineCount / DrawWrappedCjk）——僵尸强化投票框成了第二个消费方，
// 测量与绘制共用的"唯一实现"纪律升格为全局。

static ModText::Font* RunPickCjkFont() { return ModText::GetFont(CjkPointSize(20), false); }

class RerollClickButton:public CjkStoneButton
{
public:
    RerollClickButton(int id,ButtonListener* listener):CjkStoneButton(id,listener){}
    void MouseUp(int x,int y,int button,int clicks) override
    {
        if(clicks>1){Widget::MouseUp(x,y,button,clicks);return;}
        CjkStoneButton::MouseUp(x,y,button,clicks);
    }
};

// 一条描述按显式 \n 拆行后的最大行宽——列宽就按它定，不让任何一条被硬折
static int RunPickDescWidth(ModText::Font* theFont, const char* theUtf8)
{
	std::wstring aText = ModText::WideFromUtf8(theUtf8);
	int aMax = 0;
	int aStart = 0;
	while (true)
	{
		int aBreak = (int)aText.find(L'\n', aStart);
		if (aBreak < 0) aBreak = (int)aText.size();
		int aWidth = ModText::TextWidth(theFont, aText.substr(aStart, aBreak - aStart));
		if (aWidth > aMax) aMax = aWidth;
		if (aBreak >= (int)aText.size()) break;
		aStart = aBreak + 1;
	}
	return aMax;
}


namespace {
struct LineExtent {int next=0,maximum=1;};
struct CatalogLayout {int width=-1;std::map<int,LineExtent> lines;};
CatalogLayout& Catalog(ModText::Font* font){
 static std::map<std::pair<ModText::Font*,int>,CatalogLayout> layouts;
 return layouts[{font,ModText::GetLanguage()}];
}
int CatalogWidth(ModText::Font* font){
 auto& layout=Catalog(font);if(layout.width>=0)return layout.width;
 int result=0;for(int id=0;id<RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT+RUN_PLANT_BUFF2_COUNT;++id){
  if(id>=RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT&&!RunPlantBuff2InPool(id-RUN_BUFF_COUNT-RUN_PLANT_UPGRADE_COUNT))continue;
  result=std::max(result,RunPickDescWidth(font,GetRunChoiceDesc(id)));
  // The caller adds12 then caps at220; wider catalogue entries cannot matter.
  if(result>=208){result=208;break;}
 }return layout.width=result;
}
int CatalogLines(ModText::Font* font,int width,int fittingLines){
 auto& extent=Catalog(font).lines[width];
 // Once one entry exceeds this height, the constructor must grow. Resume later
 // only if a subsequent dialog can fit the lower bound already measured.
 while(extent.next<RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT+RUN_PLANT_BUFF2_COUNT&&extent.maximum<=fittingLines){
  const int id=extent.next++;
  if(id>=RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT&&!RunPlantBuff2InPool(id-RUN_BUFF_COUNT-RUN_PLANT_UPGRADE_COUNT))continue;
  extent.maximum=std::max(extent.maximum,ModText::WrapLineCount(font,GetRunChoiceDesc(id),width));
 }return extent.maximum;
}
using CardKey=std::tuple<int,int,int,int,int,unsigned long,ModText::Font*>;
struct CardImage {CardKey key{};std::unique_ptr<Sexy::MemoryImage> image;};
// No object/save layout change. Each dialog owns at most three images; erase on
// destruction so repeated picks cannot retain cards from earlier dialogs.
std::map<const RunPickDialog*,std::array<CardImage,3>> cards;
}

RunPickDialog::RunPickDialog(LawnApp* theApp, RunState* theRun) : LawnDialog(
	theApp,
	Dialogs::DIALOG_RUN_PICK,
	true,
	theRun->IsPlantPick() ? ModText::Tr("选择植物", "Choose a plant") : ModText::Tr("选择增益", "Choose a buff"),
	_S(""),
	_S(""),
	Dialog::BUTTONS_NONE)
{
	mRun = theRun;
	mExpectedCounter = theRun->mPickCounter;
	mPlantPick = theRun->IsPlantPick();
	mAreaTop = 0;
	mAreaHeight = 0;
	mColumnWidth = 0;
	for (int i = 0; i < 3; i++)
	{
		mColumnX[i] = 0;
		// 按钮上就是这株植物 / 这条增益的名字——名字和卡面对得上，玩家才知道自己点的是哪张
		SexyString aLabel;
		if (mPlantPick)
		{
			// @pvz-online: 空缺格（玩家闪退修复 2026-10-11）：植物候选抽干时 RollChoices
			// 按屏格数缺格填 SEED_NONE——空位不取名。名字走 Plant::GetNameString，
			// SEED_NONE（-1）会读穿植物定义表（GetPlantDefinition 裸下标，Release 断言
			// 为空）→ 野指针崩。2026-10-10 玩家「深夜泳池第三关」闪退的根因。
			if (theRun->mPlantChoices[i] >= 0 && theRun->mPlantChoices[i] < RunState::RUN_POOL_MAX)
			{
				aLabel = Plant::GetNameString(theRun->mPlantChoices[i]);
			}
		}
		else if (theRun->mBuffChoices[i] != RunState::RUN_BUFF_CHOICE_NONE)
		{
			aLabel = SexyString(GetRunChoiceName(theRun->mBuffChoices[i]));
		}
		mChoiceButtons[i] = MakeButton(RunPickDialog_Choice0 + i, this, aLabel);
		// 空缺格（防御，方案 §2.4）：空着不画、不可点——置灰兜底；逻辑层 TakePlantChoice /
		// TakeBuffChoice 还会再忽略一次哨兵，双保险。植物屏与增益屏同口径（闪退修复
		// 2026-10-11 前只有增益屏这一道）。
		bool aEmptySlot = mPlantPick
			? (theRun->mPlantChoices[i] < 0 || theRun->mPlantChoices[i] >= RunState::RUN_POOL_MAX)
			: (theRun->mBuffChoices[i] == RunState::RUN_BUFF_CHOICE_NONE);
		if (aEmptySlot)
		{
			mChoiceButtons[i]->mDisabled = true;
		}
		// 3★ 金色字（权重批 2026-10-09，docs/06 §8.7）：增益屏非空格按稀有度档位染色。
		// 未定档（0）恒为白，零行为。植物屏不参与染色——mBuffChoices 还是上一批增益屏
		// 的残留值，拿来查会把植物卡错染成金。
		else if (!mPlantPick && GetRunChoiceRarity(theRun->mBuffChoices[i]) == 3)
		{
			mChoiceButtons[i]->mGoldLabel = true;
		}
	}
	// 「放弃」（Skip，2026-10-03 用户定案）：三条都不想要时的出路。按钮文案走位图字体
	// （② 汉字字形批后中文模式可直接渲染），中英文经 Tr 择串；和「重摇」并排（见 Resize）。
	mSkipButton = MakeButton(RunPickDialog_Skip, this, ModText::Tr("跳过本次", "Skip this pick"));
	// 「重摇」（花园批 2026-10-13 加付费态；2026-10-14 用户令「第一次也要花金币」取消免费
	// 「换一批」——单按钮不再两态，恒为付费重摇）：N = RunState::GetPaidRerollCost 的当前价
	// （底价 10 币，同一屏里每摇一次 ×10：显示 $100/$1,000/$10,000…，进新一屏回底价——
	// 2026-10-14 二次定案：玩家看到的 100/1000/10000 就是显示数，币面按 ×10 口径取）。
	// 金额按显示口径走 GetMoneyString（币 ×10 的 $ 串）——和本屏左下角钱罐、局内钱罐、商店
	// 是同一个串数，玩家在任何地方看到的都是同一个数。钱不够当前价（比原始币）时置灰。
	// 置灰走框架的 Widget::mDisabled，点在派发层就被拦下（和本作其它置灰石钮同口径：
	// 不变暗、只是按不动）；重开这一屏时按当时的记账重算（价跟着阶梯计数走，见 RunState::PaidReroll）。
	mRefreshButton = new RerollClickButton(RunPickDialog_Refresh,this);
    for(int index=0;index<3;++index)mLockButtons[index]=MakeButton(150+index,this,"");
    RefreshRerollLabels();

    if(theRun->mAdmission.phase==Admission::PICKS)
        mFastButton=MakeButton(RunPickDialog_Fast,this,ModText::Tr("快速完成","Quick catch-up"));
	mTallBottom = true;
	mVerticalCenterText = false;
	// 尺寸由内容反推，且两个驱动按屏分家（2026-10-04 用户反馈"NEW PLANT 屏过大"）：
	//  植物屏 列宽只按按钮上的植物名（标签，位图字体）与种子包定，面积只要放下包；
	//  增益屏 列宽按说明的显式行宽，面积按"最多的那条说明在当前列宽下折几行"——
	//         RunPickCountLines 走的就是绘制端同一份断行，不再按显式 \n 行数估。
	// 之前两屏共用一套驱动：植物屏根本不放那些说明，却被它们的折行数顶着长到
	// CalcSize 顶档——弹窗比画布 800 还宽，两侧石头边框被画布切掉，包和按钮之间
	// 空着百来像素的死区。以后改文案不用回来调数字。
	ModText::Font* aCjkFont = RunPickCjkFont();
	int aTargetColumn = 0;
	if (mPlantPick)
	{
		aTargetColumn = SEED_PACKET_WIDTH + 16;
		for (int i = 0; i < 3; i++)
		{
			// 标签用绘制端同款字体量（GameButton.cpp 的 DrawStoneButton）——宽就是列宽
			int aLabelWidth = Sexy::FONT_DWARVENTODCRAFT18GREENINSET->StringWidth(mChoiceButtons[i]->mLabel) + 16;
			if (aLabelWidth > aTargetColumn) aTargetColumn = aLabelWidth;
		}
	}
	else
	{
		// 全池静态测量（增益屏列宽按全池最长说明）：三段 id 都测，第二表里未进池的
		// 条目跳过（不会上屏，不参与定宽）。
		aTargetColumn = CatalogWidth(aCjkFont);
		aTargetColumn += 12; // 两侧各留一点白
		// 列宽目标封顶：英文说明是长句（"Sky sun falls 20% faster, up to 4 stacks"），
		// 按显式行宽推目标会一路把弹窗顶着长到整屏宽——封顶后交给折行处理
		// （2026-10-04 用户反馈：英文档这个框大到快出屏）。
		if (aTargetColumn > 220) aTargetColumn = 220;
	}
	int aFitLineHeight = ModText::LineHeight(aCjkFont) + 3;
	// 底部是两行按钮（三张卡一行、最底「放弃」单独一行）：最小高与上限高都比原来多让
	// 出一行（按钮高 + 6，与 Resize 里三张卡那一行上移的量是同一笔账）。卡片区口径不变。
	int aExtraHeight = IMAGE_BUTTON_LEFT->mHeight + 40;
	// 起手宽度：植物屏三列只为放包与名字，不必按增益屏的 430 起步（360 起，两屏的
	// 标题与边框内缩量会把总宽再抬到比画布窄一截的位置）。
	int aSizeX = mPlantPick ? 360 : 430;
	int aSizeY = 150 + aExtraHeight;
	CalcSize(aSizeX, aSizeY);
	// 宽的上限从 740 收到 580（2026-10-04）：说明改按词折行后，不再需要为一句长文
	// 把弹窗拉满整屏——到 580 就收手，多出来的行交给折行。
	// 宽、高各自达标即各自停长：两个条件绑在同一个 i 上时，高不达标会把宽也一级一级
	// 抬到顶档（植物屏被增益说明的折行数拖成整屏宽，正是这条路）。
	for (int i = 1; i <= 6; i++)
	{
		int aNeededArea;
		if (mPlantPick)
		{
			// 面积里只有种子包（名字在按钮上）——包高 + 上下留白
			aNeededArea = SEED_PACKET_HEIGHT + 12;
		}
		else
		{
			int aMaxLines = CatalogLines(aCjkFont,mColumnWidth-4,(mAreaHeight-6)/aFitLineHeight-2);
			aNeededArea = (aMaxLines + 2) * aFitLineHeight + 6;
		}
		bool aWidthDone = mColumnWidth >= aTargetColumn || mWidth >= 580;
		// 高上限 460→500（2026-10-14 用户报「已有行叠字」）：中文说明比英文长（向日葵那条
		// 折 6 行），460 封顶时卡片区差一行放不下，末行「层」压到「已有」行上。
		bool aHeightDone = mAreaHeight >= aNeededArea || mHeight >= 500 + aExtraHeight;
		if (aWidthDone && aHeightDone) break;
		if (!aWidthDone) aSizeX += 20;
		if (!aHeightDone) aSizeY += 20;
		CalcSize(aSizeX, aSizeY);
	}
	mApp->CenterDialog(this, mWidth, mHeight);
	mClip = false;
}

RunPickDialog::~RunPickDialog()
{
    cards.erase(this);
	for(auto* button:mLockButtons)delete button;
	for (int i = 0; i < 3; i++) delete mChoiceButtons[i];
	delete mSkipButton;
	delete mRefreshButton;
    delete mFastButton;
}

void RunPickDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);

	int aButtonHeight = IMAGE_BUTTON_LEFT->mHeight;
    if(mFastButton)mFastButton->Resize(mWidth-220,16,190,40);
	int aLeft = mContentInsets.mLeft + mBackgroundInsets.mLeft;
	int aContentWidth = mWidth - mContentInsets.mLeft - mContentInsets.mRight - mBackgroundInsets.mLeft - mBackgroundInsets.mRight;
	int aGap = 16;
	mColumnWidth = (aContentWidth - aGap * 2) / 3;
	for (int i = 0; i < 3; i++)
	{
		mColumnX[i] = aLeft + i * (mColumnWidth + aGap);
	}

	// 卡片区 = 说明那行以下、三张卡那一行以上。跟着同一个公式算，标题换行也不错位。
	int aAreaTop = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	if (mDialogHeader.size() > 0) aAreaTop += mHeaderFont->GetHeight() + mSpaceAfterHeader;
	aAreaTop += mLinesFont->GetHeight() + 10;
	mAreaTop = aAreaTop + 34;

	// 最底下那一行放两个按钮（2026-10-08 起：「重摇」在左、「放弃」在右，整行居中）：
	// 三张卡那一行整体上移一行（按钮高 + 6），卡片区的上沿不动、下沿从卡片行反推——
	// 和构造函数里多让的 aExtraHeight 是同一笔账（行数没变，高度账原样）。
	int aSkipY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - aButtonHeight + 2;
	if (mTallBottom) aSkipY += 5;
	int aButtonY = aSkipY - aButtonHeight - 6;
	mAreaHeight = aButtonY - mAreaTop - 6;
	if (mAreaHeight < 0) mAreaHeight = 0;

	for (int i = 0; i < 3; i++)
	{
		mChoiceButtons[i]->Resize(mColumnX[i], aButtonY, mColumnWidth, aButtonHeight);
		if(mLockButtons[i])mLockButtons[i]->Resize(mColumnX[i],aAreaTop,mColumnWidth,30);
	}
	int aFootCenter = (mColumnX[0] + mColumnX[2] + mColumnWidth) / 2;
	int aFootWidth = std::min(240, (mWidth - 64) / 2);
	mRefreshButton->Resize(aFootCenter - 8 - aFootWidth, aSkipY, aFootWidth, aButtonHeight);
	mSkipButton->Resize(aFootCenter + 8, aSkipY, aFootWidth, aButtonHeight);
}

void RunPickDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	for (int i = 0; i < 3; i++) AddWidget(mChoiceButtons[i]);
	AddWidget(mSkipButton);
	AddWidget(mRefreshButton);
    for(auto* button:mLockButtons)AddWidget(button);
    if(mFastButton)AddWidget(mFastButton);
}

void RunPickDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	for (int i = 0; i < 3; i++) RemoveWidget(mChoiceButtons[i]);
	RemoveWidget(mSkipButton);
	RemoveWidget(mRefreshButton);
    for(auto* button:mLockButtons)RemoveWidget(button);
    if(mFastButton)RemoveWidget(mFastButton);
}

void RunPickDialog::Draw(Graphics* g)
{
    // A queued draw must never read an old or replaced run transaction.
    if (!mRun || mApp->GetRunState() != mRun || mRun->mPickCounter != mExpectedCounter) return;
	LawnDialog::Draw(g);
    if(mApp->mCoopPlay)mApp->mCoopPlay->DrawEntry(g,20,16,140,40);

	// 标题下面那行说明。这一屏的三张卡是画上去的（种子包 / 效果说明），不是文本，
	// 所以 Dialog 自己的正文留空，排版在这里现算——和 LawnDialog::Draw 用同一套边距。
	int aFontY = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
	if (mDialogHeader.size() > 0)
	{
		int aOffsetY = aFontY - mHeaderFont->GetAscentPadding() + mHeaderFont->GetAscent();
		aFontY = aOffsetY - mHeaderFont->GetAscent() + mHeaderFont->GetHeight() + mSpaceAfterHeader;
	}
	g->SetFont(mLinesFont);
	g->SetColor(mColors[Dialog::COLOR_LINES]);
	// 2026-10-14：这行是位图字体画的——「局」不在汉化像素字库里（1419 字，逐字查过），
	// 画出来是个空档（用户报「格子」）。改用字库内的「本次闯关」。
    if(mRun->mAdmission.phase==Admission::PICKS)
        SeatUi::Text(g,ModText::Tr("请选择增益，还剩 ", "Buff picks left: ")+std::to_string(mRun->mPendingBuffPicks),
            24,aFontY+18,mWidth-48,18,Color(245,230,165),true);
    else WriteCenteredLine(g, aFontY, mPlantPick ? ModText::Tr("点击植物，加入本局卡池。", "Choose a plant to add to this run.") : ModText::Tr("点击一项增益；不想选可跳过。", "Choose a buff, or skip this pick."));

	// 金币余额（花园批 2026-10-13 加；2026-10-14 用户令改版「获得金币显示在下面那个栏位，
	// 三选一屏也要看得到金币数」）：画成局内左下角同款钱罐——位置、贴图、字体、颜色、数字
	// 口径（GetMoneyString，币 ×10 的 $ 串）全与 Board::DrawUICoinBank 一字不差，同一钱包在
	// 任何屏、任何位都是同一个数（旧口径这屏直显原始币，比钱罐/商店小 10 倍，作废）。
	// 钱罐锚在屏幕坐标：本对话框 mClip=false（ctor 尾），Draw 收到的 g 以对话框左上角为
	// 原点，减去 mX/mY 即回到屏幕位（局内钱罐位 = 57, 599-H）。
	if (mApp->mPlayerInfo != nullptr)
	{
		SeatUi::Strip(g, Rect(28 - mX, 568 - mY, 744, 32));
		int aBankX = 57 - mX;
		int aBankY = 599 - Sexy::IMAGE_COINBANK->GetHeight() - mY;
		g->DrawImage(Sexy::IMAGE_COINBANK, aBankX, aBankY);
		g->SetFont(Sexy::FONT_CONTINUUMBOLD14);
		g->SetColor(Sexy::Color(180, 255, 90));
		SexyString aCoinLabel = mApp->GetMoneyString(mApp->mPlayerInfo->mCoins);
		g->DrawString(aCoinLabel, aBankX + 116 - Sexy::FONT_CONTINUUMBOLD14->StringWidth(aCoinLabel), aBankY + 24);
		std::string aCost = mRun->mRerollVouchers > 0
			? ModText::Tr("重摇：使用1张券", "Reroll: use 1 voucher")
			: ModText::Tr("本次重摇：", "Reroll cost: ") + LawnApp::GetMoneyString(mRun->GetPaidRerollCost());
        if(mLockedChoice>=0)aCost+=ModText::Tr(" + 锁定 "," + lock ")+LawnApp::GetMoneyString(100);
		if (mRefreshButton->mDisabled) aCost += ModText::Tr(" · 金币不足", " / Not enough coins");
		SeatUi::Text(g, aCost, 208 - mX, 591 - mY, 548, 18, Color(245, 230, 165));
	}

	for (int i = 0; i < 3; i++)
	{
		if (mPlantPick)
		{
			// 空缺格（玩家闪退修复 2026-10-11）：植物候选抽干时缺格填 SEED_NONE，空着不画——
			// DrawSeedPacket 对 SEED_NONE（-1）会走种子包路径的查表/升级判断（Release 断言为空）。
			// 与增益屏下面那道哨兵护栏同口径。
			if (mRun->mPlantChoices[i] < 0 || mRun->mPlantChoices[i] >= RunState::RUN_POOL_MAX) continue;
			DrawSeedPacket(g, (float)(mColumnX[i] + (mColumnWidth - SEED_PACKET_WIDTH) / 2), (float)mAreaTop,
				mRun->mPlantChoices[i], SeedType::SEED_NONE, 0.0f, 255, false, false);
		}
		else
		{
			unsigned short aBuffId = mRun->mBuffChoices[i];
			// 空缺格（防御，方案 §2.4）：空着不画——这一列什么都不出现
			if (aBuffId == RunState::RUN_BUFF_CHOICE_NONE) continue;

            auto& cached=cards[this][i];
            const CardKey key{aBuffId,mRun->GetBuffCount(aBuffId),ModText::GetLanguage(),mColumnWidth,mAreaHeight,
                mColors[Dialog::COLOR_LINES].ToInt(),RunPickCjkFont()};
            if(!cached.image||cached.key!=key){
                cached.key=key;cached.image=std::make_unique<Sexy::MemoryImage>(mApp);
                cached.image->Create(mColumnWidth,mAreaHeight);
                Sexy::Graphics tile(cached.image.get());tile.Translate(-mColumnX[i],-mAreaTop);
                auto render=[&](Sexy::Graphics* g){
			// 效果说明贴着各自的按钮画：三列各说各的，不用让人去猜哪句话配哪个名字。
			// 中文走 ModText 宽字符直绘，按列宽断行、逐行居中。
			ModText::Font* aFont = RunPickCjkFont();
			int aLineHeight = ModText::LineHeight(aFont) + 3;
			// 列顶一行：单株升级的【植物名】（2026-10-03 玩家反馈——按钮名字是英文位图
			// 字体、说明文案多数也不含植物名，不标出来分不清这条 buff 是哪株的）+ 稀有度
			// 星标（2026-10-09 晚实机反馈：全档可见，3★ 金色、1★/2★ 灰色，同图鉴
			// Almanac::DrawRarityStars 口径；旧口径「三选一屏只标 3★」作废）。全局条
			// 没有植物名，就只画星。
			const char* aPlantName = GetRunChoicePlantName(aBuffId);
			std::wstring aHead;
			if (aPlantName != NULL)
			{
				// 括号也随语言：【植物名】 / [Plant Name]
				std::string aHeadUtf8 = ModText::IsChinese()
					? std::string("【") + aPlantName + "】"
					: std::string("[") + aPlantName + "]";
				aHead = ModText::WideFromUtf8(aHeadUtf8.c_str());
			}
			int aRarity = GetRunChoiceRarity(aBuffId);
			std::wstring aStars = (aRarity > 0)
				? std::wstring((size_t)aRarity, (wchar_t)0x2605) : std::wstring();
			int aHeadBudget = mColumnWidth - ModText::TextWidth(aFont, aStars) - (aStars.empty() ? 0 : 6);
			if (ModText::TextWidth(aFont, aHead) > aHeadBudget)
			{
				while (!aHead.empty() && ModText::TextWidth(aFont, aHead + L"...") > aHeadBudget)
					aHead.pop_back();
				aHead += L"...";
			}
			// 说明区：顶边整行让给名字/星标行、底边整行让给「已有 x/N」。列高在构造时
			// 按真实折行数算过（预算的 +2 行就是这两行），正常放得下；真到尺寸上限也
			// 只会往下压「已有」一行，2026-10-03 那种标题/说明/已有三头叠字不会再出现。
			Rect aRect(mColumnX[i] + 2, mAreaTop + aLineHeight, mColumnWidth - 4,
				mAreaHeight - 2 * aLineHeight - 4);
			{
				int aHeadW = aHead.empty() ? 0 : ModText::TextWidth(aFont, aHead);
				int aStarsW = aStars.empty() ? 0 : ModText::TextWidth(aFont, aStars);
				int aTopX = mColumnX[i] + (mColumnWidth - aHeadW - aStarsW
					- ((aHeadW > 0 && aStarsW > 0) ? 6 : 0)) / 2;
				if (aTopX < mColumnX[i]) aTopX = mColumnX[i];
				if (aHeadW > 0)
				{
					ModText::DrawTextWide(g, aFont, aTopX, mAreaTop, aHead,
						mColors[Dialog::COLOR_LINES], g->mClipRect);
					aTopX += aHeadW + 6;
				}
				if (aStarsW > 0)
				{
					ModText::DrawTextWide(g, aFont, aTopX, mAreaTop, aStars,
						aRarity >= 3 ? Sexy::Color(255, 200, 60) : Sexy::Color(150, 140, 130), g->mClipRect);
				}
			}
			ModText::DrawWrappedCjk(g, RunChoiceLayout::DescriptionFont(GetRunChoiceDesc(aBuffId), aRect.mWidth, aRect.mHeight),
				aRect, GetRunChoiceDesc(aBuffId), mColors[Dialog::COLOR_LINES]);

			// 「已有 x/N」：封顶条目带 /N；无限条目只报已有层数。画在卡片区底边、逐列居中。
			int aOwned = mRun->GetBuffCount(aBuffId);
			int aCap = GetRunChoiceMaxStacks(aBuffId);
			char aCountUtf8[64];
			if (aCap > 0) snprintf(aCountUtf8, sizeof(aCountUtf8), ModText::Tr("层数 %d/%d", "Stacks %d/%d"), aOwned, aCap);
			else snprintf(aCountUtf8, sizeof(aCountUtf8), ModText::Tr("层数 %d", "Stacks %d"), aOwned);
			std::wstring aCountText = ModText::WideFromUtf8(aCountUtf8);
			int aCountY = mAreaTop + mAreaHeight - aLineHeight;
			if (aCountY < mAreaTop) aCountY = mAreaTop;
			int aCountX = mColumnX[i] + (mColumnWidth - ModText::TextWidth(aFont, aCountText)) / 2;
			ModText::DrawTextWide(g, aFont, aCountX, aCountY, aCountText,
				mColors[Dialog::COLOR_LINES], g->mClipRect);
                };render(&tile);
            }
            g->DrawImage(cached.image.get(),mColumnX[i],mAreaTop);
		}
	}
}

void RunPickDialog::ButtonDepress(int theId)
{
    if(!mRun || mSubmitted || mApp->GetRunState()!=mRun || mRun->mPickCounter!=mExpectedCounter)return;
    if(theId>=150 && theId<153) {
        const int index=theId-150;
        if(!mRun->CanLockChoice(index))return;
        mLockedChoice=mLockedChoice==index?-1:index;
        RefreshRerollLabels();MarkDirty();return;
    }
    if(theId==RunPickDialog_Fast){mApp->StartAdmissionFastPicks();return;}
	if (theId >= RunPickDialog_Choice0 && theId <= RunPickDialog_Choice2)
	{
		mApp->RunPickChosen(theId - RunPickDialog_Choice0);
	}
	else if (theId == RunPickDialog_Skip)
	{
		mApp->RunPickSkipped();
	}
	else if (theId == RunPickDialog_Refresh)
	{
		// 恒付费（2026-10-14 用户令：第一次也花金币，免费「换一批」取消）：扣币、关屏、重抽
		// 全在 LawnApp::RunPickPaidReroll——这里不设任何免费分流。
        mSubmitted=true;
        if(!mApp->RunPickPaidReroll(mLockedChoice,mExpectedCounter))mSubmitted=false;
	}
}

void RunPickDialog::RefreshRerollLabels()
{
    if (!mRun || mApp->GetRunState() != mRun || mRun->mPickCounter != mExpectedCounter) return;
    const int cost=mRun->GetRerollPurchaseCost(mLockedChoice);
    mRefreshButton->mLabel=cost>0 ? ModText::Tr("重摇 -","Reroll -")+LawnApp::GetMoneyString(cost)
        : ModText::Tr("使用重摇券","Use voucher");
    mRefreshButton->SetDisabled(!mApp->mPlayerInfo || mApp->mPlayerInfo->mCoins<cost);
    for(int index=0;index<3;++index) {
        mLockButtons[index]->mLabel=mLockedChoice==index?ModText::Tr("取消锁定","Unlock"):ModText::Tr("锁定","Lock");
        mLockButtons[index]->SetDisabled(!mRun->CanLockChoice(index));
    }
}

// 键盘一律不认。按空格/回车会走 LawnDialog::KeyDown 那条"当成点了 Yes"的老路，
// 而这一屏没有 Yes——屏被关掉的话待选的卡还在，下一帧它照样会弹回来，
// 玩家只会觉得"按了没反应"。要么点一张卡、要么点「放弃」（放弃要的是玩家明确的
// 一次点击，不给键盘顺手带过的机会），键盘在这儿什么也别做。
void RunPickDialog::KeyDown(KeyCode theKey)
{
	if(theKey=='B')mApp->ShowCoopPlay();
}

void RunPickDialog::MouseDown(int x,int y,int clicks)
{
    if(clicks>0 && Rect(20,16,140,40).Contains(x,y) && mApp->IsOnlineGame()){mApp->ShowCoopPlay();return;}
    LawnDialog::MouseDown(x,y,clicks);
}
