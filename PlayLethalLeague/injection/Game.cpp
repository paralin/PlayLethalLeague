#include "Game.h"
#include <sys/stat.h>
#include <caves/AllCaves.h>
#include <Tlhelp32.h>
#include <cassert>
#include "PatternScanner.h"

void __stdcall callHookedFrameTick(Game* g)
{
	g->hookedFrameTick();
}

void (__stdcall* ptr_to_callHookedFrameTick)(Game* g) = callHookedFrameTick;

#define FREE_HANDLE(thing) if (thing != nullptr) {CloseHandle(thing);} thing = nullptr;
#define REGISTER_CODECAVE(CLASS) caves.push_back(std::make_shared<CLASS>());
#define REGISTER_PATTSCAN(CLASS) patternScans.push_back(std::make_shared<CLASS>());

#define ZERO_STRUCT(stru) ZeroMemory(&stru, sizeof(stru));
Game::Game(std::string scriptsRoot) : 
	gameHandle(GetCurrentProcess()), 
	processId(0) ,
	wasInGame(false),
	reloadingPythonCode(false)
{
	gameData = static_cast<GameStorage*>(VirtualAlloc(nullptr, sizeof(GameStorage), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	memset(gameData, 0, sizeof(GameStorage));
	gameData->frameHookAddress = ptr_to_callHookedFrameTick;

	python = std::make_shared<PythonEngine>(this, scriptsRoot);

	printedHookSuccessful = false;

	LOG("Location of storage: " << std::hex << gameData);

	REGISTER_CODECAVE(DevCave); // OK
	REGISTER_CODECAVE(BallCave); // OK
	// REGISTER_CODECAVE(GameRulesCave);
	REGISTER_CODECAVE(StageCave);
	REGISTER_CODECAVE(PlayerCave);

	/*
	// REGISTER_CODECAVE(PlayerSpawnCave);
	// REGISTER_CODECAVE(WindowRefocusCave);
	*/

	REGISTER_CODECAVE(WindowUnfocusCave);
	REGISTER_CODECAVE(InputPressedCave);
	REGISTER_CODECAVE(InputHeldCave);
	REGISTER_CODECAVE(ResetCave);
	REGISTER_CODECAVE(StartOfFrameCave);
	// REGISTER_CODECAVE(DeathCave);
	REGISTER_CODECAVE(Reset2Cave);
	REGISTER_CODECAVE(OfflineInputsCave);
	REGISTER_CODECAVE(OnlineInputsCave);
}

void Game::resetBall() const
{
	GameStorage* gd = gameData;
	if (!gd->ball_state)
		return;

	gd->ball_state->state = 14;
	gd->ball_state->serveResetCounter = 300000;
}

void Game::respawnPlayer(int i) const
{
	GameStorage* gd = gameData;
	if (!gd->player_states[i])
		return;

	gd->player_states[i]->special_meter = 0;
	gd->player_states[i]->character_state = 3;
	gd->player_states[i]->respawn_timer = 200000;
}


void Game::hookedFrameTick()
{
	if (!printedHookSuccessful)
	{
		LOG("Hooked frame tick successfully.");
		printedHookSuccessful = true;
	}

	setInputsEnabled(true);
	checkResetOffsets();
	updateInputs();

	bool isInGame = gameData->ball_base && gameData->ball_state && gameData->ball_coord && gameData->stage_base;

	// Kinda freeze the game until the python reload is done
  while (isInGame && reloadingPythonCode)
  {
    Sleep(100);
    wasInGame = false;
  }

	if (isInGame != wasInGame && isInGame)
	{
		LOG("=== Entered a new Match ===");
		python->newMatchStarted();
	}
	if (!isInGame && wasInGame) {
		python->matchReset();
		resetInputs();
	}
	wasInGame = isInGame;

	if (!isInGame)
		return;

	// process sticky inputs
	for (auto px = 0; px < 4; px++)
	{
		// reference variable (important)
		char inputSticky = gameData->inputsSavedSticky[px];
		// 2 bytes for this one
		const char inputNow = gameData->inputsSaved[px * 2];
		bool wasAttacking = (inputSticky & CONTROL_ATTACK) || (inputNow & CONTROL_ATTACK);
		bool wasBunting = (inputSticky & CONTROL_BUNT) || (inputNow & CONTROL_BUNT);

		// HACK: AI players do NOT record the attack or bunt keys.
		// Temporary fix: assume we're playing with p1 as an AI, and record attack key if he is in attack anim
		if (px == 1)
		{
			char animState = gameData->player_states[px]->animation_state;
			if (!wasAttacking)
			{
				switch (animState)
				{
				case 1:
				case 4:
					wasAttacking = true;
					break;
				}
			}
			if (animState >= 16 && animState <= 19)
				wasBunting = true;
		}

		inputSticky = inputNow;
		if (wasAttacking)
			inputSticky |= CONTROL_ATTACK;
		if (wasBunting)
			inputSticky |= CONTROL_BUNT;
		gameData->inputsSavedSticky[px] = inputSticky;
	}

	python->playOneFrame();
}


bool Game::locateExecutable()
{
	executablePath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\lethalleague\\LethalLeague.exe";
	steamPath = "C:\\Program Files (x86)\\Steam\\Steam.exe";
	// Check Steam location
	struct stat buffer;
	return (stat(executablePath.c_str(), &buffer) == 0) && (stat(executablePath.c_str(), &buffer) == 0);
}

#define ZEROOFF(IDENT) memset(&gameData->IDENT, 0, sizeof(gameData->IDENT));
void Game::checkResetOffsets() const
{
	if (gameData->do_reset)
	{
		LOG("do_reset bit flipped to 1, resetting game state.");
		ZEROOFF(do_reset);
		ZEROOFF(ball_base);
		ZEROOFF(ball_coord);
		ZEROOFF(ball_state);
		ZEROOFF(stage_base);
		ZEROOFF(player_bases);
		ZEROOFF(currentPlayer);
		ZEROOFF(player_coords);
		ZEROOFF(player_states);
		ZEROOFF(player_spawn);
		ZEROOFF(inputsSaved);
		ZEROOFF(inputsSavedSticky);
		ZEROOFF(isOnline);
	}
}

MODULEENTRY32 GetModuleInfo()
{
	void* hSnap = nullptr;
	MODULEENTRY32 Mod32 = { 0 };

	if ((hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId())) == INVALID_HANDLE_VALUE)
		return Mod32;

	Mod32.dwSize = sizeof(MODULEENTRY32);
	while (Module32Next(hSnap, &Mod32))
	{
		if (!_stricmp("lethalleague.exe", Mod32.szModule))
		{
			CloseHandle(hSnap);
			return Mod32;
		}
	}

	CloseHandle(hSnap);
	return{ 0 };
}

void DoSuspendOtherThreads(std::vector<DWORD>& suspended)
{
	HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (h != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32 te;
		te.dwSize = sizeof(te);
		if (Thread32First(h, &te))
		{
			do
			{
				if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID))
				{
					// Suspend all threads EXCEPT the one we want to keep running
					if (te.th32ThreadID != GetCurrentThreadId() && te.th32OwnerProcessID == GetCurrentProcessId())
					{
						HANDLE thread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
						if (thread != NULL)
						{
							suspended.push_back(te.th32ThreadID);
							SuspendThread(thread);
							CloseHandle(thread);
						}
					}
				}
				te.dwSize = sizeof(te);
			} while (Thread32Next(h, &te));
		}
		CloseHandle(h);
	}
}

