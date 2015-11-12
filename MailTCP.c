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

// Mail and Chat Server for BPQ32 Packet Switch
//
//	TCP access module - POP and SMTP

#include "BPQMail.h"

VOID ReleaseSock(SOCKET sock);

#define MaxSockets 64

SocketConn * Sockets = NULL;

int CurrentConnections;

int CurrentSockets=0;

#define MAX_PENDING_CONNECTS 4

#define VERSION_MAJOR         2
#define VERSION_MINOR         0

SOCKADDR_IN local_sin;  /* Local socket - internet style */

PSOCKADDR_IN psin;

SOCKET smtpsock, pop3sock;

char szBuff[80];

int SMTPInPort;
int POP3InPort;

BOOL RemoteEmail;			// Set to listen on INADDR_ANY rather than LOCALHOST

BOOL ISP_Gateway_Enabled;

char MyDomain[50];			// Mail domain for BBS<>Internet Mapping

char ISPSMTPName[50];
int ISPSMTPPort;

char ISPPOP3Name[50];
int ISPPOP3Port;

char ISPAccountName[50];
char ISPAccountPass[50];
char EncryptedISPAccountPass[100];
int EncryptedPassLen;

BOOL SMTPAuthNeeded;

BOOL GMailMode = FALSE;
char GMailName[50];

int POP3Timer=9999;							// Run on startup
int ISPPOP3Interval;

BOOL SMTPMsgCreated=FALSE;					// Set to cause SMTP client to send messages to ISP
BOOL SMTPActive=FALSE;						// SO we don't try every 10 secs!

char mycd64[256];
static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

//char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
//char *dat[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

void decodeblock( unsigned char in[4], unsigned char out[3] );
VOID FormatTime(char * Time, time_t cTime);

int SendSock(SocketConn * sockptr, char * msg)
{
	int len = strlen(msg), sent;
	char * newmsg = malloc(len+10);

 	WriteLogLine(NULL, '>',msg,  len, LOG_TCP);

	strcpy(newmsg, msg);

	strcat(newmsg, "\r\n");

	len+=2;

	if (sockptr->SendBuffer)
	{
		// Already queued, so add to end

		if ((sockptr->SendSize + len) > sockptr->SendBufferSize)
		{
			sockptr->SendBufferSize += (10000 + len);
			sockptr->SendBuffer = realloc(sockptr->SendBuffer, sockptr->SendBufferSize);
		}

		memcpy(&sockptr->SendBuffer[sockptr->SendSize], newmsg, len);
		sockptr->SendSize += len;
		free (newmsg);
		return len;
	}
	
	sent = send(sockptr->socket, newmsg, len, 0);
		
	if (sent < len)
	{
		int error, remains;

		// Not all could be sent - queue rest

		if (sent == SOCKET_ERROR)
		{
			error = WSAGetLastError();
			if (error == WSAEWOULDBLOCK)
				sent=0;

			//	What else??
		}

		remains = len - sent;

		sockptr->SendBufferSize += (10000 + remains);
		sockptr->SendBuffer = malloc(sockptr->SendBufferSize);

		memcpy(sockptr->SendBuffer, &newmsg[sent], remains);

		sockptr->SendSize = remains;
		sockptr->SendPtr = 0;

	}

	free (newmsg);

	return sent;
}

VOID __cdecl sockprintf(SocketConn * sockptr, const char * format, ...)
{
	// printf to a socket

	char buff[1000];
	va_list(arglist);
	
	va_start(arglist, format);
	vsprintf(buff, format, arglist);

	SendSock(sockptr, buff);
}

extern SMTPMsgs;

fd_set ListenSet;
SOCKET ListenMax = 0;

extern SOCKET nntpsock;

int NNTP_Read(SocketConn * sockptr, SOCKET sock);

VOID SetupListenSet()
{
	// Set up master set of fd's for checking for incoming calls

	fd_set * readfd = &ListenSet;
	SOCKET sock;

	FD_ZERO(readfd);

	sock = nntpsock;	
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > ListenMax)
			ListenMax = sock;
	}

	sock = smtpsock;
	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > ListenMax)
			ListenMax = sock;
	}
		
	sock = pop3sock;

	if (sock)
	{
		FD_SET(sock, readfd);
		if (sock > ListenMax)
			ListenMax = sock;
	}
}

VOID Socket_Connected(SocketConn * sockptr, int error)
{
	SOCKET sock = sockptr->socket;

	if (error)
	{
		Logprintf(LOG_TCP, NULL, '|', "Connect Failed");

		if (sockptr->Type == SMTPClient)
			SMTPActive = FALSE;

		ReleaseSock(sock);

		return;
	}
	
	sockptr->State = WaitingForGreeting;
	
	if (sockptr->Type == NNTPServer)
		SendSock(sockptr, "200 BPQMail NNTP Server ready");	

	else if (sockptr->Type == SMTPServer)
		SendSock(sockptr, "220 BPQMail SMTP Server ready");
	
	else if (sockptr->Type == POP3SLAVE)
	{
		SendSock(sockptr, "+OK POP3 server ready");
		sockptr->State = GettingUser;
	}	
}

VOID TCPFastTimer()
{
	//	we now poll for incoming connections and data

	fd_set readfd, writefd, exceptfd;
	struct timeval timeout;
	int retval;
	SocketConn * sockptr = Sockets;
	SOCKET sock;
	int Active = 0;
	SOCKET maxsock;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;				// poll

	if (ListenMax)
	{	
		memcpy(&readfd, &ListenSet, sizeof(fd_set));

		retval = select(ListenMax + 1, &readfd, NULL, NULL, &timeout);

		if (retval == -1)
		{
			retval = 0;
			perror("Listen select");
		}

		if (retval)
		{
			sock = pop3sock;
			if (sock)
				if (FD_ISSET(sock, &readfd))
					Socket_Accept(sock);

			sock = smtpsock;
			if (sock)
				if (FD_ISSET(sock, &readfd))
					Socket_Accept(sock);
		
			sock = nntpsock;
			if (sock)
				if (FD_ISSET(sock, &readfd))
					NNTP_Accept(sock);
		}
	}

	// look for data on any active sockets

	maxsock = 0;

	FD_ZERO(&readfd);
	FD_ZERO(&writefd);
	FD_ZERO(&exceptfd);
	sockptr=Sockets;
		
	while (sockptr)
	{		
		if (sockptr->State & Connecting)
		{
			// look for complete or failed

			FD_SET(sockptr->socket, &writefd);
			FD_SET(sockptr->socket, &exceptfd);
		}
		else
			FD_SET(sockptr->socket, &readfd);

		Active++;

		if (sockptr->socket > maxsock)
			maxsock = sockptr->socket;
		
		sockptr = sockptr->Next;
	}

	if (Active == 0)
		return;

	retval = select(maxsock + 1, &readfd, &writefd, &exceptfd, &timeout);

	if (retval == -1)
		perror("select");
	else
	{
		if (retval)
		{
			sockptr = Sockets;

			// see who has data

			while (sockptr)
			{		
				sock = sockptr->socket;
			
				if (FD_ISSET(sock, &readfd))
				{
					if (sockptr->Type == NNTPServer)
					{
						if (NNTP_Read(sockptr, sock) == 0)
							break;						// We've messed with the chain
					}
					else
					{
						if (DataSocket_Read(sockptr, sock) == 0)
							break;						// We've messed with the chain
					}
				}
				if (FD_ISSET(sockptr->socket, &writefd))
					Socket_Connected(sockptr, 0);

				if (FD_ISSET(sockptr->socket, &exceptfd))
				{
					Socket_Connected(sockptr, 1);
					return;
				}
				sockptr = sockptr->Next;
			}			
		}
	}
}

VOID TCPTimer()
{
	POP3Timer+=10;

	if (POP3Timer > ISPPOP3Interval)			// 5 mins
	{
		POP3Timer=0;

		if ((ISPSMTPPort && ISP_Gateway_Enabled))
			SendtoISP();
		
		if (ISPPOP3Port  && ISP_Gateway_Enabled)
			POP3Connect(ISPPOP3Name, ISPPOP3Port);

		if (SMTPMsgs && ISPSMTPPort && ISP_Gateway_Enabled)
			SendtoISP();
	}
	else
	{
		if (SMTPMsgCreated && ISPSMTPPort && ISP_Gateway_Enabled)
			SendtoISP();
	}
}
BOOL InitialiseTCP()
{
	int			  Error;              // catches return value of WSAStartup
#ifdef	WIN32
	WORD          VersionRequested;   // passed to WSAStartup
    WSADATA       WsaData;            // receives data from WSAStartup
#endif
	int i,j;


	for (i=0;i<64; i++)
	{
		j=cb64[i];
		mycd64[j]=i;
	}

#ifdef WIN32
	
	VersionRequested = MAKEWORD(VERSION_MAJOR, VERSION_MINOR);

	Error = WSAStartup(VersionRequested, &WsaData);
    
	if (Error)
	{
#ifndef LINBPQ
		MessageBox(NULL,
            "Could not find high enough version of WinSock",
            "BPQMailChat", MB_OK | MB_ICONSTOP | MB_SETFOREGROUND);
#else
		printf("Could not find high enough version of WinSock\n");
#endif
		return FALSE;
	}

#endif

//	Create listening sockets


	if (SMTPInPort)
		smtpsock = CreateListeningSocket(SMTPInPort);

	if (POP3InPort)
		pop3sock = CreateListeningSocket(POP3InPort);

	if (ISP_Gateway_Enabled)
	{
		// See if using GMail

		char * ptr = strchr(ISPAccountName, '@');

		if (ptr)
		{
			if (_stricmp(&ptr[1], "gmail.com") == 0 || _stricmp(&ptr[1], "googlemail.com") == 0)
			{
				strcpy(GMailName, ISPAccountName);
				strlop(GMailName, '@');
				GMailMode = TRUE;
				SMTPAuthNeeded = TRUE;
			}
		}
	}

	return TRUE;

}


SOCKET CreateListeningSocket(int Port)
{
	SOCKET sock;
	unsigned int param = 1;
	
	sock = socket( AF_INET, SOCK_STREAM, 0);

    if (sock == INVALID_SOCKET)
	{
        sprintf(szBuff, "socket() failed error %d", WSAGetLastError());
#ifdef LINBPQ
		perror(szBuff);
#else
		MessageBox(MainWnd, szBuff, "BPQMailChat", MB_OK);
#endif
		return FALSE;
	}

	setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (char *)&param,4);

	psin=&local_sin;

	psin->sin_family = AF_INET;
	psin->sin_addr.s_addr = htonl(RemoteEmail ? INADDR_ANY  : INADDR_LOOPBACK);	// Local Host Olny
	
	psin->sin_port = htons(Port);        /* Convert to network ordering */

    if (bind( sock, (struct sockaddr FAR *) &local_sin, sizeof(local_sin)) == SOCKET_ERROR)
	{
         sprintf(szBuff, "bind(%d) failed Error %d", Port, WSAGetLastError());
#ifdef LINBPQ
		perror(szBuff);
#else
		MessageBox(MainWnd, szBuff, "BPQMailChat", MB_OK);
#endif
         closesocket( sock );
		 return FALSE;
	}

    if (listen( sock, MAX_PENDING_CONNECTS ) < 0)
	{
		sprintf(szBuff, "listen(%d) failed Error %d", Port, WSAGetLastError());
#ifdef LINBPQ
		perror(szBuff);
#else
		MessageBox(MainWnd, szBuff, "BPQMailChat", MB_OK);
#endif
		closesocket( sock );
		return FALSE;
	}

	ioctl(sock, FIONBIO, &param);
	return sock;
}

int Socket_Accept(int SocketId)
{
	int addrlen;
	SocketConn * sockptr;
	SOCKET sock;
	unsigned int param = 1;

	addrlen=sizeof(struct sockaddr);

	//   Allocate a Socket entry

	sockptr = malloc(sizeof(SocketConn));
	memset(sockptr, 0, sizeof (SocketConn));

	sockptr->Next = Sockets;
	Sockets = sockptr;

	sock = accept(SocketId, (struct sockaddr *)&sockptr->sin, &addrlen);

	if (sock == INVALID_SOCKET)
	{
		Logprintf(LOG_TCP, NULL, '|', " accept() failed Error %d", WSAGetLastError());

		// get rid of socket record

		Sockets = sockptr->Next;
		free(sockptr);
		return FALSE;
	}

	ioctl(sock, FIONBIO, &param);

	sockptr->socket = sock;

	if (SocketId == pop3sock)
	{
		sockptr->Type = POP3SLAVE;
		SendSock(sockptr, "+OK POP3 server ready");
		sockptr->State = GettingUser;
	}	
	else
	{
		sockptr->Type = SMTPServer;
		sockptr->State = WaitingForGreeting;
	//	SendSock(sockptr, "200 BPQMail NNTP Server ready");	
		SendSock(sockptr, "220 BPQMail SMTP Server ready");
	}	

	return 0;
}


