#include <windows.h>
#include <windowsx.h>
#include <dsound.h>

#include <stdio.h>
#include <assert.h>

#include "lucerna_common.c"

#include "wglext.h"

internal MemoryArena global_platform_layer_frame_memory;

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz) arena_push(&global_platform_layer_frame_memory, sz)
internal void *
realloc_for_stb(void *p,
                U64 old_size,
                U64 new_size)
{
 void *result = arena_push(&global_platform_layer_frame_memory, new_size);
 memcpy(result, p, old_size);
 return result;
}
#define STBI_REALLOC_SIZED(p, old_size, new_size) realloc_for_stb(p, old_size, new_size)
#define STBI_FREE(p) do {} while (0)
#include "stb_image.h"

internal void
windows_print_error(U8 *function)
{
#ifdef LUCERNA_DEBUG
 void *message_buffer;
 DWORD error = GetLastError(); 
 
 FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                error,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR) &message_buffer,
                0, 
                NULL);
 
 debug_log("%s : %s", function, (char *)message_buffer);
 
 LocalFree(message_buffer);
#endif
}

//
// NOTE(tbt): file IO
//~

struct PlatformFile
{
 HANDLE file;
#ifdef LUCERNA_DEBUG
 U8 *name;
#endif
};

PlatformFile *
platform_open_file_ex(S8 path,
                      PlatformOpenFileFlags flags)
{
 PlatformFile *result = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PlatformFile));
 if (result)
 {
  arena_temporary_memory(&global_platform_layer_frame_memory)
  {
   U8 *path_cstr = cstring_from_s8(&global_platform_layer_frame_memory, path);
   
   DWORD desired_access =
    (GENERIC_READ * !!(flags & PLATFORM_OPEN_FILE_read)) |
    (GENERIC_WRITE * !!(flags & PLATFORM_OPEN_FILE_write));
   
   DWORD share_mode = 0;
   SECURITY_ATTRIBUTES security_attributes =
   {
    (DWORD)sizeof(SECURITY_ATTRIBUTES),
    0,
    0,
   };
   
   DWORD creation_disposition = 0;
   if (flags & PLATFORM_OPEN_FILE_always_create)
   {
    creation_disposition |= CREATE_ALWAYS;
   }
   else if (flags & PLATFORM_OPEN_FILE_never_create)
   {
    creation_disposition |= OPEN_EXISTING;
   }
   else
   {
    creation_disposition |= OPEN_ALWAYS;
   }
   
   DWORD flags_and_attributes = 0;
   HANDLE template_file = 0;
   
   result->file = CreateFileA(path_cstr,
                              desired_access,
                              share_mode,
                              &security_attributes,
                              creation_disposition,
                              flags_and_attributes,
                              template_file);
   
   if (result->file == INVALID_HANDLE_VALUE)
   {
    debug_log("failure opening file '%.*s' - ", unravel_s8(path));
    windows_print_error("CreateFileA");
    platform_close_file(&result);
   }
   else
   {
#ifdef LUCERNA_DEBUG
    result->name = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, path.size + 1);
    memcpy(result->name, path.buffer, path.size);
#endif
   }
  }
 }
 
 return result;
}

PlatformFile *
platform_open_file(S8 path)
{
 return platform_open_file_ex(path, PLATFORM_OPEN_FILE_read | PLATFORM_OPEN_FILE_write);
}

void
platform_close_file(PlatformFile **file)
{
 if (file &&
     *file)
 {
  CloseHandle((*file)->file);
#ifdef LUCERNA_DEBUG
  HeapFree(GetProcessHeap(), 0, (*file)->name);
#endif
  HeapFree(GetProcessHeap(), 0, *file);
  *file = NULL;
 }
}

U64
platform_get_file_size_f(PlatformFile *file)
{
 U64 result = 0;
 if (file)
 {
  result = GetFileSize(file->file, 0);
 }
 return result;
}

U64
platform_get_file_modified_time_f(PlatformFile *file)
{
 U64 result = 0;
 FILETIME file_modified_time;
 
 if (file)
 {
  if (GetFileTime(file->file, NULL, NULL, &file_modified_time))
  {
   ULARGE_INTEGER u_large_integer = {0};
   u_large_integer.LowPart = file_modified_time.dwLowDateTime;
   u_large_integer.HighPart = file_modified_time.dwHighDateTime;
   
   result = u_large_integer.QuadPart;
  }
  else
  {
   debug_log("failure getting last modified time for file '%s' - ", file->name);
   windows_print_error("GetFileTime");
  }
 }
 
 return result;
}

