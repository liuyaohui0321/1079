#include "7x_dma.h"
#include "cmd.h"   // 2023.9.20加

int path_length=0;
int Dma1Init()
{
		int Status;
		XAxiDma_Config *Config;
		CmdTxBufferPtr1 = (u8 *)TX_BUFFER_BASE1;
		CmdRxBufferPtr1 = (u8 *)RX_BUFFER_BASE1;

		Config = XAxiDma_LookupConfig(XPAR_AXI_DMA_1_DEVICE_ID);
		if (!Config) {
			xil_printf("No config found for %d\r\n", XPAR_AXIDMA_1_DEVICE_ID);
			return XST_FAILURE;
		}
//		xil_printf("%s %d\n", __FUNCTION__, __LINE__);
		/* Initialize DMA engine */
		Status = XAxiDma_CfgInitialize(&AxiDma1, Config);

		if (Status != XST_SUCCESS) {
			xil_printf("Initialization failed %d\r\n", Status);
			return XST_FAILURE;
		}

		if(XAxiDma_HasSg(&AxiDma1)) {
			xil_printf("Device configured as SG mode \r\n");
			return XST_FAILURE;
		}

		/* Set up Interrupt system  */
		Status = SetupIntrSystem1(&Intc1, &AxiDma1, TX_INTR_ID1, RX_INTR_ID1);
		if (Status != XST_SUCCESS) {

			xil_printf("Failed intr setup\r\n");
			return XST_FAILURE;
		}  //10.8

		/* Disable all interrupts before setup */

		XAxiDma_IntrDisable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK,
							XAXIDMA_DMA_TO_DEVICE);

		XAxiDma_IntrDisable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK,
					XAXIDMA_DEVICE_TO_DMA);

		/* Enable all interrupts */
		XAxiDma_IntrEnable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK,
								XAXIDMA_DMA_TO_DEVICE);

		XAxiDma_IntrEnable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK,
								XAXIDMA_DEVICE_TO_DMA);

		/* Initialize flags before start transfer test  */
		TxDone1 = 0;
		RxDone1 = 0;
		Error1 = 0;

		Xil_DCacheFlushRange((UINTPTR)CmdRxBufferPtr1, MAX_PKT_LEN);

		Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR) CmdRxBufferPtr1,
					MAX_PKT_LEN, XAXIDMA_DEVICE_TO_DMA);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		// 12.4 add by lyh
		Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR) CmdTxBufferPtr1,
					MAX_PKT_LEN, XAXIDMA_DMA_TO_DEVICE);

		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		return 0;

}

