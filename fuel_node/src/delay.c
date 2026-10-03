//delay.c
#include "types.h"
void delay_us(u32 dlyUS)
{
	dlyUS*=12;
	while(dlyUS--);
}

void delay_ms(u32 dlyMS)
{
	dlyMS*=12000;
	while(dlyMS--);
}

void delay_s(u32 dlyS)
{
	dlyS*=12000000;
	while(dlyS--);
}
