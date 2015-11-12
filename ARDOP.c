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
//	Interface to allow G8BPQ switch to use ARDOP Virtual TNC


#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <stdio.h>
#include <time.h>

#include "CHeaders.h"

#ifdef WIN32
#include <Psapi.h>
#endif

int (WINAPI FAR *GetModuleFileNameExPtr)();
int (WINAPI FAR *EnumProcessesPtr)();

#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02

#include "bpq32.h"

#include "tncinfo.h"


#define WSA_ACCEPT WM_USER + 1
#define WSA_DATA WM_USER + 2
#define WSA_CONNECT WM_USER + 3

static int Socket_Data(int sock, int error, int eventcode);

int ARDOPKillTNC(struct TNCINFO * TNC);
int ARDOPRestartTNC(struct TNCINFO * TNC);
int KillPopups(struct TNCINFO * TNC);
VOID MoveWindows(struct TNCINFO * TNC);
int SendReporttoWL2K(struct TNCINFO * TNC);
char * CheckAppl(struct TNCINFO * TNC, char * Appl);
int DoScanLine(struct TNCINFO * TNC, char * Buff, int Len);
BOOL KillOldTNC(char * Path);
int ARDOPSendData(struct TNCINFO * TNC, char * Buff, int Len);
VOID ARDOPSendCommand(struct TNCINFO * TNC, char * Buff, BOOL Queue);
VOID SendToTNC(struct TNCINFO * TNC, UCHAR * Encoded, int EncLen);
VOID ARDOPProcessDataPacket(struct TNCINFO * TNC, UCHAR * Type, UCHAR * Data, int Length);

#ifndef LINBPQ
BOOL CALLBACK EnumARDOPWindowsProc(HWND hwnd, LPARAM  lParam);
#endif

static char ClassName[]="ARDOPSTATUS";
static char WindowTitle[] = "ARDOP";
static int RigControlRow = 165;

#define WINMOR
#define NARROWMODE 21
#define WIDEMODE 22

#ifndef LINBPQ
#include <commctrl.h>
#endif

extern char * PortConfig[33];
extern int SemHeldByAPI;

static RECT Rect;

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

static int ProcessLine(char * buf, int Port);

unsigned long _beginthread( void( *start_address )(), unsigned stack_size, int arglist);

// RIGCONTROL COM60 19200 ICOM IC706 5e 4 14.103/U1w 14.112/u1 18.1/U1n 10.12/l1

int GenCRC16(unsigned char * Data, unsigned short length)
{
	// For  CRC-16-CCITT =    x^16 + x^12 +x^5 + 1  intPoly = 1021 Init FFFF
    // intSeed is the seed value for the shift register and must be in the range 0-&HFFFF

	int intRegister = 0xffff; //intSeed
	int i,j;
	int Bit;
	int intPoly = 0x8810;	//  This implements the CRC polynomial  x^16 + x^12 +x^5 + 1

	for (j = 0; j <  (length); j++)	
	{
		int Mask = 0x80;			// Top bit first

		for (i = 0; i < 8; i++)	// for each bit processing MS bit first
		{
			Bit = Data[j] & Mask;
			Mask >>= 1;

            if (intRegister & 0x8000)		//  Then ' the MSB of the register is set
			{
                // Shift left, place data bit as LSB, then divide
                // Register := shiftRegister left shift 1
                // Register := shiftRegister xor polynomial
                 
              if (Bit)
                 intRegister = 0xFFFF & (1 + (intRegister << 1));
			  else
                  intRegister = 0xFFFF & (intRegister << 1);
	
				intRegister = intRegister ^ intPoly;
			}
			else  
			{
				// the MSB is not set
                // Register is not divisible by polynomial yet.
                // Just shift left and bring current data bit onto LSB of shiftRegister
              if (Bit)
                 intRegister = 0xFFFF & (1 + (intRegister << 1));
			  else
                  intRegister = 0xFFFF & (intRegister << 1);
			}
		}
	}
 
	return intRegister;
}

BOOL checkcrc16(unsigned char * Data, unsigned short length)
{
	int intRegister = 0xffff; //intSeed
	int i,j;
	int Bit;
	int intPoly = 0x8810;	//  This implements the CRC polynomial  x^16 + x^12 +x^5 + 1

	for (j = 0; j <  (length - 2); j++)		// ' 2 bytes short of data length
	{
		int Mask = 0x80;			// Top bit first

		for (i = 0; i < 8; i++)	// for each bit processing MS bit first
		{
			Bit = Data[j] & Mask;
			Mask >>= 1;

            if (intRegister & 0x8000)		//  Then ' the MSB of the register is set
			{
                // Shift left, place data bit as LSB, then divide
                // Register := shiftRegister left shift 1
                // Register := shiftRegister xor polynomial
                 
              if (Bit)
                 intRegister = 0xFFFF & (1 + (intRegister << 1));
			  else
                  intRegister = 0xFFFF & (intRegister << 1);
	
				intRegister = intRegister ^ intPoly;
			}
			else  
			{
				// the MSB is not set
                // Register is not divisible by polynomial yet.
                // Just shift left and bring current data bit onto LSB of shiftRegister
              if (Bit)
                 intRegister = 0xFFFF & (1 + (intRegister << 1));
			  else
                  intRegister = 0xFFFF & (intRegister << 1);
			}
		}
	}

    if (Data[length - 2] == intRegister >> 8)
		if (Data[length - 1] == (intRegister & 0xFF))
			return TRUE;
   
	return FALSE;
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

	TNC = TNCInfo[BPQport] = malloc(sizeof(struct TNCINFO));
	memset(TNC, 0, sizeof(struct TNCINFO));

		TNC->InitScript = malloc(1000);
		TNC->InitScript[0] = 0;
	
		if (p_ipad == NULL)
			p_ipad = strtok(NULL, " \t\n\r");

		if (p_ipad == NULL) return (FALSE);
	
		p_port = strtok(NULL, " \t\n\r");
			
		if (p_port == NULL) return (FALSE);

		WINMORport = atoi(p_port);

		TNC->destaddr.sin_family = AF_INET;
		TNC->destaddr.sin_port = htons(WINMORport);
		TNC->Datadestaddr.sin_family = AF_INET;
		TNC->Datadestaddr.sin_port = htons(WINMORport+1);

		TNC->WINMORHostName = malloc(strlen(p_ipad)+1);

		if (TNC->WINMORHostName == NULL) return TRUE;

		strcpy(TNC->WINMORHostName,p_ipad);

		ptr = strtok(NULL, " \t\n\r");

		if (ptr)
		{
			if (_stricmp(ptr, "PTT") == 0)
			{
				ptr = strtok(NULL, " \t\n\r");

				if (ptr)
				{
					if (_stricmp(ptr, "CI-V") == 0)
						TNC->PTTMode = PTTCI_V;
					else if (_stricmp(ptr, "CAT") == 0)
						TNC->PTTMode = PTTCI_V;
					else if (_stricmp(ptr, "RTS") == 0)
						TNC->PTTMode = PTTRTS;
					else if (_stricmp(ptr, "DTR") == 0)
						TNC->PTTMode = PTTDTR;
					else if (_stricmp(ptr, "DTRRTS") == 0)
						TNC->PTTMode = PTTDTR | PTTRTS;

					ptr = strtok(NULL, " \t\n\r");
				}
			}
		}
		
		if (ptr)
		{
			if (_memicmp(ptr, "PATH", 4) == 0)
			{
				p_cmd = strtok(NULL, "\n\r");
				if (p_cmd) TNC->ProgramPath = _strdup(p_cmd);
//				if (p_cmd) TNC->ProgramPath = _strdup(_strupr(p_cmd));
			}
		}

		TNC->MaxConReq = 10;		// Default

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
				
			if ((_memicmp(buf, "CAPTURE", 7) == 0) || (_memicmp(buf, "PLAYBACK", 8) == 0))
			{}		// Ignore
			else
/*
			if (_memicmp(buf, "PATH", 4) == 0)
			{
				char * Context;
				p_cmd = strtok_s(&buf[5], "\n\r", &Context);
				if (p_cmd) TNC->ProgramPath = _strdup(p_cmd);
			}
			else
*/
			if (_memicmp(buf, "WL2KREPORT", 10) == 0)
				TNC->WL2K = DecodeWL2KReportLine(buf);
			else
			if (_memicmp(buf, "BUSYHOLD", 8) == 0)		// Hold Time for Busy Detect
				TNC->BusyHold = atoi(&buf[8]);

			else
			if (_memicmp(buf, "BUSYWAIT", 8) == 0)		// Wait time beofre failing connect if busy
				TNC->BusyWait = atoi(&buf[8]);

			else
			if (_memicmp(buf, "MAXCONREQ", 9) == 0)		// Hold Time for Busy Detect
				TNC->MaxConReq = atoi(&buf[9]);

			else
			if (_memicmp(buf, "STARTINROBUST", 13) == 0)
				TNC->StartInRobust = TRUE;
			
			else
			if (_memicmp(buf, "ROBUST", 6) == 0)
			{
				if (_memicmp(&buf[7], "TRUE", 4) == 0)
					TNC->Robust = TRUE;
				
				strcat (TNC->InitScript, buf);
			}
			else

			strcat (TNC->InitScript, buf);
		}


	return (TRUE);	
}



void ARDOPThread(int port);
VOID ARDOPProcessDataSocketData(int port);
int ConnecttoARDOP();
static VOID ARDOPProcessReceivedData(struct TNCINFO * TNC);
int V4ProcessReceivedData(struct TNCINFO * TNC);
VOID ARDOPReleaseTNC(struct TNCINFO * TNC);
VOID SuspendOtherPorts(struct TNCINFO * ThisTNC);
VOID ReleaseOtherPorts(struct TNCINFO * ThisTNC);
VOID WritetoTrace(struct TNCINFO * TNC, char * Msg, int Len);


#define MAXBPQPORTS 32

static time_t ltime;


static SOCKADDR_IN sinx; 
static SOCKADDR_IN rxaddr;

static int addrlen=sizeof(sinx);

unsigned short int compute_crc(unsigned char *buf,int len);

