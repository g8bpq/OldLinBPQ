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
//	Runs FLARQ-like protocol over UI Packets
//

#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include "CHeaders.h"

int (WINAPI FAR *GetModuleFileNameExPtr)();
int (WINAPI FAR *EnumProcessesPtr)();


#include <stdio.h>
#include <time.h>

#include "tncinfo.h"

#include "bpq32.h"

#define MAXARQ 10

#define DLE 0x10
#define SOH 1
#define STX 2
#define EOT 4

#define FEND 0xC0 
#define FESC 0xDB
#define TFEND 0xDC
#define TFESC 0xDD

#define TIMESTAMP 352

#define CONTIMEOUT 1200

#define AGWHDDRLEN sizeof(struct AGWHEADER)

unsigned long _beginthread( void( *start_address )(), unsigned stack_size, int arglist);

extern int (WINAPI FAR *GetModuleFileNameExPtr)();

//int ResetExtDriver(int num);
extern char * PortConfig[33];
int SemHeldByAPI;

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

static int ProcessReceivedData(int bpqport);
static ProcessLine(char * buf, int Port);
static VOID ProcessFLDigiPacket(struct TNCINFO * TNC, char * Message, int Len);
VOID ProcessFLDigiKISSPacket(struct TNCINFO * TNC, char * Message, int Len);
struct TNCINFO * GetSessionKey(char * key, struct TNCINFO * TNC);
static VOID SendARQData(struct TNCINFO * TNC, UINT * Buffer, int Stream);
VOID SendRPBeacon(struct TNCINFO * TNC);
unsigned int CalcCRC(UCHAR * ptr, int Len);
static VOID ARQTimer(struct TNCINFO * TNC);
VOID QueueAndSend(struct TNCINFO * TNC, struct ARQINFO * ARQ, SOCKET sock, char * Msg, int MsgLen);
VOID SaveAndSend(struct TNCINFO * TNC, struct ARQINFO * ARQ, SOCKET sock, char * Msg, int MsgLen);
static VOID ProcessARQStatus(struct TNCINFO * TNC, int Stream, struct ARQINFO * ARQ, char *Input);
VOID CheckFLDigiData(struct TNCINFO * TNC);
static VOID SendPacket(struct TNCINFO * TNC, struct STREAMINFO * STREAM, UCHAR * Msg, int MsgLen);
int	KissEncode(UCHAR * inbuff, UCHAR * outbuff, int len);
VOID Send_AX_Datagram(PDIGIMESSAGE Block, DWORD Len, UCHAR Port);
int DoScanLine(struct TNCINFO * TNC, char * Buff, int Len);
VOID ProcessARQPacket(struct PORTCONTROL * PORT, MESSAGE * Buffer);
char * strlop(char * buf, char delim);

extern UCHAR BPQDirectory[];
extern char MYALIASLOPPED[10];


#define MAXBPQPORTS 32
#define MAXMPSKPORTS 16

static BOOL GotMsg;

static HANDLE STDOUT=0;

extern int Blocksizes[10];

static char WindowTitle[] = "UIARQ";
static char ClassName[] = "UIARQSTATUS";
static int RigControlRow = 165;



static int ExtProc(int fn, int port,unsigned char * buff)
{
	UINT * buffptr;
	unsigned int txlen=0;
	struct TNCINFO * TNC = TNCInfo[port];
	int Stream = 0;
	struct STREAMINFO * STREAM;

	if (TNC == NULL)
		return 0;					// Port not defined

	// Look for attach on any call

	for (Stream = 0; Stream <= MAXARQ; Stream++)
	{
		STREAM = &TNC->Streams[Stream];
	
		if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] && TNC->Streams[Stream].Attached == 0)
		{
			// New Attach

			int calllen;
			STREAM->Attached = TRUE;

			calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4USER, STREAM->MyCall);
			STREAM->MyCall[calllen] = 0;
			STREAM->FramesOutstanding = 0;

			SetWindowText(STREAM->xIDC_STATUS, "Attached");
			SetWindowText(STREAM->xIDC_MYCALL, STREAM->MyCall);
	
		}
	}

	switch (fn)
	{
	case 7:			

		// 100 mS Timer. 

		for (Stream = 0; Stream <= MAXARQ; Stream++)
		{
			STREAM = &TNC->Streams[Stream];
	
			if (STREAM->NeedDisc)
			{
				STREAM->NeedDisc--;

				if (STREAM->NeedDisc == 0)
				{
					// Send the DISCONNECT

					TidyClose(TNC, Stream);
				}
			}
		}
	
		ARQTimer(TNC);
	
		return 0;

	case 1:				// poll

		// See if any frames for this port

		for (Stream = 0; Stream <= MAXARQ; Stream++)
		{
			STREAM = &TNC->Streams[Stream];
			
			if (STREAM->Attached)
				CheckForDetach(TNC, Stream, STREAM, TidyClose, ForcedClose, CloseComplete);

			if (STREAM->ReportDISC)
			{
				STREAM->ReportDISC = FALSE;
				buff[4] = Stream;

				return -1;
			}
	
			if (STREAM->PACTORtoBPQ_Q == 0)
			{
				if (STREAM->DiscWhenAllSent)
				{
					STREAM->DiscWhenAllSent--;
					if (STREAM->DiscWhenAllSent == 0)
						STREAM->ReportDISC = TRUE;				// Dont want to leave session attached. Causes too much confusion
				}
			}
			else
			{
				int datalen;
			
				buffptr=Q_REM(&STREAM->PACTORtoBPQ_Q);

				datalen=buffptr[1];

				buff[4] = Stream;
				buff[7] = 0xf0;
				memcpy(&buff[8],buffptr+2,datalen);		// Data goes to +7, but we have an extra byte
				datalen+=8;
				
				PutLengthinBuffer(buff, datalen);
		
				ReleaseBuffer(buffptr);
	
				return (1);
			}
		}

		if (TNC->PortRecord->UI_Q)
		{
			struct _MESSAGE * buffptr;
			int SendLen;
			char Reply[256];
			int UILen;
			char * UIMsg;

			buffptr = Q_REM(&TNC->PortRecord->UI_Q);

			UILen = buffptr->LENGTH;
			UILen -= 23;
			UIMsg = buffptr->L2DATA;

			UIMsg[UILen] = 0;

			if (UILen < 129 && STREAM->Attached == FALSE)			// Be sensible!
			{
				// >00uG8BPQ:72 TestA
				SendLen = sprintf(Reply, "u%s:72 %s", TNC->NodeCall, UIMsg);
				SendPacket(TNC, STREAM, Reply, SendLen);
			}
			ReleaseBuffer(buffptr);
		}
			
		return (0);

	case 2:				// send

	
		Stream = buff[4];
		
		STREAM = &TNC->Streams[Stream]; 

//		txlen=(buff[6]<<8) + buff[5] - 8;	

		txlen = GetLengthfromBuffer(buff) - 8;
				
		if (STREAM->Connected)
		{
			buffptr = GetBuff();

			if (buffptr == 0) return (0);			// No buffers, so ignore
		
			buffptr[1] = txlen;
			memcpy(buffptr+2, &buff[8], txlen);
		
			C_Q_ADD(&TNC->Streams[Stream].BPQtoPACTOR_Q, buffptr);

			return (0);
		}
		else
		{
			buff[8 + txlen] = 0;
			_strupr(&buff[8]);

			if (_memicmp(&buff[8], "D\r", 2) == 0)
			{
				if (STREAM->Connected)
					TidyClose(TNC, buff[4]);

				STREAM->ReportDISC = TRUE;		// Tell Node
				return 0;
			}

			// See if a Connect Command.

			if (toupper(buff[8]) == 'C' && buff[9] == ' ' && txlen > 2)	// Connect
			{
				char * ptr;
				char * context;
				struct ARQINFO * ARQ = STREAM->ARQInfo;
				int SendLen;
				char Reply[80];

				_strupr(&buff[8]);
				buff[8 + txlen] = 0;

				memset(ARQ, 0, sizeof(struct ARQINFO));		// Reset ARQ State
				ARQ->TXSeq = ARQ->TXLastACK = 63;			// Last Sent
				ARQ->RXHighest = ARQ->RXNoGaps = 63;		// Last Received
				ARQ->OurStream = Stream + 64;
				ARQ->FarStream = 64;						// Not yet defined

				memset(STREAM->RemoteCall, 0, 10);

				ptr = strtok_s(&buff[10], " ,\r", &context);
				strcpy(STREAM->RemoteCall, ptr);

//<SOH>00cG8BPQ:1025 G8BPQ:24 0 7 T60R5W10FA36<EOT>

				SendLen = sprintf(Reply, "c%s:42 %s:24 %c 7 T60R5W10",
					STREAM->MyCall, STREAM->RemoteCall, ARQ->OurStream); 

				ARQ->ARQState = ARQ_ACTIVE;

				ARQ->ARQTimerState = ARQ_CONNECTING;
				SaveAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);

				SetWindowText(STREAM->xIDC_STATUS, "Connecting");
				SetWindowText(STREAM->xIDC_MYCALL, STREAM->MyCall);
				SetWindowText(STREAM->xIDC_DESTCALL, STREAM->RemoteCall);
				SetWindowText(STREAM->xIDC_DIRN, "Out");

				STREAM->Connecting = TRUE;	

				return 0;
			}
		}
		return (0);

	case 3:	

		Stream = (int)buff;

		STREAM = &TNC->Streams[Stream];
		{
			// Busy if TX Window reached

			struct ARQINFO * ARQ = STREAM->ARQInfo;
			int Outstanding;

			Outstanding = ARQ->TXSeq - ARQ->TXLastACK;

			if (Outstanding < 0)
				Outstanding += 64;

			TNC->PortRecord->FramesQueued = Outstanding + C_Q_COUNT(&STREAM->BPQtoPACTOR_Q);		// Save for Appl Level Queued Frames

			if (Outstanding > ARQ->TXWindow)
				return (1 | 1 << 8 | STREAM->Disconnecting << 15); // 3rd Nibble is frames unacked
			else
				return 1 << 8 | STREAM->Disconnecting << 15;

		}
		return 1 << 8 | STREAM->Disconnecting << 15;		// OK, but lock attach if disconnecting
	
	case 4:				// reinit

		return (0);

	case 5:				// Close

		return 0;
	}

	return 0;
}