S8
platform_read_entire_file_f(MemoryArena *memory,
                            PlatformFile *file)
{
 S8 result = {0};
 
 if (file)
 {
  DWORD bytes_to_read = GetFileSize(file->file, 0);
  if (bytes_to_read)
  {
   void *read_data = arena_push(memory, bytes_to_read);
   DWORD bytes_read = 0;
   OVERLAPPED overlapped = {0};
   
   ReadFile(file->file, read_data, bytes_to_read, &bytes_read, &overlapped);
   
   result.buffer = read_data;
   result.size = (U64)bytes_read;
   
   if (bytes_to_read == bytes_read &&
       read_data &&
       bytes_read)
   {
    debug_log("successfully read entire file '%s'\n", file->name);
   }
   else
   {
    debug_log("failure reading entire file '%s' - ", file->name);
    windows_print_error("ReadFile");
   }
  }
  else
  {
   debug_log("failure reading entire file '%s' - file has no size\n", file->name);
  }
 }
 
 return result;
}

U64
platform_read_file_f(PlatformFile *file,
                     U64 offset,
                     U64 read_size,
                     void *buffer)
{
 DWORD bytes_read = 0;
 
 if (file &&
     buffer &&
     read_size)
 {
  OVERLAPPED overlapped = {0};
  overlapped.Pointer = (PVOID)offset;
  ReadFile(file->file, buffer, read_size, &bytes_read, &overlapped);
  
  if (!bytes_read)
  {
   debug_log("failure reading file '%s' - ", file->name);
   windows_print_error("ReadFile");
  }
  else
  {
   debug_log("successfully read fom file '%s'\n", file->name);
  }
 }
 return (U64)bytes_read;
}

U64
platform_write_to_file_f(PlatformFile *file,
                         void *buffer,
                         U64 buffer_size)
{
 DWORD bytes_written = 0;
 
 if (file &&
     buffer &&
     buffer_size)
 {
  void *data_to_write = buffer;
  DWORD data_to_write_size = (DWORD)buffer_size;
  
  if (0 == WriteFile(file->file, data_to_write, data_to_write_size, &bytes_written, 0))
  {
   debug_log("failure writing file '%s' - ", file->name);
   windows_print_error("WriteFile");
  }
  else
  {
   debug_log("successfully wrote to file '%s'\n", file->name);
  }
 }
 
 return bytes_written;
}

U64
platform_get_file_size_p(S8 path)
{
 PlatformFile *file = platform_open_file_ex(path,
                                            PLATFORM_OPEN_FILE_read | PLATFORM_OPEN_FILE_never_create);
 U64 result = platform_get_file_size_f(file);
 platform_close_file(&file);
 return result;
}

U64
platform_get_file_modified_time_p(S8 path)
{
 PlatformFile *file = platform_open_file_ex(path,
                                            PLATFORM_OPEN_FILE_read | PLATFORM_OPEN_FILE_never_create);
 U64 result = platform_get_file_modified_time_f(file);
 platform_close_file(&file);
 return result;
}

S8
platform_read_entire_file_p(MemoryArena *memory,
                            S8 path)
{
 PlatformFile *file = platform_open_file_ex(path,
                                            PLATFORM_OPEN_FILE_read | PLATFORM_OPEN_FILE_never_create);
 S8 result = platform_read_entire_file_f(memory, file);
 platform_close_file(&file);
 return result;
}

U64
platform_read_file_p(S8 path,
                     U64 offset,
                     U64 read_size,
                     void *buffer)
{
 PlatformFile *file = platform_open_file_ex(path,
                                            PLATFORM_OPEN_FILE_read | PLATFORM_OPEN_FILE_never_create);
 U64 result = platform_read_file_f(file, offset, read_size, buffer);
 platform_close_file(&file);
 return result;
}

U64
platform_write_entire_file_p(S8 path,
                             void *buffer,
                             U64 buffer_size)
{
 PlatformFile *file = platform_open_file_ex(path,
                                            PLATFORM_OPEN_FILE_read |
                                            PLATFORM_OPEN_FILE_write |
                                            PLATFORM_OPEN_FILE_always_create);
 U64 result = platform_write_to_file_f(file, buffer, buffer_size);
 platform_close_file(&file);
 return result;
}

