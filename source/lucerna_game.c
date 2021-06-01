#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>

#define LUCERNA_GAME
#include "lucerna_common.c"

//-TODO:
//
//   - save game system
//   - make the player walk less weirdly
//   - second pass on menus
//   - cutscenes?
//   - asset packing for release builds?
//   - gameplay!!!!!!!
//
//~

#define gl_func(_type, _func) internal PFNGL ## _type ## PROC gl ## _func;
#include "gl_funcs.h"

internal MemoryArena global_static_memory;
internal MemoryArena global_frame_memory;
internal MemoryArena global_level_memory;
internal MemoryArena global_temp_memory;

//
// NOTE(tbt): libraries
//~

internal void *
malloc_for_stb(U64 size)
{
 return arena_push(&global_temp_memory, size);
}

internal void *
realloc_for_stb(void *p,
                U64 old_size,
                U64 new_size)
{
 void *result = arena_push(&global_temp_memory, new_size);
 memcpy(result, p, old_size);
 return result;
}

internal void
free_for_stb(void *p)
{
 return;
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz) malloc_for_stb(sz)
#define STBI_REALLOC_SIZED(p, old_size, new_size) realloc_for_stb(p, old_size, new_size)
#define STBI_FREE(p) free_for_stb(p)
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#define STBTT_malloc(x, u) ((void)(u),malloc_for_stb(x))
#define STBTT_free(x, u) ((void)(u),free_for_stb(x))
#include "stb_rect_pack.h"
#include "stb_truetype.h"

#include "cmixer.h"
#include "cmixer.c"

//
// NOTE(tbt): constants
//~

enum
{
 BATCH_SIZE = 1024,
 
 SHADER_INFO_LOG_MAX_LEN = 4096,
 
 SCREEN_W_IN_WORLD_UNITS = 1920,
 SCREEN_H_IN_WORLD_UNITS = 1080,
 
 BLUR_TEXTURE_W = SCREEN_W_IN_WORLD_UNITS / 2,
 BLUR_TEXTURE_H = SCREEN_H_IN_WORLD_UNITS / 2,
 
 UI_SORT_DEPTH = 128,
 
 MAX_ENTITIES = 120,
 
 CURRENT_LEVEL_PATH_BUFFER_SIZE = 64,
 
 ENTITY_STRING_BUFFER_SIZE = 64,
};

//
// NOTE(tbt): types
//~

typedef enum
{
#define locale(_locale) LOCALE_ ## _locale,
#include "locales.h"
 LOCALE_MAX,
} Locale;

typedef U32 TextureID;

typedef struct Texture Texture;
struct Texture
{
 Texture *next_loaded;
 S8 path;
 U64 last_modified;
 TextureID id;
 I32 width, height;
};

typedef struct
{
 F32 min_x, min_y;
 F32 max_x, max_y;
} SubTexture;

typedef struct
{
 Texture texture;
 I32 bake_begin, bake_end;
 stbtt_packedchar *char_data;
 I32 size;
 F32 vertical_advance;
} Font;

typedef U32 ShaderID;

typedef struct
{
 F32 x, y;
 F32 r, g, b, a;
 F32 u, v;
} Vertex;

typedef struct
{
 Vertex bl, br, tr, tl;
} Quad;

typedef struct
{
 Quad buffer[BATCH_SIZE];
 U64 quad_count;
 TextureID texture;
 ShaderID shader;
 F32 *projection_matrix;
 Rect mask;
 B32 in_use;
} RenderBatch;

typedef enum
{
 RENDER_MESSAGE_draw_rectangle,
 RENDER_MESSAGE_stroke_rectangle,
 RENDER_MESSAGE_draw_text,
 RENDER_MESSAGE_blur_screen_region,
 RENDER_MESSAGE_draw_gradient,
 RENDER_MESSAGE_do_post_processing,
} RenderMessageKind;

typedef struct
{
 U32 target;
 TextureID texture;
} Framebuffer;

typedef struct
{
 F64 time_playing;
 I32 characters_showing;
 B32 playing;
 F32 fade_out_transition;
 
 S8List *dialogue;
 F32 x, y;
 Colour colour;
} DialogueState;

typedef enum
{
 GAME_STATE_playing,
 GAME_STATE_editor,
 GAME_STATE_main_menu,
 
 GAME_STATE_MAX,
} GameState;

typedef I32 PostProcessingKind;
typedef enum
{
 POST_PROCESSING_KIND_world,
 POST_PROCESSING_KIND_memory,
} PostProcessingKind_ENUM;

typedef U64 EntityFlags;
typedef enum
{
 ENTITY_FLAG_marked_for_removal,
 ENTITY_FLAG_teleport,
 ENTITY_FLAG_trigger_dialogue,
 ENTITY_FLAG_draw_texture,
 ENTITY_FLAG_set_exposure,
 ENTITY_FLAG_set_player_scale,
 ENTITY_FLAG_set_floor_gradient,
 ENTITY_FLAG_set_post_processing_kind,
} EntityFlags_ENUM;

typedef U8 EntityTriggers;
typedef enum
{
 ENTITY_TRIGGER_player_entered,
 ENTITY_TRIGGER_player_intersecting,
 ENTITY_TRIGGER_player_left,
} EntityTriggers_ENUM;

typedef I32 SetExposureMode;
typedef enum
{
 SET_EXPOSURE_MODE_uniform,
 SET_EXPOSURE_MODE_fade_out_n,
 SET_EXPOSURE_MODE_fade_out_e,
 SET_EXPOSURE_MODE_fade_out_s,
 SET_EXPOSURE_MODE_fade_out_w,
} SetExposureMode_ENUM;

//-
// NOTE(tbt): update serialisation code when changing this struct
//-
#define ENTITY_SERIALISATION_VERSION 1
typedef struct Entity Entity;
struct Entity
{
 Entity *next;
 Rect bounds;
 EntityFlags flags;
 EntityTriggers triggers;
 U8 editor_name_buffer[ENTITY_STRING_BUFFER_SIZE];
 S8 editor_name;
 
 // NOTE(tbt): ENTITY_FLAG_teleport
 U8 teleport_to_level_path_buffer[ENTITY_STRING_BUFFER_SIZE];
 S8 teleport_to_level_path;
 F32 teleport_to_x;
 F32 teleport_to_y;
 
 // NOTE(tbt): ENTITY_FLAG_trigger_dialogue
 B32 dialogue_repeat;
 B32 dialogue_played;
 F32 dialogue_x;
 F32 dialogue_y;
 U8 dialogue_path_buffer[ENTITY_STRING_BUFFER_SIZE];
 S8 dialogue_path;
 
 // NOTE(tbt): ENTITY_FLAG_draw_texture
 Texture *texture;
 B32 draw_texture_in_fg;
 
 // NOTE(tbt): ENTITY_FLAG_set_exposure
 SetExposureMode set_exposure_mode;
 F32 set_exposure_to;
 
 // NOTE(tbt): ENTITY_FLAG_set_player_scale
 F32 set_player_scale_to;
 
 // NOTE(tbt): ENTITY_FLAG_set_floor_gradient
 F32 set_floor_gradient_to;
 
 // NOTE(tbt): ENTITY_FLAG_set_post_processing_kind
 PostProcessingKind set_post_processing_kind_to;
};

#pragma pack(push, 1)

typedef struct
{
 Rect bounds;
 U8 texture_path[ENTITY_STRING_BUFFER_SIZE];
 B32 draw_texture_in_fg;
 EntityFlags flags;
 B32 dialogue_repeat;
 F32 dialogue_x;
 F32 dialogue_y;
 U8 dialogue_path[ENTITY_STRING_BUFFER_SIZE];
 SetExposureMode set_exposure_mode;
 F32 set_exposure_to;
 U8 teleport_to_level_path[ENTITY_STRING_BUFFER_SIZE];
 F32 teleport_to_x;
 F32 teleport_to_y;
 F32 set_player_scale_to;
 F32 set_floor_gradient_to;
 PostProcessingKind set_post_processing_kind_to;
 EntityTriggers set_post_processing_kind_on_trigger;
} Entity_SERIALISABLE_V0;

typedef struct
{
 U8 editor_name[ENTITY_STRING_BUFFER_SIZE];
 Rect bounds;
 U8 texture_path[ENTITY_STRING_BUFFER_SIZE];
 B32 draw_texture_in_fg;
 EntityFlags flags;
 B32 dialogue_repeat;
 F32 dialogue_x;
 F32 dialogue_y;
 U8 dialogue_path[ENTITY_STRING_BUFFER_SIZE];
 SetExposureMode set_exposure_mode;
 F32 set_exposure_to;
 U8 teleport_to_level_path[ENTITY_STRING_BUFFER_SIZE];
 F32 teleport_to_x;
 F32 teleport_to_y;
 F32 set_player_scale_to;
 F32 set_floor_gradient_to;
 PostProcessingKind set_post_processing_kind_to;
 EntityTriggers set_post_processing_kind_on_trigger;
} Entity_SERIALISABLE_V1;

#define Entity_SERIALISABLE macro_concatenate(Entity_SERIALISABLE_V, ENTITY_SERIALISATION_VERSION)

#pragma pack(pop)

typedef struct RenderMessage RenderMessage;
struct RenderMessage
{
 RenderMessage *next;
 RenderMessageKind kind;
 U8 sort;
 Rect mask;
 
 Rect rectangle;
 F32 *projection_matrix;
 Font *font;
 S8 string;
 TextureID texture;
 SubTexture sub_texture;
 F32 angle;
 F32 stroke_width;
 Colour colour;
 Gradient gradient;
 I32 strength;
 F32 exposure;
 PostProcessingKind post_processing_kind;
};

typedef U64 UIWidgetFlags;
typedef enum UIWidgetFlags_ENUM
{
 //-NOTE(tbt): drawing
 UI_WIDGET_FLAG_draw_outline                 = 1 <<  0,
 UI_WIDGET_FLAG_draw_background              = 1 <<  1,
 UI_WIDGET_FLAG_draw_label                   = 1 <<  2,
 UI_WIDGET_FLAG_blur_background              = 1 <<  3,
 UI_WIDGET_FLAG_hot_effect                   = 1 <<  4,
 UI_WIDGET_FLAG_active_effect                = 1 <<  5,
 UI_WIDGET_FLAG_toggled_effect               = 1 <<  6,
 UI_WIDGET_FLAG_draw_cursor                  = 1 <<  7,
 UI_WIDGET_FLAG_do_not_mask_children         = 1 <<  15,
 
 //-NOTE(tbt): interaction
 UI_WIDGET_FLAG_no_input                     = 1 <<  8,
 UI_WIDGET_FLAG_clickable                    = 1 <<  9,
 UI_WIDGET_FLAG_draggable_x                  = 1 << 10,
 UI_WIDGET_FLAG_draggable_y                  = 1 << 11,
 UI_WIDGET_FLAG_keyboard_focusable           = 1 << 12,
 UI_WIDGET_FLAG_scrollable                   = 1 << 13,
 
 //-NOTE(tbt): convenience
 UI_WIDGET_FLAG_draggable = UI_WIDGET_FLAG_draggable_x | UI_WIDGET_FLAG_draggable_y,
} UIWidgetFlags_ENUM;

typedef enum
{
 UI_LAYOUT_PLACEMENT_vertical,
 UI_LAYOUT_PLACEMENT_horizontal,
} UILayoutPlacement;

typedef struct
{
 F32 dim;
 F32 strictness;
} UIDimension;

typedef struct UIWidget UIWidget;
struct UIWidget
{
 // NOTE(tbt): hash table of widgets
 UIWidget *next_hash;
 S8 key;
 
 // NOTE(tbt): tree structure
 UIWidget *first_child;
 UIWidget *last_child;
 UIWidget *next_sibling;
 UIWidget *parent;
 I32 child_count;
 
 // NOTE(tbt): stack of insertion points
 UIWidget *next_insertion_point;
 
 // NOTE(tbt): doubley linked list of keyboard-focusable widgets 
 UIWidget *next_keyboard_focus;
 UIWidget *prev_keyboard_focus;
 
 // NOTE(tbt): processed when the widget is updated
 B32_s hot;
 B32_s active;
 B32 clicked;
 F32 drag_x, drag_y;
 F32 drag_x_offset;
 F32 drag_y_offset;
 B32_s toggled_transition;
 B32_s keyboard_focused_transition;
 Rect text_cursor_rect;
 Rect text_selection_rect;
 F32 scroll;
 F32 scroll_max;
 U8 temp_string_buffer[8];
 S8 temp_string;
 
 // NOTE(tbt): input
 UIWidgetFlags flags;
 Colour colour;
 Colour background;
 Colour hot_colour;
 Colour hot_background;
 Colour active_colour;
 Colour active_background;
 UIDimension w;
 UIDimension h;
 F32 indent;
 Font *font;
 S8 label;
 UILayoutPlacement children_placement;
 B32 toggled;
 I32 cursor;
 I32 mark;
 
 // NOTE(tbt): measurement pass
 struct
 {
  // NOTE(tbt): the desired width and height - may be reduced during layouting
  F32 w;
  F32 h;
  
  // NOTE(tbt): the maximum amount the width and height can be reduced by to make the layout work
  F32 w_can_loose;
  F32 h_can_loose;
  
  // NOTE(tbt): sum of the children's above (including padding)
  F32 children_total_w;
  F32 children_total_h;
  F32 children_total_w_can_loose;
  F32 children_total_h_can_loose;
 } measure;
 
 // NOTE(tbt): layout pass
 Rect layout;
 Rect interactable;
};

//
// NOTE(tbt): global state
//~

internal F64 global_time = 0.0;
internal F32 global_exposure = 1.0;

internal F64 global_audio_master_level = 0.8;

internal struct
{
 Locale locale;
 
 Font *normal_font;
 Font *title_font;
 
 F64 dialogue_seconds_per_character;
 
 S8 title;
 S8 play;
 S8 exit;
} global_current_locale_config = {0};

internal struct
{
 struct
 {
  Texture texture;
  
  SubTexture forward_head;
  SubTexture forward_left_arm;
  SubTexture forward_right_arm;
  SubTexture forward_lower_body;
  SubTexture forward_torso;
  
  SubTexture left_jacket;
  SubTexture left_leg;
  SubTexture left_head;
  SubTexture left_arm;
  
  SubTexture right_jacket;
  SubTexture right_leg;
  SubTexture right_head;
  SubTexture right_arm;
 } art;
 
 F32 x, y;
 F32 x_velocity, y_velocity;
 Rect collision_bounds;
} global_player = {0};

internal F32 global_world_projection_matrix[16];
internal F32 global_ui_projection_matrix[16];

struct
{
 struct RcxMessageQueue
 {
  RenderMessage *start;
  RenderMessage *end;
 } message_queue;
 
 U32 vao;
 U32 ibo;
 U32 vbo;
 
 struct RcxShaders
 {
#define shader(_name, _vertex_shader) ShaderID _name;
#include "shader_list.h"
  
  struct RcxShadersLastModified
  {
#define shader(_name, _vertex_shader) U64 _name;
#include "shader_list.h"
  } last_modified;
  
  ShaderID current;
 } shaders;
 
 // NOTE(tbt): uniform cache
 struct RcxUniformLocations
 {
  // NOTE(tbt): uniforms for texture shader
  struct RcxTextureShaderUniformLocations
  {
   I32 projection_matrix;
  } texture;
  
  // NOTE(tbt): uniforms for text shader
  struct RcxTextShaderUniformLocations
  {
   I32 projection_matrix;
  } text;
  
  // NOTE(tbt): uniforms for blur shader
  struct RcxBlurShaderUniformLocations
  {
   I32 direction;
  } blur;
  
  // NOTE(tbt): uniforms for post processing and memory post processing shaders
  struct RcxPostProcessingUniformLocations
  {
   I32 time;
   I32 exposure;
   I32 screen_texture;
   I32 blur_texture;
  };
  struct RcxPostProcessingUniformLocations post_processing;
  struct RcxPostProcessingUniformLocations memory_post_processing;
 } uniform_locations;
 
 struct RcxWindow
 {
  I32 w;
  I32 h;
 } window;
 
 struct RcxCamera
 {
  F32 x;
  F32 y;
 } camera;
 
 struct RcxFramebuffers
 {
  Framebuffer blur_a;
  Framebuffer blur_b;
  
  Framebuffer bloom_blur_a;
  Framebuffer bloom_blur_b;
  
  Framebuffer post_processing;
 } framebuffers;
 
 Rect mask_stack[64];
 I32 mask_stack_size;
 
 TextureID flat_colour_texture;
 
 TextureID current_texture;
} global_rcx = {{0}};

internal Font *global_ui_font;

internal GameState global_game_state = GAME_STATE_main_menu;

internal cm_Source *global_click_sound = NULL;

internal Entity *global_editor_selected_entity = NULL;

internal Entity global_dummy_entity = {0};
internal Entity global_entity_pool[MAX_ENTITIES] = {{0}};

struct
{
 UIDimension w_stack[64];
 I32 w_stack_size;
 
 UIDimension h_stack[64];
 I32 h_stack_size;
 
 Colour colour_stack[64];
 I32 colour_stack_size;
 
 Colour background_stack[64];
 I32 background_stack_size;
 
 Colour hot_colour_stack[64];
 I32 hot_colour_stack_size;
 
 Colour hot_background_stack[64];
 I32 hot_background_stack_size;
 
 Colour active_colour_stack[64];
 I32 active_colour_stack_size;
 
 Colour active_background_stack[64];
 I32 active_background_stack_size;
 
 F32 indent_stack[64];
 I32 indent_stack_size;
 
 Font *font_stack[64];
 I32 font_stack_size;
 
 F32 padding;
 F32 stroke_width;
 Colour keyboard_focus_highlight;
 
 UIWidget root;
 UIWidget *insertion_point;
 
 UIWidget *hot;
 
 UIWidget *first_keyboard_focus;
 UIWidget *last_keyboard_focus;
 UIWidget *keyboard_focus;
 
 UIWidget widget_dict[4096];
 
 F64 frametime_in_s;
 PlatformState *input;
} global_ui_context = {{{0}}};

internal struct
{
 S8 path;
 Texture *textures;
 U64 entity_next_index;
 Entity *entity_free_list;
 Entity *first_entity;
 Entity *last_entity;
 F32 y_offset_per_x;
 F32 player_scale;
 PostProcessingKind post_processing_kind;
} global_current_level_state = {{0}};

//
// NOTE(tbt): localisation
//~

internal Font *load_font(MemoryArena *memory, S8 path, I32 size, I32 bake_begin, I32 bake_count);


