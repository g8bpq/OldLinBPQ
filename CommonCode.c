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



// General C Routines common to bpq32 and linbpq. Mainly moved from BPQ32.c

#pragma data_seg("_BPQDATA")

#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma data_seg("_BPQDATA")

#include "CHeaders.h"
#include "tncinfo.h"

#define LIBCONFIG_STATIC
#include "libconfig.h"

#ifndef LINBPQ

#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.

#include "commctrl.h"
#include "Commdlg.h"

#endif

#define MAXDATA BUFFLEN-16

extern struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

extern int ReportTimer;

Dll VOID APIENTRY Send_AX(UCHAR * Block, DWORD Len, UCHAR Port);
TRANSPORTENTRY * SetupSessionFromHost(PBPQVECSTRUC HOST, UINT ApplMask);
int Check_Timer();
VOID SENDUIMESSAGE(struct DATAMESSAGE * Msg);
DllExport struct PORTCONTROL * APIENTRY GetPortTableEntryFromSlot(int portslot);
VOID APIENTRY md5 (char *arg, unsigned char * checksum);
VOID COMSetDTR(HANDLE fd);
VOID COMClearDTR(HANDLE fd);
VOID COMSetRTS(HANDLE fd);
VOID COMClearRTS(HANDLE fd);

VOID WriteMiniDump();
void printStack(void);

//	Read/Write lenght field in a buffer header

//	Needed for Big/LittleEndian and ARM5 (unaligned operation problem) portability


VOID PutLengthinBuffer(UCHAR * buff, int datalen)		// Neded for arm5 portability
{
#ifdef __BIG_ENDIAN__
	short * sp;					// MAC POWERPC etc
	sp = (short *)&buff[5];
	*sp = datalen;
#else
	buff[5]=(datalen & 0xff);	// 
	buff[6]=(datalen >> 8);
#endif
}

int GetLengthfromBuffer(UCHAR * buff)				// Neded for arm5 portability
{
#ifdef __BIG_ENDIAN__
//	short * sp;					// MAC POWERPC etc
//	return sp = (short *)&buff[5];
	return (buff[5]<<8) + buff[6];	
#else
	return (buff[6]<<8) + buff[5];	
#endif
}


BOOL CheckQHeadder(UINT * Q)
{
#ifdef WIN32
	UINT Test;

	__try
	{
		Test = *Q;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		Debugprintf("Invalid Q Header %X", Q);
		printStack();
		return FALSE;
	}
#endif
	return TRUE;
}

// Get buffer from Queue


VOID * _Q_REM(VOID *PQ, char * File, int Line)
{
	UINT * Q;
	UINT * first;
	UINT next;

	//	PQ may not be word aligned, so copy as bytes (for ARM5)

	Q = (UINT *) PQ;

	if (Semaphore.Flag == 0)
		Debugprintf("Q_REM called without semaphore from %s Line %d", File, Line);

	if (CheckQHeadder(Q) == 0)
		return(0);

	first = (UINT *)Q[0];

	if (first == 0) return (0);			// Empty

	next= first[0];						// Address of next buffer

	Q[0] = next;

	// Make sure guard zone is zeros

	if (*(first + BUFFLEN/4) != 0)
	{
		Debugprintf("Q_REM %X GUARD ZONE CORRUPT %x Called from %s Line %d", first, *(first + BUFFLEN/4), File, Line);
		printStack();
	}

	return (first);
}

// Return Buffer to Free Queue

extern VOID * BUFFERPOOL;
extern UINT * Bufferlist[1000];
void printStack(void);


UINT _ReleaseBuffer(VOID *pBUFF, char * File, int Line)
{
	UINT * pointer, * BUFF = pBUFF;
	int n = 0;

	if (Semaphore.Flag == 0)
		Debugprintf("ReleaseBuffer called without semaphore from %s Line %d", File, Line);

	// Make sure address is within pool

	while (n <= NUMBEROFBUFFERS)
	{
		if (BUFF == Bufferlist[n++])
			goto BOK1;
	}

	Debugprintf("ReleaseBuffer %X not in Pool called from %s Line %d", BUFF, File, Line);
	printStack();

	return 0;


BOK1:

	// See if already on free Queue

	pointer = (UINT *)FREE_Q;

	while (pointer)
	{
		if (pointer == BUFF)
		{
			Debugprintf("Trying to free buffer when already on FREE_Q");
//			WriteMiniDump();

			return 0;
		}
		pointer = (UINT *)pointer[0];
	}

	pointer = (UINT *)FREE_Q;

	*BUFF=(UINT)pointer;

	FREE_Q=(UINT)BUFF;

	QCOUNT++;

	return 0;
}

int _C_Q_ADD(VOID *PQ, VOID *PBUFF, char * File, int Line)
{
	UINT * Q;
	UINT * BUFF = (UINT *)PBUFF;
	UINT * next;
	int n = 0;

//	PQ may not be word aligned, so copy as bytes (for ARM5)

	Q = (UINT *) PQ;

	if (Semaphore.Flag == 0)
		Debugprintf("C_Q_ADD called without semaphore from %s Line %d", File, Line);

	if (CheckQHeadder(Q) == 0)			// Make sure Q header is readable
		return(0);

	// Make sure guard zone is zeros

	if (*(BUFF + BUFFLEN/4) != 0)
	{
		Debugprintf("C_Q_ADD %X GUARD ZONE CORRUPT %x Called from %s Line %d", BUFF, *(BUFF + BUFFLEN/4), File, Line);
		printStack();

		return 0;
	}

	// Make sure address is within pool

	while (n <= NUMBEROFBUFFERS)
	{
		if (BUFF == Bufferlist[n++])
			goto BOK2;
	}

	Debugprintf("C_Q_ADD %X not in Pool called from %s Line %d", BUFF, File, Line);
	printStack();

	return 0;

BOK2:

	BUFF[0]=0;							// Clear chain in new buffer

	if (Q[0] == 0)						// Empty
	{
		Q[0]=(UINT)BUFF;				// New one on front
		return(0);
	}

	next = (UINT *)Q[0];

	while (next[0]!=0)
	{
		next=(UINT *)next[0];			// Chain to end of queue
	}
	next[0]=(UINT)BUFF;					// New one on end

	return(0);
}

// Non-pool version

int C_Q_ADD_NP(VOID *PQ, VOID *PBUFF)
{
	UINT * Q;
	UINT * BUFF = (UINT *)PBUFF;
	UINT * next;
	int n = 0;

//	PQ may not be word aligned, so copy as bytes (for ARM5)

	Q = (UINT *) PQ;

	if (CheckQHeadder(Q) == 0)			// Make sure Q header is readable
		return(0);

	BUFF[0]=0;							// Clear chain in new buffer

	if (Q[0] == 0)						// Empty
	{
//		Q[0]=(UINT)BUFF;				// New one on front
		memcpy(PQ, &BUFF, 4);
		return 0;
	}
	next = (UINT *)Q[0];

	while (next[0]!=0)
		next=(UINT *)next[0];			// Chain to end of queue

	next[0]=(UINT)BUFF;					// New one on end

	return(0);
}


int C_Q_COUNT(VOID *PQ)
{
	UINT * Q;
	int count = 0;

//	PQ may not be word aligned, so copy as bytes (for ARM5)

	Q = (UINT *) PQ;

	if (CheckQHeadder(Q) == 0)			// Make sure Q header is readable
		return(0);

	//	SEE HOW MANY BUFFERS ATTACHED TO Q HEADER

	while (*Q)
	{
		count++;
		if ((count + QCOUNT) > MAXBUFFS)
		{
			Debugprintf("C_Q_COUNT Detected corrupt Q %p len %d", PQ, count);
			return count;
		}
		Q = (UINT *)*Q;
	}

	return count;
}

VOID * _GetBuff(char * File, int Line)
{
	UINT * Temp = Q_REM(&FREE_Q);
	MESSAGE * Msg;

//	FindLostBuffers();

	if (Semaphore.Flag == 0)
		Debugprintf("GetBuff called without semaphore from %s Line %d", File, Line);

	if (Temp)
	{
		QCOUNT--;

		if (QCOUNT < MINBUFFCOUNT)
			MINBUFFCOUNT = QCOUNT;

		Msg = (MESSAGE *)Temp;
		Msg->Process = (short)GetCurrentProcessId();
	}
	else
		Debugprintf("Warning - Getbuff returned NULL");

	return Temp;
}

void * zalloc(int len)
{
	// malloc and clear

	void * ptr;

	ptr=malloc(len);

	if (ptr)
		memset(ptr, 0, len);

	return ptr;
}

char * strlop(char * buf, char delim)
{
	// Terminate buf at delim, and return rest of string

	char * ptr = strchr(buf, delim);

	if (ptr == NULL) return NULL;

	*(ptr)++=0;

	return ptr;
}

VOID DISPLAYCIRCUIT(TRANSPORTENTRY * L4, char * Buffer)
{
	UCHAR Type = L4->L4CIRCUITTYPE;
	struct PORTCONTROL * PORT;
	struct _LINKTABLE * LINK;
	BPQVECSTRUC * VEC;
	struct DEST_LIST * DEST;

	char Normcall[20] = "";			// Could be alias:call
	char Normcall2[11] = "";
	char Alias[11] = "";

	Buffer[0] = 0;

	switch (Type)
	{
	case PACTOR+UPLINK:

		PORT = L4->L4TARGET.PORT;

		ConvFromAX25(L4->L4USER, Normcall);
		strlop(Normcall, ' ');

		if (PORT)
			sprintf(Buffer, "%s %d/%d(%s)", "TNC Uplink Port", PORT->PORTNUMBER, L4->KAMSESSION, Normcall);

		return;


	case PACTOR+DOWNLINK:

		PORT = L4->L4TARGET.PORT;

		if (PORT)
			sprintf(Buffer, "%s %d/%d", "Attached to Port", PORT->PORTNUMBER, L4->KAMSESSION);
		return;


	case L2LINK+UPLINK:

		LINK = L4->L4TARGET.LINK;

		ConvFromAX25(L4->L4USER, Normcall);
		strlop(Normcall, ' ');

		if (LINK)
			sprintf(Buffer, "%s %d(%s)", "Uplink", LINK->LINKPORT->PORTNUMBER, Normcall);

		return;

	case L2LINK+DOWNLINK:

		LINK = L4->L4TARGET.LINK;

		if (LINK == NULL)
			return;

		ConvFromAX25(LINK->OURCALL, Normcall);
		strlop(Normcall, ' ');

		ConvFromAX25(LINK->LINKCALL, Normcall2);
		strlop(Normcall2, ' ');

		sprintf(Buffer, "%s %d(%s %s)", "Downlink", LINK->LINKPORT->PORTNUMBER, Normcall, Normcall2);
		return;

	case BPQHOST + UPLINK:
	case BPQHOST + DOWNLINK:

		// if the call has a Level 4 address display ALIAS:CALL, else just Call

		if (FindDestination(L4->L4USER, &DEST))
			Normcall[DecodeNodeName(DEST->DEST_CALL, Normcall)] = 0;		// null terminate
		else
			Normcall[ConvFromAX25(L4->L4USER, Normcall)] = 0;

		VEC = L4->L4TARGET.HOST;
		sprintf(Buffer, "%s%02d(%s)", "Host", (VEC - BPQHOSTVECTOR) + 1, Normcall);
		return;

	case SESSION + DOWNLINK:
	case SESSION + UPLINK:

		ConvFromAX25(L4->L4USER, Normcall);
		strlop(Normcall, ' ');

		DEST = L4->L4TARGET.DEST;

		if (DEST == NULL)
			return;

		ConvFromAX25(DEST->DEST_CALL, Normcall2);
		strlop(Normcall2, ' ');

		memcpy(Alias, DEST->DEST_ALIAS, 6);
		strlop(Alias, ' ');

		sprintf(Buffer, "Circuit(%s:%s %s)", Alias, Normcall2, Normcall);

		return;
	}
}

VOID CheckForDetach(struct TNCINFO * TNC, int Stream, struct STREAMINFO * STREAM,
			VOID TidyCloseProc(), VOID ForcedCloseProc(), VOID CloseComplete())
{
	UINT * buffptr;

	if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] == 0)
	{
		// Node has disconnected - clear any connection

 		if (STREAM->Disconnecting)
		{
			// Already detected the detach, and have started to close

			STREAM->DisconnectingTimeout--;

			if (STREAM->DisconnectingTimeout)
				return;							// Give it a bit longer

			// Close has timed out - force a disc, and clear

			ForcedCloseProc(TNC, Stream);		// Send Tidy Disconnect

			goto NotConnected;
		}

		// New Disconnect

		Debugprintf("New Disconnect Port %d Q %x", TNC->Port, STREAM->BPQtoPACTOR_Q);

		if (STREAM->Connected || STREAM->Connecting)
		{
			char logmsg[120];
			time_t Duration;

			// Need to do a tidy close

			STREAM->Disconnecting = TRUE;
			STREAM->DisconnectingTimeout = 300;			// 30 Secs

			if (Stream == 0)
				SetWindowText(TNC->xIDC_TNCSTATE, "Disconnecting");

			// Create a traffic record

			if (STREAM->Connected)
			{
				Duration = time(NULL) - STREAM->ConnectTime;

				if (Duration == 0)
					Duration = 1;				// Or will get divide by zero error 

				sprintf(logmsg,"Port %2d %9s Bytes Sent %d  BPS %d Bytes Received %d BPS %d Time %d Seconds",
					TNC->Port, STREAM->RemoteCall,
					STREAM->BytesTXed, (int)(STREAM->BytesTXed/Duration),
					STREAM->BytesRXed, (int)(STREAM->BytesRXed/Duration), (int)Duration);

				Debugprintf(logmsg);
			}

			if (STREAM->BPQtoPACTOR_Q)					// Still data to send?
				return;									// Will close when all acked

//			if (STREAM->FramesOutstanding && TNC->Hardware == H_UZ7HO)
//				return;									// Will close when all acked

			TidyCloseProc(TNC, Stream);					// Send Tidy Disconnect

			return;
		}

		// Not connected
NotConnected:

		STREAM->Disconnecting = FALSE;
		STREAM->Attached = FALSE;
		STREAM->Connecting = FALSE;
		STREAM->Connected = FALSE;

		if (Stream == 0)
			SetWindowText(TNC->xIDC_TNCSTATE, "Free");

		STREAM->FramesQueued = 0;
		STREAM->FramesOutstanding = 0;

		CloseComplete(TNC, Stream);

		while(STREAM->BPQtoPACTOR_Q)
		{
			buffptr=Q_REM(&STREAM->BPQtoPACTOR_Q);
			ReleaseBuffer(buffptr);
		}

		while(STREAM->PACTORtoBPQ_Q)
		{
			buffptr=Q_REM(&STREAM->PACTORtoBPQ_Q);
			ReleaseBuffer(buffptr);
		}
	}
}

