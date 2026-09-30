#include "BassLoader.h"
#include <stdlib.h>

using namespace Sexy;

BASS_INSTANCE* Sexy::gBass = NULL;
static long gBassLoadCount = 0;

///////////////////////////////////////////////////////////////////////////////
// BASS 2.1 compatibility: wrap the plural-attribute / narrow-arg exports behind
// the 2.4-style members (the Steam GOTY re-release ships a 2.1 bass.dll)
///////////////////////////////////////////////////////////////////////////////
typedef BOOL (WINAPI *BASS21_SetAttributesProc)(DWORD handle, int freq, int volume, int pan);
typedef BOOL (WINAPI *BASS21_GetAttributesProc)(DWORD handle, int* freq, int* volume, int* pan);
typedef BOOL (WINAPI *BASS21_SlideAttributesProc)(DWORD handle, int freq, int volume, int pan, DWORD time);
typedef BOOL (WINAPI *BASS21_InitProc)(DWORD freq, DWORD flags, HWND win);
typedef HMUSIC (WINAPI *BASS21_MusicLoadProc)(BOOL mem, const void* file, DWORD offset, DWORD length, DWORD flags, DWORD freq);
typedef HSTREAM (WINAPI *BASS21_StreamCreateFileProc)(BOOL mem, const void* file, DWORD offset, DWORD length, DWORD flags);
typedef BOOL (WINAPI *BASS21_ChannelSetPositionProc)(DWORD handle, DWORD pos);
typedef DWORD (WINAPI *BASS21_ChannelGetPositionProc)(DWORD handle);
typedef BOOL (WINAPI *BASS21_MusicSetAmplifyProc)(HMUSIC handle, DWORD amp);

static BASS21_SetAttributesProc      gBass21_SetAttributes = NULL;
static BASS21_GetAttributesProc      gBass21_GetAttributes = NULL;
static BASS21_SlideAttributesProc    gBass21_SlideAttributes = NULL;
static BASS21_InitProc               gBass21_Init = NULL;
static BASS21_MusicLoadProc          gBass21_MusicLoad = NULL;
static BASS21_StreamCreateFileProc   gBass21_StreamCreateFile = NULL;
static BASS21_ChannelSetPositionProc gBass21_ChannelSetPosition = NULL;
static BASS21_ChannelGetPositionProc gBass21_ChannelGetPosition = NULL;
static BASS21_MusicSetAmplifyProc    gBass21_MusicSetAmplify = NULL;

static int Bass21_VolTo0100(float theValue)
{
	int aVol = (int)(theValue * 100.0f + 0.5f);
	if (aVol < 0) aVol = 0;
	if (aVol > 100) aVol = 100;
	return aVol;
}

static BOOL WINAPI Bass21_ChannelSetAttribute(DWORD handle, DWORD attrib, float value)
{
	if (attrib != BASS_ATTRIB_VOL || gBass21_SetAttributes == NULL)
		return FALSE;
	// -1 freq / -101 pan = leave unchanged in BASS 2.1
	return gBass21_SetAttributes(handle, -1, Bass21_VolTo0100(value), -101);
}

static BOOL WINAPI Bass21_ChannelGetAttribute(DWORD handle, DWORD attrib, float* value)
{
	if (attrib != BASS_ATTRIB_VOL || gBass21_GetAttributes == NULL || value == NULL)
		return FALSE;
	int aFreq = -1, aVol = 0, aPan = -101;
	if (!gBass21_GetAttributes(handle, &aFreq, &aVol, &aPan))
		return FALSE;
	*value = aVol / 100.0f;
	return TRUE;
}

static BOOL WINAPI Bass21_ChannelSlideAttribute(DWORD handle, DWORD attrib, float value, DWORD time)
{
	if (attrib != BASS_ATTRIB_VOL || gBass21_SlideAttributes == NULL)
		return FALSE;
	return gBass21_SlideAttributes(handle, -1, Bass21_VolTo0100(value), -101, time);
}

