//ADC.c
#include "pin_connect_block.h"
#include "ADC_defines.h"
#include <lpc21xx.h>
#include "delay.h"
void Init_ADC(void)
{
	//cfg p0.27 as AIN0
	CfgPortPinFunc(0,27,1);//ch0
	CfgPortPinFunc(0,28,1);//ch1
	//CfgPortPinFunc(0,29,1);//ch2
	//CfgPortPinFunc(0,30,1);//ch3
	//PINSEL1|=0x15400000;
	//take ADC into operational mode and set ADC_CLK
	ADCR=PDN_BIT|CLKDIV_VALUE;
}
void Read_ADC(u32 CHNO,u32* AdcDval,f32* eAR)
{
	//clear the channel bits
	ADCR&=~(255<<0);
	//select channel & start conv
	ADCR|=CHNO|START_CONV;
	//conversion time
	delay_us(3);
	//wait for done bit status
	while(((ADDR>>DONE_BIT)&1)==0);
	//stop conv
	ADCR&=~(START_CONV);
	//extract result
	*AdcDval=((ADDR>>RESULT)&1023);
	//analog reading
	//(stepsize*digital_output)
	*eAR=(3.3/1024)*(*AdcDval);
}
/*u32 dval;
	f32 eAR;
int main()
{
	
	Init_ADC();
	while(1)
	{
		Read_ADC(CH0,&dval,&eAR);
	}
}*/