char * CheckAppl(struct TNCINFO * TNC, char * Appl)
{
	APPLCALLS * APPL;
	BPQVECSTRUC * PORTVEC;
	int Allocated = 0, Available = 0;
	int App, Stream;
	struct TNCINFO * APPLTNC;

//	Debugprintf("Checking if %s is running", Appl);

	for (App = 0; App < 32; App++)
	{
		APPL=&APPLCALLTABLE[App];

		if (_memicmp(APPL->APPLCMD, Appl, 12) == 0)
		{
			int _APPLMASK = 1 << App;

			// If App has an alias, assume it is running , unless a CMS alias - then check CMS

			if (APPL->APPLHASALIAS)
			{
				if (_memicmp(APPL->APPLCMD, "RELAY ", 6) == 0)
					return APPL->APPLCALL_TEXT;			// Assume people using RELAY know what they are doing

				if (APPL->APPLPORT && (_memicmp(APPL->APPLCMD, "RMS ", 4) == 0))
				{
					APPLTNC = TNCInfo[APPL->APPLPORT];
					{
						if (APPLTNC)
						{
							if (APPLTNC->TCPInfo && !APPLTNC->TCPInfo->CMSOK && !APPLTNC->TCPInfo->FallbacktoRelay)
							return NULL;
						}
					}
				}
				return APPL->APPLCALL_TEXT;
			}

			// See if App is running

			PORTVEC = &BPQHOSTVECTOR[0];

			for (Stream = 0; Stream < 64; Stream++)
			{
				if (PORTVEC->HOSTAPPLMASK & _APPLMASK)
				{
					Allocated++;

					if (PORTVEC->HOSTSESSION == 0 && (PORTVEC->HOSTFLAGS & 3) == 0)
					{
						// Free and no outstanding report

						return APPL->APPLCALL_TEXT;		// Running
					}
				}
				PORTVEC++;
			}
		}
	}

	return NULL;			// Not Running
}

VOID SetApplPorts()
{
	// If any appl has an alias, get port number

	struct APPLCONFIG * App;
	APPLCALLS * APPL;

	char C[80];
	char Port[80];
	char Call[80];

	int i, n;

	App = (struct APPLCONFIG *)&ConfigBuffer[ApplOffset];

	for (i=0; i < NumberofAppls; i++)
	{
		APPL=&APPLCALLTABLE[i];

		if (APPL->APPLHASALIAS)
		{
			n = sscanf(App->CommandAlias, "%s %s %s", &C[0], &Port[0], &Call[0]);
			if (n == 3)
				APPL->APPLPORT = atoi(Port);
		}
		App++;
	}
}


struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

BOOL ProcessIncommingConnect(struct TNCINFO * TNC, char * Call, int Stream, BOOL SENDCTEXT)
{
	return ProcessIncommingConnectEx(TNC, Call, Stream, SENDCTEXT, FALSE);
}

BOOL ProcessIncommingConnectEx(struct TNCINFO * TNC, char * Call, int Stream, BOOL SENDCTEXT, BOOL AllowTR)
{
	TRANSPORTENTRY * Session;
	int Index = 0;
	UINT * buffptr;

	// Stop Scanner

	if (Stream == 0 || TNC->Hardware == H_UZ7HO)
	{
		char Msg[80];

		sprintf(Msg, "%d SCANSTOP", TNC->Port);

		Rig_Command(-1, Msg);
		UpdateMH(TNC, Call, '+', 'I');
	}

	Session=L4TABLE;

	// Find a free Circuit Entry

	while (Index < MAXCIRCUITS)
	{
		if (Session->L4USER[0] == 0)
			break;

		Session++;
		Index++;
	}

	if (Index == MAXCIRCUITS)
		return FALSE;					// Tables Full

	memset(Session, 0, sizeof(TRANSPORTENTRY));

	memcpy(TNC->Streams[Stream].RemoteCall, Call, 9);	// Save Text Callsign

	if (AllowTR)
		ConvToAX25Ex(Call, Session->L4USER);				// Allow -T and -R SSID's for MPS
	else
		ConvToAX25(Call, Session->L4USER);
	ConvToAX25(MYNODECALL, Session->L4MYCALL);
	Session->CIRCUITINDEX = Index;
	Session->CIRCUITID = NEXTID;
	NEXTID++;
	if (NEXTID == 0) NEXTID++;		// Keep non-zero

	TNC->PortRecord->ATTACHEDSESSIONS[Stream] = Session;
	TNC->Streams[Stream].Attached = TRUE;

	Session->L4TARGET.EXTPORT = TNC->PortRecord;

	Session->L4CIRCUITTYPE = UPLINK+PACTOR;
	Session->L4WINDOW = L4DEFAULTWINDOW;
	Session->L4STATE = 5;
	Session->SESSIONT1 = L4T1;
	Session->SESSPACLEN = TNC->PortRecord->PORTCONTROL.PORTPACLEN;
	Session->KAMSESSION = Stream;

	TNC->Streams[Stream].Connected = TRUE;			// Subsequent data to data channel

	if (HFCTEXTLEN > 1 && SENDCTEXT)
	{
		buffptr = GetBuff();
		if (buffptr == 0) return TRUE;			// No buffers

		buffptr[1] = HFCTEXTLEN;
		memcpy(&buffptr[2], HFCTEXT, HFCTEXTLEN);
		C_Q_ADD(&TNC->Streams[Stream].BPQtoPACTOR_Q, buffptr);
	}
	return TRUE;
}

char * Config;
static char * ptr1, * ptr2;

BOOL ReadConfigFile(int Port, int ProcLine())
{
	char buf[256],errbuf[256];

	if (TNCInfo[Port])					// If restarting, free old config
		free(TNCInfo[Port]);

	TNCInfo[Port] = NULL;

	Config = PortConfig[Port];

	if (Config)
	{
		// Using config from bpq32.cfg

		if (strlen(Config) == 0)
		{
			// Empty Config File - OK for most types

			struct TNCINFO * TNC = TNCInfo[Port] = zalloc(sizeof(struct TNCINFO));

			TNC->InitScript = malloc(2);
			TNC->InitScript[0] = 0;

			return TRUE;
		}

		ptr1 = Config;

		ptr2 = strchr(ptr1, 13);
		while(ptr2)
		{
			memcpy(buf, ptr1, ptr2 - ptr1 + 1);
			buf[ptr2 - ptr1 + 1] = 0;
			ptr1 = ptr2 + 2;
			ptr2 = strchr(ptr1, 13);

			strcpy(errbuf,buf);			// save in case of error

			if (!ProcLine(buf, Port))
			{
				WritetoConsoleLocal("\n");
				WritetoConsoleLocal("Bad config record ");
				WritetoConsoleLocal(errbuf);
			}
		}
	}
	else
	{
		sprintf(buf," ** Error - No Configuration info in bpq32.cfg");
		WritetoConsoleLocal(buf);
	}

	return (TRUE);
}
int GetLine(char * buf)
{
loop:

	if (ptr2 == NULL)
		return 0;

	memcpy(buf, ptr1, ptr2 - ptr1 + 2);
	buf[ptr2 - ptr1 + 2] = 0;
	ptr1 = ptr2 + 2;
	ptr2 = strchr(ptr1, 13);

	if (buf[0] < 0x20) goto loop;
	if (buf[0] == '#') goto loop;
	if (buf[0] == ';') goto loop;

	if (buf[strlen(buf)-1] < 0x20) buf[strlen(buf)-1] = 0;
	if (buf[strlen(buf)-1] < 0x20) buf[strlen(buf)-1] = 0;
	buf[strlen(buf)] = 13;

	return 1;
}
VOID DigiToMultiplePorts(struct PORTCONTROL * PORTVEC, PMESSAGE Msg)
{
	USHORT Mask=PORTVEC->DIGIMASK;
	int i;

	for (i=1; i<=NUMBEROFPORTS; i++)
	{
		if (Mask & 1)
		{
			// Block includes the Msg Header (7 bytes), Len Does not!

			Msg->PORT = i;
			Send_AX((UCHAR *)&Msg, Msg->LENGTH - 7, i);
			Mask>>=1;
		}
	}
}

int CompareAlias( void *a, void *b)
{
	struct DEST_LIST * x;
	struct DEST_LIST * y;
	UINT * c;

	c = a;
	c = (UINT *)*c;
	x = (struct DEST_LIST *)c;

	c = b;
	c = (UINT *)*c;
	y = (struct DEST_LIST *)c;

	return memcmp(x->DEST_ALIAS, y->DEST_ALIAS, 6);
	/* strcmp functions works exactly as expected from
	comparison function */
}


int CompareNode(void *a, void *b)
{
	struct DEST_LIST * x;
	struct DEST_LIST * y;
	UINT * c;

	c = a;
	c = (UINT *)*c;
	x = (struct DEST_LIST *)c;

	c = b;
	c = (UINT *)*c;
	y = (struct DEST_LIST *)c;

	return memcmp(x->DEST_CALL, y->DEST_CALL, 7);
	/* strcmp functions works exactly as expected from
	comparison function */
}

DllExport int APIENTRY CountFramesQueuedOnStream(int Stream)
{
	BPQVECSTRUC * PORTVEC = &BPQHOSTVECTOR[Stream-1];		// API counts from 1
	TRANSPORTENTRY * L4 = PORTVEC->HOSTSESSION;

	int Count = 0;

	if (L4)
	{
		if (L4->L4CROSSLINK)		// CONNECTED?
			Count = CountFramesQueuedOnSession(L4->L4CROSSLINK);
		else
			Count = CountFramesQueuedOnSession(L4);
	}
	return Count;
}

DllExport int APIENTRY ChangeSessionCallsign(int Stream, unsigned char * AXCall)
{
	// Equivalent to "*** linked to" command

	memcpy(BPQHOSTVECTOR[Stream-1].HOSTSESSION->L4USER, AXCall, 7);
	return (0);
}

DllExport int APIENTRY ChangeSessionPaclen(int Stream, int Paclen)
{
	BPQHOSTVECTOR[Stream-1].HOSTSESSION->SESSPACLEN = Paclen;
	return (0);
}

DllExport int APIENTRY ChangeSessionIdletime(int Stream, int idletime)
{
	if (BPQHOSTVECTOR[Stream-1].HOSTSESSION)
		BPQHOSTVECTOR[Stream-1].HOSTSESSION->L4LIMIT = idletime;
	return (0);
}

DllExport int APIENTRY Get_APPLMASK(int Stream)
{
	return	BPQHOSTVECTOR[Stream-1].HOSTAPPLMASK;
}
DllExport int APIENTRY GetStreamPID(int Stream)
{
	return	BPQHOSTVECTOR[Stream-1].STREAMOWNER;
}

DllExport int APIENTRY GetApplFlags(int Stream)
{
	return	BPQHOSTVECTOR[Stream-1].HOSTAPPLFLAGS;
}

DllExport int APIENTRY GetApplNum(int Stream)
{
	return	BPQHOSTVECTOR[Stream-1].HOSTAPPLNUM;
}

DllExport int APIENTRY GetApplMask(int Stream)
{
	return	BPQHOSTVECTOR[Stream-1].HOSTAPPLMASK;
}

DllExport BOOL APIENTRY GetAllocationState(int Stream)
{
	return	BPQHOSTVECTOR[Stream-1].HOSTFLAGS & 0x80;
}

VOID Send_AX_Datagram(PDIGIMESSAGE Block, DWORD Len, UCHAR Port);

extern int InitDone;
extern int SemHeldByAPI;
extern char pgm[256];		// Uninitialised so per process
extern int BPQHOSTAPI();


VOID POSTSTATECHANGE(BPQVECSTRUC * SESS)
{
	//	Post a message if requested
#ifndef LINBPQ
	if (SESS->HOSTHANDLE)
		PostMessage(SESS->HOSTHANDLE, BPQMsg, SESS->HOSTSTREAM, 4);
#endif
	return;
}


DllExport int APIENTRY SessionControl(int stream, int command, int Mask)
{
	BPQVECSTRUC * SESS;
	TRANSPORTENTRY * L4;

	stream--;						// API uses 1 - 64

	if (stream < 0 || stream > 63)
		return (0);

	SESS = &BPQHOSTVECTOR[stream];

	//	Send Session Control command (BPQHOST function 6)
	//;	CL=0 CONNECT USING APPL MASK IN DL
	//;	CL=1, CONNECT. CL=2 - DISCONNECT. CL=3 RETURN TO NODE

	if 	(command > 1)
	{
		// Disconnect

		if (SESS->HOSTSESSION == 0)
		{
			SESS->HOSTFLAGS |= 1;		// State Change
			POSTSTATECHANGE(SESS);
			return 0;					// NOT CONNECTED
		}

		if (command == 3)
			SESS->HOSTFLAGS |= 0x20;	// Set Stay

		SESS->HOSTFLAGS |= 0x40;		// SET 'DISC REQ' FLAG

		return 0;
	}

	// 0 or 1 - connect

	if (SESS->HOSTSESSION)				// ALREADY CONNECTED
	{
		SESS->HOSTFLAGS |= 1;			// State Change
		POSTSTATECHANGE(SESS);
		return 0;
	}

	//	SET UP A SESSION FOR THE CONSOLE

	SESS->HOSTFLAGS |= 0x80;			// SET ALLOCATED BIT

	if (command == 1)					// Zero is mask supplied by caller
		Mask = SESS->HOSTAPPLMASK;		// SO WE GET CORRECT CALLSIGN

	L4 = SetupSessionFromHost(SESS, Mask);

	if (L4 == 0)						// tables Full
	{
		SESS->HOSTFLAGS |= 3;			// State Change
		POSTSTATECHANGE(SESS);
		return 0;
	}

	SESS->HOSTSESSION = L4;
	L4->L4CIRCUITTYPE = BPQHOST | UPLINK;
 	L4->Secure_Session = AuthorisedProgram;	// Secure Host Session

	SESS->HOSTFLAGS |= 1;		// State Change
	POSTSTATECHANGE(SESS);
	return 0;					// ALREADY CONNECTED
}

