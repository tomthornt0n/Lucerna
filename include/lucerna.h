#ifndef LUCERNA_H
#define LUCENRA_H

#include "glcorearb.h"
#include "khrplatform.h"
#include "stdint.h"

#define true  1
#define false 0

#define internal static

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

typedef char     I8;
typedef int16_t  I16;
typedef int32_t  I32;
typedef int64_t  I64;

typedef float    F32;
typedef double   F64;

typedef uint32_t B32;

#include "errno.h"

internal F64
min_f(F64 a,
      F64 b)
{
    return a < b ? a : b;
}

internal F64
max_f(F64 a,
      F64 b)
{
    return a > b ? a : b;
}

internal F64
clamp_f(F64 n,
        F64 min, F64 max)
{
    return max_f(min, min_f(n, max));
}

internal F32
reciprocal_sqrt_f(F32 n)
{
    union FloatAsInt { F32 f; I32 i; } i;

    i.f = n;
    i.i = 0x5f375a86 - (i.i >> 1);
    i.f *= 1.5f - (i.f * 0.5f * i.f * i.f);

    return i.f;
}

internal I64
min_i(I64 a,
      I64 b)
{
    return a < b ? a : b;
}

internal I64
max_i(I64 a,
      I64 b)
{
    return a > b ? a : b;
}

internal I64
clamp_i(I64 n,
        I64 min, I64 max)
{
    return max_f(min, min_f(n, max));
}

internal U64
min_u(U64 a,
      U64 b)
{
    return a < b ? a : b;
}

internal U64
max_u(U64 a,
      U64 b)
{
    return a > b ? a : b;
}

internal U64
clamp_u(U64 n,
        U64 min, U64 max)
{
    return max_f(min, min_f(n, max));
}

U64
hash_string(I8 *string,
            U32 bounds)
{
    U64 hash = 5381;
    I32 c;

    while (c = *string++)
    {
        hash = ((hash << 5) + hash) + c;
    }

    return hash % bounds;
}

#define BIT(n) (1 << n)

#define RECTANGLE(_x, _y, _w, _h) ((Rectangle){ (_x), (_y), (_w), (_h) })
typedef struct
{
    F32 x, y;
    F32 w, h;
} Rectangle;

#define COLOUR(_r, _g, _b, _a) ((Colour){ (_r), (_g), (_b), (_a) })
typedef struct
{
    F32 r, g, b, a;
} Colour;

internal B32
rectangles_are_intersecting(Rectangle a,
                            Rectangle b)
{
    if (a.x + a.w < b.x || a.x > b.x + b.w) { return false; }
    if (a.y + a.h < b.y || a.y > b.y + b.h) { return false; }
    return true;
}

internal B32
point_is_in_region(F32 x, F32 y,
                   Rectangle region)
{
    if (x < region.x              ||
        y < region.y              ||
        x > (region.x + region.w) ||
        y > (region.y + region.h))
    {
        return false;
    }
    return true;
}


