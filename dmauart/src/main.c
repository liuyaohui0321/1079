#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"

#include "simple_dma.h"
#include "cmd.h"
#include <time.h>
#include "nhc_amba.h"
#include "mem_test.h"
#include "uart_get.h"
#include "xllfifo_drv.h"
#include "xstatus.h"

#include "fat/ff.h"		/* Declarations of FatFs API */
#include "fat/diskio.h"		/* Declarations of device I/O functions */
#include "fat/ffconf.h"
#include <wchar.h>
#include "7x_dma.h"
#include "xil_io.h"
#include "xil_cache.h"
//#include "time.h"
#include "check.h"

CreateTimeList	CreateTimelist;
ChangeTimeList	ChangeTimelist;
AccessTimeList	AccessTimelist;
/********************文件系统格式化与挂载有关参数*******************/
static BYTE Buff[4096];  //与格式化空间大小有关
FATFS fs;
//FIL fnew,fnew1;
FRESULT fr;
FILINFO fno;
FIL file;

/************************************************************/
//u32 DestinationBuffer[MAX_DATA_BUFFER_SIZE * WORD_SIZE];
//u32 DestinationBuffer_1[MAX_DATA_BUFFER_SIZE * WORD_SIZE];

int  Status;
char input_string[10];
uint fnum;
BYTE WriteBuffer[4096];
uint32_t wbuff[4096];
uint32_t  cmd_len;
uint32_t  len;
uint32_t  buff = (void *)(MEM_DDR3_BASE);
uint32_t  buff_r=(void *)(MEM_DDR3_BASE+(3*512*1024*1024));
uint32_t  buff1= (void *)(MEM_DDR3_BASE);
uint32_t  packnum;
//int rxflag=0;
uint8_t  cancel=0;
int main()
{
		int ret;
		uint8_t inputcmd=0;
		init_platform();
		xil_printf("UCAS Prj1079 \r\n");
		DiskInit();
		SimpleDmaInit();        // 1x DMA0
		MsgQueryInit();         // Queue初始化
		InitTimeList();
		XLLFIFO_SysInit();		// Sfifo初始化
//		XAxiDma1_tx(XPAR_AXIDMA_1_DEVICE_ID);
		for(int i=0;i<4096;i++)
		{
			wbuff[i]=i;
		}

#if   0//format the filesysterm
		ret = f_mkfs(
			"",	/* Logical drive number */
			0,			/* Format option  FM_EXFAT*/
			Buff,			/* Pointer to working buffer (null: use heap memory) */
			sizeof Buff			/* Size of working buffer [byte] */
			);
		if (ret != FR_OK) {
			xil_printf("f_mkfs  Failed! ret=%d\n", ret);
			return 0;
		}
#endif //format the filesysterm

/********* mount filesysterm *********/
#if  1
mount:	ret = f_mount (&fs, "", 1);
		if (ret != FR_OK) {
			xil_printf("f_mount  Failed! ret=%d\n", ret);
			if(ret==FR_NO_FILESYSTEM)
			{
				//是否重新格式化代码
				xil_printf("if reformat the filesysterm? '1:yes/2:no' \r\n");
				xil_printf("Command: \0");
				mygets(input_string);
				inputcmd=strtoul(input_string, NULL,0);
				if(0x1==inputcmd)
				{
					goto Re_MKFS;
				}
				return 0;
			}
//			return 0;
		}
		xil_printf(" Init All ok!\r\n");
#endif
//		cmd_reply_a203(0,0xA2,0x1,0x11);
//		cmd_reply_a208("0:");
//		cmd_reply_a206(0,"0:/abc");
//		cmd_reply_a207(0,"0:/bcd");

#if  1
   #if  1 //recv
		while(1)
		{
			memset(&CurMsg,0,sizeof(CurMsg));
			GetMessage(&CurMsg);
			switch(CurMsg.HandType)
			{
    				case 0XA2:
    					xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
    					switch(CurMsg.HandId)
    					{
    						case 0x1:
    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							xil_printf("------Start executing commands!------\r\n");
    							run_cmd_a201(&CurMsg);
    							xil_printf("------commands executing complete!------\r\n");
    						break;
    						case 0x2:
    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							xil_printf("------Start executing commands!------\r\n");
    							run_cmd_a202(&CurMsg);
    							xil_printf("------commands executing complete!------\r\n");
    						break;
    						case 0x4:
    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							xil_printf("------Start executing commands!------\r\n");
    							run_cmd_a204(&CurMsg);
    							xil_printf("------commands executing complete!------\r\n");
    						break;
    						case 0x5:
    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							xil_printf("------Start executing commands!------\r\n");
    							run_cmd_a205(&CurMsg);
    							xil_printf("------commands executing complete!------\r\n");
    						break;
//    						case 0x8:
//								xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
//								xil_printf("------Start executing commands!------\r\n");
//								run_cmd_a208(&CurMsg);
//								xil_printf("------commands executing complete!------\r\n");
//							break;
    						default:
    						break;
    					}
    					break;
    				case 0XB2:
    					xil_printf("%s %d  CurMsg.HandType:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType);
    					xil_printf("------Start executing commands!------\r\n");
    					run_cmd_b201(&CurMsg);
    					xil_printf("------commands executing complete!------\r\n");
    					break;
    				case 0XD2:
    					xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
    					switch( CurMsg.HandId)
    					{
    						case 0x1:
    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
    							xil_printf("------Start executing commands!------\r\n");
    							run_cmd_d201(&CurMsg);
    							xil_printf("------commands executing complete!------\r\n");
    						break;
//    						case 0x2:
//    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
//    							run_cmd_d202(&CurMsg);
//    						break;
//    						case 0x3:
//    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
//    							run_cmd_d203(&CurMsg);
//    						break;
    						case 0x4:
    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							run_cmd_d204(&CurMsg);
    						break;
    						case 0x5:
    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							run_cmd_d205(&CurMsg);
    						break;
//    						case 0x6:
//    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
//    							run_cmd_d206(&CurMsg);
//    						break;
    						case 0x7:
    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							run_reply_d207(&CurMsg);
    						break;
    						case 0x8:
//    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							run_reply_d208(&CurMsg);
    						break;
    						case 0xA:
    							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
    							run_cmd_d20A(&CurMsg);
    						break;
    						default:
    						break;
    					}
    					break;

					case 0XF2:
						xil_printf("%s %d  CurMsg.HandType:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType);
						xil_printf("------Start executing commands!------\r\n");
						run_cmd_f201(&CurMsg);
						xil_printf("------commands executing complete!------\r\n");
					break;

    				default:
    					break;
			}
    }
#endif  //recv
    cleanup_platform();
    return 0;
Re_MKFS:
	ret = f_mkfs(
		"",	/* Logical drive number */
		0,			/* Format option  FM_EXFAT*/
		Buff,			/* Pointer to working buffer (null: use heap memory) */
		sizeof Buff			/* Size of working buffer [byte] */
		);
	if (ret != FR_OK) {
		xil_printf("f_mkfs  Failed! ret=%d\n", ret);
		return 0;
		}
	goto mount;
}
#endif
