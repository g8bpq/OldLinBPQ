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
//	DLL to inteface DED Host Mode TNCs to BPQ32 switch 
//
//	Uses BPQ EXTERNAL interface

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <stdio.h>
#include <stdlib.h>
#include "time.h"

#define MaxStreams 10	

#include "CHeaders.h"
#include "tncinfo.h"

#include "bpq32.h"

static char ClassName[]="TRACKERSTATUS";
static char WindowTitle[] = "SCS Tracker";
static int RigControlRow = 140;

#define NARROWMODE 30
#define WIDEMODE 30			// PIII only

extern UCHAR BPQDirectory[];

extern char * PortConfig[33];

static RECT Rect;

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

VOID __cdecl Debugprintf(const char * format, ...);
char * strlop(char * buf, char delim);

char NodeCall[11];		// Nodecall, Null Terminated

unsigned long _beginthread( void( *start_address )(), unsigned stack_size, int arglist);

static ProcessLine(char * buf, int Port)
{
	UCHAR * ptr;
	char * p_port = 0;
	int BPQport;
	int len=510;
	struct TNCINFO * TNC;
	char errbuf[256];

	strcpy(errbuf, buf);

	BPQport = Port;

	TNC = TNCInfo[BPQport] = malloc(sizeof(struct TNCINFO));
	memset(TNC, 0, sizeof(struct TNCINFO));

	TNC->InitScript = malloc(1000);
	TNC->InitScript[0] = 0;

	TNC->PacketChannels = 10; // Default

	goto ConfigLine;

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
		}
		else
		if (_memicmp(buf, "RIGCONTROL", 10) == 0)
		{
		}
		else
		
		if (_memicmp(buf, "SWITCHMODES", 11) == 0)
		{
		}
		else
		if (_memicmp(buf, "USEAPPLCALLS", 12) == 0)
		{
//			TNC->UseAPPLCalls = TRUE;
		}
		else
		if (_memicmp(buf, "DEFAULT ROBUST", 14) == 0)
		{
		}
		else

		if (_memicmp(buf, "WL2KREPORT", 10) == 0)
		{
		}
		else
		if (_memicmp(buf, "UPDATEMAP", 9) == 0)
			TNC->PktUpdateMap = TRUE;
		else
		if (_memicmp(buf, "PACKETCHANNELS", 14) == 0)

			// Packet Channels

			TNC->PacketChannels = atoi(&buf[14]);
		else
			strcat (TNC->InitScript, buf);
	}
	return (TRUE);

}

struct TNCINFO * CreateTTYInfo(int port, int speed);
BOOL OpenConnection(int);
BOOL SetupConnection(int);
BOOL CloseConnection(struct TNCINFO * conn);
static BOOL WriteCommBlock(struct TNCINFO * TNC);
BOOL DestroyTTYInfo(int port);
static void DEDCheckRX(struct TNCINFO * TNC);
static VOID DEDPoll(int Port);
VOID StuffAndSend(struct TNCINFO * TNC, UCHAR * Msg, int Len);
unsigned short int compute_crc(unsigned char *buf,int len);
int Unstuff(UCHAR * MsgIn, UCHAR * MsgOut, int len);
static VOID ProcessDEDFrame(struct TNCINFO * TNC);
static VOID ProcessTermModeResponse(struct TNCINFO * TNC);
static VOID ExitHost(struct TNCINFO * TNC);
static VOID DoTNCReinit(struct TNCINFO * TNC);
static VOID DoTermModeTimeout(struct TNCINFO * TNC);
VOID DoMonitorHddr(struct TNCINFO * TNC, UCHAR * Msg, int Len, int Type);
VOID DoMonitorData(struct TNCINFO * TNC, UCHAR * Msg, int Len);
int Switchmode(struct TNCINFO * TNC, int Mode);
VOID SwitchToRPacket(struct TNCINFO * TNC);
VOID SwitchToNormPacket(struct TNCINFO * TNC);


