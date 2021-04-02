typedef struct
{
 U8 *buffer;
 U32 size;
} S8;

typedef struct
{
 U32 *buffer;
 U32 size; // NOTE(tbt): size of buffer in bytes
 U32 len;  // NOTE(tbt): number of characters
} S32;

typedef struct S8List S8List;
struct S8List
{
 S8List *next;
 S8 string;
};

typedef struct
{
 U32 codepoint;
 U32 advance;
} UTF8Consume;

#define s8_literal(_cstr) ((S8){ _cstr, calculate_utf8_cstring_size(_cstr)})

internal S8
s8_from_cstring(MemoryArena *memory,
                I8 *string)
{
 S8 result;
 
 result.size = strlen(string);
 result.buffer = arena_allocate(memory, result.size);
 memcpy(result.buffer, string, result.size);
 
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
 result.size = vsnprintf(0, 0, format, args);
 result.buffer = arena_allocate(memory, result.size);
 vsnprintf(result.buffer, result.size, format, _args);
 
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
                    U64 size)
{
 S8 result;
 
 result.size = size;
 result.buffer = buffer;
 
 return copy_s8(memory, result);
}

internal I8 *
cstring_from_s8(MemoryArena *memory,
                S8 string)
{
 I8 *result = arena_allocate(memory, string.size + 1);
 strncpy(result, string.buffer, string.size);
 return result;
}

internal U64
hash_s8(S8 string,
        U32 bounds)
{
 U64 hash = 5381;
 
 for (I32 i = 0;
      i < string.size;
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
                     a.size < b.size ?
                     a.size : b.size);
}

internal S8
copy_s8(MemoryArena *memory,
        S8 string)
{
 S8 result;
 
 result.size = string.size;
 result.buffer = arena_allocate(memory, string.size);
 memcpy(result.buffer, string.buffer, string.size);
 
 return result;
}

internal void
debug_print_s8(S8 string)
{
 fprintf(stderr, "%.*s\n", (I32)string.size, string.buffer);
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
join_s8_list(MemoryArena *memory,
             S8List *list)
{
 S8 result = {0};
 result.size = list->string.size;
 result.buffer = arena_allocate(memory, result.size);
 memcpy(result.buffer, list->string.buffer, result.size);
 
 for (S8List *node = list->next;
      NULL != node;
      node = node->next)
 {
  arena_allocate_aligned(memory, node->string.size, 1);
  memcpy(result.buffer + result.size, node->string.buffer, node->string.size);
  result.size += node->string.size;
 }
 
 return result;
}

internal UTF8Consume
consume_utf8_from_buffer(U8 *buffer,
                         U32 index,
                         U32 max)
{
 U8 utf8_class[32] = 
 {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,2,2,2,2,3,3,4,5,
 };
 
 U32 bitmask_3 = 0x07;
 U32 bitmask_4 = 0x0F;
 U32 bitmask_5 = 0x1F;
 U32 bitmask_6 = 0x3F;
 
 UTF8Consume result = { ~((U32)0), 1 };
 U8 byte = buffer[index];
 U8 byte_class = utf8_class[byte >> 3];
 
 switch (byte_class)
 {
  case 1:
  {
   result.codepoint = byte;
   break;
  }
  
  case 2:
  {
   if (2 <= max)
   {
    U8 cont_byte = buffer[index + 1];
    if (0 == utf8_class[cont_byte >> 3])
    {
     result.codepoint = (byte & bitmask_5) << 6;
     result.codepoint |= (cont_byte & bitmask_6);
     result.advance = 2;
    }
   }
   break;
  }
  
  case 3:
  {
   if (3 <= max)
   {
    U8 cont_byte[2] = { buffer[index + 1], buffer[index + 2] };
    if (0 == utf8_class[cont_byte[0] >> 3] &&
        0 == utf8_class[cont_byte[1] >> 3])
    {
     result.codepoint = (byte & bitmask_4) << 12;
     result.codepoint |= ((cont_byte[0] & bitmask_6) << 6);
     result.codepoint |= (cont_byte[1] & bitmask_6);
     result.advance = 3;
    }
   }
   break;
  }
  
  case 4:
  {
   if (4 <= max)
   {
    U8 cont_byte[3] =
    {
     buffer[index + 1],
     buffer[index + 2],
     buffer[index + 3]
    };
    if (0 == utf8_class[cont_byte[0] >> 3] &&
        0 == utf8_class[cont_byte[1] >> 3] &&
        0 == utf8_class[cont_byte[2] >> 3])
    {
     result.codepoint = (byte & bitmask_3) << 18;
     result.codepoint |= (cont_byte[0] & bitmask_6) << 12;
     result.codepoint |= (cont_byte[1] & bitmask_6) << 6;
     result.codepoint |= (cont_byte[2] & bitmask_6);
     result.advance = 4;
    }
   }
   break;
  }
 }
 
 return result;
}

internal UTF8Consume
consume_utf8_from_string(S8 string,
                         U32 index)
{
 return consume_utf8_from_buffer(string.buffer,
                                 index,
                                 string.size - index);
}

internal S32
s32_from_s8(MemoryArena *memory,
            S8 string)
{
 S32 result = {0};
 
 U64 i = 0;
 while (i < string.size)
 {
  UTF8Consume consume = consume_utf8_from_string(string, i);
  
  U32 *character = arena_allocate_aligned(memory, sizeof(*character), 1);
  if (!result.buffer)
  {
   result.buffer = character;
  }
  *character = consume.codepoint;
  result.len += 1;
  
  i += consume.advance;
 }
 
 return result;
}

internal U32
calculate_utf8_cstring_size(U8 *cstring)
{
 U32 result = 0;
 
 for (;;)
 {
  UTF8Consume consume = consume_utf8_from_buffer(cstring, result, 4);
  if (0 == consume.codepoint)
  {
   break;
  }
  else
  {
   result += consume.advance;
  }
 }
 
 return result;
}


internal U32
calculate_utf8_cstring_length(U8 *cstring)
{
 U32 result = 0;
 
 U32 i = 0;
 for (;;)
 {
  UTF8Consume consume = consume_utf8_from_buffer(cstring, i, 4);
  if (0 == consume.codepoint)
  {
   break;
  }
  else
  {
   i += consume.advance;
   result += 1;
  }
 }
 
 return result;
}