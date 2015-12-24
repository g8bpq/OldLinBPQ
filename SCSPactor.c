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
//	DLL to inteface SCS TNC in Pactor Mode to BPQ32 switch 
//
//	Uses BPQ EXTERNAL interface


// Dec 29 2009

//	Add Scan Control using %W Hostmode Command
//	Map Rig control port to a Virtual Serial Port.
//	Add Support for packet port(s).

// July 2010

// Support up to 32 BPQ Ports

// Version 1.1.1.14 August 2010 

// Drop RTS as well as DTR on close

// Version 1.2.1.1 August 2010 

// Save Minimized State

// Version 1.2.1.2 August 2010 

// Implement scan bandwidth change

// Version 1.2.1.3 September 2010 

// Don't connect if channel is busy
// Add WL2K reporting
// Add PACKETCHANNELS config command
// And Port Selector (P1 or P2) for Packet Ports

// Version 1.2.1.4 September 2010

// Fix Freq Display after Node reconfig
// Only use AutoConnect APPL for Pactor Connects

// Version 1.2.2.1 September 2010

// Add option to get config from bpq32.cfg

// October 2011

// Changes for P4Dragon

#ifdef WIN32
#define WRITELOG
#endif

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T


#include <stdio.h>
#include <stdlib.h>
#include "time.h"

//#include <process.h>
//#include <time.h>

#define MaxStreams 10			// First is used for Pactor, even though Pactor uses channel 31

#include "CHeaders.h"
#include "tncinfo.h"

#include "bpq32.h"

#ifndef WIN32
#ifndef MACBPQ
#include <sys/ioctl.h>
#include <linux/serial.h>
#endif
#endif

static char ClassName[]="PACTORSTATUS";
static char WindowTitle[] = "SCS Pactor";
static int RigControlRow = 185;


#define NARROWMODE 12		// PI/II
#define WIDEMODE 16			// PIII only

extern UCHAR BPQDirectory[];

extern char * PortConfig[33];
extern BOOL RIG_DEBUG;

static RECT Rect;

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd
extern char * RigConfigMsg[35];

VOID __cdecl Debugprintf(const char * format, ...);

char NodeCall[11];		// Nodecall, Null Terminated

unsigned long _beginthread( void( *start_address )(), unsigned stack_size, int arglist);
int DoScanLine(struct TNCINFO * TNC, char * Buff, int Len);

VOID SuspendOtherPorts(struct TNCINFO * ThisTNC);
VOID ReleaseOtherPorts(struct TNCINFO * ThisTNC);

VOID PTCSuspendPort(struct TNCINFO * TNC);
VOID PTCReleasePort(struct TNCINFO * TNC);
int	KissEncode(UCHAR * inbuff, UCHAR * outbuff, int len);

#define	FEND	0xC0	// KISS CONTROL CODES 
#define	FESC	0xDB
#define	TFEND	0xDC
#define	TFESC	0xDD

#ifdef WRITELOG
static HANDLE LogHandle[32] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
#endif

//char * Logs[4] = {"1", "2", "3", "4"};

static char BaseDir[MAX_PATH]="c:\\";

static VOID CloseLogFile(int Flags)
{
#ifdef WRITELOG
	CloseHandle(LogHandle[Flags]);
	LogHandle[Flags] = INVALID_HANDLE_VALUE;
#endif
}

static BOOL OpenLogFile(int Flags)
{
#ifdef WRITELOG
	UCHAR FN[MAX_PATH];

	time_t T;
	struct tm * tm;

	T = time(NULL);
	tm = gmtime(&T);	

	sprintf(FN,"%s\\SCSLog_%02d%02d_%d.txt", BPQDirectory, tm->tm_mon + 1, tm->tm_mday, Flags);

	LogHandle[Flags] = CreateFile(FN,
					GENERIC_WRITE,
					FILE_SHARE_READ,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);

	SetFilePointer(LogHandle[Flags], 0, 0, FILE_END);

	return (LogHandle[Flags] != INVALID_HANDLE_VALUE);
#endif
	return 0;
}

static void WriteLogLine(int Flags, char * Msg, int MsgLen)
{
#ifdef WRITELOG
	int cnt;
	WriteFile(LogHandle[Flags], Msg , MsgLen, &cnt, NULL);
	WriteFile(LogHandle[Flags], "\r\n" , 2, &cnt, NULL);
#endif
}




static ProcessLine(char * buf, int Port)
{
	UCHAR * ptr,* p_cmd;
	char * p_ipad = 0;
	char * p_port = 0;
	unsigned short WINMORport = 0;
	int BPQport;
	int len=510;
	struct TNCINFO * TNC;
	char errbuf[256];

	BPQport = Port;
	
	TNC = TNCInfo[BPQport] = malloc(sizeof(struct TNCINFO));
	memset(TNC, 0, sizeof(struct TNCINFO));

	TNC->InitScript = malloc(1000);
	TNC->InitScript[0] = 0;
	
	goto ConfigLine;


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
		
		if (_memicmp(buf, "APPL", 4) == 0)
		{
			p_cmd = strtok(&buf[5], " \t\n\r");

			if (p_cmd && p_cmd[0] != ';' && p_cmd[0] != '#')
				TNC->ApplCmd=_strdup(_strupr(p_cmd));
		}
		else			
		if (_memicmp(buf, "PACKETCHANNELS", 14) == 0)	// Packet Channels
			TNC->PacketChannels = atoi(&buf[14]);
		
		else
		if (_memicmp(buf, "BUSYHOLD", 8) == 0)		// Hold Time for Busy Detect
			TNC->BusyHold = atoi(&buf[8]);

		else
		if (_memicmp(buf, "BUSYWAIT", 8) == 0)		// Wait time beofre failing connect if busy
			TNC->BusyWait = atoi(&buf[8]);

		else
		if (_memicmp(buf, "SCANFORROBUSTPACKET", 19) == 0)
		{
			// Spend a percentage of scan time in Robust Packet Mode

			double Robust = atof(&buf[20]);
			#pragma warning(push)
			#pragma warning(disable : 4244)
			TNC->RobustTime = Robust * 10;
			#pragma warning(pop)
		}
		else
		if (_memicmp(buf, "USEAPPLCALLS", 12) == 0 && buf[12] != 'F' && buf[12] != 'f')
			TNC->UseAPPLCalls = TRUE;
		else
		if (_memicmp(buf, "USEAPPLCALLSFORPACTOR", 21) == 0)
			TNC->UseAPPLCallsforPactor = TRUE;
		else
		if (_memicmp(buf, "DRAGON", 6) == 0)
		{
			TNC->Dragon = TRUE;
			if (_memicmp(&buf[7], "SINGLE", 6) == 0)
				TNC->DragonSingle = TRUE;

			if (_memicmp(&buf[7], "KISS", 4) == 0)
				TNC->DragonKISS = TRUE;
		}
		else
		if (_memicmp(buf, "DEFAULT ROBUST", 14) == 0)
			TNC->RobustDefault = TRUE;
		else
		if (_memicmp(buf, "FORCE ROBUST", 12) == 0)
			TNC->ForceRobust = TNC->RobustDefault = TRUE;
		else
		if (_memicmp(buf, "MAXLEVEL", 8) == 0)		// Maximum Pactor Level to use.
			TNC->MaxLevel = atoi(&buf[8]);
		else
		if (_memicmp(buf, "WL2KREPORT", 10) == 0)
			TNC->WL2K = DecodeWL2KReportLine(buf);
		else
		if (_memicmp(buf, "DATE", 4) == 0)
		{
			char Cmd[32];
			time_t T;
			struct tm * tm;

			T = time(NULL);
			tm = gmtime(&T);	

			sprintf(Cmd,"DATE %02d%02d%02d\r",  tm->tm_mday, tm->tm_mon + 1, tm->tm_year - 100);

			strcat (TNC->InitScript, Cmd);
		}
		else if (_memicmp(buf, "TIME", 4) == 0)
		{
			char Cmd[32];
			time_t T;
			struct tm * tm;

			T = time(NULL);
			tm = gmtime(&T);	

			sprintf(Cmd,"TIME %02d%02d%02d\r",  tm->tm_hour, tm->tm_min, tm->tm_sec);

			strcat (TNC->InitScript, Cmd);
		}
		else
			strcat (TNC->InitScript, buf);
	}
	
	return (TRUE);
	
}

struct TNCINFO * CreateTTYInfo(int port, int speed);
BOOL OpenConnection(int);
BOOL SetupConnection(int);
BOOL CloseConnection(struct TNCINFO * conn);
BOOL WriteCommBlock(struct TNCINFO * TNC);
BOOL DestroyTTYInfo(int port);
void SCSCheckRX(struct TNCINFO * TNC);
VOID SCSPoll(int Port);
VOID CRCStuffAndSend(struct TNCINFO * TNC, UCHAR * Msg, int Len);
unsigned short int compute_crc(unsigned char *buf,int len);
int Unstuff(UCHAR * MsgIn, UCHAR * MsgOut, int len);
VOID ProcessDEDFrame(struct TNCINFO * TNC, UCHAR * rxbuff, int len);
VOID ProcessTermModeResponse(struct TNCINFO * TNC);
VOID ExitHost(struct TNCINFO * TNC);
VOID DoTNCReinit(struct TNCINFO * TNC);
VOID DoTermModeTimeout(struct TNCINFO * TNC);
static VOID DoMonitor(struct TNCINFO * TNC, UCHAR * Msg, int Len);
int Switchmode(struct TNCINFO * TNC, int Mode);
VOID SwitchToPactor(struct TNCINFO * TNC);
VOID SwitchToPacket(struct TNCINFO * TNC);


char status[8][8] = {"ERROR",  "REQUEST", "TRAFFIC", "IDLE", "OVER", "PHASE", "SYNCH", ""};

char ModeText[8][14] = {"STANDBY", "AMTOR-ARQ",  "PACTOR-ARQ", "AMTOR-FEC", "PACTOR-FEC", "RTTY / CW", "LISTEN", "Channel-Busy"};

char PactorLevelText[5][14] = {"Not Connected", "PACTOR-I", "PACTOR-II", "PACTOR-III", "PACTOR-IV"};

char PleveltoMode[5] = {30, 11, 12, 16, 19};	// WL2K Reporting Modes - RP, P1, P2, P3, P4


