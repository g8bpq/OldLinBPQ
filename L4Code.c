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
//	C replacement for L4Code.asm
//
#define Kernel

#define _CRT_SECURE_NO_DEPRECATE 
#define _USE_32BIT_TIME_T

#pragma data_seg("_BPQDATA")

#include "time.h"
#include "stdio.h"
#include <fcntl.h>					 

#include "CHeaders.h"

extern BPQVECSTRUC BPQHOSTVECTOR[];
#define BPQHOSTSTREAMS 64
#define IPHOSTVECTOR BPQHOSTVECTOR[BPQHOSTSTREAMS + 3]

VOID CLOSECURRENTSESSION(TRANSPORTENTRY * Session);
VOID SENDL4DISC(TRANSPORTENTRY * Session);
int C_Q_COUNT(VOID * Q);
TRANSPORTENTRY * SetupSessionForL2(struct _LINKTABLE * LINK);
VOID InformPartner(struct _LINKTABLE * LINK);
VOID IFRM150(TRANSPORTENTRY * Session, PDATAMESSAGE Buffer);
VOID SendConNAK(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * L3MSG);
BOOL FINDCIRCUIT(L3MESSAGEBUFFER * L3MSG, TRANSPORTENTRY ** REQL4, int * NewIndex);
int GETBUSYBIT(TRANSPORTENTRY * L4);
BOOL cATTACHTOBBS(TRANSPORTENTRY * Session, UINT Mask, int Paclen, int * AnySessions);
VOID SETUPNEWCIRCUIT(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * L3MSG, 
		 TRANSPORTENTRY * L4, char * BPQPARAMS, int ApplMask, int * BPQNODE);
extern char * ALIASPTR;
VOID SendConACK(struct _LINKTABLE * LINK, TRANSPORTENTRY * L4, L3MESSAGEBUFFER * L3MSG, BOOL BPQNODE, UINT Applmask, UCHAR * ApplCall);
VOID L3SWAPADDRESSES(L3MESSAGEBUFFER * L3MSG);
VOID L4TIMEOUT(TRANSPORTENTRY * L4);
struct DEST_LIST * CHECKL3TABLES(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * Msg);
int CHECKIFBUSYL4(TRANSPORTENTRY * L4);
VOID AUTOTIMER();
VOID NRRecordRoute(char * Buff, int Len);
VOID REFRESHROUTE(TRANSPORTENTRY * Session);
VOID ACKFRAMES(L3MESSAGEBUFFER * L3MSG, TRANSPORTENTRY * L4, int NR);
VOID SENDL4IACK(TRANSPORTENTRY * Session);
VOID CHECKNEIGHBOUR(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * Msg);
VOID ProcessINP3RIF(struct ROUTE * Route, UCHAR * ptr1, int msglen, int Port);
VOID ProcessRTTMsg(struct ROUTE * Route, struct _L3MESSAGEBUFFER * Buff, int Len, int Port);
VOID FRAMEFORUS(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * L3MSG, int ApplMask, UCHAR * ApplCall);

extern UINT APPLMASK;

extern BOOL LogL4Connects;

// L4 Flags Values

#define DISCPENDING	8		// SEND DISC WHEN ALL DATA ACK'ED

VOID NETROMMSG(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * L3MSG)
{
	//	MAKE SURE PID IS 0CF - IN CASE SOMEONE IS SENDING L2 STUFF ON WHAT 
	//	WE THINK IS A _NODE-_NODE LINK

	struct DEST_LIST * DEST;
	APPLCALLS * APPL;
	int n;

	if (L3MSG->L3PID != 0xCF)
	{
		ReleaseBuffer(L3MSG);
		return;
	}

	if (LINK->NEIGHBOUR == 0)
	{
		// NO ROUTE ASSOCIATED WITH THIS CIRCUIT - SET ONE UP

		CHECKNEIGHBOUR(LINK, L3MSG);

		if (LINK->NEIGHBOUR == 0)
		{
			//	COULDNT SET UP NEIGHBOUR - CAN ONLY THROW IT AWAY

			ReleaseBuffer(L3MSG);
			return;
		}
	}

	// See if a INP3 RIF (first Byte 0xFF)

	if (L3MSG->L3SRCE[0] == 0xff)
	{
		// INP3

		ProcessINP3RIF(LINK->NEIGHBOUR, &L3MSG->L3SRCE[1], L3MSG->LENGTH - 9, L3MSG->Port);
		ReleaseBuffer(L3MSG);
		return;
	}

	APPLMASK = 0;		//	NOT APPLICATION 
	
	if (NODE)				// _NODE SUPPORT INCLUDED?
	{

		if (CompareCalls(L3MSG->L3DEST, MYCALL))
		{
			FRAMEFORUS(LINK, L3MSG, APPLMASK, MYCALL);
			return;
		}
	}
	
	//	CHECK ALL L4 CALLS

	APPLMASK = 1;
	ALIASPTR = &CMDALIAS[0][0];

	n = NumberofAppls;

	APPL = APPLCALLTABLE;

	while (n--)
	{
		if (APPL->APPLCALL[0] > 0x40)		// Valid ax.25 addr
		{
			if (CompareCalls(L3MSG->L3DEST, APPL->APPLCALL))
			{
				FRAMEFORUS(LINK, L3MSG, APPLMASK, APPL->APPLCALL);
				return;
			}
		}
		APPLMASK <<= 1;
		ALIASPTR += ALIASLEN;
		APPL++;
	}

	//	IS IT INP3 (L3RTT)

	if (CompareCalls(L3MSG->L3DEST, L3RTT))
	{
		ProcessRTTMsg(LINK->NEIGHBOUR, L3MSG, L3MSG->LENGTH, L3MSG->Port);
		return;
	}

	L3MSG->L3TTL--;
	
	if (L3MSG->L3TTL == 0)
	{
		ReleaseBuffer(L3MSG);
		return;
	}

	//	If it is a record route frame we should add our call to the list before sending on

	if (L3MSG->L4FLAGS == 0 && L3MSG->L4ID == 1 && L3MSG->L4INDEX == 0)
	{
		//	Add our call on end, and increase count

		int Len = L3MSG->LENGTH;
		int Count;

		UCHAR * ptr = (UCHAR *)L3MSG;

		if (Len < 248)
		{
			ptr += (Len - 1);

			Count = *(ptr++) = 0x7F;			// Mask End of Route

			memcpy(ptr, MYCALL, 7);

			ptr += 7;

			Count--;
			*(ptr) = Count;

			if (Count)
				L3MSG->LENGTH += 8;
		}
	}

	if (L3MSG->L3TTL > L3LIVES)
		L3MSG->L3TTL = L3LIVES;		// ENFORCE LIMIT ON ALL FRAMES SENT

	if (FindDestination(L3MSG->L3DEST, &DEST) == 0)
	{
		ReleaseBuffer(L3MSG);			// CANT FIND DESTINATION
		return;
	}

	//	IF MESSAGE ORIGINTED HERE, THERE MUST BE A ROUTING LOOP - 
	//   THERE IS LITTLE POINT SENDING IT OVER THE SAME ROUTE AGAIN,
	//   SO SET ANOTHER ROUTE ACTIVE IF POSSIBLE

	if (CompareCalls(L3MSG->L3SRCE, MYCALL) || CompareCalls(L3MSG->L3SRCE, APPLCALLTABLE->APPLCALL))
	{
		//	MESSAGE HAS COME BACK TO ITS STARTING POINT - ACTIVATE ANOTHER ROUTE,
		// UNLESS THERE IS ONLY ONE, IN WHICH CASE DISCARD IT

		if (DEST->NRROUTE[1].ROUT_NEIGHBOUR == 0)	// No more routes
		{
			ReleaseBuffer(L3MSG);
			return;
		}

		DEST->DEST_ROUTE++;

		if (DEST->DEST_ROUTE == 4)		// TO NEXT 
			DEST->DEST_ROUTE  = 1;		// TRY TO ACTIVATE FIRST
	}

	//	IF CURRENT ROUTE IS BACK THE WAY WE CAME, THEN ACTIVATE
	//ANOTHER (IF POSSIBLE). 

	if (DEST->DEST_ROUTE)
	{
		if (DEST->NRROUTE[DEST->DEST_ROUTE -1].ROUT_NEIGHBOUR == LINK->NEIGHBOUR)
		{
			//	Current ROUTE IS BACK THE WAY WE CAME - ACTIVATE ANOTHER IF POSSIBLE

			DEST->DEST_ROUTE++;
			if (DEST->DEST_ROUTE == 4)
				DEST->DEST_ROUTE =1;
		}
		goto NO_PROBLEM;
	}
	else
	{
		//	DONT HAVE AN ACTIVE ROUTE

		if (DEST->NRROUTE[0].ROUT_NEIGHBOUR == LINK->NEIGHBOUR)
		{
			//	FIRST ROUTE IS BACK THE WAY WE CAME - ACTIVATE ANOTHER IF POSSIBLE

			DEST->DEST_ROUTE = 2;	// WILL BE RESET BY L3 CODE IF THERE IS NOT OTHER ROUTE
		}
	}

NO_PROBLEM:

	CHECKL3TABLES(LINK, L3MSG);

//	EVEN IF WE CANT PUT ORIGINATING NODE INTO OUR TABLES, PASS MSG ON
//	   ANYWAY - THE FINAL TARGET MAY HAVE ANOTHER WAY BACK


	C_Q_ADD(&DEST->DEST_Q, L3MSG);

	L3FRAMES++;
}

