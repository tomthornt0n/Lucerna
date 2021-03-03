#include <windows.h>
#include <windowsx.h>
#include <dsound.h>

#include <stdio.h>
#include <assert.h>

#include "lucerna.h"
#include "wglext.h"

internal MemoryArena global_platform_layer_frame_memory;

internal void
windows_print_error(U8 *function)
{
 void *message_buffer;
 DWORD error = GetLastError(); 
 
 FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
               FORMAT_MESSAGE_FROM_SYSTEM |
               FORMAT_MESSAGE_IGNORE_INSERTS,
               NULL,
               error,
               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
               (LPTSTR) &message_buffer,
               0, 
               NULL);
 
 
 fprintf(stderr, "%s : %s", function, (char *)message_buffer);
 
 LocalFree(message_buffer);
}

internal HANDLE
windows_create_file(B32 *success,
                    DWORD creation_disposition,
                    U8 *path_cstr)
{
 HANDLE file;
 
 DWORD desired_access = GENERIC_READ | GENERIC_WRITE;
 DWORD share_mode = 0;
 SECURITY_ATTRIBUTES security_attributes =
 {
  (DWORD)sizeof(SECURITY_ATTRIBUTES),
  0,
  0,
 };
 DWORD flags_and_attributes = FILE_ATTRIBUTE_NORMAL;
 HANDLE template_file = 0;
 
 file = CreateFile(path_cstr,
                   desired_access,
                   share_mode,
                   &security_attributes,
                   creation_disposition,
                   flags_and_attributes,
                   template_file);
 
 *success = *success && (file != INVALID_HANDLE_VALUE);
 
 return file;
}

S8
platform_read_entire_file(MemoryArena *memory,
                          S8 path)
{
 S8 result = {0};
 B32 success = true;
 
 temporary_memory_begin(&global_platform_layer_frame_memory);
 
 char *path_cstr = cstring_from_s8(&global_platform_layer_frame_memory, path);
 
 HANDLE file = windows_create_file(&success, OPEN_EXISTING, path_cstr);
 
 if (success)
 {
  DWORD read_bytes = GetFileSize(file, 0);
  if(read_bytes)
  {
   void *read_data = arena_allocate(memory, read_bytes);
   DWORD bytes_read = 0;
   OVERLAPPED overlapped = {0};
   
   ReadFile(file, read_data, read_bytes, &bytes_read, &overlapped);
   
   result.buffer = read_data;
   result.len = (U64)bytes_read;
   
   if (read_bytes != bytes_read ||
       !read_data ||
       !bytes_read)
   {
    windows_print_error("ReadFile");
   }
  }
  CloseHandle(file);
 }
 else
 {
  fprintf(stderr, "failure reading entire file '%s' - ", path_cstr);
  windows_print_error("CreateFile");
 }
 
#if LUCERNA_DEBUG
 if (success)
 {
  fprintf(stderr, "successfully read entire file '%s'\n", path_cstr);
 }
#endif
 
 temporary_memory_end(&global_platform_layer_frame_memory);
 
 return result;
}

B32
platform_write_entire_file(S8 path,
                           void *buffer,
                           U64 size)
{
 HANDLE file = {0};
 B32 success = true;
 
 temporary_memory_begin(&global_platform_layer_frame_memory);
 
 I8 *temp_path = arena_allocate(&global_platform_layer_frame_memory, path.len + 2);
 memcpy(temp_path, path.buffer, path.len);
 strcat(temp_path, "~");
 
 I8 *path_cstr = cstring_from_s8(&global_platform_layer_frame_memory, path);
 
 file = windows_create_file(&success, CREATE_ALWAYS, temp_path);
 
 if(success)
 {
  void *data_to_write = buffer;
  DWORD data_to_write_size = (DWORD)size;
  DWORD bytes_written = 0;
  
  success = success && WriteFile(file, data_to_write, data_to_write_size, &bytes_written, 0);
  
  if (!success)
  {
   fprintf(stderr, "failure writing entire file '%s' - ", temp_path);
   windows_print_error("WriteFile");
  }
  
  success = success && CloseHandle(file);
  
  if (success)
  {
   if (!MoveFileEx(temp_path, path_cstr, MOVEFILE_REPLACE_EXISTING))
   {
    fprintf(stderr, "failure renaming file '%s' over '%s' - ", temp_path, path_cstr);
    windows_print_error("MoveFileEx");
   }
  }
  else
  {
   fprintf(stderr, "failure writing entire file '%s'", temp_path);
   windows_print_error("CloseHandle");
  }
 }
 else
 {
  fprintf(stderr, "failure writing entire file '%s' - ", temp_path);
  windows_print_error("CreateFile");
 }
 
#if LUCERNA_DEBUG
 if (success)
 {
  fprintf(stderr, "successfully wrote entire file '%s'\n", path_cstr);
 }
#endif
 
 temporary_memory_end(&global_platform_layer_frame_memory);
 return success;
}

