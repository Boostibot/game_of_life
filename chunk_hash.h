#pragma once
#include "types.h"
#include "time.h"

//Chunk hashes
struct Hash_Slot;
struct Chunk_Hash
{
	Chunk* chunks;
	i32 hash_capacity;
	i32 chunk_size;
	i32 chunk_capacity;
	
	Hash_Slot* hash;
};

Chunk_Hash create_chunk_hash();
void destroy_chunk_hash(Chunk_Hash* chunk_hash);

i32 insert_chunk(Chunk_Hash* chunk_hash, Vec2i pos);
i32 find_chunk(Chunk_Hash* chunk_hash, Vec2i pos);
Chunk* get_chunk(Chunk_Hash* chunk_hash, i32 index);

Chunk* get_chunk_or(Chunk_Hash* chunk_hash, Vec2i chunk_pos, Chunk* if_not_found);
void clear_chunk_hash(Chunk_Hash* chunk_hash);



u64 hash64(u64 value) 
{
    //source: https://stackoverflow.com/a/12996028
    u64 hash = value;
    hash = (hash ^ (hash >> 30)) * (u64) 0xbf58476d1ce4e5b9;
    hash = (hash ^ (hash >> 27)) * (u64) 0x94d049bb133111eb;
    hash = hash ^ (hash >> 31);
    return hash;
}

u64 splat(Vec2i pos)
{
	return (u64) pos.y << 32 | (u64) pos.x;
}

bool is_power_of_two(i32 n) 
{
    return (n>0 && ((n & (n-1)) == 0));
}

enum Chunk_Hash_Flag
{
	CHUNK_EMPTY = 0,
	CHUNK_GREAVESTONE = 1,
	CHUNK_HASH_FLAG_OFFSET = 2
};

struct Hash_Slot
{
	Vec2i pos;

	//Index of the given chunk in the chunk array + 2 (CHUNK_HASH_FLAG_OFFSET).
	//0 means this slot is empty
	//1 means this slot is gravestone
	uint32_t chunk;
};

Chunk_Hash create_chunk_hash()
{
	return Chunk_Hash{0};
}

void destroy_chunk_hash(Chunk_Hash* chunk_hash)
{
	free(chunk_hash->chunks);
	free(chunk_hash->hash);
}

void clear_chunk_hash(Chunk_Hash* chunk_hash)
{
	memset(chunk_hash->hash, 0, chunk_hash->hash_capacity*sizeof(Hash_Slot));
	chunk_hash->chunk_size = 0;
}

Chunk* get_chunk(Chunk_Hash* chunk_hash, i32 index)
{
	assert(0 <= index && index < chunk_hash->chunk_size);
	return &chunk_hash->chunks[index];
}

i32 insert_chunk(Chunk_Hash* chunk_hash, Vec2i pos)
{
	PERF_COUNTER();
	//if is overfull rehash
	if(chunk_hash->chunk_size * 2 >= chunk_hash->hash_capacity)
	{
		PERF_COUNTER("rehash");
		i32 new_capacity = 16;
		i32 max = chunk_hash->chunk_size * 4;
		while(new_capacity < max)
			new_capacity *= 2;
		
		printf("... rehash to %lld B \n", (long long) (new_capacity * sizeof(Hash_Slot)));
		//Allocate new slots 
		Hash_Slot* new_hash = (Hash_Slot*) calloc(new_capacity * sizeof(Hash_Slot), new_capacity * sizeof(Hash_Slot));
		if(new_hash == nullptr)
		{
			printf("OUT OF MEMORY! ATTEMPTED TO GET %lld B", (long long int) new_capacity * sizeof(Hash_Slot));
			abort();
		}
		assert(is_power_of_two(new_capacity));

		//Go through all items and vec_add them to the new hash
		for(i32 i = 0; i < chunk_hash->hash_capacity; i++)
		{
			//skip empty or dead
			Hash_Slot* curr = &chunk_hash->hash[i];
			if(curr->chunk < CHUNK_HASH_FLAG_OFFSET)
				continue;
            
			//hash the non empty entry
			u64 at = curr->chunk - CHUNK_HASH_FLAG_OFFSET;
			u64 curr_splat = splat(curr->pos);
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

		free(chunk_hash->hash);
		chunk_hash->hash = new_hash;
		chunk_hash->hash_capacity = new_capacity;
	}

	//If has too little size for new entry grow twice the original size
	if(chunk_hash->chunk_size >= chunk_hash->chunk_capacity)
	{
		PERF_COUNTER("grow");
		i32 new_capacity = chunk_hash->chunk_capacity*2;
		if(new_capacity == 0)
			new_capacity = 16;

		Chunk* new_chunks = (Chunk*) realloc(chunk_hash->chunks, new_capacity*sizeof(Chunk));
		if(new_chunks == nullptr)
		{
			printf("OUT OF MEMORY! ATTEMPTED TO GET %lld B", (long long int) new_capacity*sizeof(Chunk));
			abort();
		}

		printf("... realloc to %lld B\n", (long long) new_capacity*sizeof(Chunk));

		chunk_hash->chunks = new_chunks;
		chunk_hash->chunk_capacity = new_capacity;
	}
	
	assert(is_power_of_two(chunk_hash->chunk_capacity));
	assert(is_power_of_two(chunk_hash->hash_capacity));
	assert(chunk_hash->hash_capacity > 0 && chunk_hash->chunk_capacity > 0);
	assert(chunk_hash->hash != nullptr && chunk_hash->chunks != nullptr);

	u64 pos_splat = splat(pos);
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

i32 find_chunk(Chunk_Hash* chunk_hash, Vec2i pos)
{
	PERF_COUNTER();
	if(chunk_hash->hash_capacity == 0)
		return -1;

	assert(is_power_of_two(chunk_hash->chunk_capacity));
	assert(is_power_of_two(chunk_hash->hash_capacity));
	assert(chunk_hash->hash_capacity > 0 && chunk_hash->chunk_capacity > 0);
	assert(chunk_hash->hash != nullptr && chunk_hash->chunks != nullptr);

	u64 hash = hash64(splat(pos));
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

Chunk* get_chunk_or(Chunk_Hash* chunk_hash, Vec2i chunk_pos, Chunk* if_not_found){
	i32 found = find_chunk(chunk_hash, chunk_pos);
	if(found == -1)
		return if_not_found;
	else
		return get_chunk(chunk_hash, found);
};