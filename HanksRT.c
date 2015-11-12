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


#define _CRT_SECURE_NO_DEPRECATE 
#define _USE_32BIT_TIME_T

#pragma data_seg("_BPQDATA")

#define LIBCONFIG_STATIC
#include "libconfig.h"


#ifdef LINBPQ
#include "CHeaders.h"
#endif

#include "bpqchat.h"

#ifdef LINBPQ

iconv_t link_toUTF8 = NULL;

#endif

VOID ChatClearQueue(ChatCIRCUIT * conn);
VOID ChatFlush(ChatCIRCUIT * conn);
VOID APIENTRY SendChatReport(SOCKET ChatReportSocket, char * buff, int txlen);

extern struct SEM ChatSemaphore;

extern SOCKADDR_IN Chatreportdest;

char OurNode[10];
char OurAlias[10];

#define MaxSockets 64

int MaxChatStreams=0;
ChatCIRCUIT ChatConnections[MaxSockets+1];

ULONG ChatApplMask;

int	NumberofChatStreams=0;

char ChatSignoffMsg[100];

char OtherNodesList[1000];
char ChatWelcomeMsg[1000];

char Position[81] = "";
char PopupText[260] = "";
int PopupMode = 0;


char RtKnown[MAX_PATH];
char RtUsr[MAX_PATH] = "STUsers.txt";
char RtUsrTemp[MAX_PATH] = "STUsers.tmp";

int AXIPPort = 0;

ChatCIRCUIT *circuit_hd = NULL;			// This is a chain of RT circuits. There may be others

CHATNODE *node_hd = NULL;				// Nodes

LINK *link_hd = NULL;					// Nodes we link to
TOPIC *topic_hd = NULL;

USER *user_hd = NULL;

KNOWNNODE * known_hd = NULL;

int ChatTmr = 0;

BOOL NeedStatus = FALSE;

char Verstring[80];

static void node_dec(CHATNODE *node);
static KNOWNNODE *knownnode_add(char *call);
VOID SendChatLinkStatus();
char * lookupuser(char * call);
VOID ChatSendWelcomeMsg(int Stream, ChatCIRCUIT * conn, struct UserInfo * user);

static int AutoColours[20] = {0, 4, 9, 11, 13, 16, 17, 42, 45, 50, 61, 64, 66, 72, 81, 84, 85, 86, 87, 89};

#define MaxSockets 64

extern struct SEM OutputSEM;


//#undef free
//#define   free(p) 


int ChatIsUTF8(unsigned char *ptr, int len)
{
	int n; 
	unsigned char * cpt = ptr;

	// This is simpler than the Term version, as it only handles complete lines of text, so cant get split sequences

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

#ifndef LINBPQ

char * strlop(char * buf, char delim)
{
	// Terminate buf at delim, and return rest of string

	char * ptr = strchr(buf, delim);

	if (ptr == NULL) return NULL;

	*(ptr)++=0;

	return ptr;
}


VOID * _zalloc_dbg(int len, int type, char * file, int line)
{
	// ?? malloc and clear

	void * ptr;

	ptr=_malloc_dbg(len, type, file, line);

	if (ptr == NULL)
		CriticalErrorHandler("malloc failed");

	memset(ptr, 0, len);

	return ptr;
}


VOID * _zalloc(int len)
{
	// ?? malloc and clear

	void * ptr;

	ptr=malloc(len);

	if (ptr == NULL)
		CriticalErrorHandler("malloc failed");

	memset(ptr, 0, len);

	return ptr;
}

#endif

#ifdef LINBPQ

static VOID __cdecl nprintf(ChatCIRCUIT * conn, const char * format, ...)

{
	// seems to be printf to a socket

	char buff[600];
	va_list(arglist);
	
	va_start(arglist, format);
	vsprintf(buff, format, arglist);

	nputs(conn, buff);
}

#endif

static VOID nputc(ChatCIRCUIT * conn, char chr)
{
	// Seems to send chr to socket

	WriteLogLine(conn, '>',&chr,  1, LOG_CHAT);
	ChatQueueMsg(conn, &chr, 1);
}

static VOID nputs(ChatCIRCUIT * conn, char * buf)
{
	// Seems to send buf to socket

	ChatQueueMsg(conn, buf, strlen(buf));

	if (*buf == 0x1b)
		buf += 2;				// Colour Escape
	
	WriteLogLine(conn, '>',buf,  strlen(buf), LOG_CHAT);
}

int ChatQueueMsg(ChatCIRCUIT * conn, char * msg, int len)
{
	// Add Message to queue for this connection

	//	UCHAR * OutputQueue;		// Messages to user
	//	int OutputQueueLength;		// Total Malloc'ed size. Also Put Pointer for next Message
	//	int OutputGetPointer;		// Next byte to send. When Getpointer = Quele Length all is sent - free the buffer and start again.

	// Create or extend buffer

	GetSemaphore(&OutputSEM, 0);

	if (conn->OutputQueueLength + len > 9999)
	{
		Debugprintf("Corrupt Output Queue Len %d", conn->OutputQueueLength);
		conn->OutputQueueLength = 0;
		conn->OutputGetPointer = 0;
	}

	memcpy(&conn->OutputQueue[conn->OutputQueueLength], msg, len);
	conn->OutputQueueLength += len;

	FreeSemaphore(&OutputSEM);

	return len;
}

VOID ChatSendWelcomeMsg(int Stream, ChatCIRCUIT * conn, struct UserInfo * user)
{
		if (!rtloginu (conn, TRUE))
		{
			// Already connected - close
			
			ChatFlush(conn);
			Sleep(1000);
			Disconnect(conn->BPQStream);
		}
		return;

}

VOID ChatExpandAndSendMessage(ChatCIRCUIT * conn, char * Msg, int LOG)
{
	char NewMessage[10000];
	char * OldP = Msg;
	char * NewP = NewMessage;
	char * ptr, * pptr;
	int len;
	char Dollar[] = "$";
	char CR[] = "\r";
	int Msgs = 0, Unread = 0;


	ptr = strchr(OldP, '$');

	while (ptr)
	{
		len = ptr - OldP;		// Chars before $
		memcpy(NewP, OldP, len);
		NewP += len;

		switch (*++ptr)
		{
		case 'I': // First name of the connected user.

			pptr = conn->UserPointer->Name;
			break;


		case 'U': // Callsign of the connected user.

			pptr = conn->UserPointer->Call;
			break;

		case 'W': // Inserts a carriage return.

			pptr = CR;
			break;

			break;

		default:

			pptr = Dollar;		// Just Copy $
		}

		len = strlen(pptr);
		memcpy(NewP, pptr, len);
		NewP += len;

		OldP = ++ptr;
		ptr = strchr(OldP, '$');
	}

	strcpy(NewP, OldP);

	len = RemoveLF(NewMessage, strlen(NewMessage));

	WriteLogLine(conn, '>', NewMessage,  len, LOG);
	ChatQueueMsg(conn, NewMessage, len);
}



void chat_link_out (LINK *link)
{
	int n, p;
	ChatCIRCUIT * conn;
	char Msg[80];

	for (n = NumberofChatStreams-1; n >= 0 ; n--)
	{
		conn = &ChatConnections[n];
		
		if (conn->Active == FALSE)
		{
			p = conn->BPQStream;
			memset(conn, 0, sizeof(ChatCIRCUIT));		// Clear everything
			conn->BPQStream = p;

			conn->Active = TRUE;
			circuit_new(conn,p_linkini);
			conn->u.link = link;
			conn->Flags = CHATMODE | CHATLINK;

			n=sprintf_s(Msg, sizeof(Msg), "Connecting to Chat Node %s", conn->u.link->alias);

			strcpy(conn->Callsign, conn->u.link->alias);

			WriteLogLine(conn, '|',Msg, n, LOG_CHAT);

			ConnectUsingAppl(conn->BPQStream, ChatApplMask);

			//	Connected Event will trigger connect to remote system

			return;
		}
	}

	return;
	

}


VOID saywhat(ChatCIRCUIT *circuit)
{
	nputs(circuit, "Invalid Command\r");
}

VOID saydone(ChatCIRCUIT *circuit)
{
	nputs(circuit, "Ok\r");
}

VOID strnew(char ** new, char *f1)
{
	// seems to allocate a new string, and copy the old one to it
	// how is this different to strdup??

	*new = _strdup(f1);
}

#define sl_ins_hd(link, hd) \
	if (hd == NULL)\
		hd=link;\
	else\
	{\
		link->next=hd->next;\
		hd->next=link;\
	}

BOOL matchi(char * p1, char * p2)
{
	// Return TRUE is strings match
	
	if (_stricmp(p1, p2)) 
		return FALSE;
	else
		return TRUE;
}


VOID ProcessChatLine(ChatCIRCUIT * conn, struct UserInfo * user, char* OrigBuffer, int len)
{
	ChatCIRCUIT *c;
	char * Buffer = OrigBuffer;
	WCHAR BufferW[65536];
	UCHAR BufferB[65536];

	// Convert to UTF8 if not already in UTF-8

	if (len == 73 && memcmp(&OrigBuffer[40], "                    ", 20) == 0)
	{
		// Chat Signon Message. If Topic is present, switch to it

		char * Context;
		char * Appl;
		char * topic;

		Appl = strtok_s(OrigBuffer, " ,\r", &Context);
		topic = strtok_s(NULL, " ,\r", &Context);

		if (topic == NULL)
			return;					// Just Chat
		
		// Have a Topic

		if (conn->Flags & GETTINGUSER)
		{
			// Need to log in before switching topic, so Give a dummy name here
				
			conn->Flags &=  ~GETTINGUSER;
			strcpy(user->Name, "?_name");
			ChatSendWelcomeMsg(conn->BPQStream, conn, user);
		}

		OrigBuffer[40] = 0;
		sprintf(&OrigBuffer[40],"/t %s\r", topic);
		strcpy(OrigBuffer, &OrigBuffer[40]);
		len = strlen(OrigBuffer);
	}
	else
	{ 
		// Normal input

		if (conn->Flags & GETTINGUSER)
		{
			conn->Flags &=  ~GETTINGUSER;
			memcpy(user->Name, Buffer, len-1);
			ChatSendWelcomeMsg(conn->BPQStream, conn, user);

			return;
		}
	}

	if (ChatIsUTF8(OrigBuffer, len) == FALSE)
	{
		// With Windows it is simple - convert using current codepage
		// I think the only reliable way is to convert to unicode and back

#ifndef LINBPQ

		int wlen;

		wlen = MultiByteToWideChar(CP_ACP, 0, Buffer, len, BufferW, 65536); 
		len = WideCharToMultiByte(CP_UTF8, 0, BufferW, wlen, BufferB, 63336, NULL, NULL); 
		Buffer = BufferB;

#else
		int left = 65536;
		UCHAR * BufferBP = BufferB;
		struct user_t * icu = conn->u.user;

		if (conn->rtcflags & p_user)
		{
			if (icu->iconv_toUTF8 == NULL)
			{
				icu->iconv_toUTF8 = iconv_open("UTF-8", icu->Codepage);
			
				if (icu->iconv_toUTF8 == (iconv_t)-1)
					icu->iconv_toUTF8 = iconv_open("UTF-8", "CP1252");
			}

			iconv(icu->iconv_toUTF8, NULL, NULL, NULL, NULL);		// Reset State Machine
			iconv(icu->iconv_toUTF8, &Buffer, &len, &BufferBP, &left);
		}
		else
		{
			if (link_toUTF8 == NULL)
				link_toUTF8 = iconv_open("UTF-8", "CP1252");

			iconv(link_toUTF8, NULL, NULL, NULL, NULL);		// Reset State Machine
			iconv(link_toUTF8, &Buffer, &len, &BufferBP, &left);
		}
		len = 65536 - left;
		Buffer = BufferB;

#endif

	}
	WriteLogLine(conn, '<',Buffer, len, LOG_CHAT);


	Buffer[len] = 0;

	strlop(Buffer, '\r');

	if (conn->rtcflags == p_linkwait)
	{
		//waiting for *RTL

		if (memcmp(Buffer, "*RTL", 4) == 0)
		{
			// Node - Node Connect

			if (rtloginl (conn, conn->Callsign))
			{
				// Accepted
		
				conn->Flags |= CHATLINK;
				return;
			}
			else
			{
				// Connection refused
			
				Disconnect(conn->BPQStream);
				return;
			}
		}

		if (Buffer[0] == '[' && Buffer[len-2] == ']')		// SID
			return;

		nprintf(conn, "Unexpected Message on Chat Node-Node Link - Disconnecting\r");
		ChatFlush(conn);
		Sleep(500);
		Disconnect(conn->BPQStream);
		return;
	}

	if (conn->Flags & CHATLINK)
	{
#ifndef LINBPQ

		struct _EXCEPTION_POINTERS exinfo;

		__try 
		{
			chkctl(conn, Buffer, len);
		}

		#define EXCEPTMSG "Process Chat Line"
		#include "StdExcept.c"

		Debugprintf("CHAT *** Was procesing Chat Node Message %s", Buffer);
		Disconnect(conn->BPQStream);
		CheckProgramErrors();
		}
#else
			chkctl(conn, Buffer, len);
#endif
		return;
	}

	if(conn->u.user == NULL)
	{
		// A node link, but not activated yet, or a chat console which has dosconnected

		if (conn->BPQStream != -2)
			return;	

		// Log console user in
			
		if (rtloginu (conn, TRUE))
			conn->Flags |= CHATMODE;

		return;

	}

	if ((len <6) && (memcmp(Buffer, "*RTL", 4) == 0))
	{
		// Other end thinks this is a node-node link

		Logprintf(LOG_CHAT, conn, '!', "Station %s trying to start Node Protocol, but not defined as a Node",
			conn->Callsign);

		knownnode_add(conn->Callsign);			// So it won't happen again

		Disconnect(conn->BPQStream);
		return;
	}

	if (Buffer[0] == '/')
	{
		// Process Command

		if (_memicmp(&Buffer[1], "Bye", 1) == 0)
		{
			SendUnbuffered(conn->BPQStream, ChatSignoffMsg, strlen(ChatSignoffMsg));
			
			if (conn->BPQStream < 0)
			{
				logout(conn);
				conn->Flags = 0;
				if (conn->BPQStream == -2)
					CloseConsole(conn->BPQStream);
			}
			else
				ReturntoNode(conn->BPQStream);
								
			return;
		}

		if (_memicmp(&Buffer[1], "Quit", 4) == 0)
		{
			SendUnbuffered(conn->BPQStream, ChatSignoffMsg, strlen(ChatSignoffMsg));

			if (conn->BPQStream < 0)
			{
				logout(conn);
				conn->Flags = 0;
				if (conn->BPQStream == -2)
					CloseConsole(conn->BPQStream);
			}

			else
			{
				Sleep(1000);
				Disconnect(conn->BPQStream);
			}	
			return;
		}

		if (_memicmp(&Buffer[1], "Keepalive", 4) == 0)
		{
			conn->u.user->rtflags ^= u_keepalive;
			upduser(conn->u.user);
			nprintf(conn, "Keepalive is %s\r",  (conn->u.user->rtflags & u_keepalive) ? "Enabled" : "Disabled");
			conn->u.user->lastsendtime = time(NULL);
			return;
		}
		if (_memicmp(&Buffer[1], "AUTOCHARSET", 4) == 0)
		{
			conn->u.user->rtflags ^= u_auto;
			upduser(conn->u.user);
			nprintf(conn, "Automatic Character set selection is %s\r",  (conn->u.user->rtflags & u_auto) ? "Enabled" : "Disabled");
			conn->u.user->lastsendtime = time(NULL);
			return;
		}
		if (_memicmp(&Buffer[1], "UTF-8", 3) == 0)
		{
			conn->u.user->rtflags ^= u_noUTF8;
			upduser(conn->u.user);
			nprintf(conn, "Character set is %s\r",  (conn->u.user->rtflags & u_noUTF8) ? "8 Bit" : "UTF-8");
			conn->u.user->lastsendtime = time(NULL);
			return;
		}

		if ((_memicmp(&Buffer[1], "CodePage", 2) == 0) || (_memicmp(&Buffer[1], "CP", 2) == 0))
		{
			char * Context;
			char * CP = strtok_s(&Buffer[1], " ,\r", &Context);
#ifdef LINBPQ
			iconv_t temp = NULL;
#else 
			int temp = 0;
			WCHAR TempW[10];
#endif
			CP  = strtok_s(NULL, " ,\r", &Context);
			
			if (CP == NULL || CP[0] == 0)
			{
#ifdef LINBPQ
				if (conn->u.user->Codepage[0])
					nprintf(conn, "Codepage is %s\r", conn->u.user->Codepage);
#else
				if (conn->u.user->Codepage)
					nprintf(conn, "Codepage is %d\r", conn->u.user->Codepage);
#endif
				else
					nprintf(conn, "Codepage is not set\r");

				return;
			}
			_strupr(CP);

#ifdef LINBPQ

			// Validate Code Page by trying to open an iconv descriptor
			
			temp = iconv_open("UTF-8", CP);
				
			if (temp == (iconv_t)-1)
			{
				nprintf(conn, "Invalid Codepage %s\r", CP);
				return;
			}

			iconv_close(conn->u.user->iconv_toUTF8);
			iconv_close(conn->u.user->iconv_fromUTF8);

			conn->u.user->iconv_toUTF8 = temp;
			conn->u.user->iconv_fromUTF8 = iconv_open(CP, "UTF-8");

			strcpy(conn->u.user->Codepage, CP);
			nprintf(conn, "Codepage set to %s\r", conn->u.user->Codepage);
#else
			if (CP[0] == 'C')
				CP +=2;

			// Validate by trying ot use it

			temp = atoi(CP);

			if (MultiByteToWideChar(temp, 0, "\r", 2, TempW, 10) == 0)
			{
				int err = GetLastError();

				if (err == ERROR_INVALID_PARAMETER)
				{
					nprintf(conn, "Invalid Codepage %d\r", temp);
					return;
				}
			}

			conn->u.user->Codepage = temp;
			nprintf(conn, "Codepage set to %d\r", conn->u.user->Codepage);
#endif
			upduser(conn->u.user);

			return;
		}

		if (_memicmp(&Buffer[1], "Shownames", 4) == 0)
		{
			conn->u.user->rtflags ^= u_shownames;
			upduser(conn->u.user);
			nprintf(conn, "Shownames is %s\r",  (conn->u.user->rtflags & u_shownames) ? "Enabled" : "Disabled");
			conn->u.user->lastsendtime = time(NULL);
			return;
		}

		if (_memicmp(&Buffer[1], "Time", 4) == 0)
		{
			conn->u.user->rtflags ^= u_showtime;
			upduser(conn->u.user);
			nprintf(conn, "Show Time is %s\r",  (conn->u.user->rtflags & u_showtime) ? "Enabled" : "Disabled");
			conn->u.user->lastsendtime = time(NULL);
			return;
		}

		if (_memicmp(&Buffer[1], "colours", 4) == 0)
		{
			int i =0;

			while (i < 100)
			{
				nprintf(conn, "\x1b%c%02d XXXXX\r", i + 10, i);
				i++;
				if (i == 3)
					i++;
			}
			return;
		}

		rt_cmd(conn, Buffer);

		return;
	}

	// Send message to all other connected users on same channel
		
	text_tellu(conn->u.user, Buffer, NULL, o_topic); // To local users.

	conn->u.user->lastmsgtime = time(NULL);
		
	// Send to Linked nodes

	for (c = circuit_hd; c; c = c->next)
	{
		if ((c->rtcflags & p_linked) && c->refcnt && ct_find(c, conn->u.user->topic))
			nprintf(c, "%c%c%s %s %s\r", FORMAT, id_data, OurNode, conn->u.user->call, Buffer);
	}
}

