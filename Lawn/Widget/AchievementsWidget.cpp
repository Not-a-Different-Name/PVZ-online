// @Patoke: implement file
#include "AchievementsWidget.h"
#include "../Board.h"
#include "GameButton.h"
#include "GameSelector.h"
#include "../../LawnApp.h"
#include "Almanac.h"
#include "../../Resources.h"
#include "../System/Music.h"
#include "../../GameConstants.h"
#include "../System/PlayerInfo.h"
#include "../System/ProfileMgr.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Sexy.TodLib/TodDebug.h"
#include "graphics/Font.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "widget/Dialog.h"
#include "widget/WidgetManager.h"

Rect aBackButtonRect = { 120, 35, 130, 80 };

AchievementItem gAchievementList[MAX_ACHIEVEMENTS] = {
	{ "Home Lawn Security", "Complete Adventure Mode." },
	{ "Nobel Peas Prize", "Get the golden sunflower trophy." },
	{ "Better Off Dead", "Get to a streak of 10 in I, Zombie Endless" },
	{ "China Shop", "Get to a streak of 15 in Vasebreaker Endless" },
	{ "SPUDOW!", "Blow up a zombie using a Potato Mine." },
	{ "Explodonator", "Take out 10 full-sized zombies with a single Cherry Bomb." },
	{ "Morticulturalist", "Collect all 49 plants (including plants from Crazy Dave's shop)." },
	{ "Don't Pea in the Pool", "Complete a daytime pool level without using pea shooters of any kind." },
	{ "Roll Some Heads", "Complete a daytime pool level without using pea shooters of any kind." },
	{ "Grounded", "Defeat a normal roof level without using any catapult plants." },
	{ "Zombologist", "Discover the Yeti zombie." },
	{ "Penny Pincher", "Pick up 30 coins in a row on a single level without letting any disappear." },
	{ "Sunny Days", "Get 8000 sun during a single level." },
	{ "Popcorn Party", "Defeat 2 Gargantuars with Corn Cob missiles in a single level." },
	{ "Good Morning", "Complete a daytime level by planting only Mushrooms and Coffee Beans." },
	{ "No Fungus Among Us", "Complete a nighttime level without planting any Mushrooms." },
	{ "Beyond the Grave", "Beat all 20 mini games." },
	{ "Immortal", "Survive 20 waves of pure zombie ferocity." },
	{ "Towering Wisdom", "Grow the Tree of Wisdom to 100 feet." },
	{ "Mustache Mode", "Enable Mustache Mode" }
};

// GOTY @Patoke: 0x401000
AchievementsWidget::AchievementsWidget(LawnApp* theApp) {
	mApp = theApp;
	mWidth = 800;
	// @pvz-online: was CHINA->mHeight + BG->mHeight + 15700 == 16150, a scrolling wall of
	// which the top ~750px was the only part that ever had anything on it. With that
	// height, Update()'s clamp computes aMaxScroll == 2*600 + 50 - 16150 == -14900, so the
	// first wheel tick or arrow key flung the whole page 14900px off-screen and the player
	// was left looking at an empty wall - the "achievements page has no content" bug.
	// The page is one screen tall now: 20 achievements in a 3x7 grid (laid out in Draw).
	mHeight = mApp->mHeight;
	mScrollDirection = -1;
	mScrollValue = 0;
	mDefaultScrollValue = 30;
	mScrollDecay = 1;
	mDidPressMoreButton = false;
	// Nothing to scroll to, so the more/scroll button is retired: the zero rect makes
	// every Contains() in MouseDown/MouseUp below false.
	mMoreRockRect = Rect(0, 0, 0, 0);
	// The page had no visible exit: Resources.h only ever had the *hover highlight* for
	// this button (IMAGE_ACHEESEMENTS_BACK does not exist), and Draw only drew that
	// highlight, so there was nothing on screen to click. Use the menu's own BACK sign
	// instead, in a box of our choosing so it is always fully on screen, placed
	// bottom-right the way the menu itself places it.
	aBackButtonRect = Rect(mApp->mWidth - 156, mApp->mHeight - 100, 144, 88);
}

// GOTY @Patoke: 0x4010E0
AchievementsWidget::~AchievementsWidget() {

}

// GOTY @Patoke: 0x401A10
void AchievementsWidget::Update() {
	// @pvz-online: the page is exactly one screen tall now, so there is nothing to scroll.
	// This guard is not just an optimisation - the clamp further down is wrong once the
	// page fits: aMaxScroll becomes 2*600 + 50 - 600 == 650, and `aNewY <= aMaxScroll` is
	// then true for every reachable value, so the page gets thrown *down* to y=650, i.e.
	// off the bottom of the screen, on the first wheel tick.
	if (mHeight <= mApp->mHeight)
	{
		mScrollValue = 0;
		return;
	}

	if (mScrollValue <= 0)
		return;

	if (mScrollValue > mDefaultScrollValue)
		mScrollValue = mDefaultScrollValue;

	mScrollValue -= mScrollDecay;

	int aNewY = mY + mScrollValue * mScrollDirection;
	if (aNewY >= -1)
		aNewY = -1;
	//if (aNewY >= mApp->mHeight)
	//	aNewY = mApp->mHeight;

	int aMaxScroll = 2 * mApp->mHeight + 50 - mHeight;
	if (aNewY <= aMaxScroll)
		aNewY = aMaxScroll;

	// @pvz-online: aDelta was computed *after* mY = aNewY, so it was always 0 and the
	// back/more rects never followed the scroll at all.
	int aDelta = aNewY - mY;
	mY = aNewY;
	mMoreRockRect.mY += aDelta;
	aBackButtonRect.mY += aDelta;

	if (mScrollValue <= 0)
		mScrollValue = 0;
}

