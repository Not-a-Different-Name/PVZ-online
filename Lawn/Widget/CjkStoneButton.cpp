#include "CjkStoneButton.h"
#include "../ModText.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "graphics/Graphics.h"

int CjkPointSize(int thePixelHeight)
{
	HDC aDC = ::GetDC(gSexyAppBase->mHWnd);
	int aDpi = GetDeviceCaps(aDC, LOGPIXELSY);
	::ReleaseDC(gSexyAppBase->mHWnd, aDC);
	if (aDpi <= 0) aDpi = 96;
	int aPointSize = (thePixelHeight * 72 + aDpi / 2) / aDpi;
	return aPointSize < 1 ? 1 : aPointSize;
}

int CjkStoneButtonWidth(int theWidth, bool theRoundUp)
{
	int aMid = Sexy::IMAGE_BUTTON_MIDDLE->mWidth;
	int aMin = Sexy::IMAGE_BUTTON_LEFT->mWidth + Sexy::IMAGE_BUTTON_RIGHT->mWidth;
	int anExtra = theWidth - aMin;
	if (anExtra < 0)
	{
		anExtra = 0;
	}
	else if (aMid > 0)
	{
		int aRemainder = anExtra % aMid;
		if (aRemainder != 0)
		{
			if (theRoundUp) anExtra += aMid - aRemainder;
			else anExtra -= aRemainder;
		}
	}
	int aWidth = aMin + anExtra;
	int aFloor = aMin + aMid;   // 至少带一个中段：光两块端头拼不成石头
	return aWidth < aFloor ? aFloor : aWidth;
}

// 标签是不是纯 ASCII。英文档回落到位图字体是有前提的：那张字体表只到 Latin，
// "Easy ×0.5" 里的 × 没有对应字形，硬走位图会画成乱码——含非 ASCII 的标签两种
// 语言都得走宽字符直绘。
static bool LabelIsAscii(const std::string& theLabel)
{
	for (size_t i = 0; i < theLabel.size(); i++)
	{
		if ((unsigned char)theLabel[i] >= 0x80) return false;
	}
	return true;
}

void CjkStoneButton::Draw(Graphics* g)
{
	if (mBtnNoDraw) return;

	// 英文档 + 纯 ASCII 标签：位图字体自带英文，回落到原版画法（字色与基线都是贴图/字体烤好的）
	if (!ModText::IsChinese() && LabelIsAscii(mLabel))
	{
		LawnStoneButton::Draw(g);
		return;
	}

	bool aDown = (mIsDown && mIsOver && !mDisabled) ^ mInverted;
	Image* aLeftImage = aDown ? Sexy::IMAGE_BUTTON_DOWN_LEFT : Sexy::IMAGE_BUTTON_LEFT;
	Image* aMiddleImage = aDown ? Sexy::IMAGE_BUTTON_DOWN_MIDDLE : Sexy::IMAGE_BUTTON_MIDDLE;
	Image* aRightImage = aDown ? Sexy::IMAGE_BUTTON_DOWN_RIGHT : Sexy::IMAGE_BUTTON_RIGHT;

	int aFontX = 0;
	int aFontY = 0;
	int aImageX = 0;
	if (aDown)
	{
		aFontX++;
		aFontY++;
		aImageX++;
	}

	int aRepeat = (mWidth - aLeftImage->mWidth - aRightImage->mWidth) / aMiddleImage->mWidth;
	g->DrawImage(aLeftImage, aImageX, 0);
	aImageX += aLeftImage->mWidth;
	while (aRepeat > 0)
	{
		g->DrawImage(aMiddleImage, aImageX, 0);
		aImageX += aMiddleImage->mWidth;
		--aRepeat;
	}
	g->DrawImage(aRightImage, aImageX, 0);

	ModText::Font* aFont = ModText::GetFont(CjkPointSize(CJK_BUTTON_LABEL_PX), false);
	std::wstring aLabel = ModText::WideFromUtf8(mLabel.c_str());
	aFontX += (mWidth - ModText::TextWidth(aFont, aLabel)) / 2;
	aFontY += (mHeight - ModText::LineHeight(aFont)) / 2;
	ModText::DrawTextWide(g, aFont, aFontX, aFontY, aLabel,
		mIsOver ? Color(0x9B, 0xF0, 0x60) : Color(0x2F, 0x6B, 0x2B), g->mClipRect);
}
