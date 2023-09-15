#pragma once
#include "types.h"

#define CHUNK_SIZE 61
typedef struct Chunk
{
	Vec2i pos;

	//contains 64x64 bit field of cells
	//Only the cells starting at index (1,1) and 
	//ending at (61,61) are considered a part of the chunk
	//the rest is from neighbouring chunks during loading 
	u64 data[64]; 
} Chunk;

static bool chunk_get_cell(const Chunk* chunk, Vec2i pos)
{
	assert(-1 <= pos.x && pos.x < CHUNK_SIZE + 1);
	assert(-1 <= pos.y && pos.y < CHUNK_SIZE + 1);
	
	u64 bit = (u64) 1 << (pos.x + 1);
	
	return (chunk->data[pos.y + 1] & bit) > 0;
}

static void chunk_set_cell(Chunk* chunk, Vec2i pos, bool to)
{
	assert(-1 <= pos.x && pos.x < CHUNK_SIZE + 1);
	assert(-1 <= pos.y && pos.y < CHUNK_SIZE + 1);

	u64 bit = (u64) 1 << (pos.x + 1);
	
	if(to)
		chunk->data[pos.y + 1] |= bit;
	else
		chunk->data[pos.y + 1] &= ~bit;
}	
