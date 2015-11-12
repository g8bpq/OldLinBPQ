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
//	DLL to inteface HAL Communications Corp Clover/Pacor controllers to BPQ32 switch 
//
//	Uses BPQ EXTERNAL interface
//

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include "time.h"

#include "CHeaders.h"
#include "tncinfo.h"

#include "bpq32.h"

#define HAL 1

#define SetMYCALL 0x13
#define ConnectEnable 0x52
#define ConnectDisable 0x42
#define SetEAS 0x59				// Echo as Sent
#define SetTones 0xec
#define ClearOnDisc 0x57

static char ClassName[]="HALSTATUS";

static char WindowTitle[] = "HAL";
static int RigControlRow = 185;

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

#define	SOH	0x01	// CONTROL CODES 
#define	ETB	0x17
#define	DLE	0x10

//int MaxStreams = 0;

#ifndef LINBPQ
extern HFONT hFont;
#endif

static char status[23][50] = {"IDLE", "TFC", "RQ", "ERR", "PHS", "OVER", "FSK TX",
		"FSK RX", "P-MODE100", "P-MODE200", "HUFMAN ON", "HUFMAN OFF", "P-MODE SBY(LISTEN ON)",
		"P-MODE SBY(LISTEN OFF)", "ISS", "IRS",
		"AMTOR SBY(LISTEN ON)", "AMTOR SBY(LISTEN OFF)", "AMTOR FEC TX", "AMTOR FEC RX",  "P-MODE FEC TX", 
		"FREE SIGNAL TX (AMTOR)", "FREE SIGNAL TX TIMED OUT (AMTOR)"};

struct TNCINFO * CreateTTYInfo(int port, int speed);
BOOL OpenConnection(int);
BOOL SetupConnection(int);
static BOOL WriteCommBlock(struct TNCINFO * TNC);
static void CheckRX(struct TNCINFO * TNC);
VOID HALPoll(int Port);
VOID ProcessDEDFrame(struct TNCINFO * TNC, UCHAR * rxbuff, int len);
VOID ProcessTermModeResponse(struct TNCINFO * TNC);
static VOID DoTNCReinit(struct TNCINFO * TNC);
VOID DoTermModeTimeout(struct TNCINFO * TNC);
VOID ProcessHALBuffer(struct TNCINFO * TNC, int Length);
VOID ProcessHALCmd(struct TNCINFO * TNC);
VOID ProcessHALData(struct TNCINFO * TNC);
VOID ProcessKHOSTPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID ProcessKNormCommand(struct TNCINFO * TNC, UCHAR * rxbuffer);
VOID ProcessHostFrame(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID DoMonitor(struct TNCINFO * TNC, UCHAR * Msg, int Len);

BOOL HALConnected(struct TNCINFO * TNC, char * Call);
VOID HALDisconnected(struct TNCINFO * TNC);

static VOID EncodeAndSend(struct TNCINFO * TNC, UCHAR * txbuffer, int Len);
VOID SendCmd(struct TNCINFO * TNC, UCHAR * txbuffer, int Len);
int	DLEEncode(UCHAR * inbuff, UCHAR * outbuff, int len);
int	DLEDecode(UCHAR * inbuff, UCHAR * outbuff, int len);

VOID COMClearDTR(HANDLE fd);
VOID COMClearRTS(HANDLE fd);
int DoScanLine(struct TNCINFO * TNC, char * Buff, int Len);



//static HANDLE LogHandle[4] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};

//char * Logs[4] = {"1", "2", "3", "4"};

//char BaseDir[]="c:";

static VOID CloseLogfile(int Flags)
{
//	CloseHandle(LogHandle[Flags]);
//	LogHandle[Flags] = INVALID_HANDLE_VALUE;
}

static VOID OpenLogfile(int Flags)
{
/*
UCHAR FN[MAX_PATH];
	time_t T;
	struct tm * tm;

	T = time(NULL);
	tm = gmtime(&T);	

	sprintf(FN,"%s\\HALLog_%02d%02d%02d_%s.bin", BaseDir, tm->tm_mday, tm->tm_hour, tm->tm_min, Logs[Flags]);

	LogHandle[Flags] = CreateFile(FN,
					GENERIC_WRITE,
					FILE_SHARE_READ,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);

	SetFilePointer(LogHandle[Flags], 0, 0, FILE_END);

	return (LogHandle[Flags] != INVALID_HANDLE_VALUE);
*/
}

static void WriteLogLine(int Flags, char * Msg, int MsgLen)
{
//	int cnt;
//	WriteFile(LogHandle[Flags] ,Msg , MsgLen, &cnt, NULL);
}



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

		TNC = TNCInfo[BPQport] = zalloc(sizeof(struct TNCINFO));

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
		TNC = TNCInfo[BPQport] = zalloc(sizeof(struct TNCINFO));
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
			{
				TNC->WL2K = DecodeWL2KReportLine(buf);
				continue;
			}
			if (_memicmp(buf, "NEEDXONXOFF", 10) == 0)
			{
				TNC->XONXOFF = TRUE;
				continue;
			}

			if (_memicmp(buf, "TONES", 5) == 0)
			{
				int tone1 = 0, tone2 = 0;

				ptr = strtok(&buf[6], " ,/\t\n\r");
				if (ptr)
				{
					tone1 = atoi(ptr);
					ptr = strtok(NULL, " ,/\t\n\r");
					if (ptr)
					{
						tone2 = atoi(ptr);
						ptr = &TNC->InitScript[TNC->InitScriptLen];

						// Try putting into FSK mode first

						*(ptr++) = 0x84;
						*(ptr++) = SetTones;		// Set Tones (Mark, Space HI byte first)
						*(ptr++) = tone1 >> 8;
						*(ptr++) = tone1 & 0xff;
						*(ptr++) = tone2 >> 8;
						*(ptr++) = tone2 & 0xff;

						TNC->InitScriptLen += 6;	

						continue;
					}
				}
				goto BadLine;
			}
			if (_memicmp(buf, "DEFAULTMODE ", 12) == 0)
			{
					
				ptr = strtok(&buf[12], " ,\t\n\r");
				if (ptr)
				{
					if (_stricmp(ptr, "CLOVER") == 0)
						TNC->DefaultMode = Clover;
					else if (_stricmp(ptr, "PACTOR") == 0)
						TNC->DefaultMode = Pactor;
					else if (_stricmp(ptr, "AMTOR") == 0)
						TNC->DefaultMode = AMTOR;
					else goto BadLine;
					
					continue;
				}
				goto BadLine;
			}
		}
		BadLine:
			WritetoConsole(" Bad config record ");
			WritetoConsole(errbuf);
			WritetoConsole("\r\n");
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

	if (TNC == NULL)
		return 0;
	
	if (fn < 4 || fn > 5)
		if (TNC->hDevice == 0)
			return 0;					// Port not open

	STREAM = &TNC->Streams[0];

	switch (fn)
	{
	case 1:				// poll

		while (TNC->PortRecord->UI_Q)			// Release anything accidentally put on UI_Q
		{
			buffptr = Q_REM(&TNC->PortRecord->UI_Q);
			ReleaseBuffer(buffptr);
		}

		//for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			if (STREAM->ReportDISC)
			{
				STREAM->ReportDISC = FALSE;
				buff[4] = 0;

				return -1;
			}
		}

		CheckRX(TNC);
		HALPoll(port);

		//for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			if (STREAM->PACTORtoBPQ_Q !=0)
			{
				int datalen;
			
				buffptr=Q_REM(&STREAM->PACTORtoBPQ_Q);

				datalen=buffptr[1];

				buff[4] = 0;
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

		STREAM->FramesQueued++;

		return (0);


	case 3:				// CHECK IF OK TO SEND. Also used to check if TNC is responding
		
		Stream = (int)buff;
			
		if (STREAM->FramesQueued  > 4)
			return (1 | TNC->HostMode << 8);
	
		return TNC->HostMode << 8 | STREAM->Disconnecting << 15;		// OK, but lock attach if disconnecting

	case 4:				// reinit

		return (0);

	case 5:				// Close

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
	"<head><title>HAL Status</title></head><body><h3>HAL Status</h3>");

	Len += sprintf(&Buff[Len], "<table style=\"text-align: left; width: 480px; font-family: monospace; align=center \" border=1 cellpadding=2 cellspacing=2>");

	Len += sprintf(&Buff[Len], "<tr><td width=90px>Comms State</td><td>%s</td></tr>", TNC->WEB_COMMSSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>TNC State</td><td>%s</td></tr>", TNC->WEB_TNCSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>Mode</td><td>%s</td></tr>", TNC->WEB_MODE);
	Len += sprintf(&Buff[Len], "<tr><td>Status</td><td>%s</td></tr>", TNC->WEB_STATE);
	Len += sprintf(&Buff[Len], "<tr><td>TX/RX State</td><td>%s</td></tr>", TNC->WEB_TXRX);
	Len += sprintf(&Buff[Len], "<tr><td>Traffic</td><td>%s</td></tr>", TNC->WEB_TRAFFIC);
	Len += sprintf(&Buff[Len], "<tr><td>LEDS</td><td>STBY CALL LINK ERROR TX RX</td></tr>");
	Len += sprintf(&Buff[Len], "<tr><td> </td><td>%s</td></tr>", TNC->WEB_LEDS);
	Len += sprintf(&Buff[Len], "</table>");

	Len = DoScanLine(TNC, Buff, Len);

	return Len;
}


