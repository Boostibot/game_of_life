#pragma once
#include "types.h"

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//acts like realloc except when new_size == 0 performs free (instead of unspecified).
//If memory allocation fails panics (aborts program with an error message)
static void* sure_realloc(void* old, isize new_size, isize old_size)
{
	static isize total_memory = 0;
	isize delta = new_size - old_size;
	total_memory += delta;
	printf("realloc called to get: %-16lld B total memory usage: %-16lld\n", (lld) delta, (lld) total_memory);

	if(new_size == 0)
	{
		free(old);
		return NULL;
	}
	else
	{
		void* out = realloc(old, new_size);
		if(out == NULL)
		{
			printf("OUT OF MEMORY! ABORTING!\n");
			abort();
		}

		return out;
	}
}