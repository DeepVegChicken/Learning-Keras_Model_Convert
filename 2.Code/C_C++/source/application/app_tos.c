/*
 * app_tos.c
 *
 *  Created on: 2022年2月10日
 *      Author: niu
 */
#include <stdio.h>

#include "tos_k.h"

#include "fsl_pxp.h"
#include "display_support.h"
#include "camera_support.h"







#define DISPLAY_STK_SIZE       2048
k_task_t display_task;
uint8_t display_task_stk[DISPLAY_STK_SIZE];

void display_entry(void *arg)
{

}
