/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 03 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>

#include "lucerna.h"

#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <X11/Xlib-xcb.h>
#include <GL/glx.h>
#include "glxext.h"

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#define GAME_LIBRARY_PATH "./liblucerna.so"

typedef void ( *PFNGLXDESTROYCONTEXTPROC) (Display *dpy, GLXContext ctx);
typedef const char *( *PFNGLXQUERYEXTENSIONSSTRINGPROC) (Display *dpy, int screen);
typedef void ( *PFNGLXSWAPBUFFERSPROC) (Display *dpy, GLXDrawable drawable);

internal B32 global_running = true;

internal U8 global_key_lut[256] =
{
 0,  41, 30, 31,  32,  33,  34,  35,  36,  37,  38,  39,  45,  46,  42,
 43, 20, 26, 8,   21,  23,  28,  24,  12,  18,  19,  47,  48,  158, 224,
 4,  22, 7,  9,   10,  11,  13,  14,  15,  51,  52,  53,  225, 49,  29,
 27, 6,  25, 5,   17,  16,  54,  55,  56,  229, 85,  226, 44,  57,  58,
 59, 60, 61, 62,  63,  64,  65,  66,  67,  83,  71,  95,  96,  97,  86,
 92, 93, 94, 87,  89,  90,  91,  98,  99,  0,   0,   100, 68,  69,  0,
 0,  0,  0,  0,   0,   0,   88,  228, 84,  154, 230, 0,   74,  82,  75,
 80, 79, 77, 81,  78,  73,  76,  0,   0,   0,   0,   0,   103, 0,   72,
 0,  0,  0,  0,   0,   227, 231, 0,   0,   0,   0,   0,   0,   0,   0,
 0,  0,  0,  0,   118, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
 0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
 0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
 0,  0,  0,  104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
 0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
 0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
 0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
 0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
 0
};

internal struct
{
 PFNGLXCHOOSEFBCONFIGPROC         ChooseFBConfig;
 PFNGLXCREATENEWCONTEXTPROC       CreateNewContext;
 PFNGLXCREATEWINDOWPROC           CreateWindow;
 PFNGLXDESTROYCONTEXTPROC         DestroyContext;
 PFNGLXDESTROYWINDOWPROC          DestroyWindow;
 PFNGLXGETFBCONFIGATTRIBPROC      GetFBConfigAttrib;
 PFNGLXMAKECONTEXTCURRENTPROC     MakeContextCurrent;
 PFNGLXQUERYEXTENSIONSSTRINGPROC  QueryExtensionsString;
 PFNGLXSWAPBUFFERSPROC            SwapBuffers;
 PFNGLXSWAPINTERVALEXTPROC        SwapIntervalEXT;
 PFNGLXSWAPINTERVALMESAPROC       SwapIntervalMESA;
 PFNGLXSWAPINTERVALSGIPROC        SwapIntervalSGI;
} glX;

GameAudioCallback global_game_audio_callback;

typedef struct
{
 xcb_generic_event_t *previous;
 xcb_generic_event_t *current;
 xcb_generic_event_t *next;
} LinuxEventQueue;

internal void
linux_update_event_queue(xcb_connection_t *connection,
                         LinuxEventQueue *queue)
{
 free(queue->previous);
 queue->previous = queue->current;
 queue->current = queue->next;
 queue->next = xcb_poll_for_queued_event(connection);
}

internal B32
linux_is_opengl_extension_present(Display *display,
                                  I32 screen,
                                  I8 *extension)
{
 const I8 *start;
 start = glX.QueryExtensionsString(display, screen);
 
 assert(start && "Error getting gl extensions string");
 
 for (;;)
 {
  I8 *at;
  I8 *terminator;
  
  at = strstr(start, extension);
  if (!at)
   return false;
  
  terminator = at + strlen(extension);
  if (at == start || *(at - 1) == ' ')
  {
   if (*terminator == ' ' || *terminator == '\0')
    break;
  }
  
  start = terminator;
 }
 
 return true;
}

