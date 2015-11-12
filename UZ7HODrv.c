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
//	DLL to provide interface to allow G8BPQ switch to use UZ7HOPE as a Port Driver 
//
//	Uses BPQ EXTERNAL interface
//


#define _CRT_SECURE_NO_DEPRECATE

#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <stdio.h>
#include <time.h>

#include "CHeaders.h"
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

void ConnecttoUZ7HOThread(int port);

void CreateMHWindow();
int Update_MH_List(struct in_addr ipad, char * call, char proto);

int ConnecttoUZ7HO();
static int ProcessReceivedData(int bpqport);
static ProcessLine(char * buf, int Port);
int static KillTNC(struct TNCINFO * TNC);
int static RestartTNC(struct TNCINFO * TNC);
VOID ProcessAGWPacket(struct TNCINFO * TNC, UCHAR * Message);
struct TNCINFO * GetSessionKey(char * key, struct TNCINFO * TNC);
static VOID SendData(struct TNCINFO * TNC, char * key, char * Msg, int MsgLen);
static VOID DoMonitorHddr(struct TNCINFO * TNC, struct AGWHEADER * RXHeader, UCHAR * Msg);
VOID SendRPBeacon(struct TNCINFO * TNC);
VOID MHPROC(struct PORTCONTROL * PORT, MESSAGE * Buffer);

extern UCHAR BPQDirectory[];

#define MAXBPQPORTS 32
#define MAXUZ7HOPORTS 16

//LOGFONT LFTTYFONT ;

//HFONT hFont ;

static int UZ7HOChannel[MAXBPQPORTS+1];			// BPQ Port to UZ7HO Port
static int BPQPort[MAXUZ7HOPORTS][MAXBPQPORTS+1];	// UZ7HO Port and Connection to BPQ Port
static int UZ7HOtoBPQ_Q[MAXBPQPORTS+1];			// Frames for BPQ, indexed by BPQ Port
static int BPQtoUZ7HO_Q[MAXBPQPORTS+1];			// Frames for UZ7HO. indexed by UZ7HO port. Only used it TCP session is blocked

static int MasterPort[MAXBPQPORTS+1];			// Pointer to first BPQ port for a specific UZ7HO host

//	Each port may be on a different machine. We only open one connection to each UZ7HO instance

static char * UZ7HOSignon[MAXBPQPORTS+1];			// Pointer to message for secure signin


static unsigned int UZ7HOInst = 0;
static int AttachedProcesses=0;

static HWND hResWnd,hMHWnd;
static BOOL GotMsg;

static HANDLE STDOUT=0;

//SOCKET sock;

static  struct sockaddr_in  sinx; 
static  struct sockaddr_in  rxaddr;
static  struct sockaddr_in  destaddr[MAXBPQPORTS+1];

static int addrlen=sizeof(sinx);

//static short UZ7HOPort=0;

static time_t ltime,lasttime[MAXBPQPORTS+1];

static BOOL CONNECTING[MAXBPQPORTS+1];
static BOOL CONNECTED[MAXBPQPORTS+1];

//HANDLE hInstance;


static fd_set readfs;
static fd_set writefs;
static fd_set errorfs;
static struct timeval timeout;

unsigned int reverse(unsigned int val)
{
	char x[4];
	char y[4];

	memcpy(x, &val,4);
	y[0] = x[3];
	y[1] = x[2];
	y[2] = x[1];
	y[3] = x[0];

	memcpy(&val, y, 4);

	return val;
}


#ifndef LINBPQ



