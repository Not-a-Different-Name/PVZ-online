#pragma once
#include "OnlineUiDraw.h"
#include "../Run/TeamGoalsStorage.h"

class GrowthCollectionView {
    LawnApp*app;
    TeamGoals::Ledger records;
    std::vector<TeamProgressionView::Goal> goals;
    int selected=0,pressed=-1;
    bool valid=false;
    std::string status;
    Sexy::Rect Row(int i)const{return Sexy::Rect(32,100+58*i,736,53);}
    int Hit(int x,int y)const{for(int i=0;i<7;++i)if(Row(i).Contains(x,y))return i;return -1;}
public:
    explicit GrowthCollectionView(LawnApp*owner):app(owner){Reload();}
    void Reload(){valid=TeamGoalsStorage::Load(app,records);selected=valid?TeamGoalsStorage::Title(app,records):0;goals=TeamGoalsStorage::View(records.Total());status=valid?ModText::Tr("点击已解锁称号展示，再点卸下；收藏永久保留。", "Choose an unlocked title to wear it; choose it again to remove it. Keep collections forever."):ModText::Tr("收藏记录无法读取，已保留原文件。","Collection records could not be read; the original file is retained.");pressed=-1;}
    void Down(int x,int y){pressed=Hit(x,y);}
    void Up(int x,int y){const int i=Hit(x,y),before=pressed;pressed=-1;if(!valid||i<0||i!=before||!goals[i].earned)return;const int next=selected==i+1?0:i+1;
        if(TeamGoalsStorage::Equip(app,next)){selected=next;status=next?ModText::Tr("展示称号：","Displayed title: ")+goals[i].name:ModText::Tr("已取消展示称号。","Title display removed.");}
        else status=ModText::Tr("称号未保存，请检查可用空间。","Title was not saved; check available storage.");}
    void Draw(Sexy::Graphics*g){
        SeatUi::Panel(g,Sexy::Rect(0,0,800,600));
        SeatUi::Text(g,ModText::Tr("合作收藏","Teamwork collection"),32,46,580,25,Sexy::Color(245,230,165));
        SeatUi::Text(g,ModText::Tr("称号与纪念收藏不增加战斗属性。","Titles and collectibles do not grant combat bonuses."),32,80,730,16,Sexy::Color(225,220,200));
        for(int i=0;i<7;++i){const auto&r=Row(i);const auto&goal=goals[i];SeatUi::Strip(g,r);
            const auto color=goal.earned?Sexy::Color(245,222,130):Sexy::Color(185,180,165);
            SeatUi::Text(g,goal.name,r.mX+12,r.mY+22,228,19,color);
            SeatUi::Text(g,goal.description,r.mX+244,r.mY+21,470,14,Sexy::Color(230,225,205));
            SeatUi::Text(g,goal.progress,r.mX+244,r.mY+44,120,15,color);
            SeatUi::Text(g,selected==i+1?ModText::Tr("展示中","Displayed"):goal.earned?ModText::Tr("点击展示", "Select to wear"):ModText::Tr("尚未解锁","Locked"),r.mX+12,r.mY+44,228,14,color);
        }
        SeatUi::Paragraph(g,status,Sexy::Rect(32,524,730,57),16,Sexy::Color(245,230,165));
    }
};
