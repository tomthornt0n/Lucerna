#ifndef LUCERNA_H
#define LUCERNA_H

#include "glcorearb.h"
#include "KHR/khrplatform.h"
#include "stdint.h"
#include <immintrin.h>

//
// NOTE(tbt): typedefs
//~

#define true  1
#define false 0

#define internal static
#define persist  static

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

typedef int8_t   I8;
typedef int16_t  I16;
typedef int32_t  I32;
typedef int64_t  I64;

typedef float    F32;
typedef double   F64;

typedef uint32_t B32;
typedef float    B32_s;

#include "errno.h"

//
// NOTE(tbt): logging
//~

#ifdef LUCERNA_DEBUG
#define debug_log(_fmt, ...) fprintf(stderr, _fmt, __VA_ARGS__)
#else
#define debug_log(_fmt, ...) 
#endif

//
// NOTE(tbt): handle DLL on windows
//~

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#ifdef LUCERNA_GAME
#define LC_API __declspec(dllimport)
#else
#define LC_API __declspec(dllexport)
#endif
#else
#define LC_API 
#endif

internal U32 calculate_utf8_cstring_size(U8 *cstring);

//
// NOTE(tbt): arenas
//~

#define initialise_arena_with_new_memory(_arena, _size) initialise_arena(_arena, malloc(_size), _size)

enum
{
 ONE_KB = 1024,
 ONE_MB = 1048576,
 ONE_GB = 1073741824l,
};

enum { ARENA_DEFAULT_ALIGNMENT = 2 * sizeof(void *) };

typedef struct
{
 U8 *buffer;         // NOTE(tbt): backing memory
 U64 buffer_size;    // NOTE(tbt): size of the backing memory in bytes
 U64 current_offset; // NOTE(tbt): index to allocate from
 U64 saved_offset;   // NOTE(tbt): 'checkpoint' to return to when the temporary memory scope exits
} MemoryArena;

internal void
initialise_arena(MemoryArena *arena,
                 void *backing_memory,
                 U64 size)
{
 arena->buffer = backing_memory;
 arena->buffer_size = size;
 arena->current_offset = 0;
 arena->saved_offset = 0;
}

internal U64
align_forward(uintptr_t pointer, U64 align)
{
 uintptr_t result, alignment, modulo;
 
 assert((align & (align - 1)) == 0 && "alignment must be power of two");
 
 result = (uintptr_t)pointer;
 alignment = (uintptr_t)align;
 modulo = result & (alignment - 1);
 
 if (modulo != 0)
 {
  result += alignment - modulo;
 }
 
 return result;
}

internal void *
arena_push_FUNCTION(MemoryArena *memory,
                    U64 size,
                    U64 alignment,
                    U8 *arena_name,
                    U8 *file,
                    I32 line)
{
 uintptr_t current_pointer = (uintptr_t)memory->buffer +
  (uintptr_t)memory->current_offset;
 
 uintptr_t offset = align_forward(current_pointer, alignment) -
  (uintptr_t)memory->buffer;
 
 if (offset + size <= memory->buffer_size)
 {
  void *result = memory->buffer + offset;
  memory->current_offset = offset + size;
  
  memset(result, 0, size);
  
  return result;
 }
 else
 {
  debug_log("\n%s(%d): error: arena '%s' out of memory\n",
            file,
            line,
            arena_name);
 }
 return NULL;
}

#define arena_push(_memory, _size) arena_push_FUNCTION((_memory), (_size), ARENA_DEFAULT_ALIGNMENT, #_memory, __FILE__, __LINE__)
#define arena_push_aligned(_memory, _size, _alignment) arena_push_FUNCTION((_memory), (_size), (_alignment), #_memory, __FILE__, __LINE__); \

internal void
arena_pop(MemoryArena *memory,
          U64 size)
{
 memory->current_offset -= size;
}

internal void
arena_free_all(MemoryArena *memory)
{
 memory->current_offset = 0;
 memory->saved_offset = 0;
}

#define arena_temporary_memory(_memory) defer_loop(arena_begin_temporary_memory(_memory), arena_end_temporary_memory(_memory))

