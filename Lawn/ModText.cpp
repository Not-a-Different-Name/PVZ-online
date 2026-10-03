#include "ModText.h"
#include "SexyAppBase.h"
#include "graphics/Graphics.h"
#include "graphics/DDImage.h"
#include "graphics/DDInterface.h"
#include "graphics/D3DInterface.h"
#include "graphics/MemoryImage.h"
#include "widget/WidgetManager.h"
#include <algorithm>

using namespace Sexy;

// 不透明字体（只在本文件里成型）：一支 GDI 宽字符字体 + 基本度量。
class ModText::Font
{
public:
	HFONT	mHFont;
	int		mHeight;
	int		mAscent;
};

// ── 语言档 ─────────────────────────────────────────────────────────────
// 存注册表（SexyAppBase 的 Registry 助手，键 "ModLanguage"；CurUser 走同一条路）。
// 读失败保留默认 MODLANG_AUTO——首次运行没有这个值，此时按系统码页自动判。

int ModText::GetLanguageSetting()
{
	int aValue = MODLANG_AUTO;
	if (gSexyAppBase != NULL)
		gSexyAppBase->RegistryReadInteger("ModLanguage", &aValue);
	if (aValue != MODLANG_AUTO && aValue != MODLANG_CHINESE && aValue != MODLANG_ENGLISH)
		aValue = MODLANG_AUTO;
	return aValue;
}

void ModText::SetLanguageSetting(int theLang)
{
	if (gSexyAppBase != NULL)
		gSexyAppBase->RegistryWriteInteger("ModLanguage", theLang);
}

int ModText::GetLanguage()
{
	int aSetting = GetLanguageSetting();
	if (aSetting == MODLANG_AUTO)
		return (GetACP() == 936) ? MODLANG_CHINESE : MODLANG_ENGLISH;
	return aSetting;
}

bool ModText::IsChinese()
{
	return GetLanguage() == MODLANG_CHINESE;
}

const char* ModText::Tr(const char* theZhUtf8, const char* theEnUtf8)
{
	return IsChinese() ? theZhUtf8 : theEnUtf8;
}

std::wstring ModText::WideFromUtf8(const char* theUtf8)
{
	std::wstring aWide;
	if (theUtf8 == NULL || theUtf8[0] == '\0') return aWide;
	int aLength = MultiByteToWideChar(CP_UTF8, 0, theUtf8, -1, NULL, 0);
	if (aLength <= 1) return aWide;
	aWide.resize((size_t)aLength - 1);		// 去掉 -1 口径带上的结尾 NUL
	MultiByteToWideChar(CP_UTF8, 0, theUtf8, -1, &aWide[0], aLength);
	return aWide;
}

// ── 宽字符字体 ─────────────────────────────────────────────────────────
// 三档回退沿用小助手时代的惯例：雅黑 → 黑体 → 宋体；都找不到也照样建（GDI 会替一支）。
// 与旧路线的区别只在 charset：宽字符直绘不挑码页（DEFAULT_CHARSET），非中文系统也照画。

static const wchar_t* ModTextCjkFace()
{
	static const wchar_t* aFace = NULL;
	if (aFace == NULL)
	{
		aFace = L"Microsoft YaHei";
		if (GetFileAttributesW(L"C:\\Windows\\Fonts\\msyh.ttc") == INVALID_FILE_ATTRIBUTES)
		{
			aFace = (GetFileAttributesW(L"C:\\Windows\\Fonts\\simhei.ttf") != INVALID_FILE_ATTRIBUTES)
				? L"SimHei" : L"SimSun";
		}
	}
	return aFace;
}

static const int MODTEXT_FONT_CACHE = 16;

