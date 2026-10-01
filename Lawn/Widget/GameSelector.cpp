#include "../Board.h"
#include "GameButton.h"
#include "StoreScreen.h"
#include "../ZenGarden.h"
#include "GameSelector.h"
#include "../../LawnApp.h"
#include "Almanac.h"
#include "../../Resources.h"
#include "../System/Music.h"
#include "../ToolTipWidget.h"
#include "../System/SaveGame.h"
#include "../../GameConstants.h"
#include "../System/PlayerInfo.h"
#include "../System/ProfileMgr.h"
#include "../System/TypingCheck.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Sexy.TodLib/TodDebug.h"
#include "graphics/Font.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "widget/Dialog.h"
#include "widget/WidgetManager.h"
#include <climits>
#include <cstdlib>
#include <cstring>

static float gFlowerCenter[3][2] = { { 765.0f, 483.0f }, { 663.0f, 455.0f }, { 701.0f, 439.0f } };  //0x665430

// @pvz-online: upstream parks the selector at x=-800 and never rolls it in, so the
// menu is drawn entirely off-screen (the "stuck at Click to Start" bug). The roll-in
// cannot simply be re-enabled: Sexy's dirty-rect tracking never erases the pixels a
// widget vacates while it is still off-screen, so every one of the 75 slide frames
// leaves a permanent ghost of the menu (verified by A/B screenshot). Until the render
// path is reworked the selector is parked where it belongs instead. PVZ_SELECTOR_X
// overrides the parking x, PVZ_SELECTOR_BGDX the Draw() background shift.
static bool sDumped = false;  // @pvz-online debug: PVZ_DUMP one-shot guard
static int sDumpFrame = 0;    // @pvz-online debug: frame counter for the PVZ_DUMP delay

static int EnvInt(const char* theName, int theDefault)
{
	const char* aEnv = getenv(theName);
	return (aEnv != nullptr) ? atoi(aEnv) : theDefault;
}

static int SelectorParkedX()
{
	static int sX = INT_MIN;
	if (sX == INT_MIN)
		sX = EnvInt("PVZ_SELECTOR_X", 0);
	return sX;
}

// The background shift must equal -SelectorParkedX() to keep the reanim art where it
// is authored while the child widgets move with the widget. Parking at 0 means no
// shift at all.
static int SelectorBGOffsetX()
{
	static int sDX = INT_MIN;
	if (sDX == INT_MIN)
		sDX = EnvInt("PVZ_SELECTOR_BGDX", -SelectorParkedX());
	return sDX;
}

//0x448C80
void GameSelectorOverlay::Draw(Graphics* g)
{ 
	mParent->DrawOverlay(g);
}

GameSelectorOverlay::GameSelectorOverlay(GameSelector* theGameSelector)
{
	mParent = theGameSelector;
	mMouseVisible = false;
	mHasAlpha = true;
}

