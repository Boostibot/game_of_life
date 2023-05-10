#pragma once
#include <stddef.h>
#include <thread>

//More threads then we would ever want
#define MAX_THREADS 64

#define DISABLE_PAR_FOR
using isize = ptrdiff_t;

template<typename Fn>
void parallel_for_ranges(isize num_elements, Fn functor, isize max_threads = MAX_THREADS) 
{
	#ifdef DISABLE_PAR_FOR
	max_threads = 0;
	#endif

	//std::thread my_thread = std::thread(functor, 0, num_elements);
	//my_thread.join();
	//return;

    isize extra_threads = std::thread::hardware_concurrency() - 1;
	if(extra_threads <= 0)
		extra_threads = 7;

	if(extra_threads > MAX_THREADS)
		extra_threads = MAX_THREADS;

	if(extra_threads > max_threads)
		extra_threads = max_threads;

	if(extra_threads >= num_elements)
		extra_threads = num_elements - 1;

	if(extra_threads <= 0)
	{
        functor(0, num_elements);
		return;
	}

    isize batch_size = num_elements / extra_threads;
    isize batch_remainder = num_elements % extra_threads;

    std::thread my_threads[MAX_THREADS];
    for(isize i = 0; i < extra_threads; ++i)
        my_threads[i] = std::thread(functor, i * batch_size, (i + 1) * batch_size);
	
	if(batch_remainder != 0)
	{
		isize start = extra_threads * batch_size;
		functor(start, start + batch_remainder);
	}

    for(isize i = 0; i < extra_threads; ++i)
		my_threads[i].join();	
}

template<typename Fn>
void parallel_for(isize from, isize to, Fn functor, isize max_threads = MAX_THREADS) 
{
	parallel_for_ranges(to - from, [&](isize offset_from, isize offset_to){
		for(isize i = offset_from; i < offset_to; i++)
			functor(i + from);
	}, max_threads);
}