VOID ReleaseSock(SOCKET sock)
{
	// remove and free the socket record

	SocketConn * sockptr, * lastptr;

	sockptr=Sockets;
	lastptr=NULL;
		
	while (sockptr)
	{
		if (sockptr->socket == sock)
		{
			if (lastptr)
				lastptr->Next=sockptr->Next;
			else
				Sockets=sockptr->Next;

			if (sockptr->POP3User)
				sockptr->POP3User->POP3Locked = FALSE;

			if (sockptr->State == WaitingForGreeting)
			{
				Logprintf(LOG_TCP, NULL, '|', "Premature Close on Socket %d", sock);
	
				if (sockptr->Type == SMTPClient)
					SMTPActive = FALSE;	
			}

			free(sockptr);
			return;
		}
		else
		{
			lastptr=sockptr;
			sockptr=sockptr->Next;
		}
	}
}

/*
int Socket_Data(int sock, int error, int eventcode)
{
	SocketConn * sockptr;

	//	Find Connection Record

	sockptr=Sockets;
		
	while (sockptr)
	{
		if (sockptr->socket == sock)
		{
			switch (eventcode)
			{
				case FD_READ:

					return DataSocket_Read(sockptr,sock);

				case FD_WRITE:

					// Either Just connected, or flow contorl cleared

					if (sockptr->SendBuffer)
						// Data Queued
						SendFromQueue(sockptr);
					else
					{
						if (sockptr->Type == SMTPServer)
							SendSock(sockptr, "220 BPQMail SMTP Server ready");
						else
						{
							if (sockptr->Type == POP3SLAVE)
							{
								SendSock(sockptr, "+OK POP3 server ready");
								sockptr->State = GettingUser;
							}
						}
					}
					return 0;

				case FD_OOB:

					return 0;

				case FD_ACCEPT:

					return 0;

				case FD_CONNECT:

					return 0;

				case FD_CLOSE:

					closesocket(sock);
					ReleaseSock(sock);
					return 0;
				}
			return 0;
		}
		else
			sockptr=sockptr->Next;
	}

	return 0;
}
*/
int DataSocket_Read(SocketConn * sockptr, SOCKET sock)
{
	int InputLen, MsgLen;
	char * ptr, * ptr2;
	char Buffer[2000];

	// May have several messages per packet, or message split over packets

	if (sockptr->InputLen > 1000)	// Shouldnt have lines longer  than this in text mode
	{
		sockptr->InputLen=0;
	}
				
	InputLen=recv(sock, &sockptr->TCPBuffer[sockptr->InputLen], 1000, 0);

	if (InputLen <= 0)
	{
		int x = WSAGetLastError();

		closesocket(sock);
		ReleaseSock(sock);

		return 0;					// Does this mean closed?
	}

	sockptr->InputLen += InputLen;

loop:
	
	ptr = memchr(sockptr->TCPBuffer, '\n', sockptr->InputLen);

	if (ptr)	//  CR in buffer
	{
		ptr2 = &sockptr->TCPBuffer[sockptr->InputLen];
		ptr++;				// Assume LF Follows CR

		if (ptr == ptr2)
		{
			// Usual Case - single meg in buffer
	
			if (sockptr->Type == SMTPServer)
				ProcessSMTPServerMessage(sockptr, sockptr->TCPBuffer, sockptr->InputLen);
			else
			if (sockptr->Type == POP3SLAVE)
				ProcessPOP3ServerMessage(sockptr, sockptr->TCPBuffer, sockptr->InputLen);
			else
			if (sockptr->Type == SMTPClient)
				ProcessSMTPClientMessage(sockptr, sockptr->TCPBuffer, sockptr->InputLen);
			else
			if (sockptr->Type == POP3Client)
				ProcessPOP3ClientMessage(sockptr, sockptr->TCPBuffer, sockptr->InputLen);

			sockptr->InputLen=0;
		
		}
		else
		{
			// buffer contains more that 1 message

			MsgLen = sockptr->InputLen - (ptr2-ptr);

			memcpy(Buffer, sockptr->TCPBuffer, MsgLen);


			if (sockptr->Type == SMTPServer)
				ProcessSMTPServerMessage(sockptr, Buffer, MsgLen);
			else
			if (sockptr->Type == POP3SLAVE)
				ProcessPOP3ServerMessage(sockptr, Buffer, MsgLen);
			else
			if (sockptr->Type == SMTPClient)
				ProcessSMTPClientMessage(sockptr, Buffer, MsgLen);
			else
			if (sockptr->Type == POP3Client)
				ProcessPOP3ClientMessage(sockptr, Buffer, MsgLen);


			memmove(sockptr->TCPBuffer, ptr, sockptr->InputLen-MsgLen);

			sockptr->InputLen -= MsgLen;

			goto loop;

		}
	}
	return TRUE;
}

char * FindPart(char ** Msg, char * Boundary, int * PartLen)
{
	char * ptr = *Msg, * ptr2;
	char * Msgptr = *Msg;
	int BLen = strlen(Boundary);
	char * Part;

	while(*ptr)				// Just in case we run off end
	{
		ptr2 = strchr(ptr, 10);	// Find LF

		if (ptr2 == NULL) return NULL;

		if (*ptr == '-' && *(ptr+1) == '-')
		{
			if (memcmp(&ptr[2], Boundary, BLen) == 0)
			{
				// Found Boundary

				int Partlen = ptr - Msgptr;
				Part = malloc(Partlen + 1);
				memcpy(Part, Msgptr, Partlen);
				Part[Partlen] = 0;

				*Msg = ++ptr2;
		
				*PartLen = Partlen;

				return Part; 
			}
		}

		ptr = ++ptr2;
	}
	return NULL;
}





