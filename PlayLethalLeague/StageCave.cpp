#include <AllCaves.h>

OVERWRITTEN_SIZE(StageCave, 5);

SCAN_PATTERN(StageCave, 0xBB, '?', 0x0, 0x0, 0x0, 0x89, 0x45, 0xFC, 0x8B, 0x87, 0xA4, 0, 0, 0, 0xDB, 0x45, 0xFC);

RENDER(StageCave, {
	// originalcode
	// mov ebx, 1
	a.mov(x86::ebx, 0x01);

	// mov [stagebase], ecx
	a.mov(PTO(stage_base), x86::ecx);
});