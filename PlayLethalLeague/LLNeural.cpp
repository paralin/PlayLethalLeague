#include "stdafx.h"
#include "LLNeural.h"
#include "Game.h"
#include <Shlwapi.h>

#define NLOG(MSGG) LOG("[LLNeural] " << MSGG)
// test each individual twice (2 lives)
#define NUM_TESTS_PER_ITERATION 2
#define ACTIVATE_DEPTH 5

LLNeural::LLNeural(Game* game)
	: game(game),
	wasPlaying(false),
	testN(0),
	individualFitness(0),
	currentIndividual(0),
	currentSpecies(0)
{
	NLOG("Initializing...");

	// for inputs we want:
	// ball:
	//  0: ball position x
	//  1: ball position y
	//  2: ball velocity x
	//  3: ball velocity y
	//  4: ball tag, simplified (either 0 or 1)
	//  5: ball state, simplified 0: moving, 1: hitlag, 2: bunted
	// player:
	//  6: player 1 position x
	//  7: player 1 position y
	//  8: player 2 position x
	//  9: player 2 position y
	// total 10 inputs
	inputs.resize(10);

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

void LLNeural::loadFromFilesystem()
{
	NLOG(" == initializing from saved params == ");
	pop = std::make_shared<NEAT::Population>(populationPath);
}

void LLNeural::initFromScratch()
{
	NLOG(" == initializing net from scratch ==");
	NEAT::Parameters params;

	params.PopulationSize = 50;
	params.DynamicCompatibility = true;
	params.WeightDiffCoeff = 4.0;
	params.CompatTreshold = 2.0;
	params.YoungAgeTreshold = 15;
	params.SpeciesMaxStagnation = 15;
	params.OldAgeTreshold = 35;
	params.MinSpecies = 5;
	params.MaxSpecies = 25;
	params.RouletteWheelSelection = false;
	params.RecurrentProb = 0.0;
	params.OverallMutationRate = 0.8;

	params.MutateWeightsProb = 0.90;

	params.WeightMutationMaxPower = 2.5;
	params.WeightReplacementMaxPower = 5.0;
	params.MutateWeightsSevereProb = 0.5;
	params.WeightMutationRate = 0.25;

	params.MaxWeight = 8;

	params.MutateAddNeuronProb = 0.03;
	params.MutateAddLinkProb = 0.05;
	params.MutateRemLinkProb = 0.0;

	params.MinActivationA = 4.9;
	params.MaxActivationA = 4.9;

	params.ActivationFunction_SignedSigmoid_Prob = 0.0;
	params.ActivationFunction_UnsignedSigmoid_Prob = 1.0;
	params.ActivationFunction_Tanh_Prob = 0.0;
	params.ActivationFunction_SignedStep_Prob = 0.0;

	params.CrossoverRate = 0.75;
	params.MultipointCrossoverRate = 0.4;
	params.SurvivalRate = 0.2;

	genome = std::make_shared<NEAT::Genome>(0, inputs.size(), 0, 7, false, NEAT::UNSIGNED_SIGMOID, NEAT::UNSIGNED_SIGMOID, 0, params);
	// seed is time(0). for now just randomize it.
	pop = std::make_shared<NEAT::Population>(*genome, params, true, 1.0, time(nullptr));
	
}

void LLNeural::newMatchStarted()
{
	NLOG("Re-init neural network for new match.");
	NLOG("Note: we don't do anything here.");
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
		NLOG("End of life, resetting stocks to 2.");

		if (game->players[0].state.lives > game->players[1].state.lives)
		{
			NLOG("We killed him! +200");
			individualFitness += 200;
		}

		game->setPlayerLives(0, 2);
		game->setPlayerLives(1, 2);

		testN++;
		if (testN >= NUM_TESTS_PER_ITERATION)
		{
			NLOG("== finished individual ==");
			game->resetPlayerHitCounters(0);
			game->resetPlayerHitCounters(1);

			game->resetPlayerBuntCounters(0);
			game->resetPlayerBuntCounters(1);

			lastBuntCount = lastHitCount = testN = 0;

			NLOG("Fitness: " << std::dec << individualFitness);

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
					NLOG("Best fitness last evolution: " << std::dec << pop->GetBestFitnessEver());
					NLOG("Evolving! Sexytime is currently occuring...");
					pop->Epoch();
					currentSpecies = 0;
				}
				NLOG("Species: " << std::dec << (currentSpecies + 1) << "/" << pop->m_Species.size());
			}

			NLOG("Individual: " << std::dec << (currentIndividual + 1) << "/" << pop->m_Species[currentSpecies].NumIndividuals());
			// reset it
			currentNet.reset();
			saveToFile();
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

	// Set the inputs based on current state
	inputs[0] = game->localBallCoords.xcoord;
	inputs[1] = game->localBallCoords.ycoord;
	inputs[2] = game->localBallState.xspeed;
	inputs[3] = game->localBallState.yspeed;
	inputs[4] = game->localBallState.ballTag == 0 ? 0x0 : 0x01;
	inputs[5] = 0;
	const int& ballState = game->localBallState.state;
	if (ballState == 10)
		inputs[5] = 0x01;
	else if (ballState == 6)
		inputs[5] = 0x02;
	inputs[6] = game->players[0].coords.xcoord;
	inputs[7] = game->players[0].coords.ycoord;
	inputs[8] = game->players[1].coords.xcoord;
	inputs[9] = game->players[1].coords.ycoord;

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
	if (hits != lastHitCount)
	{
		lastHitCount = hits;
		NLOG("We hit the ball! +50! Hits: " << lastHitCount);
		individualFitness += 50;
	}

	auto& bunts = game->players[0].state.bunt_counter;
	if (bunts != lastBuntCount)
	{
		lastBuntCount = bunts;
		NLOG("We bunted the ball! +50! Bunts: " << lastBuntCount);
		individualFitness += 50;
	}
}

LLNeural::~LLNeural()
{
	NLOG("Deconstruct...")

}
