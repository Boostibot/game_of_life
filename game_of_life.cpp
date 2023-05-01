#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <SDL/SDL.h>

//needs to be lower or vec_equal to 64!
#define CHUNK_SIZE 64
#define MINIMAP_WIDTH 30
#define MINIMAP_HEIGHT 30
#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900
#define TARGET_MS 50

struct Vec2i
{
	int32_t x;
	int32_t y;
};

const static Vec2i DIRECTIONS[8] = {
	{-1, -1},
	{0, -1},
	{1, -1},
	{-1, 0},
	//{0, 0},
	{1, 0},
	{-1, 1},
	{0, 1},
	{1, 1},
};

Vec2i vec_add(Vec2i a, Vec2i b);
bool vec_equal(Vec2i a, Vec2i b);

//Chunk helpers
struct Chunk
{
	Vec2i pos;
	uint64_t data[CHUNK_SIZE];
};

bool get_at(const Chunk* chunk, Vec2i pos);
void set_at(Chunk* chunk, Vec2i pos);
bool get_at_chunk(Chunk* chunks[9], Vec2i pos);

//Chunk hashes
struct Hash_Slot;
struct Chunk_Hash
{
	Chunk* chunks;
	int32_t hash_capacity;
	int32_t chunk_size;
	int32_t chunk_capacity;
	
	Hash_Slot* hash;
};

Chunk_Hash create_chunk_hash();
void destroy_chunk_hash(Chunk_Hash* chunk_hash);

int32_t insert_chunk(Chunk_Hash* chunk_hash, Vec2i pos);
int32_t find_chunk(Chunk_Hash* chunk_hash, Vec2i pos);
Chunk* get_chunk(Chunk_Hash* chunk_hash, int32_t index);
void clear_chunk_hash(Chunk_Hash* chunk_hash);

//Rendering
void render_board(SDL_Renderer* renderer, SDL_Texture* chunk_texture, Chunk_Hash* chunk_hash, Vec2i chunk_pos, Vec2i place_pos, float scale, uint32_t clear_color);
void render_minimap(SDL_Renderer* renderer, SDL_Texture* minimap_texture, Chunk_Hash* chunk_hash);

//Loading
enum Parse_Error
{
	PARSE_ERROR_NONE = 0,
	PARSE_ERROR_BAD_DIMENSIONS,
	PARSE_ERROR_INVALID_CAHARCTER,
};

char* read_whole_file(const char* path);
Parse_Error parse_text_into_chunks(Chunk_Hash* chunk_hash, const char* read_data);

