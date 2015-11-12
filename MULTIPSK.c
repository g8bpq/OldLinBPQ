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
//	DLL to provide interface to allow G8BPQ switch to use MultoPSK ALE400 Mode
//
//	Uses BPQ EXTERNAL interface
//


#define _CRT_SECURE_NO_DEPRECATE

#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include "CHeaders.h"
#include <stdio.h>
#include <time.h>

#include "tncinfo.h"

#include "bpq32.h"

#define VERSION_MAJOR         2
#define VERSION_MINOR         0

#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02

#define TIMESTAMP 352

#define CONTIMEOUT 1200



#define AGWHDDRLEN sizeof(struct AGWHEADER)

unsigned long _beginthread( void( *start_address )(), unsigned stack_size, int arglist);

extern int (WINAPI FAR *GetModuleFileNameExPtr)();

//int ResetExtDriver(int num);
extern char * PortConfig[33];

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

static void ConnecttoMPSKThread(int port);

void CreateMHWindow();
int Update_MH_List(struct in_addr ipad, char * call, char proto);

static int ConnecttoMPSK();
static int ProcessReceivedData(int bpqport);
static ProcessLine(char * buf, int Port);
int KillTNC(struct TNCINFO * TNC);
int RestartTNC(struct TNCINFO * TNC);
VOID ProcessMPSKPacket(struct TNCINFO * TNC, char * Message, int Len);
struct TNCINFO * GetSessionKey(char * key, struct TNCINFO * TNC);
static VOID SendData(struct TNCINFO * TNC, char * Msg, int MsgLen);
static VOID DoMonitorHddr(struct TNCINFO * TNC, struct AGWHEADER * RXHeader, UCHAR * Msg);
VOID SendRPBeacon(struct TNCINFO * TNC);

char * strlop(char * buf, char delim);

extern UCHAR BPQDirectory[];

#define MAXBPQPORTS 32
#define MAXMPSKPORTS 16

//LOGFONT LFTTYFONT ;

//HFONT hFont ;

static int MPSKChannel[MAXBPQPORTS+1];			// BPQ Port to MPSK Port
static int BPQPort[MAXMPSKPORTS][MAXBPQPORTS+1];	// MPSK Port and Connection to BPQ Port
static int MPSKtoBPQ_Q[MAXBPQPORTS+1];			// Frames for BPQ, indexed by BPQ Port
static int BPQtoMPSK_Q[MAXBPQPORTS+1];			// Frames for MPSK. indexed by MPSK port. Only used it TCP session is blocked

static int MasterPort[MAXBPQPORTS+1];			// Pointer to first BPQ port for a specific MPSK host

//	Each port may be on a different machine. We only open one connection to each MPSK instance

static char * MPSKSignon[MAXBPQPORTS+1];			// Pointer to message for secure signin

static unsigned int MPSKInst = 0;
static int AttachedProcesses=0;

static HWND hResWnd,hMHWnd;
static BOOL GotMsg;

static HANDLE STDOUT=0;

//SOCKET sock;

static SOCKADDR_IN sinx; 
static SOCKADDR_IN rxaddr;
static SOCKADDR_IN destaddr[MAXBPQPORTS+1];

static int addrlen=sizeof(sinx);

//static short MPSKPort=0;

static time_t ltime,lasttime[MAXBPQPORTS+1];

static BOOL CONNECTING[MAXBPQPORTS+1];
static BOOL CONNECTED[MAXBPQPORTS+1];

//HANDLE hInstance;


static fd_set readfs;
static fd_set writefs;
static fd_set errorfs;
static struct timeval timeout;

#ifndef LINBPQ

static BOOL CALLBACK EnumTNCWindowsProc(HWND hwnd, LPARAM  lParam)
{
	char wtext[200];
	struct TNCINFO * TNC = (struct TNCINFO *)lParam; 
	UINT ProcessId;
	char FN[MAX_PATH] = "";

	if (TNC->ProgramPath == NULL)
		return FALSE;

	GetWindowText(hwnd, wtext, 199);

	if (strstr(wtext,"* MULTIPSK"))
	{
		GetWindowThreadProcessId(hwnd, &ProcessId);

		TNC->WIMMORPID = ProcessId;
		return FALSE;
	}
	
	return (TRUE);
}

#endif

