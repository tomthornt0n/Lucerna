#define TILE_SIZE 64.0f

typedef enum
{
    TILE_KIND_none,
    TILE_KIND_grass,
    TILE_KIND_dirt,
    TILE_KIND_trees,
    
    TILE_KIND_MAX,
} TileKind;

#define s8_from_tile_kind(_tile_kind)\
(((_tile_kind) == TILE_KIND_none)  ? s8_literal("none")  : \
((_tile_kind) == TILE_KIND_grass) ? s8_literal("grass") : \
((_tile_kind) == TILE_KIND_dirt)  ? s8_literal("dirt")  : \
((_tile_kind) == TILE_KIND_trees) ? s8_literal("trees") : \
s8_literal("ERROR CONVERTING TileKind TO S8"))

typedef struct
{
    Asset *texture;
    SubTexture sub_texture;
    B32 solid;
    B32 visible;
} Tile;

internal struct
{
    Tile tiles[54];
    Tile *grass_tiles;
    Tile *dirt_tiles;
    Tile *tree_tiles;
} global_autotiler;

typedef struct
{
    TileKind *kinds; // NOTE(tbt): this is what's written to when placing a tile - calling `_refresh_autotiling()` will write the relevant tiles into `tiles`
    U32 *tiles;      // NOTE(tbt): each tile is an index into `global_autotiler.tiles`
    U64 width;
    U64 height;
} TileMap;

internal void
initialise_auto_tiler(OpenGLFunctions *gl)
{
    Asset *spritesheet = asset_from_path(s8_literal("../assets/textures/spritesheet.png"));
    load_texture(gl, spritesheet);
    
    //
    // NOTE(tbt): setup default blank tile
    //
    
    global_autotiler.tiles[0].texture = NULL;
    global_autotiler.tiles[0].sub_texture = ENTIRE_TEXTURE;
    global_autotiler.tiles[0].solid = true;
    global_autotiler.tiles[0].visible = false;
    
    //
    // NOTE(tbt): setup grass tiles
    //
    
    SubTexture grass_sub_textures[3];
    slice_animation(grass_sub_textures, spritesheet->texture, 128, 48, 16, 16, 3, 1);
    
    global_autotiler.grass_tiles = global_autotiler.tiles + 1;
    for (I32 i = 0;
         i < 3;
         ++i)
    {
        global_autotiler.grass_tiles[i].texture = spritesheet;
        global_autotiler.grass_tiles[i].sub_texture = grass_sub_textures[i];
        global_autotiler.grass_tiles[i].solid = false;
        global_autotiler.grass_tiles[i].visible = true;
    }
    
    //
    // NOTE(tbt): setup dirt tiles
    //
    
    SubTexture dirt_sub_textures[16];
    slice_animation(dirt_sub_textures, spritesheet->texture, 64, 0, 16, 16, 4, 4);
    
    global_autotiler.dirt_tiles = global_autotiler.tiles + 4;
    for (I32 i = 0;
         i < 16;
         ++i)
    {
        global_autotiler.dirt_tiles[i].texture = spritesheet;
        global_autotiler.dirt_tiles[i].sub_texture = dirt_sub_textures[i];
        global_autotiler.dirt_tiles[i].solid = false;
        global_autotiler.dirt_tiles[i].visible = true;
    }
    
    //
    // NOTE(tbt): setup tree tiles
    //
    
    SubTexture tree_sub_textures[18];
    slice_animation(tree_sub_textures, spritesheet->texture, 128, 0, 16, 16, 6, 3);
    
    global_autotiler.tree_tiles = global_autotiler.tiles + 20;
    for (I32 i = 0;
         i < 18;
         ++i)
    {
        global_autotiler.tree_tiles[i].texture = spritesheet;
        global_autotiler.tree_tiles[i].sub_texture = tree_sub_textures[i];
        global_autotiler.tree_tiles[i].solid = true;
        global_autotiler.tree_tiles[i].visible = true;
    }
}

internal void
initialise_tile_map(MemoryArena *memory,
                    TileMap *tile_map,
                    U64 width, U64 height)
{
    tile_map->tiles = arena_allocate(memory, width * height * sizeof(*tile_map->tiles));
    tile_map->kinds = arena_allocate(memory, width * height * sizeof(*tile_map->kinds));
    tile_map->width = width;
    tile_map->height = height;
}

internal void _refresh_auto_tiling(TileMap *tile_map);

internal void
_auto_tile_grass(TileMap *tile_map,
                 U64 index)
{
    if (index > tile_map->width * tile_map->height) { return; }
    
    Tile *tile = &global_autotiler.grass_tiles[max_i(0, rand() % 6 - 3)];
    U32 tile_index = tile - global_autotiler.tiles;
    tile_map->tiles[index] = tile_index;
}

internal U8
_get_4_bit_tile_mask(TileMap *tile_map,
                     I64 index)
{
    U8 mask = 0;
    U64 tile_count = tile_map->width * tile_map->height;
    TileKind kind = tile_map->kinds[index];
    
    if ((I64)index - (I64)tile_map->width >= 0 &&
        (I64)index - (I64)tile_map->width < tile_count)
    {
        mask |= (1 << 0) * (tile_map->kinds[index - tile_map->width] == kind);
    }
    
    if ((I64)index - 1 >= 0 &&
        (I64)index - 1 <= tile_count &&
        index % tile_map->width)
    {
        mask |= (1 << 1) * (tile_map->kinds[index - 1] == kind);
    }
    
    if (index + 1 >= 0 &&
        index + 1 < tile_count &&
        (index + 1) % tile_map->width)
    {
        mask |= (1 << 2) * (tile_map->kinds[index + 1] == kind);
    }
    
    if (index + tile_map->width >= 0 &&
        index + tile_map->width < tile_count)
    {
        mask |= (1 << 3) * (tile_map->kinds[index + tile_map->width] == kind);
    }
    
    return mask;
}