int XAxiDma1_tx_rx(u16 DeviceId)
{
		XAxiDma_Config *CfgPtr;
		int Status;
//		int Tries = NUMBER_OF_TRANSFERS;
		int value = 0;
		int Index;
		u64 *TxBufferPtr;
		u64 *RxBufferPtr;
		TxBufferPtr = (u64 *)TX_BUFFER_BASE1;
		RxBufferPtr = (u64 *)RX_BUFFER_BASE1;
		/* Initialize the XAxiDma device.
		 */
		CfgPtr = XAxiDma_LookupConfig(DeviceId);
		if (!CfgPtr) {
			xil_printf("No config found for %d\r\n", DeviceId);
			return XST_FAILURE;
		}

		Status = XAxiDma_CfgInitialize(&AxiDma1, CfgPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("Initialization failed %d\r\n", Status);
			return XST_FAILURE;
		}

		if(XAxiDma_HasSg(&AxiDma1)){
			xil_printf("Device configured as SG mode \r\n");
			return XST_FAILURE;
		}

		/* Disable interrupts, we use polling mode
		 */
		XAxiDma_IntrDisable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK,
							XAXIDMA_DEVICE_TO_DMA);
		XAxiDma_IntrDisable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK,
							XAXIDMA_DMA_TO_DEVICE);

		//****************LYH  2023.8.7锟斤拷锟斤拷  锟斤拷***************//
		//    	Value = TEST_START_VALUE;     //********LYH  2023.8.7锟斤拷锟斤拷  注锟斤拷*******//

				for(Index = 0; Index < 259; Index ++) {
					   TxBufferPtr[Index]=(u64)0;
					   TxBufferPtr[Index]|=value++;
				 }

					   TxBufferPtr[259]=0x123456783C3CBCBA;
					   TxBufferPtr[260]=0x020000003C3CBCBB;
					   TxBufferPtr[261]=0x000200003C3CBCBC;
		/* Flush the SrcBuffer before the DMA transfer, in case the Data Cache
		 * is enabled
		 */
			Xil_DCacheFlushRange((UINTPTR)TxBufferPtr, MAX_PKT_LEN);
		#ifdef __aarch64__
			Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, MAX_PKT_LEN);
		#endif

	//	for(Index = 0; Index < Tries; Index ++) {
			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR) RxBufferPtr,
					2096, XAXIDMA_DEVICE_TO_DMA);

			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}

			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR) TxBufferPtr,
					2096, XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}
		return XST_SUCCESS;
}
static void TxIntrHandler1(void *Callback)
{

	u32 IrqStatus;
	int TimeOut;
	XAxiDma *AxiDmaInst = (XAxiDma *)Callback;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DMA_TO_DEVICE);

	/* Acknowledge pending interrupts */


	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DMA_TO_DEVICE);

	/*
	 * If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {

		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		Error1 = 1;

		/*
		 * Reset should never fail for transmit channel
		 */
		XAxiDma_Reset(AxiDmaInst);

		TimeOut = 10000;

		while (TimeOut) {
			if (XAxiDma_ResetIsDone(AxiDmaInst)) {
				break;
			}
			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If Completion interrupt is asserted, then set the TxDone flag
	 */
	if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK))
	{
		TxDone1 = 1;
	}
}

/*****************************************************************************/
/*
*
* This is the DMA RX interrupt handler function
*
* It gets the interrupt status from the hardware, acknowledges it, and if any
* error happens, it resets the hardware. Otherwise, if a completion interrupt
* is present, then it sets the RxDone flag.
*
* @param	Callback is a pointer to RX channel of the DMA engine.
*
* @return	None.
*
* @note		None.
*
//******************************************************************************/
static void RxIntrHandler1(void *Callback)
{
	u32 IrqStatus,Status;
	int TimeOut;
	XAxiDma *AxiDmaInst = (XAxiDma *)Callback;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DEVICE_TO_DMA);

	/* Acknowledge pending interrupts */
	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DEVICE_TO_DMA);

	/*
	 * If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		Error1 = 1;

		/* Reset could fail and hang
		 * NEED a way to handle this or do not call it??
		 */
		XAxiDma_Reset(AxiDmaInst);

		TimeOut = 10000;

		while (TimeOut)
		{
			if(XAxiDma_ResetIsDone(AxiDmaInst))
			{
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If completion interrupt is asserted, then set RxDone flag
	 */
	if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)) {

		RxDone1 = 1;
	}
	x7_parse();
