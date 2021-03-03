#ifndef LUCERNA_H
#define LUCENRA_H

#include "glcorearb.h"
#include "KHR/khrplatform.h"
#include "stdint.h"

#define true  1
#define false 0

#define internal static

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

#include "errno.h"

#define KEY_none                0
#define KEY_a                   4
#define KEY_b                   5
#define KEY_c                   6
#define KEY_d                   7
#define KEY_e                   8
#define KEY_f                   9
#define KEY_g                   10
#define KEY_h                   11
#define KEY_i                   12
#define KEY_j                   13
#define KEY_k                   14
#define KEY_l                   15
#define KEY_m                   16
#define KEY_n                   17
#define KEY_o                   18
#define KEY_p                   19
#define KEY_q                   20
#define KEY_r                   21
#define KEY_s                   22
#define KEY_t                   23
#define KEY_u                   24
#define KEY_v                   25
#define KEY_w                   26
#define KEY_x                   27
#define KEY_y                   28
#define KEY_z                   29
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
#define KEY_escape              41
#define KEY_delete              42
#define KEY_tab                 43
#define KEY_space               44
#define KEY_minus               45
#define KEY_equals              46
#define KEY_left_bracket        47
#define KEY_right_bracket       48
#define KEY_backslash           49
#define KEY_semicolon           51
#define KEY_quote               52
#define KEY_grave               53
#define KEY_comma               54
#define KEY_period              55
#define KEY_slash               56
#define KEY_caps_lock           57
#define KEY_f1                  58
#define KEY_f2                  59
#define KEY_f3                  60
#define KEY_f4                  61
#define KEY_f5                  62
#define KEY_f6                  63
#define KEY_f7                  64
#define KEY_f8                  65
#define KEY_f9                  66
#define KEY_f10                 67
#define KEY_f11                 68
#define KEY_f12                 69
#define KEY_print_screen        70
#define KEY_scroll_lock         71
#define KEY_pause               72
#define KEY_insert              73
#define KEY_home                74
#define KEY_pageup              75
#define KEY_del                 76
#define KEY_end                 77
#define KEY_page_down           78
#define KEY_right               79
#define KEY_left                80
#define KEY_down                81
#define KEY_up                  82
#define KEY_kp_num_lock         83
#define KEY_kp_divide           84
#define KEY_kp_multiply         85
#define KEY_kp_subtract         86
#define KEY_kp_add              87
#define KEY_kp_enter            88
#define KEY_kp_1                89
#define KEY_kp_2                90
#define KEY_kp_3                91
#define KEY_kp_4                92
#define KEY_kp_5                93
#define KEY_kp_6                94
#define KEY_kp_7                95
#define KEY_kp_8                96
#define KEY_kp_9                97
#define KEY_kp_0                98
#define KEY_kp_point            99
#define KEY_non_us_backslash    100
#define KEY_kp_equals           103
#define KEY_f13                 104
#define KEY_f14                 105
#define KEY_f15                 106
#define KEY_f16                 107
#define KEY_f17                 108
#define KEY_f18                 109
#define KEY_f19                 110
#define KEY_f20                 111
#define KEY_f21                 112
#define KEY_f22                 113
#define KEY_f23                 114
#define KEY_f24                 115
#define KEY_help                117
#define KEY_menu                118
#define KEY_mute                127
#define KEY_sys_req             154
#define KEY_return              158
#define KEY_kp_clear            216
#define KEY_kp_decimal          220
#define KEY_left_control        224
#define KEY_left_shift          225
#define KEY_left_alt            226
#define KEY_left_super          227
#define KEY_right_control       228
#define KEY_right_shift         229
#define KEY_right_alt           230
#define KEY_right_super         231

#define MOUSE_BUTTON_left       0
#define MOUSE_BUTTON_middle     1
#define MOUSE_BUTTON_right      2
#define MOUSE_BUTTON_4          3
#define MOUSE_BUTTON_5          4
#define MOUSE_BUTTON_6          5
#define MOUSE_BUTTON_7          6
#define MOUSE_BUTTON_8          7

#define ctrl(_char) ((_char) - 96)

#define PLATFORM_LAYER_FRAME_MEMORY_SIZE 1 * ONE_MB
#define PLATFORM_LAYER_STATIC_MEMORY_SIZE 1 * ONE_MB
#define WINDOW_TITLE "Lucerna"
#define DEFAULT_WINDOW_WIDTH 1920
#define DEFAULT_WINDOW_HEIGHT 1040

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
 U32 window_w, window_h;
} PlatformState;

internal B32
is_key_typed(PlatformState *input,
             U8 key)
{
 for (KeyTyped *key_typed = input->keys_typed;
      NULL != key_typed;
      key_typed = key_typed->next)
 {
  if (key_typed->key == key)
  {
   return true;
  }
 }
 return false;
}

// NOTE(tbt): functions loaded by the platform layer before the game begins
typedef struct
{
 PFNGLACTIVETEXTUREPROC           ActiveTexture;
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
 PFNGLDRAWARRAYSPROC              DrawArrays;
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
 PFNGLUNIFORM1IPROC               Uniform1i;
 PFNGLUNIFORM1FPROC               Uniform1f;
 PFNGLUNIFORM2FPROC               Uniform2f;
 PFNGLUSEPROGRAMPROC              UseProgram;
 PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer;
 PFNGLVIEWPORTPROC                Viewport;
} OpenGLFunctions;

// NOTE(tbt): used by both the game and platform layer
#include "../game/arena.c"
#include "../game/strings.c"

//
// NOTE(tbt): the functions called by the platform layer
//

typedef void ( *GameInit) (OpenGLFunctions *gl);                                                       // NOTE(tbt): called after the platform layer has finished setup - last thing before entering the main loop
typedef void ( *GameUpdateAndRender) (OpenGLFunctions *gl, PlatformState *input, F64 frametime_in_s);  // NOTE(tbt): called every frame
typedef void ( *GameAudioCallback) (void *buffer, U32 buffer_size);                                    // NOTE(tbt): called from the audio thread when the buffer needs refilling
typedef void ( *GameCleanup) (OpenGLFunctions *opengl_functions);                                      // NOTE(tbt): called when the window is closed and the main loop exits

//
// NOTE(tbt): functions in the platform layer called by the game
//

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#ifdef LC_GAME
#define LC_API __declspec(dllimport)
#else
#define LC_API __declspec(dllexport)
#endif
#else
#define LC_API 
#endif

// NOTE(tbt): signal to the platform layer to exit
LC_API void platform_quit(void);

// NOTE(tbt): control for a lock to be used with the audio thread
LC_API void platform_get_audio_lock(void);
LC_API void platform_release_audio_lock(void);

// NOTE(tbt): control visual settings
LC_API void platform_set_vsync(B32 enabled);
LC_API void platform_toggle_fullscreen(void);

// NOTE(tbt): basic file IO
typedef struct PlatformFile PlatformFile;

LC_API B32 platform_write_entire_file(S8 path, void *buffer, U64 size);
LC_API S8 platform_read_entire_file(MemoryArena *memory, S8 path);
LC_API PlatformFile *platform_open_file(S8 path);
LC_API B32 platform_append_to_file(PlatformFile *file, void *buffer, U64 size);
LC_API void platform_close_file(PlatformFile **file);

#endif

