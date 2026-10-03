//ds18b20.h
#include "types.h"
void Init_DS18B20(void);
s32  Read_DS18B20_TempC(void);   //returns engine temperature in whole degrees C,
                                  //or -999 if no sensor responded (wiring fault)