UINT HALExtInit(EXTPORTDATA *  PortEntry)
{
	char msg[500];
	struct TNCINFO * TNC;
	int port;
	char * ptr;
	int len;
	char Msg[80];
	HWND x;

	//
	//	Will be called once for each Pactor Port
	//	The COM port number is in IOBASE
	//

	sprintf(msg,"HAL Driver %s", PortEntry->PORTCONTROL.SerialPortName);
	WritetoConsole(msg);

	port=PortEntry->PORTCONTROL.PORTNUMBER;

	ReadConfigFile(port, ProcessLine);
	TNC = TNCInfo[port];

	if (TNC == NULL)
	{
		// Not defined in Config file

		sprintf(msg," ** Error - no info in BPQ32.cfg for this port");
		WritetoConsole(msg);

		return (int)ExtProc;
	}
	
	TNC->Port = port;

	TNC->Hardware = H_HAL;

	TNC->Interlock = PortEntry->PORTCONTROL.PORTINTERLOCK;

	PortEntry->MAXHOSTMODESESSIONS = 1;		// Default

	TNC->PortRecord = PortEntry;

	if (PortEntry->PORTCONTROL.PORTCALL[0] == 0)
	{
		memcpy(TNC->NodeCall, MYNODECALL, 10);
	}
	else
	{
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);
	}

	PortEntry->PORTCONTROL.PROTOCOL = 10;
	PortEntry->PORTCONTROL.PORTQUALITY = 0;

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0)
		PortEntry->PORTCONTROL.PORTPACLEN = 100;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	if (TNC->DefaultMode)
		TNC->CurrentMode = TNC->DefaultMode;
	else
		TNC->CurrentMode = Clover;

	TNC->PollDelay = 999999999;

	// Set Disable +?, ExpandedStatus , Channel Stats Off, ClearOnDisc, EAS and MYCALL

	len = sprintf(Msg, "%c%c%c%c%c%c%s", 0xcc, 0x56, 0x41, ClearOnDisc, SetEAS, SetMYCALL, TNC->NodeCall);
	len++;					// We include the NULL

	memcpy(&TNC->InitScript[TNC->InitScriptLen], Msg, len); 
	TNC->InitScriptLen += len;

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
	TNC->WEB_LEDS = zalloc(100);
	strcpy(TNC->WEB_LEDS, "  X   X     X    X   X  X");

#ifndef LINBPQ

	CreatePactorWindow(TNC, ClassName, WindowTitle, RigControlRow, PacWndProc, 500, 233);

	x = CreateWindowEx(0, "STATIC", "Comms State", WS_CHILD | WS_VISIBLE, 10,6,120,20, TNC->hDlg, NULL, hInstance, NULL);
	x = TNC->xIDC_COMMSSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,6,386,20, TNC->hDlg, NULL, hInstance, NULL);
	
	x = CreateWindowEx(0, "STATIC", "TNC State", WS_CHILD | WS_VISIBLE, 10,28,106,20, TNC->hDlg, NULL, hInstance, NULL);
	x = TNC->xIDC_TNCSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,28,520,20, TNC->hDlg, NULL, hInstance, NULL);

	x = CreateWindowEx(0, "STATIC", "Mode", WS_CHILD | WS_VISIBLE, 10,50,80,20, TNC->hDlg, NULL, hInstance, NULL);
	x = TNC->xIDC_MODE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,50,200,20, TNC->hDlg, NULL, hInstance, NULL);

	x = CreateWindowEx(0, "STATIC", "Status", WS_CHILD | WS_VISIBLE, 10,72,110,20, TNC->hDlg, NULL, hInstance, NULL);
	x = TNC->xIDC_STATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,72,144,20, TNC->hDlg, NULL, hInstance, NULL);

 	x = CreateWindowEx(0, "STATIC", "TX/RX State", WS_CHILD | WS_VISIBLE,10,94,80,20, TNC->hDlg, NULL, hInstance, NULL);
	x = TNC->xIDC_TXRX = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE,116,94,374,20 , TNC->hDlg, NULL, hInstance, NULL);

	x = CreateWindowEx(0, "STATIC", "Traffic", WS_CHILD | WS_VISIBLE,10,116,80,20, TNC->hDlg, NULL, hInstance, NULL);
	x = TNC->xIDC_TRAFFIC = CreateWindowEx(0, "STATIC", "RX 0 TX 0 ACKED 0", WS_CHILD | WS_VISIBLE,116,116,374,20 , TNC->hDlg, NULL, hInstance, NULL);

	x = CreateWindowEx(0, "STATIC", "LEDS", WS_CHILD | WS_VISIBLE,10,138,60,20, TNC->hDlg, NULL, hInstance, NULL);
	SendMessage(x, WM_SETFONT, (WPARAM)hFont, 0);
	x = CreateWindowEx(0, "STATIC",                  "STBY CALL LINK ERROR TX RX", WS_CHILD | WS_VISIBLE,116,138,280,20, TNC->hDlg, NULL, hInstance, NULL);
	SendMessage(x, WM_SETFONT, (WPARAM)hFont, 0);
	x = TNC->xIDC_LEDS = CreateWindowEx(0, "STATIC", "  X   X     X    X   X  X", WS_CHILD | WS_VISIBLE,116,158,280,20 , TNC->hDlg, NULL, hInstance, NULL);
	SendMessage(x, WM_SETFONT, (WPARAM)hFont, 0);
	
	TNC->ClientHeight = 233;
	TNC->ClientWidth = 500;
	
	MoveWindows(TNC);
