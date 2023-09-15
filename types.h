#pragma once
#include <stdint.h>
#include <assert.h>

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

//for printf
typedef long long int lld;

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

static Vec2i vec(i32 x, i32 y)
{
	Vec2i out = {x, y};
	return out;
}

static Vec2i vec_add(Vec2i a, Vec2i b)
{
	return vec(a.x + b.x, a.y + b.y);
}

static Vec2i vec_sub(Vec2i a, Vec2i b)
{
	return vec(a.x - b.x, a.y - b.y);
}

static bool vec_equal(Vec2i a, Vec2i b)
{
	return a.x == b.x && a.y == b.y;
}

static i32 div_round_down(i32 val, i32 div_by)
{
	if(val >= 0)
		return val / div_by;
	else
		return (val - div_by + 1) / div_by;
}