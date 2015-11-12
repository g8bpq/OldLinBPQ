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
//	C replacement for L3Code.asm
//
#define Kernel

#define _CRT_SECURE_NO_DEPRECATE 
#define _USE_32BIT_TIME_T

#pragma data_seg("_BPQDATA")

#include "time.h"
#include "stdio.h"
#include <fcntl.h>

#include "CHeaders.h"

VOID UPDATEDESTLIST();
VOID MOVEALL(dest_list * DEST);
VOID MOVE3TO2(dest_list * DEST);
VOID CLEARTHIRD(dest_list * DEST);
VOID L3TRYNEXTDEST(struct ROUTE * ROUTE);


extern BOOL NODESINPROGRESS ;;
PPORTCONTROL L3CURRENTPORT;
extern dest_list * CURRENTNODE;

int L3_10SECS = 10;


VOID L3BG()
{
	//	TRANSFER MESSAGES FROM DEST TO LINK

	int n = MAXDESTS;
	struct DEST_LIST * DEST = DESTS;		// NODE LIST
	struct PORTCONTROL * PORT = PORTTABLE;
	struct ROUTE * ROUTE;

	struct _LINKTABLE * LINK;

	while (n--)
	{
		if (DEST->DEST_CALL[0])				// Active entry?
		{
			while(DEST->DEST_Q)				// FRAMES TO SEND?
			{
				int ActiveRoute = DEST->DEST_ROUTE;

				if (ActiveRoute)
				{
					ROUTE = DEST->NRROUTE[ActiveRoute - 1].ROUT_NEIGHBOUR;
					if (ROUTE)
						LINK = ROUTE->NEIGHBOUR_LINK;
					else
						LINK= NULL;

					if (LINK)
					{
						if (LINK->L2STATE == 0)
						{
							//	LINK ENTRY IS INVALID - IT PROBABLY HAS BEEN 'ZAPPED', SO CANCEL IT
							
							ROUTE->NEIGHBOUR_LINK = NULL;
						}
						else
						{
							if (LINK->L2STATE < 5)
							{
								goto NextDest;			// Wait for it to activate
							}
							else
							{
								ROUTE->NBOUR_IFRAMES++;
								C_Q_ADD(&LINK->TX_Q, Q_REM(&DEST->DEST_Q));
								continue;				// See if more
							}
						}
					}
					// Drop through to Activate
				}
				
				if (ACTIVATE_DEST(DEST) == FALSE)
				{
					// Node has no routes - get rid of it

					REMOVENODE(DEST);
					return;					// Avoid riskof looking at lod entries
				}
			}
		}
		
NextDest:
		DEST++;
	}
}

BOOL ACTIVATE_DEST(struct DEST_LIST * DEST)
{
	int n = MAXDESTS;
	struct PORTCONTROL * PORT = PORTTABLE;
	struct ROUTE * ROUTE;
	struct _LINKTABLE * LINK;

	int ActiveRoute;

	if (DEST->DEST_ROUTE == 0)		// ALREADY HAVE A SELECTED ROUTE?
		DEST->DEST_ROUTE = 1;		// TRY TO ACTIVATE FIRST

	ActiveRoute = DEST->DEST_ROUTE - 1;

	ROUTE = DEST->NRROUTE[ActiveRoute].ROUT_NEIGHBOUR;

	if (ROUTE == 0)
	{
		//	Currnet Route not present
		//	If  current route is 1, then we must have INP3 routes (or entry is corrupt)

		if (DEST->DEST_ROUTE != 1)
			goto NOROUTETODEST;

		// Current Route is 1

		if (DEST->ROUTE[0].ROUT_NEIGHBOUR == 0)
			return FALSE;					// No INP3 so No Routes

		DEST->DEST_ROUTE = 4;			// First INP3
		ROUTE = DEST->ROUTE[0].ROUT_NEIGHBOUR;
	}

	LINK = ROUTE->NEIGHBOUR_LINK; 
	
	if (LINK == 0)
	{
		// Need to Activate Link
	
		// SET UP LINK TABLE ENTRY

		return L2SETUPCROSSLINK(ROUTE);
	}
	
	// We mst be waiting for link to come up
	
	return TRUE;

NOROUTETODEST:

	//	CURRENT NEIGHBOUR NOT DEFINED - RESET TO USE FIRST

	if (DEST->DEST_ROUTE == 1)
		return FALSE;				// First not defined  so give up
	
	DEST->DEST_ROUTE = 0;
	return TRUE;
}

//	PUBLIC _PROCESSUZ7HONODEMESSAGE

//_PROCESSUZ7HONODEMESSAGE:

//	MOV _SAVEPORT,AL

