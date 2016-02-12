#pragma once
#include <windows.h>

void killProcessByName(const char* filename);
void startProcess(const char* filename, const char* args);
void enableDebugPriv();
HANDLE findProcessByName(const char* name);