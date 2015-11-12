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
//	C replacement for L2Code.asm
//
#define Kernel

#define _CRT_SECURE_NO_DEPRECATE 
#define _USE_32BIT_TIME_T

#pragma data_seg("_BPQDATA")

#include "time.h"
#include "stdio.h"
				 
#include "CHeaders.h"

#define	UI	3
#define	SABM 0x2F
#define	DISC 0x43
#define	DM	0x0F
#define	UA	0x63
#define	FRMR 0x87
#define	RR	1
#define	RNR	5
#define	REJ	9

#define	PFBIT 0x10		// POLL/FINAL BIT IN CONTROL BYTE

#define	REJSENT	1		// SET WHEN FIRST REJ IS SENT IN REPLY
						// TO AN I(P)
#define	RNRSET	2		// RNR RECEIVED FROM OTHER END
#define DISCPENDING	8		// SEND DISC WHEN ALL DATA ACK'ED
#define	RNRSENT	0x10	// WE HAVE SEND RNR
#define	POLLSENT 0x20	// POLL BIT OUTSTANDING

#define	ONEMINUTE 60*3		
#define	TENSECS	10*3
#define	THREESECS 3*3


VOID L2SENDCOMMAND();
VOID L2ROUTINE();
MESSAGE * SETUPL2MESSAGE(struct _LINKTABLE * LINK, UCHAR CMD);
VOID SendSupervisCmd(struct _LINKTABLE * LINK);
void SEND_RR_RESP(struct _LINKTABLE * LINK, UCHAR PF);
VOID L2SENDRESPONSE(struct _LINKTABLE * LINK, int CMD);
VOID L2SENDCOMMAND(struct _LINKTABLE * LINK, int CMD);
VOID ACKMSG(struct _LINKTABLE * LINK);
VOID InformPartner(struct _LINKTABLE * LINK, int Reason);
UINT RR_OR_RNR(struct _LINKTABLE * LINK);
VOID L2TIMEOUT(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT);
VOID CLEAROUTLINK(struct _LINKTABLE * LINK);
VOID SENDFRMR(struct _LINKTABLE * LINK);
char * SetupNodeHeader(struct DATAMESSAGE * Buffer);
VOID CLEARSESSIONENTRY(TRANSPORTENTRY * Session);
VOID SDFRMR(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT);
VOID SDNRCHK(struct _LINKTABLE * LINK, UCHAR CTL);
VOID RESETNS(struct _LINKTABLE * LINK, UCHAR NS);
VOID PROC_I_FRAME(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer);
VOID RESET2X(struct _LINKTABLE * LINK);
VOID RESET2(struct _LINKTABLE * LINK);
VOID CONNECTREFUSED(struct _LINKTABLE * LINK);
VOID SDUFRM(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR CTL);
VOID SFRAME(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, UCHAR CTL, UCHAR MSGFLAG);
VOID SDIFRM(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR CTL, UCHAR MSGFLAG);
VOID SENDCONNECTREPLY(struct _LINKTABLE * LINK);
VOID SETUPNEWL2SESSION(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR MSGFLAG);
BOOL FindNeighbour(UCHAR * Call, int Port, struct ROUTE ** REQROUTE);
VOID L2SABM(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER, UCHAR MSGFLAG);
VOID L2SENDUA(struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER);
VOID L2SENDDM(struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER);
VOID L2SENDRESP(struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER, UCHAR CTL);
int COUNTLINKS(int Port);
VOID L2_PROCESS(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR CTL, UCHAR MSGFLAG);
TRANSPORTENTRY * SetupSessionForL2(struct _LINKTABLE * LINK);
BOOL cATTACHTOBBS(TRANSPORTENTRY * Session, UINT Mask, int Paclen, int * AnySessions);
VOID PUT_ON_PORT_Q(struct PORTCONTROL * PORT, MESSAGE * Buffer);
VOID L2SWAPADDRESSES(MESSAGE * Buffer);
BOOL FindLink(UCHAR * LinkCall, UCHAR * OurCall, int Port, struct _LINKTABLE ** REQLINK);
VOID SENDSABM(struct _LINKTABLE * LINK);
VOID __cdecl Debugprintf(const char * format, ...);
VOID Q_IP_MSG(MESSAGE * Buffer);
VOID PROCESSNODEMESSAGE(MESSAGE * Msg, struct PORTCONTROL * PORT);
VOID L2LINKACTIVE(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER, UCHAR CTL, UCHAR MSGFLAG);
BOOL CompareAliases(UCHAR * c1, UCHAR * c2);
VOID L2FORUS(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER, UCHAR CTL, UCHAR MSGFLAG);
VOID Digipeat(struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR * OurCall, int toPort, int UIOnly);
VOID DigiToMultiplePorts(struct PORTCONTROL * PORTVEC, PMESSAGE Msg);
VOID MHPROC(struct PORTCONTROL * PORT, MESSAGE * Buffer);
BOOL CheckForListeningSession(struct PORTCONTROL * PORT, MESSAGE * Msg);

extern int REALTIMETICKS;

//	MSGFLAG contains CMD/RESPONSE BITS

#define	CMDBIT	4		// CURRENT MESSAGE IS A COMMAND
#define	RESP 2		// CURRENT MSG IS RESPONSE
#define	VER1 1 		// CURRENT MSG IS VERSION 1

//	FRMR REJECT FLAGS

#define	SDINVC 1		// INVALID COMMAND
#define	SDNRER 8		// INVALID N(R)



UCHAR NO_CTEXT = 0;
UCHAR ALIASMSG = 0;
extern UINT APPLMASK;
static UCHAR ISNETROMMSG = 0;
UCHAR MSGFLAG = 0;
extern UCHAR * ALIASPTR;

UCHAR QSTCALL[7] = {'Q'+'Q','S'+'S','T'+'T',0x40,0x40,0x40,0xe0};	// QST IN AX25
UCHAR NODECALL[7] = {0x9C, 0x9E, 0x88, 0x8A, 0xA6, 0x40, 0xE0};		// 'NODES' IN AX25 FORMAT


VOID L2Routine(struct PORTCONTROL * PORT, MESSAGE * Buffer)
{
	//	LEVEL 2 PROCESSING

	MESSAGE * ADJBUFFER;
	struct _LINKTABLE * LINK;
	APPLCALLS * APPL;
	UCHAR * ptr;
	int n;
	UCHAR CTL;
	UINT Work;
	UCHAR c;

	//	Check for invalid length (< 22 7Header + 7Addr + 7Addr + CTL

	if (Buffer->LENGTH < 22)
	{
		Debugprintf("BPQ32 Bad L2 Msg Port %d Len %d", PORT->PORTNUMBER, Buffer->LENGTH);
		ReleaseBuffer(Buffer);
		return;
	}

 	PORT->L2FRAMES++;

	ALIASMSG = 0;
	APPLMASK = 0;
	ISNETROMMSG = 0;

	MSGFLAG = 0;					// CMD/RESP UNDEFINED

	//	Check for Corrupted Callsign in Origin (to keep MH list clean)

	ptr = &Buffer->ORIGIN[0];
	n = 6;

	while(n--)
	{
		// Try a bit harder to detect corruption

		c = *(ptr++);

		if (c & 1)
		{
			ReleaseBuffer(Buffer);
			return;
		}

		c = c >> 1;
		
		if (!isalnum(c) && !(c == '#') && !(c == ' '))
		{
			ReleaseBuffer(Buffer);
			return;
		}
	}

	//	Check Digis if present

	if ((Buffer->ORIGIN[6] & 1) == 0)	// Digis
	{
		ptr = &Buffer->CTL;
		n = 6;

		while(n--)
		{
			c = *(ptr++);

			if (c & 1)
			{
				ReleaseBuffer(Buffer);
				return;
			}

			c = c >> 1;
		
			if (!isalnum(c) && !(c == '#') && !(c == ' '))
			{
				ReleaseBuffer(Buffer);
				return;
			}
		}
	}

	BPQTRACE(Buffer, TRUE);				// TRACE - RX frames to APRS

	if (PORT->PORTMHEARD)
		MHPROC(PORT, Buffer);

	//	CHECK THAT ALL DIGIS HAVE BEEN ACTIONED,
	//  AND ADJUST FOR DIGIPEATERS IF PRESENT

	n = 8;						// MAX DIGIS
	ptr = &Buffer->ORIGIN[6];	// End of Address bit

	while ((*ptr & 1) == 0)
	{
		//	MORE TO COME
	
		ptr += 7;

		if ((*ptr & 0x80) == 0)				// Digi'd bit
		{
			//	FRAME HAS NOT BEEN REPEATED THROUGH CURRENT DIGI -
			// SEE IF WE ARE MEANT TO DIGI IT

			struct XDIGI * XDigi = PORT->XDIGIS;		// Cross port digi setup

			ptr -= 6;						// To start of Call

			if (CompareCalls(ptr, MYCALL) || CompareAliases(ptr, MYALIAS) ||
					CompareCalls(ptr, PORT->PORTALIAS) || CompareCalls(ptr, PORT->PORTALIAS2))	
			{
				Digipeat(PORT, Buffer, ptr, 0, 0);		// Digi it (if enabled)
				return;
			}

			while (XDigi)
			{
				if (CompareCalls(ptr, XDigi->Call))
				{
					Digipeat(PORT, Buffer, ptr, XDigi->Port, XDigi->UIOnly);		// Digi it (if enabled)
					return;
				}
				XDigi = XDigi->Next;
			}

			ReleaseBuffer(Buffer);
			return;							// not complete and not for us
		}
		n--;

		if (n == 0)
		{
			ReleaseBuffer(Buffer);
			return;						// Corrupt - no end of address bit
		}
	}

	// Reached End of digis, and all actioned, so can process it

	Work = (UINT)&Buffer->ORIGIN[6];
	ptr -= 	Work;							// ptr is now length of digis

	Work = (UINT)Buffer;
	ptr += Work;

	ADJBUFFER = (MESSAGE * )ptr;			// ADJBUFFER points to CTL, etc. allowing for digis

	//	GET CMD/RESP BITS

	if (Buffer->DEST[6] & 0x80)
	{
		if (Buffer->ORIGIN[6] & 0x80)			//	Both set, assume V1
			MSGFLAG |= VER1;
		else
			MSGFLAG |= CMDBIT;
	}
	else
	{
		if (Buffer->ORIGIN[6] & 0x80)			//	Only Dest Set
			MSGFLAG |= RESP;
		else
			MSGFLAG |= VER1;				// Neither, assume V1
	}

	//	SEE IF FOR AN ACTIVE LINK SESSION

	CTL = ADJBUFFER->CTL;

	// IF A UI, THERE IS NO SESSION

	if (FindLink(Buffer->ORIGIN, Buffer->DEST, PORT->PORTNUMBER, &LINK))
	{
		L2LINKACTIVE(LINK, PORT, Buffer,ADJBUFFER, CTL, MSGFLAG);
		return;
	}

	//	NOT FOR ACTIVE LINK - SEE IF ADDRESSED TO OUR ADDRESSES

	ISNETROMMSG = PORT->PORTL3FLAG;		// Set if L3 only and call to NETROMCALL
	
	if (CompareCalls(Buffer->DEST, NETROMCALL))
		goto FORUS;

	if (PORT->PORTL3FLAG)				// L3 Only Port?
		goto NOTFORUS;					// If L3ONLY, only accept calls to NETROMCALL
	
	ISNETROMMSG = 0;

	//	FIRST TRY PORT ADDR/ALIAS

	if(PORT->PORTBBSFLAG == 1)
		goto PORTCALLISBBS;				// PORT CALL/ALIAS ARE FOR BBS

	if (NODE)
		goto USING_NODE;

PORTCALLISBBS:

	//	NODE IS NOT ACTIVE, SO PASS CALLS TO PORTCALL/ALIAS TO BBS

	APPLMASK = 1;

USING_NODE:

	if (CompareCalls(Buffer->DEST, PORT->PORTCALL))
		goto FORUS;
	
	ALIASMSG = 1;

	if (CompareAliases(Buffer->DEST, PORT->PORTALIAS))		// only compare 6 bits - allow any ssid
		goto FORUS;

	if (NODE == 0)
		goto TRYBBS;					// NOT USING NODE SYSTEM

	ALIASMSG = 0;
	
	if (CompareCalls(Buffer->DEST, MYCALL))
		goto FORUS;

	ALIASMSG = 1;

	if (CompareAliases(Buffer->DEST, MYALIAS))		// only compare 6 bits - allow any ssid
		goto FORUS;

	//	WE MAY NOT BE ALLOWED TO USE THE BBS CALL ON SOME BANDS DUE TO
	//	THE RATHER ODD UK LICENCING RULES!

	if (PORT->BBSBANNED == 1)
		goto NOWTRY_NODES;

TRYBBS:

	if (BBS == 0)
		goto NOWTRY_NODES;			// NOT USING BBS CALLS

	//	TRY APPLICATION CALLSIGNS/ALIASES


	APPLMASK = 1;
	ALIASPTR = &CMDALIAS[0][0];

	n = NumberofAppls;

	APPL = APPLCALLTABLE;

	while (n--)
	{
		if (APPL->APPLCALL[0] > 0x40)		// Valid ax.25 addr
		{
			ALIASMSG = 0;
			
			if (CompareCalls(Buffer->DEST, APPL->APPLCALL))
				goto FORUS;

			ALIASMSG = 1;

			if (CompareAliases(Buffer->DEST, APPL->APPLALIAS))		// only compare 6 bits - allow any ssid
				goto FORUS;

			if (CompareAliases(Buffer->DEST, APPL->L2ALIAS))		// only compare 6 bits - allow any ssid
				goto FORUS;

		}
		APPLMASK <<= 1;
		ALIASPTR += ALIASLEN;
		APPL++;
	}
	
	// NOT FOR US - SEE IF 'NODES' OR IP/ARP BROADCAST MESSAGE

NOWTRY_NODES:

	if (CompareCalls(Buffer->DEST, QSTCALL))
	{
		Q_IP_MSG(Buffer);				// IP BROADCAST
		return;
	}

	if (ADJBUFFER->PID != 0xCF)				// NETROM MSG?
		goto NOTFORUS;						// NO

	if (CompareCalls(Buffer->DEST, NODECALL))
	{
		if (Buffer->L2DATA[0] == 0xff)	// Valid NODES Broadcast
		{
			PROCESSNODEMESSAGE(Buffer, PORT);
		}
	}
	
	ReleaseBuffer(Buffer);
	return;
	
NOTFORUS:
//
//	MAY JUST BE A REPLY TO A 'PRIMED' CQ CALL
//
	if ((CTL & ~PFBIT) == SABM)
		if (CheckForListeningSession(PORT, Buffer))
			return;		// Used buffer to send UA

	ReleaseBuffer(Buffer);
	return;

FORUS:

	// if a UI frame and UIHook Specified, call it

	if (PORT->UIHook && CTL == 3)
		PORT->UIHook(LINK, PORT, Buffer, ADJBUFFER, CTL, MSGFLAG);

	L2FORUS(LINK, PORT, Buffer, ADJBUFFER, CTL, MSGFLAG);
}
	