static BOOL CALLBACK EnumTNCWindowsProc(HWND hwnd, LPARAM  lParam)
{
	char wtext[100];
	struct TNCINFO * TNC = (struct TNCINFO *)lParam; 
	UINT ProcessId;
	char FN[MAX_PATH] = "";
	HANDLE hProc;

	if (TNC->ProgramPath == NULL)
		return FALSE;

	GetWindowText(hwnd,wtext,99);

	if (memcmp(wtext,"Soundmodem", 10) == 0)
	{
		GetWindowThreadProcessId(hwnd, &ProcessId);

//		if (TNC->WIMMORPID == ProcessId)
		{
			 // Our Process

			hProc =  OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ProcessId);

			if (hProc && GetModuleFileNameExPtr)
			{
				GetModuleFileNameExPtr(hProc, NULL, FN, MAX_PATH);

				// Make sure this is the right copy

				CloseHandle(hProc);

				if (_stricmp(FN, TNC->ProgramPath))
					return TRUE;					//Wrong Copy
			}

			TNC->WIMMORPID = ProcessId;

			sprintf (wtext, "Soundmodem - BPQ %s", TNC->PortRecord->PORTCONTROL.PORTDESCRIPTION);
			SetWindowText(hwnd, wtext);
			return FALSE;
		}
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
	struct AGWINFO * AGW;
	int Stream = 0;
	struct STREAMINFO * STREAM;
	int TNCOK;

	if (TNC == NULL)
		return 0;					// Port not defined

	AGW = TNC->AGWInfo;

	// Look for attach on any call

	for (Stream = 0; Stream <= TNC->AGWInfo->MaxSessions; Stream++)
	{
		STREAM = &TNC->Streams[Stream];
	
		if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] && TNC->Streams[Stream].Attached == 0)
		{
			char Cmd[80];

			// New Attach

			int calllen;
			STREAM->Attached = TRUE;

			TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4USER[6] |= 0x60; // Ensure P or T aren't used on ax.25
			calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4USER, STREAM->MyCall);
			STREAM->MyCall[calllen] = 0;
			STREAM->FramesOutstanding = 0;

			// Stop Scanning

			sprintf(Cmd, "%d SCANSTOP", TNC->Port);
			Rig_Command(-1, Cmd);

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
					ConnecttoUZ7HO(port);
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
					if (BPQtoUZ7HO_Q[port] == 0)
					{
						APPLCALLS * APPL;
						char * ApplPtr = APPLS;
						int App;
						char Appl[10];
						char * ptr;

						char NodeCall[11];

						memcpy(NodeCall, MYNODECALL, 10);
						strlop(NodeCall, ' ');

						//	Connect success

						TNC->CONNECTED = TRUE;
						TNC->CONNECTING = FALSE;

						// If required, send signon

						if (UZ7HOSignon[port])
							send(TNC->WINMORSock,UZ7HOSignon[port],546,0);

						// Request Raw Frames

						AGW->TXHeader.Port=0;
						AGW->TXHeader.DataKind='k';		// Raw Frames
						AGW->TXHeader.DataLength=0;
						send(TNC->WINMORSock,(const char FAR *)&AGW->TXHeader,AGWHDDRLEN,0);

						AGW->TXHeader.DataKind='m';		// Monitor Frames
						send(TNC->WINMORSock,(const char FAR *)&AGW->TXHeader,AGWHDDRLEN,0);
		
						// Register all applcalls

						AGW->TXHeader.DataKind='X';		// Register
						memset(AGW->TXHeader.callfrom, 0, 10);
						strcpy(AGW->TXHeader.callfrom, TNC->NodeCall);
						send(TNC->WINMORSock,(const char FAR *)&AGW->TXHeader,AGWHDDRLEN,0);
					
						memset(AGW->TXHeader.callfrom, 0, 10);
						strcpy(AGW->TXHeader.callfrom, NodeCall);
						send(TNC->WINMORSock,(const char FAR *)&AGW->TXHeader,AGWHDDRLEN,0);

						for (App = 0; App < 32; App++)
						{
							APPL=&APPLCALLTABLE[App];
							memcpy(Appl, APPL->APPLCALL_TEXT, 10);
							ptr=strchr(Appl, ' ');

							if (ptr)
								*ptr = 0;

							if (Appl[0])
							{
								memset(AGW->TXHeader.callfrom, 0, 10);
								strcpy(AGW->TXHeader.callfrom, Appl);
								send(TNC->WINMORSock,(const char FAR *)&AGW->TXHeader,AGWHDDRLEN,0);
							}
						}
#ifndef LINBPQ
						EnumWindows(EnumTNCWindowsProc, (LPARAM)TNC);
#endif
					}
					else
					{
						// Write block has cleared. Send rest of packet

						buffptr=Q_REM(&BPQtoUZ7HO_Q[port]);

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
//						i=sprintf(ErrMsg, "UZ7HO Connection lost for BPQ Port %d\r\n", port);
//						WritetoConsole(ErrMsg);
//					}

					CONNECTING[port]=FALSE;
					CONNECTED[port]=FALSE;
				
				}

			}

		}

		// See if any frames for this port

		for (Stream = 0; Stream <= TNC->AGWInfo->MaxSessions; Stream++)
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
						buffptr[1] = sprintf((UCHAR *)&buffptr[2], "UZ7HO} Failure with %s\r", STREAM->RemoteCall);
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

			if (STREAM->Connected && STREAM->FramesOutstanding)
			{
				struct AGWINFO * AGW = TNC->AGWInfo;

				AGW->PollDelay++;

				if (AGW->PollDelay > 10)
				{
					char * Key = &STREAM->AGWKey[0];
				
					AGW->PollDelay = 0;

					AGW->TXHeader.Port = Key[0] - '1';
					AGW->TXHeader.DataKind='Y';
					strcpy(AGW->TXHeader.callfrom, &Key[11]);
					strcpy(AGW->TXHeader.callto, &Key[1]);
					AGW->TXHeader.DataLength = 0;

					send(TNCInfo[MasterPort[port]]->WINMORSock, (char *)&AGW->TXHeader, AGWHDDRLEN, 0);
				}
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

	//			buff[5]=(datalen & 0xff);
	//			buff[6]=(datalen >> 8);
				
				ReleaseBuffer(buffptr);
	
				return (1);
			}
		}

		if (TNC->PortRecord->UI_Q)
		{
			struct AGWINFO * AGW = TNC->AGWInfo;
	
			int MsgLen;
			struct _MESSAGE * buffptr;
			char * Buffer;
			SOCKET Sock;	
			buffptr = Q_REM(&TNC->PortRecord->UI_Q);

			Sock = TNCInfo[MasterPort[port]]->WINMORSock;
		
			MsgLen = buffptr->LENGTH - 6;	// 7 Header, need extra Null
			buffptr->LENGTH = 0;				// Need a NULL on front	
			Buffer = &buffptr->DEST[0];		// Raw Frame
			Buffer--;						// Need to send an extra byte on front
	
			AGW->TXHeader.Port = UZ7HOChannel[port];
			AGW->TXHeader.DataKind = 'K';
			memset(AGW->TXHeader.callfrom, 0, 10);
			memset(AGW->TXHeader.callto, 0, 10);
#ifdef __BIG_ENDIAN__
			AGW->TXHeader.DataLength = reverse(MsgLen);
#else
			AGW->TXHeader.DataLength = MsgLen;
#endif
			send(Sock, (char *)&AGW->TXHeader, AGWHDDRLEN, 0);
			send(Sock, Buffer, MsgLen, 0);

			ReleaseBuffer((UINT *)buffptr);
		}
			
	
		return (0);



	case 2:				// send
	
		if (!TNCInfo[MasterPort[port]]->CONNECTED) return 0;		// Don't try if not connected to TNC

		Stream = buff[4];
		
		STREAM = &TNC->Streams[Stream]; 
		AGW = TNC->AGWInfo;

