#pragma once
#include <vector>
#include <cstddef>
class Board;
class Zombie;
namespace NetProto { struct MsgEscapedZombie; }
namespace RunSpeciesRuntime {
enum Flag : unsigned { THROWN_USED=1, NO_THROW=2, FLYING_GIANT=4, BLAST_COMMITTED=8 };
constexpr unsigned BLASTS_PER_FRAME=64;
void Reset(Board*);
unsigned Flags(Zombie*);
void SetFlags(Zombie*,unsigned);
bool CanThrow(Zombie*);
bool GiantReplacement(Zombie*);
void ChildThrown(Zombie* parent,Zombie* child);
bool UpdateFlight(Zombie*);
void CombatDeath(Zombie*);
bool CommitBlast(Zombie*,bool active=false);
unsigned Pending(Board*);
unsigned Drain(Board*);
void ExportRelay(Zombie*,NetProto::MsgEscapedZombie&);
bool ImportRelay(Zombie*,const NetProto::MsgEscapedZombie&);
std::vector<unsigned char> SaveTrailer(Board*);
bool LoadTrailer(Board*,const unsigned char*,size_t);
}
