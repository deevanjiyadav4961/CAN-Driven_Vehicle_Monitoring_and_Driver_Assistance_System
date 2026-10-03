//main.c  --  MAIN NODE
//CAN-Driven Vehicle Monitoring and Driver Assistance System
//MCU: NXP LPC2129 (ARM7TDMI-S) | Compiler: Keil uVision / ARM C
//
//20x4 LCD layout:
//  Row1: TEMP:xx C
//  Row2: FUEL:xx%  <fuel level icon, 5 levels, built in CGRAM>
//  Row3: MODE:FWD                      (Forward mode)
//        REV:xxxCM SAFE/WARNING/STOP   (Reverse mode)
//  Row4: L:<left arrow, blinks when active>  R:<right arrow, blinks when active>
//
//Responsibilities:
//  - Continuously reads Engine Temperature (DS18B20) -> Row1
//  - Continuously receives Fuel % from the Fuel Node over CAN -> Row2 + icon
//  - Mode Selection Switch (EINT0) -> toggles Forward/Reverse, sent over CAN
//  - In Forward mode, Left/Right indicator switches (EINT1/EINT2) send
//    indicator commands to the Indicator & Reverse Alert Node, and the
//    corresponding Row4 arrow blinks.
//  - In Reverse mode, receives distance/SAFE/WARNING/STOP from the
//    Indicator & Reverse Alert Node -> Row3
//
//---------------------------------------------------------------------------
//PIN MAP (edit here if your board is wired differently)
//---------------------------------------------------------------------------
//  LCD 20x4 (8-bit)         : D0-D7=P0.8-P0.15, RS=P0.16, RW=P0.17, EN=P0.18
//  DS18B20 DQ (4.7k pullup) : P0.11
//  Mode Select Switch(EINT0): P0.1   (active LOW)
//  Left Indicator SW (EINT1): P0.3   (active LOW)
//  Right Indicator SW(EINT2): P0.7   (active LOW)
//  Buzzer (mirrors STOP)    : P0.19
//  CAN1 (to MCP2551)        : TD1 = dedicated pin, RD1 = P0.25
//----------------------------------------------------------------------------
#include <LPC21xx.h>
#include "types.h"
#include "defines.h"
#include "pin_connect_block.h"
#include "pin_func_defines.h"
#include "delay.h"
#include "can_defines.h"
#include "can.h"
#include "can_ids.h"
#include "lcd_defines.h"
#include "lcd.h"
#include "ds18b20.h"

#define SW_MODE_PIN     1    //P0.1  (EINT0)
#define BUZZER_PIN      19   //P0.19

#define EINT0_CH 14
#define EINT1_CH 15
#define EINT2_CH 16

//---- CGRAM custom character slots ------------------------------------
#define FUEL_ICON_0  0   //empty
#define FUEL_ICON_1  1   //~25%
#define FUEL_ICON_2  2   //~50%
#define FUEL_ICON_3  3   //~75%
#define FUEL_ICON_4  4   //full
#define ARROW_LEFT   5
#define ARROW_RIGHT  6

static s8 fuelIcon0[8]={0x0E,0x11,0x11,0x11,0x11,0x11,0x11,0x1F};
static s8 fuelIcon1[8]={0x0E,0x11,0x11,0x11,0x11,0x11,0x1F,0x1F};
static s8 fuelIcon2[8]={0x0E,0x11,0x11,0x11,0x1F,0x1F,0x1F,0x1F};
static s8 fuelIcon3[8]={0x0E,0x11,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F};
static s8 fuelIcon4[8]={0x0E,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F};
static s8 arrowLeft [8]={0x00,0x02,0x06,0x0E,0x1E,0x0E,0x06,0x02};
static s8 arrowRight[8]={0x00,0x08,0x0C,0x0E,0x0F,0x0E,0x0C,0x08};

//---- state shared between the ISRs and main() --------------------------
static volatile u8 vehicle_mode   = MODE_FORWARD;
static volatile u8 left_ind_on    = 0;
static volatile u8 right_ind_on   = 0;
static volatile u8 mode_changed   = 1;   //force first CAN update
static volatile u8 fuel_percent   = 0;
static volatile u8 reverse_status = REV_STATUS_SAFE;
static volatile u32 reverse_dist_cm = 0;

void eint0_isr(void)__irq;   //Mode select switch
void eint1_isr(void)__irq;   //Left indicator switch
void eint2_isr(void)__irq;   //Right indicator switch