//		txlen=(buff[6]<<8) + buff[5] - 8;	

		txlen = GetLengthfromBuffer(buff) - 8;
			
		if (STREAM->Connected)
		{
			SendData(TNC, &STREAM->AGWKey[0], &buff[8], txlen);
			STREAM->FramesOutstanding++;
		}
		else
		{
			if (_memicmp(&buff[8], "D\r", 2) == 0)
			{
				TidyClose(TNC, buff[4]);
				STREAM->ReportDISC = TRUE;		// Tell Node
				return 0;
			}

			if (STREAM->Connecting)
				return 0;

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

			if (_memicmp(&buff[8], "INUSE?", 6) == 0)
			{
				// Return Error if in use, OK if not

				UINT * buffptr = GetBuff();
				int s = 0;

				while(s <= TNC->AGWInfo->MaxSessions)
				{
					if (s != Stream)
					{		
						if (TNC->PortRecord->ATTACHEDSESSIONS[s])
						{
							buffptr[1] = sprintf((UCHAR *)&buffptr[2], "UZ7HO} Error - In use\r");
							C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
							return 1;							// Busy
						}
					}
					s++;
				}
				buffptr[1] = sprintf((UCHAR *)&buffptr[2], "UZ7HO} Ok - Not in use\r");
				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
			
				return 1;
			}

			// See if a Connect Command.

			if (toupper(buff[8]) == 'C' && buff[9] == ' ' && txlen > 2)	// Connect
			{
				struct AGWINFO * AGW = TNC->AGWInfo;
				char ViaList[82] = "";
				int Digis = 0;
				char * viaptr;
				char * ptr;
				char * context;
				int S;
				struct STREAMINFO * TSTREAM;
				char Key[21];

				_strupr(&buff[8]);
				buff[8 + txlen] = 0;

				memset(STREAM->RemoteCall, 0, 10);

				// See if any digis - accept V VIA or nothing, seps space or comma

				ptr = strtok_s(&buff[10], " ,\r", &context);
				strcpy(STREAM->RemoteCall, ptr);
	
				Key[0] = UZ7HOChannel[port] + '1';
				memset(&Key[1], 0, 20);
				strcpy(&Key[11], STREAM->MyCall);
				strcpy(&Key[1], ptr);

				// Make sure we don't already have a session for this station

				S = 0;

				while (S <= AGW->MaxSessions)
				{
					TSTREAM = &TNC->Streams[S];

					if (memcmp(TSTREAM->AGWKey, Key, 21) == 0)
					{
						// Found it;

						UINT * buffptr = GetBuff();
						buffptr[1] = sprintf((UCHAR *)&buffptr[2],
							"UZ7HO} Sorry - Session between %s and %s already Exists\r",
							STREAM->MyCall, STREAM->RemoteCall);

						C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
						STREAM->DiscWhenAllSent = 10;
			
						return 0;
					}
					S++;
				}		
			
				// Not Found

				memcpy(&STREAM->AGWKey[0], &Key[0], 21);

				AGW->TXHeader.Port = UZ7HOChannel[port];
				AGW->TXHeader.DataKind='C';
				memcpy(AGW->TXHeader.callfrom, &STREAM->AGWKey[11], 10);
				memcpy(AGW->TXHeader.callto, &STREAM->AGWKey[1], 10);
				AGW->TXHeader.DataLength = 0;

				ptr = strtok_s(NULL, " ,\r", &context);

				if (ptr)
				{
					// we have digis

					viaptr = &ViaList[1];
		
					if (strcmp(ptr, "V") == 0 || strcmp(ptr, "VIA") == 0)
						ptr = strtok_s(NULL, " ,\r", &context);

					while (ptr)
					{
						strcpy(viaptr, ptr);
						Digis++;
						viaptr += 10;
						ptr = strtok_s(NULL, " ,\r", &context);
					}

#ifdef __BIG_ENDIAN__
					AGW->TXHeader.DataLength = reverse(Digis * 10 + 1);
#else
					AGW->TXHeader.DataLength = Digis * 10 + 1;
#endif

					AGW->TXHeader.DataKind='v';
					ViaList[0] = Digis;
				}

				send(TNCInfo[MasterPort[port]]->WINMORSock, (char *)&AGW->TXHeader, AGWHDDRLEN, 0);
				if (Digis)
					send(TNCInfo[MasterPort[port]]->WINMORSock, ViaList, Digis * 10 + 1, 0);

				STREAM->Connecting = TNC->AGWInfo->ConnTimeOut;	// It doesn't report failure

//				sprintf(Status, "%s Connecting to %s", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
//				SetDlgItemText(TNC->hDlg, IDC_TNCSTATE, Status);
			}
		}

/*

		
		AGW->TXHeader.Port=UZ7HOChannel[port];
		AGW->TXHeader.DataKind='K';				// raw send
		AGW->TXHeader.DataLength=txlen;

		memcpy(&txbuff, &AGW->TXHeader, AGWHDDRLEN);
		memcpy(&txbuff[AGWHDDRLEN],&buff[6],txlen);

		txbuff[AGWHDDRLEN] = 0;
		txlen+=AGWHDDRLEN;

		bytes=send(TNC->WINMORSock,(const char FAR *)&txbuff,txlen,0);
		
		if (bytes != txlen)
		{

			// UZ7HO doesn't seem to recover from a blocked write. For now just reset
			
//			if (bytes == SOCKET_ERROR)
//			{
				winerr=WSAGetLastError();
				
				i=sprintf(ErrMsg, "UZ7HO Write Failed for port %d - error code = %d\r\n", port, winerr);
				WritetoConsole(ErrMsg);
					
	
//				if (winerr != WSAEWOULDBLOCK)
//				{
				
					closesocket(TNC->WINMORSock);
					
					TNC->CONNECTED = FALSE;

					return (0);
//				}
//				else
//				{
//					bytes=0;		// resent whole packet
//				}

//			}

			// Partial Send or WSAEWOULDBLOCK. Save data, and send once busy clears

			
			// Get a buffer
						
//			buffptr=GetBuff();

//			if (buffptr == 0)
//			{
				// No buffers, so can only break connection and try again

//				closesocket(UZ7HOSock[MasterPort[port]]);
					
//				CONNECTED[MasterPort[port]]=FALSE;

//				return (0);
//			}
	
//			buffptr[1]=txlen-bytes;			// Bytes still to send

//			memcpy(buffptr+2,&txbuff[bytes],txlen-bytes);

//			C_Q_ADD(&BPQtoUZ7HO_Q[MasterPort[port]],buffptr);
*/	
			return (0);
		

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

static int KillTNC(struct TNCINFO * TNC)
{
#ifndef LINBPQ
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
#endif
	return 0;
}


static int RestartTNC(struct TNCINFO * TNC)
{
#ifndef LINBPQ
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
		ret = CreateProcess(TNC->ProgramPath, NULL, NULL, NULL, FALSE,0 ,NULL ,HomeDir, &SInfo, &PInfo);

		if (ret)
			TNC->WIMMORPID = PInfo.dwProcessId;

		return ret;
	}
#endif
	return 0;
}


