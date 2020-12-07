/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 07 Dec 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define initialise_arena(_arena, _backing_memory, _size) initialise_memory_arena(_arena, #_arena, _backing_memory, _size)
#define initialise_arena_with_new_memory(_arena, _size) initialise_memory_arena(_arena, #_arena, malloc(_size), _size)

#define ONE_KB 1024
#define ONE_MB 1048576
#define ONE_GB 1073741824l

#define ARENA_NAME_MAX 32

#define DEFAULT_ALIGNMENT (2 * sizeof(void *))


#ifdef LUCERNA_DEBUG

#define LAST_ALLOCATION_FILE_STRING_SIZE 64
I8 global_last_allocation_file[LAST_ALLOCATION_FILE_STRING_SIZE];
I32 global_last_allocation_line;

#define arena_allocate(_arena, _size) \
    _arena_allocate((_arena), (_size)); \
    strncpy(global_last_allocation_file, __FILE__, LAST_ALLOCATION_FILE_STRING_SIZE); \
    global_last_allocation_line = __LINE__

#define arena_allocate_aligned(_arena, _size, _alignment) \
    _arena_allocate_aligned((_arena), (_size), (_alignment)); \
    strncpy(global_last_allocation_file, __FILE__, LAST_ALLOCATION_FILE_STRING_SIZE); \
    global_last_allocation_line = __LINE__

#else

#define arena_allocate(_arena, _size) _arena_allocate((_arena), (_size))
#define arena_allocate_aligned(_arena, _size, _alignment) _arena_allocate_aligned((_arena), (_size), (_alignment))

#endif

struct MemoryArena
{
    U8 *buffer;
    U64 buffer_size;
    U64 current_offset;
    /* NOTE(tbt): for temporary memory API */
    U64 saved_offset;

#ifdef LUCERNA_DEBUG
    I8 name[ARENA_NAME_MAX];
#endif
};

internal void
initialise_memory_arena(MemoryArena *arena,
                        I8 *name,
                        void *backing_memory,
                        U64 size)
{
    arena->buffer = backing_memory;
    arena->buffer_size = size;
    arena->current_offset = 0;
    arena->saved_offset = 0;

#ifdef LUCERNA_DEBUG
    U32 len = strlen(name) + 1;
    memcpy(arena->name, name, len < ARENA_NAME_MAX ? len : ARENA_NAME_MAX);
    fprintf(stderr, "initialised arena '%s'\n", arena->name);
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
_arena_allocate_aligned(MemoryArena *arena,
                        U64 size,
                        U64 alignment)
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
        fprintf(stderr,
                "arena %s: \x1b[31mOUT OF MEMORY!\x1b[0m\n"
                "last allocation was at line %d of file %s\n",
                arena->name,
                global_last_allocation_line,
                global_last_allocation_file);
#else
        fprintf(stderr, "\x1b[31mOUT OF MEMORY!\x1b[0m\n");
#endif
        exit(-1);
    }
}

internal void *
_arena_allocate(MemoryArena *arena,
                U64 size)
{
    return _arena_allocate_aligned(arena, size, DEFAULT_ALIGNMENT);
}

internal void
arena_free_all(MemoryArena *arena)
{
    arena->current_offset = 0;
    arena->saved_offset = 0;
}

internal void
temporary_memory_begin(MemoryArena *arena)
{
    arena->saved_offset = arena->current_offset;
}

internal void
temporary_memory_end(MemoryArena *arena)
{
    arena->current_offset = arena->saved_offset;
}

/*
MIT License

Copyright (c) 2020 Tom Thornton

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

