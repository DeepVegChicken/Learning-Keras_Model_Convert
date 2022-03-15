/*
 * dev_test.c
 *
 *  Created on: 2022年2月10日
 *      Author: niu
 */

#include <stdio.h>
#include "fsl_debug_console.h"

int ram_test(uint32_t start, uint32_t size)
{
	printf("ram test size %ldMB...", size/1024/1024);
	for(uint32_t i = start; i < start + size; i += 4)
		*(uint32_t *)i = i;

	printf("write...");

	for(uint32_t i = start; i < start + size; i += 4) {
		if(*(uint32_t *)i != i) {
			printf("read error 0x%08lx 0x%08lx\r\n", i, *(uint32_t *)i);
			return -1;
		}
	}

	printf("read ok\r\n");
	return 0;
}

#include "ff.h"
#include "diskio.h"
#include "fsl_sd_disk.h"
#include "sdmmc_config.h"
/*================FATFS=====================*/
static FATFS g_fileSystem; /* File system object */
static FIL g_fileObject;   /* File object */

/* @brief decription about the read/write buffer
 * The size of the read/write buffer should be a multiple of 512, since SDHC/SDXC card uses 512-byte fixed
 * block length and this driver example is enabled with a SDHC/SDXC card.If you are using a SDSC card, you
 * can define the block length by yourself if the card supports partial access.
 * The address of the read/write buffer should align to the specific DMA data buffer address align value if
 * DMA transfer is used, otherwise the buffer address is not important.
 * At the same time buffer address/size should be aligned to the cache line size if cache is supported.
 */
/* buffer size (in byte) for read/write operations */
#define BUFFER_SIZE (513U)
/*! @brief align with cache line size */
#define BOARD_SDMMC_DATA_BUFFER_ALIGN_SIZE (32U)
/*! @brief Data written to the card */
SDK_ALIGN(uint8_t g_bufferWrite[BUFFER_SIZE], BOARD_SDMMC_DATA_BUFFER_ALIGN_SIZE);
/*! @brief Data read from the card */
SDK_ALIGN(uint8_t g_bufferRead[BUFFER_SIZE], BOARD_SDMMC_DATA_BUFFER_ALIGN_SIZE);


static status_t sdcardWaitCardInsert(void)
{
    BOARD_SD_Config(&g_sd, NULL, BOARD_SDMMC_SD_HOST_IRQ_PRIORITY, NULL);

    /* SD host init function */
    if (SD_HostInit(&g_sd) != kStatus_Success)
    {
        PRINTF("SD host init fail\r\n");
        return kStatus_Fail;
    }

    /* wait card insert */
    if (SD_PollingCardInsert(&g_sd, kSD_Inserted) == kStatus_Success)
    {
        //PRINTF("Card inserted.\r\n");
        /* power off card */
        SD_SetCardPower(&g_sd, false);
        /* power on the card */
        SD_SetCardPower(&g_sd, true);
    }
    else
    {
        PRINTF("Card detect fail.\r\n");
        return kStatus_Fail;
    }

    return kStatus_Success;
}