VOID PROCESSNODEMESSAGE(MESSAGE * Msg, struct PORTCONTROL * PORT)
{
	//	PROCESS A NET/ROM 'NODES' MESSAGE

	//	    UPDATE _NEIGHBOURS LIST WITH ORIGINATING CALL, AND
	//	    DESTINATION LIST WITH ANY PRACTICAL ROUTES

	struct DEST_LIST * DEST;
	struct ROUTE * ROUTE;
	int Portno = PORT->PORTNUMBER;
	UINT Stamp, HH, MM;
	int Msglen = Msg->LENGTH;
	int n;
	UCHAR * ptr1, * ptr2, * saveptr;
	int Qual;
	APPLCALLS * APPL;
	int App;


	if (PORT->PORTQUALITY == 0 || PORT->INP3ONLY)
		return;

	//	SEE IF OUR CALL - DONT WANT TO PUT IT IN LIST!

	if (CompareCalls(Msg->ORIGIN, NETROMCALL))
		return;
	
	for (App = 0; App < NumberofAppls; App++)
	{
		APPL=&APPLCALLTABLE[App];

		if (APPL->APPLHASALIAS == 0 && CompareCalls(Msg->ORIGIN, APPL->APPLCALL))
			return;
	}

	Msg->ORIGIN[6] &= 0x1E;			// MASK OFF LAST ADDR BIT

	//	SEE IF ORIGINATING CALL IS IN NEIGHBOUR LIST - IF NOT ADD IT

	if (FindNeighbour(Msg->ORIGIN, Portno, &ROUTE) == 0)
	{
		// Not in list

		if (ROUTE == NULL)
			return;					// Table full

		//	CREATE NEIGHBOUR RECORD

		memcpy(ROUTE->NEIGHBOUR_CALL, Msg->ORIGIN, 7);

		ROUTE->NEIGHBOUR_PORT = Portno;
		ROUTE->NEIGHBOUR_QUAL = PORT->PORTQUALITY;
		
		ROUTE->NEIGHBOUR_LINK = 0;		// CANT HAVE A LINK IF NEW _NODE
	
		ROUTE->NoKeepAlive = PORT->PortNoKeepAlive;
	}

	// if locked route with quality zero ignore

	if ((ROUTE->NEIGHBOUR_FLAG & 1))	 // LOCKED ROUTE
		if (ROUTE->NEIGHBOUR_QUAL == 0)
			return;

	//	If Ignoreunlocked set, ignore it not locked

	if ((ROUTE->NEIGHBOUR_FLAG & 1) == 0)	 // LOCKED ROUTE
		if (PORT->IgnoreUnlocked)
			return;
	

	// if not locked, update route quality from port quality (may have changed config and not cleared SAVENODES

	if ((ROUTE->NEIGHBOUR_FLAG & 1) == 0)	 // Not LOCKED ROUTE
		ROUTE->NEIGHBOUR_QUAL = PORT->PORTQUALITY;

	//	GET TIME FROM BIOS DATA AREA OR RTC

	time((time_t *)&Stamp);

	Stamp = Stamp % 86400;			// Secs into day
	HH = Stamp / 3600;

	Stamp -= HH * 3600;
	MM = Stamp  / 60;

	ROUTE->NEIGHBOUR_TIME = 256 * HH + MM;

	//	GET QUALITY

	Qual = ROUTEQUAL = ROUTE->NEIGHBOUR_QUAL;		// FOR INITIAL ROUTE TABLE UPDATE
	
	//	CHECK LINK IS IN DEST LIST

	if (FindDestination(Msg->ORIGIN, &DEST) == 0)
	{
		if (DEST == NULL)
			return;				// Tsble Full

		//	CREATE DESTINATION RECORD

		memset(DEST, 0, sizeof(struct DEST_LIST));

		memcpy(DEST->DEST_CALL, Msg->ORIGIN, 7);
	
		NUMBEROFNODES++;
	}

	//	ALWAYS UPDATE ALIAS IN CASE NOT PRESENT IN ORIGINAL TABLE

	ptr1 = &Msg->L2DATA[1];
	ptr2 = &DEST->DEST_ALIAS[0];

	if (*ptr1  > ' ')			// Only of present
	{
		// Validate Alias, mainly for DISABL KPC3 Problem
		UCHAR c;

		n = 6;
		while (n--)
		{
			c = *(ptr1++);
			if (c < 0x20 || c > 0x7A)
				c = ' ';

			*(ptr2++) = c;
		}
	}

	//	UPDATE QUALITY AND OBS COUNT

	PROCROUTES(DEST, ROUTE, Qual);

	Msglen -= 23;				// LEVEL 2 HEADER FLAG and NODE MNEMONIC

	//	PROCESS DESTINATIONS from message

	// ptr1 = start

	saveptr = ptr1;

	while (Msglen > 21)		// STILL AT LEAST 1 ENTRY LEFT
	{

		Msglen -= 21;
		ptr1 = saveptr;
		saveptr += 21;

		// SEE IF OUR and of our CALLs - DONT WANT TO PUT IT IN LIST!

		if (CompareCalls(ptr1, MYCALL))
		{
			// But use it to get route quality setting from other end
			
			// As we now get qual ftom highest, only use this after a reload in
			// case other end has changed.
			if (ROUTE->FirstTimeFlag == 0)
			{
				// Check if route is via our node

				if (memcmp(ptr1, &ptr1[13], 7) == 0)
				{
					if (ROUTE->OtherendLocked == 0)	// Dont update locked quality
						ROUTE->OtherendsRouteQual = ptr1[20];

					ROUTE->FirstTimeFlag = 1;		// Only do it first time after load
				}
			}
			continue;
		}
		for (n = 0; n < 32; n++)
		{
			if (CompareCalls(ptr1, APPLCALLTABLE[n].APPLCALL))
				continue;
		}

		//	MAKE SURE ITS NOT CORRUPTED

		n = 6;

		while (n--)
		{
			if (*(ptr1++) < 0x40)	// Call
				continue;
		}
		ptr1++;						// skip ssid

		n = 6;

		while (n--)
		{
			if (*(ptr1++) < 0x20)	// Alias
				continue;
		}

		ptr1 -= 13;					// Back to start

		// CALCULATE ROUTE QUALITY

		// Experimantal Code to adjuct received route qualities based on deduced quality
		// settings at other end of link.

		// Don't mess with Application Qualities. There are almost always 255, and
		// if not there is probably a good reason for the value chosen.

		if (CompareCalls(Msg->ORIGIN, &ptr1[13]))
		{
			// Application Node - Just do normal update

			Qual = (((ROUTEQUAL * ptr1[20]) + 128)) / 256;
		}
		else
		{
			// Try using the highest reported indirect route as remote qual

			if (ROUTE->OtherendLocked == 0)			// Not locked
			{
				if (ptr1[20] > ROUTE->OtherendsRouteQual)
					ROUTE->OtherendsRouteQual = ptr1[20];
			}

			// Treat 255 as 254, so 255 routes doen't get included with minquals 
			//	designed to only include applcalls

			if (ptr1[20] == 255)
				ptr1[20] = 254;	

			Qual = (((ROUTEQUAL * ptr1[20]) + 128)) / 256;

			if (ROUTE->OtherendsRouteQual && PORT->NormalizeQuality)
			{
				Qual = (Qual * ROUTEQUAL) / ROUTE->OtherendsRouteQual;
				if (Qual > ROUTEQUAL)
					Qual = ROUTEQUAL;
			}
		}

		//	SEE IF BELOW MIN QUAL FOR AUTO UPDATES

		if (Qual < MINQUAL)
			continue;

		//	CHECK LINK IS IN DEST LIST
		
		if (FindDestination(ptr1, &DEST) == 0)
		{
			if (DEST == NULL)
				continue;
		
			//	CREATE DESTINATION RECORD

			memset(DEST, 0, sizeof(struct DEST_LIST));
			memcpy(DEST->DEST_CALL, ptr1, 7);
			NUMBEROFNODES++;
		}

		ptr1 += 7;
	
		//	UPDATE ALIAS

		memcpy(DEST->DEST_ALIAS, ptr1, 6);

		ptr1 += 6;
		
		//	NOW POINTING AT BEST NEIGHBOUR - IF THIS IS US, THEN ROUTE IS A LOOP
	
		if (CompareCalls(ptr1, NETROMCALL))
			Qual = 0;
	
		//	DEST IS NOW IN TABLE -

		//	   1. SEE IF THIS ROUTE IS IN TABLE - IF SO UPDATE QUALITY,
		//	      IF NOT, ADD THIS ONE IF IT HAS HIGHER QUALITY THAN EXISTING ONES


		PROCROUTES(DEST, ROUTE, Qual);
		ptr1 += 8;
	}
}

