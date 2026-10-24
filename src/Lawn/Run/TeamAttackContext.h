#pragma once
namespace TeamAttackContext {
inline thread_local bool exactBlast=false;
class ExactBlast {
    bool previous;
public: ExactBlast():previous(exactBlast){exactBlast=true;}
    ~ExactBlast(){exactBlast=previous;}
};
inline float BlastMultiplier(float ordinary){return exactBlast?1.0f:ordinary;}
}