static int ExtProc(int fn, int port,unsigned char * buff)
{
	UINT * buffptr;
	char txbuff[500];
	unsigned int bytes,txlen=0;
	struct TNCINFO * TNC = TNCInfo[port];
	int Stream = 0;
	struct STREAMINFO * STREAM;
	int TNCOK;

	if (TNC == NULL)
		return 0;					// Port not defined

	// Look for attach on any call

	for (Stream = 0; Stream <= TNC->MPSKInfo->MaxSessions; Stream++)
	{
		STREAM = &TNC->Streams[Stream];
	
		if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] && TNC->Streams[Stream].Attached == 0)
		{
			char Cmd[80];
			int len;

			// New Attach

			int calllen;
			STREAM->Attached = TRUE;

			calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4USER, STREAM->MyCall);
			STREAM->MyCall[calllen] = 0;
			STREAM->FramesOutstanding = 0;

			// Stop Scanning

			sprintf(Cmd, "%d SCANSTOP", TNC->Port);
			Rig_Command(-1, Cmd);

			len = sprintf(Cmd, "%cSTOP_BEACON_ARQ_FAE\x1b", '\x1a');
	
			if (TNC->MPSKInfo->TX)
				TNC->CmdSet = TNC->CmdSave = _strdup(Cmd);		// Savde till not transmitting
			else
				send(TNC->WINMORSock, Cmd, len, 0);

		}
	}

	switch (fn)
	{
	case 1:				// poll

		if (MasterPort[port] == port)
		{
			// Only on first port using a host

			if (TNC->CONNECTED == FALSE && TNC->CONNECTING == FALSE)
			{
				//	See if time to reconnect
		
				time( &ltime );
				if (ltime-lasttime[port] >9 )
				{
					ConnecttoMPSK(port);
					lasttime[port]=ltime;
				}
			}
		
			FD_ZERO(&readfs);
			
			if (TNC->CONNECTED) FD_SET(TNC->WINMORSock,&readfs);

			
			FD_ZERO(&writefs);

			if (TNC->CONNECTING) FD_SET(TNC->WINMORSock,&writefs);	// Need notification of Connect

			if (TNC->BPQtoWINMOR_Q) FD_SET(TNC->WINMORSock,&writefs);	// Need notification of busy clearing



			FD_ZERO(&errorfs);
		
			if (TNC->CONNECTING ||TNC->CONNECTED) FD_SET(TNC->WINMORSock,&errorfs);

			if (select(3,&readfs,&writefs,&errorfs,&timeout) > 0)
			{
				//	See what happened

				if (FD_ISSET(TNC->WINMORSock,&readfs))
				{
					// data available
			
					ProcessReceivedData(port);			
				}

				if (FD_ISSET(TNC->WINMORSock,&writefs))
				{
					if (BPQtoMPSK_Q[port] == 0)
					{
						//	Connect success

						TNC->CONNECTED = TRUE;
						TNC->CONNECTING = FALSE;

						// If required, send signon
				
						send(TNC->WINMORSock,"\x1a", 1, 0);
						send(TNC->WINMORSock,"DIGITAL MODE ?", 14, 0);
						send(TNC->WINMORSock,"\x1b", 1, 0);

//						EnumWindows(EnumTNCWindowsProc, (LPARAM)TNC);
					}
					else
					{
						// Write block has cleared. Send rest of packet

						buffptr=Q_REM(&BPQtoMPSK_Q[port]);

						txlen=buffptr[1];

						memcpy(txbuff,buffptr+2,txlen);

						bytes=send(TNC->WINMORSock,(const char FAR *)&txbuff,txlen,0);
					
						ReleaseBuffer(buffptr);

					}

				}
					
				if (FD_ISSET(TNC->WINMORSock,&errorfs))
				{

					//	if connecting, then failed, if connected then has just disconnected

//					if (CONNECTED[port])
//					if (!CONNECTING[port])
//					{
//						i=sprintf(ErrMsg, "MPSK Connection lost for BPQ Port %d\r\n", port);
//						WritetoConsole(ErrMsg);
//					}

					CONNECTING[port]=FALSE;
					CONNECTED[port]=FALSE;
				
				}

			}

		}

		// See if any frames for this port

		for (Stream = 0; Stream <= TNC->MPSKInfo->MaxSessions; Stream++)
		{
			STREAM = &TNC->Streams[Stream];

			// Have to time out connects, as TNC doesn't report failure

			if (STREAM->Connecting)
			{
				STREAM->Connecting--;
			
				if (STREAM->Connecting == 0)
				{
					// Report Connect Failed, and drop back to command mode

					buffptr = GetBuff();

					if (buffptr)
					{
						buffptr[1] = sprintf((UCHAR *)&buffptr[2], "MPSK} Failure with %s\r", STREAM->RemoteCall);
						C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
					}
	
					STREAM->Connected = FALSE;		// Back to Command Mode
					STREAM->DiscWhenAllSent = 10;

					// Send Disc to TNC

					TidyClose(TNC, Stream);
				}
			}
			
			if (STREAM->Attached)
				CheckForDetach(TNC, Stream, STREAM, TidyClose, ForcedClose, CloseComplete);

			if (STREAM->ReportDISC)
			{
				STREAM->ReportDISC = FALSE;
				buff[4] = Stream;

				return -1;
			}

			// if Busy, send buffer status poll
	
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
	//			buff[5]=(datalen & 0xff);
	//			buff[6]=(datalen >> 8);
		
				ReleaseBuffer(buffptr);
	
				return (1);
			}
		}

		if (TNC->PortRecord->UI_Q)
		{
			struct _MESSAGE * buffptr;

			SOCKET Sock;	
			buffptr = Q_REM(&TNC->PortRecord->UI_Q);

			Sock = TNCInfo[MasterPort[port]]->WINMORSock;
	
			ReleaseBuffer((UINT *)buffptr);
		}
			
	
		return (0);



	case 2:				// send

		
		if (!TNCInfo[MasterPort[port]]->CONNECTED) return 0;		// Don't try if not connected to TNC

		Stream = buff[4];
		
		STREAM = &TNC->Streams[Stream]; 

