#ifndef __ATTACHMENT_H__
#define __ATTACHMENT_H__

#include "../ConstEnums.h"
#include "DataArray.h"
#include "misc/SexyMatrix.h"
#include <cstdint>

namespace Sexy
{
    class Graphics;
}
using namespace Sexy;

#define MAX_EFFECTS_PER_ATTACHMENT 16

class Trail;
class Reanimation;
class TodParticleSystem;

class AttachEffect
{
public:
    unsigned int            mEffectID;
    EffectType              mEffectType;
    SexyTransform2D         mOffset;
    bool                    mDontDrawIfParentHidden;
    bool                    mDontPropogateColor;
};

class AttacherInfo
{
public:
    std::string             mReanimName;
    std::string             mTrackName;
    float                   mAnimRate;
    ReanimLoopType          mLoopType;

public:
    ~AttacherInfo() { ; }
};

class Attachment
{
public:
    AttachEffect            mEffectArray[MAX_EFFECTS_PER_ATTACHMENT];
    int                     mNumEffects;
    bool                    mDead;

public:
    Attachment();
    ~Attachment();

    void                    Update();
    void                    SetPosition(const SexyVector2& thePosition);
    void                    SetMatrix(const SexyTransform2D& theMatrix);
    void                    OverrideColor(const Color& theColor);
    void                    OverrideScale(float theScale);
    void                    Draw(Graphics* g, bool theParentHidden);
    void                    AttachmentDie();
    void                    Detach();
    void                    CrossFade(const char* theCrossFadeName);
    void                    PropogateColor(const Color& theColor, bool theEnableAdditiveColor, const Color& theAdditiveColor, bool theEnableOverlayColor, const Color& theOverlayColor);
};

AttachEffect*               AttachReanim(AttachmentID& theAttachmentID, Reanimation* theReanimation, float theOffsetX, float theOffsetY);
AttachEffect*               AttachParticle(AttachmentID& theAttachmentID, TodParticleSystem* theParticleSystem, float theOffsetX, float theOffsetY);
AttachEffect*               AttachTrail(AttachmentID& theAttachmentID, Trail* theTrail, float theOffsetX, float theOffsetY);
/*inline*/ void             AttachmentPropogateColor(AttachmentID& theAttachmentID, const Color& theColor, bool theEnableAdditiveColor, const Color& theAdditiveColor, bool theEnableOverlayColor, const Color& theOverlayColor);
/*inline*/ void             AttachmentOverrideColor(AttachmentID& theAttachmentID, const Color& theColor);
/*inline*/ void             AttachmentOverrideScale(AttachmentID& theAttachmentID, float theScale);
/*inline*/ void             AttachmentUpdateAndMove(AttachmentID& theAttachmentID, float theX, float theY);
/*inline*/ void             AttachmentUpdateAndSetMatrix(AttachmentID& theAttachmentID, SexyTransform2D& theMatrix);
/*inline*/ void             AttachmentDraw(AttachmentID& theAttachmentID, Graphics* g, bool theParentHidden);
/*inline*/ void             AttachmentDetach(AttachmentID& theAttachmentID);
/*inline*/ void             AttachmentDetachCrossFadeParticleType(AttachmentID& theAttachmentID, ParticleEffect theParticleEffect, const char* theCrossFadeName);
/*inline*/ void             AttachmentReanimTypeDie(AttachmentID& theAttachmentID, ReanimationType theReanimType);
/*inline*/ void             AttachmentDie(AttachmentID& theAttachmentID);
/*inline*/ void             AttachmentCrossFade(AttachmentID& theAttachmentID, const char* theCrossFadeName);
AttachEffect*               FindFirstAttachment(AttachmentID& theAttachmentID);
Reanimation*                FindReanimAttachment(AttachmentID& theAttachmentID);
AttachEffect*               CreateEffectAttachment(AttachmentID& theAttachmentID, EffectType theEffectType, unsigned int theDataID, float theOffsetX, float theOffsetY);
/*inline*/ bool             IsFullOfAttachments(AttachmentID& theAttachmentID);

// @pvz-online: 诊断用——attachment 的「出生记录」：分配时写进去的 AttachmentID& 字段地址，及首个效果的
// 类型/子类型（粒子 ParticleEffect、reanim ReanimationType）。只在 PVZ_REANIM_STATS=1 时记录，
// 供 [attachorigin] 对孤儿反查持有者；不参与游戏逻辑、不碰随机数。
enum AttachHolderKind { ATTACH_HOLDER_UNKNOWN, ATTACH_HOLDER_ZOMBIE, ATTACH_HOLDER_PROJECTILE, ATTACH_HOLDER_COIN, ATTACH_HOLDER_REANIM_TRACK };
struct AttachOriginDiag
{
    uintptr_t               mHolderAddress;
    EffectType              mEffectType;
    int                     mEffectSubtype;
    AttachHolderKind        mHolderKind;     // 由 resolver 在分配当时判定
    unsigned int            mHolderId;       // 持有者在其池里的 ID（含代数键，可判断「还是不是同一个」）
    int                     mHolderSubtype;  // 僵尸类型 / 子弹类型 / 金币类型 / reanim 类型
};
typedef void (*AttachOriginResolver)(uintptr_t theHolderAddress, AttachOriginDiag& theOrigin);
void                        AttachOriginSetResolver(AttachOriginResolver theResolver);
bool                        AttachOriginDiagEnabled();
const AttachOriginDiag*     AttachOriginLookup(unsigned int theAttachmentID);
void                        AttachOriginClear(); // discard diagnostic records at a new main board
void                        AttachOriginPrune();  // 丢弃池里已不存在的 attachment 的记录

class AttachmentHolder
{
public:
    DataArray<Attachment>   mAttachments;

public:
    AttachmentHolder();
    ~AttachmentHolder();

    void                    InitializeHolder();
    void                    DisposeHolder();
    Attachment*             AllocAttachment();
};

#endif