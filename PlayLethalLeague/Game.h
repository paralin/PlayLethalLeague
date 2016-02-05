#pragma once

#include <windows.h>
#include <vector>
#include <memory>
#define APP_ID 261180

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
	byte  do_reset;
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

private:
	std::string executablePath;
	std::string steamPath;

	HANDLE gameHandle;
	DWORD processId;

	std::vector<std::shared_ptr<CodeCave>> caves;

public:
	// Local copy
	GameOffsetStorage* localOffsetStorage;
	// Position of the remote GameOffsetStorage
	void* remoteOffsetStorage;
};
