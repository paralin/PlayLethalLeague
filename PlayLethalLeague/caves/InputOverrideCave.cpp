#include <caves/AllCaves.h>

OVERWRITTEN_SIZE(InputHeldCave, 5);
SCAN_PATTERN(InputHeldCave, 0xE8, '?', '?', '?', '?', 0x84, 0xC0, 0x74, 0x1B, 0x6A, 0x00, 0x8D, 0x55, 0xE8, 0x52, 0x8D, 0x4E, 0x14, 0xE8);
RENDER(InputHeldCave, {
	using namespace x86;

	Label code_exit = a.newLabel();
	Label noinput_exit = a.newLabel();

	a.push(ebx);
	a.push(ecx);
	a.push(edx);
	a.push(edi);

	a.mov(ebx, PTO(inputsCurrentPlayer));
	a.cmp(ptr(ebx, reinterpret_cast<intptr_t>(&s->inputsForcePlayers), 0x01), 0x01);
	a.jne(code_exit);

	a.imul(ebx, PTO(inputsCurrentPlayer), 0x2);
	a.mov(ecx, reinterpret_cast<intptr_t>(&s->forcedInputs));
	a.mov(dx, ptr(ecx, ebx));
	a.mov(ecx, edi);
	a.mov(bx, 0x01);
	a.shl(bx, cl);
	a.test(bx, dx);
	a.je(noinput_exit);
	a.mov(al, 0x01);
	a.jmp(code_exit);

	a.bind(noinput_exit);
	a.mov(al, 0x0);

	a.bind(code_exit);
	a.pop(edi);
	a.pop(edx);
	a.pop(ecx);
	a.pop(ebx);
});

OVERWRITTEN_SIZE(InputPressedCave, 5);
SCAN_PATTERN(InputPressedCave, 0xE8, '?', '?', '?', '?', 0x84, 0xC0, 0x74, 0x1B, 0x6A, 0x00, 0x8D, 0x55, 0xE8, 0x52, 0x8D, 0x4E, 0x24, 0xE8);
RENDER(InputPressedCave, {
	using namespace x86;

	Label code_exit = a.newLabel();
	Label noinput_exit = a.newLabel();
	Label code_return = a.newLabel();

	a.push(ebx);
	a.push(ecx);
	a.push(edx);
	a.push(edi);

	a.mov(ecx, PTO(inputsCurrentPlayer));
	a.cmp(ptr(ecx, reinterpret_cast<intptr_t>(&s->inputsForcePlayers), 0x01), 0x01);
	a.jne(code_exit);

	a.imul(ecx, PTO(inputsCurrentPlayer), 0x02);
	// we could lea but its easier to just mov since we know this at jit time
	a.mov(eax, reinterpret_cast<intptr_t>(&s->forcedInputs));
	a.mov(ebx, reinterpret_cast<intptr_t>(&s->inputsLastFrame));
	a.mov(ax, ptr(eax, ecx));
	a.mov(bx, ptr(ebx, ecx));
	a.mov(ecx, edi);
	a.mov(dx, 0x01);
	a.shl(dx, cl);
	a.test(ax, dx);
	a.je(noinput_exit);
	a.test(bx, dx);
	a.jne(noinput_exit);
	a.mov(al, 0x01);
	a.jmp(code_exit);

	a.bind(noinput_exit);
	a.mov(al, 0x0);

	a.bind(code_exit);
	a.pop(edi);
	a.pop(edx);
	a.pop(ecx);
	a.pop(ebx);
	a.cmp(edi, 0x00000008);
	a.jne(code_return);
	a.inc(PTO(inputsCurrentPlayer));
	a.cmp(PTO(inputsCurrentPlayer), 0x04);
	a.jb(code_return);
	a.mov(PTO(inputsCurrentPlayer), 0x0);

	a.push(eax);

	a.mov(eax, ptr_abs(reinterpret_cast<intptr_t>(&s->forcedInputs), 0x0, 0x04));
	a.mov(ptr_abs(reinterpret_cast<intptr_t>(&s->inputsLastFrame), 0x0, 0x04), eax);

	a.mov(eax, ptr_abs(reinterpret_cast<intptr_t>(s->forcedInputs), 0x04, 0x04));
	a.mov(ptr_abs(reinterpret_cast<intptr_t>(&s->inputsLastFrame), 0x04, 0x04), eax);

	a.pop(eax);

	a.bind(code_return);
});