VOID SENDL4MESSAGE(TRANSPORTENTRY * L4, struct DATAMESSAGE * Msg)
{
	L3MESSAGEBUFFER * L3MSG;
	struct DEST_LIST * DEST;
	struct DATAMESSAGE * Copy;
	int FRAGFLAG = 0;
	int Len;

	if (Msg->LENGTH == 8)
	{
		//	NO DATA - DISCARD IT

		ReleaseBuffer(Msg);
		return;
	}

	L3MSG = GetBuff();

	if (L3MSG == 0)
	{
		//	DONT THINK WE SHOULD GET HERE, UNLESS _QCOUNT IS CORRUPT,
		//	BUT IF WE DO, SHOULD RETURN MSG TO FREE Q - START TIMER, AND
		//	DROP THROUGH TO RELBUFF

		L4->L4TIMER = L4->SESSIONT1;

		ReleaseBuffer(Msg);
		return;
	}

	L3MSG->L3PID = 0xCF;			// NET MESSAGE

	memcpy(L3MSG->L3SRCE, L4->L4MYCALL, 7);

	DEST = L4->L4TARGET.DEST;
	memcpy(L3MSG->L3DEST, DEST->DEST_CALL, 7);

	L3MSG->L3TTL = L3LIVES;
	
	L3MSG->L4ID = L4->FARID;
	L3MSG->L4INDEX = L4->FARINDEX;

	L3MSG->L4TXNO = L4->TXSEQNO;

	//	SET UP RTT TIMER

	if (L4->RTT_TIMER == 0)
	{
		L4->RTT_SEQ = L4->TXSEQNO;

		L4->RTT_TIMER = GetTickCount();
	}

	L4->TXSEQNO++;
	

	L4->L4LASTACKED	= L3MSG->L4RXNO = L4->RXSEQNO;		// SAVE LAST NUMBER ACKED

	// SEE IF CROSSSESSION IS BUSY

	GETBUSYBIT(L4);							// Sets BUSY in NAKBITS if Busy

	L3MSG->L4FLAGS = L4INFO | L4->NAKBITS;

	L4->L4TIMER = L4->SESSIONT1;			// SET TIMER
	L4->L4ACKREQ = 0;						// CANCEL ACK NEEDED

	Len = Msg->LENGTH;

	if (Len > 244)							// 236 DATA + 8 HEADER
	{
		//	MUST FRAGMENT MESSAGE

		L3MSG->L4FLAGS |= L4MORE;
		FRAGFLAG = 1;

		Len = 244;
	}
	
	Len += 20;								// L3/4 Header

	L3MSG->LENGTH = Len;

	Len -= 28;

	memcpy(L3MSG->L4DATA, Msg->L2DATA, Len);

	//	CREATE COPY FOR POSSIBLE RETRY

	Copy = GetBuff();

	if (Copy == 0)
	{
		// SHOULD NEVER HAPPEN
		
		ReleaseBuffer(Msg);
		return;
	}

	memcpy(Copy, L3MSG, L3MSG->LENGTH);
	
	// If we have fragmented, we should adjust length, or retry will send too much
	//	(bug in .asm code)

	if (FRAGFLAG)
		Copy->LENGTH = 244;

	C_Q_ADD(&L4->L4HOLD_Q, Copy);

	C_Q_ADD(&DEST->DEST_Q, L3MSG);

	DEST->DEST_COUNT++;				// COUNT THEM

	L4FRAMESTX++;

	if (FRAGFLAG)
	{
		//	MESSAGE WAS TOO BIG - ADJUST IT AND LOOP BACK

		Msg->LENGTH -= 236;

		memmove(Msg->L2DATA, &Msg->L2DATA[236], Msg->LENGTH - 8);
	
		SENDL4MESSAGE(L4, Msg);
	}
}


int GETBUSYBIT(TRANSPORTENTRY * L4)
{
	//	SEE IF CROSSSESSION IS BUSY

	L4->NAKBITS &= ~L4BUSY;		// Clear busy
			
	L4->NAKBITS |= CHECKIFBUSYL4(L4);		// RETURNS AL WITH BUSY BIT SET IF CROSSSESSION IS BUSY
	
	return L4->NAKBITS;
}

VOID Q_IP_MSG(PDATAMESSAGE Buffer)
{
	if (IPHOSTVECTOR.HOSTAPPLFLAGS & 0x80)
	{
		//	CHECK WE ARENT USING TOO MANY BUFFERS
		
		if (C_Q_COUNT(&IPHOSTVECTOR.HOSTTRACEQ) > 20)
			ReleaseBuffer(Q_REM(&IPHOSTVECTOR.HOSTTRACEQ));

		C_Q_ADD(&IPHOSTVECTOR.HOSTTRACEQ, Buffer);
		return;
	}

	ReleaseBuffer(Buffer);
}

VOID SENDL4CONNECT(TRANSPORTENTRY * Session)
{
	PL3MESSAGEBUFFER MSG = (PL3MESSAGEBUFFER)GetBuff();
	struct DEST_LIST * DEST = Session->L4TARGET.DEST;

	if (MSG == NULL)
		return;

	if (DEST->DEST_CALL[0] == 0)
	{
		Debugprintf("Trying to send L4CREEQ to NULL Destination");
		ReleaseBuffer(MSG);
		return;
	}

	MSG->L3PID = 0xCF;			// NET MESSAGE
	
	memcpy(MSG->L3SRCE, Session->L4MYCALL, 7);
	memcpy(MSG->L3DEST, DEST->DEST_CALL, 7);

	MSG->L3TTL = L3LIVES;

	MSG->L4INDEX = Session->CIRCUITINDEX;
	MSG->L4ID = Session->CIRCUITID;
	MSG->L4TXNO = 0;
	MSG->L4RXNO = 0;
	MSG->L4FLAGS = L4CREQ;

	MSG->L4DATA[0] = L4DEFAULTWINDOW;	// PROPOSED WINDOW

	memcpy(&MSG->L4DATA[1], Session->L4USER, 7);		// ORIG CALL
	memcpy(&MSG->L4DATA[8], Session->L4MYCALL, 7);
	
	Session->L4TIMER = Session->SESSIONT1;				// START TIMER
	memcpy(&MSG->L4DATA[15], &Session->SESSIONT1, 2);	// AND PUT IN MSG

	MSG->LENGTH = &MSG->L4DATA[17] - (UCHAR *)MSG;

	if (Session->SPYFLAG)
	{
		MSG->L4DATA[17] = 'Z';							// ADD SPY ON BBS FLAG
		MSG->LENGTH++;
	}
	C_Q_ADD(&DEST->DEST_Q, (UINT *)MSG);
}

void RETURNEDTONODE(TRANSPORTENTRY * Session)
{
	//	SEND RETURNED TO ALIAS:CALL

	struct DATAMESSAGE * Msg = (struct DATAMESSAGE *)GetBuff();
	char Nodename[20];

	if (Msg)
	{
		Msg->PID = 0xf0;

		Nodename[DecodeNodeName(MYCALLWITHALIAS, Nodename)] = 0;		// null terminate

		Msg->LENGTH = sprintf(&Msg->L2DATA[0], "Returned to Node %s\r", Nodename) + 8 ;
		C_Q_ADD(&Session->L4TX_Q, (UINT *)Msg);
		PostDataAvailable(Session);
	}
}


extern void * BUFFER;

