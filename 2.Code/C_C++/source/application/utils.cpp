/*
 * utils.c
 *
 *  Created on: 2022年2月10日
 *      Author: niu
 */
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include "tensorflow/lite/version.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "display_support.h"

/* 从sd卡中获取tflite模型 */
int fatfs_get_model(void* pModel, uint32_t size)
{
	DIR DirInfo;
	FILINFO FilInfo;
	FIL File;
	FRESULT error;
	UINT bytesRead;
	UINT bytesModel = 0;
	char name[64];

    if(f_opendir(&DirInfo, (const TCHAR*)"/") == FR_OK)/* 打开文件夹目录成功，目录信息已经在dir结构体中保存 */
    {
        while(f_readdir(&DirInfo, &FilInfo) == FR_OK)  /* 读文件信息到文件状态结构体中 */
        {
            if(!FilInfo.fname[0])
            	break; /* 如果文件名为‘\0'，说明读取完成结束 */

			if(strstr(FilInfo.fname, "tflite") != NULL)
			{
				printf("-> %s | %lu KB <-\r\n", FilInfo.fname, FilInfo.fsize / 1024);
				break;
			}
        }

        bytesModel = FilInfo.fsize;
        strncpy(name, FilInfo.fname, sizeof(name));
        f_closedir(&DirInfo);
    }

    if(bytesModel > size)
    {
    	printf("\tmodel size error\r\n");
    	return -1;
    }

	error = f_open(&File, name, FA_READ);
	if (error)
	{
		if (error == FR_EXIST)
		{
			printf("\tFile exists.\r\n");
		}
		else
		{
			printf("\tOpen file failed.\r\n");
			return -1;
		}
	}

	error = f_read(&File, pModel, FilInfo.fsize, &bytesRead);
	if (error)
	{
		printf("\tRead file failed. \r\n");
		return -1;
	}

	if(bytesRead != FilInfo.fsize)
	{
		printf("\tRead size %lu %d.\r\n", FilInfo.fsize, bytesRead);
		return -1;
	}

	printf("\tread model file ok. \r\n");

	return 0;
}


uint32_t scale_x = 4;
uint32_t scale_y = 3;
const uint32_t width  = 800;
const uint32_t height = 480;

#define INPUT_DATA_MODE 3
extern TfLiteTensor* input;

void input_data_process(uint32_t* frame)
{
	uint32_t pixel;
	uint32_t input_size = input->dims->data[1];
	//scale = height / input_size;

	float q_scale = 255.0*input->params.scale;
	for(uint32_t y = 0; y < input_size; y++) {
		for(uint32_t x = 0; x < input_size; x++) {
			pixel = *(frame + y*width*scale_y + x*scale_x);
#if INPUT_DATA_MODE == 0
			input->data.int8[3*(y*input_size + x) ] 	= ((pixel & 0x00ff0000) >> 16) - 128;	// scale倍降采样
			input->data.int8[3*(y*input_size + x) + 1] 	= ((pixel & 0x0000ff00) >>  8) - 128;
			input->data.int8[3*(y*input_size + x) + 2] 	= ( pixel & 0x000000ff) - 128;

#elif INPUT_DATA_MODE == 1


#elif INPUT_DATA_MODE == 2
			/* 耗时比较长160*160分辨率 大概130ms */
			input->data.int8[3*(y*input_size + x) ] 	= ((pixel & 0x00ff0000) >> 16)	/255.0/input->params.scale - input->params.zero_point;	// scale倍降采样
			input->data.int8[3*(y*input_size + x) + 1] 	= ((pixel & 0x0000ff00) >>  8)	/255.0/input->params.scale - input->params.zero_point;
			input->data.int8[3*(y*input_size + x) + 2] 	= ( pixel & 0x000000ff)		/255.0/input->params.scale - input->params.zero_point;
#elif INPUT_DATA_MODE == 3
			input->data.int8[3*(y*input_size + x) ] 	= ((pixel & 0x00ff0000) >> 16)	/q_scale - input->params.zero_point;	// scale倍降采样
			input->data.int8[3*(y*input_size + x) + 1] 	= ((pixel & 0x0000ff00) >>  8)	/q_scale - input->params.zero_point;
			input->data.int8[3*(y*input_size + x) + 2] 	= ( pixel & 0x000000ff)			/q_scale - input->params.zero_point;
#elif INPUT_DATA_MODE == 4
			input->data.f[3*(y*input_size + x) ] 	= ((pixel & 0x00ff0000) >> 16)	/255.0;	// scale倍降采样
			input->data.f[3*(y*input_size + x) + 1] = ((pixel & 0x0000ff00) >>  8)	/255.0;
			input->data.f[3*(y*input_size + x) + 2] = ( pixel & 0x000000ff)			/255.0;
#endif
		}
	}
}

