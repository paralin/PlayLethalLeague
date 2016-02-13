#include "stdafx.h"
#include "LLNeural.h"
#include "Game.h"
#include <Shlwapi.h>

#define NLOG(MSGG) LOG("[LLNeural] " << MSGG)
// test each individual twice (2 lives)
#define MAX_TESTS_PER_ITERATION 10
#define NUM_DEATHS_PER_ITERATION 1
#define ACTIVATE_DEPTH 1

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
	hitsThisIndividual(0),
	timeIndividualStarted(0),
	timeNextUpdate(0),
	playedOneFrame(false),
	maxAccuracyLastRound(0),
	timeBallNotBunted(0),
	forceNewIndividual(false),
	bestFitnessEver(0)
{
	NLOG("Initializing...");
	inputs.resize(8);
	newMatchStarted();

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
	params.AllowClones = false;

	params.CompatTreshold = 2.0;
	params.CompatTresholdModifier = 0.3;

	params.YoungAgeTreshold = 1;
	params.YoungAgeFitnessBoost = 1.01;
	params.SpeciesMaxStagnation = 10;
	params.OldAgeTreshold = 30;
	params.MinSpecies = 3;
	params.MaxSpecies = 10;
	params.RouletteWheelSelection = false;
	params.RecurrentProb = 0.0;
	params.OverallMutationRate = 0.9;
	params.MutateWeightsProb = 0.9;
	params.WeightMutationMaxPower = 1.0;
	params.WeightReplacementMaxPower = 5.0;
	params.MutateWeightsSevereProb = 0.2;
	params.WeightMutationRate = 0.75;
	params.MaxWeight = 20;
	params.MinActivationA = 9;
	params.MaxActivationA = 10;
	params.ActivationFunction_UnsignedSigmoid_Prob = 0.0;
	params.ActivationFunction_Relu_Prob = 0.0;
	params.ActivationFunction_Abs_Prob = 0.0;
	params.ActivationFunction_Softplus_Prob = 0.0;
	params.ActivationFunction_Linear_Prob = 0.0;
	params.ActivationFunction_SignedSigmoid_Prob = 1.0;
	params.MutateAddNeuronProb = 0.05;
	params.MutateAddLinkProb = 0.10;
	params.MutateRemLinkProb = 0.01;
	params.DivisionThreshold = 0.5;
	params.VarianceThreshold = 0.03;
	params.BandThreshold = 0.3;
	params.InitialDepth = 3;
	params.MaxDepth = 5;
	params.IterationLevel = 1;
	params.Leo = false;
	params.GeometrySeed = false; // variable not used anywhere? wtf
	params.LeoSeed = false;
	params.LeoThreshold = 0.3;
	// params.CPPN_Bias = -3.0;
	// params.Qtree_X = 0;
	// params.Qtree_Y = 0;
	// params.Width = 1.0;
	// params.Height = 1.0;
	params.EliteFraction = 0.05;
	params.CrossoverRate = 0.5;
	params.MutateWeightsSevereProb = 0.01;
	params.SurvivalRate = 0.05;

	params.TrainingHitOnly = true;
	params.TargetAccuracy = 0.8;

	params.SimplifyingPhaseStagnationTreshold = 30;
	params.ComplexityFloorGenerations = 2;

	params.NoveltySearch_Recompute_Sparseness_Each = 5;
	params.NoveltySearch_Quick_Archiving_Min_Evaluations = 2;

	return params;
}

void LLNeural::loadFromFilesystem()
{
	NLOG(" == initializing from saved params == ");
	pop = std::make_shared<NEAT::Population>(populationPath);
	// pop->m_Parameters = getParams();
	calcBestFitnessEver();
	if (pop->m_Species[0].m_Individuals[0].IsEvaluated())
		pop->Epoch();
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
	currentIndividual = 0;
	forceNewIndividual = false;
	timeIndividualStarted = 0;
	wasSwinging = false;
	currentNet.reset();
	wasPlaying = false;
	playedOneFrame = false;
	bestFitnessEver = 0;
	maxAccuracyLastRound = 0;
	// game->holdInputUntil(CONTROL_JUMP, no + std::chrono::milliseconds(500));
}