VOID ARDOPSendCommand(struct TNCINFO * TNC, char * Buff, BOOL Queue)
{
	// Encode and send to TNC. May be TCP or Serial

	// Command Formst is C:TEXT<CR><CRC>

	UINT * buffptr;
	int EncLen;
	unsigned short CRC;
	UCHAR * Encoded;

	buffptr = GetBuff();

	//	Have to save copy for possible retry (and possibly until previous 
	//	command is acked

	if (buffptr == NULL)
	{
		return;
	}

	Encoded = (UCHAR *)&buffptr[2];
	EncLen = sprintf(Encoded, "C:%s\r", Buff);

	CRC = GenCRC16(Encoded + 2, EncLen -2);			// Don't include c:

	Encoded[EncLen++] = CRC >> 8;
	Encoded[EncLen++] = CRC & 0xFF;

	buffptr[1] = EncLen;

	if (Queue)
	{
		if (TNC->BPQtoWINMOR_Q)
		{
			// Something already queued
		
			C_Q_ADD(&TNC->BPQtoWINMOR_Q, buffptr);
			return;
		}

		// Nothing on Queue, so OK to send now

		C_Q_ADD(&TNC->BPQtoWINMOR_Q, buffptr);
		SendToTNC(TNC, Encoded, EncLen);
		return;
	}

	SendToTNC(TNC, Encoded, EncLen);
	ReleaseBuffer(buffptr);

	return;
}

VOID SendToTNC(struct TNCINFO * TNC, UCHAR * Encoded, int EncLen)
{
	int SentLen;

	if (TNC->hDevice)
	{
		WriteCOMBlock(TNC->hDevice, Encoded, EncLen);
		return;
	}

	if(TNC->WINMORSock)
	{
		SentLen = send(TNC->WINMORSock, Encoded, EncLen, 0);
		
		if (SentLen != EncLen)
		{
			// WINMOR doesn't seem to recover from a blocked write. For now just reset
			
//			if (bytes == SOCKET_ERROR)
//			{
			int winerr=WSAGetLastError();
			char ErrMsg[80];
				
			sprintf(ErrMsg, "ARDOP Write Failed for port %d - error code = %d\r\n", TNC->Port, winerr);
			WritetoConsole(ErrMsg);
					
	
//				if (winerr != WSAEWOULDBLOCK)
//				{
		
			closesocket(TNC->WINMORSock);
					
			TNC->CONNECTED = FALSE;
			return;
		}
	}

//			else
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

//				closesocket(WINMORSock[MasterPort[port]]);
					
//				CONNECTED[MasterPort[port]]=FALSE;

//				return (0);
//			}
	
//			buffptr[1]=txlen-bytes;			// Bytes still to send

//			memcpy(buffptr+2,&txbuff[bytes],txlen-bytes);

//			C_Q_ADD(&BPQtoWINMOR_Q[MasterPort[port]],buffptr);
	
//			return (0);

}

int ARDOPSendData(struct TNCINFO * TNC, char * Buff, int Len)
{
	// Encode and send to TNC. May be TCP or Serial

	UINT * buffptr = GetBuff();
	int EncLen;
	unsigned short CRC;

	//	Have to save copy for possible retry (and possibly until previous 
	//	command is acked

	UCHAR * Encoded = (UCHAR *)&buffptr[2];
	UCHAR * Msg = (UCHAR *)&buffptr[2];

	if (buffptr == NULL)
		return 0;

	*(Encoded++) = 'D';
	*(Encoded++) = ':';
	*(Encoded++) = Len >> 8;
	*(Encoded++) = Len & 0xff;

	memcpy(Encoded, Buff, Len);
	Encoded += Len;

	EncLen = Len + 4;

	CRC = GenCRC16(Msg + 2, Len + 2);	// Don't include c:

	*(Encoded++) = CRC >> 8;
	*(Encoded++) = CRC & 0xFF;

	buffptr[1] = Len + 6;

	if (TNC->BPQtoWINMOR_Q)
	{
		// Something already queued
		
		C_Q_ADD(&TNC->BPQtoWINMOR_Q, buffptr);
		return Len;
	}

	// Nothing on Queue, so OK to send now

	C_Q_ADD(&TNC->BPQtoWINMOR_Q, buffptr);

	SendToTNC(TNC, Msg, Len + 6);

	return Len;
}


VOID ARDOPChangeMYC(struct TNCINFO * TNC, char * Call)
{
	UCHAR TXMsg[100];
	int datalen;

	if (strcmp(Call, TNC->CurrentMYC) == 0)
		return;								// No Change

	strcpy(TNC->CurrentMYC, Call);

//	ARDOPSendCommand(TNC, "CODEC FALSE");

	datalen = sprintf(TXMsg, "MYCALL %s", Call);
	ARDOPSendCommand(TNC, TXMsg, TRUE);

//	ARDOPSendCommand(TNC, "CODEC TRUE");
//	TNC->StartSent = TRUE;

//	ARDOPSendCommand(TNC, "MYCALL", TRUE);
}

