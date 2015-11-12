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
//	Telnet Driver for BPQ Switch 
//
//	Uses BPQ EXTERNAL interface
//

//#define WIN32_LEAN_AND_MEAN

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T


#include <stdio.h>
#include <stdlib.h>
#include "time.h"

#include "kernelresource.h"

#define IDM_DISCONNECT			2000
#define IDM_LOGGING				2100

#define MAXBLOCK 4096
#include "CHeaders.h"
#include "tncinfo.h"

#ifdef WIN32
#include <winioctl.h>
#include "WS2tcpip.h"
#else
#define TIOCOUTQ        0x5411
#define SIOCOUTQ        TIOCOUTQ        /* output queue size (not sent + not acked) */
#endif

#include "telnetserver.h"

#define MAX_PENDING_CONNECTS 4

extern char * PortConfig[];

static char ClassName[]="TELNETSERVER";
static char WindowTitle[] = "Telnet Server";
static int RigControlRow = 190;

static BOOL OpenSockets(struct TNCINFO * TNC);
static BOOL OpenSockets6(struct TNCINFO * TNC);
int ProcessHTTPMessage(struct ConnectionInfo * conn);
static VOID SetupListenSet(struct TNCINFO * TNC);
int IntDecodeFrame(MESSAGE * msg, char * buffer, int Stamp, UINT Mask, BOOL APRS, BOOL MCTL);
DllExport int APIENTRY SetTraceOptionsEx(int mask, int mtxparam, int mcomparam, int monUIOnly);
int WritetoConsoleLocal(char * buff);
BOOL TelSendPacket(int Stream, struct STREAMINFO * STREAM, UINT * buffptr);
int GetCMSHash(char * Challenge, char * Password);
int IsUTF8(unsigned char *cpt, int len);
int Convert437toUTF8(unsigned char * MsgPtr, int len, unsigned char * UTF);
int Convert1251toUTF8(unsigned char * MsgPtr, int len, unsigned char * UTF);
int Convert1252toUTF8(unsigned char * MsgPtr, int len, unsigned char * UTF);
VOID initUTF8();
int TrytoGuessCode(unsigned char * Char, int Len);
DllExport struct PORTCONTROL * APIENTRY GetPortTableEntryFromSlot(int portslot);


extern BPQVECSTRUC * TELNETMONVECPTR;


#ifndef LINBPQ
extern HKEY REGTREE;
extern HMENU hMainFrameMenu;
extern HMENU hBaseMenu;
extern HMENU hWndMenu;
extern HFONT hFont;
extern HBRUSH bgBrush;

extern HWND ClientWnd, FrameWnd;

extern HANDLE hInstance;
static RECT Rect;
#endif

extern int REALTIMETICKS;

extern struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

#define MaxSockets 26

struct UserRec RelayUser;
struct UserRec CMSUser;
struct UserRec HostUser= {"","Host"};
struct UserRec TriModeUser;



static char AttemptsMsg[] = "Too many attempts - Disconnected\r\n";
static char disMsg[] = "Disconnected by SYSOP\r\n";

static char BlankCall[]="         ";

BOOL LogEnabled = FALSE;
BOOL CMSLogEnabled = TRUE;

static	HMENU hMenu, hPopMenu, hPopMenu2, hPopMenu3;		// handle of menu 

static int ProcessLine(char * buf, int Port);
VOID __cdecl Debugprintf(const char * format, ...);
char * strlop(char * buf, char delim);

unsigned long _beginthread( void( *start_address )(), unsigned stack_size, struct TNCINFO * arglist);

#ifndef LINBPQ
LRESULT CALLBACK TelWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
#endif

int DisplaySessions(struct TNCINFO * TNC);
int DoStateChange(int Stream);
int Connected(int Stream);
int Disconnected(int Stream);
//int DeleteConnection(con);
static int Socket_Accept(struct TNCINFO * TNC, int SocketId);
static int Socket_Data(struct TNCINFO * TNC, int SocketId,int error, int eventcode);
static int DataSocket_Read(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, struct STREAMINFO * STREAM);
int DataSocket_ReadFBB(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, int Stream);
int DataSocket_ReadRelay(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, struct STREAMINFO * STREAM);
int DataSocket_ReadHTTP(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, int Stream);
int DataSocket_Write(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET TCPSock);
int DataSocket_Disconnect(struct TNCINFO * TNC, struct ConnectionInfo * sockptr);
BOOL ProcessTelnetCommand(struct ConnectionInfo * sockptr, byte * Msg, int Len);
int ShowConnections(struct TNCINFO * TNC);
int Terminate();
int SendtoSocket(SOCKET TCPSock,char * Msg);
int WriteLog(char * msg);
VOID WriteCMSLog(char * msg);
byte * EncodeCall(byte * Call);
VOID SendtoNode(struct TNCINFO * TNC, int Stream, char * Msg, int MsgLen);
DllExport int APIENTRY GetNumberofPorts();

BOOL CheckCMS(struct TNCINFO * TNC);
int TCPConnect(struct TNCINFO * TNC, struct TCPINFO * TCP, struct STREAMINFO * STREAM, char * Host, int Port, BOOL FBB);
int CMSConnect(struct TNCINFO * TNC, struct TCPINFO * TCP, struct STREAMINFO * STREAM,  int Stream);
int Telnet_Connected(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, int Error);
BOOL ProcessConfig();
VOID FreeConfig();
VOID SaveCMSHostInfo(int port, struct TCPINFO * TCP, int CMSNo);
VOID GetCMSCachedInfo(struct TNCINFO * TNC);
BOOL CMSCheck(struct TNCINFO * TNC, struct TCPINFO * TCP);
VOID Tel_Format_Addr(struct ConnectionInfo * sockptr, char * dst);
VOID ProcessTrimodeCommand(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, char * MsgPtr);
VOID ProcessTrimodeResponse(struct TNCINFO * TNC, struct STREAMINFO * STREAM, unsigned char * MsgPtr, int Msglen);
VOID ProcessTriModeDataMessage(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, struct STREAMINFO * STREAM);

void BuffertoNode(struct ConnectionInfo * sockptr, char * MsgPtr, int InputLen)
{
	// Queue to Node. Data may arrive it large quatities, possibly exceeding node buffer capacity

	if (sockptr->FromHostBuffPutptr + InputLen > sockptr->FromHostBufferSize)
	{
		sockptr->FromHostBufferSize += 10000;
		sockptr->FromHostBuffer = realloc(sockptr->FromHostBuffer, sockptr->FromHostBufferSize);
	}

	memcpy(&sockptr->FromHostBuffer[sockptr->FromHostBuffPutptr], MsgPtr, InputLen); 

	sockptr->FromHostBuffPutptr += InputLen;
	sockptr->InputLen = 0;

	return;
	}

BOOL SendAndCheck(struct ConnectionInfo * sockptr, unsigned char * MsgPtr, int len, int flags)
{
	int err;
	int sent = send(sockptr->socket, MsgPtr, len, flags);

	if (sent == len)
		return TRUE;			// OK

	err = WSAGetLastError();
				
	Debugprintf("TCP Send Failed - Sent %d should be %d err %d - requeue data", sent, len, err);

	if (err == 10035 || err == 115 || err == 36)		//EWOULDBLOCK
	{
		// Save unsent data

		if (sent == -1)				// -1 means none sent
			sent = 0;

		sockptr->ResendBuffer = malloc(len - sent);
		sockptr->ResendLen = len - sent;

		memmove(sockptr->ResendBuffer, MsgPtr + sent, len - sent);
	}
	return FALSE;
}

VOID SendPortsForMonitor(SOCKET sock)
{
	UCHAR PortInfo[1500] = {0xff, 0xff};
	UCHAR * ptr = &PortInfo[2];
	char ID[31] = "";
	struct PORTCONTROL * PORT;
	int i, n, p;

	// Sends the ID of each Port to TermTCP

	p = GetNumberofPorts();

	ptr += sprintf(ptr, "%d|", p);

	for (n=1; n <= p; n++)
	{
		PORT = GetPortTableEntryFromSlot(n);
		
		memcpy(ID, PORT->PORTDESCRIPTION, 30);

		for (i = 29; i; i--)
		{
			if (ID[i] != ' ')
				break;

			ID[i] = 0;
		}

		ptr += sprintf(ptr,"%d %s|", PORT->PORTNUMBER, ID);
	}
	send(sock, PortInfo, ptr - PortInfo, 0);
}

ProcessLine(char * buf, int Port)
{
	UCHAR * ptr;
	UCHAR * ptr1;

	char * p_ipad = 0;
	char * p_port = 0;
	unsigned short WINMORport = 0;
	int len=510;
	char errbuf[256];
	char * param;
	char * value;
	char *User, *Pwd, *UserCall, *Secure, * Appl;
	int End = strlen(buf) -1;
	struct TNCINFO * TNC;
	struct TCPINFO * TCP;

	strcpy(errbuf,buf);			// save in case of error

	if (buf[End] == 10)
		buf[End]=0;			// remove newline

	if(buf[0] =='#') return (TRUE);			// comment

	if(buf[0] ==';') return (TRUE);			// comment

	ptr=strchr(buf,'=');

	if (!ptr)
		ptr=strchr(buf,' ');

	if (!ptr)
		return 0;

	if (TNCInfo[Port] == NULL)
	{
		TNC = TNCInfo[Port] = zalloc(sizeof(struct TNCINFO));
		TCP = TNC->TCPInfo = zalloc(sizeof (struct TCPINFO)); // Telnet Server Specific Data

		TCP->MaxSessions = 10;				// Default Values
		TNC->Hardware = H_TELNET;
		TCP->IPV4 = TRUE;
	}

	TNC = TNCInfo[Port];
	TCP = TNC->TCPInfo;

	param=buf;
	*(ptr)=0;
	value=ptr+1;

	if (_stricmp(param, "IPV4") == 0)
		TCP->IPV4 = atoi(value);
	else
	if (_stricmp(param, "IPV6") == 0)
		TCP->IPV6 = atoi(value);
	else
	if (_stricmp(param, "CMS") == 0)
		TCP->CMS = atoi(value);
	else
	if (_stricmp(param, "CMSPASS") == 0)
	{
		if (strlen(value) > 79)
			value[79] = 0;
		strcpy(TCP->SecureCMSPassword, value);
		strlop(TCP->SecureCMSPassword, 32);
		strlop(TCP->SecureCMSPassword, 13);
		_strupr(TCP->SecureCMSPassword);
	}
	else
	if (_stricmp(param, "CMSCALL") == 0)
	{
		if (strlen(value) > 9)
			value[9] = 0;
		strcpy(TCP->GatewayCall, value);
		strlop(TCP->GatewayCall, 13);
		_strupr(TCP->GatewayCall);
	}
	else
	if (_stricmp(param,"LOGGING") == 0)
		LogEnabled = atoi(value);
	else
	if (_stricmp(param,"CMSLOGGING") == 0)
		CMSLogEnabled = atoi(value);
	else
	if (_stricmp(param,"DisconnectOnClose") == 0)
		TCP->DisconnectOnClose = atoi(value);
	else
	if (_stricmp(param,"CloseOnDisconnect") == 0)
		TCP->DisconnectOnClose = atoi(value);
	else
		if (_stricmp(param,"TCPPORT") == 0)
			TCP->TCPPort = atoi(value);
		else
		if (_stricmp(param,"TRIMODEPORT") == 0)
			TCP->TriModePort = atoi(value);
		else
		if (_stricmp(param,"HTTPPORT") == 0)
			TCP->HTTPPort = atoi(value);
		else
		if ((_stricmp(param,"CMDPORT") == 0) || (_stricmp(param,"LINUXPORT") == 0))
		{
			int n = 0;
			char * context;
			char * ptr = strtok_s(value, ", ", &context);

			while (ptr && n < 33)
			{
				TCP->CMDPort[n++] = atoi(ptr);
				ptr = strtok_s(NULL, ", ", &context);
			}
		}
		else
		if (_stricmp(param,"RELAYHOST") == 0)
		{
			int n = 0;
			char * context;
			char * ptr = strtok_s(value, ", \r", &context);

			strcpy(TCP->RELAYHOST, ptr);
		}

		else
		if (_stricmp(param,"FALLBACKTORELAY") == 0)
		{
			int n = 0;
			char * context;
			char * ptr = strtok_s(value, ", \r", &context);

			TCP->FallbacktoRelay = atoi(value);
		}

		else
		if (_stricmp(param,"FBBPORT") == 0)
		{
			int n = 0;
			char * context;
			char * ptr = strtok_s(value, ", ", &context);

			while (ptr && n < 99)
			{
				TCP->FBBPort[n++] = atoi(ptr);
				ptr = strtok_s(NULL, ", ", &context);
			}
		}
		else
		if (_stricmp(param,"RELAYAPPL") == 0)
		{
			TCP->RelayPort = 8772;
			strcpy(TCP->RelayAPPL, value);
			strcat(TCP->RelayAPPL, "\r");
		}
		else
		//		if (strcmp(param,"LOGINRESPONSE") == 0) cfgLOGINRESPONSE = value;
//	    if (strcmp(param,"PASSWORDRESPONSE") == 0) cfgPASSWORDRESPONSE = value;

	    if (_stricmp(param,"LOGINPROMPT") == 0)
		{
			ptr1 = strchr(value, 13);
			if (ptr1) *ptr1 = 0;
			strcpy(TCP->LoginMsg,value);
		}
		else
	    if (_stricmp(param,"PASSWORDPROMPT") == 0)
		{
			ptr1 = strchr(value, 13);
			if (ptr1) *ptr1 = 0;
			strcpy(TCP->PasswordMsg, value);
		}
		else
	    if (_stricmp(param,"HOSTPROMPT") == 0)
			strcpy(TCP->cfgHOSTPROMPT, value);
		else
		if (_stricmp(param,"LOCALECHO") == 0)
			strcpy(TCP->cfgLOCALECHO, value);
		else
		if (_stricmp(param,"MAXSESSIONS") == 0)
		{
			TCP->MaxSessions = atoi(value);
			if (TCP->MaxSessions > MaxSockets ) TCP->MaxSessions = MaxSockets;
		}
		else
		if (_stricmp(param,"CTEXT") == 0 )
		{
			int len = strlen (value);

			if (value[len -1] == ' ')
				value[len -1] = 0;

			strcpy(TCP->cfgCTEXT, value);

			// Replace \n Signon string with cr lf

			ptr = &TCP->cfgCTEXT[0];

		scanCTEXT:

			ptr = strstr(ptr, "\\n");
    
			if (ptr)
			{    
				*(ptr++)=13;			// put in cr
				*(ptr++)=10;			// put in lf

				goto scanCTEXT;
			}  
		}
		else
		if (_stricmp(param,"USER") == 0)
		{
			struct UserRec * USER;
			char Param[8][256];
			char * ptr1, * ptr2;
			int n = 0;

			// USER=user,password,call,appl,SYSOP

			memset(Param, 0, 2048);
			strlop(value, 13);
			strlop(value, ';');

			ptr1 = value;

			while (ptr1 && *ptr1 && n < 8)
			{
				ptr2 = strchr(ptr1, ',');
				if (ptr2) *ptr2++ = 0;

				strcpy(&Param[n][0], ptr1);
				strlop(Param[n++], ' ');
				ptr1 = ptr2;
				while(ptr1 && *ptr1 && *ptr1 == ' ')
					ptr1++;
			}


			User = &Param[0][0];
			Pwd = &Param[1][0];
			UserCall = &Param[2][0];
			Appl = &Param[3][0];
			Secure = &Param[4][0];

			if (User[0] == 0 || Pwd[0] == 0 || UserCall[0] == 0) // invalid record
				return 0;

			_strupr(UserCall);

			if (TCP->NumberofUsers == 0)
				TCP->UserRecPtr = malloc(4);
			else
				TCP->UserRecPtr = realloc(TCP->UserRecPtr, (TCP->NumberofUsers+1)*4);

			USER = zalloc(sizeof(struct UserRec));

			TCP->UserRecPtr[TCP->NumberofUsers] = USER;
 
			USER->Callsign = _strdup(UserCall);
			USER->Password = _strdup(Pwd);
			USER->UserName = _strdup(User);
			USER->Appl = zalloc(32);
			USER->Secure = FALSE;

			if (_stricmp(Secure, "SYSOP") == 0)
				USER->Secure = TRUE;
		
			if (Appl[0] && strcmp(Appl, "\"\"") != 0)
			{
				strcpy(USER->Appl, _strupr(Appl));
				strcat(USER->Appl, "\r\n");
			}
			TCP->NumberofUsers += 1;
		}
		else
			return FALSE;

	return TRUE;
}

int BPQTRACE(MESSAGE * Msg, BOOL TOAPRS);

static int MaxStreams = 26;

struct TNCINFO * CreateTTYInfo(int port, int speed);
BOOL OpenConnection(int);
BOOL SetupConnection(int);
BOOL CloseConnection(struct TNCINFO * conn);
BOOL WriteCommBlock(struct TNCINFO * TNC);
BOOL DestroyTTYInfo(int port);
void CheckRX(struct TNCINFO * TNC);
VOID TelnetPoll(int Port);
VOID ProcessDEDFrame(struct TNCINFO * TNC, UCHAR * rxbuff, int len);
VOID ProcessTermModeResponse(struct TNCINFO * TNC);
VOID DoTNCReinit(struct TNCINFO * TNC);
VOID DoTermModeTimeout(struct TNCINFO * TNC);