internal void ( *linux_load_opengl_function(I8 *func))(void)
{
 void (*result)(void);
 
 result = glXGetProcAddressARB((const GLubyte *)func);
 
 /* NOTE(tbt): probably not very useful - glXGetProcAddress never returns a
               NULL pointer as it may be called without an active gl context
               , so has no way of knowing which functions are available.
               see https://dri.freedesktop.org/wiki/glXGetProcAddressNeverReturnsNULL/
 */
 
 if (!result) fprintf(stderr, "Could not load OpenGL function '%s'.\n", func);
 
 return result;
}

internal void
linux_load_all_opengl_functions(OpenGLFunctions *result)
{
 result->AttachShader            = (PFNGLATTACHSHADERPROC           )linux_load_opengl_function("glAttachShader");
 result->BindBuffer              = (PFNGLBINDBUFFERPROC             )linux_load_opengl_function("glBindBuffer");
 result->BindFramebuffer         = (PFNGLBINDFRAMEBUFFERPROC        )linux_load_opengl_function("glBindFramebuffer");
 result->BindTexture             = (PFNGLBINDTEXTUREPROC            )linux_load_opengl_function("glBindTexture");
 result->BindVertexArray         = (PFNGLBINDVERTEXARRAYPROC        )linux_load_opengl_function("glBindVertexArray");
 result->BlendFunc               = (PFNGLBLENDFUNCPROC              )linux_load_opengl_function("glBlendFunc");
 result->BlitFramebuffer         = (PFNGLBLITFRAMEBUFFERPROC        )linux_load_opengl_function("glBlitFramebuffer");
 result->BufferData              = (PFNGLBUFFERDATAPROC             )linux_load_opengl_function("glBufferData");
 result->BufferSubData           = (PFNGLBUFFERSUBDATAPROC          )linux_load_opengl_function("glBufferSubData");
 result->Clear                   = (PFNGLCLEARPROC                  )linux_load_opengl_function("glClear");
 result->ClearColor              = (PFNGLCLEARCOLORPROC             )linux_load_opengl_function("glClearColor");
 result->CompileShader           = (PFNGLCOMPILESHADERPROC          )linux_load_opengl_function("glCompileShader");
 result->CreateProgram           = (PFNGLCREATEPROGRAMPROC          )linux_load_opengl_function("glCreateProgram");
 result->CreateShader            = (PFNGLCREATESHADERPROC           )linux_load_opengl_function("glCreateShader");
 result->DeleteBuffers           = (PFNGLDELETEBUFFERSPROC          )linux_load_opengl_function("glDeleteBuffers");
 result->DeleteProgram           = (PFNGLDELETEPROGRAMPROC          )linux_load_opengl_function("glDeleteProgram");
 result->DeleteShader            = (PFNGLDELETESHADERPROC           )linux_load_opengl_function("glDeleteShader");
 result->DeleteTextures          = (PFNGLDELETETEXTURESPROC         )linux_load_opengl_function("glDeleteTextures");
 result->DeleteVertexArrays      = (PFNGLDELETEVERTEXARRAYSPROC     )linux_load_opengl_function("glDeleteVertexArrays");
 result->DetachShader            = (PFNGLDETACHSHADERPROC           )linux_load_opengl_function("glDetachShader");
 result->Disable                 = (PFNGLDISABLEPROC                )linux_load_opengl_function("glDisable");
 result->DrawElements            = (PFNGLDRAWELEMENTSPROC           )linux_load_opengl_function("glDrawElements");
 result->Enable                  = (PFNGLENABLEPROC                 )linux_load_opengl_function("glEnable");
 result->EnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)linux_load_opengl_function("glEnableVertexAttribArray");
 result->FramebufferTexture2D    = (PFNGLFRAMEBUFFERTEXTURE2DPROC   )linux_load_opengl_function("glFramebufferTexture2D");
 result->GenBuffers              = (PFNGLGENBUFFERSPROC             )linux_load_opengl_function("glGenBuffers");
 result->GenFramebuffers         = (PFNGLGENFRAMEBUFFERSPROC        )linux_load_opengl_function("glGenFramebuffers");
 result->GenTextures             = (PFNGLGENTEXTURESPROC            )linux_load_opengl_function("glGenTextures");
 result->GenVertexArrays         = (PFNGLGENVERTEXARRAYSPROC        )linux_load_opengl_function("glGenVertexArrays");
 result->GetError                = (PFNGLGETERRORPROC               )linux_load_opengl_function("glGetError");
 result->GetProgramiv            = (PFNGLGETPROGRAMIVPROC           )linux_load_opengl_function("glGetProgramiv");
 result->GetUniformLocation      = (PFNGLGETUNIFORMLOCATIONPROC     )linux_load_opengl_function("glGetUniformLocation");
 result->GetShaderInfoLog        = (PFNGLGETSHADERINFOLOGPROC       )linux_load_opengl_function("glGetShaderInfoLog");
 result->GetShaderiv             = (PFNGLGETSHADERIVPROC            )linux_load_opengl_function("glGetShaderiv");
 result->LinkProgram             = (PFNGLLINKPROGRAMPROC            )linux_load_opengl_function("glLinkProgram");
 result->Scissor                 = (PFNGLSCISSORPROC                )linux_load_opengl_function("glScissor");
 result->ShaderSource            = (PFNGLSHADERSOURCEPROC           )linux_load_opengl_function("glShaderSource");
 result->TexImage2D              = (PFNGLTEXIMAGE2DPROC             )linux_load_opengl_function("glTexImage2D");
 result->TexParameteri           = (PFNGLTEXPARAMETERIPROC          )linux_load_opengl_function("glTexParameteri");
 result->UniformMatrix4fv        = (PFNGLUNIFORMMATRIX4FVPROC       )linux_load_opengl_function("glUniformMatrix4fv");
 result->Uniform2f               = (PFNGLUNIFORM2FPROC              )linux_load_opengl_function("glUniform2f");
 result->UseProgram              = (PFNGLUSEPROGRAMPROC             )linux_load_opengl_function("glUseProgram");
 result->VertexAttribPointer     = (PFNGLVERTEXATTRIBPOINTERPROC    )linux_load_opengl_function("glVertexAttribPointer");
 result->Viewport                = (PFNGLVIEWPORTPROC               )linux_load_opengl_function("glViewport");
 
 glX.ChooseFBConfig         = (PFNGLXCHOOSEFBCONFIGPROC        )linux_load_opengl_function("glXChooseFBConfig");
 glX.CreateNewContext       = (PFNGLXCREATENEWCONTEXTPROC      )linux_load_opengl_function("glXCreateNewContext");
 glX.CreateWindow           = (PFNGLXCREATEWINDOWPROC          )linux_load_opengl_function("glXCreateWindow");
 glX.DestroyContext         = (PFNGLXDESTROYCONTEXTPROC        )linux_load_opengl_function("glXDestroyContext");
 glX.DestroyWindow          = (PFNGLXDESTROYWINDOWPROC         )linux_load_opengl_function("glXDestroyWindow");
 glX.GetFBConfigAttrib      = (PFNGLXGETFBCONFIGATTRIBPROC     )linux_load_opengl_function("glXGetFBConfigAttrib");
 glX.MakeContextCurrent     = (PFNGLXMAKECONTEXTCURRENTPROC    )linux_load_opengl_function("glXMakeContextCurrent");
 glX.QueryExtensionsString  = (PFNGLXQUERYEXTENSIONSSTRINGPROC )linux_load_opengl_function("glXQueryExtensionsString");
 glX.SwapBuffers            = (PFNGLXSWAPBUFFERSPROC           )linux_load_opengl_function("glXSwapBuffers");
 glX.SwapIntervalEXT        = (PFNGLXSWAPINTERVALEXTPROC       )linux_load_opengl_function("glXSwapIntervalEXT");
 glX.SwapIntervalMESA       = (PFNGLXSWAPINTERVALMESAPROC      )linux_load_opengl_function("glXSwapIntervalMESA");
 glX.SwapIntervalSGI        = (PFNGLXSWAPINTERVALSGIPROC       )linux_load_opengl_function("glXSwapIntervalSGI");
}

