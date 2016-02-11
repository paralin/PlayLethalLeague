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
	lastHitCount(0),
	bestAccuracy(0),
	swingCount(0),
	bestFitnessThisSpecies(0),
	hitsThisIndividual(0)
{
	NLOG("Initializing...");
	inputs.resize(5);

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
	params.MaxSpecies = 12;
	params.RouletteWheelSelection = false;
	params.RecurrentProb = 0.0;
	params.OverallMutationRate = 0.8;

	params.StagnationDelta = 20;
	params.DeltaCoding = false;

	params.OldAgeTreshold = 30;

	params.PhasedSearching = false;
	params.InnovationsForever = false;

	params.MutateWeightsProb = 0.90;

	params.WeightMutationMaxPower = 2.5;
	params.WeightReplacementMaxPower = 6.0;
	params.MutateWeightsSevereProb = 0.2;
	params.WeightMutationRate = 0.25;

	params.MaxWeight = 8;

	params.MutateAddNeuronProb = 0.05;
	params.MutateAddLinkProb = 0.06;
	params.MutateRemLinkProb = 0.01;

	params.MinActivationA = 4.9;
	params.MaxActivationA = 4.9;

	params.ActivationFunction_SignedSigmoid_Prob = 0.25;
	params.ActivationFunction_UnsignedSigmoid_Prob = 0;
	params.ActivationFunction_Relu_Prob = 0.25;
	params.ActivationFunction_Tanh_Prob = 0.25;
	params.ActivationFunction_SignedStep_Prob = 0.25;

	// consider changing this
	params.AllowClones = false;

	params.TrainingHitOnly = true;
	params.TargetAccuracy = 0.8;

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
	genome = std::make_shared<NEAT::Genome>(0, inputs.size(), 0, 5, false, NEAT::SIGNED_SIGMOID, NEAT::SIGNED_SIGMOID, 0, params);
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
	lastFitness = individualFitness = 0;
	currentIndividual = currentSpecies = 0;
	timeSinceLastFitness = CLOCK_U::now();
	wasSwinging = false;
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
	int ballSpeed = game->localBallState.ballSpeed;

	char tarP2Exist = pop->m_Parameters.TrainingHitOnly ? 0x0 : 0x01;
	game->setPlayerExists(1, tarP2Exist);

	if (!playing && wasPlaying)
	{
		wasPlaying = playing;

		if (game->players[0].state.lives > game->players[1].state.lives)
		{
			NLOG("We killed him! +300");
			individualFitness += 300;
			game->resetInputs();
			game->sendTaunt();
		}
		else
			deathCount++;

		NLOG("End of life, resetting stocks to 18. Current deaths: " << deathCount);

		game->setPlayerLives(0, 18);
		game->setPlayerLives(1, 18);
		ballIsBunted = false;
		wasSwinging = false;
		lastBallSpeed = 0;

		testN++;
		if (deathCount >= NUM_DEATHS_PER_ITERATION || testN >= MAX_TESTS_PER_ITERATION)
		{
			NLOG("== finished individual ==");
			NLOG("Fitness: " << std::dec << individualFitness);
			if (individualFitness > bestFitnessEver)
				bestFitnessEver = individualFitness;

			double accur;
			if (hitsThisIndividual == 0)
			{
				NLOG("We didn't hit even once, forcing fitness to zero.");
				individualFitness = 0;
			}
			else
			{
				accur = hitsThisIndividual / swingCount;
				NLOG("Accuracy: " << accur);

			}

			NLOG("Best ever: " << std::dec << bestFitnessEver);

			game->resetPlayerHitCounters(0);
			game->resetPlayerHitCounters(1);

			game->resetPlayerBuntCounters(0);
			game->resetPlayerBuntCounters(1);

			timeSinceLastFitness = CLOCK_U::now();
			lastFitness = 0;
			individualFitness = 0;
			hitsThisIndividual = 0;
			lastBuntCount = lastHitCount = testN = 0;
			deathCount = 0;
			swingCount = 0;

			pop->m_Species[currentSpecies].m_Individuals[currentIndividual].SetFitness(individualFitness);
			pop->m_Species[currentSpecies].m_Individuals[currentIndividual].SetEvaluated();

			individualFitness = 0;
			currentIndividual++;
			if (currentIndividual == pop->m_Species[currentSpecies].NumIndividuals())
			{
				NLOG("===== FINISHED SPECIES =====");
				currentIndividual = 0;
				bestFitnessThisSpecies = 0;
				currentSpecies++;
				if (currentSpecies == pop->m_Species.size())
				{
					NLOG("===== FINISHED EVOLUTION =====");
					NLOG("Best fitness: " << std::dec << bestFitnessEver);
					saveToFile();
					game->resetInputs();
					game->sendTaunt();
					NLOG("Evolving!");
					pop->Epoch();
					currentSpecies = 0;
					game->setPlayerLives(0, 18);
					game->setPlayerLives(1, 18);
					bestAccuracy = 0;
				}
			}

			NLOG("Now testing:")
				NLOG("Species: " << std::dec << (currentSpecies + 1) << "/" << pop->m_Species.size());
			NLOG("Individual: " << std::dec << (currentIndividual + 1) << "/" << pop->m_Species[currentSpecies].NumIndividuals());
			// reset it
			currentNet.reset();
			pop->Save("population_evolving.dat");
			timeSinceLastFitness = CLOCK_U::now();
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

	//  0: player 1 position x
	//  1: player 1 position y
	//  2: player 1 direction 
	//  3: ball position rel x
	//  4: ball position rel y

	// temporarily disabled:
	//  5: player 1 meter
	//  6: player 2 position x
	//  7: player 2 position y
	//  8: ball position x
	//  9: ball position y
	//  10: ball direction
	//  11: ball tag
	//  12: ball speed
	//  13: hitlag timer
	//  14: ball state, simplified 0: moving, 1: hitlag, 2: bunted
	// player:
	// Set the inputs based on current state
	// Map positional inputs to [-1, 1]
#define COORD_OFFSET 50000.0
#define TO_INPUT_RANGE(VAL, MAXVAL)  std::max(std::min(2.0 * ((((double)VAL) - (0.5 * MAXVAL)) / MAXVAL), 1.0), -1.0)
	// range is 0 -> sizeof(stage)
	// subtract 1/2 sizeof stage
	// range is from -.5sizeof(stage) -> .5sizeof(stage)
	// divide by sizeof stage
	// range is from -.5 -> .5
	// multiply by 2
	// range is from -1 to 1
	double stageX = game->stage.x_origin * COORD_OFFSET;
	double stageXSize = game->stage.x_size * COORD_OFFSET;
	double stageY = game->stage.y_origin * COORD_OFFSET;
	double stageYSize = game->stage.y_size * COORD_OFFSET;
	inputs[0] = TO_INPUT_RANGE(game->players[0].coords.xcoord - stageX, stageXSize);
	inputs[1] = TO_INPUT_RANGE(game->players[0].coords.ycoord - stageY, stageYSize);
	// 0 or 1
	inputs[2] = game->players[0].state.facing_direction ? -1 : 1;

	double ballPosX = TO_INPUT_RANGE(game->localBallCoords.xcoord - stageX, stageXSize);
	double ballPosY = TO_INPUT_RANGE(game->localBallCoords.ycoord - stageY, stageYSize);
	// ball pose rel x
	// ball pose rel y
	inputs[3] = ballPosX - inputs[0];
	inputs[4] = ballPosY - inputs[1];

	/*
	// 4 charges
	inputs[3] = TO_INPUT_RANGE(game->players[0].state.special_meter, 4.0);
	inputs[4] = TO_INPUT_RANGE(game->players[1].coords.xcoord - stageX, stageXSize);
	inputs[5] = TO_INPUT_RANGE(game->players[1].coords.ycoord - stageY, stageYSize);
	inputs[6] = ballPosX
	inputs[7] = 
	// if we make the pose of the ball to -1, 1 based on location in stage
	// then we subtract by the player pose 
	// should be relative, O_O
	inputs[8] = inputs[6] - inputs[0];
	inputs[9] = inputs[7] - inputs[1];
	inputs[10] = TO_INPUT_RANGE(game->localBallState.direction, 10.0);
	inputs[11] = game->localBallState.ballTag == 0 ? -0x01 : 0x01;
	// 0 to 1300
	// after 1300 the speed doesnt really matter since it travels the entire stage distance in 1 frame
	inputs[12] = std::min(TO_INPUT_RANGE(game->localBallState.ballSpeed, 1300), 1.0);
	// im assuming idk some amount of seconds
	inputs[13] = TO_INPUT_RANGE(game->localBallState.hitstunCooldown, 130000);
	if (game->localBallState.hitstunCooldown || ballState != 6) inputs[13] = -1;
	if (ballState == 10)
		inputs[14] = 0x01;
	else if (ballState == 6)
		inputs[14] = 0x02;
	else
		inputs[14] = 0;
		*/

	const int& ballState = game->localBallState.state;
	bool ballCurrentlyBunted = ballState == 10;
	if (!ballCurrentlyBunted && ballIsBunted)
	{
		// ball exited bunt
		// store the time
		timeBallNotBunted = CLOCK_U::now();
	}
	ballIsBunted = ballCurrentlyBunted;
	bool currentlySwinging = game->players[0].state.character_state == 0x01;

	currentNet->Flush();
	currentNet->Input(inputs);
	for (int i = 0; i < ACTIVATE_DEPTH; i++)
		currentNet->Activate();
	auto out = currentNet->Output();
	// outputs:
	// (consider > 0 = on, < 0 = off or otherwise for arrows)
	//   0: left arrow key & right arrow key
	//   1: up arrow key & down arrow key
	//   2: jump
	//   3: attack
	//   4: bunt
	game->setInputImmediate(CONTROL_LEFT, out[0] < -0.2);
	game->setInputImmediate(CONTROL_RIGHT, out[0] > 0.2);
	game->setInputImmediate(CONTROL_UP, out[1] > 0.2);
	game->setInputImmediate(CONTROL_DOWN, out[1] < -0.2);
	game->setInputImmediate(CONTROL_JUMP, out[2] > 0.2 || out[2] < -0.2);
	game->setInputImmediate(CONTROL_ATTACK, out[3] > 0.2 || out[3] < -0.2);
	game->setInputImmediate(CONTROL_BUNT, out[4] > 0.2 || out[4] < -0.2);
	// inputs will be committed in main()

	bool justFinishedSwinging = false;
	if (wasSwinging && !currentlySwinging)
		justFinishedSwinging = true;

	// Calculate points!
	auto& hits = game->players[0].state.total_hit_counter;
	auto& bunts = game->players[0].state.bunt_counter;

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
			if (ballIsBunted || (timeBallNotBunted > CLOCK_U::now() - std::chrono::milliseconds(50)))
			{
				timeBallNotBunted = timeBallNotBunted.min();
				NLOG(" - hit was a bunted ball! POINT MULTIPLIER x2!");
				points *= 2;
			}
			individualFitness += points;
		}
	} else if (justFinishedSwinging)
	{
		swingCount++;
		if (hits == hitsStartedSwinging)
		{
			NLOG("missed a swing, " << swingCount << "missed so far.");
		}
	}

	lastBallSpeed = ballSpeed;

	if (individualFitness != lastFitness)
	{
		if (individualFitness > lastFitness)
			timeSinceLastFitness = CLOCK_U::now();
		lastFitness = individualFitness;
	}

	if (timeSinceLastFitness < CLOCK_U::now() - std::chrono::seconds(30))
	{
		NLOG("No progress for 30 seconds!");
		game->localBallState.direction = 0;
		game->localBallState.ballSpeed = 8;
		game->localBallState.state = 8;
		game->localBallState.ballTag = 0x01;
		// re-init the net
		currentNet.reset();
		individualFitness = lastFitness = 0;
		timeSinceLastFitness = CLOCK_U::now();
	}
	wasSwinging = currentlySwinging;
}

LLNeural::~LLNeural()
{
	NLOG("Deconstruct...")

}
