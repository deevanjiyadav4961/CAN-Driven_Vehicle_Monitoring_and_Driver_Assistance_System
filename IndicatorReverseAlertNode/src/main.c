//main.c  --  INDICATOR & REVERSE ALERT NODE
//CAN-Driven Vehicle Monitoring and Driver Assistance System
//MCU: NXP LPC2129 (ARM7TDMI-S) | Compiler: Keil uVision / ARM C
//
//Forward mode : scrolls the 8 indicator LEDs left/right per commands from
//               the Main Node. Ultrasonic sensing is disabled.
//Reverse mode : continuously measures obstacle distance with the HC-SR04,
//               drives the buzzer/STOP LED, and reports distance+status
//               back to the Main Node over CAN (CAN_ID_REVERSE_STATUS:
//               Data1 bits[7:0]=status, bits[23:8]=distance in cm).
//
//---------------------------------------------------------------------------
//PIN MAP (edit here if your board is wired differently)
//---------------------------------------------------------------------------
//  8 Indicator LEDs         : P0.0 - P0.7
//  Reverse Alert (STOP) LED : P0.19
//  Buzzer                   : P0.20
//  HC-SR04 TRIG / ECHO      : P0.16 / P0.17
//  CAN1 (to MCP2551)        : TD1 = dedicated pin, RD1 = P0.25
//---------------------------------------------------------------------------
//Obstacle distance thresholds (edit to match your test setup):
//    >  SAFE_DIST_CM               -> SAFE     (buzzer OFF)
//    WARN_DIST_CM..SAFE_DIST_CM    -> WARNING  (intermittent buzzer)
//    <  WARN_DIST_CM               -> STOP     (continuous buzzer, LED ON)
//----------------------------------------------------------------------------
#include <LPC21xx.h>
#include "types.h"
#include "defines.h"
#include "delay.h"
#include "can_defines.h"
#include "can.h"
#include "can_ids.h"
#include "hcsr04.h"

#define LED_MASK        0x000000FFUL   //P0.0 - P0.7 : 8 indicator LEDs
#define STOP_LED_PIN    19              //P0.19 : Reverse Alert LED
#define BUZZER_PIN      20              //P0.20

#define SAFE_DIST_CM     100U   //> 100cm  : SAFE
#define WARN_DIST_CM      40U   //40-100cm : WARNING
                                  //< 40cm   : STOP

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
	//VPBDIV left at reset value 0 -> PCLK = CCLK/4 = 15MHz, matching can_defines.h
}

static void init_gpio(void)
{
	IODIR0|=LED_MASK;
	IOCLR0=LED_MASK;

	IODIR0|=(1<<STOP_LED_PIN)|(1<<BUZZER_PIN);
	IOCLR0=(1<<STOP_LED_PIN)|(1<<BUZZER_PIN);
}

static void leds_write(u8 pattern)
{
	IOCLR0=LED_MASK;
	IOSET0=((u32)pattern);
}

static void leds_off(void){ leds_write(0x00); }

//Steps the indicator LED animation by one frame per call.
//left=1 -> "scroll right to left" (Left indicator)
//left=0 -> "scroll left to right" (Right indicator)
static void indicator_animate(u8 left)
{
	static s8 pos_l=7;
	static u8 pos_r=0;

	if(left)
	{
		leds_write((u8)(1U<<pos_l));
		pos_l--;
		if(pos_l<0) pos_l=7;
	}
	else
	{
		leds_write((u8)(1U<<pos_r));
		pos_r++;
		if(pos_r>7) pos_r=0;
	}
	delay_ms(120);
}

int main(void)
{
	CANF rxF, txF;		                                                                                                                               
	u8 current_mode=MODE_FORWARD;
	u8 indicator_cmd=IND_OFF;
	u8 buzz_toggle=0;

	init_clocks();
	init_gpio();
	Init_HCSR04();
	Init_CAN1();

	while(1)
	{
		//--- receive Mode / Indicator commands from the Main Node ----------
		if(READBIT(C1GSR,RBS_BIT))
		{
			CAN1_Rx(&rxF);

			if(rxF.ID==CAN_ID_MODE_INDICATOR)
			{
				u8 cmd=(u8)(rxF.Data1&0xFF);
				switch(cmd)
				{
					case MODE_FORWARD:
					case MODE_REVERSE:
						current_mode=cmd;
						leds_off();
						IOCLR0=(1<<STOP_LED_PIN)|(1<<BUZZER_PIN);
						indicator_cmd=IND_OFF;
						break;

					case IND_LEFT_ON:
					case IND_RIGHT_ON:
					case IND_OFF:
						indicator_cmd=cmd;
						if(indicator_cmd==IND_OFF) leds_off();
						break;

					default:
						break;   //unknown command, ignore
				}
			}
		}

		if(current_mode==MODE_FORWARD)
		{
			//--- normal indicator operation, ultrasonic sensing disabled ---
			if(indicator_cmd==IND_LEFT_ON)       indicator_animate(1);
			else if(indicator_cmd==IND_RIGHT_ON) indicator_animate(0);
			//IND_OFF: LEDs already cleared above, nothing else to do
		}
		else //MODE_REVERSE
		{
			u32 distance_cm=Read_HCSR04_DistCM();
			u8 status;

			if(distance_cm==0xFFFF)
			{
				//sensor gave no echo at all - this is a FAULT, not "clear/far
				//away". Check ECHO/TRIG wiring, sensor power, and that
				//nothing is blocking the sensor face.
				status=REV_STATUS_FAULT;
				IOCLR0=1<<STOP_LED_PIN;
				buzz_toggle=(u8)!buzz_toggle;   //slow intermittent buzzer = fault alert
				if(buzz_toggle) IOSET0=1<<BUZZER_PIN; else IOCLR0=1<<BUZZER_PIN;
				distance_cm=0;   //don't forward the raw 65535 sentinel as a "distance"
			}
			else if(distance_cm>SAFE_DIST_CM)
			{
				status=REV_STATUS_SAFE;
				IOCLR0=(1<<STOP_LED_PIN)|(1<<BUZZER_PIN);
			}
			else if(distance_cm>WARN_DIST_CM)
			{
				status=REV_STATUS_WARNING;
				IOCLR0=1<<STOP_LED_PIN;
				buzz_toggle=(u8)!buzz_toggle;   //intermittent buzzer
				if(buzz_toggle) IOSET0=1<<BUZZER_PIN; else IOCLR0=1<<BUZZER_PIN;
			}
			else
			{
				status=REV_STATUS_STOP;
				IOSET0=(1<<STOP_LED_PIN)|(1<<BUZZER_PIN);  //continuous buzzer+LED
			}

			if(distance_cm>0xFFFF) distance_cm=0xFFFF;

			txF.ID=CAN_ID_REVERSE_STATUS;
			txF.BFV.RTR=0; txF.BFV.DLC=3;
			txF.Data1=((u32)status)|((distance_cm&0xFFFF)<<8);
			txF.Data2=0;
			CAN1_Tx(txF);

			delay_ms(150);   //obstacle re-check / buzzer toggle rate
		}
	}
}
