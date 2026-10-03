//main.c  --  FUEL NODE
//CAN-Driven Vehicle Monitoring and Driver Assistance System
//MCU: NXP LPC2129 (ARM7TDMI-S) | Compiler: Keil uVision / ARM C
//
//Reads the Fuel Gauge potentiometer on AD0.1 (P0.28) using the class
//Init_ADC()/Read_ADC() driver, converts it to 0-100%, and transmits it to
//the Main Node over CAN (CAN_ID_FUEL_LEVEL), periodically and immediately
//on a significant change.
//
//PIN MAP:
//    Fuel gauge wiper  : P0.28 / AD0.1 (channel CH1)
//    CAN1 (to MCP2551) : TD1 = dedicated pin, RD1 = P0.25
#include <LPC21xx.h>
#include "types.h"
#include "pin_connect_block.h"
#include "ADC_defines.h"
#include "ADC.h"
#include "delay.h"
#include "can_defines.h"
#include "can.h"
#include "can_ids.h"

#define FUEL_CHANGE_THRESHOLD   2   //percent - triggers an immediate CAN update

static void init_clocks(void)
{
	PLLCFG  = 0x24;
	PLLFEED = 0xAA; PLLFEED = 0x55;
	PLLCON  = 0x01;
	PLLFEED = 0xAA; PLLFEED = 0x55;
	while(!(PLLSTAT&(1<<10)));
	PLLCON  = 0x03;
	PLLFEED = 0xAA; PLLFEED = 0x55;

	MAMCR  = 0x02;
	MAMTIM = 0x04;
	//VPBDIV left at reset value 0 -> PCLK = CCLK/4 = 15MHz, matching
	//can_defines.h and ADC_defines.h
}

static u8 read_fuel_percent(void)
{
	u32 dval;
	f32 eAR;
	Read_ADC(CH1,&dval,&eAR);
	return (u8)((dval*100UL)/1023UL);
}

int main(void)
{
	u8 fuel, last_sent=0xFF;   //0xFF = "never sent yet", forces first transmit
	s32 diff;
	CANF txF;

	init_clocks();
	Init_ADC();
	Init_CAN1();

	while(1)
	{
		fuel=read_fuel_percent();
		diff=(s32)fuel-(s32)last_sent;
		if(diff<0) diff=-diff;

		//immediate update on a significant change, otherwise a periodic
		//"keep-alive" update so the Main Node always has a recent value
		if(last_sent==0xFF || diff>=FUEL_CHANGE_THRESHOLD)
		{
			txF.ID=CAN_ID_FUEL_LEVEL;
			txF.BFV.RTR=0; txF.BFV.DLC=1;
			txF.Data1=fuel;
			txF.Data2=0;
			CAN1_Tx(txF);
			last_sent=fuel;
		}

		delay_ms(500);

		txF.ID=CAN_ID_FUEL_LEVEL;
		txF.BFV.RTR=0; txF.BFV.DLC=1;
		txF.Data1=fuel;
		txF.Data2=0;
		CAN1_Tx(txF);
		last_sent=fuel;
	}
}
