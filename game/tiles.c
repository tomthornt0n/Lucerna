typedef enum
{
    TILE_KIND_none,
    TILE_KIND_grass,
    TILE_KIND_dirt,
    TILE_KIND_trees,
} TileKind;

typedef struct
{
    Asset *texture;
    SubTexture sub_texture;
    B32 solid;
    B32 visible;
} Tile;

internal struct
{
    Tile tiles[36];
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

    SubTexture grass_sub_textures[3];
    slice_animation(grass_sub_textures, spritesheet->texture, 192, 0, 16, 16, 1, 3);

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

    SubTexture tree_sub_textures[16];
    slice_animation(tree_sub_textures, spritesheet->texture, 128, 0, 16, 16, 4, 4);

    global_autotiler.tree_tiles = global_autotiler.tiles + 20;
    for (I32 i = 0;
         i < 16;
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
                    TileMap *map,
                    U64 width, U64 height)
{
    map->tiles  = arena_allocate(memory, width * height * sizeof(*map->tiles));
    map->kinds  = arena_allocate(memory, width * height * sizeof(*map->kinds));
    map->width  = width;
    map->height = height;
}

internal void _refresh_auto_tiling(TileMap *tile_map);

internal void
place_tile(TileMap *tile_map,
           TileKind kind,
           U64 x, U64 y)
{
    if (x > tile_map->width)  { return; }
    if (y > tile_map->height) { return; }

    tile_map->kinds[x + y * tile_map->width] = kind;
    _refresh_auto_tiling(tile_map);
}

internal void
_auto_tile_random(TileMap *tile_map,
                  U64 index,
                  Tile *tile_set,
                  U32 zero_bias, // NOTE(tbt): bias towards 0 for more natural looking grass, etc.
                  U64 tile_set_size)
{
    if (index > tile_map->width * tile_map->height) { return; }

    Tile *tile = &tile_set[max_i(0, rand() % (tile_set_size * (zero_bias << 1)) - (tile_set_size * zero_bias))];
    U32 tile_index = tile - global_autotiler.tiles;
    tile_map->tiles[index] = tile_index;
}

internal void
_auto_tile_bitwise(TileMap *tile_map,
                   U64 index,
                   Tile *tile_set,
                   TileKind kind)
{
    if (index > tile_map->width * tile_map->height) { return; }

    U8 mask = 0;

    if (index - tile_map->width > 0)
    {
        mask |= (1 << 0) * (tile_map->kinds[index - tile_map->width] == kind);
    }

    if (index - 1 > 0 &&
        index % tile_map->width)
    {
        mask |= (1 << 1) * (tile_map->kinds[index - 1] == kind);
    }

    if (index + 1 < tile_map->width * tile_map->height &&
        (index + 1) % tile_map->width)
    {
        mask |= (1 << 2) * (tile_map->kinds[index + 1] == kind);
    }

    if (index + tile_map->width < tile_map->width * tile_map->height)
    {
        mask |= (1 << 3) * (tile_map->kinds[index + tile_map->width] == kind);
    }

    Tile *tile = &tile_set[mask];
    U32 tile_index = tile - global_autotiler.tiles;
    tile_map->tiles[index] = tile_index;
}

internal void
_refresh_auto_tiling(TileMap *tile_map)
{
    srand(0);
    for (U64 i = 0;
         i < tile_map->width * tile_map->height;
         ++i)
    {
        switch (tile_map->kinds[i])
        {
            case TILE_KIND_none:
                break;
            case TILE_KIND_grass:
                _auto_tile_random(tile_map, i, global_autotiler.grass_tiles, 1, 3);
                break;
            case TILE_KIND_dirt:
                _auto_tile_bitwise(tile_map, i, global_autotiler.dirt_tiles, TILE_KIND_dirt);
                break;
            case TILE_KIND_trees:
                _auto_tile_bitwise(tile_map, i, global_autotiler.tree_tiles, TILE_KIND_trees);
                break;
        }
    }
}

internal void
render_tile_map(TileMap *tile_map)
{
    static U32 tile_size = 64;

    for (U64 x = 0;
         x < tile_map->width;
         ++x)
    {
        for (U64 y = 0;
             y < tile_map->height;
             ++y)
        {
            Tile tile = global_autotiler.tiles[tile_map->tiles[x + y * tile_map->width]];

            if (tile.visible &&
                tile.texture)
            {
                world_draw_sub_texture(rectangle_literal(x * tile_size,
                                                         y * tile_size,
                                                         tile_size,
                                                         tile_size),
                                       colour_literal(1.0f, 1.0f, 1.0f, 1.0f),
                                       tile.texture,
                                       tile.sub_texture);
            }
        }
    }
}