BOOL CheckforMIME(SocketConn * sockptr, char * Msg, char ** Body, int * MsgLen)	// Will reformat message if necessary. 
{
	int i;
	char * ptr, * ptr2, * ptr3, * ptr4;
	char Boundary[1000];
	BOOL Multipart = FALSE;
	BOOL ALT = FALSE;
	int Partlen;
	char * Save;
	BOOL Base64 = FALSE;
	BOOL QuotedP = FALSE;
	
	char FileName[100][250] = {""};
	int FileLen[100];
	char * FileBody[100];
	char * MallocSave[100];
	UCHAR * NewMsg;

	int Files = 0;

	ptr = Msg;

	while(*ptr != 13)
	{
		ptr2 = strchr(ptr, 10);	// Find CR

		while(ptr2[1] == ' ' || ptr2[1] == 9)		// Whitespace - continuation line
		{
			ptr2 = strchr(&ptr2[1], 10);	// Find CR
		}

//		Content-Type: multipart/mixed;
//	boundary="----=_NextPart_000_025B_01CAA004.84449180"
//		7.2.2 The Multipart/mixed (primary) subtype
//		7.2.3 The Multipart/alternative subtype


		if (_memicmp(ptr, "Content-Type: ", 14) == 0)
		{
			char Line[1000] = "";
			char lcLine[1000] = "";

			char * ptr3;

			memcpy(Line, &ptr[14], ptr2-ptr-14);
			memcpy(lcLine, &ptr[14], ptr2-ptr-14);
			_strlwr(lcLine);

			if (_memicmp(Line, "Multipart/", 10) == 0)
			{
				Multipart = TRUE;

				if (_memicmp(&Line[10], "alternative", 11) == 0)
				{
					ALT = TRUE;
				}

				ptr3 = strstr(Line, "boundary");

				if (ptr3)
				{
					ptr3+=9;

					if ((*ptr3) == '"')
						ptr3++;

					strcpy(Boundary, ptr3);
					ptr3 = strchr(Boundary, '"');
					if (ptr3) *ptr3 = 0;
					ptr3 = strchr(Boundary, 13);			// CR
					if (ptr3) *ptr3 = 0;

				}
				else
					return FALSE;						// Can't do anything without a boundary ??
			}

		}

		else if (_memicmp(ptr, "Content-Transfer-Encoding:", 26) == 0)
		{
			if (strstr(&ptr[26], "base64"))
				Base64 = TRUE;
			else
			if (strstr(&ptr[26], "quoted-printable"))
				QuotedP = TRUE;
		}


		ptr = ptr2;
		ptr++;

	}

	if (Multipart == FALSE)
	{
		// We only have one part, but it could have an odd encoding

		if (Base64)
		{
			int i = 0, Len = *MsgLen, NewLen;
			char * ptr2;
			char * End;

			ptr = ptr2 = *Body;
			End = ptr + Len;

			while (ptr < End)
			{
				while (*ptr < 33)
					{ptr++;}

				*ptr2++ = *ptr++;
			}

			*ptr2 = 0;

			ptr = *Body;
			Len = ptr2 - ptr -1;

			ptr2 = ptr;

			while (Len > 0)
			{
				decodeblock(ptr, ptr2);
				ptr += 4;
				ptr2 += 3;
				Len -= 4;
			}

			NewLen = ptr2 - *Body;

			if (*(ptr-1) == '=')
				NewLen--;

			if (*(ptr-2) == '=')
				NewLen--;

			*MsgLen = NewLen;
		}
		else if (QuotedP)
		{
			int i = 0, Len = *MsgLen;
			char * ptr2;
			char * End;

			ptr = ptr2 =*Body;

			End = ptr + Len;

			while (ptr < End)
			{
				if ((*ptr) == '=')
				{
					char c = *(++ptr);
					char d;

					c = c - 48;
					if (c < 0)
					{
						// = CRLF as a soft break

						ptr += 2;
						continue;
					}

					if (c > 9)
						c -= 7;
					d  = *(++ptr);
					d = d - 48;
					if (d > 9)
						d -= 7;

					*(ptr2) = c << 4 | d;
					ptr2++;	
					ptr++;
				}
				else
				{
					*ptr2++ = *ptr++;
				}
			}
			*ptr2 = 0;

			*MsgLen = ptr2 - *Body;

		}

		return FALSE;
	}
	// FindPart Returns Next Part of Message, Updates Input Pointer
	// Skip to first Boundary (over the non MIME Alt Part)

	ptr = FindPart(Body, Boundary, &Partlen);

	if (ptr == NULL)
		return FALSE;			// Couldn't find separator

	free(ptr);
	
	if (ALT)
	{
		// Assume HTML and Plain Text Versions of the same single body.

		ptr = FindPart(Body, Boundary, &Partlen);	

		Save = ptr;		// For free();

		// Should be the First (Least desireable part, but the bit we want, as we are only interested in plain text)

		// Skip any headers
	
		while(*ptr != 13)
		{
			if (_memicmp(ptr, "Content-Transfer-Encoding:", 26) == 0)
			{
				if (strstr(&ptr[26], "base64"))
					Base64 = TRUE;
				else
				if (strstr(&ptr[26], "quoted-printable"))
					QuotedP = TRUE;
			}

			ptr2 = strchr(ptr, 10);	// Find CR
					
			while(ptr2[1] == ' ' || ptr2[1] == 9)		// Whitespace - continuation line
			{
				ptr2 = strchr(&ptr2[1], 10);	// Find CR
			}

			ptr = ++ptr2;
		}

		ptr += 2;		// Skip rerminating line

		// Should now have a plain text body to return;

		// But could be an odd encoding

		if (Base64)
		{
			int i = 0, Len = strlen(ptr), NewLen;
			char * ptr2;
			char * End;
			char * Save = ptr;

			ptr2 = ptr;
			End = ptr + Len;

			while (ptr < End)
			{
				while (*ptr < 33)
					{ptr++;}

				*ptr2++ = *ptr++;
			}

			*ptr2 = 0;

			ptr = Save;
			Len = ptr2 - ptr -1;

			ptr2 = *Body;

			while (Len > 0)
			{
				decodeblock(ptr, ptr2);
				ptr += 4;
				ptr2 += 3;
				Len -= 4;
			}

			NewLen = ptr2 - *Body;

			if (*(ptr-1) == '=')
				NewLen--;

			if (*(ptr-2) == '=')
				NewLen--;

			*MsgLen = NewLen;
		}
		else if (QuotedP)
		{
			int i = 0, Len = strlen(ptr);
			char * ptr2;
			char * End;
			char * Save = ptr;

			ptr2 = *Body;

			End = ptr + Len;

			while (ptr < End)
			{
				if ((*ptr) == '=')
				{
					char c = *(++ptr);
					char d;

					c = c - 48;
					if (c < 0)
					{
						// = CRLF as a soft break

						ptr += 2;
						continue;
					}

					if (c > 9)
						c -= 7;
					d  = *(++ptr);
					d = d - 48;
					if (d > 9)
						d -= 7;

					*(ptr2) = c << 4 | d;
					ptr2++;	
					ptr++;
				}
				else
				{
					*ptr2++ = *ptr++;
				}
			}
			*ptr2 = 0;

			*MsgLen = ptr2 - *Body;
		}
		else
		{
			strcpy(*Body, ptr);
			*MsgLen = strlen(ptr);
		}
		free(Save);
	
		return FALSE;
	}

	// Assume Multipart/Mixed - Message with attachments

	ptr = FindPart(Body, Boundary, &Partlen);

	if (ptr == NULL)
		return FALSE;			// Couldn't find separator

	while (ptr)
	{
		BOOL Base64 = FALSE;
		BOOL QuotedP = FALSE;

		MallocSave[Files] = ptr;		// For free();

		// Should be the First (Least desireable part, but the bit we want, as we are only interested in plain text)

		// Process headers - looking for Content-Disposition: attachment;

		// The first could also be a Content-Type: multipart/alternative; - if so, feed back to mime handler
	
		while(*ptr != 13)
		{
			char lcLine[1000] = "";

			ptr2 = strchr(ptr, 10);	// Find CR

			if (ptr2 == 0)
				return FALSE;
					
			while(ptr2[1] == ' ' || ptr2[1] == 9)		// Whitespace - continuation line
			{
				ptr2 = strchr(&ptr2[1], 10);	// Find CR
			}

			memcpy(lcLine, ptr, ptr2-ptr-1);
			_strlwr(lcLine);

			ptr = lcLine;

			if (_memicmp(ptr, "Content-Type: Multipart/alternative", 30) == 0)
			{
				// Feed Back
				int MsgLen;
				char * Text = malloc(Partlen+1);

				memcpy(Text, MallocSave[Files], Partlen);

				free(MallocSave[Files]);
				MallocSave[Files] = Text;


				CheckforMIME(sockptr, Text, &Text, &MsgLen);

				FileName[Files][0] = 0;				
				FileBody[Files] = Text;


				FileLen[Files++] = MsgLen;

				goto NextPart;

			}
			else if (_memicmp(ptr, "Content-Disposition: ", 21) == 0)
			{
				ptr3 = strstr(&ptr[21], "filename");
				
				if (ptr3)
				{
					ptr3 += 9;
					if (*ptr3 == '"') ptr3++;
					ptr4 = strchr(ptr3, '"');
					if (ptr4) *ptr4 = 0;

					strcpy(FileName[Files], ptr3);
				}
			}

			else if (_memicmp(ptr, "Content-Transfer-Encoding:", 26) == 0)
			{
				if (strstr(&ptr[26], "base64"))
					Base64 = TRUE;
				else
				if (strstr(&ptr[26], "quoted-printable"))
					QuotedP = TRUE;
			}

			ptr = ++ptr2;
		}

		ptr += 2;

		// Should now have file or plain text. If file is Base64 encoded, decode it.

		FileBody[Files] = ptr;
		FileLen[Files] = Partlen -2 - (ptr - MallocSave[Files]);

		if (Base64)
		{
			int i = 0, Len = FileLen[Files], NewLen;
			char * ptr2 = ptr;
			char * End;

			End = ptr + FileLen[Files];

			while (ptr < End)
			{
				while (*ptr < 33)
					{ptr++;}

				*ptr2++ = *ptr++;
			}
			*ptr2 = 0;

			ptr = FileBody[Files];
			Len = ptr2 - ptr -1;

			ptr2 = ptr;

			while (Len > 0)
			{
				decodeblock(ptr, ptr2);
				ptr += 4;
				ptr2 += 3;
				Len -= 4;
			}

			NewLen = ptr2 - FileBody[Files];

			if (*(ptr-1) == '=')
				NewLen--;

			if (*(ptr-2) == '=')
				NewLen--;
	
			FileLen[Files] = NewLen;
		}
		else if (QuotedP)
		{
			int i = 0, Len = FileLen[Files], NewLen;
			char * ptr2 = ptr;
			char * End;

			End = ptr + FileLen[Files];

			while (ptr < End)
			{
				if ((*ptr) == '=')
				{
					char c = *(++ptr);
					char d;

					c = c - 48;
					if (c < 0)
					{
						// = CRLF as a soft break

						ptr += 2;
						continue;
					}

					if (c > 9)
						c -= 7;
					d  = *(++ptr);
					d = d - 48;
					if (d > 9)
						d -= 7;

					*(ptr2) = c << 4 | d;
					ptr2++;	
					ptr++;
				}
				else
				{
					*ptr2++ = *ptr++;
				}
			}
			*ptr2 = 0;

			NewLen = ptr2 - FileBody[Files];

			FileLen[Files] = NewLen;
		}
		
		Files++;

	NextPart:
		ptr = FindPart(Body, Boundary, &Partlen);
	}

	// Now have all the parts - build a B2 Message. Leave the first part of header for later,
	// as we may have multiple recipients. Start with the Body: Line.

	// We need to add the first part of header later, so start message part way down buffer.
	// Make sure buffer is big enough.

	if ((sockptr->MailSize + 2000) > sockptr->MailBufferSize)
	{
		sockptr->MailBufferSize += 2000;
		sockptr->MailBuffer = realloc(sockptr->MailBuffer, sockptr->MailBufferSize);
	
		if (sockptr->MailBuffer == NULL)
		{
			CriticalErrorHandler("Failed to extend Message Buffer");
			shutdown(sockptr->socket, 0);
			return FALSE;
		}
	}


	NewMsg = sockptr->MailBuffer + 1000;

	NewMsg += sprintf(NewMsg, "Body: %d\r\n", FileLen[0]);

	for (i = 1; i < Files; i++)
	{
		NewMsg += sprintf(NewMsg, "File: %d %s\r\n", FileLen[i], FileName[i]);
	}

	NewMsg += sprintf(NewMsg, "\r\n");

	for (i = 0; i < Files; i++)
	{
		memcpy(NewMsg, FileBody[i], FileLen[i]);
		NewMsg += FileLen[i];
		free(MallocSave[i]);
		NewMsg += sprintf(NewMsg, "\r\n");
	}

	*MsgLen = NewMsg - (sockptr->MailBuffer + 1000);
	*Body = sockptr->MailBuffer + 1000;

	return TRUE;		// B2 Message
}


