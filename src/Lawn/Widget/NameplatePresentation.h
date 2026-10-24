#pragma once
#include "../Online/RoomPresentation.h"
#include "../System/NameplateCatalog.h"
#include "../Run/RunState.h"

namespace NameplatePresentation {
inline int ForSeat(LawnApp* app,uint8_t seat)
{
    auto* session=RoomPresentation::Session(app);
    if(!session)return 0;
    if(seat==session->GetLocalSeat())return app->mPlayerInfo?NameplateCatalog::Equipped(*app->mPlayerInfo):0;
    return session->GetSeatNameplate(seat);
}
inline int ForPlayer(LawnApp* app,uint64_t player)
{
    if(!app || !player)return 0;
    const auto* run=app->GetRunState();
    if(run && run->mTeam.player==player)return app->mPlayerInfo?NameplateCatalog::Equipped(*app->mPlayerInfo):0;
    auto* session=RoomPresentation::Session(app);
    return session?ForSeat(app,session->FindCoopSeat(player)):0;
}
}
