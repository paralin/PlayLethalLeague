#include "stdafx.h"
#include "LLNeural.h"
#include "Game.h"
#include <Shlwapi.h>

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
	currentNet.first = 0;
	inputs.resize(12);
	newMatchStarted();

	if (PathFileExists(populationPath) != 0)
	{
		loadFromFilesystem();
	}
	else
		initFromScratch();

	// for (int i = 0; i < 2; i++)
		workQueueThreads.push_back(std::thread(&LLNeural::workQueueThread, this));
}

void LLNeural::workQueueThread()
{
	JLOG("Starting work queue thread...");
	while (true)
	{
		std::pair<NEAT::Genome*, std::shared_ptr<NEAT::NeuralNetwork>> pair;
		bool wasEmpty = false;
		{
			std::lock_guard<std::mutex> lock_q(queueAccessMutex);
			if (individualQueue.empty())
			{
				wasEmpty = true;
			} else
			{
				pair = individualQueue.front();
				individualQueue.pop();
			}
		}

		if (wasEmpty)
		{
			Sleep(1000);
			continue;
		}

		JLOG("[" << pair.first->GetID() << "] Calculating nn phenotype");
		pair.first->BuildESHyperNEATPhenotype(*pair.second, *substrate, pop->m_Parameters);
		JLOG("[" << pair.first->GetID() << "] Done.");
		readyQueue.push(pair);
	}
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
	params.WeightMutationMaxPower = 1;
	params.WeightReplacementMaxPower = 5.0;
	params.MutateWeightsSevereProb = 0.1;
	params.WeightMutationRate = 0.9;
	params.MaxWeight = 8.0;
	params.MinActivationA = 4.0;
	params.MaxActivationA = 5.0;
	params.ActivationFunction_UnsignedSigmoid_Prob = 0.0;
	params.ActivationFunction_Relu_Prob = 0.0;
	params.ActivationFunction_Abs_Prob = 0.0;
	params.ActivationFunction_Softplus_Prob = 0.0;
	params.ActivationFunction_Linear_Prob = 1.0;
	params.ActivationFunction_SignedSigmoid_Prob = 1.0;
	params.ActivationFunction_SignedSine_Prob = 1.0;
	params.ActivationFunction_SignedGauss_Prob = 1.0;
	params.ActivationFunction_Tanh_Prob = 1.0;
	params.MutateAddNeuronProb = 0.02;
	params.MutateAddLinkProb = 0.08;
	params.MutateRemLinkProb = 0.01;
	params.DivisionThreshold = 0.03;
	params.VarianceThreshold = 0.03;
	params.BandThreshold = 0.3;
	params.InitialDepth = 2;
	params.MaxDepth = ACTIVATE_DEPTH;
	params.IterationLevel = 1;
	params.Leo = false;
	params.GeometrySeed = false; // variable not used anywhere? wtf
	params.LeoSeed = false;
	params.LeoThreshold = 0.3;
	params.CPPN_Bias = -1.0;
	params.Qtree_X = 0;
	params.Qtree_Y = 0;
	params.Width = 1.0;
	params.Height = 1.0;
	params.EliteFraction = 0.05;
	params.CrossoverRate = 0.5;
	params.MutateWeightsSevereProb = 0.01;
	params.SurvivalRate = 0.05;

	params.TrainingHitOnly = false;
	params.TargetAccuracy = 0.8;

	params.SimplifyingPhaseStagnationTreshold = 30;
	params.ComplexityFloorGenerations = 2;

	params.NoveltySearch_Recompute_Sparseness_Each = 5;
	params.NoveltySearch_Quick_Archiving_Min_Evaluations = 2;

	params.PhasedSearching = false;

	return params;
}

void LLNeural::loadFromFilesystem()
{
	NLOG(" == initializing from saved params == ");
	pop = std::make_shared<NEAT::Population>(populationPath);
	// pop->m_Parameters = getParams();
	calcBestFitnessEver();
	initSubstrate();
#ifndef PLAY_BEST
	if (pop->m_Species[0].m_Individuals[0].IsEvaluated())
		pop->Epoch();
#endif
	queueIndividuals();
}

#define VECTOR3(TARG, X, Y, Z) { \
	std::vector<double> inputssb; \
	inputssb.push_back(X); \
	inputssb.push_back(Y); \
	inputssb.push_back(Z); \
	TARG.push_back(inputssb); \
}