VOID ProcessPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID ProcessKPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID ProcessKHOSTPacket(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID ProcessKNormCommand(struct TNCINFO * TNC, UCHAR * rxbuffer);
VOID ProcessHostFrame(struct TNCINFO * TNC, UCHAR * rxbuffer, int Len);
VOID DoMonitor(struct TNCINFO * TNC, UCHAR * Msg, int Len);


static VOID WritetoTrace(int Stream, char * Msg, int Len)
{
	int index = 0;
	UCHAR * ptr1 = Msg, * ptr2;
	UCHAR Line[1000];
	int LineLen, i;
	char logmsg[200];

lineloop:

	if (Len > 0)
	{
		//	copy text to file a line at a time	
					
		ptr2 = memchr(ptr1, 13 , Len);

		if (ptr2)
		{
			ptr2++;
			LineLen = ptr2 - ptr1;
			Len -= LineLen;

			if (LineLen > 900)
				goto Skip;

			memcpy(Line, ptr1, LineLen);
			memcpy(&Line[LineLen - 1], "<cr>", 4);
			LineLen += 3;

			if ((*ptr2) == 10)
			{
				memcpy(&Line[LineLen], "<lf>", 4);
				LineLen += 4;
				ptr2++;
				Len --;
			}
			
			Line[LineLen] = 0;

			// If line contains any data above 7f, assume binary and dont display

			for (i = 0; i < LineLen; i++)
			{
				if (Line[i] > 127)
					goto Skip;
			}

			if (strlen(Line) < 100)
			{
				sprintf(logmsg,"%d %s\r\n", Stream, Line);
				WriteCMSLog (logmsg);
			}

		Skip:
			ptr1 = ptr2;
			goto lineloop;
		}

		// No CR

		for (i = 0; i < Len; i++)
		{
			if (ptr1[i] > 127)
				break;
		}

		if (i == Len)			// No binary data
		{
			if (strlen(ptr1) < 100)
			{
				sprintf(logmsg,"%d %s\r\n", Stream, ptr1);
				WriteCMSLog (logmsg);
			}
		}
	}
}

static int ExtProc(int fn, int port,unsigned char * buff)
{
	int txlen = 0, n;
	UINT * buffptr;
	struct TNCINFO * TNC = TNCInfo[port];
	struct TCPINFO * TCP;

	int Stream;
	struct ConnectionInfo * sockptr;
	struct STREAMINFO * STREAM;

	if (TNC == NULL)
		return 0;					// Not configured

	switch (fn)
	{
	case 7:			

		// 100 mS Timer. Now needed, as Poll can be called more ferquently in some circimstances

		while (TNC->PortRecord->UI_Q)			// Release anything accidentally put on UI_Q
		{
			buffptr = Q_REM(&TNC->PortRecord->UI_Q);
			ReleaseBuffer(buffptr);
		}

		TCP = TNC->TCPInfo;
	
		if (TCP->CMS)
		{
			TCP->CheckCMSTimer++;

			if (TCP->CMSOK)
			{
				if (TCP->CheckCMSTimer > 600 * 15)
					CheckCMS(TNC);
			}
			else
			{
				if (TCP->CheckCMSTimer > 600 * 2)
					CheckCMS(TNC);
			}
		}

		// We now use persistent HTTP sessions, so need to close after a reasonable time
		
		for (n = 0; n <= TCP->MaxSessions; n++)
		{
			sockptr = TNC->Streams[n].ConnectionInfo;

			if (sockptr->SocketActive && sockptr->HTTPMode && sockptr->LastSendTime)
			{
				if ((REALTIMETICKS -sockptr->LastSendTime) > 1500)	// ~ 2.5 mins
				{
					closesocket(sockptr->socket);
					sockptr->SocketActive = FALSE;
					ShowConnections(TNC);
				}
			}
		}



		return 0;

	case 1:				// poll

		for (Stream = 0; Stream <= MaxStreams; Stream++)
		{	
			TRANSPORTENTRY * SESS;
			struct ConnectionInfo * sockptr = TNC->Streams[Stream].ConnectionInfo;

			if (sockptr && sockptr->UserPointer == &CMSUser)	// Connected to CMS
			{
				SESS = TNC->PortRecord->ATTACHEDSESSIONS[sockptr->Number];

				if (SESS)
				{
					n = SESS->L4KILLTIMER;
					if (n < (SESS->L4LIMIT - 120))
					{
						SESS->L4KILLTIMER = SESS->L4LIMIT - 120;
						SESS = SESS->L4CROSSLINK;
						if (SESS)
							SESS->L4KILLTIMER = SESS->L4LIMIT - 120;
					}
				}
			}

			STREAM = &TNC->Streams[Stream];

			if (STREAM->NeedDisc)
			{
				STREAM->NeedDisc--;

				if (STREAM->NeedDisc == 0)
				{
					// Send the DISCONNECT

					STREAM->ReportDISC = TRUE;
				}
			}

			if (STREAM->ReportDISC)
			{
				STREAM->ReportDISC = FALSE;
				buff[4] = Stream;

				return -1;
			}
		}

		TelnetPoll(port);

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

		txlen = GetLengthfromBuffer(buff) - 8;	

		buffptr[1] = txlen;
		memcpy(buffptr+2, &buff[8], txlen);
		
		C_Q_ADD(&STREAM->BPQtoPACTOR_Q, buffptr);
		STREAM->FramesQueued++;

		return (0);


	case 3:				// CHECK IF OK TO SEND. Also used to check if TNC is responding
		
		Stream = (int)buff;
		
		STREAM = &TNC->Streams[Stream];

		if (STREAM->FramesQueued  > 40)
			return (257);						// Busy

		return 256;		// OK

#define SD_BOTH         0x02

	case 4:				// reinit
	{
		struct _EXTPORTDATA * PortRecord;

#ifndef LINBPQ
		HWND SavehDlg, SaveCMS, SaveMonitor;
		HMENU SaveMenu1, SaveMenu2, SaveMenu3; 
#endif	
		int n, i;

		ProcessConfig();
		FreeConfig();

		for (n = 1; n <= TNC->TCPInfo->CurrentSockets; n++)
		{
			sockptr = TNC->Streams[n].ConnectionInfo;
			sockptr->SocketActive = FALSE;
			closesocket(sockptr->socket);
		}

		TCP = TNC->TCPInfo;

		shutdown(TCP->TCPSock, SD_BOTH);
		shutdown(TCP->sock6, SD_BOTH);

		n = 0;
		while (TCP->FBBsock[n])
			shutdown(TCP->FBBsock[n++], SD_BOTH);

		shutdown(TCP->Relaysock, SD_BOTH);
		shutdown(TCP->HTTPsock, SD_BOTH);
		shutdown(TCP->HTTPsock6, SD_BOTH);


		n = 0;
		while (TCP->FBBsock6[n])
			shutdown(TCP->FBBsock[n++], SD_BOTH);

		shutdown(TCP->Relaysock6, SD_BOTH);
		Sleep(500);

		closesocket(TCP->TCPSock);
		closesocket(TCP->sock6);

		n = 0;
		while (TCP->FBBsock[n])
			closesocket(TCP->FBBsock[n++]);

		n = 0;
		while (TCP->FBBsock6[n])
			closesocket(TCP->FBBsock6[n++]);

		closesocket(TCP->Relaysock);
		closesocket(TCP->Relaysock6);
		closesocket(TCP->HTTPsock);
		closesocket(TCP->HTTPsock6);

		// Save info from old TNC record
			
		n = TNC->Port;
		PortRecord = TNC->PortRecord;
#ifndef LINBPQ
		SavehDlg = TNC->hDlg;
		SaveCMS = TCP->hCMSWnd;
		SaveMonitor = TNC->hMonitor;
		SaveMenu1 = TCP->hActionMenu;
		SaveMenu2 = TCP->hDisMenu;
		SaveMenu3 = TCP->hLogMenu;
#endif
		// Free old TCP Session Stucts

		for (i = 0; i <= TNC->TCPInfo->MaxSessions; i++)
		{
			free(TNC->Streams[i].ConnectionInfo);
		}

		ReadConfigFile(TNC->Port, ProcessLine);

		TNC = TNCInfo[n];
		TNC->Port = n;
		TNC->Hardware = H_TELNET;
		TNC->RIG = &TNC->DummyRig;			// Not using Rig control, so use Dummy

		// Get Menu Handles

		TCP = TNC->TCPInfo;
#ifndef LINBPQ
		TNC->hDlg = SavehDlg;
		TCP->hCMSWnd = SaveCMS;
		TNC->hMonitor = SaveMonitor;
		TCP->hActionMenu = SaveMenu1;
		TCP->hDisMenu = SaveMenu2;
		TCP->hLogMenu = SaveMenu3;

		CheckMenuItem(TCP->hActionMenu, 3, MF_BYPOSITION | TNC->TCPInfo->CMS<<3);
		CheckMenuItem(TCP->hLogMenu, 0, MF_BYPOSITION | LogEnabled<<3);
		CheckMenuItem(TCP->hLogMenu, 1, MF_BYPOSITION | CMSLogEnabled<<3);
#endif
		// Malloc TCP Session Stucts

		for (i = 0; i <= TNC->TCPInfo->MaxSessions; i++)
		{			
			TNC->Streams[i].ConnectionInfo = zalloc(sizeof(struct ConnectionInfo));
			TCP->CurrentSockets = i;  //Record max used to save searching all entries
#ifndef LINBPQ
			if (i > 0)
				ModifyMenu(TCP->hDisMenu,i - 1 ,MF_BYPOSITION | MF_STRING,IDM_DISCONNECT + 1, ".");
#endif
		}

		TNC->PortRecord = PortRecord;

		Sleep(500);
		OpenSockets(TNC);
		OpenSockets6(TNC);
		SetupListenSet(TNC);
		CheckCMS(TNC);
		ShowConnections(TNC);
	}

	break;

	case 5:				// Close

		TCP = TNC->TCPInfo;

		for (n = 1; n <= TCP->CurrentSockets; n++)
		{
			sockptr = TNC->Streams[n].ConnectionInfo;
			closesocket(sockptr->socket);
		}
	
		shutdown(TCP->TCPSock, SD_BOTH);

		n = 0;
		while (TCP->FBBsock[n])
			shutdown(TCP->FBBsock[n++], SD_BOTH);

		shutdown(TCP->Relaysock, SD_BOTH);
		shutdown(TCP->HTTPsock, SD_BOTH);
		shutdown(TCP->HTTPsock6, SD_BOTH);

		shutdown(TCP->sock6, SD_BOTH);

		n = 0;
		while (TCP->FBBsock6[n])
			shutdown(TCP->FBBsock6[n++], SD_BOTH);

		shutdown(TCP->Relaysock6, SD_BOTH);

		Sleep(500);
		closesocket(TCP->TCPSock);

		n = 0;
		while (TCP->FBBsock[n])
			closesocket(TCP->FBBsock[n++]);

		closesocket(TCP->Relaysock);

		closesocket(TCP->sock6);

		n = 0;
		while (TCP->FBBsock6[n])
			closesocket(TCP->FBBsock6[n++]);

		closesocket(TCP->Relaysock6);
		closesocket(TCP->HTTPsock);
		closesocket(TCP->HTTPsock6);

		return (0);

	case 6:				// Scan Control

		return 0;		// None Yet

	}
	return 0;

}



static int WebProc(struct TNCINFO * TNC, char * Buff, BOOL LOCAL)
{
	int Len;
	char msg[256];
	struct ConnectionInfo * sockptr;
	int i,n;

	char CMSStatus[80] = "";

	if (TNC->TCPInfo->CMS)
	{
		if (TNC->TCPInfo->CMSOK)
			strcpy(CMSStatus, "CMS Ok");
		else
			strcpy(CMSStatus, "No CMS");
	}

	Len = sprintf(Buff, "<html><meta http-equiv=expires content=0><meta http-equiv=refresh content=15>"
	"<head><title>Telnet Status</title></head><body><b>Telnet Status &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%s</b>", CMSStatus);

	Len += sprintf(&Buff[Len], "<table style=\"text-align: left; width: 250px; font-family: monospace; align=center \" border=1 cellpadding=2 cellspacing=2>");


	Len += sprintf(&Buff[Len], "<tr><th>User</th><th>Callsign</th><th>Queue</th></tr>");

	for (n = 1; n <= TNC->TCPInfo->CurrentSockets; n++)
	{
		sockptr=TNC->Streams[n].ConnectionInfo;

		if (!sockptr->SocketActive)
		{
			strcpy(msg,"<tr><td>Idle</td><td>&nbsp;</td><td>&nbsp;</td></tr>");
		}
		else
		{
			if (sockptr->UserPointer == 0)
			{
				if (sockptr->HTTPMode)
				{
					char Addr[100];
					Tel_Format_Addr(sockptr, Addr);
					sprintf(msg,"<tr><td>HTTP<%s</td><td>&nbsp;</td><td>&nbsp;</td></tr>", Addr);
				}
				else
					strcpy(msg,"<tr><td>Logging in</td><td>&nbsp;</td><td>&nbsp;</td></tr>");
			}
			else
			{
				i=sprintf(msg,"<tr><td>%s</td><td>%s</td><td>%d</td></tr>",
					sockptr->UserPointer->UserName, sockptr->Callsign, 
					sockptr->FromHostBuffPutptr - sockptr->FromHostBuffGetptr);
			}
		}
		Len += sprintf(&Buff[Len], "%s", msg);
	}

	Len += sprintf(&Buff[Len], "</table>");
	return Len;
}



UINT TelnetExtInit(EXTPORTDATA * PortEntry)
{
	char msg[500];
	struct TNCINFO * TNC;
	struct TCPINFO * TCP;
	int port;
	char * ptr;
	int i;
	HWND x=0;

/*
	UCHAR NC[257];
	WCHAR WC[1024];
	
	int WLen, NLen;

	UINT UTF[256] = {0};
	UINT UTFL[256];

	int n, u;

	for (n = 0; n < 256; n++)
		NC[n] =n ;

	n = MultiByteToWideChar(437, 0, NC, 256, &WC[0], 1024); 

	for (n = 0; n < 256; n++)
		UTFL[n] = WideCharToMultiByte(CP_UTF8, 0, &WC[n], 1, &UTF[n], 1024 , NULL, NULL);

	// write UTF8 data as source

	{
	HANDLE Handle;
	int i, n, Len;
	char Line [256];


	Handle = CreateFile("c:\\UTF8437Data.c", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	n = wsprintf (Line, "unsigned int CP437toUTF8Data[128] = {\r\n");

	WriteFile(Handle, Line ,n , &n, NULL);

	if (Handle != INVALID_HANDLE_VALUE)
	{
		for (i = 128; i < 256; i += 4)
		{
			n = wsprintf (Line, "  %d, %d, %d, %d,  \r\n",
				UTF[i], UTF[i+1], UTF[i+2], UTF[i+3]);
			WriteFile(Handle, Line ,n , &n, NULL);

		}

		WriteFile(Handle, "};\r\n", 4, &n, NULL); 
	}
	n = wsprintf (Line, "unsigned int CP437toUTF8DataLen[128] = {\r\n");

	WriteFile(Handle, Line ,n , &n, NULL);

	if (Handle != INVALID_HANDLE_VALUE)
	{
		for (i = 128; i < 256;i += 4)
		{
			n = wsprintf (Line, "  %d, %d, %d, %d,  \r\n",
				UTFL[i], UTFL[i+1], UTFL[i+2], UTFL[i+3]);
			WriteFile(Handle, Line ,n , &n, NULL);

		}

		WriteFile(Handle, "};\r\n", 4, &n, NULL); 

		SetEndOfFile(Handle);
	
		CloseHandle(Handle);
	}
	}
*/

	initUTF8();

	sprintf(msg,"Telnet Server ");
	WritetoConsoleLocal(msg);

	port=PortEntry->PORTCONTROL.PORTNUMBER;

	ReadConfigFile(port, ProcessLine);

	TNC = TNCInfo[port];

	if (TNC == NULL)
	{
		// Not defined in Config file

		WritetoConsoleLocal("\n");
		return (int)ExtProc;
	}

	TCP = TNC->TCPInfo;

	TNC->Port = port;

	TNC->Hardware = H_TELNET;

	PortEntry->MAXHOSTMODESESSIONS = TNC->TCPInfo->MaxSessions + 1;		// Default

	TNC->PortRecord = PortEntry;

	if (PortEntry->PORTCONTROL.PORTCALL[0] != 0)
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);

	PortEntry->PORTCONTROL.PROTOCOL = 10;	// WINMOR/Pactor
	PortEntry->PORTCONTROL.PORTQUALITY = 0;
	PortEntry->SCANCAPABILITIES = NONE;		// No Scan Control 
	PortEntry->PERMITGATEWAY = TRUE;

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	TELNETMONVECPTR->HOSTAPPLFLAGS = 0x80;		// Requext Monitoring

	if (TCP->LoginMsg[0] == 0)
		strcpy(TCP->LoginMsg, "user:");
	if (TCP->PasswordMsg[0] == 0)
		strcpy(TCP->PasswordMsg, "password:");
	if (TCP->cfgCTEXT[0] == 0)
	{
		char Call[10];
		memcpy(Call, MYNODECALL, 10);
		strlop(Call, ' ');

		sprintf(TCP->cfgCTEXT, "Connected to %s's Telnet Server\r\n\r\n", Call);
	}

	TNC->WebWindowProc = WebProc;
	TNC->WebWinX = 260;
	TNC->WebWinY = 325;

#ifndef LINBPQ

	CreatePactorWindow(TNC, ClassName, WindowTitle, RigControlRow, TelWndProc, 400, 300);

	TCP->hCMSWnd = CreateWindowEx(0, "STATIC", "CMS OK ", WS_CHILD | WS_VISIBLE,
			240,0,50,16, TNC->hDlg, NULL, hInstance, NULL);

	SendMessage(TCP->hCMSWnd, WM_SETFONT, (WPARAM)hFont, 0);

	x = CreateWindowEx(0, "STATIC", " User      Callsign  Queue", WS_CHILD | WS_VISIBLE,
			0,0,240,16, TNC->hDlg, NULL, hInstance, NULL);

	SendMessage(x, WM_SETFONT, (WPARAM)hFont, 0);


	TNC->hMonitor= CreateWindowEx(0, "LISTBOX", "", WS_CHILD | WS_VISIBLE  | WS_VSCROLL,
			0,20,400,2800, TNC->hDlg, NULL, hInstance, NULL);

	SendMessage(TNC->hMonitor, WM_SETFONT, (WPARAM)hFont, 0);

	hPopMenu = CreatePopupMenu();
	hPopMenu2 = CreatePopupMenu();
	hPopMenu3 = CreatePopupMenu();

	AppendMenu(hPopMenu, MF_STRING + MF_POPUP, (UINT)hPopMenu2,"Logging Options");
	AppendMenu(hPopMenu, MF_STRING + MF_POPUP, (UINT)hPopMenu3,"Disconnect User");
	AppendMenu(hPopMenu, MF_STRING, TELNET_RECONFIG, "ReRead Config");
	AppendMenu(hPopMenu, MF_STRING, CMSENABLED, "CMS Access Enabled");
	AppendMenu(hPopMenu, MF_STRING, USECACHEDCMS, "Using Cached CMS Addresses");

	AppendMenu(hPopMenu2, MF_STRING, IDM_LOGGING, "Log incoming connections");
	AppendMenu(hPopMenu2, MF_STRING, IDM_CMS_LOGGING, "Log CMS Connections");

	AppendMenu(hPopMenu3, MF_STRING, IDM_DISCONNECT, "1");

	TCP->hActionMenu = hPopMenu;

	CheckMenuItem(TCP->hActionMenu, 3, MF_BYPOSITION | TCP->CMS<<3);

	TCP->hLogMenu = hPopMenu2;
	TCP->hDisMenu = hPopMenu3;

	CheckMenuItem(TCP->hLogMenu, 0, MF_BYPOSITION | LogEnabled<<3);
	CheckMenuItem(TCP->hLogMenu, 1, MF_BYPOSITION | CMSLogEnabled<<3);

//	ModifyMenu(hMenu, 1, MF_BYPOSITION | MF_OWNERDRAW | MF_STRING, 10000,  0); 
 
#endif

	// Malloc TCP Session Stucts

	for (i = 0; i <= TNC->TCPInfo->MaxSessions; i++)
	{
		TNC->Streams[i].ConnectionInfo = zalloc(sizeof(struct ConnectionInfo));
		TNC->Streams[i].ConnectionInfo->Number = i;
		TCP->CurrentSockets = i;  //Record max used to save searching all entries

		sprintf(msg,"%d", i);

#ifndef LINBPQ
		if (i > 1)
			AppendMenu(TCP->hDisMenu, MF_STRING, IDM_DISCONNECT ,msg);
#endif
	}

	OpenSockets(TNC);
	OpenSockets6(TNC);
	SetupListenSet(TNC);
	TNC->RIG = &TNC->DummyRig;			// Not using Rig control, so use Dummy

	if (TCP->CMS)
		CheckCMS(TNC);

	WritetoConsoleLocal("\n");

	ShowConnections(TNC);

	return ((int)ExtProc);
}

SOCKET OpenSocket4(struct TNCINFO * xTNC, int port)
{
	struct sockaddr_in  local_sin;  /* Local socket - internet style */
	struct sockaddr_in * psin;
	SOCKET sock = 0;
	u_long param=1;

	char szBuff[80];

	psin=&local_sin;
	psin->sin_family = AF_INET;
	psin->sin_addr.s_addr = INADDR_ANY;

	if (port)
	{
		sock = socket(AF_INET, SOCK_STREAM, 0);

	    if (sock == INVALID_SOCKET)
		{
	        sprintf(szBuff, "socket() failed error %d\n", WSAGetLastError());
			WritetoConsoleLocal(szBuff);
			return 0;
		}

		setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (char *)&param,4);

 
		psin->sin_port = htons(port);        // Convert to network ordering 

		if (bind( sock, (struct sockaddr FAR *) &local_sin, sizeof(local_sin)) == SOCKET_ERROR)
		{
			sprintf(szBuff, "bind(sock) failed port %d Error %d\n", port, WSAGetLastError());
			WritetoConsoleLocal(szBuff);
		    closesocket( sock );
			return FALSE;
		}

		if (listen( sock, MAX_PENDING_CONNECTS ) < 0)
		{
			sprintf(szBuff, "listen(sock) failed port %d Error %d\n", port, WSAGetLastError());
			WritetoConsoleLocal(szBuff);
			return FALSE;
		}
		ioctl(sock, FIONBIO, &param);
	}

	return sock;
}

BOOL OpenSockets(struct TNCINFO * TNC)
{
	struct sockaddr_in  local_sin;  /* Local socket - internet style */
	struct sockaddr_in * psin;
	u_long param=1;
	struct TCPINFO * TCP = TNC->TCPInfo;
	int n;

	if (!TCP->IPV4)
		return TRUE;

	psin=&local_sin;
	psin->sin_family = AF_INET;
	psin->sin_addr.s_addr = INADDR_ANY;

	if (TCP->TCPPort)
		TCP->TCPSock = OpenSocket4(TNC, TCP->TCPPort);

	n = 0;

	while (TCP->FBBPort[n])
	{
		TCP->FBBsock[n] = OpenSocket4(TNC, TCP->FBBPort[n]);
		n++;
	}

	if (TCP->RelayPort)
		TCP->Relaysock = OpenSocket4(TNC, TCP->RelayPort);

	if (TCP->TriModePort)
	{
		TCP->TriModeSock = OpenSocket4(TNC, TCP->TriModePort);
		TCP->TriModeDataSock = OpenSocket4(TNC, TCP->TriModePort + 1);
	}

	if (TCP->HTTPPort)
		TCP->HTTPsock = OpenSocket4(TNC, TCP->HTTPPort);

	CMSUser.UserName = _strdup("CMS");

	TriModeUser.Secure = TRUE;
	TriModeUser.UserName = _strdup("TRIMODE");
	TriModeUser.Callsign = zalloc(10);

	return TRUE;
}

BOOL OpenSocket6(struct TNCINFO * TNC, int port)
{
	struct sockaddr_in6 local_sin;  /* Local socket - internet style */
	struct sockaddr_in6 * psin;
	SOCKET sock;
	char szBuff[80];
	u_long param=1;

	memset(&local_sin, 0, sizeof(local_sin));

	psin=&local_sin;
	psin->sin6_family = AF_INET6;
	psin->sin6_addr = in6addr_any;
	psin->sin6_flowinfo = 0;
	psin->sin6_scope_id = 0;

	sock = socket(AF_INET6, SOCK_STREAM, 0);
	
#ifndef WIN32

	if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &param, sizeof(param)) < 0)
	{
      perror("setting option IPV6_V6ONLY");
    }

