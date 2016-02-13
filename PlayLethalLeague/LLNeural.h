#pragma once
#include <mutex>

class Game;
class LLNeural
{
public:
	LLNeural(Game* game);
	~LLNeural();

private:
	Game* game;


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

public:
	void newMatchStarted();
	void playOneFrame();
	void saveToFile() const;
	void initFromScratch();
	void loadFromFilesystem();
	void calcBestFitnessEver();
};