VOID L4BG()
{
	// PROCESS DATA QUEUED ON SESSIONS

	int n = MAXCIRCUITS;
	TRANSPORTENTRY * L4 = L4TABLE;
	int MaxLinks = MAXLINKS;
	UCHAR Outstanding;
	struct DATAMESSAGE * Msg;
	struct PORTCONTROL * PORT;
	struct _LINKTABLE * LINK;

	while (n--)
	{
		if (L4->L4USER[0] == 0)
		{
			L4++;
			continue;
		}
		while (L4->L4TX_Q)	
		{
			if (L4->L4CIRCUITTYPE & BPQHOST)
				break;							// Leave on TXQ

			//	SEE IF BUSY - NEED DIFFERENT TESTS FOR EACH SESSION TYPE

			if (L4->L4CIRCUITTYPE & SESSION)
			{
				//	L4 SESSION - WILL NEED BUFFERS FOR SAVING COPY,
				//	 AND POSSIBLY FRAGMENTATION

				if (QCOUNT < 15)
					break;

				if (L4->FLAGS & L4BUSY)
				{
					//	CHOKED - MAKE SURE TIMER IS RUNNING

					if (L4->L4TIMER == 0)
						L4->L4TIMER = L4->SESSIONT1;

					break;
				}
				
				//	CHECK WINDOW

				Outstanding = L4->TXSEQNO - L4->L4WS;	// LAST FRAME ACKED - GIVES NUMBER OUTSTANING

				//	MOD 256, SO SHOULD HANDLE WRAP??

				if (Outstanding > L4->L4WINDOW)
					break;

			}
			else if (L4->L4CIRCUITTYPE & L2LINK)
			{
				// L2 LINK 

				LINK = L4->L4TARGET.LINK;

				if (COUNT_AT_L2(LINK) > 5)
					break;
			}

			// Not busy, so continue

			L4->L4KILLTIMER = 0;		//RESET SESSION TIMEOUTS

			if(L4->L4CROSSLINK)
				L4->L4CROSSLINK->L4KILLTIMER = 0;

			Msg = Q_REM(&L4->L4TX_Q);

			if (L4->L4CIRCUITTYPE & PACTOR)
			{
				//	PACTOR-like - queue to Port

				//  Stream Number is in KAMSESSION

				Msg->PORT = L4->KAMSESSION;
				PORT = L4->L4TARGET.PORT;

				C_Q_ADD(&PORT->PORTTX_Q, Msg);

				continue;
			}
			//	non-pactor

			//	IF CROSSLINK, QUEUE TO NEIGHBOUR, ELSE QUEUE ON LINK ENTRY

			if (L4->L4CIRCUITTYPE & SESSION)
			{
				SENDL4MESSAGE(L4, Msg);
				ReleaseBuffer(Msg);
				break;
			}
	
			LINK = L4->L4TARGET.LINK;
			C_Q_ADD(&LINK->TX_Q, Msg);
		}

		// if nothing on TX Queue If there is stuff on hold queue, timer must be running

//		if (L4->L4TX_Q == 0 && L4->L4HOLD_Q)
		if (L4->L4HOLD_Q)
		{
			if (L4->L4TIMER == 0)
			{
				L4->L4TIMER = L4->SESSIONT1;
			}
		}
		
		// now check for rxed frames

		while(L4->L4RX_Q)
		{
			Msg = Q_REM(&L4->L4RX_Q);

			IFRM150(L4, Msg);

			if (L4->L4USER[0] == 0)		// HAVE JUST CLOSED SESSION!	
				goto NextSess;
		}

		//	IF ACK IS PENDING, AND WE ARE AT RX WINDOW, SEND ACK NOW

		Outstanding = L4->RXSEQNO - L4->L4LASTACKED;
		if (Outstanding >= L4->L4WINDOW)
			SENDL4IACK(L4);
NextSess:
		L4++;
	}
}

VOID CLEARSESSIONENTRY(TRANSPORTENTRY * Session)
{

	//	RETURN ANY QUEUED BUFFERS TO FREE QUEUE

	while (Session->L4TX_Q)
		ReleaseBuffer(Q_REM(&Session->L4TX_Q));

	while (Session->L4RX_Q)
		ReleaseBuffer(Q_REM(&Session->L4RX_Q));

	while (Session->L4HOLD_Q)
		ReleaseBuffer(Q_REM(&Session->L4HOLD_Q));

	if (C_Q_COUNT(&Session->L4RESEQ_Q) > Session->L4WINDOW)
	{
		Debugprintf("Corrupt RESEQ_Q Q Len %d Free Buffs %d", C_Q_COUNT(&Session->L4RESEQ_Q), QCOUNT);
		Session->L4RESEQ_Q = 0;
	}

	while (Session->L4RESEQ_Q)
		ReleaseBuffer(Q_REM(&Session->L4RESEQ_Q));

	memset(Session, 0, sizeof(TRANSPORTENTRY));
}

VOID CloseSessionPartner(TRANSPORTENTRY * Session)
{
	//	SEND CLOSE TO CROSSLINKED SESSION AND CLEAR LOCAL SESSION

	if (Session == NULL)
		return;

	if (Session->L4CROSSLINK)
		CLOSECURRENTSESSION(Session->L4CROSSLINK);

	CLEARSESSIONENTRY(Session);
}


VOID CLOSECURRENTSESSION(TRANSPORTENTRY * Session)
{
	MESSAGE * Buffer;
	struct _LINKTABLE * LINK;

//	SHUT DOWN SESSION AND UNLINK IF CROSSLINKED

	if (Session == NULL)
		return;

	Session->L4CROSSLINK = NULL;

	//	IF STAY FLAG SET, KEEP SESSION, AND SEND MESSAGE

	if (Session->STAYFLAG)
	{
		RETURNEDTONODE(Session);
		Session->STAYFLAG = 0;			// Only do it once
		return;
	}

	if (Session->L4CIRCUITTYPE & BPQHOST)
	{
		//	BPQ HOST MODE SESSION - INDICATE STATUS CHANGE

		PBPQVECSTRUC HOST = Session->L4TARGET.HOST;
		HOST->HOSTSESSION = 0;
		HOST->HOSTFLAGS |= 3;		/// State Change

		PostStateChange(Session);
		CLEARSESSIONENTRY(Session);
		return;
	}

	if (Session->L4CIRCUITTYPE & PACTOR)
	{
		//	PACTOR-type (Session linked to Port)

		struct _EXTPORTDATA * EXTPORT = Session->L4TARGET.EXTPORT;

		// If any data is queued, move it to the port entry, so it can be sent before the disconnect
		
		while (Session->L4TX_Q)
		{
			Buffer = Q_REM(&Session->L4TX_Q);
			EXTPORT->PORTCONTROL.PORTTXROUTINE(EXTPORT, Buffer);
		}

		EXTPORT->ATTACHEDSESSIONS[Session->KAMSESSION] = NULL;

		CLEARSESSIONENTRY(Session);
		return;
	}

	if (Session->L4CIRCUITTYPE & SESSION)
	{
		//	L4 SESSION TO CLOSE

		if (Session->L4HOLD_Q || Session->L4TX_Q)	// WAITING FOR ACK or MORE TO SEND - SEND DISC LATER
		{
			Session->FLAGS |= DISCPENDING;			// SEND DISC WHEN ALL DATA ACKED
			return;
		}

		SENDL4DISC(Session);
		return;
	}

	//	Must be LEVEL 2 SESSION TO CLOSE

	//	COPY ANY PENDING DATA TO L2 TX Q, THEN GET RID OF SESSION

	LINK = Session->L4TARGET.LINK;

	if (LINK == NULL)			// just in case!
	{
		CLEARSESSIONENTRY(Session);
		return;
	}

	while (Session->L4TX_Q)
	{
		Buffer = Q_REM(&Session->L4TX_Q);
		C_Q_ADD(&LINK->TX_Q, Buffer);
	}

	//	NOTHING LEFT AT SESSION LEVEL
	
	LINK->CIRCUITPOINTER = NULL;	// CLEAR REVERSE LINK

	if ((LINK->LINKWS != LINK->LINKNS) || LINK->TX_Q)
	{
		// STILL MORE TO SEND - SEND DISC LATER
		
		LINK->L2FLAGS |= DISCPENDING;	// SEND DISC WHEN ALL DATA ACKED
	}
	else
	{
		//	NOTHING QUEUED - CAN SEND DISC NOW

		LINK->L2STATE = 4;				// DISCONNECTING
		LINK->L2TIMER = 1;				// USE TIMER TO KICK OFF DISC
	}

	CLEARSESSIONENTRY(Session);

}

VOID L4TimerProc()
{
	//	CHECK FOR TIMER EXPIRY

	int n = MAXCIRCUITS;
	TRANSPORTENTRY * L4 = L4TABLE;
	TRANSPORTENTRY * Partner;
	int MaxLinks = MAXLINKS;

	while (n--)
	{
		if (L4->L4USER[0] == 0)
		{
			L4++;
			continue;
		}
		
		//	CHECK FOR L4BUSY SET AND NO LONGER BUSY

		if (L4->NAKBITS & L4BUSY)
		{
			if ((CHECKIFBUSYL4(L4) & L4BUSY) == 0)
			{
				// no longer busy

				L4->NAKBITS &= ~L4BUSY;		// Clear busy
				L4->L4ACKREQ = 1;			// SEND ACK
			}
		}

		if (L4->L4TIMER)					// Timer Running?
		{
			L4->L4TIMER--;
			if (L4->L4TIMER == 0)			// Expired
				L4TIMEOUT(L4);	
		}
	
		if (L4->L4ACKREQ)					// DELAYED ACK Timer Running?
		{
			L4->L4ACKREQ--;
			if (L4->L4ACKREQ == 0)			// Expired
				SENDL4IACK(L4);
		}

		L4->L4KILLTIMER++;

		//	IF BIT 6 OF APPLFLAGS SET, SEND MSG EVERY 11 MINS TO KEEP SESSION OPEN

		if (L4->L4CROSSLINK)			// CONNECTED?
			if (L4->SESS_APPLFLAGS & 0x40)
				if (L4->L4KILLTIMER > 11 * 60)
					AUTOTIMER(L4);

		if (L4->L4LIMIT == 0)
			L4->L4LIMIT = L4LIMIT;
		else
		{
			if (L4->L4KILLTIMER > L4->L4LIMIT)
			{
				L4->L4KILLTIMER = 0;
				
				//	CLOSE THIS SESSION, AND ITS PARTNER (IF ANY)

				L4->STAYFLAG = 0;

				Partner = L4->L4CROSSLINK;
				CLOSECURRENTSESSION(L4);
				
				if (Partner)
				{
					Partner->L4KILLTIMER = 0;		//ITS TIMES IS ALSO ABOUT TO EXPIRE
					CLOSECURRENTSESSION(Partner);	// CLOSE THIS ONE
				}
			}
		}
		L4++;
	}
}