#define KEY_NONE                0
#define KEY_A                   4
#define KEY_B                   5
#define KEY_C                   6
#define KEY_D                   7
#define KEY_E                   8
#define KEY_F                   9
#define KEY_G                   10
#define KEY_H                   11
#define KEY_I                   12
#define KEY_J                   13
#define KEY_K                   14
#define KEY_L                   15
#define KEY_M                   16
#define KEY_N                   17
#define KEY_O                   18
#define KEY_P                   19
#define KEY_Q                   20
#define KEY_R                   21
#define KEY_S                   22
#define KEY_T                   23
#define KEY_U                   24
#define KEY_V                   25
#define KEY_W                   26
#define KEY_X                   27
#define KEY_Y                   28
#define KEY_Z                   29
#define KEY_1                   30
#define KEY_2                   31
#define KEY_3                   32
#define KEY_4                   33
#define KEY_5                   34
#define KEY_6                   35
#define KEY_7                   36
#define KEY_8                   37
#define KEY_9                   38
#define KEY_0                   39
#define KEY_ESCAPE              41
#define KEY_DELETE              42
#define KEY_TAB                 43
#define KEY_SPACE               44
#define KEY_MINUS               45
#define KEY_EQUALS              46
#define KEY_LEFT_BRACKET        47
#define KEY_RIGHT_BRACKET       48
#define KEY_BACKSLASH           49
#define KEY_SEMICOLON           51
#define KEY_QUOTE               52
#define KEY_GRAVE               53
#define KEY_COMMA               54
#define KEY_PERIOD              55
#define KEY_SLASH               56
#define KEY_CAPS_LOCK           57
#define KEY_F1                  58
#define KEY_F2                  59
#define KEY_F3                  60
#define KEY_F4                  61
#define KEY_F5                  62
#define KEY_F6                  63
#define KEY_F7                  64
#define KEY_F8                  65
#define KEY_F9                  66
#define KEY_F10                 67
#define KEY_F11                 68
#define KEY_F12                 69
#define KEY_PRINT_SCREEN        70
#define KEY_SCROLL_LOCK         71
#define KEY_PAUSE               72
#define KEY_INSERT              73
#define KEY_HOME                74
#define KEY_PAGEUP              75
#define KEY_DEL                 76
#define KEY_END                 77
#define KEY_PAGE_DOWN           78
#define KEY_RIGHT               79
#define KEY_LEFT                80
#define KEY_DOWN                81
#define KEY_UP                  82
#define KEY_KP_NUM_LOCK         83
#define KEY_KP_DIVIDE           84
#define KEY_KP_MULTIPLY         85
#define KEY_KP_SUBTRACT         86
#define KEY_KP_ADD              87
#define KEY_KP_ENTER            88
#define KEY_KP_1                89
#define KEY_KP_2                90
#define KEY_KP_3                91
#define KEY_KP_4                92
#define KEY_KP_5                93
#define KEY_KP_6                94
#define KEY_KP_7                95
#define KEY_KP_8                96
#define KEY_KP_9                97
#define KEY_KP_0                98
#define KEY_KP_POINT            99
#define KEY_NON_US_BACKSLASH    100
#define KEY_KP_EQUALS           103
#define KEY_F13                 104
#define KEY_F14                 105
#define KEY_F15                 106
#define KEY_F16                 107
#define KEY_F17                 108
#define KEY_F18                 109
#define KEY_F19                 110
#define KEY_F20                 111
#define KEY_F21                 112
#define KEY_F22                 113
#define KEY_F23                 114
#define KEY_F24                 115
#define KEY_HELP                117
#define KEY_MENU                118
#define KEY_MUTE                127
#define KEY_SYS_REQ             154
#define KEY_RETURN              158
#define KEY_KP_CLEAR            216
#define KEY_KP_DECIMAL          220
#define KEY_LEFT_CONTROL        224
#define KEY_LEFT_SHIFT          225
#define KEY_LEFT_ALT            226
#define KEY_LEFT_SUPER          227
#define KEY_RIGHT_CONTROL       228
#define KEY_RIGHT_SHIFT         229
#define KEY_RIGHT_ALT           230
#define KEY_RIGHT_SUPER         231

#define MOUSE_BUTTON_LEFT       0
#define MOUSE_BUTTON_MIDDLE     1
#define MOUSE_BUTTON_RIGHT      2
#define MOUSE_BUTTON_4          3
#define MOUSE_BUTTON_5          4
#define MOUSE_BUTTON_6          5
#define MOUSE_BUTTON_7          6
#define MOUSE_BUTTON_8          7

#define CTRL(_char) ((_char) - 96)

// NOTE(tbt): the platform layer constructs a linked list of the ASCII values relating
//            to all of the key presses for a given frame
typedef struct KeyTyped KeyTyped;
struct KeyTyped
{
    KeyTyped *next;
    I8 key;
};

// NOTE(tbt): input recorded by the platform layer in the main event processing loop
typedef struct
{
    B32 is_key_pressed[256];
    KeyTyped *keys_typed;
    B32 is_mouse_button_pressed[8];
    I16 mouse_x, mouse_y;
    I32 mouse_scroll;
    U32 window_width, window_height;
} PlatformState;