VOID UIHook(struct _LINKTABLE * LINK, struct PORTCONTROL * PORT, MESSAGE * Buffer, MESSAGE * ADJBUFFER, UCHAR CTL, UCHAR MSGFLAG)
{
	PORT = GetPortTableEntryFromPortNum(PORT->HookPort->PORTNUMBER);
	if (PORT)
		ProcessARQPacket(PORT, Buffer);
}
static VOID UpdateStatsLine(struct TNCINFO * TNC, struct STREAMINFO * STREAM)
{
	char Count[16];
	
	sprintf(Count, "%d", STREAM->BytesRXed);
	SetWindowText(STREAM->xIDC_RXED, Count);

	sprintf(Count, "%d", STREAM->BytesTXed);
	SetWindowText(STREAM->xIDC_SEND, Count);
	
	sprintf(Count, "%d", STREAM->BytesResent);
	SetWindowText(STREAM->xIDC_RESENT, Count);

	sprintf(Count, "%d", STREAM->BytesAcked);
	SetWindowText(STREAM->xIDC_ACKED, Count);
}

static int WebProc(struct TNCINFO * TNC, char * Buff, BOOL LOCAL)
{
	int Len = sprintf(Buff, "<html><meta http-equiv=expires content=0><meta http-equiv=refresh content=15>"
		"<script type=\"text/javascript\">\r\n"
		"function ScrollOutput()\r\n"
		"{var textarea = document.getElementById('textarea');"
		"textarea.scrollTop = textarea.scrollHeight;}</script>"
		"</head><title>FLDigi Status</title></head><body id=Text onload=\"ScrollOutput()\">"
		"<h2>FLDIGI Status</h2>");

	Len += sprintf(&Buff[Len], "<table style=\"text-align: left; width: 500px; font-family: monospace; align=center \" border=1 cellpadding=2 cellspacing=2>");

	Len += sprintf(&Buff[Len], "<tr><td width=110px>Comms State</td><td>%s</td></tr>", TNC->WEB_COMMSSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>TNC State</td><td>%s</td></tr>", TNC->WEB_TNCSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>Mode</td><td>%s</td></tr>", TNC->WEB_MODE);
	Len += sprintf(&Buff[Len], "<tr><td>Channel State</td><td>%s</td></tr>", TNC->WEB_CHANSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>Proto State</td><td>%s</td></tr>", TNC->WEB_PROTOSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>Traffic</td><td>%s</td></tr>", TNC->WEB_TRAFFIC);
//	Len += sprintf(&Buff[Len], "<tr><td>TNC Restarts</td><td></td></tr>", TNC->WEB_RESTARTS);
	Len += sprintf(&Buff[Len], "</table>");

	Len += sprintf(&Buff[Len], "<textarea rows=10 style=\"width:500px; height:250px;\" id=textarea >%s</textarea>", TNC->WebBuffer);
	Len = DoScanLine(TNC, Buff, Len);

	return Len;
}