//0x448CB0
// GOTY @Patoke: 0x44B8D0
GameSelector::GameSelector(LawnApp* theApp)
{
	TodHesitationTrace("pregameselector");
	TodLoadResources("DelayLoad_Zombatar");

	mApp = theApp;
	mLevel = 1;
	mLoading = false;
	mHasTrophy = false;
	mToolTip = new ToolTipWidget();

	mAdventureButton = MakeNewButton(
		GameSelector::GameSelector_Adventure, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_ADVENTURE_BUTTON, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_ADVENTURE_HIGHLIGHT, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_ADVENTURE_HIGHLIGHT
	);
	
	mAdventureButton->Resize(0, 0, Sexy::IMAGE_REANIM_SELECTORSCREEN_ADVENTURE_BUTTON->mWidth, 125);
	mAdventureButton->mClip = false;
	mAdventureButton->mBtnNoDraw = true;
	mAdventureButton->mMouseVisible = false;
	mAdventureButton->mPolygonShape[0] = SexyVector2(7.0f, 1.0f);
	mAdventureButton->mPolygonShape[1] = SexyVector2(328.0f, 30.0f);
	mAdventureButton->mPolygonShape[2] = SexyVector2(314.0f, 125.0f);
	mAdventureButton->mPolygonShape[3] = SexyVector2(1.0f, 78.0f);
	mAdventureButton->mUsePolygonShape = true;

	mMinigameButton = MakeNewButton(
		GameSelector::GameSelector_Minigame, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_BUTTON, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_HIGHLIGHT, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_HIGHLIGHT
	);
	mMinigameButton->Resize(0, 0, Sexy::IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_BUTTON->mWidth, 130);
	mMinigameButton->mClip = false;
	mMinigameButton->mBtnNoDraw = true;
	mMinigameButton->mMouseVisible = false;
	mMinigameButton->mPolygonShape[0] = SexyVector2(4.0f, 2.0f);
	mMinigameButton->mPolygonShape[1] = SexyVector2(312.0f, 51.0f);
	mMinigameButton->mPolygonShape[2] = SexyVector2(296.0f, 130.0f);
	mMinigameButton->mPolygonShape[3] = SexyVector2(7.0f, 77.0f);
	mMinigameButton->mUsePolygonShape = true;

	mPuzzleButton = MakeNewButton(
		GameSelector::GameSelector_Puzzle, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_BUTTON, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_HIGHLIGHT, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_HIGHLIGHT
	);
	mPuzzleButton->Resize(0, 0, Sexy::IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_BUTTON->mWidth, 121);
	mPuzzleButton->mClip = false;
	mPuzzleButton->mBtnNoDraw = true;
	mPuzzleButton->mMouseVisible = false;
	mPuzzleButton->mPolygonShape[0] = SexyVector2(2.0f, 0.0f);
	mPuzzleButton->mPolygonShape[1] = SexyVector2(281.0f, 55.0f);
	mPuzzleButton->mPolygonShape[2] = SexyVector2(268.0f, 121.0f);
	mPuzzleButton->mPolygonShape[3] = SexyVector2(3.0f, 60.0f);
	mPuzzleButton->mUsePolygonShape = true;

	mSurvivalButton = MakeNewButton(
		GameSelector::GameSelector_Survival,
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_BUTTON, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_HIGHLIGHT, 
		Sexy::IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_HIGHLIGHT
	);
	mSurvivalButton->Resize(0, 0, Sexy::IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_BUTTON->mWidth, 124);
	mSurvivalButton->mClip = false;
	mSurvivalButton->mBtnNoDraw = true;
	mSurvivalButton->mMouseVisible = false;
	mSurvivalButton->mPolygonShape[0] = SexyVector2(7.0f, 1.0f);
	mSurvivalButton->mPolygonShape[1] = SexyVector2(267.0f, 62.0f);
	mSurvivalButton->mPolygonShape[2] = SexyVector2(257.0f, 124.0f);
	mSurvivalButton->mPolygonShape[3] = SexyVector2(7.0f, 57.0f);
	mSurvivalButton->mUsePolygonShape = true;

	// @Patoke: add these button defs
	mZombatarButton = MakeNewButton(
		GameSelector::GameSelector_Zombatar,
		this,
		"",
		nullptr,
		Sexy::IMAGE_BLANK,
		Sexy::IMAGE_BLANK,
		Sexy::IMAGE_BLANK
	);
	mZombatarButton->Resize(0, 0, Sexy::IMAGE_REANIM_SELECTORSCREEN_WOODSIGN3_PRESS->mWidth, Sexy::IMAGE_REANIM_SELECTORSCREEN_WOODSIGN3_PRESS->mHeight);
	mZombatarButton->mClip = false;
	mZombatarButton->mBtnNoDraw = true;
	mZombatarButton->mMouseVisible = false;

	mAchievementsButton = MakeNewButton(
		GameSelector::GameSelector_Achievements,
		this,
		"",
		nullptr,
		Sexy::IMAGE_SELECTORSCREEN_ACHIEVEMENTS_PEDESTAL,
		Sexy::IMAGE_SELECTORSCREEN_ACHIEVEMENTS_PEDESTAL_PRESS,
		Sexy::IMAGE_SELECTORSCREEN_ACHIEVEMENTS_PEDESTAL_PRESS
	);
	mAchievementsButton->Resize(20, mApp->mHeight - Sexy::IMAGE_SELECTORSCREEN_ACHIEVEMENTS_PEDESTAL->mHeight - 35, Sexy::IMAGE_SELECTORSCREEN_ACHIEVEMENTS_PEDESTAL->mWidth, Sexy::IMAGE_SELECTORSCREEN_ACHIEVEMENTS_PEDESTAL->mHeight);
	mAchievementsButton->mClip = false;
	mAchievementsButton->mBtnNoDraw = false;
	mAchievementsButton->mMouseVisible = false;

	mZenGardenButton = MakeNewButton(
		GameSelector::GameSelector_ZenGarden, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_SELECTORSCREEN_ZENGARDEN, 
		Sexy::IMAGE_SELECTORSCREEN_ZENGARDENHIGHLIGHT, 
		Sexy::IMAGE_SELECTORSCREEN_ZENGARDENHIGHLIGHT
	);
	mZenGardenButton->Resize(0, 0, 130, 130);
	mZenGardenButton->mMouseVisible = false;
	mZenGardenButton->mClip = false;

	mOptionsButton = MakeNewButton(
		GameSelector::GameSelector_Options, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_SELECTORSCREEN_OPTIONS1, 
		Sexy::IMAGE_SELECTORSCREEN_OPTIONS2, 
		Sexy::IMAGE_SELECTORSCREEN_OPTIONS2
	);
	mOptionsButton->Resize(0, 0, Sexy::IMAGE_SELECTORSCREEN_OPTIONS1->mWidth, Sexy::IMAGE_SELECTORSCREEN_OPTIONS1->mHeight + 23);
	mOptionsButton->mClip = false; // @Patoke: not in original but fixes stuff
	mOptionsButton->mBtnNoDraw = true;
	mOptionsButton->mMouseVisible = false;
	mOptionsButton->mButtonOffsetY = 15;

	mHelpButton = MakeNewButton(
		GameSelector::GameSelector_Help, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_SELECTORSCREEN_HELP1, 
		Sexy::IMAGE_SELECTORSCREEN_HELP2, 
		Sexy::IMAGE_SELECTORSCREEN_HELP2
	);
	mHelpButton->Resize(0, 0, Sexy::IMAGE_SELECTORSCREEN_HELP1->mWidth, Sexy::IMAGE_SELECTORSCREEN_HELP1->mHeight + 33);
	mHelpButton->mClip = false; // @Patoke: not in original but fixes stuff
	mHelpButton->mBtnNoDraw = true;
	mHelpButton->mMouseVisible = false;
	mHelpButton->mButtonOffsetY = 30;

	mQuitButton = MakeNewButton(
		GameSelector::GameSelector_Quit, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_SELECTORSCREEN_QUIT1, 
		Sexy::IMAGE_SELECTORSCREEN_QUIT2, 
		Sexy::IMAGE_SELECTORSCREEN_QUIT2
	);
	mQuitButton->Resize(0, 0, Sexy::IMAGE_SELECTORSCREEN_QUIT1->mWidth + 10, Sexy::IMAGE_SELECTORSCREEN_QUIT1->mHeight + 10);
	mQuitButton->mClip = false; // @Patoke: not in original but fixes stuff
	mQuitButton->mBtnNoDraw = true;
	mQuitButton->mMouseVisible = false;
	mQuitButton->mButtonOffsetX = 5;
	mQuitButton->mButtonOffsetY = 5;

	mChangeUserButton = MakeNewButton(
		GameSelector::GameSelector_ChangeUser, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_BLANK, 
		Sexy::IMAGE_BLANK, 
		Sexy::IMAGE_BLANK
	);
	mChangeUserButton->Resize(0, 0, 250, 30);
	mChangeUserButton->mBtnNoDraw = true;
	mChangeUserButton->mMouseVisible = false;

	mOverlayWidget = new GameSelectorOverlay(this);
	mOverlayWidget->Resize(0, 0, BOARD_WIDTH, BOARD_HEIGHT);

	mStoreButton = MakeNewButton(
		GameSelector::GameSelector_Store, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_SELECTORSCREEN_STORE, 
		Sexy::IMAGE_SELECTORSCREEN_STOREHIGHLIGHT, 
		Sexy::IMAGE_SELECTORSCREEN_STOREHIGHLIGHT
	);
	mStoreButton->Resize(405, 484, Sexy::IMAGE_SELECTORSCREEN_STORE->mWidth, Sexy::IMAGE_SELECTORSCREEN_STORE->mHeight);
	mStoreButton->mClip = false; // @Patoke: not in original but fixes stuff
	mStoreButton->mMouseVisible = false;
	
	mAlmanacButton = MakeNewButton(
		GameSelector::GameSelector_Almanac, 
		this, 
		"", 
		nullptr, 
		Sexy::IMAGE_SELECTORSCREEN_ALMANAC, 
		Sexy::IMAGE_SELECTORSCREEN_ALMANACHIGHLIGHT, 
		Sexy::IMAGE_SELECTORSCREEN_ALMANACHIGHLIGHT
	);
	mAlmanacButton->Resize(327, 428, Sexy::IMAGE_SELECTORSCREEN_ALMANAC->mWidth, Sexy::IMAGE_SELECTORSCREEN_ALMANAC->mHeight);
	mAlmanacButton->mClip = false; // @Patoke: not in original but fixes stuff
	mAlmanacButton->mMouseVisible = false;

	mQuickPlayButton = MakeNewButton(
		GameSelector::GameSelector_QuickPlay,
		this,
		"",
		nullptr,
		Sexy::IMAGE_QUICKPLAY_BACK_BUTTON,
		Sexy::IMAGE_QUICKPLAY_BACK_BUTTON_HIGHLIGHT,
		Sexy::IMAGE_QUICKPLAY_BACK_BUTTON_HIGHLIGHT
	);
	mQuickPlayButton->Resize(mApp->mWidth - 150, 455, Sexy::IMAGE_QUICKPLAY_BACK_BUTTON->mWidth, Sexy::IMAGE_QUICKPLAY_BACK_BUTTON->mHeight);

	mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME);

	mStartingGame = false;
	mStartingGameCounter = 0;
	mMinigamesLocked = false;
	mPuzzleLocked = false;
	mSurvivalLocked = false;
	mUnlockSelectorCheat = false;
	mTrophyParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mShowStartButton = false;

	Reanimation* aSelectorReanim = mApp->AddReanimation(0.5f, 0.5f, 0, ReanimationType::REANIM_SELECTOR_SCREEN);
	aSelectorReanim->PlayReanim("anim_open", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 30.0f);
	aSelectorReanim->AssignRenderGroupToPrefix("flower", RENDER_GROUP_HIDDEN);
	aSelectorReanim->AssignRenderGroupToPrefix("leaf", RENDER_GROUP_HIDDEN);
	aSelectorReanim->AssignRenderGroupToTrack("SelectorScreen_BG", 1);
	mSelectorReanimID = mApp->ReanimationGetID(aSelectorReanim);
	mSelectorState = SelectorAnimState::SELECTOR_OPEN;
	int aFrameStart, aFrameCount;
	aSelectorReanim->GetFramesForLayer("anim_sign", aFrameStart, aFrameCount);
	aSelectorReanim->mFrameBasePose = aFrameStart + aFrameCount - 1;

	for (int i = 0; i < 6; i++)
	{
		Reanimation* aCloudReanim = mApp->AddReanimation(0.5f, 0.5f, 0, ReanimationType::REANIM_SELECTOR_SCREEN);
		std::string aAnimName = Sexy::StrFormat("anim_cloud%d", (i > 1 ? i + 2 : i + 1));
		aCloudReanim->PlayReanim(aAnimName.c_str(), ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 0.0f);
		mCloudReanimID[i] = mApp->ReanimationGetID(aCloudReanim);
		mCloudCounter[i] = RandRangeInt(-6000, 2000);
		if (mCloudCounter[i] < 0)
		{
			aCloudReanim->mAnimTime = -mCloudCounter[i] / 6000.0f;
			aCloudReanim->mAnimRate = 0.5f;
			mCloudCounter[i] = 0;
		}
		else
			aCloudReanim->mAnimRate = 0.0f;
	}

	for (int i = 0; i < 3; i++)
	{
		Reanimation* aFlowerReanim = mApp->AddReanimation(0.5f, 0.5f, 0, ReanimationType::REANIM_SELECTOR_SCREEN);
		std::string aAnimName = Sexy::StrFormat("anim_flower%d", i + 1);
		aFlowerReanim->PlayReanim(aAnimName.c_str(), ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 0.0f);
		aFlowerReanim->mAnimRate = 0.0f;
		aFlowerReanim->AttachToAnotherReanimation(aSelectorReanim, "SelectorScreen_BG_Right");
		aFlowerReanim->mIsAttachment = false;
		mFlowerReanimID[i] = mApp->ReanimationGetID(aFlowerReanim);
	}

	Reanimation* aLeafReanim = mApp->AddReanimation(0.5f, 0.5f, 0, ReanimationType::REANIM_SELECTOR_SCREEN);
	aLeafReanim->PlayReanim("anim_grass", ReanimLoopType::REANIM_LOOP, 0, 6.0f);
	aLeafReanim->mAnimRate = 0.0f;
	mLeafReanimID = mApp->ReanimationGetID(aLeafReanim);
	mLeafCounter = 50;

	SyncProfile(false);
	mApp->PlaySample(Sexy::SOUND_ROLL_IN);

	// @Patoke: add new var init
	mSlideCounter = 0;
	mStartX = 0;
	mStartY = 0;
	mDestX = 0;
	mDestY = 0;
	mMenuHiddenForAchievements = false;
	// @pvz-online: the achievements page is a TOP-LEVEL manager widget that rides the
	// slide one screen below the menu (absolute y = mHeight at rest). It must not be a
	// child: (a) WidgetContainer clips children to the container's 800x600 local rect,
	// so a child parked at y=mHeight never renders; (b) WidgetManager::MouseUp converts
	// cursor coords with a single `x - widget.mX` level, which is wrong for any widget
	// nested inside the scrolled selector - clicks on the page (and on every menu button
	// while the selector is off y=0) landed hundreds of pixels away from their targets.
	// Absolute positioning keeps single-level math correct.
	//mZombatarWidget = new ZombatarWidget(this);
	//mZombatarWidget->Resize(800, 0, mApp->mWidth, mApp->mHeight);
	mAchievementsWidget = new AchievementsWidget(this->mApp);
	mAchievementsWidget->Move(0, mApp->mHeight);

	this->AddWidget(mAdventureButton);
	this->AddWidget(mMinigameButton);
	this->AddWidget(mPuzzleButton);
	this->AddWidget(mOptionsButton);
	this->AddWidget(mQuitButton);
	this->AddWidget(mHelpButton);
	this->AddWidget(mStoreButton);
	this->AddWidget(mAlmanacButton);
	this->AddWidget(mSurvivalButton);
	this->AddWidget(mZenGardenButton);
	this->AddWidget(mChangeUserButton);
	this->AddWidget(mZombatarButton); // @Patoke: add new widgets
	//this->AddWidget(mZombatarScreen); // @Patoke: add new widgets
	this->AddWidget(mAchievementsButton);
	// (mAchievementsWidget is added to the manager, not here - see the constructor note.)
	this->AddWidget(mQuickPlayButton);
	// @pvz-online: the BACK button belongs to the (still stubbed) Quick-Play screen. On the
	// real main menu the bottom-right corner shows the OPTIONS/HELP/QUIT vases (reanim art),
	// not a BACK sign - hide it until ShowQuickPlayScreen() exists and can own it.
	mQuickPlayButton->mVisible = false;
	// @pvz-online: M1 模式裁剪——主菜单只留「关卡(冒险) / 无尽(生存) / 图鉴 / 成就 / 换用户 /
	// 选项 / 帮助 / 退出」。小游戏、解谜、商店、禅境花园、僵尸工坊的入口整体下架。
	// 隐藏即不可点：WidgetContainer 只把鼠标事件派发给 mVisible 的子控件。
	// 美术：这五个按钮里小游戏/解谜/商店/禅境花园都用真图（IMAGE_*）画自己，隐藏
	// widget 即隐藏美术；僵尸工坊是例外——它用 IMAGE_BLANK，木牌由 reanim 的
	// woodsign3 轨道画，必须单独隐藏该轨道（woodsign2=换用户，保留）。
	mMinigameButton->mVisible = false;
	mPuzzleButton->mVisible = false;
	mStoreButton->mVisible = false;
	mZenGardenButton->mVisible = false;
	mZombatarButton->mVisible = false;
	aSelectorReanim->AssignRenderGroupToTrack("woodsign3", RENDER_GROUP_HIDDEN);
	this->AddWidget(mOverlayWidget);

	TodHesitationTrace("gameselectorinit");
}

