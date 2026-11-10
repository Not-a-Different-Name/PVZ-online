#pragma once
#include "../../LawnApp.h"
#include "../TeamPlay.h"
#include "../CoopPlay.h"
#include "../Board.h"
#include "../CursorObject.h"
#include "SeedChooserScreen.h"
#include "OnlineUiDraw.h"
#include "CoopHud.h"
#include "WatchRoomDialog.h"
#include "widget/WidgetManager.h"
#include <algorithm>
#include <set>
#include "graphics/MemoryImage.h"
#include <memory>

// Local presentation only. Every resource/loan/support action still enters its
// original transaction controller; this widget never grants a game effect.
class TeamQuickBar final:public Sexy::Widget {
public:
 enum Kind { NAME=1,REQUEST,PERMISSION,GIFT,CHERRY,WATCH,BORROW,PLANT,PREV,NEXT,CANCEL,RETURN,ACCEPT,DECLINE,CLOSE,DETAIL,ROSTER,SUPPORT_CARD,WATCH_ROOM };
 struct Button {Kind kind;Sexy::Rect box;std::string label;uint64_t target=0,stamp=0;int value=0;bool enabled=true;};
 LawnApp* app;TeamProgressionView::Controller* controller;Board* board;RunState* run;
 std::vector<Button> buttons;
 // Only the visible button slots are retained. Never cache arbitrary player text
 // globally: a renamed player or changed enabled/pressed state replaces its slot.
 struct ButtonImage {Button key{};bool down=false;std::unique_ptr<Sexy::MemoryImage> image;};
 std::vector<ButtonImage> buttonImages;
 TeamProgressionView::Snapshot snapshot;
 uint64_t selected=0;int plantPage=0,detailPage=0,quotedCost=-1;bool plants=false,details=false,captured=false,rosterOpen=false,supports=false;
 Button pressed{};TeamProgressionView::Context pressedContext{};
 explicit TeamQuickBar(LawnApp*a,TeamProgressionView::Controller*c):app(a),controller(c),board(a->mBoard),run(a->GetRunState()){Resize(0,0,800,600);mZOrder=1;}
 bool Valid()const{return app&&app->mBoard==board&&app->GetRunState()==run&&app->mTeamProgressionController==controller&&board&&run;}
 NetSession* Session()const{return RoomPresentation::Session(app);}
 CoopPlay* Coop()const{return app->mCoopPlay.get();}
 bool ReadySurface()const {
  auto*n=Session();return Valid()&&n&&n->IsConnected()&&app->IsRunMode()&&app->mActive&&!app->mMinimized&&
   !app->AdmissionBuilding()&&!CoopHud::Watching(board)&&!CoopHud::Held(board)&&!board->mPaused&&app->GetDialogCount()==0&&
   !app->mTeamProgressionDialog&&!run->HasPendingPick()&&!app->mDamageStats.mOpen&&
   !board->mChatPanelOpen&&board->mSunGiftSelectFrames<=0&&
   ((app->mGameScene==GameScenes::SCENE_PLAYING&&!app->mRunIntroHeld)||
    (app->mSeedChooserScreen&&app->mSeedChooserScreen->mVisible&&app->mSeedChooserScreen->mChooseState==CHOOSE_NORMAL));
 }
 bool Base()const {return ReadySurface();}
 bool Incoming()const{auto*c=Coop();return ReadySurface()&&c&&c->hasIncoming&&c->Preparing();}
 bool Surface()const{return Base()||Incoming();}
 static bool Same(const TeamProgressionView::Context&a,const TeamProgressionView::Context&b){return a.run==b.run&&a.player==b.player&&a.level==b.level&&a.attempt==b.attempt&&a.term==b.term&&a.revision==b.revision;}
 static bool SameButton(const Button&a,const Button&b){return a.kind==b.kind&&a.target==b.target&&a.stamp==b.stamp&&a.value==b.value&&a.box==b.box&&b.enabled;}
 TeamProgressionView::Snapshot ReadSnapshot()const{auto*c=dynamic_cast<TeamPlay*>(controller);return c?c->ReadQuick():controller->Read();}
 void Submit(TeamProgressionView::Action action,uint64_t target=0,int value=0,uint64_t quote=0){
  if(!Valid())return;TeamProgressionView::Intent i;i.action=action;i.context=ReadSnapshot().context;i.target=target;i.value=value;i.quoteId=quote;i.row=i.column=0;controller->Submit(i);
 }
 const TeamProgressionView::Peer* Peer(uint64_t player)const{auto it=std::find_if(snapshot.peers.begin(),snapshot.peers.end(),[&](const auto&p){return p.player==player;});return it==snapshot.peers.end()?nullptr:&*it;}
 void Add(Kind k,Sexy::Rect box,const std::string&label,bool enabled=true,uint64_t target=0,int value=0,uint64_t stamp=0){buttons.push_back({k,box,label,target,stamp,value,enabled});}
 bool CompactBattle()const{return app->mGameScene==GameScenes::SCENE_PLAYING&&!app->mRunIntroHeld&&!rosterOpen;}
 Sexy::Rect RosterBox()const{if(CompactBattle())return Sexy::Rect(0,90,40,58);if(!rosterOpen&&app->mSeedChooserScreen)return Sexy::Rect(528,138,180,44);return Sexy::Rect(4,88,180,44);}
 Sexy::Rect Popup()const{return Sexy::Rect(4,188,458,plants||details||supports?302:174);}
 bool LoanActive()const{return run->mLoanLevel==run->mLevelIndex&&run->mLoanPlant>=0;}
 std::vector<int> PlantList()const{std::vector<int> seeds;auto mask=TeamPlay::For(app).PlayerPlantMask(selected);for(int i=0;i<48;++i)if(mask&(uint64_t(1)<<i))seeds.push_back(i);return seeds;}
 void Refresh(){
  buttons.clear();if(!Valid())return;snapshot=ReadSnapshot();auto*n=Session();auto*c=Coop();
  if(!Base()||Incoming()){selected=0;rosterOpen=false;}
  if(selected){auto*p=Peer(selected);if(!p||!p->online||p->player==snapshot.context.player)selected=0;}
  if(Base()){
   int count=0;bool aid=false;for(const auto&p:snapshot.peers)if(p.seat!=n->GetLocalSeat()){++count;aid|=c&&c->requests.count(p.player);}
   Add(ROSTER,RosterBox(),ModText::Tr("队友 ","Team ")+std::to_string(count)+(aid?ModText::Tr(" 求援"," Aid"):"")+(rosterOpen?" -":" +"));
   if(rosterOpen&&n->SupportsWatchRoom())Add(WATCH_ROOM,Sexy::Rect(192,88,126,44),ModText::Tr("观众","Spectators"));
   int index=0;if(rosterOpen)for(const auto&p:snapshot.peers){if(p.seat==n->GetLocalSeat())continue;
    std::string label=p.name.empty()?"P"+std::to_string(p.seat):p.name;
    if(c&&c->requests.count(p.player))label=ModText::Tr("求阳光 ","Aid ")+label;
    else if(!p.online)label+=ModText::Tr(" 离线"," offline");
    Add(NAME,Sexy::Rect(4+index++*104,138,100,44),SeatUi::Fit(label,92,18),p.online&&p.player,p.player);
   }
   bool battle=app->mGameScene==GameScenes::SCENE_PLAYING&&!app->mRunIntroHeld;
   std::string request=c&&c->requestTicks>0?ModText::Tr("已求阳光","Aid requested"):c&&c->requestCooldown>0?ModText::Tr("求援 ","Aid ")+std::to_string((c->requestCooldown+99)/100)+"s":ModText::Tr("求阳光","Request sun");
   if(!battle||rosterOpen){
   Add(REQUEST,Sexy::Rect(528,88,100,44),request,battle&&c&&!c->requestTicks&&!c->requestCooldown&&n->HasOtherSeats());
   Add(PERMISSION,Sexy::Rect(636,88,106,44),run->mTeam.allow?ModText::Tr("接收支援 开","Accept aid ON"):ModText::Tr("接收支援 关","Accept aid OFF"),snapshot.canAct,snapshot.context.player,!run->mTeam.allow);
   }
   if(selected){
    auto*p=Peer(selected);const int y=188;
    Add(CLOSE,Sexy::Rect(412,y+6,42,32),"X");
    if(supports){
     for(size_t i=0;i<snapshot.supportOptions.size()&&i<4;++i){const auto&option=snapshot.supportOptions[i];
      std::string label=std::string(GetRunPlantName(SeedType(option.seed)))+" "+std::to_string(option.cost);
      Add(SUPPORT_CARD,Sexy::Rect(12+(i%2)*224,y+44+(i/2)*92,216,84),label,option.enabled,selected,option.seed,uint64_t(option.cost));
     }
    }else if(plants){
     auto seeds=PlantList();plantPage=std::clamp(plantPage,0,std::max(0,(int(seeds.size())-1)/8));
     const bool can=c&&c->Preparing()&&!c->loanWaiting&&c->pendingCancels.empty()&&!LoanActive()&&p&&p->online;
     for(int i=0;i<8&&plantPage*8+i<int(seeds.size());++i){int seed=seeds[plantPage*8+i];Add(PLANT,Sexy::Rect(12+(i%4)*110,y+45+(i/4)*92,102,84),GetRunPlantName((SeedType)seed),can,selected,seed);}
     Add(PREV,Sexy::Rect(12,y+244,84,42),ModText::Tr("上页","Prev"),plantPage>0);Add(NEXT,Sexy::Rect(366,y+244,84,42),ModText::Tr("下页","Next"),(plantPage+1)*8<int(seeds.size()));
    }else if(details){
     Add(PREV,Sexy::Rect(12,y+244,84,42),ModText::Tr("上页","Prev"),detailPage>0);Add(NEXT,Sexy::Rect(366,y+244,84,42),ModText::Tr("下页","Next"),(detailPage+1)*4<int(LoanLines().size()));
    }else{
     std::string giftLabel=c&&c->sunCooldown>0?ModText::Tr("阳光 ","Sun ")+std::to_string((c->sunCooldown+99)/100)+"s":run->mTeam.shared.stacks[TeamProgression::GROUP_SUN]?ModText::Tr("全队阳光100","Team sun 100"):ModText::Tr("送阳光 100","Give 100 sun");
     Add(GIFT,Sexy::Rect(12,y+44,140,44),giftLabel,battle&&p&&p->online&&CoopHud::GiftEnabled(board,p->seat),selected);
     std::string aidLabel=snapshot.support.cooldownSeconds>0?ModText::Tr("支援卡 ","Aid cards ")+std::to_string(snapshot.support.cooldownSeconds)+"s":ModText::Tr("支援卡","Aid cards");
     Add(CHERRY,Sexy::Rect(160,y+44,144,44),aidLabel,battle&&p&&p->online,selected);
     Add(WATCH,Sexy::Rect(312,y+44,140,44),ModText::Tr("观战","Watch"),p&&n->CanWatchSeat((uint8_t)p->seat),selected);
     if(c&&c->Preparing()){
      if(c->loanWaiting)Add(CANCEL,Sexy::Rect(12,y+96,212,42),ModText::Tr("取消借用申请","Cancel request"),true,c->pendingLoan.target,0,c->pendingLoan.serial);
      else if(LoanActive()){Add(RETURN,Sexy::Rect(12,y+96,212,42),ModText::Tr("归还借用植物","Return plant"),c->pendingCancels.empty());Add(DETAIL,Sexy::Rect(232,y+96,220,42),ModText::Tr("借用增益","Loan effects"));}
      else Add(BORROW,Sexy::Rect(12,y+96,440,42),ModText::Tr("向这位队友借植物","Borrow a plant"),p&&p->online&&c->pendingCancels.empty(),selected);
     }else if(LoanActive())Add(DETAIL,Sexy::Rect(12,y+96,440,42),ModText::Tr("查看借用植物与增益","Loan effects"));
    }
   }
  }
  if(Incoming()){auto&in=c->incoming;Add(ACCEPT,Sexy::Rect(522,146,132,42),ModText::Tr("同意借出","Accept"),true,in.sender,0,in.serial);Add(DECLINE,Sexy::Rect(662,146,130,42),ModText::Tr("拒绝","Decline"),true,in.sender,0,in.serial);}
 }
 void Quote(int seed=2){if(!selected||!Valid())return;Submit(TeamProgressionView::Action::SELECT_PEER,selected);Submit(TeamProgressionView::Action::SUPPORT_PREVIEW,selected,seed);Refresh();quotedCost=snapshot.support.cost;}
 std::vector<std::string> LoanLines()const{std::vector<std::string> lines;std::set<int> ids;for(const auto&b:run->mBuffs)if(b.mCount&&GetRunChoicePlant(b.mId)==(SeedType)run->mLoanPlant)ids.insert(b.mId);for(const auto&b:run->mLoanBuffs)if(b.mCount)ids.insert(b.mId);for(int id:ids)lines.push_back(std::string(GetRunChoiceDisplayName(id))+" "+std::to_string(run->GetEffectiveBuffCount(id))+ModText::Tr("层"," stack(s)"));if(LoanActive()&&run->mTeam.shared.stacks[TeamProgression::BORROW])lines.push_back(ModText::Tr("借兵增援：攻击+30%","Borrowed reinforcements: attack +30%"));return lines;}
 void RestoreFocus(){if(!app->mWidgetManager||app->mWidgetManager->mFocusWidget!=this)return;Sexy::Widget*next=app->mSeedChooserScreen?static_cast<Sexy::Widget*>(app->mSeedChooserScreen.get()):static_cast<Sexy::Widget*>(board);app->mWidgetManager->SetFocus(next);}
 void Collapse(){selected=0;plants=details=supports=false;captured=false;rosterOpen=false;RestoreFocus();MarkDirty();}
 void ClearOtherPanels(){board->CloseQuickChatPanel();board->mSunGiftSelectFrames=0;app->mDamageStats.mOpen=0;app->mDamageStats.mCaptured=false;app->mDamageStats.mPressed=-1;
#ifdef __ANDROID__
 board->mCoopToolsOpen=false;board->mTouchSeedDrag=false;board->mTouchSeedDragMoved=false;
#endif
 }
 void OpenFirst(){ClearOtherPanels();Refresh();rosterOpen=true;Refresh();for(const auto&p:snapshot.peers)if(p.player&&p.player!=snapshot.context.player&&p.online){selected=p.player;plants=details=supports=false;Quote();app->mWidgetManager->SetFocus(this);MarkDirty();return;}}
 void Update()override{
  Sexy::Widget::Update();Refresh();
  if(!captured&&selected&&!plants&&!details&&!supports&&snapshot.support.enabled&&(!snapshot.support.quoteId||snapshot.support.cost!=quotedCost))Quote();
  if(rosterOpen&&!captured&&app->mWidgetManager->IsLeftButtonDown()&&!IsPointVisible(app->mWidgetManager->mLastMouseX,app->mWidgetManager->mLastMouseY))Collapse();
  if(!app->mActive||app->mMinimized)captured=false;
  MarkDirty();
 }
 const Button* Hit(int x,int y)const{for(const auto&b:buttons)if(b.box.Contains(x,y))return &b;return nullptr;}
 bool IsPointVisible(int x,int y)override{
  Refresh();bool inside=Hit(x,y)||(Base()&&selected&&Popup().Contains(x,y))||(Incoming()&&Sexy::Rect(4,140,792,54).Contains(x,y));
  // MouseDown sets the button state before hit testing. Close here as well as
  // Update: a quick Android down/up pair can both arrive between two updates.
  if(!inside&&rosterOpen&&app->mWidgetManager&&app->mWidgetManager->IsLeftButtonDown())Collapse();
  return inside;
 }
 void MouseDown(int x,int y,int clicks)override{Refresh();captured=false;if(clicks<=0)return;if(auto*b=Hit(x,y);b&&b->enabled){pressed=*b;pressedContext=snapshot.context;captured=true;}if(board){board->RefreshSeedPacketFromCursor();board->ClearCursor();board->mIgnoreMouseUp=true;}}
 void MouseDrag(int x,int y)override{if(captured&&!pressed.box.Contains(x,y))captured=false;}
 void LostFocus()override{captured=false;Sexy::Widget::LostFocus();}
 void MouseLeave()override{captured=false;Sexy::Widget::MouseLeave();}
 void KeyDown(Sexy::KeyCode key)override{if(key==Sexy::KEYCODE_ESCAPE){Collapse();return;}if(board)board->KeyDown(key);}
 void MouseUp(int x,int y,int clicks)override{
  bool had=captured;Button action=pressed;auto context=pressedContext;captured=false;Refresh();auto*now=Hit(x,y);
  if(!had||clicks<=0||!now||!SameButton(action,*now)||!Same(context,snapshot.context))return;
  auto*c=Coop();auto*n=Session();using A=TeamProgressionView::Action;
  switch(action.kind){
   case WATCH_ROOM:Collapse();WatchRoomUi::Open(app);break;
   case ROSTER:if(rosterOpen)Collapse();else{ClearOtherPanels();rosterOpen=true;app->mWidgetManager->SetFocus(this);}break;
   case NAME:if(selected==action.target){Collapse();break;}ClearOtherPanels();selected=action.target;plantPage=detailPage=0;plants=details=supports=false;Quote();app->mWidgetManager->SetFocus(this);break;
   case CLOSE:if(plants||details||supports){plants=details=supports=false;Quote();}else Collapse();break;
   case REQUEST:if(c){c->RequestSun();board->PushCustomBanner(c->notice.c_str());}break;
   case PERMISSION:Submit(A::SUPPORT_PERMISSION,action.target,action.value);break;
   case GIFT:if(c){if(c->requests.count(action.target))c->Aid(action.target);else c->SendGift(action.target,0,true);board->PushCustomBanner(c->notice.c_str());}Collapse();break;
   case CHERRY:supports=true;plants=details=false;break;
   case SUPPORT_CARD:Quote(action.value);if(snapshot.support.canConfirm){Submit(A::SUPPORT_CONFIRM,action.target,action.value,snapshot.support.quoteId);Collapse();}break;
   case WATCH:if(auto*p=Peer(action.target);p&&n->CanWatchSeat((uint8_t)p->seat)&&n->BeginWatch((uint8_t)p->seat)){
#ifdef __ANDROID__
    board->mMobileWatchOpen=true;
#else
    board->mWatchKeyHeld=true;board->mWatchLastSeat=p->seat;board->mCoopWatchLatched=true;
#endif
    Collapse();}break;
   case BORROW:plants=true;details=false;plantPage=0;break;
   case PLANT:if(c)c->Ask(action.target,(SeedType)action.value);Collapse();break;
   case CANCEL:if(c)c->CancelLoanRequest();Collapse();break;
   case RETURN:if(c)c->ReturnLoan();Collapse();break;
   case ACCEPT:case DECLINE:if(c&&c->hasIncoming&&c->incoming.sender==action.target&&c->incoming.serial==action.stamp)c->Answer(action.kind==ACCEPT);break;
   case PREV:if(details)detailPage=std::max(0,detailPage-1);else plantPage=std::max(0,plantPage-1);break;
   case NEXT:if(details)++detailPage;else ++plantPage;break;
   case DETAIL:details=true;plants=false;detailPage=0;break;
  }
  Refresh();MarkDirty();
 }
 void Draw(Sexy::Graphics*g)override{
  Refresh();if(!Valid())return;auto*c=Coop();auto*n=Session();
  if(Base()&&selected){auto box=Popup();SeatUi::Strip(g,box);auto*p=Peer(selected);std::string title=p?p->name:"";
   if(supports)title+=ModText::Tr(" · 送支援卡"," · Give aid card");else if(plants)title+=ModText::Tr(" · 选植物借用"," · Choose plant");else if(details)title=ModText::Tr("借自 ","From ")+(c?c->mLoanLenderName:"");
   SeatUi::Text(g,SeatUi::Fit(title,384,19),14,214,382,19,TextColor(),true);
   if(supports){std::string line=snapshot.support.cooldownSeconds>0?ModText::Tr("共用冷却：","Shared cooldown: ")+std::to_string(snapshot.support.cooldownSeconds)+"s":ModText::Tr("本人付费 · 队友免费用 · 每人持有1张","You pay; teammate uses free; one held card");SeatUi::Text(g,SeatUi::Fit(line,430,16),14,455,430,16,TextColor());SeatUi::Text(g,ModText::Tr("需对应支援增益；未拥有植物则送白板","Aid buff required; unowned plants give base cards"),14,477,430,15,Sexy::Color(205,210,190));}
   else if(plants){auto list=PlantList();if(list.empty())SeatUi::Text(g,ModText::Tr("队友还没准备好","Teammate not ready"),14,310,430,18,TextColor(),true);SeatUi::Text(g,std::to_string(plantPage+1)+" / "+std::to_string(std::max(1,(int(list.size())+7)/8)),120,455,220,18,TextColor(),true);}
   else if(details){auto lines=LoanLines();int pages=std::max(1,(int(lines.size())+3)/4);detailPage=std::clamp(detailPage,0,pages-1);SeatUi::Text(g,run->mLoanPlant>=0?GetRunPlantName((SeedType)run->mLoanPlant):ModText::Tr("借用已结束","Loan ended"),14,257,430,21,TextColor(),true);for(int i=detailPage*4;i<int(lines.size())&&i<detailPage*4+4;++i)SeatUi::Text(g,SeatUi::Fit(lines[i],424,17),14,290+(i%4)*32,430,17,TextColor());if(lines.empty())SeatUi::Text(g,ModText::Tr("没有专属增益","No plant traits"),14,310,430,18,TextColor());}
   else if(!snapshot.support.canConfirm&&!snapshot.support.reason.empty())SeatUi::Text(g,SeatUi::Fit(snapshot.support.reason,430,16),14,347,430,16,Sexy::Color(210,215,190));
  }
  if(Incoming()){auto&in=c->incoming;SeatUi::Strip(g,Sexy::Rect(4,140,792,54));auto label=n->GetSeatName(in.src)+ModText::Tr(" 想借 "," wants ")+GetRunPlantName((SeedType)in.plant);SeatUi::Text(g,SeatUi::Fit(label,502,18),12,174,502,18,TextColor());}
  buttonImages.resize(buttons.size());
  for(size_t index=0;index<buttons.size();++index){const auto&b=buttons[index];bool down=captured&&SameButton(pressed,b);down=down||(b.kind==NAME&&b.target==selected);
   auto&cached=buttonImages[index];
   if(!cached.image||cached.key.kind!=b.kind||cached.key.label!=b.label||cached.key.value!=b.value||cached.key.enabled!=b.enabled||cached.key.box.mWidth!=b.box.mWidth||cached.key.box.mHeight!=b.box.mHeight||cached.down!=down){
    cached.key=b;cached.down=down;cached.image=std::make_unique<Sexy::MemoryImage>(app);cached.image->Create(b.box.mWidth,b.box.mHeight);
    Sexy::Graphics tile(cached.image.get());Sexy::Rect local(0,0,b.box.mWidth,b.box.mHeight);
    const bool compactRoster=b.kind==ROSTER&&b.box.mWidth==40;
    SeatUi::Button(&tile,local,(b.kind==PLANT||b.kind==SUPPORT_CARD||compactRoster)?"":b.label.c_str(),b.enabled,down);
    if(compactRoster){
     const std::string title=ModText::Tr("队友","Team");
     SeatUi::Text(&tile,title,2,22,36,title=="Team"?13:16,TextColor(),true);
     auto begin=b.label.find(' ');auto end=begin==std::string::npos?begin:b.label.find(' ',begin+1);
     std::string count=begin==std::string::npos?"":b.label.substr(begin+1,end-begin-1);
     if(b.label.find(ModText::Tr("求援","Aid"))!=std::string::npos)count+=" !";
     SeatUi::Text(&tile,count,2,46,36,16,TextColor(),true);
    }
    if(b.kind==SUPPORT_CARD){DrawSeedPacket(&tile,8,6,(SeedType)b.value,SEED_NONE,0,b.enabled?255:140,true,false);SeatUi::Text(&tile,SeatUi::Fit(GetRunPlantName((SeedType)b.value),134,17),74,33,134,17,TextColor(),true);auto gap=b.label.find_last_of(' ');std::string cost=gap==std::string::npos?b.label:b.label.substr(gap+1);SeatUi::Text(&tile,ModText::Tr("阳光 ","Sun ")+cost,74,62,134,17,TextColor(),true);}
    else if(b.kind==PLANT){DrawSeedPacket(&tile,27.0f,2.0f,(SeedType)b.value,SEED_NONE,0,b.enabled?255:140,true,false);SeatUi::Text(&tile,SeatUi::Fit(b.label,94,15),4,79,94,15,TextColor(),true);}
   }
   g->DrawImage(cached.image.get(),b.box.mX,b.box.mY);
  }
 }
 static Sexy::Color TextColor(){return Sexy::Color(245,230,165);}
};
