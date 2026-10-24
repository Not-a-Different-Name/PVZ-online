#pragma once
#include "../../SexyAppFramework/Common.h"
#include "LawnDialog.h"
#include "OnlineUiDraw.h"
#include "../Online/AutomaticRooms.h"
#include "../Online/RoomPresentation.h"

class RoomDirectoryDialog : public LawnDialog {
    LawnApp* app;
    RelayNodes::Directory browser;
    int selected=-1,down=-1;
    static Rect Row(int i){return Rect(48,152+i*54,704,48);}
    static Rect Back(){return Rect(48,528,144,46);}
    static Rect Refresh(){return Rect(208,528,136,46);}
    static Rect Prev(){return Rect(360,528,96,46);}
    static Rect Next(){return Rect(472,528,96,46);}
    static Rect Join(){return Rect(584,528,168,46);}
    bool Joinable()const{return selected>=0 && selected<int(browser.entries.size()) && browser.entries[selected].room.Joinable() && browser.state==RelayNodes::Directory::READY && !RoomPresentation::Active(RoomPresentation::Session(app));}
    int Hit(int x,int y) {
        if(Back().Contains(x,y))return 100;
        if(Refresh().Contains(x,y)&&browser.CanRefresh())return 101;
        if(Prev().Contains(x,y)&&browser.page>0&&browser.CanRefresh())return 102;
        if(Next().Contains(x,y)&&browser.HasNext()&&browser.CanRefresh())return 103;
        if(Join().Contains(x,y)&&Joinable())return 104;
        for(int i=0;i<int(browser.entries.size());++i)if(Row(i).Contains(x,y))return i;
        return -1;
    }

public:
    explicit RoomDirectoryDialog(LawnApp* a,bool refresh=true):LawnDialog(a,DIALOG_ROOM_DIRECTORY,true,"","","",Dialog::BUTTONS_NONE),app(a) {
        Resize(0,0,800,600);Sexy::MkDir(Sexy::GetAppDataFolder()+"userdata");RelayNodes::SetCatalogCachePath(Sexy::GetAppDataFolder()+"userdata/relay-nodes.bin");if(refresh)browser.Refresh();
    }
    void Update() override {LawnDialog::Update();browser.Update();MarkDirty();}
    void Draw(Graphics* g) override {
        SeatUi::Panel(g,Rect(0,0,800,600));
        SeatUi::Text(g,ModText::Tr("公开房间","Public rooms"),48,76,704,29,Color(245,230,165),true);
        SeatUi::Text(g,ModText::Tr("已开局请返回房间，用「粘贴加入」。","To join a run in progress, go Back and use Paste & join."),48,110,704,17,Color(205,220,225),true);
        std::string status=browser.state==RelayNodes::Directory::LOADING?ModText::Tr("正在找房间…","Loading rooms..."):
            browser.state==RelayNodes::Directory::UNAVAILABLE?ModText::Tr("没能找到房间，请点「刷新」。","Could not load rooms. Choose Refresh."):
            browser.partial?ModText::Tr("部分房间没显示，可点「刷新」重试。","Some rooms are missing. Choose Refresh to retry."):
            browser.entries.empty()?ModText::Tr("暂无公开房间，返回后可「创建房间」。","No public rooms. Go Back to create one."):
            ModText::Tr("选中房间，再点「加入房间」。","Select a room, then choose Join room.");
        SeatUi::Text(g,status,48,138,704,17,browser.state==RelayNodes::Directory::UNAVAILABLE?Color(255,175,120):Color(210,220,195),true);
        for(int i=0;i<int(browser.entries.size());++i) {
            const auto& located=browser.entries[i];const auto& entry=located.room;auto box=Row(i);SeatUi::Strip(g,box);
            if(i==selected){g->SetColor(Color(225,205,100));g->DrawRect(box.mX+1,box.mY+1,box.mWidth-3,box.mHeight-3);}
            const auto color=entry.Joinable()?Color(245,230,165):Color(175,175,170);
            SeatUi::Text(g,entry.code+"  "+entry.host,box.mX+12,box.mY+29,382,21,color);
            const std::string availability=entry.build!=NetProto::MOD_BUILD?ModText::Tr("版本不同，需更新","Update needed"):
                entry.players>=6?ModText::Tr("已满","Full"):ModText::Tr("等待开局","Waiting");
            SeatUi::Text(g,std::to_string(entry.players)+"/6  ·  "+availability+(entry.build!=NetProto::MOD_BUILD?"  ·  MOD"+std::to_string(entry.build):std::string{}),box.mX+404,box.mY+29,288,18,color);
        }
        SeatUi::Text(g,ModText::Tr("第 ","Page ")+std::to_string(browser.page+1)+ModText::Tr(" 页",""),48,490,704,18,Color(205,210,195),true);
        SeatUi::Button(g,Back(),ModText::Tr("返回房间","Back"),true,down==100);
        SeatUi::Button(g,Refresh(),ModText::Tr("刷新","Refresh"),browser.CanRefresh(),down==101);
        SeatUi::Button(g,Prev(),ModText::Tr("上页","Prev"),browser.page>0&&browser.CanRefresh(),down==102);
        SeatUi::Button(g,Next(),ModText::Tr("下页","Next"),browser.HasNext()&&browser.CanRefresh(),down==103);
        SeatUi::Button(g,Join(),ModText::Tr("加入房间","Join room"),Joinable(),down==104);
    }
    void MouseDown(int x,int y,int clicks) override{down=clicks>0?Hit(x,y):-1;MarkDirty();}
    void MouseUp(int x,int y,int clicks) override {
        int hit=Hit(x,y),pressed=down;down=-1;if(clicks<=0||hit<0||hit!=pressed)return;
        if(hit==100){browser.Close();app->KillDialog(DIALOG_ROOM_DIRECTORY);return;}
        if(hit==101){if(browser.Refresh())selected=-1;return;}
        if(hit==102){if(browser.Prev())selected=-1;return;}
        if(hit==103){if(browser.Next())selected=-1;return;}
        if(hit==104 && Joinable()) {
            const auto target=browser.entries[selected];browser.Close();
            auto* session=RoomPresentation::Session(app);
            session->StartDirectoryJoin(target.endpoint.host.c_str(),target.room.code.c_str(),target.endpoint.port);app->KillDialog(DIALOG_ROOM_DIRECTORY);return;
        }
        if(hit<RoomDirectory::PAGE_SIZE)selected=hit;
    }
    void KeyDown(KeyCode key) override{if(key==KEYCODE_ESCAPE){browser.Close();app->KillDialog(DIALOG_ROOM_DIRECTORY);}}
};
