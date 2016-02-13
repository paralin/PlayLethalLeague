#include "stdafx.h"
#include "LLNeural.h"
#include "Game.h"
#include <Shlwapi.h>
#include <algorithm>

// #define PLAY_BEST

#define NLOG(MSGG)  { \
	std::lock_guard<std::mutex> scoped_lock(logMutex); \
	LOG("[LLNeural] " << MSGG); \
}
#define JLOG(MSGG)  { \
	std::lock_guard<std::mutex> scoped_lock(logMutex); \
	LOG("[JobQueue] " << MSGG); \
}
// test each individual twice (2 lives)
#define MAX_TESTS_PER_ITERATION 10
#define NUM_DEATHS_PER_ITERATION 1
#define ACTIVATE_DEPTH 3 

LLNeural::LLNeural(Game* game)
	: game(game),
	wasPlaying(false),
	testN(0),
	individualFitness(0),
	currentIndividual(0),
	deathCount(0),
	lastBuntCount(0),
	lastHitCount(0),
	bestAccuracy(0),
	swingCount(0),
	bestFitnessThisSpecies(0),
	hitsThisIndividual(0),
	timeIndividualStarted(0),
	timeNextUpdate(0),
	playedOneFrame(false),
	maxAccuracyLastRound(0),
	timeBallNotBunted(0),
	forceNewIndividual(false),
	bestFitnessEver(0),
	resetLivesUntil(0)
{
	NLOG("Initializing...");
	newMatchStarted();

	/*
	if (PathFileExists(populationPath) != 0)
	{
		loadFromFilesystem();
	}
	else
	*/
		initFromScratch();
}

void LLNeural::loadFromFilesystem()
{
	NLOG(" == initializing from saved params == ");
}


void LLNeural::initFromScratch()
{
	NLOG(" == initializing net from scratch ==");
}

void LLNeural::calcBestFitnessEver()
{
}

void LLNeural::newMatchStarted()
{
	NLOG("Re-init neural network for new match.");
	lastBuntCount = lastHitCount = testN = 0;
	deathCount = 0;
	lastFitness = individualFitness = 0;
	currentIndividual = 0;
	currentIndividual = 0;
	forceNewIndividual = false;
	timeIndividualStarted = 0;
	wasSwinging = false;
	wasPlaying = false;
	playedOneFrame = false;
	bestFitnessEver = 0;
	maxAccuracyLastRound = 0;
}

void LLNeural::saveToFile() const
{
}

