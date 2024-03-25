
#include "xil_printf.h"
#include "xil_io.h"
#include "nhc_amba.h"
#include "mem_test.h"
#include "xllfifo_drv.h"
#include "xil_cache.h"
uint8_t queue_wptr  = 0x0;
uint8_t queue_rptr  = 0x0;
uint8_t queue2_wptr = 0x0;
uint8_t queue2_rptr = 0x0;
uint8_t queue3_wptr = 0x0;
uint8_t queue3_rptr = 0x0;
uint8_t queue4_wptr = 0x0;
uint8_t queue4_rptr = 0x0;

uint8_t queue_depth = 0x0; // 0's based value: 0~63

u32 SourceBuffer[MAX_DATA_BUFFER_SIZE * WORD_SIZE];

uint32_t  cmd_ack_cnt  = 0;

/**********************NVME控制器参数设置*************************/
uint8_t   init_done = 0x0;
uint32_t  freq_MHz = 250;
uint32_t  timeout_sec;
// *********************************************************************************
// NHC initialization
// <-0x0 for error; 0x1 for okay
// *********************************************************************************
uint8_t nhc_init(uint8_t inst, uint32_t timeout_sec, uint32_t freq_MHz)
{
	uint8_t link_up = 0x0;
	uint8_t link_width = 0x0;
	uint8_t link_speed = 0x0;

	uint8_t init_busy = 0x1;
	uint8_t nhc_sts = 0x0;

	uint32_t rdata;

	uint64_t cap = 0x0;

	uint32_t timeout_value = timeout_sec*freq_MHz*1000*1000;

	uint32_t nhc_base = (inst == 0)?NHC_BASE:
			                (inst == 1)?NHC2_BASE:
			            		(inst == 2)?NHC3_BASE:NHC4_BASE;
	// Check Link
	while(link_up == 0x0)
	{
		rdata = Xil_In32(nhc_base + NHC_DBG_LINK);
		link_up = (rdata >> 0) & 0x1; // bit0

		if(link_up > 0x0)
		{
			rdata = Xil_In32(nhc_base + NHC_DBG_LINK); // read again to avoid CDC
			link_width = (rdata >> 16) & 0xF; // bit19~16
			link_speed = (rdata >> 24) & 0xF; // bit27~24

//			link_width = (rdata >> 16) & 0xFF; // bit23~16
//			link_speed = (rdata >> 24) & 0xFF; // bit31~24
			xil_printf("Link-up: link_width=%x, link_speed=%x\n\r", link_width, link_speed);
		}
		else
		{
			xil_printf("Wait link-up>>>>>>>>>>>>>>>>>>>>>\n\r" );
		}
	}

	// NHC Initialization
	Xil_Out32(nhc_base + NHC_IP_CSR, 0x1 << 4);

	// Check Status
	while(nhc_sts != 0x2)
	{
		rdata = Xil_In32(nhc_base + NHC_IP_CSR);
		init_busy = (rdata >> 4) & 0x1; // bit4

		xil_printf("Initialization ongoing>>>>>>>>>\n\r");
		if(init_busy)
		{
			//xil_printf("Initialization ongoing>>>>>>>>>\n");
		}
		else
		{
			nhc_sts = (rdata >> 0) & 0x3; // bit1~0
			if(nhc_sts == 0x2)
			{
				xil_printf("Initialization Completes!\n\r");
			}
			else
			{
				xil_printf("Initialization Error!\n\r");
				rdata = Xil_In32(nhc_base + NHC_IP_CSR);
				xil_printf("nhc_intr_sts = %x!\n\r", rdata);
				return 0x0;
			}
		}
	}

	// Report NVMe SSD registers
	rdata = Xil_In32(nhc_base + NHC_IP_VS);
	xil_printf("nhc_ip_vs = %x\n\r", rdata);

	rdata = Xil_In32(nhc_base + NHC_NVME_VS);
	xil_printf("nhc_nvme_vs = %x\n\r", rdata);

	rdata = Xil_In32(nhc_base + NHC_NVME_CAP + 0x4);
	cap = rdata;
	rdata = Xil_In32(nhc_base + NHC_NVME_CAP);
	cap = (cap << 32) | rdata;
	xil_printf("nhc_nvme_cap = %x\n\r", cap);

	// Set Timeout Value: 4ns
	Xil_Out32(nhc_base + NHC_TIMEOUT_SET, timeout_value);

	return 0x1;
}

// *********************************************************************************
// NHC Queue Initialization
// ->depth: 0~63, 0's based value
// ->mode: 0x0 for User mode; 0x1 for Debug mode
// <-0x0 for error; 0x1 for okay
// *********************************************************************************
uint8_t nhc_queue_init(uint8_t inst, uint8_t depth, uint8_t mode)
{
	uint32_t rdata;
	uint32_t wdata;

	uint32_t nhc_base = (inst == 0)?NHC_BASE:
                        (inst == 1)?NHC2_BASE:
        		        (inst == 2)?NHC3_BASE:NHC4_BASE;

	// Clear Queue
	Xil_Out32(nhc_base + NHC_QUEUE_CFG, 0x0);

	// Check and Configuration
	if(mode > 0x1)
	{
		xil_printf("Illegal mode value!\n\r");
		return 0x0;
	}
	else
	{
		xil_printf("Legal depth/mode value!\n\r");

		// Clear Queue pointer
		queue_wptr = 0x0;
		queue_rptr = 0x0;
		queue2_wptr = 0x0;
		queue2_rptr = 0x0;

		// Set Queue Depth
		queue_depth = depth;

		// Update register: NHC_QUEUE_CFG
		// -Queue Depth: bit5~0
		// -Queue Mode: bit8
		// -Queue Enable: bit9
		wdata = ((depth << 0) & 0x3F) | ((mode << 8) & 0x1) | 0x200;
		Xil_Out32(nhc_base + NHC_QUEUE_CFG, wdata);
	}

	// Report
	rdata = Xil_In32(nhc_base + NHC_QUEUE_CFG);
	xil_printf("nhc_queue_cfg = %x\n", rdata);
	return 0x1;
}

// *********************************************************************************
// NHC Queue Next Pointer
// ->current pointer
// <-next pointer
// *********************************************************************************
uint8_t nhc_queue_next(uint8_t queue_ptr)
{
	uint8_t queue_ptr_next = queue_ptr + 0x1;

	if(queue_ptr_next > queue_depth)
	{
		return 0x0; // Wrap
	}
	else
	{
		return queue_ptr_next; // Increase
	}
}
// *********************************************************************************
// NHC Queue Full check
// <-0x0 for not full; 0x1 for full
// *********************************************************************************
uint8_t nhc_queue_full(void)
{
	if(nhc_queue_next(queue_wptr) == queue_rptr)
	{
		return 0x1; // full
	}
	else
	{
		return 0x0; // not full
	}
}
// *********************************************************************************
// NHC Queue2 Full check
// <-0x0 for not full; 0x1 for full
// *********************************************************************************
uint8_t nhc2_queue_full(void)
{
	if(nhc_queue_next(queue2_wptr) == queue2_rptr)
	{
		return 0x1; // full
	}
	else
	{
		return 0x0; // not full
	}
}

