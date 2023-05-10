#pragma once
#include <stdint.h>

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

typedef struct Vec2i
{
	i32 x;
	i32 y;
} Vec2i;

typedef struct Vec2f64
{
	f64 x;
	f64 y;
} Vec2f64;

static Vec2i vec_add(Vec2i a, Vec2i b);
static Vec2i vec_sub(Vec2i a, Vec2i b);
static Vec2i vec_mul(i32 a, Vec2i b);
static bool vec_equal(Vec2i a, Vec2i b);

struct Chunk
{
	Vec2i pos;

	//contains 64x64 bit field of cells
	//Only the cells starting at index (1,1) and 
	//ending at (61,61) are considered a part of the chunk
	//the rest is from neighbouring chunks during loading 
	u64 data[64]; 
};

static  Vec2i vec_add(Vec2i a, Vec2i b)
{
	return Vec2i{a.x + b.x, a.y + b.y};
}

static Vec2i vec_sub(Vec2i a, Vec2i b)
{
	return Vec2i{a.x - b.x, a.y - b.y};
}

static Vec2i vec_mul(i32 scalar, Vec2i vec)
{
	return Vec2i{scalar*vec.x, scalar*vec.y};
}

static bool vec_equal(Vec2i a, Vec2i b)
{
	return a.x == b.x && a.y == b.y;
}