internal void
arena_begin_temporary_memory(MemoryArena *memory)
{
 memory->saved_offset = memory->current_offset;
}

internal void
arena_end_temporary_memory(MemoryArena *memory)
{
 memory->current_offset = memory->saved_offset;
}

//
// NOTE(tbt): random utilities
//~

#define array_count(_arr) (sizeof(_arr) / sizeof(_arr[0]))

#define defer_loop(_begin, _end) for (I32 _i = ((_begin), 0); !_i; ((_end), (_i = 1)))

#define _macro_concatenate(_a, _b) _a ## _b
#define macro_concatenate(_a, _b) _macro_concatenate(_a, _b)

typedef struct
{
 F32 x, y;
 F32 w, h;
} Rect;

internal Rect
rect(F32 x,
     F32 y,
     F32 w,
     F32 h)
{
 Rect result;
 result.x = x;
 result.y = y;
 result.w = w;
 result.h = h;
 return result;
}

typedef struct
{
 F32 r, g, b, a;
} Colour;

internal inline Colour
col(F32 r, F32 g, F32 b, F32 a)
{
 Colour result;
 result.r = r;
 result.g = g;
 result.b = b;
 result.a = a;
 return result;
}

#define WHITE col(1.0f, 1.0f, 1.0f, 1.0f)

#define gradient_literal(_tl, _tr, _bl, _br) ((Gradient){ (_tl), (_tr), (_bl), (_br) })
typedef struct
{
 Colour tl, tr, bl, br;
} Gradient;


internal inline F64
min_f(F64 a,
      F64 b)
{
 return a < b ? a : b;
}

internal inline F64
max_f(F64 a,
      F64 b)
{
 return a > b ? a : b;
}

internal inline F64
clamp_f(F64 n,
        F64 min, F64 max)
{
 return max_f(min, min_f(n, max));
}

internal inline F32
abs_f(F32 n)
{
 union { F32 f; U32 u; } c;
 c.f = n;
 c.u &= (1 << 31);
 return c.f;
}

internal inline I64
min_i(I64 a,
      I64 b)
{
 return a < b ? a : b;
}

internal inline I64
max_i(I64 a,
      I64 b)
{
 return a > b ? a : b;
}

internal inline I64
clamp_i(I64 n,
        I64 min, I64 max)
{
 return max_f(min, min_f(n, max));
}

internal inline U64
min_u(U64 a,
      U64 b)
{
 return a < b ? a : b;
}

internal inline U64
max_u(U64 a,
      U64 b)
{
 return a > b ? a : b;
}

internal inline U64
clamp_u(U64 n,
        U64 min, U64 max)
{
 return max_f(min, min_f(n, max));
}


internal inline __m128
min_raw_vec4(__m128 a,
             __m128 b)
{
 __m128 mask = _mm_cmpgt_ps(a, b);
 return _mm_blendv_ps(a, b, mask);
}

internal inline __m128
max_raw_vec4(__m128 a,
             __m128 b)
{
 __m128 mask = _mm_cmplt_ps(a, b);
 return _mm_blendv_ps(a, b, mask);
}

internal void
min_vec4(F32 *a,
         F32 *b,
         F32 *result)
{
 __m128 _a = _mm_load_ps(a);
 __m128 _b = _mm_load_ps(b);
 __m128 _result = min_raw_vec4(_a, _b);
 _mm_store_ps(result, _result);
}

internal void
max_vec4(F32 *a,
         F32 *b,
         F32 *result)
{
 __m128 _a = _mm_load_ps(a);
 __m128 _b = _mm_load_ps(b);
 __m128 _result = max_raw_vec4(_a, _b);
 _mm_store_ps(result, _result);
}

internal inline Rect
offset_rect(Rect rectangle,
            F32 offset_x, F32 offset_y)
{
 return rect(rectangle.x + offset_x,
             rectangle.y + offset_y,
             rectangle.w,
             rectangle.h);
}