DllExport int APIENTRY FindFreeStream()
{
	int stream, n;
	BPQVECSTRUC * PORTVEC;

//	Returns number of first unused BPQHOST stream. If none available,
//	returns 255. See API function 13.

	// if init has not yet been run, wait.

	while (InitDone == 0)
	{
		Debugprintf("Waiting for init to complete");
		Sleep(1000);
	}

	if (InitDone == -1)			// Init failed
		exit(0);

	GetSemaphore(&Semaphore, 9);

	stream = 0;
	n = 64;

	while (n--)
	{
		PORTVEC = &BPQHOSTVECTOR[stream++];
		if ((PORTVEC->HOSTFLAGS & 0x80) == 0)
		{
			PORTVEC->STREAMOWNER=GetCurrentProcessId();
			PORTVEC->HOSTFLAGS = 128; // SET ALLOCATED BIT, clear others
			memcpy(&PORTVEC->PgmName[0], pgm, 31);
			FreeSemaphore(&Semaphore);
			return stream;
		}
	}
	FreeSemaphore(&Semaphore);
	return 255;
}

DllExport int APIENTRY AllocateStream(int stream)
{
//	Allocate stream. If stream is already allocated, return nonzero.
//	Otherwise allocate stream, and return zero.

	BPQVECSTRUC * PORTVEC = &BPQHOSTVECTOR[stream -1];		// API counts from 1

	if ((PORTVEC->HOSTFLAGS & 0x80) == 0)
	{
		PORTVEC->STREAMOWNER=GetCurrentProcessId();
		PORTVEC->HOSTFLAGS = 128; // SET ALLOCATED BIT, clear others
		memcpy(&PORTVEC->PgmName[0], pgm, 31);
		FreeSemaphore(&Semaphore);
		return 0;
	}

	return 1;				// Already allocated
}


DllExport int APIENTRY DeallocateStream(int stream)
{
	BPQVECSTRUC * PORTVEC;
	UINT * monbuff;
	BOOL GotSem = Semaphore.Flag;

//	Release stream.

	stream--;

	if (stream < 0 || stream > 63)
		return (0);

	PORTVEC=&BPQHOSTVECTOR[stream];

	PORTVEC->STREAMOWNER=0;
	PORTVEC->PgmName[0] = 0;
	PORTVEC->HOSTAPPLFLAGS=0;
	PORTVEC->HOSTAPPLMASK=0;
	PORTVEC->HOSTHANDLE=0;

	// Clear Trace Queue

	if (PORTVEC->HOSTSESSION)
		SessionControl(stream + 1, 2, 0);

	if (GotSem == 0)
		GetSemaphore(&Semaphore, 0);

	while (PORTVEC->HOSTTRACEQ)
	{
		monbuff = Q_REM(&PORTVEC->HOSTTRACEQ);
		ReleaseBuffer(monbuff);
	}

	if (GotSem == 0)
		FreeSemaphore(&Semaphore);

	PORTVEC->HOSTFLAGS &= 0x60;			// Clear Allocated. Must leave any DISC Pending bits

	return(0);
}
DllExport int APIENTRY SessionState(int stream, int * state, int * change)
{
	//	Get current Session State. Any state changed is ACK'ed
	//	automatically. See BPQHOST functions 4 and 5.

	BPQVECSTRUC * HOST = &BPQHOSTVECTOR[stream -1];		// API counts from 1

	Check_Timer();				// In case Appl doesnt call it often ehough

	GetSemaphore(&Semaphore, 20);

	//	CX = 0 if stream disconnected or CX = 1 if stream connected
	//	DX = 0 if no change of state since last read, or DX = 1 if
	//	       the connected/disconnected state has changed since
	//	       last read (ie. delta-stream status).

	//	HOSTFLAGS = Bit 80 = Allocated
	//		  Bit 40 = Disc Request
	//		  Bit 20 = Stay Flag
	//		  Bit 02 and 01 State Change Bits

	if ((HOST->HOSTFLAGS & 3) == 0)
		// No Chaange
		*change = 0;
	else
		*change = 1;

	if (HOST->HOSTSESSION)			// LOCAL SESSION
		// Connected
		*state = 1;
	else
		*state = 0;

	HOST->HOSTFLAGS &= 0xFC;		// Clear Change Bitd

	FreeSemaphore(&Semaphore);
	return 0;
}

DllExport int APIENTRY SessionStateNoAck(int stream, int * state)
{
	//	Get current Session State. Dont ACK any change
	//	See BPQHOST function 4

	BPQVECSTRUC * HOST = &BPQHOSTVECTOR[stream -1];		// API counts from 1

	Check_Timer();				// In case Appl doesnt call it often ehough

	if (HOST->HOSTSESSION)			// LOCAL SESSION
		// Connected
		*state = 1;
	else
		*state = 0;

	return 0;
}

DllExport int APIENTRY SendMsg(int stream, char * msg, int len)
{
	//	Send message to stream (BPQHOST Function 2)

	BPQVECSTRUC * SESS;
	TRANSPORTENTRY * L4;
	TRANSPORTENTRY * Partner;
	PDATAMESSAGE MSG;

	Check_Timer();

	if (len > 256)
		return 0;						// IGNORE

	if (stream == 0)
	{
		// Send UNPROTO - SEND FRAME TO ALL RADIO PORTS

		//	COPY DATA TO A BUFFER IN OUR SEGMENTS - SIMPLFIES THINGS LATER

		if (QCOUNT < 50)
			return 0;					// Dont want to run out

		GetSemaphore(&Semaphore, 10);

		if ((MSG = GetBuff()) == 0)
		{
			FreeSemaphore(&Semaphore);
			return 0;
		}

		MSG->PID = 0xF0;				// Normal Data PID

		memcpy(&MSG->L2DATA[0], msg, len);
		MSG->LENGTH = len + MSGHDDRLEN + 1;

		SENDUIMESSAGE(MSG);
		ReleaseBuffer(MSG);
		FreeSemaphore(&Semaphore);
		return 0;
	}

	stream--;						// API uses 1 - 64

	if (stream < 0 || stream > 63)
		return 0;

	SESS = &BPQHOSTVECTOR[stream];
	L4 = SESS->HOSTSESSION;

	if (L4 == 0)
		return 0;

	GetSemaphore(&Semaphore, 22);

	SESS->HOSTFLAGS |= 0x80;		// SET ALLOCATED BIT

	if (QCOUNT < 40)				// PLENTY FREE?
	{
		FreeSemaphore(&Semaphore);
		return 1;
	}

	// Dont allow massive queues to form

	if (QCOUNT < 100)
	{
		int n = CountFramesQueuedOnStream(stream + 1);

		if (n > 100)
		{
			Debugprintf("Stream %d QCOUNT %d Q Len %d - discarding", stream, QCOUNT, n);
			FreeSemaphore(&Semaphore);
			return 1;
		}
	}

	if ((MSG = GetBuff()) == 0)
	{
		FreeSemaphore(&Semaphore);
		return 1;
	}

	MSG->PID = 0xF0;				// Normal Data PID

	memcpy(&MSG->L2DATA[0], msg, len);
	MSG->LENGTH = len + MSGHDDRLEN + 1;

	//	IF CONNECTED, PASS MESSAGE TO TARGET CIRCUIT - FLOW CONTROL AND
	//	DELAYED DISC ONLY WORK ON ONE SIDE

	Partner = L4->L4CROSSLINK;

	L4->L4KILLTIMER = 0;		// RESET SESSION TIMEOUT

	if (Partner && Partner->L4STATE > 4)	// Partner and link up
	{
		//	Connected

		Partner->L4KILLTIMER = 0;		// RESET SESSION TIMEOUT
		C_Q_ADD(&Partner->L4TX_Q, MSG);
		PostDataAvailable(Partner);
	}
	else
		C_Q_ADD(&L4->L4RX_Q, MSG);

	FreeSemaphore(&Semaphore);
	return 0;
}
DllExport int APIENTRY SendRaw(int port, char * msg, int len)
{
	struct PORTCONTROL * PORT;
	MESSAGE * MSG;

	Check_Timer();

	//	Send Raw (KISS mode) frame to port (BPQHOST function 10)

	if (len > (MAXDATA - (MSGHDDRLEN + 8)))
		return 0;

	if (QCOUNT < 50)
		return 1;

	//	GET A BUFFER

	PORT = GetPortTableEntryFromSlot(port);

	if (PORT == 0)
		return 0;

	GetSemaphore(&Semaphore, 24);

	MSG = GetBuff();

	if (MSG == 0)
	{
		FreeSemaphore(&Semaphore);
		return 1;
	}

	memcpy(MSG->DEST, msg, len);

	MSG->LENGTH = len + MSGHDDRLEN;

	if (PORT->PROTOCOL == 10)		 // PACTOR/WINMOR Style
	{
		//	Pactor Style. Probably will only be used for Tracker uneless we do APRS over V4 or WINMOR

		EXTPORTDATA * EXTPORT = (EXTPORTDATA *) PORT;

		C_Q_ADD(&EXTPORT->UI_Q,	MSG);

		FreeSemaphore(&Semaphore);
		return 0;
	}

	MSG->PORT = PORT->PORTNUMBER;

	PUT_ON_PORT_Q(PORT, MSG);

	FreeSemaphore(&Semaphore);
	return 0;
}

DllExport time_t APIENTRY GetRaw(int stream, char * msg, int * len, int * count)
{
	time_t Stamp;
	BPQVECSTRUC * SESS;
	PMESSAGE MSG;
	int Msglen;

	Check_Timer();

	*len = 0;
	*count = 0;

	stream--;						// API uses 1 - 64

	if (stream < 0 || stream > 63)
		return 0;

	SESS = &BPQHOSTVECTOR[stream];

	GetSemaphore(&Semaphore, 26);

	if (SESS->HOSTTRACEQ == 0)
	{
		FreeSemaphore(&Semaphore);
		return 0;
	}

	MSG = Q_REM(&SESS->HOSTTRACEQ);

	Msglen = MSG->LENGTH;

	if (Msglen < 0 || Msglen > 350)
	{
		FreeSemaphore(&Semaphore);
		return 0;
	}

	Stamp = MSG->Timestamp;

	memcpy(msg, MSG, Msglen);

	*len = Msglen;

	ReleaseBuffer(MSG);

	*count = C_Q_COUNT(&SESS->HOSTTRACEQ);
	FreeSemaphore(&Semaphore);

	return Stamp;
}

DllExport int APIENTRY GetMsg(int stream, char * msg, int * len, int * count )
{
//	Get message from stream. Returns length, and count of frames
//	still waiting to be collected. (BPQHOST function 3)
//	AH = 3	Receive frame into buffer at ES:DI, length of frame returned
//		in CX.  BX returns the number of outstanding frames still to
//		be received (ie. after this one) or zero if no more frames
//		(ie. this is last one).
//

	BPQVECSTRUC * SESS;
	TRANSPORTENTRY * L4;
	PDATAMESSAGE MSG;
	int Msglen;

	Check_Timer();

	*len = 0;
	*count = 0;

	stream--;						// API uses 1 - 64

	if (stream < 0 || stream > 63)
		return 0;


	SESS = &BPQHOSTVECTOR[stream];
	L4 = SESS->HOSTSESSION;

	GetSemaphore(&Semaphore, 25);

	if (L4 == 0 || L4->L4TX_Q == 0)
	{
		FreeSemaphore(&Semaphore);
		return 0;
	}

	L4->L4KILLTIMER = 0;		// RESET SESSION TIMEOUT

	if(L4->L4CROSSLINK)
		L4->L4CROSSLINK->L4KILLTIMER = 0;

	MSG = Q_REM(&L4->L4TX_Q);

	Msglen = MSG->LENGTH - (MSGHDDRLEN + 1);	// Dont want PID

	if (Msglen < 0)
	{
		FreeSemaphore(&Semaphore);
		return 0;
	}

	if (Msglen > 256)
		Msglen = 256;

	memcpy(msg, &MSG->L2DATA[0], Msglen);

	*len = Msglen;

	ReleaseBuffer(MSG);

	*count = C_Q_COUNT(&L4->L4TX_Q);
	FreeSemaphore(&Semaphore);

	return 0;
}


DllExport int APIENTRY RXCount(int stream)
{
//	Returns count of packets waiting on stream
//	 (BPQHOST function 7 (part)).

	BPQVECSTRUC * SESS;
	TRANSPORTENTRY * L4;

	Check_Timer();

	stream--;						// API uses 1 - 64

	if (stream < 0 || stream > 63)
		return 0;

	SESS = &BPQHOSTVECTOR[stream];
	L4 = SESS->HOSTSESSION;

	if (L4 == 0)
		return 0;			// NOT CONNECTED

	return C_Q_COUNT(&L4->L4TX_Q);
}