VOID MHPROC(struct PORTCONTROL * PORT, MESSAGE * Buffer)
{
	PMHSTRUC MH = PORT->PORTMHEARD;
	PMHSTRUC MHBASE = MH;
	int i;
	int OldCount = 0;

	char DIGI = '*';
	
//	if (Buffer->ORIGIN[6] & 1)
		DIGI = 0;					// DOn't think we wnat to do this
	
	// See if in list

	for (i = 0; i < MHENTRIES; i++)
	{
		if ((MH->MHCALL[0] == 0) || (CompareCalls(Buffer->ORIGIN, MH->MHCALL) && MH->MHDIGI == DIGI)) // Spare or our entry
		{
			OldCount = MH->MHCOUNT;
			goto DoMove;
		}
		MH++;
	}

	//	TABLE FULL AND ENTRY NOT FOUND - MOVE DOWN ONE, AND ADD TO TOP

	i = MHENTRIES - 1;
		
	// Move others down and add at front
DoMove:
	if (i != 0)				// First
		memmove(MHBASE + 1, MHBASE, i * sizeof(MHSTRUC));

	memcpy (MHBASE->MHCALL, Buffer->ORIGIN, 7 * 9);		// Save Digis
	MHBASE->MHDIGI = DIGI;
	MHBASE->MHTIME = time(NULL);
	MHBASE->MHCOUNT = ++OldCount;

	return;
}


int CountFramesQueuedOnSession(TRANSPORTENTRY * Session)
{
	//	COUNT NUMBER OF FRAMES QUEUED ON A SESSION

	if (Session == 0)
		return 0;

	if (Session->L4CIRCUITTYPE & BPQHOST)
	{
		return C_Q_COUNT(&Session->L4TX_Q);
	}

	if (Session->L4CIRCUITTYPE & SESSION)
	{
		//	L4 SESSION - GET NUMBER UNACKED, AND ADD NUMBER ON TX QUEUE

		int Count = C_Q_COUNT(&Session->L4TX_Q);
		UCHAR Unacked = Session->TXSEQNO - Session->L4WS;

		return Count + Unacked;
	}

	if (Session->L4CIRCUITTYPE & PACTOR)
	{
		//	PACTOR Type - Frames are queued on the Port Entry

		struct PORTCONTROL * PORT = Session->L4TARGET.PORT;
		EXTPORTDATA * EXT = (EXTPORTDATA *)PORT;

		int ret = EXT->FramesQueued;

		// Check L4 Queue as messages can stay there briefly

		ret += C_Q_COUNT(&Session->L4RX_Q);
					
		return ret + C_Q_COUNT(&PORT->PORTTX_Q);
	}

	//	L2 CIRCUIT

	{
		int SessCount = C_Q_COUNT(&Session->L4TX_Q);
		struct _LINKTABLE * LINK = Session->L4TARGET.LINK;
		int L2 = COUNT_AT_L2(LINK);

		return SessCount + L2;
	}
}

int CHECKIFBUSYL2(TRANSPORTENTRY * Session)
{
	// RETURN TOP BIT OF AL SET IF SESSION PARTNER IS BUSY

	if (Session->L4CROSSLINK)			// CONNECTED?
	{
		Session = Session->L4CROSSLINK;

		if (CountFramesQueuedOnSession(Session) > 10)
			return L4BUSY;;
	}
	return 0;
}

VOID L2FORUS(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER, UCHAR CTL, UCHAR MSGFLAG)
{
	//	MESSAGE ADDRESSED TO OUR CALL OR ALIAS, BUT NOT FOR AN ACTIVE SESSION

	struct ROUTE * ROUTE;
	int CTLlessPF = CTL & ~PFBIT;
	
	PORT->L2FRAMESFORUS++;
	InOctets[PORT->PORTNUMBER] += Buffer->LENGTH - 7;

	NO_CTEXT = 0;

	//	ONLY SABM or UI  ALLOWED IF NO SESSION 

	if (CTLlessPF == 3)			// UI
	{
		//	A UI ADDRESSED TO US - SHOULD ONLY BE FOR IP, or possibly addressed NODES

		switch(ADJBUFFER->PID)
		{
		case 0xcf:				// Netrom
		
			if (Buffer->L2DATA[0] == 0xff)	// NODES
				PROCESSNODEMESSAGE(Buffer, PORT);

			break;

		case 0xcc:				// TCP
		case 0xcd:				// ARP
		case 0x08:				// NOS FRAGMENTED AX25 TCP/IP
	
			Q_IP_MSG( Buffer);
			return;
		}

		ReleaseBuffer(Buffer);
		return;
	}

	if (PORT->PortUIONLY)						// Port is for UI only
	{
		ReleaseBuffer(Buffer);
		return;
	}
	
	if (CTLlessPF != SABM)
	{
		if ((MSGFLAG & CMDBIT) && (CTL & PFBIT))	// Command with P?
			L2SENDDM(PORT, Buffer, ADJBUFFER);
		else
			ReleaseBuffer(Buffer);					// Ignore if not

		return;
	}

#ifdef	BLACKBITS
;
;	CHECK BLACKLIST
;
	EXTRN	CHECKBLACKLIST:NEAR

	MOV	EDI,_BUFFER
	LEA	ESI,MSGORIGIN[EDI]	; ORIGIN FROM MESSAGE

	CALL	CHECKBLACKLIST		; RETURN Z IF _NODE IS IN LIST

	JZ SHORT IGNORESABM		; DONT ACCEPT

#endif

	//	IF WE HAVE A PERMITTED CALLS LIST, SEE IF HE IS IN IT

	if (PORT->PERMITTEDCALLS)
	{
		UCHAR * ptr = PORT->PERMITTEDCALLS;

		while (TRUE)
		{
			if (memcmp(Buffer->ORIGIN, ptr, 6) == 0)	// Ignore SSID
				break;

			ptr += 7;

			if ((*ptr) == 0)							// Not in list
			{
				ReleaseBuffer(Buffer);
				return;
			}
		}
	}

	//	IF CALL REQUEST IS FROM A LOCKED NODE WITH QUALITY ZERO, IGNORE IT

	if (FindNeighbour(Buffer->ORIGIN, PORT->PORTNUMBER, &ROUTE))
	{
		// From a known node

		NO_CTEXT = 1;
		
		if (ROUTE->NEIGHBOUR_FLAG == 1 && ROUTE->NEIGHBOUR_QUAL == 0)		// Locked, qual 0
		{
			ReleaseBuffer(Buffer);
			return;
		}
	}

	//	CHECK PORT CONNECT LIMITS
	
	if (PORT->USERS)
	{
		if (COUNTLINKS(PORT->PORTNUMBER) >= PORT->USERS)
		{
			L2SENDDM(PORT, Buffer, ADJBUFFER);
			return;
		}
	}

	L2SABM(LINK, PORT, Buffer, ADJBUFFER, MSGFLAG);			// Process the SABM
	
}


int COUNTLINKS(int Port)
{
	//COUNT LINKS ON PORT

	int i = MAXLINKS, n = 0;
	struct _LINKTABLE * LINK = LINKS;

	while (i--)
	{
		if (LINK->LINKPORT && LINK->LINKPORT->PORTNUMBER == Port)
			n++;

		LINK++;
	}

	return n;
}


