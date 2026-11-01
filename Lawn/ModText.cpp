#include "Lawn/ModText.h"
#include "Lawn/Observer/ObserverDrawScene.h"
#include "SexyAppBase.h"
#include "graphics/Graphics.h"
#include "graphics/DDImage.h"
#include "graphics/DDInterface.h"
#include "graphics/D3DInterface.h"
#include "graphics/MemoryImage.h"
#include "widget/WidgetManager.h"
#include <algorithm>
#include <mutex>

using namespace Sexy;

// 不透明字体（只在本文件里成型）：一支 GDI 宽字符字体 + 基本度量。
class ModText::Font
{
public:
	HFONT	mHFont;
    int mPointSize=16; bool mBold=false;
	int		mHeight;
	int		mAscent;
};

// ── 语言档 ─────────────────────────────────────────────────────────────
// 存注册表（HKCU 直通，见下 Reg* 助手；键名 "ModLanguage"；CurUser/ModSlideCollect 同路）。
// 读失败保留默认 MODLANG_AUTO——首次运行没有这个值，此时按系统码页自动判。

// ── 注册表直通 ─────────────────────────────────────────────────────────
// SexyAppBase::{Read,Write}Registry 的键前缀 = "SOFTWARE\" + 属性 "RegistryKey"；
// 本工程 properties 没有这个属性 → mRegKey 恒空 → 链上所有读写直接失败（2026-10-13
// 实机排障：ModLanguage/ModSlideCollect/CurUser 全都存不下来）。这里不唤醒框架链，
// 直接读写 HKCU\SOFTWARE\PopCap\PlantsVsZombies（即框架本意的键位）。
static const char* MOD_REG_PATH = "SOFTWARE\\PopCap\\PlantsVsZombies";

// 注：不用 RegGetValueA——winreg.h 只在目标 >= Vista 时才声明它，本工程目标平台更老。
bool ModText::RegReadDword(const char* theName, int* theValue)
{
	HKEY aKey;
	if (RegOpenKeyExA(HKEY_CURRENT_USER, MOD_REG_PATH, 0, KEY_QUERY_VALUE, &aKey) != ERROR_SUCCESS)
		return false;
	DWORD aType = 0, aData = 0, aSize = sizeof(aData);
	LSTATUS aResult = RegQueryValueExA(aKey, theName, NULL, &aType, (LPBYTE)&aData, &aSize);
	RegCloseKey(aKey);
	if (aResult != ERROR_SUCCESS || aType != REG_DWORD || aSize != sizeof(aData))
		return false;
	*theValue = (int)aData;
	return true;
}

bool ModText::RegWriteDword(const char* theName, int theValue)
{
	HKEY aKey;
	if (RegCreateKeyExA(HKEY_CURRENT_USER, MOD_REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE, NULL, &aKey, NULL) != ERROR_SUCCESS)
		return false;
	DWORD aData = (DWORD)theValue;
	LSTATUS aResult = RegSetValueExA(aKey, theName, 0, REG_DWORD, (const BYTE*)&aData, sizeof(aData));
	RegCloseKey(aKey);
	return aResult == ERROR_SUCCESS;
}

bool ModText::RegReadString(const char* theName, std::string* theValue)
{
	HKEY aKey;
	if (RegOpenKeyExA(HKEY_CURRENT_USER, MOD_REG_PATH, 0, KEY_QUERY_VALUE, &aKey) != ERROR_SUCCESS)
		return false;
	char aBuf[256];
	DWORD aType = 0, aSize = sizeof(aBuf);
	LSTATUS aResult = RegQueryValueExA(aKey, theName, NULL, &aType, (LPBYTE)aBuf, &aSize);
	RegCloseKey(aKey);
	if (aResult != ERROR_SUCCESS || (aType != REG_SZ && aType != REG_EXPAND_SZ) || aSize == 0)
		return false;
	theValue->assign(aBuf, aSize - 1);	// 去掉结尾 NUL
	return true;
}

bool ModText::RegWriteString(const char* theName, const std::string& theValue)
{
	HKEY aKey;
	if (RegCreateKeyExA(HKEY_CURRENT_USER, MOD_REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE, NULL, &aKey, NULL) != ERROR_SUCCESS)
		return false;
	LSTATUS aResult = RegSetValueExA(aKey, theName, 0, REG_SZ, (const BYTE*)theValue.c_str(), (DWORD)theValue.size() + 1);
	RegCloseKey(aKey);
	return aResult == ERROR_SUCCESS;
}

namespace {
struct LanguageCache {
 std::mutex mutex;
 int value=ModText::MODLANG_AUTO;
 DWORD checked=0;
 bool valid=false;
};
LanguageCache& CachedLanguage(){static LanguageCache cache;return cache;}
}

int ModText::GetLanguageSetting()
{
 // Translating the live roster must not open/query/close HKCU for each label.
 // Keep cross-process preference changes visible, with at most 250 ms delay.
 auto& cache=CachedLanguage();std::lock_guard<std::mutex> lock(cache.mutex);
 const DWORD now=GetTickCount();
 if(!cache.valid||DWORD(now-cache.checked)>=250){
  int value=MODLANG_AUTO;RegReadDword("ModLanguage",&value);
  if(value!=MODLANG_AUTO&&value!=MODLANG_CHINESE&&value!=MODLANG_ENGLISH)value=MODLANG_AUTO;
  cache.value=value;cache.checked=now;cache.valid=true;
 }
 return cache.value;
}