VOID L4TIMEOUT(TRANSPORTENTRY * L4)
{
	//	TIMER EXPIRED

	//	    IF LINK UP REPEAT TEXT
	//	    IF S2, REPEAT CONNECT REQUEST
	//	    IF S4, REPEAT DISCONNECT REQUEST

	struct DATAMESSAGE * Msg;
	struct DATAMESSAGE * Copy;
	struct DEST_LIST * DEST;

	if (L4->L4STATE < 2)
		return;

	if (L4->L4STATE == 2)
	{
		//	RETRY CONNECT

		L4->L4RETRIES++;

		if (L4->L4RETRIES > L4N2)
		{
			//	RETRIED N2 TIMES - FAIL LINK

			L4CONNECTFAILED(L4);		// TELL OTHER PARTNER IT FAILED
			CLEARSESSIONENTRY(L4);
			return;
		}

		Debugprintf("Retrying L4 Connect Request");

		SENDL4CONNECT(L4);				// Resend connect
		return;
	}

	if (L4->L4STATE == 4)
	{
		//	RETRY DISCONNECT

		L4->L4RETRIES++;

		if (L4->L4RETRIES > L4N2)
		{
			//	RETRIED N2 TIMES - FAIL LINK


			CLEARSESSIONENTRY(L4);
			return;
		}

		SENDL4DISC(L4);				// Resend connect
		return;
	}
	//	STATE 5 OR ABOVE - RETRY INFO


	L4->FLAGS &= ~L4BUSY;		// CANCEL CHOKE
	
	L4->L4RETRIES++;

	if (L4->L4RETRIES > L4N2)
	{
		//	RETRIED N2 TIMES - FAIL LINK

		CloseSessionPartner(L4);	// SEND CLOSE TO PARTNER (IF PRESENT)
		return;
	}

	//	RESEND ALL OUTSTANDING FRAMES

	L4->FLAGS &= 0x7F;				// CLEAR CHOKED

	Msg = L4->L4HOLD_Q;

	while (Msg)
	{ 
		Copy = GetBuff();

		if (Copy == 0)
			return;

		memcpy(Copy, Msg, Msg->LENGTH);

		DEST = L4->L4TARGET.DEST;

		C_Q_ADD(&DEST->DEST_Q, Copy);
	
		L4FRAMESRETRIED++;

		Msg = Msg->CHAIN;
	}
}

VOID AUTOTIMER(TRANSPORTENTRY * L4)
{
	//	SEND MESSAGE TO USER TO KEEP CIRCUIT OPEN

	struct DATAMESSAGE * Msg = GetBuff();

	if (Msg == 0)
		return;

	Msg->PID = 0xf0;
	Msg->L2DATA[0] = 0;
	Msg->L2DATA[1] = 0;

	Msg->LENGTH = MSGHDDRLEN + 3;

	C_Q_ADD(&L4->L4TX_Q, Msg);
	
	PostDataAvailable(L4);

	L4->L4KILLTIMER = 0;
	
	if (L4->L4CROSSLINK)
		L4->L4CROSSLINK->L4KILLTIMER = 0;
}

VOID L4CONNECTFAILED(TRANSPORTENTRY * L4)
{
	//	CONNECT HAS TIMED OUT - SEND MESSAGE TO OTHER END

	struct DATAMESSAGE * Msg;
	TRANSPORTENTRY * Partner;
	UCHAR * ptr1;
	char Nodename[20];
	struct DEST_LIST * DEST;

	Partner = L4->L4CROSSLINK;

	if (Partner == 0)
		return;

	Msg = GetBuff();

	if (Msg == 0)
		return;

	Msg->PID = 0xf0;

	ptr1 = SetupNodeHeader(Msg);

	DEST = L4->L4TARGET.DEST;
	Nodename[DecodeNodeName(DEST->DEST_CALL, Nodename)] = 0;		// null terminate

	ptr1 += sprintf(ptr1, "Failure with %s\r", Nodename);

	Msg->LENGTH = ptr1 - (UCHAR *)Msg;

	C_Q_ADD(&Partner->L4TX_Q, Msg);

	PostDataAvailable(Partner);

	Partner->L4CROSSLINK = 0;		// Back to command lewel
}


VOID ProcessIframe(struct _LINKTABLE * LINK, PDATAMESSAGE Buffer)
{
	//	IF UP/DOWN LINK, AND CIRCUIT ESTABLISHED, ADD LEVEL 3/4 HEADERS
	//	   (FRAGMENTING IF NECESSARY), AND PASS TO TRANSPORT CONTROL
	//	   FOR ESTABLISHED ROUTE

	//	IF INTERNODE MESSAGE, PASS TO ROUTE CONTROL 

	//	IF UP/DOWN, AND NO CIRCUIT, PASS TO COMMAND HANDLER

	TRANSPORTENTRY * Session;

	//	IT IS POSSIBLE TO MULTIPLEX NETROM AND IP STUFF ON THE SAME LINK

	if (Buffer->PID == 0xCC || Buffer->PID == 0xCD)
	{
		Q_IP_MSG(Buffer);
		return;
	}

	if (Buffer->PID ==	0xCF)
	{
		//	INTERNODE frame

		//	IF LINKTYPE IS NOT 3, MUST CHECK IF WE HAVE ACCIDENTALLY  ATTACHED A BBS PORT TO THE NODE

		if (LINK->LINKTYPE != 3)
		{
			if (LINK->CIRCUITPOINTER)
			{
				//	MUST KILL SESSION

				InformPartner(LINK);			// CLOSE IT
				LINK->CIRCUITPOINTER = NULL;	// AND UNHOOK
			}
			LINK->LINKTYPE = 3;					// NOW WE KNOW ITS A CROSSLINK
		}

		NETROMMSG(LINK, (L3MESSAGEBUFFER *)Buffer);
		return;
	}

	if (LINK->LINKTYPE == 3)
	{
		// Weve receved a non- netrom frame on an inernode link

		ReleaseBuffer(Buffer);
		return;
	}

	if (LINK->CIRCUITPOINTER)
	{
		// Pass to Session 
		
		IFRM150(LINK->CIRCUITPOINTER, Buffer);
		return;
	}

	//	UPLINK MESSAGE WITHOUT LEVEL 4 ENTRY - CREATE ONE

	Session = SetupSessionForL2(LINK);

	if (Session == NULL)
		return;

	CommandHandler(Session, Buffer);
	return;
}


VOID IFRM100(struct _LINKTABLE * LINK, PDATAMESSAGE Buffer)
{
	TRANSPORTENTRY * Session;
	
	if (LINK->CIRCUITPOINTER)
	{
		// Pass to Session 
		
		IFRM150(LINK->CIRCUITPOINTER, Buffer);
		return;
	}

	//	UPLINK MESSAGE WITHOUT LEVEL 4 ENTRY - CREATE ONE

	Session = SetupSessionForL2(LINK);

	if (Session == NULL)
		return;

	CommandHandler(Session, Buffer);
	return;
}


VOID IFRM150(TRANSPORTENTRY * Session, PDATAMESSAGE Buffer)
{
	TRANSPORTENTRY * Partner;
	struct _LINKTABLE * LINK;
	
	Session->L4KILLTIMER = 0;			// RESET SESSION TIMEOUT

	if (Session->L4CROSSLINK == NULL)			// CONNECTED?
	{
		//	NO, SO PASS TO COMMAND HANDLER
		
		CommandHandler(Session, Buffer);
		return;
	}

 	Partner = Session->L4CROSSLINK;			// TO SESSION PARTNER

	if (Partner->L4STATE == 5)
	{
		C_Q_ADD(&Partner->L4TX_Q, Buffer);
		PostDataAvailable(Partner);
		return;
	}



	//	MESSAGE RECEIVED BEFORE SESSION IS UP - CANCEL SESSION
	//	  AND PASS MESSAGE TO COMMAND HANDLER

	if (Partner->L4CIRCUITTYPE & L2LINK)		// L2 SESSION?
	{
		//	MUST CANCEL L2 SESSION

		LINK = Partner->L4TARGET.LINK;
		LINK->CIRCUITPOINTER = NULL;	// CLEAR REVERSE LINK

		LINK->L2STATE = 4;				// DISCONNECTING
		LINK->L2TIMER = 1;				// USE TIMER TO KICK OFF DISC

		LINK->L2RETRIES = LINK->LINKPORT->PORTN2 - 2;	//ONLY SEND DISC ONCE
	}

	CLEARSESSIONENTRY(Partner);

	Session->L4CROSSLINK = 0;		// CLEAR CROSS LINK
	CommandHandler(Session, Buffer);
	return;
}


