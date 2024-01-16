#include "uart_get.h"

// *********************************************************************************
// mygets: get data from UART input port
// *********************************************************************************
char * mygets(char *string)
{
	char x=0;

	while(x!= '\r')
	{
		x=inbyte();
		*string++ = x;
	}

   *(--string) = '\0';
}