UINT UZ7HOExtInit(EXTPORTDATA * PortEntry)
{
	int i, port;
	char Msg[255];
	struct TNCINFO * TNC;
	char * ptr;

	//
	//	Will be called once for each UZ7HO port to be mapped to a BPQ Port
	//	The UZ7HO port number is in CHANNEL - A=0, B=1 etc
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
	PortEntry->PORTCONTROL.UICAPABLE = 1;
	PortEntry->PORTCONTROL.PORTQUALITY = 0;
	PortEntry->PERMITGATEWAY = TRUE;					// Can change ax.25 call on each stream
	PortEntry->SCANCAPABILITIES = NONE;					// Scan Control - pending connect only

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0)
		PortEntry->PORTCONTROL.PORTPACLEN = 64;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	TNC->Hardware = H_UZ7HO;

	UZ7HOChannel[port] = PortEntry->PORTCONTROL.CHANNELNUM-65;
	
	PortEntry->MAXHOSTMODESESSIONS = TNC->AGWInfo->MaxSessions;	

	i=sprintf(Msg,"UZ7HO Host %s Port %d Chan %c\n",
		TNC->WINMORHostName, TNC->WINMORPort, PortEntry->PORTCONTROL.CHANNELNUM);
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
			
	if (MasterPort[port] == port)
	{
#ifndef LINBPQ
		if (EnumWindows(EnumTNCWindowsProc, (LPARAM)TNC))
			if (TNC->ProgramPath)
				TNC->WeStartedTNC = RestartTNC(TNC);
#endif
		ConnecttoUZ7HO(port);
	}

	time(&lasttime[port]);			// Get initial time value
	
	return ((int) ExtProc);

}

/*

#	Config file for BPQtoUZ7HO
#
#	For each UZ7HO port defined in BPQCFG.TXT, Add a line here
#	Format is BPQ Port, Host/IP Address, Port

#
#	Any unspecified Ports will use 127.0.0.1 and port for BPQCFG.TXT IOADDR field
#

1 127.0.0.1 8000
2 127.0.0.1 8001

*/


static ProcessLine(char * buf, int Port)
{
	UCHAR * ptr,* p_cmd;
	char * p_ipad = 0;
	char * p_port = 0;
	unsigned short WINMORport = 0;
	int BPQport;
	int len=510;
	struct TNCINFO * TNC;
	struct AGWINFO * AGW;

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
	AGW = TNC->AGWInfo = zalloc(sizeof(struct AGWINFO)); // AGW Sream Mode Specific Data

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
			
			if (_memicmp(buf, "MAXSESSIONS", 11) == 0)
			{
				AGW->MaxSessions = atoi(&buf[12]);
				if (AGW->MaxSessions > 26 ) AGW->MaxSessions = 26;
			}
			if (_memicmp(buf, "CONTIMEOUT", 10) == 0)
				AGW->ConnTimeOut = atoi(&buf[11]) * 10;
			else
			if (_memicmp(buf, "UPDATEMAP", 9) == 0)
				TNC->PktUpdateMap = TRUE;
			else
			if (_memicmp(buf, "BEACONAFTERSESSION", 18) == 0) // Send Beacon after each session 
				TNC->RPBEACON = TRUE;
			else
				
//			if (_memicmp(buf, "WL2KREPORT", 10) == 0)
//				DecodeWL2KReportLine(TNC, buf, NARROWMODE, WIDEMODE);
//			else

			strcat (TNC->InitScript, buf);
		}


	return (TRUE);	
}

int ConnecttoUZ7HO(int port)
{

	_beginthread(ConnecttoUZ7HOThread,0,port);
	return 0;
}

VOID ConnecttoUZ7HOThread(port)
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
		i=sprintf(Msg, "Socket Failed for UZ7HO socket - error code = %d\n", WSAGetLastError());
		WritetoConsole(Msg);

  	 	return; 
	}
 
	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	TNC->CONNECTING = TRUE;

	if (connect(TNC->WINMORSock,(struct sockaddr *) &TNC->destaddr,sizeof(TNC->destaddr)) == 0)
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
   			i=sprintf(Msg, "Connect Failed for UZ7HO socket - error code = %d\n", err);
			WritetoConsole(Msg);

			TNC->Alerted = TRUE;
		}
		
		TNC->CONNECTING = FALSE;
		return;
	}

	TNC->LastFreq = 0;			//	so V4 display will be updated

	return;

}

