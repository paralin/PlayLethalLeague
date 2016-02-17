#pragma once
#include <windows.h>
#include <chrono>

struct TickCountClock
{
	typedef unsigned long long                       rep;
	typedef std::milli                               period;
	typedef std::chrono::duration<rep, period>       duration;
	typedef std::chrono::time_point<TickCountClock>  time_point;
	static const bool is_steady = true;

	static time_point now() noexcept
	{
		return time_point(duration(GetTickCount()));
	}
};


_declspec(dllexport) int testInjectedPlayLL();

#define CLOCK_U TickCountClock
#define TIME_POINT std::chrono::time_point<TickCountClock>