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

/*

 AGWPE emulation Interface for BPQ32
 
 Based on AGWtoBPQ
 	
*/
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include "CHeaders.h"

#include "bpq32.h"

// Internal AGW Interface

//#define VVICON 400

struct AGWHeader
{
    int Port;
	unsigned char DataKind;
    unsigned char filler2;
	unsigned char PID;
    unsigned char filler3;
    unsigned char callfrom[10];
    unsigned char callto[10];
    int DataLength;
    int reserved;
};

struct AGWSocketConnectionInfo
{
	int Number;					// Number of record - for AGWConnections display
    SOCKET socket;
	SOCKADDR_IN sin;  
	BOOL SocketActive;
    BOOL RawFlag;
    BOOL MonFlag;
    unsigned char  CallSign[10];
    BOOL GotHeader;
    int MsgDataLength;
    struct AGWHeader AGWRXHeader;   
};

struct BPQConnectionInfo
{    
    struct AGWSocketConnectionInfo * SocketIndex;
    int BPQStream;
    unsigned char  CallKey[21];					// Port + two calls
    BOOL Connecting;					// Set while waiting for connection to complete
    BOOL Listening;
    int ApplMask;   
} ConInfoRec;


char AGWPorts[1000];

byte AGWMessage[1000];

struct AGWHeader AGWTXHeader;

char SessionList[100];

struct BPQConnectionInfo AGWConnections[65];

#define MaxSockets 64

static struct AGWSocketConnectionInfo Sockets[MaxSockets+1];

int CurrentConnections;

static int CurrentSockets=0;

int AGWPort = 0;
int AGWSessions = 0;
int	AGWMask = 0;

BOOL LoopMonFlag = FALSE;
BOOL Loopflag = FALSE;

extern char pgm[256];	

SOCKET agwsock;

extern BPQVECSTRUC * AGWMONVECPTR;

extern int SemHeldByAPI;

char szBuff[ 80 ];

BOOL Initialise();
int SetUpHostSessions();
int DisplaySessions();
int AGWDoStateChange(int Stream);
int AGWDoReceivedData(int Stream);
int AGWDoMonitorData();
int AGWConnected(struct BPQConnectionInfo * Con, int Stream);
int AGWDisconnected(struct BPQConnectionInfo * Con, int Stream);
int DeleteConnection(struct BPQConnectionInfo * Con);
int SendConMsgtoAppl(BOOL Incomming, struct BPQConnectionInfo * Con, char * CallSign);
int SendDisMsgtoAppl(char * Msg, struct AGWSocketConnectionInfo * sockptr);
int AGWSocket_Accept(int SocketId);
int Socket_Data(int SocketId,int error, int eventcode);
int AGWDataSocket_Read(struct AGWSocketConnectionInfo * sockptr, SOCKET sock);
int DataSocket_Write(struct AGWSocketConnectionInfo * sockptr, SOCKET sock);
int AGWGetSessionKey(char * key, struct AGWSocketConnectionInfo * sockptr);
int ProcessAGWCommand(struct AGWSocketConnectionInfo * sockptr);
int SendDataToAppl(int Stream, byte * Buffer, int Length);
int InternalAGWDecodeFrame(char * msg, char * buffer, int Stamp, int * FrameType);
int AGWDataSocket_Disconnect( struct AGWSocketConnectionInfo * sockptr);
int SendRawPacket(struct AGWSocketConnectionInfo * sockptr, char *txmsg, int Length);
int ShowApps();
int Terminate();
int SendtoSocket(SOCKET sock,char * Msg);


VOID Poll_AGW()
{
	int state, change, i;
	int Stream;
	struct BPQConnectionInfo * Con;
	struct AGWSocketConnectionInfo * sockptr;

	// Look for incoming connects

	fd_set readfd, writefd, exceptfd;
	struct timeval timeout;
	int retval;
	int n;
	int Active = 0;
	SOCKET maxsock;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;				// poll
		
	FD_ZERO(&readfd);

	FD_SET(agwsock, &readfd);

	retval = select(agwsock + 1, &readfd, NULL, NULL, &timeout);

	if (retval == -1)
	{
		retval = WSAGetLastError();
		perror("listen select");
	}

	if (retval)
		if (FD_ISSET(agwsock, &readfd))
			AGWSocket_Accept(agwsock);

	// look for data on any active sockets

	maxsock = 0;

	FD_ZERO(&readfd);
	FD_ZERO(&writefd);
	FD_ZERO(&exceptfd);

	// Check for data on active streams
	
	for (i = 0; i < CurrentConnections; i++)
	{
		Con = &AGWConnections[i];
		Stream = Con->BPQStream;

		SessionState(Stream, &state, &change);
	
		if (change == 1)
		{
			if (state == 1)
	
			// Connected
			
				AGWConnected(Con, Stream);	
			else
				AGWDisconnected(Con, Stream);
		}


		if (Con->SocketIndex)		// Active Session
		{
			AGWDoReceivedData(Stream);

			//	Get current Session State. Any state changed is ACK'ed
			//	automatically. See BPQHOST functions 4 and 5.
			
			SessionState(Stream, &state, &change);
	
			if (change == 1)
			{
				if (state == 1)
	
				// Connected
			
					AGWConnected(Con, Stream);	
				else
					AGWDisconnected(Con, Stream);
			}
		}
	}
	for (n = 1; n <= MaxSockets; n++)
	{
		sockptr=&Sockets[n];
		
		if (sockptr->SocketActive)
		{
			SOCKET sock = sockptr->socket;

			FD_SET(sock, &readfd);
			FD_SET(sock, &exceptfd);
				
			Active++;
			if (sock > maxsock)
				maxsock = sock;		
		}
	}

	if (Active)
	{
		retval = select(maxsock + 1, &readfd, &writefd, &exceptfd, &timeout);

		if (retval == -1)
		{				
			perror("data select");
			Debugprintf("Select Error %d", WSAGetLastError());
		}
		else
		{
			if (retval)
			{
				// see who has data

				for (n = 1; n <= MaxSockets; n++)
				{
					sockptr=&Sockets[n];
		
					if (sockptr->SocketActive)
					{
						SOCKET sock = sockptr->socket;

						if (FD_ISSET(sock, &exceptfd))
							AGWDataSocket_Disconnect(sockptr);

						if (FD_ISSET(sock, &readfd))
							AGWDataSocket_Read(sockptr, sock);

					}
				}
			}		
		}
	}
	
	AGWDoMonitorData();
}