UINT UIARQExtInit(EXTPORTDATA * PortEntry)
{
	int i, port;
	char Msg[255];
	struct TNCINFO * TNC;
	char * ptr;
	struct PORTCONTROL * PORT;
	struct STREAMINFO * STREAM;

	srand((unsigned int)time(NULL));

	port = PortEntry->PORTCONTROL.PORTNUMBER;

	ReadConfigFile(port, ProcessLine);

	TNC = TNCInfo[port];

	if (TNC == NULL)
	{
		// Not defined in Config file

		sprintf(Msg," ** Error - no info in BPQ32.cfg for this port\n");
		WritetoConsole(Msg);

		return (int) ExtProc;
	}

	for (i = 0; i <MAXARQ; i++)
	{
		TNC->Streams[i].ARQInfo = zalloc(sizeof(struct ARQINFO)); 
	}

	TNC->Port = port;

	TNC->PortRecord = PortEntry;

	if (PortEntry->PORTCONTROL.PORTCALL[0] == 0)
		memcpy(TNC->NodeCall, MYNODECALL, 10);
	else
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);

	TNC->Interlock = PortEntry->PORTCONTROL.PORTINTERLOCK;


	PortEntry->PORTCONTROL.PROTOCOL = 10;
	PortEntry->PERMITGATEWAY = TRUE;					// Can change ax.25 call on each stream
	PortEntry->PORTCONTROL.UICAPABLE = 1;				// Can send beacons
	PortEntry->PORTCONTROL.PORTQUALITY = 0;
	PortEntry->SCANCAPABILITIES = NONE;					// Scan Control - None

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0 || PortEntry->PORTCONTROL.PORTPACLEN > 128)
		PortEntry->PORTCONTROL.PORTPACLEN = 64;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	TNC->Hardware = H_UIARQ;

	if (TNC->BusyWait == 0)
		TNC->BusyWait = 10;
	
	PortEntry->MAXHOSTMODESESSIONS = MAXARQ;

	i = 0;

	while (TNC->ARQPorts[i])
	{
		PORT = GetPortTableEntryFromPortNum(TNC->ARQPorts[i]);
		PORT->UIHook = (FARPROCY)UIHook;
		PORT->HookPort = (struct PORTCONTROL *)PortEntry;
		i++;
	}

	TNC->WEB_MODE = zalloc(50);
	TNC->WEB_TRAFFIC = zalloc(100);

	TNC->WebWindowProc = WebProc;
	TNC->WebWinX = 520;
	TNC->WebWinY = 500;
	TNC->WebBuffer = zalloc(5000);

	TNC->WEB_COMMSSTATE = zalloc(100);
	TNC->WEB_TNCSTATE = zalloc(100);
	TNC->WEB_CHANSTATE = zalloc(100);
	TNC->WEB_BUFFERS = zalloc(100);
	TNC->WEB_PROTOSTATE = zalloc(100);
	TNC->WEB_RESTARTTIME = zalloc(100);
	TNC->WEB_RESTARTS = zalloc(100);

	TNC->WEB_MODE = zalloc(50);
	TNC->WEB_TRAFFIC = zalloc(100);


#ifndef LINBPQ

	CreatePactorWindow(TNC, ClassName, WindowTitle, RigControlRow, PacWndProc, 560, 350);

	CreateWindowEx(WS_EX_STATICEDGE, "STATIC", " MyCall", WS_CHILD | WS_VISIBLE, 5,6,79,20, TNC->hDlg, NULL, hInstance, NULL);
	CreateWindowEx(WS_EX_STATICEDGE, "STATIC", " DestCall", WS_CHILD | WS_VISIBLE, 85,6,79,20, TNC->hDlg, NULL, hInstance, NULL);
	CreateWindowEx(WS_EX_STATICEDGE, "STATIC", " Status", WS_CHILD | WS_VISIBLE, 165,6,84,20, TNC->hDlg, NULL, hInstance, NULL);
	CreateWindowEx(WS_EX_STATICEDGE, "STATIC", " Sent", WS_CHILD | WS_VISIBLE, 250,6,59,20, TNC->hDlg, NULL, hInstance, NULL);
	CreateWindowEx(WS_EX_STATICEDGE, "STATIC", " Rxed", WS_CHILD | WS_VISIBLE, 310,6,59,20, TNC->hDlg, NULL, hInstance, NULL);
	CreateWindowEx(WS_EX_STATICEDGE, "STATIC", " Resent", WS_CHILD | WS_VISIBLE, 370,6,59,20, TNC->hDlg, NULL, hInstance, NULL);
	CreateWindowEx(WS_EX_STATICEDGE, "STATIC", " Acked", WS_CHILD | WS_VISIBLE, 430,6,59,20, TNC->hDlg, NULL, hInstance, NULL);
	CreateWindowEx(WS_EX_STATICEDGE, "STATIC", " Dirn", WS_CHILD | WS_VISIBLE, 490,6,49,20, TNC->hDlg, NULL, hInstance, NULL);

	for (i = 0; i <MAXARQ; i++)
	{
		STREAM = &TNC->Streams[i];

		STREAM->xIDC_MYCALL = CreateWindowEx(WS_EX_CLIENTEDGE, "STATIC", "", WS_CHILD | WS_VISIBLE, 5, 26 + i*20, 79, 20, TNC->hDlg, NULL, hInstance, NULL);
		STREAM->xIDC_DESTCALL = CreateWindowEx(WS_EX_CLIENTEDGE, "STATIC", "", WS_CHILD | WS_VISIBLE,  85, 26 + i*20, 79, 20, TNC->hDlg, NULL, hInstance, NULL);
		STREAM->xIDC_STATUS = CreateWindowEx(WS_EX_CLIENTEDGE, "STATIC", "", WS_CHILD | WS_VISIBLE,  165, 26 + i*20, 84, 20, TNC->hDlg, NULL, hInstance, NULL);
		STREAM->xIDC_SEND = CreateWindowEx(WS_EX_CLIENTEDGE, "STATIC", "", WS_CHILD | WS_VISIBLE,  250, 26 + i*20, 59, 20, TNC->hDlg, NULL, hInstance, NULL);
		STREAM->xIDC_RXED = CreateWindowEx(WS_EX_CLIENTEDGE, "STATIC", "", WS_CHILD | WS_VISIBLE,  310, 26 + i*20, 59, 20, TNC->hDlg, NULL, hInstance, NULL);
		STREAM->xIDC_RESENT = CreateWindowEx(WS_EX_CLIENTEDGE, "STATIC", "", WS_CHILD | WS_VISIBLE,  370, 26 + i*20, 59, 20, TNC->hDlg, NULL, hInstance, NULL);
		STREAM->xIDC_ACKED = CreateWindowEx(WS_EX_CLIENTEDGE, "STATIC", "", WS_CHILD | WS_VISIBLE,  430, 26 + i*20, 59, 20, TNC->hDlg, NULL, hInstance, NULL);
		STREAM->xIDC_DIRN = CreateWindowEx(WS_EX_CLIENTEDGE, "STATIC", "", WS_CHILD | WS_VISIBLE,  490, 26 + i*20, 49, 20, TNC->hDlg, NULL, hInstance, NULL);
	}

	TNC->ClientHeight = 360;
	TNC->ClientWidth = 560;

	MoveWindows(TNC);

#endif

	i=sprintf(Msg,"UIARQ\n");
	WritetoConsole(Msg);

	return ((int) ExtProc);
}


