#pragma once
#include "../../LawnApp.h"
#include "../BoardInclude.h"
#include "../Run/RunState.h"
#include "../SeedPacket.h"
#include "../Challenge.h"
#include "../System/PlayerInfo.h"
#include "../System/ReanimationLawn.h"
#include "ObserverDrawScene.h"
#ifdef __ANDROID__
#include "PvzpLib/Reanimator.h"
#include "PvzpLib/Attachment.h"
#include "PvzpLib/EffectSystem.h"
#else
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/EffectSystem.h"
#endif
#include <cstring>
#include "ObserverReanimationMap.h"
#include "WorldWire.h"
namespace ObserverWorld {
template<class IO>void Fields(IO&io,WaveHealthState&v){io.Value(v.start);io.Value(v.next);}
template<class IO>void Fields(IO&io,WaveSpawnRetryState&v){io.Value(v.wave);io.Value(v.index);io.Value(v.part);}
template<class IO>void Fields(IO&io,Admission::Entry&v){io.Value(v.player);io.Value(v.joined);io.Value(v.budget);}
template<class IO>void Fields(IO&io,Admission::State&v){io.Value(v.run);io.Value(v.player);io.Value(v.joined);io.Value(v.budget);io.Value(v.totalBudget);io.Value(v.phase);io.Value(v.fast);io.Value(v.allow);io.Value(v.history);}
template<class IO>void Fields(IO&io,TeamProgression::State&v){io.Value(v.player);io.Value(v.attempt);io.Value(v.localLevel);io.Value(v.shared.stacks);io.Value(v.allow);io.Value(v.cooldown);io.Value(v.restrictedSun);}
template<class IO>void Fields(IO&io,Sexy::Rect&v){io.Value(v.mX);io.Value(v.mY);io.Value(v.mWidth);io.Value(v.mHeight);}
template<class IO>void Fields(IO&io,Sexy::Color&v){io.Value(v.mRed);io.Value(v.mGreen);io.Value(v.mBlue);io.Value(v.mAlpha);}
template<class IO>void Fields(IO&io,Sexy::SexyTransform2D&v){io.Value(v.m00);io.Value(v.m01);io.Value(v.m02);io.Value(v.m10);io.Value(v.m11);io.Value(v.m12);}
template<class IO>void Fields(IO&io,RunState::BuffStack&v){io.Value(v.mId);io.Value(v.mCount);}
// Forward declarations permit nested structures independent of header order.
template<class IO>void Fields(IO&,GameObject&);
template<class IO>void Fields(IO&,Plant&);
template<class IO>void Fields(IO&,Zombie&);
template<class IO>void Fields(IO&,Projectile&);
template<class IO>void Fields(IO&,Coin&);
template<class IO>void Fields(IO&,LawnMower&);
template<class IO>void Fields(IO&,GridItem&);
template<class IO>void Fields(IO&,MagnetItem&);
template<class IO>void Fields(IO&,MotionTrailFrame&);
template<class IO>void Fields(IO&,SeedPacket&);
template<class IO>void Fields(IO&,SeedBank&);
template<class IO>void Fields(IO&,Challenge&);
template<class IO>void Fields(IO&,Board&);
template<class IO>void Fields(IO&,RunState&);
template<class IO>void Fields(IO&,Reanimation&);
template<class IO>void Fields(IO&,ReanimatorTrackInstance&);
template<class IO>void Fields(IO&,ReanimatorTransform&);
template<class IO>void Fields(IO&,AttachEffect&);
template<class IO>void Fields(IO&,Attachment&);
template<class IO>void Fields(IO&,PottedPlant&);
template<class IO>void Fields(IO&,PvzpSmoothArray&);
template<class IO>void Fields(IO&,PvzpWeightedGridArray&);
#include "WorldFields.inl"
}