#endif	

	OpenCOMMPort(TNC, PortEntry->PORTCONTROL.SerialPortName, PortEntry->PORTCONTROL.BAUDRATE, FALSE);

	SendCmd(TNC, "\x09" , 1);		// Reset

	WritetoConsole("\n");

	return ((int)ExtProc);
}


 
static VOID KISSCLOSE(int Port)
{ 
	struct TNCINFO * conn = TNCInfo[Port];

	// drop DTR and RTS

	COMClearDTR(conn->hDevice);
	COMClearRTS(conn->hDevice);

   // purge any outstanding reads/writes and close device handle

	CloseCOMPort(conn->hDevice);		
	
	return;
}


static void CheckRX(struct TNCINFO * TNC)
{
	int Length, Len;
	UCHAR * Xptr;

	// only try to read number of bytes in queue 

	if (TNC->RXLen == 500)
		TNC->RXLen = 0;

	Len = ReadCOMBlock(TNC->hDevice, &TNC->RXBuffer[TNC->RXLen], 500 - TNC->RXLen);

	if (Len == 0)
		return;

	TNC->RXLen += Len;

	Length = TNC->RXLen;

	// We need to konw whether data is received or echoed, so we can't split commands and data here.
	// Pass everything to the Command Handler. It will check that there are enough bytes for the command,
	// and wait for more if not.

	// The USB version also uses 0x91 0x31 to eacape 0x11, 0x91 0x33 for 0x13 and 0x91 0xB1 for 0x91

	// If USB version, we might get unescaped xon and xoff, which we must ignore

	if (TNC->XONXOFF)
	{
		Xptr = memchr(&TNC->RXBuffer, 0x11, Length);
	
		while(Xptr)
		{
			Debugprintf("XON Port %d", TNC->Port);
			memmove(Xptr, Xptr + 1, Length-- - (TNC->RXBuffer - Xptr));
			Xptr = memchr(&TNC->RXBuffer, 0x11, Length);
		}

		Xptr = memchr(&TNC->RXBuffer, 0x13, Length);
	
		while(Xptr)
		{
			Debugprintf("XOFF Port %d", TNC->Port);
			memmove(Xptr, Xptr + 1, Length-- - (TNC->RXBuffer - Xptr));
			Xptr = memchr(&TNC->RXBuffer, 0x13, Length);
		}
	
		Xptr = memchr(&TNC->RXBuffer, 0x91, Length);			// See if packet contains 0x91 escape

		if (Xptr)
	
			// Make sure we have the escaped char as well
	
			if ((Xptr - &TNC->RXBuffer[0]) == Length - 1)		// x91 is last char
				return;
	}

	ProcessHALBuffer(TNC, Length);

	TNC->RXLen = 0;
	
	return;

}



static BOOL WriteCommBlock(struct TNCINFO * TNC)
{
	WriteCOMBlock(TNC->hDevice, TNC->TXBuffer, TNC->TXLen);
	return TRUE;
}

VOID HALPoll(int Port)
{
	struct TNCINFO * TNC = TNCInfo[Port];
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	UCHAR * Poll = TNC->TXBuffer;
	char Status[80];
	UCHAR TXMsg[1000];
	int datalen;

	if (TNC->Timeout)
	{  
		TNC->Timeout--;
		
		if (TNC->Timeout)			// Still waiting
			return;

		// Timed Out

		TNC->TNCOK = FALSE;
		TNC->HostMode = 0;
				
		sprintf(TNC->WEB_COMMSSTATE,"%s Open but TNC not responding", TNC->PortRecord->PORTCONTROL.SerialPortName);
		SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

		//for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			if (TNC->PortRecord->ATTACHEDSESSIONS[0])		// Connected
			{
				STREAM->Connected = FALSE;		// Back to Command Mode
				STREAM->ReportDISC = TRUE;		// Tell Node
			}
		}

	}

	// if we have just restarted or TNC appears to be in terminal mode, run Initialisation Sequence

	if (TNC->TNCOK)
		if (!TNC->HostMode)
		{
			DoTNCReinit(TNC);
			return;
		}	

	if (TNC->PortRecord->ATTACHEDSESSIONS[0] && STREAM->Attached == 0)
	{
		// New Attach

		int calllen;
		char Msg[80];

		STREAM->Attached = TRUE;

		STREAM->BytesRXed = STREAM->BytesTXed = STREAM->BytesAcked = 0;

		calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[0]->L4USER, STREAM->MyCall);
		STREAM->MyCall[calllen] = 0;
		
		datalen = sprintf(TXMsg, "%c%s", SetMYCALL, STREAM->MyCall);
		SendCmd(TNC, TXMsg, datalen + 1);			// Send the NULL

		sprintf(TNC->WEB_TNCSTATE, "In Use by %s", STREAM->MyCall);
		SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

		// Stop Scanning

		sprintf(Msg, "%d SCANSTOP", TNC->Port);
		
		Rig_Command(-1, Msg);

		SendCmd(TNC, "\x42", 1);		// Connect Enable off

		return;

	}

	//for (Stream = 0; Stream <= MaxStreams; Stream++)

	if (STREAM->Attached)
		CheckForDetach(TNC, 0, STREAM, TidyClose, ForcedClose, CloseComplete);

	if (TNC->NeedPACTOR)
	{
		TNC->NeedPACTOR--;

		if (TNC->NeedPACTOR == 0)
		{
			int datalen;

			UCHAR TXMsg[80];

			datalen = sprintf(TXMsg, "%c%s", SetMYCALL, TNC->NodeCall);
			SendCmd(TNC, TXMsg, datalen + 1);			// Send the NULL

			// Set Listen Mode

			switch (TNC->CurrentMode)
			{
			case Pactor:

				SendCmd(TNC, "\x84", 1);		// FSK
				SendCmd(TNC, "\x83", 1);		// Select P-MODE Standby
				SendCmd(TNC, "\x58", 1);		// Listen

				break;

			case Clover:

				SendCmd(TNC, "\x80", 1);		// Clover
				SendCmd(TNC, "\x54", 1);		// Enable adaptive Clover format
				SendCmd(TNC, "\x41", 1);		// No Statistics
				SendCmd(TNC, "\x60\x09", 2);	// Robust Retries
				SendCmd(TNC, "\x61\x09", 2);	// Normal Retries

				break;			
			}

			SendCmd(TNC, "\x52", 1);			// ConnectEnable

			// Restart Scanning

			sprintf(Status, "%d SCANSTART 15", TNC->Port);
		
			Rig_Command(-1, Status);

			return;
		}
	}

