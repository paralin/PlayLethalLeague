#include <caves/AllCaves.h>

OVERWRITTEN_SIZE(StageCave, 5);

SCAN_PATTERN(StageCave, 0xBB, '?', 0x0, 0x0, 0x0, 0x89, 0x45, 0xFC, 0x8B, 0x87, 0xA4, 0, 0, 0, 0xDB, 0x45, 0xFC);

RENDER(StageCave, {
	// mov [stagebase], ecx
	a.mov(PTO(stage_base), x86::ecx);
});