DllExport int APIENTRY TXCount(int stream)
{
//	Returns number of packets on TX queue for stream
//	 (BPQHOST function 7 (part)).

	BPQVECSTRUC * SESS;
	TRANSPORTENTRY * L4;

	Check_Timer();

	stream--;						// API uses 1 - 64

	if (stream < 0 || stream > 63)
		return 0;

	SESS = &BPQHOSTVECTOR[stream];
	L4 = SESS->HOSTSESSION;

	if (L4 == 0)
		return 0;			// NOT CONNECTED

	L4 = L4->L4CROSSLINK;

	if (L4 == 0)
		return 0;			// NOTHING ro Q on

	return (CountFramesQueuedOnSession(L4));
}

DllExport int APIENTRY MONCount(int stream)
{
//	Returns number of monitor frames available
//	 (BPQHOST function 7 (part)).

	BPQVECSTRUC * SESS;

	Check_Timer();

	stream--;						// API uses 1 - 64

	if (stream < 0 || stream > 63)
		return 0;

	SESS = &BPQHOSTVECTOR[stream];

	return C_Q_COUNT(&SESS->HOSTTRACEQ);
}


DllExport int APIENTRY GetCallsign(int stream, char * callsign)
{
	//	Returns call connected on stream (BPQHOST function 8 (part)).

	BPQVECSTRUC * SESS;
	TRANSPORTENTRY * L4;
	TRANSPORTENTRY * Partner;
	UCHAR  Call[11] = "SWITCH    ";
	UCHAR * AXCall = NULL;
	Check_Timer();

	stream--;						// API uses 1 - 64

	if (stream < 0 || stream > 63)
		return 0;

	SESS = &BPQHOSTVECTOR[stream];
	L4 = SESS->HOSTSESSION;

	GetSemaphore(&Semaphore, 26);

	if (L4 == 0)
	{
		FreeSemaphore(&Semaphore);
		return 0;
	}

	Partner = L4->L4CROSSLINK;

	if (Partner)
	{
		//	CONNECTED OUT - GET TARGET SESSION

		if (Partner->L4CIRCUITTYPE & BPQHOST)
		{
			AXCall = &Partner->L4USER[0];
		}
		else if (Partner->L4CIRCUITTYPE & L2LINK)
		{
			struct _LINKTABLE * LINK = Partner->L4TARGET.LINK;

			if (LINK)
				AXCall = LINK->LINKCALL;

			if (Partner->L4CIRCUITTYPE & UPLINK)
			{
				// IF UPLINK, SHOULD USE SESSION CALL, IN CASE *** LINKED HAS BEEN USED

				AXCall = &Partner->L4USER[0];
			}
		}
		else if (Partner->L4CIRCUITTYPE & PACTOR)
		{
			//	PACTOR Type - Frames are queued on the Port Entry

			EXTPORTDATA * EXTPORT = Partner->L4TARGET.EXTPORT;

			if (EXTPORT)
				AXCall = &EXTPORT->ATTACHEDSESSIONS[Partner->KAMSESSION]->L4USER[0];

		}
		else
		{
			//	MUST BE NODE SESSION

			//	ANOTHER NODE

			//	IF THE HOST IS THE UPLINKING STATION, WE NEED THE TARGET CALL

			if (L4->L4CIRCUITTYPE & UPLINK)
			{
				struct DEST_LIST *DEST = Partner->L4TARGET.DEST;

				if (DEST)
					AXCall = &DEST->DEST_CALL[0];
			}
			else
				AXCall = Partner->L4USER;
		}
		if (AXCall)
			ConvFromAX25(AXCall, Call);
	}

	memcpy(callsign, Call, 10);

	FreeSemaphore(&Semaphore);
	return 0;
}

DllExport int APIENTRY GetConnectionInfo(int stream, char * callsign,
										 int * port, int * sesstype, int * paclen,
										 int * maxframe, int * l4window)
{
	// Return the Secure Session Flag rather than not connected

	BPQVECSTRUC * SESS;
	TRANSPORTENTRY * L4;
	TRANSPORTENTRY * Partner;
	UCHAR  Call[11] = "SWITCH    ";
	UCHAR * AXCall;
	Check_Timer();

	stream--;						// API uses 1 - 64

	if (stream < 0 || stream > 63)
		return 0;

	SESS = &BPQHOSTVECTOR[stream];
	L4 = SESS->HOSTSESSION;

	GetSemaphore(&Semaphore, 27);

	if (L4 == 0)
	{
		FreeSemaphore(&Semaphore);
		return 0;
	}

	Partner = L4->L4CROSSLINK;

	// Return the Secure Session Flag rather than not connected

	//		AL = Radio port on which channel is connected (or zero)
	//		AH = SESSION TYPE BITS
	//		EBX = L2 paclen for the radio port
	//		ECX = L2 maxframe for the radio port
	//		EDX = L4 window size (if L4 circuit, or zero) or -1 if not connected
	//		ES:DI = CALLSIGN

	*port = 0;
	*sesstype = 0;
	*paclen = 0;
	*maxframe = 0;
	*l4window = 0;
	if (L4->SESSPACLEN)
		*paclen = L4->SESSPACLEN;
	else
		*paclen = 256;

	if (Partner)
	{
		//	CONNECTED OUT - GET TARGET SESSION

		*l4window = Partner->L4WINDOW;
		*sesstype = Partner->L4CIRCUITTYPE;

		if (Partner->L4CIRCUITTYPE & BPQHOST)
		{
			AXCall = &Partner->L4USER[0];
		}
		else if (Partner->L4CIRCUITTYPE & L2LINK)
		{
			struct _LINKTABLE * LINK = Partner->L4TARGET.LINK;

			//	EXTRACT PORT AND MAXFRAME

			*port = LINK->LINKPORT->PORTNUMBER;
			*maxframe = LINK->LINKWINDOW;
			*l4window = 0;

			AXCall = LINK->LINKCALL;

			if (Partner->L4CIRCUITTYPE & UPLINK)
			{
				// IF UPLINK, SHOULD USE SESSION CALL, IN CASE *** LINKED HAS BEEN USED

				AXCall = &Partner->L4USER[0];
			}
		}
		else if (Partner->L4CIRCUITTYPE & PACTOR)
		{
			//	PACTOR Type - Frames are queued on the Port Entry

			EXTPORTDATA * EXTPORT = Partner->L4TARGET.EXTPORT;

			*port = EXTPORT->PORTCONTROL.PORTNUMBER;
			AXCall = &EXTPORT->ATTACHEDSESSIONS[Partner->KAMSESSION]->L4USER[0];

		}
		else
		{
			//	MUST BE NODE SESSION

			//	ANOTHER NODE

			//	IF THE HOST IS THE UPLINKING STATION, WE NEED THE TARGET CALL

			if (L4->L4CIRCUITTYPE & UPLINK)
			{
				struct DEST_LIST *DEST = Partner->L4TARGET.DEST;

				AXCall = &DEST->DEST_CALL[0];
			}
			else
				AXCall = Partner->L4USER;
		}
		ConvFromAX25(AXCall, Call);
	}

	memcpy(callsign, Call, 10);

	FreeSemaphore(&Semaphore);

	if (Partner)
		return Partner->Secure_Session;

	return 0;
}


DllExport int APIENTRY SetAppl(int stream, int flags, int mask)
{
//	Sets Application Flags and Mask for stream. (BPQHOST function 1)
//	AH = 1	Set application mask to value in EDX (or even DX if 16
//		applications are ever to be supported).
//
//		Set application flag(s) to value in CL (or CX).
//		whether user gets connected/disconnected messages issued
//		by the node etc.


	BPQVECSTRUC * PORTVEC;
	stream--;

	if (stream < 0 || stream > 63)
		return (0);

	PORTVEC=&BPQHOSTVECTOR[stream];

	PORTVEC->HOSTAPPLFLAGS = flags;
	PORTVEC->HOSTAPPLMASK = mask;

	// If either is non-zero, set allocated and Process. This gets round problem with
	// stations that don't call allocate stream

	if (flags || mask)
	{
		if ((PORTVEC->HOSTFLAGS & 128) == 0)	// Not allocated
		{
			PORTVEC->STREAMOWNER=GetCurrentProcessId();
			memcpy(&PORTVEC->PgmName[0], pgm, 31);
			PORTVEC->HOSTFLAGS = 128;				 // SET ALLOCATED BIT, clear others
		}
	}

	return (0);
}

DllExport struct PORTCONTROL * APIENTRY GetPortTableEntry(int portslot)		// Kept for Legacy apps
{
	struct PORTCONTROL * PORTVEC=PORTTABLE;

	if (portslot>NUMBEROFPORTS)
		portslot=NUMBEROFPORTS;

	while (--portslot > 0)
		PORTVEC=PORTVEC->PORTPOINTER;

	return PORTVEC;
}

// Proc below renamed to avoid confusion with GetPortTableEntryFromPortNum

DllExport struct PORTCONTROL * APIENTRY GetPortTableEntryFromSlot(int portslot)
{
	struct PORTCONTROL * PORTVEC=PORTTABLE;

	if (portslot>NUMBEROFPORTS)
		portslot=NUMBEROFPORTS;

	while (--portslot > 0)
		PORTVEC=PORTVEC->PORTPOINTER;

	return PORTVEC;
}

struct PORTCONTROL * APIENTRY GetPortTableEntryFromPortNum(int portnum)
{
	struct PORTCONTROL * PORTVEC=PORTTABLE;

	do
	{
		if (PORTVEC->PORTNUMBER == portnum)
			return PORTVEC;

		PORTVEC=PORTVEC->PORTPOINTER;
	}
	while (PORTVEC);

	return NULL;
}

DllExport UCHAR * APIENTRY GetPortDescription(int portslot, char * Desc)
{
	struct PORTCONTROL * PORTVEC=PORTTABLE;

	if (portslot>NUMBEROFPORTS)
		portslot=NUMBEROFPORTS;

	while (--portslot > 0)
		PORTVEC=PORTVEC->PORTPOINTER;

	memcpy(Desc, PORTVEC->PORTDESCRIPTION, 30);
	Desc[30]=0;

	return 0;
}

// Standard serial port handling routines, used by lots of modules.

OpenCOMMPort(struct TNCINFO * conn, char * Port, int Speed, BOOL Quiet)
{
	char buf[80];
	int PortNum;

	if (conn->WEB_COMMSSTATE == NULL)
		conn->WEB_COMMSSTATE = zalloc(100);

	if (_memicmp(Port, "COM", 3) == 0)
	{
		PortNum = atoi(&Port[3]);
		conn->hDevice = OpenCOMPort((VOID *)PortNum, Speed, TRUE, TRUE, Quiet, 0);
	}
	else
		conn->hDevice = OpenCOMPort((VOID *)Port, Speed, TRUE, TRUE, Quiet, 0);

	if (conn->hDevice == 0)
	{
		sprintf(conn->WEB_COMMSSTATE,"%s Open failed - Error %d", Port, GetLastError());
		SetWindowText(conn->xIDC_COMMSSTATE, conn->WEB_COMMSSTATE);

		return (FALSE);
	}

	sprintf(conn->WEB_COMMSSTATE,"COM%s Open", Port);
	SetWindowText(conn->xIDC_COMMSSTATE, buf);

	return TRUE;
}



#ifdef WIN32

HANDLE OpenCOMPort(VOID * pPort, int speed, BOOL SetDTR, BOOL SetRTS, BOOL Quiet, int Stopbits)
{
	char szPort[80];
	BOOL fRetVal ;
	COMMTIMEOUTS  CommTimeOuts ;
	int	Err;
	char buf[100];
	HANDLE fd;
	DCB dcb;

	// if Port Name starts COM, convert to \\.\COM or ports above 10 wont work

	if ((UINT)pPort < 256)			// just a com port number
		sprintf( szPort, "\\\\.\\COM%d", pPort);

	else if (_memicmp(pPort, "COM", 3) == 0)
	{
		char * pp = (char *)pPort;
		int p = atoi(&pp[3]);
		sprintf( szPort, "\\\\.\\COM%d", p);
	}
	else
		strcpy(szPort, pPort);

	// open COMM device

	fd = CreateFile( szPort, GENERIC_READ | GENERIC_WRITE,
                  0,                    // exclusive access
                  NULL,                 // no security attrs
                  OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL,
                  NULL );

	if (fd == (HANDLE) -1)
	{
		if (Quiet == 0)
		{
			if (pPort < (VOID *)256)
				sprintf(buf," COM%d could not be opened \r\n ", (UINT)pPort);
			else
				sprintf(buf," %s could not be opened \r\n ", pPort);

	//		WritetoConsoleLocal(buf);
			OutputDebugString(buf);
		}
		return (FALSE);
	}

	Err = GetFileType(fd);

	// setup device buffers

	SetupComm(fd, 4096, 4096 ) ;

	// purge any information in the buffer

	PurgeComm(fd, PURGE_TXABORT | PURGE_RXABORT |
                                      PURGE_TXCLEAR | PURGE_RXCLEAR ) ;

	// set up for overlapped I/O

	CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF ;
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0 ;
	CommTimeOuts.ReadTotalTimeoutConstant = 0 ;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0 ;
//     CommTimeOuts.WriteTotalTimeoutConstant = 0 ;
	CommTimeOuts.WriteTotalTimeoutConstant = 500 ;
	SetCommTimeouts(fd, &CommTimeOuts ) ;

   dcb.DCBlength = sizeof( DCB ) ;

   GetCommState(fd, &dcb ) ;

   dcb.BaudRate = speed;
   dcb.ByteSize = 8;
   dcb.Parity = 0;
   dcb.StopBits = TWOSTOPBITS;
   dcb.StopBits = Stopbits;

	// setup hardware flow control

	dcb.fOutxDsrFlow = 0;
	dcb.fDtrControl = DTR_CONTROL_DISABLE ;

	dcb.fOutxCtsFlow = 0;
	dcb.fRtsControl = RTS_CONTROL_DISABLE ;

	// setup software flow control

   dcb.fInX = dcb.fOutX = 0;
   dcb.XonChar = 0;
   dcb.XoffChar = 0;
   dcb.XonLim = 100 ;
   dcb.XoffLim = 100 ;

   // other various settings

   dcb.fBinary = TRUE ;
   dcb.fParity = FALSE;

   fRetVal = SetCommState(fd, &dcb);

	if (fRetVal)
	{
		if (SetDTR)
			EscapeCommFunction(fd, SETDTR);
		if (SetRTS)
			EscapeCommFunction(fd, SETRTS);
	}
	else
	{
		if ((UINT)pPort < 256)
			sprintf(buf,"COM%d Setup Failed %d ", pPort, GetLastError());
		else
			sprintf(buf,"%s Setup Failed %d ", pPort, GetLastError());

		WritetoConsoleLocal(buf);
		OutputDebugString(buf);
		CloseHandle(fd);
		return 0;
	}

	return fd;

}

