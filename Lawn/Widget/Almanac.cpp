#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "GameButton.h"
#include "../SeedPacket.h"
#include "../../LawnApp.h"
#include "Almanac.h"
#include "../../Resources.h"
#include "../System/Music.h"
#include "../../GameConstants.h"
#include "../System/PlayerInfo.h"
#include "../System/PoolEffect.h"
#include "../System/ReanimationLawn.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include "widget/WidgetManager.h"
#include "../Run/RunBuffs.h"
#include "../ModText.h"
#include <string>
#include <vector>

bool gZombieDefeated[NUM_ZOMBIE_TYPES] = { false };

// ── 词条附录的中文绘制（图鉴全解锁批，2026-10-03；语言批起走 ModText）────────
// 词条文案（RunBuffs 表）是 UTF-8 中文，main.pak 那套位图字体没有汉字字形——自
// 2026-10-03 语言批起走 ModText 的宽字符直绘（UTF-8 → UTF-16 → TextOutW，与系统码页
// 脱钩）；原先这里的 Utf8ToAnsi 与本地字体（SysFont + TextOutA、雅黑/黑体/宋体回退、
// 进程级缓存）已并入 Lawn/ModText。
// 字号仍按"目标像素高"反算点值（同 OnlineStartDialog）：150% 缩放下写死的点值会被
// 放大近 2 倍，258px 宽的卡面放不下。取 12px：13px 时块高 68px，最长简介（机枪豌豆）
// 底下只剩 41px 塞不下，统一降一档配合「连接式单行」结构（见 DrawAlmanacRunEntry）。
// 断行工具已宽字符化（不再有 CP936 双字节的字节口径，一个 wchar_t = 一个单元）。
#define ALMANAC_CJK_PX 12

static int AlmanacCjkPointSize()
{
	HDC aDC = ::GetDC(gSexyAppBase->mHWnd);
	int aDpi = GetDeviceCaps(aDC, LOGPIXELSY);
	::ReleaseDC(gSexyAppBase->mHWnd, aDC);
	if (aDpi <= 0) aDpi = 96;
	int aPointSize = (ALMANAC_CJK_PX * 72 + aDpi / 2) / aDpi;
	return aPointSize < 1 ? 1 : aPointSize;
}

// 宽字符版折行单元：一个 wchar_t = 一个单元；ASCII 连续段仍算一个整体——
// 「×0.75」「20%」不在中间断开（同 RunPickDialog 的语义）
static int AlmanacBreakUnit(const std::wstring& theText, int theIndex)
{
	if (theText[theIndex] >= 0x80) return 1;
	int anEnd = theIndex;
	while (anEnd < (int)theText.size() && theText[anEnd] < 0x80) anEnd++;
	return anEnd - theIndex;
}

// 显式 \n 分段 + 按列宽折行（表里说法都短，折行是保险，防以后加长文案溢出）
static void AlmanacWrapEntry(ModText::Font* theFont, const std::wstring& theText, int theWidth, std::vector<std::wstring>& theLines)
{
	if (theWidth <= 0) theWidth = 1;
	int aStart = 0;
	while (true)
	{
		int aBreak = (int)theText.find(L'\n', aStart);
		if (aBreak < 0) aBreak = (int)theText.size();
		std::wstring aPara = theText.substr(aStart, aBreak - aStart);
		int aPos = 0;
		while (aPos < (int)aPara.size())
		{
			int aFit = aPos;
			int aCursor = aPos;
			while (aCursor < (int)aPara.size())
			{
				int aNext = aCursor + AlmanacBreakUnit(aPara, aCursor);
				if (aFit > aPos && ModText::TextWidth(theFont, aPara.substr(aPos, aNext - aPos)) > theWidth) break;
				aFit = aNext;
				aCursor = aNext;
			}
			if (aFit == aPos) aFit = aPos + AlmanacBreakUnit(aPara, aPos);	// 单单元超宽：硬放，防死循环
			theLines.push_back(aPara.substr(aPos, aFit - aPos));
			aPos = aFit;
		}
		if (aBreak >= (int)theText.size()) break;
		aStart = aBreak + 1;
	}
}

// 词条块的排版常量：左缘/宽度跟着介绍正文栏走（与 TodDrawStringWrapped 的矩形同源），
// 底缘 519 是死线——费用/充能两行从 y=520 起画，词条块不压上去。
#define ALMANAC_ENTRY_X 485
#define ALMANAC_ENTRY_W 258
#define ALMANAC_ENTRY_BOTTOM 519

// 条名与稀有度星标之间的间距（档 3 内联式量宽与绘制共用同一个数）
#define ALMANAC_STAR_GAP 6

// 稀有度星标（2026-10-09 用户定案：图鉴只补星标、布局不动）：条名右缘画 mRarity 颗 ★，
// 3★ 金色、1★/2★ 灰色（三选一屏只标 3★，图鉴是静态参考，低档也画出来）。放不下不画。
// 返回画完后的右缘 x（没画 = 原样返回，档 3 的冒号/正文起点用它保持连贯）。
static int DrawRarityStars(Graphics* g, ModText::Font* theFont, int theRarity, int theNameEndX, int theY)
{
	if (theRarity <= 0) return theNameEndX;
	std::wstring aStars((size_t)theRarity, (wchar_t)0x2605);
	int aWidth = ModText::TextWidth(theFont, aStars);
	int anEnd = theNameEndX + ALMANAC_STAR_GAP + aWidth;
	if (anEnd > ALMANAC_ENTRY_X + ALMANAC_ENTRY_W) return theNameEndX;
	ModText::DrawTextWide(g, theFont, theNameEndX + ALMANAC_STAR_GAP, theY, aStars,
		theRarity >= 3 ? Color(255, 200, 60) : Color(150, 140, 130), g->mClipRect);
	return anEnd;
}