internal void
set_locale(Locale locale)
{
 if (locale == LOCALE_en_gb)
 {
  global_current_locale_config.locale = LOCALE_en_gb;
  
  I32 font_bake_begin = 32;
  I32 font_bake_end = 255;
  
  global_current_locale_config.normal_font = load_font(&global_static_memory,
                                                       s8_lit("../assets/fonts/PlayfairDisplay-Regular.ttf"),
                                                       28,
                                                       font_bake_begin,
                                                       font_bake_end - font_bake_begin);
  global_current_locale_config.title_font = load_font(&global_static_memory,
                                                      s8_lit("../assets/fonts/PlayfairDisplay-Regular.ttf"),
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
  
  I32 font_bake_begin = 32;
  I32 font_bake_end = 255;
  
  global_current_locale_config.normal_font = load_font(&global_static_memory,
                                                       s8_lit("../assets/fonts/PlayfairDisplay-Regular.ttf"),
                                                       28,
                                                       font_bake_begin,
                                                       font_bake_end - font_bake_begin);
  global_current_locale_config.title_font = load_font(&global_static_memory,
                                                      s8_lit("../assets/fonts/PlayfairDisplay-Regular.ttf"),
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
  
  I32 font_bake_begin = 0x4E00;
  I32 font_bake_end = 0x9FFF;
  
  global_current_locale_config.normal_font = load_font(&global_static_memory,
                                                       s8_lit("../assets/fonts/LiuJianMaoCao-Regular.ttf"),
                                                       28,
                                                       font_bake_begin,
                                                       font_bake_end - font_bake_begin);
  global_current_locale_config.title_font = load_font(&global_static_memory,
                                                      s8_lit("../assets/fonts/LiuJianMaoCao-Regular.ttf"),
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
#define locale(_locale) if (locale == LOCALE_ ## _locale) { string = s8(#_locale); }
#include "locales.h"
 
 return copy_s8(memory, string);
}

//
// NOTE(tbt): asset management
//~

internal Texture global_dummy_texture = {0};

#define ENTIRE_TEXTURE ((SubTexture){ 0.0f, 0.0f, 1.0f, 1.0f })

internal S8
path_from_level_path(MemoryArena *memory,
                     S8 path)
{
 S8List *list = NULL;
 list = push_s8_to_list(&global_frame_memory, list, path);
 list = push_s8_to_list(&global_frame_memory, list, s8_lit("../assets/levels/"));
 return join_s8_list(memory, list);
}

internal S8
path_from_dialogue_path(MemoryArena *memory,
                        S8 path)
{
 S8List *list = NULL;
 list = push_s8_to_list(&global_frame_memory, list, path);
 list = push_s8_to_list(&global_frame_memory, list, s8_lit("/"));
 list = push_s8_to_list(&global_frame_memory, list, s8_from_locale(&global_frame_memory, global_current_locale_config.locale));
 list = push_s8_to_list(&global_frame_memory, list, s8_lit("../assets/dialogue/"));
 return join_s8_list(memory, list);
}

internal B32
load_texture(Texture *result)
{
 B32 success;
 
 TextureID texture_id;
 I32 width, height;
 
 arena_temporary_memory(&global_temp_memory)
 {
  U8 *pixels = stbi_load(cstring_from_s8(&global_temp_memory, result->path),
                         &width,
                         &height,
                         NULL, 4);
  
  if (pixels)
  {
   glGenTextures(1, &texture_id);
   glBindTexture(GL_TEXTURE_2D, texture_id);
   
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   
   glTexImage2D(GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                width,
                height,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                pixels);
   
   result->last_modified = platform_get_file_modified_time_p(result->path);
   result->id = texture_id;
   result->width = width;
   result->height = height;
   
   debug_log("successfully loaded texture: '%.*s'\n", unravel_s8(result->path));
   success = true;
  }
  else
  {
   debug_log("error loading texture: '%.*s'\n", unravel_s8(result->path));
   success = false;
  }
 }
 
 return success;
}

internal Texture *
texture_from_path(S8 path)
{
 Texture *result;
 
 for (Texture *t = global_current_level_state.textures;
      NULL != t;
      t = t->next_loaded)
 {
  if (s8_match(t->path, path))
  {
   return t;
  }
 }
 
 Texture temp = {0};
 temp.path = path;
 if (load_texture(&temp))
 {
  result = arena_push(&global_level_memory, sizeof(*result));
  *result = temp;
  result->path = copy_s8(&global_level_memory, result->path);
  result->next_loaded = global_current_level_state.textures;
  global_current_level_state.textures = result;
 }
 else
 {
  result = &global_dummy_texture;
 }
 
 return result;
}

internal void renderer_flush_message_queue(void);

internal void
unload_texture(Texture *texture)
{
 if (global_rcx.flat_colour_texture == texture->id)
 {
  debug_log("warning: trying to unload flat colour texture\n");
  return;
 }
 // NOTE(tbt): early process all currently queued render messages in case any of them depend on the texture about to be unloaded
 renderer_flush_message_queue();
 
 if (global_rcx.current_texture == texture->id)
 {
  global_rcx.current_texture = 0;
 }
 glDeleteTextures(1, &texture->id);
 
 texture->id = 0;
 texture->width = 0;
 texture->height = 0;
 
 debug_log("unloaded texture\n");
}

internal SubTexture
sub_texture_from_texture(Texture *texture,
                         F32 x, F32 y,
                         F32 w, F32 h)
{
 SubTexture result;
 
 result.min_x = x / texture->width;
 result.min_y = y / texture->height;
 
 result.max_x = (x + w) / texture->width;
 result.max_y = (y + h) / texture->height;
 
 return result;
}

internal Font *
load_font(MemoryArena *memory,
          S8 path,
          I32 size,
          I32 font_bake_begin,
          I32 font_bake_count)
{
 Font *result = NULL;
 
 I32 font_texture_w = 8192;
 I32 font_texture_h = 8192;
 
 arena_temporary_memory(&global_temp_memory)
 {
  S8 file = platform_read_entire_file_p(&global_temp_memory, path);
  if (file.buffer)
  {
   U8 *bitmap = arena_push(&global_temp_memory, font_texture_w * font_texture_h);
   
   stbtt_pack_context packing_context;
   if (stbtt_PackBegin(&packing_context,
                       bitmap,
                       font_texture_w, font_texture_h,
                       0, 1, NULL))
    
   {
    result = arena_push(memory, sizeof(*result));
    
    result->char_data = arena_push(memory, sizeof(result->char_data[0]) * font_bake_count);
    
    stbtt_PackFontRange(&packing_context,
                        file.buffer,
                        0,
                        size,
                        font_bake_begin,
                        font_bake_count,
                        result->char_data);
    
    result->texture.width = font_texture_w;
    result->texture.height = font_texture_h;
    result->bake_begin = font_bake_begin;
    result->bake_end = font_bake_begin + font_bake_count;
    result->size = size;
    
    stbtt_fontinfo font_info = {0};
    if (stbtt_InitFont(&font_info,
                       file.buffer,
                       0))
    {
     F32 scale = stbtt_ScaleForMappingEmToPixels(&font_info, size);
     
     I32 ascent, descent, line_gap;
     stbtt_GetFontVMetrics(&font_info,
                           &ascent,
                           &descent,
                           &line_gap);
     result->vertical_advance = ascent - descent + line_gap;
     result->vertical_advance *= scale;
    }
    else
    {
     debug_log("warning loading font - could not get accurate metrics\n");
     result->vertical_advance = size;
    }
    
    glGenTextures(1, &result->texture.id);
    glBindTexture(GL_TEXTURE_2D, result->texture.id);
    global_rcx.current_texture = result->texture.id;
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RED,
                 result->texture.width,
                 result->texture.height,
                 0,
                 GL_RED,
                 GL_UNSIGNED_BYTE,
                 bitmap);
   }
   else
   {
    debug_log("error loading font - could not initialise packing context\n");
    result = NULL;
   }
   
   stbtt_PackEnd(&packing_context);
  }
  else
  {
   debug_log("error loading font - could not read file\n");
   result = NULL;
  }
 }
 
 return result;
}

internal S8List *
load_dialogue(MemoryArena *memory,
              S8 path)
{
 S8List *result = NULL;
 
 S8 file = platform_read_entire_file_p(memory, path);
 if (file.buffer)
 {
  S8 line = {0};
  line.buffer = file.buffer;
  
  for (U64 i = 0;
       i <= file.size;
       ++i)
  {
   if (file.buffer[i] == '\n')
   {
    result = append_s8_to_list(memory, result, line);
    i += 1;
    line.size = 0;
    line.buffer = file.buffer + i;
   }
   else
   {
    line.size += 1;
   }
  }
  result = append_s8_to_list(memory, result, line);
 }
 
 return result;
}

internal void
generate_orthographic_projection_matrix(F32 *matrix,
                                        F32 left,
                                        F32 right,
                                        F32 top,
                                        F32 bottom)
{
 memset(matrix, 0, 16 * sizeof(F32));
 matrix[0]  = 2.0f / (right - left);
 matrix[5]  = 2.0f / (top - bottom);
 matrix[10] = -1.0f;
 matrix[12] = -(right + left) / (right - left);
 matrix[13] = -(top + bottom) / (top - bottom);
 matrix[14] = 0.0f;
 matrix[15] = 1.0f;
}

void
gl_debug_message_callback(GLenum source​,
                          GLenum type​,
                          GLuint id​,
                          GLenum severity​,
                          GLsizei length​,
                          const GLchar *message,
                          const void *userParam​)
{
 debug_log("\n*** %s ***\n", message);
#if defined(_WIN32) && defined(LUCERNA_DEBUG)
 __debugbreak();
#endif
}


internal Rect
measure_s32(Font *font,
            F32 x, F32 y,
            I32 wrap_width,
            S32 string)
{
 Rect result = rect(x, y, 0, 0);
 
 F32 line_start = x;
 F32 curr_x = x, curr_y = y;
 
 for (I32 i = 0;
      i < string.len;
      ++i)
 {
  if (string.buffer[i] == '\n')
  {
   curr_x = line_start;
   curr_y += font->vertical_advance;
  }
  else if (string.buffer[i] >= font->bake_begin &&
           string.buffer[i] < font->bake_end)
  {
   stbtt_packedchar *b = font->char_data + (string.buffer[i] - font->bake_begin);
   
   if (wrap_width > 0.0f &&
       is_char_space(string.buffer[i]) &&
       curr_x > line_start + wrap_width)
   {
    curr_x = line_start;
    curr_y += font->vertical_advance;
   }
   else
   {
    result.w += b->xadvance;
    if (curr_x + b->xoff < result.x)
    {
     result.x = curr_x + b->xoff;
    }
    if (curr_x + b->xoff2 > (result.x + result.w))
    {
     result.w = (curr_x + b->xoff2) - result.x;
    }
    if (curr_y + b->yoff < result.y)
    {
     result.y = curr_y + b->yoff;
    }
    if (curr_y + b->yoff2 > (result.y + result.h))
    {
     result.h = (curr_y + b->yoff2) - result.y;
    }
    curr_x += b->xadvance;
   }
  }
 }
 
 return result;
}


Quad
generate_quad(Rect rectangle,
              Colour colour,
              SubTexture sub_texture)
{
 Quad result;
 
 result.bl.x = rectangle.x;
 result.bl.y = rectangle.y + rectangle.h;
 result.bl.r = colour.r;
 result.bl.g = colour.g;
 result.bl.b = colour.b;
 result.bl.a = colour.a;
 result.bl.u = sub_texture.min_x;
 result.bl.v = sub_texture.max_y;
 
 result.br.x = rectangle.x + rectangle.w;
 result.br.y = rectangle.y + rectangle.h;
 result.br.r = colour.r;
 result.br.g = colour.g;
 result.br.b = colour.b;
 result.br.a = colour.a;
 result.br.u = sub_texture.max_x;
 result.br.v = sub_texture.max_y;
 
 result.tr.x = rectangle.x + rectangle.w;
 result.tr.y = rectangle.y;
 result.tr.r = colour.r;
 result.tr.g = colour.g;
 result.tr.b = colour.b;
 result.tr.a = colour.a;
 result.tr.u = sub_texture.max_x;
 result.tr.v = sub_texture.min_y;
 
 result.tl.x = rectangle.x;
 result.tl.y = rectangle.y;
 result.tl.r = colour.r;
 result.tl.g = colour.g;
 result.tl.b = colour.b;
 result.tl.a = colour.a;
 result.tl.u = sub_texture.min_x;
 result.tl.v = sub_texture.min_y;
 
 return result;
}

Quad
generate_rotated_quad(Rect rectangle,
                      F32 angle,
                      Colour colour,
                      SubTexture sub_texture)
{
 Quad result;
 
 //
 // NOTE(tbt): we want to rotate about the quad's origin, not world's, so
 //            first a quad is generated at (0, 0), a rotation matrix is applied
 //            and it is then translated to be at the intended position.
 //
 
 Rect at_origin = rect(0.0f, 0.0f, rectangle.w, rectangle.h);
 
 result = generate_quad(at_origin, colour, sub_texture);
 
 
 // NOTE(tbt): cast the quad to a vertex pointer, so we can iterate through
 //            each vertex
 Vertex *vertices = (Vertex *)(&result);
 
 for (I32 i = 0; i < 4; ++i)
 {
  // NOTE(tbt): matrix generation and multiplication are combined into one step
  Vertex vertex = vertices[i];
  vertices[i].x = vertex.x * cos(angle) - vertex.y * sin(angle);
  vertices[i].y = vertex.x * sin(angle) + vertex.y * cos(angle);
  
  // NOTE(tbt): once a vertex has been rotated it can be translated to the
  //            intended position
  vertices[i].x += rectangle.x;
  vertices[i].y += rectangle.y;
 }
 
 return result;
}

//
// NOTE(tbt): x-macro shader compilation (should probably have used LCDDL)
//

#define renderer_compile_and_link_fragment_shader(_name, _vertex_shader)                                     \
shader_src = cstring_from_s8(&global_static_memory,                                                         \
platform_read_entire_file_p(&global_static_memory,                             \
s8_lit("../assets/shaders/" #_name ".frag"))); \
fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);                                                       \
glShaderSource(fragment_shader, 1, &shader_src, NULL);                                                      \
glCompileShader(fragment_shader);                                                                         \
glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);                                               \
if (status == GL_FALSE)                                                                                     \
{                                                                                                           \
I8 msg[SHADER_INFO_LOG_MAX_LEN];                                                                           \
glGetShaderInfoLog(fragment_shader,                                                                      \
SHADER_INFO_LOG_MAX_LEN,                                                              \
NULL,                                                                                 \
msg);                                                                                 \
debug_log(#_name " fragment shader compilation failure. '%s'\n", msg);                                     \
exit(-1);                                                                                                  \
}                                                                                                           \
glAttachShader(global_rcx.shaders. ## _name, fragment_shader);                                            \
glLinkProgram(global_rcx.shaders. ## _name);                                                              \
glGetProgramiv(global_rcx.shaders. ## _name, GL_LINK_STATUS, &status);                                    \
if (status == GL_FALSE)                                                                                     \
{                                                                                                           \
I8 msg[SHADER_INFO_LOG_MAX_LEN];                                                                           \
glGetShaderInfoLog(global_rcx.shaders. ## _name,                                                         \
SHADER_INFO_LOG_MAX_LEN,                                                              \
NULL,                                                                                 \
msg);                                                                                 \
glDeleteProgram(global_rcx.shaders. ## _name);                                                           \
glDeleteShader(_vertex_shader);                                                                          \
glDeleteShader(fragment_shader);                                                                         \
debug_log(#_name " shader link failure. '%s'\n", msg);                                                     \
exit(-1);                                                                                                  \
}                                                                                                           \
glDetachShader(global_rcx.shaders. ## _name, _vertex_shader);                                             \
glDetachShader(global_rcx.shaders. ## _name, fragment_shader);                                            \
glDeleteShader(fragment_shader);                                                                          \
global_rcx.shaders.last_modified. ## _name = platform_get_file_modified_time_p(s8_lit("../assets/shaders/" #_name ".frag"))

internal void
cache_uniform_locations(void)
{
 global_rcx.uniform_locations.texture.projection_matrix = glGetUniformLocation(global_rcx.shaders.texture, "u_projection_matrix");
 
 global_rcx.uniform_locations.text.projection_matrix = glGetUniformLocation(global_rcx.shaders.text, "u_projection_matrix");
 
 global_rcx.uniform_locations.blur.direction = glGetUniformLocation(global_rcx.shaders.blur, "u_direction");
 
 global_rcx.uniform_locations.post_processing.time = glGetUniformLocation(global_rcx.shaders.post_processing, "u_time");
 global_rcx.uniform_locations.post_processing.exposure = glGetUniformLocation(global_rcx.shaders.post_processing, "u_exposure");
 global_rcx.uniform_locations.post_processing.screen_texture = glGetUniformLocation(global_rcx.shaders.post_processing, "u_screen_texture");
 global_rcx.uniform_locations.post_processing.blur_texture = glGetUniformLocation(global_rcx.shaders.post_processing, "u_blur_texture");
 
 global_rcx.uniform_locations.memory_post_processing.time = glGetUniformLocation(global_rcx.shaders.memory_post_processing, "u_time");
 global_rcx.uniform_locations.memory_post_processing.exposure = glGetUniformLocation(global_rcx.shaders.memory_post_processing, "u_exposure");
 global_rcx.uniform_locations.memory_post_processing.screen_texture = glGetUniformLocation(global_rcx.shaders.memory_post_processing, "u_screen_texture");
 global_rcx.uniform_locations.memory_post_processing.blur_texture = glGetUniformLocation(global_rcx.shaders.memory_post_processing, "u_blur_texture");
}

// NOTE(tbt): I have no idea why I chose this preprocessor monstrosity over a simple LCDDL metaprogram, but it works
internal void
hot_reload_shaders(F64 frametime_in_s)
{
 persist F64 time = 0.0;
 time += frametime_in_s;
 
 I32 status;
 U32 fragment_shader;
 const GLchar *shader_src;
 
 F64 refresh_time = 2.0;
 if (time > refresh_time)
 {
#define shader(_name, _vertex_shader_name) \
{\
U64 last_modified = platform_get_file_modified_time_p(s8_lit("../assets/shaders/" #_name ".frag"));\
if (last_modified > global_rcx.shaders.last_modified. ## _name) \
{\
renderer_flush_message_queue();\
global_rcx.shaders.last_modified. ## _name = last_modified;\
debug_log("hot reloading " #_name " shader\n");\
shader_src = cstring_from_s8(&global_temp_memory, platform_read_entire_file_p(&global_temp_memory, s8_lit("../assets/shaders/" #_vertex_shader_name ".vert")));\
U32 _vertex_shader_name ## _vertex_shader = glCreateShader(GL_VERTEX_SHADER);\
glShaderSource(_vertex_shader_name ## _vertex_shader, 1, &shader_src, NULL);\
glCompileShader(_vertex_shader_name ## _vertex_shader);\
glGetShaderiv(_vertex_shader_name ## _vertex_shader, GL_COMPILE_STATUS, &status);\
if (status == GL_FALSE)\
{\
I8 msg[SHADER_INFO_LOG_MAX_LEN];\
glGetShaderInfoLog(_vertex_shader_name ## _vertex_shader, SHADER_INFO_LOG_MAX_LEN, NULL, msg);\
glDeleteShader(_vertex_shader_name ## _vertex_shader);\
debug_log("vertex shader compilation failure. '%s'\n", msg);\
exit(-1);\
}\
glAttachShader(global_rcx.shaders. ## _name, _vertex_shader_name ## _vertex_shader);\
renderer_compile_and_link_fragment_shader(_name, _vertex_shader_name ## _vertex_shader);\
glDeleteShader(_vertex_shader_name ## _vertex_shader);\
cache_uniform_locations();\
}\
}
#include "shader_list.h"
  
 }
}

//
// NOTE(tbt): renderer
//~

internal void
initialise_renderer(void)
{
 I32 i;
 I32 offset;
 I32 indices[BATCH_SIZE * 6];
 void *render_queue_backing_memory;
 
 //
 // NOTE(tbt): general OpenGL setup
 //
 
#ifdef LUCERNA_DEBUG
 glDebugMessageCallback(gl_debug_message_callback, NULL);
#endif
 
 glGenVertexArrays(1, &global_rcx.vao);
 glBindVertexArray(global_rcx.vao);
 
 glGenBuffers(1, &global_rcx.vbo);
 glBindBuffer(GL_ARRAY_BUFFER, global_rcx.vbo);
 
 glEnableVertexAttribArray(0);
 glVertexAttribPointer(0,
                       2,
                       GL_FLOAT,
                       GL_FALSE,
                       sizeof(Vertex),
                       NULL);
 
 glEnableVertexAttribArray(1);
 glVertexAttribPointer(1,
                       4,
                       GL_FLOAT,
                       GL_FALSE,
                       sizeof(Vertex),
                       (const void *)(2 * sizeof(F32)));
 
 glEnableVertexAttribArray(2);
 glVertexAttribPointer(2,
                       2,
                       GL_FLOAT,
                       GL_FALSE,
                       sizeof(Vertex),
                       (const void *)(6 * sizeof(F32)));
 
 glGenBuffers(1, &global_rcx.ibo);
 offset = 0;
 for (i = 0; i < BATCH_SIZE * 6; i += 6)
 {
  indices[i + 0] = 0 + offset;
  indices[i + 1] = 1 + offset;
  indices[i + 2] = 2 + offset;
  
  indices[i + 3] = 2 + offset;
  indices[i + 4] = 3 + offset;
  indices[i + 5] = 0 + offset;
  
  offset += 4;
 }
 
 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_rcx.ibo);
 glBufferData(GL_ELEMENT_ARRAY_BUFFER,
              BATCH_SIZE * 6 * sizeof(indices[0]),
              indices,
              GL_STATIC_DRAW);
 
 glBufferData(GL_ARRAY_BUFFER,
              BATCH_SIZE * 4 * sizeof(Vertex),
              NULL,
              GL_DYNAMIC_DRAW);
 
 glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
 
 glEnable(GL_BLEND);
 glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
 
 glEnable(GL_SCISSOR_TEST);
 
 //
 // NOTE(tbt): setup 1x1 white texture for rendering flat colours
 //
 
 U32 flat_colour_texture_data = 0xffffffff;
 
 glGenTextures(1, &global_rcx.flat_colour_texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.flat_colour_texture);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              1,
              1,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              &flat_colour_texture_data);
 
 //
 // NOTE(tbt): shader compilation
 //
 
 I32 status;
 global_rcx.shaders.texture = glCreateProgram();
 global_rcx.shaders.blur = glCreateProgram();
 global_rcx.shaders.text = glCreateProgram();
 global_rcx.shaders.post_processing = glCreateProgram();
 global_rcx.shaders.memory_post_processing = glCreateProgram();
 const GLchar *shader_src;
 ShaderID default_vertex_shader, fullscreen_vertex_shader;
 ShaderID fragment_shader;
 
 arena_temporary_memory(&global_temp_memory)
 {
  // NOTE(tbt): compile the default vertex shader
  shader_src = cstring_from_s8(&global_temp_memory,
                               platform_read_entire_file_p(&global_temp_memory,
                                                           s8_lit("../assets/shaders/default.vert")));
  
  default_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(default_vertex_shader, 1, &shader_src, NULL);
  glCompileShader(default_vertex_shader);
  
  // NOTE(tbt): check for default vertex shader compilation errors
  glGetShaderiv(default_vertex_shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
  {
   I8 msg[SHADER_INFO_LOG_MAX_LEN];
   
   glGetShaderInfoLog(default_vertex_shader,
                      SHADER_INFO_LOG_MAX_LEN,
                      NULL,
                      msg);
   glDeleteShader(default_vertex_shader);
   
   debug_log("default vertex shader compilation failure. '%s'\n", msg);
   exit(-1);
  }
  
  debug_log("successfully compiled default vertex shader\n");
  
  // NOTE(tbt): compile the fullscreen vertex shader
  shader_src = cstring_from_s8(&global_temp_memory,
                               platform_read_entire_file_p(&global_temp_memory,
                                                           s8_lit("../assets/shaders/fullscreen.vert")));
  
  fullscreen_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(fullscreen_vertex_shader, 1, &shader_src, NULL);
  glCompileShader(fullscreen_vertex_shader);
  
  // NOTE(tbt): check for fullscreen vertex shader compilation errors
  glGetShaderiv(fullscreen_vertex_shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
  {
   I8 msg[SHADER_INFO_LOG_MAX_LEN];
   
   glGetShaderInfoLog(fullscreen_vertex_shader,
                      SHADER_INFO_LOG_MAX_LEN,
                      NULL,
                      msg);
   glDeleteShader(fullscreen_vertex_shader);
   
   debug_log("fullscreen vertex shader compilation failure. '%s'\n", msg);
   exit(-1);
  }
  
  debug_log("successfully compiled fullscreen vertex shader\n");
  
  // NOTE(tbt): attach vertex shaders to shader programs
#define shader(_name, _vertex_shader_name) glAttachShader(global_rcx.shaders. ## _name, _vertex_shader_name ## _vertex_shader);
#include "shader_list.h"
  
  // NOTE(tbt): compile and line fragment shaders
#define shader(_name, _vertex_shader_name) renderer_compile_and_link_fragment_shader(_name, _vertex_shader_name ## _vertex_shader);
#include "shader_list.h"
  
  // NOTE(tbt): cleanup shader stuff
  glDeleteShader(default_vertex_shader);
  glDeleteShader(fullscreen_vertex_shader);
 }
 
 cache_uniform_locations();
 
 // NOTE(tbt): reset currently bound shader
 glUseProgram(global_rcx.shaders.current);
 
 
 //
 // NOTE(tbt): setup framebuffers
 //
 
 // NOTE(tbt): framebuffer for first blur pass
 glGenFramebuffers(1, &global_rcx.framebuffers.blur_a.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.blur_a.target);
 
 glGenTextures(1, &global_rcx.framebuffers.blur_a.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.blur_a.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              BLUR_TEXTURE_W,
              BLUR_TEXTURE_H,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.blur_a.texture,
                        0);
 
 // NOTE(tbt): framebuffer for second blur pass
 glGenFramebuffers(1, &global_rcx.framebuffers.blur_b.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.blur_b.target);
 
 glGenTextures(1, &global_rcx.framebuffers.blur_b.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.blur_b.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              BLUR_TEXTURE_W,
              BLUR_TEXTURE_H,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.blur_b.texture,
                        0);
 
 // NOTE(tbt): framebuffer for first blur pass for bloom
 glGenFramebuffers(1, &global_rcx.framebuffers.bloom_blur_a.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.bloom_blur_a.target);
 
 glGenTextures(1, &global_rcx.framebuffers.bloom_blur_a.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.bloom_blur_a.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              BLUR_TEXTURE_W,
              BLUR_TEXTURE_H,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.bloom_blur_a.texture,
                        0);
 
 // NOTE(tbt): framebuffer for second blur pass for bloom
 glGenFramebuffers(1, &global_rcx.framebuffers.bloom_blur_b.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.bloom_blur_b.target);
 
 glGenTextures(1, &global_rcx.framebuffers.bloom_blur_b.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.bloom_blur_b.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              BLUR_TEXTURE_W,
              BLUR_TEXTURE_H,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.bloom_blur_b.texture,
                        0);
 
 // NOTE(tbt): framebuffer for post processing
 glGenFramebuffers(1, &global_rcx.framebuffers.post_processing.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.post_processing.target);
 
 glGenTextures(1, &global_rcx.framebuffers.post_processing.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.post_processing.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              DEFAULT_WINDOW_WIDTH,
              DEFAULT_WINDOW_HEIGHT,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.post_processing.texture,
                        0);
 
 glBindTexture(GL_TEXTURE_2D, 0);
 glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

internal void
renderer_enqueue_message(RenderMessage message)
{
 RenderMessage *queued_message =
  arena_push(&global_frame_memory,
             sizeof(*queued_message));
 
 *queued_message = message;
 
 // NOTE(tbt): copy mask from stack
 {
  queued_message->mask = global_rcx.mask_stack[global_rcx.mask_stack_size];
  
  if (queued_message->mask.x < 0.0f)
  {
   queued_message->mask.w -= queued_message->mask.x;
   queued_message->mask.x = 0.0f;
  }
  if (queued_message->mask.y < 0.0f)
  {
   queued_message->mask.h -= queued_message->mask.y;
   queued_message->mask.y = 0.0f;
  }
  queued_message->mask.w = max_f(queued_message->mask.w, 0.0f);
  queued_message->mask.h = max_f(queued_message->mask.h, 0.0f);
 }
 
 if (global_rcx.message_queue.end)
 {
  global_rcx.message_queue.end->next = queued_message;
 }
 else
 {
  global_rcx.message_queue.start = queued_message;
 }
 
 global_rcx.message_queue.end = queued_message;
}

internal B32
renderer_dequeue_message(RenderMessage *result)
{
 
 if (!global_rcx.message_queue.start) { return false; }
 
 *result = *global_rcx.message_queue.start;
 global_rcx.message_queue.start = global_rcx.message_queue.start->next;
 
 return true;
}

internal void
renderer_flush_batch(RenderBatch *batch)
{
 if (!batch->in_use) return;
 
 glScissor(batch->mask.x,
           global_rcx.window.h - batch->mask.y - batch->mask.h,
           batch->mask.w,
           batch->mask.h);
 
 if (global_rcx.shaders.current != batch->shader)
 {
  glUseProgram(batch->shader);
  global_rcx.shaders.current = batch->shader;
 }
 
 if (global_rcx.current_texture != batch->texture)
 {
  glBindTexture(GL_TEXTURE_2D, batch->texture);
  global_rcx.current_texture = batch->texture;
 }
 
 if (batch->shader == global_rcx.shaders.texture)
 {
  glUniformMatrix4fv(global_rcx.uniform_locations.texture.projection_matrix,
                     1,
                     GL_FALSE,
                     batch->projection_matrix);
 }
 else if (batch->shader == global_rcx.shaders.text)
 {
  glUniformMatrix4fv(global_rcx.uniform_locations.text.projection_matrix,
                     1,
                     GL_FALSE,
                     batch->projection_matrix);
 }
 
 glBufferData(GL_ARRAY_BUFFER,
              batch->quad_count * sizeof(Quad),
              batch->buffer,
              GL_DYNAMIC_DRAW);
 
 glDrawElements(GL_TRIANGLES,
                batch->quad_count * 6,
                GL_UNSIGNED_INT,
                NULL);
 
 batch->quad_count = 0;
 batch->texture = 0;
 batch->shader = 0;
 batch->projection_matrix = NULL;
 batch->in_use = false;
}

internal void
renderer_flush_message_queue()
{
 RenderMessage message;
 
 RenderBatch batch;
 batch.quad_count = 0;
 batch.texture = 0;
 batch.shader = 0;
 batch.in_use = false;
 
 glEnable(GL_SCISSOR_TEST);
 
 //
 // NOTE(tbt): sort message queue
 //
 
 // NOTE(tbt): form a bucket for each depth
 RenderMessage *heads[256] = {0};
 RenderMessage *tails[256] = {0};
 
 RenderMessage *next = NULL;
 
 for (RenderMessage *node = global_rcx.message_queue.start;
      node;
      node = next)
 {
  if (tails[node->sort])
  {
   tails[node->sort]->next = node;
  }
  else
  {
   heads[node->sort] = node;
  }
  tails[node->sort] = node;
  next = node->next;
  node->next = NULL;
 }
 
 // NOTE(tbt): reconstruct queue in correct order
 global_rcx.message_queue.start = NULL;
 global_rcx.message_queue.end = NULL;
 for (I32 i = 0;
      i < 256;
      ++i)
 {
  if (!heads[i]) { continue; }
  
  if (global_rcx.message_queue.end)
  {
   global_rcx.message_queue.end->next = heads[i];
  }
  else
  {
   global_rcx.message_queue.start = heads[i];
  }
  global_rcx.message_queue.end = tails[i];
 }
 
 //
 // NOTE(tbt): main render message processing loop
 //
 
 while (renderer_dequeue_message(&message))
 {
  switch (message.kind)
  {
   case RENDER_MESSAGE_draw_rectangle:
   {
    if (batch.texture != message.texture ||
        batch.shader != global_rcx.shaders.texture ||
        batch.quad_count >= BATCH_SIZE ||
        batch.projection_matrix != message.projection_matrix ||
        !rect_match(batch.mask, message.mask) ||
        !(batch.in_use))
    {
     renderer_flush_batch(&batch);
     
     batch.shader = global_rcx.shaders.texture;
     batch.texture = message.texture;
     batch.projection_matrix = message.projection_matrix;
     batch.mask = message.mask;
    }
    
    batch.in_use = true;
    
    batch.buffer[batch.quad_count] = generate_rotated_quad(message.rectangle,
                                                           message.angle,
                                                           message.colour,
                                                           message.sub_texture);
    batch.quad_count += 1;
    
    break;
   }
   
   case RENDER_MESSAGE_stroke_rectangle:
   {
    Rect top, bottom, left, right;
    F32 stroke_width;
    
    if (batch.texture != global_rcx.flat_colour_texture ||
        batch.shader != global_rcx.shaders.texture||
        batch.projection_matrix != message.projection_matrix ||
        batch.quad_count >= BATCH_SIZE - 4 || // NOTE(tbt): 4 quads to stroke a rectangle
        !rect_match(batch.mask, message.mask) ||
        !(batch.in_use))
    {
     renderer_flush_batch(&batch);
     
     batch.shader = global_rcx.shaders.texture;
     batch.texture = global_rcx.flat_colour_texture;
     batch.projection_matrix = message.projection_matrix;
     batch.mask = message.mask;
    }
    
    batch.in_use = true;
    
    stroke_width = message.stroke_width;
    
    top = rect(message.rectangle.x,
               message.rectangle.y,
               message.rectangle.w,
               stroke_width);
    
    bottom = rect(message.rectangle.x,
                  message.rectangle.y +
                  message.rectangle.h -
                  stroke_width,
                  message.rectangle.w,
                  stroke_width);
    
    left = rect(message.rectangle.x,
                message.rectangle.y +
                stroke_width,
                stroke_width,
                message.rectangle.h -
                stroke_width * 2);
    
    right = rect(message.rectangle.x +
                 message.rectangle.w -
                 stroke_width,
                 message.rectangle.y +
                 stroke_width,
                 stroke_width,
                 message.rectangle.h -
                 stroke_width * 2);
    
    batch.buffer[batch.quad_count++] =
     generate_quad(top,
                   message.colour,
                   message.sub_texture);
    
    batch.buffer[batch.quad_count++] =
     generate_quad(bottom,
                   message.colour,
                   message.sub_texture);
    
    batch.buffer[batch.quad_count++] =
     generate_quad(left,
                   message.colour,
                   message.sub_texture);
    
    batch.buffer[batch.quad_count++] =
     generate_quad(right,
                   message.colour,
                   message.sub_texture);
    
    break;
   }
   
   case RENDER_MESSAGE_draw_text:
   {
    F32 line_start = message.rectangle.x;
    F32 x = message.rectangle.x;
    F32 y = message.rectangle.y;
    I32 wrap_width = message.rectangle.w;
    
    if (batch.texture != message.font->texture.id ||
        batch.shader != global_rcx.shaders.text ||
        batch.projection_matrix != message.projection_matrix ||
        batch.quad_count >= BATCH_SIZE ||
        !rect_match(batch.mask, message.mask) ||
        !(batch.in_use))
    {
     renderer_flush_batch(&batch);
     
     batch.shader = global_rcx.shaders.text;
     batch.texture = message.font->texture.id;
     batch.projection_matrix = message.projection_matrix;
     batch.mask = message.mask;
    }
    
    batch.in_use = true;
    
    I32 font_bake_begin = message.font->bake_begin;
    I32 font_bake_end = message.font->bake_end;
    
    I32 i = 0;
    for (UTF8Consume consume = consume_utf8_from_string(message.string, i);
         i < message.string.size;
         i += consume.advance, consume = consume_utf8_from_string(message.string, i))
    {
     if (consume.codepoint == '\n')
     {
      x = line_start;
      y += message.font->vertical_advance;
     }
     else if (consume.codepoint >= font_bake_begin &&
              consume.codepoint < font_bake_end)
     {
      stbtt_aligned_quad q;
      Rect rectangle;
      SubTexture sub_texture;
      
      stbtt_GetPackedQuad(message.font->char_data,
                          message.font->texture.width,
                          message.font->texture.height,
                          consume.codepoint - font_bake_begin,
                          &x, &y,
                          &q,
                          false);
      
      sub_texture.min_x = q.s0;
      sub_texture.min_y = q.t0;
      sub_texture.max_x = q.s1;
      sub_texture.max_y = q.t1;
      
      rectangle = rect(q.x0, q.y0,
                       q.x1 - q.x0,
                       q.y1 - q.y0);
      
      if (batch.quad_count >= BATCH_SIZE)
      {
       renderer_flush_batch(&batch);
       
       batch.shader = global_rcx.shaders.text;
       batch.texture = message.font->texture.id;
       batch.projection_matrix = message.projection_matrix;
       batch.mask = message.mask;
      }
      else
      {
       batch.buffer[batch.quad_count++] =
        generate_quad(rectangle,
                      message.colour,
                      sub_texture);
      }
      if (wrap_width > 0.0f &&
          is_char_space(consume.codepoint))
      {
       if (x > line_start + wrap_width)
       {
        x = line_start;
        y += message.font->vertical_advance;
       }
      }
     }
    }
    
    break;
   }
   
   case RENDER_MESSAGE_blur_screen_region:
   {
    renderer_flush_batch(&batch);
    
    glDisable(GL_SCISSOR_TEST);
    
    // NOTE(tbt): blit screen to framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, global_rcx.framebuffers.blur_a.target);
    
    glViewport(0, 0, BLUR_TEXTURE_W, BLUR_TEXTURE_H);
    
    glBlitFramebuffer(0,
                      0,
                      global_rcx.window.w,
                      global_rcx.window.h,
                      0,
                      0,
                      BLUR_TEXTURE_W,
                      BLUR_TEXTURE_H,
                      GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);
    
    glUseProgram(global_rcx.shaders.blur);
    global_rcx.shaders.current = global_rcx.shaders.blur;
    
    for (I32 i = 0;
         i < message.strength;
         ++i)
    {
     // NOTE(tbt): apply first (horizontal) blur pass
     glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.blur_b.target);
     glUniform2f(global_rcx.uniform_locations.blur.direction, 1.0f, 0.0f);
     glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.blur_a.texture);
     glDrawArrays(GL_TRIANGLES, 0, 6);
     
     // NOTE(tbt): apply second (vertical) blur pass
     glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.blur_a.target);
     glUniform2f(global_rcx.uniform_locations.blur.direction, 0.0f, 1.0f);
     glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.blur_b.texture);
     glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    // NOTE(tbt): blit desired region back to screen
    glBindFramebuffer(GL_READ_FRAMEBUFFER, global_rcx.framebuffers.blur_a.target);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    
    glViewport(0,
               0,
               global_rcx.window.w,
               global_rcx.window.h);
    
    glEnable(GL_SCISSOR_TEST);
    glScissor(message.mask.x,
              global_rcx.window.h - message.mask.y - message.mask.h,
              message.mask.w,
              message.mask.h);
    
    F32 x0 = message.rectangle.x;
    F32 y0 = global_rcx.window.h - message.rectangle.y;
    F32 x1 = message.rectangle.x + message.rectangle.w;
    F32 y1 = global_rcx.window.h - message.rectangle.y - message.rectangle.h;
    
    F32 x_scale = (F32)BLUR_TEXTURE_W / (F32)global_rcx.window.w;
    F32 y_scale = (F32)BLUR_TEXTURE_H / (F32)global_rcx.window.h;
    glBlitFramebuffer(x0 * x_scale,
                      y0 * y_scale,
                      x1 * x_scale,
                      y1 * y_scale,
                      x0, y0, x1, y1,
                      GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);
    
    // NOTE(tbt): reset current texture
    glBindTexture(GL_TEXTURE_2D, global_rcx.current_texture);
    
    break;
   }
   
   case RENDER_MESSAGE_draw_gradient:
   {
    Quad quad;
    
    quad.bl.x = message.rectangle.x;
    quad.bl.y = message.rectangle.y + message.rectangle.h;
    quad.bl.r = message.gradient.bl.r;
    quad.bl.g = message.gradient.bl.g;
    quad.bl.b = message.gradient.bl.b;
    quad.bl.a = message.gradient.bl.a;
    quad.bl.u = 0.0f;
    quad.bl.v = 1.0f;
    
    quad.br.x = message.rectangle.x + message.rectangle.w;
    quad.br.y = message.rectangle.y + message.rectangle.h;
    quad.br.r = message.gradient.br.r;
    quad.br.g = message.gradient.br.g;
    quad.br.b = message.gradient.br.b;
    quad.br.a = message.gradient.br.a;
    quad.br.u = 1.0f;
    quad.br.v = 1.0f;
    
    quad.tr.x = message.rectangle.x + message.rectangle.w;
    quad.tr.y = message.rectangle.y;
    quad.tr.r = message.gradient.tr.r;
    quad.tr.g = message.gradient.tr.g;
    quad.tr.b = message.gradient.tr.b;
    quad.tr.a = message.gradient.tr.a;
    quad.tr.u = 1.0f;
    quad.tr.v = 0.0f;
    
    quad.tl.x = message.rectangle.x;
    quad.tl.y = message.rectangle.y;
    quad.tl.r = message.gradient.tl.r;
    quad.tl.g = message.gradient.tl.g;
    quad.tl.b = message.gradient.tl.b;
    quad.tl.a = message.gradient.tl.a;
    quad.tl.u = 0.0f;
    quad.tl.v = 0.0f;
    
    if (batch.texture != global_rcx.flat_colour_texture ||
        batch.shader != global_rcx.shaders.texture ||
        batch.quad_count >= BATCH_SIZE ||
        batch.projection_matrix != message.projection_matrix ||
        !rect_match(batch.mask, message.mask) ||
        !(batch.in_use))
    {
     renderer_flush_batch(&batch);
     
     batch.shader = global_rcx.shaders.texture;
     batch.texture = global_rcx.flat_colour_texture;
     batch.projection_matrix = message.projection_matrix;
     batch.mask = message.mask;
    }
    
    batch.in_use = true;
    
    batch.buffer[batch.quad_count++] = quad;
    
    break;
   }
   
   case RENDER_MESSAGE_do_post_processing:
   {
    renderer_flush_batch(&batch);
    
    glDisable(GL_SCISSOR_TEST);
    
    //-NOTE(tbt): setup for relevant post processing kind
    
    U32 post_shader;
    Framebuffer *blur_framebuffer_1;
    Framebuffer *blur_framebuffer_2;
    struct RcxPostProcessingUniformLocations *uniforms;
    
    if (message.post_processing_kind == POST_PROCESSING_KIND_memory)
    {
     post_shader = global_rcx.shaders.memory_post_processing;
     uniforms = &global_rcx.uniform_locations.memory_post_processing;
     blur_framebuffer_1 = &global_rcx.framebuffers.bloom_blur_a;
     blur_framebuffer_2 = &global_rcx.framebuffers.bloom_blur_b;
    }
    else if (message.post_processing_kind == POST_PROCESSING_KIND_world)
    {
     post_shader = global_rcx.shaders.post_processing;
     uniforms = &global_rcx.uniform_locations.post_processing;
     blur_framebuffer_1 = &global_rcx.framebuffers.blur_a;
     blur_framebuffer_2 = &global_rcx.framebuffers.blur_b;
    }
    else
    {
     break;
    }
    
    //-NOTE(tbt): blit screen to framebuffers
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, global_rcx.framebuffers.post_processing.target);
    {
     glBlitFramebuffer(0, 0, global_rcx.window.w, global_rcx.window.h,
                       0, 0, global_rcx.window.w, global_rcx.window.h,
                       GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, blur_framebuffer_1->target);
    {
     glViewport(0, 0, BLUR_TEXTURE_W, BLUR_TEXTURE_H);
     glBlitFramebuffer(0, 0, global_rcx.window.w, global_rcx.window.h,
                       0, 0, BLUR_TEXTURE_W, BLUR_TEXTURE_H,
                       GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    
    //-NOTE(tbt): blur for bloom
    glUseProgram(global_rcx.shaders.blur);
    
    // NOTE(tbt): apply first (horizontal) blur pass
    glBindFramebuffer(GL_FRAMEBUFFER, blur_framebuffer_2->target);
    glUniform2f(global_rcx.uniform_locations.blur.direction, 1.0f, 0.0f);
    glBindTexture(GL_TEXTURE_2D, blur_framebuffer_1->texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // NOTE(tbt): apply second (vertical) blur pass
    glBindFramebuffer(GL_FRAMEBUFFER, blur_framebuffer_1->target);
    glUniform2f(global_rcx.uniform_locations.blur.direction, 0.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, blur_framebuffer_2->texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    //-NOTE(tbt): blend back to screen
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    
    glUseProgram(post_shader);
    glUniform1f(uniforms->time, (F32)global_time);
    glUniform1f(uniforms->exposure, message.exposure);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blur_framebuffer_1->texture);
    glUniform1i(uniforms->blur_texture, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.post_processing.texture);
    glUniform1i(uniforms->screen_texture, 1);
    
    glViewport(0, 0, global_rcx.window.w, global_rcx.window.h);
    
    glEnable(GL_SCISSOR_TEST);
    glScissor(message.mask.x,
              global_rcx.window.h - message.mask.y - message.mask.h,
              message.mask.w,
              message.mask.h);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, global_rcx.current_texture);
    glUseProgram(global_rcx.shaders.current);
    
    break;
   }
  }
 }
 
 renderer_flush_batch(&batch);
 
 global_rcx.message_queue.start = NULL;
 global_rcx.message_queue.end = NULL;
 glDisable(GL_SCISSOR_TEST);
}

//
// NOTE(tbt): renderer API
//~

#define MOUSE_WORLD_X ((((F32)input->mouse_x / (F32)global_rcx.window.w) - 0.5f) * SCREEN_W_IN_WORLD_UNITS + global_rcx.camera.x)
#define MOUSE_WORLD_Y ((((F32)input->mouse_y / (F32)global_rcx.window.h) - 0.5f) * (SCREEN_W_IN_WORLD_UNITS * ((F32)global_rcx.window.h / (F32)global_rcx.window.w)) + global_rcx.camera.y)

internal void
renderer_recalculate_world_projection_matrix(void)
{
 persist F32 half_screen_width_in_world_units = (F32)(SCREEN_W_IN_WORLD_UNITS >> 1);
 
 F32 aspect = (F32)global_rcx.window.h / (F32)global_rcx.window.w;
 
 generate_orthographic_projection_matrix(global_world_projection_matrix,
                                         global_rcx.camera.x - half_screen_width_in_world_units,
                                         global_rcx.camera.x + half_screen_width_in_world_units,
                                         global_rcx.camera.y - half_screen_width_in_world_units * aspect,
                                         global_rcx.camera.y + half_screen_width_in_world_units * aspect);
}

internal void
renderer_set_window_size(I32 w, I32 h)
{
 global_rcx.window.w = w;
 global_rcx.window.h = h;
 
 global_rcx.mask_stack[0] = rect(0.0f, 0.0f, w, h);
 
 glViewport(0, 0, w, h);
 
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.post_processing.texture);
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              w,
              h,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glBindTexture(GL_TEXTURE_2D, global_rcx.current_texture);
 
 generate_orthographic_projection_matrix(global_ui_projection_matrix,
                                         0, w,
                                         0, h);
 
 renderer_recalculate_world_projection_matrix();
 
 glUseProgram(global_rcx.shaders.current);
}

internal void
set_camera_position(F32 x,
                    F32 y)
{
 global_rcx.camera.x = x;
 global_rcx.camera.y = y;
 
 renderer_recalculate_world_projection_matrix();
}

#define mask_rectangle(_mask) defer_loop(renderer_push_mask(_mask), renderer_pop_mask())

internal void
renderer_push_mask(Rect mask)
{
 if (global_rcx.mask_stack_size < array_count(global_rcx.mask_stack))
 {
  global_rcx.mask_stack_size += 1;
  global_rcx.mask_stack[global_rcx.mask_stack_size] = mask;
 }
}

internal void
renderer_pop_mask(void)
{
 if (global_rcx.mask_stack_size)
 {
  global_rcx.mask_stack_size -= 1;
 }
}

internal Rect
renderer_current_mask(void)
{
 return global_rcx.mask_stack[global_rcx.mask_stack_size];
}

internal void
renderer_push_projection_matrix(F32 *projection_matrix)
{
}

internal void
draw_rotated_sub_texture(Rect rectangle,
                         F32 angle,
                         Colour colour,
                         Texture *texture,
                         SubTexture sub_texture,
                         U8 sort,
                         F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_draw_rectangle;
 message.rectangle = rectangle;
 message.angle = angle;
 message.colour = colour;
 message.texture = texture->id;
 message.sub_texture = sub_texture;
 message.projection_matrix = projection_matrix;
 message.sort = sort;
 
 renderer_enqueue_message(message);
}

internal void
draw_sub_texture(Rect rectangle,
                 Colour colour,
                 Texture *texture,
                 SubTexture sub_texture,
                 U8 sort,
                 F32 *projection_matrix)
{
 draw_rotated_sub_texture(rectangle, 0.0f, colour, texture, sub_texture, sort, projection_matrix);
}

internal void
fill_rotated_rectangle(Rect rectangle,
                       F32 angle,
                       Colour colour,
                       U8 sort,
                       F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_draw_rectangle;
 message.rectangle = rectangle;
 message.angle = angle;
 message.colour = colour;
 message.texture = global_rcx.flat_colour_texture;
 message.sub_texture = ENTIRE_TEXTURE;
 message.projection_matrix = projection_matrix;
 message.sort = sort;
 
 renderer_enqueue_message(message);
}

internal void
fill_rectangle(Rect rectangle,
               Colour colour,
               U8 sort,
               F32 *projection_matrix)
{
 fill_rotated_rectangle(rectangle, 0.0f, colour, sort, projection_matrix);
}

internal void
stroke_rectangle(Rect rectangle,
                 Colour colour,
                 F32 stroke_width,
                 U8 sort,
                 F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_stroke_rectangle;
 message.rectangle = rectangle;
 message.colour = colour;
 message.stroke_width = stroke_width;
 message.sort = sort;
 message.projection_matrix = projection_matrix;
 
 renderer_enqueue_message(message);
}

internal void
draw_s8(Font *font,
        F32 x, F32 y,
        I32 wrap_width,
        Colour colour,
        S8 string,
        U8 sort,
        F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_draw_text;
 message.font = font;
 message.colour = colour;
 message.rectangle.x = x;
 message.rectangle.y = y;
 message.rectangle.w = wrap_width;
 message.string = string;
 message.sort = sort;
 message.projection_matrix = projection_matrix;
 
 renderer_enqueue_message(message);
}

internal void
draw_gradient(Rect rectangle,
              Gradient gradient,
              U8 sort,
              F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_draw_gradient;
 message.rectangle = rectangle;
 message.projection_matrix = projection_matrix;
 message.sort = sort;
 message.gradient = gradient;
 
 renderer_enqueue_message(message);
}

internal void
blur_screen_region(Rect region,
                   I32 strength,
                   U8 sort)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_blur_screen_region;
 message.rectangle = region;
 message.strength = strength;
 message.sort = sort;
 
 renderer_enqueue_message(message);
}

internal void
do_post_processing(F32 exposure,
                   PostProcessingKind kind,
                   U8 sort)
{
 RenderMessage message = {0};
 
 message.sort = sort;
 message.kind = RENDER_MESSAGE_do_post_processing;
 message.exposure = exposure;
 message.post_processing_kind = kind;
 
 renderer_enqueue_message(message);
}

//
// NOTE(tbt): developer UI
//~

#define UI_ANIMATION_SPEED (5.0 * frametime_in_s)

#define ui_width(_w, _strictness)     defer_loop(ui_push_width((UIDimension){ _w, _strictness }), ui_pop_width())
#define ui_height(_h, _strictness)    defer_loop(ui_push_height((UIDimension){ _h, _strictness }), ui_pop_height())
#define ui_colour(_colour)            defer_loop(ui_push_colour(_colour), ui_pop_colour())
#define ui_background(_colour)        defer_loop(ui_push_background(_colour), ui_pop_background())
#define ui_active_colour(_colour)     defer_loop(ui_push_active_colour(_colour), ui_pop_active_colour())
#define ui_active_background(_colour) defer_loop(ui_push_active_background(_colour), ui_pop_active_background())
#define ui_hot_colour(_colour)        defer_loop(ui_push_hot_colour(_colour), ui_pop_hot_colour())
#define ui_hot_background(_colour)    defer_loop(ui_push_hot_background(_colour), ui_pop_hot_background())
#define ui_indent(_indent)            defer_loop(ui_push_indent(_indent), ui_pop_indent())
#define ui_font(_font)                defer_loop(ui_push_font(_font), ui_pop_font())

internal void
ui_push_width(UIDimension dimension)
{
 if (global_ui_context.w_stack_size < array_count(global_ui_context.w_stack))
 {
  global_ui_context.w_stack_size += 1;
  global_ui_context.w_stack[global_ui_context.w_stack_size] = dimension;
 }
}

internal void
ui_pop_width(void)
{
 if (global_ui_context.w_stack_size)
 {
  global_ui_context.w_stack_size -= 1;
 }
}

internal void
ui_push_height(UIDimension dimension)
{
 if (global_ui_context.h_stack_size < array_count(global_ui_context.h_stack))
 {
  global_ui_context.h_stack_size += 1;
  global_ui_context.h_stack[global_ui_context.h_stack_size] = dimension;
 }
}

internal void
ui_pop_height(void)
{
 if (global_ui_context.h_stack_size)
 {
  global_ui_context.h_stack_size -= 1;
 }
}

internal void
ui_push_colour(Colour colour)
{
 if (global_ui_context.colour_stack_size < array_count(global_ui_context.colour_stack))
 {
  global_ui_context.colour_stack_size += 1;
  global_ui_context.colour_stack[global_ui_context.colour_stack_size] = colour;
 }
}

internal void
ui_pop_colour(void)
{
 if (global_ui_context.colour_stack_size)
 {
  global_ui_context.colour_stack_size -= 1;
 }
}

internal void
ui_push_background(Colour colour)
{
 if (global_ui_context.background_stack_size < array_count(global_ui_context.background_stack))
 {
  global_ui_context.background_stack_size += 1;
  global_ui_context.background_stack[global_ui_context.background_stack_size] = colour;
 }
}

internal void
ui_pop_background(void)
{
 if (global_ui_context.background_stack_size)
 {
  global_ui_context.background_stack_size -= 1;
 }
}

internal void
ui_push_active_colour(Colour colour)
{
 if (global_ui_context.active_colour_stack_size < array_count(global_ui_context.active_colour_stack))
 {
  global_ui_context.active_colour_stack_size += 1;
  global_ui_context.active_colour_stack[global_ui_context.active_colour_stack_size] = colour;
 }
}

internal void
ui_pop_active_colour(void)
{
 if (global_ui_context.active_colour_stack_size)
 {
  global_ui_context.active_colour_stack_size -= 1;
 }
}

internal void
ui_push_active_background(Colour colour)
{
 if (global_ui_context.active_background_stack_size < array_count(global_ui_context.active_background_stack))
 {
  global_ui_context.active_background_stack_size += 1;
  global_ui_context.active_background_stack[global_ui_context.active_background_stack_size] = colour;
 }
}

internal void
ui_pop_active_background(void)
{
 if (global_ui_context.active_background_stack_size)
 {
  global_ui_context.active_background_stack_size -= 1;
 }
}

internal void
ui_push_hot_colour(Colour colour)
{
 if (global_ui_context.hot_colour_stack_size < array_count(global_ui_context.hot_colour_stack))
 {
  global_ui_context.hot_colour_stack_size += 1;
  global_ui_context.hot_colour_stack[global_ui_context.hot_colour_stack_size] = colour;
 }
}

internal void
ui_pop_hot_colour(void)
{
 if (global_ui_context.hot_colour_stack_size)
 {
  global_ui_context.hot_colour_stack_size -= 1;
 }
}

internal void
ui_push_hot_background(Colour colour)
{
 if (global_ui_context.hot_background_stack_size < array_count(global_ui_context.hot_background_stack))
 {
  global_ui_context.hot_background_stack_size += 1;
  global_ui_context.hot_background_stack[global_ui_context.hot_background_stack_size] = colour;
 }
}

internal void
ui_pop_hot_background(void)
{
 if (global_ui_context.hot_background_stack_size)
 {
  global_ui_context.hot_background_stack_size -= 1;
 }
}

internal void
ui_push_indent(F32 indent)
{
 if (global_ui_context.indent_stack_size < array_count(global_ui_context.indent_stack))
 {
  global_ui_context.indent_stack_size += 1;
  global_ui_context.indent_stack[global_ui_context.indent_stack_size] = indent;
 }
}

internal void
ui_pop_indent(void)
{
 if (global_ui_context.indent_stack_size)
 {
  global_ui_context.indent_stack_size -= 1;
 }
}

internal void
ui_push_font(Font *font)
{
 if (global_ui_context.font_stack_size < array_count(global_ui_context.font_stack))
 {
  global_ui_context.font_stack_size += 1;
  global_ui_context.font_stack[global_ui_context.font_stack_size] = font;
 }
}

internal void
ui_pop_font(void)
{
 if (global_ui_context.font_stack_size)
 {
  global_ui_context.font_stack_size -= 1;
 }
}

internal void
ui_push_insertion_point(UIWidget *widget)
{
 widget->next_insertion_point = global_ui_context.insertion_point;
 global_ui_context.insertion_point = widget;
}

internal void
ui_pop_insertion_point(void)
{
 if (global_ui_context.insertion_point)
 {
  global_ui_context.insertion_point = global_ui_context.insertion_point->next_insertion_point;
 }
}

internal void
ui_set_styles(UIWidget *widget)
{
 widget->w = global_ui_context.w_stack[global_ui_context.w_stack_size];
 widget->h = global_ui_context.h_stack[global_ui_context.h_stack_size];
 widget->colour = global_ui_context.colour_stack[global_ui_context.colour_stack_size];
 widget->background = global_ui_context.background_stack[global_ui_context.background_stack_size];
 widget->hot_colour = global_ui_context.hot_colour_stack[global_ui_context.hot_colour_stack_size];
 widget->hot_background = global_ui_context.hot_background_stack[global_ui_context.hot_background_stack_size];
 widget->active_colour = global_ui_context.active_colour_stack[global_ui_context.active_colour_stack_size];
 widget->active_background = global_ui_context.active_background_stack[global_ui_context.active_background_stack_size];
 widget->indent = global_ui_context.indent_stack[global_ui_context.indent_stack_size];
 widget->font = global_ui_context.font_stack[global_ui_context.font_stack_size];
}

internal void
ui_insert_widget(UIWidget *widget)
{
 widget->parent = global_ui_context.insertion_point;
 if (global_ui_context.insertion_point->last_child)
 {
  global_ui_context.insertion_point->last_child->next_sibling = widget;
 }
 else
 {
  global_ui_context.insertion_point->first_child = widget;
 }
 global_ui_context.insertion_point->last_child = widget;
 
 global_ui_context.insertion_point->child_count += 1;
}

internal UIWidget *
ui_widget_from_string(S8 identifier)
{
 UIWidget *result = NULL;
 
 S8List *identifier_list = NULL;
 identifier_list = push_s8_to_list(&global_frame_memory,
                                   identifier_list,
                                   identifier);
 identifier_list = push_s8_to_list(&global_frame_memory,
                                   identifier_list,
                                   s8_lit("\\"));
 identifier_list = push_s8_to_list(&global_frame_memory,
                                   identifier_list,
                                   global_ui_context.insertion_point->key);
 S8 _identifier = join_s8_list(&global_frame_memory, identifier_list);
 
 U64 index = hash_s8(_identifier,
                     array_count(global_ui_context.widget_dict));
 
 UIWidget *chain = global_ui_context.widget_dict + index;
 
 if (s8_match(chain->key, _identifier))
  // NOTE(tbt): matching widget directly in hash table slot
 {
  result = chain;
 }
 else if (NULL == chain->key.buffer)
  // NOTE(tbt): hash table slot unused
 {
  result = chain;
  result->key = copy_s8(&global_static_memory, _identifier);
 }
 else
 {
  // NOTE(tbt): look through the chain for a matching widget
  for (UIWidget *widget = chain;
       NULL != widget;
       widget = widget->next_hash)
  {
   if (s8_match(widget->key, _identifier))
   {
    result = widget;
    break;
   }
  }
  
  // NOTE(tbt): push a new widget to the chain if a match is not found
  if (NULL == result)
  {
   result = arena_push(&global_static_memory, sizeof(*result));
   result->next_hash = global_ui_context.widget_dict[index].next_hash;
   result->key = copy_s8(&global_static_memory, _identifier);
   global_ui_context.widget_dict[index].next_hash = result;
  }
 }
 
 // NOTE(tbt): copy styles from context style stacks
 ui_set_styles(result);
 
 // NOTE(tbt): reset per frame data
 result->first_child = NULL;
 result->last_child = NULL;
 result->next_sibling = NULL;
 result->next_insertion_point = NULL;
 result->next_keyboard_focus = NULL;
 result->child_count = 0;
 result->temp_string.buffer = result->temp_string_buffer;
 
 ui_insert_widget(result);
 
 return result;
}

internal void
ui_update_widget(UIWidget *widget)
{
 PlatformState *input = global_ui_context.input;
 F64 frametime_in_s = global_ui_context.frametime_in_s;
 
 if ((widget->flags & UI_WIDGET_FLAG_draw_label ||
      widget->flags & UI_WIDGET_FLAG_draw_cursor) &&
     NULL == widget->label.buffer)
 {
  widget->label = s8_lit("ERROR: label was NULL");
 }
 
 widget->clicked = false;
 
 if (widget->flags & UI_WIDGET_FLAG_keyboard_focusable)
 {
  if (global_ui_context.last_keyboard_focus)
  {
   global_ui_context.last_keyboard_focus->next_keyboard_focus = widget;
  }
  else
  {
   global_ui_context.first_keyboard_focus = widget;
  }
  widget->prev_keyboard_focus = global_ui_context.last_keyboard_focus;
  global_ui_context.last_keyboard_focus = widget;
  
  if (widget == global_ui_context.keyboard_focus)
  {
   if (widget->flags & UI_WIDGET_FLAG_clickable &&
       is_key_pressed(input, KEY_enter, 0))
   {
    widget->clicked = true;
   }
   
   widget->keyboard_focused_transition = min_f(widget->keyboard_focused_transition + UI_ANIMATION_SPEED, 1.0f);
  }
  else
  {
   widget->keyboard_focused_transition = max_f(widget->keyboard_focused_transition - UI_ANIMATION_SPEED, 0.0f);
  }
 }
 
 if (widget == global_ui_context.hot)
 {
  widget->hot += UI_ANIMATION_SPEED;
  
  if (is_mouse_button_pressed(input, MOUSE_BUTTON_left, 0))
  {
   if (widget->flags & UI_WIDGET_FLAG_clickable)
   {
    widget->active = true;
    widget->clicked = true;
    cm_play(global_click_sound);
   }
   
   if (widget->flags & UI_WIDGET_FLAG_draggable_x ||
       widget->flags & UI_WIDGET_FLAG_draggable_y)
   {
    widget->active = true;
    widget->drag_x_offset = input->mouse_x - widget->layout.x;
    widget->drag_y_offset = input->mouse_y - widget->layout.y;
   }
  }
  else
  {
   if (widget->flags & UI_WIDGET_FLAG_clickable)
   {
    widget->active -= UI_ANIMATION_SPEED;
   }
  }
 }
 else
 {
  widget->hot -= UI_ANIMATION_SPEED;
  if (!(widget->flags & UI_WIDGET_FLAG_draggable))
  {
   widget->active = false;
  }
 }
 
 if (widget->active &&
     widget->flags & UI_WIDGET_FLAG_draggable)
 {
  if (widget->flags & UI_WIDGET_FLAG_draggable_x)
  {
   widget->drag_x = input->mouse_x - widget->drag_x_offset - widget->parent->layout.x;
  }
  if (widget->flags & UI_WIDGET_FLAG_draggable_y)
  {
   widget->drag_y = input->mouse_y - widget->drag_y_offset - widget->parent->layout.y;
  }
  
  if (!input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   widget->active = false;
  }
 }
 
 if (widget->flags & UI_WIDGET_FLAG_toggled_effect)
 {
  if (widget->toggled)
  {
   widget->toggled_transition = min_f(widget->toggled_transition + UI_ANIMATION_SPEED, 1.0f);
  }
  else
  {
   widget->toggled_transition = max_f(widget->toggled_transition - UI_ANIMATION_SPEED, 0.0f);
  }
 }
 
 if (widget->flags & UI_WIDGET_FLAG_draw_cursor)
 {
  F32 cursor_width = 2.0f;
  
  Rect cursor, selection;
  
  F32 line_start = widget->layout.x + global_ui_context.padding;
  F32 x = widget->layout.x + global_ui_context.padding;
  
  I32 font_bake_begin = widget->font->bake_begin;
  I32 font_bake_end = widget->font->bake_end;
  
  cursor.x = selection.x = x - cursor_width / 2;
  cursor.y = selection.y = widget->layout.y + global_ui_context.padding;
  cursor.w = cursor_width;
  cursor.h = selection.h = widget->font->vertical_advance - global_ui_context.padding;
  
  I32 i = 0;
  for (UTF8Consume consume = consume_utf8_from_string(widget->label, i);
       i < widget->label.size;
       i += consume.advance, consume = consume_utf8_from_string(widget->label, i))
  {
   if (consume.codepoint == '\n')
   {
    x = line_start;
    cursor.y += widget->font->vertical_advance;
    selection.y += widget->font->vertical_advance;
   }
   else if (consume.codepoint >= font_bake_begin &&
            consume.codepoint < font_bake_end)
   {
    x += widget->font->char_data[consume.codepoint - font_bake_begin].xadvance;
    if (i == widget->cursor - 1)
    {
     cursor.x = x;
    }
    if (i == widget->mark - 1)
    {
     selection.x = x;
    }
   }
  }
  selection.w = (cursor.x + cursor.w) - selection.x;
  
  widget->text_cursor_rect = cursor;
  widget->text_selection_rect = selection;
 }
 
 if (is_point_in_rect(input->mouse_x, input->mouse_y, widget->interactable) &&
     widget->flags & UI_WIDGET_FLAG_scrollable)
 {
  // TODO(tbt): x axis scrolling
  F32 scroll_speed = 12.0f;
  widget->scroll += input->mouse_scroll_v * scroll_speed;
  
  widget->scroll_max = max_f(widget->measure.children_total_h - widget->layout.h - widget->measure.children_total_h_can_loose, 0.0f);
  
  widget->scroll = clamp_f(widget->scroll, -(widget->scroll_max), 0.0f);
 }
 
 widget->hot = clamp_f(widget->hot, 0.0f, 1.0f);
 widget->active = clamp_f(widget->active, 0.0f, 1.0f);
}

internal void
ui_measurement_pass(UIWidget *root)
{
 root->measure.w = root->w.dim;
 root->measure.h = root->h.dim;
 root->measure.children_total_w = global_ui_context.padding;
 root->measure.children_total_h = global_ui_context.padding;
 root->measure.children_total_w_can_loose = 0.0f;
 root->measure.children_total_h_can_loose = 0.0f;
 root->measure.w_can_loose = (1.0f - root->w.strictness) * root->measure.w;
 root->measure.h_can_loose = (1.0f - root->h.strictness) * root->measure.h;
 
 for (UIWidget *child = root->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  ui_measurement_pass(child);
  root->measure.children_total_w += child->measure.w + global_ui_context.padding + child->indent;
  root->measure.children_total_h += child->measure.h + global_ui_context.padding;
  root->measure.children_total_w_can_loose += child->measure.w_can_loose;
  root->measure.children_total_h_can_loose += child->measure.h_can_loose;
 }
}

internal void
ui_layout_pass(UIWidget *root,
               F32 x, F32 y)
{
 x += root->indent;
 
 if (root->flags & UI_WIDGET_FLAG_draggable_x)
 {
  x = root->parent->layout.x + root->drag_x;
 }
 if (root->flags & UI_WIDGET_FLAG_draggable_y)
 {
  y = root->parent->layout.y + root->drag_y;
 }
 
 root->layout = rect(x, y, root->measure.w, root->measure.h);
 
 y += root->scroll;
 
 if (root->parent)
 {
  root->interactable = rect_at_intersection(root->layout,
                                            root->parent->interactable);
 }
 else
 {
  root->interactable = root->layout;
 }
 
 if (root->children_placement == UI_LAYOUT_PLACEMENT_vertical)
 {
  y += root->scroll;
  
  F32 to_loose = max_f(root->measure.children_total_h - root->layout.h, 0.0f);
  
  F32 proportion = 0.0f;
  if (root->measure.children_total_h_can_loose > 0.0f &&
      to_loose > 0.0f)
  {
   proportion = clamp_f(to_loose / root->measure.children_total_h_can_loose, 0.0f, 1.0f);
  }
  
  for (UIWidget *child = root->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   child->measure.h -= child->measure.h_can_loose * proportion;
   if (child->measure.w > root->measure.w)
   {
    child->measure.w -= min_f(child->measure.w - root->measure.w, child->measure.w_can_loose);
   }
   ui_layout_pass(child, x, y);
   y += child->layout.h + global_ui_context.padding;
  }
 }
 else if (root->children_placement == UI_LAYOUT_PLACEMENT_horizontal)
 {
  F32 to_loose = root->measure.children_total_w - root->measure.w;
  
  F32 proportion = 0.0f;
  if (root->measure.children_total_w_can_loose > 0.0f &&
      to_loose > 0.0f)
  {
   proportion = clamp_f(to_loose / root->measure.children_total_w_can_loose, 0.0f, 1.0f);
  }
  
  for (UIWidget *child = root->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   child->measure.w -= child->measure.w_can_loose * proportion;
   if (child->measure.h > root->measure.h)
   {
    child->measure.h -= min_f(child->measure.h - root->measure.h, child->measure.h_can_loose);
   }
   ui_layout_pass(child, x, y);
   x += child->layout.w + global_ui_context.padding;
  }
 }
}

internal void
ui_render_pass(UIWidget *root)
{
 Colour foreground = root->colour;
 Colour background = root->background;
 
 if (root->flags & UI_WIDGET_FLAG_toggled_effect)
 {
  foreground = colour_lerp(foreground, root->hot_colour, (F32)(1 - root->toggled_transition));
  background = colour_lerp(background, root->hot_background, (F32)(1 - root->toggled_transition));
 }
 if (root->flags & UI_WIDGET_FLAG_keyboard_focusable)
 {
  foreground = colour_lerp(foreground, root->active_colour, 1.0f - root->keyboard_focused_transition);
  background = colour_lerp(background, root->active_background, 1.0f - root->keyboard_focused_transition);
 }
 if (root->flags & UI_WIDGET_FLAG_hot_effect)
 {
  foreground = colour_lerp(foreground, root->hot_colour, 1.0f - (root->hot * root->hot));
  background = colour_lerp(background, root->hot_background, 1.0f - (root->hot * root->hot));
 }
 if (root->flags & UI_WIDGET_FLAG_active_effect)
 {
  foreground = colour_lerp(foreground, root->active_colour, 1.0f - root->active);
  background = colour_lerp(background, root->active_background, 1.0f - root->active);
 }
 
 F32 offset_scale = 5.0f;
 F32 x_offset = 0;
 x_offset += !!(root->flags & UI_WIDGET_FLAG_hot_effect) * root->hot * root->hot;
 x_offset += !!(root->flags & UI_WIDGET_FLAG_toggled_effect) * root->toggled_transition;
 x_offset += !!(root->flags & UI_WIDGET_FLAG_keyboard_focusable) * root->keyboard_focused_transition;
 
 Rect final_rectangle = offset_rect(root->layout,
                                    x_offset * offset_scale,
                                    0.0f);
 
 Rect mask;
 if (root->flags & UI_WIDGET_FLAG_do_not_mask_children)
 {
  mask = renderer_current_mask();
 }
 else
 {
  mask = rect_at_intersection(offset_rect(root->interactable,
                                          x_offset * offset_scale,
                                          0.0f),
                              renderer_current_mask());
 }
 
 if (root->flags & UI_WIDGET_FLAG_blur_background)
 {
  blur_screen_region(final_rectangle, 1, UI_SORT_DEPTH);
 }
 
 if (root->flags & UI_WIDGET_FLAG_draw_background)
 {
  fill_rectangle(final_rectangle, background, UI_SORT_DEPTH, global_ui_projection_matrix);
 }
 
 if (root->flags & UI_WIDGET_FLAG_draw_label)
 {
  mask_rectangle(mask) draw_s8(root->font,
                               final_rectangle.x + global_ui_context.padding,
                               final_rectangle.y + root->font->vertical_advance,
                               -1.0f,
                               foreground,
                               root->label,
                               UI_SORT_DEPTH,
                               global_ui_projection_matrix);
 }
 
 if (root->flags & UI_WIDGET_FLAG_draw_cursor)
 {
  if (global_ui_context.keyboard_focus == root)
  {
   Rect cursor = offset_rect(root->text_cursor_rect, x_offset * offset_scale, 0.0f); 
   Rect selection = offset_rect(root->text_selection_rect, x_offset * offset_scale, 0.0f); 
   
   fill_rectangle(selection,
                  col(0.18f, 0.24f, 0.76f, 0.5f),
                  UI_SORT_DEPTH,
                  global_ui_projection_matrix);
   
   fill_rectangle(cursor,
                  foreground,
                  UI_SORT_DEPTH,
                  global_ui_projection_matrix);
  }
 }
 
 if (root->flags & UI_WIDGET_FLAG_draw_outline)
 {
  stroke_rectangle(final_rectangle,
                   foreground,
                   global_ui_context.stroke_width,
                   UI_SORT_DEPTH,
                   global_ui_projection_matrix);
 }
 
 if (global_ui_context.keyboard_focus == root)
 {
  stroke_rectangle(final_rectangle,
                   global_ui_context.keyboard_focus_highlight,
                   global_ui_context.stroke_width,
                   UI_SORT_DEPTH,
                   global_ui_projection_matrix);
 }
 
 mask_rectangle(mask)
 {
  for (UIWidget *child = root->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   ui_render_pass(child);
  }
 }
}

internal void
ui_recursively_find_hot_widget(PlatformState *input,
                               UIWidget *root)
{
 if (is_point_in_rect(input->mouse_x,
                      input->mouse_y,
                      root->interactable) &&
     !(root->flags & UI_WIDGET_FLAG_no_input))
 {
  global_ui_context.hot = root;
 }
 
 for (UIWidget *child = root->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  ui_recursively_find_hot_widget(input, child);
 }
}

internal void
ui_defered_input(PlatformState *input)
{
 ui_recursively_find_hot_widget(global_ui_context.input,
                                &global_ui_context.root);
 
 for (PlatformEvent *e = input->events;
      NULL != e;
      e = e->next)
 {
  if (e->kind == PLATFORM_EVENT_key_press)
  {
   if (e->key == KEY_tab)
   {
    if (global_ui_context.keyboard_focus)
    {
     if (e->modifiers & INPUT_MODIFIER_shift)
     {
      global_ui_context.keyboard_focus = global_ui_context.keyboard_focus->prev_keyboard_focus;
     }
     else
     {
      global_ui_context.keyboard_focus = global_ui_context.keyboard_focus->next_keyboard_focus;
     }
    }
    else
    {
     if (e->modifiers & INPUT_MODIFIER_shift)
     {
      global_ui_context.keyboard_focus = global_ui_context.last_keyboard_focus;
     }
     else
     {
      global_ui_context.keyboard_focus = global_ui_context.first_keyboard_focus;
     }
    }
   }
   else if (e->key == KEY_esc)
   {
    global_ui_context.keyboard_focus = NULL;
   }
  }
 }
 if (global_ui_context.input->is_mouse_button_down[MOUSE_BUTTON_left])
 {
  if (!global_ui_context.hot ||
      !(global_ui_context.hot->flags & UI_WIDGET_FLAG_keyboard_focusable))
  {
   global_ui_context.keyboard_focus = NULL;
  }
  else if (global_ui_context.keyboard_focus)
  {
   global_ui_context.keyboard_focus = global_ui_context.hot;
  }
 }
}

internal void
ui_initialise(void)
{
 // NOTE(tbt): fill bottoms of style stacks with default values
 global_ui_context.w_stack[0] = (UIDimension){ 999999999.0f, 0.0f };
 global_ui_context.h_stack[0] = (UIDimension){ 999999999.0f, 0.0f };
 global_ui_context.colour_stack[0] = col(1.0f, 0.967f, 0.982f, 1.0f);
 global_ui_context.background_stack[0] = col(0.0f, 0.0f, 0.0f, 0.8f);
 global_ui_context.hot_colour_stack[0] = col(0.0f, 0.0f, 0.0f, 0.5f);
 global_ui_context.hot_background_stack[0] = col(1.0f, 0.967f, 0.982f, 1.0f);
 global_ui_context.active_colour_stack[0] = col(1.0f, 0.967f, 0.982f, 1.0f);
 global_ui_context.active_background_stack[0] = col(0.018f, 0.018f, 0.018f, 0.8f);
 global_ui_context.indent_stack[0] = 0.0f;
 global_ui_context.font_stack[0] = global_ui_font;
 
 // NOTE(tbt): initialise config
 global_ui_context.padding = 8.0f;
 global_ui_context.stroke_width = 2.0f;
 global_ui_context.keyboard_focus_highlight= col(0.18f, 0.24f, 0.76f, 0.5f);
}

internal void
ui_prepare(PlatformState *input,
           F64 frametime_in_s)
{
 global_ui_context.insertion_point = &global_ui_context.root;
 global_ui_context.input = input;
 global_ui_context.frametime_in_s = frametime_in_s;
 global_ui_context.first_keyboard_focus = NULL;
 global_ui_context.last_keyboard_focus = NULL;
 memset(&global_ui_context.root, 0, sizeof(global_ui_context.root));
}

internal void
ui_finish(void)
{
 PlatformState *input = global_ui_context.input;
 
 global_ui_context.hot = NULL;
 global_ui_context.root.layout = rect(0.0f, 0.0f, global_rcx.window.w, global_rcx.window.h);
 global_ui_context.root.w = (UIDimension){ input->window_w, 1.0f };
 global_ui_context.root.h = (UIDimension){ input->window_h, 1.0f };
 ui_defered_input(input);
 ui_measurement_pass(&global_ui_context.root);
 ui_layout_pass(&global_ui_context.root, 0.0f, 0.0f);
 ui_render_pass(&global_ui_context.root);
}

internal void
ui_label(S8 text)
{
 UIWidget *widget = ui_widget_from_string(text);
 
 widget->flags |= UI_WIDGET_FLAG_draw_label;
 widget->flags |= UI_WIDGET_FLAG_no_input;
 widget->label = text;
 
 ui_update_widget(widget);
}


#define ui_row() defer_loop(ui_push_layout(UI_LAYOUT_PLACEMENT_horizontal), ui_pop_insertion_point())

#define ui_column() defer_loop(ui_push_layout(UI_LAYOUT_PLACEMENT_vertical), ui_pop_insertion_point())

internal void
ui_push_layout(UILayoutPlacement advancement_direction)
{
 UIWidget *widget = ui_widget_from_string(s8_from_format_string(&global_frame_memory,
                                                                "__child_%u",
                                                                global_ui_context.insertion_point->child_count));
 widget->flags |= UI_WIDGET_FLAG_no_input;
 widget->flags |= UI_WIDGET_FLAG_do_not_mask_children;
 widget->children_placement = advancement_direction;
 ui_update_widget(widget);
 ui_push_insertion_point(widget);
}

internal B32
ui_button_with_id(S8 identifier,
                  S8 text,
                  B32 highlighted)
{
 UIWidget *widget;
 
 ui_background(col(0.0f, 0.0f, 0.0f, 0.0f))
 {
  widget = ui_widget_from_string(identifier);
  widget->flags |= UI_WIDGET_FLAG_draw_background;
  widget->flags |= UI_WIDGET_FLAG_draw_label;
  widget->flags |= UI_WIDGET_FLAG_draw_outline;
  widget->flags |= UI_WIDGET_FLAG_hot_effect;
  widget->flags |= UI_WIDGET_FLAG_active_effect;
  widget->flags |= UI_WIDGET_FLAG_toggled_effect;
  widget->flags |= UI_WIDGET_FLAG_clickable;
  widget->flags |= UI_WIDGET_FLAG_keyboard_focusable;
  widget->label = text;
  widget->toggled = highlighted;
  ui_update_widget(widget);
 }
 
 return widget->clicked;
}

internal B32
ui_highlightable_button(S8 text,
                        B32 highlighted)
{
 return ui_button_with_id(text, text, highlighted);
}

internal B32
ui_button(S8 text)
{
 return ui_highlightable_button(text, false);
}

internal void
ui_toggle_button(S8 text,
                 B32 *toggled)
{
 if (ui_highlightable_button(text, *toggled))
 {
  *toggled = !(*toggled);
 }
}

internal B32
ui_bit_toggle_button(S8 text,
                     U64 *flags, U8 bit)
{
 B32 bit_on = (*flags) & (1 << bit);
 
 if (ui_highlightable_button(text, bit_on))
 {
  if (bit_on)
  {
   *flags &= ~(1 << bit);
  }
  else
  {
   *flags |= (1 << bit);
  }
 }
 
 return bit_on;
}

// TODO(tbt): api managed version with variable length strings
internal B32
ui_line_edit(S8 identifier,
             S8 *result,
             U64 capacity)
{
 PlatformState *input = global_ui_context.input;
 
 F32 line_height = global_ui_context.font_stack[global_ui_context.font_stack_size]->vertical_advance;
 line_height += global_ui_context.padding;
 
 UIWidget *widget;
 ui_height(line_height, 1.0f) widget = ui_widget_from_string(identifier);
 
 if (widget->clicked)
 {
  global_ui_context.keyboard_focus = widget;
 }
 
 widget->label = *result;
 widget->cursor = min_u(widget->label.size, widget->cursor);
 widget->mark = min_u(widget->label.size, widget->mark);
 
 U8 character_insertion_buffer[4];
 
 enum
 {
  ACTION_FLAG_move_cursor = 1 << 0,
  ACTION_FLAG_move_mark   = 1 << 1,
  ACTION_FLAG_set_cursor  = 1 << 2,
  ACTION_FLAG_stick_mark  = 1 << 3,
  ACTION_FLAG_delete      = 1 << 4,
  ACTION_FLAG_insert      = 1 << 5,
  ACTION_FLAG_copy        = 1 << 6,
 };
 
 struct
 {
  U8 flags;
  CursorAdvanceMode advance_mode;
  CursorAdvanceDirection advance_direction;
  I32 set_cursor_to;
  S8 to_insert;
 } action = {0};
 
 if (global_ui_context.keyboard_focus == widget)
 {
  for (PlatformEvent *e = input->events;
       NULL != e;
       e = e->next)
  {
   if (e->kind == PLATFORM_EVENT_key_typed)
   {
    U64 size = utf8_from_codepoint(character_insertion_buffer, e->character);
    if (widget->cursor + size + (widget->label.size - widget->cursor) <= capacity)
    {
     action.flags = ACTION_FLAG_move_cursor | ACTION_FLAG_stick_mark | ACTION_FLAG_insert;
     action.advance_direction = CURSOR_ADVANCE_DIRECTION_forwards;
     action.to_insert.buffer = character_insertion_buffer;
     action.to_insert.size = size;
     
     if (widget->cursor != widget->mark)
     {
      // TODO(tbt): ideally this wouldn't be a hard coded special case
      U64 start = min_u(widget->cursor, widget->mark);
      U64 end = max_u(widget->cursor, widget->mark);
      
      memcpy(widget->label.buffer + start,
             widget->label.buffer + end,
             widget->label.size - end);
      widget->label.size -= end - start;
      widget->cursor = start;
     }
    }
   }
   else if (e->kind == PLATFORM_EVENT_key_press)
   {
    B32 range_selected = widget->cursor != widget->mark;
    if (e->key == KEY_left) 
    {
     action.flags = ACTION_FLAG_move_cursor;
     action.advance_direction = CURSOR_ADVANCE_DIRECTION_backwards;
    }
    else if (e->key == KEY_right)
    {
     action.flags = ACTION_FLAG_move_cursor;
     action.advance_direction = CURSOR_ADVANCE_DIRECTION_forwards;
    }
    else if (e->key == KEY_backspace)
    {
     action.flags = ACTION_FLAG_delete;
     if (!range_selected)
     {
      action.flags |= ACTION_FLAG_move_cursor | ACTION_FLAG_stick_mark;
      action.advance_direction = CURSOR_ADVANCE_DIRECTION_backwards;
     }
    }
    else if (e->key == KEY_delete)
    {
     action.flags = ACTION_FLAG_delete;
     if (!range_selected)
     {
      action.flags |= ACTION_FLAG_move_mark | ACTION_FLAG_stick_mark;
      action.advance_direction = CURSOR_ADVANCE_DIRECTION_forwards;
     }
    }
    else if (e->key == KEY_home)
    {
     action.flags = ACTION_FLAG_set_cursor;
     action.set_cursor_to = 0;
    }
    else if (e->key == KEY_end)
    {
     action.flags = ACTION_FLAG_set_cursor;
     action.set_cursor_to = widget->label.size;
    }
    else if (e->key == KEY_c && (e->modifiers & INPUT_MODIFIER_ctrl))
    {
     action.flags = ACTION_FLAG_copy;
    }
    else if (e->key == KEY_x && (e->modifiers & INPUT_MODIFIER_ctrl))
    {
     action.flags = ACTION_FLAG_copy | ACTION_FLAG_delete;
    }
    else if (e->key == KEY_v && (e->modifiers & INPUT_MODIFIER_ctrl))
    {
     action.flags = ACTION_FLAG_insert | ACTION_FLAG_stick_mark | ACTION_FLAG_set_cursor;
     action.to_insert = platform_get_clipboard_text(&global_frame_memory);
     action.set_cursor_to = widget->cursor + action.to_insert.size;
    }
    else { goto skip_action; }
    
    if (e->modifiers & INPUT_MODIFIER_ctrl) { action.advance_mode = CURSOR_ADVANCE_MODE_word; }
    else                                    { action.advance_mode = CURSOR_ADVANCE_MODE_char; }
    if (!(e->modifiers & INPUT_MODIFIER_shift)) { action.flags |= ACTION_FLAG_stick_mark; }
   }
  }
  
  if (action.flags & ACTION_FLAG_move_mark)
  {
   advance_cursor(widget->label,
                  action.advance_direction,
                  action.advance_mode,
                  &widget->mark);
  }
  
  if (action.flags & ACTION_FLAG_copy)
  {
   U64 start = min_u(widget->cursor, widget->mark);
   U64 end = max_u(widget->cursor, widget->mark);
   
   S8 to_copy = widget->label;
   to_copy.buffer += start;
   to_copy.size = end - start;
   platform_set_clipboard_text(to_copy);
  }
  
  if (action.flags & ACTION_FLAG_insert)
  {
   {
    U64 index_to_copy_to = widget->cursor + action.to_insert.size;
    memcpy(widget->label.buffer + index_to_copy_to,
           widget->label.buffer + widget->cursor,
           capacity - index_to_copy_to);
   }
   memcpy(widget->label.buffer + widget->cursor,
          action.to_insert.buffer,
          action.to_insert.size);
   widget->label.size += action.to_insert.size;
  }
  
  if (action.flags & ACTION_FLAG_set_cursor)
  {
   widget->cursor = action.set_cursor_to;
  }
  
  if (action.flags & ACTION_FLAG_move_cursor)
  {
   advance_cursor(widget->label,
                  action.advance_direction,
                  action.advance_mode,
                  &widget->cursor);
  }
  
  if (action.flags & ACTION_FLAG_delete)
  {
   U64 start = min_u(widget->cursor, widget->mark);
   U64 end = max_u(widget->cursor, widget->mark);
   
   memcpy(widget->label.buffer + start,
          widget->label.buffer + end,
          widget->label.size - end);
   widget->label.size -= end - start;
   widget->cursor = start;
  }
  
  if (action.flags & ACTION_FLAG_stick_mark)
  {
   widget->mark = widget->cursor;
  }
 }
 
 skip_action:
 
 widget->flags |= UI_WIDGET_FLAG_draw_background;
 widget->flags |= UI_WIDGET_FLAG_draw_label;
 widget->flags |= UI_WIDGET_FLAG_draw_outline;
 widget->flags |= UI_WIDGET_FLAG_draw_cursor;
 widget->flags |= UI_WIDGET_FLAG_active_effect;
 widget->flags |= UI_WIDGET_FLAG_clickable;
 widget->flags |= UI_WIDGET_FLAG_keyboard_focusable;
 ui_update_widget(widget);
 
 *result = widget->label;
 
 return (global_ui_context.keyboard_focus == widget);
}

internal inline void
ui_update_slider_f32(F32 *value,
                     F32 min, F32 max,
                     UIWidget *thumb,
                     UIWidget *track)
{
 if (thumb->active)
 {
  // NOTE(tbt): calculate value from drag position
  *value = ((thumb->layout.x - track->layout.x) / (track->layout.w - thumb->layout.w)) * (max - min) + min;
 }
 else
 {
  // NOTE(tbt): calculate drag position from value
  thumb->drag_x = ((*value - min) / (max - min)) * (track->layout.w - thumb->layout.w);
 }
 
 thumb->drag_x = clamp_f(thumb->drag_x, 0.0f, track->layout.w - thumb->layout.w);
}

internal void
ui_h_slider_f32(S8 identifier,
                F32 *value,
                F32 min, F32 max)
{
 F32 slider_thickness = 16.0f, slider_thumb_size = 24.0f;
 
 UIWidget *track, *thumb, *text_edit;
 
 ui_height(slider_thickness, 1.0f)
 {
  track = ui_widget_from_string(identifier);
  track->flags |= UI_WIDGET_FLAG_draw_outline;
  ui_update_widget(track);
  ui_push_insertion_point(track);
  {
   ui_width(slider_thumb_size, 1.0f)
   {
    thumb = ui_widget_from_string(s8_lit("__thumb"));
    thumb->flags |= UI_WIDGET_FLAG_draw_outline;
    thumb->flags |= UI_WIDGET_FLAG_draw_background;
    thumb->flags |= UI_WIDGET_FLAG_hot_effect;
    thumb->flags |= UI_WIDGET_FLAG_active_effect;
    thumb->flags |= UI_WIDGET_FLAG_draggable_x;
    ui_update_widget(thumb);
   }
  } ui_pop_insertion_point();
 }
 
 ui_update_slider_f32(value, min, max, thumb, track);
}

internal void
ui_h_slider_text_edit_f32(S8 identifier,
                          F32 *value,
                          F32 min, F32 max,
                          UIDimension text_edit_w)
{
 UIWidget *widget = ui_widget_from_string(identifier);
 widget->flags |= UI_WIDGET_FLAG_no_input;
 widget->flags |= UI_WIDGET_FLAG_do_not_mask_children;
 widget->children_placement = UI_LAYOUT_PLACEMENT_horizontal;
 ui_update_widget(widget);
 
 ui_push_insertion_point(widget);
 {
  ui_h_slider_f32(s8_lit("__slider"), value, min, max);
  
  {
   ui_push_width(text_edit_w);
   if (ui_line_edit(s8_lit("__text_edit"), &widget->temp_string, sizeof(widget->temp_string_buffer)))
   {
    *value = f64_from_s8(widget->temp_string);
   }
   else
   {
    widget->temp_string.size = min_u(snprintf(widget->temp_string_buffer, sizeof(widget->temp_string_buffer), "%f", *value),
                                     sizeof(widget->temp_string_buffer) - 1);
   }
  }
  ui_pop_width();
 } ui_pop_insertion_point();
}

internal void
ui_v_slider_f32(S8 identifier,
                F32 *value,
                F32 min, F32 max)
{
 F32 slider_thickness = 16.0f, slider_thumb_size = 24.0f;
 
 UIWidget *track, *thumb, *text_edit;
 
 ui_width(slider_thickness, 1.0f)
 {
  track = ui_widget_from_string(identifier);
  track->flags |= UI_WIDGET_FLAG_draw_outline;
  ui_update_widget(track);
  ui_push_insertion_point(track);
  {
   ui_height(slider_thumb_size, 1.0f)
   {
    thumb = ui_widget_from_string(s8_lit("__thumb"));
    thumb->flags |= UI_WIDGET_FLAG_draw_outline;
    thumb->flags |= UI_WIDGET_FLAG_draw_background;
    thumb->flags |= UI_WIDGET_FLAG_hot_effect;
    thumb->flags |= UI_WIDGET_FLAG_active_effect;
    thumb->flags |= UI_WIDGET_FLAG_draggable_y;
    ui_update_widget(thumb);
   }
  } ui_pop_insertion_point();
 }
 
 ui_update_slider_f32(value, min, max, thumb, track);
}

#define ui_window(_title) defer_loop((ui_push_insertion_point(&global_ui_context.root), ui_push_window(_title)), (ui_pop_indent(), ui_pop_insertion_point(), ui_pop_insertion_point(), ui_pop_insertion_point()))

internal void
ui_push_window(S8 identifier)
{
 PlatformState *input = global_ui_context.input;
 
 UIWidget *window = ui_widget_from_string(identifier);
 window->flags |= UI_WIDGET_FLAG_draw_background;
 window->flags |= UI_WIDGET_FLAG_blur_background;
 window->flags |= UI_WIDGET_FLAG_draggable;
 ui_update_widget(window);
 
 ui_push_insertion_point(window);
 {
  ui_height(32.0f, 1.0f) ui_indent(0.0f)
  {
   UIWidget *title = ui_widget_from_string(s8_lit("__title"));
   title->flags |= UI_WIDGET_FLAG_draw_background;
   title->flags |= UI_WIDGET_FLAG_draw_label;
   title->flags |= UI_WIDGET_FLAG_no_input;
   title->label = identifier;
   ui_update_widget(title);
  }
  
  ui_indent(global_ui_context.padding)
  {
   UIWidget *body = ui_widget_from_string(s8_lit("__body"));
   body->flags |= UI_WIDGET_FLAG_no_input;
   body->flags |= UI_WIDGET_FLAG_scrollable;
   ui_update_widget(body);
   ui_push_insertion_point(body);
  }
 }
}

//
// NOTE(tbt): dialogue
//~

internal F32 DIALOGUE_FADE_OUT_SPEED = 2.0f;

internal void
play_dialogue(DialogueState *dialogue_state,
              S8List *dialogue,
              F32 x, F32 y,
              Colour colour)
{
 dialogue_state->playing = true;
 dialogue_state->time_playing = 0.0;
 dialogue_state->dialogue = dialogue;
 dialogue_state->x = x;
 dialogue_state->y = y;
 dialogue_state->colour = colour;
}

internal void
do_dialogue(DialogueState *dialogue_state,
            F64 frametime_in_s)
{
 if (dialogue_state->playing)
 {
  dialogue_state->time_playing += frametime_in_s;
  
  dialogue_state->characters_showing = dialogue_state->time_playing / global_current_locale_config.dialogue_seconds_per_character;
  
  S8 string_to_draw;
  string_to_draw.buffer = dialogue_state->dialogue->string.buffer;
  string_to_draw.size = 0;
  
  for(I32 i = 0;
      i < dialogue_state->characters_showing;
      ++i)
  {
   UTF8Consume consume = consume_utf8_from_string(dialogue_state->dialogue->string,
                                                  string_to_draw.size);
   string_to_draw.size += consume.advance;
  }
  
  if (string_to_draw.size >= dialogue_state->dialogue->string.size - 1)
  {
   if (dialogue_state->fade_out_transition < 1.0f)
   {
    dialogue_state->fade_out_transition += DIALOGUE_FADE_OUT_SPEED * frametime_in_s;
   }
   else
   {
    if (dialogue_state->dialogue->next)
    {
     dialogue_state->dialogue = dialogue_state->dialogue->next;
     dialogue_state->fade_out_transition = 1.0f;
     dialogue_state->time_playing = 0.0;
    }
    else
    {
     memset(dialogue_state, 0, sizeof(*dialogue_state));
     dialogue_state->fade_out_transition = 1.0f;
    }
   }
  }
  else
  {
   dialogue_state->fade_out_transition = 0.0f;
  }
  
  draw_s8(global_current_locale_config.normal_font,
          dialogue_state->x,
          dialogue_state->y,
          -1,
          col(dialogue_state->colour.r,
              dialogue_state->colour.g,
              dialogue_state->colour.b,
              dialogue_state->colour.a * (1.0f - dialogue_state->fade_out_transition)),
          string_to_draw,
          UI_SORT_DEPTH - 1,
          global_world_projection_matrix);
 }
}

//
// NOTE(tbt): entities
//~

internal void
load_player_art(void)
{
 global_player.art.texture.path = s8_lit("../assets/textures/player.png");
 
 if (load_texture(&global_player.art.texture))
 {
  global_player.art.forward_head = sub_texture_from_texture(&global_player.art.texture, 16.0f, 16.0f, 88.0f, 175.0f);
  global_player.art.forward_left_arm = sub_texture_from_texture(&global_player.art.texture, 112.0f, 16.0f, 43.0f, 212.0f);
  global_player.art.forward_right_arm = sub_texture_from_texture(&global_player.art.texture, 160.0f, 16.0f, 29.0f, 216.0f);
  global_player.art.forward_lower_body = sub_texture_from_texture(&global_player.art.texture, 192.0f, 16.0f, 131.0f, 320.0f);
  global_player.art.forward_torso = sub_texture_from_texture(&global_player.art.texture, 336.0f, 16.0f, 175.0f, 310.0f);
  
  global_player.art.left_jacket = sub_texture_from_texture(&global_player.art.texture, 16.0f, 352.0f, 110.0f, 312.0f);
  global_player.art.left_leg = sub_texture_from_texture(&global_player.art.texture, 128.0f, 352.0f, 71.0f, 246.0f);
  global_player.art.left_head = sub_texture_from_texture(&global_player.art.texture, 208.0f, 352.0f, 113.0f, 203.0f);
  global_player.art.left_arm = sub_texture_from_texture(&global_player.art.texture, 464.0f, 352.0f, 42.0f, 213.0f);
  
  global_player.art.right_jacket = sub_texture_from_texture(&global_player.art.texture, 126.0f, 352.0f, -110.0f, 312.0f);
  global_player.art.right_leg = sub_texture_from_texture(&global_player.art.texture, 199.0f, 352.0f, -71.0f, 246.0f);
  global_player.art.right_head = sub_texture_from_texture(&global_player.art.texture, 336.0f, 352.0f, 113.0f, 203.0f);
  global_player.art.right_arm = sub_texture_from_texture(&global_player.art.texture, 512.0f, 352.0f, 42.0f, 213.0f);
 }
 else
 {
  debug_log("*** could not load player art ***\n");
 }
}

#define PLAYER_COLLISION_X  000.0f
#define PLAYER_COLLISION_Y -155.0f
#define PLAYER_COLLISION_W  175.0f
#define PLAYER_COLLISION_H  605.0f

internal Entity *
allocate_and_push_entity(void)
{
 Entity *result;
 
 if (global_current_level_state.entity_free_list)
 {
  result = global_current_level_state.entity_free_list;
  global_current_level_state.entity_free_list = global_current_level_state.entity_free_list->next;
 }
 else if (global_current_level_state.entity_next_index < MAX_ENTITIES)
 {
  result = &global_entity_pool[global_current_level_state.entity_next_index++];
 }
 else
 {
  result = &global_dummy_entity;
 }
 
 memset(result, 0, sizeof(*result));
 result->teleport_to_level_path.buffer = result->teleport_to_level_path_buffer;
 result->dialogue_path.buffer = result->dialogue_path_buffer;
 result->editor_name.size = snprintf(result->editor_name_buffer, ENTITY_STRING_BUFFER_SIZE, "new entity");
 result->editor_name.size = min_u(result->editor_name.size, ENTITY_STRING_BUFFER_SIZE - 1);
 result->editor_name.buffer = result->editor_name_buffer;
 
 if (global_current_level_state.last_entity)
 {
  global_current_level_state.last_entity->next = result;
 }
 else
 {
  global_current_level_state.first_entity = result;
 }
 global_current_level_state.last_entity = result;
 
 return result;
}

internal void
serialise_entity(Entity *e,
                 PlatformFile *f)
{
 Entity_SERIALISABLE _e;
 
 _e.bounds = e->bounds;
 _e.flags = e->flags;
 _e.dialogue_repeat = e->dialogue_repeat;
 _e.dialogue_x = e->dialogue_x;
 _e.dialogue_y = e->dialogue_y;
 _e.set_exposure_mode = e->set_exposure_mode;
 _e.set_exposure_to = e->set_exposure_to;
 _e.teleport_to_x = e->teleport_to_x;
 _e.teleport_to_y = e->teleport_to_y;
 _e.set_player_scale_to = e->set_player_scale_to;
 _e.set_floor_gradient_to = e->set_floor_gradient_to;
 _e.set_post_processing_kind_to = e->set_post_processing_kind_to;
 _e.draw_texture_in_fg = e->draw_texture_in_fg;
 if (e->texture)
 {
  snprintf(_e.texture_path, ENTITY_STRING_BUFFER_SIZE, "%.*s", unravel_s8(e->texture->path));
 }
 snprintf(_e.editor_name, ENTITY_STRING_BUFFER_SIZE, "%.*s", unravel_s8(e->editor_name));
 snprintf(_e.dialogue_path, ENTITY_STRING_BUFFER_SIZE, "%.*s", unravel_s8(e->dialogue_path));
 snprintf(_e.teleport_to_level_path, ENTITY_STRING_BUFFER_SIZE, "%.*s", unravel_s8(e->teleport_to_level_path));
 
 platform_write_to_file_f(f, &_e, sizeof(_e));
}

internal void
deserialise_entity(U64 version,
                   Entity *e,
                   PlatformFile *f,
                   U64 *i)
{
 if (version == 0)
 {
  Entity_SERIALISABLE_V0 _e;
  platform_read_file_f(f, *i, sizeof(_e), &_e);
  
  e->bounds = _e.bounds;
  e->flags = _e.flags;
  e->dialogue_repeat = _e.dialogue_repeat;
  e->dialogue_x = _e.dialogue_x;
  e->dialogue_y = _e.dialogue_y;
  e->set_exposure_mode = _e.set_exposure_mode;
  e->set_exposure_to = _e.set_exposure_to;
  e->teleport_to_x = _e.teleport_to_x;
  e->teleport_to_y = _e.teleport_to_y;
  e->set_player_scale_to = _e.set_player_scale_to;
  e->set_floor_gradient_to = _e.set_floor_gradient_to;
  e->set_post_processing_kind_to = _e.set_post_processing_kind_to;
  e->texture = texture_from_path(s8(_e.texture_path));
  e->draw_texture_in_fg = _e.draw_texture_in_fg;
  
  e->dialogue_path.buffer = e->dialogue_path_buffer;
  memcpy(e->dialogue_path.buffer, _e.dialogue_path, ENTITY_STRING_BUFFER_SIZE);
  e->dialogue_path.size = calculate_utf8_cstring_size(e->dialogue_path.buffer);
  
  e->teleport_to_level_path.buffer = e->teleport_to_level_path_buffer;
  memcpy(e->teleport_to_level_path.buffer, _e.teleport_to_level_path, ENTITY_STRING_BUFFER_SIZE);
  e->teleport_to_level_path.size = calculate_utf8_cstring_size(e->teleport_to_level_path.buffer);
  
  *i += sizeof(_e);
 }
 else if (version == 1)
 {
  Entity_SERIALISABLE_V1 _e;
  platform_read_file_f(f, *i, sizeof(_e), &_e);
  
  e->bounds = _e.bounds;
  e->flags = _e.flags;
  e->dialogue_repeat = _e.dialogue_repeat;
  e->dialogue_x = _e.dialogue_x;
  e->dialogue_y = _e.dialogue_y;
  e->set_exposure_mode = _e.set_exposure_mode;
  e->set_exposure_to = _e.set_exposure_to;
  e->teleport_to_x = _e.teleport_to_x;
  e->teleport_to_y = _e.teleport_to_y;
  e->set_player_scale_to = _e.set_player_scale_to;
  e->set_floor_gradient_to = _e.set_floor_gradient_to;
  e->set_post_processing_kind_to = _e.set_post_processing_kind_to;
  e->texture = texture_from_path(s8(_e.texture_path));
  e->draw_texture_in_fg = _e.draw_texture_in_fg;
  
  e->editor_name.buffer = e->editor_name_buffer;
  memcpy(e->editor_name.buffer, _e.editor_name, ENTITY_STRING_BUFFER_SIZE);
  e->editor_name.size = calculate_utf8_cstring_size(e->editor_name.buffer);
  
  e->dialogue_path.buffer = e->dialogue_path_buffer;
  memcpy(e->dialogue_path.buffer, _e.dialogue_path, ENTITY_STRING_BUFFER_SIZE);
  e->dialogue_path.size = calculate_utf8_cstring_size(e->dialogue_path.buffer);
  
  e->teleport_to_level_path.buffer = e->teleport_to_level_path_buffer;
  memcpy(e->teleport_to_level_path.buffer, _e.teleport_to_level_path, ENTITY_STRING_BUFFER_SIZE);
  e->teleport_to_level_path.size = calculate_utf8_cstring_size(e->teleport_to_level_path.buffer);
  
  *i += sizeof(_e);
 }
}

internal void
serialise_current_level(void)
{
 PlatformFile *f = platform_open_file_ex(global_current_level_state.path,
                                         PLATFORM_OPEN_FILE_write);
 
 platform_write_to_file_f(f, (U64[]){ ENTITY_SERIALISATION_VERSION }, sizeof(U64));
 
 for (Entity *e = global_current_level_state.first_entity;
      NULL != e;
      e = e->next)
 {
  serialise_entity(e, f);
 }
 
 platform_close_file(&f);
}

internal void
set_current_level(S8 path)
{
 // NOTE(tbt): unload textures loaded for previous level
 for (Texture *texture = global_current_level_state.textures;
      NULL != texture;
      texture = texture->next_loaded)
 {
  unload_texture(texture);
 }
 
 // NOTE(tbt): clear current level state, saving statically allocated path buffer
 U8 *path_buffer = global_current_level_state.path.buffer;
 memset(&global_current_level_state, 0, sizeof(global_current_level_state));
 
 // NOTE(tbt): save path
 global_current_level_state.path.buffer = path_buffer;
 global_current_level_state.path.size = path.size;
 memcpy(global_current_level_state.path.buffer, path.buffer, min_u(path.size, CURRENT_LEVEL_PATH_BUFFER_SIZE));
 
 debug_log("setting level to %.*s\n", unravel_s8(global_current_level_state.path));
 
 // NOTE(tbt): clear level duration memory
 arena_free_all(&global_level_memory);
 
 // NOTE(tbt): load the new level
 arena_temporary_memory(&global_temp_memory)
 {
  PlatformFile *f = platform_open_file_ex(global_current_level_state.path,
                                          PLATFORM_OPEN_FILE_read);
  U64 file_size = platform_get_file_size_f(f);
  
  if (file_size > sizeof(U64))
  {
   U64 entity_version;
   platform_read_file_f(f, 0, sizeof(entity_version), &entity_version);
   
   U64 i = sizeof(entity_version);
   while (i < file_size)
   {
    Entity *e = allocate_and_push_entity();
    deserialise_entity(entity_version, e, f, &i);
   }
  }
  
  platform_close_file(&f);
 }
}

internal void
hot_reload_textures(F64 frametime_in_s)
{
 persist F64 time = 0.0;
 time += frametime_in_s;
 
 F64 refresh_time = 2.0; // NOTE(tbt): refresh every 2 seconds
 if (time > refresh_time)
 {
  time = 0.0;
  
  // NOTE(tbt): reload entity textures
  for (Texture *t = global_current_level_state.textures;
       NULL != t;
       t = t->next_loaded)
  {
   if (platform_get_file_modified_time_p(t->path) > t->last_modified)
   {
    load_texture(t);
   }
  }
  
  // NOTE(tbt): reload player texture
  if (platform_get_file_modified_time_p(global_player.art.texture.path) > global_player.art.texture.last_modified)
  {
   load_texture(&global_player.art.texture);
  }
 }
}

internal void
do_player(PlatformState *input,
          F64 frametime_in_s)
{
 // TODO(tbt): better walk animation
 
 F32 player_speed = (128.0f + sin(global_time * 0.2) * 5.0f) * frametime_in_s;
 
 F32 animation_y_offset = -sin(global_time * 6.0f) * 2.0f;
 
 global_player.x_velocity =
  input->is_key_down[KEY_a] * -player_speed +
  input->is_key_down[KEY_d] * player_speed;
 
 global_player.y_velocity = global_player.x_velocity * global_current_level_state.y_offset_per_x;
 
 global_player.x += global_player.x_velocity;
 global_player.y += global_player.y_velocity;
 
 F32 scale = global_current_level_state.player_scale / (SCREEN_H_IN_WORLD_UNITS - global_player.y);
 
 //
 // NOTE(tbt): walk right animation
 //
 
 if (global_player.x_velocity >  0.01f)
 {
  draw_sub_texture(rect(global_player.x + 31.0f * scale,
                        global_player.y + (-153.0f + animation_y_offset) * scale,
                        113.0f * scale, 203.0f * scale),
                   WHITE,
                   &global_player.art.texture,
                   global_player.art.right_head,
                   0, global_world_projection_matrix);
  
  draw_rotated_sub_texture(rect(global_player.x + 40.0f * scale,
                                global_player.y + (205.0f + sin(global_time * 3.0f + 2.0f) * 7.0f + animation_y_offset) * scale,
                                71.0f * scale, 246.0f * scale),
                           (sin(global_time * 3.0f + 2.0f) * 0.1f + 0.03f) * scale,
                           col(0.5f, 0.5f, 0.5f, 1.0f),
                           &global_player.art.texture,
                           global_player.art.right_leg,
                           0, global_world_projection_matrix);
  
  draw_rotated_sub_texture(rect(global_player.x + 40.0f * scale,
                                global_player.y + (205.0f + sin(global_time * 3.0f) * 7.0f + animation_y_offset) * scale,
                                71.0f * scale, 246.0f * scale),
                           (sin(global_time * 3.0f) * 0.1f + 0.03f) * scale,
                           WHITE,
                           &global_player.art.texture,
                           global_player.art.right_leg,
                           0, global_world_projection_matrix);
  
  draw_sub_texture(rect(global_player.x,
                        global_player.y + animation_y_offset * scale,
                        110.0f * scale, 312.0f * scale),
                   WHITE,
                   &global_player.art.texture,
                   global_player.art.right_jacket,
                   0, global_world_projection_matrix);
  
  draw_rotated_sub_texture(rect(global_player.x + 40.0f * scale,
                                global_player.y + (28.0f + animation_y_offset) * scale,
                                42.0f * scale, 213.0f * scale),
                           (sin(global_time * 2.8f) * 0.12f) * scale,
                           WHITE,
                           &global_player.art.texture,
                           global_player.art.right_arm,
                           0, global_world_projection_matrix);
 }
 
 //
 // NOTE(tbt): walk left animation
 //
 
 else if (global_player.x_velocity < -0.01f)
 {
  draw_sub_texture(rect(global_player.x + 31 * scale,
                        global_player.y + (-153.0f + animation_y_offset) * scale,
                        113.0f * scale, 203.0f * scale),
                   WHITE,
                   &global_player.art.texture,
                   global_player.art.left_head,
                   0, global_world_projection_matrix);
  
  draw_rotated_sub_texture(rect(global_player.x + 76.0f * scale,
                                global_player.y + (205.0f + sin(global_time * 3.0f + 2.0f) * 7.0f + animation_y_offset) * scale,
                                71.0f * scale, 246.0f * scale),
                           (sin(global_time * 3.0f + 2.0f) * 0.1f - 0.03f) * scale,
                           col(0.5f, 0.5f, 0.5f, 1.0f),
                           &global_player.art.texture,
                           global_player.art.left_leg,
                           0, global_world_projection_matrix);
  
  draw_rotated_sub_texture(rect(global_player.x + 76.0f * scale,
                                global_player.y + (205.0f + sin(global_time * 3.0f) * 7.0f + animation_y_offset) * scale,
                                71.0f * scale, 246.0f * scale),
                           (sin(global_time * 3.0f) * 0.1f - 0.03f) * scale,
                           WHITE,
                           &global_player.art.texture,
                           global_player.art.left_leg,
                           0, global_world_projection_matrix);
  
  draw_sub_texture(rect(global_player.x + 64 * scale,
                        global_player.y + animation_y_offset * scale,
                        110.0f * scale, 312.0f * scale),
                   WHITE,
                   &global_player.art.texture,
                   global_player.art.left_jacket,
                   0, global_world_projection_matrix);
  
  draw_rotated_sub_texture(rect(global_player.x + 86.0f * scale,
                                global_player.y + (28.0f + animation_y_offset) * scale,
                                42.0f * scale, 213.0f * scale),
                           (sin(global_time * 2.8f) * 0.12f) * scale,
                           WHITE,
                           &global_player.art.texture,
                           global_player.art.left_arm,
                           0, global_world_projection_matrix);
 }
 
 //
 // NOTE(tbt): idle animation
 //
 
 else
 {
  draw_sub_texture(rect(global_player.x + 22.0f * scale,
                        global_player.y + 128.0f * scale,
                        131.0f * scale, 320.0f * scale),
                   WHITE,
                   &global_player.art.texture,
                   global_player.art.forward_lower_body,
                   0, global_world_projection_matrix);
  
  draw_sub_texture(rect(global_player.x + 41.0f * scale,
                        global_player.y - 155.0f * scale,
                        88.0f * scale, 175.0f * scale),
                   WHITE,
                   &global_player.art.texture,
                   global_player.art.forward_head,
                   0, global_world_projection_matrix);
  
  draw_sub_texture(rect(global_player.x + 135.0f * scale,
                        global_player.y + (25.0f + sin(global_time + 2.0f) * 7.0f) * scale,
                        29.0f * scale, 216.0f * scale),
                   WHITE,
                   &global_player.art.texture,
                   global_player.art.forward_right_arm,
                   0, global_world_projection_matrix);
  
  draw_sub_texture(rect(global_player.x,
                        global_player.y + (25.0f + sin(global_time + 2.0f) * 7.0f) * scale,
                        43.0f * scale, 212.0f * scale),
                   WHITE,
                   &global_player.art.texture,
                   global_player.art.forward_left_arm,
                   0, global_world_projection_matrix);
  
  draw_sub_texture(rect(global_player.x,
                        global_player.y + (sin(global_time + 2.0f) * 3.0f) * scale,
                        175.0f * scale, 310.0 * scale),
                   WHITE,
                   &global_player.art.texture,
                   global_player.art.forward_torso,
                   0, global_world_projection_matrix);
 }
 
 global_player.collision_bounds = rect(global_player.x + PLAYER_COLLISION_X * scale,
                                       global_player.y + PLAYER_COLLISION_Y * scale,
                                       PLAYER_COLLISION_W * scale, PLAYER_COLLISION_H * scale);
}

internal void
do_current_level(F64 frametime_in_s)
{
 Rect mask;
 {
  F32 desired_aspect = (F32)SCREEN_H_IN_WORLD_UNITS / (F32)SCREEN_W_IN_WORLD_UNITS;
  F32 mask_h = desired_aspect * global_rcx.window.w;
  F32 mask_y = (global_rcx.window.h - mask_h) / 2.0f;
  if (mask_y < 0.0f)
  {
   mask_h += global_rcx.window.h;
   mask_y = 0.0f;
  }
  mask = rect(0.0f, mask_y, global_rcx.window.w, mask_h);
 }
 
 mask_rectangle(mask)
 {
  //
  // NOTE(tbt): entities
  //
  {
   persist DialogueState dialogue_state = {0};
   
   for (Entity *prev = NULL, *e = global_current_level_state.first_entity;
        NULL != e;
        prev = e, e = e->next)
   {
    //
    // NOTE(tbt): remove deleted entities
    //
    
    if (e->flags & ENTITY_FLAG_marked_for_removal)
    {
     if (prev)
     {
      prev->next = e->next;
      if (global_current_level_state.last_entity == e)
      {
       global_current_level_state.last_entity = prev;
      }
     }
     else
     {
      global_current_level_state.first_entity = e->next;
     }
     
     e->next = global_current_level_state.entity_free_list;
     global_current_level_state.entity_free_list = e;
     
     continue;
    }
    
    //
    // NOTE(tbt): process entity triggers
    //
    
    if (are_rects_intersecting(e->bounds, global_player.collision_bounds))
    {
     if (e->triggers & (1 << ENTITY_TRIGGER_player_intersecting))
     {
      e->triggers &= ~(1 << ENTITY_TRIGGER_player_entered);
     }
     else
     {
      e->triggers |= (1 << ENTITY_TRIGGER_player_entered);
      e->triggers |= (1 << ENTITY_TRIGGER_player_intersecting);
     }
    }
    else
    {
     if (e->triggers & (1 << ENTITY_TRIGGER_player_intersecting))
     {
      e->triggers |= (1 << ENTITY_TRIGGER_player_left);
      e->triggers &= ~(1 << ENTITY_TRIGGER_player_intersecting);
     }
     else
     {
      e->triggers &= ~(1 << ENTITY_TRIGGER_player_left);
     }
    }
    
    //
    // NOTE(tbt): process entity flags
    //
    
    // NOTE(tbt): texture rendering
    if (e->flags & (1 << ENTITY_FLAG_draw_texture))
    {
     draw_sub_texture(e->bounds,
                      WHITE,
                      e->texture,
                      ENTIRE_TEXTURE,
                      e->draw_texture_in_fg,
                      global_world_projection_matrix);
    }
    
    if (e->triggers & (1 << ENTITY_TRIGGER_player_intersecting))
    {
     // NOTE(tbt): set exposure
     if (e->flags & (1 << ENTITY_FLAG_set_exposure))
     {
      F32 player_centre_x = global_player.collision_bounds.x + (global_player.collision_bounds.w / 2.0f);
      F32 player_centre_y = global_player.collision_bounds.y + (global_player.collision_bounds.h / 2.0f);
      
      F32 fade = 1.0f;
      
      switch(e->set_exposure_mode)
      {
       case SET_EXPOSURE_MODE_fade_out_n:
       {
        fade = (player_centre_y - e->bounds.y) / e->bounds.h;
        break;
       }
       case SET_EXPOSURE_MODE_fade_out_e:
       {
        fade = ((e->bounds.x + e->bounds.w) - player_centre_x) / e->bounds.w;
        break;
       }
       case SET_EXPOSURE_MODE_fade_out_s:
       {
        fade = ((e->bounds.y + e->bounds.h) - player_centre_y) / e->bounds.h;
        break;
       }
       case SET_EXPOSURE_MODE_fade_out_w:
       {
        fade = (player_centre_x - e->bounds.x) / e->bounds.w;
        break;
       }
      }
      
      fade = clamp_f(fade, 0.0f, 1.0f);
      
      global_exposure = e->set_exposure_to * fade;
      cm_set_master_gain(fade * global_audio_master_level);
     }
     
     // NOTE(tbt): set player scale
     if (e->flags & (1 << ENTITY_FLAG_set_player_scale))
     {
      global_current_level_state.player_scale = e->set_player_scale_to;
     }
     
     // NOTE(tbt): set floor gradient
     if (e->flags & (1 << ENTITY_FLAG_set_floor_gradient))
     {
      global_current_level_state.y_offset_per_x = e->set_floor_gradient_to;
     }
     
     // NOTE(tbt): set post processing kind
     if (e->flags & (1 << ENTITY_FLAG_set_post_processing_kind))
     {
      global_current_level_state.post_processing_kind = e->set_post_processing_kind_to;
     }
    }
    
    // NOTE(tbt): teleports
    if (e->flags & (1 << ENTITY_FLAG_teleport) &&
        e->triggers & (1 << ENTITY_TRIGGER_player_entered))
    {
     global_player.x = e->teleport_to_x;
     global_player.y = e->teleport_to_y;
     set_current_level(path_from_level_path(&global_frame_memory, e->teleport_to_level_path));
     break;
    }
    
    // NOTE(tbt): dialogue
    if (e->flags & (1 << ENTITY_FLAG_trigger_dialogue) &&
        e->triggers & (1 << ENTITY_TRIGGER_player_entered))
    {
     if (e->dialogue_repeat ||
         !e->dialogue_played)
     {
      debug_log("playing dialogue\n");
      
      play_dialogue(&dialogue_state,
                    load_dialogue(&global_level_memory,
                                  path_from_dialogue_path(&global_frame_memory,
                                                          e->dialogue_path)),
                    e->dialogue_x, e->dialogue_y,
                    WHITE);
      
      e->dialogue_played = true;
     }
    }
   }
   do_dialogue(&dialogue_state, frametime_in_s);
  }
 }
 
 do_post_processing(global_exposure, global_current_level_state.post_processing_kind, UI_SORT_DEPTH - 2);
}

//
// NOTE(tbt): level editor
//~


internal void
do_level_editor(PlatformState *input,
                F64 frametime_in_s)
{
 Entity *do_not_set_selection = (void *)(~0);
 Entity *set_selection_to = do_not_set_selection;
 
 ui_width(400.0f, 0.0f) ui_height(600.0f, 1.0f) ui_window(s8_lit("level editor"))
 {
  ui_height(32.0f, 1.0f) ui_row()
  {
   ui_label(s8_concatenate(&global_frame_memory, &global_frame_memory,
                           2, s8_lit("editing: "), global_current_level_state.path));
  }
  
  ui_height(32.0f, 1.0f) ui_row()
  {
   if (ui_button(s8_lit("new entity")))
   {
    Entity *e = allocate_and_push_entity();
    e->bounds = rect(960.0f, 540.0f, 64.0f, 64.0f);
    set_selection_to = e;
   }
   
   if (ui_button(s8_lit("save level")))
   {
    serialise_current_level();
   }
   
   persist B32 opening_level = false;
   ui_toggle_button(s8_lit("open level"), &opening_level);
   if (opening_level)
   {
    global_editor_selected_entity = NULL;
    
    ui_height(200.0f, 1.0f) ui_window(s8_lit("open level"))
    {
     persist U8 level_path_buffer[64];
     persist S8 level_path = { .buffer = level_path_buffer, };
     
     ui_height(32.0f, 1.0f)
     {
      ui_line_edit(s8_lit("level path"), &level_path, sizeof(level_path_buffer));
      
      ui_row()
      {
       B32 set_level;
       if ((set_level = ui_button(s8_lit("open"))) ||
           ui_button(s8_lit("cancel")))
       {
        if (set_level) { set_current_level(path_from_level_path(&global_frame_memory, level_path)); }
        memset(level_path_buffer, 0, sizeof(level_path_buffer));
        level_path.size = 0;
        opening_level = false;
       }
      }
     }
    }
   }
  }
  
  //
  // NOTE(tbt): entity lister
  //
  
  ui_height(32.0f, 1.0f)
  {
   ui_label(s8_lit("entities:"));
   
   ui_indent(32)
   {
    I32 entity_index = 0;
    for (Entity *e = global_current_level_state.first_entity;
         NULL != e;
         e = e->next)
    {
     if (ui_button_with_id(s8_from_format_string(&global_frame_memory, "%d", entity_index++),
                           e->editor_name,
                           global_editor_selected_entity == e))
     {
      set_selection_to = e;
     }
    }
   }
  }
 }
 
 //
 // NOTE(tbt): edit entities
 //
 
 {
  persist F32 drag_x_offset;
  persist F32 drag_y_offset;
  persist Entity *dragging;
  persist Entity *resizing;
  Entity *hot = NULL;
  
  for (Entity *prev = NULL, *e = global_current_level_state.first_entity;
       NULL != e;
       prev = e, e = e->next)
  {
   if (e->flags & (1 << ENTITY_FLAG_marked_for_removal))
   {
    if (prev)
    {
     prev->next = e->next;
     if (global_current_level_state.last_entity == e)
     {
      global_current_level_state.last_entity = prev;
     }
    }
    else
    {
     global_current_level_state.first_entity = e->next;
    }
    
    e->next = global_current_level_state.entity_free_list;
    global_current_level_state.entity_free_list = e;
    
    break;
   }
   
   if (e->flags & (1 << ENTITY_FLAG_draw_texture))
   {
    draw_sub_texture(e->bounds,
                     WHITE,
                     e->texture,
                     ENTIRE_TEXTURE,
                     0,
                     global_world_projection_matrix);
   }
   
   stroke_rectangle(e->bounds,
                    col(0.2f, 0.8f, 0.3f, 0.5f),
                    2.0f,
                    0, global_world_projection_matrix);
   
   if (is_point_in_rect(MOUSE_WORLD_X, MOUSE_WORLD_Y, e->bounds) &&
       !global_ui_context.hot)
   {
    hot = e;
   }
   
   if (global_editor_selected_entity == e)
   {
    fill_rectangle(e->bounds,
                   col(0.2f, 0.8f, 0.3f, 0.25f),
                   0, global_world_projection_matrix);
   }
   
   if (dragging == e)
   {
    e->bounds.x = MOUSE_WORLD_X + drag_x_offset;
    e->bounds.y = MOUSE_WORLD_Y + drag_y_offset;
   }
   
   if (resizing == e)
   {
    e->bounds.w = max_f(MOUSE_WORLD_X - e->bounds.x, 1.0f);
    e->bounds.h = max_f(MOUSE_WORLD_Y - e->bounds.y, 1.0f);
   }
  }
  
  // NOTE(tbt): highlight the entity under the mouse
  if (hot)
  {
   fill_rectangle(hot->bounds,
                  col(0.2f, 0.8f, 0.3f, 0.125f),
                  0, global_world_projection_matrix);
  }
  
  if (global_ui_context.hot)
  {
   dragging = NULL;
   resizing = NULL;
  }
  else
  {
   for (PlatformEvent *event = input->events;
        NULL != event;
        event = event->next)
   {
    if (event->kind == PLATFORM_EVENT_mouse_press)
    {
     if (event->mouse_button == MOUSE_BUTTON_left)
     {
      if (hot && global_editor_selected_entity == hot)
      {
       dragging = hot;
       drag_x_offset = hot->bounds.x - MOUSE_WORLD_X;
       drag_y_offset = hot->bounds.y - MOUSE_WORLD_Y;
      }
      else
      {
       set_selection_to = hot;
      }
     }
     else if (event->mouse_button == MOUSE_BUTTON_right)
     {
      if (hot && global_editor_selected_entity == hot)
      {
       dragging = NULL;
       resizing = hot;
      }
     }
    }
    else if (event->kind == PLATFORM_EVENT_key_press &&
             event->key == KEY_delete &&
             NULL != global_editor_selected_entity)
    {
     global_editor_selected_entity->flags |= (1 << ENTITY_FLAG_marked_for_removal);
     global_editor_selected_entity = NULL;
    }
   }
   
   if (!input->is_mouse_button_down[MOUSE_BUTTON_left])
   {
    dragging = NULL;
   }
   
   if (!input->is_mouse_button_down[MOUSE_BUTTON_right])
   {
    resizing = NULL;
   }
  }
  
  if (input->is_key_down[KEY_esc])
  {
   set_selection_to = NULL;
  }
  
  // NOTE(tbt): buffers for strings in entity inspector panel
  persist U8 texture_path_buffer[64] = {0};
  persist S8 texture_path = { .buffer = texture_path_buffer, };
  
  // NOTE(tbt): set selected entity
  if (do_not_set_selection != set_selection_to)
  {
   global_editor_selected_entity = set_selection_to;
   
   memset(texture_path_buffer, 0, sizeof(texture_path_buffer));
   if (NULL != global_editor_selected_entity &&
       NULL != global_editor_selected_entity->texture)
   {
    texture_path.size = global_editor_selected_entity->texture->path.size;
    memcpy(texture_path_buffer,
           global_editor_selected_entity->texture->path.buffer,
           min_u(sizeof(texture_path_buffer), texture_path.size));
   }
  }
  
  //
  // NOTE(tbt): entity inspector panel
  //
  
  if (global_editor_selected_entity)
  {
   Entity *e = global_editor_selected_entity;
   
   ui_width(800.0f, 0.0f) ui_height(800.0f, 1.0f) ui_window(s8_lit("entity inspector"))
   {
    F32 column_0_w = 150.0f;
    UIDimension slider_text_edit_w = (UIDimension){ 75.0f, 0.75f };
    
    ui_height(32.0f, 1.0f)
    {
     // NOTE(tbt): default inspector section
     {
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("name:"));
       ui_line_edit(s8_lit("name entry"), &e->editor_name, ENTITY_STRING_BUFFER_SIZE);
      }
      
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("bounds:"));
       ui_h_slider_f32(s8_lit("entity bounds x slider"),
                       &e->bounds.x,
                       0.0f, SCREEN_W_IN_WORLD_UNITS);
       ui_h_slider_f32(s8_lit("entity bounds y slider"),
                       &e->bounds.y,
                       0.0f, SCREEN_H_IN_WORLD_UNITS);
       ui_h_slider_f32(s8_lit("entity bounds w slider"),
                       &e->bounds.w,
                       0.0f, SCREEN_W_IN_WORLD_UNITS);
       ui_h_slider_f32(s8_lit("entity bounds h slider"),
                       &e->bounds.h,
                       0.0f, SCREEN_H_IN_WORLD_UNITS);
      }
     }
     
     // NOTE(tbt): telport inspector section
     if (ui_bit_toggle_button(s8_lit("teleport"), &e->flags, ENTITY_FLAG_teleport))
     {
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("level:"));
       ui_line_edit(s8_lit("level path"), &e->teleport_to_level_path, ENTITY_STRING_BUFFER_SIZE);
      }
      
      ui_row()
      {
       
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("spawn:"));
       
       ui_h_slider_text_edit_f32(s8_lit("spawn x"), &e->teleport_to_x, 0.0f, SCREEN_W_IN_WORLD_UNITS, slider_text_edit_w);
       
       ui_h_slider_text_edit_f32(s8_lit("spawn y"), &e->teleport_to_y, 0.0f, SCREEN_H_IN_WORLD_UNITS, slider_text_edit_w);
      }
     }
     
     // NOTE(tbt): trigger dialogue inspector section
     if (ui_bit_toggle_button(s8_lit("trigger dialogue"), &e->flags, ENTITY_FLAG_trigger_dialogue))
     {
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("dialogue:"));
       ui_line_edit(s8_lit("dialogue path"), &e->dialogue_path, ENTITY_STRING_BUFFER_SIZE);
      }
      
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("position:"));
       ui_h_slider_text_edit_f32(s8_lit("dialogue x"), &e->dialogue_x, 0.0f, SCREEN_W_IN_WORLD_UNITS, slider_text_edit_w);
       ui_h_slider_text_edit_f32(s8_lit("dialogue y"), &e->dialogue_y, 0.0f, SCREEN_H_IN_WORLD_UNITS, slider_text_edit_w);
      }
      
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("repeat:"));
       ui_width(150.0f, 0.5f) ui_toggle_button(s8_lit("repeat"), &e->dialogue_repeat);
      }
      
      ui_row()
      {
       persist DialogueState dialogue_state = {0};
       do_dialogue(&dialogue_state, frametime_in_s);
       if (dialogue_state.playing)
       {
        do_post_processing(global_exposure,
                           global_current_level_state.post_processing_kind,
                           UI_SORT_DEPTH - 2);
       }
       ui_width(150.0f, 1.0f) ui_indent(global_ui_context.padding)
        if (ui_button(s8_lit("preview")))
       {
        play_dialogue(&dialogue_state,
                      load_dialogue(&global_level_memory,
                                    path_from_dialogue_path(&global_frame_memory,
                                                            e->dialogue_path)),
                      e->dialogue_x, e->dialogue_y,
                      WHITE);
       }
      }
     }
     
     // NOTE(tbt): draw texture inspector section
     if (ui_bit_toggle_button(s8_lit("draw texture"), &e->flags, ENTITY_FLAG_draw_texture))
     {
      ui_row()
      {
       U64 texture_path_buffer_size = 128;
       
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("texture:"));
       
       if (!ui_line_edit(s8_lit("path"),
                         &texture_path,
                         sizeof(texture_path_buffer)))
       {
        if (NULL == e->texture ||
            (0 != texture_path.size && !s8_match(e->texture->path, texture_path)))
        {
         e->texture = texture_from_path(texture_path);
        }
        
        if (NULL == e->texture)
        {
         memset(texture_path_buffer, 0, sizeof(texture_path_buffer));
         texture_path.size = 0;
        }
        else
        {
         texture_path.size = snprintf(texture_path_buffer,
                                      sizeof(texture_path_buffer),
                                      "%.*s", unravel_s8(e->texture->path));
         texture_path.size = min_u(texture_path.size, sizeof(texture_path_buffer) - 1);
        }
       }
      }
      
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("sort depth:"));
       ui_width(150.0f, 1.0f) ui_toggle_button(s8_lit("draw in fg"), &e->draw_texture_in_fg);
      }
     }
     
     // NOTE(tbt): set exposure inspector section
     if (ui_bit_toggle_button(s8_lit("set exposure"), &e->flags, ENTITY_FLAG_set_exposure))
     {
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("mode:"));
       
       if (ui_highlightable_button(s8_lit("uniform"), e->set_exposure_mode == SET_EXPOSURE_MODE_uniform))
       {
        e->set_exposure_mode = SET_EXPOSURE_MODE_uniform;
       }
       if (ui_highlightable_button(s8_lit("fade out N"), e->set_exposure_mode == SET_EXPOSURE_MODE_fade_out_n))
       {
        e->set_exposure_mode = SET_EXPOSURE_MODE_fade_out_n;
       }
       if (ui_highlightable_button(s8_lit("fade out E"), e->set_exposure_mode == SET_EXPOSURE_MODE_fade_out_e))
       {
        e->set_exposure_mode = SET_EXPOSURE_MODE_fade_out_e;
       }
       if (ui_highlightable_button(s8_lit("fade out S"), e->set_exposure_mode == SET_EXPOSURE_MODE_fade_out_s))
       {
        e->set_exposure_mode = SET_EXPOSURE_MODE_fade_out_s;
       }
       if (ui_highlightable_button(s8_lit("fade out W"), e->set_exposure_mode == SET_EXPOSURE_MODE_fade_out_w))
       {
        e->set_exposure_mode = SET_EXPOSURE_MODE_fade_out_w;
       }
      }
      
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("exposure:"));
       ui_h_slider_text_edit_f32(s8_lit("exposure slider"), &e->set_exposure_to, 0.5f, 3.0f, slider_text_edit_w);
      }
     }
     
     // NOTE(tbt): set player scale inspector section
     if (ui_bit_toggle_button(s8_lit("set player scale"), &e->flags, ENTITY_FLAG_set_player_scale))
     {
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("player scale:"));
       ui_h_slider_text_edit_f32(s8_lit("player scale"), &e->set_player_scale_to, 600.0f, 1000.0f, slider_text_edit_w);
      }
     }
     
     // NOTE(tbt): set floor gradient player scale inspector section
     if (ui_bit_toggle_button(s8_lit("set floor gradient"), &e->flags, ENTITY_FLAG_set_floor_gradient))
     {
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("floor gradient:"));
       ui_h_slider_text_edit_f32(s8_lit("floor gradient slider"), &e->set_floor_gradient_to, -1.6f, 1.6f, slider_text_edit_w);
      }
     }
     
     // NOTE(tbt): set post processing kind inspector section
     if (ui_bit_toggle_button(s8_lit("set post processing kind"), &e->flags, ENTITY_FLAG_set_post_processing_kind))
     {
      ui_row()
      {
       ui_width(column_0_w, 1.0f) ui_label(s8_lit("kind:"));
       
       if (ui_highlightable_button(s8_lit("world"), e->set_post_processing_kind_to == POST_PROCESSING_KIND_world))
       {
        e->set_post_processing_kind_to = POST_PROCESSING_KIND_world;
       }
       if (ui_highlightable_button(s8_lit("memory"), e->set_post_processing_kind_to == POST_PROCESSING_KIND_memory))
       {
        e->set_post_processing_kind_to = POST_PROCESSING_KIND_memory;
       }
      }
     }
    }
   }
  }
 }
}

//
// NOTE(tbt): main menu
//~
// TODO(tbt): needs a second pass

internal Rect
main_menu_measure_and_draw_s8(Font *font,
                              F32 x, F32 y,
                              F32 wrap_width,
                              Colour colour,
                              S8 text,
                              B32 centre_align)
{
 Rect result = measure_s32(font,
                           x, y,
                           wrap_width,
                           s32_from_s8(&global_frame_memory, text));
 if (centre_align)
 {
  F32 offset = result.w / 2.0f;
  x -= offset;
  result.x -= offset;
 }
 draw_s8(font, x, y, wrap_width, colour, text, 0, global_ui_projection_matrix);
 
 return result;
}

#define MAIN_MENU_BUTTON_SHIFT_AMOUNT 16.0f
#define MAIN_MENU_BUTTON_SHIFT_SPEED 256.0f
#define MAIN_MENU_TEXT_COLOUR col(0.92f, 0.97f, 0.92f, 1.0f)
#define MAIN_MENU_BUTTON_REGION_TOLERANCE 16.0f

#define MAIN_MENU_BUTTON(_text, _y, _selected_with_keyboard)                                                       \
Rect _button_bounds = measure_s32(global_current_locale_config.normal_font,                                        \
global_rcx.window.w / 2.0f,                                                 \
(_y),                                                                            \
-1.0f,                                                                           \
s32_from_s8(&global_frame_memory, (_text)));                                     \
persist B32 _hovered = false;                                                                                       \
persist F32 _x_offset = 0.0f;                                                                                       \
if (is_point_in_rect(input->mouse_x,                                                                             \
input->mouse_y,                                                                             \
rect(_button_bounds.x - MAIN_MENU_BUTTON_REGION_TOLERANCE / 2.0f,              \
_button_bounds.y - MAIN_MENU_BUTTON_REGION_TOLERANCE / 2.0f,              \
_button_bounds.w + MAIN_MENU_BUTTON_REGION_TOLERANCE,                     \
_button_bounds.h + MAIN_MENU_BUTTON_REGION_TOLERANCE)) ||                 \
(_selected_with_keyboard))                                                                                     \
{                                                                                                                  \
if (!_hovered)                                                                                                    \
{                                                                                                                 \
_hovered = true;                                                                                                 \
cm_play(global_click_sound);                                                                                     \
}                                                                                                                 \
_x_offset = min(_x_offset + frametime_in_s * MAIN_MENU_BUTTON_SHIFT_SPEED, MAIN_MENU_BUTTON_SHIFT_AMOUNT);        \
}                                                                                                                  \
else                                                                                                               \
{                                                                                                                  \
_hovered = false;                                                                                                 \
_x_offset = max(_x_offset - frametime_in_s * MAIN_MENU_BUTTON_SHIFT_SPEED, 0.0);                                  \
}                                                                                                                  \
draw_s8(global_current_locale_config.normal_font,                                                                  \
global_rcx.window.w / 2.0f - _button_bounds.w / 2.0f + _x_offset, (_y),                               \
-1.0f,                                                                                                     \
MAIN_MENU_TEXT_COLOUR,                                                                                     \
(_text),                                                                                                   \
0, global_ui_projection_matrix);                                                                           \
if (_hovered &&                                                                                                    \
(input->is_mouse_button_down[MOUSE_BUTTON_left] ||                                                             \
is_key_pressed(input, KEY_enter, 0)))

internal void
do_main_menu(PlatformState *input,
             F64 frametime_in_s)
{
 typedef enum
 {
  MAIN_MENU_BUTTON_NONE,
  
  MAIN_MENU_BUTTON_play,
  MAIN_MENU_BUTTON_exit,
  
  MAIN_MENU_BUTTON_MAX,
 } MainMenuButton;
 persist MainMenuButton keyboard_selection = MAIN_MENU_BUTTON_NONE;
 
 if (is_key_pressed(input, KEY_tab, 0))
 {
  keyboard_selection = (keyboard_selection + 1) % MAIN_MENU_BUTTON_MAX;
 }
 
 main_menu_measure_and_draw_s8(global_current_locale_config.title_font,
                               global_rcx.window.w / 2.0f,
                               300.0f,
                               -1.0f,
                               col(1.0f, 1.0f, 1.0f, 1.0f),
                               global_current_locale_config.title,
                               true);
 
 
 {
  MAIN_MENU_BUTTON(global_current_locale_config.play, 400.0f, keyboard_selection == MAIN_MENU_BUTTON_play)
  {
   set_current_level(s8_lit("../assets/levels/office_1.level"));
   global_player.x = 960.0f;
   global_player.y = 490.0f;
   if (input->is_key_down[KEY_ctrl])
   {
    global_game_state = GAME_STATE_editor;
   }
   else
   {
    global_game_state = GAME_STATE_playing;
   }
  }
 }
 
 {
  MAIN_MENU_BUTTON(global_current_locale_config.exit, 450.0f, keyboard_selection == MAIN_MENU_BUTTON_exit)
  {
   platform_quit();
  }
 }
 
 do_post_processing(1.0f, POST_PROCESSING_KIND_memory, 1);
}

//
// NOTE(tbt): audio
//~

internal void
cmixer_lock_handler(cm_Event *e)
{
 if (e->type == CM_EVENT_LOCK)
 {
  platform_get_audio_lock();
 }
 else if (e->type == CM_EVENT_UNLOCK)
 {
  platform_release_audio_lock();
 }
}

void
game_audio_callback(void *buffer,
                    U64 buffer_size)
{
 cm_process(buffer, buffer_size / 2);
}

//
// NOTE(tbt): initialisation
//~

void
game_init(OpenGLFunctions *gl)
{
 // NOTE(tbt): copy OpenGLFunctions struct to global function pointers
#define gl_func(_type, _func) gl ## _func = gl-> ## _func;
#include "gl_funcs.h"
 
 initialise_arena_with_new_memory(&global_static_memory, 100 * ONE_MB);
 initialise_arena_with_new_memory(&global_frame_memory, 100 * ONE_MB);
 initialise_arena_with_new_memory(&global_level_memory, 100 * ONE_MB);
 initialise_arena_with_new_memory(&global_temp_memory, 100 * ONE_MB);
 
 set_locale(LOCALE_en_gb);
 
 cm_init(AUDIO_SAMPLERATE);
 cm_set_lock(cmixer_lock_handler);
 cm_set_master_gain(global_audio_master_level);
 
 global_ui_font = load_font(&global_static_memory,
                            s8_lit("../assets/fonts/mononoki.ttf"),
                            19,
                            32, 255);
 
 global_click_sound = cm_new_source_from_file("../assets/audio/click.wav");
 
 memset(&global_current_level_state, 0, sizeof(global_current_level_state));
 global_current_level_state.path.buffer = arena_push(&global_static_memory, CURRENT_LEVEL_PATH_BUFFER_SIZE);
 
 load_player_art();
 
 initialise_renderer();
 
 ui_initialise();
 
 set_camera_position(960.0f, 540.0f);
}

//
// NOTE(tbt): main loop
//~

void
game_update_and_render(PlatformState *input,
                       F64 frametime_in_s)
{
 renderer_set_window_size(input->window_w, input->window_h);
 
 ui_prepare(input, frametime_in_s);
 
 glClear(GL_COLOR_BUFFER_BIT);
 
 
 if (global_game_state == GAME_STATE_playing)
 {
#ifdef LUCERNA_DEBUG
  hot_reload_textures(frametime_in_s);
  hot_reload_shaders(frametime_in_s);
#endif
  do_current_level(frametime_in_s);
  do_player(input, frametime_in_s);
 }
 else if (global_game_state == GAME_STATE_main_menu)
 {
  do_main_menu(input, frametime_in_s);
 }
 else if (global_game_state == GAME_STATE_editor)
 {
  do_level_editor(input, frametime_in_s);
 }
 
 //
 // NOTE(tbt): debug extras
 //~
#if defined LUCERNA_DEBUG
 
 U8 debug_overlay_str[256];
 snprintf(debug_overlay_str,
          sizeof(debug_overlay_str),
          "frametime  : %f ms (%f fps)\n"
          "player pos : %f %f",
          frametime_in_s * 1000.0,
          1.0 / frametime_in_s,
          global_player.x,
          global_player.y);
 
 draw_s8(global_ui_font,
         16.0f, 16.0f,
         -1.0f,
         col(1.0f, 1.0f, 1.0f, 1.0f),
         s8(debug_overlay_str),
         UI_SORT_DEPTH, global_ui_projection_matrix);
 
 if (is_key_pressed(input,
                    KEY_v,
                    INPUT_MODIFIER_ctrl))
 {
  persist B32 vsync = true;
  platform_set_vsync((vsync = !vsync));
 }
 else if (is_key_pressed(input,
                         KEY_f,
                         INPUT_MODIFIER_ctrl))
 {
  platform_toggle_fullscreen();
 }
 else if (is_key_pressed(input,
                         KEY_e,
                         INPUT_MODIFIER_ctrl))
 {
  if (global_game_state == GAME_STATE_editor)
  {
   global_editor_selected_entity = NULL;
   
   serialise_current_level();
   set_current_level(global_current_level_state.path);
   
   // NOTE(tbt): reset camera position
   set_camera_position(SCREEN_W_IN_WORLD_UNITS / 2,
                       SCREEN_H_IN_WORLD_UNITS / 2);
   
   global_game_state = GAME_STATE_playing;
  }
  else if (global_game_state == GAME_STATE_playing)
  {
   global_game_state = GAME_STATE_editor;
  }
 }
 
 draw_s8(global_ui_font,
         global_rcx.window.w - 200,
         32.0f,
         200.0f,
         col(1.0f, 0.0f, 0.0f, 1.0f),
         s8_lit("debug build"),
         UI_SORT_DEPTH, global_ui_projection_matrix);
#endif
 
 //
 // NOTE(tbt): finish main loop
 //~
 ui_finish();
 renderer_flush_message_queue();
 arena_free_all(&global_frame_memory);
 global_time += frametime_in_s;
}

void
game_cleanup(void)
{
 if (global_game_state == GAME_STATE_editor)
 {
  serialise_current_level();
 }
}



