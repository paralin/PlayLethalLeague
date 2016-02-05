#include "AllCaves.h"

OVERWRITTEN_SIZE(GameRulesCave, 6);

SCAN_PATTERN(GameRulesCave, 0x89, 0x8E, 0xF0, 0, 0, 0, 0x8B, 0x5D, 0x08, 0x83, 0xBF, 0x10, 0x01, 0, 0, 0x08);

RENDER(GameRulesCave, {
	// originalcode
	// mov [esi+000000F0],ecx
	a.mov(x86::ptr(x86::esi, 0x0F0), x86::ecx);

	// mov [GameRuleSet],esi
	a.mov(PTO(gamerule_set), x86::esi);
});