//0x449D00、0x449D20
GameSelector::~GameSelector()
{
	// @pvz-online: every button below was handed to AddWidget() in the constructor, so
	// each one still holds mParent == this. Deleting a child without first removing it
	// trips WidgetContainer's "call RemoveWidget before you delete it!" assertion
	// (WidgetContainer.cpp:~32) and the game dies the moment the selector is torn down.
	// RemoveAllWidgets() detaches and deletes every child in one pass, clearing mParent.
	RemoveAllWidgets(true);

	// @pvz-online: the achievements page is a top-level manager widget (not a child), so
	// RemoveAllWidgets() above does not cover it. RemovedFromManager has detached it.
	delete mAchievementsWidget;

	// mToolTip is owned here but was never AddWidget()'d, so it is not covered above.
	delete mToolTip;
}

//0x449E60
// GOTY @Patoke: 0x44CDD0
void GameSelector::SyncButtons()
{
	bool aAlmanacAvailable = mApp->CanShowAlmanac() || mUnlockSelectorCheat;
	bool aStoreOpen = mApp->CanShowStore() || mUnlockSelectorCheat;
	bool aZenGardenOpen = mApp->CanShowZenGarden() || mUnlockSelectorCheat;

	mAlmanacButton->mDisabled = !aAlmanacAvailable;
	mAlmanacButton->mVisible = aAlmanacAvailable;
	// @pvz-online: M1 裁剪——商店 / 禅境花园 / 僵尸工坊入口下架，不随解锁状态再显示
	//（mDisabled 照旧跟随解锁状态，只影响已隐藏按钮的颜色，无副作用）
	mStoreButton->mDisabled = !aStoreOpen;
	mStoreButton->mVisible = false;
	mZombatarButton->mDisabled = false; // @Patoke: added these
	mZombatarButton->mVisible = false;

	Reanimation* aSelectorReanim = mApp->ReanimationGet(mSelectorReanimID);
	if (aAlmanacAvailable)
	{
		aSelectorReanim->AssignRenderGroupToPrefix("almanac_key_shadow", RENDER_GROUP_NORMAL);
		if (aStoreOpen)
			aSelectorReanim->SetImageOverride("almanac_key_shadow", nullptr);
		else
			aSelectorReanim->SetImageOverride("almanac_key_shadow", Sexy::IMAGE_REANIM_SELECTORSCREEN_ALMANAC_SHADOW);
	}
	else if (aStoreOpen)
	{
		aSelectorReanim->AssignRenderGroupToPrefix("almanac_key_shadow", RENDER_GROUP_NORMAL);
		aSelectorReanim->SetImageOverride("almanac_key_shadow", Sexy::IMAGE_REANIM_SELECTORSCREEN_KEY_SHADOW);
	}
	else
		aSelectorReanim->AssignRenderGroupToPrefix("almanac_key_shadow", RENDER_GROUP_HIDDEN);

	mZenGardenButton->mDisabled = !aZenGardenOpen;
	mZenGardenButton->mVisible = false; // @pvz-online: M1 裁剪——禅境花园入口下架

	// @Patoke: all of these are already assigned in the constructor, why assign them here? (this fixes the hover highlight)
	if (mMinigamesLocked)
	{
		//mMinigameButton->mOverImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_HIGHLIGHT;
		//mMinigameButton->mDownImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_HIGHLIGHT;
		mMinigameButton->SetColor(ButtonWidget::COLOR_BKG, Color(128, 128, 128));
	}
	else
	{
		//mMinigameButton->mOverImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_HIGHLIGHT;
		//mMinigameButton->mDownImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_HIGHLIGHT;
		mMinigameButton->SetColor(ButtonWidget::COLOR_BKG, Color::White);
	}
	if (mPuzzleLocked)
	{
		//mPuzzleButton->mOverImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_HIGHLIGHT;
		//mPuzzleButton->mDownImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_HIGHLIGHT;
		mPuzzleButton->SetColor(ButtonWidget::COLOR_BKG, Color(128, 128, 128));
	}
	else
	{
		//mPuzzleButton->mOverImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_HIGHLIGHT;
		//mPuzzleButton->mDownImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_HIGHLIGHT;
		mPuzzleButton->SetColor(ButtonWidget::COLOR_BKG, Color::White);
	}
	if (mSurvivalLocked)
	{
		//mSurvivalButton->mOverImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_HIGHLIGHT;
		//mSurvivalButton->mDownImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_HIGHLIGHT;
		mSurvivalButton->SetColor(ButtonWidget::COLOR_BKG, Color(128, 128, 128));
	}
	else
	{
		//mSurvivalButton->mOverImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_HIGHLIGHT;
		//mSurvivalButton->mDownImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_HIGHLIGHT;
		mSurvivalButton->SetColor(ButtonWidget::COLOR_BKG, Color::White);
	}

	ReanimatorTrackInstance* aMinigameTrack = aSelectorReanim->GetTrackInstanceByName("SelectorScreen_Survival_button");
	ReanimatorTrackInstance* aPuzzleTrack = aSelectorReanim->GetTrackInstanceByName("SelectorScreen_Challenges_button");
	ReanimatorTrackInstance* aSurvivalTrack = aSelectorReanim->GetTrackInstanceByName("SelectorScreen_ZenGarden_button");
	aMinigameTrack->mTrackColor = mMinigameButton->GetColor(ButtonWidget::COLOR_BKG);
	aPuzzleTrack->mTrackColor = mPuzzleButton->GetColor(ButtonWidget::COLOR_BKG);
	aSurvivalTrack->mTrackColor = mSurvivalButton->GetColor(ButtonWidget::COLOR_BKG);

	if (mShowStartButton)
	{
		aSelectorReanim->AssignRenderGroupToPrefix("SelectorScreen_Adventure_", RENDER_GROUP_HIDDEN);
		//mAdventureButton->mButtonImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_STARTADVENTURE_BUTTON;
		//mAdventureButton->mOverImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_STARTADVENTURE_HIGHLIGHT;
		//mAdventureButton->mDownImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_STARTADVENTURE_HIGHLIGHT;
	}
	else
	{
		aSelectorReanim->AssignRenderGroupToPrefix("SelectorScreen_StartAdventure_", RENDER_GROUP_HIDDEN);
		//mAdventureButton->mButtonImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_ADVENTURE_BUTTON;
		//mAdventureButton->mOverImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_ADVENTURE_HIGHLIGHT;
		//mAdventureButton->mDownImage = Sexy::IMAGE_REANIM_SELECTORSCREEN_ADVENTURE_HIGHLIGHT;
	}
}

//0x44A2E0
// GOTY @Patoke: 0x44D230
void GameSelector::AddTrophySparkle()
{
	TOD_ASSERT(mTrophyParticleID == PARTICLESYSTEMID_NULL);
	TodParticleSystem* aTrophyParticle = mApp->AddTodParticle(85.0f, 380.0f, RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_TROPHY_SPARKLE);
	mTrophyParticleID = mApp->ParticleGetID(aTrophyParticle);
}

//0x44A320
// GOTY @Patoke: 0x44D270
void GameSelector::SyncProfile(bool theShowLoading)
{
	if (theShowLoading)
	{
		mLoading = true;

		mApp->DoDialog(Dialogs::DIALOG_MESSAGE, true, _S("Loading..."), _S(""), _S(""), Dialog::BUTTONS_NONE);
		mApp->DrawDirtyStuff();
		mApp->PreloadForUser();
		mApp->KillDialog(Dialogs::DIALOG_MESSAGE);

		mLoading = false;
	}

	TodParticleSystem* aTrophyParticle = mApp->ParticleTryToGet(mTrophyParticleID);
	if (aTrophyParticle)
	{
		aTrophyParticle->ParticleSystemDie();
		mTrophyParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	}

	mLevel = 1;
	if (mApp->mPlayerInfo)
		mLevel = mApp->mPlayerInfo->GetLevel();
	mShowStartButton = true;
	mMinigamesLocked = true;
	mPuzzleLocked = true;
	mSurvivalLocked = true;
	if (mApp->mPlayerInfo && !mApp->IsIceDemo())
	{
		if (mLevel >= 2)
			mShowStartButton = false;

		if (mApp->HasFinishedAdventure())
		{
			mMinigamesLocked = false;
			mSurvivalLocked = false;
			mPuzzleLocked = false;
			mShowStartButton = false;
		}

		if (mApp->mPlayerInfo->mHasUnlockedMinigames)
			mMinigamesLocked = false;
		if (mApp->mPlayerInfo->mHasUnlockedPuzzleMode)
			mPuzzleLocked = false;
		if (mApp->mPlayerInfo->mHasUnlockedSurvivalMode)
			mSurvivalLocked = false;

		if (mApp->IsTrialStageLocked())
		{
			mPuzzleLocked = true;
			mSurvivalLocked = true;
		}
	}

	// @pvz-online: M1 裁剪——生存按钮现在是「无尽联机」的唯一入口，原版的解锁条件
	//（通关冒险 / 存档里的 mHasUnlockedSurvivalMode / 试玩锁）在裁剪后不成立，直接解锁。
	// 不这么做的话点它只会弹 [MODE_LOCKED]（ButtonDepress 会拦下）
	mSurvivalLocked = false;

	if (mApp->HasFinishedAdventure() && !mApp->IsTrialStageLocked())
		mHasTrophy = true;
	else
		mHasTrophy = false;
	if (mHasTrophy && mSelectorState != SelectorAnimState::SELECTOR_OPEN)
		AddTrophySparkle();

	SyncButtons();
	AlmanacInitForPlayer();
	BoardInitForPlayer();
	ReportAchievement::AchievementInitForPlayer(this); // @Patoke: add call
}