static int ExtProc(int fn, int port,unsigned char * buff)
{
	int datalen;
	UINT * buffptr;
	char txbuff[500];
	unsigned int bytes,txlen=0;
	int Param;
	HKEY hKey=0;
	struct TNCINFO * TNC = TNCInfo[port];
	struct STREAMINFO * STREAM = &TNC->Streams[0];
	struct ScanEntry * Scan;

	if (TNC == NULL)
		return 0;							// Port not defined

	if (TNC->CONNECTED == 0)
	{
		// clear Q if not connected

		while(TNC->BPQtoWINMOR_Q)
		{
			buffptr = Q_REM(&TNC->BPQtoWINMOR_Q);

			if (buffptr)
				ReleaseBuffer(buffptr);
		}
	}


	switch (fn)
	{
	case 1:				// poll

		while (TNC->PortRecord->UI_Q)
		{
			int datalen;
			char * Buffer;
			char FECMsg[256] = "";
			char Call[12] = "           ";		
			struct _MESSAGE * buffptr;
			
			buffptr = Q_REM(&TNC->PortRecord->UI_Q);

			if (TNC->CONNECTED == 0)
			{
				// discard if not connected

				ReleaseBuffer(buffptr);
				continue;
			}
	
			datalen = buffptr->LENGTH - 7;
			Buffer = &buffptr->DEST[0];		// Raw Frame
			Buffer[datalen] = 0;

			// Frame has ax.25 format header. Convert to Text

			ConvFromAX25(Buffer + 7, Call);		// Origin
			strlop(Call, ' ');
			strcat(FECMsg, Call);
			strcat(FECMsg, ">");

			ConvFromAX25(Buffer, Call);			// Dest
			strlop(Call, ' ');
			strcat(FECMsg, Call);

			Buffer += 14;						// TO Digis
			datalen -= 7;

			while ((Buffer[-1] & 1) == 0)
			{
				Call[0] = ',';
				ConvFromAX25(Buffer, &Call[1]);
				strlop(&Call[1], ' ');
				strcat(FECMsg, Call);
				Buffer += 7;	// End of addr
				datalen -= 7;
			}

			strcat(FECMsg, "|");

			if (Buffer[0] == 3)				// UI
			{
				Buffer += 2;
				datalen -= 2;

			}
			strcat(FECMsg, Buffer);

			ARDOPSendData(TNC, FECMsg, strlen(FECMsg));

			if (TNC->BusyFlags)
				TNC->FECPending = 1;
			else
				ARDOPSendCommand(TNC,"FECSEND TRUE", TRUE);
		
			ReleaseBuffer((UINT *)buffptr);
		}

		if (TNC->Busy)							//  Count down to clear
		{
			if ((TNC->BusyFlags & CDBusy) == 0)	// TNC Has reported not busy
			{
				TNC->Busy--;
				if (TNC->Busy == 0)
					SetWindowText(TNC->xIDC_CHANSTATE, "Clear");
					strcpy(TNC->WEB_CHANSTATE, "Clear");
			}
		}

		if (TNC->BusyDelay)
		{
			// Still Busy?

			if (InterlockedCheckBusy(TNC) == FALSE)
			{
				// No, so send

				ARDOPSendCommand(TNC, TNC->ConnectCmd, TRUE);
				TNC->Streams[0].Connecting = TRUE;

				memset(TNC->Streams[0].RemoteCall, 0, 10);
				memcpy(TNC->Streams[0].RemoteCall, &TNC->ConnectCmd[8], strlen(TNC->ConnectCmd)-10);

				sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
				SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

				free(TNC->ConnectCmd);
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

					C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
					free(TNC->ConnectCmd);

					sprintf(TNC->WEB_TNCSTATE, "In Use by %s", TNC->Streams[0].MyCall);
					SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

				}
			}
		}

		if (TNC->HeartBeat++ > 600 || (TNC->Streams[0].Connected && TNC->HeartBeat > 50))			// Every Minute unless connected
		{
			TNC->HeartBeat = 0;

			if (TNC->CONNECTED)
			{
				// Probe link

				if (TNC->Streams[0].Connecting || TNC->Streams[0].Connected)
					fn =fn; //ARDOPSendCommand(TNC, "MODE", TRUE);
				else
				{
//					if (time(NULL) - TNC->WinmorRestartCodecTimer > 300)	// 5 mins
//					{
//						ARDOPSendCommand(TNC, "CODEC FALSE", TRUE);
//						ARDOPSendCommand(TNC, "CODEC TRUE", TRUE);
//					}
//					else
						ARDOPSendCommand(TNC, "STATE", TRUE);
				}
			}
		}

		if (TNC->FECMode)
		{
			if (TNC->FECIDTimer++ > 6000)		// ID every 10 Mins
			{
				if (!TNC->Busy)
				{
					TNC->FECIDTimer = 0;
					ARDOPSendCommand(TNC, "SENDID", TRUE);
				}
			}
			if (TNC->FECPending)	// Check if FEC Send needed
			{
				if (!TNC->Busy)
				{
					TNC->FECPending = 0;
					ARDOPSendCommand(TNC,"FECSEND TRUE", TRUE);
				}
			}
		}

		if (STREAM->NeedDisc)
		{
			STREAM->NeedDisc--;

			if (STREAM->NeedDisc == 0)
			{
				// Send the DISCONNECT

				ARDOPSendCommand(TNC, "DISCONNECT", TRUE);
			}
		}

		if (TNC->DiscPending)
		{
			TNC->DiscPending--;

			if (TNC->DiscPending == 0)
			{
				// Too long in Disc Pending - Kill and Restart TNC

				if (TNC->WIMMORPID)
				{
					ARDOPKillTNC(TNC);
					ARDOPRestartTNC(TNC);
				}
			}
		}

		if (TNC->TimeSinceLast++ > 800)			// Allow 10 secs for Keepalive
		{
			// Restart TNC
		
			if (TNC->ProgramPath)
			{
				if (strstr(TNC->ProgramPath, "WINMOR TNC"))
				{
					struct tm * tm;
					char Time[80];
				
					TNC->Restarts++;
					TNC->LastRestart = time(NULL);

					tm = gmtime(&TNC->LastRestart);	
				
					sprintf_s(Time, sizeof(Time),"%04d/%02d/%02d %02d:%02dZ",
						tm->tm_year +1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);

					MySetWindowText(TNC->xIDC_RESTARTTIME, Time);
					strcpy(TNC->WEB_RESTARTTIME, Time);

					sprintf_s(Time, sizeof(Time),"%d", TNC->Restarts);
					MySetWindowText(TNC->xIDC_RESTARTS, Time);
					strcpy(TNC->WEB_RESTARTS, Time);
	
					ARDOPKillTNC(TNC);
					ARDOPRestartTNC(TNC);

					TNC->TimeSinceLast = 0;
				}
			}
		}

		if (TNC->PortRecord->ATTACHEDSESSIONS[0] && TNC->Streams[0].Attached == 0)
		{
			// New Attach

			int calllen;
			char Msg[80];

			TNC->Streams[0].Attached = TRUE;

			calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[0]->L4USER, TNC->Streams[0].MyCall);
			TNC->Streams[0].MyCall[calllen] = 0;

			// Stop Listening, and set MYCALL to user's call

			ARDOPSendCommand(TNC, "LISTEN FALSE", TRUE);
			ARDOPChangeMYC(TNC, TNC->Streams[0].MyCall);

			// Stop other ports in same group

			SuspendOtherPorts(TNC);

			sprintf(TNC->WEB_TNCSTATE, "In Use by %s", TNC->Streams[0].MyCall);
			MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

			// Stop Scanning

			sprintf(Msg, "%d SCANSTOP", TNC->Port);
	
			Rig_Command(-1, Msg);

		}

		if (TNC->Streams[0].Attached)
			CheckForDetach(TNC, 0, &TNC->Streams[0], TidyClose, ForcedClose, CloseComplete);

		if (TNC->Streams[0].ReportDISC)
		{
			TNC->Streams[0].ReportDISC = FALSE;
			buff[4] = 0;
			return -1;
		}

		if (TNC->CONNECTED == FALSE && TNC->CONNECTING == FALSE)
		{
			//	See if time to reconnect
		
			time(&ltime);
			if (ltime - TNC->lasttime > 9 )
			{
				ConnecttoARDOP(port);
				TNC->lasttime = ltime;
			}
		}
		
		// See if any frames for this port

		if (TNC->Streams[0].BPQtoPACTOR_Q)		//Used for CTEXT
		{
			UINT * buffptr = Q_REM(&TNC->Streams[0].BPQtoPACTOR_Q);
			txlen=buffptr[1];
			memcpy(txbuff,buffptr+2,txlen);
			bytes = ARDOPSendData(TNC, &txbuff[0], txlen);
			STREAM->BytesTXed += bytes;
			WritetoTrace(TNC, txbuff, txlen);
			ReleaseBuffer(buffptr);
		}


		if (TNC->WINMORtoBPQ_Q != 0)
		{
			buffptr=Q_REM(&TNC->WINMORtoBPQ_Q);

			datalen=buffptr[1];

			buff[4] = 0;						// Compatibility with Kam Driver
			buff[7] = 0xf0;
			memcpy(&buff[8],buffptr+2,datalen);	// Data goes to +7, but we have an extra byte
			datalen+=8;
			buff[5]=(datalen & 0xff);
			buff[6]=(datalen >> 8);
		
			ReleaseBuffer(buffptr);

			return (1);
		}

		return (0);

	case 2:				// send

		if (!TNC->CONNECTED)
		{
			// Send Error Response

			UINT * buffptr = GetBuff();

			if (buffptr == 0) return (0);			// No buffers, so ignore

			buffptr[1]=36;
			memcpy(buffptr+2,"No Connection to ARDOP TNC\r", 36);

			C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
			
			return 0;		// Don't try if not connected
		}

		if (TNC->Streams[0].BPQtoPACTOR_Q)		//Used for CTEXT
		{
			UINT * buffptr = Q_REM(&TNC->Streams[0].BPQtoPACTOR_Q);
			txlen=buffptr[1];
			memcpy(txbuff,buffptr+2,txlen);
			bytes = ARDOPSendData(TNC, &txbuff[0], txlen);
			STREAM->BytesTXed += bytes;
			WritetoTrace(TNC, txbuff, txlen);
			ReleaseBuffer(buffptr);
		}
		
		if (TNC->SwallowSignon)
		{
			TNC->SwallowSignon = FALSE;		// Discard *** connected
			return 0;
		}


		txlen=(buff[6]<<8) + buff[5]-8;	
		
		if (TNC->Streams[0].Connected)
		{
			STREAM->PacketsSent++;

			if (STREAM->PacketsSent == 3)
			{
//				if (TNC->Robust)
//					ARDOPSendCommand(TNC, "ROBUST TRUE");
//				else
//					ARDOPSendCommand(TNC, "ROBUST FALSE");
			}

			bytes=ARDOPSendData(TNC, &buff[8], txlen);
			STREAM->BytesTXed += bytes;
			WritetoTrace(TNC, &buff[8], txlen);

		}
		else
		{
			if (_memicmp(&buff[8], "D\r", 2) == 0 || _memicmp(&buff[8], "BYE\r", 4) == 0)
			{
				TNC->Streams[0].ReportDISC = TRUE;		// Tell Node
				return 0;
			}
	
			if (TNC->FECMode)
			{
				char Buffer[300];
				int len;

				// Send FEC Data

				buff[8 + txlen] = 0;
				len = sprintf(Buffer, "%-9s: %s", TNC->Streams[0].MyCall, &buff[8]);

				ARDOPSendData(TNC, Buffer, len);

				if (TNC->BusyFlags)
				{
					TNC->FECPending = 1;
				}
				else
				{
					ARDOPSendCommand(TNC,"FECSEND TRUE", TRUE);
				}
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
					C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
				}
				return 1;
			}

			if (_memicmp(&buff[8], "OVERRIDEBUSY", 12) == 0)
			{
				UINT * buffptr = GetBuff();

				TNC->OverrideBusy = TRUE;

				if (buffptr)
				{
					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "ARDOP} OK\r");
					C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
				}

				return 0;

			}


			if (_memicmp(&buff[8], "MAXCONREQ", 9) == 0)
			{
				if (buff[17] != 13)
				{
					UINT * buffptr = GetBuff();
				
					// Limit connects

					int tries = atoi(&buff[18]);
					if (tries > 10) tries = 10;

					TNC->MaxConReq = tries;

					if (buffptr)
					{
						buffptr[1] = sprintf((UCHAR *)&buffptr[2], "ARDOP} OK\r");
						C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
					}
					return 0;
				}
			}
			if ((_memicmp(&buff[8], "BW 500", 6) == 0) || (_memicmp(&buff[8], "BW 1600", 7) == 0))
			{
				// Generate a local response
				
				UINT * buffptr = GetBuff();

				if (buffptr)
				{
					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "ARDOP} OK\r");
					C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
				}
				TNC->WinmorCurrentMode = 0;			// So scanner will set next value
			}

			if (_memicmp(&buff[8], "CODEC TRUE", 9) == 0)
				TNC->StartSent = TRUE;

			if (_memicmp(&buff[8], "ROBUST", 6) == 0)
			{
				if (_memicmp(&buff[15], "TRUE", 4) == 0)
					TNC->Robust = TRUE;
				else
					TNC->Robust = FALSE;
			}

			if (_memicmp(&buff[8], "D\r", 2) == 0)
			{
				TNC->Streams[0].ReportDISC = TRUE;		// Tell Node
				return 0;
			}

			if (_memicmp(&buff[8], "FEC\r", 4) == 0 || _memicmp(&buff[8], "FEC ", 4) == 0)
			{
				TNC->FECMode = TRUE;
				TNC->FECIDTimer = 0;
//				ARDOPSendCommand(TNC,"FECRCV TRUE");
		
				if (_memicmp(&buff[8], "FEC 1600", 8) == 0)
					TNC->FEC1600 = TRUE;
				else
					TNC->FEC1600 = FALSE;

				return 0;
			}

			// See if a Connect Command. If so, start codec and set Connecting

			if (toupper(buff[8]) == 'C' && buff[9] == ' ' && txlen > 2)	// Connect
			{
				char Connect[80];
				char * ptr = strchr(&buff[10], 13);

				if (ptr)
					*ptr = 0;

				_strupr(&buff[10]);

				if (strlen(&buff[10]) > 9)
					buff[19] = 0;

				sprintf(Connect, "ARQCALL %s %d", &buff[10], TNC->MaxConReq);

				ARDOPChangeMYC(TNC, TNC->Streams[0].MyCall);

				// See if Busy
				
				if (InterlockedCheckBusy(TNC))
				{
					// Channel Busy. Unless override set, wait

					if (TNC->OverrideBusy == 0)
					{
						// Save Command, and wait up to 10 secs
						
						sprintf(TNC->WEB_TNCSTATE, "Waiting for clear channel");
						MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

						TNC->ConnectCmd = _strdup(Connect);
						TNC->BusyDelay = TNC->BusyWait * 10;		// BusyWait secs
						return 0;
					}
				}

				TNC->OverrideBusy = FALSE;

				ARDOPSendCommand(TNC, Connect, TRUE);
				TNC->Streams[0].Connecting = TRUE;

				memset(TNC->Streams[0].RemoteCall, 0, 10);
				strcpy(TNC->Streams[0].RemoteCall, &buff[10]);

				sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
				MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);
			}
			else
			{
				buff[7 + txlen] = 0;
				ARDOPSendCommand(TNC, &buff[8], TRUE);
			}
		}
		return (0);

	case 3:	
		
		// CHECK IF OK TO SEND (And check TNC Status)

		if (TNC->Streams[0].Attached == 0)
			return TNC->CONNECTED << 8 | 1;

		return (TNC->CONNECTED << 8 | TNC->Streams[0].Disconnecting << 15);		// OK
			
		break;

	case 4:				// reinit

		return 0;

	case 5:				// Close

		if (TNC->CONNECTED)
		{
			GetSemaphore(&Semaphore, 52);
			ARDOPSendCommand(TNC, "CLOSE", FALSE);
			FreeSemaphore(&Semaphore);
			Sleep(100);
		}
		shutdown(TNC->WINMORSock, SD_BOTH);
		Sleep(100);
		closesocket(TNC->WINMORSock);
		return 0;

	case 6:				// Scan Stop Interface

		Param = (int)buff;
	
		if (Param == 1)		// Request Permission
		{
			if (TNC->ConnectPending)
				TNC->ConnectPending--;		// Time out if set too long

			if (!TNC->ConnectPending)
				return 0;	// OK to Change

//			ARDOPSendCommand(TNC, "LISTEN FALSE", TRUE);

			return TRUE;
		}

		if (Param == 2)		// Check  Permission
		{
			if (TNC->ConnectPending)
			{
				TNC->ConnectPending--;
				return -1;	// Skip Interval
			}
			return 1;		// OK to change
		}

		if (Param == 3)		// Release  Permission
		{
//			ARDOPSendCommand(TNC, "LISTEN TRUE", TRUE);
			return 0;
		}

		// Param is Address of a struct ScanEntry

		Scan = (struct ScanEntry *)buff;

		if (Scan->Bandwidth == 'W')		// Set Wide Mode
		{
			if (TNC->WinmorCurrentMode != 1600)
			{
				if (TNC->WinmorCurrentMode == 0)
					ARDOPSendCommand(TNC, "LISTEN TRUE", TRUE);

				ARDOPSendCommand(TNC, "ARQBW 2000MAX", TRUE);

				TNC->WinmorCurrentMode = 1600;
			}
			TNC->WL2KMode = 22;
			return 0;
		}


		if (Scan->Bandwidth == 'N')		// Set Wide Mode
		{
			if (TNC->WinmorCurrentMode != 500)
			{
				if (TNC->WinmorCurrentMode == 0)
					ARDOPSendCommand(TNC, "LISTEN TRUE", TRUE);

				TNC->WinmorCurrentMode = 500;
				ARDOPSendCommand(TNC, "ARQBW 500MAX", TRUE);
			}
			TNC->WL2KMode = 21;
			return 0;
		}

		if (Scan->Bandwidth == 'X')		// Dont Allow Connects
		{
			if (TNC->WinmorCurrentMode != 0)
			{
				ARDOPSendCommand(TNC, "LISTEN FALSE", TRUE);
				TNC->WinmorCurrentMode = 0;
			}

			TNC->WL2KMode = 0;
			return 0;
		}

		return 0;
	}
	return 0;
}

