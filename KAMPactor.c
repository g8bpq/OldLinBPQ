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
//	DLL to inteface KAM TNC in Pactor Mode to BPQ32 switch 
//
//	Uses BPQ EXTERNAL interface
//

// Version 1.2.1.2 July 2010

// Send Change to ISS before each transmission
// Support up to 32 BPQ Ports

// Version 1.2.1.3 August 2010 

// Drop RTS as well as DTR on close

// Version 1.2.1.4 August 2010 

// Save Minimized State

// Version 1.2.1.5 September 2010

// Fix Freq Display after Node reconfig
// Only use AutoConnect APPL for Pactor Connects

// Version 1.2.2.1 September 2010

// Add option to get config from bpq32.cfg

//#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <stdio.h>
#include <stdlib.h>
#include "time.h"

#include "CHeaders.h"
#include "tncinfo.h"

#include "bpq32.h"

#define NARROWMODE Report_P1
#define WIDEMODE Report_P1		// Only supports PI

static char ClassName[]="KAMPACTORSTATUS";
static char WindowTitle[] = "KAM Pactor";
static int RigControlRow = 165;


static RECT Rect;

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

unsigned long _beginthread( void( *start_address )(), unsigned stack_size, int arglist);
int DoScanLine(struct TNCINFO * TNC, char * Buff, int Len);


ProcessLine(char * buf, int Port)
{
	UCHAR * ptr,* p_cmd;
	char * p_ipad = 0;
	char * p_port = 0;
	unsigned short WINMORport = 0;
	int BPQport;
	int len=510;
	struct TNCINFO * TNC;
	char errbuf[256];

	strcpy(errbuf, buf);

	ptr = strtok(buf, " \t\n\r");

	if(ptr == NULL) return (TRUE);

	if(*ptr =='#') return (TRUE);			// comment

	if(*ptr ==';') return (TRUE);			// comment

	ptr = strtok(NULL, " \t\n\r");

	if (_stricmp(buf, "ADDR") == 0)			// Winmor Using BPQ32 COnfig
	{
		BPQport = Port;
		p_ipad = ptr;
	}
	else
	if (_stricmp(buf, "APPL") == 0)			// Using BPQ32 C0nfig
	{
		BPQport = Port;
		p_cmd = ptr;
	}
	else
	if (_stricmp(buf, "PORT") != 0)			// Using Old Config
	{
		// New config without a PORT or APPL  - this is a Config Command

		strcpy(buf, errbuf);
		strcat(buf, "\r");

		BPQport = Port;

		TNC = TNCInfo[BPQport] = malloc(sizeof(struct TNCINFO));
		memset(TNC, 0, sizeof(struct TNCINFO));

		TNC->InitScript = malloc(1000);
		TNC->InitScript[0] = 0;
		goto ConfigLine;
	}
	else
	{

		// Old Config from file

		BPQport=0;
		BPQport = atoi(ptr);
	
		p_cmd = strtok(NULL, " \t\n\r");

		if (Port && Port != BPQport)
		{
			// Want a particular port, and this isn't it

			while(TRUE)
			{
				if (GetLine(buf) == 0)
					return TRUE;

				if (memcmp(buf, "****", 4) == 0)
					return TRUE;

			}
		}
	}

	if(BPQport > 0 && BPQport < 33)
	{
		TNC = TNCInfo[BPQport] = malloc(sizeof(struct TNCINFO));
		memset(TNC, 0, sizeof(struct TNCINFO));

		TNC->InitScript = malloc(1000);
		TNC->InitScript[0] = 0;

		if (p_cmd != NULL)
		{
			if (p_cmd[0] != ';' && p_cmd[0] != '#')
				TNC->ApplCmd=_strdup(p_cmd);
		}

		// Read Initialisation lines

		while(TRUE)
		{
			if (GetLine(buf) == 0)
				return TRUE;
ConfigLine:
			strcpy(errbuf, buf);

			if (memcmp(buf, "****", 4) == 0)
				return TRUE;

			ptr = strchr(buf, ';');
			if (ptr)
			{
				*ptr++ = 13;
				*ptr = 0;
			}

			if (_memicmp(buf, "OLDMODE", 7) == 0)
				TNC->OldMode = TRUE;
			else
			if (_memicmp(buf, "VERYOLDMODE", 7) == 0)
			{
				TNC->OldMode = TRUE;
				TNC->VeryOldMode = TRUE;
			}
			else
		
			if (_memicmp(buf, "BUSYWAIT", 8) == 0)		// Wait time beofre failing connect if busy
				TNC->BusyWait = atoi(&buf[8]);

			else

			if (_memicmp(buf, "WL2KREPORT", 10) == 0)
				TNC->WL2K = DecodeWL2KReportLine(buf);
			else
				strcat (TNC->InitScript, buf);
		}
	}
	return (TRUE);
}

#define	FEND	0xC0	// KISS CONTROL CODES 
#define	FESC	0xDB
#define	TFEND	0xDC
#define	TFESC	0xDD

static int MaxStreams = 26;

struct TNCINFO * CreateTTYInfo(int port, int speed);
BOOL OpenConnection(int);
BOOL SetupConnection(int);
BOOL CloseConnection(struct TNCINFO * conn);
static BOOL WriteCommBlock(struct TNCINFO * TNC);
BOOL DestroyTTYInfo(int port);
void CheckRXKAM(struct TNCINFO * TNC);
VOID KAMPoll(int Port);
VOID ProcessDEDFrame(struct TNCINFO * TNC, UCHAR * rxbuff, int len);
static VOID ProcessTermModeResponse(struct TNCINFO * TNC);
static VOID DoTNCReinit(struct TNCINFO * TNC);
static VOID DoTermModeTimeout(struct TNCINFO * TNC);

