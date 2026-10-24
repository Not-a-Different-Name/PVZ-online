#include "widget/WidgetManager.h"
// Called before widget/global shortcut dispatch. The fence retains held releases
// after exit, including a release delivered into a different scene.
bool LawnApp::FilterObserverInput(int type,int code,int x,int y){
 bool watching=mBoard&&mOnlineSession&&mOnlineSession->IsWatching()&&
mBoard->mMobileWatchOpen;
 if(watching&&!mObserverInput.Active()){
  std::bitset<256> keys;std::bitset<64> pointers;
  for(int k=0;k<255;++k)if(mWidgetManager->mKeyDown[k])keys.set(k);
  for(int p=0;p<3;++p)if(mWidgetManager->mDownButtons&(1<<p))pointers.set(p);
  mObserverInput.Open(keys,pointers);mObserverTextTail=true;
  mWidgetManager->mDownButtons=0;mWidgetManager->mLastDownWidget=nullptr;
 }
 if(!watching&&mObserverInput.Active())mObserverInput.Close();
 if(type<0||type>7)return false;
 const Observer::Input kinds[]={Observer::Input::KeyDown,Observer::Input::KeyUp,Observer::Input::Character,Observer::Input::PointerDown,Observer::Input::PointerUp,Observer::Input::PointerMove,Observer::Input::Wheel,Observer::Input::Cancel};
 const int slot=(type>=3&&type<=5)?(code<0?1:code==3?2:0):code;
 bool consumed=mObserverInput.Consume(kinds[type],slot);
 if(type==2)return consumed||mObserverTextTail;
 if(type==0&&!consumed)mObserverTextTail=false;
 if(!consumed)return false;
 if(type==0||type==1){if(code>=0&&code<255)mWidgetManager->mKeyDown[code]=(type==0);}
 if(!watching)return true;
 if(type==0){
  if(code==27||code=='V')mBoard->StopMobileWatch();else if(code==39)mBoard->CycleMobileWatch();

 }else if(type==1&&code=='V'){

 }else if(type==3)mBoard->CoopMouseDown(x-mBoard->mX,y-mBoard->mY,code);
 else if(type==4)mBoard->CoopMouseUp(x-mBoard->mX,y-mBoard->mY,code);
 return true;
}