static int ExtProc(int fn, int port,unsigned char * buff)
{
	int txlen = 0;
	UINT * buffptr;
	struct TNCINFO * TNC = TNCInfo[port];
	int Stream = 0;
	struct STREAMINFO * STREAM;
	int TNCOK;

	if (TNC == NULL)
		return 0;
	
	if (TNC->hDevice == 0)
	{
		// Try to reopen every 30 secs

		TNC->ReopenTimer++;

		if (TNC->ReopenTimer < 300)
			return 0;

		TNC->ReopenTimer = 0;
		
		OpenCOMMPort(TNC, TNC->PortRecord->PORTCONTROL.SerialPortName, TNC->PortRecord->PORTCONTROL.BAUDRATE, TRUE);

		if (TNC->hDevice == 0)
			return 0;
	}
	switch (fn)
	{
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

		DEDCheckRX(TNC);
		DEDPoll(port);
		DEDCheckRX(TNC);

		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			STREAM = &TNC->Streams[Stream];
			
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

				PutLengthinBuffer(buff, datalen);		// Neded for arm5 portability

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

		TNCOK = (TNC->HostMode == 1 && TNC->ReinitState != 10);

		STREAM = &TNC->Streams[Stream];

		if (Stream == 0)
		{
			if (STREAM->FramesOutstanding  > 4)
				return (1 | TNCOK << 8 | STREAM->Disconnecting << 15);
		}
		else
		{
			if (STREAM->FramesOutstanding > 3 || TNC->Buffers < 200)	
				return (1 | TNCOK << 8 | STREAM->Disconnecting << 15);		}

		return TNCOK << 8 | STREAM->Disconnecting << 15;		// OK, but lock attach if disconnecting


	case 4:				// reinit

		return (0);

	case 5:				// Close

		// Ensure in Pactor

		ExitHost(TNC);

		Sleep(25);

		CloseCOMPort(TNCInfo[port]->hDevice);
		return (0);

	case 6:

		return 0;				// No scan interface
}
	return 0;
}

UINT TrackerMExtInit(EXTPORTDATA *  PortEntry)
{
	char msg[500];
	struct TNCINFO * TNC;
	int port;
	char * ptr;
	int Stream = 0;
	char * TempScript;
	char YCmd[10];

	//
	//	Will be called once for each DED Host TNC Port
	//	The COM port number is in IOBASE
	//

	sprintf(msg,"SCSTRK M %s", PortEntry->PORTCONTROL.SerialPortName);

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
	TNC->Hardware = H_TRKM;

	// Set up DED addresses for streams
	
	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		TNC->Streams[Stream].DEDStream = Stream;	// DED Stream = BPQ Stream (We don't use Stream 0)
	}

	if (TNC->PacketChannels > MaxStreams)
		TNC->PacketChannels = MaxStreams;

	PortEntry->MAXHOSTMODESESSIONS = TNC->PacketChannels + 1; //TNC->PacketChannels + 1;
	PortEntry->PERMITGATEWAY = TRUE;					// Can change ax.25 call on each stream
	PortEntry->SCANCAPABILITIES = NONE;				// Scan Control 3 stage/conlock 

	TNC->PortRecord = PortEntry;

	if (PortEntry->PORTCONTROL.PORTCALL[0] == 0)
		memcpy(TNC->NodeCall, MYNODECALL, 10);
	else
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);
		
	PortEntry->PORTCONTROL.PROTOCOL = 10;
	PortEntry->PORTCONTROL.PORTQUALITY = 0;
	PortEntry->PORTCONTROL.UICAPABLE = 1;

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0)
		PortEntry->PORTCONTROL.PORTPACLEN = 100;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	// get NODECALL for RP tests

	memcpy(NodeCall, MYNODECALL, 10);
		
	ptr=strchr(NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	TempScript = malloc(1000);

	strcpy(TempScript, "M UISC\r");
	strcat(TempScript, "F 200\r");			// Sets SABM retry time to about 5 secs
	strcat(TempScript, "%F 1500\r");		// Tones may be changed but I want this as standard

	strcat(TempScript, TNC->InitScript);

	free(TNC->InitScript);
	TNC->InitScript = TempScript;

	// Others go on end so they can't be overriden

	strcat(TNC->InitScript, "Z 0\r");      //  	No Flow Control
	sprintf(YCmd, "Y %d\r", TNC->PacketChannels);
	strcat(TNC->InitScript, YCmd);
	strcat(TNC->InitScript, "E 1\r");      //  	Echo - Restart process needs echo
	
	sprintf(msg, "I %s\r", TNC->NodeCall);
	strcat(TNC->InitScript, msg);

	OpenCOMMPort(TNC,PortEntry->PORTCONTROL.SerialPortName, PortEntry->PORTCONTROL.BAUDRATE, FALSE);

	TNC->InitPtr = TNC->InitScript;

	WritetoConsole("\n");

	return ((int)ExtProc);
}