VOID ARDOPReleaseTNC(struct TNCINFO * TNC)
{
	// Set mycall back to Node or Port Call, and Start Scanner

	UCHAR TXMsg[1000];

	ARDOPChangeMYC(TNC, TNC->NodeCall);

	ARDOPSendCommand(TNC, "LISTEN TRUE", TRUE);

	strcpy(TNC->WEB_TNCSTATE, "Free");
	MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

	//	Start Scanner
				
	sprintf(TXMsg, "%d SCANSTART 15", TNC->Port);

	Rig_Command(-1, TXMsg);

	ReleaseOtherPorts(TNC);

}

VOID ARDOPSuspendPort(struct TNCINFO * TNC)
{
	ARDOPSendCommand(TNC, "CODEC FALSE", TRUE);
}

VOID ARDOPReleasePort(struct TNCINFO * TNC)
{
	ARDOPSendCommand(TNC, "CODEC TRUE", TRUE);
}


static int WebProc(struct TNCINFO * TNC, char * Buff, BOOL LOCAL)
{
	int Len = sprintf(Buff, "<html><meta http-equiv=expires content=0><meta http-equiv=refresh content=15>"
		"<script type=\"text/javascript\">\r\n"
		"function ScrollOutput()\r\n"
		"{var textarea = document.getElementById('textarea');"
		"textarea.scrollTop = textarea.scrollHeight;}</script>"
		"</head><title>ARDOP Status</title></head><body id=Text onload=\"ScrollOutput()\">"
		"<h2>ARDOP Status</h2>");


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


UINT ARDOPExtInit(EXTPORTDATA * PortEntry)
{
	int i, port;
	char Msg[255];
	char * ptr;
	APPLCALLS * APPL;
	struct TNCINFO * TNC;
	char Aux[100] = "MYAUX ";
	char Appl[11];
	char * TempScript;

	//
	//	Will be called once for each WINMOR port 
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

	TNC->ARDOPBuffer = malloc(8192);

	if (TNC->ProgramPath)
		TNC->WeStartedTNC = ARDOPRestartTNC(TNC);

	TNC->Hardware = H_ARDOP;

	if (TNC->BusyWait == 0)
		TNC->BusyWait = 10;

	if (TNC->BusyHold == 0)
		TNC->BusyHold = 1;

	TNC->PortRecord = PortEntry;

	if (PortEntry->PORTCONTROL.PORTCALL[0] == 0)
		memcpy(TNC->NodeCall, MYNODECALL, 10);
	else
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);

	TNC->Interlock = PortEntry->PORTCONTROL.PORTINTERLOCK;

	PortEntry->PORTCONTROL.PROTOCOL = 10;
	PortEntry->PORTCONTROL.PORTQUALITY = 0;
	PortEntry->MAXHOSTMODESESSIONS = 1;	
	PortEntry->SCANCAPABILITIES = SIMPLE;			// Scan Control - pending connect only

	PortEntry->PORTCONTROL.UICAPABLE = TRUE;

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0)
		PortEntry->PORTCONTROL.PORTPACLEN = 236;

	TNC->SuspendPortProc = ARDOPSuspendPort;
	TNC->ReleasePortProc = ARDOPReleasePort;

	TNC->ModemCentre = 1500;				// WINMOR is always 1500 Offset

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	// Set Essential Params and MYCALL

	// Put overridable ones on front, essential ones on end

	TempScript = malloc(1000);

	strcpy(TempScript, "INITIALIZE\r");
	strcat(TempScript, "VERSION\r");
	strcat(TempScript, "CWID False\r");
	strcat(TempScript, "PROTOCOLMODE ARQ\r");
	strcat(TempScript, "ARQTIMEOUT 90\r");
//	strcat(TempScript, "ROBUST False\r");

	strcat(TempScript, TNC->InitScript);

	free(TNC->InitScript);
	TNC->InitScript = TempScript;

	// Set MYCALL

//	strcat(TNC->InitScript,"FECRCV True\r");
//	strcat(TNC->InitScript,"AUTOBREAK True\r");

	sprintf(Msg, "MYCALL %s\r", TNC->NodeCall);
	strcat(TNC->InitScript, Msg);
//	strcat(TNC->InitScript,"PROCESSID\r");
//	strcat(TNC->InitScript,"CODEC TRUE\r");
//	strcat(TNC->InitScript,"LISTEN TRUE\r");
	strcat(TNC->InitScript,"MYCALL\r");


	for (i = 0; i < 32; i++)
	{
		APPL=&APPLCALLTABLE[i];

		if (APPL->APPLCALL_TEXT[0] > ' ')
		{
			char * ptr;
			memcpy(Appl, APPL->APPLCALL_TEXT, 10);
			ptr=strchr(Appl, ' ');

			if (ptr)
			{
				*ptr++ = ',';
				*ptr = 0;
			}
			strcat(Aux, Appl);
		}
	}

	if (strlen(Aux) > 8)
	{
		strcat(TNC->InitScript, Aux);
		strcat(TNC->InitScript,"\rMYAUX\r");	// READ BACK
	}

	strcpy(TNC->CurrentMYC, TNC->NodeCall);

	if (TNC->WL2K == NULL)
		if (PortEntry->PORTCONTROL.WL2KInfo.RMSCall[0])			// Alrerady decoded
			TNC->WL2K = &PortEntry->PORTCONTROL.WL2KInfo;

	if (TNC->destaddr.sin_family == 0)
	{
		// not defined in config file, so use localhost and port from IOBASE

		TNC->destaddr.sin_family = AF_INET;
		TNC->destaddr.sin_port = htons(PortEntry->PORTCONTROL.IOBASE);
		TNC->Datadestaddr.sin_family = AF_INET;
		TNC->Datadestaddr.sin_port = htons(PortEntry->PORTCONTROL.IOBASE+1);

		TNC->WINMORHostName=malloc(10);

		if (TNC->WINMORHostName != NULL) 
			strcpy(TNC->WINMORHostName,"127.0.0.1");

	}

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

	TNC->WEB_MODE = zalloc(20);
	TNC->WEB_TRAFFIC = zalloc(100);