#define TOP_N_SIZE 5
typedef struct {
	float conf;
	int32_t x_1;
	int32_t y_1;
	int32_t x_2;
	int32_t y_2;
	int32_t layer;
} yolo_t;

yolo_t top[TOP_N_SIZE];

void clear_top_n(void)
{
	memset(top, 0, sizeof(top));
}

void push_top_n(float conf, int32_t x_1, int32_t y_1, int32_t x_2, int32_t y_2, int32_t layer)
{
	for(uint32_t i = 0; i < TOP_N_SIZE; i++) {
		if(conf > top[i].conf) {
			for(uint32_t j = TOP_N_SIZE - 1; j > i; j--) {
				top[j] = top[j - 1];
			}
			top[i].conf = conf;
			top[i].x_1 = scale_x * x_1;
			top[i].x_2 = scale_x * x_2;
			top[i].y_1 = scale_y * y_1;
			top[i].y_2 = scale_y * y_2;
			top[i].layer = layer;
			break;
		}
	}
}

int32_t max(int32_t x, int32_t y)
{
	return x > y ? x : y;
}

int32_t min(int32_t x, int32_t y)
{
	return x < y ? x : y;
}

float IoU(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t u1, int32_t v1, int32_t u2, int32_t v2)
{
	float inters = max(min(x2, u2) - max(x1, u1), 0.0) * max(min(y2, v2) - max(y1, v1), 0.0);
	float unions = (x2 - x1)*(y2 - y1) + (u2 - u1)*(v2 - v1) - inters;
	return inters / unions;
}
extern "C" {
void display_print(uint32_t * frame, int pos_x, int pos_y, uint32_t color, const char *fmt,...);
}

int nms(float threshold, uint32_t* frame)
{
	int cnt = 0;

	for(uint32_t i = 0; i < TOP_N_SIZE; i++) {
		if(top[i].conf < 0.01)
			continue;
		for(uint32_t n = i + 1; n < TOP_N_SIZE; n++) {
			if(top[n].conf < 0.01)
				break;

			float iou = IoU(top[i].x_1, top[i].y_1, top[i].x_2, top[i].y_2,
							top[n].x_1, top[n].y_1, top[n].x_2, top[n].y_2);
			if(iou > threshold) {
				top[n].conf = 0;
			}
		}
	}

	for(uint32_t i = 0; i < TOP_N_SIZE; i++) {
		if(top[i].conf < 0.01)
			continue;

		uint32_t color;
		if(top[i].layer == 0)
			color = 0x00ff0000;		// 5*5	 红色
		else if(top[i].layer == 1)
			color = 0x0000ff00;		// 10*10 绿色
		else
			color = 0x000000ff;		// 20*20 蓝色
		cnt++;
		display_draw_rect(top[i].x_1, top[i].y_1, top[i].x_2, top[i].y_2, frame, color);
		display_print(frame, top[i].x_1 + 3, top[i].y_1 + 3,  color, "%.2f", top[i].conf);
	}

	return cnt;
}