// 画词条附录：标题「闯关词条 · <英文条名>」+ 中文说明（与局内三选一屏文案同源，
// 封顶条目自带「，至多 N 层」尾注）。theDescBottom = 介绍正文画完的底高（调用方实测）。
// 三级结构自适应（2026-10-03 实机验收两轮后定案）：死线 519（费用行从 520 起画）
// 减去实测底高决定形态——12px 下 标题行 19、正文行距 20：
//   1) 底高 ≤455：标题 + 表里 \n 分行的两行说明（59px，同三选一屏的形）；
//   2) 否则   ：\n 连接成单行（最长连接式 ≈246px < 258 放得下），标题 + 1 行（39px）；
//   3) 否则   ：连标题行也放不下（机枪豌豆实测底高 495，只剩 24px）→ 单行式：条名内联
//      在说明前面（宽度放得下才带），整块就一行（20px）。
// 任何一档都套「越死线整块上提」兜底；实测最长简介（13 行）在 3) 下不越线。
static void DrawAlmanacRunEntry(Graphics* g, int theEntryIndex, int theDescBottom)
{
	ModText::Font* aHeadFont = ModText::GetFont(AlmanacCjkPointSize(), true);
	ModText::Font* aBodyFont = ModText::GetFont(AlmanacCjkPointSize(), false);

	int aId = RUN_BUFF_COUNT + theEntryIndex;
	// 词条文案与三选一屏同源（UTF-8），折行/量宽/绘制全走宽字符；
	// DrawTextWide 收顶对齐（原 DrawString 是基线口径，这里换算成 y 即顶）
	std::wstring aDesc = ModText::WideFromUtf8(GetRunChoiceDesc(aId));
	std::wstring aJoined = aDesc;
	for (size_t aPos = aJoined.find(L'\n'); aPos != std::wstring::npos; aPos = aJoined.find(L'\n', aPos))
		aJoined.erase(aPos, 1);

	std::vector<std::wstring> aSplitLines;
	std::vector<std::wstring> aJoinedLines;
	AlmanacWrapEntry(aBodyFont, aDesc, ALMANAC_ENTRY_W, aSplitLines);
	AlmanacWrapEntry(aBodyFont, aJoined, ALMANAC_ENTRY_W, aJoinedLines);

	int aLineHeight = ModText::LineHeight(aBodyFont) + 3;
	int aHeadStep = ModText::LineHeight(aHeadFont) + 2;
	int aSplitBlock = aHeadStep + (int)aSplitLines.size() * aLineHeight;
	int aJoinedBlock = aHeadStep + (int)aJoinedLines.size() * aLineHeight;

	bool aFitsSplit = theDescBottom + 5 + aSplitBlock <= ALMANAC_ENTRY_BOTTOM;
	bool aFitsJoined = !aFitsSplit && theDescBottom + 3 + aJoinedBlock <= ALMANAC_ENTRY_BOTTOM;

	if (aFitsSplit || aFitsJoined)		// 1)/2) 带标题行
	{
		int aY = theDescBottom + (aFitsSplit ? 5 : 3);
		std::wstring aHead = ModText::WideFromUtf8(ModText::Tr("闯关词条 · ", "Run Modifier · "));
		aHead += ModText::WideFromUtf8(GetRunChoiceName(aId));		// 英文条名与三选一屏按钮同字
		ModText::DrawTextWide(g, aHeadFont, ALMANAC_ENTRY_X, aY, aHead, Color(160, 75, 15), g->mClipRect);
		DrawRarityStars(g, aHeadFont, GetRunPlantUpgradeDef(theEntryIndex).mRarity,
			ALMANAC_ENTRY_X + ModText::TextWidth(aHeadFont, aHead), aY);
		aY += aHeadStep;

		const std::vector<std::wstring>& aLines = aFitsSplit ? aSplitLines : aJoinedLines;
		for (int i = 0; i < (int)aLines.size(); i++)
		{
			ModText::DrawTextWide(g, aBodyFont, ALMANAC_ENTRY_X, aY, aLines[i], Color(125, 65, 30), g->mClipRect);
			aY += aLineHeight;
		}
		return;
	}

	// 3) 单行式：条名内联 + 连接式说明；名字加冒号后仍在一行宽内才带名字
	int aBlock = (int)aJoinedLines.size() * aLineHeight;
	int aY = theDescBottom + 3;
	if (aY + aBlock > ALMANAC_ENTRY_BOTTOM) aY = ALMANAC_ENTRY_BOTTOM - aBlock;

	std::wstring aName = ModText::WideFromUtf8(GetRunChoiceName(aId));
	std::wstring aColon = ModText::WideFromUtf8(ModText::Tr("：", ": "));
	int aRarity = GetRunPlantUpgradeDef(theEntryIndex).mRarity;
	std::wstring aStars = (aRarity > 0) ? std::wstring((size_t)aRarity, (wchar_t)0x2605) : L"";
	// 星标画在名字与冒号之间（超右限不画时起点差一颗星的缝——兜底档可接受）
	int aPrefixW = ModText::TextWidth(aHeadFont, aName + aColon);
	if (!aStars.empty()) aPrefixW += ALMANAC_STAR_GAP + ModText::TextWidth(aHeadFont, aStars);
	bool aWithName = !aJoinedLines.empty() && aPrefixW + ModText::TextWidth(aBodyFont, aJoinedLines[0]) <= ALMANAC_ENTRY_W;

	int aLineX = ALMANAC_ENTRY_X;
	if (aWithName)
	{
		ModText::DrawTextWide(g, aHeadFont, ALMANAC_ENTRY_X, aY, aName, Color(160, 75, 15), g->mClipRect);
		int aStarsEnd = DrawRarityStars(g, aHeadFont, aRarity,
			ALMANAC_ENTRY_X + ModText::TextWidth(aHeadFont, aName), aY);
		ModText::DrawTextWide(g, aHeadFont, aStarsEnd, aY, aColon, Color(160, 75, 15), g->mClipRect);
		aLineX += aPrefixW;
	}
	for (int i = 0; i < (int)aJoinedLines.size(); i++)
	{
		ModText::DrawTextWide(g, aBodyFont, (i == 0 && aWithName) ? aLineX : ALMANAC_ENTRY_X, aY,
			aJoinedLines[i], Color(125, 65, 30), g->mClipRect);
		aY += aLineHeight;
	}
}