SOCKADDR_IN local_sin;  /* Local socket - internet style */

PSOCKADDR_IN psin;


BOOL AGWAPIInit()
{	
	struct PORTCONTROL * PORT = PORTTABLE;
	int i = 1;
	char * ptr;
	BOOL opt=TRUE;

	if (AGWPort == 0)
		return FALSE;

//	Create listening socket

	agwsock = socket( AF_INET, SOCK_STREAM, 0);

    if (agwsock == INVALID_SOCKET)
	{
        sprintf(szBuff, "socket() failed error %d", WSAGetLastError());
		WritetoConsole(szBuff);
		return FALSE;        
	}

	setsockopt (agwsock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&opt,4);

	psin=&local_sin;

	psin->sin_family = AF_INET;
	psin->sin_addr.s_addr = INADDR_ANY;
    psin->sin_port = htons(AGWPort);        /* Convert to network ordering */

    if (bind(agwsock, (struct sockaddr FAR *) &local_sin, sizeof(local_sin)) == SOCKET_ERROR)
	{
        sprintf(szBuff, "bind(sock) failed Error %d", WSAGetLastError());
		WritetoConsole(szBuff);
        closesocket(agwsock);

		return FALSE;
	}

    if (listen(agwsock, 5) < 0)
	{
		sprintf(szBuff, "listen(sock) failed Error %d", WSAGetLastError());
		WritetoConsole(szBuff);

		return FALSE;
	} 

	SetUpHostSessions();

	// Set up port List

	ptr = &AGWPorts[0];

	ptr += sprintf(ptr, "%d", NUMBEROFPORTS);
				
	*(ptr)=';';
	*(++ptr)=0;

	while (PORT)
	{
		memcpy(ptr,"Port",4);
		ptr += sprintf(ptr, "%d", i);
		memcpy(ptr, " with ", 6);
		ptr+=6;
		memcpy(ptr, PORT->PORTDESCRIPTION, 29);		// ";"
		ptr+=29;
					
		while (*(--ptr) == ' ') {}

		ptr++;

		*(ptr++)=';';
		i++;
		PORT=PORT->PORTPOINTER;
	}

	*(ptr)=0;

	AGWMONVECPTR->HOSTAPPLFLAGS = 0x80;		// Requext Monitoring
          
	return TRUE;
}

int SetUpHostSessions()
{
	int Stream, i;

	if (AGWMask == 0) return 0;
	
	for (i = 1; i <= AGWSessions; i++)
	{ 
		strcpy(pgm, "AGW");

		Stream = FindFreeStream();
  
		if (Stream == 255) break;
    	
		SetAppl(Stream, 2, AGWMask);
    
		strcpy(pgm, "bpq32.exe");

		AGWConnections[CurrentConnections].CallKey[0] = 0;
		AGWConnections[CurrentConnections].BPQStream = Stream;
		AGWConnections[CurrentConnections].SocketIndex = 0;
		AGWConnections[CurrentConnections].Connecting = FALSE;
		AGWConnections[CurrentConnections].Listening = TRUE;
		AGWConnections[CurrentConnections].ApplMask = AGWMask;
          
		CurrentConnections++;
	}

	return 0;
}

extern struct DATAMESSAGE * REPLYBUFFER;
extern BOOL AGWActive;