static int ProcessReceivedData(int port)
{
	unsigned int bytes;
	int datalen,i;
	char ErrMsg[255];
	char Message[1000];
	struct TNCINFO * TNC = TNCInfo[port];
	struct AGWINFO * AGW = TNC->AGWInfo;
	struct TNCINFO * SaveTNC;

	//	Need to extract messages from byte stream

	//	Use MSG_PEEK to ensure whole message is available

	bytes = recv(TNC->WINMORSock, (char *) &AGW->RXHeader, AGWHDDRLEN, MSG_PEEK);

	if (bytes == SOCKET_ERROR)
	{
//		i=sprintf(ErrMsg, "Read Failed for UZ7HO socket - error code = %d\r\n", WSAGetLastError());
//		WritetoConsole(ErrMsg);
				
		closesocket(TNC->WINMORSock);
					
		TNC->CONNECTED = FALSE;

		return (0);
	}

	if (bytes == 0)
	{
		//	zero bytes means connection closed

		i=sprintf(ErrMsg, "UZ7HO Connection closed for BPQ Port %d\n", port);
		WritetoConsole(ErrMsg);

		TNC->CONNECTED = FALSE;
		return (0);
	}

	//	Have some data
	
	if (bytes == AGWHDDRLEN)
	{
		//	Have a header - see if we have any associated data
		
		datalen = AGW->RXHeader.DataLength;

#ifdef __BIG_ENDIAN__
		datalen = reverse(datalen);
#endif
		if (datalen > 0)
		{
			// Need data - See if enough there
				
			bytes = recv(TNC->WINMORSock, (char *)&Message, AGWHDDRLEN + datalen, MSG_PEEK);
		}

		if (bytes == AGWHDDRLEN + datalen)
		{
			bytes = recv(TNC->WINMORSock, (char *)&AGW->RXHeader, AGWHDDRLEN,0);

			if (datalen > 0)
			{
				bytes = recv(TNC->WINMORSock,(char *)&Message, datalen,0);
			}

			// Have header, and data if needed

			SaveTNC = TNC;
			ProcessAGWPacket(TNC, Message);			// Data may be for another port
			TNC = SaveTNC;

			return (0);
		}

		// Have header, but not sufficient data

		return (0);
	
	}

	// Dont have at least header bytes
	
	return (0);

}
/*
VOID ConnecttoMODEMThread(port);

int ConnecttoMODEM(int port)
{
	_beginthread(ConnecttoMODEMThread,0,port);

	return 0;
}

VOID ConnecttoMODEMThread(port)
{
	char Msg[255];
	int err,i;
	u_long param=1;
	BOOL bcopt=TRUE;
	struct hostent * HostEnt;
	struct TNCINFO * TNC = TNCInfo[port];

	Sleep(5000);		// Allow init to complete 

	TNC->destaddr.sin_addr.s_addr = inet_addr(TNC->WINMORHostName);
	TNC->Datadestaddr.sin_addr.s_addr = inet_addr(TNC->WINMORHostName);

	if (TNC->destaddr.sin_addr.s_addr == INADDR_NONE)
	{
		//	Resolve name to address

		 HostEnt = gethostbyname(TNC->WINMORHostName);
		 
		 if (!HostEnt) return;			// Resolve failed

		 memcpy(&TNC->destaddr.sin_addr.s_addr,HostEnt->h_addr,4);
		 memcpy(&TNC->Datadestaddr.sin_addr.s_addr,HostEnt->h_addr,4);

	}

	closesocket(TNC->WINMORSock);
	closesocket(TNC->WINMORDataSock);

	TNC->WINMORSock=socket(AF_INET,SOCK_STREAM,0);
	TNC->WINMORDataSock=socket(AF_INET,SOCK_STREAM,0);

	if (TNC->WINMORSock == INVALID_SOCKET || TNC->WINMORDataSock == INVALID_SOCKET)
	{
		i=sprintf(Msg, "Socket Failed for UZ7HO socket - error code = %d\n", WSAGetLastError());
		WritetoConsole(Msg);

  	 	return; 
	}
 
	setsockopt (TNC->WINMORDataSock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt, 4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (bind(TNC->WINMORSock, (LPSOCKADDR) &sinx, addrlen) != 0 )
	{
		//
		//	Bind Failed
		//
	
		i=sprintf(Msg, "Bind Failed for UZ7HO socket - error code = %d\n", WSAGetLastError());
		WritetoConsole(Msg);

  	 	return; 
	}

	TNC->CONNECTING = TRUE;

	if (connect(TNC->WINMORSock,(LPSOCKADDR) &TNC->destaddr,sizeof(TNC->destaddr)) == 0)
	{
		//
		//	Connected successful
		//

		TNC->CONNECTED=TRUE;
		SetDlgItemText(TNC->hDlg, IDC_COMMSSTATE, "Connected to UZ7HO TNC");
	}
	else
	{
		if (TNC->Alerted == FALSE)
		{
			err=WSAGetLastError();
   			i=sprintf(Msg, "Connect Failed for UZ7HO socket - error code = %d\n", err);
			WritetoConsole(Msg);
			SetDlgItemText(TNC->hDlg, IDC_COMMSSTATE, "Connection to TNC failed");

			TNC->Alerted = TRUE;
		}
		
		TNC->CONNECTING = FALSE;
		return;
	}

	TNC->LastFreq = 0;			//	so V4 display will be updated

	return;
}
*/
/*
UZ7HO C GM8BPQ GM8BPQ-2 *** CONNECTED To Station GM8BPQ-0

UZ7HO D GM8BPQ GM8BPQ-2 asasasas
M8BPQ
New Disconnect Port 7 Q 0
UZ7HO d GM8BPQ GM8BPQ-2 *** DISCONNECTED From Station GM8BPQ-0

New Disconnect Port 7 Q 0
*/

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

extern VOID PROCESSUZ7HONODEMESSAGE();