static int ExtProc(int fn, int port,unsigned char * buff)
{
	int txlen = 0;
	UINT * buffptr;
	struct TNCINFO * TNC = TNCInfo[port];
	int Param;
	int Stream = 0;
	struct STREAMINFO * STREAM;
	char PLevel;
	struct ScanEntry * Scan;

	if (TNC == NULL)
		return 0;
	
	if (TNC->hDevice == 0)
	{
		// Clear anything from UI_Q

		while (TNC->PortRecord->UI_Q)
		{
			buffptr = Q_REM(&TNC->PortRecord->UI_Q);
			ReleaseBuffer(buffptr);
		}

		// Try to reopen every 30 secs

		if (fn > 3  && fn < 7)
			goto ok;

		TNC->ReopenTimer++;

		if (TNC->ReopenTimer < 300)
			return 0;

		TNC->ReopenTimer = 0;
		
		OpenCOMMPort(TNC, TNC->PortRecord->PORTCONTROL.SerialPortName, TNC->PortRecord->PORTCONTROL.BAUDRATE, TRUE);

		if (TNC->hDevice == 0)
			return 0;

#ifndef WIN32
#ifndef MACBPQ

		if (TNC->Dragon)
		{
			struct serial_struct sstruct;

			// Need to set custom baud rate

			if (ioctl(TNC->hDevice, TIOCGSERIAL, &sstruct) < 0)
			{
				Debugprintf("Error: Dragon could not get comm ioctl\n");
			}
			else
			{
				// set custom divisor to get 829440 baud
	
				sstruct.custom_divisor = 29;
				sstruct.flags |= ASYNC_SPD_CUST;

				// set serial_struct
		
				if (ioctl(TNC->hDevice, TIOCSSERIAL, &sstruct) < 0)
					Debugprintf("Error: Dragon could not set custom comm baud divisor\n");
				else
					Debugprintf("Dragon custom baud rate set\n");
			}
		}
#endif
#endif
	}
ok:
	switch (fn)
	{
	case 7:			

		// 100 mS Timer. May now be needed, as Poll can be called more frequently in some circumstances

		SCSCheckRX(TNC);
		SCSPoll(port);

		return 0;

	case 1:				// poll

		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			if (TNC->Streams[Stream].ReportDISC)
			{
				TNC->Streams[Stream].ReportDISC = FALSE;
				buff[4] = Stream;

				return -1;
			}
		}
	
		if (TNC->EnterExit)
			return 0;						// Switching to Term mode to change bandwidth
		
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

		TNC->Streams[Stream].FramesOutstanding++;
		
		return (0);

	case 3:				// CHECK IF OK TO SEND. Also used to check if TNC is responding

		Stream = (int)buff;
	
		STREAM = &TNC->Streams[Stream];

		if (Stream == 0)
		{
			if (STREAM->FramesOutstanding  > 10)
				return (1 | TNC->HostMode << 8 | STREAM->Disconnecting << 15);
		}
		else
		{
			if (STREAM->FramesOutstanding > 3 || TNC->Buffers < 200)	
				return (1 | TNC->HostMode << 8 | STREAM->Disconnecting << 15);		}

		return TNC->HostMode << 8 | STREAM->Disconnecting << 15;		// OK, but lock attach if disconnecting


	case 4:				// reinit

		// Ensure in Pactor

		TNC->TXBuffer[2] = 31;
		TNC->TXBuffer[3] = 0x1;
		TNC->TXBuffer[4] = 0x1;
		memcpy(&TNC->TXBuffer[5], "PT", 2);

		CRCStuffAndSend(TNC, TNC->TXBuffer, 7);

		Sleep(25);
		ExitHost(TNC);
		Sleep(50);
		CloseCOMPort(TNC->hDevice);
		TNC->hDevice =(HANDLE) -1;
		TNC->ReopenTimer = 250;
		TNC->HostMode = FALSE;

		return (0);

	case 5:				// Close

		// Ensure in Pactor

		TNC->TXBuffer[2] = 31;
		TNC->TXBuffer[3] = 0x1;
		TNC->TXBuffer[4] = 0x1;
		memcpy(&TNC->TXBuffer[5], "PT", 2);

		CRCStuffAndSend(TNC, TNC->TXBuffer, 7);

		Sleep(25);

		ExitHost(TNC);

		Sleep(25);

		CloseCOMPort(TNCInfo[port]->hDevice);
				
		return (0);

	case 6:				// Scan Interface

		Param = (int)buff;

		switch (Param)
		{
		case 1:		// Request Permission

			if (TNC->TNCOK)
			{
				// If been in Sync a long time, or if using applcalls and
				// Scan had been locked too long just let it change
				
				if (TNC->UseAPPLCallsforPactor)
				{
					if (TNC->PTCStatus == 6)	// Sync
					{
						int insync = time(NULL) - TNC->TimeEnteredSYNCMode;
						if (insync > 4)
						{
							Debugprintf("SCS Scan - in SYNC for %d Secs - allow change regardless", insync);
							return 0;
						}
					}
					else if (TNC->TimeScanLocked)
					{
						int timeLocked = time(NULL) - TNC->TimeScanLocked;
						if (timeLocked > 4)
						{
							Debugprintf("SCS Scan - Scan Locked for %d Secs - allow change regardless", timeLocked);
							TNC->TimeScanLocked = 0;
							return 0;
						}
					}
				}

				TNC->WantToChangeFreq = TRUE;
				TNC->OKToChangeFreq = FALSE;
				return TRUE;
			}
			return 0;		// Don't lock scan if TNC isn't responding
		

		case 2:		// Check  Permission
			return TNC->OKToChangeFreq;

		case 3:		// Release  Permission
		
			TNC->WantToChangeFreq = FALSE;
			
			if (TNC->DontReleasePermission)			// Disable connects during this interval?
			{
				TNC->DontReleasePermission = FALSE;
				if (TNC->SyncSupported == FALSE)
					TNC->TimeScanLocked = time(NULL) + 100;	// Make sure doesnt time out
				return 0;
			}

			TNC->DontWantToChangeFreq = TRUE;
			return 0;

		default: // Param is Address of a struct ScanEntry

			Scan = (struct ScanEntry *)buff;

			PLevel = Scan->PMaxLevel;

			if (PLevel == 0 && (Scan->HFPacketMode || Scan->RPacketMode))
			{
				// Switch to Packet for this Interval
				
				if (RIG_DEBUG)
					Debugprintf("SCS Switching to Packet, %d", TNC->HFPacket);

				if (TNC->HFPacket == FALSE)
					SwitchToPacket(TNC);

				return 0;
			}

			if (PLevel > '0' && PLevel < '5')		// 1 - 4 
			{
				if (TNC->Bandwidth != PLevel)
				{
					TNC->Bandwidth = PLevel;
					TNC->MinLevel = Scan->PMinLevel - '0';
					Switchmode(TNC, PLevel - '0');
				}

				if (TNC->UseAPPLCallsforPactor && Scan->APPLCALL[0])
				{
					// Switch callsign

					STREAM = &TNC->Streams[0];
					STREAM->CmdSet = STREAM->CmdSave = malloc(100);

					strcpy(STREAM->MyCall, Scan->APPLCALL);

					sprintf(STREAM->CmdSet, "I%s\rI\r", STREAM->MyCall);
					if (RIG_DEBUG)
						Debugprintf("SCS Pactor APPLCALL Set to  %s", STREAM->MyCall);
				}

				else
				{
					if (TNC->HFPacket)
						SwitchToPactor(TNC);
				}
			}

			if (Scan->RPacketMode)
				if (TNC->RobustTime)
					SwitchToPacket(TNC);			// Always start in packet, switch to pactor after RobustTime ticks
			
			if (PLevel == '0')
				TNC->DontReleasePermission = TRUE;	// Dont allow connects in this interval
			else
				TNC->DontReleasePermission = FALSE;

			return 0;
		}
	}
	return 0;
}

int DoScanLine(struct TNCINFO * TNC, char * Buff, int Len)
{
	struct RIGINFO * RIG = TNC->RIG;

	if (RIG && RIG->WEB_Label)
	{
		Len += sprintf(&Buff[Len], "<table style=\"font-family: monospace; align=center \"><tr>");
		Len += sprintf(&Buff[Len], "<td width=90px>%s</td>", RIG->WEB_Label);
		Len += sprintf(&Buff[Len], "<td width=90px>%s</td>", RIG->WEB_FREQ);
		Len += sprintf(&Buff[Len], "<td width=90px>%s</td>", RIG->WEB_MODE);
		Len += sprintf(&Buff[Len], "<td width=90px>%c</td>", RIG->WEB_SCAN);
		Len += sprintf(&Buff[Len], "<td width=90px>%c</td></tr></table>", RIG->WEB_PTT);
	}
	return Len;
}

static int WebProc(struct TNCINFO * TNC, char * Buff, BOOL LOCAL)
{
	int Len = sprintf(Buff, "<html><meta http-equiv=expires content=0><meta http-equiv=refresh content=15>"
	"<head><title>SCS Pactor Status</title></head><body><h3>SCS Pactor Status</h3>");

	Len += sprintf(&Buff[Len], "<table style=\"text-align: left; width: 480px; font-family: monospace; align=center \" border=1 cellpadding=2 cellspacing=2>");

	Len += sprintf(&Buff[Len], "<tr><td width=90px>Comms State</td><td>%s</td></tr>", TNC->WEB_COMMSSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>TNC State</td><td>%s</td></tr>", TNC->WEB_TNCSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>Mode</td><td>%s</td></tr>", TNC->WEB_MODE);
	Len += sprintf(&Buff[Len], "<tr><td>Status</td><td>%s</td></tr>", TNC->WEB_STATE);
	Len += sprintf(&Buff[Len], "<tr><td>TX/RX State</td><td>%s</td></tr>", TNC->WEB_TXRX);
	Len += sprintf(&Buff[Len], "<tr><td>Buffers</td><td>%s</td></tr>", TNC->WEB_BUFFERS);
	Len += sprintf(&Buff[Len], "<tr><td>Traffic</td><td>%s</td></tr>", TNC->WEB_TRAFFIC);
	Len += sprintf(&Buff[Len], "<tr><td>Mode</td><td>%s</td></tr>", TNC->WEB_PACTORLEVEL);
	Len += sprintf(&Buff[Len], "</table>");

	Len = DoScanLine(TNC, Buff, Len);

	return Len;
}

UINT SCSExtInit(EXTPORTDATA *  PortEntry)
{
	char msg[500];
	struct TNCINFO * TNC;
	int port;
	char * ptr;
	int Stream = 0;
	char * TempScript;

	//
	//	Will be called once for each Pactor Port
	//	The COM port number is in IOBASE
	//

	sprintf(msg,"SCS Pactor %s", PortEntry->PORTCONTROL.SerialPortName);
	WritetoConsole(msg);

	port=PortEntry->PORTCONTROL.PORTNUMBER;

	ReadConfigFile(port, ProcessLine);

	TNC = TNCInfo[port];

	if (TNC == NULL)
	{
		// Not defined in Config file

		sprintf(msg," ** Error - no info in BPQ32.cfg for this port\n");
		WritetoConsole(msg);

		return (int) ExtProc;
	}
	
	TNC->Port = port;
	TNC->Hardware = H_SCS;

	OpenLogFile(TNC->Port);
	CloseLogFile(TNC->Port);


	if (TNC->BusyHold == 0)
		TNC->BusyHold = 3;

	if (TNC->BusyWait == 0)
		TNC->BusyWait = 10;

	if (TNC->MaxLevel == 0)
		TNC->MaxLevel = 3;

	// Set up DED addresses for streams (first stream (Pactor) = DED 31
	
	TNC->Streams[0].DEDStream = 31;

	for (Stream = 1; Stream <= MaxStreams; Stream++)
	{
		TNC->Streams[Stream].DEDStream = Stream;
	}

	if (TNC->PacketChannels > MaxStreams)
		TNC->PacketChannels = MaxStreams;

	PortEntry->MAXHOSTMODESESSIONS = TNC->PacketChannels + 1;
	PortEntry->PERMITGATEWAY = TRUE;					// Can change ax.25 call on each stream
	PortEntry->SCANCAPABILITIES = CONLOCK;				// Scan Control 3 stage/conlock 

	TNC->PortRecord = PortEntry;

	TNC->Interlock = PortEntry->PORTCONTROL.PORTINTERLOCK;

	if (PortEntry->PORTCONTROL.PORTCALL[0] == 0)
		memcpy(TNC->NodeCall, MYNODECALL, 10);
	else
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);
		
	PortEntry->PORTCONTROL.PROTOCOL = 10;
	PortEntry->PORTCONTROL.PORTQUALITY = 0;

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0)
		PortEntry->PORTCONTROL.PORTPACLEN = 100;

	TNC->SuspendPortProc = PTCSuspendPort;
	TNC->ReleasePortProc = PTCReleasePort;

	PortEntry->PORTCONTROL.UICAPABLE = TRUE;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	// get NODECALL for RP tests

	memcpy(NodeCall, MYNODECALL, 10);
		
	ptr=strchr(NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate


	// Set TONES to 4

	TempScript = malloc(1000);

	strcpy(TempScript, "QUIT\r");				// In case in pac: mode
	strcat(TempScript, "TONES 4\r");			// Tones may be changed but I want this as standard
	strcat(TempScript, "MAXERR 30\r");			// Max retries 
	strcat(TempScript, "MODE 0\r");				// ASCII mode, no PTC II compression (Forwarding will use FBB Compression)
	strcat(TempScript, "MAXSUM 20\r");			// Max count for memory ARQ
	strcat(TempScript, "CWID 0 2\r");			// CW ID disabled
	strcat(TempScript, "PTCC 0\r");				// Dragon out of PTC Compatibility Mode
	strcat(TempScript, "VER\r");				// Try to determine Controller Type

	sprintf(msg, "MYLEVEL %d\r", TNC->MaxLevel);
	strcat(TempScript, msg);					// Default Level to MAXLEVEL

	strcat(TempScript, TNC->InitScript);

	free(TNC->InitScript);
	TNC->InitScript = TempScript;

	// Others go on end so they can't be overriden

	strcat(TNC->InitScript, "ADDLF 0\r");      //      Auto Line Feed disabled
	strcat(TNC->InitScript, "ARX 0\r");      //        Amtor Phasing disabled
	strcat(TNC->InitScript, "BELL 0\r");      //       Disable Bell
	strcat(TNC->InitScript, "BC 0\r");      //         FEC reception is disabled
	strcat(TNC->InitScript, "BKCHR 2\r");      //      Breakin Char = 2
	strcat(TNC->InitScript, "CHOBELL 0\r");      //    Changeover Bell off
	strcat(TNC->InitScript, "CMSG 0\r");      //       Connect Message Off
	strcat(TNC->InitScript, "LFIGNORE 0\r");      //   No insertion of Line feed
	strcat(TNC->InitScript, "LISTEN 0\r");      //     Pactor Listen disabled
	strcat(TNC->InitScript, "MAIL 0\r");      //       Disable internal mailbox reporting
	strcat(TNC->InitScript, "REMOTE 0\r");      //     Disable remote control
	strcat(TNC->InitScript, "PAC CBELL 0\r");      //  
	strcat(TNC->InitScript, "PAC CMSG 0\r");      //  
	strcat(TNC->InitScript, "PAC PRBOX 0\r");      //  	Turn off Packet Radio Mailbox
	
	//  Automatic Status must be enabled for BPQ32
	//  Pactor must use Host Mode Chanel 31
	//  PDuplex must be set. The Node code relies on automatic IRS/ISS changeover
	//	5 second duplex timer

	strcat(TNC->InitScript, "STATUS 2\rPTCHN 31\rPDUPLEX 1\rPDTIMER 5\r");

	sprintf(msg, "MYCALL %s\rPAC MYCALL %s\r", TNC->NodeCall, TNC->NodeCall);
	strcat(TNC->InitScript, msg);

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
	TNC->WEB_PACTORLEVEL = zalloc(100);

#ifndef LINBPQ

	CreatePactorWindow(TNC, ClassName, WindowTitle, RigControlRow, PacWndProc, 235, 500);
 
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
	TNC->xIDC_TRAFFIC = CreateWindowEx(0, "STATIC", "RX 0 TX 0 ACKED 0", WS_CHILD | WS_VISIBLE,116,138,374,20 , TNC->hDlg, NULL, hInstance, NULL);

	TNC->xIDC_PACTORLEVEL = CreateWindowEx(0, "STATIC", "Mode", WS_CHILD | WS_VISIBLE,10,160,430,20, TNC->hDlg, NULL, hInstance, NULL);

	TNC->ClientHeight = 240;
	TNC->ClientWidth = 500;
	
	MoveWindows(TNC);
#endif
	OpenCOMMPort(TNC, PortEntry->PORTCONTROL.SerialPortName, PortEntry->PORTCONTROL.BAUDRATE, FALSE);

#ifndef WIN32
#ifndef MACBPQ

	if (TNC->Dragon)
	{
		struct serial_struct sstruct;

		// Need to set custom baud rate

		if (ioctl(TNC->hDevice, TIOCGSERIAL, &sstruct) < 0)
		{
			printf("Error: Dragon could not get comm ioctl\n");
		}
		else
		{
			// set custom divisor to get 829440 baud
	
			sstruct.custom_divisor = 29;
			sstruct.flags |= ASYNC_SPD_CUST;

			// set serial_struct
		
			if (ioctl(TNC->hDevice, TIOCSSERIAL, &sstruct) < 0)
				Debugprintf("Error: Dragon could not set custom comm baud divisor\n");
			else
				Debugprintf("Dragon custom baud rate set\n");
		}
	}
#endif
#endif

	if (TNC->RobustDefault)
		SwitchToPacket(TNC);

	WritetoConsole("\n");

	return ((int)ExtProc);
}