struct PlatformFile
{
 HANDLE file;
#ifdef LUCERNA_DEBUG
 I8 *name;
#endif
};

PlatformFile *
platform_open_file(S8 path)
{
 PlatformFile *result = NULL;
 HANDLE file = {0};
 
 temporary_memory_begin(&global_platform_layer_frame_memory);
 
 I8 *path_cstr = cstring_from_s8(&global_platform_layer_frame_memory, path);
 
 DWORD desired_access = GENERIC_READ | GENERIC_WRITE;
 DWORD share_mode = 0;
 SECURITY_ATTRIBUTES security_attributes =
 {
  (DWORD)sizeof(SECURITY_ATTRIBUTES),
  0,
  0,
 };
 DWORD creation_disposition = OPEN_ALWAYS;
 DWORD flags_and_attributes = 0;
 HANDLE template_file = 0;
 
 if ((file = CreateFile(path_cstr,
                        desired_access,
                        share_mode,
                        &security_attributes,
                        creation_disposition,
                        flags_and_attributes,
                        template_file)) != INVALID_HANDLE_VALUE)
 {
  result = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PlatformFile));
  result->file = file;
 }
 
 
 temporary_memory_end(&global_platform_layer_frame_memory);
 
#ifdef LUCERNA_DEBUG
 if (result)
 {
  result->name = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, path.len + 1);
  memcpy(result->name, path.buffer, path.len);
  fprintf(stderr, "successfully opened file '%.*s'\n", (I32)path.len, path.buffer);
 }
 else
 {
  fprintf(stderr, "failure opening file '%.*s'\n", (I32)path.len, path.buffer);
 }
#endif
 
 return result;
}

B32 platform_append_to_file(PlatformFile *file,
                            void *buffer,
                            U64 size)
{
 DWORD bytes_written;
 B32 success = true;
 
 if (file && size && buffer)
 {
  success = WriteFile(file->file, buffer, size, &bytes_written, 0);
  success = success && (bytes_written == size);
  
  if (!success)
  {
#ifdef LUCERNA_DEBUG
   fprintf(stderr, "failure appending to file '%s' - ", file->name);
#endif
   windows_print_error("WriteFile");
  }
 }
 
 return success;
}

void
platform_close_file(PlatformFile **file)
{
 CloseHandle((*file)->file);
#ifdef LUCERNA_DEBUG
 fprintf(stderr, "Successfully closed file %s\n", (*file)->name);
 HeapFree(GetProcessHeap(), 0, (*file)->name);
#endif
 HeapFree(GetProcessHeap(), 0, *file);
 *file = NULL;
}

internal struct
{
 PFNWGLSWAPINTERVALEXTPROC         SwapIntervalEXT;
 PFNWGLCHOOSEPIXELFORMATARBPROC    ChoosePixelFormatARB;
 PFNWGLCREATECONTEXTATTRIBSARBPROC CreateContextAttribsARB;
 PFNWGLGETEXTENSIONSSTRINGARBPROC  GetExtensionsStringARB;
} wgl;

void
platform_set_vsync(B32 enabled)
{
 if (wgl.SwapIntervalEXT)
 {
  wgl.SwapIntervalEXT(enabled);
 }
}

internal B32
windows_is_opengl_extension_present(HDC device_context,
                                    I8 *extension)
{
 const I8 *start;
 start = wgl.GetExtensionsStringARB(device_context);
 
 assert(start);
 
 for (;;)
 {
  I8 *at;
  I8 *terminator;
  
  at = strstr(start, extension);
  if (!at)
  {
   return false;
  }
  
  terminator = at + strlen(extension);
  if (at == start || *(at - 1) == ' ')
  {
   if (*terminator == ' ' || *terminator == '\0')
   {
    break;
   }
  }
  
  start = terminator;
 }
 
 return true;
}

