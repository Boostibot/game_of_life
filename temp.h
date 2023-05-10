#pragma once
#include <cstdio>
#include "chunk_hash.h"

void print_chunk(const Chunk* chunk)
{
	for(i32 y = 0; y < 64; y++)
	{
		for(i32 x = 0; x < 64; x ++)
		{
			u64 bit = (u64) 1 << x;
			if(chunk->data[y] & bit)
				printf("#");
			else
			{
				if(x == 0 || x == 63 || y == 0 || y == 63)
					printf("_");
				else
					printf(".");
			}

		}

		printf("\n");
	}
}

void print_bits(u64 val) 
{
	for(i32 i = 0; i < 64; i ++)
	{
		u64 bit = (val >> i) & 1;
		printf("%d ", (int) bit);
	}
		
	printf("\n");
};

#if 0
void chunk_textures()
{
	SDL_Texture* chunk_texture = SDL_CreateTexture(renderer,
                           SDL_PIXELFORMAT_BGRA8888,
                           SDL_TEXTUREACCESS_STREAMING, 
                           CHUNK_SIZE,
                           CHUNK_SIZE);

	SDL_Texture* clear_chunk_texture1 = SDL_CreateTexture(renderer,
                           SDL_PIXELFORMAT_BGRA8888,
                           SDL_TEXTUREACCESS_STREAMING, 
                           CHUNK_SIZE,
                           CHUNK_SIZE);
						   
	SDL_Texture* clear_chunk_texture2 = SDL_CreateTexture(renderer,
                           SDL_PIXELFORMAT_BGRA8888,
                           SDL_TEXTUREACCESS_STREAMING, 
                           CHUNK_SIZE,
                           CHUNK_SIZE);

	{
		uint32_t* pixels1 = nullptr;
		uint32_t* pixels2 = nullptr;
		int pitch = 0;
		SDL_LockTexture(clear_chunk_texture1, NULL, (void**) &pixels1, &pitch);
		SDL_LockTexture(clear_chunk_texture2, NULL, (void**) &pixels2, &pitch);
				
		for(i32 j = 0; j < CHUNK_SIZE; j++)
			for(i32 i = 0; i < CHUNK_SIZE; i ++)
			{
				pixels1[i + j*CHUNK_SIZE] = CLEAR_COLOR_1;
				pixels2[i + j*CHUNK_SIZE] = CLEAR_COLOR_2;
			}
		
		SDL_UnlockTexture(clear_chunk_texture1);
		SDL_UnlockTexture(clear_chunk_texture2);
	}
}
#endif