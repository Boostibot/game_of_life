#pragma once
#include "types.h"
#include "chunk.h"

// This file provides an iterface to a simple (but very performant) hash map implementation. 
// 
// It uses 2 arrays: one for the keys and one for the values.
// This is important because it gives us optimal traversal speed over the values.
// It uses simple linear probing on hash closions but the downsides of this approach 
// are compansated by extremely small allowed hash fullness (only 25%). 
// 
// We can do this because the ratio of key size (12B) to value size (64*64B) is extremely small,
// thus having 4x more keys than values still results in very little space used.
// 
// Uses growth old*5/4 + 8 for the chunks array to minimize wasted memory when many chunks are present. 
// Still because we use simple malloc/free to obtain memory we often run out of system memory and freeze.
// This could be circumwented by properly memory mapping straight from the OS but I am far too lazy to do 
// that in ths project.
// 
// See implementation for more details.

typedef struct Hash_Slot
{
	Vec2i pos;

	//Index of the given chunk in the chunk array + 1 (CHUNK_HASH_FLAG_OFFSET).
	//0 means this slot is empty
	u32 chunk;
} Hash_Slot;

typedef struct Chunk_Hash
{
	Chunk* chunks;
	Hash_Slot* hash;

	i32 hash_capacity;
	i32 chunk_size;
	i32 chunk_capacity;
} Chunk_Hash;

void chunk_hash_init(Chunk_Hash* chunk_hash);
void chunk_hash_deinit(Chunk_Hash* chunk_hash);

i32 chunk_hash_insert(Chunk_Hash* chunk_hash, Vec2i pos);
i32 chunk_hash_find(Chunk_Hash* chunk_hash, Vec2i pos);
Chunk* chunk_hash_at(Chunk_Hash* chunk_hash, i32 index);

Chunk* chunk_hash_get_or(Chunk_Hash* chunk_hash, Vec2i chunk_pos, Chunk* if_not_found);
void chunk_hash_clear(Chunk_Hash* chunk_hash);