VOID ProcessPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID ProcessKPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID ProcessKHOSTPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID ProcessKNormCommand(struct TNCINFO * TNC, UCHAR * rxbuffer);
VOID ProcessHostFrame(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
static VOID DoMonitor(struct TNCINFO * TNC, UCHAR * Msg, int Len);

//	Note that Kantronics host Mode uses KISS format Packets (without a KISS COntrol Byte)

VOID EncodeAndSend(struct TNCINFO * TNC, UCHAR * txbuffer, int Len);
static int	KissEncode(UCHAR * inbuff, UCHAR * outbuff, int len);
static int	KissDecode(UCHAR * inbuff, UCHAR * outbuff, int len);

int KAMExtProc(int fn, int port,unsigned char * buff)
{
	int txlen = 0;
	UINT * buffptr;
	struct TNCINFO * TNC = TNCInfo[port];
	struct STREAMINFO * STREAM;

	int Stream;

	if (TNC == NULL)
		return 0;
	
	if (fn < 4 || fn > 5)
		if (TNC->hDevice == 0)
			return 0;					// Port not open

	switch (fn)
	{
	case 1:				// poll

		while (TNC->PortRecord->UI_Q)			// Release anything accidentally put on UI_Q
		{
			buffptr = Q_REM(&TNC->PortRecord->UI_Q);
			ReleaseBuffer(buffptr);
		}

		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			STREAM = &TNC->Streams[Stream];

			if (STREAM->ReportDISC)
			{
				STREAM->ReportDISC = FALSE;
				buff[4] = Stream;

				return -1;
			}
		}

		CheckRXKAM(TNC);
		KAMPoll(port);

		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			STREAM = &TNC->Streams[Stream];

			if (STREAM->PACTORtoBPQ_Q !=0)
			{
				int datalen;
			
				buffptr=Q_REM(&STREAM->PACTORtoBPQ_Q);

				datalen=buffptr[1];

				buff[4] = Stream;
				buff[7] = 0xf0;
				memcpy(&buff[8],buffptr+2,datalen);		// Data goes to +7, but we have an extra byte
				datalen+=8;

				PutLengthinBuffer(buff, datalen);

	//			buff[5]=(datalen & 0xff);
	//			buff[6]=(datalen >> 8);
	
				ReleaseBuffer(buffptr);
	
				return (1);
			}
		}
			
		return 0;

	case 2:				// send

		buffptr = GetBuff();

		if (buffptr == 0) return (0);			// No buffers, so ignore

		// Find TNC Record

		Stream = buff[4];
		STREAM = &TNC->Streams[Stream];
		
		if (!TNC->TNCOK)
		{
			// Send Error Response

			buffptr[1] = 36;
			memcpy(buffptr+2, "No Connection to PACTOR TNC\r", 36);

			C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
			
			return 0;
		}

		txlen = GetLengthfromBuffer(buff) - 8;

		buffptr[1] = txlen;
		memcpy(buffptr+2, &buff[8], txlen);

		C_Q_ADD(&STREAM->BPQtoPACTOR_Q, buffptr);

		if(STREAM->Connected)
		{
			STREAM->FramesOutstanding++;
			STREAM->FramesQueued++;

			STREAM->BytesOutstanding += txlen;
		}
		return (0);


	case 3:				// CHECK IF OK TO SEND. Also used to check if TNC is responding
		
		Stream = (int)buff;
	
		STREAM = &TNC->Streams[Stream];

		if (Stream == 0)
		{
			if (TNC->HFPacket)
			{
				if (TNC->Mem1 < 2000 || TNC->Streams[0].FramesOutstanding  > 4)	
					return (1 | TNC->HostMode << 8 | STREAM->Disconnecting << 15);

			}
			else
			{
				if (TNC->Streams[0].FramesQueued > 4)
					return (1 | TNC->HostMode << 8 | STREAM->Disconnecting << 15);
			}
		}
		else
		{
			if (STREAM->FramesOutstanding > 3 || STREAM->BytesOutstanding > 500 || TNC->Mem1 < 500)	
				return (1 | TNC->HostMode << 8 | STREAM->Disconnecting << 15);
		}

		return TNC->HostMode << 8 | STREAM->Disconnecting << 15;		// OK, but lock attach if disconnecting

	case 4:				// reinit

		return (0);

	case 5:				// Close

		EncodeAndSend(TNC, "Q", 1);			// Exit Host Mode
		Sleep(50);

		CloseCOMPort(TNCInfo[port]->hDevice);		
		return (0);

	case 6:				// Scan Control

		return 0;		// None Yet

	}
	return 0;

}

static int WebProc(struct TNCINFO * TNC, char * Buff, BOOL LOCAL)
{
	int Len = sprintf(Buff, "<html><meta http-equiv=expires content=0><meta http-equiv=refresh content=15>"
	"<head><title>KAM Pactor Status</title></head><body><h3>KAM Pactor Status</h3>");

	Len += sprintf(&Buff[Len], "<table style=\"text-align: left; width: 480px; font-family: monospace; align=center \" border=1 cellpadding=2 cellspacing=2>");

	Len += sprintf(&Buff[Len], "<tr><td width=90px>Comms State</td><td>%s</td></tr>", TNC->WEB_COMMSSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>TNC State</td><td>%s</td></tr>", TNC->WEB_TNCSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>Mode</td><td>%s</td></tr>", TNC->WEB_MODE);
	Len += sprintf(&Buff[Len], "<tr><td>Status</td><td>%s</td></tr>", TNC->WEB_STATE);
	Len += sprintf(&Buff[Len], "<tr><td>TX/RX State</td><td>%s</td></tr>", TNC->WEB_TXRX);
	Len += sprintf(&Buff[Len], "<tr><td>Free Space</td><td>%s</td></tr>", TNC->WEB_BUFFERS);
	Len += sprintf(&Buff[Len], "<tr><td>Traffic</td><td>%s</td></tr>", TNC->WEB_TRAFFIC);
	Len += sprintf(&Buff[Len], "</table>");

	Len = DoScanLine(TNC, Buff, Len);

	return Len;
}


