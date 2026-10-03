//hcsr04.h
#include "types.h"
void Init_HCSR04(void);
u32  Read_HCSR04_DistCM(void);   //returns 0xFFFF if no echo (out of range/no sensor)