VOID ProcessSMTPServerMessage(SocketConn * sockptr, char * Buffer, int Len)
{
	SOCKET sock;
	int i;
	time_t Date = 0;

	sock=sockptr->socket;

	WriteLogLine(NULL, '<',Buffer, Len-2, LOG_TCP);

	if (sockptr->Flags == GETTINGMESSAGE)
	{
		if(memcmp(Buffer, ".\r\n", 3) == 0)
		{
			char * ptr1, * ptr2;
			int linelen, MsgLen;
			char Msgtitle[62];
			BOOL B2Flag;
			int ToLen = 0;
			char * ToString;
			char * Via;
			
			// Scan headers for a Subject: or Date: Line (Headers end at blank line)

			ptr1 = sockptr->MailBuffer;
		Loop:
			ptr2 = strchr(ptr1, '\r');

			if (ptr2 == NULL)
			{
				SendSock(sockptr, "500 Eh");
				return;
			}

			linelen = ptr2 - ptr1;

			if (_memicmp(ptr1, "Subject:", 8) == 0)
			{
				if (linelen > 68) linelen = 68;
				memcpy(Msgtitle, &ptr1[9], linelen-9);
				Msgtitle[linelen-9]=0;
			}

			if (_memicmp(ptr1, "Date:", 5) == 0)
			{
				struct tm rtime;
				char * Context;
				char seps[] = " ,\t\r";
				char Offset[10] = "";
				int i, HH, MM;
				char Copy[500]="";

				// Copy message, so original isn't messed up by strtok
				
				memcpy(Copy, ptr1, linelen);

				ptr1 = Copy;

				memset(&rtime, 0, sizeof(struct tm));

				// Date: Tue, 9 Jun 2009 20:54:55 +0100

				ptr1 = strtok_s(&ptr1[5], seps, &Context);	// Skip Day
				ptr1 = strtok_s(NULL, seps, &Context);		// Day

				rtime.tm_mday = atoi(ptr1);

				ptr1 = strtok_s(NULL, seps, &Context);		// Month

				for (i=0; i < 12; i++)
				{
					if (strcmp(month[i], ptr1) == 0)
					{
						rtime.tm_mon = i;
						break;
					}
				}
		
				sscanf(Context, "%04d %02d:%02d:%02d%s",
					&rtime.tm_year, &rtime.tm_hour, &rtime.tm_min, &rtime.tm_sec, Offset);

				rtime.tm_year -= 1900;

				Date = mktime(&rtime) - (time_t)_MYTIMEZONE;
	
				if (Date == (time_t)-1)
					Date = 0;
				else
				{
					if ((Offset[0] == '+') || (Offset[0] == '-'))
					{
						MM = atoi(&Offset[3]);
						Offset[3] = 0;
						HH = atoi(&Offset[1]);
						MM = MM + (60 * HH);

						if (Offset[0] == '+')
							Date -= (60*MM);
						else
							Date += (60*MM);

					}
				}
			}

			ptr1 = ptr2 + 2;		// Skip crlf
			
			if (linelen)			// Not Null line
			{
				goto Loop;
			}

			ptr2 = ptr1;
			ptr1 = sockptr->MailBuffer;

			MsgLen = sockptr->MailSize - (ptr2 - ptr1);

			// We Just want the from call, not the full address.
			
			TidyString(sockptr->MailFrom);
			
			strlop(sockptr->MailFrom, '@');
			if (strlen(sockptr->MailFrom) > 6) sockptr->MailFrom[6]=0;

			// Examine Message to look for html formatting and attachments.

			B2Flag = CheckforMIME(sockptr, sockptr->MailBuffer, &ptr2, &MsgLen);	// Will reformat message if necessary. 

			// If any recipients are via RMS, create one message for them, and separate messages for all others

			ToString = zalloc(sockptr->Recipients * 100);
	
			for (i=0; i < sockptr->Recipients; i++)
			{
				char Addr[256];					// Need copy, as we may change it then decide it isn't for RMS

				strcpy(Addr, sockptr->RecpTo[i]);
				Debugprintf("To Addr %s", Addr);

				TidyString(Addr);
				Debugprintf("To Addr after Tidy %s", Addr);

				if ((_memicmp (Addr, "RMS:", 4) == 0) |(_memicmp (Addr, "RMS/", 4) == 0))
				{
					// Add to B2 Message for RMS
										
					_strlwr(Addr);
					
					Via = strlop(&Addr[4], '@');
				
					if (Via && _stricmp(Via, "winlink.org") == 0)
					{
						if (CheckifLocalRMSUser(Addr)) // if local RMS - Leave Here
							continue;
						
						ToLen = sprintf(ToString, "%sTo: %s\r\n", ToString, &Addr[4]);
						*sockptr->RecpTo[i] = 0;		// So we dont create individual one later
						continue;
					}

					ToLen = sprintf(ToString, "%sTo: %s@%s\r\n", ToString, &Addr[4], Via);
					*sockptr->RecpTo[i] = 0;			// So we dont create individual one later
					continue;
				}

				_strupr(Addr);
				Debugprintf("To Addr after strupr %s", Addr);

				Via = strlop(Addr, '@');
				Debugprintf("Via %s", Via);

				if (Via && _stricmp(Via, "winlink.org") == 0)
				{
					if (CheckifLocalRMSUser(Addr)) // if local RMS - Leave Here
						continue;
					
					ToLen = sprintf(ToString, "%sTo: %s\r\n", ToString, Addr);
					*sockptr->RecpTo[i] = 0;		// So we dont create individual one later

					continue;
				}
			}

			if (ToLen)						// Have some RMS Addresses
			{
				char B2Hddr[1000];
				int B2HddrLen;
				char DateString[80];
				char * NewBody;
				struct tm * tm;
				struct MsgInfo * Msg;
				BIDRec * BIDRec;

				Msg = AllocateMsgRecord();
		
				// Set number here so they remain in sequence
		
				Msg->number = ++LatestMsg;
				MsgnotoMsg[Msg->number] = Msg;
				Msg->length = MsgLen;

				sprintf_s(Msg->bid, sizeof(Msg->bid), "%d_%s", LatestMsg, BBSName);

				Msg->type = 'P';
				Msg->status = 'N';
				strcpy(Msg->to, "RMS");
				strcpy(Msg->from, sockptr->MailFrom);
				strcpy(Msg->title, Msgtitle);

				BIDRec = AllocateBIDRecord();

				strcpy(BIDRec->BID, Msg->bid);
				BIDRec->mode = Msg->type;
				BIDRec->u.msgno = LOWORD(Msg->number);
				BIDRec->u.timestamp = LOWORD(time(NULL)/86400);

				Msg->datereceived = Msg->datechanged = Msg->datecreated = time(NULL);

				if (Date)
					Msg->datecreated = Date;

				tm = gmtime(&Date);	
	
				sprintf(DateString, "%04d/%02d/%02d %02d:%02d",
					tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);

				if (B2Flag)				// Message has attachments, so Body: line is present
				{
					Msg->B2Flags = B2Msg | Attachments;
		
					B2HddrLen = sprintf(B2Hddr,
						"MID: %s\r\nDate: %s\r\nType: %s\r\nFrom: %s\r\n%sSubject: %s\r\nMbo: %s\r\n",
						Msg->bid, DateString, "Private", Msg->from, ToString, Msg->title, BBSName);
				}
				else
				{
					Msg->B2Flags = B2Msg;
					B2HddrLen = sprintf(B2Hddr,
						"MID: %s\r\nDate: %s\r\nType: %s\r\nFrom: %s\r\n%sSubject: %s\r\nMbo: %s\r\nBody: %d\r\n\r\n",
						Msg->bid, DateString, "Private", Msg->from, ToString, Msg->title, BBSName, Msg->length);

				}
			
				NewBody = ptr2 - B2HddrLen;

				memcpy(NewBody, B2Hddr, B2HddrLen);

				Msg->length += B2HddrLen;

				free(ToString);
	
				// Set up forwarding bitmap

				MatchMessagetoBBSList(Msg, 0);

				CreateSMTPMessageFile(NewBody, Msg);
			}
			
			for (i=0; i < sockptr->Recipients; i++)
			{
				if (*sockptr->RecpTo[i])			// not already sent to RMS?
					CreateSMTPMessage(sockptr, i, Msgtitle, Date, ptr2, MsgLen, B2Flag);
				else
					free(sockptr->RecpTo[i]);
			}

			free(sockptr->RecpTo);
			sockptr->RecpTo = NULL;
			free(sockptr->MailFrom);
			free(sockptr->MailBuffer);

			sockptr->MailBufferSize=0;
			sockptr->MailBuffer=0;
			sockptr->MailSize = 0;
	
			sockptr->Flags = 0;
			sockptr->Recipients = 0;

			SendSock(sockptr, "250 Ok");
			return;
		}

		if ((sockptr->MailSize + Len) > sockptr->MailBufferSize)
		{
			sockptr->MailBufferSize += 10000;
			sockptr->MailBuffer = realloc(sockptr->MailBuffer, sockptr->MailBufferSize);
	
			if (sockptr->MailBuffer == NULL)
			{
				CriticalErrorHandler("Failed to extend Message Buffer");
				shutdown(sock, 0);
				return;
			}
		}

		memcpy(&sockptr->MailBuffer[sockptr->MailSize], Buffer, Len);
		sockptr->MailSize += Len;

		return;
	}

	if (sockptr->State == GettingUser)
	{
		char Out[30];
		
		Buffer[Len-2]=0;

		decodeblock(Buffer, Out);
		decodeblock(&Buffer[4], &Out[3]);
		decodeblock(&Buffer[8], &Out[6]);
		decodeblock(&Buffer[12], &Out[9]);

		if (strlen(Out) > 10) Out[10] = 0;

		strcpy(sockptr->CallSign, Out);
		
		sockptr->State = GettingPass;
		SendSock(sockptr, "334 UGFzc3dvcmQ6");
		return;
	}

	if (sockptr->State == GettingPass)
	{
		struct UserInfo * user = NULL;
		char Out[30];

		Buffer[Len-2]=0;

		decodeblock(Buffer, Out);
		decodeblock(&Buffer[4], &Out[3]);
		decodeblock(&Buffer[8], &Out[6]);
		decodeblock(&Buffer[12], &Out[9]);
		decodeblock(&Buffer[16], &Out[12]);
		decodeblock(&Buffer[20], &Out[15]);

		user = LookupCall(sockptr->CallSign);

		if (user)
		{
			if (strcmp(user->pass, Out) == 0)
			{
				sockptr->State = Authenticated;
				SendSock(sockptr, "235 2.0.0 OK Authenticated"); //535 authorization failed
				return;
			}
		}

		SendSock(sockptr, "535 authorization failed");
		sockptr->State = 0;
		return;
	}



/*AUTH LOGIN

334 VXNlcm5hbWU6
a4msl9ux
334 UGFzc3dvcmQ6
ZvVx9G1hcg==
235 2.0.0 OK Authenticated
*/


	if(memcmp(Buffer, "AUTH LOGIN", 10) == 0)
	{
		sockptr->State = GettingUser;
		SendSock(sockptr, "334 VXNlcm5hbWU6");
		return;
	}

	if(memcmp(Buffer, "EHLO",4) == 0)
	{
		SendSock(sockptr, "250-BPQ Mail Server");
		SendSock(sockptr, "250 AUTH LOGIN");

		//250-8BITMIME

		return;
	}

	if(memcmp(Buffer, "AUTH LOGIN", 10) == 0)
	{
		sockptr->State = GettingUser;
		SendSock(sockptr, "334 VXNlcm5hbWU6");
		return;
	}


	if(memcmp(Buffer, "HELO",4) == 0)
	{
		SendSock(sockptr, "250 Ok");
		return;
	}
	
	if(_memicmp(Buffer, "MAIL FROM:", 10) == 0)
	{
		if (sockptr->State != Authenticated)
		{
			// Accept if from 44/8 and ends in ampr.org

			if (_memicmp(&Buffer[Len - 11], "ampr.org", 8) == 0 &&
				(sockptr->sin.sin_addr.s_addr & 0xff) == 44)	
			{
			}
			else
			{
				SendSock(sockptr, "530 Authentication required");
				return;
			}
		}
		
		sockptr->MailFrom = zalloc(Len);
		memcpy(sockptr->MailFrom, &Buffer[10], Len-12);
			
		SendSock(sockptr, "250 Ok");

		return;
	}

	if(_memicmp(Buffer, "RCPT TO:", 8) == 0)
	{
		if (sockptr->State != Authenticated)
		{
			// Accept if from 44/8 and ends in ampr.org



			if (_memicmp(&Buffer[Len - 11], "ampr.org", 8) == 0 &&
				(sockptr->sin.sin_addr.s_addr & 0xff) == 44)	
			{
			}
			else
			{
				SendSock(sockptr, "530 Authentication required");
				return;
			}
		}

		sockptr->RecpTo=realloc(sockptr->RecpTo, (sockptr->Recipients+1)*4);
		sockptr->RecpTo[sockptr->Recipients] = zalloc(Len);

		memcpy(sockptr->RecpTo[sockptr->Recipients++], &Buffer[8], Len-10);
			
		SendSock(sockptr, "250 Ok");
		return;
	}

	if(memcmp(Buffer, "DATA\r\n", 6) == 0)
	{
		sockptr->MailBuffer=malloc(10000);
		sockptr->MailBufferSize=10000;

		if (sockptr->MailBuffer == NULL)
		{
			CriticalErrorHandler("Failed to create SMTP Message Buffer");
			SendSock(sockptr, "250 Failed");
			shutdown(sock, 0);

			return;
		}
	
		sockptr->Flags |= GETTINGMESSAGE;

		SendSock(sockptr, "354 End data with <CR><LF>.<CR><LF>");
		return;
	}

	if(memcmp(Buffer, "QUIT\r\n", 6) == 0)
	{
		SendSock(sockptr, "221 OK");
		Sleep(500);
		shutdown(sock, 0);
		return;
	}

	if(memcmp(Buffer, "RSET\r\n", 6) == 0)
	{
		SendSock(sockptr, "250 Ok");
		sockptr->State = 0;
		sockptr->Recipients = 0;
//		Sleep(500);
//		shutdown(sock, 0);
		return;
	}

	return;
}


CreateSMTPMessage(SocketConn * sockptr, int i, char * MsgTitle, time_t Date, char * MsgBody, int MsgLen, BOOL B2Flag)
{
	struct MsgInfo * Msg;
	BIDRec * BIDRec;
	char * To;
	char * via;

	// Allocate a message Record slot

	Msg = AllocateMsgRecord();
		
	// Set number here so they remain in sequence
		
	Msg->number = ++LatestMsg;
	MsgnotoMsg[Msg->number] = Msg;
	Msg->length = MsgLen;

	sprintf_s(Msg->bid, sizeof(Msg->bid), "%d_%s", LatestMsg, BBSName);

	Msg->type = 'P';
	Msg->status = 'N';

	BIDRec = AllocateBIDRecord();

	strcpy(BIDRec->BID, Msg->bid);
	BIDRec->mode = Msg->type;
	BIDRec->u.msgno = LOWORD(Msg->number);
	BIDRec->u.timestamp = LOWORD(time(NULL)/86400);

	Msg->datereceived = Msg->datechanged = Msg->datecreated = time(NULL);

	if (Date)
		Msg->datecreated = Date;

	To = sockptr->RecpTo[i];

	Debugprintf("To %s", To);

	TidyString(To);

	Debugprintf("To after tidy %s", To);

	if (_memicmp(To, "bull/", 5) == 0)
	{
		Msg->type = 'B';
		memmove(To, &To[5], strlen(&To[4]));
	}

	if ((_memicmp(To, "nts/", 4) == 0) ||(_memicmp(To, "nts:", 4) == 0) ||
		(_memicmp(To, "nts.", 4) == 0))
	{
		Msg->type = 'T';
		memmove(To, &To[4], strlen(&To[3]));
	}

	if (_memicmp(To, "rms:", 4) == 0)
	{
		via = _strlwr(strlop(To, ':'));
	}
	else if (_memicmp(To, "rms/", 4) == 0)
	{
		via = _strlwr(strlop(To, '/'));
	}
	else if (_memicmp(To, "rms.", 4) == 0)
	{
		via = _strlwr(strlop(To, '.'));
	}
	else if (_memicmp(To, "smtp:", 5) == 0)
	{
		via = _strlwr(strlop(To, ':'));
		To[0] = 0;
	}
	else if (_memicmp(To, "smtp/", 5) == 0)
	{
		via = _strlwr(strlop(To, '/'));
		To[0] = 0;
	}
	else
	{
		via = strlop(To, '@');
	}

	Debugprintf("via %s", via);

	if (via)
	{
		int toLen;
		
		if (strlen(via) > 40) via[40] = 0;

		strcpy(Msg->via, via);		// Save before messing with it

		// if ending in AMPR.ORG send via ISP if we have enabled forwarding AMPR

		toLen = strlen(via);

		if (_memicmp(&via[toLen - 8], "ampr.org", 8) == 0)
		{
			// if our domain keep here.
				
			// if not, and SendAMPRDirect set, set as ISP,
			// else set as RMS			
				
			if (_stricmp(via, AMPRDomain) == 0)
			{
				// Our Message- dont forward
			}
			else
			{
				// AMPR but not us

				if (SendAMPRDirect)
				{
//					sprintf(Msg->via,"%s@%s", To, via);
//					strcpy(To, "AMPR");
				}
				else
				{
					sprintf(Msg->via,"%s@%s", To, via);
					strcpy(To, "RMS");
				}
			}
		}
		else
		{	
			strlop(via, '.');			// Get first part of address

			if (_stricmp(via, BBSName) == 0)
			{
				// sent via us - clear the name

				Msg->via[0] = 0;
			}
		}
	}

	if (strlen(To) > 6) To[6]=0;

	strcpy(Msg->to, To);

	strcpy(Msg->from, sockptr->MailFrom);

	strcpy(Msg->title, MsgTitle);

	if(Msg->to[0] == 0)
		SMTPMsgCreated=TRUE;

	// If NTS message (TO is numeric and AT is NTSxx or NTSxx.NTS - Outlook won't accept x@y)

	if (isdigits(Msg->to) && memcmp(Msg->via, "NTS", 3) == 0)
	{
		if (Msg->via[5] == 0 || strcmp(&Msg->via[5], ".NTS") == 0)
		{
			Msg->type = 'T';
			Msg->via[5] = 0;
		}
	}

	Debugprintf("Msg->Via %s", Msg->via);

	if (B2Flag)
	{
		char B2Hddr[1000];
		int B2HddrLen;
		char B2To[80];
		char * NewBody;
		char DateString[80];
		char * TypeString;
		struct tm * tm;

		tm = gmtime(&Date);	
	
		sprintf(DateString, "%04d/%02d/%02d %02d:%02d",
			tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);


		if (strcmp(Msg->to, "RMS") == 0)		// Address is in via
		strcpy(B2To, Msg->via);
	else
		if (Msg->via[0])
			sprintf(B2To, "%s@%s", Msg->to, Msg->via);
		else
			strcpy(B2To, Msg->to);

		
		Msg->B2Flags = B2Msg | Attachments;

		if (Msg->type == 'P')
			TypeString = "Private" ;
		else if (Msg->type == 'B')
			TypeString = "Bulletin";
		else if (Msg->type == 'T')
			TypeString = "Traffic";

		B2HddrLen = sprintf(B2Hddr,
			"MID: %s\r\nDate: %s\r\nType: %s\r\nFrom: %s\r\nTo: %s\r\nSubject: %s\r\nMbo: %s\r\n",
			Msg->bid, DateString, TypeString,
			Msg->from, B2To, Msg->title, BBSName);

		NewBody = MsgBody - B2HddrLen;

		memcpy(NewBody, B2Hddr, B2HddrLen);

		Msg->length += B2HddrLen;

		free(To);
	
		// Set up forwarding bitmap

		MatchMessagetoBBSList(Msg, 0);

		if (Msg->type == 'B' && memcmp(Msg->fbbs, zeros, NBMASK) != 0)
			Msg->status = '$';				// Has forwarding

		return CreateSMTPMessageFile(NewBody, Msg);

	}

	free(To);

	// Set up forwarding bitmap

	MatchMessagetoBBSList(Msg, 0);

	if (Msg->type == 'B' && memcmp( Msg->fbbs, zeros, NBMASK) != 0)
		Msg->status = '$';				// Has forwarding

	return CreateSMTPMessageFile(MsgBody, Msg);
		
}


