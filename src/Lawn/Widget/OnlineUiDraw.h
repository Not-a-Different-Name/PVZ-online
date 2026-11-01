#pragma once
#include "../../Resources.h"
#include "../ModText.h"
#include "graphics/Graphics.h"
#include "graphics/Font.h"
#include "graphics/Image.h"
#include <algorithm>
#include <sstream>
#ifdef __ANDROID__
#include "MobileCoopDraw.h"
#include "graphics/CjkFont.h"
#else
#include "CjkStoneButton.h"
#endif

// @pvz-online: shared readable stone UI; layout and hit regions use the same rectangles.
namespace SeatUi {
using namespace Sexy;
inline _Font* BodyFont(int size) {
    auto* font=GetCjkUiFont(size);
    return font?font:ModText::FontFor(FONT_DWARVENTODCRAFT18,size);
}
inline int Width(const std::string& text,int size) {
#ifdef __ANDROID__
    return BodyFont(size)->StringWidth(text);
#else
    return ModText::TextWidth(ModText::GetFont(CjkPointSize(size),false),ModText::WideFromUtf8(text.c_str()));
#endif
}
inline std::string Fit(std::string text,int width,int size) {
    if(Width(text,size)<=width)return text;
    while(!text.empty()&&Width(text+"...",size)>width) {
        size_t end=text.size()-1;
        while(end>0&&(static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;
        text.resize(end);
    }
    return text+"...";
}
inline void Text(Graphics* g,std::string text,int x,int baseline,int width,int size,Color color,bool center=false) {
    Graphics line(*g);line.ClipRect(x,baseline-size-3,width,size+10);
    text=Fit(text,width,size);
#ifdef __ANDROID__
    auto* font=BodyFont(size);
    line.SetFont(font);line.SetColor(color);
    line.DrawString(text,x+(center?(width-font->StringWidth(text))/2:0),baseline);
#else
    auto* font=ModText::GetFont(CjkPointSize(size),false);
    auto wide=ModText::WideFromUtf8(text.c_str());
    ModText::DrawTextWide(&line,font,x+(center?(width-ModText::TextWidth(font,wide))/2:0),baseline-ModText::Ascent(font),wide,color,line.mClipRect);
#endif
}
inline void Paragraph(Graphics* g,const std::string& text,Rect box,int size,Color color) {
    std::istringstream source(text);std::string remaining;int y=box.mY+size;
    while(std::getline(source,remaining)) {
        do {
            size_t end=0,space=std::string::npos;
            while(end<remaining.size()) {
                size_t next=end+1;
                while(next<remaining.size()&&(static_cast<unsigned char>(remaining[next])&0xc0)==0x80)++next;
                if(Width(remaining.substr(0,next),size)>box.mWidth&&end>0)break;
                if(remaining[end]==' ')space=end;
                end=next;
            }
            if(end<remaining.size()&&space!=std::string::npos&&space>0)end=space;
            if(y>box.mY+box.mHeight)return;
            Text(g,remaining.substr(0,end),box.mX,y,box.mWidth,size,color);
            remaining.erase(0,end);
            while(!remaining.empty()&&remaining.front()==' ')remaining.erase(0,1);
            y+=size+6;
        } while(!remaining.empty());
    }
}
inline void Panel(Graphics* g,Rect rect) {
    Image* tiles[3][3]={{IMAGE_DIALOG_TOPLEFT,IMAGE_DIALOG_TOPMIDDLE,IMAGE_DIALOG_TOPRIGHT},
        {IMAGE_DIALOG_CENTERLEFT,IMAGE_DIALOG_CENTERMIDDLE,IMAGE_DIALOG_CENTERRIGHT},
        {IMAGE_DIALOG_BOTTOMLEFT,IMAGE_DIALOG_BOTTOMMIDDLE,IMAGE_DIALOG_BOTTOMRIGHT}};
    int left=std::min(tiles[0][0]->mWidth,rect.mWidth/3),right=std::min(tiles[0][2]->mWidth,rect.mWidth/3);
    int top=std::min(tiles[0][0]->mHeight,rect.mHeight/3),bottom=std::min(tiles[2][0]->mHeight,rect.mHeight/3);
    int widths[]={left,rect.mWidth-left-right,right},heights[]={top,rect.mHeight-top-bottom,bottom};
    Graphics panel(*g);panel.SetColorizeImages(false);int y=rect.mY;
    for(int row=0;row<3;++row) {int x=rect.mX;
        for(int col=0;col<3;++col) {auto* image=tiles[row][col];
            panel.DrawImage(image,Rect(x,y,widths[col],heights[row]),Rect(0,0,image->mWidth,image->mHeight));x+=widths[col];}
        y+=heights[row];
    }
}
inline void Strip(Graphics* g,Rect rect) {
    Graphics strip(*g);strip.SetColorizeImages(false);auto* image=IMAGE_DIALOG_CENTERMIDDLE;
    strip.DrawImage(image,rect,Rect(0,0,image->mWidth,image->mHeight));
    strip.SetColor(Color(175,160,120));strip.DrawRect(rect.mX,rect.mY,rect.mWidth-1,rect.mHeight-1);
}
inline void Button(Graphics* g,Rect r,const char* label,bool enabled,bool down=false,bool highlighted=false) {
    Graphics stone(*g);stone.SetColorizeImages(!enabled);stone.SetColor(Color(145,145,145));
    Image* left=down?IMAGE_BUTTON_DOWN_LEFT:IMAGE_BUTTON_LEFT;
    Image* mid=down?IMAGE_BUTTON_DOWN_MIDDLE:IMAGE_BUTTON_MIDDLE;
    Image* right=down?IMAGE_BUTTON_DOWN_RIGHT:IMAGE_BUTTON_RIGHT;
    // Compact HUD buttons must fit the stone end caps inside their hit rectangle.
    int leftWidth=std::min(left->mWidth,r.mWidth/3),rightWidth=std::min(right->mWidth,r.mWidth/3);
    stone.DrawImage(left,Rect(r.mX,r.mY,leftWidth,r.mHeight),Rect(0,0,left->mWidth,left->mHeight));
    stone.DrawImage(mid,Rect(r.mX+leftWidth,r.mY,r.mWidth-leftWidth-rightWidth,r.mHeight),Rect(0,0,mid->mWidth,mid->mHeight));
    stone.DrawImage(right,Rect(r.mX+r.mWidth-rightWidth,r.mY,rightWidth,r.mHeight),Rect(0,0,right->mWidth,right->mHeight));
#ifdef __ANDROID__
    MobileButtonFont font(down || highlighted);
    const int inset=r.mWidth<64?4:10;
    const auto line = MobileTextLine(&font,label,r.mWidth-inset*2);
    Graphics text(*g);text.ClipRect(r.mX+inset,r.mY,r.mWidth-inset*2,r.mHeight);
    text.SetFont(&font);text.SetColor(enabled?Color::White:Color(145,145,145));
    text.DrawString(line,r.mX+(r.mWidth-font.StringWidth(line))/2,r.mY+(r.mHeight+font.GetAscent())/2-3);
#else
    Text(g,label,r.mX+10,r.mY+(r.mHeight+22)/2-3,r.mWidth-20,22,enabled?Color(245,230,165):Color(170,170,160),true);
#endif
}
}
