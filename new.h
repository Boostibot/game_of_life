#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <SDL/SDL.h>

//needs to be lower or vec_equal to 64!
#define CHUNK_SIZE 61
#define MINIMAP_WIDTH 30
#define MINIMAP_HEIGHT 30
#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900
#define TARGET_MS 50

#define UPDATE_SCREEN true
#define UPDATE_SYMULATION true
#define UPDATE_INPUT true

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef bool b8;
typedef uint16_t b16;
typedef uint32_t b32;
typedef uint64_t b64;

typedef float f32;
typedef double f64;

typedef u8 byte;
typedef const char* cstring;

typedef ptrdiff_t isize;
typedef size_t usize;

struct Vec2i
{
	i32 x;
	i32 y;
};

struct Vec2f64
{
	f64 x;
	f64 y;
};

const static Vec2i DIRECTIONS[8] = {
	{-1, -1},{0, -1},{1, -1},
	{-1, 0},         {1,  0},
	{-1,  1},{0,  1},{1,  1},
};

Vec2i vec_add(Vec2i a, Vec2i b);
Vec2i vec_sub(Vec2i a, Vec2i b);
Vec2i vec_mul(i32 a, Vec2i b);
bool vec_equal(Vec2i a, Vec2i b);

//Chunk helpers
struct Chunk
{
	Vec2i pos;
	u64 data[64];
};

bool get_at(const Chunk* chunk, Vec2i pos);
void set_at(Chunk* chunk, Vec2i pos);
bool get_at_chunk(Chunk* chunks[9], Vec2i pos);

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
void clear_chunk_hash(Chunk_Hash* chunk_hash);

//Rendering
void render_minimap(SDL_Renderer* renderer, SDL_Texture* minimap_texture, Chunk_Hash* chunk_hash);

inline int64_t perf_counter();
inline int64_t perf_counter_freq();
inline int64_t clock_ns();
inline double  clock_s();

//Loading
enum Parse_Error
{
	PARSE_ERROR_NONE = 0,
	PARSE_ERROR_BAD_DIMENSIONS,
	PARSE_ERROR_INVALID_CAHARCTER,
};

char* read_whole_file(const char* path);
Parse_Error parse_text_into_chunks(Chunk_Hash* chunk_hash, const char* read_data);

#define MAX_PERF_COUNTERS 1000

struct Perf_Counter
{
	i64 counter;
	i64 runs;
	i64 line;
	const char* file;
	const char* function;
	const char* name;
};

static Perf_Counter perf_counters[MAX_PERF_COUNTERS] = {0};

struct Perf_Counter_Executor
{
	i64 start = 0;
	i64 index = 0;
	i64 line = 0;
	const char* file = nullptr;
	const char* function = nullptr;
	const char* name = nullptr;

	Perf_Counter_Executor(i64 _index, i64 _line, const char* _file, const char* _function, const char* _name = nullptr)
	{
		start = perf_counter();
		index = _index;
		line = _line;
		file = _file;
		function = _function;
		name = _name;
	}

	~Perf_Counter_Executor()
	{
		i64 delta = perf_counter() - start;
		perf_counters[index].counter += delta;
		perf_counters[index].file = file;
		perf_counters[index].line = line;
		perf_counters[index].function = function;
		perf_counters[index].name = name;
		perf_counters[index].runs += 1;
	}
};

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