//0x44A650
// GOTY @Patoke: 0x44D700
void GameSelector::Draw(Graphics* g)
{
	// @pvz-online debug: is the selector screen actually being drawn?
	{
		static int sDrawCount = 0;
		if ((++sDrawCount % 60) == 1)
			TodTrace("GameSelector::Draw #%d state=%d pos=%d,%d size=%dx%d reanim=%d dialogs=%p/%p",
				sDrawCount, (int)mSelectorState, mX, mY, mWidth, mHeight, (int)mSelectorReanimID,
				(void*)mApp->GetDialog(Dialogs::DIALOG_STORE), (void*)mApp->GetDialog(Dialogs::DIALOG_ALMANAC));
	}

	// @pvz-online debug: PVZ_GEO=1 prints, once, the coordinate facts that decide where
	// the menu art lands: the app's logical size versus the widget manager's, and the
	// transform/clip the Graphics arrives with. Guessing these from screenshots was
	// what made every earlier layout measurement wrong.
	if (getenv("PVZ_GEO"))
	{
		static bool sGeoDumped = false;
		if (!sGeoDumped)
		{
			sGeoDumped = true;
			fprintf(stderr, "[geo] app=%dx%d widgetManager=%dx%d\n",
				mApp->mWidth, mApp->mHeight, mWidgetManager->mWidth, mWidgetManager->mHeight);
			fprintf(stderr, "[geo] g trans=(%.1f,%.1f) clip=(%d,%d,%d,%d) colorize=%d\n",
				g->mTransX, g->mTransY, g->mClipRect.mX, g->mClipRect.mY,
				g->mClipRect.mWidth, g->mClipRect.mHeight, (int)g->GetColorizeImages());
			fprintf(stderr, "[geo] selector mX=%d mY=%d %dx%d\n", mX, mY, mWidth, mHeight);
			fflush(stderr);
		}
	}

	if (mApp->GetDialog(Dialogs::DIALOG_STORE) || mApp->GetDialog(Dialogs::DIALOG_ALMANAC))
		return;

	// @Patoke: decided to manually inline these methods as i cannot be arsed to figure out what the name of the 2 functions called in here are
	// GOTY @Patoke: 0x44FD20
	g->SetLinearBlend(true);
	g->Translate(SelectorBGOffsetX(), 0);

	Reanimation* aSelectorReanim = mApp->ReanimationGet(mSelectorReanimID);
	aSelectorReanim->DrawRenderGroup(g, 1);  // "SelectorScreen_BG"
	for (int i = 0; i < 6; i++)
		mApp->ReanimationGet(mCloudReanimID[i])->Draw(g);
	aSelectorReanim->DrawRenderGroup(g, RENDER_GROUP_NORMAL);

	g->Translate(-SelectorBGOffsetX(), 0);
	// @pvz-online debug: PVZ_MARKER=1 lays a magenta ruler over the menu - a vertical
	// line every 100 client pixels, the one at x=400 made thick. Reading the menu off
	// the ruler tells a layout error apart from a coordinate error by eye.
	if (getenv("PVZ_MARKER"))
	{
		g->SetColor(Color(255, 0, 255));
		for (int aX = 0; aX < 800; aX += 100)
			g->FillRect(aX, 0, (aX == 400) ? 9 : 3, 600);
	}
	if (mSelectorState == SelectorAnimState::SELECTOR_OPEN)
	{
		int aBGIdx = aSelectorReanim->FindTrackIndex("SelectorScreen_BG_Right");
		ReanimatorTransform aTransform;
		aSelectorReanim->GetCurrentTransform(aBGIdx, &aTransform);
		float aFractionalOffsetX = fmod(aTransform.mTransX, 1.0f);
		float aFractionalOffsetY = fmod(aTransform.mTransY, 1.0f);
		g->DrawImageF(
			mOptionsButton->mButtonImage,
			mOptionsButton->mX + mOptionsButton->mButtonOffsetX + aFractionalOffsetX,
			mOptionsButton->mY + mOptionsButton->mButtonOffsetY + aFractionalOffsetY
		);
		g->DrawImageF(
			mQuitButton->mButtonImage,
			mQuitButton->mX + mQuitButton->mButtonOffsetX + aFractionalOffsetX,
			mQuitButton->mY + mQuitButton->mButtonOffsetY + aFractionalOffsetY
		);
		g->DrawImageF(
			mHelpButton->mButtonImage, 
			mHelpButton->mX + mHelpButton->mButtonOffsetX + aFractionalOffsetX, 
			mHelpButton->mY + mHelpButton->mButtonOffsetY + aFractionalOffsetY
		);
	}

	if (mApp->mPlayerInfo && mApp->mPlayerInfo->mName.size() &&
		mSelectorState != SelectorAnimState::SELECTOR_OPEN && mSelectorState != SelectorAnimState::SELECTOR_NEW_USER)
	{
		SexyString aWelcomeStr = mApp->mPlayerInfo->mName + _S('!');

		int aSignIdx = aSelectorReanim->FindTrackIndex("woodsign1");
		SexyTransform2D aOverlayMatrix;
		aSelectorReanim->GetAttachmentOverlayMatrix(aSignIdx, aOverlayMatrix);
		float aStringWidth = Sexy::FONT_BRIANNETOD16->StringWidth(aWelcomeStr);
		SexyTransform2D aOffsetMatrix;
		// @Patoke: add position so it moves when sliding to position
		aOffsetMatrix.Translate(170.5f - (int)(aStringWidth * 0.5f) + mX, 102.5f + mY);
		TodDrawStringMatrix(g, Sexy::FONT_BRIANNETOD16, aOverlayMatrix * aOffsetMatrix, aWelcomeStr, Color(255, 245, 200));

	}
	 
	// GOTY @Patoke: 0x4500E0
	// @pvz-online: these four draws are the QUICK-PLAY screen's own backdrop (opaque
	// 800x600 painting, tree on the RIGHT) and its three cloud sprites. In the GOTY binary
	// they came from a separate routine (0x4500E0) that only ran while that screen was up;
	// this decomp inlined it here WITHOUT its condition, so every main-menu frame painted
	// the Quick-Play scene straight over the reanim backdrop - the "tree ended up on the
	// right, tombstone/dirt path gone" report. The Quick-Play screen is still stubbed
	// (GameSelector_QuickPlay handler below has ShowQuickPlayScreen() commented out), so
	// nothing needs these drawn here; when that screen is implemented, move these lines
	// into it behind its mode flag.
	//g->DrawImage(IMAGE_SELECTORSCREEN_MOREWAYSTOPLAY_BG, M(0), M(0));
	//g->DrawImage(IMAGE_QUICKPLAY_MINIGAMES_CLOUD, M(20), M(40));
	//g->DrawImage(IMAGE_QUICKPLAY_PUZZLES_CLOUD, M(350), M(285));
	//g->DrawImage(IMAGE_QUICKPLAY_SURVIVAL_CLOUD, M(130), M(135));
}