BOOL CreateSMTPMessageFile(char * Message, struct MsgInfo * Msg)
{
	char MsgFile[250];
	FILE * hFile;
	int WriteLen=0;
	char Mess[255];
	int len;


	sprintf_s(MsgFile, sizeof(MsgFile), "%s/m_%06d.mes", MailDir, Msg->number);
	
	hFile = fopen(MsgFile, "wb");
	
	if (hFile)
	{
		WriteLen = fwrite(Message, 1, Msg->length, hFile); 
		fclose(hFile);
	}

	if (WriteLen != Msg->length)
	{
		len = sprintf_s(Mess, sizeof(Mess), "Failed to create Message File\r");
		CriticalErrorHandler(Mess);

		return FALSE;
	}

	SaveMessageDatabase();
	SaveBIDDatabase();

	return TRUE;
}

TidyString(char * Address)
{
	// Cleans up a From: or To: Address

	// May have leading or trailing spaces, or be enclosed by <>,  or have a " " part

	// From: "John Wiseman" <john.wiseman@ntlworld.com>

	char * ptr1, * ptr2;
	int len;

	_strupr(Address);

	ptr1 = strchr(Address, '<');

	if (ptr1)
	{
		ptr1++;
		ptr2 = strlop(ptr1, '>');
		len = strlen(ptr1);
		memmove(Address, ptr1, len);
		Address[len] = 0;

		// Could have surrounding "" ""
	
		if (Address[0] == '"')
		{
			int len = strlen(Address) - 1;
		
			if (Address[len] == '"')
			{
				Address[len] = 0;
				memmove(Address, &Address[1], len);
				return 0;
			}
			
			// Thunderbird can put "" round part of address "rms:john.wiseman"@cantab.net
			
			ptr2 = strchr(&Address[1], '"');

			if (ptr2)
			{
				memmove(Address, &Address[1], ptr2 - &Address[1]);
				memmove(ptr2 - 1, ptr2 + 1, strlen(ptr2 + 1) + 1);

			}


		}

		return 0;
	}

	ptr1 = Address;

	while (*ptr1 == ' ') ptr1++;

	if (*ptr1 == '"')
	{
		ptr1++;
		ptr1=strlop(ptr1, '"');
		ptr2=strlop(ptr1, ' ');
		ptr1=ptr2;
	}

	if (*ptr1 == '<') ptr1++;

	ptr2 = strlop(ptr1, '>');
	strlop(ptr1, ' ');

	len = strlen(ptr1);
	memmove(Address, ptr1, len);
	Address[len] = 0;

	return 0;
}
/*
+OK POP3 server ready
USER john.wiseman
+OK please send PASS command
PASS gb7bpq
+OK john.wiseman is welcome here
STAT
+OK 6 115834

UIDL
+OK 6 messages
1 <4A0DC6E0.5020504@hb9bza.net>
2 <gul8gb+of9r@eGroups.com>
3 <1085101c9d5d0$09b15420$16f9280a@phx.gbl>
4 <gul9ms+qkht@eGroups.com>
5 <B0139742084@email.bigvalley.net>
6 <20090516011401.53DB013804@panix1.panix.com>
.
LIST
+OK 6 messages
1 7167
2 10160
3 52898
4 4746
5 20218
6 20645
.

*/

VOID ProcessPOP3ServerMessage(SocketConn * sockptr, char * Buffer, int Len)
{
	SOCKET sock;
	int i;
	struct MsgInfo * Msg;

	sock=sockptr->socket;

	WriteLogLine(NULL, '<',Buffer, Len-2, LOG_TCP);

	if(memcmp(Buffer, "CAPA",4) == 0)
	{
		SendSock(sockptr, "+OK Capability list follows");
		SendSock(sockptr, "UIDL");
		SendSock(sockptr, "EXPIRE 30");
		SendSock(sockptr, ".");
		return;
	}  

	if(memcmp(Buffer, "AUTH",4) == 0)
	{
		SendSock(sockptr, "-ERR");
		return;
	}  
	if (sockptr->State == GettingUser)
	{
		
		Buffer[Len-2]=0;
		if (Len > 15) Buffer[15]=0;

		strcpy(sockptr->CallSign, &Buffer[5]);
		
		sockptr->State = GettingPass;
		SendSock(sockptr, "+OK please send PASS command");
		return;
	}

	if (sockptr->State == GettingPass)
	{
		struct UserInfo * user = NULL;

		Buffer[Len-2]=0;
		user = LookupCall(sockptr->CallSign);

		if (user)
		{
			if (strcmp(user->pass, &Buffer[5]) == 0)
			{
				if (user->POP3Locked)
				{
					SendSock(sockptr, "-ERR Mailbox Locked");
					sockptr->State = 0;
					return;
				}

				sockptr->State = Authenticated;
				SendSock(sockptr, "+OK Authenticated");

				sockptr->POP3User = user;
				user->POP3Locked = TRUE;

				// Get Message List

				for (i=0; i<=NumberofMessages; i++)
				{
					Msg = MsgHddrPtr[i];
					
					if ((_stricmp(Msg->to, sockptr->CallSign) == 0) ||
						((_stricmp(Msg->to, "SYSOP") == 0) && (user->flags & F_SYSOP) && (Msg->type == 'P')))
					{
						if (Msg->status != 'K' && Msg->status != 'H')
						{
							sockptr->POP3Msgs = realloc(sockptr->POP3Msgs, (sockptr->POP3MsgCount+1)*4);
							sockptr->POP3Msgs[sockptr->POP3MsgCount++] = MsgHddrPtr[i];
						}
					}
				}

				return;
			}
		}

		SendSock(sockptr, "-ERR Authentication failed");
		sockptr->State = 0;
		return;
	}

	if (memcmp(Buffer, "QUIT",4) == 0)
	{
		SendSock(sockptr, "+OK Finished");

		if (sockptr->POP3User)
			sockptr->POP3User->POP3Locked = FALSE;

		return;
	}

	if (memcmp(Buffer, "NOOP",4) == 0)
	{
		SendSock(sockptr, "+OK ");
		return;
	}

//	if (memcmp(Buffer, "LAST",4) == 0)
//	{
//		SendSock(sockptr, "+OK 0");
//		return;
//	}

	if (sockptr->State != Authenticated)
	{
		SendSock(sockptr, "-ERR Need Authentication");
		sockptr->State = 0;
		return;
	}

	if (memcmp(Buffer, "STAT",4) == 0)
	{
		char reply[40];
		int i, size=0;

		for (i=0; i< sockptr->POP3MsgCount; i++)
		{
			size+=sockptr->POP3Msgs[i]->length;
		}

		sprintf_s(reply, sizeof(reply), "+OK %d %d", sockptr->POP3MsgCount, size);

		SendSock(sockptr, reply);
		return;
	}

	if (memcmp(Buffer, "UIDL",4) == 0)
	{
		char reply[40];
		int i, count=0, size=0;
		int MsgNo=1;

		SendSock(sockptr, "+OK ");

		for (i=0; i< sockptr->POP3MsgCount; i++)
		{
			sprintf_s(reply, sizeof(reply), "%d %s", i+1, sockptr->POP3Msgs[i]->bid);
			SendSock(sockptr, reply);	
		}

		SendSock(sockptr, ".");
		return;
	}

	if (memcmp(Buffer, "LIST",4) == 0)
	{
		char reply[40];
		int i, count=0, size=0;
		int MsgNo = atoi(&Buffer[4]);

		if (MsgNo)
		{
			sprintf(reply, "+OK %d %d", MsgNo, sockptr->POP3Msgs[MsgNo - 1]->length);
			SendSock(sockptr, reply);
			return;
		}


		SendSock(sockptr, "+OK ");

		for (i=0; i< sockptr->POP3MsgCount; i++)
		{
			sprintf_s(reply, sizeof(reply), "%d %d", i+1, sockptr->POP3Msgs[i]->length);
			SendSock(sockptr, reply);	
		}

		SendSock(sockptr, ".");
		return;
	}

	if (memcmp(Buffer, "RETR", 4) == 0 || memcmp(Buffer, "TOP", 3) == 0)
	{
		char * ptr;		
		char Header[120];
		int i, count=0, size=0;
		int MsgNo=1;
		char * msgbytes;
		struct MsgInfo * Msg;
		char B2From[80];
		struct UserInfo * FromUser;
		char TimeString[64];
		BOOL TOP = FALSE;

		if (memcmp(Buffer, "TOP", 3) == 0)
			TOP = TRUE;

		ptr=strlop(Buffer, ' ');			// Get Number

		i=atoi(ptr);

		if ((i > sockptr->POP3MsgCount)  || (i == 0))
		{
			SendSock(sockptr, "-ERR no such message");
			return;
		}

		Msg = sockptr->POP3Msgs[i-1];

		msgbytes = ReadMessageFile(Msg->number);

		if (msgbytes == NULL)
		{
			SendSock(sockptr, "-ERR no such message");
			return;
		}

		SendSock(sockptr, "+OK ");

		// Build an RFC822 ish header

//Received: from [69.147.65.148] by n15.bullet.sp1.yahoo.com with NNFMP; 16 May 2009 02:30:47 -0000
//Received: from [69.147.108.192] by t11.bullet.mail.sp1.yahoo.com with NNFMP; 16 May 2009 02:30:47 -0000

		FormatTime(TimeString, Msg->datecreated);

		sprintf_s(Header, sizeof(Header), "Date: %s", TimeString);
		SendSock(sockptr, Header);

		sprintf_s(Header, sizeof(Header), "To: %s", Msg->to);
		SendSock(sockptr, Header);
		
		sprintf_s(Header, sizeof(Header), "Message-ID: %s", Msg->bid);
		SendSock(sockptr, Header);

		if (_stricmp(Msg->from, "smtp:") == 0)
		{
			sprintf_s(Header, sizeof(Header), "From: smtp/%s", Msg->emailfrom);
		}
		else
		{
			if (_stricmp(Msg->from, "rms:") == 0)
			{
				sprintf_s(Header, sizeof(Header), "From: RMS/%s", Msg->emailfrom);
			}
			else
			{
				// If there is an adddress in Msg->emailfrom use it

				if (Msg->emailfrom[0] == '@')
				{
					strcpy(B2From, Msg->from);
					strcat(B2From, Msg->emailfrom);
				}
				else
				{
					// Packet Address. Mail client will need more than just a call to respond to
	
					strcpy(B2From, Msg->from);

					if (strcmp(Msg->from, "SMTP:") == 0)		// Address is in via
						strcpy(B2From, Msg->emailfrom);
					else
					{
						FromUser = LookupCall(Msg->from);

						if (FromUser)
						{
							if (FromUser->HomeBBS[0])
								sprintf(B2From, "%s@%s", Msg->from, FromUser->HomeBBS);
							else
								sprintf(B2From, "%s@%s", Msg->from, BBSName);
						}
						else
						{
							WPRecP WP = LookupWP(Msg->from);
							if (WP)
								sprintf(B2From, "%s@%s", Msg->from, WP->first_homebbs);
						}
					}	
				}
				sprintf_s(Header, sizeof(Header), "From: %s", B2From);	
			}
		}	
		SendSock(sockptr, Header);
		sprintf_s(Header, sizeof(Header), "Subject: %s", Msg->title);
		SendSock(sockptr, Header);

		if ((Msg->B2Flags & Attachments) && TOP == FALSE)
		{
			// B2 Message with Attachments. Create a Mime-Encoded Multipart message

			SendMultiPartMessage(sockptr, Msg, msgbytes);
			return;
		}

		SendSock(sockptr, "");							// Blank line before body

		if (TOP)
		{
			char * ptr1, * ptr2;

			ptr = strlop(ptr, ' ');			// Get Number of lines
			i = atoi(ptr);

			// Get first i lines of message

			ptr1 = msgbytes;
			ptr2 = --ptr1;					// Point both to char before message
			
			while(i--)
			{
				ptr2 = strchr(++ptr1, 10);

				if (ptr2 == 0)				// No more lines
					i = 0;

				ptr1 = ptr2;
			}
			if (ptr2)
				*(ptr2 + 1) = 0;
		}

		SendSock(sockptr, msgbytes);
		SendSock(sockptr, "");
		SendSock(sockptr, ".");

		free(msgbytes);

		return;

	}


	if (memcmp(Buffer, "DELE",4) == 0)
	{
		char * ptr;		
		int i;
		struct MsgInfo * Msg;

		ptr=strlop(Buffer, ' ');			// Get Number

		i=atoi(ptr);

		if ((i > sockptr->POP3MsgCount)  || (i == 0))
		{
			SendSock(sockptr, "-ERR no such message");
			return;
		}

		Msg = sockptr->POP3Msgs[i-1];

		FlagAsKilled(Msg);

		SendSock(sockptr, "+OK ");
		return;
	}


	if (memcmp(Buffer, "QUIT",4) == 0)
	{
		SendSock(sockptr, "+OK Finished");

		if (sockptr->POP3User)
			sockptr->POP3User->POP3Locked = FALSE;

		return;
	}

	SendSock(sockptr, "-ERR Unrecognised Command");

}



