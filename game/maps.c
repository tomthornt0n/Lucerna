/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 02 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

internal B32
save_map(void)
{
} 

typedef enum
{
    LOAD_MAP_STATUS_success,
    LOAD_MAP_STATUS_file_not_found,
    LOAD_MAP_STATUS_error,
} LoadMapStatus;

internal LoadMapStatus
load_map(OpenGLFunctions *gl,
         I8 *path)
{
}

internal void
create_new_map(OpenGLFunctions *gl,
               U32 tilemap_width,
               U32 tilemap_height)
{
}