VOID L2LINKACTIVE(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER, UCHAR CTL, UCHAR MSGFLAG)
{
	//	MESSAGE ON AN ACTIVE LINK 

	int CTLlessPF = CTL & ~PFBIT;
	
	PORT->L2FRAMESFORUS++;
	InOctets[PORT->PORTNUMBER] += Buffer->LENGTH - 7;

	//	ONLY SABM or UI  ALLOWED IF NO SESSION 

	if (CTLlessPF == 3)			// UI
	{
		//	A UI ADDRESSED TO US - SHOULD ONLY BE FOR IP, or possibly addressed NODES

		switch(ADJBUFFER->PID)
		{
		case 0xcf:				// Netrom
		
			if (Buffer->L2DATA[0] == 0xff)	// NODES
				PROCESSNODEMESSAGE(Buffer, PORT);
	
			break;

		case 0xcc:				// TCP
		case 0xcd:				// ARP
		case 0x08:				// NOS FRAGMENTED AX25 TCP/IP
	
			Q_IP_MSG( Buffer);
			return;
		}

		ReleaseBuffer(Buffer);
		return;
	}

	if (CTLlessPF == DISC)
	{
		InformPartner(LINK, NORMALCLOSE);		// SEND DISC TO OTHER END
		CLEAROUTLINK(LINK);

		L2SENDUA(PORT, Buffer, ADJBUFFER);
		return;
	}

	if (CTLlessPF == SABM)
	{
		//	SABM ON EXISTING SESSION - IF DISCONNECTING, REJECT

		if (LINK->L2STATE == 4)			// DISCONNECTING?
		{
			L2SENDDM(PORT, Buffer, ADJBUFFER);
			return;
		}

		//	THIS IS A SABM ON AN EXISTING SESSION

		//	THERE ARE SEVERAL POSSIBILITIES:

		//	1. RECONNECT COMMAND TO TNC
		//	2. OTHER END THINKS LINK HAS DIED
		//	3. RECOVERY FROM FRMR CONDITION
		//	4. REPEAT OF ORIGINAL SABM COS OTHER END MISSED UA

		//	FOR 1-3 IT IS REASONABLE TO FULLY RESET THE CIRCUIT, BUT IN 4
		//	SUCH ACTION WILL LOSE THE INITIAL SIGNON MSG IF CONNECTING TO A
		//	BBS. THE PROBLEM IS TELLING THE DIFFERENCE. I'M GOING TO SET A FLAG 
		//	WHEN FIRST INFO RECEIVED - IF SABM REPEATED BEFORE THIS, I'LL ASSUME
		//	CONDITION 4, AND JUST RESEND THE UA


		if (LINK->SESSACTIVE == 0)			// RESET OF ACTIVE CIRCUIT?
		{
			L2SENDUA(PORT, Buffer, ADJBUFFER);			// No, so repeat UA
			return;
		}
		
		InformPartner(LINK, NORMALCLOSE);	// SEND DISC TO OTHER END

		L2SABM(LINK, PORT, Buffer, ADJBUFFER, MSGFLAG);			// Process the SABM
		return;

	}

	L2_PROCESS(LINK, PORT, Buffer, CTL, MSGFLAG);
}


VOID L2SABM(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER, UCHAR MSGFLAG)
{
	//	SET UP NEW SESSION (OR RESET EXISTING ONE)

	TRANSPORTENTRY * Session;
	int CONERROR;
	
	if (LINK == 0)			// NO LINK ENTRIES - SEND DM RESPONSE
	{
		L2SENDDM(PORT, Buffer, ADJBUFFER);
		return;
	}

	SETUPNEWL2SESSION(LINK, PORT, Buffer, MSGFLAG);

	if (LINK->L2STATE != 5)			// Setup OK?
	{
		L2SENDDM(PORT, Buffer, ADJBUFFER);		// Failed
		return;
	}


	//	IF CONNECT TO APPL ADDRESS, SET UP APPL SESSION

	if (APPLMASK == 0)
	{
		//	Not ATTACH TO APPL
	
		//	Send CTEXT if connect to NODE/Port Alias, or NODE/Port Call, and FULL_CTEXT set
		//	Dont sent to known NODEs, or appl connects 

		L2SENDUA(PORT, Buffer, ADJBUFFER);
		
		if (NO_CTEXT == 1)
			return;

		if (CTEXTLEN && (FULL_CTEXT == 1 || ALIASMSG))	// Any connect, or call to alias
		{
			struct DATAMESSAGE * Msg;
			int Totallen = CTEXTLEN;
			int Paclen= PORT->PORTPACLEN;
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

				C_Q_ADD(&LINK->TX_Q, Msg);

				ptr += Paclen;
				Totallen -= Paclen;
			}
		}
		return;
	}

	//	Connnect to APPL

	if (LINK->LINKTYPE != 1)
	{
		L2SENDUA(PORT, Buffer, ADJBUFFER);		// RESET OF DOWN/CROSSLINK
		return;
	}

	if (LINK->CIRCUITPOINTER)
	{
		L2SENDUA(PORT, Buffer, ADJBUFFER);		// ALREADY SET UP - MUST BE REPEAT OF SABM OR LINK RESET
		return;
	}

	//	IF RUNNING ONLY BBS (NODE=0), THIS MAY BE EITHER A USER OR NODE
	//	TRYING TO SET UP A L4 CIRCUIT - WE DONT WANT TO ATTACH A NODE TO
	//	THE BBS!

	if (NODE == 0)
	{
		//	NOW THINGS GET DIFICULT - WE MUST EITHER WAIT TO SEE IF A PID CF MSG
		//	ARRIVES, OR ASSUME ALL NODES ARE IN NEIGHBOURS - I'LL TRY THE LATTER
		//	AND SEE HOW IT GOES. tHIS MEANS THAT YOU MUST DEFINE ALL ROUTES
		//	IN CONFIG FILE

		struct ROUTE * ROUTE;

		if (FindNeighbour(Buffer->ORIGIN, PORT->PORTNUMBER, &ROUTE))
		{
			// It's a node

			L2SENDUA(PORT, Buffer, ADJBUFFER);		// ALREADY SET UP - MUST BE REPEAT OF SABM OR LINK RESET
			return;
		}
	}


	Session = SetupSessionForL2(LINK);	// CREATE INCOMING L4 SESSION
	
	if (Session == NULL)
	{
		CLEAROUTLINK(LINK);
		L2SENDDM(PORT, Buffer, ADJBUFFER);
		return;
	}

	//	NOW TRY A BBS CONNECT
	//	IF APPL CONNECT, SEE IF APPL HAS AN ALIAS

	if (ALIASPTR[0] > ' ')
	{
		struct DATAMESSAGE * Msg;

		//	ACCEPT THE CONNECT, THEN INVOKE THE ALIAS

		L2SENDUA(PORT, Buffer, ADJBUFFER);

		Msg = GetBuff();

		if (Msg)
		{

			Msg->PID = 0xf0;
				
			memcpy(Msg->L2DATA, ALIASPTR, ALIASLEN);
			Msg->L2DATA[ALIASLEN] = 13;
			
			Msg->LENGTH = MSGHDDRLEN + ALIASLEN + 2;		// 2 for PID and CR

			C_Q_ADD(&LINK->RX_Q, Msg);
		}

		return;
	}

	if (cATTACHTOBBS(Session, APPLMASK, PORT->PORTPACLEN, &CONERROR) == 0)
	{
		//	NO BBS AVAILABLE
	
		CLEARSESSIONENTRY(Session);
		CLEAROUTLINK(LINK);
		L2SENDDM(PORT, Buffer, ADJBUFFER);
		return;
	}

	L2SENDUA(PORT, Buffer, ADJBUFFER);
}

VOID SETUPNEWL2SESSION(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR MSGFLAG)
{
	//	COPY ADDRESS INFO TO LINK TABLE

	UCHAR * ptr1, * ptr2;
	UCHAR TEMPDIGI[57];
	int n;

	memcpy(LINK->LINKCALL, Buffer->ORIGIN, 7);
	LINK->LINKCALL[6] &= 0x1e;		// Mask SSID

	memcpy(LINK->OURCALL, Buffer->DEST, 7);
	LINK->OURCALL[6] &= 0x1e;		// Mask SSID

	memset(LINK->DIGIS, 0, 56);		// CLEAR DIGI FIELD IN CASE RECONNECT

	LINK->L2TIME = PORT->PORTT1;	// Set tomeoiut for no digis

	if ((Buffer->ORIGIN[6] & 1) == 0)	// End of Address
	{
		//	THERE ARE DIGIS TO PROCESS - COPY TO WORK AREA reversed, THEN COPY BACK
	
		memset(TEMPDIGI, 0, 57);		// CLEAR DIGI FIELD IN CASE RECONNECT

		ptr1 = &Buffer->ORIGIN[6];		// End of add 
		ptr2 = &TEMPDIGI[7 * 7];		// Last Temp Digi

		while((*ptr1 & 1) == 0)			// End of address bit
		{
			ptr1++;
			memcpy(ptr2, ptr1, 7);
			ptr2[6] &= 0x1e;			// Mask Repeated and Last bits
			ptr2 -= 7;
			ptr1 += 6;
		}
		
		//	LIST OF DIGI CALLS COMPLETE - COPY TO LINK CONTROL ENTRY

		n = PORT->PORTMAXDIGIS;

		ptr1 = ptr2 + 7;				// First in TEMPDIGIS
		ptr2 = &LINK->DIGIS[0];

		while (*ptr1)
		{
			if (n == 0)
			{
				// Too many for us
				
				CLEAROUTLINK(LINK);
				return;
			}

			memcpy(ptr2, ptr1, 7);
			ptr1 += 7;
			ptr2 += 7;
			n--;

			LINK->L2TIME += PORT->PORTT1;	// Adjust timeout for digis
		}
	}

	//	THIS MAY BE RESETTING A LINK - BEWARE OF CONVERTING A CROSSLINK TO 
	//	AN UPLINK AND CONFUSING EVERYTHING

	LINK->LINKPORT = PORT;

	if (LINK->LINKTYPE == 0)
		LINK->LINKTYPE = 1;			// Uplink

	LINK->L2TIMER = 0;				// CANCEL TIMER

	LINK->L2SLOTIM = T3;			// SET FRAME SENT RECENTLY

	LINK->LINKWINDOW = PORT->PORTWINDOW;

	RESET2(LINK);					// RESET ALL FLAGS

	LINK->L2STATE = 5;

	//	IF VERSION 1 MSG, SET FLAG

	if (MSGFLAG & VER1)
		LINK->VER1FLAG |= 1;

}

VOID L2SENDUA(struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER)
{
	L2SENDRESP(PORT, Buffer, ADJBUFFER, UA);
}

VOID L2SENDDM(struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER)
{
	L2SENDRESP(PORT, Buffer, ADJBUFFER, DM);
}

VOID L2SENDRESP(struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER, UCHAR CTL)
{
	//	QUEUE RESPONSE TO PORT CONTROL - MAY NOT HAVE A LINK ENTRY 

	//	SET APPROPRIATE P/F BIT 

	ADJBUFFER->CTL = CTL | PFBIT;

 	Buffer->LENGTH = (UCHAR *)ADJBUFFER - (UCHAR *)Buffer + MSGHDDRLEN + 15;	// SET UP BYTE COUNT

	L2SWAPADDRESSES(Buffer);			// SWAP ADDRESSES AND SET RESP BITS

	PUT_ON_PORT_Q(PORT, Buffer);

	return;
}

VOID L2SWAPADDRESSES(MESSAGE * Buffer)
{
	//	EXCHANGE ORIGIN AND DEST, AND REVERSE DIGIS (IF PRESENT)

	char TEMPFIELD[7];
	UCHAR * ptr1, * ptr2;
	UCHAR TEMPDIGI[57];

	memcpy(TEMPFIELD, Buffer->ORIGIN, 7);
	memcpy(Buffer->ORIGIN, Buffer->DEST, 7);
	memcpy(Buffer->DEST, TEMPFIELD, 7);

	Buffer->ORIGIN[6] &= 0x1e;			// Mask SSID
	Buffer->ORIGIN[6] |= 0xe0;			// Reserved and Response

	Buffer->DEST[6] &= 0x1e;			// Mask SSID
	Buffer->DEST[6] |= 0x60;			// Reserved 

	if ((TEMPFIELD[6] & 1) == 0)
	{
		//	THERE ARE DIGIS TO PROCESS - COPY TO WORK AREA reversed, THEN COPY BACK
	
		memset(TEMPDIGI, 0, 57);		// CLEAR DIGI FIELD IN CASE RECONNECT

		ptr1 = &Buffer->ORIGIN[6];		// End of add 
		ptr2 = &TEMPDIGI[7 * 7];		// Last Temp Digi

		while((*ptr1 & 1) == 0)			// End of address bit
		{
			ptr1++;
			memcpy(ptr2, ptr1, 7);
			ptr2[6] &= 0x1e;			// Mask Repeated and Last bits
			ptr2 -= 7;
			ptr1 += 6;
		}
		
		//	LIST OF DIGI CALLS COMPLETE - copy back

		ptr1 = ptr2 + 7;				// First in TEMPDIGIS
		ptr2 = &Buffer->CTL;

		while (*ptr1)
		{
			memcpy(ptr2, ptr1, 7);
			ptr1 += 7;
			ptr2 += 7;
		}

		*(ptr2 - 1) |= 1;				// End of addresses
	}
	else
	{
		Buffer->ORIGIN[6] |= 1;			// End of address
	}
}