VOID PROCROUTES(struct DEST_LIST * DEST, struct ROUTE * ROUTE, int Qual)
{
	//	ADD NEIGHBOUR ADDRESS IN ROUTE TO NODE's ROUTE TABLE IF BETTER QUALITY THAN THOSE PRESENT

	int Index = 0;
	struct NR_DEST_ROUTE_ENTRY Temp;
	
	if (DEST->DEST_STATE & 0x80)			// BBS ENTRY
		return;

	for (Index = 0; Index < 4; Index++)
	{
		if (DEST->NRROUTE[Index].ROUT_NEIGHBOUR == ROUTE)
		{
			if (Index == 0)
			{
				//	THIS IS A REFRESH OF BEST - IF THIS ISNT ACTIVE ROUTE, MAKE IT ACTIVE
	
				if (DEST->DEST_ROUTE > 1)	// LEAVE IT IF NOT SELECTED OR ALREADY USING BEST
					DEST->DEST_ROUTE = 1;
			}

			goto UpdatateThisEntry;
		}
	}
				
	//	NOT IN ANY ROUTE

	Index = 0;

	if (DEST->NRROUTE[0].ROUT_NEIGHBOUR == 0)
		goto UpdatateThisEntry;					// SPARE ENTRY, SO USE IT

	if (DEST->NRROUTE[0].ROUT_QUALITY < Qual)
	{
		// New route is better than best, so move other two down and add here

		DEST->NRROUTE[2] = DEST->NRROUTE[1];
		DEST->NRROUTE[1] = DEST->NRROUTE[0];

		DEST->DEST_ROUTE = 0;			// Se we will switch to new one
		
		goto UpdatateThisEntry;
	}

	Index = 1;

	if (DEST->NRROUTE[1].ROUT_NEIGHBOUR == 0)
		goto UpdatateThisEntry;					// SPARE ENTRY, SO USE IT

	if (DEST->NRROUTE[1].ROUT_QUALITY < Qual)
	{
		// New route is better than second, so move down and add here

		DEST->NRROUTE[2] = DEST->NRROUTE[1];
		goto UpdatateThisEntry;					// SPARE ENTRY, SO USE IT
	}

	Index = 2;

	if (DEST->NRROUTE[2].ROUT_NEIGHBOUR == 0)
		goto UpdatateThisEntry;					// SPARE ENTRY, SO USE IT

	if (DEST->NRROUTE[2].ROUT_QUALITY < Qual)
	{
		// New route is better than third, so  add here
		goto UpdatateThisEntry;					// SPARE ENTRY, SO USE IT
	}

	//	THIS ROUTE IS WORSE THAN ANY OF THE CURRENT 3 - IGNORE IT

	return;


UpdatateThisEntry:

	DEST->NRROUTE[Index].ROUT_NEIGHBOUR = ROUTE;

	//	I DONT KNOW WHY I DID THIS, BUT IT CAUSES REFLECTED ROUTES
	//	TO BE SET UP WITH OBS = 0. THIS MAY PREVENT A VALID ALTERNATE
	//	VIA THE SAME _NODE TO FAIL TO BE FOUND. SO I'LL TAKE OUT THE
	//	TEST AND SEE IF ANYTHING NASTY HAPPENS
	//	IT DID - THIS IS ALSO CALLED BY CHECKL3TABLES. TRY RESETING
	//	OBS, BUT NOT QUALITY 

	if ((DEST->NRROUTE[Index].ROUT_OBSCOUNT & 0x80) == 0)
		DEST->NRROUTE[Index].ROUT_OBSCOUNT = OBSINIT;	// SET OBSOLESCENCE COUNT

	if (Qual)
		DEST->NRROUTE[Index].ROUT_QUALITY = Qual;		// IF ZERO, SKIP UPDATE

	//	IT IS POSSIBLE ROUTES ARE NOW OUT OF ORDER

SORTROUTES:

	if (DEST->NRROUTE[1].ROUT_QUALITY > DEST->NRROUTE[0].ROUT_QUALITY)
	{
		//	SWAP 1 AND 2

		Temp = DEST->NRROUTE[0];
		
		DEST->NRROUTE[0] = DEST->NRROUTE[1];
		DEST->NRROUTE[1] = Temp;

		DEST->DEST_ROUTE = 0;		// FORCE A RE-ASSESSMENT
	}

	if (DEST->NRROUTE[2].ROUT_QUALITY > DEST->NRROUTE[1].ROUT_QUALITY)
	{
		//	SWAP 2 AND 3 

		Temp = DEST->NRROUTE[1];
		
		DEST->NRROUTE[1] = DEST->NRROUTE[2];
		DEST->NRROUTE[2] = Temp;
		
		goto SORTROUTES;			//  1 AND 2 MAY NOW BE WRONG!	
	}
}

