#pragma once
#include <array>
#include <cstdint>
#include <vector>
class Reanimation;
class RenderItem;
class Board;
namespace Sexy {class Graphics;}
namespace Observer {
// Wire values only. These structures never contain a game pointer or a save.
struct Motion {
 uint32_t id=0;uint8_t kind=0;
 float vx=0,vy=0,ay=0,life=0;
};
struct VisualTrack {
 uint16_t index=0,image=65535,blend=0,blendTotal=0;uint8_t flags=0;
 uint32_t color=0xffffffff;
 float shakeX=0,shakeY=0;
 std::array<float,8> transform{};
};
struct VisualAnimation {
 uint16_t type=0,first=0,count=0;uint8_t loop=0,filter=0,flags=0;
 float time=0,rate=0;
 std::array<float,6> matrix{1,0,0,0,1,0};
 uint32_t color=0xffffffff,additive=0xffffffff,overlay=0xffffffff;
 std::vector<VisualTrack> tracks;
};
// The ordinary renderer calls these hooks only for a snapshot draw target.
int CaptureAnimationStart(Sexy::Graphics*,Reanimation*);
bool CaptureAnimationTrack(Sexy::Graphics*,Reanimation*,int,int);
void CaptureRenderItem(Sexy::Graphics*,Board*,const RenderItem&);
}