//----------------------------------------------------------------------------
//init_clocks() - PLL setup for CCLK = 60 MHz from a 12 MHz crystal (matches
//FOSC/CCLK used in can_defines.h). VPBDIV is left at its reset value (0),
//so PCLK = CCLK/4 = 15 MHz, matching can_defines.h's PCLK definition.
//----------------------------------------------------------------------------
static void init_clocks(void)
{
	PLLCFG  = 0x24;              //MSEL=4(x5), PSEL=1(/2): 12MHz*5=60MHz CCLK
	PLLFEED = 0xAA; PLLFEED = 0x55;
	PLLCON  = 0x01;               //enable PLL
	PLLFEED = 0xAA; PLLFEED = 0x55;
	while(!(PLLSTAT&(1<<10)));    //wait for PLOCK
	PLLCON  = 0x03;               //connect PLL as the CPU clock
	PLLFEED = 0xAA; PLLFEED = 0x55;

	MAMCR  = 0x02;
	MAMTIM = 0x04;
}

static void init_gpio(void)
{
	IODIR0|=1<<BUZZER_PIN;
	IOCLR0=1<<BUZZER_PIN;
}

//----------------------------------------------------------------------------
//enable_eint0/1/2 - Mode/Left/Right switches, falling edge, vectored IRQ,
//following the same enable_eintX() pattern used in EINT0_EINT1.c
//----------------------------------------------------------------------------
static void enable_eint0(void)
{
	CfgPortPinFunc(0,1,3);        //P0.1 -> EINT0 (see PINSEL0 bits 3:2 = 11)
	VICIntSelect=0<<EINT0_CH;
	VICIntEnable=1<<EINT0_CH;
	VICVectCntl0=(1<<5)|EINT0_CH;
	VICVectAddr0=(unsigned int)eint0_isr;
	SETBIT(EXTMODE,0);            //EINT0 edge sensitive
	CLRBIT(EXTPOLAR,0);           //falling edge
}

static void enable_eint1(void)
{
	CfgPortPinFunc(0,3,3);        //P0.3 -> EINT1 (see PINSEL0 bits 7:6 = 11)
	VICIntSelect=0<<EINT1_CH;
	VICIntEnable=1<<EINT1_CH;
	VICVectCntl1=(1<<5)|EINT1_CH;
	VICVectAddr1=(unsigned int)eint1_isr;
	SETBIT(EXTMODE,1);
	CLRBIT(EXTPOLAR,1);
}

static void enable_eint2(void)
{
	CfgPortPinFunc(0,7,3);        //P0.7 -> EINT2 (see PINSEL0 bits 15:14 = 11)
	VICIntSelect=0<<EINT2_CH;
	VICIntEnable=1<<EINT2_CH;
	VICVectCntl2=(1<<5)|EINT2_CH;
	VICVectAddr2=(unsigned int)eint2_isr;
	SETBIT(EXTMODE,2);
	CLRBIT(EXTPOLAR,2);
}

//----------------------------------------------------------------------------
//ISRs - simple, blocking software debounce.
//Indicator switches are only actioned while in Forward mode, per spec.
//----------------------------------------------------------------------------
void eint0_isr(void)__irq
{
	delay_ms(200);   //debounce
	vehicle_mode=(vehicle_mode==MODE_FORWARD)?MODE_REVERSE:MODE_FORWARD;
	mode_changed=1;
	left_ind_on=0; right_ind_on=0;

	EXTINT=1<<0;
	VICVectAddr=0;
}

void eint1_isr(void)__irq
{
	delay_ms(200);
	if(vehicle_mode==MODE_FORWARD)
	{
		CANF txF;
		left_ind_on=(u8)!left_ind_on;
		right_ind_on=0;

		txF.ID=CAN_ID_MODE_INDICATOR;
		txF.BFV.RTR=0; txF.BFV.DLC=1;
		txF.Data1=left_ind_on?IND_LEFT_ON:IND_OFF;
		txF.Data2=0;
		CAN1_Tx(txF);
	}
	EXTINT=1<<1;
	VICVectAddr=0;
}

void eint2_isr(void)__irq
{
	delay_ms(200);
	if(vehicle_mode==MODE_FORWARD)
	{
		CANF txF;
		right_ind_on=(u8)!right_ind_on;
		left_ind_on=0;

		txF.ID=CAN_ID_MODE_INDICATOR;
		txF.BFV.RTR=0; txF.BFV.DLC=1;
		txF.Data1=right_ind_on?IND_RIGHT_ON:IND_OFF;
		txF.Data2=0;
		CAN1_Tx(txF);
	}
	EXTINT=1<<2;
	VICVectAddr=0;
}

