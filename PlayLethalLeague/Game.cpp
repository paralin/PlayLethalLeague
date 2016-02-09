#include "stdafx.h"
#include "Game.h"
#include "Util.h"
#include <sys/stat.h>
#include "AllCaves.h"
#include <Tlhelp32.h>
#include <cassert>
#include <ctime>

#define FREE_HANDLE(thing) if (thing != nullptr) {CloseHandle(thing);} thing = nullptr;
#define REGISTER_CODECAVE(CLASS) caves.push_back(std::make_shared<CLASS>());

#define ZERO_STRUCT(stru) ZeroMemory(&stru, sizeof(stru));
Game::Game() : 
	gameHandle(nullptr), 
	processId(0), 
	remoteOffsetStorage(nullptr)
{
	localOffsetStorage = static_cast<GameOffsetStorage*>(malloc(sizeof(GameOffsetStorage)));

	ZERO_STRUCT(localDevRegion);
	ZERO_STRUCT(localBallState);
	ZERO_STRUCT(localBallCoords);
	ZERO_STRUCT(players);

	REGISTER_CODECAVE(BallCave);
	REGISTER_CODECAVE(GameRulesCave);
	REGISTER_CODECAVE(StageCave);
	REGISTER_CODECAVE(DevCave);
	REGISTER_CODECAVE(PlayerCave);
	REGISTER_CODECAVE(PlayerSpawnCave);
	REGISTER_CODECAVE(WindowRefocusCave);
	REGISTER_CODECAVE(WindowUnfocusCave);
	REGISTER_CODECAVE(InputPressedCave);
	REGISTER_CODECAVE(InputHeldCave);
	REGISTER_CODECAVE(ResetCave);
}


bool Game::locateExecutable()
{
	executablePath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\lethalleague\\LethalLeague.exe";
	steamPath = "C:\\Program Files (x86)\\Steam\\Steam.exe";
	// Check Steam location
	struct stat buffer;
	return (stat(executablePath.c_str(), &buffer) == 0) && (stat(executablePath.c_str(), &buffer) == 0);
}

void Game::killProcesses()
{
	killProcessByName("LethalLeague.exe");
}

void Game::launch() const
{
	LOG("Telling Steam to launch the game...");
	const char* args = "-applaunch 261180";
	startProcess(steamPath.c_str(), args);
}

bool Game::attach()
{
	HANDLE hand = findProcessByName("LethalLeague.exe");
	if (hand == nullptr)
		return false;

	FREE_HANDLE(gameHandle);
	gameHandle = hand;
	processId = GetProcessId(gameHandle);
	return true;
}

void Game::suspend() const
{
	DebugActiveProcess(processId);
}

void Game::unsuspend() const
{
	DebugActiveProcessStop(processId);
}

void Game::initOffsetStorage()
{
	// Allocate offset storage in the space
	remoteOffsetStorage = VirtualAllocEx(gameHandle, nullptr, sizeof(GameOffsetStorage), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	// Initialize local to zero
	memset(localOffsetStorage, 0, sizeof(GameOffsetStorage));
	// Copy it to the remote process to zero that too
	WriteProcessMemory(gameHandle, remoteOffsetStorage, localOffsetStorage, sizeof(GameOffsetStorage), nullptr);
}

#define ZEROOFF(IDENT) memset(&localOffsetStorage->IDENT, 0, sizeof(localOffsetStorage->IDENT));
void Game::readOffsets() const
{
	ReadProcessMemory(gameHandle, remoteOffsetStorage, localOffsetStorage, sizeof(GameOffsetStorage), nullptr);
	if (localOffsetStorage->do_reset)
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
		// Copy it to the remote process to zero that too
		WriteProcessMemory(gameHandle, remoteOffsetStorage, localOffsetStorage, sizeof(GameOffsetStorage), nullptr);
	}
}

MODULEENTRY32 GetModuleInfo(std::uint32_t ProcessID)
{
	void* hSnap = nullptr;
	MODULEENTRY32 Mod32 = { 0 };

	if ((hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ProcessID)) == INVALID_HANDLE_VALUE)
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

struct CodeCaveScan
{
	std::shared_ptr<CodeCave> cave;
	char* pattern;
	size_t patternSize;
};

#define LIMIT_TO_MODULE

