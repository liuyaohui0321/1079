#include "time.h"

int TmrCtrSelfTestExample(u16 DeviceId, u8 TmrCtrNumber)
{
    int Status;
	XTmrCtr *TmrCtrInstancePtr = &TimerCounter;

	/*
	 * Initialize the TmrCtr driver so that it iss ready to use
	 */
	Status = XTmrCtr_Initialize(TmrCtrInstancePtr, DeviceId);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
}
