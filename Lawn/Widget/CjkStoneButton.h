#ifndef __CJKSTONEBUTTON_H__
#define __CJKSTONEBUTTON_H__

#include "GameButton.h"

// @pvz-online: 石材按钮 + 宽字符标签（语言批 2026-10-03 从 OnlineStartDialog.cpp 提取——
// 联机面板的按钮也要用同一种画法，两边各写一份迟早画歪）。
//
// 为什么要有它：原版 DrawStoneButton 把标签字体写死成位图字体（main.pak 里的
// DwarvenTodCraft 系只到 Latin），画不了汉字。这里照抄它的画法（贴图平铺、按下位移、
// 居中），中文档把标签绘制换成 ModText 宽字符直绘（UTF-8 → UTF-16 → TextOutW，与系统
// 码页脱钩）；英文档原样回落到 LawnStoneButton::Draw——位图字体那套绿字内嵌样式是烤进
// 贴图的，比宽字符更贴原版。
//
// 标签是 UTF-8 原样（SetLabel 收的也是原样字节），绘制时才现转宽字符。

// 标签落到石材按钮上用多大（像素高）。20 与联机弹窗正文同档。
#define CJK_BUTTON_LABEL_PX 20

// 点值反算：ModText::GetFont 会把点值按屏幕 DPI 折算成像素，写死点值在 150% 缩放
// （144 DPI）下会被放大出近 2 倍。这里按"目标像素高"反着折，让落地像素与 DPI 无关。
int		CjkPointSize(int thePixelHeight);

// 石材按钮是"左端贴图 + 中段贴图 × n + 右端贴图"平铺画的（见 CjkStoneButton::Draw），
// 宽度必须正好是这三段的和；随手给个宽度的话平铺铺不满，画出来的石头比控件窄一截，
// 标签按控件宽居中就会整体偏右（字越多越显得贴边）。原版 LawnDialog::Resize 也做同款
// 取整："不足中部贴图宽度的部分补充至中部贴图宽度"。
int		CjkStoneButtonWidth(int theWidth, bool theRoundUp);

class CjkStoneButton : public LawnStoneButton
{
public:
	CjkStoneButton(int theId, ButtonListener* theListener) : LawnStoneButton(nullptr, theId, theListener) { }

	virtual void	Draw(Graphics* g);
};

#endif
