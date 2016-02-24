#include "Game.h"
#include <io.h>
#include <Fcntl.h>
#include <Shlwapi.h>
#include <PythonEngine.h>
#include "Utils.h"

#define USE_NEURAL

#define PRINT_BEGIN(NAME) auto& t = g.NAME;
#define PRINT_VAR(NAME, VAR) LOG("  - " << NAME << ": " << (static_cast<int>(t.VAR)));

// dummy function, please ignore
_declspec(dllexport) int testInjectedPlayLL()
{
	return 1;
}

int PlayLethalLeagueMain();
static BOOL WINAPI InjectedConsoleCtrlHandler(DWORD dwctrl)
{
	return false;
}

int oldStdin;
int oldStdout;
int oldStderr;

#define ENABLE_REDIRECT_STDINOUT
#define OPEN_MODE _O_TEXT
void InitInjectedConsole()
{
	AllocConsole();
	SetConsoleTitle("PlayLethalLeague");
	SetConsoleCtrlHandler(InjectedConsoleCtrlHandler, TRUE);
	SetConsoleOutputCP(CP_UTF8);  // also this is needed when you want to use UTF8

#ifdef ENABLE_REDIRECT_STDINOUT
	const int in = _open_osfhandle(INT_PTR(GetStdHandle(STD_INPUT_HANDLE)), OPEN_MODE);
	const int out = _open_osfhandle(INT_PTR(GetStdHandle(STD_OUTPUT_HANDLE)), OPEN_MODE);
	const int err = _open_osfhandle(INT_PTR(GetStdHandle(STD_ERROR_HANDLE)), OPEN_MODE);
	oldStdin = in;
	oldStdout = out;
	oldStderr = err;
	*stdin = *_fdopen(in, "r");
	*stdout = *_fdopen(out, "w");
	*stderr = *_fdopen(err, "w");

	// Redirect the CRT standard input, output, and error handles to the console
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
#endif

	//Clear the error state for each of the C++ standard stream objects. We need to do this, as
	//attempts to access the standard streams before they refer to a valid target will cause the
	//iostream objects to enter an error state. In versions of Visual Studio after 2005, this seems
	//to always occur during startup regardless of whether anything has been read from or written to
	//the console or not.
	std::wcout.clear();
	std::cout.clear();
	std::wcerr.clear();
	std::cerr.clear();
	std::wcin.clear();
	std::cin.clear();
}

DWORD WINAPI RealInjectedMain(LPVOID lpParam)
{
	PlayLethalLeagueMain();
	return 0;
}

#define USE_DLL_THREAD
// BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
HINSTANCE injectedModuleHandle;
BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
#ifdef USE_DLL_THREAD
	switch(ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		injectedModuleHandle = hModule;
		// DisableThreadLibraryCalls(hModule);
		DWORD dwThreadId;
		HANDLE hThread = CreateThread(
			NULL, 0, RealInjectedMain, NULL, 0, &dwThreadId);
		break;
	}
#endif
	return TRUE;
}

Game* injectedGameInstance;
int PlayLethalLeagueMain()
{
	InitInjectedConsole();
	LOG("Initializing python interpreter...");
	PythonEngine::initializePython();

	LOG("Python initialization done.");

	std::string rootPath;
	std::string scriptsPath;
	{
		char pathToModule[MAX_PATH];
		size_t pathLength = GetModuleFileName(injectedModuleHandle, pathToModule, MAX_PATH);
		if (pathLength == 0)
		{
			LOG("Unable to get path to current module!");
			return 0;
		}

		LOG("Path to module: " << pathToModule);
		// remove filename and dirname
		PathRemoveFileSpec(pathToModule);
		PathRemoveFileSpec(pathToModule);
		rootPath = std::string(pathToModule);
		scriptsPath = rootPath + std::string("\\scripts\\");
		LOG("Scripts path: " << scriptsPath.c_str());
	}

	auto g = injectedGameInstance = new Game(scriptsPath);
	LOG("Instantiated game...");

	LOG("Loading python code...");
	if (!g->python->loadPythonCode())
	{
		LOG("Unable to load python code.");
		LOG("The bot will have no brain!");
	}

	LOG("Initializing offset code caves...");
	if (!g->performCodeCaves())
	{
		LOG("Unable to perform one of the code caves.");
		return 0;
	}
	g->clearCaves();
	LOG("Done performing all code caves...");

	while (g && g->python)
	{
		getchar();
		LOG("Attempting to reload python code...");
		g->python->reloadPythonCode();
	}

    return 0;
}

