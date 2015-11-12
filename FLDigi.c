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
//	FLARQ Emulator/FLDIGI Interface for BPQ32
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

#define VERSION_MAJOR         2
#define VERSION_MINOR         0

#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02

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

static void ConnecttoFLDigiThread(int port);

void CreateMHWindow();
int Update_MH_List(struct in_addr ipad, char * call, char proto);

static int ConnecttoFLDigi();
static int ProcessReceivedData(int bpqport);
static ProcessLine(char * buf, int Port);
int KillTNC(struct TNCINFO * TNC);
static int RestartTNC(struct TNCINFO * TNC);
VOID ProcessFLDigiPacket(struct TNCINFO * TNC, char * Message, int Len);
VOID ProcessFLDigiKISSPacket(struct TNCINFO * TNC, char * Message, int Len);
struct TNCINFO * GetSessionKey(char * key, struct TNCINFO * TNC);
VOID SendARQData(struct TNCINFO * TNC, UINT * Buffer);
static VOID DoMonitorHddr(struct TNCINFO * TNC, struct AGWHEADER * RXHeader, UCHAR * Msg);
VOID SendRPBeacon(struct TNCINFO * TNC);
VOID FLReleaseTNC(struct TNCINFO * TNC);
unsigned int CalcCRC(UCHAR * ptr, int Len);
VOID ARQTimer(struct TNCINFO * TNC);
VOID QueueAndSend(struct TNCINFO * TNC, struct ARQINFO * ARQ, SOCKET sock, char * Msg, int MsgLen);
VOID SaveAndSend(struct TNCINFO * TNC, struct ARQINFO * ARQ, SOCKET sock, char * Msg, int MsgLen);
VOID ProcessARQStatus(struct TNCINFO * TNC, struct ARQINFO * ARQ, char *Input);
VOID SendXMLPoll(struct TNCINFO * TNC);
static int ProcessXMLData(int port);
VOID CheckFLDigiData(struct TNCINFO * TNC);
VOID SendPacket(struct TNCINFO * TNC, UCHAR * Msg, int MsgLen);
int	KissEncode(UCHAR * inbuff, UCHAR * outbuff, int len);
VOID SendXMLCommand(struct TNCINFO * TNC, char * Command, char * Value, char ParamType);
VOID FLSlowTimer(struct TNCINFO * TNC);
VOID SendKISSCommand(struct TNCINFO * TNC, char * Msg);

int DoScanLine(struct TNCINFO * TNC, char * Buff, int Len);
VOID SuspendOtherPorts(struct TNCINFO * ThisTNC);
VOID ReleaseOtherPorts(struct TNCINFO * ThisTNC);

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

static char WindowTitle[] = "FLDIGI";
static char ClassName[] = "FLDIGISTATUS";
static int RigControlRow = 165;

static fd_set readfs;
static fd_set writefs;
static fd_set errorfs;
static struct timeval timeout;

int Blocksizes[10] = {0,2,4,8,16,32,64,128,256,512};


static int ExtProc(int fn, int port,unsigned char * buff)
{
	UINT * buffptr;
	char txbuff[500];
	unsigned int txlen=0;
	struct TNCINFO * TNC = TNCInfo[port];
	int Stream = 0;
	struct STREAMINFO * STREAM;
	int TNCOK;

	if (TNC == NULL)
		return 0;					// Port not defined

	// Look for attach on any call

//	for (Stream = 0; Stream <= 1; Stream++)
	{
		STREAM = &TNC->Streams[Stream];
	
		if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] && TNC->Streams[Stream].Attached == 0)
		{
			char Cmd[80];

			// New Attach

			int calllen;
			STREAM->Attached = TRUE;

			TNC->FLInfo->RAW = FALSE;

			calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4USER, STREAM->MyCall);
			STREAM->MyCall[calllen] = 0;
			STREAM->FramesOutstanding = 0;

			SuspendOtherPorts(TNC);				// Dont allow connects on interlocked ports

			// Stop Scanning

			sprintf(Cmd, "%d SCANSTOP", TNC->Port);
			Rig_Command(-1, Cmd);

			sprintf(TNC->WEB_TNCSTATE, "In Use by %s", TNC->Streams[0].MyCall);
			SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

/*			len = sprintf(Cmd, "%cSTOP_BEACON_ARQ_FAE\x1b", '\x1a');
	
			if (TNC->MPSKInfo->TX)
				TNC->CmdSet = TNC->CmdSave = _strdup(Cmd);		// Savde till not transmitting
			else
				SendPacket(TNC->WINMORDataSock, Cmd, len, 0);
*/
		}
	}

	switch (fn)
	{
	case 7:			

		// 100 mS Timer. 

		//	See if waiting for busy to clear before sending a connect

		if (TNC->BusyDelay)
		{
			// Still Busy?

			if (InterlockedCheckBusy(TNC) == FALSE)
			{
				// No, so send connect

				struct ARQINFO * ARQ = TNC->ARQInfo;
				int SendLen;
				char Reply[80];

				SendLen = sprintf(Reply, "c%s:42 %s:24 %c 7 T60R5W10",
				STREAM->MyCall, STREAM->RemoteCall, ARQ->OurStream); 

				strcpy(TNC->WEB_PROTOSTATE, "Connecting");
				SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

				ARQ->ARQState = ARQ_ACTIVE;

				ARQ->ARQTimerState = ARQ_CONNECTING;
				SaveAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);

				STREAM->Connecting = TRUE;	

				sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s", STREAM->MyCall, STREAM->RemoteCall);
				SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

				strcpy(TNC->WEB_PROTOSTATE, "Connecting");
				SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

				TNC->BusyDelay = 0;
			}
			else
			{
				// Wait Longer

				TNC->BusyDelay--;

				if (TNC->BusyDelay == 0)
				{
					// Timed out - Send Error Response

					UINT * buffptr = GetBuff();

					if (buffptr == 0) return (0);			// No buffers, so ignore

					buffptr[1]=39;
					memcpy(buffptr+2,"Sorry, Can't Connect - Channel is busy\r", 39);

					C_Q_ADD(&TNC->Streams[0].PACTORtoBPQ_Q, buffptr);
					free(TNC->ConnectCmd);

					sprintf(TNC->WEB_TNCSTATE, "In Use by %s", TNC->Streams[0].MyCall);
					SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

				}
			}
		}



		if (STREAM->NeedDisc)
		{
			STREAM->NeedDisc--;

			if (STREAM->NeedDisc == 0)
			{
				// Send the DISCONNECT

				TidyClose(TNC, 0);
			}
		}

		ARQTimer(TNC);
		SendXMLPoll(TNC);

		TNC->SlowTimer--;

		if (TNC->SlowTimer < 0)
		{
			TNC->SlowTimer = 100;
			FLSlowTimer(TNC);			// 10 Secs
		}
	
		return 0;

	case 1:				// poll

			if (TNC->CONNECTED == FALSE && TNC->CONNECTING == FALSE && TNC->FLInfo->KISSMODE == FALSE)
			{
				//	See if time to reconnect
		
				time( &ltime );
				if (ltime-lasttime[port] >9 )
				{
					ConnecttoFLDigi(port);
					lasttime[port]=ltime;
				}
			}
		
			FD_ZERO(&readfs);
			
			if (TNC->CONNECTED)
				if (TNC->WINMORSock)
					FD_SET(TNC->WINMORSock,&readfs);

			if (TNC->CONNECTED || TNC->FLInfo->KISSMODE)
				FD_SET(TNC->WINMORDataSock,&readfs);
			
			
//			FD_ZERO(&writefs);

