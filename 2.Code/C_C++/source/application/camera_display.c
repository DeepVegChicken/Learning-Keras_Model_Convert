/*
 * camera_display.c
 *
 *  Created on: 2022年2月11日
 *      Author: niu
 */

#include "tos_k.h"
#include "fsl_debug_console.h"
#include "fsl_pxp.h"
#include "display_support.h"
#include "camera_support.h"
#include "camera_display.h"
#include "lcd_font.h"
#include "utils.h"
#include <stdio.h>
#include <cr_section_macros.h>

#define DEMO_PXP  PXP

#define APP_FRAME_BUFFER_COUNT 2
#define APP_CAMERA_BUFFER_COUNT 2

/* Camera's Pixel format RGB565, bytesPerPixel is 2. */
#define APP_CAMERA_BUFFER_BPP 2

/* LCD's Pixel format XRGB8888, bytesPerPixel is 4. */
#define APP_FRAME_BUFFER_BPP 4

#if (FRAME_BUFFER_ALIGN > DEMO_CAMERA_BUFFER_ALIGN)
#define DEMO_LCD_BUFFER_ALIGN FRAME_BUFFER_ALIGN
#else
#define DEMO_LCD_BUFFER_ALIGN DEMO_CAMERA_BUFFER_ALIGN
#endif

/*
 * 注意内存顺序应设置成如下
 * RAM 	DTC
 * RAM2 SDRAM
 * RAM3 NCACHE
*/
__BSS(RAM3) static uint16_t s_cameraBuffer[APP_CAMERA_BUFFER_COUNT][DEMO_CAMERA_HEIGHT][DEMO_CAMERA_WIDTH];
__BSS(RAM3) static uint32_t s_lcdBuffer[APP_FRAME_BUFFER_COUNT][DEMO_BUFFER_HEIGHT][DEMO_BUFFER_WIDTH];

/*
 * When new frame buffer sent to display, it might not be shown immediately.
 * Application could use callback to get new frame shown notification, at the
 * same time, when this flag is set, application could write to the older
 * frame buffer.
 */
static volatile bool s_newFrameShown = false;
static dc_fb_info_t fbInfo;
static volatile uint8_t s_lcdActiveFbIdx;

k_task_t camera_task;
uint8_t camera_task_stk[CAMERA_STK_SIZE];

k_event_t event;
k_event_t event_2;
const k_event_flag_t event_camera   	= (k_event_flag_t)(1 << 0);
const k_event_flag_t event_detect   	= (k_event_flag_t)(1 << 1);
const k_event_flag_t event_detect_done  = (k_event_flag_t)(1 << 2);
const k_event_flag_t event_diaplay  	= (k_event_flag_t)(1 << 3);
const k_event_flag_t event_mqtt  		= (k_event_flag_t)(1 << 4);

void camera_receiver_callback(camera_receiver_handle_t *handle, status_t status, void *userData)
{
	if(tos_knl_is_running()) {
		tos_knl_irq_enter();
		tos_event_post_keep(&event, event_camera);
		tos_knl_irq_leave();
	}
}

void APP_BufferSwitchOffCallback(void *param, void *switchOffBuffer)
{
	if(tos_knl_is_running()) {
		tos_knl_irq_enter();
		tos_event_post_keep(&event, event_diaplay);
		tos_knl_irq_leave();
	}

    s_lcdActiveFbIdx ^= 1;
}

void limit(int32_t* p, int32_t min, int32_t max)
{
	*p = *p > min ? *p : min;
	*p = *p < max ? *p : max;
}

void display_draw_rect(int32_t x_1, int32_t y_1, int32_t x_2, int32_t y_2, void* frame, uint32_t color)
{
	uint32_t* pos = 0;
	const uint32_t width = 3;
	const uint32_t pitchBytes = DEMO_BUFFER_WIDTH * APP_FRAME_BUFFER_BPP;

	limit(&x_1, 0, 799);
	limit(&x_2, 0, 799);
	limit(&y_1, 0, 479);
	limit(&y_2, 0, 479);

	for(uint32_t n = 0; n < width; n++) {
		pos = frame + (y_1 + n)* pitchBytes;
		for(uint32_t x = x_1; x < x_2; x++) {
			*(pos + x) = color;
		}
	}

	for(uint32_t n = 0; n < width; n++) {
		pos = frame + (y_2 - n)* pitchBytes;
		for(uint32_t x = x_1; x < x_2; x++) {
			*(pos + x) = color;
		}
	}

	for(uint32_t n = 0; n < width; n++) {
		pos = frame + (x_1 + n) * APP_FRAME_BUFFER_BPP;
		for(uint32_t y = y_1 * DEMO_BUFFER_WIDTH; y < y_2 * DEMO_BUFFER_WIDTH; y += DEMO_BUFFER_WIDTH) {
			*(pos + y) = color;
		}
	}

	for(uint32_t n = 0; n < width; n++) {
		pos = frame + (x_2 - n) * APP_FRAME_BUFFER_BPP;
		for(uint32_t y = y_1 * DEMO_BUFFER_WIDTH; y < y_2 * DEMO_BUFFER_WIDTH; y += DEMO_BUFFER_WIDTH) {
			*(pos + y) = color;
		}
	}
}