// NOTE(tbt): functions loaded by the platform layer before the game begins
typedef struct
{
    PFNGLATTACHSHADERPROC            AttachShader;
    PFNGLBINDBUFFERPROC              BindBuffer;
    PFNGLBINDFRAMEBUFFERPROC         BindFramebuffer;
    PFNGLBINDTEXTUREPROC             BindTexture;
    PFNGLBINDVERTEXARRAYPROC         BindVertexArray;
    PFNGLBLENDFUNCPROC               BlendFunc;
    PFNGLBLITFRAMEBUFFERPROC         BlitFramebuffer;
    PFNGLBUFFERDATAPROC              BufferData;
    PFNGLBUFFERSUBDATAPROC           BufferSubData;
    PFNGLCLEARPROC                   Clear;
    PFNGLCLEARCOLORPROC              ClearColor;
    PFNGLCOMPILESHADERPROC           CompileShader;
    PFNGLCREATEPROGRAMPROC           CreateProgram;
    PFNGLCREATESHADERPROC            CreateShader;
    PFNGLDELETEBUFFERSPROC           DeleteBuffers;
    PFNGLDELETEPROGRAMPROC           DeleteProgram;
    PFNGLDELETESHADERPROC            DeleteShader;
    PFNGLDELETETEXTURESPROC          DeleteTextures;
    PFNGLDELETEVERTEXARRAYSPROC      DeleteVertexArrays;
    PFNGLDETACHSHADERPROC            DetachShader;
    PFNGLDISABLEPROC                 Disable;
    PFNGLDRAWELEMENTSPROC            DrawElements;
    PFNGLENABLEPROC                  Enable;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
    PFNGLFRAMEBUFFERTEXTURE2DPROC    FramebufferTexture2D;
    PFNGLGENBUFFERSPROC              GenBuffers;
    PFNGLGENFRAMEBUFFERSPROC         GenFramebuffers;
    PFNGLGENTEXTURESPROC             GenTextures;
    PFNGLGENVERTEXARRAYSPROC         GenVertexArrays;
    PFNGLGETERRORPROC                GetError;
    PFNGLGETPROGRAMIVPROC            GetProgramiv;
    PFNGLGETUNIFORMLOCATIONPROC      GetUniformLocation;
    PFNGLGETSHADERINFOLOGPROC        GetShaderInfoLog;
    PFNGLGETSHADERIVPROC             GetShaderiv;
    PFNGLLINKPROGRAMPROC             LinkProgram;
    PFNGLSCISSORPROC                 Scissor;
    PFNGLSHADERSOURCEPROC            ShaderSource;
    PFNGLTEXIMAGE2DPROC              TexImage2D;
    PFNGLTEXPARAMETERIPROC           TexParameteri;
    PFNGLUNIFORMMATRIX4FVPROC        UniformMatrix4fv;
    PFNGLUNIFORM2FPROC               Uniform2f;
    PFNGLUSEPROGRAMPROC              UseProgram;
    PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer;
    PFNGLVIEWPORTPROC                Viewport;
} OpenGLFunctions;

enum
{
    GAME_STATE_PLAYING,
    GAME_STATE_EDITOR,
};

// NOTE(tbt): memory arenas are used by both the platform layer and the game
typedef struct MemoryArena MemoryArena;

// NOTE(tbt): the functions called by the platform layer
typedef void ( *GameInit) (OpenGLFunctions *gl);                                                       // NOTE(tbt): called after the platform layer has finished setup - last thing before entering the main loop
typedef void ( *GameUpdateAndRender) (OpenGLFunctions *gl, PlatformState *input, F64 frametime_in_s);  // NOTE(tbt): called every frame
typedef void ( *GameAudioCallback) (void *buffer, U32 buffer_size);                                    // NOTE(tbt): called from the audio thread when the buffer needs refilling
typedef void ( *GameCleanup) (OpenGLFunctions *opengl_functions);                                      // NOTE(tbt): called when the window is closed and the main loop exits

// NOTE(tbt): control for a lock to be used with the audio thread
void platform_get_audio_lock(void);
void platform_release_audio_lock(void);

void platform_set_vsync(B32 enabled);

typedef struct Job PlatformJobHandle; // NOTE(tbt): enqueueing work returns a handle to the job which allows you to wait untill it has finished

typedef void ( *JobFunction) (void *arg, U64 memory_size, void *memory); // NOTE(tbt): prototype for functions which can be pushed to a queue to be executed by a thread pool

//
// NOTE(tbt): `arg_size` bytes are first copied from `arg_buffer` on the main thread
//            before `work()` is called on the worker thread with a pointer to the
//            copy as an argument
//
PlatformJobHandle *platform_enqueue_job(JobFunction work, void *arg_buffer, U32 arg_size);
// NOTE(tbt): when the job terminates, it's memory is returned to the main thread
PlatformJobHandle *platform_enqueue_job_with_memory(JobFunction work, void *arg_buffer, U32 arg_size, U64 size);
// NOTE(tbt): halts the thread untill the job terminates, and returns it's memory if any was allocated
void *platform_wait_for_job(PlatformJobHandle *handle);

// NOTE(tbt): mutex locks in case global state needs to be shared with jobs in the work queue
typedef struct MutexLock MutexLock;
MutexLock *platform_allocate_mutex(void);
void platform_lock_mutex(MutexLock *lock);
void platform_unlock_mutex(MutexLock *lock);

#endif