//			if (TNC->BPQtoWINMOR_Q) FD_SET(TNC->WINMORDataSock,&writefs);	// Need notification of busy clearing

			FD_ZERO(&errorfs);
		
			if (TNC->CONNECTED)
				if (TNC->WINMORSock)
					FD_SET(TNC->WINMORSock,&errorfs);
	
			if (TNC->CONNECTED || TNC->FLInfo->KISSMODE)
				FD_SET(TNC->WINMORDataSock,&errorfs);
			

			if (select(TNC->WINMORDataSock + 1, &readfs, &writefs, &errorfs, &timeout) > 0)
			{
				//	See what happened

				if (FD_ISSET(TNC->WINMORDataSock,&readfs))
				{
					// data available
			
					ProcessReceivedData(port);			
				}

				if (FD_ISSET(TNC->WINMORSock,&readfs))
				{
					// data available
			
					ProcessXMLData(port);			
				}


				if (FD_ISSET(TNC->WINMORDataSock,&writefs))
				{
					if (BPQtoMPSK_Q[port] == 0)
					{
						//	Connect success

						TNC->CONNECTED = TRUE;
						TNC->CONNECTING = FALSE;

						sprintf(TNC->WEB_COMMSSTATE, "Connected to FLDIGI");
						SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

						// If required, send signon
				
//						SendPacket(TNC->WINMORDataSock,"\x1a", 1, 0);
//						SendPacket(TNC->WINMORDataSock,"DIGITAL MODE ?", 14, 0);
//						SendPacket(TNC->WINMORDataSock,"\x1b", 1, 0);

//						EnumWindows(EnumTNCWindowsProc, (LPARAM)TNC);
					}
					else
					{
						// Write block has cleared. Send rest of packet

						buffptr=Q_REM(&BPQtoMPSK_Q[port]);

						txlen=buffptr[1];

						memcpy(txbuff,buffptr+2,txlen);

						SendPacket(TNC, &txbuff[0], txlen);
					
						ReleaseBuffer(buffptr);

					}

				}
					
				if (FD_ISSET(TNC->WINMORDataSock,&errorfs) || FD_ISSET(TNC->WINMORSock,&errorfs))
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



		// See if any frames for this port

		for (Stream = 0; Stream <= 1; Stream++)
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

			if (UILen < 129 && TNC->Streams[0].Attached == FALSE)			// Be sensible!
			{
				// >00uG8BPQ:72 TestA
				SendLen = sprintf(Reply, "u%s:72 %s", TNC->NodeCall, UIMsg);
				SendPacket(TNC, Reply, SendLen);
			}
			ReleaseBuffer(buffptr);
		}
			
		return (0);

	case 2:				// send

		
		if (!TNC->CONNECTED) return 0;		// Don't try if not connected to TNC

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

			if (_memicmp(&buff[8], "MODEM ", 6) == 0)
			{
				_strupr(&buff[8]);
				buff[7 + txlen] = 0;	
			
				// If in KISS mode, send as a KISS command Frame

				if (TNC->FLInfo->KISSMODE)
				{
					sprintf(txbuff, "MODEM:%s MODEM:", &buff[14]);
					SendKISSCommand(TNC, txbuff);
				}
				else
				{
					SendXMLCommand(TNC, "modem.set_by_name", &buff[14], 'S');
				}

				TNC->InternalCmd = TRUE;
				return 1;
			}

			if (_memicmp(&buff[8], "FREQ ", 5) == 0)
			{
				_strupr(&buff[8]);
				buff[7 + txlen] = 0;	
			
				// If in KISS mode, send as a KISS command Frame

				if (TNC->FLInfo->KISSMODE)
				{
					sprintf(txbuff, "WFF:%s WFF:",&buff[13]);
					SendKISSCommand(TNC, txbuff);
				}
				else
				{
					SendXMLCommand(TNC, "modem.set_carrier", &buff[13], 'I');
				}

				TNC->InternalCmd = TRUE;
				return 1;
			}

			if (_memicmp(&buff[8], "SQUELCH ", 8) == 0)
			{
				_strupr(&buff[8]);
				buff[7 + txlen] = 0;	
			
				// Only works in KISS
				
				if (TNC->FLInfo->KISSMODE)
				{
					if (_memicmp(&buff[16], "ON", 2) == 0)
						sprintf(txbuff, "KPSQL:ON KPSQL:");

					else if (_memicmp(&buff[16], "OFF", 3) == 0)
						sprintf(txbuff, "KPSQL:OFF KPSQL:");
					else
						txlen = sprintf(txbuff, "KPSQLS:%s KPSQLS:", &buff[16]);

					SendKISSCommand(TNC, txbuff);	
					TNC->InternalCmd = TRUE;
				}
				return 1;
			}

			if (_memicmp(&buff[8], "KPSATT ", 7) == 0)
			{
				_strupr(&buff[8]);
				buff[7 + txlen] = 0;

				// If in KISS mode, send as a KISS command Frame

				if (TNC->FLInfo->KISSMODE)
				{
					sprintf(txbuff, "KPSATT:%s KPSATT:", &buff[15]);
					SendKISSCommand(TNC, txbuff);
					TNC->InternalCmd = TRUE;
				}

				return 1;
			}

			if (STREAM->Connecting && _memicmp(&buff[8], "ABORT", 5) == 0)
			{
//				len = sprintf(Command,"%cSTOP_SELECTIVE_CALL_ARQ_FAE\x1b", '\x1a');
	
//				if (TNC->MPSKInfo->TX)
//					TNC->CmdSet = TNC->CmdSave = _strdup(Command);		// Save till not transmitting
//				else
//					SendPacket(TNC->WINMORDataSock, Command, len, 0);

//				TNC->InternalCmd = TRUE;
				return (0);
			}

			if (_memicmp(&buff[8], "MODE", 4) == 0)
			{
				UINT * buffptr = GetBuff();
				buff[7 + txlen] = 0;		// Remove CR
				
				if (strstr(&buff[8], "RAW"))
					TNC->FLInfo->RAW = TRUE;
				else if (strstr(&buff[8], "KISS"))
					TNC->FLInfo->RAW = FALSE;
				else
				{
					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "FLDigi} Error - Invalid Mode\r");
					C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
					return 1;
				}

				buffptr[1] = sprintf((UCHAR *)&buffptr[2], "FLDigi} Ok - Mode is %s\r",
					(TNC->FLInfo->RAW)?"RAW":"KISS");

				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
			
				return 1;
			}


			if (_memicmp(&buff[8], "INUSE?", 6) == 0)
			{
				// Return Error if in use, OK if not

				UINT * buffptr = GetBuff();
				int s = 0;

				while(s <= 1)
				{
					if (s != Stream)
					{		
						if (TNC->PortRecord->ATTACHEDSESSIONS[s])
						{
							buffptr[1] = sprintf((UCHAR *)&buffptr[2], "FLDig} Error - In use\r");
							C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
							return 1;							// Busy
						}
					}
					s++;
				}
				buffptr[1] = sprintf((UCHAR *)&buffptr[2], "FLDigi} Ok - Not in use\r");
				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
			
				return 1;
			}

			// See if a Connect Command.

			if (toupper(buff[8]) == 'C' && buff[9] == ' ' && txlen > 2)	// Connect
			{
				char * ptr;
				char * context;
				struct ARQINFO * ARQ = TNC->ARQInfo;
				int SendLen;
				char Reply[80];

				_strupr(&buff[8]);
				buff[8 + txlen] = 0;

				memset(ARQ, 0, sizeof(struct ARQINFO));		// Reset ARQ State
				ARQ->TXSeq = ARQ->TXLastACK = 63;			// Last Sent
				ARQ->RXHighest = ARQ->RXNoGaps = 63;		// Last Received
				ARQ->OurStream = (rand() % 78) + 49;		// To give some protection against other stuff on channel	
				ARQ->FarStream = 48;						// Not yet defined
				TNC->FLInfo->FLARQ = FALSE;

				memset(STREAM->RemoteCall, 0, 10);

				ptr = strtok_s(&buff[10], " ,\r", &context);
				strcpy(STREAM->RemoteCall, ptr);

				// See if Busy
				
				if (InterlockedCheckBusy(TNC))
				{
					// Channel Busy. Unless override set, wait

					if (TNC->OverrideBusy == 0)
					{
						// Save Command, and wait up to 10 secs
						
						sprintf(TNC->WEB_TNCSTATE, "Waiting for clear channel");
						SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

						TNC->BusyDelay = TNC->BusyWait * 10;		// BusyWait secs
						return 0;
					}
				}

				TNC->OverrideBusy = FALSE;

//<SOH>00cG8BPQ:1025 G8BPQ:24 0 7 T60R5W10FA36<EOT>

				SendLen = sprintf(Reply, "c%s:42 %s:24 %c 7 T60R5W10",
					STREAM->MyCall, STREAM->RemoteCall, ARQ->OurStream); 

				strcpy(TNC->WEB_PROTOSTATE, "Connecting");
				SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

				ARQ->ARQState = ARQ_ACTIVE;

				ARQ->ARQTimerState = ARQ_CONNECTING;
				SaveAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);

				STREAM->Connecting = TRUE;	

				sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s", STREAM->MyCall, STREAM->RemoteCall);
				SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

				strcpy(TNC->WEB_PROTOSTATE, "Connecting");
				SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

				return 0;
			}

			// Send any other command to FLDIGI

			_strupr(&buff[8]);
			buff[7 + txlen] = 0;	
			
			// If in KISS mode, send as a KISS command Frame

			if (TNC->FLInfo->KISSMODE)
			{
				char outbuff[1000];
				int newlen;

				buff[7] = 6;				// KISS Control

				newlen = KissEncode(&buff[7], outbuff, txlen);	
				sendto(TNC->WINMORDataSock, outbuff, newlen, 0, (struct sockaddr *)&TNC->Datadestaddr, sizeof(struct sockaddr));
			}
			else
			{
				SendXMLCommand(TNC, "modem.set_by_name", &buff[8], 'S');
			}

			TNC->InternalCmd = TRUE;
		}

		return (0);

	case 3:	

		Stream = (int)buff;

		TNCOK = TNC->CONNECTED;

		STREAM = &TNC->Streams[Stream];
		{
			// Busy if TX Window reached

			struct ARQINFO * ARQ = TNC->ARQInfo;
			int Outstanding;

			Outstanding = ARQ->TXSeq - ARQ->TXLastACK;

			if (Outstanding < 0)
				Outstanding += 64;

			TNC->PortRecord->FramesQueued = Outstanding + TNC->Streams[0].BPQtoPACTOR_Q;		// Save for Appl Level Queued Frames

			if (Outstanding > ARQ->TXWindow)
				return (1 | TNCOK << 8 | STREAM->Disconnecting << 15); // 3rd Nibble is frames unacked
			else
				return TNCOK << 8 | STREAM->Disconnecting << 15;

		}
		return TNCOK << 8 | STREAM->Disconnecting << 15;		// OK, but lock attach if disconnecting
	
	case 4:				// reinit

		shutdown(TNC->WINMORSock, SD_BOTH);
		shutdown(TNC->WINMORDataSock, SD_BOTH);
		Sleep(100);

		closesocket(TNC->WINMORSock);
		closesocket(TNC->WINMORDataSock);
		TNC->CONNECTED = FALSE;

		if (TNC->WeStartedTNC)
		{
			KillTNC(TNC);
			RestartTNC(TNC);
		}

		return (0);

	case 5:				// Close

		shutdown(TNC->WINMORSock, SD_BOTH);
		shutdown(TNC->WINMORDataSock, SD_BOTH);
		Sleep(100);

		closesocket(TNC->WINMORSock);
		closesocket(TNC->WINMORDataSock);

		if (TNC->WeStartedTNC)
		{
			KillTNC(TNC);
		}

		return 0;
	}

	return 0;
}

