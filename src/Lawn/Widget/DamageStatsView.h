#pragma once
#include "OnlineUiDraw.h"
#include "CoopHud.h"
#include "RunInfoView.h"
#include "../System/DamageStats.h"
#include <algorithm>
#include <chrono>

namespace DamageStatsView
{
using namespace Sexy;

// Below the sun shortcut: keep the shovel and every seed slot unobstructed.
inline Rect BuffEntry() { return Rect(752, 88, 44, 44); }
inline RunInfoView& BuffView()
{
    // No board/run pointers are retained. Opening always resets this local-only view.
    static RunInfoView view;
    return view;
}

inline Rect SettingsEntry() { return Rect(184, 568, 54, 26); }
inline Rect TotalEntry() { return Rect(242, 568, 124, 26); }
inline Rect BossEntry() { return Rect(370, 568, 124, 26); }
inline Rect Panel() { return Rect(152, 122, 496, 398); }
inline Rect Back() { return Rect(536, 132, 96, 34); }
inline Rect Previous() { return Rect(168, 470, 116, 34); }
inline Rect Next() { return Rect(516, 470, 116, 34); }
inline Rect SettingChoice(int row, int choice) { return Rect(352 + choice * 140, 214 + row * 70, 128, 38); }

inline bool Available(Board* board)
{
    auto* app = board->mApp;
    return app->IsRunMode() && !app->AdmissionBuilding() && app->mDamageStats.mActive && app->mGameScene == SCENE_PLAYING &&
        !board->mPaused && !CoopHud::Watching(board) &&
        (CoopHud::RunInfoOpen(board) || (!board->mChatPanelOpen && board->mSunGiftSelectFrames <= 0));
}

inline std::string ShortNumber(uint64_t value)
{
    if (value < 10000) return std::to_string(value);
    const uint64_t divisor = value >= 1000000000000000000ULL ? 1000000000000000000ULL :
        value >= 1000000000000000ULL ? 1000000000000000ULL : value >= 1000000000000ULL ? 1000000000000ULL :
        value >= 1000000000ULL ? 1000000000ULL : value >= 1000000ULL ? 1000000ULL : 1000ULL;
    const char* suffix = divisor == 1000000000000000000ULL ? "E" : divisor == 1000000000000000ULL ? "P" :
        divisor == 1000000000000ULL ? "T" : divisor == 1000000000ULL ? "B" : divisor == 1000000ULL ? "M" : "K";
    return std::to_string(value / divisor) + "." + std::to_string((value % divisor) / (divisor / 10)) + suffix;
}

inline std::vector<DamageStats::Row> Rows(Board* board)
{
    auto& stats = board->mApp->mDamageStats;
    auto* session = RoomPresentation::Session(board->mApp);
    std::vector<DamageStats::Row> rows;
    if (session && board->mApp->IsOnlineGame())
    {
        for (uint8_t seat = 1; seat <= NetProto::MAX_PLAYERS; ++seat)
        {
            if (!session->IsSeatOccupied(seat)) continue;
            const uint64_t token = seat == session->GetLocalSeat() ? stats.mLocalPlayer : session->mCoopPeers[seat];
            DamageStats::Row entry;
            for (const auto& row : stats.mRows) if (token && row.mPlayer == token) entry = row;
            entry.mPlayer = token; entry.mSeat = seat; entry.mName = session->GetSeatName(seat);
            rows.push_back(entry);
        }
        for (const auto& row : stats.mRows)
        {
            const bool current = std::any_of(rows.begin(), rows.end(), [&row](const DamageStats::Row& entry) { return entry.mPlayer == row.mPlayer; });
            if (!current) rows.push_back(row);
        }
    }
    else rows = stats.mRows;
    return rows;
}

inline int Hit(Board* board, int x, int y)
{
    if (!Available(board)) return -1;
    auto& stats = board->mApp->mDamageStats;
    if (CoopHud::RunInfoOpen(board))
    {
        const int action = BuffView().Hit(x - 20, y - 20, board->mApp->GetRunState(), true, board->mApp->IsOnlineGame(), true, board->mApp->TeamProgressionAvailable());
        // The whole overlay consumes input, including gaps and the lawn behind it.
        return std::max(0, action);
    }
    if (BuffEntry().Contains(x, y)) return 5;
    if (SettingsEntry().Contains(x, y)) return 3;
    if (stats.ShowTotal() && TotalEntry().Contains(x, y)) return 1;
    if (stats.ShowBoss() && BossEntry().Contains(x, y)) return 2;
    if (!stats.mOpen) return -1;
    if (Back().Contains(x, y)) return 4;
    if (stats.mOpen == 3)
    {
        for (int row = 0; row < 3; ++row)
            for (int choice = 0; choice < 2; ++choice)
                if (SettingChoice(row, choice).Contains(x, y)) return 10 + row * 2 + choice;
    }
    else
    {
        if (Previous().Contains(x, y)) return 20;
        if (Next().Contains(x, y)) return 21;
    }
    return Panel().Contains(x, y) ? 0 : -1;
}

inline void Action(Board* board, int action)
{
    auto& stats = board->mApp->mDamageStats;
    if (CoopHud::RunInfoOpen(board))
    {
        if(action==550&&board->mApp->TeamProgressionAvailable())
        {
            stats.mOpen=0;
            board->mApp->ShowTeamProgression(TeamProgressionView::Page::TEAM_EFFECTS);
            return;
        }
        if (action == 100) stats.mOpen = 0;
        else BuffView().Act(action, board->mApp->GetRunState());
        return;
    }
    if (action == 5)
    {
        BuffView() = RunInfoView();
        BuffView().tab = 2;
        board->RefreshSeedPacketFromCursor();
        board->ClearCursor();
        stats.mOpen = CoopHud::RUN_INFO_PANEL;
        return;
    }
    if (action >= 1 && action <= 3)
    {
        stats.mOpen = stats.mOpen == action ? 0 : action;
        stats.mPage = 0;
        return;
    }
    if (action == 4) { stats.mOpen = 0; return; }
    if (action >= 10 && action <= 15)
    {
        const int row = (action - 10) / 2, choice = (action - 10) % 2;
        const int mask = 1 << row;
        const int setting = (stats.mSetting & ~mask) | (choice ? mask : 0);
        stats.mSettingsError = !stats.SaveSettings(setting);
        return;
    }
    if (action == 20 || action == 21)
    {
        const int pages = std::max(1, static_cast<int>((Rows(board).size() + 5) / 6));
        stats.mPage = (stats.mPage + pages + (action == 20 ? -1 : 1)) % pages;
    }
}

inline bool MouseDown(Board* board, int x, int y, int clicks)
{
    auto* app = board->mApp;
    if (!app->mActive || app->mMinimized) return false;
    if (clicks <= 0) return CoopHud::RunInfoOpen(board) && Available(board);
    const int action = Hit(board, x, y);
    if (action < 0) return false;
    auto& stats = app->mDamageStats;
    stats.mCaptured = true; stats.mPressed = action;
    board->mIgnoreMouseUp = true;
#ifdef __ANDROID__
    board->mTouchSeedDrag = false; board->mTouchSeedDragMoved = false;
#endif
    return true;
}

inline bool MouseUp(Board* board, int x, int y, int clicks)
{
    auto& stats = board->mApp->mDamageStats;
    if (!stats.mCaptured) return CoopHud::RunInfoOpen(board) && Available(board);
    const int action = stats.mPressed;
    stats.mCaptured = false; stats.mPressed = -1;
    board->mIgnoreMouseUp = false;
    if (clicks > 0 && board->mApp->mActive && !board->mApp->mMinimized && Hit(board, x, y) == action)
    {
        if (action > 0) board->mApp->PlaySample(SOUND_GRAVEBUTTON);
        Action(board, action);
    }
    return true;
}

inline void Button(Board* board, Graphics* graphics, Rect rectangle, const char* text, int action, bool selected = false)
{
    const auto& stats = board->mApp->mDamageStats;
    SeatUi::Button(graphics, rectangle, text, true, selected || (stats.mCaptured && stats.mPressed == action));
}

inline void Draw(Board* board, Graphics* graphics)
{
    if (!Available(board)) return;
    auto& stats = board->mApp->mDamageStats;
    if (CoopHud::RunInfoOpen(board))
    {
        Graphics panel(*graphics);
        panel.mTransX += 20 * panel.mScaleX;
        panel.mTransY += 20 * panel.mScaleY;
        BuffView().Draw(&panel, board->mApp->GetRunState(), true, board->mApp->IsOnlineGame(),
            false, false, false, "", stats.mCaptured ? stats.mPressed : -1, true, board->mApp->TeamProgressionAvailable());
        return;
    }
    CoopHud::SmallButton(graphics, BuffEntry(), ModText::Tr("增益", "Buff"), true, stats.mCaptured && stats.mPressed == 5);
    const auto* local = stats.Local();
    DamageStatsWire::Counters counters;
    if (local) counters = stats.WholeRun() ? local->mRunDamage : local->mLevelDamage;
    Button(board, graphics, SettingsEntry(), ModText::Tr("统计", "DMG"), 3, stats.mOpen == 3);
    if (stats.ShowTotal())
    {
        const auto label = std::string(ModText::Tr("伤害 ", "All ")) + ShortNumber(counters.mAll);
        Button(board, graphics, TotalEntry(), label.c_str(), 1, stats.mOpen == 1);
    }
    if (stats.ShowBoss())
    {
        const auto label = std::string("BOSS ") + ShortNumber(counters.mBoss);
        Button(board, graphics, BossEntry(), label.c_str(), 2, stats.mOpen == 2);
    }
    if (!stats.mOpen) return;
    SeatUi::Panel(graphics, Panel());
    const char* title = stats.mOpen == 3 ? ModText::Tr("高级选项 · 伤害统计", "Advanced / Damage statistics") :
        stats.mOpen == 2 ? ModText::Tr("BOSS伤害统计", "Boss damage") : ModText::Tr("总伤害统计", "Total damage");
    SeatUi::Text(graphics, title, 172, 160, 352, 24, Color(245, 230, 165));
    Button(board, graphics, Back(), ModText::Tr("返回", "Back"), 4);
    if (stats.mOpen == 3)
    {
        const char* captions[] = {ModText::Tr("统计范围", "Scope"), ModText::Tr("总统计显示", "Total display"), ModText::Tr("BOSS统计显示", "Boss display")};
        for (int row = 0; row < 3; ++row)
        {
            SeatUi::Text(graphics, captions[row], 174, 241 + row * 70, 170, 20, Color(230, 225, 205));
            for (int choice = 0; choice < 2; ++choice)
            {
                const char* label = row == 0 ? (choice ? ModText::Tr("本轮", "Run") : ModText::Tr("本关", "Level")) :
                    (choice ? ModText::Tr("开启", "On") : ModText::Tr("关闭", "Off"));
                Button(board, graphics, SettingChoice(row, choice), label, 10 + row * 2 + choice,
                    ((stats.mSetting >> row) & 1) == choice);
            }
        }
        SeatUi::Paragraph(graphics, stats.mSettingsError ? ModText::Tr("设置未保存，请检查可用存储后重试。", "Settings were not saved. Check storage and retry.") :
            ModText::Tr("仅修改本机显示；总统计包含BOSS伤害。\n关闭显示或切换范围不会清空计数。", "Local display only; total includes boss damage.\nSwitches and scope changes do not clear damage."),
            Rect(174, 426, 448, 70), 18, Color(215, 210, 190));
        return;
    }
    const auto rows = Rows(board);
    const int pages = std::max(1, static_cast<int>((rows.size() + 5) / 6));
    stats.mPage = std::min(stats.mPage, pages - 1);
    const auto scope = stats.WholeRun() ? ModText::Tr("本轮累计", "Whole run") : ModText::Tr("本关", "Current level");
    SeatUi::Text(graphics, scope, 174, 192, 440, 18, Color(215, 210, 190));
    auto* session = RoomPresentation::Session(board->mApp);
    const uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    for (int index = 0; index < 6; ++index)
    {
        const size_t position = static_cast<size_t>(stats.mPage * 6 + index);
        if (position >= rows.size()) break;
        const auto& row = rows[position];
        const bool own = stats.IsLocal(row.mPlayer);
        const bool online = session && board->mApp->IsOnlineGame();
        const bool current = own || (online && row.mPlayer && session->FindCoopSeat(row.mPlayer) != 0);
        const bool offline = online && (!current || session->IsSeatOffline(row.mSeat));
        std::string name = row.mName.empty() ? "P" + std::to_string(row.mSeat) : row.mName;
        if (own) name += ModText::Tr("（你）", " (you)");
        else if (offline) name += ModText::Tr("（离线）", " (offline)");
        else if (!row.mUpdated || now - row.mUpdated > 3000) name += ModText::Tr("（未同步）", " (stale)");
        if (row.mPartial) name += "*";
        const auto& damage = stats.WholeRun() ? row.mRunDamage : row.mLevelDamage;
        const auto number = !own && !row.mSequence && !damage.mAll ? std::string("--") :
            std::to_string(stats.mOpen == 2 ? damage.mBoss : damage.mAll);
        const int baseline = 228 + index * 36;
        SeatUi::Text(graphics, name, 174, baseline, 218, 18, Color(235, 230, 205));
        int numberSize = 18;
        while (numberSize > 10 && SeatUi::Width(number, numberSize) > 226) --numberSize;
        SeatUi::Text(graphics, number, 398, baseline, 226, numberSize, Color(175, 245, 120), true);
    }
    SeatUi::Text(graphics, ModText::Tr("* 历史不完整；离线/未同步显示最后记录", "* Partial history; offline/stale values are last known"), 174, 454, 448, 16, Color(205, 200, 180));
    Button(board, graphics, Previous(), ModText::Tr("上一页", "Previous"), 20);
    Button(board, graphics, Next(), ModText::Tr("下一页", "Next"), 21);
    SeatUi::Text(graphics, std::to_string(stats.mPage + 1) + " / " + std::to_string(pages), 300, 494, 200, 18, Color(235, 225, 190), true);
}
}
