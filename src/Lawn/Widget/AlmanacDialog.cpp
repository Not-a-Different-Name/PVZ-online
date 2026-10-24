#include "MobileCoopDraw.h"
#include "RunInfoView.h"
#include "../Run/RunBuffs.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/Reanimator.h"
#include <vector>
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

#include <format>
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Run/AnimationAdmission.h"
#include "GameButton.h"
#include "../SeedPacket.h"
#include "../../LawnApp.h"
#include "AlmanacDialog.h"
#include "../../Resources.h"
#include "../System/Music.h"
#include "../../GameConstants.h"
#include "../System/PlayerInfo.h"
#include "../System/PoolEffect.h"
#include "../System/ReanimationLawn.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "widget/WidgetManager.h"

static Rect AlmanacEntryArrow(bool right) { return Rect(right ? 562 : 222, 552, 90, 44); }

static std::vector<int> MobileAlmanacEntries(SeedType seed)
{
    std::vector<int> entries;
    int first=RunPlantUpgradeIndexFor(seed);
    if(first>=0 && RunPlantUpgradeInPool(seed)) entries.push_back(RUN_BUFF_COUNT+first);
    for(int i=0;i<RUN_PLANT_BUFF2_COUNT;++i)
        if(GetRunPlantBuff2Def(i).mPlant==seed && RunPlantBuff2InPool(i))
            entries.push_back(RUN_BUFF_COUNT+RUN_PLANT_UPGRADE_COUNT+i);
    return entries;
}

bool gZombieDefeated[NUM_ZOMBIE_TYPES] = { false };

// Preview construction is synchronous, just like Board admission. Keep the plant
// reservation and existing actors intact; a missing cosmetic preview can retry.
static bool AlmanacCanCreatePreview(LawnApp* app, unsigned int count)
{
    if (!app || !app->mEffectSystem || !app->mEffectSystem->mReanimationHolder) return false;
    ReanimationHolder* holder = app->mEffectSystem->mReanimationHolder.get();
    auto& pool = holder->mReanimations;
    if (!pool.mMaxSize || count > pool.mMaxSize || holder->mPlantReservation > pool.mMaxSize - count)
        return false;
    return pool.DataArrayCanAllocate(count + holder->mPlantReservation) && pool.DataArrayValidateState();
}

static void AlmanacDeletePlant(std::unique_ptr<Plant>& plant)
{
    if (!plant) return;
    plant->Die();
    plant.reset();
}

static void AlmanacDeleteZombie(std::unique_ptr<Zombie>& zombie)
{
    if (!zombie) return;
    zombie->DieNoLoot();
    zombie.reset();
}