//		txlen=(buff[6]<<8) + buff[5] - 8;	

		txlen = GetLengthfromBuffer(buff) - 8;
						
		if (STREAM->Connected)
		{
			SendData(TNC, &buff[8], txlen);
		}
		else
		{
			char Command[80];
			int len;

			buff[8 + txlen] = 0;
			_strupr(&buff[8]);

			if (_memicmp(&buff[8], "D\r", 2) == 0)
			{
				TidyClose(TNC, buff[4]);
				STREAM->ReportDISC = TRUE;		// Tell Node
				return 0;
			}

			// See if Local command (eg RADIO)

			if (_memicmp(&buff[8], "RADIO ", 6) == 0)
			{
				sprintf(&buff[8], "%d %s", TNC->Port, &buff[14]);

				if (Rig_Command(TNC->PortRecord->ATTACHEDSESSIONS[0]->L4CROSSLINK->CIRCUITINDEX, &buff[8]))
				{
				}
				else
				{
					UINT * buffptr = GetBuff();

					if (buffptr == 0) return 1;			// No buffers, so ignore

					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "%s", &buff[8]);
					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
				}
				return 1;
			}

			if (STREAM->Connecting && _memicmp(&buff[8], "ABORT", 5) == 0)
			{
				len = sprintf(Command,"%cSTOP_SELECTIVE_CALL_ARQ_FAE\x1b", '\x1a');
	
				if (TNC->MPSKInfo->TX)
					TNC->CmdSet = TNC->CmdSave = _strdup(Command);		// Save till not transmitting
				else
					send(TNC->WINMORSock, Command, len, 0);

				TNC->InternalCmd = TRUE;
				return (0);
			}

			if (_memicmp(&buff[8], "MODE", 4) == 0)
			{
				buff[7 + txlen] = 0;		// Remove CR
				
				len = sprintf(Command,"%cDIGITAL MODE %s\x1b", '\x1a', &buff[13]);
	
				if (TNC->MPSKInfo->TX)
					TNC->CmdSet = TNC->CmdSave = _strdup(Command);		// Save till not transmitting
				else
					send(TNC->WINMORSock, Command, len, 0);

				TNC->InternalCmd = TRUE;
				return (0);
			}


			if (_memicmp(&buff[8], "INUSE?", 6) == 0)
			{
				// Return Error if in use, OK if not

				UINT * buffptr = GetBuff();
				int s = 0;

				while(s <= TNC->MPSKInfo->MaxSessions)
				{
					if (s != Stream)
					{		
						if (TNC->PortRecord->ATTACHEDSESSIONS[s])
						{
							buffptr[1] = sprintf((UCHAR *)&buffptr[2], "MPSK} Error - In use\r");
							C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
							return 1;							// Busy
						}
					}
					s++;
				}
				buffptr[1] = sprintf((UCHAR *)&buffptr[2], "MPSK} Ok - Not in use\r");
				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
			
				return 1;
			}

			// See if a Connect Command.

			if (toupper(buff[8]) == 'C' && buff[9] == ' ' && txlen > 2)	// Connect
			{
				char * ptr;
				char * context;

				_strupr(&buff[8]);
				buff[8 + txlen] = 0;

				memset(STREAM->RemoteCall, 0, 10);

				ptr = strtok_s(&buff[10], " ,\r", &context);
				strcpy(STREAM->RemoteCall, ptr);

				len = sprintf(Command,"%cCALLSIGN_TO_CALL_ARQ_FAE %s%c%cSELECTIVE_CALL_ARQ_FAE\x1b",
					'\x1a', STREAM->RemoteCall, '\x1b', '\x1a');

				if (TNC->MPSKInfo->TX)
					TNC->CmdSet = TNC->CmdSave = _strdup(Command);		// Save till not transmitting
				else
					send(TNC->WINMORSock, Command, len, 0);
		
				STREAM->Connecting = TNC->MPSKInfo->ConnTimeOut;	// It doesn't report failure

//				sprintf(Status, "%s Connecting to %s", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
//				SetDlgItemText(TNC->hDlg, IDC_TNCSTATE, Status);

				return 0;
			}

			// Send any other command to Multipsk

			_strupr(&buff[8]);
			buff[7 + txlen] = 0;
			len = sprintf(Command,"%c%s\x1b", '\x1a', &buff[8]);
		
			if (TNC->MPSKInfo->TX)
				TNC->CmdSet = TNC->CmdSave = _strdup(Command);		// Save till not transmitting
			else
				send(TNC->WINMORSock, Command, len, 0);

			TNC->InternalCmd = TRUE;

		}

		return (0);

	case 3:	

		Stream = (int)buff;

		TNCOK = TNCInfo[MasterPort[port]]->CONNECTED;

		STREAM = &TNC->Streams[Stream];

		if (STREAM->FramesOutstanding > 8)	
			return (1 | TNCOK << 8 | STREAM->Disconnecting << 15);

		return TNCOK << 8 | STREAM->Disconnecting << 15;		// OK, but lock attach if disconnecting
	
		break;

	case 4:				// reinit

		shutdown(TNC->WINMORSock, SD_BOTH);
		Sleep(100);

		closesocket(TNC->WINMORSock);
		TNC->CONNECTED = FALSE;

		if (TNC->WIMMORPID && TNC->WeStartedTNC)
		{
			KillTNC(TNC);
			RestartTNC(TNC);
		}

		return (0);

	case 5:				// Close

		shutdown(TNC->WINMORSock, SD_BOTH);
		Sleep(100);

		closesocket(TNC->WINMORSock);

		if (TNC->WIMMORPID && TNC->WeStartedTNC)
		{
			KillTNC(TNC);
		}

		return 0;
	}

	return 0;
}