internal void
linux_set_vsync(Display *display,
                I32 screen_id,
                GLXDrawable drawable,
                B32 enabled)
{
 if (linux_is_opengl_extension_present(display,
                                       screen_id,
                                       "GLX_EXT_swap_control"))
 {
  glX.SwapIntervalEXT(display, drawable, enabled);
 }
 else if (linux_is_opengl_extension_present(display,
                                            screen_id,
                                            "GLX_SGI_swap_control"))
 {
  glX.SwapIntervalSGI(enabled);
 }
 else if (linux_is_opengl_extension_present(display,
                                            screen_id,
                                            "GLX_MESA_swap_control"))
 {
  glX.SwapIntervalMESA(enabled);
 }
 else
 {
  fprintf(stderr, "could not set vsync\n");
 }
}

pthread_mutex_t global_audio_lock = PTHREAD_MUTEX_INITIALIZER;

void
platform_get_audio_lock(void)
{
 pthread_mutex_lock(&global_audio_lock);
}

void
platform_release_audio_lock(void)
{
 pthread_mutex_unlock(&global_audio_lock);
}

internal void *
linux_audio_thread_main(void *arg)
{
 snd_pcm_t *handle;
 U8 *buffer[1024];
 U32 buffer_size = sizeof(buffer);
 
 assert(snd_pcm_open(&handle,
                     "default",
                     SND_PCM_STREAM_PLAYBACK,
                     0) >= 0);
 
 assert(snd_pcm_set_params(handle,
                           SND_PCM_FORMAT_S16_LE,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           2,
                           44100,
                           1,
                           10000) >= 0);
 
 
 snd_pcm_prepare(handle);
 
 snd_pcm_sframes_t frames;
 
 while (global_running)
 {
  global_game_audio_callback(buffer, buffer_size);
  
  snd_pcm_wait(handle,
               1000);
  
  frames = snd_pcm_writei(handle,
                          buffer,
                          buffer_size >> 2);
  
  if (frames == -EPIPE)
  {
#ifdef LUCERNA_DEBUG
   fprintf(stderr, "Audio buffer underrun occured!\n");
#endif
   snd_pcm_prepare(handle);
  }
  else if (frames < 0)
  {
   frames = snd_pcm_recover(handle, frames, 0);
  }
  
  if (frames < 0)
  {
#ifdef LUCERNA_DEBUG
   fprintf(stderr,
           "snd_pcm_writei failed: %s\n",
           snd_strerror(frames));
#endif
  }
 }
 
 snd_pcm_drop(handle);
 snd_pcm_drain(handle);
 snd_pcm_close(handle);
 
 return NULL;
}

