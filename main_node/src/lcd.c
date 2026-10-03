//lcd.c
#include <LPC21xx.h>
#include "types.h"
#include "lcd_defines.h"
#include "defines.h"
#include "delay.h"
#include "lcd.h"

void WriteLCD(u8 byte)
{
	//write to data pins
	WRITEBYTE(IOPIN0,LCD_DATA,byte);
	//select write operations
	IOCLR0=1<<LCD_RW;
	//provide high to low enable pulse
	IOSET0=1<<LCD_EN;
	delay_us(1);
	IOCLR0=1<<LCD_EN;
	delay_ms(2);
}

void CmdLCD(u8 opcode)
{
	//clr rs pin for cmd register select
  IOCLR0=1<<LCD_RS;
  //write to cmd register via d0 to d7
  WriteLCD(opcode);	
}

void InitLCD(void)
{
	//cfg p0.8-p0.15,rs,rw,en as gpio out pins
	IODIR0|=((0xFF<<LCD_DATA)|
	        (1<<LCD_RS)|(1<<LCD_RW)|(1<<LCD_EN));
	
	delay_ms(15);
	CmdLCD(0x30);
	delay_ms(4);
	delay_us(100);
	CmdLCD(0x30);
	delay_us(100);
	CmdLCD(0x30);
	CmdLCD(MODE_8BIT_2LINE);
	CmdLCD(DSP_ON_CUR_OFF);
	CmdLCD(CLEAR_LCD);
	CmdLCD(SHIFT_CUR_RIGHT);
}
void CharLCD(u8 asciiVal)
{
	//set rs pin for data register select
	IOSET0=1<<LCD_RS;
	//write to ddram via data reg via data pins
	WriteLCD(asciiVal);
}

void StrLCD(s8 *str)
{
	while(*str)
		CharLCD(*str++);
}

void U32LCD(u32 num)
{
  u8 a[10];
  s32 i=0;
  if(num==0)
  {
		CharLCD('0');
  }		
	else
	{
		while(num>0)
		{
			a[i++]=(num%10)+48;
			num/=10;
		}
		for(--i;i>=0;i--)
		 CharLCD(a[i]);
	}
}

//---- additions for this project (20x4 panel) ---------------------------

//S32LCD - prints a signed decimal integer (e.g. engine temperature,
//which the DS18B20 can report below 0 C, or a -999 sensor-fault code)
void S32LCD(s32 num)
{
	if(num<0)
	{
		CharLCD('-');
		num=-num;
	}
	U32LCD((u32)num);
}

//BuildCGRAM - loads an 8-byte 5x8 pixel pattern into one of the 8 CGRAM
//custom character slots (location = 0..7). After calling this, the
//cursor is left inside CGRAM address space, so GotoXYLCD()/CmdLCD() must
//be used again before writing further text to the visible display.
void BuildCGRAM(s8 *pattern, u32 location)
{
	u8 i;
	CmdLCD(GOTO_CGRAM_START+((u8)location*8));
	for(i=0;i<8;i++)
		CharLCD((u8)pattern[i]);
}

//ClearLCD - clears the display and returns the cursor to line1/pos0
void ClearLCD(void)
{
	CmdLCD(CLEAR_LCD);
	delay_ms(2);
}

//GotoXYLCD - positions the cursor on a 20x4 panel (row = 0..3, col = 0..19)
void GotoXYLCD(u8 row, u8 col)
{
	u8 addr;
	switch(row)
	{
		case 0:  addr=GOTO_LINE1_POS0+col; break;
		case 1:  addr=GOTO_LINE2_POS0+col; break;
		case 2:  addr=GOTO_LINE3_POS0+col; break;
		default: addr=GOTO_LINE4_POS0+col; break;
	}
	CmdLCD(addr);
}