static ProcessLine(char * buf, int Port)
{
	UCHAR * ptr;
	char * p_ipad = 0;
	int BPQport;
	int len=510;
	struct TNCINFO * TNC;

	char errbuf[256];

	strcpy(errbuf, buf);

	BPQport = Port;
	TNC = TNCInfo[BPQport] = zalloc(sizeof(struct TNCINFO));

	TNC->Timeout = 50;		// Default retry = 5 seconds
	TNC->Retries = 6;		// Default Retries
	TNC->Window = 16;

	TNC->InitScript = malloc(1000);
	TNC->InitScript[0] = 0;
	
	// Read Initialisation lines

	while(TRUE)
	{
		strcpy(errbuf, buf);

		if (memcmp(buf, "****", 4) == 0)
			return TRUE;

		ptr = strchr(buf, ';');
		if (ptr)
		{
			*ptr++ = 13;
			*ptr = 0;
		}

		if (_memicmp(buf, "TIMEOUT", 7) == 0)
			TNC->Timeout = atoi(&buf[8]) * 10;
		else if (_memicmp(buf, "RETRIES", 7) == 0)
			TNC->Retries = atoi(&buf[8]);
		else if (_memicmp(buf, "WINDOW", 6) == 0)
			TNC->Window = atoi(&buf[7]);
		else
		{
			char * ptr, * p_value, * p_port;
			int i;

			ptr = strtok(buf, "=\t\n\r");
			p_value = strtok(NULL, " \t\n\r");

			if (ptr == NULL) return (TRUE);

			if (*ptr =='#') return (TRUE);			// comment

			if (*ptr ==';') return (TRUE);			// comment

			if (_stricmp(ptr,"Ports") == 0)
			{
				int n = 0;
				
				p_port = strtok(p_value, " ,\t\n\r");
		
				while (p_port != NULL)
				{
					i = atoi(p_port);
					if (i == 0) return FALSE;
					if (i > NUMBEROFPORTS) return FALSE;

					TNC->ARQPorts[n++] = i;
					p_port = strtok(NULL, " ,\t\n\r");
				}
			}
		}
		if (GetLine(buf) == 0)
			return TRUE;

	}
	return FALSE;	
}
static VOID SendPacket(struct TNCINFO * TNC, struct STREAMINFO * STREAM, UCHAR * Msg, int MsgLen)
{
	DIGIMESSAGE Block = {0};
	int Port = TNC->ARQPorts[0];

	Block.CTL = 3;
	Block.PID = 0xF0;

	ConvToAX25(STREAM->RemoteCall, Block.DEST);
	memcpy(Block.ORIGIN, MYCALL, 7);

	Block.L2DATA[0] = STREAM->ARQInfo->FarStream;
	memcpy(&Block.L2DATA[1], Msg, MsgLen);
	MsgLen += 1;
	
	Send_AX_Datagram(&Block, MsgLen + 2, Port);		// Inulude CTL and PID
}

VOID ProcessFLDigiData(struct TNCINFO * TNC, UCHAR * Input, int Len, int Stream);

VOID ProcessARQPacket(struct PORTCONTROL * PORT, MESSAGE * Buffer)
{
	// ARQ Packet from KISS-Like Hardware

	struct TNCINFO * TNC = TNCInfo[PORT->PORTNUMBER];
	UCHAR * Input;
	int Len;
	int Stream = Buffer->L2DATA[0] - 64;

	if (Stream < 0 || Stream > MAXARQ)
		return;

	// First Bytes is Stream Number (as ASCII Letter)

	Input = &Buffer->L2DATA[1];
	Len = Buffer->LENGTH - 24;	
	
	ProcessFLDigiData(TNC, Input, Len, Stream);
}

/*

<SOH>00cG8BPQ:1025 G8BPQ:24 0 8 T60R6W108E06<EOT>
<SOH>00kG8BPQ:24 G8BPQ 4 85F9B<EOT>

<SOH>00cG8BPQ:1025 GM8BPQ:24 0 7 T60R5W1051D5<EOT> (128, 5)

,<SOH>00cG8BPQ:1025 G8BPQ:24 0 7 T60R5W10FA36<EOT>
<SOH>00kG8BPQ:24 G8BPQ 5 89FCA<EOT>

First no sees to be a connection counter. Next may be stream


<SOH>08s___ABFC<EOT>
<SOH>08tG8BPQ:73 xxx 33FA<EOT>
<SOH>00tG8BPQ:73 yyy 99A3<EOT>
<SOH>08dG8BPQ:90986C<EOT>
<SOH>00bG8BPQ:911207<EOT>

call:90 for dis 91 for dis ack 73<sp> for chat)

<SOH>08pG8BPQ<SUB>?__645E<EOT>
<SOH>00s_??4235<EOT>

<SOH>08pG8BPQ<SUB>?__645E<EOT>
<SOH>00s_??4235<EOT>

i Ident
c Connect
k Connect Ack
r Connect NAK
d Disconnect req
s Data Ack/ Retransmit Req )status)
p Poll
f Format Fail
b dis ack
t talk

a Abort
o Abort ACK


<SOH>00cG8BPQ:1025 G8BPQ:24 0 7 T60R5W10FA36<EOT>
<SOH>00kG8BPQ:24 G8BPQ 6 49A3A<EOT>
<SOH>08s___ABFC<EOT>
<SOH>08 ARQ:FILE::flarqmail-1.eml
ARQ:EMAIL::
ARQ:SIZE::90
ARQ::STX
//FLARQ COMPOSER
Date: 09/01/2014 23:24:42
To: gm8bpq
From: 
SubjectA0E0<SOH>
<SOH>08!: Test

Test Message

ARQ::ETX
F0F2<SOH>
<SOH>08pG8BPQ<SUB>!__623E<EOT>
<SOH>08pG8BPQ<SUB>!__623E<EOT>
<SOH>08pG8BPQ<SUB>!__623E<EOT>




*/
static VOID ProcessFLDigiData(struct TNCINFO * TNC, UCHAR * Input, int Len, int Stream)
{
	UINT * buffptr;
	struct STREAMINFO * STREAM = &TNC->Streams[Stream];
	char CTRL = Input[0];
	struct ARQINFO * ARQ = STREAM->ARQInfo;
	char Channel = Stream + 64;
	int SendLen;
	char Reply[80];

	Input[Len] = 0;

	// Process Message

	// This processes eitrher message from the KISS or RAW interfaces.
	//	Headers and RAW checksum have been removed, so packet starts with Control Byte

	// Only a connect request is allowed with no session, so check first

	if (CTRL == 'c')
	{
		// Connect Request

		char * call1;
		char * call2;
		char * port1;
		char * port2;
		char * ptr;
		char * context;
		char FarStream = 0;
		int BlockSize = 6;			// 64 default
		int Window = TNC->Window;		
		APPLCALLS * APPL;
		char * ApplPtr = APPLS;
		int App;
		char Appl[10];
		struct WL2KInfo * WL2K = TNC->WL2K;
		TRANSPORTENTRY * SESS;

		if (Stream)
			return;					// Shouldn't have Stream on Connect Request

		call1 = strtok_s(&Input[1], " ", &context);
		call2 = strtok_s(NULL, " ", &context);

		port1 = strlop(call1, ':');
		port2 = strlop(call2, ':');

		// See if for us

		for (App = 0; App < 32; App++)
		{
			APPL=&APPLCALLTABLE[App];
			memcpy(Appl, APPL->APPLCALL_TEXT, 10);
			ptr=strchr(Appl, ' ');

			if (ptr) *ptr = 0;
	
			if (_stricmp(call2, Appl) == 0)
					break;

			memcpy(Appl, APPL->APPLALIAS_TEXT, 10);
			ptr=strchr(Appl, ' ');

			if (ptr) *ptr = 0;
	
			if (_stricmp(call2, Appl) == 0)
					break;

		}

		if (App > 31)
			if (strcmp(TNC->NodeCall, call2) !=0)
				if (strcmp(call2, MYALIASLOPPED) !=0)
					return;				// Not Appl or Port/Node Call

		ptr =  strtok_s(NULL, " ", &context);
		FarStream = *ptr;
		ptr =  strtok_s(NULL, " ", &context);
		BlockSize = atoi(ptr);

		if (ARQ->ARQState > ARQ_CONNECTING)
		{
			// We have already received a connect request - just ACK it

			goto AckConnectRequest;
		}

		// Get a Session

		Stream = 1;

		while(Stream <= MAXARQ)
		{
			if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] == 0)
				goto GotStream;

			Stream++;
		}

		// No free streams - send Disconnect

		return;

	GotStream:

		STREAM = &TNC->Streams[Stream];

		ProcessIncommingConnect(TNC, call1, Stream, FALSE);
				
		SESS = TNC->PortRecord->ATTACHEDSESSIONS[Stream];

		strcpy(STREAM->MyCall, call2);
		STREAM->ConnectTime = time(NULL); 
		STREAM->BytesRXed = STREAM->BytesTXed = STREAM->BytesAcked = STREAM->BytesResent = 0;
		
		if (WL2K)
			strcpy(SESS->RMSCall, WL2K->RMSCall);

		ARQ = STREAM->ARQInfo;

		memset(ARQ, 0, sizeof(struct ARQINFO));		// Reset ARQ State
		ARQ->FarStream = FarStream;
		ARQ->TXSeq = ARQ->TXLastACK = 63;			// Last Sent
		ARQ->RXHighest = ARQ->RXNoGaps = 63;		// Last Received
		ARQ->ARQState = ARQ_ACTIVE;
		ARQ->OurStream = Stream + 64;

		STREAM->NeedDisc = 0;

		if (App < 32)
		{
			char AppName[13];

			memcpy(AppName, &ApplPtr[App * sizeof(CMDX)], 12);
			AppName[12] = 0;

			// Make sure app is available

			if (CheckAppl(TNC, AppName))
			{
				char Buffer[32];
				int MsgLen = sprintf(Buffer, "%s\r", AppName);

				buffptr = GetBuff();

				if (buffptr == 0)
				{
					return;			// No buffers, so ignore
				}

				buffptr[1] = MsgLen;
				memcpy(buffptr+2, Buffer, MsgLen);

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

				TNC->SwallowSignon = TRUE;

				// Save Appl Call in case needed for 

			}
			else
			{	
				STREAM->NeedDisc = 50;	// 1 sec
			}
		}
	
		ARQ->TXWindow = Window;

		if (BlockSize < 4)  BlockSize = 4;
		if (BlockSize < 9)  BlockSize = 9;

		ARQ->MaxBlock = Blocksizes[BlockSize];


		ARQ->ARQTimer = 1;			// To force CTEXT to be Queued
		
		if (App == 32)
		{
			// Connect to Node - send CTEXT

			if (HFCTEXTLEN > 1)
			{
				buffptr = GetBuff();
				if (buffptr)
				{
					buffptr[1] = HFCTEXTLEN;
					memcpy(&buffptr[2], HFCTEXT, HFCTEXTLEN);
					SendARQData(TNC, buffptr, Stream);
				}
			}
		}

		if (STREAM->NeedDisc)
		{
			// Send Not Avail 

			buffptr = GetBuff();
			if (buffptr)
			{
				buffptr[1] = sprintf((char *)&buffptr[2], "Application Not Available\r"); 
				SendARQData(TNC, buffptr, Stream);
			}
		}

		SetWindowText(STREAM->xIDC_MYCALL, STREAM->MyCall);
		SetWindowText(STREAM->xIDC_DESTCALL, STREAM->RemoteCall);
		SetWindowText(STREAM->xIDC_STATUS, "ConPending");
		SetWindowText(STREAM->xIDC_DIRN, "In");


