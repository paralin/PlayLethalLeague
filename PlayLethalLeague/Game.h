#pragma once

#include <windows.h>
#include <vector>
#include <memory>
#include "GameStructs.h"
#include <map>
#include <chrono>
#define APP_ID 261180

#define CLOCK_U std::chrono::steady_clock
#define TIME_POINT std::chrono::time_point<CLOCK_U>

// Don't dereference these!
struct GameOffsetStorage
{
	// Offsets
	void* ball_base;
	void* ball_coord;
	void* ball_state;
	void* gamerule_set;
	void* dev_base;
	void* stage_base;
	void* player_bases[4];
	void* player_coords[4];
	void* player_states[4];
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
};

class CodeCave;
class Game
{
public:
	Game();
	~Game();

	bool locateExecutable();
	static void killProcesses();

	void launch() const;
	bool attach();

	void suspend() const;
	void unsuspend() const;

	void initOffsetStorage();
	void readOffsets() const;

	bool performCodeCaves();
	void performCodeCave(intptr_t injectLoc, CodeCave* cav);

	void readGameData();

	void setInputsEnabled(bool b);

	void setInputImmediate(char input, bool set);
	void holdInputUntil(char input, TIME_POINT time);;

	void writeInputOverrides() const;
	void updateInputs();

	void setPlayerLives(int playerN, int lives);
	void setPlayerExists(int playerN, bool exists);

	void resetPlayerHitCounters(int playerN);
	void resetPlayerBuntCounters(int playerN);

	void sendTaunt();

	void resetInputs();

	HANDLE gameHandle;
	DWORD processId;

private:
	std::string executablePath;
	std::string steamPath;

	std::vector<std::shared_ptr<CodeCave>> caves;
	std::map<char, TIME_POINT> inputTimings;

	static void readHitbox(Hitbox& box, intptr_t plyrbase);

public:
	// Local copy
	GameOffsetStorage* localOffsetStorage;
	// Position of the remote GameOffsetStorage
	void* remoteOffsetStorage;

	DevRegion localDevRegion;
	BallState localBallState;
	EntityCoords localBallCoords;
	Player players[4];
	Stage stage;
};
