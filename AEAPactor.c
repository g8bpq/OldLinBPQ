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
//	DLL to inteface AEA/Timewave TNC in Pactor Mode to BPQ32 switch 
//
//	Uses BPQ EXTERNAL interface
//

// Version 1.1.1.2 Sept 2010

// Fix CTEXT
// Turn round link when all acked (not all sent)

// Version 1.1.1.3 Sept 2010

// Turn round link if too long in receive

// Version 1.1.1.4 September 2010

// Fix Freq Display after Node reconfig
// Only use AutoConnect APPL for Pactor Connects

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include "time.h"

//#include <process.h>
//#include <time.h>

#include "CHeaders.h"
#include "tncinfo.h"

#include "bpq32.h"


static char ClassName[]="AEAPACTORSTATUS";

static char WindowTitle[] = "AEA Pactor";
static int RigControlRow = 165;

#define	SOH	0x01	// CONTROL CODES 
#define	ETB	0x17
#define	DLE	0x10

#define MaxStreams 26

#define PTOVER 0x1a					// Pactor Turnround Char

char OverMsg[3] = " \x1a";

static RECT Rect;

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd



static char status[8][8] = {"STANDBY",  "PHASING", "CHGOVER", "IDLE", "TRAFFIC", "ERROR", "RQ", "XXXX"};

struct TNCINFO * CreateTTYInfo(int port, int speed);
BOOL OpenConnection(int);
BOOL SetupConnection(int);
BOOL CloseConnection(struct TNCINFO * conn);
static BOOL WriteCommBlock(struct TNCINFO * TNC);
BOOL DestroyTTYInfo(int port);
static void CheckRX(struct TNCINFO * TNC);
VOID AEAPoll(int Port);
static VOID ProcessDEDFrame(struct TNCINFO * TNC, UCHAR * rxbuff, int len);
static VOID ProcessTermModeResponse(struct TNCINFO * TNC);
static VOID DoTNCReinit(struct TNCINFO * TNC);
static VOID DoTermModeTimeout(struct TNCINFO * TNC);

VOID ProcessPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID ProcessKPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
static VOID ProcessAEAPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID ProcessKNormCommand(struct TNCINFO * TNC, UCHAR * rxbuffer);
static VOID ProcessHostFrame(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);

//	Note that AEA host Mode uses SOH/ETB delimiters, with DLE stuffing

static VOID EncodeAndSend(struct TNCINFO * TNC, UCHAR * txbuffer, int Len);
static int	DLEEncode(UCHAR * inbuff, UCHAR * outbuff, int len);
static int	DLEDecode(UCHAR * inbuff, UCHAR * outbuff, int len);

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
	if (_stricmp(buf, "APPL") == 0)			// Using BPQ32 COnfig
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
			
			if (_memicmp(buf, "WL2KREPORT", 10) == 0)
				TNC->WL2K = DecodeWL2KReportLine(buf);
			else
				strcat (TNC->InitScript, buf);
		}
	}

	return (TRUE);
}



static int ExtProc(int fn, int port,unsigned char * buff)
{
	int txlen = 0;
	UINT * buffptr;
	struct TNCINFO * TNC = TNCInfo[port];
	struct STREAMINFO * STREAM;
	int Stream;

	if (TNC == NULL || TNC->hDevice == 0)
		return 0;							// Port not open

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
			if (TNC->Streams[Stream].ReportDISC)
			{
				TNC->Streams[Stream].ReportDISC = FALSE;
				buff[4] = Stream;

				return -1;
			}
		}

		CheckRX(TNC);
		AEAPoll(port);

		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			if (TNC->Streams[Stream].PACTORtoBPQ_Q !=0)
			{
				int datalen;
			
				buffptr=Q_REM(&TNC->Streams[Stream].PACTORtoBPQ_Q);

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
			
		return 0;

	case 2:				// send

		buffptr = GetBuff();

		if (buffptr == 0) return (0);			// No buffers, so ignore

		// Find TNC Record

		Stream = buff[4];
		
		if (!TNC->TNCOK)
		{
			// Send Error Response

			buffptr[1] = 36;
			memcpy(buffptr+2, "No Connection to PACTOR TNC\r", 36);

			C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
			
			return 0;
		}

		txlen = GetLengthfromBuffer(buff) - 8;

		buffptr[1] = txlen;
		memcpy(buffptr+2, &buff[8], txlen);
		
		C_Q_ADD(&TNC->Streams[Stream].BPQtoPACTOR_Q, buffptr);

		if(TNC->Streams[Stream].Connected)
		{
			TNC->Streams[Stream].FramesQueued++;
		}
		return (0);


	case 3:				// CHECK IF OK TO SEND. Also used to check if TNC is responding
		
		Stream = (int)buff;
		STREAM = &TNC->Streams[Stream];

		if (STREAM->FramesQueued  > 4)
			return 1 | TNC->HostMode << 8 | STREAM->Disconnecting << 15;	// Busy
	
		return TNC->HostMode << 8 | STREAM->Disconnecting << 15;		// OK, but lock attach if disconnecting;		// OK

	case 4:				// reinit

		return (0);

	case 5:				// Close

		EncodeAndSend(TNC, "OHON", 4);		// HOST N
		Sleep(50);

		SaveWindowPos(port);

		CloseCOMPort(TNC->hDevice);

		return (0);

	case 6:				// Scan Control

		return 0;		// None Yet

	}
	return 0;

}