Display *global_display;
GLXDrawable global_drawable;
I32 global_default_screen_id;
GLXContext global_context;
xcb_connection_t *global_connection; 
xcb_window_t global_window;

void
platform_set_vsync(B32 enabled)
{
 linux_set_vsync(global_display,
                 global_default_screen_id,
                 global_drawable,
                 enabled);
}

internal xcb_intern_atom_reply_t *
linux_get_atom_reply(I8 *atom_id)
{
 xcb_intern_atom_cookie_t cookie = xcb_intern_atom(global_connection,
                                                   0,
                                                   strlen(atom_id),
                                                   atom_id);
 xcb_flush(global_connection);
 
 return xcb_intern_atom_reply(global_connection, cookie, 0);
}

void
platform_set_fullscreen(B32 fullscreen)
{
 xcb_intern_atom_reply_t *wm_state_reply = linux_get_atom_reply("_NET_WM_STATE");
 xcb_intern_atom_reply_t *fullscreen_reply = linux_get_atom_reply("_NET_WM_STATE_FULLSCREEN");
 
 if (fullscreen)
 {
  xcb_change_property(global_connection,
                      XCB_PROP_MODE_REPLACE,
                      global_window,
                      wm_state_reply->atom,
                      XCB_ATOM_ATOM,
                      32,
                      1,
                      &(fullscreen_reply->atom));
 }
 else
 {
  xcb_delete_property(global_connection, global_window, wm_state_reply->atom);
 }
 
 free(fullscreen_reply);
 free(wm_state_reply);
 
 xcb_unmap_window(global_connection, global_window);
 xcb_map_window(global_connection, global_window);
}

