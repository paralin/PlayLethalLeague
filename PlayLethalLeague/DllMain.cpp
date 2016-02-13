// PlayLethalLeague.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Game.h"
#include <io.h>
#include <Fcntl.h>

#define USE_NEURAL

#define PRINT_BEGIN(NAME) auto& t = g.NAME;
#define PRINT_VAR(NAME, VAR) LOG("  - " << NAME << ": " << (static_cast<int>(t.VAR)));

int PlayLethalLeagueMain();
static BOOL WINAPI InjectedConsoleCtrlHandler(DWORD dwctrl)
{
	return false;
}

int oldStdin;
int oldStdout;

void InitInjectedConsole()
{
	AllocConsole();
	SetConsoleCtrlHandler(InjectedConsoleCtrlHandler, TRUE);
	// RemoveMenu(GetSystemMenu(GetConsoleWindow(), FALSE), SC_CLOSE, MF_BYCOMMAND);
	const int in = _open_osfhandle(INT_PTR(GetStdHandle(STD_INPUT_HANDLE)), _O_TEXT);
	const int out = _open_osfhandle(INT_PTR(GetStdHandle(STD_OUTPUT_HANDLE)), _O_TEXT);
	oldStdin = in;
	oldStdout = out;
	*stdin = *_fdopen(in, "r");
	*stdout = *_fdopen(out, "w");
	//freopen("CONOUT$", "w", stdout);
	//freopen("CONOUT$", "r", stdout);

	// Redirect the CRT standard input, output, and error handles to the console
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

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
	InitInjectedConsole();
	LOG("Allocated console!");
	PlayLethalLeagueMain();
	return 0;
}

// BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// DisableThreadLibraryCalls(hModule);
		DWORD dwThreadId;
		HANDLE hThread = CreateThread(
			NULL, 0, RealInjectedMain, NULL, 0, &dwThreadId);
		break;
	}
	return TRUE;
}

Game* injectedGameInstance;
int PlayLethalLeagueMain()
{
	LOG("Press enter to continue.");
	// getchar();

	auto g = injectedGameInstance = new Game();

	LOG("Initializing offset code caves...");
	if (!g->performCodeCaves())
	{
		LOG("Unable to perform one of the code caves.");
		return 0;
	}
	g->clearCaves();
	LOG("Done performing all code caves...");

    return 0;
}