void LLNeural::playOneFrame()
{
	const GameStorage* gd = game->gameData;
	DWORD tickCountNow = GetTickCount();
	if (timeIndividualStarted == 0)
		timeIndividualStarted = tickCountNow;

	// Determine if we are actually playing (both players are spawned and ball is hittable
	// ball not spawned states: 
	// 14: just after destroyed
	// 1: being spawned
	// 3: spawned, tagged
	// 5: sitting still, untagged
	// ball spawned states:
	// 10: bunt flying
	// 0: regular (flying)
	// 6: being hit (hitlag)

	// fitness scheme: 
	//  - 50 pts for hitting the ball
	//  - 50 pts for bunting the ball
	//  - 200 pts for winning the life

	bool playing = !gd->player_states[0]->respawn_timer
		&& gd->ball_state->state != 14
		&& gd->ball_state->state != 1;
	int ballSpeed = gd->ball_state->ballSpeed;

	// if (pop->m_Parameters.PhasedSearching)
		// pop->m_Parameters.TrainingHitOnly = pop->m_SearchMode == NEAT::SIMPLIFYING;
	bool trainHitOnly = false;
	char tarP2Exist = trainHitOnly ? 0x0 : 0x01;
	game->setPlayerExists(1, tarP2Exist);
	// game->gameData->decrementLifeOnDeath = pop->m_Parameters.TrainingHitOnly ? 0x0 : 0x01;

	if ((!playing && wasPlaying && !trainHitOnly) || (forceNewIndividual && trainHitOnly))
	{
		wasPlaying = playing;

		if (!forceNewIndividual)
		{
			if (gd->player_states[0]->lives > gd->player_states[1]->lives && tarP2Exist)
			{
				NLOG("We killed him! +300");
				individualFitness += 300;
				game->resetInputs();
			}
			else
				deathCount++;

			NLOG("End of life, resetting stocks to 18. Current deaths: " << deathCount);
		}
		else
			NLOG("Next individual.");

		game->setPlayerLives(0, 18);
		game->setPlayerLives(1, 18);
		ballIsBunted = false;
		wasSwinging = false;
		lastBallSpeed = 0;
		resetLivesUntil = tickCountNow + 100;

		// reset the ball
		if (trainHitOnly)
		{
			gd->ball_state->state = 14;
			gd->ball_state->serveResetCounter = 300000;
			gd->player_states[0]->special_meter = 0;
			gd->player_states[1]->special_meter = 0;
			gd->player_states[0]->character_state = 3;
			gd->player_states[0]->respawn_timer = 200000;
		}

		testN++;
		if (deathCount >= NUM_DEATHS_PER_ITERATION || testN >= MAX_TESTS_PER_ITERATION || forceNewIndividual)
		{
			NLOG("== finished individual ==");
			timeNextUpdate = tickCountNow + 200;

			NLOG("Best ever: " << std::dec << bestFitnessEver);
			NLOG("Fitness pre-accuracy: " << std::dec << individualFitness);
			double accur = 0;
			if (hitsThisIndividual == 0)
			{
				NLOG("We didn't hit even once, forcing fitness to zero.");
				individualFitness = 0;
			}
			else
			{
				accur = ((double)hitsThisIndividual) / ((double)swingCount);
				NLOG("Accuracy: " << accur << " (" << hitsThisIndividual << "/" << swingCount << ")");
				individualFitness += accur * 300.0;
			}

			if (accur > maxAccuracyLastRound)
				maxAccuracyLastRound = accur;

			NLOG("Fitness: " << std::dec << individualFitness);
			NLOG("Best accuracy this round: " << maxAccuracyLastRound);

			if (individualFitness > bestFitnessEver)
				bestFitnessEver = individualFitness;

			game->resetPlayerHitCounters(0);
			game->resetPlayerHitCounters(1);

			game->resetPlayerBuntCounters(0);
			game->resetPlayerBuntCounters(1);

			// serialize fitness
			// set evaluated

			timeIndividualStarted = 0;
			lastFitness = 0;
			hitsThisIndividual = 0;
			lastBuntCount = lastHitCount = testN = 0;
			deathCount = 0;
			swingCount = 0;

			individualFitness = 0;
			currentIndividual++;
			// MAX_INDIVIDUALS for 0
			if (currentIndividual == 0)
			{
				NLOG("===== FINISHED EVOLUTION =====");
				NLOG("Best fitness: " << std::dec << bestFitnessEver);
				saveToFile();
				game->resetInputs();
				maxAccuracyLastRound = 0;
				forceNewIndividual = false;
				game->setPlayerLives(0, 18);
				game->setPlayerLives(1, 18);
				bestAccuracy = 0;
			}

			NLOG("Now testing:");
			// NLOG("Individual: " << std::dec << (currentIndividual + 1) << "/" << pop->m_Parameters.PopulationSize);
		}
		forceNewIndividual = false;
		timeIndividualStarted = tickCountNow;
	}

	if (!playing)
	{
		game->setPlayerLives(0, 18);
		game->setPlayerLives(1, 18);
		return;
	}
	wasPlaying = true;

	// Decide how to play!

	// Set the inputs based on current state
	// Map positional inputs to [-1, 1]0
#define COORD_OFFSET 50000.0
#define TO_INPUT_RANGE(VAL, MAXVAL)  std::max(std::min(2.0 * ((((double)VAL) - (0.5 * MAXVAL)) / MAXVAL), 1.0), -1.0)
	double stageX = game->gameData->stage_base->x_origin * COORD_OFFSET;
	double stageXSize = game->gameData->stage_base->x_size * COORD_OFFSET;
	double stageY = game->gameData->stage_base->y_origin * COORD_OFFSET;
	double stageYSize = game->gameData->stage_base->y_size * COORD_OFFSET;

	double ballPosX = TO_INPUT_RANGE(game->gameData->ball_coord->xcoord - stageX, stageXSize);
	double ballPosY = TO_INPUT_RANGE(game->gameData->ball_coord->ycoord - stageY, stageYSize);

	// add inputs here

	const int& ballState = gd->ball_state->state;
	bool ballCurrentlyBunted = ballState == 10;
	if (!ballCurrentlyBunted && ballIsBunted)
		timeBallNotBunted = GetTickCount();
	ballIsBunted = ballCurrentlyBunted;

	bool currentlySwinging = gd->player_states[0]->character_state == 0x01;

	// send inputs
	// activate
	// get output
	// outputs:
	// game->setOutputImmediate for outputs

	bool justFinishedSwinging = false;
	if (wasSwinging && !currentlySwinging)
		justFinishedSwinging = true;

	// Calculate points!
	auto& hits = gd->player_states[0]->total_hit_counter;
	auto& bunts = gd->player_states[0]->bunt_counter;

	if (!wasSwinging && currentlySwinging)
	{
		hitsStartedSwinging = hits;
		wasSwinging = true;
	}

	if (hits != lastHitCount)
	{
		lastHitCount = hits;
		auto speed = lastBallSpeed;
		float speedFactor = std::min(std::max(static_cast<float>(speed) - 100.0f, 0.0f) / (1300 - 100.0f), 1.0f);
		if (bunts != lastBuntCount)
		{
			int points = 20;
			points += (int)(speedFactor * 200.0f);
			lastBuntCount = bunts;
			NLOG("We bunted the ball at speed " << speed << "! +" << points << "! Bunts: " << lastBuntCount);
			individualFitness += points;
		}
		else {
			int points = 50;
			points += (int)(speedFactor * 300.0f);
			lastBuntCount = bunts;
			hitsThisIndividual++;
			NLOG("We hit the ball at speed " << speed << "! +" << points << "! Hits: " << (lastHitCount - lastBuntCount));
			/*
			if (ballIsBunted || (tickCountNow - timeBallNotBunted >= 50))
			{
				timeBallNotBunted = tickCountNow;
				NLOG(" - hit was a bunted ball! POINT MULTIPLIER x2!");
				points *= 2;
			}
			*/
			individualFitness += points;
		}
	}
	if (justFinishedSwinging)
	{
		swingCount++;
		if (hits == hitsStartedSwinging)
		{
			NLOG("missed a swing, " << swingCount << " swings total.");
		}
	}

	lastBallSpeed = ballSpeed;

	if ((tickCountNow - timeIndividualStarted) >= 30000 && trainHitOnly)
	{
		NLOG("Reached time limit of 30 seconds!");
		forceNewIndividual = true;
	}
	wasSwinging = currentlySwinging;
}

LLNeural::~LLNeural()
{
	NLOG("Deconstruct...")

}
