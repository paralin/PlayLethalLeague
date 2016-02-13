#pragma once
#include "../thirdparty/MultiNEAT/src/Parameters.h"
#include "../thirdparty/MultiNEAT/src/Random.h"
#include "../thirdparty/MultiNEAT/src/Genome.h"
#include "../thirdparty/MultiNEAT/src/Population.h"
#include "Utils.h"
#include <mutex>

class Game;
class LLNeural
{
public:
	LLNeural(Game* game);
	~LLNeural();

private:
	Game* game;

	NEAT::Parameters neatParams;
	NEAT::RNG rng;

	std::shared_ptr<NEAT::Genome> genome;
	std::shared_ptr<NEAT::Population> pop;
	std::shared_ptr<NEAT::Substrate> substrate;

	void initSubstrate();

	bool wasPlaying;
	bool ballIsBunted;

	int testN;
	int individualFitness;

	int currentIndividual;

	int lastHitCount;
	int lastBuntCount;
	int deathCount;
	int bestFitnessEver;
	int lastBallSpeed;
	int swingCount;
	int bestFitnessThisSpecies;
	int hitsThisIndividual;

	DWORD timeIndividualStarted;
	int lastFitness = 0;

	const char* populationPath = "population.dat";

	std::pair<NEAT::Genome*, std::shared_ptr<NEAT::NeuralNetwork>> currentNet;

	std::vector<double> inputs;

	DWORD timeBallNotBunted;
	int hitsStartedSwinging;
	bool wasSwinging;
	double bestAccuracy;
	bool forceNewIndividual;
	DWORD timeNextUpdate;
	bool playedOneFrame;
	double maxAccuracyLastRound;
	DWORD resetLivesUntil;

	std::mutex logMutex;
	std::mutex queueAccessMutex;
	std::queue<std::pair<NEAT::Genome*, std::shared_ptr<NEAT::NeuralNetwork>>> individualQueue;
	std::queue<std::pair<NEAT::Genome*, std::shared_ptr<NEAT::NeuralNetwork>>> readyQueue;

	std::vector<std::thread> workQueueThreads;

	void workQueueThread();
	void queueIndividuals();

public:
	void newMatchStarted();
	void playOneFrame();
	void saveToFile() const;
	void initFromScratch();
	void loadFromFilesystem();
	void calcBestFitnessEver();
};

