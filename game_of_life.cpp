#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <SDL/SDL.h>

#include "chunk_hash.h"
#include "time.h"

//#include "parallel.h"

#define TARGET_FRAME_TIME	16.0
#define DEF_WINDOW_WIDTH	1200
#define DEF_WINDOW_HEIGHT	700
#define DEF_SYM_TIME		30.0

#define CLEAR_COLOR_1		 0x111111FF
#define CLEAR_COLOR_2		 0x070707FF
#define CLEAR_COLOR_ACTIVE_1 0x221111FF
#define CLEAR_COLOR_ACTIVE_2 0x140707FF

#define UPDATE_SCREEN		true
#define UPDATE_SYMULATION	true
#define UPDATE_INPUT		true

#define INPUT_FACTOR_ZOOM						1.02
#define INPUT_FACTOR_INCREASE_SPEED				5000
#define INPUT_FACTOR_INCREASE_SPEED_FRACTION	1.015

#define CHUNK_SIZE 61

bool get_cell_in_chunk(const Chunk* chunk, Vec2i pos)
{
	assert(-1 <= pos.x && pos.x < CHUNK_SIZE + 1);
	assert(-1 <= pos.y && pos.y < CHUNK_SIZE + 1);
	
	u64 bit = (u64) 1 << (pos.x + 1);
	
	return (chunk->data[pos.y + 1] & bit) > 0;
}

void set_cell_in_chunk(Chunk* chunk, Vec2i pos, bool to)
{
	assert(-1 <= pos.x && pos.x < CHUNK_SIZE + 1);
	assert(-1 <= pos.y && pos.y < CHUNK_SIZE + 1);

	u64 bit = (u64) 1 << (pos.x + 1);
	
	if(to)
		chunk->data[pos.y + 1] |= bit;
	else
		chunk->data[pos.y + 1] &= ~bit;
}

i32 div_round_down(i32 val, i32 div_by)
{
	if(val >= 0)
		return val / div_by;
	else
		return (val - div_by + 1) / div_by;
}
	
Vec2i get_chunk_pos(Vec2i sym_position){
	Vec2i output = {0};
	output.x = div_round_down(sym_position.x, CHUNK_SIZE);
	output.y = div_round_down(sym_position.y, CHUNK_SIZE);

	return output;
};
	
