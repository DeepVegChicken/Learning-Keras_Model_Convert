/*
 * bmp.c
 *
 *  Created on: 2022年3月8日
 *      Author: niu
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <cr_section_macros.h>
typedef struct
{
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t off_bits;
}__attribute__ ((packed)) bmp_file_header_t;

typedef struct
{
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t size_image;
    uint32_t x_pels_permeter;
    uint32_t y_pels_permeter;
    uint32_t clr_used;
    uint32_t clr_important;
} bmp_info_header_t;

static bmp_file_header_t s_bmp_file_header = { 0x4d42, 0, 0, 0, 0 };
static bmp_info_header_t s_bmp_info_header = { 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0 };

char out_file_path[256] = "out.bmp";

__BSS(RAM2) uint8_t adaptor_ram[1000*480*4];
uint32_t adaptor_idx = 0;

void adaptor_file_to_ram(const void *p , size_t _size, size_t _n, FILE * nouse)
{
	if(adaptor_idx > sizeof(adaptor_ram)) {
		printf("adaptor_idx error");
		return;
	}

	memcpy(&adaptor_ram[adaptor_idx], p, _size*_n);
	adaptor_idx +=  _size*_n;
}

int fatfs_write_file(char * name, void* p, uint32_t size);
int32_t dump_image_to_bmp_file(const char *file_path, uint8_t *image, uint32_t width, uint32_t height)
{
    FILE *file = NULL;
    int32_t err = 0;

    const uint32_t real_width = 600;
    do {
        if (NULL == file_path || NULL == image)
        {
            err = -1;
            break;
        }

        //uint32_t line_width = (width + 3) / 4 * 4;
        s_bmp_file_header.off_bits = sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t)
                + 4 * 256;
        s_bmp_file_header.size = s_bmp_file_header.off_bits + real_width * height * 4;

        s_bmp_info_header.size = sizeof(bmp_info_header_t);
        s_bmp_info_header.width = real_width;
        s_bmp_info_header.height = height;
        s_bmp_info_header.size_image = real_width * height * 4;

        //printf("[%s] line_width = %d, width = %d, height = %d\n", __func__, line_width, width, height);

        //file = fopen(file_path, "wb");
//        if (NULL == file)
//        {
//			err = -1;
//            break;
//        }

        adaptor_file_to_ram(&s_bmp_file_header.type, 1, sizeof(s_bmp_file_header.type), file);
        adaptor_file_to_ram(&s_bmp_file_header.size, 1, sizeof(s_bmp_file_header.size), file);
        adaptor_file_to_ram(&s_bmp_file_header.reserved1, 1, sizeof(s_bmp_file_header.reserved1), file);
        adaptor_file_to_ram(&s_bmp_file_header.reserved2, 1, sizeof(s_bmp_file_header.reserved2), file);
        adaptor_file_to_ram(&s_bmp_file_header.off_bits, 1, sizeof(s_bmp_file_header.off_bits), file);

        adaptor_file_to_ram(&s_bmp_info_header, 1, sizeof(bmp_info_header_t), file);
        uint8_t alpha = 0;
        int32_t i;
        for (i = 0; i < 256; i++)
        {
        	adaptor_file_to_ram(&i, 1, sizeof(uint8_t), file);
        	adaptor_file_to_ram(&i, 1, sizeof(uint8_t), file);
        	adaptor_file_to_ram(&i, 1, sizeof(uint8_t), file);
        	adaptor_file_to_ram(&alpha, 1, sizeof(uint8_t), file);
        }

        for (i = height - 1; i >= 0; i--)
        {
        	adaptor_file_to_ram(image + i * width *4, 1, real_width * 4, file);
//            if (line_width > width)
//            {
//                uint8_t line_align[4] = { 0 };
//                adaptor_file_to_ram(line_align, 1, line_width - width, file);
//            }
        }

        //fflush(file);
    } while (0);

    if (file != NULL)
    {
        //fclose(file);
    }

    fatfs_write_file("wa", adaptor_ram, adaptor_idx);
    adaptor_idx = 0;

    return err;
}
