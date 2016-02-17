#pragma once

#include <boost/python.hpp>

class Game;
class PythonEngine
{
public:
	_declspec(dllexport) PythonEngine(Game* game, std::string scriptsRoot);
	_declspec(dllexport) ~PythonEngine();

	_declspec(dllexport) static void initializePython();
	_declspec(dllexport) bool loadPythonCode();

	// callbacks
	void newMatchStarted();
	void playOneFrame();

	std::string scriptsRoot;
	Game* game;

	boost::python::object global;
	boost::python::object interfaceInstance;
};