#ifndef LINBPQ

int FindFLDIGI(char * Path)
{
	HANDLE hProc;
	char ExeName[256] = "";
	char FLDIGIName[256];
	DWORD Pid = 0;
	DWORD Processes[1024], Needed, Count;
    unsigned int i;

	if (EnumProcessesPtr == NULL)
		return 0;			// Cant get PID

	if (!EnumProcessesPtr(Processes, sizeof(Processes), &Needed))
		return TRUE;

	//	Path is to .bat, so need to strip extension of both names

	strcpy(FLDIGIName, Path);
	strlop(FLDIGIName, '.');

	// Calculate how many process identifiers were returned.

	Count = Needed / sizeof(DWORD);

	for (i = 0; i < Count; i++)
	{
		if (Processes[i] != 0)
		{
			hProc =  OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, Processes[i]);
	
			if (hProc)
			{
				GetModuleFileNameExPtr(hProc, 0,  ExeName, 255);
				CloseHandle(hProc);

				strlop(ExeName, '.');
						
				if (_stricmp(ExeName, FLDIGIName) == 0)
					return Processes[i];
						
			}
		}
	}
	return 0;
}


static KillTNC(struct TNCINFO * TNC)
{
	HANDLE hProc;

	if (TNC->PTTMode)
		Rig_PTT(TNC->RIG, FALSE);			// Make sure PTT is down

	if (TNC->ProgramPath)
		TNC->WIMMORPID = FindFLDIGI(TNC->ProgramPath);

	if (TNC->WIMMORPID == 0) return 0;

	hProc =  OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, TNC->WIMMORPID);

	if (hProc)
	{
		TerminateProcess(hProc, 0);
		CloseHandle(hProc);
	}

	TNC->WeStartedTNC = 0;			// So we don't try again

	return 0;
}

#endif

static RestartTNC(struct TNCINFO * TNC)
{
	if (TNC->ProgramPath == NULL)
		return 0;

	_strlwr(TNC->ProgramPath);

	if (_memicmp(TNC->ProgramPath, "REMOTE:", 7) == 0)
	{
		int n;
		
		// Try to start TNC on a remote host

		SOCKET sock = socket(AF_INET,SOCK_DGRAM,0);
		struct sockaddr_in destaddr;

		Debugprintf("trying to restart FLDIGI %s", TNC->ProgramPath);

		if (sock == INVALID_SOCKET)
			return 0;

		destaddr.sin_family = AF_INET;
		destaddr.sin_addr.s_addr = inet_addr(TNC->WINMORHostName);
		destaddr.sin_port = htons(8500);

		if (destaddr.sin_addr.s_addr == INADDR_NONE)
		{
			//	Resolve name to address

			struct hostent * HostEnt = gethostbyname (TNC->WINMORHostName);
		 
			if (!HostEnt)
				return 0;			// Resolve failed

			memcpy(&destaddr.sin_addr.s_addr,HostEnt->h_addr,4);
		}

		n = sendto(sock, TNC->ProgramPath, strlen(TNC->ProgramPath), 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
	
		Debugprintf("Restart FLDIGI - sento returned %d", n);

		Sleep(100);
		closesocket(sock);

		return 1;				// Cant tell if it worked, but assume ok
	}
#ifndef LINBPQ
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

		//	for some reason the program name must be lower case

		_strlwr(TNC->ProgramPath);

		ret = CreateProcess(TNC->ProgramPath, NULL, NULL, NULL, FALSE,0 ,NULL , NULL, &SInfo, &PInfo);
		return ret;
	}
	}
#endif
	return 0;
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

VOID FLDIGISuspendPort(struct TNCINFO * TNC)
{
	TNC->FLInfo->CONOK = FALSE;
}

VOID FLDIGIReleasePort(struct TNCINFO * TNC)
{
	TNC->FLInfo->CONOK = TRUE;
}

VOID SendKISSCommand(struct TNCINFO * TNC, char * Msg)
{
	int txlen, rc;
	char txbuff[256];
	char outbuff[256];

	txlen = sprintf(txbuff, "%c%s", 6, Msg);
	txlen = KissEncode(txbuff, outbuff, txlen);	
	rc = sendto(TNC->WINMORDataSock, outbuff, txlen, 0, (struct sockaddr *)&TNC->Datadestaddr, sizeof(struct sockaddr));
}

UINT FLDigiExtInit(EXTPORTDATA * PortEntry)
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

	TNC->FLInfo->CONOK = TRUE;

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0 || PortEntry->PORTCONTROL.PORTPACLEN > 128)
		PortEntry->PORTCONTROL.PORTPACLEN = 64;

	TNC->SuspendPortProc = FLDIGISuspendPort;
	TNC->ReleasePortProc = FLDIGIReleasePort;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	TNC->Hardware = H_FLDIGI;

	if (TNC->BusyWait == 0)
		TNC->BusyWait = 10;

	MPSKChannel[port] = PortEntry->PORTCONTROL.CHANNELNUM-65;
	
	PortEntry->MAXHOSTMODESESSIONS = 1;	

	i=sprintf(Msg,"FLDigi Host %s Port %d \n",
		TNC->WINMORHostName, TNC->WINMORPort);

	WritetoConsole(Msg);

#ifndef LINBPQ

	if (TNC->ProgramPath)
		TNC->WIMMORPID = FindFLDIGI(TNC->ProgramPath);

	if (TNC->WIMMORPID == 0)	// Not running
#endif
		TNC->WeStartedTNC = RestartTNC(TNC);		// Always try if Linux

	if (TNC->FLInfo->KISSMODE)
	{
		// Open Datagram port

		SOCKET sock;
		u_long param=1;
		BOOL bcopt=TRUE;
		struct sockaddr_in sinx;
		struct hostent * HostEnt = NULL;

		TNC->FLInfo->CmdControl = 5;			//Send params immediately
		
		TNC->Datadestaddr.sin_addr.s_addr = inet_addr(TNC->WINMORHostName);

		if (TNC->Datadestaddr.sin_addr.s_addr == INADDR_NONE)
		{
			//	Resolve name to address

			 HostEnt = gethostbyname (TNC->WINMORHostName);
		 
			if (HostEnt)
			{
				memcpy(&TNC->Datadestaddr.sin_addr.s_addr,HostEnt->h_addr,4);
			}
		}

		TNC->WINMORDataSock = sock = socket(AF_INET,SOCK_DGRAM,0);

		ioctl(sock, FIONBIO, &param);

		setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char FAR *)&bcopt,4);

		sinx.sin_family = AF_INET;
		sinx.sin_addr.s_addr = INADDR_ANY;		
		sinx.sin_port = htons(TNC->WINMORPort + 1);

		if (bind(sock, (struct sockaddr *) &sinx, sizeof(sinx)) != 0 )
		{
			//	Bind Failed

			int err = WSAGetLastError();
			Consoleprintf("Bind Failed for UDP port %d - error code = %d", TNC->WINMORPort, err);
		}

		TNC->Datadestaddr.sin_family = AF_INET;	
		TNC->Datadestaddr.sin_port = htons(TNC->WINMORPort);
	}
	else
		ConnecttoFLDigi(port);

	time(&lasttime[port]);			// Get initial time value

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

	CreatePactorWindow(TNC, ClassName, WindowTitle, RigControlRow, PacWndProc, 500, 450);

	CreateWindowEx(0, "STATIC", "Comms State", WS_CHILD | WS_VISIBLE, 10,6,120,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_COMMSSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,6,386,20, TNC->hDlg, NULL, hInstance, NULL);
	
	CreateWindowEx(0, "STATIC", "TNC State", WS_CHILD | WS_VISIBLE, 10,28,106,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TNCSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,28,520,20, TNC->hDlg, NULL, hInstance, NULL);

	CreateWindowEx(0, "STATIC", "Mode/CF", WS_CHILD | WS_VISIBLE, 10,50,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_MODE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,50,200,20, TNC->hDlg, NULL, hInstance, NULL);
 
	CreateWindowEx(0, "STATIC", "Channel State", WS_CHILD | WS_VISIBLE, 10,72,110,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_CHANSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,72,144,20, TNC->hDlg, NULL, hInstance, NULL);
 
 	CreateWindowEx(0, "STATIC", "Proto State", WS_CHILD | WS_VISIBLE,10,94,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_PROTOSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE,116,94,374,20 , TNC->hDlg, NULL, hInstance, NULL);
 
	CreateWindowEx(0, "STATIC", "Traffic", WS_CHILD | WS_VISIBLE,10,116,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TRAFFIC = CreateWindowEx(0, "STATIC", "RX 0 TX 0 ACKED 0 Resent 0", WS_CHILD | WS_VISIBLE,116,116,374,20 , TNC->hDlg, NULL, hInstance, NULL);

	TNC->hMonitor= CreateWindowEx(0, "LISTBOX", "", WS_CHILD |  WS_VISIBLE  | LBS_NOINTEGRALHEIGHT | 
            LBS_DISABLENOSCROLL | WS_HSCROLL | WS_VSCROLL,
			0,170,250,300, TNC->hDlg, NULL, hInstance, NULL);

	TNC->ClientHeight = 450;
	TNC->ClientWidth = 500;

	TNC->hMenu = CreatePopupMenu();

	AppendMenu(TNC->hMenu, MF_STRING, WINMOR_KILL, "Kill FLDigi");
	AppendMenu(TNC->hMenu, MF_STRING, WINMOR_RESTART, "Kill and Restart FLDigi");

	MoveWindows(TNC);
#endif

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
	struct ARQINFO * ARQ;
	struct FLINFO * FL;

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

	ARQ = TNC->ARQInfo = zalloc(sizeof(struct ARQINFO)); 
	FL = TNC->FLInfo = zalloc(sizeof(struct FLINFO)); 

	TNC->Timeout = 50;		// Default retry = 5 seconds
	TNC->Retries = 6;		// Default Retries
	TNC->Window = 16;

	TNC->FLInfo->KISSMODE = TRUE;		// Default to KISS

	TNC->InitScript = malloc(1000);
	TNC->InitScript[0] = 0;
	
		if (p_ipad == NULL)
			p_ipad = strtok(NULL, " \t\n\r");

		if (p_ipad == NULL) return (FALSE);
	
		p_port = strtok(NULL, " \t\n\r");
			
		if (p_port == NULL) return (FALSE);

		TNC->WINMORPort = atoi(p_port);

		TNC->destaddr.sin_family = AF_INET;
		TNC->destaddr.sin_port = htons(TNC->WINMORPort + 40);		// Defaults XML 7362 ARQ 7322
		
		TNC->Datadestaddr.sin_family = AF_INET;
		TNC->Datadestaddr.sin_port = htons(TNC->WINMORPort);

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

			if (_memicmp(buf, "TIMEOUT", 7) == 0)
				TNC->Timeout = atoi(&buf[8]) * 10;
			else
			if (_memicmp(buf, "RETRIES", 7) == 0)
				TNC->Retries = atoi(&buf[8]);
			else
			if (_memicmp(buf, "WINDOW", 6) == 0)
				TNC->Window = atoi(&buf[7]);
			else
			if (_memicmp(buf, "ARQMODE", 7) == 0)
				TNC->FLInfo->KISSMODE = FALSE;
			else
			if (_memicmp(buf, "DEFAULTMODEM", 12) == 0) // Send Beacon after each session 
			{
				// Check that freq is also specified

				char * Freq = strchr(&buf[13], '/');

				if (Freq)
				{
					*(Freq++) = 0;
					strcpy(TNC->FLInfo->DefaultMode, &buf[13]);
					TNC->FLInfo->DefaultFreq = atoi(Freq);
				}
			}
			else
			
			strcat (TNC->InitScript, buf);
		}


	return (TRUE);	
}

