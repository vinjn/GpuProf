#pragma once

#define NOMINMAX
#include <Windows.h>
#include <tlhelp32.h>

void GoToXY(int column, int line);
PROCESSENTRY32 getEntryFromPID(DWORD pid);