VOID ProcessAGWPacket(struct TNCINFO * TNC, UCHAR * Message)
{
	UINT * buffptr;
	MESSAGEY Monframe;

 	struct AGWINFO * AGW = TNC->AGWInfo;
	struct AGWHEADER * RXHeader = &AGW->RXHeader;
	char Key[21];
	int Stream;
	struct STREAMINFO * STREAM;
	UCHAR AGWPort;

#ifdef __BIG_ENDIAN__
	RXHeader->DataLength = reverse(RXHeader->DataLength);
#endif

	switch (RXHeader->DataKind)
	{
	case 'D':			// Appl Data

		TNC = GetSessionKey(Key, TNC);
	
		if (TNC == NULL)
			return;

		// Find our Session

		Stream = 0;

		while (Stream <= AGW->MaxSessions)
		{
			STREAM = &TNC->Streams[Stream];

			if (memcmp(STREAM->AGWKey, Key, 21) == 0)
			{
				// Found it;

				buffptr = GetBuff();
				if (buffptr == 0) return;			// No buffers, so ignore

				buffptr[1]  = RXHeader->DataLength;
				memcpy(&buffptr[2], Message, RXHeader->DataLength);

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
				return;
			}
			Stream++;
		}		
			
		// Not Found

		return;


	case 'd':			// Disconnected

		TNC = GetSessionKey(Key, TNC);

		if (TNC == NULL)
			return;

		// Find our Session

		Stream = 0;

		while (Stream <= AGW->MaxSessions)
		{
			STREAM = &TNC->Streams[Stream];

			if (memcmp(STREAM->AGWKey, Key, 21) == 0)
			{
				// Found it;

				if (STREAM->DiscWhenAllSent)
					return;						// Already notified

				if (STREAM->Connecting)
				{
					// Report Connect Failed, and drop back to command mode

					STREAM->Connecting = FALSE;
					buffptr = GetBuff();

					if (buffptr == 0) return;			// No buffers, so ignore

					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "UZ7HO} Failure with %s\r", STREAM->RemoteCall);

					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
					STREAM->DiscWhenAllSent = 10;

					if (TNC->RPBEACON)
						SendRPBeacon(TNC);
			
					return;
				}

				// Release Session

				STREAM->Connecting = FALSE;
				STREAM->Connected = FALSE;		// Back to Command Mode
				STREAM->ReportDISC = TRUE;		// Tell Node

		//		if (STREAM->Disconnecting)		// 
		//			ReleaseTNC(TNC);

				STREAM->Disconnecting = FALSE;
				STREAM->DiscWhenAllSent = 10;
				STREAM->FramesOutstanding = 0;

				if (TNC->RPBEACON)
					SendRPBeacon(TNC);

				return;
			}
			Stream++;
		}

		return;

	case 'C':

        //   Connect. Can be Incoming or Outgoing

		// "*** CONNECTED To Station [CALLSIGN]" When the other station starts the connection
		// "*** CONNECTED With [CALLSIGN]" When we started the connection

        //   Create Session Key from port and callsign pair

		TNC = GetSessionKey(Key, TNC);

		if (TNC == NULL)
			return;

		if (strstr(Message, " To Station"))
		{
			// Incoming. Look for a free Stream

			Stream = 1;

			while(Stream <= AGW->MaxSessions)
			{
				if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] == 0)
					goto GotStream;

				Stream++;
			}

			// No free streams - send Disconnect

			return;

	GotStream:

			STREAM = &TNC->Streams[Stream];
			memcpy(STREAM->AGWKey, Key, 21);
			STREAM->Connected = TRUE;
			STREAM->ConnectTime = time(NULL); 
			STREAM->BytesRXed = STREAM->BytesTXed = 0;

			UpdateMH(TNC, RXHeader->callfrom, '+', 'I');

			ProcessIncommingConnect(TNC, RXHeader->callfrom, Stream, FALSE);

			if (HFCTEXTLEN)
			{
				if (HFCTEXTLEN > 1)
					SendData(TNC, &STREAM->AGWKey[0], HFCTEXT, HFCTEXTLEN);
			}
			else
			{
				if (FULL_CTEXT)
				{
					int Len = CTEXTLEN, CTPaclen = 50;
					int Next = 0;

					while (Len > CTPaclen)		// CTEXT Paclen
					{
						SendData(TNC, &STREAM->AGWKey[0], &CTEXTMSG[Next], CTPaclen);
						Next += CTPaclen;
						Len -= CTPaclen;
					}
					SendData(TNC, &STREAM->AGWKey[0], &CTEXTMSG[Next], Len);
				}
			}

			if (strcmp(RXHeader->callto, TNC->NodeCall) != 0)		// Not Connect to Node Call
			{
				APPLCALLS * APPL;
				char * ApplPtr = APPLS;
				int App;
				char Appl[10];
				char * ptr;
				char Buffer[80];				// Data portion of frame

				for (App = 0; App < 32; App++)
				{
					APPL=&APPLCALLTABLE[App];
					memcpy(Appl, APPL->APPLCALL_TEXT, 10);
					ptr=strchr(Appl, ' ');

					if (ptr)
						*ptr = 0;
	
					if (_stricmp(RXHeader->callto, Appl) == 0)
						break;
				}

				if (App < 32)
				{
					char AppName[13];

					memcpy(AppName, &ApplPtr[App * sizeof(CMDX)], 12);
					AppName[12] = 0;

					// Make sure app is available

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
					
						SendData(TNC, Key, Msg, strlen(Msg));

						STREAM->DiscWhenAllSent = 100;	// 10 secs
					}
					return;
				}
			}
		
			// Not to a known appl - drop through to Node

			return;
		}
		else
		{
			// Connect Complete

			// Find our Session

			Stream = 0;

			while (Stream <= AGW->MaxSessions)
			{
				STREAM = &TNC->Streams[Stream];

				if (memcmp(STREAM->AGWKey, Key, 21) == 0)
				{
					// Found it;

					STREAM->Connected = TRUE;
					STREAM->Connecting = FALSE;
					STREAM->ConnectTime = time(NULL); 
					STREAM->BytesRXed = STREAM->BytesTXed = 0;

					buffptr = GetBuff();
					if (buffptr == 0) return;			// No buffers, so ignore

					buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Connected to %s\r", RXHeader->callfrom);

					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
					return;
				}
				Stream++;
			}		
			
			// Not Found

			return;
		}

	case 'T':				// Trasmitted Dats

		DoMonitorHddr(TNC, RXHeader, Message);

		return;

	case 'S':				// Monitored Supervisory

		// Check for SABM

		if (strstr(Message, "<SABM") == 0 && strstr(Message, "<UA") == 0)
			return;

		// Drop through

	case 'U':

		AGWPort = Message[1];

		if (AGWPort < ' ' || AGWPort > '8')
			return;

		AGWPort = BPQPort[AGWPort - '1'][TNC->Port];

		TNC = TNCInfo[AGWPort];

		if (TNC == NULL)
			return;

		strlop(Message, '<');

//		if (strchr(Message, '*'))
//			UpdateMH(TNC, RXHeader->callfrom, '*', 0);
//		else
//			UpdateMH(TNC, RXHeader->callfrom, ' ', 0);
		
		return;

	case 'K':				// raw data	

		Monframe.PORT = BPQPort[RXHeader->Port][TNC->Port];

		if (Monframe.PORT == 0)		// Unused UZ7HO port?
			return;
		
		Monframe.LENGTH = RXHeader->DataLength + 6;

		memcpy(&Monframe.DEST[0], &Message[1], RXHeader->DataLength);

/*		// if NETROM is enabled, and it is a NODES broadcast, process it

		if (TNC->PortRecord->PORTCONTROL.PORTQUALITY)
		{
			int i;
			char * fiddle;
			
			if (Message[15] == 3 && Message[16] == 0xcf && Message[17] == 255)
				i = 0;

			_asm
			{
				pushad

				mov al, Monframe.PORT
				lea edi, Monframe

				call PROCESSUZ7HONODEMESSAGE

				popad
			}
		}
*/
		// Pass to Monitor

		time(&Monframe.Timestamp);
		MHPROC(&TNC->PortRecord->PORTCONTROL, (MESSAGE *)&Monframe);
		BPQTRACE((MESSAGE *)&Monframe, TRUE);

		return;

	case 'I':
		break;

	case 'X':
		break;

	case 'Y':				// Session Queue

		AGWPort = RXHeader->Port;
		Key[0] = AGWPort + '1'; 
        
		memset(&Key[1], 0, 20);
		strcpy(&Key[11], RXHeader->callfrom);		// Wrong way round for GetSessionKey
		strcpy(&Key[1], RXHeader->callto);

		// Need to get BPQ Port from AGW Port

		if (AGWPort > 8)
			return;

		AGWPort = BPQPort[AGWPort][TNC->Port];
		
		TNC = TNCInfo[AGWPort];

		if (TNC == NULL)
			return;

