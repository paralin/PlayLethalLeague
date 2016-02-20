#pragma once

#include <boost/python.hpp>
#include <mutex>

class Game;
class PythonEngine
{
public:
	_declspec(dllexport) PythonEngine(Game* game, std::string scriptsRoot);
	_declspec(dllexport) ~PythonEngine();

	// must be at the beginning
	bool shutDown;

	_declspec(dllexport) void reloadPythonCode();
	_declspec(dllexport) static void initializePython();
	_declspec(dllexport) bool loadPythonCode();

	// callbacks
	void newMatchStarted();
	void playOneFrame();
	void matchReset();
  void learnOneFrame();

	void learnOneFrameThread();

	std::string scriptsRoot;
	Game* game;

	boost::python::object global;
	boost::python::object interfaceInstance;

	std::mutex pyMtx;
	std::thread learnOnceThread;

	DWORD nextFrameUpdateTime = 0;

private:
  boost::python::object tryCallFunction(const char* fcn, bool doLockPy=true);
};