//0x401010
AlmanacDialog::AlmanacDialog(LawnApp* theApp) : LawnDialog(theApp, DIALOG_ALMANAC, true, _S("Almanac"), _S(""), _S(""), BUTTONS_NONE)
{
	mApp = (LawnApp*)gSexyAppBase;
	mOpenPage = ALMANAC_PAGE_INDEX;
	mSelectedSeed = SEED_PEASHOOTER;
	mSelectedZombie = ZOMBIE_NORMAL;
	mZombie = nullptr;
	mPlant = nullptr;
	mDrawStandardBack = false;
	TodLoadResources("DelayLoad_Almanac");
	for (size_t i = 0; i < LENGTH(mZombiePerfTest); i++) mZombiePerfTest[i] = nullptr;
	LawnDialog::Resize(0, 0, BOARD_WIDTH, BOARD_HEIGHT);

	mCloseButton = new GameButton(AlmanacDialog::ALMANAC_BUTTON_CLOSE);
	mCloseButton->SetLabel(_S("[CLOSE_BUTTON]"));
	mCloseButton->mButtonImage = Sexy::IMAGE_ALMANAC_CLOSEBUTTON;
	mCloseButton->mOverImage = Sexy::IMAGE_ALMANAC_CLOSEBUTTONHIGHLIGHT;
	mCloseButton->mDownImage = nullptr;
	mCloseButton->SetFont(Sexy::FONT_BRIANNETOD12);
	Color aColor = Color(42, 42, 90);
	mCloseButton->mColors[ButtonWidget::COLOR_LABEL] = aColor;
	mCloseButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = aColor;
	mCloseButton->Resize(676, 567, 89, 26);
	mCloseButton->mParentWidget = this;
	mCloseButton->mTextOffsetX = -8;
	mCloseButton->mTextOffsetY = 1;

	mIndexButton = new GameButton(AlmanacDialog::ALMANAC_BUTTON_INDEX);
	mIndexButton->SetLabel(_S("[ALMANAC_INDEX]"));
	mIndexButton->mButtonImage = Sexy::IMAGE_ALMANAC_INDEXBUTTON;
	mIndexButton->mOverImage = Sexy::IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT;
	mIndexButton->mDownImage = nullptr;
	mIndexButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mIndexButton->mColors[ButtonWidget::COLOR_LABEL] = aColor;
	mIndexButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = aColor;
	mIndexButton->Resize(32, 567, 164, 26);
	mIndexButton->mParentWidget = this;
	mIndexButton->mTextOffsetX = 8;
	mIndexButton->mTextOffsetY = 1;

	mPlantButton = new GameButton(AlmanacDialog::ALMANAC_BUTTON_PLANT);
	mPlantButton->SetLabel(_S("[VIEW_PLANTS]"));
	mPlantButton->mButtonImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON;
	mPlantButton->mOverImage = nullptr;
	mPlantButton->mDownImage = nullptr;
	mPlantButton->mDisabledImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON_DISABLED;
	mPlantButton->mOverOverlayImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON_GLOW;
	mPlantButton->SetFont(Sexy::FONT_DWARVENTODCRAFT18YELLOW);
	mPlantButton->mColors[ButtonWidget::COLOR_LABEL] = Color::White;
	mPlantButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = Color::White;
	mPlantButton->Resize(130, 345, 156, 42);
	mPlantButton->mTextOffsetY = -1;
	mPlantButton->mParentWidget = this;

	mZombieButton = new GameButton(AlmanacDialog::ALMANAC_BUTTON_ZOMBIE);
	mZombieButton->SetLabel(_S("[VIEW_ZOMBIES]"));
	mZombieButton->Resize(487, 345, 210, 48);
	mZombieButton->mDrawStoneButton = true;
	mZombieButton->mParentWidget = this;

	SetPage(ALMANAC_PAGE_INDEX);
	if (!mApp->mBoard || !mApp->mBoard->mPaused)
		mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
}

//0x401880 & 0x4018A0
AlmanacDialog::~AlmanacDialog()
{
	if (mCloseButton)	delete mCloseButton;
	if (mIndexButton)	delete mIndexButton;
	if (mPlantButton)	delete mPlantButton;
	if (mZombieButton)	delete mZombieButton;

	ClearPlantsAndZombies();
}

//0x401970
void AlmanacDialog::ClearPlantsAndZombies()
{
	if (mPlant)
	{
		mPlant->Die();
		delete mPlant;
		mPlant = nullptr;
	}
	if (mZombie)
	{
		mZombie->DieNoLoot();
		delete mZombie;
		mZombie = nullptr;
	}
	for (Zombie* &aZombie : mZombiePerfTest)
	{
		if (aZombie)
		{
			aZombie->DieNoLoot();
			delete aZombie;
		}
		aZombie = nullptr;
	}
}

