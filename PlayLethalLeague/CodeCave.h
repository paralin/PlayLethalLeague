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
	virtual unsigned char* render(Game* game, intptr_t injectPoint, size_t* size, size_t prependSpace) = 0;
	virtual size_t overwrittenInstructionSize() = 0;
	virtual bool shouldPrependOriginal() = 0;
};

#define DO_DEFINE_CODECAVE(CLASS, CALLORIGN) \
class CLASS : public CodeCave \
{ \
public: \
	CLASS(){}; \
	~CLASS(){}; \
	char* getScanPattern(size_t* size); \
	unsigned char* render(Game* game, intptr_t injectPoint, size_t* size, size_t prependSpace); \
	size_t overwrittenInstructionSize(); \
	bool shouldPrependOriginal() { return CALLORIGN; } \
};

#define DEFINE_CODECAVE(CLASS) DO_DEFINE_CODECAVE(CLASS, false);
#define DEFINE_CODECAVE_KEEPORIG(CLASS) DO_DEFINE_CODECAVE(CLASS, true);

#define OFFSETOF(THING) (intptr_t)((intptr_t) &s->THING - (intptr_t) s)
#define PTO(THING) x86::ptr_abs(remoteAddr, OFFSETOF(THING), sizeof(s->THING))
#define RENDER(CLASS, CODE) unsigned char* CLASS::render(Game* game, intptr_t injectPoint, size_t* size, size_t prependSpace) { \
		using namespace asmjit; \
		const GameOffsetStorage* s = game->localOffsetStorage; \
		intptr_t remoteAddr = reinterpret_cast<intptr_t>(game->remoteOffsetStorage); \
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
#define SCAN_PATTERN_NONE(CLASS) char* CLASS::getScanPattern(size_t* size) { return NULL; }
#define SCAN_PATTERN(CLASS, ...) char* CLASS::getScanPattern(size_t* size) { \
		const char inject_point[] = { __VA_ARGS__ }; \
		char* buf = static_cast<char*>(malloc(sizeof(char) * (*size = sizeof(inject_point)))); \
		memcpy(buf, inject_point, *size); \
		return buf; \
	}

#define OVERWRITTEN_SIZE(CLASS, SIZE) \
	size_t CLASS::overwrittenInstructionSize() { return SIZE; };