static void AlmanacRefreshPreviews(AlmanacDialog* dialog)
{
    const bool index = dialog->mOpenPage == AlmanacPage::ALMANAC_PAGE_INDEX;
    const bool plants = dialog->mOpenPage == AlmanacPage::ALMANAC_PAGE_PLANTS;
    const bool zombies = dialog->mOpenPage == AlmanacPage::ALMANAC_PAGE_ZOMBIES;
    if (!index && !plants && !zombies) return;
    if (zombies) AlmanacDeletePlant(dialog->mPlant);
    if (plants) AlmanacDeleteZombie(dialog->mZombie);

    if (index || plants)
    {
        const SeedType seed = index ? SeedType::SEED_SUNFLOWER : dialog->mSelectedSeed;
        if (!dialog->mPlant || dialog->mPlant->mSeedType != seed)
        {
            unsigned int need = GetPlantDefinition(seed).mReanimationType == ReanimationType::REANIM_NONE ? 0U : 1U;
            switch (seed)
            {
            case SeedType::SEED_PEASHOOTER: case SeedType::SEED_SNOWPEA:
            case SeedType::SEED_REPEATER: case SeedType::SEED_LEFTPEATER: case SeedType::SEED_GATLINGPEA: ++need; break;
            case SeedType::SEED_SPLITPEA: need += 2; break;
            case SeedType::SEED_THREEPEATER: need += 3; break;
            default: break;
            }
            // Off-board previews do not allocate sleeping indicators.
            if (AlmanacCanCreatePreview(dialog->mApp, need))
            {
                Plant::PreloadPlantResources(seed);
                auto replacement = std::make_unique<Plant>();
                replacement->mBoard = nullptr;
                replacement->mIsOnBoard = false;
                replacement->PlantInitialize(0, 0, seed, SeedType::SEED_NONE);
                AlmanacDeletePlant(dialog->mPlant);
                dialog->mPlant = std::move(replacement);
            }
        }
        if (dialog->mPlant && dialog->mPlant->mSeedType == seed)
        {
            float x = index ? ALMANAC_INDEXPLANT_POSITION_X : ALMANAC_PLANT_POSITION_X;
            float y = index ? ALMANAC_INDEXPLANT_POSITION_Y : ALMANAC_PLANT_POSITION_Y;
            if (!index)
            {
                if (seed == SEED_TALLNUT) y += 18;
                else if (seed == SEED_COBCANNON) x -= 40;
                else if (seed == SEED_FLOWERPOT) y -= 20;
                else if (seed == SEED_INSTANT_COFFEE) y += 20;
                else if (seed == SEED_GRAVEBUSTER) y += 55;
            }
            dialog->mPlant->mX = x;
            dialog->mPlant->mY = y;
        }
    }
    if (index || zombies)
    {
        const ZombieType type = index ? ZombieType::ZOMBIE_NORMAL : dialog->mSelectedZombie;
        if (!dialog->mZombie || dialog->mZombie->mZombieType != type)
        {
            const ZombieType admittedType = type;
            if (AlmanacCanCreatePreview(dialog->mApp, AnimationAdmission::ZombieOne(admittedType)))
            {
                Zombie::PreloadZombieResources(type);
                auto replacement = std::make_unique<Zombie>();
                replacement->mBoard = nullptr;
                replacement->ZombieInitialize(0, type, false, nullptr, Zombie::ZOMBIE_WAVE_UI);
                AlmanacDeleteZombie(dialog->mZombie);
                dialog->mZombie = std::move(replacement);
            }
        }
        if (dialog->mZombie && dialog->mZombie->mZombieType == type)
        {
            dialog->mZombie->mPosX = index ? ALMANAC_INDEXZOMBIE_POSITION_X : ALMANAC_ZOMBIE_POSITION_X;
            dialog->mZombie->mPosY = index ? ALMANAC_INDEXZOMBIE_POSITION_Y : ALMANAC_ZOMBIE_POSITION_Y;
        }
    }
}