internal Rect
rect_at_intersection(Rect a,
                     Rect b)
{
 Rect result;
 
 struct
 {
  F32 min[2];
  F32 max[2];
 }
 _a = { { a.x, a.y }, { a.x + a.w, a.y + a.h} },
 _b = { { b.x, b.y }, { b.x + b.w, b.y + b.h} },
 _result;
 
 _result.min[0] = max_f(_a.min[0], _b.min[0]);
 _result.min[1] = max_f(_a.min[1], _b.min[1]);
 _result.max[0] = min_f(_a.max[0], _b.max[0]);
 _result.max[1] = min_f(_a.max[1], _b.max[1]);
 
 result.x = _result.min[0];
 result.y = _result.min[1];
 result.w = max_f(0.0f, _result.max[0] - _result.min[0]);
 result.h = max_f(0.0f, _result.max[1] - _result.min[1]);
 
 return result;
}

internal B32
rect_match(Rect a,
           Rect b)
{
 B32 eq[4];
 __m128 _a = _mm_load_ps((F32 *)&a);
 __m128 _b = _mm_load_ps((F32 *)&b);
 __m128 mask = _mm_cmpeq_ps(_a, _b);
 _mm_store_ps((F32 *)eq, mask);
 
 return eq[0] && eq[1] && eq[2] && eq[3];
}

internal inline B32
are_rects_intersecting(Rect a,
                       Rect b)
{
 if (a.x + a.w < b.x || a.x > b.x + b.w) { return false; }
 if (a.y + a.h < b.y || a.y > b.y + b.h) { return false; }
 return true;
}

internal inline B32
is_point_in_rect(F32 x, F32 y,
                 Rect region)
{
 return !(x < region.x ||
          y < region.y ||
          x > (region.x + region.w) ||
          y > (region.y + region.h));
}

internal inline Rect
rect_lerp(Rect a,
          Rect b,
          F32 t)
{
 Rect result;
 
 __m128 _a = _mm_load_ps((F32 *)&a);
 __m128 _b = _mm_load_ps((F32 *)&b);
 __m128 _t = _mm_load_ps((F32[]){ t, t, t, t });
 __m128 _x = _mm_load_ps((F32[]){ 1.0f, 1.0f, 1.0f, 1.0f });
 
 _a = _mm_mul_ps(_a, _t);
 _t = _mm_sub_ps(_x, _t);
 _b = _mm_mul_ps(_b, _t);
 _x = _mm_add_ps(_a, _b);
 _mm_store_ps((F32 *)&result, _x);
 
 return result;
}

internal inline Colour
colour_lerp(Colour a,
            Colour b,
            F32 t)
{
 Colour result;
 
 __m128 _a = _mm_load_ps((F32 *)&a);
 __m128 _b = _mm_load_ps((F32 *)&b);
 __m128 _t = _mm_load_ps((F32[]){ t, t, t, t });
 __m128 _x = _mm_load_ps((F32[]){ 1.0f, 1.0f, 1.0f, 1.0f });
 
 _a = _mm_mul_ps(_a, _t);
 _t = _mm_sub_ps(_x, _t);
 _b = _mm_mul_ps(_b, _t);
 _x = _mm_add_ps(_a, _b);
 _mm_store_ps((F32 *)&result, _x);
 
 return result;
}

//
// NOTE(tbt): strings
//~

internal U32 calculate_utf8_cstring_size(U8 *cstring);
internal U32 calculate_utf8_cstring_length(U8 *cstring);

typedef struct
{
 U8 *buffer; // NOTE(tbt): backing memory
 U32 size;   // NOTE(tbt): size of string in bytes
} S8;

#define s8_lit(_cstring) ((S8){ _cstring, sizeof(_cstring) - 1 })

internal inline S8
s8(U8 *cstring)
{
 S8 result;
 result.buffer = cstring;
 result.size = calculate_utf8_cstring_size(cstring);
 return result;
}

#define unravel_s8(_string) (int)_string.size, _string.buffer