void display_show_char(uint32_t * frame, int pos_x, int pos_y, char c, uint32_t color)
{
	frame += pos_y * 800 + pos_x;
	c -= ' ';
	const unsigned char* ptr = &ascii_1608[c * 16];
	for(int i = 0; i < 16; i++) {
		c = ptr[i];
		for(int n = 0; n < 8; n++) {
			if(c & (0x01 << n))
				*(frame + i * 800 + n) = color;
		}
	}
}

void display_show_str(uint32_t * frame, int pos_x, int pos_y, char* p, uint32_t color)
{
    while(*p!='\0')
    {
    	display_show_char(frame, pos_x, pos_y, *p, color);
        pos_x += 8;
        p++;
    }
}

void display_print(uint32_t * frame, int pos_x, int pos_y, uint32_t color, const char *fmt,...)
{
		static char buffer[128]={0};
		va_list ap;

		va_start(ap,fmt);
		vsprintf((char*)buffer, fmt, ap);
		va_end(ap);

		display_show_str(frame, pos_x, pos_y, buffer, color);
}

void *lcdFrameAddr;

void camera_entry(void *arg)
{
    k_err_t err;
    k_event_flag_t flag_match;
    uint32_t cameraReceivedFrameAddr;

    APP_InitPxP();
    APP_InitCamera();
    APP_InitDisplay();

    printf("camera & display init done\r\n");

    pxp_ps_buffer_config_t psBufferConfig = {
        .pixelFormat = kPXP_PsPixelFormatRGB565, /* Note: This is 16-bit per pixel */
        .swapByte    = false,
        .bufferAddrU = 0U,
        .bufferAddrV = 0U,
        .pitchBytes  = DEMO_CAMERA_WIDTH * APP_CAMERA_BUFFER_BPP,
    };

    /* Output config. */
    pxp_output_buffer_config_t outputBufferConfig = {
        .pixelFormat    = kPXP_OutputPixelFormatARGB8888,
        .interlacedMode = kPXP_OutputProgressive,
        .buffer1Addr    = 0U,
        .pitchBytes     = DEMO_BUFFER_WIDTH * APP_FRAME_BUFFER_BPP,
        .width  = DEMO_BUFFER_WIDTH,
        .height = DEMO_BUFFER_HEIGHT,
    };

    tos_event_create(&event, (k_event_flag_t)0u);
    tos_event_post_keep(&event, event_diaplay);

    CAMERA_RECEIVER_Start(&cameraReceiver);

    while(1)
    {
    	err = tos_event_pend(&event, event_camera, &flag_match, TOS_TIME_FOREVER, TOS_OPT_EVENT_PEND_ALL | TOS_OPT_EVENT_PEND_CLR);

		if (err == K_ERR_NONE) {
			CAMERA_RECEIVER_GetFullBuffer(&cameraReceiver, &cameraReceivedFrameAddr);

			/* Convert the camera input picture to RGB format. */
			psBufferConfig.bufferAddr = cameraReceivedFrameAddr;
			PXP_SetProcessSurfaceBufferConfig(DEMO_PXP, &psBufferConfig);

			lcdFrameAddr                   = s_lcdBuffer[s_lcdActiveFbIdx ^ 1];
			outputBufferConfig.buffer0Addr = (uint32_t)lcdFrameAddr;
			PXP_SetOutputBufferConfig(DEMO_PXP, &outputBufferConfig);

			PXP_Start(DEMO_PXP);

			/* Wait for PXP process complete. */
			while (!(kPXP_CompleteFlag & PXP_GetStatusFlags(DEMO_PXP)))
			{
			}
			PXP_ClearStatusFlags(DEMO_PXP, kPXP_CompleteFlag);

			tos_event_post_keep(&event, event_detect);
			tos_event_pend(&event, event_detect_done, &flag_match, TOS_TIME_FOREVER, TOS_OPT_EVENT_PEND_ALL | TOS_OPT_EVENT_PEND_CLR);

			/* Return the camera buffer to camera receiver handle. */
			CAMERA_RECEIVER_SubmitEmptyBuffer(&cameraReceiver, (uint32_t)cameraReceivedFrameAddr);

			g_dc.ops->setFrameBuffer(&g_dc, 0, lcdFrameAddr);
		}
    }
}