void SCSCheckRX(struct TNCINFO * TNC)
{
	int Length, Len;
	unsigned short crc;
	char UnstuffBuffer[500];

	if (TNC->RXLen == 500)
		TNC->RXLen = 0;

	Len = ReadCOMBlock(TNC->hDevice, &TNC->RXBuffer[TNC->RXLen], 500 - TNC->RXLen);

	if (Len == 0)
		return;
	
	TNC->RXLen += Len;

	Length = TNC->RXLen;

	// DED mode doesn't have an end of frame delimiter. We need to know if we have a full frame

	// Fortunately this is a polled protocol, so we only get one frame at a time

	// If first char != 170, then probably a Terminal Mode Frame. Wait for CR on end

	// If first char is 170, we could check rhe length field, but that could be corrupt, as
	// we haen't checked CRC. All I can think of is to check the CRC and if it is ok, assume frame is
	// complete. If CRC is duff, we will eventually time out and get a retry. The retry code
	// can clear the RC buffer
			
	if (TNC->RXBuffer[0] != 170)
	{
		// Char Mode Frame I think we need to see cmd: on end

		// If we think we are in host mode, then to could be noise - just discard.

		if (TNC->HostMode)
		{
			TNC->RXLen = 0;		// Ready for next frame
			return;
		}

		TNC->RXBuffer[TNC->RXLen] = 0;

//		if (TNC->Streams[Stream].RXBuffer[TNC->Streams[Stream].RXLen-2] != ':')

		if (strlen(TNC->RXBuffer) < TNC->RXLen)
			TNC->RXLen = 0;

		if ((strstr(TNC->RXBuffer, "cmd: ") == 0) && (strstr(TNC->RXBuffer, "pac: ") == 0))

			return;				// Wait for rest of frame

		// Complete Char Mode Frame

		OpenLogFile(TNC->Port);
		WriteLogLine(TNC->Port, TNC->RXBuffer, strlen(TNC->RXBuffer));
		CloseLogFile(TNC->Port);

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

	if (Length < 6)				// Minimum Frame Sise
		return;

	if (TNC->RXBuffer[2] == 170)
	{
		// Retransmit Request
	
		TNC->RXLen = 0;
		return;				// Ignore for now
	}

	// Can't unstuff into same buffer - fails if partial msg received, and we unstuff twice


	Length = Unstuff(&TNC->RXBuffer[2], &UnstuffBuffer[2], Length - 2);

	if (Length == -1)
	{
		// Unstuff returned an errors (170 not followed by 0)

		TNC->RXLen = 0;
		return;				// Ignore for now
	}
	crc = compute_crc(&UnstuffBuffer[2], Length);

	if (crc == 0xf0b8)		// Good CRC
	{
		TNC->RXLen = 0;		// Ready for next frame
		ProcessDEDFrame(TNC, UnstuffBuffer, Length);

		// If there are more channels to poll (more than 1 entry in general poll response,
		// and link is not active, poll the next one

		if (TNC->Timeout == 0)
		{
			UCHAR * Poll = TNC->TXBuffer;

			if (TNC->NexttoPoll[0])
			{
				UCHAR Chan = TNC->NexttoPoll[0] - 1;

				memmove(&TNC->NexttoPoll[0], &TNC->NexttoPoll[1], 19);

				Poll[2] = Chan;			// Channel
				Poll[3] = 0x1;			// Command

				if (Chan == 254)		// Status - Send Extended Status (G3)
				{
					Poll[4] = 1;			// Len-1
					Poll[5] = 'G';			// Extended Status Poll
					Poll[6] = '3';
				}
				else
				{
					Poll[4] = 0;			// Len-1
					Poll[5] = 'G';			// Poll
				}

				CRCStuffAndSend(TNC, Poll, Poll[4] + 6);
				TNC->InternalCmd = FALSE;

				return;
			}
			else
			{
				// if last message wasn't a general poll, send one now

				if (TNC->PollSent)
					return;

				TNC->PollSent = TRUE;

				// Use General Poll (255)

				Poll[2] = 255 ;			// Channel
				Poll[3] = 0x1;			// Command

				Poll[4] = 0;			// Len-1
				Poll[5] = 'G';			// Poll

				CRCStuffAndSend(TNC, Poll, 6);
				TNC->InternalCmd = FALSE;
			}
		}
		return;
	}

	// Bad CRC - assume incomplete frame, and wait for rest. If it was a full bad frame, timeout and retry will recover link.

	return;
}

BOOL WriteCommBlock(struct TNCINFO * TNC)
{
	BOOL ret = WriteCOMBlock(TNC->hDevice, TNC->TXBuffer, TNC->TXLen);

	TNC->Timeout = 20;				// 2 secs
	return ret;
}

VOID SCSPoll(int Port)
{
	struct TNCINFO * TNC = TNCInfo[Port];
	UCHAR * Poll = TNC->TXBuffer;
	char Status[80];
	int Stream = 0;
	int nn;
	struct STREAMINFO * STREAM;

	if (TNC->MinLevelTimer)
	{
		TNC->MinLevelTimer--;
	
		if (TNC->MinLevelTimer == 0)
		{
			// Failed to reach min level in 15 secs

			STREAM = &TNC->Streams[0];

			if (STREAM->Connected)
			{
				UINT * buffptr;

				Debugprintf("Required Min Level not reached - disconnecting");

				// Discard Queued Data, Send a Message, then a disconnect

				while (STREAM->BPQtoPACTOR_Q)
					ReleaseBuffer(Q_REM(&STREAM->BPQtoPACTOR_Q));

				STREAM->NeedDisc = 15;				// 1 secs
				
				buffptr = GetBuff();
				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1] = sprintf((char *)&buffptr[2],
					"This port only allows Pactor Level %d or above - Disconnecting\r\n", TNC->MinLevel);

				C_Q_ADD(&STREAM->BPQtoPACTOR_Q, buffptr);
			}
		}
	}

	if (TNC->SwitchToPactor)
	{
		TNC->SwitchToPactor--;
	
		if (TNC->SwitchToPactor == 0)
			SwitchToPactor(TNC);
	}
		
	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] && TNC->Streams[Stream].Attached == 0)
		{
			// New Attach

			// If Pactor, stop scanning and take out of listen mode.

			// Set call to connecting user's call

			// If Stream 0 Put in Pactor Mode so Busy Detect will work

			int calllen=0;

			STREAM = &TNC->Streams[Stream];
			Debugprintf("SCS New Attach Stream %d DEDStream %d", Stream, STREAM->DEDStream);

			if (Stream == 0)
				STREAM->DEDStream = 31;	// Pactor

			STREAM->Attached = TRUE;

			calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4USER, STREAM->MyCall);

			STREAM->MyCall[calllen] = 0;
				
			STREAM->CmdSet = STREAM->CmdSave = malloc(100);

			if (Stream == 0)
			{
				// Release Scan Lock if it is held
				
				if (TNC->DontReleasePermission)
				{
					TNC->DontReleasePermission = FALSE;
					TNC->DontWantToChangeFreq = TRUE;
				}

				sprintf(STREAM->CmdSet, "I%s\r", "SCSPTC");

				Debugprintf("SCS Pactor CMDSet = %s", STREAM->CmdSet);

				SuspendOtherPorts(TNC);			// Prevent connects on other ports in same scan gruop

				sprintf(TNC->WEB_TNCSTATE, "In Use by %s", STREAM->MyCall);
				SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

				// Stop Scanner
		
				sprintf(Status, "%d SCANSTOP", TNC->Port);
				TNC->SwitchToPactor = 0;						// Cancel any RP to Pactor switch
		
				Rig_Command(-1, Status);
			}
			else
			{
				sprintf(STREAM->CmdSet, "I%s\r", STREAM->MyCall);
				Debugprintf("SCS Pactor Attach CMDSet = %s", STREAM->CmdSet);
		}
	}
	}

	if (TNC->Timeout)
	{
		TNC->Timeout--;
		
		if (TNC->Timeout)			// Still waiting
			return;

		TNC->Retries--;

		if(TNC->Retries)
		{
			WriteCommBlock(TNC);	// Retransmit Block
			return;
		}

		// Retried out.

		if (TNC->HostMode == 0)
		{
			DoTermModeTimeout(TNC);
			return;
		}

		// Retried out in host mode - Clear any connection and reinit the TNC

		Debugprintf("PACTOR - Link to TNC Lost");
		TNC->TNCOK = FALSE;

		sprintf(TNC->WEB_COMMSSTATE,"%s Open but TNC not responding", TNC->PortRecord->PORTCONTROL.SerialPortName);
		SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

		// Clear anything from UI_Q

		while (TNC->PortRecord->UI_Q)
		{
			UINT * buffptr = Q_REM(&TNC->PortRecord->UI_Q);
			ReleaseBuffer(buffptr);
		}


		TNC->HostMode = 0;
		TNC->ReinitState = 0;
		
		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			if (TNC->PortRecord->ATTACHEDSESSIONS[Stream])		// Connected
			{
				TNC->Streams[Stream].Connected = FALSE;		// Back to Command Mode
				TNC->Streams[Stream].ReportDISC = TRUE;		// Tell Node
			}
		}
	}

	// We delay clearing busy for BusyHold secs

	if (TNC->Busy)
		if (TNC->Mode != 7)
			TNC->Busy--;

	if (TNC->BusyDelay)		// Waiting to send connect
	{
		// Still Busy?

		if (InterlockedCheckBusy(TNC) == 0)
		{
			// No, so send

			TNC->Streams[0].CmdSet = TNC->ConnectCmd;
			TNC->Streams[0].Connecting = TRUE;

			sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
			SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

			Debugprintf("SCS Pactor CMDSet = %s", TNC->Streams[0].CmdSet);

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

	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		STREAM = &TNC->Streams[Stream];

		if (STREAM->Attached)
			CheckForDetach(TNC, Stream, STREAM, TidyClose, ForcedClose, CloseComplete);

		if (STREAM->NeedDisc)
		{
			STREAM->NeedDisc--;

			if (STREAM->NeedDisc == 0)
					STREAM->ReportDISC = TRUE;

		}

		if (TNC->Timeout)
			return;				// We've sent something
	}

	// if we have just restarted or TNC appears to be in terminal mode, run Initialisation Sequence

	if (!TNC->HostMode)
	{
		DoTNCReinit(TNC);
		return;
	}

	TNC->PollSent = FALSE;

	//If sending internal command list, send next element

	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		if (TNC->Streams[Stream].CmdSet)
		{
			unsigned char * start, * end;
			int len;

			start = TNC->Streams[Stream].CmdSet;
			
			if (*(start) == 250)			// 2nd part of long KISS packet
			{
				len = start[1];
				
				Poll[2] = 250;				// KISS Channel
				Poll[3] = 0;				// Data
				Poll[4] = len - 1;
				memcpy(&Poll[5], &start[2], len);
		
				CRCStuffAndSend(TNC, Poll, len + 5);

				free(TNC->Streams[Stream].CmdSave);
				TNC->Streams[Stream].CmdSet = NULL;
				return;
			}

			if (*(start) == 0)			// End of Script
			{
				free(TNC->Streams[Stream].CmdSave);
				TNC->Streams[Stream].CmdSet = NULL;
			}
			else
			{
				end = strchr(start, 13);
				len = ++end - start -1;	// exclude cr
				TNC->Streams[Stream].CmdSet = end;

				if (*(start) == 1)
				{
					// This is UI data, not a command. Send it to channel 0

					len --;

					Poll[2] = 0;				// UI Channel
					Poll[3] = 0;				// Data
					Poll[4] = len - 1;
					memcpy(&Poll[5], &start[1], len);
		
					CRCStuffAndSend(TNC, Poll, len + 5);
	
					return;
				}

				if (*(start) == 2)
				{
					// This is a UI command Send it to channel 0

					len--;

					Poll[2] = 0;				// UI Channel
					Poll[3] = 1;				// Command
					Poll[4] = len - 1;
					memcpy(&Poll[5], &start[1], len);
		
					CRCStuffAndSend(TNC, Poll, len + 5);
	
					return;
				}

				Poll[2] = TNC->Streams[Stream].DEDStream;		// Channel
				Poll[3] = 1;			// Command
				Poll[4] = len - 1;
				memcpy(&Poll[5], start, len);
		

				OpenLogFile(TNC->Port);
				WriteLogLine(TNC->Port, &Poll[5], len);
				CloseLogFile(TNC->Port);

				CRCStuffAndSend(TNC, Poll, len + 5);

				return;
			}
		}
	}
	// if Freq Change needed, check if ok to do it.
	
	if (TNC->TNCOK)
	{
		if (TNC->WantToChangeFreq)
		{
			Poll[2] = 31;			// Command
			Poll[3] = 1;			// Command
			Poll[4] = 2;			// Len -1
			Poll[5] = '%';
			Poll[6] = 'W';
			Poll[7] = '0';
		
			CRCStuffAndSend(TNC, Poll, 8);

			TNC->InternalCmd = TRUE;
			TNC->WantToChangeFreq = FALSE;

			return;
		}

		if (TNC->DontWantToChangeFreq)
		{
			Poll[2] = 31;			// Command
			Poll[3] = 1;			// Command
			Poll[4] = 2;			// Len -1
			Poll[5] = '%';
			Poll[6] = 'W';
			Poll[7] = '1';
		
			CRCStuffAndSend(TNC, Poll, 8);

			TNC->InternalCmd = TRUE;
			TNC->DontWantToChangeFreq = FALSE;
			TNC->OKToChangeFreq = FALSE;

			return;
		}
	}

	// Send Radio Command if avail

	if (TNC->TNCOK && TNC->BPQtoRadio_Q)
	{
		int datalen;
		UINT * buffptr;
			
		buffptr=Q_REM(&TNC->BPQtoRadio_Q);

		datalen=buffptr[1];

		Poll[2] = 253;		// Radio Channel
		Poll[3] = 0;		// Data?
		Poll[4] = datalen - 1;
	
		memcpy(&Poll[5], buffptr+2, datalen);
		
		ReleaseBuffer(buffptr);
		
		CRCStuffAndSend(TNC, Poll, datalen + 5);

		if (RIG_DEBUG)
		{
			Debugprintf("SCS Rig Command Queued, Len = %d", datalen );
			Debugprintf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
				Poll[5], Poll[6], Poll[7], Poll[8], Poll[9], Poll[10], Poll[11], Poll[12],
				Poll[13], Poll[14], Poll[15], Poll[16], Poll[17], Poll[18], Poll[19], Poll[20]);
		}

