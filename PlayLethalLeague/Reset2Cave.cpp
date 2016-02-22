#include "AllCaves.h"

OVERWRITTEN_SIZE(Reset2Cave, 6);
SCAN_PATTERN(Reset2Cave, 0x8B, 0x46, 0x2C, 0x8B, 0x56, 0x24, 0x8B, 0xC8);
RENDER(Reset2Cave, {
	a.mov(PTO(isOnline), 0x0);
});