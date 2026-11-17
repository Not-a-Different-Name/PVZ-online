#pragma once
#include "../../LawnApp.h"
#include "../Board.h"
#include "../Observer/ObserverBoard.h"
#include "../Widget/GameSelector.h"
#include "../Widget/TitleScreen.h"
#include "../../Resources.h"
#ifdef __ANDROID__
#include "../../PvzpLib/Reanimator.h"
#else
#include "../../Sexy.TodLib/Reanimator.h"
#endif
#include "graphics/MemoryImage.h"
#include "graphics/Graphics.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

class SideBackdropCache {
    std::unique_ptr<Sexy::MemoryImage> menu, output;
    Sexy::Image* lastSource=nullptr;
    int lastWidth=0,lastOffset=0,lastScene=-1,lastMenu=-1,lastMenuX=0,lastMenuY=0;
    unsigned int lastAnimation=0;
public:
    Sexy::Image* Get(LawnApp* app,int width) {
        using namespace Sexy;
        if(width<=800||width>4096)return nullptr;
        if(app->mTitleScreen&&(app->mTitleScreen->mTitleState!=TITLESTATE_SCREEN||!app->mTitleScreen->mLoaderScreenIsLoaded))return nullptr;
        Image* source=nullptr;int offset=0,scene=0;
        const Board* board=app->mBoard;
        if(app->mBoard&&app->mBoard->mObserverView&&app->mBoard->IsWatchingView())if(auto* watched=app->mBoard->mObserverView->LocalBattle())board=watched;
        if(app->GetDialog(DIALOG_ALMANAC)) {source=IMAGE_ALMANAC_INDEXBACK;scene=4;}
        else if(app->GetDialog(DIALOG_STORE)) {source=app->IsNight()?IMAGE_STORE_BACKGROUNDNIGHT:IMAGE_STORE_BACKGROUND;scene=4;}
        else if(board&&app->mGameScene!=SCENE_MENU) {
            switch(board->mBackground) {
                case BACKGROUND_1_DAY:source=IMAGE_BACKGROUND1;break;
                case BACKGROUND_2_NIGHT:source=IMAGE_BACKGROUND2;break;
                case BACKGROUND_3_POOL:source=IMAGE_BACKGROUND3;break;
                case BACKGROUND_4_FOG:source=IMAGE_BACKGROUND4;break;
                case BACKGROUND_5_ROOF:source=IMAGE_BACKGROUND5;break;
                case BACKGROUND_6_BOSS:source=IMAGE_BACKGROUND6BOSS;break;
                case BACKGROUND_MUSHROOM_GARDEN:source=IMAGE_BACKGROUND_MUSHROOMGARDEN;break;
                case BACKGROUND_GREENHOUSE:source=IMAGE_BACKGROUND_GREENHOUSE;break;
                case BACKGROUND_ZOMBIQUARIUM:source=IMAGE_AQUARIUM1;break;
                default:break;
            }
            scene=1;
            if(board==app->mBoard&&app->IsFirstTimeAdventureMode()&&board->mLevel<=3)source=IMAGE_BACKGROUND1UNSODDED;
            if(source&&source->GetWidth()>=BOARD_WIDTH+BOARD_OFFSET)offset=BOARD_OFFSET-board->mX;
            if(!source&&board->mBackground==BACKGROUND_TREEOFWISDOM)source=IMAGE_REANIM_TREE_BG;
        } else if(app->mTitleScreen) {source=IMAGE_TITLESCREEN;scene=2;}
        else if(app->mGameSelector) {
#ifdef __ANDROID__
            auto* selector=app->mGameSelector.get();
#else
            auto* selector=app->mGameSelector;
#endif
            const auto animation=static_cast<unsigned int>(selector->mSelectorReanimID);
            auto* reanim=app->ReanimationTryToGet(selector->mSelectorReanimID);
            if(!reanim)return nullptr;
            const int state=static_cast<int>(selector->mSelectorState)*128+
                (selector->mSelectorState==SelectorAnimState::SELECTOR_IDLE?0:static_cast<int>(reanim->mAnimTime*60));
            if(!menu||lastAnimation!=animation||lastMenu!=state||lastMenuX!=selector->mX||lastMenuY!=selector->mY) {
                menu=std::make_unique<MemoryImage>(app);menu->Create(800,600);
                std::fill(menu->GetBits(),menu->GetBits()+800*600,0);
                Graphics graphics(menu.get());
                if(IMAGE_REANIM_SELECTORSCREEN_BG)graphics.DrawImage(IMAGE_REANIM_SELECTORSCREEN_BG,Rect(0,0,800,600),
                    Rect(0,0,IMAGE_REANIM_SELECTORSCREEN_BG->GetWidth(),IMAGE_REANIM_SELECTORSCREEN_BG->GetHeight()));
                graphics.Translate(selector->mX,selector->mY);
                selector->Draw(&graphics);
                lastAnimation=animation;lastMenu=state;lastMenuX=selector->mX;lastMenuY=selector->mY;
                lastSource=nullptr;
            }
            source=menu.get();offset=0;scene=3;
        }
        if(!source)return nullptr;
        if(output&&lastSource==source&&lastWidth==width&&lastOffset==offset&&lastScene==scene)return output.get();
        auto* memory=dynamic_cast<MemoryImage*>(source);
        if(!memory)return nullptr;
        const int sw=source->GetWidth(),sh=source->GetHeight();
        if(sw<=0||sh<=0)return nullptr;
        const auto* pixels=memory->GetBits();
        if(!pixels)return nullptr;
        constexpr int bw=96,bh=48;
        std::vector<unsigned int> samples(bw*bh),blur(bw*bh);
        for(int y=0;y<bh;++y)for(int x=0;x<bw;++x) {
            int sx=std::clamp(offset+(x*800)/bw,0,sw-1),sy=std::min(sh-1,y*sh/bh);
            samples[y*bw+x]=pixels[sy*sw+sx];
        }
        for(int y=0;y<bh;++y)for(int x=0;x<bw;++x) {
            int red=0,green=0,blue=0;
            for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx) {
                auto color=samples[std::clamp(y+dy,0,bh-1)*bw+std::clamp(x+dx,0,bw-1)];
                red+=(color>>16)&255;green+=(color>>8)&255;blue+=color&255;
            }
            red/=25;green/=25;blue/=25;int grey=(red*3+green*6+blue)/10;
            red=(red*3+grey)*18/100;green=(green*3+grey)*18/100;blue=(blue*3+grey)*18/100;
            blur[y*bw+x]=0xff000000u|(red<<16)|(green<<8)|blue;
        }
        if(!output)output=std::make_unique<MemoryImage>(app);
        output->Delete3DBuffers();
        output->Create(width,600);auto* target=output->GetBits();
        const int pad=(width-800)/2;
        auto mix=[](unsigned int first,unsigned int second,int fraction) {
            unsigned int value=0xff000000u;
            for(int shift:{0,8,16})value|=((((first>>shift)&255)*(256-fraction)+((second>>shift)&255)*fraction)/256)<<shift;
            return value;
        };
        for(int y=0;y<600;++y)for(int x=0;x<width;++x) {
            const int sx=x-pad+offset,sy=std::min(sh-1,y*sh/600);
            const int fx=x*(bw-1)*256/(width-1),fy=y*(bh-1)*256/599;
            const int bx=fx/256,by=fy/256,nx=std::min(bw-1,bx+1),ny=std::min(bh-1,by+1);
            auto back=mix(mix(blur[by*bw+bx],blur[by*bw+nx],fx%256),mix(blur[ny*bw+bx],blur[ny*bw+nx],fx%256),fy%256);
            if(sx>=0&&sx<sw) {
                auto front=pixels[sy*sw+sx];
                const int feather=std::min(96,std::max(1,x<pad?offset:sw-offset-800));
                int blend=std::min(feather,x<pad?sx:sw-1-sx)*255/feather;
                blend=std::clamp(blend,0,255);
                if(scene==3||scene==4)blend=0;
                blend=blend*int((front>>24)&255)/255;
                int red=(((front>>16)&255)*blend+((back>>16)&255)*(255-blend))/255;
                int green=(((front>>8)&255)*blend+((back>>8)&255)*(255-blend))/255;
                int blue=((front&255)*blend+(back&255)*(255-blend))/255;
                back=0xff000000u|(red<<16)|(green<<8)|blue;
            }
            target[y*width+x]=back;
        }
        output->BitsChanged();lastSource=source;lastWidth=width;lastOffset=offset;lastScene=scene;
        return output.get();
    }
};