UINT AEAExtInit(EXTPORTDATA *  PortEntry)
{
	char msg[500];
	struct TNCINFO * TNC;
	int port;
	char * TempScript;
	char * ptr;

	//
	//	Will be called once for each Pactor Port
	//	The COM port number is in IOBASE
	//

	sprintf(msg,"AEA Pactor %s", PortEntry->PORTCONTROL.SerialPortName);
	WritetoConsole(msg);

	port=PortEntry->PORTCONTROL.PORTNUMBER;

	ReadConfigFile(port, ProcessLine);

	TNC = TNCInfo[port];

	if (TNC == NULL)
	{
		// Not defined in Config file

		sprintf(msg," ** Error - no info in BPQ32.cfg for this port\n");
		WritetoConsole(msg);

		return (int)ExtProc;
	}

	TNC->Port = port;

	TNC->Hardware = H_AEA;

	TNC->TEXTMODE = FALSE;

	PortEntry->MAXHOSTMODESESSIONS = 11;		// Default

	TNC->InitScript = _strupr(TNC->InitScript);

	TNC->PortRecord = PortEntry;

	if (PortEntry->PORTCONTROL.PORTCALL[0] == 0)
	{
		memcpy(TNC->NodeCall, MYNODECALL, 10);
	}
	else
	{
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);
	}

	TNC->Interlock = PortEntry->PORTCONTROL.PORTINTERLOCK;

	PortEntry->PORTCONTROL.PROTOCOL = 10;
	PortEntry->PORTCONTROL.PORTQUALITY = 0;
	PortEntry->SCANCAPABILITIES = NONE;		// No Scan Interlock 

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0)
		PortEntry->PORTCONTROL.PORTPACLEN = 100;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	// Set Essential Params and MYCALL

	TempScript = malloc(4000);

	strcpy(TempScript, "RESTART\r");
	strcat(TempScript, "EXPERT ON\r");
	strcat(TempScript, "PTHUFF 0\r");
	strcat(TempScript, "PT200 ON\r");
	strcat(TempScript, "WIDESHFT OFF\r");
	strcat(TempScript, "ARQT 30\r");

	strcat(TempScript, TNC->InitScript);

	free(TNC->InitScript);
	TNC->InitScript = TempScript;

	// Others go on end so they can't be overriden

	strcat(TNC->InitScript, "XMITOK ON\r");
	strcat(TNC->InitScript, "XFLOW OFF\r");
	strcat(TNC->InitScript, "RXREV OFF\r");
	strcat(TNC->InitScript, "FLOW OFF\r");
	strcat(TNC->InitScript, "AWLEN 8\r");
	strcat(TNC->InitScript, "AUTOBAUD OFF\r");
	strcat(TNC->InitScript, "8BITCONV ON\r");
	strcat(TNC->InitScript, "ALFPAC OFF\r");
	strcat(TNC->InitScript, "ALFDISP OFF\r");
	strcat(TNC->InitScript, "ACRRTTY 0\r");
	strcat(TNC->InitScript, "HPOLL ON\r");
	strcat(TNC->InitScript, "EAS ON\r\r");
	strcat(TNC->InitScript, "CONMODE TRANS\r");
	strcat(TNC->InitScript, "PTOVER $1A\r\r");

	// Set the ax.25 MYCALL

	sprintf(msg, "MYCALL %s\r", TNC->NodeCall);
	strcat(TNC->InitScript, msg);

