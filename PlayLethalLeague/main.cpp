// PlayLethalLeague.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Game.h"
#include "Util.h"
#include <ctime>
#include <chrono>
#include "LLNeural.h"
#define USE_NEURAL

#define PRINT_BEGIN(NAME) auto& t = g.NAME;
#define PRINT_VAR(NAME, VAR) LOG("  - " << NAME << ": " << (static_cast<int>(t.VAR)));

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

	g.readOffsets();
	g.readGameData();
	g.setInputsEnabled(true);
	g.localDevRegion.windowActive = 0x01;

	LOG("Done performing all code caves...");
	g.unsuspend();

#ifdef USE_NEURAL
	LLNeural neural(&g);
#endif

	bool wasInGame = false;
	bool playedOneFrame = true;
	char currentTagValue = 0;
	while (true)
	{
		// make sure we are only overwriting inputs for our player
		g.localOffsetStorage->inputsForcePlayers[0] = 0x01;
		g.updateInputs();
		g.writeInputOverrides();

#ifdef PRINT_VALUES
		Sleep(2000);
#else
		Sleep(100);
#endif

		g.readOffsets();

		bool isInGame = g.localOffsetStorage->ball_base && g.localOffsetStorage->ball_state && g.localOffsetStorage->ball_coord;

		if (isInGame != wasInGame)
		{
			LOG("=== Entered a new Match ===");
			LOG(" == offsets ==")
			LOG("ball_base: " << g.localOffsetStorage->ball_base);
			LOG("game_rules: " << g.localOffsetStorage->gamerule_set);
			LOG("dev_base: " << g.localOffsetStorage->dev_base);
			LOG("spawn: " << g.localOffsetStorage->player_spawn);

#ifdef USE_NEURAL
			if (playedOneFrame)
				neural.newMatchStarted();
#endif
			playedOneFrame = false;
		}
		if (!isInGame && wasInGame)
		{
			g.resetInputs();
		}
		wasInGame = isInGame;

		if (!isInGame)
			continue;

		g.readGameData();

#ifdef PRINT_VALUES
		for (auto i = 0; i < 4; i++)
		{
			/*
			LOG("-> plyr " << i);
			LOG("   - base:  " << g.localOffsetStorage->player_bases[i]);
			LOG("   - coord: " << g.localOffsetStorage->player_coords[i]);
			LOG("   - state: " << g.localOffsetStorage->player_states[i]);
			*/
			if (!g.localOffsetStorage->player_bases[i])
				continue;

			LOG("== player " << i << " state ==");
			LOG("max_speed: " << g.players[i].entity.max_speed);
			LOG("charge_length: " << g.players[i].entity.charge_length);
			LOG("lives: " << g.players[i].state.lives);
			LOG("hit_counter: " << g.players[i].state.total_hit_counter);
			LOG("bunt_counter: " << g.players[i].state.bunt_counter);
			LOG("x: " << g.players[i].coords.xcoord);
			LOG("y: " << g.players[i].coords.ycoord);
		}
#endif

		if (!g.localDevRegion.windowActive)
			g.setInputsEnabled(true);
#ifdef PRINT_VALUES
		if (g.localOffsetStorage->ball_state != nullptr)
		{
			LOG("== ball state ==");
			PRINT_BEGIN(localBallState);
			PRINT_VAR("hitstun", hitstun);
			PRINT_VAR("exists", ballExists);
			PRINT_VAR("xspeed", xspeed);
			PRINT_VAR("yspeed", yspeed);
			PRINT_VAR("stunCooldown", hitstunCooldown);
			PRINT_VAR("speed", ballSpeed);
			PRINT_VAR("tag", ballTag);
			PRINT_VAR("direction", direction);
			PRINT_VAR("hitCount", hitCount);
		}

		if (g.localOffsetStorage->ball_coord != nullptr)
		{
			LOG("== ball coord ==");
			PRINT_BEGIN(localBallCoords);
			PRINT_VAR("x", xcoord);
			PRINT_VAR("y", ycoord);
		}
#endif

		char tagValue = g.localBallState.ballTag;
		if (currentTagValue != tagValue)
		{
			LOG("Ball tag changed: " << std::hex << (static_cast<int>(currentTagValue)) << " -> " << (static_cast<int>(tagValue)));
			currentTagValue = tagValue;
		}

#ifdef USE_NEURAL
		neural.playOneFrame();
#endif
	}

	LOG("Done.");
	getchar();

    return 0;
}