void upduser(USER *user)
{
	FILE *in, *out;
	char *c;
	char Buffer[2048];
	char *buf = Buffer;

	in = fopen(RtUsr, "r");

	if (!(in))
	{
		in = fopen(RtUsr, "w");
		fclose(in);
		in = fopen(RtUsr, "r");
	}

	out = fopen(RtUsrTemp, "w");

	if (!(in) || !(out)) return;

	while(fgets(buf, 128, in))
	{
 	  c = strchr(buf, ' ');
 	  if (c) *c = '\0';
		if (!matchi(buf, user->call))
		{
			if (c) *c = ' ';
			fputs(buf, out);
		}
	}

#ifdef LINBPQ
	fprintf(out, "%s %d %s %s¬%d¬%s\n", user->call, user->rtflags, user->name, user->qth, user->Colour, user->Codepage);
#else
	fprintf(out, "%s %d %s %s¬%d¬%d\n", user->call, user->rtflags, user->name, user->qth, user->Colour, user->Codepage);
#endif
	fclose(in);
	fclose(out);

	remove(RtUsr);
	rename(RtUsrTemp, RtUsr);
}

char * lookupuser(char * call)
{
	FILE *in;
	char *flags;
	char Buffer[2048];
	char *buf = Buffer;
	char * name;

	in = fopen(RtUsr, "r");

	if (in)
	{
		while(fgets(buf, 128, in))
		{
			strlop(buf, '\n');

			flags = strlop(buf, ' ');
			if (!matchi(buf, call)) continue;
			if (!flags) break;

			fclose(in);
			name = strlop(flags, ' ');
			strlop(name, ' ');
			return _strdup(name);
		}
		fclose(in);
	}

	return NULL;
}



void rduser(USER *user)
{
	FILE *in;
	char *name, *flags, *qth;
	char Buffer[2048];
	char *buf = Buffer;
	char * ptr;

	user->name = _strdup("?_name");
	user->qth  = _strdup("?_qth");

	in = fopen(RtUsr, "r");

	if (in)
	{
	  while(fgets(buf, 128, in))
	  {
		strlop(buf, '\n');

	    flags = strlop(buf, ' ');
		if (!matchi(buf, user->call)) continue;
		if (!flags) break;

		name = strlop(flags, ' ');
		user->rtflags = atoi(flags);

		qth = strlop(name, ' ');
		strnew(&user->name, name);

		if (!qth) break;

		// Colour Code may follow QTH, and Code Page may follow Colour
			
		ptr = strchr(qth, '¬');
		if (ptr)
		{
			*ptr++ = 0;
 			user->Colour = atoi(ptr);

			ptr = strchr(ptr, '¬');

			if (ptr)
			{
				*ptr++ = 0;
#ifdef LINBPQ
 				strcpy(user->Codepage, ptr);
#else
 				user->Codepage = atoi(ptr);
#endif
			}
		}

		strnew(&user->qth,  qth);
		break;
	}
	fclose(in);

#ifdef LINBPQ

	// Open an iconv decriptor for each conversion

	if (user->Codepage[0])
		user->iconv_toUTF8 = iconv_open("UTF-8", user->Codepage);
	else
		user->iconv_toUTF8 = (iconv_t)-1;
				
	if (user->iconv_toUTF8 == (iconv_t)-1)
		user->iconv_toUTF8 = iconv_open("UTF-8", "CP1252");
		

	if (user->Codepage[0])
		user->iconv_fromUTF8 = iconv_open(user->Codepage, "UTF-8");
	else
		user->iconv_fromUTF8 = (iconv_t)-1;

	if (user->iconv_fromUTF8 == (iconv_t)-1)
		user->iconv_fromUTF8 = iconv_open("CP1252", "UTF-8");
#endif
	}
}


void ReportBadJoin(char * ncall, char *ucall)
{
	Logprintf(LOG_CHAT, NULL, '!', "User %s Join from Node %s but already connected", ucall, ncall);
}

void ReportBadLeave(char * ncall, char * ucall)
{
	Logprintf(LOG_CHAT, NULL, '!', "Node %s reporting Node %s as a leaving user", ncall, ucall);
}


struct DUPINFO DupInfo[MAXDUPS];