VOID SENDL4DISC(TRANSPORTENTRY * Session)
{
	L3MESSAGEBUFFER * MSG;
	struct DEST_LIST * DEST = Session->L4TARGET.DEST;

	if (Session->L4STATE < 4)
	{
		//	CIRCUIT NOT UP OR CLOSING - PROBABLY NOT YET SET UP - JUST ZAP IT

		CLEARSESSIONENTRY(Session);
		return;
	}
	
	Session->L4TIMER = Session->SESSIONT1;	// START TIMER
	Session->L4STATE = 4;					// SET DISCONNECTING
	Session->L4ACKREQ = 0;					// CANCEL ACK NEEDED

	MSG = GetBuff();

	if (MSG == NULL)
		return;

	MSG->L3PID = 0xCF;			// NET MESSAGE
	
	memcpy(MSG->L3SRCE, Session->L4MYCALL, 7);
	memcpy(MSG->L3DEST, DEST->DEST_CALL, 7);
	MSG->L3TTL = L3LIVES;
	MSG->L4INDEX = Session->FARINDEX;
	MSG->L4ID = Session->FARID;
	MSG->L4TXNO = 0;
	MSG->L4FLAGS = L4DREQ;

	MSG->LENGTH = &MSG->L4DATA[0] - (UCHAR *)MSG;

	C_Q_ADD(&DEST->DEST_Q, (UINT *)MSG);
}


void WriteL4LogLine(UCHAR * mycall, UCHAR * call, UCHAR * node)
{
	UCHAR FN[MAX_PATH];
	FILE * L4LogHandle;
	time_t T;
	struct tm * tm;

	char Call1[12], Call2[12], Call3[12];

	char LogMsg[256];	
	int MsgLen;
	
	Call1[ConvFromAX25(mycall, Call1)] = 0;
	Call2[ConvFromAX25(call, Call2)] = 0;
	Call3[ConvFromAX25(node, Call3)] = 0;


	T = time(NULL);
	tm = gmtime(&T);	

	sprintf(FN,"%s\\L4Log_%02d%02d.txt", BPQDirectory, tm->tm_mon + 1, tm->tm_mday);

	L4LogHandle = fopen(FN, "ab");

	if (L4LogHandle == NULL)
		return;

	MsgLen = sprintf(LogMsg, "%02d:%02d:%02d Call to %s from %s at Node %s\r\n", tm->tm_hour, tm->tm_min, tm->tm_sec, Call1, Call2, Call3);

	fwrite(LogMsg , 1, MsgLen, L4LogHandle);

	fclose(L4LogHandle);
}


VOID CONNECTREQUEST(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * L3MSG, UINT ApplMask, UCHAR * ApplCall)
{
	//	CONNECT REQUEST - SEE IF EXISTING SESSION
	//	IF NOT, GET AND FORMAT SESSION TABLE ENTRY
	//	SEND CONNECT ACK

	//	EDI = _BUFFER, EBX = LINK

	TRANSPORTENTRY * L4;
	int BPQNODE = 0;				// NOT ONE OF MINE
	char BPQPARAMS[10];				// Extended Connect Params from BPQ Node
	int CONERROR;
	int Index;

	memcpy(BPQPARAMS, &L4T1, 2);	// SET DEFAULT T1 IN CASE NOT FROM ANOTHER BPQ NODE

	BPQPARAMS[2] = 0;				// 'SPY' NOT SET
										
	if (FINDCIRCUIT(L3MSG, &L4, &Index))
	{
		// SESSION EXISTS - ASSUME RETRY AND SEND ACK

		SendConACK(LINK, L4, L3MSG, BPQNODE, ApplMask, ApplCall);
		return;
	}


	if (L4 == 0)
	{
		SendConNAK(LINK, L3MSG);
		return;
	}

	L4->CIRCUITINDEX = Index;
	
	SETUPNEWCIRCUIT(LINK, L3MSG, L4, BPQPARAMS, ApplMask, &BPQNODE);

	if (L4->L4TARGET.DEST == 0)
	{
		// NODE NOT IN TABLE, AND TABLE FULL - CANCEL IT

		memset(L4, 0, sizeof (TRANSPORTENTRY));
		SendConNAK(LINK, L3MSG);
		return;
	}
	//	IF CONNECT TO APPL, ALLOCATE BBS PORT

	if (ApplMask == 0 || BPQPARAMS[2] == 'Z')		// Z is "Spy" Connect
	{
		SendConACK(LINK, L4, L3MSG, BPQNODE, ApplMask, ApplCall);

		return;
	}

	//	IF APPL CONNECT, SEE IF APPL HAS AN ALIAS


	if (ALIASPTR[0] > ' ')
	{
		struct DATAMESSAGE * Msg;

		//	ACCEPT THE CONNECT, THEN INVOKE THE ALIAS

		SendConACK(LINK, L4, L3MSG, BPQNODE, ApplMask, ApplCall);

		Msg = GetBuff();

		if (Msg)
		{
			Msg->PID = 0xf0;
				
			memcpy(Msg->L2DATA, ALIASPTR, ALIASLEN);
			Msg->L2DATA[ALIASLEN] = 13;
			
			Msg->LENGTH = MSGHDDRLEN + ALIASLEN + 2;		// 2 for PID and CR

			C_Q_ADD(&L4->L4RX_Q, Msg);
			return;
		}
	}

	if (cATTACHTOBBS(L4, ApplMask, PACLEN, &CONERROR))
	{
		SendConACK(LINK, L4, L3MSG, BPQNODE, ApplMask, ApplCall);
		return;
	}
	
	//	NO BBS AVAILABLE
	
	CLEARSESSIONENTRY(L4);
	SendConNAK(LINK, L3MSG);
	return;
}

VOID SendConACK(struct _LINKTABLE * LINK, TRANSPORTENTRY * L4, L3MESSAGEBUFFER * L3MSG, BOOL BPQNODE, UINT Applmask, UCHAR * ApplCall)
{
	//	SEND CONNECT ACK	
	
	L3MSG->L4TXNO = L4->CIRCUITINDEX;
	L3MSG->L4RXNO = L4->CIRCUITID;

	L3MSG->L4DATA[0] = L4->L4WINDOW;			//WINDOW

	L3MSG->L4FLAGS = L4CACK;

	if (LogL4Connects)
		WriteL4LogLine(ApplCall, L4->L4USER, L3MSG->L3SRCE);

	if (CTEXTLEN && (Applmask == 0) && FULL_CTEXT)	// Any connect, or call to alias
	{
		struct DATAMESSAGE * Msg;
		int Totallen = CTEXTLEN;
		int Paclen= PACLEN;
		UCHAR * ptr = CTEXTMSG;

		if (Paclen == 0)
			Paclen = PACLEN;

		while(Totallen)
		{
			Msg = GetBuff();

			if (Msg == NULL)
				break;				// No Buffers

			Msg->PID = 0xf0;

			if (Paclen > Totallen)
				Paclen = Totallen;
			
			memcpy(Msg->L2DATA, ptr, Paclen);
			Msg->LENGTH = Paclen + MSGHDDRLEN + 1;

			C_Q_ADD(&L4->L4TX_Q, Msg);			// SEND MESSAGE TO CALLER
			PostDataAvailable(L4);
			ptr += Paclen;
			Totallen -= Paclen;
		}
	}

	L3SWAPADDRESSES(L3MSG);
	
	L3MSG->L3TTL = L3LIVES;

	L3MSG->LENGTH = MSGHDDRLEN + 22;		// CTL 20 BYTE Header Window

	if (BPQNODE)
	{
		L3MSG->L4DATA[1] = L3LIVES;		// Our TTL
		L3MSG->LENGTH++;
	}

	C_Q_ADD(&LINK->TX_Q, L3MSG);
}