/*================FATFS END=====================*/
int fatfs_sd_test(void)
{
    FRESULT error;
    DIR directory; /* Directory object */
    FILINFO fileInformation;
    UINT bytesWritten;
    UINT bytesRead;
    const TCHAR driverNumberBuffer[3U] = {SDDISK + '0', ':', '/'};
    volatile bool failedFlag           = false;
    char ch                            = '0';
    BYTE work[FF_MAX_SS];

	PRINTF("\r\n[FATFS SD card test]\r\n");

	if (sdcardWaitCardInsert() != kStatus_Success)
	{
		return -1;
	}

	if (f_mount(&g_fileSystem, driverNumberBuffer, 0U))
	{
		PRINTF("Mount volume failed.\r\n");
		return -1;
	}

#if (FF_FS_RPATH >= 2U)
	error = f_chdrive((char const *)&driverNumberBuffer[0U]);
	if (error)
	{
		PRINTF("Change drive failed.\r\n");
		return -1;
	}
#endif

	error = f_mkdir(_T("/dir_1"));
	if (error)
	{
		if (error == FR_EXIST)
		{
			//PRINTF("Directory exists.\r\n");
		}
		else
		{
			PRINTF("Make directory failed.\r\n");
			return -1;
		}
	}

	error = f_open(&g_fileObject, _T("/dir_1/f_1.dat"), (FA_WRITE | FA_READ | FA_CREATE_ALWAYS));
	if (error)
	{
		if (error == FR_EXIST)
		{
			//PRINTF("File exists.\r\n");
		}
		else
		{
			PRINTF("Open file failed.\r\n");
			return -1;
		}
	}

	error = f_mkdir(_T("/dir_1/dir_2"));
	if (error)
	{
		if (error == FR_EXIST)
		{
			//PRINTF("Directory exists.\r\n");
		}
		else
		{
			PRINTF("Directory creation failed.\r\n");
			return -1;
		}
	}

	if (f_opendir(&directory, "/dir_1"))
	{
		PRINTF("Open directory failed.\r\n");
		return -1;
	}

	for (;;)
	{
		error = f_readdir(&directory, &fileInformation);

		/* To the end. */
		if ((error != FR_OK) || (fileInformation.fname[0U] == 0U))
		{
			break;
		}
		if (fileInformation.fname[0] == '.')
		{
			continue;
		}
		if (fileInformation.fattrib & AM_DIR)
		{
			//PRINTF("Directory file : %s.\r\n", fileInformation.fname);
		}
		else
		{
			//PRINTF("General file : %s.\r\n", fileInformation.fname);
		}
	}

	memset(g_bufferWrite, 'a', sizeof(g_bufferWrite));
	g_bufferWrite[BUFFER_SIZE - 2U] = '\r';
	g_bufferWrite[BUFFER_SIZE - 1U] = '\n';

	while (true)
	{
		if (failedFlag || (ch == 'q'))
		{
			break;
		}

		error = f_write(&g_fileObject, g_bufferWrite, sizeof(g_bufferWrite), &bytesWritten);
		if ((error) || (bytesWritten != sizeof(g_bufferWrite)))
		{
			PRINTF("Write file failed. \r\n");
			failedFlag = true;
			continue;
		}

		/* Move the file pointer */
		if (f_lseek(&g_fileObject, 0U))
		{
			PRINTF("Set file pointer position failed. \r\n");
			failedFlag = true;
			continue;
		}

		memset(g_bufferRead, 0U, sizeof(g_bufferRead));
		error = f_read(&g_fileObject, g_bufferRead, sizeof(g_bufferRead), &bytesRead);
		if ((error) || (bytesRead != sizeof(g_bufferRead)))
		{
			PRINTF("Read file failed. \r\n");
			failedFlag = true;
			continue;
		}

		if (memcmp(g_bufferWrite, g_bufferRead, sizeof(g_bufferWrite)))
		{
			PRINTF("Compare read/write content isn't consistent.\r\n");
			failedFlag = true;
			continue;
		}

		break;
	}

	if (f_close(&g_fileObject))
	{
		PRINTF("\r\nClose file failed\r\n");
		return -1;
	}

	PRINTF("FATFS test ok\r\n");

	return 0;
}

int fatfs_write_file(char * name, void* p, uint32_t size)
{
	static uint32_t cnt = 1;
	char full_name[20];
	UINT bytesWritten;
	snprintf(full_name, sizeof(full_name), "%s-%d .bmp", name, cnt++);
	f_open(&g_fileObject, full_name, (FA_WRITE | FA_READ | FA_CREATE_ALWAYS));
	f_write(&g_fileObject, p, size, &bytesWritten);
	f_close(&g_fileObject);
}

int drv_test(void)
{
	ram_test(0x80000000, 0x1000000);
	fatfs_sd_test();

	return 0;
}