#define PERF_COUNTER(...) Perf_Counter_Executor CONCAT(__perf_counter__, __LINE__)(__COUNTER__, __LINE__, __FILE__, __FUNCTION__, ##__VA_ARGS__)

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
	
	const auto get_chunk_pos = [](Vec2i sym_position){
		Vec2i output = {0};
		if(sym_position.x >= 0)
			output.x = sym_position.x / CHUNK_SIZE;
		else
			output.x = (sym_position.x - CHUNK_SIZE + 1) / CHUNK_SIZE;

		if(sym_position.y >= 0)
			output.y = sym_position.y / CHUNK_SIZE;
		else
			output.y = (sym_position.y - CHUNK_SIZE + 1) / CHUNK_SIZE;

		return output;
	};
	
	const auto get_pixel_pos = [](Vec2i sym_position){
		Vec2i output = {
			(sym_position.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE,
			(sym_position.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE,
		};

		return output;
	};

	const auto to_screen_pos = [](Vec2f64 sym_position, Vec2f64 sym_center, Vec2i screen_center, f64 zoom){
		Vec2i screen_offset = {
			(i32) floor((sym_position.x - sym_center.x) * zoom),
			(i32) floor((sym_position.y - sym_center.y) * zoom)
		};

		Vec2i screen_position = vec_add(screen_offset, screen_center);
		return screen_position;
	};
	
	const auto to_sym_pos = [](Vec2i screen_position, Vec2f64 sym_center, Vec2i screen_center, f64 zoom){
		Vec2i screen_offset = vec_sub(screen_position, screen_center);

		Vec2f64 sym_position = {
			(f64) screen_offset.x / zoom + sym_center.x,
			(f64) screen_offset.y / zoom + sym_center.y,
		};

		return sym_position;
	};

	const auto set_pixel_at = [&](Chunk_Hash* chunk_hash, Vec2i sym_pos){
		Vec2i place_at_chunk = get_chunk_pos(sym_pos);
		Vec2i place_at_pixel = get_pixel_pos(sym_pos);

		i32 chunk_i = insert_chunk(curr_chunk_hash, place_at_chunk);
		Chunk* chunk = get_chunk(curr_chunk_hash, chunk_i);
		set_at(chunk, place_at_pixel);

		for(i32 i = 0; i < 8; i++)
			insert_chunk(curr_chunk_hash, vec_add(DIRECTIONS[i], place_at_chunk));
	};
	
	const auto get_chunk_or = [](Chunk_Hash* chunk_hash, Vec2i chunk_pos, Chunk* if_not_found){
		i32 found = find_chunk(chunk_hash, chunk_pos);
		if(found == -1)
			return if_not_found;
		else
			return get_chunk(chunk_hash, found);
	};

	const auto set_pixel_at_f = [&](Chunk_Hash* chunk_hash, Vec2f64 sym_posf){
		Vec2i sym_pos = {
			(i32) round(sym_posf.x),
			(i32) round(sym_posf.y),
		};

		set_pixel_at(chunk_hash, sym_pos);
	};
	
	const auto draw_chunk = [&](Chunk* chunk, Vec2i chunk_pos_sym, Vec2f64 sym_center, Vec2i screen_center, f64 zoom, SDL_Texture* chunk_texture, SDL_Renderer* renderer){
		Vec2f64 sym_pos_top = {(f64) chunk_pos_sym.x * CHUNK_SIZE, (f64) chunk_pos_sym.y * CHUNK_SIZE};
		Vec2f64 sym_pos_bot = {(f64) (chunk_pos_sym.x + 1) * CHUNK_SIZE, (f64) (chunk_pos_sym.y + 1) * CHUNK_SIZE};

		Vec2i screen_pos_top = to_screen_pos(sym_pos_top, sym_center, screen_center, zoom);
		Vec2i screen_pos_bot = to_screen_pos(sym_pos_bot, sym_center, screen_center, zoom);
		Vec2i screen_size = vec_sub(screen_pos_bot, screen_pos_top);

		u32 clear_color = 0x111111FF;
		if((chunk_pos_sym.y % 2 + chunk_pos_sym.x) % 2)
			clear_color = 0x070707FF;

		uint32_t* pixels = nullptr;
		int pitch = 0;
		SDL_LockTexture(chunk_texture, NULL, (void**) &pixels, &pitch);
				
		for(i32 j = 0; j < CHUNK_SIZE; j++)
			for(i32 i = 0; i < CHUNK_SIZE; i ++)
			{
				if(get_at(chunk, Vec2i{i, j}))
					pixels[i + j*CHUNK_SIZE] = (uint32_t) -1; 
				else
					pixels[i + j*CHUNK_SIZE] = clear_color;
			}
			
		
		SDL_UnlockTexture(chunk_texture);
			
		SDL_Rect dest_rect = {}; 
		dest_rect.x = (int) screen_pos_top.x; 
		dest_rect.y = (int) screen_pos_top.y; 
		dest_rect.w = (int) screen_size.x; 
		dest_rect.h = (int) screen_size.y; 
		SDL_RenderCopy(renderer, chunk_texture, NULL, &dest_rect);
	};

	const auto update_screen = [&](Vec2i top_chunk, Vec2i bot_chunk, Vec2f64 sym_center, Vec2i screen_center, f64 zoom, Chunk_Hash* chunk_hash, SDL_Texture* chunk_texture, SDL_Renderer* renderer){
		SDL_RenderClear(renderer);
		PERF_COUNTER();
		for(i32 chunk_y = top_chunk.y; chunk_y < bot_chunk.y; chunk_y++)
		{
			for(i32 chunk_x = top_chunk.x; chunk_x < bot_chunk.x; chunk_x++)
			{
				Vec2i chunk_pos_sym = Vec2i{chunk_x, chunk_y};
				Chunk* chunk = get_chunk_or(chunk_hash, chunk_pos_sym, empty_chunks);
				draw_chunk(chunk, chunk_pos_sym, sym_center, screen_center, zoom, chunk_texture, renderer);
			}
		}
		SDL_RenderPresent(renderer);
	};

	{
		const auto set_at = [](u64 data[64], i32 x, i32 y) {
			
			i32 i = x + y*64;

			i32 mask_i = i / 64;
			i32 bit_i = i % 64;
			u64 bit = (u64) 1 << bit_i;

			data[mask_i] |= bit;
		};
		
		const auto get_at = [](u64 data[64], i32 x, i32 y) {
			u64 bit = (u64) 1 << x;

			return (bool) (data[y] & bit);
		};
		
		const auto print_oct = [](const char* label, u64 val) {
			printf("%s", label);
			for(i32 i = 0; i < 64; i+= 3)
			{
				u64 oct = (val >> i) & 0b111;
				printf("%d ", (int) oct);
			}

			printf("\n");
		};
		
		const auto print_oct_board = [](const char* label, u8 arr[64][64]) {
			printf("\n%s\n", label);
			for(i32 y = 0; y < 64; y += 1)
			{
				for(i32 x = 0; x < 64; x += 1)
				{
					printf("%d ", (int) arr[y][x]);
				}
				printf("\n");
			}
		};
		
		const auto splice_into = [](u64 val, i32 k, u8 arr[64]) {
			for(i32 i = k; i < 64; i+= 3)
			{
				u64 oct = (val >> (i - k)) & 0b111;
				assert(arr[i] == 0);
				arr[i] = oct;
			}
		};
		
		u64 data[64] = {0};
		u64 new_data[64] = {0};

		//0 0 1 1 1 0 0 0 1 1 0 1 1 1 0 0
		//0 0 0 0 0 0 1 1 1 0 0 0 1 1 1 0
		//flier
		//set_at(data, 2, 1);
		//set_at(data, 3, 2);
		//set_at(data, 3, 3);
		//set_at(data, 2, 3);
		//set_at(data, 1, 3);

		////test
		//set_at(data, 15, 1);
		

		//Test overfull
		//set_at(data, 10, 10);
		//set_at(data, 11, 10);
		//set_at(data, 12, 10);
		//
		//set_at(data, 10, 11);
		//set_at(data, 11, 11);
		//set_at(data, 12, 11);
		//
		//set_at(data, 10, 12);
		//set_at(data, 11, 12);
		//set_at(data, 12, 12);


		//Test line
		for(i32 x = 1; x < 62; x++)
			set_at(data, x, 30);

		for(i32 y = 0; y < 64; y++)
		{
			for(isize x = 0; x < 64; x ++)
			{
				if(get_at(data, x, y))
					printf("#");
				else
					printf(".");
			}

			printf("\n");
		}
		printf("END\n\n");
		
		i32 print_from = 0;
		i32 print_to = 0;
		i32 repeats = 0;
		u64 sliding_mask = 0b111;
		u64 pattern = 011'11111'11111'11111'11111;
		u64 oct0 = pattern << 0; //pattern of 0b001001...
		u64 oct1 = pattern << 1; //pattern of 0b010010...
		u64 oct2 = pattern << 2; //pattern of 0b100100...
		print_bits(pattern);

		u64 data1 = data[1];
		print_bits(data1);
			
		u8 accumulated[64] = {0};

		for(i32 slot = 0; slot < 3; slot++)
		{
			u64 slid = (data1 << 1);
			u64 curr_accumulator = 0;
			curr_accumulator += (oct0 & (slid >> (0 + slot)));
			curr_accumulator += (oct0 & (slid >> (1 + slot)));
			curr_accumulator += (oct0 & (slid >> (2 + slot)));
			splice_into(curr_accumulator, slot, accumulated);
		}

		for(i32 x = 0; x < 64; x += 1)
		{
			printf("%d ", (int) accumulated[x]);
		}
		printf("\n");

		for(i32 l = 0; l < repeats; l++)
		{
			u8 accumulators_[64][64] = {0};
			u8 first_sum_	[64][64] = {0};
			u8 has_4_		[64][64] = {0};
			u8 has_any_		[64][64] = {0};
			u8 is_overfull_	[64][64] = {0};
			u8 complete_sum_[64][64] = {0};
			u8 complete_sum_stage_1_[64][64] = {0};
			u8 complete_sum_stage_2_[64][64] = {0};
			u8 is_alive_	[64][64] = {0};
			u8 is_three_	[64][64] = {0};
			u8 is_four_		[64][64] = {0};
			u8 is_next_alive_[64][64] = {0};

			for(i32 y = 0; y < 64; y++)
				new_data[y] = 0;
				
			//u64 pattern = 0111111; //pattern of 0b001001... repeating (in oct)
			

			//for all three offsets in the 3 bit slots
			for(i32 slot = 0; slot < 3; slot++)
			{
				u64 accumulators[64] = {0};
				

				for(i32 i = 0; i < 64; i++)
				{
					u64 curr_accumulator = 0;
					u64 slid = (data[i] << 1); 
					//we have to align it so that its in the ceneter of the oct
					//otherwise for     0b 0 0 1 0 0 0 1 0
					//we woudl geenrate    1 1 1 0 1 1 1 0
					//but we want:         0 1 1 1 0 1 1 1

					curr_accumulator += (oct0 & (slid >> (0 + slot)));
					curr_accumulator += (oct0 & (slid >> (1 + slot)));
					curr_accumulator += (oct0 & (slid >> (2 + slot)));
					accumulators[i] = curr_accumulator;
					splice_into(accumulators[i], slot, accumulators_[i]);
				}

				//iterate all inner cells of the chunk
				for(i32 y = 1; y < 63; y++)
				{
					u64 first_sum = accumulators[y - 1] + accumulators[y];
					u64 has_4 = first_sum & oct2;
					
					u64 has_any = ((accumulators[y + 1] & oct1) << 1 | (accumulators[y + 1] & oct0) << 2);

					u64 is_overfull = has_4 & has_any;
					u64 complete_sum_stage_1 = ~is_overfull & first_sum;
					u64 complete_sum_stage_2 = accumulators[y + 1];
					u64 complete_sum = (~is_overfull & first_sum) + accumulators[y + 1];

					u64 three_pattern = oct0 | oct1;
					u64 four_pattern = oct2;

					u64 three_check = three_pattern ^ complete_sum; //completely 0 if is three
					u64 four_check = four_pattern ^ complete_sum; //completely 0 if is four

					u64 is_not_three = (three_check & oct0) << 2 | (three_check & oct1) << 1 | (three_check & oct2) << 0;
					u64 is_not_four  = (four_check & oct0) << 2 | (four_check & oct1) << 1 | (four_check & oct2) << 0;    
					u64 is_alive     = (oct0 & (data[y] >> slot)) << 2; 
					
					u64 is_three = ~is_not_three; 
					u64 is_four = ~is_not_four; 
					u64 is_next_alive = (is_three | (is_alive & is_four)) & ~is_overfull;

					is_next_alive &= oct2;

					new_data[y] |= (is_next_alive >> (2 - slot));
					splice_into(first_sum, slot, first_sum_[y]);
					splice_into(has_4, slot, has_4_	[y]);
					splice_into(has_any, slot, has_any_	[y]);
					splice_into(is_overfull, slot, is_overfull_[y]);
					splice_into(complete_sum, slot, complete_sum_[y]);
					splice_into(complete_sum_stage_1, slot, complete_sum_stage_1_[y]);
					splice_into(complete_sum_stage_2, slot, complete_sum_stage_2_[y]);
					splice_into(is_three, slot, is_three_[y]);
					splice_into(is_four, slot, is_four_	[y]);
					splice_into(is_alive, slot, is_alive_	[y]);
					splice_into(is_next_alive, slot, is_next_alive_[y]);
				}
			}
			 
			if(print_from <= l && l < print_to)
			{
				print_oct_board("accumulators_", accumulators_);
				//print_oct_board("first_sum_", first_sum_);
				//print_oct_board("has_4_", has_4_);
				//print_oct_board("has_any_", has_any_);
				//print_oct_board("is_overfull_", is_overfull_);
				//print_oct_board("complete_sum_stage1", complete_sum_stage_1_);
				//print_oct_board("complete_sum_stage2", complete_sum_stage_2_);
				print_oct_board("complete_sum_", complete_sum_);
				print_oct_board("is_alive_", is_alive_);
				//print_oct_board("is_three_", is_three_);
				//print_oct_board("is_four_", is_four_);
				//print_oct_board("is_next_alive_", is_next_alive_);
			}
			
			for(i32 y = 0; y < 64; y++)
			{
				for(isize x = 0; x < 64; x ++)
				{
					if(get_at(new_data, x, y))
						printf("#");
					else
						printf(".");
				}

				printf("\n");
			}
			printf("END\n\n");
			
			int x = 10;

			for(i32 y = 0; y < 64; y++)
				data[y] = new_data[y];

		}

		int x = 10;
	}

	// main loop
	f64 zoom = 3.0; //=real/sym
	Vec2f64 sym_center = {0.0, 0.0};
	Vec2i screen_center = {WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2};
	
	f64 dt = 1.0 / TARGET_MS * 1000;
	f64 last_update_clock = clock_s();
	f64 last_frame_clock = clock_s();

	for(i32 x = -250; x < 250; x++)
		for(i32 y = -250; y < 250; y++)
			set_pixel_at(curr_chunk_hash, Vec2i{x, y});

	Vec2i old_mouse_pos = {0,0};
	int x = 0;
	int y = 0;
	SDL_GetMouseState(&x, &y);
	old_mouse_pos.x = x;
	old_mouse_pos.y = y;

	bool paused = false;
	
	#if 0
	set_pixel_at(curr_chunk_hash, Vec2i{1, 0});
	set_pixel_at(curr_chunk_hash, Vec2i{2, 0});
	set_pixel_at(curr_chunk_hash, Vec2i{3, 0});

	
	set_pixel_at(curr_chunk_hash, Vec2i{0, 11});
	set_pixel_at(curr_chunk_hash, Vec2i{0, 12});
	set_pixel_at(curr_chunk_hash, Vec2i{0, 13});

	//first
	set_pixel_at(curr_chunk_hash, Vec2i{0, 30});
	set_pixel_at(curr_chunk_hash, Vec2i{0, 31});
	set_pixel_at(curr_chunk_hash, Vec2i{0, 32});
	
	//Left
	set_pixel_at(curr_chunk_hash, Vec2i{-1, 1});
	set_pixel_at(curr_chunk_hash, Vec2i{-1, 2});
	set_pixel_at(curr_chunk_hash, Vec2i{-1, 3});
	set_pixel_at(curr_chunk_hash, Vec2i{-2, 2});
	set_pixel_at(curr_chunk_hash, Vec2i{-61, 1});
	set_pixel_at(curr_chunk_hash, Vec2i{-61, 2});
	set_pixel_at(curr_chunk_hash, Vec2i{-61, 3});
	set_pixel_at(curr_chunk_hash, Vec2i{-62, 2});

	//printf("\n\nleft\n"); print_chunk(left);

	//corner top left
	set_pixel_at(curr_chunk_hash, Vec2i{-1, -1});
	set_pixel_at(curr_chunk_hash, Vec2i{62, 62});

	//Right
	set_pixel_at(curr_chunk_hash, Vec2i{62, 1});
	set_pixel_at(curr_chunk_hash, Vec2i{62, 2});
	set_pixel_at(curr_chunk_hash, Vec2i{62, 3});
	set_pixel_at(curr_chunk_hash, Vec2i{63, 2});

	//Top 
	set_pixel_at(curr_chunk_hash, Vec2i{1, -1});
	set_pixel_at(curr_chunk_hash, Vec2i{2, -2});
	set_pixel_at(curr_chunk_hash, Vec2i{3, -1});

	//Bottom 
	set_pixel_at(curr_chunk_hash, Vec2i{2, 62});
	set_pixel_at(curr_chunk_hash, Vec2i{3, 63});
	set_pixel_at(curr_chunk_hash, Vec2i{4, 62});
	#endif

	while(true) 
	{
		// event handling
		SDL_Event e;
		if(SDL_PollEvent(&e)) 
		{
			if(e.type == SDL_QUIT)
				break;
				
			int mouse_state = SDL_GetMouseState(&x, &y);
			Vec2i new_mouse_pos = {x, y};
			Vec2i mouse_delta = vec_sub(new_mouse_pos, old_mouse_pos);

			if(e.type == SDL_KEYUP)
			{
				if(e.key.keysym.sym == SDLK_SPACE)
					paused = !paused;
			}

			if(e.type == SDL_MOUSEWHEEL)
			{
				f64 div_by = TARGET_MS > 0 ? TARGET_MS : 1e8;
				f64 normal_dt = 1000.0 / div_by;
				f64 factor = 1.05;
				if(dt > TARGET_MS)
					factor *= normal_dt * dt;
				if(e.wheel.y + e.wheel.x > 0)
					zoom *= factor;
				else
					zoom /= factor;
			}

			if(mouse_state == SDL_BUTTON_MIDDLE || mouse_state == SDL_BUTTON_RIGHT)
			{
				Vec2f64 sym_mouse_delta = {
					(f64) mouse_delta.x / zoom,
					(f64) mouse_delta.y / zoom,
				};

				sym_center.x -= sym_mouse_delta.x;
				sym_center.y -= sym_mouse_delta.y;
			}

			if(mouse_state == SDL_BUTTON_LEFT)
			{
				Vec2f64 new_mouse_sym_f = to_sym_pos(new_mouse_pos, sym_center, screen_center, zoom);
				Vec2f64 old_mouse_sym_f = to_sym_pos(old_mouse_pos, sym_center, screen_center, zoom);

				Vec2f64 mouse_sym_delta_f = {
					new_mouse_sym_f.x - old_mouse_sym_f.x,
					new_mouse_sym_f.y - old_mouse_sym_f.y,
				};

				if(mouse_sym_delta_f.x == 0 && mouse_sym_delta_f.y == 0)
				{
					set_pixel_at_f(curr_chunk_hash, Vec2f64{new_mouse_sym_f.x, new_mouse_sym_f.y});
				}
				else if(fabs(mouse_sym_delta_f.x) >= fabs(mouse_sym_delta_f.y))
				{
					f64 r = mouse_sym_delta_f.y / mouse_sym_delta_f.x;
					for(f64 x = new_mouse_sym_f.x; x <= old_mouse_sym_f.x; x++)
					{
						f64 y = r*(x - new_mouse_sym_f.x) + new_mouse_sym_f.y;
						set_pixel_at_f(curr_chunk_hash, Vec2f64{x, y});
					}
					
					for(f64 x = old_mouse_sym_f.x; x <= new_mouse_sym_f.x; x++)
					{
						f64 y = r*(x - old_mouse_sym_f.x) + old_mouse_sym_f.y;
						set_pixel_at_f(curr_chunk_hash, Vec2f64{x, y});
					}
				}
				else
				{
					f64 r = mouse_sym_delta_f.x / mouse_sym_delta_f.y;
					for(f64 y = old_mouse_sym_f.y; y <= new_mouse_sym_f.y; y++)
					{
						f64 x = r*(y - old_mouse_sym_f.y) + old_mouse_sym_f.x;
						set_pixel_at_f(curr_chunk_hash, Vec2f64{x, y});
					}
					
					for(f64 y = new_mouse_sym_f.y; y <= old_mouse_sym_f.y; y++)
					{
						f64 x = r*(y - new_mouse_sym_f.y) + new_mouse_sym_f.x;
						set_pixel_at_f(curr_chunk_hash, Vec2f64{x, y});
					}
				}
			}

			old_mouse_pos = new_mouse_pos;
		} 
		
		f64 sym_pixels_w = WINDOW_WIDTH / zoom;
		f64 sym_pixels_h = WINDOW_HEIGHT / zoom;

		Vec2i sym_top = {0};
		sym_top.x = (i32) floor(sym_center.x - sym_pixels_w/2);
		sym_top.y = (i32) floor(sym_center.y - sym_pixels_h/2);

		Vec2i sym_bot = {0};
		sym_bot.x = (i32) ceil(sym_center.x + sym_pixels_w/2);
		sym_bot.y = (i32) ceil(sym_center.y + sym_pixels_h/2);
		
		Vec2i top_chunk = get_chunk_pos(sym_top);
		Vec2i bot_chunk = get_chunk_pos(sym_bot);
		bot_chunk.x += 1;
		bot_chunk.y += 1;

		if(UPDATE_SCREEN)
			update_screen(top_chunk, bot_chunk, sym_center, screen_center, zoom, curr_chunk_hash, chunk_texture, renderer);

		if((clock_s() - last_update_clock)*1000 >= TARGET_MS && paused == false && UPDATE_SYMULATION)
		{
			PERF_COUNTER("sym update");
			f64 clock_update = clock_s();
			for(i32 i = 0; i < curr_chunk_hash->chunk_size; i++)
			{
				Chunk* chunk = &curr_chunk_hash->chunks[i];
				if(vec_equal(chunk->pos, Vec2i{-1, 0}))
				{
					int z = 10;
				}

				Chunk* top   = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 0, -1}), empty_chunks);
				Chunk* bot   = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 0,  1}), empty_chunks);
				Chunk* left  = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{-1,  0}), empty_chunks);
				Chunk* right = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 1,  0}), empty_chunks);
				
				Chunk* top_l = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{-1, -1}), empty_chunks);
				Chunk* top_r = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 1, -1}), empty_chunks);
				Chunk* bot_l = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{-1,  1}), empty_chunks);
				Chunk* bot_r = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 1,  1}), empty_chunks);
				
				//update_screen(top_chunk, bot_chunk, sym_center, screen_center, zoom, curr_chunk_hash, chunk_texture, renderer);

				//corner bottom right

				#define OUTER (CHUNK_SIZE + 1)

				u64 last_bit = ((u64) 1 << OUTER);
				u64 first_bit = ((u64) 1);
				u64 middle_bits = (((u64) -1) & ~last_bit) & ~first_bit;
				for(i32 i = OUTER; i < 64; i++)
				{
					middle_bits &= ~((u64) 1 << i);
				}

				//u64 middle_bits = ((u64) -1 << 1) & ((u64) -1 >> 2);

				//@TODO: use get set for this somehow & figure out what to do with middles
				Chunk assembled = {};

				assembled.data[0]  = top->data[OUTER - 1] & middle_bits;
				assembled.data[OUTER] = bot->data[1] & middle_bits;
				
				u64 top_l_bit = (top_l->data[OUTER - 1] << 1) & last_bit;
				u64 top_r_bit = (top_r->data[OUTER - 1] >> 1) & first_bit;
				u64 bot_l_bit = (bot_l->data[1] << 1) & last_bit;
				u64 bot_r_bit = (bot_r->data[1] >> 1) & first_bit;

				assembled.data[0]  |= top_l_bit >> OUTER;
				assembled.data[0]  |= top_r_bit << OUTER;
				
				assembled.data[OUTER] |= bot_l_bit >> OUTER;
				assembled.data[OUTER] |= bot_r_bit << OUTER;

				for(i32 i = 0; i < CHUNK_SIZE; i++)
				{
					u64 middle = chunk->data[i + 1] & middle_bits;
					u64 first = (left->data[i + 1] << 1) & last_bit;
					u64 last = (right->data[i + 1] >> 1) & first_bit;

					assembled.data[1 + i] = (first >> OUTER) | middle | (last << OUTER);
				}
				
				//draw_chunk(&assembled, chunk->pos, sym_center, screen_center, zoom, chunk_texture, renderer);
				
				//printf("ASSEMBLED!!!\n\n");
				//print_chunk(&assembled);
				//printf("END\n\n");
				
				Chunk new_chunk = {0};
				Chunk new_chunk2 = {0};
				new_chunk.pos = chunk->pos;
				new_chunk2.pos = chunk->pos;
				{
					u64 pattern = 011'11111'11111'11111'11111;//pattern of 0b001001... repeating (in oct)
					u64 sliding_mask = 0b111;
					{
						PERF_COUNTER("life");
						for(i32 slot = 0; slot < 3; slot++)
						{
							u64 accumulators[64] = {0};

							for(i32 i = 0; i < 64; i++)
							{
								accumulators[i] += pattern & (assembled.data[i] >> (slot + 0));
								accumulators[i] += pattern & (assembled.data[i] >> (slot + 1));
								accumulators[i] += pattern & (assembled.data[i] >> (slot + 2));
							}

							//iterate all inner cells of the chunk
							for(i32 y = 1; y < OUTER; y++)
							{
								for(i32 slot_x = 0; slot_x < OUTER - 1; slot_x += 3)
								{
									u8 sum = 0;
									sum += (accumulators[y - 1] >> slot_x) & sliding_mask;
									sum += (accumulators[y    ] >> slot_x) & sliding_mask;
									sum += (accumulators[y + 1] >> slot_x) & sliding_mask;

									i32 x = slot_x + slot + 1;
									assert(x < 64);
						
									u64 bit = (u64) 1 << x;

									bool is_alive = !!(assembled.data[y] & bit);
									bool set_to = (sum == 3) || (is_alive && sum == 4);

									//hopefully conditional move
									u64 new_val = new_chunk.data[y] | bit;
									if(set_to)
										new_chunk.data[y] = new_val;
								}
							}
						}
					}
				}
				
				if(true)
				{
					u64 pattern = 011'11111'11111'11111'11111;//pattern of 0b001001... repeating (in oct)
					u64 oct0 = pattern << 0; //pattern of 0b001001...
					u64 oct1 = pattern << 1; //pattern of 0b010010...
					u64 oct2 = pattern << 2; //pattern of 0b100100...
					
					//for all three offsets in the 3 bit slots
					for(i32 slot = 0; slot < 3; slot++)
					{
						u64 accumulators[64] = {0};

						for(i32 i = 0; i < 64; i++)
						{
							u64 curr_accumulator = 0;
							u64 slid = (assembled.data[i] << 1); 
							//we have to align it so that its in the ceneter of the oct
							//otherwise for     0b 0 0 1 0 0 0 1 0
							//we woudl geenrate    1 1 1 0 1 1 1 0
							//but we want:         0 1 1 1 0 1 1 1

							curr_accumulator += (oct0 & (slid >> (0 + slot)));
							curr_accumulator += (oct0 & (slid >> (1 + slot)));
							curr_accumulator += (oct0 & (slid >> (2 + slot)));
							accumulators[i] = curr_accumulator;
						}

						//iterate all inner cells of the chunk
						for(i32 y = 1; y < OUTER; y++)
						{
							u64 first_sum = accumulators[y - 1] + accumulators[y];
							u64 has_4 = first_sum & oct2;
					
							u64 has_any = ((accumulators[y + 1] & oct1) << 1 | (accumulators[y + 1] & oct0) << 2);

							u64 is_overfull = has_4 & has_any;
							u64 complete_sum_stage_1 = ~is_overfull & first_sum;
							u64 complete_sum_stage_2 = accumulators[y + 1];
							u64 complete_sum = (~is_overfull & first_sum) + accumulators[y + 1];

							u64 three_pattern = oct0 | oct1;
							u64 four_pattern = oct2;

							u64 three_check = three_pattern ^ complete_sum; //completely 0 if is three
							u64 four_check = four_pattern ^ complete_sum; //completely 0 if is four

							u64 is_not_three = (three_check & oct0) << 2 | (three_check & oct1) << 1 | (three_check & oct2) << 0;
							u64 is_not_four  = (four_check & oct0) << 2 | (four_check & oct1) << 1 | (four_check & oct2) << 0;    
							u64 is_alive     = (oct0 & (assembled.data[y] >> slot)) << 2; 
					
							u64 is_three = ~is_not_three; 
							u64 is_four = ~is_not_four; 
							u64 is_next_alive = (is_three | (is_alive & is_four)) & ~is_overfull;

							is_next_alive &= oct2;

							new_chunk2.data[y] |= (is_next_alive >> (2 - slot));
						}
					}
				}
				
				//printf("NEW!!!\n\n");
				//print_chunk(&new_chunk);
				//printf("END\n\n");
				//assert(new_chunk.data[0] == 0);
				//assert(new_chunk.data[63] == 0);
				if(true)
				for(i32 i = 0; i < CHUNK_SIZE; i++)
				{
					u64 middle1 = new_chunk.data[i + 1] & middle_bits;
					u64 middle2 = new_chunk2.data[i + 1] & middle_bits;
					if(middle1 != middle2)
					{
						print_bits(middle_bits);
						print_bits(middle1);
						print_bits(middle2);
					}
					assert(middle1 == middle2);
				}

				u64 acummulated = 0;
				for(i32 i = 0; i < CHUNK_SIZE; i++)
					acummulated |= new_chunk2.data[1 + i] & middle_bits;

				//Unless chunk is comletely dead insert itself alongside all neigboring chunks chunk_hash the next generation
				if(acummulated != 0)
				{
					PERF_COUNTER("neighbour add");
					i32 curr_i = insert_chunk(next_chunk_hash, chunk->pos);
					*get_chunk(next_chunk_hash, curr_i) = new_chunk2;
					for(i32 k = 0; k < 8; k++)
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, DIRECTIONS[k]));
				}
			}

			//Swap chunk hashes
			Chunk_Hash* temp = curr_chunk_hash;
			curr_chunk_hash = next_chunk_hash;
			next_chunk_hash = temp;
			clear_chunk_hash(next_chunk_hash);

			last_update_clock = clock_s();	
			printf("update time: %lf\n", clock_s() - clock_update);
		}

		f64 new_frame_clock = clock_s();
		dt = new_frame_clock - last_frame_clock;

		//printf("fps: %lf\n", 1.0 / dt);
		last_frame_clock = new_frame_clock;
	}
	
	SDL_DestroyTexture(chunk_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	destroy_chunk_hash(&chunk_hash1);
	destroy_chunk_hash(&chunk_hash2);
	
	for(isize i = 0; i < MAX_PERF_COUNTERS; i++)
	{
		if(perf_counters[i].function != nullptr)
		{
			Perf_Counter* counter = &perf_counters[i];
			f64 total_s = (f64) counter->counter / (f64) perf_counter_freq() ;
			f64 per_run_s = total_s / (f64) counter->runs;
			using ll = long long;

			if(counter->name)
				printf("%s %lld \"%s\": total: %8.8lf run: %8.8lf runs: %lld\n", counter->function, (ll) counter->line, counter->name, total_s, per_run_s, (ll) counter->runs);
			else
				printf("%s %lld: total: %8.8lf run: %8.8lf runs: %lld\n", counter->function, (ll) counter->line, total_s, per_run_s, (ll) counter->runs);
		}
	}

	SDL_Quit(); 
	return 0;
}

