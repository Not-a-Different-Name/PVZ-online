#pragma once
#include "OnlineUiDraw.h"

namespace NameplateDraw {
inline int LabelWidth(const std::string& value)
{
#ifdef __ANDROID__
    MobileButtonFont font(false);return font.StringWidth(value);
#else
    return CjkStoneButtonLabelWidth(value);
#endif
}
inline void TrimGlyph(std::string& value)
{
    if(value.empty())return;
    size_t end=value.size()-1;
    while(end>0&&(static_cast<unsigned char>(value[end])&0xc0)==0x80)--end;
    value.resize(end);
}
inline std::string FitLabel(std::string value,int width)
{
    if(LabelWidth(value)<=width)return value;
    while(!value.empty()&&LabelWidth(value+"...")>width)TrimGlyph(value);
    return LabelWidth(value+"...")<=width?value+"...":std::string();
}
inline std::string Label(const std::string& value,int width)
{
    if(LabelWidth(value)<=width)return value;
    const std::string divider=" · ";const auto split=value.rfind(divider);
    if(split==std::string::npos)return FitLabel(value,width);
    const auto original=value.substr(0,split),title=value.substr(split+divider.size());
    size_t minimum=0;
    for(int glyph=0;glyph<2&&minimum<original.size();++glyph){++minimum;while(minimum<original.size()&&(static_cast<unsigned char>(original[minimum])&0xc0)==0x80)++minimum;}
    auto prefix=original;
    while(prefix.size()>minimum&&LabelWidth(prefix+"..."+divider+title)>width)TrimGlyph(prefix);
    if(prefix!=original)prefix+="...";
    auto suffix=title;
    while(!suffix.empty()&&LabelWidth(prefix+divider+suffix+(suffix==title?"":"..."))>width)TrimGlyph(suffix);
    return suffix.empty()?FitLabel(value,width):prefix+divider+suffix+(suffix==title?"":"...");
}
inline void Leaf(Sexy::Graphics* graphics,int x,int y,Sexy::Color color,bool reverse=false)
{
    graphics->SetColor(Sexy::Color(35,37,32));
    for(int row=0;row<8;++row) {
        const int width=row<4?6+row*2:20-row*2;
        graphics->FillRect(x+(reverse?8-row:row),y+row*2,width,2);
    }
    graphics->SetColor(color);
    for(int row=1;row<7;++row) {
        const int width=row<4?4+row*2:18-row*2;
        graphics->FillRect(x+(reverse?9-row:row+1),y+row*2,width,2);
    }
    graphics->SetColor(Sexy::Color(color.mRed*3/4,color.mGreen*3/4,color.mBlue*3/4));
    graphics->DrawLine(x+3,y+3,x+16,y+14);
}
inline void Draw(Sexy::Graphics* graphics,Sexy::Rect bounds,int id,const std::string& name)
{
    if(id<1 || id>3)return;
    const int tier=id-1;
    const Sexy::Color accents[]={Sexy::Color(183,121,73),Sexy::Color(196,209,216),Sexy::Color(237,198,95)};
    const auto accent=accents[tier];
    const auto fitted=Label(name,bounds.mWidth-112);
    SeatUi::Button(graphics,bounds,fitted.c_str(),true);
    Sexy::Graphics ornament(*graphics);ornament.ClipRect(bounds);
    ornament.SetColor(Sexy::Color(44,42,53));
    ornament.DrawRect(bounds.mX+9,bounds.mY+8,bounds.mWidth-19,bounds.mHeight-17);
    ornament.SetColor(accent);
    const int length=tier==0?28:tier==1?48:68;
    for(int side=0;side<2;++side) {
        const int start=side?bounds.mX+bounds.mWidth-18-length:bounds.mX+17;
        ornament.DrawLine(start,bounds.mY+10,start+length,bounds.mY+10);
        ornament.DrawLine(start,bounds.mY+bounds.mHeight-12,start+length,bounds.mY+bounds.mHeight-12);
    }
    const int centerY=bounds.mY+(bounds.mHeight-16)/2;
    Leaf(&ornament,bounds.mX+13,centerY,accent);
    Leaf(&ornament,bounds.mX+bounds.mWidth-34,centerY,accent,true);
    if(tier>0) {
        Leaf(&ornament,bounds.mX+24,centerY-9,accent,true);
        Leaf(&ornament,bounds.mX+bounds.mWidth-45,centerY+8,accent);
    }
    if(tier>1) {
        Leaf(&ornament,bounds.mX+24,centerY+9,accent);
        Leaf(&ornament,bounds.mX+bounds.mWidth-45,centerY-9,accent,true);
    }
}
}
