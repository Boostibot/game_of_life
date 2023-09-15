#include "chunk_hash.h"
#include "perf.h"
#include "alloc.h"

u64 hash64(u64 value) 
{
    //source: https://stackoverflow.com/a/12996028
    u64 hash = value;
    hash = (hash ^ (hash >> 30)) * (u64) 0xbf58476d1ce4e5b9;
    hash = (hash ^ (hash >> 27)) * (u64) 0x94d049bb133111eb;
    hash = hash ^ (hash >> 31);
    return hash;
}

u64 splat_vec2i_bits(Vec2i pos)
{
	return (u64) pos.y << 32 | (u64) pos.x;
}

bool is_power_of_two(i64 n) 
{
    return (n>0 && ((n & (n-1)) == 0));
}

enum
{
	CHUNK_EMPTY = 0,
	CHUNK_HASH_FLAG_OFFSET = 1
};

void chunk_hash_init(Chunk_Hash* chunk_hash)
{
	//1: deinit as of custom
	chunk_hash_deinit(chunk_hash);

	//2: there is nothing to init here
}

void chunk_hash_deinit(Chunk_Hash* chunk_hash)
{
	sure_realloc(chunk_hash->chunks, 0, chunk_hash->chunk_capacity*sizeof(Chunk));
	sure_realloc(chunk_hash->hash, 0, chunk_hash->hash_capacity*sizeof(Hash_Slot));
	memset(chunk_hash, 0, sizeof *chunk_hash);
}

void chunk_hash_clear(Chunk_Hash* chunk_hash)
{
	memset(chunk_hash->hash, 0, chunk_hash->hash_capacity*sizeof(Hash_Slot));
	chunk_hash->chunk_size = 0;
}

Chunk* chunk_hash_at(Chunk_Hash* chunk_hash, i32 index)
{
	assert(0 <= index && index < chunk_hash->chunk_size);
	return &chunk_hash->chunks[index];
}

i32 chunk_hash_insert(Chunk_Hash* chunk_hash, Vec2i pos)
{
	PERF_COUNTER("insert");
	//if is overfull rehash
	if(chunk_hash->chunk_size * 2 >= chunk_hash->hash_capacity)
	{
		PERF_COUNTER("rehash");
		
		//Calculate size to which we rehash
		i32 new_capacity = 16;
		i32 max = chunk_hash->chunk_size * 4;
		while(new_capacity < max)
			new_capacity *= 2;
		
		assert(is_power_of_two(new_capacity));

		//Allocate new slots 
		Hash_Slot* new_hash = (Hash_Slot*) sure_realloc(NULL, new_capacity * sizeof(Hash_Slot), 0);
		memset(new_hash, 0, new_capacity * sizeof(Hash_Slot));

		//Go through all items and add them to the new hash
		for(i32 i = 0; i < chunk_hash->hash_capacity; i++)
		{
			//skip empty or dead
			Hash_Slot* curr = &chunk_hash->hash[i];
			if(curr->chunk < CHUNK_HASH_FLAG_OFFSET)
				continue;
            
			//hash the non empty entry
			u64 at = curr->chunk - CHUNK_HASH_FLAG_OFFSET;
			u64 curr_splat = splat_vec2i_bits(curr->pos);
			u64 hash = hash64(curr_splat);

			//find an empty slot in the new array to place the entry
			//we use & instead of % because its faster and we know that 
			// new_capacity is always power of two
			u64 mask = (u64) new_capacity - 1; 
			u64 k = hash & mask;
			i32 counter = 0;
			for(; new_hash[k].chunk > 0; k = (k + 1) & mask)
				assert(counter ++ < chunk_hash->hash_capacity && "there must be an empty slot!");

			//place it there
			new_hash[k] = chunk_hash->hash[i];
		}

		//Reassign the newly created hash to the structure cleaning old mess
		sure_realloc(chunk_hash->hash, 0, chunk_hash->hash_capacity * sizeof(Hash_Slot));
		chunk_hash->hash = new_hash;
		chunk_hash->hash_capacity = new_capacity;
	}

	//If has too little size for new entry grow twice the original size
	if(chunk_hash->chunk_size >= chunk_hash->chunk_capacity)
	{
		PERF_COUNTER("grow");
		i32 old_capacity = chunk_hash->chunk_capacity;
		i32 new_capacity = old_capacity*5/4 + 8;
		chunk_hash->chunks = (Chunk*) sure_realloc(chunk_hash->chunks, new_capacity*sizeof(Chunk), old_capacity*sizeof(Chunk));
		chunk_hash->chunk_capacity = new_capacity;
	}
	
	//assert(is_power_of_two(chunk_hash->chunk_capacity));
	assert(is_power_of_two(chunk_hash->hash_capacity));
	assert(chunk_hash->hash_capacity > 0 && chunk_hash->chunk_capacity > 0);
	assert(chunk_hash->hash != NULL && chunk_hash->chunks != NULL);

	u64 pos_splat = splat_vec2i_bits(pos);
	u64 hash = hash64(pos_splat);
	u64 mask = (u64) chunk_hash->hash_capacity - 1;
	u64 i = hash & mask;

	//Find empty slot for the newly added. If it is already in the hash
	// return its index
	i32 counter = 0;
	for(; chunk_hash->hash[i].chunk > 0; i = (i + 1) & mask)
	{
		assert(counter ++ < chunk_hash->hash_capacity && "there must be an empty slot!");
		if(vec_equal(chunk_hash->hash[i].pos, pos))
			return (i32) chunk_hash->hash[i].chunk - CHUNK_HASH_FLAG_OFFSET;
	}

	//Push back the new chunk
	assert(chunk_hash->chunk_size < chunk_hash->chunk_capacity);
	chunk_hash->chunks[chunk_hash->chunk_size] = Chunk{0};
	chunk_hash->chunks[chunk_hash->chunk_size].pos = pos;

	//Link it in the hash
	chunk_hash->hash[i].chunk = (uint32_t) chunk_hash->chunk_size + CHUNK_HASH_FLAG_OFFSET;
	chunk_hash->hash[i].pos = pos;

	chunk_hash->chunk_size ++;
	return chunk_hash->chunk_size - 1;
}

i32 chunk_hash_find(Chunk_Hash* chunk_hash, Vec2i pos)
{
	PERF_COUNTER();
	if(chunk_hash->hash_capacity == 0)
		return -1;

	//assert(is_power_of_two(chunk_hash->chunk_capacity));
	assert(is_power_of_two(chunk_hash->hash_capacity));
	assert(chunk_hash->hash_capacity > 0 && chunk_hash->chunk_capacity > 0);
	assert(chunk_hash->hash != NULL && chunk_hash->chunks != NULL);

	u64 hash = hash64(splat_vec2i_bits(pos));
	u64 mask = (u64) chunk_hash->hash_capacity - 1;
	u64 i = hash & mask;
	i32 counter = 0;
	for(; chunk_hash->hash[i].chunk > 0; i = (i + 1) & mask)
	{
		assert(counter ++ < chunk_hash->hash_capacity && "there must be an empty slot!");
		if(vec_equal(chunk_hash->hash[i].pos, pos))
			return chunk_hash->hash[i].chunk - CHUNK_HASH_FLAG_OFFSET;
	}

	return -1;
}

Chunk* chunk_hash_get_or(Chunk_Hash* chunk_hash, Vec2i chunk_pos, Chunk* if_not_found)
{
	i32 found = chunk_hash_find(chunk_hash, chunk_pos);
	if(found == -1)
		return if_not_found;
	else
		return chunk_hash_at(chunk_hash, found);
};