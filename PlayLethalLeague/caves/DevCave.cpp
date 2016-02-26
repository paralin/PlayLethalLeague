#include "AllCaves.h"

OVERWRITTEN_SIZE(DevCave, 6);

SCAN_PATTERN(DevCave, 0x8D, 0x8E, 0x80, 0, 0, 0, 0xFF, 0x15, '?', '?', '?', '?', 0x8B, 0x08, 0x89, 0x8E, 0x90, 0, 0, 0);

RENDER(DevCave, {
	// originalcode
	// lea ecx,[esi+00000080]
	a.lea(x86::ecx, x86::ptr(x86::esi, 0x80));

	// mov [devbase],esi
	a.mov(PTO(dev_base), x86::esi);
});