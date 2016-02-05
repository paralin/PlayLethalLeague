#pragma once

#include "Game.h"
#include <asmjit/asmjit.h>

class Game;
class CodeCave
{
public:
	virtual ~CodeCave() {}

	// Get the scan pattern, outputs the size. Return NULL for no auto inject.
	virtual char* getScanPattern(size_t* size) = 0;
	// Render the machine code
	virtual char* render(Game* game, intptr_t injectPoint, size_t* size) = 0;
	virtual size_t overwrittenInstructionSize() = 0;
};

#define DEFINE_CODECAVE(CLASS) \
class CLASS : public CodeCave \
{ \
public: \
	CLASS(){}; \
	~CLASS(){}; \
	char* getScanPattern(size_t* size); \
	char* render(Game* game, intptr_t injectPoint, size_t* size); \
	size_t overwrittenInstructionSize(); \
};

#define OFFSETOF(THING) ((intptr_t) &s->THING - (intptr_t) s)
#define PTO(THING) x86::ptr_abs(remoteAddr, OFFSETOF(THING), sizeof(s->THING))
#define RENDER(CLASS, CODE) char* CLASS::render(Game* game, intptr_t injectPoint, size_t* size) { \
		using namespace asmjit; \
		const GameOffsetStorage* s = game->localOffsetStorage; \
		intptr_t remoteAddr = reinterpret_cast<intptr_t>(game->remoteOffsetStorage); \
		JitRuntime r; X86Assembler a(&r); \
		CODE; \
		size_t alloc_size; \
		size_t code_size = alloc_size = a.getCodeSize(); \
		char* codeBuf = static_cast<char*>(malloc(code_size)); \
		code_size = a.relocCode(codeBuf); \
		*size = code_size; \
		return codeBuf; \
	}
// if (code_size != alloc_size) codeBuf = realloc(codeBuf, code_size);
#define SCAN_PATTERN_NONE(CLASS) char* CLASS:getScanPattern(size_t* size) { return NULL; }
#define SCAN_PATTERN(CLASS, ...) char* CLASS::getScanPattern(size_t* size) { \
		const char inject_point[] = { __VA_ARGS__ }; \
		char* buf = static_cast<char*>(malloc(sizeof(char) * (*size = sizeof(inject_point)))); \
		memcpy(buf, inject_point, *size); \
		return buf; \
	}

#define OVERWRITTEN_SIZE(CLASS, SIZE) \
	size_t CLASS::overwrittenInstructionSize() { return SIZE; };