static BOOL CheckforDups(ChatCIRCUIT * circuit, char * Call, char * Msg)
{
	// Primitive duplicate suppression - see if same call and text reeived in last few secons
	
	time_t Now = time(NULL);
	time_t DupCheck = Now - DUPSECONDS;
	int i, saveindex = -1;

	for (i = 0; i < MAXDUPS; i++)
	{
		if (DupInfo[i].DupTime < DupCheck)
		{
			// too old - use first if we need to save it 

			if (saveindex == -1)
			{
				saveindex = i;
			}
			continue;	
		}

		if ((strcmp(Call, DupInfo[i].DupUser) == 0) && (memcmp(Msg, DupInfo[i].DupText, strlen(DupInfo[i].DupText)) == 0))
		{
			// Duplicate, so discard, but save time

			DupInfo[i].DupTime = Now;
			Logprintf(LOG_CHAT, circuit, '?', "Duplicate Message From %s %s supressed", Call, Msg);

			return TRUE;					// Duplicate
		}
		
	}

	// Not in list

	if (saveindex == -1)  // List is full
		saveindex = MAXDUPS - 1;	// Stick on end	

	DupInfo[saveindex].DupTime = Now;
	strcpy(DupInfo[saveindex].DupUser, Call);

	if (strlen(Msg) > 99)
	{
		memcpy(DupInfo[saveindex].DupText, Msg, 99);
		DupInfo[saveindex].DupText[99] = 0;
	}
	else
		strcpy(DupInfo[saveindex].DupText, Msg);

	return FALSE;
}

void chkctl(ChatCIRCUIT *ckt_from, char * Buffer, int Len)
{
	CHATNODE    *node, *ln;
	ChatCIRCUIT *ckt_to;
	USER    *user, *su;
	char    *ncall, *ucall, *f1, *f2, *buf;

	if (Buffer[FORMAT_O] != FORMAT) return; // Not a control message.

	buf = _strdup(Buffer + DATA_O);

// FORMAT and TYPE bytes are followed by node and user callsigns.

	ncall = buf;
	ucall = strlop(buf, ' ');
	if (!ucall) { free(buf); return; } // Not a control message.

// There may be at least one field after the node and user callsigns.
// Node leave (id_unlink) has no F1.

	f1 = strlop(ucall, ' ');

// If the frame came from an unknown node ignore it.
// If the frame came from us ignore it (loop breaking).

	node = node_find(ncall);
	if (!node || matchi(ncall, OurNode)) { free(buf); return; }

	switch(Buffer[TYPE_O])
	{
		// Data from user ucall at node ncall.

		case id_data :

			// Check for dups

			if (CheckforDups(ckt_from, ucall, f1))
				break;

			user = user_find(ucall, ncall);

			if (!user)
				break;

			user->lastmsgtime = time(NULL);

			text_tellu(user, f1, NULL, o_topic);

			for (ckt_to = circuit_hd; ckt_to; ckt_to = ckt_to->next)
			{
				if ((ckt_to->rtcflags & p_linked) && ckt_to->refcnt &&
					!cn_find(ckt_to, node) && ct_find(ckt_to, user->topic))
				   nprintf(ckt_to, "%s\r", Buffer);
			}
			break;

		// User ucall at node ncall changed their Name/QTH info.

		case id_user :

			user = user_find(ucall, ncall);
			if (!user) break;
			f2 = strlop(f1, ' ');
			if (!f2) break;

			if ((strcmp(user->name, f1) == 0) && (strcmp(user->qth, f2) == 0))	// No Change?
				break;

			echo(ckt_from, node, Buffer);  // Relay to other nodes.
			strnew(&user->name, f1);
			strnew(&user->qth,  f2);
			upduser(user);
			break;

		// User ucall logged into node ncall.

		case id_join :

			user = user_find(ucall, ncall);

			if (user)
			{
				// Already Here

				ReportBadJoin(ncall, ucall);

				//if (strcmp(user->node->call, OurNode) == 0)
				//{
					// Locally connected, and at another node
				//}
		
				break;				// We have this user as an active Node
			}


			echo(ckt_from, node, Buffer);  // Relay to other nodes.
			f2 = strlop(f1, ' ');
			if (!f2) break;
			user = user_join(ckt_from, ucall, ncall, NULL, FALSE);
			if (!user) break;
			ckt_from->refcnt++;
			text_tellu_Joined(user);
			strnew(&user->name, f1);
			strnew(&user->qth, f2);
			upduser(user);
//			makelinks();					// Bring up our links if not already up

			break;

		// User ucall logged out of node ncall.

		case id_leave :

			user = user_find(ucall, ncall);
			if (!user)
			{
				Debugprintf("MAILCHAT: Leave for %s from %s when not on list", ucall, ncall);
				break;
			}

			echo(ckt_from, node, Buffer);  // Relay to other nodes.

			f2 = strlop(f1, ' ');
			if (!f2) break;

			text_tellu(user, rtleave, NULL, o_all);
			ckt_from->refcnt--;
			strnew(&user->name, f1);
			strnew(&user->qth, f2);
			upduser(user);
			user_leave(user);

			cn_dec(ckt_from, node);
			node_dec(node);

			break;

		// Node ncall lost its link to node ucall, alias f1.

		case id_unlink :

			// Only relay to other nodes if we had node. Could get loop otherwise.
			// ?? This could possibly cause stuck nodes

			ln = node_find(ucall);
			if (ln)
			{
				// is it on this circuit?

				if (cn_find(ckt_from, ln))
				{
					cn_dec(ckt_from, ln);
					node_dec(ln);
					echo(ckt_from, node, Buffer);  // Relay to other nodes if we had node. COuld get loop if
				}
				else
				{
					Debugprintf("MAILCHAT: node %s unlink for %s when not on this link", ncall, ucall);
				}
			}
			else
			{
				Debugprintf("MAILCHAT: node %s unlink for %s when not on list", ncall, ucall);
			}

			break;

		// Node ncall acquired a link to node ucall, alias f1.
		// If we are not linked, is no problem, don't link.
		// If we are linked, is a loop, do what? (Try ignore!)

		case id_link :

			ln = node_find(ucall);
			if (!ln && !matchi(ncall, OurNode))
			{
				f2 = strlop(f1, ' ');
				cn_inc(ckt_from, ucall, f1, f2);
				echo(ckt_from, node, Buffer);  // Relay to other nodes.
			}
			else
			{
				Debugprintf("MAILCHAT: node %s link for %s when already on list", ncall, ucall);
				break;
			}

			break;

		// User ucall at node ncall sent f2 to user f1.

		case id_send :
			user = user_find(ucall, ncall);
			if (!user) break;
			f2 = strlop(f1, ' ');
			if (!f2) break;
			su = user_find(f1, NULL);
			if (!su) break;

			if (su->circuit->rtcflags & p_user)
				text_tellu(user, f2, f1, o_one);
			else
				echo(ckt_from, node, Buffer);  // Relay to other nodes.
			break;

		// User ucall at node ncall changed topic.

		case id_topic :
			user = user_find(ucall, ncall);
			if (user)
			{
				if (_stricmp(user->topic->name, f1) != 0)
				{
					echo(ckt_from, node, Buffer);  //  Relay to other nodes.
					topic_chg(user, f1);
				}
			}
			break;

					
		case id_keepalive :

			ln = node_find(ncall);
			if (ln)
			{
				if (ln->Version == NULL)
					if (f1)
						ln->Version = _strdup(f1);
			}
			break;

		default :  break;
	}

	free(buf);
}

// Tell another node about nodes known by this node.
// Do not tell it about this node, the other node knows who it
// linked to (or who linked to it).
// Tell another node about users known by this node.
// Done at incoming or outgoing link establishment.

void state_tell(ChatCIRCUIT *circuit, char * Version)
{
	CHATNODE *node;
	USER *user;

	node = cn_inc(circuit, circuit->u.link->call, circuit->u.link->alias, Version);
	node_tell(node, id_link); // Tell other nodes about this new link

	// Tell the node that just linked here about nodes known on other links.

	for (node = node_hd; node; node = node->next)
	{
	  if (!matchi(node->call, OurNode))
		  node_xmit(node, id_link, circuit);
	}

	// Tell the node that just linked here about known users, and their topics.

	for (user = user_hd; user; user = user->next)
	{
		user_xmit(user, id_join, circuit);
		topic_xmit(user, circuit);
	}
}

static void circuit_free(ChatCIRCUIT *circuit)
{
	ChatCIRCUIT *c, *cp;
	CN      *ncn;
	CHATNODE    *nn;
	TOPIC   *tn;

	cp = NULL;

	for (c = circuit_hd; c; cp = c, c = c->next)
	{
		if (c == circuit)
		{
			if (cp) cp->next = c->next; else circuit_hd = c->next;

			while (c->hnode)
			{
				ncn = c->hnode->next;
				free(c->hnode);
				c->hnode = ncn;
			}	
			
			break;
		}
	}

	if (circuit_hd) return;

// RT has gone inactive. Clean up.

	while (node_hd)
	{
		nn = node_hd->next;
		free(node_hd->alias);
		free(node_hd->call);
		free(node_hd);
		node_hd = nn;
	}

	while (topic_hd)
	{
		tn = topic_hd->next;
		free(topic_hd->name);
		free(topic_hd);
		topic_hd = tn;
	}
}


// Find a node in the node list.

CHATNODE *node_find(char *call)
{
	CHATNODE *node;

	for (node = node_hd; node; node = node->next)
	{
		//if (node->refcnt && matchi(node->call, call))   I don't think this is right!!!
		if (matchi(node->call, call))
			break;
	}

	return node;
}

// Add a reference to a node.

static CHATNODE *node_inc(char *call, char *alias, char * Version)
{
	CHATNODE *node;

	node = node_find(call);

	if (!node)
	{
		knownnode_add(call);

		node = zalloc(sizeof(CHATNODE));
		sl_ins_hd(node, node_hd);
		node->call  = _strdup(call);
		node->alias = _strdup(alias);
		if (Version)
			node->Version = _strdup(Version);

//		Debugprintf("New Node Rec Created at %x for %s %s", node, node->call, node->alias);
	}

	node->refcnt++;
	return node;
}

// Remove a reference to a node.

static void node_dec(CHATNODE *node)
{
	CHATNODE *t, *tp;
	USER *user;

	ChatCIRCUIT *circuit;
	CN	*cn;

	if (--node->refcnt) return; // Other references.

	// Remove the node from the node list.

	tp = NULL;

	// Make sure there aren't any user or circuit records pointing to it

	for (user = user_hd; user; user = user->next)
	{
		if (user->node == node)
		{
			Debugprintf("Trying to remove node %s that is linked from user %s", node->call, user->call);
			node->refcnt++;
		}
	}

	for (circuit = circuit_hd; circuit; circuit = circuit->next)
	{
		if (circuit->rtcflags & p_linked)
		{
			for (cn = circuit->hnode; cn; cn = cn->next)
			{
				if (cn->node == node)
				{
					Debugprintf("Trying to remove node %s that is linked from circuit %s", node->call, circuit->Callsign);
					node->refcnt++;
				}
			}
		}
	}

	if (node->refcnt) return; // Now have other references.

	for (t = node_hd; t; tp = t, t = t->next)
	{
		if (t == node)
		{
			if (tp) tp->next = t->next; else node_hd = t->next;
			free(t->alias);
			t->alias = NULL;
			free(t->call);
			t->call = NULL;
			free(t);
			break;
		}
	}
}

// User joins a topic.

static TOPIC *topic_join(ChatCIRCUIT *circuit, char *s)
{
	CT    *ct;
	TOPIC *topic;

// Look for an existing topic.

	for (topic = topic_hd; topic; topic = topic->next)
	{
		if (matchi(topic->name, s))
			break;
	}

// Create a new topic, if needed.

	if (!topic)
	{
		topic = zalloc(sizeof(TOPIC));
		sl_ins_hd(topic, topic_hd);
		topic->name = _strdup(s);
	}

	topic->refcnt++;  // One more user in this topic.

	Logprintf(LOG_CHAT, circuit, '?', "topic_join complete user %s topic %s addr %x ref %d",
		circuit->u.user->call, topic->name, topic, topic->refcnt);


// Add the circuit / topic association.

	for (ct = circuit->topic; ct; ct = ct->next)
	{
		if (ct->topic == topic)
		{
			ct->refcnt++;
			return topic;
		}
	}

	ct = zalloc(sizeof(CT));
	sl_ins_hd(ct, circuit->topic);
	ct->topic  = topic;
	ct->refcnt = 1;
	return topic;
}

