#include "CjkFont.h"
#include "Lawn/Observer/ObserverDrawScene.h"
#include "Graphics.h"
#include "MemoryImage.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <vector>
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

namespace Sexy
{
namespace
{
struct FontData
{
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info{};
    bool valid = false;
    FontData()
    {
        SDL_RWops* stream = SDL_RWFromFile("fonts/NotoSansCJKsc-Regular.otf", "rb");
        if (!stream) return;
        const Sint64 size = SDL_RWsize(stream);
        if (size > 0 && size < 32 * 1024 * 1024)
        {
            bytes.resize(static_cast<size_t>(size));
            if (SDL_RWread(stream, bytes.data(), 1, bytes.size()) == bytes.size())
                valid = stbtt_InitFont(&info, bytes.data(), stbtt_GetFontOffsetForIndex(bytes.data(), 0));
        }
        SDL_RWclose(stream);
    }
};

struct Glyph
{
    std::unique_ptr<MemoryImage> image;
    int advance = 0, x = 0, y = 0;
};

struct RasterCache
{
    std::map<char32_t, Glyph> glyphs;
};

class CjkFont final : public _Font
{
    std::shared_ptr<FontData> data;
    std::shared_ptr<RasterCache> cache = std::make_shared<RasterCache>();
    float scale;
    Glyph& GetGlyph(char32_t cp)
    {
        auto found = cache->glyphs.find(cp);
        if (found != cache->glyphs.end()) return found->second;
        if (cache->glyphs.size() >= 512) cache->glyphs.clear();
        auto& glyph = cache->glyphs[cp];
        int advance, bearing;
        stbtt_GetCodepointHMetrics(&data->info, cp, &advance, &bearing);
        glyph.advance = static_cast<int>(std::round(advance * scale));
        int width, height;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&data->info, scale, scale, cp, &width, &height, &glyph.x, &glyph.y);
        if (bitmap && width > 0 && height > 0)
        {
            glyph.image = std::make_unique<MemoryImage>();
            glyph.image->Create(width, height);
            uint32_t* pixels = glyph.image->GetBits();
            for (int i = 0; i < width * height; ++i) pixels[i] = (uint32_t(bitmap[i]) << 24) | 0x00ffffff;
            glyph.image->BitsChanged();
        }
        stbtt_FreeBitmap(bitmap, nullptr);
        return glyph;
    }
public:
    CjkFont(std::shared_ptr<FontData> fontData, int height) : data(std::move(fontData))
    {
        scale = stbtt_ScaleForPixelHeight(&data->info, height);
        int ascent, descent, gap;
        stbtt_GetFontVMetrics(&data->info, &ascent, &descent, &gap);
        mAscent = static_cast<int>(std::ceil(ascent * scale));
        mHeight = height;
        mAscentPadding = 0;
        mLineSpacingOffset = static_cast<int>(std::ceil(gap * scale));
    }
    int CharWidth(char32_t cp) override
    {
        int advance, bearing;
        stbtt_GetCodepointHMetrics(&data->info, cp, &advance, &bearing);
        return static_cast<int>(std::round(advance * scale));
    }
    int CharWidthKern(char32_t cp, char32_t prev) override
    {
        return CharWidth(cp) + (prev ? static_cast<int>(std::round(stbtt_GetCodepointKernAdvance(&data->info, prev, cp) * scale)) : 0);
    }
    int StringWidth(std::string_view text) override
    {
        size_t offset = 0;
        char32_t cp, prev = 0;
        int width = 0;
        while (UTF8DecodeNext(text, offset, cp)) { width += CharWidthKern(cp, prev); prev = cp; }
        return width;
    }
    void DrawString(Graphics* g, int x, int y, std::string_view text, const Color& color, const Rect& clip) override
    {
        if(auto*capture=dynamic_cast<Observer::DrawRecorder*>(g->mDestImage)){
            // Text commands use a top-left origin and point size on both platforms.
            capture->Text(std::string(text),x+int(g->mTransX),y-mAscent+int(g->mTransY),
                int(std::round(mHeight*72.0/96.0)),false,color,clip);return;
        }
        Graphics draw(*g);
        draw.SetColorizeImages(true);
        draw.SetColor(color);
        // Graphics' clip is already in translated device coordinates.
        draw.mClipRect = draw.mClipRect.Intersection(clip);
        size_t offset = 0;
        char32_t cp, prev = 0;
        while (UTF8DecodeNext(text, offset, cp))
        {
            if (prev) x += static_cast<int>(std::round(stbtt_GetCodepointKernAdvance(&data->info, prev, cp) * scale));
            auto& glyph = GetGlyph(cp);
            if (glyph.image) draw.DrawImage(glyph.image.get(), x + glyph.x, y + glyph.y);
            x += glyph.advance;
            prev = cp;
        }
    }
    _Font* Duplicate() override { return new CjkFont(*this); }
};

// Freed explicitly while the app/rendering context is still alive.
std::map<int, std::unique_ptr<CjkFont>>* fonts = new std::map<int, std::unique_ptr<CjkFont>>;
}

_Font* GetCjkFont(int height)
{
    static auto data = std::make_shared<FontData>();
    if (!data->valid) return nullptr;
    height = std::clamp(height, 10, 72);
    auto& font = (*fonts)[height];
    if (!font) font = std::make_unique<CjkFont>(data, height);
    return font.get();
}

void ClearCjkFonts() { fonts->clear(); }
}