//0x44AB50
// GOTY @Patoke: 0x44D750
void GameSelector::DrawOverlay(Graphics* g)
{
	g->SetLinearBlend(true);
	if (mApp->mPlayerInfo == nullptr)
		return;

	if (!mApp->IsIceDemo() && !mShowStartButton)
	{
		int aOffsetX, aOffsetY;
		if (mAdventureButton->mIsDown && mAdventureButton->mIsOver)
		{
			aOffsetX = 1;
			aOffsetY = 1;
		}
		else
		{
			aOffsetX = 0;
			aOffsetY = 0;
		}

		Reanimation* aSelectorReanim = mApp->ReanimationGet(mSelectorReanimID);
		int aRightIdx = aSelectorReanim->FindTrackIndex("SelectorScreen_BG_Right");
		ReanimatorTransform aTransform;
		aSelectorReanim->GetCurrentTransform(aRightIdx, &aTransform);
		float aTransAreaX = aTransform.mTransX + aOffsetX;
		float aTransAreaY = aTransform.mTransY + aOffsetY;
		float aTransSubX = aTransAreaX;
		float aTransSubY = aTransAreaY;

		int aStage = ClampInt((mLevel - 1) / 10 + 1, 1, 6);  // 大关
		int aSub = mLevel - (aStage - 1) * 10;  // 小关
		if (mApp->IsTrialStageLocked() && (mLevel >= 25 || mApp->HasFinishedAdventure()))
		{
			aStage = 3;
			aSub = 4;
		}
		else
		{
			if (aStage == 1)
			{
				aTransAreaY += 1.0f;
			}
			else if (aStage == 4)
			{
				aTransAreaX -= 1.0f;
			}
			if (aSub == 3)
			{
				aTransSubX -= 1.0f;
			}
		}

		g->SetColorizeImages(true);
		g->SetColor(mAdventureButton->mColors[ButtonWidget::COLOR_BKG]);
		// @Patoke: changed positions for GOTY adventure icon
		TodDrawImageCelF(g, Sexy::IMAGE_SELECTORSCREEN_LEVELNUMBERS, aTransAreaX + 486.0f, aTransAreaY + 47.f, aStage, 0);  // 绘制大关数
		if (aSub < 10)
		{
			TodDrawImageCelF(g, Sexy::IMAGE_SELECTORSCREEN_LEVELNUMBERS, aTransSubX + 509.f, aTransSubY + 50.f, aSub, 0);
		}
		else if (aSub == 10)
		{
			TodDrawImageCelF(g, Sexy::IMAGE_SELECTORSCREEN_LEVELNUMBERS, aTransSubX + 509.f, aTransSubY + 50.f, 1, 0);
			TodDrawImageCelF(g, Sexy::IMAGE_SELECTORSCREEN_LEVELNUMBERS, aTransSubX + 518.f, aTransSubY + 51.f, 0, 0);
		}
		g->SetColorizeImages(false);
	}

	if (mZenGardenButton->mVisible && mApp->mZenGarden->PlantsNeedWater())
	{
		g->DrawImage(Sexy::IMAGE_PLANTSPEECHBUBBLE, mZenGardenButton->mX + 106, mZenGardenButton->mY + 36);
		g->DrawImage(Sexy::IMAGE_WATERDROP, mZenGardenButton->mX + 123, mZenGardenButton->mY + 45);
	}

	Reanimation* aHandReanim = mApp->ReanimationTryToGet(mHandReanimID);
	if (aHandReanim)
	{
		g->SetClipRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT - 40);
		aHandReanim->Draw(g);
		g->ClearClipRect();
	}

	// @pvz-online: same reason as the buttons hidden in Update - these reanims draw at
	// absolute positions, so sliding the selector up left them on screen growing over the
	// achievements page. The page's mY is its absolute screen y now (top-level widget):
	// 0 exactly when the page is up.
	if (!(mAchievementsWidget && mAchievementsWidget->mY <= 0))
	{
		g->Translate(-(mX + 800), 0);
		mApp->ReanimationGet(mLeafReanimID)->Draw(g);
		g->Translate(mX + 800, 0);

		for (int i = 0; i < 3; i++)
			mApp->ReanimationGet(mFlowerReanimID[i])->Draw(g);
	}

	if (mApp->mBetaValidate)
	{
		g->SetFont(Sexy::FONT_BRIANNETOD16);
		g->SetColor(Color(200, 200, 200));

		if (gIsPartnerBuild)
			g->DrawString(_S("PRESS/PARTNER PREVIEW BUILD: DO NOT DISTRIBUTE"), 27, 594);
		else
			g->DrawString(_S("BETA BUILD: DO NOT DISTRIBUTE"), 27, 594);
	}

	// @Minerscale: Trophy needs to draw in the DrawOverlay
	Reanimation* aSelectorReanim = mApp->ReanimationGet(mSelectorReanimID);

	int aLeftIdx = aSelectorReanim->FindTrackIndex("SelectorScreen_BG_Left");
	ReanimatorTransform aTransformLeft;
	aSelectorReanim->GetCurrentTransform(aLeftIdx, &aTransformLeft);
	if (mHasTrophy)
	{
		// @Patoke: updated pos to match GOTY
		if (mApp->EarnedGoldTrophy())
			TodDrawImageCelF(g, Sexy::IMAGE_SUNFLOWER_TROPHY, aTransformLeft.mTransX + 12.f, aTransformLeft.mTransY + 345.f, 1, 0);
		else
			TodDrawImageCelF(g, Sexy::IMAGE_SUNFLOWER_TROPHY, aTransformLeft.mTransX + 12.f, aTransformLeft.mTransY + 345.f, 0, 0);
		
		TodParticleSystem* aTrophyParticle = mApp->ParticleTryToGet(mTrophyParticleID);
		if (aTrophyParticle)
			aTrophyParticle->Draw(g);
	}

	mToolTip->Draw(g);
}

//0x44B0D0
// GOTY @Patoke: 0x44DE6D
void GameSelector::UpdateTooltip()
{
	if (!mApp->HasFinishedAdventure() || mApp->GetDialog(Dialogs::DIALOG_MESSAGE))
		return;

	if (mHasTrophy)
	{
		int aMouseX = mApp->mWidgetManager->mLastMouseX;
		int aMouseY = mApp->mWidgetManager->mLastMouseY;
		if (aMouseX >= 50 && aMouseX < 135 && aMouseY >= 325 && aMouseY <= 550)
		{
			if (mApp->EarnedGoldTrophy())
			{
				mToolTip->SetLabel(LawnApp::Pluralize(mApp->mPlayerInfo->mFinishedAdventure, _S("[GOLD_SUNFLOWER_TOOLTIP]"), _S("[GOLD_SUNFLOWER_TOOLTIP_PLURAL]")));
				mToolTip->mX = 32;
				mToolTip->mY = 510;
				mToolTip->mVisible = true;
			}
			else
			{
				mToolTip->SetLabel(_S("[SILVER_SUNFLOWER_TOOLTIP]"));
				mToolTip->mX = 20;
				mToolTip->mY = 495;
				mToolTip->mVisible = true;
			}

			return;
		}
	}

	mToolTip->mVisible = false;
	mToolTip->Update();
}

