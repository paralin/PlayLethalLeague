#include "stdafx.h"
#include "Game.h"
#include "Util.h"
#include <sys/stat.h>
#include "AllCaves.h"
#include <asmjit/asmjit.h>
#include <Tlhelp32.h>
#include <cassert>

#define FREE_HANDLE(thing) if (thing != nullptr) {CloseHandle(thing);} thing = nullptr;
#define REGISTER_CODECAVE(CLASS) caves.push_back(std::make_shared<CLASS>());

Game::Game() : 
	gameHandle(nullptr), 
	processId(0), 
	remoteOffsetStorage(nullptr)
{
	localOffsetStorage = static_cast<GameOffsetStorage*>(malloc(sizeof(GameOffsetStorage)));

	REGISTER_CODECAVE(BallCave);
	REGISTER_CODECAVE(GameRulesCave);
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

void Game::readOffsets() const
{
	ReadProcessMemory(gameHandle, remoteOffsetStorage, localOffsetStorage, sizeof(GameOffsetStorage), nullptr);
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
			for (int i = 0; i < scn->patternSize; i++)
			{
				LOG(" " << std::dec << i << ": " << std::hex << ((int)scn->pattern[i]));
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
		for (auto x = 0; x < scans.size(); x++)
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
					for (int sx = 1; sx < s->patternSize; sx++)
					{
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
						LOG("   - " << std::dec << sx << ": " << std::hex << ((int)buf[i + sx]) <<  " = " << ((int)s->pattern[sx]));
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
	LOG("Performing code cave at injection location " << injectLoc);
	size_t codeSize;
	size_t codeSizeNoJmp;
	char* ren = cav->render(this, injectLoc, &codeSize);

	// Now we need to make the return jump
	// add 24 (twice what we need) bytes to be safe
	codeSizeNoJmp = codeSize;
	codeSize += 24;
	LPVOID cavLoc = VirtualAllocEx(gameHandle, nullptr, codeSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	LOG("Location of cave: " << std::hex << (reinterpret_cast<intptr_t>(cavLoc)) << std::dec);

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
	DWORD oldProtect;
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

	VirtualProtectEx(gameHandle, reinterpret_cast<LPVOID>(injectLoc), cav->overwrittenInstructionSize(), PAGE_EXECUTE_READWRITE, &oldProtect);
	WriteProcessMemory(gameHandle, reinterpret_cast<LPVOID>(injectLoc), overBuf, cav->overwrittenInstructionSize(), nullptr);
	VirtualProtectEx(gameHandle, reinterpret_cast<LPVOID>(injectLoc), cav->overwrittenInstructionSize(), oldProtect, &oldProtect);
	free(overBuf);
}


Game::~Game()
{
	free(localOffsetStorage);
	FREE_HANDLE(gameHandle);
}