// *********************************************************************************
// NHC Queue3 Full check
// <-0x0 for not full; 0x1 for full
// *********************************************************************************
uint8_t nhc3_queue_full(void)
{
	if(nhc_queue_next(queue3_wptr) == queue3_rptr)
	{
		return 0x1; // full
	}
	else
	{
		return 0x0; // not full
	}
}

// *********************************************************************************
// NHC Queue4 Full check
// <-0x0 for not full; 0x1 for full
// *********************************************************************************
uint8_t nhc4_queue_full(void)
{
	if(nhc_queue_next(queue4_wptr) == queue4_rptr)
	{
		return 0x1; // full
	}
	else
	{
		return 0x0; // not full
	}
}

//// *********************************************************************************
//// NHC Queue Full check
//// <-0x0 for not full; 0x1 for full
//// *********************************************************************************
//uint8_t nhc_queue_full(uint8_t inst)
//{
//	if (inst == 0)
//	{
//		if(nhc_queue_next(queue_wptr) == queue_rptr)
//		{
//			return 0x1; // full
//		}
//		else
//		{
//			return 0x0; // not full
//		}
//	}else if (inst == 1)
//	{
//		if(nhc_queue_next(queue2_wptr) == queue2_rptr)
//		{
//			return 0x1; // full
//		}
//		else
//		{
//			return 0x0; // not full
//		}
//	}else if (inst == 2)
//	{
//		if(nhc_queue_next(queue3_wptr) == queue3_rptr)
//		{
//			return 0x1; // full
//		}
//		else
//		{
//			return 0x0; // not full
//		}
//	}else{
//		if(nhc_queue_next(queue4_wptr) == queue4_rptr)
//		{
//			return 0x1; // full
//		}
//		else
//		{
//			return 0x0; // not full
//		}
//	}
//}

// *********************************************************************************
// NHC Queue Full check
// <-0x0 for not ept; 0x1 for ept
// *********************************************************************************
uint8_t nhc_queue_ept(uint8_t inst)
{
	if (inst == 0)
	{
		if(queue_wptr == queue_rptr)
		{
			return 0x1; // ept
		}
		else
		{
			return 0x0; // not ept
		}
	}else if (inst == 1)
	{
		if(queue2_wptr == queue2_rptr)
		{
			return 0x1; //
		}
		else
		{
			return 0x0; // not
		}
	}else if (inst == 2)
	{
		if(queue3_wptr == queue3_rptr)
		{
			return 0x1; //
		}
		else
		{
			return 0x0; // not
		}
	}else{
		if(queue4_wptr == queue4_rptr)
		{
			return 0x1; //
		}
		else
		{
			return 0x0; // not
		}
	}
}

// *********************************************************************************
// NHC Queue2 Full check
// <-0x0 for not full; 0x1 for full
// *********************************************************************************
//uint8_t nhc2_queue_full(void)
//{
//	if(nhc_queue_next(queue2_wptr) == queue2_rptr)
//	{
//		return 0x1; // full
//	}
//	else
//	{
//		return 0x0; // not full
//	}
//}