//0x44B2A0
// GOTY @Patoke: 0x44E030
void GameSelector::Update()
{
	// @pvz-online debug: is the selector screen actually updating?
	{
		static int sUpdateCount = 0;
		if ((++sUpdateCount % 60) == 1)
			TodTrace("GameSelector::Update #%d state=%d slide=%d pos=%d,%d starting=%d",
				sUpdateCount, (int)mSelectorState, mSlideCounter, mX, mY, (int)mStartingGame);
	}

	Widget::Update();
	MarkDirty();
	UpdateTooltip();

	// @pvz-online debug: PVZ_DUMP=1 prints the runtime position of every component once
	// the layout has settled (PVZ_DUMPFRAME, default 300 updates), so the menu can be
	// checked against the background art instead of guessed at.
	if (getenv("PVZ_DUMP") && !sDumped && ++sDumpFrame > EnvInt("PVZ_DUMPFRAME", 300))
	{
		sDumped = true;
		fprintf(stderr, "[dump] selector mX=%d mY=%d %dx%d state=%d\n",
			mX, mY, mWidth, mHeight, (int)mSelectorState);
		Reanimation* aR = mApp->ReanimationGet(mSelectorReanimID);
		if (aR)
			fprintf(stderr, "[dump] reanim overlay=(%.1f,%.1f) loops=%d\n",
				aR->mOverlayMatrix.m02, aR->mOverlayMatrix.m12, aR->mLoopCount);
		// Every track with the render group it currently belongs to. GameSelector::Draw
		// only issues DrawRenderGroup(g, 1) and DrawRenderGroup(g, RENDER_GROUP_NORMAL),
		// so anything sitting in another group is invisible no matter where it is.
		if (aR)
		{
			int aGroupCounts[8] = { 0 };
			for (int i = 0; i < aR->mDefinition->mTracks.count; i++)
			{
				int aGroup = aR->mTrackInstances[i].mRenderGroup;
				if (aGroup >= 0 && aGroup < 8)
					aGroupCounts[aGroup]++;
				fprintf(stderr, "[dump] track %-38s group=%d trans=(%.1f,%.1f)\n",
					aR->mDefinition->mTracks.tracks[i].mName, aGroup,
					aR->mTrackInstances[i].mBlendTransform.mTransX,
					aR->mTrackInstances[i].mBlendTransform.mTransY);
			}
			fprintf(stderr, "[dump] groups:");
			for (int i = 0; i < 8; i++)
				fprintf(stderr, " %d=%d", i, aGroupCounts[i]);
			fprintf(stderr, "  RENDER_GROUP_NORMAL=%d (total tracks=%d)\n",
				(int)RENDER_GROUP_NORMAL, aR->mDefinition->mTracks.count);
		}
		#define PVZ_DUMP_BTN(n) fprintf(stderr, "[dump] %-18s mX=%-5d mY=%-5d %dx%d noDraw=%d vis=%d\n", \
			#n, n->mX, n->mY, n->mWidth, n->mHeight, n->mBtnNoDraw, n->mVisible)
		PVZ_DUMP_BTN(mAdventureButton); PVZ_DUMP_BTN(mMinigameButton);
		PVZ_DUMP_BTN(mPuzzleButton);    PVZ_DUMP_BTN(mSurvivalButton);
		PVZ_DUMP_BTN(mZenGardenButton); PVZ_DUMP_BTN(mOptionsButton);
		PVZ_DUMP_BTN(mQuitButton);      PVZ_DUMP_BTN(mHelpButton);
		PVZ_DUMP_BTN(mStoreButton);     PVZ_DUMP_BTN(mAlmanacButton);
		PVZ_DUMP_BTN(mChangeUserButton); PVZ_DUMP_BTN(mAchievementsButton);
		#undef PVZ_DUMP_BTN
		fflush(stderr);
	}

	// @Patoke: implemented this
	if (mSlideCounter > 0) {
		int aNewX = TodAnimateCurve(75, 0, mSlideCounter, mStartX, mDestX, TodCurves::CURVE_EASE_IN_OUT);
		int aNewY = TodAnimateCurve(75, 0, mSlideCounter, mStartY, mDestY, TodCurves::CURVE_EASE_IN_OUT);
		// @pvz-online: Move() on the selector is the entire slide. The framework already
		// adds a parent's mX/mY to every child, at draw AND hit-test time (WidgetManager::
		// DrawWidgetsTo -> WidgetContainer::DrawAll / GetWidgetAtHelper), so the menu
		// buttons and mOverlayWidget - both children, both positioned against static reanim
		// tracks by TrackButton() - ride along by themselves. This block used to also feed
		// the same slide offset into every button's NewLawnButton::SetOffset and into
		// mOverlayWidget->Move: that stacked a second copy of the offset on top of the
		// inherited one, so the buttons and the level digits slid at TWICE the stone's
		// speed, clicks made during the slide missed (the hit rect moved once, the art
		// twice), and the three vase buttons stayed 15/5/30 px below the vases painted into
		// the BG_Right art for the rest of the session - SetOffset is a draw offset and
		// nothing ever reset it.
		Move(aNewX, aNewY);

		// @pvz-online: the page is a top-level widget riding exactly one screen below the
		// menu (absolute y = aNewY + mHeight): it reaches y=0 - covering the screen - the
		// moment the slide completes, and drops back below the fold on the way out. Move()
		// marks it dirty through Resize, so it repaints every sliding frame; the rigid
		// column means menu-bottom and page-top always share an edge and no stale band
		// can open between them.
		mAchievementsWidget->Move(aNewX, aNewY + mApp->mHeight);

		// @Patoke: make sure these are drawn even outside of bounds (force redraw)
		mAchievementsButton->MarkDirty();
		mOptionsButton->MarkDirty();
		mHelpButton->MarkDirty();
		mQuitButton->MarkDirty();
		mStoreButton->MarkDirty();

		mSlideCounter--;
	}

	// @pvz-online: hide the menu's buttons for exactly as long as the achievements page
	// covers the screen. They are children of the selector, so the slide already carries
	// them off the top; hiding them as well keeps them out of hit testing while the page
	// owns the frame, and restores them the moment the slide brings them back.
	{
		bool aPageUp = mAchievementsWidget && mAchievementsWidget->mY <= 0;
		if (aPageUp != mMenuHiddenForAchievements)
		{
			mMenuHiddenForAchievements = aPageUp;
			// (mQuickPlayButton is deliberately absent: it stays hidden until the
			// Quick-Play screen exists, so this loop must never restore it to visible.
			// @pvz-online: the M1-culled entries - mMinigameButton, mPuzzleButton,
			// mStoreButton, mZenGardenButton, mZombatarButton - are absent for the same
			// reason: this loop runs when the page slides away and would otherwise
			// restore buttons the cull just removed.)
			NewLawnButton* aMenuButtons[] = {
				mAdventureButton, mSurvivalButton,
				mOptionsButton, mQuitButton, mHelpButton,
				mAlmanacButton, mChangeUserButton,
				mAchievementsButton
			};
			for (int i = 0; i < (int)(sizeof(aMenuButtons) / sizeof(aMenuButtons[0])); i++)
			{
				aMenuButtons[i]->mVisible = !aPageUp;
				aMenuButtons[i]->MarkDirty();
			}
		}
	}

	mApp->mZenGarden->UpdatePlantNeeds();

	TodParticleSystem* aParticle = mApp->ParticleTryToGet(mTrophyParticleID);
	if (aParticle)
		aParticle->Update();

	if (mStartingGame)
	{
		mStartingGameCounter++;
		if (mStartingGameCounter > 450)
		{
			mApp->KillGameSelector();

			if (mApp->IsIceDemo())
			{
				mApp->PreNewGame(GameMode::GAMEMODE_CHALLENGE_ICE, false);
				return;
			}
			if (mApp->IsFirstTimeAdventureMode() && mLevel == 0 && mApp->SaveFileExists())
			{
				mApp->PreNewGame(GameMode::GAMEMODE_INTRO, false);
				return;
			}
			if (mApp->mPlayerInfo->mNeedsMagicTacoReward && mLevel == 35)
			{
				StoreScreen* aStore = mApp->ShowStoreScreen();
				aStore->SetupForIntro(601);
				aStore->WaitForResult(true);
				mApp->PreNewGame(GameMode::GAMEMODE_ADVENTURE, false);
				return;
			}
			if (ShouldDoZenTuturialBeforeAdventure())
			{
				mApp->PreNewGame(GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN, false);
				mApp->mZenGarden->SetupForZenTutorial();
				return;
			}
			mApp->PreNewGame(GameMode::GAMEMODE_ADVENTURE, true);
			return;
		}

		mAdventureButton->SetColor(ButtonWidget::COLOR_BKG, mStartingGameCounter % 20 < 10 ? Color(80, 80, 80) : Color::White);
		if (mStartingGameCounter == 125)
			mApp->PlaySample(Sexy::SOUND_EVILLAUGH);
	}

	Reanimation* aSelectorReanim = mApp->ReanimationGet(mSelectorReanimID);
	switch (mSelectorState)
	{
	case SelectorAnimState::SELECTOR_OPEN:
		if (mWidgetManager)
			mWidgetManager->RehupMouse();
		if (aSelectorReanim->mLoopCount > 0)
		{
			// @pvz-online: the roll-in is intentionally NOT triggered here. Sexy's
			// dirty-rect tracking cannot erase the pixels a widget vacates while it is
			// still off-screen, so each of the 75 slide frames left a permanent ghost
			// of the menu (verified by A/B screenshot). The selector is parked at its
			// final position in AddedToManager instead.
			// @pvz-online debug: PVZ_BOARDS=reanim draws the reanim's own button art and
			// hides the ButtonWidgets instead, so the two coordinate spaces can be
			// compared directly (the buttons are supposed to land exactly on top of the
			// reanim tracks that TrackButton() reads its positions from).
			const char* aBoardsEnv = getenv("PVZ_BOARDS");
			bool aBoardsFromReanim = (aBoardsEnv != nullptr) && (strcmp(aBoardsEnv, "reanim") == 0);
			bool aBoardsFromWidget = !aBoardsFromReanim;

			if (!aBoardsFromReanim)
			{
				aSelectorReanim->AssignRenderGroupToTrack("SelectorScreen_Adventure_button", RENDER_GROUP_HIDDEN);
				aSelectorReanim->AssignRenderGroupToTrack("SelectorScreen_StartAdventure_button", RENDER_GROUP_HIDDEN);
				aSelectorReanim->AssignRenderGroupToTrack("SelectorScreen_Survival_button", RENDER_GROUP_HIDDEN);
				aSelectorReanim->AssignRenderGroupToTrack("SelectorScreen_Challenges_button", RENDER_GROUP_HIDDEN);
				aSelectorReanim->AssignRenderGroupToTrack("SelectorScreen_ZenGarden_button", RENDER_GROUP_HIDDEN);
			}
			mAdventureButton->mBtnNoDraw = !aBoardsFromWidget;
			mMinigameButton->mBtnNoDraw = !aBoardsFromWidget;
			mPuzzleButton->mBtnNoDraw = !aBoardsFromWidget;
			mSurvivalButton->mBtnNoDraw = !aBoardsFromWidget;
			mZenGardenButton->mBtnNoDraw = !aBoardsFromWidget;
			mHelpButton->mBtnNoDraw = false;
			mOptionsButton->mBtnNoDraw = false;
			mQuitButton->mBtnNoDraw = false;
			mZombatarButton->mBtnNoDraw = false; // @Patoke: new widgets
			mAchievementsButton->mBtnNoDraw = false;
			mAdventureButton->mMouseVisible = true;
			mMinigameButton->mMouseVisible = true;
			mPuzzleButton->mMouseVisible = true;
			mSurvivalButton->mMouseVisible = true;
			mZenGardenButton->mMouseVisible = true;
			mHelpButton->mMouseVisible = true;
			mOptionsButton->mMouseVisible = true;
			mQuitButton->mMouseVisible = true;
			mStoreButton->mMouseVisible = true;
			mAlmanacButton->mMouseVisible = true;
			mChangeUserButton->mMouseVisible = true;
			mZombatarButton->mMouseVisible = true; // @Patoke: new widgets
			mAchievementsButton->mMouseVisible = true;

			if (mApp->mPlayerInfo == nullptr)
			{
				mApp->DoCreateUserDialog();
				if (gIsPartnerBuild)
					AddPreviewProfiles();

				mSelectorState = SelectorAnimState::SELECTOR_NEW_USER;
			}
			else
			{
				aSelectorReanim->PlayReanim("anim_sign", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 30.0f);
				mSelectorState = SelectorAnimState::SELECTOR_IDLE;
			}

			if (mHasTrophy)
				AddTrophySparkle();

			if (mApp->mPlayerInfo && mApp->mPlayerInfo->mNeedsMessageOnGameSelector)
			{
				mApp->mPlayerInfo->mNeedsMessageOnGameSelector = 0;
				mApp->WriteCurrentUserConfig();
				mApp->LawnMessageBox(
					Dialogs::DIALOG_MESSAGE, 
					_S("[ADVENTURE_COMPLETE_HEADER]"), 
					_S("[ADVENTURE_COMPLETE_BODY]"), 
					_S("[DIALOG_BUTTON_OK]"), 
					_S(""), 
					Dialog::BUTTONS_FOOTER
				);
			}
		}
		break;
	case SelectorAnimState::SELECTOR_NEW_USER:
		if (mApp->GetDialog(Dialogs::DIALOG_CREATEUSER) == nullptr)
		{
			aSelectorReanim->PlayReanim("anim_sign", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 30.0f);
			mSelectorState = SelectorAnimState::SELECTOR_SHOW_SIGN;
		}
		break;
	case SelectorAnimState::SELECTOR_SHOW_SIGN:
		if (aSelectorReanim->mLoopCount > 0)
			mSelectorState = SelectorAnimState::SELECTOR_IDLE;
		break;
	case SelectorAnimState::SELECTOR_IDLE:
		break;
	}

	for (int i = 0; i < 6; i++)
	{
		Reanimation* aCloudReanim = mApp->ReanimationGet(mCloudReanimID[i]);
		if (mCloudCounter[i] > 0)
		{
			if (--mCloudCounter[i] == 0)
			{
				aCloudReanim->mLoopCount = 0;
				aCloudReanim->mAnimTime = 0.0f;
				aCloudReanim->mAnimRate = 0.5f;
			}
		}
		else if (aCloudReanim->mLoopCount > 0)
			mCloudCounter[i] = RandRangeInt(2000, 4000);
	}
	aSelectorReanim->Update();

	Reanimation* aLeafReanim = mApp->ReanimationGet(mLeafReanimID);
	aLeafReanim->Update();
	int aLeafTrackIndex = aSelectorReanim->FindTrackIndex("SelectorScreen_BG_Right");
	ReanimatorTransform aLeafTransform;
	aSelectorReanim->GetCurrentTransform(aLeafTrackIndex, &aLeafTransform);
	aLeafReanim->SetPosition(aLeafTransform.mTransX - 71.0f, aLeafTransform.mTransY - 41.0f);
	if (--mLeafCounter == 0)
	{
		float aRate = RandRangeFloat(3.0f, 12.0f);
		aLeafReanim->PlayReanim("anim_grass", ReanimLoopType::REANIM_LOOP, 20, aRate);
		mLeafCounter = RandRangeInt(200, 400);
	}

	for (int i = 0; i < 6; i++)
		mApp->ReanimationGet(mCloudReanimID[i])->Update();
	Reanimation* aHandReanim = mApp->ReanimationTryToGet(mHandReanimID);
	if (aHandReanim)
		aHandReanim->Update();

	TrackButton(mAdventureButton, mShowStartButton ? "SelectorScreen_StartAdventure_button" : "SelectorScreen_Adventure_button", 0.0f, 0.0f);
	TrackButton(mMinigameButton, "SelectorScreen_Survival_button", 0.0f, 0.0f);
	TrackButton(mPuzzleButton, "SelectorScreen_Challenges_button", 0.0f, 0.0f);
	TrackButton(mSurvivalButton, "SelectorScreen_ZenGarden_button", 0.0f, 0.0f);
	TrackButton(mZenGardenButton, "SelectorScreen_BG_Right", 100.0f, 360.0f);
	TrackButton(mOptionsButton, "SelectorScreen_BG_Right", 494.0f, 434.0f);
	TrackButton(mQuitButton, "SelectorScreen_BG_Right", 644.0f, 469.0f);
	TrackButton(mHelpButton, "SelectorScreen_BG_Right", 576.0f, 458.0f);
	TrackButton(mAlmanacButton, "SelectorScreen_BG_Right", 256.0f, 387.0f);
	TrackButton(mStoreButton, "SelectorScreen_BG_Right", 334.0f, 441.0f);
	TrackButton(mChangeUserButton, "woodsign2", 24.0f, 10.0f);
	TrackButton(mZombatarButton, "woodsign3", 0.f, 0.f); // @Patoke: add shart here
	TrackButton(mAchievementsButton, "SelectorScreen_BG_Left", 20.f, 480.f);
	aSelectorReanim->SetImageOverride("woodsign2", (mChangeUserButton->mIsOver || mChangeUserButton->mIsDown) ? Sexy::IMAGE_REANIM_SELECTORSCREEN_WOODSIGN2_PRESS : nullptr);
	aSelectorReanim->SetImageOverride("woodsign3", (mZombatarButton->mIsOver || mZombatarButton->mIsDown) ? Sexy::IMAGE_REANIM_SELECTORSCREEN_WOODSIGN3_PRESS : nullptr);
}