void ResumeOtherThreads(std::vector<DWORD>& suspended)
{
	for (auto susp : suspended)
	{
		HANDLE thread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, susp);
		if (thread != NULL)
		{
			ResumeThread(thread);
			CloseHandle(thread);
		}
	}
}

#define LIMIT_TO_MODULE

bool Game::performCodeCaves()
{
	std::vector<std::shared_ptr<CodeCaveScan>> scans;
	// Allocate the space
	for (auto& c : caves)
	{
		size_t scanSize;
		unsigned char* pat = c->getScanPattern(&scanSize);
		if (pat != nullptr)
		{
			auto scn = std::make_shared<CodeCaveScan>();
			scn->cave = c;
			scn->pattern = pat;
			scn->patternSize = scanSize;
			scn->name = c->getCodeCaveName();
			scans.push_back(scn);
		}
	}

	for (auto& c : patternScans)
	{
		size_t scanSize;
		unsigned char* pat = c->getScanPattern(&scanSize);
		if (pat != nullptr)
		{
			auto scn = std::make_shared<CodeCaveScan>();
			scn->pattern = pat;
			scn->patternSize = scanSize;
			scn->patterns = c;
			scn->name = c->getCodeCaveName();
			scans.push_back(scn);
		}
	}

	if (scans.empty())
		return true;

	MODULEENTRY32 llmod = GetModuleInfo();
	if (llmod.modBaseAddr == nullptr)
	{
		LOG("Unable to find module.");
		return false;
	}

	LOG("Suspending other threads...");
	std::vector<DWORD> suspended;
	DoSuspendOtherThreads(suspended);
	LOG("Other threads suspended.")

		LOG("Starting searches...");
	PatternScanner::search(llmod.modBaseAddr, llmod.modBaseSize, scans);
	LOG("Searches complete, performing injections...");

	for (auto it = scans.begin(); it != scans.end(); ++it)
	{
		const auto& t = it->get();
		if (t->foundAddress)
		{
			if (t->cave)
				performCodeCave(reinterpret_cast<intptr_t>(t->foundAddress), t->cave.get());
			else if (t->patterns)
				t->patterns->setFoundLocation(t->foundAddress);
			free(t->pattern);
		}
	}

	LOG("Press enter to continue game.");
	getchar();
	ResumeOtherThreads(suspended);

	return true;
}

