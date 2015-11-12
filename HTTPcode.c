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


//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#define _CRT_SECURE_NO_DEPRECATE

#define _USE_32BIT_TIME_T

#define DllImport

#include "CHeaders.h"
#include <stdlib.h>

#include "tncinfo.h"
#include "time.h"
#include "bpq32.h"
#include "telnetserver.h"

#define CKernel
#include "HTTPConnectionInfo.h"

extern int MAXBUFFS, QCOUNT, MINBUFFCOUNT, NOBUFFCOUNT, BUFFERWAITS, L3FRAMES;
extern int NUMBEROFNODES, MAXDESTS, L4CONNECTSOUT, L4CONNECTSIN, L4FRAMESTX, L4FRAMESRX, L4FRAMESRETRIED, OLDFRAMES;
extern int STATSTIME;
extern  TRANSPORTENTRY * L4TABLE;
extern BPQVECSTRUC BPQHOSTVECTOR[];
extern BOOL APRSApplConnected;  
extern char VersionString[];
VOID FormatTime2(char * Time, time_t cTime);
DllExport int APIENTRY Get_APPLMASK(int Stream);
VOID SaveUIConfig();
int ProcessNodeSignon(SOCKET sock, struct TCPINFO * TCP, char * MsgPtr, char * Appl, char * Reply, struct HTTPConnectionInfo ** Session);
VOID SetupUI(int Port);
VOID SendUIBeacon(int Port);


extern struct ROUTE * NEIGHBOURS;
extern int  ROUTE_LEN;
extern int  MAXNEIGHBOURS;

extern struct DEST_LIST * DESTS;				// NODE LIST
extern int  DEST_LIST_LEN;
extern int  MAXDESTS;			// MAX NODES IN SYSTEM

extern struct _LINKTABLE * LINKS;
extern int	LINK_TABLE_LEN; 
extern int	MAXLINKS;

extern COLORREF Colours[256];

extern BOOL IncludesMail;
extern BOOL IncludesChat;

extern BOOL APRSWeb;  

extern char * UIUIDigi[33];
extern char UIUIDEST[33][11];		// Dest for Beacons
extern UCHAR FN[33][256];			// Filename
extern int Interval[33];			// Beacon Interval (Mins)
extern char Message[33][1000];		// Beacon Text

extern int MinCounter[33];			// Interval Countdown
extern BOOL SendFromFile[33];

extern HKEY REGTREE;

extern BOOL APRSActive;

char * strlop(char * buf, char delim);
VOID sendandcheck(SOCKET sock, const char * Buffer, int Len);
int CompareNode(const void *a, const void *b);
int CompareAlias(const void *a, const void *b);
void ProcessMailHTTPMessage(struct HTTPConnectionInfo * Session, char * Method, char * URL, char * input, char * Reply, int * RLen);
void ProcessChatHTTPMessage(struct HTTPConnectionInfo * Session, char * Method, char * URL, char * input, char * Reply, int * RLen);
struct PORTCONTROL * APIENTRY GetPortTableEntryFromSlot(int portslot);
int SetupNodeMenu(char * Buff);
int StatusProc(char * Buff);
int ProcessMailSignon(struct TCPINFO * TCP, char * MsgPtr, char * Appl, char * Reply, struct HTTPConnectionInfo ** Session, BOOL WebMail);
int ProcessChatSignon(struct TCPINFO * TCP, char * MsgPtr, char * Appl, char * Reply, struct HTTPConnectionInfo ** Session);
VOID APRSProcessHTTPMessage(SOCKET sock, char * MsgPtr);


static struct HTTPConnectionInfo * SessionList;	// active term mode sessions

char Mycall[10];

char APRSPipeFileName[] = "\\\\.\\pipe\\BPQAPRSWebPipe";
char MAILPipeFileName[] = "\\\\.\\pipe\\BPQMAILWebPipe";
char CHATPipeFileName[] = "\\\\.\\pipe\\BPQCHATWebPipe";

char Index[] = "<html><head><title>%s's BPQ32 Web Server</title></head><body><P align=center>"
	"<table border=2 cellpadding=2 cellspacing=2 bgcolor=white>"
	"<tr><td align=center><a href=/Node/NodeMenu.html>Node Pages</a></td>"
	"<td align=center><a href=/aprs/all.html>APRS Pages</a></td></tr></table></body></html>";

char IndexNoAPRS[] = "<meta http-equiv=\"refresh\" content=\"0;url=/Node/NodeIndex.html\">"
	"<html><head></head><body></body></html>";

char NodeMenuHeader[] = "<html><head><title>%s's BPQ32 Web Server</title><script>"
	"function dev_win(URL,x,y){"
	"var xx = \"width=\" + x;"
	"var yy = \",height=\" + y;"
	"var param = \"toolbar=no, location=no, directories=no, status=no, "
	"menubar=no, scrollbars=no, resizable=no, titlebar=no, toobar=no, \" + xx + yy;"
	"window.open(URL,\"_blank\",param);"
	"}"
	"function open_win(){";

char NodeMenuLine[] = "dev_win(\"/Node/Port?%d\",%d,%d);";
char NodeMenuRest[] = "}</script></head>"
	"<body background=\"/background.jpg\"><h1 align=center>BPQ32 Node %s</h1><P>"
	"<P align=center><table border=1 cellpadding=2 bgcolor=white><tr>"
	"<td><a href=/Node/Routes.html>Routes</a></td>"
	"<td><a href=/Node/Nodes.html>Nodes</a></td>"
	"<td><a href=/Node/Ports.html>Ports</a></td>"
	"<td><a href=/Node/Links.html>Links</a></td>"
	"<td><a href=/Node/Users.html>Users</a></td>"
	"<td><a href=/Node/Stats.html>Stats</a></td>"
	"<td><a href=/Node/Terminal.html>Terminal</a></td>%s%s%s%s%s";
	char DriverBit[] = "<td><a href=\"javascript:open_win();\">Driver Windows</a></td>"
	"<td><a href=javascript:dev_win(\"/Node/Streams\",820,700);>Stream Status</a></td>";
char APRSBit[] = "<td><a href=../aprs/all.html>APRS Pages</a></td>";

char MailBit[] = "<td><a href=../Mail/Header>Mail Server Pages</a></td>";
char ChatBit[] = "<td><a href=../Chat/Header>Chat Server Pages</a></td>";

char NodeTail[] = "<td><a href=/Node/Signon.html>SYSOP Signin</a></td>" 
 				  "<td><a href=/Node/EditCfg.html>Edit Config</a></td>"
				  "</tr></table>";
	
char Tail[] = "</body></html>";

char RouteHddr[] = "<h2 align=center>Routes</h2><table align=center border=2 style=font-family:monospace bgcolor=white>"
	"<tr><th>Port</th><th>Call</th><th>Quality</th><th>Node Count</th><th>Frame Count</th><th>Retries</th><th>Percent</th><th>Maxframe</th><th>Frack</th><th>Last Heard</th><th>Queued</th><th>Rem Qual</th></tr>";

char RouteLine[] = "<tr><td>%s%d</td><td>%s%c</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d%</td><td>%d</td><td>%d</td><td>%02d:%02d</td><td>%d</td><td>%d</td></tr>";
char xNodeHddr[] = "<align=center><form align=center method=get action=/Node/Nodes.html>"
	"<table align=center  bgcolor=white>"
	"<tr><td><input type=submit name=a value=\"Nodes Sorted by Alias\"></td><td>"
	"<input type=submit name=c value=\"Nodes Sorted by Call\"></td><td>"
	"<input type=submit name=t value=\"Nodes with traffic\"></td></tr></form></table>"
	"<h2 align=center>Nodes %s</h2><table style=font-family:monospace align=center border=2 bgcolor=white><tr>";

char NodeHddr[] = "<center><form method=get action=/Node/Nodes.html>"
	"<input type=submit name=a value=\"Nodes Sorted by Alias\">"
	"<input type=submit name=c value=\"Nodes Sorted by Call\">"
	"<input type=submit name=t value=\"Nodes with traffic\"></form></center>"
	"<h2 align=center>Nodes %s</h2><table style=font-family:monospace align=center border=2 bgcolor=white><tr>";

char NodeLine[] = "<td><a href=NodeDetail?%s>%s:%s</td>";

char PortsHddr[] = "<h2 align=center>Ports</h2><table align=center border=2 bgcolor=white>"
	"<tr><th>Port</th><th>Driver</th><th>ID</th><th>Beacons</th></tr>";

char PortLine[] = "<tr><td>%d</td><td><a href=PortStats?%d&%s>&nbsp;%s</a></td><td>%s</td></tr>";
char PortLineWithBeacon[] = "<tr><td>%d</td><td><a href=PortStats?%d&%s>&nbsp;%s</a></td><td>%s</td><td><a href=PortBeacons?%d>&nbsp;Beacons</a></td></tr>";

char SessionPortLine[] = "<tr><td>%d</td><td>%s</td><td>%s</td><td> </td></tr>";

char StatsHddr[] = "<h2 align=center>Node Stats</h2><table align=center cellpadding=2 bgcolor=white>"
	"<col width=250 /><col width=80 /><col width=80 /><col width=80 /><col width=80 /><col width=80 />";

char PortStatsHddr[] = "<h2 align=center>Stats for Port %d</h2><table align=center border=2 cellpadding=2 bgcolor=white>";

char PortStatsLine[] = "<tr><td> %s </td><td> %d </td></tr>";


char Beacons[] = "<h2 align=center>Beacon Configuration for Port %d</h2><h3 align=center>You need to be signed in to save changes</h3><table align=center border=2 cellpadding=2 bgcolor=white>"
	"<form method=post action=BeaconAction>"
	"<table align=center  bgcolor=white>"
	"<tr><td>Send Interval (Minutes)</td><td><input type=text name=Every tabindex=1 size=5 value=%d></td></tr>" 
	"<tr><td>To</td><td><input name=Dest tabindex=2 size=5 value=%s></td></tr>"  
	"<tr><td>Path</td><td><input type=text name=Path size=50 maxlength=50 value=%s></td></tr>"
	"<tr><td>Send From File</td><td><input type=text name=File size=50 maxlength=50  value=%s></td></tr>"
	"<tr><td>Text</td><td><textarea name=\"Text\" cols=40 rows=5>%s</textarea></td></tr>"
	"</table>" 
	"<input type=hidden name=Port value=%d>"

	"<p align=center><input type=submit value=Save><input type=submit value=Test name=Test>"
	"</form>";


char LinkHddr[] = "<h2 align=center>Links</h2><table align=center border=2 bgcolor=white>"
	"<tr><th>Far Call</th><th>Our Call</th><th>Port</th><th>ax.25 state</th><th>Link Type</th><th>ax.25 Version</th></tr>";

char LinkLine[] = "<tr><td>%s</td><td>%s</td><td>%d</td><td>%s</td><td>%s</td><td align=center >%d</td></tr>";

char UserHddr[] = "<h2 align=center>Sessions</h2><table align=center border=2 cellpadding=2 bgcolor=white>";

char UserLine[] = "<tr><td>%s</td><td>%s</td><td>%s</td></tr>";

char TermSignon[] = "<html><head><title>BPQ32 Node %s Terminal Access</title></head><body background=\"/background.jpg\">"
	"<h2 align=center>BPQ32 Node %s Terminal Access</h2>"
	"<h3 align=center>Please enter username and password to access the node</h3>"
	"<form method=post action=TermSignon>"
	"<table align=center  bgcolor=white>"
	"<tr><td>User</td><td><input type=text name=user tabindex=1 size=20 maxlength=50 /></td></tr>" 
	"<tr><td>Password</td><td><input type=password name=password tabindex=2 size=20 maxlength=50 /></td></tr></table>"  
	"<p align=center><input type=submit value=Submit><input type=submit value=Cancel name=Cancel>"
	"<input type=hidden name=Appl value=\"%s\"  id=Pass></form>";


char PassError[] = "<p align=center>Sorry, User or Password is invalid - please try again</p>";

char BusyError[] = "<p align=center>Sorry, No sessions available - please try later</p>";

char LostSession[] = "<html><body>Sorry, Session had been lost - refresh page to sign in again";

