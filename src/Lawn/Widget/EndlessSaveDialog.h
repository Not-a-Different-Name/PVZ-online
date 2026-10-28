#pragma once
#include "LawnDialog.h"
#include "../../LawnApp.h"
#include "../Run/EndlessSaves.h"
#include "../System/PlayerInfo.h"
#include "OnlineUiDraw.h"
#include <ctime>

// The same hit rectangles and CJK text path are used by both clients.
class EndlessSaveDialog : public LawnDialog {
    LawnApp* app;
    bool listing=false;
    int page=0,down=-1;
    std::vector<EndlessSaves::Record> saves;
    bool migrated=true;
    static Rect Row(int i){return Rect(56,134+i*66,688,58);}
    void ContinueButton(Graphics* g,Rect box)const {
#ifdef __ANDROID__
        if(ModText::IsChinese()) {
            const bool enabled=!saves.empty();SeatUi::Button(g,box,"",enabled,down==11);
            auto* font=ModText::FontFor(FONT_DWARVENTODCRAFT18,28);const std::string label="继续存档";
            Graphics text(*g);text.ClipRect(box.mX+10,box.mY,box.mWidth-20,box.mHeight);text.SetFont(font);
            const int x=box.mX+(box.mWidth-font->StringWidth(label))/2,y=box.mY+(box.mHeight+font->GetAscent())/2-3;
            text.SetColor(Color(16,44,16));for(const auto& delta:std::vector<Point>{{1,0},{-1,0},{0,1},{0,-1}})text.DrawString(label,x+delta.mX,y+delta.mY);
            text.SetColor(!enabled?Color(145,145,145):down==11?Color(155,240,96):Color(0,196,0));text.DrawString(label,x,y);return;
        }
#endif
        SeatUi::Button(g,box,ModText::Tr("继续存档","Continue saved run"),!saves.empty(),down==11);
    }
    int Hit(int x,int y)const {
        if(Rect(56,530,150,46).Contains(x,y))return 0;
        if(!listing){if(Rect(210,226,380,66).Contains(x,y))return 10;if(!saves.empty()&&Rect(210,324,380,66).Contains(x,y))return 11;return -1;}
        if(page>0&&Rect(270,530,120,46).Contains(x,y))return 20;
        if((page+1)*5<int(saves.size())&&Rect(410,530,120,46).Contains(x,y))return 21;
        for(int i=0;i<5&&page*5+i<int(saves.size());++i)if(Row(i).Contains(x,y))return 100+page*5+i;
        return -1;
    }
public:
    uint64_t selected=0;int scene=0;
    explicit EndlessSaveDialog(LawnApp* a,bool showList=false):LawnDialog(a,DIALOG_ONLINE_START,true,"","","",Dialog::BUTTONS_NONE),app(a),listing(showList) {
        Resize(0,0,800,600);saves=EndlessSaves::List(a->mPlayerInfo->mId,a->IsOnlineGame(),&migrated);
    }
    void Draw(Graphics* g)override {
        SeatUi::Panel(g,Rect(0,0,800,600));SeatUi::Text(g,listing?ModText::Tr("继续存档","Continue saved run"):ModText::Tr("无尽","Endless"),56,86,688,30,Color(245,230,165),true);
        if(!listing){SeatUi::Button(g,Rect(210,226,380,66),ModText::Tr("开始新无尽","Start new endless run"),true,down==10);ContinueButton(g,Rect(210,324,380,66));SeatUi::Text(g,ModText::Tr("新开一局也会保留已有存档。","Starting a new run keeps your saved runs."),56,440,688,21,Color(220,225,210),true);}
        else {
            static const char* zh[]={"白天","夜晚","泳池","浓雾","屋顶"};static const char* en[]={"Day","Night","Pool","Fog","Roof"};
            for(int i=0;i<5&&page*5+i<int(saves.size());++i){const auto& s=saves[page*5+i];auto box=Row(i);SeatUi::Strip(g,box);
                auto label=std::string(ModText::Tr(zh[s.scene],en[s.scene]))+ModText::Tr(" · 无尽 第 "," · Endless Level ")+std::to_string(s.level+1)+ModText::Tr(" 关","");
                SeatUi::Text(g,label,box.mX+12,box.mY+24,664,22,Color(245,230,165));
                std::time_t stamp=s.modified;std::tm local{};
#ifdef _WIN32
                localtime_s(&local,&stamp);
#else
                localtime_r(&stamp,&local);
#endif
                char date[32]{};std::strftime(date,sizeof(date),"%Y-%m-%d %H:%M",&local);SeatUi::Text(g,date,box.mX+12,box.mY+47,664,18,Color(210,220,205));}
            if(saves.empty())SeatUi::Text(g,ModText::Tr("暂无可继续的存档。","No saved runs available."),56,280,688,24,Color(220,225,210),true);
            SeatUi::Text(g,ModText::Tr("第 ","Page ")+std::to_string(page+1)+" / "+std::to_string(std::max(1,(int(saves.size())+4)/5)),56,496,688,18,Color(210,220,205),true);
            SeatUi::Button(g,Rect(270,530,120,46),ModText::Tr("上页","Prev"),page>0,down==20);SeatUi::Button(g,Rect(410,530,120,46),ModText::Tr("下页","Next"),(page+1)*5<int(saves.size()),down==21);
        }
        if(!migrated)SeatUi::Text(g,ModText::Tr("存档保留失败，请检查空间后重试。","Could not retain a save. Check storage and retry."),56,468,688,18,Color(245,190,130),true);
        SeatUi::Button(g,Rect(56,530,150,46),ModText::Tr("返回","Back"),true,down==0);
    }
    void MouseDown(int x,int y,int n)override {down=n>0?Hit(x,y):-1;MarkDirty();}
    void MouseUp(int x,int y,int n)override {int h=Hit(x,y),was=down;down=-1;MarkDirty();if(n<=0||h<0||was!=h)return;
        if(h==0){mResult=Dialog::ID_NO;return;}if(h==10){mResult=10;return;}if(h==11){listing=true;page=0;return;}if(h==20){--page;return;}if(h==21){++page;return;}
        if(h>=100&&h-100<int(saves.size())){selected=saves[h-100].id;scene=saves[h-100].scene;mResult=Dialog::ID_YES;}
    }
    void KeyDown(KeyCode key)override {if(key==KEYCODE_ESCAPE)mResult=Dialog::ID_NO;}
};