#ifndef LINBPQ

	CreatePactorWindow(TNC, ClassName, WindowTitle, RigControlRow, PacWndProc, 0, 0);

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
 
	CreateWindowEx(0, "STATIC", "Buffers", WS_CHILD | WS_VISIBLE,10,116,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_BUFFERS = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE,116,116,374,20 , TNC->hDlg, NULL, hInstance, NULL);
	
	CreateWindowEx(0, "STATIC", "Traffic", WS_CHILD | WS_VISIBLE,10,138,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TRAFFIC = CreateWindowEx(0, "STATIC", "RX 0 TX 0", WS_CHILD | WS_VISIBLE,116,138,374,20 , TNC->hDlg, NULL, hInstance, NULL);

	TNC->ClientHeight = 213;
	TNC->ClientWidth = 500;

	TNC->hMenu = CreateMenu();
	TNC->hWndMenu = CreatePopupMenu();
	
	MoveWindows(TNC);
#endif

	OpenCOMMPort(TNC, PortEntry->PORTCONTROL.SerialPortName, PortEntry->PORTCONTROL.BAUDRATE, FALSE);

	WritetoConsole("\n");

	return ((int)ExtProc);
}

static void CheckRX(struct TNCINFO * TNC)
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

	// AEA uses SOH ETX Framing

	if (TNC->RXBuffer[0] != SOH)
	{
		// Char Mode Frame I think we need to see cmd: on end

		// If we think we are in host mode, then to could be noise - just discard.

		if (TNC->HostMode)
		{
			Debugprintf("AEA Bad Host Frame");
			TNC->RXLen = 0;		// Ready for next frame
			return;
		}

		TNC->RXBuffer[TNC->RXLen] = 0;

//		if (TNC->RXBuffer[TNC->RXLen-2] != ':')
		if (strstr(TNC->RXBuffer, "cmd:") == 0)
			return;				// Wait for rest of frame

		// Complete Char Mode Frame

		TNC->RXLen = 0;		// Ready for next frame
					
		ProcessTermModeResponse(TNC);
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

	if (TNC->RXBuffer[Length-1] != ETB)
		return;					// Wait till we have a full frame

	if (TNC->RXBuffer[Length-2] == DLE && TNC->RXBuffer[Length-3] != DLE)
		return;					// ??? DLE ETB isn't end of frame, but DLE DLE ETB is

	ProcessHostFrame(TNC, TNC->RXBuffer, Length);	// Could have multiple packets in buffer

	TNC->RXLen = 0;		// Ready for next frame

		
	return;

}

static VOID ProcessHostFrame(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len)
{
	UCHAR * FendPtr;
	int NewLen;

	//	Split into Packets. By far the most likely is a single packet, so treat as special case
	//  Beware of DLE ETB and DLE DLE ETB!

	FendPtr = memchr(&rxbuffer[1], ETB, Len-1);

FENDLoop:

	if (*(FendPtr - 1) == DLE && *(FendPtr - 2) != DLE)
	{
		FendPtr++;
		FendPtr = memchr(FendPtr, ETB, Len-1);
		goto FENDLoop;
	}
	
	if (FendPtr == &rxbuffer[Len-1])
	{
		ProcessAEAPacket(TNC, &rxbuffer[1], Len - 2);
		return;
	}
		
	// Process the first Packet in the buffer

	NewLen =  FendPtr - rxbuffer -1;

	ProcessAEAPacket(TNC, &rxbuffer[1], NewLen);
	
	// Loop Back

	ProcessHostFrame(TNC, FendPtr+1, Len - NewLen - 2);
	return;

}



static BOOL WriteCommBlock(struct TNCINFO * TNC)
{
	WriteCOMBlock(TNC->hDevice, TNC->TXBuffer, TNC->TXLen);
	TNC->Timeout = 50;
	return TRUE;
}

