#pragma once
#include "Run/TeamProgressionView.h"
#include "Online/TeamProgressionWire.h"
#include "Run/TeamGoals.h"
#include <map>
class LawnApp;class RunState;class Board;class NetSession;
namespace TeamPlayUi {
void Install(LawnApp*,TeamProgressionView::Controller*);
void Show(LawnApp*,TeamProgressionView::Page);
void Close(LawnApp*);
}
class TeamPlay final : public TeamProgressionView::Controller {
public:
    explicit TeamPlay(LawnApp* owner):app(owner){}
    static TeamPlay& For(LawnApp*);
    static void Detach(LawnApp*);
    static bool PrepareCheckpoint(LawnApp*,std::vector<unsigned char>& previous,bool& captured);
    static void NoteDamage(LawnApp*,uint64_t caster,uint64_t strike,int amount,bool boss);
    static bool NoteGift(LawnApp*,int actualSun);
    static bool NoteVictory(LawnApp*);
    static bool SaveCoopTransaction(LawnApp*,bool lawn=true);
    static void NoteSharedPlant(LawnApp*,int plant);
    static float HealthMultiplier(LawnApp*);
    static float SeedMultiplier(LawnApp*);
    static bool IsSupportCard(LawnApp*,uint32_t coin);
    static bool UseSupportCard(LawnApp*,uint32_t coin,int row,int column);
    static void SpendRestrictedSun(LawnApp*,int amount);
    void Update();
    bool HoldPreparation();
    bool HoldPersonalRewards();
    void PrepareBattle();
    TeamProgressionView::Snapshot Read()const override;
    void Submit(const TeamProgressionView::Intent&)override;
    LawnApp* app;
private:
    RunState* observed=nullptr;
    TeamProgressionView::Page page=TeamProgressionView::Page::TEAM_EFFECTS;
    std::map<uint64_t,TeamProgression::Member> peers;
    std::map<uint64_t,int> peerAge;
    uint64_t selected=0,quote=0;
    uint32_t viewRevision=1;
    uint32_t shownBallotRevision=0;
    int quoteRow=-1,quoteColumn=-1,quoteCost=-1,reportTicks=0,voteTicks=0,saveTicks=0;
    bool wantsVote=false,dirty=false,reportedClear=false,baselineCaptured=false,recoverLawn=false;
    std::string notice;
    TeamGoals::Ledger goals;
    int goalsProfile=-1;
    bool goalsValid=true,goalsPending=false;
    bool SyncGoals();
    NetSession* Session()const;
    bool Host()const;
    bool HasTeammates()const;
    uint32_t Attempt()const;
    TeamWire::Message Message(uint8_t)const;
    bool Send(TeamWire::Message,uint64_t target=0);
    void Handle(const TeamWire::Message&);
    void Broadcast();
    void Report();
    bool Save(bool lawn=false);
    bool BattleCheckpointReady()const;
    void BeginContext();
    void GrantRelief();
    void Reply(const TeamProgression::Strike&,uint8_t);
    void Execute(TeamProgression::Receipt&);
    void ExecuteBlast(TeamProgression::Receipt&);
    bool HasSupportCard()const;
    void ExpireSupportCard();
    void GrantStageBuffs();
    bool Capture(std::vector<unsigned char>&)const;
    bool Restore(const std::vector<unsigned char>&);
    bool CanSupport(uint64_t target,int row,int col,std::string& why)const;
    int CherryCost()const;
    void StartSupport(uint64_t,int,int);
    std::vector<TeamProgression::Member> Members()const;
};