#ifndef LINBPQ

static KillTNC(struct TNCINFO * TNC)
{
	HANDLE hProc;

	if (TNC->PTTMode)
		Rig_PTT(TNC->RIG, FALSE);			// Make sure PTT is down

	if (TNC->WIMMORPID == 0) return 0;

	hProc =  OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, TNC->WIMMORPID);

	if (hProc)
	{
		TerminateProcess(hProc, 0);
		CloseHandle(hProc);
	}

	TNC->WIMMORPID = 0;			// So we don't try again

	return 0;
}

static RestartTNC(struct TNCINFO * TNC)
{
	STARTUPINFO  SInfo;			// pointer to STARTUPINFO 
    PROCESS_INFORMATION PInfo; 	// pointer to PROCESS_INFORMATION 
	char HomeDir[MAX_PATH];
	int i, ret;

	SInfo.cb=sizeof(SInfo);
	SInfo.lpReserved=NULL; 
	SInfo.lpDesktop=NULL; 
	SInfo.lpTitle=NULL; 
	SInfo.dwFlags=0; 
	SInfo.cbReserved2=0; 
  	SInfo.lpReserved2=NULL; 

	if (TNC->ProgramPath)
	{
		strcpy(HomeDir, TNC->ProgramPath);
		i = strlen(HomeDir);

		while(--i)
		{
			if (HomeDir[i] == '/' || HomeDir[i] == '\\')
			{
				HomeDir[i] = 0;
				break;
			}
		}
		ret = CreateProcess(TNC->ProgramPath, "MultiPSK TCP_IP_ON", NULL, NULL, FALSE,0 ,NULL ,HomeDir, &SInfo, &PInfo);

		if (ret)
			TNC->WIMMORPID = PInfo.dwProcessId;

		return ret;
	}
	return 0;
}
#endif

UINT MPSKExtInit(EXTPORTDATA * PortEntry)
{
	int i, port;
	char Msg[255];
	struct TNCINFO * TNC;
	char * ptr;

	//
	//	Will be called once for each MPSK port to be mapped to a BPQ Port
	//	The MPSK port number is in CHANNEL - A=0, B=1 etc
	//
	//	The Socket to connect to is in IOBASE
	//

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

	TNC->Port = port;

	TNC->PortRecord = PortEntry;

	if (PortEntry->PORTCONTROL.PORTCALL[0] == 0)
		memcpy(TNC->NodeCall, MYNODECALL, 10);
	else
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);

	TNC->Interlock = PortEntry->PORTCONTROL.PORTINTERLOCK;

	PortEntry->PORTCONTROL.PROTOCOL = 10;
	PortEntry->PERMITGATEWAY = TRUE;					// Can change ax.25 call on each stream
	PortEntry->PORTCONTROL.PORTQUALITY = 0;
	PortEntry->SCANCAPABILITIES = NONE;					// Scan Control - None

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0)
		PortEntry->PORTCONTROL.PORTPACLEN = 64;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	TNC->Hardware = H_MPSK;

	MPSKChannel[port] = PortEntry->PORTCONTROL.CHANNELNUM-65;
	
	PortEntry->MAXHOSTMODESESSIONS = 1;	

	i=sprintf(Msg,"MPSK Host %s Port %d \n",
		TNC->WINMORHostName, TNC->WINMORPort);

	WritetoConsole(Msg);

	// See if we already have a port for this host

	MasterPort[port] = port;

	for (i = 1; i < port; i++)
	{
		if (i == port) continue;

		if (TNCInfo[i] && TNCInfo[i]->WINMORPort == TNC->WINMORPort &&
			 _stricmp(TNCInfo[i]->WINMORHostName, TNC->WINMORHostName) == 0)
		{
			MasterPort[port] = i;
			break;
		}
	}

	BPQPort[PortEntry->PORTCONTROL.CHANNELNUM-65][MasterPort[port]] = port;
			
#ifndef LINBPQ
	if (MasterPort[port] == port)
	{
		if (EnumWindows(EnumTNCWindowsProc, (LPARAM)TNC))
			if (TNC->ProgramPath)
				TNC->WeStartedTNC = RestartTNC(TNC);

		ConnecttoMPSK(port);
	}
#endif
	time(&lasttime[port]);			// Get initial time value