int ReadCOMBlock(HANDLE fd, char * Block, int MaxLength )
{
	BOOL       fReadStat ;
	COMSTAT    ComStat ;
	DWORD      dwErrorFlags;
	DWORD      dwLength;

	// only try to read number of bytes in queue

	ClearCommError(fd, &dwErrorFlags, &ComStat);

	dwLength = min((DWORD) MaxLength, ComStat.cbInQue);

	if (dwLength > 0)
	{
		fReadStat = ReadFile(fd, Block, dwLength, &dwLength, NULL) ;

		if (!fReadStat)
		{
		    dwLength = 0 ;
			ClearCommError(fd, &dwErrorFlags, &ComStat ) ;
		}
	}

   return dwLength;
}


BOOL WriteCOMBlock(HANDLE fd, char * Block, int BytesToWrite)
{
	BOOL        fWriteStat;
	DWORD       BytesWritten;
	DWORD       ErrorFlags;
	COMSTAT     ComStat;

	fWriteStat = WriteFile(fd, Block, BytesToWrite,
	                       &BytesWritten, NULL );

	if ((!fWriteStat) || (BytesToWrite != BytesWritten))
	{
		int Err = GetLastError();
		ClearCommError(fd, &ErrorFlags, &ComStat);
		return FALSE;
	}
	return TRUE;
}

VOID CloseCOMPort(HANDLE fd)
{
	SetCommMask(fd, 0);

	// drop DTR

	COMClearDTR(fd);

	// purge any outstanding reads/writes and close device handle

	PurgeComm(fd, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ) ;

	CloseHandle(fd);
}


VOID COMSetDTR(HANDLE fd)
{
	EscapeCommFunction(fd, SETDTR);
}

VOID COMClearDTR(HANDLE fd)
{
	EscapeCommFunction(fd, CLRDTR);
}

VOID COMSetRTS(HANDLE fd)
{
	EscapeCommFunction(fd, SETRTS);
}

VOID COMClearRTS(HANDLE fd)
{
	EscapeCommFunction(fd, CLRRTS);
}


#else

static struct speed_struct
{
	int	user_speed;
	speed_t termios_speed;
} speed_table[] = {
	{300,         B300},
	{600,         B600},
	{1200,        B1200},
	{2400,        B2400},
	{4800,        B4800},
	{9600,        B9600},
	{19200,       B19200},
	{38400,       B38400},
	{57600,       B57600},
	{115200,      B115200},
	{-1,          B0}
};


HANDLE OpenCOMPort(VOID * pPort, int speed, BOOL SetDTR, BOOL SetRTS, BOOL Quiet, int Stopbits)
{
	char Port[256];
	char buf[100];

	//	Linux Version.

	int fd;
	int hwflag = 0;
	u_long param=1;
	struct termios term;
	struct speed_struct *s;

	// As Serial ports under linux can have all sorts of odd names, the code assumes that
	// they are symlinked to a com1-com255 in the BPQ Directory (normally the one it is started from

	if ((UINT)pPort < 256)
		sprintf(Port, "%s/com%d", BPQDirectory, (int)pPort);
	else
		strcpy(Port, pPort);

	if ((fd = open(Port, O_RDWR | O_NDELAY)) == -1)
	{
		if (Quiet == 0)
		{
			perror("Com Open Failed");
			sprintf(buf," %s could not be opened \n", Port);
			WritetoConsoleLocal(buf);
			Debugprintf(buf);
		}
		return 0;
	}

	// Validate Speed Param

	for (s = speed_table; s->user_speed != -1; s++)
		if (s->user_speed == speed)
			break;

   if (s->user_speed == -1)
   {
	   fprintf(stderr, "tty_speed: invalid speed %d\n", speed);
	   return FALSE;
   }

   if (tcgetattr(fd, &term) == -1)
   {
	   perror("tty_speed: tcgetattr");
	   return FALSE;
   }

   	cfmakeraw(&term);
	cfsetispeed(&term, s->termios_speed);
	cfsetospeed(&term, s->termios_speed);

	if (tcsetattr(fd, TCSANOW, &term) == -1)
	{
		perror("tty_speed: tcsetattr");
		return FALSE;
	}

	ioctl(fd, FIONBIO, &param);

	Debugprintf("LinBPQ Port %s fd %d", Port, fd);

	return fd;
}

int ReadCOMBlock(HANDLE fd, char * Block, int MaxLength)
{
	int Length;

	Length = read(fd, Block, MaxLength);

	if (Length < 0)
	{
		if (errno != 11 && errno != 35)					// Would Block
		{
			perror("read");
			printf("Handle %d Errno %d\n", fd, errno);
		}
		return 0;
	}

	return Length;
}

BOOL WriteCOMBlock(HANDLE fd, char * Block, int BytesToWrite)
{
	//	Some systems seem to have a very small max write size
	
	int ToSend = BytesToWrite;
	int Sent = 0, ret;

	while (ToSend)
	{
		ret = write(fd, &Block[Sent], ToSend);

		if (ret >= ToSend)
			return TRUE;

		if (ret == -1)
		{
			if (errno != 11 && errno != 35)					// Would Block
				return FALSE;
	
			usleep(10000);
			ret = 0;
		}
						
		Sent += ret;
		ToSend -= ret;
	}
	return TRUE;
}

VOID CloseCOMPort(HANDLE fd)
{
	close(fd);
}

VOID COMSetDTR(HANDLE fd)
{
	int status;

	ioctl(fd, TIOCMGET, &status);
	status |= TIOCM_DTR;
    ioctl(fd, TIOCMSET, &status);
}

VOID COMClearDTR(HANDLE fd)
{
	int status;

	ioctl(fd, TIOCMGET, &status);
	status &= ~TIOCM_DTR;
    ioctl(fd, TIOCMSET, &status);
}

VOID COMSetRTS(HANDLE fd)
{
	int status;

	ioctl(fd, TIOCMGET, &status);
	status |= TIOCM_RTS;
    ioctl(fd, TIOCMSET, &status);
}

VOID COMClearRTS(HANDLE fd)
{
	int status;

	ioctl(fd, TIOCMGET, &status);
	status &= ~TIOCM_RTS;
    ioctl(fd, TIOCMSET, &status);
}

#endif


int MaxNodes;
int MaxRoutes;
int NodeLen;
int RouteLen;
struct DEST_LIST * Dests;
struct ROUTE * Routes;

FILE *file;

int DoRoutes()
{
	char digis[30] = "";
	char locked[3]; 
	int count, len;
	char Normcall[10], Portcall[10];
	char line[80];

	for (count=0; count<MaxRoutes; count++)
	{
		if (Routes->NEIGHBOUR_CALL[0] != 0)
		{
			len=ConvFromAX25(Routes->NEIGHBOUR_CALL,Normcall);
			Normcall[len]=0;

			if ((Routes->NEIGHBOUR_FLAG & 1) == 1)

				strcpy(locked," !");
			else
				strcpy(locked," ");


			if (Routes->NEIGHBOUR_DIGI1[0] != 0)
			{
				memcpy(digis," VIA ",5);

				len=ConvFromAX25(Routes->NEIGHBOUR_DIGI1,Portcall);
				Portcall[len]=0;
				strcpy(&digis[5],Portcall);

				if (Routes->NEIGHBOUR_DIGI2[0] != 0)
				{
					len=ConvFromAX25(Routes->NEIGHBOUR_DIGI2,Portcall);
					Portcall[len]=0;
					strcat(digis," ");
					strcat(digis,Portcall);
				}
			}
			else
				digis[0] = 0;

			len=sprintf(line,
					"ROUTE ADD %s %d %d%s%s %d %d %d %d %d\n",
					Normcall,
					Routes->NEIGHBOUR_PORT,
					Routes->NEIGHBOUR_QUAL,	locked, digis,
					Routes->NBOUR_MAXFRAME,
					Routes->NBOUR_FRACK,
					Routes->NBOUR_PACLEN,
					Routes->INP3Node | (Routes->NoKeepAlive << 2),
					Routes->OtherendsRouteQual);

					fputs(line, file);
		}

		Routes+=1;
	}

	return (0);
}

int DoNodes()
{
	int count, len, cursor, i;
	char Normcall[10], Portcall[10];
	char line[80];
	char Alias[7];

	Dests-=1;

	for (count=0; count<MaxNodes; count++)
	{
		Dests+=1;

		if (Dests->NRROUTE[0].ROUT_NEIGHBOUR == 0)
			continue;

		{
			len=ConvFromAX25(Dests->DEST_CALL,Normcall);
			Normcall[len]=0;

			memcpy(Alias,Dests->DEST_ALIAS,6);

			Alias[6]=0;

			for (i=0;i<6;i++)
			{
				if (Alias[i] == ' ')
					Alias[i] = 0;
			}

			cursor=sprintf(line,"NODE ADD %s:%s ", Alias,Normcall);

			if (Dests->NRROUTE[0].ROUT_NEIGHBOUR != 0 && Dests->NRROUTE[0].ROUT_NEIGHBOUR->INP3Node == 0)
			{
				len=ConvFromAX25(
					Dests->NRROUTE[0].ROUT_NEIGHBOUR->NEIGHBOUR_CALL,Portcall);
				Portcall[len]=0;

				len=sprintf(&line[cursor],"%s %d %d ",
					Portcall,
					Dests->NRROUTE[0].ROUT_NEIGHBOUR->NEIGHBOUR_PORT,
					Dests->NRROUTE[0].ROUT_QUALITY);

				cursor+=len;

				if (Dests->NRROUTE[0].ROUT_OBSCOUNT > 127)
				{
					len=sprintf(&line[cursor],"! ");
					cursor+=len;
				}
			}

			if (Dests->NRROUTE[1].ROUT_NEIGHBOUR != 0 && Dests->NRROUTE[1].ROUT_NEIGHBOUR->INP3Node == 0)
			{
				len=ConvFromAX25(
					Dests->NRROUTE[1].ROUT_NEIGHBOUR->NEIGHBOUR_CALL,Portcall);
				Portcall[len]=0;

				len=sprintf(&line[cursor],"%s %d %d ",
					Portcall,
					Dests->NRROUTE[1].ROUT_NEIGHBOUR->NEIGHBOUR_PORT,
					Dests->NRROUTE[1].ROUT_QUALITY);

				cursor+=len;

				if (Dests->NRROUTE[1].ROUT_OBSCOUNT > 127)
				{
					len=sprintf(&line[cursor],"! ");
					cursor+=len;
				}
			}

		if (Dests->NRROUTE[2].ROUT_NEIGHBOUR != 0 && Dests->NRROUTE[2].ROUT_NEIGHBOUR->INP3Node == 0)
		{
			len=ConvFromAX25(
				Dests->NRROUTE[2].ROUT_NEIGHBOUR->NEIGHBOUR_CALL,Portcall);
			Portcall[len]=0;

			len=sprintf(&line[cursor],"%s %d %d ",
				Portcall,
				Dests->NRROUTE[2].ROUT_NEIGHBOUR->NEIGHBOUR_PORT,
				Dests->NRROUTE[2].ROUT_QUALITY);

			cursor+=len;

			if (Dests->NRROUTE[2].ROUT_OBSCOUNT > 127)
			{
				len=sprintf(&line[cursor],"! ");
				cursor+=len;
			}
		}

		if (cursor > 30)
		{
			line[cursor++]='\n';
			line[cursor++]=0;
			fputs(line, file);
		}
		}
	}
	return (0);
}

int APIENTRY SaveNodes ()
{
	char FN[250];

	Routes = NEIGHBOURS;
	RouteLen = ROUTE_LEN;
	MaxRoutes = MAXNEIGHBOURS;

	Dests = DESTS;
	NodeLen = DEST_LIST_LEN;
	MaxNodes = MAXDESTS;

	// Set up pointer to BPQNODES file

	if (BPQDirectory[0] == 0)
	{
		strcpy(FN,"BPQNODES.dat");
	}
	else
	{
		strcpy(FN,BPQDirectory);
		strcat(FN,"/");
		strcat(FN,"BPQNODES.dat");
	}

	if ((file = fopen(FN, "w")) == NULL)
		return FALSE;

	DoRoutes();
	DoNodes();

	fclose(file);

	return (0);
}

DllExport int APIENTRY ClearNodes ()
{
	char FN[250];

	// Set up pointer to BPQNODES file

	if (BPQDirectory[0] == 0)
	{
		strcpy(FN,"BPQNODES.dat");
	}
	else
	{
		strcpy(FN,BPQDirectory);
		strcat(FN,"/");
		strcat(FN,"BPQNODES.dat");
	}

	if ((file = fopen(FN, "w")) == NULL)
		return FALSE;

	fclose(file);

	return (0);
}
char * FormatUptime(int Uptime)
 {
	struct tm * TM;
	static char UPTime[50];
	time_t szClock = Uptime * 60;

	TM = gmtime(&szClock);

	sprintf(UPTime, "Uptime (Days Hours Mins)     %.2d:%.2d:%.2d\r",
		TM->tm_yday, TM->tm_hour, TM->tm_min);

	return UPTime;
 }

 char * FormatMH(PMHSTRUC MH)
 {
	struct tm * TM;
	static char MHTime[50];
	time_t szClock;
	char LOC[7];

	memcpy(LOC, MH->MHLocator, 6);
	LOC[6] = 0;

	szClock = time(NULL) - MH->MHTIME;

	TM = gmtime(&szClock);

	sprintf(MHTime, "%.2d:%.2d:%.2d:%.2d  %s %s",
		TM->tm_yday, TM->tm_hour, TM->tm_min, TM->tm_sec, MH->MHFreq, LOC);

	return MHTime;

 }