//		Debugprintf("SCS Sending Rig Command");

		return;
	}

	if (TNC->TNCOK && TNC->PortRecord->UI_Q)
	{
		int datalen;
		char * Buffer;
		char ICall[16];
		char CCMD[80] = "C";
		char Call[12] = "           ";	
		struct _MESSAGE * buffptr;
			
		buffptr = Q_REM(&TNC->PortRecord->UI_Q);
		
		datalen = buffptr->LENGTH - 7;
		Buffer = &buffptr->DEST[0];		// Raw Frame
		
		Buffer[datalen] = 0;
							
		// Buffer has an ax.25 header, which we need to pick out and set as channel 0 Connect address
		// before sending the beacon

		// If a Dragon with KISS over Hostmade we can just send it

		if (TNC->DragonKISS)
		{
			int EncLen;

			Poll[2] = 250;				// KISS Channel
			Poll[3] = 0;				// CMD
			Poll[4] = datalen + 2;		// 3 extrac chars, but need Len - 1

			Buffer--;
			*(Buffer) = 0;			// KISS Control on front
			EncLen = KissEncode(Buffer, &Poll[5], datalen + 1);

			// We can only send 256 bytes in HostMode, so if longer will
			// have to fragemt

			if (EncLen > 256)
			{
				//We have to wait for response before sending rest, so use CmdSet

				// We need to save the extra first, as CRC will overwrite the first two bytes

				UCHAR * ptr = TNC->Streams[0].CmdSet = TNC->Streams[0].CmdSave = zalloc(400);
		
				(*ptr++) = 250;			// KISS Channel
				(*ptr++) = EncLen - 256;
				memcpy(ptr, &Poll[5 + 256], EncLen - 256);

				// Send first 256

				Poll[4] = 255;			//need Len - 1
				CRCStuffAndSend(TNC, Poll, 261);	
			}
			else
			{
				Poll[4] = EncLen - 1;	//need Len - 1
				CRCStuffAndSend(TNC, Poll, EncLen + 5);	
			}

			ReleaseBuffer((UINT *)buffptr);
			return;
		}

		// We also need to set Chan 0 Mycall so digi'ing can work, and put
		// it back after so incoming calls will work

		// But we cant digipeated bit in call, so if we find one, skip message

		ConvFromAX25(Buffer + 7, ICall);		// Origin
		strlop(ICall, ' ');
	
		ConvFromAX25(Buffer, &Call[1]);			// Dest
		strlop(&Call[1], ' ');
		strcat(CCMD, Call);
		Buffer += 14;							// Skip Origin
		datalen -= 7;

		while ((Buffer[-1] & 1) == 0)
		{
			if (Buffer[6] & 0x80)				// Digied bit set?
			{
				ReleaseBuffer((UINT *)buffptr);
				return;
			}

			ConvFromAX25(Buffer, &Call[1]);
			strlop(&Call[1], ' ');
			strcat(CCMD, Call);
			Buffer += 7;	// End of addr
			datalen -= 7;
		}

		if (Buffer[0] == 3)				// UI
		{
			Buffer += 2;
			datalen -= 2;

			Poll[2] = 0;				// UI Channel
			Poll[3] = 1;				// CMD
			Poll[4] = strlen(CCMD) - 1;
			strcpy(&Poll[5], CCMD);
			CRCStuffAndSend(TNC, Poll, Poll[4] + 6);	// Set Dest and Path

			TNC->Streams[0].CmdSet = TNC->Streams[0].CmdSave = zalloc(400);
			sprintf(TNC->Streams[0].CmdSet, "%cI%s\r%c%s\r%cI%s\r",
				2, ICall,			// Flag as Chan 0 Command
				1, Buffer,			// Flag CmdSet as Data
				2, TNC->NodeCall);	// Flag as Chan 0 Command
		}

		ReleaseBuffer((UINT *)buffptr);
		return;
	}


		// Check status Periodically
		
	if (TNC->TNCOK)
	{
		if (TNC->IntCmdDelay == 6)
		{
			Poll[2] = 254;			// Channel
			Poll[3] = 0x1;			// Command
			Poll[4] = 1;			// Len-1
			Poll[5] = 'G';			// Extended Status Poll
			Poll[6] = '3';

			CRCStuffAndSend(TNC, Poll, 7);
						
			TNC->InternalCmd = TRUE;
			TNC->IntCmdDelay--;

			return;
		}

		if (TNC->IntCmdDelay == 4)
		{
			Poll[2] = 31;			 // Channel
			Poll[3] = 0x1;			// Command
			Poll[4] = 1;			// Len-1
			Poll[5] = '%';			// Bytes acked Status
			Poll[6] = 'T';

			CRCStuffAndSend(TNC, Poll, 7);

			TNC->InternalCmd = TRUE;
			TNC->IntCmdDelay--;

			return;
		}

		if (TNC->IntCmdDelay <=0)
		{
			Poll[2] = 31;			// Channel
			Poll[3] = 0x1;			// Command
			Poll[4] = 1;			// Len-1
			Poll[5] = '@';			// Buffer Status
			Poll[6] = 'B';

			CRCStuffAndSend(TNC, Poll, 7);

			TNC->InternalCmd = TRUE;
			TNC->IntCmdDelay = 20;	// Every 2 secs

			return;
		}
		else
			TNC->IntCmdDelay--;
	}

	// If busy, send status poll, send Data if avail

	// We need to start where we last left off, or a busy stream will lock out the others

	for (nn = 0; nn <= MaxStreams; nn++)
	{
		Stream = TNC->LastStream++;

		if (TNC->LastStream > MaxStreams) TNC->LastStream = 0;

		if (TNC->TNCOK && TNC->Streams[Stream].BPQtoPACTOR_Q)
		{
			int datalen;
			UINT * buffptr;
			char * Buffer;

			// Dont send to Pactor if waiting for Min Level to be reached

			if (TNC->MinLevelTimer && Stream == 0)
				continue;
			
			buffptr=Q_REM(&TNC->Streams[Stream].BPQtoPACTOR_Q);

			datalen=buffptr[1];
			Buffer = (char *)&buffptr[2];	// Data portion of frame

			Poll[2] = TNC->Streams[Stream].DEDStream;		// Channel

			if (TNC->Streams[Stream].Connected)
			{
				if (TNC->SwallowSignon && Stream == 0)
				{
					TNC->SwallowSignon = FALSE;	
					if (strstr(Buffer, "Connected"))	// Discard *** connected
					{
						ReleaseBuffer(buffptr);
						return;
					}
				}

				Poll[3] = 0;			// Data?
				TNC->Streams[Stream].BytesTXed += datalen;

				Poll[4] = datalen - 1;
				memcpy(&Poll[5], buffptr+2, datalen);
		
				ReleaseBuffer(buffptr);
				OpenLogFile(TNC->Port);
				WriteLogLine(TNC->Port, &Poll[5], datalen);
				CloseLogFile(TNC->Port);
		
				CRCStuffAndSend(TNC, Poll, datalen + 5);

				TNC->Streams[Stream].InternalCmd = TNC->Streams[Stream].Connected;

				if (STREAM->Disconnecting && TNC->Streams[Stream].BPQtoPACTOR_Q == 0)
					TidyClose(TNC, 0);

				return;
			}
			
			// Command. Do some sanity checking and look for things to process locally

			Poll[3] = 1;			// Command
			datalen--;				// Exclude CR
			Buffer[datalen] = 0;	// Null Terminate
			_strupr(Buffer);

				if (_memicmp(Buffer, "D", 1) == 0)
				{
					TNC->Streams[Stream].ReportDISC = TRUE;		// Tell Node
					ReleaseBuffer(buffptr);
					return;
				}

				if (memcmp(Buffer, "RADIO ", 6) == 0)
				{
					sprintf(&Buffer[40], "%d %s", TNC->Port, &Buffer[6]);

					if (Rig_Command(TNC->PortRecord->ATTACHEDSESSIONS[0]->L4CROSSLINK->CIRCUITINDEX, &Buffer[40]))
					{
						ReleaseBuffer(buffptr);
					}
					else
					{
						buffptr[1] = sprintf((UCHAR *)&buffptr[2], "%s", &Buffer[40]);
						C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
					}
					return;
				}

				if (memcmp(Buffer, "MYLEVEL ", 8) == 0)
				{
					Switchmode(TNC, Buffer[8] - '0');

					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "Ok\r");		
					C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

					return;
				}

				if (_memicmp(Buffer, "OVERRIDEBUSY", 12) == 0)
				{
					TNC->OverrideBusy = TRUE;

					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "SCS} OK\r");
					C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

					return;
				}

				if ((Stream == 0) && memcmp(Buffer, "RPACKET", 7) == 0)
				{
					TNC->HFPacket = TRUE;
					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "SCS} OK\r");
					C_Q_ADD(&TNC->Streams[0].PACTORtoBPQ_Q, buffptr);
					return;
				}

				if ((Stream == 0) && memcmp(Buffer, "PACTOR", 6) == 0)
				{
					TNC->HFPacket = FALSE;
					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "SCS} OK\r");
					C_Q_ADD(&TNC->Streams[0].PACTORtoBPQ_Q, buffptr);
					return;
				}

				if (Stream == 0 && Buffer[0] == 'C' && datalen > 2)	    // Pactor Connect
					Poll[2] = TNC->Streams[0].DEDStream = 31;			// Pactor Channel

				if (Stream == 0 && Buffer[0] == 'R' && Buffer[1] == 'C')	// Robust Packet Connect
				{
					Poll[2] = TNC->Streams[0].DEDStream = 30;			// Last Packet Channel
					memmove(Buffer, &Buffer[1], datalen--);
				}

				if (Buffer[0] == 'C' && datalen > 2)	// Connect
				{
					if (*(++Buffer) == ' ') Buffer++;		// Space isn't needed

					if ((memcmp(Buffer, "P1 ", 3) == 0) ||(memcmp(Buffer, "P2 ", 3) == 0))
					{
						// Port Selector for Packet Connect convert to 2:CALL

						Buffer[0] = Buffer[1];		
						Buffer[1] = ':';
						memmove(&Buffer[2], &Buffer[3], datalen--);
						Buffer += 2;
					}

					memcpy(TNC->Streams[Stream].RemoteCall, Buffer, 9);

					TNC->Streams[Stream].Connecting = TRUE;

					if (Stream == 0)
					{
						// Send Call, Mode Command followed by connect 

						TNC->Streams[0].CmdSet = TNC->Streams[0].CmdSave = malloc(100);

						if (TNC->Dragon)
							sprintf(TNC->Streams[0].CmdSet, "I%s\r%s\r", TNC->Streams[0].MyCall, (char *)buffptr+8);
						else
						{
							if (TNC->Streams[0].DEDStream == 31)
								sprintf(TNC->Streams[0].CmdSet, "I%s\rPT\r%s\r", TNC->Streams[0].MyCall, (char *)buffptr+8);
							else
								sprintf(TNC->Streams[0].CmdSet, "I%s\rPR\r%s\r", TNC->Streams[0].MyCall, (char *)buffptr+8);
						}

						ReleaseBuffer(buffptr);
					
						// See if Busy
				
						if (InterlockedCheckBusy(TNC))
						{
							// Channel Busy. Unless override set, wait

							if (TNC->OverrideBusy == 0)
							{
								// Send Mode Command now, save command, and wait up to 10 secs
								// No, leave in Pactor, or Busy Detect won't work. Queue the whole conect sequence

								TNC->ConnectCmd = TNC->Streams[0].CmdSet;
								TNC->Streams[0].CmdSet = NULL;

								sprintf(TNC->WEB_TNCSTATE, "Waiting for clear channel");
								SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

								TNC->BusyDelay = TNC->BusyWait * 10;
								TNC->Streams[Stream].Connecting = FALSE;	// Not connecting Yet

								return;
							}
						}

						TNC->OverrideBusy = FALSE;

						sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s", TNC->Streams[Stream].MyCall, TNC->Streams[Stream].RemoteCall);
						SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

						Debugprintf("SCS Pactor CMDSet = %s", TNC->Streams[Stream].CmdSet);

						TNC->Streams[0].InternalCmd = FALSE;
						return;
					}
				}

			Poll[4] = datalen - 1;
			memcpy(&Poll[5], buffptr+2, datalen);
		
			ReleaseBuffer(buffptr);
		
			OpenLogFile(TNC->Port);
			WriteLogLine(TNC->Port, &Poll[5], datalen);
			CloseLogFile(TNC->Port);

			CRCStuffAndSend(TNC, Poll, datalen + 5);

			TNC->Streams[Stream].InternalCmd = TNC->Streams[Stream].Connected;

			return;
		}

		// if frames outstanding, issue a poll

		if (TNC->Streams[Stream].FramesOutstanding)
		{
			Poll[2] = TNC->Streams[Stream].DEDStream;
			Poll[3] = 0x1;			// Command
			Poll[4] = 0;			// Len-1
			Poll[5] = 'L';			// Status

			CRCStuffAndSend(TNC, Poll, 6);

			TNC->InternalCmd = TRUE;
			TNC->IntCmdDelay--;
			return;
		}

	}

	TNC->PollSent = TRUE;

	// Use General Poll (255)

	Poll[2] = 255 ;			// Channel
	Poll[3] = 0x1;			// Command

	if (TNC->ReinitState == 3)
	{
		TNC->ReinitState = 0;
		Poll[3] = 0x41;
	}

	Poll[4] = 0;			// Len-1
	Poll[5] = 'G';			// Poll

	CRCStuffAndSend(TNC, Poll, 6);
	TNC->InternalCmd = FALSE;

	return;

}

