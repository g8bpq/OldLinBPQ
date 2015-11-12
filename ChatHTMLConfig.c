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


#include "bpqchat.h"

extern char OurNode[10];

// Flags Equates

#define GETTINGUSER 1
#define GETTINGBBS 2
#define CHATMODE 4
#define GETTINGTITLE 8
#define GETTINGMESSAGE 16
#define CHATLINK 32					// Link to another Chat Node
#define SENDTITLE 64
#define SENDBODY 128
#define WAITPROMPT 256				// Waiting for prompt after message


extern char PassError[];
extern char BusyError[];

extern char NodeTail[];
extern BOOL APRSApplConnected;

extern char ChatConfigName[250];

extern char OtherNodesList[1000];

extern char ChatWelcomeMsg[1000];

extern USER *user_hd;
extern LINK *link_hd;	

extern UCHAR BPQDirectory[260];

#define MaxSockets 64

extern ChatCIRCUIT ChatConnections[MaxSockets+1];

extern int	NumberofChatStreams;

extern int SMTPMsgs;

extern int ChatApplNum;
extern int MaxChatStreams;

extern char Position[81];
extern char PopupText[251];
extern int PopupMode;

#include "HTTPConnectionInfo.h"

static struct HTTPConnectionInfo * SessionList;	// active bbs config sessions