void ModText::SetLanguageSetting(int theLang)
{
 auto& cache=CachedLanguage();std::lock_guard<std::mutex> lock(cache.mutex);
 RegWriteDword("ModLanguage",theLang);
 // Always re-read after the write: a failed save must not report a new setting.
 cache.valid=false;
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
    aFont->mPointSize=thePointSize;aFont->mBold=theBold;

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
    if(auto* capture=dynamic_cast<Observer::DrawRecorder*>(g->mDestImage)) {
        const int count=WideCharToMultiByte(CP_UTF8,0,theText.data(),int(theText.size()),nullptr,0,nullptr,nullptr);
        if(count<0||count>1024){capture->Fail();return;}
        std::string utf8(count,'\0');
        WideCharToMultiByte(CP_UTF8,0,theText.data(),int(theText.size()),utf8.data(),count,nullptr,nullptr);
        capture->Text(utf8,theX+int(g->mTransX),theY+int(g->mTransY),theFont->mPointSize,theFont->mBold,theColor,theClipRect);
        return;
    }


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

// ── 中文断行与整块绘制（2026-10-14 从 RunPickDialog.cpp 提取）────────────
// 原实现见 2026-10-03 语言批的 RunPickDialog：中文说明在那一屏首次成套出现，
// 断行系统随之定型（测量与绘制共用一份，杜绝叠字/出栏）。僵尸强化投票框成为第二个
// 消费方，遂收进这里——两份实现迟早画歪。

namespace
{
// 折行时的最小单元：中文一字一个；空格独立成单元（英文在空格处断行）；英文单词
// 不打散——「0.75/stack」「20%」整着走，「×0.75」的 × 归到后一个词里一起走
// （2026-10-03 玩家截图里「0.75」被拆成两行是旧口径的毛病）。
// 2026-10-04：旧口径把"连续 ASCII"当一个单元——纯英文句子整句都是 ASCII，成了一句
// 放不下的巨型单元、被硬放进位（wrap 里"一个单元比整列还宽：硬放"），英文档的说明
// 就是这么横穿邻列叠在一起的。
int BreakUnit(const std::wstring& theText, int theIndex)
{
	wchar_t aChar = theText[theIndex];
	if (aChar == L' ') return 1;
	if (aChar >= 0x80 && aChar != 0x00D7) return 1;	// ×（0xD7）例外：随后面的词走
	int anEnd = theIndex;
	while (anEnd < (int)theText.size())
	{
		wchar_t aCh = theText[anEnd];
		if (aCh == L' ') break;
		if (aCh >= 0x80 && aCh != 0x00D7) break;
		anEnd++;
	}
	return anEnd - theIndex;
}

// 不能起行的字（收尾标点）；断行时把它们留在上一行
bool NoLineStart(const std::wstring& theText, int theIndex)
{
	static const wchar_t* aPuncts[] = { L"、",L"，",L"。",L"！",L"？",L"：",L"；",L"）",L"】",L"》",L"”",L"’",L"%",L"…" };
	std::wstring aUnit = theText.substr(theIndex, 1);
	for (int i = 0; i < (int)(sizeof(aPuncts) / sizeof(aPuncts[0])); i++)
	{
		if (aUnit == aPuncts[i]) return true;
	}
	return false;
}
}

void ModText::WrapLines(Font* theFont, const std::wstring& theText, int theWidth,
	std::vector<std::wstring>& theLines)
{
	if (theWidth <= 0) theWidth = 1;
	int aStart = 0;
	while (true)
	{
		int aBreak = (int)theText.find(L'\n', aStart);
		if (aBreak < 0) aBreak = (int)theText.size();
		std::wstring aPara = theText.substr(aStart, aBreak - aStart);
		int aPos = 0;
		while (aPos < (int)aPara.size())
		{
			int aFit = aPos;
			int aCursor = aPos;
			while (aCursor < (int)aPara.size())
			{
				int aNext = aCursor + BreakUnit(aPara, aCursor);
				if (aFit > aPos && TextWidth(theFont, aPara.substr(aPos, aNext - aPos)) > theWidth) break;
				aFit = aNext;
				aCursor = aNext;
			}
			if (aFit == aPos) aFit = aPos + BreakUnit(aPara, aPos); // 一个单元比整列还宽：硬放
			while (aFit < (int)aPara.size() && NoLineStart(aPara, aFit))
			{
				aFit += BreakUnit(aPara, aFit);
			}
			theLines.push_back(aPara.substr(aPos, aFit - aPos));
			aPos = aFit;
		}
		if (aBreak >= (int)theText.size()) break;
		aStart = aBreak + 1;
	}
}

int ModText::WrapLineCount(Font* theFont, const char* theUtf8, int theWidth)
{
	std::vector<std::wstring> aLines;
	WrapLines(theFont, WideFromUtf8(theUtf8), theWidth, aLines);
	return (int)aLines.size() > 0 ? (int)aLines.size() : 1;
}

void ModText::DrawWrappedCjk(Graphics* g, Font* theFont, const Rect& theRect,
	const char* theUtf8, const Color& theColor)
{
	if (theRect.mWidth <= 0) return;

	std::vector<std::wstring> aLines;
	WrapLines(theFont, WideFromUtf8(theUtf8), theRect.mWidth, aLines);

	int aLineHeight = LineHeight(theFont) + 3;
	int aY = theRect.mY + (theRect.mHeight - (int)aLines.size() * aLineHeight) / 2;
	if (aY < theRect.mY) aY = theRect.mY;
	for (int i = 0; i < (int)aLines.size(); i++)
	{
		int aX = theRect.mX + (theRect.mWidth - TextWidth(theFont, aLines[i])) / 2;
		if (aX < theRect.mX) aX = theRect.mX;
		// 原 DrawString 是基线口径（y + Ascent 才是顶），这里收顶对齐：
		// 行的顶就是 aY + i * aLineHeight
		DrawTextWide(g, theFont, aX, aY + i * aLineHeight, aLines[i], theColor, g->mClipRect);
	}
}