int COUNTNODES(struct ROUTE * ROUTE)
{
	//	COUNT NODES WITH ROUTE VIA NEIGHBOUR IN ESI

	int count = 0;
	int n = MAXDESTS;
	struct DEST_LIST * DEST = DESTS;		// NODE LIST

	while (n--)
	{
		if (DEST->NRROUTE[0].ROUT_NEIGHBOUR == ROUTE)
			count++;
		else if (DEST->NRROUTE[1].ROUT_NEIGHBOUR == ROUTE)
			count++;
		else if (DEST->NRROUTE[2].ROUT_NEIGHBOUR == ROUTE)
			count++;
		else if (DEST->ROUTE[0].ROUT_NEIGHBOUR == ROUTE)
			count++;
		else if (DEST->ROUTE[1].ROUT_NEIGHBOUR == ROUTE)
			count++;
		else if (DEST->ROUTE[2].ROUT_NEIGHBOUR == ROUTE)
			count++;

		DEST++;
	}
	return count;
}

VOID SENDNODE00();
VOID L3BG();



VOID SENDNEXTNODESFRAGMENT();

VOID SENDNODESMSG()
{
	if (NODESINPROGRESS)
		return;

	L3CURRENTPORT = PORTTABLE;
	SENDNEXTNODESFRAGMENT();
}