/* jer:
 * This is the original file, my mods were only to change the name/semantics on the b64decode function
 * and remove some dependencies.
 */
/*
	LibCGI base64 manipulation functions is extremly based on the work of Bob Tower,
	from its projec http://base64.sourceforge.net. The functions were a bit modicated. 
	Above is the MIT license from b64.c original code:

LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

                Permission is hereby granted, free of charge, to any person
                obtaining a copy of this software and associated
                documentation files (the "Software"), to deal in the
                Software without restriction, including without limitation
                the rights to use, copy, modify, merge, publish, distribute,
                sublicense, and/or sell copies of the Software, and to
                permit persons to whom the Software is furnished to do so,
                subject to the following conditions:

                The above copyright notice and this permission notice shall
                be included in all copies or substantial portions of the
                Software.

                THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
                KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
                WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
                PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
                OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
                OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
                OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
                SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE

*/
void encodeblock( unsigned char in[3], unsigned char out[4], int len )
{
    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}

void decodeblock( unsigned char in[4], unsigned char out[3] )
{   
    char Block[5];
	
	Block[0]=mycd64[in[0]];
    Block[1]=mycd64[in[1]];
    Block[2]=mycd64[in[2]];
    Block[3]=mycd64[in[3]];

	out[0] = (unsigned char ) (Block[0] << 2 | Block[1] >> 4);
    out[1] = (unsigned char ) (Block[1] << 4 | Block[2] >> 2);
    out[2] = (unsigned char ) (((Block[2] << 6) & 0xc0) | Block[3]);
}

/** 
* @ingroup libcgi_string
* @{
*/

/**
* Encodes a given tring to its base64 form.
* 
* @param *str String to convert
* @return Base64 encoded String
* @see str_base64_decode
**/
char *str_base64_encode(char *str)
{
    unsigned int i = 0, j = 0, len = strlen(str);
	char *tmp = str;
	char *result = (char *)zalloc((len+1)*4);
	
	if (!result)
		return NULL;

	while (len  > 2 )
	{
		encodeblock(&str[i], &result[j],3);
		i+=3;
		j+=4;
		len -=3;
	}
	if (len)
	{
		encodeblock(&str[i], &result[j], len);
	}

	return result;
}

