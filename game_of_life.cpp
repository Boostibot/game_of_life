#define _CRT_SECURE_NO_WARNINGS

// A proof of concept of extremely performant Game of Life (GOL) implementation.
//
// We represent cells as single bits and use clever bit shifts to effectlively achieve 
// 21 wide parallelism within a single u64 number. This is more or less improvised version of SIMD.
// (in the old days of programming this was more common thing to do).
// We try to operate on as many bits as possible simultanously. How much is that? 
// We will answer indirectly. How many bits do we need to represent the entire GOL algorhitm? 
// (yes specifically GOL and NOT *any* automata of certain kind). 
// 
// It seems we need 4. We need to sum all adjecent pixels and then do simple comparison of 
// the final value to determin if the cell lives. We need 4 bits because the maximum sum can be 8 (0b1000). 
// However if we discard all values higher than 7 as those will result in dead cell anyway we need just 3!
// (We apply some clever masking to achieve this. Even if a bit tricky its fairly straigtforward)
//
// Okay so we need 3 bits to do the calculation. Now how does the actual algorhitm work. Lets explain this with a pictures.
// We are trying to determine if the middle cell lives or dies. 
// 
// 0   1   0
//   -----
// 1 | 1 | 1
//   -----
// 0   0   1
//
// To do this we first sum along each rows separately: 
// 
// 0 + 1 + 0 = 1
//   -----
// 1 + 1 + 1 = 3
//   -----
// 0 + 0 + 1 = 1
// 
// This can be achived fairly easily in binary by the following piece of code
// where oct is a pattern of bits spaced 2 appart.
// 
//  u64 row = /* bits in a single row of our bitmap chunk*/;
//	u64 oct = 01111111111111111111111; //..1001001 in binary
//	u64 sum_every_3_bits = (oct & (row >> 0));
//		+ (oct & (row >> 1));
//		+ (oct & (row >> 2));
//
// Now all thats left to do is to sum vertically. Here however we need to be careful since we cannot sum all 3 rows due to overflow 
// out of our 3 bits. So instead we only sum the first two and then dermine if the next addition would overflow. 
// We then use this to detrmine if the cell is "overfull" that is if the sum is a number which kills the cell no matter the circumstance.
//
// We then test if the bit patterns exactly match certain values (this can be done in effect with the following):
// 
//  u64 pattern = /* bit pattern of 5 repeating */
//  u64 row = /* bits in a single row of our bitmap chunk*/;
//  u64 xored = pattern ^ row; //if are equl their XOR is everywhere 0
//  u64 are_every_3_bits_not_equal_to_5 = (oct & (xored >> 0));
//		| (oct & (xored >> 1));
//		| (oct & (xored >> 2));
//
// /* now are_every_3_bits_not_equal_to_5 contains a set bit at the lowest adress in each oct (bit triplet) */
//
// Then we can test for equality simply like so:
// 
//  u64 are_every_3_bits_equal_to_5 = ~are_every_3_bits_not_equal_to_5;
//
// Then we simply compose the necessary tests together using bitwise OR, AND and NEG and we recieved every 3rd bit of the output!
// We repeat this exact same procedure except with everything shifted down a bit to recieve every 3rd bit in the second set of the output.
// then finally the thir as well. We or these together and this yields the final value.
// 
// This might look daunting (and indeed its painful to debug) but with a bit of patience its not that diffuclt to trial and error into.
//
// This approach is ~200x fatster than naive implementation using serial ifs, and about ~20x faster than branchless version. It can further be sped up about 4x
// by using 256 bit SIMD registers and doing essentially the same thing wider. I was however lazy to implement this as this was fast enough for my need as the
// current bottle neck is rendering and then hash map lookup.

// Controls:
// mouse wheel			- zoom in and out
// mouse left			- draw
// mouse right/middle/M	- pan camera
// mouse left + D		- erase
// SPACE				- stop/resume symulation
// P					- increase symulation speed
// O					- decrease symulation speed

#include "chunk.h"
#include "chunk_hash.h"
#include "time.h"
#include "perf.h"
#include "alloc.h"

#include <SDL/SDL.h>

#define WINDOW_TITLE        "Game of Life"
#define TARGET_FRAME_TIME	16.0
#define DEF_WINDOW_WIDTH	1200
#define DEF_WINDOW_HEIGHT	700
#define DEF_SYM_TIME		30.0