UINT KAMExtInit(EXTPORTDATA * PortEntry)
{
	char msg[500];
	struct TNCINFO * TNC;
	int port;
	char * ptr;
	char * TempScript;

	//
	//	Will be called once for each Pactor Port
	//	The COM port number is in IOBASE
	//

	Debugprintf("KAM Extinit %x", KAMExtInit);

	port=PortEntry->PORTCONTROL.PORTNUMBER;

	sprintf(msg,"KAM Pactor %s", PortEntry->PORTCONTROL.SerialPortName);
	WritetoConsole(msg);

	ReadConfigFile(port, ProcessLine);

	TNC = TNCInfo[port];

	if (TNC == NULL)
	{
		// Not defined in Config file

		sprintf(msg," ** Error - no info in BPQ32.cfg for this port\n");
		WritetoConsole(msg);

		return (int)KAMExtProc;
	}
	TNC->Port = port;

	TNC->Hardware = H_KAM;

	if (TNC->BusyWait == 0)
		TNC->BusyWait = 10;

	PortEntry->MAXHOSTMODESESSIONS = 11;		// Default

	TNC->PortRecord = PortEntry;

	if (PortEntry->PORTCONTROL.PORTCALL[0] == 0)
		memcpy(TNC->NodeCall, MYNODECALL, 10);
	else
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);

	PortEntry->PORTCONTROL.PROTOCOL = 10;	// WINMOR/Pactor
	PortEntry->PORTCONTROL.PORTQUALITY = 0;
	PortEntry->SCANCAPABILITIES = NONE;		// No Scan Control 

	TNC->Interlock = PortEntry->PORTCONTROL.PORTINTERLOCK;

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0)
		PortEntry->PORTCONTROL.PORTPACLEN = 100;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	// Set Essential Params and MYCALL

	TempScript = malloc(4000);

	strcpy(TempScript, "MARK 1400\r");
	strcat(TempScript, "SPACE 1600\r");
	strcat(TempScript, "SHIFT MODEM\r");
	strcat(TempScript, "INV ON\r");
	strcat(TempScript, "PTERRS 30\r");			// Default Retries
	strcat(TempScript, "MAXUSERS 1/10\r");
	strcat(TempScript, TNC->InitScript);

	free(TNC->InitScript);
	TNC->InitScript = TempScript;

	// Others go on end so they can't be overriden

	strcat(TNC->InitScript, "ECHO OFF\r");
	strcat(TNC->InitScript, "XMITECHO ON\r");
	strcat(TNC->InitScript, "TXFLOW OFF\r");
	strcat(TNC->InitScript, "XFLOW OFF\r");
	strcat(TNC->InitScript, "TRFLOW OFF\r");
	strcat(TNC->InitScript, "AUTOCR 0\r");
	strcat(TNC->InitScript, "AUTOLF OFF\r");
	strcat(TNC->InitScript, "CRADD OFF\r");
	strcat(TNC->InitScript, "CRSUP OFF\r");
	strcat(TNC->InitScript, "CRSUP OFF/OFF\r");
	strcat(TNC->InitScript, "LFADD OFF/OFF\r");
	strcat(TNC->InitScript, "LFADD OFF\r");
	strcat(TNC->InitScript, "LFSUP OFF/OFF\r");
	strcat(TNC->InitScript, "LFSUP OFF\r");
	strcat(TNC->InitScript, "RING OFF\r");
	strcat(TNC->InitScript, "ARQBBS OFF\r");

	// Set the ax.25 MYCALL


	sprintf(msg, "MYCALL %s/%s\r", TNC->NodeCall, TNC->NodeCall);
	strcat(TNC->InitScript, msg);

	// look for the MAXUSERS config line, and get the limits

	TNC->InitScript = _strupr(TNC->InitScript);

	ptr = strstr(TNC->InitScript, "MAXUSERS");
	
	if (ptr)
	{
		ptr = strchr(ptr,'/');  // to the separator
		if (ptr)
			PortEntry->MAXHOSTMODESESSIONS = atoi(++ptr) + 1;
	}

	if (PortEntry->MAXHOSTMODESESSIONS > 26)
		PortEntry->MAXHOSTMODESESSIONS = 26;

	TNC->WebWindowProc = WebProc;
	TNC->WebWinX = 510;
	TNC->WebWinY = 280;

	TNC->WEB_COMMSSTATE = zalloc(100);
	TNC->WEB_TNCSTATE = zalloc(100);
	strcpy(TNC->WEB_TNCSTATE, "Free");
	TNC->WEB_MODE = zalloc(100);
	TNC->WEB_TRAFFIC = zalloc(100);
	TNC->WEB_BUFFERS = zalloc(100);
	TNC->WEB_STATE = zalloc(100);
	TNC->WEB_TXRX = zalloc(100);


#ifndef LINBPQ

	CreatePactorWindow(TNC, ClassName, WindowTitle, RigControlRow, PacWndProc, 500, 213);

 
	CreateWindowEx(0, "STATIC", "Comms State", WS_CHILD | WS_VISIBLE, 10,6,120,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_COMMSSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,6,386,20, TNC->hDlg, NULL, hInstance, NULL);
	
	CreateWindowEx(0, "STATIC", "TNC State", WS_CHILD | WS_VISIBLE, 10,28,106,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TNCSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,28,520,20, TNC->hDlg, NULL, hInstance, NULL);

	CreateWindowEx(0, "STATIC", "Mode", WS_CHILD | WS_VISIBLE, 10,50,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_MODE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,50,200,20, TNC->hDlg, NULL, hInstance, NULL);
 
	CreateWindowEx(0, "STATIC", "Status", WS_CHILD | WS_VISIBLE, 10,72,110,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_STATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,72,144,20, TNC->hDlg, NULL, hInstance, NULL);

 	CreateWindowEx(0, "STATIC", "TX/RX State", WS_CHILD | WS_VISIBLE,10,94,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TXRX = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE,116,94,374,20 , TNC->hDlg, NULL, hInstance, NULL);
 
	CreateWindowEx(0, "STATIC", "Free Space", WS_CHILD | WS_VISIBLE,10,116,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_BUFFERS = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE,116,116,374,20 , TNC->hDlg, NULL, hInstance, NULL);
	
	CreateWindowEx(0, "STATIC", "Traffic", WS_CHILD | WS_VISIBLE,10,138,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TRAFFIC = CreateWindowEx(0, "STATIC", "RX 0 TX 0 ACKED 0", WS_CHILD | WS_VISIBLE,116,138,374,20 , TNC->hDlg, NULL, hInstance, NULL);

	TNC->ClientHeight = 213;
	TNC->ClientWidth = 500;
	
	MoveWindows(TNC);
#endif
	OpenCOMMPort(TNC, PortEntry->PORTCONTROL.SerialPortName, PortEntry->PORTCONTROL.BAUDRATE, FALSE);

	WritetoConsole("\n");

	return ((int)KAMExtProc);
}



void CheckRXKAM(struct TNCINFO * TNC)
{
	int Length, Len;

	// only try to read number of bytes in queue 

	if (TNC->RXLen == 500)
		TNC->RXLen = 0;

	Len = ReadCOMBlock(TNC->hDevice, &TNC->RXBuffer[TNC->RXLen], 500 - TNC->RXLen);

	if (Len == 0)
		return;

	TNC->RXLen += Len;

	Length = TNC->RXLen;

	// If first char != FEND, then probably a Terminal Mode Frame. Wait for CR on end
			
	if (TNC->RXBuffer[0] != FEND)
	{
		// Char Mode Frame I think we need to see cmd: on end

		// If we think we are in host mode, then to could be noise - just discard.

		if (TNC->HostMode)
		{
			TNC->RXLen = 0;		// Ready for next frame
			return;
		}

		TNC->RXBuffer[TNC->RXLen] = 0;

//		if (TNC->RXBuffer[TNC->RXLen-2] != ':')
		if (strstr(TNC->RXBuffer, "cmd:") == 0)
			return;				// Wait for rest of frame

		// Complete Char Mode Frame

		TNC->RXLen = 0;		// Ready for next frame
					
		if (TNC->HostMode == 0)
		{
			// We think TNC is in Terminal Mode
			ProcessTermModeResponse(TNC);
			return;
		}
		// We thought it was in Host Mode, but are wrong.

		TNC->HostMode = FALSE;
		return;
	}

	// Receiving a Host Mode frame

	if (TNC->HostMode == 0)	// If we are in Term Mode, discard it. Probably in recovery
	{
		TNC->RXLen = 0;		// Ready for next frame
		return;
	}

	if (Length < 3)				// Minimum Frame Sise
		return;

	if (TNC->RXBuffer[Length-1] != FEND)
		return;					// Wait till we have a full frame

	ProcessHostFrame(TNC, TNC->RXBuffer, Length);	// Could have multiple packets in buffer

	TNC->RXLen = 0;		// Ready for next frame

		
	return;

}

