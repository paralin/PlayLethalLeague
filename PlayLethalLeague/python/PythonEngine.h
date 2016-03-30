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
  bool engineNeedReinit;

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
	boost::python::object interfaceReloader;

	DWORD nextFrameUpdateTime = 0;

	std::mutex pyMtx;

	static const char* pythonHome;

private:
  boost::python::object tryCallFunction(const char* fcn);
};

