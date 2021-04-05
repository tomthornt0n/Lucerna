#include <immintrin.h>

#define stack_array_size(_arr) (sizeof(_arr) / sizeof(_arr[0]))

#define defer_loop(_begin, _end) for (I32 _i = ((_begin), 0); !_i; ((_end), (_i = 1)))

#define rectangle_literal(_x, _y, _w, _h) ((Rect){ (_x), (_y), (_w), (_h) })
typedef struct
{
 F32 x, y;
 F32 w, h;
} Rect;

#define colour_literal(_r, _g, _b, _a) ((Colour){ (_r), (_g), (_b), (_a) })
#define WHITE colour_literal(1.0f, 1.0f, 1.0f, 1.0f)
typedef struct
{
 F32 r, g, b, a;
} Colour;

#define gradient_literal(_tl, _tr, _bl, _br) ((Gradient){ (_tl), (_tr), (_bl), (_br) })
typedef struct
{
 Colour tl, tr, bl, br;
} Gradient;


internal inline F64
min_f(F64 a,
      F64 b)
{
 return a < b ? a : b;
}

internal inline F64
max_f(F64 a,
      F64 b)
{
 return a > b ? a : b;
}

internal inline F64
clamp_f(F64 n,
        F64 min, F64 max)
{
 return max_f(min, min_f(n, max));
}

internal F32
reciprocal_sqrt_f(F32 n)
{
 union FloatAsInt { F32 f; I32 i; } i;
 
 i.f = n;
 i.i = 0x5f375a86 - (i.i >> 1);
 i.f *= 1.5f - (i.f * 0.5f * i.f * i.f);
 
 return i.f;
}

internal inline I64
min_i(I64 a,
      I64 b)
{
 return a < b ? a : b;
}

internal inline I64
max_i(I64 a,
      I64 b)
{
 return a > b ? a : b;
}

internal inline I64
clamp_i(I64 n,
        I64 min, I64 max)
{
 return max_f(min, min_f(n, max));
}

internal inline U64
min_u(U64 a,
      U64 b)
{
 return a < b ? a : b;
}

internal inline U64
max_u(U64 a,
      U64 b)
{
 return a > b ? a : b;
}

internal inline U64
clamp_u(U64 n,
        U64 min, U64 max)
{
 return max_f(min, min_f(n, max));
}

internal inline Rect
offset_rectangle(Rect rectangle,
                 F32 offset_x, F32 offset_y)
{
 return rectangle_literal(rectangle.x + offset_x,
                          rectangle.y + offset_y,
                          rectangle.w,
                          rectangle.h);
}

internal inline Colour
colour_lerp(Colour a,
            Colour b,
            F32 t)
{
 Colour result;
 
 result.r = (a.r * t) + (b.r * (1.0f - t));
 result.g = (a.g * t) + (b.g * (1.0f - t));
 result.b = (a.b * t) + (b.b * (1.0f - t));
 result.a = (a.a * t) + (b.a * (1.0f - t));
 
 return result;
}

internal B32
rect_match(Rect a,
           Rect b)
{
 F32 slop = 0.0015;
 
 if (a.x < (b.x - slop) || a.x > (b.x + slop)) { return false; }
 if (a.y < (b.y - slop) || a.y > (b.y + slop)) { return false; }
 if (a.w < (b.w - slop) || a.w > (b.w + slop)) { return false; }
 if (a.h < (b.h - slop) || a.h > (b.h + slop)) { return false; }
 
 return true;
}

internal inline B32
are_rectangles_intersecting(Rect a,
                            Rect b)
{
 if (a.x + a.w < b.x || a.x > b.x + b.w) { return false; }
 if (a.y + a.h < b.y || a.y > b.y + b.h) { return false; }
 return true;
}

internal inline B32
is_point_in_region(F32 x, F32 y,
                   Rect region)
{
 return !(x < region.x              ||
          y < region.y              ||
          x > (region.x + region.w) ||
          y > (region.y + region.h));
}

internal void
matrix_transform_point(F32 x, F32 y,
                       F32 *matrix, // NOTE(tbt): 4x4 matrix
                       F32 *result_x,
                       F32 *result_y)
{
 F32 vector[4] = { x, y, 0.0f, 0.0f };
 
 __m128 vector_column = _mm_load_ps(vector);
 __m128 matrix_row, res;
 F32 mul_results[4];
 
 matrix_row = _mm_load_ps(matrix);
 res = _mm_mul_ps(matrix_row, vector_column);
 _mm_store_ps(mul_results, res);
 
 *result_x = mul_results[0] + mul_results[1] + mul_results[2] + mul_results[3];
 
 //-
 
 matrix_row = _mm_load_ps(matrix + 4);
 res = _mm_mul_ps(matrix_row, vector_column);
 _mm_store_ps(mul_results, res);
 
 *result_x = mul_results[0] + mul_results[1] + mul_results[2] + mul_results[3];
}