static void DEDCheckRX(struct TNCINFO * TNC)
{
	int Length, Len;
	UCHAR  * ptr;
	UCHAR character;
	UCHAR * CURSOR;

	Len = ReadCOMBlock(TNC->hDevice, &TNC->RXBuffer[TNC->RXLen], 500 - TNC->RXLen);

	if (Len == 0)
		return;					// Nothing doing
	
	TNC->RXLen += Len;

	Length = TNC->RXLen;
	
	ptr = TNC->RXBuffer;

	CURSOR = &TNC->DEDBuffer[TNC->InputLen];

	if (TNC->HostMode == 0)
	{
		// If we are just restarting, and TNC is in host mode, we may get "Invalid Channel" Back
		
		if (memcmp(ptr, "\x18\x02INVALID", 9) == 0)
		{
			TNC->HostMode = TRUE;
			TNC->HOSTSTATE = 0;
			TNC->Timeout = 0;
			TNC->RXLen = 0;
			return;
		}

		// Command is echoed as * command * 

		if (strstr(ptr, "*") || TNC->ReinitState == 5)		// 5 is waiting for reponse to JHOST1
		{
			ProcessTermModeResponse(TNC);
			TNC->RXLen = 0;
			TNC->HOSTSTATE = 0;

			return;
		}
	}

	while (Length--)
	{
		character = *(ptr++);

		if (TNC->HostMode)
		{
			// n       0        Success (nothing follows)
			// n       1        Success (message follows, null terminated)
			// n       2        Failure (message follows, null terminated)
			// n       3        Link Status (null terminated)
			// n       4        Monitor Header (null terminated)
			// n       5        Monitor Header (null terminated)
			// n       6        Monitor Information (preceeded by length-1)
			// n       7        Connect Information (preceeded by length-1)


			switch(TNC->HOSTSTATE)
			{
			case 0: 	//  SETCHANNEL

				TNC->MSGCHANNEL = character;
				TNC->HOSTSTATE++;
				break;

			case 1:		//	SETMSGTYPE

				TNC->MSGTYPE = character;

				if (character == 0)
				{
					// Success, no more info

					ProcessDEDFrame(TNC);
						
					TNC->HOSTSTATE = 0;
					break;
				}

				if (character > 0 && character < 6)
				{
					// Null Terminated Response)
					
					TNC->HOSTSTATE = 5;
					CURSOR = &TNC->DEDBuffer[0];
					break;
				}

				TNC->HOSTSTATE = 2;						// Get Length
				break;

			case 2:		//  Get Length

				TNC->MSGCOUNT = character;
				TNC->MSGCOUNT++;						// Param is len - 1
				TNC->MSGLENGTH = TNC->MSGCOUNT;

				CURSOR = &TNC->DEDBuffer[0];
				TNC->HOSTSTATE = 3;						// Get Data

				break;

			case 5:		//  Collecting Null Terminated Response

				*(CURSOR++) = character;
				
				if (character)
					continue;			// MORE TO COME

				ProcessDEDFrame(TNC);

				TNC->HOSTSTATE = 0;
				TNC->InputLen = 0;

				break;

			default:

			//	RECEIVING Counted Response

			*(CURSOR++) = character;
			TNC->MSGCOUNT--;

			if (TNC->MSGCOUNT)
				continue;			// MORE TO COME

			TNC->InputLen = CURSOR - TNC->DEDBuffer;
			ProcessDEDFrame(TNC);

			TNC->HOSTSTATE = 0;
			TNC->InputLen = 0;
			}
		}
	}

	// End of Input - Save buffer position

	TNC->InputLen = CURSOR - TNC->DEDBuffer;
	TNC->RXLen = 0;
}

static BOOL WriteCommBlock(struct TNCINFO * TNC)
{
	WriteCOMBlock(TNC->hDevice, TNC->TXBuffer, TNC->TXLen);

	TNC->Timeout = 20;				// 2 secs
	return TRUE;
}

