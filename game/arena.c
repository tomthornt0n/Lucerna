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
                    U8 *arena,
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
            arena);
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

