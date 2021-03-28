#define initialise_arena(_arena, _backing_memory, _size) initialise_memory_arena(_arena, #_arena, _backing_memory, _size)
#define initialise_arena_with_new_memory(_arena, _size) initialise_memory_arena(_arena, #_arena, malloc(_size), _size)

#define ONE_KB 1024
#define ONE_MB 1048576
#define ONE_GB 1073741824l

#define ARENA_NAME_MAX 32

#define DEFAULT_ALIGNMENT (2 * sizeof(void *))

#ifdef LUCERNA_DEBUG

#define arena_allocate(_arena, _size) _arena_allocate((_arena), (_size), DEFAULT_ALIGNMENT, __FILE__, __LINE__)
#define arena_allocate_aligned(_arena, _size, _alignment) _arena_allocate((_arena), (_size), (_alignment), __FILE__, __LINE__); \

#else

#define arena_allocate(_arena, _size) _arena_allocate((_arena), (_size), DEFAULT_ALIGNMENT, NULL, 0)
#define arena_allocate_aligned(_arena, _size, _alignment) _arena_allocate((_arena), (_size), (_alignment), NULL, 0)

#endif

typedef struct
{
 U8 *buffer;         // NOTE(tbt): backing memory
 U64 buffer_size;    // NOTE(tbt): size of the backing memory in bytes
 U64 current_offset; // NOTE(tbt): index to allocate from
 U64 saved_offset;   // NOTE(tbt): 'checkpoint' to return to when the temporary memory scope exits
 
#ifdef LUCERNA_DEBUG
 I8 name[ARENA_NAME_MAX];
#endif
} MemoryArena;

internal void
initialise_memory_arena(MemoryArena *arena,
                        I8 *name,
                        void *backing_memory,
                        U64 size)
{
 assert(backing_memory);
 
 arena->buffer = backing_memory;
 arena->buffer_size = size;
 arena->current_offset = 0;
 arena->saved_offset = 0;
 
#ifdef LUCERNA_DEBUG
 U32 len = calculate_utf8_cstring_size(name) + 1;
 memcpy(arena->name, name, len < ARENA_NAME_MAX ? len : ARENA_NAME_MAX);
#endif
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
_arena_allocate(MemoryArena *arena,
                U64 size,
                U64 alignment,
                I8 *file,
                I32 line)
{
 uintptr_t current_pointer = (uintptr_t)arena->buffer +
  (uintptr_t)arena->current_offset;
 
 uintptr_t offset = align_forward(current_pointer, alignment) -
  (uintptr_t)arena->buffer;
 
 if (offset + size <= arena->buffer_size)
 {
  void *result = arena->buffer + offset;
  arena->current_offset = offset + size;
  
  memset(result, 0, size);
  
  return result;
 }
 else
 {
#ifdef LUCERNA_DEBUG
  debug_log("arena %s: \x1b[31mOUT OF MEMORY!\x1b[0m\n"
            "last allocation was at line %d of file %s\n",
            arena->name,
            line,
            file);
#else
  fprintf(stderr, "\x1b[31mOUT OF MEMORY!\x1b[0m\n");
#endif
  exit(-1);
 }
 return NULL;
}

internal void
arena_free_all(MemoryArena *arena)
{
 arena->current_offset = 0;
 arena->saved_offset = 0;
}

#define arena_temporary_memory(_arena) for (I32 i = (_temporary_memory_begin(_arena), 0); !i; (_temporary_memory_end(_arena), ++i))

internal void
_temporary_memory_begin(MemoryArena *arena)
{
 arena->saved_offset = arena->current_offset;
}

internal void
_temporary_memory_end(MemoryArena *arena)
{
 arena->current_offset = arena->saved_offset;
}