Dll VOID APIENTRY CreateOneTimePassword(char * Password, char * KeyPhrase, int TimeOffset)
{
	// Create a time dependent One Time Password from the KeyPhrase
	// TimeOffset is used when checking to allow for slight variation in clocks

	time_t NOW = time(NULL);
	UCHAR Hash[16];
	char Key[1000];
	int i, chr;

	NOW = NOW/30 + TimeOffset;				// Only Change every 30 secs

	sprintf(Key, "%s%x", KeyPhrase, (int)NOW);

	md5(Key, Hash);

	for (i=0; i<16; i++)
	{
		chr = (Hash[i] & 31);
		if (chr > 9) chr += 7;

		Password[i] = chr + 48;
	}

	Password[16] = 0;
	return;
}

Dll BOOL APIENTRY CheckOneTimePassword(char * Password, char * KeyPhrase)
{
	char CheckPassword[17];
	int Offsets[10] = {0, -1, 1, -2, 2, -3, 3, -4, 4};
	int i, Pass;

	if (strlen(Password) < 16)
		Pass = atoi(Password);

	for (i = 0; i < 9; i++)
	{
		CreateOneTimePassword(CheckPassword, KeyPhrase, Offsets[i]);

		if (strlen(Password) < 16)
		{
			// Using a numeric extract

			long long Val;

			memcpy(&Val, CheckPassword, 8);
			Val = Val %= 1000000;

			if (Pass == Val)
				return TRUE;
		}
		else
			if (memcmp(Password, CheckPassword, 16) == 0)
				return TRUE;
	}

	return FALSE;
}


DllExport BOOL ConvToAX25Ex(unsigned char * callsign, unsigned char * ax25call)
{
	// Allows SSID's of 'T and 'R'
	
	int i;

	memset(ax25call,0x40,6);		// in case short
	ax25call[6]=0x60;				// default SSID

	for (i=0;i<7;i++)
	{
		if (callsign[i] == '-')
		{
			//
			//	process ssid and return
			//
			
			if (callsign[i+1] == 'T')
			{
				ax25call[6]=0x42;
				return TRUE;
			}

			if (callsign[i+1] == 'R')
			{
				ax25call[6]=0x44;
				return TRUE;
			}
			i = atoi(&callsign[i+1]);

			if (i < 16)
			{
				ax25call[6] |= i<<1;
				return (TRUE);
			}
			return (FALSE);
		}

		if (callsign[i] == 0 || callsign[i] == 13 || callsign[i] == ' ' || callsign[i] == ',')
		{
			//
			//	End of call - no ssid
			//
			return (TRUE);
		}

		ax25call[i] = callsign[i] << 1;
	}

	//
	//	Too many chars
	//

	return (FALSE);
}


DllExport BOOL ConvToAX25(unsigned char * callsign, unsigned char * ax25call)
{
	int i;

	memset(ax25call,0x40,6);		// in case short
	ax25call[6]=0x60;				// default SSID

	for (i=0;i<7;i++)
	{
		if (callsign[i] == '-')
		{
			//
			//	process ssid and return
			//
			i = atoi(&callsign[i+1]);

			if (i < 16)
			{
				ax25call[6] |= i<<1;
				return (TRUE);
			}
			return (FALSE);
		}

		if (callsign[i] == 0 || callsign[i] == 13 || callsign[i] == ' ' || callsign[i] == ',')
		{
			//
			//	End of call - no ssid
			//
			return (TRUE);
		}

		ax25call[i] = callsign[i] << 1;
	}

	//
	//	Too many chars
	//

	return (FALSE);
}


DllExport int ConvFromAX25(unsigned char * incall,unsigned char * outcall)
{
	int in,out=0;
	unsigned char chr;

	memset(outcall,0x20,10);

	for (in=0;in<6;in++)
	{
		chr=incall[in];
		if (chr == 0x40)
			break;
		chr >>= 1;
		outcall[out++]=chr;
	}

	chr=incall[6];				// ssid

	if (chr == 0x42)
	{
		outcall[out++]='-';
		outcall[out++]='T';
		return out;
	}

	if (chr == 0x44)
	{
		outcall[out++]='-';
		outcall[out++]='R';
		return out;
	}

	chr >>= 1;
	chr	&= 15;

	if (chr > 0)
	{
		outcall[out++]='-';
		if (chr > 9)
		{
			chr-=10;
			outcall[out++]='1';
		}
		chr+=48;
		outcall[out++]=chr;
	}
	return (out);
}

unsigned short int compute_crc(unsigned char *buf, int txlen);

SOCKADDR_IN reportdest = {0};

SOCKET ReportSocket = 0;

SOCKADDR_IN Chatreportdest = {0};

extern char LOCATOR[];			// Locator for Reporting - may be Maidenhead or LAT:LON
extern char MAPCOMMENT[];		// Locator for Reporting - may be Maidenhead or LAT:LON
extern char LOC[7];				// Maidenhead Locator for Reporting
extern char ReportDest[7];


VOID SendReportMsg(char * buff, int txlen)
{
 	unsigned short int crc = compute_crc(buff, txlen);

	crc ^= 0xffff;

	buff[txlen++] = (crc&0xff);
	buff[txlen++] = (crc>>8);

	sendto(ReportSocket, buff, txlen, 0, (LPSOCKADDR)&reportdest, sizeof(reportdest));

}
VOID SendLocation()
{
	MESSAGE AXMSG;
	PMESSAGE AXPTR = &AXMSG;
	char Msg[512];
	int Len;

	Len = sprintf(Msg, "%s %s<br>%s", LOCATOR, VersionString, MAPCOMMENT);

#ifdef LINBPQ
	Len = sprintf(Msg, "%s L%s<br>%s", LOCATOR, VersionString, MAPCOMMENT);
#endif
#ifdef MACBPQ
	Len = sprintf(Msg, "%s M%s<br>%s", LOCATOR, VersionString, MAPCOMMENT);
#endif

	if (Len > 256)
		Len = 256;

	// Block includes the Msg Header (7 bytes), Len Does not!

	memcpy(AXPTR->DEST, ReportDest, 7);
	memcpy(AXPTR->ORIGIN, MYCALL, 7);
	AXPTR->DEST[6] &= 0x7e;			// Clear End of Call
	AXPTR->DEST[6] |= 0x80;			// set Command Bit

	AXPTR->ORIGIN[6] |= 1;			// Set End of Call
	AXPTR->CTL = 3;		//UI
	AXPTR->PID = 0xf0;
	memcpy(AXPTR->L2DATA, Msg, Len);

	SendReportMsg((char *)&AXMSG.DEST, Len + 16);

	return;

}




VOID SendMH(int Hardware, char * call, char * freq, char * LOC, char * Mode)
{
	MESSAGE AXMSG;
	PMESSAGE AXPTR = &AXMSG;
	char Msg[100];
	int Len;

	if (ReportSocket == 0 || LOCATOR[0] == 0)
		return;

	Len = sprintf(Msg, "MH %s,%s,%s,%s", call, freq, LOC, Mode);

	// Block includes the Msg Header (7 bytes), Len Does not!

	memcpy(AXPTR->DEST, ReportDest, 7);
	memcpy(AXPTR->ORIGIN, MYCALL, 7);
	AXPTR->DEST[6] &= 0x7e;			// Clear End of Call
	AXPTR->DEST[6] |= 0x80;			// set Command Bit

	AXPTR->ORIGIN[6] |= 1;			// Set End of Call
	AXPTR->CTL = 3;		//UI
	AXPTR->PID = 0xf0;
	memcpy(AXPTR->L2DATA, Msg, Len);

	SendReportMsg((char *)&AXMSG.DEST, Len + 16) ;

	return;

}
DllExport char * APIENTRY GetApplCall(int Appl)
{
	if (Appl < 1 || Appl > NumberofAppls ) return NULL;

	return (UCHAR *)(&APPLCALLTABLE[Appl-1].APPLCALL_TEXT);
}
DllExport char * APIENTRY GetApplAlias(int Appl)
{
	if (Appl < 1 || Appl > NumberofAppls ) return NULL;

	return (UCHAR *)(&APPLCALLTABLE[Appl-1].APPLALIAS_TEXT);
}

DllExport long APIENTRY GetApplQual(int Appl)
{
	if (Appl < 1 || Appl > NumberofAppls ) return 0;

	return (APPLCALLTABLE[Appl-1].APPLQUAL);
}

char * GetApplCallFromName(char * App)
{
	int i;
	char PaddedAppl[13] = "            ";

	memcpy(PaddedAppl, App, strlen(App));

	for (i = 0; i < NumberofAppls; i++)
	{
		if (memcmp(&APPLCALLTABLE[i].APPLCMD, PaddedAppl, 12) == 0)
			return &APPLCALLTABLE[i].APPLCALL_TEXT[0];
	}
	return NULL;
}


DllExport char * APIENTRY GetApplName(int Appl)
{
	if (Appl < 1 || Appl > NumberofAppls ) return NULL;

	return (UCHAR *)(&APPLCALLTABLE[Appl-1].APPLCMD);
}

DllExport int APIENTRY GetNumberofPorts()
{
	return (NUMBEROFPORTS);
}

DllExport int APIENTRY GetPortNumber(int portslot)
{
	struct PORTCONTROL * PORTVEC=PORTTABLE;

	if (portslot>NUMBEROFPORTS)
		portslot=NUMBEROFPORTS;

	while (--portslot > 0)
		PORTVEC=PORTVEC->PORTPOINTER;

	return PORTVEC->PORTNUMBER;

}

DllExport char * APIENTRY GetVersionString()
{
//	return ((char *)&VersionStringWithBuild);
	return ((char *)&VersionString);
}

#ifdef MACBPQ

//Fiddle till I find a better solution

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1060
int __sync_lock_test_and_set(int * ptr, int val)
{
	*ptr = val;
	return 0;
}
#endif // __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#endif // MACBPQ

void GetSemaphore(struct SEM * Semaphore, int ID)
{
	//
	//	Wait for it to be free
	//

	if (Semaphore->Flag != 0)
	{
		Semaphore->Clashes++;
	}

loop1:

	while (Semaphore->Flag != 0)
	{
		Sleep(10);
	}

	//
	//	try to get semaphore
	//

#ifdef WIN32

	_asm{

	mov	eax,1
	mov ebx, Semaphore
	xchg [ebx],eax		// this instruction is locked

	cmp	eax,0
	jne loop1			// someone else got it - try again

	}

#else

	if (__sync_lock_test_and_set(&Semaphore->Flag, 1) != 0)

		// Failed to get it
		goto loop1;		// try again;

#endif

	//Ok. got it

	Semaphore->Gets++;
	Semaphore->SemProcessID = GetCurrentProcessId();
	Semaphore->SemThreadID = GetCurrentThreadId();
	SemHeldByAPI = ID;

	return;
}

void FreeSemaphore(struct SEM * Semaphore)
{
	if (Semaphore->Flag == 0)
		Debugprintf("Free Semaphore Called when Sem not held");

	Semaphore->Rels++;
	Semaphore->Flag = 0;

	return;
}

#ifdef WIN32

#include "DbgHelp.h"

USHORT WINAPI RtlCaptureStackBackTrace(
  __in       ULONG FramesToSkip,
  __in       ULONG FramesToCapture,
  __out      PVOID *BackTrace,
  __out_opt  PULONG BackTraceHash
);
#endif

void printStack(void)
{
#ifdef WIN32
#ifdef _DEBUG					// So we can use on 98/2K

     unsigned int   i;
     void         * stack[ 100 ];
     unsigned short frames;
     SYMBOL_INFO  * symbol;
     HANDLE         process;

	 Debugprintf("Stack Backtrace");

     process = GetCurrentProcess();

     SymInitialize( process, NULL, TRUE );

     frames               = RtlCaptureStackBackTrace( 0, 60, stack, NULL );
     symbol               = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + 256 * sizeof( char ), 1 );
     symbol->MaxNameLen   = 255;
     symbol->SizeOfStruct = sizeof( SYMBOL_INFO );

     for( i = 0; i < frames; i++ )
     {
         SymFromAddr( process, ( DWORD64 )( stack[ i ] ), 0, symbol );

         Debugprintf( "%i: %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address );
     }

     free(symbol);

#endif
#endif
}

pthread_t ResolveUpdateThreadId = 0;

VOID ResolveUpdateThread()
{
	struct hostent * HostEnt1;
	struct hostent * HostEnt2;

	ResolveUpdateThreadId = GetCurrentThreadId();

	while (TRUE)
	{
		if (pthread_equal(ResolveUpdateThreadId, GetCurrentThreadId()) == FALSE)
		{
			Debugprintf("Resolve Update thread %x redundant - closing", GetCurrentThreadId());
			return;
		}

		//	Resolve name to address

		Debugprintf("Resolving %s", "update.g8bpq.net");
		HostEnt1 = gethostbyname ("update.g8bpq.net");

		if (HostEnt1)
			memcpy(&reportdest.sin_addr.s_addr,HostEnt1->h_addr,4);

		Debugprintf("Resolving %s", "chatmap.g8bpq.net");
		HostEnt2 = gethostbyname ("chatmap.g8bpq.net");

		if (HostEnt2)
			memcpy(&Chatreportdest.sin_addr.s_addr,HostEnt2->h_addr,4);

		if (HostEnt1 && HostEnt2)
		{
			Sleep(1000 * 60 * 30);
			continue;
		}

		Debugprintf("Resolve Failed for update.g8bpq.net or chatmap.g8bpq.net");
		Sleep(1000 * 60 * 5);
	}
}


