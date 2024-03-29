#pragma once
#include <stdint.h>

// This file provides a simple interface for measuring performance of a scope.
// Simply write PERF_COUNTER() into a function and its runtime will 
// automatically be measured. 
//
// The gathered statistics can be retrieved by calling the get_counters() which 
// is a simple pointer to an array of Perf_Counter structs. 

struct Perf_Counter
{
	int64_t counter;
	int64_t runs;
	int64_t line;
	const char* file;
	const char* function;
	const char* name;
};

//Returns a pointer to the gathered statistics. The array has perf_get_counter_count() size.
const Perf_Counter* const* perf_get_counters();
//Returns the number of currently registered statistics
int64_t perf_get_counter_count();

//Return the total running time of the counter in seconds
double perf_counter_get_total_running_time_s(Perf_Counter counter);

//Return the average running time of the counter in seconds
double perf_counter_get_average_running_time_s(Perf_Counter counter);

//Creates a performance counter
#define PERF_COUNTER(...) \
		static Perf_Counter CONCAT(__perf_counter__, __LINE__) = {0}; \
		Perf_Counter_Executor CONCAT(__perf_executor__, __LINE__)(&CONCAT(__perf_counter__, __LINE__), __LINE__, __FILE__, __FUNCTION__, ##__VA_ARGS__)

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

#ifndef _MSC_VER
#define __FUNCTION__ __func__
#endif 

struct Perf_Counter_Executor
{
	Perf_Counter* my_counter;
	int64_t start;
	int64_t line;
	const char* file;
	const char* function;
	const char* name;
	
	Perf_Counter_Executor(Perf_Counter* _my_counter, int64_t _line, const char* _file, const char* _function = nullptr, const char* _name = nullptr);
	~Perf_Counter_Executor();
};