internal S8
s8_from_cstring(MemoryArena *memory,
                I8 *string)
{
 S8 result;
 
 result.size = calculate_utf8_cstring_size(string);
 result.buffer = arena_push(memory, result.size);
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
 result.size = vsnprintf(0, 0, format, args) + 1;
 result.buffer = arena_push(memory, result.size);
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
 result = s8_from_format_string_v(memory, format, args);
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
 I8 *result = arena_push(memory, string.size + 1);
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
 if (a.size != b.size) { return false; }
 
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
 result.buffer = arena_push(memory, string.size);
 memcpy(result.buffer, string.buffer, string.size);
 
 return result;
}

internal B32
is_char_alpha_lower(U8 c)
{
 return (c >= 'a' && c <= 'z');
}

internal B32
is_char_alpha_upper(U8 c)
{
 return (c >= 'A' && c <= 'Z');
}

internal B32
is_char_alpha(U8 c)
{
 return (is_char_alpha_upper(c) || is_char_alpha_lower(c));
}

internal B32
is_char_digit(U8 c)
{
 return (c >= '0' && c <= '9');
}

internal B32
is_char_space(U8 c)
{
 return (c == ' ' || c == '\r' || c == '\t' || c == '\f' || c == '\v');
}

internal B32
is_char_symbol(U8 c)
{
 return (c == '~' || c == '!' || c == '@' || c == '#' || c == '$' ||
         c == '%' || c == '^' || c == '&' || c == '*' || c == '(' ||
         c == ')' || c == '-' || c == '=' || c == '+' || c == '[' ||
         c == ']' || c == '{' || c == '}' || c == ':' || c == ';' ||
         c == ',' || c == '<' || c == '.' || c == '>' || c == '/' ||
         c == '?' || c == '|' || c == '\\');
}


internal inline B32
is_word_boundry(S8 string,
                U32 index)
{
 if (index > 0 && index < string.size)
 {
  return ((is_char_space(string.buffer[index - 1]) || is_char_symbol(string.buffer[index - 1])) &&
          (!is_char_space(string.buffer[index]) &&
           !is_char_symbol(string.buffer[index])));
 }
 else
 {
  return true;
 }
}

typedef enum
{
 CURSOR_ADVANCE_DIRECTION_forwards,
 CURSOR_ADVANCE_DIRECTION_backwards,
} CursorAdvanceDirection;

typedef enum
{
 CURSOR_ADVANCE_MODE_char,
 CURSOR_ADVANCE_MODE_word,
} CursorAdvanceMode;

internal inline is_utf8_continuation_byte(S8 string, U32 index);

internal void
advance_cursor(S8 string,
               CursorAdvanceDirection dir,
               CursorAdvanceMode mode,
               U32 *cursor)
{
 I32 delta_table[] = { +1, -1 };
 I32 delta = delta_table[dir];
 
 B32 to_continue = true; // NOTE(tbt): always move at least one character
 while (*cursor + delta >= 0 &&
        *cursor + delta <= string.size &&
        to_continue)
 {
  *cursor += delta;
  to_continue =
   (is_utf8_continuation_byte(string, *cursor) ||
    (mode == CURSOR_ADVANCE_MODE_word && !is_word_boundry(string, *cursor)));
 }
}

internal F64
f64_from_s8(S8 string)
{
 U64 value = 0;
 U64 power = 1;
 I32 sign;
 
 I32 i = 0;
 
 // NOTE(tbt): ignore leading whitespace
 while (i < string.size && is_char_space(string.buffer[i])) { i += 1; }
 
 // NOTE(tbt): get sign
 if (string.buffer[i] == '-')
 {
  sign = -1;
 }
 else
 {
  sign = 1;
 }
 
 if (string.buffer[i] == '+' ||
     string.buffer[i] == '-')
 {
  i += 1;
 }
 
 // NOTE(tbt): read digits
 for (B32 past_decimal = false;
      i < string.size &&
      is_char_digit(string.buffer[i]) || string.buffer[i] == '.';
      ++i)
 {
  if (string.buffer[i] == '.')
  {
   past_decimal = true;
  }
  else
  {
   value *= 10;
   value += string.buffer[i] - '0';
   if (past_decimal)
   {
    power *= 10;
   }
  }
 }
 
 return (F64)sign * (F64)value / (F64)power;
}