VOID ProcessHostFrame(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len)
{
	UCHAR * FendPtr;
	int NewLen;

	//	Split into KISS Packets. By far the most likely is a single KISS frame
	//	so treat as special case

	if (rxbuffer[1] == FEND)			// Two FENDS - probably got out of sync
	{
		rxbuffer++;
		Len--;
	}
	
	FendPtr = memchr(&rxbuffer[1], FEND, Len-1);
	
	if (FendPtr == &rxbuffer[Len-1])
	{
		ProcessKHOSTPacket(TNC, &rxbuffer[1], Len - 2);
		return;
	}
		
	// Process the first Packet in the buffer

	NewLen =  FendPtr - rxbuffer -1;

	ProcessKHOSTPacket(TNC, &rxbuffer[1], NewLen);
	
	// Loop Back

	ProcessHostFrame(TNC, FendPtr+1, Len - NewLen - 2);
	return;

}



static BOOL WriteCommBlock(struct TNCINFO * TNC)
{
	WriteCOMBlock(TNC->hDevice, TNC->TXBuffer, TNC->TXLen);

	return TRUE;
}

VOID KAMPoll(int Port)
{
	struct TNCINFO * TNC = TNCInfo[Port];
	struct STREAMINFO * STREAM;

	UCHAR * Poll = TNC->TXBuffer;
	char Status[80];
	int Stream;

	if (TNC->PortRecord == 0)
		Stream = 0;


	// If Pactor Session has just been attached, drop back to cmd mode and set Pactor Call to 
	// the connecting user's callsign

	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		STREAM = &TNC->Streams[Stream];

		if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] && STREAM->Attached == 0)
		{
			// New Attach

			STREAM->Attached = TRUE;

			if (Stream == 0)			// HF Port
			{
				int calllen;
				UCHAR TXMsg[1000] = "D20";
				int datalen;
				char Msg[80];
				
				TNC->HFPacket = FALSE;
				TNC->TimeInRX = 0;

				calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[0]->L4USER, TNC->Streams[0].MyCall);
				TNC->Streams[0].MyCall[calllen] = 0;
		
					EncodeAndSend(TNC, "X", 1);			// ??Return to packet mode??
					if (TNC->VeryOldMode)
						datalen = sprintf(TXMsg, "C20MYCALL %s", TNC->Streams[0].MyCall);
					else
						datalen = sprintf(TXMsg, "C20MYPTCALL %s", TNC->Streams[0].MyCall);
					EncodeAndSend(TNC, TXMsg, datalen);
					TNC->InternalCmd = 'M';
		
					TNC->NeedPACTOR = 0;		// Cancel enter Pactor

					sprintf(TNC->WEB_TNCSTATE, "In Use by %s", TNC->Streams[0].MyCall);
					SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

					// Stop Scanning

					sprintf(Msg, "%d SCANSTOP", TNC->Port);
		
					Rig_Command(-1, Msg);

			}
		}
	}
	
	if (TNC->Timeout)
	{
		TNC->Timeout--;
		
		if (TNC->Timeout)			// Still waiting
			return;

		// Timed Out

		if (TNC->HostMode == 0)
		{
			DoTermModeTimeout(TNC);
			return;
		}

		// Timed out in host mode - Clear any connection and reinit the TNC

		Debugprintf("KAM PACTOR - Link to TNC Lost");
		TNC->TNCOK = FALSE;
		TNC->HostMode = 0;
		TNC->ReinitState = 0;
				
		sprintf(TNC->WEB_COMMSSTATE, "%s Open but TNC not responding", TNC->PortRecord->PORTCONTROL.SerialPortName);
		SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			UINT * buffptr;
			
			STREAM = &TNC->Streams[Stream];

			if (TNC->PortRecord->ATTACHEDSESSIONS[Stream])		// Connected
			{
				STREAM->Connected = FALSE;		// Back to Command Mode
				STREAM->ReportDISC = TRUE;		// Tell Node
			}
			
			STREAM->FramesQueued = 0;

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

	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		STREAM = &TNC->Streams[Stream];

		if (STREAM->Attached)
			CheckForDetach(TNC, Stream, STREAM, TidyClose, ForcedClose, CloseComplete);

	}

	// if we have just restarted or TNC appears to be in terminal mode, run Initialisation Sequence

	if (!TNC->HostMode)
	{
		DoTNCReinit(TNC);
		return;
	}

	if (TNC->BusyDelay)		// Waiting to send connect
	{
		// Still Busy?

		if (InterlockedCheckBusy(TNC) == 0)
		{
			// No, so send

			EncodeAndSend(TNC, TNC->ConnectCmd, strlen(TNC->ConnectCmd));
			free(TNC->ConnectCmd);

			TNC->Timeout = 50;
			TNC->InternalCmd = 'C';			// So we dont send the reply to the user.
			STREAM->Connecting = TRUE;

			TNC->Streams[0].Connecting = TRUE;

			sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
			SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

			TNC->BusyDelay = 0;
			return;
		}
		else
		{
			// Wait Longer

			TNC->BusyDelay--;

			if (TNC->BusyDelay == 0)
			{
				// Timed out - Send Error Response

				UINT * buffptr = GetBuff();

				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1]=39;
				memcpy(buffptr+2,"Sorry, Can't Connect - Channel is busy\r", 39);

				C_Q_ADD(&TNC->Streams[0].PACTORtoBPQ_Q, buffptr);

				free(TNC->ConnectCmd);

			}
		}
	}

	if (TNC->NeedPACTOR)
	{
		TNC->NeedPACTOR--;

		if (TNC->NeedPACTOR == 0)
		{
			int datalen;
			UCHAR TXMsg[80] = "D20";

			if (TNC->VeryOldMode)
				datalen = sprintf(TXMsg, "C20MYCALL %s", TNC->NodeCall);
			else
				datalen = sprintf(TXMsg, "C20MYPTCALL %s", TNC->NodeCall);
			EncodeAndSend(TNC, TXMsg, datalen);

			if (TNC->OldMode)
				EncodeAndSend(TNC, "C20PACTOR", 9);			// Back to Listen
			else
				EncodeAndSend(TNC, "C20TOR", 6);			// Back to Listen

			TNC->InternalCmd = 'T';
			TNC->Timeout = 50;
			TNC->IntCmdDelay--;

			// Restart Scanning

			sprintf(Status, "%d SCANSTART 15", TNC->Port);
		
			Rig_Command(-1, Status);

			return;
		}
	}

	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		STREAM = &TNC->Streams[Stream];

		// If in HF Packet mode, normal flow control doesn't seem to work
		//	If more that 4 packets sent, send a status poll. and use response to 
		//	reset frames outstanding

		if ((Stream == 0) && (TNC->HFPacket) && (TNC->Streams[0].FramesOutstanding  > 4))
		{
			EncodeAndSend(TNC, "C10S", 4);
			TNC->InternalCmd = 'S';
			TNC->Timeout = 50;
			return;

		}

		if (TNC->TNCOK && STREAM->BPQtoPACTOR_Q)
		{
			int datalen;
			UCHAR TXMsg[1000] = "D20";
			int * buffptr;
			UCHAR * MsgPtr;
			char Status[80];

			if (STREAM->Connected)
			{
				int Next;
				
				if (Stream > 0)
					sprintf(TXMsg, "D1%c", Stream + '@');
				else if (TNC->HFPacket)
					memcpy(TXMsg, "D2A", 3);
				else
				{
					// Pactor

					// Limit amount in TX, so we keep some on the TX Q and don't send turnround too early

					if (TNC->Streams[0].BytesTXed - TNC->Streams[0].BytesAcked > 200)
						continue;

					// Dont send if IRS State
					// If in IRS state for too long, force turnround

					if (TNC->TXRXState == 'R')
					{
						if (TNC->TimeInRX++ > 15)
							EncodeAndSend(TNC, "T", 1);			// Changeover to ISS 
						else
							goto Poll;
					}
					TNC->TimeInRX = 0;
				}

				buffptr=Q_REM(&STREAM->BPQtoPACTOR_Q);
				STREAM->FramesQueued--;

				datalen=buffptr[1];
				MsgPtr = (UCHAR *)&buffptr[2];

				if (TNC->SwallowSignon && Stream == 0)
				{
					TNC->SwallowSignon = FALSE;	
					if (strstr(MsgPtr, "Connected"))	// Discard *** connected
					{
						ReleaseBuffer(buffptr);
						return;
					}
				}

				Next = 0;
				STREAM->BytesTXed += datalen; 

				if (Stream == 0)
				{
					while (datalen > 100)		// Limit Pactor Sends
					{
						memcpy(&TXMsg[3], &MsgPtr[Next], 100);
						EncodeAndSend(TNC, TXMsg, 103);
						Next += 100;
						datalen -= 100;
					}
				}

				memcpy(&TXMsg[3], &MsgPtr[Next], datalen);
				EncodeAndSend(TNC, TXMsg, datalen + 3);
				ReleaseBuffer(buffptr);

				if (Stream == 0)
				{
					sprintf(Status, "RX %d TX %d ACKED %d ",
						TNC->Streams[0].BytesRXed, TNC->Streams[0].BytesTXed, TNC->Streams[0].BytesAcked);
					SetWindowText(TNC->xIDC_TRAFFIC, Status);

					if ((TNC->HFPacket == 0) && (TNC->Streams[0].BPQtoPACTOR_Q == 0))		// Nothing following
					{
						EncodeAndSend(TNC, "E", 1);			// Changeover when all sent
					}
				}

				if (STREAM->Disconnecting)
				{
					Debugprintf("Send with Disc Pending, Q = %x", STREAM->BPQtoPACTOR_Q);
					if (STREAM->BPQtoPACTOR_Q == 0)		// All Sent

						// KAM doesnt have a tidy close!

						STREAM->DisconnectingTimeout = 100;							// Give 5 secs to get to other end
				}
				return;
			}
			else // Not Connected
			{
				buffptr=Q_REM(&STREAM->BPQtoPACTOR_Q);
				datalen=buffptr[1];
				MsgPtr = (UCHAR *)&buffptr[2];

				// Command. Do some sanity checking and look for things to process locally

				datalen--;				// Exclude CR
				MsgPtr[datalen] = 0;	// Null Terminate
				_strupr(MsgPtr);

				if ((Stream == 0) && memcmp(MsgPtr, "RADIO ", 6) == 0)
				{
					sprintf(&MsgPtr[40], "%d %s", TNC->Port, &MsgPtr[6]);
					if (Rig_Command(TNC->PortRecord->ATTACHEDSESSIONS[0]->L4CROSSLINK->CIRCUITINDEX, &MsgPtr[40]))
					{
						ReleaseBuffer(buffptr);
					}
					else
					{
						buffptr[1] = sprintf((UCHAR *)&buffptr[2], "%s", &MsgPtr[40]);
						C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
					}
					return;
				}

				if (_memicmp(MsgPtr, "D\r", 2) == 0)
				{
					STREAM->ReportDISC = TRUE;		// Tell Node
					return;
				}

				if ((Stream == 0) && memcmp(MsgPtr, "HFPACKET", 8) == 0)
				{
					TNC->HFPacket = TRUE;
					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "KAM} OK\r");
					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
					return;
				}

				if (MsgPtr[0] == 'C' && MsgPtr[1] == ' ' && datalen > 2)	// Connect
				{
					memcpy(STREAM->RemoteCall, &MsgPtr[2], 9);
					STREAM->Connecting = TRUE;

					// If Stream 0, Convert C CALL to PACTOR CALL

					if (Stream == 0)
					{
						if (TNC->HFPacket)
							datalen = sprintf(TXMsg, "C2AC %s", TNC->Streams[0].RemoteCall);
						else
							datalen = sprintf(TXMsg, "C20PACTOR %s", TNC->Streams[0].RemoteCall);
						
						// If Pactor, check busy detecters on any interlocked ports

						if (TNC->HFPacket == 0 && InterlockedCheckBusy(TNC) && TNC->OverrideBusy == 0)
						{
							// Channel Busy. Wait

							TNC->ConnectCmd = _strdup(TXMsg);

							sprintf(TNC->WEB_TNCSTATE, "Waiting for clear channel");
							SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

							TNC->BusyDelay = TNC->BusyWait * 10;

							return;
						}

						TNC->OverrideBusy = FALSE;

						sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s",
							TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
						SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);
					}
					else
						datalen = sprintf(TXMsg, "C1%cC %s", Stream + '@', STREAM->RemoteCall);

					EncodeAndSend(TNC, TXMsg, datalen);
					TNC->Timeout = 50;
					TNC->InternalCmd = 'C';			// So we dont send the reply to the user.
					ReleaseBuffer(buffptr);
					STREAM->Connecting = TRUE;

					return;
				}

				if (memcmp(MsgPtr, "DISCONNECT", datalen) == 0)	// Disconnect
				{
					if (Stream == 0)
					{
						if (TNC->HFPacket)
							EncodeAndSend(TNC, "C2AD", 4);		// ??Return to packet mode??
						else
							EncodeAndSend(TNC, "X", 1);			// ??Return to packet mode??
			
						TNC->NeedPACTOR = 50;
					}
					else
					{
						sprintf(TXMsg, "C1%cD", Stream + '@');
						EncodeAndSend(TNC, TXMsg, 4);
						TNC->CmdStream = Stream;
						TNC->Timeout = 50;
					}

					TNC->Timeout = 0;					//	Don't expect a response
					STREAM->Connecting = FALSE;
					STREAM->ReportDISC = TRUE;
					ReleaseBuffer(buffptr);

					return;
				}
	
				// Other Command ??

				if (Stream > 0)
					datalen = sprintf(TXMsg, "C1%c%s", Stream + '@', MsgPtr);
				else
					datalen = sprintf(TXMsg, "C20%s", MsgPtr);
				EncodeAndSend(TNC, TXMsg, datalen);
				ReleaseBuffer(buffptr);
				TNC->Timeout = 50;
				TNC->InternalCmd = 0;
				TNC->CmdStream = Stream;

			}
		}
	}