void
platform_toggle_fullscreen(void)
{
 xcb_intern_atom_reply_t *wm_state_reply = linux_get_atom_reply("_NET_WM_STATE");
 xcb_intern_atom_reply_t *fullscreen_reply = linux_get_atom_reply("_NET_WM_STATE_FULLSCREEN");
 
 xcb_get_property_cookie_t current_fullscreen_cookie = xcb_get_property(global_connection,
                                                                        0,
                                                                        global_window,
                                                                        wm_state_reply->atom,
                                                                        XCB_ATOM_ATOM,
                                                                        0,
                                                                        sizeof(xcb_atom_t));
 
 xcb_get_property_reply_t *current_fullscreen_property = xcb_get_property_reply(global_connection, current_fullscreen_cookie, NULL);
 xcb_atom_t *current_atom_fullscreen = (xcb_atom_t *)xcb_get_property_value(current_fullscreen_property);
 
 B32 fullscreen = (*current_atom_fullscreen != fullscreen_reply->atom);
 
 if (fullscreen)
 {
  xcb_change_property(global_connection,
                      XCB_PROP_MODE_REPLACE,
                      global_window,
                      wm_state_reply->atom,
                      XCB_ATOM_ATOM,
                      32,
                      1,
                      &(fullscreen_reply->atom));
 }
 else
 {
  xcb_delete_property(global_connection, global_window, wm_state_reply->atom);
 }
 
 free(current_fullscreen_property);
 free(wm_state_reply);
 free(fullscreen_reply);
 
 xcb_unmap_window(global_connection, global_window);
 xcb_map_window(global_connection, global_window);
}

MemoryArena global_platform_static_memory;

B32
platform_write_entire_file(S8 path,
                           U8 *buffer,
                           U64 buffer_size)
{
 B32 success = false;
 
 temporary_memory_begin(&global_platform_static_memory);
 
 I8 *temp_path = arena_allocate(&global_platform_static_memory, path.len + 2);
 memcpy(temp_path, path.buffer, path.len);
 strcat(temp_path, "~");
 
 I32 fd = open(temp_path,
               O_CREAT | O_WRONLY,
               S_IRUSR | S_IWUSR);
 
 if (-1 != fd)
 {
  U64 bytes_written = write(fd, buffer, buffer_size);
  
  if (bytes_written == buffer_size)
  {
   // TODO(tbt): get rid of stdio.h
   rename(temp_path, cstring_from_s8(&global_platform_static_memory, path));
   success = true;
  }
  
  B32 file_closed_successfully = -1 != close(fd);
  success = success & file_closed_successfully;
 }
 
 temporary_memory_end(&global_platform_static_memory);
 
 return success;
}

S8
platform_read_entire_file(MemoryArena *memory,
                          S8 path)
{
 S8 result;
 result.buffer = NULL;
 result.len = 0;
 
 temporary_memory_begin(&global_platform_static_memory);
 
 I32 fd = open(cstring_from_s8(&global_platform_static_memory, path), O_RDONLY);
 
 temporary_memory_end(&global_platform_static_memory);
 
 if (-1 != fd)
 {
  U64 file_size = lseek(fd, 0, SEEK_END);
  
  if (-1 != file_size)
  {
   if (-1 != lseek(fd, 0, SEEK_SET))
   {
    U8 *buffer = arena_allocate(memory, file_size);
    
    ssize_t bytes_read = read(fd, buffer, file_size);
    if (bytes_read == file_size)
    {
     result.buffer = buffer;
     result.len = file_size;
    }
    else
    {
     fprintf(stderr, "read %ld bytes: %s\n", bytes_read, strerror(errno));
    }
   }
  }
  
  if (-1 == close(fd))
  {
   result.buffer = NULL;
   result.len = 0;
  }
 }
 
 return result;
}