VOID SHOWAGW(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	DISPLAY AGW Session Status
	
	int i, con;
	struct BPQConnectionInfo * Con;
	byte key[21];

	struct AGWSocketConnectionInfo * sockptr;
	char IPAddr[20];

	if (AGWActive == FALSE)
	{
		Bufferptr += sprintf(Bufferptr, "\rAGW Interface is not enabled\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Bufferptr += sprintf(Bufferptr, "\rSockets\r");
	for (i = 1; i <= CurrentSockets; i++)
	{
		sockptr=&Sockets[i];

		if (sockptr->SocketActive)
		{
			unsigned char work[4];
			memcpy(work, &sockptr->sin.sin_addr.s_addr, 4);
		
			sprintf(IPAddr, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

			Bufferptr = CHECKBUFFER(Session, Bufferptr);
			Bufferptr += sprintf(Bufferptr, "%2d   %-16s %5d %-10s\r", i, IPAddr, htons(sockptr->sin.sin_port), &sockptr->CallSign[0]);
		}
		else
		{
			Bufferptr = CHECKBUFFER(Session, Bufferptr);
			Bufferptr += sprintf(Bufferptr, "%2d   Idle\r", 1);
		}
	}
		
	Bufferptr += sprintf(Bufferptr, "\rPort    Calls        Stream Socket Connecting Listening Mask\r");

	for (con = 0; con < CurrentConnections; con++)
	{
		Con = &AGWConnections[con];

		memcpy(key,Con->CallKey,21);

		if (key[0] == 0) key[0] = 32;

		key[10]=0;
		key[20]=0;

		Bufferptr = CHECKBUFFER(Session, Bufferptr);
		Bufferptr += sprintf(Bufferptr, "%2c ", key[0]);
		Bufferptr += sprintf(Bufferptr, "%-10s%-10s ",&key[1],&key[11]);

		Bufferptr += sprintf(Bufferptr, "%2d     %2d      %s     %s    %X\r",
			Con->BPQStream,
			(Con->SocketIndex == 0) ? 0 : AGWConnections[con].SocketIndex->Number,
			(Con->Connecting == 0) ? "FALSE" : "TRUE ",
			(Con->Listening == 0) ? "FALSE" : "TRUE ",
			Con->ApplMask);

	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}



int DisplaySessions()
{
/*	char * ptr;
	int i, con;

	byte key[21]; // As String, char As String

	strcpy (SessionList,"   Port    Calls         Stream Socket Connecting Listening Mask");
  	
	SendDlgItemMessage(MainWnd,IDC_TEXTBOX3,LB_RESETCONTENT,0,0);
	SendDlgItemMessage(MainWnd,IDC_TEXTBOX3,LB_ADDSTRING,0,(LPARAM) SessionList);

	for (con = 0; con < CurrentConnections; con++)
	{
		memcpy(key,AGWConnections[con].CallKey,21);

		if (key[0] == 0) key[0] = 32;


		key[10]=0;
		key[20]=0;

		ptr=&SessionList[0];

		i=sprintf(ptr,"%2d %2c ",con,key[0]);

		ptr+=i;

		i=sprintf(ptr,"%-10s%-10s ",&key[1],&key[11]);

		ptr+=i;

		i=sprintf(ptr,"%2d     %2d      %s     %s    %x",
			AGWConnections[con].BPQStream,
			(AGWConnections[con].SocketIndex == 0) ? 0 : AGWConnections[con].SocketIndex->Number,
			(AGWConnections[con].Connecting == 0) ? "FALSE" : "TRUE ",
			(AGWConnections[con].Listening == 0) ? "FALSE" : "TRUE ",
			AGWConnections[con].ApplMask);

		SendDlgItemMessage(MainWnd,IDC_TEXTBOX3,LB_ADDSTRING,0,(LPARAM) SessionList);
	}
*/
	return (0);

}

int AGWConnected(struct BPQConnectionInfo * Con, int Stream)
{
	byte ConnectingCall[10];
	byte * ApplCallPtr;
	byte * keyptr;
	byte ApplCall[10]="";
	int i;

	int ApplNum;
	struct AGWSocketConnectionInfo * sockptr;

	GetCallsign(Stream, ConnectingCall);

	for (i=9;i>0;i--)
		if (ConnectingCall[i]==32)
			ConnectingCall[i]=0;

	ApplNum = GetApplNum(Stream);

	if (ApplNum == 0) 
	{
		return 0; // Cant be an incomming connect
	}
	ApplCallPtr = GetApplCall(ApplNum);

	if (ApplCallPtr != 0) memcpy(ApplCall,ApplCallPtr,10);

	// Convert trailing spaces to nulls
	
	for (i=9;i>0;i--)
		if (ApplCall[i]==32)
			ApplCall[i]=0;

//   See if incomming connection

	if (Con->Listening)
	{    
		//	Allocate Session and send "c" Message
		//
		//	Find an AGW session
                          
		for (sockptr=&Sockets[1]; sockptr <= &Sockets[CurrentSockets]; sockptr++)
		{
			if (sockptr->SocketActive && (memcmp(sockptr->CallSign,ApplCall,10) == 0))
			{
				// Create Key
            
				keyptr=(byte *)&Con->CallKey;

				*(keyptr++)='1';
				memcpy(keyptr,sockptr->CallSign, 10);
				keyptr+=10;
				memcpy(keyptr,ConnectingCall, 10);
                        		
				// Make sure key is not already in use

				for (i = 0; i < CurrentConnections; i++)
				{
					if (Con->BPQStream == Stream)
						continue;		// Dont Check ourself!

					if (AGWConnections[i].SocketIndex == sockptr &&
							memcmp(&Con->CallKey, &AGWConnections[i].CallKey,21) == 0)
					{
						SendMsg(Stream, "AGWtoBPQ - Callsign is already connected\r", 43);
						Sleep (500);
						Disconnect(Stream);
						Con->CallKey[0]=0;

						return 0;
					}
				}

				Con->Listening = FALSE;
				Con->SocketIndex = sockptr; 

				DisplaySessions();
                      
				SendConMsgtoAppl(TRUE, Con, ConnectingCall);

				return 0;
   			}
		}
			
		SendMsg(Stream, "No AGWPE Host Sessions available\r", 33);
		Sleep (500);
		Disconnect(Stream);   // disconnect
		return (0);
	}
	
	// Not listening ??

	OutputDebugString("Inbound Connection on Outgoing Stream");

	SendMsg(Stream, "AGWtoBPQ - Inbound Connection on Outgoing Stream\r", 49);
	Sleep (500);
	Disconnect(Stream);   // disconnect
	return (0);
}

int AGWDisconnected(struct BPQConnectionInfo * Con, int Stream)
{
	struct AGWSocketConnectionInfo * sockptr;	
	char key[21];

	memcpy(key,Con->CallKey,21);

	sockptr = Con->SocketIndex;
        
	if (sockptr != 0)
	{  
		AGWTXHeader.Port = key[0] - 49;
		
		memcpy(&AGWTXHeader.callfrom,&key[11],10);
		memcpy(&AGWTXHeader.callto,&key[1],10);

		//   Send a "d" Message
        
		// DisMsg = "*** DISCONNECTED From Station "

 		SendDisMsgtoAppl("*** DISCONNECTED From Station ", sockptr);
		
	}

	if (Con->ApplMask != 0)
	{
		Con->Listening = TRUE;
		Con->SocketIndex = 0;
		memset(&Con->CallKey ,0 ,21);
            
		DisplaySessions();
	}
	else
	{
		DeleteConnection(Con);
	}
		
	return 0;

}
int AGWDoReceivedData(int Stream)
{
	byte Buffer[400];
	int len,count;

//Dim n As Integer, i As Integer, j As Integer, portcount As Integer
//Dim start As Integer

	do
	{ 
		GetMsg(Stream, Buffer,&len, &count);

		if (len > 0)
			SendDataToAppl(Stream, Buffer,len);
    
	}
	while (count > 0);
	
	return 0;
}

int AGWDoMonitorData()
{
	byte Buffer[500];
	int RawLen, Length;
	byte Port;
	struct AGWSocketConnectionInfo * sockptr;	
	byte AGWBuffer[1000];
	int n;
	int Stamp, Frametype;
	BOOL RXFlag, NeedAGW;

	// Look for Monitor Data

	while (AGWMONVECPTR->HOSTTRACEQ)
	{
		MESSAGE * monbuff;

		GetSemaphore(&Semaphore, 99);
		
		monbuff = Q_REM((UINT *)&AGWMONVECPTR->HOSTTRACEQ);

		RawLen = monbuff->LENGTH;

		if (RawLen < 7 || RawLen > 350)
		{	
			ReleaseBuffer(monbuff);
			FreeSemaphore(&Semaphore);
			return 0;
		}

		Stamp = (UINT)monbuff->Timestamp;

		memcpy(Buffer, monbuff, RawLen);

		ReleaseBuffer(monbuff);

		FreeSemaphore(&Semaphore);
	
//'   4 byte chain
//'   1 byte port - top bit = transmit
//'   2 byte length (LO-HI)

		Port = Buffer[4];

		if (Port > 127)
		{
			RXFlag = FALSE;
			Port = Port - 128;
		}
		else
		{
		    RXFlag = TRUE;
		}

		NeedAGW = FALSE;

		for (n = 1; n<= CurrentSockets; n++)
		{
			sockptr=&Sockets[n];

			if (sockptr->SocketActive && sockptr->MonFlag) NeedAGW = TRUE;
		}

		if (NeedAGW)
		{
			if (RXFlag || LoopMonFlag)    // only send txed frames if requested
			{
				Length = InternalAGWDecodeFrame(Buffer, AGWBuffer,Stamp, &Frametype);
	 
				//
				//   Decode frame and send to applications which have requested monitoring
				//
				if (Length > 0)
				{
					AGWTXHeader.Port = Port - 1;       // AGW Ports start from 0
        
					if (Frametype == 3)
					{
						AGWTXHeader.DataKind = 'U';
					}
					else
					{
						if (Frametype && 1 == 0)
						{
							AGWTXHeader.DataKind = 'I';
						}
						else
						{
							AGWTXHeader.DataKind = 'S';
						}
					}

                      
           /* For i = 8 To 17
        
                cChar = Asc(Mid(AGWBuffer, i - 1))
            
                If cChar = 32 Then Exit For
            
                AGWTXHeader(i) = cChar
            
            Next i
        
            j = i + 3
        
            For i = 18 To 27
        
                cChar = Asc(Mid(AGWBuffer, j))
            
                If cChar = 32 Then Exit For
            
                AGWTXHeader(i) = cChar
            
                j = j + 1
            
            Next i

        */

					AGWTXHeader.DataLength = Length;

				    memset(AGWTXHeader.callfrom, 0,10);
					ConvFromAX25(monbuff->ORIGIN, AGWTXHeader.callfrom);
       
	 				for (n = 1; n<= CurrentSockets; n++)
					{
						sockptr=&Sockets[n];
        
						if (sockptr->SocketActive && sockptr->MonFlag)
							SendRawPacket(sockptr, AGWBuffer, Length);
					}
				}
			}
		}

		RawLen = RawLen - 6;
 
		if (RXFlag || Loopflag) // Send transmitted frames if requested
		{

        //
        //  Send raw data to any sockets that have requested Raw frames
        //
        
			Buffer[6]=0;
       
			AGWTXHeader.Port = Port - 1;       // AGW Ports start from 0
			AGWTXHeader.DataKind = 'K';
        
			AGWTXHeader.DataLength = RawLen;
         
	 		for (n = 1; n<= CurrentSockets; n++)
			{
				sockptr=&Sockets[n];
       
				if (sockptr->SocketActive && sockptr->RawFlag)
					SendRawPacket(sockptr, &Buffer[6], RawLen);
        
			}
		}		
   }

	return 0;

}

int DeleteConnection(struct BPQConnectionInfo * Con)
{
	int i;
	int con;

	//
	//	remove specified session
	//

    SetAppl(Con->BPQStream, 0, 0);

	Disconnect(Con->BPQStream);
 
    DeallocateStream(Con->BPQStream);

//   move all down one

	con = (Con - &AGWConnections[0]) / sizeof(struct BPQConnectionInfo);

	for (i = con; i <= CurrentConnections - 2; i++)
	{
	    memcpy(&AGWConnections[i],&AGWConnections[i + 1],sizeof ConInfoRec);  
	}
    
	CurrentConnections--;

	return 0;
}

int SendConMsgtoAppl(BOOL Incomming, struct BPQConnectionInfo * Con, char * CallSign)
{
	char key[21];
	char ConMsg[80]="*** CONNECTED ";
	struct AGWSocketConnectionInfo * sockptr;


    memcpy(key,&Con->CallKey,21);
        
    sockptr = Con->SocketIndex;
        
    AGWTXHeader.Port = key[0] - 49;
            
    memcpy(AGWTXHeader.callfrom, &key[11],10);
        
    memcpy(AGWTXHeader.callto, &key[1],10);
            
/*    '
    '   Send a "C" Message
    '
'01 00 00 00 43 00 00 00 47 4D 38 42 50 51 2D 34    C   GM8BPQ-4
'00 EA 47 4D 38 42 50 51 2D 34 00 FF 25 00 00 00  êGM8BPQ-4 ÿ%
'00 00 00 00 2A 2A 2A 20 43 4F 4E 4E 45 43 54 45     *** CONNECTE
'44 20 57 69 74 68 20 53 74 61 74 69 6F 6E 20 47 D With Station G
'4D 38 42 50 51 2D 34 0D 00 M8BPQ-4
*/

    AGWTXHeader.DataKind = 'C';

    AGWTXHeader.PID = 0;
                    
    if (Incomming)	
		strcat(ConMsg,"To");
	else
		strcat(ConMsg,"With");
    
    strcat(ConMsg," Station ");
    strcat(ConMsg,CallSign);
                
    AGWTXHeader.DataLength = strlen(ConMsg)+1;
               
	SendtoSocket(sockptr->socket, ConMsg);
            
	return 0;

}




int SendDisMsgtoAppl(char * Msg, struct AGWSocketConnectionInfo * sockptr)
{
    byte DisMsg[100];
            
    strcpy(DisMsg,Msg);
	strcat(DisMsg,(const char *)&AGWTXHeader.callfrom);
            
    AGWTXHeader.Port = sockptr->AGWRXHeader.Port;
    AGWTXHeader.DataKind = 'd';
 
    strcat(DisMsg,"\r\0");
                                
    AGWTXHeader.DataLength = strlen(DisMsg)+1;
            
	SendtoSocket(sockptr->socket, DisMsg);

	return 0;

}



int AGWSocket_Accept(int SocketId)
{
	int n,addrlen;
	struct AGWSocketConnectionInfo * sockptr;
	SOCKET sock;

//   Find a free Socket

	for (n = 1; n <= MaxSockets; n++)
	{
		sockptr=&Sockets[n];
		
		if (sockptr->SocketActive == FALSE)
		{
			addrlen=sizeof(struct sockaddr);

			sock = accept(SocketId, (struct sockaddr *)&sockptr->sin, &addrlen);

			if (sock == INVALID_SOCKET)
			{
				sprintf(szBuff, "AGW accept() failed Error %d\r", WSAGetLastError());
				WritetoConsole(szBuff);
				return FALSE;
			}

			sockptr->socket = sock;
			sockptr->SocketActive = TRUE;
			sockptr->GotHeader = FALSE;
			sockptr->MsgDataLength = 0;
			sockptr->Number = n;

			if (CurrentSockets < n) CurrentSockets=n;  //Record max used to save searching all entries

			ShowApps();

			return 0;
		}
	}

	// Should accept, then immediately close

	return 0;
}

int SendtoSocket(SOCKET sock,char * Msg)
{
	int len;
	
	len=AGWTXHeader.DataLength;
	
	send(sock,(char *)&AGWTXHeader, 36,0);
	if (len > 0) send(sock, Msg, len,0);

	return 0;
}



int AGWDataSocket_Read(struct AGWSocketConnectionInfo * sockptr, SOCKET sock)
{
	int i;
	int DataLength;

	ioctlsocket(sock,FIONREAD,&DataLength);

	if (DataLength == SOCKET_ERROR || DataLength == 0)
	{
		// Failed or closed - clear connection

		AGWDataSocket_Disconnect(sockptr);
		return 0;
	}


	if (sockptr->GotHeader)
	{
		// Received a header, without sufficient data bytes
   
		if (DataLength < sockptr->MsgDataLength)
		{
			// Fiddle - seem to be problems somtimes with un-Neagled hosts
        
			Sleep(500);

			ioctlsocket(sock,FIONREAD,&DataLength);
		}
		
		if (DataLength >= sockptr->MsgDataLength)
		{
			//   Read Data and Process Command
    
			i=recv(sock, AGWMessage, sockptr->MsgDataLength, 0);

			ProcessAGWCommand (sockptr);
        
			sockptr->GotHeader = FALSE;
		}

		// Not Enough Data - wait

	}
	else	// Not got header
	{
		if (DataLength > 35)//         A header
		{
			i=recv(sock,(char *)&sockptr->AGWRXHeader, 36, 0);
            
			if (i == SOCKET_ERROR)
			{
				i=WSAGetLastError();

				AGWDataSocket_Disconnect(sockptr);
			}

			
			sockptr->MsgDataLength = sockptr->AGWRXHeader.DataLength;

			if (sockptr->MsgDataLength > 500)
				OutputDebugString("Corrupt AGW message");

            
		    if (sockptr->MsgDataLength == 0)
			{
				ProcessAGWCommand (sockptr);
			}
			else
			{
				sockptr->GotHeader = TRUE;            // Wait for data
			}

		} 
		
		// not got 36 bytes

	}
	
	return 0;
}


int ProcessAGWCommand(struct AGWSocketConnectionInfo * sockptr)
{
	int AGWVersion[2]={2003,999};
	char AGWRegReply[1];
	struct BPQConnectionInfo * Connection;
	int Stream;
	char AXCall[10];
	unsigned char TXMessage[500];
	int Digis,MsgStart,j;
	byte * TXMessageptr;
	char key[21];
	char ToCall[10];
	char ConnectMsg[20];
	int con,conport;
	int AGWYReply = 0;

	switch (sockptr->AGWRXHeader.DataKind)
	{
	case 'C':

        //   Connect
        
        //   Create Session Key from port and callsign pair

		AGWGetSessionKey(key, sockptr);
            
        memcpy(ToCall, &key[11],10);

		strcpy(pgm, "AGW");
           
        Stream = FindFreeStream();

		strcpy(pgm, "bpq32.exe");

        if (Stream == 255) return 0;

		Connection=&AGWConnections[CurrentConnections];

        memcpy(&Connection->CallKey,key,21);
        Connection->BPQStream = Stream;
        Connection->SocketIndex = sockptr;
        Connection->Connecting = TRUE;
        
        Connect(Stream);				// Connect
        
		ConvToAX25(sockptr->CallSign, AXCall);
		ChangeSessionCallsign(Stream, AXCall);

        DisplaySessions();
        
        if (memcmp(ToCall,"SWITCH",6) == 0)
		{
			//  Just connect to command level on switch
            
            SendConMsgtoAppl(FALSE, Connection, ToCall);
            Connection->Connecting = FALSE;
		} 
        else
		{
 
			// Need to convert port index (used by AGW) to port number

			conport=GetPortNumber(key[0]-48);

			sprintf(ConnectMsg,"C %d %s\r",conport,ToCall);
            SendMsg(Stream, ConnectMsg, strlen(ConnectMsg));

		}

        CurrentConnections++;

        DisplaySessions();

		return 0;
        
	case 'D':
   
        //   Send Data
        //
        //   Create Session Key from port and callsign pair
		
        AGWGetSessionKey(key, sockptr);

        for (con = 0; con < CurrentConnections; con++)
		{
			if (memcmp(AGWConnections[con].CallKey,key,21) == 0)
			{
				SendMsg(AGWConnections[con].BPQStream, AGWMessage, sockptr->MsgDataLength);
				return 0;
			}
		}

		return 0;
  
	case 'd':

    //   Disconnect
            
        memcpy(AGWTXHeader.callto,sockptr->AGWRXHeader.callfrom,10);
        memcpy(AGWTXHeader.callfrom,sockptr->AGWRXHeader.callto,10);
        
        SendDisMsgtoAppl("*** DISCONNECTED RETRYOUT With ", sockptr);
   
        AGWGetSessionKey(key, sockptr);
 
        for (con = 0; con < CurrentConnections; con++)
		{
			if (memcmp(AGWConnections[con].CallKey,key,21) == 0)
			{ 
                Disconnect(AGWConnections[con].BPQStream);
				return 0;
			}
		}

		// There is confusion about the correct ordring of calls in the "d" packet. AGW appears to accept either,
		//	so I will too.

		memset(&key[1],0,20);
		strcpy(&key[1],sockptr->AGWRXHeader.callto);
		strcpy(&key[11],sockptr->AGWRXHeader.callfrom);

		for (con = 0; con < CurrentConnections; con++)
		{
			if (memcmp(AGWConnections[con].CallKey,key,21) == 0)
			{ 
                Disconnect(AGWConnections[con].BPQStream);
				return 0;
			}
		}

		return 0;


	case 'R':
    
    //   Version
    
        memset(&AGWTXHeader,0,36);
    
        AGWTXHeader.DataKind = 'R';

        AGWTXHeader.DataLength = 8;       // Length
    
        SendtoSocket(sockptr->socket, (char *)&AGWVersion[0]);

		return 0;
    

	case 'G':

        //   Port info. String is in AGWPorts
        
        
        memset(&AGWTXHeader,0,36);

        AGWTXHeader.DataKind = 'G';

        AGWTXHeader.DataLength = strlen(AGWPorts)+1;     // Length
    
        SendtoSocket(sockptr->socket, AGWPorts);

		return 0;

    
	case 'k':

       //   Toggle Raw receive

        sockptr->RawFlag = !sockptr->RawFlag;
        
		return 0;

	case 'K':

        // Send Raw Frame
 
		SendRaw(sockptr->AGWRXHeader.Port+1,&AGWMessage[1], sockptr->MsgDataLength - 1);
        return 0;


	case 'm':
     
       //   Toggle Monitor receive
    
        sockptr->MonFlag = !sockptr->MonFlag;
		return 0;
    
  
	case 'M':
	case 'V':         // Send UNProto Frame "V" includes Via string
  
   
        ConvToAX25(sockptr->AGWRXHeader.callto,TXMessage);
		ConvToAX25(sockptr->AGWRXHeader.callfrom,&TXMessage[7]);

		Digis=0;
        MsgStart = 0;

        if (sockptr->AGWRXHeader.DataKind == 'V')	// Unproto with VIA string
		{        
            Digis = AGWMessage[0];                 // Number of digis
                    
			for (j = 1; j<= Digis; j++)
			{
				ConvToAX25(&AGWMessage[(j - 1) * 10 + 1],&TXMessage[7+(j*7)]);      // No "last" bit
			}

			// set end of call 

           MsgStart = Digis * 10 + 1;                // UI Data follows digis in message
 		}
   
		TXMessageptr=&TXMessage[13+(Digis*7)];

		*(TXMessageptr++) |= 1;		// set last bit
        
		*(TXMessageptr++) = 3;     // UI

        if (sockptr->AGWRXHeader.PID == 0)

            *(TXMessageptr++) = 240;		 // PID
		else
            *(TXMessageptr++) = sockptr->AGWRXHeader.PID; 
   
        memcpy(TXMessageptr,&AGWMessage[MsgStart], sockptr->MsgDataLength - MsgStart);
        
		TXMessageptr += (sockptr->MsgDataLength - MsgStart);

        SendRaw(sockptr->AGWRXHeader.Port + 1, TXMessage, TXMessageptr-&TXMessage[0]);

		return 0;

	case 'X':
 
        //   Register Callsign
        
		memset(&AGWTXHeader,0,36);
		
		memset(&sockptr->CallSign,0,10);
	  
		memcpy(sockptr->CallSign, sockptr->AGWRXHeader.callfrom,strlen(sockptr->AGWRXHeader.callfrom));  
        AGWTXHeader.DataKind = 'X';
        
        AGWTXHeader.DataLength = 1;      // Length
             
        AGWRegReply[0] = 1;

        SendtoSocket(sockptr->socket, AGWRegReply);

		ShowApps();


		return 0;

   
	case 'Y':
    
        //   Session Status
        
        //   Create Session Key from port and callsign pair
        
        AGWGetSessionKey(key, sockptr);

        for (con = 0; con < CurrentConnections; con++)
		{
			if (memcmp(AGWConnections[con].CallKey,key,21) == 0)
			{
				memcpy(&AGWTXHeader,&sockptr->AGWRXHeader,36);

				AGWYReply = CountFramesQueuedOnStream(AGWConnections[con].BPQStream);
				AGWTXHeader.DataLength = 4;      // Length
				SendtoSocket(sockptr->socket, (char *)&AGWYReply);

				return 0;
			}
		}

		return 0;


	default:
    
        //If Debugging Then Print #10, "Unknown Message "; Chr$(Sockets(Index).AGWRXHeader(4))
       // Debug.Print "Unknown Message "; Chr$(Sockets(Index).AGWRXHeader(4))
        
		return 0;

		}

	return 0;
   
}

int AGWGetSessionKey(char * key, struct AGWSocketConnectionInfo * sockptr)
{

//   Create Session Key from port and callsign pair
        
  

	key[0] = sockptr->AGWRXHeader.Port + '1'; 
        
	memset(&key[1],0,20);
	strcpy(&key[1],sockptr->AGWRXHeader.callfrom);
	strcpy(&key[11],sockptr->AGWRXHeader.callto);

	return 0;

}
int SendDataToAppl(int Stream, byte * Buffer, int Length)
{
	int con;
	char * i;
	char ConMsg[80];
	char DisMsg[80];
	char key[21];
	struct AGWSocketConnectionInfo * sockptr;

//Dim i As Long, Length As Long, con As Long, key As String, hilen As Long, lolen As Long
//Dim Index As Integer, ConMsg As String, DisMsg As String
//Dim BytesSent As Long


	//'   Find Connection number and call pair

	for (con = 0; con < CurrentConnections; con++)
	{
		if (AGWConnections[con].BPQStream == Stream)
		{
			memcpy(key,&AGWConnections[con].CallKey,21);

			if (key[0] == 32)
			{
				//Debug.Print "Data on Unconnected Session"

				Disconnect(Stream);
				return (0);
			}
        
			sockptr = AGWConnections[con].SocketIndex;
			
			if (sockptr == 0)
			{
				// No connection, but have a key - wot's going on!!
				// Probably best to clear out connection

				Disconnect(Stream);

				return (0);
			}

			AGWTXHeader.Port = key[0] - 49;

			memcpy(AGWTXHeader.callfrom,&key[11],10);
			memcpy(AGWTXHeader.callto,&key[1],10);

			if (AGWConnections[con].Connecting)
			{

            //   See if *** Connected message

				i = strstr(Buffer, "Connected to");
            
				if (i != 0)
				{  
					AGWConnections[con].Connecting = FALSE;
                
					DisplaySessions();

					AGWTXHeader.DataKind = 'C';
	                AGWTXHeader.PID = 0;
                
		            strcpy(ConMsg,"*** CONNECTED With Station ");
					strcat(ConMsg, AGWTXHeader.callfrom);
					strcat(ConMsg,"\0");
                
					AGWTXHeader.DataLength = strlen(ConMsg)+1;
           
					SendtoSocket(sockptr->socket, ConMsg);
            
					return (0);

				}
            
				i = strstr(Buffer, "Failure with");
            
				if (i != 0)
				{   
					AGWConnections[con].Connecting = FALSE;
                               
					strcpy(DisMsg,"*** DISCONNECTED RETRYOUT With ");
        
					SendDisMsgtoAppl(DisMsg, sockptr);

					DeleteConnection(&AGWConnections[con]);
                
		            return 0;

				}
				
				i = strstr(Buffer, "Busy from");
            
				if (i != 0)
				{   
					AGWConnections[con].Connecting = FALSE;
                               
					strcpy(DisMsg,"*** DISCONNECTED RETRYOUT With ");
        
					SendDisMsgtoAppl(DisMsg, sockptr);
					
					DeleteConnection(&AGWConnections[con]);
                
		            return 0;

				}
			}

	        AGWTXHeader.DataKind = 'D';
		    AGWTXHeader.PID = 0xF0;
			AGWTXHeader.DataLength = Length;              
        
			SendtoSocket(sockptr->socket, Buffer);
		}
	}

	return 0;
 }


int AGWDataSocket_Disconnect(struct AGWSocketConnectionInfo * sockptr)
{
	int con;

	closesocket(sockptr->socket);

	for (con = 0; con < CurrentConnections; con++)
	{
		if (AGWConnections[con].SocketIndex == sockptr)
			Disconnect(AGWConnections[con].BPQStream);
	}

	sockptr->SocketActive = FALSE;
	sockptr->RawFlag = FALSE;
	sockptr->MonFlag = FALSE;
	
	ShowApps();


	return 0;
}

int SendRawPacket(struct AGWSocketConnectionInfo * sockptr, char *txmsg, int Length)
{
	SendtoSocket(sockptr->socket, txmsg);

	return 0;
}

int ShowApps()
{
/*
	struct AGWSocketConnectionInfo * sockptr;
	int i;
	char Msg[80];
	char IPAddr[20];

	if (ConnWnd == 0) return 0; // Not on display
	
	SendDlgItemMessage(ConnWnd,IDC_CONNECTIONS_LIST,LB_RESETCONTENT,0,0);

	for (i = 1; i <= CurrentSockets; i++)
	{
		sockptr=&Sockets[i];

		if (sockptr->SocketActive)
		{
			sprintf(IPAddr,"%d.%d.%d.%d",
				sockptr->sin.sin_addr.S_un.S_un_b.s_b1,
				sockptr->sin.sin_addr.S_un.S_un_b.s_b2,
				sockptr->sin.sin_addr.S_un.S_un_b.s_b3,
				sockptr->sin.sin_addr.S_un.S_un_b.s_b4);

			sprintf(Msg,"%2d   %-16s %5d %-10s",i,IPAddr,htons(sockptr->sin.sin_port),&sockptr->CallSign);
		}
		else
		{
			sprintf(Msg,"%2d   Idle",i);
		}

		SendDlgItemMessage(ConnWnd,IDC_CONNECTIONS_LIST,LB_ADDSTRING,0,(LPARAM) Msg);
	}
*/
	return 0;
}


int LocalSessionState(int stream, int * state, int * change, BOOL ACK);

int AGWAPITerminate()
{
	int con, State, Change, n;
	struct BPQConnectionInfo * Connection;
	struct AGWSocketConnectionInfo * sockptr;
//
//   Release all streams
//
	for (con = 0; con < CurrentConnections; con++)
	{
		Connection=&AGWConnections[con];

        SetAppl(Connection->BPQStream, 0, 0);

        Disconnect(Connection->BPQStream);

        DeallocateStream(Connection->BPQStream);
    
        LocalSessionState(Connection->BPQStream, &State, &Change, TRUE);

		memset(Connection, 0, sizeof(struct AGWSocketConnectionInfo));
		    
	}

	CurrentConnections = 0;

	// Close Listening socket and any connections
	
	shutdown(agwsock, 2);
	closesocket(agwsock);

	for (n = 1; n <= MaxSockets; n++)
	{
		sockptr=&Sockets[n];
		
		if (sockptr->SocketActive)
		{
			SOCKET sock = sockptr->socket;

			shutdown(sock, 2);
			closesocket(sock);
		}
		memset(sockptr, 0, sizeof(struct BPQConnectionInfo));
	}

	return 0;
}