int FINDCIRCUIT(L3MESSAGEBUFFER * L3MSG, TRANSPORTENTRY ** REQL4, int * NewIndex)
{
	//	FIND CIRCUIT FOR AN INCOMING MESSAGE

	TRANSPORTENTRY * L4 = L4TABLE;
	TRANSPORTENTRY * FIRSTSPARE = NULL;
	struct DEST_LIST * DEST;

	int Index = 0;
	
	while (Index < MAXCIRCUITS)
	{
		if (L4->L4USER[0] == 0)		// Spare
		{
			if (FIRSTSPARE == NULL)
			{			
				FIRSTSPARE = L4;
				*NewIndex = Index;
			}

			L4++;
			Index++;
			continue;
		}

		DEST = L4->L4TARGET.DEST;

		if (DEST == NULL)
		{
			// L4 entry without a Dest shouldn't happen. (I don't think!)

			char Call1[12], Call2[12];

			Call1[ConvFromAX25(L4->L4USER, Call1)] = 0;
			Call2[ConvFromAX25(L4->L4MYCALL, Call2)] = 0;

			Debugprintf("L4 entry without Target. Type = %02x Calls %s %s",
				L4->L4CIRCUITTYPE, Call1, Call2);

			L4++;
			Index++;
			continue;
		}
		
		if (CompareCalls(L3MSG->L3SRCE, DEST->DEST_CALL))
		{
			if (L4->FARID == L3MSG->L4ID && L4->FARINDEX == L3MSG->L4INDEX)
			{
				// Found it
				
				*REQL4 = L4;
				return TRUE;
			}
		}
		L4++;
		Index++;
	}

	//	ENTRY NOT FOUND - FIRSTSPARE HAS FIRST FREE ENTRY, OR ZERO IF TABLE FULL

	*REQL4 = FIRSTSPARE;
	return FALSE;
}

VOID L3SWAPADDRESSES(L3MESSAGEBUFFER * L3MSG)
{
	//	EXCHANGE ORIGIN AND DEST

	char Temp[7];

	memcpy(Temp, L3MSG->L3SRCE, 7);
	memcpy(L3MSG->L3SRCE, L3MSG->L3DEST, 7);
	memcpy(L3MSG->L3DEST, Temp, 7);

	L3MSG->L3DEST[6] &= 0x1E;		// Mack EOA and CMD
	L3MSG->L3SRCE[6] &= 0x1E;
	L3MSG->L3SRCE[6] |= 1;			// Set Last Call
}

VOID SendConNAK(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * L3MSG)
{
	L3MSG->L4FLAGS = L4CACK | L4BUSY;			// REJECT
	L3MSG->L4DATA[0] = 0;						// WINDOW

	L3SWAPADDRESSES(L3MSG);	
	L3MSG->L3TTL = L3LIVES;

	C_Q_ADD(&LINK->TX_Q, L3MSG);
}

VOID SETUPNEWCIRCUIT(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * L3MSG, 
					 TRANSPORTENTRY * L4, char * BPQPARAMS, int ApplMask, int * BPQNODE)
{
	struct DEST_LIST * DEST;
	int Maxtries = 2;					// Just in case

	L4->FARINDEX = L3MSG->L4INDEX;
	L4->FARID = L3MSG->L4ID;

	// Index set by caller

	L4->CIRCUITID = NEXTID;

	NEXTID++;
		if (NEXTID == 0)
			NEXTID++;								// kEEP nON-ZERO

	L4->SESSIONT1 = L4T1;
	
	L4->L4WINDOW = (UCHAR)L4DEFAULTWINDOW;

	if (L3MSG->L4DATA[0] > L4DEFAULTWINDOW)
		L4->L4WINDOW = L3MSG->L4DATA[0];
		
	memcpy(L4->L4USER, &L3MSG->L4DATA[1], 7);		// Originator's call from Call Request
	
	if (ApplMask)
	{
		// Should get APPLCALL if set ( maybe ???????????????
	}

//	MOV	ESI,APPLCALLTABLEPTR
//	LEA	ESI,APPLCALL[ESI]

	memcpy(L4->L4MYCALL, MYCALL, 7);

	//	GET BPQ EXTENDED CONNECT PARAMS IF PRESENT

	if (L3MSG->LENGTH  == MSGHDDRLEN + 38 || L3MSG->LENGTH  == MSGHDDRLEN + 39)
	{
		*BPQNODE = 1;
		memcpy(BPQPARAMS, &L3MSG->L4DATA[15],L3MSG->LENGTH - (MSGHDDRLEN + 36));
	}

	L4->L4CIRCUITTYPE = SESSION | UPLINK;	
	L4->L4STATE = 5;
	
TryAgain:

	DEST = CHECKL3TABLES(LINK, L3MSG);

	L4->L4TARGET.DEST = DEST;

	if (DEST == 0)
	{
		int WorstQual = 256;
		struct DEST_LIST * WorstDest = NULL;
		int n = MAXDESTS;

		//	Node not it table and table full

		//	Replace worst quality node with session counts of zero

		DEST = DESTS;

		while (n--)
		{
			if (DEST->DEST_COUNT == 0 && DEST->DEST_RTT == 0)		// Not used and not INP3
			{
				if (DEST->NRROUTE[0].ROUT_QUALITY < WorstQual)
				{
					WorstQual = DEST->NRROUTE[0].ROUT_QUALITY;
					WorstDest = DEST;
				}
			}
			DEST++;
		}
		
		if (WorstDest)
		{
			REMOVENODE(WorstDest);
			if (Maxtries--)
				goto TryAgain;			// We now have a spare (but protect against loop if something amiss)
		}

		// Nothing to delete, so just ignore connect

		return;
	}

	if (*BPQNODE)
	{	
		SHORT T1;
		
		DEST->DEST_STATE |= 0x40;			// SET BPQ _NODE BIT
		memcpy((char *)&T1, BPQPARAMS, 2);

		if (T1 > 300)
			L4->SESSIONT1 = L4T1;
		else
			L4->SESSIONT1 = T1;
	}
	else
		L4->SESSIONT1 = L4T1;			// DEFAULT TIMEOUT
	
	L4->SESSPACLEN = PACLEN;			// DEFAULT
}


int CHECKIFBUSYL4(TRANSPORTENTRY * L4)
{
	//	RETURN TOP BIT OF AL SET IF SESSION PARTNER IS BUSY

	int Count;

	if (L4->L4CROSSLINK)		// CONNECTED?
		Count = CountFramesQueuedOnSession(L4->L4CROSSLINK);
	else
		Count = CountFramesQueuedOnSession(L4);

	if (Count < L4->L4WINDOW)
		return 0;
	else
		return L4BUSY;
}

