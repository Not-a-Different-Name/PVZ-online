#include "ObserverDrawScene.h"
#include "Lawn/Board.h"
#include "LawnApp.h"
#include "Lawn/Run/RunSpeciesEffects.h"
#include "Resources.h"
#include "Lawn/System/ReanimationLawn.h"
#include "graphics/MemoryImage.h"
#include "graphics/ImageFont.h"
#include "graphics/Font.h"
#include "misc/MTRand.h"
#ifdef __ANDROID__
#include "PvzpLib/Reanimator.h"
#include "PvzpLib/PvzpCommon.h"
#include "PvzpLib/EffectSystem.h"
#include "PvzpLib/ReanimAtlas.h"
#include "PvzpLib/FilterEffect.h"
#else
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/ReanimAtlas.h"
#include "Sexy.TodLib/FilterEffect.h"
#include "Lawn/ModText.h"
#endif
#include <cmath>
#include <limits>
#include <algorithm>
#include <cstring>
#include <tuple>
using namespace Sexy;
namespace Observer {
namespace {
uint32_t Hash(const char*s){uint32_t h=2166136261u;for(;*s;++s)h=(h^uint8_t(*s))*16777619u;return h;}
std::array<int,4> Box(const Rect&r){return {r.mX,r.mY,r.mWidth,r.mHeight};}
Rect RectOf(const std::array<int,4>&r){return Rect(r[0],r[1],r[2],r[3]);}
uint32_t ColorOf(const Color&c){return uint32_t(c.mAlpha)<<24|uint32_t(c.mRed)<<16|uint32_t(c.mGreen)<<8|uint32_t(c.mBlue);}
Color ColorOf(uint32_t c){return Color((c>>16)&255,(c>>8)&255,c&255,(c>>24)&255);}
template<class T>T* Raw(T*p){return p;}template<class T>T* Raw(const std::unique_ptr<T>&p){return p.get();}
struct RandomGuard {decltype(Sexy::GetRandState()) state=Sexy::GetRandState();~RandomGuard(){Sexy::RestoreRandState(state);}};
bool Finite(double f){return std::isfinite(f)&&std::abs(f)<=65536;}
bool Bounds(const std::array<int,4>&r){return r[0]>=-8192&&r[0]<=8192&&r[1]>=-8192&&r[1]<=8192&&r[2]>=0&&r[2]<=8192&&r[3]>=0&&r[3]<=8192;}
}
struct DrawAssets::Impl {
 LawnApp*app;std::map<Image*,AssetKey> ids;std::map<AssetKey,Image*> images;
 std::map<AssetKey,std::unique_ptr<MemoryImage>> cached;
 explicit Impl(LawnApp*a):app(a){}
 void Add(Image*p,AssetKey k){if(!p)return;ids.emplace(p,k);images.emplace(k,p);}
 void Font(_Font*f,uint32_t id){auto*font=dynamic_cast<ImageFont*>(f);if(!font||!font->mFontData)return;
  uint16_t n=0;for(auto&layer:font->mFontData->mFontLayerList)Add((Image*)layer.mImage,{2,id,n++,0});
  n=0;for(auto&layer:font->mActiveLayerList)Add(layer.mScaledImage,{9,id,n++,0});
 }
};
DrawAssets::DrawAssets(LawnApp*a):impl(new Impl(a)){Refresh();}DrawAssets::~DrawAssets()=default;
void DrawAssets::Refresh(){
 impl->ids.clear();impl->images.clear();
#define OBS_IMAGE(name) impl->Add(name,{1,Hash(#name),0,0});
#define OBS_FONT(name) impl->Font(name,Hash(#name));
#include "ObserverAssets.inc"
#undef OBS_IMAGE
#undef OBS_FONT
 for(uint32_t i=0;i<gReanimatorDefCount;++i){auto&d=gReanimatorDefArray[i];auto*a=d.mReanimAtlas;if(!a)continue;
  impl->Add(Raw(a->mMemoryImage),{3,i,0,0});
#ifdef __ANDROID__
  int count=int(a->mImageArray.size());
#else
  int count=a->mImageCount;
#endif
  for(int j=0;j<count&&j<1024;++j)impl->Add(a->mImageArray[j].mOriginalImage,{4,i,uint16_t(j),0});
 }
 if(auto*c=Raw(impl->app->mReanimatorCache)){
  for(int i=0;i<NUM_SEED_TYPES;++i)impl->Add(Raw(c->mPlantImages[i]),{6,uint32_t(i),uint16_t(VARIATION_NORMAL),0});
  for(auto*p=c->mImageVariationList.mHead;p;p=p->mNext)impl->Add(p->mValue.mImage,{6,uint32_t(p->mValue.mSeedType),uint16_t(p->mValue.mDrawVariation),0});
  for(int i=0;i<NUM_MOWER_TYPES;++i)impl->Add(Raw(c->mLawnMowers[i]),{7,uint32_t(i),0,0});
  for(int i=0;i<NUM_CACHED_ZOMBIE_TYPES;++i)impl->Add(Raw(c->mZombieImages[i]),{8,uint32_t(i),0,0});
 }
 for(auto&entry:impl->cached)impl->Add(entry.second.get(),entry.first);
 for(int pass=0;pass<4;++pass)for(int f=0;f<NUM_FILTER_EFFECTS;++f)for(auto&entry:gFilterMap[f]){
  auto it=impl->ids.find(entry.first);if(it==impl->ids.end()||it->second.filters>=64)continue;
  auto k=it->second;k.filters=uint16_t((k.filters<<2)|(f+1));impl->Add(Raw(entry.second),k);
 }
}
bool DrawAssets::Identify(Image*p,AssetKey&k){auto it=impl->ids.find(p);if(it==impl->ids.end()){Refresh();it=impl->ids.find(p);}if(it==impl->ids.end())return false;k=it->second;return true;}
Image* DrawAssets::Resolve(const AssetKey&k){
 auto it=impl->images.find(k);if(it!=impl->images.end())return it->second;
 if(k.filters){auto base=k;int f=(k.filters&3)-1;base.filters>>=2;if(f<0||f>=NUM_FILTER_EFFECTS)return nullptr;auto*p=Resolve(base);return p?FilterEffectGetImage(p,FilterEffect(f)):nullptr;}
 if((k.kind==3||k.kind==4)&&k.id<gReanimatorDefCount){ReanimatorEnsureDefinitionLoaded(ReanimationType(k.id),false);Refresh();it=impl->images.find(k);return it==impl->images.end()?nullptr:it->second;}
 if(k.kind>=6&&k.kind<=8&&impl->cached.size()<128){auto*c=Raw(impl->app->mReanimatorCache);if(!c)return nullptr;
  if((k.kind==6&&(k.id>=NUM_SEED_TYPES||k.item>VARIATION_AQUARIUM))||(k.kind==7&&k.id>=NUM_MOWER_TYPES)||(k.kind==8&&k.id>=NUM_CACHED_ZOMBIE_TYPES))return nullptr;
  const auto random=Sexy::GetRandState();
  std::unique_ptr<MemoryImage> image;
#ifdef __ANDROID__
  if(k.kind==6)image=c->MakeCachedPlantFrame(SeedType(k.id),DrawVariation(k.item));
  else if(k.kind==7)image=c->MakeCachedMowerFrame(LawnMowerType(k.id));else image=c->MakeCachedZombieFrame(ZombieType(k.id));
#else
  if(k.kind==6)image.reset(c->MakeCachedPlantFrame(SeedType(k.id),DrawVariation(k.item)));
  else if(k.kind==7)image.reset(c->MakeCachedMowerFrame(LawnMowerType(k.id)));else image.reset(c->MakeCachedZombieFrame(ZombieType(k.id)));
#endif
  Sexy::RestoreRandState(random);auto*p=image.get();impl->cached.emplace(k,std::move(image));impl->Add(p,k);return p;
 }
 Refresh();it=impl->images.find(k);return it==impl->images.end()?nullptr:it->second;
}
DrawRecorder::DrawRecorder(LawnApp*a,DrawFrame&f):assets(a),frame(f){mWidth=800;mHeight=600;}
DrawCommand DrawRecorder::Base(DrawOp op,const Color&c,int mode){DrawCommand d;d.op=op;d.color=ColorOf(c);d.mode=uint8_t(mode);d.motion=motion;return d;}
void DrawRecorder::Add(DrawCommand c,Image*image){
 if(!valid)return;if(frame.commands.size()>=8192){valid=false;return;}
 if(image){AssetKey k;if(!assets.Identify(image,k)){fprintf(stderr,"[observer2] unregistered asset: %s (%dx%d)\n",image->mFilePath.c_str(),image->mWidth,image->mHeight);valid=false;return;}auto it=ids.find(k);
  if(it==ids.end()){if(frame.assets.size()>=512){valid=false;return;}c.asset=uint16_t(frame.assets.size());ids.emplace(k,c.asset);frame.assets.push_back(k);}else c.asset=it->second;}
 frame.commands.push_back(std::move(c));
}
void DrawRecorder::FillRect(const Rect&r,const Color&c,int m){auto d=Base(DrawOp::Fill,c,m);d.dst=Box(r);Add(d);}
void DrawRecorder::DrawRect(const Rect&r,const Color&c,int m){auto d=Base(DrawOp::Rect,c,m);d.dst=Box(r);Add(d);}
void DrawRecorder::ClearRect(const Rect&){Fail();}
void DrawRecorder::DrawLine(double x,double y,double xx,double yy,const Color&c,int m){auto d=Base(DrawOp::Line,c,m);d.values={float(x),float(y),float(xx),float(yy)};Add(d);}
void DrawRecorder::DrawLineAA(double x,double y,double xx,double yy,const Color&c,int m){auto d=Base(DrawOp::LineAA,c,m);d.values={float(x),float(y),float(xx),float(yy)};Add(d);}
bool DrawRecorder::PolyFill3D(const Point*,int,const Rect*,const Color&,int,int,int){return false;}
void DrawRecorder::FillScanLines(Span*s,int n,const Color&c,int m){if(n<0||n>8192){Fail();return;}for(int i=0;i<n;++i)FillRect(Rect(s[i].mX,s[i].mY,s[i].mWidth,1),c,m);}
#ifdef __ANDROID__
void DrawRecorder::FillScanLinesWithCoverage(Span*,int,const Color&,int,const uint8_t*,int,int,int,int){Fail();}
void DrawRecorder::Blt(Image*i,int x,int y,const Rect&r,const Color&c,int m,bool){
#else
void DrawRecorder::FillScanLinesWithCoverage(Span*,int,const Color&,int,const BYTE*,int,int,int,int){Fail();}
void DrawRecorder::Blt(Image*i,int x,int y,const Rect&r,const Color&c,int m){
#endif
 auto d=Base(DrawOp::Blt,c,m);d.src=Box(r);d.values[0]=float(x);d.values[1]=float(y);Add(d,i);}
#ifdef __ANDROID__
void DrawRecorder::BltMirror(Image*i,int x,int y,const Rect&r,const Color&c,int m,bool){
#else
void DrawRecorder::BltMirror(Image*i,int x,int y,const Rect&r,const Color&c,int m){
#endif
 auto d=Base(DrawOp::Mirror,c,m);d.src=Box(r);d.values[0]=float(x);d.values[1]=float(y);Add(d,i);}
void DrawRecorder::BltF(Image*i,float x,float y,const Rect&r,const Rect&clip,const Color&c,int m){auto d=Base(DrawOp::BltF,c,m);d.src=Box(r);d.clip=Box(clip);d.values[0]=x;d.values[1]=y;Add(d,i);}
void DrawRecorder::BltRotated(Image*i,float x,float y,const Rect&r,const Rect&clip,const Color&c,int m,double rotation,float cx,float cy){auto d=Base(DrawOp::Rotate,c,m);d.src=Box(r);d.clip=Box(clip);d.values={x,y,rotation,cx,cy};Add(d,i);}
void DrawRecorder::StretchBlt(Image*i,const Rect&dst,const Rect&src,const Rect&clip,const Color&c,int m,bool fast){auto d=Base(DrawOp::Stretch,c,m);d.dst=Box(dst);d.src=Box(src);d.clip=Box(clip);d.flags=fast;Add(d,i);}
void DrawRecorder::StretchBltMirror(Image*i,const Rect&dst,const Rect&src,const Rect&clip,const Color&c,int m,bool fast){auto d=Base(DrawOp::StretchMirror,c,m);d.dst=Box(dst);d.src=Box(src);d.clip=Box(clip);d.flags=fast;Add(d,i);}
void DrawRecorder::BltMatrix(Image*i,float x,float y,const SexyMatrix3&t,const Rect&clip,const Color&c,int m,const Rect&src,bool blend){auto d=Base(DrawOp::Matrix,c,m);d.src=Box(src);d.clip=Box(clip);d.values={x,y,t.m00,t.m01,t.m02,t.m10,t.m11,t.m12};d.flags=blend;Add(d,i);}
void DrawRecorder::BltTrianglesTex(Image*i,const TriVertex v[][3],int n,const Rect&clip,const Color&c,int m,float x,float y,bool blend){if(n<0||n>4096){Fail();return;}auto d=Base(DrawOp::Triangles,c,m);d.clip=Box(clip);d.values[0]=x;d.values[1]=y;d.flags=blend;d.vertices.assign(&v[0][0],&v[0][0]+n*3);Add(std::move(d),i);}
void DrawRecorder::Text(const std::string&t,int x,int y,int height,bool bold,const Color&c,const Rect&clip){auto d=Base(DrawOp::Text,c,0);d.text=t;d.values={float(x),float(y),float(height)};d.flags=bold;d.clip=Box(clip);Add(std::move(d));}
bool CaptureDrawFrame(Board*b,DrawFrame&f){if(!b||!b->mApp)return false;f.assets.clear();f.commands.clear();f.motions.clear();f.animations.clear();f.flags=uint8_t((f.flags&4)|(b->mPaused?1:0)|(b->mLevelAwardSpawned?2:0));f.wave=uint16_t(std::max(0,b->mCurrentWave));f.total=uint16_t(std::max(0,b->mNumWaves));
 DrawRecorder target(b->mApp,f);Graphics g(&target);g.SetLinearBlend(true);const auto rng=Sexy::GetRandState();b->DrawGameObjects(&g);Sexy::RestoreRandState(rng);return target.Good()&&!f.commands.empty();}
DrawScene::DrawScene(LawnApp*a):assets(a){}
DrawScene::~DrawScene()=default;
std::size_t DrawScene::Bytes(const DrawFrame&f)const {size_t n=sizeof(f)+f.assets.capacity()*sizeof(AssetKey)+f.commands.capacity()*sizeof(DrawCommand);for(auto&d:f.commands)n+=d.vertices.capacity()*sizeof(TriVertex)+d.text.capacity();n+=f.motions.capacity()*sizeof(Motion)+f.animations.capacity()*sizeof(VisualAnimation);for(auto&a:f.animations)n+=a.tracks.capacity()*sizeof(VisualTrack);return n;}
bool DrawScene::Validate(const DrawFrame&f)const {
 if(f.assets.size()>512||f.commands.empty()||f.commands.size()>8192||f.flags>7||f.wave>f.total||Bytes(f)>4*1024*1024)return false;
 if(f.motions.size()>2048||f.animations.size()>512)return false;
 for(auto&m:f.motions)if(m.kind<1||m.kind>3||!m.id||!Finite(m.vx)||!Finite(m.vy)||!Finite(m.ay)||!std::isfinite(m.life)||m.life<0||m.life>60)return false;
 size_t tracks=0;
 for(auto&a:f.animations){
  tracks+=a.tracks.size();if(tracks>16384||a.type>=gReanimatorDefCount||!a.count||a.loop>5||a.filter>NUM_FILTER_EFFECTS||a.flags>3||!Finite(a.time)||a.time<0||a.time>1||!Finite(a.rate)||!std::all_of(a.matrix.begin(),a.matrix.end(),Finite))return false;
  for(auto&t:a.tracks)if(t.index>=1024||t.flags>15||(t.image!=65535&&t.image>=f.assets.size())||t.blend>t.blendTotal||!Finite(t.shakeX)||!Finite(t.shakeY)||!std::all_of(t.transform.begin(),t.transform.end(),Finite))return false;
 }
 size_t vertices=0,text=0;for(auto&k:f.assets)if(k.kind<1||k.kind>9||k.kind==5||k.filters>255)return false;
 for(auto&d:f.commands){if(d.op>DrawOp::AnimationTrack||d.motion>f.motions.size()||d.mode>1||d.flags>1||!Bounds(d.src)||!Bounds(d.dst)||!Bounds(d.clip)||!std::all_of(d.values.begin(),d.values.end(),Finite))return false;
  if(d.op==DrawOp::AnimationTrack&&(d.asset>=f.animations.size()||d.src[0]<0||size_t(d.src[0])>=f.animations[d.asset].tracks.size()))return false;
  const bool image=d.op>=DrawOp::Blt&&d.op<=DrawOp::StretchMirror;if(image&&d.asset>=f.assets.size())return false;
  vertices+=d.vertices.size();text+=d.text.size();if(vertices>16384||text>4096||d.text.size()>1024||d.vertices.size()%3)return false;
  for(auto&v:d.vertices)if(!Finite(v.x)||!Finite(v.y)||!Finite(v.u)||!Finite(v.v)||v.u<0||v.u>1||v.v<0||v.v>1)return false;
  if(d.op==DrawOp::Text&&(d.values[2]<1||d.values[2]>96))return false;
 }return true;
}
bool DrawScene::Prepare(const DrawFrame&f)try{RandomGuard randomGuard;if(!Validate(f))return false;std::vector<Image*> next;
 for(auto&key:f.assets){auto*p=assets.Resolve(key);if(!p)return false;next.push_back(p);}
 for(auto&d:f.commands)if(d.op>=DrawOp::Blt&&d.op<=DrawOp::StretchMirror&&d.op!=DrawOp::Triangles){auto*p=next[d.asset];auto&r=d.src;if(r[0]<0||r[1]<0||r[2]<=0||r[3]<=0||r[0]>p->mWidth-r[2]||r[1]>p->mHeight-r[3])return false;}
 std::vector<std::unique_ptr<Reanimation>> visual;size_t allocatedTracks=0;
 for(auto&a:f.animations){
  ReanimatorEnsureDefinitionLoaded(ReanimationType(a.type),false);auto&def=gReanimatorDefArray[a.type];
  allocatedTracks+=def.mTracks.count;if(def.mTracks.count<=0||def.mTracks.count>1024||allocatedTracks>32768)return false;
  if(size_t(a.first)+a.count>size_t(def.mTracks.tracks[0].mTransforms.count))return false;
  for(auto&t:a.tracks)if(t.index>=def.mTracks.count||size_t(a.first)+a.count>size_t(def.mTracks.tracks[t.index].mTransforms.count))return false;
  std::unique_ptr<Reanimation> r(new Reanimation());r->ReanimationInitializeType(0,0,ReanimationType(a.type));
  r->mFrameStart=a.first;r->mFrameCount=a.count;r->mLoopType=ReanimLoopType(a.loop);r->mAnimTime=a.time;r->mAnimRate=a.rate;
  auto&m=r->mOverlayMatrix;m.m00=a.matrix[0];m.m01=a.matrix[1];m.m02=a.matrix[2];m.m10=a.matrix[3];m.m11=a.matrix[4];m.m12=a.matrix[5];
  r->mColorOverride=ColorOf(a.color);r->mExtraAdditiveColor=ColorOf(a.additive);r->mExtraOverlayColor=ColorOf(a.overlay);
  r->mEnableExtraAdditiveDraw=a.flags&1;r->mEnableExtraOverlayDraw=a.flags&2;r->mFilterEffect=FilterEffect(int(a.filter)-1);
  for(auto&t:a.tracks){auto&v=r->mTrackInstances[t.index];v.mTrackColor=ColorOf(t.color);v.mImageOverride=t.image==65535?nullptr:next[t.image];
   v.mIgnoreClipRect=t.flags&1;v.mTruncateDisappearingFrames=t.flags&2;v.mIgnoreColorOverride=t.flags&4;v.mIgnoreExtraAdditiveColor=t.flags&8;
   v.mShakeX=t.shakeX;v.mShakeY=t.shakeY;v.mBlendCounter=t.blend;v.mBlendTime=t.blendTotal;
   auto&b=v.mBlendTransform;b.mTransX=t.transform[0];b.mTransY=t.transform[1];b.mSkewX=t.transform[2];b.mSkewY=t.transform[3];b.mScaleX=t.transform[4];b.mScaleY=t.transform[5];b.mFrame=t.transform[6];b.mAlpha=t.transform[7];
  }visual.push_back(std::move(r));
 }
 current=f;images=std::move(next);animations=std::move(visual);predictionAge=0;return true;
}catch(const std::bad_alloc&){return false;}
void DrawScene::Clear(){animations.clear();current={};images.clear();predictionAge=0;}
void DrawScene::Draw(Graphics*g){if(!g)return;auto*dest=g->mDestImage;
 const double age=(current.flags&1)||!std::isfinite(predictionAge)?0:std::clamp(predictionAge,0.0,1.e9);
 for(size_t j=0;j<animations.size();++j){auto&r=*animations[j];auto&a=current.animations[j];double t=a.time+age*a.rate/a.count;
  if(a.loop==REANIM_LOOP||a.loop==REANIM_LOOP_FULL_LAST_FRAME)t-=std::floor(t);else t=std::clamp(t,0.0,1.0);r.mAnimTime=float(t);
  for(auto&v:a.tracks)r.mTrackInstances[v.index].mBlendCounter=int(std::max(0.0,double(v.blend)-age*100));
 }
 const float baseX=g->mTransX,baseY=g->mTransY;
 for(auto&d:current.commands){
  float dx=0,dy=0;if(d.motion){auto&m=current.motions[d.motion-1];if(m.life>0&&age>m.life)continue;
   dx=float(std::clamp(age*m.vx,-4096.0,4096.0));dy=float(std::clamp(age*m.vy+age*age*m.ay*.5,-4096.0,4096.0));}
  const float x=baseX+dx,y=baseY+dy;
  auto color=ColorOf(d.color);Rect src=RectOf(d.src),dst=RectOf(d.dst),clip=RectOf(d.clip);dst.Offset(int(x),int(y));clip.Offset(int(x),int(y));Image*i=d.asset<images.size()?images[d.asset]:nullptr;const auto&v=d.values;
  clip=clip.Intersection(g->mClipRect).Intersection(Rect(0,0,dest->mWidth,dest->mHeight));
  if(clip.mWidth<=0||clip.mHeight<=0)continue;
  Graphics safe(*g);safe.mTransX=x;safe.mTransY=y;safe.mClipRect=clip;safe.SetColor(color);safe.SetColorizeImages(true);safe.SetDrawMode(d.mode);

  switch(d.op){case DrawOp::AnimationTrack:{
   auto&r=*animations[d.asset];auto&t=current.animations[d.asset].tracks[d.src[0]];
   safe.mTransX=x+float(v[0]);safe.mTransY=y+float(v[1]);safe.SetColorizeImages(d.flags);safe.SetLinearBlend(true);
#ifdef __ANDROID__
   PvzpTriangleGroup triangles;
#else
   TodTriangleGroup triangles;
#endif
   r.DrawTrack(&safe,t.index,0,&triangles);triangles.DrawGroup(&safe);break;}
  case DrawOp::Fill:safe.FillRect(RectOf(d.dst));break;case DrawOp::Rect:safe.DrawRect(RectOf(d.dst));break;
  case DrawOp::Line:safe.DrawLine(int(v[0]),int(v[1]),int(v[2]),int(v[3]));break;case DrawOp::LineAA:safe.DrawLineAA(int(v[0]),int(v[1]),int(v[2]),int(v[3]));break;
  case DrawOp::Blt:safe.DrawImage(i,int(v[0]),int(v[1]),src);break;case DrawOp::Mirror:safe.DrawImageMirror(i,int(v[0]),int(v[1]),src,true);break;
  case DrawOp::BltF:dest->BltF(i,v[0]+x,v[1]+y,src,clip,color,d.mode);break;
  case DrawOp::Rotate:dest->BltRotated(i,v[0]+x,v[1]+y,src,clip,color,d.mode,v[2],v[3],v[4]);break;
  case DrawOp::Stretch:dest->StretchBlt(i,dst,src,clip,color,d.mode,d.flags);break;case DrawOp::StretchMirror:dest->StretchBltMirror(i,dst,src,clip,color,d.mode,d.flags);break;
  case DrawOp::Matrix:{SexyMatrix3 m;m.LoadIdentity();m.m00=v[2];m.m01=v[3];m.m02=v[4];m.m10=v[5];m.m11=v[6];m.m12=v[7];dest->BltMatrix(i,v[0]+x,v[1]+y,m,clip,color,d.mode,src,d.flags);break;}
  case DrawOp::Triangles:dest->BltTrianglesTex(i,reinterpret_cast<const TriVertex(*)[3]>(d.vertices.data()),int(d.vertices.size()/3),clip,color,d.mode,v[0]+x,v[1]+y,d.flags);break;
  case DrawOp::Text:{Graphics text(g?*g:Graphics());text.SetClipRect(clip);text.SetColor(color);
#ifdef __ANDROID__
   text.SetFont(FONT_BRIANNETOD16);text.DrawString(d.text,int(v[0]),int(v[1]));
#else
   ModText::DrawTextWide(&text,ModText::GetFont(int(v[2]),d.flags),int(v[0]),int(v[1]),ModText::WideFromUtf8(d.text.c_str()),color,clip);
#endif
   break;}
  }
 }
}
#include "ObserverPrediction.inl"

}
