/*
 * utils.h
 *
 *  Created on: 2022年2月10日
 *      Author: niu
 */

#ifndef APPLICATION_UTILS_H_
#define APPLICATION_UTILS_H_

extern const uint32_t width;
extern const uint32_t height;

int fatfs_get_model(void* pModel, uint32_t size);
void input_data_process(uint32_t* frame);
int nms(float threshold, uint32_t* frame);
void clear_top_n(void);
void push_top_n(float conf, int32_t x_1, int32_t y_1, int32_t x_2, int32_t y_2, int32_t layer);

#endif /* APPLICATION_UTILS_H_ */