//// *********************************************************************************
//// NHC Command Submission
//// ->cmd_cdw0~15
//// *********************************************************************************
//uint8_t nhc_cmd_sub(uint8_t inst, uint32_t cmd_cdw[16])
//{
//  uint32_t waddr;
//  uint32_t i;
//  uint32_t rd_data;
//
//  uint32_t nhc_base = (inst == 0)?NHC_BASE:
//                      (inst == 1)?NHC2_BASE:
//      		          (inst == 2)?NHC3_BASE:NHC4_BASE;
//
//  // Add command into the current write pointer
//  // CDW1~15
//  if (inst == 0)
//  {
//	  waddr = (queue_wptr << 6) + 0x4; // 16DW for each Node
//  }
//  else if (inst == 1)
//  {
//	  waddr = (queue2_wptr << 6) + 0x4; // 16DW for each Node
//  }
//  else if (inst == 2)
//  {
//	  waddr = (queue3_wptr << 6) + 0x4; // 16DW for each Node
//  }
//  else
//  {
//	  waddr = (queue4_wptr << 6) + 0x4; // 16DW for each Node
//  }
//  for(i=0; i<=14; i++)
//  {
//	  Xil_Out32(nhc_base + NHC_REGION_QCMD + waddr, cmd_cdw[i+1]);
//	  waddr = waddr + 0x4;
//  }
//
//  // CDW0: update CDW0 finally
//  if (inst == 0)
//  {
//	  waddr = (queue_wptr << 6); // 16DW for each Node
//  }
//  else if (inst == 1)
//  {
//	  waddr = (queue2_wptr << 6); // 16DW for each Node
//  }
//  else if (inst == 2)
//  {
//	  waddr = (queue3_wptr << 6); // 16DW for each Node
//  }
//  else
//  {
//	  waddr = (queue4_wptr << 6); // 16DW for each Node
//  }
//  Xil_Out32(nhc_base + NHC_REGION_QCMD + waddr, cmd_cdw[0]);
//  rd_data = Xil_In32(nhc_base + NHC_REGION_QCMD + waddr);
//  if (rd_data != cmd_cdw[0])
//  {
//	  xil_printf("write command verify error.\n");
//	  rd_data = Xil_In32(nhc_base + NHC_REGION_QCMD + waddr);
//  }
//  // Update the write pointer
//  if (inst == 0)
//  {
//	  queue_wptr = nhc_queue_next(queue_wptr);
//  }
//  else if (inst == 1)
//  {
//	  queue2_wptr = nhc_queue_next(queue2_wptr);
//  }
//  else if (inst == 2)
//  {
//	  queue3_wptr = nhc_queue_next(queue3_wptr);
//  }
//  else
//  {
//	  queue4_wptr = nhc_queue_next(queue4_wptr);
//  }
//  return 0x02;
//}
// *********************************************************************************
// NHC Command Submission
// ->cmd_cdw0~15
// *********************************************************************************
uint8_t nhc_cmd_sub(uint8_t inst, uint32_t cmd_cdw[16])
{
  uint32_t waddr;
  uint32_t i;
  uint8_t  queue_full;
  uint32_t rd_data;

  uint32_t nhc_base = (inst == 0)?NHC_BASE:
                      (inst == 1)?NHC2_BASE:
      		          (inst == 2)?NHC3_BASE:NHC4_BASE;
  // Check Queue full status
  while(1)
  {
	  if (inst == 0)
	  {
	      queue_full = nhc_queue_full();
	  }
	  else if (inst == 1)
	  {
		  queue_full = nhc2_queue_full();
	  }
	  else if (inst == 2)
	  {
		  queue_full = nhc3_queue_full();
	  }
	  else
	  {
		  queue_full = nhc4_queue_full();
	  }
	  if(queue_full == 0x1)
	  {
		  //xil_printf("Queue is full: queue_wptr=%x, queue_rptr=%x,!\n", queue_wptr, queue_rptr);
		  return 0;
	  }
	  else
	  {
		  break;
	  }
  }

  // Add command into the current write pointer
  // CDW1~15
  if (inst == 0)
  {
	  waddr = (queue_wptr << 6) + 0x4; // 16DW for each Node
  }
  else if (inst == 1)
  {
	  waddr = (queue2_wptr << 6) + 0x4; // 16DW for each Node
  }
  else if (inst == 2)
  {
	  waddr = (queue3_wptr << 6) + 0x4; // 16DW for each Node
  }
  else
  {
	  waddr = (queue4_wptr << 6) + 0x4; // 16DW for each Node
  }
  for(i=0; i<=14; i++)
  {
	  Xil_Out32(nhc_base + NHC_REGION_QCMD + waddr, cmd_cdw[i+1]);
	  waddr = waddr + 0x4;
  }

  // CDW0: update CDW0 finally
  if (inst == 0)
  {
	  waddr = (queue_wptr << 6); // 16DW for each Node
  }
  else if (inst == 1)
  {
	  waddr = (queue2_wptr << 6); // 16DW for each Node
  }
  else if (inst == 2)
  {
	  waddr = (queue3_wptr << 6); // 16DW for each Node
  }
  else
  {
	  waddr = (queue4_wptr << 6); // 16DW for each Node
  }
  Xil_Out32(nhc_base + NHC_REGION_QCMD + waddr, cmd_cdw[0]);
  rd_data = Xil_In32(nhc_base + NHC_REGION_QCMD + waddr);
  if (rd_data != cmd_cdw[0])
  {
	  xil_printf("write command verify error.\n");
	  xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
	  rd_data = Xil_In32(nhc_base + NHC_REGION_QCMD + waddr);
  }
  // Update the write pointer
  if (inst == 0)
  {
	  queue_wptr = nhc_queue_next(queue_wptr);
  }
  else if (inst == 1)
  {
	  queue2_wptr = nhc_queue_next(queue2_wptr);
  }
  else if (inst == 2)
  {
	  queue3_wptr = nhc_queue_next(queue3_wptr);
  }
  else
  {
	  queue4_wptr = nhc_queue_next(queue4_wptr);
  }
  return 1;
}
// *********************************************************************************
// NHC Command Status
// <-0x0: not submitted; 0x1: Ongoing; 0x2: Successful; 0x3: Error
// *********************************************************************************
uint8_t nhc_cmd_sts(uint8_t inst)
{
	uint32_t rdata;
	uint32_t raddr;
	uint8_t *p_queue_rptr;

	uint8_t sts;
	uint32_t nhc_base = (inst == 0)?NHC_BASE:
                        (inst == 1)?NHC2_BASE:
        		        (inst == 2)?NHC3_BASE:NHC4_BASE;

	p_queue_rptr = (inst == 0)?(&queue_rptr):
			       (inst == 1)?(&queue2_rptr):
			       (inst == 2)?(&queue3_rptr):(&queue4_rptr);
	while(1)
	{

		raddr = (*p_queue_rptr << 6);
		rdata = Xil_In32(nhc_base + NHC_REGION_QCMD + raddr);
		sts = ((rdata >> 16) & 0x3); // bit17~16

		if(sts == 0x3) // Error
		{
			*p_queue_rptr = nhc_queue_next(*p_queue_rptr);
			// Must:
			// ---->SW Reset/PCIe Host Reset/Ignore, depends on Error Type
			//
			break;
		}
		else if(sts == 0x2) // Completes
		{
			// Clear current Node
			if(*p_queue_rptr < 32) // Node is in Lower DW
			{
				Xil_Out32(nhc_base + NHC_CMD_STS, (0x1 << *p_queue_rptr));
			}
			else // Node is in Higher DW
			{
			    Xil_Out32(nhc_base + NHC_CMD_STS + 4, (0x1 << (*p_queue_rptr - 32)));
			}

			break;
		}
		else if(sts == 0x1) // Ongoing
		{
			// Do nothing
			return sts;
		}
		else
		{
			return sts;
		}
	}

	// Update read pointer
	*p_queue_rptr = nhc_queue_next(*p_queue_rptr);
	return sts;
}

// *********************************************************************************
// Fast Startup
// <-0x0: not submitted; 0x1: Ongoing; 0x2: Successful; 0x3: Error
// *********************************************************************************
uint8_t adm_startup(uint8_t inst)
{
	uint32_t cmd_cdw[16];
	uint8_t  sts;

	// Command Submission
	cmd_cdw[0]  = 0x000100C0;
	cmd_cdw[1]  = 0x0;
	cmd_cdw[2]  = 0x0;
	cmd_cdw[3]  = 0x0;
	cmd_cdw[4]  = 0x0;
	cmd_cdw[5]  = 0x0;
	cmd_cdw[6]  = 0x0;
	cmd_cdw[7]  = 0x0;
	cmd_cdw[8]  = 0x0;
	cmd_cdw[9]  = 0x0;
	cmd_cdw[10] = 0x0;
	cmd_cdw[11] = 0x0;
	cmd_cdw[12] = 0x0;
	cmd_cdw[13] = 0x0;
	cmd_cdw[14] = 0x0;
	cmd_cdw[15] = 0x0;

	nhc_cmd_sub(inst,cmd_cdw);

	// Command Status
	do {
		sts = nhc_cmd_sts(inst);
	}while(sts == 0x01);

	return sts;
}

// *********************************************************************************
// Shutdown
// <-0x0: not submitted; 0x1: Ongoing; 0x2: Successful; 0x3: Error
// *********************************************************************************
uint8_t adm_shutdown(uint8_t inst)
{
	uint32_t cmd_cdw[16];
	uint8_t  sts;

	// Command Submission
	cmd_cdw[0]  = 0x000100C1;
	cmd_cdw[1]  = 0x0;
	cmd_cdw[2]  = 0x0;
	cmd_cdw[3]  = 0x0;
	cmd_cdw[4]  = 0x0;
	cmd_cdw[5]  = 0x0;
	cmd_cdw[6]  = 0x0;
	cmd_cdw[7]  = 0x0;
	cmd_cdw[8]  = 0x0;
	cmd_cdw[9]  = 0x0;
	cmd_cdw[10] = 0x0;
	cmd_cdw[11] = 0x0;
	cmd_cdw[12] = 0x0;
	cmd_cdw[13] = 0x0;
	cmd_cdw[14] = 0x0;
	cmd_cdw[15] = 0x0;

	nhc_cmd_sub(inst,cmd_cdw);

	// Command Status
	sts = nhc_cmd_sts(inst);
	return sts;
}

