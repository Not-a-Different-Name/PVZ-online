#include "../System/PerformanceMonitor.h"
#include <chrono>
#include "World.h"
#include "WorldSchema.h"
#include "ObserverIsolation.h"
#include "../Run/RunSpeciesRuntime.h"
#include "../Run/RunDotEffects.h"
#include "../Run/DeferredZombieSpawnSave.h"
#include "../Run/DeferredPlantSetupSave.h"
#include "../System/PoolEffect.h"
#include "../System/Music.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../MessageWidget.h"
#include "../ZenGarden.h"
#include "misc/MTRand.h"
#ifndef __ANDROID__
#include "graphics/DDInterface.h"
#include "zlib/zlib.h"
#else
#include <zlib.h>
#endif
#ifdef __ANDROID__
#include "PvzpLib/Trail.h"
#include "PvzpLib/PvzpFoley.h"
#include "PvzpLib/PvzpParticle.h"
#else
#include "Sexy.TodLib/Trail.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/TodParticle.h"
#endif
#include <set>
#include <algorithm>
namespace ObserverWorld {
struct Context {
 LawnApp* old;EffectSystem* effect;Sexy::SexyAppBase* sexy;std::string random;ObserverIsolation::Fence fence;std::string* local;
 explicit Context(LawnApp*a,std::string* rng=nullptr,LawnApp*render=nullptr):old(gLawnApp),effect(gEffectSystem),sexy(Sexy::gSexyAppBase),random(Sexy::GetRandState()),local(rng){gLawnApp=a;Sexy::gSexyAppBase=render?render:a;
#ifdef __ANDROID__
 gEffectSystem=a->mEffectSystem.get();
#else
 gEffectSystem=a->mEffectSystem;
#endif
 if(local&&!local->empty())Sexy::RestoreRandState(*local);}
 ~Context(){if(local)*local=Sexy::GetRandState();Sexy::RestoreRandState(random);gLawnApp=old;gEffectSystem=effect;Sexy::gSexyAppBase=sexy;}
};

template<class T>T& Item(DataArray<T>&p,unsigned i){
#ifdef __ANDROID__
 return p.DataArrayGetItemAt(i);
#else
 return p.mBlock[i].mItem;
#endif
}
template<class T>unsigned& PoolID(DataArray<T>&p,unsigned i){
#ifdef __ANDROID__
 return p.DataArrayGetIDAt(i);
#else
 return p.mBlock[i].mID;
#endif
}
template<class T>T* ResetItem(DataArray<T>&p,unsigned i){
#ifdef __ANDROID__
 return &p.DataArrayResetItemAt(i);
#else
 std::memset(&p.mBlock[i].mItem,0,sizeof(T));return new(&p.mBlock[i].mItem)T();
#endif
}
template<class T>std::vector<T*> Live(DataArray<T>&p){std::vector<T*>v;for(unsigned i=0;i<p.mMaxUsedCount;++i)if(PoolID(p,i)&0xffff0000u)v.push_back(&Item(p,i));return v;}
template<class T>T* Pointer(T*p){return p;}
template<class T>T* Pointer(const std::unique_ptr<T>&p){return p.get();}

template<class T>void Pool(Wire&io,DataArray<T>&pool){
 uint32_t count=pool.mSize,key=pool.mNextKey;io.Value(count);io.Value(key);
 if(!io.ok||count>pool.mMaxSize||!key||key>65535){io.ok=false;return;}
 if(!io.reading){for(T*p:Live(pool)){uint32_t id=pool.DataArrayGetID(p);io.Value(id);io.Value(*p);}return;}
 pool.DataArrayFreeAll();for(unsigned i=0;i<pool.mMaxSize;++i)PoolID(pool,i)=0;std::set<unsigned> ids;unsigned used=0;
 for(unsigned i=0;i<count&&io.ok;++i){uint32_t id=0;io.Value(id);unsigned at=id&65535;if(!(id>>16)||at>=pool.mMaxSize||!ids.insert(at).second){io.ok=false;return;}T*item=ResetItem(pool,at);if constexpr(requires{item->mApp;}){item->mApp=gLawnApp;item->mBoard=gLawnApp->mBoard;}PoolID(pool,at)=id;used=std::max(used,at+1);pool.mMaxUsedCount=used;pool.mSize=i+1;io.Value(*item);}
 pool.mMaxUsedCount=used;pool.mSize=count;pool.mNextKey=key;pool.mFreeListHead=used;
 for(unsigned i=used;i>0;--i)if(!ids.count(i-1)){PoolID(pool,i-1)=pool.mFreeListHead;pool.mFreeListHead=i-1;}
}
// Animation definitions stay local. Restore a trusted semantic resource and all
// numeric phase/track state; never deserialize definition or holder pointers.
void Images(Wire&io,Observer::DrawAssets&assets){io.image=[&](Sexy::Image*&p){Observer::AssetKey key;bool present=p!=nullptr;if(!io.reading&&present&&!assets.Identify(p,key))present=false;io.Value(present);if(!present){if(io.reading)p=nullptr;return;}io.Value(key.kind);io.Value(key.id);io.Value(key.item);io.Value(key.filters);if(io.reading){p=assets.Resolve(key);if(!p)io.ok=false;}};}
void AnimationPool(Wire&io,LawnApp*app){
 auto&pool=app->mEffectSystem->mReanimationHolder->mReanimations;uint32_t count=pool.mSize,key=pool.mNextKey;io.Value(count);io.Value(key);if(count>pool.mMaxSize||!key||key>65535){io.ok=false;return;}
 std::vector<Reanimation*> list;
 if(!io.reading){list=Live(pool);}else {pool.DataArrayFreeAll();for(unsigned i=0;i<pool.mMaxSize;++i)PoolID(pool,i)=0;}
 std::set<unsigned>ids;unsigned used=0;
 for(unsigned i=0;i<count&&io.ok;++i){uint32_t id=io.reading?0:pool.DataArrayGetID(list[i]);io.Value(id);unsigned at=id&65535;if(!(id>>16)||at>=pool.mMaxSize||!ids.insert(at).second){io.ok=false;return;}
  Reanimation*r=io.reading?ResetItem(pool,at):list[i];if(io.reading){PoolID(pool,at)=id;used=std::max(used,at+1);pool.mMaxUsedCount=used;pool.mSize=i+1;}
  uint16_t wire=0;if(!io.reading&&!Observer::ReanimationToWire(uint32_t(r->mReanimationType),wire)){io.ok=false;return;}io.Value(wire);
  if(io.reading){ReanimationType type;if(!Observer::ReanimationFromWire(wire,type)){io.ok=false;return;}r->ReanimationInitializeType(0,0,type);r->mReanimationHolder=Pointer(app->mEffectSystem->mReanimationHolder);}
  io.Value(*r);uint32_t tracks=r->mDefinition?r->mDefinition->mTracks.count:0;io.Value(tracks);if(!r->mDefinition||tracks!=uint32_t(r->mDefinition->mTracks.count)||tracks>1024||r->mFrameStart<0||r->mFrameCount<1||r->mFrameStart>r->mDefinition->mTracks.tracks[0].mTransforms.count||r->mFrameCount>r->mDefinition->mTracks.tracks[0].mTransforms.count-r->mFrameStart){io.ok=false;return;}
  for(unsigned t=0;t<tracks;++t){io.Value(r->mTrackInstances[t]);}
 }
 if(io.reading){pool.mMaxUsedCount=used;pool.mSize=count;pool.mNextKey=key;pool.mFreeListHead=used;for(unsigned i=used;i>0;--i)if(!ids.count(i-1)){PoolID(pool,i-1)=pool.mFreeListHead;pool.mFreeListHead=i-1;}}
}
void State(Wire&io,Board*b,std::string&rng){
 auto*a=b->mApp;uint32_t magic=0x31575350,version=2;io.Value(magic);io.Value(version);if(magic!=0x31575350||version!=2){io.ok=false;return;}
 io.Value(a->mGameMode);io.Value(a->mGameScene);io.Scalar<int32_t>(a->mAppRandSeed);io.Scalar<int32_t>(a->mPlayerInfo->mLevel);io.Scalar<int32_t>(a->mPlayerInfo->mFinishedAdventure);
 bool run=a->GetRunState()!=nullptr;io.Value(run);
 if(io.reading){
#ifdef __ANDROID__
  if(run)a->mRunState=std::make_unique<RunState>();else a->mRunState.reset();
#else
  delete a->mRunState;a->mRunState=run?new RunState:nullptr;
#endif
 }
 if(run)io.Value(*a->GetRunState());io.Value(*b);io.Value(*b->mChallenge);io.Value(*b->mSeedBank);
 AnimationPool(io,a);Pool(io,b->mPlants);Pool(io,b->mZombies);/* Existing bullets are intentionally omitted; ordinary plants refill the scene locally. */Pool(io,b->mCoins);Pool(io,b->mLawnMowers);Pool(io,b->mGridItems);
 Pool(io,a->mEffectSystem->mAttachmentHolder->mAttachments);if(io.reading){for(auto*attachment:Live(a->mEffectSystem->mAttachmentHolder->mAttachments)){if(attachment->mNumEffects<0||attachment->mNumEffects>MAX_EFFECTS_PER_ATTACHMENT){attachment->mNumEffects=0;io.ok=false;return;}int kept=0;for(int i=0;i<attachment->mNumEffects;++i)if(attachment->mEffectArray[i].mEffectType==EFFECT_REANIM||attachment->mEffectArray[i].mEffectType==EFFECT_ATTACHMENT)attachment->mEffectArray[kept++]=attachment->mEffectArray[i];attachment->mNumEffects=kept;}}
 auto trailer=[&](auto save,auto load){std::vector<unsigned char> blob;if(!io.reading)blob=save(b);io.Value(blob);if(io.reading&&io.ok&&!load(b,blob.data(),blob.size()))io.ok=false;};
 trailer(RunSpeciesRuntime::SaveTrailer,RunSpeciesRuntime::LoadTrailer);
 {struct Preserve{Board*b;decltype(Board::mRunDotStates) state;~Preserve(){b->mRunDotStates.swap(state);}};if(io.reading)trailer(RunDotEffects::SaveTrailer,RunDotEffects::LoadTrailer);else{Preserve guard{b,b->mRunDotStates};trailer(RunDotEffects::SaveTrailer,RunDotEffects::LoadTrailer);}}

 trailer(DeferredZombieSpawnSave::SaveTrailer,DeferredZombieSpawnSave::LoadTrailer);
 trailer(DeferredPlantSetupSave::SaveTrailer,DeferredPlantSetupSave::LoadTrailer);
 // Source marker sets may retain IDs until the next relayed spawn. Only
 // serialize live objects; do not mutate the real battle's set while capturing.
 std::vector<unsigned> relayedIds;
 if(!io.reading){for(auto id:b->mRelayedZombieIds){auto*z=b->mZombies.DataArrayTryToGet(id);if(z&&!z->mDead)relayedIds.push_back(id);}std::sort(relayedIds.begin(),relayedIds.end());}
 uint32_t relayed=uint32_t(relayedIds.size());io.Value(relayed);if(relayed>b->mZombies.mMaxSize){io.ok=false;return;}
 if(io.reading){b->mRelayedZombieIds.clear();for(unsigned i=0;i<relayed&&io.ok;++i){uint32_t id=0;io.Value(id);if(!b->mZombies.DataArrayTryToGet(id)||!b->mRelayedZombieIds.insert(id).second)io.ok=false;}}
 else for(auto id:relayedIds)io.Value(id);
}
bool Capture(Board*b,std::vector<uint8_t>&out)try{
 PerformanceMonitor::Scope monitor(PerformanceMonitor::ObserverCapture);if(b)monitor.Objects(b->mPlants.mSize,b->mZombies.mSize,b->mProjectiles.mSize);
 if(!b||!b->mApp||!b->mApp->mPlayerInfo)return false;
 Observer::DrawAssets assets(b->mApp);Wire io;Images(io,assets);std::string rng=Sexy::GetRandState();State(io,b,rng);if(!io.End()||io.bytes.size()>1024*1024)return false;
 out.resize(compressBound(uLong(io.bytes.size()))+5);out[0]=5;uint32_t raw=uint32_t(io.bytes.size());for(int i=0;i<4;++i)out[i+1]=uint8_t(raw>>(i*8));uLongf n=out.size()-5;
 if(compress2(out.data()+5,&n,io.bytes.data(),uLong(io.bytes.size()),6)!=Z_OK||n+5>65536){out.clear();return false;}out.resize(n+5);return true;
}catch(const std::bad_alloc&){out.clear();return false;}
struct World::Impl {
 LawnApp*owner;LawnApp*app=nullptr;Board*board=nullptr;std::string rng;unsigned accumulated=0;std::unique_ptr<Observer::DrawAssets> assets;
 explicit Impl(LawnApp*o):owner(o){}
 ~Impl(){if(!app)return;Context scope(app,&rng,owner);if(app->mEffectSystem){for(auto*r:Live(app->mEffectSystem->mReanimationHolder->mReanimations)){if(r->mDefinition&&r->mTrackInstances){for(int i=0;i<r->mDefinition->mTracks.count;++i)r->mTrackInstances[i].mAttachmentID=ATTACHMENTID_NULL;}else r->mDead=true;}for(auto*a:Live(app->mEffectSystem->mAttachmentHolder->mAttachments))a->mNumEffects=0;}if(board){app->mBoard=nullptr;delete board;}assets.reset();if(app->mEffectSystem){app->mEffectSystem->EffectSystemDispose();app->mEffectSystem.reset();}if(app->mReanimatorCache){app->mReanimatorCache->ReanimatorCacheDispose();app->mReanimatorCache.reset();}delete app->mPlayerInfo;app->mPlayerInfo=nullptr;
#ifndef __ANDROID__
 app->mDDInterface=nullptr;
#endif
 app->mShutdown=true;delete app;
 }
 bool Init(){Context context(owner);app=new LawnApp;{Context scope(app,&rng,owner);app->mGameMode=GAMEMODE_ADVENTURE;app->mGameScene=GameScenes::SCENE_PLAYING;app->mMuteSoundsForCutscene=true;app->mPlayerInfo=new PlayerInfo;app->mPlayerInfo->mFinishedAdventure=1;app->mPlayerInfo->mLevel=50;app->mEffectSystem=std::make_unique<EffectSystem>();app->mEffectSystem->EffectSystemInitialize();app->mReanimatorCache=std::make_unique<ReanimatorCache>();app->mReanimatorCache->mApp=app;app->mReanimatorCache->ReanimatorCacheInitialize();app->mMusic=std::make_unique<Music>();app->mMusic->mApp=app;app->mMusic->mMusicInterface=nullptr;app->mMusic->mMusicDisabled=true;app->mZenGarden=std::make_unique<ZenGarden>();app->mSoundSystem=std::make_unique<PvzpFoley>();app->mPoolEffect=std::make_unique<PoolEffect>();app->mPoolEffect->PoolEffectInitialize();
#ifndef __ANDROID__
 app->mDDInterface=owner->mDDInterface;
#endif
 assets=std::make_unique<Observer::DrawAssets>(app);board=new Board(app);app->mZenGarden->mApp=app;app->mZenGarden->mBoard=board;board->mWidgetManager=Pointer(app->mWidgetManager);board->mCursorPreview->mApp=app;board->mCursorPreview->mBoard=board;board->mCursorObject->mApp=app;board->mCursorObject->mBoard=board;board->mChallenge->mApp=app;board->mChallenge->mBoard=board;board->mCutScene->mApp=app;board->mCutScene->mBoard=board;board->mSeedBank->mApp=app;board->mSeedBank->mBoard=board;
 }return true;}
 bool Reject(int line){std::fprintf(stderr,"[observer] world validation rejected (%d)\n",line);return false;}
 bool Validate(){if(app->mGameMode!=GAMEMODE_ADVENTURE||app->mGameScene!=GameScenes::SCENE_PLAYING||board->mBackground<BACKGROUND_1_DAY||board->mBackground>BACKGROUND_6_BOSS||board->mLevel<1||board->mLevel>50)return Reject(__LINE__);if(board->mNumWaves<0||board->mNumWaves>MAX_ZOMBIE_WAVES||board->mCurrentWave<0||board->mCurrentWave>board->mNumWaves||board->mSeedBank->mNumPackets<0||board->mSeedBank->mNumPackets>10)return Reject(__LINE__);
  if(auto*r=app->GetRunState()){if(r->mMode<0||r->mMode>3||r->mDiff<0||r->mDiff>2||r->mEndlessScene<0||r->mEndlessScene>4||r->mLevelIndex<0||r->mLevelIndex>=RunState::RUN_ENDLESS_LEVEL_COUNT)return Reject(__LINE__);for(auto&list:{r->mBuffs,r->mLoanBuffs}){std::set<int>ids;for(auto&b:list){if(b.mId>=int(RUN_BUFF_COUNT)+int(RUN_PLANT_UPGRADE_COUNT)+int(RUN_PLANT_BUFF2_COUNT)||!ids.insert(b.mId).second)return Reject(__LINE__);int max=GetRunChoiceMaxStacks(b.mId);if(max>0&&b.mCount>max)return Reject(__LINE__);}}}
  auto fix=[&](auto&pool){for(auto*p:Live(pool)){p->mApp=app;p->mBoard=board;if constexpr(requires{p->mRow;})if(p->mRow<(std::is_same_v<std::remove_reference_t<decltype(*p)>,Coin>?-1:0)||p->mRow>=6)return Reject(__LINE__);}return true;};
  if(!fix(board->mPlants)||!fix(board->mZombies)||!fix(board->mProjectiles)||!fix(board->mCoins)||!fix(board->mLawnMowers)||!fix(board->mGridItems))return Reject(__LINE__);
  for(int i=0;i<board->mSeedBank->mNumPackets;++i){board->mSeedBank->mSeedPackets[i].mApp=app;board->mSeedBank->mSeedPackets[i].mBoard=board;auto&packet=board->mSeedBank->mSeedPackets[i];if(packet.mPacketType<SEED_NONE||packet.mPacketType>=NUM_SEED_TYPES||packet.mImitaterType<SEED_NONE||packet.mImitaterType>=NUM_SEED_TYPES||packet.mSlotMachiningNextSeed<SEED_NONE||packet.mSlotMachiningNextSeed>=NUM_SEED_TYPES)return Reject(__LINE__);packet.mIndex=i;}
  for(Plant*p:Live(board->mPlants))if(p->mSeedType<0||p->mSeedType>=NUM_SEED_TYPES||p->mPlantCol<0||p->mPlantCol>=9||p->mState<STATE_NOTREADY||p->mState>STATE_LILYPAD_INVULNERABLE||p->mSubclass<SUBCLASS_NORMAL||p->mSubclass>SUBCLASS_SHOOTER||p->mOnBungeeState<NOT_ON_BUNGEE||p->mOnBungeeState>RISING_WITH_BUNGEE||p->mStartRow<0||p->mStartRow>=6||p->mNumFrames<0||p->mNumFrames>1000||p->mFrameLength<0||p->mFrameLength>100000)return Reject(__LINE__);
  for(Zombie*z:Live(board->mZombies))if(z->mZombieType<0||z->mZombieType>=NUM_ZOMBIE_TYPES||z->mZombiePhase<PHASE_ZOMBIE_NORMAL||z->mZombiePhase>PHASE_SQUASH_DONE_FALLING||z->mZombieHeight<HEIGHT_ZOMBIE_NORMAL||z->mZombieHeight>HEIGHT_ZOMBIQUARIUM||std::abs(z->mPosX)>20000||std::abs(z->mPosY)>20000||std::abs(z->mVelX)>10000||std::abs(z->mVelZ)>10000||z->mScaleZombie<=0||z->mScaleZombie>1000||z->mTargetRow < -1||z->mTargetRow>=6||z->mTargetCol < -1||z->mTargetCol>=9||z->mUseLadderCol < -1||z->mUseLadderCol>=9||z->mFireballRow < -1||z->mFireballRow>=6)return Reject(__LINE__);
  for(auto*p:Live(board->mProjectiles))if(p->mProjectileType<PROJECTILE_PEA||p->mProjectileType>=NUM_PROJECTILES||p->mMotionType<MOTION_STRAIGHT||p->mMotionType>MOTION_HOMING||p->mCobTargetRow<0||p->mCobTargetRow>=6||p->mNumFrames<0||p->mNumFrames>1000||p->mAnimTicksPerFrame<0||p->mAnimTicksPerFrame>100000||std::abs(p->mPosX)>20000||std::abs(p->mPosY)>20000||std::abs(p->mVelX)>10000||std::abs(p->mVelY)>10000)return Reject(__LINE__);
  for(auto*c:Live(board->mCoins))if(c->mType<COIN_NONE||c->mType>COIN_PRESENT_SURVIVAL_MODE||c->mUsableSeedType<SEED_NONE||c->mUsableSeedType>=NUM_SEED_TYPES||c->mScale<0||c->mScale>1000||std::abs(c->mPosX)>20000||std::abs(c->mPosY)>20000)return Reject(__LINE__);
  for(auto*g:Live(board->mGridItems))if(g->mGridItemType<GRIDITEM_NONE||g->mGridItemType>GRIDITEM_IZOMBIE_BRAIN||g->mGridX<0||g->mGridX>=9||g->mGridY<0||g->mGridY>=6||g->mMotionTrailCount<0||g->mMotionTrailCount>NUM_MOTION_TRAIL_FRAMES)return Reject(__LINE__);
  for(auto*m:Live(board->mLawnMowers))if(m->mMowerState<MOWER_ROLLING_IN||m->mMowerState>MOWER_SQUISHED||m->mMowerType<LAWNMOWER_LAWN||m->mMowerType>=NUM_MOWER_TYPES)return Reject(__LINE__);
  auto&rp=app->mEffectSystem->mReanimationHolder->mReanimations;auto&ap=app->mEffectSystem->mAttachmentHolder->mAttachments;
  auto validAnim=[&](ReanimationID id){return id==REANIMATIONID_NULL||rp.DataArrayTryToGet(uint32_t(id))!=nullptr;};
  for(auto*z:Live(board->mZombies)){for(auto id:{z->mBodyReanimID,z->mBossFireBallReanimID,z->mSpecialHeadReanimID,z->mPaperHelmReanimID,z->mZombatarHeadReanimID,z->mMoweredReanimID})if(!validAnim(id))return Reject(__LINE__);if(!z->mDead){auto*body=rp.DataArrayTryToGet(uint32_t(z->mBodyReanimID));if(!body)return Reject(__LINE__);auto expected=GetZombieDefinition(z->mZombieType).mReanimationType;if(body->mReanimationType!=expected&&!(z->mZombieType==ZOMBIE_BOBSLED&&body->mReanimationType==REANIM_ZOMBIE))return Reject(__LINE__);}}
  for(auto*p:Live(board->mPlants))for(auto id:{p->mBodyReanimID,p->mHeadReanimID,p->mHeadReanimID2,p->mHeadReanimID3,p->mBlinkReanimID,p->mLightReanimID,p->mSleepingReanimID})if(!validAnim(id))return Reject(__LINE__);
  for(auto*r:Live(rp))if(r->mLoopType<REANIM_LOOP||r->mLoopType>REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD||r->mAnimTime<0||r->mAnimTime>1.001||std::abs(r->mAnimRate)>100000||r->mFilterEffect<FILTER_EFFECT_NONE||r->mFilterEffect>=NUM_FILTER_EFFECTS||std::abs(r->mOverlayMatrix.m00)>1000||std::abs(r->mOverlayMatrix.m11)>1000||std::abs(r->mOverlayMatrix.m01)>1000||std::abs(r->mOverlayMatrix.m10)>1000||std::abs(r->mOverlayMatrix.m02)>20000||std::abs(r->mOverlayMatrix.m12)>20000||r->mFrameBasePose < -2||r->mFrameBasePose>=r->mDefinition->mTracks.tracks[0].mTransforms.count)return Reject(__LINE__);
  std::set<uint64_t> active,done;std::function<bool(uint64_t,unsigned)> visit=[&](uint64_t key,unsigned depth){if(depth>24||active.count(key))return Reject(__LINE__);if(done.count(key))return true;active.insert(key);bool good=true;if(key>>32){auto*a=ap.DataArrayTryToGet(unsigned(key));if(a){for(int i=0;i<a->mNumEffects&&good;++i){auto&e=a->mEffectArray[i];good=visit(uint64_t(e.mEffectID)|(e.mEffectType==EFFECT_ATTACHMENT?uint64_t(1)<<32:0),depth+1);}}}else{auto*r=rp.DataArrayTryToGet(unsigned(key));if(r&&r->mTrackInstances)for(int i=0;i<r->mDefinition->mTracks.count&&good;++i)if(r->mTrackInstances[i].mAttachmentID)good=visit(uint64_t(1)<<32|uint32_t(r->mTrackInstances[i].mAttachmentID),depth+1);}active.erase(key);if(good)done.insert(key);return good;};
  for(auto*r:Live(rp))if(!visit(rp.DataArrayGetID(r),0))return Reject(__LINE__);
  return true;
 }
};
World::World(LawnApp*a):impl(std::make_unique<Impl>(a)){}
World::~World()=default;
bool World::Restore(const std::vector<uint8_t>&bytes)try{
 PerformanceMonitor::Scope monitor(PerformanceMonitor::ObserverRestore);
 if(bytes.size()<6||bytes.size()>65536||bytes[0]!=5)return false;uint32_t n=0;for(int i=0;i<4;++i)n|=uint32_t(bytes[i+1])<<(8*i);if(!n||n>1024*1024)return false;
 std::vector<uint8_t>raw(n);z_stream z{};z.next_in=const_cast<Bytef*>(bytes.data()+5);z.avail_in=unsigned(bytes.size()-5);z.next_out=raw.data();z.avail_out=n;if(inflateInit(&z)!=Z_OK)return false;int result=inflate(&z,Z_FINISH);bool good=result==Z_STREAM_END&&z.total_in==bytes.size()-5&&z.total_out==n;inflateEnd(&z);if(!good)return false;
 auto next=std::make_unique<Impl>(impl->owner);if(!next->Init())return false;{Context scope(next->app,nullptr,next->owner);Wire io(raw);Images(io,*next->assets);State(io,next->board,next->rng);if(!io.End()){std::fprintf(stderr,"[observer] invalid world fields at %zu of %zu\n",io.Position(),raw.size());return false;}if(!next->Validate())return false;Sexy::SRand(uint32_t(next->board->mBoardRandSeed)^uint32_t(next->board->mBoardUpdateCounter));next->rng=Sexy::GetRandState();}impl=std::move(next);return true;
}catch(const std::exception&){return false;}
void World::Advance(unsigned ms){
 if(!impl->board||impl->board->mPaused||!ms)return;
 PerformanceMonitor::Scope monitor(PerformanceMonitor::ObserverAdvance);
 auto*b=impl->board;monitor.Objects(b->mPlants.mSize,b->mZombies.mSize,b->mProjectiles.mSize);
 if(monitor.enabled){auto&e=*impl->app->mEffectSystem;monitor.Effects(e.mReanimationHolder->mReanimations.mSize,e.mParticleHolder->mParticleSystems.mSize,e.mTrailHolder->mTrails.mSize,e.mAttachmentHolder->mAttachments.mSize);}
 Context scope(impl->app,&impl->rng,impl->owner);
 impl->accumulated=std::min(impl->accumulated+ms,200u);
 // Keep catch-up from monopolizing the real battle's main thread. A single
 // engine step is indivisible; discard excess overdue time after the budget.
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(4);
 unsigned steps=0;
 while(impl->accumulated>=10&&impl->app->mGameScene==GameScenes::SCENE_PLAYING){
  if(steps++&&std::chrono::steady_clock::now()>=deadline){impl->accumulated%=10;break;}
  impl->accumulated-=10;impl->app->mEffectSystem->Update();
  if(b->mCobCannonCursorDelayCounter>0)--b->mCobCannonCursorDelayCounter;
  if(b->mOutOfMoneyCounter>0)--b->mOutOfMoneyCounter;
  if(b->mShakeCounter>0)--b->mShakeCounter;
  if(b->mTimeStopCounter>0){
   --b->mTimeStopCounter;b->ProcessDeleteQueue();
   impl->app->mEffectSystem->ProcessDeleteQueue();continue;
  }
  ++b->mEffectCounter;
  if(b->StageHasPool()&&!b->mIceTrapCounter)++impl->app->mPoolEffect->mPoolCounter;
  b->UpdateGridItems();b->UpdateFwoosh();b->UpdateGame();b->UpdateFog();
  b->ProcessDeleteQueue();
  // Mirror LawnApp's normal frame cleanup, including particles, trails,
  // animations and attachments created by speculative attacks.
  impl->app->mEffectSystem->ProcessDeleteQueue();
 }
}
void World::Draw(Sexy::Graphics*g){if(!impl->board)return;PerformanceMonitor::Scope monitor(PerformanceMonitor::ObserverDraw);monitor.Objects(impl->board->mPlants.mSize,impl->board->mZombies.mSize,impl->board->mProjectiles.mSize);Context scope(impl->app,&impl->rng,impl->owner);impl->board->DrawGameObjects(g);}
Board*World::Inspect()const{return impl->board;}
}