Poll:

	// Need to poll data and control channel (for responses to commands)

	// Also check status if we have data buffered (for flow control)

	if (TNC->TNCOK)
	{
		if (TNC->IntCmdDelay == 50)
		{
			EncodeAndSend(TNC, "C10S", 4);
			TNC->InternalCmd = 'S';
			TNC->Timeout = 50;
			TNC->IntCmdDelay--;
			return;
		}

		if (TNC->IntCmdDelay <=0)
		{
			if (TNC->VeryOldMode == FALSE)
			{
				EncodeAndSend(TNC, "?", 1);
				TNC->InternalCmd = '?';
				TNC->Timeout = 50;
			}
			TNC->IntCmdDelay = 100;	// Every 30
			return;
		}
		else
			TNC->IntCmdDelay--;
	}

	return;

}

static VOID DoTNCReinit(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 1)		// Forcing back to Term
		TNC->ReinitState = 0;		// Got Response, so must be back in term mode

	if (TNC->ReinitState == 0)
	{
		// Just Starting - Send a TNC Mode Command to see if in Terminal or Host Mode
		
		sprintf(TNC->WEB_COMMSSTATE,"%s Initialising TNC", TNC->PortRecord->PORTCONTROL.SerialPortName);
		SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

		Poll[0] = 13;
		TNC->TXLen = 1;

		WriteCommBlock(TNC);
		TNC->Timeout = 50;

		return;
	}

	if (TNC->ReinitState == 2)		// In Term State, Sending Initialisation Commands
	{
		char * start, * end;
		int len;

		start = TNC->InitPtr;
		
		if (*(start) == 0)			// End of Script
		{
			// Put into Host Mode

			memcpy(Poll, "INTFACE HOST\r", 13);

			TNC->TXLen = 13;
			WriteCommBlock(TNC);
			TNC->Timeout = 50;

			TNC->ReinitState = 4;	// Need Reset 

			return;
		}
		
		end = strchr(start, 13);
		len = ++end - start;
		TNC->InitPtr = end;
		memcpy(Poll, start, len);

		TNC->TXLen = len;
		WriteCommBlock(TNC);
		TNC->Timeout = 50;

		return;

	}
}

