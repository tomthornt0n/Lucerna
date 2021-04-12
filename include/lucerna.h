#ifndef LUCERNA_H
#define LUCERNA_H

#include "glcorearb.h"
#include "KHR/khrplatform.h"
#include "stdint.h"

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

//
// NOTE(tbt): game code shared with the platform layer
//~

internal U32 calculate_utf8_cstring_size(U8 *cstring);
#include "../game/arena.c"
#include "../game/util.c"
#include "../game/strings.c"

//
// NOTE(tbt): keyboard input symbols
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
typedef void ( *GameAudioCallback) (void *buffer, U32 buffer_size);                                    // NOTE(tbt): called from the audio thread when the buffer needs refilling
typedef void ( *GameCleanup) (void);                                      // NOTE(tbt): called when the window is closed and the main loop exits

//
// NOTE(tbt): functions in the platform layer called by the game
//?

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