//0x401A10
void AlmanacDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	ClearPlantsAndZombies();
}

//0x401A30
// GOTY @Patoke: 0x402C50
void AlmanacDialog::SetupPlant()
{
	ClearPlantsAndZombies();

	float aPosX = ALMANAC_PLANT_POSITION_X;
	float aPosY = ALMANAC_PLANT_POSITION_Y;
	if (mSelectedSeed == SEED_TALLNUT)				aPosY += 18;
	else if (mSelectedSeed == SEED_COBCANNON)		aPosX -= 40;
	else if (mSelectedSeed == SEED_FLOWERPOT)		aPosY -= 20;
	else if (mSelectedSeed == SEED_INSTANT_COFFEE)	aPosY += 20;
	else if (mSelectedSeed == SEED_GRAVEBUSTER)		aPosY += 55;

	mPlant = new Plant();
	mPlant->mBoard = nullptr;
	mPlant->mIsOnBoard = false;
	mPlant->PlantInitialize(0, 0, mSelectedSeed, SEED_NONE);
	mPlant->mX = aPosX;
	mPlant->mY = aPosY;
}

//0x401B70
// GOTY @Patoke: 0x402D90
void AlmanacDialog::SetupZombie()
{
	ClearPlantsAndZombies();

	mZombie = new Zombie();
	mZombie->mBoard = nullptr;
	mZombie->ZombieInitialize(0, mSelectedZombie, false, nullptr, Zombie::ZOMBIE_WAVE_UI);
	mZombie->mPosX = ALMANAC_ZOMBIE_POSITION_X;
	mZombie->mPosY = ALMANAC_ZOMBIE_POSITION_Y;
}