#ifndef LINBPQ

	CreatePactorWindow(TNC, ClassName, WindowTitle, RigControlRow, PacWndProc, 500, 450);

	CreateWindowEx(0, "STATIC", "Comms State", WS_CHILD | WS_VISIBLE, 10,6,120,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_COMMSSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,6,386,20, TNC->hDlg, NULL, hInstance, NULL);
	
	CreateWindowEx(0, "STATIC", "TNC State", WS_CHILD | WS_VISIBLE, 10,28,106,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TNCSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,28,520,20, TNC->hDlg, NULL, hInstance, NULL);

	CreateWindowEx(0, "STATIC", "Mode", WS_CHILD | WS_VISIBLE, 10,50,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_MODE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,50,200,20, TNC->hDlg, NULL, hInstance, NULL);
 
	CreateWindowEx(0, "STATIC", "Channel State", WS_CHILD | WS_VISIBLE, 10,72,110,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_CHANSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,72,144,20, TNC->hDlg, NULL, hInstance, NULL);
 
 	CreateWindowEx(0, "STATIC", "Proto State", WS_CHILD | WS_VISIBLE,10,94,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_PROTOSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE,116,94,374,20 , TNC->hDlg, NULL, hInstance, NULL);
 
	CreateWindowEx(0, "STATIC", "Traffic", WS_CHILD | WS_VISIBLE,10,116,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TRAFFIC = CreateWindowEx(0, "STATIC", "0 0 0 0", WS_CHILD | WS_VISIBLE,116,116,374,20 , TNC->hDlg, NULL, hInstance, NULL);

	CreateWindowEx(0, "STATIC", "TNC Restarts", WS_CHILD | WS_VISIBLE,10,138,100,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_RESTARTS = CreateWindowEx(0, "STATIC", "0", WS_CHILD | WS_VISIBLE,116,138,40,20 , TNC->hDlg, NULL, hInstance, NULL);
	CreateWindowEx(0, "STATIC", "Last Restart", WS_CHILD | WS_VISIBLE,140,138,100,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_RESTARTTIME = CreateWindowEx(0, "STATIC", "Never", WS_CHILD | WS_VISIBLE,250,138,200,20, TNC->hDlg, NULL, hInstance, NULL);

	TNC->hMonitor= CreateWindowEx(0, "LISTBOX", "", WS_CHILD |  WS_VISIBLE  | LBS_NOINTEGRALHEIGHT | 
            LBS_DISABLENOSCROLL | WS_HSCROLL | WS_VSCROLL,
			0,170,250,300, TNC->hDlg, NULL, hInstance, NULL);

	TNC->ClientHeight = 450;
	TNC->ClientWidth = 500;

	TNC->hMenu = CreatePopupMenu();

	AppendMenu(TNC->hMenu, MF_STRING, WINMOR_KILL, "Kill ARDOP TNC");
	AppendMenu(TNC->hMenu, MF_STRING, WINMOR_RESTART, "Kill and Restart ARDOP TNC");
	AppendMenu(TNC->hMenu, MF_STRING, WINMOR_RESTARTAFTERFAILURE, "Restart TNC after each Connection");
	
	CheckMenuItem(TNC->hMenu, WINMOR_RESTARTAFTERFAILURE, (TNC->RestartAfterFailure) ? MF_CHECKED : MF_UNCHECKED);

	MoveWindows(TNC);
#endif
	Consoleprintf("ARDOP Host %s %d", TNC->WINMORHostName, htons(TNC->destaddr.sin_port));

	ConnecttoARDOP(port);

	time(&TNC->lasttime);			// Get initial time value

	return ((int) ExtProc);
}

int ConnecttoARDOP(int port)
{
	_beginthread(ARDOPThread,0,port);

	return 0;
}

VOID ARDOPThread(port)
{
	// Opens socket and looks for data on control socket.
	
	// Socket may be TCP/IP or Serial

	char Msg[255];
	int err, i, ret;
	u_long param=1;
	BOOL bcopt=TRUE;
	struct hostent * HostEnt;
	struct TNCINFO * TNC = TNCInfo[port];
	fd_set readfs;
	fd_set errorfs;
	struct timeval timeout;
	char * ptr1, * ptr2;
	UINT * buffptr;

	if (TNC->WINMORHostName == NULL)
		return;

	TNC->BusyFlags = 0;

	TNC->CONNECTING = TRUE;

	Sleep(5000);		// Allow init to complete 

//	// If we started the TNC make sure it is still running.

//	if (!IsProcess(TNC->WIMMORPID))
//	{
//		ARDOPRestartTNC(TNC);
//		Sleep(3000);
//	}


	TNC->destaddr.sin_addr.s_addr = inet_addr(TNC->WINMORHostName);
	TNC->Datadestaddr.sin_addr.s_addr = inet_addr(TNC->WINMORHostName);

	if (TNC->destaddr.sin_addr.s_addr == INADDR_NONE)
	{
		//	Resolve name to address

		HostEnt = gethostbyname (TNC->WINMORHostName);
		 
		 if (!HostEnt)
		 {
			 	TNC->CONNECTING = FALSE;
				return;			// Resolve failed
		 }
		 memcpy(&TNC->destaddr.sin_addr.s_addr,HostEnt->h_addr,4);
		 memcpy(&TNC->Datadestaddr.sin_addr.s_addr,HostEnt->h_addr,4);

	}

//	closesocket(TNC->WINMORSock);
//	closesocket(TNC->WINMORDataSock);

	TNC->WINMORSock=socket(AF_INET,SOCK_STREAM,0);

	if (TNC->WINMORSock == INVALID_SOCKET)
	{
		i=sprintf(Msg, "Socket Failed for ARDOP socket - error code = %d\r\n", WSAGetLastError());
		WritetoConsole(Msg);

	 	TNC->CONNECTING = FALSE;
  	 	return; 
	}
 
	setsockopt (TNC->WINMORSock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt, 4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (bind(TNC->WINMORSock, (LPSOCKADDR) &sinx, addrlen) != 0 )
	{
		//
		//	Bind Failed
		//
	
		i=sprintf(Msg, "Bind Failed for ARDOP socket - error code = %d\r\n", WSAGetLastError());
		WritetoConsole(Msg);
			
		closesocket(TNC->WINMORSock);
	 	TNC->CONNECTING = FALSE;

  	 	return; 
	}

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
   			i=sprintf(Msg, "Connect Failed for ARDOP socket - error code = %d\r\n", err);
			WritetoConsole(Msg);
			sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC failed");
			SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

			TNC->Alerted = TRUE;
		}
		
		closesocket(TNC->WINMORSock);
		TNC->WINMORSock = 0;
	 	TNC->CONNECTING = FALSE;
		return;
	}

#ifndef LINBPQ
	EnumWindows(EnumARDOPWindowsProc, (LPARAM)TNC);
#endif
	Sleep(1000);

	TNC->LastFreq = 0;			//	so V4 display will be updated

 	TNC->CONNECTING = FALSE;
	TNC->CONNECTED = TRUE;
	TNC->BusyFlags = 0;

	// Send INIT script

	// ARDOP needs each command in a separate send

	ptr1 = &TNC->InitScript[0];

	// We should wait for first RDY. Cheat by queueing a null command

	GetSemaphore(&Semaphore, 52);

	while(TNC->BPQtoWINMOR_Q)
	{
		buffptr = Q_REM(&TNC->BPQtoWINMOR_Q);

		if (buffptr)
			ReleaseBuffer(buffptr);
	}

	buffptr = GetBuff();
	buffptr[1] = 0;
	C_Q_ADD(&TNC->BPQtoWINMOR_Q, buffptr);

	while (ptr1 && ptr1[0])
	{
		ptr2 = strchr(ptr1, 13);
		if (ptr2)
			*(ptr2) = 0; 
	
		ARDOPSendCommand(TNC, ptr1, TRUE);

		if (ptr2)
			*(ptr2++) = 13;		// Put CR back for next time 

		ptr1 = ptr2;
	}
	
	TNC->Alerted = TRUE;

	sprintf(TNC->WEB_COMMSSTATE, "Connected to ARDOP TNC");		
	MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

	FreeSemaphore(&Semaphore);

	while (TRUE)
	{
		FD_ZERO(&readfs);	
		FD_ZERO(&errorfs);

		FD_SET(TNC->WINMORSock,&readfs);
		FD_SET(TNC->WINMORSock,&errorfs);

		timeout.tv_sec = 90;
		timeout.tv_usec = 0;				// We should get messages more frequently that this

		ret = select(TNC->WINMORSock + 1, &readfs, NULL, &errorfs, &timeout);
		
		if (ret == SOCKET_ERROR)
		{
			Debugprintf("ARDOP Select failed %d ", WSAGetLastError());
			goto Lost;
		}
		if (ret > 0)
		{
			//	See what happened

			if (FD_ISSET(TNC->WINMORSock, &readfs))
			{
				GetSemaphore(&Semaphore, 52);
				ARDOPProcessReceivedData(TNC);
				FreeSemaphore(&Semaphore);
			}
								
			if (FD_ISSET(TNC->WINMORSock, &errorfs))
			{
Lost:	
				sprintf(Msg, "ARDOP Connection lost for Port %d\r\n", TNC->Port);
				WritetoConsole(Msg);

				sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC lost");
				SetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

				TNC->CONNECTED = FALSE;
				TNC->Alerted = FALSE;

				if (TNC->PTTMode)
					Rig_PTT(TNC->RIG, FALSE);			// Make sure PTT is down

				if (TNC->Streams[0].Attached)
					TNC->Streams[0].ReportDISC = TRUE;

				closesocket(TNC->WINMORSock);
				TNC->WINMORSock = 0;
				return;
			}
	
			continue;
		}
		else
		{
			// 60 secs without data. Shouldn't happen

			sprintf(Msg, "ARDOP No Data Timeout Port %d\r\n", TNC->Port);
			WritetoConsole(Msg);

//			sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC lost");
//			GetSemaphore(&Semaphore, 52);
//			MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);
//			FreeSemaphore(&Semaphore);
	

			TNC->CONNECTED = FALSE;
			TNC->Alerted = FALSE;

			if (TNC->PTTMode)
				Rig_PTT(TNC->RIG, FALSE);			// Make sure PTT is down

			if (TNC->Streams[0].Attached)
				TNC->Streams[0].ReportDISC = TRUE;

			ARDOPSendCommand(TNC, "CODEC FALSE", FALSE);
	
			Sleep(100);
			shutdown(TNC->WINMORSock, SD_BOTH);
			Sleep(100);

			closesocket(TNC->WINMORSock);

			if (TNC->WIMMORPID && TNC->WeStartedTNC)
			{
				ARDOPKillTNC(TNC);
			}
			return;
		}
	}
}

#ifndef LINBPQ

BOOL CALLBACK EnumARDOPWindowsProc(HWND hwnd, LPARAM  lParam)
{
	char wtext[100];
	struct TNCINFO * TNC = (struct TNCINFO *)lParam; 
	UINT ProcessId;

	GetWindowText(hwnd,wtext,99);

	if (memcmp(wtext,"ARDOP_Win ", 10) == 0)
	{
		GetWindowThreadProcessId(hwnd, &ProcessId);

		if (TNC->WIMMORPID == ProcessId)
		{
			 // Our Process

			sprintf (wtext, "ARDOP Virtual TNC - BPQ %s", TNC->PortRecord->PORTCONTROL.PORTDESCRIPTION);
			SetWindowText(hwnd, wtext);
			return FALSE;
		}
	}
	
	return (TRUE);
}
#endif

VOID ARDOPProcessResponse(struct TNCINFO * TNC, UCHAR * Buffer, int MsgLen)
{
	UINT * buffptr;
	struct STREAMINFO * STREAM = &TNC->Streams[0];
	unsigned int CRC;

	CRC = checkcrc16(&Buffer[2], MsgLen - 2);

	Buffer[MsgLen - 3] = 0;		// Remove CR

	if (CRC == 0)
	{
		Debugprintf("ADDOP CRC Error %s", Buffer);
		return;
	}
	
	Buffer+=2;					// Skip c:

	if (_memicmp(Buffer, "RDY", 3) == 0)
	{
		//	Command ACK. Remove from bufer and send next if any

		UINT * buffptr;
		UINT * Q;
	
	
		buffptr = Q_REM(&TNC->BPQtoWINMOR_Q);

		if (buffptr)
			ReleaseBuffer(buffptr);

		// See if another

		// Leave on Queue till acked

		// Q may not be word aligned, so copy as bytes (for ARM5)

		Q = (UINT *)&TNC->BPQtoWINMOR_Q;

		buffptr = (UINT *)Q[0];

		if (buffptr)
			SendToTNC(TNC, (UCHAR *)&buffptr[2], buffptr[1]);

		return;
	}

	if (_memicmp(Buffer, "CRCFAULT", 8) == 0)
	{
		//	Command NAK. Resend 

		UINT * buffptr;
		UINT * Q;
	
		// Leave on Queue till acked

		// Q may not be word aligned, so copy as bytes (for ARM5)

		Q = (UINT *)&TNC->BPQtoWINMOR_Q;

		buffptr = (UINT *)Q[0];

		if (buffptr)
			SendToTNC(TNC, (UCHAR *)&buffptr[2], buffptr[1]);

		Debugprintf("ARDP CRCFAULT Received");
		return;
	}

	if (_memicmp(Buffer, "FAULT failure to Restart Sound card", 20) == 0)
	{
		Debugprintf(Buffer);
	
		// Force a restart

			ARDOPSendCommand(TNC, "CODEC FALSE", TRUE);
			ARDOPSendCommand(TNC, "CODEC TRUE", TRUE);
	}
	else
	{
		TNC->TimeSinceLast = 0;
	}


	if (_memicmp(Buffer, "STATE ", 6) == 0)
	{
		Debugprintf(Buffer);
	
		if (_memicmp(&Buffer[6], "OFFLINE", 7) == 0)
		{
			// Force a restart

			ARDOPSendCommand(TNC, "CODEC FALSE", TRUE);
			ARDOPSendCommand(TNC, "CODEC TRUE", TRUE);
		}
		return;
	}
	
	if (_memicmp(Buffer, "PTT T", 5) == 0)
	{
		TNC->Busy = TNC->BusyHold * 10;				// BusyHold  delay

		if (TNC->PTTMode)
			Rig_PTT(TNC->RIG, TRUE);

		ARDOPSendCommand(TNC, "RDY", FALSE);
		return;
	}
	if (_memicmp(Buffer, "PTT F", 5) == 0)
	{
		if (TNC->PTTMode)
			Rig_PTT(TNC->RIG, FALSE);
		ARDOPSendCommand(TNC, "RDY", FALSE);
		return;
	}

	if (_memicmp(Buffer, "BUSY TRUE", 9) == 0)
	{	
		TNC->BusyFlags |= CDBusy;
		TNC->Busy = TNC->BusyHold * 10;				// BusyHold  delay

		MySetWindowText(TNC->xIDC_CHANSTATE, "Busy");
		strcpy(TNC->WEB_CHANSTATE, "Busy");

		TNC->WinmorRestartCodecTimer = time(NULL);
		ARDOPSendCommand(TNC, "RDY", FALSE);
		return;
	}

	if (_memicmp(Buffer, "BUSY FALSE", 10) == 0)
	{
		TNC->BusyFlags &= ~CDBusy;
		if (TNC->BusyHold)
			strcpy(TNC->WEB_CHANSTATE, "BusyHold");
		else
			strcpy(TNC->WEB_CHANSTATE, "Clear");

		MySetWindowText(TNC->xIDC_CHANSTATE, TNC->WEB_CHANSTATE);
		TNC->WinmorRestartCodecTimer = time(NULL);
		ARDOPSendCommand(TNC, "RDY", FALSE);
		return;
	}

	if (_memicmp(Buffer, "TARGET", 6) == 0)
	{
		TNC->ConnectPending = 6;					// This comes before Pending
		Debugprintf(Buffer);
		WritetoTrace(TNC, Buffer, MsgLen - 5);
		memcpy(TNC->TargetCall, &Buffer[7], 10);
		return;
	}

	if (_memicmp(Buffer, "OFFSET", 6) == 0)
	{
//		WritetoTrace(TNC, Buffer, MsgLen - 5);
//		memcpy(TNC->TargetCall, &Buffer[7], 10);
		return;
	}

	if (_memicmp(Buffer, "BUFFER", 6) == 0)
	{
		int inq, inrx, Sent, BPM;

		sscanf(&Buffer[7], "%d%d%d%d%d", &inq, &inrx, &TNC->Streams[0].BytesOutstanding, &Sent, &BPM);

		if (TNC->Streams[0].BytesOutstanding == 0)
		{
			// all sent
			
			if (TNC->Streams[0].Disconnecting)						// Disconnect when all sent
			{
				if (STREAM->NeedDisc == 0)
					STREAM->NeedDisc = 60;								// 6 secs
			}
//			else
//			if (TNC->TXRXState == 'S')
//				ARDOPSendCommand(TNC,"OVER");

		}
		else
		{
			// Make sure Node Keepalive doesn't kill session.

			TRANSPORTENTRY * SESS = TNC->PortRecord->ATTACHEDSESSIONS[0];

			if (SESS)
			{
				SESS->L4KILLTIMER = 0;
				SESS = SESS->L4CROSSLINK;
				if (SESS)
					SESS->L4KILLTIMER = 0;
			}
		}

		sprintf(TNC->WEB_TRAFFIC, "Sent %d RXed %d Queued %s",
			STREAM->BytesTXed, STREAM->BytesRXed, &Buffer[7]);
		MySetWindowText(TNC->xIDC_TRAFFIC, TNC->WEB_TRAFFIC);
		ARDOPSendCommand(TNC, "RDY", FALSE);
		return;
	}

	if (_memicmp(Buffer, "CONNECTED ", 10) == 0)
	{
		char Call[11];
		char * ptr;
		APPLCALLS * APPL;
		char * ApplPtr = APPLS;
		int App;
		char Appl[10];
		struct WL2KInfo * WL2K = TNC->WL2K;

		Debugprintf(Buffer);
		WritetoTrace(TNC, Buffer, MsgLen - 5);

		ARDOPSendCommand(TNC, "RDY", FALSE);
	
		STREAM->ConnectTime = time(NULL); 
		STREAM->BytesRXed = STREAM->BytesTXed = STREAM->PacketsSent = 0;

//		if (TNC->StartInRobust)
//			ARDOPSendCommand(TNC, "ROBUST TRUE");

		memcpy(Call, &Buffer[10], 10);

		ptr = strchr(Call, ' ');	
		if (ptr) *ptr = 0;

		TNC->HadConnect = TRUE;

		if (TNC->PortRecord->ATTACHEDSESSIONS[0] == 0)
		{
			TRANSPORTENTRY * SESS;
			
			// Incomming Connect

			// Stop other ports in same group

			SuspendOtherPorts(TNC);

			ProcessIncommingConnectEx(TNC, Call, 0, TRUE, TRUE);
				
			SESS = TNC->PortRecord->ATTACHEDSESSIONS[0];
			
			TNC->ConnectPending = FALSE;

			if (TNC->RIG && TNC->RIG != &TNC->DummyRig && strcmp(TNC->RIG->RigName, "PTT"))
			{
				sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound Freq %s", TNC->Streams[0].RemoteCall, TNC->TargetCall, TNC->RIG->Valchar);
				SESS->Frequency = (atof(TNC->RIG->Valchar) * 1000000.0) + 1500;		// Convert to Centre Freq
				SESS->Mode = TNC->WL2KMode;
			}
			else
			{
				sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound", TNC->Streams[0].RemoteCall, TNC->TargetCall);
				if (WL2K)
				{
					SESS->Frequency = WL2K->Freq;
					SESS->Mode = WL2K->mode;
				}
			}
			
			if (WL2K)
				strcpy(SESS->RMSCall, WL2K->RMSCall);

			SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);
			
			// See which application the connect is for

			for (App = 0; App < 32; App++)
			{
				APPL=&APPLCALLTABLE[App];
				memcpy(Appl, APPL->APPLCALL_TEXT, 10);
				ptr=strchr(Appl, ' ');

				if (ptr)
					*ptr = 0;
	
				if (_stricmp(TNC->TargetCall, Appl) == 0)
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
					MsgLen = sprintf(Buffer, "%s\r", AppName);

					buffptr = GetBuff();

					if (buffptr == 0)
					{
						return;			// No buffers, so ignore
					}

					buffptr[1] = MsgLen;
					memcpy(buffptr+2, Buffer, MsgLen);

					C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
		
					TNC->SwallowSignon = TRUE;

					// Save Appl Call in case needed for 

				}
				else
				{
					char Msg[] = "Application not available\r\n";
					
					// Send a Message, then a disconenct
					
					ARDOPSendData(TNC, Msg, strlen(Msg));
					STREAM->NeedDisc = 100;	// 10 secs
				}
			}
		
			return;
		}
		else
		{
			// Connect Complete

			char Reply[80];
			int ReplyLen;
			
			buffptr = GetBuff();

			if (buffptr == 0)
			{
				return;			// No buffers, so ignore
			}
			ReplyLen = sprintf(Reply, "*** Connected to %s\r", Call);

			buffptr[1] = ReplyLen;
			memcpy(buffptr+2, Reply, ReplyLen);

			C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);

			TNC->Streams[0].Connecting = FALSE;
			TNC->Streams[0].Connected = TRUE;			// Subsequent data to data channel

			if (TNC->RIG && TNC->RIG->Valchar[0])
				sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound Freq %s",  TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall, TNC->RIG->Valchar);
			else
				sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
			
			SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

			UpdateMH(TNC, Call, '+', 'O');
			return;
		}
	}


	if (_memicmp(Buffer, "DISCONNECTED", 12) == 0
		|| _memicmp(Buffer, "STATUS CONNECT TO", 17) == 0  
		|| _memicmp(Buffer, "STATUS ARQ TIMEOUT FROM PROTOCOL STATE", 24) == 0)
	{
		Debugprintf(Buffer);

		ARDOPSendCommand(TNC, "RDY", FALSE);
		TNC->ConnectPending = FALSE;			// Cancel Scan Lock

		if (TNC->FECMode)
			return;

		if (TNC->StartSent)
		{
			TNC->StartSent = FALSE;		// Disconnect reported following start codec
			return;
		}

		if (TNC->Streams[0].Connecting)
		{
			// Report Connect Failed, and drop back to command mode

			TNC->Streams[0].Connecting = FALSE;

			buffptr = GetBuff();

			if (buffptr == 0)
			{
				return;			// No buffers, so ignore
			}

			buffptr[1] = sprintf((UCHAR *)&buffptr[2], "ARDOP} Failure with %s\r", TNC->Streams[0].RemoteCall);

			C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);

			return;
		}

		WritetoTrace(TNC, Buffer, MsgLen - 5);

		// Release Session

		if (TNC->Streams[0].Connected)
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


		TNC->Streams[0].Connecting = FALSE;
		TNC->Streams[0].Connected = FALSE;		// Back to Command Mode
		TNC->Streams[0].ReportDISC = TRUE;		// Tell Node

		if (TNC->Streams[0].Disconnecting)		// 
			ARDOPReleaseTNC(TNC);

		TNC->Streams[0].Disconnecting = FALSE;

		return;
	}

	if (_memicmp(Buffer, "MONCALL", 7) == 0)
	{
		Debugprintf(Buffer);

		// Add to MHEARD

		WritetoTrace(TNC, Buffer, MsgLen - 5);
		UpdateMH(TNC, &Buffer[8], '!', 0);
		
		if (!TNC->FECMode)
			return;							// If in FEC mode pass ID messages to user.
	}

	Debugprintf(Buffer);

	if (_memicmp(Buffer, "RADIOMODELS", 11) == 0)
		return;

	if (_memicmp(Buffer, "MODE", 4) == 0)
	{
	//	Debugprintf("WINMOR RX: %s", Buffer);

		strcpy(TNC->WEB_MODE, &Buffer[5]);
		SetWindowText(TNC->xIDC_MODE, &Buffer[5]);
		return;
	}

	if (_memicmp(&Buffer[0], "PENDING", 7) == 0)	// Save Pending state for scan control
	{
		ARDOPSendCommand(TNC, "RDY", FALSE);
		TNC->ConnectPending = 6;				// Time out after 6 Scanintervals
		return;
	}

	if (_memicmp(&Buffer[0], "CANCELPENDING", 13) == 0
		|| _memicmp(&Buffer[0], "REJECTEDB", 9) == 0)  //REJECTEDBUSY or REJECTEDBW
	{
		ARDOPSendCommand(TNC, "RDY", FALSE);
		TNC->ConnectPending = FALSE;
		return;
	}

	if (_memicmp(Buffer, "FAULT", 5) == 0)
	{
		WritetoTrace(TNC, Buffer, MsgLen - 5);
//		return;
	}

	if (_memicmp(Buffer, "NEWSTATE", 8) == 0)
	{
		ARDOPSendCommand(TNC, "RDY", FALSE);

		TNC->WinmorRestartCodecTimer = time(NULL);

		SetWindowText(TNC->xIDC_PROTOSTATE, &Buffer[9]);
		strcpy(TNC->WEB_PROTOSTATE,  &Buffer[9]);
	
		if (_memicmp(&Buffer[9], "DISCONNECTING", 13) == 0)	// So we can timout stuck discpending
		{
			TNC->DiscPending = 600;
			return;
		}
		if (_memicmp(&Buffer[9], "DISCONNECTED", 12) == 0)
		{
			TNC->DiscPending = FALSE;
			TNC->ConnectPending = FALSE;

			if (TNC->RestartAfterFailure)
			{
				if (TNC->HadConnect)
				{
					TNC->HadConnect = FALSE;

					if (TNC->WIMMORPID)
					{
						ARDOPKillTNC(TNC);
						ARDOPRestartTNC(TNC);
					}
				}
			}
			return;
		}

		if (strcmp(&Buffer[9], "ISS") == 0)	// Save Pending state for scan control
			TNC->TXRXState = 'S';
		else if (strcmp(&Buffer[9], "IRS") == 0)
			TNC->TXRXState = 'R';
	
		return;
	}


	if (_memicmp(Buffer, "PROCESSID", 9) == 0)
	{
		HANDLE hProc;
		char ExeName[256] = "";

		TNC->WIMMORPID = atoi(&Buffer[10]);

#ifndef LINBPQ

		// Get the File Name in case we want to restart it.

		if (TNC->ProgramPath == NULL)
		{
			if (GetModuleFileNameExPtr)
			{
				hProc =  OpenProcess(PROCESS_QUERY_INFORMATION |PROCESS_VM_READ, FALSE, TNC->WIMMORPID);
	
				if (hProc)
				{
					GetModuleFileNameExPtr(hProc, 0,  ExeName, 255);
					CloseHandle(hProc);

					TNC->ProgramPath = _strdup(ExeName);
				}
			}
		}

		// Set Window Title to reflect BPQ Port Description

		EnumWindows(EnumARDOPWindowsProc, (LPARAM)TNC);
#endif
	}

	if ((_memicmp(Buffer, "FAULT Not from state FEC", 24) == 0) || (_memicmp(Buffer, "FAULT Blocked by Busy Lock", 24) == 0))
	{
		if (TNC->FECMode)
		{
			Sleep(1000);
			
//			if (TNC->FEC1600)
//				ARDOPSendCommand(TNC,"FECSEND 1600");
//			else
//				ARDOPSendCommand(TNC,"FECSEND 500");
			return;
		}
	}

	if (_memicmp(Buffer, "PLAYBACKDEVICES", 15) == 0)
	{
		TNC->PlaybackDevices = _strdup(&Buffer[16]);
	}
	// Others should be responses to commands

	if (_memicmp(Buffer, "BLOCKED", 6) == 0)
	{
		WritetoTrace(TNC, Buffer, MsgLen - 5);
		return;
	}

	if (_memicmp(Buffer, "OVER", 4) == 0)
	{
		WritetoTrace(TNC, Buffer, MsgLen - 5);
		return;
	}

	//	Return others to user (if attached but not connected)

	if (TNC->Streams[0].Attached == 0)
		return;

	if (TNC->Streams[0].Connected)
		return;

	if (MsgLen > 200)
		MsgLen = 200;

	buffptr = GetBuff();

	if (buffptr == 0)
	{
		return;			// No buffers, so ignore
	}
	
	buffptr[1] = sprintf((UCHAR *)&buffptr[2], "ARDOP} %s\r", Buffer);

	C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
}

