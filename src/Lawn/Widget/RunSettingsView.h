#pragma once
#include "OnlineUiDraw.h"
#include "../Run/RunState.h"
#include <vector>

class RunSettingsView {
public:
    struct Entry {
        std::string label;
        std::string value;
    };
    int page = 0;
    static constexpr int ROWS = 7;

    static Sexy::Rect BackRect() { return Sexy::Rect(612, 26, 124, 44); }
    static Sexy::Rect PreviousRect() { return Sexy::Rect(36, 486, 140, 44); }
    static Sexy::Rect NextRect() { return Sexy::Rect(584, 486, 140, 44); }
    static Sexy::Rect RowRect(int index) { return Sexy::Rect(36, 124 + index * 44, 688, 40); }

    template<size_t count>
    static const char* Choice(int value, const char* const (&labels)[count], int fallback = 0) {
        return labels[value >= 0 && value < static_cast<int>(count) ? value : fallback];
    }
    static const char* Toggle(bool enabled) {
        return enabled ? ModText::Tr("开启", "On") : ModText::Tr("关闭", "Off");
    }
    static std::vector<Entry> Entries(const RunState* run) {
        if (!run) return {};
        const char* modes[] = {ModText::Tr("完整闯关（25关）", "Full run (25 stages)"),
            ModText::Tr("普通闯关（10关）", "Normal run (10 stages)"),
            ModText::Tr("快速闯关（5关）", "Quick run (5 stages)"), ModText::Tr("无尽", "Endless")};
        const char* scenes[] = {ModText::Tr("白天", "Day"), ModText::Tr("夜晚", "Night"),
            ModText::Tr("泳池", "Pool"), ModText::Tr("迷雾", "Fog"), ModText::Tr("屋顶", "Roof")};
        const char* difficulty[] = {ModText::Tr("轻松 x0.5", "Easy x0.5"),
            ModText::Tr("标准 x1", "Standard x1"), ModText::Tr("高压 x2", "Hard x2")};
        const char* scale[] = {"x0.5", "x1", "x2", "x4"};
        const char* tempo[] = {ModText::Tr("快 x0.6", "Fast x0.6"),
            ModText::Tr("标准 x1", "Standard x1"), ModText::Tr("慢 x1.6", "Slow x1.6")};
        const char* firstWave[] = {ModText::Tr("30秒", "30 sec"),
            ModText::Tr("18秒", "18 sec"), ModText::Tr("60秒", "60 sec")};
        const char* bosses[] = {ModText::Tr("关闭", "Off"), ModText::Tr("仅末关", "Final only"),
            ModText::Tr("末关 + 场景3", "Final + Scene 3")};
        const char* ladder[] = {"x1.33", "x1", "x1.2", "x1.5", "x2"};
        const char* speed[] = {ModText::Tr("关闭", "Off"), "x1.03", "x1.05"};
        const char* zombieBuffs[] = {ModText::Tr("关闭", "Off"),
            ModText::Tr("1条", "1 buff"), ModText::Tr("2条", "2 buffs")};
        std::vector<Entry> entries = {{ModText::Tr("模式", "Mode"), Choice(run->mMode, modes)}};
        if (run->IsEndless()) {
            entries.push_back({ModText::Tr("无尽规则", "Endless rules"), run->IsNewEndless() ?
                ModText::Tr("新版", "Current") : ModText::Tr("旧版（沿用存档规则）", "Legacy (saved rules)")});
            entries.push_back({ModText::Tr("开局场景", "Starting scene"), Choice(run->mEndlessScene, scenes)});
        }
        entries.push_back({ModText::Tr("出怪难度", "Spawn difficulty"), Choice(run->mDiff, difficulty, 1)});
        entries.push_back({ModText::Tr("出怪规模", "Spawn scale"), Choice(run->mScale, scale, 1)});
        entries.push_back({ModText::Tr("波间隔节奏", "Wave interval"), Choice(run->mTempo, tempo, 1)});
        entries.push_back({ModText::Tr("首波出怪", "First wave delay"), Choice(run->mFirstWave, firstWave)});
        entries.push_back({ModText::Tr("植物僵尸", "ZomBotany"), Toggle(run->mZombotany != 0)});
        if (run->IsEndless()) {
            const int period = run->EndlessBossPeriod();
            entries.push_back({ModText::Tr("周期BOSS", "Boss cycle"), period == 5 ?
                ModText::Tr("每五关", "Every 5 stages") : period == 10 ?
                ModText::Tr("每十关", "Every 10 stages") : ModText::Tr("关闭", "Off")});
        } else {
            entries.push_back({ModText::Tr("巨型BOSS", "Giant Boss"), Choice(run->GiantBossTier(), bosses)});
            entries.push_back({ModText::Tr("僵王终章", "Zomboss finale"),
                (run->mBossFlag & RunState::BOSS_FINALE) ? ModText::Tr("开启", "On") : ModText::Tr("关闭", "Off")});
        }
        entries.push_back({ModText::Tr("增益倍选（屏数）", "Buff pick screens"),
            "x" + std::to_string(RunState::BuffMultValueFor(run->mBuffMult))});
        if (!run->IsEndless())
            entries.push_back({ModText::Tr("场景难度阶梯", "Scene difficulty ladder"), Choice(run->mLadder, ladder)});
        entries.push_back({run->IsEndless() ? ModText::Tr("移速成长（每五关）", "Speed growth / 5 stages") :
            ModText::Tr("移速成长（每新场景）", "Speed growth / new scene"), Choice(run->mSpeedTier, speed)});
        entries.push_back({run->IsEndless() ? ModText::Tr("僵尸强化（每五关）", "Zombie buffs / 5 stages") :
            ModText::Tr("僵尸强化（每新场景）", "Zombie buffs / new scene"),
            Choice(run->GetZombieBuffPerScene(), zombieBuffs)});
        if (run->IsEndless()) {
            entries.push_back({ModText::Tr("阶段主题", "Stage themes"),
                Toggle(run->IsNewEndless() && (run->mEndlessOptions & 4))});
            entries.push_back({ModText::Tr("阵容偏好", "Lineup preference"), Toggle(run->HasLineupPreference())});
        }
        return entries;
    }
    static int PageCount(const RunState* run) {
        return std::max(1, (static_cast<int>(Entries(run).size()) + ROWS - 1) / ROWS);
    }
    void Flip(int delta, const RunState* run) {
        page = std::max(0, std::min(PageCount(run) - 1, page + delta));
    }
    int Hit(int x, int y, const RunState* run) const {
        if (BackRect().Contains(x, y)) return 100;
        if (page > 0 && PreviousRect().Contains(x, y)) return 101;
        if (page + 1 < PageCount(run) && NextRect().Contains(x, y)) return 102;
        return -1;
    }
    void Act(int action, const RunState* run) {
        if (action == 101 || action == 102) Flip(action == 101 ? -1 : 1, run);
    }
    void Draw(Sexy::Graphics* graphics, const RunState* run, bool online, int down) {
        using namespace Sexy;
        const Color title(245, 230, 165), body(235, 230, 210), muted(190, 190, 175);
        SeatUi::Panel(graphics, Rect(0, 0, 760, 560));
        SeatUi::Text(graphics, ModText::Tr("开局设定", "Starting settings"), 36, 60, 550, 28, title);
        SeatUi::Button(graphics, BackRect(), ModText::Tr("返回", "Back"), true, down == 100);
        SeatUi::Text(graphics, online ? ModText::Tr("本局已同步的房主设定（只读）", "This run's synced host settings (read-only)") :
            ModText::Tr("本局开局时选择的设定（只读）", "Settings chosen for this run (read-only)"), 36, 104, 688, 19, muted);
        auto entries = Entries(run);
        page = std::max(0, std::min(PageCount(run) - 1, page));
        for (int index = 0; index < ROWS && page * ROWS + index < static_cast<int>(entries.size()); ++index) {
            const auto& entry = entries[page * ROWS + index];
            const auto box = RowRect(index);
            SeatUi::Strip(graphics, box);
            SeatUi::Text(graphics, entry.label, box.mX + 12, box.mY + 27, 296, 18, body);
            SeatUi::Text(graphics, entry.value, box.mX + 324, box.mY + 27, 352, 19, title);
        }
        if (!run) SeatUi::Paragraph(graphics, ModText::Tr("当前没有正在进行的闯关或无尽。", "No active run or endless game."),
            Rect(52, 180, 656, 120), 22, body);
        SeatUi::Text(graphics, ModText::Tr("这里只查看开局选择；当前增益请到词条页面查看。", "Starting choices only. See modifiers for acquired buffs."),
            36, 448, 688, 18, muted);
        SeatUi::Button(graphics, PreviousRect(), ModText::Tr("上页", "Previous"), page > 0, down == 101);
        SeatUi::Button(graphics, NextRect(), ModText::Tr("下页", "Next"), page + 1 < PageCount(run), down == 102);
        SeatUi::Text(graphics, std::to_string(page + 1) + " / " + std::to_string(PageCount(run)),
            200, 515, 360, 20, body, true);
    }
};
