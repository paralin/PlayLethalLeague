// PlayLethalLeague.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Game.h"
#include "Util.h"

int main()
{
	enableDebugPriv();
	Game g;

	LOG("Locating game executable...");
	if (!g.locateExecutable())
	{
		LOG("Unable to locate game executable. Exiting.");
		getchar();
		return 1;
	}

	LOG("Killing existing processes....");
	Game::killProcesses();

	// Wait for it to die
	Sleep(1000);

	LOG("Launching process...");
	g.launch();

	LOG("Attaching to process....")
	{
		int tries = 0;
		while (tries < 20)
		{
			if (g.attach())
				break;
			LOG("Process hasn't started yet...");
			Sleep(1000);
			tries++;
		}

		if (tries == 20)
		{
			LOG("Unable to attach to process.");
			getchar();
			return 1;
		}
	}

	LOG("Waiting for startup...");
	Sleep(5000);

	LOG("Initializing offset code caves...");
	// Avoid anything changing while we're working
	g.suspend();
	g.initOffsetStorage();
	if (!g.performCodeCaves())
	{
		LOG("Unable to perform one of the code caves.");
		getchar();
		return 1;
	}

	LOG("Done performing all code caves...");
	g.unsuspend();

	// LOG("Advancing to main menu...");
	while (true)
	{
		Sleep(2000);
		g.suspend();
		g.readOffsets();
		g.unsuspend();

		LOG(" == offsets ==")
		LOG("ball_base: " << g.localOffsetStorage->ball_base);
		LOG("game_rules: " << g.localOffsetStorage->gamerule_set);
		LOG("dev_base: " << g.localOffsetStorage->dev_base);
		LOG("spawn: " << g.localOffsetStorage->player_spawn);
		for (int i = 0; i < 4; i++)
		{
			LOG("-> plyr " << i);
			LOG("   - base:  " << g.localOffsetStorage->player_bases[i]);
			LOG("   - coord: " << g.localOffsetStorage->player_coords[i]);
			LOG("   - state: " << g.localOffsetStorage->player_states[i]);
		}
	}

	LOG("Done.");
	getchar();

    return 0;
}

