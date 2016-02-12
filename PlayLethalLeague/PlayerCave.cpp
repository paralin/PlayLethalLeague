#include <AllCaves.h>

OVERWRITTEN_SIZE(PlayerCave, 5);

SCAN_PATTERN(PlayerCave, 0x8B, 0x16, 0x8B, 0x52, 0x44, 0x57, 0x8D, 0x45, 0xFC);

RENDER(PlayerCave, {
	Label player_code = a.newLabel();
	Label player_code_notp1base = a.newLabel();
	Label player_end = a.newLabel();

	// player_code label
	a.bind(player_code);

	// mov edx,[esi]
	a.mov(x86::edx, x86::ptr(x86::esi));

	// mov edx,[edx+44]
	a.mov(x86::edx, x86::ptr(x86::edx, 0x44));

	// push eax
	a.push(x86::eax);

	// push ebx
	a.push(x86::ebx);

	// push ecx
	a.push(x86::ecx);

	// mov eax,[currentplayer]
	a.mov(x86::eax, PTO(currentPlayer));

	// cmp [p1base],esi
	a.cmp(PTO(player_bases[0]), x86::esi);

	// jne player_code_notp1base
	a.jne(player_code_notp1base);

	// mov al,00
	a.mov(x86::al, 0x0);
	
	// mov [currentPlayer],al
	a.mov(PTO(currentPlayer), x86::al);

	// player_code_notp1base
	a.bind(player_code_notp1base);

	// now eax is player id

	// multiply by 4 bytes for array offset
	//imul eax,0x04
	a.imul(x86::eax, 0x04);

	// move to player bases[0] + 4 * index esi
	// mov [p1base+eax],esi
	a.mov(x86::ptr(x86::eax, reinterpret_cast<intptr_t>(&s->player_bases[0]), sizeof(void*)), x86::esi);

	// mov ebx,[esi+14]
	a.mov(x86::ebx, x86::ptr(x86::esi, 0x14));

	// mov [p1coords+eax],ebx
	a.mov(x86::ptr(x86::eax, (intptr_t)(&s->player_coords[0]), sizeof(void*)), x86::ebx);

	// mov ebx,[esi+194]
	a.mov(x86::ebx, x86::ptr(x86::esi, 0x194));

	// mov [p1state+eax],ebx
	a.mov(x86::ptr(x86::eax, (intptr_t)(s->player_states[0]), sizeof(void*)), x86::ebx);

	// mov eax,[currentPlayer]
	a.mov(x86::eax, PTO(currentPlayer));

	// inc al
	a.inc(x86::al);

	// check if we're at the last player
	// these checks are unnecessary and have been commented
	//cmp al,04
	//a.cmp(x86::al, 0x04);

	// jb player_end
	//a.jb(player_end);

	// mov al, 00
	//a.mov(x86::al, 0x0);

	//player_end
	a.bind(player_end);

	//mov [currentPlayer], al
	// actually, move the entire register
	// mov [currentPlayer],eax
	a.mov(PTO(currentPlayer), x86::eax);

	// pop ecx
	a.pop(x86::ecx);

	// pop ebx
	a.pop(x86::ebx);

	// pop eax
	a.pop(x86::eax);
});