VOID DoTNCReinit(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 0)
	{
		// Just Starting - Send a TNC Mode Command to see if in Terminal or Host Mode
	
		TNC->TNCOK = FALSE;
		sprintf(TNC->WEB_COMMSSTATE,"%s Initialising TNC", TNC->PortRecord->PORTCONTROL.SerialPortName);
		SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

		Poll[0] = 13;
		Poll[1] = 0x1B;
		TNC->TXLen = 2;

		if (WriteCommBlock(TNC) == FALSE)
		{
			CloseCOMPort(TNC->hDevice);
			OpenCOMMPort(TNC, TNC->PortRecord->PORTCONTROL.SerialPortName, TNC->PortRecord->PORTCONTROL.BAUDRATE, TRUE);
		}

		TNC->Retries = 1;
	}

	if (TNC->ReinitState == 1)		// Forcing back to Term
		TNC->ReinitState = 0;

	if (TNC->ReinitState == 2)		// In Term State, Sending Initialisation Commands
	{
		char * start, * end;
		int len;

		start = TNC->InitPtr;
		
		if (*(start) == 0)			// End of Script
		{
			// Put into Host Mode

			Debugprintf("DOTNCReinit Complete - Entering Hostmode");

			TNC->TXBuffer[2] = 0;
			TNC->Toggle = 0;

			memcpy(Poll, "JHOST4\r", 7);

			TNC->TXLen = 7;
			WriteCommBlock(TNC);

			// Timeout will enter host mode

			TNC->Timeout = 1;
			TNC->Retries = 1;
			TNC->Toggle = 0;
			TNC->ReinitState = 3;	// Set toggle force bit
			TNC->OKToChangeFreq = 1;	// In case failed whilst waiting for permission

			return;
		}
		
		end = strchr(start, 13);
		len = ++end - start;
		TNC->InitPtr = end;
		memcpy(Poll, start, len);

		OpenLogFile(TNC->Port);
		WriteLogLine(TNC->Port, Poll, len);
		CloseLogFile(TNC->Port);

		TNC->TXLen = len;
		WriteCommBlock(TNC);

		TNC->Retries = 2;
	}
}

VOID DoTermModeTimeout(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 0)
	{
		//Checking if in Terminal Mode - Try to set back to Term Mode

		TNC->ReinitState = 1;
		ExitHost(TNC);
		TNC->Retries = 1;

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
		TNC->IntCmdDelay = 10;

		return;
	}
}

BOOL CheckRXText(struct TNCINFO * TNC)
{
	int Length;

	// only try to read number of bytes in queue 

	if (TNC->RXLen == 500)
		TNC->RXLen = 0;

	Length = ReadCOMBlock(TNC->hDevice, &TNC->RXBuffer[TNC->RXLen], 500 - TNC->RXLen);

	if (Length == 0)
		return FALSE;					// Nothing doing
	
	TNC->RXLen += Length;

	Length = TNC->RXLen;

	TNC->RXBuffer[TNC->RXLen] = 0;

	if (strlen(TNC->RXBuffer) < TNC->RXLen)
		TNC->RXLen = 0;

	if ((strstr(TNC->RXBuffer, "cmd: ") == 0) && (strstr(TNC->RXBuffer, "pac: ") == 0))
		return 0;				// Wait for rest of frame

	// Complete Char Mode Frame

	OpenLogFile(TNC->Port);
	WriteLogLine(TNC->Port, TNC->RXBuffer, TNC->RXLen);
	CloseLogFile(TNC->Port);

	TNC->RXBuffer[TNC->RXLen] = 0;
	
	if (RIG_DEBUG)
		Debugprintf(TNC->RXBuffer);

	TNC->RXLen = 0;		// Ready for next frame

	return TRUE;				
}

BOOL CheckRXHost(struct TNCINFO * TNC, char * UnstuffBuffer)
{
	int Length;
	unsigned short crc;

	// only try to read number of bytes in queue 

	if (TNC->RXLen == 500)
		TNC->RXLen = 0;

	Length = ReadCOMBlock(TNC->hDevice, &TNC->RXBuffer[TNC->RXLen], 500 - TNC->RXLen);

	if (Length == 0)
		return FALSE;					// Nothing doing
	
	TNC->RXLen += Length;

	Length = TNC->RXLen;

	if (Length < 6)				// Minimum Frame Sise
		return FALSE;

	if (TNC->RXBuffer[2] == 170)
	{
		// Retransmit Request
	
		TNC->RXLen = 0;
		return FALSE;				// Ignore for now
	}

	// Can't unstuff into same buffer - fails if partial msg received, and we unstuff twice

	Length = Unstuff(&TNC->RXBuffer[2], &UnstuffBuffer[2], Length - 2);

	if (Length == -1)
	{
		// Unstuff returned an errors (170 not followed by 0)

		TNC->RXLen = 0;
		return FALSE;				// Ignore for now
	}

	crc = compute_crc(&UnstuffBuffer[2], Length);

	if (crc == 0xf0b8)		// Good CRC
	{
		TNC->RXLen = 0;		// Ready for next frame
		return TRUE;
	}

	// Bad CRC - assume incomplete frame, and wait for rest. If it was a full bad frame, timeout and retry will recover link.

	return FALSE;
}


//#include "Mmsystem.h"

int Sleeptime = 250;

Switchmode(struct TNCINFO * TNC, int Mode)
{
	int n;
	char UnstuffBuffer[500];	

	// Send Exit/Enter Host Sequence

	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->HostMode == 0)
		return 0;							// Don't try if initialising

	TNC->EnterExit = TRUE;

	if (TNC->HFPacket)
	{
		Poll[2] = 31;
		Poll[3] = 0x1;
		Poll[4] = 0x1;
		memcpy(&Poll[5], "PT", 2);
		CRCStuffAndSend(TNC, Poll, 7);
		OpenLogFile(TNC->Port);
		WriteLogLine(TNC->Port, "SwitchModes - Setting Pactor", 28);
		CloseLogFile(TNC->Port);

		TNC->HFPacket = FALSE;
		TNC->Streams[0].DEDStream = 31;		// Pactor Channel

		n = 0;
		while (CheckRXHost(TNC, UnstuffBuffer) == FALSE)
		{
			Sleep(5);
			n++;
			if (n > 100) break;
		}

//		Debugprintf("Set Pactor ACK received in %d mS, Sleeping for %d", 5 * n, Sleeptime);
		Sleep(Sleeptime);
	}

	Poll[2] = 31;
	Poll[3] = 0x41;
	Poll[4] = 0x5;
	memcpy(&Poll[5], "JHOST0", 6);

	CRCStuffAndSend(TNC, Poll, 11);

	n = 0;

	while (CheckRXHost(TNC, UnstuffBuffer) == FALSE)
	{
		Sleep(5);
		n++;

		if (n > 100) break;
	}

//	Debugprintf("JHOST0 ACK received in %d mS", 5 * n);

	sprintf(Poll, "MYL %d\r", Mode);

//	Debugprintf("MYL %d", Mode);
	
	OpenLogFile(TNC->Port);
	WriteLogLine(TNC->Port, Poll, 5);
	CloseLogFile(TNC->Port);

	TNC->TXLen = 6;
	WriteCommBlock(TNC);

	n = 0;

	while (CheckRXText(TNC) == FALSE)
	{
		Sleep(5);
		n++;
		if (n > 100) break;
	}

//	Debugprintf("MYL ACK received in %d mS", 5 * n);

	memcpy(Poll, "JHOST4\r", 7);

	TNC->TXLen = 7;
	WriteCommBlock(TNC);

	// No response expected

	Sleep(10);

	Poll[2] = 255;			// Channel
	TNC->Toggle = 0;
	Poll[3] = 0x41;
	Poll[4] = 0;			// Len-1
	Poll[5] = 'G';			// Poll

	CRCStuffAndSend(TNC, Poll, 6);
	TNC->InternalCmd = FALSE;
	TNC->Timeout = 5;		// 1/2 sec - In case missed

	TNC->EnterExit = FALSE;
	return 0;
}

VOID SwitchToPactor(struct TNCINFO * TNC)
{
	if (TNC->ForceRobust)
		return;

	TNC->Streams[0].CmdSet = TNC->Streams[0].CmdSave = malloc(100);
	sprintf(TNC->Streams[0].CmdSet, "PT\r");

	TNC->HFPacket = FALSE;
	TNC->Streams[0].DEDStream = 31;		// Pactor Channel

	if (RIG_DEBUG)
		Debugprintf("BPQ32 Scan - switch to Pactor");
}

VOID SwitchToPacket(struct TNCINFO * TNC)
{
	TNC->Streams[0].CmdSet = TNC->Streams[0].CmdSave = malloc(100);
	sprintf(TNC->Streams[0].CmdSet, "PR\r");

	TNC->HFPacket = TRUE;
	TNC->Streams[0].DEDStream = 30;		// Packet Channel

	TNC->SwitchToPactor = TNC->RobustTime;

	if (RIG_DEBUG)
		Debugprintf("BPQ32 Scan - switch to Packet");
}

VOID ExitHost(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	// Try to exit Host Mode

	TNC->TXBuffer[2] = 31;
	TNC->TXBuffer[3] = 0x41;
	TNC->TXBuffer[4] = 0x5;
	memcpy(&TNC->TXBuffer[5], "JHOST0", 6);

	CRCStuffAndSend(TNC, Poll, 11);
	return;
}

VOID CRCStuffAndSend(struct TNCINFO * TNC, UCHAR * Msg, int Len)
{
	unsigned short int crc;
	UCHAR StuffedMsg[500];
	int i, j;

    Msg[3] |= TNC->Toggle;
	TNC->Toggle ^= 0x80;

	crc = compute_crc(&Msg[2], Len-2);
	crc ^= 0xffff;

	Msg[Len++] = (crc&0xff);
	Msg[Len++] = (crc>>8);

	for (i = j = 2; i < Len; i++)
	{
		StuffedMsg[j++] = Msg[i];
		if (Msg[i] == 170)
		{
			StuffedMsg[j++] = 0;
		}
	}

	if (j != i)
	{
		Len = j;
		memcpy(Msg, StuffedMsg, j);
	}

	TNC->TXLen = Len;

	Msg[0] = 170;
	Msg[1] = 170;

	WriteCommBlock(TNC);

	TNC->Retries = 5;
}

int Unstuff(UCHAR * MsgIn, UCHAR * MsgOut, int len)
{
	int i, j=0;

	for (i=0; i<len; i++, j++)
	{
		MsgOut[j] = MsgIn[i];
		if (MsgIn[i] == 170)			// Remove next byte
		{
			i++;
			if (MsgIn[i] != 0)
				if (i != len) return -1;
		}
	}

	return j;
}

VOID ProcessTermModeResponse(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 0)
	{
		// Testing if in Term Mode. It is, so can now send Init Commands

		TNC->InitPtr = TNC->InitScript;
		TNC->ReinitState = 2;

		// Send Restart to make sure PTC is in a known state

		strcpy(Poll, "RESTART\r");

		OpenLogFile(TNC->Port);
		WriteLogLine(TNC->Port, Poll, 7);
		CloseLogFile(TNC->Port);

		TNC->TXLen = 8;
		WriteCommBlock(TNC);

		TNC->Timeout = 60;				// 6 secs

		return;
	}
	if (TNC->ReinitState == 2)
	{
		// Sending Init Commands

		if (strstr(TNC->RXBuffer, "SCS P4dragon"))
		{
			TNC->Dragon = TRUE;
			Debugprintf("SCSPactor in P4dragon mode");
		}

		DoTNCReinit(TNC);		// Send Next Command
		return;
	}
}

