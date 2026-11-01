// Included by ObserverDrawScene.cpp after its trusted engine/resource helpers.
uint16_t DrawRecorder::Asset(Image* image){
 if(!image)return 65535;
 AssetKey k;if(!assets.Identify(image,k)){Fail();return 65535;}
 auto it=ids.find(k);if(it!=ids.end())return it->second;
 if(frame.assets.size()>=512){Fail();return 65535;}
 auto n=uint16_t(frame.assets.size());ids.emplace(k,n);frame.assets.push_back(k);return n;
}
int DrawRecorder::AnimationStart(Reanimation*r){
 if(!Predicting())return -1;
 if(!r||!r->mDefinition||!r->mTrackInstances||r->mReanimationType<0||uint32_t(r->mReanimationType)>=gReanimatorDefCount||frame.animations.size()>=512){Fail();return -1;}
 uint16_t wire=0;if(!ReanimationToWire(uint32_t(r->mReanimationType),wire)){Fail();return -1;}
 VisualAnimation a;a.type=wire;a.first=uint16_t(r->mFrameStart);a.count=uint16_t(r->mFrameCount);
 a.loop=uint8_t(r->mLoopType);a.filter=uint8_t(int(r->mFilterEffect)+1);
 a.flags=uint8_t(r->mEnableExtraAdditiveDraw)|(uint8_t(r->mEnableExtraOverlayDraw)<<1);
 a.time=r->mAnimTime;a.rate=r->mAnimRate;
 auto&m=r->mOverlayMatrix;a.matrix={m.m00,m.m01,m.m02,m.m10,m.m11,m.m12};
 a.color=ColorOf(r->mColorOverride);a.additive=ColorOf(r->mExtraAdditiveColor);a.overlay=ColorOf(r->mExtraOverlayColor);
 frame.animations.push_back(std::move(a));return int(frame.animations.size()-1);
}
bool DrawRecorder::AnimationTrack(Graphics*g,Reanimation*r,int animation,int track){
 if(animation<0||size_t(animation)>=frame.animations.size()||track<0||track>=r->mDefinition->mTracks.count){Fail();return false;}
 auto&t=r->mTrackInstances[track];VisualTrack v;v.index=uint16_t(track);v.image=Asset(t.mImageOverride);
 v.blend=uint16_t(std::clamp(t.mBlendCounter,0,65535));v.blendTotal=uint16_t(std::clamp(t.mBlendTime,0,65535));
 v.flags=uint8_t(t.mIgnoreClipRect)|(uint8_t(t.mTruncateDisappearingFrames)<<1)|(uint8_t(t.mIgnoreColorOverride)<<2)|(uint8_t(t.mIgnoreExtraAdditiveColor)<<3);
 v.color=ColorOf(t.mTrackColor);v.shakeX=t.mShakeX;v.shakeY=t.mShakeY;
 auto&b=t.mBlendTransform;v.transform={b.mTransX,b.mTransY,b.mSkewX,b.mSkewY,b.mScaleX,b.mScaleY,b.mFrame,b.mAlpha};
 auto&a=frame.animations[animation];if(a.tracks.size()>=1024){Fail();return false;}a.tracks.push_back(v);
 auto d=Base(DrawOp::AnimationTrack,g->GetColor(),g->GetDrawMode());d.asset=uint16_t(animation);d.src[0]=int(a.tracks.size()-1);
 d.clip=Box(g->mClipRect);d.flags=g->GetColorizeImages();d.values[0]=g->mTransX;d.values[1]=g->mTransY;Add(std::move(d));
 // Attachments use the same visibility test as the original track renderer.
 ReanimatorTransform x;r->GetCurrentTransform(track,&x);
 Color c=t.mTrackColor;if(!t.mIgnoreColorOverride)c=ColorsMultiply(c,r->mColorOverride);if(g->GetColorizeImages())c=ColorsMultiply(c,g->GetColor());
 return FloatRoundToInt(x.mFrame)>=0&&std::clamp(FloatRoundToInt(x.mAlpha*c.mAlpha),0,255)>0&&
  (x.mImage|| (x.mFont&&x.mText&&*x.mText)||!strcmp(r->mDefinition->mTracks.tracks[track].mName,"fullscreen"));
}
// A snapshot must not hold one instantaneous footstep velocity for seconds.
// Average the current animation range, including its full-last-frame dwell.
static float ObserverGroundSpeed(Reanimation* a,float scale){
 const auto& track=a->mDefinition->mTracks.tracks[a->FindTrackIndex("_ground")];
 const int first=a->mFrameStart,count=a->mFrameCount;
 if(count<2||first<0||first+count>track.mTransforms.count)return 0;
 const auto* t=track.mTransforms.mTransforms;
 const bool full=a->mLoopType==REANIM_LOOP_FULL_LAST_FRAME||a->mLoopType==REANIM_PLAY_ONCE_FULL_LAST_FRAME||a->mLoopType==REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD;
 return (t[first+count-1].mTransX-t[first].mTransX)*a->mAnimRate*scale*.01f/(full?count:count-1);
}
void DrawRecorder::RenderItemMotion(Board*b,const RenderItem&r){
 motion=0;if(!Predicting())return;
 Motion m;
 switch(r.mRenderObjectType){
 case RENDER_ITEM_ZOMBIE:case RENDER_ITEM_ZOMBIE_SHADOW:{
  auto*z=r.mZombie;m.kind=1;m.id=b->mZombies.DataArrayGetID(z);
  if(!z->IsDeadOrDying()&&!z->ZombieNotWalking()){
   const bool chilled=z->IsMovingAtChilledSpeed();
   float speed=z->mVelX;
   auto*a=b->mApp->ReanimationTryToGet(z->mBodyReanimID);
   const bool chilledVelocity=z->IsBouncingPogo()||z->mZombiePhase==PHASE_BALLOON_FLYING||z->mZombiePhase==PHASE_DOLPHIN_RIDING||z->mZombiePhase==PHASE_SNORKEL_WALKING_IN_POOL||z->mZombieType==ZOMBIE_CATAPULT;
   const bool rawVelocity=z->mZombieType==ZOMBIE_ZAMBONI||z->mZombiePhase==PHASE_DIGGER_TUNNELING||z->mZombiePhase==PHASE_DOLPHIN_IN_JUMP||z->IsBobsledTeamWithSled()||z->mZombiePhase==PHASE_POLEVAULTER_IN_VAULT||z->mZombiePhase==PHASE_SNORKEL_INTO_POOL;
   if(a&&!chilledVelocity&&!rawVelocity&&a->TrackExists("_ground"))speed=ObserverGroundSpeed(a,z->mScaleZombie);
   else if(!rawVelocity&&chilled)speed*=.4f;
   if(z->mZombieType==ZOMBIE_ZAMBONI&&b->IsRunBossZombie(z)){
    if(chilled)speed*=.4f;
    else {if(z->mFetidSlowCounter>0)speed*=1.f-.2f*b->mApp->RunPlantBuff2Count(SEED_FUMESHROOM);if(z->mFrostArmorCounter>0)speed*=.65f;}
   }
   float barbed=1.f;
   if(z->mRunBarbedCounter>0&&z->CanBeChilled()){
    float existing=chilled?.5f:1.f;
    if(!chilled){if(z->mFetidSlowCounter>0)existing*=1.f-.2f*b->mApp->RunPlantBuff2Count(SEED_FUMESHROOM);if(z->mFrostArmorCounter>0)existing*=.65f;}
    if(existing>.8f)barbed=.8f/existing;
   }
   m.vx=(z->IsWalkingBackwards()||z->mZombiePhase==PHASE_DANCER_DANCING_IN?1.f:-1.f)*speed*barbed*RunSpeciesEffects::WalkFactor(z)*100.f;
  }break;}
 case RENDER_ITEM_PROJECTILE:case RENDER_ITEM_PROJECTILE_SHADOW:{
  auto*p=r.mProjectile;m.kind=r.mRenderObjectType==RENDER_ITEM_PROJECTILE?2:3;m.id=b->mProjectiles.DataArrayGetID(p);
  m.vx=p->mVelX*100;m.vy=p->mVelY*100;m.life=10;
  if(m.kind==2&&p->mMotionType==MOTION_LOBBED){m.vy+=p->mVelZ*100;m.ay=p->mAccZ*10000;}
  break;}
 default:return;
 }
 if(!std::isfinite(m.vx)||!std::isfinite(m.vy)||!std::isfinite(m.ay)){Fail();return;}
 for(size_t i=0;i<frame.motions.size();++i)if(frame.motions[i].kind==m.kind&&frame.motions[i].id==m.id){motion=uint16_t(i+1);return;}
 if(frame.motions.size()>=2048){Fail();return;}frame.motions.push_back(m);motion=uint16_t(frame.motions.size());
}
int CaptureAnimationStart(Graphics*g,Reanimation*r){auto*p=dynamic_cast<DrawRecorder*>(g->mDestImage);return p?p->AnimationStart(r):-1;}
bool CaptureAnimationTrack(Graphics*g,Reanimation*r,int a,int t){auto*p=dynamic_cast<DrawRecorder*>(g->mDestImage);return p&&p->AnimationTrack(g,r,a,t);}
void CaptureRenderItem(Graphics*g,Board*b,const RenderItem&r){auto*p=dynamic_cast<DrawRecorder*>(g->mDestImage);if(p)p->RenderItemMotion(b,r);}
