#ifndef __NEWOPTIONSDIALOG_H__
#define __NEWOPTIONSDIALOG_H__

#include "widget/Dialog.h"
#include "widget/SliderListener.h"
#include "widget/CheckboxListener.h"

class LawnApp;
class LawnStoneButton;
class NewLawnButton;
namespace Sexy
{
	class Slider;
	class Checkbox;
};

class NewOptionsDialog : public Sexy::Dialog, public Sexy::SliderListener, public Sexy::CheckboxListener
{
protected:
	enum
	{
		NewOptionsDialog_Almanac,
		NewOptionsDialog_MainMenu,
		NewOptionsDialog_Restart,
		NewOptionsDialog_Update,
		NewOptionsDialog_MusicVolume,
		NewOptionsDialog_SoundVolume,
		NewOptionsDialog_Fullscreen,
		NewOptionsDialog_HardwareAcceleration,
	};

public:
	LawnApp*				mApp;								//+0x158
	Sexy::Slider*			mMusicVolumeSlider;					//+0x15C
	Sexy::Slider*			mSfxVolumeSlider;					//+0x160
	Sexy::Checkbox*			mFullscreenCheckbox;				//+0x164
	Sexy::Checkbox*			mHardwareAccelerationCheckbox;		//+0x168
	LawnStoneButton*		mAlmanacButton;						//+0x16C
	LawnStoneButton*		mBackToMainButton;					//+0x170
	LawnStoneButton*		mRestartButton;						//+0x174
	NewLawnButton*			mBackToGameButton;					//+0x178
	bool					mFromGameSelector;					//+0x17C
	// @pvz-online: 局内词条查看器（2026-10-03 用户定案）：不另起对话框类，就在这个暂停面板上
	// 盖一层整屏覆盖——避开嵌套消息循环/焦点/析构三件麻烦。开着时八个控件整体藏起，
	// 关掉按原样还原（mRunInfoWidgetVis 是藏之前的快照）。
	bool					mRunInfoOpen;
	bool					mRunInfoWidgetVis[8];

public:
	NewOptionsDialog(LawnApp* theApp, bool theFromGameSelector);
	~NewOptionsDialog();

	int						GetPreferredHeight(int theWidth);
	void					AddedToManager(Sexy::WidgetManager* theWidgetManager);
	void					RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	void					Resize(int theX, int theY, int theWidth, int theHeight);
	void					Draw(Sexy::Graphics* g);
	void					SliderVal(int theId, double theVal);
	void					CheckboxChecked(int theId, bool checked);
	void					ButtonPress(int theId);
	void					ButtonDepress(int theId);
	void					KeyDown(Sexy::KeyCode theKey);
	void					MouseDown(int x, int y, int theClickCount);

	// 词条查看器（见 .cpp 同名分节）：只在闯关局可用，覆盖层的开关与绘制都收在这几个口里。
	bool					RunInfoAvailable();
	Sexy::Rect				RunInfoEntryRect();
	void					OpenRunInfo();
	void					CloseRunInfo();
	void					DrawRunInfo(Sexy::Graphics* g);
};

#endif
