#define _CRT_SECURE_NO_WARNINGS

#include "alloc.h"
#include "load.h"
#include "types.h"

bool read_whole_file_alloc(const char* path, char** into)
{
	size_t alloced_size = 0;
	size_t data_size = 0;
	size_t chunk_size = 1024;
	char* data = NULL;
	
	FILE* file = fopen(path, "rb");
	if(file == NULL)
		return false;

	while(true)
	{
		if(data_size + chunk_size + 1 > alloced_size)
		{
			size_t new_size = (data_size + chunk_size) * 2 + 1;
			void* new_data = sure_realloc(data, data_size, new_size);
			if(new_data == NULL)
				break;

			data = (char*) new_data;
		}

        size_t read = fread(data + data_size, 1, chunk_size, file);
        if (read == 0)
            break;

        data_size += read;
		chunk_size = chunk_size * 3/2;
	}
	
	if(data != NULL)
		data[data_size - 1] = '\0';
		
	fclose(file);
	*into = data;
	return true;
}

Parse_Error parse_text_into_chunks(Chunk_Hash* chunk_hash, const char* read_data)
{
	int width = -1;
	int height = -1;
	i32 size = read_data ? (i32) strlen(read_data) : 0;
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
					
					i32 chunk_i = chunk_hash_insert(chunk_hash, Vec2i{chunk_x, chunk_y});
					Chunk* chunk = chunk_hash_at(chunk_hash, chunk_i);
					assert(chunk != NULL);
					chunk_set_cell(chunk, vec(local_x, local_y), true);
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