internal void *
windows_load_opengl_function(HMODULE opengl32,
                             I8 *func)
{
 void *p;
 p = wglGetProcAddress(func);
 
 if(p == NULL       ||
    p == (void*)0x1 ||
    p == (void*)0x2 ||
    p == (void*)0x3 ||
    p == (void*)-1 )
 {
  fprintf(stderr,
          "wglGetProcAddress returned NULL - "
          "trying to load '%s' from opengl32.dll... ",
          func);
  
  p = (void *)GetProcAddress(opengl32, func);
  if (p)
  {
   fprintf(stderr, "Success!\n");
  }
  else
  {
   fprintf(stderr, "Could not load OpenGL function: %s\n", func);
  }
 }
 
 return p;
}

internal void
windows_load_all_opengl_functions(OpenGLFunctions *result)
{
 HMODULE opengl32;
 opengl32 = LoadLibrary("opengl32.dll");
 
 result->ActiveTexture           = (PFNGLACTIVETEXTUREPROC          )windows_load_opengl_function(opengl32, "glActiveTexture");
 result->AttachShader            = (PFNGLATTACHSHADERPROC           )windows_load_opengl_function(opengl32, "glAttachShader");
 result->BindBuffer              = (PFNGLBINDBUFFERPROC             )windows_load_opengl_function(opengl32, "glBindBuffer");
 result->BindFramebuffer         = (PFNGLBINDFRAMEBUFFERPROC        )windows_load_opengl_function(opengl32, "glBindFramebuffer");
 result->BindTexture             = (PFNGLBINDTEXTUREPROC            )windows_load_opengl_function(opengl32, "glBindTexture");
 result->BindVertexArray         = (PFNGLBINDVERTEXARRAYPROC        )windows_load_opengl_function(opengl32, "glBindVertexArray");
 result->BlendFunc               = (PFNGLBLENDFUNCPROC              )windows_load_opengl_function(opengl32, "glBlendFunc");
 result->BlitFramebuffer         = (PFNGLBLITFRAMEBUFFERPROC        )windows_load_opengl_function(opengl32, "glBlitFramebuffer");
 result->BufferData              = (PFNGLBUFFERDATAPROC             )windows_load_opengl_function(opengl32, "glBufferData");
 result->BufferSubData           = (PFNGLBUFFERSUBDATAPROC          )windows_load_opengl_function(opengl32, "glBufferSubData");
 result->Clear                   = (PFNGLCLEARPROC                  )windows_load_opengl_function(opengl32, "glClear");
 result->ClearColor              = (PFNGLCLEARCOLORPROC             )windows_load_opengl_function(opengl32, "glClearColor");
 result->CompileShader           = (PFNGLCOMPILESHADERPROC          )windows_load_opengl_function(opengl32, "glCompileShader");
 result->CreateProgram           = (PFNGLCREATEPROGRAMPROC          )windows_load_opengl_function(opengl32, "glCreateProgram");
 result->CreateShader            = (PFNGLCREATESHADERPROC           )windows_load_opengl_function(opengl32, "glCreateShader");
 result->DeleteBuffers           = (PFNGLDELETEBUFFERSPROC          )windows_load_opengl_function(opengl32, "glDeleteBuffers");
 result->DeleteProgram           = (PFNGLDELETEPROGRAMPROC          )windows_load_opengl_function(opengl32, "glDeleteProgram");
 result->DeleteShader            = (PFNGLDELETESHADERPROC           )windows_load_opengl_function(opengl32, "glDeleteShader");
 result->DeleteTextures          = (PFNGLDELETETEXTURESPROC         )windows_load_opengl_function(opengl32, "glDeleteTextures");
 result->DeleteVertexArrays      = (PFNGLDELETEVERTEXARRAYSPROC     )windows_load_opengl_function(opengl32, "glDeleteVertexArrays");
 result->DetachShader            = (PFNGLDETACHSHADERPROC           )windows_load_opengl_function(opengl32, "glDetachShader");
 result->Disable                 = (PFNGLDISABLEPROC                )windows_load_opengl_function(opengl32, "glDisable");
 result->DrawArrays              = (PFNGLDRAWARRAYSPROC             )windows_load_opengl_function(opengl32, "glDrawArrays");
 result->DrawElements            = (PFNGLDRAWELEMENTSPROC           )windows_load_opengl_function(opengl32, "glDrawElements");
 result->Enable                  = (PFNGLENABLEPROC                 )windows_load_opengl_function(opengl32, "glEnable");
 result->EnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)windows_load_opengl_function(opengl32, "glEnableVertexAttribArray");
 result->FramebufferTexture2D    = (PFNGLFRAMEBUFFERTEXTURE2DPROC   )windows_load_opengl_function(opengl32, "glFramebufferTexture2D");
 result->GenBuffers              = (PFNGLGENBUFFERSPROC             )windows_load_opengl_function(opengl32, "glGenBuffers");
 result->GenFramebuffers         = (PFNGLGENFRAMEBUFFERSPROC        )windows_load_opengl_function(opengl32, "glGenFramebuffers");
 result->GenTextures             = (PFNGLGENTEXTURESPROC            )windows_load_opengl_function(opengl32, "glGenTextures");
 result->GenVertexArrays         = (PFNGLGENVERTEXARRAYSPROC        )windows_load_opengl_function(opengl32, "glGenVertexArrays");
 result->GetError                = (PFNGLGETERRORPROC               )windows_load_opengl_function(opengl32, "glGetError");
 result->GetProgramiv            = (PFNGLGETPROGRAMIVPROC           )windows_load_opengl_function(opengl32, "glGetProgramiv");
 result->GetUniformLocation      = (PFNGLGETUNIFORMLOCATIONPROC     )windows_load_opengl_function(opengl32, "glGetUniformLocation");
 result->GetShaderInfoLog        = (PFNGLGETSHADERINFOLOGPROC       )windows_load_opengl_function(opengl32, "glGetShaderInfoLog");
 result->GetShaderiv             = (PFNGLGETSHADERIVPROC            )windows_load_opengl_function(opengl32, "glGetShaderiv");
 result->LinkProgram             = (PFNGLLINKPROGRAMPROC            )windows_load_opengl_function(opengl32, "glLinkProgram");
 result->Scissor                 = (PFNGLSCISSORPROC                )windows_load_opengl_function(opengl32, "glScissor");
 result->ShaderSource            = (PFNGLSHADERSOURCEPROC           )windows_load_opengl_function(opengl32, "glShaderSource");
 result->TexImage2D              = (PFNGLTEXIMAGE2DPROC             )windows_load_opengl_function(opengl32, "glTexImage2D");
 result->TexParameteri           = (PFNGLTEXPARAMETERIPROC          )windows_load_opengl_function(opengl32, "glTexParameteri");
 result->UniformMatrix4fv        = (PFNGLUNIFORMMATRIX4FVPROC       )windows_load_opengl_function(opengl32, "glUniformMatrix4fv");
 result->Uniform1i               = (PFNGLUNIFORM1IPROC              )windows_load_opengl_function(opengl32, "glUniform1i");
 result->Uniform1f               = (PFNGLUNIFORM1FPROC              )windows_load_opengl_function(opengl32, "glUniform1f");
 result->Uniform2f               = (PFNGLUNIFORM2FPROC              )windows_load_opengl_function(opengl32, "glUniform2f");
 result->UseProgram              = (PFNGLUSEPROGRAMPROC             )windows_load_opengl_function(opengl32, "glUseProgram");
 result->VertexAttribPointer     = (PFNGLVERTEXATTRIBPOINTERPROC    )windows_load_opengl_function(opengl32, "glVertexAttribPointer");
 result->Viewport                = (PFNGLVIEWPORTPROC               )windows_load_opengl_function(opengl32, "glViewport");
 
 FreeModule(opengl32);
}