#define MAXHALTX 256

	//for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		if (TNC->TNCOK && STREAM->BPQtoPACTOR_Q && (STREAM->BytesTXed - STREAM->BytesAcked < 600))
		{
			int datalen;
			UINT * buffptr;
			UCHAR * MsgPtr;
			unsigned char TXMsg[500];
			
			buffptr = (UINT * )STREAM->BPQtoPACTOR_Q;
			datalen=buffptr[1];
			MsgPtr = (UCHAR *)&buffptr[2];

			if (STREAM->Connected)
			{
				if (TNC->SwallowSignon)
				{
					TNC->SwallowSignon = FALSE;	
					if (strstr(MsgPtr, "Connected"))	// Discard *** connected
					{
						ReleaseBuffer(buffptr);
						STREAM->FramesQueued--;
						return;
					}
				}

				// Must send data in small chunks - the Hal has limited buffer space

				// If in IRS force a turnround 

				if (TNC->TXRXState == 'R' && TNC->CurrentMode != Clover)
				{
					if (TNC->TimeInRX++ > 15)
						SendCmd(TNC, "\x87", 1);		// Changeover to ISS 
					else
						goto Poll;
				}

				TNC->TimeInRX = 0;
					
				EncodeAndSend(TNC, MsgPtr, datalen);
				buffptr=Q_REM(&STREAM->BPQtoPACTOR_Q);
				ReleaseBuffer(buffptr);
				WriteLogLine(2, MsgPtr, datalen);

				STREAM->BytesTXed += datalen; 
				STREAM->FramesQueued--;

				ShowTraffic(TNC);

				return;
			}
			else
			{
				buffptr=Q_REM(&STREAM->BPQtoPACTOR_Q);
				STREAM->FramesQueued--;

				// Command. Do some sanity checking and look for things to process locally

				datalen--;				// Exclude CR
				MsgPtr[datalen] = 0;	// Null Terminate
				_strupr(MsgPtr);

				if (memcmp(MsgPtr, "RADIO ", 6) == 0)
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

				if (memcmp(MsgPtr, "MODE CLOVER", 11) == 0)
				{
					TNC->CurrentMode = Clover;
					buffptr[1] = sprintf((UCHAR *)&buffptr[2],"HAL} Ok\r");
					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
			
					SetWindowText(TNC->xIDC_MODE, "Clover");
					strcpy(TNC->WEB_MODE, "Clover");

					SendCmd(TNC, "\x80", 1);		// Clover
					SendCmd(TNC, "\x54", 1);		// Enable adaptive Clover format
					SendCmd(TNC, "\x41", 1);		// No Statistics

					return;
				}

				if (memcmp(MsgPtr, "MODE PACTOR", 11) == 0)
				{
					TNC->CurrentMode = Pactor;
					buffptr[1] = sprintf((UCHAR *)&buffptr[2],"HAL} Ok\r");
					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

					SendCmd(TNC, "\x84", 1);		// FSK
					SendCmd(TNC, "\x83", 1);		// Select P-MODE Standby
					SendCmd(TNC, "\x48", 1);		// Listen Off

					return;
				}
				if (memcmp(MsgPtr, "MODE AMTOR", 11) == 0)
				{
					TNC->CurrentMode = AMTOR;
					buffptr[1] = sprintf((UCHAR *)&buffptr[2],"HAL} Ok\r");
					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

					return;
				}

				if (MsgPtr[0] == 'C' && MsgPtr[1] == ' ' && datalen > 2)	// Connect
				{
					memcpy(STREAM->RemoteCall, &MsgPtr[2], 9);

					switch (TNC->CurrentMode)
					{
					case Pactor:

						SendCmd(TNC, "\x84", 1);		// FSK
						SendCmd(TNC, "\x83", 1);		// Select P-MODE Standby

						datalen = sprintf(TXMsg, "\x19%s", STREAM->RemoteCall);
					
						sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s - PACTOR", STREAM->MyCall, STREAM->RemoteCall);

						// DOnt set connecting till we get the 19 response so we can trap listen as a fail
						break;

					case Clover:

						SendCmd(TNC, "\x54", 1);		// Enable adaptive Clover format
						SendCmd(TNC, "\x57", 1);		// Enable TX buffer clear on disconnect

						datalen = sprintf(TXMsg, "\x11%s", STREAM->RemoteCall);
					
						sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s - CLOVER", STREAM->MyCall, STREAM->RemoteCall);

						break;			
					}
					
					SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);
					SendCmd(TNC, TXMsg, datalen + 1);	// Include NULL

					ReleaseBuffer(buffptr);

					return;
				}

				if (memcmp(MsgPtr, "CLOVER ", 7) == 0)
				{
					memcpy(STREAM->RemoteCall, &MsgPtr[2], 9);

					SendCmd(TNC, "\x54", 1);		// Enable adaptive Clover format
					SendCmd(TNC, "\x57", 1);		// Enable TX buffer clear on disconnect

					datalen = sprintf(TXMsg, "\x11%s", STREAM->RemoteCall);
					SendCmd(TNC, TXMsg, datalen + 1);	// Include NULL
					
					sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s - CLOVER",
					STREAM->MyCall, STREAM->RemoteCall);
					SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

					ReleaseBuffer(buffptr);

					return;
				}

				if (memcmp(MsgPtr, "DISCONNECT", datalen) == 0)	// Disconnect
				{
					SendCmd(TNC, "\x07", 1);		// Normal Disconnect
					TNC->NeedPACTOR = 50;
	
					STREAM->Connecting = FALSE;
					STREAM->ReportDISC = TRUE;
					ReleaseBuffer(buffptr);

					return;
				}
	
				// Other Command ?? Treat as HEX string

				datalen = sscanf(MsgPtr, "%X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X",
					(UINT *)&TXMsg[0], (UINT *)&TXMsg[1], (UINT *)&TXMsg[2], (UINT *)&TXMsg[3], (UINT *)&TXMsg[4],
					(UINT *)&TXMsg[5], (UINT *)&TXMsg[6], (UINT *)&TXMsg[7], (UINT *)&TXMsg[8], (UINT *)&TXMsg[9], 
					(UINT *)&TXMsg[10], (UINT *)&TXMsg[11], (UINT *)&TXMsg[12], (UINT *)&TXMsg[13], 
					(UINT *)&TXMsg[14], (UINT *)&TXMsg[15]);

