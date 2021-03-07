typedef struct Entity Entity;
struct Entity
{
	U64 flags;
	F32 teleport_to_y;
	F32 teleport_to_x;
	B32 teleport_to_default_spawn;
	U8 teleport_to_level[64];
	I32 fade_out_direction;
	U8 dialogue_path[64];
	F32 dialogue_y;
	F32 dialogue_x;
	B32 repeat_dialogue;
	U8 triggers;
	Rect bounds;
	Entity *next_free;
	Entity *next;
};

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

