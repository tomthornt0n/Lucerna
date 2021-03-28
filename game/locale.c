typedef enum
{
#define locale(_locale) LOCALE_ ## _locale,
#include "locales.h"
 LOCALE_MAX,
} Locale;

typedef struct Font Font;
internal Font *
load_font(OpenGLFunctions *gl, MemoryArena *memory, S8 path, U32 size, U32 bake_begin, U32 bake_count);

typedef struct
{
 Locale locale;
 
 Font *normal_font;
 Font *title_font;
 
 F64 dialogue_seconds_per_character;
 
 S8 title;
 S8 play;
 S8 exit;
} LocaleConfig;

internal LocaleConfig global_current_locale_config = {0};

internal void
set_locale(OpenGLFunctions *gl,
           Locale locale)
{
 if (locale == LOCALE_en_gb)
 {
  global_current_locale_config.locale = LOCALE_en_gb;
  
  U32 font_bake_begin = 32;
  U32 font_bake_end = 255;
  
  global_current_locale_config.normal_font = load_font(gl,
                                                       &global_static_memory,
                                                       s8_literal("../assets/fonts/PlayfairDisplay-Regular.ttf"),
                                                       28,
                                                       font_bake_begin,
                                                       font_bake_end - font_bake_begin);
  global_current_locale_config.title_font = load_font(gl,
                                                      &global_static_memory,
                                                      s8_literal("../assets/fonts/PlayfairDisplay-Regular.ttf"),
                                                      72,
                                                      font_bake_begin,
                                                      font_bake_end - font_bake_begin);
  
  global_current_locale_config.dialogue_seconds_per_character = 0.2;
  
  global_current_locale_config.title = s8_from_cstring(&global_static_memory, "Lucerna");
  global_current_locale_config.play = s8_from_cstring(&global_static_memory, "Play");
  global_current_locale_config.exit = s8_from_cstring(&global_static_memory, "Exit");
 }
 else if (locale == LOCALE_fr)
 {
  global_current_locale_config.locale = LOCALE_fr;
  
  U32 font_bake_begin = 32;
  U32 font_bake_end = 255;
  
  global_current_locale_config.normal_font = load_font(gl,
                                                       &global_static_memory,
                                                       s8_literal("../assets/fonts/PlayfairDisplay-Regular.ttf"),
                                                       28,
                                                       font_bake_begin,
                                                       font_bake_end - font_bake_begin);
  global_current_locale_config.title_font = load_font(gl,
                                                      &global_static_memory,
                                                      s8_literal("../assets/fonts/PlayfairDisplay-Regular.ttf"),
                                                      72,
                                                      font_bake_begin,
                                                      font_bake_end - font_bake_begin);
  
  global_current_locale_config.dialogue_seconds_per_character = 0.2;
  
  global_current_locale_config.title = s8_from_cstring(&global_static_memory, "Lucerna");
  global_current_locale_config.play = s8_from_cstring(&global_static_memory, "Jouer");
  global_current_locale_config.exit = s8_from_cstring(&global_static_memory, "Sortir");
 }
 else if (locale == LOCALE_sc)
 {
  global_current_locale_config.locale = LOCALE_sc;
  
  U32 font_bake_begin = 0x4E00;
  U32 font_bake_end = 0x9FFF;
  
  global_current_locale_config.normal_font = load_font(gl,
                                                       &global_static_memory,
                                                       s8_literal("../assets/fonts/LiuJianMaoCao-Regular.ttf"),
                                                       28,
                                                       font_bake_begin,
                                                       font_bake_end - font_bake_begin);
  global_current_locale_config.title_font = load_font(gl,
                                                      &global_static_memory,
                                                      s8_literal("../assets/fonts/LiuJianMaoCao-Regular.ttf"),
                                                      72,
                                                      font_bake_begin,
                                                      font_bake_end - font_bake_begin);
  
  global_current_locale_config.dialogue_seconds_per_character = 0.5;
  
  global_current_locale_config.title = s8_from_cstring(&global_static_memory, "光");
  global_current_locale_config.play = s8_from_cstring(&global_static_memory, "玩");
  global_current_locale_config.exit = s8_from_cstring(&global_static_memory, "出口");
 }
}

internal S8
s8_from_locale(MemoryArena *memory,
               Locale locale)
{
 S8 string;
#define locale(_locale) if (locale == LOCALE_ ## _locale) { string = s8_literal(#_locale); }
#include "locales.h"
 
 return copy_s8(memory, string);
}