static VOID DoTermModeTimeout(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 0)
	{
		//Checking if in Terminal Mode - Try to set back to Term Mode

		TNC->ReinitState = 1;
		Poll[0] = 3;
		Poll[1] = 0x58;				// ?? Back to cmd: mode ??
		TNC->TXLen = 2;

		Poll[0] = 0xc0;
		Poll[1] = 'Q';				// ?? Back to cmd: mode ??
		Poll[2] = 0xc0;
		TNC->TXLen = 3;

		WriteCommBlock(TNC);

		return;
	}
	if (TNC->ReinitState == 1)
	{
		// Forcing back to Term Mode

		TNC->ReinitState = 0;
		DoTNCReinit(TNC);				// See if worked
		return;
	}

	if (TNC->ReinitState == 3)
	{
		// Entering Host Mode
	
		// Assume ok

		TNC->HostMode = TRUE;
		return;
	}
}

	

static VOID ProcessTermModeResponse(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 0 || TNC->ReinitState == 1) 
	{
		// Testing if in Term Mode. It is, so can now send Init Commands

		TNC->InitPtr = TNC->InitScript;
		TNC->ReinitState = 2;
		DoTNCReinit(TNC);		// Send First Command
		return;
	}
	if (TNC->ReinitState == 2)
	{
		// Sending Init Commands

		DoTNCReinit(TNC);		// Send Next Command
		return;
	}

	if (TNC->ReinitState == 4)		// Send INTFACE, Need RESET
	{
		TNC->ReinitState = 5;
			
		memcpy(Poll, "RESET\r", 6);

		TNC->TXLen = 6;
		WriteCommBlock(TNC);
		TNC->Timeout = 50;
		TNC->HostMode = TRUE;		// Should now be in Host Mode
		TNC->NeedPACTOR = 50;		// Need to Send PACTOR command after 5 secs

		return;
	}

	if (TNC->ReinitState == 5)		// RESET sent
	{
		TNC->ReinitState = 5;

		return;
	}



}