BOOL InternalL2SETUPCROSSLINK(PROUTE ROUTE, int Retries)
{
	//	ROUTE POINTS TO A NEIGHBOUR - FIND AN L2 SESSION FROM US TO IT, OR INITIATE A NEW ONE

	struct _LINKTABLE * LINK;
	struct PORTCONTROL * PORT;
	int FRACK;

	if (FindLink(ROUTE->NEIGHBOUR_CALL, NETROMCALL, ROUTE->NEIGHBOUR_PORT, &LINK))
	{
		//	SESSION ALREADY EXISTS

		LINK->LINKTYPE = 3;				// MAKE SURE IT KNOWS ITS A CROSSLINK
		ROUTE->NEIGHBOUR_LINK = LINK;
		LINK->NEIGHBOUR = ROUTE;

		return TRUE;
	}

	//	SET UP NEW SESSION (OR RESET EXISTING ONE)

	if (LINK == NULL)
		return FALSE;						// No free links

	
	ROUTE->NEIGHBOUR_LINK = LINK;
	LINK->NEIGHBOUR = ROUTE;
	
	LINK->LINKPORT = PORT = GetPortTableEntryFromPortNum(ROUTE->NEIGHBOUR_PORT);

	if (PORT == NULL)
		return FALSE;						// maybe port has been deleted

	//	IF ROUTE HAS A FRACK, SET IT

	if (ROUTE->NBOUR_FRACK)
		FRACK = ROUTE->NBOUR_FRACK;
	else
		FRACK = PORT->PORTT1;

	LINK->L2TIME = FRACK;			// SET TIMER VALUE

	//	IF ROUTE HAS A WINDOW, SET IT

	if (ROUTE->NBOUR_MAXFRAME)
		LINK->LINKWINDOW = ROUTE->NBOUR_MAXFRAME;
	else
		LINK->LINKWINDOW = PORT->PORTWINDOW;

	LINK->L2STATE = 2;

	memcpy(LINK->LINKCALL, ROUTE->NEIGHBOUR_CALL, 7);
	memcpy(LINK->OURCALL, NETROMCALL, 7);

	if (ROUTE->NEIGHBOUR_DIGI1[0])
	{
		memcpy(LINK->DIGIS, ROUTE->NEIGHBOUR_DIGI1, 7);
		LINK->L2TIME += FRACK;
	}

	if (ROUTE->NEIGHBOUR_DIGI2[0])
	{
		memcpy(&LINK->DIGIS[7], ROUTE->NEIGHBOUR_DIGI1, 7);
		LINK->L2TIME += FRACK;
	}

	LINK->LINKTYPE = 3;					// CROSSLINK

	if (Retries)
		LINK->L2RETRIES = PORT->PORTN2 - Retries;

	SENDSABM(LINK);
	return TRUE;
}



BOOL L2SETUPCROSSLINKEX(PROUTE ROUTE, int Retries)
{
	//	Allows caller to specify number of times SABM should be sent

	return InternalL2SETUPCROSSLINK(ROUTE, Retries);
}

BOOL L2SETUPCROSSLINK(PROUTE ROUTE)
{
	return InternalL2SETUPCROSSLINK(ROUTE, 0);
}

VOID L2_PROCESS(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR CTL, UCHAR MSGFLAG)
{
	//	PROCESS LEVEL 2 PROTOCOL STUFF

	//	SEE IF COMMAND OR RESPONSE

	if ((MSGFLAG & CMDBIT) == 0)
	{

		// RESPONSE OR VERSION 1

		//	IF RETRYING, MUST ONLY ACCEPT RESPONSES WITH F SET (UNLESS RUNNING V1) 

		if ((CTL & PFBIT) || LINK->VER1FLAG == 1)
		{
			//	F SET or V1 - CAN CANCEL TIMER

			LINK->L2TIMER = 0;			// CANCEL LINK TIMER
		}
	}

	if (LINK->L2STATE == 3)
	{
	
	//	FRMR STATE - IF C(P) SEND FRMR, ELSE IGNORE

		if (CTL & PFBIT)
		{
			if (CTL == (FRMR | PFBIT))	//	if both ends in FRMR state, reset link
			{
				RESET2(LINK);

				LINK->L2STATE = 2;				// INITIALISING
				LINK->L2ACKREQ = 0;				// DONT SEND ANYTHING ELSE
				LINK->L2RETRIES = 0;			// ALLOW FULL RETRY COUNT FOR SABM

				L2SENDCOMMAND(LINK, SABM | PFBIT);
			}
		}

		if (MSGFLAG & CMDBIT)
		{
			//	SEND FRMR AGAIN

			SENDFRMR(LINK);
		}

		ReleaseBuffer(Buffer);
		return;
	}

	if (LINK->L2STATE >= 5)
	{
		//	LINK IN STATE 5 OR ABOVE - LINK RUNNING

		if ((CTL & 1) == 0)			// I frame
		{
			SDIFRM(LINK, PORT, Buffer, CTL, MSGFLAG);			// consumes buffer
			return;
		}
	
		if ((CTL & 2))			// U frame
		{
			SDUFRM(LINK, PORT, Buffer, CTL);					//consumes buffer
			return;
		}

		// ELSE SUPERVISORY, MASK OFF N(R) AND P-BIT
	
		switch (CTL & 0x0f)
		{
		case REJ:
			
			PORT->L2REJCOUNT++;

		case RR:
		case RNR:
				
			SFRAME(LINK, PORT, CTL, MSGFLAG);
			break;
		
		default:
			
			//	UNRECOGNISABLE COMMAND

			LINK->SDRBYTE = CTL;			// SAVE FOR FRMR RESPONSE
 			LINK->SDREJF |= SDINVC;			// SET INVALID COMMAND REJECT
			SDFRMR(LINK, PORT);				// PROCESS FRAME REJECT CONDITION
		}
		
		ReleaseBuffer(Buffer);
		return;
	}

	//	NORMAL DISCONNECT MODE

	//	COULD BE UA, DM - SABM AND DISC HANDLED ABOVE

	switch (CTL & ~PFBIT)
	{
	case UA:

		//	UA RECEIVED
			
		if (LINK->L2STATE == 2)
		{
			//	RESPONSE TO SABM - SET LINK  UP

			RESET2X(LINK);			// LEAVE QUEUED STUFF

			LINK->L2STATE = 5;
			LINK->L2TIMER = 0;		// CANCEL TIMER
			LINK->L2RETRIES = 0;
			LINK->L2SLOTIM, T3;		// SET FRAME SENT RECENTLY

			//	IF VERSION 1 MSG, SET FLAG

			if (MSGFLAG & VER1)
				LINK->VER1FLAG |= 1;

			//	TELL PARTNER CONNECTION IS ESTABLISHED
			
			SENDCONNECTREPLY(LINK);
			ReleaseBuffer(Buffer);
			return;
		}

		if (LINK->L2STATE == 4)				// DISCONNECTING?
		{
			InformPartner(LINK, NORMALCLOSE);	// SEND DISC TO OTHER END
			CLEAROUTLINK(LINK);
		}

		//	UA, BUT NOT IN STATE 2 OR 4 - IGNORE

		ReleaseBuffer(Buffer);
		return;
	
	case DM:
			
		//	DM RESPONSE - IF TO SABM, SEND BUSY MSG

		if (LINK->L2STATE == 2)
		{
			CONNECTREFUSED(LINK);	// SEND MESSAGE IF DOWNLINK
			return;
		}
			
		//	DM ESP TO DISC RECEIVED - OTHER END HAS LOST SESSION

		//	CLEAR OUT TABLE ENTRY - IF INTERNAL TNC, SHOULD SEND *** DISCONNECTED

		InformPartner(LINK, LINKLOST);		// SEND DISC TO OTHER END
		CLEAROUTLINK(LINK);

		ReleaseBuffer(Buffer);
		return;

	case FRMR:

	//	FRAME REJECT RECEIVED - LOG IT AND RESET LINK

		RESET2(LINK);
	
		LINK->L2STATE = 2;				// INITIALISING
		LINK->L2ACKREQ = 0;				// DONT SEND ANYTHING ELSE
		LINK->L2RETRIES = 0;			// ALLOW FULL RETRY COUNT FOR SABM

		PORT->L2FRMRRX++;

		L2SENDCOMMAND(LINK, SABM | PFBIT);
		return;

	default:
			
		//	ANY OTHER - IGNORE
		
		ReleaseBuffer(Buffer);
	}
}
			
VOID SDUFRM(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR CTL)
{
	//	PROCESS AN UNSEQUENCED COMMAND (IN LINK UP STATES)

	switch (CTL & ~PFBIT)
	{
	case UA:

		// DISCARD - PROBABLY REPEAT OF  ACK OF SABM

		break;

	case FRMR:

		//	FRAME REJECT RECEIVED - LOG IT AND RESET LINK

		RESET2(LINK);

		LINK->L2STATE = 2;				// INITIALISING
		LINK->L2ACKREQ = 0;				// DONT SEND ANYTHING ELSE
		LINK->L2RETRIES = 0;			// ALLOW FULL RETRY COUNT FOR SABM

		PORT->L2FRMRRX++;

		L2SENDCOMMAND(LINK, SABM | PFBIT);
		break;

	case DM:

		// DM RESPONSE - SESSION MUST HAVE GONE

		//	SEE IF CROSSLINK ACTIVE

		InformPartner(LINK, LINKLOST);		// SEND DISC TO OTHER END
		CLEAROUTLINK(LINK);	
		break;

	default:

		//	UNDEFINED COMMAND

		LINK->SDRBYTE = CTL;			// SAVE FOR FRMR RESPONSE
		LINK->SDREJF |= SDINVC;
		SDFRMR(LINK, PORT);		// PROCESS FRAME REJECT CONDITION

	}

	ReleaseBuffer(Buffer);
}
	