static VOID DEDPoll(int Port)
{
	struct TNCINFO * TNC = TNCInfo[Port];
	UCHAR * Poll = TNC->TXBuffer;
	int Stream = 0;
	int nn;
	struct STREAMINFO * STREAM;

	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] && TNC->Streams[Stream].Attached == 0)
		{
			// New Attach. Set call my session callsign

			int calllen=0;

			TNC->Streams[Stream].Attached = TRUE;

			TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4USER[6] |= 0x60; // Ensure P or T aren't used on ax.25
			calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4USER, TNC->Streams[Stream].MyCall);
			TNC->Streams[Stream].MyCall[calllen] = 0;

			if (Stream)			//Leave Stream 0 call alone
			{
				TNC->Streams[Stream].CmdSet = TNC->Streams[Stream].CmdSave = zalloc(100);
				sprintf(TNC->Streams[Stream].CmdSet, "%c%c%cI%s", Stream, 1, 1, TNC->Streams[Stream].MyCall);
			}
		}
	}

	if (TNC->Timeout)
	{
		TNC->Timeout--;
		
		if (TNC->Timeout)			// Still waiting
			return;

		// Can't use retries, as we have no way of detecting lost chars. Have to re-init on timeout

		if (TNC->HostMode == 0 || TNC->ReinitState == 10)		// 10 is Recovery Mode
		{
			DoTermModeTimeout(TNC);
			return;
		}

		// Timed out in host mode - Clear any connection and reinit the TNC

		Debugprintf("DEDHOST - Link to TNC Lost");
		TNC->TNCOK = FALSE;

		TNC->HostMode = 0;
		TNC->ReinitState = 0;

		TNC->InitPtr = TNC->InitScript;
		TNC->HOSTSTATE = 0;
		
		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{
			if (TNC->PortRecord->ATTACHEDSESSIONS[Stream])		// Connected
			{
				TNC->Streams[Stream].Connected = FALSE;		// Back to Command Mode
				TNC->Streams[Stream].ReportDISC = TRUE;		// Tell Node
			}
		}
	}

	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		STREAM = &TNC->Streams[Stream];

		if (STREAM->Attached)
			CheckForDetach(TNC, Stream, STREAM, TidyClose, ForcedClose, CloseComplete);

		if (TNC->Timeout)
			return;				// We've sent something
	}

	// if we have just restarted or TNC appears to be in terminal mode, run Initialisation Sequence

	if (!TNC->HostMode)
	{
		DoTNCReinit(TNC);
		return;
	}

	if (TNC->InitPtr)
	{
		char * start, * end;
		int len;

		start = TNC->InitPtr;
		
		if (*(start) == 0)			// End of Script
		{
			TNC->InitPtr = NULL;
		}
		else
		{
			end = strchr(start, 13);
			len = ++end - start -1;	// exclude cr
			
			TNC->InitPtr = end;

			Poll[0] = 0;			// Channel
			Poll[1] = 1;			// Command
			Poll[2] = len - 1;
			memcpy(&Poll[3], start, len);
		
			StuffAndSend(TNC, Poll, len + 3);

			return;

		}
	}
		
	for (Stream = 0; Stream <= MaxStreams; Stream++)
	{
		if (TNC->Streams[Stream].CmdSet)
		{
			char * start, * end;
			int len;

			start = TNC->Streams[Stream].CmdSet;
		
			if (*(start + 2) == 0)			// End of Script
			{
				free(TNC->Streams[Stream].CmdSave);
				TNC->Streams[Stream].CmdSet = NULL;
			}
			else
			{
				end = strchr(start + 3, 0);
				len = ++end - start -1;	// exclude cr
				TNC->Streams[Stream].CmdSet = end;

				memcpy(&Poll[0], start, len);
				Poll[2] = len - 4;
		
				StuffAndSend(TNC, Poll, len);

				return;
			}
		}
	}

	for (nn = 0; nn <= MaxStreams; nn++)
	{
		Stream = TNC->LastStream++;

		if (TNC->LastStream > MaxStreams) TNC->LastStream = 0;

		if (TNC->TNCOK && TNC->Streams[Stream].BPQtoPACTOR_Q)
		{
			int datalen;
			UINT * buffptr;
			char * Buffer;
			
			buffptr=Q_REM(&TNC->Streams[Stream].BPQtoPACTOR_Q);

			datalen=buffptr[1];
			Buffer = (char *)&buffptr[2];	// Data portion of frame

			Poll[0] = TNC->Streams[Stream].DEDStream;		// Channel

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

				Poll[1] = 0;			// Data
				TNC->Streams[Stream].BytesTXed += datalen;

				Poll[2] = datalen - 1;
				memcpy(&Poll[3], buffptr+2, datalen);
		
				ReleaseBuffer(buffptr);
		
				StuffAndSend(TNC, Poll, datalen + 3);

				TNC->Streams[Stream].InternalCmd = TNC->Streams[Stream].Connected;

				if (STREAM->Disconnecting && TNC->Streams[Stream].BPQtoPACTOR_Q == 0)
					TidyClose(TNC, 0);

				return;
			}
			
			// Command. Do some sanity checking and look for things to process locally

			Poll[1] = 1;			// Command
			datalen--;				// Exclude CR

			if (datalen == 0)		// Null Command
			{
				ReleaseBuffer(buffptr);
				return;
			}

			Buffer[datalen] = 0;	// Null Terminate
			_strupr(Buffer);

			if (_memicmp(Buffer, "D", 1) == 0)
			{
				TNC->Streams[Stream].ReportDISC = TRUE;		// Tell Node
				ReleaseBuffer(buffptr);
				return;
			}

			if (Buffer[0] == 'C' && datalen > 2)	// Connect
			{
				if (Stream == 0)
				{
					// No connects on Stream zero - for mgmt only

					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "TRK} Can't Connect after ATTACH\r");
					C_Q_ADD(&TNC->Streams[0].PACTORtoBPQ_Q, buffptr);
				
					return;

				}
				
				if (*(++Buffer) == ' ') Buffer++;		// Space isn't needed

				memcpy(TNC->Streams[Stream].RemoteCall, Buffer, 9);

				TNC->Streams[Stream].Connecting = TRUE;

				TNC->Streams[Stream].CmdSet = TNC->Streams[Stream].CmdSave = zalloc(100);
							
				sprintf(TNC->Streams[Stream].CmdSet, "%c%c%cI%s%c%c%c%c%s", Stream, 1, 1,
					TNC->Streams[Stream].MyCall, 0, Stream, 1, 1, (char *)buffptr+8);

				ReleaseBuffer(buffptr);
	
				TNC->Streams[Stream].InternalCmd = FALSE;
				return;				
			}

			Poll[2] = datalen - 1;
			memcpy(&Poll[3], buffptr+2, datalen);
		
			ReleaseBuffer(buffptr);
		
			StuffAndSend(TNC, Poll, datalen + 3);

			TNC->Streams[Stream].InternalCmd = TNC->Streams[Stream].Connected;

			return;
		}
	}

	if (TNC->TNCOK && TNC->PortRecord->UI_Q)
	{
		int datalen;
		char * Buffer;
		char CCMD[80] = "C";
		char Call[12] = "           ";		
		struct _MESSAGE * buffptr;
			
		buffptr = Q_REM(&TNC->PortRecord->UI_Q);
		
		datalen = buffptr->LENGTH - 7;
		Buffer = &buffptr->DEST[0];		// Raw Frame
		Buffer[datalen] = 0;

		TNC->Streams[0].CmdSet = TNC->Streams[0].CmdSave = zalloc(100);
							
//		sprintf(TNC->Streams[Stream].CmdSet, "I%s\r%s\r", TNC->Streams[Stream].MyCall, buffptr+2);

		// Buffer has an ax.25 header, which we need to pick out and set as channel 0 Connect address
		// before sending the beacon

		ConvFromAX25(Buffer, &Call[1]);			// Dest
		strlop(&Call[1], ' ');
		strcat(CCMD, Call);
		Buffer += 14;							// Skip Origin
		datalen -= 7;

		while ((Buffer[-1] & 1) == 0)
		{
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

			Poll[0] = 0;				// UI Channel
			Poll[1] = 1;				// Data
			Poll[2] = strlen(CCMD) - 1;
			strcpy(&Poll[3], CCMD);
			StuffAndSend(TNC, Poll, Poll[2] + 4);

			sprintf(TNC->Streams[0].CmdSet, "%c%c%c%s", 0, 0, 1, Buffer);
		}

		ReleaseBuffer((UINT *)buffptr);
		return;
	}

	// if frames outstanding, issue a poll (but not too often)

	TNC->IntCmdDelay++;

	if (TNC->IntCmdDelay > 10)
	{
		TNC->IntCmdDelay = 0;

		Poll[0] = TNC->Streams[0].DEDStream;
		Poll[1] = 0x1;			// Command
		TNC->Streams[0].InternalCmd = TRUE;
	
		Poll[2] = 1;			// Len-1
		Poll[3] = '@';
		Poll[4] = 'B';			// Buffers
		StuffAndSend(TNC, Poll, 5);
		return;
	}

	// Need to poll all channels . Just Poll zero here, the ProcessMessage will poll next

	Poll[0] = 0;		// Channel
	Poll[1] = 0x1;			// Command
	Poll[2] = 0;			// Len-1
	Poll[3] = 'G';			// Poll

	StuffAndSend(TNC, Poll, 4);

	return;


	Stream = TNC->StreamtoPoll;

	STREAM = &TNC->Streams[Stream];

	STREAM->IntCmdDelay++;
	
	if (STREAM->IntCmdDelay > 10)
	{
		STREAM->IntCmdDelay = 0;
		
		if (STREAM->FramesOutstanding)
		{
			Poll[0] = STREAM->DEDStream;
			Poll[1] = 0x1;			// Command
			STREAM->InternalCmd = TRUE;
	
			Poll[2] = 0;			// Len-1
			Poll[3] = 'L';			// Status
			StuffAndSend(TNC, Poll, 4);

			return;	
		}
	}


	Poll[0] = Stream;		// Channel
	Poll[1] = 0x1;			// Command
	Poll[2] = 0;			// Len-1
	Poll[3] = 'G';			// Poll

	StuffAndSend(TNC, Poll, 4);
	STREAM->InternalCmd = FALSE;

	return;

}