internal void
_auto_tile_dirt(TileMap *tile_map,
                I64 index)
{
    if (index > tile_map->width * tile_map->height) { return; }
    
    Tile *tile = &global_autotiler.dirt_tiles[_get_4_bit_tile_mask(tile_map, index)];
    U32 tile_index = tile - global_autotiler.tiles;
    tile_map->tiles[index] = tile_index;
}


internal void
_auto_tile_trees(TileMap *tile_map,
                 I64 index)
{
    if (index > tile_map->width * tile_map->height) { return; }
    
    U8 mask = _get_4_bit_tile_mask(tile_map, index);
    
    U64 tile_count = tile_map->width * tile_map->height;
    
    if (mask == 15)
    {
        U64 sw_i = index + tile_map->width - 1;
        U64 se_i = index + tile_map->width + 1;
        
        if (sw_i >= 0 &&
            sw_i < tile_count &&
            index % tile_map->width &&
            tile_map->kinds[sw_i] != tile_map->kinds[index])
        {
            mask = 17;
        }
        else if (se_i >= 0 &&
                 se_i < tile_count &&
                 se_i % tile_map->width &&
                 tile_map->kinds[se_i] != tile_map->kinds[index])
        {
            mask = 16;
        }
    }
    
    Tile *tile = &global_autotiler.tree_tiles[mask];
    U32 tile_index = tile - global_autotiler.tiles;
    tile_map->tiles[index] = tile_index;
}

internal void
_refresh_auto_tiling(TileMap *tile_map)
{
    srand(0);
    
    for (I64 i = 0;
         i < tile_map->width * tile_map->height;
         ++i)
    {
        switch (tile_map->kinds[i])
        {
            case TILE_KIND_none:
            {
                tile_map->tiles[i] = 0;
                break;
            }
            case TILE_KIND_grass:
            {
                _auto_tile_grass(tile_map, i);
                break;
            }
            case TILE_KIND_dirt:
            {
                _auto_tile_dirt(tile_map, i);
                break;
            }
            case TILE_KIND_trees:
            {
                _auto_tile_trees(tile_map, i);
                break;
            }
        }
    }
}

internal void
render_tile_map(TileMap *tile_map)
{
    Rect viewport = rectangle_literal(global_camera_x, global_camera_y,
                                      global_renderer_window_w,
                                      global_renderer_window_h);
    
    I32 min_x = clamp_i(viewport.x * (1.0f / TILE_SIZE), 0, tile_map->width);
    I32 min_y = clamp_i(viewport.y * (1.0f / TILE_SIZE), 0, tile_map->height);
    I32 max_x = clamp_i((viewport.x + viewport.w) * (1.0f / TILE_SIZE) + 1, 0, tile_map->width);
    I32 max_y = clamp_i((viewport.y + viewport.h) * (1.0f / TILE_SIZE) + 1, 0, tile_map->height);
    
    for (U64 x = min_x;
         x < max_x;
         ++x)
    {
        for (U64 y = min_y;
             y < max_y;
             ++y)
        {
            Tile tile = global_autotiler.tiles[tile_map->tiles[x + y * tile_map->width]];
            
            if (tile.visible &&
                tile.texture)
            {
                world_draw_sub_texture(rectangle_literal(x * TILE_SIZE,
                                                         y * TILE_SIZE,
                                                         TILE_SIZE,
                                                         TILE_SIZE),
                                       colour_literal(1.0f, 1.0f, 1.0f, 1.0f),
                                       tile.texture,
                                       tile.sub_texture);
            }
        }
    }
}

internal B32
serialise_tile_map(TileMap *tile_map,
                   S8 path)
{
    temporary_memory_begin(&global_static_memory);
    
    U64 buffer_size = sizeof(U32) * tile_map->width * tile_map->height + 2 * sizeof(U64);
    U8 *buffer = arena_allocate(&global_static_memory, buffer_size);
    
    U64 *dimensions = (U64 *)buffer;
    U32 *tile_kinds = (U32 *)(dimensions + 2);
    
    dimensions[0] = tile_map->width;
    dimensions[1] = tile_map->height;
    
    for (U64 i = 0;
         i < tile_map->width * tile_map->height;
         ++i)
    {
        tile_kinds[i] = tile_map->kinds[i];
    }
    
    B32 success = platform_write_entire_file(path, buffer, buffer_size);
    
    temporary_memory_end(&global_static_memory);
    
    return success;
}

internal B32
deserialise_tile_map(MemoryArena *memory,
                     TileMap *tile_map,
                     S8 path)
{
    S8 file = platform_read_entire_file(&global_frame_memory, path);
    
    if (file.buffer)
    {
        U64 *dimensions = (U64 *)file.buffer;
        U32 *tile_kinds = (U32 *)(dimensions + 2);
        
        initialise_tile_map(memory, tile_map, dimensions[0], dimensions[1]);
        
        for (U64 i = 0;
             i < tile_map->width * tile_map->height;
             ++i)
        {
            tile_map->kinds[i] = tile_kinds[i];
        }
        
        _refresh_auto_tiling(tile_map);
        
        return true;
    }
    else
    {
        return false;
    }
}

