#include "AllCaves.h"

// Hooks a function that prints out 'destroying gamestate'
// Sets the do_reset bit
// Main loop will notice this flip and zero the struct.

OVERWRITTEN_SIZE(ResetCave, 6);

SCAN_PATTERN(ResetCave, 0x8D, 0x8D, 0x28, 0xFF, 0xFF, 0xFF, 0x68, '?', '?', '?', '?', 0x51, 0xE8, '?', '?', '?', '?', 0x83, 0xC4, 0x08, 0x50, 0xE8, '?', '?', '?', '?', 0x83, 0xC4, 0x08, 0x50, 0xE8, '?', '?', '?', '?', 0x83, 0xC4, 0x08, 0x39, 0x5E, 0x30);

RENDER(ResetCave, {
	// flip do_reset to 1
	a.mov(PTO(do_reset), 0x01);
});