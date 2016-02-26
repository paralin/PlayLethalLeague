#include "AllCaves.h"

OVERWRITTEN_SIZE(DeathCave, 6);
SCAN_PATTERN(DeathCave, 0x29, 0xB0, 0x6C, 0x01, 0x00, 0x00, 0x0F, 0xB6, 0x82, 0xBC, 0x01, 0x00, 0x00, 0x8B, 0x89, 0x14, 0x01, 0x00, 0x00, 0x8B, 0x91, 0xA4, 0x00, 0x00, 0x00);
RENDER(DeathCave, {
	Label endlabel;
	a.cmp(PTO(decrementLifeOnDeath), 0x01);
	a.jne(endlabel);

	// originalcode
	// decrements lives
	a.sub(x86::ptr(x86::eax, 0x16C), x86::esi);

	a.bind(endlabel);
});