static VOID ARDOPProcessReceivedData(struct TNCINFO * TNC)
{
	int InputLen, MsgLen;
	char * ptr, * ptr2;
	char Buffer[2000];

	// shouldn't get several messages per packet, as each should need an ack
	// May get message split over packets

	//	Both command and data arrive here, which complicated things a bit

	//	Commands start with c: and end with CR.
	//	Data starts with d: and has a length field
	//	“d:ARQ|FEC|ERR|, 2 byte count (Hex 0001 – FFFF), binary data, +2 Byte CRC”

	//	As far as I can see, shortest frame is “c:RDY<Cr> + 2 byte CRC” = 8 bytes

	if (TNC->InputLen > 1000)	// Shouldnt have packets longer than this
		TNC->InputLen=0;

	//	I don't think it likely we will get packets this long, but be aware...

	//	We can get pretty big ones in the faster 
				
	InputLen=recv(TNC->WINMORSock, &TNC->ARDOPBuffer[TNC->InputLen], 8192 - TNC->InputLen, 0);

	if (InputLen == 0 || InputLen == SOCKET_ERROR)
	{
		// Does this mean closed?
		
		closesocket(TNC->WINMORSock);

		TNC->WINMORSock = 0;

		TNC->CONNECTED = FALSE;
		TNC->Streams[0].ReportDISC = TRUE;

		return;					
	}

	TNC->InputLen += InputLen;

loop:

	if (TNC->InputLen < 8)
		return;					// Wait for more to arrive (?? timeout??)

	if (TNC->ARDOPBuffer[1] = ':')	// At least message looks reasonable
	{
		if (TNC->ARDOPBuffer[0] == 'c')
		{
			// Command = look for CR

			ptr = memchr(TNC->ARDOPBuffer, '\r', TNC->InputLen);

			if (ptr == 0)	//  CR in buffer
				return;		// Wait for it

			ptr2 = &TNC->ARDOPBuffer[TNC->InputLen];

			if ((ptr2 - ptr) == 3)	// CR + CRC
			{
				// Usual Case - single meg in buffer
	
				ARDOPProcessResponse(TNC, TNC->ARDOPBuffer, TNC->InputLen);
				TNC->InputLen=0;
				return;
			}
			else
			{
				// buffer contains more that 1 message

				//	I dont think this should happen, but...

				MsgLen = TNC->InputLen - (ptr2-ptr) + 3;	// Include CR and CRC

				memcpy(Buffer, TNC->ARDOPBuffer, MsgLen);

				ARDOPProcessResponse(TNC, Buffer, MsgLen);

				if (TNC->InputLen < MsgLen)
				{
					TNC->InputLen = 0;
					return;
				}
				memmove(TNC->ARDOPBuffer, ptr + 3,  TNC->InputLen-MsgLen);

				TNC->InputLen -= MsgLen;
				goto loop;
			}
		}
		if (TNC->ARDOPBuffer[0] == 'd')
		{
			// Data = check we have it all

			int DataLen = (TNC->ARDOPBuffer[2] << 8) + TNC->ARDOPBuffer[3]; // HI First
			unsigned short CRC;
			UCHAR DataType[4];
			UCHAR * Data;
			
			if (TNC->InputLen < DataLen + 6)
				return;					// Wait for more

			MsgLen = DataLen + 6;		// d: Len CRC

			// Check CRC

			CRC = compute_crc(&TNC->ARDOPBuffer[2], DataLen + 4);

			CRC = checkcrc16(&TNC->ARDOPBuffer[2], DataLen + 4);

			if (CRC == 0)
			{
				Debugprintf("ADDOP CRC Error %s", &TNC->ARDOPBuffer[2]);
				return;
			}


			memcpy(DataType, &TNC->ARDOPBuffer[4] , 3);
			DataType[3] = 0;
			Data = &TNC->ARDOPBuffer[7];
			DataLen -= 3;

			ARDOPProcessDataPacket(TNC, DataType, Data, DataLen);
		
			ARDOPSendCommand(TNC, "RDY", FALSE);

			// See if anything else in buffer

			TNC->InputLen -= MsgLen;

			if (TNC->InputLen == 0)
				return;

			memmove(TNC->ARDOPBuffer, &TNC->ARDOPBuffer[MsgLen],  TNC->InputLen);
			goto loop;
		}
	}	
	return;
}


