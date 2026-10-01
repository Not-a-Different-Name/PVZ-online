#include "LawnApp.h"
#include "Resources.h"
#include "Sexy.TodLib/TodStringFile.h"
#include <crtdbg.h>
#include <string.h>
using namespace Sexy;

bool (*gAppCloseRequest)();				//[0x69E6A0]
bool (*gAppHasUsedCheatKeys)();			//[0x69E6A4]
SexyString (*gGetCurrentLevelName)();

// @pvz-online debug: heap-corruption hunting. CRT asserts always go to stderr (no modal
// dialogs). PVZ_HEAPCHECK=1 validates the whole heap on every allocation — ~100x slower
// and quadratic as the heap grows, so the loading screen never finishes; PVZ_HEAPCHECK=late
// skips that and instead walks the heap periodically from LawnApp::UpdateApp.
static void PvzDebugHeapInit()
{
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);

	char aHeapCheck[16] = {0};
	if (GetEnvironmentVariableA("PVZ_HEAPCHECK", aHeapCheck, 15) > 0)
	{
		if (_stricmp(aHeapCheck, "late") == 0)
			_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
		else
			_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
	}
}

//0x44E8F0
int WINAPI WinMain(_In_ HINSTANCE /* hInstance */, _In_opt_ HINSTANCE /* hPrevInstance */, _In_ LPSTR /* lpCmdLine */, _In_ int /* nCmdShow */)
{
	// @pvz-online: the game ships no manifest, so on a scaled display (125% here) Windows
	// treats it as DPI-unaware: it creates an 800x600 *logical* window but gives it a
	// 1000x750 *physical* client and upsamples our 800x600 framebuffer by 1.25x. That is
	// where the mystery 1000x750 came from - nothing in the engine ever sets that size
	// (DDInterface's widescreen resize needs mEnableWindowAspect, which is false
	// everywhere), and GetDpiForWindow still answers 96 because the process is unaware.
	// Declaring awareness before any window exists keeps the client at exactly
	// mWidth x mHeight, so the framebuffer maps 1:1 and the art stops being resampled.
	// Scaling only - this never affects the windowed/fullscreen decision.
	// (Bound at runtime: the SDK headers hide SetProcessDPIAware behind WINVER >= 0x0600,
	// and this project still targets the Win98-era version the original was built for.)
	typedef BOOL (WINAPI *SetProcessDPIAwareFn)();
	if (HMODULE aUser32 = ::LoadLibraryA("user32.dll"))
	{
		if (SetProcessDPIAwareFn aSetDpiAware = (SetProcessDPIAwareFn)::GetProcAddress(aUser32, "SetProcessDPIAware"))
			aSetDpiAware();
	}

	// @pvz-online: build stamp - the first stderr line identifies the exact binary behind
	// every test log. Reads the exe's own link timestamp, so it is fresh on EVERY build
	// (unlike __DATE__, which only changes when main.cpp itself is recompiled).
	{
		char aExePath[MAX_PATH];
		if (GetModuleFileNameA(nullptr, aExePath, MAX_PATH))
		{
			WIN32_FILE_ATTRIBUTE_DATA aFileData;
			if (GetFileAttributesExA(aExePath, GetFileExInfoStandard, &aFileData))
			{
				FILETIME aLocalTime;
				FileTimeToLocalFileTime(&aFileData.ftLastWriteTime, &aLocalTime);
				SYSTEMTIME aLinkTime;
				FileTimeToSystemTime(&aLocalTime, &aLinkTime);
				fprintf(stderr, "[build] exe linked %04d-%02d-%02d %02d:%02d:%02d\n",
					aLinkTime.wYear, aLinkTime.wMonth, aLinkTime.wDay,
					aLinkTime.wHour, aLinkTime.wMinute, aLinkTime.wSecond);
			}
		}
	}

	PvzDebugHeapInit();
	TodStringListSetColors(gLawnStringFormats, gLawnStringFormatCount);
	gGetCurrentLevelName = LawnGetCurrentLevelName;
	gAppCloseRequest = LawnGetCloseRequest;
	gAppHasUsedCheatKeys = LawnHasUsedCheatKeys;
	gExtractResourcesByName = Sexy::ExtractResourcesByName;
	gLawnApp = new LawnApp();
	gLawnApp->mChangeDirTo = (!Sexy::FileExists("properties\\resources.xml") && Sexy::FileExists("..\\properties\\resources.xml")) ? ".." : ".";
	gLawnApp->Init();
	gLawnApp->Start();
	gLawnApp->Shutdown();
	if (gLawnApp)
		delete gLawnApp;

	return 0;
};