//~NOTE(tbt): UTF-8 handling and conversions

typedef struct
{
 U32 codepoint;
 U32 advance;
} UTF8Consume;

typedef struct
{
 U32 *buffer; // NOTE(tbt): backing memory
 U32 size;    // NOTE(tbt): size of buffer in bytes
 U32 len;     // NOTE(tbt): number of characters
} S32;


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
  
  U32 *character = arena_push_aligned(memory, sizeof(*character), 1);
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

internal U32
utf8_from_codepoint(U8 *buffer,
                    U32 codepoint)
{
 U32 advance;
 
 if (codepoint <= 0x7f)
 {
  advance = 1;
  buffer[0] = codepoint;
 }
 else if (codepoint <= 0x7ff)
 {
  advance = 2;
  
  buffer[0] = 0xc0 | (codepoint >> 6);
  buffer[1] = 0x80 | (codepoint & 0x3f);
 }
 else if (codepoint <= 0xffff)
 {
  advance = 3;
  
  buffer[0] = 0xe0 | ((codepoint >> 12));
  buffer[1] = 0x80 | ((codepoint >>  6) & 0x3f);
  buffer[2] = 0x80 | ((codepoint & 0x3f));
 }
 else if (codepoint <= 0xffff)
 {
  advance = 4;
  
  buffer[0] = 0xf0 | ((codepoint >> 18));
  buffer[1] = 0x80 | ((codepoint >> 12) & 0x3f);
  buffer[2] = 0x80 | ((codepoint >>  6) & 0x3f);
  buffer[3] = 0x80 | ((codepoint & 0x3f));
 }
 
 return advance;
}

internal inline
is_utf8_continuation_byte(S8 string,
                          U32 index)
{
 return (1 == (string.buffer[index] & (1 << 7)) &&
         0 == (string.buffer[index] & (1 << 6)));
}

//~NOTE(tbt): string lists

typedef struct S8List S8List;
struct S8List
{
 S8List *next;
 S8 string;
};

internal S8List *
append_s8_to_list(MemoryArena *memory,
                  S8List *list,
                  S8 string)
{
 S8List *new_node = arena_push(memory, sizeof(*new_node));
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
 S8List *result = arena_push(memory, sizeof(*result));
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
 result.buffer = arena_push(memory, result.size);
 memcpy(result.buffer, list->string.buffer, result.size);
 
 for (S8List *node = list->next;
      NULL != node;
      node = node->next)
 {
  arena_push_aligned(memory, node->string.size, 1);
  memcpy(result.buffer + result.size, node->string.buffer, node->string.size);
  result.size += node->string.size;
 }
 
 return result;
}


internal S8
s8_concatenate_v(MemoryArena *temp_memory,
                 MemoryArena *result_memory,
                 U32 count,
                 va_list args)
{
 S8List *result = NULL;
 
 while (count--)
 {
  result = append_s8_to_list(temp_memory, result, va_arg(args, S8));
 }
 
 return join_s8_list(result_memory, result);
}

internal S8
s8_concatenate(MemoryArena *temp_memory,
               MemoryArena *result_memory,
               U32 count,
               ...)
{
 S8 result;
 
 va_list args;
 va_start(args, count);
 result = s8_concatenate_v(temp_memory, result_memory, count, args);
 va_end(args);
 
 return result;
}

//
// NOTE(tbt): input symbolic constants
//~

