typedef struct
{
 union
 {
  U8 *buffer;
  void * data;
 };
 U64 len;
} S8;

typedef struct S8List S8List;
struct S8List
{
 S8List *next;
 S8 string;
};

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

internal S8
s8_from_format_string_v(MemoryArena *memory,
                        char *format,
                        va_list args)
{
 S8 result;
 
 va_list _args;
 va_copy(_args, args);
 result.len = vsnprintf(0, 0, format, args);
 result.buffer = arena_allocate(memory, result.len);
 vsnprintf(result.buffer, result.len, format, _args);
 
 return result;
}

internal S8
s8_from_format_string(MemoryArena *memory,
                      char *format,
                      ...)
{
 S8 result;
 
 va_list args;
 va_start(args, format);
 result = s8_from_format_string_v(memory,
                                  format,
                                  args);
 va_end(args);
 
 return result;
}

internal S8 copy_s8(MemoryArena *arena, S8 string);

internal S8
s8_from_char_buffer(MemoryArena *memory,
                    U8 *buffer,
                    U64 len)
{
 S8 result;
 
 result.len = len;
 result.buffer = buffer;
 
 return copy_s8(memory, result);
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
hash_s8(S8 string,
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
s8_match(S8 a,
         S8 b)
{
 return 0 == strncmp(a.buffer,
                     b.buffer,
                     a.len < b.len ?
                     a.len : b.len);
}

internal S8
copy_s8(MemoryArena *memory,
        S8 string)
{
 S8 result;
 
 result.len = string.len;
 result.buffer = arena_allocate(memory, string.len);
 memcpy(result.buffer, string.buffer, string.len);
 
 return result;
}

internal void
debug_print_s8(S8 string)
{
 fprintf(stderr, "%.*s\n", (I32)string.len, string.buffer);
}

internal S8List *
append_s8_to_list(MemoryArena *memory,
                  S8List *list,
                  S8 string)
{
 S8List *new_node = arena_allocate(memory, sizeof(*new_node));
 new_node->string = string;
 
 S8List **indirect = &list;
 while (NULL != *indirect) { indirect = &(*indirect)->next; }
 *indirect = new_node;
 
 return list;
}

internal S8List *
push_s8_to_list(MemoryArena *memory,
                S8List *list,
                S8 string)
{
 S8List *result = arena_allocate(memory, sizeof(*result));
 result->next = list;
 result->string = string;
 return result;
}

internal S8
expand_s8_list(MemoryArena *memory,
               S8List *list)
{
 S8 result = {0};
 result.len = list->string.len;
 result.buffer = arena_allocate(memory, result.len);
 memcpy(result.buffer, list->string.buffer, result.len);
 
 for (S8List *node = list->next;
      NULL != node;
      node = node->next)
 {
  arena_allocate_aligned(memory, node->string.len, 1);
  memcpy(result.buffer + result.len, node->string.buffer, node->string.len);
  result.len += node->string.len;
 }
 
 return result;
}