VOID ProcessIncomingCall(struct TNCINFO * TNC, struct STREAMINFO * STREAM, int Stream)
{
	APPLCALLS * APPL;
	char * ApplPtr = APPLS;
	int App;
	char Appl[10];
	char FreqAppl[10] = "";				// Frequecy-specific application
	char DestCall[10];
	TRANSPORTENTRY * SESS;
	struct WL2KInfo * WL2K = TNC->WL2K;
	UCHAR * ptr;
	UCHAR Buffer[80];	
	UINT * buffptr;
	BOOL PactorCall = FALSE;
	
	char * Call = STREAM->RemoteCall;

	if (Stream > 0 && Stream < 30)
		ProcessIncommingConnectEx(TNC, Call, Stream, FALSE, TRUE);	// No CTEXT
	else
		ProcessIncommingConnectEx(TNC, Call, Stream, TRUE, TRUE);

	SESS = TNC->PortRecord->ATTACHEDSESSIONS[Stream];

	if (SESS == NULL)
		return;							// Cant do much without one

	if (Stream > 0 && Stream < 30)
	{
		// Packet Connect. Much safer to process here, even though it means
		// duplicating some code, or the Pactor/RP mode tests get very complicated

		strcpy(DestCall, STREAM->MyCall);
		Debugprintf("PTC Packet Incoming Call - MYCALL = *%s*", DestCall);
					
		for (App = 0; App < 32; App++)
		{
			APPL=&APPLCALLTABLE[App];
			memcpy(Appl, APPL->APPLCALL_TEXT, 10);

			ptr=strchr(Appl, ' ');
								
			if (ptr)
				*ptr = 0;

			if (_stricmp(DestCall, Appl) == 0)
				break;
		}

		if (App < 32)
		{
			char AppName[13];
			
			memcpy(AppName, &ApplPtr[App * sizeof(CMDX)], 12);
			AppName[12] = 0;

			// Make sure app is available

			Debugprintf("Connect is to APPL %s", AppName);

			if (CheckAppl(TNC, AppName))
			{
				int MsgLen = sprintf(Buffer, "%s\r", AppName);

				buffptr = GetBuff();

				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1] = MsgLen;

				memcpy(buffptr+2, Buffer, MsgLen);

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
				TNC->SwallowSignon = TRUE;
			}
			else
			{
				char Msg[] = "Application not available\r\n";
					
				// Send a Message, then a disconenct
					
				buffptr = GetBuff();

				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1] = strlen(Msg);
				memcpy(&buffptr[2], Msg, strlen(Msg));
				C_Q_ADD(&STREAM->BPQtoPACTOR_Q, buffptr);

				STREAM->NeedDisc = 100;	// 10 secs
			}							
			return;			
		}				
		
		// Not to a known appl - drop through to Node
		
		if (CTEXTLEN)
		{
			int Len = CTEXTLEN, CTPaclen = 100;
			int Next = 0;

			while (Len > CTPaclen)		// CTEXT Paclen
			{
				buffptr = GetBuff();
				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1] = CTPaclen;
				memcpy(&buffptr[2], &CTEXTMSG[Next], CTPaclen);
				C_Q_ADD(&STREAM->BPQtoPACTOR_Q, buffptr);

				Next += CTPaclen;
				Len -= CTPaclen;
			}

			buffptr = GetBuff();
			if (buffptr == 0) return;			// No buffers, so ignore

			buffptr[1] = Len;
			memcpy(&buffptr[2], &CTEXTMSG[Next], Len);
			C_Q_ADD(&STREAM->BPQtoPACTOR_Q, buffptr);
		}
		return;
	}

	//Connect on HF port. May be Pactor or RP on some models
	
	if (STREAM->DEDStream == 31)
		PactorCall = TRUE;

	if (TNC->RIG && TNC->RIG != &TNC->DummyRig)
	{
		sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound Freq %s", STREAM->RemoteCall, TNC->NodeCall, TNC->RIG->Valchar);
		SESS->Frequency = (atof(TNC->RIG->Valchar) * 1000000.0) + 1500;		// Convert to Centre Freq

		// If Scan Entry has a Appl, save it

		if (PactorCall && TNC->RIG->FreqPtr && TNC->RIG->FreqPtr[0]->APPL[0])
			strcpy(FreqAppl, &TNC->RIG->FreqPtr[0]->APPL[0]);
	}
	else
	{
		sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound", STREAM->RemoteCall, TNC->NodeCall);
		if (WL2K)
			SESS->Frequency = WL2K->Freq;
	}

	if (WL2K)
		strcpy(SESS->RMSCall, WL2K->RMSCall);						

	SESS->Mode = PleveltoMode[TNC->Streams[Stream].PTCStatus1];

	SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

	if (PactorCall && TNC->MinLevel > 1)
		TNC->MinLevelTimer = 150;		// Check we have reached right level
					
	// If an autoconnect APPL is defined, send it

	// See which application the connect is for
	
	strcpy(DestCall, STREAM->MyCall);

	if (PactorCall)
		Debugprintf("Pactor Incoming Call - MYCALL = *%s*", DestCall);					
	else					
		Debugprintf("HF Packet/RP Incoming Call - MYCALL = *%s*", DestCall);					

	if ( ((PactorCall && TNC->UseAPPLCallsforPactor) || (PactorCall == 0 && TNC->UseAPPLCalls))
		&& strcmp(DestCall, TNC->NodeCall) != 0)		// Not Connect to Node Call
	{		
		for (App = 0; App < 32; App++)
		{
			APPL=&APPLCALLTABLE[App];
			memcpy(Appl, APPL->APPLCALL_TEXT, 10);
			ptr=strchr(Appl, ' ');

			if (ptr)
				*ptr = 0;
	
			if (_stricmp(DestCall, Appl) == 0)
				break;
		}

		if (App < 32)
		{
			char AppName[13];

			memcpy(AppName, &ApplPtr[App * sizeof(CMDX)], 12);
			AppName[12] = 0;

			// Make sure app is available

			Debugprintf("Connect is to APPL %s", AppName);

			if (CheckAppl(TNC, AppName))
			{
				int MsgLen = sprintf(Buffer, "%s\r", AppName);
				buffptr = GetBuff();

				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1] = MsgLen;
				memcpy(buffptr+2, Buffer, MsgLen);

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
				TNC->SwallowSignon = TRUE;
			}
			else
			{
				char Msg[] = "Application not available\r\n";
					
				// Send a Message, then a disconenct
					
				buffptr = GetBuff();
				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1] = strlen(Msg);
				memcpy(&buffptr[2], Msg, strlen(Msg));
				C_Q_ADD(&STREAM->BPQtoPACTOR_Q, buffptr);

				STREAM->NeedDisc = 100;	// 10 secs
			}
			return;		
		}

		// Not to a known appl - drop through to Node
				
	}

	if (TNC->HFPacket && TNC->UseAPPLCalls)
		goto DontUseAPPLCmd;

	Debugprintf("Pactor Call is %s Freq Specific Appl is %s Freq is %s",
	DestCall, FreqAppl, TNC->RIG->Valchar);
						
	if (FreqAppl[0])			// Frequency spcific APPL overrides TNC APPL
	{
		buffptr = GetBuff();
		if (buffptr == 0) return;			// No buffers, so ignore

		Debugprintf("Using Freq Specific Appl %s", FreqAppl);

		buffptr[1] = sprintf((UCHAR *)&buffptr[2], "%s\r", FreqAppl);
		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
		TNC->SwallowSignon = TRUE;
		return;
	}
						
	if (TNC->ApplCmd)	
	{
		buffptr = GetBuff();
		if (buffptr == 0) return;			// No buffers, so ignore

		Debugprintf("Using Default Appl %s", TNC->ApplCmd);

		buffptr[1] = sprintf((UCHAR *)&buffptr[2], "%s\r", TNC->ApplCmd);
		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
		TNC->SwallowSignon = TRUE;
		return;
	}
		
DontUseAPPLCmd:
			
	if (FULL_CTEXT && CTEXTLEN && HFCTEXTLEN == 0)
	{
		int Len = CTEXTLEN, CTPaclen = 100;
		int Next = 0;

		while (Len > CTPaclen)		// CTEXT Paclen
		{
			buffptr = GetBuff();
			if (buffptr == 0) return;			// No buffers, so ignore

			buffptr[1] = CTPaclen;
			memcpy(&buffptr[2], &CTEXTMSG[Next], CTPaclen);
			C_Q_ADD(&STREAM->BPQtoPACTOR_Q, buffptr);

			Next += CTPaclen;
			Len -= CTPaclen;
		}

		buffptr = GetBuff();
		if (buffptr == 0) return;			// No buffers, so ignore

		buffptr[1] = Len;
		memcpy(&buffptr[2], &CTEXTMSG[Next], Len);
		C_Q_ADD(&STREAM->BPQtoPACTOR_Q, buffptr);
	}
}

