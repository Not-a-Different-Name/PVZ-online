#ifndef __MODTEXT_H__
#define __MODTEXT_H__

#include <string>
#include "graphics/Color.h"
#include "graphics/Graphics.h"
#include "misc/Rect.h"

namespace Sexy
{
	class DDImage;
}

// @pvz-online: 语言与中文文本基建（2026-10-03 语言选项批）。
// 背景：main.pak 的位图字体只到 Latin；mod 的中文文案此前在六个源文件里各自走
// 「UTF-8 → 本机 ANSI + SysFont(TextOutA)」——非 CP936 系统上汉字会被 WideCharToMultiByte
// 烂成 '?'。这里收成一份、并升级为**宽字符直绘**（UTF-8 → UTF-16 → TextOutW）：
// 与系统码页彻底脱钩，英文系统上也能正常显示中文。语言档（自动 / 中文 / English）
// 存注册表（SexyAppBase 的 Registry 助手，键名 "ModLanguage"），语言=English 时
// 文案调度由调用方走英文、不必过这里——这里是"画中文"的基建。
namespace ModText
{
	enum
	{
		// 别用 LANG_CHINESE / LANG_ENGLISH 等短名：winnt.h 里就是宏（0x04/0x09），
		// 一旦链进 windows.h 就会被预处理器替换掉。
		MODLANG_AUTO = 0,		// 按系统码页：936 → 中文，其它 → English
		MODLANG_CHINESE = 1,
		MODLANG_ENGLISH = 2
	};

	int			GetLanguageSetting();		// 注册表里的原始档（取不到 = MODLANG_AUTO）
	void		SetLanguageSetting(int theLang);
	int			GetLanguage();				// 解析 AUTO 后的实际语言
	bool		IsChinese();

	// 双语择串：按当前语言在两条 UTF-8 里选一条返回（不做拷贝，两条都得是静态存储——
	// 字面量或静态表，别传栈上的缓冲）。不画字的场景（标题、提示、按钮标签）也能用。
	const char*	Tr(const char* theZhUtf8, const char* theEnUtf8);

	// DDImage::DeleteAllNonSurfaceData 只 friend 了 SysFont 又要照调（surface 像素被
	// GDI 改过、缓存的派生位作废），这里留一座桥；friend 声明在 DDImage.h 里。
	void		DeleteNonSurfaceDataForDraw(Sexy::DDImage* theImage);

	// UTF-8（源码字面量的形式）→ UTF-16。
	std::wstring	WideFromUtf8(const char* theUtf8);

	// 宽字符字体：按“点值”取（DPI 语义同 SysFont::Init）、进程级缓存、故意不释放
	// （字号档位是有限的几个，先例同各调用点此前的 static 字体缓存）。
	class Font;
	Font*		GetFont(int thePointSize, bool theBold);

	int			LineHeight(Font* theFont);
	int			Ascent(Font* theFont);		// 基线 = 顶 + Ascent（对齐旧 DrawString 口径时用）
	int			TextWidth(Font* theFont, const std::wstring& theText);

	// 在 (theX, theY) 画一行，(theX, theY) = 文本左上角（SysFont::DrawString 是基线
	// 口径，这里顶对齐，调用方不用再减 ascent）。theClipRect 只在 DD 路径生效（同 SysFont）。
	// 名字叫 Wide 后缀：wingdi.h 会把 DrawText 宏替换成 DrawTextA/W，定义处会炸。
	void		DrawTextWide(Sexy::Graphics* g, Font* theFont, int theX, int theY,
					const std::wstring& theText, const Sexy::Color& theColor, const Sexy::Rect& theClipRect);
}

#endif