void LLNeural::initSubstrate()
{
	// xcoordinate is used for property type
	//  - xcoord influence: -1
	//  - ycoord influence: -0.5
	//  - ball influence:  0.5
	//  - unrelated:   1
	// ycoordinate is used for i/o
	//    input: 0
	//    output: 1
	auto& params = pop->m_Parameters;
	/* y coord is just -1 for inputs and 1 for outputs
		- 0: player xcoord (-1, -1, -1)
		- 1: player y coord (0, -1, -1)
		- 2: horizontal speed (-1, -1, -1)
		- 3: vertical speed (0, -1, -1)
		- 4: ballposx (-1, -1, 0)
		- 5: ballposy (0, -1, 0)
		- 6: ball speed (-0.5, -1, 0)
		- 7: ball direction (0.5, -1, 0)
		- 8: facing direction (-1, -1, -1)
		- 9:  player 1 posx (-1, -1, 1)
		- 10: player 1 posy (0, -1, 1)
		- 11: 1.0 constant (1, -1, 1)
	*/
	std::vector<std::vector<double>> inputss;

	VECTOR3(inputss, -1, -1, -1);
	VECTOR2(inputss, 0, -1, -1);
	VECTOR2(inputss, -1, -1, -1);
	VECTOR2(inputss, 0, -1, -1);
	VECTOR2(inputss, -1, -1, 0);
	VECTOR2(inputss, 0, -1, 0);
	VECTOR2(inputss, -0.5, -1, 0);
	VECTOR2(inputss, 0.5, -1, 0);
	VECTOR2(inputss, -1, 0);
	VECTOR2(inputss, 1, 0);
	VECTOR2(inputss, 1, 0);
	VECTOR2(inputss, 1, 0);

	std::vector<std::vector<double>> hidden;

	// xcoordinate is used for property type
	//  - xcoord influence: -1
	//  - ycoord influence: -0.5
	//  - ball influence:  0.5
	//  - unrelated:   1
	//   0: left arrow key & right arrow key (-1, 1, -1)
	//   1: up arrow key & down arrow key (0, 1, -1)
	//   2: jump (-0.5, 1, -1)
	//   3: attack (0, 1, 0)
	//   4: bunt (0, 1, 0.5)
	//   5: unused
	std::vector<std::vector<double>> outputss;

	VECTOR2(outputss, -1, 1);
	VECTOR2(outputss, 0.5, 1);
	VECTOR2(outputss, -0.5, 1);
	VECTOR2(outputss, 0.5, 1);
	VECTOR2(outputss, 0.5, 1);
	VECTOR2(outputss, 1, 1);

	substrate = std::make_shared<NEAT::Substrate>(inputss, hidden, outputss);
	substrate->m_max_weight_and_bias = params.MaxWeight;
	substrate->m_with_distance = false;
	substrate->m_allow_input_hidden_links = true;
	substrate->m_allow_hidden_output_links = true;
	substrate->m_allow_input_output_links = true;
	substrate->m_allow_hidden_hidden_links = false;
	substrate->m_allow_looped_hidden_links = false;
	substrate->m_allow_looped_output_links = false;
	substrate->m_hidden_nodes_activation = NEAT::SIGNED_SIGMOID;
	substrate->m_output_nodes_activation = NEAT::SIGNED_SIGMOID;
}

void LLNeural::initFromScratch()
{
	NLOG(" == initializing net from scratch ==");
	NEAT::Parameters params = getParams();
	genome = std::make_shared<NEAT::Genome>(0, inputs.size(), 0, 5, false, NEAT::SIGNED_SIGMOID, NEAT::SIGNED_SIGMOID, 0, params);
	// seed is time(0). for now just randomize it.
	pop = std::make_shared<NEAT::Population>(*genome, params, true, 1.0, time(nullptr));
	bestFitnessEver = 0;
	initSubstrate();
	queueIndividuals();
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
	currentIndividual = 0;
	currentIndividual = 0;
	forceNewIndividual = false;
	timeIndividualStarted = 0;
	wasSwinging = false;
	currentNet.first = nullptr;
	individualQueue = std::queue<std::pair<NEAT::Genome*, std::shared_ptr<NEAT::NeuralNetwork>>>();
	wasPlaying = false;
	playedOneFrame = false;
	bestFitnessEver = 0;
	maxAccuracyLastRound = 0;
	// game->holdInputUntil(CONTROL_JUMP, no + std::chrono::milliseconds(500));

#ifdef PLAY_BEST
	loadFromFilesystem();
	for (int i = 0; i < pop->m_Species.size(); i++)
	{
		for (int x = 0; x < pop->m_Species[i].m_Individuals.size(); x++)
		{
			auto& ind = pop->m_Species[i].m_Individuals[x];
			if (!best_individual || ind.GetFitness() > best_individual->GetFitness())
				best_individual = &ind;
		}
}
#endif
}

void LLNeural::saveToFile() const
{
	pop->Save(populationPath);
}

void LLNeural::queueIndividuals()
{
	std::lock_guard<std::mutex> qlock(queueAccessMutex);
	for (auto it = pop->m_Species.begin(); it != pop->m_Species.end(); ++it)
		for (auto id = it->m_Individuals.begin(); id != it->m_Individuals.end(); ++id)
			individualQueue.push(std::pair<NEAT::Genome*, std::shared_ptr<NEAT::NeuralNetwork>>(id._Ptr, std::make_shared<NEAT::NeuralNetwork>()));
}