VOID ProcessKHOSTPacket(struct TNCINFO * TNC, UCHAR * Msg, int Len)
{
	UINT * buffptr;
	char * Buffer = &Msg[3];			// Data portion of frame
	char * Call;
	char Status[80];
	int Stream = 0;
	struct STREAMINFO * STREAM;

	// Any valid frame is an ACK

	TNC->TNCOK = TRUE;

	Len = KissDecode(Msg, Msg, Len);	// Remove KISS transparency

	if (Msg[1] == '0' && Msg[2] == '0')
		Stream = 0;
	else
		if (Msg[1] == '2') Stream = 0; else Stream = Msg[2] - '@';	

	STREAM = &TNC->Streams[Stream];

	//	See if Poll Reply or Data

	Msg[Len] = 0; // Terminate

	if (Msg[0] == 'M')					// Monitor
	{
		DoMonitor(TNC, Msg, Len);
		return;
	}


	if (Msg[0] == 'E')					// Data Echo
	{
		if (Msg[1] == '2')				// HF Port
		{
			if (TNC->Streams[0].BytesTXed)
				TNC->Streams[0].BytesAcked += Len - 3;	// We get an ack before the first send

			sprintf(Status, "RX %d TX %d ACKED %d ",
				TNC->Streams[0].BytesRXed, TNC->Streams[0].BytesTXed, TNC->Streams[0].BytesAcked);
			SetWindowText(TNC->xIDC_TRAFFIC, Status);

			if (TNC->Streams[0].BytesTXed - TNC->Streams[0].BytesAcked < 500)
				TNC->Streams[0].FramesOutstanding = 0;
		}
		return;
	}

	if (Msg[0] == 'D')					// Data
	{	
		// Pass to Appl

		buffptr = GetBuff();
		if (buffptr == NULL) return;			// No buffers, so ignore

		Len-=3;							// Remove Header

		buffptr[1] = Len;				// Length
		STREAM->BytesRXed += Len;
		memcpy(&buffptr[2], Buffer, Len);

		C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

		if (Stream == 0)
		{
			sprintf(TNC->WEB_TRAFFIC, "RX %d TX %d ACKED %d ", 
					TNC->Streams[0].BytesRXed, TNC->Streams[0].BytesTXed, TNC->Streams[0].BytesAcked);
			SetWindowText(TNC->xIDC_TRAFFIC, TNC->WEB_TRAFFIC);
		}
		return;
	}


	if (Msg[0] == 'C')					// Command Reponse
	{
		TNC->Timeout = 0;
	
		// See if we need to process locally (Response to our command, Incoming Call, Disconencted, etc

		// See if a response to internal command

		if (TNC->InternalCmd)
		{
			// Process it

			if (TNC->InternalCmd == 'S')		// Status
			{
				char * Line;
				char * ptr;
					
				// Message is line giving free bytes, followed by a line for each active (packet) stream

				// FREE BYTES 1366/5094
				// A/2 #1145(12) CONNECTED to KE7XO-3
				// S/2 CONNECTED to NLV

				// each line is teminated by CR, and by the time it gets here it is null terminated

				//FREE BYTES 2628
				//A/H #80(1) CONNECTED to DK0MNL..

				if (TNC->HFPacket)
					TNC->Streams[0].FramesOutstanding = 0;

				Line = strchr(&Msg[3], 13);
				if (Line == 0) return;

				*(Line) = 0;

				ptr =  strchr(&Msg[13], '/');
				TNC->Mem1 = atoi(&Msg[13]);
				if (ptr) 
					TNC->Mem2 = atoi(++ptr);
				else
					TNC->Mem2 = 0;

				SetWindowText(TNC->xIDC_BUFFERS, &Msg[14]);
				strcpy(TNC->WEB_BUFFERS, &Msg[14]);

				while (Line[1] != 0)		// End of stream
				{
					Stream = Line[1] - '@';
					STREAM = &TNC->Streams[Stream];

					if (Line[5] == '#')
					{
						STREAM->BytesOutstanding = atoi(&Line[6]);
						ptr = strchr(&Line[6], '(');
						if (ptr)
							STREAM->FramesOutstanding = atoi(++ptr);
					}
					else
					{
						STREAM->BytesOutstanding = 0;
						STREAM->FramesOutstanding = 0;
					}
						
					Line = strchr(&Line[1], 13);
				}
				return;
			}
			return;
		}

		// Pass to Appl

		Stream = TNC->CmdStream;


		buffptr = GetBuff();

		if (buffptr == NULL) return;			// No buffers, so ignore

		buffptr[1] = sprintf((UCHAR *)&buffptr[2],"KAM} %s", Buffer);

		C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

		return;
	}

	if (Msg[0] == 'I')					// ISS/IRS State
	{
		if (Msg[2] == '1')
		{
			strcpy(TNC->WEB_TXRX, "Sender");
			SetWindowText(TNC->xIDC_TXRX, "Sender");
			TNC->TXRXState = 'S';
		}
		else
		{
			strcpy(TNC->WEB_TXRX, "Receiver");
			SetWindowText(TNC->xIDC_TXRX, "Receiver");
			TNC->TXRXState = 'R';
		}
		return;
	}

	if (Msg[0] == '?')					// Status
	{
		TNC->Timeout = 0;
		return;
	}

	if (Msg[0] == 'S')					// Status
	{
		if (Len < 4)
		{
			// Reset Response FEND FEND S00 FEND
					
			TNC->Timeout = 0;

			sprintf(TNC->WEB_COMMSSTATE,"%s TNC link OK", TNC->PortRecord->PORTCONTROL.SerialPortName);
			SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

			return;
		}
		
		// Pass to Appl

		if (strstr(Buffer, "STANDBY>") || strstr(Buffer, "*** DISCONNECTED"))
		{
			if ((STREAM->Connecting | STREAM->Connected) == 0)
			{
				// Not connected or Connecting. Probably response to going into Pactor Listen Mode

				return;
			}
	
			if (STREAM->Connecting && STREAM->Disconnecting == FALSE)
			{
				// Connect Failed
			
				buffptr = GetBuff();
				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Failure with %s\r", STREAM->RemoteCall);

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
	
				STREAM->Connecting = FALSE;
				STREAM->Connected = FALSE;				// In case!
				STREAM->FramesOutstanding = 0;

				return;
			}

			// Must Have been connected or disconnecting - Release Session

			STREAM->Connecting = FALSE;
			STREAM->Connected = FALSE;		// Back to Command Mode
			STREAM->FramesOutstanding = 0;

			if (STREAM->Disconnecting == FALSE)
				STREAM->ReportDISC = TRUE;		// Tell Node

			STREAM->Disconnecting = FALSE;

			if (Stream == 0)
			{
				// Need to reset Pactor Call in case it was changed

				EncodeAndSend(TNC, "X", 1);			// ??Return to packet mode??
				TNC->NeedPACTOR = 20;
			}

			return;
		}

		if (Msg[2] == '0')
			Call = strstr(Buffer, "<LINKED TO");
		else
			Call = strstr(Buffer, "NNECTED to");

		if (Call)
		{	
			Call+=11;					// To Callsign
			
			if (Stream == 0 && TNC->HFPacket == 0)
			{
				Buffer[Len-4] = 0;
			}

			STREAM->BytesRXed = STREAM->BytesTXed = STREAM->BytesAcked = 0;
			STREAM->ConnectTime = time(NULL); 

			if (Stream == 0)
			{
				// Stop Scanner

				char Msg[80];
				
				sprintf(Msg, "%d SCANSTOP", TNC->Port);
		
				Rig_Command(-1, Msg);

				sprintf(TNC->WEB_TRAFFIC, "RX %d TX %d ACKED %d ", 
					TNC->Streams[0].BytesRXed, TNC->Streams[0].BytesTXed, TNC->Streams[0].BytesAcked);
				
				SetWindowText(TNC->xIDC_TRAFFIC, TNC->WEB_TRAFFIC);
			}

			if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] == 0)
			{
				// Incoming Connect

				TRANSPORTENTRY * SESS;

				if (Msg[1] == '2' && Msg[2] == 'A')
					TNC->HFPacket = TRUE;

				ProcessIncommingConnect(TNC, Call, Stream, TRUE);

				SESS = TNC->PortRecord->ATTACHEDSESSIONS[Stream];

				if (Stream == 0)
				{
					struct WL2KInfo * WL2K = TNC->WL2K;
					char FreqAppl[10] = "";				// Frequecy-specific application
	
					if (TNC->RIG && TNC->RIG != &TNC->DummyRig)
					{
						sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound Freq %s", TNC->Streams[0].RemoteCall, TNC->NodeCall, TNC->RIG->Valchar);
						SESS->Frequency = (atof(TNC->RIG->Valchar) * 1000000.0) + 1500;		// Convert to Centre Freq
						// If Scan Entry has a Appl, save it

						if (TNC->RIG->FreqPtr[0]->APPL[0])
							strcpy(FreqAppl, &TNC->RIG->FreqPtr[0]->APPL[0]);
					}
					else
					{
						sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound", TNC->Streams[0].RemoteCall, TNC->NodeCall);
				
						if (WL2K)
						{
							SESS->Frequency = WL2K->Freq;
						}
					}

					SESS->Mode = 11;			// P1
					
					if (WL2K)
						strcpy(SESS->RMSCall, WL2K->RMSCall);		
				
					SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

					EncodeAndSend(TNC, "T", 1);			// Changeover to ISS 

					// If an autoconnect APPL is defined, send it	

					if (FreqAppl[0])			// Frequency spcific APPL overrides TNC APPL
					{
						buffptr = GetBuff();
						if (buffptr == 0) return;			// No buffers, so ignore

						buffptr[1] = sprintf((UCHAR *)&buffptr[2], "%s\r", FreqAppl);
						C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
						TNC->SwallowSignon = TRUE;
						return;
					}

					if (TNC->ApplCmd)
					{
						buffptr = GetBuff();
						if (buffptr == 0) return;			// No buffers, so ignore

						buffptr[1] = sprintf((UCHAR *)&buffptr[2], "%s\r", TNC->ApplCmd);
						C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

						TNC->SwallowSignon = TRUE;
						return;
					}
				}

				if (FULL_CTEXT && HFCTEXTLEN == 0)
				{
					char CTBuff[300] = "D20";
					int Len = CTEXTLEN, CTPaclen = 50;
					int Next = 0;

					if (Stream > 0)
						sprintf(CTBuff, "D1%c", Stream + '@');

					while (Len > CTPaclen)		// CTEXT Paclen
					{
						memcpy(&CTBuff[3], &CTEXTMSG[Next], CTPaclen);
						EncodeAndSend(TNC, CTBuff, CTPaclen + 3);
						Next += CTPaclen;
						Len -= CTPaclen;
					}

					memcpy(&CTBuff[3], &CTEXTMSG[Next], Len);
					EncodeAndSend(TNC, CTBuff, Len + 3);
					EncodeAndSend(TNC, "E", 1);			// Changeover when all sent
					TNC->Streams[0].BytesTXed += CTEXTLEN;
				}
				return;

			}
			else
			{
				// Connect Complete
			
				buffptr = GetBuff();
				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Connected to %s\r", Call);;

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
	
				STREAM->Connecting = FALSE;
				STREAM->Connected = TRUE;			// Subsequent data to data channel

				
				if (Stream == 0)
				{
					if (TNC->RIG)
						sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound Freq %s", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall, TNC->RIG->Valchar);
					else
						sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);

					SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

					UpdateMH(TNC, Call, '+', 'O');

				}

				return;
			}
		}
	}
}


