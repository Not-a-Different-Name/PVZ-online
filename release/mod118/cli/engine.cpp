// The CLI adapter links the native game engine.
#define NOMINMAX
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include "LawnApp.h"
#include "Resources.h"
#include "Lawn/BoardInclude.h"
#include "Lawn/CutScene.h"
#include "Lawn/Run/RunState.h"
#include "Lawn/Run/RunBuffs.h"
#include "Lawn/Run/EndlessSaves.h"
#include "Lawn/System/ProfileMgr.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/Widget/GameSelector.h"
#include "Lawn/Widget/RunPickDialog.h"
#include "Lawn/Widget/SeedChooserScreen.h"
#include "Lawn/Online/NetSession.h"
#include "Lawn/Online/NetProtocol.h"
#include "Lawn/Widget/TeamProgressionDialog.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "widget/WidgetManager.h"
#include "widget/ButtonWidget.h"
#include "graphics/MemoryImage.h"
#include "imagelib/ImageLib.h"
#include "misc/SEHCatcher.h"

bool (*gAppCloseRequest)();
bool (*gAppHasUsedCheatKeys)();
SexyString (*gGetCurrentLevelName)();
using namespace Sexy;
namespace fs=std::filesystem;

static std::string Quote(const std::string& s) {
    std::ostringstream o;o<<'"';
    for(unsigned char c:s){switch(c){case '"':o<<"\\\"";break;case '\\':o<<"\\\\";break;
    case '\n':o<<"\\n";break;case '\r':o<<"\\r";break;case '\t':o<<"\\t";break;
    default:if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;else o<<c;}}
    o<<'"';return o.str();
}
static std::string Utf8(const std::wstring& s){
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);
    std::string r(n,0);if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),r.data(),n,nullptr,nullptr);return r;
}
static std::string Utf8(const std::string& s){
    if(s.empty()||MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0)>0)return s;
    int n=MultiByteToWideChar(CP_ACP,0,s.data(),int(s.size()),nullptr,0);std::wstring w(n,0);
    if(n)MultiByteToWideChar(CP_ACP,0,s.data(),int(s.size()),w.data(),n);return Utf8(w);
}
static void Require(bool yes,const char* msg){if(!yes)throw std::runtime_error(msg);}