//	SendMessage(0x40eaa, WM_COMMAND, 0x03000eaa, 0x40eaa);

	return ((int) ExtProc);

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
	struct MPSKINFO * AGW;

	char errbuf[256];

	strcpy(errbuf, buf);

	ptr = strtok(buf, " \t\n\r");

	if(ptr == NULL) return (TRUE);

	if(*ptr =='#') return (TRUE);			// comment

	if(*ptr ==';') return (TRUE);			// comment

	if (_stricmp(buf, "ADDR"))
		return FALSE;						// Must start with ADDR

	ptr = strtok(NULL, " \t\n\r");

	BPQport = Port;
	p_ipad = ptr;

	TNC = TNCInfo[BPQport] = zalloc(sizeof(struct TNCINFO));
	AGW = TNC->MPSKInfo = zalloc(sizeof(struct MPSKINFO)); // AGW Sream Mode Specific Data

	AGW->MaxSessions = 10;
	AGW->ConnTimeOut = CONTIMEOUT;

	TNC->InitScript = malloc(1000);
	TNC->InitScript[0] = 0;
	
		if (p_ipad == NULL)
			p_ipad = strtok(NULL, " \t\n\r");

		if (p_ipad == NULL) return (FALSE);
	
		p_port = strtok(NULL, " \t\n\r");
			
		if (p_port == NULL) return (FALSE);

		TNC->WINMORPort = atoi(p_port);

		TNC->destaddr.sin_family = AF_INET;
		TNC->destaddr.sin_port = htons(TNC->WINMORPort);
		TNC->WINMORHostName = malloc(strlen(p_ipad)+1);

		if (TNC->WINMORHostName == NULL) return TRUE;

		strcpy(TNC->WINMORHostName,p_ipad);

		ptr = strtok(NULL, " \t\n\r");

		if (ptr)
		{
			if (_memicmp(ptr, "PATH", 4) == 0)
			{
				p_cmd = strtok(NULL, "\n\r");
				if (p_cmd) TNC->ProgramPath = _strdup(_strupr(p_cmd));
			}
		}

		// Read Initialisation lines

		while(TRUE)
		{
			if (GetLine(buf) == 0)
				return TRUE;

			strcpy(errbuf, buf);

			if (memcmp(buf, "****", 4) == 0)
				return TRUE;

			ptr = strchr(buf, ';');
			if (ptr)
			{
				*ptr++ = 13;
				*ptr = 0;
			}

			if (_memicmp(buf, "CONTIMEOUT", 10) == 0)
				AGW->ConnTimeOut = atoi(&buf[11]) * 10;
			else
			if (_memicmp(buf, "UPDATEMAP", 9) == 0)
				TNC->PktUpdateMap = TRUE;
			else
			if (_memicmp(buf, "ALEBEACON", 9) == 0) // Send Beacon after each session 
				TNC->MPSKInfo->Beacon = TRUE;
			else
			if (_memicmp(buf, "DEFAULTMODE", 11) == 0) // Send Beacon after each session 
				strcpy(TNC->MPSKInfo->DefaultMode, &buf[12]);
			else
				
			strcat (TNC->InitScript, buf);
		}


	return (TRUE);	
}

static int ConnecttoMPSK(int port)
{
	_beginthread(ConnecttoMPSKThread,0,port);

	return 0;
}

static VOID ConnecttoMPSKThread(port)
{
	char Msg[255];
	int err,i;
	u_long param=1;
	BOOL bcopt=TRUE;
	struct hostent * HostEnt;
	struct TNCINFO * TNC = TNCInfo[port];

	Sleep(5000);		// Allow init to complete 

	TNC->destaddr.sin_addr.s_addr = inet_addr(TNC->WINMORHostName);

	if (TNC->destaddr.sin_addr.s_addr == INADDR_NONE)
	{
		//	Resolve name to address

		 HostEnt = gethostbyname (TNC->WINMORHostName);
		 
		 if (!HostEnt) return;			// Resolve failed

		 memcpy(&TNC->destaddr.sin_addr.s_addr,HostEnt->h_addr,4);
		 memcpy(&TNC->Datadestaddr.sin_addr.s_addr,HostEnt->h_addr,4);

	}

	closesocket(TNC->WINMORSock);

	TNC->WINMORSock=socket(AF_INET,SOCK_STREAM,0);

	if (TNC->WINMORSock == INVALID_SOCKET)
	{
		i=sprintf(Msg, "Socket Failed for MPSK socket - error code = %d\n", WSAGetLastError());
		WritetoConsole(Msg);

  	 	return; 
	}
 
	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	TNC->CONNECTING = TRUE;

	if (connect(TNC->WINMORSock,(LPSOCKADDR) &TNC->destaddr,sizeof(TNC->destaddr)) == 0)
	{
		//
		//	Connected successful
		//

		TNC->CONNECTED=TRUE;
	}
	else
	{
		if (TNC->Alerted == FALSE)
		{
			err=WSAGetLastError();
   			i=sprintf(Msg, "Connect Failed for MPSK socket - error code = %d\n", err);
			WritetoConsole(Msg);
			SetDlgItemText(TNC->hDlg, IDC_COMMSSTATE, "Connection to TNC failed");

			TNC->Alerted = TRUE;
		}
		
		TNC->CONNECTING = FALSE;
		return;
	}

	TNC->LastFreq = 0;			//	so V4 display will be updated

	SetDlgItemText(TNC->hDlg, IDC_COMMSSTATE, "Connected to MPSK TNC");

	return;

}