static int	KissDecode(UCHAR * inbuff, UCHAR * outbuff, int len)
{
	int i,txptr=0;
	UCHAR c;

	for (i=0;i<len;i++)
	{
		c=inbuff[i];

		if (c == FESC)
		{
			c=inbuff[++i];
			{
				if (c == TFESC)
					c=FESC;
				else
				if (c == TFEND)
					c=FEND;
			}
		}

		outbuff[txptr++]=c;
	}

	return txptr;

}

VOID EncodeAndSend(struct TNCINFO * TNC, UCHAR * txbuffer, int Len)
{
	// Send A Packet With KISS Encoding

	TNC->TXLen = KissEncode(txbuffer, TNC->TXBuffer, Len);
	WriteCommBlock(TNC);
}

static int	KissEncode(UCHAR * inbuff, UCHAR * outbuff, int len)
{
	int i,txptr=0;
	UCHAR c;

	outbuff[0]=FEND;
	txptr=1;

	for (i=0;i<len;i++)
	{
		c=inbuff[i];
		
		switch (c)
		{
		case FEND:
			outbuff[txptr++]=FESC;
			outbuff[txptr++]=TFEND;
			break;

		case FESC:

			outbuff[txptr++]=FESC;
			outbuff[txptr++]=TFESC;
			break;

		default:

			outbuff[txptr++]=c;
		}
	}

	outbuff[txptr++]=FEND;

	return txptr;

}

static VOID DoMonitor(struct TNCINFO * TNC, UCHAR * Msg, int Len)
{
	// Update HM list and maube pass to monitor somehow

	UCHAR * ptr;

	ptr = strchr(&Msg[3], '>');

	if (ptr) *(ptr) = 0;

	UpdateMH(TNC, &Msg[3], ' ', 0);

}
VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
	if (Stream == 0)					// Pactor Stream
	{
		TNC->TimeInRX = 0;
		if (TNC->HFPacket)
			EncodeAndSend(TNC, "C2AD", 4);		// Disconnect
		else
			EncodeAndSend(TNC, "X", 1);			// ??Return to packet mode??
//			EncodeAndSend(TNC, "C20TOR", 6);		// Disconnect

		TNC->HFPacket = FALSE;
	}
	else
	{
		UCHAR TXMsg[10];

		sprintf(TXMsg, "C1%cD", Stream + '@');
		EncodeAndSend(TNC, TXMsg, 4);
		TNC->Timeout = 50;
	}
}

VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	if (Stream == 0)					// Pactor Stream
	{
		TNC->TimeInRX = 0;

		if (TNC->HFPacket)
			EncodeAndSend(TNC, "C2AD", 4);		// Disconnect
		else
			EncodeAndSend(TNC, "X", 1);			// ??Return to packet mode??

		TNC->HFPacket = FALSE;
	}
	else
	{
		UCHAR TXMsg[10];

		sprintf(TXMsg, "C1%cD", Stream + '@');
		EncodeAndSend(TNC, TXMsg, 4);		// Send twice - must force a disconnect
		TNC->Timeout = 50;
	}}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
		TNC->NeedPACTOR = 50;	
}



/*


MARK 1400
SPACE 1600
SHIFT MODEM
INV ON
MAXUSERS 1/10
PTERRS 30

	// Others go on end so they can't be overriden

ECHO OFF
XMITECHO ON
TXFLOW OFF
XFLOW OFF
TRFLOW OFF
AUTOCR 0
AUTOLF OFF
CRADD OFF
CRSUP OFF
CRSUP OFF/OFF
LFADD OFF/OFF
LFADD OFF
LFSUP OFF/OFF
LFSUP OFF
RING OFF
ARQBBS OFF

MYCALL 

*/
