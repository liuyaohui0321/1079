#ifndef __7X_DMA_H_
#define __7X_DMA_H_
/*
 * 7X_dma.h
 *
 *  Created on: 2023Äê8ÔÂ23ÈÕ
 *      Author: liuyaohui
 */

#include "xaxidma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xdebug.h"
#include "cmd.h"


#ifdef XPAR_INTC_0_DEVICE_ID
#include "xintc.h"
#else
#include "xscugic.h"
#endif

#define TX_BUFFER_BASE1		    0xC0100000
#define RX_BUFFER_BASE1		    0xC0300000

#define TX_BUFFER_BASE2		    0xC0400000
#define RX_BUFFER_BASE2		    0xC0600000

#define RX_INTR_ID1				XPAR_AXI_INTC_0_AXI_DMA_1_S2MM_INTROUT_INTR   // DMA1_INTC0
#define TX_INTR_ID1				XPAR_AXI_INTC_0_AXI_DMA_1_MM2S_INTROUT_INTR
#define MAX_PKT_LEN		0x100
//
static XAxiDma AxiDma1;
static XIntc Intc1;
volatile int TxDone1;
volatile int RxDone1;
volatile int Error1;

u8 *CmdTxBufferPtr1;
u8 *CmdRxBufferPtr1;
int XAxiDma1_tx_rx(u16 DeviceId);
static void TxIntrHandler1(void *Callback);
static void RxIntrHandler1(void *Callback);
static int SetupIntrSystem1(XIntc * IntcInstancePtr,XAxiDma * AxiDmaPtr, u16 TxIntrId, u16 RxIntrId);
int Dma1Init();
int XAxiDma1_tx(u16 DeviceId);
void DisableIntrSystem1(XIntc * IntcInstancePtr,u16 TxIntrId, u16 RxIntrId);
#endif /* _7X_DMA_H_ */