#endif

	if (sock == INVALID_SOCKET)
	{
		sprintf(szBuff, "IPV6 socket() failed error %d\n", WSAGetLastError());
		WritetoConsoleLocal(szBuff);
		return FALSE;  
	}

	psin->sin6_port = htons(port);        // Convert to network ordering 

	if (bind(sock, (struct sockaddr FAR *)psin, sizeof(local_sin)) == SOCKET_ERROR)
	{
		sprintf(szBuff, "IPV6 bind(sock) failed Error %d\n", WSAGetLastError());
		WritetoConsoleLocal(szBuff);
	    closesocket( sock );

		return FALSE;
	}

	if (listen( sock, MAX_PENDING_CONNECTS ) < 0)
	{
		sprintf(szBuff, "IPV6 listen(sock) failed Error %d\n", WSAGetLastError());
		WritetoConsoleLocal(szBuff);

		return FALSE;
	}

	ioctl(sock, FIONBIO, &param);
	return sock;
}


BOOL OpenSockets6(struct TNCINFO * TNC)
{
	struct sockaddr_in6 local_sin;  /* Local socket - internet style */

	struct sockaddr_in6 * psin;
	struct TCPINFO * TCP = TNC->TCPInfo;
	int n;
	u_long param=1;

	if (!TCP->IPV6)
		return TRUE;

	memset(&local_sin, 0, sizeof(local_sin));

	psin=&local_sin;
	psin->sin6_family = AF_INET6;
	psin->sin6_addr = in6addr_any;
	psin->sin6_flowinfo = 0;
	psin->sin6_scope_id = 0;


	if (TCP->TCPPort)
		TCP->sock6 = OpenSocket6(TNC, TCP->TCPPort);

	n = 0;

	while (TCP->FBBPort[n])
	{
		TCP->FBBsock6[n] = OpenSocket6(TNC, TCP->FBBPort[n]);
		n++;
	}

	if (TCP->RelayPort)
		TCP->Relaysock6 = OpenSocket6(TNC, TCP->RelayPort);


	if (TCP->HTTPPort)
		TCP->HTTPsock6 = OpenSocket6(TNC, TCP->HTTPPort);

	CMSUser.UserName = _strdup("CMS");

	return TRUE;
}

static VOID SetupListenSet(struct TNCINFO * TNC)
{
	// Set up master set of fd's for checking for incoming calls

	struct TCPINFO * TCP = TNC->TCPInfo;
	SOCKET maxsock = 0;
	int n;
	fd_set * readfd = &TCP->ListenSet;
	SOCKET sock;

	FD_ZERO(readfd);

	n = 0;
	while (n < 100)
	{
		sock = TCP->FBBsock[n++];
		if (sock)
		{
			FD_SET(sock, readfd);
			if (sock > maxsock)
				maxsock = sock;
		}
	}

	sock = TCP->TCPSock;
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > maxsock)
			maxsock = sock;
	}
		
	sock = TCP->Relaysock;
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > maxsock)
			maxsock = sock;
	}
		
	sock = TCP->HTTPsock;
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > maxsock)
			maxsock = sock;
	}
		
	sock = TCP->TriModeSock;
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > maxsock)
			maxsock = sock;
	}

	sock = TCP->TriModeDataSock;
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > maxsock)
			maxsock = sock;
	}
	n = 0;
	while (n < 100)
	{
		sock = TCP->FBBsock6[n++];
		if (sock)
		{
			FD_SET(sock, readfd);
			if (sock > maxsock)
				maxsock = sock;
		}
	}

	sock = TCP->sock6;
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > maxsock)
			maxsock = sock;
	}
		
	sock = TCP->Relaysock6;
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > maxsock)
			maxsock = sock;
	}
		
	sock = TCP->HTTPsock6;
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > maxsock)
			maxsock = sock;
	}

	TCP->maxsock = maxsock;
}


VOID TelnetPoll(int Port)
{
	struct TNCINFO * TNC = TNCInfo[Port];
	UCHAR * Poll = TNC->TXBuffer;
	struct TCPINFO * TCP = TNC->TCPInfo;
	struct STREAMINFO * STREAM;
	int Msglen;
	int Stream;

	//	we now poll for incoming connections and data

	{
		fd_set readfd, writefd, exceptfd;
		struct timeval timeout;
		int retval;
		int n;
		struct ConnectionInfo * sockptr;
		SOCKET sock;
		int Active = 0;
		SOCKET maxsock;

		timeout.tv_sec = 0;
		timeout.tv_usec = 0;				// poll

		if (TCP->maxsock == 0)
			goto nosocks;
		
		memcpy(&readfd, &TCP->ListenSet, sizeof(fd_set));

		retval = select(TCP->maxsock + 1, &readfd, NULL, NULL, &timeout);

		if (retval == -1)
		{
			retval = WSAGetLastError();
			perror("listen select");
		}

		if (retval)
		{
		
		n = 0;
		while (TCP->FBBsock[n])
		{
			sock = TCP->FBBsock[n++];
			if (FD_ISSET(sock, &readfd))
				Socket_Accept(TNC, sock);
		}

		sock = TCP->TCPSock;
		if (sock)
		{
			if (FD_ISSET(sock, &readfd))
				Socket_Accept(TNC, sock);
		}
		
		sock = TCP->TriModeSock;
		if (sock)
		{
			if (FD_ISSET(sock, &readfd))
				Socket_Accept(TNC, sock);
		}
		sock = TCP->TriModeDataSock;
		if (sock)
		{
			if (FD_ISSET(sock, &readfd))
				Socket_Accept(TNC, sock);
		}
	
		sock = TCP->Relaysock;
		if (sock)
		{
			if (FD_ISSET(sock, &readfd))
				Socket_Accept(TNC, sock);
		}
		
		sock = TCP->HTTPsock;
		if (sock)
		{
			if (FD_ISSET(sock, &readfd))
				Socket_Accept(TNC, sock);
		}

		n = 0;
		
		while (TCP->FBBsock6[n])
		{
			sock = TCP->FBBsock6[n++];
			if (FD_ISSET(sock, &readfd))
			Socket_Accept(TNC, sock);
		}

		sock = TCP->sock6;
		if (sock)
		{
			if (FD_ISSET(sock, &readfd))
			Socket_Accept(TNC, sock);
		}
		
		sock = TCP->Relaysock6;
		if (sock)
		{
			if (FD_ISSET(sock, &readfd))
			Socket_Accept(TNC, sock);
		}
		
		sock = TCP->HTTPsock6;
		if (sock)
		{
			if (FD_ISSET(sock, &readfd))
			Socket_Accept(TNC, sock);
		}
		
		}

		// look for data on any active sockets

		maxsock = 0;

		FD_ZERO(&readfd);
		FD_ZERO(&writefd);
		FD_ZERO(&exceptfd);

		for (n = 0; n <= TCP->MaxSessions; n++)
		{
			sockptr = TNC->Streams[n].ConnectionInfo;
		
			//	Should we use write event after a blocked write ????

			if (sockptr->SocketActive)
			{
				if (TNC->Streams[n].Connecting)
				{
					// look for complete or failed

					FD_SET(sockptr->socket, &writefd);
					FD_SET(sockptr->socket, &exceptfd);
				}
				else
				{
					FD_SET(sockptr->socket, &readfd);
					FD_SET(sockptr->socket, &exceptfd);
				}
				Active++;
				if (sockptr->socket > maxsock)
					maxsock = sockptr->socket;

				if (sockptr->TriModeDataSock)
				{
					FD_SET(sockptr->TriModeDataSock, &readfd);
					FD_SET(sockptr->TriModeDataSock, &exceptfd);
			
					if (sockptr->TriModeDataSock > maxsock)
						maxsock = sockptr->TriModeDataSock;
				}
			}
		}

		if (Active)
		{
			retval = select(maxsock + 1, &readfd, &writefd, &exceptfd, &timeout);

			if (retval == -1)
			{				
				perror("data select");
				Debugprintf("Select Error %d", WSAGetLastError());

				// 
			}
			else
			{
				if (retval)
				{
					// see who has data

					for (n = 0; n <= TCP->MaxSessions; n++)
					{
						sockptr = TNC->Streams[n].ConnectionInfo;
		
						if (sockptr->SocketActive)
						{
							sock = sockptr->socket;

							if (sockptr->TriModeDataSock)
							{
								if (FD_ISSET(sockptr->TriModeDataSock, &readfd))
								{
									ProcessTriModeDataMessage(TNC, sockptr, sockptr->TriModeDataSock, &TNC->Streams[n]);
								}
							}

							if (FD_ISSET(sock, &readfd))
							{
								if (sockptr->RelayMode)
									DataSocket_ReadRelay(TNC, sockptr, sock, &TNC->Streams[n]);
								else if (sockptr->HTTPMode)
									DataSocket_ReadHTTP(TNC, sockptr, sock, n);
								else if (sockptr->FBBMode)
									DataSocket_ReadFBB(TNC, sockptr, sock, n);
								else
									DataSocket_Read(TNC, sockptr, sock, &TNC->Streams[n]);
							}

							if (FD_ISSET(sock, &exceptfd))
							{
								Debugprintf("exceptfd set");
								Telnet_Connected(TNC, sockptr, sock, 1);
							}

							if (FD_ISSET(sock, &writefd))
								Telnet_Connected(TNC, sockptr, sock, 0);

						}
					}
				}
			}
		}
	}

nosocks:

	while (TELNETMONVECPTR->HOSTTRACEQ)
	{
		int stamp, len;
		BOOL MonitorNODES = FALSE;
		MESSAGE * monbuff;
		UCHAR * monchars;

		unsigned char buffer[1024] = "\xff\x1b\xb";

		monbuff = Q_REM((UINT *)&TELNETMONVECPTR->HOSTTRACEQ);
		monchars = (UCHAR *)monbuff;

		stamp = monbuff->Timestamp;

		if (monbuff->PORT & 0x80)		// TX
			buffer[2] = 91;
		else
			buffer[2] = 17;
	
		for (Stream = 0; Stream <= TCP->MaxSessions; Stream++)
		{
			STREAM = &TNC->Streams[Stream];

			if (TNC->PortRecord->ATTACHEDSESSIONS[Stream])
			{
				struct ConnectionInfo * sockptr = STREAM->ConnectionInfo;

				if (sockptr->BPQTermMode)
				{
					if (!sockptr->MonitorNODES && monchars[21] == 3 && monchars[22] == 0xcf && monchars[23] == 0xff)
					{
						len = 0;
					}
					else
					{
						ULONG SaveMMASK = MMASK;
						BOOL SaveMTX = MTX;
						BOOL SaveMCOM = MCOM;
						BOOL SaveMUI = MUIONLY;

						SetTraceOptionsEx(sockptr->MMASK, sockptr->MTX, sockptr->MCOM, sockptr->MUIOnly);
						len = IntDecodeFrame((MESSAGE *)monbuff, &buffer[3], stamp, sockptr->MMASK, FALSE, FALSE);
						SetTraceOptionsEx(SaveMMASK, SaveMTX, SaveMCOM, SaveMUI);

						if (len)
						{
							len += 3;
							buffer[len++] = 0xfe;
							send(STREAM->ConnectionInfo->socket, buffer, len, 0);
						}
					}
				}
			}
		}

		ReleaseBuffer(monbuff);
	}
	
	for (Stream = 0; Stream <= TCP->MaxSessions; Stream++)
	{
		STREAM = &TNC->Streams[Stream];

		if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] && STREAM->Attached == 0)
		{
			// New Attach

			int calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4USER, TNC->Streams[Stream].MyCall);
			TNC->Streams[Stream].MyCall[calllen] = 0;

			STREAM->Attached = TRUE;
			STREAM->FramesQueued= 0;

			continue;
		}

		if (STREAM->Attached)
		{
			if (TNC->PortRecord->ATTACHEDSESSIONS[Stream] == 0 && STREAM->Attached)
		{
			// Node has disconnected - clear any connection

			struct ConnectionInfo * sockptr = TNC->Streams[Stream].ConnectionInfo;
			SOCKET sock = sockptr->socket;
			char Msg[80];	
			UINT * buffptr;

			STREAM->Attached = FALSE;
			STREAM->Connected = FALSE;

			sockptr->FromHostBuffPutptr = sockptr->FromHostBuffGetptr = 0;	// clear any queued data

			while(TNC->Streams[Stream].BPQtoPACTOR_Q)
			{
				buffptr=Q_REM(&TNC->Streams[Stream].BPQtoPACTOR_Q);
				if (TelSendPacket(Stream, &TNC->Streams[Stream], buffptr) == FALSE)
				{
					//	Send failed, and has saved packet
					//	free saved and discard any more on queue

					free(sockptr->ResendBuffer);
					sockptr->ResendBuffer = NULL;
					sockptr->ResendLen = 0;

					while(TNC->Streams[Stream].BPQtoPACTOR_Q)
					{
						buffptr=Q_REM(&TNC->Streams[Stream].BPQtoPACTOR_Q);
						ReleaseBuffer(buffptr);
					}
					break;
				}
			}

			while(TNC->Streams[Stream].PACTORtoBPQ_Q)
			{
				buffptr=Q_REM(&TNC->Streams[Stream].PACTORtoBPQ_Q);
				ReleaseBuffer(buffptr);
			}

            if (!sockptr->FBBMode)
			{
				sprintf(Msg,"*** Disconnected from Stream %d\r\n",Stream);
				send(sock, Msg, strlen(Msg),0);
			}

			if (sockptr->UserPointer == &TriModeUser)
			{
				// Always Disconnect

				send(sockptr->socket, "DISCONNECTED\r\n", 14, 0);
				return;
			}

			if (sockptr->UserPointer == &CMSUser)
			{
				if (CMSLogEnabled)
				{
					char logmsg[120];
					sprintf(logmsg,"%d Disconnected. Bytes Sent = %d Bytes Received %d Time %d Seconds\r\n",
						sockptr->Number, STREAM->BytesTXed, STREAM->BytesRXed, (int)(time(NULL) - sockptr->ConnectTime));

					WriteCMSLog (logmsg);
				}
				// Always Disconnect CMS Socket

				DataSocket_Disconnect(TNC, sockptr);	
			}

			if (sockptr->RelayMode)
			{
				if (LogEnabled)
				{
					char logmsg[120];
					sprintf(logmsg,"%d Disconnected. Bytes Sent = %d Bytes Received %d\n",
						sockptr->Number, STREAM->BytesTXed, STREAM->BytesRXed);

					WriteLog (logmsg);
				}

				// Always Disconnect Relay Socket

				Sleep(100);
				DataSocket_Disconnect(TNC, sockptr);	
			}

			else
			{
				if (sockptr->Signon[0] || sockptr->ClientSession)		// Outward Connect
				{
					Sleep(1000);
					DataSocket_Disconnect(TNC, sockptr);
					return;
				}

				if (LogEnabled)
				{
					char logmsg[120];
					sprintf(logmsg,"%d Disconnected. Bytes Sent = %d Bytes Received %d\n",
						sockptr->Number, STREAM->BytesTXed, STREAM->BytesRXed);

					WriteLog (logmsg);
				}

				if (TCP->DisconnectOnClose)
				{
					Sleep(1000);
					DataSocket_Disconnect(TNC, sockptr);
				}
				else
				{
					char DisfromNodeMsg[] = "Disconnected from Node - Telnet Session kept\r\n";
					send(sockptr->socket, DisfromNodeMsg, strlen(DisfromNodeMsg),0);
				}
			}
		}
	}
	}

	for (Stream = 0; Stream <= TCP->MaxSessions; Stream++)
	{
		struct ConnectionInfo * sockptr = TNC->Streams[Stream].ConnectionInfo;
		STREAM = &TNC->Streams[Stream];

		if (sockptr->SocketActive && sockptr->Keepalive && L4LIMIT)
		{
#ifdef WIN32
			if ((REALTIMETICKS - sockptr->LastSendTime) > (L4LIMIT - 60) * 9)	// PC Ticks are about 10% slow
#else
			if ((REALTIMETICKS - sockptr->LastSendTime) > (L4LIMIT - 60) * 10)
#endif
			{
				// Send Keepalive

				sockptr->LastSendTime = REALTIMETICKS;
				BuffertoNode(sockptr, "Keepalive\r", 10);
			}
		}
			
		if (sockptr->ResendBuffer)
		{
			// Data saved after EWOULDBLOCK returned to send

			UCHAR * ptr = sockptr->ResendBuffer;
			sockptr->ResendBuffer = NULL;

			SendAndCheck(sockptr, ptr, sockptr->ResendLen, 0);
			free(ptr);

			continue;
		}
			
		while (STREAM->BPQtoPACTOR_Q)
		{
			int datalen;
			UINT * buffptr;
			UCHAR * MsgPtr;

			// Make sure there is space. Linux TCP buffer is quite small
			// Windows doesn't support SIOCOUTQ

#ifndef WIN32
			int value = 0, error;

			error = ioctl(sockptr->socket, SIOCOUTQ, &value);

			if (value > 1500)
				break;
#endif
			buffptr=Q_REM(&TNC->Streams[Stream].BPQtoPACTOR_Q);
			STREAM->FramesQueued--;
			datalen=buffptr[1];
			MsgPtr = (UCHAR *)&buffptr[2];

			if (STREAM->ConnectionInfo->TriMode)
			{
				ProcessTrimodeResponse(TNC, STREAM, MsgPtr, datalen);
				ReleaseBuffer(buffptr);
				return;
			}


			if (TNC->Streams[Stream].Connected)
			{
				if (TelSendPacket(Stream, STREAM, buffptr) == FALSE)
				{
					//	Send failed, and has requeued packet
					//	Dont send any more

					break;
				}
			}
			else // Not Connected
			{
				// Command. Do some sanity checking and look for things to process locally

				datalen--;				// Exclude CR
				MsgPtr[datalen] = 0;	// Null Terminate

				if (_stricmp(MsgPtr, "NoFallback") == 0)
				{
					TNC->Streams[Stream].NoCMSFallback = TRUE;
					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "Ok\r");
					C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
					return;
				}

				if (_memicmp(MsgPtr, "D", 1) == 0)
				{
					TNC->Streams[Stream].ReportDISC = TRUE;		// Tell Node
					ReleaseBuffer(buffptr);
					return;
				}
				
				if ((_memicmp(MsgPtr, "C", 1) == 0) && MsgPtr[1] == ' ' && datalen > 2)	// Connect
				{
					char Host[100] = "";
					char P2[100] = "";
					char P3[100] = "";
					char P4[100] = "";
					char P5[100] = "";
					char P6[100] = "";
					char P7[100] = "";
					unsigned int Port = 0;
					int n;

					n = sscanf(&MsgPtr[2], "%s %s %s %s %s %s %s",
							&Host[0], &P2[0], &P3[0], &P4[0], &P5[0], &P6[0], &P7[0]);
	
					sockptr->Signon[0] = 0;		// Not outgoing;
					sockptr->Keepalive = FALSE;	// No Keepalives
					sockptr->UTF8 = 0;			// Not UTF8

					if (_stricmp(Host, "HOST") == 0)
					{
						Port = atoi(P3);

						if (Port > 33 || TCP->CMDPort[Port] == 0)
						{
							buffptr[1] = sprintf((UCHAR *)&buffptr[2], "Error - Invalid HOST Port\r");
							C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
							STREAM->NeedDisc = 10;
							return;
						}

						STREAM->Connecting = TRUE;
						sockptr->CMSSession = FALSE;
						sockptr->FBBMode = FALSE;
						if (P4[0] == 'K' || P5[0] == 'K')
						{
							sockptr->Keepalive = TRUE;
							sockptr->LastSendTime = REALTIMETICKS;
						}
						TCPConnect(TNC, TCP, STREAM, "127.0.0.1", TCP->CMDPort[Port], FALSE);
						ReleaseBuffer(buffptr);
						return;
					}

					if (_stricmp(Host, "RELAY") == 0 && TCP->RELAYHOST[0])
					{
						STREAM->Connecting = TRUE;
						STREAM->ConnectionInfo->CMSSession = TRUE;
						STREAM->ConnectionInfo->RelaySession = TRUE;
						TCPConnect(TNC, TCP, STREAM, TCP->RELAYHOST, 8772, TRUE);
						ReleaseBuffer(buffptr);
						return;
					}
							
					if (_stricmp(Host, "CMS") == 0)
					{
						if (!TCP->CMSOK)
						{
							if (TCP->RELAYHOST[0] && TCP->FallbacktoRelay && STREAM->NoCMSFallback == 0)
							{
								STREAM->Connecting = TRUE;
								STREAM->ConnectionInfo->CMSSession = TRUE;
								STREAM->ConnectionInfo->RelaySession = TRUE;
								TCPConnect(TNC, TCP, STREAM, TCP->RELAYHOST, 8772, TRUE);
								ReleaseBuffer(buffptr);
								return;
							}
					
							buffptr[1] = sprintf((UCHAR *)&buffptr[2], "Error - CMS Not Available\r");
							C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
							STREAM->NeedDisc = 10;
							CheckCMS(TNC);

							return;
						}

						STREAM->Connecting = TRUE;
						STREAM->ConnectionInfo->CMSSession = TRUE;
						CMSConnect(TNC, TCP, STREAM, Stream);
						ReleaseBuffer(buffptr);

						return;
					}

					// Outward Connect. 
						
					Port = atoi(P2);

					if (Port)
					{
						STREAM->Connecting = TRUE;
						STREAM->ConnectionInfo->CMSSession = FALSE;
						STREAM->ConnectionInfo->RelaySession = FALSE;

						if (_stricmp(P3, "TELNET") == 0)
						{
//							// FBB with CRLF
		
							STREAM->ConnectionInfo->NeedLF = TRUE;
						}
						else
							STREAM->ConnectionInfo->NeedLF = FALSE;


						STREAM->ConnectionInfo->FBBMode = TRUE;

						if (_stricmp(P3, "NEEDLF") == 0 || STREAM->ConnectionInfo->NeedLF)
						{
							// Send LF after each param
						
							if (P6[0])
								sprintf(STREAM->ConnectionInfo->Signon, "%s\r\n%s\r\n%s\r\n", P4, P5, P6);
							else
							if (P5[0])
								sprintf(STREAM->ConnectionInfo->Signon, "%s\r\n%s\r\n", P4, P5);
							else
							if (P4[0])
								sprintf(STREAM->ConnectionInfo->Signon, "%s\r\n", P4);
						}
						else
						{
							if (P5[0])
								sprintf(STREAM->ConnectionInfo->Signon, "%s\r%s\r%s\r", P3, P4, P5);
							else
							if (P4[0])
								sprintf(STREAM->ConnectionInfo->Signon, "%s\r%s\r", P3, P4);
							else
								sprintf(STREAM->ConnectionInfo->Signon, "%s\r", P3);
						}

						TCPConnect(TNC, TCP, STREAM, Host, Port, TRUE);
						ReleaseBuffer(buffptr);
						return;
					}
				}
	
				buffptr[1] = sprintf((UCHAR *)&buffptr[2], "Error - Invalid Command\r");
				C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
				STREAM->NeedDisc = 10;
				return;
			}
		}

		Msglen = sockptr->FromHostBuffPutptr - sockptr->FromHostBuffGetptr;

		if (Msglen)
		{
			int Paclen = 0;
			int Queued = 0;
			TRANSPORTENTRY * Sess1 = TNC->PortRecord->ATTACHEDSESSIONS[Stream];
			TRANSPORTENTRY * Sess2 = NULL;
			
			if (Sess1)
				Sess2 = Sess1->L4CROSSLINK;
			
			// Can't use TXCount - it is Semaphored=

			Queued = C_Q_COUNT(&TNC->Streams[Stream].PACTORtoBPQ_Q);
			Queued += C_Q_COUNT((UINT *)&TNC->PortRecord->PORTCONTROL.PORTRX_Q);

			if (Sess2)
				Queued += CountFramesQueuedOnSession(Sess2);

			if (Sess1)
				Queued += CountFramesQueuedOnSession(Sess1);

			if (Queued > 15)
				continue;

			if (Sess1)
				Paclen = Sess1->SESSPACLEN;

			if (Paclen == 0)
				Paclen = 256;

			ShowConnections(TNC);

			if (Msglen > Paclen)
				Msglen = Paclen;

			if (Sess1) Sess1->L4KILLTIMER = 0;
			if (Sess2) Sess2->L4KILLTIMER = 0;

			SendtoNode(TNC, Stream, &sockptr->FromHostBuffer[sockptr->FromHostBuffGetptr], Msglen);
			sockptr->FromHostBuffGetptr += Msglen;
			sockptr->LastSendTime = REALTIMETICKS;
		}
	}
}

