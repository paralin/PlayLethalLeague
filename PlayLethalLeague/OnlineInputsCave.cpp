#include "AllCaves.h"

OVERWRITTEN_SIZE(OnlineInputsCave, 5);
SCAN_PATTERN(OnlineInputsCave, 0x8B, 0x46, 0x04, 0x8B, 0x08, 0x50, 0x51, 0x8D, 0x55, 0x08);
RENDER(OnlineInputsCave, {
	a.push(x86::eax);
	a.push(x86::ecx);

	a.mov(PTO(isOnline), 0x01);

	// for each of the players
	for (int i = 0; i < 4; i++)
	{
		a.mov(x86::cx, x86::ptr(x86::esi, 0x10 + (0x04 * i)));
		a.mov(x86::ptr_abs(reinterpret_cast<intptr_t>(&s->inputsSaved), 0x02 * i), x86::cx);
	}

	a.pop(x86::ecx);
	a.pop(x86::eax);
});