class CliApp:public LawnApp {
public:
    std::string dataRoot;
    bool served=false;
    unsigned long long ticks=0;
    std::vector<bool> replied;
    std::vector<EndlessSaves::Record> saves;
    std::string details="null", stopped="null";
    Board* observedBoard=nullptr;
    unsigned int boardId=0;
    void ObserveBoard(){
        if(mBoard!=observedBoard){observedBoard=mBoard;++boardId;}
    }
    bool VisibleDrop(const Coin* coin) const {
        return coin&&!coin->mDead&&!coin->mIsBeingCollected&&
            coin->mPosX+coin->mWidth>0&&coin->mPosX<mWidth&&
            coin->mPosY+coin->mHeight>0&&coin->mPosY<mHeight;
    }
    void Audit(const char*when){
        for(auto*w:mWidgetManager->mWidgets)if(auto*c=dynamic_cast<SeedChooserScreen*>(w))
            if(c->mBoard!=mBoard)std::cerr<<"[cli-lifetime] "<<when<<" stale chooser="<<c<<" chooser-board="<<c->mBoard<<" active-board="<<mBoard<<std::endl;
    }
    void InitPropertiesHook()override{
        LawnApp::InitPropertiesHook();mFullCompanyName="PvZOnlineCLI";
        mProdName="PvZOnlineCLI-"+std::to_string(GetCurrentProcessId());mRegKey=mProdName;
        // Music::TodLoadMusic requires BassMusicInterface even with volume zero.
        mOnlyAllowOneCopyToRun=false;mNoSoundNeeded=false;mAutoEnable3D=false;
    }
    bool ChangeDirHook(const char*)override{SetAppDataFolder(dataRoot+"/");return false;}
    void ReadFromRegistry()override{mIsWindowed=true;mForceWindowed=true;mAutoEnable3D=false;mMusicVolume=0;mSfxVolume=0;}
    void WriteToRegistry()override{}
    void DoParseCmdLine()override{mCmdLineParsed=true;}
    void Popup(const std::string& s)override{std::cerr<<s<<std::endl;ExitProcess(3);}
    void Popup(const std::wstring& s)override{Popup(Utf8(s));}
    void ShowResourceError(bool=false)override{Popup("CLI resource loading failed; verify the complete resource directory.");}
    void Tick(int count,int delay=0){
        for(int i=0;i<count&&!mShutdown;++i){
            // CLI owns simulation time; background focus must not pause the real board.
            mActive=true;mMinimized=false;
            Audit("before-update");
            LawnApp::UpdateFrames();if(replied.size()<=1)ProcessSafeDeleteList();++ticks;ObserveBoard();
            // CutScene waits for the first real board draw before preloading.
            // Perform that draw offscreen instead of forging its completion flag.
            if(mBoard&&mBoard->mDrawCount==0){MemoryImage image(this);image.Create(800,600);Graphics g(&image);mBoard->Draw(&g);}
            if(delay)Sleep(delay);
        }
    }
    void Click(int x,int y,int button=1){
        mWidgetManager->mMouseIn=true;mWidgetManager->MouseMove(x,y);
        mWidgetManager->MouseDown(x,y,button);mWidgetManager->MouseUp(x,y,button);
    }
    void Controls(std::ostream&o,WidgetContainer*root,int x,int y,bool&first,int depth=0){
        if(depth>32)return;
        for(auto*w:root->mWidgets){if(!w->mVisible)continue;
            int ax=x+w->mX,ay=y+w->mY;
            if(auto*b=dynamic_cast<ButtonWidget*>(w)){
                if(!first)o<<',';first=false;
                o<<"{\"id\":"<<b->mId<<",\"label\":"<<Quote(Utf8(b->mLabel))<<",\"enabled\":"<<(!b->mDisabled?"true":"false")
                 <<",\"rect\":["<<ax<<','<<ay<<','<<w->mWidth<<','<<w->mHeight<<"]}";
            }
            Controls(o,w,ax,ay,first,depth+1);
            if(auto*d=dynamic_cast<TeamProgressionDialog*>(w))for(const auto&c:d->Controls()){
                if(!first)o<<',';first=false;
                o<<"{\"kind\":\"team\",\"id\":"<<int(c.kind)<<",\"label\":"<<Quote(c.label)<<",\"enabled\":"<<c.enabled
                <<",\"value\":"<<c.value<<",\"rect\":["<<ax+c.rect.mX<<','<<ay+c.rect.mY<<','<<c.rect.mWidth<<','<<c.rect.mHeight<<"]}";
            }
        }
    }
    std::string State(){
        ObserveBoard();
        std::ostringstream o;o<<std::boolalpha<<"{\"build\":"<<NetProto::MOD_BUILD<<",\"cli_protocol\":2,\"tick\":"<<ticks
        <<",\"scene\":"<<int(mGameScene)<<",\"result\":"<<int(mBoardResult)<<",\"data_root\":"<<Quote(Utf8(dataRoot));
        o<<",\"profile\":";
        if(mPlayerInfo)o<<"{\"id\":"<<mPlayerInfo->mId<<",\"name\":"<<Quote(Utf8(mPlayerInfo->mName))<<",\"coins\":"<<mPlayerInfo->mCoins<<",\"adventure_level\":"<<mPlayerInfo->mLevel<<'}';else o<<"null";
        o<<",\"profiles\":[";bool first=true;
        for(auto&[name,p]:mProfileMgr->GetProfileMap()){if(!first)o<<',';first=false;o<<"{\"id\":"<<p.mId<<",\"name\":"<<Quote(Utf8(name))<<'}';}o<<']';
        o<<",\"run\":";
        if(auto*r=mRunState){o<<"{\"mode\":"<<r->mMode<<",\"slot\":"<<Quote(std::to_string(r->mAdmission.run))<<",\"seed\":"<<r->mRunSeed<<",\"level_index\":"<<r->mLevelIndex
            <<",\"plant_picks\":"<<r->mPendingPlantPicks<<",\"buff_picks\":"<<r->mPendingBuffPicks<<",\"pool\":[";
            for(size_t i=0;i<r->mPool.size();++i){if(i)o<<',';o<<int(r->mPool[i]);}o<<"],\"buffs\":[";
            for(size_t i=0;i<r->mBuffs.size();++i){if(i)o<<',';auto&b=r->mBuffs[i];o<<"{\"id\":"<<b.mId<<",\"count\":"<<b.mCount<<'}';}o<<"]}";
        }else o<<"null";
        o<<",\"saves\":[";first=true;for(const auto&s:saves){if(!first)o<<',';first=false;o<<"{\"slot\":"<<Quote(std::to_string(s.id))<<",\"scene\":"<<s.scene<<",\"level\":"<<s.level<<",\"joint\":"<<s.joint<<'}';}o<<']';
        auto*pick=dynamic_cast<RunPickDialog*>(GetDialog(DIALOG_RUN_PICK));
        o<<",\"choice\":";
        if(pick&&mRunState){o<<"{\"kind\":"<<Quote(pick->mPlantPick?"plant":"buff")<<",\"choice_id\":"<<mRunState->mPickCounter<<",\"items\":[";
            for(int i=0;i<3;++i){if(i)o<<',';int id=pick->mPlantPick?int(mRunState->mPlantChoices[i]):mRunState->mBuffChoices[i];
                const char*name=pick->mPlantPick?(id>=0&&id<48?GetRunPlantName(SeedType(id)):nullptr):(id!=65535?GetRunChoiceDisplayName(id):nullptr);
                o<<"{\"index\":"<<i<<",\"id\":"<<id<<",\"name\":"<<Quote(name?name:"");
                if(name&&!pick->mPlantPick)o<<",\"description\":"<<Quote(GetRunChoiceDesc(id))
                    <<",\"rarity\":"<<GetRunChoiceRarity(id)<<",\"owned\":"<<mRunState->GetBuffCount(id)
                    <<",\"max_stacks\":"<<GetRunChoiceMaxStacks(id);
                o<<'}';}o<<"]}";
        }else o<<"null";
        o<<",\"chooser\":";
        if(auto*c=mSeedChooserScreen){o<<"{\"required\":"<<mBoard->mSeedBank->mNumPackets<<",\"selected\":"<<c->mSeedsInBank<<",\"interactive\":"<<c->mMouseVisible<<",\"y\":"<<c->mY<<",\"in_flight\":"<<c->mSeedsInFlight<<",\"available\":[";first=true;
            for(int i=0;i<48;++i)if(SeedTypeAvailable(SeedType(i))){if(!first)o<<',';first=false;
                o<<"{\"seed\":"<<i<<",\"name\":"<<Quote(GetRunPlantName(SeedType(i))?GetRunPlantName(SeedType(i)):"")<<",\"state\":"<<int(c->mChosenSeeds[i].mSeedState)<<'}';}o<<"]}";
        }else o<<"null";
        o<<",\"board\":";
        if(auto*b=mBoard){o<<"{\"board_id\":"<<boardId<<",\"level\":"<<b->mLevel<<",\"sun\":"<<b->mSunMoney<<",\"wave\":"<<b->mCurrentWave<<",\"waves\":"<<b->mNumWaves
            <<",\"intro_held\":"<<mRunIntroHeld<<",\"seeds_held\":"<<mOnlineSeedsHeld
            <<",\"intro_tick\":"<<(b->mCutScene?b->mCutScene->mCutsceneTime:0)
            <<",\"paused\":"<<b->mPaused<<",\"complete\":"<<b->mLevelComplete<<",\"rows\":"<<(b->StageHasPool()?6:5)<<",\"columns\":9,\"packets\":[";
            for(int i=0;i<b->mSeedBank->mNumPackets;++i){if(i)o<<',';auto&p=b->mSeedBank->mSeedPackets[i];
                o<<"{\"slot\":"<<i<<",\"seed\":"<<int(p.mPacketType)<<",\"cost\":"<<(p.mPacketType>=0&&p.mPacketType<NUM_SEED_TYPES?Plant::GetCost(p.mPacketType,p.mImitaterType):0)<<",\"active\":"<<p.mActive<<",\"refreshing\":"<<p.mRefreshing<<",\"refresh\":"<<p.mRefreshCounter<<",\"refresh_time\":"<<p.mRefreshTime<<'}';}o<<"],\"plants\":[";
            first=true;Plant*p=nullptr;while(b->IteratePlants(p)){if(p->mDead)continue;if(!first)o<<',';first=false;
                o<<"{\"id\":"<<b->mPlants.DataArrayGetID(p)<<",\"seed\":"<<int(p->mSeedType)<<",\"row\":"<<p->mRow<<",\"col\":"<<p->mPlantCol<<",\"health\":"<<p->mPlantHealth<<'}';}o<<"],\"zombies\":[";
            first=true;Zombie*z=nullptr;while(b->IterateZombies(z)){if(z->mDead)continue;if(!first)o<<',';first=false;
                o<<"{\"id\":"<<b->mZombies.DataArrayGetID(z)<<",\"type\":"<<int(z->mZombieType)<<",\"row\":"<<z->mRow<<",\"x\":"<<z->mPosX<<",\"y\":"<<z->mPosY<<",\"health\":"<<z->mBodyHealth<<",\"max_health\":"<<z->mBodyMaxHealth<<",\"helmet\":"<<z->mHelmHealth<<",\"shield\":"<<z->mShieldHealth<<",\"phase\":"<<int(z->mZombiePhase)<<'}';}o<<"],\"coins\":[";
            first=true;Coin*c=nullptr;while(b->IterateCoins(c)){if(!VisibleDrop(c))continue;if(!first)o<<',';first=false;
                o<<"{\"id\":"<<b->mCoins.DataArrayGetID(c)<<",\"type\":"<<int(c->mType)<<",\"x\":"<<c->mPosX<<",\"y\":"<<c->mPosY<<'}';}o<<"]}";
        }else o<<"null";
        o<<",\"network\":";
        if(auto*n=mOnlineSession){o<<"{\"active\":"<<n->IsActive()<<",\"connected\":"<<n->IsConnected()<<",\"state\":"<<int(n->GetState())<<",\"role\":"<<int(n->GetRole())
            <<",\"seat\":"<<int(n->GetLocalSeat())<<",\"room\":"<<Quote(n->GetRoomCode())<<",\"players\":"<<n->GetConnectedPlayerCount()
            <<",\"preparation_phase\":"<<int(n->GetPreparationPhase())
            <<",\"ready\":"<<n->IsSeatSeedsReady(n->GetLocalSeat())<<",\"peers_ready\":"<<n->IsPeerSeedsReady()
            <<",\"incoming_swap\":"<<n->HasIncomingSwapRequest()<<",\"outgoing_swap\":"<<n->IsSwapRequestPending()
            <<",\"status\":"<<Quote(Utf8(n->GetStatusText()))<<",\"seats\":[";
            bool firstSeat=true;
            for(unsigned char seat=1;seat<=NetProto::MAX_PLAYERS;++seat){
                if(!n->IsSeatOccupied(seat))continue;
                if(!firstSeat)o<<',';firstSeat=false;
                o<<"{\"seat\":"<<int(seat)<<",\"name\":"<<Quote(n->GetSeatName(seat))
                    <<",\"offline\":"<<n->IsSeatOffline(seat)<<",\"waiting\":"<<n->IsWaitingForNextRound(seat)<<'}';
            }
            o<<"]}";}else o<<"null";
        o<<",\"dialogs\":[";first=true;for(auto&[id,d]:mDialogMap){if(!first)o<<',';first=false;o<<"{\"id\":"<<id<<",\"title\":"<<Quote(Utf8(d->mDialogHeader))<<",\"text\":"<<Quote(Utf8(d->mDialogLines))<<'}';}o<<']';
        o<<",\"controls\":[";first=true;Controls(o,mWidgetManager,0,0,first);o<<"]}";return o.str();
    }
    void Execute(const std::string& line){
        std::istringstream in(line);std::string cmd;in>>cmd;
        auto num=[&](){int n;Require(bool(in>>n),"missing integer argument");return n;};
        if(cmd=="state")return;
        if(cmd=="team_vote"){
            const int choice=num();
            auto* panel=mTeamProgressionDialog;
            Require(panel&&!panel->Closed(),"no active team vote");
            for(const auto& control:panel->Controls()){
                if(control.kind!=TeamProgressionDialog::ControlKind::VOTE||!control.enabled||control.value!=choice)continue;
                const int x=control.rect.mX+control.rect.mWidth/2;
                const int y=control.rect.mY+control.rect.mHeight/2;
                panel->MouseDown(x,y,1);
                panel->MouseUp(x,y,1);
                Tick(1);return;
            }
            throw std::runtime_error("vote option unavailable");
        }
        if(cmd=="ready"){
            const int desired=num();
            Require((desired==0||desired==1)&&mOnlineSeedsHeld&&mBoard&&IsOnlineGame(),"no online ready prompt");
            if(mOnlineSession->IsSeatSeedsReady(mOnlineSession->GetLocalSeat())!=(desired!=0)){
                Require(desired==0||mOnlineSession->CanReadyPreparation(),"the team cannot confirm cards yet");
                ToggleOnlineSeedsReady();
                Tick(1);
            }
            return;
        }
        if(cmd=="swap"||cmd=="swap_answer"||cmd=="room_role"||cmd=="start_answer"){
            const int value=num();
            auto* session=mOnlineSession;
            Require(session&&session->IsConnected(),"not in an online room");
            if(cmd=="swap"){
                Require(value>=1&&value<=NetProto::MAX_PLAYERS,"invalid seat");
                Require(session->SwapSeats(static_cast<uint8_t>(value)),"seat swap request rejected");
            }else if(cmd=="swap_answer"){
                Require(value==0||value==1,"invalid answer");
                Require(session->HasIncomingSwapRequest(),"no incoming swap request");
                session->AnswerSwapRequest(value==1);
            }else if(cmd=="room_role"){
                Require(value==WatchRoom::Fight||value==WatchRoom::Watch,"invalid room role");
                Require(session->SetRoomRole(static_cast<uint8_t>(value)),"role change request rejected");
            }else{
                Require((value==0||value==1)&&mOnlineStartPromptActive,"no start invitation");
                OnlineStartPromptAnswer(value==1);
            }
            Tick(1);return;
        }
        if(cmd=="pause"){
            const int enable=num();
            Require((enable==0||enable==1)&&mBoard&&mGameScene==SCENE_PLAYING,"pause unavailable");
            if(enable){
                Require(!GetDialogCount(),"another dialog is open");
                DoNewOptions(false);
            }else{
                Require(GetDialog(DIALOG_NEWOPTIONS)!=nullptr,"pause menu is not open");
                KillNewOptionsDialog();
            }
            Tick(1);return;
        }
        if(cmd=="gift_sun"||cmd=="cob"){
            Require(mBoard&&!mBoard->mPaused&&!mBoard->IsWatchingView()&&
                mGameScene==SCENE_PLAYING&&!GetDialogCount()&&!TeamProgressionInputBlocked(),"no interactive battle");
            if(cmd=="gift_sun"){
                const int seat=num();
                Require(seat>=1&&seat<=NetProto::MAX_PLAYERS,"invalid seat");
                Require(mBoard->TrySendSunGift(static_cast<uint8_t>(seat)),"game rejected the sun gift");
            }else{
                unsigned int plantId;
                Require(bool(in>>plantId),"missing cannon ID");
                const int row=num(),col=num();
                Require(row>=0&&row<(mBoard->StageHasPool()?6:5)&&col>=0&&col<9,"invalid grid");
                auto* plant=mBoard->mPlants.DataArrayTryToGet(plantId);
                Require(plant&&!plant->mDead&&plant->mSeedType==SEED_COBCANNON&&
                    plant->mState==PlantState::STATE_COBCANNON_READY,"cannon unavailable");
                plant->CobCannonFire(mBoard->GridToPixelX(col,row)+35,mBoard->GridToPixelY(col,row)+40);
            }
            Tick(1);return;
        }
        if(cmd=="plant_buffs"){
            const int seed=num();
            Require(seed>=0&&seed<RunState::RUN_POOL_MAX,"unknown plant");
            std::ostringstream out;
            const char* name=GetRunPlantName(static_cast<SeedType>(seed));
            out<<"{\"plant\":"<<Quote(name?name:"")<<",\"buffs\":[";
            bool first=true;
            for(int id:GetRunPlantChoices(static_cast<SeedType>(seed))){
                if(!first)out<<',';first=false;
                out<<"{\"id\":"<<id<<",\"name\":"<<Quote(GetRunChoiceDisplayName(id))
                    <<",\"description\":"<<Quote(GetRunChoiceDesc(id))<<",\"rarity\":"<<GetRunChoiceRarity(id)
                    <<",\"owned\":"<<(mRunState?mRunState->GetBuffCount(id):0)<<",\"max_stacks\":"<<GetRunChoiceMaxStacks(id)<<'}';
            }
            out<<"]}";details=out.str();return;
        }
        if(cmd=="advance"){
            const int count=num(),events=num(),delay=num();
            Require(count>=0&&count<=10000&&events>=0&&events<=15&&delay>=0&&delay<=10,"invalid advance limits");
            const auto originalBoard=boardId;
            stopped=Quote("timeout");
            for(int i=0;i<=count;++i){
                if(boardId!=originalBoard){stopped=Quote("board_changed");break;}
                if((events&1)&&GetDialogCount()){stopped=Quote("dialog");break;}
                if(mBoard){
                    if((events&2)&&mBoard->mLevelComplete){stopped=Quote("complete");break;}
                    Coin* coin=nullptr;
                    if(events&4)while(mBoard->IterateCoins(coin))if(VisibleDrop(coin)){stopped=Quote("drop");break;}
                    if(stopped!=Quote("timeout"))break;
                    Zombie* z=nullptr;
                    if(events&8)while(mBoard->IterateZombies(z))if(!z->mDead&&!z->mMindControlled&&z->mPosX<280){stopped=Quote("danger");break;}
                    if(stopped!=Quote("timeout"))break;
                }
                if(i<count)Tick(1,delay);
            }
            return;
        }
        if(cmd=="step"){int n=num(),d=num();Require(n>=0&&n<=10000&&d>=0&&d<=10,"invalid step limits");Tick(n,d);return;}
        if(cmd=="new_run"){
            Require(!mRunState&&!mBoard,"return to menu before creating another run");
            const int mode=num(),diff=num(),scene=num(),scale=num(),tempo=num(),zombotany=num(),boss=num(),buffMult=num(),firstWave=num();
            Require(mode>=0&&mode<=3&&diff>=0&&diff<=2&&scene>=0&&scene<=4&&
                scale>=0&&scale<=3&&tempo>=0&&tempo<=2&&zombotany>=0&&zombotany<=1&&
                boss>=0&&boss<=6&&(boss&3)!=3&&buffMult>=1&&buffMult<=3&&firstWave>=0&&firstWave<=2,"invalid run options");
            Require(mode!=RunState::RUN_MODE_ENDLESS||!(boss&RunState::BOSS_FINALE),"finale unavailable in endless");
            StartRun(mode,diff,scale,tempo,zombotany,scene,boss,buffMult-1,0,1,0,firstWave);Tick(1);return;
        }
        if(cmd=="continue"){Require(!mRunState&&!mBoard,"return to menu before loading a run");int mode=num(),scene=num();uint64_t slot;Require(bool(in>>slot),"missing slot");Require(mode>=0&&mode<=3&&scene>=0&&scene<=4,"invalid continue options");
            if(mode==3&&!slot){saves=EndlessSaves::List(mPlayerInfo->mId,IsOnlineGame());for(const auto&s:saves)if(s.scene==scene){slot=s.id;break;}}
            ContinueRun(mode,scene,slot);Tick(1);Require(mRunState!=nullptr,"no compatible checkpoint for this profile/mode");return;}
        if(cmd=="saves"){saves=EndlessSaves::List(mPlayerInfo->mId,IsOnlineGame());return;}
        if(cmd=="choice"){auto*d=dynamic_cast<RunPickDialog*>(GetDialog(DIALOG_RUN_PICK));Require(d!=nullptr,"no active personal choice");int i=num();Require(i>=-1&&i<3,"choice index must be -1 or 0..2");
            if(i>=0)Require(d->mPlantPick?mRunState->mPlantChoices[i]!=SEED_NONE:mRunState->mBuffChoices[i]!=65535,"empty choice");
            d->ButtonDepress(i<0?RunPickDialog::RunPickDialog_Skip:RunPickDialog::RunPickDialog_Choice0+i);Tick(1);return;}
        if(cmd=="seed"){Require(mSeedChooserScreen!=nullptr&&GetDialogCount()==0,"seed chooser is not available");int s=num();Require(s>=0&&s<48&&SeedTypeAvailable(SeedType(s)),"seed unavailable");auto*c=mSeedChooserScreen;Require(c->mMouseVisible,"wait for chooser intro to finish");Require(c->mSeedsInFlight==0,"wait for seeds in flight");int before=c->mSeedsInBank;auto&p=c->mChosenSeeds[s];Click(c->mX+p.mX+20,c->mY+p.mY+25);Tick(1);Require(c->mSeedsInBank!=before,"seed input was blocked");return;}
        if(cmd=="start"){Require(mSeedChooserScreen&&GetDialogCount()==0,"no active seed chooser");auto*c=mSeedChooserScreen;Require(c->mSeedsInFlight==0&&c->mSeedsInBank==mBoard->mSeedBank->mNumPackets,"finish selecting all seed slots first");c->OnStartButton();Tick(1);return;}
        if(cmd=="plant"){Require(mBoard&&mGameScene==SCENE_PLAYING&&GetDialogCount()==0&&!mSeedChooserScreen,"not in an interactive battle");int slot=num(),row=num(),col=num();auto*b=mBoard;
            Require(slot>=0&&slot<b->mSeedBank->mNumPackets&&row>=0&&row<(b->StageHasPool()?6:5)&&col>=0&&col<9,"slot/grid out of range");
            auto&p=b->mSeedBank->mSeedPackets[slot];Require(p.mPacketType!=SEED_NONE&&p.CanPickUp(),"packet unavailable, insufficient sun or cooling down");
            Require(b->CanPlantAt(col,row,p.mPacketType)==PLANTING_OK,"planting rules reject this cell");
            const int used=p.mTimesUsed;
            Click(b->mSeedBank->mX+p.mX+20,b->mSeedBank->mY+p.mY+25);Click(b->GridToPixelX(col,row)+35,b->GridToPixelY(col,row)+40);Tick(1);
            Require(p.mTimesUsed>used,"plant input was blocked");return;}
        if(cmd=="collect"){
            Require(mBoard&&!mBoard->mPaused&&mGameScene==SCENE_PLAYING&&GetDialogCount()==0,"no interactive board");
            unsigned id;
            Require(bool(in>>id),"missing coin id");
            Coin* coin=mBoard->mCoins.DataArrayTryToGet(id);
            Require(VisibleDrop(coin),"drop unavailable");
            coin->MouseDown(static_cast<int>(coin->mPosX),static_cast<int>(coin->mPosY),1);
            Require(coin->mIsBeingCollected||coin->mDead,"collection refused by game");
            Tick(1);return;
        }
        if(cmd=="shovel"){
            const int row=num(),col=num();
            Require(mBoard&&mGameScene==SCENE_PLAYING&&!GetDialogCount()&&mBoard->mShowShovel&&!mBoard->mPaused,"shovel unavailable");
            Require(row>=0&&row<(mBoard->StageHasPool()?6:5)&&col>=0&&col<9,"invalid grid");
            const Rect tool=mBoard->GetShovelButtonRect();
            const auto before=mBoard->mPlants.mSize;
            Click(tool.mX+tool.mWidth/2,tool.mY+tool.mHeight/2);
            Click(mBoard->GridToPixelX(col,row)+35,mBoard->GridToPixelY(col,row)+40);
            Tick(1);
            Require(mBoard->mPlants.mSize<before,"shovel did not remove a plant");return;
        }
        if(cmd=="reroll"){
            const int lock=num();
            auto* dialog=dynamic_cast<RunPickDialog*>(GetDialog(DIALOG_RUN_PICK));
            Require(dialog&&mRunState,"no active personal choice");
            Require(RunPickPaidReroll(lock,mRunState->mPickCounter),"reroll refused");Tick(1);return;
        }
        if(cmd=="click"){int x=num(),y=num(),b=num();Require(x>=0&&x<800&&y>=0&&y<600&&(b==1||b==-1),"click out of bounds");Click(x,y,b);Tick(1);return;}
        if(cmd=="key"){int k=num();Require(k>0&&k<255,"invalid key");mWidgetManager->KeyDown(KeyCode(k));mWidgetManager->KeyUp(KeyCode(k));Tick(1);return;}
        if(cmd=="save"){Require(mPlayerInfo,"no player");if(mBoard)mBoard->TryToSaveGame();if(mRunState)Require(mRunState->Save(mPlayerInfo->mId),"run save failed");Require(mPlayerInfo->SaveDetailsChecked(),"profile save failed");mProfileMgr->Save();return;}
        if(cmd=="menu"){DoBackToMain();Tick(1);return;}
        if(cmd=="profile"){Require(!mBoard&&!mRunState,"switch profile at menu only");int id=num();PlayerInfo*found=nullptr;for(auto&[name,p]:mProfileMgr->GetProfileMap())if(p.mId==id)found=mProfileMgr->GetProfile(name);Require(found,"unknown profile id");mPlayerInfo=found;return;}
        if(cmd=="host"||cmd=="join"||cmd=="room_host"||cmd=="room_join"){
            Require(!mBoard&&!mRunState,"connect from menu only");std::string server,code;int port;
            if(cmd=="host")port=num();else{Require(bool(in>>server),"missing server");port=num();}
            Require(port>0&&port<65536,"invalid port");if(!mOnlineSession)mOnlineSession=new NetSession;
            auto*n=mOnlineSession;n->SetLocalName("CLI");bool ok=false;
            if(cmd=="host")ok=n->StartHost(uint16_t(port));
            if(cmd=="join")ok=n->StartJoin(server.c_str(),uint16_t(port));
            if(cmd=="room_host")ok=n->StartRoomHost(server.c_str(),uint16_t(port));
            if(cmd=="room_join"){Require(bool(in>>code),"missing room code");ok=n->StartRoomJoin(server.c_str(),code.c_str(),uint16_t(port));}
            Require(ok,"network start rejected");Tick(1);return;
        }
        if(cmd=="disconnect"){Require(!mBoard&&!mRunState,"return to menu before disconnecting");if(mOnlineSession)mOnlineSession->Close();Tick(1);return;}
        if(cmd=="screenshot"){
            fs::create_directories(fs::path(dataRoot)/"screenshots");auto path=fs::path(dataRoot)/"screenshots"/(std::to_string(ticks)+".png");
            MemoryImage image(this);image.Create(800,600);Graphics g(&image);mWidgetManager->DrawWidgetsTo(&g);
            ImageLib::Image png;png.mWidth=800;png.mHeight=600;png.mBits=new unsigned long[800*600];std::copy(image.GetBits(),image.GetBits()+800*600,png.mBits);ImageLib::WritePNGImage(path.string(),&png);return;
        }
        if(cmd=="quit"){mShutdown=true;return;}
        throw std::runtime_error("unknown engine command");
    }
    void Handle(const std::string& line){
        if(line=="pulse 2"){
            Tick(2);
            std::cout<<"{\"ok\":true,\"active\":"<<(mOnlineSession&&mOnlineSession->IsActive()?"true":"false")<<"}"<<std::endl;
            return;
        }
        replied.push_back(false);details="null";stopped="null";
        Audit("before-command");
        try{Execute(line);if(replied.size()==1)ProcessSafeDeleteList();
            if(!replied.back())std::cout<<"{\"ok\":true,\"state\":"<<State()<<",\"details\":"<<details<<",\"stopped\":"<<stopped<<"}"<<std::endl;
        }catch(const std::exception&e){if(!replied.back())std::cout<<"{\"ok\":false,\"error\":"<<Quote(e.what())<<",\"state\":"<<State()<<"}"<<std::endl;}
        replied.pop_back();
    }
    bool UpdateAppStep(bool* updated)override{
        if(!served||replied.empty())return LawnApp::UpdateAppStep(updated);
        // Legacy warning/store dialogs wait synchronously inside an input call.
        // Yield its response now, then service the agent's next JSON request in
        // the nested loop. A later click resolves the ORIGINAL production dialog.
        if(updated)*updated=false;
        if(!replied.back()){
            std::cout<<"{\"ok\":true,\"pending\":true,\"state\":"<<State()<<"}"<<std::endl;
            replied.back()=true;
        }
        std::string line;
        if(mShutdown||!std::getline(std::cin,line)){mShutdown=true;return false;}
        if(replied.size()>=16){std::cout<<"{\"ok\":false,\"error\":\"too many nested dialogs\"}"<<std::endl;return true;}
        Handle(line);return !mShutdown;
    }
    void UpdateFrames()override{
        if(served)return;
        LawnApp::UpdateFrames();if(!mLoadingThreadCompleted)return;
        served=true;mShowedStartupAnnounce=true;
        if(!mPlayerInfo){mPlayerInfo=mProfileMgr->AddProfile(_S("CLI"));Require(mPlayerInfo,"cannot create CLI profile");mPlayerInfo->SaveDetails();mProfileMgr->Save();}
        if(mTitleScreen)LoadingCompleted();
        ProcessSafeDeleteList();std::cout<<"{\"ok\":true,\"state\":"<<State()<<"}"<<std::endl;
        std::string line;
        while(!mShutdown&&std::getline(std::cin,line)){
            Handle(line);
        }
        mShutdown=true;
    }
};