char TermPage[] = "<html><meta http-equiv=""Content-Type"" content=""text/html; charset=UTF-8"" />"
	"<head><title>BPQ32 Node %s</title></head>"
	"<body><h3 align=center>BPQ32 Node %s</h3>"
	"<form method=post action=/Node/TermClose?%s>"
	"<p align=center><input type=submit value=\"Close and return to Node Page\" /></form>"
    "<iframe src=OutputScreen.html?%s width=100%% height=80%%></iframe>"
	"<iframe src=InputLine.html?%s width=100%% height=60></iframe>"
	"</body>";

char TermOutput[] = "<html><head>"
	"<meta http-equiv=cache-control content=no-cache>"
	"<meta http-equiv=pragma content=no-cache>"
	"<meta http-equiv=expires content=0>" 
	"<meta http-equiv=refresh content=2>"
	"<script type=\"text/javascript\">\r\n"
	"function ScrollOutput()\r\n"
	"{window.scrollBy(0,document.body.scrollHeight)}</script>"
	"</head><body id=Text onload=\"ScrollOutput()\">"
	"<p style=font-family:monospace>";
//<body onLoad="pageScroll()">
//char TermOutputTail[] = "</p><script>scrollElementToEnd(document.getElementById(\"Text\"));</script>";
//char TermOutputTail[] = "</p><script>document.getElementById(\"Text\").scrollTo(0,1500)</script>";
//char TermOutputTail[] = "</p><script>window.scrollBy(0,500);</script></body></html>";
char TermOutputTail[] = "</p></script></body></html>";

char InputLine[] = "<html><head></head><body>"
	"<form name=inputform method=post action=/TermInput?%s>"
	"<input type=text size=105 name=input />"
	"<script>document.inputform.input.focus();</script></form>";

static char NodeSignon[] = "<html><head><title>BPQ32 Node SYSOP Access</title></head><body background=\"/background.jpg\">"
	"<h3 align=center>BPQ32 Node %s SYSOP Access</h3>"
	"<h3 align=center>This page sets Cookies. Don't continue if you object to this</h3>"
	"<h3 align=center>Please enter Callsign and Password to access the Node</h3>"
	"<form method=post action=/Node/Signon?Node>"
	"<table align=center  bgcolor=white>"
	"<tr><td>User</td><td><input type=text name=user tabindex=1 size=20 maxlength=50 /></td></tr>" 
	"<tr><td>Password</td><td><input type=password name=password tabindex=2 size=20 maxlength=50 /></td></tr></table>"  
	"<p align=center><input type=submit value=Submit /><input type=submit value=Cancel name=Cancel /></form>";


static char MailSignon[] = "<html><head><title>BPQ32 Mail Server Access</title></head><body background=\"/background.jpg\">"
	"<h3 align=center>BPQ32 Mail Server %s Access</h3>"
	"<h3 align=center>Please enter Callsign and Password to access the BBS</h3>"
	"<form method=post action=/Mail/Signon?Mail>"
	"<table align=center  bgcolor=white>"
	"<tr><td>User</td><td><input type=text name=user tabindex=1 size=20 maxlength=50 /></td></tr>" 
	"<tr><td>Password</td><td><input type=password name=password tabindex=2 size=20 maxlength=50 /></td></tr></table>"  
	"<p align=center><input type=submit value=Submit /><input type=submit value=Cancel name=Cancel /></form>";

static char ChatSignon[] = "<html><head><title>BPQ32 Chat Server Access</title></head><body background=\"/background.jpg\">"
	"<h3 align=center>BPQ32 Chat Server %s Access</h3>"
	"<h3 align=center>Please enter Callsign and Password to access the Chat Server</h3>"
	"<form method=post action=/Chat/Signon?Chat>"
	"<table align=center  bgcolor=white>"
	"<tr><td>User</td><td><input type=text name=user tabindex=1 size=20 maxlength=50 /></td></tr>" 
	"<tr><td>Password</td><td><input type=password name=password tabindex=2 size=20 maxlength=50 /></td></tr></table>"  
	"<p align=center><input type=submit value=Submit /><input type=submit value=Cancel name=Cancel /></form>";


static char MailLostSession[] = "<html><body>"
"<form style=\"font-family: monospace; text-align: center;\" method=post action=/Mail/Lost?%s>"
"Sorry, Session had been lost<br><br>&nbsp;&nbsp;&nbsp;&nbsp;"
"<input name=Submit value=Restart type=submit> <input type=submit value=Exit name=Cancel><br></form>";


static char ConfigEditPage[] = "<html><head><meta content=\"text/html; charset=ISO-8859-1\" http-equiv=\"content-type\">"
"<title></title></head><body>"
"<form style=\"font-family: monospace;  text-align: center;\"method=post action=CFGSave?%s>"
"<textarea cols=100 rows=25 name=Msg>%s</textarea><br><br>"
"<input name=Save value=Save type=submit><input name=Cancel value=Cancel type=submit><br></form>";

static char EXCEPTMSG[80] = "";

VOID PollSession(struct HTTPConnectionInfo * Session)
{
	int state, change;
	int count, len;
	char Msg[400] = "";
	char Formatted[2048];
	char * ptr1, * ptr2;
	char c;
	int Line;

	// Poll Node

	SessionState(Session->Stream, &state, &change);
		
	if (change == 1)
	{
		int Line = Session->LastLine++;
		free(Session->ScreenLines[Line]);

		if (state == 1)// Connected
			Session->ScreenLines[Line] = _strdup("*** Connected<br>\r\n");
		else
			Session->ScreenLines[Line] = _strdup("*** Disconnected<br>\r\n");
	
		if (Line == 99)
		Session->LastLine = 0;
		
		Session->Changed = TRUE;
	}
	
	if (RXCount(Session->Stream) > 0)
	{
		do
		{
			GetMsg(Session->Stream, &Msg[0], &len, &count);

			// replace cr with <br> and space with &nbsp;

			ptr1 = Msg;
			ptr2 = &Formatted[0];

			if (Session->PartLine)
			{
				// Last line was incomplete - append to it

				Line = Session->LastLine - 1;

				if (Line < 0)
					Line = 99;

				strcpy(Formatted, Session->ScreenLines[Line]);
				ptr2 += strlen(Formatted);

				Session->LastLine = Line;
				Session->PartLine = FALSE;
			}

			while (len--)
			{
				c = *(ptr1++);
				if (c == 13)
				{
					int LineLen;
					
					strcpy(ptr2, "<br>\r\n");
	
					// Write to screen

					Line = Session->LastLine++;
					free(Session->ScreenLines[Line]);

					LineLen = strlen(Formatted);

					// if line starts with a colour code, process it

					if (Formatted[0] ==  0x1b && LineLen > 1)
					{
						int ColourCode = Formatted[1] - 10;
						COLORREF Colour = Colours[ColourCode];
						char ColString[30];

						memmove(&Formatted[20], &Formatted[2], LineLen);
						sprintf(ColString, "<font color=#%02X%02X%02X>",  GetRValue(Colour), GetGValue(Colour), GetBValue(Colour));
						memcpy(Formatted, ColString, 20);
						strcat(Formatted, "</font>");
						LineLen =+ 28;
					}
					
					Session->ScreenLineLen[Line] = LineLen;
					Session->ScreenLines[Line] = _strdup(Formatted);
					
					if (Line == 99)
						Session->LastLine = 0;
					
					ptr2 = &Formatted[0];
					
				}
				else if (c == 32)
				{
					memcpy(ptr2, "&nbsp;", 6);
					ptr2 += 6;
				}
				else if (c == '>')
				{
					memcpy(ptr2, "&gt;", 4);
					ptr2 += 4;
				}
				else if (c == '<')
				{
					memcpy(ptr2, "&lt;", 4);
					ptr2 += 4;
				}
				else
					*(ptr2++) = c;
			}

			*ptr2 = 0;

			if (ptr2 != &Formatted[0])
			{
				// Incomplete line

				// Save to screen

				Line = Session->LastLine++;
				free(Session->ScreenLines[Line]);
		
				Session->ScreenLines[Line] = _strdup(Formatted);

				if (Line == 99)
					Session->LastLine = 0;
				
				Session->PartLine = TRUE;
			}

		//	strcat(Session->ScreenBuffer, Formatted);
			Session->Changed = TRUE;
		
		} while (count > 0);
	}
}


VOID HTTPTimer()
{
	// Run every tick. Check for status change and data available

	struct HTTPConnectionInfo * Session = SessionList;	// active term mode sessions
	struct HTTPConnectionInfo * PreviousSession = NULL;

	while (Session)
	{
		Session->KillTimer++;

		if (Session->Key[0] != 'T')
		{
			PreviousSession = Session;
			Session = Session->Next;
			continue;
		}

		if (Session->KillTimer > 3000)		// Around 5 mins
		{
			int i;
			int Stream = Session->Stream;

			for (i = 0; i < 100; i++)
			{
				free(Session->ScreenLines[i]);
			}

			SessionControl(Stream, 2, 0);
			SessionState(Stream, &i, &i);
			DeallocateStream(Stream);

			if (PreviousSession)
				PreviousSession->Next = Session->Next;		// Remove from chain
			else
				SessionList = Session->Next;

			free(Session);

			break;
		}
		
		PollSession(Session);

//		if (Session->ResponseTimer == 0 && Session->Changed)
//			Debugprintf("Data to send but no outstanding GET");

		if (Session->ResponseTimer)
		{
			Session->ResponseTimer--;

			if (Session->ResponseTimer == 0 || Session->Changed)
			{
				SOCKET sock = Session->sock;
				char _REPLYBUFFER[100000];
				int ReplyLen;
				char Header[256];
				int HeaderLen;
				int Last = Session->LastLine;
				int n;
	
				strcpy(_REPLYBUFFER, TermOutput);

				for (n = Last;;)
				{
					strcat(_REPLYBUFFER, Session->ScreenLines[n]);

					if (n == 99)
						n = -1;
	
					if (++n == Last)
						break;
				}

				ReplyLen = strlen(_REPLYBUFFER);
				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", TermOutputTail);
		
				HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen);
				sendandcheck(sock, Header, HeaderLen);
				sendandcheck(sock, _REPLYBUFFER, ReplyLen);

				Session->ResponseTimer = Session->Changed = 0;
			}
		}
		PreviousSession = Session;
		Session = Session->Next;
	}
}

struct HTTPConnectionInfo * AllocateSession(SOCKET sock, char Mode)
{
	int KeyVal;
	struct HTTPConnectionInfo * Session = zalloc(sizeof(struct HTTPConnectionInfo));
	int i;
	
	if (Session == NULL)
		return NULL;

	if (Mode == 'T')
	{
		// Terminal

		for (i = 0; i < 20; i++)
			Session->ScreenLines[i] = _strdup("Scroll to end<br>");
		
		for (i = 20; i < 100; i++)
			Session->ScreenLines[i] = _strdup("<br>\r\n");

		Session->Stream = FindFreeStream();

		if (Session->Stream == 0)
			return NULL;

		SessionControl(Session->Stream, 1, 0);
	}
	
	KeyVal = (int)sock * time(NULL);

	sprintf(Session->Key, "%c%012X", Mode, KeyVal);

	if (SessionList)
		Session->Next = SessionList;

	SessionList = Session;

	return Session;
}

struct HTTPConnectionInfo * FindSession(char * Key)
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

