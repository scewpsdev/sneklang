module win32;
namespace win32;

extern int QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
extern int QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);


int64 getTimestamp()
{
	LARGE_INTEGER ticks;
	LARGE_INTEGER freq;

	int result1 = QueryPerformanceCounter(&ticks);
	int result2 = QueryPerformanceFrequency(&freq);

	ticks.quadPart *= 1000000000;
	ticks.quadPart /= freq.quadPart;

	return ticks.quadPart;
}