// User leaves a topic.

static void topic_leave(ChatCIRCUIT *circuit, TOPIC *topic)
{
	CT    *ct, *ctp;
	TOPIC *t,  *tp;

	Logprintf(LOG_CHAT, circuit, '?', "topic_leave user %s topic %s addr %x ref %d",
		circuit->u.user->call, topic->name, topic, topic->refcnt);

	topic->refcnt--;

	ctp = NULL;

	for (ct = circuit->topic; ct; ctp = ct, ct = ct->next)
	{
		if (ct->topic == topic)
		{
			if (!--ct->refcnt)
			{
	  			if (ctp) ctp->next = ct->next; else circuit->topic = ct->next;
				free(ct);
				break;
			}
		}
	}

	tp = NULL;

	for (t = topic_hd; t; tp = t, t = t->next)
	{
		if (!t->refcnt && (t == topic))
		{
			if (tp) tp->next = t->next; else topic_hd = t->next;
			free(t->name);
			free(t);
			break;
		}
	}
}

// Find a circuit/topic association.

int ct_find(ChatCIRCUIT *circuit, TOPIC *topic)
{
	CT *ct;

	for (ct = circuit->topic; ct; ct = ct->next)
	{
		if (ct->topic == topic)
			return ct->refcnt;
	}
	return 0;
}

// Nodes reached from each circuit. Used only if the circuit is a link.

// Remove a circuit/node association.

static void cn_dec(ChatCIRCUIT *circuit, CHATNODE *node)
{
	CN *c, *cp;

//	Debugprintf("MAILCHAT: Remove c/n %s ", node->call);

	cp = NULL;

	for (c = circuit->hnode; c; cp = c, c = c->next)
	{
		if (c->node == node)
		{
//			CN * cn;
//			int len;
//			char line[1000]="";
			
			if (--c->refcnt) 
			{
//				Debugprintf("MAILCHAT: Remove c/n Node %s still in use refcount %d", node->call, c->refcnt);
				return;			// Still in use
			}

			if (cp)
				cp->next = c->next;
			else
				circuit->hnode = c->next;

			free(c);

			break;
		}
	}

	if (c == NULL)
	{
		CN * cn;
		int len;
		char line[1000]="";
	
		// not found??
	
		Debugprintf("MAILCHAT: !! Remove c/n Node %s addr %x not found cn chain follows", node->call, node);

		line[0] = 0;

		for (cn = circuit->hnode; cn; cn = cn->next)
		{
				if (cn->node && cn->node->call)
				{
#ifndef LINBPQ
					__try
					{
#endif
						len = sprintf(line, "%s %p %s", line, cn->node, cn->node->alias);
						if (len > 80)
						{
							Debugprintf("%s", line);
							len = sprintf(line, "            ");
						}
#ifndef LINBPQ
					}
					__except(EXCEPTION_EXECUTE_HANDLER)
					{len = sprintf("%s *PE* Corrupt Rec %x %x ", line, cn, cn->node);}
#endif
				}
				else
				{
					len = sprintf("%s Corrupt Rec %x %x ", line, cn, cn->node);
				}
		}
		Debugprintf("%s", line);

	}


}

// Add a circuit/node association.

static CHATNODE *cn_inc(ChatCIRCUIT *circuit, char *call, char *alias, char * Version)
{
	CHATNODE *node;
	CN *cn;

	node = node_inc(call, alias, Version);

	for (cn = circuit->hnode; cn; cn = cn->next)
	{
		if (cn->node == node)
		{
			cn->refcnt++;
//			Debugprintf("cn_inc cn Refcount for %s->%s  incremented to %d - adding Call %s",
//				circuit->Callsign, node->call, cn->refcnt, call);

			return node;
		}
	}

	cn = zalloc(sizeof(CN));
	sl_ins_hd(cn, circuit->hnode);
	cn->node   = node;
	cn->refcnt = 1;

//	Debugprintf("cn_inc New cn for %s->%s - adding Call %s",
//				circuit->Callsign, node->call, call);

	return node;
}

// Find a circuit/node association.

static int cn_find(ChatCIRCUIT *circuit, CHATNODE *node)
{
	CN *cn;

	for (cn = circuit->hnode; cn; cn = cn->next)
	{
		if (cn->node == node)
			return cn->refcnt;
	}
	return 0;
}

// From a local user to a specific user at another node.

static void text_xmit(USER *user, USER *to, char *text)
{
	nprintf(to->circuit, "%c%c%s %s %s %s\r",
		FORMAT, id_send, OurNode, user->call, to->call, text);
}

void put_text(ChatCIRCUIT * circuit, USER * user, UCHAR * buf)
{
	UCHAR BufferB[4096];

	// Text is UTF-8 internally. If use doen't want UTF-8. convert to Node's locale

	if (circuit->u.user->rtflags & u_noUTF8)
	{
#ifndef LINBPQ
		char * Buffer = buf;
		WCHAR BufferW[4096];
		int wlen, blen;
		BOOL DefaultUsed = FALSE;
		char Subst = '?';

		wlen = MultiByteToWideChar(CP_UTF8, 0, buf, strlen(buf) + 1, BufferW, 4096); 
		blen = WideCharToMultiByte(circuit->u.user->Codepage, 0, BufferW, wlen, BufferB + 2, 4096, &Subst, &DefaultUsed); 

		if (blen == 0)				// Probably means invalid code page
			blen = WideCharToMultiByte(CP_ACP, 0, BufferW, wlen, BufferB + 2, 4096, &Subst, &DefaultUsed); 

		buf = BufferB + 2;
		BufferB[blen + 2] = 0;
#else

		int left = 4096;
		UCHAR * BufferBP = BufferB;
		int len = strlen(buf) + 1;
		struct user_t * icu = circuit->u.user;

		if (icu->iconv_fromUTF8 == NULL)
		{
			icu->iconv_fromUTF8 = iconv_open(icu->Codepage, "UTF-8");
		
			if (icu->iconv_fromUTF8 == (iconv_t)-1)
				icu->iconv_fromUTF8 = iconv_open("CP1252", "UTF-8");
		}

		iconv(icu->iconv_fromUTF8, NULL, NULL, NULL, NULL);		// Reset State Machine
		iconv(icu->iconv_fromUTF8, &buf, &len, &BufferBP, &left);

		len = 4096 - left;
		buf = BufferB;

#endif

	}


	if (circuit->u.user->rtflags & u_colour)	// Use Colour
	{
		// Put a colour header on message
												
		*(--buf) = user->Colour; 
		*(--buf) = 0x1b;
		nputs(circuit, buf);
		buf +=2;
	}	
	else	
		nputs(circuit, buf);



	circuit->u.user->lastsendtime = time(NULL);
}

void text_tellu(USER *user, char *text, char *to, int who)
{
	ChatCIRCUIT *circuit;
	UCHAR Buffer[2048];
	UCHAR *buf = &Buffer[4];
	char * Time;
	struct tm * tm;
	char Stamp[20];
	time_t T;

	T = time(NULL);
	tm = gmtime(&T);	

	sprintf(Stamp,"%02d:%02d ", tm->tm_hour, tm->tm_min);

// Send it to all connected users in the same topic.
// Echo to originator if requested.

	for (circuit = circuit_hd; circuit; circuit = circuit->next)
	{
		if (!(circuit->rtcflags & p_user)) continue;  // Circuit is a link.

		if ((circuit->u.user == user) && !(user->rtflags & u_echo)) continue;

		if (circuit->u.user->rtflags & u_showtime)
			Time = Stamp;
		else
			Time = "";
	
		if (circuit->u.user->rtflags & u_shownames)
			sprintf(buf, "%s%-6.6s %s %c %s\r", Time, user->call, user->name, (who == o_one) ? '>' : ':', text);
		else
			sprintf(buf, "%s%-6.6s %c %s\r", Time, user->call, (who == o_one) ? '>' : ':', text);


		switch(who)
		{
			case o_topic :
				if (circuit->u.user->topic == user->topic)
					put_text(circuit, user, buf);	// Send adding Colour if wanted
	
				break;

			case o_all:

				put_text(circuit, user, buf);		// Send adding Colour if wanted
	
				break;
	
			case o_one :
				if (matchi(circuit->u.user->call, to))
					put_text(circuit, user, buf);	// Send adding Colour if wanted
				break;
		}
	}
}

void text_tellu_Joined(USER * user)
{
	ChatCIRCUIT *circuit;
	UCHAR Buffer[200];
	UCHAR *buf = &Buffer[4];
	char * Time;
	struct tm * tm;
	char Stamp[20];
	time_t T;

	T = time(NULL);
	tm = gmtime(&T);	

	sprintf(Stamp,"%02d:%02d ", tm->tm_hour, tm->tm_min);

	sprintf(buf, "%s%-6.6s : %s *** Joined Chat, Topic %s", Stamp, user->call, user->name, user->topic->name);

// Send it to all connected users in the same topic.
// Echo to originator if requested.

	for (circuit = circuit_hd; circuit; circuit = circuit->next)
	{
		if (!(circuit->rtcflags & p_user)) continue;  // Circuit is a link.
		if ((circuit->u.user == user) && !(user->rtflags & u_echo)) continue;

		if (circuit->u.user->rtflags & u_showtime)
			Time = Stamp;
		else
			Time = "";

		sprintf(buf, "%s%-6.6s : %s *** Joined Chat, Topic %s", Time, user->call, user->name, user->topic->name);

		put_text(circuit, user, buf);	// Send adding Colour if wanted

		if (circuit->u.user->rtflags & u_bells)
			if (circuit->BPQStream < 0) // Console
			{
#ifndef LINBPQ
				if (ConsHeader[1]->FlashOnConnect) FlashWindow(ConsHeader[1]->hConsole, TRUE);
#endif
				nputc(circuit, 7);
//				PlaySound ("BPQCHAT_USER_LOGIN", NULL, SND_ALIAS | SND_APPLICATION | SND_ASYNC);
			}
			else
				nputc(circuit, 7);

		nputc(circuit, 13);
	}
}
// Tell one link circuit about a local user change of topic.

static void topic_xmit(USER *user, ChatCIRCUIT *circuit)
{
	nprintf(circuit, "%c%c%s %s %s\r",
		FORMAT, id_topic, OurNode, user->call, user->topic->name);
}

// Tell another node about one known node on a link add or drop
// if that node is from some other link.

static void node_xmit(CHATNODE *node, char kind, ChatCIRCUIT *circuit)
{
#ifndef LINBPQ
	struct _EXCEPTION_POINTERS exinfo;

	__try
	{
#endif
		if (!cn_find(circuit, node))
			if (node->Version && (kind == id_link))
				nprintf(circuit, "%c%c%s %s %s %s\r", FORMAT, kind, OurNode, node->call, node->alias, node->Version);
			else
				nprintf(circuit, "%c%c%s %s %s\r", FORMAT, kind, OurNode, node->call, node->alias);

#ifndef LINBPQ
	}

	#define EXCEPTMSG "node_xmit"
	#include "StdExcept.c"

	Debugprintf("Corrupt Rec %x %x %x", node, node->call, node->alias);
	}
#endif
}

// Tell all other nodes about one node known by this node.

static void node_tell(CHATNODE *node, char kind)
{
	ChatCIRCUIT *circuit;

	for (circuit = circuit_hd; circuit; circuit = circuit->next)
	{
		if (circuit->rtcflags & p_linked)
			node_xmit(node, kind, circuit);
	}
}

// Tell another node about a user login/logout at this node.

static void user_xmit(USER *user, char kind, ChatCIRCUIT *circuit)
{
	CHATNODE *node;

	node = user->node;

	if (!cn_find(circuit, node))
		nprintf(circuit, "%c%c%s %s %s %s\r", FORMAT, kind, node->call, user->call, user->name, user->qth);
}

// Tell all other nodes about a user login/logout at this node.

static void user_tell(USER *user, char kind)
{
	ChatCIRCUIT *circuit;

	for (circuit = circuit_hd; circuit; circuit = circuit->next)
	{
		if (circuit->rtcflags & p_linked)
			user_xmit(user, kind, circuit);
	}
}

