#ifndef __PROJECTILE_H__
#define __PROJECTILE_H__

#include "../ConstEnums.h"
#include "GameObject.h"

class Plant;
class Zombie;
namespace Sexy
{
    class Graphics;
};
using namespace Sexy;

class ProjectileDefinition
{
public:
    ProjectileType          mProjectileType;
    int                     mImageRow;
    int                     mDamage;
};
extern ProjectileDefinition gProjectileDefinition[NUM_PROJECTILES];  //0x69F1C0

class Projectile : public GameObject
{
public:
    int                     mFrame;                 //+0x24
    int                     mNumFrames;             //+0x28
    int                     mAnimCounter;           //+0x2C
    float                   mPosX;                  //+0x30
    float                   mPosY;                  //+0x34
    float                   mPosZ;                  //+0x38
    float                   mVelX;                  //+0x3C
    float                   mVelY;                  //+0x40
    float                   mVelZ;                  //+0x44
    float                   mAccZ;                  //+0x48
    float                   mShadowY;               //+0x4C
    bool                    mDead;                  //+0x50
    int                     mAnimTicksPerFrame;     //+0x54
    ProjectileMotion        mMotionType;            //+0x58
    ProjectileType          mProjectileType;        //+0x5C
    int                     mProjectileAge;         //+0x60
    int                     mClickBackoffCounter;   //+0x64
    float                   mRotation;              //+0x68
    float                   mRotationSpeed;         //+0x6C
    bool                    mOnHighGround;          //+0x70
    int                     mDamageRangeFlags;      //+0x74
    int                     mHitTorchwoodGridX;     //+0x78
    AttachmentID            mAttachmentID;          //+0x7C
    float                   mCobTargetX;            //+0x80
    int                     mCobTargetRow;          //+0x84
    ZombieID                mTargetZombieID;        //+0x88
    int                     mLastPortalX;           //+0x8C
    // @pvz-online: 单株升级「Prickly」（仙人掌）：尖刺穿透标记——0 = 无、-1 = 无限穿透
    // （2026-10-09 晚实机反馈起 Fire 预置 -1）、>0 = 剩余穿透次数（兼容保留，未用）。
    // 只有仙人掌的弹带；命中判定/扣点在 Projectile::DoImpact 末尾。
    int                     mPricklyHitsLeft;

public:
    Projectile();
    ~Projectile();

    void                    ProjectileInitialize(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType);
    void                    Update();
    void                    Draw(Graphics* g);
    void                    DrawShadow(Graphics* g);
    void                    Die();
    void                    DoImpact(Zombie* theZombie);
    void                    UpdateMotion();
    void                    CheckForCollision();
    Zombie*                 FindCollisionTarget();
    void                    UpdateLobMotion();
    void                    CheckForHighGround();
    bool                    CantHitHighGround();
    void                    DoSplashDamage(Zombie* theZombie);
    ProjectileDefinition&   GetProjectileDef();
    unsigned int            GetDamageFlags(Zombie* theZombie/* = nullptr*/);
    Rect                    GetProjectileRect();
    void                    UpdateNormalMotion();
    Plant*                  FindCollisionTargetPlant();
    void                    ConvertToFireball(int theGridX);
    void                    ConvertToPea(int theGridX);
    bool                    IsSplashDamage(Zombie* theZombie/* = nullptr*/);
    void                    PlayImpactSound(Zombie* theZombie);
    bool                    IsZombieHitBySplash(Zombie* theZombie);
    bool                    PeaAboutToHitTorchwood();

};

#endif