#define CLEAR_COLOR_1		 0x111111FF
#define CLEAR_COLOR_2		 0x070707FF
#define CLEAR_COLOR_ACTIVE_1 0x221111FF
#define CLEAR_COLOR_ACTIVE_2 0x140707FF

#define DO_LIN_DOWNSAMPLING		false
#define DO_UPDATE_SCREEN		true
#define DO_UPDATE_SYMULATION	true
#define DO_UPDATE_INPUT			true

#define INPUT_FACTOR_ZOOM						1.02
#define INPUT_FACTOR_INCREASE_SPEED				5000
#define INPUT_FACTOR_INCREASE_SPEED_FRACTION	1.015

// Enables cleanup code to run when the application exits. 
// This is absolutely not necessary since we can just leak all resources anyway 
// and let the OS handle our mess, but just in case
// we want to be good programmers we can enable this. ;)
// 
// Reallistically only incurs delay upon closing the application.
// 
//#define DO_CLEANUP

//A single generation step of the symulation
void game_of_life_generation_step(Chunk_Hash* curr_chunk_hash, Chunk_Hash* next_chunk_hash)
{
	Chunk empty_chunks[9] = {0};
	PERF_COUNTER("step");

	for(i32 i = 0; i < curr_chunk_hash->chunk_size; i++)
	{
		PERF_COUNTER("single chunk");
		Chunk* chunk = &curr_chunk_hash->chunks[i];
				
		Chunk* top   = NULL;
		Chunk* bot   = NULL;
		Chunk* left  = NULL;
		Chunk* right = NULL;
				
		Chunk* top_l = NULL;
		Chunk* top_r = NULL;
		Chunk* bot_l = NULL;
		Chunk* bot_r = NULL;

		{
			PERF_COUNTER("neighbour gather");
			//@TODO: only add only needed chunks just like in the add after
			//save the computation from last time

			top   = chunk_hash_get_or(curr_chunk_hash, vec_add(chunk->pos, vec( 0, -1)), empty_chunks);
			bot   = chunk_hash_get_or(curr_chunk_hash, vec_add(chunk->pos, vec( 0,  1)), empty_chunks);
			left  = chunk_hash_get_or(curr_chunk_hash, vec_add(chunk->pos, vec(-1,  0)), empty_chunks);
			right = chunk_hash_get_or(curr_chunk_hash, vec_add(chunk->pos, vec( 1,  0)), empty_chunks);
				
			top_l = chunk_hash_get_or(curr_chunk_hash, vec_add(chunk->pos, vec(-1, -1)), empty_chunks);
			top_r = chunk_hash_get_or(curr_chunk_hash, vec_add(chunk->pos, vec( 1, -1)), empty_chunks);
			bot_l = chunk_hash_get_or(curr_chunk_hash, vec_add(chunk->pos, vec(-1,  1)), empty_chunks);
			bot_r = chunk_hash_get_or(curr_chunk_hash, vec_add(chunk->pos, vec( 1,  1)), empty_chunks);
		}

		//Main life algorhirm
		{
			#define OUTER (CHUNK_SIZE + 1)

			//Generate Masks: R|CONTENT_BITS|L|0
			u64 R_OUTER_BIT = ((u64) 1 << OUTER); 
			u64 L_OUTER_BIT = ((u64) 1);
			u64 CONTENT_BITS = (((u64) -1) & ~R_OUTER_BIT) & ~L_OUTER_BIT;

			for(i32 i = OUTER; i < 64; i++)
				CONTENT_BITS &= ~((u64) 1 << i);

			//Holds a composed value for the currently processed block.
			//Includes the edge from adjecent chunks
			Chunk assembled = {0};

			//Fill edge pixels from adjecent chunks
			assembled.data[0]  = top->data[OUTER - 1] & CONTENT_BITS;
			assembled.data[OUTER] = bot->data[1] & CONTENT_BITS;
				
			u64 top_l_bit = (top_l->data[OUTER - 1] << 1) & R_OUTER_BIT;
			u64 top_r_bit = (top_r->data[OUTER - 1] >> 1) & L_OUTER_BIT;
			u64 bot_l_bit = (bot_l->data[1] << 1) & R_OUTER_BIT;
			u64 bot_r_bit = (bot_r->data[1] >> 1) & L_OUTER_BIT;

			assembled.data[0]  |= top_l_bit >> OUTER;
			assembled.data[0]  |= top_r_bit << OUTER;
				
			assembled.data[OUTER] |= bot_l_bit >> OUTER;
			assembled.data[OUTER] |= bot_r_bit << OUTER;

			//Adds the middle set of data from the main processed chunk
			for(i32 i = 0; i < CHUNK_SIZE; i++)
			{
				u64 middle = chunk->data[i + 1] & CONTENT_BITS;
				u64 first = (left->data[i + 1] << 1) & R_OUTER_BIT;
				u64 last = (right->data[i + 1] >> 1) & L_OUTER_BIT;

				assembled.data[1 + i] = (first >> OUTER) | middle | (last << OUTER);
			}
			Chunk new_chunk = {0};
			new_chunk.pos = chunk->pos;
				
			u64 pattern = 01111111111111111111111;//pattern of 0b...001001 repeating (in oct)
			u64 oct0 = pattern << 0; //pattern of 0b...001001
			u64 oct1 = pattern << 1; //pattern of 0b...010010
			u64 oct2 = pattern << 2; //pattern of 0b...100100 
					
			{
				PERF_COUNTER("life");
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

						u64 is_overfull = has_4 & has_any; //the sum around has value higher or equal to 4 (if is true dies)
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
						u64 is_next_generation_alive = (is_three | (is_alive & is_four)) & ~is_overfull;

						is_next_generation_alive &= oct2;

						new_chunk.data[y] |= (is_next_generation_alive >> (2 - slot));
					}
				}
			}

			const auto compare_chunks = [&](Chunk* a, Chunk* b){
				u64 result = 0;
				for(i32 i = 0; i < CHUNK_SIZE; i++)
					result |= (a->data[1 + i] & CONTENT_BITS) ^ (b->data[1 + i] & CONTENT_BITS);
					
				return result == 0;
			};

			u64 acummulated = 0;
			for(i32 i = 0; i < CHUNK_SIZE; i++)
				acummulated |= new_chunk.data[1 + i] & CONTENT_BITS;

			//Unless chunk is comletely dead insert itself alongside all neigboring chunks chunk_hash the next generation
			if(acummulated != 0)
			{
				PERF_COUNTER("neighbour add");
				i32 curr_i = chunk_hash_insert(next_chunk_hash, chunk->pos);
				*chunk_hash_at(next_chunk_hash, curr_i) = new_chunk;
					
				//left right 
				if(acummulated & ((u64) 1 << 1))
					chunk_hash_insert(next_chunk_hash, vec_add(chunk->pos, vec(-1,  0)));
				if(acummulated & ((u64) 1 << CHUNK_SIZE))
					chunk_hash_insert(next_chunk_hash, vec_add(chunk->pos, vec(1,  0)));

				//top bot
				if(new_chunk.data[1] & CONTENT_BITS)
					chunk_hash_insert(next_chunk_hash, vec_add(chunk->pos, vec(0,  -1)));
				if(new_chunk.data[CHUNK_SIZE] & CONTENT_BITS)
					chunk_hash_insert(next_chunk_hash, vec_add(chunk->pos, vec(0,  1)));
						
				//diagonals - there is only very small chence these will get added
				if(chunk_get_cell(&new_chunk, vec(0, 0)))
					chunk_hash_insert(next_chunk_hash, vec_add(chunk->pos, vec(-1, -1)));
				if(chunk_get_cell(&new_chunk, vec(0, CHUNK_SIZE - 1)))
					chunk_hash_insert(next_chunk_hash, vec_add(chunk->pos, vec(-1,  1)));
				if(chunk_get_cell(&new_chunk, vec(CHUNK_SIZE - 1, 0)))
					chunk_hash_insert(next_chunk_hash, vec_add(chunk->pos, vec(1,  -1)));
				if(chunk_get_cell(&new_chunk, vec(CHUNK_SIZE - 1, CHUNK_SIZE - 1)))
					chunk_hash_insert(next_chunk_hash, vec_add(chunk->pos, vec(1,  1)));
			}
		}
	}
}