//	run_cmd_d203();  // 9.20 加
	Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR) CmdRxBufferPtr1,
		MAX_PKT_LEN, XAXIDMA_DEVICE_TO_DMA);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
}
/*****************************************************************************/
/** This function setups the interrupt system so interrupts can occur for the
* DMA, it assumes INTC component exists in the hardware system.
*
* @param	IntcInstancePtr is a pointer to the instance of the INTC.
* @param	AxiDmaPtr is a pointer to the instance of the DMA engine
* @param	TxIntrId is the TX channel Interrupt ID.
* @param	RxIntrId is the RX channel Interrupt ID.
*
* @return
*		- XST_SUCCESS if successful,
*		- XST_FAILURE.if not succesful
*
* @note		None.
*
******************************************************************************/
static int SetupIntrSystem1(XIntc * IntcInstancePtr,XAxiDma * AxiDmaPtr, u16 TxIntrId, u16 RxIntrId)
{
	int Status;

#ifdef XPAR_INTC_0_DEVICE_ID

	/* Initialize the interrupt controller and connect the ISRs */
	Status = XIntc_Initialize(IntcInstancePtr, XPAR_AXI_INTC_0_DEVICE_ID);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed init intc\r\n");
		return XST_FAILURE;
	}

	Status = XIntc_Connect(IntcInstancePtr, TxIntrId,
			       (XInterruptHandler) TxIntrHandler1, AxiDmaPtr);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed tx connect intc\r\n");
		return XST_FAILURE;
	}

	Status = XIntc_Connect(IntcInstancePtr, RxIntrId,
			       (XInterruptHandler) RxIntrHandler1, AxiDmaPtr);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed rx connect intc\r\n");
		return XST_FAILURE;
	}

	/* Start the interrupt controller */
	Status = XIntc_Start(IntcInstancePtr, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed to start intc\r\n");
		return XST_FAILURE;
	}

	XIntc_Enable(IntcInstancePtr, TxIntrId);
	XIntc_Enable(IntcInstancePtr, RxIntrId);

#else

	XScuGic_Config *IntcConfig;


	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	XScuGic_SetPriorityTriggerType(IntcInstancePtr, TxIntrId, 0xA0, 0x3);

	XScuGic_SetPriorityTriggerType(IntcInstancePtr, RxIntrId, 0xA0, 0x3);
	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */
	Status = XScuGic_Connect(IntcInstancePtr, TxIntrId,
				(Xil_InterruptHandler)TxIntrHandler,
				AxiDmaPtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	Status = XScuGic_Connect(IntcInstancePtr, RxIntrId,
				(Xil_InterruptHandler)RxIntrHandler,
				AxiDmaPtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	XScuGic_Enable(IntcInstancePtr, TxIntrId);
	XScuGic_Enable(IntcInstancePtr, RxIntrId);


#endif

	/* Enable interrupts from the hardware */

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)XIntc_InterruptHandler,
			(void *)IntcInstancePtr);

	Xil_ExceptionEnable();

	return XST_SUCCESS;
}


int XAxiDma1_tx(u16 DeviceId)
{
		XAxiDma_Config *CfgPtr;
		int Status;
		int value = 0;
		int Index;
		u64 *TxBufferPtr1;
		u64 *RxBufferPtr1;

		TxBufferPtr1 = (u64 *)TX_BUFFER_BASE2;
		RxBufferPtr1 = (u64 *)RX_BUFFER_BASE2;
		/* Initialize the XAxiDma device.
		 */
		CfgPtr = XAxiDma_LookupConfig(DeviceId);
		if (!CfgPtr) {
			xil_printf("No config found for %d\r\n", DeviceId);
			return XST_FAILURE;
		}

		Status = XAxiDma_CfgInitialize(&AxiDma1, CfgPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("Initialization failed %d\r\n", Status);
			return XST_FAILURE;
		}

		if(XAxiDma_HasSg(&AxiDma1)){
			xil_printf("Device configured as SG mode \r\n");
			return XST_FAILURE;
		}

		/* Disable all interrupts before setup */

		XAxiDma_IntrDisable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK,
							XAXIDMA_DMA_TO_DEVICE);

		XAxiDma_IntrDisable(&AxiDma1, XAXIDMA_IRQ_ALL_MASK,
					XAXIDMA_DEVICE_TO_DMA);

		for(Index = 0; Index < 259; Index ++)
		{
			   TxBufferPtr1[Index]=(u64)0;
			   TxBufferPtr1[Index]|=value++;
		}
			   TxBufferPtr1[259]=0x123456783C3CBCBA;
			   TxBufferPtr1[260]=0x000000103C3CBCBB;
			   TxBufferPtr1[261]=0x000200003C3CBCBC;
		/* Flush the SrcBuffer before the DMA transfer, in case the Data Cache
		 * is enabled
		 */
			Xil_DCacheFlushRange((UINTPTR)TxBufferPtr1, MAX_PKT_LEN);
		#ifdef __aarch64__
			Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, MAX_PKT_LEN);
		#endif