VOID FRAMEFORUS(struct _LINKTABLE * LINK, L3MESSAGEBUFFER * L3MSG, int ApplMask, UCHAR * ApplCall)
{
	//	INTERNODE LINK

	TRANSPORTENTRY * L4;
	struct DEST_LIST * DEST;
	int Opcode;
	char Nodename[20];
	char ReplyText[20];
	struct DATAMESSAGE * Msg;
	TRANSPORTENTRY * Partner;
	UCHAR * ptr1;
	int FramesMissing;
	L3MESSAGEBUFFER * Saved;
	L3MESSAGEBUFFER ** Prev;
	char Call[10];

	L4FRAMESRX++;

	Opcode = L3MSG->L4FLAGS & 15;

	switch (Opcode)
	{
	case 0:

		//	OPCODE 0 is used for a variety of functions, using L4INDEX and L4ID as qualifiers
		//	0c0c is used for IP

		if (L3MSG->L4ID == 0x0C && L3MSG->L4INDEX == 0x0C)
		{
			Q_IP_MSG((PDATAMESSAGE)L3MSG);
			return;
		}

		//	 00 01 Seesm to be Netrom Record Route

		if (L3MSG->L4ID == 1 && L3MSG->L4INDEX == 0)
		{
			NRRecordRoute((char *)L3MSG, L3MSG->LENGTH);
			return;
		}

		ReleaseBuffer(L3MSG);
		return;

	case L4CREQ:

		CONNECTREQUEST(LINK, L3MSG, ApplMask, ApplCall);
		return;
	}
		
	//	OTHERS NEED A SESSION

	L4 = &L4TABLE[L3MSG->L4INDEX];

	if (L4->CIRCUITID!= L3MSG->L4ID)
	{
		ReleaseBuffer(L3MSG);
		return;
	}

	if ((L4->L4CIRCUITTYPE & SESSION) == 0)
	{
		// Not an L4 Session - must be an old connection

		ReleaseBuffer(L3MSG);
		return;
	}

	//	HAVE FOUND CORRECT SESSION ENTRY

	switch (Opcode)
	{
	case L4CACK:

		//	CONNECT ACK
	
		DEST = L4->L4TARGET.DEST;
	
		//	EXTRACT EXTENDED PARAMS IF PRESENT

		if (L3MSG->LENGTH > MSGHDDRLEN + 22)		// Standard Msg
		{
			DEST->DEST_STATE &= 0x80;
			DEST->DEST_STATE |= (L3MSG->L4DATA[1] - L3MSG->L3TTL) + 0x41; // Hops to dest + x40
		}

		Partner = L4->L4CROSSLINK;

		if (L3MSG->L4FLAGS & L4BUSY)
		{
			// Refused

			CLEARSESSIONENTRY(L4);
			if (Partner)
				Partner->L4CROSSLINK = NULL;	// CLEAR CROSSLINK

			strcpy(ReplyText, "Busy from");
		}
		else
		{
			// Connect OK

			if (L4->L4STATE == 5)
			{
				// MUST BE REPEAT MSG - DISCARD

				ReleaseBuffer(L3MSG);
				return;
			}

			L4->FARINDEX = L3MSG->L4TXNO;
			L4->FARID = L3MSG->L4RXNO;

			L4->L4STATE = 5;			// ACTIVE
			L4->L4TIMER = 0;
			L4->L4RETRIES = 0;

			L4->L4WINDOW = L3MSG->L4DATA[0];

			strcpy(ReplyText, "Connected to");
		}

		if (Partner == 0)
		{
			ReleaseBuffer(L3MSG);
			return;
		}

		Msg = (PDATAMESSAGE)L3MSG;					// reuse input buffer

		Msg->PID = 0xf0;
		ptr1 = SetupNodeHeader(Msg);

		Nodename[DecodeNodeName(DEST->DEST_CALL, Nodename)] = 0;		// null terminate

		ptr1 += sprintf(ptr1, "%s %s\r", ReplyText, Nodename);

		Msg->LENGTH = ptr1 - (UCHAR *)Msg;

		C_Q_ADD(&Partner->L4TX_Q, Msg);

		PostDataAvailable(Partner);
		return;

	case L4DREQ:

		// DISCONNECT REQUEST

		 L3MSG->L4INDEX = L4->FARINDEX;
		 L3MSG->L4ID = L4->FARID;
		 
		 L3MSG->L4FLAGS = L4DACK;

		 L3SWAPADDRESSES(L3MSG);				// EXCHANGE SOURCE AND DEST
		 L3MSG->L3TTL = L3LIVES;

		 C_Q_ADD(&LINK->TX_Q, L3MSG);

		 CloseSessionPartner(L4);				// SEND CLOSE TO PARTNER (IF PRESENT)
		 return;
	
	case L4DACK:
	
		CLEARSESSIONENTRY(L4);
		ReleaseBuffer(L3MSG);
		return;

	case L4INFO:

		//MAKE SURE SESSION IS UP - FIRST I FRAME COULD ARRIVE BEFORE CONNECT ACK

		if (L4->L4STATE == 2)
		{
			ReleaseBuffer(L3MSG);		// SHOULD SAVE - WILL AVOID NEED TO RETRANSMIT
			return;	
		}

		ACKFRAMES(L3MSG, L4, L3MSG->L4RXNO);

		//	If DISCPENDING or STATE IS 4, THEN SESSION IS CLOSING - IGNORE ANY I FRAMES

		if ((L4->FLAGS & DISCPENDING) || L4->L4STATE == 4)
		{
			ReleaseBuffer(L3MSG);
			return;
		} 

		// CHECK RECEIVED SEQUENCE

		FramesMissing = L3MSG->L4TXNO - L4->RXSEQNO;	// WHAT WE GOT -  WHAT WE WANT

		if (FramesMissing > 128)
			FramesMissing -= 256;

		// if NUMBER OF FRAMES MISSING is  -VE, THEN IN FACT IT	INDICATES A REPEAT

		if (FramesMissing < 0)
		{
			// FRAME IS A REPEAT

			Call[ConvFromAX25(L3MSG->L3SRCE, Call)] = 0;
			Debugprintf("Discarding repeated frame seq %d from %s", L3MSG->L4TXNO, Call);

			L4->L4ACKREQ = 1;
			ReleaseBuffer(L3MSG);
			return;
		}
		
		if (FramesMissing > 0)
		{
			//	EXPECTED FRAME HAS BEEN MISSED - ASK FOR IT AGAIN,
			//	AND KEEP THIS FRAME UNTIL MISSING ONE ARRIVES

			L4->NAKBITS |= L4NAK;			// SET NAK REQUIRED
	
			SENDL4IACK(L4);			// SEND DATA ACK COMMAND TO ACK OUTSTANDING FRAMES
	
			//	SEE IF WE ALREADY HAVE A COPY OF THIS ONE

			Saved = L4->L4RESEQ_Q;

			Call[ConvFromAX25(L3MSG->L3SRCE, Call)] = 0;
			Debugprintf("saving seq %d from %s", L3MSG->L4TXNO, Call);

			while (Saved)
			{
				if (Saved->L4TXNO == L3MSG->L4TXNO)
				{
					//	ALREADY HAVE A COPY - DISCARD IT
			
					Debugprintf("Already have seq %d - discarding", L3MSG->L4TXNO);
					ReleaseBuffer(L3MSG);
					return;
				}

				Saved = Saved->Next;
			}

			C_Q_ADD(&L4->L4RESEQ_Q, L3MSG);		// ADD TO CHAIN
			return;
		}

		// Frame is OK

L4INFO_OK:

		if (L3MSG == 0)
		{
			Debugprintf("Trying to Process NULL L3 Message");
			return;
		}

		L4->NAKBITS &= ~L4NAK;				// CLEAR MESSAGE LOST STATE

		L4->RXSEQNO++;
	
		//	REMOVE HEADERS, AND QUEUE INFO 

		L3MSG->LENGTH -= 20;				// L3/L4 Header

		if (L3MSG->LENGTH < 8)				// No PID
		{					
			ReleaseBuffer(L3MSG);
			return;
		}

		L3MSG->L3PID = 0xF0;				// Normal Data PID

		memmove(L3MSG->L3SRCE, L3MSG->L4DATA, L3MSG->LENGTH - 8);

		REFRESHROUTE(L4);

		L4->L4ACKREQ = L4DELAY;				// SEND INFO ACK AFTER L4DELAY (UNLESS I FRAME SENT) 

		IFRM150(L4, (PDATAMESSAGE)L3MSG);	// CHECK IF SETTING UP AND PASS ON

		// See if anything on reseq Q to process

		if (L4->L4RESEQ_Q == 0)
			return;

		Prev = &L4->L4RESEQ_Q;
		Saved = L4->L4RESEQ_Q;

		while (Saved)
		{
			if (Saved->L4TXNO == L4->RXSEQNO)		// The one we want
			{
				// REMOVE IT FROM QUEUE,AND PROCESS IT

				*Prev = Saved->Next;		// CHAIN  NEXT IN CHAIN TO PREVIOUS

				OLDFRAMES++;			// COUNT FOR STATS
	
				L3MSG = Saved;
				Debugprintf("Processing Saved Message %d Address %x", L4->RXSEQNO, L3MSG);
				goto L4INFO_OK;
			}

			Debugprintf("Message %d %x still on Reseq Queue", Saved->L4TXNO, Saved);

			Prev = &Saved;
			Saved = Saved->Next;
		}

		return;

	case L4IACK:

		ACKFRAMES(L3MSG, L4, L3MSG->L4RXNO);
		REFRESHROUTE(L4);

		// Drop Through
	}

	// Unrecognised - Ignore

	ReleaseBuffer(L3MSG);
	return;
}


VOID ACKFRAMES(L3MESSAGEBUFFER * L3MSG, TRANSPORTENTRY * L4, int NR)
{
	//	SEE HOW MANY FRAMES ARE ACKED - IF NEGATIVE, THAN THIS MUST BE A
	//	DELAYED REPEAT OF AN ACK ALREADY PROCESSED

	int Count = NR - L4->L4WS;
	L3MESSAGEBUFFER * Saved;
	struct DEST_LIST * DEST;
	struct DATAMESSAGE * Msg;
	struct DATAMESSAGE * Copy;
	int RTT;


	if (Count < -128)
		Count += 256;

	if (Count < 0)
	{
		//	THIS MAY BE A DELAYED REPEAT OF AN ACK ALREADY PROCESSED

		return;				// IGNORE COMPLETELY
	}

	while (Count > 0)
	{
		// new ACK

		//	FRAME L4WS HAS BEED ACKED - IT SHOULD BE FIRST ON HOLD QUEUE

		Saved = Q_REM(&L4->L4HOLD_Q);

		if (Saved)
			ReleaseBuffer(Saved);

		//	CHECK RTT SEQUENCE

		if (L4->L4WS == L4->RTT_SEQ)
		{
			if (L4->RTT_TIMER)
			{
				//	FRAME BEING TIMED HAS BEEN ACKED - UPDATE DEST RTT TIMER
				
				DEST = L4->L4TARGET.DEST;
				
				RTT = GetTickCount() - L4->RTT_TIMER;

				if (DEST->DEST_RTT == 0)
					DEST->DEST_RTT = RTT;
				else
					DEST->DEST_RTT = ((DEST->DEST_RTT * 9) + RTT) /10;	// 90% Old + New
			}
		}

		L4->L4WS++;
		Count--;
	}

	L4->L4TIMER = 0;
	L4->L4RETRIES = 0;

	if (NR != L4->TXSEQNO)
	{
		// Not all Acked

		L4->L4TIMER = L4->SESSIONT1;	// RESTART TIMER
	}
	else
	{
		if ((L4->FLAGS & DISCPENDING) && L4->L4TX_Q == 0)
		{
			// All Acked and DISC Pending, so send it
		
			SENDL4DISC(L4);
			return;
		}
	}

	//	SEE IF CHOKE SET

	L4->FLAGS &= ~L4BUSY;
		
	if (L3MSG->L4FLAGS & L4BUSY)
	{
		L4->FLAGS |= L3MSG->L4FLAGS & L4BUSY;		// Get Busy flag from message

		if ((L3MSG->L4FLAGS & L4NAK) == 0)
			return;						// Dont send while biust unless NAC received
	}

	if (L3MSG->L4FLAGS & L4NAK)
	{
		//	RETRANSMIT REQUESTED MESSAGE - WILL BE FIRST ON HOLD QUEUE

		Msg = L4->L4HOLD_Q;
		
		if (Msg == 0)
			return;
 
		Copy = GetBuff();

		if (Copy == 0)
			return;

		memcpy(Copy, Msg, Msg->LENGTH);

		DEST = L4->L4TARGET.DEST;

		C_Q_ADD(&DEST->DEST_Q, Copy);
	}
}















