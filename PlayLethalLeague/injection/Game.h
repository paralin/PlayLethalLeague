#pragma once

#include <windows.h>
#include <vector>
#include <memory>
#include "GameStructs.h"
#include <map>
#include <chrono>
#include "PatternScan.h"
#include <misc/Utils.h>
#include <python/PythonEngine.h>

class CodeCave;
#define APP_ID 261180

struct SinglePlayer
{
	EntityBase* base;
	EntityCoords* coords;
	PlayerState* state;
};

struct GameStorage
{
	void* ball_base;
	EntityCoords* ball_coord;
	BallState* ball_state;
	void* gamerule_set;
	DevRegion* dev_base;
	Stage* stage_base;
	EntityBase* player_bases[4];
	EntityCoords* player_coords[4];
	PlayerState* player_states[4];
	void* player_spawn;

	// Storage
	// current player for player loop
	int currentPlayer;

	// Flags
	// should the buffer be reset
	char  do_reset;

	// 2 bytes per player. 
	char forcedInputs[8];

	// At the end of each frame the inputs are copied here
	// Don't set this
	char inputsLastFrame[8];

	// Counter for inputs code
	// Don't set this
	int inputsCurrentPlayer;

	// Which players should have their inputs set.
	// Set the indexes to 1
	char inputsForcePlayers[4];

	// saved inputs by asm
	char inputsSaved[8];

	// saved inputs pre python hooks, sticky attack until observe
	char inputsSavedSticky[4];

	// Address of frame hook function
	void* frameHookAddress;

	// Remove lives on deaths
	char decrementLifeOnDeath;

	// Are we in online play or not
	char isOnline;

	SinglePlayer getSinglePlayer(int idx)
	{
		SinglePlayer p;
		p.state = player_states[idx];
		p.coords = player_coords[idx];
		p.base = player_bases[idx];
		return p;
	}

	boost::python::list getSinglePlayerInputs(int idx)
	{
		boost::python::list inps;
		const char inp = inputsSavedSticky[idx];
		// 8 bits
		inps.append(inp & CONTROL_UP);
		inps.append(inp & CONTROL_DOWN);
		inps.append(inp & CONTROL_LEFT);
		inps.append(inp & CONTROL_RIGHT);
		inps.append(inp & CONTROL_ATTACK);
		inps.append(inp & CONTROL_BUNT);
		inps.append(inp & CONTROL_JUMP);

		// every get we clear it so that it can be updated again later
		inputsSavedSticky[idx] = 0;
		return inps;
	}
};

struct CodeCaveScan
{
	std::shared_ptr<CodeCave> cave;
	std::shared_ptr<PatternScan> patterns;
	unsigned char* pattern;
	size_t patternSize;
	const char* name;
	void* foundAddress;
};

class CodeCave;
class Game
{
public:
	_declspec(dllexport) Game()
	{
		LOG("WARN: game default constructor used, probably from python.");
		LOG("Constructing Game from Python is not supported.");
	}

	_declspec(dllexport) Game(std::string scriptsPath);
	_declspec(dllexport) ~Game();

	bool locateExecutable();

	void checkResetOffsets() const;

	bool performCodeCaves();
	void performCodeCave(intptr_t injectLoc, CodeCave* cav);

	void setInputsEnabled(bool b) const;
	void setInputOverride(int idx, bool set) const;

	void setInputImmediate(int input, bool set);
	void holdInputUntil(int input, TIME_POINT time);
	void clearCaves() { caves.clear(); patternScans.clear(); };

	void updateInputs();

	void setPlayerLives(int playerN, int lives) const;
	void setPlayerExists(int playerN, bool exists) const;

	void resetPlayerHitCounters(int playerN) const;
	void resetPlayerBuntCounters(int playerN) const;

	void sendTaunt() const;
	void resetInputs();

	void respawnPlayer(int i) const;
	void resetBall() const;

	// Hooked function for frame tick!
	void hookedFrameTick();

	HANDLE gameHandle;
	DWORD processId;

	std::string executablePath;
	std::string steamPath;

	std::vector<std::shared_ptr<CodeCave>> caves;
	std::vector<std::shared_ptr<PatternScan>> patternScans;
	std::map<char, TIME_POINT> inputTimings;

	bool printedHookSuccessful;
	bool wasInGame;

	// Local copy
	GameStorage* gameData;

	std::shared_ptr<PythonEngine> python;

	bool reloadingPythonCode;
};