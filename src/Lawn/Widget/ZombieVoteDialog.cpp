#include "ZombieVoteDialog.h"
#include "ZombieBuffButtonLabel.h"
#include "../../LawnApp.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "../Run/RunState.h"
#include "MobileCoopDraw.h"
#include "OnlineUiDraw.h"

ZombieVoteDialog::ZombieVoteDialog(LawnApp *app)
    : LawnDialog(app, DIALOG_ZOMBIE_VOTE, true, "", "", "", BUTTONS_NONE)
{
    Resize(0, 0, 730, 520);
    for (int i = 0; i < 4; ++i)
    {
        auto button = std::make_unique<MobileStoneButton>(i, this);
        button->mLabel =
            i < 3 ? (app->mZombieVoteCandidates[i]<RunState::RUN_ZOMBIE_BUFF_COUNT?ZombieBuffButtonLabel(app->mZombieVoteCandidates[i]):ModText::Tr("无候选","Unavailable")) : ModText::Tr("弃权", "Abstain");
        button->Resize(i < 3 ? 25 + i * 230 : 255, i < 3 ? 362 : 432, 215, 52);
        AddWidget(button.get());
        mChoices.push_back(std::move(button));
    }
}
ZombieVoteDialog::~ZombieVoteDialog()
{
    for (auto &b : mChoices)
        RemoveWidget(b.get());
}
void ZombieVoteDialog::Update()
{
    LawnDialog::Update();
    for (auto &b : mChoices)
        b->SetDisabled(mApp->mZombieVoteMyChoice >= 0 || (b->mId<3 && mApp->mZombieVoteCandidates[b->mId]>=RunState::RUN_ZOMBIE_BUFF_COUNT));
    MarkDirty();
}
void ZombieVoteDialog::Draw(Sexy::Graphics *g)
{
    LawnDialog::Draw(g);
    PvzpDrawStringWrapped(g, ModText::Tr("僵尸强化投票", "Zombie boost vote"), Sexy::Rect(40, 48, 650, 40),
        ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT24, 30), Sexy::Color(245, 230, 165), DS_ALIGN_CENTER);
    SeatUi::Text(g, mApp->ZombieVoteStatusText(), 40, 116, 650, 22, Sexy::Color(220, 215, 190), true);
    SeatUi::Paragraph(g, ModText::Tr("选一项强化或弃权；投票后等待队友，超时自动弃权。",
        "Choose a boost or abstain; wait for teammates after voting. Timeout abstains."),
        Sexy::Rect(50, 136, 630, 48), 20, Sexy::Color(220, 215, 190));
    for (int i = 0; i < 3; ++i)
    {
        const int id = mApp->mZombieVoteCandidates[i];
        if(id>=RunState::RUN_ZOMBIE_BUFF_COUNT)continue;
        PvzpDrawStringWrapped(g, GetZombieBuffDesc(id), Sexy::Rect(32 + i * 230, 196, 202, 132),
                              ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18, 20), Sexy::Color(245, 230, 165),
                              DrawStringJustification::DS_ALIGN_LEFT);
        SeatUi::Text(g, ModText::Format("层数 {}", "Stacks {}", mApp->mRunState->ZombieBuffCount(id)),
            25 + i * 230, 348, 215, 20, Sexy::Color(220, 215, 190), true);
    }
}
void ZombieVoteDialog::ButtonDepress(int id)
{
    if (id < 0 || id > 3 || mApp->mZombieVoteMyChoice >= 0)
        return;
    for (auto &b : mChoices)
        b->SetDisabled(true);
    mApp->ZombieVoteChosen(id);
}