VOID SFRAME(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, UCHAR CTL, UCHAR MSGFLAG)
{
	//	CHECK COUNTS, AND IF RNR INDICATE _BUFFER SHORTAGE AT OTHER END

	if (LINK->SDREJF)			// ARE ANY REJECT FLAGS SET?
	{
		SDFRMR(LINK, PORT);		// PROCESS FRAME REJECT CONDITION
		return;
	}

	SDNRCHK(LINK, CTL);				// CHECK RECEIVED N(R)

	if (LINK->SDREJF)			// ARE ANY REJECT FLAGS SET NOW?
	{
		SDFRMR(LINK, PORT);		// PROCESS FRAME REJECT CONDITION
		return;
	}

	//	VALID RR/RNR RECEIVED

	LINK->L2FLAGS &= ~RNRSET;		//CLEAR RNR

	if ((CTL & 0xf) == RNR)
		LINK->L2FLAGS |= RNRSET;	//Set RNR

	if (MSGFLAG & CMDBIT)
	{
		//	ALWAYS REPLY TO RR/RNR/REJ COMMAND (even if no P bit ??)

		//	FIRST PROCESS RESEQ QUEUE

		//;	CALL	PROCESS_RESEQ

		// IGNORE IF AN 'F' HAS BEEN SENT RECENTLY

		if (LINK->LAST_F_TIME + 15 > REALTIMETICKS)
			return;			// DISCARD

		CTL = RR_OR_RNR(LINK);

		CTL |= LINK->LINKNR << 5;	// SHIFT N(R) TO TOP 3 BITS
		CTL |= PFBIT;

		L2SENDRESPONSE(LINK, CTL);

		LINK->L2SLOTIM = T3 + rand() % 15;	// SET FRAME SENT RECENTLY	

		LINK->L2ACKREQ = 0;				// CANCEL DELAYED ACKL2
	
		//	SAVE TIME IF 'F' SENT'

		LINK->LAST_F_TIME = REALTIMETICKS;

		return;
	}

	// Response

	if ((CTL & PFBIT) == 0 && LINK->VER1FLAG == 0)
	{
		//	RESPONSE WITHOUT P/F DONT RESET N(S) (UNLESS V1)
	
		return;

	}

	//	RESPONSE WITH P/F - MUST BE REPLY TO POLL FOLLOWING TIMEOUT OR I(P)

	LINK->L2FLAGS &= ~POLLSENT;				// CLEAR I(P) SET

	//	THERE IS A PROBLEM WITH REPEATED RR(F), SAY CAUSED BY DELAY AT L1

	//	AS FAR AS I CAN SEE, WE SHOULD ONLY RESET N(S) IF AN RR(F) FOLLOWS
	//	AN RR(P) AFTER A TIMEOUT - AN RR(F) FOLLOWING AN I(P) CANT POSSIBLY
	//	INDICATE A LOST FRAME. ON THE OTHER HAND, A REJ(F) MUST INDICATE
	//	A LOST FRAME. So dont reset NS if not retrying, unless REJ

	if ((CTL & 0xf) == REJ || LINK->L2RETRIES)
	{
		RESETNS(LINK, (CTL >> 5) & 7);	// RESET N(S) AND COUNT RETRIED FRAMES

		LINK->L2RETRIES = 0;
		LINK->L2TIMER = 0;			// WILL RESTART TIMER WHEN RETRY SENT
	}

	if ((CTL & 0xf) == RNR)
	{
		//	Dont Clear timer on receipt of RNR(F), spec says should poll for clearing of busy,
		//	and loss of subsequent RR will cause hang. Perhaps should set slightly longer time??
		//	Timer may have been cleared earlier, so restart it

		LINK->L2TIMER = LINK->L2TIME;
	}
}


//***	PROCESS AN INFORMATION FRAME


VOID SDIFRM(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR CTL, UCHAR MSGFLAG)
{
	int NS;
	
	if (LINK->SDREJF)			// ARE ANY REJECT FLAGS SET?
	{
		SDFRMR(LINK, PORT);		// PROCESS FRAME REJECT CONDITION
		ReleaseBuffer(Buffer);
		return;
	}

	SDNRCHK(LINK, CTL);				// CHECK RECEIVED N(R)

	if (LINK->SDREJF)			// ARE ANY REJECT FLAGS SET NOW?
	{
		SDFRMR(LINK, PORT);		// PROCESS FRAME REJECT CONDITION
		ReleaseBuffer(Buffer);
		return;
	}

	LINK->SESSACTIVE = 1;		// SESSION IS DEFINITELY SET UP

	NS = (CTL >> 1) & 7;			// ISOLATE RECEIVED N(S)
	
	if (NS != LINK->LINKNR)		// EQUAL TO OUR N(R)?
	{
		//	BAD FRAME, SEND REJ (AFTER RESPTIME - OR WE MAY SEND LOTS!)

		//	ALSO SAVE THE FRAME - NEXT TIME WE MAY GET A DIFFERENT SUBSET
		//  AND SOON WE WILL HANDLE SREJ

		PORT->L2OUTOFSEQ++;

		LINK->L2STATE = 6;

		//	IF RUNNING VER1, AND OTHER END MISSES THIS REJ, LINK WILL FAIL
		//	SO TIME OUT REJ SENT STATE (MUST KEEP IT FOR A WHILE TO AVOID
		//	'MULTIPLE REJ' PROBLEM)
	
		if (LINK->VER1FLAG == 1)
			LINK->REJTIMER = TENSECS;

		//	SET ACK REQUIRED TIMER - REJ WILL BE SENT WHEN IT EXPIRES

		LINK->L2ACKREQ = THREESECS;		// EXTRA LONG RESPTIME, AS SENDING TOO MANY REJ'S IS SERIOUS

//		if (LINK->FRAMES[NS])
//		{
//			// Already have a copy, so discard this one
//			
//			ReleaseBuffer(Buffer);
//		}
//		else
//			LINK->RXFRAMES[NS] = Buffer;

		ReleaseBuffer(Buffer);
	
		goto CheckPF;
	}

	//	IN SEQUENCE FRAME - CANCEL REJ

	if (LINK->L2STATE == 6)				// REJ?
	{
		LINK->L2STATE = 5;
		LINK->L2FLAGS &= ~REJSENT;
	}

	PROC_I_FRAME(LINK, PORT, Buffer);		// Passes on  or releases Buffer

CheckPF:

	if (LINK->L2FLAGS & REJSENT)
		return;						// DONT SEND ANOTHER TILL REJ IS CANCELLED

	if (CTL & PFBIT)
	{
		if (LINK->L2STATE == 6)
			LINK->L2FLAGS |= REJSENT;	// Set "REJ Sent"

		LINK->L2ACKREQ = 0;					// CANCEL RR NEEDED

		SEND_RR_RESP(LINK, PFBIT);
	
		//	RECORD TIME

		LINK->LAST_F_TIME = REALTIMETICKS;
	}
}


VOID PROC_I_FRAME(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer)
{
	int Length;
	char * Info;
	UCHAR PID;
	struct DATAMESSAGE * Msg = (struct DATAMESSAGE *)Buffer;
	UCHAR * EOA;
	int n = 8;					// Max Digis

	LINK->LINKNR++;				// INCREMENT OUR N(R)
	LINK->LINKNR &= 7;			//  MODULO 8

	//	ATTACH I FRAMES TO LINK TABLE RX QUEUE - ONLY DATA IS ADDED (NOT ADDRESSES)

	//	IF DISC PENDING SET, IGNORE FRAME

	if (LINK->L2FLAGS & DISCPENDING)
	{
		ReleaseBuffer(Buffer);
		return;
	}

	// Copy data down the buffer so PID comes after Header (DATAMESSAGE format)

	Length = Buffer->LENGTH - MSGHDDRLEN - 15;	// Buffer Header + addrs + CTL
	Info  = &Buffer->PID;

	// Adjust for DIGIS

	EOA = &Buffer->ORIGIN[6];		// End of address Bit

	while (((*EOA & 1) == 0) && n--)
	{
		Length -= 7;
		Info += 7;
		EOA += 7;
	}

	PID = EOA[2];

	switch(PID)
	{
	case 0xcc:
	case 0xcd:
		
		// IP Message

		if (n < 8)			// If digis, move data back down buffer
		{
			memmove(&Buffer->PID, &EOA[2], Length);
			Buffer->LENGTH -= (&EOA[2] - &Buffer->PID);
		}

		Q_IP_MSG( Buffer);
		break;

	case 8:

		// NOS FRAGMENTED IP

		if (n < 8)			// If digis, move data back down buffer
		{
			memmove(&Buffer->PID, &EOA[2], Length);
			Buffer->LENGTH -= (&EOA[2] - &Buffer->PID);
		}

		C_Q_ADD(&LINK->L2FRAG_Q, Buffer);

		if (Buffer->L2DATA[0] == 0)
		{
			//	THERE IS A WHOLE MESSAGE ON FRAG_Q - PASS TO IP

			while(LINK->L2FRAG_Q)
			{
				Buffer = Q_REM(&LINK->L2FRAG_Q);
				Q_IP_MSG( Buffer);
			}
		}
		break;

	default:

		if (Length < 1 || Length > 257)
		{
			ReleaseBuffer(Buffer);
			return;
		}

		// Copy Data back over

		memmove(&Msg->PID, Info, Length);

		Buffer->LENGTH = Length + MSGHDDRLEN;
 
		C_Q_ADD(&LINK->RX_Q, Buffer);
	}

	LINK->L2ACKREQ = PORT->PORTT2;			// SET RR NEEDED
	LINK->KILLTIMER = 0;					// RESET IDLE LINK TIMER
}

//***	CHECK RECEIVED N(R) COUNT

VOID SDNRCHK(struct _LINKTABLE * LINK, UCHAR CTL)
{
	UCHAR NR = (CTL >> 5) & 7;

	if (NR >= LINK->LINKWS)			// N(R) >= WINDOW START?
	{
		//	N(R) ABOVE OR EQUAL TO WINDOW START - OK IF NOT ABOVE N(S), OR N(S) BELOW WS

		if (NR > LINK->LINKNS)			// N(R) <= WINDOW END?
		{
			//	N(R) ABOVE N(S) - DOES COUNT WRAP?

			if (LINK->LINKNS >= LINK->LINKWS)		// Doesnt wrap
				goto BadNR;
		}

GoodNR:
		
		LINK->LINKWS = NR;				// NEW WINDOW START = RECEIVED N(R)
		ACKMSG(LINK);					// Remove any acjed messages
		return;
	}

	//	N(R) LESS THAN WINDOW START - ONLY OK IF WINDOW	WRAPS

	if (NR <= LINK->LINKNS)				// N(R) <= WINDOW END?
		goto GoodNR;

BadNR:

	//	RECEIVED N(R) IS INVALID
	
	LINK->SDREJF |= SDNRER;			// FLAG A REJECT CONDITION
	LINK->SDRBYTE = CTL;			// SAVE FOR FRMR RESPONSE
}

VOID RESETNS(struct _LINKTABLE * LINK, UCHAR NS)
{
	int Resent = (LINK->LINKNS - NS) & 7;	// FRAMES TO RESEND

	LINK->LINKNS = NS;			// RESET N(S)

	if (LINK->LINKTYPE == 3)	// mode-Node
	{
		if (LINK->NEIGHBOUR)
			LINK->NEIGHBOUR->NBOUR_RETRIES += Resent;
	}
}

int COUNT_AT_L2(struct _LINKTABLE * LINK)
{
	//	COUNTS FRAMES QUEUED ON AN L2 SESSION (IN BX)

	int count = 0, abovelink = 0;
	int n = 0;

	if (LINK == NULL)
		return 0;
	
	abovelink = C_Q_COUNT((UINT *)&LINK->TX_Q);

	//	COUNT FRAMES IN TSLOTS

	while (n < 8)
	{
		if (LINK->FRAMES[n])
			count++;
		n++;
	}

//	ADD	AL,AH			; TOTAL IN AL, NUMBER ABOVE LINK IN AH

	return abovelink + count;
}

//***	RESET HDLC AND PURGE ALL QUEUES ETC.

VOID RESET2X(struct _LINKTABLE * LINK)
{
	LINK->SDREJF = 0;			// CLEAR FRAME REJECT FLAGS
	LINK->LINKWS = 0;			// CLEAR WINDOW POINTERS
	LINK->LINKOWS = 0;
	LINK->LINKNR = 0;			// CLEAR N(R)
	LINK->LINKNS = 0;			// CLEAR N(S)
	LINK->SDTSLOT= 0;
	LINK->L2STATE = 5;			// RESET STATE
	LINK->L2FLAGS = 0;
}