void ProcessTermInput(SOCKET sock, char * MsgPtr, int MsgLen, char * Key)
{
	char _REPLYBUFFER[1024];
	int ReplyLen = sprintf(_REPLYBUFFER, InputLine, Key, Key);
	char Header[256];
	int HeaderLen;
	int State;
	struct HTTPConnectionInfo * Session = FindSession(Key);
	int Stream;

	if (Session == NULL)
	{
		ReplyLen = sprintf(_REPLYBUFFER, "%s", LostSession);
	}
	else
	{
		char * input = strstr(MsgPtr, "\r\n\r\n");	// End of headers
		char * end = &MsgPtr[MsgLen];
		int Line = Session->LastLine++;
		char * ptr1, * ptr2;
		char c;
		UCHAR hex;

		Stream = Session->Stream;

		input += 10;
		ptr1 = ptr2 = input;

		// Convert any %xx constructs

		while (ptr1 != end)
		{
			c = *(ptr1++);
			if (c == '%')
			{
				c = *(ptr1++);
				if(isdigit(c))
					hex = (c - '0') << 4;
				else
					hex = (tolower(c) - 'a' + 10) << 4;

				c = *(ptr1++);
				if(isdigit(c))
					hex += (c - '0');
				else
					hex += (tolower(c) - 'a' + 10);

				*(ptr2++) = hex;
			}
			else if (c == '+')
				*(ptr2++) = 32;
			else
				*(ptr2++) = c;
		}

		end = ptr2;
		
		*ptr2 = 0;

		strcat(input, "<br>\r\n");

		free(Session->ScreenLines[Line]);
		
		Session->ScreenLines[Line] = _strdup(input);

		if (Line == 99)
			Session->LastLine = 0;
		
		*end++ = 13;
		*end = 0;

		SessionStateNoAck(Stream, &State);

		if (State == 0)
		{
			char AXCall[10];
			SessionControl(Stream, 1, 0);
			ConvToAX25(Session->HTTPCall, AXCall);
			ChangeSessionCallsign(Stream, AXCall);
			BPQHOSTVECTOR[Session->Stream -1].HOSTSESSION->Secure_Session = Session->USER->Secure;
		}

		SendMsg(Stream, input, end - input);
		Session->Changed = TRUE;
		Session->KillTimer = 0;
	}

	HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));
	send(sock, Header, HeaderLen, 0);
	send(sock, _REPLYBUFFER, ReplyLen, 0);
	send(sock, Tail, strlen(Tail), 0);
}


void ProcessTermClose(SOCKET sock, char * MsgPtr, int MsgLen, char * Key)
{
	char _REPLYBUFFER[8192];
	int ReplyLen = sprintf(_REPLYBUFFER, InputLine, Key, Key);
	char Header[256];
	int HeaderLen;
	struct HTTPConnectionInfo * Session = FindSession(Key);

	if (Session)
	{
		Session->KillTimer = 99999;
	}

	ReplyLen = SetupNodeMenu(_REPLYBUFFER);

	HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n"
		"\r\n", ReplyLen + strlen(Tail));
	send(sock, Header, HeaderLen, 0);
	send(sock, _REPLYBUFFER, ReplyLen, 0);
	send(sock, Tail, strlen(Tail), 0);
}

ProcessTermSignon(struct TNCINFO * TNC, SOCKET sock, char * MsgPtr, int MsgLen)
{
	char _REPLYBUFFER[8192];
	int ReplyLen;
	char Header[256];
	int HeaderLen;
	char * input = strstr(MsgPtr, "\r\n\r\n");	// End of headers
	char * user, * password, * Context, * Appl;
	char NoApp[] = "";
	struct TCPINFO * TCP = TNC->TCPInfo;

	if (input)
	{
		int i;
		struct UserRec * USER;

		if (strstr(input, "Cancel=Cancel"))
		{
			ReplyLen = SetupNodeMenu(_REPLYBUFFER);	
			goto Sendit;
		}
		user = strtok_s(&input[9], "&", &Context);
		password = strtok_s(NULL, "=", &Context);
		password = strtok_s(NULL, "&", &Context);
		Appl = strtok_s(NULL, "=", &Context);
		Appl = strtok_s(NULL, "&", &Context);

		if (Appl == 0)
			Appl = NoApp;

		for (i = 0; i < TCP->NumberofUsers; i++)
		{
			USER = TCP->UserRecPtr[i];

			if (_stricmp(user, USER->UserName) == 0)
			{
 				if (strcmp(password, USER->Password) == 0)
				{
					// ok

					struct HTTPConnectionInfo * Session = AllocateSession(sock, 'T');

					if (Session)
					{
						char AXCall[10];
						ReplyLen = sprintf(_REPLYBUFFER, TermPage, Mycall, Mycall, Session->Key, Session->Key, Session->Key);
						strcpy(Session->HTTPCall, USER->Callsign);
						ConvToAX25(Session->HTTPCall, AXCall);
						ChangeSessionCallsign(Session->Stream, AXCall);
						BPQHOSTVECTOR[Session->Stream -1].HOSTSESSION->Secure_Session = USER->Secure;
						Session->USER = USER;

						if (Appl[0])
						{
							strcat(Appl, "\r");
							SendMsg(Session->Stream, Appl, strlen(Appl));
						}

					}
					else
					{
						ReplyLen = SetupNodeMenu(_REPLYBUFFER);

						ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", BusyError);
					}
					break;
				}
			}
		}

		if (i == TCP->NumberofUsers)
		{
			//   Not found

			ReplyLen = sprintf(_REPLYBUFFER, TermSignon, Mycall, Mycall, Appl);
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", PassError);
		}
  
	}

Sendit:

	HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));
	send(sock, Header, HeaderLen, 0);
	send(sock, _REPLYBUFFER, ReplyLen, 0);
	send(sock, Tail, strlen(Tail), 0);

	return 1;
}

char * LookupKey(char * Key)
{
	if (strcmp(Key, "##MY_CALLSIGN##") == 0)
	{
		char Mycall[10];
		memcpy(Mycall, &MYNODECALL, 10);
		strlop(Mycall, ' ');

		return _strdup(Mycall);
	}
	return NULL;
}


int ProcessSpecialPage(char * Buffer, int FileSize)
{
	// replaces ##xxx### constructs with the requested data

	char * NewMessage = malloc(100000);
	char * ptr1 = Buffer, * ptr2, * ptr3, * ptr4, * NewPtr = NewMessage;
	int PrevLen;
	int BytesLeft = FileSize;
	int NewFileSize = FileSize;
	char * StripPtr = ptr1;

	// strip comments blocks

	while (ptr4 = strstr(ptr1, "<!--"))
	{
		ptr2 = strstr(ptr4, "-->");
		if (ptr2)
		{
			PrevLen = (ptr4 - ptr1);
			memcpy(StripPtr, ptr1, PrevLen);
			StripPtr += PrevLen;
			ptr1 = ptr2 + 3;
			BytesLeft = FileSize - (ptr1 - Buffer);
		}
	}

	memcpy(StripPtr, ptr1, BytesLeft);
	StripPtr += BytesLeft;

	BytesLeft = StripPtr - Buffer;

	FileSize = BytesLeft;
	NewFileSize = FileSize;
	ptr1 = Buffer;
	ptr1[FileSize] = 0;

loop:
	ptr2 = strstr(ptr1, "##");

	if (ptr2)
	{
		PrevLen = (ptr2 - ptr1);			// Bytes before special text
		
		ptr3 = strstr(ptr2+2, "##");

		if (ptr3)
		{
			char Key[80] = "";
			int KeyLen;
			char * NewText;
			int NewTextLen;

			ptr3 += 2;
			KeyLen = ptr3 - ptr2;

			if (KeyLen < 80)
				memcpy(Key, ptr2, KeyLen);

			NewText = LookupKey(Key);
			
			if (NewText)
			{
				NewTextLen = strlen(NewText);
				NewFileSize = NewFileSize + NewTextLen - KeyLen;					
			//	NewMessage = realloc(NewMessage, NewFileSize);

				memcpy(NewPtr, ptr1, PrevLen);
				NewPtr += PrevLen;
				memcpy(NewPtr, NewText, NewTextLen);
				NewPtr += NewTextLen;

				free(NewText);
				NewText = NULL;
			}
			else
			{
				// Key not found, so just leave

				memcpy(NewPtr, ptr1, PrevLen + KeyLen);
				NewPtr += (PrevLen + KeyLen);
			}

			ptr1 = ptr3;			// Continue scan from here
			BytesLeft = Buffer + FileSize - ptr3;
		}
		else		// Unmatched ##
		{
			memcpy(NewPtr, ptr1, PrevLen + 2);
			NewPtr += (PrevLen + 2);
			ptr1 = ptr2 + 2;
		}
		goto loop;
	}

	// Copy Rest

	memcpy(NewPtr, ptr1, BytesLeft);
	NewMessage[NewFileSize] = 0;

	strcpy(Buffer, NewMessage);
	free(NewMessage);
	
	return NewFileSize;
}

int SendMessageFile(SOCKET sock, char * FN, BOOL OnlyifExists)
{
	int FileSize, Sent, Loops = 0;
	char * MsgBytes;
	char MsgFile[512];
	FILE * hFile;
	int ReadLen;
	BOOL Special = FALSE;
	int Len;
	int HeaderLen;
	char Header[256];
	time_t ctime;
	char TimeString[64];
	char FileTimeString[64];
	struct stat STAT;

#ifdef WIN32

	struct _EXCEPTION_POINTERS exinfo;
	strcpy(EXCEPTMSG, "SendMessageFile");

	__try {
#endif

	if (strlen(FN) > 256)
	{
		FN[256] = 0;
		Debugprintf("HTTP File Name too long %s", FN);
	}

	if (strcmp(FN, "/") == 0)
		if (APRSActive)
			sprintf(MsgFile, "%s/HTML/index.html", BPQDirectory);
		else
			sprintf(MsgFile, "%s/HTML/indexnoaprs.html", BPQDirectory);
	else
		sprintf(MsgFile, "%s/HTML%s", BPQDirectory, FN);

	if (stat(MsgFile, &STAT) == -1)
	{
		if (OnlyifExists)					// Set if we dont want an error response if missing
			return -1;
		
		Len = sprintf(Header, "HTTP/1.1 404 Not Found\r\nContent-Length: 16\r\n\r\nPage not found\r\n");
		send(sock, Header, Len, 0);
		return 0;
	}

	hFile = fopen(MsgFile, "rb");
	
	if (hFile == 0)
	{
		if (OnlyifExists)					// Set if we dont want an error response if missing
			return -1;
		
		Len = sprintf(Header, "HTTP/1.1 404 Not Found\r\nContent-Length: 16\r\n\r\nPage not found\r\n");
		send(sock, Header, Len, 0);
		return 0;
	}
	
	FileSize = STAT.st_size;

	MsgBytes = malloc(FileSize + 1);

	ReadLen = fread(MsgBytes, 1, FileSize, hFile); 

	fclose(hFile);

//	ft.QuadPart -=  116444736000000000;
//	ft.QuadPart /= 10000000;

//	ctime = ft.LowPart;

	FormatTime2(TimeString, STAT.st_ctime);

	// if HTML file, look for ##...## substitutions

	if ((strcmp(FN, "/") == 0 || strstr(FN, "htm" ) || strstr(FN, "HTM")) &&  strstr(MsgBytes, "##" ))
	{
		FileSize = ProcessSpecialPage(MsgBytes, FileSize); 
		ctime = time(NULL);
	}

	FormatTime2(FileTimeString, STAT.st_ctime);
	FormatTime2(TimeString, time(NULL));

	HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n"
		"Content-Type: text/html\r\n"
		"Date: %s\r\n"
		"Last-Modified: %s\r\n" 
		"\r\n", FileSize, TimeString, FileTimeString);

	send(sock, Header, HeaderLen, 0);

	Sent = send(sock, MsgBytes, FileSize, 0);
		
	while (Sent != FileSize && Loops++ < 3000)				// 100 secs max
	{	
		if (Sent > 0)					// something sent
		{
			FileSize -= Sent;
			memmove(MsgBytes, &MsgBytes[Sent], FileSize);
		}
					
		Sleep(30);
		Sent = send(sock, MsgBytes, FileSize, 0);
//		Debugprintf("%d out of %d sent", Sent, FileSize);
	}

	free (MsgBytes);

#ifdef WIN32
	}
	#include "StdExcept.c"
	Debugprintf("Sending FIle %s", FN);
	}
#endif

	return 0;
}