// Find the user record for call@node. Node can be NULL, meaning any node

USER *user_find(char *call, char * node)
{
	USER *user;

	for (user = user_hd; user; user = user->next)
	{
		if (node)
		{
			if (matchi(user->call, call) && matchi(user->node->call, node))
				break;
		}
		else
		{
			if (matchi(user->call, call))
			break;
		}
	}

	return user;
}

static void user_leave(USER *user)
{
	USER *t, *tp;

	topic_leave(user->circuit, user->topic);

	tp = NULL;

	for (t = user_hd; t; tp = t, t = t->next)
	{
		if (t == user)
		{
			if (tp) tp->next = t->next; else user_hd = t->next;
		
			free(t->name);
			free(t->call);
			free(t->qth);
#ifdef LINBPQ
			if (t->iconv_fromUTF8)
				iconv_close(t->iconv_fromUTF8);
			if (t->iconv_toUTF8)
				iconv_close(t->iconv_toUTF8);
#endif
			free(t);
			break;
		}
	}

	if (user_hd == NULL)
		ChatTmr = 59;					// If no users, disconnect links after 10-20 secs
}

// User changed to a different topic.

static BOOL topic_chg(USER *user, char *s)
{
	char buf[128];

	if (_stricmp(user->topic->name, s) == 0) return FALSE;			// Not Changed

	sprintf(buf, "*** Left Topic: %s", user->topic->name);
	text_tellu(user, buf, NULL, o_topic); // Tell everyone in the old topic.
	topic_leave(user->circuit, user->topic);
	user->topic = topic_join(user->circuit, s);
	sprintf(buf, "*** Joined Topic: %s", user->topic->name);
	text_tellu(user, buf, NULL, o_topic); // Tell everyone in the new topic.

	return TRUE;
}

// Create a user record for this user.

static USER *user_join(ChatCIRCUIT *circuit, char *ucall, char *ncall, char *nalias, BOOL Local)
{
	CHATNODE *node;
	USER *user;

	if (Local)
	{
		node = cn_inc(circuit, ncall, nalias, Verstring);
	}
	else
		node = cn_inc(circuit, ncall, nalias, NULL);

// Is this user already logged in at this node?

	for (user = user_hd; user; user = user->next)
	{
		if (matchi(user->call, ucall) && (user->node == node))
			return user;
	}

// User is not logged in, create a user record for them.

	user = zalloc(sizeof(USER));
	sl_ins_hd(user, user_hd);
	user->circuit = circuit;
	user->call = _strdup(ucall);
	_strupr(user->call);
	user->node = node;
	rduser(user);

	if (user->Colour == 0 || user->Colour == 11)	// None or default
	{
		// Allocate Random
		int sum = 0, i;

		for (i = 0; i < 9; i++)
		sum += user->call[i];
		sum %= 20;

		user->Colour = AutoColours[sum] + 10;		// Best 20 colours
	}

	if (circuit->rtcflags & p_user)
		circuit->u.user = user;

	user->lastmsgtime = time(NULL);

	user->topic   = topic_join(circuit, deftopic);
	return user;
}

// Link went away. We dropped it, or the other node dropped it.
// Drop nodes and users connected from this link.
// Tell other (still connected) links what was dropped.

void link_drop(ChatCIRCUIT *circuit)
{
	USER *user, *usernext;
	CN   *cn;

// So we don't try and send anything on this circuit.

	if (circuit->u.link)
		if (circuit->rtcflags == p_linkini)
			Debugprintf("Chat link %s Link Setup Failed", circuit->u.link->call);
	
	if (circuit->u.link)
		circuit->u.link->flags = p_nil;
	
	circuit->rtcflags = p_nil;

// Users connected on the dropped link are no longer connected.


	for (user = user_hd; user; user = usernext)
	{
		usernext = user->next;				// Save next pointer in case entry is free'd

		if (user->circuit == circuit)
		{
			CHATNODE *node;

			node = user->node;

			text_tellu(user, rtleave, NULL, o_all);
			user_tell(user, id_leave);
			user_leave(user);

			circuit->refcnt--;
			if (node)
				node_dec(node);
		}
	}

// Any node known from the dropped link is no longer known.

	for (cn = circuit->hnode; cn; cn = cn->next)
	{
		node_tell(cn->node, id_unlink);
		node_dec(cn->node);
	}

// The circuit is no longer used.

	circuit_free(circuit);
	NeedStatus = TRUE;
}

// Handle an incoming control frame from a linked RT system.

static void echo(ChatCIRCUIT *fc, CHATNODE *node, char * Buffer)
{
	ChatCIRCUIT *tc;

	for (tc = circuit_hd; tc; tc = tc->next)
	{
		if ((tc != fc) && (tc->rtcflags & p_linked) && !cn_find(tc, node))
			nprintf(tc, "%s\r", Buffer);
	}
}


// Add an entry to list of link partners

int rtlink (char * Call)
{
	LINK *link, *temp;
	char *c;

	_strupr(Call);
	c = strlop(Call, ':');
	if (!c) return FALSE;

	link = zalloc(sizeof(LINK));

	link->alias = _strdup(Call);
	link->call  = _strdup(c);

	if (link_hd == NULL)
		link_hd = link;
	else
	{
		temp = link_hd;
		while(temp->next)
			temp = temp->next;

		temp->next = link;
	}

	return TRUE;
}

VOID removelinks()
{
	LINK *link, *nextlink;

	for (link = link_hd; link; link = nextlink)
	{
		nextlink = link->next;
		
		free(link->alias);
		free(link->call);
		free(link);
	}
	link_hd = NULL;
}
VOID removeknown()
{
	// Save Known Nodes list and free struct
	
	KNOWNNODE *node, *nextnode;
	FILE *out;

	out = fopen(RtKnown, "w");

	for (node = known_hd; node; node = nextnode)
	{
		fprintf(out, "%s %u\n", node->call, (unsigned int)node->LastHeard);

		nextnode = node->next;
		free(node->call);
		free(node);
	}
	known_hd = NULL;

	fclose(out);
}

VOID LoadKnown()
{
	// Reload Known Nodes list 
	
	FILE *in;
	char buf[128];
	char * ptr;

	in = fopen(RtKnown, "r");

	if (in == NULL)
		return;

	while(fgets(buf, 128, in))
	{
		ptr = strchr(buf, ' ');
		if (ptr)
		{
			*(ptr) = 0;
			knownnode_add(buf);
		}
	}

	fclose(in);
}

// We don't allocate memory for circuit, but we do chain it

ChatCIRCUIT *circuit_new(ChatCIRCUIT *circuit, int flags)
{
	// Make sure circuit isn't already on list
	
	ChatCIRCUIT *c;

	circuit->rtcflags = flags;
	circuit->next = NULL;

	for (c = circuit_hd; c; c = c->next)
	{
		if (c == circuit)
		{
			Debugprintf("MAILCHAT: Attempting to add Circuit when already on list");
			return circuit;
		}
	}
	
	sl_ins_hd(circuit, circuit_hd);

	return circuit;
}

// Handle an incoming link. We should only get here if we think the station is a node.

int rtloginl (ChatCIRCUIT *conn, char * call)
{
	LINK    *link;

	if (node_find(call))
	{
		Logprintf(LOG_CHAT, conn, '|', "Refusing link with %s to prevent a loop", conn->Callsign);
		return FALSE; // Already linked.
	}

	for (link = link_hd; link; link = link->next)
	{
		if (matchi(call, link->call))
			break;
	}

	if (!link) return FALSE;           // We don't link with this system.

	if (link->flags & (p_linked | p_linkini))
	{
		// Already Linked. Used to Disconnect, but that can cause sync errors
		// Try closing old link and keeping new

		ChatCIRCUIT *c;
		int len;
		char Msg[80];

		for (c = circuit_hd; c; c = c->next)
		{
			if (c->u.link == link)
			{
				len=sprintf_s(Msg, sizeof(Msg), "Chat Node %s Connect when Connected - Old Connection Closed", call);
				WriteLogLine(conn, '|',Msg, len, LOG_CHAT);

				c->Active = FALSE;			// So we don't try to clear circuit again
				Disconnect(c->BPQStream);
				link_drop(c);
				RefreshMainWindow();
				break;
			}
		}
	}

// Accept the link request.

	circuit_new(conn, p_linked);

	nputs(conn, "OK\r");
	conn->u.link = link;
	link->flags = p_linked;
	link->delay = 0;			// Dont delay first restart
	state_tell(conn, NULL);
	nprintf(conn, "%c%c%s %s %s\r", FORMAT, id_keepalive, OurNode, conn->u.link->call, Verstring);

	NeedStatus = TRUE;

	return TRUE;
}

// User connected to chat, or did chat command from BBS

int rtloginu (ChatCIRCUIT *circuit, BOOL Local)
{
	USER *user;

// Is this user already logged in to RT somewhere else?

	user = user_find(circuit->UserPointer->Call, NULL);
	
	if (user)
	{
		// if connected at this node, kill old connection and allow new login

		if (user->node == node_find(OurNode))
		{
			nputs(circuit, "*** Already connected at this node - old session will be closed.\r");

			if (user->circuit->BPQStream < 0)
			{
				CloseConsole(user->circuit->BPQStream);	
			}
			else
			{
				Disconnect(user->circuit->BPQStream);
			}
		}
		else
			nputs(circuit, "*** Already connected at another node.\r");
		
		return FALSE;
	}

// Create the user entry.

	circuit_new(circuit, p_user);

	user = user_join(circuit, circuit->UserPointer->Call, OurNode, OurAlias, Local);
	circuit->u.user = user;

	if (strcmp(user->name, "?_name") == 0)
	{
		user->name = _strdup(circuit->UserPointer->Name);
	}
	upduser(user);

	ChatExpandAndSendMessage(circuit, ChatWelcomeMsg, LOG_CHAT);
	text_tellu_Joined(user);
	user_tell(user, id_join);
	show_users(circuit);
	user->lastsendtime = time(NULL);
//	makelinks();

	return TRUE;
}

void logout(ChatCIRCUIT *circuit)
{
	USER *user;
	CHATNODE *node;

	circuit->rtcflags = p_nil;
	user = circuit->u.user;

	if (user)			// May not have logged in if already conencted
	{
		node = user->node;

		user_tell(user, id_leave);
		text_tellu(user, rtleave, NULL, o_all);
		user_leave(user);

		// order changed so node_dec can check if a node that is about the be deleted has eny users

		if (node)
		{
			cn_dec(circuit, node);
			node_dec(node);
		}

		circuit->u.user = NULL;
	}

	circuit_free(circuit);
}

void show_users(ChatCIRCUIT *circuit)
{
	USER *user;
	char * Alias;
	char * Topic;

	int i = 0;

	// First count them

	for (user = user_hd; user; user = user->next)
	{
		i++;
	}

	nprintf(circuit, "%d Station(s) connected:\r", i);

	for (user = user_hd; user; user = user->next)
	{
		if ((user->node == 0) || (user->node->alias == 0))
			Alias = "(Corrupt Alias)";
		else
			Alias = user->node->alias;

		if ((user->topic == 0) || (user->topic->name == 0))
			Topic = "(Corrupt Topic)";
		else
			Topic = user->topic->name;

#ifndef LINBPQ
		__try 
		{
#endif
			if (circuit->u.user->rtflags & u_colour)	// Use Colour
				nprintf(circuit, "\x1b%c%-6.6s at %-9.9s %s, %s [%s] Idle for %d seconds\r",
					user->Colour, user->call, Alias, user->name, user->qth, Topic, time(NULL) - user->lastmsgtime);
			else
				nprintf(circuit, "%-6.6s at %-9.9s %s, %s [%s] Idle for %d seconds\r",
					user->call, Alias, user->name, user->qth, Topic, time(NULL) - user->lastmsgtime);
#ifndef LINBPQ
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			Debugprintf("MAILCHAT *** Program Error in show_users");
			CheckProgramErrors();
		}
#endif
	}
}


