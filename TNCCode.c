/*
Copyright 2001-2015 John Wiseman G8BPQ

This file is part of LinBPQ/BPQ32.

LinBPQ/BPQ32 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

LinBPQ/BPQ32 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with LinBPQ/BPQ32.  If not, see http://www.gnu.org/licenses
*/	

//
//	C replacement for TNCCode.asm
//
#define Kernel

#define _CRT_SECURE_NO_DEPRECATE 
#define _USE_32BIT_TIME_T

#pragma data_seg("_BPQDATA")

#include "time.h"
#include "stdio.h"
#include <fcntl.h>

#include "CHeaders.h"

int C_Q_COUNT(VOID *PQ);
VOID SENDUIMESSAGE(struct DATAMESSAGE * Msg);

VOID TNCTimerProc()
{
	//	CALLED AT 10 HZ

	int n = BPQHOSTSTREAMS;
	PBPQVECSTRUC HOSTSESS = BPQHOSTVECTOR;
	TRANSPORTENTRY * Session;
	UCHAR DISCFLAG = 0;

	while (n--)
	{
		//	Action any DISC Requests (must be done in timer owning process)

		if (HOSTSESS->HOSTFLAGS & 0x40)		// DISC REQUEST
		{
			if (HOSTSESS->HOSTFLAGS & 0x20)	// Stay?
				DISCFLAG = 'S';

			HOSTSESS->HOSTFLAGS &= 0x9F;		// Clear Flags

			Session = HOSTSESS->HOSTSESSION;

			if (Session == 0)					// Gone??
			{
				HOSTSESS->HOSTFLAGS |= 3;		//  STATE CHANGE
#ifndef LINBPQ
				if (HOSTSESS->HOSTHANDLE);
				{
					PostMessage(HOSTSESS->HOSTHANDLE, BPQMsg, HOSTSESS->HOSTSTREAM, 4);
				}
#endif
				continue;
			}
		
			if (Session->L4CROSSLINK)
				Session->L4CROSSLINK->STAYFLAG = DISCFLAG;

			HOSTSESS->HOSTSESSION = 0;
			HOSTSESS->HOSTFLAGS |= 3;		//  STATE CHANGE

			PostStateChange(Session);

			CloseSessionPartner(Session);	// SEND CLOSE TO PARTNER (IF PRESENT)
		}
	
		// Check Trace Q

		if (HOSTSESS->HOSTAPPLFLAGS & 0x80)
		{
			if (HOSTSESS->HOSTTRACEQ)
			{
				int Count = C_Q_COUNT(&HOSTSESS->HOSTTRACEQ);

				if (Count > 100)
						ReleaseBuffer(Q_REM(&HOSTSESS->HOSTTRACEQ));
			}
		}
		HOSTSESS++;
	}
}


VOID SENDIDMSG()
{
	struct PORTCONTROL * PORT = PORTTABLE;
	struct _MESSAGE * ID = IDMSG;
	struct _MESSAGE * Buffer;

	while (PORT)
	{
		if (PORT->PROTOCOL < 10)			// Not Pactor-style
		{
			Buffer = GetBuff();
		
			if (Buffer)
			{
				memcpy(Buffer, ID, ID->LENGTH);
			
				Buffer->PORT = PORT->PORTNUMBER;

				//	IF PORT HAS A CALLSIGN DEFINED, SEND THAT INSTEAD

				if (PORT->PORTCALL[0] > 0x40)
				{
					memcpy(Buffer->ORIGIN, PORT->PORTCALL, 7);
					Buffer->ORIGIN[6] |= 1;		// SET END OF CALL BIT
				}
				C_Q_ADD(&IDMSG_Q, Buffer);
			}
		}
		PORT = PORT->PORTPOINTER;
	}
}



VOID SENDBTMSG()
{
	struct PORTCONTROL * PORT = PORTTABLE;
	struct _MESSAGE * Buffer;
	char * ptr1, * ptr2;

	while (PORT)
	{
		if (PORT->PROTOCOL >= 10 || PORT->PORTUNPROTO == 0)	// Pactor-style or no UNPROTO ADDR?
		{
			PORT = PORT->PORTPOINTER;
			continue;
		}
		
		Buffer = GetBuff();

		if (Buffer)
		{
			memcpy(Buffer->DEST, PORT->PORTUNPROTO, 7);
			Buffer->DEST[6] |= 0xC0;		// Set COmmand bits

			//	Send from BBSCALL unless PORTBCALL defined

			if (PORT->PORTBCALL[0] > 32)
				memcpy(Buffer->ORIGIN, PORT->PORTBCALL, 7);
			else if (APPLCALLTABLE->APPLCALL[0] > 32) 
				memcpy(Buffer->ORIGIN, APPLCALLTABLE->APPLCALL, 7);
			else
				memcpy(Buffer->ORIGIN, MYCALL, 7);

			ptr1 = &PORT->PORTUNPROTO[7];		// First Digi
			ptr2 = &Buffer->CTL;				// Digi field in buffer

			// Copy any digis

			while (*(ptr1))
			{
				memcpy(ptr2, ptr1, 7);
				ptr1 += 7;
				ptr2 += 7;
			}

			*(ptr2 - 1) |= 1;					// Set End of Address
			*(ptr2++) = UI;
	
			memcpy(ptr2, &BTHDDR.PID, BTHDDR.LENGTH);
			ptr2 += BTHDDR.LENGTH;
			Buffer->LENGTH = ptr2 - (char *)Buffer;			
			Buffer->PORT = PORT->PORTNUMBER;
 	
			C_Q_ADD(&IDMSG_Q, Buffer);
		}
		PORT = PORT->PORTPOINTER;
	}
}

