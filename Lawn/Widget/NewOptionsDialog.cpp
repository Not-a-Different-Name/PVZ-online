#include "../Board.h"
#include "GameButton.h"
#include "../Cutscene.h"
#include "Almanac.h"
#include "../LawnCommon.h"
#include "../../LawnApp.h"
#include "../Online/NetSession.h"
#include "../System/Music.h"
#include "../../Resources.h"
#include "NewOptionsDialog.h"
#include "../../ConstEnums.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "widget/Slider.h"
#include "widget/Checkbox.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include "../Run/RunState.h"
#include "../Run/RunBuffs.h"
#include "graphics/Graphics.h"
#include "../ModText.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace Sexy;

// ── 中文绘制这一档（词条查看器用）──────────────────────────────────────
// 引擎自带的字体全是位图字体，没有中文字形，中文说明自 2026-10-03 语言批起走 ModText
// 的宽字符直绘（UTF-8 → UTF-16 → TextOutW，与系统码页脱钩）；此前这里是一份独立的
// 转码 + 字体缓存拷贝（第三份），已并入 Lawn/ModText。
// 字号 13pt = 正文（与三选一屏同档）；入口按钮 11pt——按钮列右侧那条竖带只有 ~100px 宽
//（1:1 截图实测），13pt 无论四字还是六字都放不下，见 RunInfoEntryRect。
static ModText::Font* NewOptionsCjkFont() { return ModText::GetFont(13, false); }
static ModText::Font* NewOptionsCjkFontSmall() { return ModText::GetFont(11, false); }

// ── 语言选项行（2026-10-03 语言批）──────────────────────────────────────
// 左空带竖排四行：标题 + 三档（自动/中文/English，行 1..3 与 MODLANG_* 同序）。
// 等步进 21px；命中区按行铺满整步、比字形略宽一点，点着不费劲。绘制与命中同一份几何。
static const int LANGUAGE_ROW_COUNT = 4;
static const int LANGUAGE_X = 38;
static const int LANGUAGE_TOP = 142;
static const int LANGUAGE_STEP = 21;

// 这一排文案两语并排固定、不随语言变——选择器本身得让两种语言的人都认得出，
// 所以不走 ModText::Tr（它按当前语言择一）。
static const char* NewOptionsLanguageRowText(int theRow)
{
	static const char* aRows[LANGUAGE_ROW_COUNT] = { "语言/Language", "自动/Auto", "中文", "English" };
	return (theRow >= 0 && theRow < LANGUAGE_ROW_COUNT) ? aRows[theRow] : "";
}

static Sexy::Rect NewOptionsLanguageRowRect(int theIndex)
{
	return Sexy::Rect(LANGUAGE_X - 4, LANGUAGE_TOP + theIndex * LANGUAGE_STEP - 2, 78, LANGUAGE_STEP);
}

// 一"行"文字的落点：调用方给的是行顶；ModText::DrawTextWide 也收顶对齐
//（原 DrawString 是基线口径、要补 +GetAscent()，这份换算已并入 ModText 的口径）。
static void NewOptionsDrawCjk(Sexy::Graphics* g, ModText::Font* theFont, int theX, int theTopY,
	const char* theUtf8, const Sexy::Color& theColor)
{
	ModText::DrawTextWide(g, theFont, theX, theTopY, ModText::WideFromUtf8(theUtf8), theColor, g->mClipRect);
}

static void NewOptionsDrawCjkCentered(Sexy::Graphics* g, ModText::Font* theFont, int theCenterX, int theTopY,
	const char* theUtf8, const Sexy::Color& theColor)
{
	std::wstring aText = ModText::WideFromUtf8(theUtf8);
	ModText::DrawTextWide(g, theFont, theCenterX - ModText::TextWidth(theFont, aText) / 2, theTopY,
		aText, theColor, g->mClipRect);
}

