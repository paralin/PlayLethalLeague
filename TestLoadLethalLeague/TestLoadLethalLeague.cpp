#include <misc/Utils.h>
#include <iostream>
#include <Shlwapi.h>
#include <python/PythonEngine.h>
#include <injection/Game.h>

// The purpose of this is just to see if it opens at all
// If you get an error message about a missing dll
// This is how to figure this out before trying to inject the dll into the game
// Note to inject into the game you have to copy the dlls into the game dir
// To use this properly uncomment USE_DLL_THREAD in DllMain.cpp
int main()
{
	std::cout << "Success, the process stared without crashing. You have all the dlls.";
	testInjectedPlayLL();

	std::string rootPath;
	std::string scriptsPath;
	{
		char pathToModule[MAX_PATH];
		size_t pathLength = GetModuleFileName(NULL, pathToModule, MAX_PATH);
		if (pathLength == 0)
		{
			LOG("Unable to get path to current module!");
			getchar();
			return 0;
		}

		LOG("Path to module: " << pathToModule);
		// remove filename and dirname
		PathRemoveFileSpec(pathToModule);
		PathRemoveFileSpec(pathToModule);
		rootPath = std::string(pathToModule);
		scriptsPath = rootPath + std::string("\\scripts\\");
		LOG("Scripts path: " << scriptsPath.c_str());
	}

	LOG("Initializing python interpreter...");
	PythonEngine::initializePython(scriptsPath);

	Game* game = new Game(scriptsPath);
	if (!game->python->loadPythonCode())
	{
		LOG("Unable to load python code.");
	}

	getchar();
    return 0;
}