//		Debugprintf("UZ7HO Port %d %d %c %s %s %d", TNC->Port, RXHeader->Port,
//			RXHeader->DataKind, RXHeader->callfrom, RXHeader->callto, Message[0]); 

		Stream = 0;

		while (Stream <= AGW->MaxSessions)
		{
			STREAM = &TNC->Streams[Stream];

			if (memcmp(STREAM->AGWKey, Key, 21) == 0)
			{
				// Found it;

				memcpy(&STREAM->FramesOutstanding, Message, 4);

				if (STREAM->FramesOutstanding == 0)			// All Acked
					if (STREAM->Disconnecting && STREAM->BPQtoPACTOR_Q == 0)
						TidyClose(TNC, 0);

				return;
			}
			Stream++;
		}		
			
		// Not Found

		return;

	default:

		Debugprintf("UZ7HO Port %d %c %s %s %s %d", TNC->Port, RXHeader->DataKind, RXHeader->callfrom, RXHeader->callto, Message, Message[0]); 

		return;
	}
}
struct TNCINFO * GetSessionKey(char * key, struct TNCINFO * TNC)
{
	struct AGWINFO * AGW = TNC->AGWInfo;
	struct AGWHEADER * RXHeader = &AGW->RXHeader;
	int AGWPort;

//   Create Session Key from port and callsign pair
        
	AGWPort = RXHeader->Port;
	key[0] = AGWPort + '1'; 
        
	memset(&key[1], 0, 20);
	strcpy(&key[1], RXHeader->callfrom);
	strcpy(&key[11], RXHeader->callto);

	// Need to get BPQ Port from AGW Port

	if (AGWPort > 8)
		return 0;

	AGWPort = BPQPort[AGWPort][TNC->Port];
	TNC = TNCInfo[AGWPort];
	return TNC;
}

/*
Port field is the port where we want the data to tx
DataKind field =MAKELONG('D',0); The ASCII value of letter D
CallFrom is our call
CallTo is the call of the other station
DataLen is the length of the data that follow
*/

VOID SendData(struct TNCINFO * TNC, char * Key, char * Msg, int MsgLen)
{
	struct AGWINFO * AGW = TNC->AGWInfo;
	SOCKET sock = TNCInfo[MasterPort[TNC->Port]]->WINMORSock;
	
	AGW->TXHeader.Port = Key[0] - '1';
	AGW->TXHeader.DataKind='D';
	memcpy(AGW->TXHeader.callfrom, &Key[11], 10);
	memcpy(AGW->TXHeader.callto, &Key[1], 10);
#ifdef __BIG_ENDIAN__
	AGW->TXHeader.DataLength = reverse(MsgLen);
#else
	AGW->TXHeader.DataLength = MsgLen;
#endif

	send(sock, (char *)&AGW->TXHeader, AGWHDDRLEN, 0);
	send(sock, Msg, MsgLen, 0);
}

VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
	char * Key = &TNC->Streams[Stream].AGWKey[0];
	struct AGWINFO * AGW = TNC->AGWInfo;
	
	AGW->TXHeader.Port = Key[0] - '1';
	AGW->TXHeader.DataKind='d';
	strcpy(AGW->TXHeader.callfrom, &Key[11]);
	strcpy(AGW->TXHeader.callto, &Key[1]);
	AGW->TXHeader.DataLength = 0;

	send(TNCInfo[MasterPort[TNC->Port]]->WINMORSock, (char *)&AGW->TXHeader, AGWHDDRLEN, 0);
}



VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	TidyClose(TNC, Stream);			// I don't think Hostmode has a DD
}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
	char Status[80];
	int s;

	// Clear Session Key
	
	memset(TNC->Streams[Stream].AGWKey, 0, 21);

	// if all streams are free, start scanner

	s = 0;

	while(s <= TNC->AGWInfo->MaxSessions)
	{
		if (s != Stream)
		{		
			if (TNC->PortRecord->ATTACHEDSESSIONS[s])
				return;										// Busy
		}
		s++;
	}

	sprintf(Status, "%d SCANSTART 15", TNC->Port);
	Rig_Command(-1, Status);
}

static MESSAGEY Monframe;		// I frames come in two parts.

#define TIMESTAMP 352

MESSAGEY * AdjMsg;		// Adjusted fir digis