VOID SENDUIMESSAGE(struct DATAMESSAGE * Msg)
{
	struct PORTCONTROL * PORT = PORTTABLE;
	struct _MESSAGE * Buffer;
	char * ptr1, * ptr2;

	Msg->LENGTH -= 7;	// Remove Header

	while (PORT)
	{
		if (PORT->PROTOCOL >= 10 || PORT->PORTUNPROTO == 0)	// Pactor-style or no UNPROTO ADDR?
		{
			PORT = PORT->PORTPOINTER;
			continue;
		}

		Buffer = GetBuff();

		if (Buffer)
		{
			memcpy(Buffer->DEST, PORT->PORTUNPROTO, 7);
			Buffer->DEST[6] |= 0xC0;		// Set Command bits

			//	Send from BBSCALL unless PORTBCALL defined

			if (PORT->PORTBCALL[0] > 32)
				memcpy(Buffer->ORIGIN, PORT->PORTBCALL, 7);
			else if (APPLCALLTABLE->APPLCALL[0] > 32) 
				memcpy(Buffer->ORIGIN, APPLCALLTABLE->APPLCALL, 7);
			else
				memcpy(Buffer->ORIGIN, MYCALL, 7);

			ptr1  = &PORT->PORTUNPROTO[7];		// First Digi
			ptr2 = &Buffer->CTL;				// Digi field in buffer

			// Copy any digis

			while (*(ptr1))
			{
				memcpy(ptr2, ptr1, 7);
				ptr1 += 7;
				ptr2 += 7;
			}

			*(ptr2 - 1) |= 1;					// Set End of Address
			*(ptr2++) = UI;
	
			memcpy(ptr2, &Msg->PID, Msg->LENGTH);
			ptr2 += Msg->LENGTH;
			Buffer->LENGTH = ptr2 - (char *)Buffer;			
			Buffer->PORT = PORT->PORTNUMBER;
 	
			C_Q_ADD(&IDMSG_Q, Buffer);
		}
		PORT = PORT->PORTPOINTER;
	}
}

Dll VOID APIENTRY Send_AX(UCHAR * Block, DWORD Len, UCHAR Port)
{
	// Block included the 7 byte header, Len does not

	struct PORTCONTROL * PORT;
	PMESSAGE Copy;

	if (Len > 360 - 15)
		return;

	if (QCOUNT < 50)
		return;				// Running low

	PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == 0)
		return;

	Copy = GetBuff();

	if (Copy == 0)
		return;

	memcpy(&Copy->DEST[0], &Block[7], Len);

	Copy->LENGTH = (USHORT)Len + 7;

	if (PORT->PROTOCOL == 10)
	{
		// 	Pactor Style. Probably will only be used for Tracker uneless we do APRS over V4 or WINMOR

		EXTPORTDATA * EXTPORT = (EXTPORTDATA *) PORT;

		C_Q_ADD(&EXTPORT->UI_Q, Copy);
		return;
	}

	Copy->PORT = Port; 

	PUT_ON_PORT_Q(PORT, Copy);
}


TRANSPORTENTRY * SetupSessionFromHost(PBPQVECSTRUC HOST, UINT ApplMask)
{
	// Create a Transport (L4) session linked to an incoming HOST (API) Session

	TRANSPORTENTRY * NewSess = L4TABLE;
	int Index = 0;

	
	while (Index < MAXCIRCUITS)
	{
		if (NewSess->L4USER[0] == 0)
		{
			// Got One

			UCHAR * ourcall = &MYCALL[0];
		
			// IF APPL PORT USE APPL CALL, ELSE NODE CALL

			if (ApplMask)
			{
				// Circuit for APPL - look for an APPLCALL

				APPLCALLS * APPL = APPLCALLTABLE;

				while ((ApplMask & 1) == 0)
				{
					ApplMask >>= 1;
					APPL++;
				}
				if (APPL->APPLCALL[0] > 0x40)		// We have an applcall
					ourcall = &APPL->APPLCALL[0];
			}

			memcpy(NewSess->L4USER, ourcall, 7);
			memcpy(NewSess->L4MYCALL, ourcall, 7);

			NewSess->CIRCUITINDEX = Index;				//OUR INDEX
			NewSess->CIRCUITID = NEXTID;

			NEXTID++;
			if (NEXTID == 0)
				NEXTID++;								// Keep Non-Zero

			NewSess->L4TARGET.HOST = HOST;
			NewSess->L4STATE = 5;

			
			NewSess->SESSIONT1 = L4T1;
			NewSess->L4WINDOW = (UCHAR)L4DEFAULTWINDOW;
			NewSess->SESSPACLEN = PACLEN;				// Default;

			return NewSess;
			}
		Index++;
		NewSess++;
	}

	// Table Full

	return NULL;
}