int main(int argc, char *argv[]) {

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
			return 1;
	
	char __STATIC_ASSERT_[0 < CHUNK_SIZE && CHUNK_SIZE <= 64 ? 1 : 0] = {0};

	SDL_Window* window = SDL_CreateWindow("Game of Life", 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	
	SDL_Texture* chunk_texture = SDL_CreateTexture(renderer,
                           SDL_PIXELFORMAT_BGRA8888,
                           SDL_TEXTUREACCESS_STREAMING, 
                           CHUNK_SIZE,
                           CHUNK_SIZE);
						   
	SDL_Texture* minimap_texture = SDL_CreateTexture(renderer,
                           SDL_PIXELFORMAT_BGRA8888,
                           SDL_TEXTUREACCESS_STREAMING, 
                           MINIMAP_WIDTH,
                           MINIMAP_HEIGHT);

	//We keep two hashes and swap between them on every uodate
	Chunk_Hash chunk_hash1 = create_chunk_hash();
	Chunk_Hash chunk_hash2 = create_chunk_hash();
	Chunk empty_chunks[9] = {0};

	Chunk_Hash* curr_chunk_hash = &chunk_hash1;
	Chunk_Hash* next_chunk_hash = &chunk_hash2;

	const char* filename = "map.txt";
	char* read = read_whole_file(filename);
	if(read == nullptr)
	{
		printf("Invalid input file!");
		abort();
	}
	
	Parse_Error parse_error = parse_text_into_chunks(curr_chunk_hash, read);
	switch(parse_error)
	{
		case PARSE_ERROR_BAD_DIMENSIONS:
			printf("Error reading file %s. The file must have the width and height on the fist line!\n"
				"File:\n=======\n%s\n=======", filename, read);
			abort();

		case PARSE_ERROR_INVALID_CAHARCTER:
			printf("Error reading file %s. The file must only contain 'X' (alive), '.' (dead) and newlines!\n"
				"File:\n=======\n%s\n=======", filename, read);
			abort();

		default: break;
	}
	
	//Insert all neigbouring chunks
	for(int32_t i = 0; i < curr_chunk_hash->chunk_size; i++)
	{
		for(int32_t i = 0; i < 8; i++)
			insert_chunk(curr_chunk_hash, DIRECTIONS[i]);
	}


	// main loop
	uint64_t last_ms = SDL_GetTicks64();
	while (1) 
	{
		// event handling
		SDL_Event e;
		if ( SDL_PollEvent(&e) ) 
		{
			if (e.type == SDL_QUIT)
				break;
			else if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_ESCAPE)
				break;
		} 
		
		// clear the screen
		SDL_RenderClear(renderer);

	    render_board(renderer, chunk_texture, curr_chunk_hash, Vec2i{-1, -1}, Vec2i{0, 0}, 1.0f/3, 0x070707FF);
	    render_board(renderer, chunk_texture, curr_chunk_hash, Vec2i{ 0, -1}, Vec2i{1, 0}, 1.0f/3, 0x111111FF);
	    render_board(renderer, chunk_texture, curr_chunk_hash, Vec2i{ 1, -1}, Vec2i{2, 0}, 1.0f/3, 0x070707FF);
		
	    render_board(renderer, chunk_texture, curr_chunk_hash, Vec2i{-1,  0}, Vec2i{0, 1}, 1.0f/3, 0x111111FF);
	    render_board(renderer, chunk_texture, curr_chunk_hash, Vec2i{ 0,  0}, Vec2i{1, 1}, 1.0f/3, 0x070707FF);
	    render_board(renderer, chunk_texture, curr_chunk_hash, Vec2i{ 1,  0}, Vec2i{2, 1}, 1.0f/3, 0x111111FF);
		
	    render_board(renderer, chunk_texture, curr_chunk_hash, Vec2i{-1,  1}, Vec2i{0, 2}, 1.0f/3, 0x070707FF);
	    render_board(renderer, chunk_texture, curr_chunk_hash, Vec2i{ 0,  1}, Vec2i{1, 2}, 1.0f/3, 0x111111FF);
	    render_board(renderer, chunk_texture, curr_chunk_hash, Vec2i{ 1,  1}, Vec2i{2, 2}, 1.0f/3, 0x070707FF);

		render_minimap(renderer, minimap_texture, curr_chunk_hash);
		
		SDL_Rect minimap_rect = {}; 
		minimap_rect.x = WINDOW_HEIGHT; 
		minimap_rect.y = 0; 
		minimap_rect.w = WINDOW_WIDTH - WINDOW_HEIGHT; 
		minimap_rect.h = WINDOW_WIDTH - WINDOW_HEIGHT; 
		SDL_RenderCopy(renderer, minimap_texture, NULL, &minimap_rect);
		SDL_RenderPresent(renderer);

		if(SDL_GetTicks64() - last_ms >= TARGET_MS)
		{
			int32_t alive_count = 0;
			for(int32_t i = 0; i < curr_chunk_hash->chunk_size; i++)
			{
				Chunk* chunk = &curr_chunk_hash->chunks[i];

				//Count all alive
				for(int32_t j = 0; j < CHUNK_SIZE; j++)
				{
					for(int32_t k = 0; k < CHUNK_SIZE; k++)
						alive_count += get_at(chunk, Vec2i{j, k});
				}

				Vec2i all_directions[9] = {
					{-1, -1}, {0, -1}, {1, -1},
					{-1, 0},  {0, 0},  {1, 0},
					{-1, 1},  {0, 1},  {1, 1},
				};

				//Get all surounding chunks 
				Chunk* surrounding[9] = {0};
				for(int32_t i = 0; i < 9; i++)
				{
					Vec2i pos = vec_add(chunk->pos, all_directions[i]);
					int32_t found = find_chunk(curr_chunk_hash, pos);
					if(found == -1)
					{
						surrounding[i] = &empty_chunks[i];
						surrounding[i]->pos = pos;
					}
					else
					{
						surrounding[i] = get_chunk(curr_chunk_hash, found);
					}
				}

				//Any live cell with two or three live neighbours survives.
				//Any dead cell with three live neighbours becomes a live cell.
				//All other live cells die in the next generation. Similarly, all other dead cells stay dead.

				Chunk new_chunk = {0};
				new_chunk.pos = surrounding[4]->pos;
				for(int32_t y = 0; y < CHUNK_SIZE; y++)
				{
					for(int32_t x = 0; x < CHUNK_SIZE; x++)
					{
						Vec2i curr_pos = Vec2i{x, y};
						bool is_alive = get_at(surrounding[4], curr_pos);
						int32_t neighbours = 0;
						for(int32_t i = 0; i < 8; i++)
							neighbours += get_at_chunk(surrounding, vec_add(curr_pos, DIRECTIONS[i]));

						if(is_alive)
						{
							if(neighbours == 2 || neighbours == 3)
								set_at(&new_chunk, curr_pos);
						}
						else
						{
							if(neighbours == 3)
								set_at(&new_chunk, curr_pos);
						}
					}
				}

				uint64_t acummulated = 0;
				for(int32_t i = 0; i < CHUNK_SIZE; i++)
					acummulated |= new_chunk.data[i];

				//Unless chunk is comletely dead insert itself alongside all neigboring chunks chunk_hash the next generation
				if(acummulated != 0)
				{
					int32_t curr_i = insert_chunk(next_chunk_hash, chunk->pos);
					*get_chunk(next_chunk_hash, curr_i) = new_chunk;
					for(int32_t k = 0; k < 8; k++)
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, DIRECTIONS[k]));
				}
			}

			//Swap chunk hashes
			Chunk_Hash* temp = curr_chunk_hash;
			curr_chunk_hash = next_chunk_hash;
			next_chunk_hash = temp;
			clear_chunk_hash(next_chunk_hash);

			last_ms = SDL_GetTicks64();	
			printf("Last iter alive: %d\n", (int) alive_count);
		}

	}
	
	SDL_DestroyTexture(chunk_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	destroy_chunk_hash(&chunk_hash1);
	destroy_chunk_hash(&chunk_hash2);
	
	SDL_Quit(); 
	return 0;
}