VOID AEAPoll(int Port)
{
	struct TNCINFO * TNC = TNCInfo[Port];
	struct STREAMINFO * STREAM;
	UCHAR * Poll = TNC->TXBuffer;
	char Status[80];
	int Stream;

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

		TNC->TNCOK = FALSE;
		TNC->HostMode = 0;
		TNC->ReinitState = 0;
				
		sprintf(Status,"%s Open but TNC not responding", TNC->PortRecord->PORTCONTROL.SerialPortName);
		SetWindowText(TNC->xIDC_COMMSSTATE, Status);

		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			if (TNC->PortRecord->ATTACHEDSESSIONS[Stream])		// Connected
			{
				TNC->Streams[Stream].Connected = FALSE;		// Back to Command Mode
				TNC->Streams[Stream].ReportDISC = TRUE;		// Tell Nod
			}
		}
	}

	// if we have just restarted or TNC appears to be in terminal mode, run Initialisation Sequence

	if (!TNC->HostMode)
	{
		DoTNCReinit(TNC);
		return;
	}	

	if (TNC->CommandBusy)
		goto Poll;

	// We don't check for a new attach unless Timeout and CommandBusy are both zero, as we need to send a command.

	// If Pactor Session has just been attached, set Pactor Call to the connecting user's callsign


	if (TNC->PortRecord->ATTACHEDSESSIONS[0] && TNC->Streams[0].Attached == 0)
	{
		// New Attach

		int calllen;
		UCHAR TXMsg[1000];
		int datalen;
		char Msg[80];

		TNC->Streams[0].Attached = TRUE;
		TNC->Streams[0].TimeInRX = 0;

		calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[0]->L4USER, TNC->Streams[0].MyCall);
		TNC->Streams[0].MyCall[calllen] = 0;
		
		datalen = sprintf(TXMsg, "OMf%s", TNC->Streams[0].MyCall);
		EncodeAndSend(TNC, TXMsg, datalen);
		TNC->InternalCmd = 'M';
		TNC->CommandBusy = TRUE;

		sprintf(Status, "In Use by %s", TNC->Streams[0].MyCall);
		SetWindowText(TNC->xIDC_TNCSTATE, Status);

		// Stop Scanning

		sprintf(Msg, "%d SCANSTOP", TNC->Port);
		
		Rig_Command(-1, Msg);

		// Shouldn't we also take out of standby mode?? PN is Pactor Listen, for monitoring

		return;

	}
	
	//If sending internal command list, send next element

	if (TNC->CmdSet)
	{
		char * start, * end;
		int len;

		start = TNC->CmdSet;
		
		if (*(start) == 0)			// End of Script
		{
			free(TNC->CmdSave);
			TNC->CmdSet = NULL;
		}
		else
		{
			end = strchr(start, 13);
			len = ++end - start -1;	// exclude cr
			TNC->CmdSet = end;
		
			EncodeAndSend(TNC, start, len);
			TNC->InternalCmd = 'X';
			TNC->CommandBusy = TRUE;

			return;
		}
	}

	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		STREAM = &TNC->Streams[Stream];

		if (STREAM->Attached)
			CheckForDetach(TNC, Stream, STREAM, TidyClose, ForcedClose, CloseComplete);
	}

	if (TNC->NeedPACTOR)
	{
		TNC->NeedPACTOR--;

		if (TNC->NeedPACTOR == 0)
		{
			EncodeAndSend(TNC, "OPA", 3);	// ??Return to packet mode??
			TNC->CommandBusy = TRUE;

			TNC->CmdSet = TNC->CmdSave = malloc(100);

			sprintf(TNC->CmdSet, "OMf%s\rOPt\rOCETRANS\r", TNC->NodeCall);  // Queue Back to Pactor Standby
			TNC->InternalCmd = 'T';
			TNC->TEXTMODE = FALSE;
			TNC->IntCmdDelay--;

			// Restart Scanning

			sprintf(Status, "%d SCANSTART 15", TNC->Port);
		
			Rig_Command(-1, Status);

			return;
		}
	}

	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		if (TNC->TNCOK && TNC->Streams[Stream].BPQtoPACTOR_Q && TNC->DataBusy == FALSE)
		{
			int datalen;
			UCHAR TXMsg[1000];
			int * buffptr;
			UCHAR * MsgPtr;
			char Status[80];
			
			if (TNC->Streams[Stream].Connected)
			{
				if (Stream == 0)
				{
					// Limit amount in TX

					if (TNC->Streams[0].BytesTXed - TNC->Streams[0].BytesAcked > 200)
						continue;

					// If in IRS state for too long, force turnround

					if (TNC->TXRXState == 'R')
					{
						if (TNC->Streams[0].TimeInRX++ > 15)
						{
							EncodeAndSend(TNC, "OAG", 3);
							TNC->InternalCmd = 'A';
							TNC->CommandBusy = TRUE;
						}
						else
							goto GetStatus;
					}
					TNC->Streams[0].TimeInRX = 0;
				}

				buffptr=Q_REM(&TNC->Streams[Stream].BPQtoPACTOR_Q);
				TNC->Streams[Stream].FramesQueued--;
				datalen=buffptr[1];
				MsgPtr = (UCHAR *)&buffptr[2];

				if (TNC->SwallowSignon)
				{
					TNC->SwallowSignon = FALSE;	
					if (strstr(MsgPtr, "Connected"))	// Discard *** connected
					{
						ReleaseBuffer(buffptr);
						return;
					}
				}

				// If in CONV, and data looks binary, switch to TRAN

				TNC->NeedTurnRound = TRUE;		// Sending data, so need turnround at end

				if (TNC->TEXTMODE)
				{
					int i;
					UCHAR j;

					for (i = 0; i < datalen; i++)
					{
						j = MsgPtr[i];

						if (j > 127 ||  j == 26 || j < 10)
						{
							TNC->TEXTMODE = FALSE;
							EncodeAndSend(TNC, "OCETRANS", 8);
							Debugprintf("Switching to TRANS");
							TNC->CommandBusy = TRUE;
							TNC->InternalCmd = 'A';	
							break;
						}
					}
				}

				sprintf(TXMsg, "%c", Stream + ' ');
					
				memcpy(&TXMsg[1], buffptr + 2, datalen);
				
				EncodeAndSend(TNC, TXMsg, datalen + 1);
				ReleaseBuffer(buffptr);
				TNC->Streams[Stream].BytesTXed += datalen; 
				Debugprintf("Stream %d Sending %d, BytesTXED now %d", Stream, datalen, TNC->Streams[Stream].BytesTXed);
				TNC->Timeout = 0;
				TNC->DataBusy = TRUE;

				if (Stream == 0)
					ShowTraffic(TNC);

				if (STREAM->Disconnecting && TNC->Streams[Stream].BPQtoPACTOR_Q == 0)
					TidyClose(TNC, 0);

				return;
			}
			else
			{
				buffptr=Q_REM(&TNC->Streams[Stream].BPQtoPACTOR_Q);
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
						C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
					}
					return;
				}

				if (memcmp(MsgPtr, "MODE CONV", 9) == 0)
				{
					TNC->TEXTMODE = TRUE;
					buffptr[1] = sprintf((UCHAR *)&buffptr[2],"AEA} Ok\r");
					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

					EncodeAndSend(TNC, "OCECONV", 7);
					TNC->CommandBusy = TRUE;

					return;
				}

				if (memcmp(MsgPtr, "MODE TRANS", 9) == 0)
				{
					TNC->TEXTMODE = FALSE;
					buffptr[1] = sprintf((UCHAR *)&buffptr[2],"AEA} Ok\r");
					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

					EncodeAndSend(TNC, "OCETRANS", 8);
					TNC->CommandBusy = TRUE;

					return;
				}

				if (MsgPtr[0] == 'C' && MsgPtr[1] == ' ' && datalen > 2)	// Connect
				{
					memcpy(TNC->Streams[Stream].RemoteCall, &MsgPtr[2], 9);
					TNC->Streams[Stream].Connecting = TRUE;

					// If Stream 0, Convert C CALL to PACTOR CALL

					if (Stream == 0)
					{
						datalen = sprintf(TXMsg, "%cPG%s", Stream + '@', TNC->Streams[0].RemoteCall);
						sprintf(Status, "%s Connecting to %s",
							TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
						SetWindowText(TNC->xIDC_TNCSTATE, Status);
					}
					else
						datalen = sprintf(TXMsg, "%cCO %s", Stream + '@', TNC->Streams[Stream].RemoteCall);

					EncodeAndSend(TNC, TXMsg, datalen);
					TNC->InternalCmd = 'C';			// So we dont send the reply to the user.
					ReleaseBuffer(buffptr);
					TNC->Streams[Stream].Connecting = TRUE;

					return;
				}

				if (memcmp(MsgPtr, "DISCONNECT", datalen) == 0)	// Disconnect
				{
					if (Stream == 0)
					{
						EncodeAndSend(TNC, "ODI", 3);			// ??Return to packet mode??
						TNC->NeedPACTOR = 50;
						TNC->CommandBusy = TRUE;
					}
					else
					{
						sprintf(TXMsg, "%cDI", Stream + '@');
						EncodeAndSend(TNC, TXMsg, 3);
						TNC->CmdStream = Stream;
					}

					TNC->Streams[Stream].Connecting = FALSE;
					TNC->Streams[Stream].ReportDISC = TRUE;
					ReleaseBuffer(buffptr);

					return;
				}
	
				// Other Command ??

				if (Stream > 0)
					datalen = sprintf(TXMsg, "C20%s", MsgPtr);
				else
					datalen = sprintf(TXMsg, "O%s", MsgPtr);

				EncodeAndSend(TNC, TXMsg, datalen);
				ReleaseBuffer(buffptr);
				TNC->InternalCmd = 0;
				TNC->CommandBusy = TRUE;
				TNC->CmdStream = Stream;
			}
		}
	}