static int ConnecttoFLDigi(int port)
{
	_beginthread(ConnecttoFLDigiThread,0,port);

	
	return 0;
}

static VOID ConnecttoFLDigiThread(port)
{
	char Msg[255];
	int err,i;
	u_long param=1;
	BOOL bcopt=TRUE;
	struct hostent * HostEnt = NULL;
	struct TNCINFO * TNC = TNCInfo[port];

	Sleep(5000);		// Allow init to complete 

	TNC->destaddr.sin_addr.s_addr = inet_addr(TNC->WINMORHostName);
	TNC->Datadestaddr.sin_addr.s_addr = inet_addr(TNC->WINMORHostName);

	if (TNC->destaddr.sin_addr.s_addr == INADDR_NONE)
	{
		//	Resolve name to address

		 HostEnt = gethostbyname (TNC->WINMORHostName);
		 
		 if (!HostEnt) return;			// Resolve failed

		 memcpy(&TNC->destaddr.sin_addr.s_addr,HostEnt->h_addr,4);
		 memcpy(&TNC->Datadestaddr.sin_addr.s_addr,HostEnt->h_addr,4);
	}

	TNC->WINMORSock=socket(AF_INET,SOCK_STREAM,0);

	if (TNC->WINMORSock == INVALID_SOCKET)
	{
		i=sprintf(Msg, "Socket Failed for FLDigi Control socket - error code = %d\n", WSAGetLastError());
		WritetoConsole(Msg);
  	 	return; 
	}
 
	setsockopt (TNC->WINMORSock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt, 4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	TNC->CONNECTING = TRUE;

	if (connect(TNC->WINMORSock,(LPSOCKADDR) &TNC->destaddr,sizeof(TNC->destaddr)) == 0)
	{
		//
		//	Connected successful
		//
	}
	else
	{
		if (TNC->Alerted == FALSE)
		{
			err=WSAGetLastError();
   			i=sprintf(Msg, "Connect Failed for FLDigi Control socket - error code = %d\n", err);
			WritetoConsole(Msg);

			sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC failed");
			SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);
			TNC->Alerted = TRUE;
		}
		
		TNC->CONNECTING = FALSE;
		return;
	}

	TNC->LastFreq = 0;

	TNC->WINMORDataSock=socket(AF_INET,SOCK_STREAM,0);

	setsockopt (TNC->WINMORDataSock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt, 4);

	if (TNC->WINMORDataSock == INVALID_SOCKET)
	{
		i=sprintf(Msg, "Socket Failed for FLDigi socket - error code = %d\r\n", WSAGetLastError());
		WritetoConsole(Msg);

		closesocket(TNC->WINMORSock);
		closesocket(TNC->WINMORDataSock);
	 	TNC->CONNECTING = FALSE;

  	 	return; 
	}
 
	if (bind(TNC->WINMORDataSock, (LPSOCKADDR) &sinx, addrlen) != 0 )
	{
		//
		//	Bind Failed
		//
	
		i=sprintf(Msg, "Bind Failed for FLDigi Data socket - error code = %d\r\n", WSAGetLastError());
		WritetoConsole(Msg);

		closesocket(TNC->WINMORSock);
		closesocket(TNC->WINMORDataSock);
	 	TNC->CONNECTING = FALSE;
  	 	return; 
	}

	if (connect(TNC->WINMORDataSock,(LPSOCKADDR) &TNC->Datadestaddr,sizeof(TNC->Datadestaddr)) == 0)
	{
		ioctlsocket (TNC->WINMORDataSock,FIONBIO,&param);		// Set nonblocking
		TNC->CONNECTED = TRUE;
	 	TNC->CONNECTING = FALSE;

		TNC->Alerted = TRUE;

		sprintf(TNC->WEB_COMMSSTATE, "Connected to FLDIGI");
		SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);
	}
	else
	{
		sprintf(Msg, "Connect Failed for FLDigi Data socket Port %d - error code = %d\r\n", port, WSAGetLastError());
		WritetoConsole(Msg);

		closesocket(TNC->WINMORSock);
		closesocket(TNC->WINMORDataSock);
	 	TNC->CONNECTING = FALSE;
	}

	return;

}

VOID UpdateStatsLine(struct TNCINFO * TNC, struct STREAMINFO * STREAM)
{
	sprintf(TNC->WEB_TRAFFIC, "RX %d TX %d ACKED %d Resent %d Queued %d",
	STREAM->BytesRXed, STREAM->BytesTXed, STREAM->BytesAcked, STREAM->BytesResent, STREAM->BytesOutstanding);
	SetWindowText(TNC->xIDC_TRAFFIC, TNC->WEB_TRAFFIC);
}

VOID SendPacket(struct TNCINFO * TNC, UCHAR * Msg, int MsgLen)
{
	if (TNC->FLInfo->KISSMODE)
	{
		char KissMsg[1000];
		char outbuff[1000];
		int newlen;

		if (TNC->FLInfo->RAW)
		{
			// KISS RAW 

			// Add CRC and Send

			unsigned short CRC;
			char crcstring[6];

			KissMsg[0] = 7;			// KISS Raw
			KissMsg[1] = 1;			// SOH
			KissMsg[2] = '0';		// Version
			KissMsg[3] = TNC->ARQInfo->FarStream;

			Msg[MsgLen] = 0;

			memcpy(&KissMsg[4], Msg, MsgLen +1 );		// Get terminating NULL

			CRC = CalcCRC(KissMsg + 1, MsgLen + 3);

			sprintf(crcstring, "%04X%c", CRC, 4);

			strcat(KissMsg, crcstring);
			MsgLen += 9;
		}
		else
		{
			// Normal KISS

			KissMsg[0] = 0;					// KISS Control
			KissMsg[1] = TNC->ARQInfo->FarStream;
			memcpy(&KissMsg[2], Msg, MsgLen);
			MsgLen += 2;
		}

		newlen = KissEncode(KissMsg, outbuff, MsgLen);
		sendto(TNC->WINMORDataSock, outbuff, newlen, 0, (struct sockaddr *)&TNC->Datadestaddr, sizeof(struct sockaddr));

		SendKISSCommand(TNC, "TXBUF:");

	}
	else
	{
		// ARQ Scoket

		// Add Header, CRC and Send

		unsigned short CRC;
		char crcstring[6];
		char outbuff[1000];

		outbuff[0] = 1;			// SOH
		outbuff[1] = '0';		// Version
		outbuff[2] = TNC->ARQInfo->FarStream;

		Msg[MsgLen] = 0;

		memcpy(&outbuff[3], Msg, MsgLen + 1);

		CRC = CalcCRC(outbuff , MsgLen + 3);

		sprintf(crcstring, "%04X%c", CRC, 4);

		strcat(outbuff, crcstring);
		MsgLen += 8;

		send(TNC->WINMORDataSock, outbuff, MsgLen, 0);
	}
}

VOID ProcessFLDigiData(struct TNCINFO * TNC, UCHAR * Input, int Len, char Channel, BOOL RAW);