void APP_InitPxP(void)
{
    /*
     * Configure the PXP for rotate and scale.
     */
    PXP_Init(DEMO_PXP);

    PXP_SetProcessSurfaceBackGroundColor(DEMO_PXP, 0U);

    PXP_SetProcessSurfacePosition(DEMO_PXP, 0U, 0U, DEMO_CAMERA_WIDTH - 1U, DEMO_CAMERA_HEIGHT - 1U);

    /* Disable AS. */
    PXP_SetAlphaSurfacePosition(DEMO_PXP, 0xFFFFU, 0xFFFFU, 0U, 0U);

    PXP_EnableCsc1(DEMO_PXP, false);
}

void APP_InitCamera(void)
{
    const camera_config_t cameraConfig = {
        .pixelFormat   = kVIDEO_PixelFormatRGB565,
        .bytesPerPixel = APP_CAMERA_BUFFER_BPP,
        .resolution    = FSL_VIDEO_RESOLUTION(DEMO_CAMERA_WIDTH, DEMO_CAMERA_HEIGHT),
        /* Set the camera buffer stride according to panel, so that if
         * camera resoution is smaller than display, it can still be shown
         * correct in the screen.
         */
        .frameBufferLinePitch_Bytes = DEMO_CAMERA_WIDTH * APP_CAMERA_BUFFER_BPP,
        .interface                  = kCAMERA_InterfaceGatedClock,
        .controlFlags               = DEMO_CAMERA_CONTROL_FLAGS,
        .framePerSec                = 30,
    };


    BOARD_InitCameraResource();

    CAMERA_RECEIVER_Init(&cameraReceiver, &cameraConfig, camera_receiver_callback, NULL);

    if (kStatus_Success != CAMERA_DEVICE_Init(&cameraDevice, &cameraConfig))
    {
        PRINTF("Camera device initialization failed\r\n");
        while (1)
        {
            ;
        }
    }

    CAMERA_DEVICE_Start(&cameraDevice);

    /* Submit the empty frame buffers to buffer queue. */
    for (uint32_t i = 0; i < APP_CAMERA_BUFFER_COUNT; i++)
    {
        CAMERA_RECEIVER_SubmitEmptyBuffer(&cameraReceiver, (uint32_t)(s_cameraBuffer[i]));
    }
}

void APP_InitDisplay(void)
{
    status_t status;

    BOARD_PrepareDisplayController();

    status = g_dc.ops->init(&g_dc);
    if (kStatus_Success != status)
    {
        PRINTF("Display initialization failed\r\n");
        assert(0);
    }

    g_dc.ops->getLayerDefaultConfig(&g_dc, 0, &fbInfo);
    fbInfo.pixelFormat = kVIDEO_PixelFormatXRGB8888;
    fbInfo.width       = DEMO_BUFFER_WIDTH;
    fbInfo.height      = DEMO_BUFFER_HEIGHT;
    fbInfo.startX      = DEMO_BUFFER_START_X;
    fbInfo.startY      = DEMO_BUFFER_START_Y;
    fbInfo.strideBytes = DEMO_BUFFER_WIDTH * APP_FRAME_BUFFER_BPP;
    g_dc.ops->setLayerConfig(&g_dc, 0, &fbInfo);

    g_dc.ops->setCallback(&g_dc, 0, APP_BufferSwitchOffCallback, NULL);

    s_lcdActiveFbIdx = 0;
    s_newFrameShown  = false;
    g_dc.ops->setFrameBuffer(&g_dc, 0, s_lcdBuffer[s_lcdActiveFbIdx]);

    /* For the DBI interface display, application must wait for the first
     * frame buffer sent to the panel.
     */
    if ((g_dc.ops->getProperty(&g_dc) & kDC_FB_ReserveFrameBuffer) == 0)
    {
        while (s_newFrameShown == false)
        {
        }
    }

    s_newFrameShown = true;

    g_dc.ops->enableLayer(&g_dc, 0);
}