Vec2i get_chunk_pos(Vec2i sym_position);
Vec2i get_cell_pos(Vec2i sym_position);
void set_cell_at(Chunk_Hash* chunk_hash, Vec2i sym_pos, bool to);
void set_cell_at_f(Chunk_Hash* chunk_hash, Vec2f64 sym_posf, bool to);
Vec2i to_screen_pos(Vec2f64 sym_position, Vec2f64 sym_center, Vec2i screen_center, f64 zoom);
Vec2f64 to_sym_pos(Vec2i screen_position, Vec2f64 sym_center, Vec2i screen_center, f64 zoom);
Vec2i get_mouse_pos(u32* state);

void init_textures(SDL_Texture** normal_chunk_texture, SDL_Texture** clear_tex1, SDL_Texture** clear_tex2, SDL_Renderer* renderer);
void update_screen(Vec2i top_chunk, Vec2i bot_chunk, Vec2f64 sym_center, Vec2i screen_center, f64 zoom, Chunk_Hash* chunk_hash, SDL_Texture* chunk_texture, SDL_Texture* clear_tex1, SDL_Texture* clear_tex2, SDL_Renderer* renderer);

int main(int argc, char *argv[]) {

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
			return 1;

	SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, 100, 100, 
			DEF_WINDOW_WIDTH, 
			DEF_WINDOW_HEIGHT, 
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	if(DO_LIN_DOWNSAMPLING)
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	
	SDL_Texture* chunk_texture = NULL;
	SDL_Texture* clear_chunk_texture1 = NULL;
	SDL_Texture* clear_chunk_texture2 = NULL;

	init_textures(&chunk_texture, &clear_chunk_texture1, &clear_chunk_texture2, renderer);

	//We keep two hashes and swap between them on every uodate
	#define CHUNK_HASHES_COUNT 2
	Chunk_Hash chunk_hashes[CHUNK_HASHES_COUNT] = {};
	for(i32 i = 0; i < CHUNK_HASHES_COUNT; i++)
		chunk_hash_init(&chunk_hashes[i]);
	
	i64 generation = 0;
	Chunk_Hash* curr_chunk_hash  = &chunk_hashes[(generation + 0) % CHUNK_HASHES_COUNT];
	Chunk_Hash* next_chunk_hash  = &chunk_hashes[(generation + 1) % CHUNK_HASHES_COUNT];

	f64 zoom = 3.0;
	f64 symulation_time = DEF_SYM_TIME;
	Vec2f64 sym_center = {0.0, 0.0};
	Vec2i window_size = {DEF_WINDOW_WIDTH, DEF_WINDOW_HEIGHT};
	Vec2i screen_center = {window_size.x / 2, window_size.y / 2};
	
	f64 dt = 1.0 / TARGET_FRAME_TIME * 1000;
	f64 last_sym_update_clock = clock_s();
	f64 last_screen_update_clock = clock_s();
	f64 last_frame_clock = clock_s();

	f64 last_update_duration = 0;
	f64 last_draw_duration = 0;

	Vec2i old_mouse_pos = get_mouse_pos(NULL);
	bool paused = false;

	//Initialize the screen to square
	for(i32 x = -250; x < 250; x++)
		for(i32 y = -250; y < 250; y++)
			set_cell_at(curr_chunk_hash, vec(x, y), true);

	// main loop
	while(true) 
	{
		// event handling
		SDL_Event event;
		if(SDL_PollEvent(&event)) 
		{
			if(event.type == SDL_QUIT)
				break;
				
			const u8* keayboard_state = SDL_GetKeyboardState(NULL);
			if (keayboard_state[SDL_SCANCODE_P]) 
			{
				symulation_time += INPUT_FACTOR_INCREASE_SPEED*dt;
				printf("new sym time: %lf ms\n", symulation_time);
			}
			
			if (keayboard_state[SDL_SCANCODE_O]) 
			{
				f64 new_symulation_time = symulation_time - INPUT_FACTOR_INCREASE_SPEED*dt;
				if(symulation_time / INPUT_FACTOR_INCREASE_SPEED_FRACTION > new_symulation_time)
					symulation_time /= INPUT_FACTOR_INCREASE_SPEED_FRACTION;
				else
					symulation_time = new_symulation_time;
				printf("new sym time: %lf ms\n", symulation_time);
			}

			if(event.type == SDL_WINDOWEVENT)
			{
				if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					int w = 0;
					int h = 0;

					SDL_GetWindowSize(window, &w, &h);
					window_size.x = w;
					window_size.y = h;
					screen_center = {window_size.x / 2, window_size.y / 2};
				}
			}

			if(event.type == SDL_KEYUP)
			{
				if(event.key.keysym.sym == SDLK_SPACE)
					paused = !paused;
			}

			if(event.type == SDL_MOUSEWHEEL)
			{
				f64 div_by = TARGET_FRAME_TIME > 0 ? TARGET_FRAME_TIME : 1e8;
				f64 normal_dt = 1000.0 / div_by;
				f64 factor = INPUT_FACTOR_ZOOM;
				if(dt > TARGET_FRAME_TIME)
					factor *= normal_dt * dt;
				if(event.wheel.y + event.wheel.x > 0)
					zoom *= factor;
				else
					zoom /= factor;
			}
			
			u32 mouse_state = 0;
			Vec2i new_mouse_pos = get_mouse_pos(&mouse_state);
			Vec2i mouse_delta = vec_sub(new_mouse_pos, old_mouse_pos);
			if(mouse_state == SDL_BUTTON_MIDDLE || mouse_state == SDL_BUTTON_RIGHT || keayboard_state[SDL_SCANCODE_M])
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
				PERF_COUNTER("draw");
				Vec2f64 new_mouse_sym_f = to_sym_pos(new_mouse_pos, sym_center, screen_center, zoom);
				Vec2f64 old_mouse_sym_f = to_sym_pos(old_mouse_pos, sym_center, screen_center, zoom);

				Vec2f64 mouse_sym_delta_f = {
					new_mouse_sym_f.x - old_mouse_sym_f.x,
					new_mouse_sym_f.y - old_mouse_sym_f.y,
				};

				bool is_draw = !keayboard_state[SDL_SCANCODE_D];
				if(mouse_sym_delta_f.x == 0 && mouse_sym_delta_f.y == 0)
				{
					set_cell_at_f(curr_chunk_hash, Vec2f64{new_mouse_sym_f.x, new_mouse_sym_f.y}, is_draw);
				}
				else if(fabs(mouse_sym_delta_f.x) >= fabs(mouse_sym_delta_f.y))
				{
					f64 r = mouse_sym_delta_f.y / mouse_sym_delta_f.x;
					for(f64 x = new_mouse_sym_f.x; x <= old_mouse_sym_f.x; x++)
					{
						f64 y = r*(x - new_mouse_sym_f.x) + new_mouse_sym_f.y;
						set_cell_at_f(curr_chunk_hash, Vec2f64{x, y}, is_draw);
					}
					
					for(f64 x = old_mouse_sym_f.x; x <= new_mouse_sym_f.x; x++)
					{
						f64 y = r*(x - old_mouse_sym_f.x) + old_mouse_sym_f.y;
						set_cell_at_f(curr_chunk_hash, Vec2f64{x, y}, is_draw);
					}
				}
				else
				{
					f64 r = mouse_sym_delta_f.x / mouse_sym_delta_f.y;
					for(f64 y = old_mouse_sym_f.y; y <= new_mouse_sym_f.y; y++)
					{
						f64 x = r*(y - old_mouse_sym_f.y) + old_mouse_sym_f.x;
						set_cell_at_f(curr_chunk_hash, Vec2f64{x, y}, is_draw);
					}
					
					for(f64 y = new_mouse_sym_f.y; y <= old_mouse_sym_f.y; y++)
					{
						f64 x = r*(y - new_mouse_sym_f.y) + new_mouse_sym_f.x;
						set_cell_at_f(curr_chunk_hash, Vec2f64{x, y}, is_draw);
					}
				}
			}

			old_mouse_pos = new_mouse_pos;
		} 
		
		f64 sym_pixels_w = window_size.x / zoom;
		f64 sym_pixels_h = window_size.y / zoom;

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

		if((clock_s() - last_screen_update_clock)*1000 >= TARGET_FRAME_TIME && DO_UPDATE_SCREEN)
		{
			f64 screen_update_start = clock_s();
			update_screen(top_chunk, bot_chunk, sym_center, screen_center, zoom, curr_chunk_hash, chunk_texture, clear_chunk_texture1, clear_chunk_texture2, renderer);
			last_screen_update_clock = clock_s();

			last_draw_duration = last_screen_update_clock - screen_update_start;
		}
		
		if((clock_s() - last_sym_update_clock)*1000 >= symulation_time && paused == false && DO_UPDATE_SYMULATION)
		{
			generation++;
			f64 clock_update_start = clock_s();

			chunk_hash_clear(next_chunk_hash);
			game_of_life_generation_step(curr_chunk_hash, next_chunk_hash);
			
			curr_chunk_hash  = &chunk_hashes[(generation + 0) % CHUNK_HASHES_COUNT];
			next_chunk_hash  = &chunk_hashes[(generation + 1) % CHUNK_HASHES_COUNT];

			last_sym_update_clock = clock_s();	
			last_update_duration = last_sym_update_clock - clock_update_start;

			char title_buffer[256] = "";
			snprintf(title_buffer, sizeof title_buffer, "%s update: %8lf draw: %8lf", WINDOW_TITLE, last_update_duration, last_draw_duration);
			SDL_SetWindowTitle(window, title_buffer);
		}

		f64 new_frame_clock = clock_s();
		dt = new_frame_clock - last_frame_clock;
		last_frame_clock = new_frame_clock;
	}
	
	printf("total time: %lf\n", clock_s());
	printf("generations: %d\n", (int) generation);
	printf("generations/s: %lf\n", generation / clock_s());

	const Perf_Counter* perf_counters = perf_get_counters();
	for(isize i = 0; i < MAX_PERF_COUNTERS; i++)
	{
		Perf_Counter counter = perf_counters[i];
		if(counter.runs != 0)
		{
			f64 total_s = perf_counter_get_total_running_time_s(counter);
			f64 per_run_s = perf_counter_get_average_running_time_s(counter);

			printf("%-25s: total: %8.8lf run: %8.8lf runs: %-8lld at %s %lld\n", 
				counter.name ? counter.name : "", total_s, per_run_s, (lld) counter.runs, counter.function, (lld) counter.line);
		}
	}
	
	#ifdef DO_CLEANUP
	SDL_DestroyTexture(clear_chunk_texture1);
	SDL_DestroyTexture(clear_chunk_texture2);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	
	for(i32 i = 0; i < CHUNK_HASHES_COUNT; i++)
		chunk_hash_deinit(&chunk_hashes[i]);

	SDL_Quit(); 
	#endif // DO_CLEANUP
	return 0;
}