VOID OpenReportingSockets()
{
	u_long param=1;
	BOOL bcopt=TRUE;

	if (LOCATOR[0])
	{
		// Enable Node Map Reports

		ReportTimer = 600;

		ReportSocket = socket(AF_INET,SOCK_DGRAM,0);

		if (ReportSocket == INVALID_SOCKET)
		{
			Debugprintf("Failed to create Reporting socket");
			ReportSocket = 0;
  		 	return;
		}

		ioctlsocket (ReportSocket, FIONBIO, &param);
		setsockopt (ReportSocket, SOL_SOCKET, SO_BROADCAST, (const char FAR *)&bcopt,4);

		reportdest.sin_family = AF_INET;
		reportdest.sin_port = htons(81);
		ConvToAX25("DUMMY-1", ReportDest);
	}

	// Set up Chat Report even if no LOCATOR	reportdest.sin_family = AF_INET;
	// Socket must be opened in MailChat Process

	Chatreportdest.sin_family = AF_INET;
	Chatreportdest.sin_port = htons(10090);

	_beginthread(ResolveUpdateThread,0,(int)0);
}

/*
VOID WriteMiniDump()
{
#ifdef WIN32

	HANDLE hFile;
	BOOL ret;
	char FN[256];

	sprintf(FN, "MiniDump%d.dmp", (time(NULL) & 0xffff));

	hFile = CreateFile(FN, GENERIC_READ | GENERIC_WRITE,
		0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE))
	{
		// Create the minidump

		ret = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
			hFile, MiniDumpNormal, 0, 0, 0 );

		if(!ret)
			Debugprintf("MiniDumpWriteDump failed. Error: %u", GetLastError());
		else
			Debugprintf("Minidump %s created.", FN);
			CloseHandle(hFile);
	}
#endif
}
*/

// UI Util Code

#pragma pack(1)

typedef struct _MESSAGEX
{
//	BASIC LINK LEVEL MESSAGE BUFFER LAYOUT

	struct _MESSAGEX * CHAIN;

	UCHAR	PORT;
	USHORT	LENGTH;

	UCHAR	DEST[7];
	UCHAR	ORIGIN[7];

//	 MAY BE UP TO 56 BYTES OF DIGIS

	UCHAR	CTL;
	UCHAR	PID;
	UCHAR	DATA[256];
	UCHAR	PADDING[56];			// In case he have Digis

}MESSAGEX, *PMESSAGEX;

#pragma pack()


int PortNum[33] = {0};				// Tab nunber to port

char * UIUIDigi[33]= {0};
char * UIUIDigiAX[33] = {0};		// ax.25 version of digistring
int UIUIDigiLen[33] = {0};			// Length of AX string

char UIUIDEST[33][11] = {0};		// Dest for Beacons

char UIAXDEST[33][7] = {0};


UCHAR FN[33][256];			// Filename
int Interval[33];			// Beacon Interval (Mins)
int MinCounter[33];			// Interval Countdown

BOOL SendFromFile[33];
char Message[33][1000];		// Beacon Text

VOID SendUIBeacon(int Port);

BOOL RunUI = TRUE;

VOID UIThread()
{
	int Port, MaxPorts = GetNumberofPorts();

	while (RunUI)
	{
		Sleep(60000);
		for (Port = 1; Port <= MaxPorts; Port++)
		{
			if (MinCounter[Port])
			{
				MinCounter[Port] --;

				if (MinCounter[Port] == 0)
				{
					MinCounter[Port] = Interval[Port];
					SendUIBeacon(Port);
				}
			}
		}
	}
}

int UIRemoveLF(char * Message, int len)
{
	// Remove lf chars

	char * ptr1, * ptr2;

	ptr1 = ptr2 = Message;

	while (len-- > 0)
	{
		*ptr2 = *ptr1;
	
		if (*ptr1 == '\r')
			if (*(ptr1+1) == '\n')
			{
				ptr1++;
				len--;
			}
		ptr1++;
		ptr2++;
	}

	return (ptr2 - Message);
}




VOID UISend_AX_Datagram(UCHAR * Msg, DWORD Len, UCHAR Port, UCHAR * HWADDR, BOOL Queue)
{
	MESSAGEX AXMSG;
	PMESSAGEX AXPTR = &AXMSG;
	int DataLen = Len;

	// Block includes the Msg Header (7 bytes), Len Does not!

	memcpy(AXPTR->DEST, HWADDR, 7);
	memcpy(AXPTR->ORIGIN, MYCALL, 7);
	AXPTR->DEST[6] &= 0x7e;			// Clear End of Call
	AXPTR->DEST[6] |= 0x80;			// set Command Bit

	if (UIUIDigi[Port])
	{
		// This port has a digi string

		int DigiLen = UIUIDigiLen[Port];
		UCHAR * ptr;

		memcpy(&AXPTR->CTL, UIUIDigiAX[Port], DigiLen);
		
		ptr = (UCHAR *)AXPTR;
		ptr += DigiLen;
		AXPTR = (PMESSAGEX)ptr;

		Len += DigiLen;
	}

	AXPTR->ORIGIN[6] |= 1;			// Set End of Call
	AXPTR->CTL = 3;		//UI
	AXPTR->PID = 0xf0;
	memcpy(AXPTR->DATA, Msg, DataLen);

//	if (Queue)
//		QueueRaw(Port, &AXMSG, Len + 16);
//	else
		SendRaw(Port, (char *)&AXMSG.DEST, Len + 16);

	return;

}



VOID SendUIBeacon(int Port)
{
	char UIMessage[1024];
	int Len = strlen(Message[Port]);
	int Index = 0;

	if (SendFromFile[Port])
	{
		FILE * hFile;

		hFile = fopen(FN[Port], "rb");
	
		if (hFile == 0)
			return;

		Len = fread(UIMessage, 1, 1024, hFile); 
		
		fclose(hFile);

	}
	else
		strcpy(UIMessage, Message[Port]);

	Len =  UIRemoveLF(UIMessage, Len);

	while (Len > 256)
	{
		UISend_AX_Datagram(&UIMessage[Index], 256, Port, UIAXDEST[Port], TRUE);
		Index += 256;
		Len -= 256;
		Sleep(2000);
	}
	UISend_AX_Datagram(&UIMessage[Index], Len, Port, UIAXDEST[Port], TRUE);
}

#ifndef LINBPQ

typedef struct tag_dlghdr
{
	HWND hwndTab; // tab control
	HWND hwndDisplay; // current child dialog box
	RECT rcDisplay; // display rectangle for the tab control

	DLGTEMPLATE *apRes[33];

} DLGHDR;

DLGTEMPLATE * WINAPI DoLockDlgRes(LPCSTR lpszResName);

#endif

HWND hwndDlg;
int PageCount;
int CurrentPage=0;				// Page currently on show in tabbed Dialog


VOID WINAPI OnSelChanged(HWND hwndDlg);
VOID WINAPI OnChildDialogInit(HWND hwndDlg);

#define ICC_STANDARD_CLASSES   0x00004000

HWND hwndDisplay;

#define ID_TEST                         102
#define IDD_DIAGLOG1                    103
#define IDC_FROMFILE                    1022
#define IDC_EDIT1                       1054
#define IDC_FILENAME                    1054
#define IDC_EDIT2                       1055
#define IDC_MESSAGE                     1055
#define IDC_EDIT3                       1056
#define IDC_INTERVAL                    1056
#define IDC_EDIT4                       1057
#define IDC_UIDEST                      1057
#define IDC_FILE                        1058
#define IDC_TAB1                        1059
#define IDC_UIDIGIS                     1059
#define IDC_PORTNAME                    1060

extern HKEY REGTREE;
HBRUSH bgBrush; 

VOID SetupUI(int Port)
{
	char DigiString[100], * DigiLeft;

	ConvToAX25(UIUIDEST[Port], &UIAXDEST[Port][0]);

	UIUIDigiLen[Port] = 0;

	if (UIUIDigi[Port])
	{
		UIUIDigiAX[Port] = zalloc(100);
		strcpy(DigiString, UIUIDigi[Port]);
		DigiLeft = strlop(DigiString,',');

		while(DigiString[0])
		{
			ConvToAX25(DigiString, &UIUIDigiAX[Port][UIUIDigiLen[Port]]);
			UIUIDigiLen[Port] += 7;

			if (DigiLeft)
			{
				memmove(DigiString, DigiLeft, strlen(DigiLeft) + 1);
				DigiLeft = strlop(DigiString,',');
			}
			else
				DigiString[0] = 0;
		}
	}
}

/*

VOID SaveIntValue(config_setting_t * group, char * name, int value)
{
	config_setting_t *setting;
	
	setting = config_setting_add(group, name, CONFIG_TYPE_INT);
	if(setting)
		config_setting_set_int(setting, value);
}

VOID SaveStringValue(config_setting_t * group, char * name, char * value)
{
	config_setting_t *setting;

	setting = config_setting_add(group, name, CONFIG_TYPE_STRING);
	if (setting)
		config_setting_set_string(setting, value);

}
*/

#ifdef LINBPQ

config_t cfg;

VOID SaveUIConfig()
{
	config_setting_t *root, *group, *UIGroup;
	int Port, MaxPort = GetNumberofPorts();
	char ConfigName[256];

	if (BPQDirectory[0] == 0)
	{
		strcpy(ConfigName,"UIUtil.cfg");
	}
	else
	{
		strcpy(ConfigName,BPQDirectory);
		strcat(ConfigName,"/");
		strcat(ConfigName,"UIUtil.cfg");
	}

	//	Get rid of old config before saving
	
	config_init(&cfg);

	root = config_root_setting(&cfg);

	group = config_setting_add(root, "main", CONFIG_TYPE_GROUP);

	UIGroup = config_setting_add(group, "UIUtil", CONFIG_TYPE_GROUP);

	for (Port = 1; Port <= MaxPort; Port++)
	{
		char Key[20];
		
		sprintf(Key, "Port%d", Port); 
		group = config_setting_add(UIGroup, Key, CONFIG_TYPE_GROUP);

		SaveStringValue(group, "UIDEST", &UIUIDEST[Port][0]);
		SaveStringValue(group, "FileName", &FN[Port][0]);
		SaveStringValue(group, "Message", &Message[Port][0]);
		SaveStringValue(group, "Digis", UIUIDigi[Port]);
	
		SaveIntValue(group, "Interval", Interval[Port]);
		SaveIntValue(group, "SendFromFile", SendFromFile[Port]);
	}

	if(!config_write_file(&cfg, ConfigName))
	{
		fprintf(stderr, "Error while writing file.\n");
		config_destroy(&cfg);
		return;
	}

	config_destroy(&cfg);
}
#endif

VOID GetUIConfig()
{
#ifdef LINBPQ

	char Key[100];
	char CfgFN[256];
	char Digis[100];
	struct stat STAT;

	config_t cfg;
	config_setting_t *root, *group;
	int Port, MaxPort = GetNumberofPorts();

	memset((void *)&cfg, 0, sizeof(config_t));

	config_init(&cfg);

	if (BPQDirectory[0] == 0)
	{
		strcpy(CfgFN,"UIUtil.cfg");
	}
	else
	{
		strcpy(CfgFN,BPQDirectory);
		strcat(CfgFN,"/");
		strcat(CfgFN,"UIUtil.cfg");
	}

	if (stat(CfgFN, &STAT) == -1)
	{
		Debugprintf("UIUtil Config File not found\n");
		return;
	}

	if(!config_read_file(&cfg, CfgFN))
	{
		fprintf(stderr, "UI Util Config Error Line %d - %s\n", config_error_line(&cfg), config_error_text(&cfg));

		config_destroy(&cfg);
		return;
	}

	group = config_lookup(&cfg, "main");

	if (group)
	{
		for (Port = 1; Port <= MaxPort; Port++)
		{	
			sprintf(Key, "main.UIUtil.Port%d", Port); 

			group = config_lookup (&cfg, Key);

			if (group)
			{
				GetStringValue(group, "UIDEST", &UIUIDEST[Port][0]);
				GetStringValue(group, "FileName", &FN[Port][0]);
				GetStringValue(group, "Message", &Message[Port][0]);
				GetStringValue(group, "Digis", Digis);
				UIUIDigi[Port] = _strdup(Digis);
	
				Interval[Port] = GetIntValue(group, "Interval");
				MinCounter[Port] = Interval[Port];

				SendFromFile[Port] = GetIntValue(group, "SendFromFile");

				SetupUI(Port);
			}
		}
	}

#else
	
	int retCode, Vallen, Type, i;
	char Key[80];
	char Size[80];
	HKEY hKey;
	RECT Rect;

	wsprintf(Key, "SOFTWARE\\G8BPQ\\BPQ32\\UIUtil");
	
	retCode = RegOpenKeyEx (REGTREE, Key, 0, KEY_QUERY_VALUE, &hKey);

	if (retCode == ERROR_SUCCESS)
	{
		Vallen=80;

		retCode = RegQueryValueEx(hKey,"Size",0,			
			(ULONG *)&Type,(UCHAR *)&Size,(ULONG *)&Vallen);

		if (retCode == ERROR_SUCCESS)
			sscanf(Size,"%d,%d,%d,%d",&Rect.left,&Rect.right,&Rect.top,&Rect.bottom);

		RegCloseKey(hKey);
	}

	for (i=1; i<=32; i++)
	{
		wsprintf(Key, "SOFTWARE\\G8BPQ\\BPQ32\\UIUtil\\UIPort%d", i);

		retCode = RegOpenKeyEx (REGTREE,
                              Key,
                              0,
                              KEY_QUERY_VALUE,
                              &hKey);

		if (retCode == ERROR_SUCCESS)
		{	
			Vallen=0;
			RegQueryValueEx(hKey,"Digis",0,			
				(ULONG *)&Type, NULL, (ULONG *)&Vallen);

			if (Vallen)
			{
				UIUIDigi[i] = malloc(Vallen);
				RegQueryValueEx(hKey,"Digis",0,			
					(ULONG *)&Type, UIUIDigi[i], (ULONG *)&Vallen);
			}

			Vallen=4;
			retCode = RegQueryValueEx(hKey, "Interval", 0,			
				(ULONG *)&Type, (UCHAR *)&Interval[i], (ULONG *)&Vallen);

			MinCounter[i] = Interval[i];

			Vallen=4;
			retCode = RegQueryValueEx(hKey, "SendFromFile", 0,			
				(ULONG *)&Type, (UCHAR *)&SendFromFile[i], (ULONG *)&Vallen);


			Vallen=10;
			retCode = RegQueryValueEx(hKey, "UIDEST", 0, &Type, &UIUIDEST[i][0], &Vallen);

			Vallen=255;
			retCode = RegQueryValueEx(hKey, "FileName", 0, &Type, &FN[i][0], &Vallen);

			Vallen=999;
			retCode = RegQueryValueEx(hKey, "Message", 0, &Type, &Message[i][0], &Vallen);

			SetupUI(i);

			RegCloseKey(hKey);
		}
	}
#endif

	_beginthread(UIThread,0,(int)0);

}