VOID CLEARL2QUEUES(struct _LINKTABLE * LINK)
{
	//	GET RID OF ALL FRAMES THAT ARE QUEUED

	int n = 0;

	while (n < 8)
	{
		while (LINK->FRAMES[n])
			ReleaseBuffer(Q_REM(&LINK->FRAMES[n]));
		while (LINK->RXFRAMES[n])
			ReleaseBuffer(Q_REM(&LINK->RXFRAMES[n]));
		n++;
	}

	//	GET RID OF ALL FRAMES THAT ARE
	//	QUEUED ON THE TX HOLDING QUEUE, RX QUEUE AND LEVEL 3 QUEUE


	while (LINK->TX_Q)
		ReleaseBuffer(Q_REM(&LINK->TX_Q));

	while (LINK->RX_Q)
		ReleaseBuffer(Q_REM(&LINK->RX_Q));

}

VOID RESET2(struct _LINKTABLE * LINK)
{
	CLEARL2QUEUES(LINK);
	RESET2X(LINK);
}

VOID SENDSABM(struct _LINKTABLE * LINK)
{
	L2SENDCOMMAND(LINK, SABM | PFBIT);
}


VOID PUT_ON_PORT_Q(struct PORTCONTROL * PORT, MESSAGE * Buffer)
{
	//	TIME STAMP IT
	
	time(&Buffer->Timestamp);

	if (PORT->TXPORT)
	{
		Buffer->PORT = PORT->TXPORT;		// update port no in header

		PORT = GetPortTableEntryFromPortNum(PORT->TXPORT);

		if (PORT == NULL)
		{
			ReleaseBuffer(Buffer);
			return;
		}
	}
	C_Q_ADD(&PORT->PORTTX_Q, (UINT *)Buffer);
}


UCHAR * SETUPADDRESSES(struct _LINKTABLE * LINK, PMESSAGE Msg)
{
	//	COPY ADDRESSES FROM LINK TABLE TO MESSAGE _BUFFER

	UCHAR * ptr1 = &LINK->DIGIS[0];
	UCHAR * ptr2 = &Msg->CTL;
	int Digis  = 8;

	memcpy(&Msg->DEST[0], &LINK->LINKCALL[0], 14); // COPY DEST AND ORIGIN

	Msg->DEST[6] |= 0x60;
	Msg->ORIGIN[6] |= 0x60;

	while (Digis)
	{
		if (*(ptr1))			// any more to copy?
		{
			memcpy(ptr2, ptr1, 7);
			ptr1 += 7;
			ptr2 += 7;
			Digis--;
		}
		else
			break;
	}

	*(ptr2 - 1) |= 1;		// SET END OF ADDRESSES

	return ptr2;			// Pointer to CTL
}


VOID SDETX(struct _LINKTABLE * LINK)
{
	// Start sending frsmes if possible

	struct PORTCONTROL * PORT;
	int Outstanding;
	UCHAR * ptr1, * ptr2;
	UCHAR CTL;
	int count;
	MESSAGE * Msg;
	MESSAGE * Buffer;

	//	DONT SEND IF RESEQUENCING RECEIVED FRAMES - CAN CAUSE FRMR PROBLEMS

//	if (LINK->L2RESEQ_Q)
//		return;
	
	Outstanding = LINK->LINKWS - LINK->LINKOWS;

	if (Outstanding < 0)
		Outstanding += 8;		// allow fro wrap

	if (Outstanding >= LINK->LINKWINDOW)		// LIMIT
		return;

	//	See if we can load any more frames into the frame holding q

	while (LINK->TX_Q && LINK->FRAMES[LINK->SDTSLOT] == NULL)
	{
		Msg = Q_REM(&LINK->TX_Q);
		Msg->CHAIN = NULL;
		LINK->FRAMES[LINK->SDTSLOT] = Msg;
		LINK->SDTSLOT ++;
		LINK->SDTSLOT &= 7;
	}
	
	// dont send while poll outstanding

	while ((LINK->L2FLAGS & POLLSENT) == 0)
	{
		Msg = LINK->FRAMES[LINK->LINKNS];	// is next frame available?

		if (Msg == NULL)
			return;

		// send the frame

		//	GET BUFFER FOR COPY OF MESSAGE - HAVE TO KEEP ORIGINAL FOR RETRIES	
		
		Buffer = GetBuff();

		if (Buffer == NULL)
			return;

		ptr2 = SETUPADDRESSES(LINK, Buffer);	// copy addresses

		// ptr2  NOW POINTS TO COMMAND BYTE

		//	GOING TO SEND I FRAME - WILL ACK ANY RECEIVED FRAMES

		LINK->L2ACKREQ = 0;			// CLEAR ACK NEEDED
		LINK->L2SLOTIM = T3 + rand() % 15;		// SET FRAME SENT RECENTLY
		LINK->KILLTIMER = 0;		// RESET IDLE CIRCUIT TIMER

		CTL = LINK->LINKNR << 5;	// GET CURRENT N(R), SHIFT IT TO TOP 3 BITS			
		CTL |= LINK->LINKNS << 1;	// BITS 1-3 OF CONTROL BYTE

		LINK->LINKNS++;				// INCREMENT NS
		LINK->LINKNS &= 7;			// mod 8

		//	SET P BIT IF END OF WINDOW OR NO MORE TO SEND

		if (LINK->VER1FLAG == 0)	// NO POLL BIT IF V1
		{
			Outstanding = LINK->LINKNS - LINK->LINKOWS;

			if (Outstanding < 0)
				Outstanding += 8;		// allow for wrap

			// if at limit, or no more to send, set P)
	
			if (Outstanding >= LINK->LINKWINDOW || LINK->FRAMES[LINK->LINKNS] == NULL)
			{
				CTL |= PFBIT;
				LINK->L2FLAGS |= POLLSENT;
				LINK->L2TIMER = ONEMINUTE;	// (RE)SET TIMER

				// FLAG BUFFER TO CAUSE TIMER TO BE RESET AFTER SEND (or ACK if ACKMODE)

				Buffer->Linkptr = LINK;
			}
		}
	
		*(ptr2++) = CTL;		// TO DATA (STARTING WITH PID)

		count = Msg->LENGTH - MSGHDDRLEN;
	
		if (count > 0)			// SHOULD ALWAYS BE A PID, BUT BETTER SAFE THAN SORRY
		{
			ptr1 = (UCHAR *)Msg;
			ptr1 += MSGHDDRLEN;
			memcpy(ptr2, ptr1, count);
		}

		Buffer->DEST[6] |= 0x80;		// SET COMMAND

		Buffer->LENGTH = ptr2 + count - (UCHAR *)Buffer;		// SET NEW LENGTH

		LINK->L2TIMER = ONEMINUTE;	// (RE)SET TIMER

		PORT = LINK->LINKPORT;

		if (PORT)
		{
			Buffer->PORT = PORT->PORTNUMBER;
			PUT_ON_PORT_Q(PORT, Buffer);
		}
		else
		{
			Buffer->Linkptr = 0;
			ReleaseBuffer(Buffer);
		}
	}
}

VOID L2TimerProc()
{
	int i = MAXLINKS;
	struct _LINKTABLE * LINK = LINKS;
	struct PORTCONTROL * PORT = PORTTABLE;

	while (i--)
	{
		if (LINK->LINKCALL[0] == 0)
		{
			LINK++;
			continue;
		}		

		//	CHECK FOR TIMER EXPIRY OR BUSY CLEARED  

		PORT = LINK->LINKPORT;
	
		if (PORT == NULL)
		{
			LINK++;
			continue;				// just ion case!!
		}

		if (LINK->L2TIMER)
		{
			LINK->L2TIMER--;
			if (LINK->L2TIMER == 0)
			{
				L2TIMEOUT(LINK, PORT);
				LINK++;
				continue;
			}		
		}
		else
		{
			// TIMER NOT RUNNING - MAKE SURE STATE NOT BELOW 5 - IF
			// IT IS, SOMETHING HAS GONE WRONG, AND LINK WILL HANG FOREVER

			if (LINK->L2STATE < 5 && LINK->L2STATE != 2)	// 2 = CONNECT - PROBABLY TO CQ
				LINK->L2TIMER = 2;							// ARBITRARY VALUE
		}

		//	TEST FOR RNR SENT, AND NOT STILL BUSY

		if (LINK->L2FLAGS & RNRSENT)
		{
			//	Was busy

			if (RR_OR_RNR(LINK) != RNR)		//  SEE IF STILL BUSY
			{
				// Not still busy - tell other end

				//	Just sending RR will hause a hang of RR is missed, and other end does not poll on Busy
				//	Try sending RR CP, so we will retry if not acked

				LINK->L2ACKREQ = 0;					// CLEAR ANY DELAYED ACK TIMER
	
				if (LINK->L2RETRIES == 0)			// IF RR(P) OUTSTANDING WILl REPORT ANYWAY
				{
					SendSupervisCmd(LINK);
					LINK++;
					continue;
				}
			}
		}
		else
		{
			// NOT	BUSY

			if (LINK->L2ACKREQ)							// DELAYED ACK TIMER
			{
				if (LINK->L2RETRIES == 0)				// DONT SEND RR RESPONSE WHILEST RR(P) OUTSTANDING
				{
					LINK->L2ACKREQ--;
					if (LINK->L2ACKREQ == 0)
					{
						SEND_RR_RESP(LINK, 0);	// NO F BIT
						LINK++;
						continue;
					}
				}
			}
		}

		//	CHECK FOR REJ TIMEOUT

		if (LINK->REJTIMER)
		{
			LINK->REJTIMER--;
			if (LINK->REJTIMER == 0)			// {REJ HAS TIMED OUT (THIS MUST BE A VERSION 1 SESSION)
			{
				// CANCEL REJ STATE

				if (LINK->L2STATE == 6)				// REJ?
					LINK->L2STATE = 5;				// CLEAR REJ 
			}
		}

		// See if time for link validation poll

		if (LINK->L2SLOTIM)
		{
			LINK->L2SLOTIM--;
			if (LINK->L2SLOTIM == 0)			// Time to poll
			{
				SendSupervisCmd(LINK);
				LINK++;
					continue;
			}
		}

		// See if idle too long

		LINK->KILLTIMER++;

		if (L2KILLTIME && LINK->KILLTIMER > L2KILLTIME)
		{
			// CIRCUIT HAS BEEN IDLE TOO LONG - SHUT IT DOWN

			LINK->KILLTIMER = 0;
			LINK->L2TIMER = 1;		// TO FORCE DISC
			LINK->L2STATE = 4;		// DISCONNECTING

			//	TELL OTHER LEVELS

			InformPartner(LINK, NORMALCLOSE);
		}
		LINK++;
	}
}

VOID SendSupervisCmd(struct _LINKTABLE * LINK)
{
	// Send Super Command RR/RNR/REJ(P)

	UCHAR CTL;
	
	if (LINK->VER1FLAG == 1)
	{
		//	VERSION 1 TIMEOUT

		//	RESET TO RESEND I FRAMES

		LINK->LINKNS = LINK->LINKOWS;

		SDETX(LINK);				// PREVENT FRMR (I HOPE)
	}

	//	SEND RR COMMAND - EITHER AS LINK VALIDATION POLL OR FOLLOWING TIMEOUT

	LINK->L2ACKREQ = 0;			// CLEAR ACK NEEDED

	CTL = RR_OR_RNR(LINK);

//	MOV	L2STATE[EBX],5			; CANCEL REJ - ACTUALLY GOING TO 'PENDING ACK'

	CTL |= LINK->LINKNR << 5;	// SHIFT N(R) TO TOP 3 BITS
	CTL |= PFBIT;

	LINK->L2FLAGS |= POLLSENT;

	L2SENDCOMMAND(LINK, CTL);

	LINK->L2SLOTIM = T3 + rand() % 15;		// SET FRAME SENT RECENTLY	
}