VOID ARDOPProcessDataPacket(struct TNCINFO * TNC, UCHAR * Type, UCHAR * Data, int Length)
{
	// Info on Data Socket - just packetize and send on
	
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	int PacLen = 236;
	UINT * buffptr;
		
	TNC->TimeSinceLast = 0;

	if (strcmp(Type, "IDF") == 0)
	{
		// Place ID frames in Monitor Window and MH

		char Call[20];

		Data[Length] = 0;
		WritetoTrace(TNC, Data, Length);

		if (memcmp(Data, "ID:", 3) == 0)	// These seem to be transmitted ID's
		{
			memcpy(Call, &Data[3], 20);
			strlop(Call, ':'); 
			UpdateMH(TNC, Call, '!', 'I');
		}
		return;
	}

	STREAM->BytesRXed += Length;

	Data[Length] = 0;	
	Debugprintf("ARDOP: RXD %d bytes", Length);
	
	if (TNC->FECMode)
	{	
		Length = strlen(Data);
		if (Data[Length - 1] == 10)
			Data[Length - 1] = 13;	

	}

	if (strcmp(Type, "FEC") == 0)
	{
		// May be an APRS Message

		char * ptr1 = Data;
		char * ptr2 = strchr(ptr1, '>');
		int Len = 80;

		if (ptr2 && (ptr2 - ptr1) < 10)
		{
			// Could be APRS

			if (memcmp(ptr2 + 1, "AP", 2) == 0)
			{
				// assume it is

				char * ptr3 = strchr(ptr2, '|');
				struct _MESSAGE * buffptr = GetBuff();

				if (ptr3 == 0)
					return;

				*(ptr3++) = 0;		// Terminate TO call

				Len = strlen(ptr3);

				// Convert to ax.25 format

				if (buffptr == 0)
					return;			// No buffers, so ignore

				buffptr->PORT = TNC->Port;

				ConvToAX25(ptr1, buffptr->ORIGIN);
				ConvToAX25(ptr2 + 1, buffptr->DEST);
				buffptr->ORIGIN[6] |= 1;				// Set end of address
				buffptr->CTL = 3;
				buffptr->PID = 0xF0;
				memcpy(buffptr->L2DATA, ptr3, Len);
				buffptr->LENGTH  = 23 + Len;
				time(&buffptr->Timestamp);

				BPQTRACE((MESSAGE *)buffptr, TRUE);

				return;

			}
		}

		// FEC but not APRS. Discard if connected

		if (TNC->Streams[0].Connected)
			return;
	}

	WritetoTrace(TNC, Data, Length);

	// We can get messages of form ARQ [ConReq2000M: GM8BPQ-2 > OE3FQU]
	// when not connected.

	if (TNC->Streams[0].Connected == FALSE)
	{
		if (strcmp(Type, "ARQ") == 0)
		{
			if (Data[1] == '[')
			{
				// Log to MH
			
				char Call[20];
				char * ptr;

				ptr = strchr(Data, ':');

				if (ptr)
				{
					memcpy(Call, &ptr[2], 20);
					strlop(Call, ' '); 
					UpdateMH(TNC, Call, '!', 'I');
				}
			}
		}
	}



	//	May need to fragment

	while (Length)
	{
		int Fraglen = Length;

		if (Length > PACLEN)
			Fraglen = PACLEN;

		Length -= Fraglen;

		buffptr = GetBuff();	

		if (buffptr == 0)
			return;			// No buffers, so ignore
				
		memcpy(&buffptr[2], Data, Fraglen);

		Data += Fraglen;

		buffptr[1] = Fraglen;

		C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
	}

	return;
}