VOID ProcessDEDFrame(struct TNCINFO * TNC, UCHAR * Msg, int framelen)
{
	UINT * buffptr;
	UCHAR * Buffer;				// Data portion of frame
	char Status[80];
	unsigned int Stream = 0, RealStream;

	if (TNC->HostMode == 0)
		return;

	// Any valid frame is an ACK

	TNC->Timeout = 0;

	if (TNC->TNCOK == FALSE)
	{
		// Just come up
		
		TNC->TNCOK = TRUE;
		sprintf(TNC->WEB_COMMSSTATE,"%s TNC link OK", TNC->PortRecord->PORTCONTROL.SerialPortName);
		SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);
	}

	Stream = RealStream = Msg[2];

	if (Stream > 29)
		Stream = 0;				// 31 = Pactor or 30 = Robust Packet Outgoing

	//	if in Dragon Single Mode (Pactor and Packet on Same Port)
	//	we only use stream 0, so if a packet frame, set DEDStream

	//	Im not convinced this is the bast place to do this, but let's try

	if (TNC->DragonSingle && RealStream && RealStream < 31)	// Not a Pactor or control frame
	{
		//	must be packet

		TNC->Streams[0].DEDStream = RealStream;		// Packet Channel
		Stream = 0;
	}

	//	See if Poll Reply or Data
	
	if (Msg[3] == 0)
	{
		// Success - Nothing Follows

		if (Stream < 32)
			if (TNC->Streams[Stream].CmdSet)
				return;						// Response to Command Set

		if ((TNC->TXBuffer[3] & 1) == 0)	// Data
			return;

		// If the response to a Command, then we should convert to a text "Ok" for forward scripts, etc

		if (TNC->TXBuffer[5] == 'G')	// Poll
			return;

		if (TNC->TXBuffer[5] == 'C')	// Connect - reply we need is async
			return;

		if (TNC->TXBuffer[5] == 'L')	// Shouldnt happen!
			return;

		if (TNC->TXBuffer[5] == '%' && TNC->TXBuffer[6] == 'W')	// Scan Control - Response to W1
			if (TNC->InternalCmd)
				return;					// Just Ignore

		if (TNC->TXBuffer[5] == 'J')	// JHOST
		{
			if (TNC->TXBuffer[10] == '0')	// JHOST0
			{
				TNC->Timeout = 1;			// 
				return;
			}
		}
		
		if (TNC->Streams[Stream].Connected)
			return;

		buffptr = GetBuff();

		if (buffptr == NULL) return;			// No buffers, so ignore

		buffptr[1] = sprintf((UCHAR *)&buffptr[2],"Pactor} Ok\r");

		OpenLogFile(TNC->Port);
		WriteLogLine(TNC->Port, (UCHAR *)&buffptr[2], buffptr[1]);
		CloseLogFile(TNC->Port);

		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
		return;
	}

	if (Msg[3] > 0 && Msg[3] < 6)
	{
		// Success with message - null terminated

		UCHAR * ptr;
		int len;

		if (Msg[2] == 0xff)			// General Poll Response
		{
			UCHAR * Poll = TNC->TXBuffer;
			UCHAR Chan = Msg[4] - 1;

			if (Chan == 255)			// Nothing doing
				return;	
		
			if (Msg[5] != 0)
			{
				// More than one to poll - save the list of channels to poll

				strcpy(TNC->NexttoPoll, &Msg[5]);
			}

			// Poll the channel that had data

			Poll[2] = Chan;			// Channel
			Poll[3] = 0x1;			// Command

			if (Chan == 254)		// Status - Send Extended Status (G3)
			{
				Poll[4] = 1;			// Len-1
				Poll[5] = 'G';			// Extended Status Poll
				Poll[6] = '3';
			}
			else
			{
				Poll[4] = 0;			// Len-1
				Poll[5] = 'G';			// Poll
			}

			CRCStuffAndSend(TNC, Poll, Poll[4] + 6);
			TNC->InternalCmd = FALSE;

			return;
		}

		Buffer = &Msg[4];
		
		ptr = strchr(Buffer, 0);

		if (ptr == 0)
			return;

		*(ptr++) = 13;
		*(ptr) = 0;

		len = ptr - Buffer;

		if (len > 256)
			return;

		// See if we need to process locally (Response to our command, Incoming Call, Disconencted, etc

		if (Msg[3] < 3)						// 1 or 2 - Success or Fail
		{
			char LastCmd = TNC->TXBuffer[5];
			struct STREAMINFO * STREAM = &TNC->Streams[Stream];
			
			// See if a response to internal command

			if (RIG_DEBUG)
				if (LastCmd == 'I')
					Debugprintf("SCS I Cmd Response %s", Buffer);

			if (LastCmd == 'I' && STREAM->CheckingCall == TRUE)
			{
				// We've received a connect and are checking MYCALL

				Debugprintf("SCS Incoming Call I Cmd Response %s Stream %d DED Stream %d", Buffer, Stream, RealStream);

				strlop(Buffer, 13);
				strcpy(STREAM->MyCall, Buffer);

				ProcessIncomingCall(TNC, STREAM, Stream);
				STREAM->CheckingCall = FALSE;
				return;
			}

			if (TNC->InternalCmd)
			{
				// Process it

				if (LastCmd == 'L')		// Status
				{
					int s1, s2, s3, s4, s5, s6, num;

					num = sscanf(Buffer, "%d %d %d %d %d %d", &s1, &s2, &s3, &s4, &s5, &s6);
			
					TNC->Streams[Stream].FramesOutstanding = s3;
					return;
				}

				if (LastCmd == '@')		// @ Commands
				{
					if (TNC->TXBuffer[6]== 'B')	// Buffer Status
					{
						TNC->Buffers = atoi(Buffer);
						SetWindowText(TNC->xIDC_BUFFERS, Buffer);
						strcpy(TNC->WEB_BUFFERS, Buffer);
						return;
					}
				}

				if (LastCmd == '%')		// % Commands
				{					
					if (TNC->TXBuffer[6]== 'T')	// TX count Status
					{
						sprintf(TNC->WEB_TRAFFIC, "RX %d TX %d ACKED %s", TNC->Streams[Stream].BytesRXed, TNC->Streams[Stream].BytesTXed, Buffer);
						SetWindowText(TNC->xIDC_TRAFFIC, TNC->WEB_TRAFFIC);
						return;
					}

					if (TNC->TXBuffer[6] == 'W')	// Scan Control
					{
						if (Msg[4] == '1')			// Ok to Change
						{
							TNC->OKToChangeFreq = 1;
							TNC->TimeScanLocked = 0;
						}
						else
						{
							TNC->OKToChangeFreq = -1;
							if (TNC->SyncSupported == FALSE && TNC->UseAPPLCallsforPactor && TNC->TimeScanLocked == 0)	
								TNC->TimeScanLocked = time(NULL);
						}
					}
				}
				return;
			}
		}

		if (Msg[3] == 3)					// Status
		{			
			struct STREAMINFO * STREAM = &TNC->Streams[Stream];

			if (strstr(Buffer, "DISCONNECTED") || strstr(Buffer, "LINK FAILURE"))
			{
				if ((STREAM->Connecting | STREAM->Connected) == 0)
					return;

				if (STREAM->Connecting && STREAM->Disconnecting == FALSE)
				{
					// Connect Failed
			
					buffptr = GetBuff();
					if (buffptr == 0) return;			// No buffers, so ignore

					buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Failure with %s\r", TNC->Streams[Stream].RemoteCall);

					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
	
					STREAM->Connecting = FALSE;
					STREAM->Connected = FALSE;				// In case!
					STREAM->FramesOutstanding = 0;

					if (Stream == 0)
					{
						sprintf(TNC->WEB_TNCSTATE, "In Use by %s", STREAM->MyCall);
						SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);
					}

					return;
				}
					
				// Must Have been connected or disconnecting - Release Session

				STREAM->Connecting = FALSE;
				STREAM->Connected = FALSE;		// Back to Command Mode
				STREAM->FramesOutstanding = 0;

				if (STREAM->Disconnecting == FALSE)
					STREAM->ReportDISC = TRUE;		// Tell Node

				STREAM->Disconnecting = FALSE;
				return;
			}

			if (strstr(Buffer, "CONNECTED"))
			{
				char * Call = strstr(Buffer, " to ");
				char * ptr;
				char MHCall[30];

				// Do we need to protect against 2nd call in Dragon Single Mode???

				Call += 4;

				if (Call[1] == ':')
					Call +=2;

				ptr = strchr(Call, ' ');	
				if (ptr) *ptr = 0;

				ptr = strchr(Call, 13);	
				if (ptr) *ptr = 0;

				STREAM->Connected = TRUE;			// Subsequent data to data channel
				STREAM->Connecting = FALSE;
				STREAM->ConnectTime = time(NULL); 
				STREAM->BytesRXed = STREAM->BytesTXed = 0;

				//	Stop Scanner

				if (Stream == 0 || TNC->HFPacket)
				{
					TNC->SwitchToPactor = 0;						// Cancel any RP to Pactor switch

					sprintf(Status, "%d SCANSTOP", TNC->Port);
					Rig_Command(-1, Status);

					SuspendOtherPorts(TNC);			// Prevent connects on other ports in same scan gruop

					memcpy(MHCall, Call, 9);
					MHCall[9] = 0;
				}

				if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] == 0)
				{
					// Incoming Connect

					// Check for Blacklist

					// As a test use premittedcalls

					if (TNC->PortRecord->PORTCONTROL.PERMITTEDCALLS)
					{
						UCHAR * ptr = TNC->PortRecord->PORTCONTROL.PERMITTEDCALLS;
						UCHAR AXCALL[7];
						
						ConvToAX25(MHCall, AXCALL);			//Permitted calls are stored in ax.25 format

						while (*ptr)
						{
							if (memcmp(AXCALL, ptr, 6) == 0)	// Ignore SSID
							{
								TidyClose(TNC, Stream);
								sprintf(Status, "%d SCANSTART 15", TNC->Port);
								Rig_Command(-1, Status);
								Debugprintf("SCS Call from %s rejected", MHCall);
								return;
							}
							ptr += 7;
						}
					}

		
					// Check that we think we are in the right mode

					if (Stream == 0 && TNC->Dragon == 0)	// Dragon runs both at the same time
					{
						if (TNC->HFPacket && RealStream == 31)
						{
							Debugprintf("Incoming Pactor Call while in Packet Mode");
							TNC->HFPacket = FALSE;
							STREAM->DEDStream = 31;
						}
						else
						if (TNC->HFPacket == 0 && RealStream == 30)
						{
							Debugprintf("Incoming Packet Call while in Pactor Mode");
							TNC->HFPacket = TRUE;
							STREAM->DEDStream = 30;
						}
					}

					if (TNC->HFPacket)
					{
						char Save = TNC->RIG->CurrentBandWidth;
						TNC->RIG->CurrentBandWidth = 'R';
						UpdateMH(TNC, MHCall, '+', 'I');
						TNC->RIG->CurrentBandWidth = Save;
					}

					memcpy(STREAM->RemoteCall, Call, 9);	// Save Text Callsign

					//	We need to check what MYCALL is set to, either in case
					//	Appl Scan has failed to change the callsign or if a
					//	Packet Call to MYALIAS

					STREAM->CmdSet = STREAM->CmdSave = malloc(100);
					sprintf(STREAM->CmdSet, "I\r");

					STREAM->CheckingCall = TRUE;
					return;
				}
				else
				{
					// Connect Complete
			
					buffptr = GetBuff();
					if (buffptr == 0) return;			// No buffers, so ignore

					buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Connected to %s\r", Call);;

					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

					if (Stream == 0)
					{
						if (TNC->RIG)
							sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound Freq %s", STREAM->MyCall, STREAM->RemoteCall, TNC->RIG->Valchar);
						else
							sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound", STREAM->MyCall, STREAM->RemoteCall);

						SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

						if (STREAM->DEDStream == 30)	// Robust Mode
						{
							char Save = TNC->RIG->CurrentBandWidth;
							TNC->RIG->CurrentBandWidth = 'R';
							UpdateMH(TNC, Call, '+', 'O');
							TNC->RIG->CurrentBandWidth = Save;
						}
						else
						{
							UpdateMH(TNC, Call, '+', 'O');
						}
					}
					return;
				}
			}
			return;
		}

		if (Msg[3] == 4 || Msg[3] == 5)
		{
			struct STREAMINFO * STREAM = &TNC->Streams[1];		// RP Stream

			// Monitor

			if ((TNC->HFPacket || TNC->DragonSingle) && TNC->UseAPPLCalls && strstr(&Msg[4], "SABM") && STREAM->Connected == FALSE)
			{
				// See if a call to Nodecall or one of our APPLCALLS - if so, stop scan and switch MYCALL

				char DestCall[10] = "NOCALL  ";
				char * ptr1 = strstr(&Msg[7], "to ");
				int i;
				APPLCALLS * APPL;
				char Appl[11];
				char Status[80];

				if (ptr1) memcpy(DestCall, &ptr1[3], 10);
				
				ptr1 = strchr(DestCall, ' ');
				if (ptr1) *(ptr1) = 0;					// Null Terminate

				Debugprintf("RP SABM Received for %s" , DestCall);

				if (strcmp(TNC->NodeCall, DestCall) != 0)
				{
					// Not Calling NodeCall/Portcall

					if (strcmp(NodeCall, DestCall) == 0)
						goto SetThisCall;

					// See if to one of our ApplCalls

					for (i = 0; i < 32; i++)
					{
						APPL=&APPLCALLTABLE[i];

						if (APPL->APPLCALL_TEXT[0] > ' ')
						{
							char * ptr;
							memcpy(Appl, APPL->APPLCALL_TEXT, 10);
							ptr=strchr(Appl, ' ');

							if (ptr) *ptr = 0;

							if (strcmp(Appl, DestCall) == 0)
							{
						SetThisCall:
								Debugprintf("RP SABM is for NODECALL or one of our APPLCalls - setting MYCALL to %s and pausing scan", DestCall);

								sprintf(Status, "%d SCANSTART 30", TNC->Port);
								Rig_Command(-1, Status);
								TNC->SwitchToPactor = 0;		// Stay in RP
				
								strcpy(STREAM->MyCall, DestCall);
								STREAM->CmdSet = STREAM->CmdSave = malloc(100);
								sprintf(STREAM->CmdSet, "I%s\r", DestCall);
								TNC->InternalCmd = TRUE;
								
								break;
							}
						}
					}
				}
			}

			DoMonitor(TNC, &Msg[3], framelen - 3);
			return;

		}

		// 1, 2, 4, 5 - pass to Appl

		buffptr = GetBuff();

		if (buffptr == NULL) return;			// No buffers, so ignore

		buffptr[1] = sprintf((UCHAR *)&buffptr[2],"Pactor} %s", &Msg[4]);

		OpenLogFile(TNC->Port);
		WriteLogLine(TNC->Port, &Msg[4], strlen(&Msg[4]));
		CloseLogFile(TNC->Port);

		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

		return;
	}

	if (Msg[3] == 6)
	{
		// Monitor Data With length)

		DoMonitor(TNC, &Msg[3], framelen - 3);
		return;
	}

	if (Msg[3] == 7)
	{
		char StatusMsg[60];
		int Status, ISS, Offset;
		
		if (Msg[2] == 0xfe)						// Status Poll Response
		{
			int PactorLevel = Msg[6] & 7;		// Pactor Level 1-4
			
			if (TNC->MinLevelTimer)
			{
				if (PactorLevel >= TNC->MinLevel)
				{
					Debugprintf("Reached MIN Pactor Level");
					TNC->MinLevelTimer = 0;
				}
				else
					Debugprintf("Still waiting for Min Level Now %d Need %d", PactorLevel, TNC->MinLevel);
			}

			Status = Msg[5];
			
			TNC->Streams[0].PTCStatus0 = Status;
			TNC->Streams[0].PTCStatus1 = PactorLevel;		// Pactor Level 1-4
			TNC->Streams[0].PTCStatus2 = Msg[7];			// Speed Level
			Offset = Msg[8];

			if (Offset > 128)
				Offset -= 128;

			TNC->Streams[0].PTCStatus3 = Offset; 

			TNC->Mode = (Status >> 4) & 7;
			ISS = Status & 8;
			Status &= 7;

			if (TNC->PTCStatus != Status)		// Changed
			{
				Debugprintf("SCS status changed, now %s", status[Status]);

				if (Status == 6)		// SYNCH
				{
					// New Sync

					if (RIG_DEBUG)
						Debugprintf("SCS New SYNC Detected");
	
					TNC->TimeEnteredSYNCMode = time(NULL);
					TNC->SyncSupported = TRUE;
				}
				else
				{
					if (TNC->PTCStatus == 6)
					{
						if (RIG_DEBUG)
							Debugprintf("SCS left SYNC, now %s", status[Status]);
	
						TNC->TimeEnteredSYNCMode = 0;
					}
				}
				TNC->PTCStatus = Status;
			}
			sprintf(StatusMsg, "%x %x %x %x", TNC->Streams[0].PTCStatus0,
				TNC->Streams[0].PTCStatus1, TNC->Streams[0].PTCStatus2, TNC->Streams[0].PTCStatus3);
			
			if (ISS)
			{
				SetWindowText(TNC->xIDC_TXRX, "Sender");
				strcpy(TNC->WEB_TXRX, "Sender"); 
			}
			else
			{
				SetWindowText(TNC->xIDC_TXRX, "Receiver");
				strcpy(TNC->WEB_TXRX, "Receiver"); 
			}

			SetWindowText(TNC->xIDC_STATE, status[Status]);
			strcpy(TNC->WEB_STATE, status[Status]);
			SetWindowText(TNC->xIDC_MODE, ModeText[TNC->Mode]);
			strcpy(TNC->WEB_MODE, ModeText[TNC->Mode]);

			if (TNC->Mode == 7)
				TNC->Busy = TNC->BusyHold * 10;				// BusyHold  delay

			if (Offset == 128)		// Undefined
				sprintf(StatusMsg, "Mode %s Speed Level %d Freq Offset Unknown",
					PactorLevelText[TNC->Streams[0].PTCStatus1], Msg[7]);
			else
				sprintf(StatusMsg, "Mode %s Speed Level %d Freq Offset %d",
					PactorLevelText[TNC->Streams[0].PTCStatus1], Msg[7], Offset);

			strcpy(TNC->WEB_PACTORLEVEL, StatusMsg);
			SetWindowText(TNC->xIDC_PACTORLEVEL, StatusMsg);

			return;
		}

		if (Msg[2] == 253)						// Rig Port Response
		{
			// Queue for Rig Control Driver
			
			int datalen = Msg[4] + 1;
			UINT * buffptr;
			
			buffptr = GetBuff();

			if (buffptr)
			{
				buffptr[1] = datalen;
				memcpy(buffptr + 2, &Msg[5], datalen);
				C_Q_ADD(&TNC->RadiotoBPQ_Q, buffptr);
				if (RIG_DEBUG)
				{
					Debugprintf("SCS RIG frame received, len %d", datalen);
					Debugprintf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
						Msg[5], Msg[6], Msg[7], Msg[8], Msg[9], Msg[10], Msg[11], Msg[12],
						Msg[13], Msg[14], Msg[15], Msg[16], Msg[17], Msg[18], Msg[19], Msg[20]);
		
				}
			}
			return;
		}

		// Connected Data
		
		buffptr = GetBuff();

		if (buffptr == NULL) return;			// No buffers, so ignore
			
		buffptr[1] = Msg[4] + 1;				// Length
		TNC->Streams[Stream].BytesRXed += buffptr[1];
		memcpy(&buffptr[2], &Msg[5], buffptr[1]);
		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

		return;
	}
}