AckConnectRequest:

		SendLen = sprintf(Reply, "k%s:24 %s %c 7", call2, call1, ARQ->OurStream); 

		SaveAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);
		ARQ->ARQTimerState = ARQ_CONNECTACK;

		return;
	}

	// All others need a session

//	if (!STREAM->Connected && !STREAM->Connecting)
//		return;

	if (CTRL == 'k')
	{
		// Connect ACK

		char * call1;
		char * call2;
		char * port1;
		char * port2;
		char * ptr;
		char * context;
		char FarStream = 0;
		int BlockSize = 6;			// 64 default
		int Window = 16;
		
		char Reply[80];
		int ReplyLen;

		call1 = strtok_s(&Input[1], " ", &context);
		call2 = strtok_s(NULL, " ", &context);

		port1 = strlop(call1, ':');
		port2 = strlop(call2, ':');

		if (strcmp(call1, STREAM->RemoteCall) != 0)
			return;

		if (Channel != ARQ->OurStream)
			return;					// Wrong Session

		ptr =  strtok_s(NULL, " ", &context);
		if (ptr)
			FarStream = *ptr;
		ptr =  strtok_s(NULL, " ", &context);
		if (ptr)
			BlockSize = atoi(ptr);

		if (STREAM->Connected)
			goto SendKReply;		// Repeated ACK

		STREAM->ConnectTime = time(NULL); 
		STREAM->BytesRXed = STREAM->BytesTXed = STREAM->BytesAcked = STREAM->BytesResent = 0;
		STREAM->Connected = TRUE;

		ARQ->ARQTimerState = 0;
		ARQ->ARQTimer = 0;

		sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound", STREAM->MyCall, STREAM->RemoteCall);
			
		SetWindowText(STREAM->xIDC_MYCALL, STREAM->MyCall);
		SetWindowText(STREAM->xIDC_DESTCALL, STREAM->RemoteCall);
		SetWindowText(STREAM->xIDC_DIRN, "Out");

		UpdateMH(TNC, STREAM->RemoteCall, '+', 'Z');
			
		ARQ->ARQTimerState = 0;
		ARQ->FarStream = FarStream;
		ARQ->TXWindow = TNC->Window;
		ARQ->MaxBlock = Blocksizes[BlockSize];

		ARQ->ARQState = ARQ_ACTIVE;

		STREAM->NeedDisc = 0;

		buffptr = GetBuff();

		if (buffptr)
		{
			ReplyLen = sprintf(Reply, "*** Connected to %s\r", STREAM->RemoteCall);

			buffptr[1] = ReplyLen;
			memcpy(buffptr+2, Reply, ReplyLen);

			C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
		}

		strcpy(TNC->WEB_PROTOSTATE, "Connected");
		SetWindowText(STREAM->xIDC_STATUS, "Connected");

SendKReply:

		// Reply with status

		SendLen = sprintf(Reply, "s%c%c%c", ARQ->TXSeq + 32, ARQ->RXNoGaps + 32, ARQ->RXHighest + 32);

		if (ARQ->RXHighest != ARQ->RXNoGaps)
		{
			int n = ARQ->RXNoGaps + 1;
			n &= 63;

			while (n != ARQ->RXHighest)
			{
				if (ARQ->RXHOLDQ[n] == 0)		// Dont have it
					SendLen += sprintf(&Reply[SendLen], "%c", n + 32);

				n++;
				n &= 63;
			}
		}

		QueueAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);
		return;
	}

	// All others need a session

	//if (!STREAM->Connected)
	//	return;


	if (CTRL == 's')
	{
		// Status

		if (Channel != ARQ->OurStream)
			return;					// Wrong Session

		ARQ->ARQTimer = 0;			// Stop retry timer
		Input[Len] = 0;
		ProcessARQStatus(TNC, Stream, ARQ, &Input[1]);

		return;
	}

	if (CTRL == 'p')
	{
		// Poll

		char * call1;
		char * context;

		call1 = strtok_s(&Input[1], " \x1A", &context);

		if (strcmp(call1, STREAM->RemoteCall) != 0)
			return;

		if (Channel != ARQ->OurStream)
			return;					// Wrong Session

		Debugprintf("Sending Poll Resp TX NOGaps High %d %d %d", ARQ->TXSeq, ARQ->RXNoGaps, ARQ->RXHighest);

		SendLen = sprintf(Reply, "s%c%c%c", ARQ->TXSeq + 32, ARQ->RXNoGaps + 32, ARQ->RXHighest + 32);

		if (ARQ->RXHighest != ARQ->RXNoGaps)
		{
			int n = ARQ->RXNoGaps + 1;
			n &= 63;

			while (n != ARQ->RXHighest)
			{
				if (ARQ->RXHOLDQ[n] == 0)		// Dont have it
					SendLen += sprintf(&Reply[SendLen], "%c", n + 32);

				n++;
				n &= 63;
			}
		}
		else
			ARQ->TurnroundTimer = 15;			// Allow us to send it all acked

		QueueAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);

		return;
	}


	if (CTRL == 'a')
	{
		// Abort. Send Abort ACK - same as 

		char * call1;
		char * context;

		call1 = strtok_s(&Input[1], " :", &context);

		if (strcmp(call1, STREAM->RemoteCall) != 0)
			return;

		if (Channel != ARQ->OurStream)
			return;					// Wrong Session

		SendLen = sprintf(Reply, "o%c%c%c", ARQ->TXSeq + 32, ARQ->RXNoGaps + 32, ARQ->RXHighest + 32);

		if (ARQ->RXHighest != ARQ->RXNoGaps)
		{
			int n = ARQ->RXNoGaps + 1;
			n &= 63;

			while (n != ARQ->RXHighest)
			{
				if (ARQ->RXHOLDQ[n] == 0)		// Dont have it
					SendLen += sprintf(&Reply[SendLen], "%c", n + 32);

				n++;
				n &= 63;
			}
		}

		QueueAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);
		return;
	}

	if (CTRL == 'i')
	{
		// Ident

		return;
	}

	if (CTRL == 't')
	{
		// Talk - not sure what to do with these

		return;
	}

	if (CTRL == 'd')
	{
		// Disconnect Request

		char * call1;
		char * context;

		call1 = strtok_s(&Input[1], " ", &context);
		strlop(call1, ':');

		if (strcmp(STREAM->RemoteCall, call1))
			return;

		if (Channel != ARQ->OurStream)
			return;					// Wrong Session


		// As the Disc ACK isn't repeated, we have to clear session now

		STREAM->Connected = FALSE;
		STREAM->Connecting = FALSE;
		STREAM->ReportDISC = TRUE;

		strcpy(TNC->WEB_PROTOSTATE, "Disconncted");

		SetWindowText(STREAM->xIDC_MYCALL, "");
		SetWindowText(STREAM->xIDC_DESTCALL, "");
		SetWindowText(STREAM->xIDC_DIRN, "");
		SetWindowText(STREAM->xIDC_STATUS, "");
		
		ARQ->ARQState = 0;

		SendLen = sprintf(Reply, "b%s:91", STREAM->MyCall); 

		ARQ->ARQTimerState = ARQ_WAITACK;
		SaveAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);
		ARQ->Retries = 2;
		return;
	}

	if (CTRL == 'b')
	{
		// Disconnect ACK

		char * call1;
		char * context;

		call1 = strtok_s(&Input[1], " ", &context);
		strlop(call1, ':');

		if (strcmp(STREAM->RemoteCall, call1))
			return;

		if (Channel != ARQ->OurStream)
			return;					// Wrong Session

		ARQ->ARQTimer = 0;
		ARQ->ARQTimerState = 0;
		ARQ->ARQState = 0;

		if (STREAM->Connected)
		{
			// Create a traffic record
		
			char logmsg[120];	
			time_t Duration;

			Duration = time(NULL) - STREAM->ConnectTime;
				
			if (Duration == 0)
				Duration = 1;
			
			sprintf(logmsg,"Port %2d %9s Bytes Sent %d  BPS %d Bytes Received %d BPS %d Time %d Seconds",
				TNC->Port, STREAM->RemoteCall,
				STREAM->BytesTXed, (int)(STREAM->BytesTXed/Duration),
				STREAM->BytesRXed, (int)(STREAM->BytesRXed/Duration), (int)Duration);

			Debugprintf(logmsg);
		}

		STREAM->Connecting = FALSE;
		STREAM->Connected = FALSE;		// Back to Command Mode
		STREAM->ReportDISC = TRUE;		// Tell Node

		STREAM->Disconnecting = FALSE;

		strcpy(TNC->WEB_PROTOSTATE, "Disconncted");
		SetWindowText(STREAM->xIDC_MYCALL, "");
		SetWindowText(STREAM->xIDC_DESTCALL, "");
		SetWindowText(STREAM->xIDC_DIRN, "");
		SetWindowText(STREAM->xIDC_STATUS, "");

		return;
	}

	if (CTRL == 'u')
	{
		// Beacon

		//>00uGM8BPQ:72 GM8BPQ TestingAD67

		char * Call = &Input[1];
		strlop(Call, ':');

		UpdateMH(TNC, Call, '!', 0);
		return;
	}

	if (STREAM->Connected)
	{
		if (Channel != ARQ->OurStream)
			return;					// Wrong Session

		if (CTRL >= ' ' && CTRL < 96)
		{
			// ARQ Data

			int Seq = CTRL - 32;
			int Work;

//			if (rand() % 5 == 2)
//			{
//				Debugprintf("Dropping %d", Seq);
//				return; 
//			}

			buffptr = GetBuff();
	
			if (buffptr == NULL)
				return;				// Sould never run out, but cant do much else

			// Remove any DLE transparency

			Len -= 1;

			buffptr[1]  = Len;
			memcpy(&buffptr[2], &Input[1], Len);
			STREAM->BytesRXed += Len;

			UpdateStatsLine(TNC, STREAM);

			// Safest always to save, then see what we can process

			if (ARQ->RXHOLDQ[Seq])
			{
				// Wot! Shouldn't happen

				ReleaseBuffer(ARQ->RXHOLDQ[Seq]);
//				Debugprintf("ARQ Seq %d Duplicate");
			}

			ARQ->RXHOLDQ[Seq] = buffptr;
//			Debugprintf("ARQ saving %d", Seq);

			// If this is higher that highest received, save. But beware of wrap'

			// Hi = 2, Seq = 60  dont save s=h = 58
			// Hi = 10 Seq = 12	 save s-h = 2
			// Hi = 14 Seq = 10  dont save s-h = -4
			// Hi = 60 Seq = 2	 save s-h = -58

			Work = Seq - ARQ->RXHighest;

			if ((Work > 0 && Work < 32) || Work < -32)
				ARQ->RXHighest = Seq;

			// We may now be able to process some

			Work = (ARQ->RXNoGaps + 1) & 63;		// The next one we need

			while (ARQ->RXHOLDQ[Work])
			{
				// We have it

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, ARQ->RXHOLDQ[Work]);
//				ReleaseBuffer(ARQ->RXHOLDQ[Work]);

				ARQ->RXHOLDQ[Work] = NULL;
//				Debugprintf("Processing %d from Q", Work);

				ARQ->RXNoGaps = Work;
				Work = (Work + 1) & 63;		// The next one we need
			}

			ARQ->TurnroundTimer = 200;		// Delay before allowing reply. Will normally be reset by the poll following data
			return;
		}
	}
}


