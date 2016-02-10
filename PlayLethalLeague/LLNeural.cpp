#include "stdafx.h"
#include "LLNeural.h"
#include "Game.h"
#include <Shlwapi.h>

#define NLOG(MSGG) LOG("[LLNeural] " << MSGG)
// test each individual twice (2 lives)
#define MAX_TESTS_PER_ITERATION 10
#define NUM_DEATHS_PER_ITERATION 1
#define ACTIVATE_DEPTH 8 

LLNeural::LLNeural(Game* game)
	: game(game),
	wasPlaying(false),
	testN(0),
	individualFitness(0),
	currentIndividual(0),
	currentSpecies(0),
	deathCount(0),
	lastBuntCount(0),
	lastHitCount(0)
{
	NLOG("Initializing...");
	inputs.resize(13);

	// outputs:
	// (consider > 0 = on, < 0 = off)
	//   0: left arrow key
	//   1: right arrow key
	//   2: up arrow key
	//   3: down arrow key
	//   4: jump
	//   5: attack
	//   6: bunt

	if (PathFileExists(populationPath) != 0)
	{
		loadFromFilesystem();
	}
	else
		initFromScratch();
}


NEAT::Parameters getParams()
{
	NEAT::Parameters params;

	params.PopulationSize = 50;
	params.DynamicCompatibility = true;
	params.WeightDiffCoeff = 4.0;
	params.CompatTreshold = 2.0;
	params.YoungAgeTreshold = 1;
	params.SpeciesMaxStagnation = 3;
	params.MinSpecies = 3;
	params.MaxSpecies = 10;
	params.RouletteWheelSelection = false;
	params.RecurrentProb = 0.00;
	params.OverallMutationRate = 0.9;

	params.StagnationDelta = 20;
	params.DeltaCoding = false;

	params.OldAgeTreshold = 30;

	params.PhasedSearching = false;
	params.InnovationsForever = true;

	params.MutateWeightsProb = 0.90;

	params.WeightMutationMaxPower = 2.5;
	params.WeightReplacementMaxPower = 6.0;
	params.MutateWeightsSevereProb = 0.5;
	params.WeightMutationRate = 0.25;

	params.MaxWeight = 8;

	params.MutateAddNeuronProb = 0.05;
	params.MutateAddLinkProb = 0.06;
	params.MutateRemLinkProb = 0.01;

	params.MinActivationA = 4.0;
	params.MaxActivationA = 4.9;

	params.ActivationFunction_SignedSigmoid_Prob = 0.00;
	params.ActivationFunction_UnsignedSigmoid_Prob = 0.25;
	params.ActivationFunction_Relu_Prob = 0.25;
	params.ActivationFunction_Tanh_Prob = 0.25;
	params.ActivationFunction_SignedStep_Prob = 0.25;

	// consider changing this
	params.AllowClones = true;

	params.CrossoverRate = 0.75;
	params.MultipointCrossoverRate = 0.4;
	params.SurvivalRate = 0.10;
	params.EliteFraction = 0.05;

	return params;
}

void LLNeural::loadFromFilesystem()
{
	NLOG(" == initializing from saved params == ");
	pop = std::make_shared<NEAT::Population>(populationPath);
	// pop->m_Parameters = getParams();
	calcBestFitnessEver();
}

void LLNeural::initFromScratch()
{
	NLOG(" == initializing net from scratch ==");

	NEAT::Parameters params = getParams();
	genome = std::make_shared<NEAT::Genome>(0, inputs.size(), 0, 7, false, NEAT::UNSIGNED_SIGMOID, NEAT::UNSIGNED_SIGMOID, 0, params);
	// seed is time(0). for now just randomize it.
	pop = std::make_shared<NEAT::Population>(*genome, params, true, 1.0, time(nullptr));
	bestFitnessEver = 0;
}