GetStatus:

	// Need to poll data and control channel (for responses to commands)

	// Also check status if we have data buffered (for flow control)

	if (TNC->TNCOK)
	{
		if (TNC->IntCmdDelay <= 0)
		{
			EncodeAndSend(TNC, "OOP", 3);
			TNC->InternalCmd = 'S';
			TNC->CommandBusy = TRUE;
			TNC->IntCmdDelay = 9;	// Every second
			return;
		}
		else
			TNC->IntCmdDelay--;
	}

Poll:
	// Nothing doing - send Poll (but not too often)

	TNC->PollDelay++;

	if (TNC->PollDelay < 3)
		return;

	TNC->PollDelay = 0;

	EncodeAndSend(TNC, "OGG", 3);			// Poll

	return;
}

static VOID DoTNCReinit(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 1)		// Forcing back to Term
		TNC->ReinitState = 0;		// Got Response, so must be back in term mode

	if (TNC->ReinitState == 0)
	{
		// Just Starting - Send a CR to see if in Terminal or Host Mode

		char Status[80];
		
		sprintf(Status,"%s Initialising TNC", TNC->PortRecord->PORTCONTROL.SerialPortName);
		SetWindowText(TNC->xIDC_COMMSSTATE, Status);

		Poll[0] = 13;
		TNC->TXLen = 1;

		WriteCommBlock(TNC);

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


			Poll[0] = 0x11;		// XON
			Poll[1] = 0x18;		// CAN
			Poll[2] = 0x03;		// COM

			memcpy(&Poll[3], "HOST Y\r", 7);

			TNC->TXLen = 10;
			WriteCommBlock(TNC);
			TNC->Timeout = 0;
			TNC->CommandBusy = FALSE;
			TNC->DataBusy = FALSE;

			TNC->HostMode = TRUE;		// Should now be in Host Mode
			TNC->NeedPACTOR = 50;		// Need to Send PACTOR command after 5 secs

			return;
		}
		
		end = strchr(start, 13);
		len = ++end - start;
		TNC->InitPtr = end;
		memcpy(Poll, start, len);

		TNC->TXLen = len;
		WriteCommBlock(TNC);


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

		EncodeAndSend(TNC, "OHON", 4);		// HOST N
		TNC->Timeout = 20;
		return;
	}
	if (TNC->ReinitState == 1)
	{
		// Forcing back to Term Mode

		TNC->ReinitState = 0;
		DoTNCReinit(TNC);				// See if worked
		return;
	}
}