#ifndef LINBPQ

LRESULT CALLBACK TelWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	struct ConnectionInfo * sockptr;
	SOCKET sock;
	int i, n;
	struct TNCINFO * TNC;
	struct TCPINFO * TCP;
	struct _EXTPORTDATA * PortRecord;
	HWND SavehDlg, SaveCMS, SaveMonitor;
	HMENU SaveMenu1, SaveMenu2, SaveMenu3; 
	MINMAXINFO * mmi;

    LPMEASUREITEMSTRUCT lpmis;  // pointer to item of data             
	LPDRAWITEMSTRUCT lpdis;     // pointer to item drawing data

//	struct ConnectionInfo * ConnectionInfo;

	for (i=1; i<33; i++)
	{
		TNC = TNCInfo[i];
		if (TNC == NULL)
			continue;
		
		if (TNC->hDlg == hWnd)
			break;
	}

	if (TNC == NULL)
			return DefMDIChildProc(hWnd, message, wParam, lParam);

	switch (message)
	{
//	case WM_SIZING:
	case WM_SIZE:
	{
		RECT rcClient;
		int ClientHeight, ClientWidth;

		GetClientRect(TNC->hDlg, &rcClient); 

		ClientHeight = rcClient.bottom;
		ClientWidth = rcClient.right;

		MoveWindow(TNC->hMonitor, 0,20 ,ClientWidth-4, ClientHeight-25, TRUE);
		break;
	}

	case WM_GETMINMAXINFO:

 		mmi = (MINMAXINFO *)lParam;
		mmi->ptMaxSize.x = 400;
		mmi->ptMaxSize.y = 500;
		mmi->ptMaxTrackSize.x = 400;
		mmi->ptMaxTrackSize.y = 500;
		break;


	case WM_MDIACTIVATE:
	{			 
		// Set the system info menu when getting activated
			 
		if (lParam == (LPARAM) hWnd)
		{
			// Activate

			RemoveMenu(hBaseMenu, 1, MF_BYPOSITION);
			AppendMenu(hBaseMenu, MF_STRING + MF_POPUP, (UINT)hPopMenu, "Actions");
			SendMessage(ClientWnd, WM_MDISETMENU, (WPARAM) hBaseMenu, (LPARAM) hWndMenu);

		}
		else
		{
			 // Deactivate
	
			SendMessage(ClientWnd, WM_MDISETMENU, (WPARAM) hMainFrameMenu, (LPARAM) NULL);
		 }
			 
		// call DrawMenuBar after the menu items are set
		DrawMenuBar(FrameWnd);

		return TRUE; //DefMDIChildProc(hWnd, message, wParam, lParam);

	}

	case WM_SYSCOMMAND:

		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

		switch (wmId)
		{ 
		case SC_RESTORE:

			TNC->Minimized = FALSE;
			SendMessage(ClientWnd, WM_MDIRESTORE, (WPARAM)hWnd, 0);
			return DefMDIChildProc(hWnd, message, wParam, lParam);

		case  SC_MINIMIZE: 

			TNC->Minimized = TRUE;
			return DefMDIChildProc(hWnd, message, wParam, lParam);
		}
		
		return DefMDIChildProc(hWnd, message, wParam, lParam);

	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:

		if (wmId > IDM_DISCONNECT && wmId < IDM_DISCONNECT+MaxSockets+1)
		{
			// disconnect user

			sockptr = TNC->Streams[wmId-IDM_DISCONNECT].ConnectionInfo;
		
			if (sockptr->SocketActive)
			{
				sock=sockptr->socket;

				send(sock,disMsg, strlen(disMsg),0);

				Sleep (1000);
    
				shutdown(sock,2);

				DataSocket_Disconnect(TNC, sockptr);

				TNC->Streams[wmId-IDM_DISCONNECT].ReportDISC = TRUE;		//Tell Node

				return 0;
			}
		}

		switch (wmId)
		{
		case CMSENABLED:

			// Toggle CMS Enabled Flag

			TCP = TNC->TCPInfo;
			
			TCP->CMS = !TCP->CMS;
			CheckMenuItem(TCP->hActionMenu, 3, MF_BYPOSITION | TCP->CMS<<3);

			if (TCP->CMS)
				CheckCMS(TNC);
			else
			{
				TCP->CMSOK = FALSE;
				SetWindowText(TCP->hCMSWnd, ""); 
			}
			break;

		case IDM_LOGGING:

			// Toggle Logging Flag

			LogEnabled = !LogEnabled;
			CheckMenuItem(TNC->TCPInfo->hLogMenu, 0, MF_BYPOSITION | LogEnabled<<3);

			break;

		case IDM_CMS_LOGGING:

			// Toggle Logging Flag

			LogEnabled = !LogEnabled;
			CheckMenuItem(TNC->TCPInfo->hLogMenu, 1, MF_BYPOSITION | CMSLogEnabled<<3);

			break;

		case TELNET_RECONFIG:

			ProcessConfig();
			FreeConfig();

			for (n = 1; n <= TNC->TCPInfo->CurrentSockets; n++)
			{
				sockptr = TNC->Streams[n].ConnectionInfo;
				sockptr->SocketActive = FALSE;
				closesocket(sockptr->socket);
			}

			TCP = TNC->TCPInfo;

			shutdown(TCP->TCPSock, SD_BOTH);
			shutdown(TCP->sock6, SD_BOTH);

			n = 0;
			while (TCP->FBBsock[n])
				shutdown(TCP->FBBsock[n++], SD_BOTH);

			shutdown(TCP->Relaysock, SD_BOTH);
			shutdown(TCP->HTTPsock, SD_BOTH);
			shutdown(TCP->HTTPsock6, SD_BOTH);


			n = 0;
			while (TCP->FBBsock6[n])
				shutdown(TCP->FBBsock[n++], SD_BOTH);

			shutdown(TCP->Relaysock6, SD_BOTH);

			Sleep(500);
	
			closesocket(TCP->TCPSock);
			closesocket(TCP->sock6);

			n = 0;
			while (TCP->FBBsock[n])
				closesocket(TCP->FBBsock[n++]);

			n = 0;
			while (TCP->FBBsock6[n])
				closesocket(TCP->FBBsock6[n++]);

			closesocket(TCP->Relaysock);
			closesocket(TCP->Relaysock6);
			closesocket(TCP->HTTPsock);
			closesocket(TCP->HTTPsock6);

			// Save info from old TNC record
			
			n = TNC->Port;
			PortRecord = TNC->PortRecord;
			SavehDlg = TNC->hDlg;
			SaveCMS = TCP->hCMSWnd;
			SaveMonitor = TNC->hMonitor;
			SaveMenu1 = TCP->hActionMenu;
			SaveMenu2 = TCP->hDisMenu;
			SaveMenu3 = TCP->hLogMenu;

			// Free old TCP Session Stucts

			for (i = 0; i <= TNC->TCPInfo->MaxSessions; i++)
			{
				free(TNC->Streams[i].ConnectionInfo);
			}

			ReadConfigFile(TNC->Port, ProcessLine);

			TNC = TNCInfo[n];
			TNC->Port = n;
			TNC->Hardware = H_TELNET;
			TNC->hDlg = SavehDlg;
			TNC->RIG = &TNC->DummyRig;			// Not using Rig control, so use Dummy

			// Get Menu Handles

			TCP = TNC->TCPInfo;

			TCP->hCMSWnd = SaveCMS;
			TNC->hMonitor = SaveMonitor;
			TCP->hActionMenu = SaveMenu1;
			TCP->hDisMenu = SaveMenu2;
			TCP->hLogMenu = SaveMenu3;

			CheckMenuItem(TCP->hActionMenu, 3, MF_BYPOSITION | TNC->TCPInfo->CMS<<3);
			CheckMenuItem(TCP->hLogMenu, 0, MF_BYPOSITION | LogEnabled<<3);
			CheckMenuItem(TCP->hLogMenu, 1, MF_BYPOSITION | CMSLogEnabled<<3);

			// Malloc TCP Session Stucts

			for (i = 0; i <= TNC->TCPInfo->MaxSessions; i++)
			{			
				TNC->Streams[i].ConnectionInfo = zalloc(sizeof(struct ConnectionInfo));
				TNC->Streams[i].ConnectionInfo->Number = i;

				TCP->CurrentSockets = i;  //Record max used to save searching all entries

				if (i > 0)
					ModifyMenu(TCP->hDisMenu,i - 1 ,MF_BYPOSITION | MF_STRING,IDM_DISCONNECT + 1, ".");
			}

			TNC->PortRecord = PortRecord;

			Sleep(500);
			OpenSockets(TNC);
			OpenSockets6(TNC);
			SetupListenSet(TNC);
			CheckCMS(TNC);
			ShowConnections(TNC);

			break;
		default:
			return DefMDIChildProc(hWnd, message, wParam, lParam);
		}
		break;

 //   case WM_SIZE:

//		if (wParam == SIZE_MINIMIZED)
//		return (0);

	case WM_CTLCOLORDLG:
	
		return (LONG)bgBrush;


	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;

		if (TNC->TCPInfo->hCMSWnd == (HWND)lParam)
		{
			if (TNC->TCPInfo->CMSOK)
				SetTextColor(hdcStatic, RGB(0, 128, 0));
			else
				SetTextColor(hdcStatic, RGB(255, 0, 0));
		}
	
		SetBkMode(hdcStatic, TRANSPARENT);
		return (LONG)bgBrush;
	}
	case WM_MEASUREITEM: 
 
		// Retrieve pointers to the menu item's 
		// MEASUREITEMSTRUCT structure and MYITEM structure. 
 
		lpmis = (LPMEASUREITEMSTRUCT) lParam;  
		lpmis->itemWidth = 300; 

		return TRUE; 

	case WM_DRAWITEM: 
 
            // Get pointers to the menu item's DRAWITEMSTRUCT 
            // structure and MYITEM structure. 
 
            lpdis = (LPDRAWITEMSTRUCT) lParam; 
 
            // If the user has selected the item, use the selected 
            // text and background colors to display the item.

			SetTextColor(lpdis->hDC, RGB(0, 128, 0));

			if (TNC->TCPInfo->CMS)
			{
				if (TNC->TCPInfo->CMSOK)
				  TextOut(lpdis->hDC, 340, lpdis->rcItem.top + 2, "CMS OK", 6);
				else
				{
					SetTextColor(lpdis->hDC, RGB(255, 0, 0));
					TextOut(lpdis->hDC, 340, lpdis->rcItem.top + 2, "NO CMS", 6);
				}
			}
			else
				TextOut(lpdis->hDC, 340, lpdis->rcItem.top + 2, "             ", 13);

            return TRUE; 

	case WM_DESTROY:

		break;
	}
			
	return DefMDIChildProc(hWnd, message, wParam, lParam);

}

#endif

int Socket_Accept(struct TNCINFO * TNC, int SocketId)
{
	int n, addrlen = sizeof(struct sockaddr_in6);
	struct sockaddr_in6 sin6;  

	struct ConnectionInfo * sockptr;
	SOCKET sock;
	char Negotiate[6]={IAC,WILL,suppressgoahead,IAC,WILL,echo};
//	char Negotiate[3]={IAC,WILL,echo};
	struct TCPINFO * TCP = TNC->TCPInfo;
	HMENU hDisMenu = TCP->hDisMenu;
	u_long param=1;

	// if for TriModeData Session, use the TriMode Control connection entry 

	if (SocketId == TCP->TriModeDataSock)
	{
		sockptr = TNC->TCPInfo->TriModeControlSession;
		sock = accept(SocketId, (struct sockaddr *)&sockptr->sin, &addrlen);
		sockptr->TriModeDataSock = sock;
		ioctl(sock, FIONBIO, &param);

		return 0;
	}

//   Find a free Session

	for (n = 1; n <= TCP->MaxSessions; n++)
	{
		sockptr = TNC->Streams[n].ConnectionInfo;
		
		if (sockptr->SocketActive == FALSE)
		{
			sock = accept(SocketId, (struct sockaddr *)&sockptr->sin, &addrlen);

			if (sock == INVALID_SOCKET)
			{
				Debugprintf("BPQ32 Telnet accept() failed Error %d", WSAGetLastError());
				return FALSE;
			}

			ioctl(sock, FIONBIO, &param);

			sockptr->socket = sock;
			sockptr->SocketActive = TRUE;
			sockptr->InputLen = 0;
			sockptr->Number = n;
			sockptr->LoginState = 0;
			sockptr->UserPointer = 0;
			sockptr->DoEcho = FALSE;
			sockptr->BPQTermMode = FALSE;
			sockptr->ConnectTime = time(NULL);
			sockptr->Keepalive = FALSE;
			sockptr->UTF8 = 0;

			TNC->Streams[n].BytesRXed = TNC->Streams[n].BytesTXed = 0;
			TNC->Streams[n].FramesQueued = 0;

			sockptr->HTTPMode = FALSE;	
			sockptr->FBBMode = FALSE;	
			sockptr->RelayMode = FALSE;
			sockptr->ClientSession = FALSE;
			sockptr->NeedLF = FALSE;

			if (SocketId == TCP->HTTPsock || SocketId == TCP->HTTPsock6)
				sockptr->HTTPMode = TRUE;
			else if (SocketId == TCP->Relaysock || SocketId == TCP->Relaysock6)
			{
				sockptr->RelayMode = TRUE;
				sockptr->FBBMode = TRUE;
			}
			else if (SocketId == TCP->TriModeSock)
			{
				sockptr->TriMode = TRUE;
				sockptr->FBBMode = TRUE;
				TNC->TCPInfo->TriModeControlSession = sockptr;
				sockptr->TriModeConnected = FALSE;
				sockptr->TriModeDataSock = 0;
			}
			else if (SocketId != TCP->TCPSock && SocketId != TCP->sock6)				// We can have several listening FBB mode sockets
				sockptr->FBBMode = TRUE;
#ifndef LINBPQ
			ModifyMenu(hDisMenu, n - 1, MF_BYPOSITION | MF_STRING, IDM_DISCONNECT + n, ".");
			DrawMenuBar(TNC->hDlg);	
#endif
			ShowConnections(TNC);

			if (sockptr->HTTPMode)
				return 0;

			if (sockptr->RelayMode)
			{
				send(sock,"\r\rCallsign :\r", 13,0);
			}
			else
			if (sockptr->TriMode)
			{
				// Trimode emulator Control Connection.

				sockptr->UserPointer  = &TriModeUser;


				send(sock,"CMD\r\n", 5,0);
				sockptr->LoginState = 5;
			}
			else
			if (sockptr->FBBMode == FALSE)
			{
				send(sock, Negotiate, 6, 0);
				send(sock, TCP->LoginMsg, strlen(TCP->LoginMsg), 0);
			}

			if (sockptr->FromHostBuffer == 0)
			{
				sockptr->FromHostBuffer = malloc(10000);
				sockptr->FromHostBufferSize = 10000;
			}

			sockptr->FromHostBuffPutptr = sockptr->FromHostBuffGetptr = 0;

			return 0;
		}	
	}

	//	No free sessions. Must accept() then close

	sock = accept(SocketId, (struct sockaddr *)&sin6, &addrlen);

	send(sock,"No Free Sessions\r\n", 18,0);
	Sleep (1000);
	closesocket(sock);

	return 0;
}


int Socket_Data(struct TNCINFO * TNC, int sock, int error, int eventcode)
{
	int n;
	struct ConnectionInfo * sockptr;
	struct TCPINFO * TCP = TNC->TCPInfo;
	HMENU hDisMenu = TCP->hDisMenu;

	//	Find Connection Record

	for (n = 0; n <= TCP->CurrentSockets; n++)
	{
		sockptr = TNC->Streams[n].ConnectionInfo;
	
		if (sockptr->socket == sock && sockptr->SocketActive)
		{
#ifndef LINBPQ
			switch (eventcode)
			{
				case FD_READ:

					if (sockptr->RelayMode)
						return DataSocket_ReadRelay(TNC, sockptr, sock, &TNC->Streams[n]);

					if (sockptr->HTTPMode)
						return DataSocket_ReadHTTP(TNC, sockptr, sock, n);
					
					if (sockptr->FBBMode)
						return DataSocket_ReadFBB(TNC, sockptr, sock, n);
					else
						return DataSocket_Read(TNC, sockptr, sock, &TNC->Streams[n]);

				case FD_WRITE:

					return 0;

				case FD_OOB:

					return 0;

				case FD_ACCEPT:

					return 0;

				case FD_CONNECT:

					return 0;

				case FD_CLOSE:

					TNC->Streams[n].ReportDISC = TRUE;		//Tell Node
					DataSocket_Disconnect(TNC, sockptr);
					return 0;
				}
			return 0;
#endif	
		}

	}

	return 0;
}

#define PACLEN 100

VOID SendtoNode(struct TNCINFO * TNC, int Stream, char * Msg, int MsgLen)
{
	UINT * buffptr = GetBuff();

	if (buffptr == NULL) 
		return;			// No buffers, so ignore

	if (TNC->Streams[Stream].Connected == 0)
	{
		// Connection Closed - Get Another

		struct ConnectionInfo * sockptr = TNC->Streams[Stream].ConnectionInfo;

		if (ProcessIncommingConnect(TNC, sockptr->Callsign, sockptr->Number, FALSE) == FALSE)
		{
			DataSocket_Disconnect(TNC, sockptr);      //' Tidy up
			return;
		}

		if (sockptr->UserPointer)
			TNC->PortRecord->ATTACHEDSESSIONS[sockptr->Number]->Secure_Session = sockptr->UserPointer->Secure;
	}
			
	buffptr[1] = MsgLen;				// Length
	
	memcpy(&buffptr[2], Msg, MsgLen);
	
	C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
}


