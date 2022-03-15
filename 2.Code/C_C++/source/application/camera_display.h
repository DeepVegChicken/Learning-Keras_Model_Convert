/*
 * camera_display.h
 *
 *  Created on: 2022年2月11日
 *      Author: niu
 */

#ifndef APPLICATION_CAMERA_DISPLAY_H_
#define APPLICATION_CAMERA_DISPLAY_H_

#define CAMERA_STK_SIZE       4096
#include "tos_k.h"
extern k_event_t event;
extern k_event_t event_2;
extern k_task_t camera_task;
extern uint8_t camera_task_stk[CAMERA_STK_SIZE];

extern const k_event_flag_t event_camera;
extern const k_event_flag_t event_detect;
extern const k_event_flag_t event_detect_done;
extern const k_event_flag_t event_diaplay;
extern const k_event_flag_t event_mqtt;

extern void *lcdFrameAddr;
void camera_entry(void *arg);

void APP_BufferSwitchOffCallback(void *param, void *switchOffBuffer);
void APP_InitCamera(void);
void APP_InitDisplay(void);
void APP_InitPxP(void);

#endif /* APPLICATION_CAMERA_DISPLAY_H_ */