Vec2i vec_add(Vec2i a, Vec2i b)
{
	return Vec2i{a.x + b.x, a.y + b.y};
}

bool vec_equal(Vec2i a, Vec2i b)
{
	return a.x == b.x && a.y == b.y;
}

bool get_at_chunk(Chunk* chunks[9], Vec2i pos)
{
	int32_t fixed_x = CHUNK_SIZE + pos.x;
	int32_t fixed_y = CHUNK_SIZE + pos.y;

	assert(0 <= fixed_x && fixed_x < CHUNK_SIZE*3);
	assert(0 <= fixed_y && fixed_y < CHUNK_SIZE*3);

	int32_t chunk_i_x = fixed_x / CHUNK_SIZE;
	int32_t chunk_i_y = fixed_y / CHUNK_SIZE;
	int32_t chunk_i = chunk_i_x + chunk_i_y*3;
	assert(0 <= chunk_i && chunk_i < 9);

	int32_t i = (fixed_x % CHUNK_SIZE) + (fixed_y % CHUNK_SIZE)*CHUNK_SIZE;
	assert(0 <= i);
	
	int32_t mask_i = i / 64;
	int32_t bit_i = i % 64;
	uint64_t bit = (uint64_t) 1 << bit_i;
	Chunk* chunk = chunks[chunk_i];
	return !!(chunk->data[mask_i] & bit);
}

bool get_at(const Chunk* chunk, Vec2i pos)
{
	assert(0 <= pos.x && pos.x < CHUNK_SIZE);
	assert(0 <= pos.y && pos.y < CHUNK_SIZE);
	int32_t i = pos.x + pos.y*CHUNK_SIZE;

	int32_t mask_i = i / 64;
	int32_t bit_i = i % 64;
	uint64_t bit = (uint64_t) 1 << bit_i;

	return (chunk->data[mask_i] & bit) > 0;
}

void set_at(Chunk* chunk, Vec2i pos)
{
	assert(0 <= pos.x && pos.x < CHUNK_SIZE);
	assert(0 <= pos.y && pos.y < CHUNK_SIZE);
	int32_t i = pos.x + pos.y*CHUNK_SIZE;

	int32_t mask_i = i / 64;
	int32_t bit_i = i % 64;
	uint64_t bit = (uint64_t) 1 << bit_i;

	chunk->data[mask_i] |= bit;
}

