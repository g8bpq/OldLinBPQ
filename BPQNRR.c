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
//	Netrom Record ROute Suport Code for BPQ32 Switch
//

//	All code runs from the BPQ32 Received or Timer Routines under Semaphore.
//	As most data areas are dynamically allocated, they will not survive a Timer Process Swap.
//	Shared data can be used for Config Info.


#define _CRT_SECURE_NO_DEPRECATE 
#define _USE_32BIT_TIME_T

#pragma data_seg("_BPQDATA")

#include "time.h"
#include "stdio.h"
#include <fcntl.h>					 
//#include "vmm.h"


#include "CHeaders.h"


extern int SENDNETFRAME();
extern VOID Q_ADD();

VOID __cdecl Debugprintf(const char * format, ...);

TRANSPORTENTRY * NRRSession;

/*
datagrams (and other things) to be transported in Netrom L3 frames. 
When the frametype is 0x00, the "circuit index" and "circuit id" (first 2 
bytes of the transport header) take on a different meaning, something like 
"protocol family" and "protocol id". IP over netrom uses 0x0C for both 
bytes, TheNet uses 0x00 for both bytes when making L3RTT measurements, and 
Xnet uses family 0x00, protocol id 0x01 for Netrom Record Route. I believe 
there are authors using other values too. Unfortunately there is no 
co-ordinating authority for these numbers, so authors just pick an unused 
one. 
*/

VOID NRRecordRoute(char * Buff, int Len)
{
	// NRR frame for us. If We originated it, report outcome, else put our call on end, and send back
	
	L3MESSAGEBUFFER * Msg = (L3MESSAGEBUFFER *)Buff;
	struct DEST_LIST * DEST;
	char Temp[7];
	int NRRLen = Len - 28;
	UCHAR Flags;
	char call[10];
	int calllen;
	char * Save = Buff;

	if (memcmp(&Buff[28], MYCALL, 7) == 0)
	{
		UCHAR * BUFFER = GetBuff();
		UCHAR * ptr1;
		struct _MESSAGE * Msg;

		if (BUFFER == NULL)
			return;

		ptr1 = &BUFFER[7];
		
		*ptr1++ = 0xf0;			// PID

		ptr1 += sprintf(ptr1, "NRR Response:");

		Buff += 28;
		Len -= 28;

		while (Len > 0)
		{
			calllen = ConvFromAX25(Buff, call);
			call[calllen] = 0;
			ptr1 += sprintf(ptr1, " %s", call);
			if ((Buff[7] & 0x80) == 0x80)			// Check turnround bit
				*ptr1++ = '*';
	
			Buff+=8;
			Len -= 8;
		}

		// Add ours on end for neatness

		calllen = ConvFromAX25(MYCALL, call);
		call[calllen] = 0;
		ptr1 += sprintf(ptr1, " %s", call);

		*ptr1++ = 0x0d;			// CR

		Len = ptr1 - BUFFER;

		Msg = (struct _MESSAGE *)BUFFER;
		
		Msg->LENGTH = Len;

		Msg->CHAIN = NULL;

		C_Q_ADD(&NRRSession->L4TX_Q, (UINT *)BUFFER);

		PostDataAvailable(NRRSession);

		ReleaseBuffer(Save);

		return;
	}

	// Add our call on end, and increase count

	Flags = Buff[Len -1];

	Flags--;

	if (Flags && NRRLen < 228)					// Dont update if full
	{
		Flags |= 0x80;			// Set End of route bit

		Msg->L3PID = NRPID;

		memcpy(&Msg->L4DATA[NRRLen], MYCALL, 7);
		Msg->L4DATA[NRRLen+7] = Flags;
		NRRLen += 8;
	}

	// We should send it back via our bast route, or recorded route could be wrong

	memcpy(Temp, Msg->L3DEST, 7);
	memcpy(Msg->L3DEST, Msg->L3SRCE, 7);
	memcpy(Msg->L3SRCE, Temp, 7);

	if (FindDestination(Msg->L3DEST, &DEST) == 0)
	{
		ReleaseBuffer(Msg);			// CANT FIND DESTINATION
		return;
	}
		
	Msg->LENGTH = NRRLen + 20 + 8;
	
	C_Q_ADD(&DEST->DEST_Q, Msg);
}

	
VOID SendNRRecordRoute(struct DEST_LIST * DEST, TRANSPORTENTRY * Session)
{	
	L3MESSAGEBUFFER * Msg = GetBuff();
	int Stream = 1;

	if (Msg == NULL)
		return;

	NRRSession = Session;			// Save Session Pointer for reply

	Msg->Port = 0;
	Msg->L3PID = NRPID;

	memcpy(Msg->L3DEST, DEST->DEST_CALL, 7);
	memcpy(Msg->L3SRCE, MYCALL, 7);
		
	Msg->L3TTL = L3LIVES;
	Msg->L4ID = 1;
	Msg->L4INDEX = 0;
	Msg->L4FLAGS = 0;

	memcpy(Msg->L4DATA, MYCALL, 7);
	Msg->L4DATA[7] = Stream + 28;
		
	Msg->LENGTH = 8 + 20 + 8;
	
	C_Q_ADD(&DEST->DEST_Q, Msg);
}
