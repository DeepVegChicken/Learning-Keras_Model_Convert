/*
 * app_tflm.h
 *
 *  Created on: 2022年2月10日
 *      Author: niu
 */

#ifndef APPLICATION_APP_TFLM_H_
#define APPLICATION_APP_TFLM_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include "tos_k.h"

#define DETECT_STK_SIZE       16*2048

extern k_task_t detect_task;
extern uint8_t detect_task_stk[DETECT_STK_SIZE];
void detect_entry(void *arg);

#define RAM_MODEL_SIZE (2*1024*1024)

void tensorflow_detect(void);
extern unsigned char ram_model[RAM_MODEL_SIZE];

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* APPLICATION_APP_TFLM_H_ */