static struct HTTPConnectionInfo * AllocateSession(char Appl);
static struct HTTPConnectionInfo * FindSession(char * Key);
VOID ProcessUserUpdate(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
VOID ProcessMsgFwdUpdate(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
VOID SendConfigPage(char * Reply, int * ReplyLen, char * Key);
VOID ProcessConfUpdate(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
VOID ProcessUIUpdate(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
VOID SendUserSelectPage(char * Reply, int * ReplyLen, char * Key);
VOID SendFWDSelectPage(char * Reply, int * ReplyLen, char * Key);
int EncryptPass(char * Pass, char * Encrypt);
VOID ProcessFWDUpdate(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
VOID SendStatusPage(char * Reply, int * ReplyLen, char * Key);
VOID SendChatStatusPage(char * Reply, int * ReplyLen, char * Key);
VOID SendUIPage(char * Reply, int * ReplyLen, char * Key);
static VOID GetParam(char * input, char * key, char * value);
BOOL GetConfig(char * ConfigName);
VOID ProcessChatDisUser(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
int APIENTRY SessionControl(int stream, int command, int param);
int APIENTRY GetNumberofPorts();
int APIENTRY GetPortNumber(int portslot);
UCHAR * APIENTRY GetPortDescription(int portslot, char * Desc);
struct PORTCONTROL * APIENTRY GetPortTableEntryFromSlot(int portslot);
VOID SendHouseKeeping(char * Reply, int * ReplyLen, char * Key);
VOID SendWelcomePage(char * Reply, int * ReplyLen, char * Key);
VOID SaveWelcome(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Key);
VOID GetMallocedParam(char * input, char * key, char ** value);
VOID SaveMessageText(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
VOID SaveHousekeeping(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Key);
VOID SaveWP(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Key);
int SetupNodeMenu(char * Buff);
VOID SendFwdSelectPage(char * Reply, int * ReplyLen, char * Key);
VOID SendFwdDetails(struct HTTPConnectionInfo * Session, char * Reply, int * ReplyLen, char * Key);
VOID SendFwdMainPage(char * Reply, int * ReplyLen, char * Key);
VOID SaveFwdCommon(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
VOID SaveFwdDetails(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
char **	SeparateMultiString(char * MultiString);
VOID SendChatConfigPage(char * Reply, int * ReplyLen, char * Key);
VOID SaveChatInfo(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Key);
int rtlink (char * Call);
UCHAR * APIENTRY GetBPQDirectory();
VOID SaveChatConfig(char * ConfigName);
BOOL GetChatConfig(char * ConfigName);
char * GetTemplateFromFile(int Version, char * FN);

static char UNC[] = "";
static char CHKD[] = "checked=checked ";
static char sel[] = "selected";


char ChatSignon[] = "<html><head><title>BPQ32 Chat Server Access</title></head><body background=\"/background.jpg\">"
	"<h3 align=center>BPQ32 Chat Server %s Access</h3>"
	"<h3 align=center>Please enter Callsign and Password to access the Chat Server</h3>"
	"<form method=post action=/Chat/Signon?Chat>"
	"<table align=center  bgcolor=white>"
	"<tr><td>User</td><td><input type=text name=user tabindex=1 size=20 maxlength=50 /></td></tr>" 
	"<tr><td>Password</td><td><input type=password name=password tabindex=2 size=20 maxlength=50 /></td></tr></table>"  
	"<p align=center><input type=submit value=Submit /><input type=submit value=Cancel name=Cancel /></form>";


char ChatPage[] = "<html><head><title>%s's Chat Server</title></head>"
	"<body background=\"/background.jpg\"><h3 align=center>BPQ32 Chat Node %s</h3><P>"
	"<P align=center><table border=1 cellpadding=2 bgcolor=white><tr>"
	"<td><a href=/Chat/ChatStatus?%s>Status</a></td>"
	"<td><a href=/Chat/ChatConf?%s>Configuration</a></td>"
	"<td><a href=/>Node Menu</a></td>"
	"</tr></table>";



static char LostSession[] = "<html><body>"
"<form style=\"font-family: monospace; text-align: center;\" method=post action=/Chat/Lost?%s>"
"Sorry, Session had been lost<br><br>&nbsp;&nbsp;&nbsp;&nbsp;"
"<input name=Submit value=Restart type=submit> <input type=submit value=Exit name=Cancel><br></form>";

char * ChatConfigTemplate = NULL;
char * ChatStatusTemplate = NULL;

static int compare(const void *arg1, const void *arg2)
{
   // Compare Calls. Fortunately call is at start of stuct

   return _stricmp(*(char**)arg1 , *(char**)arg2);
}

int SendChatHeader(char * Reply, char * Key)
{
	return sprintf(Reply, ChatPage, OurNode, OurNode, Key, Key);
}


void ProcessChatHTTPMessage(struct HTTPConnectionInfo * Session, char * Method, char * URL, char * input, char * Reply, int * RLen)
{
	char * Conxtext = 0, * NodeURL;
	int ReplyLen;
	char * Key;
	char Appl = 'M';

	NodeURL = strtok_s(URL, "?", &Conxtext);
	Key = Session->Key;


	if (strcmp(Method, "POST") == 0)
	{
		if (_stricmp(NodeURL, "/Chat/Header") == 0)
		{
			*RLen = SendChatHeader(Reply, Session->Key);
 			return;
		}

		if (_stricmp(NodeURL, "/Chat/ChatConfig") == 0)
		{
			if (ChatConfigTemplate)
				free(ChatConfigTemplate);

			ChatConfigTemplate = GetTemplateFromFile(1, "ChatConfig.txt");
			
			NodeURL[strlen(NodeURL)] = ' ';				// Undo strtok
			SaveChatInfo(Session, input, Reply, RLen, Key);
			return ;
		}

		if (_stricmp(NodeURL, "/Chat/ChatDisSession") == 0)
		{
			ProcessChatDisUser(Session, input, Reply, RLen, Key);
			return ;
		}


		// End of POST section
	}

	if ((_stricmp(NodeURL, "/chat/Chat.html") == 0) || (_stricmp(NodeURL, "/chat/Header") == 0))
	{
		*RLen = SendChatHeader(Reply, Session->Key);
 		return;
	}

	if ((_stricmp(NodeURL, "/Chat/ChatStatus") == 0) || (_stricmp(NodeURL, "/Chat/ChatDisSession") == 0))
	{
		if (ChatStatusTemplate)
			free(ChatStatusTemplate);
	
		ChatStatusTemplate = GetTemplateFromFile(1, "ChatStatus.txt");
		SendChatStatusPage(Reply, RLen, Key);

		return;
	}

	if (_stricmp(NodeURL, "/Chat/ChatConf") == 0)
	{
		if (ChatConfigTemplate)
			free(ChatConfigTemplate);

		ChatConfigTemplate = GetTemplateFromFile(1, "ChatConfig.txt");

		SendChatConfigPage(Reply, RLen, Key);
		return;
	}

	ReplyLen = sprintf(Reply, ChatSignon, OurNode, OurNode);
	*RLen = ReplyLen;

}


static VOID GetParam(char * input, char * key, char * value)
{
	char * ptr = strstr(input, key);
	char Param[2048];
	char * ptr1, * ptr2;
	char c;

	if (ptr)
	{
		ptr2 = strchr(ptr, '&');
		if (ptr2) *ptr2 = 0;
		strcpy(Param, ptr + strlen(key));
		if (ptr2) *ptr2 = '&';					// Restore string

		// Undo any % transparency

		ptr1 = Param;
		ptr2 = Param;

		c = *(ptr1++);

		while (c)
		{
			if (c == '%')
			{
				int n;
				int m = *(ptr1++) - '0';
				if (m > 9) m = m - 7;
				n = *(ptr1++) - '0';
				if (n > 9) n = n - 7;

				*(ptr2++) = m * 16 + n;
			}
			else if (c == '+')
				*(ptr2++) = ' ';
			else
				*(ptr2++) = c;

			c = *(ptr1++);
		}

		*(ptr2++) = 0;

		strcpy(value, Param);
	}
}

static VOID GetCheckBox(char * input, char * key, int * value)
{
	char * ptr = strstr(input, key);
	if (ptr)
		*value = 1;
	else
		*value = 0;
}


VOID SaveChatInfo(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Key)
{
	int ReplyLen = 0;
	char * input;
	struct UserInfo * USER = NULL;
	char Temp[80];
	char Nodes[1000] = "";
	char * ptr1, * ptr2;

	input = strstr(MsgPtr, "\r\n\r\n");	// End of headers

	if (input)
	{
		if (strstr(input, "Cancel=Cancel"))
		{
			*RLen = SendChatHeader(Reply, Session->Key);
 			return;
		}

	
		GetParam(input, "ApplNum=", Temp);
		ChatApplNum = atoi(Temp);
		GetParam(input, "Streams=", Temp);
		MaxChatStreams = atoi(Temp);

		GetParam(input, "nodes=", Nodes);

		ptr1 = Nodes;
		ptr2 = OtherNodesList;

		while (*ptr1)
		{
			if ((*ptr1) == 13)
			{
				*(ptr2++) = ' ';
				ptr1 += 2;
			}
			else
				*(ptr2++) = *(ptr1++);
		}

		*ptr2 = 0;

		GetParam(input, "Posn=", Position);
		GetParam(input, "MapText=", PopupText);
		GetParam(input, "welcome=", ChatWelcomeMsg);

		// Replace cr lf in string with $W

		ptr1 = ChatWelcomeMsg;

	scan2:

		ptr1 = strstr(ptr1, "\r\n");
    
		if (ptr1)
		{    
			*(ptr1++)='$';			// put in cr
			*(ptr1++)='W';			// put in lf

			goto scan2;
		} 

		GetCheckBox(input, "PopType=Click", &PopupMode);

		if (strstr(input, "Restart=Restart+Links"))
		{
			char * ptr1, * ptr2, * Context;

			node_close();

			Sleep(2);
			
			// Dont call removelinks - they may still be attached to a circuit. Just clear header

			link_hd = NULL;
	 
			// Set up other nodes list. rtlink messes with the string so pass copy
	
			ptr2 = ptr1 = strtok_s(_strdup(OtherNodesList), " ,\r", &Context);

			while (ptr1)
			{
				rtlink(ptr1);			
				ptr1 = strtok_s(NULL, " ,\r", &Context);
			}

			free(ptr2);

			if (user_hd)			// Any Users?
				makelinks();		// Bring up links
		}

		if (strstr(input, "UpdateMap=Update+Map"))
		{
			char Msg[500];
			int len;

			len = sprintf(Msg, "INFO %s|%s|%d|\r", Position, PopupText, PopupMode);

			if (len < 256)
					Send_MON_Datagram(Msg, len);

		}

				
#ifdef LINBPQ
		SaveChatConfig(ChatConfigName);
		GetChatConfig(ChatConfigName);
#endif
	}
	
	SendChatConfigPage(Reply, RLen, Key);
	return;
}

VOID ProcessChatDisUser(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest)
{
	char * input;
	char * ptr;

	input = strstr(MsgPtr, "\r\n\r\n");	// End of headers

	if (input)
	{
		ptr = strstr(input, "Stream=");
		if (ptr)
		{
			int Stream = atoi(ptr + 7);
			SessionControl(Stream, 2, 0);
		}
	}	
	SendChatStatusPage(Reply, RLen, Rest);
}

VOID SendChatConfigPage(char * Reply, int * ReplyLen, char * Key)
{
	int Len;
	char Nodes[1000];
	char Text[1000];
	char * ptr1, * ptr2;

	//	Replace spaces in Node List with CR/LF

	ptr1 = OtherNodesList;
	ptr2 = Nodes;

	while (*ptr1)
	{
		if ((*ptr1) == ' ')
		{
			*(ptr2++) = 13;
			*(ptr2++) = 10;
			ptr1++ ;
		}
		else
			*(ptr2++) = *(ptr1++);
	}

	*ptr2 = 0;

	// Replace " in Text with &quot; 
		
	ptr1 = PopupText;
	ptr2 = Text;

	while (*ptr1)
	{
		if ((*ptr1) == '"')
		{
			*(ptr2++) = '&';
			*(ptr2++) = 'q';
			*(ptr2++) = 'u';
			*(ptr2++) = 'o';
			*(ptr2++) = 't';
			*(ptr2++) = ';';
			ptr1++ ;
		}
		else
			*(ptr2++) = *(ptr1++);
	}

	*ptr2 = 0;

	// Replace $W in  Welcome Message with cr lf

	ptr2 = ptr1 = _strdup(ChatWelcomeMsg);

scan:

	ptr1 = strstr(ptr1, "$W");
    
	if (ptr1)
	{    
		*(ptr1++)=13;			// put in cr
		*(ptr1++)=10;			// put in lf

		goto scan;
	} 
	
	Len = sprintf(Reply, ChatConfigTemplate,
		OurNode, Key, Key, Key,
		ChatApplNum, MaxChatStreams, Nodes, Position,
		(PopupMode) ? UNC  : CHKD, 
		(PopupMode) ? CHKD  : UNC,  Text, ptr2);

	free(ptr2);

	*ReplyLen = Len;
}

VOID SendChatStatusPage(char * Reply, int * ReplyLen, char * Key)
{
	int Len = 0;
	USER *user;
	char * Alias;
	char * Topic;
	LINK *link;

	char Streams[8192];
	char Users[8192];
	char Links[8192];

	ChatCIRCUIT * conn;
	int i = 0, n; 

	Users[0] = 0;

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

		Len += sprintf(&Users[Len], "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td></tr>",
			user->call, Alias, user->name, Topic, (int)(time(NULL) - user->lastmsgtime), user->qth);
	
	}

	Links[0] = 0;

	Len = 0;

	for (link = link_hd; link; link = link->next)
	{
		if (link->flags & p_linked )
			Len += sprintf(&Links[Len], "<tr><td>%s</td><td>Open</td></tr>", link->call);
		else if (link->flags & (p_linked | p_linkini))
			Len += sprintf(&Links[Len], "<tr><td>%s</td><td>Connecting</td></tr>", link->call);
		else
			Len += sprintf(&Links[Len], "<tr><td>%s</td><td>Idle</td></tr>", link->call);
	}

	Len = 0;
	Streams[0] = 0;

	for (n = 0; n < NumberofChatStreams; n++)
	{
		conn=&ChatConnections[n];
		i = conn->BPQStream;
		if (!conn->Active)
		{
			Len += sprintf(&Streams[Len], "<tr><td onclick= SelectRow(%d) id=cell_%d>Idle</td><td>&nbsp;&nbsp;</td><td>&nbsp;&nbsp;</td><td>&nbsp;&nbsp;</td><td>&nbsp;&nbsp;</td></tr>", i, i);
		}
		else
		{
			if (conn->Flags & CHATLINK)
			{
				if (conn->BPQStream > 64 || conn->u.link == 0)
					Len += sprintf(&Streams[Len], "<tr><td onclick= SelectRow(%d) id=cell_%d>** Corrupt ChatLink **</td>"
					"<td>&nbsp;&nbsp;</td><td>&nbsp;&nbsp;</td><td>&nbsp;&nbsp;</td><td>&nbsp;&nbsp;</td></tr>", i, i);
				else
					Len += sprintf(&Streams[Len], "<tr><td onclick='SelectRow(%d)' id='cell_%d'>"
					"%s</td><td>%s</td><td>%d</td><td>%s</td><td>%d</td></tr>",
						i, i, "Chat Link", conn->u.link->alias, conn->BPQStream,
						"", conn->OutputQueueLength - conn->OutputGetPointer);
			}
			else
			if ((conn->Flags & CHATMODE) && conn->topic)
			{
				Len += sprintf(&Streams[Len],  "<tr><td onclick='SelectRow(%d)' id='cell_%d'>%s</td><td>%s</td><td>%d</td><td>%s</td><td>%d</td></tr>",
					i, i, conn->u.user->name, conn->u.user->call, conn->BPQStream,
					conn->topic->topic->name, conn->OutputQueueLength - conn->OutputGetPointer);
			}
			else
			{
				if (conn->UserPointer == 0)
					Len += sprintf(&Streams[Len], "Logging in");
				else
				{
					Len += sprintf(&Streams[Len], "<tr><td onclick='SelectRow(%d)' id='cell_%d'>%s</td><td>%s</td><td>%d</td><td>%s</td><td>%d</td></tr>",
						i, i, conn->UserPointer->Name, conn->UserPointer->Call, conn->BPQStream,
						"CHAT", conn->OutputQueueLength - conn->OutputGetPointer);
				}
			}
		}
	}

	Len = sprintf(Reply, ChatStatusTemplate, OurNode, OurNode, Key, Key, Key, Streams, Users, Links);
	*ReplyLen = Len;
}


static struct HTTPConnectionInfo * AllocateSession(char Appl)
{
	int KeyVal;
	struct HTTPConnectionInfo * Session = zalloc(sizeof(struct HTTPConnectionInfo));

	if (Session == NULL)
		return NULL;

	KeyVal = time(NULL);

	sprintf(Session->Key, "%c%012X", Appl, KeyVal);

	if (SessionList)
		Session->Next = SessionList;

	SessionList = Session;

	return Session;
}

static struct HTTPConnectionInfo * FindSession(char * Key)
{
	struct HTTPConnectionInfo * Session = SessionList;

	while (Session)
	{
		if (strcmp(Session->Key, Key) == 0)
			return Session;

		Session = Session->Next;
	}

	return NULL;
}
#ifdef WIN32

static char PipeFileName[] = "\\\\.\\pipe\\BPQChatWebPipe";

static DWORD WINAPI InstanceThread(LPVOID lpvParam)

// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{ 
   DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0; 
   BOOL fSuccess = FALSE;
   HANDLE hPipe  = NULL;
   char Buffer[4096];
   char OutBuffer[100000];
   char * MsgPtr;
   int InputLen = 0;
   int OutputLen = 0;
	struct HTTPConnectionInfo Session;
	char URL[4096];
	char * Context, * Method;
	int n;

	char * ptr;

//	Debugprintf("InstanceThread created, receiving and processing messages.");

// The thread's parameter is a handle to a pipe object instance. 
 
   hPipe = (HANDLE) lpvParam; 

   // Read client requests from the pipe. This simplistic code only allows messages
   // up to BUFSIZE characters in length.
 
   n = ReadFile(hPipe, &Session, sizeof (struct HTTPConnectionInfo), &n, NULL);
   fSuccess = ReadFile(hPipe, Buffer, 4096, &InputLen, NULL);

	if (!fSuccess || InputLen == 0)
	{   
		if (GetLastError() == ERROR_BROKEN_PIPE)
			Debugprintf("InstanceThread: client disconnected.", GetLastError()); 
		else
			Debugprintf("InstanceThread ReadFile failed, GLE=%d.", GetLastError()); 
	}
	else
	{
		Buffer[InputLen] = 0;

		MsgPtr = &Buffer[0];

		strcpy(URL, MsgPtr);

		ptr = strstr(URL, " HTTP");

		if (ptr)
			*ptr = 0;

		Method = strtok_s(URL, " ", &Context);

		ProcessChatHTTPMessage(&Session, Method, Context, MsgPtr, OutBuffer, &OutputLen);

		WriteFile(hPipe, &Session, sizeof (struct HTTPConnectionInfo), &n, NULL);
		WriteFile(hPipe, OutBuffer, OutputLen, &cbWritten, NULL); 

		FlushFileBuffers(hPipe); 
		DisconnectNamedPipe(hPipe); 
		CloseHandle(hPipe);
	}
	return 1;
}

static DWORD WINAPI PipeThreadProc(LPVOID lpvParam)
{
	BOOL   fConnected = FALSE; 
	DWORD  dwThreadId = 0; 
	HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL; 
 
// The main loop creates an instance of the named pipe and 
// then waits for a client to connect to it. When the client 
// connects, a thread is created to handle communications 
// with that client, and this loop is free to wait for the
// next client connect request. It is an infinite loop.
 
	for (;;) 
	{ 
      hPipe = CreateNamedPipe( 
          PipeFileName,             // pipe name 
          PIPE_ACCESS_DUPLEX,       // read/write access 
          PIPE_TYPE_BYTE |       // message type pipe 
          PIPE_WAIT,                // blocking mode 
          PIPE_UNLIMITED_INSTANCES, // max. instances  
          4096,                  // output buffer size 
          4096,                  // input buffer size 
          0,                        // client time-out 
          NULL);                    // default security attribute 

      if (hPipe == INVALID_HANDLE_VALUE) 
      {
          Debugprintf("CreateNamedPipe failed, GLE=%d.\n", GetLastError()); 
          return -1;
      }
 
      // Wait for the client to connect; if it succeeds, 
      // the function returns a nonzero value. If the function
      // returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 
 
      fConnected = ConnectNamedPipe(hPipe, NULL) ? 
         TRUE : (GetLastError() == ERROR_PIPE_CONNECTED); 
 
      if (fConnected) 
	  {
         // Create a thread for this client. 
   
		 hThread = CreateThread( 
            NULL,              // no security attribute 
            0,                 // default stack size 
            InstanceThread,    // thread proc
            (LPVOID) hPipe,    // thread parameter 
            0,                 // not suspended 
            &dwThreadId);      // returns thread ID 

         if (hThread == NULL) 
         {
            Debugprintf("CreateThread failed, GLE=%d.\n", GetLastError()); 
            return -1;
         }
         else CloseHandle(hThread); 
       } 
      else 
        // The client could not connect, so close the pipe. 
         CloseHandle(hPipe); 
   } 

   return 0; 
} 

BOOL CreateChatPipeThread()
{
	DWORD ThreadId;
	CreateThread(NULL, 0, PipeThreadProc, 0, 0, &ThreadId);
	return TRUE;
}

static char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static char *dat[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};


static VOID FormatTime(char * Time, time_t cTime)
{
	struct tm * TM;
	TM = gmtime(&cTime);

	sprintf(Time, "%s, %02d %s %3d %02d:%02d:%02d GMT", dat[TM->tm_wday], TM->tm_mday, month[TM->tm_mon],
		TM->tm_year + 1900, TM->tm_hour, TM->tm_min, TM->tm_sec);

}

#endif




