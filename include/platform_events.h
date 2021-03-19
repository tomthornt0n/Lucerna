
internal PlatformEvent
platform_key_press_event(Key key,
                         InputModifiers modifiers)
{
 PlatformEvent result = {0};
 
 result.kind = PLATFORM_EVENT_key_press;
 result.key = key;
 result.modifiers = modifiers;
 
 return result;
}

internal PlatformEvent
platform_key_release_event(Key key,
                           InputModifiers modifiers)
{
 PlatformEvent result = {0};
 
 result.kind = PLATFORM_EVENT_key_release;
 result.key = key;
 result.modifiers = modifiers;
 
 return result;
}

internal PlatformEvent
platform_key_typed_event(U64 character)
{
 PlatformEvent result = {0};
 
 result.kind = PLATFORM_EVENT_key_typed;
 result.character = character;
 
 return result;
}

internal PlatformEvent
platform_mouse_move_event(F32 mouse_x,
                          F32 mouse_y)
{
 PlatformEvent result = {0};
 
 result.kind = PLATFORM_EVENT_mouse_move;
 result.mouse_x = mouse_x;
 result.mouse_y = mouse_y;
 
 return result;
}

internal PlatformEvent
platform_mouse_press_event(MouseButton button,
                           F32 mouse_x, F32 mouse_y,
                           InputModifiers modifiers)
{
 PlatformEvent result = {0};
 
 result.kind = PLATFORM_EVENT_mouse_press;
 result.mouse_button = button;
 result.mouse_x = mouse_x;
 result.mouse_y = mouse_y;
 result.modifiers = modifiers;
 
 return result;
}

internal PlatformEvent
platform_mouse_release_event(MouseButton button,
                             F32 mouse_x, F32 mouse_y,
                             InputModifiers modifiers)
{
 PlatformEvent result = {0};
 
 result.kind = PLATFORM_EVENT_mouse_release;
 result.mouse_button = button;
 result.mouse_x = mouse_x;
 result.mouse_y = mouse_y;
 result.modifiers = modifiers;
 
 return result;
}

internal PlatformEvent
platform_mouse_scroll_event(I32 h_delta, I32 v_delta,
                            F32 mouse_x, F32 mouse_y,
                            InputModifiers modifiers)
{
 PlatformEvent result = {0};
 
 result.kind = PLATFORM_EVENT_mouse_scroll;
 result.mouse_scroll_h = h_delta;
 result.mouse_scroll_v = v_delta;
 result.mouse_x = mouse_x;
 result.mouse_y = mouse_y;
 result.modifiers = modifiers;
 
 return result;
}

internal PlatformEvent
platform_resize_event(U32 w,
                      U32 h)
{
 PlatformEvent result = {0};
 
 result.kind = PLATFORM_EVENT_window_resize;
 result.window_w = w;
 result.window_h = h;
 
 return result;
}
