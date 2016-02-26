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
	_declspec(dllexport) static void initializePython(std::string& scriptsRoot);
	_declspec(dllexport) bool loadPythonCode();

	// callbacks
	void newMatchStarted();
	void playOneFrame();
	void matchReset();

	std::string scriptsRoot;
	Game* game;

	boost::python::object global;
	boost::python::object interfaceInstance;

	DWORD nextFrameUpdateTime = 0;

	std::mutex pyMtx;

	static const char* pythonHome;

private:
  boost::python::object tryCallFunction(const char* fcn);
};