VOID sendandcheck(SOCKET sock, const char * Buffer, int Len)
{
	int Loops = 0;
	int Sent = send(sock, Buffer, Len, 0);
	char * Copy = NULL;
		
	while (Sent != Len && Loops++ < 300)					// 10 secs max
	{	
//		Debugprintf("%d out of %d sent %d Loops", Sent, Len, Loops);
				
		if (Copy == NULL)
		{
			Copy = malloc(Len);
			memcpy(Copy, Buffer, Len);
		}

		if (Sent > 0)					// something sent
		{
			Len -= Sent;
			memmove(Copy, &Copy[Sent], Len);
		}		
		
		Sleep(30);
		Sent = send(sock, Copy, Len, 0);
	}

	if (Copy)
		free(Copy);

	return;
}

int RefreshTermWindow(struct HTTPConnectionInfo * Session, char * _REPLYBUFFER)
{
	char Msg[400] = "";
	int HeaderLen, ReplyLen;
	char Header[256];
	
	PollSession(Session);			// See if anything received 

	if (Session->Changed)
	{
				int Last = Session->LastLine;
				int n;
	
				strcpy(_REPLYBUFFER, TermOutput);

				for (n = Last;;)
				{
					strcat(_REPLYBUFFER, Session->ScreenLines[n]);

					if (n == 99)
						n = -1;
	
					if (++n == Last)
						break;
				}

				Session->Changed = 0;

				ReplyLen = strlen(_REPLYBUFFER);
				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", TermOutputTail);

				HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen);
				sendandcheck(Session->sock, Header, HeaderLen);
				sendandcheck(Session->sock, _REPLYBUFFER, ReplyLen);

				return 1;
	}
	else
		return 0;
}


struct TNCINFO * TNCInfo[34];	

int SetupNodeMenu(char * Buff)
{
	int Len = 0, i;
	struct TNCINFO * TNC;

	Len = sprintf(Buff, NodeMenuHeader, Mycall);
		
	for (i=1; i<33; i++)
	{
		TNC = TNCInfo[i];
		if (TNC == NULL)
			continue;
	
		if (TNC->WebWindowProc)
			Len += sprintf(&Buff[Len], NodeMenuLine, i, TNC->WebWinX, TNC->WebWinY);
	}
		
	Len += sprintf(&Buff[Len], NodeMenuRest, Mycall,
		DriverBit,
		(APRSWeb)?APRSBit:"",
		(IncludesMail)?MailBit:"", (IncludesChat)?ChatBit:"", NodeTail);

	return Len;
}

VOID SaveConfigFile(SOCKET sock , char * MsgPtr, char * Rest)
{
	int ReplyLen = 0;
	char * ptr, * ptr1, * ptr2, *input;
	char c;
	int MsgLen, WriteLen = 0;
	char inputname[250]="bpqcfg.txt";
	FILE *fp1;
	char Header[256];
	int HeaderLen;
	char Reply[4096];
	char Mess[80] = "Configuration Saved";

	input = strstr(MsgPtr, "\r\n\r\n");	// End of headers

	if (input)
	{
		if (strstr(input, "Cancel=Cancel"))
		{
			ReplyLen = SetupNodeMenu(Reply);
//			ReplyLen = sprintf(Reply, "%s", "<html><script>window.close();</script></html>");
			HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));
			send(sock, Header, HeaderLen, 0);
			send(sock, Reply, ReplyLen, 0);
			send(sock, Tail, strlen(Tail), 0);
			return;
		}

		ptr = strstr(input, "&Save=");

		if (ptr)
		{
			*ptr = 0;
		
			// Undo any % transparency

			ptr1 = ptr2 = input + 8;

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

					c  = m * 16 + n;
				}
				else if (c == '+')
					c = ' ';

#ifndef WIN32
				if (c != 13)				// Strip CR if Linux
#endif
					*(ptr2++) = c;

				c = *(ptr1++);
		
			}

			*(ptr2++) = 0;

			MsgLen = strlen(input + 8);
	
			if (BPQDirectory[0] == 0)
			{
				strcpy(inputname, "bpq32.cfg");
			}
				else
			{
				strcpy(inputname,BPQDirectory);
				strcat(inputname,"/");
				strcat(inputname, "bpq32.cfg");
			}

			fp1 = fopen(inputname, "wb");
	
			if (fp1)
			{
				WriteLen = fwrite(input + 8, 1, MsgLen, fp1); 
				fclose(fp1);
			}

			if (WriteLen != MsgLen)
				sprintf_s(Mess, sizeof(Mess), "Failed to write Config File");
		}
	
		ReplyLen = sprintf(Reply, "<html><script>alert(\"%s\");window.close();</script></html>", Mess);
		HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));
		send(sock, Header, HeaderLen, 0);
		send(sock, Reply, ReplyLen, 0);
		send(sock, Tail, strlen(Tail), 0);
	}
	return;
}





int InnerProcessHTTPMessage(struct ConnectionInfo * conn)
{
	struct TCPINFO * TCP = conn->TNC->TCPInfo;
	SOCKET sock = conn->socket;
	char * MsgPtr = conn->InputBuffer;
	int MsgLen = conn->InputLen;
	int InputLen = 0;
	int OutputLen = 0;
	int Bufferlen;
	struct HTTPConnectionInfo CI;
	struct HTTPConnectionInfo * sockptr = &CI;
	struct HTTPConnectionInfo * Session = NULL;

	HANDLE hPipe;
	char URL[100000];
	char * ptr;
	char * Context, * Method, * NodeURL, * Key;
	int ReplyLen = 0;
	char Header[256];
	int HeaderLen;
	char TimeString[64];
	BOOL LOCAL = FALSE;
	BOOL COOKIE = FALSE;
	char Reply[100000];
	int Len;

#ifdef WIN32

	struct _EXCEPTION_POINTERS exinfo;
	strcpy(EXCEPTMSG, "ProcessHTTPMessage");

	__try {
#endif

	Len = strlen(MsgPtr);
	if (Len > 100000)
		return 0; 

	strcpy(URL, MsgPtr);

	if (strstr(MsgPtr, "Host: 127.0.0.1"))
		LOCAL = TRUE;

	ptr = strstr(MsgPtr, "BPQSessionCookie=N");

	if (ptr)
	{
		COOKIE = TRUE;
		Key = ptr + 17;
		ptr = strchr(Key, ',');
		if (ptr)
		{
			*ptr = 0;
			Session = FindSession(Key);
			*ptr = ',';
		}
		else
		{
			ptr = strchr(Key, 13);
			if (ptr)
			{
				*ptr = 0;
				Session = FindSession(Key);
				*ptr = 13;
			}
		}
	}

	ptr = strstr(URL, " HTTP");

	if (ptr)
		*ptr = 0;

	Method = strtok_s(URL, " ", &Context);

	memcpy(Mycall, &MYNODECALL, 10);
	strlop(Mycall, ' ');

	// APRS is internal

	if (_memicmp(Context, "/APRS/", 6) == 0)
	{
		APRSProcessHTTPMessage(sock, MsgPtr);
		return 0;
	}

/*

	// Windows - Pass to BPQAPRS via Pipi

	if (_memicmp(Context, "/APRS/", 6) == 0)
	{
		// If for APRS, Pass to APRS Server via Named Pipe

		char _REPLYBUFFER[8192];

		hPipe = CreateFile(APRSPipeFileName, GENERIC_READ | GENERIC_WRITE,
                  0,                    // exclusive access
                  NULL,                 // no security attrs
                  OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, 
                  NULL );

		if (hPipe == (HANDLE)-1)
		{
			InputLen = sprintf(_REPLYBUFFER, "HTTP/1.1 404 Not Found\r\nContent-Length: 28\r\n\r\nAPRS Data is not available\r\n");
			send(sock, _REPLYBUFFER, InputLen, 0);
		}
		else
		{
			int Sent;
			int Loops = 0;
			WriteFile(hPipe, MsgPtr, MsgLen, &InputLen, NULL);

			while(TRUE)
			{
				ReadFile(hPipe, _REPLYBUFFER, 8192, &InputLen, NULL);
				if (InputLen <= 0)
				{
					InputLen = GetLastError();
					break;
				}
				Sent = send(sock, _REPLYBUFFER, InputLen, 0);
		
				while (Sent != InputLen && Loops++ < 3000)					// 100 secs max
				{	
//					Debugprintf("%d out of %d sent %d Loops", Sent, InputLen, Loops);
				
					if (Sent > 0)					// something sent
					{
						InputLen -= Sent;
						memmove(_REPLYBUFFER, &_REPLYBUFFER[Sent], InputLen);
					}
					
					Sleep(30);
					Sent = send(sock, _REPLYBUFFER, InputLen, 0);
				}
			}
			CloseHandle(hPipe);
		}
		return 0;
	}

*/
	

	if (_stricmp(Context, "/Node/Signon?Node") == 0)
	{
		char * IContext;

		NodeURL = strtok_s(Context, "?", &IContext);
		Key = strtok_s(NULL, "?", &IContext);
		
		ProcessNodeSignon(sock, TCP, MsgPtr, Key, Reply, &Session);
		return 0;
		
	}


	// If for Mail or Chat, check for a session, and send login screen if necessary

	// Moved here to simplify operation with both internal and external clients

	if (_memicmp(Context, "/Mail/", 6) == 0)
	{
		int RLen = 0;
		char Appl;
		char * input;
		char * IContext;
	
	NodeURL = strtok_s(Context, "?", &IContext);
	Key = strtok_s(NULL, "?", &IContext);

	if (_stricmp(NodeURL, "/Mail/Signon") == 0)
	{
		ReplyLen = ProcessMailSignon(TCP, MsgPtr, Key, Reply, &Session, FALSE);
		
		if (ReplyLen)
		{
			goto Returnit;
		}
	
#ifdef LINBPQ
		strcpy(Context, "/Mail/Header");
#else
		strcpy(MsgPtr, "POST /Mail/Header");
#endif
		goto doHeader;
	}

	if (_stricmp(NodeURL, "/Mail/Lost") == 0)
	{
		input = strstr(MsgPtr, "\r\n\r\n");	// End of headers

		if (input && strstr(input, "Cancel=Exit"))
		{
			ReplyLen = SetupNodeMenu(Reply);
			RLen = ReplyLen;
			goto Returnit;
		}
		if (Key)
			Appl = Key[0];

		Key = 0;
	}

	if (Key == 0 || Key[0] == 0)
	{
		// No Session 

		// if not local send a signon screen, else create a user session

		if (LOCAL || COOKIE)
		{
			Session = AllocateSession(sock, 'M');

			if (Session)
			{
				strcpy(Context, "/Mail/Header");
				goto doHeader;
			}
			else
			{
				ReplyLen = SetupNodeMenu(Reply);
				ReplyLen += sprintf(&Reply[ReplyLen], "%s", BusyError);
			}
			RLen = ReplyLen;

			goto Returnit;

		}

		ReplyLen = sprintf(Reply, MailSignon, Mycall, Mycall);

		RLen = ReplyLen;
			goto Returnit;
	}

	Session = FindSession(Key);

	if (Session == NULL)
	{
		ReplyLen = sprintf(Reply, MailLostSession, Key);
		RLen = ReplyLen;
		goto Returnit;
	}
	}

	if (_memicmp(Context, "/Chat/", 6) == 0)
	{
		int RLen = 0;
		char Appl;
		char * input;
		char * IContext;
	

	if (strstr(MsgPtr, "Host: 127.0.0.1"))
		LOCAL = TRUE;

	NodeURL = strtok_s(Context, "?", &IContext);
	Key = strtok_s(NULL, "?", &IContext);

	if (_stricmp(NodeURL, "/Chat/Signon") == 0)
	{
		ReplyLen = ProcessChatSignon(TCP, MsgPtr, Key, Reply, &Session);

		if (ReplyLen)
		{
			goto Returnit;
		}
		
#ifdef LINBPQ
		strcpy(Context, "/Chat/Header");
#else
		strcpy(MsgPtr, "POST /Chat/Header");
#endif
		goto doHeader;
	}

	if (_stricmp(NodeURL, "/Chat/Lost") == 0)
	{
		input = strstr(MsgPtr, "\r\n\r\n");	// End of headers

		if (input && strstr(input, "Cancel=Exit"))
		{
			ReplyLen = SetupNodeMenu(Reply);
			RLen = ReplyLen;
			goto Returnit;
		}
		if (Key)
			Appl = Key[0];

		Key = 0;
	}

	if (Key == 0 || Key[0] == 0)
	{
		// No Session 

		// if not local send a signon screen, else create a user session

		if (LOCAL || COOKIE)
		{
			Session = AllocateSession(sock, 'C');

			if (Session)
			{
				strcpy(Context, "/CHat/Header");
				goto doHeader;
			}
			else
			{
				ReplyLen = SetupNodeMenu(Reply);
				ReplyLen += sprintf(&Reply[ReplyLen], "%s", BusyError);
			}
			RLen = ReplyLen;

			goto Returnit;

		}
		
		ReplyLen = sprintf(Reply, ChatSignon, Mycall, Mycall);

		RLen = ReplyLen;
			goto Returnit;
	}

	Session = FindSession(Key);

	if (Session == NULL)
	{
		ReplyLen = sprintf(Reply, MailLostSession, Key);
		RLen = ReplyLen;
Returnit:
		if (memcmp(Reply, "HTTP", 4) == 0)
		{
			// Full Header provided by appl - just send it
			
			send(sock, Reply, ReplyLen, 0);
			return 0;
		}
		
		HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));	
		send(sock, Header, HeaderLen, 0);
		send(sock, Reply, ReplyLen, 0);
		send(sock, Tail, strlen(Tail), 0);

		return 0;
	}
	}