void SEND_RR_RESP(struct _LINKTABLE * LINK, UCHAR PF)
{
	UCHAR CTL;
	
	CTL = RR_OR_RNR(LINK);

//	MOV	L2STATE[EBX],5			; CANCEL REJ - ACTUALLY GOING TO 'PENDING ACK'

	CTL |= LINK->LINKNR << 5;	// SHIFT N(R) TO TOP 3 BITS
	CTL |= PF;

	L2SENDRESPONSE(LINK, CTL);
	
	ACKMSG(LINK);		// SEE IF STILL WAITING FOR ACK
}

VOID ACKMSG(struct _LINKTABLE * LINK)
{
	//	RELEASE ANY ACKNOWLEDGED FRAMES

	while (LINK->LINKOWS != LINK->LINKWS)	// is OLD WINDOW START EQUAL TO NEW WINDOW START?
	{
		// No, so frames to ack

		if (LINK->FRAMES[LINK->LINKOWS])
			ReleaseBuffer(Q_REM(&LINK->FRAMES[LINK->LINKOWS]));
		else
		{
			char Call1[12], Call2[12];

			Call1[ConvFromAX25(LINK->LINKCALL, Call1)] = 0;
			Call2[ConvFromAX25(LINK->OURCALL, Call2)] = 0;

			Debugprintf("Missing frame to ack Seq %d Calls %s %s", LINK->LINKOWS, Call1, Call2);
		}

		LINK->LINKOWS++;			// INCREMENT OLD WINDOW START
		LINK->LINKOWS &= 7;			// MODULO 8

		//	SOMETHING HAS BEEN ACKED - RESET RETRY COUNTER

		if (LINK->L2RETRIES)
			LINK->L2RETRIES = 1;	// MUSTN'T SET TO ZERO - COULD CAUSE PREMATURE RETRANSMIT

	}

	if (LINK->LINKWS != LINK->LINKNS)		// IS N(S) = NEW WINDOW START?
	{
		//	NOT ALL I-FRAMES HAVE BEEN ACK'ED - RESTART TIMER

		LINK->L2TIMER = LINK->L2TIME;
		return;
	}

	//	ALL FRAMES HAVE BEEN ACKED - CANCEL TIMER UNLESS RETRYING
	//	   IF RETRYING, MUST ONLY CANCEL WHEN RR(F) RECEIVED

	if (LINK->VER1FLAG == 1 || LINK->L2RETRIES == 0)	 // STOP TIMER IF LEVEL 1 or not retrying
	{
		LINK->L2TIMER = 0;
		LINK->L2FLAGS &= ~POLLSENT;	// CLEAR I(P) SET (IN CASE TALKING TO OLD BPQ!)
	}

	//	IF DISCONNECT REQUEST OUTSTANDING, AND NO FRAMES ON TX QUEUE,  SEND DISC

	if (LINK->L2FLAGS & DISCPENDING && LINK->TX_Q == 0)
	{
		LINK->L2FLAGS &=  ~DISCPENDING;

		LINK->L2TIMER = 1;		// USE TIMER TO SEND DISC
		LINK->L2STATE = 4;		// DISCONNECTING
	}
}

VOID CONNECTFAILED();
	
VOID L2TIMEOUT(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT)
{
	//	TIMER EXPIRED

	//	IF LINK UP (STATE 5 OR ABOVE) SEND RR/RNR AS REQUIRED
	//	IF S2, REPEAT SABM
	//  IF S3, REPEAT FRMR
	//  IF S4, REPEAT DISC


	PORT->L2TIMEOUTS++;			// FOR STATS	

	if (LINK->L2STATE <= 1)		// NO IDLE STATE
		return;

	if (LINK->L2STATE == 2)
	{
		//	CONNECTING

		LINK->L2RETRIES++;
		if (LINK->L2RETRIES >= PORT->PORTN2)
		{
			//	RETRIED N2 TIMES - Give up

			CONNECTFAILED(LINK);		// TELL LEVEL 4 IT FAILED
			CLEAROUTLINK(LINK);
			return;
		}

		SENDSABM(LINK);
		return;
	}

	if (LINK->L2STATE == 4)
	{
		//	DISCONNECTING

		LINK->L2RETRIES++;
		if (LINK->L2RETRIES >= PORT->PORTN2)
		{
			//	RETRIED N2 TIMES - JUST CLEAR OUT LINK
		
			CLEAROUTLINK(LINK);
			return;
		}

		L2SENDCOMMAND(LINK, DISC | PFBIT);
		return;
	}
	
	if (LINK->L2STATE == 3)
	{
		//	FRMR

		LINK->L2RETRIES++;
		if (LINK->L2RETRIES >= PORT->PORTN2)
		{
			//	RETRIED N2 TIMES - RESET LINK

			LINK->L2RETRIES = 0;
			LINK->L2STATE = 2;
			SENDSABM(LINK);
			return;
		}
	}

	//	STATE 5 OR ABOVE

	//	SEND RR(P) UP TO N2 TIMES

	LINK->L2RETRIES++;
	if (LINK->L2RETRIES >= PORT->PORTN2)
	{
		//	RETRIED N TIMES SEND A COUPLE OF DISCS AND THEN CLOSE

		InformPartner(LINK, RETRIEDOUT);	// TELL OTHER END ITS GONE

		LINK->L2RETRIES =-2;
		LINK->L2STATE = 4;			// CLOSING

		L2SENDCOMMAND(LINK, DISC | PFBIT);
		return;
	}
	
	SendSupervisCmd(LINK);
}

VOID SDFRMR(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT)
{
	PORT->L2FRMRTX++;

	LINK->L2STATE = 3;				// ENTER FRMR STATE

	LINK->L2TIMER = LINK->L2TIME;	//SET TIMER

	SENDFRMR(LINK);
}

VOID SENDFRMR(struct _LINKTABLE * LINK)
{
	//	RESEND FRMR

	struct PORTCONTROL * PORT;
	MESSAGE * Buffer;
	UCHAR * ptr;

	Buffer = SETUPL2MESSAGE(LINK, FRMR);

	if (Buffer == NULL)
		return;

	Buffer->ORIGIN[6] |= 0x80;		// SET RESPONSE

	ptr = &Buffer->PID;

	*(ptr++) = LINK->SDRBYTE;			// MOVE REJECT C-BYTE

	*(ptr++) = LINK->LINKNR << 5 | LINK->LINKNS << 1;

	*(ptr++) = LINK->SDREJF;			// MOVE REJECT FLAGS

	Buffer->LENGTH += 3;

	PORT = LINK->LINKPORT;
	Buffer->PORT = PORT->PORTNUMBER;

	if (PORT)
		PUT_ON_PORT_Q(PORT, Buffer);
	else
		ReleaseBuffer(Buffer);

	return;
}

VOID CLEAROUTLINK(struct _LINKTABLE * LINK)
{
	CLEARL2QUEUES(LINK);				// TO RELEASE ANY BUFFERS

	memset(LINK, 0, sizeof(struct _LINKTABLE));
}

VOID L2SENDCOMMAND(struct _LINKTABLE * LINK, int CMD)
{
	//	SEND COMMAND IN CMD

	struct PORTCONTROL * PORT;
	MESSAGE * Buffer;

	if (LINK->LINKPORT == 0)
		return;						//??? has been zapped

	Buffer = SETUPL2MESSAGE(LINK, CMD);

	if (Buffer == NULL)
	{
		// NO BUFFERS - SET TIMER TO FORCE RETRY

		if (CMD & PFBIT)				// RESPONSE EXPECTED?
			LINK->L2TIMER = 10*3;		// SET TIMER

		return;
	}

	Buffer->DEST[6] |= 0x80;		// SET COMMAND

	if (CMD & PFBIT)				// RESPONSE EXPECTED?
	{
		LINK->L2TIMER = ONEMINUTE;	// (RE)SET TIMER

		// FLAG BUFFER TO CAUSE TIMER TO BE RESET AFTER SEND

		Buffer->Linkptr = LINK;
	}

	PORT = LINK->LINKPORT;

	if (PORT)
	{
		Buffer->PORT = PORT->PORTNUMBER;
		PUT_ON_PORT_Q(PORT, Buffer);
	}
	else
	{
		Buffer->Linkptr = 0;
		ReleaseBuffer(Buffer);
	}
}

VOID L2SENDRESPONSE(struct _LINKTABLE * LINK, int CMD)
{
	//	SEND Response IN CMD

	struct PORTCONTROL * PORT;
	MESSAGE * Buffer;

	Buffer = SETUPL2MESSAGE(LINK, CMD);

	if (Buffer == NULL)
	{
		// NO BUFFERS - SET TIMER TO FORCE RETRY

		if (CMD & PFBIT)				// RESPONSE EXPECTED?
			LINK->L2TIMER = 10*3;		// SET TIMER

		return;
	}

	Buffer->ORIGIN[6] |= 0x80;		// SET RESPONSE

	LINK->L2SLOTIM = T3 + rand() % 15;	// SET FRAME SENT RECENTLY

	PORT = LINK->LINKPORT;
	Buffer->PORT = PORT->PORTNUMBER;

	if (PORT)
		PUT_ON_PORT_Q(PORT, Buffer);
	else
		ReleaseBuffer(Buffer);

}


MESSAGE * SETUPL2MESSAGE(struct _LINKTABLE * LINK, UCHAR CMD)
{
	MESSAGE * Buffer;
	UCHAR * ptr;

	Buffer = GetBuff();

	if (Buffer == NULL)
		return NULL;

	ptr = SETUPADDRESSES(LINK, Buffer);	// copy addresses

	// ptr  NOW POINTS TO COMMAND BYTE

	*(ptr)++ = CMD;

	Buffer->LENGTH = ptr  - (UCHAR *)Buffer;		// SET LENGTH

	return Buffer;
}


VOID L3LINKCLOSED(struct _LINKTABLE * LINK, int Reason);

VOID InformPartner(struct _LINKTABLE * LINK, int Reason)
{
	//	LINK IS DISCONNECTING - IF THERE IS A CROSSLINK, SEND DISC TO IT

	if (LINK->LINKTYPE == 3)
	{
		L3LINKCLOSED(LINK, Reason);
		return;
	}
	
	if (LINK->CIRCUITPOINTER)
		CloseSessionPartner(LINK->CIRCUITPOINTER);

}


UINT RR_OR_RNR(struct _LINKTABLE * LINK)
{
	UCHAR Temp;
	TRANSPORTENTRY * Session;
	
	LINK->L2FLAGS &= ~RNRSENT;

	//	SET UP APPROPRIATE SUPER COMMAND

	if (LINK->LINKTYPE == 3)

		// Node to Node - only busy if short of buffers

		goto CHKBUFFS;

//	UP OR DOWN LINK - SEE IF SESSION IS BUSY


	if (LINK->CIRCUITPOINTER == 0)
		goto CHKBUFFS;				// NOT CONNECTED

	Session = LINK->CIRCUITPOINTER;	// TO CIRCUIT ENTRY

	Temp = CHECKIFBUSYL2(Session);		//TARGET SESSION BUSY?
	
	if (Temp & L4BUSY)
		goto SENDRNR;			// BUSY

CHKBUFFS:

	if (QCOUNT < 20)
		goto SENDRNR;			// NOT ENOUGH

	//	SEND REJ IF IN REJ STATE

	if (LINK->L2STATE == 6)
		return REJ;

	return RR;

SENDRNR:

	LINK->L2FLAGS |= RNRSENT;		// REMEMBER

	return RNR;
}


VOID ConnectFailedOrRefused(struct _LINKTABLE * LINK, char * Msg);

VOID CONNECTFAILED(struct _LINKTABLE * LINK)
{
	ConnectFailedOrRefused(LINK, "Failure with");
}
VOID CONNECTREFUSED(struct _LINKTABLE * LINK)
{
	ConnectFailedOrRefused(LINK, "Busy from");
}

VOID L3CONNECTFAILED();