static int ProcessReceivedData(int port)
{
	unsigned int bytes;
	int i;
	char ErrMsg[255];
	char Message[500];
	struct TNCINFO * TNC = TNCInfo[port];

	//	Need to extract messages from byte stream

	bytes = recv(TNC->WINMORSock,(char *)&Message, 500, 0);

	if (bytes == SOCKET_ERROR)
	{
//		i=sprintf(ErrMsg, "Read Failed for MPSK socket - error code = %d\r\n", WSAGetLastError());
//		WritetoConsole(ErrMsg);
				
		closesocket(TNC->WINMORSock);
					
		TNC->CONNECTED = FALSE;
		if (TNC->Streams[0].Attached)
			TNC->Streams[0].ReportDISC = TRUE;

		return (0);
	}

	if (bytes == 0)
	{
		//	zero bytes means connection closed

		i=sprintf(ErrMsg, "MPSK Connection closed for BPQ Port %d\n", port);
		WritetoConsole(ErrMsg);

		TNC->CONNECTED = FALSE;
		if (TNC->Streams[0].Attached)
			TNC->Streams[0].ReportDISC = TRUE;

		return (0);
	}

	//	Have some data
	
	ProcessMPSKPacket(TNC, Message, bytes);			// Data may be for another port

	return (0);

}

VOID ProcessMSPKCmd(struct TNCINFO * TNC);
VOID ProcessMSPKComment(struct TNCINFO * TNC);
VOID ProcessMSPKData(struct TNCINFO * TNC);

VOID ProcessMPSKPacket(struct TNCINFO * TNC, char * Message, int Len)
{
	char * MPTR = Message;

/*
3) each text character transmitted by the client to the server (for the Multipsk TX text editor) must be preceded by the character CHR(25) or CHR(22) in the case of a special link  (KISS in Packet or Pax, for example).

4) each command string transmitted by the client to the server must be preceded by the character CHR(26) and finished by CHR(27),

5) each character effectively transmitted by Multipsk to the transceiver and transmitted to the client is preceded by the character CHR(28),

6) each character received by Multipsk and transmitted to the client is preceded by the character CHR(29),

7) each command string transmitted by the server to the client must be preceded by the character CHR(30) and finished by CHR(31),

8) all commands (written in readable text ) will have an answer (see further for details),

9) each server comment (Call ID or RS ID reception, switch to RX or to TX) string transmitted by the server to the client must be preceded by a string: "CHR(23)RX CALL ID=", "CHR(23)RX RS ID=", "CHR(23)SWITCH=RX", "CHR(23) SWITCH=TX",  and finished by CHR(24).

10) each server command, for the transceiver control, transmitted by the server to the client must be preceded by the string "CHR(23) XCVR=" and finished by CHR(24).

Data

End of TX] ARQ FAE CQ[End of TX] ARQ FAE CQ[End of TX] call "THIS I[End of TX] end of link to GM8BPQ[End of TX] sounding "THIS WAS"[End of TX] ARQ FAE CQ[End of TX] ARQ FAE CQ[End of TX] ARQ FAE CQ[End of TX] ARQ FAE CQFAE BEACON OH5RM Kouvola KP30JR
[End of TX] ARQ FAE selective callGM8BPQ DE OH5RM 

[Connection made with OH5RM]


18103 but I have to go out to change antenna

[End of connection with OH5RM]FAE BEACON OH5RM Kouvola KP30JR
S" to GM8BPQ

10:23:55 AM Comment: SWITCH=RX
10:24:00 AM Comment: RX RS ID=10:24:00 UTC  ALE400 1609 Hz 0 MHz
10:24:19 AM Comment: RX RS ID=10:24:19 UTC  ALE400 1604 Hz 0 MHz
10:25:04 AM Comment: SWITCH=TX
10:25:07 AM Comment: SWITCH=RX
10:25:15 AM Comment: SWITCH=TX
:30:22 AM Comment: SWITCH=RX
10:30:25 AM Comment: SWITCH=TX
10:30:27 AM Comment: SWITCH=RX
10:30:35 AM Comment: RX RS ID=10:30:35 UTC  ALE400 1598 Hz 0 MHz


*/

	// Reuse the HAL CMD and Data Buffers to build messages from TCP stream

	// See if sequence split over a packet boundary

	if (TNC->CmdEsc == 23)
	{
		TNC->CmdEsc = 0;
		goto CommentEsc;
	}

	if (TNC->CmdEsc == 29)
	{
		TNC->CmdEsc = 0;
		goto DataEsc;
	}

	if (TNC->CmdEsc == 30)
	{
		TNC->CmdEsc = 0;
		goto CmdEsc;
	}

	// No Split

	while(Len)
	{
		switch (*(MPTR++))
		{
		case 29:				// Data Char

			Len--;
		DataEsc:		
			if (Len)
			{
				TNC->DataBuffer[TNC->DataLen++] = *MPTR;
				MPTR++;
				Len--;
				goto OuterLoop;
			}

			TNC->CmdEsc = 29;
	
			if (TNC->DataLen)
				ProcessMSPKData(TNC);


			return;					// Nothing left

		case 30:

			Len --;
		CmdEsc:			
			while (Len)
			{
				if (*MPTR == 31)	// End of String
				{
					ProcessMSPKCmd(TNC);
					TNC->CmdLen = 0;

					// Process any data left in buffer
					
					MPTR++;
					Len--;
					goto OuterLoop;
				}
			
				TNC->CmdBuffer[TNC->CmdLen++] = *MPTR;
				MPTR++;
				Len--;
			}

			TNC->CmdEsc = 30;
			return;					// Nothing left
	
		case 23:					// Server Comment

			Len --;
		CommentEsc:			
			while (Len)
			{
				if (*MPTR == 24)	// End of String
				{
					// Process Comment

					ProcessMSPKCmd(TNC);
					TNC->CmdLen = 0;

					// Process any data left in buffer

					MPTR++;
					Len--;
					goto OuterLoop;
				}
			
				TNC->CmdBuffer[TNC->CmdLen++] = *MPTR;
				MPTR++;
				Len--;
			}

			TNC->CmdEsc = 23;
			return;					// Nothing left

		default:

			Len--;

		}
OuterLoop:;
	}

	if (TNC->DataLen)
		ProcessMSPKData(TNC);
}

