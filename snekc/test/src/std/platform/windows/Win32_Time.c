import Win32_Windows;




int QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
int QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);

long win32GetNanos()
{
	LARGE_INTEGER ticks;
	LARGE_INTEGER freq;

	int result1 = QueryPerformanceCounter(&ticks);
	int result2 = QueryPerformanceFrequency(&freq);

	ticks.quadPart *= 1000000000;
	ticks.quadPart /= freq.quadPart;

	return ticks.quadPart;
}