static VOID ProcessTermModeResponse(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	Debugprintf("AEA Initstate %d Response %s", TNC->ReinitState, TNC->RXBuffer);

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
}

static VOID ProcessAEAPacket(struct TNCINFO * TNC, UCHAR * Msg, int Len)
{
	UINT * buffptr;
	char * Buffer = &Msg[1];			// Data portion of frame
	char * Call;
	char Status[80];
	int Stream = 0;
	int Opcode;
	struct STREAMINFO * STREAM;

	// Any valid frame is an ACK

	TNC->TNCOK = TRUE;
	TNC->Timeout = 0;

	Len = DLEDecode(Msg, Msg, Len);		// Remove KISS transparency

	Stream = Msg[0] & 15;
	Opcode = Msg[0] >> 4;

	STREAM = &TNC->Streams[Stream];

	if (Msg[0] == 'O' && Msg[1] == 'G' && Msg[2] == 'G')
	{
		if (Msg[3] == 0)
		{
			// OK Response

			sprintf(Status,"%s TNC link OK", TNC->PortRecord->PORTCONTROL.SerialPortName);
			SetWindowText(TNC->xIDC_COMMSSTATE, Status);

			return;
		}
	}

	if (Msg[0] == '/')			// 2F
	{
		char * Eptr;
		
		// Echoed Data

		Len--;

		Eptr = memchr(Buffer, 0x1c, Len);

		if (Eptr) 
			Debugprintf("Echoed 1c followed by %x", *(++Eptr));

		TNC->Streams[0].BytesAcked += Len;

		Debugprintf("Ack for %d, BytesAcked now %d", Len, TNC->Streams[0].BytesAcked);
		ShowTraffic(TNC);

		// If nothing more to send, turn round link
						
		if ((TNC->Streams[0].BPQtoPACTOR_Q == 0) && TNC->NeedTurnRound &&
			(TNC->Streams[0].BytesAcked >= TNC->Streams[0].BytesTXed))		// Nothing following and all acked
			{
				Debugprintf("AEA Sent = Acked - sending Turnround");
						
				if (TNC->TEXTMODE == 0)
				{
					// In Trans - switch back

					TNC->TEXTMODE = TRUE;
					EncodeAndSend(TNC, "OCECONV", 7);
					Debugprintf("Switching to CONV");
					TNC->CommandBusy = TRUE;
					TNC->InternalCmd = 'A';	
					TNC->NeedTRANS = TRUE;
				}
					
				TNC->NeedTurnRound = FALSE;
				EncodeAndSend(TNC, OverMsg, 2);
				TNC->Timeout = 0;						//  No response to data
		}
		return;
	}

	if (Opcode == 3)
	{
		// Received Data

		// Pass to Appl

		buffptr = GetBuff();
		if (buffptr == NULL) return;	// No buffers, so ignore

		Len--;							// Remove Header

		buffptr[1] = Len;				// Length
		TNC->Streams[Stream].BytesRXed += Len;
		memcpy(&buffptr[2], Buffer, Len);
		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

		if (Stream == 0)
			ShowTraffic(TNC);

		return;
	}


	if (Opcode == 4)
	{
		// Link Status or Command Response

		TNC->CommandBusy = FALSE;

		if (TNC->InternalCmd)
		{
			// Process it

			if (TNC->InternalCmd == 'S')		// Status
			{
//				if (Msg[3] == 'P' && Msg[4] == 'G')
				{
					SetWindowText(TNC->xIDC_STATE, status[Msg[5] - 0x30]);
					
					TNC->TXRXState = Msg[6];

					if (Msg[6] == 'S')
						SetWindowText(TNC->xIDC_TXRX, "Sender");
					else
						SetWindowText(TNC->xIDC_TXRX, "Receiver");

					Msg[12] = 0;
					SetWindowText(TNC->xIDC_MODE, Msg);

					// Testing.. I think ZF returns buffers

					EncodeAndSend(TNC, "OZF", 3);
					TNC->InternalCmd = 'Z';
					TNC->CommandBusy = TRUE;
				}
			
				return;
			}

			if (TNC->InternalCmd == 'Z')		// Buffers?
			{
				Msg[Len] = 0;
				SetWindowText(TNC->xIDC_BUFFERS, &Msg[3]);
				return;
			}

			return;
		}

		// Reply to Manual command - Pass to Appl

		Stream = TNC->CmdStream;
		STREAM = &TNC->Streams[Stream];


		buffptr = GetBuff();

		if (buffptr == NULL) return;			// No buffers, so ignore

		Buffer[Len - 1] = 13;
		Buffer[Len] = 0;

		buffptr[1] = sprintf((UCHAR *)&buffptr[2],"AEA} %s", Buffer);

		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

		return;

	}

	if (Opcode == 5)
	{
		// Link Messages (Connect, etc)

		if (Stream == 15)
		{
			// SOH $5F X X $00 ETB data acknowledgement 

			if (Msg[1] == 'X' && Msg[2] == 'X' && Msg[3] == 0)
			{
				TNC->DataBusy = FALSE;

				if (TNC->NeedTRANS)		// Sent CTRL/Z in conv mode - switch back to trans
				{
					TNC->NeedTRANS = FALSE;
					TNC->TEXTMODE = FALSE;
					EncodeAndSend(TNC, "OCETRANS", 8);
					Debugprintf("Switching to TRANS");
					TNC->CommandBusy = TRUE;
					TNC->InternalCmd = 'A';	
				}
				else
					EncodeAndSend(TNC, "OGG", 3);			// Send another Poll
			}
			return;
		}

		if (strstr(Buffer, "DISCONNECTED") || strstr(Buffer, "Timeout"))
		{
			if ((TNC->Streams[Stream].Connecting | TNC->Streams[Stream].Connected) == 0)
			{
				// Not connected or Connecting. Probably response to going into Pactor Listen Mode

				return;
			}
	
			if (STREAM->Connecting && STREAM->Disconnecting == FALSE)
			{
				// Connect Failed
			
				buffptr = GetBuff();
				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Failure with %s\r", TNC->Streams[Stream].RemoteCall);

				C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
	
				TNC->Streams[Stream].Connecting = FALSE;
				TNC->Streams[Stream].Connected = FALSE;				// In case!
				TNC->Streams[Stream].FramesQueued = 0;

				return;
			}

			// Connected, or Disconnecting - Release Session

			TNC->Streams[Stream].Connecting = FALSE;
			TNC->Streams[Stream].Connected = FALSE;		// Back to Command Mode
			TNC->Streams[Stream].FramesQueued = 0;

			if (STREAM->Disconnecting == FALSE)
				STREAM->ReportDISC = TRUE;		// Tell Node

			STREAM->Disconnecting = FALSE;

			// Need to reset Pactor Call in case it was changed

			TNC->NeedPACTOR = 20;

			if (Stream == 0)
			{
				// Claar any data from buffers 

				EncodeAndSend(TNC, "OTC", 3);			// Clear buffers
				TNC->NeedPACTOR = 20;
				TNC->InternalCmd = 'T';
				TNC->CommandBusy = TRUE;
			}

			return;
		}

		Call = strstr(Buffer, "NNECTED to");

		if (Call)
		{	
			Call+=11;					// To Callsign
			
			if (Stream == 0)
			{
				Buffer[Len-2] = 0;
			}

			TNC->Streams[Stream].BytesRXed = TNC->Streams[Stream].BytesTXed = TNC->Streams[Stream].BytesAcked = 0;
			TNC->Streams[Stream].ConnectTime = time(NULL); 

			if (Stream == 0)
			{
				// Stop Scanner

				char Msg[80];
				
				sprintf(Msg, "%d SCANSTOP", TNC->Port);

				Rig_Command(-1, Msg);

				ShowTraffic(TNC);

			}

			if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] == 0)
			{
				// Incoming Connect

				ProcessIncommingConnect(TNC, Call, Stream, TRUE);

				if (Stream == 0)
				{
					struct WL2KInfo * WL2K = TNC->WL2K;
					char FreqAppl[10] = "";				// Frequecy-specific application
	
					if (TNC->RIG && TNC->RIG != &TNC->DummyRig)
					{
						// If Scan Entry has a Appl, save it

						if (TNC->RIG->FreqPtr[0]->APPL[0])
							strcpy(FreqAppl, &TNC->RIG->FreqPtr[0]->APPL[0]);
					}

					// We are going to Send something, so turn link round
				
					EncodeAndSend(TNC, "OAG", 3);
					TNC->InternalCmd = 'A';
					TNC->CommandBusy = TRUE;
				
					sprintf(Status, "%s Connected to %s Inbound", TNC->Streams[0].RemoteCall, TNC->NodeCall);
					SetWindowText(TNC->xIDC_TNCSTATE, Status);

					// If an autoconnect APPL is defined, send it

					if (FreqAppl[0])				// Frequency spcific APPL overrides TNC APPL
					{
						buffptr = GetBuff();
						if (buffptr == 0) return;	// No buffers, so ignore

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
						C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
												
						TNC->SwallowSignon = TRUE;

						return;
					}
				}

				if (FULL_CTEXT && HFCTEXTLEN == 0)
				{
					char CTBuff[300];
					int Len = CTEXTLEN, CTPaclen = 50;
					int Next = 0;

					CTBuff[0] = Stream + ' ';

					while (Len > CTPaclen)		// CTEXT Paclen
					{
						memcpy(&CTBuff[1], &CTEXTMSG[Next], CTPaclen);
						EncodeAndSend(TNC, CTBuff, CTPaclen + 1);
						Next += CTPaclen;
						Len -= CTPaclen;
					}

					memcpy(&CTBuff[1], &CTEXTMSG[Next], Len);
					EncodeAndSend(TNC, CTBuff, Len + 1);
				}
				return;

			}
			else
			{
				// Connect Complete
			
				buffptr = GetBuff();
				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Connected to %s\r", Call);;

				C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
	
				TNC->Streams[Stream].Connecting = FALSE;
				TNC->Streams[Stream].Connected = TRUE;			// Subsequent data to data channel
	
				if (Stream == 0)
				{
					sprintf(Status, "%s Connected to %s Outbound", TNC->NodeCall, TNC->Streams[0].RemoteCall);
					SetWindowText(TNC->xIDC_TNCSTATE, Status);
					UpdateMH(TNC, Call, '+', 'O');
				}

				return;
			}
		}
	}	
}