//				SendCmd(TNC, TXMsg, datalen);
				ReleaseBuffer(buffptr);
				TNC->InternalCmd = 0;
			}
		}
	}
Poll:
	// Nothing doing - send Poll (but not too often)

	TNC->PollDelay++;

	if (TNC->PollDelay < 20)
		return;

	TNC->PollDelay = 0;

	if (TNC->TNCOK)
		SendCmd(TNC, "\x7d" , 1);			// Use Get LEDS as Poll
	else
		SendCmd(TNC, "\x09" , 1);			// Reset

	TNC->Timeout = 100;

	return;
}

static VOID DoTNCReinit(struct TNCINFO * TNC)
{
	// TNC Has Restarted, send init commands (can probably send all at once)

//	TNC->TXBuffer[0] = 0x1b;
//	TNC->TXLen = 1;

	WriteCommBlock(TNC);

	SendCmd(TNC, TNC->InitScript, TNC->InitScriptLen);

	TNC->HostMode = TRUE;		// Should now be in Host Mode
	TNC->NeedPACTOR = 20;		// Need to set Calls and start scan

	TNC->DataMode = RXDATA;		// Start with RX Data

	SendCmd(TNC, "\x7d" , 1);	// Use Get LEDS as Poll
//	SendCmd(TNC, "\xc9" , 1);	// Huffman Off
	SendCmd(TNC, "\x57", 1);	// Enable TX buffer clear on disconnect

	SendCmd(TNC, "\x60\x06", 2);	// Robust Mode Retries

//	SendCmd(TNC, "\x6f\x03" , 2);	// Undocumented XON/XOFF On - used to see if old or new style modem

	TNC->Timeout = 50;

	return;

}	

VOID ProcessHALData(struct TNCINFO * TNC)
{
	// Received Data just pass to Appl

	UINT * buffptr;
	int Len = TNC->DataLen;
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	TNC->DataLen = 0;

	if (TNC->DataMode == TXDATA)
	{
		STREAM->BytesAcked += Len;
//		Debugprintf("Acked %d", Len);

		if (STREAM->BytesAcked > STREAM->BytesTXed)
			Debugprintf("Too Much Acked");

		if ((STREAM->BPQtoPACTOR_Q == 0) && STREAM->BytesAcked >= STREAM->BytesTXed)
		{
			// All sent 

			if (STREAM->Disconnecting)
				TidyClose(TNC, 0);
			else
				if (TNC->CurrentMode != Clover)
			
				// turn round link

					SendCmd(TNC, "\x0c" , 1);	// Turnround
			
		}
	}
	else
	{
		if (TNC->DataMode == RXDATA)
		{
//			Debugprintf("RXed %d", Len);
			buffptr = GetBuff();
			if (buffptr == NULL) 
				return;	// No buffers, so ignore

			buffptr[1] = Len;				// Length

			WriteLogLine(1, TNC->DataBuffer, Len);

			STREAM->BytesRXed += Len;

			memcpy(&buffptr[2], TNC->DataBuffer, Len);

			C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
		}
	}

	ShowTraffic(TNC);

	return;
}



VOID ProcessHALBuffer(struct TNCINFO * TNC, int Length)
{
	UCHAR Char;
	UCHAR * inptr;
	UCHAR * cmdptr;
	UCHAR * dataptr;
	BOOL CmdEsc, DataEsc;

	inptr = TNC->RXBuffer;

	cmdptr = &TNC->CmdBuffer[TNC->CmdLen];
	dataptr = &TNC->DataBuffer[TNC->DataLen];
	CmdEsc = TNC->CmdEsc;
	DataEsc = TNC->DataEsc;

	// HAL uses HEX 80 as a command escape, 81 to  ESCAPE 80 and 81

	// The USB version also uses 0x91 0x31 to eacape 0x11, 0x91 0x33 for 0x13 and 0x91 0xB1 for 0x91

	// Command Responses can be variable length

	// Command Handler will check for each command/response if it has enough - if not it will wait till more arrives

	while(Length--)
	{
		Char = *(inptr++);

		if (CmdEsc)
		{
			CmdEsc = FALSE;

			if (TNC->XONXOFF && Char == 0x91)
			{
				// XON/XOFF escape. We ensured above that data follows so we can process it inline

				Length--;
				Char = *(inptr++) - 0x20;
			}
			*(cmdptr++) = Char;
		}
		else if (DataEsc)
		{
			DataEsc = FALSE;
			goto DataChar;
		}
		else
NotData:
		if (Char == 0x80)				// Next Char is Command
			CmdEsc = TRUE;
		else if (Char == 0x81)				// Next Char is escaped data (80 or 81)
			DataEsc = TRUE;
		else
		{
			// This is a Data Char. We must process any Commands received so far, so we know the type of data

		DataChar:

			TNC->CmdLen = cmdptr - TNC->CmdBuffer;
			ProcessHALCmd(TNC);
			cmdptr = &TNC->CmdBuffer[TNC->CmdLen];
			dataptr = &TNC->DataBuffer[TNC->DataLen];

			*(dataptr++) = Char;			// Normal Data

			// Now process any other data chars

			while(Length--)
			{
				Char = *(inptr++);

				if (TNC->XONXOFF && Char == 0x91)
				{
					// XON/XOFF escape within data. We ensured above that data follows so we
					// can process it here

						Length--;
						Char = *(inptr++) - 0x20;
				}

				if (Char == 0x80 || Char == 0x81)
				{
					// Process any data we have, then loop back

					TNC->DataLen = dataptr - TNC->DataBuffer;
					ProcessHALData(TNC);

					goto NotData;
				}
				*(dataptr++) = Char;			// Normal Data
			}

			// Used all data

			TNC->DataLen = dataptr - TNC->DataBuffer;

			ProcessHALData(TNC);
			TNC->CmdEsc = CmdEsc;
			TNC->DataEsc = DataEsc;

			return;
		}	
	}

	// Save State

	TNC->CmdLen = cmdptr - TNC->CmdBuffer;

	TNC->CmdEsc = CmdEsc;
	TNC->DataEsc = DataEsc;

	if (TNC->DataLen)
		ProcessHALData(TNC);

	if (TNC->CmdLen)
		ProcessHALCmd(TNC);
}

