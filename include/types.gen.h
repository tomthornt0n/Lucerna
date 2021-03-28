typedef struct LevelDescriptor LevelDescriptor;
struct LevelDescriptor
{
	U8 music_path[64];
	U8 fg_path[64];
	U8 bg_path[64];
	F32 player_scale;
	F32 floor_gradient;
	F32 exposure;
	F32 player_spawn_x;
	F32 player_spawn_y;
	I32 kind;
};

typedef enum
{
	LEVEL_KIND_world,
	LEVEL_KIND_memory,
} LevelKind;

typedef enum
{
	ENTITY_TRIGGER_player_left,
	ENTITY_TRIGGER_player_intersecting,
	ENTITY_TRIGGER_player_entered,
} EntityTriggers;

typedef enum
{
	FADE_OUT_DIR_w,
	FADE_OUT_DIR_s,
	FADE_OUT_DIR_e,
	FADE_OUT_DIR_n,
} FadeOutDir;

typedef enum
{
	ENTITY_FLAG_trigger_dialogue,
	ENTITY_FLAG_teleport,
	ENTITY_FLAG_marked_for_removal,
	ENTITY_FLAG_fade_out,
} EntityFlags;

typedef struct Entity Entity;
struct Entity
{
	U64 flags;
	F32 teleport_to_x;
	F32 teleport_to_y;
	I32 teleport_on_trigger;
	B32 teleport_do_not_persist_exposure;
	B32 teleport_to_non_default_spawn;
	U8 teleport_to_level[64];
	I32 fade_out_direction;
	U8 dialogue_path[64];
	F32 dialogue_y;
	F32 dialogue_x;
	B32 dialogue_played;
	B32 repeat_dialogue;
	Rect bounds;
	U8 triggers;
	Entity *next_free;
	Entity *next;
};