void LLNeural::playOneFrame()
{
	const GameStorage* gd = game->gameData;
	DWORD tickCountNow = GetTickCount();


	if (!currentNet.first)
	{
		if (readyQueue.empty())
		{
			if (gd->ball_state->state == 14 && gd->ball_state->serveResetCounter < 300000)
				return;
			if (game->gameData->forcedInputs[0])
				NLOG("Waiting for next phenotype...");
			gd->ball_state->state = 14;
			gd->ball_state->serveResetCounter = 10;
			game->gameData->forcedInputs[0] = 0x00;
			game->gameData->forcedInputs[1] = 0xFF;
			return;
		}
		currentNet = readyQueue.front();
		readyQueue.pop();
		NLOG("Dequeued genome ID " << currentNet.first->GetID());
		game->gameData->forcedInputs[1] = 0;
	}

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

#ifndef PLAY_BEST
	// if (pop->m_Parameters.PhasedSearching)
		// pop->m_Parameters.TrainingHitOnly = pop->m_SearchMode == NEAT::SIMPLIFYING;
	pop->m_Parameters.TrainingHitOnly = false;
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
		if (pop->m_Parameters.TrainingHitOnly)
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
			NLOG("== finished individual " << currentIndividual << "/" << pop->m_Parameters.PopulationSize << " ==");
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

			currentNet.first->SetFitness(individualFitness);
			currentNet.first->SetEvaluated();

			timeIndividualStarted = 0;
			lastFitness = 0;
			hitsThisIndividual = 0;
			lastBuntCount = lastHitCount = testN = 0;
			deathCount = 0;
			swingCount = 0;

			individualFitness = 0;
			currentIndividual++;
			if (currentIndividual == pop->m_Parameters.PopulationSize)
			{
				NLOG("===== FINISHED EVOLUTION =====");
				NLOG("Best fitness: " << std::dec << bestFitnessEver);
				saveToFile();
				game->resetInputs();

				if (!pop->m_Parameters.PhasedSearching)
					pop->m_Parameters.TrainingHitOnly = maxAccuracyLastRound < 0.8;
				else
				{
					std::cout << "Current search mode: " << pop->m_SearchMode << " ";
					switch (pop->m_SearchMode)
					{
					case NEAT::COMPLEXIFYING:
						std::cout << "(Complexifying)";
						break;
					case NEAT::SIMPLIFYING:
						std::cout << "(Simplifying)";
						break;
					case NEAT::BLENDED:
						std::cout << "(Blended)";
						break;
					default:
						std::cout << "(Unknown)";
						break;
					}
				}
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
				currentIndividual = 0;
				forceNewIndividual = false;
				game->setPlayerLives(0, 18);
				game->setPlayerLives(1, 18);
				bestAccuracy = 0;
				// queue all the individuals
				queueIndividuals();
			}

			NLOG("Now testing:");
			NLOG("Individual: " << std::dec << (currentIndividual + 1) << "/" << pop->m_Parameters.PopulationSize);
			// reset it
			currentNet.first = nullptr;
			pop->Save("population_evolving.dat");
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
#else
	if (!playing)
		return;
#endif
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

	inputs[0] = TO_INPUT_RANGE(game->gameData->player_coords[0]->xcoord - stageX, stageXSize);
	inputs[1] = TO_INPUT_RANGE(game->gameData->player_coords[0]->ycoord - stageY, stageYSize);
	inputs[2] = TO_INPUT_RANGE(game->gameData->player_states[0]->horizontal_speed, game->gameData->player_bases[0]->max_speed);
	inputs[3] = TO_INPUT_RANGE(game->gameData->player_states[0]->vertical_speed, game->gameData->player_bases[0]->max_speed);

	// ball poses
	inputs[4] = ballPosX;
	inputs[5] = ballPosY;
	inputs[6] = TO_INPUT_RANGE((game->gameData->ball_state->state == 0) ? game->gameData->ball_state->ballSpeed : 0, 1300);
	inputs[7] = game->gameData->ball_state->direction;
	// bias
	inputs[8] = game->gameData->player_states[0]->facing_direction;
	inputs[9] = pop->m_Parameters.TrainingHitOnly ? 0 : TO_INPUT_RANGE(game->gameData->player_coords[1]->xcoord - stageX, stageXSize);
	inputs[10] = pop->m_Parameters.TrainingHitOnly ? 0 : TO_INPUT_RANGE(game->gameData->player_coords[1]->ycoord - stageY, stageYSize);
	inputs[11] = 1;

	// ball dir

	// add

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

	currentNet.second->Flush();
	currentNet.second->Input(inputs);
	for (int i = 0; i < ACTIVATE_DEPTH; i++)
		currentNet.second->Activate();
	auto out = currentNet.second->Output();
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

#ifndef PLAY_BEST	
	if ((tickCountNow - timeIndividualStarted) >= 30000 && pop->m_Parameters.TrainingHitOnly)
	{
		NLOG("Reached time limit of 30 seconds!");
		forceNewIndividual = true;
	}
#endif
	wasSwinging = currentlySwinging;
}

LLNeural::~LLNeural()
{
	NLOG("Deconstruct...")

}