//0x401BE0
void AlmanacDialog::SetPage(AlmanacPage thePage)
{
	mOpenPage = thePage;
	ClearPlantsAndZombies();

	if (mOpenPage == AlmanacPage::ALMANAC_PAGE_INDEX)
	{
		mPlant = new Plant();
		mPlant->mBoard = nullptr;
		mPlant->mIsOnBoard = false;
		mPlant->PlantInitialize(0, 0, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
		mPlant->mX = ALMANAC_INDEXPLANT_POSITION_X;
		mPlant->mY = ALMANAC_INDEXPLANT_POSITION_Y;

		mZombie = new Zombie();
		mZombie->mBoard = nullptr;
		mZombie->ZombieInitialize(0, ZombieType::ZOMBIE_NORMAL, false, nullptr, Zombie::ZOMBIE_WAVE_UI);
		mZombie->mPosX = ALMANAC_INDEXZOMBIE_POSITION_X;
		mZombie->mPosY = ALMANAC_INDEXZOMBIE_POSITION_Y;

		mIndexButton->mBtnNoDraw = true;
		mPlantButton->mBtnNoDraw = false;
		mZombieButton->mBtnNoDraw = false;
	}
	else
	{
		if (mOpenPage == AlmanacPage::ALMANAC_PAGE_PLANTS)
			SetupPlant();
		else if (mOpenPage == AlmanacPage::ALMANAC_PAGE_ZOMBIES)
			SetupZombie();
		else return;

		mIndexButton->mBtnNoDraw = false;
		mPlantButton->mBtnNoDraw = true;
		mZombieButton->mBtnNoDraw = true;
	}
}

void AlmanacDialog::ShowPlant(SeedType theSeedType)
{
	mSelectedSeed = theSeedType;
	SetPage(ALMANAC_PAGE_PLANTS);
}

void AlmanacDialog::ShowZombie(ZombieType theZombieType)
{
	mSelectedZombie = theZombieType;
	SetPage(ALMANAC_PAGE_ZOMBIES);
}

//0x401D30
void AlmanacDialog::Update()
{
	mCloseButton->Update();
	mIndexButton->Update();
	mPlantButton->Update();
	mZombieButton->Update();
	if (mPlant) mPlant->Update();
	if (mZombie) mZombie->Update();
	for (Zombie* aZombie : mZombiePerfTest)
	{
		if (aZombie)
		{
			aZombie->Update();
		}
	}

	int aMouseX = mApp->mWidgetManager->mLastMouseX;
	int aMouseY = mApp->mWidgetManager->mLastMouseY;
	if (SeedHitTest(aMouseX, aMouseY) != SeedType::SEED_NONE || ZombieHitTest(aMouseX, aMouseY) != ZombieType::ZOMBIE_INVALID || 
		mCloseButton->IsMouseOver() || mIndexButton->IsMouseOver() || mPlantButton->IsMouseOver() || mZombieButton->IsMouseOver())
	{
		mApp->SetCursor(CURSOR_HAND);
	}
	else
	{
		mApp->SetCursor(CURSOR_POINTER);
	}

	mApp->mPoolEffect->PoolEffectUpdate();
	MarkDirty();
}

ZombieType AlmanacDialog::GetZombieType(int theIndex)
{
	return theIndex < NUM_ZOMBIE_TYPES ? (ZombieType)theIndex : ZOMBIE_INVALID;
}

//0x401E70
void AlmanacDialog::DrawIndex(Graphics* g)
{
	g->DrawImage(Sexy::IMAGE_ALMANAC_INDEXBACK, 0, 0);
	TodDrawString(g, _S("[SUBURBAN_ALMANAC_INDEX]"), BOARD_WIDTH / 2, 60, Sexy::FONT_HOUSEOFTERROR28, Color(220, 220, 220), DrawStringJustification::DS_ALIGN_CENTER);
	
	if (mPlant)
	{
		Graphics aPlantGraphics = Graphics(*g);
		mPlant->BeginDraw(&aPlantGraphics);
		mPlant->Draw(&aPlantGraphics);
	}
	if (mZombie)
	{
		Graphics aZombieGraphics = Graphics(*g);
		mZombie->BeginDraw(&aZombieGraphics);
		mZombie->Draw(&aZombieGraphics);
	}
}

//0x402060
void AlmanacDialog::DrawPlants(Graphics* g)
{
	g->DrawImage(Sexy::IMAGE_ALMANAC_PLANTBACK, 0, 0);
	TodDrawString(g, _S("[SUBURBAN_ALMANAC_PLANTS]"), BOARD_WIDTH / 2, 48, Sexy::FONT_HOUSEOFTERROR20, Color(213, 159, 43), DrawStringJustification::DS_ALIGN_CENTER);

	SeedType aSeedMouseOn = SeedHitTest(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY);
	for (SeedType aSeedType = SeedType::SEED_PEASHOOTER; aSeedType < NUM_ALMANAC_SEEDS; aSeedType = (SeedType)(aSeedType + 1))
	{
		int aPosX, aPosY;
		GetSeedPosition(aSeedType, aPosX, aPosY);
		// 图鉴全解锁（2026-10-03 用户定案）：49 格全画、全可点——不再按 SeedTypeAvailable
		// 过滤（闯关局按本局卡池、平时按档案购买记录，都会让图鉴缺格 / 交互半残）。
		if (aSeedType == SeedType::SEED_IMITATER)
		{
			if (aSeedType == aSeedMouseOn)
				g->DrawImage(Sexy::IMAGE_ALMANAC_IMITATER, aPosX, aPosY);
			g->DrawImage(Sexy::IMAGE_ALMANAC_IMITATER, aPosX, aPosY);
		}
		else
		{
			DrawSeedPacket(g, aPosX, aPosY, aSeedType, SeedType::SEED_NONE, 0, 255, true, false);
			if (aSeedType == aSeedMouseOn)
				g->DrawImage(Sexy::IMAGE_SEEDPACKETFLASH, aPosX, aPosY);
		}
	}

	if (mSelectedSeed == SeedType::SEED_LILYPAD || mSelectedSeed == SeedType::SEED_TANGLEKELP || 
		mSelectedSeed == SeedType::SEED_CATTAIL || mSelectedSeed == SeedType::SEED_SEASHROOM)
	{
		bool aNight = mSelectedSeed == SeedType::SEED_SEASHROOM;
		g->DrawImage(aNight ? Sexy::IMAGE_ALMANAC_GROUNDNIGHTPOOL : Sexy::IMAGE_ALMANAC_GROUNDPOOL, 521, 107);

		if (mApp->Is3DAccelerated())
		{
			g->SetClipRect(475, 0, 397, 500);
			g->mTransY -= 145;
			mApp->mPoolEffect->PoolEffectDraw(g, aNight);
			g->mTransY += 145;
			g->ClearClipRect();
		}
	}
	else
	{
		g->DrawImage(
			Plant::IsNocturnal(mSelectedSeed) || mSelectedSeed == SeedType::SEED_GRAVEBUSTER || mSelectedSeed == SeedType::SEED_PLANTERN ? Sexy::IMAGE_ALMANAC_GROUNDNIGHT :
			mSelectedSeed == SeedType::SEED_FLOWERPOT ? Sexy::IMAGE_ALMANAC_GROUNDROOF : Sexy::IMAGE_ALMANAC_GROUNDDAY,
			521, 107
		);
	}
	
	if (mPlant)
	{
		Graphics aPlantGraphics = Graphics(*g);
		mPlant->BeginDraw(&aPlantGraphics);
		mPlant->Draw(&aPlantGraphics);
	}

	g->DrawImage(Sexy::IMAGE_ALMANAC_PLANTCARD, 459, 86);
	PlantDefinition& aPlantDef = GetPlantDefinition(mSelectedSeed);
	SexyString aName = Plant::GetNameString(mSelectedSeed, SEED_NONE);
	SexyString aDescriptionName = StrFormat(_S("[%s_DESCRIPTION]"), aPlantDef.mPlantName);
	TodDrawString(g, aName, 617, 288, Sexy::FONT_DWARVENTODCRAFT18YELLOW, Color::White, DS_ALIGN_CENTER);
	TodDrawStringWrapped(g, aDescriptionName, Rect(485, 309, 258, 230), Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), DS_ALIGN_LEFT);

	// 词条附录（2026-10-03 用户定案）：介绍正文下方附这株在闯关里的单株词条
	// （名字 + 中文说明，与局内三选一屏同源）。模仿者没有单株条目 → 没有附录。
	int aRunEntryIndex = RunPlantUpgradeIndexFor(mSelectedSeed);
	if (aRunEntryIndex >= 0)
	{
		int aDescHeight = TodDrawStringWrappedHelper(g, TodStringTranslate(aDescriptionName), Rect(485, 309, 258, 230), Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), DS_ALIGN_LEFT, false);
		DrawAlmanacRunEntry(g, aRunEntryIndex, 309 + aDescHeight);
	}

	if (mSelectedSeed != SeedType::SEED_IMITATER)
	{
		SexyString aCostStr = TodReplaceString(StrFormat(_S("{KEYWORD}{COST}:{STAT} %d"), aPlantDef.mSeedCost), _S("{COST}"), _S("[COST]"));
		TodDrawStringWrapped(g, aCostStr, Rect(485, 520, 134, 50), Sexy::FONT_BRIANNETOD12, Color::White, DS_ALIGN_LEFT);

		SexyString aRechargeStr = TodReplaceString(
			_S("{KEYWORD}{WAIT_TIME}:{STAT}{WAIT_TIME_LENGTH}"), 
			_S("{WAIT_TIME_LENGTH}"),
			aPlantDef.mRefreshTime == 750 ? _S("[WAIT_TIME_SHORT]") : aPlantDef.mRefreshTime == 3000 ? _S("[WAIT_TIME_LONG]") : _S("[WAIT_TIME_VERY_LONG]") // @Patoke: fix typo XD
		);
		aRechargeStr = TodReplaceString(aRechargeStr, _S("{WAIT_TIME}"), _S("[WAIT_TIME]"));
		TodDrawStringWrapped(g, aRechargeStr, Rect(600, 520, 139, 50), Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), DS_ALIGN_RIGHT);
	}
}

