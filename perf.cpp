#include "perf.h"
#include "time.h"

#ifndef MAX_PERF_COUNTERS
#define MAX_PERF_COUNTERS 100
#endif

static Perf_Counter* perf_counters[MAX_PERF_COUNTERS] = {0};
static int64_t perf_counter_count = 0;

Perf_Counter_Executor::Perf_Counter_Executor(Perf_Counter* _my_counter, int64_t _line, const char* _file, const char* _function, const char* _name)
{
	start = perf_counter();
	my_counter = _my_counter;
	line = _line;
	file = _file;
	function = _function;
	name = _name;
}

Perf_Counter_Executor::~Perf_Counter_Executor()
{
	int64_t delta = perf_counter() - start;

	//if is a first run add itself to counters
	//and set first run only stats
	if(my_counter->runs == 0)
	{
		my_counter->file = file;
		my_counter->line = line;
		my_counter->function = function;
		my_counter->name = name;
		perf_counters[perf_counter_count++] = my_counter;
	}
	
	//Add cumulative stats
	my_counter->counter += delta;
	my_counter->runs += 1;
}

const Perf_Counter* const* perf_get_counters()
{
	return perf_counters;
}

int64_t perf_get_counter_count()
{
	return perf_counter_count;
}

double perf_counter_get_total_running_time_s(Perf_Counter counter)
{
	return (double) counter.counter / (double) perf_counter_freq();
}

double perf_counter_get_average_running_time_s(Perf_Counter counter)
{
	return (double) counter.counter / (double) (counter.runs * perf_counter_freq());
}