I32
main(int argc,
     char **argv)
{
 I32 i;
 
 MemoryArena platform_layer_frame_memory;
 initialise_arena_with_new_memory(&platform_layer_frame_memory, PLATFORM_LAYER_FRAME_MEMORY_SIZE);
 initialise_arena_with_new_memory(&global_platform_static_memory, PLATFORM_LAYER_STATIC_MEMORY_SIZE);
 
 PlatformState input;
 OpenGLFunctions gl;
 
 U32 window_width = DEFAULT_WINDOW_WIDTH;
 U32 window_height = DEFAULT_WINDOW_HEIGHT;
 I8 *window_title = WINDOW_TITLE;
 
 xcb_atom_t window_close_atom;
 xcb_screen_t *screen;
 const xcb_setup_t *setup;
 xcb_screen_iterator_t screen_iterator;
 GLXFBConfig *framebuffer_configs;
 I32 framebuffer_config_count;
 GLXFBConfig framebuffer_config;
 I32 visual_attributes[] =
 {
  GLX_X_RENDERABLE, True,
  GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
  GLX_RENDER_TYPE, GLX_RGBA_BIT,
  GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
  GLX_RED_SIZE, 8,
  GLX_GREEN_SIZE, 8,
  GLX_BLUE_SIZE, 8,
  GLX_ALPHA_SIZE, 8,
  GLX_DEPTH_SIZE, 24,
  GLX_STENCIL_SIZE, 8,
  GLX_DOUBLEBUFFER, True,
  None
 };
 xcb_colormap_t colour_map;
 U32 event_mask = XCB_EVENT_MASK_EXPOSURE         |
  XCB_EVENT_MASK_KEY_PRESS        |
  XCB_EVENT_MASK_KEY_RELEASE      |
  XCB_EVENT_MASK_BUTTON_PRESS     |
  XCB_EVENT_MASK_BUTTON_RELEASE   |
  XCB_EVENT_MASK_POINTER_MOTION   |
  XCB_EVENT_MASK_ENTER_WINDOW     |
  XCB_EVENT_MASK_STRUCTURE_NOTIFY;
 U32 value_list[3];
 U32 value_mask = XCB_CW_EVENT_MASK |
  XCB_CW_COLORMAP;
 xcb_intern_atom_cookie_t wm_protocol_cookie;
 xcb_intern_atom_reply_t *wm_protocol_reply;
 xcb_intern_atom_cookie_t window_close_cookie;
 xcb_intern_atom_reply_t *window_close_reply;
 I32 visual_id;
 LinuxEventQueue event_queue;
 
 pthread_t audio_thread;
 
 void *game_library;
 GameInit game_init;
 GameCleanup game_cleanup;
 GameUpdateAndRender game_update_and_render;
 
 linux_load_all_opengl_functions(&gl);
 
 memset(&input, 0, sizeof(input));
 
 global_display = XOpenDisplay(0);
 assert(global_display && "could not open display");
 
 global_default_screen_id = DefaultScreen(global_display);
 
 global_connection = XGetXCBConnection(global_display);
 
 if (!global_connection)
 {
  XCloseDisplay(global_display);
  fprintf(stderr, "Can't get XCB connection from display\n");
  exit(-1);
 }
 
 XSetEventQueueOwner(global_display, XCBOwnsEventQueue);
 
 setup = xcb_get_setup(global_connection);
 screen_iterator = xcb_setup_roots_iterator(setup);
 
 for (i = global_default_screen_id;
      screen_iterator.rem && i > 0;
      --i, xcb_screen_next(&screen_iterator));
 
 screen = screen_iterator.data;
 
 framebuffer_configs = NULL;
 framebuffer_config_count = 0;
 
 framebuffer_configs = glX.ChooseFBConfig(global_display,
                                          global_default_screen_id,
                                          visual_attributes,
                                          &framebuffer_config_count);
 
 assert(framebuffer_configs && framebuffer_config_count > 0 &&
        "Error getting frame buffer configs");
 
 framebuffer_config = framebuffer_configs[0];
 
 glX.GetFBConfigAttrib(global_display,
                       framebuffer_config,
                       GLX_VISUAL_ID,
                       &visual_id);
 
 global_context = glX.CreateNewContext(global_display,
                                       framebuffer_config,
                                       GLX_RGBA_TYPE,
                                       0,
                                       True);
 
 assert(global_context && "OpenGL context creation failed");
 
 
 colour_map = xcb_generate_id(global_connection);
 xcb_create_colormap(global_connection,
                     XCB_COLORMAP_ALLOC_NONE,
                     colour_map,
                     screen->root,
                     visual_id);
 
 value_list[0] = event_mask;
 value_list[1] = colour_map;
 value_list[2] = 0;
 
 global_window = xcb_generate_id(global_connection);
 xcb_create_window(global_connection,
                   XCB_COPY_FROM_PARENT,
                   global_window,
                   screen->root,
                   0, 0,
                   window_width, window_height,
                   0,
                   XCB_WINDOW_CLASS_INPUT_OUTPUT,
                   visual_id,
                   value_mask,
                   value_list);
 
 wm_protocol_reply = linux_get_atom_reply("WM_PROTOCOLS");
 
 window_close_reply = linux_get_atom_reply("WM_DELETE_WINDOW");
 
 window_close_atom = window_close_reply->atom;
 
 xcb_change_property(global_connection,
                     XCB_PROP_MODE_REPLACE,
                     global_window,
                     wm_protocol_reply->atom,
                     XCB_ATOM_ATOM,
                     32,
                     1,
                     &window_close_atom);
 
 xcb_change_property(global_connection,
                     XCB_PROP_MODE_REPLACE,
                     global_window,
                     XCB_ATOM_WM_NAME,
                     XCB_ATOM_STRING,
                     8,
                     strlen(window_title),
                     window_title);
 
 xcb_map_window(global_connection, global_window);
 
 global_drawable = glX.CreateWindow(global_display,
                                    framebuffer_config,
                                    global_window,
                                    NULL);
 
 if (!global_window ||
     !global_drawable)
 {
  xcb_destroy_window(global_connection, global_window);
  glX.DestroyContext(global_display, global_context);
  
  fprintf(stderr, "Error creating OpenGL context.\n");
  exit(-1);
 }
 
 if (!glX.MakeContextCurrent(global_display,
                             global_drawable,
                             global_drawable,
                             global_context))
 {
  xcb_destroy_window(global_connection, global_window);
  glX.DestroyContext(global_display, global_context);
  
  fprintf(stderr, "Could not make context current.\n");
  exit(-1);
 }
 
 free(wm_protocol_reply);
 free(window_close_reply);
 free(framebuffer_configs);
 
 platform_set_vsync(true);
 
 game_library = dlopen(GAME_LIBRARY_PATH, RTLD_NOW | RTLD_GLOBAL);
 assert("could not open game library." && game_library);
 
 game_init = dlsym(game_library, "game_init");
 game_cleanup = dlsym(game_library, "game_cleanup");
 game_update_and_render = dlsym(game_library, "game_update_and_render");
 global_game_audio_callback = dlsym(game_library, "game_audio_callback");
 
 assert(game_init);
 assert(game_cleanup);
 assert(game_update_and_render);
 assert(global_game_audio_callback);
 
 pthread_create(&audio_thread,
                NULL,
                linux_audio_thread_main,
                NULL);
 
 game_init(&gl);
 
 event_queue.previous = NULL;
 event_queue.current = NULL;
 event_queue.next = NULL;
 
 struct timeval start_time, end_time;
 F64 frametime_in_s = 0.0f;
 
 while (global_running)
 {
  xcb_generic_event_t *event;
  
  gettimeofday(&start_time, NULL);
  
  input.mouse_scroll = 0;
  input.keys_typed = NULL;
  
  linux_update_event_queue(global_connection, &event_queue);
  event = event_queue.current;
  
  while (event)
  {
   switch(event->response_type & ~0x80)
   {
    case XCB_KEY_PRESS:
    {
     xcb_key_press_event_t *press;
     U8 key;
     
     press = (xcb_key_press_event_t *)event;
     
     // NOTE(tbt): assume evdev driver so subtract offset of 8
     key = global_key_lut[press->detail - 8];
     
     input.is_key_pressed[key] = true;
     
     // NOTE(tbt): use XLookupString from xlib to get unicode character from key event
     */
      XKeyEvent x_key_event;
     x_key_event.display = global_display;
     x_key_event.keycode = press->detail;
     x_key_event.state = press->state;
     
     I8 buffer[16];
     if (XLookupString(&x_key_event, buffer, 16, NULL, NULL))
     {
      KeyTyped *key_typed;
      key_typed = arena_allocate(&platform_layer_frame_memory,
                                 sizeof(*key_typed));
      // NOTE(tbt): only want ASCII for now, so only take first byte.
      // NOTE(tbt): this is a massive hack
      // TODO(tbt): proper unicode input
      key_typed->key = buffer[0];
      key_typed->next = input.keys_typed;
      input.keys_typed = key_typed;
     }
     
     break;
    }
    case XCB_KEY_RELEASE:
    {
     U32 key;
     xcb_key_press_event_t *release, *next;
     
     release = (xcb_key_press_event_t *)event;
     next = (xcb_key_press_event_t *)event_queue.next;
     
     if (event_queue.next &&
         (next->response_type & ~0x80) == XCB_KEY_PRESS &&
         next->time == release->time &&
         next->detail == release->detail)
     {
      /* NOTE(tbt): register repeat events as 'typed' but not
                    'pressed'
      */
      XKeyEvent x_key_event;
      x_key_event.display = global_display;
      x_key_event.keycode = release->detail;
      x_key_event.state = release->state;
      
      I8 buffer[16];
      if (XLookupString(&x_key_event, buffer, 16, NULL, NULL))
      {
       KeyTyped *key_typed;
       key_typed = arena_allocate(&platform_layer_frame_memory,
                                  sizeof(*key_typed));
       /* NOTE(tbt): only want ASCII for now, so only take
                     first byte.
       */
       key_typed->key = buffer[0];
       key_typed->next = input.keys_typed;
       input.keys_typed = key_typed;
      }
      
      /* NOTE(tbt): eat repeat events */
      linux_update_event_queue(global_connection, &event_queue);
      break;
     }
     
     /* HACK(tbt): assume evdev driver so subtract offset of 8 */
     key = global_key_lut[release->detail - 8];
     
     input.is_key_pressed[key] = false;
     
     break;
    }
    case XCB_BUTTON_PRESS:
    {
     U8 code;
     
     code = ((xcb_button_press_event_t *)event)->detail - 1;
     
     if (code == 3)
     {
      ++input.mouse_scroll;
     }
     else if (code == 4)
     {
      --input.mouse_scroll;
     }
     else
     {
      input.is_mouse_button_pressed[code] = true;
     }
     
     break;
    }
    case XCB_BUTTON_RELEASE:
    {
     U8 code;
     
     code = ((xcb_button_press_event_t *)event)->detail - 1;
     input.is_mouse_button_pressed[code] = false;
     
     break;
    }
    case XCB_ENTER_NOTIFY:
    {
     xcb_enter_notify_event_t *enter;
     enter = (xcb_enter_notify_event_t *)event;
     
     input.mouse_x = enter->event_x;
     input.mouse_y = enter->event_y;
     
     break;
    }
    case XCB_MOTION_NOTIFY:
    {
     xcb_motion_notify_event_t *motion;
     motion = (xcb_motion_notify_event_t *)event;
     
     input.mouse_x = motion->event_x;
     input.mouse_y = motion->event_y;
     
     break;
    }
    case XCB_CONFIGURE_NOTIFY:
    {
     xcb_configure_notify_event_t *config;
     config = (xcb_configure_notify_event_t *)event;
     
     if (input.window_width != config->width ||
         input.window_height != config->height)
     {
      input.window_width = config->width;
      input.window_height = config->height;
     }
     
     break;
    }
    case XCB_CLIENT_MESSAGE:
    {
     if (((xcb_client_message_event_t *)event)->data.data32[0] ==
         window_close_atom)
     {
      global_running = false;
     }
     
     break;
    }
    default:
    break;
   }
   
   linux_update_event_queue(global_connection, &event_queue);
   event = event_queue.current;
  }
  
  glX.SwapBuffers(global_display, global_drawable);
  game_update_and_render(&gl,
                         &input,
                         frametime_in_s);
  
  arena_free_all(&platform_layer_frame_memory);
  
  gettimeofday(&end_time, NULL);
  
  frametime_in_s = (F64)((end_time.tv_sec * 1000000   + end_time.tv_usec) -
                         (start_time.tv_sec * 1000000 + start_time.tv_usec)) *
   1e-6;
 }
 
 game_cleanup(&gl);
 
 glX.DestroyWindow(global_display, global_drawable);
 xcb_destroy_window(global_connection, global_window);
 glX.DestroyContext(global_display, global_context);
 XCloseDisplay(global_display);
}