U64
platform_append_to_file_p(S8 path,
                          void *buffer,
                          U64 buffer_size)
{
 PlatformFile *file = platform_open_file_ex(path, PLATFORM_OPEN_FILE_read | PLATFORM_OPEN_FILE_write);
 U64 result = platform_write_to_file_f(file, buffer, buffer_size);
 platform_close_file(&file);
 return result;
}

//
// NOTE(tbt): OpenGL loading
//~

internal struct
{
 PFNWGLSWAPINTERVALEXTPROC SwapIntervalEXT;
 PFNWGLCHOOSEPIXELFORMATARBPROC ChoosePixelFormatARB;
 PFNWGLCREATECONTEXTATTRIBSARBPROC CreateContextAttribsARB;
 PFNWGLGETEXTENSIONSSTRINGARBPROC GetExtensionsStringARB;
} wgl;

internal B32
windows_is_opengl_extension_present(HDC device_context,
                                    U8 *extension)
{
 const U8 *start;
 start = wgl.GetExtensionsStringARB(device_context);
 
 assert(start);
 
 for (;;)
 {
  U8 *at;
  U8 *terminator;
  
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
                             U8 *func)
{
 void *p;
 p = wglGetProcAddress(func);
 
 if(p == NULL       ||
    p == (void*)0x1 ||
    p == (void*)0x2 ||
    p == (void*)0x3 ||
    p == (void*)-1 )
 {
  debug_log("wglGetProcAddress returned NULL - "
            "trying to load '%s' from opengl32.dll... ",
            func);
  
  p = (void *)GetProcAddress(opengl32, func);
  if (p)
  {
   debug_log("Success!\n");
  }
  else
  {
   U8 message[512];
   snprintf(message, sizeof(message), "could not load OpenGL function '%s'\n", func);
   MessageBoxA(NULL, message, "Error", MB_OK | MB_ICONERROR);
   debug_log(message);
  }
 }
 
 return p;
}

internal void
windows_load_all_opengl_functions(OpenGLFunctions *result)
{
 HMODULE opengl32;
 opengl32 = LoadLibraryA("opengl32.dll");
#define gl_func(_type, _name) result-> ## _name = (PFNGL ## _type ## PROC)windows_load_opengl_function(opengl32, "gl" #_name)
#include "gl_funcs.h"
 
 FreeModule(opengl32);
}

PlatformState global_platform_state = {0};
internal volatile B32 global_running = true;

internal B32 global_dummy_context = true;
internal HWND global_window_handle;

internal GameInit game_init;
internal GameUpdateAndRender game_update_and_render;
internal GameAudioCallback game_audio_callback;
internal GameCleanup game_cleanup;

//
// NOTE(tbt): clipboard
//~

void
platform_set_clipboard_text(S8 text)
{
 if (OpenClipboard(global_window_handle))
 {
  if (EmptyClipboard())
  {
   HGLOBAL buffer_handle = GlobalAlloc(GMEM_MOVEABLE, text.size + 1);
   if (NULL != buffer_handle)
   {
    LPSTR buffer = GlobalLock(buffer_handle);
    memcpy(buffer, text.buffer, text.size);
    buffer[text.size] = 0; // NOTE(tbt): NULL terminate string
    GlobalUnlock(buffer_handle);
    
    SetClipboardData(CF_TEXT, buffer_handle);
   }
  }
  CloseClipboard();
 }
}

S8
platform_get_clipboard_text(MemoryArena *memory)
{
 S8 result = {0};
 
 if (OpenClipboard(global_window_handle))
 {
  HGLOBAL buffer_handle = GetClipboardData(CF_TEXT);
  if (NULL != buffer_handle)
  {
   LPSTR buffer = GlobalLock(buffer_handle);
   if (NULL != buffer)
   {
    result.size = calculate_utf8_cstring_size(buffer);
    result.buffer = arena_push(memory, result.size);
    memcpy(result.buffer, buffer, result.size);
   }
   GlobalUnlock(buffer_handle);
  }
  CloseClipboard();
 }
 
 return result;
}

//
// NOTE(tbt): platform layer utilities
//~

void
platform_set_vsync(B32 enabled)
{
 if (wgl.SwapIntervalEXT)
 {
  wgl.SwapIntervalEXT(enabled);
 }
}

internal WINDOWPLACEMENT global_previous_window_placement = { sizeof(global_previous_window_placement) };
void
platform_toggle_fullscreen(void)
{
 DWORD style = GetWindowLongA(global_window_handle, GWL_STYLE);
 if (style & WS_OVERLAPPEDWINDOW)
 {
  MONITORINFO monitor_info = { sizeof(monitor_info) };
  if (GetWindowPlacement(global_window_handle, &global_previous_window_placement) &&
      GetMonitorInfoA(MonitorFromWindow(global_window_handle, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
  {
   SetWindowLongA(global_window_handle,
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
  SetWindowLongA(global_window_handle,
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
 platform_audio_critical_section
 {
  global_running = false;
  PostQuitMessage(0);
 }
}

//
// NOTE(tbt): event processing loop
//~

internal void
windows_push_platform_event(PlatformEvent event)
{
 PlatformEvent *_event = arena_push(&global_platform_layer_frame_memory, sizeof(*_event));
 *_event = event;
 
 _event->next = global_platform_state.events;
 global_platform_state.events = _event;
}

#include "platform_events.h"

LRESULT CALLBACK
window_proc(HWND window_handle,
            UINT message,
            WPARAM w_param,
            LPARAM l_param)
{
 LRESULT result = 0;
 
 persist B32 mouse_hover_active = false;
 
 InputModifiers modifiers = 0;
 if(GetKeyState(VK_CONTROL) & 0x8000)
 {
  modifiers |= INPUT_MODIFIER_ctrl;
 }
 if(GetKeyState(VK_SHIFT) & 0x8000)
 {
  modifiers |= INPUT_MODIFIER_shift;
 }
 if(GetKeyState(VK_MENU) & 0x8000)
 {
  modifiers |= INPUT_MODIFIER_alt;
 }
 
 switch(message)
 {
  case WM_CHAR:
  {
   if (w_param >= 32 &&
       w_param != VK_RETURN &&
       w_param != VK_ESCAPE &&
       w_param != 127)
   {
    windows_push_platform_event(platform_key_typed_event(w_param));
   }
   break;
  }
  case WM_KEYDOWN:
  case WM_KEYUP:
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  {
   Key key_input = 0;
   B32 is_down = !(l_param & (1 << 31));;
   
   if(w_param >= 'A' && w_param <= 'Z')
   {
    key_input = KEY_a + (w_param - 'A');
   }
   else if (w_param >= '0' && w_param <= '9')
   {
    key_input = KEY_0 + (w_param - '0');
   }
   else
   {
    if(w_param == VK_ESCAPE)
    {
     key_input = KEY_esc;
    }
    else if(w_param >= VK_F1 && w_param <= VK_F12)
    {
     key_input = KEY_F1 + w_param - VK_F1;
    }
    else if(w_param == VK_OEM_3)
    {
     key_input = KEY_grave_accent;
    }
    else if(w_param == VK_OEM_MINUS)
    {
     key_input = KEY_minus;
    }
    else if(w_param == VK_OEM_PLUS)
    {
     key_input = KEY_equal;
    }
    else if(w_param == VK_BACK)
    {
     key_input = KEY_backspace;
    }
    else if(w_param == VK_TAB)
    {
     key_input = KEY_tab;
    }
    else if(w_param == VK_SPACE)
    {
     key_input = KEY_space;
    }
    else if(w_param == VK_RETURN)
    {
     key_input = KEY_enter;
    }
    else if(w_param == VK_CONTROL)
    {
     key_input = KEY_ctrl;
     modifiers &= ~INPUT_MODIFIER_ctrl;
    }
    else if(w_param == VK_SHIFT)
    {
     key_input = KEY_shift;
     modifiers &= ~INPUT_MODIFIER_shift;
    }
    else if(w_param == VK_MENU)
    {
     key_input = KEY_alt;
     modifiers &= ~INPUT_MODIFIER_alt;
    }
    else if(w_param == VK_UP)
    {
     key_input = KEY_up;
    }
    else if(w_param == VK_LEFT)
    {
     key_input = KEY_left;
    }
    else if(w_param == VK_DOWN)
    {
     key_input = KEY_down;
    }
    else if(w_param == VK_RIGHT)
    {
     key_input = KEY_right;
    }
    else if(w_param == VK_DELETE)
    {
     key_input = KEY_delete;
    }
    else if(w_param == VK_PRIOR)
    {
     key_input = KEY_page_up;
    }
    else if(w_param == VK_NEXT)
    {
     key_input = KEY_page_down;
    }
    else if(w_param == VK_HOME)
    {
     key_input = KEY_home;
    }
    else if(w_param == VK_END)
    {
     key_input = KEY_end;
    }
    else if(w_param == VK_OEM_2)
    {
     key_input = KEY_forward_slash;
    }
    else if(w_param == VK_OEM_PERIOD)
    {
     key_input = KEY_period;
    }
    else if(w_param == VK_OEM_COMMA)
    {
     key_input = KEY_comma;
    }
    else if(w_param == VK_OEM_7)
    {
     key_input = KEY_quote;
    }
    else if(w_param == VK_OEM_4)
    {
     key_input = KEY_left_bracket;
    }
    else if(w_param == VK_OEM_6)
    {
     key_input = KEY_right_bracket;
    }
   }
   
   global_platform_state.is_key_down[key_input] = is_down;
   
   if(is_down)
   {
    windows_push_platform_event(platform_key_press_event(key_input, modifiers));
   }
   else
   {
    windows_push_platform_event(platform_key_release_event(key_input, modifiers));
   }
   
   result = DefWindowProc(window_handle, message, w_param, l_param);
   
   break;
  }
  case WM_LBUTTONDOWN:
  {
   global_platform_state.is_mouse_button_down[MOUSE_BUTTON_left] = true;
   windows_push_platform_event(platform_mouse_press_event(MOUSE_BUTTON_left,
                                                          GET_X_LPARAM(l_param),
                                                          GET_Y_LPARAM(l_param),
                                                          modifiers));
   break;
  }
  case WM_LBUTTONUP:
  {
   global_platform_state.is_mouse_button_down[MOUSE_BUTTON_left] = false;
   windows_push_platform_event(platform_mouse_release_event(MOUSE_BUTTON_left,
                                                            GET_X_LPARAM(l_param),
                                                            GET_Y_LPARAM(l_param),
                                                            modifiers));
   break;
  }
  case WM_MBUTTONDOWN:
  {
   global_platform_state.is_mouse_button_down[MOUSE_BUTTON_middle] = true;
   windows_push_platform_event(platform_mouse_press_event(MOUSE_BUTTON_middle,
                                                          GET_X_LPARAM(l_param),
                                                          GET_Y_LPARAM(l_param),
                                                          modifiers));
   break;
  }
  case WM_MBUTTONUP:
  {
   global_platform_state.is_mouse_button_down[MOUSE_BUTTON_middle] = false;
   windows_push_platform_event(platform_mouse_release_event(MOUSE_BUTTON_middle,
                                                            GET_X_LPARAM(l_param),
                                                            GET_Y_LPARAM(l_param),
                                                            modifiers));
   break;
  }
  case WM_RBUTTONDOWN:
  {
   global_platform_state.is_mouse_button_down[MOUSE_BUTTON_right] = true;
   windows_push_platform_event(platform_mouse_press_event(MOUSE_BUTTON_right,
                                                          GET_X_LPARAM(l_param),
                                                          GET_Y_LPARAM(l_param),
                                                          modifiers));
   break;
  }
  case WM_RBUTTONUP:
  {
   global_platform_state.is_mouse_button_down[MOUSE_BUTTON_right] = false;
   windows_push_platform_event(platform_mouse_release_event(MOUSE_BUTTON_right,
                                                            GET_X_LPARAM(l_param),
                                                            GET_Y_LPARAM(l_param),
                                                            modifiers));
   break;
  }
  case WM_MOUSEWHEEL:
  {
   global_platform_state.mouse_scroll_v = GET_WHEEL_DELTA_WPARAM(w_param) / 120;
   windows_push_platform_event(platform_mouse_scroll_event(0,
                                                           global_platform_state.mouse_scroll_v,
                                                           global_platform_state.mouse_x,
                                                           global_platform_state.mouse_y,
                                                           modifiers));
   break;
  }
  case WM_MOUSEHWHEEL:
  {
   global_platform_state.mouse_scroll_h = GET_WHEEL_DELTA_WPARAM(w_param) / 120;
   windows_push_platform_event(platform_mouse_scroll_event(global_platform_state.mouse_scroll_h,
                                                           0,
                                                           global_platform_state.mouse_x,
                                                           global_platform_state.mouse_y,
                                                           modifiers));
   break;
  }
  case WM_MOUSEMOVE:
  {
   POINT mouse;
   GetCursorPos(&mouse);
   ScreenToClient(window_handle, &mouse);
   global_platform_state.mouse_x = mouse.x;
   global_platform_state.mouse_y = mouse.y;
   
   windows_push_platform_event(platform_mouse_move_event(global_platform_state.mouse_x,
                                                         global_platform_state.mouse_y));
   
   if (!mouse_hover_active)
   {
    mouse_hover_active = true;
    
    TRACKMOUSEEVENT track_mouse_event = {0};
    track_mouse_event.cbSize = sizeof(track_mouse_event);
    track_mouse_event.dwFlags = TME_LEAVE;
    track_mouse_event.hwndTrack = window_handle;
    track_mouse_event.dwHoverTime = HOVER_DEFAULT;
    
    TrackMouseEvent(&track_mouse_event);
   }
   break;
  }
  case WM_MOUSELEAVE:
  {
   mouse_hover_active = false;
   break;
  }
  case WM_SETCURSOR:
  {
   if (global_platform_state.mouse_x > 1 &&
       global_platform_state.mouse_x <= global_platform_state.window_w - 1 &&
       global_platform_state.mouse_y >= 1 &&
       global_platform_state.mouse_x <= global_platform_state.window_w - 1 &&
       mouse_hover_active)
   {
    SetCursor(LoadCursorA(0, IDC_ARROW));
   }
   else
   {
    result = DefWindowProc(window_handle, message, l_param, w_param);
   }
   break;
  }
  case WM_SIZE:
  {
   RECT client_rect;
   GetClientRect(window_handle, &client_rect);
   
   global_platform_state.window_w = client_rect.right - client_rect.left;
   global_platform_state.window_h = client_rect.bottom - client_rect.top;
   break;
  }
  case WM_CLOSE:
  case WM_DESTROY:
  case WM_QUIT:
  {
   if (global_dummy_context)
   {
    global_dummy_context = false;
   }
   else
   {
    platform_audio_critical_section global_running = false;
   }
   break;
  }
  case WM_KILLFOCUS:
  {
   U32 window_w = global_platform_state.window_w;
   U32 window_h = global_platform_state.window_h;
   
   ZeroMemory(&global_platform_state,
              sizeof(global_platform_state));
   
   global_platform_state.window_w = window_w;
   global_platform_state.window_h = window_h;
   
   result = DefWindowProc(window_handle, message, w_param, l_param);
   break;
  }
  default:
  {
   result = DefWindowProc(window_handle, message, w_param, l_param);
  }
 }
 
 return result;
}

//
// NOTE(tbt): audio
//~

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
   wave_format.nSamplesPerSec = AUDIO_SAMPLERATE;
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
     MessageBoxA(global_window_handle,
                 "Audio Error",
                 "could not set the primary buffer format",
                 MB_OK | MB_ICONWARNING);
     goto end;
    }
   }
   else
   {
    MessageBoxA(global_window_handle,
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
   secondary_buffer_desc.dwFlags = DSBCAPS_GLOBALFOCUS;
   secondary_buffer_desc.lpwfxFormat = &wave_format;
   
   if (SUCCEEDED(direct_sound->lpVtbl->CreateSoundBuffer(direct_sound,
                                                         &secondary_buffer_desc,
                                                         &secondary_buffer,
                                                         0)))
   {
   }
   else
   {
    MessageBoxA(global_window_handle,
                "Audio Error",
                "could not create the secondary buffer",
                MB_OK | MB_ICONWARNING);
    goto end;
   }
  }
  else
  {
   MessageBoxA(global_window_handle,
               "Audio Error",
               "could not set the DirectSound cooperative level",
               MB_OK | MB_ICONWARNING);
   goto end;
  }
 }
 else
 {
  MessageBoxA(global_window_handle,
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
    game_audio_callback(region_one, region_one_size);
    game_audio_callback(region_two, region_two_size);
    
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

//
// NOTE(tbt): entry point
//~

I32 WINAPI
WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR pCmdLine,
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
 
 HMODULE game = LoadLibraryA("lucerna.dll");
 
 game_init = (GameInit)GetProcAddress(game, "game_init");
 game_update_and_render = (GameUpdateAndRender)GetProcAddress(game, "game_update_and_render");
 game_audio_callback = (GameAudioCallback)GetProcAddress(game, "game_audio_callback");
 game_cleanup = (GameCleanup)GetProcAddress(game, "game_cleanup");
 
 assert(game_init);
 assert(game_update_and_render);
 assert(game_audio_callback);
 assert(game_cleanup);
 
 //
 // NOTE(tbt): icons ugggh..
 //~
 
 HICON icon = NULL;
 
 {
  I32 width, height, channels;
  U8 *pixels = stbi_load(ICON_PATH,
                         &width, &height, &channels,
                         4);
  
  if (pixels)
  {
   HDC dc = GetDC(NULL);
   
   union
   {
    BITMAPV5HEADER v5_header;
    BITMAPINFO info;
   } bitmap = {0};
   bitmap.v5_header.bV5Size = sizeof(bitmap.v5_header);
   bitmap.v5_header.bV5Width = width;
   bitmap.v5_header.bV5Height = -height;
   bitmap.v5_header.bV5Planes = 1;
   bitmap.v5_header.bV5BitCount = 32;
   bitmap.v5_header.bV5Compression = BI_BITFIELDS;
   bitmap.v5_header.bV5RedMask = 0x00ff0000;
   bitmap.v5_header.bV5GreenMask = 0x0000ff00;
   bitmap.v5_header.bV5BlueMask = 0x000000ff;
   bitmap.v5_header.bV5AlphaMask = 0xff000000;
   
   U8 *target;
   HBITMAP colour = CreateDIBSection(dc,
                                     &bitmap.info,
                                     DIB_RGB_COLORS,
                                     (void **)&target,
                                     NULL, 0);
   
   ReleaseDC(NULL, dc);
   
   if (colour)
   {
    HBITMAP mask = CreateBitmap(width, height, 1, 1, NULL);
    
    if (mask)
    {
     memcpy(target, pixels, width * height * 4);
     
     ICONINFO icon_info = {0};
     icon_info.fIcon    = true;
     icon_info.xHotspot = width / 2;
     icon_info.yHotspot = height / 2;
     icon_info.hbmMask  = mask;
     icon_info.hbmColor = colour;
     
     icon = CreateIconIndirect(&icon_info);
     
     DeleteObject(colour);
     DeleteObject(mask);
    }
   }
  }
 }
 
 //
 // NOTE(tbt): setup window and opengl context
 //~
 
 ZeroMemory(&window_class, sizeof(window_class));
 
 window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
 window_class.lpfnWndProc = window_proc;
 window_class.hInstance = hInstance;
 window_class.lpszClassName = "LUCERNA";
 window_class.hIcon = icon;
 
 RegisterClassA(&window_class);
 
 global_window_handle = CreateWindowA("LUCERNA",
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
  
  global_window_handle = CreateWindowA("LUCERNA",
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
  MessageBoxA(NULL, "Could not recreate OpenGL contex. Some graphical error may occur.", "Warning", MB_OK | MB_ICONWARNING);
  global_dummy_context = false;
 }
 
 ShowWindow(global_window_handle, SW_SHOW);
 
 windows_load_all_opengl_functions(&gl);
 
 platform_set_vsync(true);
 
 game_init(&gl);
 
 //
 // NOTE(tbt): setup audio thread
 //~
 
 InitializeCriticalSection(&global_audio_lock);
 CreateThread(NULL, 0, windows_audio_thread_main, NULL, 0, NULL);
 
 //
 // NOTE(tbt): main loop
 //~
 
 
 QueryPerformanceFrequency(&clock_frequency);
 
 LARGE_INTEGER start_time = {0}, end_time = {0};
 F64 frametime_in_s = 0.0;
 
 while (global_running)
 {
  MSG msg;
  
  QueryPerformanceCounter(&start_time);
  
  global_platform_state.mouse_scroll_h = 0;
  global_platform_state.mouse_scroll_v = 0;
  global_platform_state.events = NULL;
  
  if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
  {
   TranslateMessage(&msg);
   DispatchMessageA(&msg);
  }
  
  SwapBuffers(device_context);
  
  game_update_and_render(&global_platform_state, frametime_in_s);
  
  arena_free_all(&global_platform_layer_frame_memory);
  
  QueryPerformanceCounter(&end_time);
  frametime_in_s = (F64)(end_time.QuadPart - start_time.QuadPart) / (F64)clock_frequency.QuadPart;
 }
 
 game_cleanup();
 
 FreeModule(game);
}
