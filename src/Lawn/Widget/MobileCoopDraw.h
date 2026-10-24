#pragma once
#include <algorithm>
#include "../../Resources.h"
#include "../ModText.h"
#include "GameButton.h"
#include "MobileButtonFont.h"
#include "graphics/Font.h"
#include "graphics/Graphics.h"
#include "graphics/Image.h"
#include "widget/ButtonWidget.h"

inline std::string MobileTextLine(Sexy::_Font* font, std::string text, int width)
{
    if (font->StringWidth(text) <= width) return text;
    while (!text.empty() && font->StringWidth(text + "...") > width) {
        size_t end = text.size() - 1;
        while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
        text.resize(end);
    }
    return text + "...";
}

// Same stone/down art as PC; keep state and UTF-8 layout local to this draw.
inline void DrawMobileStone(Sexy::Graphics *g, const Sexy::Rect &rect, const std::string &label,
                            bool down = false, bool enabled = true, bool selected = false)
{
    Sexy::Graphics stone(*g);
    stone.SetColorizeImages(!enabled);
    stone.SetColor(Sexy::Color(145,145,145));
    auto* left = (down || selected) ? Sexy::IMAGE_BUTTON_DOWN_LEFT : Sexy::IMAGE_BUTTON_LEFT;
    auto* middle = (down || selected) ? Sexy::IMAGE_BUTTON_DOWN_MIDDLE : Sexy::IMAGE_BUTTON_MIDDLE;
    auto* right = (down || selected) ? Sexy::IMAGE_BUTTON_DOWN_RIGHT : Sexy::IMAGE_BUTTON_RIGHT;
    stone.DrawImage(left, Sexy::Rect(rect.mX, rect.mY, left->mWidth, rect.mHeight),
        Sexy::Rect(0, 0, left->mWidth, left->mHeight));
    stone.DrawImage(middle, Sexy::Rect(rect.mX + left->mWidth, rect.mY,
        rect.mWidth - left->mWidth - right->mWidth, rect.mHeight),
        Sexy::Rect(0, 0, middle->mWidth, middle->mHeight));
    stone.DrawImage(right, Sexy::Rect(rect.mX + rect.mWidth - right->mWidth, rect.mY,
        right->mWidth, rect.mHeight), Sexy::Rect(0, 0, right->mWidth, right->mHeight));
    MobileButtonFont artwork(down || selected);
    auto* font = &artwork;
    const auto line = MobileTextLine(font, label, rect.mWidth - 24);
    Sexy::Graphics text(*g);
    text.ClipRect(rect.mX+10,rect.mY,rect.mWidth-20,rect.mHeight);
    text.SetFont(font);
    int x=rect.mX+(rect.mWidth-font->StringWidth(line))/2;
    int y=rect.mY+(rect.mHeight+font->GetAscent())/2-3;
    text.SetColor(enabled ? Sexy::Color::White : Sexy::Color(145,145,145));
    text.DrawString(line,x,y);
}

class MobileStoneButton : public Sexy::ButtonWidget
{
  public:
    MobileStoneButton(int id, Sexy::ButtonListener *listener) : ButtonWidget(id, listener)
    {
    }
    void Draw(Sexy::Graphics *g) override
    {
        DrawMobileStone(g, Sexy::Rect(0, 0, mWidth, mHeight), mLabel, mIsDown && mIsOver,
            !mDisabled, mInverted);
    }
};

inline void DrawMobilePanel(Sexy::Graphics *g, const Sexy::Rect &rect)
{
    // @pvz-online: preserve the original frame instead of stretching only its flat center.
    Sexy::Image* tiles[3][3] = {
        {Sexy::IMAGE_DIALOG_TOPLEFT, Sexy::IMAGE_DIALOG_TOPMIDDLE, Sexy::IMAGE_DIALOG_TOPRIGHT},
        {Sexy::IMAGE_DIALOG_CENTERLEFT, Sexy::IMAGE_DIALOG_CENTERMIDDLE, Sexy::IMAGE_DIALOG_CENTERRIGHT},
        {Sexy::IMAGE_DIALOG_BOTTOMLEFT, Sexy::IMAGE_DIALOG_BOTTOMMIDDLE, Sexy::IMAGE_DIALOG_BOTTOMRIGHT}
    };
    int left=std::min(tiles[0][0]->mWidth,rect.mWidth/3);
    int right=std::min(tiles[0][2]->mWidth,rect.mWidth/3);
    int top=std::min(tiles[0][0]->mHeight,rect.mHeight/3);
    int bottom=std::min(tiles[2][0]->mHeight,rect.mHeight/3);
    int widths[]={left,rect.mWidth-left-right,right};
    int heights[]={top,rect.mHeight-top-bottom,bottom};
    Sexy::Graphics background(*g); background.SetColorizeImages(false);
    int y=rect.mY;
    for(int row=0;row<3;++row) {
        int x=rect.mX;
        for(int col=0;col<3;++col) {
            auto* image=tiles[row][col];
            background.DrawImage(image,Sexy::Rect(x,y,widths[col],heights[row]),
                Sexy::Rect(0,0,image->mWidth,image->mHeight));
            x+=widths[col];
        }
        y+=heights[row];
    }
}

// Compact HUD uses the same dialog stone without squeezing its large outer frame.
inline void DrawMobileStrip(Sexy::Graphics* g, const Sexy::Rect& rect)
{
    Sexy::Graphics strip(*g); strip.SetColorizeImages(false);
    auto* image=Sexy::IMAGE_DIALOG_CENTERMIDDLE;
    strip.DrawImage(image,rect,Sexy::Rect(0,0,image->mWidth,image->mHeight));
    strip.SetColor(Sexy::Color(175,160,120));
    strip.DrawRect(rect.mX,rect.mY,rect.mWidth-1,rect.mHeight-1);
}
