#include "AllCaves.h"

SCAN_PATTERN(OfflineInputsCave, 0x89, 0x30, 0x0F, 0xB7, 0x17, 0x33, 0xC9);
OVERWRITTEN_SIZE(OfflineInputsCave, 5);
RENDER(OfflineInputsCave, {
	Label code_exit = a.newLabel();

	a.cmp(PTO(isOnline), 0x01);
	a.jge(code_exit);

	a.push(x86::ecx);
	a.push(x86::ebx);
	a.mov(x86::ecx, x86::esi);

	// store inputs
	a.mov(x86::ebx, 0x0);
	a.mov(x86::bl, x86::dl);
	a.imul(x86::ebx, x86::ebx, 0x02);
	a.mov(x86::ptr(x86::ebx, reinterpret_cast<intptr_t>(&s->inputsSaved), 0x02), x86::cx);

	a.pop(x86::ebx);
	a.pop(x86::ecx);

	a.bind(code_exit);
	a.mov(x86::ptr(x86::eax), x86::esi);
	a.movzx(x86::edx, x86::ptr(x86::edi, 0x0, sizeof(DWORD)));
});