int DataSocket_Read(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, struct STREAMINFO * STREAM)
{
	int len=0, maxlen, InputLen, MsgLen, i, n,charsAfter;
	char NLMsg[3]={13,10,0};
	byte * IACptr;
	byte * CRPtr;
	byte * LFPtr;
	byte * BSptr;
	byte * MsgPtr;
	BOOL wait;
	char logmsg[120];
	struct UserRec * USER;
	struct TCPINFO * TCP = TNC->TCPInfo;
	int SendIndex = 0;

	ioctl(sock,FIONREAD,&len);

	maxlen = InputBufferLen - sockptr->InputLen;
	
	if (len > maxlen) len=maxlen;

	len = recv(sock, &sockptr->InputBuffer[sockptr->InputLen], len, 0);

	if (len == SOCKET_ERROR || len ==0)
	{
		// Failed or closed - clear connection

		TNC->Streams[sockptr->Number].ReportDISC = TRUE;		//Tell Node
		DataSocket_Disconnect(TNC, sockptr);
		return 0;
	}

	// echo data just read

	if (sockptr->DoEcho && sockptr->LoginState != 1)  // Password
		send(sockptr->socket,&sockptr->InputBuffer[sockptr->InputLen],len,0);

	// look for backspaces in data just read
	
	BSptr = memchr(&sockptr->InputBuffer[sockptr->InputLen], 8, len);
	
	if (BSptr == NULL)
		BSptr = memchr(&sockptr->InputBuffer[sockptr->InputLen], 127, len);

    if (BSptr != 0)
	{
		// single char or BS as last is most likely, and worth treating as a special case

		int n;
		
		charsAfter = len-(BSptr-&sockptr->InputBuffer[sockptr->InputLen])-1;

		if (charsAfter == 0)
		{
			sockptr->InputLen+=(len-1);
			if (sockptr->InputLen > 0) sockptr->InputLen--; //Remove last char
			return 0;
		}

		// more than single char. Copy stuff after bs over char before

		memmove(BSptr-1, BSptr+1, charsAfter);

		n = sockptr->InputLen;
	
		sockptr->InputLen+=(len-2);		// drop bs and char before

		// see if more bs chars
BSCheck:
		BSptr = memchr(&sockptr->InputBuffer[0], 8, sockptr->InputLen);

		if (BSptr == NULL)
			BSptr = memchr(&sockptr->InputBuffer[0], 127, sockptr->InputLen);

		if (BSptr == NULL)
			goto IACLoop;

		n = sockptr->InputLen;
		charsAfter = sockptr->InputLen-(BSptr-&sockptr->InputBuffer[0])-1;

		if (charsAfter == 0)
		{
			sockptr->InputLen--;		// Remove BS
			if (sockptr->InputLen > 0) sockptr->InputLen--; //Remove last char if not at start
			return 0;
		}

		memmove(BSptr-1, BSptr+1, charsAfter);
		sockptr->InputLen--;		// Remove BS
		if (sockptr->InputLen > 0) sockptr->InputLen--; //Remove last char if not at start

		goto BSCheck;					// may be more bs chars

	}
	else
	{
		sockptr->InputLen+=len;
	}

IACLoop:

	IACptr=memchr(sockptr->InputBuffer, IAC, sockptr->InputLen);

	if (IACptr)
	{
		wait = ProcessTelnetCommand(sockptr, IACptr, sockptr->InputLen+IACptr-&sockptr->InputBuffer[0]);

		if (wait) return 0;				// need more chars

		// If ProcessTelnet Command returns FALSE, then it has removed the IAC and its
		// params from the buffer. There may still be more to process.
		
		if (sockptr->InputLen == 0) return 0;	// Nothing Left
	
		goto IACLoop;					// There may be more
	}

	// Extract lines from input stream

	MsgPtr = &sockptr->InputBuffer[0];
	InputLen = sockptr->InputLen;

MsgLoop:

	// if in Client Mode, accept CR, CR Null, CR LF or LF, and replace with CR
	// Also send immediately to client - dont wait for complete lines

	if (sockptr->ClientSession)
	{
		int n = InputLen;
		char * ptr = MsgPtr;
		char * Start = MsgPtr;
		char c;
		int len = 0;
		char NodeLine[300];
		char * optr = NodeLine;

		while (n--)
		{
			c = *(ptr++);

			if (c == 0)
				// Ignore Nulls
				continue;

			len++;
			*(optr++) = c;

			if (c == 13)
			{
				// See if next is lf or null

				if (n)
				{
					// Some Left

					if ((*ptr) == 0 || *(ptr) == 10)
					{
						// skip next
						
						n--;
						ptr++;
					}
				}
			}
			else if (c == 10)
			{
				*(optr - 1) = 13;
			}							
			else
			{
				//  Normal Char

				if (len >= PACLEN)
				{
					BuffertoNode(sockptr, NodeLine, len); 
					optr = NodeLine;
					len = 0;
				}
			}
		}

		// All scanned - send anything outstanding

		if (len)
			BuffertoNode(sockptr, NodeLine, len);

		sockptr->InputLen = 0;
		ShowConnections(TNC);;

		return 0;
	}


	// Server Mode
	
	CRPtr=memchr(MsgPtr, 13, InputLen);

	if (CRPtr)
	{
		// Convert CR Null to CR LF

		LFPtr=memchr(MsgPtr, 0, InputLen);

		if (LFPtr && *(LFPtr - 1) == 13)		// Convert CR NULL to CR LF
		{
			*LFPtr = 10;						// Replace NULL with LF
			send(sockptr->socket, LFPtr, 1, 0);	// And echo it
		}
	}

	// could just have LF??
	
	LFPtr=memchr(MsgPtr, 10, InputLen);

	if (LFPtr == 0)
		if (CRPtr)
		{
			LFPtr = ++CRPtr;
			InputLen++;
		}
		if (LFPtr == 0)
	{
		// Check Paclen

		if (InputLen > PACLEN)
		{
			if (sockptr->LoginState != 2)		// Normal Data
			{
				// Long message received when waiting for user or password - just ignore

				sockptr->InputLen=0;

				return 0;
			}

			// Send to Node
    
			// Line could be up to 500 chars if coming from a program rather than an interative user
			// Limit send to node to 255

			while (InputLen > 255)
			{		
				SendtoNode(TNC, sockptr->Number, MsgPtr, 255);
				sockptr->InputLen -= 255;
				InputLen -= 255;

				memmove(MsgPtr,MsgPtr+255,InputLen);
			}
	           
			SendtoNode(TNC, sockptr->Number, MsgPtr, InputLen);

			sockptr->InputLen = 0;
		
		} // PACLEN

		return 0;	// No CR
	}
	
	// Got a LF

	// Process data up to the cr

	MsgLen=LFPtr-MsgPtr;	// Include the CR but not LF

	switch (sockptr->LoginState)
	{

	case 2:

		// Normal Data State
			
		STREAM->BytesRXed += MsgLen;
		SendIndex = 0;

		// Line could be up to 500 chars if coming from a program rather than an interative user
		// Limit send to node to 255. Should really use PACLEN instead of 255....

		while (MsgLen > 255)
		{		
			SendtoNode(TNC, sockptr->Number, MsgPtr + SendIndex, 255);
			SendIndex += 255;
			MsgLen -= 255;
		}

		SendtoNode(TNC, sockptr->Number, MsgPtr + SendIndex, MsgLen);
		
		MsgLen += SendIndex;

		// If anything left, copy down buffer, and go back

		InputLen=InputLen-MsgLen-1;

		sockptr->InputLen=InputLen;

		if (InputLen > 0)
		{
			memmove(MsgPtr,LFPtr+1,InputLen);

			goto MsgLoop;
		}

		return 0;

	case 0:
		
        //   Check Username
        //

		*(LFPtr-1)=0;				 // remove cr
        
  //      send(sock, NLMsg, 2, 0);

        if (LogEnabled)
		{
			char Addr[100];
			
			Tel_Format_Addr(sockptr, Addr);
			sprintf(logmsg,"%d %s User=%s\n", sockptr->Number, Addr, MsgPtr);
			WriteLog (logmsg);
		}

		for (i = 0; i < TCP->NumberofUsers; i++)
		{
			USER = TCP->UserRecPtr[i];

			if (strcmp(MsgPtr,USER->UserName) == 0)
			{
                sockptr->UserPointer = USER;      //' Save pointer for checking password
                strcpy(sockptr->Callsign, USER->Callsign); //' for *** linked
                
                send(sock, TCP->PasswordMsg, strlen(TCP->PasswordMsg),0);
                
                sockptr->Retries = 0;
                
                sockptr->LoginState = 1;
                sockptr->InputLen = 0;

				n=sockptr->Number;
#ifndef LINBPQ
				ModifyMenu(TCP->hDisMenu, n - 1, MF_BYPOSITION | MF_STRING, IDM_DISCONNECT + n, MsgPtr);
#endif
				ShowConnections(TNC);;

                return 0;
			}
		}
        
        //   Not found
        
        
        if (sockptr->Retries++ == 4)
		{
            send(sock,AttemptsMsg,sizeof(AttemptsMsg),0);
            Sleep (1000);
            DataSocket_Disconnect(TNC, sockptr);       //' Tidy up
		}
		else
		{        
            send(sock, TCP->LoginMsg, strlen(TCP->LoginMsg), 0);
            sockptr->InputLen=0;

		}

		return 0;
       
	case 1:
		   
		*(LFPtr-1)=0;				 // remove cr
        
        send(sock, NLMsg, 2, 0);	// Need to echo NL, as password is not echoed
    
        if (LogEnabled)
		{
			char Addr[100];
			
			Tel_Format_Addr(sockptr, Addr);
			sprintf(logmsg,"%d %s Password=%s\n", sockptr->Number, Addr, MsgPtr);
			WriteLog (logmsg);
		}

		if (strcmp(MsgPtr, sockptr->UserPointer->Password) == 0)
		{
			char * ct = TCP->cfgCTEXT;
			char * Appl;
			int ctlen = strlen(ct);

			if (ProcessIncommingConnect(TNC, sockptr->Callsign, sockptr->Number, FALSE) == FALSE)
			{
				DataSocket_Disconnect(TNC, sockptr);      //' Tidy up
				return 0;
			}

			TNC->PortRecord->ATTACHEDSESSIONS[sockptr->Number]->Secure_Session = sockptr->UserPointer->Secure;

            sockptr->LoginState = 2;
            sockptr->InputLen = 0;
            
            if (ctlen > 0)  send(sock, ct, ctlen, 0);

			STREAM->BytesTXed = ctlen;

            if (LogEnabled)
			{
				char Addr[100];
			
				Tel_Format_Addr(sockptr, Addr);
				sprintf(logmsg,"%d %s Call Accepted Callsign=%s\n", sockptr->Number, Addr, sockptr->Callsign);
				WriteLog (logmsg);
			}

			Appl = sockptr->UserPointer->Appl;
			
			if (Appl[0])
				SendtoNode(TNC, sockptr->Number, Appl, strlen(Appl));

			ShowConnections(TNC);

            return 0;
		}

		// Bad Password
        
        if (sockptr->Retries++ == 4)
		{
			send(sock,AttemptsMsg, strlen(AttemptsMsg),0);
            Sleep (1000);
            DataSocket_Disconnect (TNC, sockptr);      //' Tidy up
		}
		else
		{
			send(sock, TCP->PasswordMsg, strlen(TCP->PasswordMsg), 0);
			sockptr->InputLen=0;
		}

		return 0;

	default:

		return 0;

	}

	return 0;
}

int DataSocket_ReadRelay(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, struct STREAMINFO * STREAM)
{
	int len=0, maxlen, InputLen, MsgLen, n;
	char NLMsg[3]={13,10,0};
	byte * LFPtr;
	byte * MsgPtr;
	char logmsg[120];
	char RelayMsg[] = "No CMS connection available - using local BPQMail\r";
	struct TCPINFO * TCP = TNC->TCPInfo;

	ioctl(sock,FIONREAD,&len);

	maxlen = InputBufferLen - sockptr->InputLen;
	
	if (len > maxlen) len=maxlen;

	len = recv(sock, &sockptr->InputBuffer[sockptr->InputLen], len, 0);


	if (len == SOCKET_ERROR || len ==0)
	{
		// Failed or closed - clear connection

		TNC->Streams[sockptr->Number].ReportDISC = TRUE;		//Tell Node
		DataSocket_Disconnect(TNC, sockptr);
		return 0;
	}

	sockptr->InputLen+=len;

	// Extract lines from input stream

	MsgPtr = &sockptr->InputBuffer[0];
	InputLen = sockptr->InputLen;

	STREAM->BytesRXed += InputLen;

	if (sockptr->LoginState == 2)
	{
		// Data. FBB is binary

		// Send to Node


		// Queue to Node. Data may arrive it large quatities, possibly exceeding node buffer capacity

		STREAM->BytesRXed += InputLen;

		if (sockptr->FromHostBuffPutptr + InputLen > sockptr->FromHostBufferSize)
		{
			sockptr->FromHostBufferSize += 10000;
			sockptr->FromHostBuffer = realloc(sockptr->FromHostBuffer, sockptr->FromHostBufferSize);
		}

		memcpy(&sockptr->FromHostBuffer[sockptr->FromHostBuffPutptr], MsgPtr, InputLen); 

		sockptr->FromHostBuffPutptr += InputLen;
		sockptr->InputLen = 0;

		return 0;
	}
/*	
		if (InputLen > 256)
		{		
			SendtoNode(TNC, sockptr->Number, MsgPtr, 256);
			sockptr->InputLen -= 256;

			InputLen -= 256;

			memmove(MsgPtr,MsgPtr+256,InputLen);

			goto MsgLoop;
		}
			
		SendtoNode(TNC, sockptr->Number, MsgPtr, InputLen);
		sockptr->InputLen = 0;

		return 0;
	}
*/	
	if (InputLen > 256)
	{
		// Long message received when waiting for user or password - just ignore

		sockptr->InputLen=0;

		return 0;
	}

	LFPtr=memchr(MsgPtr, 13, InputLen);
	
	if (LFPtr == 0)
		return 0;							// Waitr for more
	
	// Got a CR

	// Process data up to the cr

	MsgLen=LFPtr-MsgPtr;

	switch (sockptr->LoginState)
	{

	case 0:
		
        //   Check Username
        //

		*(LFPtr)=0;				 // remove cr

		if (*MsgPtr == '.')
			MsgPtr++;
        
        if (LogEnabled)
		{
			unsigned char work[4];
			memcpy(work, &sockptr->sin.sin_addr.s_addr, 4);
			sprintf(logmsg,"%d %d.%d.%d.%d User=%s\n",
					sockptr->Number,
					work[0], work[1], work[2], work[3],
					MsgPtr);

			WriteLog (logmsg);
		}

		strcpy(sockptr->Callsign, MsgPtr);

		// Save callsign  for *** linked
                                            
		send(sock, "Password :\r", 11,0);
                
		sockptr->Retries = 0;
		sockptr->LoginState = 1;
        sockptr->InputLen = 0;

		n=sockptr->Number;
#ifndef LINBPQ
		ModifyMenu(TCP->hDisMenu, n - 1, MF_BYPOSITION | MF_STRING, IDM_DISCONNECT + n, MsgPtr);
#endif
		ShowConnections(TNC);;

        return 0;
	
       
	case 1:
		   
		*(LFPtr)=0;				 // remove cr
            
        if (LogEnabled)
		{
			unsigned char work[4];
			memcpy(work, &sockptr->sin.sin_addr.s_addr, 4);
			sprintf(logmsg,"%d %d.%d.%d.%d Password=%s\n",
					sockptr->Number,
					work[0], work[1], work[2], work[3],
					MsgPtr);

			WriteLog (logmsg);
		}

		sockptr->UserPointer  = &RelayUser;

		if (ProcessIncommingConnect(TNC, sockptr->Callsign, sockptr->Number, FALSE) == 0)
		{
			DataSocket_Disconnect(TNC, sockptr);      //' Tidy up
			return 0;
		}

		send(sock, RelayMsg, strlen(RelayMsg), 0);

		sockptr->LoginState = 2;

		sockptr->InputLen = 0;

		if (LogEnabled)
		{
			unsigned char work[4];
			memcpy(work, &sockptr->sin.sin_addr.s_addr, 4);
			sprintf(logmsg,"%d %d.%d.%d.%d Call Accepted Callsign =%s\n",
					sockptr->Number,
					work[0], work[1], work[2], work[3],
					sockptr->Callsign);

			WriteLog (logmsg);
		}

		ShowConnections(TNC);

		sockptr->InputLen = 0;

		// Connect to the BBS

		SendtoNode(TNC, sockptr->Number, TCP->RelayAPPL, strlen(TCP->RelayAPPL));

		ShowConnections(TNC);;
	
		return 0;
	
	default:

		return 0;

	}

	return 0;
}


