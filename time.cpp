#include "time.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)   
#include <windows.h>
#include <profileapi.h>

int64_t _query_perf_counter_freq()
{
    LARGE_INTEGER ticks = {};
    (void) QueryPerformanceFrequency(&ticks);
    return ticks.QuadPart;
}

int64_t perf_counter()
{
    LARGE_INTEGER ticks = {};
    (void) QueryPerformanceCounter(&ticks);
    return ticks.QuadPart;
}

int64_t* _perf_counter_base()
{
    static int64_t base = perf_counter();
    return &base;
}

int64_t perf_counter_freq()
{
    static int64_t freq = _query_perf_counter_freq(); //doesnt change so we can cache it
    return freq;
}
    
double _double_perf_counter_freq()
{
    static double freq = (double) _query_perf_counter_freq();
    return freq;
}

int64_t clock_ns()
{
    int64_t freq = perf_counter_freq();
    int64_t counter = perf_counter() - *_perf_counter_base();

    int64_t sec_to_nanosec = 1'000'000'000;
    //We assume _perf_counter_base is set to some reasonable thing so this will not overflow
    // (with this we are only able to represent 1e12 secons (A LOT) without overflowing)
    return counter * sec_to_nanosec / freq;
}

//We might be rightfully scared that after some ammount of time the clock_s will get sufficiently large and
// we will start loosing pression. This is however not a problem. If we assume the _perf_counter_freq is equal to 
// 10Mhz = 1e7 (which is very common) then we would like the clock_s to have enough precision to represent
// 1 / 10Mhz = 1e-7. This precission is held up untill 1e9 secons have passed which is roughly 31 years. 
// => Please dont run your program for more than 31 years or you will loose precision
double clock_s()
{
    double freq = _double_perf_counter_freq();
    double counter = (double) (perf_counter() - *_perf_counter_base());
    return counter / freq;
}
#else
#include <time.h>

int64_t perf_counter()
{
    struct timespec ts;
    (void) clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    return ts.tv_nsec;
}

int64_t perf_counter_freq()
{
	return 1'000'000'000;
}

int64_t clock_ns()
{
    static int64_t base = perf_counter();
    return _clock_ns() - base;
}
    
double clock_s()
{
    return (double) clock_ns() / 1.0e9;
}
#endif

