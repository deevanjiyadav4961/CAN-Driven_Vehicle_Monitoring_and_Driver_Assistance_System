//hcsr04.c
//HC-SR04 ultrasonic sensor driver (reverse obstacle detection on the
//Indicator & Reverse Alert Node).
//
//Pins:
//    TRIG : P0.16 (output) - 10us HIGH pulse starts the ranging cycle
//    ECHO : P0.17 (input)  - stays HIGH for the round-trip time of the burst
//
//Distance (cm) = echo_pulse_width(us) / 58, per the HC-SR04 datasheet.
//
//REVISION NOTE: the first version of this driver measured the echo pulse
//width by counting repeated delay_us(1) busy-loop calls. That is NOT
//accurate enough for a signal that is only tens-to-low-thousands of
//microseconds wide - loop/call overhead makes the count run fast or slow
//depending on compiler optimization, which is exactly what caused
//readings of either 0cm (loop overhead ate the whole pulse) or 65535cm /
//timeout (the transition got missed). This version uses Timer0 as a real
//free-running 1us hardware tick and time-stamps the ECHO edges directly,
//which is accurate regardless of loop overhead.
#include <LPC21xx.h>
#include "types.h"
#include "delay.h"
#include "hcsr04.h"

#define TRIG_PIN   16   //P0.16
#define ECHO_PIN   17   //P0.17

//PCLK = CCLK/4 = 15MHz here (VPBDIV left at its reset value, matching
//can_defines.h / ADC_defines.h elsewhere in this project).
#define PCLK   15000000UL

#define ECHO_START_TIMEOUT_LOOPS   200000UL   //waiting for ECHO to go HIGH
#define ECHO_END_TIMEOUT_LOOPS     400000UL   //waiting for ECHO to go LOW (~5m max range)

static void timer0_init(void)
{
	T0TCR=0x02;                       //reset Timer0
	T0PR=(PCLK/1000000UL)-1;          //prescale so T0TC increments every 1us
	T0TCR=0x01;                       //enable Timer0, free running
}

void Init_HCSR04(void)
{
	IODIR0|=1<<TRIG_PIN;    //TRIG = output
	IODIR0&=~(1<<ECHO_PIN); //ECHO = input
	IOCLR0=1<<TRIG_PIN;
	timer0_init();
}

//Returns distance in cm, or 0xFFFF if the sensor never returned an echo
//(check wiring / power / that nothing is blocking TRIG or ECHO) - the
//caller MUST treat 0xFFFF as a fault, not as "clear/far away".
u32 Read_HCSR04_DistCM(void)
{
	u32 start,end,pulse_us,waited;

	IOCLR0=1<<TRIG_PIN;
	delay_us(2);
	IOSET0=1<<TRIG_PIN;
	delay_us(10);            //>=10us trigger pulse, per datasheet
	IOCLR0=1<<TRIG_PIN;

	//wait for ECHO to go HIGH (start of the return burst)
	waited=0;
	while(!(IOPIN0&(1<<ECHO_PIN)))
	{
		if(++waited>ECHO_START_TIMEOUT_LOOPS) return 0xFFFF;   //sensor not responding
	}
	start=T0TC;

	//wait for ECHO to go LOW again (end of the return burst)
	waited=0;
	while(IOPIN0&(1<<ECHO_PIN))
	{
		if(++waited>ECHO_END_TIMEOUT_LOOPS) break;   //out of range - stop timing
	}
	end=T0TC;

	pulse_us=end-start;      //unsigned subtraction handles T0TC wrap-around correctly
	return (pulse_us/58UL);  //distance in centimeters
}