doHeader:

#ifdef LINBPQ

	if ((_memicmp(Context, "/MAIL/", 6) == 0) || (_memicmp(Context, "/WebMail", 8) == 0))
	{
		char _REPLYBUFFER[100000];
		struct HTTPConnectionInfo Dummy = {0};

		ReplyLen = 0;

		if (Session == 0)
			Session = &Dummy;

		ProcessMailHTTPMessage(Session, Method, Context, MsgPtr, _REPLYBUFFER, &ReplyLen);

		if (memcmp(_REPLYBUFFER, "HTTP", 4) == 0)
		{
			// Full Header provided by appl - just send it
			
			send(sock, _REPLYBUFFER, ReplyLen, 0);
			return 0;
		}
		
		HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));	

		HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));	
		send(sock, Header, HeaderLen, 0);
		send(sock, _REPLYBUFFER, ReplyLen, 0);
		send(sock, Tail, strlen(Tail), 0);

		return 0;

	}

	if (_memicmp(Context, "/CHAT/", 6) == 0)
	{
		char _REPLYBUFFER[100000];

		ReplyLen = 0;

		ProcessChatHTTPMessage(Session, Method, Context, MsgPtr, _REPLYBUFFER, &ReplyLen);

		HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));	
		send(sock, Header, HeaderLen, 0);
		send(sock, _REPLYBUFFER, ReplyLen, 0);
		send(sock, Tail, strlen(Tail), 0);

		return 0;

	}

		
/*
		Sent = send(sock, _REPLYBUFFER, InputLen, 0);
		
		while (Sent != InputLen && Loops++ < 3000)					// 100 secs max
		{	
//					Debugprintf("%d out of %d sent %d Loops", Sent, InputLen, Loops);
		
			if (Sent > 0)					// something sent
			{
				InputLen -= Sent;
				memmove(_REPLYBUFFER, &_REPLYBUFFER[Sent], InputLen);
			}
					
			Sleep(30);
			Sent = send(sock, _REPLYBUFFER, InputLen, 0);
		}
		return 0;
	}
*/
#else

	// Pass to MailChat if active

	if ((_memicmp(Context, "/MAIL/", 6) == 0) || (_memicmp(Context, "/WebMail", 8) == 0))
	{
		// If for Mail, Pass to Mail Server via Named Pipe

		hPipe = CreateFile(MAILPipeFileName, GENERIC_READ | GENERIC_WRITE,
                  0,                    // exclusive access
                  NULL,                 // no security attrs
                  OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, 
                  NULL );

		if (hPipe == (HANDLE)-1)
		{
			InputLen = sprintf(Reply, "HTTP/1.1 404 Not Found\r\nContent-Length: 28\r\n\r\nMail Data is not available\r\n");
			send(sock, Reply, InputLen, 0);
		}
		else
		{
//			int Sent;
			int Loops = 0;
			struct HTTPConnectionInfo Dummy = {0};

			if (Session == 0)
				Session = &Dummy;

			WriteFile(hPipe, Session, sizeof (struct HTTPConnectionInfo), &InputLen, NULL);
			WriteFile(hPipe, MsgPtr, MsgLen, &InputLen, NULL);

		
			ReadFile(hPipe, Session, sizeof (struct HTTPConnectionInfo), &InputLen, NULL);
			ReadFile(hPipe, Reply, 100000, &ReplyLen, NULL);
			if (ReplyLen <= 0)
			{
				InputLen = GetLastError();
			}

			CloseHandle(hPipe);
			goto Returnit;
		}
		return 0;
	}

	if (_memicmp(Context, "/CHAT/", 6) == 0)
	{
		hPipe = CreateFile(CHATPipeFileName, GENERIC_READ | GENERIC_WRITE,
                  0,                    // exclusive access
                  NULL,                 // no security attrs
                  OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, 
                  NULL );

		if (hPipe == (HANDLE)-1)
		{
			InputLen = sprintf(Reply, "HTTP/1.1 404 Not Found\r\nContent-Length: 28\r\n\r\nMail Data is not available\r\n");
			send(sock, Reply, InputLen, 0);
		}
		else
		{
//			int Sent;
			int Loops = 0;

			WriteFile(hPipe, Session, sizeof (struct HTTPConnectionInfo), &InputLen, NULL);
			WriteFile(hPipe, MsgPtr, MsgLen, &InputLen, NULL);

		
			ReadFile(hPipe, Session, sizeof (struct HTTPConnectionInfo), &InputLen, NULL);
			ReadFile(hPipe, Reply, 100000, &ReplyLen, NULL);
			if (ReplyLen <= 0)
			{
				InputLen = GetLastError();
			}

			CloseHandle(hPipe);
			goto Returnit;
		}
		return 0;
	}

#endif

	NodeURL = strtok_s(NULL, "?", &Context);

	if (strcmp(Method, "POST") == 0)
	{
		char _REPLYBUFFER[8192];

		if (_stricmp(NodeURL, "/TermInput") == 0)
		{
			ProcessTermInput(sock, MsgPtr, MsgLen, Context);
			return 0;
		}

		if (_stricmp(NodeURL, "/Node/TermSignon") == 0)
		{
			ProcessTermSignon(conn->TNC, sock, MsgPtr, MsgLen);
		}

		if (_stricmp(NodeURL, "/Node/Signon") == 0)
		{
			ProcessNodeSignon(sock, TCP, MsgPtr, Key, Reply, &Session);
			return 0;
		}

		if (_stricmp(NodeURL, "/Node/TermClose") == 0)
		{
			ProcessTermClose(sock, MsgPtr, MsgLen, Context);
			return 0;
		}

		if (_stricmp(NodeURL, "/Node/BeaconAction") == 0)
		{
			char _REPLYBUFFER[8192];
			int ReplyLen;
			char Header[256];
			int HeaderLen;
			char * input = strstr(MsgPtr, "\r\n\r\n");	// End of headers
			int Port;
			char Param[100];
			int retCode, disp;
			char Key[80];
			HKEY hKey;


			if (LOCAL == FALSE && COOKIE == FALSE)
			{
				//	Send Not Authorized

				ReplyLen = SetupNodeMenu(_REPLYBUFFER);	
				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<br><B>Not authorized - please sign in</B>");
				HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));
				send(sock, Header, HeaderLen, 0);
				send(sock, _REPLYBUFFER, ReplyLen, 0);
				send(sock, Tail, strlen(Tail), 0);

				return 1;
			}



			if (strstr(input, "Cancel=Cancel"))
			{
				ReplyLen = SetupNodeMenu(_REPLYBUFFER);	
				HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));
				send(sock, Header, HeaderLen, 0);
				send(sock, _REPLYBUFFER, ReplyLen, 0);
				send(sock, Tail, strlen(Tail), 0);

				return 1;
			}

			GetParam(input, "Port", &Param);
			Port = atoi(&Param[1]);
			GetParam(input, "Every", &Param);
			Interval[Port] = atoi(&Param[1]);


//extern char * UIUIDigi[33];
//extern char UIUIDEST[33][11];		// Dest for Beacons
//extern UCHAR FN[33][256];			// Filename
//extern int [33];			// Beacon Interval (Mins)
//extern char Message[33][1000];		// Beacon Text


			GetParam(input, "Dest", &Param);
			strcpy(UIUIDEST[Port], &Param[1]);

			GetParam(input, "Path", &Param);
			if (UIUIDigi[Port])
				free(UIUIDigi[Port]);
			UIUIDigi[Port] = _strdup(&Param[1]);

			GetParam(input, "File", &Param);
			strcpy(FN[Port], &Param[1]);
			GetParam(input, "Text", &Param);
			strcpy(Message[Port], &Param[1]);
		
			MinCounter[Port] = Interval[Port];

			SendFromFile[Port] = 0;
			
			if (FN[Port][0])
				SendFromFile[Port] = 1;

			SetupUI(Port);

#ifdef LINBPQ
			SaveUIConfig();
#else
			
			wsprintf(Key, "SOFTWARE\\G8BPQ\\BPQ32\\UIUtil\\UIPort%d", Port);

			retCode = RegCreateKeyEx(REGTREE,
					Key, 0, 0, 0, KEY_ALL_ACCESS, NULL, &hKey, &disp);
	
			if (retCode == ERROR_SUCCESS)
			{
				retCode = RegSetValueEx(hKey, "UIDEST", 0, REG_SZ,(BYTE *)&UIUIDEST[Port][0], strlen(&UIUIDEST[Port][0]));
				retCode = RegSetValueEx(hKey, "FileName", 0, REG_SZ,(BYTE *)&FN[Port][0], strlen(&FN[Port][0]));
				retCode = RegSetValueEx(hKey, "Message", 0, REG_SZ,(BYTE *)&Message[Port][0], strlen(&Message[Port][0]));
				retCode = RegSetValueEx(hKey, "Interval", 0, REG_DWORD,(BYTE *)&Interval[Port], 4);
				retCode = RegSetValueEx(hKey, "SendFromFile", 0, REG_DWORD,(BYTE *)&SendFromFile[Port], 4);
				retCode = RegSetValueEx(hKey, "Digis",0, REG_SZ, UIUIDigi[Port], strlen(UIUIDigi[Port]));

				RegCloseKey(hKey);
			}
