typedef struct
{
 U8 *buffer;
 U64 len;
} S8;

#define s8_literal(_cstr) ((S8){ _cstr, strlen(_cstr)})

internal S8
s8_from_cstring(MemoryArena *memory,
                I8 *string)
{
 S8 result;
 
 result.len = strlen(string);
 result.buffer = arena_allocate(memory, result.len);
 memcpy(result.buffer, string, result.len);
 
 return result;
}

internal S8 copy_string(MemoryArena *arena, S8 string);

internal S8
s8_from_char_buffer(MemoryArena *memory,
                    U8 *buffer,
                    U64 len)
{
 S8 result;
 
 result.len = len;
 result.buffer = buffer;
 
 return copy_string(memory, result);
}

internal I8 *
cstring_from_s8(MemoryArena *memory,
                S8 string)
{
 I8 *result = arena_allocate(memory, string.len + 1);
 strncpy(result, string.buffer, string.len);
 return result;
}

internal U64
hash_string(S8 string,
            U32 bounds)
{
 U64 hash = 5381;
 
 for (I32 i = 0;
      i < string.len;
      ++i)
 {
  hash = ((hash << 5) + hash) + string.buffer[i];
 }
 
 return hash % bounds;
}

internal B32
string_match(S8 a,
             S8 b)
{
 return 0 == strncmp(a.buffer,
                     b.buffer,
                     a.len < b.len ?
                     a.len : b.len);
}

internal S8
copy_string(MemoryArena *arena,
            S8 string)
{
 S8 result;
 
 result.len = string.len;
 result.buffer = arena_allocate(arena, string.len);
 memcpy(result.buffer, string.buffer, string.len);
 
 return result;
}

internal void
debug_print_s8(S8 string)
{
 fprintf(stderr, "%.*s\n", (I32)string.len, string.buffer);
}

