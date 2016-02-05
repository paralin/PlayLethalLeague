#include <AllCaves.h>

// We're overwriting a 6 byte instruction
OVERWRITTEN_SIZE(BallCave, 6);

// This is the pattern to scan for
SCAN_PATTERN(BallCave, 0x8B, 0x81, 0xCC, 0, 0, 0, 0x8B, 0x50, 0x70, 0x8B, 0x86,0xA8,0x02,0,0,0x57);

// Render the asm to use
RENDER(BallCave, {
	// originalcode
	// mov eax, [ecx + 000000CC]
	a.mov(x86::eax, x86::ptr(x86::ecx, 0x0CC, sizeof(int32_t)));

	// mov[ballbase], esi
	a.mov(PTO(ball_base), x86::esi);

	// mov esi, [esi + 14]
	a.mov(x86::esi, x86::ptr(x86::esi, 0x14));

	// mov[ballcoords], esi
	a.mov(PTO(ball_coord), x86::esi);

	// mov esi, [ballbase]
	a.mov(x86::esi, PTO(ball_base));

	// mov esi, [esi + 194]
	a.mov(x86::esi, x86::ptr(x86::esi, 0x194));

	// mov[ballstate], esi
	a.mov(PTO(ball_state), x86::esi);

	// mov esi, [ballbase]
	a.mov(x86::esi, PTO(ball_base));
})