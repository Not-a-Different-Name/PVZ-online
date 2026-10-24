#pragma once
#include "RunState.h"
#include "../System/PlayerInfo.h"

namespace RunRerollPurchase {
enum class Result { APPLIED, STALE, INVALID, INSUFFICIENT, SAVE_FAILED };

template<class Save>
Result Apply(RunState& run,PlayerInfo& player,int lockedChoice,unsigned int expectedCounter,Save save)
{
    if(!run.HasPendingPick() || !run.mChoicesRolled || run.mPickCounter!=expectedCounter)return Result::STALE;
    if(lockedChoice < -1 || (lockedChoice!=-1 && !run.CanLockChoice(lockedChoice)))return Result::INVALID;
    const int cost=run.GetRerollPurchaseCost(lockedChoice);
    if(player.mCoins<cost)return Result::INSUFFICIENT;
    const RunState before=run;
    const int coins=player.mCoins;
    player.AddCoins(-cost);
    if(run.mRerollVouchers>0)--run.mRerollVouchers;
    run.PaidReroll(lockedChoice);
    if(save(run,player))return Result::APPLIED;
    run=before;
    player.mCoins=coins;
    return Result::SAVE_FAILED;
}
}