// 列宽装不下的条目：按宽字符逐字回退、接省略号——「Swift Strikes×2」宁可截名字也
// 不丢「×N」（层数是玩家最要看的一格），连「…×N」都放不下就整条不画。这里的每一行
// 都保证不越过给它的宽度（2026-10-03 1:1 截图实证：无钳制的两列流会互相压字、右列
// 冲出面板右缘被对话框边缘裁断）。宽字符化后逐字回退即 pop_back 一个 wchar_t。
static void NewOptionsDrawCjkFit(Sexy::Graphics* g, ModText::Font* theFont, int theX, int theTopY,
	int theMaxW, const char* theMainUtf8, const char* theSuffixUtf8, const Sexy::Color& theColor)
{
	std::wstring aSuffix = ModText::WideFromUtf8(theSuffixUtf8);
	std::wstring aDraw = ModText::WideFromUtf8(theMainUtf8) + aSuffix;
	if (ModText::TextWidth(theFont, aDraw) > theMaxW)
	{
		std::wstring aEll = ModText::WideFromUtf8("…");
		std::wstring aWide = ModText::WideFromUtf8(theMainUtf8);
		aDraw.clear();
		for (;;)
		{
			std::wstring aProbe = aWide + aEll + aSuffix;
			if (ModText::TextWidth(theFont, aProbe) <= theMaxW)
			{
				aDraw = aProbe;
				break;
			}
			if (aWide.empty()) break;
			aWide.pop_back();
		}
	}
	if (!aDraw.empty())
	{
		ModText::DrawTextWide(g, theFont, theX, theTopY, aDraw, theColor, g->mClipRect);
	}
}

//0x45C050
NewOptionsDialog::NewOptionsDialog(LawnApp* theApp, bool theFromGameSelector) : 
	Dialog(nullptr, nullptr, Dialogs::DIALOG_NEWOPTIONS, true, _S("Options"), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
    mApp = theApp;
    mFromGameSelector = theFromGameSelector;
    mRunInfoOpen = false;
    for (int i = 0; i < 8; i++)
    {
        mRunInfoWidgetVis[i] = false;
    }
    SetColor(Dialog::COLOR_BUTTON_TEXT, Color(255, 255, 100));
    mAlmanacButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Almanac, this, _S("[VIEW_ALMANAC_BUTTON]"));
    mRestartButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Restart, this, _S("[RESTART_LEVEL_BUTTON]")); // @Patoke: wrong local name
    mBackToMainButton = MakeButton(NewOptionsDialog::NewOptionsDialog_MainMenu, this, _S("[MAIN_MENU_BUTTON]"));

    mBackToGameButton = MakeNewButton(
        Dialog::ID_OK, 
        this, 
        _S("[BACK_TO_GAME]"), 
        nullptr, 
        IMAGE_OPTIONS_BACKTOGAMEBUTTON0, 
        IMAGE_OPTIONS_BACKTOGAMEBUTTON0, 
        IMAGE_OPTIONS_BACKTOGAMEBUTTON2
    );
    mBackToGameButton->mTranslateX = 0;
    mBackToGameButton->mTranslateY = 0;
    mBackToGameButton->mTextOffsetX = -2;
    mBackToGameButton->mTextOffsetY = -5;
    mBackToGameButton->mTextDownOffsetX = 0;
    mBackToGameButton->mTextDownOffsetY = 1;
    mBackToGameButton->SetFont(FONT_DWARVENTODCRAFT36GREENINSET);
    mBackToGameButton->SetColor(ButtonWidget::COLOR_LABEL, Color::White);
    mBackToGameButton->SetColor(ButtonWidget::COLOR_LABEL_HILITE, Color::White);
    mBackToGameButton->mHiliteFont = FONT_DWARVENTODCRAFT36BRIGHTGREENINSET;
    
    mMusicVolumeSlider = new Slider(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_MusicVolume, this);
    double aMusicVolume = theApp->GetMusicVolume();
    aMusicVolume = std::max(0.0, std::min(1.0, aMusicVolume));
    mMusicVolumeSlider->SetValue(aMusicVolume);

    mSfxVolumeSlider = new Slider(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_SoundVolume, this);
    mSfxVolumeSlider->SetValue(theApp->GetSfxVolume() / 0.65);

    mFullscreenCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_Fullscreen, this, !theApp->mIsWindowed);
    mHardwareAccelerationCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_HardwareAcceleration, this, theApp->Is3DAccelerated());

    if (mFromGameSelector)
    {
        mRestartButton->SetVisible(false);
        mBackToGameButton->SetLabel(_S("[DIALOG_BUTTON_OK]"));
        if (mApp->HasFinishedAdventure() && !mApp->IsTrialStageLocked())
        {
            mBackToMainButton->SetLabel(_S("[CREDITS]"));
        }
        else
        {
            mBackToMainButton->SetVisible(false);
        }
    }

    if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE || 
        mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || 
        mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
    {
        mRestartButton->SetVisible(false);
    }
    if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && !mApp->mBoard->mCutScene->IsSurvivalRepick())
    {
        mRestartButton->SetVisible(false);
    }
    if (!mApp->CanShowAlmanac() ||
        mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO ||
        mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
        mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
        mFromGameSelector)
    {
        mAlmanacButton->SetVisible(false);
    }

    // @pvz-online: 联机局不给重开——一边重开、另一边还在打，两边棋盘就对不上了。
    // 这条与上面几条并列：不管走哪个分支进来，最后都以联机为准。
    if (mApp->IsOnlineGame())
    {
        mRestartButton->SetVisible(false);
    }
}

