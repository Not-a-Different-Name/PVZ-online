#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Read-only presentation contract. Widgets emit intents; they never grant effects.
namespace TeamProgressionView {
enum class Page { VOTE, TEAM_EFFECTS, TEAMMATES, BRIEF, GOALS };
enum class Phase { DISABLED, VOTING, COMMITTED, PREPARING, PLAYING, FAILED, COMPLETE, RECOVERING };
enum class Action { CLOSE, SELECT_PAGE, VOTE, SELECT_PEER, SUN_REQUEST, SUN_GIFT,
    LOAN, WATCH, SUPPORT_PREVIEW, SELECT_CELL, SUPPORT_CONFIRM, SUPPORT_PERMISSION, DETAILS };
struct Context {
    uint64_t run=0, player=0;
    uint32_t level=0, attempt=0, term=0, revision=0;
};
struct Choice {
    int id=-1, stacks=0;
    std::string name, description, unavailableReason;
    bool enabled=false;
};
struct Peer {
    uint64_t player=0;
    int seat=0;
    std::string name, phase, voteStatus;
    bool online=false, eligible=false, allowSupport=false;
};
struct Effect { int id=-1,stacks=0; std::string name,description,source; };
struct Goal { std::string name,description,progress; int progressPermille=0; bool earned=false; };
struct Support {
    uint64_t target=0, quoteId=0;
    int row=-1,column=-1,cost=0,cooldownSeconds=0;
    bool enabled=false,canConfirm=false,pending=false;
    std::string reason;
};
struct Snapshot {
    Context context;
    Phase phase=Phase::DISABLED;
    Page page=Page::VOTE;
    bool canAct=false,canVote=false,voteOpen=false;
    std::string status,briefTitle,briefSummary,damageSummary;
    std::vector<Choice> choices;
    std::vector<Peer> peers;
    std::vector<Effect> teamEffects,personalEffects;
    std::vector<Goal> goals;
    Support support;
};
struct Intent {
    Action action=Action::CLOSE;
    Context context;
    uint64_t target=0,quoteId=0;
    int choice=-1,row=-1,column=-1,value=0;
};
class Controller {
public:
    virtual ~Controller()=default;
    virtual Snapshot Read() const=0;
    virtual void Submit(const Intent& intent)=0;
};
}