//0x44BB20
// GOTY @Patoke: 0x44EA40
void GameSelector::TrackButton(DialogButton* theButton, const char* theTrackName, float theOffsetX, float theOffsetY)
{
	Reanimation* aSelectorReanim = mApp->ReanimationGet(mSelectorReanimID);
	int aTrackIndex = aSelectorReanim->FindTrackIndex(theTrackName);
	ReanimatorTransform aTransform;
	aSelectorReanim->GetCurrentTransform(aTrackIndex, &aTransform);
	
	theButton->mX = (int)(aTransform.mTransX + theOffsetX);
	theButton->mY = (int)(aTransform.mTransY + theOffsetY);
}

//0x44BBC0
void GameSelector::AddedToManager(WidgetManager* theWidgetManager)
{
	Widget::AddedToManager(theWidgetManager);
	this->Move(SelectorParkedX(), 0);
	// @pvz-online: register the achievements page as its own top-level widget (see the
	// constructor note). Appended after the selector, so it draws in front of the menu;
	// LawnApp's BringToBack(mGameSelector) right after this only lowers the selector.
	theWidgetManager->AddWidget(mAchievementsWidget);
}

//0x44BCA0
void GameSelector::RemovedFromManager(WidgetManager* theWidgetManager)
{
	Widget::RemovedFromManager(theWidgetManager);
	theWidgetManager->RemoveWidget(mAchievementsWidget);
}

//0x44BD80
void GameSelector::OrderInManagerChanged()
{
	//mWidgetManager->PutInfront(mAchievementsWidget, this);
	//mWidgetManager->PutInfront(mOverlayWidget, this);
	//mWidgetManager->PutInfront(mAlmanacButton, this);
	//mWidgetManager->PutInfront(mStoreButton, this);
	//mWidgetManager->PutInfront(mHelpButton, this);
	//mWidgetManager->PutInfront(mQuitButton, this);
	//mWidgetManager->PutInfront(mOptionsButton, this);
	//mWidgetManager->PutInfront(mAdventureButton, this);
	//mWidgetManager->PutInfront(mMinigameButton, this);
	//mWidgetManager->PutInfront(mPuzzleButton, this);
	//mWidgetManager->PutInfront(mZenGardenButton, this);
	//mWidgetManager->PutInfront(mSurvivalButton, this);
	//mWidgetManager->PutInfront(mChangeUserButton, this);
	//mWidgetManager->PutInfront(mZombatarButton, this); // @Patoke: z order for new widgets
	//mWidgetManager->PutInfront(mAchievementsButton, this);
	////mWidgetManager->PutInfront(mQuickPlayButton, this);
}

