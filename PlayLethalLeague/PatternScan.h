#pragma once
#pragma once

#include "CodeCave.h"
#include <asmjit/asmjit.h>

class Game;
class PatternScan
{
public:
	virtual ~PatternScan() {}

	// Get the scan pattern, outputs the size.
	virtual unsigned char* getScanPattern(size_t* size) = 0;
	virtual void* getFoundLocation() = 0;
	virtual void setFoundLocation(void* loc) = 0;
	virtual const char* getCodeCaveName() = 0;
};

#define DEFINE_PATTERNSCAN(CLASS, NAME) \
class CLASS : public PatternScan \
{ \
public: \
	CLASS(){}; \
	~CLASS(){}; \
	void* m_foundLoc; \
	unsigned char* getScanPattern(size_t* size); \
	void* getFoundLocation() { return m_foundLoc; } \
	void setFoundLocation(void* loc) { m_foundLoc = loc; } \
	const char* getCodeCaveName() { return NAME; } \
};
