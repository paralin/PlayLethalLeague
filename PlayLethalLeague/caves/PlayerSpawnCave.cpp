#include <caves/AllCaves.h>

OVERWRITTEN_SIZE(PlayerSpawnCave, 5);
SCAN_PATTERN(PlayerSpawnCave, 0x8B, 0x11, 0x8B, 0x52, 0x10, 0x57, 0x8D, 0x45, 0xFC);
RENDER(PlayerSpawnCave, {
	a.mov(x86::edx, x86::ptr(x86::ecx));
	a.mov(x86::edx, x86::ptr(x86::edx, 0x10));
	a.mov(PTO(player_spawn), x86::ecx);
});