// *********************************************************************************
// I/O Flush
// <-0x0: not submitted; 0x1: Ongoing; 0x2: Successful; 0x3: Error
// ->nsid: Namespace ID, 1h for first NS
// *********************************************************************************
uint8_t io_flush(uint8_t nhc_num,uint32_t nsid)
{
	uint32_t cmd_cdw[16];
	uint8_t  sts;
	uint8_t  i;

	// Command Submission
	cmd_cdw[0]  = 0x80010080;
	cmd_cdw[1]  = nsid;
	cmd_cdw[2]  = 0x0;
	cmd_cdw[3]  = 0x0;
	cmd_cdw[4]  = 0x0;
	cmd_cdw[5]  = 0x0;
	cmd_cdw[6]  = 0x0;
	cmd_cdw[7]  = 0x0;
	cmd_cdw[8]  = 0x0;
	cmd_cdw[9]  = 0x0;
	cmd_cdw[10] = 0x0;
	cmd_cdw[11] = 0x0;
	cmd_cdw[12] = 0x0;
	cmd_cdw[13] = 0x0;
	cmd_cdw[14] = 0x0;
	cmd_cdw[15] = 0x0;

	for(i=0;i<nhc_num;i++)
	{
		nhc_cmd_sub(i,cmd_cdw);
	}

	// Command Status
	for(i=0;i<nhc_num;i++)
	{
		sts = nhc_cmd_sts(i);
	}
	return sts;
}

// *********************************************************************************
// I/O Write
// <-0x0: not submitted; 0x1: Ongoing; 0x2: Successful; 0x3: Error
// *********************************************************************************
uint8_t io_write(uint8_t nhc_num, uint32_t nsid, uint32_t addr, uint64_t slba, uint32_t len, uint32_t dsm)
{

	uint32_t cmd_cdw[16];
	uint8_t  sts;
	uint8_t  i,j;
	static uint32_t full_wr_cnt = 0;
	slba = 2*slba/512/nhc_num;
//	xil_printf("io_write addr=0x%x slba=%u  ", addr, slba);
//	xil_printf("len=%lu\n", len);
	for(i=0;i<nhc_num;i++)
	{
		// Command Submission
		cmd_cdw[0]  = 0x80350081;
		cmd_cdw[1]  = nsid;
		cmd_cdw[2]  = 0x0;
		cmd_cdw[3]  = 0x0;
		cmd_cdw[4]  = 0x0;
		cmd_cdw[5]  = 0x0;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*U_BLK_SIZE/nhc_num;   // 9.27改
		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len*2/nhc_num;
//		cmd_cdw[6]  = addr + i*len/nhc_num;
		cmd_cdw[7]  = 0x0; // Non-zero if use 64bit memory address
		cmd_cdw[8]  = 0x0;
		cmd_cdw[9]  = 0x0;
		cmd_cdw[10] = (uint32_t) (slba >> 0);
		cmd_cdw[11] = (uint32_t) (slba >> 32);
		cmd_cdw[12] = len/512/nhc_num*2;
//		cmd_cdw[12] = len/512/nhc_num*2*2;
//		cmd_cdw[12] = len/512/nhc_num*2;
	//			cmd_cdw[12] = 64;
		cmd_cdw[13] = dsm & 0xFF;
		cmd_cdw[14] = 0x0;
		cmd_cdw[15] = 0x0;

		sts = nhc_cmd_sub(i,cmd_cdw);////Previously, sub was behind while

//		while (nhc_queue_full(i) == 0x1)
//		{
			//wait command ack.
			for(j=0;j<nhc_num;j++)
			{
				do {
					sts = nhc_cmd_sts(j);
				}while(sts == 0x01);
			}
			//send ack.
			//send_axis_ack(0x01,0x0,sts);
//		}

	}
	if (addr == DDR3_START_ADDR)
	{
	 	full_wr_cnt = full_wr_cnt + 1;
//		xil_printf("Have write full ddr area.full write cnt is %d.\n",full_wr_cnt);
	}
// Command Status
	while (queue_rptr != queue_wptr)
	{
		sts = nhc_cmd_sts(0);
		if (sts == 3)
			return sts;
	}
	while (queue2_rptr != queue2_wptr)
	{
		sts = nhc_cmd_sts(1);
		if (sts == 3)
			return sts;
	}
	while (queue3_rptr != queue3_wptr)
	{
		sts = nhc_cmd_sts(2);
		if (sts == 3)
			return sts;
	}
	while (queue4_rptr != queue4_wptr)
	{
		sts = nhc_cmd_sts(3);
		if (sts == 3)
			return sts;
	}
	return 0x02;
}

uint8_t io_write1(uint8_t nhc_num, uint32_t nsid, uint32_t addr, uint64_t slba, uint32_t len,uint32_t nlba, uint32_t dsm)
{

	uint32_t cmd_cdw[16];
	uint8_t  sts;
	uint8_t  i,j;
	static uint32_t full_wr_cnt = 0;
	slba = slba/512/nhc_num;
	i=0;
 while(i<nlba)
 {
	for(j=0;j<nhc_num;j++)
	{
		// Command Submission
		cmd_cdw[0]  = 0x80350081;
		cmd_cdw[1]  = nsid;
		cmd_cdw[2]  = 0x0;
		cmd_cdw[3]  = 0x0;
		cmd_cdw[4]  = 0x0;
		cmd_cdw[5]  = 0x0;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*U_BLK_SIZE/nhc_num;   // 9.27改
		cmd_cdw[6]  = addr + (j%(nhc_num/DDR_NUM))*len/nhc_num;
		cmd_cdw[7]  = 0x0; // Non-zero if use 64bit memory address
		cmd_cdw[8]  = 0x0;
		cmd_cdw[9]  = 0x0;
		cmd_cdw[10] = (uint32_t) (slba >> 0);
		cmd_cdw[11] = (uint32_t) (slba >> 32);
		cmd_cdw[12] = len/512/nhc_num;
//		cmd_cdw[12] = len/512/nhc_num*2*2;
//		cmd_cdw[12] = len/512/nhc_num*2;
	//			cmd_cdw[12] = 64;
		cmd_cdw[13] = dsm & 0xFF;
		cmd_cdw[14] = 0x0;
		cmd_cdw[15] = 0x0;

		sts = nhc_cmd_sub(j,cmd_cdw);////Previously, sub was behind while
		if ((nhc_num==1 || j == nhc_num-1) && (sts == 1))
		{
			i=i+1;
			j=j+1;
//			slba = slba + U_BLK_LBA_CNT/nhc_num;
			slba = slba +len/512/nhc_num;
			if (addr < (0x9F000000))
			{
				addr+=len/DDR_NUM;
			}
			else
			{
				addr= MEM_DDR3_BASE;
			}

//			if (addr >= (0xBFFFFFFF-len/DDR_NUM))
//				addr = 0x80000000;
//			else
//				addr = addr + len/DDR_NUM;
		}
		else if (sts == 1)
		{
			j=j+1;
		}
		else
		{
			sts = nhc_cmd_sts(j);
		}
	}
	if (addr == DDR3_START_ADDR)
	{
		full_wr_cnt = full_wr_cnt + 1;
//			xil_printf("Have write full ddr area.full write cnt is %d.\n",full_wr_cnt);
	}
}//while
    // Command Status
 	while (queue_rptr != queue_wptr)
 	{
 		sts = nhc_cmd_sts(0);
 		if (sts == 3)
 			return sts;
 	}
 	while (queue2_rptr != queue2_wptr)
 	{
 		sts = nhc_cmd_sts(1);
 		if (sts == 3)
 			return sts;
 	}
 	while (queue3_rptr != queue3_wptr)
 	{
 		sts = nhc_cmd_sts(2);
 		if (sts == 3)
 			return sts;
 	}
 	while (queue4_rptr != queue4_wptr)
 	{
 		sts = nhc_cmd_sts(3);
 		if (sts == 3)
 			return sts;
 	}
	return 0x02;
}