static int ProcessReceivedData(int port)
{
	int bytes, used, bytesleft;
	int i;
	char ErrMsg[255];
	unsigned char MessageBuff[1500];
	unsigned char * Message = MessageBuff;
	unsigned char * MessageBase = MessageBuff;

	struct TNCINFO * TNC = TNCInfo[port];
	struct FLINFO * FL = TNC->FLInfo;
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	//	If using KISS/UDP interface use recvfrom

	if (FL->KISSMODE)
	{
		struct sockaddr_in rxaddr;
		int addrlen = sizeof(struct sockaddr_in);
		unsigned char * KissEnd;

		bytesleft = recvfrom(TNC->WINMORDataSock, Message, 1500, 0, (struct sockaddr *)&rxaddr, &addrlen);
	
		if (bytesleft < 0)
		{
			int err = WSAGetLastError();
	//		if (err != 11)
	//			printf("KISS Error %d %d\n", nLength, err);
			bytes = 0;
		}

		while (bytesleft > 0)
		{
			unsigned char * in;
			unsigned char * out;
			unsigned char c;

			if (bytesleft < 3)
				return 0;

			if (Message[0] != FEND)
				return 0;				// Duff

			Message = MessageBase;
			in = out = &Message[2];

			// We may have more than one KISS message in a packet
	
			KissEnd = memchr(&Message[2], FEND, bytesleft );

			if (KissEnd == 0)
				return 0;						// Duff

			*(KissEnd) = 0;

			used = KissEnd - Message + 1;

			bytesleft -= used;
			bytes = used;

			MessageBase += used;

			if (Message[1] == 6)				// KISS Command
			{
				UCHAR * ptr = strchr(&Message[2], FEND);

				if (ptr) *ptr = 0;			// Null Terminate

				if (bytes > 250)
					Message[250] = 0;

				FL->Responding = 5;

				if (TNC->TNCOK == 0)
				{
					TNC->TNCOK = TRUE;
					TNC->CONNECTED = TRUE;

					sprintf(TNC->WEB_COMMSSTATE, "Connected to FLDIGI");
					SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);
				}

				// Trap BUSY fiest - there are lots of them, and they are likely to be confused 
				//	with tesponses to Interactive commands

				if (memcmp(&Message[2], "BUSY", 4) == 0)
				{
					BOOL Changed = FALSE;

					if (Message[7] == 'T' && FL->Busy == FALSE)
					{
						TNC->Busy = FL->Busy = TRUE;
						Changed = TRUE;
					}
					else
					{
						if (Message[7] == 'F' && FL->Busy == TRUE)
						{
							TNC->Busy = FL->Busy = FALSE;
							Changed = TRUE;
						}
					}

					if (Changed)
					{	
						if (FL->TX)
							strcpy(TNC->WEB_CHANSTATE, "TX");
						else
						if (FL->Busy)
							strcpy(TNC->WEB_CHANSTATE, "Busy");
						else
							strcpy(TNC->WEB_CHANSTATE, "Idle");

						SetWindowText(TNC->xIDC_CHANSTATE, TNC->WEB_CHANSTATE);
					}

					continue;
				}

				if (TNC->InternalCmd)
				{
					ULONG * buffptr = GetBuff();
	
					TNC->InternalCmd = FALSE;

					if (buffptr)
					{
						buffptr[1] = sprintf((UCHAR *)&buffptr[2], "FLDIGI} Ok %s\r", &Message[2]);
						C_Q_ADD(&TNC->Streams[0].PACTORtoBPQ_Q, buffptr);
					}

					// Drop through in case need to extract info from command
				}

				// Auto Command

//				Debugprintf("%d %s", TNC->PortRecord->PORTCONTROL.PORTNUMBER, &Message[2]);
		
				if (memcmp(&Message[2], "FLSTAT", 4) == 0)
				{
					if (strstr(&Message[2], "FLSTAT:INIT"))
					{
						// FLDIGI Reloaded - set parmas
						SendKISSCommand(TNC, "RSIDBCAST:ON TRXSBCAST:ON TXBEBCAST:ON KISSRAW:ON");
					}
					continue;
				}

				if (memcmp(&Message[2], "TRXS", 4) == 0)
				{
					char * ptr1, * context;
					BOOL Changed = FALSE;

					ptr1 = strtok_s(&Message[7], ",", &context);

					if (strstr(ptr1, "TX"))
					{
						if (TNC->FLInfo->TX == FALSE)
						{
							TNC->FLInfo->TX = TRUE;
							Changed = TRUE;
						}
					}
					else
					{
						if (TNC->FLInfo->TX)
						{
							TNC->FLInfo->TX = FALSE;
							Changed = TRUE;
						}
					}

					if (Changed)
					{
						if (FL->TX)
							strcpy(TNC->WEB_CHANSTATE, "TX");
						else
						if (FL->Busy)
							strcpy(TNC->WEB_CHANSTATE, "Busy");
						else
							strcpy(TNC->WEB_CHANSTATE, "Idle");

						SetWindowText(TNC->xIDC_CHANSTATE, TNC->WEB_CHANSTATE);
					}

					continue;
				}

				if (memcmp(&Message[2], "TXBUF:", 6) == 0)
				{
					char * ptr1, * context;

					ptr1 = strtok_s(&Message[8], ",", &context);
					STREAM->BytesOutstanding = atoi(ptr1);
					UpdateStatsLine(TNC, STREAM);
					continue;
				}

				if (memcmp(&Message[2], "TXBE:", 5) == 0)
				{
					STREAM->BytesOutstanding = 0;
					UpdateStatsLine(TNC, STREAM);
					continue;
				}

				if (memcmp(&Message[2], "RSIDN:", 6) == 0)
				{
					char * ptr1, * context;

					ptr1 = strtok_s(&Message[8], ",", &context);

					TNC->FLInfo->CenterFreq = atoi(ptr1);
					ptr1 = strtok_s(NULL, ",", &context);
					if (strlen(ptr1) > 19)
						ptr1[19] = 0;

					strcpy(TNC->FLInfo->CurrentMode, ptr1);
				}

				if (memcmp(&Message[2], "MODEM:", 6) == 0)
				{
					char * ptr1, * context;

					ptr1 = strtok_s(&Message[8], ",", &context);
					if (strlen(ptr1) > 19)
						ptr1[19] = 0;

					strcpy(TNC->FLInfo->CurrentMode, ptr1);
				}

				if (memcmp(&Message[2], "WFF:", 4) == 0)
				{
					char * ptr1, * context;

					ptr1 = strtok_s(&Message[6], ",", &context);
					TNC->FLInfo->CenterFreq = atoi(ptr1);
				}

				sprintf(TNC->WEB_MODE, "%s/%d", TNC->FLInfo->CurrentMode, TNC->FLInfo->CenterFreq);
				SetWindowText(TNC->xIDC_MODE, TNC->WEB_MODE);
			
				continue;
			}

			if (Message[1] == 7)				// Not Normal Data
			{
				// "RAW" Mode. Just process as if received from TCP Socket Interface

				ProcessFLDigiPacket(TNC, &Message[2] , bytes - 3);	// Data may be for another port
				continue;
			}

			bytes -= 3;					// Two FEND and Control

			// Undo KISS

			while (bytes)
			{
				bytes--;

				c = *(in++);
	
				if (c == FESC)
				{
					c = *(in++);
					bytes--;

					if (c == TFESC)
						c = FESC;
					else if (c == TFEND)
						c = FEND;
				}
				*(out++) = c;
			}
			ProcessFLDigiData(TNC, &Message[3], out - &Message[3], Message[2], FALSE);	// KISS not RAW
		}
		return 0;
	}

	//	Need to extract messages from byte stream

	bytes = recv(TNC->WINMORDataSock, Message, 500, 0);

	if (bytes == SOCKET_ERROR)
	{
//		i=sprintf(ErrMsg, "Read Failed for MPSK socket - error code = %d\r\n", WSAGetLastError());
//		WritetoConsole(ErrMsg);
				
		closesocket(TNC->WINMORDataSock);
					
		TNC->CONNECTED = FALSE;
		if (TNC->Streams[0].Attached)
			TNC->Streams[0].ReportDISC = TRUE;

		return (0);
	}

	if (bytes == 0)
	{
		//	zero bytes means connection closed

		i=sprintf(ErrMsg, "FlDigi Connection closed for BPQ Port %d\n", port);
		WritetoConsole(ErrMsg);

		TNC->CONNECTED = FALSE;
		if (TNC->Streams[0].Attached)
			TNC->Streams[0].ReportDISC = TRUE;

		return (0);
	}

	//	Have some data
	
	ProcessFLDigiPacket(TNC, Message, bytes);			// Data may be for another port

	return (0);

}