VOID SENDNEXTNODESFRAGMENT()
{
	//	SEND NEXT FRAGMENT OF A NODES BROADCAST

	//	FRAGMENTS ARE SENT AT 10 SECONDS INTERVALS - PARTLY TO REDUCE
	//	QRM, PARTLY TO REDUCE LOAD ON BUFFERS (AND TX POWER SUPPLIES!)

	//	SEND TO PORT IN CURRENTPORT, STARTING AT CURRENTNODE

	struct PORTCONTROL * PORT = L3CURRENTPORT;
	dest_list * DEST = CURRENTNODE;
	MESSAGE * Buffer;
	int Count;
	int Qual;
	int CURRENTPORTNO, TXMINQUAL;
	UCHAR * ptr1, * ptr2;

	if (DEST == 0)
	{
		//	FIRST FRAGMENT TO A PORT

		while (PORT->PORTQUALITY == 0 || PORT->TXPORT || PORT->INP3ONLY)
			// Don't send NODES to Shared TX or INP3 Port
		{
			// No NODES to this port, so go to next

			PORT = PORT->PORTPOINTER;
			if (PORT == NULL)
			{
				// Finished

				NODESINPROGRESS = 0;
				return;
			}
		}

		L3CURRENTPORT = PORT;

		DEST = CURRENTNODE = DESTS;			// START OF LIST
		NODESINPROGRESS = 1;
	}

	CURRENTPORTNO = PORT->PORTNUMBER; 

	TXMINQUAL = PORT->PORTMINQUAL;

	if (TXMINQUAL == 0)
		TXMINQUAL = 1;						// Dont send zero

	Buffer = GetBuff();

	if (Buffer == 0)
		return;

	Buffer->PORT = CURRENTPORTNO;

	memcpy(Buffer->ORIGIN, NETROMCALL, 7);
	memcpy(Buffer->DEST, NODECALL, 7);

	Buffer->ORIGIN[6] |= 0x61;		// SET CMD END AND RESERVED BITS

	Buffer->CTL = UI;
	Buffer->PID = 0xCF;				// Netrom

	ptr1 = &Buffer->L2DATA[0];
	
	*(ptr1++) = 0xff;		// Nodes Flag

	memcpy(ptr1, MYALIASTEXT, 6);

	ptr1+= 6;

	//	ADD DESTINATION INFO (UNLESS BBS ONLY)

	if (NODE == 0)
	{
		CURRENTNODE = 0;			// Finished on this port
		goto Sendit;
	}

	Count = PORT->NODESPACLEN;
	
	if (Count == 0)
		Count = 256;
	
	if (Count < 50)				// STUPIDLY SMALL?
		Count = 50;					// EVEN THIS IS RATHER SILLY

	Count -= 22;					// Fixed Part

	Count /= 21;					// 21 Bytres per entry

	while (Count)
	{
		if (DEST >= ENDDESTLIST)
		{
			CURRENTNODE = 0;			// Finished on this port
			L3CURRENTPORT = PORT->PORTPOINTER;
			if (L3CURRENTPORT == NULL)
			{
				// Finished

				NODESINPROGRESS = 0;
			}
			goto Sendit;
		}

		if (DEST->NRROUTE[0].ROUT_QUALITY >= TXMINQUAL && DEST->NRROUTE[0].ROUT_OBSCOUNT >= OBSMIN)
		{		
			// Send it
			
			ptr2 = &DEST->DEST_CALL[0];
			memcpy(ptr1, ptr2, 13);				// Dest and Alias
			ptr1 += 13;

			ptr2 = (UCHAR *)DEST->NRROUTE[0].ROUT_NEIGHBOUR;
			
			if (ptr2 == 0)
		
				//	DUMMY POINTER IN BBS ENTRY - PUT IN OUR CALL

				ptr2 = MYCALL;
		
			memcpy(ptr1, ptr2, 7);				// Neighbour Call
			ptr1 += 7;
	
			Qual = 100;

			if (DEST->NRROUTE[0].ROUT_NEIGHBOUR && DEST->NRROUTE[0].ROUT_NEIGHBOUR->NEIGHBOUR_PORT == CURRENTPORTNO)
			{
				//	BEST NEIGHBOUR IS ON CURRENT PORT - REDUCE QUALITY BY QUAL_ADJUST

				Qual -= PORT->QUAL_ADJUST;
			}

			Qual *= DEST->NRROUTE[0].ROUT_QUALITY;
			Qual /= 100;

			*(ptr1++) = (UCHAR)Qual;

			Count--;
		}
		DEST++;
	}

	CURRENTNODE = DEST;

Sendit:

	Buffer->LENGTH = ptr1 - (UCHAR *)Buffer;

	PUT_ON_PORT_Q(PORT, Buffer);

}

VOID L3LINKCLOSED(struct _LINKTABLE * LINK, int Reason)
{
	//	L2 SESSION HAS SHUT DOWN (PROBABLY DUE TO INACTIVITY)
	
	struct ROUTE * ROUTE;
	
	//	CLEAR NEIGHBOUR

	ROUTE = LINK->NEIGHBOUR;			// TO NEIGHBOUR

	if (ROUTE)
	{
		LINK->NEIGHBOUR = NULL;			// Clear links
		ROUTE->NEIGHBOUR_LINK = NULL;

		CLEARACTIVEROUTE(ROUTE, Reason);		// CLEAR ASSOCIATED DEST ENTRIES
	}
}

VOID CLEARACTIVEROUTE(struct ROUTE * ROUTE, int Reason)
{
	//	FIND ANY DESINATIONS WITH [ESI] AS ACTIVE NEIGHBOUR, AND
	//	SET INACTIVE

	dest_list * DEST;
	int n;

	if (Reason != NORMALCLOSE || ROUTE->INP3Node)
		TellINP3LinkGone(ROUTE);
	
	DEST = DESTS;
	n = MAXDESTS;

	DEST--;							// So we can increment at start of loop

	while (n--)
	{
		DEST++;

		if (DEST->DEST_ROUTE == 0)
			continue;

		if (DEST->ROUTE[DEST->DEST_ROUTE].ROUT_NEIGHBOUR == ROUTE)   // Is this the active route
		{
			// Yes, so clear

			DEST->DEST_ROUTE = 0;			// SET NO ACTIVE ROUTE
		}
	}
}


