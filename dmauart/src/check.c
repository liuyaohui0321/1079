#include "check.h"   // 2023.10.26 lyh¼Ó

//Check whether the rule of the block address value increment is consistent with the rule of number creation
CheckResult CheckOutAll(uint32_t *write,uint32_t *read,uint32_t num)
{
	uint32_t i=0;
    while(i<num/4)
    {
        if(*(write+i)!=*(read+i))
        {
        	 xil_printf("error address1:0x%lx  0x%x  error address2:0x%lx  0x%x\r\n",write+i,*(write+i),read+i,*(read+i));
             return error;
        }
        i++;
    }
    return right;
}

//Check the difference between the header address and the tail address
CheckResult CheckOut_HeadTail(uint32_t *frist,uint32_t *last,uint32_t blocksize)
{
	uint32_t i=0;
	uint32_t length=(last-frist)*4;
	while(i<length/blocksize)
	{
//		if(  *(frist+i*blocksize/4+blocksize/4-1)-*(frist+i*blocksize/4)!=(blocksize-4))
		if(  *(frist+i*blocksize/4+blocksize/4-1)-*(frist+i*blocksize/4)!=(blocksize/2-9))
		{
			xil_printf("Error Tail Address:0x%lx  0x%x  Error Head Address:0x%lx  0x%x\r\n",frist+i*blocksize/4+blocksize/4-1,*(frist+i*blocksize/4+blocksize/4-1),frist+i*blocksize/4,*(frist+i*blocksize/4));
			xil_printf("The Total Size Of Checking Success:%dMB\r\n",i*blocksize/1024/1024);
			return error;
		}
		xil_printf("Check going... Tail Address:0x%lx  0x%x   Head Address:0x%lx  0x%x offset:0x%x\r\n",frist+i*blocksize/4+blocksize/4-1,*(frist+i*blocksize/4+blocksize/4-1),frist+i*blocksize/4,*(frist+i*blocksize/4),blocksize);
		i++;
	}
	xil_printf("The Total Size Of Checking Success:%dMB\r\n",i*blocksize/1024/1024);
	return right;
}