#endif

			if (strstr(input, "Test=Test"))
				SendUIBeacon(Port);
		}

		if (_stricmp(NodeURL, "/Node/CfgSave") == 0)
		{
			//	Save Config File

			SaveConfigFile(sock, MsgPtr, Key);
			return 0;
		}
	
		send(sock, _REPLYBUFFER, InputLen, 0);
		return 0;
	}

	if (_stricmp(NodeURL, "/") == 0 || _stricmp(NodeURL, "/Index.html") == 0)
	{		
		// Send if present, else use default

		Bufferlen = SendMessageFile(sock, NodeURL, TRUE);		// return -1 if not found
		
		if (Bufferlen != -1)
			return 0;						// We've sent it
		else
		{
			char _REPLYBUFFER[1000];
	
			if (APRSApplConnected)
				ReplyLen = sprintf(_REPLYBUFFER, Index, Mycall, Mycall);
			else
				ReplyLen = sprintf(_REPLYBUFFER, IndexNoAPRS, Mycall, Mycall);

			HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));	
			send(sock, Header, HeaderLen, 0);
			send(sock, _REPLYBUFFER, ReplyLen, 0);
			send(sock, Tail, strlen(Tail), 0);

			return 0;
		}
	}

	else if (_stricmp(NodeURL, "/NodeMenu.html") == 0 || _stricmp(NodeURL, "/Node/NodeMenu.html") == 0)
	{		
		// Send if present, else use default

		Bufferlen = SendMessageFile(sock, "/NodeMenu.html", TRUE);		// return -1 if not found
		
		if (Bufferlen != -1)
			return 0;											// We've sent it
	}

	else if (_memicmp(NodeURL, "/NODE/", 6))
	{
		// Not Node, See if a local file
		
		Bufferlen = SendMessageFile(sock, NodeURL, FALSE);		// Send error if not found
		return 0;
	}

	// Node URL

	{

	char _REPLYBUFFER[100000];

	ReplyLen = SetupNodeMenu(_REPLYBUFFER);

	if (_stricmp(NodeURL, "/Node/Port") == 0)
	{
		int port = atoi(Context);

		if (port > 0 && port < 33)
		{
			struct TNCINFO * TNC = TNCInfo[port];

			if (TNC && TNC->WebWindowProc)
				ReplyLen = TNC->WebWindowProc(TNC, _REPLYBUFFER, LOCAL);
		}
		
	}

	else if (_stricmp(NodeURL, "/Node/Streams") == 0)
	{
		ReplyLen = StatusProc(_REPLYBUFFER);
	}

	else if (_stricmp(NodeURL, "/Node/Stats.html") == 0)
	{
		struct tm * TM;
		char UPTime[50];
		time_t szClock = STATSTIME * 60;

		TM = gmtime(&szClock);

		sprintf(UPTime, "%.2d:%.2d:%.2d", TM->tm_yday, TM->tm_hour, TM->tm_min);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", StatsHddr);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td>%s</td><td align=right>%s</td></tr>",
			"Version", VersionString);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td>%s</td><td align=right>%s</td></tr>",
			"Uptime (Days Hours Mins)", UPTime);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td>%s</td><td align=right>%d</td><td align=right>%d</td></tr>",
		"Semaphore: Get-Rel/Clashes", Semaphore.Gets - Semaphore.Rels, Semaphore.Clashes);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td>%s</td><td align=right>%d</td><td align=right>%d</td><td align=right>%d</td><td align=right>%d</td align=right><td align=right>%d</td></tr>",
			"Buffers: Max/Cur/Min/Out/Wait", MAXBUFFS, QCOUNT, MINBUFFCOUNT, NOBUFFCOUNT, BUFFERWAITS);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td>%s</td><td align=right>%d</td><td align=right>%d</td></tr>",
		"Known Nodes/Max Nodes", NUMBEROFNODES, MAXDESTS);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td>%s</td><td align=right>%d</td><td align=right>%d</td></tr>",
		"L4 Connects Sent/Rxed ", L4CONNECTSOUT, L4CONNECTSIN);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td>%s</td><td align=right>%d</td><td align=right>%d</td><td align=right>%d</td align=right><td align=right>%d</td></tr>",
			"L4 Frames TX/RX/Resent/Reseq", L4FRAMESTX, L4FRAMESRX, L4FRAMESRETRIED, OLDFRAMES);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td>%s</td><td align=right>%d</td></tr>",
		"L3 Frames Relayed", L3FRAMES);

	}
	else if (_stricmp(NodeURL, "/Node/EditCfg.html") == 0)
	{
		char * _REPLYBUFFER;
		int ReplyLen;
		char Header[256];
		int HeaderLen;
		char * CfgBytes;
		int CfgLen;
		char inputname[250]="bpqcfg.txt";
		FILE *fp1;
		struct stat STAT;
		char DummyKey[] = "DummyKey";

		if (LOCAL == FALSE && COOKIE == FALSE)
		{
			//	Send Not Authorized

			char _REPLYBUFFER[1000];	
			ReplyLen = SetupNodeMenu(_REPLYBUFFER);	
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<br><B>Not authorized - please sign in</B>");
			HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));
			send(sock, Header, HeaderLen, 0);
			send(sock, _REPLYBUFFER, ReplyLen, 0);
			send(sock, Tail, strlen(Tail), 0);
			return 1;
		}

		if (COOKIE ==FALSE)
			Key = DummyKey;

		if (BPQDirectory[0] == 0)
		{
			strcpy(inputname, "bpq32.cfg");
		}
			else
		{
			strcpy(inputname,BPQDirectory);
			strcat(inputname,"/");
			strcat(inputname, "bpq32.cfg");
		}

		
		if (stat(inputname, &STAT) == -1)
		{
			CfgBytes = _strdup("Config File not found");
		}
		else
		{
			fp1 = fopen(inputname, "rb");
	
			if (fp1 == 0)
			{
				CfgBytes = _strdup("Config File not found");
			}
			else
			{	
				CfgLen = STAT.st_size;

				CfgBytes = malloc(CfgLen + 1);

				CfgLen = fread(CfgBytes, 1, CfgLen, fp1); 
				CfgBytes[CfgLen] = 0;
			}		
		}

		_REPLYBUFFER = malloc(CfgLen + 1000);

		ReplyLen = sprintf(_REPLYBUFFER, ConfigEditPage, Key, CfgBytes);
		free (CfgBytes);

		HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));
		sendandcheck(sock, Header, HeaderLen);
		sendandcheck(sock, _REPLYBUFFER, ReplyLen);
		sendandcheck(sock, Tail, strlen(Tail));
		free (_REPLYBUFFER);

		return 1;
	}

	if (_stricmp(NodeURL, "/Node/PortBeacons") == 0)
	{
		char * PortChar = strtok_s(NULL, "&", &Context);
		int PortNo = atoi(PortChar);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], Beacons, PortNo,
			Interval[PortNo], &UIUIDEST[PortNo][0], &UIUIDigi[PortNo][0], &FN[PortNo][0], &Message[PortNo][0], PortNo);
	}



	if (_stricmp(NodeURL, "/Node/PortStats") == 0)
	{
		struct _EXTPORTDATA * Port;

		char * PortChar = strtok_s(NULL, "&", &Context);
		int PortNo = atoi(PortChar);
		int Protocol;
		int PortType;
		
		//		char PORTTYPE;	// H/W TYPE
		// 0 = ASYNC, 2 = PC120, 4 = DRSI
					// 6 = TOSH, 8 = QUAD, 10 = RLC100
					// 12 = RLC400 14 = INTERNAL 16 = EXTERNAL

#define KISS 0
#define NETROM 2
#define HDLC 6
#define L2 8
#define WINMOR 10


		// char PROTOCOL;	// PORT PROTOCOL
					// 0 = KISS, 2 = NETROM, 4 = BPQKISS
					//; 6 = HDLC, 8 = L2

	
		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsHddr, PortNo);
		
		Port = (struct _EXTPORTDATA *)GetPortTableEntryFromPortNum(PortNo);
		
		if (Port == NULL)
		{
			ReplyLen = sprintf(_REPLYBUFFER, "Invalid Port");
			goto SendResp;
		}
		
		Protocol = Port->PORTCONTROL.PROTOCOL;
		PortType = Port->PORTCONTROL.PROTOCOL;

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "L2 Frames Digied", Port->PORTCONTROL.L2DIGIED);
		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "L2 Frames Heard", Port->PORTCONTROL.L2FRAMES);
		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "L2 Frames Received", Port->PORTCONTROL.L2FRAMESFORUS);
		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "L2 Frames Sent", Port->PORTCONTROL.L2FRAMESSENT);
		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "L2 Timeouts", Port->PORTCONTROL.L2TIMEOUTS);
		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "REJ Frames Received", Port->PORTCONTROL.L2REJCOUNT);
		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "RX out of Seq", Port->PORTCONTROL.L2OUTOFSEQ);
//		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "L2 Resequenced", Port->PORTCONTROL.L2RESEQ);
		if (Protocol == HDLC)
		{
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "Underrun", Port->PORTCONTROL.L2URUNC);
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "RX Overruns", Port->PORTCONTROL.L2ORUNC);
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "RX CRC Errors", Port->PORTCONTROL.RXERRORS);
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "Frames abandoned", Port->PORTCONTROL.L1DISCARD);
		}
		else if ((Protocol == KISS && Port->PORTCONTROL.KISSFLAGS) || Protocol == NETROM)
		{
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "Poll Timeout", Port->PORTCONTROL.L2URUNC);
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "RX CRC Errors", Port->PORTCONTROL.RXERRORS);
		}

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "FRMRs Sent", Port->PORTCONTROL.L2FRMRTX);
		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortStatsLine, "FRMRs Received", Port->PORTCONTROL.L2FRMRRX);

//		DB	'Link Active %   '
//		DD	AVSENDING

	}

	if (_stricmp(NodeURL, "/Node/Ports.html") == 0)
	{
		struct _EXTPORTDATA * ExtPort;
		struct PORTCONTROL * Port;

		int count;
		char DLL[20];

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", PortsHddr);
		
		for (count = 1; count <= NUMBEROFPORTS; count++)
		{
			Port = GetPortTableEntryFromSlot(count);
			ExtPort = (struct _EXTPORTDATA *)Port;
	
			if (Port->PORTTYPE == 0x10)
			{	
				strcpy(DLL, ExtPort->PORT_DLL_NAME);
				strlop(DLL, '.');
			}
			else if (Port->PORTTYPE == 0)
				strcpy(DLL, "ASYNC");

			else if (Port->PORTTYPE == 14)
				strcpy(DLL, "INTERNAL");
	
			else if (Port->PORTTYPE > 0 && Port->PORTTYPE < 14)
				strcpy(DLL, "HDLC");



		if (Port->PORTTYPE == 16 && Port->PROTOCOL == 10 && Port->UICAPABLE == 0)		// EXTERNAL, Pactor/WINMO
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], SessionPortLine, Port->PORTNUMBER, DLL, Port->PORTDESCRIPTION);
		else
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], PortLineWithBeacon, Port->PORTNUMBER, Port->PORTNUMBER,
					DLL, DLL, Port->PORTDESCRIPTION, Port->PORTNUMBER);
		}
	}

	if (_stricmp(NodeURL, "/Node/Nodes.html") == 0)
	{
		struct DEST_LIST * Dests = DESTS;
		int count, i;
		char Normcall[10];
		char Alias[10];
		int Width = 5;
		int x = 0, n = 0;
		struct DEST_LIST * List[1000];
		char Param = 0;

		if (Context)
		{
			_strupr(Context);
			Param = Context[0];
		}

		for (count = 0; count < MAXDESTS; count++)
		{
			if (Dests->DEST_CALL[0] != 0)
			{
				if (Param != 'T' || Dests->DEST_COUNT)
					List[n++] = Dests;

				if (n > 999)
					break;
			}

			Dests++;
		}

		if (n > 1)
		{
			if (Param == 'C') 
				qsort(List, n, 4, CompareNode);
			else
				qsort(List, n, 4, CompareAlias);
		}
		
		Alias[6] = 0; 

		if (Param == 'T')
		{
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], NodeHddr, "with traffic");
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<td>Call</td><td>Frames</td><td>RTT</td><td>BPQ?</td><td>Hops</td></tr><tr>");
		}
		else if (Param == 'C') 
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], NodeHddr, "sorted by Call");
		else
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], NodeHddr, "sorted by Alias");

		for (i = 0; i < n; i++)
		{
			int len = ConvFromAX25(List[i]->DEST_CALL, Normcall);
			Normcall[len]=0;
		
			memcpy(Alias, List[i]->DEST_ALIAS, 6);
			strlop(Alias, ' ');

			if (Param == 'T')
			{
				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<td>%s:%s</td><td align=center>%d</td><td align=center>%d</td><td align=center>%c</td><td align=center>%.0d</td></tr><tr>",
					Normcall, Alias, List[i]->DEST_COUNT, List[i]->DEST_RTT /16,
					(List[i]->DEST_STATE & 0x40)? 'B':' ', (List[i]->DEST_STATE & 63));

			}
			else
			{
				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], NodeLine, Normcall, Alias, Normcall);

				if (++x == Width)
				{
					x = 0;
					ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "</tr><tr>");
				}
			}
		}

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "</tr>");
	}

	if (_stricmp(NodeURL, "/Node/NodeDetail") == 0)
	{
		UCHAR AXCall[8];
		struct DEST_LIST * Dest = DESTS;
		struct NR_DEST_ROUTE_ENTRY * NRRoute;
		struct ROUTE * Neighbour;
		char Normcall[10];
		int i, len, count, Active;
		char Alias[7];

		Alias[6] = 0;

		_strupr(Context);

		ConvToAX25(Context, AXCall);

		for (count = 0; count < MAXDESTS; count++)
		{
			if (CompareCalls(Dest->DEST_CALL, AXCall))
			{
				break;
			}
			Dest++;
		}
		
		if (count == MAXDESTS)
		{
			ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<h3 align = center>Call %s not found</h3>", Context);
			goto SendResp;
		}

		memcpy(Alias, Dest->DEST_ALIAS, 6);
		strlop(Alias, ' ');

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen],
			"<h3 align=center>Info for Node %s:%s</h3><p style=font-family:monospace align=center>", Alias, Context);

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<table border=1 bgcolor=white><tr><td>Frames</td><td>RTT</td><td>BPQ?</td><td>Hops</td></tr>");	

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td align=center>%d</td><td align=center>%d</td><td align=center>%c</td><td align=center>%.0d</td></tr></table>",
				Dest->DEST_COUNT, Dest->DEST_RTT /16,
				(Dest->DEST_STATE & 0x40)? 'B':' ', (Dest->DEST_STATE & 63));

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<h3 align=center>Neighbours</h3>");

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], 
			"<table border=1 style=font-family:monospace align=center bgcolor=white>"
			"<tr><td> </td><td> Qual </td><td> Obs </td><td> Port </td><td> Call </td></tr>");	

		NRRoute = &Dest->NRROUTE[0];

		Active = Dest->DEST_ROUTE;

		for (i = 1; i < 4; i++)
		{
			Neighbour = NRRoute->ROUT_NEIGHBOUR;

			if (Neighbour)
			{
				len = ConvFromAX25(Neighbour->NEIGHBOUR_CALL, Normcall);
				Normcall[len] = 0;

				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "<tr><td>%c&nbsp;</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
					(Active == i)?'>':' ',NRRoute->ROUT_QUALITY, NRRoute->ROUT_OBSCOUNT, Neighbour->NEIGHBOUR_PORT, Normcall);
			}
			NRRoute++;
		}

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "</table>");

		goto SendResp;

	}