VOID L3TimerProc()
{
	int i;
	struct PORTCONTROL * PORT = PORTTABLE;
	struct ROUTE * ROUTE;

	struct _LINKTABLE * LINK;

	//	CHECK FOR EXCESSIVE BUFFERS QUEUED AT LINK LEVEL

	if (QCOUNT < 100)
	{
		LINK = LINKS;
		i = MAXLINKS;

		while (i--);
		{
			if (LINK->LINKTYPE == 3)		// Only if Internode
			{
				if (COUNT_AT_L2(LINK) > 50)
				{
					Debugprintf("Excessive L3 Queue");
					L3LINKCLOSED(LINK, LINKSTUCK);			// REPORT TO LEVEL 3
					CLEAROUTLINK(LINK);
				}
			}
			LINK++;
		}
	}

	STATSTIME++;

	if (IDTIMER)				// Not if Disabled
	{
		IDTIMER--;

		if (IDTIMER == 0)
		{
			IDTIMER = IDINTERVAL;
			SENDIDMSG();
		}
	}

	//	CHECK FOR BEACON

	if (BTTIMER)				// Not if Disabled
	{
		BTTIMER--;

		if (BTTIMER == 0)
		{
			BTTIMER = BTINTERVAL;
			SENDBTMSG();
		}
	}

	//	CHECK FOR NODES BROADCAST

	if (L3TIMER)				// Not if Disabled
	{
		L3TIMER--;

		if (L3TIMER == 0)
		{
			//	UPDATE DEST LIST AND SEND 'NODES' MESSAGE
			
			L3TIMER = L3INTERVAL;
			UPDATEDESTLIST();
			SENDNODESMSG();
		}
	}

	//	TIDY ROUTES

	ROUTE = NEIGHBOURS;
	i = MAXNEIGHBOURS;

	ROUTE--;

	while (i--)
	{
		ROUTE++;

		if (ROUTE->NEIGHBOUR_FLAG & 1)			// Locked?
			continue;

		if (ROUTE->NEIGHBOUR_LINK)				// Has an active Session
			continue;

		if (COUNTNODES(ROUTE) == 0)				// NODES USING THIS DESTINATION
		{
			// IF NUMBER USING ROUTE IS ZERO, DELETE IT

			memset(ROUTE, 0, sizeof (struct ROUTE));
		}
	}
}

VOID L3FastTimer()
{
	//	CALLED ONCE PER SECOND - USED ONLY TO SEND NEXT PART OF A NODES OR
	//	ID MESSAGE SEQUENCE

	MESSAGE * Msg;
	struct PORTCONTROL * PORT ;

	INP3TIMER();

	L3_10SECS--;

	if (L3_10SECS == 0)
	{
		L3_10SECS = 10;
	
		if (IDMSG_Q)				// ID/BEACON TO SEND
		{
			Msg = Q_REM(&IDMSG_Q);

			PORT = GetPortTableEntryFromPortNum(Msg->PORT);

			if (PORT && PORT->TXPORT == 0)			// DONT SEND IF SHARED TX
				PUT_ON_PORT_Q(PORT, Msg);
			else
				ReleaseBuffer(Msg);
		}

		if (NODESINPROGRESS)
			SENDNEXTNODESFRAGMENT();
	}
}


VOID UPDATEDESTLIST()
{
	//	DECREMENT OBS COUNTERS ON EACH ROUTE, AND REMOVE 'DEAD' ENTRIES
	
	dest_list * DEST;
	int n;

	DEST = DESTS;
	n = MAXDESTS;

	DEST--;							// So we can increment at start of loop

	while (n--)
	{
		DEST++;

		if (DEST->DEST_CALL[0] == 0)	// SPARE ENTRY
			continue;

		if (DEST->DEST_STATE & 0x80)	// LOCKED DESTINATION
			continue;

UPDEST000:

		if (DEST->NRROUTE[0].ROUT_NEIGHBOUR == 0)		 // NO DESTINATIONS - DELETE ENTRY unless inp3 routes
		{
			//	 Any INP3 Routes?

			if (DEST->ROUTE[0].ROUT_NEIGHBOUR == 0)
			{
				//	NO ROUTES LEFT TO DEST - REMOVE IT

				REMOVENODE(DEST);						// Unchain, Clear queue and zap
			}
			continue;			
		}

		if (DEST->NRROUTE[0].ROUT_OBSCOUNT == 0)
		{
			//	 FAILED IN USE - DELETE
			
			MOVEALL(DEST);
			goto UPDEST000; //LOOP BACK TO PROCESS MOVED ENTRIES
		}

		if ((DEST->NRROUTE[0].ROUT_OBSCOUNT & 0x80) == 0)	// Locked?
		{
			DEST->NRROUTE[0].ROUT_OBSCOUNT--;

			if (DEST->NRROUTE[0].ROUT_OBSCOUNT == 0)		// Timed out
			{
				MOVEALL(DEST);
				goto UPDEST000; //LOOP BACK TO PROCESS MOVED ENTRIES
			}
		}
	
		// Process Next Neighbour

UPDEST010:

		if (DEST->NRROUTE[1].ROUT_NEIGHBOUR == 0)
			continue;				// NO MORE DESTINATIONS

		if (DEST->NRROUTE[1].ROUT_OBSCOUNT == 0)
		{
			//	 FAILED IN USE - DELETE
			
			MOVE3TO2(DEST);
			goto UPDEST010;			//LOOP BACK TO PROCESS MOVED ENTRIES
		}

		if ((DEST->NRROUTE[1].ROUT_OBSCOUNT & 0x80) == 0)	// Locked?
		{
			DEST->NRROUTE[1].ROUT_OBSCOUNT--;

			if (DEST->NRROUTE[1].ROUT_OBSCOUNT == 0)		// Timed out
			{
				MOVE3TO2(DEST);
				goto UPDEST010; //LOOP BACK TO PROCESS MOVED ENTRIES
			}
		}
	
		// Process Next Neighbour

		if (DEST->NRROUTE[2].ROUT_NEIGHBOUR == 0)
			continue;				// NO MORE DESTINATIONS

		if (DEST->NRROUTE[2].ROUT_OBSCOUNT == 0)
		{
			//	 FAILED IN USE - DELETE
			
			CLEARTHIRD(DEST);
			continue;
		}

		if ((DEST->NRROUTE[2].ROUT_OBSCOUNT & 0x80) == 0)	// Locked?
		{
			DEST->NRROUTE[2].ROUT_OBSCOUNT--;

			if (DEST->NRROUTE[2].ROUT_OBSCOUNT == 0)		// Timed out
			{
				CLEARTHIRD(DEST);
				continue;
			}
		}
	}
}