//0x402C00
// GOTY @Patoke: 0x403DE0
void AlmanacDialog::DrawZombies(Graphics* g)
{
	g->DrawImage(Sexy::IMAGE_ALMANAC_ZOMBIEBACK, 0, 0);
	TodDrawString(g, _S("[SUBURBAN_ALMANAC_ZOMBIES]"), BOARD_WIDTH / 2, 54, Sexy::FONT_DWARVENTODCRAFT24, Color(0, 196, 0), DS_ALIGN_CENTER);

	ZombieType aZombieMouseOn = ZombieHitTest(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY);
	for (int i = 0; i < NUM_ALMANAC_ZOMBIES; i++)
	{
		ZombieType aZombieType = GetZombieType(i);
		int aPosX, aPosY;
		GetZombiePosition(aZombieType, aPosX, aPosY);
		if (aZombieType != ZombieType::ZOMBIE_INVALID)
		{
			if (!ZombieIsShown(aZombieType))
				g->DrawImage(Sexy::IMAGE_ALMANAC_ZOMBIEBLANK, aPosX, aPosY);
			else
			{
				g->DrawImage(Sexy::IMAGE_ALMANAC_ZOMBIEWINDOW, aPosX, aPosY);
				if (aZombieType == aZombieMouseOn)
				{
					g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
					g->SetColor(Color(255, 255, 255, 48));
					g->SetColorizeImages(true);
					g->DrawImage(Sexy::IMAGE_ALMANAC_ZOMBIEWINDOW, aPosX, aPosY);
					g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
					g->SetColorizeImages(false);
				}

				ZombieType aZombieTypeToDraw = aZombieType;
				Graphics aZombieGraphics = Graphics(*g);
				aZombieGraphics.SetClipRect(aPosX + 2, aPosY + 2, 72, 72);
				aZombieGraphics.Translate(aPosX + 1, aPosY - 6);
				aZombieGraphics.mScaleX = 0.5f;
				aZombieGraphics.mScaleY = 0.5f;
				// @Patoke todo: add new functionality
				switch (aZombieType)
				{
				case ZombieType::ZOMBIE_POLEVAULTER:
					aZombieGraphics.TranslateF(2, -3);
					aZombieTypeToDraw = ZombieType::ZOMBIE_CACHED_POLEVAULTER_WITH_POLE;		break;
				case ZombieType::ZOMBIE_FLAG:			aZombieGraphics.TranslateF(2, 10);		break;
				case ZombieType::ZOMBIE_TRAFFIC_CONE:	aZombieGraphics.TranslateF(0, 12);		break;
				case ZombieType::ZOMBIE_PAIL:			aZombieGraphics.TranslateF(0, 9);		break;
				case ZombieType::ZOMBIE_FOOTBALL:		aZombieGraphics.TranslateF(-15, -1);	break;
				case ZombieType::ZOMBIE_ZAMBONI:		aZombieGraphics.TranslateF(0, 3);		break;
				case ZombieType::ZOMBIE_DOLPHIN_RIDER:	aZombieGraphics.TranslateF(-2, -10);	break;
				case ZombieType::ZOMBIE_POGO:			aZombieGraphics.TranslateF(0, -3);		break;
				case ZombieType::ZOMBIE_GARGANTUAR:		aZombieGraphics.TranslateF(15, 17);		break;
				case ZombieType::ZOMBIE_IMP:			aZombieGraphics.TranslateF(-8, -7);		break;
				case ZombieType::ZOMBIE_BUNGEE:			aZombieGraphics.TranslateF(-4, 3);		break;
				case ZombieType::ZOMBIE_BACKUP_DANCER:	aZombieGraphics.TranslateF(-8, 5);		break;
				case ZombieType::ZOMBIE_SNORKEL:		aZombieGraphics.TranslateF(-10, 0);		break;
				case ZombieType::ZOMBIE_YETI:			aZombieGraphics.TranslateF(0, 4);		break;
				case ZombieType::ZOMBIE_CATAPULT:		aZombieGraphics.TranslateF(-24, -1);	break;
				case ZombieType::ZOMBIE_BOBSLED:		aZombieGraphics.TranslateF(0, -8);		break;
				case ZombieType::ZOMBIE_LADDER:			aZombieGraphics.TranslateF(0, -3);		break;
				default: break;
				}
				if (ZombieHasSilhouette(aZombieType))
				{
					aZombieGraphics.SetColor(Color(0, 0, 0, 40));
					aZombieGraphics.SetColorizeImages(true);
				}
				mApp->mReanimatorCache->DrawCachedZombie(&aZombieGraphics, 0, 0, aZombieTypeToDraw);
				aZombieGraphics.SetColorizeImages(false);

				g->DrawImage(Sexy::IMAGE_ALMANAC_ZOMBIEWINDOW2, aPosX, aPosY);
				if (aZombieType == aZombieMouseOn)
				{
					g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
					g->SetColor(Color(255, 255, 255, 48));
					g->SetColorizeImages(true);
					g->DrawImage(Sexy::IMAGE_ALMANAC_ZOMBIEWINDOW2, aPosX, aPosY);
					g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
					g->SetColorizeImages(false);
				}
			}
		}
	}

	g->DrawImage(mZombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombie->mZombieType == ZombieType::ZOMBIE_BOBSLED ?
		Sexy::IMAGE_ALMANAC_GROUNDICE : Sexy::IMAGE_ALMANAC_GROUNDDAY, 518, 110);
	if (mZombie && !ZombieHasSilhouette(mZombie->mZombieType))
	{
		Graphics aZombieGraphics = Graphics(*g);
		mZombie->BeginDraw(&aZombieGraphics);
		aZombieGraphics.SetClipRect(-42, -51, 197, 187);
		// @Patoke todo: add new functionality
		switch (mZombie->mZombieType)
		{
		case ZombieType::ZOMBIE_ZAMBONI:		aZombieGraphics.TranslateF(-30, 5);		break;
		case ZombieType::ZOMBIE_GARGANTUAR:		aZombieGraphics.TranslateF(0, 40);		break;
		case ZombieType::ZOMBIE_FOOTBALL:		aZombieGraphics.TranslateF(-10, 0);		break;
		case ZombieType::ZOMBIE_BALLOON:		aZombieGraphics.TranslateF(0, -20);		break;
		case ZombieType::ZOMBIE_BUNGEE:			aZombieGraphics.TranslateF(15, 0);		break;
		case ZombieType::ZOMBIE_CATAPULT:		aZombieGraphics.TranslateF(-10, 0);		break;
		case ZombieType::ZOMBIE_BOSS:			aZombieGraphics.TranslateF(-540, -175);	break;
		default: break;
		}
		if (mZombie->mZombieType != ZombieType::ZOMBIE_BUNGEE && mZombie->mZombieType != ZombieType::ZOMBIE_BOSS &&
			mZombie->mZombieType != ZombieType::ZOMBIE_ZAMBONI && mZombie->mZombieType != ZombieType::ZOMBIE_CATAPULT)
			mZombie->DrawShadow(&aZombieGraphics);
		mZombie->Draw(&aZombieGraphics);
	}
	g->DrawImage(Sexy::IMAGE_ALMANAC_ZOMBIECARD, 455, 78);

	ZombieDefinition& aZombieDef = GetZombieDefinition(mSelectedZombie);
	SexyString aName = ZombieHasSilhouette(mSelectedZombie) ? _S("???") : StrFormat(_S("[%s]"), aZombieDef.mZombieName);
	TodDrawString(g, aName, 613, 362, Sexy::FONT_DWARVENTODCRAFT18GREENINSET, Color(190, 255, 235, 255), DS_ALIGN_CENTER);

	SexyString aDescription;
	DrawStringJustification aAlign;
	if (ZombieHasDescription(mSelectedZombie))
	{
		aDescription = TodStringTranslate(StrFormat(_S("[%s_DESCRIPTION]"), aZombieDef.mZombieName));
		aAlign = DS_ALIGN_LEFT;
	}
	else
	{
		aDescription = _S("[NOT_ENCOUNTERED_YET]");
		aAlign = DS_ALIGN_CENTER_VERTICAL_MIDDLE;
	}
	for (TodStringListFormat& aFormat : gLawnStringFormats)
	{
		if (TestBit(aFormat.mFormatFlags, TodStringFormatFlag::TOD_FORMAT_HIDE_UNTIL_MAGNETSHROOM))
		{
			// 图鉴全解锁（2026-10-03 用户定案）：原先要拥有磁力菇才显形的段落
			//（僵尸说明里的弱点行）不再等拥有——直接全亮显示。
			aFormat.mNewColor.mAlpha = 255;
			aFormat.mLineSpacingOffset = 0;
		}
	}
	// todo @Patoke: fix stuff that have another formatter after them, ex: "{KEYWORD}Weakness:{STAT} fume-shroom{METAL} and magnet-shroom{KEYWORD}" (magnet-shroom will show with the {KEYWORD} colors)
	// @Patoke: added extra check for the zamboni zombie
	TodDrawStringWrapped(g, aDescription, Rect(484, mSelectedZombie == ZombieType::ZOMBIE_ZAMBONI ? 372 : 377, 258, 170), Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), aAlign);
}