/*

	MOV	ESI,OFFSET32 NODEROUTEHDDR
	MOV	ECX,11
	REP MOVSB

	LEA	ESI,DEST_CALL[EBX]
	CALL	DECODENODENAME		; CONVERT TO ALIAS:CALL
	REP MOVSB

	CMP	DEST_RTT[EBX],0
	JE SHORT @f			; TIMER NOT SET - DEST PROBABLY NOT USED

	MOVSB				; ADD SPACE
	CALL	DORTT
	
@@:
 	MOV	AL,CR
	STOSB

	MOV	ECX,3
	MOV	DH,DEST_ROUTE[EBX]	; CURRENT ACTIVE ROUTE
	MOV	DL,1
	
	push ebx
	
	PUBLIC	CMDN110
CMDN110:

	MOV	ESI,ROUT1_NEIGHBOUR[EBX]
	CMP	ESI,0
	JE CMDN199


	MOV	AX,'  '
	CMP	DH,DL
	JNE SHORT CMDN112			; NOT CURRENT DEST
	MOV	AX,' >'

CMDN112:

	STOSW

	PUSH	ECX

	MOV	AL,ROUT1_QUALITY[EBX]
	CALL	CONV_DIGITS		; CONVERT AL TO DECIMAL DIGITS

	mov	AL,' '
	stosb

	MOV	AL,ROUT1_OBSCOUNT[EBX]
	CALL	CONV_DIGITS		; CONVERT AL TO DECIMAL DIGITS

	mov	AL,' '
	stosb

	MOV	AL,NEIGHBOUR_PORT[ESI]	; GET PORT
	CALL	CONV_DIGITS		; CONVERT AL TO DECIMAL DIGITS

	mov	AL,' '
	stosb


	PUSH	EDI
	CALL	CONVFROMAX25		; CONVERT TO CALL
	POP	EDI

	MOV	ESI,OFFSET32 NORMCALL
	REP MOVSB
	
	MOV	AL,CR
	STOSB

	ADD	EBX,ROUTEVECLEN
	INC	DL			; ROUTE NUMBER

	POP	ECX
	DEC	ECX
	JNZ	CMDN110

	PUBLIC	CMDN199
CMDN199:

	POP	EBX
	
; DISPLAY  INP3 ROUTES

	MOV	ECX,3
	MOV	DL,4

	PUBLIC	CMDNINP3
CMDNINP3:

	MOV	ESI,INPROUT1_NEIGHBOUR[EBX]
	CMP	ESI,0
	JE CMDNINPEND

	MOV	AX,'  '
	CMP	DH,DL
	JNE SHORT @F			; NOT CURRENT DEST
	MOV	AX,' >'

@@:

	STOSW

	PUSH	ECX
	
	MOV	AL, Hops1[EBX]
	CALL	CONV_DIGITS		; CONVERT AL TO DECIMAL DIGITS

	mov	AL,' '
	stosb

	MOVZX	EAX, SRTT1[EBX]
	
	MOV	EDX,0
	MOV ECX, 100
	DIV ECX	
	CALL	CONV_5DIGITS
	MOV	AL,'.'
	STOSB
	MOV EAX, EDX
	CALL	PRINTNUM
	MOV	AL,'s'
	STOSB
	MOV	AL,' '
	STOSB
	
	MOV	AL,NEIGHBOUR_PORT[ESI]	; GET PORT
	CALL	CONV_DIGITS		; CONVERT AL TO DECIMAL DIGITS

	mov	AL,' '
	stosb

	PUSH	EDI
	CALL	CONVFROMAX25		; CONVERT TO CALL
	POP	EDI

	MOV	ESI,OFFSET32 NORMCALL
	REP MOVSB
	

	MOV	AL,CR
	STOSB

	ADD	EBX,INPROUTEVECLEN
	INC	DL			; ROUTE NUMBER

	POP	ECX
	LOOP	CMDNINP3

CMDNINPEND:

	ret
	
*/


	if (_stricmp(NodeURL, "/Node/Routes.html") == 0)
	{
		struct ROUTE * Routes = NEIGHBOURS;
		int MaxRoutes = MAXNEIGHBOURS;
		int count;
		char Normcall[10];
		char locked;
		int NodeCount;
		int Percent = 0;
		int Iframes, Retries;
		char Active[10];
		int Queued;

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", RouteHddr);
		
		for (count=0; count<MaxRoutes; count++)
		{
			if (Routes->NEIGHBOUR_CALL[0] != 0)
			{
				int len = ConvFromAX25(Routes->NEIGHBOUR_CALL, Normcall);
				Normcall[len]=0;

				if ((Routes->NEIGHBOUR_FLAG & 1) == 1)
					locked = '!';
				else
					locked = ' ';

				NodeCount = COUNTNODES(Routes);
				
				if (Routes->NEIGHBOUR_LINK)
					Queued = COUNT_AT_L2(Routes->NEIGHBOUR_LINK);
				else
					Queued = 0;

				Iframes = Routes->NBOUR_IFRAMES;
				Retries = Routes->NBOUR_RETRIES;

				if (Routes->NEIGHBOUR_LINK && Routes->NEIGHBOUR_LINK->L2STATE >= 5)
					strcpy(Active, ">");
				else
					strcpy(Active, "&nbsp;");

				if (Iframes)
					Percent = (Retries * 100) / Iframes;
				else
					Percent = 0;
		
				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], RouteLine, Active, Routes->NEIGHBOUR_PORT, Normcall, locked, 
					Routes->NEIGHBOUR_QUAL,	NodeCount, Iframes, Retries, Percent, Routes->NBOUR_MAXFRAME, Routes->NBOUR_FRACK,
					Routes->NEIGHBOUR_TIME >> 8, Routes->NEIGHBOUR_TIME & 0xff, Queued, Routes->OtherendsRouteQual);
			}
			Routes+=1;
		}
	}

	if (_stricmp(NodeURL, "/Node/Links.html") == 0)
	{
		struct _LINKTABLE * Links = LINKS;
		int MaxLinks = MAXLINKS;
		int count;
		char Normcall1[10];
		char Normcall2[10];
		char State[12] = "", Type[12] = "Uplink";
		int axState;
		int cctType;

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", LinkHddr);
		
		for (count=0; count<MaxLinks; count++)
		{
			if (Links->LINKCALL[0] != 0)
			{
				int len = ConvFromAX25(Links->LINKCALL, Normcall1);
				Normcall1[len] = 0;

				len = ConvFromAX25(Links->OURCALL, Normcall2);
				Normcall2[len] = 0;

				axState = Links->L2STATE;

				if (axState == 2)
					strcpy(State, "Connecting");
				else if (axState == 3)
					strcpy(State, "FRMR");
				else if (axState == 4)
					strcpy(State, "Closing");
				else if (axState == 5)
					strcpy(State, "Active");
				else if (axState == 6)
					strcpy(State, "REJ Sent");

				cctType = Links->LINKTYPE;

				if (cctType == 1)
					strcpy(Type, "Uplink");
				else if (cctType == 2)
					strcpy(Type, "Downlink");
				else if (cctType == 3)
					strcpy(Type, "Node-Node");

				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], LinkLine, Normcall1, Normcall2, Links->LINKPORT->PORTNUMBER, 
					State, Type, 2 - Links->VER1FLAG );
						
			Links+=1;
			}
		}
	}

	if (_stricmp(NodeURL, "/Node/Users.html") == 0)
	{
		TRANSPORTENTRY * L4 = L4TABLE;
		TRANSPORTENTRY * Partner;
		int MaxLinks = MAXLINKS;
		int count;
		char State[12] = "", Type[12] = "Uplink";
		char LHS[50] = "", MID[10] = "", RHS[50] = "";

		ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", UserHddr);
		
		for (count=0; count < MAXCIRCUITS; count++)
		{
			if (L4->L4USER[0])
			{
				RHS[0] = MID[0] = 0;

				if ((L4->L4CIRCUITTYPE & UPLINK) == 0)   //SHORT CMDS10A		; YES
				{
					// IF DOWNLINK, ONLY DISPLAY IF NO CROSSLINK

					if (L4->L4CROSSLINK == 0)	//jne 	CMDS60			; WILL PROCESS FROM OTHER END
					{
						// ITS A DOWNLINK WITH NO PARTNER - MUST BE A CLOSING SESSION
						// DISPLAY TO THE RIGHT FOR NOW

						strcpy(LHS, "(Closing) ");
						DISPLAYCIRCUIT(L4, RHS);
						goto CMDS50;
					}
					else
						goto CMDS60;		// WILL PROCESS FROM OTHER END
				}
			
				if (L4->L4CROSSLINK == 0)
				{
					// Single Entry

					DISPLAYCIRCUIT(L4, LHS);
				}
				else
				{
					DISPLAYCIRCUIT(L4, LHS);

					Partner = L4->L4CROSSLINK;

					if (Partner->L4STATE == 5)
						strcpy(MID, "<-->");
					else
						strcpy(MID, "<~~>");
					
					DISPLAYCIRCUIT(Partner, RHS);
				}
CMDS50:
				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], UserLine, LHS, MID, RHS);
			}
CMDS60:			
		L4++;	
		}
	}