VOID ConnectFailedOrRefused(struct _LINKTABLE * LINK, char * Msg)
{
	//	IF DOWNLINK, TELL PARTNER
	//	IF CROSSLINK, TELL ROUTE CONTROL

	struct DATAMESSAGE * Buffer;
	UCHAR * ptr1;
	char Normcall[10];
	TRANSPORTENTRY * Session;
	TRANSPORTENTRY * InSession;

	if (LINK->LINKTYPE == 3)
	{
		L3CONNECTFAILED(LINK);		// REPORT TO LEVEL 3
		return;
	}
	
	if (LINK->CIRCUITPOINTER == 0)	// No crosslink??
		return;

	Buffer = GetBuff();

	if (Buffer == NULL)
		return;

	// SET UP HEADER

	Buffer->PID = 0xf0;
	
	ptr1 = SetupNodeHeader(Buffer);

	Normcall[ConvFromAX25(LINK->LINKCALL, Normcall)] = 0;

	ptr1 += sprintf(ptr1, "%s %s\r", Msg, Normcall);

	Buffer->LENGTH = ptr1 - (UCHAR *)Buffer;

	Session = LINK->CIRCUITPOINTER;			// GET CIRCUIT TABLE ENTRY
	InSession = Session->L4CROSSLINK;		// TO INCOMMONG SESSION

	CLEARSESSIONENTRY(Session);

	if (InSession)
	{
		InSession->L4CROSSLINK = NULL;		// CLEAR REVERSE LINK
		C_Q_ADD(&InSession->L4TX_Q, Buffer);
		PostDataAvailable(InSession);
	}
	else
		ReleaseBuffer(Buffer);
}

VOID SENDCONNECTREPLY(struct _LINKTABLE * LINK)
{
	//	LINK SETUP COMPLETE

	struct DATAMESSAGE * Buffer;
	UCHAR * ptr1;
	char Normcall[10];
	TRANSPORTENTRY * Session;
	TRANSPORTENTRY * InSession;

	if (LINK->LINKTYPE == 3)
		return;

	//	UP/DOWN LINK

	if (LINK->CIRCUITPOINTER == 0)	// No crosslink??
		return;

	Buffer = GetBuff();

	if (Buffer == NULL)
		return;

	// SET UP HEADER

	Buffer->PID = 0xf0;
	
	ptr1 = SetupNodeHeader(Buffer);

	Normcall[ConvFromAX25(LINK->LINKCALL, Normcall)] = 0;

	ptr1 += sprintf(ptr1, "Connected to %s\r", Normcall);

	Buffer->LENGTH = ptr1 - (UCHAR *)Buffer;

	Session = LINK->CIRCUITPOINTER;			// GET CIRCUIT TABLE ENTRY
	Session->L4STATE = 5;
	InSession = Session->L4CROSSLINK;		// TO INCOMMONG SESSION

	if (InSession)
	{
		C_Q_ADD(&InSession->L4TX_Q, Buffer);
		PostDataAvailable(InSession);
	}
}


TRANSPORTENTRY * SetupSessionForL2(struct _LINKTABLE * LINK)
{
	TRANSPORTENTRY * NewSess = L4TABLE;
	int Index = 0;
	
	while (Index < MAXCIRCUITS)
	{
		if (NewSess->L4USER[0] == 0)
		{
			// Got One
			
			LINK->CIRCUITPOINTER = NewSess;			// SETUP LINK-CIRCUIT CONNECTION
 
			memcpy(NewSess->L4USER, LINK->LINKCALL, 7);
			memcpy(NewSess->L4MYCALL, MYCALL, 7);	// ALWAYS USE _NODE CALL

			NewSess->CIRCUITINDEX = Index;				//OUR INDEX
			NewSess->CIRCUITID = NEXTID;

			NEXTID++;
			if (NEXTID == 0)
				NEXTID++;								// kEEP nON-ZERO

			NewSess->L4TARGET.LINK = LINK;
			
			NewSess->L4CIRCUITTYPE = L2LINK | UPLINK;

			NewSess->L4STATE = 5;						// SET LINK ACTIVE

			NewSess->SESSPACLEN = LINK->LINKPORT->PORTPACLEN;


			NewSess->SESSIONT1 = L4T1;					// Default
			NewSess->L4WINDOW = (UCHAR)L4DEFAULTWINDOW;

			return NewSess;
		}
		Index++;
		NewSess++;
	}

	return NULL;
}


VOID Digipeat(struct PORTCONTROL * PORT, MESSAGE * Buffer, UCHAR * OurCall, int toPort, int UIOnly)		// Digi it (if enabled)
{
	//	WE MAY HAVE DISABLED DIGIPEAT ALTOGETHER, (DIGIFLAG=0),
	//	OR ALLOW ALLOW ONLY UI FRAMES TO BE DIGIED (DIGIFLAG=-1)

	// toPort and UIOnly are used for Cross Port digi feature

	int n;

	if (PORT->DIGIFLAG == 0 && toPort == 0)
	{
		ReleaseBuffer(Buffer);
		return;
	}

	OurCall[6] |= 0x80;					// SET HAS BEEN REPEATED

	// SEE IF UI FRAME - scan forward for end of address bit

	n = 8;

	while ((OurCall[6] & 1) == 0)
	{
		OurCall += 7;

		if ((OurCall - &Buffer->CTL) > 56)
		{
			// Run off end before findin end of address
	
			ReleaseBuffer(Buffer);
			return;
		}
	}

	if (toPort)			// Cross port digi
	{
		if  (((OurCall[7] & ~PFBIT) == 3) || UIOnly == 0)
		{
			// UI or Digi all

			Buffer->PORT = toPort;	// update port no in header
			PORT = GetPortTableEntryFromPortNum(toPort);
	
			if (PORT == NULL)
				ReleaseBuffer(Buffer);
			else
				PUT_ON_PORT_Q(PORT, Buffer);
			return;
		}
		else
		{
			ReleaseBuffer(Buffer);
			return;
		}
	}

	if ((OurCall[7] & ~PFBIT) == 3)
	{
		// UI

		//	UI FRAME. IF DIGIMASK IS NON-ZERO, SEND TO ALL PORTS SET, OTHERWISE SEND TO DIGIPORT

		PORT->L2DIGIED++;

		if (toPort)
		{
			// Cross port digi

			PORT = GetPortTableEntryFromPortNum(toPort);
			Buffer->PORT = PORT->DIGIPORT;	// update port no in header

			if (PORT == NULL)
				ReleaseBuffer(Buffer);
			else
				PUT_ON_PORT_Q(PORT, Buffer);

			return;
		}
	
		if (PORT->DIGIMASK == 0)
		{
			if (PORT->DIGIPORT)					// Cross Band Digi?
			{
				Buffer->PORT = PORT->DIGIPORT;	// update port no in header

				PORT = GetPortTableEntryFromPortNum(PORT->DIGIPORT);

				if (PORT == NULL)
				{
					ReleaseBuffer(Buffer);
					return;
				}
			}
			PUT_ON_PORT_Q(PORT, Buffer);
		}
		else
		{
			DigiToMultiplePorts(PORT, Buffer);
			ReleaseBuffer(Buffer);
		}
		return;
	}

	//  Not UI - Only Digi if Digiflag  not -1

	if (PORT->DIGIFLAG == -1)
	{
		ReleaseBuffer(Buffer);
		return;
	}
	
	PORT->L2DIGIED++;
	
	if (PORT->DIGIPORT)					// Cross Band Digi?
	{
		Buffer->PORT = PORT->DIGIPORT;	// update port no in header

		PORT = GetPortTableEntryFromPortNum(PORT->DIGIPORT);

		if (PORT == NULL)
		{
			ReleaseBuffer(Buffer);
			return;
		}
	}
	PUT_ON_PORT_Q(PORT, Buffer);
}

BOOL CheckForListeningSession(struct PORTCONTROL * PORT, MESSAGE * Msg)
{
	TRANSPORTENTRY * L4 = L4TABLE;
	struct DATAMESSAGE * Buffer;
	int i = MAXCIRCUITS;
	UCHAR * ptr;

	while (i--)
	{
		if ((Msg->PORT & 0x7f) == L4->LISTEN)
		{
			// See if he is calling our call

			UCHAR ourcall[7];				// Call we are using (may have SSID bits inverted
			memcpy(ourcall, L4->L4USER, 7);
			ourcall[6] ^= 0x1e;				// Flip SSID

			if (CompareCalls(Msg->DEST, ourcall))
			{
				// Get Session Entry for Downlink
					
				TRANSPORTENTRY * NewSess = L4TABLE;
				struct _LINKTABLE * LINK;
				char Normcall[10];
				
				int Index = 0;
	
				while (Index < MAXCIRCUITS)
				{
					if (NewSess->L4USER[0] == 0)
					{
						// Got One

						L4->L4CROSSLINK = NewSess;
						NewSess->L4CROSSLINK = L4;

						memcpy(NewSess->L4USER, L4->L4USER, 7);
						memcpy(NewSess->L4MYCALL, L4->L4USER, 7);
	
						NewSess->CIRCUITINDEX = Index;				//OUR INDEX
						NewSess->CIRCUITID = NEXTID;
						NewSess->L4STATE = 5;
						NewSess->L4CIRCUITTYPE = L2LINK+UPLINK;

						NEXTID++;
						if (NEXTID == 0)
							NEXTID++;								// kEEP nON-ZERO

						NewSess->SESSIONT1 = L4->SESSIONT1;
						NewSess->L4WINDOW = (UCHAR)L4DEFAULTWINDOW;
	
						//	SET UP NEW SESSION (OR RESET EXISTING ONE)

						FindLink(Msg->ORIGIN, ourcall, L4->LISTEN, &LINK);
	
						if (LINK == NULL)
							return FALSE;

						memcpy(LINK->LINKCALL, Msg->ORIGIN, 7);
						memcpy(LINK->OURCALL, ourcall, 7);

						LINK->LINKPORT = PORT;

						LINK->L2TIME = PORT->PORTT1;
/*	
						// Copy Digis

						n = 7;
						ptr = &LINK->DIGIS[0];

						while (axcalls[n])
						{
							memcpy(ptr, &axcalls[n], 7);
							n += 7;
							ptr += 7;

							LINK->L2TIME += 2 * PORT->PORTT1;	// ADJUST TIMER VALUE FOR 1 DIGI
						}
*/
						LINK->LINKTYPE = 2;						// DOWNLINK
						LINK->LINKWINDOW = PORT->PORTWINDOW;

						RESET2(LINK);						// RESET ALL FLAGS

						LINK->L2STATE = 5;					// CONNECTED

						LINK->CIRCUITPOINTER = NewSess;

						NewSess->L4TARGET.LINK = LINK;

						if (PORT->PORTPACLEN)
							NewSess->SESSPACLEN = L4->SESSPACLEN = PORT->PORTPACLEN;

						L2SENDUA(PORT, Msg, Msg);		// RESET OF DOWN/CROSSLINK

						L4->LISTEN = FALSE;		// Take out of listen mode

						// Tell User

						Buffer = GetBuff();

						if (Buffer == NULL)
							return TRUE;

						// SET UP HEADER

						Buffer->PID = 0xf0;
	
						ptr = &Buffer->L2DATA[0];

						Normcall[ConvFromAX25(LINK->LINKCALL, Normcall)] = 0;

						ptr += sprintf(ptr, "Incoming call from %s\r", Normcall);

						Buffer->LENGTH = ptr - (UCHAR *)Buffer;

						C_Q_ADD(&L4->L4TX_Q, Buffer);
						PostDataAvailable(L4);
		
						return TRUE;

					}
					Index++;
					NewSess++;
				}
				return FALSE;
			}
		}
		L4++;
	}

	return FALSE;
}