SocketConn * SMTPConnect(char * Host, int Port, BOOL AMPR, struct MsgInfo * Msg, char * MsgBody)
{
	int err;
	u_long param=1;
	BOOL bcopt=TRUE;

	SocketConn * sockptr;

	SOCKADDR_IN sinx; 
	SOCKADDR_IN destaddr;
	int addrlen=sizeof(sinx);
	struct hostent * HostEnt;

	// Resolve Name if needed

	destaddr.sin_family = AF_INET; 
	destaddr.sin_port = htons(Port);

	destaddr.sin_addr.s_addr = inet_addr(Host);

	if (destaddr.sin_addr.s_addr == INADDR_NONE)
	{
		//	Resolve name to address

		 HostEnt = gethostbyname (Host);
		 
		 if (!HostEnt)
		 {
 			Logprintf(LOG_TCP, NULL, '|', "Resolve Failed for SMTP Server %s", Host);
			SMTPActive = FALSE;
			return FALSE;			// Resolve failed
		 }
		 memcpy(&destaddr.sin_addr.s_addr,HostEnt->h_addr,4);
	}

//   Allocate a Socket entry

	sockptr=malloc(sizeof(SocketConn));
	memset(sockptr, 0, sizeof (SocketConn));

	sockptr->Next=Sockets;
	Sockets=sockptr;

	sockptr->socket=socket(AF_INET,SOCK_STREAM,0);

	if (sockptr->socket == INVALID_SOCKET)
	{
  	 	return FALSE; 
	}

	sockptr->Type = SMTPClient;
	sockptr->AMPR = AMPR;

	if (AMPR)
		strcpy(sockptr->FromDomain, AMPRDomain);
	else
		strcpy(sockptr->FromDomain, MyDomain);
	
	sockptr->SMTPMsg = Msg;
	sockptr->MailBuffer = MsgBody;

	ioctlsocket (sockptr->socket, FIONBIO, &param);
 
	setsockopt (sockptr->socket, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt,4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (bind(sockptr->socket, (LPSOCKADDR) &sinx, addrlen) != 0 )
	{
		//
		//	Bind Failed
		//
	
  	 	return FALSE; 
	}

	if (connect(sockptr->socket,(LPSOCKADDR) &destaddr, sizeof(destaddr)) == 0)
	{
		//
		//	Connected successful
		//

		sockptr->State = WaitingForGreeting;

		return sockptr;
	}
	else
	{
		err=WSAGetLastError();

		if (err == WSAEWOULDBLOCK || err == 115 || err == 36)
		{
			//
			//	Connect in Progress
			//

			sockptr->State = Connecting;
			return sockptr;
		}
		else
		{
			//
			//	Connect failed
			//

			printf("SMTP Connect failed immediately\n");
			closesocket(sockptr->socket);
			ReleaseSock(sockptr->socket);
			return FALSE;

			return FALSE;
		}
	}
	return FALSE;

}

VOID ProcessSMTPClientMessage(SocketConn * sockptr, char * Buffer, int Len)
{
	SOCKET sock;

	sock=sockptr->socket;

	WriteLogLine(NULL, '<',Buffer, Len-2, LOG_TCP);

	Buffer[Len] = 0;

	if (sockptr->State == WaitingForGreeting)
	{
		if (memcmp(Buffer, "220 ",4) == 0)
		{
			if (sockptr->AMPR)
				sockprintf(sockptr, "EHLO %s", AMPRDomain);
			else
				sockprintf(sockptr, "EHLO %s", BBSName);
			sockptr->State = WaitingForHELOResponse;
		}
		else
		{
			SendSock(sockptr, "QUIT");
			sockptr->State = 0;
		}

		return;
	}

	if (sockptr->State == WaitingForHELOResponse)
	{
		if (memcmp(Buffer, "250-",4) == 0)
			return;

		if (memcmp(Buffer, "250 ",4) == 0)
		{
			if (SMTPAuthNeeded && sockptr->AMPR == FALSE)
			{
				sockprintf(sockptr, "AUTH LOGIN");
				sockptr->State = WaitingForAUTHResponse;
			}
			else
			{
				sockprintf(sockptr, "MAIL FROM: <%s@%s>", sockptr->SMTPMsg->from, sockptr->FromDomain);
				sockptr->State = WaitingForFROMResponse;
			}
		}
		else
		{
			SendSock(sockptr, "QUIT");
			sockptr->State = 0;
			SMTPActive = FALSE;

		}

		return;
	}

	if (sockptr->State == WaitingForAUTHResponse)
	{
		if (memcmp(Buffer, "334 VXN", 7) == 0)
		{
			char * Msg = str_base64_encode(ISPAccountName);
			SendSock(sockptr, Msg);
			free(Msg);
			return;
		}
		else if (memcmp(Buffer, "334 UGF", 7) == 0)
		{
			char * Msg = str_base64_encode(ISPAccountPass);
			SendSock(sockptr, Msg);
			free(Msg);
			return;
		}
		else if (memcmp(Buffer, "235 ", 4) == 0)
		{
			sockprintf(sockptr, "MAIL FROM: <%s@%s>", sockptr->SMTPMsg->from, sockptr->FromDomain);
//			sockprintf(sockptr, "MAIL FROM: <%s@%s.%s>", sockptr->SMTPMsg->from, BBSName, HRoute);
			sockptr->State = WaitingForFROMResponse;
		}

		else
		{
			SendSock(sockptr, "QUIT");
			sockptr->State = 0;
			SMTPActive = FALSE;
		}

		return;

	}


	if (sockptr->State == WaitingForFROMResponse)
	{
		if (memcmp(Buffer, "250 ",4) == 0)
		{
			sockprintf(sockptr, "RCPT TO: <%s>", sockptr->SMTPMsg->via);
			sockptr->State = WaitingForTOResponse;
		}
		else
		{
			sockptr->SMTPMsg->status = 'H';			// Hold for review
			SendSock(sockptr, "QUIT");
			sockptr->State = 0;
			SMTPActive = FALSE;
		}

		return;
	}

	if (sockptr->State == WaitingForTOResponse)
	{
		if (memcmp(Buffer, "250 ",4) == 0)
		{
			SendSock(sockptr, "DATA");
			sockptr->State = WaitingForDATAResponse;
		}
		else
		{
			sockptr->SMTPMsg->status = 'H';			// Hold for review
			SendSock(sockptr, "QUIT");
			sockptr->State = 0;
			SMTPActive = FALSE;
		}

		return;
	}

	if (sockptr->State == WaitingForDATAResponse)
	{
		if (memcmp(Buffer, "354 ",4) == 0)
		{
			sockprintf(sockptr, "To: %s", sockptr->SMTPMsg->via);
			sockprintf(sockptr, "From: %s <%s@%s>", sockptr->SMTPMsg->from, sockptr->SMTPMsg->from, sockptr->FromDomain);
			sockprintf(sockptr, "Sender: %s@%s", sockptr->SMTPMsg->from, sockptr->FromDomain);
			if (GMailMode && sockptr->AMPR == FALSE)
				sockprintf(sockptr, "Reply-To: %s+%s@%s", GMailName, sockptr->SMTPMsg->from, sockptr->FromDomain);
			else
				sockprintf(sockptr, "Reply-To: %s@%s", sockptr->SMTPMsg->from, sockptr->FromDomain);

			sockprintf(sockptr, "Subject: %s", sockptr->SMTPMsg->title);
			
			sockptr->State = WaitingForBodyResponse;

			if (sockptr->SMTPMsg->B2Flags & Attachments)
			{
				// B2 Message with Attachments. Create a Mime-Encoded Multipart message

				SendMultiPartMessage(sockptr, sockptr->SMTPMsg, sockptr->MailBuffer);
				return;
			}

			SendSock(sockptr, "");
			SendSock(sockptr, sockptr->MailBuffer);
			SendSock(sockptr, ".");

		}
		else
		{
			SendSock(sockptr, "QUIT");
			sockptr->State = 0;
			SMTPActive = FALSE;
		}

		return;
	}

	if (sockptr->State == WaitingForBodyResponse)
	{
		struct MsgInfo * Msg = sockptr->SMTPMsg;

		if (memcmp(Buffer, "250 ",  4) == 0)
		{
			// if AMPR, clear forwarding bitmap

			if (sockptr->AMPR)
			{
				// Mark mail as sent, and look for more

				struct UserInfo * bbs = sockptr->bbs;
	
				clear_fwd_bit(Msg->fbbs, bbs->BBSNumber);
				set_fwd_bit(Msg->forw, bbs->BBSNumber);

				//  Only mark as forwarded if sent to all BBSs that should have it
			
				if (memcmp(Msg->fbbs, zeros, NBMASK) == 0)
				{
					Msg->status = 'F';			// Mark as forwarded
					Msg->datechanged=time(NULL);
				}
				
				bbs->ForwardingInfo->MsgCount--;
				bbs->ForwardingInfo->Forwarding = 0;

				// See if any more

				if (bbs->ForwardingInfo->MsgCount)
					bbs->ForwardingInfo->FwdTimer = bbs->ForwardingInfo->FwdInterval; // Reschdul send
			
			}
			else
			{
				Msg->status = 'F';
				SMTPActive = FALSE;
				SMTPMsgCreated=TRUE;					// See if any more
			}
		}

		SendSock(sockptr, "QUIT");
		sockptr->State = 0;

		SMTPActive = FALSE;

		SMTPMsgCreated=TRUE;					// See if any more

		return;
	}
}	

BOOL SendtoAMPR(CIRCUIT * conn)
{
	struct MsgInfo * Msg = conn->FwdMsg;
	SocketConn * sockptr;

	char * Body;
	int toLen;
	char * tocopy;
	char * Host;
	
	// Make sure message exists

	Body = ReadMessageFile(Msg->number);

	if (Body == NULL)
	{
		FlagAsKilled(Msg);
		return FALSE;
	}
		
	toLen = strlen(Msg->via);

	tocopy = _strdup(Msg->via);

	Host = strlop(tocopy, '@');

	Logprintf(LOG_TCP, NULL, '|', "Connecting to Server %s to send Msg %d", Host, Msg->number);

	sockptr = SMTPConnect(Host, 25, TRUE, Msg, Body);
	
	free(tocopy);

	if (sockptr)
	{
		sockptr->bbs = conn->UserPointer;

		return TRUE;
	}

	return FALSE;
}

BOOL SendtoISP()
{
	// Find a message intended for the Internet and send it

	int m = NumberofMessages;
	char * Body;

	struct MsgInfo * Msg;

	if (SMTPActive)
		return FALSE;

	do
	{
		Msg=MsgHddrPtr[m];

		if ((Msg->status == 'N') && (Msg->to[0] == 0) && (Msg->from[0] != 0))
		{
			// Make sure message exists

			Body = ReadMessageFile(Msg->number);

			if (Body == NULL)
			{
				FlagAsKilled(Msg);
				return FALSE;
			}

			Logprintf(LOG_TCP, NULL, '|', "Connecting to Server %s to send Msg %d", ISPSMTPName, Msg->number);

			SMTPMsgCreated=FALSE;		// Stop any more attempts
			SMTPConnect(ISPSMTPName, ISPSMTPPort, FALSE, Msg, Body);

			SMTPActive = TRUE;

			return TRUE;
		}

		m--;

	} while (m> 0);

	return FALSE;

}


BOOL POP3Connect(char * Host, int Port)
{
	int err;
	u_long param=1;
	BOOL bcopt=TRUE;

	SocketConn * sockptr;

	SOCKADDR_IN sinx; 
	SOCKADDR_IN destaddr;
	int addrlen=sizeof(sinx);
	struct hostent * HostEnt;

	Logprintf(LOG_TCP, NULL, '|', "Connecting to POP3 Server %s", Host);

	// Resolve Name if needed

	destaddr.sin_family = AF_INET; 
	destaddr.sin_port = htons(Port);

	destaddr.sin_addr.s_addr = inet_addr(Host);

	if (destaddr.sin_addr.s_addr == INADDR_NONE)
	{
		//	Resolve name to address

		 HostEnt = gethostbyname (Host);
		 
		 if (!HostEnt)
		 {
			Logprintf(LOG_TCP, NULL, '|', "Resolve Failed for POP3 Server %s", Host);
			return FALSE;			// Resolve failed
		 }
		 memcpy(&destaddr.sin_addr.s_addr,HostEnt->h_addr,4);
	}

//   Allocate a Socket entry

	sockptr=malloc(sizeof(SocketConn));
	memset(sockptr, 0, sizeof (SocketConn));

	sockptr->Next=Sockets;
	Sockets=sockptr;

	sockptr->socket=socket(AF_INET,SOCK_STREAM,0);

	if (sockptr->socket == INVALID_SOCKET)
	{
  	 	return FALSE; 
	}

	sockptr->Type = POP3Client;
	
	ioctlsocket (sockptr->socket, FIONBIO, &param);
 
	setsockopt (sockptr->socket, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt,4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (bind(sockptr->socket, (LPSOCKADDR) &sinx, addrlen) != 0 )
	{
		//
		//	Bind Failed
		//
	
  	 	return FALSE; 
	}

	if (connect(sockptr->socket,(LPSOCKADDR) &destaddr, sizeof(destaddr)) == 0)
	{
		//
		//	Connected successful
		//

		sockptr->State = WaitingForGreeting;

		return TRUE;
	}
	else
	{
		err=WSAGetLastError();

		if (err == WSAEWOULDBLOCK || err == 115 || err == 36)
		{
			//
			//	Connect in Progressing
			//

			sockptr->State = Connecting;
			return TRUE;
		}
		else
		{
			//
			//	Connect failed
			//

			printf("Connect failed immediately %d\n", err);
			perror("POP Connect");
			closesocket(sockptr->socket);
			ReleaseSock(sockptr->socket);
			return FALSE;
		}
	}
	return FALSE;
}

VOID ProcessPOP3ClientMessage(SocketConn * sockptr, char * Buffer, int Len)
{
	SOCKET sock;
	time_t Date;
	BOOL B2Flag;

	sock=sockptr->socket;

	WriteLogLine(NULL, '<',Buffer, Len-2, LOG_TCP);

	if (sockptr->Flags == GETTINGMESSAGE)
	{
		if(memcmp(Buffer, ".\r\n", 3) == 0)
		{
			// File Message

			char * ptr1, * ptr2;
			int linelen, MsgLen;
			char MsgFrom[62], MsgTo[100], Msgtitle[62];

			// Scan headers for From: To: and Subject: Line (Headers end at blank line)

			ptr1 = sockptr->MailBuffer;
		Loop:
			ptr2 = strchr(ptr1, '\r');

			if (ptr2 == NULL)
			{
				SendSock(sockptr, "500 Eh");
				return;
			}

			linelen = ptr2 - ptr1;

			// From: "John Wiseman" <john.wiseman@ntlworld.com>
			// To: <G8BPQ@g8bpq.org.uk>
			//<To: <gm8bpq+g8bpq@googlemail.com>


			if (_memicmp(ptr1, "From:", 5) == 0)
			{
				if (linelen > 65) linelen = 65;
				memcpy(MsgFrom, &ptr1[5], linelen-5);
				MsgFrom[linelen-5]=0;
			}
			else
			if (_memicmp(ptr1, "To:", 3) == 0)
			{
				if (linelen > 99) linelen = 99;
				memcpy(MsgTo, &ptr1[4], linelen-4);
				MsgTo[linelen-4]=0;
			}
			else
			if (_memicmp(ptr1, "Subject:", 8) == 0)
			{
				if (linelen > 68) linelen = 68;
				memcpy(Msgtitle, &ptr1[9], linelen-9);
				Msgtitle[linelen-9]=0;
			}
			else
			if (_memicmp(ptr1, "Date:", 5) == 0)
			{
				struct tm rtime;
				char * Context;
				char seps[] = " ,\t\r";
				char Offset[10] = "";
				int i, HH, MM;
				char Copy[500]="";

				// Copy message, so original isn't messed up by strtok
				
				memcpy(Copy, ptr1, linelen);

				ptr1 = Copy;

				memset(&rtime, 0, sizeof(struct tm));

				// Date: Tue, 9 Jun 2009 20:54:55 +0100

				ptr1 = strtok_s(&ptr1[5], seps, &Context);	// Skip Day
				ptr1 = strtok_s(NULL, seps, &Context);		// Day

				rtime.tm_mday = atoi(ptr1);

				ptr1 = strtok_s(NULL, seps, &Context);		// Month

				for (i=0; i < 12; i++)
				{
					if (strcmp(month[i], ptr1) == 0)
					{
						rtime.tm_mon = i;
						break;
					}
				}
		
				sscanf(Context, "%04d %02d:%02d:%02d%s",
					&rtime.tm_year, &rtime.tm_hour, &rtime.tm_min, &rtime.tm_sec, Offset);

				rtime.tm_year -= 1900;

				Date = mktime(&rtime) - (time_t)_MYTIMEZONE; 
				
				if (Date == (time_t)-1)
					Date = 0;
				else
				{
					if ((Offset[0] == '+') || (Offset[0] == '-'))
					{
						MM = atoi(&Offset[3]);
						Offset[3] = 0;
						HH = atoi(&Offset[1]);
						MM = MM + (60 * HH);

						if (Offset[0] == '+')
							Date -= (60*MM);
						else
							Date += (60*MM);


					}
				}
			}

			
			if (linelen)			// Not Null line
			{
				ptr1 = ptr2 + 2;		// Skip crlf
				goto Loop;
			}

			ptr1 = sockptr->MailBuffer;

			TidyString(MsgFrom);
			_strlwr(MsgFrom);

			MsgLen = sockptr->MailSize - (ptr2 - ptr1);

			B2Flag = CheckforMIME(sockptr, sockptr->MailBuffer, &ptr2, &MsgLen);	// Will reformat message if necessary. 

			CreatePOP3Message(MsgFrom, MsgTo, Msgtitle, Date, ptr2, MsgLen, B2Flag);

			free(sockptr->MailBuffer);
			sockptr->MailBufferSize=0;
			sockptr->MailBuffer=0;
			sockptr->MailSize = 0;

			sockptr->Flags &= ~GETTINGMESSAGE;

			if (sockptr->POP3MsgCount > sockptr->POP3MsgNum++)
			{
				sockprintf(sockptr, "RETR %d", sockptr->POP3MsgNum);

				sockptr->State = WaitingForRETRResponse;
			}
			else
			{
				sockptr->POP3MsgNum = 1;
				sockprintf(sockptr, "DELE %d", sockptr->POP3MsgNum);;
				sockptr->State = WaitingForDELEResponse;
			}

			return;
		}

		if ((sockptr->MailSize + Len) > sockptr->MailBufferSize)
		{
			sockptr->MailBufferSize += 10000;
			sockptr->MailBuffer = realloc(sockptr->MailBuffer, sockptr->MailBufferSize);
	
			if (sockptr->MailBuffer == NULL)
			{
				CriticalErrorHandler("Failed to extend Message Buffer");
				shutdown(sock, 0);
				return;
			}
		}

		memcpy(&sockptr->MailBuffer[sockptr->MailSize], Buffer, Len);
		sockptr->MailSize += Len;

		return;
	}

	if (sockptr->State == WaitingForGreeting)
	{
		if (memcmp(Buffer, "+OK", 3) == 0)
		{
			sockprintf(sockptr, "USER %s", ISPAccountName);
			sockptr->State = WaitingForUSERResponse;
		}
		else
		{
			SendSock(sockptr, "QUIT");
			sockptr->State = 0;
		}

		return;
	}

	if (sockptr->State == WaitingForUSERResponse)
	{
		if (memcmp(Buffer, "+OK", 3) == 0)
		{
			sockprintf(sockptr, "PASS %s", ISPAccountPass);
			sockptr->State = WaitingForPASSResponse;
		}
		else
		{
			SendSock(sockptr, "QUIT");
			sockptr->State = WaitingForQUITResponse;
		}

		return;
	}

	if (sockptr->State == WaitingForPASSResponse)
	{
		if (memcmp(Buffer, "+OK", 3) == 0)
		{
			SendSock(sockptr, "STAT");
			sockptr->State = WaitingForSTATResponse;
		}
		else
		{
			shutdown(sock, 0);
			sockptr->State = 0;
		}

		return;
	}

	if (sockptr->State == WaitingForSTATResponse)
	{
		if (memcmp(Buffer, "+OK", 3) == 0)
		{
			int Msgs = atoi(&Buffer[3]);
			
			if (Msgs > 0)
			{
				sockptr->POP3MsgCount = Msgs;
				sockptr->POP3MsgNum = 1;
				SendSock(sockptr, "RETR 1");

				sockptr->State = WaitingForRETRResponse;

			}
			else
			{
				SendSock(sockptr, "QUIT");
				sockptr->State = WaitingForQUITResponse;
			}
		}
		else
		{
			SendSock(sockptr, "QUIT");
			sockptr->State = WaitingForQUITResponse;
		}

		return;
	}

	if (sockptr->State == WaitingForRETRResponse)
	{
		if (memcmp(Buffer, "+OK", 3) == 0)
		{
			sockptr->MailBuffer=malloc(10000);
			sockptr->MailBufferSize=10000;

			if (sockptr->MailBuffer == NULL)
			{
				CriticalErrorHandler("Failed to create POP3 Message Buffer");
				SendSock(sockptr, "QUIT");
				sockptr->State = WaitingForQUITResponse;
				shutdown(sock, 0);

				return;
			}
	
			sockptr->Flags |= GETTINGMESSAGE;
		
		}
		else
		{
			SendSock(sockptr, "QUIT");
			sockptr->State = WaitingForQUITResponse;
		}

		return;
	}
	if (sockptr->State == WaitingForDELEResponse)
	{
		if (memcmp(Buffer, "+OK", 3) == 0)
		{
			if (sockptr->POP3MsgCount > sockptr->POP3MsgNum++)
			{
				sockprintf(sockptr, "DELE %d", sockptr->POP3MsgNum);;
			}
			else
			{
				SendSock(sockptr, "QUIT");
				sockptr->Flags = WaitingForQUITResponse;
			}
		}
		else
		{
			shutdown(sock,0);
			sockptr->State = 0;
		}
		return;
	}

	if (sockptr->State == WaitingForQUITResponse)
	{
		shutdown(sock,0);
		sockptr->State = 0;
		return;
	}

	SendSock(sockptr, "QUIT");
	shutdown(sock,0);
	sockptr->State = 0;

}

CreatePOP3Message(char * From, char * To, char * MsgTitle, time_t Date, char * MsgBody, int MsgLen, BOOL B2Flag)
{
	struct MsgInfo * Msg;
	BIDRec * BIDRec;

	// Allocate a message Record slot

	Msg = AllocateMsgRecord();
		
	// Set number here so they remain in sequence
		
	Msg->number = ++LatestMsg;
	MsgnotoMsg[Msg->number] = Msg;
	Msg->length = MsgLen;


	sprintf_s(Msg->bid, sizeof(Msg->bid), "%d_%s", LatestMsg, BBSName);

	Msg->type = 'P';
	Msg->status = 'N';
	Msg->datereceived = Msg->datechanged = Msg->datecreated = time(NULL);

	if (Date)
		Msg->datecreated = Date;

	BIDRec = AllocateBIDRecord();

	strcpy(BIDRec->BID, Msg->bid);
	BIDRec->mode = Msg->type;
	BIDRec->u.msgno = LOWORD(Msg->number);
	BIDRec->u.timestamp = LOWORD(time(NULL)/86400);


	TidyString(To);
	strlop(To, '@');

	// Could have surrounding "" ""

	if (To[0] == '"')
	{
		int len = strlen(To) - 1;
		
		if (To[len] == '"')
		{
			To[len] = 0;
			memmove(To, &To[1], len);
		}
	}

	if (GMailMode)
	{
		// + separates our address and the target user

		char * GMailto;

		GMailto = strlop(To,'+');
		
		if (GMailto)
		{
			char * GmailVia = NULL;
		
			strcpy(To, GMailto);
			GmailVia = strlop(To, '|');

			if (GmailVia)
				strcpy(Msg->via, GmailVia);
		}
		else
		{
			// Someone has sent to the GMAIL account without a +. 
			// This should go to the BBS Call

			strcpy(To, BBSName);
		}
	}

	if ((_memicmp(To, "bull/", 5) == 0) || (_memicmp(To, "bull.", 5) == 0)
		|| (_memicmp(To, "bull:", 5) == 0))
	{
		Msg->type = 'B';
		memmove(To, &To[5], strlen(&To[4]));
	}

	if ((_memicmp(To, "nts/", 4) == 0) || (_memicmp(To, "nts.", 4) == 0)
		|| (_memicmp(To, "nts:", 4) == 0))
	{
		Msg->type = 'T';
		memmove(To, &To[4], strlen(&To[3]));
	}

	if (Msg->type == 'P' && Msg->via[0] == 0)
	{
		// No via - add one from HomeBBS or WP

		struct UserInfo * ToUser = LookupCall(To);

		if (ToUser)
		{
			// Local User. If Home BBS is specified, use it

			if (ToUser->HomeBBS[0])
				strcpy(Msg->via, ToUser->HomeBBS);
		}
		else
		{
			WPRec * WP = LookupWP(To);

			if (WP)
				strcpy(Msg->via, WP->first_homebbs);
		}
	}

/*	if (_memicmp(To, "rms:", 4) == 0)
	{
		via = _strlwr(strlop(To, ':'));
	}
	else if (_memicmp(To, "rms/", 4) == 0)
	{
		via = _strlwr(strlop(To, '/'));
	}
	else if (_memicmp(To, "rms.", 4) == 0)
	{
		via = _strlwr(strlop(To, '.'));
	}
*/	


	if (strlen(To) > 6) To[6]=0;

	strcpy(Msg->to, To);
	strcpy(Msg->from, "smtp:");
	strcpy(Msg->emailfrom, From);
	strcpy(Msg->title, MsgTitle);

	if(Msg->to[0] == 0)
		SMTPMsgCreated=TRUE;

	if (B2Flag)
	{
		char B2Hddr[1000];
		int B2HddrLen;
		char B2To[80];
		char * NewBody;
		char DateString[80];
		struct tm * tm;

		tm = gmtime(&Date);	
	
		sprintf(DateString, "%04d/%02d/%02d %02d:%02d",
			tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);


		if (strcmp(Msg->to, "RMS") == 0)		// Address is in via
		strcpy(B2To, Msg->via);
	else
		if (Msg->via[0])
			sprintf(B2To, "%s@%s", Msg->to, Msg->via);
		else
			strcpy(B2To, Msg->to);

		
		Msg->B2Flags = B2Msg | Attachments;

		B2HddrLen = sprintf(B2Hddr,
			"MID: %s\r\nDate: %s\r\nType: %s\r\nFrom: %s\r\nTo: %s\r\nSubject: %s\r\nMbo: %s\r\n",
			Msg->bid, DateString, "Private",
			Msg->from, B2To, Msg->title, BBSName);

		NewBody = MsgBody - B2HddrLen;

		memcpy(NewBody, B2Hddr, B2HddrLen);

		Msg->length += B2HddrLen;

		// Set up forwarding bitmap

		MatchMessagetoBBSList(Msg, 0);

		return CreateSMTPMessageFile(NewBody, Msg);
	}

	// Set up forwarding bitmap

	MatchMessagetoBBSList(Msg, 0);

	return CreateSMTPMessageFile(MsgBody, Msg);

}

VOID base64_encode(char *str, char * result, int len)
{
    unsigned int i = 0, j = 0;
	char *tmp = str;
	

	while (len  > 2 )
	{
		encodeblock(&str[i], &result[j],3);
		i+=3;
		j+=4;
		len -=3;
	}
	if (len)
	{
		encodeblock(&str[i], &result[j], len);
	}

	return;
}

void Base64EncodeAndSend(SocketConn * sockptr, UCHAR * Msg, int Len)
{
	char Base64Line[80];
	int i = Len;
	int j = 0;

	Base64Line[76] = 13;
	Base64Line[77] = 10;
	Base64Line[78] = 0;

	// Need to encode in 57 byte chunks to give 76 char lines.

	while(i > 57)
	{
		base64_encode(&Msg[j], Base64Line, 57);
		SendSock(sockptr, Base64Line);
	
		j += 57;
		i -= 57;
	}

	memset(Base64Line, 0, 79);

	base64_encode(&Msg[j], Base64Line, i);
	SendSock(sockptr, Base64Line);
	SendSock(sockptr, "");
}

VOID SendMultiPartMessage(SocketConn * sockptr, struct MsgInfo * Msg, UCHAR * msgbytes)
{
	char * ptr;		
	char Header[120];
	char Separator[33]="";
	char FileName[100][250] = {""};
	int FileLen[100];
	int Files = 0;
	int BodyLen;
	int i;

	CreateOneTimePassword(&Separator[0], "Key", 0); 
	CreateOneTimePassword(&Separator[16], "Key", 1); 

	SendSock(sockptr, "MIME-Version: 1.0");

	sprintf_s(Header, sizeof(Header), "Content-Type: multipart/mixed; boundary=\"%s\"", Separator);
	SendSock(sockptr, Header);

	SendSock(sockptr, "");							// Blank line before body

// Get Part Sizes and Filenames

	ptr = msgbytes;

	while(*ptr != 13)
	{
		char * ptr2 = strchr(ptr, 10);	// Find CR

		if (memcmp(ptr, "Body: ", 6) == 0)
		{
			BodyLen = atoi(&ptr[6]);
		}

		if (memcmp(ptr, "File: ", 6) == 0)
		{
			char * ptr1 = strchr(&ptr[6], ' ');	// Find Space

			FileLen[Files] = atoi(&ptr[6]);

			memcpy(FileName[Files++], &ptr1[1], (ptr2-ptr1 - 2));
		}
				
		ptr = ptr2;
		ptr++;
	}

	ptr += 2;			// Over Blank Line 

	// Write the none-Mime Part

	SendSock(sockptr, "This is a multi-part message in MIME format.");
	SendSock(sockptr, "");

	// Write the Body as the first part.

	sprintf_s(Header, sizeof(Header), "--%s", Separator);
	SendSock(sockptr, Header);
	SendSock(sockptr, "Content-Type: text/plain");
	SendSock(sockptr, "");

	ptr[BodyLen] = 0;

	SendSock(sockptr, ptr);

	ptr += BodyLen;		// to first file
	ptr += 2;			// Over Blank Line

	// Write Each Attachment

	for (i = 0; i < Files; i++)
	{
		sprintf_s(Header, sizeof(Header), "--%s", Separator);
		SendSock(sockptr, Header);
//		Content-Type: image/png; name="UserParams.png"
		SendSock(sockptr, "Content-Transfer-Encoding: base64");

		sprintf_s(Header, sizeof(Header), "Content-Disposition: attachment; filename=\"%s\"", FileName[i]);
		SendSock(sockptr, Header);

		SendSock(sockptr, "");

		// base64 encode and send file	

		Base64EncodeAndSend(sockptr, ptr, FileLen[i]);

		ptr += FileLen[i];
		ptr +=2;				// Over separator
	}
	
	sprintf_s(Header, sizeof(Header), "--%s--", Separator);
	SendSock(sockptr, Header);

	SendSock(sockptr, "");
	SendSock(sockptr, ".");

	free(msgbytes);

	return;
}

BOOL SendAMPRSMTP(CIRCUIT * conn)
{
	struct UserInfo * bbs = conn->UserPointer;

	while (FindMessagestoForward(conn))
	{
		if (SendtoAMPR(conn))
		{
			bbs->ForwardingInfo->Forwarding = TRUE;
			return TRUE;
		}
	}

	bbs->ForwardingInfo->Forwarding = FALSE;
	return FALSE;
}