/*
INT_PTR CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	int Cmd = LOWORD(wParam);

	switch (message)
	{
	case WM_INITDIALOG:
	{
		struct TNCINFO * TNC = (struct TNCINFO * )lParam;
		char * ptr1, *ptr2;
		int ptr3 = 0;
		char Line[1000];
		int len;

		ptr1 = TNC->CaptureDevices;

		if (!ptr1)
			return 0;				// No Devices


		while (ptr2 = strchr(ptr1, ','))
		{
			len = ptr2 - ptr1;
			memcpy(&Line[ptr3], ptr1, len);
			ptr3 += len;
			Line[ptr3++] = '\r';
			Line[ptr3++] = '\n';

			ptr1 = ++ptr2;
		}
		Line[ptr3] = 0;
		strcat(Line, ptr1);
	
		SetDlgItemText(hDlg, IDC_CAPTURE, Line);

		ptr3 = 0;

		ptr1 = TNC->PlaybackDevices;
	
		if (!ptr1)
			return 0;				// No Devices


		while (ptr2 = strchr(ptr1, ','))
		{
			len = ptr2 - ptr1;
			memcpy(&Line[ptr3], ptr1, len);
			ptr3 += len;
			Line[ptr3++] = '\r';
			Line[ptr3++] = '\n';

			ptr1 = ++ptr2;
		}
		Line[ptr3] = 0;
		strcat(Line, ptr1);
	
		SetDlgItemText(hDlg, IDC_PLAYBACK, Line);

		SendDlgItemMessage(hDlg, IDC_PLAYBACK, EM_SETSEL, -1, 0);

//		ARDOPKillTNC(TNC);

		return TRUE; 
	}

	case WM_SIZING:
	{
		return TRUE;
	}

	case WM_ACTIVATE:

//		SendDlgItemMessage(hDlg, IDC_MESSAGE, EM_SETSEL, -1, 0);

		break;


	case WM_COMMAND:


		if (Cmd == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}

		return (INT_PTR)TRUE;

		break;
	}
	return (INT_PTR)FALSE;
}
*/


int ARDOPKillTNC(struct TNCINFO * TNC)
{
	if (TNC->WIMMORPID == 0)
		return 0;

	if (TNC->ProgramPath && _memicmp(TNC->ProgramPath, "REMOTE:", 7) == 0)
	{
		// Try to Kill TNC on a remote host

		SOCKET sock = socket(AF_INET,SOCK_DGRAM,0);
		struct sockaddr_in destaddr;
		char Msg[80];
		int Len;

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
		Len = sprintf(Msg, "KILL %d", TNC->WIMMORPID);
		sendto(sock, Msg, Len, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
		Sleep(100);
		closesocket(sock);

		TNC->WIMMORPID = 0;			// So we don't try again
		return 1;				// Cant tell if it worked, but assume ok
	}

#ifndef LINBPQ
	{
	HANDLE hProc;

	Debugprintf("ARDOPKillTNC Called for Pid %d", TNC->WIMMORPID);

	if (TNC->PTTMode)
		Rig_PTT(TNC->RIG, FALSE);			// Make sure PTT is down

	hProc =  OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, TNC->WIMMORPID);

	if (hProc)
	{
		TerminateProcess(hProc, 0);
		CloseHandle(hProc);
	}
	}
#endif
	TNC->WIMMORPID = 0;			// So we don't try again

	return 0;
}

BOOL ARDOPRestartTNC(struct TNCINFO * TNC)
{
	if (TNC->ProgramPath == NULL)
		return 0;

	if (_memicmp(TNC->ProgramPath, "REMOTE:", 7) == 0)
	{
		int n;
		
		// Try to start TNC on a remote host

		SOCKET sock = socket(AF_INET,SOCK_DGRAM,0);
		struct sockaddr_in destaddr;

		Debugprintf("trying to restart ARDOP TNC %s", TNC->ProgramPath);

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
	
		Debugprintf("Restart ARDOP TNC - sento returned %d", n);

		Sleep(100);
		closesocket(sock);

		return 1;				// Cant tell if it worked, but assume ok
	}
#ifndef WIN32
	{
		char * arg_list[] = {NULL, NULL};
		pid_t child_pid;	

		signal(SIGCHLD, SIG_IGN); // Silently (and portably) reap children. 

		//	Fork and Exec ARDOP

		printf("Trying to start %s\n", TNC->ProgramPath);

		arg_list[0] = TNC->ProgramPath;
	 
    	/* Duplicate this process. */ 

		child_pid = fork (); 

		if (child_pid == -1) 
 		{    				
			printf ("ARDOPStart fork() Failed\n"); 
			return 0;
		}

		if (child_pid == 0) 
 		{    				
			execvp (arg_list[0], arg_list); 
        
			/* The execvp  function returns only if an error occurs.  */ 

			printf ("Failed to run ARDOP\n"); 
			exit(0);			// Kill the new process
		}
		printf("Started ARDOP TNC\n");
		return TRUE;
	}								 
#else
	{
		int n = 0;
		
		STARTUPINFO  SInfo;			// pointer to STARTUPINFO 
	    PROCESS_INFORMATION PInfo; 	// pointer to PROCESS_INFORMATION 

		SInfo.cb=sizeof(SInfo);
		SInfo.lpReserved=NULL; 
		SInfo.lpDesktop=NULL; 
		SInfo.lpTitle=NULL; 
		SInfo.dwFlags=0; 
		SInfo.cbReserved2=0; 
	  	SInfo.lpReserved2=NULL; 

		Debugprintf("ARDOPRestartTNC Called for %s", TNC->ProgramPath);

		while (KillOldTNC(TNC->ProgramPath) && n++ < 100)
		{
			Sleep(100);
		}

		if (CreateProcess(TNC->ProgramPath, NULL, NULL, NULL, FALSE,0 ,NULL ,NULL, &SInfo, &PInfo))
		{
			Debugprintf("Restart TNC OK");
			TNC->WIMMORPID = PInfo.dwProcessId;
			return TRUE;
		}
		else
		{
			Debugprintf("Restart TNC Failed %d ", GetLastError());
			return FALSE;
		}
	}
#endif
	return 0;
}

VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
	// If all acked, send disc
	
	if (TNC->Streams[0].BytesOutstanding == 0)
		ARDOPSendCommand(TNC, "DISCONNECT", TRUE);
}

VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	ARDOPSendCommand(TNC, "ABORT", TRUE);
}

VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
	ARDOPReleaseTNC(TNC);

	if (TNC->FECMode)
	{
		TNC->FECMode = FALSE;
		ARDOPSendCommand(TNC, "SENDID", TRUE);
	}
}