//     	    for(Index = 0; Index < Tries; Index ++) {
//			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR) RxBufferPtr1,
//					2096, XAXIDMA_DEVICE_TO_DMA);
//
//			if (Status != XST_SUCCESS) {
//				return XST_FAILURE;
//			}

			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR) TxBufferPtr1,
					2096, XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}
			return XST_SUCCESS;
}

void DisableIntrSystem1(XIntc * IntcInstancePtr,u16 TxIntrId, u16 RxIntrId)
{
		#ifdef XPAR_INTC_0_DEVICE_ID
			/* Disconnect the interrupts for the DMA TX and RX channels */
			XIntc_Disconnect(IntcInstancePtr, TxIntrId);
			XIntc_Disconnect(IntcInstancePtr, RxIntrId);
		#else
			XScuGic_Disconnect(IntcInstancePtr, TxIntrId);
			XScuGic_Disconnect(IntcInstancePtr, RxIntrId);
		#endif
}


// 存盘的时候从DMA1获取存盘的路径
int x7_parse(void)
{
			StructMsg TMsg;
			int i=0;
			Xil_DCacheInvalidateRange((UINTPTR)CmdRxBufferPtr1, CMD_PACK_LEN);
			if(0x55555555 != CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3]))
				return -1;
			i+=4;
			if(SRC_ID != CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3]))
				return -1;
			i+=4;
			if(DEST_ID != CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3]))
				return -1;
			i+=4;
			TMsg.HandType = CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3]);
			i+=4;
			TMsg.HandId = CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3]);
			i+=4;
			TMsg.PackNum = CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3]);
			i+=4;

			while(1)
			{
				if(CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3])==0xAAAAAAAA)  break;
				i++;
				path_length++;
			}
			TMsg.DataLen=path_length;
			i-=8;
			if(CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3])==0xBBBBBBBBCCCCCCCC)
			{
				xil_printf("stop write\r\n");
				return -1;
			}
			xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
			x7_cmd_type_id_parse(&TMsg);

//			switch(TMsg.HandType)
//			{
//				case 0xd2:
//					switch(TMsg.HandId)
//					{
//
//						case 0x3:
//							i+=D203_DATA_LEN;
//						break;
//
//						case 0x5:
//							i+=D205_DATA_LEN;
//						break;
//
//						default:
//						break;
//					}
//				break;
//				default:
//				break;
//			}
//			xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
//	//	    RecvStructCmd.CheckCode = CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
//	//		i+=4;    //校验码功能加好之后，需要打开
//			a=CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3]);
//			if(0xAAAAAAAA != CW32(CmdRxBufferPtr1[i+0],CmdRxBufferPtr1[i+1],CmdRxBufferPtr1[i+2],CmdRxBufferPtr1[i+3]))
//				return -1;

}

void x7_cmd_type_id_parse(StructMsg *pMsg)
{
		int i=0;
		StructMsg TMsg;
		TMsg.HandType=pMsg->HandType;
		TMsg.HandId=pMsg->HandId;
		TMsg.PackNum=pMsg->PackNum;
		TMsg.DataLen=pMsg->DataLen;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		switch(pMsg->HandType)
		{

				case 0xd2:
					switch(pMsg->HandId)
					{
						case 0x3:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr1[i+24];
							}
						break;

						case 0x5:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr1[i+24];
							}
						break;

						default:
							return 0;
						break;
					}
				break;

				default:
					return 0;
				break;
		}
		SendMessage( &TMsg );
}