VOID ProcessFLDigiPacket(struct TNCINFO * TNC, char * Message, int Len)
{
	char * MPTR = Message;
	char c;
	struct FLINFO *	FL = TNC->FLInfo;

	// Look for SOH/EOT delimiters. May Have several SOH before EOT

	while(Len)
	{
		c = *(MPTR++);

		switch (c)
		{
		case 01:				// New Packet

			if (TNC->InPacket)
				CheckFLDigiData(TNC);

			TNC->DataBuffer[0] = 1;
			TNC->DataLen = 1;
			TNC->InPacket = TRUE;
			break;

		case 04:

			if (TNC->InPacket)
				CheckFLDigiData(TNC);
			TNC->DataLen = 0;
			TNC->InPacket = FALSE;

			break;

		default:

			if (TNC->InPacket)
			{
				if (TNC->DataLen == 1)
				{
					if (c != '0' && c != '1')		
					{
						// Drop if not Protocol '0' or '1' - this should eliminate almost all noise packets

						TNC->InPacket = 0;
						break;
					}
				}
				TNC->DataBuffer[TNC->DataLen++] = c;
			}

			if (TNC->DataLen > 520)
				TNC->DataLen--;			// Protect Buffer

		}
		Len--;
	}
}
VOID CheckFLDigiData(struct TNCINFO * TNC)
{
	UCHAR * Input = &TNC->DataBuffer[0];
	int Len = TNC->DataLen - 4;		// Not including CRC
	unsigned short CRC;
	char crcstring[6];

	if (Len < 0)
		return;

	TNC->DataBuffer[TNC->DataLen] = 0;

	// RAW format message, either from ARQ Scoket or RAW KISS

	// Check Checksum

	CRC = CalcCRC(Input , Len);

	sprintf(crcstring, "%04X", CRC);

	if (memcmp(&Input[Len], crcstring, 4) !=0)
	{
		// CRC Error - could just be noise

//		Debugprintf("%s %s", crcstring, Input);
		return;
	}
	ProcessFLDigiData(TNC, &Input[3], Len - 3, Input[2], TRUE);		// From RAW 
}
/*
VOID ProcessARQPacket(struct PORTCONTROL * PORT, MESSAGE * Buffer)
{
	// ARQ Packet from KISS-Like Hardware

	struct TNCINFO * TNC = TNCInfo[PORT->PORTNUMBER];
	UCHAR * Input;
	int Len;

	if (TNC == NULL)
	{
		// Set up TNC info

		TNC = TNCInfo[PORT->PORTNUMBER] = zalloc(sizeof(struct TNCINFO));
		TNC->ARQInfo = zalloc(sizeof(struct ARQINFO)); 
		TNC->FLInfo = zalloc(sizeof(struct FLINFO)); 

		TNC->Timeout = 50;		// Default retry = 10 seconds
		TNC->Retries = 6;		// Default Retries
		TNC->Window = 16;
	}

	Input = &Buffer->DEST[0];
	Len = Buffer->LENGTH - 7;	// Not including CRC
	
	// Look for attach on any call

	ProcessFLDigiData(TNC, Input, Len);
}
*/
static int Stuff(UCHAR * inbuff, UCHAR * outbuff, int len)
{
	int i, txptr = 0;
	UCHAR c;
	UCHAR * ptr = inbuff;

	// DLE Escape DLE, SOH, EOT

	for (i = 0; i < len; i++)
	{
		c = *(ptr++);

//		if (c == 0 || c == DLE || c == SOH || c == EOT)
		if (c < 32 && c != 10 && c != 13 && c != 8)
		{
			outbuff[txptr++] = DLE;

			// if between 0 and 0x1F, Add 40,
			// if > x80 and less than 0xa0 subtract 20
			
			c += 0x40;
		}
		outbuff[txptr++]=c;
	}

	return txptr;
}


static int UnStuff(UCHAR * inbuff, int len)
{
	int i, txptr = 0;
	UCHAR c;
	UCHAR * outbuff = inbuff;
	UCHAR * ptr = inbuff;

	// This unstuffs into the input buffer

	for (i = 0; i < len; i++)
	{
		c = *(ptr++);

		if (c == DLE)
		{
			c = *(ptr++);
			i++;

			// if between 0x40 and 0x5F, subtract 0x40,
			// else add 0x20 (so we can send chars 80-9f without a double DLE)
			
			if (c < 0x60)
				c -= 0x40;
			else
				c += 0x20;
		}
		outbuff[txptr++] = c;
	}

	return txptr;
}

unsigned int crcval = 0xFFFF;

void update(char c)
{
	int i;
	
	crcval ^= c & 255;
    for (i = 0; i < 8; ++i)
	{
        if (crcval & 1)
            crcval = (crcval >> 1) ^ 0xA001;
        else
            crcval = (crcval >> 1);
    }
}
	
unsigned int CalcCRC(UCHAR * ptr, int Len)
{
	int i;
	
	crcval = 0xFFFF;
	for (i = 0; i < Len; i++)
	{
		update(*ptr++);
	}
	return crcval;
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
VOID ProcessFLDigiData(struct TNCINFO * TNC, UCHAR * Input, int Len, char Channel, BOOL RAW)
{
	UINT * buffptr;
	int Stream = 0;
	struct STREAMINFO * STREAM = &TNC->Streams[0];
	char CTRL = Input[0];
	struct ARQINFO * ARQ = TNC->ARQInfo;
	struct FLINFO *	FL = TNC->FLInfo;

	int SendLen;
	char Reply[80];


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

		if (FL->CONOK == FALSE)
			return;

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
		}

		if (App > 31)
			if (strcmp(TNC->NodeCall, call2) !=0)
				return;				// Not Appl or Port/Node Call

		ptr =  strtok_s(NULL, " ", &context);
		FarStream = *ptr;
		ptr =  strtok_s(NULL, " ", &context);
		BlockSize = atoi(ptr);

		if (ARQ->ARQState)
		{
			// We have already received a connect request - just ACK it

			goto AckConnectRequest;
		}

		// Get a Session

		SuspendOtherPorts(TNC);

		ProcessIncommingConnect(TNC, call1, 0, FALSE);
				
		SESS = TNC->PortRecord->ATTACHEDSESSIONS[0];

		strcpy(STREAM->MyCall, call2);
		STREAM->ConnectTime = time(NULL); 
		STREAM->BytesRXed = STREAM->BytesTXed = STREAM->BytesAcked = STREAM->BytesResent = 0;
		
		if (TNC->RIG && TNC->RIG != &TNC->DummyRig && strcmp(TNC->RIG->RigName, "PTT"))
		{
			sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound Freq %s", TNC->Streams[0].RemoteCall, call2, TNC->RIG->Valchar);
			SESS->Frequency = (atof(TNC->RIG->Valchar) * 1000000.0) + 1500;		// Convert to Centre Freq
			SESS->Mode = TNC->WL2KMode;
		}
		else
		{
			sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound", TNC->Streams[0].RemoteCall, call2);
			if (WL2K)
			{
				SESS->Frequency = WL2K->Freq;
				SESS->Mode = WL2K->mode;
			}
		}
			
		if (WL2K)
			strcpy(SESS->RMSCall, WL2K->RMSCall);

		SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

		strcpy(TNC->WEB_PROTOSTATE, "Connect Pending");
		SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

		memset(ARQ, 0, sizeof(struct ARQINFO));		// Reset ARQ State
		ARQ->FarStream = FarStream;
		ARQ->TXSeq = ARQ->TXLastACK = 63;			// Last Sent
		ARQ->RXHighest = ARQ->RXNoGaps = 63;		// Last Received
		ARQ->ARQState = ARQ_ACTIVE;
		ARQ->OurStream = (rand() % 78) + 49;		// To give some protection against other stuff on channel	
		ARQ->FarStream = FarStream;						// Not Yet defined
		if (strcmp(port1, "1025") == 0)
		{
			FL->FLARQ = TRUE;						// From FLARQ
			ARQ->OurStream = '8';					// FLARQ Ignores what we send
		}
		else
			FL->FLARQ = FALSE;						// From other app (eg BPQ)

		FL->RAW = RAW;

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


		ARQ->ARQTimer = 10;			// To force CTEXT to be Queued
		
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
					SendARQData(TNC, buffptr);
				}
			}
		}

		if (STREAM->NeedDisc)
		{
			// Send Not Avail 

			buffptr = GetBuff();
			if (buffptr)
			{
				buffptr[1] = sprintf((char *)&buffptr[2], "Application Not Available\n"); 
				SendARQData(TNC, buffptr);
			}
		}

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

		if (TNC->RIG)
			sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound Freq %s",  STREAM->MyCall, STREAM->RemoteCall, TNC->RIG->Valchar);
		else
			sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound", STREAM->MyCall, STREAM->RemoteCall);
			
		SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

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
		SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

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
		ProcessARQStatus(TNC, ARQ, &Input[1]);

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
		SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);
	
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

		if (STREAM->Disconnecting)		// 
			FLReleaseTNC(TNC);

		STREAM->Disconnecting = FALSE;

		strcpy(TNC->WEB_PROTOSTATE, "Disconncted");
		SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

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

			if (TNC->FLInfo->KISSMODE)
				Len -= 1;
			else
				Len = UnStuff(&Input[1], Len - 1);

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


VOID SendARQData(struct TNCINFO * TNC, UINT * Buffer)
{
	// Send Data, saving a copy until acked.

	struct ARQINFO * ARQ = TNC->ARQInfo;
	struct FLINFO *	FL = TNC->FLInfo;
	struct STREAMINFO * STREAM = &TNC->Streams[0];


	UCHAR TXBuffer[300];
	SOCKET sock = TNC->WINMORDataSock;
	int SendLen;
	UCHAR * ptr;
	int Origlen = Buffer[1];
	int Stuffedlen;
	
	ARQ->TXSeq++;
	ARQ->TXSeq &= 63;
	
	SendLen = sprintf(TXBuffer, "%c", ARQ->TXSeq + 32);

	ptr = (UCHAR *)&Buffer[2];			// Start of data;

	ptr[Buffer[1]] = 0;

	if (memcmp(ptr, "ARQ:", 4) == 0)
	{
		// FLARQ Mail/FIle transfer. Turn off CR > LF translate (used for terminal mode)

		FL->FLARQ = FALSE;
	}

	if (FL->FLARQ)
	{
		// Terminal Mode. Need to convert CR to LF so it displays in FLARQ Window
		
		ptr = strchr(ptr, 13);
	
		while (ptr)
		{
			*(ptr++) = 10;			// Replace CR with LF
			ptr = strchr(ptr, 13);
		}
	}

	if (TNC->FLInfo->KISSMODE)
	{
		memcpy(&TXBuffer[SendLen], (UCHAR *)&Buffer[2], Origlen);
		SendLen += Origlen;
	}
	else
	{
		Stuffedlen = Stuff((UCHAR *)&Buffer[2], &TXBuffer[SendLen], Origlen);
		SendLen += Stuffedlen;
	}

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
		SendPacket(TNC, TXBuffer, SendLen);
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

	struct ARQINFO * ARQ = TNC->ARQInfo;

	SendLen = sprintf(Reply, "d%s:90", TNC->Streams[0].MyCall); 

	SaveAndSend(TNC, ARQ, TNC->WINMORDataSock, Reply, SendLen);
	ARQ->ARQTimerState = ARQ_DISC;

	strcpy(TNC->WEB_PROTOSTATE, "Disconncting");
	SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);
}

VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	TidyClose(TNC, Stream);			// I don't think Hostmode has a DD
}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
	FLReleaseTNC(TNC);
}

VOID FLReleaseTNC(struct TNCINFO * TNC)
{
	// Set mycall back to Node or Port Call, and Start Scanner

	UCHAR TXMsg[1000];

	strcpy(TNC->WEB_TNCSTATE, "Free");
	SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

	// if a default Modem is defined, select it

	if (TNC->FLInfo->DefaultMode[0])
	{
		char txbuff[80];
				
		if (TNC->FLInfo->KISSMODE)
		{
			sprintf(txbuff, "WFF:%d MODEM:%s MODEM: WFF:", TNC->FLInfo->DefaultFreq, TNC->FLInfo->DefaultMode);
			SendKISSCommand(TNC, txbuff);
		}
		else
		{
			SendXMLCommand(TNC, "modem.set_by_name", TNC->FLInfo->DefaultMode, 'S');
			SendXMLCommand(TNC, "modem.set_carrier", TNC->FLInfo->DefaultFreq, 'I');
		}
	}
	//	Start Scanner
				
	sprintf(TXMsg, "%d SCANSTART 15", TNC->Port);

	Rig_Command(-1, TXMsg);

	ReleaseOtherPorts(TNC);

}
VOID QueueAndSend(struct TNCINFO * TNC, struct ARQINFO * ARQ, SOCKET sock, char * Msg, int MsgLen)
{
	// Queue to be sent after TXDELAY

	memcpy(ARQ->TXMsg, Msg, MsgLen + 1);
	ARQ->TXLen = MsgLen;
	ARQ->TXDelay = 15;					// Try 1500 ms
}

VOID SaveAndSend(struct TNCINFO * TNC, struct ARQINFO * ARQ, SOCKET sock, char * Msg, int MsgLen)
{
	// Used for Messages that need a reply. Save, send and set timeout

	memcpy(ARQ->LastMsg, Msg, MsgLen + 1);	// Include Null
	ARQ->LastLen = MsgLen;

	// Delay the send for a short while Just use the timeout code

//	SendPacket(sock, Msg, MsgLen, 0);
	ARQ->ARQTimer = 1;					// Try 500 ms
	ARQ->Retries = TNC->Retries + 1;	// First timout is rthe real send

	return;
}


VOID ARQTimer(struct TNCINFO * TNC)
{
	struct ARQINFO * ARQ = TNC->ARQInfo;
	UINT * buffptr;
	struct STREAMINFO * STREAM = &TNC->Streams[0];
	int SendLen;
	char Reply[80];
	struct FLINFO *	FL = TNC->FLInfo;

	//Send frames, unless held by TurnroundTimer or Window

	int Outstanding;

	// Use new BUSY: poll to detect busy state

	if (FL->TX == FALSE)
		SendKISSCommand(TNC, "BUSY:");					// Send every poll for now - may need to optimize later


/*
// Use Received chars as a rough channel active indicator

	FL->BusyTimer++;

	if (FL->BusyTimer > 4)
	{
		FL->BusyTimer = 0;
		
		if (FL->BusyCounter > 2)		// 2 chars in last .3 secs
			FL->Busy = TRUE;
		else
			FL->Busy = FALSE;

		if (FL->TX)
			strcpy(TNC->WEB_CHANSTATE, "TX");
		else
			if (FL->Busy)
				strcpy(TNC->WEB_CHANSTATE, "Busy");
			else
				strcpy(TNC->WEB_CHANSTATE, "Idle");

		FL->BusyCounter = 0;

		SetWindowText(TNC->xIDC_CHANSTATE, TNC->WEB_CHANSTATE);
	}

*/	//	TXDelay is used as a turn round delay for frames that don't have to be retried. It doesn't
	//	need to check for busy (or anything else (I think!)

	if (ARQ->TXDelay)
	{
		ARQ->TXDelay--;

		if (ARQ->TXDelay)
			return;

		SendPacket(TNC, ARQ->TXMsg, ARQ->TXLen);
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

			if (Outstanding > ARQ->TXWindow)
				break;
		
			buffptr = Q_REM(&STREAM->BPQtoPACTOR_Q);
			SendARQData(TNC, buffptr);
		}

		ARQ->ARQTimer--;

		if (ARQ->ARQTimer > 0)
			return;					// Timer Still Running
	
		// No more data available - send poll 

		SendLen = sprintf(Reply, "p%s", TNC->Streams[0].MyCall);

		ARQ->ARQTimerState = ARQ_WAITACK;

		// This is one message that should not be queued so it is sent straiget after data

//		Debugprintf("Sending Poll");

		memcpy(ARQ->LastMsg, Reply, SendLen + 1);
		ARQ->LastLen = SendLen;

		SendPacket(TNC, Reply, SendLen);
		
		ARQ->ARQTimer = TNC->Timeout;
		ARQ->Retries = TNC->Retries;

		strcpy(TNC->WEB_PROTOSTATE, "Wait ACK");
		SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

		return;
	
	}

	// TrunroundTimer is used to allow time for far end to revert to RX

	if (ARQ->TurnroundTimer  && !FL->Busy)
		ARQ->TurnroundTimer--;

	if (ARQ->TurnroundTimer == 0)
	{
		while (STREAM->BPQtoPACTOR_Q)
		{
			Outstanding = ARQ->TXSeq - ARQ->TXLastACK;
		
			if (Outstanding < 0)
				Outstanding += 64;

			TNC->PortRecord->FramesQueued = Outstanding + STREAM->BPQtoPACTOR_Q + 1; // Make sure busy is reported to BBS

			if (Outstanding > ARQ->TXWindow)
				break;
		
			buffptr = Q_REM(&STREAM->BPQtoPACTOR_Q);
			SendARQData(TNC, buffptr);
		}
	}

	if (ARQ->ARQTimer)
	{
		if (FL->TX || FL->Busy)
		{
			// Only decrement if running send poll timer
			
			if (ARQ->ARQTimerState != ARQ_WAITDATA)
				return;
		}

		ARQ->ARQTimer--;
		{
			if (ARQ->ARQTimer)
				return;					// Timer Still Running
		}

		ARQ->Retries--;

		if (ARQ->Retries)
		{
			// Retry Current Message

			SendPacket(TNC, ARQ->LastMsg, ARQ->LastLen);
			ARQ->ARQTimer = TNC->Timeout + (rand() % 30);

			return;
		}

		// Retried out.

		switch (ARQ->ARQTimerState)
		{
		case ARQ_WAITDATA:

			// No more data available - send poll 

			SendLen = sprintf(Reply, "p%s", TNC->Streams[0].MyCall);

			ARQ->ARQTimerState = ARQ_WAITACK;

			// This is one message that should not be queued so it is sent straiget after data

			memcpy(ARQ->LastMsg, Reply, SendLen + 1);
			ARQ->LastLen = SendLen;

			SendPacket(TNC, Reply, SendLen);
		
			ARQ->ARQTimer = TNC->Timeout;
			ARQ->Retries = TNC->Retries;

			strcpy(TNC->WEB_PROTOSTATE, "Wait ACK");
			SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

			return;
	
		case ARQ_CONNECTING:

			// Report Connect Failed, and drop back to command mode

			buffptr = GetBuff();

			if (buffptr)
			{
				buffptr[1] = sprintf((UCHAR *)&buffptr[2], "FLDigi} Failure with %s\r", STREAM->RemoteCall);
				C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
			}
	
			// Send Disc to TNC in case it got the Connects, but we missed the ACKs

			TidyClose(TNC, 0);
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
			SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

			strcpy(TNC->WEB_PROTOSTATE, "Disconncted");
			SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

			break;

		}
	}
}