VOID ProcessMSPKCmd(struct TNCINFO * TNC)
{
	TNC->CmdBuffer[TNC->CmdLen] = 0;

	if (strcmp(TNC->CmdBuffer, "SWITCH=TX") == 0)
		TNC->MPSKInfo->TX = TRUE;
	else
	{
		if (strcmp(TNC->CmdBuffer, "SWITCH=RX") == 0)
		{
			TNC->MPSKInfo->TX = FALSE;
			
			// See if a command was queued while busy
			
			if (TNC->CmdSet)
			{
				send(TNC->WINMORSock, TNC->CmdSet, strlen(TNC->CmdSet), 0);
				free (TNC->CmdSet);
				TNC->CmdSet = NULL;
			}
		}
		else
		{
			Debugprintf("MPSK CMD %s", TNC->CmdBuffer);

			if (TNC->InternalCmd)
			{
				ULONG * buffptr = GetBuff();
				char * ptr = strstr(TNC->CmdBuffer, "OK");

				if (ptr)
					*(ptr+2) = 0;				// Convert OKn to OK for BBS Connect Script

				TNC->InternalCmd = FALSE;

				if (buffptr)
				{
					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "MPSK} %s\r", TNC->CmdBuffer);
					C_Q_ADD(&TNC->Streams[0].PACTORtoBPQ_Q, buffptr);
				}

				if (strstr(TNC->CmdBuffer, "STOP_SELECTIVE_CALL_ARQ_FAE OK"))
					TNC->Streams[0].Connecting = FALSE;

			}
		}
	}
}

VOID ProcessMSPKComment(struct TNCINFO * TNC)
{
	TNC->CmdBuffer[TNC->CmdLen] = 0;
	Debugprintf("MPSK Comment %s", TNC->CmdBuffer);
}

static int UnStuff(UCHAR * inbuff, int len)
{
	int i,txptr=0;
	UCHAR c;
	UCHAR * outbuff = inbuff;

	for (i = 0; i < len; i++)
	{
		c = inbuff[i];

		if (c == 0xc0)
			c = inbuff[++i] - 0x20;

		outbuff[txptr++]=c;
	}

	return txptr;
}

VOID ProcessMSPKData(struct TNCINFO * TNC)
{
	UINT * buffptr;
	int Stream = 0;
	struct STREAMINFO * STREAM = &TNC->Streams[0];
	char * ptr;
	int Len = TNC->DataLen;

	TNC->DataBuffer[TNC->DataLen] = 0;

	// Process Data

	if (STREAM->Connected)
	{
		ptr = strstr(TNC->DataBuffer, "[End of connection");

		if (ptr)
		{
			// Disconnect

			TNC->DataLen = 0;
		
			if (STREAM->DiscWhenAllSent)
				return;						// Already notified

			if (STREAM->Connecting)
			{
				// Report Connect Failed, and drop back to command mode

				STREAM->Connecting = FALSE;
				buffptr = GetBuff();

				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1] = sprintf((UCHAR *)&buffptr[2], "MPSK} Failure with %s\r", STREAM->RemoteCall);

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
				STREAM->DiscWhenAllSent = 10;
			
				return;
			}

			// Release Session

			STREAM->Connecting = FALSE;
			STREAM->Connected = FALSE;		// Back to Command Mode
			STREAM->ReportDISC = TRUE;		// Tell Node

			STREAM->Disconnecting = FALSE;
			STREAM->DiscWhenAllSent = 10;
			STREAM->FramesOutstanding = 0;

			return;
		}

		// Pass to Application. Remove any transparency (hex 0xc0 used as an escape)

		buffptr = GetBuff();

		if (TNC->DataBuffer[TNC->DataLen - 1] == 0xc0)
			return;			// Last char is an escape, so wait for the escaped char to arrive
		
		if (buffptr)
		{
			if (memchr(TNC->DataBuffer, 0xc0, TNC->DataLen))
				TNC->DataLen = UnStuff(TNC->DataBuffer, TNC->DataLen);

			buffptr[1]  = TNC->DataLen;
			memcpy(&buffptr[2], TNC->DataBuffer, TNC->DataLen);

			C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);

			STREAM->BytesRXed += TNC->DataLen;
		}

		TNC->DataLen = 0;
		return;
	}

	// Not Connected. We get various status messages, including Connection made,
	// but they may be split across packets, or have more that one to a packet.
	// I think they are all CR/LF terminated . No they aren't!

	// Look for [] this seems to be what is important