//0x403810
void AlmanacDialog::Draw(Graphics* g)
{
	g->SetLinearBlend(true);
	switch (mOpenPage)
	{
	case AlmanacPage::ALMANAC_PAGE_INDEX:	DrawIndex(g);	break;
	case AlmanacPage::ALMANAC_PAGE_PLANTS:	DrawPlants(g);	break;
	case AlmanacPage::ALMANAC_PAGE_ZOMBIES:	DrawZombies(g);	break;
	}

	for (Zombie* aZombie : mZombiePerfTest)
	{
		if (aZombie)
		{
			Graphics aTestGraphics = Graphics(*g);
			aZombie->Draw(&aTestGraphics);
		}
	}

	mCloseButton->Draw(g);
	mIndexButton->Draw(g);
	mPlantButton->Draw(g);
	mZombieButton->Draw(g);
}

void AlmanacDialog::GetSeedPosition(SeedType theSeedType, int& x, int& y)
{
	if (theSeedType == SeedType::SEED_IMITATER)
		x = 20, y = 23;
	else
	{
		x = theSeedType % 8 * 52 + 26;
		y = theSeedType / 8 * 78 + 92;
	}
}

//0x403940
SeedType AlmanacDialog::SeedHitTest(int x, int y)
{
	if (mMouseVisible && mOpenPage == AlmanacPage::ALMANAC_PAGE_PLANTS)
	{
		for (SeedType aSeedType = SeedType::SEED_PEASHOOTER; aSeedType < NUM_ALMANAC_SEEDS; aSeedType = (SeedType)(aSeedType + 1))
		{
			// 图鉴全解锁（2026-10-03）：命中不再看 SeedTypeAvailable，49 格全可点
			int aSeedX, aSeedY;
			GetSeedPosition(aSeedType, aSeedX, aSeedY);
			Rect aSeedRect = aSeedType == SeedType::SEED_IMITATER ? Rect(aSeedX, aSeedY, 34, 46) : Rect(aSeedX, aSeedY, SEED_PACKET_WIDTH, SEED_PACKET_HEIGHT);
			if (aSeedRect.Contains(x, y)) return aSeedType;
		}
	}
	return SeedType::SEED_NONE;
}