VOID MOVEALL(dest_list * DEST)
{
	DEST->NRROUTE[0] = DEST->NRROUTE[1];
	MOVE3TO2(DEST);
}

VOID MOVE3TO2(dest_list * DEST)
{
	DEST->NRROUTE[1] = DEST->NRROUTE[2];
	CLEARTHIRD(DEST);
}

VOID CLEARTHIRD(dest_list * DEST)
{
	memset(&DEST->NRROUTE[2], 0, sizeof (struct NR_DEST_ROUTE_ENTRY));
	DEST->DEST_ROUTE = 0;		// CANCEL ACTIVE ROUTE, SO WILL RE-ASSESS BEST
}

// L4 Flags Values

#define DISCPENDING	8		// SEND DISC WHEN ALL DATA ACK'ED

VOID REMOVENODE(dest_list * DEST)
{
	TRANSPORTENTRY * L4 = L4TABLE;
	int n = MAXCIRCUITS;

	//	Remove a node, either because routes have gone, or APPL API has invalidated it

	while (DEST->DEST_Q)
		ReleaseBuffer(Q_REM(&DEST->DEST_Q));

	//	MAY NEED TO CHECK FOR L4 CIRCUITS USING DEST, BUT PROBABLY NOT,
	//	 AS THEY SHOULD HAVE TIMED OUT LONG AGO

	// Not necessarily true with INP3, so had better check

	while (n--)
	{
		if (L4->L4USER[0])
		{
			if (L4->L4TARGET.DEST == DEST)
			{
				// Session to/from this Dest

				TRANSPORTENTRY * Partner = L4->L4CROSSLINK;
				char Nodename[20];

				Nodename[DecodeNodeName(DEST->DEST_CALL, Nodename)] = 0;		// null terminate

				Debugprintf("Delete Node for %s Called with active L4 Session - State %d",
					Nodename, L4->L4STATE);

				if (Partner)
				{	
					// if connnecting, send error message and drop back to command level

					if (L4->L4STATE == 2)
					{
						struct DATAMESSAGE * Msg = GetBuff();
						Partner->L4CROSSLINK = 0;		// Back to command lewel

						if (Msg)
						{
							UCHAR * ptr1;
		
							Msg->PID = 0xf0;
							ptr1 = SetupNodeHeader(Msg);
							ptr1 += sprintf(ptr1, "Error - Node %s has disappeared\r", Nodename);

							Msg->LENGTH = ptr1 - (UCHAR *)Msg;
							C_Q_ADD(&Partner->L4TX_Q, Msg);
							PostDataAvailable(Partner);
						}
					}
					else
					{
						// Failed in session - treat as if a L4DREQ received

						CLOSECURRENTSESSION(Partner);
					}
				}
				CLEARSESSIONENTRY(L4);
			}
		}
		L4++;
	}
	memset(DEST, 0, sizeof(struct DEST_LIST));	
	NUMBEROFNODES--;
}

VOID L3CONNECTFAILED(struct _LINKTABLE * LINK)
{
	//	L2 LINK SETUP HAS FAILED - SEE IF ANOTHER NEIGHBOUR CAN BE USED

	struct PORTCONTROL * PORT = PORTTABLE;
	struct ROUTE * ROUTE;


	ROUTE = LINK->NEIGHBOUR;		// TO NEIGHBOUR
	
	if (ROUTE == NULL)
		return;						// NOTHING ???
	
	TellINP3LinkSetupFailed(ROUTE);

	ROUTE->NEIGHBOUR_LINK = 0;		// CLEAR IT

	L3TRYNEXTDEST(ROUTE);			// RESET ASSOCIATED DEST ENTRIES
}


VOID L3TRYNEXTDEST(struct ROUTE * ROUTE)
{
	//	FIND ANY DESINATIONS WITH [ESI] AS ACTIVE NEIGHBOUR, AND
	//	SET NEXT BEST NEIGHBOUR (IF ANY) ACTIVE

	int n = MAXDESTS;
	struct DEST_LIST * DEST = DESTS;		// NODE LIST
	int ActiveRoute;

	while (n--)
	{
		ActiveRoute = DEST->DEST_ROUTE;
		
		if (ActiveRoute)
		{
			ActiveRoute --;			// Routes numbered 1 - 6, idex from 0
			
			if (DEST->NRROUTE[ActiveRoute].ROUT_NEIGHBOUR == ROUTE)
			{
				// We were best

				//	NEIGHBOUR HAS FAILED - DECREMENT OBSCOUNT
				//	AND TRY TO ACTIVATE ANOTHER (IF ANY)

				if (DEST->NRROUTE[ActiveRoute].ROUT_OBSCOUNT)
				{
					// IF ALREADY ZERO - WILL BE DELETED BY NEXT NODES UPDATE

					if ((DEST->NRROUTE[ActiveRoute].ROUT_OBSCOUNT & 0x80) == 0)
					{
						// not Locked

						DEST->NRROUTE[ActiveRoute].ROUT_OBSCOUNT--;

						// if ROUTE HAS EXPIRED - WE SHOULD CLEAR IT, AND MOVE OTHERS (IF ANY) UP
					}
				}

				//	REMOVE FIRST MESSAGE FROM DEST_Q. L4 WILL RETRY - IF IT IS LEFT HERE
				//	WE WILL TRY TO ACTIVATE THE DESTINATION FOR EVER

				if (DEST->DEST_Q)
					ReleaseBuffer(Q_REM(&DEST->DEST_Q));

				DEST->DEST_ROUTE++;			// TO NEXT
				
				if (DEST->DEST_ROUTE = 7)
					DEST->DEST_ROUTE = 1;	// TRY TO ACTIVATE FIRST
			}
		}
		DEST++;
	}
}