ModText::Font* ModText::GetFont(int thePointSize, bool theBold)
{
	static ModText::Font* sFonts[MODTEXT_FONT_CACHE] = { NULL };
	static int sPointSizes[MODTEXT_FONT_CACHE] = { 0 };
	static bool sBolds[MODTEXT_FONT_CACHE] = { false };
	static int sCount = 0;

	for (int i = 0; i < sCount; i++)
	{
		if (sPointSizes[i] == thePointSize && sBolds[i] == theBold)
			return sFonts[i];
	}

	ModText::Font* aFont = new ModText::Font();
	aFont->mHeight = 0;
	aFont->mAscent = 0;

	HDC aDC = ::GetDC(gSexyAppBase->mHWnd);

	LOGFONTW aLogFont = { 0 };
	aLogFont.lfHeight = -MulDiv(thePointSize, GetDeviceCaps(aDC, LOGPIXELSY), 72);
	aLogFont.lfWeight = theBold ? FW_BOLD : FW_NORMAL;
	aLogFont.lfCharSet = DEFAULT_CHARSET;
	aLogFont.lfQuality = ANTIALIASED_QUALITY;
	aLogFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	wcscpy_s(aLogFont.lfFaceName, LF_FACESIZE, ModTextCjkFace());
	aFont->mHFont = CreateFontIndirectW(&aLogFont);

	TEXTMETRICW aMetrics;
	HFONT anOldFont = (HFONT)SelectObject(aDC, aFont->mHFont);
	GetTextMetricsW(aDC, &aMetrics);
	SelectObject(aDC, anOldFont);
	::ReleaseDC(gSexyAppBase->mHWnd, aDC);

	aFont->mHeight = aMetrics.tmHeight;
	aFont->mAscent = aMetrics.tmAscent;

	if (sCount < MODTEXT_FONT_CACHE)
	{
		sFonts[sCount] = aFont;
		sPointSizes[sCount] = thePointSize;
		sBolds[sCount] = theBold;
		sCount++;
	}
	return aFont;
}

int ModText::LineHeight(Font* theFont)
{
	return (theFont != NULL) ? theFont->mHeight : 0;
}

int ModText::Ascent(Font* theFont)
{
	return (theFont != NULL) ? theFont->mAscent : 0;
}

int ModText::TextWidth(Font* theFont, const std::wstring& theText)
{
	if (theFont == NULL || theText.empty()) return 0;
	HDC aDC = ::GetDC(gSexyAppBase->mHWnd);
	HFONT anOldFont = (HFONT)SelectObject(aDC, theFont->mHFont);
	SIZE aSize = { 0, 0 };
	GetTextExtentPoint32W(aDC, theText.c_str(), (int)theText.size(), &aSize);
	SelectObject(aDC, anOldFont);
	::ReleaseDC(gSexyAppBase->mHWnd, aDC);
	return (int)aSize.cx;
}

// ── 绘制 ───────────────────────────────────────────────────────────────
// 与 SysFont::DrawString 同构的两条路（DD 表面直画 / 其它目标先画 DIB 再贴），
// 只把 TextOutA 换 TextOutW，且 (theX, theY) 是文本左上角。

void ModText::DeleteNonSurfaceDataForDraw(DDImage* theImage)
{
	if (theImage != NULL)
		theImage->DeleteAllNonSurfaceData();
}