static VOID DoMonitorHddr(struct TNCINFO * TNC, struct AGWHEADER * RXHeader, UCHAR * Msg)
{
	// Convert to ax.25 form and pass to monitor

	// Only update MH on UI, SABM, UA

	UCHAR * ptr, * starptr, * CPPtr, * nrptr, * nsptr;
	char * context;
	char MHCall[11];
	int ILen;
	char * temp;

	Msg[RXHeader->DataLength] = 0;

//	OutputDebugString(Msg);

	Monframe.LENGTH = 23;				// Control Frame
	Monframe.PORT = BPQPort[RXHeader->Port][TNC->Port];

	if (RXHeader->DataKind == 'T')		// Transmitted
		Monframe.PORT += 128;

	/*
UZ7HO T GM8BPQ-2 G8XXX  1:Fm GM8BPQ-2 To G8XXX <SABM P>[12:08:42]
UZ7HO d G8XXX GM8BPQ-2 *** DISCONNECTED From Station G8XXX-0
UZ7HO T GM8BPQ-2 G8XXX  1:Fm GM8BPQ-2 To G8XXX <DISC P>[12:08:48]
UZ7HO T GM8BPQ-2 APRS  1:Fm GM8BPQ-2 To APRS Via WIDE2-2 <UI F pid=F0 Len=28 >[12:08:54]
=5828.54N/00612.69W- {BPQ32}
*/

	
	AdjMsg = &Monframe;					// Adjusted fir digis
	ptr = strstr(Msg, "Fm ");

	ConvToAX25(&ptr[3], Monframe.ORIGIN);

	memcpy(MHCall, &ptr[3], 11);
	strlop(MHCall, ' ');

	ptr = strstr(ptr, "To ");

	ConvToAX25(&ptr[3], Monframe.DEST);

	ptr = strstr(ptr, "Via ");

	if (ptr)
	{
		// We have digis

		char Save[100];

		memcpy(Save, &ptr[4], 60);

		ptr = strtok_s(Save, ", ", &context);
DigiLoop:

		temp = (char *)AdjMsg;
		temp += 7;
		AdjMsg = (MESSAGEY *)temp;

		Monframe.LENGTH += 7;

		starptr = strchr(ptr, '*');
		if (starptr)
			*(starptr) = 0;

		ConvToAX25(ptr, AdjMsg->ORIGIN);

		if (starptr)
			AdjMsg->ORIGIN[6] |= 0x80;				// Set end of address

		ptr = strtok_s(NULL, ", ", &context);

		if (ptr[0] != '<')
			goto DigiLoop;
	}
	AdjMsg->ORIGIN[6] |= 1;				// Set end of address

	ptr = strstr(Msg, "<");

	if (memcmp(&ptr[1], "SABM", 4) == 0)
	{
		AdjMsg->CTL = 0x2f;
//		UpdateMH(TNC, MHCall, ' ', 0);
	}
	else  
	if (memcmp(&ptr[1], "DISC", 4) == 0)
		AdjMsg->CTL = 0x43;
	else 
	if (memcmp(&ptr[1], "UA", 2) == 0)
	{
		AdjMsg->CTL = 0x63;
//		UpdateMH(TNC, MHCall, ' ', 0);
	}
	else  
	if (memcmp(&ptr[1], "DM", 2) == 0)
		AdjMsg->CTL = 0x0f;
	else 
	if (memcmp(&ptr[1], "UI", 2) == 0)
	{
		AdjMsg->CTL = 0x03;
//		UpdateMH(TNC, MHCall, ' ', 0);
	}
	else 
	if (memcmp(&ptr[1], "RR", 2) == 0)
	{
		nrptr = strchr(&ptr[3], '>');
		AdjMsg->CTL = 0x1 | (nrptr[-2] << 5);
	}
	else 
	if (memcmp(&ptr[1], "RNR", 3) == 0)
	{
		nrptr = strchr(&ptr[4], '>');
		AdjMsg->CTL = 0x5 | (nrptr[-2] << 5);
	}
	else 
	if (memcmp(&ptr[1], "REJ", 3) == 0)
	{
		nrptr = strchr(&ptr[4], '>');
		AdjMsg->CTL = 0x9 | (nrptr[-2] << 5);
	}
	else 
	if (memcmp(&ptr[1], "FRMR", 4) == 0)
		AdjMsg->CTL = 0x87;
	else  
	if (ptr[1] == 'I')
	{
		nsptr = strchr(&ptr[3], 'S');

		AdjMsg->CTL = (nsptr[-2] << 5) | (nsptr[1] & 7) << 1 ;
	}

	CPPtr = strchr(ptr, ' ');		

	if (strchr(&CPPtr[1], 'P'))
	{
		if (AdjMsg->CTL != 3)
			AdjMsg->CTL |= 0x10;
//		Monframe.DEST[6] |= 0x80;				// SET COMMAND
	}

	if (strchr(&CPPtr[1], 'F'))
	{
		if (AdjMsg->CTL != 3)
			AdjMsg->CTL |= 0x10;
//		Monframe.ORIGIN[6] |= 0x80;				// SET P/F bit
	}

	if ((AdjMsg->CTL & 1) == 0 || AdjMsg->CTL == 3)	// I or UI
	{
		ptr = strstr(ptr, "pid");	
		sscanf(&ptr[4], "%x", (unsigned int *)&AdjMsg->PID);
	
		ptr = strstr(ptr, "Len");	
		ILen = atoi(&ptr[4]);

		ptr = strstr(ptr, "]");
		ptr += 2;						// Skip ] and cr
		memcpy(AdjMsg->L2DATA, ptr, ILen);
		Monframe.LENGTH += ILen;
	}
	
	time(&Monframe.Timestamp);
	BPQTRACE((MESSAGE *)&Monframe, TRUE);

}

/*

1:Fm GM8BPQ To GM8BPQ-2 <RR R1 >[17:36:17]
 1:Fm GM8BPQ To GM8BPQ-2 <I R1 S7 pid=F0 Len=56 >[17:36:29]
BPQ:GM8BPQ-2} G8BPQ Win32 Test Switch, Skigersta, Isle o

 1:Fm GM8BPQ-2 To GM8BPQ <RR R0 >[17:36:32]
 1:Fm GM8BPQ To GM8BPQ-2 <I R1 S0 pid=F0 Len=9 >[17:36:33]
f Lewis.

 1:Fm GM8BPQ-2 To GM8BPQ <RR R1 >[17:36:36]

1:Fm GM8BPQ To GM8BPQ-2 <RR F/R R1> [17:36:18R]
1:Fm GM8BPQ To GM8BPQ-2 <I F/C R1 S7 Pid=F0 Len=56> [17:36:30R]
BPQ:GM8BPQ-2} G8BPQ Win32 Test Switch, Skigersta, Isle o
1:Fm GM8BPQ-2 To GM8BPQ <RR F/R R0> [17:36:32T]
1:Fm GM8BPQ To GM8BPQ-2 <I F/C R1 S0 Pid=F0 Len=9> [17:36:34R]
f Lewis.

1:Fm GM8BPQ-2 To GM8BPQ <RR F/R R1> [17:36:36T]



1:Fm GM8BPQ To GM8BPQ-2 <RR F/R R1> [17:36:17T]
1:Fm GM8BPQ To GM8BPQ-2 <I F/C R1 S7 Pid=F0 Len=56> [17:36:29T]
BPQ:GM8BPQ-2} G8BPQ Win32 Test Switch, Skigersta, Isle o
1:Fm GM8BPQ-2 To GM8BPQ <RR F/R R0> [17:36:32R]
1:Fm GM8BPQ To GM8BPQ-2 <I F/C R1 S0 Pid=F0 Len=9> [17:36:33T]
f Lewis.

1:Fm GM8BPQ-2 To GM8BPQ <RR F/R R1> [17:36:36R]
*/