VOID mySetWindowText(struct TNCINFO * TNC, char * Msg)
{
	SetWindowText(TNC->xIDC_STATE, Msg);
	strcpy(TNC->WEB_STATE, Msg);
}

VOID ProcessHALCmd(struct TNCINFO * TNC)
{
	char * Call;
	int Stream = 0;
	int Opcode;
	int StatusByte;
	int Leds;
	int Len;
	int Used;
	struct STREAMINFO * STREAM = &TNC->Streams[0];

CmdLoop:

	Opcode = TNC->CmdBuffer[0];
	Len = TNC->CmdLen;

	if (Len == 0)
		return;

	TNC->TNCOK = TRUE;
	TNC->Timeout = 0;

	sprintf(TNC->WEB_COMMSSTATE,"%s TNC link OK", TNC->PortRecord->PORTCONTROL.SerialPortName);
	SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

	// We may have more than one response in the buffer, and only each cmd/response decoder knows how many it needs

	switch(Opcode)
	{
	case 0x09:			//Hardware Reset - equivalent to power on reset

		// Hardware has reset - need to reinitialise

		TNC->HostMode = 0;			// Force Reinit

		Used = 1;
		break;

	case 0x7a:				// FSK Modes Status

		// Mixture of mode and state - eg listen huffman on/off irs/iss, so cant just display

		if (Len < 2) return;		// Wait for more
	
		StatusByte = TNC->CmdBuffer[1];

		switch (StatusByte)
		{
			case 0x06:		// FSK TX (RTTY)
			case 0x07:		// FSK RX (RTTY)
			case 0x10:		// AMTOR STANDBY (LISTEN ON)
			case 0x11:		// AMTOR STANDBY (LISTEN OFF)
			case 0x12:		// AMTOR FEC TX (AMTOR)
			case 0x13:		// AMTOR FEC RX (AMTOR)
			case 0x14:		// P-MODE FEC TX (P-MODE)
			case 0x15:		// FREE SIGNAL TX (AMTOR)
			case 0x16:		// FREE SIGNAL TX TIMED OUT (AMTOR)

			// Diaplay Linke Status

			SetWindowText(TNC->xIDC_MODE, status[StatusByte]);
			strcpy(TNC->WEB_MODE, status[StatusByte]);

			break;

			case 0x0C:		// P-MODE STANDBY (LISTEN ON)
			case 0x0D:		// P-MODE STANDBY (LISTEN OFF)

			// if we were connecting, this means connect failed.

			SetWindowText(TNC->xIDC_MODE, status[StatusByte]);
			strcpy(TNC->WEB_MODE, status[StatusByte]);

			if (STREAM->Connecting)
				HALDisconnected(TNC);

			break;

			case 0x0E:		// ISS (AMTOR/P-MODE)

				SetWindowText(TNC->xIDC_TXRX,"ISS");
				strcpy(TNC->WEB_TXRX, "ISS");
				TNC->TXRXState = 'S';
				break;

			case 0x0F:		// IRS (AMTOR/P-MODE)

				SetWindowText(TNC->xIDC_TXRX,"IRS");
				strcpy(TNC->WEB_TXRX, "IRS");
				TNC->TXRXState = 'R';
				break;

			case 0x00:		//  IDLE (AMTOR/P-MODE)
			case 0x01:		//  TFC (AMTOR/P-MODE)
			case 0x02:		//  RQ (AMTOR/P-MODE)
			case 0x03:		//  ERR (AMTOR/P-MODE)
			case 0x04:		//  PHS (AMTOR/P-MODE)
			case 0x05:		//  OVER (AMTOR/P-MODE) (not implemented)

				SetWindowText(TNC->xIDC_STATE, status[StatusByte]);
				strcpy(TNC->WEB_MODE, status[StatusByte]);



//$807A $8008 P-MODE100 (P-MODE)
//$807A $8009 P-MODE200 (P-MODE)
//$807A $800A HUFFMAN ON (P-MODE)
//$807A $800B HUFFMAN OFF (P-MODE)
				;
		}
		Used = 2;
		break;
		

	case 0x7d:				// Get LED Status
		
		// We use Get LED Status as a Poll

		if (Len < 2) return;		// Wait for more
	
		Leds = TNC->CmdBuffer[1];
		sprintf(TNC->WEB_LEDS,"  %c   %c    %c    %c     %c %c ", 
			(Leds & 0x20)? 'X' : ' ',
			(Leds & 0x10)? 'X' : ' ',
			(Leds & 0x08)? 'X' : ' ',
			(Leds & 0x04)? 'X' : ' ',
			(Leds & 0x02)? 'X' : ' ',
			(Leds & 0x01)? 'X' : ' ');

//		STBY CALL LINK ERROR TX RX
		SetWindowText(TNC->xIDC_LEDS, TNC->WEB_LEDS);

		Used = 2;
		break;

	case 0x21:				// Monitored FEC CCB
	case 0x22:				// Monitored ARQ CCB

		// As the reply is variable, make sure we have the terminating NULL

		if (memchr(TNC->CmdBuffer, 0, Len) == NULL)
			return;					// Wait for more

		Call = &TNC->CmdBuffer[1];
		Used = strlen(Call) + 2;	// Opcode and Null

		UpdateMH(TNC, Call, '!', 0);

		break;

	case 0x27:						// Clover ARQ LINK REQUEST status message 
		
		//indicates an incoming link request to either MYCALL ($8027 $8000), or MYALTCALL ($8027 $8001).

		if (Len < 2) return;		// Wait for more

		// Don't need to do anything (but may eventally use ALTCALL as an APPLCALL
		Used = 2;
		break;

	case 0x2D:						// FSK ARQ Link Request status message
		
		// $802D $8001 $8000 CLOVER Link Request (not implemented)
		// $802D $8002 $8000 AMTOR CCIR-476 Link Request
		// $802D $8003 $8000 AMTOR CCIR-625 Link Request
		// $802D $8004 $8000 P-MODE Link Request

		if (Len < 3) return;		// Wait for more

		// Don't need to do anything (but may save Session type later

		Used = 3;
		break;


	case 0x28:				// Monitored Call

		// As the reply is variable, make sure we have the terminating NULL

		if (memchr(TNC->CmdBuffer, 0, Len) == NULL)
			return;					// Wait for more

		Call = &TNC->CmdBuffer[1];
		Used = strlen(Call) + 2;	// Opcode and Null

		// Could possibly be used for APPLCALLS by changing MYCALL when we see a call to one of our calls

		break;


	case 0x20:			// Clover Linked with - Call Connected
	case 0x29:			// The Linked 476 message indicates the start of a CCIR 476 linked session.
	case 0x2A:			// The Linked 625 message indicates the start of a CCIR 625 linked session to <CALL>.
	case 0x2B:			// P-MODE link to

		// As the reply is variable, make sure we have the terminating NULL

		if (memchr(TNC->CmdBuffer, 0, Len) == NULL)
			return;					// Wait for more

		Call = &TNC->CmdBuffer[1];
		Used = strlen(Call) + 2;	// Opcode and Null

		HALConnected(TNC, Call);

		break;

	case 0x23:						// Normal Disconnected - followed by $8000
	case 0x24:						// Link failed (any of the link errors)
	case 0x25:						// Signal Lost (LOS)

		if (Len < 2) return;		// Wait for more

		HALDisconnected(TNC);

		Used = 2;
		break;


		// Stream Switch Reports - we will need to do something with these if Echo as Sent is set
		// or we do something with the secondary port

	case 0x30:						// Switch to Receive Data characters
	case 0x31:						// Switch to Transmit Data characters
	case 0x32:						// Switch to RX data from secondary port

		TNC->DataMode = Opcode; 
		Used = 1;
		break;

	case 0x33:						// Send TX data to modem
	case 0x34:						// Send TX data to secondary port

		TNC->TXMode = Opcode; 
		Used = 1;
		break;

	case 0x70:						// Channel Spectra Data 
									// $807F $80xx $8030 Invalid or unimplemented command code
		if (Len < 9) return;		// Wait for more

		Used = 9;
		break;
		
	case 0x71:						// SelCall On/Off

		if (Len < 2) return;		// Wait for more

		Used = 2;
		break;

	case 0x72:						// Channel Spectra Data 
									// $807F $80xx $8030 Invalid or unimplemented command code
		if (Len < 15) return;		// Wait for more

		Used = 15;
		break;

	case 0x73:						// Clover Link state

		if (Len < 2) return;		// Wait for more

		StatusByte = TNC->CmdBuffer[1];

		switch (StatusByte)
		{
		case 0x00:		mySetWindowText(TNC, "Channel idle"); break;
		case 0x01:		mySetWindowText(TNC, "Channel occupied with non-Clover signal"); break;
		case 0x42:		mySetWindowText(TNC, "Linked stations monitored"); break;
		case 0x64:		mySetWindowText(TNC, "Attempting normal link"); break;
		case 0x65:		mySetWindowText(TNC, "Attempting robust link"); break;
		case 0x66:		mySetWindowText(TNC, "Calling ARQ CQ"); break;
		case 0x78:		mySetWindowText(TNC, "Clover Control Block (CCB) send retry"); break;
		case 0x79:		mySetWindowText(TNC, "Clover Control Block (CCB) receive retry"); break;
		case 0x7D:		mySetWindowText(TNC, "Clover Control Block (CCB) received successfully"); break;
		case 0x8A:		mySetWindowText(TNC, "TX data block sent"); break;
		case 0x8B:		mySetWindowText(TNC, "RX data block received ok (precedes data block)"); break;
		case 0x8C:		mySetWindowText(TNC, "TX data block re-sent"); break;
		case 0x8D:		mySetWindowText(TNC, "RX data block decode failed (precedes data block)"); break;
		case 0x8E:		mySetWindowText(TNC, "TX idle"); break;
		case 0x8F:		mySetWindowText(TNC, "RX idle"); break;
		case 0x9C:		mySetWindowText(TNC, "Link failed: CCB send retries exceeded"); break;
		case 0x9D:		mySetWindowText(TNC, "Link failed: CCB receive retries exceeded"); break;
		case 0x9E:		mySetWindowText(TNC, "Link failed: protocol error"); break;
		case 0xA0:		mySetWindowText(TNC, "Receiving FEC SYNC sequence"); break;
		}

		Used = 2;
		break;

	case 0x75:		// Clover waveform format

		if (Len < 5) return;		// Wait for more

		Used = 5;
		break;

	case 0x7F:						// Error $80xx $80yy Error in command $80xx of type $80yy
									// $807F $80xx $8030 Invalid or unimplemented command code
									// $807F $80xx $8031 Invalid parameter value
									// $807F $80xx $8032 Not allowed when connected
									// $807F $80xx $8033 Not allowed when disconnected
									// $807F $80xx $8034 Not valid in this mode
									// $807F $80xx $8035 Not valid in this code
									// $807F $8096 $8036 EEPROM write error

		if (Len < 3) return;		// Wait for more

		if (TNC->CmdBuffer[1] == 0x6f && TNC->CmdBuffer[2] == 0x31)
		{
			// Reject of XON/XOFF enable

//			TNC->XONXOFF = FALSE;
//			Debugprintf("BPQ32 HAL Port %d - Disabling XON/XOFF mode", TNC->Port);
		}
		else
			Debugprintf("HAL Port %d Command Error Cmd %X Error %X", TNC->Port, TNC->CmdBuffer[1], TNC->CmdBuffer[2]);

		Used = 3;
		break;

		// Following are all immediate commands - response is echo of command

	case 0x6f:				// XON/XOFF on
		
//		TNC->XONXOFF = TRUE;	// And drop through
//		Debugprintf("BPQ32 HAL Port %d - Enabling XON/XOFF mode", TNC->Port);

	case 0x19:			// Call P-MODE to <CALL>
	case 0x10:			// Robust Link to <CALL> using MYCALL
	case 0x11:			// Normal Link to <CALL> using MYCALL

		STREAM->Connecting = TRUE;

	case 0x00:			// P Load LOD file
	case 0x01:			// P Load S28 file
	case 0x02:			//Check Unit Error Status
	case 0x03:			//F Check System Clock
	case 0x04:			//C Close PTT and transmit Clover waveform
	case 0x05:			//Open PTT and stop transmit test
	case 0x06:			//Immediate Abort (Panic Kill)
	case 0x07:			//Normal disconnect (wait for ACK)
	case 0x08:			//Software reset - restore all program defaults
	case 0x0A:			//Send CW ID
	case 0x0B:			//Close PTT and transmit Single Tone
	case 0x0C:			//F Normal OVER (AMTOR,P-MODE)
	case 0x0D:			//F Force RTTY TX (Baudot/ASCII)
	case 0x0E:			//F Go to RTTY RX (Baudot/ASCII)
	case 0x0F:			//Go to LOD/S28 file loader
	case SetMYCALL:		// Set MYCALL Response

	case 0x1E:			// Set MYALTCALL Response

	case 0x41:
	case 0x42:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x4d:
	case 0x52:			// Enable adaptive Clover format
	case 0x54:			// Enable adaptive Clover format

	case 0x56:			// Expanded Link State Reports OFF/ON
	case 0x57:			// Clear buffers on disc
	case 0x58:
	case 0x59:
	case 0x60:			// Robust Mode Retries
	case 0x61:			// Normal Mode Retries
	case 0x80:			//Switch to CLOVER mode
	case 0x81:			//Select AMTOR Standby
	case 0x82:			//Select AMTOR FEC
	case 0x83:			//Select P-MODE Standby
	case 0x84:			//Switch to FSK modes
	case 0x85:			//Select Baudot
	case 0x86:			//Select ASCII
	case 0x87:			//Forced OVER (AMTOR, P-MODE)
	case 0x88:			//Forced END (AMTOR, P-MODE)
	case 0x89:			//Force LTRS shift
	case 0x8A:			//Force FIGS shift
	case 0x8B:			//Send MARK tone
	case 0x8C:			//Send SPACE tone
	case 0x8D:			//Send MARK/SPACE tones
	case 0x8E:			//Received first character on line
	case 0x8F:			//Close PTT only (no tones)

	case 0xC9:			//Huffman Off/On
	case 0xCC:
	case 0xD9:			//Close PTT only (no tones)

	case SetTones:

		Used = 1;
		break;

	case 0x91:				// ????

//		if (Len < 2) return;		// Wait for more

		Used = 1;
		break;

	default:

		// We didn't recognise command, so don't know how long it is - disaster!

		Debugprintf("HAL Port %d Unrecognised Command %x", TNC->Port, Opcode);
		TNC->CmdLen = 0;

		return;
	}

	if (Used == Len)
	{
		// All used - most likely case

		TNC->CmdLen = 0;
		return;
	}

	// Move Command Down buffer, and reenter

	TNC->CmdLen -= Used;

	memmove(TNC->CmdBuffer, &TNC->CmdBuffer[Used], TNC->CmdLen);

	goto CmdLoop;


}

	
VOID HALDisconnected(struct TNCINFO * TNC)
{
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	CloseLogfile(0);
	CloseLogfile(1);
	CloseLogfile(2);

	if ((STREAM->Connecting | STREAM->Connected) == 0)
	{
		// Not connected or Connecting. Probably response to going into Pactor Listen Mode

		return;
	}

	if (STREAM->Connecting && STREAM->Disconnecting == FALSE)
	{
		UINT * buffptr;

		// Connect Failed - actually I think HAL uses another code for connect failed, but leave here for now
			
		buffptr = GetBuff();
	
		if (buffptr)
		{
			buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Failure with %s\r", STREAM->RemoteCall);

			C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
		}

		STREAM->Connecting = FALSE;
		STREAM->Connected = FALSE;				// In case!
		STREAM->FramesQueued = 0;

		sprintf(TNC->WEB_TNCSTATE, "In Use by %s", STREAM->MyCall);
		SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

		return;
	}

	// Connected, or Disconnecting - Release Session

	STREAM->Connecting = FALSE;
	STREAM->Connected = FALSE;		// Back to Command Mode
	STREAM->FramesQueued = 0;

	if (STREAM->Disconnecting == FALSE)
		STREAM->ReportDISC = TRUE;		// Tell Node

	STREAM->Disconnecting = FALSE;

	// Need to reset Pactor Call in case it was changed

	TNC->NeedPACTOR = 20;
}

