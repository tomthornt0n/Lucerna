typedef enum
{
	FADE_OUT_DIR_w,
	FADE_OUT_DIR_s,
	FADE_OUT_DIR_e,
	FADE_OUT_DIR_n,
} FadeOutDir;

typedef enum
{
	ENTITY_FLAG_teleport,
	ENTITY_FLAG_marked_for_removal,
	ENTITY_FLAG_fade_out,
} EntityFlags;

typedef struct Entity Entity;
struct Entity
{
	U64 flags;
	U8 teleport_to[64];
	I32 fade_out_direction;
	Rect bounds;
	Entity *next_free;
	Entity *next;
};

