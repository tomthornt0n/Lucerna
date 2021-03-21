internal Rect
_measure_and_draw_text(Font *font,
                       F32 x, F32 y,
                       F32 wrap_width,
                       Colour colour,
                       S8 text,
                       B32 centre_align)
{
 Rect result = _ui_measure_text(font, x, y, wrap_width, text);
 if (centre_align)
 {
  x -= result.w / 2.0f;
 }
 ui_draw_text(font, x, y, wrap_width, colour, text);
 
 return result;
}

#define MAIN_MENU_BUTTON_SHIFT_AMOUNT 16.0f
#define MAIN_MENU_BUTTON_SHIFT_SPEED 256.0f
#define MAIN_MENU_TEXT_COLOUR colour_literal(0.92f, 0.97f, 0.92f, 1.0f)
#define MAIN_MENU_BUTTON_REGION_TOLERANCE 16.0f

#define _MAIN_MENU_BUTTON(_text, _y, _selected_with_keyboard)                                                       \
Rect _button_bounds = _ui_measure_text(global_normal_font, global_renderer_window_w / 2.0f, (_y), -1.0f, (_text)); \
static B32 _hovered = false;                                                                                       \
static F32 _x_offset = 0.0f;                                                                                       \
if (is_point_in_region(input->mouse_x,                                                                             \
input->mouse_y,                                                                             \
rectangle_literal(_button_bounds.x - MAIN_MENU_BUTTON_REGION_TOLERANCE / 2.0f,              \
_button_bounds.y - MAIN_MENU_BUTTON_REGION_TOLERANCE / 2.0f,              \
_button_bounds.w + MAIN_MENU_BUTTON_REGION_TOLERANCE,                     \
_button_bounds.h + MAIN_MENU_BUTTON_REGION_TOLERANCE)) ||                 \
(_selected_with_keyboard))                                                                                     \
{                                                                                                                  \
if (!_hovered)                                                                                                    \
{                                                                                                                 \
_hovered = true;                                                                                                 \
play_audio_source(global_click_sound);                                                                           \
}                                                                                                                 \
_x_offset = min(_x_offset + frametime_in_s * MAIN_MENU_BUTTON_SHIFT_SPEED, MAIN_MENU_BUTTON_SHIFT_AMOUNT);        \
}                                                                                                                  \
else                                                                                                               \
{                                                                                                                  \
_hovered = false;                                                                                                 \
_x_offset = max(_x_offset - frametime_in_s * MAIN_MENU_BUTTON_SHIFT_SPEED, 0.0);                                  \
}                                                                                                                  \
ui_draw_text(global_normal_font,                                                                                   \
global_renderer_window_w / 2.0f - _button_bounds.w / 2.0f + _x_offset, (_y),                          \
-1.0f,                                                                                                \
MAIN_MENU_TEXT_COLOUR,                                                                                \
(_text));                                                                                             \
if (_hovered &&                                                                                                    \
(input->is_mouse_button_down[MOUSE_BUTTON_left] ||                                                             \
is_key_pressed(input, KEY_enter, 0)))

internal void
do_main_menu(OpenGLFunctions *gl,
             PlatformState *input,
             F64 frametime_in_s)
{
 typedef enum
 {
  MAIN_MENU_BUTTON_NONE,
  
  MAIN_MENU_BUTTON_play,
  MAIN_MENU_BUTTON_exit,
  
  MAIN_MENU_BUTTON_MAX,
 } MainMenuButton;
 static MainMenuButton keyboard_selection = MAIN_MENU_BUTTON_NONE;
 
 if (is_key_pressed(input, KEY_tab, 0))
 {
  keyboard_selection = (keyboard_selection + 1) % MAIN_MENU_BUTTON_MAX;
 }
 
 _measure_and_draw_text(global_title_font,
                        global_renderer_window_w / 2.0f,
                        300.0f,
                        -1.0f,
                        colour_literal(1.0f, 1.0f, 1.0f, 1.0f),
                        s8_literal("Lucerna"),
                        true);
 
 
 {
  _MAIN_MENU_BUTTON(s8_literal("Play"), 400.0f, keyboard_selection == MAIN_MENU_BUTTON_play)
  {
   if (set_current_level(gl, s8_literal("../assets/levels/office_1.level"), false))
   {
    global_game_state = GAME_STATE_playing;
   }
   else
   {
    platform_quit();
   }
  }
 }
 
 {
  _MAIN_MENU_BUTTON(s8_literal("Exit"), 450.0f, keyboard_selection == MAIN_MENU_BUTTON_exit)
  {
   platform_quit();
  }
 }
}