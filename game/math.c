/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna
  
  Author  : Tom Thornton
  Updated : 26 Nov 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

internal void
add_vector2f(F32 *result,
             F32 *vector1,
             F32 *vector2)
{
    result[0] = vector1[0] + vector2[0];
    result[1] = vector1[1] + vector2[1];
}

internal void
subtract_vector2f(F32 *result,
                  F32 *vector1,
                  F32 *vector2)
{
    result[0] = vector1[0] - vector2[0];
    result[1] = vector1[1] - vector2[1];
}

internal void
multiply_vector2f(F32 *result,
                  F32 *vector1,
                  F32 *vector2)
{
    result[0] = vector1[0] * vector2[0];
    result[1] = vector1[1] * vector2[1];
}

internal void
divide_vector2f(F32 *result,
                F32 *vector1,
                F32 *vector2)
{
    result[0] = vector1[0] / vector2[0];
    result[1] = vector1[1] / vector2[1];
}

internal F32
dot_vector2f(F32 *vector1,
             F32 *vector2)
{
    return ((vector1[0] * vector2[0]) +
            (vector1[1] * vector2[1]));
}

internal void
add_vector3f(F32 *result,
             F32 *vector1,
             F32 *vector2)
{
    result[0] = vector1[0] + vector2[0];
    result[1] = vector1[1] + vector2[1];
    result[2] = vector1[2] + vector2[2];
}

internal void
subtract_vector3f(F32 *result,
                  F32 *vector1,
                  F32 *vector2)
{
    result[0] = vector1[0] - vector2[0];
    result[1] = vector1[1] - vector2[1];
    result[2] = vector1[2] - vector2[2];
}

internal void
multiply_vector3f(F32 *result,
                  F32 *vector1,
                  F32 *vector2)
{
    result[0] = vector1[0] * vector2[0];
    result[1] = vector1[1] * vector2[1];
    result[2] = vector1[2] * vector2[2];
}

internal void
divide_vector3f(F32 *result,
                F32 *vector1,
                F32 *vector2)
{
    result[0] = vector1[0] / vector2[0];
    result[1] = vector1[1] / vector2[1];
    result[2] = vector1[2] / vector2[2];
}

internal F32
dot_vector3f(F32 *vector1,
             F32 *vector2)
{
    return ((vector1[0] * vector2[0]) +
            (vector1[1] * vector2[1]) +
            (vector1[2] * vector2[2]));
}

internal void
add_vector4f(F32 *result,
             F32 *vector1,
             F32 *vector2)
{
    __m128 left, right, answer;

    left   = _mm_load_ps(vector1);
    right  = _mm_load_ps(vector2);
    answer = _mm_add_ps(left, right);

    _mm_store_ps(result, answer);
}

internal void
subtract_vector4f(F32 *result,
                  F32 *vector1,
                  F32 *vector2)
{
    __m128 left, right, answer;

    left   = _mm_load_ps(vector1);
    right  = _mm_load_ps(vector2);
    answer = _mm_sub_ps(left, right);

    _mm_store_ps(result, answer);
}

internal void
multiply_vector4f(F32 *result,
                  F32 *vector1,
                  F32 *vector2)
{
    __m128 left, right, answer;

    left   = _mm_load_ps(vector1);
    right  = _mm_load_ps(vector2);
    answer = _mm_mul_ps(left, right);
    
    _mm_store_ps(result, answer);
}

internal void
divide_vector4f(F32 *result,
                F32 *vector1,
                F32 *vector2)
{
    __m128 left, right, answer;

    left   = _mm_load_ps(vector1);
    right  = _mm_load_ps(vector2);
    answer = _mm_div_ps(left, right);

    _mm_store_ps(result, answer);
}

internal F32
dot_vector4f(F32 *vector1,
             F32 *vector2)
{
    return ((vector1[0] * vector2[0]) +
            (vector1[1] * vector2[1]) +
            (vector1[2] * vector2[2]) +
            (vector1[3] * vector2[3]));
}

internal void
multiply_matrix4f(F32 *result,
                  F32 *matrix1,
                  F32 *matrix2)
{
    const __m128 matrix2Column1 = _mm_load_ps(&(matrix1[0]));
    const __m128 matrix2Column2 = _mm_load_ps(&(matrix1[4]));
    const __m128 matrix2Column3 = _mm_load_ps(&(matrix1[8]));
    const __m128 matrix2Column4 = _mm_load_ps(&(matrix1[12]));

    F32 *leftRowPointer = &matrix2[0];
    F32 *resultRowPointer = &result[0];

    int i;
    for (i = 0;
         i < 4;
         ++i, leftRowPointer += 4, resultRowPointer += 4)
    {
        __m128 matrix1Row1 = _mm_set1_ps(leftRowPointer[0]);
        __m128 matrix1Row2 = _mm_set1_ps(leftRowPointer[1]);
        __m128 matrix1Row3 = _mm_set1_ps(leftRowPointer[2]);
        __m128 matrix1Row4 = _mm_set1_ps(leftRowPointer[3]);

        __m128 x = _mm_mul_ps(matrix1Row1, matrix2Column1);
        __m128 y = _mm_mul_ps(matrix1Row2, matrix2Column2);
        __m128 z = _mm_mul_ps(matrix1Row3, matrix2Column3);
        __m128 w = _mm_mul_ps(matrix1Row4, matrix2Column4);

        __m128 result = _mm_add_ps(_mm_add_ps(x, y), _mm_add_ps(z, w));
        _mm_store_ps(resultRowPointer, result);
    }
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

internal void
generate_translation_matrix(F32 *matrix,
                            F32 x,
                            F32 y)
{
    memset(matrix, 0, 16 * sizeof(F32));

    matrix[0]  = 1.0f;
    matrix[5]  = 1.0f;
    matrix[10] = 1.0f;
    matrix[12] = x;
    matrix[13] = y;
    matrix[15] = 1.0f;
}

internal void
generate_scale_matrix(F32 *matrix,
                      F32 x,
                      F32 y)
{
    memset(matrix, 0, 16 * sizeof(F32));

    matrix[0]  = x;
    matrix[5]  = y;
    matrix[10] = 1.0f;
    matrix[15] = 1.0f;
}


/*
MIT License

Copyright (c) 2020 Tom Thornton

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

