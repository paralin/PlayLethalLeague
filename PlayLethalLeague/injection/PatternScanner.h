#pragma once
#include "Game.h"

class PatternScanner
{
public:
	PatternScanner();

	static void search(PBYTE baseAddress, DWORD baseLength, std::vector<std::shared_ptr<CodeCaveScan>> scans);

public:

	~PatternScanner();
};