uint8_t io_write2(uint8_t nhc_num, uint32_t nsid, uint32_t addr, uint64_t slba, uint32_t len, uint32_t dsm)
{

	uint32_t cmd_cdw[16];
	uint8_t  sts;
	uint8_t  i,j;
	static uint32_t full_wr_cnt = 0;
	slba = slba/512/nhc_num*2;
//	slba = slba/512;
//	j=0;  //j代表总包数
//	xil_printf("io_write addr=0x%x slba=%u  ", addr, slba);
//	xil_printf("len=%lu\n", len);
	for(i=0;i<nhc_num;)
	{
		// Command Submission
		cmd_cdw[0]  = 0x80350081;
		cmd_cdw[1]  = nsid;
		cmd_cdw[2]  = 0x0;
		cmd_cdw[3]  = 0x0;
		cmd_cdw[4]  = 0x0;
		cmd_cdw[5]  = 0x0;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*U_BLK_SIZE/nhc_num;   // 9.27改
		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len*2/nhc_num;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len/nhc_num;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len;
//		cmd_cdw[6]  = addr + i*len/nhc_num;		// 3.9
		cmd_cdw[7]  = 0x0; // Non-zero if use 64bit memory address
		cmd_cdw[8]  = 0x0;
		cmd_cdw[9]  = 0x0;
		cmd_cdw[10] = (uint32_t) (slba >> 0);
		cmd_cdw[11] = (uint32_t) (slba >> 32);
//		cmd_cdw[12] = len/512/nhc_num;  	// 3.9
//		cmd_cdw[12] = len/512/nhc_num*2*2;
		cmd_cdw[12] = len/512/nhc_num*2;
	//			cmd_cdw[12] = 64;
		cmd_cdw[13] = dsm & 0xFF;
		cmd_cdw[14] = 0x0;
		cmd_cdw[15] = 0x0;

		sts = nhc_cmd_sub(i,cmd_cdw);////Previously, sub was behind while
		if(sts == 1)
		{
			i+=1;
		}
		else
		{
			sts = nhc_cmd_sts(i);
		}
	}
	if (addr == DDR3_START_ADDR)
	{
	 	full_wr_cnt = full_wr_cnt + 1;
//		xil_printf("Have write full ddr area.full write cnt is %d.\n",full_wr_cnt);
	}
// Command Status
	while (queue_rptr != queue_wptr)
	{
		sts = nhc_cmd_sts(0);
		if (sts == 3)
			return sts;
	}
	while (queue2_rptr != queue2_wptr)
	{
		sts = nhc_cmd_sts(1);
		if (sts == 3)
			return sts;
	}
	while (queue3_rptr != queue3_wptr)
	{
		sts = nhc_cmd_sts(2);
		if (sts == 3)
			return sts;
	}
	while (queue4_rptr != queue4_wptr)
	{
		sts = nhc_cmd_sts(3);
		if (sts == 3)
			return sts;
	}
	return 0x02;
}
// *********************************************************************************
// I/O Read
// <-0x0: not submitted; 0x1: Ongoing; 0x2: Successful; 0x3: Error
// *********************************************************************************
uint8_t io_read(uint8_t nhc_num, uint32_t nsid, uint32_t addr, uint64_t slba, uint32_t len, uint32_t dsm)
{
	uint32_t cmd_cdw[16];
	uint8_t  sts;
	uint8_t  i,j;
	u32 read_dataaddr;
	static uint32_t full_rd_cnt = 0;

	slba = 2*slba/512/nhc_num;
//	slba = slba/512;
	for(i=0;i<nhc_num;i++)
	{
//		if (i == 0)
//			continue;
		// Command Submission
		cmd_cdw[0]  = 0x80350082;
		cmd_cdw[1]  = nsid;
		cmd_cdw[2]  = 0x0;
		cmd_cdw[3]  = 0x0;
		cmd_cdw[4]  = 0x0;
		cmd_cdw[5]  = 0x0;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*U_BLK_SIZE/nhc_num;   // 9.27改
		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len*2/nhc_num;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len;
		cmd_cdw[7]  = 0x0; // Non-zero if use 64bit memory address
		cmd_cdw[8]  = 0x0;
		cmd_cdw[9]  = 0x0;
		cmd_cdw[10] = (uint32_t) (slba >> 0);
		cmd_cdw[11] = (uint32_t) (slba >> 32);
//		cmd_cdw[12] = len/512/nhc_num;
//		cmd_cdw[12] = len/512/nhc_num*2*2;   // 9.25
		cmd_cdw[12] = len/512/nhc_num*2;
//		cmd_cdw[12] = U_BLK_SIZE/512/nhc_num;
		cmd_cdw[13] = dsm & 0xFF;
		cmd_cdw[14] = 0x0;
		cmd_cdw[15] = 0x0;

//		while (nhc_queue_full(i) == 0x1)
//		{
//
//
//		}
		sts = nhc_cmd_sub(i,cmd_cdw);
	}
	//wait command ack.
	for(j=0;j<nhc_num;j++)
	{
		do {
			sts = nhc_cmd_sts(j);
		}while(sts == 0x01);
	}
//	usleep(5000);
//	read_dataaddr = Xil_In32(addr);
//	 xil_printf("  address: 0x%08x, data: 0x%08x \r\n",addr, read_dataaddr);
	//xil_printf("sleep 1s before\n");

//		xil_printf("sleep 1s after.....\n");
	//send ack.
//    send_axis_ack_1(0x02,addr,len/8192,sts);

	//send_axis_ack_1(0x02,addr,len/2,sts);
	//send_axis_ack_1(0x02,0x0,sts);
//	xil_printf("send_axis_ack is ok\n");
	if (addr == DDR3_START_ADDR)
	{
		full_rd_cnt = full_rd_cnt + 1;
		xil_printf("Have read full ddr area.full read cnt is %d.\n",full_rd_cnt);
	}
// Command Status
	while (queue_rptr != queue_wptr)
	{
		sts = nhc_cmd_sts(0);
		if (sts == 3)
			return sts;
	}
	while (queue2_rptr != queue2_wptr)
	{
		sts = nhc_cmd_sts(1);
		if (sts == 3)
			return sts;
	}
	while (queue3_rptr != queue3_wptr)
	{
		sts = nhc_cmd_sts(2);
		if (sts == 3)
			return sts;
	}
	while (queue4_rptr != queue4_wptr)
	{
		sts = nhc_cmd_sts(3);
		if (sts == 3)
			return sts;
	}
	return 0x02;
}