/*
	PUBLIC	CMDUXX_1
CMDUXX_1:
	push	EBX
	push	ESI
	PUSH	ECX
	push	EDI

	call	_FINDDESTINATION
	pop	EDI

	jz SHORT NODE_FOUND

	push	EDI			; NET/ROM not found
	call	CONVFROMAX25		; CONVERT TO CALL
	pop	EDI
	mov	ESI,OFFSET32 NORMCALL
	rep movsb

	jmp	SHORT END_CMDUXX

	PUBLIC	NODE_FOUND
NODE_FOUND:

	lea	ESI,DEST_CALL[EBX]
	call	DECODENODENAME

	REP 	MOVSB

	PUBLIC	END_CMDUXX
END_CMDUXX:

	POP	ECX
	pop	ESI
	pop	EBX
	ret

}}}
*/

	else if (_stricmp(NodeURL, "/Node/Terminal.html") == 0)
	{
		if (COOKIE && Session)
		{
			// Already signed in as sysop
			
			struct UserRec * USER = Session->USER;

			struct HTTPConnectionInfo * NewSession = AllocateSession(sock, 'T');

			if (NewSession)
			{
				char AXCall[10];
				ReplyLen = sprintf(_REPLYBUFFER, TermPage, Mycall, Mycall, NewSession->Key, NewSession->Key, NewSession->Key);
				strcpy(NewSession->HTTPCall, USER->Callsign);
				ConvToAX25(NewSession->HTTPCall, AXCall);
				ChangeSessionCallsign(NewSession->Stream, AXCall);
				BPQHOSTVECTOR[NewSession->Stream -1].HOSTSESSION->Secure_Session = USER->Secure;
				Session->USER = USER;

	//			if (Appl[0])
	//			{
	//				strcat(Appl, "\r");
	//				SendMsg(Session->Stream, Appl, strlen(Appl));
	//			}

			}
			else
			{
				ReplyLen = SetupNodeMenu(_REPLYBUFFER);
				ReplyLen += sprintf(&_REPLYBUFFER[ReplyLen], "%s", BusyError);
			}
		}
		else
			ReplyLen = sprintf(_REPLYBUFFER, TermSignon, Mycall, Mycall, Context);
	}

	else if (_stricmp(NodeURL, "/Node/Signon.html") == 0)
	{
		ReplyLen = sprintf(_REPLYBUFFER, NodeSignon, Mycall, Mycall, Context);
	}

	else if (_stricmp(NodeURL, "/Node/Drivers") == 0)
	{
		int Bufferlen = SendMessageFile(sock, "/Drivers.htm", TRUE);		// return -1 if not found
		
		if (Bufferlen != -1)
			return 0;											// We've sent it
	}
	
	else if (_stricmp(NodeURL, "/Node/OutputScreen.html") == 0)
	{
		struct HTTPConnectionInfo * Session = FindSession(Context);

		if (Session == NULL)
		{
			ReplyLen = sprintf(_REPLYBUFFER, "%s", LostSession);
		}
		else
		{
			Session->sock = sock;				// socket to reply on
			ReplyLen = RefreshTermWindow(Session, _REPLYBUFFER);
			
			if (ReplyLen == 0)		// Nothing new
			{
//				Debugprintf("GET with no data avail - response held");
				Session->ResponseTimer = 1200;		// Delay response for up to a minute
			}
			else
			{
//				Debugprintf("GET - outpur sent, timer was %d, set to zero", Session->ResponseTimer);
				Session->ResponseTimer = 0;
			}
			
			Session->KillTimer = 0;
			return 0;				// Refresh has sent any available output
		}
	}

	else if (_stricmp(NodeURL, "/Node/InputLine.html") == 0)
	{
		ReplyLen = sprintf(_REPLYBUFFER, InputLine, Context, Context);
	}

SendResp:

	FormatTime2(TimeString, time(NULL));

	HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n"
		"Date: %s\r\n\r\n", ReplyLen + strlen(Tail), TimeString);
	sendandcheck(sock, Header, HeaderLen);
	sendandcheck(sock, _REPLYBUFFER, ReplyLen);
	sendandcheck(sock, Tail, strlen(Tail));
	}
	return 0;

#ifdef WIN32
	}
	#include "StdExcept.c"
	}
#endif
}

int ProcessHTTPMessage(struct ConnectionInfo * conn)
{
	// conn is a malloc'ed copy to handle reused connections, so need to free it

	int ret = InnerProcessHTTPMessage(conn);
	free(conn);
	return ret;
}

static char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static char *dat[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};


VOID FormatTime2(char * Time, time_t cTime)
{
	struct tm * TM;
	TM = gmtime(&cTime);

	sprintf(Time, "%s, %02d %s %3d %02d:%02d:%02d GMT", dat[TM->tm_wday], TM->tm_mday, month[TM->tm_mon],
		TM->tm_year + 1900, TM->tm_hour, TM->tm_min, TM->tm_sec);

}

// Sun, 06 Nov 1994 08:49:37 GMT

int StatusProc(char * Buff)
{
	int i;
	char callsign[12] = "";
	char flag[3];
	UINT Mask, MaskCopy;
	int Flags;
	int AppNumber;
	int OneBits;
	int Len = sprintf(Buff, "<html><meta http-equiv=expires content=0><meta http-equiv=refresh content=15>"
	"<head><title>Stream Status</title></head><body>");

	Len += sprintf(&Buff[Len], "<table style=\"text-align: left; font-family: monospace; align=center \" border=1 cellpadding=1 cellspacing=0>");
	Len += sprintf(&Buff[Len], "<tr><th>&nbsp;&nbsp;&nbsp;</th><th>&nbsp;RX&nbsp;&nbsp;</th><th>&nbsp;TX&nbsp;&nbsp;</th>");
	Len += sprintf(&Buff[Len], "<th>&nbsp;MON&nbsp;</th><th>&nbsp;App&nbsp;</th><th>&nbsp;Flg&nbsp;</th>");
	Len += sprintf(&Buff[Len], "<th>Callsign&nbsp;&nbsp;</th><th width=200px>Program</th>");
	Len += sprintf(&Buff[Len], "<th>&nbsp;&nbsp;&nbsp;</th><th>&nbsp;RX&nbsp;&nbsp;</th><th>&nbsp;TX&nbsp;&nbsp;</th>");
	Len += sprintf(&Buff[Len], "<th>&nbsp;MON&nbsp;</th><th>&nbsp;App&nbsp;</th><th>&nbsp;Flg&nbsp;</th>");
	Len += sprintf(&Buff[Len], "<th>Callsign&nbsp;&nbsp;</th><th width=200px>Program</th></tr><tr>");
		
	for (i=1;i<65; i++)
	{		
		callsign[0]=0;
		
		if (GetAllocationState(i))

			strcpy(flag,"*");
		else
			strcpy(flag," ");

		GetCallsign(i,callsign);

		Mask = MaskCopy = Get_APPLMASK(i);

		// if only one bit set, convert to number

		AppNumber = 0;
		OneBits = 0;

		while (MaskCopy)
		{
			if (MaskCopy & 1)
				OneBits++;
			
			AppNumber++;
			MaskCopy = MaskCopy >> 1;
		}

		Flags=GetApplFlags(i);

		if (OneBits > 1)
			Len += sprintf(&Buff[Len], "<td>%d%s</td><td>%d</td><td>%d</td><td>%d</td><td>%x</td>"
			"<td>%x</td><td>%s</td><td>%s</td>", 
				i, flag, RXCount(i), TXCount(i), MONCount(i), Mask, Flags, callsign, BPQHOSTVECTOR[i-1].PgmName);

		else
			Len += sprintf(&Buff[Len], "<td>%d%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td>"
			"<td>%x</td><td>%s</td><td>%s</td>", 
				i, flag, RXCount(i), TXCount(i), MONCount(i), AppNumber, Flags, callsign, BPQHOSTVECTOR[i-1].PgmName);

		if ((i & 1) == 0)
			Len += sprintf(&Buff[Len], "</tr><tr>");

	}

	Len += sprintf(&Buff[Len], "</tr></table>");
	return Len;
}

int ProcessNodeSignon(SOCKET sock, struct TCPINFO * TCP, char * MsgPtr, char * Appl, char * Reply, struct HTTPConnectionInfo ** Session)
{
	int ReplyLen = 0;
	char * input = strstr(MsgPtr, "\r\n\r\n");	// End of headers
	char * user, * password, * Key;
	char Header[256];
	int HeaderLen;
	struct HTTPConnectionInfo *Sess;


	if (input)
	{
		int i;
		struct UserRec * USER;

		if (strstr(input, "Cancel=Cancel"))
		{
			ReplyLen = SetupNodeMenu(Reply);
			return ReplyLen;
		}
		user = strtok_s(&input[9], "&", &Key);
		password = strtok_s(NULL, "=", &Key);
		password = Key;

		for (i = 0; i < TCP->NumberofUsers; i++)
		{
			USER = TCP->UserRecPtr[i];

			if (user && _stricmp(user, USER->UserName) == 0)
			{
 				if (strcmp(password, USER->Password) == 0 && USER->Secure)
				{
					// ok

					Sess = *Session = AllocateSession(sock, 'N');
					Sess->USER = USER;

					ReplyLen =  SetupNodeMenu(Reply);

					HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n"
						"Set-Cookie: BPQSessionCookie=%s; Path = /\r\n\r\n", ReplyLen + strlen(Tail), Sess->Key);	
					send(sock, Header, HeaderLen, 0);
					send(sock, Reply, ReplyLen, 0);
					send(sock, Tail, strlen(Tail), 0);

					return ReplyLen;
				}
			}
		}
	}	
	
	ReplyLen = sprintf(Reply, NodeSignon, Mycall, Mycall);
	ReplyLen += sprintf(&Reply[ReplyLen], "%s", PassError);

	HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", ReplyLen + strlen(Tail));	
	send(sock, Header, HeaderLen, 0);
	send(sock, Reply, ReplyLen, 0);
	send(sock, Tail, strlen(Tail), 0);

		return 0;


	return ReplyLen;
}




int ProcessMailSignon(struct TCPINFO * TCP, char * MsgPtr, char * Appl, char * Reply, struct HTTPConnectionInfo ** Session, BOOL WebMail)
{
	int ReplyLen = 0;
	char * input = strstr(MsgPtr, "\r\n\r\n");	// End of headers
	char * user, * password, * Key;
	struct HTTPConnectionInfo * NewSession;

	if (input)
	{
		int i;
		struct UserRec * USER;

		if (strstr(input, "Cancel=Cancel"))
		{
			ReplyLen = SetupNodeMenu(Reply);
			return ReplyLen;
		}
		user = strtok_s(&input[9], "&", &Key);
		password = strtok_s(NULL, "=", &Key);
		password = Key;

		for (i = 0; i < TCP->NumberofUsers; i++)
		{
			USER = TCP->UserRecPtr[i];

			if (user && _stricmp(user, USER->UserName) == 0)
			{
 				if (strcmp(password, USER->Password) == 0 && (USER->Secure || WebMail))
				{
					// ok

					NewSession = AllocateSession(Appl[0], 'M');
				
					*Session = NewSession;
				
					if (NewSession)
					{
						
						ReplyLen = 0;
						strcpy(NewSession->Callsign, USER->Callsign);
					}
					else
					{
						ReplyLen =  SetupNodeMenu(Reply);
						ReplyLen += sprintf(&Reply[ReplyLen], "%s", BusyError);
					}
					return ReplyLen;
				}
			}
		}
	}	
	
	ReplyLen = sprintf(Reply, MailSignon, Mycall, Mycall);
	ReplyLen += sprintf(&Reply[ReplyLen], "%s", PassError);

	return ReplyLen;
}


int ProcessChatSignon(struct TCPINFO * TCP, char * MsgPtr, char * Appl, char * Reply, struct HTTPConnectionInfo ** Session)
{
	int ReplyLen = 0;
	char * input = strstr(MsgPtr, "\r\n\r\n");	// End of headers
	char * user, * password, * Key;

	if (input)
	{
		int i;
		struct UserRec * USER;

		if (strstr(input, "Cancel=Cancel"))
		{
			ReplyLen = SetupNodeMenu(Reply);
			return ReplyLen;
		}

		user = strtok_s(&input[9], "&", &Key);
		password = strtok_s(NULL, "=", &Key);
		password = Key;


		for (i = 0; i < TCP->NumberofUsers; i++)
		{
			USER = TCP->UserRecPtr[i];

			if (user && _stricmp(user, USER->UserName) == 0)
			{
 				if (strcmp(password, USER->Password) == 0 && USER->Secure)
				{
					// ok

					*Session = AllocateSession(Appl[0], 'C');

					if (Session)
					{
						ReplyLen = 0;
					}
					else
					{
						ReplyLen = SetupNodeMenu(Reply);
						ReplyLen += sprintf(&Reply[ReplyLen], "%s", BusyError);
					}
					return ReplyLen;
				}
			}
		}
	}	
	
	ReplyLen = sprintf(Reply, ChatSignon, Mycall, Mycall);
	ReplyLen += sprintf(&Reply[ReplyLen], "%s", PassError);
		
	return ReplyLen;

}