Vec2i vec_add(Vec2i a, Vec2i b)
{
	return Vec2i{a.x + b.x, a.y + b.y};
}

Vec2i vec_sub(Vec2i a, Vec2i b)
{
	return Vec2i{a.x - b.x, a.y - b.y};
}


Vec2i vec_mul(i32 scalar, Vec2i vec)
{
	return Vec2i{scalar*vec.x, scalar*vec.y};
}

bool vec_equal(Vec2i a, Vec2i b)
{
	return a.x == b.x && a.y == b.y;
}

bool get_at(const Chunk* chunk, Vec2i pos)
{
	assert(-1 <= pos.x && pos.x < CHUNK_SIZE + 1);
	assert(-1 <= pos.y && pos.y < CHUNK_SIZE + 1);
	
	u64 bit = (u64) 1 << (pos.x + 1);
	
	return (chunk->data[pos.y + 1] & bit) > 0;
}

void set_at(Chunk* chunk, Vec2i pos)
{
	assert(-1 <= pos.x && pos.x < CHUNK_SIZE + 1);
	assert(-1 <= pos.y && pos.y < CHUNK_SIZE + 1);

	u64 bit = (u64) 1 << (pos.x + 1);
	
	chunk->data[pos.y + 1] |= bit;
}

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
		i32 new_capacity = 16;
		i32 max = chunk_hash->chunk_size * 4;
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
		for(i32 i = 0; i < chunk_hash->hash_capacity; i++)
		{
			//skip empty or dead
			Hash_Slot* curr = &chunk_hash->hash[i];
			if(curr->chunk < CHUNK_STATE_OFFSET)
				continue;
            
			//hash the non empty entry
			u64 at = curr->chunk - CHUNK_STATE_OFFSET;
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
		i32 new_capacity = chunk_hash->chunk_capacity*2;
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
			return (i32) chunk_hash->hash[i].chunk - CHUNK_STATE_OFFSET;
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
			return chunk_hash->hash[i].chunk - CHUNK_STATE_OFFSET;
	}

	return -1;
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
	int width = -1;
	int height = -1;
	i32 size = (i32) strlen(read_data);
	i32 line_count = 0;
	i32 line_size = 0;

	for(i32 i = 0; i < size; i += line_size + 1, line_count ++)
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
		i32 y = line_count;
        for(i32 x = 0; x < line_size; x++)
        {
			char c = line[x];
            switch(c)
            {
                case 'X': {
				
					i32 offset_x = x - (i32) width/2 + CHUNK_SIZE/2; 
					i32 offset_y = y - (i32) height/2  + CHUNK_SIZE/2;
				
					i32 chunk_x = offset_x / CHUNK_SIZE;
					i32 chunk_y = offset_y / CHUNK_SIZE;
					
					i32 local_x = (offset_x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
					i32 local_y = (offset_y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
					
					i32 chunk_i = insert_chunk(chunk_hash, Vec2i{chunk_x, chunk_y});
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

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)   
#include <windows.h>
#include <profileapi.h>


inline int64_t _query_perf_counter_freq()
{
    LARGE_INTEGER ticks = {};
    (void) QueryPerformanceFrequency(&ticks);
    return ticks.QuadPart;
}

inline int64_t perf_counter()
{
    LARGE_INTEGER ticks = {};
    (void) QueryPerformanceCounter(&ticks);
    return ticks.QuadPart;
}

inline int64_t* _perf_counter_base()
{
    static int64_t base = perf_counter();
    return &base;
}

inline int64_t perf_counter_freq()
{
    static int64_t freq = _query_perf_counter_freq(); //doesnt change so we can cache it
    return freq;
}
    
inline double _double_perf_counter_freq()
{
    static double freq = (double) _query_perf_counter_freq();
    return freq;
}

inline int64_t clock_ns()
{
    int64_t freq = perf_counter_freq();
    int64_t counter = perf_counter() - *_perf_counter_base();

    int64_t sec_to_nanosec = 1'000'000'000;
    //We assume _perf_counter_base is set to some reasonable thing so this will not overflow
    // (with this we are only able to represent 1e12 secons (A LOT) without overflowing)
    return counter * sec_to_nanosec / freq;
}

//We might be rightfully scared that after some ammount of time the clock_s will get sufficiently large and
// we will start loosing pression. This is however not a problem. If we assume the _perf_counter_freq is equal to 
// 10Mhz = 1e7 (which is very common) then we would like the clock_s to have enough precision to represent
// 1 / 10Mhz = 1e-7. This precission is held up untill 1e9 secons have passed which is roughly 31 years. 
// => Please dont run your program for more than 31 years or you will loose precision
inline double clock_s()
{
    double freq = _double_perf_counter_freq();
    double counter = (double) (perf_counter() - *_perf_counter_base());
    return counter / freq;
}
#else
#include <time.h>

inline int64_t perf_counter()
{
    struct timespec ts;
    (void) clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    return ts.tv_nsec;
}

inline int64_t perf_counter_freq()
{
	return 1'000'000'000;
}

inline int64_t clock_ns()
{
    static int64_t base = perf_counter();
    return _clock_ns() - base;
}
    
inline double clock_s()
{
    return (double) clock_ns() / 1.0e9;
}
#endif