DataLoop:

	if (memcmp(TNC->DataBuffer, "[End of TX] ARQ FAE CQ", 22) == 0)
	{
		// Remove string from buffer

		if (Len == 22)  // Most Likely
		{
			TNC->DataLen = 0;
			return;
		}

		TNC->DataLen -= 22;
		memmove(TNC->DataBuffer, &TNC->DataBuffer[22], Len - 21);  //Copy Null
		Len -= 22;
		goto DataLoop;

	}		

	ptr = strchr(TNC->DataBuffer, '[');
	
	if (ptr)
	{
		// Start of a significant Message

		char * eptr = strchr(TNC->DataBuffer, ']');
		char CallFrom[20];
		char * cptr ;

		if (eptr == 0)
			return;				// wait for matching []

		cptr = strstr(TNC->DataBuffer, "[Connection made with ");

	//	TNC->DataLen -= LineLen;
	//	memmove(TNC->DataBuffer, &TNC->DataBuffer[LineLen], 1 + Len - LineLen);  //Copy Null
	//	Len -= LineLen;
	//	goto DataLoop;


		if (cptr)			// Have a connection
		{

			// Connected

			memcpy(CallFrom, &cptr[22], 18);
			cptr = strchr(CallFrom, ']');
			if (cptr)
				*cptr = 0;

			if (STREAM->Connecting)
			{
				// Connect Complete

				STREAM->Connected = TRUE;
				STREAM->Connecting = FALSE;
				STREAM->ConnectTime = time(NULL); 
				STREAM->BytesRXed = STREAM->BytesTXed = 0;

				buffptr = GetBuff();
				if (buffptr)
				{
					buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Connected to %s\r", CallFrom);
					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
				}				
			}
			else
			{
					// Incoming. Look for a free Stream

					STREAM->Connected = TRUE;
					STREAM->ConnectTime = time(NULL); 
					STREAM->BytesRXed = STREAM->BytesTXed = 0;

					UpdateMH(TNC, CallFrom, '+', 'I');
	
					ProcessIncommingConnect(TNC, CallFrom, Stream, FALSE);
	
					if (HFCTEXTLEN)
					{
						if (HFCTEXTLEN > 1)
							SendData(TNC, HFCTEXT, HFCTEXTLEN);
					}
					else
					{
						if (FULL_CTEXT)
						{
							int Len = CTEXTLEN, CTPaclen = 50;
							int Next = 0;

							while (Len > CTPaclen)		// CTEXT Paclen
							{
								SendData(TNC, &CTEXTMSG[Next], CTPaclen);
								Next += CTPaclen;
								Len -= CTPaclen;
							}
							SendData(TNC, &CTEXTMSG[Next], Len);
						}
					}
				}
		}

	}

	// Doesnt contain [ - just discard
		
	TNC->DataLen = 0;
	Debugprintf(TNC->DataBuffer);
	return;
	
}



/*
		buffptr = GetBuff();
				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1]  = RXHeader->DataLength;
				memcpy(&buffptr[2], Message, RXHeader->DataLength);

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
				return;

		return;


	case 'd':			// Disconnected


	
	case 'C':

        //   Connect. Can be Incoming or Outgoing

		// "*** CONNECTED To Station [CALLSIGN]" When the other station starts the connection
		// "*** CONNECTED With [CALLSIGN]" When we started the connection

 */


VOID SendData(struct TNCINFO * TNC, char * Msg, int MsgLen)
{
	// Preceed each data byte with 25 (decimal)

	char * NewMsg = malloc (MsgLen * 4);
	int n;
	UCHAR c;
	int ExtraLen = 0;
	char * ptr = NewMsg;
	char * inptr = Msg;
	SOCKET sock = TNCInfo[MasterPort[TNC->Port]]->WINMORSock;

	TNC->Streams[0].BytesTXed += MsgLen;

	for (n = 0; n < MsgLen; n++)
	{
		*(ptr++) = 25;
		c = *inptr++;

		if (c < 0x20 || c == 0xc0)
		{
			if (c != 0x0d)
			{
				*ptr++ = 0x0c0;
				*(ptr++) = 25;
				*ptr++ = c + 0x20;
				ExtraLen += 2;
				continue;
			}
		}

		*ptr++ = c;
	}
	
	send(sock, NewMsg, MsgLen * 2 + ExtraLen, 0);

	free(NewMsg);
}

VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
	char Command[80];
	int len;

	len = sprintf(Command,"%cSTOP_SELECTIVE_CALL_ARQ_FAE\x1b", '\x1a');
	if (TNC->MPSKInfo->TX)
		TNC->CmdSet = TNC->CmdSave = _strdup(Command);		// Savde till not transmitting
	else
		send(TNC->WINMORSock, Command, len, 0);
}

VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	TidyClose(TNC, Stream);			// I don't think Hostmode has a DD
}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
	char Cmd[80];
	int Len;

	sprintf(Cmd, "%d SCANSTART 15", TNC->Port);
	Rig_Command(-1, Cmd);

	Cmd[0] = 0;
	
	if (TNC->MPSKInfo->DefaultMode[0])
		sprintf(Cmd, "%cDIGITAL MODE %s\x1b", '\x1a', TNC->MPSKInfo->DefaultMode);

	if (TNC->MPSKInfo->Beacon)
		sprintf(Cmd, "%s%cBEACON_ARQ_FAE\x1b", Cmd, '\x1a');
	
	Len = strlen(Cmd);

	if(Len)
	{
		if (TNC->MPSKInfo->TX)
			TNC->CmdSet = TNC->CmdSave = _strdup(Cmd);		// Savde till not transmitting
		else
			send(TNC->WINMORSock, Cmd, Len, 0);
	}
}

