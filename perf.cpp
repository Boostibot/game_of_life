#include "perf.h"
#include "time.h"

static Perf_Counter perf_counters[MAX_PERF_COUNTERS] = {0};

Perf_Counter_Executor::Perf_Counter_Executor(int64_t _index, int64_t _line, const char* _file, const char* _function, const char* _name)
{
	start = perf_counter();
	index = _index;
	line = _line;
	file = _file;
	function = _function;
	name = _name;
}

Perf_Counter_Executor::~Perf_Counter_Executor()
{
	int64_t delta = perf_counter() - start;
	perf_counters[index].counter += delta;
	perf_counters[index].file = file;
	perf_counters[index].line = line;
	perf_counters[index].function = function;
	perf_counters[index].name = name;
	perf_counters[index].runs += 1;
}

const Perf_Counter* perf_get_counters()
{
	return perf_counters;
}

double perf_counter_get_total_running_time_s(Perf_Counter counter)
{
	return (double) counter.counter / (double) perf_counter_freq();
}

double perf_counter_get_average_running_time_s(Perf_Counter counter)
{
	return (double) counter.counter / (double) (counter.runs * perf_counter_freq());
}