// GOTY @Patoke: 0x401160
void AchievementsWidget::Draw(Graphics* g) {
	// @pvz-online: vanilla drew the wall once and then 70 hole tiles down a 16150px
	// surface, with the Bejeweled/Zuma gallery art pinned at y=1125..11250 and the CHINA
	// piece at mHeight-875. Here the tiles simply stop at the screen edge; the gallery
	// pieces are dropped, since at those offsets they were decoration nobody could
	// reach. (CHINA->mHeight - 875 also went negative once mHeight became 600.)
	// The page has to erase what is underneath before it draws itself. WidgetManager never
	// clears its persistent 800x600 surface - ordinarily that is fine because a full-screen
	// widget paints over all of it - but this page cannot: the wall art carries alpha, so
	// drawing it only blends onto whatever was already there. What was already there is the
	// menu as it stood during the slide-in plus earlier frames of this very page, which is
	// what made the page look like it "had no content": it was mostly stale pixels showing
	// through. SetColor + FillRect is the same idiom AwardScreen uses to erase the board.
	g->SetColorizeImages(true);
	g->SetColor(Color(72, 60, 44));
	g->FillRect(0, 0, mWidth, mHeight);
	g->SetColorizeImages(false);

	g->DrawImage(IMAGE_SELECTORSCREEN_ACHIEVEMENTS_BG, 0, 0);
	int aTileHeight = IMAGE_ACHEESEMENTS_HOLE_TILE->mHeight;
	// Fall back to the wall art if the hole tile is missing: tile height 0 would leave
	// everything below the first 225px unpainted.
	if (aTileHeight <= 0)
		aTileHeight = IMAGE_SELECTORSCREEN_ACHIEVEMENTS_BG->mHeight;
	if (aTileHeight > 0)
	{
		for (int aY = IMAGE_SELECTORSCREEN_ACHIEVEMENTS_BG->mHeight; aY < mHeight; aY += aTileHeight)
			g->DrawImage(aTileHeight == IMAGE_ACHEESEMENTS_HOLE_TILE->mHeight ? IMAGE_ACHEESEMENTS_HOLE_TILE : IMAGE_SELECTORSCREEN_ACHIEVEMENTS_BG, 0, aY);
	}

	// @pvz-online: 20 achievements in 3 columns x 7 rows, which is what fits one screen.
	// The row pitch is the readability fix: vanilla used 57px in a 2-column layout while
	// each entry draws a 15pt title plus a 12pt description that wraps to up to 3 lines,
	// so every row overlapped the one below it.
	for (int i = 0; i < MAX_ACHIEVEMENTS; i++) {
		bool aHasAchievement;
		if (mApp->mPlayerInfo) aHasAchievement = mApp->mPlayerInfo->mEarnedAchievements[i];
		else aHasAchievement = false;

		int aImageXPos = 12 + (i % 3) * 208;
		int aImageYPos = 82 + (i / 3) * 64;
		int aTextXPos = aImageXPos + 62;
		int aTextYPos = aImageYPos + 2;

		// Achievement images
		Rect aSrcRect(70 * (i % 7), 70 * (i / 7), 70, 70);
		Rect aDestRect(aImageXPos, aImageYPos, 56, 56);

		g->SetColorizeImages(true);
		g->SetColor(aHasAchievement ? Color(255, 255, 255) : Color(255, 255, 255, 32));

		g->DrawImage(IMAGE_ACHEESEMENTS_ICONS, aDestRect, aSrcRect);
		g->SetColorizeImages(false);

		// Achievement titles
		g->SetFont(FONT_DWARVENTODCRAFT15);
		g->SetColor(Color(21, 175, 0));

		g->DrawString(gAchievementList[i].name, aTextXPos, aTextYPos);

		// Achievement descriptions
		Rect aPos = Rect(aTextXPos, aTextYPos + 18, 146, 46);

		g->SetFont(FONT_DWARVENTODCRAFT12);
		g->SetColor(Color(255, 255, 255));

		g->WriteWordWrapped(aPos, gAchievementList[i].description, 12);
	}

	// @pvz-online: the exit. Drawn last so it sits over the wall, and drawn as a *sign*
	// rather than only the hover highlight - that was the original bug, the page had no
	// visible way out at all. Stretched into aBackButtonRect so the art always lands
	// fully on screen regardless of its natural size.
	bool aBackHighlight = aBackButtonRect.Contains(mWidgetManager->mLastMouseX - mX, mWidgetManager->mLastMouseY - mY);
	g->DrawImage(IMAGE_QUICKPLAY_BACK_BUTTON, aBackButtonRect,
		Rect(0, 0, IMAGE_QUICKPLAY_BACK_BUTTON->mWidth, IMAGE_QUICKPLAY_BACK_BUTTON->mHeight));
	if (aBackHighlight)
		g->DrawImage(IMAGE_ACHEESEMENTS_BACK_HIGHLIGHT, aBackButtonRect,
			Rect(0, 0, IMAGE_ACHEESEMENTS_BACK_HIGHLIGHT->mWidth, IMAGE_ACHEESEMENTS_BACK_HIGHLIGHT->mHeight));
}