// *********************************************************************************
// I/O Read1   FIFO
// <-0x0: not submitted; 0x1: Ongoing; 0x2: Successful; 0x3: Error
// *********************************************************************************
uint8_t io_read1(uint8_t nhc_num, uint32_t nsid, uint32_t addr, uint64_t slba, uint32_t len, uint32_t dsm)
{
	uint32_t cmd_cdw[16];
	uint8_t  sts;
	uint8_t  i,j;
	u32 read_dataaddr;
	static uint32_t full_rd_cnt = 0;
	u32 Timeout=0;
	slba = slba/512/nhc_num;
//	Xil_L1DCacheFlush();
	for(i=0;i<nhc_num;i++)
	{
//		if (i == 0)
//			continue;
		// Command Submission
		cmd_cdw[0]  = 0x80350082;
		cmd_cdw[1]  = nsid;
		cmd_cdw[2]  = 0x0;
		cmd_cdw[3]  = 0x0;
		cmd_cdw[4]  = 0x0;
		cmd_cdw[5]  = 0x0;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*U_BLK_SIZE/nhc_num;   // 9.27改
		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len/nhc_num;
		cmd_cdw[7]  = 0x0; // Non-zero if use 64bit memory address
		cmd_cdw[8]  = 0x0;
		cmd_cdw[9]  = 0x0;
		cmd_cdw[10] = (uint32_t) (slba >> 0);
		cmd_cdw[11] = (uint32_t) (slba >> 32);
		cmd_cdw[12] = len/512/nhc_num;
//		cmd_cdw[12] = len/512/nhc_num*2*2;   // 9.25
//		cmd_cdw[12] = len/512/nhc_num*2;
//		cmd_cdw[12] = U_BLK_SIZE/512/nhc_num;
		cmd_cdw[13] = dsm & 0xFF;
		cmd_cdw[14] = 0x0;
		cmd_cdw[15] = 0x0;

//		while (nhc_queue_full(i) == 0x1)
//		{
//
//
//		}
		sts = nhc_cmd_sub(i,cmd_cdw);
	}
	//wait command ack.
	for(j=0;j<nhc_num;j++)
	{
		do {
			Timeout++;
			sts = nhc_cmd_sts(j);
		}while(sts == 0x01&&Timeout<=0x100000);
	}
//	usleep(5000);
//	read_dataaddr = Xil_In32(addr);
//	 xil_printf("  address: 0x%08x, data: 0x%08x \r\n",addr, read_dataaddr);
	//xil_printf("sleep 1s before\n");

//		xil_printf("sleep 1s after.....\n");
	//send ack.
    send_axis_ack_1(0x02,addr,len/8192,sts);

	//send_axis_ack_1(0x02,addr,len/2,sts);
	//send_axis_ack_1(0x02,0x0,sts);
//	xil_printf("send_axis_ack is ok\n");
	if (addr == DDR3_START_ADDR)
	{
		full_rd_cnt = full_rd_cnt + 1;
		xil_printf("Have read full ddr area.full read cnt is %d.\n",full_rd_cnt);
	}
// Command Status
	while (queue_rptr != queue_wptr)
	{
		sts = nhc_cmd_sts(0);
		if (sts == 3)
			return sts;
	}
	while (queue2_rptr != queue2_wptr)
	{
		sts = nhc_cmd_sts(1);
		if (sts == 3)
			return sts;
	}
	while (queue3_rptr != queue3_wptr)
	{
		sts = nhc_cmd_sts(2);
		if (sts == 3)
			return sts;
	}
	while (queue4_rptr != queue4_wptr)
	{
		sts = nhc_cmd_sts(3);
		if (sts == 3)
			return sts;
	}
	return 0x02;
}


//io_write2
uint8_t io_read2(uint8_t nhc_num, uint32_t nsid, uint32_t addr, uint64_t slba, uint32_t len, uint32_t dsm)
{
	uint32_t cmd_cdw[16];
	uint8_t  sts;
	uint8_t  i,j;
	u32 read_dataaddr;
	static uint32_t full_rd_cnt = 0;

	slba = slba/512/nhc_num*2;
//	slba = slba/512;
	for(i=0;i<nhc_num;)
	{
//		if (i == 0)
//			continue;
		// Command Submission
		cmd_cdw[0]  = 0x80350082;
		cmd_cdw[1]  = nsid;
		cmd_cdw[2]  = 0x0;
		cmd_cdw[3]  = 0x0;
		cmd_cdw[4]  = 0x0;
		cmd_cdw[5]  = 0x0;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*U_BLK_SIZE/nhc_num;   // 9.27改
		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len*2/nhc_num;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len/nhc_num;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len;
		cmd_cdw[7]  = 0x0; // Non-zero if use 64bit memory address
		cmd_cdw[8]  = 0x0;
		cmd_cdw[9]  = 0x0;
		cmd_cdw[10] = (uint32_t) (slba >> 0);
		cmd_cdw[11] = (uint32_t) (slba >> 32);
//		cmd_cdw[12] = len/512/nhc_num;
//		cmd_cdw[12] = len/512/nhc_num*2*2;   // 9.25
		cmd_cdw[12] = len/512/nhc_num*2;
//		cmd_cdw[12] = U_BLK_SIZE/512/nhc_num;
		cmd_cdw[13] = dsm & 0xFF;
		cmd_cdw[14] = 0x0;
		cmd_cdw[15] = 0x0;

//		while (nhc_queue_full(i) == 0x1)
//		{
//
//
//		}
		sts = nhc_cmd_sub(i,cmd_cdw);
		if(sts == 1)
		{
			i+=1;
		}
		else
		{
			sts = nhc_cmd_sts(i);
		}
	}
//	//wait command ack.
//	for(j=0;j<nhc_num;j++)
//	{
//		do {
//			sts = nhc_cmd_sts(j);
//		}while(sts == 0x01);
//	}
//	usleep(5000);
//	read_dataaddr = Xil_In32(addr);
//	 xil_printf("  address: 0x%08x, data: 0x%08x \r\n",addr, read_dataaddr);
	//xil_printf("sleep 1s before\n");

//		xil_printf("sleep 1s after.....\n");
	//send ack.
//    send_axis_ack_1(0x02,addr,len/8192,sts);

	//send_axis_ack_1(0x02,addr,len/2,sts);
	//send_axis_ack_1(0x02,0x0,sts);
//	xil_printf("send_axis_ack is ok\n");
	/*
	if (addr == DDR3_START_ADDR)
	{
		full_rd_cnt = full_rd_cnt + 1;
		xil_printf("Have read full ddr area.full read cnt is %d.\n",full_rd_cnt);
	}
	*/ //3.25注释 by lyh
// Command Status
	while (queue_rptr != queue_wptr)
	{
		sts = nhc_cmd_sts(0);
		if (sts == 3)
			return sts;
	}
	while (queue2_rptr != queue2_wptr)
	{
		sts = nhc_cmd_sts(1);
		if (sts == 3)
			return sts;
	}
	while (queue3_rptr != queue3_wptr)
	{
		sts = nhc_cmd_sts(2);
		if (sts == 3)
			return sts;
	}
	while (queue4_rptr != queue4_wptr)
	{
		sts = nhc_cmd_sts(3);
		if (sts == 3)
			return sts;
	}
	return 0x02;
}