//----------------------------------------------------------------------------
//build_lcd_icons() - loads the 5 fuel-level icons and 2 arrow icons into
//CGRAM once at startup, then returns the cursor to line1/pos0.
//----------------------------------------------------------------------------
static void build_lcd_icons(void)
{
	BuildCGRAM(fuelIcon0,FUEL_ICON_0);
	BuildCGRAM(fuelIcon1,FUEL_ICON_1);
	BuildCGRAM(fuelIcon2,FUEL_ICON_2);
	BuildCGRAM(fuelIcon3,FUEL_ICON_3);
	BuildCGRAM(fuelIcon4,FUEL_ICON_4);
	BuildCGRAM(arrowLeft,ARROW_LEFT);
	BuildCGRAM(arrowRight,ARROW_RIGHT);
	GotoXYLCD(0,0);
}

static u8 fuel_icon_for(u8 percent)
{
	if(percent>=80) return FUEL_ICON_4;
	if(percent>=60) return FUEL_ICON_3;
	if(percent>=35) return FUEL_ICON_2;
	if(percent>=10) return FUEL_ICON_1;
	return FUEL_ICON_0;
}

//----------------------------------------------------------------------------
//main()
//----------------------------------------------------------------------------
int main(void)
{
	CANF rxF;
	s32 temp_c;
	u8  blink_on=0;
	u32 loopcnt=0;

	init_clocks();
	init_gpio();
	InitLCD();
	build_lcd_icons();
	Init_DS18B20();
	Init_CAN1();
	enable_eint0();
	enable_eint1();
	enable_eint2();

	ClearLCD();
	GotoXYLCD(0,0);
	StrLCD("Vehicle Monitor");
	delay_ms(1000);
	ClearLCD();

	while(1)
	{
		//--- announce a mode change to the Indicator & Reverse Alert Node ---
		if(mode_changed)
  		{
			CANF txF;
			txF.ID=CAN_ID_MODE_INDICATOR;
			txF.BFV.RTR=0; txF.BFV.DLC=1;
			txF.Data1=vehicle_mode;
			txF.Data2=0;
			CAN1_Tx(txF);
			mode_changed=0;
			if(vehicle_mode==MODE_REVERSE) reverse_status=REV_STATUS_SAFE;
		}

		//--- pull in Fuel% / Reverse status+distance from the CAN bus -------
		if(READBIT(C1GSR,RBS_BIT))
		{
			CAN1_Rx(&rxF);

			if(rxF.ID==CAN_ID_FUEL_LEVEL)
			{
				fuel_percent=(u8)(rxF.Data1&0xFF);
			}
			else if(rxF.ID==CAN_ID_REVERSE_STATUS)
			{
				reverse_status=(u8)(rxF.Data1&0xFF);
				reverse_dist_cm=(rxF.Data1>>8)&0xFFFF;
			}
		}

		//--- Engine temperature (blocks ~750ms for a DS18B20 conversion) ----
		temp_c=Read_DS18B20_TempC();

		//--- Row1: temperature ------------------------------------------
		GotoXYLCD(0,0);
		StrLCD("TEMP:");
		S32LCD(temp_c);
		CharLCD(0xDF);
		StrLCD(" C      ");

		//--- Row2: fuel percentage + icon --------------------------------
		GotoXYLCD(1,0);
		StrLCD("FUEL:");
		U32LCD(fuel_percent);
		StrLCD("%  ");
		CharLCD(fuel_icon_for(fuel_percent));
		StrLCD("      ");

		//--- Row3: mode / reverse distance+status ------------------------
		GotoXYLCD(2,0);
		if(vehicle_mode==MODE_FORWARD)
		{
			IOCLR0=1<<BUZZER_PIN;
			StrLCD("MODE:FWD            ");
		}
		else //MODE_REVERSE
		{
			if(reverse_status==REV_STATUS_FAULT)
			{
				IOCLR0=1<<BUZZER_PIN;
				StrLCD("REV: SENSOR FAULT   ");
			}
			else
			{
				StrLCD("REV:");
				U32LCD(reverse_dist_cm);
				StrLCD("CM ");
				switch(reverse_status)
				{
					case REV_STATUS_WARNING:
						IOCLR0=1<<BUZZER_PIN;
						StrLCD("WARNING   ");
						break;
					case REV_STATUS_STOP:
						IOSET0=1<<BUZZER_PIN;
						StrLCD("STOP!     ");
						break;
					default: //REV_STATUS_SAFE
						IOCLR0=1<<BUZZER_PIN;
						StrLCD("SAFE      ");
						break;
				}
			}
		}

		//--- Row4: L/R indicator symbols, blinking when active -----------
		blink_on=(u8)(loopcnt&0x01);   //toggles every loop pass (~1Hz blink)
		GotoXYLCD(3,0);
		StrLCD("L:");
		if(left_ind_on && blink_on) CharLCD(ARROW_LEFT); else CharLCD(' ');
		StrLCD("     R:");
		if(right_ind_on && blink_on) CharLCD(ARROW_RIGHT); else CharLCD(' ');
		StrLCD("        ");

		loopcnt++;
	}
}