// GOTY @Patoke: 0x4019D0
void AchievementsWidget::KeyDown(KeyCode theKey) {
	if (theKey == KEYCODE_UP) {
		mScrollValue = mDefaultScrollValue;
		mScrollDirection = 1;
	}
	else if (theKey == KEYCODE_DOWN) {
		mScrollValue = mDefaultScrollValue;
		mScrollDirection = -1;
	}
	// @pvz-online: leaving the page used to depend on ESC reaching the GameSelector, so
	// the only exit that reliably worked was the one the player could not see. Handle it
	// here too, the same way the back button returns.
	else if (theKey == KEYCODE_ESCAPE) {
		mApp->mGameSelector->SlideTo(0, 0);
		mWidgetManager->SetFocus(mApp->mGameSelector);
	}
}

// GOTY @Patoke: 0x4017F0
void AchievementsWidget::MouseDown(int x, int y, int theClickCount) {
	(void)theClickCount;
	if (aBackButtonRect.Contains(x, y))
		mApp->PlaySample(SOUND_GRAVEBUTTON);

	if (mMoreRockRect.Contains(x, y))
		mApp->PlaySample(SOUND_GRAVEBUTTON);
}

// GOTY @Patoke: 0x401890
void AchievementsWidget::MouseUp(int x, int y, int theClickCount) {
	(void)theClickCount;
	Point aPos = Point(x, y);
	if (aBackButtonRect.Contains(aPos)) {
		mApp->mGameSelector->SlideTo(0, 0);
		mApp->mGameSelector->mWidgetManager->SetFocus(mApp->mGameSelector);
	}

	if (mMoreRockRect.Contains(aPos)) {
		mDidPressMoreButton = !mDidPressMoreButton;
		mScrollDirection = mDidPressMoreButton ? -1 : 1;
		mScrollValue = 20;
	}
}

// GOTY @Patoke: 0x4019A0
void AchievementsWidget::MouseWheel(int theDelta) {
	mScrollValue = mDefaultScrollValue;

	if (theDelta > 0)
		mScrollDirection = 1;
	else if (theDelta < 0)
		mScrollDirection = -1;
}

// GOTY @Patoke: 0x459670
bool ReportAchievement::GiveAchievement(LawnApp* theApp, int theAchievement, bool theForceGive) {
	// todo @Patoke: finish adding the achievement give events
	if (!theApp->mPlayerInfo)
		return false;

	if (theApp->mPlayerInfo->mEarnedAchievements[theAchievement])
		return false;

	theApp->mPlayerInfo->mEarnedAchievements[theAchievement] = true;

	if (!theForceGive)
		return true;

	std::string aAchievementName = gAchievementList[theAchievement].name;
	aAchievementName.append(" Achievement!");

	theApp->mBoard->DisplayAdvice(aAchievementName, MESSAGE_STYLE_ACHIEVEMENT, AdviceType::ADVICE_NONE);
	theApp->PlaySample(SOUND_ACHIEVEMENT);

	return true;
}

// GOTY @Patoke: 0x44D5B0
void ReportAchievement::AchievementInitForPlayer(GameSelector* theSelector) {
	if (!theSelector->mApp || !theSelector->mApp->mPlayerInfo)
		return;

	if (theSelector->mApp->HasFinishedAdventure()) {
		GiveAchievement(theSelector->mApp, AchievementId::HomeSecurity, true);
	}

	if (theSelector->mApp->EarnedGoldTrophy()) {
		GiveAchievement(theSelector->mApp, AchievementId::NovelPeasPrize, true);
	}

	if (theSelector->mApp->CanSpawnYetis()) {
		GiveAchievement(theSelector->mApp, AchievementId::Zombologist, true);
	}

	int aTreeSize = theSelector->mApp->mPlayerInfo->mChallengeRecords[GAMEMODE_TREE_OF_WISDOM - 1];
	if (aTreeSize >= 100) {
		GiveAchievement(theSelector->mApp, AchievementId::ToweringWisdom, true);
	}

	bool aGiveAchievement = true;
	for (int i = STORE_ITEM_PLANT_GATLINGPEA; i <= STORE_ITEM_PLANT_IMITATER; i++) {
		if (theSelector->mApp->SeedTypeAvailable(SeedType(i)))
			aGiveAchievement = false;
	}

	if (aGiveAchievement) {
		GiveAchievement(theSelector->mApp, AchievementId::Morticulturalist, aGiveAchievement);
	}
}