Vec2i get_cell_in_chunk_pos(Vec2i sym_position){
	Vec2i output = {0};
	output.x = (sym_position.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	output.y = (sym_position.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

	return output;
};

Vec2i to_screen_pos(Vec2f64 sym_position, Vec2f64 sym_center, Vec2i screen_center, f64 zoom){
	Vec2i screen_offset = {
		(i32) floor((sym_position.x - sym_center.x) * zoom),
		(i32) floor((sym_position.y - sym_center.y) * zoom)
	};

	Vec2i screen_position = vec_add(screen_offset, screen_center);
	return screen_position;
};
	
Vec2f64 to_sym_pos(Vec2i screen_position, Vec2f64 sym_center, Vec2i screen_center, f64 zoom){
	Vec2i screen_offset = vec_sub(screen_position, screen_center);

	Vec2f64 sym_position = {
		(f64) screen_offset.x / zoom + sym_center.x,
		(f64) screen_offset.y / zoom + sym_center.y,
	};

	return sym_position;
};

void set_cell_at(Chunk_Hash* chunk_hash, Vec2i sym_pos, bool to = true){
	Vec2i place_at_chunk = get_chunk_pos(sym_pos);
	Vec2i place_at_pixel = get_cell_in_chunk_pos(sym_pos);

	i32 chunk_i = insert_chunk(chunk_hash, place_at_chunk);
	Chunk* chunk = get_chunk(chunk_hash, chunk_i);
	set_cell_in_chunk(chunk, place_at_pixel, to);

	//@TODO: careful insertion of only the chunks we need!
	if(to)
	{
		const Vec2i directions[8] = {
			{-1, -1},{0, -1},{1, -1},
			{-1, 0},         {1,  0},
			{-1,  1},{0,  1},{1,  1},
		};

		for(i32 i = 0; i < 8; i++)
			insert_chunk(chunk_hash, vec_add(directions[i], place_at_chunk));
	}
};
	

SDL_Rect get_screen_rect(Vec2f64 sym_pos_top, Vec2f64 sym_pos_bot, Vec2f64 sym_center, Vec2i screen_center, f64 zoom)
{
	Vec2i screen_pos_top = to_screen_pos(sym_pos_top, sym_center, screen_center, zoom);
	Vec2i screen_pos_bot = to_screen_pos(sym_pos_bot, sym_center, screen_center, zoom);
	Vec2i screen_size = vec_sub(screen_pos_bot, screen_pos_top);
	
	SDL_Rect screen_rect = {}; 
	screen_rect.x = (int) screen_pos_top.x; 
	screen_rect.y = (int) screen_pos_top.y; 
	screen_rect.w = (int) screen_size.x; 
	screen_rect.h = (int) screen_size.y; 

	return screen_rect;
}

void draw_chunk(Chunk* chunk, Vec2i chunk_pos_sym, Vec2f64 sym_center, Vec2i screen_center, f64 zoom, SDL_Texture* chunk_texture, SDL_Texture* clear_tex1, SDL_Texture* clear_tex2, SDL_Renderer* renderer)
{
	Vec2f64 sym_pos_top = {
		(f64) chunk_pos_sym.x * CHUNK_SIZE, 
		(f64) chunk_pos_sym.y * CHUNK_SIZE
	};
	Vec2f64 sym_pos_bot = {
		(f64) (chunk_pos_sym.x + 1) * CHUNK_SIZE, 
		(f64) (chunk_pos_sym.y + 1) * CHUNK_SIZE
	};

	SDL_Rect dest_rect = get_screen_rect(sym_pos_top, sym_pos_bot, sym_center, screen_center, zoom);
	SDL_Texture* clear_tex = clear_tex1;
	u32 clear_color = CLEAR_COLOR_ACTIVE_1;
	if((chunk_pos_sym.y % 2 + chunk_pos_sym.x) % 2)
	{
		clear_color = CLEAR_COLOR_ACTIVE_2;
		clear_tex = clear_tex2;
	}

	if(chunk == nullptr)
	{
		SDL_RenderCopy(renderer, clear_tex, NULL, &dest_rect);
		return;
	}

	uint32_t* pixels = nullptr;
	int pitch = 0;
	SDL_LockTexture(chunk_texture, NULL, (void**) &pixels, &pitch);
				
	for(i32 j = 0; j < CHUNK_SIZE; j++)
		for(i32 i = 0; i < CHUNK_SIZE; i ++)
		{
			if(get_cell_in_chunk(chunk, Vec2i{i, j}))
				pixels[i + j*CHUNK_SIZE] = (uint32_t) -1; 
			else
				pixels[i + j*CHUNK_SIZE] = clear_color;
		}
			
		
	SDL_UnlockTexture(chunk_texture);
	SDL_RenderCopy(renderer, chunk_texture, NULL, &dest_rect);
};



void update_screen(Vec2i top_chunk, Vec2i bot_chunk, Vec2f64 sym_center, Vec2i screen_center, f64 zoom, Chunk_Hash* chunk_hash, SDL_Texture* chunk_texture, SDL_Texture* clear_tex1, SDL_Texture* clear_tex2, SDL_Renderer* renderer)
{
	PERF_COUNTER();
	SDL_RenderClear(renderer);
	for(i32 chunk_y = top_chunk.y; chunk_y < bot_chunk.y; chunk_y++)
	{
		for(i32 chunk_x = top_chunk.x; chunk_x < bot_chunk.x; chunk_x++)
		{
			Vec2i chunk_pos_sym = Vec2i{chunk_x, chunk_y};
			Chunk* chunk = get_chunk_or(chunk_hash, chunk_pos_sym, nullptr);
			draw_chunk(chunk, chunk_pos_sym, sym_center, screen_center, zoom, chunk_texture, clear_tex1, clear_tex2, renderer);
		}
	}
	SDL_RenderPresent(renderer);
};

#if 0
#define SCREEN_RENDER_THREADS 8
void update_screen(Vec2i top_chunk, Vec2i bot_chunk, Vec2f64 sym_center, Vec2i screen_center, f64 zoom, Chunk_Hash* chunk_hash, 
	SDL_Texture* xchunk_strip, SDL_Texture* ychunk_strip, SDL_Renderer* renderer)
{
	PERF_COUNTER();

	SDL_RenderClear(renderer);
	for(i32 chunk_y = top_chunk.y; chunk_y < bot_chunk.y; chunk_y++)
	{
		for(i32 from_chunk_x = top_chunk.x; from_chunk_x < bot_chunk.x; from_chunk_x += SCREEN_RENDER_THREADS)
		{
			u32* pixels = nullptr;
			int byte_pitch = 0;
			SDL_LockTexture(xchunk_strip, NULL, (void**) &pixels, &byte_pitch);
			u32 pixel_pitch = byte_pitch / sizeof(u32);

			i32 to_chunk_x = from_chunk_x + SCREEN_RENDER_THREADS;
			if(to_chunk_x > bot_chunk.x)
				to_chunk_x = bot_chunk.x + 1;

			Vec2f64 sym_pos_top = {
				(f64) from_chunk_x * CHUNK_SIZE, 
				(f64) chunk_y * CHUNK_SIZE
			};
			Vec2f64 sym_pos_bot = {
				(f64) (to_chunk_x) * CHUNK_SIZE, 
				(f64) (chunk_y + 1) * CHUNK_SIZE
			};

			SDL_Rect dest_rect = get_screen_rect(sym_pos_top, sym_pos_bot, sym_center, screen_center, zoom);
			parallel_for(from_chunk_x, to_chunk_x, [&](i32 chunk_x){
				
				Chunk* chunk = get_chunk_or(chunk_hash, Vec2i{chunk_x, chunk_y}, nullptr);
				u32 clear_color = CLEAR_COLOR_ACTIVE_1;
				if((chunk_y % 2 + chunk_x) % 2)
					clear_color = CLEAR_COLOR_ACTIVE_2;
				
				u32 chunk_offset = (chunk_x - from_chunk_x);
				u32* chunk_pixels = pixels + chunk_offset * CHUNK_SIZE ;
				if(chunk == nullptr)
				{
					for(i32 y = 0; y < CHUNK_SIZE; y++)
					{
						u32* row = chunk_pixels + y*pixel_pitch;
						for(i32 x = 0; x < CHUNK_SIZE; x ++)
							row[x] = clear_color;
					}
				}
				else
				{
					for(i32 y = 0; y < CHUNK_SIZE; y++)
					{
						u32* row = chunk_pixels + y*pixel_pitch;
						for(i32 x = 0; x < CHUNK_SIZE; x ++)
						{
							if(get_cell_in_chunk(chunk, Vec2i{x, y}))
								row[x] = (uint32_t) -1; 
							else
								row[x] = clear_color;
						}
					}
				}
			});
			
			SDL_UnlockTexture(xchunk_strip);
			SDL_RenderCopy(renderer, xchunk_strip, NULL, &dest_rect);
		}
	}
	SDL_RenderPresent(renderer);
};
#endif

Vec2i get_mouse_pos(u32* state = nullptr)
{
	int x = 0;
	int y = 0;
	u32 local_state = (i32) SDL_GetMouseState(&x, &y);
	if(state != nullptr)
		*state = local_state;

	return {x, y};
}

int main(int argc, char *argv[]) {

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
			return 1;

	SDL_Window* window = SDL_CreateWindow("Game of Life", 100, 100, 
			DEF_WINDOW_WIDTH, 
			DEF_WINDOW_HEIGHT, 
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	
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
				

	//We keep two hashes and swap between them on every uodate
	#define CHUNK_HASHES_COUNT 2
	Chunk_Hash chunk_hashes[CHUNK_HASHES_COUNT] = {};
	for(i32 i = 0; i < CHUNK_HASHES_COUNT; i++)
		chunk_hashes[i] = create_chunk_hash();

	Chunk empty_chunks[9] = {0};
	
	i64 generation = 0;
	Chunk_Hash* curr_chunk_hash  = &chunk_hashes[(generation + 0) % CHUNK_HASHES_COUNT];
	Chunk_Hash* next_chunk_hash  = &chunk_hashes[(generation + 1) % CHUNK_HASHES_COUNT];

	// main loop
	f64 zoom = 3.0;
	f64 symulation_time = DEF_SYM_TIME;
	Vec2f64 sym_center = {0.0, 0.0};
	Vec2i window_size = {DEF_WINDOW_WIDTH, DEF_WINDOW_HEIGHT};
	Vec2i screen_center = {window_size.x / 2, window_size.y / 2};
	
	f64 dt = 1.0 / TARGET_FRAME_TIME * 1000;
	f64 last_sym_update_clock = clock_s();
	f64 last_screen_update_clock = clock_s();
	f64 last_frame_clock = clock_s();

	for(i32 x = -250; x < 250; x++)
		for(i32 y = -250; y < 250; y++)
			set_cell_at(curr_chunk_hash, Vec2i{x, y});

	Vec2i old_mouse_pos = get_mouse_pos();
	bool paused = false;

	while(true) 
	{
		// event handling
		SDL_Event event;
		if(SDL_PollEvent(&event)) 
		{
			if(event.type == SDL_QUIT)
				break;
				
			const u8* keayboard_state = SDL_GetKeyboardState(nullptr);
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

				const auto set_cell_at_f = [](Chunk_Hash* chunk_hash, Vec2f64 sym_posf, bool to){
					Vec2i sym_pos = {
						(i32) round(sym_posf.x),
						(i32) round(sym_posf.y),
					};

					set_cell_at(chunk_hash, sym_pos, to);
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

		if((clock_s() - last_screen_update_clock)*1000 >= TARGET_FRAME_TIME && UPDATE_SCREEN)
		{
			update_screen(top_chunk, bot_chunk, sym_center, screen_center, zoom, curr_chunk_hash, chunk_texture, clear_chunk_texture1, clear_chunk_texture2, renderer);
			last_screen_update_clock = clock_s();
		}
		
		if((clock_s() - last_sym_update_clock)*1000 >= symulation_time && paused == false && UPDATE_SYMULATION)
		{
			generation++;
			clear_chunk_hash(next_chunk_hash);

			PERF_COUNTER("sym update");
			f64 clock_update = clock_s();
			for(i32 i = 0; i < curr_chunk_hash->chunk_size; i++)
			{
				PERF_COUNTER("single chunk");
				Chunk* chunk = &curr_chunk_hash->chunks[i];
				
				Chunk* top   = nullptr;
				Chunk* bot   = nullptr;
				Chunk* left  = nullptr;
				Chunk* right = nullptr;
				
				Chunk* top_l = nullptr;
				Chunk* top_r = nullptr;
				Chunk* bot_l = nullptr;
				Chunk* bot_r = nullptr;

				{
					PERF_COUNTER("neighbour gather");
					//@TODO: only add only needed chunks just like in the add after
					//save the computation from last time

					//@TODO: multithread this computation by adding processed chunks into separate arrays and then merging them tothether
					//

					top   = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 0, -1}), empty_chunks);
					bot   = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 0,  1}), empty_chunks);
					left  = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{-1,  0}), empty_chunks);
					right = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 1,  0}), empty_chunks);
				
					top_l = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{-1, -1}), empty_chunks);
					top_r = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 1, -1}), empty_chunks);
					bot_l = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{-1,  1}), empty_chunks);
					bot_r = get_chunk_or(curr_chunk_hash, vec_add(chunk->pos, Vec2i{ 1,  1}), empty_chunks);
				}

				#define OUTER (CHUNK_SIZE + 1)

				u64 R_OUTER_BIT = ((u64) 1 << OUTER);
				u64 L_OUTER_BIT = ((u64) 1);
				u64 CONTENT_BITS = (((u64) -1) & ~R_OUTER_BIT) & ~L_OUTER_BIT;
				for(i32 i = OUTER; i < 64; i++)
					CONTENT_BITS &= ~((u64) 1 << i);

				//@TODO: use get set for this somehow & figure out what to do with middles
				Chunk assembled = {};

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

				for(i32 i = 0; i < CHUNK_SIZE; i++)
				{
					u64 middle = chunk->data[i + 1] & CONTENT_BITS;
					u64 first = (left->data[i + 1] << 1) & R_OUTER_BIT;
					u64 last = (right->data[i + 1] >> 1) & L_OUTER_BIT;

					assembled.data[1 + i] = (first >> OUTER) | middle | (last << OUTER);
				}
				
				Chunk new_chunk = {0};
				new_chunk.pos = chunk->pos;
				u64 pattern = 011'11111'11111'11111'11111;//pattern of 0b001001... repeating (in oct)
				u64 oct0 = pattern << 0; //pattern of 0b001001...
				u64 oct1 = pattern << 1; //pattern of 0b010010...
				u64 oct2 = pattern << 2; //pattern of 0b100100...
					
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

							new_chunk.data[y] |= (is_next_alive >> (2 - slot));
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
					i32 curr_i = insert_chunk(next_chunk_hash, chunk->pos);
					*get_chunk(next_chunk_hash, curr_i) = new_chunk;
					
					//left right 
					if(acummulated & ((u64) 1 << 1))
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, Vec2i{-1,  0}));
					if(acummulated & ((u64) 1 << CHUNK_SIZE))
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, Vec2i{1,  0}));

					//top bot
					if(new_chunk.data[1] & CONTENT_BITS)
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, Vec2i{0,  -1}));
					if(new_chunk.data[CHUNK_SIZE] & CONTENT_BITS)
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, Vec2i{0,  1}));
						
					//diagonals - there is only very small chence these will get added
					if(get_cell_in_chunk(&new_chunk, Vec2i{0, 0}))
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, Vec2i{-1, -1}));
					if(get_cell_in_chunk(&new_chunk, Vec2i{0, CHUNK_SIZE - 1}))
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, Vec2i{-1,  1}));
					if(get_cell_in_chunk(&new_chunk, Vec2i{CHUNK_SIZE - 1, 0}))
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, Vec2i{1,  -1}));
					if(get_cell_in_chunk(&new_chunk, Vec2i{CHUNK_SIZE - 1, CHUNK_SIZE - 1}))
						insert_chunk(next_chunk_hash, vec_add(chunk->pos, Vec2i{1,  1}));
				}
			}
			
			curr_chunk_hash  = &chunk_hashes[(generation + 0) % CHUNK_HASHES_COUNT];
			next_chunk_hash  = &chunk_hashes[(generation + 1) % CHUNK_HASHES_COUNT];

			last_sym_update_clock = clock_s();	
			//printf("update time: %lf\n", clock_s() - clock_update);
		}

		f64 new_frame_clock = clock_s();
		dt = new_frame_clock - last_frame_clock;

		//printf("fps: %lf\n", 1.0 / dt);
		last_frame_clock = new_frame_clock;
	}
	
	printf("total time: %lf\n", clock_s());
	printf("generations: %d\n", (int) generation);
	printf("generations/s: %lf\n", generation / clock_s());
	for(isize i = 0; i < MAX_PERF_COUNTERS; i++)
	{
		if(perf_counters[i].function != nullptr)
		{
			Perf_Counter* counter = &perf_counters[i];
			f64 total_s = (f64) counter->counter / (f64) perf_counter_freq() ;
			f64 per_run_s = total_s / (f64) counter->runs;
			using ll = long long;

			if(counter->name)
				printf("%s %lld \"%s\": total: %8.8lf run: %8.8lf runs: %lld\n", 
					counter->function, (ll) counter->line, counter->name, total_s, per_run_s, (ll) counter->runs);
			else
				printf("%s %lld: total: %8.8lf run: %8.8lf runs: %lld\n", 
					counter->function, (ll) counter->line, total_s, per_run_s, (ll) counter->runs);
		}
	}
	
	#ifdef DO_CLEANUP
	SDL_DestroyTexture(xstrip_chunk_texture);
	SDL_DestroyTexture(ystrip_chunk_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	
	for(i32 i = 0; i < CHUNK_HASHES_COUNT; i++)
		destroy_chunk_hash(&chunk_hashes[i]);
	#endif // DO_CLEANUP

	SDL_Quit(); 
	return 0;
}