Vec2i get_chunk_pos(Vec2i sym_position)
{
	Vec2i output = {0};
	output.x = div_round_down(sym_position.x, CHUNK_SIZE);
	output.y = div_round_down(sym_position.y, CHUNK_SIZE);

	return output;
};
	
Vec2i get_cell_pos(Vec2i sym_position)
{
	Vec2i output = {0};
	output.x = (sym_position.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	output.y = (sym_position.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

	return output;
};

void set_cell_at(Chunk_Hash* chunk_hash, Vec2i sym_pos, bool to)
{
	Vec2i place_at_chunk = get_chunk_pos(sym_pos);
	Vec2i place_at_pixel = get_cell_pos(sym_pos);

	i32 chunk_i = chunk_hash_insert(chunk_hash, place_at_chunk);
	Chunk* chunk = chunk_hash_at(chunk_hash, chunk_i);
	chunk_set_cell(chunk, place_at_pixel, to);

	//@TODO: careful insertion of only the chunks we need!
	if(to)
	{
		const Vec2i directions[8] = {
			{-1, -1},{0, -1},{1, -1},
			{-1, 0}, /* X */ {1,  0},
			{-1,  1},{0,  1},{1,  1},
		};

		for(i32 i = 0; i < 8; i++)
			chunk_hash_insert(chunk_hash, vec_add(directions[i], place_at_chunk));
	}
};

void set_cell_at_f(Chunk_Hash* chunk_hash, Vec2f64 sym_posf, bool to)
{
	Vec2i sym_pos = {
		(i32) round(sym_posf.x),
		(i32) round(sym_posf.y),
	};

	set_cell_at(chunk_hash, sym_pos, to);
};

Vec2i to_screen_pos(Vec2f64 sym_position, Vec2f64 sym_center, Vec2i screen_center, f64 zoom)
{
	Vec2i screen_offset = {
		(i32) floor((sym_position.x - sym_center.x) * zoom),
		(i32) floor((sym_position.y - sym_center.y) * zoom)
	};

	Vec2i screen_position = vec_add(screen_offset, screen_center);
	return screen_position;
};
	
Vec2f64 to_sym_pos(Vec2i screen_position, Vec2f64 sym_center, Vec2i screen_center, f64 zoom)
{
	Vec2i screen_offset = vec_sub(screen_position, screen_center);

	Vec2f64 sym_position = {
		(f64) screen_offset.x / zoom + sym_center.x,
		(f64) screen_offset.y / zoom + sym_center.y,
	};

	return sym_position;
};

Vec2i get_mouse_pos(u32* state)
{
	int x = 0;
	int y = 0;
	u32 local_state = (i32) SDL_GetMouseState(&x, &y);
	if(state != NULL)
		*state = local_state;

	return {x, y};
}

void init_textures(SDL_Texture** normal_chunk_texture, SDL_Texture** clear_tex1, SDL_Texture** clear_tex2, SDL_Renderer* renderer)
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

	
	//Initialize the two textures to solid color
	{
		uint32_t* pixels1 = NULL;
		uint32_t* pixels2 = NULL;
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

	*clear_tex1 = clear_chunk_texture1;
	*clear_tex1 = clear_chunk_texture2;
	*normal_chunk_texture = chunk_texture;
}

void update_screen(Vec2i top_chunk, Vec2i bot_chunk, Vec2f64 sym_center, Vec2i screen_center, f64 zoom, Chunk_Hash* chunk_hash, SDL_Texture* chunk_texture, SDL_Texture* clear_tex1, SDL_Texture* clear_tex2, SDL_Renderer* renderer)
{
	PERF_COUNTER();
	SDL_RenderClear(renderer);
	for(i32 chunk_y = top_chunk.y; chunk_y < bot_chunk.y; chunk_y++)
	{
		for(i32 chunk_x = top_chunk.x; chunk_x < bot_chunk.x; chunk_x++)
		{
			Vec2i chunk_pos_sym = vec(chunk_x, chunk_y);
			Chunk* chunk = chunk_hash_get_or(chunk_hash, chunk_pos_sym, NULL);
			Vec2f64 sym_pos_top = {
				(f64) chunk_pos_sym.x * CHUNK_SIZE, 
				(f64) chunk_pos_sym.y * CHUNK_SIZE
			};
			Vec2f64 sym_pos_bot = {
				(f64) (chunk_pos_sym.x + 1) * CHUNK_SIZE, 
				(f64) (chunk_pos_sym.y + 1) * CHUNK_SIZE
			};

			Vec2i screen_pos_top = to_screen_pos(sym_pos_top, sym_center, screen_center, zoom);
			Vec2i screen_pos_bot = to_screen_pos(sym_pos_bot, sym_center, screen_center, zoom);
			Vec2i screen_size = vec_sub(screen_pos_bot, screen_pos_top);
	
			SDL_Rect dest_rect = {0}; 
			dest_rect.x = (int) screen_pos_top.x; 
			dest_rect.y = (int) screen_pos_top.y; 
			dest_rect.w = (int) screen_size.x; 
			dest_rect.h = (int) screen_size.y; 

			SDL_Texture* clear_tex = clear_tex1;
			u32 clear_color = CLEAR_COLOR_ACTIVE_1;
			if((chunk_pos_sym.y % 2 + chunk_pos_sym.x) % 2)
			{
				clear_color = CLEAR_COLOR_ACTIVE_2;
				clear_tex = clear_tex2;
			}

			if(chunk == NULL)
			{
				SDL_RenderCopy(renderer, clear_tex, NULL, &dest_rect);
			}
			else
			{
				uint32_t* pixels = NULL;
				int pitch = 0;
				SDL_LockTexture(chunk_texture, NULL, (void**) &pixels, &pitch);
				
				for(i32 j = 0; j < CHUNK_SIZE; j++)
					for(i32 i = 0; i < CHUNK_SIZE; i ++)
					{
						if(chunk_get_cell(chunk, vec(i, j)))
							pixels[i + j*CHUNK_SIZE] = (uint32_t) -1; 
						else
							pixels[i + j*CHUNK_SIZE] = clear_color;
					}
			
		
				SDL_UnlockTexture(chunk_texture);
				SDL_RenderCopy(renderer, chunk_texture, NULL, &dest_rect);
			}
		}
	}
	SDL_RenderPresent(renderer);
};