static VOID SendARQData(struct TNCINFO * TNC, UINT * Buffer, int Stream)
{
	// Send Data, saving a copy until acked.

	struct STREAMINFO * STREAM = &TNC->Streams[Stream];
	struct ARQINFO * ARQ = STREAM->ARQInfo;


	UCHAR TXBuffer[300];
	SOCKET sock = TNC->WINMORDataSock;
	int SendLen;
	UCHAR * ptr;
	int Origlen = Buffer[1];
	
	ARQ->TXSeq++;
	ARQ->TXSeq &= 63;
	
	SendLen = sprintf(TXBuffer, "%c", ARQ->TXSeq + 32);

	ptr = (UCHAR *)&Buffer[2];			// Start of data;

	ptr[Buffer[1]] = 0;

	memcpy(&TXBuffer[SendLen], (UCHAR *)&Buffer[2], Origlen);
	SendLen += Origlen;

	TXBuffer[SendLen] = 0;

//	if (rand() % 5 == 2)
//		Debugprintf("Dropping %d", ARQ->TXSeq);
//	else 

	ARQ->TXHOLDQ[ARQ->TXSeq] = Buffer;

	STREAM->BytesTXed += Origlen;

	UpdateStatsLine(TNC, STREAM);


	// if waiting for ack, don't send, just queue. Will be sent when ack received

	if (ARQ->ARQTimer == 0 || ARQ->ARQTimerState == ARQ_WAITDATA)
	{
		SendPacket(TNC, STREAM, TXBuffer, SendLen);
		ARQ->ARQTimer = 15;			// wait up to 1.5 sec for more data before polling
		ARQ->Retries = 1;
		ARQ->ARQTimerState = ARQ_WAITDATA;
	}
	else
		STREAM->BytesResent -= Origlen;	// So wont be included in resent bytes
}

VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
	char Reply[80];
	int SendLen;

	struct STREAMINFO * STREAM = &TNC->Streams[Stream];
	struct ARQINFO * ARQ = STREAM->ARQInfo;

	SendLen = sprintf(Reply, "d%s:90", STREAM->MyCall); 

	SaveAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);
	ARQ->ARQTimerState = ARQ_DISC;
}

VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	TidyClose(TNC, Stream);			// I don't think Hostmode has a DD
}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
}

static VOID SaveAndSend(struct TNCINFO * TNC, struct ARQINFO * ARQ, SOCKET sock, char * Msg, int MsgLen)
{
	// Used for Messages that need a reply. Save, send and set timeout

	memcpy(ARQ->LastMsg, Msg, MsgLen + 1);	// Include Null
	ARQ->LastLen = MsgLen;

	// Delay the send for a shot while
	
//	SendPacket(sock, Msg, MsgLen, 0);

	ARQ->ARQTimer = 1;
	ARQ->Retries = TNC->Retries + 1;	// First timout is the real send

	return;
}




static VOID ARQTimer(struct TNCINFO * TNC)
{
	UINT * buffptr;
	struct STREAMINFO * STREAM;
	struct ARQINFO * ARQ;
	int SendLen;
	char Reply[80];
	int Stream;

	//Send frames, unless held by TurnroundTimer or Window

	int Outstanding;

	for (Stream = 0; Stream <MAXARQ; Stream++)
	{
		STREAM = &TNC->Streams[Stream];
		ARQ = STREAM->ARQInfo;

		//	TXDelay is used as a turn round delay for frames that don't have to be retried. It doesn't
		//	need to check for busy (or anything else (I think!)

	if (ARQ->TXDelay)
	{
		ARQ->TXDelay--;

		if (ARQ->TXDelay)
			continue;

		SendPacket(TNC, STREAM, ARQ->TXMsg, ARQ->TXLen);
	}

	// if We are alredy sending (State = ARQ_WAITDATA) we should allow it to send more (and the Poll at end)

	if (ARQ->ARQTimerState == ARQ_WAITDATA)
	{
		while (STREAM->BPQtoPACTOR_Q)
		{
			Outstanding = ARQ->TXSeq - ARQ->TXLastACK;
		
			if (Outstanding < 0)
				Outstanding += 64;

			TNC->PortRecord->FramesQueued = Outstanding + STREAM->BPQtoPACTOR_Q;		// Save for Appl Level Queued Frames

			if (Outstanding >= ARQ->TXWindow)
				break;
		
			buffptr = Q_REM(&STREAM->BPQtoPACTOR_Q);
			SendARQData(TNC, buffptr, Stream);
		}

		ARQ->ARQTimer--;

		if (ARQ->ARQTimer > 0)
			continue;					// Timer Still Running
	
		// No more data available - send poll 

		SendLen = sprintf(Reply, "p%s", STREAM->MyCall);

		ARQ->ARQTimerState = ARQ_WAITACK;

		// This is one message that should not be queued so it is sent straiget after data

		Debugprintf("Sending Poll After Data");

		memcpy(ARQ->LastMsg, Reply, SendLen + 1);
		ARQ->LastLen = SendLen;

		SendPacket(TNC, STREAM, Reply, SendLen);
		
		ARQ->ARQTimer = TNC->Timeout;
		ARQ->Retries = TNC->Retries;

		strcpy(TNC->WEB_PROTOSTATE, "Wait ACK");
		SetWindowText(STREAM->xIDC_STATUS, "Wait ACK");

		continue;
	
	}

	// TrunroundTimer is used to allow time for far end to revert to RX

	if (ARQ->TurnroundTimer)
		ARQ->TurnroundTimer--;

	if (ARQ->TurnroundTimer == 0)
	{
		while (STREAM->BPQtoPACTOR_Q)
		{
			Outstanding = ARQ->TXSeq - ARQ->TXLastACK;
		
			if (Outstanding < 0)
				Outstanding += 64;

			TNC->PortRecord->FramesQueued = Outstanding + STREAM->BPQtoPACTOR_Q + 1; // Make sure busy is reported to BBS

			if (Outstanding >= ARQ->TXWindow)
				break;
		
			buffptr = Q_REM(&STREAM->BPQtoPACTOR_Q);
			SendARQData(TNC, buffptr, Stream);
		}
	}

	if (ARQ->ARQTimer)
	{
		// Only decrement if running send poll timer
			
//		if (ARQ->ARQTimerState != ARQ_WAITDATA)
//			return;

		ARQ->ARQTimer--;
		{
			if (ARQ->ARQTimer)
				continue;					// Timer Still Running
		}

		ARQ->Retries--;

		if (ARQ->Retries)
		{
			// Retry Current Message

			SendPacket(TNC, STREAM, ARQ->LastMsg, ARQ->LastLen);
			ARQ->ARQTimer = TNC->Timeout + (rand() % 30);

			continue;
		}

		// Retried out.

		switch (ARQ->ARQTimerState)
		{
		case ARQ_WAITDATA:

			// No more data available - send poll 

			SendLen = sprintf(Reply, "p%s", STREAM->MyCall);

			Debugprintf("Sending Poll After Timeout??");

			ARQ->ARQTimerState = ARQ_WAITACK;

			// This is one message that should not be queued so it is sent straiget after data

			memcpy(ARQ->LastMsg, Reply, SendLen + 1);
			ARQ->LastLen = SendLen;

			SendPacket(TNC, STREAM, Reply, SendLen);
		
			ARQ->ARQTimer = TNC->Timeout;
			ARQ->Retries = TNC->Retries;

			strcpy(TNC->WEB_PROTOSTATE, "Wait ACK");
			SetWindowText(STREAM->xIDC_STATUS, "Wait ACK");

			continue;
	
		case ARQ_CONNECTING:

			// Report Connect Failed, and drop back to command mode

			buffptr = GetBuff();

			if (buffptr)
			{
				buffptr[1] = sprintf((UCHAR *)&buffptr[2], "UIARQ} Failure with %s\r", STREAM->RemoteCall);
				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
			}
	
			// Send Disc to TNC in case it got the Connects, but we missed the ACKs

			TidyClose(TNC, Stream);
			ARQ->Retries = 2;				// First timout is the real send, only send once
			STREAM->Connecting = FALSE;		// Back to Command Mode
			ARQ->ARQState = FALSE;

			break;

		case ARQ_WAITACK:
		case ARQ_CONNECTACK:
		case ARQ_DISC:
		
			STREAM->Connected = FALSE;		// Back to Command Mode
			STREAM->ReportDISC = TRUE;	
			ARQ->ARQState = FALSE;

			while (STREAM->PACTORtoBPQ_Q)
				ReleaseBuffer(Q_REM(&STREAM->PACTORtoBPQ_Q));
	
			while (STREAM->BPQtoPACTOR_Q)
				ReleaseBuffer(Q_REM(&STREAM->BPQtoPACTOR_Q));
	
			strcpy(TNC->WEB_TNCSTATE, "Free");

			strcpy(TNC->WEB_PROTOSTATE, "Disconncted");
		
			SetWindowText(STREAM->xIDC_MYCALL, "");
			SetWindowText(STREAM->xIDC_DESTCALL, "");
			SetWindowText(STREAM->xIDC_DIRN, "");
			SetWindowText(STREAM->xIDC_STATUS, "");

			break;

		}
	}
	}
}

