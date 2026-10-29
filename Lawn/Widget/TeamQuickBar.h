#pragma once
#include "../../LawnApp.h"
#include "../TeamPlay.h"
#include "../CoopPlay.h"
#include "../Board.h"
#include "../CursorObject.h"
#include "SeedChooserScreen.h"
#include "OnlineUiDraw.h"
#include "CoopHud.h"
#include "widget/WidgetManager.h"
#include <algorithm>
#include <set>

// Local presentation only. Every resource/loan/support action still enters its
// original transaction controller; this widget never grants a game effect.
class TeamQuickBar final:public Sexy::Widget {
public:
 enum Kind { NAME=1,REQUEST,PERMISSION,GIFT,CHERRY,WATCH,BORROW,PLANT,PREV,NEXT,CANCEL,RETURN,ACCEPT,DECLINE,CLOSE,DETAIL };
 struct Button {Kind kind;Sexy::Rect box;std::string label;uint64_t target=0,stamp=0;int value=0;bool enabled=true;};
 LawnApp* app;TeamProgressionView::Controller* controller;Board* board;RunState* run;
 std::vector<Button> buttons;
 TeamProgressionView::Snapshot snapshot;
 uint64_t selected=0;int plantPage=0,detailPage=0,quotedCost=-1;bool plants=false,details=false,captured=false;
 Button pressed{};TeamProgressionView::Context pressedContext{};
 explicit TeamQuickBar(LawnApp*a,TeamProgressionView::Controller*c):app(a),controller(c),board(a->mBoard),run(a->GetRunState()){Resize(0,0,800,600);mZOrder=1;}
 bool Valid()const{return app&&app->mBoard==board&&app->GetRunState()==run&&app->mTeamProgressionController==controller&&board&&run;}
 NetSession* Session()const{return RoomPresentation::Session(app);}
 CoopPlay* Coop()const{return app->mCoopPlay.get();}
 bool Base()const {
  auto*n=Session();return Valid()&&n&&n->IsConnected()&&app->IsRunMode()&&app->mActive&&!app->mMinimized&&
   !app->AdmissionBuilding()&&!CoopHud::Watching(board)&&!CoopHud::Held(board)&&!board->mPaused&&app->GetDialogCount()==0&&
   !app->mTeamProgressionDialog&&!run->HasPendingPick()&&
   (app->mGameScene==GameScenes::SCENE_PLAYING||(app->mSeedChooserScreen&&app->mSeedChooserScreen->mVisible));
 }
 bool Incoming()const{auto*c=Coop();return Valid()&&c&&c->hasIncoming&&c->Preparing()&&app->mActive&&!app->mMinimized&&Session()&&Session()->IsConnected();}
 bool Surface()const{return Base()||Incoming();}
 static bool Same(const TeamProgressionView::Context&a,const TeamProgressionView::Context&b){return a.run==b.run&&a.player==b.player&&a.level==b.level&&a.attempt==b.attempt&&a.term==b.term&&a.revision==b.revision;}
 static bool SameButton(const Button&a,const Button&b){return a.kind==b.kind&&a.target==b.target&&a.stamp==b.stamp&&a.value==b.value&&a.box==b.box&&b.enabled;}
 TeamProgressionView::Snapshot ReadSnapshot()const{auto*c=dynamic_cast<TeamPlay*>(controller);return c?c->ReadQuick():controller->Read();}
 void Submit(TeamProgressionView::Action action,uint64_t target=0,int value=0,uint64_t quote=0){
  if(!Valid())return;TeamProgressionView::Intent i;i.action=action;i.context=ReadSnapshot().context;i.target=target;i.value=value;i.quoteId=quote;i.row=i.column=0;controller->Submit(i);
 }
 const TeamProgressionView::Peer* Peer(uint64_t player)const{auto it=std::find_if(snapshot.peers.begin(),snapshot.peers.end(),[&](const auto&p){return p.player==player;});return it==snapshot.peers.end()?nullptr:&*it;}
 void Add(Kind k,Sexy::Rect box,const std::string&label,bool enabled=true,uint64_t target=0,int value=0,uint64_t stamp=0){buttons.push_back({k,box,label,target,stamp,value,enabled});}
 Sexy::Rect Popup()const{return Sexy::Rect(4,128,458,plants||details?302:174);}
 bool LoanActive()const{return run->mLoanLevel==run->mLevelIndex&&run->mLoanPlant>=0;}
 std::vector<int> PlantList()const{std::vector<int> seeds;auto mask=TeamPlay::For(app).PlayerPlantMask(selected);for(int i=0;i<48;++i)if(mask&(uint64_t(1)<<i))seeds.push_back(i);return seeds;}
 void Refresh(){
  buttons.clear();if(!Valid())return;snapshot=ReadSnapshot();auto*n=Session();auto*c=Coop();
  if(!Base()||Incoming())selected=0;
  if(selected){auto*p=Peer(selected);if(!p||!p->online||p->player==snapshot.context.player)selected=0;}
  if(Base()){
   int index=0;for(const auto&p:snapshot.peers){if(p.seat==n->GetLocalSeat())continue;
    std::string label=p.name.empty()?"P"+std::to_string(p.seat):p.name;
    if(c&&c->requests.count(p.player))label=ModText::Tr("求阳光 ","Aid ")+label;
    else if(!p.online)label+=ModText::Tr(" 离线"," offline");
    Add(NAME,Sexy::Rect(4+index++*104,88,100,34),SeatUi::Fit(label,88,17),p.online&&p.player,p.player);
   }
   bool battle=app->mGameScene==GameScenes::SCENE_PLAYING&&!app->mRunIntroHeld;
   std::string request=c&&c->requestTicks>0?ModText::Tr("已求阳光","Aid requested"):c&&c->requestCooldown>0?ModText::Tr("求援 ","Aid ")+std::to_string((c->requestCooldown+99)/100)+"s":ModText::Tr("求阳光","Request sun");
   Add(REQUEST,Sexy::Rect(528,88,100,34),request,battle&&c&&!c->requestTicks&&!c->requestCooldown&&n->HasOtherSeats());
   Add(PERMISSION,Sexy::Rect(636,88,106,34),run->mTeam.allow?ModText::Tr("接收樱桃 开","Accept aid ON"):ModText::Tr("接收樱桃 关","Accept aid OFF"),snapshot.canAct,snapshot.context.player,!run->mTeam.allow);
   if(selected){
    auto*p=Peer(selected);const int y=128;
    Add(CLOSE,Sexy::Rect(412,y+6,42,32),"X");
    if(plants){
     auto seeds=PlantList();plantPage=std::clamp(plantPage,0,std::max(0,(int(seeds.size())-1)/8));
     const bool can=c&&c->Preparing()&&!c->loanWaiting&&c->pendingCancels.empty()&&!LoanActive()&&p&&p->online;
     for(int i=0;i<8&&plantPage*8+i<int(seeds.size());++i){int seed=seeds[plantPage*8+i];Add(PLANT,Sexy::Rect(12+(i%4)*110,y+45+(i/4)*92,102,84),GetRunPlantName((SeedType)seed),can,selected,seed);}
     Add(PREV,Sexy::Rect(12,y+244,84,42),ModText::Tr("上页","Prev"),plantPage>0);Add(NEXT,Sexy::Rect(366,y+244,84,42),ModText::Tr("下页","Next"),(plantPage+1)*8<int(seeds.size()));
    }else if(details){
     Add(PREV,Sexy::Rect(12,y+244,84,42),ModText::Tr("上页","Prev"),detailPage>0);Add(NEXT,Sexy::Rect(366,y+244,84,42),ModText::Tr("下页","Next"),(detailPage+1)*4<int(LoanLines().size()));
    }else{
     Add(GIFT,Sexy::Rect(12,y+44,140,44),ModText::Tr("送阳光 100","Give 100 sun"),battle&&p&&p->online&&CoopHud::GiftEnabled(board,p->seat),selected);
     std::string cherry=snapshot.support.cooldownSeconds>0?ModText::Tr("樱桃 ","Cherry ")+std::to_string(snapshot.support.cooldownSeconds)+"s":ModText::Tr("送樱桃 ","Cherry ")+std::to_string(snapshot.support.cost);
     Add(CHERRY,Sexy::Rect(160,y+44,144,44),cherry,snapshot.support.canConfirm&&!snapshot.support.pending,selected,0,snapshot.support.quoteId);
     Add(WATCH,Sexy::Rect(312,y+44,140,44),ModText::Tr("观战","Watch"),p&&n->CanWatchSeat((uint8_t)p->seat),selected);
     if(c&&c->Preparing()){
      if(c->loanWaiting)Add(CANCEL,Sexy::Rect(12,y+96,212,42),ModText::Tr("取消借用申请","Cancel request"),true,c->pendingLoan.target,0,c->pendingLoan.serial);
      else if(LoanActive()){Add(RETURN,Sexy::Rect(12,y+96,212,42),ModText::Tr("归还借用植物","Return plant"),c->pendingCancels.empty());Add(DETAIL,Sexy::Rect(232,y+96,220,42),ModText::Tr("借用增益","Loan effects"));}
      else Add(BORROW,Sexy::Rect(12,y+96,440,42),ModText::Tr("向这位队友借植物","Borrow a plant"),p&&p->online&&c->pendingCancels.empty(),selected);
     }else if(LoanActive())Add(DETAIL,Sexy::Rect(12,y+96,440,42),ModText::Tr("查看借用植物与增益","Loan effects"));
    }
   }
  }
  if(Incoming()){auto&in=c->incoming;Add(ACCEPT,Sexy::Rect(522,136,132,42),ModText::Tr("同意借出","Accept"),true,in.sender,0,in.serial);Add(DECLINE,Sexy::Rect(662,136,130,42),ModText::Tr("拒绝","Decline"),true,in.sender,0,in.serial);}
 }
 void Quote(){if(!selected||!Valid())return;Submit(TeamProgressionView::Action::SELECT_PEER,selected);Submit(TeamProgressionView::Action::SUPPORT_PREVIEW,selected);Refresh();quotedCost=snapshot.support.cost;}
 std::vector<std::string> LoanLines()const{std::vector<std::string> lines;std::set<int> ids;for(const auto&b:run->mBuffs)if(b.mCount&&GetRunChoicePlant(b.mId)==(SeedType)run->mLoanPlant)ids.insert(b.mId);for(const auto&b:run->mLoanBuffs)if(b.mCount)ids.insert(b.mId);for(int id:ids)lines.push_back(std::string(GetRunChoiceDisplayName(id))+" "+std::to_string(run->GetEffectiveBuffCount(id))+ModText::Tr("层"," stack(s)"));return lines;}
 void RestoreFocus(){if(!app->mWidgetManager||app->mWidgetManager->mFocusWidget!=this)return;Sexy::Widget*next=app->mSeedChooserScreen?static_cast<Sexy::Widget*>(app->mSeedChooserScreen):static_cast<Sexy::Widget*>(board);app->mWidgetManager->SetFocus(next);}
 void Collapse(){selected=0;plants=details=false;captured=false;RestoreFocus();MarkDirty();}
 void ClearOtherPanels(){board->CloseQuickChatPanel();board->mSunGiftSelectFrames=0;app->mDamageStats.mOpen=0;app->mDamageStats.mCaptured=false;app->mDamageStats.mPressed=-1;
#ifdef __ANDROID__
 board->mCoopToolsOpen=false;board->mTouchSeedDrag=false;board->mTouchSeedDragMoved=false;
#endif
 }
 void OpenFirst(){ClearOtherPanels();Refresh();for(const auto&p:snapshot.peers)if(p.player&&p.player!=snapshot.context.player&&p.online){selected=p.player;plants=details=false;Quote();app->mWidgetManager->SetFocus(this);MarkDirty();return;}}
 void Update()override{
  Sexy::Widget::Update();Refresh();
  if(!captured&&selected&&!plants&&!details&&snapshot.support.enabled&&(!snapshot.support.quoteId||snapshot.support.cost!=quotedCost))Quote();
  if(selected&&!captured&&app->mWidgetManager->IsLeftButtonDown()&&!IsPointVisible(app->mWidgetManager->mLastMouseX,app->mWidgetManager->mLastMouseY))Collapse();
  if(!app->mActive||app->mMinimized)captured=false;
  MarkDirty();
 }
 const Button* Hit(int x,int y)const{for(const auto&b:buttons)if(b.box.Contains(x,y))return &b;return nullptr;}
 bool IsPointVisible(int x,int y)override{
  Refresh();bool inside=Hit(x,y)||(Base()&&selected&&Popup().Contains(x,y))||(Incoming()&&Sexy::Rect(4,130,792,54).Contains(x,y));
  // MouseDown sets the button state before hit testing. Close here as well as
  // Update: a quick Android down/up pair can both arrive between two updates.
  if(!inside&&selected&&app->mWidgetManager&&app->mWidgetManager->IsLeftButtonDown())Collapse();
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
   case NAME:if(selected==action.target){Collapse();break;}ClearOtherPanels();selected=action.target;plantPage=detailPage=0;plants=details=false;Quote();app->mWidgetManager->SetFocus(this);break;
   case CLOSE:if(plants||details){plants=details=false;Quote();}else Collapse();break;
   case REQUEST:if(c){c->RequestSun();board->PushCustomBanner(c->notice.c_str());}break;
   case PERMISSION:Submit(A::SUPPORT_PERMISSION,action.target,action.value);break;
   case GIFT:if(c){if(c->requests.count(action.target))c->Aid(action.target);else c->SendGift(action.target,0,true);board->PushCustomBanner(c->notice.c_str());}Collapse();break;
   case CHERRY:Submit(A::SUPPORT_CONFIRM,action.target,0,action.stamp);Collapse();break;
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
   if(plants)title+=ModText::Tr(" · 选植物借用"," · Choose plant");else if(details)title=ModText::Tr("借自 ","From ")+(c?c->mLoanLenderName:"");
   SeatUi::Text(g,SeatUi::Fit(title,384,19),14,154,382,19,TextColor(),true);
   if(plants){auto list=PlantList();if(list.empty())SeatUi::Text(g,ModText::Tr("队友还没准备好","Teammate not ready"),14,240,430,18,TextColor(),true);SeatUi::Text(g,std::to_string(plantPage+1)+" / "+std::to_string(std::max(1,(int(list.size())+7)/8)),120,395,220,18,TextColor(),true);}
   else if(details){auto lines=LoanLines();int pages=std::max(1,(int(lines.size())+3)/4);detailPage=std::clamp(detailPage,0,pages-1);SeatUi::Text(g,run->mLoanPlant>=0?GetRunPlantName((SeedType)run->mLoanPlant):ModText::Tr("借用已结束","Loan ended"),14,197,430,21,TextColor(),true);for(int i=detailPage*4;i<int(lines.size())&&i<detailPage*4+4;++i)SeatUi::Text(g,SeatUi::Fit(lines[i],424,17),14,230+(i%4)*32,430,17,TextColor());if(lines.empty())SeatUi::Text(g,ModText::Tr("没有专属增益","No plant traits"),14,250,430,18,TextColor());}
   else if(!snapshot.support.canConfirm&&!snapshot.support.reason.empty())SeatUi::Text(g,SeatUi::Fit(snapshot.support.reason,430,16),14,287,430,16,Sexy::Color(210,215,190));
  }
  if(Incoming()){auto&in=c->incoming;SeatUi::Strip(g,Sexy::Rect(4,130,792,54));auto label=n->GetSeatName(in.src)+ModText::Tr(" 想借 "," wants ")+GetRunPlantName((SeedType)in.plant);SeatUi::Text(g,SeatUi::Fit(label,502,18),12,164,502,18,TextColor());}
  for(const auto&b:buttons){bool down=captured&&SameButton(pressed,b);SeatUi::Button(g,b.box,"",b.enabled,down||b.kind==NAME&&b.target==selected);
   if(b.kind==PLANT){DrawSeedPacket(g,float(b.box.mX+27),float(b.box.mY+2),(SeedType)b.value,SEED_NONE,0,b.enabled?255:140,true,false);SeatUi::Text(g,SeatUi::Fit(b.label,94,15),b.box.mX+4,b.box.mY+79,94,15,TextColor(),true);}
   else SeatUi::Text(g,SeatUi::Fit(b.label,b.box.mWidth-10,17),b.box.mX+5,b.box.mY+(b.box.mHeight+17)/2-2,b.box.mWidth-10,17,b.enabled?TextColor():Sexy::Color(140,140,140),true);
  }
 }
 static Sexy::Color TextColor(){return Sexy::Color(245,230,165);}
};
