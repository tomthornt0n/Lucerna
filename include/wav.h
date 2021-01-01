/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  wav.h

  Author  : Thomas Thornton

  Licence : MIT (at bottom of file)

  Notes   : Simple c89 single header file library for reading uncompressed WAV
            files

  Usage   : See README.md

 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#ifndef WAV_H
#define WAV_H

#include <stdio.h>

#ifndef WAV_ASSERT
#include <assert.h>
#define WAV_ASSERT(condition) assert(condition)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void wav_read(char *path,
              int *data_rate,
              int *bits_per_sample,
              int *channels, 
              int *data_size,
              void *data);

#ifdef __cplusplus
}
#endif

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#ifdef WAV_IMPLEMENTATION

typedef unsigned short wav_u16;
typedef unsigned int   wav_u32;

typedef unsigned char  wav_b8;
#define WAV_TRUE 1
#define WAV_FALSE 0

#define WAV_RIFF_CODE(a, b, c, d) (((wav_u32)(a) << 0)  |                      \
                                   ((wav_u32)(b) << 8)  |                      \
                                   ((wav_u32)(c) << 16) |                      \
                                   ((wav_u32)(d) << 24))

#pragma pack(push, 0)
struct wav_riff_header
{
    wav_u32 id;
    wav_u32 size;
};

struct wav_fmt_chunk
{
    wav_u16 w_format_tag;
    wav_u16 n_channels;
    wav_u32 n_samples_per_sec;
    wav_u32 n_avg_bytes_per_sec;
    wav_u16 n_block_align;
    wav_u16 w_bits_per_sample;
};
#pragma pack(pop)

void
wav_read(char *filename,
         int *data_rate,
         int *bits_per_sample,
         int *channels,
         int *data_size,
         void *data)
{
    FILE *file;
    int bytes_read;

    struct wav_riff_header header;
    wav_u32 wave;

    wav_b8 fmt_read = WAV_FALSE, data_read = WAV_FALSE;

    WAV_ASSERT(sizeof(wav_u16) == 2);
    WAV_ASSERT(sizeof(wav_u32) == 4);

    WAV_ASSERT(filename);

    file = fopen(filename, "rb");
    WAV_ASSERT(file);
    
    bytes_read = fread(&header, 1, sizeof(header), file);
    WAV_ASSERT(bytes_read == sizeof(header));
    WAV_ASSERT(header.id == WAV_RIFF_CODE('R', 'I', 'F', 'F'));

    bytes_read = fread(&wave, 1, sizeof(wave), file);
    WAV_ASSERT(bytes_read == sizeof(wave));
    WAV_ASSERT(wave == WAV_RIFF_CODE('W', 'A', 'V', 'E'));

    while (!feof(file))
    {
        struct wav_fmt_chunk fmt;

        bytes_read = fread(&header, 1, sizeof(header), file);
        if (bytes_read != sizeof(header)) break;

        switch (header.id)
        {
            case WAV_RIFF_CODE('f', 'm', 't', ' '):
            {
                fmt_read = WAV_TRUE;

                WAV_ASSERT(header.size == sizeof(fmt));

                bytes_read = fread(&fmt, 1, header.size, file);
                WAV_ASSERT(bytes_read == sizeof(fmt));

                WAV_ASSERT(fmt.w_format_tag == 1); /* only uncompressed pcm data is supported */
                WAV_ASSERT(fmt.n_block_align == (fmt.w_bits_per_sample / 8) * fmt.n_channels);
                WAV_ASSERT(fmt.n_avg_bytes_per_sec == fmt.n_samples_per_sec * fmt.n_block_align);

                if (data_rate) { *data_rate = fmt.n_samples_per_sec; }
                if (channels) { *channels = fmt.n_channels; }
                if (bits_per_sample) { *bits_per_sample = fmt.w_bits_per_sample; }

                break;
            }
            case WAV_RIFF_CODE('d', 'a', 't', 'a'):
            {
                data_read = WAV_TRUE;

                if (data_size) { *data_size = header.size; }

                if (data)
                {
                    bytes_read = fread(data, 1, header.size, file);
                    WAV_ASSERT(bytes_read == header.size);
                }

                break;
            }
            default:
            {
                fseek(file, header.size, SEEK_CUR);

                break;
            }
        }

        if (fmt_read &&
            data_read)
        {
            break;
        }
    }

    WAV_ASSERT(fmt_read && data_read);

    fclose(file);
}
#endif
#endif 

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