static VOID ProcessARQStatus(struct TNCINFO * TNC, int Stream, struct ARQINFO * ARQ, char * Input)
{
	// Release any acked frames and resend any outstanding

	struct STREAMINFO * STREAM = &TNC->Streams[Stream];
	int LastInSeq = Input[1] - 32;
	int LastRXed = Input[2] - 32;
	int FirstUnAcked = ARQ->TXLastACK;
	int n = strlen(Input) - 3;
	char * ptr;
	int NexttoResend;
	int First, Last, Outstanding;
	UINT * Buffer;
	int Acked = 0;

	// First status is an ack of Connect ACK

	if (ARQ->ARQTimerState == ARQ_CONNECTACK)
	{
		ARQ->Retries = 0;
		ARQ->ARQTimer = 0;
		ARQ->ARQTimerState = 0;

		strcpy(TNC->WEB_PROTOSTATE, "Connected");
		SetWindowText(STREAM->xIDC_STATUS, "Connected");
	}

	Debugprintf("Lsast In Seq, LastRXed %d %d", LastInSeq, LastRXed); 

	//	Release all up to LastInSeq
	
	while (FirstUnAcked != LastInSeq)
	{
		FirstUnAcked++;
		FirstUnAcked &= 63;

		Buffer = ARQ->TXHOLDQ[FirstUnAcked];

		if (Buffer)
		{
			Debugprintf("Acked %d", FirstUnAcked);
			STREAM->BytesAcked += Buffer[1];
			ReleaseBuffer(Buffer);
			ARQ->TXHOLDQ[FirstUnAcked] = NULL;
			Acked++;
		}
	}

	ARQ->TXLastACK = FirstUnAcked;

	Outstanding = ARQ->TXSeq - ARQ->TXLastACK;

	if (Outstanding < 0)
		Outstanding += 64;

	TNC->PortRecord->FramesQueued = Outstanding + STREAM->BPQtoPACTOR_Q;		// Save for Appl Level Queued Frames

	if (FirstUnAcked == ARQ->TXSeq)
	{
		UpdateStatsLine(TNC, STREAM);
		ARQ->NoAckRetries = 0;

		strcpy(TNC->WEB_PROTOSTATE, "Connected");
		SetWindowText(STREAM->xIDC_STATUS, "Connected");

		return;								// All Acked
	}

	// Release any not in retry list up to LastRXed.

	ptr = &Input[3];

	while (n)
	{
		NexttoResend = *(ptr++) - 32;

		FirstUnAcked++;
		FirstUnAcked &= 63;

		while (FirstUnAcked != NexttoResend)
		{
			Buffer = ARQ->TXHOLDQ[FirstUnAcked];

			if (Buffer)
			{
				Debugprintf("Acked %d", FirstUnAcked);
				STREAM->BytesAcked += Buffer[1];
				ReleaseBuffer(Buffer);
				ARQ->TXHOLDQ[FirstUnAcked] = NULL;
				Acked++;
			}

			FirstUnAcked++;
			FirstUnAcked &= 63;
		}

		// We don't ACK this one. Process any more resend values, then release up to LastRXed.

		n--;
	}

	//	Release rest up to LastRXed
	
	while (FirstUnAcked != LastRXed)
	{
		FirstUnAcked++;
		FirstUnAcked &= 63;

		Buffer = ARQ->TXHOLDQ[FirstUnAcked];

		if (Buffer)
		{
			Debugprintf("Acked %d", FirstUnAcked);
			STREAM->BytesAcked += Buffer[1];
			ReleaseBuffer(Buffer);
			ARQ->TXHOLDQ[FirstUnAcked] = NULL;
			Acked++;
		}
	}

	// Resend anything in TX Buffer (From LastACK to TXSeq

	Last = ARQ->TXSeq + 1;
	Last &= 63;

	First = LastInSeq;

	while (First != Last)
	{
		First++;
		First &= 63;
			
		if(ARQ->TXHOLDQ[First])
		{
			UINT * Buffer = ARQ->TXHOLDQ[First];
			UCHAR TXBuffer[300];
			SOCKET sock = TNC->WINMORDataSock;
			int SendLen;

			Debugprintf("Resend %d", First);

			STREAM->BytesResent += Buffer[1];
		
			SendLen = sprintf(TXBuffer, "%c", First + 32);

				memcpy(&TXBuffer[SendLen], (UCHAR *)&Buffer[2], Buffer[1]);
				SendLen += Buffer[1];

			TXBuffer[SendLen] = 0;

			SendPacket(TNC, STREAM, TXBuffer, SendLen);

			ARQ->ARQTimer = 10;			// wait up to 1 sec for more data before polling
			ARQ->Retries = 1;
			ARQ->ARQTimerState = ARQ_WAITDATA;

			if (Acked == 0)
			{
				// Nothing acked by this statis message

				Acked = 1;					// Dont count more thna once
				ARQ->NoAckRetries++;
				if (ARQ->NoAckRetries > TNC->Retries)
				{
					// Too many retries - just disconnect

					TidyClose(TNC, Stream);
					return;
				}
			}
		}
	}
	UpdateStatsLine(TNC, STREAM);
}