U8 key_lut[] =
{
 
 0, 41, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 45, 46, 42, 43, 20, 26, 8,
 21, 23, 28, 24, 12, 18, 19, 47, 48, 158, 224, 4, 22, 7, 9, 10, 11, 13, 14,
 15, 51, 52, 53, 225, 49, 29, 27, 6, 25, 5, 17, 16, 54, 55, 56, 229, 0, 226,
 0, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 72, 71, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 68, 69, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 228, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 70, 230, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 74,
 82, 75, 0, 80, 0, 79, 0, 77, 81, 78, 73, 76, 0, 0, 0, 0, 0, 0, 0, 227, 231,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

internal volatile B32 global_running = true;

PlatformState global_platform_state = {0};

internal B32 global_dummy_context = true;

internal HWND global_window_handle;

internal WINDOWPLACEMENT global_previous_window_placement = { sizeof(global_previous_window_placement) };

void
platform_toggle_fullscreen(void)
{
 DWORD style = GetWindowLong(global_window_handle, GWL_STYLE);
 if (style & WS_OVERLAPPEDWINDOW)
 {
  MONITORINFO monitor_info = { sizeof(monitor_info) };
  if (GetWindowPlacement(global_window_handle, &global_previous_window_placement) &&
      GetMonitorInfo(MonitorFromWindow(global_window_handle, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
  {
   SetWindowLong(global_window_handle,
                 GWL_STYLE,
                 style & ~WS_OVERLAPPEDWINDOW);
   
   SetWindowPos(global_window_handle, HWND_TOP,
                monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
 }
 else
 {
  SetWindowLong(global_window_handle,
                GWL_STYLE,
                style | WS_OVERLAPPEDWINDOW);
  SetWindowPlacement(global_window_handle, &global_previous_window_placement);
  SetWindowPos(global_window_handle, NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
 }
}

void
platform_quit(void)
{
 global_running = false;
}


LRESULT CALLBACK
window_proc(HWND windowHandle,
            UINT message,
            WPARAM wParam,
            LPARAM lParam)
{
 switch(message)
 {
  case WM_CHAR:
  {
   // NOTE(tbt): this is just a massive hack
   // TODO(tbt): proper unicode input
   KeyTyped *key_typed;
   key_typed = arena_allocate(&global_platform_layer_frame_memory,
                              sizeof(*key_typed));
   key_typed->key = (U8)wParam;
   key_typed->next = global_platform_state.keys_typed;
   global_platform_state.keys_typed = key_typed;
   
   break;
  }
  case WM_KEYDOWN:
  {
   U32 key = key_lut[((lParam >> 16) & 0x7f) |
                     ((lParam & (1 << 24))
                      != 0 ?
                      0x80 : 0)];
   
   global_platform_state.is_key_pressed[key] = true;
   
   break;
  }
  case WM_KEYUP:
  {
   U32 key = key_lut[((lParam >> 16) & 0x7f) |
                     ((lParam & (1 << 24))
                      != 0 ?
                      0x80 : 0)];
   
   global_platform_state.is_key_pressed[key] = false;
   
   break;
  }
  case WM_LBUTTONDOWN:
  {
   global_platform_state.is_mouse_button_pressed[MOUSE_BUTTON_left] = true;
   
   break;
  }
  case WM_LBUTTONUP:
  {
   global_platform_state.is_mouse_button_pressed[MOUSE_BUTTON_left] = false;
   
   break;
  }
  case WM_MBUTTONDOWN:
  {
   global_platform_state.is_mouse_button_pressed[MOUSE_BUTTON_middle] = true;
   
   break;
  }
  case WM_MBUTTONUP:
  {
   global_platform_state.is_mouse_button_pressed[MOUSE_BUTTON_middle] = false;
   
   break;
  }
  case WM_RBUTTONDOWN:
  {
   global_platform_state.is_mouse_button_pressed[MOUSE_BUTTON_right] = true;
   
   break;
  }
  case WM_RBUTTONUP:
  {
   global_platform_state.is_mouse_button_pressed[MOUSE_BUTTON_right] = false;
   
   break;
  }
  case WM_MOUSEWHEEL:
  {
   global_platform_state.mouse_scroll = GET_WHEEL_DELTA_WPARAM(wParam) / 120;
   
   break;
  }
  case WM_MOUSEMOVE:
  {
   global_platform_state.mouse_x = GET_X_LPARAM(lParam);
   global_platform_state.mouse_y = GET_Y_LPARAM(lParam);
   
   break;
  }
  case WM_SIZE:
  {
   
   global_platform_state.window_w = LOWORD(lParam);
   global_platform_state.window_h = HIWORD(lParam);
   break;
  }
  case WM_DESTROY:
  {
   if (global_dummy_context)
   {
    global_dummy_context = false;
   }
   else
   {
    global_running = false;
    PostQuitMessage(0);
   }
   break;
  }
  case WM_KILLFOCUS:
  {
   ZeroMemory(&global_platform_state,
              sizeof(global_platform_state) - 2 * sizeof(U32)); // NOTE(tbt): do not zero window width and height
  }
  default:
  {
   return DefWindowProc(windowHandle, message, wParam, lParam);
  }
 }
 
 return 0;
}


internal CRITICAL_SECTION global_audio_lock;

void
platform_get_audio_lock(void)
{
 EnterCriticalSection(&global_audio_lock);
}

void
platform_release_audio_lock(void)
{
 LeaveCriticalSection(&global_audio_lock);
}

internal DWORD WINAPI
windows_audio_thread_main(LPVOID arg)
{
 LPDIRECTSOUND direct_sound;
 U32 secondary_buffer_size = 88200;
 LPDIRECTSOUNDBUFFER secondary_buffer;
 
 GameAudioCallback _game_audio_callback = (GameAudioCallback)arg;
 
 platform_get_audio_lock();
 
 if (SUCCEEDED(DirectSoundCreate(0,
                                 &direct_sound,
                                 0)))
 {
  
  if (SUCCEEDED(direct_sound->lpVtbl->SetCooperativeLevel(direct_sound,
                                                          global_window_handle,
                                                          DSSCL_PRIORITY)))
  {
   WAVEFORMATEX wave_format;
   ZeroMemory(&wave_format, sizeof(wave_format));
   wave_format.wFormatTag = WAVE_FORMAT_PCM;
   wave_format.nChannels = 2;
   wave_format.nSamplesPerSec = 44100;
   wave_format.wBitsPerSample = 16;
   wave_format.nBlockAlign = (wave_format.wBitsPerSample *
                              wave_format.nChannels) / 8;
   wave_format.nAvgBytesPerSec = wave_format.nBlockAlign * wave_format.nSamplesPerSec;
   
   // NOTE(tbt): try to create the primary buffer
   DSBUFFERDESC primary_buffer_desc;
   ZeroMemory(&primary_buffer_desc, sizeof(primary_buffer_desc));
   primary_buffer_desc.dwSize = sizeof(primary_buffer_desc);
   primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
   
   LPDIRECTSOUNDBUFFER primary_buffer;
   if (SUCCEEDED(direct_sound->lpVtbl->CreateSoundBuffer(direct_sound,
                                                         &primary_buffer_desc,
                                                         &primary_buffer,
                                                         0)))
   {
    if (!SUCCEEDED(primary_buffer->lpVtbl->SetFormat(primary_buffer,
                                                     &wave_format)))
    {
     MessageBox(global_window_handle,
                "Audio Error",
                "could not set the primary buffer format",
                MB_OK | MB_ICONWARNING);
     goto end;
    }
   }
   else
   {
    MessageBox(global_window_handle,
               "Audio Error",
               "could not create the primary buffer",
               MB_OK | MB_ICONWARNING);
    goto end;
   }
   
   //NOTE(tbt): try to create a secondary buffer (the one we actualy write to)
   DSBUFFERDESC secondary_buffer_desc;
   ZeroMemory(&secondary_buffer_desc, sizeof(secondary_buffer_desc));
   secondary_buffer_desc.dwSize = sizeof(secondary_buffer_desc);
   secondary_buffer_desc.dwBufferBytes = secondary_buffer_size;
   secondary_buffer_desc.lpwfxFormat = &wave_format;
   
   if (SUCCEEDED(direct_sound->lpVtbl->CreateSoundBuffer(direct_sound,
                                                         &secondary_buffer_desc,
                                                         &secondary_buffer,
                                                         0)))
   {
   }
   else
   {
    MessageBox(global_window_handle,
               "Audio Error",
               "could not create the secondary buffer",
               MB_OK | MB_ICONWARNING);
    goto end;
   }
  }
  else
  {
   MessageBox(global_window_handle,
              "Audio Error",
              "could not set the DirectSound cooperative level",
              MB_OK | MB_ICONWARNING);
   goto end;
  }
 }
 else
 {
  MessageBox(global_window_handle,
             "Audio Error",
             "could not create a DirectSound object",
             MB_OK | MB_ICONWARNING);
  goto end;
 }
 
 U32 output_byte_index = 0;
 
 U32 latency_bytes = 44100;
 
 platform_release_audio_lock();
 
 secondary_buffer->lpVtbl->Play(secondary_buffer, 0, 0, DSBPLAY_LOOPING);
 
 while (global_running)
 {
  DWORD play_cursor, write_cursor, target_cursor;
  
  if (SUCCEEDED(secondary_buffer->lpVtbl->GetCurrentPosition(secondary_buffer,
                                                             &play_cursor,
                                                             &write_cursor)))
  {
   DWORD byte_to_lock, bytes_to_write;
   DWORD region_one_size, region_two_size;
   VOID *region_one, *region_two;
   
   byte_to_lock = output_byte_index % secondary_buffer_size;
   
   target_cursor = (play_cursor + latency_bytes) % secondary_buffer_size;
   
   if (byte_to_lock > target_cursor)
   {
    bytes_to_write = (secondary_buffer_size - byte_to_lock) + target_cursor;
   }
   else
   {
    bytes_to_write = target_cursor - byte_to_lock;
   }
   
   if (SUCCEEDED(secondary_buffer->lpVtbl->Lock(secondary_buffer,
                                                byte_to_lock,
                                                bytes_to_write,
                                                &region_one,
                                                &region_one_size,
                                                &region_two,
                                                &region_two_size,
                                                0)))
   {
    _game_audio_callback(region_one, region_one_size);
    _game_audio_callback(region_two, region_two_size);
    
    secondary_buffer->lpVtbl->Unlock(secondary_buffer,
                                     region_one,
                                     region_one_size,
                                     region_two,
                                     region_two_size);
    
    output_byte_index += bytes_to_write;
   }
  }
 }
 
 end:
 return 0;
}

I32 WINAPI
wWinMain(HINSTANCE hInstance,
         HINSTANCE hPrevInstance,
         PWSTR pCmdLine,
         I32 nCmdShow)
{
 OpenGLFunctions gl;
 
 WNDCLASS window_class;
 PIXELFORMATDESCRIPTOR pfd;
 I32  pixel_format;
 HMODULE opengl32;
 B32 recreate_context;
 HDC device_context;
 HGLRC render_context;
 LARGE_INTEGER clock_frequency;
 
 initialise_arena_with_new_memory(&global_platform_layer_frame_memory, PLATFORM_LAYER_FRAME_MEMORY_SIZE);
 
 //
 // NOTE(tbt): load game dll
 //~
 
 HMODULE game = LoadLibrary("lucerna.dll");
 
 GameInit _game_init = (GameInit)GetProcAddress(game, "game_init");
 GameUpdateAndRender _game_update_and_render = (GameUpdateAndRender)GetProcAddress(game, "game_update_and_render");
 GameAudioCallback _game_audio_callback = (GameAudioCallback)GetProcAddress(game, "game_audio_callback");
 GameCleanup _game_cleanup = (GameCleanup)GetProcAddress(game, "game_cleanup");
 
 assert(_game_init);
 assert(_game_update_and_render);
 assert(_game_audio_callback);
 assert(_game_cleanup);
 
#if defined LUCERNA_DEBUG
 AllocConsole();
 
 {
  HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
  if (stderr_handle != INVALID_HANDLE_VALUE)
  {
   DWORD output_mode;
   if (GetConsoleMode(stderr_handle, &output_mode))
   {
    output_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // NOTE(tbt): enable ANSI escape sequences
    SetConsoleMode(stderr_handle, output_mode);
   }
  }
 }
 
 {
  HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (stdout_handle != INVALID_HANDLE_VALUE)
  {
   DWORD output_mode;
   if (GetConsoleMode(stdout_handle, &output_mode))
   {
    output_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // NOTE(tbt): enable ANSI escape sequences
    SetConsoleMode(stdout_handle, output_mode);
   }
  }
 }
#endif
 
 //
 // NOTE(tbt): setup window and opengl context
 //~
 
 ZeroMemory(&window_class, sizeof(window_class));
 
 window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
 window_class.lpfnWndProc = window_proc;
 window_class.hInstance = hInstance;
 window_class.lpszClassName = "LUCERNA";
 
 RegisterClass(&window_class);
 
 global_window_handle = CreateWindow("LUCERNA",
                                     WINDOW_TITLE,
                                     WS_OVERLAPPEDWINDOW,
                                     CW_USEDEFAULT, CW_USEDEFAULT,
                                     DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                                     NULL,
                                     NULL,
                                     hInstance,
                                     NULL);
 
 device_context = GetDC(global_window_handle);
 
 ZeroMemory(&pfd, sizeof(pfd));
 
 pfd.nSize = sizeof(pfd);
 pfd.nVersion = 1;
 pfd.dwFlags =
  PFD_DRAW_TO_WINDOW |
  PFD_SUPPORT_OPENGL |
  PFD_DOUBLEBUFFER;
 pfd.iPixelType = PFD_TYPE_RGBA;
 pfd.cColorBits = 32;
 pfd.cDepthBits = 24;
 pfd.cStencilBits = 8;
 pfd.iLayerType = PFD_MAIN_PLANE;
 
 pixel_format = ChoosePixelFormat(device_context, &pfd);
 SetPixelFormat(device_context, pixel_format, &pfd);
 
 render_context = wglCreateContext(device_context);
 wglMakeCurrent(device_context, render_context);
 
 opengl32 = LoadLibrary("opengl32.dll");
 
 wgl.GetExtensionsStringARB = windows_load_opengl_function(opengl32, "wglGetExtensionsStringARB");
 
 recreate_context =
  windows_is_opengl_extension_present(device_context, "WGL_ARB_pixel_format") &&
  windows_is_opengl_extension_present(device_context, "WGL_ARB_create_context");
 
 if (windows_is_opengl_extension_present(device_context, "WGL_EXT_swap_control"))
 {
  wgl.SwapIntervalEXT = windows_load_opengl_function(opengl32, "wglSwapIntervalEXT");
 }
 else
 {
  fprintf(stderr, "warning - 'WGL_EXT_swap_control' is not supported.\n");
  wgl.SwapIntervalEXT = NULL;
 }
 
 wgl.ChoosePixelFormatARB = windows_load_opengl_function(opengl32, "wglChoosePixelFormatARB");
 
 wgl.CreateContextAttribsARB = windows_load_opengl_function(opengl32, "wglCreateContextAttribsARB");
 
 FreeModule(opengl32);
 
 //
 // NOTE(tbt): recreate the context if the correct extensions are supported
 //~
 
 if (recreate_context)
 {
  I32 pixel_format_arb;
  I32 pixel_attributes[] = 
  {
   WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
   WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
   WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
   WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
   WGL_COLOR_BITS_ARB,     32,
   WGL_DEPTH_BITS_ARB,     24,
   WGL_STENCIL_BITS_ARB,   8,
   0
  };
  UINT pixel_format_count;
  BOOL choose_pixel_format_result;
  GLint context_attributes[] = 
  {
   WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
   WGL_CONTEXT_MINOR_VERSION_ARB, 3,
   WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_DEBUG_BIT_ARB,
   WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
   0,
  };
  
  wglMakeCurrent(NULL, NULL);
  wglDeleteContext(render_context);
  ReleaseDC(global_window_handle, device_context);
  DestroyWindow(global_window_handle);
  
  global_window_handle = CreateWindow("LUCERNA",
                                      WINDOW_TITLE,
                                      WS_OVERLAPPEDWINDOW,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                                      NULL,
                                      NULL,
                                      hInstance,
                                      NULL);
  
  device_context = GetDC(global_window_handle);
  
  choose_pixel_format_result = wgl.ChoosePixelFormatARB(device_context,
                                                        pixel_attributes,
                                                        NULL,
                                                        1,
                                                        &pixel_format_arb,
                                                        &pixel_format_count);
  
  assert(choose_pixel_format_result == TRUE && pixel_format_count > 0);
  
  SetPixelFormat(device_context, pixel_format_arb, &pfd);
  
  
  render_context = wgl.CreateContextAttribsARB(device_context,
                                               0,
                                               context_attributes);
  
  wglMakeCurrent(NULL, NULL);
  wglMakeCurrent(device_context, render_context);
 }
 else
 {
  global_dummy_context = false;
 }
 
 ShowWindow(global_window_handle, SW_SHOW);
 
 windows_load_all_opengl_functions(&gl);
 
 platform_set_vsync(true);
 
 //
 // NOTE(tbt): setup audio thread
 //~
 
 InitializeCriticalSection(&global_audio_lock);
 CreateThread(NULL, 0, windows_audio_thread_main, _game_audio_callback, 0, NULL);
 
 //
 // NOTE(tbt): main loop
 //~
 
 _game_init(&gl);
 
 QueryPerformanceFrequency(&clock_frequency);
 
 LARGE_INTEGER start_time = {0}, end_time = {0};
 F64 frametime_in_s = 0.0;
 
 while (global_running)
 {
  MSG msg;
  
  QueryPerformanceCounter(&start_time);
  
  global_platform_state.mouse_scroll = 0;
  global_platform_state.keys_typed = NULL;
  
  if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
  {
   TranslateMessage(&msg);
   DispatchMessage(&msg);
  }
  
  SwapBuffers(device_context);
  
  _game_update_and_render(&gl, &global_platform_state, frametime_in_s);
  
  arena_free_all(&global_platform_layer_frame_memory);
  
  QueryPerformanceCounter(&end_time);
  frametime_in_s = (F64)(end_time.QuadPart - start_time.QuadPart) / (F64)clock_frequency.QuadPart;
 }
 
 _game_cleanup(&gl);
 
 FreeModule(game);
}