typedef enum
{
 KEY_none,
 KEY_esc,
 KEY_F1,
 KEY_F2,
 KEY_F3,
 KEY_F4,
 KEY_F5,
 KEY_F6,
 KEY_F7,
 KEY_F8,
 KEY_F9,
 KEY_F10,
 KEY_F11,
 KEY_F12,
 KEY_grave_accent,
 KEY_0,
 KEY_1,
 KEY_2,
 KEY_3,
 KEY_4,
 KEY_5,
 KEY_6,
 KEY_7,
 KEY_8,
 KEY_9,
 KEY_minus,
 KEY_equal,
 KEY_backspace,
 KEY_delete,
 KEY_tab,
 KEY_a,
 KEY_b,
 KEY_c,
 KEY_d,
 KEY_e,
 KEY_f,
 KEY_g,
 KEY_h,
 KEY_i,
 KEY_j,
 KEY_k,
 KEY_l,
 KEY_m,
 KEY_n,
 KEY_o,
 KEY_p,
 KEY_q,
 KEY_r,
 KEY_s,
 KEY_t,
 KEY_u,
 KEY_v,
 KEY_w,
 KEY_x,
 KEY_y,
 KEY_z,
 KEY_space,
 KEY_enter,
 KEY_ctrl,
 KEY_shift,
 KEY_alt,
 KEY_up,
 KEY_left,
 KEY_down,
 KEY_right,
 KEY_page_up,
 KEY_page_down,
 KEY_home,
 KEY_end,
 KEY_forward_slash,
 KEY_period,
 KEY_comma,
 KEY_quote,
 KEY_left_bracket,
 KEY_right_bracket,
 
 
 
 KEY_MAX,
} Key;

//
// NOTE(tbt): mouse input symobls
//~

typedef enum
{
 MOUSE_BUTTON_left     = 0,
 MOUSE_BUTTON_middle   = 1,
 MOUSE_BUTTON_right    = 2,
 
 MOUSE_BUTTON_MAX,
} MouseButton;

//
// NOTE(tbt): platform layer config
//~

enum
{
 PLATFORM_LAYER_FRAME_MEMORY_SIZE = 1 * ONE_MB,
 PLATFORM_LAYER_STATIC_MEMORY_SIZE = 1 * ONE_MB,
 DEFAULT_WINDOW_WIDTH = 1920,
 DEFAULT_WINDOW_HEIGHT = 1040,
 AUDIO_SAMPLERATE = 48000,
};
#define ICON_PATH "../icon.png"
#define WINDOW_TITLE "Lucerna"

//
// NOTE(tbt): platform events
//~

typedef enum
{
 PLATFORM_EVENT_NONE,
 
 PLATFORM_EVENT_KEY_BEGIN,
 PLATFORM_EVENT_mouse_move,
 PLATFORM_EVENT_key_press,
 PLATFORM_EVENT_key_release,
 PLATFORM_EVENT_key_typed,
 PLATFORM_EVENT_KEY_END,
 
 PLATFORM_EVENT_MOUSE_BEGIN,
 PLATFORM_EVENT_mouse_press,
 PLATFORM_EVENT_mouse_release,
 PLATFORM_EVENT_mouse_scroll,
 PLATFORM_EVENT_MOUSE_END,
 
 PLATFORM_EVENT_WINDOW_BEGIN,
 PLATFORM_EVENT_window_resize,
 PLATFORM_EVENT_WINDOW_END,
 
 PLATFORM_EVENT_MAX,
} PlatformEventKind;

typedef U8 InputModifiers;
enum InputModifiers
{
 INPUT_MODIFIER_ctrl  = (1 << 0),
 INPUT_MODIFIER_shift = (1 << 1),
 INPUT_MODIFIER_alt   = (1 << 2),
};

typedef struct PlatformEvent PlatformEvent;
struct PlatformEvent
{
 PlatformEvent *next;
 PlatformEventKind kind;
 
 Key key;
 MouseButton mouse_button;
 InputModifiers modifiers;
 F32 mouse_x, mouse_y;
 I32 mouse_scroll_h, mouse_scroll_v;
 U32 character;
 U32 window_w, window_h;
};

// NOTE(tbt): passed to the game from the platform layer
typedef struct
{
 PlatformEvent *events;
 B32 is_key_down[KEY_MAX];
 B32 is_mouse_button_down[MOUSE_BUTTON_MAX];
 I32 mouse_x, mouse_y;
 I32 mouse_scroll_h, mouse_scroll_v;
 U32 window_w, window_h;
} PlatformState;

//
// NOTE(tbt): event handling helpers
//~