int	DLEDecode(UCHAR * inbuff, UCHAR * outbuff, int len)
{
	int i,txptr=0;
	UCHAR c;

	for (i=0; i<len; i++)
	{
		c = inbuff[i];

		if (c == DLE)
			c = inbuff[++i];

		outbuff[txptr++] = c;
	}

	return txptr;

}

static VOID EncodeAndSend(struct TNCINFO * TNC, UCHAR * txbuffer, int Len)
{
	// Send A Packet With DLE Encoding Encoding

	TNC->TXLen = DLEEncode(txbuffer, TNC->TXBuffer, Len);

	WriteCommBlock(TNC);
}

static int DLEEncode(UCHAR * inbuff, UCHAR * outbuff, int len)
{
	int i, txptr = 0;
	UCHAR c;

	outbuff[0] = SOH;
	txptr=1;

	for (i=0; i<len; i++)
	{
		c = inbuff[i];
		
		switch (c)
		{
		case DLE:
		case SOH:
		case ETB:

			outbuff[txptr++] = DLE;
//			Debugprintf("Escaping %x", c);
		}
		
		outbuff[txptr++] = c;
	}

	outbuff[txptr++] = ETB;

	return txptr;

}


VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
	if (Stream == 0)					// Pactor Stream
	{
		EncodeAndSend(TNC, "ODI", 3);	// Disconnect

		TNC->InternalCmd = 'P';
		TNC->CommandBusy = TRUE;
	}
	else
	{
		UCHAR TXMsg[10];

		sprintf(TXMsg, "%cDI", Stream + '@');
		EncodeAndSend(TNC, TXMsg, 4);		// Send twice - must force a disconnect
	}
}

VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	TidyClose(TNC, Stream);			// Send DI again - can't do much else
}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
	TNC->NeedPACTOR = 50;	
}
