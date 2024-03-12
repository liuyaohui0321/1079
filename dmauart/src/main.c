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
u32 DestinationBuffer[MAX_DATA_BUFFER_SIZE * WORD_SIZE];
u32 DestinationBuffer_1[MAX_DATA_BUFFER_SIZE * WORD_SIZE];

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
uint64_t  slba=0;
uint64_t  slba_r=0;
uint32_t  recv_cmd_ack;
//int rxflag=0;
uint8_t  cancel=0;
int main()
{
		int i;
		int ret;
		uint8_t sts;
		uint8_t inputcmd=0;
		init_platform();
		xil_printf("UCAS Prj1079 \r\n");
		DiskInit();
		SimpleDmaInit();        // 1x DMA0
		MsgQueryInit();         // Queue初始化
		InitTimeList();
		XLLFIFO_SysInit();		// Sfifo初始化
		uint8_t cmd;
		uint32_t  cmd_write_cnt=0;
		uint32_t rw_count=0,r_count=0;
		uint32_t  Checknum=0;
//		XAxiDma1_tx(XPAR_AXIDMA_1_DEVICE_ID);
//		for(int i=0;i<4096;i++)
//		{
//			wbuff[i]=i;
//		}

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
#if  0
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

		xil_printf("[1] I/O Write Test.\r\n");           //LYH  2023.8.21
		xil_printf("[2] I/O Read  Test.\r\n");
		xil_printf("Command: \0");
		mygets(input_string);
		cmd = strtoul(input_string, NULL, 0);
		xil_printf("Waiting FPGA Vio Ctrl Read Write Start\r\n");
//		slba=0x100000;
//		slba_r=0x100000;
#if  1 //iowrite with fpga
		while ((0x2==cmd)||(0x1==cmd))
		{
			if (0x1==cmd)
			{
#if  0	//2024.1.18改
				if (RxReceive(DestinationBuffer,&cmd_len) == XST_SUCCESS)
				{
					packnum =DestinationBuffer[0];
					buff =DestinationBuffer[1];
					len  =DestinationBuffer[2];
					buff1=(buff<0x90000000?MEM_DDR3_BASE:0x90000000);
					if (io_write2(NHC_NUM,0x1,(buff-buff1)*2+MEM_DDR3_BASE, slba, len, 0x0) != 0x02)
					{
						 xil_printf("I/O Write Failed!\r\n");
						 return 0;
					}
					slba += len;
					cmd_write_cnt += 1;
				}
#endif
				if (RxReceive(DestinationBuffer,&cmd_len) == XST_SUCCESS)
				{
					buff =DestinationBuffer[0];	//2024.1.18改
					len  =DestinationBuffer[1];
					if (io_write2(NHC_NUM,0x1,buff, slba, len, 0x0) != 0x02)
					{
						 xil_printf("I/O Write Failed!\r\n");
						 return 0;
					}
					slba += len;
					cmd_write_cnt += 1;
//					xil_printf("buff:0x%lx slba:0x%llx %ld\r\n",buff,slba,cmd_write_cnt);
				}
				else
				{
					for(i=0;i<NHC_NUM;i++)
					{
						do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
					}
				}
			}
			if (0x2==cmd)
			{
#if 0
				if(io_read2(NHC_NUM, 0x1, buff_r, slba_r, OFFSET_SIZE, 0x0) != 0x02)
				{
					xil_printf("I/O Read Failed!\r\n");
				}
				sleep(1);
				uint32_t last=buff_r+0x2000000;
				uint32_t check_block_size=0x100000;
				if(CheckOut_HeadTail((uint32_t *)buff_r,(uint32_t *)last,check_block_size)!=0)
				{
					xil_printf("check error!\r\n");
//					return 1;
				}

				Checknum++;
				xil_printf("                                                                          Checknum:%d\r\n",Checknum);
				slba_r += OFFSET_SIZE;
				if (buff_r <(DDR3_END_ADDR-OFFSET_SIZE))
					buff_r += OFFSET_SIZE;
				else
					buff_r = MEM_DDR3_BASE+(512*3*1024*1024);
				r_count++;
#endif
#if 1
				if(io_read2(NHC_NUM, 0x1, buff_r, slba_r, OFFSET_SIZE, 0x0) != 0x02)
				{
					xil_printf("I/O Read Failed!\r\n");
				}
				sleep(1);
				uint32_t last=(void *)(0xE1000000);
				uint32_t check_block_size=0x100000;

//				if(CheckOut_HeadTail((uint32_t *)buff_r,(uint32_t *)last,check_block_size)!=0)
//				{
//					xil_printf("check error!\r\n");
////					return 1;
//				}
				Checknum++;
				xil_printf("                                                                          Checknum:%d\r\n",Checknum);

				slba_r += OFFSET_SIZE;
				recv_cmd_ack = recv_cmd_ack + 1;
				r_count++;
				DestinationBuffer_1[0]=buff_r;
				DestinationBuffer_1[1]=OFFSET_SIZE/128/1024;
				XLLFIFO_SysInit();
				ret = TxSend_1(DestinationBuffer_1,8);
				if (ret != XST_SUCCESS)
				{
					 xil_printf("TxSend Failed! ret=%d\r\n", ret);
					 return ret;
				}
#endif
			}
			for(i=0;i<NHC_NUM;i++)
			{
				while (nhc_queue_ept(i) == 0)
				{
					do {
						sts = nhc_cmd_sts(i);
					}while(sts == 0x01);
				}
			}

			//写盘32M
//			if(r_count>7||cmd_write_cnt>15)    // 写256M
//			if(r_count>15||cmd_write_cnt>31)   // 写512M
			if(r_count>63||cmd_write_cnt>127)  // 写2G
//			if(r_count>79||cmd_write_cnt>159)  // 写2.56G
//			if(r_count>95||cmd_write_cnt>191)  // 写3G
//			if(r_count>111||cmd_write_cnt>223) // 写3.58G
//			if(r_count>127||cmd_write_cnt>255) // 写4G
//			if(r_count>159||cmd_write_cnt>319) // 写5G
//			if(r_count>191||cmd_write_cnt>383) // 写6G

			//写盘256M
//			if(r_count>0||cmd_write_cnt>0)  // 写1次
//			if(r_count>63||cmd_write_cnt>15)  // 写2G
//			if(r_count>95||cmd_write_cnt>23)  // 写3G
			{
				xil_printf("I/O Read or Write Test Finish!\r\n");
				xil_printf("w_count = %u   r_count=%u\r\n",cmd_write_cnt,r_count);
				for(i=0;i<NHC_NUM;i++)
				{
					while (nhc_queue_ept(i) == 0)
					{
						do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
					}
				}
				cmd_write_cnt=0;
				cmd=2;       // 2023.8.21
			}
		}// while
//			f_close(&file);
		    cleanup_platform();
			return 0;
}
#endif


#if  0
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