//0x45C760、0x45C780
NewOptionsDialog::~NewOptionsDialog()
{
    delete mMusicVolumeSlider;
    delete mSfxVolumeSlider;
    delete mFullscreenCheckbox;
    delete mHardwareAccelerationCheckbox;
    delete mAlmanacButton;
    delete mRestartButton;
    delete mBackToMainButton;
    delete mBackToGameButton;
}

//0x45C880
int NewOptionsDialog::GetPreferredHeight(int theWidth)
{
    (void)theWidth;
    return IMAGE_OPTIONS_MENUBACK->mWidth;
}

//0x45C890
void NewOptionsDialog::AddedToManager(Sexy::WidgetManager* theWidgetManager)
{
    Dialog::AddedToManager(theWidgetManager);
    AddWidget(mAlmanacButton);
    AddWidget(mRestartButton);
    AddWidget(mBackToMainButton);
    AddWidget(mMusicVolumeSlider);
    AddWidget(mSfxVolumeSlider);
    AddWidget(mHardwareAccelerationCheckbox);
    AddWidget(mFullscreenCheckbox);
    AddWidget(mBackToGameButton);
}

//0x45C930
void NewOptionsDialog::RemovedFromManager(Sexy::WidgetManager* theWidgetManager)
{
    Dialog::RemovedFromManager(theWidgetManager);
    RemoveWidget(mAlmanacButton);
    RemoveWidget(mMusicVolumeSlider);
    RemoveWidget(mSfxVolumeSlider);
    RemoveWidget(mFullscreenCheckbox);
    RemoveWidget(mHardwareAccelerationCheckbox);
    RemoveWidget(mBackToMainButton);
    RemoveWidget(mBackToGameButton);
    RemoveWidget(mRestartButton);
}

//0x45C9D0
void NewOptionsDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
    Dialog::Resize(theX, theY, theWidth, theHeight);
    mMusicVolumeSlider->Resize(199, 116, 135, 40);
    mSfxVolumeSlider->Resize(199, 143, 135, 40);
    mHardwareAccelerationCheckbox->Resize(283, 175, 46, 45);
    mFullscreenCheckbox->Resize(284, 206, 46, 45);
    mAlmanacButton->Resize(107, 241, 209, 46);
    mRestartButton->Resize(mAlmanacButton->mX, mAlmanacButton->mY + 43, 209, 46);
    mBackToMainButton->Resize(mRestartButton->mX, mRestartButton->mY + 43, 209, 46);
    mBackToGameButton->Resize(30, 381, mBackToGameButton->mWidth, mBackToGameButton->mHeight);

    if (mFromGameSelector)
    {
        mMusicVolumeSlider->mY += 5;
        mSfxVolumeSlider->mY += 10;
        mHardwareAccelerationCheckbox->mY += 15;
        mFullscreenCheckbox->mY += 20;
    }

    if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
    {
        mAlmanacButton->mY += 43;
    }
}

