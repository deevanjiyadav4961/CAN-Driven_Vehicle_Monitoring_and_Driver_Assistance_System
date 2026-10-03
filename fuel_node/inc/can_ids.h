//can_ids.h
//Shared CAN message map for the CAN-Driven Vehicle Monitoring and Driver
//Assistance System. Used together with the CANF struct from can.h - the
//values below go into CANF.ID and the low byte of CANF.Data1.
//This SAME file must be used, unmodified, on all three nodes.
#ifndef __CAN_IDS_H__
#define __CAN_IDS_H__

//---- CAN Standard (11-bit) Identifiers ------------------------------------
#define CAN_ID_FUEL_LEVEL        0x100   //Fuel Node -> Main Node : Data1 low byte = fuel %
#define CAN_ID_MODE_INDICATOR    0x200   //Main Node -> Indicator/Reverse Node : Data1 low byte = command
#define CAN_ID_REVERSE_STATUS    0x300   //Indicator/Reverse Node -> Main Node : Data1 low byte = status

//---- Command bytes on CAN_ID_MODE_INDICATOR -------------------------------
#define MODE_FORWARD    0x01   //vehicle mode changed to Forward
#define MODE_REVERSE    0x02   //vehicle mode changed to Reverse
#define IND_OFF         0x10   //indicators OFF
#define IND_LEFT_ON     0x11   //Left indicator ON
#define IND_RIGHT_ON    0x12   //Right indicator ON

//---- Status bytes on CAN_ID_REVERSE_STATUS ---------------------------------
#define REV_STATUS_SAFE      0x01
#define REV_STATUS_WARNING   0x02
#define REV_STATUS_STOP      0x03

#endif
