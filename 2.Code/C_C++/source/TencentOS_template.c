/*
 * Copyright 2016-2022 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of NXP Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file    TencentOS_template.cpp
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MIMXRT1062.h"
#include "fsl_debug_console.h"
/* TODO: insert other include files here. */
#include "tos_k.h"
#include "tos_at.h"

#include "fsl_lpuart.h"
#include "camera_support.h"
#include "camera_display.h"
#include "app_tflm.h"
/* TODO: insert other definitions and declarations here. */

/*---------------------- tos --------------------------*/
void SysTick_Handler(void)
{
	if (tos_knl_is_running())
	{
		tos_knl_irq_enter();
		tos_tick_handler();
		tos_knl_irq_leave();
	}
}

/* AT框架使用 */
void LPUART2_IRQHandler(void)
{
	uint8_t data;

	if((kLPUART_RxDataRegFullFlag)&LPUART_GetStatusFlags(LPUART2))
	{
		data = LPUART_ReadByte(LPUART2);
		if(tos_knl_is_running()) {
			tos_knl_irq_enter();
			tos_at_uart_input_byte(data);
			tos_knl_irq_leave();
		}
	}
}

#define APPLICATION_TASK_STK_SIZE       2*2048
k_task_t application_task;
uint8_t application_task_stk[APPLICATION_TASK_STK_SIZE];

void application_entry(void *arg);

int drv_test(void);
/*
 * @brief   Application entry point.
 */
int main(void) {

    /* Init board hardware. */
    BOARD_ConfigMPU();
    BOARD_InitBootPins();
    BOARD_InitDEBUG_UARTPins();
    BOARD_InitSDRAMPins();
    BOARD_EarlyPrepareCamera();
    BOARD_InitCSIPins();
    BOARD_InitLCDPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitSDPins();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();
#endif

    PRINTF("\r\n\r\n--------------------------------------\r\n");
    drv_test();

    PRINTF("\r\n Welcome to TencentOS Tiny!\r\n");

    tos_knl_init(); // TencentOS Tiny kernel initialize
	tos_task_create(&camera_task, "camera_task", camera_entry, NULL, 3, camera_task_stk, CAMERA_STK_SIZE, 0);
	tos_task_create(&detect_task, "detect_task", detect_entry, NULL, 5, detect_task_stk, DETECT_STK_SIZE, 0);
    tos_task_create(&application_task, "application_task", application_entry, NULL, 1, application_task_stk, APPLICATION_TASK_STK_SIZE, 0); // Create task2
    tos_knl_start();

    /* Force the counter to be placed into memory. */
    volatile static int i = 0 ;
    /* Enter an infinite loop, just incrementing a counter. */
    while(1) {
        i++ ;
        /* 'Dummy' NOP to allow source level single stepping of
            tight while() loop */
        __asm volatile ("nop");
    }
    return 0 ;
}