//0x45CB50
void NewOptionsDialog::Draw(Sexy::Graphics* g)
{
    g->DrawImage(IMAGE_OPTIONS_MENUBACK, 0, 0);

    int aMusicOffset = 0;
    int aSfxOffset = 0;
    int a3DAccelOffset = 0;
    int aFullScreenOffset = 0;
    if (mFromGameSelector)
    {
        aMusicOffset = 5;
        aSfxOffset = 10;
        a3DAccelOffset = 15;
        aFullScreenOffset = 20;
    }
    Sexy::Color aTextColor(107, 109, 145);

    TodDrawString(g, _S("Music"), 186, 140 + aMusicOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
    TodDrawString(g, _S("Sound FX"), 186, 167 + aSfxOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
    TodDrawString(g, _S("3D Acceleration"), 274, 197 + a3DAccelOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
    TodDrawString(g, _S("Full Screen"), 274, 229 + aFullScreenOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);

    // @pvz-online: 语言选项行（2026-10-03 语言批）。左空带竖排四行：标题 + 三档（自动/中文/
    // English），当前档高亮；点击存注册表（ModText::SetLanguageSetting）并即时生效。
    // 这一排文案故意**两语并排固定**、不随语言变——选择器本身得让两种语言的人都认得出。
    // 落点与命中同用 LanguageRowRect（1:1 截图实测 x≈30..115 为空带，四行原版选项的
    // 标签右对齐到 186/274、最长文字也从 ~120 起；滑块从 199 起）。
    {
        ModText::Font* aLangFont = NewOptionsCjkFontSmall();
        int aLangSel = ModText::GetLanguageSetting() + 1;   // 0 自动 / 1 中文 / 2 英文 → 行 1..3
        for (int i = 0; i < LANGUAGE_ROW_COUNT; i++)
        {
            NewOptionsDrawCjk(g, aLangFont, LANGUAGE_X, LANGUAGE_TOP + i * LANGUAGE_STEP,
                NewOptionsLanguageRowText(i),
                i == aLangSel ? Sexy::Color(255, 230, 120) : aTextColor);
        }
    }

    // @pvz-online: 这次暂停是队友按的，得说清楚——不然玩家会以为自己误触了。
    // 标题是烤进背景图里的，面板上半（y≈105 起）空着，就在这画一行。
    if (!mFromGameSelector && mApp->mOnlineSession && mApp->mOnlineSession->IsPausedByPeer())
    {
        TodDrawString(g, _S("Teammate paused the game"), mWidth / 2, 112,
            FONT_DWARVENTODCRAFT18, Sexy::Color(255, 220, 100), DrawStringJustification::DS_ALIGN_CENTER);
    }

    // @pvz-online: 词条查看器入口（2026-10-03）：只在闯关局出现——主菜单上的暂停面板没有"本局"。
    if (RunInfoAvailable() && !mRunInfoOpen)
    {
        Sexy::Rect aEntry = RunInfoEntryRect();
        ModText::Font* aFont = NewOptionsCjkFontSmall();
        g->SetColor(Sexy::Color(0, 0, 0, 150));
        g->FillRect(aEntry.mX, aEntry.mY, aEntry.mWidth, aEntry.mHeight);
        g->SetColor(Sexy::Color(255, 220, 100, 170));
        g->DrawRect(aEntry);
        NewOptionsDrawCjkCentered(g, aFont, aEntry.mX + aEntry.mWidth / 2, aEntry.mY + 4,
            "本局词条", Sexy::Color(255, 220, 100));
    }
    if (mRunInfoOpen)
    {
        DrawRunInfo(g);
    }
}

//0x45CF50
void NewOptionsDialog::SliderVal(int theId, double theVal)
{
    switch (theId)
    {
    case NewOptionsDialog::NewOptionsDialog_MusicVolume:
        mApp->SetMusicVolume(theVal);
        mApp->mSoundSystem->RehookupSoundWithMusicVolume();
        break;

    case NewOptionsDialog::NewOptionsDialog_SoundVolume:
        mApp->SetSfxVolume(theVal * 0.65);
        mApp->mSoundSystem->RehookupSoundWithMusicVolume();
        if (!mSfxVolumeSlider->mDragging)
        {
            mApp->PlaySample(SOUND_BUTTONCLICK);
        }
        break;
    }
}

//0x45CFF0
void NewOptionsDialog::CheckboxChecked(int theId, bool checked)
{
    switch (theId)
    {
    case NewOptionsDialog::NewOptionsDialog_Fullscreen:
        if (!checked && mApp->mForceFullscreen)
        {
            mApp->DoDialog(
                Dialogs::DIALOG_COLORDEPTH_EXP, 
                true, 
                _S("No Windowed Mode"), 
                _S( "Windowed mode is only available if your desktop was running in either\n"
                    "16 bit or 32 bit color mode when you started the game.\n\n"
                    "If you'd like to run in Windowed mode then you need to quit the game and switch your desktop to 16 or 32 bit color mode."), 
                _S("OK"), 
                Dialog::BUTTONS_FOOTER
            );

            mFullscreenCheckbox->SetChecked(true, false);
        }
        break;

    case NewOptionsDialog::NewOptionsDialog_HardwareAcceleration:
        if (checked)
        {
            if (!mApp->Is3DAccelerationSupported())
            {
                mHardwareAccelerationCheckbox->SetChecked(false, false);
                mApp->DoDialog(
                    Dialogs::DIALOG_INFO,
                    true,
                    _S("Not Supported"),
                    _S( "Hardware Acceleration cannot be enabled on this computer.\n\n"
                        "Your video card does not\n"
                        "meet the minimum requirements\n"
                        "for this game."),
                    _S("OK"),
                    Dialog::BUTTONS_FOOTER
                );
            }
            else if (!mApp->Is3DAccelerationRecommended())
            {
                mApp->DoDialog(
                    Dialogs::DIALOG_INFO,
                    true,
                    _S("Warning"),
                    _S( "Your video card may not fully support this feature.\n\n"
                        "If you experience slower performance, please disable Hardware Acceleration.\n"),
                    _S("OK"),
                    Dialog::BUTTONS_FOOTER
                );
            }
        }
        break;
    }
}

//0x45D290
void NewOptionsDialog::KeyDown(Sexy::KeyCode theKey)
{
    // @pvz-online: 查看器开着时吞掉一切键——任意键只关它（别穿透到下一条 SPACE=继续 / ESC=关面板）
    if (mRunInfoOpen)
    {
        CloseRunInfo();
        return;
    }

    if (mApp->mBoard)
    {
        mApp->mBoard->DoTypingCheck(theKey);
    }

    if (theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN)
    {
        Dialog::ButtonDepress(Dialog::ID_OK);
    }
    else if (theKey == KeyCode::KEYCODE_ESCAPE)
    {
        Dialog::ButtonDepress(Dialog::ID_CANCEL);
    }
}

//0x45D2F0
void NewOptionsDialog::ButtonPress(int theId)
{
    (void)theId;
    mApp->PlaySample(SOUND_GRAVEBUTTON);
}

//0x45D310
void NewOptionsDialog::ButtonDepress(int theId)
{
    Dialog::ButtonDepress(theId);

    switch (theId)
    {
    case NewOptionsDialog::NewOptionsDialog_Almanac:
    {
        AlmanacDialog* aDialog = mApp->DoAlmanacDialog(SeedType::SEED_NONE, ZombieType::ZOMBIE_INVALID);
        aDialog->WaitForResult(true);
        break;
    }

    case NewOptionsDialog::NewOptionsDialog_MainMenu:
    {
        if (mFromGameSelector)
        {
            mApp->KillNewOptionsDialog();
            mApp->KillGameSelector();
            mApp->ShowAwardScreen(AwardType::AWARD_CREDITS_ZOMBIENOTE, false);
        }
        else if (mApp->mBoard && mApp->mBoard->NeedSaveGame())
        {
            mApp->DoConfirmBackToMain();
        }
        else if (mApp->mBoard && mApp->mBoard->mCutScene && mApp->mBoard->mCutScene->IsSurvivalRepick())
        {
            mApp->DoConfirmBackToMain();
        }
        else
        {
            mApp->mBoardResult = BoardResult::BOARDRESULT_QUIT;
            mApp->DoBackToMain();
        }
        break;
    }

    case NewOptionsDialog::NewOptionsDialog_Restart:
    {
        // @pvz-online: 联机局不给重开（按钮在构造里已经藏起来了，这是兜底）
        if (mApp->IsOnlineGame())
        {
            break;
        }

        if (mApp->mBoard)
        {
            SexyString aDialogTitle;
            SexyString aDialogMessage;
            if (mApp->IsPuzzleMode())
            {
                aDialogTitle = _S("[RESTART_PUZZLE_HEADER]");
                aDialogMessage = _S("[RESTART_PUZZLE_BODY]");
            }
            else if (mApp->IsChallengeMode())
            {
                aDialogTitle = _S("[RESTART_CHALLENGE_HEADER]");
                aDialogMessage = _S("[RESTART_CHALLENGE_BODY]");
            }
            else if (mApp->IsSurvivalMode())
            {
                aDialogTitle = _S("[RESTART_SURVIVAL_HEADER]");
                aDialogMessage = _S("[RESTART_SURVIVAL_BODY]");
            }
            else
            {
                aDialogTitle = _S("[RESTART_LEVEL_HEADER]");
                aDialogMessage = _S("[RESTART_LEVEL_BODY]");
            }

            LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(Dialogs::DIALOG_CONFIRM_RESTART, true, aDialogTitle, aDialogMessage, _S(""), Dialog::BUTTONS_YES_NO);
            aDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[RESTART_LEVEL_BUTTON]"));
            aDialog->mLawnNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_CANCEL]"));
            
            if (aDialog->WaitForResult(true) == Dialog::ID_YES)
            {
                mApp->mMusic->StopAllMusic();
                mApp->mSoundSystem->CancelPausedFoley();
                mApp->KillNewOptionsDialog();
                mApp->mBoardResult = BoardResult::BOARDRESULT_RESTART;
                mApp->mSawYeti = mApp->mBoard->mKilledYeti;
                mApp->PreNewGame(mApp->mGameMode, false);
            }
        }
        break;
    }

    case NewOptionsDialog::NewOptionsDialog_Update:
        mApp->CheckForUpdates();
        break;
    }
}

// ── 局内词条查看器（2026-10-03 用户定案）──────────────────────────────
// 入口画在按钮列右侧的空白竖带，点开是整屏覆盖层：模式/关卡/出怪档 + 全局增益 + 单株强化，
// 单击任意处或按任意键关闭。数据全来自 mApp->mRunState；全局增益 id < RUN_BUFF_COUNT，
// 单株升级 id − RUN_BUFF_COUNT 是单株表下标（表满编后 == SeedType，仍按表查，别写死）。

bool NewOptionsDialog::RunInfoAvailable()
{
    // 主菜单上的暂停面板没有"本局"这个概念；闯关局（单机/联机都算）才有 mRunState
    return !mFromGameSelector && mApp->IsRunMode() && mApp->mRunState != NULL;
}

Sexy::Rect NewOptionsDialog::RunInfoEntryRect()
{
    // 按钮列（子控件画在最上层）右边到墓碑右缘之间这条竖带：1:1 截图实测约 100px 宽
    //（shot 495..595）。原来 13pt 六字「查看本局词条」有 185px 宽，左半段被按钮板子
    // 盖掉、只剩右缘一截，所以改成 11pt 四字、尺寸按实际文本量出来。往左别越过 495：
    // 按钮列右缘约在 shot 490。
    ModText::Font* aFont = NewOptionsCjkFontSmall();
    int aWidth = ModText::TextWidth(aFont, ModText::WideFromUtf8(ModText::Tr("本局词条", "This Run"))) + 14;
    int aHeight = ModText::LineHeight(aFont) + 10;
    return Sexy::Rect(mWidth - 16 - aWidth, 258, aWidth, aHeight);
}

// 开/关时把八个控件整体藏起、按快照还原：覆盖层期间它们一个都不该被点到、被画出来。
// 顺序数组两边共用，改动控件集合只改这一处的两处拷贝（Open/Close 各一份）。
void NewOptionsDialog::OpenRunInfo()
{
    Sexy::Widget* aWidgets[8] =
    {
        mMusicVolumeSlider, mSfxVolumeSlider, mFullscreenCheckbox, mHardwareAccelerationCheckbox,
        mAlmanacButton, mRestartButton, mBackToMainButton, mBackToGameButton,
    };
    for (int i = 0; i < 8; i++)
    {
        mRunInfoWidgetVis[i] = aWidgets[i]->mVisible;
        aWidgets[i]->SetVisible(false);
    }
    mRunInfoOpen = true;
}

void NewOptionsDialog::CloseRunInfo()
{
    Sexy::Widget* aWidgets[8] =
    {
        mMusicVolumeSlider, mSfxVolumeSlider, mFullscreenCheckbox, mHardwareAccelerationCheckbox,
        mAlmanacButton, mRestartButton, mBackToMainButton, mBackToGameButton,
    };
    for (int i = 0; i < 8; i++)
    {
        aWidgets[i]->SetVisible(mRunInfoWidgetVis[i]);
    }
    mRunInfoOpen = false;
}

void NewOptionsDialog::MouseDown(int x, int y, int theClickCount)
{
    // @pvz-online: 查看器开着时单击任意处 = 关（点开自己那一下也不再传给下面的按钮）
    if (mRunInfoOpen)
    {
        mApp->PlaySample(SOUND_GRAVEBUTTON);
        CloseRunInfo();
        return;
    }
    if (RunInfoAvailable() && RunInfoEntryRect().Contains(x, y))
    {
        mApp->PlaySample(SOUND_GRAVEBUTTON);
        OpenRunInfo();
        return;
    }
    // @pvz-online: 语言选项行（行 0 是标题不响应；三档命中即存注册表 + 石头按钮音）。
    // 生效范围：本面板的"本局词条"入口 / 查看器下一帧就换语言（ModText 每次现读），
    // 其余界面各自的文案在下次打开时才取——不必重建任何东西。
    for (int i = 1; i < LANGUAGE_ROW_COUNT; i++)
    {
        if (NewOptionsLanguageRowRect(i).Contains(x, y))
        {
            mApp->PlaySample(SOUND_GRAVEBUTTON);
            ModText::SetLanguageSetting(i - 1);
            return;
        }
    }
    Dialog::MouseDown(x, y, theClickCount);
}

void NewOptionsDialog::DrawRunInfo(Sexy::Graphics* g)
{
    // 整屏压暗：控件已全藏，这一层把"屏幕只剩词条"说清楚；底下的背景照旧透个轮廓
    g->SetColor(Sexy::Color(0, 0, 0, 200));
    g->FillRect(0, 0, mWidth, mHeight);

    RunState* aRun = mApp->mRunState;
    ModText::Font* aFont = NewOptionsCjkFont();
    if (aRun == NULL)
    {
        return;
    }

    int aPanelX = 14;
    int aPanelY = 14;
    int aPanelW = mWidth - 28;
    int aPanelH = mHeight - 28;
    g->SetColor(Sexy::Color(24, 44, 28));
    g->FillRect(aPanelX, aPanelY, aPanelW, aPanelH);
    g->SetColor(Sexy::Color(255, 220, 100, 160));
    g->DrawRect(Sexy::Rect(aPanelX, aPanelY, aPanelW, aPanelH));

    int aLineHeight = ModText::LineHeight(aFont) + 4;
    int aCenterX = mWidth / 2;
    int aY = aPanelY + 12;

    NewOptionsDrawCjkCentered(g, aFont, aCenterX, aY, ModText::Tr("本局词条", "This Run"), Sexy::Color(255, 220, 100));
    aY += aLineHeight;

    const char* aModeName = (aRun->mMode == RunState::RUN_MODE_NORMAL) ? ModText::Tr("普通版", "Normal")
        : (aRun->mMode == RunState::RUN_MODE_QUICK) ? ModText::Tr("快速版", "Quick") : ModText::Tr("完整版", "Full");
    const char* aDiffName = (aRun->mDiff == RunState::RUN_DIFF_EASY) ? ModText::Tr("轻松", "Easy")
        : (aRun->mDiff == RunState::RUN_DIFF_HIGH) ? ModText::Tr("高压", "High") : ModText::Tr("标准", "Standard");
    char aSubLine[160];
    snprintf(aSubLine, sizeof(aSubLine), ModText::Tr("%s · 第 %d/%d 关 · 出怪：%s", "%s · Level %d/%d · Spawns: %s"),
        aModeName, aRun->GetPlayingLevelIndex() + 1, aRun->GetLevelCount(), aDiffName);
    NewOptionsDrawCjkCentered(g, aFont, aCenterX, aY, aSubLine, Sexy::Color(200, 200, 200));
    aY += aLineHeight + 8;

    // 列几何按 1:1 截图实测定（2026-10-03）：13pt 下 CJK ≈26px、拉丁 ≈11-13px/字符，
    // 最长全局条目「Swift Strikes×2」≈180px。列步进 190、每列可写 182——两列间 8px
    // 空隙、右列距面板右缘 ≥10px；仍装不下的由 NewOptionsDrawCjkFit 截名保计数。
    int aColX[2] = { aPanelX + 12, aPanelX + 12 + 190 };
    int aColW = 182;
    int aBottom = aPanelY + aPanelH - 14 - aLineHeight;   // 给底部提示留一行
    Sexy::Color anEntryColor(232, 232, 232);

    // 【全局增益】：名称×层数，两列流
    NewOptionsDrawCjk(g, aFont, aColX[0], aY, ModText::Tr("【全局增益】", "[Global Boosts]"), Sexy::Color(150, 224, 150));
    aY += aLineHeight;
    int aShown = 0;
    bool anOverflow = false;
    for (int i = 0; i < RUN_BUFF_COUNT; i++)
    {
        int aStacks = aRun->GetBuffCount(i);
        if (aStacks <= 0) continue;
        if (aShown % 2 == 0 && aY > aBottom) { anOverflow = true; break; }
        char aSuffix[16];
        snprintf(aSuffix, sizeof(aSuffix), "×%d", aStacks);
        NewOptionsDrawCjkFit(g, aFont, aColX[aShown % 2], aY, aColW,
            GetRunBuffDef(i).mName, aSuffix, anEntryColor);
        if (aShown % 2 == 1) aY += aLineHeight;
        aShown++;
    }
    if (aShown % 2 == 1) aY += aLineHeight;
    if (aShown == 0 || anOverflow)
    {
        NewOptionsDrawCjk(g, aFont, aColX[0], aY, ModText::Tr(anOverflow ? "……" : "（无）", anOverflow ? "..." : "(none)"), Sexy::Color(150, 150, 150));
        aY += aLineHeight;
    }
    aY += 8;

    // 【单株强化】：植物名 + 词条名×层数，整幅宽单列——「植物名+英文词条名+×N」最长
    // 约 310px（玉米加农炮 Rapid Reload×3），两列 182px 根本装不下：截图实证会互相
    // 压字、右列冲出面板右缘被对话框边缘裁断。单列 372px 全放得下，一格一行也更易读。
    NewOptionsDrawCjk(g, aFont, aColX[0], aY, ModText::Tr("【单株强化】", "[Plant Upgrades]"), Sexy::Color(150, 224, 150));
    aY += aLineHeight;
    aShown = 0;
    anOverflow = false;
    int aFullW = aColX[1] + aColW - aColX[0];
    for (int i = 0; i < (int)aRun->mBuffs.size(); i++)
    {
        if (aRun->mBuffs[i].mId < RUN_BUFF_COUNT || aRun->mBuffs[i].mCount == 0) continue;
        if (aY > aBottom) { anOverflow = true; break; }
        const RunPlantUpgradeDef& aDef = GetRunPlantUpgradeDef(aRun->mBuffs[i].mId - RUN_BUFF_COUNT);
        const char* aPlantName = GetRunPlantName(aDef.mPlant);
        char aText[128];
        char aSuffix[16];
        snprintf(aText, sizeof(aText), "%s %s", aPlantName != NULL ? aPlantName : "?",
            GetRunChoiceName(aRun->mBuffs[i].mId));
        snprintf(aSuffix, sizeof(aSuffix), "×%d", (int)aRun->mBuffs[i].mCount);
        NewOptionsDrawCjkFit(g, aFont, aColX[0], aY, aFullW, aText, aSuffix, anEntryColor);
        aY += aLineHeight;
        aShown++;
    }
    if (aShown == 0 || anOverflow)
    {
        NewOptionsDrawCjk(g, aFont, aColX[0], aY, ModText::Tr(anOverflow ? "……" : "（无）", anOverflow ? "..." : "(none)"), Sexy::Color(150, 150, 150));
    }

    NewOptionsDrawCjkCentered(g, aFont, aCenterX, aPanelY + aPanelH - 8 - ModText::LineHeight(aFont),
        ModText::Tr("单击任意处或按任意键关闭", "Click anywhere or press any key to close"), Sexy::Color(160, 160, 160));
}
