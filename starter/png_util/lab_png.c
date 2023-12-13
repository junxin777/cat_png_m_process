#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include "crc.h"
#include "lab_png.h"
#include "zutil.h"

// int is_png(U8 *buf);
// int get_png_height(struct data_IHDR *buf);
// int get_png_width(struct data_IHDR *buf);
// int get_png_data_IHDR(struct data_IHDR *data_IHDR, FILE *fp, long offset, int whence);

void printHexPNG(FILE *file)
{
    if (file == NULL)
    {
        printf("Failed to open the file.\n");
        return;
    }

    unsigned char buffer[8];
    if (fread(buffer, sizeof(unsigned char), 8, file) != 8)
    {
        printf("Failed to read the PNG header.\n");
        fclose(file);
        return;
    }

    for (int i = 0; i < 8; i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

    fclose(file);
}

int is_png(FILE *file)
{
    if (file == NULL)
    {
        printf("Failed to open the file in is_png.\n");
        fclose(file);
        return -1;
    }

    U8 buffer[8];

    fseek(file, 0, SEEK_SET);

    if (fread(buffer, sizeof(buffer), 1, file) != 1)
    {
        printf("Failed to read the PNG header.\n");
        fclose(file);
        return -1;
    }

    else
    {
        U8 validCode[3] = {0x50, 0x4E, 0x47};

        for (int i = 1; i < 4; i++)
        {
            if (validCode[i - 1] == buffer[i])
            {
                // printf("%d \n" , validCode[i - 1]);
                // printf("%d\n", buffer[i]);
                continue;
            }
            else
            {
                fclose(file);
                return 0;
            }
        }
        fclose(file);
        return 1;
    }
}

int get_png_height(struct data_IHDR *buf)
{
    return buf->height;
}

int get_png_width(struct data_IHDR *buf)
{
    return buf->width;
}

int get_png_data_IHDR(struct data_IHDR *data_IHDR, struct recv_buf_flat file)
{
    unsigned char buf[13];

    for (size_t i = 0; i < 13; i++)
    {
        buf[i] = file.data[16 + i];
    }

    // for (int i = 0; i < 25; i++)
    // {
    //     printf("%02hhx", (unsigned char)file.data[i]);
    // }
    // printf("\n");

    // printf("%02hhx, %02hhx, %02hhx, %02hhx\n", buf[0], buf[1], buf[2], buf[3]);
    // printf("%02hhx, %02hhx, %02hhx, %02hhx\n", buf[4], buf[5], buf[6], buf[7]);

    U32 width = combine_hex(buf, 0, 3);
    U32 height = combine_hex(buf, 4, 7);
    data_IHDR->width = width;
    data_IHDR->height = height;
    data_IHDR->bit_depth = buf[8];
    data_IHDR->color_type = buf[9];
    data_IHDR->compression = buf[10];
    data_IHDR->filter = buf[11];
    data_IHDR->interlace = buf[12];

    // for (size_t i = 0; i < 13; i++)
    // {
    //     printf("IHDR DATA: %x ", buf[i]);
    // }
    // printf("\n");

    return 0;
}

int combine_hex(unsigned char *buf, int start, int end)
{
    unsigned long combine = 0;
    for (int i = start; i <= end; i++)
    {
        combine = combine * 256 + buf[i];
    }
    return combine;
}

int get_chunk_IHDR(struct chunk *chunk_IHDR, struct recv_buf_flat file)
{
    // long offset = 8;
    // int whence = SEEK_SET;

    // int check = fseek(fp, offset, whence);
    // if (check != 0)
    // {
    //     printf("Error occurred while setting the file position (chunk_IHDR).\n");
    //     return -1;
    // }

    unsigned char len[4];

    for (size_t i = 0; i < 4; i++)
    {
        len[i] = file.data[8 + i];
    }

    chunk_IHDR->length = combine_hex(len, 0, 3);

    // fread(&(chunk_IHDR->length), 4, 1, fp);
    // chunk_IHDR->length = ntohl(chunk_IHDR->length);
    // printf("length: %d\n", chunk_IHDR->length);
    chunk_IHDR->type[0] = file.data[12];
    chunk_IHDR->type[1] = file.data[13];
    chunk_IHDR->type[2] = file.data[14];
    chunk_IHDR->type[3] = file.data[15];
    // printf("%ld\n", sizeof(chunk_IHDR->type));
    chunk_IHDR->p_data = malloc(chunk_IHDR->length * sizeof(U8));
    for (size_t i = 0; i < chunk_IHDR->length; i++)
    {
        chunk_IHDR->p_data[i] = file.data[16 + i];
    }

    // fread(chunk_IHDR->p_data, chunk_IHDR->length, 1, fp);
    // printf("%d\n", chunk_IHDR->p_data[8]);

    for (size_t i = 0; i < 4; i++)
    {
        len[i] = file.data[29 + i];
    }

    chunk_IHDR->crc = combine_hex(len, 0, 3);

    // fread(&(chunk_IHDR->crc), 4, 1, fp);
    // chunk_IHDR->crc = ntohl(chunk_IHDR->crc);
    // printf("crc: %x\n", chunk_IHDR->crc);

    return 0;
}

int get_chunk_IDAT(struct chunk *chunk_IDAT, struct recv_buf_flat file)
{
    // printf("ko\n");
    // long offset = 8 + 4 + 4 + 13 + 4;
    // int whence = SEEK_SET;

    // int check = fseek(file, offset, whence);
    // if (check != 0)
    // {
    //     perror("Error occurred while setting the file position (chunk_IHDR).\n");
    //     return 1;
    // }

    unsigned char len[4];

    for (size_t i = 0; i < 4; i++)
    {
        len[i] = file.data[33 + i];
    }

    chunk_IDAT->length = combine_hex(len, 0, 3);
    // fread(&(chunk_IDAT->length), 4, 1, fp);
    // chunk_IDAT->length = ntohl(chunk_IDAT->length);
    // printf("chunk_IDAT->length: %d\n", chunk_IDAT->length);

    chunk_IDAT->type[0] = file.data[37];
    chunk_IDAT->type[1] = file.data[38];
    chunk_IDAT->type[2] = file.data[39];
    chunk_IDAT->type[3] = file.data[40];
    // fread(&(chunk_IDAT->type), 4, 1, fp);

    // printf("length: %d\n", chunk_IDAT->length);

    chunk_IDAT->p_data = calloc(chunk_IDAT->length, sizeof(U8));
    for (size_t i = 0; i < chunk_IDAT->length; i++)
    {
        chunk_IDAT->p_data[i] = file.data[41 + i];
    }
    // fread(chunk_IDAT->p_data, chunk_IDAT->length, 1, fp);
    for (size_t i = 0; i < 4; i++)
    {
        len[i] = file.data[41 + chunk_IDAT->length + i];
    }

    chunk_IDAT->crc = combine_hex(len, 0, 3);
    // chunk_IDAT->crc = ntohl(chunk_IDAT->crc); //************************

    // fread(&(chunk_IDAT->crc), 4, 1, fp);
    // chunk_IDAT->crc = ntohl(chunk_IDAT->crc);
    // printf("ko\n");
    return 0;
}

int get_chunk_IEND(struct chunk *chunk_IEND, struct recv_buf_flat file, long offset)
{
    // int whence = SEEK_SET;

    // int check = fseek(fp, 33 + 4 + 4 + offset + 4, whence);
    // if (check != 0)
    // {
    //     perror("Error occurred while setting the file position (chunk_IHDR).\n");
    //     return 1;
    // }

    unsigned char len[4];

    for (size_t i = 0; i < 4; i++)
    {
        len[i] = file.data[33 + 4 + 4 + offset + 4 + i];
    }

    // fread(&(chunk_IEND->length), 4, 1, fp);
    // chunk_IEND->length = ntohl(chunk_IEND->length);

    chunk_IEND->type[0] = file.data[33 + 4 + 4 + offset + 4 + 4];
    chunk_IEND->type[1] = file.data[33 + 4 + 4 + offset + 4 + 5];
    chunk_IEND->type[2] = file.data[33 + 4 + 4 + offset + 4 + 6];
    chunk_IEND->type[3] = file.data[33 + 4 + 4 + offset + 4 + 7];
    // fread(&(chunk_IEND->type), 4, 1, fp);
    chunk_IEND->p_data = calloc(chunk_IEND->length, sizeof(U8));
    for (size_t i = 0; i < chunk_IEND->length; i++)
    {
        chunk_IEND->p_data[i] = file.data[33 + 4 + 4 + offset + 4 + 8 + i];
    }
    // fread(chunk_IEND->p_data, chunk_IEND->length, 1, fp);

    for (size_t i = 0; i < 4; i++)
    {
        len[i] = file.data[33 + 4 + 4 + offset + 4 + 8 + chunk_IEND->length + i];
    }

    chunk_IEND->crc = combine_hex(len, 0, 3);
    // fread(&(chunk_IEND->crc), 4, 1, fp);
    // chunk_IEND->crc = ntohl(chunk_IEND->crc);

    return 0;
}

U32 get_crc(chunk_p chunk)
{
    // length of uncompressed data is type + length
    U64 len_def = 4 + chunk->length;
    U32 crc_val = 0;
    U8 *buf = (U8 *)calloc(len_def, sizeof(U8));
    for (int i = 0; i < 4; i++)
    {
        buf[i] = chunk->type[i];
    }

    for (int i = 4; i < 4 + chunk->length; i++)
    {
        buf[i] = chunk->p_data[i - 4];
    }

    crc_val = crc(buf, len_def);

    free(buf);

    return crc_val;
}

/*
    return_chuunk: indecate which chunk is corrupted
    calc_crc: computed crc value, used in output
    chunk_crc: got from chunk crc section, used in output
*/
int check_corrupted(struct recv_buf_flat file, int *return_chunk, U32 *calc_crc, U32 *chunk_crc)
{
    chunk_p chunk_IHDR = (chunk_p)calloc(1, sizeof(chunk_p));
    get_chunk_IHDR(chunk_IHDR, file);
    *calc_crc = get_crc(chunk_IHDR);
    *chunk_crc = chunk_IHDR->crc;
    if (*calc_crc != *chunk_crc)
    {
        // printf("bad IHDR\n");
        *return_chunk = 1;
        free(chunk_IHDR->p_data);
        free(chunk_IHDR);
        return 1;
    }

    chunk_p chunk_IDAT = (chunk_p)calloc(1, sizeof(chunk_p));
    get_chunk_IDAT(chunk_IDAT, file);
    *calc_crc = get_crc(chunk_IDAT);
    *chunk_crc = chunk_IDAT->crc;
    if (*calc_crc != *chunk_crc)
    {
        // printf("bad IDAT\n");
        *return_chunk = 2;
        free(chunk_IHDR->p_data);
        free(chunk_IDAT->p_data);
        free(chunk_IHDR);
        free(chunk_IDAT);
        return 2;
    }

    chunk_p chunk_IEND = (chunk_p)calloc(1, sizeof(chunk_p));
    get_chunk_IEND(chunk_IEND, file, chunk_IDAT->length);
    *calc_crc = get_crc(chunk_IEND);
    *chunk_crc = chunk_IEND->crc;
    if (*calc_crc != *chunk_crc)
    {
        // printf("bad IEND\n");
        *return_chunk = 3;
        free(chunk_IHDR->p_data);
        free(chunk_IDAT->p_data);
        free(chunk_IEND->p_data);
        free(chunk_IHDR);
        free(chunk_IDAT);
        free(chunk_IEND);
        return 3;
    }

    // printf("No corruption\n");
    free(chunk_IHDR->p_data);
    free(chunk_IDAT->p_data);
    free(chunk_IEND->p_data);
    free(chunk_IHDR);
    free(chunk_IDAT);
    free(chunk_IEND);
    return 0;
}