BOOL HALConnected(struct TNCINFO * TNC, char * Call)
{
	char Msg[80];
	UINT * buffptr;
	struct STREAMINFO * STREAM = &TNC->Streams[0];
	char CallCopy[80];

	strcpy(CallCopy, Call);
	strcat(CallCopy, "          ");			// Some routines expect 10 char calls

	STREAM->BytesRXed = STREAM->BytesTXed = STREAM->BytesAcked = 0;
	STREAM->ConnectTime = time(NULL); 

	// Stop Scanner

	sprintf(Msg, "%d SCANSTOP", TNC->Port);
		
	Rig_Command(-1, Msg);

	ShowTraffic(TNC);

	TNC->DataMode = RXDATA; 

	OpenLogfile(0);
	OpenLogfile(1);
	OpenLogfile(2);

	if (TNC->PortRecord->ATTACHEDSESSIONS[0] == 0)
	{
		// Incoming Connect

		ProcessIncommingConnect(TNC, CallCopy, 0, TRUE);
					
		sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound", STREAM->RemoteCall, TNC->NodeCall);
		SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

		if (TNC->CurrentMode != Clover)
			SendCmd(TNC, "\x87", 1);		// Changeover to ISS 

		// If an autoconnect APPL is defined, send it

		if (TNC->ApplCmd)
		{
			buffptr = GetBuff();
			if (buffptr == 0) return TRUE;			// No buffers, so ignore

			buffptr[1] = sprintf((UCHAR *)&buffptr[2], "%s\r", TNC->ApplCmd);
			C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
			TNC->SwallowSignon = TRUE;

			return TRUE;
		}

		if (FULL_CTEXT && HFCTEXTLEN == 0)
		{
			EncodeAndSend(TNC, CTEXTMSG, CTEXTLEN);
			WriteLogLine(2, CTEXTMSG, CTEXTLEN);

			STREAM->BytesTXed += CTEXTLEN;
		}
		return TRUE;
	}

	// Connect Complete
			
	buffptr = GetBuff();
	if (buffptr == 0) return TRUE;			// No buffers, so ignore

	buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Connected to %s\r", Call);;

	C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
	
	STREAM->Connecting = FALSE;
	STREAM->Connected = TRUE;			// Subsequent data to data channel

	sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound", TNC->NodeCall, STREAM->RemoteCall);
	SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

	UpdateMH(TNC, CallCopy, '+', 'O');


	return TRUE;
}	


