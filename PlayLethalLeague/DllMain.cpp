// PlayLethalLeague.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Game.h"
#include "LLNeural.h"
#include <detours.h>
#include <io.h>
#include <Fcntl.h>
#include "AllCaves.h"

#define USE_NEURAL

#define PRINT_BEGIN(NAME) auto& t = g.NAME;
#define PRINT_VAR(NAME, VAR) LOG("  - " << NAME << ": " << (static_cast<int>(t.VAR)));

typedef char(_cdecl* ORIGINAL_FUNCTION)(void* _this, int param2);
ORIGINAL_FUNCTION originalFunction;

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
	if (DetourIsHelperProcess())
		return TRUE;

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

char FrameHook(void* _origThis, int t2)
{
	LOG("Frame hook");
	return originalFunction(_origThis, t2);
}

int PlayLethalLeagueMain()
{
	LOG("Press enter to continue.");
	getchar();

	std::shared_ptr<InputUpdatePattern> inputPattern = std::make_shared<InputUpdatePattern>();
	auto g = injectedGameInstance = new Game();
	g->patternScans.push_back(inputPattern);

	LOG("Initializing offset code caves...");
	if (!g->performCodeCaves())
	{
		LOG("Unable to perform one of the code caves.");
		return 0;
	}
	g->clearCaves();
	LOG("Done performing all code caves...");

	g->setInputsEnabled(true);

	LOG("Setting up frame function detour...");
	DetourTransactionBegin();
	originalFunction = (ORIGINAL_FUNCTION)inputPattern->getFoundLocation();
	DetourAttach(reinterpret_cast<void**>(&originalFunction), FrameHook);
	auto error = DetourTransactionCommit();
	if (error == NO_ERROR) {
		LOG("Detour successfully complete!");
	}
	else {
		LOG("Detour failed: " << error);
	}

	inputPattern.reset();

#if 0
#ifdef USE_NEURAL
	LLNeural neural(&g);
#endif

	bool wasInGame = false;
	bool playedOneFrame = true;
	while (true)
	{
		// make sure we are only overwriting inputs for our player
		g.gameData->inputsForcePlayers[0] = 0x01;
		g.updateInputs();

#ifdef PRINT_VALUES
		Sleep(2000);
#else
		// Really minimal sleep here
		Sleep(5);
#endif

		g.checkResetOffsets();

		bool isInGame = g.gameData->ball_base && g.gameData->ball_state && g.gameData->ball_coord;

		if (isInGame != wasInGame)
		{
			LOG("=== Entered a new Match ===");
#ifdef USE_NEURAL
			if (playedOneFrame)
				neural.newMatchStarted();
#endif
			playedOneFrame = false;
		}
		if (!isInGame && wasInGame)
		{
			g.resetInputs();
		}
		wasInGame = isInGame;

		if (!isInGame)
			continue;

#ifdef PRINT_VALUES
		for (auto i = 0; i < 4; i++)
		{
			/*
			LOG("-> plyr " << i);
			LOG("   - base:  " << g.offsetStorage->player_bases[i]);
			LOG("   - coord: " << g.offsetStorage->player_coords[i]);
			LOG("   - state: " << g.offsetStorage->player_states[i]);
			*/
			if (!g.offsetStorage->player_bases[i])
				continue;

			LOG("== player " << i << " state ==");
			LOG("max_speed: " << g.players[i].entity.max_speed);
			LOG("charge_length: " << g.players[i].entity.charge_length);
			LOG("lives: " << g.players[i].state.lives);
			LOG("hit_counter: " << g.players[i].state.total_hit_counter);
			LOG("bunt_counter: " << g.players[i].state.bunt_counter);
			LOG("x: " << g.players[i].coords.xcoord);
			LOG("y: " << g.players[i].coords.ycoord);
		}
#endif

		if (!g.gameData->dev_base->windowActive)
			g.setInputsEnabled(true);

#ifdef PRINT_VALUES
		if (g.offsetStorage->ball_state != nullptr)
		{
			LOG("== ball state ==");
			PRINT_BEGIN(localBallState);
			PRINT_VAR("hitstun", hitstun);
			PRINT_VAR("exists", ballExists);
			PRINT_VAR("xspeed", xspeed);
			PRINT_VAR("yspeed", yspeed);
			PRINT_VAR("stunCooldown", hitstunCooldown);
			PRINT_VAR("speed", ballSpeed);
			PRINT_VAR("tag", ballTag);
			PRINT_VAR("direction", direction);
			PRINT_VAR("hitCount", hitCount);
		}

		if (g.offsetStorage->ball_coord != nullptr)
		{
			LOG("== ball coord ==");
			PRINT_BEGIN(localBallCoords);
			PRINT_VAR("x", xcoord);
			PRINT_VAR("y", ycoord);
		}
#endif

#ifdef USE_NEURAL
		neural.playOneFrame();
#endif
	}

	LOG("Done.");
	getchar();
#endif

    return 0;
}

