//can.c
#include <LPC21xx.h>
#include "types.h"
#include "can_defines.h"
#include "defines.h"
#include "can.h"

void Init_CAN1(void)
{
    //cfg p0.25 as CAN1_RX pin(RD1)
	  PINSEL1&=(u32)~3<<((25-16)*2);
    PINSEL1|=RD1_PIN_0_25;   
    //Reset CAN1 controller
    SETBIT(C1MOD,RM_BIT);		
    //all received messages are accepted
    CLRBIT(AFMR,ACC_Off_BIT);
	  SETBIT(AFMR,ACC_BP_BIT);       
    //Set baud Rate for CAN
    C1BTR=BTR_LVAL;
    //Enable CAN1 controller for tx/rx
    CLRBIT(C1MOD,RM_BIT);					
}

void CAN1_Tx(CANF txF)
{		
   // Checking that the TX buffer is empty
   while(READBIT(C1GSR,TBS1_BIT)==0);
   // Cfg Tx ID
   C1TID1=txF.ID;
   // Cfg RTR & DLC	
   C1TFI1=((txF.BFV.RTR<<RTR_BIT)|
           (txF.BFV.DLC<<DLC_BITS));
    //Check whether Data Frame/
    // Remote Frame to Transmit
     if(txF.BFV.RTR!=1)
     {	
     //if data frame,write to data tx buffers
	   C1TDA1= txF.Data1; /*bytes 4-1 */
	   C1TDB1= txF.Data2; /*bytes 8-5 */
     }
    //Select Tx Buff 1 & Start Xmission
    C1CMR|=1<<STB1_BIT|1<<TR_BIT;
    //wait until tx complete
    while(READBIT(C1GSR,TCS1_BIT)==0); 
}

void CAN1_Rx(CANF *rxF)
{
  //wait for CAN frame recv status
  while(READBIT(C1GSR,RBS_BIT)==0);
  //read 11-bit CANid of recvd frame.
  rxF->ID=C1RID; 
  //& read & extract data/remote frame status
  rxF->BFV.RTR=READBIT(C1RFS,RTR_BIT);
  //& extract data length
  rxF->BFV.DLC=READNIBBLE(C1RFS,DLC_BITS);
  //check if recvd frame is data frame,
  if(rxF->BFV.RTR==0)
  {	
    //extract data bytes 1-4
    rxF->Data1=C1RDA;
    //extract data bytes 5-8
    rxF->Data2=C1RDB;
  }
  // Release receive buffer command
  SETBIT(C1CMR,RRB_BIT);    
}
