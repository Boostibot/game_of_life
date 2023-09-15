#pragma once
#include "alloc.h"
#include "chunk_hash.h"

typedef enum Parse_Error
{
	PARSE_ERROR_NONE = 0,
	PARSE_ERROR_BAD_DIMENSIONS,
	PARSE_ERROR_INVALID_CAHARCTER,
} Parse_Error;

bool read_whole_file_alloc(const char* path, char** into);
Parse_Error parse_text_into_chunks(Chunk_Hash* chunk_hash, const char* read_data);