static BOOL WINAPI Bass21_Init(int device, DWORD freq, DWORD flags, HWND win, GUID* clsid)
{
	(void)device; (void)clsid; // 2.1 has no device selector / clsid
	return gBass21_Init(freq, flags, win);
}

static HMUSIC WINAPI Bass21_MusicLoad(BOOL mem, const void* file, QWORD offset, DWORD length, DWORD flags, DWORD freq)
{
	return gBass21_MusicLoad(mem, file, (DWORD)offset, length, flags, freq);
}

static HSTREAM WINAPI Bass21_StreamCreateFile(BOOL mem, const void* file, QWORD offset, QWORD length, DWORD flags)
{
	return gBass21_StreamCreateFile(mem, file, (DWORD)offset, (DWORD)length, flags);
}

static BOOL WINAPI Bass21_ChannelSetPosition(DWORD handle, QWORD pos, DWORD mode)
{
	(void)mode; // 2.1 has no mode param; pos encodes bytes or a music order directly
	return gBass21_ChannelSetPosition(handle, (DWORD)pos);
}

static QWORD WINAPI Bass21_ChannelGetPosition(DWORD handle, DWORD mode)
{
	(void)mode;
	return gBass21_ChannelGetPosition(handle);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static void CheckBassFunction(unsigned int theFunc, const char *theName)
{
	if (theFunc==0)
	{
		char aBuf[1024];
		sprintf(aBuf,"%s function not found in bass.dll",theName);
		MessageBoxA(NULL,aBuf,"Error",MB_OK | MB_ICONERROR);
		exit(0);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
BASS_INSTANCE::BASS_INSTANCE(const char *dllName)
{
    mModule = LoadLibrary(dllName);
	if (!mModule)
		return;

#define GETPROC(_x) CheckBassFunction(*((uintptr_t *)&_x) = (uintptr_t)GetProcAddress(mModule, #_x),#_x)    

	GETPROC(BASS_Init);
	GETPROC(BASS_Free);
	GETPROC(BASS_Stop);
	GETPROC(BASS_Start);
	
	//*((uintptr_t*) &BASS_SetGlobalVolumes) = (uintptr_t) GetProcAddress(mModule, "BASS_SetGlobalVolumes");
	*((uintptr_t*) &BASS_SetVolume) = (uintptr_t) GetProcAddress(mModule, "BASS_SetVolume");

	if (BASS_SetVolume == NULL /*&& (BASS_SetGlobalVolumes == NULL)*/)
	{
		MessageBoxA(NULL,"Whoops! You forgot to put the CD in your computer.","Error",MB_OK | MB_ICONERROR);
		exit(0);
	}

	//*((uintptr_t*) &BASS_SetConfig) = (uintptr_t) GetProcAddress(mModule, "BASS_SetConfig");
	//*((uintptr_t*) &BASS_GetConfig) = (uintptr_t) GetProcAddress(mModule, "BASS_GetConfig");
	GETPROC(BASS_SetConfig);
	GETPROC(BASS_GetConfig);

	GETPROC(BASS_GetVolume);
	GETPROC(BASS_GetInfo);

	GETPROC(BASS_GetVersion);
	// the 2.4-style attribute API is optional: BASS 2.1 (as shipped with the Steam GOTY
	// re-release) only exports the plural variants, resolved as fallbacks below
	*((uintptr_t*) &BASS_ChannelSetAttribute) = (uintptr_t) GetProcAddress(mModule, "BASS_ChannelSetAttribute");
	*((uintptr_t*) &BASS_ChannelGetAttribute) = (uintptr_t) GetProcAddress(mModule, "BASS_ChannelGetAttribute");
	*((uintptr_t*) &BASS_ChannelFlags) = (uintptr_t) GetProcAddress(mModule, "BASS_ChannelFlags");
	*((uintptr_t*) &BASS_ChannelSlideAttribute) = (uintptr_t) GetProcAddress(mModule, "BASS_ChannelSlideAttribute");
	GETPROC(BASS_ChannelStop);
	GETPROC(BASS_ChannelPlay);
	GETPROC(BASS_ChannelPause);
	GETPROC(BASS_ChannelSetPosition);
	GETPROC(BASS_ChannelGetPosition);
	GETPROC(BASS_ChannelIsActive);
	GETPROC(BASS_ChannelIsSliding);
	GETPROC(BASS_ChannelGetLevel);	
	GETPROC(BASS_ChannelSetSync);
	GETPROC(BASS_ChannelRemoveSync);
	GETPROC(BASS_ChannelGetData);

	// supported by BASS 1.1 and higher. Only work if the user has DX8 or higher though.
	GETPROC(BASS_FXSetParameters);
	GETPROC(BASS_FXGetParameters);
	GETPROC(BASS_ChannelSetFX);
	GETPROC(BASS_ChannelRemoveFX);

	GETPROC(BASS_MusicLoad);
	GETPROC(BASS_MusicFree);
	//GETPROC(BASS_MusicGetAttribute);
	//GETPROC(BASS_MusicSetAttribute);

	GETPROC(BASS_StreamCreateFile);
	GETPROC(BASS_StreamFree);

	//GETPROC(BASS_MusicGetOrders);
	//GETPROC(BASS_MusicGetOrderPosition);

	GETPROC(BASS_SampleLoad);
	GETPROC(BASS_SampleFree);
	GETPROC(BASS_SampleSetInfo);
	GETPROC(BASS_SampleGetInfo);
	GETPROC(BASS_SampleGetChannel);
	GETPROC(BASS_SampleStop);

	GETPROC(BASS_ErrorGetCode);

	// BASS 2.1 wiring: swap the members whose 2.4 signatures differ for 2.1 thunks
	if (BASS_ChannelSetAttribute == NULL)
	{
		gBass21_SetAttributes         = (BASS21_SetAttributesProc)      GetProcAddress(mModule, "BASS_ChannelSetAttributes");
		gBass21_GetAttributes         = (BASS21_GetAttributesProc)      GetProcAddress(mModule, "BASS_ChannelGetAttributes");
		gBass21_SlideAttributes       = (BASS21_SlideAttributesProc)    GetProcAddress(mModule, "BASS_ChannelSlideAttributes");
		gBass21_Init                  = (BASS21_InitProc)               GetProcAddress(mModule, "BASS_Init");
		gBass21_MusicLoad             = (BASS21_MusicLoadProc)          GetProcAddress(mModule, "BASS_MusicLoad");
		gBass21_StreamCreateFile      = (BASS21_StreamCreateFileProc)   GetProcAddress(mModule, "BASS_StreamCreateFile");
		gBass21_ChannelSetPosition    = (BASS21_ChannelSetPositionProc) GetProcAddress(mModule, "BASS_ChannelSetPosition");
		gBass21_ChannelGetPosition    = (BASS21_ChannelGetPositionProc) GetProcAddress(mModule, "BASS_ChannelGetPosition");
		gBass21_MusicSetAmplify       = (BASS21_MusicSetAmplifyProc)    GetProcAddress(mModule, "BASS_MusicSetAmplify");

		if (gBass21_SetAttributes != NULL)
			*((uintptr_t*) &BASS_ChannelSetAttribute) = (uintptr_t) &Bass21_ChannelSetAttribute;
		if (gBass21_GetAttributes != NULL)
			*((uintptr_t*) &BASS_ChannelGetAttribute) = (uintptr_t) &Bass21_ChannelGetAttribute;
		if (gBass21_SlideAttributes != NULL)
			*((uintptr_t*) &BASS_ChannelSlideAttribute) = (uintptr_t) &Bass21_ChannelSlideAttribute;
		if (gBass21_Init != NULL)
			*((uintptr_t*) &BASS_Init) = (uintptr_t) &Bass21_Init;
		if (gBass21_MusicLoad != NULL)
			*((uintptr_t*) &BASS_MusicLoad) = (uintptr_t) &Bass21_MusicLoad;
		if (gBass21_StreamCreateFile != NULL)
			*((uintptr_t*) &BASS_StreamCreateFile) = (uintptr_t) &Bass21_StreamCreateFile;
		if (gBass21_ChannelSetPosition != NULL)
			*((uintptr_t*) &BASS_ChannelSetPosition) = (uintptr_t) &Bass21_ChannelSetPosition;
		if (gBass21_ChannelGetPosition != NULL)
			*((uintptr_t*) &BASS_ChannelGetPosition) = (uintptr_t) &Bass21_ChannelGetPosition;
		// same signature, BASS 2.1 just spells it differently
		*((uintptr_t*) &BASS_ChannelFlags) = (uintptr_t) GetProcAddress(mModule, "BASS_ChannelSetFlags");
	}

	GETPROC(BASS_PluginLoad);
	GETPROC(BASS_ChannelGetLength);

	// The following are only supported in 2.2 and higher
	//*((uintptr_t*) &BASS_PluginLoad) = (uintptr_t) GetProcAddress(mModule, "BASS_PluginLoad");
	//*((uintptr_t*) &BASS_ChannelGetLength) = (uintptr_t) GetProcAddress(mModule, "BASS_ChannelGetLength");

	/*
	mVersion2 = BASS_SetConfig != NULL;
	if (mVersion2)
	{
		// Version 2 has different BASS_Init params
		*((uintptr_t*) &BASS_Init2) = (uintptr_t) BASS_Init;
		BASS_Init = NULL;

		*((uintptr_t*) &BASS_MusicLoad2) = (uintptr_t) BASS_MusicLoad;
		BASS_MusicLoad = NULL;

		

		// 2.1 and higher only
		*((uintptr_t*) &BASS_ChannelPreBuf) = (uintptr_t) GetProcAddress(mModule, "BASS_ChannelPreBuf");
	}
	else
	{
		BASS_PluginLoad = NULL;
		BASS_ChannelPreBuf = NULL;
	}
	*/

#undef GETPROC
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
BASS_INSTANCE::~BASS_INSTANCE()
{
    if (mModule)
        FreeLibrary(mModule);
}


BOOL BASS_INSTANCE::BASS_MusicSetAmplify(HMUSIC handle, DWORD amp)
{
	// BASS 2.1 exposes this as its own export instead of a channel attribute
	if (gBass21_MusicSetAmplify != NULL)
		return gBass21_MusicSetAmplify(handle, amp);
	BASS_ChannelSetAttribute(handle, BASS_ATTRIB_MUSIC_AMPLIFY, amp);
	return true;
}


BOOL BASS_INSTANCE::BASS_MusicPlay(HMUSIC handle)
{
	return BASS_ChannelPlay(handle, true);
}


BOOL BASS_INSTANCE::BASS_MusicPlayEx(HMUSIC handle, DWORD pos, int flags, BOOL reset)
{
	(void)reset;
	//int anOffset = MAKEMUSICPOS(pos,0);

	BASS_ChannelStop(handle);
	BASS_ChannelSetPosition(handle, MAKELONG(pos,0), BASS_POS_MUSIC_ORDER);
	BASS_ChannelFlags(handle, flags, -1);

	return BASS_ChannelPlay(handle, false/*reset*/); // What's wrong with actually using the reset flag?
}


BOOL BASS_INSTANCE::BASS_ChannelResume(DWORD handle)
{
	return BASS_ChannelPlay(handle, false);
}

BOOL BASS_INSTANCE::BASS_StreamPlay(HSTREAM handle, BOOL flush, DWORD flags)
{
	BASS_ChannelFlags(handle, flags, -1);
	return BASS_ChannelPlay(handle, flush);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Sexy::LoadBassDLL()
{
	InterlockedIncrement(&gBassLoadCount);
	if (gBass!=NULL)
		return;

	gBass = new BASS_INSTANCE("bass.dll");
	if (gBass->mModule==NULL)
	{
		MessageBoxA(NULL,"Can't find bass.dll." ,"Error",MB_OK | MB_ICONERROR);
		exit(0);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Sexy::FreeBassDLL()
{
	if (gBass!=NULL)
	{
		if (InterlockedDecrement(&gBassLoadCount) <= 0)
		{
			delete gBass;
			gBass = NULL;
		}
	}
}