//send_axis_ack_1
uint8_t io_read3(uint8_t nhc_num, uint32_t nsid, uint32_t addr, uint64_t slba, uint32_t len, uint32_t dsm)
{
	uint32_t cmd_cdw[16];
	uint8_t  sts;
	uint8_t  i,j;
	u32 read_dataaddr;
	static uint32_t full_rd_cnt = 0;

	slba = 2*slba/512/nhc_num;
	for(i=0;i<nhc_num;)
	{
//		if (i == 0)
//			continue;
		// Command Submission
		cmd_cdw[0]  = 0x80350082;
		cmd_cdw[1]  = nsid;
		cmd_cdw[2]  = 0x0;
		cmd_cdw[3]  = 0x0;
		cmd_cdw[4]  = 0x0;
		cmd_cdw[5]  = 0x0;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*U_BLK_SIZE/nhc_num;   // 9.27改
		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len*2/nhc_num;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len/nhc_num;
//		cmd_cdw[6]  = addr + (i%(nhc_num/DDR_NUM))*len;
		cmd_cdw[7]  = 0x0; // Non-zero if use 64bit memory address
		cmd_cdw[8]  = 0x0;
		cmd_cdw[9]  = 0x0;
		cmd_cdw[10] = (uint32_t) (slba >> 0);
		cmd_cdw[11] = (uint32_t) (slba >> 32);
//		cmd_cdw[12] = len/512/nhc_num;
//		cmd_cdw[12] = len/512/nhc_num*2*2;   // 9.25
		cmd_cdw[12] = len/512/nhc_num*2;
//		cmd_cdw[12] = U_BLK_SIZE/512/nhc_num;
		cmd_cdw[13] = dsm & 0xFF;
		cmd_cdw[14] = 0x0;
		cmd_cdw[15] = 0x0;

//		while (nhc_queue_full(i) == 0x1)
//		{
//
//
//		}
		sts = nhc_cmd_sub(i,cmd_cdw);
		if(sts == 1)
		{
			i+=1;
		}
		else
		{
			sts = nhc_cmd_sts(i);
		}
	}
//	//wait command ack.
//	for(j=0;j<nhc_num;j++)
//	{
//		do {
//			sts = nhc_cmd_sts(j);
//		}while(sts == 0x01);
//	}
//	usleep(5000);
//	read_dataaddr = Xil_In32(addr);
//	 xil_printf("  address: 0x%08x, data: 0x%08x \r\n",addr, read_dataaddr);
	//xil_printf("sleep 1s before\n");

//		xil_printf("sleep 1s after.....\n");
	//send ack.
//    send_axis_ack_1(0x02,addr,len/8192,sts);  //11.17

	//send_axis_ack_1(0x02,addr,len/2,sts);
	//send_axis_ack_1(0x02,0x0,sts);
//	xil_printf("send_axis_ack is ok\n");
	/*
	if (addr == DDR3_START_ADDR)
	{
		full_rd_cnt = full_rd_cnt + 1;
		xil_printf("Have read full ddr area.full read cnt is %d.\n",full_rd_cnt);
	}
	*/  // 3.25注释 by lyh
// Command Status
	while (queue_rptr != queue_wptr)
	{
		sts = nhc_cmd_sts(0);
		if (sts == 3)
			return sts;
	}
	while (queue2_rptr != queue2_wptr)
	{
		sts = nhc_cmd_sts(1);
		if (sts == 3)
			return sts;
	}
	while (queue3_rptr != queue3_wptr)
	{
		sts = nhc_cmd_sts(2);
		if (sts == 3)
			return sts;
	}
	while (queue4_rptr != queue4_wptr)
	{
		sts = nhc_cmd_sts(3);
		if (sts == 3)
			return sts;
	}
	return 0x02;
}
// *********************************************************************************
// I/O Dataset Management
// <-0x0: not submitted; 0x1: Ongoing; 0x2: Successful; 0x3: Error
// *********************************************************************************
uint8_t io_dsm(uint8_t nhc_num, uint32_t nsid, uint32_t addr, uint32_t dsm)
{
	uint32_t cmd_cdw[16];
	uint8_t  sts;
	uint8_t  i;

	// Command Submission
	cmd_cdw[0]  = 0x80010089;
	cmd_cdw[1]  = nsid;
	cmd_cdw[2]  = 0x0;
	cmd_cdw[3]  = 0x0;
	cmd_cdw[4]  = 0x0;
	cmd_cdw[5]  = 0x0;
	cmd_cdw[6]  = addr;
	cmd_cdw[7]  = 0x0; // Non-zero if use 64bit memory address
	cmd_cdw[8]  = 0x0;
	cmd_cdw[9]  = 0x0;
	cmd_cdw[10] = 0x0; // One Range
	cmd_cdw[11] = dsm;
	cmd_cdw[12] = 0x8; // Transfer 4KB DSM data
	cmd_cdw[13] = 0x0;
	cmd_cdw[14] = 0x0;
	cmd_cdw[15] = 0x0;

	for(i=0;i<nhc_num;i++)
	{
		nhc_cmd_sub(i,cmd_cdw);
	}

	// Command Status
	for(i=0;i<nhc_num;i++)
	{
		sts = nhc_cmd_sts(i);
	}
	return sts;
}

// *********************************************************************************
// I/O Monitor
// *********************************************************************************
void io_monitor_enable(uint8_t inst)
{
	uint32_t nhc_base = (inst == 0)?NHC_BASE:
                        (inst == 1)?NHC2_BASE:
        		        (inst == 2)?NHC3_BASE:NHC4_BASE;
	Xil_Out32(nhc_base + NHC_PMON_ER, 0x3);
}

void io_monitor_disable(uint8_t inst)
{
	uint32_t nhc_base = (inst == 0)?NHC_BASE:
                        (inst == 1)?NHC2_BASE:
        		        (inst == 2)?NHC3_BASE:NHC4_BASE;
	Xil_Out32(nhc_base + NHC_PMON_ER, 0x0);
}

void io_monitor_clear(uint8_t inst)
{
	uint32_t nhc_base = (inst == 0)?NHC_BASE:
                        (inst == 1)?NHC2_BASE:
        		        (inst == 2)?NHC3_BASE:NHC4_BASE;
	Xil_Out32(nhc_base + NHC_PMON_CR, 0x3);
}

