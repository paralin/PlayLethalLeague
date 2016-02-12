#include "stdafx.h"
#include "Util.h"
#include <iostream>
#include <fstream>
#include <Pathcch.h>

#define LOG(MSG) std::cout << MSG << std::endl;
int main()
{
	enableDebugPriv();
	HMODULE hLocKernel32 = GetModuleHandle("Kernel32");
	FARPROC hLocLoadLibrary = GetProcAddress(hLocKernel32, "LoadLibraryA");

	LOG("Killing existing processes....");
	killProcessByName("LethalLeague.exe");

	// Wait for it to die
	Sleep(1000);

	LOG("Launching process...");
	system("explorer steam://run/261180");

	LOG("Attaching to process....");
	HANDLE gameHandle;
	{
		int tries = 0;
		while (tries < 20)
		{
			gameHandle = findProcessByName("LethalLeague.exe");
			if (gameHandle == nullptr)
			{
				LOG("Process hasn't started yet...");
				Sleep(1000);
				tries++;
			}
			else
				break;
		}

		if (tries == 20)
		{
			LOG("Unable to attach to process.");
			getchar();
			return 1;
		}
	}

		LOG("Giving the game some time to start...");
		Sleep(5000);

	// Copy the DLL to the game
#if 0
	{
		if (!CopyFile("../Debug/PlayLethalLeague.dll", "C:/Program Files (x86)/Steam/steamapps/common/lethalleague/ll-neural.dll", false))
		{
			LOG("Unable to copy dll!");
			getchar();
			return 1;
		}
	}
#endif

	// Load the library remotely
	{
		void* remoteLibNameLoc;
		char* dllnamen = (char*)malloc(1024);
		GetModuleFileNameA(NULL, dllnamen, sizeof(dllnamen));
		char *pos = strrchr(dllnamen, '\\');
		if (pos != NULL) {
			*pos = '\0'; //this will put the null terminator here. you can also copy to another string if you want
		}
		std::string dllname(dllnamen);
		dllname += "\\PlayLethalLeague.dll";
		free(dllnamen);

		remoteLibNameLoc = VirtualAllocEx(gameHandle, NULL, dllname.size() + 1, MEM_COMMIT, PAGE_READWRITE);
		WriteProcessMemory(gameHandle, remoteLibNameLoc, dllname.c_str(), dllname.size() + 1, NULL);

		HANDLE hRemoteThread = CreateRemoteThread(gameHandle, NULL, 0, (LPTHREAD_START_ROUTINE)hLocLoadLibrary, remoteLibNameLoc, 0, NULL);
		if (WaitForSingleObject(hRemoteThread, 10000) == WAIT_TIMEOUT)
		{
			LOG("After 10 seconds the load library thread hasn't exited.");
			getchar();
			return 1;
		}
		VirtualFreeEx(gameHandle, remoteLibNameLoc, dllname.size() + 1, MEM_RELEASE);
	}

	CloseHandle(gameHandle);

	LOG("Completed injection.");
	getchar();

	return 0;
}

