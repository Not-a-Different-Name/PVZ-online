// Gardens are local lobbies for invitations, never unfinished combat lawns.
bool LawnApp::IsOnlineStartLobby() const
{
    return (mGameScene == GameScenes::SCENE_MENU && mBoard == nullptr) ||
        (mBoard != nullptr && (mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
            mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM));
}

// A read-only spectator screen is not a departing battle. Rebuild only
// its menu surface; ShowGameSelector would discard an already accepted START,
// RUN_GO and preparation transaction received in the same network update.
void LawnApp::RestoreOnlineStartLobby()
{
    if (mBoard != nullptr) return;
    mGameScene = GameScenes::SCENE_MENU;
    if (mGameSelector != nullptr) return;
    mGameSelector = std::make_unique<GameSelector>(this);
    mGameSelector->Resize(0, 0, mWidth, mHeight);
    mWidgetManager->AddWidget(mGameSelector.get());
    mWidgetManager->BringToBack(mGameSelector.get());
    mWidgetManager->SetFocus(mGameSelector.get());
}

void LawnApp::LeaveLocalSceneForOnlineStart()
{
    if (mBoard == nullptr || (mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN &&
        mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM)) return;

    mMusic->StopAllMusic();
    KillNewOptionsDialog();
    // Dispose under the old mode so LeaveGarden/TreeOfWisdomLeave settle local state.
    KillBoard();
    WriteCurrentUserConfig();
    mBoardResult = BoardResult::BOARDRESULT_NONE;
    mGameScene = GameScenes::SCENE_MENU;
    mRunIntroHeld = false;
    mOnlineSeedsHeld = false;
    // Do not rebuild the menu: that would discard this START/ACK/RUN_GO exchange.
}