void LLNeural::saveToFile() const
{
	pop->Save(populationPath);
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

	char tarP2Exist = pop->m_Parameters.TrainingHitOnly ? 0x0 : 0x01;
	game->setPlayerExists(1, tarP2Exist);
	// game->gameData->decrementLifeOnDeath = pop->m_Parameters.TrainingHitOnly ? 0x0 : 0x01;

	if ((!playing && wasPlaying && !pop->m_Parameters.TrainingHitOnly) || (forceNewIndividual && pop->m_Parameters.TrainingHitOnly))
	{
		wasPlaying = playing;

		if (!forceNewIndividual)
		{
			if (gd->player_states[0]->lives > gd->player_states[1]->lives && tarP2Exist)
			{
				NLOG("We killed him! +300");
				individualFitness += 300;
				game->resetInputs();
				game->sendTaunt();
			}
			else
				deathCount++;

			NLOG("End of life, resetting stocks to 18. Current deaths: " << deathCount);
		} else
			NLOG("Next individual.");

		game->setPlayerLives(0, 18);
		game->setPlayerLives(1, 18);
		ballIsBunted = false;
		wasSwinging = false;
		lastBallSpeed = 0;

		// reset the ball
		gd->ball_state->state = 14;
		gd->ball_state->serveResetCounter = 300000;
		gd->player_states[0]->special_meter = 0;
		gd->player_states[1]->special_meter = 0;
		gd->player_states[0]->character_state = 3;
		gd->player_states[0]->respawn_timer = 200000;

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
				NLOG("Accuracy: " << accur);
				individualFitness *= accur;
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

			pop->m_Species[currentSpecies].m_Individuals[currentIndividual].SetFitness(individualFitness);
			pop->m_Species[currentSpecies].m_Individuals[currentIndividual].SetEvaluated();

			timeIndividualStarted = 0;
			lastFitness = 0;
			hitsThisIndividual = 0;
			lastBuntCount = lastHitCount = testN = 0;
			deathCount = 0;
			swingCount = 0;

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

					pop->m_Parameters.TrainingHitOnly = maxAccuracyLastRound < 0.8;
					maxAccuracyLastRound = 0;

					for (auto sit = pop->m_Species.begin(); sit != pop->m_Species.end();)
					{
						for (auto it = sit->m_Individuals.begin(); it != sit->m_Individuals.end();)
						{
							if (it->GetFitness() < 10)
							{
								NLOG("Erasing individual with " << it->GetFitness() << " fitness.");
								it = sit->m_Individuals.erase(it);
							}
							else
								++it;
						}
						if (sit->m_Individuals.size() == 0)
						{
							NLOG("Erasing species age " << sit->Age() << " with 0 good individuals.");
							sit = pop->m_Species.erase(sit);
						}
						else
							++sit;
					}

					NLOG("Evolving!");
					pop->Epoch();
					currentSpecies = currentIndividual = 0;
					forceNewIndividual = false;
					game->setPlayerLives(0, 18);
					game->setPlayerLives(1, 18);
					bestAccuracy = 0;
				}
			}

			NLOG("Now testing:");
				NLOG("Species: " << std::dec << (currentSpecies + 1) << "/" << pop->m_Species.size());
			NLOG("Individual: " << std::dec << (currentIndividual + 1) << "/" << pop->m_Species[currentSpecies].NumIndividuals());
			// reset it
			currentNet.reset();
			pop->Save("population_evolving.dat");
		}
		forceNewIndividual = false;
		timeIndividualStarted = tickCountNow;
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
	// Map positional inputs to [-1, 1]
#define COORD_OFFSET 50000.0
#define TO_INPUT_RANGE(VAL, MAXVAL)  std::max(std::min(2.0 * ((((double)VAL) - (0.5 * MAXVAL)) / MAXVAL), 1.0), -1.0)
	double stageX = game->gameData->stage_base->x_origin * COORD_OFFSET;
	double stageXSize = game->gameData->stage_base->x_size * COORD_OFFSET;
	double stageY = game->gameData->stage_base->y_origin * COORD_OFFSET;
	double stageYSize = game->gameData->stage_base->y_size * COORD_OFFSET;

	double ballPosX = TO_INPUT_RANGE(game->gameData->ball_coord->xcoord - stageX, stageXSize);
	double ballPosY = TO_INPUT_RANGE(game->gameData->ball_coord->ycoord - stageY, stageYSize);

	// inputs: player_x, player_y, player_vx, player_vy, ball_x, ball_y, ball_v
	inputs[0] = TO_INPUT_RANGE(game->gameData->player_coords[0]->xcoord - stageX, stageXSize);
	inputs[1] = TO_INPUT_RANGE(game->gameData->player_coords[0]->ycoord - stageY, stageYSize);
	inputs[2] = TO_INPUT_RANGE(game->gameData->player_states[0]->horizontal_speed, game->gameData->player_bases[0]->max_speed);
	inputs[3] = TO_INPUT_RANGE(game->gameData->player_states[0]->vertical_speed, game->gameData->player_bases[0]->max_speed);

	// ball poses
	inputs[4] = ballPosX;
	inputs[5] = ballPosY;
	inputs[6] = TO_INPUT_RANGE(game->gameData->ball_state->ballSpeed, 1300);
	inputs[7] = 1;

	// 0 to 1300
	// after 1300 the speed doesnt really matter since it travels the entire stage distance in 1 frame

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

	const int& ballState = gd->ball_state->state;
	bool ballCurrentlyBunted = ballState == 10;
	if (!ballCurrentlyBunted && ballIsBunted)
	{
		// ball exited bunt
		// store the time
		timeBallNotBunted = GetTickCount();
	}
	ballIsBunted = ballCurrentlyBunted;
	bool currentlySwinging = gd->player_states[0]->character_state == 0x01;

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
	game->setInputImmediate(CONTROL_LEFT, out[0] < -0.5);
	game->setInputImmediate(CONTROL_RIGHT, out[0] > 0.5);
	game->setInputImmediate(CONTROL_UP, out[1] > 0.5);
	game->setInputImmediate(CONTROL_DOWN, out[1] < -0.5);
	game->setInputImmediate(CONTROL_JUMP, out[2] > 0.5);
	game->setInputImmediate(CONTROL_ATTACK, out[3] > 0.5);
	game->setInputImmediate(CONTROL_BUNT, out[4] > 0.5);
	// inputs will be committed in main()

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
	} else if (justFinishedSwinging)
	{
		swingCount++;
		if (hits == hitsStartedSwinging)
		{
			NLOG("missed a swing, " << swingCount << " missed so far.");
		}
	}

	lastBallSpeed = ballSpeed;

	if ((tickCountNow - timeIndividualStarted) >= 30000)
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
