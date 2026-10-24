#pragma once
#include "PlayerInfo.h"
#include "../ModText.h"

namespace NameplateCatalog {
constexpr int FIRST_PURCHASE_SLOT=76;
constexpr int EQUIPPED_SLOT=79;
constexpr int COUNT=3;
enum class Result { SAVED, ALREADY_OWNED, INVALID, NOT_OWNED, INSUFFICIENT, SAVE_FAILED };

inline bool Valid(int id){return id>=1 && id<=COUNT;}
inline int Price(int id){return id==1?1000:id==2?5000:id==3?10000:0;}
inline const char* Name(int id){
    return id==1?ModText::Tr("铜叶","Bronze Leaf"):id==2?ModText::Tr("银叶","Silver Leaf"):id==3?ModText::Tr("金叶","Gold Leaf"):ModText::Tr("无名牌","No nameplate");
}
inline bool Owned(const PlayerInfo& player,int id){
    return Valid(id) && player.mPurchases[FIRST_PURCHASE_SLOT+id-1]==1;
}
inline int Equipped(const PlayerInfo& player){
    const long id=player.mPurchases[EQUIPPED_SLOT];
    return id>=1 && id<=COUNT && Owned(player,static_cast<int>(id))?static_cast<int>(id):0;
}
template<class Save>
Result Buy(PlayerInfo& player,int id,Save save){
    if(!Valid(id))return Result::INVALID;
    if(Owned(player,id))return Result::ALREADY_OWNED;
    const int price=Price(id);
    if(player.mCoins<price)return Result::INSUFFICIENT;
    const int coins=player.mCoins;
    auto& owned=player.mPurchases[FIRST_PURCHASE_SLOT+id-1];
    const long previous=owned;
    player.AddCoins(-price);owned=1;
    if(save(player))return Result::SAVED;
    player.mCoins=coins;owned=previous;
    return Result::SAVE_FAILED;
}
template<class Save>
Result Equip(PlayerInfo& player,int id,Save save){
    if(id!=0 && !Valid(id))return Result::INVALID;
    if(id!=0 && !Owned(player,id))return Result::NOT_OWNED;
    auto& current=player.mPurchases[EQUIPPED_SLOT];
    if(current==id)return Result::SAVED;
    const long previous=current;current=id;
    if(save(player))return Result::SAVED;
    current=previous;return Result::SAVE_FAILED;
}
}
