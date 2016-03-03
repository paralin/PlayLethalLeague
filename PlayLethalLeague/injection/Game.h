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

  void setBallXCoord(int bx) const;
  void setBallYCoord(int by) const;

  void setPlayerXCoord(int idx, int bx) const;
  void setPlayerYCoord(int idx, int by) const;

  void setBallState(int state) const;
  void setBallRespawnTimer(int tmr) const;

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