void Game::performCodeCave(intptr_t injectLoc, CodeCave* cav)
{
	DWORD oldProtect;
	VirtualProtect(reinterpret_cast<LPVOID>(injectLoc), cav->overwrittenInstructionSize(), PAGE_EXECUTE_READWRITE, &oldProtect);
	bool doCallOrig = cav->shouldPrependOriginal();
	size_t codeSize;
	size_t codeSizeNoJmp;
	unsigned char* ren = cav->render(this, injectLoc, &codeSize, doCallOrig ? cav->overwrittenInstructionSize() : 0);

	if (ren == NULL)
	{
		LOG("Removing matched instructions at location " << injectLoc);
		// We just want to overwrite the instruction with nop
		memset((void*)injectLoc, 0x90, cav->overwrittenInstructionSize());
		VirtualProtect(reinterpret_cast<LPVOID>(injectLoc), cav->overwrittenInstructionSize(), PAGE_EXECUTE_READ, &oldProtect);
		return;
	}

	LOG("Performing code cave at injection location " << injectLoc);

	// Read the original instructions
	if (doCallOrig)
	{
		memcpy(ren, (LPVOID)injectLoc, cav->overwrittenInstructionSize());
		LOG("Original instructions (keeping):");
		for (auto i = 0u; i < cav->overwrittenInstructionSize(); i++)
		{
			std::cout << std::hex << static_cast<int>(ren[i]) << " ";
		}
		std::cout << std::endl;
	}


	// Now we need to make the return jump
	// add 24 (twice what we need) bytes to be safe
	codeSizeNoJmp = codeSize;
	codeSize += 5;
	LPVOID cavLoc = VirtualAlloc(NULL, codeSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	LOG("Location of cave: " << std::hex << (reinterpret_cast<intptr_t>(cavLoc)) << std::dec);

	// Adjust if its a call
	if (ren[0] == 0xE8)
	{
		intptr_t* origPtr = reinterpret_cast<intptr_t*>(ren + 1);
		// subtract the offset from origPtr and caveLocation
		*origPtr -= reinterpret_cast<intptr_t>(cavLoc) - injectLoc;
	}

	// Now that we know where the code starts
	// 5 bytes, 1 byte for E9 + 32 bit address
	size_t jmpSize = 1 + 4;
	// calculate the offset
	intptr_t targetLoc = injectLoc + cav->overwrittenInstructionSize();
	intptr_t jmpLoc = reinterpret_cast<intptr_t>(cavLoc) + codeSizeNoJmp + jmpSize;
	intptr_t jmpOffset = targetLoc - jmpLoc;

	unsigned char* jmpBuf = static_cast<unsigned char*>(malloc(sizeof(unsigned char) * jmpSize));
	// jmp
	jmpBuf[0] = 0xE9;
	memcpy(jmpBuf + 1, &jmpOffset, 4);

	// Write the main body of code
	memcpy(cavLoc, ren, codeSize);
	free(ren);

	// Write the return home jump
	memcpy((LPVOID)(reinterpret_cast<intptr_t>(cavLoc) + codeSizeNoJmp), jmpBuf, jmpSize);
	free(jmpBuf);

	// Now that we've injected the code, overwrite the target
	size_t overriddenSize = cav->overwrittenInstructionSize();
	unsigned char* overBuf = static_cast<unsigned char*>(malloc(overriddenSize * sizeof(unsigned char)));
	targetLoc = reinterpret_cast<intptr_t>(cavLoc);
	jmpLoc = injectLoc + jmpSize;
	jmpOffset = targetLoc - jmpLoc;

	// jmp
	overBuf[0] = 0xE9;
	memcpy(overBuf + 1, &jmpOffset, 4);

	// nop
	for (auto i = (1u + 4u); i < overriddenSize; i++)
		overBuf[i] = 0x90;

	memcpy(reinterpret_cast<LPVOID>(injectLoc), overBuf, cav->overwrittenInstructionSize());
	VirtualProtect(reinterpret_cast<LPVOID>(injectLoc), cav->overwrittenInstructionSize(), PAGE_EXECUTE_READ, &oldProtect);
	VirtualProtect(reinterpret_cast<LPVOID>(cavLoc), codeSize, PAGE_EXECUTE_READ, &oldProtect);
	free(overBuf);
}

void Game::sendTaunt() const
{
}

void Game::setInputsEnabled(bool b) const
{
	if (gameData->dev_base)
		gameData->dev_base->windowActive = b ? 0x01 : 0x0;
}

void Game::setInputOverride(int idx, bool set) const
{
	gameData->inputsForcePlayers[idx] = set ? 0x01 : 0x0;
}

void Game::setPlayerExists(int playerN, bool exists) const
{
	char targ = exists ? 0x01 : 0x0;
	gameData->player_states[playerN]->exists = targ;
}


void Game::setPlayerLives(int playerN, int lives) const
{
	assert(playerN < 4);
	auto addr = gameData->player_states[playerN];
	if (!addr) return;
	addr->lives = lives;
}

void Game::resetPlayerHitCounters(int playerN) const
{
	assert(playerN < 4);
	auto addr = gameData->player_states[playerN];
	if (!addr) return;
	addr->total_hit_counter = 0;
}

void Game::resetPlayerBuntCounters(int playerN) const
{
	assert(playerN < 4);
	auto addr = gameData->player_states[playerN];
	if (!addr) return;
	addr->bunt_counter = 0;
}

void Game::setInputImmediate(int inputi, bool set)
{
	// narrow that right down
	char input = (char)inputi;
	char& inputs = gameData->forcedInputs[0];
	inputTimings.erase(input);
	if (set)
		inputs |= input;
	else if (inputs & input)
		inputs ^= input;
}

void Game::holdInputUntil(int inputi, TIME_POINT time)
{
	// narrow that right down
	char input = (char)inputi;
	inputTimings[input] = time;
	gameData->forcedInputs[0] |= input;
}

void Game::updateInputs()
{
	TIME_POINT now = CLOCK_U::now();
	std::vector<int> toUnset(inputTimings.size());
	char& inputs = gameData->forcedInputs[0];
	for (auto& kv : inputTimings)
	{
		// if the time less than now
		if (kv.second <= now)
		{
			// unset the button
			// xor the first to unset it
			// 00001 ^ 00001 = 00000
			if (inputs & kv.first)
				inputs ^= kv.first;
			toUnset.push_back(kv.first);
		}
	}

	for (auto& k : toUnset)
		inputTimings.erase(k);
}

void Game::resetInputs()
{
	inputTimings.clear();
	memset(gameData->forcedInputs, 0, sizeof(gameData->forcedInputs));
}

Game::~Game()
{
	free(gameData);
	FREE_HANDLE(gameHandle);
}