uint32_t io_monitor_wr(uint8_t inst, uint16_t cycle)
{
	uint32_t nhc_base = (inst == 0)?NHC_BASE:
                        (inst == 1)?NHC2_BASE:
	                    (inst == 2)?NHC3_BASE:NHC4_BASE;

	uint32_t lbac_dw0 = Xil_In32(nhc_base + NHC_PMON_WLBAC);
	uint32_t lbac_dw1 = Xil_In32(nhc_base + NHC_PMON_WLBAC + 0x4);
	uint64_t lbac = lbac_dw1;
	lbac = (lbac << 32) | lbac_dw0;
	xil_printf("LBA counter is (in 512B unit): %x\n", lbac);

	uint32_t cc_dw0 = Xil_In32(nhc_base + NHC_PMON_WCYCLEC);
	uint32_t cc_dw1 = Xil_In32(nhc_base + NHC_PMON_WCYCLEC + 0x4);
	uint64_t cc = cc_dw1;
	cc = (cc << 32) | cc_dw0;
	xil_printf("consumed cycle is: %x\n", cc);

	uint64_t wr_us   = (cc*cycle)/(1000*1000);
	uint64_t wr_byte = (lbac*512);

	uint32_t wr_thr_KB_ms = (wr_byte*1000)/(wr_us*1024);
	uint32_t wr_thr_MB_s = (wr_thr_KB_ms*1000)/1024;

	return wr_thr_MB_s;
}

uint32_t io_monitor_rd(uint8_t inst, uint16_t cycle)
{
	uint32_t nhc_base = (inst == 0)?NHC_BASE:
            (inst == 1)?NHC2_BASE:
	        (inst == 2)?NHC3_BASE:NHC4_BASE;

	uint32_t lbac_dw0 = Xil_In32(nhc_base + NHC_PMON_RLBAC);
	uint32_t lbac_dw1 = Xil_In32(nhc_base + NHC_PMON_RLBAC + 0x4);
	uint64_t lbac = lbac_dw1;
	lbac = (lbac << 32) | lbac_dw0;
	xil_printf("LBA counter is (in 512B unit): %x\n", lbac);

	uint32_t cc_dw0 = Xil_In32(nhc_base + NHC_PMON_RCYCLEC);
	uint32_t cc_dw1 = Xil_In32(nhc_base + NHC_PMON_RCYCLEC + 0x4);
	uint64_t cc = cc_dw1;
	cc = (cc << 32) | cc_dw0;
	xil_printf("consumed cycle is: %x\n", cc);

	uint64_t rd_us   = (cc*cycle)/(1000*1000);
	uint64_t rd_byte = (lbac*512);

	uint32_t rd_thr_KB_ms = (rd_byte*1000)/(rd_us*1024);
	uint32_t rd_thr_MB_s = (rd_thr_KB_ms*1000)/1024;

	return rd_thr_MB_s;
}

// *********************************************************************************
// Software Reset
// *********************************************************************************
void nhc_sw_reset(uint8_t inst)
{
	uint32_t rdata;
	uint8_t  sts;

	uint32_t nhc_base = (inst == 0)?NHC_BASE:
            (inst == 1)?NHC2_BASE:
	        (inst == 2)?NHC3_BASE:NHC4_BASE;
        // Write '1b' to start Software Reset
	Xil_Out32(nhc_base + NHC_IP_CSR, (0x1 << 2));

        // Reset Completed when changed to '0b'
	while(1)
	{
		rdata = Xil_In32(nhc_base + NHC_IP_CSR);
		sts = ((rdata >> 2) & 0x1); // bit2

		if(sts == 0x0) // Completed
		{
			xil_printf("Software Reset is completed!\n");
			break;
		}
	}
}

// *********************************************************************************
// Send AXIS ACK
// *********************************************************************************
void send_axis_ack(uint8_t type,uint32_t addr,uint32_t len,uint8_t sts)
{
	SourceBuffer[0] = addr;
	SourceBuffer[1] = len;
//	SourceBuffer[2] = 0;
//	SourceBuffer[3] = 0;

	TxSend(SourceBuffer,8);
	cmd_ack_cnt += 1;
	//xil_printf("Have Send command ack.ack count is %d.\n",cmd_ack_cnt);
}

void send_axis_ack_1(uint8_t type,uint32_t addr,uint32_t len,uint8_t sts)
{
//	SourceBuffer[0] = (ack_dat & 0xffffffff);
//	SourceBuffer[1] = (ack_dat >> 32);
//	SourceBuffer[2] = 0;
//	SourceBuffer[3] = 0;
//
//	TxSend_1(SourceBuffer,4);
	//cmd_ack_cnt += 1;
	//xil_printf("Have Send command ack.ack count is %d.\n",cmd_ack_cnt);

	SourceBuffer[0] = addr;
	SourceBuffer[1] = len;
//	SourceBuffer[2] = 0;
//	SourceBuffer[3] = 0;
	xil_printf("%lx %lx\n",addr,len);
	TxSend_1(SourceBuffer,8);
	cmd_ack_cnt += 1;
	//xil_printf("Have Send command ack.ack count is %d.\n",cmd_ack_cnt);


}

void DiskInit()
{
	uint8_t  i;
	timeout_sec = 1;
    xil_printf("NVMe Host IP Core Initialization Starts....\r\n");

    for(i=0;i<NHC_NUM;i++)
    {
		if(nhc_init(i,timeout_sec, freq_MHz) == 0x1) // 250MHz
		{
			xil_printf("NVMe Host IP Core #%1d Initialization Completes.\r\n",i);
		}
		else
		{
			xil_printf("NVMe Host IP Core #%1d Initialization Failed!\r\n",i);
		}
    }
    // ---------------------------------------------------------------------------
    // NHC Queue Configuration: 64-nodes depth and User mode
    // ---------------------------------------------------------------------------
    xil_printf("User Command Queue Configuration Starts....\r\n");
    for(i=0;i<NHC_NUM;i++)
    {
		if(nhc_queue_init(i,63, 0) == 0x1)
		{
			xil_printf("Core #%1d User Command Queue Configuration Completes.\r\n",i);
		}
		else
		{
			xil_printf("Core #%1d User Command Queue Configuration Failed!\r\n",i);
		}
    }
    // ---------------------------------------------------------------------------
    // NHC Startup
    // ---------------------------------------------------------------------------
    xil_printf("NHC Startup Starts(Identification, I/O Queue, ...)....\r\n");
    for(i=0;i<NHC_NUM;i++)
    {
		if(adm_startup(i) == 0x2)
		{
			xil_printf("NHC%1d Startup Completes.\r\n",i);
		}
		else
		{
			xil_printf("NHC%1d Startup Failed!\r\n",i);
		}
    }

    init_done = 0xFF;
    for(i=0;i<NHC_NUM;i++)
    {
    	io_monitor_enable(i);
    }

}
