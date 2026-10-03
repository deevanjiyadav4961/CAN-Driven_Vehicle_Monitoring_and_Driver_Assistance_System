//ds18b20.c
//Bit-banged 1-Wire driver for the DS18B20 digital thermometer (Engine
//Temperature sensor on the Main Node).
//
//Pin: DQ = P0.11 (GPIO, open-drain style: driven low to pull the bus low,
//     released to input to let the external 4.7k pull-up bring it high).
//     Requires an external 4.7k resistor from DQ to VDD (3.0V - 5.5V).
//
//Only a single DS18B20 is assumed on the bus (Skip ROM [0xCC] is used).
#include <LPC21xx.h>
#include "types.h"
#include "defines.h"
#include "delay.h"
#include "ds18b20.h"

#define DQ_PIN   20   //P0.20

static void dq_release(void)
{
	CLRBIT(IODIR0,DQ_PIN);   //DQ = input, external pull-up brings it high
}

static u8 dq_read(void)
{
	return (u8)READBIT(IOPIN0,DQ_PIN);
}

void Init_DS18B20(void)
{
	dq_release();
}

static u8 ds_reset(void)
{
	u8 presence;

	SETBIT(IODIR0,DQ_PIN);
	IOCLR0=1<<DQ_PIN;
	delay_us(480);        //reset pulse, min 480us
	dq_release();
	delay_us(60);          //wait inside the 15-60us presence window
	presence=dq_read()?0:1; //sensor pulls DQ low = presence detected
	delay_us(420);         //remainder of the 480us presence/reset slot
	return presence;
}

static void ds_write_bit(u8 bit)
{
	SETBIT(IODIR0,DQ_PIN);
	IOCLR0=1<<DQ_PIN;
	if(bit){ delay_us(2);  dq_release(); delay_us(58); }
	else   { delay_us(60); dq_release(); delay_us(2);  }
}

static u8 ds_read_bit(void)
{
	u8 bit;
	SETBIT(IODIR0,DQ_PIN);
	IOCLR0=1<<DQ_PIN;
	delay_us(2);
	dq_release();
	delay_us(8);
	bit=dq_read();
	delay_us(50);
	return bit;
}

static void ds_write_byte(u8 byte)
{
	u8 i;
	for(i=0;i<8;i++){ ds_write_bit(byte&0x01); byte>>=1; }
}

static u8 ds_read_byte(void)
{
	u8 i,byte=0;
	for(i=0;i<8;i++) byte|=(u8)(ds_read_bit()<<i);
	return byte;
}

s32 Read_DS18B20_TempC(void)
{
	u8 lsb,msb;
	s32 raw;

	if(!ds_reset()) return -999;   //sensor not detected - check wiring

	ds_write_byte(0xCC);   //Skip ROM
	ds_write_byte(0x44);   //Convert T
	delay_ms(750);          //max conversion time @ 12-bit (factory default) resolution

	ds_reset();
	ds_write_byte(0xCC);   //Skip ROM
	ds_write_byte(0xBE);   //Read Scratchpad

	lsb=ds_read_byte();
	msb=ds_read_byte();

	raw=(s32)((msb<<8)|lsb);
	return raw/16;          //12-bit resolution -> 16 counts per degree C
}