AlmanacDialog::AlmanacDialog(LawnApp* theApp) : LawnDialog(theApp, DIALOG_ALMANAC, true, theApp->GetString("ALMANAC_HEADER", "Almanac"), "", "", BUTTONS_NONE)
{
	mApp = (LawnApp*)gSexyAppBase;
	mOpenPage = ALMANAC_PAGE_INDEX;
	mSelectedSeed = SEED_PEASHOOTER;
	mSelectedZombie = ZOMBIE_NORMAL;
	mDrawStandardBack = false;
	mLoadedResourceNames.push_back("DelayLoad_Almanac");
	LawnDialog::Resize(0, 0, BOARD_WIDTH, BOARD_HEIGHT);

	for (std::string& resource : mLoadedResourceNames)
		PvzpLoadResources(resource.c_str());

	mCloseButton = std::make_unique<GameButton>(AlmanacDialog::ALMANAC_BUTTON_CLOSE);
	mCloseButton->SetLabel("[CLOSE_BUTTON]");
	mCloseButton->mButtonImage = Sexy::IMAGE_ALMANAC_CLOSEBUTTON;
	mCloseButton->mOverImage = Sexy::IMAGE_ALMANAC_CLOSEBUTTONHIGHLIGHT;
	mCloseButton->mDownImage = nullptr;
	mCloseButton->SetFont(Sexy::FONT_BRIANNETOD12);
	Color aColor = Color(42, 42, 90);
	mCloseButton->SetLabelColor(aColor);
	mCloseButton->SetLabelHiliteColor(aColor);
	mCloseButton->Resize(676, 567, 89, 26);
	mCloseButton->mParentWidget = this;
	mCloseButton->mTextOffsetX = -8;
	mCloseButton->mTextOffsetY = 1;

	mIndexButton = std::make_unique<GameButton>(AlmanacDialog::ALMANAC_BUTTON_INDEX);
	mIndexButton->SetLabel("[ALMANAC_INDEX]");
	mIndexButton->mButtonImage = Sexy::IMAGE_ALMANAC_INDEXBUTTON;
	mIndexButton->mOverImage = Sexy::IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT;
	mIndexButton->mDownImage = nullptr;
	mIndexButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mIndexButton->SetLabelColor(aColor);
	mIndexButton->SetLabelHiliteColor(aColor);
	mIndexButton->Resize(32, 567, 164, 26);
	mIndexButton->mParentWidget = this;
	mIndexButton->mTextOffsetX = 8;
	mIndexButton->mTextOffsetY = 1;

	mPlantButton = std::make_unique<GameButton>(AlmanacDialog::ALMANAC_BUTTON_PLANT);
	mPlantButton->SetLabel("[VIEW_PLANTS]");
	mPlantButton->mButtonImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON;
	mPlantButton->mOverImage = nullptr;
	mPlantButton->mDownImage = nullptr;
	mPlantButton->mDisabledImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON_DISABLED;
	mPlantButton->mOverOverlayImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON_GLOW;
	mPlantButton->SetFont(Sexy::FONT_DWARVENTODCRAFT18YELLOW);
	mPlantButton->SetLabelColor(Color::White);
	mPlantButton->SetLabelHiliteColor(Color::White);
	mPlantButton->Resize(130, 345, 156, 42);
	mPlantButton->mTextOffsetY = -1;
	mPlantButton->mParentWidget = this;

	mZombieButton = std::make_unique<GameButton>(AlmanacDialog::ALMANAC_BUTTON_ZOMBIE);
	mZombieButton->SetLabel("[VIEW_ZOMBIES]");
	mZombieButton->Resize(487, 345, 210, 48);
	mZombieButton->mDrawStoneButton = true;
	mZombieButton->mParentWidget = this;

	SetPage(ALMANAC_PAGE_INDEX);
	if (!mApp->mBoard || !mApp->mBoard->mPaused)
		mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
}

AlmanacDialog::~AlmanacDialog()
{
	ClearPlantsAndZombies();
}

void AlmanacDialog::ClearPlantsAndZombies()
{
	if (mPlant)
	{
		mPlant->Die();
		mPlant.reset();
	}
	if (mZombie)
	{
		mZombie->DieNoLoot();
		mZombie.reset();
	}
	for (std::unique_ptr<Zombie>& aZombie : mZombiePerfTest)
	{
		if (aZombie)
		{
			aZombie->DieNoLoot();
			aZombie.reset();
		}
	}
}

void AlmanacDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	ClearPlantsAndZombies();
}

void AlmanacDialog::SetupPlant()
{
    AlmanacRefreshPreviews(this);
}

void AlmanacDialog::SetupZombie()
{
    AlmanacRefreshPreviews(this);
}