internal B32
is_key_pressed(PlatformState *input,
               Key key,
               InputModifiers modifiers)
{
 for (PlatformEvent *event = input->events;
      NULL != event;
      event = event->next)
 {
  if (event->kind == PLATFORM_EVENT_key_press &&
      event->key == key &&
      event->modifiers == modifiers)
  {
   return true;
  }
 }
 return false;
}

internal B32
is_mouse_button_pressed(PlatformState *input,
                        MouseButton button,
                        InputModifiers modifiers)
{
 for (PlatformEvent *event = input->events;
      NULL != event;
      event = event->next)
 {
  if (event->kind == PLATFORM_EVENT_mouse_press &&
      event->mouse_button == button &&
      event->modifiers == modifiers)
  {
   return true;
  }
 }
 return false;
}

// NOTE(tbt): functions loaded by the platform layer before the game begins
typedef struct
{
#define gl_func(_type, _func) PFNGL ## _type ## PROC _func
#include "gl_funcs.h"
} OpenGLFunctions;

//
// NOTE(tbt): functions in the game called by the platform layer
//~

typedef void ( *GameInit) (OpenGLFunctions *gl);                                                       // NOTE(tbt): called after the platform layer has finished setup - last thing before entering the main loop
typedef void ( *GameUpdateAndRender) (PlatformState *input, F64 frametime_in_s);  // NOTE(tbt): called every frame
typedef void ( *GameAudioCallback) (void *buffer, U64 buffer_size);                                    // NOTE(tbt): called from the audio thread when the buffer needs refilling
typedef void ( *GameCleanup) (void);                                      // NOTE(tbt): called when the window is closed and the main loop exits

//
// NOTE(tbt): functions in the platform layer called by the game
//~

// NOTE(tbt): signal to the platform layer to exit
LC_API void platform_quit(void);

// NOTE(tbt): control for a lock to be used with the audio thread
#define platform_audio_critical_section defer_loop(platform_get_audio_lock(), platform_release_audio_lock())
LC_API void platform_get_audio_lock(void);
LC_API void platform_release_audio_lock(void);

// NOTE(tbt): control visual settings
LC_API void platform_set_vsync(B32 enabled);
LC_API void platform_toggle_fullscreen(void);

// NOTE(tbt): clipboard control
LC_API void platform_set_clipboard_text(S8 text);
LC_API S8 platform_get_clipboard_text(MemoryArena *memory);

// NOTE(tbt): basic file IO
typedef struct PlatformFile PlatformFile;

typedef U32 PlatformOpenFileFlags;
enum PlatformOpenFileFlags
{
 PLATFORM_OPEN_FILE_never_create  = 1 << 0,
 PLATFORM_OPEN_FILE_always_create = 1 << 1, // NOTE(tbt): takes precedence if `never_create` is also specified
 PLATFORM_OPEN_FILE_read          = 1 << 2,
 PLATFORM_OPEN_FILE_write         = 1 << 3,
};

LC_API PlatformFile *platform_open_file(S8 path);
LC_API PlatformFile *platform_open_file_ex(S8 path, PlatformOpenFileFlags flags);
LC_API void platform_close_file(PlatformFile **file);

LC_API U64 platform_get_file_size_f(PlatformFile *file);
LC_API U64 platform_get_file_modified_time_f(PlatformFile *file);
LC_API S8  platform_read_entire_file_f(MemoryArena *memory, PlatformFile *file);
LC_API U64 platform_read_file_f(PlatformFile *file, U64 offset, U64 read_size, void *buffer);
LC_API U64 platform_write_to_file_f(PlatformFile *file, void *buffer, U64 buffer_size);

LC_API U64 platform_get_file_size_p(S8 path);
LC_API U64 platform_get_file_modified_time_p(S8 path);
LC_API S8  platform_read_entire_file_p(MemoryArena *memory, S8 path);
LC_API U64 platform_read_file_p(S8 path, U64 offset, U64 read_size, void *buffer);
LC_API U64 platform_write_entire_file_p(S8 path, void *buffer, U64 buffer_size);
LC_API U64 platform_append_to_file_p(S8 path, void *buffer, U64 buffer_size);

#endif