VOID CHECKNEIGHBOUR(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * Msg)
{
	//	MESSAGE RECEIVED ON LINK WITH NO ROUTE - SET ONE UP

	struct ROUTE * ROUTE;
	struct PORTCONTROL * PORT = LINK->LINKPORT;
	int Portno = PORT->PORTNUMBER;

	if (FindNeighbour(LINK->LINKCALL, Portno, &ROUTE) == 0)
	{
		if (ROUTE == 0)
			goto L3CONN08;			// TABLE FULL??
		
		// FIRST MAKE SURE WE ARE ALLOWING NETWORK ACTIVITY ON THIS PORT

		if (PORT->PORTQUALITY == 0 && PORT->INP3ONLY == 0)
			return;

		ROUTE->NEIGHBOUR_QUAL = PORT->PORTQUALITY;
		ROUTE->INP3Node = PORT->INP3ONLY;

		memcpy(ROUTE->NEIGHBOUR_CALL, LINK->LINKCALL, 7);

		ROUTE->NEIGHBOUR_PORT = Portno;
		ROUTE->NoKeepAlive = PORT->PortNoKeepAlive;
	}

	//	SET THIS AS ACTIVE LINK IF NONE PRESENT

	if (ROUTE->NEIGHBOUR_LINK == 0)
	{
		ROUTE->NEIGHBOUR_LINK = LINK;
		ROUTE->NEIGHBOUR_PORT = Portno;
	}

L3CONN08:

	LINK->NEIGHBOUR = ROUTE;		// SET LINK - NEIGHBOUR
}

struct DEST_LIST * CHECKL3TABLES(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * Msg)
{
	//	CHECK THAT FAR NODE IS IN 'NODES'.
	//	RETURNS POINTER TO NEIGHBOUR ENTRY

	struct ROUTE * ROUTE;
	struct DEST_LIST * DEST;
	int Qual;

	ROUTE = LINK->NEIGHBOUR;

	if (FindDestination(Msg->L3SRCE, &DEST))
		return DEST;					// Ok
	
	if (DEST == NULL)
		return NULL;				// Tsble Full

	//	ADD DESTINATION VIA NEIGHBOUR, UNLESS ON BLACK LIST

#ifdef BLACKBITS

	if (CHECKBLACKLIST(Msg->L3SRCE)
		return 0;
	
#endif

	memcpy(DEST->DEST_CALL, Msg->L3SRCE, 7);
	
	NUMBEROFNODES++;

	//	MAKE SURE NEIGHBOUR IS DEFINED FOR DESTINATION

	//	IF NODE is NEIGHBOUR, THEN CAN USE NEIGHBOUR QUALITY,
	//	   OTHERWISE WE DONT KNOW ROUTE, SO MUST SET QUAL TO 0


	Qual = 0;						// DONT KNOW ROUTING, SO SET QUALITY TO ZERO
	
	PROCROUTES(DEST, ROUTE, Qual);	// ADD NEIGHBOUR  IF NOT PRESENT
	
	if (DEST->DEST_ROUTE == 0)
	{
		//	MAKE CURRENT NEIGHBOUR ACTIVE

		int n = 0;

		DEST->DEST_ROUTE = 1;

		while (n < 3)
		{
			if (DEST->NRROUTE[n].ROUT_NEIGHBOUR == ROUTE)
				break;

			DEST->DEST_ROUTE++;
			n++;
		}

		if (DEST->DEST_ROUTE > 3)
		{
			DEST->DEST_ROUTE = 1;	// CURRENT NEIGHBOUR ISNT IN DEST LIST - SET TO USE BEST
			return DEST;			// Can't update OBS
		}
	}

	//	REFRESH OBS COUNT

	if (DEST->DEST_ROUTE)
	{
		int Index = DEST->DEST_ROUTE -1;
		
		if (DEST->NRROUTE[Index].ROUT_OBSCOUNT & 0x80)		// Locked:
			return DEST;

		DEST->NRROUTE[Index].ROUT_OBSCOUNT = OBSINIT;
	}
	return DEST;
}

VOID REFRESHROUTE(TRANSPORTENTRY * Session)
{
	//	RESET OBS COUNT ON CURRENT ROUTE TO DEST FOR SESSION IN [EBX]
	//	CALLED WHEN INFO ACK RECEIVED, INDICATING ROUTE IS STILL OK

	struct DEST_LIST * DEST;
	int Index;

	DEST = Session->L4TARGET.DEST;

	if (DEST == 0)
		return;					// No Dest ???

	Index = DEST->DEST_ROUTE;

	if (Index == 0)
		return;					// NONE ACTIVE???

	Index--;

	if (DEST->NRROUTE[Index].ROUT_OBSCOUNT & 0x80)
		return;					// Locked

	DEST->NRROUTE[Index].ROUT_OBSCOUNT = OBSINIT;
}