static void show_nodes(ChatCIRCUIT *circuit)
{
	CHATNODE *node;

	nputs(circuit, "Known Nodes:\r");

	for (node = node_hd; node; node = node->next)
	{
		if (node->refcnt)
			if (node->Version)
				nprintf(circuit, "%s:%s %s %u\r", node->alias, node->call, node->Version, node->refcnt);
			else
				nprintf(circuit, "%s:%s %s %u\r", node->alias, node->call, "Not Known", node->refcnt);
	}
}

// /P Command: List circuits and remote RT on them.

#define xxx "\r        "

static void show_circuits(ChatCIRCUIT *conn)
{
	ChatCIRCUIT *circuit;
	CHATNODE    *node;
	LINK *link;
	char line[1000];
	int     len;
	CN	*cn;

	int i = 0;

	// First count them

	for (node = node_hd; node; node = node->next)
	{
		i++;
	}

	nprintf(conn, "%d Node(s)\r", i);
	sprintf(line, "Here %-6.6s <-", OurAlias);

	for (node = node_hd; node; node = node->next) if (node->refcnt)
	{
		len = sprintf(line, "%s %s", line, node->alias);
		if (len > 80)
		{
			nprintf(conn, "%s\r", line);
			len = sprintf(line, "              ");
		}
	}

	nprintf(conn, "%s\r", line);

	for (circuit = circuit_hd; circuit; circuit = circuit->next)
	{
		if (circuit->rtcflags & p_linked)
		{
			len = sprintf(line, "Nodes via %-6.6s(%d) -", circuit->u.link->alias, circuit->refcnt);		
		
#ifndef LINBPQ
			__try{
				for (cn = circuit->hnode; cn; cn = cn->next)
				{
					if (cn->node && cn->node->alias)
					{
						__try
						{
							len = sprintf(line, "%s %s", line, cn->node->alias);
							if (len > 80)
							{
								nprintf(conn, "%s\r", line);
								len = sprintf(line, "            ");
							}
						}
						__except(EXCEPTION_EXECUTE_HANDLER)
							{len = sprintf(line, "%s *PE* Corrupt Rec %x %x", line, cn, cn->node);}
					}
					else
						len = sprintf(line, "%s Corrupt Rec %x %x ", line, cn, cn->node);
				}
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{len = sprintf(line, "%s *PE* Corrupt Rec %x %x ", line, cn, cn->node);}
#else
			for (cn = circuit->hnode; cn; cn = cn->next)
			{
				if (cn->node && cn->node->alias)
				{
					len = sprintf(line, "%s %s", line, cn->node->alias);
					if (len > 80)
					{
						nprintf(conn, "%s\r", line);
						len = sprintf(line, "            ");
					}
				}
				else
					len = sprintf(line, "%s Corrupt Rec %p %p ", line, cn, cn->node);
			}
#endif
			nprintf(conn, "%s\r", line);

		}
		else if (circuit->rtcflags & p_user)
			nprintf(conn, "User %-6.6s\r", circuit->u.user->call);
		else if (circuit->rtcflags & p_linkini)
		{
			if (circuit->u.link)
				nprintf(conn, "Link %-6.6s (setup)\r", circuit->u.link->alias);
			else
				nprintf(conn, "Link ?? (setup)\r");
		}
	}

	nprintf(conn, "Links Defined:\r");

	for (link = link_hd; link; link = link->next)
	{
		if (link->flags & p_linked )
			nprintf(conn, "  %-10.10s Open\r", link->call);
		else if (link->flags & (p_linked | p_linkini))
			nprintf(conn, "  %-10.10s Connecting\r", link->call);
		else
			nprintf(conn, "  %-10.10s Idle\r", link->call);
	}
}

// /T Command: List topics and users in them.

static void show_topics(ChatCIRCUIT *conn)
{
	TOPIC *topic;
	USER  *user;

	nputs(conn, "Active Topics are:\r");

	for (topic = topic_hd; topic; topic = topic->next)
	{
		nprintf(conn, "%s\r", topic->name);

		if (topic->refcnt)
		{
			nputs(conn, "  ");
			for (user = user_hd; user; user = user->next)
			{
				if (user->topic == topic)
					nprintf(conn, " %s", user->call);
			}
			nputc(conn, '\r');
		}
	}
}

static void show_users_in_topic(ChatCIRCUIT *conn)
{
	TOPIC *topic;
	USER  *user;

	nputs(conn, "Users in Topic:\r");

	topic = conn->u.user->topic;
	{
		if (topic->refcnt)
		{
			for (user = user_hd; user; user = user->next)
			{
				if (user->topic == topic)
					nprintf(conn, "%s ", user->call);
			}
			nputc(conn, '\r');
		}
	}
}

// Do a user command.

int rt_cmd(ChatCIRCUIT *circuit, char * Buffer)
{
	ChatCIRCUIT *c;
	USER    *user, *su;
	char    *f1, *f2;

	user = circuit->u.user;

//	user->lastsendtime = time(NULL);

	switch(tolower(Buffer[1]))
	{
		case 'a' :
			user->rtflags ^= u_bells;
			upduser(user);
			nprintf(circuit, "Alert %s\r",  (user->rtflags & u_bells) ? "Enabled" : "Disabled");
			return TRUE;

		case 'b' : return FALSE;

		case 'c' :
			user->rtflags ^= u_colour;
			upduser(user);
			nprintf(circuit, "Colour Mode %s\r",  (user->rtflags & u_colour) ? "Enabled" : "Disabled");
			return TRUE;

		case 'e' : 
			user->rtflags ^= u_echo;
			upduser(user);
			nprintf(circuit, "Echo %s\r",  (user->rtflags & u_echo) ? "Enabled" : "Disabled");
			return TRUE;
		
		case 'f' : makelinks(); return TRUE;

		case 'h' :
		case '?' :
			nputs(circuit, "Commands can be in upper or lower case.\r");
			nputs(circuit, "/U - Show Users.\r/N - Enter your Name.\r/Q - Enter your QTH.\r/T - Show Topics.\r");
			nputs(circuit, "/T Name - Join Topic or Create new Topic. Topic Names are not case sensitive\r/P - Show Ports and Links.\r");
			nprintf(circuit, "/A - Toggle Alert on user join - %s.\r",
				(user->rtflags & u_bells) ? "Enabled" : "Disabled");
			nprintf(circuit, "/C - Toggle Colour Mode on or off (only works on Console or BPQTerminal - %s.\r",
				(user->rtflags & u_colour) ? "Enabled" : "Disabled");
			nputs(circuit, "/Codepage CPnnnn - Set Codepage to use if UTF-9 is disabled.\r");
			nprintf(circuit, "/E - Toggle Echo - %s .\r",
				(user->rtflags & u_echo) ? "Enabled" : "Disabled");
			nprintf(circuit, "/Keepalive - Toggle sending Keepalive messages every 10 minutes - %s.\r",
				(user->rtflags & u_keepalive) ? "Enabled" : "Disabled");
			nprintf(circuit, "/ShowNames - Toggle displaying name as well as call on each message - %s\r",
				(user->rtflags & u_shownames) ? "Enabled" : "Disabled");
			nprintf(circuit, "/Auto - Toggle Automatic character set selection - %s.\r",
				(user->rtflags & u_auto) ? "Enabled" : "Disabled");
			nprintf(circuit, "/UTF-8 - Character set Selection - %s.\r",
				(user->rtflags & u_noUTF8) ? "8 Bit" : "UTF-8");
			nprintf(circuit, "/Time - Toggle displaying timestamp on each message - %s.\r",
				(user->rtflags & u_showtime) ? "Enabled" : "Disabled");
			nputs(circuit, "/S CALL Text - Send Text to that station only.\r");
			nputs(circuit, "/F - Force all links to be made.\r/K - Show Known nodes.\r");
			nputs(circuit, "/B - Leave Chat and return to node.\r/QUIT - Leave Chat and disconnect from node.\r");
			return TRUE;
		
		case 'k' : show_nodes(circuit);                 return TRUE;

		case 'n' :

			f1 = &Buffer[2];

			while ((*f1 != 0) && (*f1 == ' '))
				f1++;

			if (*f1 == 0)
			{
				nprintf(circuit, "Name is %s\r", user->name);
				return TRUE;
			}

			strnew(&user->name, f1);
			nprintf(circuit, "Name set to %s\r", user->name);
			upduser(user);
			user_tell(user, id_user);
			return TRUE;

		case 'p' : show_circuits(circuit); return TRUE;

		case 'q' :

			f1 = &Buffer[2];

			while ((*f1 != 0) && (*f1 == ' '))
				f1++;

			if (*f1 == 0)
			{
				nprintf(circuit, "QTH is %s\r", user->qth);
				return TRUE;
			}

			strnew(&user->qth, f1);
			
			nprintf(circuit, "QTH set to %s\r", user->qth);
			upduser(user);
			user_tell(user, id_user);
			return TRUE;

		case 's' :
			strcat(Buffer, "\r");
			f1 = strlop(Buffer, ' ');  // To.
			if (!f1) break;
			f2 = strlop(f1, ' ');            // Text to send.
			if (!f2) break;
			_strupr(f1);
			su = user_find(f1, NULL);

			if (!su)
			{
				nputs(circuit, "*** That user is not logged in.\r");
				return TRUE;
			}

			// Send to the desired user only.

			if (su->circuit->rtcflags & p_user)
				text_tellu(user, f2, f1, o_one);
			else
				text_xmit(user, su, f2);

			return TRUE;

		case 't' :
			f1 = strlop(Buffer, ' ');
			if (f1)
			{
				if (topic_chg(user, f1))
				{
					nprintf(circuit, "Switched to Topic %s\r", user->topic->name);
					show_users_in_topic(circuit);

					// Tell all link circuits about the change of topic.

					for (c = circuit_hd; c; c = c->next)
					{
						if (c->rtcflags & p_linked)
							topic_xmit(user, c);
					}
				}
				else
				{
					// Already in topic

					nprintf(circuit, "You were already in Topic %s\r", user->topic->name);
				}
			}
			else
			  show_topics(circuit);
			return TRUE;

		case 'u' : show_users(circuit); return TRUE;

		default  : break;
	}

	saywhat(circuit);
	return TRUE;
}

void makelinks(void)
{
	LINK *link;

	// Make the links.

	for (link = link_hd; link; link = link->next)
	{
	// Is this link already established?
		if (link->flags & (p_linked | p_linkini)) continue;

		// Already linked through some other node?
		// If so, making this link would create a loop.

		if (node_find(link->call)) continue;

		// Fire up the process to handle this link.

		if (link->delay == 0)
		{
			link->flags = p_linkini;
			link->delay = 12;			// 2 mins
			chat_link_out(link);
			return;						// One at a time
		}
		else
			link->delay--;	
	}
}

VOID node_close()
{
	// Close all Node-Node Links

	ChatCIRCUIT *circuit;

	for (circuit = circuit_hd; circuit; circuit = circuit->next)
	{
		if (circuit->rtcflags & (p_linked | p_linkini | p_linkwait))
			Disconnect(circuit->BPQStream);
	}
}

// Send Keepalives to all connected nodes

static void node_keepalive()
{
	ChatCIRCUIT *circuit;
	
	NeedStatus = TRUE;					// Send Report to Monitor

	if (user_hd)						// Any Users?
	{
		for (circuit = circuit_hd; circuit; circuit = circuit->next)
		{
			if (circuit->rtcflags & p_linked &&circuit->u.link)
				nprintf(circuit, "%c%c%s %s %s\r", FORMAT, id_keepalive, OurNode, circuit->u.link->call, Verstring);
		}
	}
	else
	{
		// No users. Close links

		node_close();
	}
}

VOID ChatTimer()
{
	// Entered every 10 seconds

	int	i = 0;
#ifndef LINBPQ
	int	len;
	CHATNODE *node;
	ChatCIRCUIT *c;
	TOPIC *topic;
	USER *user;
	time_t NOW = time(NULL);
	char Msg[256];
#endif
	GetSemaphore(&ChatSemaphore, 0);

	if (NeedStatus)
	{
		NeedStatus = FALSE;
		SendChatLinkStatus();
	}

#ifndef LINBPQ

	ClearDebugWindow();

	WritetoDebugWindow("Chat Nodes\r\n", 12);

	for (node = node_hd; node; node = node->next)
	{
		len = sprintf_s(Msg, sizeof(Msg), "%s Version %s Count %d\r\n",
			node->call, node->Version, node->refcnt);
		WritetoDebugWindow(Msg, len);

		i++;
	}

	SetDlgItemInt(hWnd, IDC_NODES, i, FALSE);

	WritetoDebugWindow("Chat Links\r\n", 12);

	i = 0;
	for (c = circuit_hd; c; c = c->next)
	{
		if (c->rtcflags & p_linked) 
		{
			char buff[1000];
			int ptr;
			CT * ct;
			ptr = sprintf_s(buff, sizeof(buff), "%s Topics: ", c->u.user->call);
	
			if (c->topic)
			{
				for (ct = c->topic; ct; ct = ct->next)
				{
					ptr+= sprintf_s(&buff[ptr], sizeof(buff) - ptr, "%s ", ct->topic->name);
				}
			}
			WritetoDebugWindow(buff, ptr);
			WritetoDebugWindow("\r\n", 2);

			i++;
		}
	}

	SetDlgItemInt(hWnd, IDC_LINKS,  i, FALSE);

	WritetoDebugWindow("Chat Topics\r\n", 12);

	i = 0;
	for (topic = topic_hd; topic; topic = topic->next)
	{
		len = sprintf_s(Msg, sizeof(Msg), "%s %d\r\n", topic->name, topic->refcnt); 
		WritetoDebugWindow(Msg, len);
		i++;
	}

	WritetoDebugWindow("Chat Users\r\n", 12);

	i = 0;
	for (user = user_hd; user; user = user->next)
	{
		len = sprintf_s(Msg, sizeof(Msg), "%s Topic %s\r\n", user->call,
			(user->topic) ? user->topic->name : "** Missing Topic **"); 
		WritetoDebugWindow(Msg, len);
		i++;

		if (user->circuit && user->circuit->rtcflags & p_user)	// Local User
		{
			if ((NOW - user->lastmsgtime) > 7200)
			{
				nprintf(user->circuit, "*** Disconnected - Idle time exceeded\r");
				Sleep(1000);

				if (user->circuit->BPQStream < 0)
				{
					CloseConsole(user->circuit->BPQStream);	
					break;
				}
				else
				{
					Disconnect(user->circuit->BPQStream);
					break;
				}
			}

			if (user->rtflags & u_keepalive && (NOW - user->lastsendtime) > 600)
			{
				nprintf(user->circuit, "Chat Keepalive\r");
				user->lastsendtime = NOW;
			}
		}
	}

	SetDlgItemInt(hWnd, IDC_USERS, i, FALSE);

#endif

	ChatTmr++;

	if (user_hd)				// Any Users?
		makelinks();

	if (ChatTmr > 60) // 10 Mins
	{
		ChatTmr = 1;
		node_keepalive();
	}

	FreeSemaphore(&ChatSemaphore);
}

VOID FreeChatMemory()
{
	removelinks();
	removeknown();
}

// Find a call in the known node list.

KNOWNNODE *knownnode_find(char *call)
{
	KNOWNNODE *node;

	for (node = known_hd; node; node = node->next)
	{
		if (matchi(node->call, call))
			break;
	}

	return node;
}

// Add a known node.

static KNOWNNODE *knownnode_add(char *call)
{
	KNOWNNODE *node;

	node = knownnode_find(call);

	if (!node)
	{
		node = zalloc(sizeof(KNOWNNODE));
		sl_ins_hd(node, known_hd);
		node->call  = _strdup(call);
	}

	node->LastHeard = time(NULL);
	return node;
}

static char UIDEST[10] = "DUMMY";
static char AXDEST[7];
static char ChatMYCALL[7];

#pragma pack(1)


typedef struct _MESSAGEX
{
//	BASIC LINK LEVEL MESSAGE BUFFER LAYOUT

	struct _MESSAGE * CHAIN;

	UCHAR	PORT;
	USHORT	LENGTH;

	UCHAR	DEST[7];
	UCHAR	ORIGIN[7];

//	 MAY BE UP TO 56 BYTES OF DIGIS

	UCHAR	CTL;
	UCHAR	PID;
	UCHAR	DATA[256];

}MESSAGEX, *PMESSAGEX;

#pragma pack()

SOCKET ChatReportSocket = 0;


VOID SetupChat()
{
	u_long param=1;
	BOOL bcopt=TRUE;

	ConvToAX25(OurNode, ChatMYCALL);
	ConvToAX25(UIDEST, AXDEST);

	sprintf(Verstring, "%d.%d.%d.%d",  Ver[0], Ver[1], Ver[2], Ver[3]);

	LoadKnown();

	ChatReportSocket = socket(AF_INET,SOCK_DGRAM,0);

	if (ChatReportSocket == INVALID_SOCKET)
	{
		Debugprintf("Failed to create Chat Reporting socket");
		ChatReportSocket = 0;
  	 	return; 
	}

	ioctlsocket (ChatReportSocket, FIONBIO, &param);
	setsockopt (ChatReportSocket, SOL_SOCKET, SO_BROADCAST, (const char FAR *)&bcopt,4);
}


VOID Send_MON_Datagram(UCHAR * Msg, DWORD Len)
{
	MESSAGEX AXMSG;
	PMESSAGEX AXPTR = &AXMSG;

	if (Len > 256)
	{
		Debugprintf("Send_MON_Datagram Error Msg = %s Len = %d", Msg, Len);
		return;
	}

//	ConvToAX25("GM4OAS-5", ChatMYCALL);

	// Block includes the Msg Header (7 bytes), Len Does not!

	memcpy(AXPTR->DEST, AXDEST, 7);
	memcpy(AXPTR->ORIGIN, ChatMYCALL, 7);
	AXPTR->DEST[6] &= 0x7e;			// Clear End of Call
	AXPTR->DEST[6] |= 0x80;			// set Command Bit

	AXPTR->ORIGIN[6] |= 1;			// Set End of Call
	AXPTR->CTL = 3;		//UI
	AXPTR->PID = 0xf0;
	memcpy(AXPTR->DATA, Msg, Len);

	SendChatReport(ChatReportSocket, (char *)&AXMSG.DEST, Len + 16);

	return;

}

VOID SendChatLinkStatus()
{
	char Msg[256] = {0};
	LINK * link;
	int len = 0;
	ChatCIRCUIT *circuit;

	if (ChatApplNum == 0)
		return;

//	if (AXIPPort == 0)
//		return;

	if (ChatMYCALL[0] == 0)
		return;

	for (link = link_hd; link; link = link->next)
	{
		if (link->flags & p_linked)
		{
			// Verify connection

			for (circuit = circuit_hd; circuit; circuit = circuit->next)
			{
				if (strcmp(circuit->Callsign, link->alias) == 0)
				{
					if (circuit->Active == 0)
					{
						// BPQ Session is dead - Simulate a Disconnect

						circuit->Active = TRUE;				// So disconnect will work
						Disconnected(circuit->BPQStream);
						NeedStatus = TRUE;					// Reenter
						return;								// Link Chain has changed
					}
					break;
				}
			}

			if (circuit == 0)
			{
				// No BPQ Session - is the only answer to restart the node?

	//			Logprintf(LOG_DEBUGx, NULL, '!', "Stuck Chat Sesion Detected");
	//			Logprintf(LOG_DEBUGx, NULL, '!', "Chat is a mess - forcing a restart");
	//			ProgramErrors = 26;
	//			CheckProgramErrors();
			}
		}

		len = sprintf(Msg, "%s%s %c ", Msg, link->call, '0' + link->flags);

		if (len > 240)
			break;
	}
	Msg[len++] = '\r';

	Send_MON_Datagram(Msg, len);
}

VOID ClearChatLinkStatus()
{
	LINK * link;

	for (link = link_hd; link; link = link->next)
	{
		link->flags = 0;
	}
}

int ProcessConnecting(ChatCIRCUIT * circuit, char * Buffer, int Len)
{
	WriteLogLine(circuit, '<' ,Buffer, Len-1, LOG_CHAT);

	Buffer = _strupr(Buffer);

	if (memcmp(Buffer, "[BPQCHATSERVER-", 15) == 0)
	{
		char * ptr = strchr(Buffer, ']');
		if (ptr)
		{
			*ptr = 0;
			strcpy(circuit->FBBReplyChars, &Buffer[15]);
		}
		else
			circuit->FBBReplyChars[0] = 0;

		return 0;
	}

	if (memcmp(Buffer, "OK", 2) == 0)
	{
		// Make sure node isn't known. There is a window here that could cause a loop

		if (node_find(circuit->u.link->call))
		{
			Logprintf(LOG_CHAT, circuit, '|', "Dropping link with %s to prevent a loop", circuit->Callsign);
			Disconnect(circuit->BPQStream);
			return FALSE;
		}

		circuit->u.link->flags = p_linked;
 	  	circuit->rtcflags = p_linked;
		state_tell(circuit, circuit->FBBReplyChars);
		NeedStatus = TRUE;

		return TRUE;
	}

	
	if (strstr(Buffer, "CONNECTED") || strstr(Buffer, "LINKED"))
	{
		// Connected - Send *RTL 
		
		nputs(circuit, "*RTL\r");  // Log in to the remote RT system.
		nprintf(circuit, "%c%c%s %s %s\r", FORMAT, id_keepalive, OurNode, circuit->u.link->call, Verstring);

		return TRUE;

	}

	if (strstr(Buffer, "BUSY") || strstr(Buffer, "FAILURE") || strstr(Buffer, "DOWNLINK")|| strstr(Buffer, "SORRY"))
	{
		link_drop(circuit);
		Disconnect(circuit->BPQStream);
	}
	
	return FALSE;

}


#ifdef LINBPQ

//	LINCHAT specific code

extern struct SEM OutputSEM;

static config_t cfg;
static config_setting_t * group;

extern char pgm[256];

char ChatSYSOPCall[50] = "";

VOID ChatSendWelcomeMsg(int Stream, ChatCIRCUIT * conn, struct UserInfo * user);


int ChatConnected(Stream)
{
	int n;
	ChatCIRCUIT * conn;
	struct UserInfo * user = NULL;
	char callsign[10];
	int port, paclen, maxframe, l4window;
	char ConnectedMsg[] = "*** CONNECTED    ";
	char Msg[100];
	LINK    *link;
	KNOWNNODE *node;

	for (n = 0; n < NumberofChatStreams; n++)
	{
  		conn = &ChatConnections[n];
		
		if (Stream == conn->BPQStream)
		{
			if (conn->Active)
			{
				// Probably an outgoing connect
		
				if (conn->rtcflags == p_linkini)
				{
					conn->paclen = 236;
					nprintf(conn, "c %s\r", conn->u.link->call);
					return 0;
				}
			}
	
			memset(conn, 0, sizeof(ChatCIRCUIT));		// Clear everything
			conn->Active = TRUE;
			conn->BPQStream = Stream;

			conn->Secure_Session = GetConnectionInfo(Stream, callsign,
				&port, &conn->SessType, &paclen, &maxframe, &l4window);

			if (paclen == 0)
				paclen = 256;

			conn->paclen = paclen;

			strlop(callsign, ' ');		// Remove trailing spaces

			memcpy(conn->Callsign, callsign, 10);

			strlop(callsign, '-');		// Remove any SSID

			user = zalloc(sizeof(struct UserInfo));

			strcpy(user->Call, callsign);

			conn->UserPointer = user;

			n=sprintf_s(Msg, sizeof(Msg), "Incoming Connect from %s", user->Call);
			
			// Send SID and Prompt

			WriteLogLine(conn, '|',Msg, n, LOG_CHAT);
			conn->Flags |= CHATMODE;

			nprintf(conn, ChatSID, Ver[0], Ver[1], Ver[2], Ver[3]);

			// See if from a defined node
				
			for (link = link_hd; link; link = link->next)
			{
				if (matchi(conn->Callsign, link->call))
				{
					conn->rtcflags = p_linkwait;
					return 0;						// Wait for *RTL
				}
			}

			// See if from a previously known node

			node = knownnode_find(conn->Callsign);

			if (node)
			{
				// A node is trying to link, but we don't have it defined - close

				Logprintf(LOG_CHAT, conn, '!', "Node %s connected, but is not defined as a Node - closing",
					conn->Callsign);

				nprintf(conn, "Node %s does not have %s defined as a node to link to - closing.\r",
					OurNode, conn->Callsign);

				ChatFlush(conn);

				Sleep(500);

				Disconnect(conn->BPQStream);

				return 0;
			}

			if (user->Name[0] == 0)
			{
				char * Name = lookupuser(user->Call);

				if (Name)
				{
					if (strlen(Name) > 17)
						Name[17] = 0;

					strcpy(user->Name, Name);
					free(Name);
				}
				else
				{
					conn->Flags |= GETTINGUSER;
					nputs(conn, NewUserPrompt);
					return TRUE;
				}
			}

			ChatSendWelcomeMsg(Stream, conn, user);
			RefreshMainWindow();
			ChatFlush(conn);
			
			return 0;
		}
	}

	return 0;
}

int ChatDisconnected (ChatCIRCUIT * conn)
{
	struct UserInfo * user = NULL;
	int Stream = conn->BPQStream;
	char Msg[255];
	int len;

	if (conn->Active == FALSE)
		return 0;

	ChatClearQueue(conn);

	conn->Active = FALSE;
	
	if (conn->Flags & CHATMODE)
	{
		if (conn->Flags & CHATLINK && conn->u.link)
		{
			len=sprintf_s(Msg, sizeof(Msg), "Chat Node %s Disconnected", conn->u.link->call);
			WriteLogLine(conn, '|',Msg, len, LOG_CHAT);
			link_drop(conn);
		}
		else
		{
			len=sprintf_s(Msg, sizeof(Msg), "Chat User %s Disconnected", conn->Callsign);
			WriteLogLine(conn, '|',Msg, len, LOG_CHAT);

			logout(conn);
	
		}

		conn->Flags = 0;
		conn->u.link = NULL;
		conn->UserPointer = NULL;	
		return 0;
	}

	return 0;
}

int ChatDoReceivedData(ChatCIRCUIT * conn)
{
	int count, InputLen;
	UINT MsgLen;
	int Stream = conn->BPQStream;
	struct UserInfo * user;
	char * ptr, * ptr2;
	char Buffer[10000];


	// May have several messages per packet, or message split over packets

	if (conn->InputLen + 1000 > 10000)	// Shouldnt have lines longer  than this in text mode
		conn->InputLen = 0;				// discard	
				
	GetMsg(Stream, &conn->InputBuffer[conn->InputLen], &InputLen, &count);

	if (InputLen == 0) return 0;

	conn->Watchdog = 900;				// 15 Minutes
	conn->InputLen += InputLen;

loop:

	if (conn->InputLen == 1 && conn->InputBuffer[0] == 0)		// Single Null
	{
		conn->InputLen = 0;
		return 0;
	}

	ptr = memchr(conn->InputBuffer, '\r', conn->InputLen);

	if (ptr)	//  CR in buffer
	{
		user = conn->UserPointer;
				
		ptr2 = &conn->InputBuffer[conn->InputLen];
					
		if (++ptr == ptr2)
		{
			// Usual Case - single meg in buffer

				if (conn->rtcflags == p_linkini)		// Chat Connect
					ProcessConnecting(conn, conn->InputBuffer, conn->InputLen);
				else
					ProcessChatLine(conn, user, conn->InputBuffer, conn->InputLen);
			conn->InputLen=0;
		}
		else
		{
			// buffer contains more that 1 message

			MsgLen = conn->InputLen - (ptr2-ptr);

			memcpy(Buffer, conn->InputBuffer, MsgLen);
						
			if (conn->rtcflags == p_linkini)
				ProcessConnecting(conn, Buffer, MsgLen);
			else
				ProcessChatLine(conn, user, Buffer, MsgLen);
						
			if (*ptr == 0 || *ptr == '\n')
			{
				/// CR LF or CR Null

				ptr++;
				conn->InputLen--;
			}

			memmove(conn->InputBuffer, ptr, conn->InputLen-MsgLen);
			conn->InputLen -= MsgLen;

			goto loop;

		}
	}
	return 0;
}


int ChatPollStreams()
{
	int state,change;
	ChatCIRCUIT * conn;
	int n;
	struct UserInfo * user = NULL;
	char ConnectedMsg[] = "*** CONNECTED    ";

	for (n = 0; n < NumberofChatStreams; n++)
	{
  		conn = &ChatConnections[n];
		
		SessionState(conn->BPQStream, &state, &change);
		
		if (change == 1)
		{
			if (state == 1) // Connected	
			{
				GetSemaphore(&ConSemaphore, 0);
				ChatConnected(conn->BPQStream);
				FreeSemaphore(&ConSemaphore);
			}
			else
			{
				GetSemaphore(&ConSemaphore, 0);
				ChatDisconnected(conn);
				FreeSemaphore(&ConSemaphore);
			}
		}

		ChatDoReceivedData(conn);
	}
	
	return 0;
}


BOOL GetChatConfig(char * ConfigName)
{
	config_init(&cfg);

	/* Read the file. If there is an error, report it and exit. */
	
	if(! config_read_file(&cfg, ConfigName))
	{
		fprintf(stderr, "%d - %s\n",
			config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return(EXIT_FAILURE);
	}

	group = config_lookup (&cfg, "Chat");

	if (group == NULL)
		return EXIT_FAILURE;

	ChatApplNum = GetIntValue(group, "ApplNum");
	MaxChatStreams = GetIntValue(group, "MaxStreams");
	GetStringValue(group, "OtherChatNodes", OtherNodesList);
	GetStringValue(group, "ChatWelcomeMsg", ChatWelcomeMsg);
	GetStringValue(group, "MapPosition", Position);
	GetStringValue(group, "MapPopup", PopupText);
	PopupMode = GetIntValue(group, "PopupMode");


	return EXIT_SUCCESS;
}

VOID SaveChatConfig(char * ConfigName)
{
	config_setting_t *root, *group;

	//	Get rid of old config before saving
	
	config_init(&cfg);

	root = config_root_setting(&cfg);

	group = config_setting_add(root, "Chat", CONFIG_TYPE_GROUP);

	SaveIntValue(group, "ApplNum", ChatApplNum);
	SaveIntValue(group, "MaxStreams", MaxChatStreams);
	SaveStringValue(group, "OtherChatNodes", OtherNodesList);
	SaveStringValue(group, "ChatWelcomeMsg", ChatWelcomeMsg);

	SaveStringValue(group, "MapPosition", Position);
	SaveStringValue(group, "MapPopup", PopupText);
	SaveIntValue(group, "PopupMode", PopupMode);

	if(! config_write_file(&cfg, ConfigName))
	{
		fprintf(stderr, "Error while writing file.\n");
		config_destroy(&cfg);
		return;
	}
	config_destroy(&cfg);
}

BOOL ChatInit()
{
	char * ptr1 = GetApplCall(ChatApplNum);
	char * ptr2;
	char * Context;
	int i;
	ChatCIRCUIT * conn;

	if (*ptr1 < 0x21)
	{
		printf("No APPLCALL for Chat APPL\n");
		return FALSE;
	}
			
	memcpy(OurNode, ptr1, 10);
	strlop(OurNode, ' ');

	ptr1 = GetApplAlias(ChatApplNum);
	memcpy(OurAlias, ptr1,10);
	strlop(OurAlias, ' ');

	if (ChatSYSOPCall[0] == 0)
	{
		strcpy(ChatSYSOPCall, OurNode);
		strlop(ChatSYSOPCall, '-');
	}

	sprintf(ChatSignoffMsg, "73 de %s\r", ChatSYSOPCall);

	if (ChatWelcomeMsg[0] == 0)
		sprintf(ChatWelcomeMsg, "%s's Chat Server.$WType /h for command summary.$WBringing up links to other nodes.$W"
			"This may take a minute or two.$WThe /p command shows what nodes are linked.$W", ChatSYSOPCall);

	ChatApplMask = 1<<(ChatApplNum-1);
		
	// Set up other nodes list. rtlink messes with the string so pass copy
	
	ptr2 = ptr1 = strtok_s(_strdup(OtherNodesList), " ,\r", &Context);

	while (ptr1)
	{
		rtlink(ptr1);			
		ptr1 = strtok_s(NULL, " ,\r", &Context);
	}

	free(ptr2);

	SetupChat();

	// Allocate Streams

	strcpy(pgm, "CHAT");

	for (i = 0; i < MaxChatStreams; i++)
	{
		conn = &ChatConnections[i];
		conn->BPQStream = FindFreeStream();

		if (conn->BPQStream == 255) break;

		NumberofChatStreams++;

		SetAppl(conn->BPQStream, 3, ChatApplMask);
		Disconnect(conn->BPQStream);
	}

	strcpy(pgm, "LINBPQ");

	return TRUE;
}


void ChatFlush(ChatCIRCUIT * conn)
{
	int tosend, len, sent;
	
	// Try to send data to user. May be stopped by user paging or node flow control

	//	UCHAR * OutputQueue;		// Messages to user
	//	int OutputQueueLength;		// Total Malloc'ed size. Also Put Pointer for next Message
	//	int OutputGetPointer;		// Next byte to send. When Getpointer = Quele Length all is sent - free the buffer and start again.

	//	BOOL Paging;				// Set if user wants paging
	//	int LinesSent;				// Count when paging
	//	int PageLen;				// Lines per page


	if (conn->OutputQueue == NULL)
	{
		// Nothing to send. If Close after Flush is set, disconnect

		if (conn->CloseAfterFlush)
		{
			conn->CloseAfterFlush--;
			
			if (conn->CloseAfterFlush)
				return;

			Disconnect(conn->BPQStream);
		}

		return;						// Nothing to send
	}
	tosend = conn->OutputQueueLength - conn->OutputGetPointer;

	sent=0;

	while (tosend > 0)
	{
		if (TXCount(conn->BPQStream) > 4)
			return;						// Busy

		if (tosend <= conn->paclen)
			len=tosend;
		else
			len=conn->paclen;

		GetSemaphore(&OutputSEM, 0);


		SendUnbuffered(conn->BPQStream, &conn->OutputQueue[conn->OutputGetPointer], len);

		conn->OutputGetPointer+=len;

		FreeSemaphore(&OutputSEM);

		tosend-=len;	
		sent++;

		if (sent > 4)
			return;
	}

	// All Sent. Free buffers and reset pointers

	ChatClearQueue(conn);
}

VOID ChatClearQueue(ChatCIRCUIT * conn)
{
	if (conn->OutputQueue == NULL)
		return;

	GetSemaphore(&OutputSEM, 0);
	
	conn->OutputGetPointer=0;
	conn->OutputQueueLength=0;

	FreeSemaphore(&OutputSEM);
}

void ChatTrytoSend()
{
	// call Flush on any connected streams with queued data

	ChatCIRCUIT * conn;

	int n;

	for (n = 0; n < NumberofChatStreams; n++)
	{
		conn = &ChatConnections[n];
		
		if (conn->Active == TRUE)
			ChatFlush(conn);
	}
}

VOID CloseChat()
{
	int BPQStream, n;
		
	for (n = 0; n < NumberofChatStreams; n++)
	{
		BPQStream = ChatConnections[n].BPQStream;
		
		if (BPQStream)
		{
			SetAppl(BPQStream, 0, 0);
			Disconnect(BPQStream);
			DeallocateStream(BPQStream);
		}
	}

	ClearChatLinkStatus();
	SendChatLinkStatus();
	Sleep(1000);				// A bit of time for links to close
	SendChatLinkStatus();		// Send again to reduce chance of being missed
}

VOID SendChatReport(SOCKET ChatReportSocket, char * buff, int txlen)
{
 	unsigned short int crc = compute_crc(buff, txlen);

	crc ^= 0xffff;

	buff[txlen++] = (crc&0xff);
	buff[txlen++] = (crc>>8);

	sendto(ChatReportSocket, buff, txlen, 0, (LPSOCKADDR)&Chatreportdest, sizeof(Chatreportdest));

}


#endif