int DataSocket_ReadFBB(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, int Stream)
{
	int len=0, maxlen, InputLen, MsgLen, i, n;
	char NLMsg[3]={13,10,0};
	byte * CRPtr;
	byte * MsgPtr;
	char logmsg[1000];
	struct UserRec * USER;
	struct TCPINFO * TCP = TNC->TCPInfo;
	struct STREAMINFO * STREAM = &TNC->Streams[Stream];
	TRANSPORTENTRY * Sess1 = TNC->PortRecord->ATTACHEDSESSIONS[Stream];
	TRANSPORTENTRY * Sess2 = NULL;
	
	ioctl(sock,FIONREAD,&len);

	maxlen = InputBufferLen - sockptr->InputLen;
	
	if (len > maxlen) len = maxlen;

	len = recv(sock, &sockptr->InputBuffer[sockptr->InputLen], len, 0);

	if (len == SOCKET_ERROR || len == 0)
	{
		// Failed or closed - clear connection

		TNC->Streams[sockptr->Number].ReportDISC = TRUE;		//Tell Node
		DataSocket_Disconnect(TNC, sockptr);
		return 0;
	}

	sockptr->InputLen+=len;

	// Extract lines from input stream

	MsgPtr = &sockptr->InputBuffer[0];
	InputLen = sockptr->InputLen;
	MsgPtr[InputLen] = 0;

	if (sockptr->LoginState == 0)
	{
		// Look for FLMSG Header

		if (InputLen > 10 && memcmp(MsgPtr, "... start\n", 10) == 0)
		{
			MsgPtr[9] = 13;				// Convert to CR
			sockptr->LoginState = 2;		// Set Logged in

			SendtoNode(TNC, Stream, "..FLMSG\r", 8);	// Dummy command to command handler

		}
	}

MsgLoop:

	if (sockptr->LoginState == 2)
	{
		// Data. FBB is binary

		int Paclen = 0;

		if (TNC->PortRecord->ATTACHEDSESSIONS[Stream])
			Paclen = TNC->PortRecord->ATTACHEDSESSIONS[Stream]->SESSPACLEN;

//		if (Paclen == 0)
			Paclen = 256;

		if (sockptr->BPQTermMode)
		{
			if (memcmp(MsgPtr, "\\\\\\\\", 4) == 0)
			{
				// Monitor Control

				int P8 = 0;
				
				int n = sscanf(&MsgPtr[4], "%x %x %x %x %x %x %x %x",
					&sockptr->MMASK, &sockptr->MTX, &sockptr->MCOM, &sockptr->MonitorNODES,
					&sockptr->MonitorColour, &sockptr->MUIOnly, &sockptr->UTF8, &P8);

				if (n == 5)
					sockptr->MUIOnly = sockptr->UTF8 = 0;

				if (n == 6)
					sockptr->UTF8 = 0;

				if (P8 == 1)
					SendPortsForMonitor(sock);

				sockptr->InputLen = 0;
				return 0;
			}
		}

		if (sockptr->UserPointer == &CMSUser)
			WritetoTrace(Stream, MsgPtr, InputLen);

		if (InputLen == 8 && memcmp(MsgPtr, ";;;;;;\r\n", 8) == 0)
		{
			//	CMS Keepalive

			sockptr->InputLen = 0;
			return 0;
		}

		// Queue to Node. Data may arrive it large quantities, possibly exceeding node buffer capacity

		STREAM->BytesRXed += InputLen;
		BuffertoNode(sockptr, MsgPtr, InputLen); 
		sockptr->InputLen = 0;

		return 0;
	}
	
	if (InputLen > 256)
	{
		// Long message received when waiting for user or password - just ignore

		sockptr->InputLen=0;

		return 0;
	}
	
	if (MsgPtr[0] == 10)			// LF
	{
		// Remove the LF

		InputLen--;
		sockptr->InputLen--;

		memmove(MsgPtr, MsgPtr+1, InputLen);
	}

	CRPtr = memchr(MsgPtr, 13, InputLen);
	
	if (CRPtr == 0)
		return 0;							// Waitr for more
	
	// Got a CR

	// Process data up to the cr

	MsgLen = CRPtr - MsgPtr;

	if (MsgLen == 0)						// Just CR
	{
		MsgPtr++;							// Skip it
		InputLen--;
		sockptr->InputLen--;
		goto MsgLoop;
	}


	switch (sockptr->LoginState)
	{
	case 5:

		// Trimode Emulator Command

		*CRPtr = 0;

		ProcessTrimodeCommand(TNC, sockptr, MsgPtr);

		MsgLen++;

		InputLen -= MsgLen;
		
		memmove(MsgPtr, MsgPtr+MsgLen, InputLen);
		sockptr->InputLen = InputLen ;
		MsgPtr[InputLen] = 0;


		goto MsgLoop;

	case 3:

		// CMS Signon
		
		strlop(MsgPtr, 13);

		sprintf(logmsg,"%d %s\r\n", Stream, MsgPtr);
		WriteCMSLog (logmsg);

		if (strstr(MsgPtr, "Callsign :")) 
		{
			char Msg[80];
			int Len;

			if (sockptr->LogonSent)
			{
				sockptr->InputLen=0;
				return TRUE;
			}

			sockptr->LogonSent = TRUE;

			if (TCP->SecureCMSPassword[0])
				Len = sprintf(Msg, "%s %s\r", TNC->Streams[sockptr->Number].MyCall, TCP->GatewayCall);			
			else
				Len = sprintf(Msg, "%s\r", TNC->Streams[sockptr->Number].MyCall);			
			
			send(sock, Msg, Len, 0);
			sprintf(logmsg,"%d %s", Stream, Msg);
			WriteCMSLog (logmsg);

			sockptr->InputLen=0;

			return TRUE;
		}
		if (memcmp(MsgPtr, ";SQ: ", 5) == 0)
		{
			// Secure CMS challenge

			char Msg[80];
			int Len;
			int Response = GetCMSHash(&MsgPtr[5], TCP->SecureCMSPassword);
			char RespString[12];
			int Freq = 0;
			int Mode = 0;

			if (Sess1)
			{
				Sess2 = Sess1->L4CROSSLINK;

				if (Sess2)
				{
					// See if L2 session - if so, get info from WL2K report line

					if (Sess2->L4CIRCUITTYPE & L2LINK)
					{
						LINKTABLE * LINK = Sess2->L4TARGET.LINK;
						PORTCONTROLX * PORT = LINK->LINKPORT;
						
						Freq = PORT->WL2KInfo.Freq;
						Mode = PORT->WL2KInfo.mode;
		
					}
					else
					{
						if (Sess2->RMSCall[0])
						{
							Freq = Sess2->Frequency;
							Mode = Sess2->Mode;
						}
					}
				}
			}


			sprintf(RespString, "%010d", Response);

			Len = sprintf(Msg, ";SR: %s %d %d\r", &RespString[2], Freq, Mode);			
			
			send(sock, Msg, Len,0);
			sprintf(logmsg,"%d %s", Stream, Msg);
			WriteCMSLog (logmsg);
			sockptr->InputLen=0;
			sockptr->LoginState = 2;		// Data
			sockptr->LogonSent = FALSE;

			return TRUE;
		}

		if (strstr(MsgPtr, "Password :")) 
		{
			// Send “CMSTelnet” + gateway callsign + frequency + emission type if info is available

			TRANSPORTENTRY * Sess1 = TNC->PortRecord->ATTACHEDSESSIONS[Stream];
			TRANSPORTENTRY * Sess2 = NULL;
			char Passline[80] = "CMSTELNET\r";
			int len = 10;

			if (Sess1)
			{
				Sess2 = Sess1->L4CROSSLINK;

				if (Sess2)
				{
					// See if L2 session - if so, get info from WL2K report line

					if (Sess2->L4CIRCUITTYPE & L2LINK)
					{
						LINKTABLE * LINK = Sess2->L4TARGET.LINK;
						PORTCONTROLX * PORT = LINK->LINKPORT;
						
						if (PORT->WL2KInfo.Freq)
							len = sprintf(Passline, "CMSTELNET %s %d %d\r", PORT->WL2KInfo.RMSCall, PORT->WL2KInfo.Freq, PORT->WL2KInfo.mode);

					}
					else
					{
						if (Sess2->RMSCall[0])
						{
							len = sprintf(Passline, "CMSTELNET %s %d %d\r", Sess2->RMSCall, Sess2->Frequency, Sess2->Mode);
						}
					}
				}
			}
			send(sock, Passline, len, 0);
			sockptr->LoginState = 2;		// Data
			sockptr->InputLen=0;
			sockptr->LogonSent = FALSE;

			if (CMSLogEnabled)
			{
				char logmsg[120];
				sprintf(logmsg,"%d %s\r\n", sockptr->Number, Passline);
				WriteCMSLog (logmsg);
			}

			return TRUE;
		}

		return TRUE;

	case 0:
		
        //   Check Username
        //

		*(CRPtr)=0;				 // remove cr
        
        if (LogEnabled)
		{
			char Addr[100];		
			Tel_Format_Addr(sockptr, Addr);
			sprintf(logmsg,"%d %s User=%s\n", sockptr->Number, Addr, MsgPtr);
			WriteLog (logmsg);
		}
		for (i = 0; i < TCP->NumberofUsers; i++)
		{
			USER = TCP->UserRecPtr[i];

			if (strcmp(MsgPtr,USER->UserName) == 0)
			{
                sockptr->UserPointer = USER;      //' Save pointer for checking password
                strcpy(sockptr->Callsign, USER->Callsign); //' for *** linked
                                
                sockptr->Retries = 0;
                
                sockptr->LoginState = 1;
                sockptr->InputLen = 0;

				n=sockptr->Number;

#ifndef LINBPQ
				ModifyMenu(TCP->hDisMenu, n - 1, MF_BYPOSITION | MF_STRING, IDM_DISCONNECT + n, MsgPtr);
#endif

				ShowConnections(TNC);;

               	InputLen=InputLen-(MsgLen+1);

				sockptr->InputLen=InputLen;
				
				if (InputLen > 0)
				{
					memmove(MsgPtr, CRPtr+1, InputLen);
					goto MsgLoop;
				}
			}
		}

        //   User Not found
        
        if (sockptr->Retries++ == 4)
		{
            send(sock,AttemptsMsg,sizeof(AttemptsMsg),0);
            Sleep (1000);
            DataSocket_Disconnect(TNC, sockptr);       //' Tidy up
		}
		else
		{        
            send(sock, TCP->LoginMsg, strlen(TCP->LoginMsg), 0);
            sockptr->InputLen=0;

		}

		return 0;

	case 1:
		   
		*(CRPtr)=0;				 // remove cr
            
        if (LogEnabled)
		{
			char Addr[100];
			Tel_Format_Addr(sockptr, Addr);
			sprintf(logmsg,"%d %s Password=%s\n", sockptr->Number, Addr, MsgPtr);
			WriteLog (logmsg);
		}
		if (strcmp(MsgPtr, sockptr->UserPointer->Password) == 0)
		{
			char * Appl;

			if (ProcessIncommingConnect(TNC, sockptr->Callsign, sockptr->Number, FALSE) == FALSE)
			{
				DataSocket_Disconnect(TNC, sockptr);      //' Tidy up
				return 0;
			}
		
			TNC->PortRecord->ATTACHEDSESSIONS[sockptr->Number]->Secure_Session = sockptr->UserPointer->Secure;

            sockptr->LoginState = 2;
            
            sockptr->InputLen = 0;
            
            if (LogEnabled)
			{
				char Addr[100];
				Tel_Format_Addr(sockptr, Addr);
				sprintf(logmsg,"%d %s Call Accepted  Callsign =%s\n",
				sockptr->Number, Addr,sockptr->Callsign);

				WriteLog (logmsg);
			}

			ShowConnections(TNC);;
			InputLen=InputLen-(MsgLen+1);

			sockptr->InputLen=InputLen;

			// What is left is the Command to connect to the BBS

			if (InputLen > 1)
			{
				if (*(CRPtr+1) == 10)
				{
					CRPtr++;
					InputLen--;
				}

				memmove(MsgPtr, CRPtr+1, InputLen);

				if (_memicmp(MsgPtr, "BPQTermTCP", 10) == 0)
				{
					send(sock, "Connected to TelnetServer\r", 26, 0);
					sockptr->BPQTermMode = TRUE;
					sockptr->InputLen -= 11;

					if (sockptr->InputLen)
					{
						// Monitor control info may arrive in same packet

						int P8 = 0;
						
						memmove(MsgPtr, &MsgPtr[11], InputLen);
						if (memcmp(MsgPtr, "\\\\\\\\", 4) == 0)
						{
							// Monitor Control

							int n = sscanf(&MsgPtr[4], "%x %x %x %x %x %x %x %x",
								&sockptr->MMASK, &sockptr->MTX, &sockptr->MCOM, &sockptr->MonitorNODES,
								&sockptr->MonitorColour, &sockptr->MUIOnly, &sockptr->UTF8, &P8);

							if (n == 5)
								sockptr->MUIOnly = sockptr->UTF8 = 0;

							if (n == 6)
								sockptr->UTF8 = 0;

							if (P8 == 1)
								SendPortsForMonitor(sock);


							sockptr->InputLen = 0;
							return 0;
						}
					}
					return 0;
				}
				else
				{
					MsgPtr[InputLen] = 13;
					SendtoNode(TNC, sockptr->Number, MsgPtr, InputLen+1);
				}
				sockptr->InputLen = 0;
				return 0;
			}

			Appl = sockptr->UserPointer->Appl;
			
			if (Appl[0])
				SendtoNode(TNC, sockptr->Number, Appl, strlen(Appl));

			return 0;
	

		}
		// Bad Password
        
        if (sockptr->Retries++ == 4)
		{
			send(sock,AttemptsMsg, strlen(AttemptsMsg),0);
            Sleep (1000);
            DataSocket_Disconnect(TNC, sockptr);      //' Tidy up
		}
		else
		{
			send(sock, TCP->PasswordMsg, strlen(TCP->PasswordMsg), 0);
			sockptr->InputLen=0;
		}

		return 0;

	default:

		return 0;
	}
	return 0;
}


int DataSocket_ReadHTTP(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, int Stream)
{
	int len=0, maxlen, InputLen;
	char NLMsg[3]={13,10,0};
	UCHAR * MsgPtr;
	UCHAR * CRLFCRLF;
	UCHAR * LenPtr;
	int BodyLen, ContentLen;
	struct ConnectionInfo * sockcopy;
	
	ioctl(sock,FIONREAD,&len);

	maxlen = InputBufferLen - sockptr->InputLen;
	
	if (len > maxlen) len = maxlen;

	len = recv(sock, &sockptr->InputBuffer[sockptr->InputLen], len, 0);

	if (len == SOCKET_ERROR || len == 0)
	{
		// Failed or closed - clear connection

		TNC->Streams[sockptr->Number].ReportDISC = TRUE;		//Tell Node
		DataSocket_Disconnect(TNC, sockptr);
		return 0;
	}

	// Make sure request is complete - should end crlfcrlf, and if a post have the required input message

	MsgPtr = &sockptr->InputBuffer[0];
	sockptr->InputLen += len;
	InputLen = sockptr->InputLen;

	MsgPtr[InputLen] = 0;

	CRLFCRLF = strstr(MsgPtr, "\r\n\r\n");

	if (CRLFCRLF == 0)
		return 0;

	LenPtr = strstr(MsgPtr, "Content-Length:");

	if (LenPtr)
	{
		ContentLen = atoi(LenPtr + 15);
		BodyLen = InputLen - (CRLFCRLF + 4 - MsgPtr);

		if (BodyLen < ContentLen)
			return 0;
	}

	sockcopy = malloc(sizeof(struct ConnectionInfo));
	sockptr->TNC = TNC;
	sockptr->LastSendTime = REALTIMETICKS;

	memcpy(sockcopy, sockptr, sizeof(struct ConnectionInfo));

	_beginthread((void (*)())ProcessHTTPMessage, 0, (VOID *)sockcopy);

	sockptr->InputLen = 0;
	return 0;
}

int DataSocket_Disconnect(struct TNCINFO * TNC,  struct ConnectionInfo * sockptr)
{
	int n;

	Debugprintf("Disconnect Reported");

	if (sockptr->SocketActive)
	{
		closesocket(sockptr->socket);

		n = sockptr->Number;

#ifndef LINBPQ
		ModifyMenu(TNC->TCPInfo->hDisMenu, n - 1, MF_BYPOSITION | MF_STRING, IDM_DISCONNECT + n, ".");
#endif

		sockptr->SocketActive = FALSE;

		ShowConnections(TNC);;
	}
	return 0;
}

int ShowConnections(struct TNCINFO * TNC)
{
#ifndef LINBPQ
	char msg[80];
	struct ConnectionInfo * sockptr;
	int i,n;

	SendMessage(TNC->hMonitor,LB_RESETCONTENT,0,0);

	for (n = 0; n <= TNC->TCPInfo->CurrentSockets; n++)
	{
		sockptr=TNC->Streams[n].ConnectionInfo;

		if (!sockptr->SocketActive)
		{
			strcpy(msg,"Idle");
		}
		else
		{
			if (sockptr->UserPointer == 0)
			{
				if (sockptr->HTTPMode)
				{
					char Addr[100];
					Tel_Format_Addr(sockptr, Addr);
					sprintf(msg, "HTTP From %s", Addr);
				}
				else
					strcpy(msg,"Logging in");
			}
			else
			{
				i=sprintf(msg,"%-10s %-10s %2d",
					sockptr->UserPointer->UserName, sockptr->Callsign, sockptr->FromHostBuffPutptr - sockptr->FromHostBuffGetptr);
			}
		}
		SendMessage(TNC->hMonitor, LB_ADDSTRING ,0, (LPARAM)msg);
	}
#endif
	return 0;
}
byte * EncodeCall(byte * Call)
{
	static char axcall[10];

	ConvToAX25(Call, axcall);
	return &axcall[0];
}
BOOL ProcessTelnetCommand(struct ConnectionInfo * sockptr, byte * Msg, int Len)
{
	int cmd, TelOption;
	int used;
	char WillSupGA[3]={IAC,WILL,suppressgoahead};
	char WillEcho[3]={IAC,WILL,echo};
	char Wont[3]={IAC,WONT,echo};


	//	Note Msg points to the IAC, which may not be at the start of the receive buffer
	//	Len is number of bytes left in buffer including the IAC

	if (Len < 2) return TRUE;		//' Wait for more

	cmd = Msg[1];
	
	if (cmd == DOx || cmd == DONT || cmd == WILL || cmd == WONT)
		if (Len < 3) return TRUE;		//' wait for option
    
	TelOption = Msg[2];

	switch (cmd)
	{
	case DOx:
            
        switch (TelOption)
		{
		case echo:
			sockptr->DoEcho = TRUE;
 			send(sockptr->socket,WillEcho,3,0);
			break;

		case suppressgoahead:

 			send(sockptr->socket,WillSupGA,3,0);
			break;

		default:

			Wont[2] = TelOption;
			send(sockptr->socket,Wont,3,0);
		}

		used=3;

		break;

	case DONT:
    
 //       Debug.Print "DONT"; TelOption

		switch (TelOption)
		{
		case echo:
			sockptr->DoEcho = FALSE;
			break;
		}

		used=3;
   
		break;

	case WILL:

 //       Debug.Print "WILL"; TelOption
        
        if (TelOption == echo) sockptr->DoEcho = TRUE;
        
		used=3;

		break;

	case WONT:
            
//        Debug.Print "WONT"; TelOption

		used=3;
       
		break;

	default:
    
		used=2;

	}
   
	// remove the processed command from the buffer

	memmove(Msg,&Msg[used],Len-used);

	sockptr->InputLen-=used;

	return FALSE;
}


int WriteLog(char * msg)
{
	FILE *file;
	char timebuf[128];

	time_t ltime;

	UCHAR Value[100];

	if (BPQDirectory[0] == 0)
	{
		strcpy(Value, "logs/BPQTelnetServer.log");
	}
	else
	{
		strcpy(Value, BPQDirectory);
		strcat(Value, "/");
		strcat(Value, "logs/BPQTelnetServer.log");
	}
		
	if ((file = fopen(Value, "a")) == NULL)
		return FALSE;

	time(&ltime);

#ifdef LINBPQ
	{
		struct tm * tmp = localtime(&ltime);
		strftime( timebuf, 128,
			"%d/%m/%Y %H:%M:%S ", tmp );
	}
#else
	{
	    struct tm today;

		_localtime32_s( &today, &ltime);

		strftime( timebuf, 128,
			"%d/%m/%Y %H:%M:%S ", &today);
	}
#endif    
	fputs(timebuf, file);

	fputs(msg, file);

	fclose(file);

	return 0;
}

char LastCMSLog[256];

VOID WriteCMSLog(char * msg)
{
	UCHAR Value[MAX_PATH];
	time_t T;
	struct tm * tm;
	FILE * Handle;
	char LogMsg[256];
	int MsgLen;

	if (CMSLogEnabled == FALSE)
		 return;

	T = time(NULL);
	tm = gmtime(&T);

	if (BPQDirectory[0] == 0)
	{
		strcpy(Value, "logs/CMSAccess");
	}
	else
	{
		strcpy(Value, BPQDirectory);
		strcat(Value, "/");
		strcat(Value, "logs/CMSAccess");
	}

	sprintf(Value, "%s_%04d%02d%02d.log", Value,
				tm->tm_year +1900, tm->tm_mon+1, tm->tm_mday);

	Handle = fopen(Value, "ab");

	if (Handle == NULL)
		return;

	MsgLen = sprintf(LogMsg, "%02d:%02d:%02d %s", tm->tm_hour, tm->tm_min, tm->tm_sec, msg);

	fwrite(LogMsg , 1, MsgLen, Handle);

	fclose(Handle);

#ifndef WIN32

	if (strcmp(Value, LastCMSLog))
	{
		UCHAR SYMLINK[MAX_PATH];

		sprintf(SYMLINK,"%s/CMSAccessLatest.log", BPQDirectory);
		unlink(SYMLINK); 
		strcpy(LastCMSLog, Value);
		symlink(Value, SYMLINK);
	}

#endif
	return;
}






int Telnet_Connected(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, int Error)
{
	struct TCPINFO * TCP = TNC->TCPInfo;
	UINT * buffptr;
	int Stream = sockptr->Number;
	char Signon[80];
	int errlen = 4;

	buffptr = GetBuff();
	if (buffptr == 0) return 0;			// No buffers, so ignore
				
	Debugprintf("Except Event Error = %d", Error);
#ifndef WIN32

//	SO_ERROR codes

#define	ETIMEDOUT		110	/* Connection timed out */
#define	ECONNREFUSED	111	/* Connection refused */
#define	EHOSTDOWN		112	/* Host is down */
#define	EHOSTUNREACH	113	/* No route to host */
#define	EALREADY		114	/* Operation already in progress */
#define	EINPROGRESS		115	/* Operation now in progress */

	if (Error == 0)
		getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&Error, &errlen);

	Debugprintf("Except Event Error after opts = %d", Error);
#endif

	if (Error)
	{
		if (sockptr->CMSSession && sockptr->RelaySession == 0)
		{
			// Try Next

			TCP->CMSFailed[sockptr->CMSIndex] = TRUE;

			CMSConnect(TNC, TNC->TCPInfo, &TNC->Streams[Stream], Stream);
			return 0;
		}
		else
		{
			int err = 0;
			int errlen = 4;

			getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen);

			buffptr[1] = sprintf((UCHAR *)&buffptr[2], "*** Failed to Connect\r");
					
			C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);
				
			closesocket(sock);
			TNC->Streams[Stream].Connecting = FALSE;
			sockptr->SocketActive = FALSE;
			ShowConnections(TNC);
			TNC->Streams[Stream].NeedDisc = 10;
			return 0;
		}
	}

	sockptr->LogonSent = FALSE;

	if (sockptr->CMSSession)
	{
		sockptr->LoginState = 3;			// Password State

		sockptr->UserPointer  = &CMSUser;
		strcpy(sockptr->Callsign, TNC->Streams[Stream].MyCall);

		sockptr->DoEcho = FALSE;
		sockptr->FBBMode = TRUE;
		sockptr->RelayMode = FALSE;
		sockptr->ClientSession = FALSE;
	
		SaveCMSHostInfo(TNC->Port, TNC->TCPInfo, sockptr->CMSIndex);

		if (CMSLogEnabled)
		{
			char logmsg[120];
			sprintf(logmsg,"%d %s Connected to CMS\r\n",
				sockptr->Number, TNC->Streams[Stream].MyCall);

			WriteCMSLog (logmsg);
		}
		
		buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** %s Connected to CMS\r", TNC->Streams[Stream].MyCall);;
	}
	else
	{
		sockptr->LoginState = 2;			// Data State
		sockptr->UserPointer  = &HostUser;
		strcpy(sockptr->Callsign, TNC->Streams[Stream].MyCall);
		sockptr->DoEcho = FALSE;
		sockptr->ClientSession = TRUE;


		if (sockptr->Signon[0])
		{
			buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "*** Connected to Server\r");
			send(sockptr->socket, sockptr->Signon, strlen(sockptr->Signon), 0);
		}
		else
		{
			buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "Connected to %s\r", 
				TNC->PortRecord->ATTACHEDSESSIONS[Stream]->L4CROSSLINK->APPL);
			
			send(sockptr->socket, Signon, sprintf(Signon, "%s\r\n", TNC->Streams[Stream].MyCall), 0);
		}
	}

	C_Q_ADD(&TNC->Streams[Stream].PACTORtoBPQ_Q, buffptr);

	sockptr->SocketActive = TRUE;
	sockptr->InputLen = 0;