void AlmanacDialog::SetPage(AlmanacPage thePage)
{
	mOpenPage = thePage;

	if (mOpenPage == AlmanacPage::ALMANAC_PAGE_INDEX)
	{
        AlmanacRefreshPreviews(this);

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

void AlmanacDialog::Update()
{
	mCloseButton->Update();
	mIndexButton->Update();
	mPlantButton->Update();
	mZombieButton->Update();
    AlmanacRefreshPreviews(this);
	if (mPlant) mPlant->Update();
	if (mZombie) mZombie->Update();
	for (const std::unique_ptr<Zombie>& aZombie : mZombiePerfTest)
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

void AlmanacDialog::DrawIndex(Graphics* g)
{
	g->DrawImage(Sexy::IMAGE_ALMANAC_INDEXBACK, 0, 0);
	PvzpDrawString(g, "[SUBURBAN_ALMANAC_INDEX]", BOARD_WIDTH / 2, 60, Sexy::FONT_HOUSEOFTERROR28, Color(220, 220, 220), DrawStringJustification::DS_ALIGN_CENTER);

	if (mPlant && mPlant->mSeedType == SeedType::SEED_SUNFLOWER)
	{
		Graphics aPlantGraphics = Graphics(*g);
		mPlant->BeginDraw(&aPlantGraphics);
		mPlant->Draw(&aPlantGraphics);
	}
	if (mZombie && mZombie->mZombieType == ZombieType::ZOMBIE_NORMAL)
	{
		Graphics aZombieGraphics = Graphics(*g);
		mZombie->BeginDraw(&aZombieGraphics);
		mZombie->Draw(&aZombieGraphics);
	}
}

void AlmanacDialog::DrawPlants(Graphics* g)
{
	g->DrawImage(Sexy::IMAGE_ALMANAC_PLANTBACK, 0, 0);
	PvzpDrawString(g, "[SUBURBAN_ALMANAC_PLANTS]", BOARD_WIDTH / 2, 48, Sexy::FONT_HOUSEOFTERROR20, Color(213, 159, 43), DrawStringJustification::DS_ALIGN_CENTER);

	SeedType aSeedMouseOn = SeedHitTest(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY);
    // @pvz-online: encyclopedia discovery is independent of battle plant ownership.
    for(int seed=SEED_PEASHOOTER;seed<NUM_ALMANAC_SEEDS;++seed) {
        auto type=(SeedType)seed;int x,y;GetSeedPosition(type,x,y);
        if(type==SEED_IMITATER)g->DrawImage(Sexy::IMAGE_ALMANAC_IMITATER,x,y);
        else {
            DrawSeedPacket(g,x,y,type,SEED_NONE,0,255,true,false);
            if(type==aSeedMouseOn)g->DrawImage(Sexy::IMAGE_SEEDPACKETFLASH,x,y);
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

	if (mPlant && mPlant->mSeedType == mSelectedSeed)
	{
		Graphics aPlantGraphics = Graphics(*g);
		mPlant->BeginDraw(&aPlantGraphics);
		mPlant->Draw(&aPlantGraphics);
	}

	g->DrawImage(Sexy::IMAGE_ALMANAC_PLANTCARD, 459, 86);
	const PlantDefinition& aPlantDef = GetPlantDefinition(mSelectedSeed);
	std::string aName = Plant::GetNameString(mSelectedSeed, SEED_NONE);
	std::string aDescriptionName = std::format("[{}_DESCRIPTION]", aPlantDef.mPlantName);
	PvzpDrawString(g, aName, 617, 288, Sexy::FONT_DWARVENTODCRAFT18YELLOW, Color::White, DS_ALIGN_CENTER);

    auto entries = MobileAlmanacEntries(mSelectedSeed);
    std::string description(PvzpStringTranslate(aDescriptionName));
    for (size_t begin = description.find('{'); begin != std::string::npos; begin = description.find('{')) {
        size_t end = description.find('}', begin);
        if (end == std::string::npos) break;
        description.erase(begin, end - begin + 1);
    }
    auto introLines = RunInfoView::Wrap(description, 258, 18);
    std::vector<std::vector<std::string>> pages;
    std::vector<std::string> summary;
    for (int index = 0; index < std::min(3, static_cast<int>(introLines.size())); ++index)
        summary.push_back(introLines[index] + (index == 2 && introLines.size() > 3 ? "..." : ""));
    summary.push_back(ModText::Tr("可获得词条（非本局）", "Available boosts (not owned)"));
    if (entries.empty()) summary.push_back(ModText::Tr("暂无可获得词条", "No available boosts"));
    else for (int index = 0; index < std::min(3, static_cast<int>(entries.size())); ++index)
        summary.push_back(GetRunChoiceDisplayName(entries[index]));
    summary.push_back(ModText::Tr("翻页查看完整说明与词条", "Next: full info and boosts"));
    pages.push_back(summary);
    auto appendPages = [&](const std::string& title, const std::vector<std::string>& lines) {
        for (int first = 0; first < std::max(1, static_cast<int>(lines.size())); first += 7) {
            std::vector<std::string> page{title};
            for (int index = first; index < std::min(first + 7, static_cast<int>(lines.size())); ++index)
                page.push_back(lines[index]);
            pages.push_back(page);
        }
    };
    appendPages(ModText::Tr("植物说明", "Plant information"), introLines);
    for (int id : entries) {
        std::string title = GetRunChoiceDisplayName(id);
        for (int star = 0; star < GetRunChoiceRarity(id); ++star) title += " ★";
        appendPages(title, RunInfoView::Wrap(GetRunChoiceDesc(id), 258, 18));
    }
    mRunEntryPageCount = static_cast<int>(pages.size());
    mRunEntryPage = std::clamp(mRunEntryPage, 0, mRunEntryPageCount - 1);
    const auto& lines = pages[mRunEntryPage];
    for (int index = 0; index < static_cast<int>(lines.size()); ++index)
        PvzpDrawStringWrapped(g, lines[index], Rect(485, 309 + index * 24, 258, 24),
            Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), DS_ALIGN_LEFT);
    DrawMobileStone(g, AlmanacEntryArrow(false), ModText::Tr("上一页", "Prev"), mRunEntryDown == -1, mRunEntryPage > 0);
    DrawMobileStone(g, AlmanacEntryArrow(true), ModText::Tr("下一页", "Next"), mRunEntryDown == 1, mRunEntryPage + 1 < mRunEntryPageCount);
    SeatUi::Text(g, std::to_string(mRunEntryPage + 1) + "/" + std::to_string(mRunEntryPageCount), 322, 581, 230, 20, Color(40, 50, 90), true);

	if (mSelectedSeed != SeedType::SEED_IMITATER)
	{
		std::string aCostStr = PvzpReplaceString(std::format("{{KEYWORD}}{{COST}}:{{STAT}} {}", aPlantDef.mSeedCost), "{COST}", "[COST]");
		PvzpDrawStringWrapped(g, aCostStr, Rect(485, 510, 134, 38), Sexy::FONT_BRIANNETOD12, Color(40,50,90), DS_ALIGN_LEFT);

		std::string aRechargeStr = PvzpReplaceString(
			"{KEYWORD}{WAIT_TIME}: {STAT}{WAIT_TIME_LENGTH}",
			"{WAIT_TIME_LENGTH}",
			aPlantDef.mRefreshTime == 750 ? "[WAIT_TIME_SHORT]" : aPlantDef.mRefreshTime == 3000 ? "[WAIT_TIME_LONG]" : "[WAIT_TIME_VERY_LONG]"
		);
		aRechargeStr = PvzpReplaceString(aRechargeStr, "{WAIT_TIME}", "[WAIT_TIME]");
		PvzpDrawStringWrapped(g, aRechargeStr, Rect(600, 510, 139, 38), Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), DS_ALIGN_RIGHT);
	}
}

void AlmanacDialog::DrawZombies(Graphics* g)
{
	g->DrawImage(Sexy::IMAGE_ALMANAC_ZOMBIEBACK, 0, 0);
	PvzpDrawString(g, "[SUBURBAN_ALMANAC_ZOMBIES]", BOARD_WIDTH / 2, 54, Sexy::FONT_DWARVENTODCRAFT24, Color(0, 196, 0), DS_ALIGN_CENTER);

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
				switch (aZombieType)
				{
				case ZombieType::ZOMBIE_POLEVAULTER:
					aZombieGraphics.TranslateF(2, -3);
					aZombieTypeToDraw = ZombieType::ZOMBIE_CACHED_POLEVAULTER_WITH_POLE;		break;
				case ZombieType::ZOMBIE_FLAG:			aZombieGraphics.TranslateF(2, 10);	break;
				case ZombieType::ZOMBIE_TRAFFIC_CONE:	aZombieGraphics.TranslateF(0, 12);	break;
				case ZombieType::ZOMBIE_PAIL:			aZombieGraphics.TranslateF(0, 9);		break;
				case ZombieType::ZOMBIE_FOOTBALL:		aZombieGraphics.TranslateF(-15, -1);	break;
				case ZombieType::ZOMBIE_ZAMBONI:		aZombieGraphics.TranslateF(0, 3);		break;
				case ZombieType::ZOMBIE_DOLPHIN_RIDER:	aZombieGraphics.TranslateF(-2, -10);	break;
				case ZombieType::ZOMBIE_POGO:			aZombieGraphics.TranslateF(0, -3);	break;
				case ZombieType::ZOMBIE_GARGANTUAR:		aZombieGraphics.TranslateF(15, 17);	break;
				case ZombieType::ZOMBIE_IMP:			aZombieGraphics.TranslateF(-8, -7);	break;
				case ZombieType::ZOMBIE_BUNGEE:			aZombieGraphics.TranslateF(-4, 3);	break;
				case ZombieType::ZOMBIE_DANCER:			aZombieGraphics.TranslateF(0, 15);	break;
				case ZombieType::ZOMBIE_BACKUP_DANCER:	aZombieGraphics.TranslateF(-4, 20);	break;
				case ZombieType::ZOMBIE_SNORKEL:		aZombieGraphics.TranslateF(-10, 0);	break;
				case ZombieType::ZOMBIE_YETI:			aZombieGraphics.TranslateF(0, 4);		break;
				case ZombieType::ZOMBIE_CATAPULT:		aZombieGraphics.TranslateF(-24, -1);	break;
				case ZombieType::ZOMBIE_BOBSLED:		aZombieGraphics.TranslateF(0, -8);	break;
				case ZombieType::ZOMBIE_LADDER:			aZombieGraphics.TranslateF(0, -3);	break;
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

	g->DrawImage(mSelectedZombie == ZombieType::ZOMBIE_ZAMBONI || mSelectedZombie == ZombieType::ZOMBIE_BOBSLED ?
		Sexy::IMAGE_ALMANAC_GROUNDICE : Sexy::IMAGE_ALMANAC_GROUNDDAY, 518, 110);
	if (mZombie && mZombie->mZombieType == mSelectedZombie && !ZombieHasSilhouette(mZombie->mZombieType))
	{
		Graphics aZombieGraphics = Graphics(*g);
		mZombie->BeginDraw(&aZombieGraphics);
		aZombieGraphics.SetClipRect(-42, -51, 197, 187);
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

	const ZombieDefinition& aZombieDef = GetZombieDefinition(mSelectedZombie);
	std::string aName = ZombieHasSilhouette(mSelectedZombie) ? "???" : std::format("[{}]", aZombieDef.mZombieName);
	PvzpDrawString(g, aName, 613, 362, Sexy::FONT_DWARVENTODCRAFT18GREENINSET, Color(190, 255, 235, 255), DS_ALIGN_CENTER);

	std::string aDescription;
	DrawStringJustification aAlign;
	if (ZombieHasDescription(mSelectedZombie))
	{
		aDescription = PvzpStringTranslate(std::format("[{}_DESCRIPTION]", aZombieDef.mZombieName));
		aAlign = DS_ALIGN_LEFT;
	}
	else
	{
		aDescription = "[NOT_ENCOUNTERED_YET]";
		aAlign = DS_ALIGN_CENTER_VERTICAL_MIDDLE;
	}
	for (PvzpStringListFormat& aFormat : gLawnStringFormats)
	{
		if (TestBit(aFormat.mFormatFlags, PvzpStringFormatFlag::PVZP_FORMAT_HIDE_UNTIL_MAGNETSHROOM))
		{
			if (mApp->HasSeedType(SeedType::SEED_MAGNETSHROOM))
			{
				aFormat.mNewColor.mAlpha = 255;
				aFormat.mLineSpacingOffset = 0;
			}
			else
			{
				aFormat.mNewColor.mAlpha = 0;
				aFormat.mLineSpacingOffset = -17;
			}
		}
	}
	// todo: fix stuff that have another formatter after them, ex: "{KEYWORD}Weakness:{STAT} fume-shroom{METAL} and magnet-shroom{KEYWORD}" (magnet-shroom will show with the {KEYWORD} colors)
	PvzpDrawStringWrapped(g, aDescription, Rect(484, mSelectedZombie == ZombieType::ZOMBIE_ZAMBONI ? 372 : 377, 258, 170), Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), aAlign);
}

void AlmanacDialog::Draw(Graphics* g)
{
	g->SetLinearBlend(true);
	switch (mOpenPage)
	{
	case AlmanacPage::ALMANAC_PAGE_INDEX:	DrawIndex(g);	break;
	case AlmanacPage::ALMANAC_PAGE_PLANTS:	DrawPlants(g);	break;
	case AlmanacPage::ALMANAC_PAGE_ZOMBIES:	DrawZombies(g);	break;
	}

	for (const std::unique_ptr<Zombie>& aZombie : mZombiePerfTest)
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

SeedType AlmanacDialog::SeedHitTest(int x,int y)
{
    if(mMouseVisible && mOpenPage==ALMANAC_PAGE_PLANTS)
        for(int seed=SEED_PEASHOOTER;seed<NUM_ALMANAC_SEEDS;++seed) {
            auto type=(SeedType)seed;int sx,sy;GetSeedPosition(type,sx,sy);
            Rect box=type==SEED_IMITATER?Rect(sx,sy,34,46):Rect(sx,sy,SEED_PACKET_WIDTH,SEED_PACKET_HEIGHT);
            if(box.Contains(x,y))return type;
        }
    return SEED_NONE;
}

bool AlmanacDialog::ZombieHasSilhouette(ZombieType theZombieType)
{
    return false; // @pvz-online: encyclopedia access does not depend on the campaign.
}

bool AlmanacDialog::ZombieIsShown(ZombieType theZombieType)
{
    return true;
}

bool AlmanacDialog::ZombieHasDescription(ZombieType theZombieType)
{
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

ZombieType AlmanacDialog::ZombieHitTest(int x, int y)
{
	if (mMouseVisible && mOpenPage == AlmanacPage::ALMANAC_PAGE_ZOMBIES)
	{
		for (int i = 0; i < NUM_ALMANAC_ZOMBIES; i++)
		{
			ZombieType aZombieType = GetZombieType(i);
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

void AlmanacDialog::LostFocus()
{
    mRunEntryDown = 0;
    Dialog::LostFocus();
}

void AlmanacDialog::MouseUp(int x, int y, int theClickCount)
{
    int direction = mRunEntryDown;
    mRunEntryDown = 0;
    if (direction) {
        if (theClickCount > 0 && mOpenPage == ALMANAC_PAGE_PLANTS &&
            AlmanacEntryArrow(direction > 0).Contains(x, y))
            mRunEntryPage = std::clamp(mRunEntryPage + direction, 0, mRunEntryPageCount - 1);
        MarkDirty();
        return;
    }
	if (mPlantButton->IsMouseOver())		SetPage(ALMANAC_PAGE_PLANTS);
	else if (mZombieButton->IsMouseOver())	SetPage(ALMANAC_PAGE_ZOMBIES);
	else if (mCloseButton->IsMouseOver())	mApp->KillAlmanacDialog();
	else if (mIndexButton->IsMouseOver())	SetPage(ALMANAC_PAGE_INDEX);
}

void AlmanacDialog::MouseDown(int x, int y, [[maybe_unused]] int theClickCount)
{
    mRunEntryDown = 0;
    if (theClickCount > 0 && mOpenPage == ALMANAC_PAGE_PLANTS) {
        if (AlmanacEntryArrow(false).Contains(x, y) && mRunEntryPage > 0) mRunEntryDown = -1;
        if (AlmanacEntryArrow(true).Contains(x, y) && mRunEntryPage + 1 < mRunEntryPageCount) mRunEntryDown = 1;
        if (mRunEntryDown) { MarkDirty(); return; }
    }

	if (mPlantButton->IsMouseOver() || mCloseButton->IsMouseOver() || mIndexButton->IsMouseOver())
		mApp->PlaySample(Sexy::SOUND_TAP);
	if (mZombieButton->IsMouseOver())
		mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);

	SeedType aSeedType = SeedHitTest(x, y);
	if (aSeedType != SeedType::SEED_NONE && aSeedType != mSelectedSeed)
	{
		mSelectedSeed = aSeedType;
        mRunEntryPage=0;
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

void AlmanacDialog::KeyDown(KeyCode theKey)
{
	if (theKey == KeyCode::KEYCODE_ESCAPE)
	{
		if (mOpenPage == AlmanacPage::ALMANAC_PAGE_INDEX)
			mApp->KillAlmanacDialog();
		else
			SetPage(AlmanacPage::ALMANAC_PAGE_INDEX);
		return;
	}

	LawnDialog::KeyDown(theKey);
}

void AlmanacInitForPlayer()
{
	for (int i = 0; i < ZombieType::NUM_ZOMBIE_TYPES; i++)
		gZombieDefeated[i] = false;
}

void AlmanacPlayerDefeatedZombie(ZombieType theZombieType)
{
	gZombieDefeated[theZombieType] = true;
}