bool Game::performCodeCaves()
{
	std::vector<std::shared_ptr<CodeCaveScan>> scans;
	// Allocate the space
	for (auto& c : caves)
	{
		size_t scanSize;
		char* pat = c->getScanPattern(&scanSize);
		if (pat != nullptr)
		{
			auto scn = std::make_shared<CodeCaveScan>();
			scn->cave = c;
			scn->pattern = pat;
			scn->patternSize = scanSize;
			scans.push_back(scn);

			LOG("Searching for pattern:");
			for (size_t i = 0; i < scn->patternSize; i++)
			{
				if (scn->pattern[i] == -1)
					LOG(" " << std::dec << i << ": ??")
				else
					LOG(" " << std::dec << i << ": " << std::hex << ((int)scn->pattern[i]))
			}
		}
	}

	if (scans.empty())
		return true;

	intptr_t min_adr;
	intptr_t max_adr;

	size_t bufSize;
#ifdef LIMIT_TO_MODULE
	MODULEENTRY32 llmod = GetModuleInfo(processId);
	min_adr = reinterpret_cast<intptr_t>(llmod.modBaseAddr);
	max_adr = min_adr + llmod.modBaseSize;
	bufSize = 50000;
#else
	{
		LPSYSTEM_INFO sys_info = static_cast<LPSYSTEM_INFO>(malloc(sizeof(_SYSTEM_INFO)));
		GetSystemInfo(sys_info);
		min_adr = reinterpret_cast<intptr_t>(sys_info->lpMinimumApplicationAddress);
		max_adr = reinterpret_cast<intptr_t>(sys_info->lpMaximumApplicationAddress);
		// bufSize = sys_info->dwPageSize + 10;
		bufSize = 60000;
		free(sys_info);
	}
#endif

	// Make array of "first characters" to identify
	size_t initChars = scans.size();
	char* chars = static_cast<char*>(malloc(sizeof(char) * initChars));

	{
		for (size_t x = 0; x < scans.size(); x++)
			chars[x] = scans.at(x)->pattern[0];
	}

	MEMORY_BASIC_INFORMATION basic_info;
	char* buf = static_cast<char*>(malloc(bufSize));
	while (min_adr < max_adr && !scans.empty())
	{
		if (VirtualQueryEx(gameHandle, reinterpret_cast<LPCVOID>(min_adr), &basic_info, sizeof(MEMORY_BASIC_INFORMATION)) == 0)
		{
			LOG("Unable to query " << std::hex << min_adr);
			min_adr += 1;
			continue;
		}

		// if (basic_info.Protect != PAGE_READWRITE) // (basic_info.Protect & PAGE_EXECUTE || basic_info.Protect & PAGE_EXECUTE_READ)
		// if (basic_info.State & MEM_COMMIT)
		if (basic_info.State == MEM_COMMIT && basic_info.RegionSize > 0)
		{
			if (basic_info.RegionSize > bufSize)
			{
				LOG("Had to reallocate from " << bufSize << " to " << basic_info.RegionSize);
				bufSize = basic_info.RegionSize;
				buf = static_cast<char*>(realloc(buf, bufSize));
			}

			SIZE_T bytesRead;
			if (!ReadProcessMemory(gameHandle, basic_info.BaseAddress, buf, basic_info.RegionSize, &bytesRead))
			{
				LOG("Unable to read memory at location " << std::hex << basic_info.BaseAddress);
				goto next_addr;
			}

			SIZE_T x;
			for (SIZE_T i = 0; i < bytesRead; i++)
			{
				for (x = 0; x < initChars; x++)
				{
					if (buf[i] != chars[x])
						continue;
					CodeCaveScan* s = scans.at(x).get();
					if (i + s->patternSize >= bytesRead)
						continue;
					bool found = true;
					for (size_t sx = 1; sx < s->patternSize; sx++)
					{
						// wildcard
						if (s->pattern[sx] == -1)
							continue;

						if (buf[i + sx] != s->pattern[sx])
						{
							found = false;
							break;
						}
					}
					if (!found)
						continue;

					intptr_t loc = (reinterpret_cast<intptr_t>(basic_info.BaseAddress) + i);
					LOG(" ==> match " << std::hex << loc);
					for (int sx = 0; sx < s->patternSize; sx++)
					{
						LOG("   - " << std::dec << sx << ": " << std::hex << ((int)buf[i + sx]) << " = " << ((int)s->pattern[sx]));
					}

					loc = (reinterpret_cast<intptr_t>(basic_info.BaseAddress) + i);
					LOG("Found cave pattern at " << std::hex << (loc));
					LOG(" - this is lethalleague.exe+" << std::hex << i);
					performCodeCave(loc, s->cave.get());
					// Remove the char from the list
					free(s->pattern);
					scans.erase(scans.begin() + x);
					// shift 1
					for (int cx = x; cx + 1 < initChars; cx++)
						chars[cx] = chars[cx + 1];
					// remove 1
					initChars--;
				}
				if (initChars == 0)
					break;
			}
			if (initChars == 0)
				break;
		}
	next_addr:
		min_adr = reinterpret_cast<intptr_t>(basic_info.BaseAddress) + static_cast<intptr_t>(basic_info.RegionSize);
		if (initChars == 0)
			break;
	}

	free(chars);
	free(buf);
	if (!scans.empty())
	{
		LOG("Unable to resolve the last " << scans.size() << " scans.");
		return false;
	}

	return true;
}

