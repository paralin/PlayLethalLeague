#pragma once

class Game;
class PythonEngine
{
public:
	PythonEngine(Game* game, std::string scriptsRoot);
	~PythonEngine();

	bool loadPythonCode();

	// callbacks
	void newMatchStarted();
	void playOneFrame();

	std::string scriptsRoot;
	Game* game;
};