uint64_t hash64(uint64_t value) 
{
    //source: https://stackoverflow.com/a/12996028
    uint64_t hash = value;
    hash = (hash ^ (hash >> 30)) * (uint64_t) 0xbf58476d1ce4e5b9;
    hash = (hash ^ (hash >> 27)) * (uint64_t) 0x94d049bb133111eb;
    hash = hash ^ (hash >> 31);
    return hash;
}

uint64_t splat(Vec2i pos)
{
	return (uint64_t) pos.y << 32 | (uint64_t) pos.x;
}

bool is_power_of_two(int32_t n) 
{
    return (n>0 && ((n & (n-1)) == 0));
}

enum Chunk_State
{
	CHUNK_EMPTY = 0,
	CHUNK_GREAVESTONE = 1,
	CHUNK_STATE_OFFSET = 2
};

struct Hash_Slot
{
	Vec2i pos;

	//Index of the given chunk in the chunk array + 2 (CHUNK_STATE_OFFSET).
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

Chunk* get_chunk(Chunk_Hash* chunk_hash, int32_t index)
{
	assert(0 <= index && index < chunk_hash->chunk_size);
	return &chunk_hash->chunks[index];
}

int32_t insert_chunk(Chunk_Hash* chunk_hash, Vec2i pos)
{
	//if is overfull rehash
	if(chunk_hash->chunk_size * 2 >= chunk_hash->hash_capacity)
	{
		int32_t new_capacity = 16;
		int32_t max = chunk_hash->chunk_size * 2;
		while(new_capacity < max)
			new_capacity *= 2;
		
		//Allocate new slots 
		Hash_Slot* new_hash = (Hash_Slot*) calloc(new_capacity * sizeof(Hash_Slot), new_capacity * sizeof(Hash_Slot));
		if(new_hash == nullptr)
		{
			printf("OUT OF MEMORY! ATTEMPTED TO GET %lld B", (long long int) new_capacity * sizeof(Hash_Slot));
			abort();
		}
		assert(is_power_of_two(new_capacity));

		//Go through all items and vec_add them to the new hash
		for(int32_t i = 0; i < chunk_hash->hash_capacity; i++)
		{
			//skip empty or dead
			Hash_Slot* curr = &chunk_hash->hash[i];
			if(curr->chunk < CHUNK_STATE_OFFSET)
				continue;
            
			//hash the non empty entry
			uint64_t at = curr->chunk - CHUNK_STATE_OFFSET;
			uint64_t curr_splat = splat(curr->pos);
			uint64_t hash = hash64(curr_splat);

			//find an empty slot in the new array to place the entry
			//we use & instead of % because its faster and we know that 
			// new_capacity is always power of two
			uint64_t mask = (uint64_t) new_capacity - 1; 
			uint64_t k = hash & mask;
			int32_t counter = 0;
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
		int32_t new_capacity = chunk_hash->chunk_capacity*2;
		if(new_capacity == 0)
			new_capacity = 16;

		Chunk* new_chunks = (Chunk*) realloc(chunk_hash->chunks, new_capacity*sizeof(Chunk));
		if(new_chunks == nullptr)
		{
			printf("OUT OF MEMORY! ATTEMPTED TO GET %lld B", (long long int) new_capacity*sizeof(Chunk));
			abort();
		}

		chunk_hash->chunks = new_chunks;
		chunk_hash->chunk_capacity = new_capacity;
	}
	
	assert(is_power_of_two(chunk_hash->chunk_capacity));
	assert(is_power_of_two(chunk_hash->hash_capacity));
	assert(chunk_hash->hash_capacity > 0 && chunk_hash->chunk_capacity > 0);
	assert(chunk_hash->hash != nullptr && chunk_hash->chunks != nullptr);

	uint64_t pos_splat = splat(pos);
	uint64_t hash = hash64(pos_splat);
	uint64_t mask = (uint64_t) chunk_hash->hash_capacity - 1;
	uint64_t i = hash & mask;

	//Find empty slot for the newly added. If it is already in the hash
	// return its index
	int32_t counter = 0;
	for(; chunk_hash->hash[i].chunk > 0; i = (i + 1) & mask)
	{
		assert(counter ++ < chunk_hash->hash_capacity && "there must be an empty slot!");
		if(vec_equal(chunk_hash->hash[i].pos, pos))
			return (int32_t) chunk_hash->hash[i].chunk - CHUNK_STATE_OFFSET;
	}

	//Push back the new chunk
	assert(chunk_hash->chunk_size < chunk_hash->chunk_capacity);
	chunk_hash->chunks[chunk_hash->chunk_size] = Chunk{0};
	chunk_hash->chunks[chunk_hash->chunk_size].pos = pos;
	
	//Link it in the hash
	chunk_hash->hash[i].chunk = (uint32_t) chunk_hash->chunk_size + CHUNK_STATE_OFFSET;
	chunk_hash->hash[i].pos = pos;

	chunk_hash->chunk_size ++;
	return chunk_hash->chunk_size - 1;
}


int32_t find_chunk(Chunk_Hash* chunk_hash, Vec2i pos)
{
	if(chunk_hash->hash_capacity == 0)
		return -1;

	assert(is_power_of_two(chunk_hash->chunk_capacity));
	assert(is_power_of_two(chunk_hash->hash_capacity));
	assert(chunk_hash->hash_capacity > 0 && chunk_hash->chunk_capacity > 0);
	assert(chunk_hash->hash != nullptr && chunk_hash->chunks != nullptr);

	uint64_t hash = hash64(splat(pos));
	uint64_t mask = (uint64_t) chunk_hash->hash_capacity - 1;
	uint64_t i = hash & mask;
	int32_t counter = 0;
	for(; chunk_hash->hash[i].chunk > 0; i = (i + 1) & mask)
	{
		assert(counter ++ < chunk_hash->hash_capacity && "there must be an empty slot!");
		if(vec_equal(chunk_hash->hash[i].pos, pos))
			return chunk_hash->hash[i].chunk - CHUNK_STATE_OFFSET;
	}

	return -1;
}

void render_board(SDL_Renderer* renderer, SDL_Texture* chunk_texture, Chunk_Hash* chunk_hash, Vec2i chunk_pos, Vec2i place_pos, float scale, uint32_t clear_color)
{
	uint32_t* pixels = nullptr;
	int pitch = 0;
	SDL_LockTexture(chunk_texture, NULL, (void**) &pixels, &pitch);

	int32_t middle_chunk_i = find_chunk(chunk_hash, chunk_pos);
	if(middle_chunk_i == -1)
	{
		for(int32_t j = 0; j < CHUNK_SIZE; j++)
			for(int32_t i = 0; i < CHUNK_SIZE; i ++)
				pixels[i + j*CHUNK_SIZE] = clear_color;
	}
	else
	{
		Chunk* middle_chunk = get_chunk(chunk_hash, middle_chunk_i);
		for(int32_t j = 0; j < CHUNK_SIZE; j++)
			for(int32_t i = 0; i < CHUNK_SIZE; i ++)
			{
				if(get_at(middle_chunk, Vec2i{i, j}))
					pixels[i + j*CHUNK_SIZE] = (uint32_t) -1; 
				else
					pixels[i + j*CHUNK_SIZE] = clear_color;
			}
			
	}
		
	SDL_UnlockTexture(chunk_texture);
			
	float size = WINDOW_HEIGHT * scale;

	SDL_Rect board_rect = {}; 
	board_rect.x = (int) (place_pos.x * size); 
	board_rect.y = (int) (place_pos.y * size); 
	board_rect.w = (int) size; 
	board_rect.h = (int) size; 
	SDL_RenderCopy(renderer, chunk_texture, NULL, &board_rect);
}


void render_minimap(SDL_Renderer* renderer, SDL_Texture* minimap_texture, Chunk_Hash* curr_chunk_hash)
{
	uint32_t* pixels = nullptr;
	int pitch = 0;
	SDL_LockTexture(minimap_texture, NULL, (void**) &pixels, &pitch);

	//clear all pixels to base color
	for(int32_t j = 0; j < MINIMAP_HEIGHT; j++)
		for(int32_t i = 0; i < MINIMAP_WIDTH; i ++)
		{
			pixels[i + j*MINIMAP_HEIGHT] = 0x000005500;
		}

	//Draw all loaded chunks chunk_hash the minimap
	for(int32_t i = 0; i < curr_chunk_hash->chunk_size; i++)
	{
		int32_t w2 = MINIMAP_WIDTH / 2;
		int32_t h2 = MINIMAP_HEIGHT / 2;
		Chunk* chunk = &curr_chunk_hash->chunks[i];

		int32_t draw_at_x = chunk->pos.x + w2;
		int32_t draw_at_y = chunk->pos.y + w2;
		int32_t draw_at = draw_at_x + draw_at_y*MINIMAP_HEIGHT;

		//If is within the drawable are draw it;
		if(0 <= draw_at_x && draw_at_x <= MINIMAP_WIDTH
			&& 0 <= draw_at_y && draw_at_y <= MINIMAP_WIDTH)
		{
			//sum all masks that contain a set bit to 
			// estimate how much it is used
			uint32_t inetnsity = 0;
			for(int32_t k = 0; k < CHUNK_SIZE; k++)
				inetnsity += chunk->data[k] > 0;

			//scale it and cap it
			inetnsity *= 4;
			if(inetnsity >= 256)
				inetnsity = 255;

			//make it chunk_hash a gray color
			inetnsity = inetnsity << 24 | inetnsity << 16 | inetnsity << 8 | 0xFF;
			pixels[draw_at_x + draw_at_y*MINIMAP_HEIGHT] = inetnsity; 
		}
	}

	SDL_UnlockTexture(minimap_texture);
}

char* read_whole_file(const char* path)
{
	size_t alloced_size = 0;
	size_t data_size = 0;
	size_t chunk_size = 1024;
	char* data = nullptr;
	
	FILE* file = fopen(path, "rb");
	if(file == nullptr)
		return nullptr;

	while(true)
	{
		if(data_size + chunk_size + 1 > alloced_size)
		{
			size_t new_size = (data_size + chunk_size) * 2 + 1;
			void* new_data = realloc(data, new_size);
			if(new_data == nullptr)
				break;

			data = (char*) new_data;
		}

        size_t read = fread(data + data_size, 1, chunk_size, file);
        if (read == 0)
            break;

        data_size += read;
		chunk_size = chunk_size * 3/2;
	}
	
	if(data != nullptr)
		data[data_size - 1] = '\0';
		
	fclose(file);
	return data;
}


Parse_Error parse_text_into_chunks(Chunk_Hash* chunk_hash, const char* read_data)
{
	Parse_Error error;
	int width = -1;
	int height = -1;
	int32_t size = (int32_t) strlen(read_data);
	int32_t line_count = 0;
	int32_t line_size = 0;

	for(int32_t i = 0; i < size; i += line_size + 1, line_count ++)
    {
		while((read_data[i] == '\r' || read_data[i] == '\n') && read_data[i] != '\0')
			i++;

		line_size = 0;
		while(true)
		{
			char c = read_data[line_size + i];
			if(c == '\n' || c == '\r' || c == '\0')
				break;
		
			line_size++;
		}
		
		const char* line = read_data + i;

		//we just flat out ignore the first two lines
		if(line_count < 2)
		{
			int state = 0;
			
			if(line_count == 0)
				state = sscanf(line, "%d", &width);
			if(line_count == 1)
				state = sscanf(line, "%d", &height);

			if(state < 1)
			{
				return PARSE_ERROR_BAD_DIMENSIONS;
			}
				
			continue;
		}

        //translate line and add it
		int32_t y = line_count;
        for(int32_t x = 0; x < line_size; x++)
        {
			char c = line[x];
            switch(c)
            {
                case 'X': {
				
					int32_t offset_x = x - (int32_t) width/2 + CHUNK_SIZE/2; 
					int32_t offset_y = y - (int32_t) height/2  + CHUNK_SIZE/2;
				
					int32_t chunk_x = offset_x / CHUNK_SIZE;
					int32_t chunk_y = offset_y / CHUNK_SIZE;
					
					int32_t local_x = (offset_x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
					int32_t local_y = (offset_y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
					
					int32_t chunk_i = insert_chunk(chunk_hash, Vec2i{chunk_x, chunk_y});
					Chunk* chunk = get_chunk(chunk_hash, chunk_i);
					assert(chunk != nullptr);
					set_at(chunk, Vec2i{local_x, local_y});
				}

				break;
                case '-': break;

                default: 
					return PARSE_ERROR_INVALID_CAHARCTER;
			}
        }
    }

	return PARSE_ERROR_NONE;
}