static VOID EncodeAndSend(struct TNCINFO * TNC, UCHAR * txbuffer, int Len)
{
	// Send A Packet With DLE Encoding Encoding

	TNC->TXLen = DLEEncode(txbuffer, TNC->TXBuffer, Len);

	WriteCommBlock(TNC);
}

VOID SendCmd(struct TNCINFO * TNC, UCHAR * txbuffer, int Len)
{
	// Send A Packet With Command Encoding (preceed each with 0x80

	int i,txptr=0;
	UCHAR * outbuff = TNC->TXBuffer;

	for (i=0; i<Len; i++)
	{
		outbuff[txptr++] = 0x80;
		outbuff[txptr++] = txbuffer[i];
	}

	TNC->TXLen = txptr;
	WriteCommBlock(TNC);
}

int	DLEEncode(UCHAR * inbuff, UCHAR * outbuff, int len)
{
	int i, txptr = 0;
	UCHAR c;

	// Escape x80 and x81 with x81

//	outbuff[0] = 0x80;
//	outbuff[1] = 0x33;		// Send data to modem
	
	for (i=0;i<len;i++)
	{
		c=inbuff[i];
		
		switch (c)
		{
		case 0x80:
		case 0x81:

			outbuff[txptr++] = 0x81;
		}
		outbuff[txptr++] = c;
	}

	return txptr;

}

VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
		SendCmd(TNC, "\x07", 1);		// Tidy Disconnect
}

VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
		SendCmd(TNC, "\x06", 1);		// Hard Disconnect
}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
		TNC->NeedPACTOR = 30;	
}