void Game::performCodeCave(intptr_t injectLoc, CodeCave* cav)
{
	DWORD oldProtect;
	VirtualProtectEx(gameHandle, reinterpret_cast<LPVOID>(injectLoc), cav->overwrittenInstructionSize(), PAGE_EXECUTE_READWRITE, &oldProtect);
	bool doCallOrig = cav->shouldPrependOriginal();
	size_t codeSize;
	size_t codeSizeNoJmp;
	unsigned char* ren = (unsigned char*) cav->render(this, injectLoc, &codeSize, doCallOrig ? cav->overwrittenInstructionSize() : 0);

	if (ren == NULL)
	{
		LOG("Removing matched instructions at location " << injectLoc);
		// We just want to overwrite the instruction with nop
		char* nopBuf = (char*)malloc(sizeof(char) * cav->overwrittenInstructionSize());
		memset(nopBuf, 0x90, cav->overwrittenInstructionSize());
		WriteProcessMemory(gameHandle, reinterpret_cast<LPVOID>(injectLoc), nopBuf, cav->overwrittenInstructionSize(), NULL);
		VirtualProtectEx(gameHandle, reinterpret_cast<LPVOID>(injectLoc), cav->overwrittenInstructionSize(), oldProtect, NULL);
		free(nopBuf);
		return;
	}

	LOG("Performing code cave at injection location " << injectLoc);

	// Read the original instructions
	if (doCallOrig)
	{
		ReadProcessMemory(gameHandle, (LPVOID)injectLoc, ren, cav->overwrittenInstructionSize(), NULL);
		LOG("Original instructions (keeping):");
		for (int i = 0; i < cav->overwrittenInstructionSize(); i++)
		{
			std::cout << std::hex << ((int)ren[i]) << " ";
		}
		std::cout << std::endl;
	}


	// Now we need to make the return jump
	// add 24 (twice what we need) bytes to be safe
	codeSizeNoJmp = codeSize;
	codeSize += 5;
	LPVOID cavLoc = VirtualAllocEx(gameHandle, nullptr, codeSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
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

	char* jmpBuf = static_cast<char*>(malloc(sizeof(char) * jmpSize));
	// jmp
	jmpBuf[0] = 0xE9;
	assert(4 == sizeof(intptr_t));
	memcpy(jmpBuf + 1, &jmpOffset, 4);

	// Write the main body of code
	WriteProcessMemory(gameHandle, cavLoc, ren, codeSize, NULL);
	free(ren);

	// Write the return home jump
	WriteProcessMemory(gameHandle, (LPVOID)(reinterpret_cast<intptr_t>(cavLoc) + codeSizeNoJmp), jmpBuf, jmpSize, NULL);
	free(jmpBuf);

	// Re-protect it
	VirtualProtectEx(gameHandle, cavLoc, codeSize, PAGE_EXECUTE_READ, &oldProtect);

	// Now that we've injected the code, overwrite the target
	size_t overriddenSize = cav->overwrittenInstructionSize();
	char* overBuf = static_cast<char*>(malloc(overriddenSize * sizeof(char)));
	targetLoc = reinterpret_cast<intptr_t>(cavLoc);
	jmpLoc = injectLoc + jmpSize;
	jmpOffset = targetLoc - jmpLoc;

	// jmp
	overBuf[0] = 0xE9;
	memcpy(overBuf + 1, &jmpOffset, 4);
	for (int i = (1 + 4); i < overriddenSize; i++)
	{
		// nop
		overBuf[i] = 0x90;
	}

	WriteProcessMemory(gameHandle, reinterpret_cast<LPVOID>(injectLoc), overBuf, cav->overwrittenInstructionSize(), nullptr);
	VirtualProtectEx(gameHandle, reinterpret_cast<LPVOID>(injectLoc), cav->overwrittenInstructionSize(), oldProtect, &oldProtect);
	free(overBuf);
}

void Game::setInputsEnabled(bool b)
{
	if (localOffsetStorage->dev_base == nullptr)
	{
		readOffsets();
		if (localOffsetStorage->dev_base == nullptr)
			return;
	}
	localDevRegion.windowActive = b ? 0x01 : 0x0;
	const DevRegion* s = &localDevRegion;
	WriteProcessMemory(gameHandle, (LPVOID)(((intptr_t)localOffsetStorage->dev_base) + OFFSETOF(windowActive)), &localDevRegion.windowActive, sizeof(char), NULL);
}

void Game::setPlayerLives(int playerN, int lives)
{
	assert(playerN < 4);
	auto addr = localOffsetStorage->player_states[playerN];
	if (!addr) return;
	// get offset
	auto offset = (reinterpret_cast<intptr_t>(&players[0].state.lives) - reinterpret_cast<intptr_t>(&players[0].state));
	int livesl = lives;
	WriteProcessMemory(gameHandle, (LPVOID)((intptr_t)addr + offset), &livesl, sizeof(livesl), NULL);
}

void Game::resetPlayerHitCounters(int playerN)
{
	assert(playerN < 4);
	auto addr = localOffsetStorage->player_states[playerN];
	if (!addr) return;
	// get offset
	auto offset = (reinterpret_cast<intptr_t>(&players[0].state.total_hit_counter) - reinterpret_cast<intptr_t>(&players[0].state));
	int hitsl = 0;
	WriteProcessMemory(gameHandle, (LPVOID)((intptr_t)addr + offset), &hitsl, sizeof(hitsl), NULL);
}

void Game::resetPlayerBuntCounters(int playerN)
{
	assert(playerN < 4);
	auto addr = localOffsetStorage->player_states[playerN];
	if (!addr) return;
	// get offset
	auto offset = (reinterpret_cast<intptr_t>(&players[0].state.bunt_counter) - reinterpret_cast<intptr_t>(&players[0].state));
	int hitsl = 0;
	WriteProcessMemory(gameHandle, (LPVOID)((intptr_t)addr + offset), &hitsl, sizeof(hitsl), nullptr);
}

void Game::setInputImmediate(char input, bool set)
{
	char& inputs = localOffsetStorage->forcedInputs[0];
	inputTimings.erase(input);
	if (set)
		inputs |= input;
	else if (inputs & input)
		inputs ^= input;
}

void Game::holdInputUntil(char input, TIME_POINT time)
{
	inputTimings[input] = time;
	localOffsetStorage->forcedInputs[0] |= input;
}

void Game::updateInputs()
{
	TIME_POINT now = CLOCK_U::now();
	std::vector<int> toUnset(inputTimings.size());
	char& inputs = localOffsetStorage->forcedInputs[0];
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

void Game::writeInputOverrides() const
{
	intptr_t forcedInputsAddr = reinterpret_cast<intptr_t>(remoteOffsetStorage) + (reinterpret_cast<intptr_t>(&localOffsetStorage->forcedInputs) - reinterpret_cast<intptr_t>(localOffsetStorage));
	WriteProcessMemory(gameHandle, reinterpret_cast<LPVOID>(forcedInputsAddr), &localOffsetStorage->forcedInputs, sizeof(localOffsetStorage->forcedInputs), nullptr);
	intptr_t forcePlayersAddr = reinterpret_cast<intptr_t>(remoteOffsetStorage) + (reinterpret_cast<intptr_t>(&localOffsetStorage->inputsForcePlayers) - reinterpret_cast<intptr_t>(localOffsetStorage));
	WriteProcessMemory(gameHandle, reinterpret_cast<LPVOID>(forcePlayersAddr), &localOffsetStorage->inputsForcePlayers, sizeof(localOffsetStorage->inputsForcePlayers), nullptr);
}

void Game::resetInputs()
{
	inputTimings.clear();
	memset(localOffsetStorage->forcedInputs, 0, sizeof(localOffsetStorage->forcedInputs));
}


void Game::readHitbox(Hitbox& box, intptr_t hitboxbase)
{
	// implement later
}


#define READ_STRUCT(STRUCT, STORNAME) if (localOffsetStorage->STORNAME) { ReadProcessMemory(gameHandle, localOffsetStorage->STORNAME, &STRUCT, sizeof(STRUCT), NULL); }
void Game::readGameData()
{
	READ_STRUCT(localDevRegion, dev_base);
	READ_STRUCT(localBallState, ball_state);
	READ_STRUCT(localBallCoords, ball_coord);
	READ_STRUCT(stage, stage_base);
	for (int i = 0; i < 4; i++)
	{
		if (localOffsetStorage->player_bases[i] == nullptr)
			continue;

		ReadProcessMemory(gameHandle, localOffsetStorage->player_bases[i], &players[i].entity, sizeof(players[i].entity), nullptr);
		ReadProcessMemory(gameHandle, localOffsetStorage->player_coords[i], &players[i].coords, sizeof(players[i].coords), nullptr);
		ReadProcessMemory(gameHandle, localOffsetStorage->player_states[i], &players[i].state, sizeof(players[i].state), nullptr);

		// Read hitboxes
#if READ_HITBOXES
		readHitbox(players[i].neutral_right_hitbox, players[i].entity.neutral_1_hitbox);
		readHitbox(players[i].neutral_left_hitbox, players[i].entity.neutral_2_hitbox);
		readHitbox(players[i].bunt_left_hitbox, players[i].entity.bunt1_hitbox);
		// readHitbox(players[i].bunt_right_hitbox, players[i].entity.bunt);
#endif
	}
}

Game::~Game()
{
	free(localOffsetStorage);
	FREE_HANDLE(gameHandle);
}