static VOID DoTNCReinit(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 0)
	{
		// Just Starting - Send a TNC Mode Command to see if in Terminal or Host Mode

		TNC->TNCOK = FALSE;

		memcpy(&TNC->TXBuffer[0], "\x18\x1b\r", 2);
		TNC->TXLen = 2;

		WriteCommBlock(TNC);
		return;
	}

	if (TNC->ReinitState == 1)		// Forcing back to Term
		TNC->ReinitState = 0;

	if (TNC->ReinitState == 2)		// In Term State, Sending Initialisation Commands
	{
		// Put into Host Mode

		memcpy(Poll, "\x18\x1bJHOST1\r", 9);

		TNC->TXLen = 9;
		WriteCommBlock(TNC);

		TNC->ReinitState = 5;
		return;
	}

	if (TNC->ReinitState == 5)
		TNC->ReinitState = 0;

}

static VOID DoTermModeTimeout(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 0)
	{
		//Checking if in Terminal Mode - Try to set back to Term Mode

		TNC->ReinitState = 1;
		ExitHost(TNC);

		return;
	}

	if (TNC->ReinitState == 1)
	{
		// No Response to trying to enter term mode - do error recovery

		TNC->ReinitState = 10;
		TNC->ReinitCount = 256;
		TNC->HostMode = TRUE;			// Must be in Host Mode if we need recovery

		Poll[0] = 1;
		TNC->TXLen = 1;
		WriteCommBlock(TNC);
		TNC->Timeout = 10;				// 2 secs

		return;
	}

	if (TNC->ReinitState == 10)
	{
		// Continue error recovery

		TNC->ReinitCount--;

		if (TNC->ReinitCount)
		{
			Poll[0] = 1;
			TNC->TXLen = 1;
			WriteCommBlock(TNC);
			TNC->Timeout = 5;				// 1/2 secs

			return;
		}

		// Try Again
		
		TNC->ReinitState = 1;
		ExitHost(TNC);

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


static VOID ExitHost(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	// Try to exit Host Mode

	TNC->TXBuffer[0] = 1;
	TNC->TXBuffer[1] = 1;
	TNC->TXBuffer[2] = 1;
	memcpy(&TNC->TXBuffer[3], "%R", 2);

	StuffAndSend(TNC, Poll, 5);

	return;
}

static VOID ProcessTermModeResponse(struct TNCINFO * TNC)
{
	UCHAR * Poll = TNC->TXBuffer;

	if (TNC->ReinitState == 0)
	{
		// Testing if in Term Mode. It is, so can now send Init Commands

		TNC->InitPtr = TNC->InitScript;
		TNC->ReinitState = 2;

		// Send Restart to make sure PTC is in a known state
	}
	if (TNC->ReinitState == 2)
	{
		// Sending Init Commands

		DoTNCReinit(TNC);		// Send Next Command
		return;
	}

	if (TNC->ReinitState == 5)	// Waiting for response to JHOST1
	{
		if (TNC->RXBuffer[TNC->RXLen-1] == 10 || TNC->RXBuffer[TNC->RXLen-1] == 13)	// NewLine
		{
			TNC->HostMode = TRUE;
			TNC->Timeout = 0;
		}
		return;
	}
}

static VOID ProcessDEDFrame(struct TNCINFO * TNC)
{
	UINT * buffptr;
	char * Buffer;				// Data portion of frame
	UINT Stream = 0;
	UCHAR * Msg = TNC->DEDBuffer;
	int framelen = TNC->InputLen;
	struct STREAMINFO * STREAM;

	if (TNC->ReinitState == 10)
	{
		// Recovering from Sync Failure

		// Any Response indicates we are in host mode, and back in sync

		TNC->HostMode = TRUE;
		TNC->Timeout = 0;
		TNC->ReinitState = 0;
		TNC->RXLen = 0;
		TNC->HOSTSTATE = 0;
		return;
	}

	// Any valid frame is an ACK

	TNC->Timeout = 0;
	TNC->TNCOK = TRUE;

	if (TNC->InitPtr)					// Response to Init Script
		return;

	if (TNC->MSGCHANNEL > 26)
		return;

	Stream = TNC->MSGCHANNEL;

	//	See if Poll Reply or Data
	
	if (TNC->MSGTYPE == 0)
	{
		// Success - Nothing Follows

		if (TNC->Streams[Stream].CmdSet)
				return;						// Response to Command Set or Init Script

		if ((TNC->TXBuffer[1] & 1) == 0)	// Data
			return;

		// If the response to a Command, then we should convert to a text "Ok" for forward scripts, etc

		if (TNC->TXBuffer[3] == 'G')	// Poll
		{
			UCHAR * Poll = TNC->TXBuffer;

			// Poll Next Channel (we need to scan all channels every DEDPOLL cycle

			Stream++;

			if (Stream > MaxStreams)
				return;
	
			STREAM = &TNC->Streams[Stream];

			STREAM->IntCmdDelay++;
	
			if (STREAM->IntCmdDelay > 10)
			{
				STREAM->IntCmdDelay = 0;
		
				if (STREAM->FramesOutstanding)
				{
					Poll[0] = STREAM->DEDStream;
					Poll[1] = 0x1;			// Command
					STREAM->InternalCmd = TRUE;
	
					Poll[2] = 0;			// Len-1
					Poll[3] = 'L';			// Status
					StuffAndSend(TNC, Poll, 4);
					return;	
				}
			}

			Poll[0] = Stream;		// Channel
			Poll[1] = 0x1;			// Command
			Poll[2] = 0;			// Len-1
			Poll[3] = 'G';			// Poll

			StuffAndSend(TNC, Poll, 4);
			STREAM->InternalCmd = FALSE;
	
			return;
		}

		if (TNC->TXBuffer[3] == 'C')	// Connect - reply we need is async
			return;

		if (TNC->TXBuffer[3] == 'L')	// Shouldnt happen!
			return;


		if (TNC->TXBuffer[3] == 'J')	// JHOST
		{
			if (TNC->TXBuffer[8] == '0')	// JHOST0
			{
				TNC->Timeout = 1;			// 
				return;
			}
		}

		if (TNC->MSGCHANNEL == 0)			// Unproto Channel
			return;

		buffptr = GetBuff();

		if (buffptr == NULL) return;			// No buffers, so ignore

		buffptr[1] = sprintf((UCHAR *)&buffptr[2],"TRK} Ok\r");

		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

		return;
	}

	if (TNC->MSGTYPE > 0 &&TNC->MSGTYPE < 6)
	{
		// Success with message - null terminated

		char * ptr;
		int len;

		Buffer = Msg;
		
		ptr = strchr(Buffer, 0);

		if (ptr == 0)
			return;

		*(ptr++) = 13;
		*(ptr) = 0;

		len = ptr - Buffer;

		if (len > 256)
			return;

		// See if we need to process locally (Response to our command, Incoming Call, Disconencted, etc

		if (TNC->MSGTYPE < 3)						// 1 or 2 - Success or Fail
		{
			// See if a response to internal command

			if (TNC->Streams[Stream].InternalCmd)
			{
				// Process it

				char LastCmd = TNC->TXBuffer[3];

				if (LastCmd == 'L')		// Status
				{
					int s1, s2, s3, s4, s5, s6, num;

					num = sscanf(Buffer, "%d %d %d %d %d %d", &s1, &s2, &s3, &s4, &s5, &s6);
			
					TNC->Streams[Stream].FramesOutstanding = s3;
					return;
				}

				if (LastCmd == '@')		// @ Commands
				{
					if (TNC->TXBuffer[4]== 'B')	// Buffer Status
					{
						TNC->Buffers = atoi(Buffer);
//						SetDlgItemText(TNC->hDlg, IDC_BUFFERS, Buffer);
						return;
					}
				}
					
				return;
			}

			// Not Internal Command, so send to user

			if (TNC->Streams[Stream].CmdSet)
				return;						// Response to Command Set

			if ((TNC->TXBuffer[1] & 1) == 0)	// Data
				return;

		// If the response to a Command, then we should convert to a text "Ok" for forward scripts, etc

		if (TNC->TXBuffer[3] == 'G')	// Poll
			return;

		if (TNC->TXBuffer[3] == 'C')	// Connect - reply we need is async
			return;

		if (TNC->TXBuffer[3] == 'L')	// Shouldnt happen!
			return;

		if (TNC->TXBuffer[3] == 'J')	// JHOST
		{
			if (TNC->TXBuffer[8] == '0')	// JHOST0
			{
				TNC->Timeout = 1;			// 
				return;
			}
		}	

		if (TNC->MSGCHANNEL == 0)			// Unproto Channel
			return;

		buffptr = GetBuff();

		if (buffptr == NULL) return;			// No buffers, so ignore

		buffptr[1] = sprintf((UCHAR *)&buffptr[2],"TRK} %s", Buffer);

		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

		return;

		}

		if (TNC->MSGTYPE == 3)					// Status
		{			
			struct STREAMINFO * STREAM = &TNC->Streams[Stream];

			if (strstr(Buffer, "DISCONNECTED") || strstr(Buffer, "LINK FAILURE") || strstr(Buffer, "BUSY"))
			{
				if ((STREAM->Connecting | STREAM->Connected) == 0)
					return;

				if (STREAM->Connecting && STREAM->Disconnecting == FALSE)
				{
					// Connect Failed
			
					buffptr = GetBuff();
					if (buffptr == 0) return;			// No buffers, so ignore

					if (strstr(Buffer, "BUSY"))
						buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Busy from %s\r", TNC->Streams[Stream].RemoteCall);
					else
						buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Failure with %s\r", TNC->Streams[Stream].RemoteCall);

					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
	
					STREAM->Connecting = FALSE;
					STREAM->Connected = FALSE;				// In case!
					STREAM->FramesOutstanding = 0;

					STREAM->DiscWhenAllSent = 15;			// Dont want to leave session attached. Causes too much confusion

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

				Call += 4;

				if (Call[1] == ':')
					Call +=2;

				ptr = strchr(Call, ' ');	
				if (ptr) *ptr = 0;

				ptr = strchr(Call, 13);	
				if (ptr) *ptr = 0;

				STREAM->Connected = TRUE;			// Subsequent data to data channel
				STREAM->Connecting = FALSE;

				STREAM->BytesRXed = STREAM->BytesTXed = 0;

				memcpy(MHCall, Call, 9);
				MHCall[9] = 0;

				if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] == 0)
				{
					// Incoming Connect

//					APPLCALLS * APPL;
//					char * ApplPtr = &APPLS;
//					int App;
//					char Appl[10];
//					char DestCall[10];

					UpdateMH(TNC, MHCall, '+', 'I');

					ProcessIncommingConnect(TNC, Call, Stream, TRUE);

					if (FULL_CTEXT && HFCTEXTLEN == 0)
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
				else
				{
					// Connect Complete
			
					buffptr = GetBuff();
					if (buffptr == 0) return;			// No buffers, so ignore

					buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Connected to %s\r", Call);;

					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

				}		
			}
			return;
		}

		if (TNC->MSGTYPE == 4 || TNC->MSGTYPE == 5)
		{
			struct STREAMINFO * STREAM = &TNC->Streams[0];		// RP Stream

			// Monitor

/*
		if (TNC->UseAPPLCalls && strstr(&Msg[4], "SABM") && STREAM->Attached == FALSE)
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

								sprintf(Status, "%d SCANSTART 60", TNC->Port);	// Pause scan for 60 secs
								Rig_Command(-1, Status);
								TNC->SwitchToPactor = 600;		// Don't change modes for 60 secs

								strcpy(STREAM->MyCall, DestCall);
								STREAM->CmdSet = STREAM->CmdSave = zalloc(100);
								sprintf(STREAM->CmdSet, "I%s\r", DestCall);
								break;
							}
						}
					}
				}
			}
*/
			DoMonitorHddr(TNC, Msg, framelen, TNC->MSGTYPE);
			return;

		
		}

		// 1, 2, 4, 5 - pass to Appl

		if (TNC->MSGCHANNEL == 0)			// Unproto Channel
			return;

		buffptr = GetBuff();

		if (buffptr == NULL) return;			// No buffers, so ignore

		buffptr[1] = sprintf((UCHAR *)&buffptr[2],"Trk} %s", &Msg[4]);

		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

		return;
	}

	if (TNC->MSGTYPE == 6)
	{
		// Monitor Data With length)

		DoMonitorData(TNC, Msg, framelen);
		return;
	}

	if (TNC->MSGTYPE == 7)
	{
		//char StatusMsg[60];
		//int Status, ISS, Offset;
		
		// Connected Data
		
		buffptr = GetBuff();

		if (buffptr == NULL) return;			// No buffers, so ignore
			
		buffptr[1] = framelen;				// Length
		TNC->Streams[Stream].BytesRXed += buffptr[1];
		memcpy(&buffptr[2], Msg, buffptr[1]);
		C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

		return;
	}
}

VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
	// Queue it as we may have just sent data

	TNC->Streams[Stream].CmdSet = TNC->Streams[Stream].CmdSave = zalloc(100);
	sprintf(TNC->Streams[Stream].CmdSet, "%c%c%cD", Stream, 1, 1);
}


VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	TidyClose(TNC, Stream);			// I don't think Hostmode has a DD
}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
}


