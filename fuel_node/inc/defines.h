//defines.h
#ifndef __DEFINES_H__
#define __DEFINES_H__
#include "types.h"

#define SETBIT(REG,BIT)         ((REG)|=((u32)1<<(BIT)))
#define CLRBIT(REG,BIT)         ((REG)&=~((u32)1<<(BIT)))
#define TOGGLEBIT(REG,BIT)      ((REG)^=((u32)1<<(BIT)))
#define READBIT(REG,BIT)        (((REG)>>(BIT))&1)
#define READNIBBLE(REG,BIT)     (((REG)>>(BIT))&0xF)
#define READBYTE(REG,BIT)       (((REG)>>(BIT))&0xFF)
#define WRITEBYTE(REG,BIT,VAL)  ((REG)=((REG)&~((u32)0xFF<<(BIT)))|((u32)(VAL)<<(BIT)))

#endif
