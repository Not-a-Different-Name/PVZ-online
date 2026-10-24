#pragma once

#include "OnlineUiDraw.h"
#include "../System/BattleMusicState.h"

class MusicSelectionView
{
public:
    int mTune = 0;
    bool mBossMusic = true;
    int mPage = 0;
    std::string mStatus;
    bool mError = false;

    static Sexy::Rect BackRect() { return Sexy::Rect(632, 26, 104, 44); }
    static Sexy::Rect RowRect(int index) { return Sexy::Rect(24, 140 + index * 48, 712, 44); }
    static Sexy::Rect AutoRect() { return Sexy::Rect(24, 456, 100, 44); }
    static Sexy::Rect PreviewRect() { return Sexy::Rect(136, 456, 100, 44); }
    static Sexy::Rect ApplyRect() { return Sexy::Rect(248, 456, 100, 44); }
    static Sexy::Rect BossRect() { return Sexy::Rect(360, 456, 164, 44); }
    static Sexy::Rect ArrowRect(bool next) { return Sexy::Rect(next ? 644 : 536, 456, 92, 44); }

    static std::string TuneName(int tune)
    {
        const char* chinese[] = {"原版自动", "白天", "夜晚", "泳池", "迷雾", "屋顶", "选卡",
            "主菜单", "花园", "解谜", "小游戏", "传送带", "僵王"};
        const char* english[] = {"Original (automatic)", "Grasswalk", "Moongrains", "Watery Graves",
            "Rigor Mormist", "Graze the Roof", "Choose Your Seeds", "Crazy Dave",
            "Zen Garden", "Cerebrawl", "Loonboon", "Conveyer", "Brainiac Maniac"};
        if (tune < 0 || tune > BattleMusicState::LAST_TUNE) tune = 0;
        if (!ModText::IsChinese() || tune == 0) return ModText::Tr(chinese[tune], english[tune]);
        return std::string(chinese[tune]) + " · " + english[tune];
    }

    void Flip(int delta) { mPage = std::max(0, std::min(1, mPage + delta)); }

    int Hit(int x, int y) const
    {
        if (BackRect().Contains(x, y)) return 100;
        if (AutoRect().Contains(x, y)) return 200;
        if (mTune > 0 && PreviewRect().Contains(x, y)) return 201;
        if (ApplyRect().Contains(x, y)) return 202;
        if (BossRect().Contains(x, y)) return 203;
        if (mPage > 0 && ArrowRect(false).Contains(x, y)) return 204;
        if (mPage < 1 && ArrowRect(true).Contains(x, y)) return 205;
        for (int index = 0; index < 6; ++index)
            if (RowRect(index).Contains(x, y)) return 300 + index;
        return -1;
    }

    void Draw(Sexy::Graphics* graphics, int down) const
    {
        using namespace Sexy;
        const Color title(245, 230, 165), body(235, 230, 210), muted(175, 175, 165);
        SeatUi::Panel(graphics, Rect(0, 0, 760, 560));
        SeatUi::Text(graphics, ModText::Tr("战斗配乐", "Battle music"), 24, 59, 600, 26, title);
        SeatUi::Button(graphics, BackRect(), ModText::Tr("返回", "Back"), true, down == 100);
        SeatUi::Text(graphics, std::string(ModText::Tr("待应用：", "Selection: ")) + TuneName(mTune),
            24, 96, 712, 18, title);
        SeatUi::Text(graphics, std::string(ModText::Tr("BOSS 自动切换僵王音乐：", "Auto Zomboss music for run bosses: ")) +
            (mBossMusic ? ModText::Tr("开", "On") : ModText::Tr("关", "Off")) +
            "    " + std::to_string(mPage + 1) + " / 2", 24, 124, 712, 16, muted);
        for (int index = 0; index < 6; ++index)
        {
            const int tune = mPage * 6 + index + 1;
            const Rect row = RowRect(index);
            SeatUi::Strip(graphics, row);
            if (tune == mTune)
            {
                graphics->SetColor(title);
                graphics->DrawRect(row.mX + 1, row.mY + 1, row.mWidth - 3, row.mHeight - 3);
            }
            SeatUi::Text(graphics, std::to_string(tune) + ".  " + TuneName(tune),
                row.mX + 12, row.mY + 29, row.mWidth - 24, 18, tune == mTune ? title : body);
        }
        SeatUi::Text(graphics, mStatus.empty() ? ModText::Tr(
            "选一首歌，可先「试听」，再点「应用」。", "Select a track, Preview it, then Apply.") : mStatus,
            24, 447, 712, 15, mError ? Color(255, 140, 120) : body);
        SeatUi::Button(graphics, AutoRect(), ModText::Tr("原版", "Auto"), true, down == 200);
        SeatUi::Button(graphics, PreviewRect(), ModText::Tr("试听", "Preview"), mTune > 0, down == 201);
        SeatUi::Button(graphics, ApplyRect(), ModText::Tr("应用", "Apply"), true, down == 202);
        SeatUi::Button(graphics, BossRect(), ModText::Tr("BOSS 音乐", "BOSS music"), true, down == 203);
        SeatUi::Button(graphics, ArrowRect(false), ModText::Tr("上一页", "Prev"), mPage > 0, down == 204);
        SeatUi::Button(graphics, ArrowRect(true), ModText::Tr("下一页", "Next"), mPage < 1, down == 205);
        SeatUi::Paragraph(graphics, ModText::Tr(
            "仅本机战斗生效；开启BOSS音乐后会临时切歌。\n试听不解除暂停；返回不保存。",
            "Local battles only. Enabled boss music temporarily overrides your track.\nPreview keeps the game paused. Back does not save."),
            Rect(24, 508, 712, 44), 14, muted);
    }
};