int GetPTCRadioCommand(struct TNCINFO * TNC, char * Block)
{
	UINT * buffptr;
	int Length;

	if (TNC->RadiotoBPQ_Q == 0)
		return 0;

	buffptr = Q_REM(&TNC->RadiotoBPQ_Q);

	Length = buffptr[1];
		
	memcpy(Block, buffptr + 2, Length);
		
	ReleaseBuffer(buffptr);

//	Debugprintf("SCS Rig Command Queued");

	return Length;;
}

int SendPTCRadioCommand(struct TNCINFO * TNC, char * Block, int Length)
{
	UINT * buffptr;

	if (!TNC->TNCOK)
		return 0;

	// Queue for TNC

	buffptr = GetBuff();

	if (buffptr == 0) return (0);			// No buffers, so ignore

	buffptr[1] = Length;
		
	memcpy(buffptr+2, Block, Length);
		
	C_Q_ADD(&TNC->BPQtoRadio_Q, buffptr);

   return 0;

}
#pragma pack(1) 

typedef struct _MESSAGEY
{
//	BASIC LINK LEVEL MESSAGE BUFFER LAYOUT

	struct _MESSAGEY * CHAIN;

	UCHAR	PORT;
	USHORT	LENGTH;

	UCHAR	DEST[7];
	UCHAR	ORIGIN[7];

//	 MAY BE UP TO 56 BYTES OF DIGIS

	UCHAR	CTL;
	UCHAR	PID; 

	union 
	{                   /*  array named screen */
		UCHAR L2DATA[256];
		struct _L3MESSAGE L3MSG;

	};

	UCHAR Padding[BUFFLEN - sizeof(time_t) - sizeof(VOID *) - 256 - 7 - 16];

	time_t Timestamp;
	VOID * Linkptr;		// For ACKMODE processing

}MESSAGEY;

#pragma pack() 

static MESSAGEY Monframe;		// I frames come in two parts.

#define TIMESTAMP 352

MESSAGEY * AdjMsg = &Monframe;				// Adjusted for digis

static VOID DoMonitor(struct TNCINFO * TNC, UCHAR * Msg, int Len)
{
	// Convert to ax.25 form and pass to monitor

	UCHAR * ptr, * starptr;
	char * context;

	if (Msg[0] == 6)		// Second part of I or UI
	{
		int len = Msg[1] +1;

		memcpy(AdjMsg->L2DATA, &Msg[2], len);
		Monframe.LENGTH += len;

		time(&Monframe.Timestamp);

		BPQTRACE((MESSAGE *)&Monframe, TRUE);
		return;
	}

	Monframe.LENGTH = 23;				// Control Frame
	Monframe.PORT = TNC->Port;
	
	AdjMsg = &Monframe;					// Adjusted for digis
	ptr = strstr(Msg, "fm ");

	ConvToAX25(&ptr[3], Monframe.ORIGIN);

	UpdateMH(TNC, &ptr[3], '.', 0);

	ptr = strstr(ptr, "to ");

	ConvToAX25(&ptr[3], Monframe.DEST);


	ptr = strstr(ptr, "via ");

	if (ptr)
	{
		// We have digis

		char Save[100];
		char * fiddle;

		memcpy(Save, &ptr[4], 60);

		ptr = strtok_s(Save, " ", &context);
DigiLoop:
		fiddle = (char *)AdjMsg;
		fiddle += 7;
		AdjMsg = (MESSAGEY *)fiddle;

		Monframe.LENGTH += 7;

		starptr = strchr(ptr, '*');
		if (starptr)
			*(starptr) = 0;

		ConvToAX25(ptr, AdjMsg->ORIGIN);

		if (starptr)
			AdjMsg->ORIGIN[6] |= 0x80;				// Set end of address

		ptr = strtok_s(NULL, " ", &context);

		if (memcmp(ptr, "ctl", 3))
			goto DigiLoop;
	}

	AdjMsg->ORIGIN[6] |= 1;				// Set end of address

	ptr = strstr(Msg, "ctl ");

	if (memcmp(&ptr[4], "SABM", 4) == 0)
		AdjMsg->CTL = 0x2f;
	else  
	if (memcmp(&ptr[4], "DISC", 4) == 0)
		AdjMsg->CTL = 0x43;
	else 
	if (memcmp(&ptr[4], "UA", 2) == 0)
		AdjMsg->CTL = 0x63;
	else  
	if (memcmp(&ptr[4], "DM", 2) == 0)
		AdjMsg->CTL = 0x0f;
	else 
	if (memcmp(&ptr[4], "UI", 2) == 0)
		AdjMsg->CTL = 0x03;
	else 
	if (memcmp(&ptr[4], "RR", 2) == 0)
		AdjMsg->CTL = 0x1 | (ptr[6] << 5);
	else 
	if (memcmp(&ptr[4], "RNR", 3) == 0)
		AdjMsg->CTL = 0x5 | (ptr[7] << 5);
	else 
	if (memcmp(&ptr[4], "REJ", 3) == 0)
		AdjMsg->CTL = 0x9 | (ptr[7] << 5);
	else 
	if (memcmp(&ptr[4], "FRMR", 4) == 0)
		AdjMsg->CTL = 0x87;
	else  
	if (ptr[4] == 'I')
	{
		AdjMsg->CTL = (ptr[5] << 5) | (ptr[6] & 7) << 1 ;
	}

	if (strchr(&ptr[4], '+'))
	{
		AdjMsg->CTL |= 0x10;
		Monframe.DEST[6] |= 0x80;				// SET COMMAND
	}

	if (strchr(&ptr[4], '-'))	
	{
		AdjMsg->CTL |= 0x10;
		Monframe.ORIGIN[6] |= 0x80;				// SET COMMAND
	}

	if (Msg[0] == 5)							// More to come
	{
		ptr = strstr(ptr, "pid ");	
		sscanf(&ptr[3], "%x", (int *)&AdjMsg->PID);
		return;	
	}

	time(&Monframe.Timestamp);

	BPQTRACE((MESSAGE *)&Monframe, TRUE);

}
//1:fm G8BPQ to KD6PGI-1 ctl I11^ pid F0
//fm KD6PGI-1 to G8BPQ ctl DISC+

VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
	// Queue it as we may have just sent data

	TNC->Streams[Stream].CmdSet = TNC->Streams[Stream].CmdSave = malloc(100);
	sprintf(TNC->Streams[Stream].CmdSet, "D\r");
}


VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	// Sending D twice should do a "Dirty Disconnect"

	// Try thst first. If it still doesn't disconnect maybe try restart

	unsigned char Resp[500] = "";
	char * Poll = &TNC->TXBuffer[0]; 
	int n;

	Debugprintf("Failed to disconnect TNC - trying a forced disconnect");

	Poll[2] = 31;
	Poll[3] = 1;
	Poll[4] = 0;
	Poll[5] = 'D';

	CRCStuffAndSend(TNC, Poll, 6);

	// Wait for response before sending another

	n = 0;
	while (CheckRXHost(TNC, Resp) == FALSE)
	{
		Sleep(5);
		n++;
		if (n > 100) break;
	}

	Poll[2] = 31;
	Poll[3] = 1;
	Poll[4] = 0;
	Poll[5] = 'D';

	CRCStuffAndSend(TNC, Poll, 6);

	n = 0;
	while (CheckRXHost(TNC, Resp) == FALSE)
	{
		Sleep(5);
		n++;
		if (n > 100) break;
	}

	// See if it worked

	Poll[2] = 254;			// Channel
	Poll[3] = 0x1;			// Command
	Poll[4] = 1;			// Len-1
	Poll[5] = 'G';			// Extended Status Poll
	Poll[6] = '3';

	CRCStuffAndSend(TNC, Poll, 7);
	
	n = 0;
	while (CheckRXHost(TNC, Resp) == FALSE)
	{
		Sleep(5);
		n++;
		if (n > 100) break;
	}

	Debugprintf("PTC Status Now %x %x %x %x %x %x %x %x",
		Resp[0], Resp[1], Resp[2], Resp[3], Resp[4], Resp[5], Resp[6], Resp[7]); 

	TNC->Timeout = 0;

	return;

	// Maybe best just to restart the TNC

	if (TNC->PacketChannels == 0)		// Not using packet
	{
		Debugprintf("Forced Disconnect Failed - restarting TNC");

		// Ensure in Pactor

		if(TNC->Dragon == 0)
		{
			TNC->TXBuffer[2] = 31;
			TNC->TXBuffer[3] = 0x1;
			TNC->TXBuffer[4] = 0x1;
			memcpy(&TNC->TXBuffer[5], "PT", 2);

			CRCStuffAndSend(TNC, TNC->TXBuffer, 7);

			n = 0;
			while (CheckRXHost(TNC, Resp) == FALSE)
			{
				Sleep(5);
				n++;
				if (n > 100) break;
			}
		}

		Sleep(50);
		ExitHost(TNC);
		Sleep(50);

		n = 0;
		while (CheckRXHost(TNC, Resp) == FALSE)
		{
			Sleep(5);
			n++;
			if (n > 100) break;
		}

		TNC->Timeout = 0;
		TNC->HostMode = FALSE;
		TNC->ReinitState = 0;

		return;
	}
}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
	char Status[80];
	struct STREAMINFO * STREAM = &TNC->Streams[Stream];

	Debugprintf("SCS Pactor Close Complete - Stream = %d", Stream);

	STREAM->CmdSet = STREAM->CmdSave = malloc(100);

	strcpy(STREAM->MyCall, TNC->NodeCall);

	if (Stream == 0 || TNC->HFPacket)
	{
		SetWindowText(TNC->xIDC_TNCSTATE, "Free");
		strcpy(TNC->WEB_TNCSTATE, "Free");
		sprintf(Status, "%d SCANSTART 15", TNC->Port);
		Rig_Command(-1, Status);

		if (TNC->Dragon)
		{
			sprintf(STREAM->CmdSet, "I%s\r", TNC->NodeCall);
			TNC->Streams[0].DEDStream = 31;		// Pactor Channel
		}
		else
		{
			if (TNC->HFPacket)
			{
				sprintf(STREAM->CmdSet, "I%s\rPR\r", TNC->NodeCall);
				TNC->Streams[0].DEDStream = 30;		// Packet Channel
				Debugprintf("BPQ32 Session Closed - switch to Packet");
			}
			else
			{
				sprintf(STREAM->CmdSet, "I%s\rPT\r", TNC->NodeCall);
				TNC->Streams[0].DEDStream = 31;		// Pactor Channel
				Debugprintf("BPQ32 Session Closed - switch to Pactor");
			}
		}
		ReleaseOtherPorts(TNC);
	}
	else
		sprintf(STREAM->CmdSet, "I%s\r", TNC->NodeCall);

	Debugprintf("SCS Pactor CMDSet = %s", STREAM->CmdSet);

}

VOID PTCSuspendPort(struct TNCINFO * TNC)
{
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	STREAM->CmdSet = STREAM->CmdSave = zalloc(100);
	sprintf(STREAM->CmdSet, "I%s\r", "SCSPTC");		// Should prevent connects

	Debugprintf("SCS Pactor CMDSet = %s", STREAM->CmdSet);
}

VOID PTCReleasePort(struct TNCINFO * TNC)
{
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	STREAM->CmdSet = STREAM->CmdSave = zalloc(100);

	if (TNC->UseAPPLCallsforPactor && TNC->RIG && TNC->RIG != &TNC->DummyRig 
			&& TNC->RIG->FreqPtr[0]->APPLCALL[0])
		sprintf(STREAM->CmdSet, "I%s\r", TNC->RIG->FreqPtr[0]->APPLCALL);
	else
		sprintf(STREAM->CmdSet, "I%s\r", TNC->NodeCall);

	Debugprintf("SCS Pactor CMDSet = %s", STREAM->CmdSet);
}



