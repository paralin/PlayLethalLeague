#pragma once

#include "Game.h"
#include <asmjit/asmjit.h>

class Game;
class CodeCave
{
public:
	virtual ~CodeCave() {}

	// Get the scan pattern, outputs the size. Return NULL for no auto inject.
	virtual unsigned char* getScanPattern(size_t* size) = 0;
	// Render the machine code
	virtual unsigned char* render(Game* game, intptr_t injectPoint, size_t* size, size_t prependSpace) = 0;
	virtual size_t overwrittenInstructionSize() = 0;
	virtual bool shouldPrependOriginal() = 0;
	virtual const char* getCodeCaveName() = 0;
};

#define DO_DEFINE_CODECAVE(CLASS, CALLORIGN, NAME) \
class CLASS : public CodeCave \
{ \
public: \
	CLASS(){}; \
	~CLASS(){}; \
	unsigned char* getScanPattern(size_t* size); \
	unsigned char* render(Game* game, intptr_t injectPoint, size_t* size, size_t prependSpace); \
	size_t overwrittenInstructionSize(); \
	bool shouldPrependOriginal() { return CALLORIGN; } \
	const char* getCodeCaveName() { return NAME; }; \
};

#define DEFINE_CODECAVE(CLASS, NAME) DO_DEFINE_CODECAVE(CLASS, false, NAME);
#define DEFINE_CODECAVE_KEEPORIG(CLASS, NAME) DO_DEFINE_CODECAVE(CLASS, true, NAME);

// because we are now in the memory space we can just directly address it
#define PTO(THING) x86::ptr_abs((intptr_t)&s->THING, 0, sizeof(s->THING))
#define RENDER(CLASS, CODE) unsigned char* CLASS::render(Game* game, intptr_t injectPoint, size_t* size, size_t prependSpace) { \
		using namespace asmjit; \
		const GameStorage* s = game->gameData; \
		JitRuntime r; X86Assembler a(&r); \
		CODE; \
		size_t alloc_size; \
		size_t code_size = alloc_size = a.getCodeSize(); \
		unsigned char* codeBuf = static_cast<unsigned char*>(malloc(code_size + prependSpace)); \
		code_size = a.relocCode(codeBuf + prependSpace); \
		*size = code_size + prependSpace; \
		return codeBuf; \
	}
#define RENDER_NOP(CLASS) unsigned char* CLASS::render(Game* game, intptr_t injectPoint, size_t* size, size_t prependSpace) { return NULL; }
// if (code_size != alloc_size) codeBuf = realloc(codeBuf, code_size);
#define SCAN_PATTERN_NONE(CLASS) unsigned char* CLASS::getScanPattern(size_t* size) { return NULL; }
#define SCAN_PATTERN(CLASS, ...) unsigned char* CLASS::getScanPattern(size_t* size) { \
		const unsigned char inject_point[] = { __VA_ARGS__ }; \
		unsigned char* buf = static_cast<unsigned char*>(malloc(sizeof(char) * (*size = sizeof(inject_point)))); \
		memcpy(buf, inject_point, *size); \
		return buf; \
	}

#define OVERWRITTEN_SIZE(CLASS, SIZE) \
	size_t CLASS::overwrittenInstructionSize() { return SIZE; };