#ifndef LINBPQ

INT_PTR CALLBACK ChildDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
//	This processes messages from controls on the tab subpages
	int Command;

	int retCode, disp;
	char Key[80];
	HKEY hKey;
	BOOL OK;
	OPENFILENAME ofn;
	char Digis[100];

	int Port = PortNum[CurrentPage];


	switch (message)
	{
	case WM_NOTIFY:

        switch (((LPNMHDR)lParam)->code)
        {
		case TCN_SELCHANGE:
			 OnSelChanged(hDlg);
				 return TRUE;
         // More cases on WM_NOTIFY switch.
		case NM_CHAR:
			return TRUE;
        }

       break;
	case WM_INITDIALOG:
		OnChildDialogInit( hDlg);
		return (INT_PTR)TRUE;

	case WM_CTLCOLORDLG:

        return (LONG)bgBrush;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
		SetTextColor(hdcStatic, RGB(0, 0, 0));
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LONG)bgBrush;
    }


	case WM_COMMAND:

		Command = LOWORD(wParam);

		if (Command == 2002)
			return TRUE;

		switch (Command)
		{
			case IDC_FILE:

			memset(&ofn, 0, sizeof (OPENFILENAME));
			ofn.lStructSize = sizeof (OPENFILENAME);
			ofn.hwndOwner = hDlg;
			ofn.lpstrFile = &FN[Port][0];
			ofn.nMaxFile = 250;
			ofn.lpstrTitle = "File to send as beacon";
			ofn.lpstrInitialDir = BPQDirectory;

			if (GetOpenFileName(&ofn))
				SetDlgItemText(hDlg, IDC_FILENAME, &FN[Port][0]);

			break;


		case IDOK:

			GetDlgItemText(hDlg, IDC_UIDEST, &UIUIDEST[Port][0], 10);

			if (UIUIDigi[Port])
			{
				free(UIUIDigi[Port]);
				UIUIDigi[Port] = NULL;
			}

			if (UIUIDigiAX[Port])
			{
				free(UIUIDigiAX[Port]);
				UIUIDigiAX[Port] = NULL;
			}

			GetDlgItemText(hDlg, IDC_UIDIGIS, Digis, 99); 
		
			UIUIDigi[Port] = _strdup(Digis);
		
			GetDlgItemText(hDlg, IDC_FILENAME, &FN[Port][0], 255); 
			GetDlgItemText(hDlg, IDC_MESSAGE, &Message[Port][0], 1000); 
	
			Interval[Port] = GetDlgItemInt(hDlg, IDC_INTERVAL, &OK, FALSE); 

			MinCounter[Port] = Interval[Port];

			SendFromFile[Port] = IsDlgButtonChecked(hDlg, IDC_FROMFILE);

			wsprintf(Key, "SOFTWARE\\G8BPQ\\BPQ32\\UIUtil\\UIPort%d", PortNum[CurrentPage]);

			retCode = RegCreateKeyEx(REGTREE,
					Key, 0, 0, 0, KEY_ALL_ACCESS, NULL, &hKey, &disp);
	
			if (retCode == ERROR_SUCCESS)
			{
				retCode = RegSetValueEx(hKey, "UIDEST", 0, REG_SZ,(BYTE *)&UIUIDEST[Port][0], strlen(&UIUIDEST[Port][0]));
				retCode = RegSetValueEx(hKey, "FileName", 0, REG_SZ,(BYTE *)&FN[Port][0], strlen(&FN[Port][0]));
				retCode = RegSetValueEx(hKey, "Message", 0, REG_SZ,(BYTE *)&Message[Port][0], strlen(&Message[Port][0]));
				retCode = RegSetValueEx(hKey, "Interval", 0, REG_DWORD,(BYTE *)&Interval[Port], 4);
				retCode = RegSetValueEx(hKey, "SendFromFile", 0, REG_DWORD,(BYTE *)&SendFromFile[Port], 4);
				retCode = RegSetValueEx(hKey, "Digis",0, REG_SZ, Digis, strlen(Digis));

				RegCloseKey(hKey);
			}

			SetupUI(Port);

			return (INT_PTR)TRUE;


		case IDCANCEL:

			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;

		case ID_TEST:

			SendUIBeacon(Port);
			return TRUE;

		}
		break;

	}	
	return (INT_PTR)FALSE;
}



VOID WINAPI OnTabbedDialogInit(HWND hDlg)
{
	DLGHDR *pHdr = (DLGHDR *) LocalAlloc(LPTR, sizeof(DLGHDR));
	DWORD dwDlgBase = GetDialogBaseUnits();
	int cxMargin = LOWORD(dwDlgBase) / 4;
	int cyMargin = HIWORD(dwDlgBase) / 8;

	TC_ITEM tie;
	RECT rcTab;

	int i, pos, tab = 0;
	INITCOMMONCONTROLSEX init;

	char PortNo[60];
	struct _EXTPORTDATA * PORTVEC;

	hwndDlg = hDlg;			// Save Window Handle

	// Save a pointer to the DLGHDR structure.

	SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) pHdr);

	// Create the tab control.


	init.dwICC = ICC_STANDARD_CLASSES;
	init.dwSize=sizeof(init);
	i=InitCommonControlsEx(&init);

	pHdr->hwndTab = CreateWindow(WC_TABCONTROL, "", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
		0, 0, 100, 100, hwndDlg, NULL, hInstance, NULL);

	if (pHdr->hwndTab == NULL) {

	// handle error

	}

	// Add a tab for each of the child dialog boxes.

	tie.mask = TCIF_TEXT | TCIF_IMAGE;

	tie.iImage = -1;

	for (i = 1; i <= NUMBEROFPORTS; i++)
	{
		// Only allow UI on ax.25 ports

		PORTVEC = (struct _EXTPORTDATA * )GetPortTableEntryFromSlot(i);

		if (PORTVEC->PORTCONTROL.PORTTYPE == 16)		// EXTERNAL
			if (PORTVEC->PORTCONTROL.PROTOCOL == 10)	// Pactor/WINMOR
				if (PORTVEC->PORTCONTROL.UICAPABLE == 0)
					continue;

		wsprintf(PortNo, "Port %2d", GetPortNumber(i));
		PortNum[tab] = i;

		tie.pszText = PortNo;
		TabCtrl_InsertItem(pHdr->hwndTab, tab, &tie);
	
		pHdr->apRes[tab++] = DoLockDlgRes("PORTPAGE");
	}

	PageCount = tab;

	// Determine the bounding rectangle for all child dialog boxes.

	SetRectEmpty(&rcTab);

	for (i = 0; i < PageCount; i++)
	{
		if (pHdr->apRes[i]->cx > rcTab.right)
			rcTab.right = pHdr->apRes[i]->cx;

		if (pHdr->apRes[i]->cy > rcTab.bottom)
			rcTab.bottom = pHdr->apRes[i]->cy;

	}

	MapDialogRect(hwndDlg, &rcTab);

//	rcTab.right = rcTab.right * LOWORD(dwDlgBase) / 4;

//	rcTab.bottom = rcTab.bottom * HIWORD(dwDlgBase) / 8;

	// Calculate how large to make the tab control, so

	// the display area can accomodate all the child dialog boxes.

	TabCtrl_AdjustRect(pHdr->hwndTab, TRUE, &rcTab);

	OffsetRect(&rcTab, cxMargin - rcTab.left, cyMargin - rcTab.top);

	// Calculate the display rectangle.

	CopyRect(&pHdr->rcDisplay, &rcTab);

	TabCtrl_AdjustRect(pHdr->hwndTab, FALSE, &pHdr->rcDisplay);

	// Set the size and position of the tab control, buttons,

	// and dialog box.

	SetWindowPos(pHdr->hwndTab, NULL, rcTab.left, rcTab.top, rcTab.right - rcTab.left, rcTab.bottom - rcTab.top, SWP_NOZORDER);

	// Move the Buttons to bottom of page

	pos=rcTab.left+cxMargin;

	
	// Size the dialog box.

	SetWindowPos(hwndDlg, NULL, 0, 0, rcTab.right + cyMargin + 2 * GetSystemMetrics(SM_CXDLGFRAME),
		rcTab.bottom  + 2 * cyMargin + 2 * GetSystemMetrics(SM_CYDLGFRAME) + GetSystemMetrics(SM_CYCAPTION),
		SWP_NOMOVE | SWP_NOZORDER);

	// Simulate selection of the first item.

	OnSelChanged(hwndDlg);

}

// DoLockDlgRes - loads and locks a dialog template resource.

// Returns a pointer to the locked resource.

// lpszResName - name of the resource

DLGTEMPLATE * WINAPI DoLockDlgRes(LPCSTR lpszResName)
{
	HRSRC hrsrc = FindResource(hInstance, lpszResName, RT_DIALOG);
	HGLOBAL hglb = LoadResource(hInstance, hrsrc);

	return (DLGTEMPLATE *) LockResource(hglb);
}

//The following function processes the TCN_SELCHANGE notification message for the main dialog box. The function destroys the dialog box for the outgoing page, if any. Then it uses the CreateDialogIndirect function to create a modeless dialog box for the incoming page.

// OnSelChanged - processes the TCN_SELCHANGE notification.

// hwndDlg - handle of the parent dialog box

VOID WINAPI OnSelChanged(HWND hwndDlg)
{
	char PortDesc[40];
	int Port;

	DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndDlg, GWL_USERDATA);

	CurrentPage = TabCtrl_GetCurSel(pHdr->hwndTab);

	// Destroy the current child dialog box, if any.

	if (pHdr->hwndDisplay != NULL)

		DestroyWindow(pHdr->hwndDisplay);

	// Create the new child dialog box.

	pHdr->hwndDisplay = CreateDialogIndirect(hInstance, pHdr->apRes[CurrentPage], hwndDlg, ChildDialogProc);

	hwndDisplay = pHdr->hwndDisplay;		// Save

	Port = PortNum[CurrentPage];
	// Fill in the controls

	GetPortDescription(PortNum[CurrentPage], PortDesc);

	SetDlgItemText(hwndDisplay, IDC_PORTNAME, PortDesc);

	CheckDlgButton(hwndDisplay, IDC_FROMFILE, SendFromFile[Port]);

	SetDlgItemInt(hwndDisplay, IDC_INTERVAL, Interval[Port], FALSE);

	SetDlgItemText(hwndDisplay, IDC_UIDEST, &UIUIDEST[Port][0]);
	SetDlgItemText(hwndDisplay, IDC_UIDIGIS, UIUIDigi[Port]);



	SetDlgItemText(hwndDisplay, IDC_FILENAME, &FN[Port][0]);
	SetDlgItemText(hwndDisplay, IDC_MESSAGE, &Message[Port][0]);

	ShowWindow(pHdr->hwndDisplay, SW_SHOWNORMAL);

}


//The following function processes the WM_INITDIALOG message for each of the child dialog boxes. You cannot specify the position of a dialog box created using the CreateDialogIndirect function. This function uses the SetWindowPos function to position the child dialog within the tab control's display area.

// OnChildDialogInit - Positions the child dialog box to fall

// within the display area of the tab control.

VOID WINAPI OnChildDialogInit(HWND hwndDlg)
{
	HWND hwndParent = GetParent(hwndDlg);
	DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);

	SetWindowPos(hwndDlg, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE);
}



LRESULT CALLBACK UIWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	HKEY hKey=0;

	switch (message) { 

	case WM_INITDIALOG:
		OnTabbedDialogInit(hWnd);
		return (INT_PTR)TRUE;

	case WM_NOTIFY:

        switch (((LPNMHDR)lParam)->code)
        {
		case TCN_SELCHANGE:
			 OnSelChanged(hWnd);
				 return TRUE;
         // More cases on WM_NOTIFY switch.
		case NM_CHAR:
			return TRUE;
        }

       break;


		case WM_CTLCOLORDLG:
			return (LONG)bgBrush;

		case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = (HDC)wParam;
			SetTextColor(hdcStatic, RGB(0, 0, 0));
			SetBkMode(hdcStatic, TRANSPARENT);

			return (LONG)bgBrush;
		}

		case WM_COMMAND:

		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

		switch (wmId) {

		case IDOK:

			return TRUE;

		default:

			return 0;
		}


		case WM_SYSCOMMAND:

		wmId    = LOWORD(wParam); // Remember, these are...
		wmEvent = HIWORD(wParam); // ...different for Win32!

		switch (wmId)
		{
		case SC_RESTORE:

			return (DefWindowProc(hWnd, message, wParam, lParam));

		case  SC_MINIMIZE: 
			
			if (MinimizetoTray)
				return ShowWindow(hWnd, SW_HIDE);
			else
				return (DefWindowProc(hWnd, message, wParam, lParam));
						
			break;
		
		default:
				return (DefWindowProc(hWnd, message, wParam, lParam));
		}

		case WM_CLOSE:
			return(DestroyWindow(hWnd));

		default:
			return (DefWindowProc(hWnd, message, wParam, lParam));

	}

	return (0);
}

#endif