bool AlmanacDialog::ZombieHasSilhouette(ZombieType theZombieType)
{
	// 图鉴全解锁（2026-10-03 用户定案）：不再画剪影——雪人僵尸也直接显示本体。
	// （原先两条分支：没见过的雪人画剪影 / 冒险模式到 4-10 后才显示本体。）
	(void)theZombieType;
	return false;
}

//0x403A10
// GOTY @Patoke: 0x404C50
bool AlmanacDialog::ZombieIsShown(ZombieType theZombieType)
{
	// 图鉴全解锁（2026-10-03 用户定案）：26 格全画、全可点——不再按试玩锁 / 档案进度 /
	// 雪人条件（CanSpawnYetis）过滤。
	(void)theZombieType;
	return true;
}

//0x403B30
// GOTY @Patoke: 0x404D50
bool AlmanacDialog::ZombieHasDescription(ZombieType theZombieType)
{
	// 图鉴全解锁（2026-10-03）：说明一律给真文案，不再有 [NOT_ENCOUNTERED_YET]。
	(void)theZombieType;
	return true;
}

void AlmanacDialog::GetZombiePosition(ZombieType theZombieType, int& x, int& y)
{
	if (theZombieType == ZombieType::ZOMBIE_BOSS)
		x = 192, y = 486;
	else
	{
		x = theZombieType % 5 * 85 + 22;
		y = theZombieType / 5 * 80 + 86;
	}
}

//0x403BB0
// GOTY @Patoke: 0x404DD0
ZombieType AlmanacDialog::ZombieHitTest(int x, int y)
{
	if (mMouseVisible && mOpenPage == AlmanacPage::ALMANAC_PAGE_ZOMBIES)
	{
		for (int i = 0; i < NUM_ALMANAC_ZOMBIES; i++)
		{
			ZombieType aZombieType = GetZombieType(i);
			// @Patoke: added IsShown check
			if (aZombieType != ZombieType::ZOMBIE_INVALID && ZombieIsShown(aZombieType))
			{
				int aZombieX, aZombieY;
				GetZombiePosition(aZombieType, aZombieX, aZombieY);
				if (Rect(aZombieX, aZombieY, 76, 76).Contains(x, y))
					return aZombieType;
			}
		}
	}
	return ZombieType::ZOMBIE_INVALID;
}

//0x403C60
void AlmanacDialog::MouseUp(int x, int y, int theClickCount)
{
	(void)x;(void)y;(void)theClickCount;
	if (mPlantButton->IsMouseOver())		SetPage(ALMANAC_PAGE_PLANTS);
	else if (mZombieButton->IsMouseOver())	SetPage(ALMANAC_PAGE_ZOMBIES);
	else if (mCloseButton->IsMouseOver())	mApp->KillAlmanacDialog();
	else if (mIndexButton->IsMouseOver())	SetPage(ALMANAC_PAGE_INDEX);
}

//0x403D00
// GOTY @Patoke: 0x404F10
void AlmanacDialog::MouseDown(int x, int y, int theClickCount)
{
	(void)theClickCount;
	if (mPlantButton->IsMouseOver() || mCloseButton->IsMouseOver() || mIndexButton->IsMouseOver())
		mApp->PlaySample(Sexy::SOUND_TAP);
	if (mZombieButton->IsMouseOver())
		mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);

	SeedType aSeedType = SeedHitTest(x, y);
	if (aSeedType != SeedType::SEED_NONE && aSeedType != mSelectedSeed)
	{
		mSelectedSeed = aSeedType;
		SetupPlant();
		mApp->PlaySample(Sexy::SOUND_TAP);
	}
	ZombieType aZombieType = ZombieHitTest(x, y);
	if (aZombieType != ZombieType::ZOMBIE_INVALID && aZombieType != mSelectedZombie)
	{
		mSelectedZombie = aZombieType;
		SetupZombie();
		mApp->PlaySample(Sexy::SOUND_TAP);
	}
}

void AlmanacInitForPlayer()
{
	for (int i = 0; i < ZombieType::NUM_ZOMBIE_TYPES; i++)
		gZombieDefeated[i] = false;
}

void AlmanacPlayerDefeatedZombie(ZombieType theZombieType)
{
	gZombieDefeated[(int)theZombieType] = true;
}