int main(int argc,char**argv){
    if(argc!=2){std::cerr<<"Internal engine: use pvz-cli.py --help\n";return 2;}
    // The legacy framework requires HWND/DirectDraw. Its entire GUI lives on a
    // private, never activated desktop; the caller only sees stdin/stdout.
    auto name="PvZCLI-"+std::to_string(GetCurrentProcessId());
    HDESK desktop=CreateDesktopA(name.c_str(),nullptr,nullptr,0,GENERIC_ALL,nullptr);
    if(!desktop||!SetThreadDesktop(desktop)){std::cerr<<"Cannot create isolated desktop\n";return 3;}
    std::string key="Software\\PvZOnlineCLI\\session-"+std::to_string(GetCurrentProcessId());HKEY registry=nullptr;
    if(RegCreateKeyExA(HKEY_CURRENT_USER,key.c_str(),0,nullptr,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&registry,nullptr)!=ERROR_SUCCESS||RegOverridePredefKey(HKEY_CURRENT_USER,registry)!=ERROR_SUCCESS)return 3;
    TodStringListSetColors(gLawnStringFormats,gLawnStringFormatCount);
    gGetCurrentLevelName=LawnGetCurrentLevelName;gAppCloseRequest=LawnGetCloseRequest;gAppHasUsedCheatKeys=LawnHasUsedCheatKeys;gExtractResourcesByName=Sexy::ExtractResourcesByName;
    auto*app=new CliApp;gLawnApp=app;app->dataRoot=fs::absolute(argv[1]).string();fs::create_directories(fs::path(app->dataRoot)/"userdata");app->mChangeDirTo=".";
    SEHCatcher::mShowUI=false;SEHCatcher::mAllowSubmit=false;
    app->Init();app->Start();app->mShutdown=false;app->Shutdown();delete app;
    RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);RegCloseKey(registry);
    typedef LSTATUS (WINAPI *DeleteTree)(HKEY,LPCSTR);
    auto removeTree=reinterpret_cast<DeleteTree>(GetProcAddress(GetModuleHandleA("advapi32.dll"),"RegDeleteTreeA"));
    if(removeTree)removeTree(HKEY_CURRENT_USER,key.c_str());
    return 0;
}
