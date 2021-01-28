/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 04 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

enum
{
    ENTITY_FLAG_is_player,
    ENTITY_FLAG_can_move,
    ENTITY_FLAG_render_sub_texture,
};

typedef struct Entity Entity;
struct Entity
{
    U64 flags;
    Entity *next;
    F32 x;
    F32 y;
    
    
    Rectangle render_sub_texture__rectangle; // NOTE(tbt): position is relative to `x` and `y`
    SubTexture render_sub_texture__sub_texture;
    Asset *render_sub_texture__texture;
};

Entity *global_entities = NULL;

internal Entity *
push_player(MemoryArena *memory,
            F32 x, F32 y)
{
    Entity *result = arena_allocate(memory, sizeof(*result));
    result->next = global_entities;
    global_entities = result;
    
    result->flags =
        (1 << ENTITY_FLAG_is_player) |
                    (1 << ENTITY_FLAG_can_move)  |
        (1 << ENTITY_FLAG_render_sub_texture);
    
    result->render_sub_texture__rectangle = rectangle_literal(x, y, 64.0f, 64.0f);
    result->render_sub_texture__texture = asset_from_path(s8_literal("../assets/textures/spritesheet.png"));
    result->render_sub_texture__sub_texture = sub_texture_from_texture(result->render_sub_texture__texture->texture, 0, 0, 16, 16);
    
    return result;
}

internal void
process_entities(OpenGLFunctions *gl,
                 PlatformState *input,
                 F64 timestep_in_s)
{
    for (Entity *e = global_entities;
         NULL != e;
         e = e->next)
    {
        if (e->flags & (1 << ENTITY_FLAG_render_sub_texture))
        {
            // TODO(tbt): render sub texture
    }
}
}