void LLNeural::calcBestFitnessEver()
{
	if (!pop)
		return;
	bestFitnessEver = 0;
	for (int s = 0; s < pop->m_Species.size(); s++)
	{
		for (int i = 0; i < pop->m_Species[s].NumIndividuals(); i++)
		{
			auto fit = pop->m_Species[s].m_Individuals[i].GetFitness();
			if (fit > bestFitnessEver)
				bestFitnessEver = fit;
		}
	}
}


void LLNeural::newMatchStarted()
{
	NLOG("Re-init neural network for new match.");
	lastBuntCount = lastHitCount = testN = 0;
	deathCount = 0;
	currentIndividual = currentSpecies = 0;
	currentNet.reset();
	wasPlaying = false;
	bestFitnessEver = 0;
	TIME_POINT no = CLOCK_U::now();
	game->holdInputUntil(CONTROL_JUMP, no + std::chrono::milliseconds(500));
}

void LLNeural::saveToFile() const
{
	pop->Save(populationPath);
}

void LLNeural::playOneFrame()
{
	const GameOffsetStorage* offs = game->localOffsetStorage;

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

	bool playing = !game->players[0].state.respawn_timer
		&& game->localBallState.state != 14
		&& game->localBallState.state != 1;

	if (!playing && wasPlaying)
	{
		wasPlaying = playing;

		if (game->players[0].state.lives > game->players[1].state.lives)
		{
			NLOG("We killed him! +200");
			individualFitness += 200;
		}
		else
			deathCount++;

		NLOG("End of life, resetting stocks to 2. Current deaths: " << deathCount);

		game->setPlayerLives(0, 2);
		game->setPlayerLives(1, 2);
		ballIsBunted = false;

		testN++;
		if (deathCount >= NUM_DEATHS_PER_ITERATION || testN >= MAX_TESTS_PER_ITERATION)
		{
			NLOG("== finished individual ==");
			NLOG("Fitness: " << std::dec << individualFitness);
			if (individualFitness > bestFitnessEver)
				bestFitnessEver = individualFitness;
			NLOG("Best ever fitness: " << std::dec << bestFitnessEver);

			game->resetPlayerHitCounters(0);
			game->resetPlayerHitCounters(1);

			game->resetPlayerBuntCounters(0);
			game->resetPlayerBuntCounters(1);

			lastBuntCount = lastHitCount = testN = 0;
			deathCount = 0;

			pop->m_Species[currentSpecies].m_Individuals[currentIndividual].SetFitness(individualFitness);
			pop->m_Species[currentSpecies].m_Individuals[currentIndividual].SetEvaluated();

			individualFitness = 0;
			currentIndividual++;
			if (currentIndividual == pop->m_Species[currentSpecies].NumIndividuals())
			{
				NLOG("===== FINISHED SPECIES =====");
				currentIndividual = 0;
				currentSpecies++;
				if (currentSpecies == pop->m_Species.size())
				{
					NLOG("===== FINISHED EVOLUTION =====");
					NLOG("Best fitness: " << std::dec << bestFitnessEver);
					saveToFile();
					NLOG("Evolving!");
					game->localOffsetStorage->forcedInputs[1] = 0xFF;
					game->writeInputOverrides();
					Sleep(100);
					game->localOffsetStorage->forcedInputs[1] = 0x00;
					game->writeInputOverrides();
					pop->Epoch();
					game->localOffsetStorage->forcedInputs[1] = 0xFF;
					game->writeInputOverrides();
					Sleep(100);
					game->localOffsetStorage->forcedInputs[1] = 0x00;
					game->writeInputOverrides();
					currentSpecies = 0;
				}
			}

			NLOG("Now testing:")
				NLOG("Species: " << std::dec << (currentSpecies + 1) << "/" << pop->m_Species.size());
			NLOG("Individual: " << std::dec << (currentIndividual + 1) << "/" << pop->m_Species[currentSpecies].NumIndividuals());
			// reset it
			currentNet.reset();
			pop->Save("population_evolving.dat");
		}
	}
	if (!playing)
		return;
	wasPlaying = true;

	// Decide how to play!
	if (!currentNet)
	{
		currentNet = std::make_shared<NEAT::NeuralNetwork>();
		pop->m_Species[currentSpecies].m_Individuals[currentIndividual].BuildPhenotype(*currentNet);
	}

	//  0: ball position x
	//  1: ball position y
	//  2: ball direction
	//  3: ball tag, simplified (either 0 or 1)
	//  4: ball state, simplified 0: moving, 1: hitlag, 2: bunted
	// player:
	//  5: player 1 position x
	//  6: player 1 position y
	//  6: player 1 direction
	//  7: player 1 meter
	//  8: player 2 position x
	//  9: player 2 position y
	//  9: player 2 direction
	//  10:player 2 meter
	// Set the inputs based on current state
	auto& stageX = game->stage.x_origin;
	inputs[0] = game->localBallCoords.xcoord - stageX;
	inputs[1] = game->localBallCoords.ycoord;
	inputs[2] = game->localBallState.direction;
	inputs[3] = game->localBallState.ballTag == 0 ? 0x0 : 0x01;
	inputs[4] = 0;
	const int& ballState = game->localBallState.state;
	if (ballState == 10)
		inputs[4] = 0x01;
	else if (ballState == 6)
		inputs[4] = 0x02;
	inputs[5] = game->players[0].coords.xcoord - stageX;
	inputs[6] = game->players[0].coords.ycoord;
	inputs[7] = game->players[0].state.facing_direction;
	inputs[8] = game->players[0].state.special_meter;
	inputs[9] = game->players[1].coords.xcoord - stageX;
	inputs[10] =game->players[1].coords.ycoord;
	inputs[11] =game->players[1].state.special_meter;
	inputs[12] =game->players[1].state.special_meter;

	bool ballCurrentlyBunted = inputs[4] == 0x01;
	if (!ballCurrentlyBunted && ballIsBunted)
	{
		// ball exited bunt
		// store the time
		timeBallNotBunted = CLOCK_U::now();
	}
	ballIsBunted = ballCurrentlyBunted;

	currentNet->Flush();
	currentNet->Input(inputs);
	for (int i = 0; i < ACTIVATE_DEPTH; i++)
		currentNet->Activate();
	auto out = currentNet->Output();
	// outputs:
	// (consider > 0 = on, < 0 = off)
	//   0: left arrow key
	//   1: right arrow key
	//   2: up arrow key
	//   3: down arrow key
	//   4: jump
	//   5: attack
	//   6: bunt
	game->setInputImmediate(CONTROL_LEFT, out[0] > 0);
	game->setInputImmediate(CONTROL_RIGHT, out[1] > 0);
	game->setInputImmediate(CONTROL_UP, out[2] > 0);
	game->setInputImmediate(CONTROL_DOWN, out[3] > 0);
	game->setInputImmediate(CONTROL_JUMP, out[4] > 0);
	game->setInputImmediate(CONTROL_ATTACK, out[5] > 0);
	game->setInputImmediate(CONTROL_BUNT, out[6] > 0);
	// inputs will be committed in main()

	// Calculate points!
	auto& hits = game->players[0].state.total_hit_counter;
	auto& bunts = game->players[0].state.bunt_counter;
	if (hits != lastHitCount)
	{
		lastHitCount = hits;
		auto speed = game->localBallState.ballSpeed;
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
			NLOG("We hit the ball at speed " << speed << "! +" << points << "! Hits: " << (lastHitCount - lastBuntCount));
			if (ballIsBunted || (timeBallNotBunted > CLOCK_U::now() - std::chrono::milliseconds(50)))
			{
				timeBallNotBunted = timeBallNotBunted.min();
				NLOG(" - hit was a bunted ball! POINT MULTIPLIER x2!");
				points *= 2;
			}
			individualFitness += points;
		}
	}
}

LLNeural::~LLNeural()
{
	NLOG("Deconstruct...")

}
