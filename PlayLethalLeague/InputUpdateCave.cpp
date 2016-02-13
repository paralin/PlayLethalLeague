#include "AllCaves.h"


// Pattern for beginning of GameLoop
// 55 8B EC 83 EC 14 56 57  8B ?? ?? ?? ?? ?? 8B F1 8D 45 F4 8D 8E 80 00 00 00 50 FF ?? 8D 4D F4 51 8D 4E 78
// 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14, 0x56, 0x57, 0x8B, '?', '?', '?', '?', '?', 0x8B, 0xF1, 0x8D, 0x45, 0xF4, 0x8D, 0x8E, 0x80, 0, 0, 0, 0x50, 0xFF, '?', 0x8D, 0x4D, 0xF4, 0x51, 0x8D, 0x4E, 0x78

SCAN_PATTERN(StartOfFrameCave, 0x56, 0x57, 0x8B, 0xF1, 0xFF, '?', '?', '?', '?', '?', 0x8D, 0x7E, 0x28, 0x57, 0x8B, 0xCE, 0xE8, '?', '?', '?', '?', 0x8B, 0x8E, 0xDC, 0, 0, 0, 0x57, 0xE8, '?', '?', '?', '?', 0x8B, 0x8E, 0xD4, 0, 0, 0);
// overwrite push ebp; mov ebp, esp; push 0FF. use auto-replicate for this.
OVERWRITTEN_SIZE(StartOfFrameCave, 10);
// GCC expects functions to preserve the following callee-save registers:
//        EBX, EDI, ESI, EBP, DS, ES, SS
//			You need not save the following registers :
//		EAX, ECX, EDX, FS, GS, EFLAGS, floating point registers
RENDER(StartOfFrameCave, {
	a.pusha();
	a.pushf();
	a.mov(x86::ecx, reinterpret_cast<intptr_t>(game));
	a.push(x86::ecx);
	a.call(PTO(frameHookAddress));
	a.popf();
	a.popa();
});