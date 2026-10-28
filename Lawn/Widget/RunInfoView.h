#pragma once
#include "OnlineUiDraw.h"
#include "../Run/RunState.h"
#include "../Run/RunBuffs.h"
#include <vector>

class RunInfoView {
public:
    struct Entry {
        int id;
        int permanent;
        int borrowed;
        int effective;
        std::string title;
        std::string description;
    };
    int tab = 0;
    int page = 0;
    int selected = 0;
    int detailPage = 0;

    static Sexy::Rect CreatorRect(int index) { return Sexy::Rect(index == 0 ? 40 : 390, 468, 330, 68); }
    static const char* CreatorUrl(int action) {
        if (action == 510) return "https://space.bilibili.com/491197058";
        if (action == 511) return "https://afdian.com/a/pvzol";
        return nullptr;
    }
    static Sexy::Rect CloseRect() { return Sexy::Rect(632, 26, 104, 44); }
    static Sexy::Rect TabRect(int index, bool buffsOnly = false, bool teamAvailable = false) {
        if(buffsOnly&&teamAvailable)return Sexy::Rect(24+(index-1)*180,82,172,48);
        return buffsOnly ? Sexy::Rect(24 + (index - 1) * 240, 82, 232, 48) :
            Sexy::Rect(24 + index * 180, 82, 172, 48);
    }
    static Sexy::Rect RowRect(int index) { return Sexy::Rect(24, 162 + index * 48, 270, 44); }
    static Sexy::Rect ListArrow(bool right) { return Sexy::Rect(right ? 222 : 24, 466, 72, 44); }
    static Sexy::Rect DetailArrow(bool right) { return Sexy::Rect(right ? 664 : 314, 466, 72, 44); }
    static Sexy::Rect ToolRect(int index) {
        if (index == 5) return Sexy::Rect(40, 416, 680, 40);
        if (index == 0) return Sexy::Rect(40, 158, 680, 48);
        if (index == 1 || index == 2) return Sexy::Rect(index == 1 ? 40 : 390, 226, 330, 48);
        return Sexy::Rect(40, index == 3 ? 294 : 362, 680, 48);
    }
    static std::vector<std::string> Wrap(const std::string& text, int width, int size) {
        std::vector<std::string> lines;
        std::string line;
        for (size_t offset = 0; offset < text.size();) {
            size_t end = offset + 1;
            while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) ++end;
            std::string glyph = text.substr(offset, end - offset);
            if (glyph == "\n") { lines.push_back(line); line.clear(); }
            else {
                if (!line.empty() && SeatUi::Width(line + glyph, size) > width) {
                    size_t space = line.find_last_of(' ');
                    if (space != std::string::npos && space > 0) {
                        lines.push_back(line.substr(0, space));
                        line.erase(0, space + 1);
                    } else { lines.push_back(line); line.clear(); }
                }
                if (!line.empty() || glyph != " ") line += glyph;
            }
            offset = end;
        }
        if (!line.empty()) lines.push_back(line);
        return lines;
    }
    std::vector<Entry> Entries(const RunState* run) const {
        std::vector<Entry> entries;
        if (!run || tab == 0) return entries;
        if (tab == 3) {
            for (int index = 0; index < RunState::RUN_ZOMBIE_BUFF_COUNT; ++index) {
                int count = run->ZombieBuffCount(index);
                if (count > 0) entries.push_back({index, count, 0, count,
                    GetZombieBuffDisplayName(index), GetZombieBuffDesc(index)});
            }
            return entries;
        }
        const int limit = static_cast<int>(RUN_BUFF_COUNT) + static_cast<int>(RUN_PLANT_UPGRADE_COUNT) +
            static_cast<int>(RUN_PLANT_BUFF2_COUNT);
        for (int id = tab == 1 ? 0 : RUN_BUFF_COUNT; id < (tab == 1 ? RUN_BUFF_COUNT : limit); ++id) {
            int permanent = run->GetBuffCount(id);
            int borrowed = 0;
            if (run->mLoanLevel == run->mLevelIndex && run->mLoanPlant >= 0 &&
                GetRunChoicePlant(id) == static_cast<SeedType>(run->mLoanPlant))
                for (const auto& stack : run->mLoanBuffs)
                    if (stack.mId == id) borrowed = std::max(borrowed, static_cast<int>(stack.mCount));
            int effective = run->GetEffectiveBuffCount(id);
            if (permanent <= 0 && borrowed <= 0 && effective <= 0) continue;
            std::string title = GetRunChoiceDisplayName(id);
            if (tab == 2) {
                const char* plants = GetRunChoicePlantName(id);
                if (plants) title = std::string(plants) + " · " + title;
            }
            entries.push_back({id, permanent, borrowed, effective, title, GetRunChoiceDesc(id)});
        }
        return entries;
    }
    int PageCount(const RunState* run) const { return std::max(1, (static_cast<int>(Entries(run).size()) + 5) / 6); }
    int DetailCount(const RunState* run) const {
        auto entries = Entries(run);
        if (selected < 0 || selected >= static_cast<int>(entries.size())) return 1;
        return std::max(1, (static_cast<int>(Wrap(entries[selected].description, 398, 18).size()) + 6) / 7);
    }
    void Flip(int delta, const RunState* run) {
        page = std::max(0, std::min(PageCount(run) - 1, page + delta));
        selected = page * 6;
        detailPage = 0;
    }
    int Hit(int x, int y, const RunState* run, bool hasBoard, bool online, bool buffsOnly = false, bool teamAvailable = false) const {
        if (CloseRect().Contains(x, y)) return 100;
        if(buffsOnly&&teamAvailable&&TabRect(4,true,true).Contains(x,y))return 550;
        for (int index = buffsOnly ? 1 : 0; index < 4; ++index)
            if ((index == 0 || run) && TabRect(index, buffsOnly, teamAvailable).Contains(x, y)) return 200 + index;
        if (tab == 0) {
            for (int index = 0; index < 2; ++index)
                if (CreatorRect(index).Contains(x, y)) return 510 + index;
            for (int index = 0; index < 6; ++index)
                if ((index == 0 || index == 3 || index == 5 || (index == 4 ? online : hasBoard)) &&
                    ToolRect(index).Contains(x, y)) return 500 + index;
        } else {
            auto entries = Entries(run);
            for (int index = 0; index < 6; ++index)
                if (page * 6 + index < static_cast<int>(entries.size()) && RowRect(index).Contains(x, y))
                    return 300 + index;
            if (page > 0 && ListArrow(false).Contains(x, y)) return 400;
            if (page + 1 < PageCount(run) && ListArrow(true).Contains(x, y)) return 401;
            if (detailPage > 0 && DetailArrow(false).Contains(x, y)) return 402;
            if (detailPage + 1 < DetailCount(run) && DetailArrow(true).Contains(x, y)) return 403;
        }
        return -1;
    }
    void Act(int action, const RunState* run) {
        if (action >= 200 && action < 204) { tab = action - 200; page = selected = detailPage = 0; }
        if (action >= 300 && action < 306) { selected = page * 6 + action - 300; detailPage = 0; }
        if (action == 400 || action == 401) Flip(action == 400 ? -1 : 1, run);
        if (action == 402 || action == 403)
            detailPage = std::max(0, std::min(DetailCount(run) - 1, detailPage + (action == 402 ? -1 : 1)));
    }
    void Draw(Sexy::Graphics* graphics, const RunState* run, bool hasBoard, bool online,
        bool plantHealth, bool zombieHealth, bool slideCollect, const std::string& language, int down, bool buffsOnly = false, bool teamAvailable = false, bool observerP2P = false) {
        using namespace Sexy;
        Color titleColor(245, 230, 165), bodyColor(235, 230, 210), muted(175, 175, 165);
        SeatUi::Panel(graphics, Rect(0, 0, 760, 560));
        SeatUi::Text(graphics, buffsOnly ? ModText::Tr("本局增益", "This run: modifiers") :
            ModText::Tr("辅助设置与本局词条", "Controls & this run"), 24, 59, 600, 26, titleColor);
        SeatUi::Button(graphics, CloseRect(), ModText::Tr("返回", "Back"), true, down == 100);
        const char* labels[] = {ModText::Tr("帮助设置", "Controls"), ModText::Tr("全部增益", "Global"),
            ModText::Tr("植物增益", "Plants"), ModText::Tr("僵尸强化", "Zombies")};
        for (int index = buffsOnly ? 1 : 0; index < 4; ++index) {
            SeatUi::Button(graphics, TabRect(index, buffsOnly, teamAvailable), labels[index], index == 0 || run, down == 200 + index);
            if (index == tab) { graphics->SetColor(titleColor); auto rect = TabRect(index, buffsOnly, teamAvailable); graphics->DrawRect(rect); }
        }
        if(buffsOnly&&teamAvailable)SeatUi::Button(graphics,TabRect(4,true,true),ModText::Tr("团队成长","Team"),true,down==550);
        if (tab == 0) {
            const char* on = ModText::Tr("开", "On");
            const char* off = ModText::Tr("关", "Off");
            std::string tools[] = {ModText::Tr("文字 / Language: ", "Language: ") + language,
                std::string(ModText::Tr("植物生命 J: ", "Plant HP J: ")) + (plantHealth ? on : off),
                std::string(ModText::Tr("僵尸生命 H: ", "Zombie HP H: ")) + (zombieHealth ? on : off),
                std::string(ModText::Tr("划取阳光: ", "Slide to collect sun: ")) + (slideCollect ? on : off),
                ModText::Tr("队友互助 B", "Teamwork B"),
                std::string("P2P") + ModText::Tr("观战: ", " viewing: ") + (observerP2P ? on : off)};
            for (int index = 0; index < 6; ++index)
                SeatUi::Button(graphics, ToolRect(index), tools[index].c_str(),
                    index == 0 || index == 3 || index == 5 || (index == 4 ? online : hasBoard), down == 500 + index);
            const char* creatorLabels[] = {ModText::Tr("B站 UP主主页", "Bilibili creator"), ModText::Tr("爱发电 · 支持作者", "Support on Afdian")};
            const char* creatorUrls[] = {"space.bilibili.com/491197058", "afdian.com/a/pvzol"};
            for (int index = 0; index < 2; ++index) {
                const auto rect = CreatorRect(index);
                SeatUi::Button(graphics, rect, "", true, down == 510 + index);
                SeatUi::Text(graphics, creatorLabels[index], rect.mX + 10, rect.mY + 27, rect.mWidth - 20, 19, titleColor, true);
                SeatUi::Text(graphics, creatorUrls[index], rect.mX + 10, rect.mY + 50, rect.mWidth - 20, 14, bodyColor, true);
            }
            return;
        }
        std::string context = run ? ModText::Tr("当前关卡 ", "Current level ") + std::to_string(run->mLevelIndex + 1) : "";
        if (run && run->mLoanLevel == run->mLevelIndex && run->mLoanPlant >= 0)
            context += std::string(ModText::Tr(" · 本关借入 ", " · loan this level: ")) + GetRunPlantName(static_cast<SeedType>(run->mLoanPlant));
        SeatUi::Text(graphics, context, 24, 153, 712, 18, muted);
        auto entries = Entries(run);
        page = std::max(0, std::min(PageCount(run) - 1, page));
        selected = std::max(0, std::min(static_cast<int>(entries.size()) - 1, selected));
        for (int index = 0; index < 6 && page * 6 + index < static_cast<int>(entries.size()); ++index) {
            int item = page * 6 + index;
            auto rect = RowRect(index);
            SeatUi::Strip(graphics, rect);
            if (item == selected) { graphics->SetColor(titleColor); graphics->DrawRect(rect); }
            SeatUi::Text(graphics, entries[item].title, rect.mX + 8, rect.mY + 28, rect.mWidth - 16, 18,
                item == selected ? titleColor : bodyColor);
        }
        SeatUi::Strip(graphics, Rect(304, 162, 432, 290));
        if (entries.empty()) SeatUi::Paragraph(graphics,
            ModText::Tr("此分类暂无已获得词条。图鉴中可查看可获得词条。", "No modifiers in this category. See the Almanac for available modifiers."),
            Rect(322, 180, 394, 220), 20, bodyColor);
        else {
            const auto& entry = entries[selected];
            auto names = Wrap(entry.title, 398, 21);
            for (int index = 0; index < std::min(2, static_cast<int>(names.size())); ++index)
                SeatUi::Text(graphics, names[index], 318, 190 + index * 26, 398, 21, titleColor);
            std::string stacks = tab == 3 ? ModText::Tr("已叠加 ", "Stacks ") + std::to_string(entry.effective) :
                ModText::Tr("永久 ", "Own ") + std::to_string(entry.permanent) + ModText::Tr(" · 借入 ", " · loan ") +
                std::to_string(entry.borrowed) + ModText::Tr(" · 生效 ", " · active ") + std::to_string(entry.effective);
            SeatUi::Text(graphics, stacks, 318, 245, 398, 18, titleColor);
            auto lines = Wrap(entry.description, 398, 18);
            detailPage = std::max(0, std::min(DetailCount(run) - 1, detailPage));
            for (int index = 0; index < 7 && detailPage * 7 + index < static_cast<int>(lines.size()); ++index)
                SeatUi::Text(graphics, lines[detailPage * 7 + index], 318, 277 + index * 24, 398, 18, bodyColor);
        }
        SeatUi::Button(graphics, ListArrow(false), "<", page > 0, down == 400);
        SeatUi::Button(graphics, ListArrow(true), ">", page + 1 < PageCount(run), down == 401);
        SeatUi::Text(graphics, std::to_string(page + 1) + "/" + std::to_string(PageCount(run)), 102, 495, 114, 20, bodyColor, true);
        SeatUi::Button(graphics, DetailArrow(false), "<", detailPage > 0, down == 402);
        SeatUi::Button(graphics, DetailArrow(true), ">", detailPage + 1 < DetailCount(run), down == 403);
        SeatUi::Text(graphics, ModText::Tr("详情 ", "Detail ") + std::to_string(detailPage + 1) + "/" + std::to_string(DetailCount(run)),
            392, 495, 266, 20, bodyColor, true);
        SeatUi::Text(graphics, ModText::Tr("借入仅本关有效；生效层数按实际规则，不相加。", "Loans last this level; active stacks follow the rules, not addition."),
            24, 535, 712, 17, muted);
    }
};
