#pragma once
#include "Game.h"
#include "../thirdparty/MultiNEAT/src/Parameters.h"
#include "../thirdparty/MultiNEAT/src/Random.h"
#include "../thirdparty/MultiNEAT/src/Genome.h"
#include "../thirdparty/MultiNEAT/src/Population.h"

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
	bool wasPlaying;

	int testN;
	int individualFitness;

	int currentSpecies;
	int currentIndividual;

	int lastHitCount;
	int lastBuntCount;
	int deathCount;
	int bestFitnessEver;

	const char* populationPath = "population.dat";

	std::shared_ptr<NEAT::NeuralNetwork> currentNet;

	std::vector<double> inputs;

public:
	void newMatchStarted();
	void playOneFrame();
	void saveToFile() const;
	void initFromScratch();
	void loadFromFilesystem();
	void calcBestFitnessEver();
};