//	sockptr->Number = Stream;
	sockptr->RelayMode = FALSE;
	sockptr->ConnectTime = time(NULL);
	TNC->Streams[Stream].Connecting = FALSE;
	TNC->Streams[Stream].Connected = TRUE;
	ShowConnections(TNC);

	if (sockptr->FromHostBuffer == 0)
	{
		sockptr->FromHostBuffer = malloc(10000);
		sockptr->FromHostBufferSize = 10000;
	}

	sockptr->FromHostBuffPutptr = sockptr->FromHostBuffGetptr = 0;

	TNC->Streams[Stream].BytesRXed = TNC->Streams[Stream].BytesTXed = 0;

	return 0;
}

VOID ReportError(struct STREAMINFO * STREAM, char * Msg)
{
	UINT * buffptr;

	buffptr = GetBuff();
	if (buffptr == 0) return;			// No buffers, so ignore
				
	buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "Error - %s\r", Msg);
					
	C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
}

VOID Report(struct STREAMINFO * STREAM, char * Msg)
{
	UINT * buffptr;

	buffptr = GetBuff();
	if (buffptr == 0) return;			// No buffers, so ignore
				
	buffptr[1]  = sprintf((UCHAR *)&buffptr[2], "%s\r", Msg);
					
	C_Q_ADD(&STREAM->PACTORtoBPQ_Q, buffptr);
}

BOOL CheckCMSThread(struct TNCINFO * TNC);

BOOL CheckCMS(struct TNCINFO * TNC)
{

	TNC->TCPInfo->CheckCMSTimer = 0;
	_beginthread((void (*)())CheckCMSThread, 0, TNC);

	return 0;
}

BOOL CheckCMSThread(struct TNCINFO * TNC)
{
	// Resolve Name and check connectivity to each address

	struct TCPINFO * TCP = TNC->TCPInfo;
//	struct hostent * HostEnt;
	struct in_addr addr;
	struct hostent *remoteHost;
    char **pAlias;	int i = 0;
	BOOL INETOK = FALSE;
	struct addrinfo hints, *res = 0, *saveres;
	int n;

	// First make sure we have a functioning DNS

	TCP->UseCachedCMSAddrs = FALSE;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_DGRAM;

	n = getaddrinfo("a.root-servers.net", NULL, &hints, &res);

	if (n == 0)
		goto rootok;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_DGRAM;
	n = getaddrinfo("b.root-servers.net", NULL, &hints, &res);
		 
	if (n == 0)
		goto rootok;

	Debugprintf("Resolve root nameserver failed");

	// Most likely is a local Internet Outage, but we could have Internet, but no name servers
	// Either way, switch to using cached CMS addresses. CMS Validation will check connectivity

	TCP->UseCachedCMSAddrs = TRUE;
	goto CheckServers;

rootok:

	freeaddrinfo(res);

//	INETOK = TRUE;			// We have connectivity

	res = 0;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_DGRAM;
	n = getaddrinfo("server.winlink.org", NULL, &hints, &res);
	 
	if (n || !res || res->ai_next == 0)	// Resolve Failed, or Returned only one Host
	{
		// Switch to Cached Servers
		
		TCP->UseCachedCMSAddrs = TRUE;

		if (res)
		{
			Debugprintf("Resolve CMS returned only on e host");
			freeaddrinfo(res);
		}
		else
			Debugprintf("Resolve CMS Failed");

		goto CheckServers;
	}

	saveres = res;

	while (res)
	{
		memcpy(&addr.s_addr, &res->ai_addr->sa_data[2], 4);
		TCP->CMSAddr[i] = addr;
		TCP->CMSFailed[i] = FALSE;
		i++;
		res = res->ai_next;
	}

	freeaddrinfo(saveres);

	TCP->NumberofCMSAddrs = i;

	i = 0;

	while (i < TCP->NumberofCMSAddrs)
	{
		if (TCP->CMSName[i])
			free(TCP->CMSName[i]);
		   		
		remoteHost = gethostbyaddr((char *) &TCP->CMSAddr[i], 4, AF_INET);

		if (remoteHost == NULL)
		{
			int dwError = WSAGetLastError();
	
			TCP->CMSName[i] = NULL;

			if (dwError != 0)
			{
				if (dwError == HOST_NOT_FOUND)
					Debugprintf("CMS - Host not found");
				else if (dwError == NO_DATA)
					Debugprintf("CMS No data record found");
			}
	   }
	   else
	   { 
		   Debugprintf("CMS #%d %s Official name : %s",i, inet_ntoa(TCP->CMSAddr[i]), remoteHost->h_name);
		   
		   TCP->CMSName[i] = _strdup(remoteHost->h_name);			// Save Host Name
	
		   for (pAlias = remoteHost->h_aliases; *pAlias != 0; pAlias++)
		   {
			   Debugprintf("\tAlternate name #%d: %s\n", ++i, *pAlias);
		   }
	   }
		i++;
	}

CheckServers:
#ifndef LINBPQ
	CheckMenuItem(TNC->TCPInfo->hActionMenu, 4, MF_BYPOSITION | TCP->UseCachedCMSAddrs<<3);
#endif
	if (TCP->UseCachedCMSAddrs)
	{
		// Get Cached Servers from Registry - Names are in RegTree\G8BPQ\BPQ32\PACTOR\PORTn\CMSInfo

		GetCMSCachedInfo(TNC);
	}

	if (TCP->NumberofCMSAddrs == 0)
	{
		TCP->CMSOK = FALSE;
#ifndef LINBPQ
		SetWindowText(TCP->hCMSWnd, "NO CMS"); 
#endif
		return TRUE;
	}

	// if we don't know we have Internet connectivity, make sure we can connect to at least one of them

	TCP->CMSOK = INETOK | CMSCheck(TNC, TCP);		// If we know we have Inet, dont check connectivity
	
#ifndef LINBPQ
	if (TCP->CMSOK)
		MySetWindowText(TCP->hCMSWnd, "CMS OK"); 
	else
		MySetWindowText(TCP->hCMSWnd, "NO CMS"); 
#endif
	return TRUE;
}

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 255
#define MAX_VALUE_DATA 255


VOID GetCMSCachedInfo(struct TNCINFO * TNC)
{
	struct TCPINFO * TCP = TNC->TCPInfo;
	ULONG IPAD;

#ifdef LINBPQ
	
	FILE *in;
	char Buffer[2048];
	char *buf = Buffer;
	char *ptr1, *ptr2, *context;
	int i = 0;

	TCP->NumberofCMSAddrs = 0;

	in = fopen("CMSInfo.txt", "r");

	if (!(in)) return;

	while(fgets(buf, 128, in))
	{
		ptr1 =  strtok_s(buf, ", ", &context);
		ptr2 =  strtok_s(NULL, ", ", &context);		// Skip Time
		ptr2 =  strtok_s(NULL, ", ", &context);
			
		IPAD = inet_addr(ptr2);

		memcpy(&TCP->CMSAddr[i], &IPAD, 4);

		TCP->CMSFailed[i] = FALSE;
		
		if (TCP->CMSName[i])
			free(TCP->CMSName[i]);
		   		
		TCP->CMSName[i] = _strdup(ptr1);			// Save Host Name
		i++;
	}

	fclose(in);

	TCP->NumberofCMSAddrs = i;

#else

    TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
    DWORD    cchClassName = MAX_PATH;  // size of class string 
    DWORD    cSubKeys=0;               // number of subkeys 
    DWORD    cbMaxSubKey;              // longest subkey size 
    DWORD    cchMaxClass;              // longest class string 
    DWORD    cValues;              // number of values for key 
    DWORD    cchMaxValue;          // longest value name 
    DWORD    cbMaxValueData;       // longest value data 
    DWORD    cbSecurityDescriptor; // size of security descriptor 
    FILETIME ftLastWriteTime;      // last write time 
 
    DWORD i, retCode; 
 
    TCHAR  achValue[MAX_VALUE_NAME]; 
    DWORD cchValue = MAX_VALUE_NAME; 

	HKEY hKey=0;
	char Key[80];

	Debugprintf("Getting cached CMS Server info"); 

	TCP->NumberofCMSAddrs = 0;

	sprintf(Key, "SOFTWARE\\G8BPQ\\BPQ32\\PACTOR\\PORT%d\\CMSInfo", TNC->Port);
	
	retCode = RegOpenKeyEx (REGTREE, Key, 0, KEY_QUERY_VALUE, &hKey);

	if (retCode != ERROR_SUCCESS)
	{
		Debugprintf("Cached CMS Server info key not found"); 
		return;
	}

    // Get the class name and the value count. 
    retCode = RegQueryInfoKey(
        hKey,                    // key handle 
        achClass,                // buffer for class name 
        &cchClassName,           // size of class string 
        NULL,                    // reserved 
        &cSubKeys,               // number of subkeys 
        &cbMaxSubKey,            // longest subkey size 
        &cchMaxClass,            // longest class string 
        &cValues,                // number of values for this key 
        &cchMaxValue,            // longest value name 
        &cbMaxValueData,         // longest value data 
        &cbSecurityDescriptor,   // security descriptor 
        &ftLastWriteTime);       // last write time 
 
    // Enumerate the key values. 

    if (cValues == 0)
	{
		Debugprintf("No Cached CMS Servers found"); 
		return;
	}

	for (i=0, retCode=ERROR_SUCCESS; i<cValues; i++) 
	{ 
		int Type;
		int ValLen = MAX_VALUE_DATA;
		UCHAR Value[MAX_VALUE_DATA] = "";

		cchValue = MAX_VALUE_NAME; 
		achValue[0] = '\0'; 
	
		retCode = RegEnumValue(hKey, i, achValue, &cchValue, NULL, &Type, Value, &ValLen);
 
		if (retCode == ERROR_SUCCESS) 
		{
			char Time[80], IPADDR[80];
			
			Debugprintf("%s = %s", achValue, Value); 
			sscanf(Value, "%s %s", &Time, &IPADDR);

			IPAD = inet_addr(IPADDR);
			memcpy(&TCP->CMSAddr[i], &IPAD, 4);

			TCP->CMSFailed[i] = FALSE;
		
			if (TCP->CMSName[i])
				free(TCP->CMSName[i]);
		   		
			TCP->CMSName[i] = _strdup(achValue);			// Save Host Name
		}
	}

	RegCloseKey(hKey);

	TCP->NumberofCMSAddrs = i;

#endif
	return;
}