void ModText::DrawTextWide(Graphics* g, Font* theFont, int theX, int theY,
	const std::wstring& theText, const Color& theColor, const Rect& theClipRect)
{
	if (theFont == NULL || theText.empty()) return;

	DDImage* aDDImage = dynamic_cast<DDImage*>(g->mDestImage);
	if (aDDImage != NULL)
	{
		LPDIRECTDRAWSURFACE aSurface = aDDImage->GetSurface();
		if (aSurface != NULL)
		{
			HDC aDC;

			if (aDDImage->mLockCount > 0)
				aDDImage->mSurface->Unlock(NULL);

			if ((g->mDestImage == gSexyAppBase->mWidgetManager->mImage) && (gSexyAppBase->Is3DAccelerated()))
				gSexyAppBase->mDDInterface->mD3DInterface->Flush();

			if (aSurface->GetDC(&aDC) == DD_OK)
			{
				HFONT anOldFont = (HFONT)SelectObject(aDC, theFont->mHFont);
				SetBkMode(aDC, TRANSPARENT);
				IntersectClipRect(aDC, theClipRect.mX, theClipRect.mY,
					theClipRect.mX + theClipRect.mWidth, theClipRect.mY + theClipRect.mHeight);
				SetTextColor(aDC, RGB(theColor.GetRed(), theColor.GetGreen(), theColor.GetBlue()));
				TextOutW(aDC, theX + g->mTransX, theY + g->mTransY, theText.c_str(), (int)theText.size());
				SelectObject(aDC, anOldFont);
				aSurface->ReleaseDC(aDC);
				DeleteNonSurfaceDataForDraw(aDDImage);
			}

			if (aDDImage->mLockCount > 0)
				aDDImage->mSurface->Lock(NULL, &aDDImage->mLockedSurfaceDesc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
		}
	}
	else if (g->mDestImage != &Graphics::mStaticImage)	// 别对着静态图模子画（同 SysFont）
	{
		HDC aDC = CreateCompatibleDC(NULL);
		HFONT anOldFont = (HFONT)SelectObject(aDC, theFont->mHFont);

		int aWidth = TextWidth(theFont, theText);
		int aHeight = theFont->mHeight;
		if (aWidth <= 0 || aHeight <= 0)
		{
			SelectObject(aDC, anOldFont);
			DeleteDC(aDC);
			return;
		}

		BITMAPINFOHEADER aHeader;
		memset(&aHeader, 0, sizeof(aHeader));
		aHeader.biPlanes = 1;
		aHeader.biWidth = aWidth;
		aHeader.biHeight = -aHeight;
		aHeader.biCompression = BI_RGB;
		aHeader.biBitCount = 32;
		aHeader.biSize = sizeof(BITMAPINFOHEADER);

		ulong* aWhiteBits;
		ulong* aBlackBits;
		HBITMAP aWhiteBitmap = (HBITMAP)CreateDIBSection(aDC, (BITMAPINFO*)&aHeader, DIB_RGB_COLORS, (void**)&aWhiteBits, NULL, 0);
		HBITMAP aBlackBitmap = (HBITMAP)CreateDIBSection(aDC, (BITMAPINFO*)&aHeader, DIB_RGB_COLORS, (void**)&aBlackBits, NULL, 0);
		RECT aRect = { 0, 0, aWidth, aHeight };

		// 白底、黑底各画一遍，再逐像素反算覆盖率与颜色（照 SysFont 的合成法）。
#define MODTEXT_DRAW_BITMAP(bmp, brush)															\
		{																						\
			HBITMAP anOldBmp = (HBITMAP)SelectObject(aDC, bmp);									\
			FillRect(aDC, &aRect, brush);														\
			SetBkMode(aDC, TRANSPARENT);														\
			SetTextColor(aDC, RGB(theColor.GetRed(), theColor.GetGreen(), theColor.GetBlue()));	\
			TextOutW(aDC, 0, 0, theText.c_str(), (int)theText.size());							\
			SelectObject(aDC, anOldBmp);														\
		}

		MODTEXT_DRAW_BITMAP(aWhiteBitmap, (HBRUSH)GetStockObject(WHITE_BRUSH));
		MODTEXT_DRAW_BITMAP(aBlackBitmap, (HBRUSH)GetStockObject(BLACK_BRUSH));

		SelectObject(aDC, anOldFont);

		MemoryImage aTempImage;
		aTempImage.Create(aWidth, aHeight);

		int aCount = aHeight * aWidth;
		ulong* aPtr1 = aWhiteBits;
		ulong* aPtr2 = aBlackBits;
		while (aCount > 0)
		{
			if (*aPtr1 == *aPtr2)
			{
				*aPtr1 |= 0xFF000000;
			}
			else if ((*aPtr1 & 0xFFFFFF) != 0xFFFFFF || (*aPtr2 & 0xFFFFFF) != 0x000000)
			{
				int ba = 255 + (*aPtr2 & 0xFF) - (*aPtr1 & 0xFF);
				int ga = 255 + ((*aPtr2 >> 8) & 0xFF) - ((*aPtr1 >> 8) & 0xFF);
				int ra = 255 + ((*aPtr2 >> 16) & 0xFF) - ((*aPtr1 >> 16) & 0xFF);
				int aBlue = (ba != 0) ? 255 * (int)(*aPtr2 & 0xFF) / ba : 0;
				int aGreen = (ga != 0) ? 255 * (int)((*aPtr2 >> 8) & 0xFF) / ga : 0;
				int aRed = (ra != 0) ? 255 * (int)((*aPtr2 >> 16) & 0xFF) / ra : 0;
				int anAlpha = std::min(ra, std::min(ga, ba));
				*aPtr1 = (aBlue) | (aGreen << 8) | (aRed << 16) | (anAlpha << 24);
			}
			else
			{
				*aPtr1 = 0;
			}

			aPtr1++;
			aPtr2++;
			--aCount;
		}

		memcpy(aTempImage.GetBits(), aWhiteBits, aWidth * aHeight * sizeof(ulong));
		g->DrawImage(&aTempImage, theX, theY);

		DeleteObject(aWhiteBitmap);
		DeleteObject(aBlackBitmap);
		DeleteDC(aDC);
	}
}