//0x44BE60
// GOTY @Patoke: 0x44EB11
void GameSelector::KeyDown(KeyCode theKey)
{
	if (mApp->mKonamiCheck->Check(theKey))
	{
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		return;
	}
	if (mApp->mMustacheCheck->Check(theKey) || mApp->mMoustacheCheck->Check(theKey))
	{
		mApp->PlayFoley(FoleyType::FOLEY_POLEVAULT);
		mApp->mMustacheMode = !mApp->mMustacheMode;
		ReportAchievement::GiveAchievement(mApp, MustacheMode, false);
		return;
	}
	if (mApp->mSuperMowerCheck->Check(theKey) || mApp->mSuperMowerCheck2->Check(theKey))
	{
		mApp->PlayFoley(FoleyType::FOLEY_ZAMBONI);
		mApp->mSuperMowerMode = !mApp->mSuperMowerMode;
		return;
	}
	if (mApp->mFutureCheck->Check(theKey))
	{
		mApp->PlaySample(Sexy::SOUND_BOING);
		mApp->mFutureMode = !mApp->mFutureMode;
		return;
	}
	if (mApp->mPinataCheck->Check(theKey))
	{
		if (mApp->CanDoPinataMode())
		{
			mApp->PlayFoley(FoleyType::FOLEY_JUICY);
			mApp->mPinataMode = !mApp->mPinataMode;
			return;
		}
		else
		{
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}
	if (mApp->mDanceCheck->Check(theKey))
	{
		if (mApp->CanDoDanceMode())
		{
			mApp->PlayFoley(FoleyType::FOLEY_DANCER);
			mApp->mDanceMode = !mApp->mDanceMode;
			return;
		}
		else
		{
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}
	if (mApp->mDaisyCheck->Check(theKey))
	{
		if (mApp->CanDoDaisyMode())
		{
			mApp->PlaySample(Sexy::SOUND_LOADINGBAR_FLOWER);
			mApp->mDaisyMode = !mApp->mDaisyMode;
			return;
		}
		else
		{
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}
	if (mApp->mSukhbirCheck->Check(theKey))
	{
		mApp->PlaySample(Sexy::SOUND_SUKHBIR);
		mApp->mSukhbirMode = !mApp->mSukhbirMode;
		return;
	}
}

//0x44C200
// GOTY @Patoke: 0x44EEE0
void GameSelector::KeyChar(char theChar)
{
	if (mStartingGame)
		return;

	if ((gIsPartnerBuild || mApp->mDebugKeysEnabled) && theChar == 'u' && mApp->mPlayerInfo)
	{
		TodTraceAndLog(_S("Selector cheat key '%c'"), theChar);

		mApp->mPlayerInfo->mFinishedAdventure = 2;
		mApp->mPlayerInfo->AddCoins(50000);
		mApp->mPlayerInfo->mHasUsedCheatKeys = true;
		mApp->mPlayerInfo->mHasUnlockedMinigames = true;
		mApp->mPlayerInfo->mHasUnlockedPuzzleMode = true;
		mApp->mPlayerInfo->mHasUnlockedSurvivalMode = true;

		for (int i = 1; i < 100; i++)
			if (i != (int)GameMode::GAMEMODE_TREE_OF_WISDOM && i != (int)GameMode::GAMEMODE_SCARY_POTTER_ENDLESS &&
				i != (int)GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS && i != (int)GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_3)
				mApp->mPlayerInfo->mChallengeRecords[i - 1] = 20;
		SyncProfile(true);

		mApp->EraseFile(GetSavedGameName(GameMode::GAMEMODE_ADVENTURE, mApp->mPlayerInfo->mId));
	}

	if (mApp->mDebugKeysEnabled)
	{
		TodTraceAndLog(_S("Selector cheat key '%c'"), theChar);
		if (theChar == 'c' || theChar == 'C')
		{
			mMinigamesLocked = false;
			mPuzzleLocked = false;
			mSurvivalLocked = false;
			SyncButtons();
		}
	}
}

//0x44C360
// GOTY @Patoke: 0x44F040
void GameSelector::MouseDown(int x, int y, int theClickCount)
{
	(void)theClickCount;
	for (int i = 0; i < 3; i++)
	{
		Reanimation* aFlowerReanim = mApp->ReanimationGet(mFlowerReanimID[i]);
		if (aFlowerReanim->mAnimRate <= 0.0f && Distance2D(x, y, gFlowerCenter[i][0], gFlowerCenter[i][1]) < 20.0f)
		{
			aFlowerReanim->mAnimRate = 24.0f;
			mApp->PlayFoley(FoleyType::FOLEY_LIMBS_POP);
		}
	}

	if (mApp->mTodCheatKeys && mStartingGame && mStartingGameCounter < 450)
		mStartingGameCounter = 450;
}

//0x44C4C0
// GOTY @Patoke: 0x44F1A0
void GameSelector::ButtonMouseEnter(int theId)
{
	if ((theId == GameSelector::GameSelector_Minigame && mMinigamesLocked) ||
		(theId == GameSelector::GameSelector_Puzzle && mPuzzleLocked) ||
		(theId == GameSelector::GameSelector_Survival && mSurvivalLocked))
		return;

	mApp->PlayFoley(FoleyType::FOLEY_BLEEP);
}

//0x44C540
// GOTY @Patoke: 0x44F220
void GameSelector::ButtonPress(int theId)
{
	if (theId == GameSelector::GameSelector_Adventure || theId == GameSelector::GameSelector_Minigame ||
		theId == GameSelector::GameSelector_Puzzle || theId == GameSelector::GameSelector_Survival ||
		theId == GameSelector::GameSelector_Zombatar) // @Patoke: add case
		mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
	else
		mApp->PlaySample(Sexy::SOUND_TAP);
}

//0x44C590
// GOTY @Patoke: 0x44F270
void GameSelector::ClickedAdventure()
{
	if (mApp->IsTrialStageLocked() && (mLevel >= 25 || mApp->HasFinishedAdventure()))
	{
		if (mApp->LawnMessageBox(
			Dialogs::DIALOG_MESSAGE,
			_S("[REPLAY_LEVEL_HEADER]"),
			_S("[REPLAY_LEVEL_BODY]"),
			_S("[DIALOG_BUTTON_YES]"),
			_S("[DIALOG_BUTTON_NO]"),
			Dialog::BUTTONS_YES_NO) == Dialog::ID_NO)
			return;

		mApp->mPlayerInfo->mLevel = 24;
		mApp->mPlayerInfo->mFinishedAdventure = 0;
		mApp->EraseFile(GetSavedGameName(GameMode::GAMEMODE_ADVENTURE, mApp->mPlayerInfo->mId));
	}

	mApp->mMusic->StopAllMusic();
	mApp->PlaySample(Sexy::SOUND_LOSEMUSIC);
	mStartingGame = true;
	mAdventureButton->SetDisabled(true);
	mMinigameButton->SetDisabled(true);
	mPuzzleButton->SetDisabled(true);
	mOptionsButton->SetDisabled(true);
	mQuitButton->SetDisabled(true);
	mHelpButton->SetDisabled(true);
	mChangeUserButton->SetDisabled(true);
	mStoreButton->SetDisabled(true);
	mAlmanacButton->SetDisabled(true);
	mSurvivalButton->SetDisabled(true);
	mZenGardenButton->SetDisabled(true);
	mZombatarButton->SetDisabled(true); // @Patoke: added new widgets
	mAchievementsButton->SetDisabled(true);

	Reanimation* aHandReanim = mApp->AddReanimation(-70.0f, 10.0f, 0, ReanimationType::REANIM_ZOMBIE_HAND);
	aHandReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
	mHandReanimID = mApp->ReanimationGetID(aHandReanim);
	mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
	for (int i = 0; i < aHandReanim->mDefinition->mTracks.count; i++)
		if (!strnicmp(aHandReanim->mDefinition->mTracks.tracks[i].mName, "rock", 4))
			aHandReanim->mTrackInstances[i].mIgnoreClipRect = true;
}

//0x44C890
// GOTY @Patoke: 0x44F590
bool GameSelector::ShouldDoZenTuturialBeforeAdventure()
{
	return !mApp->HasFinishedAdventure() && mApp->mPlayerInfo->GetLevel() == 45 && mApp->mPlayerInfo->mNumPottedPlants == 0;
}

//0x44C8C0
// GOTY @Patoke: 0x44F5C0
void GameSelector::ButtonDepress(int theId)
{
	// @pvz-online: M1 裁剪——小游戏/解谜的锁定提示随入口一起下架（这两个按钮已
	// mVisible=false，收不到点击）。生存按钮保留（无尽联机入口）。
	if (theId == GameSelector::GameSelector_Survival && mSurvivalLocked)
	{
		mApp->LawnMessageBox(Dialogs::DIALOG_MESSAGE, _S("[MODE_LOCKED]"), _S("[SURVIVAL_LOCKED_MESSAGE]"), _S("[DIALOG_BUTTON_OK]"), _S(""), Dialog::BUTTONS_FOOTER);
		return;
	}

	switch (theId)
	{
	case GameSelector::GameSelector_Adventure:
		ClickedAdventure();
		break;
	case GameSelector::GameSelector_Survival:
		mApp->KillGameSelector();
		mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
		break;
	case GameSelector::GameSelector_Quit:
		mApp->ConfirmQuit();
		break;
	case GameSelector::GameSelector_Help:
		mApp->KillGameSelector();
		mApp->ShowAwardScreen(AwardType::AWARD_HELP_ZOMBIENOTE, false);
		break;
	case GameSelector::GameSelector_Options:
		mApp->DoNewOptions(true);
		break;
	case GameSelector::GameSelector_ChangeUser:
		mApp->DoUserDialog();
		break;
	case GameSelector::GameSelector_Almanac:
		mApp->DoAlmanacDialog()->WaitForResult(true);
		mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME);
		break;
	case GameSelector::GameSelector_AchievementsBack: // @Patoke: seems to be unused
		SlideTo(0, 0);
		break;
	case GameSelector::GameSelector_Achievements:
		ShowAchievementsWidget();
		break;
	case GameSelector::GameSelector_QuickPlay:
		// GameSelector::ShowQuickPlayScreen();
		break;
	}
}

//0x44CB00
// GOTY @Patoke: 0x44F880
void GameSelector::AddPreviewProfiles()
{
	PlayerInfo* aProfile;

	aProfile = mApp->mProfileMgr->AddProfile(_S("2 Night"));
	if (aProfile)
	{
		aProfile->mLevel = 11;
		aProfile->SaveDetails();
	}

	aProfile = mApp->mProfileMgr->AddProfile(_S("3 Pool"));
	if (aProfile)
	{
		aProfile->mLevel = 21;
		aProfile->mHasUnlockedMinigames = 1;
		aProfile->mCoins = 400;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] = 1;
		aProfile->SaveDetails();
	}

	aProfile = mApp->mProfileMgr->AddProfile(_S("4 Fog"));
	if (aProfile)
	{
		aProfile->mLevel = 31;
		aProfile->mHasUnlockedMinigames = 1;
		aProfile->mHasUnlockedSurvivalMode = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] = 2;
		aProfile->mPurchases[StoreItem::STORE_ITEM_POOL_CLEANER] = 1;
		aProfile->mCoins = 400;
		aProfile->SaveDetails();
	}

	aProfile = mApp->mProfileMgr->AddProfile(_S("5 Roof"));
	if (aProfile)
	{
		aProfile->mLevel = 41;
		aProfile->mHasUnlockedMinigames = 1;
		aProfile->mHasUnlockedPuzzleMode = 1;
		aProfile->mHasUnlockedSurvivalMode = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] = 2;
		aProfile->mPurchases[StoreItem::STORE_ITEM_POOL_CLEANER] = 1;
		aProfile->mCoins = 500;
		aProfile->SaveDetails();
	}

	aProfile = mApp->mProfileMgr->AddProfile(_S("Complete"));
	if (aProfile)
	{
		aProfile->mLevel = 1;
		aProfile->mFinishedAdventure = 1;
		aProfile->mHasUnlockedMinigames = 1;
		aProfile->mHasUnlockedPuzzleMode = 1;
		aProfile->mHasUnlockedSurvivalMode = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] = 2;
		aProfile->mPurchases[StoreItem::STORE_ITEM_POOL_CLEANER] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_ROOF_CLEANER] = 1;
		aProfile->mCoins = 1000;
		aProfile->SaveDetails();
	}

	aProfile = mApp->mProfileMgr->AddProfile(_S("Full Unlock"));
	if (aProfile)
	{
		aProfile->mLevel = 1;
		aProfile->mFinishedAdventure = 2;
		aProfile->AddCoins(50000);
		aProfile->mPurchases[StoreItem::STORE_ITEM_FERTILIZER] = PURCHASE_COUNT_OFFSET + 5;
		aProfile->mPurchases[StoreItem::STORE_ITEM_BUG_SPRAY] = PURCHASE_COUNT_OFFSET + 5;
		aProfile->mPurchases[StoreItem::STORE_ITEM_CHOCOLATE] = PURCHASE_COUNT_OFFSET + 5;
		aProfile->mPurchases[StoreItem::STORE_ITEM_TREE_FOOD] = PURCHASE_COUNT_OFFSET + 5;
		aProfile->mHasUnlockedMinigames = 1;
		aProfile->mHasUnlockedPuzzleMode = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PLANT_GATLINGPEA] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PLANT_TWINSUNFLOWER] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PLANT_GLOOMSHROOM] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PLANT_CATTAIL] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PLANT_WINTERMELON] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PLANT_GOLD_MAGNET] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PLANT_SPIKEROCK] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PLANT_COBCANNON] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PLANT_IMITATER] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] = 3;
		aProfile->mPurchases[StoreItem::STORE_ITEM_POOL_CLEANER] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_ROOF_CLEANER] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_PHONOGRAPH] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_GARDENING_GLOVE] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_MUSHROOM_GARDEN] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_WHEEL_BARROW] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_AQUARIUM_GARDEN] = 1;
		aProfile->mPurchases[StoreItem::STORE_ITEM_TREE_OF_WISDOM] = 1;

		aProfile->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - 1] = 1;
		for (int i = 1; i < 100; i++)
			if (i != (int)GameMode::GAMEMODE_TREE_OF_WISDOM && i != (int)GameMode::GAMEMODE_SCARY_POTTER_ENDLESS &&
				i != (int)GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS && i != (int)GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_3)
				mApp->mPlayerInfo->mChallengeRecords[i - 1] = 20;

		aProfile->SaveDetails();
	}
}

// @Patoke: implemented functions
// GOTY @Patoke: 0x450140
void GameSelector::SlideTo(int theX, int theY) {
	mSlideCounter = 75;
	mDestX = theX;
	mDestY = theY;
	mStartX = mX;
	mStartY = mY;
}

// GOTY @Patoke: 0x450200
void GameSelector::ShowAchievementsWidget() {
	SlideTo(0, -mApp->mHeight);
	mWidgetManager->SetFocus(mAchievementsWidget);
}