BOOL CMSCheck(struct TNCINFO * TNC, struct TCPINFO * TCP)
{
	// Make sure at least one CMS can be connected to

	u_long param=1;
	BOOL bcopt=TRUE;
	SOCKET sock;
	struct sockaddr_in sinx; 
	struct sockaddr_in destaddr;
	int addrlen=sizeof(sinx);
	int n = 0;

	destaddr.sin_family = AF_INET; 
	destaddr.sin_port = htons(8772);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	for (n = 0; n < TCP->NumberofCMSAddrs;  n++)
	{
		sock = socket(AF_INET, SOCK_STREAM, 0);

		if (sock == INVALID_SOCKET)
			return FALSE;
	
		memcpy(&destaddr.sin_addr.s_addr, &TCP->CMSAddr[n], 4);

		setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt,4);

		if (bind(sock, (struct sockaddr *) &sinx, addrlen) != 0 )
	  	 	return FALSE;

		if (connect(sock,(struct sockaddr *) &destaddr, sizeof(destaddr)) == 0)
		{
			closesocket(sock);
			return TRUE;
		}

		// Failed - try next

		closesocket(sock);
	}
	return FALSE;
}


					
CMSConnect(struct TNCINFO * TNC, struct TCPINFO * TCP, struct STREAMINFO * STREAM, int Stream)
{
	int err;
	u_long param=1;
	BOOL bcopt=TRUE;
	struct ConnectionInfo * sockptr;
	SOCKET sock;
	struct sockaddr_in sinx; 
	struct sockaddr_in destaddr;
	int addrlen=sizeof(sinx);
	int n;
	char Msg[80];

	sockptr = STREAM->ConnectionInfo;
		
	sock = sockptr->socket = socket(AF_INET, SOCK_STREAM, 0);

	if (sock == INVALID_SOCKET)
	{
		ReportError(STREAM, "Create Socket Failed");
		return FALSE;
	}

	sockptr->SocketActive = TRUE;
	sockptr->InputLen = 0;
	sockptr->LoginState = 2;
	sockptr->UserPointer = 0;
	sockptr->DoEcho = FALSE;
	sockptr->BPQTermMode = FALSE;

	sockptr->FBBMode = TRUE;		// Raw Data
	sockptr->NeedLF = FALSE;

	destaddr.sin_family = AF_INET; 
	destaddr.sin_port = htons(8772);

	// See if current CMS is down

	n = 0;

	while (TCP->CMSFailed[TCP->NextCMSAddr])
	{
		TCP->NextCMSAddr++;
		if (TCP->NextCMSAddr >= TCP->NumberofCMSAddrs) TCP->NextCMSAddr = 0;
		n++;

		if (n == TCP->NumberofCMSAddrs)
		{
			TCP->CMSOK = FALSE;
#ifndef LINBPQ
			DrawMenuBar(TNC->hDlg);	
#endif
			ReportError(STREAM, "All CMS Servers are inaccessible");
			closesocket(sock);
			STREAM->NeedDisc = 10;
			TNC->Streams[Stream].Connecting = FALSE;
			sockptr->SocketActive = FALSE;
			ShowConnections(TNC);
			return FALSE;
		}
	}

	sockptr->CMSIndex = TCP->NextCMSAddr;

	sprintf(Msg, "Trying %s", TCP->CMSName[TCP->NextCMSAddr]);

	memcpy(&destaddr.sin_addr.s_addr, &TCP->CMSAddr[TCP->NextCMSAddr++], 4);

	if (TCP->NextCMSAddr >= TCP->NumberofCMSAddrs)
		TCP->NextCMSAddr = 0;

	ioctl(sockptr->socket, FIONBIO, &param);

	setsockopt (sockptr->socket, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt,4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (bind(sockptr->socket, (struct sockaddr *) &sinx, addrlen) != 0 )
	{
		ReportError(STREAM, "Bind Failed");	
  	 	return FALSE; 
	}

#ifndef LINBPQ
	ModifyMenu(TCP->hDisMenu, Stream - 1, MF_BYPOSITION | MF_STRING, IDM_DISCONNECT + Stream, "CMS");
#endif

	Report(STREAM, Msg);	

	if (connect(sockptr->socket,(struct sockaddr *) &destaddr, sizeof(destaddr)) == 0)
	{
		//
		//	Connected successful
		//

		ReportError(STREAM, "*** Connected");	
		return TRUE;
	}
	else
	{
		err=WSAGetLastError();

		if (err == 10035 || err == 115 || err == 36)		//EWOULDBLOCK
		{
			//	Connect in Progress

			sockptr->UserPointer  = &CMSUser;
			return TRUE;
		}
		else
		{
			//	Connect failed

			closesocket(sockptr->socket);

			if (sockptr->CMSSession && sockptr->RelaySession == 0)
			{
				// Try Next

				TCP->CMSFailed[sockptr->CMSIndex] = TRUE;

				CMSConnect(TNC, TNC->TCPInfo, &TNC->Streams[Stream], Stream);
				return 0;
			}

			ReportError(STREAM, "Connect Failed");
			CheckCMS(TNC);

			STREAM->Connecting = FALSE;
			sockptr->SocketActive = FALSE;
			ShowConnections(TNC);
			STREAM->NeedDisc = 10;

			return FALSE;
		}
	}
	return FALSE;

}

VOID SaveCMSHostInfo(int port, struct TCPINFO * TCP, int CMSNo)
{
	char Info[80];
#ifdef LINBPQ
	unsigned char work[4];
	FILE *in, *out;
	char *c;
	char Buffer[2048];
	char *buf = Buffer;

	memcpy(work, &TCP->CMSAddr[CMSNo].s_addr, 4);

	sprintf(Info,"%s %d %d.%d.%d.%d\n", TCP->CMSName[CMSNo], (int)time(NULL),
					work[0], work[1], work[2], work[3]);


	in = fopen("CMSInfo.txt", "r");

	if (!(in))
	{
		in = fopen("CMSInfo.txt", "w");
		fclose(in);
		in = fopen("CMSInfo.txt", "r");
	}

	out = fopen("CMSInfo.tmp", "w");

	if (!(in) || !(out)) return;

	while(fgets(buf, 128, in))
	{
		c = strchr(buf, ' ');
		if (c) *c = '\0';
			
		if (strcmp(buf, TCP->CMSName[CMSNo]) != 0)
		{
			if (c) *c = ' ';
				
			fputs(buf, out);
		}
	}

	fputs(Info, out);

	fclose(in);
	fclose(out);

	remove("CMSInfo.txt");
	rename("CMSInfo.tmp", "CMSInfo.txt");

#else

	HKEY hKey=0;
	char Key[80];
	int retCode, disp;

	sprintf(Key, "SOFTWARE\\G8BPQ\\BPQ32\\PACTOR\\PORT%d\\CMSInfo", port);
	
	retCode = RegCreateKeyEx(REGTREE, Key, 0, 0, 0,
            KEY_ALL_ACCESS, NULL, &hKey, &disp);

	if (retCode == ERROR_SUCCESS)
	{
		unsigned char work[4];
		memcpy(work, &TCP->CMSAddr[CMSNo].s_addr, 4);
		sprintf(Info,"%d %d.%d.%d.%d", time(NULL),
					work[0], work[1], work[2], work[3]);

		retCode = RegSetValueEx(hKey, TCP->CMSName[CMSNo], 0, REG_SZ, (BYTE *)&Info, strlen(Info));
		RegCloseKey(hKey);
	}
#endif
	return;
}


//Keep this in case we ever do general outgoing TCP

TCPConnect(struct TNCINFO * TNC, struct TCPINFO * TCP, struct STREAMINFO * STREAM, char * Host, int Port, BOOL FBB)
{
	int err;
	u_long param=1;
	BOOL bcopt=TRUE;
	struct ConnectionInfo * sockptr;
	SOCKET sock;
	struct sockaddr_in sinx; 
	struct sockaddr_in destaddr;
	int addrlen=sizeof(sinx);
	int i;

	sockptr = STREAM->ConnectionInfo;
		
	sock = sockptr->socket = socket(AF_INET, SOCK_STREAM, 0);

	if (sock == INVALID_SOCKET)
	{
		ReportError(STREAM, "Create Socket Failed");
		return FALSE;
	}
	
	sockptr->SocketActive = TRUE;
	sockptr->InputLen = 0;
	sockptr->LoginState = 2;
	sockptr->UserPointer = 0;
	sockptr->DoEcho = FALSE;

	sockptr->FBBMode = FBB;		// Raw Data
	
	// Resolve Name if needed

	destaddr.sin_family = AF_INET; 
	destaddr.sin_port = htons(Port);

	destaddr.sin_addr.s_addr = inet_addr(Host);

	if (destaddr.sin_addr.s_addr == INADDR_NONE)
	{
		struct hostent * HostEnt;

		//	Resolve name to address

		HostEnt = gethostbyname(Host);
		 
		if (!HostEnt)
		{
			ReportError(STREAM, "Resolve HostName Failed");
			return FALSE;			// Resolve failed
		}
		i = 0;
		while (HostEnt->h_addr_list[i] != 0)
		{
			    struct in_addr addr;
				addr.s_addr = *(u_long *) HostEnt->h_addr_list[i++];
		}
		memcpy(&destaddr.sin_addr.s_addr, HostEnt->h_addr, 4);
	}

	ioctl (sockptr->socket, FIONBIO, &param);
 
	setsockopt (sockptr->socket, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt,4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (bind(sockptr->socket, (struct sockaddr *) &sinx, addrlen) != 0 )
	{
		ReportError(STREAM, "Bind Failed");	
  	 	return FALSE; 
	}

	if (connect(sockptr->socket,(struct sockaddr *) &destaddr, sizeof(destaddr)) == 0)
	{
		//
		//	Connected successful
		//

		ReportError(STREAM, "*** Connected");

		// Get Send Buffer Size

		return TRUE;
	}
	else
	{
		err=WSAGetLastError();

		if (err == 10035 || err == 115 || err == 36)		//EWOULDBLOCK
		{
			//	Connect in Progress

			sockptr->UserPointer  = &HostUser;
			return TRUE;
		}
		else
		{
			//	Connect failed

			closesocket(sockptr->socket);
			ReportError(STREAM, "Connect Failed");	
			STREAM->Connecting = FALSE;
			sockptr->SocketActive = FALSE;
			ShowConnections(TNC);
			STREAM->NeedDisc = 10;

			return FALSE;
		}
	}

	return FALSE;

}


VOID Tel_Format_Addr(struct ConnectionInfo * sockptr, char * dst)
{
	unsigned char * src;
	char zeros[12] = "";
	char * ptr;
	struct
	{
		int base, len;
	} best, cur;
	unsigned int words[8];
	int i;

	if (sockptr->sin.sin_family != AF_INET6)
	{
		unsigned char work[4];
		memcpy(work, &sockptr->sin.sin_addr.s_addr, 4);
		sprintf(dst,"%d.%d.%d.%d", work[0], work[1], work[2], work[3]);
		return;
	}

	src = (unsigned char *)&sockptr->sin6.sin6_addr;

	// See if Encapsulated IPV4 addr

	if (src[12] != 0)
	{
		if (memcmp(src, zeros, 12) == 0)	// 12 zeros, followed by non-zero
		{
			sprintf(dst,"::%d.%d.%d.%d", src[12], src[13], src[14], src[15]);
			return;
		}
	}

	// COnvert 16 bytes to 8 words
	
	for (i = 0; i < 16; i += 2)
	    words[i / 2] = (src[i] << 8) | src[i + 1];

	// Look for longest run of zeros
	
	best.base = -1;
	cur.base = -1;
	
	for (i = 0; i < 8; i++)
	{
		if (words[i] == 0)
		{
	        if (cur.base == -1)
				cur.base = i, cur.len = 1;		// New run, save start
	          else
	            cur.len++;						// Continuation - increment length
		}
		else
		{
			// End of a run of zeros

			if (cur.base != -1)
			{
				// See if this run is longer
				
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				
				cur.base = -1;	// Start again
			}
		}
	}
	
	if (cur.base != -1)
	{
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	
	if (best.base != -1 && best.len < 2)
	    best.base = -1;
	
	ptr = dst;
	  
	for (i = 0; i < 8; i++)
	{
		/* Are we inside the best run of 0x00's? */

		if (best.base != -1 && i >= best.base && i < (best.base + best.len))
		{
			// Just output one : for whole string of zeros
			
			*ptr++ = ':';
			i = best.base + best.len - 1;	// skip rest of zeros
			continue;
		}
	    
		/* Are we following an initial run of 0x00s or any real hex? */
		
		if (i != 0)
			*ptr++ = ':';
		
		ptr += sprintf (ptr, "%x", words[i]);
	        
		//	Was it a trailing run of 0x00's?
	}

	if (best.base != -1 && (best.base + best.len) == 8)
		*ptr++ = ':';
	
	*ptr++ = '\0';	
}

BOOL TelSendPacket(int Stream, struct STREAMINFO * STREAM, UINT * buffptr)
{
	int datalen;
	UCHAR * MsgPtr;
	SOCKET sock;
	struct ConnectionInfo * sockptr = STREAM->ConnectionInfo;

	datalen = buffptr[1];
	MsgPtr = (UCHAR *)&buffptr[2];

	STREAM->BytesTXed += datalen;

	sock = sockptr->socket;

	if (sockptr->UserPointer  == &CMSUser)
		WritetoTrace(Stream, MsgPtr, datalen);

	if (sockptr->UTF8)
	{
		// Convert any non-utf8 chars

		if (IsUTF8(MsgPtr, datalen) == FALSE)
		{
			unsigned char UTF[1024];
			int u, code;

			// Try to guess encoding

			code = TrytoGuessCode(MsgPtr, datalen);

			if (code == 437)
				u = Convert437toUTF8(MsgPtr, datalen, UTF);
			else if (code == 1251)
				u = Convert1251toUTF8(MsgPtr, datalen, UTF);
			else
				u = Convert1252toUTF8(MsgPtr, datalen, UTF);
			
			SendAndCheck(sockptr, UTF, u, 0);
			ReleaseBuffer(buffptr);
			return TRUE;
		}
	}

	if (sockptr->FBBMode && sockptr->NeedLF == FALSE)
	{
/*
		// if Outward Connect to FBB,  Replace ff with ffff

		if (0)		// if we use this need to fix retry
		{
			char * ptr2, * ptr = &MsgPtr[0];
			int i;
			do 
			{
				ptr2 = memchr(ptr, 255, datalen);

				if (ptr2 == 0)
				{
					//	no ff, so just send as is 

					xxxsend(sock, ptr, datalen, 0);
					i=0;
					break;
				}

				i=ptr2+1-ptr;

				xxsend(sock,ptr,i,0);
				xxsend(sock,"\xff",1,0);

				datalen-=i;
				ptr=ptr2+1;
			}
			while (datalen>0);
		}
*/
		// Normal FBB Mode path

		BOOL ret = SendAndCheck(sockptr, MsgPtr, datalen, 0);
		ReleaseBuffer(buffptr);
		return ret;
	}
	
	// Not FBB mode, or FBB and NEEDLF Replace cr with crlf

	{
		unsigned char Out[1024];
		unsigned char c;
		unsigned char * ptr2 = Out;
		unsigned char * ptr = &MsgPtr[0];

		while (datalen--)
		{
			c = (*ptr++);
			
			if (c == 13)
			{
				*(ptr2++) = 13;
				*(ptr2++) = 10;
			}
			else
				*(ptr2++) = c;
		}

		ReleaseBuffer(buffptr);
		return SendAndCheck(sockptr, Out, ptr2 - Out, 0);
	}
}

VOID ProcessTrimodeCommand(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, char * MsgPtr)
{
	struct STREAMINFO * STREAM = &TNC->Streams[sockptr->Number];
	int Port = 4;

	Debugprintf(MsgPtr);
	
	if (strcmp(MsgPtr, "CLOSE") == 0)
	{
		if (STREAM->Connected)
		{
			STREAM->ReportDISC = TRUE;
		}
	}

// MYCALLSIGN XE2BNC
	else
	if (memcmp(MsgPtr, "MYCALLSIGN", 10) == 0)
	{
		char * call = &MsgPtr[11];
		
		if (strlen(call) > 9)
			call[9] = 0;

		memcpy(STREAM->MyCall, call, 10);

		ConvToAX25(call, &TNC->PortRecord->ATTACHEDSESSIONS[sockptr->Number]->L4USER[0]);

		strcpy(&TNCInfo[Port]->Streams[0].MyCall[0], call);
	}


// TARGETCALLSIGN KE7XO
	else
	if (memcmp(MsgPtr, "TARGETCALLSIGN", 14) == 0)
	{
		char * call = &MsgPtr[15];
		
		if (strlen(call) > 9)
			call[9] = 0;

		memcpy(STREAM->RemoteCall, call, 10);
	}
// INITIATECALL 50
	else
	if (memcmp(MsgPtr, "INITIATECALL", 12) == 0)
	{
		char Cmd[80];
		int n;
		
		n = sprintf(Cmd,"C %s\r", STREAM->RemoteCall);

		SendtoNode(TNC, sockptr->Number, Cmd, n);
	}


// CHANNEL 3586500,None,None
	else
	if (memcmp(MsgPtr, "CHANNEL", 7) == 0)
	{
		double Freq = atof(&MsgPtr[8]);
		char Radiocmd[80];
		int n;

		strcpy(sockptr->Callsign, "G8BPQ");
		
		n = sprintf(Radiocmd,"RADIO %f %s\r", Freq/1000000, "USB");

		SendtoNode(TNC, sockptr->Number, Radiocmd, n);
	}

	else
	if (memcmp(MsgPtr, "PROTOCOL", 8) == 0)
	{
		// Attach the relevant port

		SendtoNode(TNC, sockptr->Number, "ATTACH 4\r", 9);
	}

	else
	if (strcmp(MsgPtr, "BUSY") == 0)
		send(sockptr->socket, "BUSY False\r\n", 12,0);


	send(sockptr->socket, "CMD\r\n", 5,0);

//	SendtoNode(TNC, sockptr->Number, NodeLine, len);
}


VOID ProcessTrimodeResponse(struct TNCINFO * TNC, struct STREAMINFO * STREAM, unsigned char * MsgPtr, int Msglen)
{
	MsgPtr[Msglen] = 0;

	if (STREAM->ConnectionInfo->TriModeConnected)
	{
		// Send over the Data Socket

		send(STREAM->ConnectionInfo->TriModeDataSock, MsgPtr, Msglen, 0);

		return;
	}

	strlop(MsgPtr, 13);
	Debugprintf(MsgPtr);

	if (memcmp(MsgPtr, "*** Connected to ", 17) == 0)
	{
		char Cmd[80];
		int n;
		
		n = sprintf(Cmd,"CONNECTED %s\r", &MsgPtr[17]);

		STREAM->ConnectionInfo->TriModeConnected = TRUE;

		send(STREAM->ConnectionInfo->socket, Cmd, n, 0);
	}
}

VOID ProcessTriModeDataMessage(struct TNCINFO * TNC, struct ConnectionInfo * sockptr, SOCKET sock, struct STREAMINFO * STREAM)
{
	int len=0;
	char NLMsg[3]={13,10,0};
	char RelayMsg[] = "No CMS connection available - using local BPQMail\r";
	struct TCPINFO * TCP = TNC->TCPInfo;
	unsigned char Buffer[256];

	ioctl(sock,FIONREAD,&len);
	
	if (len > 256) len = 256;

	len = recv(sock, Buffer, len, 0);

	if (len == SOCKET_ERROR || len ==0)
	{
		// Failed or closed - clear connection

		closesocket(sock);
		return;
	}

	SendtoNode(TNC, sockptr->Number, Buffer, len);
}




int IsUTF8(unsigned char *ptr, int len)
{
	int n; 
	unsigned char * cpt = ptr;

	// This has to be a bit loose, as UTF8 sequences may split over packets

	memcpy(&ptr[len], "\x80\x80\x80", 3);		// in case trailing bytes are in next packet

	// Don't check first 3 if could be part of sequence
	
	if ((*(cpt) & 0xC0) == 0x80)				// Valid continuation
	{
		cpt++;
		len--;
		if ((*(cpt) & 0xC0) == 0x80)			// Valid continuation
		{
			cpt++;
			len--;
			if ((*(cpt) & 0xC0) == 0x80)		// Valid continuation
			{
				cpt++;
				len--;
			}
		}
	}

	cpt--;
										
	for (n = 0; n < len; n++)
	{
		cpt++;
		
		if (*cpt < 128)
			continue;

		if ((*cpt & 0xF8) == 0xF0)
		{ // start of 4-byte sequence
			if (((*(cpt + 1) & 0xC0) == 0x80)
		     && ((*(cpt + 2) & 0xC0) == 0x80)
			 && ((*(cpt + 3) & 0xC0) == 0x80))
			{
				cpt += 3;
				n += 3;
				continue;
			}
			return FALSE;
	    }
		else if ((*cpt & 0xF0) == 0xE0)
		{ // start of 3-byte sequence
	        if (((*(cpt + 1) & 0xC0) == 0x80)
		     && ((*(cpt + 2) & 0xC0) == 0x80))
			{
				cpt += 2;
				n += 2;
				continue;
			}
			return FALSE;
		}
		else if ((*cpt & 0xE0) == 0xC0)
		{ // start of 2-byte sequence
	        if ((*(cpt + 1) & 0xC0) == 0x80)
			{
				cpt++;
				n++;
				continue;
			}
			return FALSE;
		}
		return FALSE;
	}

    return TRUE;
}

unsigned int CP437toUTF8Data[128] = {
  34755, 48323, 43459, 41667,  
  42179, 41155, 42435, 42947,  
  43715, 43971, 43203, 44995,  
  44739, 44227, 33987, 34243,  
  35267, 42691, 34499, 46275,  
  46787, 45763, 48067, 47555,  
  49091, 38595, 40131, 41666,  
  41922, 42434, 10978018, 37574,  
  41411, 44483, 46019, 47811,  
  45507, 37315, 43714, 47810,  
  49090, 9473250, 44226, 48578,  
  48322, 41410, 43970, 48066,  
  9541346, 9606882, 9672418, 8557794,  
  10786018, 10589666, 10655202, 9868770,  
  9803234, 10720738, 9541090, 9934306,  
  10327522, 10261986, 10196450, 9475298,  
  9737442, 11834594, 11310306, 10261730,  
  8426722, 12358882, 10393058, 10458594,  
  10130914, 9737698, 11113954, 10917346,  
  10524130, 9475554, 11310562, 10982882,  
  11048418, 10786274, 10851810, 10065378,  
  9999842, 9606626, 9672162, 11245026,  
  11179490, 9999586, 9213154, 8951522,  
  8689378, 9213666, 9475810, 8427234,  
  45518, 40899, 37838, 32975,  
  41934, 33743, 46530, 33999,  
  42702, 39118, 43470, 46286,  
  10389730, 34511, 46542, 11110626,  
  10586594, 45506, 10848738, 10783202,  
  10521826, 10587362, 47043, 8948194,  
  45250, 10062050, 47042, 10127586,  
  12550626, 45762, 10524386, 41154,  
};
unsigned int CP437toUTF8DataLen[128] = {
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 3, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 3, 2, 2,  
  2, 2, 2, 2,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  3, 3, 3, 3,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  3, 2, 2, 3,  
  3, 2, 3, 3,  
  3, 3, 2, 3,  
  2, 3, 2, 3,  
  3, 2, 3, 2,  
};

unsigned int CP1251toUTF8Data[128] = {
  33488, 33744, 10125538, 37841,  
  10387682, 10911970, 10518754, 10584290,  
  11305698, 11567330, 35280, 12157154,  
  35536, 36048, 35792, 36816,  
  37585, 9994466, 10060002, 10256610,  
  10322146, 10649826, 9666786, 9732322,  
  39106, 10650850, 39377, 12222690,  
  39633, 40145, 39889, 40913,  
  41154, 36560, 40657, 35024,  
  42178, 37074, 42690, 42946,  
  33232, 43458, 34000, 43970,  
  44226, 44482, 44738, 34768,  
  45250, 45506, 34512, 38609,  
  37330, 46530, 46786, 47042,  
  37329, 9864418, 38097, 48066,  
  39121, 34256, 38353, 38865,  
  37072, 37328, 37584, 37840,  
  38096, 38352, 38608, 38864,  
  39120, 39376, 39632, 39888,  
  40144, 40400, 40656, 40912,  
  41168, 41424, 41680, 41936,  
  42192, 42448, 42704, 42960,  
  43216, 43472, 43728, 43984,  
  44240, 44496, 44752, 45008,  
  45264, 45520, 45776, 46032,  
  46288, 46544, 46800, 47056,  
  47312, 47568, 47824, 48080,  
  48336, 48592, 48848, 49104,  
  32977, 33233, 33489, 33745,  
  34001, 34257, 34513, 34769,  
  35025, 35281, 35537, 35793,  
  36049, 36305, 36561, 36817,  
};
unsigned int CP1251toUTF8DataLen[128] = {
  2, 2, 3, 2,  
  3, 3, 3, 3,  
  3, 3, 2, 3,  
  2, 2, 2, 2,  
  2, 3, 3, 3,  
  3, 3, 3, 3,  
  2, 3, 2, 3,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 3, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
};


unsigned int CP1252toUTF8Data[128] = {
  11305698, 33218, 10125538, 37574,  
  10387682, 10911970, 10518754, 10584290,  
  34507, 11567330, 41157, 12157154,  
  37573, 36290, 48581, 36802,  
  37058, 9994466, 10060002, 10256610,  
  10322146, 10649826, 9666786, 9732322,  
  40139, 10650850, 41413, 12222690,  
  37829, 40386, 48837, 47301,  
  41154, 41410, 41666, 41922,  
  42178, 42434, 42690, 42946,  
  43202, 43458, 43714, 43970,  
  44226, 44482, 44738, 44994,  
  45250, 45506, 45762, 46018,  
  46274, 46530, 46786, 47042,  
  47298, 47554, 47810, 48066,  
  48322, 48578, 48834, 49090,  
  32963, 33219, 33475, 33731,  
  33987, 34243, 34499, 34755,  
  35011, 35267, 35523, 35779,  
  36035, 36291, 36547, 36803,  
  37059, 37315, 37571, 37827,  
  38083, 38339, 38595, 38851,  
  39107, 39363, 39619, 39875,  
  40131, 40387, 40643, 40899,  
  41155, 41411, 41667, 41923,  
  42179, 42435, 42691, 42947,  
  43203, 43459, 43715, 43971,  
  44227, 44483, 44739, 44995,  
  45251, 45507, 45763, 46019,  
  46275, 46531, 46787, 47043,  
  47299, 47555, 47811, 48067,  
  48323, 48579, 48835, 49091,  
};
unsigned int CP1252toUTF8DataLen[128] = {
  3, 2, 3, 2,  
  3, 3, 3, 3,  
  2, 3, 2, 3,  
  2, 2, 2, 2,  
  2, 3, 3, 3,  
  3, 3, 3, 3,  
  2, 3, 2, 3,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
  2, 2, 2, 2,  
};

#ifdef __BIG_ENDIAN__
BOOL initUTF8Done = FALSE;
#else
BOOL initUTF8Done = TRUE;
#endif

VOID initUTF8()
{
	// Swap bytes of UTF-8 COde on Big-endian systems
	
	int n;
	char temp[4];
	char rev[4];

	if (initUTF8Done)
		return;
	
	for (n = 0; n <128; n++)
	{
		memcpy(temp, &CP437toUTF8Data[n], 4);
		rev[0] = temp[3];
		rev[1] = temp[2];
		rev[2] = temp[1];
		rev[3] = temp[0];

		memcpy(&CP437toUTF8Data[n], rev, 4);

		memcpy(temp, &CP1251toUTF8Data[n], 4);
		rev[0] = temp[3];
		rev[1] = temp[2];
		rev[2] = temp[1];
		rev[3] = temp[0];

		memcpy(&CP1251toUTF8Data[n], rev, 4);

		memcpy(temp, &CP1252toUTF8Data[n], 4);
		rev[0] = temp[3];
		rev[1] = temp[2];
		rev[2] = temp[1];
		rev[3] = temp[0];

		memcpy(&CP1252toUTF8Data[n], rev, 4);
	}

	initUTF8Done = TRUE;
}

int Convert437toUTF8(unsigned char * MsgPtr, int len, unsigned char * UTF)
{
	unsigned char * ptr1 = MsgPtr;
	unsigned char * ptr2 = UTF;
	int n;
	unsigned int c;

	for (n = 0; n < len; n++)
	{
		c = *(ptr1++);

		if (c < 128)
		{
			*(ptr2++) = c;
			continue;
		}

		memcpy(ptr2, &CP437toUTF8Data[c - 128], CP437toUTF8DataLen[c - 128]);
		ptr2 += CP437toUTF8DataLen[c - 128];
	}

	return ptr2 - UTF;
}

int Convert1251toUTF8(unsigned char * MsgPtr, int len, unsigned char * UTF)
{
	unsigned char * ptr1 = MsgPtr;
	unsigned char * ptr2 = UTF;
	int n;
	unsigned int c;

	for (n = 0; n < len; n++)
	{
		c = *(ptr1++);

		if (c < 128)
		{
			*(ptr2++) = c;
			continue;
		}

		memcpy(ptr2, &CP1251toUTF8Data[c - 128], CP1251toUTF8DataLen[c - 128]);
		ptr2 += CP1251toUTF8DataLen[c - 128];
	}

	return ptr2 - UTF;
}

int Convert1252toUTF8(unsigned char * MsgPtr, int len, unsigned char * UTF)
{
	unsigned char * ptr1 = MsgPtr;
	unsigned char * ptr2 = UTF;
	int n;
	unsigned int c;

	for (n = 0; n < len; n++)
	{
		c = *(ptr1++);

		if (c < 128)
		{
			*(ptr2++) = c;
			continue;
		}

		memcpy(ptr2, &CP1252toUTF8Data[c - 128], CP1252toUTF8DataLen[c - 128]);
		ptr2 += CP1252toUTF8DataLen[c - 128];
	}

	return ptr2 - UTF;
}

int TrytoGuessCode(unsigned char * Char, int Len)
{
	int Above127 = 0;
	int LineDraw = 0;
	int NumericAndSpaces = 0;
	int n;

	for (n = 0; n < Len; n++)
	{
		if (Char[n] < 65)
		{
			NumericAndSpaces++;
		}
		else
		{
			if (Char[n] > 127)
			{
				Above127++;
				if (Char[n] > 178 && Char[n] < 219)
				{
					LineDraw++;
				}
			}
		}
	}

	if (Above127 == 0)			// DOesn't really matter!
		return 1252;

	if (Above127 == LineDraw)
		return 437;			// If only Line Draw chars, assume line draw

	// If mainly below 128, it is probably Latin if mainly above, probably Cyrillic

	if ((Len - (NumericAndSpaces + Above127)) < Above127) 
		return 1251;
	else
		return 1252;
}