VOID SENDL4IACK(TRANSPORTENTRY * Session)
{
	//	SEND INFO ACK

	PL3MESSAGEBUFFER MSG = (PL3MESSAGEBUFFER)GetBuff();
	struct DEST_LIST * DEST = Session->L4TARGET.DEST;

	if (MSG == NULL)
		return;

	MSG->L3PID = 0xCF;			// NET MESSAGE

	memcpy(MSG->L3SRCE, Session->L4MYCALL, 7);
	memcpy(MSG->L3DEST, DEST->DEST_CALL, 7);

	MSG->L3TTL = L3LIVES;

	MSG->L4INDEX = Session->FARINDEX;
	MSG->L4ID = Session->FARID;

	MSG->L4TXNO = 0;

	
	MSG->L4RXNO = Session->RXSEQNO;
	Session->L4LASTACKED = Session->RXSEQNO;	// SAVE LAST NUMBER ACKED

	MSG->L4FLAGS = L4IACK | GETBUSYBIT(Session) | Session->NAKBITS;

	MSG->LENGTH = MSGHDDRLEN + 22;

	C_Q_ADD(&DEST->DEST_Q, (UINT *)MSG);
}




/*
	PUBLIC	KILLSESSION
KILLSESSION:

	pushad
	push	ebx
	CALL	_CLEARSESSIONENTRY
	pop	ebx
	popad

	JMP	L4CONN90		; REJECT

	PUBLIC	CONNECTACK
CONNECTACK:
;
;	EXTRACT EXTENDED PARAMS IF PRESENT
;

	CMP	BYTE PTR MSGLENGTH[EDI],L4DATA+1
	JE SHORT NOTBPQ

	MOV	AL,L4DATA+1[EDI]
	SUB	AL,L3MONR[EDI]
	ADD	AL,41H			; HOPS TO DEST + 40H

	MOV	ESI,L4TARGET[EBX]
	AND	DEST_STATE[ESI],80H
	OR	DEST_STATE[ESI],AL	; SAVE

	PUBLIC	NOTBPQ
NOTBPQ:
;
;	SEE IF SUCCESS OR FAIL
;
	PUSH	EDI

	MOV	ESI,L4TARGET[EBX]		; ADDR OF LINK/DEST ENTRY
	LEA	ESI,DEST_CALL[ESI]

	CALL	DECODENODENAME		; CONVERT TO ALIAS:CALL

	MOV	EDI,OFFSET32 CONACKCALL
	MOV	ECX,17
	REP MOVSB


	POP	EDI

	TEST	L4FLAGS[EDI],L4BUSY
	JNZ SHORT L4CONNFAILED

	CMP	L4STATE[EBX],5
	JE SHORT CONNACK05		; MUST BE REPEAT MSG - DISCARD

	MOV	AX,WORD PTR L4TXNO[EDI]	; HIS INDEX
	MOV	WORD PTR FARINDEX[EBX],AX

	MOV	L4STATE[EBX],5		; ACTIVE
	MOV	L4TIMER[EBX],0		; CANCEL TIMER
	MOV	L4RETRIES[EBX],0		; CLEAR RETRY COUNT
 
	MOV	AL,L4DATA[EDI]		; WINDOW
	MOV	L4WINDOW[EBX],AL		; SET WINDOW

	MOV	EDX,L4CROSSLINK[EBX]	; POINT TO PARTNER
;
	MOV	ESI,OFFSET32 CONNECTEDMSG
	MOV	ECX,LCONNECTEDMSG

	JMP SHORT L4CONNCOMM

	PUBLIC	L4CONNFAILED
L4CONNFAILED:
;
	MOV	EDX,L4CROSSLINK[EBX]	; SAVE PARTNER
	pushad
	push	ebx
	CALL	_CLEARSESSIONENTRY
	pop	ebx
	popad

	PUSH	EBX

	MOV	EBX,EDX
	MOV	L4CROSSLINK[EBX],0	; CLEAR CROSSLINK
	POP	EBX

	MOV	ESI,OFFSET32 BUSYMSG	; ?? BUSY
	MOV	ECX,LBUSYMSG

	PUBLIC	L4CONNCOMM
L4CONNCOMM:

	OR	EDX,EDX
	JNZ SHORT L4CONNOK10
;
;	CROSSLINK HAS GONE?? - JUST CHUCK MESSAGE
;
	PUBLIC	CONNACK05
CONNACK05:

	JMP	L4DISCARD

	PUBLIC	L4CONNOK10
L4CONNOK10:

	PUSH	EBX
	PUSH	ESI
	PUSH	ECX

	MOV	EDI,_BUFFER

	ADD	EDI,7
	MOV	AL,0F0H
	STOSB				; PID

	CALL	_SETUPNODEHEADER		; PUT IN _NODE ID


	POP	ECX
	POP	ESI
	REP MOVSB

	MOV	ESI,OFFSET32 CONACKCALL
	MOV	ECX,17			; MAX LENGTH ALIAS:CALL
	REP MOVSB

	MOV	AL,0DH
	STOSB

	MOV	ECX,EDI
	MOV	EDI,_BUFFER
	SUB	ECX,EDI

	MOV	MSGLENGTH[EDI],CX

	MOV	EBX,EDX			; CALLER'S SESSION

	LEA	ESI,L4TX_Q[EBX]
	CALL	_Q_ADD			; SEND MESSAGE TO CALLER

	CALL	_POSTDATAAVAIL
	
	POP	EBX			; ORIGINAL CIRCUIT TABLE
	RET


	PUBLIC	SENDCONNECTREPLY
SENDCONNECTREPLY:
;
;	LINK SETUP COMPLETE - EBX = LINK, EDI = _BUFFER
;
	CMP	LINKTYPE[EBX],3
	JNE SHORT CONNECTED00
;
;	_NODE - _NODE SESSION SET UP - DONT NEED TO DO ANYTHING (I THINK!)
;
	CALL	RELBUFF
	RET

;
;	UP/DOWN LINK
;
	PUBLIC	CONNECTED00
CONNECTED00:
	CMP	CIRCUITPOINTER[EBX],0	
	JNE SHORT CONNECTED01

	CALL	RELBUFF			; UP/DOWN WITH NO SESSION - NOONE TO TELL
	RET				; NO CROSS LINK
	PUBLIC	CONNECTED01
CONNECTED01:
	MOV	_BUFFER,EDI
	PUSH	EBX
	PUSH	ESI
	PUSH	ECX

	ADD	EDI,7
	MOV	AL,0F0H
	STOSB				; PID

	CALL	_SETUPNODEHEADER		; PUT IN _NODE ID

	LEA	ESI,LINKCALL[EBX]

	PUSH	EDI
	CALL	CONVFROMAX25		; ADDR OF CALLED STATION
	POP	EDI

	MOV	EBX,CIRCUITPOINTER[EBX]

	MOV	L4STATE[EBX],5		; SET LINK UP

	MOV	EBX,L4CROSSLINK[EBX]	; TO INCOMING LINK
	cmp	ebx,0
	jne	xxx
;
;	NO LINK ??? 
;
	MOV		EDI,_BUFFER
	CALL	RELBUFF	
		
	POP	ECX
	POP	ESI
	POP	EBX
	
	RET

	PUBLIC	xxx
xxx:
			
	POP	ECX
	POP	ESI
	REP MOVSB

	MOV	ESI,OFFSET32 _NORMCALL
	MOVZX	ECX,_NORMLEN
	REP MOVSB

	MOV	AL,0DH
	STOSB

	MOV	ECX,EDI
	MOV	EDI,_BUFFER
	SUB	ECX,EDI

	MOV	MSGLENGTH[EDI],CX

	LEA	ESI,L4TX_Q[EBX]
	CALL	_Q_ADD			; SEND MESSAGE TO CALLER

	CALL	_POSTDATAAVAIL

	POP	EBX
	RET
*/