VOID ProcessARQStatus(struct TNCINFO * TNC, struct ARQINFO * ARQ, char * Input)
{
	// Release any acked frames and resend any outstanding

	int LastInSeq = Input[1] - 32;
	int LastRXed = Input[2] - 32;
	int FirstUnAcked = ARQ->TXLastACK;
	int n = strlen(Input) - 3;
	char * ptr;
	int NexttoResend;
	int First, Last, Outstanding;
	UINT * Buffer;
	struct STREAMINFO * STREAM = &TNC->Streams[0];
	int Acked = 0;

	// First status is an ack of Connect ACK

	if (ARQ->ARQTimerState == ARQ_CONNECTACK)
	{
		ARQ->Retries = 0;
		ARQ->ARQTimer = 0;
		ARQ->ARQTimerState = 0;

		strcpy(TNC->WEB_PROTOSTATE, "Connected");
		SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);
	}

	//	Release all up to LastInSeq
	
	while (FirstUnAcked != LastInSeq)
	{
		FirstUnAcked++;
		FirstUnAcked &= 63;

		Buffer = ARQ->TXHOLDQ[FirstUnAcked];

		if (Buffer)
		{
//			Debugprintf("Acked %d", FirstUnAcked);
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
		SetWindowText(TNC->xIDC_PROTOSTATE, TNC->WEB_PROTOSTATE);

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
//				Debugprintf("Acked %d", FirstUnAcked);
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
//			Debugprintf("Acked %d", FirstUnAcked);
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

//			Debugprintf("Resend %d", First);

			STREAM->BytesResent += Buffer[1];
		
			SendLen = sprintf(TXBuffer, "%c", First + 32);

			if (TNC->FLInfo->KISSMODE)
			{
				memcpy(&TXBuffer[SendLen], (UCHAR *)&Buffer[2], Buffer[1]);
				SendLen += Buffer[1];
			}
			else
				SendLen += Stuff((UCHAR *)&Buffer[2], &TXBuffer[SendLen], Buffer[1]);

			TXBuffer[SendLen] = 0;

			SendPacket(TNC, TXBuffer, SendLen);

			ARQ->ARQTimer = 10;			// wait up to 1 sec for more data before polling
			ARQ->Retries = 1;
			ARQ->ARQTimerState = ARQ_WAITDATA;

			if (Acked == 0)
			{
				// Nothing acked by this statis message

				Acked = 0;					// Dont count more thna once
				ARQ->NoAckRetries++;
				if (ARQ->NoAckRetries > TNC->Retries)
				{
					// Too many retries - just disconnect

					TidyClose(TNC, 0);
					return;
				}
			}
		}
	}

	UpdateStatsLine(TNC, STREAM);
}

VOID FLSlowTimer(struct TNCINFO * TNC)
{
	struct FLINFO * FL = TNC->FLInfo;

	// Entered every 10 secs
	
	if (FL->KISSMODE)
	{
		if (FL->Responding)
			FL->Responding--;

		if (FL->Responding == 0)
		{
			TNC->TNCOK = FALSE;
			TNC->CONNECTED = FALSE;

			sprintf(TNC->WEB_COMMSSTATE, "Connection to FLDIGI lost");
			SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

			// Set basic params till it responds
		}

		FL->CmdControl++;

		if (FL->CmdControl > 5)			// Every Minute
		{
			FL->CmdControl = 0;
			
			SendKISSCommand(TNC, "FLSTAT: MODEM: WFF:");
		}

		SendKISSCommand(TNC, "TRXS: TXBUF:");	// In case TX/RX report is missed
	}
}

static int ProcessXMLData(int port)
{
	unsigned int bytes;
	int i;
	char ErrMsg[255];
	char Message[500];
	struct TNCINFO * TNC = TNCInfo[port];
	struct FLINFO *	FL = TNC->FLInfo;
	char * ptr1, * ptr2, *ptr3;

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

		i=sprintf(ErrMsg, "FlDigi Connection closed for BPQ Port %d\n", port);
		WritetoConsole(ErrMsg);

		TNC->CONNECTED = FALSE;
		if (TNC->Streams[0].Attached)
			TNC->Streams[0].ReportDISC = TRUE;

		return (0);
	}

	//	Have some data. Assume for now we get a whole packet

	if (TNC->InternalCmd)
	{
		ULONG * buffptr = GetBuff();
	
		TNC->InternalCmd = FALSE;

		ptr1 = strstr(Message, "<value>");

		if (ptr1)
		{
			ptr1 += 7;
			ptr2 = strstr(ptr1, "</value>");
			if (ptr2) *ptr2 = 0;

			ptr3 = strstr(ptr1, "<i4>");

			if (ptr3)
			{
				ptr1 = ptr3 + 4;
				ptr2 = strstr(ptr1, "</i4>");
				if (ptr2) *ptr2 = 0;
			}

			if (buffptr)
			{
				buffptr[1] = sprintf((UCHAR *)&buffptr[2], "FLDIGI} Ok Was %s\r", ptr1);
				C_Q_ADD(&TNC->Streams[0].PACTORtoBPQ_Q, buffptr);
			}
		}
	
		return 0;
	}


	ptr1 = strstr(Message, "<value>");

	if (ptr1)
	{
		ptr1 += 7;
		ptr2 = strstr(ptr1, "</value>");
		if (ptr2) *ptr2 = 0;

		ptr2 = strstr(ptr1, "<string>");

		if (ptr2)
		{
			ptr2 += 8;
			ptr1 = ptr2;
			ptr2 = strstr(ptr1, "</string>");
			if (ptr2) *ptr2 = 0;
		}

		if (strcmp(FL->LastXML, "modem.get_name") == 0)
		{
			strcpy(TNC->WEB_MODE, ptr1);
			SetWindowText(TNC->xIDC_MODE, ptr1);
		}
		else if (strcmp(FL->LastXML, "main.get_trx_state") == 0)
		{
			if (strcmp(ptr1, "TX") == 0)
				FL->TX = TRUE;
			else
				FL->TX = FALSE;


			if (FL->TX)
				strcpy(TNC->WEB_CHANSTATE, "TX");
			else
				if (FL->Busy)
					strcpy(TNC->WEB_CHANSTATE, "Busy");
				else
					strcpy(TNC->WEB_CHANSTATE, "Idle");

			SetWindowText(TNC->xIDC_CHANSTATE, TNC->WEB_CHANSTATE);
		}
		else if (strcmp(FL->LastXML, "main.get_squelch") == 0)
		{
/*
			if (_memicmp(Buffer, "BUSY TRUE", 9) == 0)
	{	
		TNC->BusyFlags |= CDBusy;
		TNC->Busy = TNC->BusyHold * 10;				// BusyHold  delay

		SetWindowText(TNC->xIDC_CHANSTATE, "Busy");
		strcpy(TNC->WEB_CHANSTATE, "Busy");

		TNC->WinmorRestartCodecTimer = time(NULL);
*/
			return 0;
	}
/*
	if (_memicmp(Buffer, "BUSY FALSE", 10) == 0)
	{
		TNC->BusyFlags &= ~CDBusy;
		if (TNC->BusyHold)
			strcpy(TNC->WEB_CHANSTATE, "BusyHold");
		else
			strcpy(TNC->WEB_CHANSTATE, "Clear");

		SetWindowText(TNC->xIDC_CHANSTATE, TNC->WEB_CHANSTATE);
		TNC->WinmorRestartCodecTimer = time(NULL);
		return;
	}
*/

	}
	
	return (0);

}



char MsgHddr[] = "POST /RPC2 HTTP/1.1\r\n"
					"User-Agent: XMLRPC++ 0.8\r\n"
					"Host: 127.0.0.1:7362\r\n"
					"Content-Type: text/xml\r\n"
					"Content-length: %d\r\n"
					"\r\n%s";

char Req[] = 	"<?xml version=\"1.0\"?>\r\n"
					"<methodCall><methodName>%s</methodName>\r\n"
					"%s"
					"</methodCall>\r\n";


VOID SendXMLCommand(struct TNCINFO * TNC, char * Command, char * Value, char ParamType)
{
	int Len;
	char ReqBuf[512];
	char SendBuff[512];
	struct FLINFO *	FL = TNC->FLInfo;
	struct ARQINFO * ARQ = TNC->ARQInfo;
	char ValueString[256] ="";

	if (!TNC->CONNECTED || TNC->FLInfo->KISSMODE)
		return;

	if (Value)
		if (ParamType == 'S')
			sprintf(ValueString, "<params><param><value><string>%s</string></value></param></params\r\n>", Value);
		else
			sprintf(ValueString, "<params><param><value><i4>%s</i4></value></param></params\r\n>", Value);

	strcpy(FL->LastXML, Command);
	Len = sprintf(ReqBuf, Req, FL->LastXML, ValueString);
	Len = sprintf(SendBuff, MsgHddr, Len, ReqBuf);
	send(TNC->WINMORSock, SendBuff, Len, 0); 
	return;
}

VOID SendXMLPoll(struct TNCINFO * TNC)
{
	int Len;
	char ReqBuf[256];
	char SendBuff[256];
	struct FLINFO *	FL = TNC->FLInfo;
	struct ARQINFO * ARQ = TNC->ARQInfo;

	if (!TNC->CONNECTED)
		return;

	if (TNC->FLInfo->KISSMODE)
		return;

	if (ARQ->ARQTimer)
	{
		// if timer is running, poll fot TX State
		
		strcpy(FL->LastXML, "main.get_trx_state");
		Len = sprintf(ReqBuf, Req, FL->LastXML, "");
		Len = sprintf(SendBuff, MsgHddr, Len, ReqBuf);
		send(TNC->WINMORSock, SendBuff, Len, 0); 
		return;
	}

	FL->XMLControl++;


	if (FL->XMLControl > 9)
	{
		FL->XMLControl = 0;
		strcpy(FL->LastXML, "modem.get_name");
	}
	else
	{
		if (FL->XMLControl == 5)
			strcpy(FL->LastXML, "main.get_trx_state");
		else
			return;
	}

	Len = sprintf(ReqBuf, Req, FL->LastXML, "");
	Len = sprintf(SendBuff, MsgHddr, Len, ReqBuf);
	send(TNC->WINMORSock, SendBuff, Len, 0); 
}

//  sudo add-apt-repository ppa:kamalmostafa/fldigi


