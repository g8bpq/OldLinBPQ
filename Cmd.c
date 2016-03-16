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
//	C replacement for cmd.asm
//
#define Kernel

#define _CRT_SECURE_NO_DEPRECATE 
#define _USE_32BIT_TIME_T

#pragma data_seg("_BPQDATA")

//#include "windows.h"
//#include "winerror.h"


#include "time.h"
#include "stdio.h"
#include <fcntl.h>					 
//#include "vmm.h"
//#include "SHELLAPI.H"

#include "CHeaders.h"

#pragma pack()

#include "tncinfo.h"
#include "telnetserver.h"

//#include "GetVersion.h"

//#define DllImport	__declspec( dllimport )
//#define DllExport	__declspec( dllexport )

BOOL DecodeCallString(char * Calls, BOOL * Stay, BOOL * Spy, UCHAR *AXCalls);
VOID Send_AX_Datagram(PDIGIMESSAGE Block, DWORD Len, UCHAR Port);
int APIENTRY ClearNodes();
VOID GetJSONValue(char * _REPLYBUFFER, char * Name, char * Value);
VOID SendHTTPRequest(SOCKET sock, char * Host, int Port, char * Request, char * Params, int Len, char * Return);
SOCKET OpenWL2KHTTPSock();
VOID FormatTime2(char * Time, time_t cTime);
VOID Format_Addr(unsigned char * Addr, char * Output, BOOL IPV6);
VOID Tel_Format_Addr(struct ConnectionInfo * sockptr, char * dst);

char COMMANDBUFFER[81] = "";		// Command Hander input buffer
char OrigCmdBuffer[81] = "";		// Command Hander input buffer

struct DATAMESSAGE * REPLYBUFFER = NULL;
UINT APPLMASK = 0;
UCHAR SAVEDAPPLFLAGS = 0;

UCHAR ALIASINVOKED = 0;


VOID * CMDPTR = 0;

short CMDPACLEN = 0;

char OKMSG[] = "Ok\r";
 
char CMDERRMSG[] = "Invalid command - Enter ? for command list\r";
#define CMDERRLEN sizeof(CMDERRMSG) - 1

char PASSWORDMSG[] = "Command requires SYSOP status - enter password\r";
#define LPASSMSG sizeof(PASSWORDMSG) - 1

char CMDLIST[] = "CONNECT BYE INFO NODES PORTS ROUTES USERS MHEARD";

#define CMDLISTLEN	sizeof(CMDLIST) - 1

char BADMSG[] = "Bad Parameter\r";
char BADPORT[] = "Invalid Port Number\r";
char NOTEXTPORT[] = "Only valid on EXT ports\r";
char NOVALCALLS[] = "No Valid Calls defined on this port\r";

char BADVALUEMSG[] = "Invalid parameter\r";

char BADCONFIGMSG[] = "Configuration File check falled - will continue with old config\r";
char REBOOTOK[] = "Rebooting in 20 secs\r";
char REBOOTFAILED[] = "Shutdown failed\r";

char RESTARTOK[] = "Restarting\r";
char RESTARTFAILED[] = "Restart failed\r";


int STATSTIME = 0;
int MAXBUFFS = 0;
int QCOUNT = 0;
int MINBUFFCOUNT = 65535;
int NOBUFFCOUNT = 0;
int BUFFERWAITS = 0;
int MAXDESTS = 0;
int NUMBEROFNODES = 0;
int L4CONNECTSOUT = 0;
int L4CONNECTSIN = 0;
int L4FRAMESTX = 0;
int L4FRAMESRX = 0;
int L4FRAMESRETRIED = 0;
int OLDFRAMES = 0;
int L3FRAMES = 0;

VOID SENDSABM();
VOID RESET2();

int APPL1 = 0;
int PASSCMD = 0;

#pragma pack (1)

struct _EXTPORTDATA DP;			// Only way I can think of to get offets to port data into cmd table

char CMDALIAS[ALIASLEN][NumberofAppls] = {0};
char * ALIASPTR	= &CMDALIAS[0][0];



CMDX COMMANDS[];

int CMDXLEN	= sizeof (CMDX);

VOID SENDNODESMSG();
VOID STOPPORT(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID STARTPORT(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID FINDBUFFS(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID WL2KSYSOP(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID AXRESOLVER(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID AXMHEARD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID SHOWTELNET(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID SHOWAGW(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID SHOWARP(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID SHOWNAT(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID PING(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID SHOWIPROUTE(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);
VOID FLMSG(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * UserCMD);
void ListExcludedCalls(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);

VOID SENDNODES(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	SENDNODESMSG();

	strcpy(Bufferptr, OKMSG);
	Bufferptr += strlen(OKMSG);
						
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID SAVENODES(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	SaveNodes();
							
	strcpy(Bufferptr, OKMSG);
	Bufferptr += strlen(OKMSG);
						
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID RECONFIG(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	if (Reconfig())
	{
		strcpy(Bufferptr, OKMSG);
		Bufferptr += strlen(OKMSG);
	}
	else
	{
		strcpy(Bufferptr, BADCONFIGMSG);
		Bufferptr += strlen(BADCONFIGMSG);
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID REBOOT(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	if (Reboot())
	{
		strcpy(Bufferptr, REBOOTOK);
		Bufferptr += strlen(REBOOTOK);
	}
	else
	{
		strcpy(Bufferptr, REBOOTFAILED);
		Bufferptr += strlen(REBOOTFAILED);
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}
	
VOID RESTART(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	if (Restart())
	{
		strcpy(Bufferptr, RESTARTOK);
		Bufferptr += strlen(RESTARTOK);
	}
	else
	{
		strcpy(Bufferptr, RESTARTFAILED);
		Bufferptr += strlen(RESTARTFAILED);
	}
							
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

UCHAR VALNODESFLAG = 0, EXTONLY = 0;

VOID PORTVAL (TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD);

VOID VALNODES(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	VALNODESFLAG = 1;
	PORTVAL(Session, Bufferptr, CmdTail, CMD);
}

VOID EXTPORTVAL(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	EXTONLY = 1;
	PORTVAL(Session, Bufferptr, CmdTail, CMD);
}
VOID PORTVAL(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	PROCESS PORT VALUE COMMANDS

	char * ptr, *Context, * ptr1;
	int portno;
	UCHAR oldvalue, newvalue;
	struct PORTCONTROL * PORT = PORTTABLE;
	int n = NUMBEROFPORTS;
	UINT offset, DPBASE;
	UCHAR * valueptr;

	// Get port number

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
	{
		portno = atoi (ptr);

		if (portno)
		{
			while (n--)
			{
				if (PORT->PORTNUMBER == portno)
				{
					if (VALNODESFLAG)
					{
						char * VNPtr = PORT->PERMITTEDCALLS;
						char Normcall[10];
						int len;
						
						VALNODESFLAG = 0;

						if (VNPtr)
						{
							while (VNPtr[0])
							{
								Bufferptr = CHECKBUFFER(Session, Bufferptr);	// ENSURE ROOM
								len = ConvFromAX25(VNPtr, Normcall);
								len++;									// Add a space
								memcpy(Bufferptr, Normcall, len);
								Bufferptr += len;

								VNPtr += 7;
							}

							*(Bufferptr++) = 13;
						}
						else
						{								
							strcpy(Bufferptr, NOVALCALLS);
							Bufferptr += strlen(NOVALCALLS);
						}
						
						SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
						
						return;

					}

					if (EXTONLY)
					{
						// Make sure an Extenal Port

						EXTONLY = 0;
						
						if (PORT->PORTTYPE != 0x10)
						{
							strcpy(Bufferptr, NOTEXTPORT);
							Bufferptr += strlen(NOTEXTPORT);
							SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
							return;
						}
					}

					valueptr = (UCHAR *)PORT;
					offset = (UINT)CMD->CMDFLAG;
					DPBASE = (UINT)&DP;
					offset -= DPBASE;
					valueptr += offset;

					oldvalue = (UCHAR)*valueptr;

					// Display Param Namee

					ptr1 = &CMD->String[0];
					n = 12;

					while (*(ptr1) != ' ' && n--)
						*(Bufferptr++) = *(ptr1++);

					// See if another param - if not, just display current value

					ptr = strtok_s(NULL, " ", &Context);

					if (ptr && ptr[0])
					{
						// Get new value

							newvalue = atoi(ptr);
							*valueptr = newvalue;

							Bufferptr += sprintf(Bufferptr, " was %d now %d\r", oldvalue, newvalue);
					}

					else
						Bufferptr += sprintf(Bufferptr, " %d\r", oldvalue);

					SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
					return;

				}
				PORT = PORT->PORTPOINTER;
			}
		}
	}
	
	// Bad port

	strcpy(Bufferptr, BADPORT);
	Bufferptr += strlen(BADPORT);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;

}

VOID SWITCHVAL (TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	// Update switch 8 bit value
	
	char * ptr, *Context, * ptr1;
	UCHAR oldvalue, newvalue;
	int n;
	UCHAR * valueptr;

	valueptr = CMD->CMDFLAG;

	oldvalue = (UCHAR)*valueptr;

	// Display Param Name

	ptr1 = &CMD->String[0];
	n = 12;

	while (*(ptr1) != ' ' && n--)
		*(Bufferptr++) = *(ptr1++);

	// See if a param - if not, just display current value

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr && ptr[0])
	{
		// Get new value

			newvalue = atoi(ptr);
			*valueptr = newvalue;

			Bufferptr += sprintf(Bufferptr, " was %d now %d\r", oldvalue, newvalue);
	}
	else
		Bufferptr += sprintf(Bufferptr, " %d\r", oldvalue);

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;

}

VOID SWITCHVALW (TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	// Update switch 16 bit value
	
	char * ptr, *Context, * ptr1;
	USHORT oldvalue, newvalue;
	int n;
	USHORT * valueptr;

	valueptr = CMD->CMDFLAG;

	oldvalue = (USHORT)*valueptr;

	// Display Param Name

	ptr1 = &CMD->String[0];
	n = 12;

	while (*(ptr1) != ' ' && n--)
		*(Bufferptr++) = *(ptr1++);

	// See if a param - if not, just display current value

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr && ptr[0])
	{
		// Get new value

			newvalue = atoi(ptr);
			*valueptr = newvalue;

			Bufferptr += sprintf(Bufferptr, " was %d now %d\r", oldvalue, newvalue);
	}
	else
		Bufferptr += sprintf(Bufferptr, " %d\r", oldvalue);

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;

}

TRANSPORTENTRY * SetupSessionFromSession(TRANSPORTENTRY * Session, PBPQVECSTRUC HOSTSESS, UINT APPLMASK)
{
	// Create a Transport (L4) session linked to an incoming Session

	TRANSPORTENTRY * NewSess = L4TABLE;
	int Index = 0;
	
	while (Index < MAXCIRCUITS)
	{
		if (NewSess->L4USER[0] == 0)
		{
			// Got One

			UCHAR * ourcall = &MYCALL[0];

			Session->L4CROSSLINK = NewSess;
			NewSess->L4CROSSLINK = Session;

			if (APPLMASK)
			{
				// Circuit for APPL - look for an APPLCALL

				APPLCALLS * APPL = APPLCALLTABLE;

				while ((APPLMASK & 1) == 0)
				{
					APPLMASK >>= 1;
					APPL++;
				}
				if (APPL->APPLCALL[0] > 0x40)		// We have an applcall
					ourcall = &APPL->APPLCALL[0];
			}

			memcpy(NewSess->L4USER, ourcall, 7);
			memcpy(NewSess->L4MYCALL, Session->L4MYCALL, 7);
	
			NewSess->CIRCUITINDEX = Index;				//OUR INDEX
			NewSess->CIRCUITID = NEXTID;

			NEXTID++;
			if (NEXTID == 0)
				NEXTID++;								// kEEP nON-ZERO

			NewSess->SESSIONT1 = Session->SESSIONT1;
			NewSess->L4WINDOW = (UCHAR)L4DEFAULTWINDOW;
			NewSess->SESSPACLEN = PACLEN;				// Default;

			NewSess->L4TARGET.HOST = HOSTSESS;
			NewSess->L4STATE = 5;
			return NewSess;
		}
		Index++;
		NewSess++;
	}
	return NULL;
}

extern GETCONNECTIONINFO();


BOOL cATTACHTOBBS(TRANSPORTENTRY * Session, UINT Mask, int Paclen, int * AnySessions)
{
	PBPQVECSTRUC HOSTSESS = BPQHOSTVECTOR;
	TRANSPORTENTRY * NewSess;
	int ApplNum;
	int n = BPQHOSTSTREAMS;
	int ConfigedPorts = 0;

	//	LOOK FOR A FREE HOST SESSION

	while (n--)
	{
		if (HOSTSESS->HOSTAPPLMASK & Mask)
		{
			// Right appl

			ConfigedPorts++;

			if (HOSTSESS->HOSTSESSION == NULL && (HOSTSESS->HOSTFLAGS & 3) == 0) // Not attached and no report outstanding
			{				
				//	WEVE GOT A FREE BPQ HOST PORT - USE IT

				NewSess = SetupSessionFromSession(Session, HOSTSESS, Mask);

				if (NewSess == NULL)
					return FALSE;					// Appl not available

				HOSTSESS->HOSTSESSION = NewSess;

				//	Convert APPLMASK to APPLNUM

				ApplNum = 1;

				while  (APPLMASK && (APPLMASK & 1) == 0)
				{
					ApplNum++;
					APPLMASK >>= 1;
				}

				HOSTSESS->HOSTAPPLNUM = ApplNum;
	
				HOSTSESS->HOSTFLAGS |= 2;			// Indicate State Change

				NewSess->L4CIRCUITTYPE = BPQHOST | DOWNLINK;

				PostStateChange(NewSess);

				NewSess->SESS_APPLFLAGS = HOSTSESS->HOSTAPPLFLAGS;

				NewSess->SESSPACLEN = Paclen;

				return TRUE;
			}
		}
		HOSTSESS++;
	}

	*AnySessions = ConfigedPorts;		// to distinguish between none and all in use
	return FALSE;
}

VOID APPLCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{		
	BOOL CONFAILED = 0;
	UINT CONERROR ;
	char APPName[13];
	char * ptr1, *ptr2;
	int n = 12;
	BOOL Stay = FALSE;

	//	Copy Appl and Null Terminate

	ptr1 = &CMD->String[0];
	ptr2 = APPName;

	while (*(ptr1) != ' ' && n--)
		*(ptr2++) = *(ptr1++);

	*(ptr2) = 0;
	
	if (Session->LISTEN)
	{
		Bufferptr += sprintf(Bufferptr, "Can't use %s while listening\r", APPName);
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}


	if (CmdTail[0] == 'S')
		Stay = TRUE;
	
	Session->STAYFLAG = Stay;

	memcpy(Session->APPL, CMD->String, 12);

	//	SEE IF THERE IS AN ALIAS DEFINDED FOR THIS COMMAND

	if (ALIASPTR[0] > ' ')
	{
		//	COPY ALIAS TO COMMAND BUFFER, THEN REENTER COMMAND HANDLER

		memcpy(COMMANDBUFFER, ALIASPTR, ALIASLEN);
		_strupr(COMMANDBUFFER);
		memcpy(OrigCmdBuffer, ALIASPTR, ALIASLEN);	// In case original case version needed
		
		ALIASINVOKED = 1;							// To prevent Alias Loops 	

		DoTheCommand(Session);
	
		return;
	}

	if (cATTACHTOBBS(Session, APPLMASK, CMDPACLEN, &CONERROR) == 0)
	{
		// No Streams

		if (CONERROR)
			Bufferptr += sprintf(Bufferptr, "Sorry, All %s Ports are in use - Please try later\r", APPName);
		else
			Bufferptr += sprintf(Bufferptr, "Sorry, Application %s is not running - Please try later\r", APPName);

		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	//	IF CMD_TO_APPL SET IN APPLFLAGS, SEND INPUT MSG TO APPL

	if (Session->L4CROSSLINK->SESS_APPLFLAGS & CMD_TO_APPL)
	{
		struct DATAMESSAGE * Msg = (struct DATAMESSAGE *)GetBuff();
		TRANSPORTENTRY * XSession = Session->L4CROSSLINK;

		if (Msg)
		{
			COMMANDBUFFER[72] = 13;
			memcpy(Msg->L2DATA, COMMANDBUFFER, 73);
			Msg->LENGTH = 81;
			Msg->PID = 0xf0;

			C_Q_ADD(&XSession->L4TX_Q, (UINT *)Msg);
			PostDataAvailable(XSession);
		}
	}

	if (Stay)
		Session->L4CROSSLINK->L4TARGET.HOST->HOSTFLAGS |= 0x20;

	//	IF MSG_TO_USER SET, SEND 'CONNECTED' MESSAGE TO USER

	Session->SESS_APPLFLAGS = Session->L4CROSSLINK->SESS_APPLFLAGS;
	
	if (Session->L4CROSSLINK->SESS_APPLFLAGS  & MSG_TO_USER)
	{
		Bufferptr += sprintf(Bufferptr, "Connected to %s\r", APPName);
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}


	//	DONT NEED BUFFER ANY MORE

	ReleaseBuffer((UINT *)REPLYBUFFER);
	return;
}


VOID CMDI00(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	Bufferptr = MOVEANDCHECK(Session, Bufferptr, INFOMSG, INFOLEN);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}
	
VOID CMDV00(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	Bufferptr += sprintf(Bufferptr, "Version %s\r", VersionString);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID BYECMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	CLOSECURRENTSESSION(Session);		// Kills any crosslink, plus local link
	ReleaseBuffer((UINT *)REPLYBUFFER);
	return;
}

VOID CMDPAC(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	SET PACLEN FOR THIS SESSION

	char * ptr, *Context;
	int newvalue;

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr && ptr[0])
	{
		// Get new value

		newvalue = atoi(ptr);
		if (newvalue > 29 && newvalue < 256)
			Session->SESSPACLEN = newvalue & 0xff;
	}

	Bufferptr += sprintf(Bufferptr, "PACLEN - %d\r", Session->SESSPACLEN);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID CMDIDLE(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	SET IDLETIME FOR THIS SESSION

	char * ptr, *Context;
	int newvalue;

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr && ptr[0])
	{
		// Get new value

		newvalue = atoi(ptr);
		if (newvalue > 59 && newvalue < 901)
			Session->L4LIMIT = newvalue;
	}

	Bufferptr += sprintf(Bufferptr, "IDLETIME - %d\r", Session->L4LIMIT);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);

}
VOID CMDT00(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	SET L4 TIMEOUT FOR CONNECTS ON THIS SESSION

	char * ptr, *Context;
	int newvalue;

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr && ptr[0])
	{
		// Get new value

		newvalue = atoi(ptr);
		if (newvalue > 20)
			Session->SESSIONT1 = newvalue;
	}

	Bufferptr += sprintf(Bufferptr, "L4TIMEOUT - %d\r", Session->SESSIONT1);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

UCHAR PWLEN;
char PWTEXT[80];

VOID PWDCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	char * ptr, *Context;
	USHORT pwsum = 0;
	int n = 5, p1, p2, p3, p4, p5;

	if (Session->Secure_Session)	// HOST - SET AUTHORISED REGARDLESS
	{
		Session->PASSWORD = 0xFFFF;		// SET AUTHORISED
		Session->Secure_Session = 1;
		strcpy(Bufferptr, OKMSG);
		Bufferptr += strlen(OKMSG);				
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr && ptr[0])
	{
		// Check Password

		n = 5;

		while (n--)
			pwsum += *(ptr++);

		if (Session->PASSWORD == pwsum)
		{
			Session->PASSWORD = 0xFFFF;		// SET AUTHORISED
			Session->Secure_Session = 1;
			strcpy(Bufferptr, OKMSG);
			Bufferptr += strlen(OKMSG);				
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}

		ReleaseBuffer((UINT *)REPLYBUFFER);
		return;
	}

	//	SEND PASSWORD PROMPT

	if (PWLEN == 0)
		PWLEN = 1;

	p1 = rand() % PWLEN;
	pwsum += PWTEXT[p1++];

	p2 = rand() % PWLEN;
	pwsum += PWTEXT[p2++];

	p3 = rand() % PWLEN;
	pwsum += PWTEXT[p3++];

	p4 = rand() % PWLEN;
	pwsum += PWTEXT[p4++];

	p5 = rand() % PWLEN;
	pwsum += PWTEXT[p5++];

	Session->PASSWORD = pwsum;

	Bufferptr += sprintf(Bufferptr, "%d %d %d %d %d\r", p1, p2, p3, p4, p5);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;
}

VOID CMDSTATS(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	char * ptr, *Context;
	int Port = 0, cols = NUMBEROFPORTS, i;
	char * uptime;
	struct PORTCONTROL * PORT = PORTTABLE;
	struct PORTCONTROL * STARTPORT;

	*(Bufferptr++) = 13;

	//	SEE IF ANY PARAM

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr && ptr[0])
		Port = atoi(ptr);

	//	IF ASKING FOR PORT STATS, DONT DO SYSTEM ONES

	if (Port == 0)
	{
		uptime = FormatUptime(STATSTIME);
		Bufferptr += sprintf(Bufferptr, "%s", uptime);

		Bufferptr += sprintf(Bufferptr, "Semaphore Get-Rel/Clashes   %9d%9d\r", 
					Semaphore.Gets - Semaphore.Rels, Semaphore.Clashes);

		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM

		Bufferptr += sprintf(Bufferptr, "Buffers:Max/Cur/Min/Out/Wait%9d%9d%9d%9d%9d\r", 
					MAXBUFFS, QCOUNT, MINBUFFCOUNT, NOBUFFCOUNT, BUFFERWAITS);

		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM

		Bufferptr += sprintf(Bufferptr, "Known Nodes/Max Nodes       %9d%9d\r", 
					NUMBEROFNODES, MAXDESTS);

		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM

		Bufferptr += sprintf(Bufferptr, "L4 Connects Sent/Rxed       %9d%9d\r", 
					L4CONNECTSOUT, L4CONNECTSIN);

		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM				

		Bufferptr += sprintf(Bufferptr, "L4 Frames TX/RX/Resent/Reseq%9d%9d%9d%9d\r", 
					L4FRAMESTX, L4FRAMESRX, L4FRAMESRETRIED, OLDFRAMES);

		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM

		Bufferptr += sprintf(Bufferptr, "L3 Frames Relayed           %9d\r", L3FRAMES);

		if (ptr && ptr[0] == 'S')
		{
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}
	}

	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM

	//	POSITION TO REQUESTED PORT

	if (Port)
	{
		while (PORT && PORT->PORTNUMBER != Port)
		{
			PORT = PORT->PORTPOINTER;
			cols--;
		}
	}

	if (PORT == NULL)			//	REQUESTED PORT NOT FOUND
	{
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM

	STARTPORT = PORT;

	if (cols > 7)
		cols = 7;

	Bufferptr += sprintf(Bufferptr, "                  ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "Port %02d  ", PORT->PORTNUMBER);
		PORT = PORT->PORTPOINTER;	
	}

	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "L2 Frames Digied");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2DIGIED);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "L2 Frames Heard ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2FRAMES);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "L2 Frames Rxed  ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2FRAMESFORUS);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "L2 Frames Sent  ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2FRAMESSENT);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "L2 Timeouts     ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2TIMEOUTS);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "REJ Frames Rxed ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2REJCOUNT);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "RX out of Seq   ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2OUTOFSEQ);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "L2 Resequenced  ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2RESEQ);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "Undrun/Poll T/o ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2URUNC);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "RX Overruns     ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2ORUNC);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;
	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "RX CRC Errors   ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->RXERRORS);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "FRMRs Sent      ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2FRMRTX);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "FRMRs Received  ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L2FRMRRX);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "Frames abandoned");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "%9d", PORT->L1DISCARD);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	PORT = STARTPORT;
	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
	Bufferptr += sprintf(Bufferptr, "Link Active %%   ");

	for (i = 0; i < cols; i++)
	{
		Bufferptr += sprintf(Bufferptr, "   %2d %3d", PORT->AVSENDING, PORT->AVACTIVE);
		PORT = PORT->PORTPOINTER;	
	}
	*(Bufferptr++) = 13;

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID CMDL00(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	PROCESS 'LINKS' MESSAGE

	struct _LINKTABLE * LINK = LINKS;
	int n = MAXLINKS;
	int len;
	char Normcall[10];

	Bufferptr += sprintf(Bufferptr, "Links\r");

	while (n--)
	{
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	

		if (LINK->LINKCALL[0])
		{
			len = ConvFromAX25(LINK->LINKCALL, Normcall);
			len++;			// add a apace
			memcpy(Bufferptr, Normcall, len);
			Bufferptr += len;

			len = ConvFromAX25(LINK->OURCALL, Normcall);
			memcpy(Bufferptr, Normcall, len);
			Bufferptr += len;

			Bufferptr += sprintf(Bufferptr, " S=%d P=%d T=%d V=%d\r",
				LINK->L2STATE, LINK->LINKPORT->PORTNUMBER, LINK->LINKTYPE, 2 - LINK->VER1FLAG);
		}
		LINK++;
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}


VOID CMDS00(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	PROCESS 'USERS'

	int n = MAXCIRCUITS;
	TRANSPORTENTRY * L4 = L4TABLE;
	TRANSPORTENTRY * Partner;
	int MaxLinks = MAXLINKS;
	char State[12] = "", Type[12] = "Uplink";
	char LHS[50] = "", MID[10] = "", RHS[50] = "";
	char Line[100];
	
	Bufferptr += sprintf(Bufferptr, "%s%d)\r", SESSIONHDDR, QCOUNT);

	while (n--)
	{
		if (L4->L4USER[0])
		{
			Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	

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
			memset(Line, 32, 100);
			memcpy(Line, LHS, strlen(LHS));
			memcpy(&Line[35], MID, strlen(MID));
			strcpy(&Line[40], RHS);
			strcat(&Line[40], "\r");
			Bufferptr = MOVEANDCHECK(Session, Bufferptr, Line, strlen(Line));
		}
CMDS60:			
		L4++;
	}
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID CMDP00(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	// Porocess PORTS Message

	struct PORTCONTROL * PORT = PORTTABLE;

	Bufferptr += sprintf(Bufferptr, "Ports\r");

	while (PORT)
	{
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	
		Bufferptr += sprintf(Bufferptr, " %2d %s\r", PORT->PORTNUMBER, PORT->PORTDESCRIPTION);

		PORT = PORT->PORTPOINTER;
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

char *  DisplayRoute(TRANSPORTENTRY * Session, char * Bufferptr, struct ROUTE * Routes, char Verbose)
{
	char Normcall[10];
	char locked[] = " ! ";
	int NodeCount;
	int Percent = 0;
	char PercentString[20];
	int Iframes, Retries;
	char Active[10];
	int Queued;

	int Port = 0;

	int len = ConvFromAX25(Routes->NEIGHBOUR_CALL, Normcall);

	Normcall[9]=0;

	if ((Routes->NEIGHBOUR_FLAG & 1) == 1)
		strcpy(locked, "!");
	else
		strcpy(locked, " ");

	NodeCount = COUNTNODES(Routes);
				
	if (Routes->NEIGHBOUR_LINK && Routes->NEIGHBOUR_LINK->L2STATE >= 5)
		strcpy(Active, ">");
	else
		strcpy(Active, " ");

	Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	

	if (Verbose)
	{
		if (Routes->NEIGHBOUR_LINK)					
			Queued = COUNT_AT_L2(Routes->NEIGHBOUR_LINK);			// SEE HOW MANY QUEUED
		else
			Queued = 0;

		Iframes = Routes->NBOUR_IFRAMES;
		Retries = Routes->NBOUR_RETRIES;

		if (Iframes)
		{
			Percent = (Retries * 100) / Iframes;
			sprintf(PercentString, "%3d%%", Percent);
		}
		else
			strcpy(PercentString, "    ");

		
		Bufferptr += sprintf(Bufferptr, "%s%2d %s %3d %3d%s%4d %4d %s %d %d %02d:%02d  %d %d",
							Active, Routes->NEIGHBOUR_PORT, Normcall, 
							Routes->NEIGHBOUR_QUAL,	NodeCount, locked, Iframes, Retries, PercentString, Routes->NBOUR_MAXFRAME, Routes->NBOUR_FRACK,
							Routes->NEIGHBOUR_TIME >> 8, (Routes->NEIGHBOUR_TIME) & 0xff, Queued, Routes->OtherendsRouteQual);

		//	IF INP3 DISPLAY SRTT

		if (Routes->INP3Node)		// INP3 Enabled?
		{
			double srtt = Routes->SRTT/1000.0;
			double nsrtt = Routes->NeighbourSRTT/1000.0;

			Bufferptr += sprintf(Bufferptr, " %4.2fs %4.2fs", srtt, nsrtt);
		}

		*(Bufferptr++) = 13;
	}
	else
	{
		Bufferptr += sprintf(Bufferptr, "%s %d %s %d %d%s\r",
			Active, Routes->NEIGHBOUR_PORT, Normcall, Routes->NEIGHBOUR_QUAL, NodeCount, locked);
	}

	return Bufferptr;
}


VOID CMDR00(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	struct ROUTE * Routes = NEIGHBOURS;
	int MaxRoutes = MAXNEIGHBOURS;
	char locked[] = " ! ";
	int Percent = 0;
	char * ptr, * Context;
	char Verbose = 0;
	int Port = 0;
	char AXCALL[7];
	BOOL Found;

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr && strlen(ptr) > 1)
	{
		// Route Update

		goto ROUTEUPDATE;
	}

	if (ptr)
	{
		Verbose = ptr[0];
		ptr = strtok_s(NULL, " ", &Context);
		if (ptr)
			Port = atoi(ptr);
	}

	Bufferptr += sprintf(Bufferptr, "Routes\r");
		
	while (MaxRoutes--)
	{
		if (Routes->NEIGHBOUR_CALL[0] != 0)
			if (Port == 0 || Port == Routes->NEIGHBOUR_PORT)
				Bufferptr = DisplayRoute(Session, Bufferptr, Routes, Verbose);

		Routes++;
	}
	goto SendReply;

ROUTEUPDATE:
	
	if (Session->PASSWORD  != 0xFFFF)
	{
		Bufferptr += sprintf(Bufferptr, "%s", PASSWORDMSG);
		goto SendReply;
	}

	// Line is 

	// ROUTES G8BPQ-2 2 100	  - Set quality to 100
    // ROUTES G8BPQ-2 2 !         - Toggle 'Locked Route' flag
    // ROUTES G8BPQ-2 2 100 !     - Set quality and toggle 'locked' flag


	ConvToAX25(ptr, AXCALL);

	ptr = strtok_s(NULL, " ", &Context);

	if (ptr)
		Port = atoi(ptr);

	if (Port == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Port Number Missing \r");
		goto SendReply;
	}

	Found = FindNeighbour(AXCALL, Port, &Routes);

	if (Context && Context[0] > 32)
	{
		// More Params

		ptr = strtok_s(NULL, " ", &Context);

		if (ptr)
		{
			// Adding

			memcpy(Routes->NEIGHBOUR_CALL, AXCALL, 7);	// In case Add
			Routes->NEIGHBOUR_PORT = Port;
			Found = TRUE;
		}

		if (strcmp(ptr, "!") == 0)
		{
			// Toggle Lock

			Routes->NEIGHBOUR_FLAG ^= 1;	// FLIP LOCKED BIT
			goto Displayit;
		}

		if (strcmp(ptr, "Z") == 0)
		{
			// Clear Counts

			Routes->NBOUR_IFRAMES = 0;
			Routes->NBOUR_RETRIES = 0;
			goto Displayit;
		}

		Routes->NEIGHBOUR_QUAL = atoi(ptr);

		if (Context && Context[0] == '!')
		{
			// Toggle Lock

			Routes->NEIGHBOUR_FLAG ^= 1;	// FLIP LOCKED BIT
			goto Displayit;
		}
	}

Displayit:

	// Just display

	if (Found)
		Bufferptr = DisplayRoute(Session, Bufferptr, Routes, 1);
	else
		Bufferptr += sprintf(Bufferptr, "Not Found\r");



/*	MOV	ROUTEDISP,1

	CMP	BYTE PTR [ESI],20H
	JE SHORT JUSTDISPLAY

	MOV	ZAPFLAG,0

	CMP	BYTE PTR [ESI],'Z'
	JNE SHORT NOTZAP

	MOV	ZAPFLAG,1
	JMP SHORT JUSTDISPLAY

	PUBLIC	NOTZAP
NOTZAP:

	MOV	ROUTEDISP,2		; LOCK UPDATE

	CMP	BYTE PTR [ESI],'!'
	JE SHORT JUSTDISPLAY
;
;	LOOK FOR V FOR ADDING A DIGI
;
	CMP	WORD PTR [ESI],' V'	; V [SPACE]
	JE 	ADDDIGI

	CALL	GETVALUE		; GET NUMBER, UP TO SPACE , CR OR OFFH
	JC SHORT BADROUTECMD		; INVALID DIGITS

	MOV	NEWROUTEVAL,AL

	MOV	ROUTEDISP,0

	CALL	SCAN			; SEE IF !
	MOV	AH,[ESI]


	PUBLIC	JUSTDISPLAY
JUSTDISPLAY:


	MOV	ESI,OFFSET32 AX25CALL
	CALL	_FINDNEIGHBOUR
	JZ SHORT FOUNDROUTE		; IN LIST - OK

	CMP	EBX,0
	JE SHORT BADROUTECMD		; TABLE FULL??

	MOV	ECX,7
	MOV	EDI,EBX
	REP MOVSB			; PUT IN CALL

	MOV	AL,SAVEPORT
	MOV	NEIGHBOUR_PORT[EBX],AL

	JMP SHORT FOUNDROUTE


	PUBLIC	BADROUTECMD
BADROUTECMD:

	POP	EDI

	JMP	PBADVALUE

	PUBLIC	FOUNDROUTE
FOUNDROUTE:

	CMP	ZAPFLAG,1
	JNE SHORT NOTCLEARCOUNTS

	XOR	AX,AX
	MOV	ES:WORD PTR NBOUR_IFRAMES[EDI],AX
	MOV	ES:WORD PTR NBOUR_IFRAMES+2[EDI],AX
	MOV	ES:WORD PTR NBOUR_RETRIES[EDI],AX
	MOV	ES:WORD PTR NBOUR_RETRIES+2[EDI],AX

	JMP SHORT NOUPDATE

	PUBLIC	NOTCLEARCOUNTS
NOTCLEARCOUNTS:

	CMP	ROUTEDISP,1
	JE SHORT NOUPDATE

	CMP	ROUTEDISP,2
	JE SHORT LOCKUPDATE

	MOV	AL,NEWROUTEVAL
	MOV	NEIGHBOUR_QUAL[EBX],AL

	CMP	AH,'!'
	JNE SHORT NOUPDATE

	PUBLIC	LOCKUPDATE
LOCKUPDATE:

	XOR	NEIGHBOUR_FLAG[EBX],1	; FLIP LOCKED BIT

	PUBLIC	NOUPDATE
NOUPDATE:

	MOV	ESI,EBX
	POP	EDI

	POP	EBX
	CALL	DISPLAYROUTE

	JMP	SENDCOMMANDREPLY

	PUBLIC	ADDDIGI
ADDDIGI:

	ADD	ESI,2
	PUSH	ESI			; SAVE INPUT BUFFER

	MOV	ESI,OFFSET32 AX25CALL
	CALL	_FINDNEIGHBOUR

	POP	ESI

	JZ SHORT ADD_FOUND		; IN LIST - OK

	JMP	BADROUTECMD

	PUBLIC	ADD_FOUND
ADD_FOUND:

	CALL	CONVTOAX25		; GET DIGI CALLSIGN

	PUSH	ESI

	MOV	ESI,OFFSET32 AX25CALL
	LEA	EDI,NEIGHBOUR_DIGI[EBX]
	MOV	ECX,7
	REP MOVSB

	POP	ESI			; MSG BUFFER
;
;	SEE IF ANOTHER DIGI
;
	CMP	BYTE PTR [ESI],20H
	JE SHORT NOMORE

	CALL	CONVTOAX25		; GET DIGI CALLSIGN
	MOV	ESI,OFFSET32 AX25CALL
	LEA	EDI,NEIGHBOUR_DIGI+7[EBX]
	MOV	ECX,7
	REP MOVSB

	PUBLIC	NOMORE
NOMORE:

	JMP	NOUPDATE



*/

SendReply:

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}


VOID LISTENCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	// PROCESS LISTEN COMMAND

	// for monitoring a remote ax.25 port

	int Port = 0, index =0;
	char * ptr, *Context;
	struct PORTCONTROL * PORT = NULL;
	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
		Port = atoi(ptr);

	if (ptr == 0 || memcmp(ptr, "OFF", 3) == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Listening disabled\r");
		Session->LISTEN = 0;
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (Port == 0 && NUMBEROFPORTS == 1)
		Port = 1;
	else
		ptr = strtok_s(NULL, " ", &Context);		// Get Unproto String

	if (Port)
		PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (PORT->PROTOCOL == 10) // && PORT->UICAPABLE == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Sorry, port is not an ax.25 port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (PORT->PORTL3FLAG)
	{
		Bufferptr += sprintf(Bufferptr, "Sorry, port is for internode traffic only\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}
	
	if (Session->L4CIRCUITTYPE == L2LINK + UPLINK)
	{
		if (Session->L4TARGET.LINK->LINKPORT->PORTNUMBER == Port)
		{
			Bufferptr += sprintf(Bufferptr, "You can't Listen to the port you are connected on\r");
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}
	}

	//	Copy Address Info to Session Record

	Session->LISTEN = Port;

	Bufferptr += sprintf(Bufferptr, "Listening on port %d. Use CQ to send a beacon, LIS to disable\r", Port);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;
}


VOID UNPROTOCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	// PROCESS UNPROTO COMMAND

	int Port = 0, index =0;
	char * ptr, *Context;
	struct PORTCONTROL * PORT = NULL;
	UCHAR axcalls[64];
	BOOL Stay, Spy;
	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
		Port = atoi(ptr);

	if (Port == 0 && NUMBEROFPORTS == 1)
		Port = 1;
	else
		ptr = strtok_s(NULL, " ", &Context);		// Get Unproto String

	if (Port)
		PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (ptr == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Destination missing\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}
	ptr[strlen(ptr)] = ' ';				// Put param back together

	if (DecodeCallString(ptr, &Stay, &Spy, &axcalls[0]) == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Call\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (PORT->PROTOCOL == 10 && PORT->UICAPABLE == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Sorry, port is not an ax.25 port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (PORT->PORTL3FLAG)
	{
		Bufferptr += sprintf(Bufferptr, "Sorry, port is for internode traffic only\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	//	Copy Address Info to Session Record

	Session->UNPROTO = Port;
	Session->UAddrLen = strlen(axcalls);
	memcpy(Session->UADDRESS, axcalls, 63);

	Bufferptr += sprintf(Bufferptr, "Unproto Mode - enter ctrl/z or /ex to exit\r");
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;
}

VOID CALCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	// PROCESS CAL COMMAND

	int Port = 0, index = 0, Count = 0;
	char * ptr, *Context;
	struct PORTCONTROL * PORT = NULL;

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
		Port = atoi(ptr);

	if (Port == 0 && NUMBEROFPORTS == 1)
		Port = 1;
	else
		ptr = strtok_s(NULL, " ", &Context);		// Get Unproto String

	if (Port)
		PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (PORT->PROTOCOL == 10 && PORT->UICAPABLE == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Sorry, port is not an ax.25 port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (ptr == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Count Missing\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Count = atoi(ptr);

	ptr = strtok_s(NULL, " ", &Context);		// Get Unproto String

	Bufferptr += sprintf(Bufferptr, "Ok\r");
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;
}



VOID CQCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	// Send a CQ Beacon on a radio port. Must be in LISTEN state

	DIGIMESSAGE Msg;
	int Port = Session->LISTEN;
	int Len;
	UCHAR CQCALL[7];
	
	if (Port == 0)
	{
		Bufferptr += sprintf(Bufferptr, "You must enter LISTEN before calling CQ\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Len = strlen(OrigCmdBuffer) - 3;

	if (Len < 0)
		Len = 0;

	memset(&Msg, 0, sizeof(Msg));

	Msg.PORT = Port;
	Msg.CTL = 3;			// UI

	ConvToAX25("CQ", CQCALL);
	memcpy(Msg.DEST, CQCALL, 7);
	memcpy(Msg.ORIGIN, Session->L4USER, 7);
	Msg.ORIGIN[6] ^= 0x1e;					// Flip SSID
	Msg.PID = 0xf0;							// Data PID
	memcpy(&Msg.L2DATA, &OrigCmdBuffer[3], Len);
	
	Send_AX_Datagram(&Msg, Len + 2, Port);		// Len is Payload ie CTL, PID and Data

	Bufferptr += sprintf(Bufferptr, "CQ sent\r");
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;

}


TRANSPORTENTRY * SetupNewSession(TRANSPORTENTRY * Session, char * Bufferptr)
{
	TRANSPORTENTRY * NewSess = L4TABLE;
	int Index = 0;
	
	while (Index < MAXCIRCUITS)
	{
		if (NewSess->L4USER[0] == 0)
		{
			// Got One

			Session->L4CROSSLINK = NewSess;
			NewSess->L4CROSSLINK = Session;

			memcpy(NewSess->L4USER, Session->L4USER, 7);
			memcpy(NewSess->L4MYCALL, Session->L4MYCALL, 7);
	

			NewSess->CIRCUITINDEX = Index;				//OUR INDEX
			NewSess->CIRCUITID = NEXTID;

			NEXTID++;
			if (NEXTID == 0)
				NEXTID++;								// kEEP nON-ZERO

			NewSess->SESSIONT1 = Session->SESSIONT1;
			NewSess->L4WINDOW = (UCHAR)L4DEFAULTWINDOW;

			return NewSess;
		}
		Index++;
		NewSess++;
	}

	Bufferptr += sprintf(Bufferptr, "Sorry - System Tables Full\r");
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return NULL;
	}


VOID DoNetromConnect(TRANSPORTENTRY * Session, char * Bufferptr, struct DEST_LIST * Dest, BOOL Spy)
{
	TRANSPORTENTRY * NewSess;
	
	NewSess = SetupNewSession(Session, Bufferptr);

	if (NewSess == NULL)
		return;						// Tables Full

	NewSess->L4CIRCUITTYPE = SESSION + DOWNLINK;

	NewSess->L4TARGET.DEST = Dest;
	NewSess->L4STATE = 2;		// CONNECTING

	NewSess->SPYFLAG = Spy;

	ReleaseBuffer((UINT *)REPLYBUFFER);

	SENDL4CONNECT(NewSess);

	L4CONNECTSOUT++;

	return;
}

BOOL FindLink(UCHAR * LinkCall, UCHAR * OurCall, int Port, struct _LINKTABLE ** REQLINK)
{
	struct _LINKTABLE * LINK = LINKS;
	struct _LINKTABLE * FIRSTSPARE = NULL;
	int n = MAXLINKS;

	while (n--)
	{
		if (LINK->LINKCALL[0] == 0)		// Spare
		{
			if (FIRSTSPARE == NULL)
				FIRSTSPARE = LINK;
		
			LINK++;
			continue;
		}

		if ((LINK->LINKPORT->PORTNUMBER == Port) && CompareCalls(LINK->LINKCALL, LinkCall) && CompareCalls(LINK->OURCALL, OurCall))
		{
			*REQLINK = LINK;
			return TRUE;
		}

		LINK++;
	}
	//	ENTRY NOT FOUND - FIRSTSPARE HAS FIRST FREE ENTRY, OR ZERO IF TABLE FULL

	*REQLINK = FIRSTSPARE;
	return FALSE;
}

VOID ATTACHCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * UserCMD);

VOID CMDC00(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	PROCESS CONNECT COMMAND
	
	TRANSPORTENTRY * NewSess;

	int CONNECTPORT, Port;
	BOOL CallEvenIfInNodes = FALSE;
	char * ptr, *Context;
	UCHAR axcalls[64];
	UCHAR ourcall[7];					// Call we are using (may have SSID bits inverted
	int ret;
	struct PORTCONTROL * PORT = PORTTABLE;
	struct _LINKTABLE * LINK;
	int CQFLAG = 0;			// NOT CQ CALL
	BOOL Stay, Spy;
	int n;
	char TextCall[10];
	int TextCallLen;
	char PortString[10];

#ifdef EXCLUDEBITS

	if (CheckExcludeList(Session->L4USER) == FALSE)
	{
		//	CONNECTS FROM THIS STATION ARE NOT ALLOWED

		ReleaseBuffer((UINT *)REPLYBUFFER);
		return;
	}

#endif

	if (Session->LISTEN)
	{
		Bufferptr += sprintf(Bufferptr, "Can't connect while listening\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	CONNECTPORT = 0;			// NO PORT SPECIFIED

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr == 0)
	{
		// No param

		if (CFLAG)				// C Command Disabled ?
		{
			// Convert to HOST (appl 32) command

		//MOV	_CMDPTR,OFFSET32 _HOSTCMD
		//MOV	_ALIASPTR,OFFSET32 _HOSTCMD + 32 * 31

		//MOV	_APPLMASK, 80000000H	; Internal Term

		}

		Bufferptr += sprintf(Bufferptr, "Invalid Call\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Port = atoi(ptr);
	
	if (Port)
	{
		//	IF THERE IS NOTHING FOLLOWING THE NUMBER, ASSUME IT IS A
		//	NUMERIC ALIAS INSTEAD OF A PORT

		sprintf(PortString, "%d", Port);

		if (strlen(PortString) < strlen(ptr))
			goto NoPort;

		PORT = GetPortTableEntryFromPortNum(Port);

		if (PORT == NULL)
		{
			Bufferptr += sprintf(Bufferptr, "Invalid Port\r");
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}

		ptr = strtok_s(NULL, " ", &Context);

		if (ptr == 0)
		{
			// No param

			Bufferptr += sprintf(Bufferptr, "Invalid Call\r");
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}
		
		CONNECTPORT = Port;
	
	}

NoPort:

	ptr[strlen(ptr)] = ' ';				// Put param back together

	if (ptr[0] == '!')
	{
		CallEvenIfInNodes = TRUE;
		ptr++;
	}

	if (DecodeCallString(ptr, &Stay, &Spy, &axcalls[0]) == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Call\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Session->STAYFLAG = Stay;

	TextCallLen = ConvFromAX25(axcalls, TextCall);

	if (CallEvenIfInNodes)
		goto Downlink;

	//	SEE IF CALL TO ANY OF OUR HOST SESSIONS - UNLESS DIGIS SPECIFIED

	if (axcalls[7] == 0)
	{
		//	If this connect is as a result of a command alias, don't check appls or we will loop

		if (ALIASINVOKED == 0)
		{
			APPLCALLS * APPL = APPLCALLTABLE;
			int n = NumberofAppls;
			APPLMASK = 1;

			while (n--)
			{
				if (memcmp(axcalls, APPL->APPLALIAS, 6) == 0 || CompareCalls(axcalls, APPL->APPLCALL))
				{
					// Call to an appl

					//	Convert to an APPL command, so any alias is actioned

					//	SEE IF THERE IS AN ALIAS DEFINDED FOR THIS COMMAND

					if (APPL->APPLHASALIAS && APPL->APPLALIASVAL[0] != 0x20)
					{
						//	COPY ALIAS TO COMMAND _BUFFER, THEN REENTER COMMAND HANDLER

						memcpy(COMMANDBUFFER, APPL->APPLALIASVAL, ALIASLEN);

						ALIASINVOKED = TRUE;			//	 To prevent Alias Loops 	
					}
					else
					{	
						
						// Copy Appl Command to COImmand Buffer

						memcpy(COMMANDBUFFER, APPL->APPLCMD, 12);
						COMMANDBUFFER[12] = 13;
					}
					DoTheCommand(Session);
					return;
				}
				APPL++;
				APPLMASK <<= 1;
			}
		}
	}

	if (axcalls[7] == 0)
	{
		//	SEE IF CALL TO ANOTHER NODE

		struct DEST_LIST * Dest = DESTS;
		int n = MAXDESTS;
	
		if (axcalls[6] == 0x60)		// if SSID, dont check aliases
		{	
			while (n--)
			{
				if (memcmp(Dest->DEST_ALIAS, TextCall, 6) == 0)	
				{
					DoNetromConnect(Session, Bufferptr, Dest, Spy);
					return;
				}
				Dest++;
			}
		}

		Dest = DESTS;
		n = MAXDESTS;
			
		while (n--)
		{
			if (CompareCalls(Dest->DEST_CALL, axcalls))	
			{
				DoNetromConnect(Session, Bufferptr, Dest, Spy);
				return;
			}
			Dest++;
		}
	}

	// Must be Downlink Connect

Downlink:

	if (CONNECTPORT == 0 && NUMBEROFPORTS > 1)
	{
		//	L2 NEEDS PORT NUMBER

		Bufferptr += sprintf(Bufferptr, "Downlink connect needs port number - C P CALLSIGN\r");

		// Send Port List

		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	//	ENSURE PORT IS AVAILABLE FOR L2 USE

	if (PORT->PROTOCOL >= 10)			// Pactor=-style port?
	{
		struct _EXTPORTDATA * EXTPORT = (struct _EXTPORTDATA *)PORT;
		int count;

		// 	if Via PACTOR or WINMOR, convert to attach and call = Digi's are in AX25STRING (+7)

		if (memcmp(&axcalls[7], &WINMOR[0], 6) == 0 || memcmp(&axcalls[7], &PACTORCALL[0], 6) == 0)
		{
			char newcmd[80];

			TextCall[TextCallLen] = 0;
			sprintf(newcmd, "%s %s", CmdTail, TextCall);

			ATTACHCMD(Session, Bufferptr, newcmd, NULL);
			return;
		}
	
		//	If on a KAM or SCS with ax.25 on port 2, do an Attach command, then pass on connect

		if (EXTPORT->MAXHOSTMODESESSIONS <= 1)
		{
			Bufferptr += sprintf(Bufferptr, "Sorry, port is not an ax.25 port\r");
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}

		//	Only Allow Attach VHF from Secure Applications or if PERMITGATEWAY is set

		if (EXTPORT->PERMITGATEWAY == 0 && Session->Secure_Session == 0)
		{
			Bufferptr += sprintf(Bufferptr, "Sorry, you are not allowed to use this port\r");
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}

		count = EXTPORT->MAXHOSTMODESESSIONS;
		count--;								// First is Pactor Stream, count is now last ax.25 session
	
		while (count)
		{
			if (EXTPORT->ATTACHEDSESSIONS[count] == 0)
			{
				int Paclen, PortPaclen;
				struct DATAMESSAGE * Buffer;
				struct DATAMESSAGE Message = {0};
				char Callstring[80];
				int len;
				
				//	Found a free one - use it

				//	See if TNC is OK

				Message.PORT = count;

				ret = PORT->PORTTXCHECKCODE(PORT, Message.PORT);
			
				if ((ret & 0xff00) == 0)
				{
					Bufferptr += sprintf(Bufferptr, "Error - TNC Not Ready\r");
					SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
					return;
				}

				//	GET CIRCUIT TABLE ENTRY FOR OTHER END OF LINK
	
				NewSess = SetupNewSession(Session, Bufferptr);
				if (NewSess == NULL)
					return;

				EXTPORT->ATTACHEDSESSIONS[count] = NewSess;

				NewSess->KAMSESSION = count;
				
				//	Set paclen to lower of incoming and outgoing

				Paclen = Session->SESSPACLEN;	// Incoming PACLEN

				if (Paclen == 0)
					Paclen = 256;				// 0 = 256

				PortPaclen = PORT->PORTPACLEN;

				if (PortPaclen == 0)
					PortPaclen = 256;				// 0 = 256

				if (PortPaclen < Paclen)
					Paclen = PortPaclen;

				NewSess->SESSPACLEN = Paclen;
				Session->SESSPACLEN = Paclen;

				NewSess->L4STATE = 5;
				NewSess->L4CIRCUITTYPE = DOWNLINK + PACTOR;
				NewSess->L4TARGET.PORT = PORT;

				// Send the connect command to the TNC

				Buffer = REPLYBUFFER;

				Buffer->PORT = count;
				Buffer->PID = 0xf0;

				TextCall[TextCallLen] = 0;

				len = sprintf(Callstring,"C %s", TextCall);

				if (axcalls[7])
				{
					int digi = 7;

					// we have digis

					len += sprintf(&Callstring[len], " via");

					while (axcalls[digi])
					{
						TextCall[ConvFromAX25(&axcalls[digi], TextCall)] = 0;
						len += sprintf(&Callstring[len], " %s", TextCall);
						digi += 7;
					}
				}
				Callstring[len++] = 13;
				Callstring[len] = 0;
						
				Buffer->LENGTH = len + 8;
				memcpy(Buffer->L2DATA, Callstring, len);
				C_Q_ADD(&PORT->PORTTX_Q, (UINT *)Buffer);

				return;
			}
			count--;
		}

		Bufferptr += sprintf(Bufferptr, "Error - No free streams on this port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if ((Session->L4CIRCUITTYPE & BPQHOST) == 0 && PORT->PORTL3FLAG)
	{
		//Port only for L3 
	
		Bufferptr += sprintf(Bufferptr, "Sorry, port is for internode traffic only\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (PORT->PortUIONLY)
	{
		//Port only for UI
	
		Bufferptr += sprintf(Bufferptr, "Sorry, port is for UI traffic only\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}
	
	if (Session->L4USER[6] == 0x42 || Session->L4USER[6] == 0x44)
	{	
		Bufferptr += sprintf(Bufferptr, "Sorry - Can't make ax.25 calls with SSID of T or R\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}


	// Get Session Entry for Downlink


	NewSess = SetupNewSession(Session, Bufferptr);
	if (NewSess == NULL)
		return;

	NewSess->L4CIRCUITTYPE = L2LINK + DOWNLINK;
	
	//	FORMAT LINK TABLE ENTRY FOR THIS CONNECTION

	memcpy(ourcall, NewSess->L4USER, 7);

	if ((Session->L4CIRCUITTYPE & (BPQHOST | PACTOR)) == 0)	// SSID SWAP TEST - LEAVE ALONE FOR HOST
		ourcall[6] ^= 0x1e;									// Flip SSID
	
	//	SET UP NEW SESSION (OR RESET EXISTING ONE)

	FindLink(axcalls, ourcall, Port, &LINK);
	
	if (LINK == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Sorry - System Tables Full\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		
		// Should release NewSess

		return;
	}

	memcpy(LINK->LINKCALL, axcalls, 7);
	memcpy(LINK->OURCALL, ourcall, 7);

	LINK->LINKPORT = PORT;

	LINK->L2TIME = PORT->PORTT1;
	
	// Copy Digis

	n = 7;
	ptr = &LINK->DIGIS[0];

	while (axcalls[n])
	{
		memcpy(ptr, &axcalls[n], 7);
		n += 7;
		ptr += 7;

		LINK->L2TIME += 2 * PORT->PORTT1;	// ADJUST TIMER VALUE FOR 1 DIGI
	}

	LINK->LINKTYPE = 2;						// DOWNLINK
	LINK->LINKWINDOW = PORT->PORTWINDOW;

	RESET2(LINK);						// RESET ALL FLAGS

	LINK->L2STATE = 2;					// CONNECTING

	LINK->CIRCUITPOINTER = NewSess;

	NewSess->L4TARGET.LINK = LINK;

	if (PORT->PORTPACLEN)
		NewSess->SESSPACLEN = Session->SESSPACLEN = PORT->PORTPACLEN;

	if (CQFLAG == 0)			// if a CQ CALL  DONT SEND SABM
		SENDSABM(LINK);

	ReleaseBuffer((UINT *)REPLYBUFFER);
	return;
}	

BOOL DecodeCallString(char * Calls, BOOL * Stay, BOOL * Spy, UCHAR * AXCalls)
{
	//	CONVERT CALL + OPTIONAL DIGI STRING TO AX25, RETURN 
	//	CONVERTED STRING IN AXCALLS. Return FALSE if invalied

	char * axptr = AXCalls;
	char * ptr, *Context;
	int CQFLAG = 0;					// NOT CQ CALL
	int n = 8;						// Max digis

	*Stay = 0;
	*Spy = 0;

	memset(AXCalls, 0, 64);

	ptr = strtok_s(Calls, " ,", &Context);

	if (ptr == NULL)
		return FALSE;

	// First field is Call

	if (ConvToAX25(ptr, axptr) == 0)
		return FALSE;

	axptr += 7;

	ptr = strtok_s(NULL, " ,", &Context);

	while (ptr && n--)
	{
		// NEXT FIELD = COULD BE CALLSIGN, VIA, OR S (FOR SAVE)

		if (strcmp(ptr, "S") == 0)
			*Stay = TRUE;
		else if (strcmp(ptr, "Z") == 0)
			*Spy = TRUE;
		else if (memcmp(ptr, "VIA", strlen(ptr)) == 0)
		{
		}	//skip via
		else					
		{
			// Convert next digi

			if (ConvToAX25(ptr, axptr) == 0)
				return FALSE;

			axptr += 7;
		}

		ptr = strtok_s(NULL, " ,", &Context);
	}

	return TRUE;
}


VOID LINKCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	// PROCESS *** LINKED to CALLSIGN

	char * ptr, *Context;
	UCHAR axcall[7];
	int ret;

	if (LINKEDFLAG == 'Y' ||			// UNCONDITIONAL?
		(LINKEDFLAG == 'A' && 
			((Session->L4CIRCUITTYPE & BPQHOST) || Session->Secure_Session || Session->PASSWORD == 0xffff)))
	{
		ptr = strtok_s(CmdTail, " ", &Context);
		if (ptr) 
			ptr = strtok_s(NULL, " ", &Context);
		
		if (ptr)
		{
			ret = ConvToAX25Ex(ptr, axcall);

			if (ret)
			{
				memcpy(Session->L4USER, axcall, 7);
				strcpy(Bufferptr, OKMSG);
				Bufferptr += strlen(OKMSG);
				SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
				return;
			}
		}
	
		strcpy(Bufferptr, BADMSG);
		Bufferptr += strlen(BADMSG);
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	memcpy(Bufferptr, PASSWORDMSG, LPASSMSG);
	Bufferptr += LPASSMSG;
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

int CompareNode(const void *a, const void *b);
int CompareAlias(const void *a, const void *b);

char * DoOneNode(TRANSPORTENTRY * Session, char * Bufferptr, struct DEST_LIST * Dest)
{
	char Normcall[10];
	char Alias[10];
	struct NR_DEST_ROUTE_ENTRY * NRRoute;
	struct DEST_ROUTE_ENTRY * Route;
	struct ROUTE * Neighbour;
	int i, Active, len;

	Alias[6] = 0;

	memcpy(Alias, Dest->DEST_ALIAS, 6);
	strlop(Alias, ' ');

	Normcall[ConvFromAX25(Dest->DEST_CALL, Normcall)] = 0;

	Bufferptr += sprintf(Bufferptr, "Routes to: %s:%s", Alias, Normcall);

	if (Dest->DEST_COUNT)
		Bufferptr += sprintf(Bufferptr, " RTT=%4.2f FR=%d %c %.1d\r",
			Dest->DEST_RTT /1000.0, Dest->DEST_COUNT, 
			(Dest->DEST_STATE & 0x40)? 'B':' ', (Dest->DEST_STATE & 63));
	else
		*(Bufferptr++) = 13;

	NRRoute = &Dest->NRROUTE[0];

	Active = Dest->DEST_ROUTE;

	for (i = 1; i < 4; i++)
	{
		Neighbour = NRRoute->ROUT_NEIGHBOUR;

		if (Neighbour)
		{
			len = ConvFromAX25(Neighbour->NEIGHBOUR_CALL, Normcall);
			Normcall[len] = 0;

			Bufferptr += sprintf(Bufferptr, "%c %d %d %d %s\r",
				(Active == i)?'>':' ',NRRoute->ROUT_QUALITY, NRRoute->ROUT_OBSCOUNT, Neighbour->NEIGHBOUR_PORT, Normcall);
		}
		NRRoute++;
	}

	//	DISPLAY  INP3 ROUTES

	Route = &Dest->ROUTE[0];

	Active = Dest->DEST_ROUTE;

	for (i = 1; i < 4; i++)
	{
		Neighbour = Route->ROUT_NEIGHBOUR;

		if (Neighbour)
		{
			double srtt = Route->SRTT/1000.0;

			len = ConvFromAX25(Neighbour->NEIGHBOUR_CALL, Normcall);
			Normcall[len] = 0;

			Bufferptr += sprintf(Bufferptr, "%c %d %4.2fs %d %s\r",
				(Active == i + 3)?'>':' ',Route->Hops, srtt, Neighbour->NEIGHBOUR_PORT, Normcall);
		}
		Route++;
	}

	return Bufferptr;
}


int DoViaEntry(struct DEST_LIST * Dest, int n, char * line, int cursor)
{
	char Portcall[10];
	int len;

	if (Dest->NRROUTE[n].ROUT_NEIGHBOUR != 0 && Dest->NRROUTE[n].ROUT_NEIGHBOUR->INP3Node == 0)
	{
		len=ConvFromAX25(Dest->NRROUTE[n].ROUT_NEIGHBOUR->NEIGHBOUR_CALL, Portcall);
		Portcall[len]=0;

		len=sprintf(&line[cursor],"%s %d %d ",
			Portcall,
			Dest->NRROUTE[n].ROUT_NEIGHBOUR->NEIGHBOUR_PORT,
			Dest->NRROUTE[n].ROUT_QUALITY);

		cursor+=len;

		if (Dest->NRROUTE[n].ROUT_OBSCOUNT > 127)
		{
			len=sprintf(&line[cursor],"! ");
			cursor+=len;
		}
	}
	return cursor;
}

int DoINP3ViaEntry(struct DEST_LIST * Dest, int n, char * line, int cursor)
{
	char Portcall[10];
	int len;
	double srtt;

	if (Dest->ROUTE[n].ROUT_NEIGHBOUR != 0)
	{
		srtt = Dest->ROUTE[n].SRTT/1000.0;

		len=ConvFromAX25(Dest->ROUTE[n].ROUT_NEIGHBOUR->NEIGHBOUR_CALL, Portcall);
		Portcall[len]=0;

		len=sprintf(&line[cursor],"%s %d %d %4.2fs ",
			Portcall,
			Dest->ROUTE[n].ROUT_NEIGHBOUR->NEIGHBOUR_PORT,
			Dest->ROUTE[n].Hops, srtt);

		cursor+=len;

		if (Dest->NRROUTE[n].ROUT_OBSCOUNT > 127)
		{
			len=sprintf(&line[cursor],"! ");
			cursor+=len;
		}
	}
	return cursor;
}

VOID CMDN00(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	struct DEST_LIST * Dest = DESTS;
	int count = MAXDESTS, i;
	char Normcall[10];
	char Alias[10];
	int Width = 4;
	int x = 0, n = 0;
	struct DEST_LIST * List[1000];
	char Param = 0;
	char * ptr, * Context;
	char Nodeline[20];
	char AXCALL[7];
	char * Call;
	char * Qualptr;
	int Qual;
	char line[160];
	int cursor, len;

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
	{
		if (strcmp(ptr, "ADD") == 0)
			goto NODE_ADD;
	
		if (strcmp(ptr, "DEL") == 0)
			goto NODE_DEL;

		if (strcmp(ptr, "VIA") == 0)
			goto NODE_VIA;
	}
	
	if (ptr)
		Param = ptr[0];

	if (ptr && strlen(ptr) > 1)
	{
		//	NODE CALL command
	
		UCHAR AXCall[8];
		int count;
		char * wildcard = strchr(ptr, '*');
		int paramlen = strlen(ptr);
		char parampadded[20];

		Alias[8] = 0;
		strcpy(parampadded, ptr);
		strcat(parampadded, "     ");

		if (wildcard)
			*wildcard = 0;

		ConvToAX25(ptr, AXCall);

		//	if * on end, list all ssids

		if (wildcard)
		{
			AXCall[6] = 0;

			*(Bufferptr++) = 13;
		
			while (AXCall[6] < 32)
			{
				Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	

				Dest = DESTS;

				for (count = 0; count < MAXDESTS; count++)
				{
					if (memcmp(Dest->DEST_ALIAS, parampadded, 6) == 0 || CompareCalls(Dest->DEST_CALL, AXCall))
					{
						break;
					}
					Dest++;
				}

				if (count < MAXDESTS)
				{
					Bufferptr = DoOneNode(Session, Bufferptr, Dest);
				}

				AXCall[6] += 2;
			}
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}

		for (count = 0; count < MAXDESTS; count++)
		{
			if (memcmp(Dest->DEST_ALIAS, parampadded, 6) == 0 || CompareCalls(Dest->DEST_CALL, AXCall))
			{
				break;
			}
			Dest++;
		}
		
		if (count == MAXDESTS)
		{
			Bufferptr += sprintf(Bufferptr, "Not found\r");
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}

		Bufferptr = DoOneNode(Session, Bufferptr, Dest);

		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Bufferptr += sprintf(Bufferptr, "Nodes\r");

	while (count--)
	{
		if (Dest->DEST_CALL[0] != 0)
		{
			if (Param != 'T' || Dest->DEST_COUNT)
				List[n++] = Dest;

			if (n > 999)
				break;
		}

		Dest++;
	}
	
	if (n > 1)
	{
		if (Param == 'C') 
			qsort(List, n, 4, CompareNode);
		else
			qsort(List, n, 4, CompareAlias);
	}
		

	for (i = 0; i < n; i++)
	{
		int len = ConvFromAX25(List[i]->DEST_CALL, Normcall);
		Normcall[len]=0;
		
		memcpy(Alias, List[i]->DEST_ALIAS, 6);
		Alias[6] = 0; 
		strlop(Alias, ' ');

		if (strlen(Alias))
			strcat(Alias, ":");

		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	

		if (Param == 'T')
		{
			Bufferptr += sprintf(Bufferptr, "%s%s RTT=%4.2f Frames = %d %c %.1d\r",
				Alias, Normcall, List[i]->DEST_RTT /1000.0, List[i]->DEST_COUNT, 
				(List[i]->DEST_STATE & 0x40)? 'B':' ', (List[i]->DEST_STATE & 63));
		}
		else
		{
			len = sprintf(Nodeline, "%s%s", Alias, Normcall);
			memset(&Nodeline[len], ' ', 20 - len);
			Bufferptr = MOVEANDCHECK(Session, Bufferptr, Nodeline, 20);

			if (++x == Width)
			{
				x = 0;
				*(Bufferptr++) =  '\r';
			}
		}
	}

	if (x)
		*(Bufferptr++) =  '\r';

	goto SendReply;


NODE_VIA:

	// List Nodes reachable via a neighbour

	ptr = strtok_s(NULL, " ", &Context);

	if (ptr == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Missing Call\r");
		goto SendReply;
	}

	Bufferptr += sprintf(Bufferptr, "\r");

	ConvToAX25(ptr, AXCALL);

	Dest = DESTS;

	Dest-=1;

	for (count=0; count<MAXDESTS; count++)
	{
		Dest+=1;

		if (Dest->NRROUTE[0].ROUT_NEIGHBOUR == 0 && Dest->ROUTE[0].ROUT_NEIGHBOUR == 0)
			continue;

		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM	

		if (
			   (Dest->NRROUTE[0].ROUT_NEIGHBOUR && CompareCalls(Dest->NRROUTE[0].ROUT_NEIGHBOUR->NEIGHBOUR_CALL, AXCALL))
			|| (Dest->NRROUTE[1].ROUT_NEIGHBOUR && CompareCalls(Dest->NRROUTE[1].ROUT_NEIGHBOUR->NEIGHBOUR_CALL, AXCALL))
			|| (Dest->NRROUTE[2].ROUT_NEIGHBOUR && CompareCalls(Dest->NRROUTE[2].ROUT_NEIGHBOUR->NEIGHBOUR_CALL, AXCALL))

			|| (Dest->ROUTE[0].ROUT_NEIGHBOUR && CompareCalls(Dest->ROUTE[0].ROUT_NEIGHBOUR->NEIGHBOUR_CALL, AXCALL))
			|| (Dest->ROUTE[1].ROUT_NEIGHBOUR && CompareCalls(Dest->ROUTE[1].ROUT_NEIGHBOUR->NEIGHBOUR_CALL, AXCALL))
			|| (Dest->ROUTE[2].ROUT_NEIGHBOUR && CompareCalls(Dest->ROUTE[2].ROUT_NEIGHBOUR->NEIGHBOUR_CALL, AXCALL)))
		{
			len=ConvFromAX25(Dest->DEST_CALL,Normcall);

			Normcall[len]=0;

			memcpy(Alias,Dest->DEST_ALIAS,6);

			Alias[6]=0;

			for (i=0;i<6;i++)
			{
				if (Alias[i] == ' ')
					Alias[i] = 0;
			}

			cursor=sprintf(line,"%s:%s ", Alias,Normcall);

			cursor = DoViaEntry(Dest, 0, line, cursor);
			cursor = DoViaEntry(Dest, 1, line, cursor);
			cursor = DoViaEntry(Dest, 2, line, cursor);
			cursor = DoINP3ViaEntry(Dest, 0, line, cursor);
			cursor = DoINP3ViaEntry(Dest, 1, line, cursor);
			cursor = DoINP3ViaEntry(Dest, 2, line, cursor);

			line[cursor++]='\r';
			line[cursor++]=0;

			Bufferptr += sprintf(Bufferptr, "%s", line);
		}
		}


	goto SendReply;

NODE_ADD:

	//	FORMAT IS NODE ADD ALIAS:CALL QUAL ROUTE PORT


	if (Session->PASSWORD  != 0xFFFF)
	{
		Bufferptr += sprintf(Bufferptr, "%s", PASSWORDMSG);
		goto SendReply;
	}

	ptr = strtok_s(NULL, " ", &Context);

	if (ptr == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Missing Alias:Call\r");
		goto SendReply;
	}

	Call = strlop(ptr, ':');

	ConvToAX25(Call, AXCALL);

	Qualptr = strtok_s(NULL, " ", &Context);

	if (Qualptr == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Quality missing\r");
		goto SendReply;
	}

	Qual = atoi(Qualptr);

	if (Qual < MINQUAL)
	{
		Bufferptr += sprintf(Bufferptr, "Quality is below MINQUAL\r");
		goto SendReply;
	}

	if (FindDestination(AXCALL, &Dest))
	{
		Bufferptr += sprintf(Bufferptr, "Node already in Table\r");
		goto SendReply;
	}

	if (Dest == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Node Table Full\r");
		goto SendReply;
	}

	memcpy(Dest->DEST_CALL, AXCALL, 7);		
	memcpy(Dest->DEST_ALIAS, ptr, 6);

	NUMBEROFNODES++;
/*
	ptr = strtok_s(NULL, seps, &Context);
	
	if (ptr == NULL || ptr[0] == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Neighbour missing\r");
		goto SendReply;
	}

	if (ConvToAX25(ptr, axcall) == 0)
	{
			ptr = strtok_s(NULL, seps, &Context);
			if (ptr == NULL) continue;
			Port = atoi(ptr);

			ptr = strtok_s(NULL, seps, &Context);
			if (ptr == NULL) continue;
			Qual = atoi(ptr);

			if (Context[0] == '!')
			{
				OBSINIT = 255;			//; SPECIAL FOR LOCKED
			}
		
			if (FindNeighbour(axcall, Port, &ROUTE))
			{
				PROCROUTES(DEST, ROUTE, Qual);
			}

			OBSINIT = SavedOBSINIT;

			goto RouteLoop;
	
PNODE48:


;	GET NEIGHBOURS FOR THIS DESTINATION
;
	CALL	CONVTOAX25
	JNZ SHORT BADROUTE
;
	CALL	GETVALUE
	MOV	SAVEPORT,AL		; SET PORT FOR _FINDNEIGHBOUR

	CALL	GETVALUE
	MOV	ROUTEQUAL,AL
;
	MOV	ESI,OFFSET32 AX25CALL

	PUSH	EBX			; SAVE DEST
	CALL	_FINDNEIGHBOUR
	MOV	EAX,EBX			; ROUTE TO AX
	POP	EBX

	JZ SHORT NOTBADROUTE

	JMP SHORT BADROUTE

NOTBADROUTE:
;
;	UPDATE ROUTE LIST FOR THIS DEST
;
	MOV	ROUT1_NEIGHBOUR[EBX],EAX
	MOV	AL,ROUTEQUAL
	MOV	ROUT1_QUALITY[EBX],AL
	MOV	ROUT1_OBSCOUNT[EBX],255	; LOCKED
;
	POP	EDI
	POP	EBX
	
	INC	_NUMBEROFNODES

	JMP	SENDOK

BADROUTE:
;
;	KILL IT
;
	MOV	ECX,TYPE DEST_LIST
	MOV	EDI,EBX
	MOV	AL,0
	REP STOSB

	JMP	BADROUTECMD	

*/
	
	goto SendReply;


NODE_DEL:
	
	if (Session->PASSWORD  != 0xFFFF)
	{
		Bufferptr += sprintf(Bufferptr, "%s", PASSWORDMSG);
		goto SendReply;
	}

	ptr = strtok_s(NULL, " ", &Context);

	if (ptr == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Missing Call\r");
		goto SendReply;
	}

	if (strcmp(ptr, "ALL") == 0)
	{
		struct DEST_LIST * DEST = DESTS;
		int n = MAXDESTS;

		while (n--)
		{
			if (DEST->DEST_CALL[0] && ((DEST->DEST_STATE & 0x80) == 0))			// Don't delete locked
					REMOVENODE(DEST);

			DEST++;
		}

		ClearNodes();

		Bufferptr += sprintf(Bufferptr, "All Nodes Deleted\r");
		goto SendReply;
	}

	ConvToAX25(ptr, AXCALL);

	if (FindDestination(AXCALL, &Dest) == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Not Found\r");
		goto SendReply;
	}

	if (Dest->DEST_STATE & 0x80)
		Bufferptr += sprintf(Bufferptr, "APPL Node - Can't delete\r");
	else
	{
		REMOVENODE(Dest);
		Bufferptr += sprintf(Bufferptr, "Node Deleted\r");
	}
	Bufferptr += sprintf(Bufferptr, "Node Deleted\r");

SendReply:

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}
char * CHECKBUFFER(TRANSPORTENTRY * Session, char * Bufferptr)
{
	//	MAKE SURE THERE IS ROOM IN CURRENT _BUFFER

	int Len = Bufferptr - (char *)REPLYBUFFER;
	struct DATAMESSAGE * Buffer;

	if (Len < CMDPACLEN && Len < 156)		// should be enough
		return Bufferptr;

	//	END OF _BUFFER - QUEUE IT AND GET ANOTHER

	Buffer = (struct DATAMESSAGE *)GetBuff();

	if (Buffer == NULL)
	{
		// No buffers, so just reuse the old one (better than crashing !!)

		Buffer = REPLYBUFFER;

		return &Buffer->L2DATA[0];
	}

	SendCommandReply(Session, REPLYBUFFER, Len);

	REPLYBUFFER = Buffer;
	Buffer->PID = 0xf0;

	return &Buffer->L2DATA[0];
}

char * MOVEANDCHECK(TRANSPORTENTRY * Session, char * Bufferptr, char * Source, int Len)
{
	while (Len--)
	{
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		*(Bufferptr++) = *(Source++);
	}
	return Bufferptr;
}

VOID CMDQUERY(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * UserCMD)
{
	//	DISPLAY AVAILABLE COMMANDS

	int n;
	char * ptr;

	CMDX * CMD = &COMMANDS[APPL1];

	for (n = 0; n < NumberofAppls; n++)
	{
		ptr = &CMD->String[0];
		if (*(ptr) != '*')
		{
			Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
			while (*ptr != ' ')
			{
				*(Bufferptr++) = *(ptr++);
			}
			*(Bufferptr++) = ' ';
		}
		CMD++;
	}

	n = CMDLISTLEN;

	if (NEEDMH == 0)
		n -= 7;					// Dont show MH

	Bufferptr = MOVEANDCHECK(Session, Bufferptr, CMDLIST, n);

	*(Bufferptr++) = 13;

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

char * FormatMH(MHSTRUC * MH);

VOID MHCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	DISPLAY HEARD LIST
	
	int Port = 0, sess = 0;
	char * ptr, *Context;
	struct PORTCONTROL * PORT = NULL;
	MHSTRUC * MH;
	int n = MHENTRIES;
	char Normcall[20];
	char From[10];
	char DigiList[100];
	char * Output;
	int len;
	char Digi = 0;


	// Note that the MHDIGIS field may contain rubbish. You have to check End of Address bit to find
	// how many digis there are

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
		Port = atoi(ptr);

	if (Port)
		PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	MH = PORT->PORTMHEARD;

	if (MH == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "MHEARD not enabled on that port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (strstr(Context, "CLEAR"))
	{
		memset(MH, 0, MHENTRIES * sizeof(MHSTRUC));
		Bufferptr += sprintf(Bufferptr, "Heard List for Port %d Cleared\r", Port);
	}
	else
		Bufferptr += sprintf(Bufferptr, "Heard List for Port %d\r", Port);

	while (n--)
	{
		if (MH->MHCALL[0] == 0)
			break;

		Bufferptr = CHECKBUFFER(Session, Bufferptr);
		
		Digi = 0;
		
		len = ConvFromAX25(MH->MHCALL, Normcall);

		Normcall[len++] = MH->MHDIGI;
		Normcall[len++] = 0;

		n = 8;					// Max number of digi-peaters

		ptr = &MH->MHCALL[6];	// End of Address bit

		Output = &DigiList[0];

		if ((*ptr & 1) == 0)
		{
			// at least one digi

			strcpy(Output, "via ");
			Output += 4;
		
			while ((*ptr & 1) == 0)
			{
				//	MORE TO COME
	
				From[ConvFromAX25(ptr + 1, From)] = 0;
				Output += sprintf(Output, "%s", From);
	
				ptr += 7;
				n--;

				if (n == 0)
					break;

				// See if digi actioned - put a * on last actioned

				if (*ptr & 0x80)
				{
					if (*ptr & 1)						// if last address, must need *
					{
						*(Output++) = '*';
						Digi = '*';
					}

					else
						if ((ptr[7] & 0x80) == 0)		// Repeased by next?
						{
							*(Output++) = '*';			// No, so need *
							Digi = '*';
						}
					
}
				*(Output++) = ',';
			}		
			*(--Output) = 0;							// remove last comma
		}
		else 
			*(Output) = 0;

		// if we used a digi set * on call and display via string


		if (Digi)
			Normcall[len++] = Digi;
		else
			DigiList[0] = 0;	// Dont show list if not used

		Normcall[len++] = 0;


		ptr = FormatMH(MH);
		
		if (CMD->String[2] == 'V')		// MHV
			Bufferptr += sprintf(Bufferptr, "%-10s %d %s %s\r", Normcall, MH->MHCOUNT, ptr, DigiList);
		else
			Bufferptr += sprintf(Bufferptr, "%-10s %s %s\r", Normcall, ptr, DigiList);

		MH++;
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}
	
VOID APRSMHCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	DISPLAY APRES HEARD LIST

	// APRS [Port] [Pattern] 

	APRSSTATIONRECORD * MH = MHDATA;
	int n = MAXHEARDENTRIES;
	int len;
	char * ptr;
	char * Pattern, * context;
	int Port = -1;
	char dummypattern[] ="";

	Pattern = strtok_s(CmdTail, " \r", &context);

	if (Pattern && strlen(Pattern) < 3)
	{
		// could be port number

		if (isdigit(Pattern[0]) && (Pattern[1] == 0 || isdigit(Pattern[1])))
		{
			Port = atoi(Pattern);
			Pattern = strtok_s(NULL, " \r", &context);
		}
	}

	// Param is a pattern to match

	if (Pattern == NULL)
		Pattern = dummypattern;

	if (Pattern[0] == ' ')
	{
		// Prepare Pattern

		char * ptr1 = Pattern + 1;
		char * ptr2 = Pattern;
		char c;
		
		do
		{
			c = *ptr1++;
			*(ptr2++) = c;
		}
		while (c != ' ');

		*(--ptr2) = 0;
	}

	strlop(Pattern, ' ');
	_strupr(Pattern);

	*(Bufferptr++) = 13;

	while (n--)
	{
		if (MH->MHCALL[0] == 0)
			break;

		if ((Port > -1) && Port != MH->Port)
		{
			MH++;
			continue;
		}

		Bufferptr = CHECKBUFFER(Session, Bufferptr);
	
		ptr = FormatAPRSMH(MH);

		MH++;
		
		if (ptr)
		{
			if (Pattern[0] && strstr(ptr, Pattern) == 0)
				continue;

			len = strlen(ptr);
			memcpy(Bufferptr, ptr, len);
			Bufferptr += len;
		}
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

int Rig_Command(int Session, char * Command);

VOID RADIOCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * UserCMD)
{
	char * ptr;
	
	if (Rig_Command(Session->CIRCUITINDEX, CmdTail))
	{
		ReleaseBuffer((UINT *)REPLYBUFFER);
		return;
	}
	
	// Error Message is in buffer

	ptr = strchr(CmdTail, 13);
	
	if (ptr)
	{
		int len = ++ptr - CmdTail;
				
		memcpy(Bufferptr, CmdTail, len);
		Bufferptr += len;
	}
	
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}


VOID SendNRRecordRoute(struct DEST_LIST * DEST, TRANSPORTENTRY * Session);


VOID NRRCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * UserCMD)
{
	//	PROCESS 'NRR - Netrom Record Route' COMMAND

	char * ptr, *Context;
	struct DEST_LIST * Dest = DESTS;
	int count = MAXDESTS;

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
	{	
		UCHAR AXCall[8];
		int count;

		ConvToAX25(ptr, AXCall);
		strcat(ptr, "      ");

		for (count = 0; count < MAXDESTS; count++)
		{
			if (memcmp(Dest->DEST_ALIAS, ptr, 6) == 0 || CompareCalls(Dest->DEST_CALL, AXCall))
			{
				SendNRRecordRoute(Dest, Session);
				memcpy(Bufferptr, OKMSG, 3);
				Bufferptr += 3;
				SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	
				return;
			}
			Dest++;
		}
	}
	Bufferptr += sprintf(Bufferptr, "Not found\r");
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;
}

int CHECKINTERLOCK(struct PORTCONTROL * OURPORT)
{
	//	See if any Interlocked ports are Busy

	struct PORTCONTROL * PORT = PORTTABLE;
	struct _EXTPORTDATA * EXTPORT;
	
	int n = NUMBEROFPORTS;
	int ourgroup = OURPORT->PORTINTERLOCK;

	while (PORT)
	{
		if (PORT != OURPORT)
		{
			if (PORT->PORTINTERLOCK == ourgroup)
			{
				// Same Group - is it busy

				EXTPORT = (struct _EXTPORTDATA *)PORT;
				if (EXTPORT->ATTACHEDSESSIONS[0])
					return PORT->PORTNUMBER;
			}
		}
		PORT = PORT->PORTPOINTER;
	}

	return 0;
}

VOID ATTACHCMD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * UserCMD)
{
	//	ATTACH to a PACTOR or similar port

	TRANSPORTENTRY * NewSess;
	struct _EXTPORTDATA * EXTPORT;
	
	int Port = 0, sess = 0;
	char * ptr, *Context;
	int ret;
	struct PORTCONTROL * PORT = NULL;
	struct DATAMESSAGE Message = {0};
	int Paclen, PortPaclen;
	struct DATAMESSAGE * Buffer;

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
		Port = atoi(ptr);

	if (Port)
		PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == NULL || PORT->PROTOCOL < 10)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	// If attach on telnet port, find a free stream

	EXTPORT = (struct _EXTPORTDATA *)PORT;

	if (strstr(EXTPORT->PORT_DLL_NAME, "TELNET"))
	{
		int count = EXTPORT->MAXHOSTMODESESSIONS;
		count--;								// First is Pactor Stream, count is now last ax.25 session
	
		while (count)
		{
			if (EXTPORT->ATTACHEDSESSIONS[count] == 0)
			{
				int Paclen, PortPaclen;
				struct DATAMESSAGE Message = {0};
				
				//	Found a free one - use it

				//	See if TNC is OK

				Message.PORT = count;

				ret = PORT->PORTTXCHECKCODE(PORT, Message.PORT);
			
				if ((ret & 0xff00) == 0)
				{
					Bufferptr += sprintf(Bufferptr, "Error - TNC Not Ready\r");
					SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
					return;
				}

				//	GET CIRCUIT TABLE ENTRY FOR OTHER END OF LINK
	
				NewSess = SetupNewSession(Session, Bufferptr);
				if (NewSess == NULL)
					return;

				EXTPORT->ATTACHEDSESSIONS[count] = NewSess;

				NewSess->KAMSESSION = count;
				
				//	Set paclen to lower of incoming and outgoing

				Paclen = Session->SESSPACLEN;	// Incoming PACLEN

				if (Paclen == 0)
					Paclen = 256;				// 0 = 256

				PortPaclen = PORT->PORTPACLEN;

				if (PortPaclen == 0)
					PortPaclen = 256;				// 0 = 256

				if (PortPaclen < Paclen)
					Paclen = PortPaclen;

				NewSess->SESSPACLEN = Paclen;
				Session->SESSPACLEN = Paclen;

				NewSess->L4STATE = 5;
				NewSess->L4CIRCUITTYPE = DOWNLINK + PACTOR;
				NewSess->L4TARGET.PORT = PORT;

				ptr = strtok_s(NULL, " ", &Context);
				sess = count;

				// Replace command tail with original (before conversion to upper case

				Context = Context + (OrigCmdBuffer - COMMANDBUFFER);

				goto checkattachandcall;


				memcpy(Bufferptr, OKMSG, 3);
				Bufferptr += 3;
				SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);

				return;
			}
			count--;
		}

		Bufferptr += sprintf(Bufferptr, "Error - No free streams on this port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}


	Message.PORT = 0;

	ret = PORT->PORTTXCHECKCODE(PORT, Message.PORT);
			
	if ((ret & 0xff00) == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Error - TNC Not Ready\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	// See if "Attach and Call" (for VHF ports)

	ptr = strtok_s(NULL, " ", &Context);

	if (ptr && strcmp(ptr, "S") == 0)
	{
		Session->STAYFLAG = TRUE;
		ptr = strtok_s(NULL, " ", &Context);
	}

	if (ptr)
	{
		// we have another param

		// if it is a single char it is a channel number for vhf attach

		if (strlen(ptr) == 1)
		{
			//	Only Allow Attach VHF from Secure Applications or if PERMITGATEWAY is set

			if (EXTPORT->PERMITGATEWAY == 0 && Session->Secure_Session == 0)
			{
				Bufferptr += sprintf(Bufferptr, "Sorry, you are not allowed to use this port\r");
				SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
				return;
			}

			sess = ptr[0] - '@';

			if (sess < 1 || sess > EXTPORT->MAXHOSTMODESESSIONS)
			{
				Bufferptr += sprintf(Bufferptr, "Error - Invalid Channel\r");
				SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
				return;
			}

			ptr = strtok_s(NULL, " ", &Context);

			if (ptr && strcmp(ptr, "S") == 0)
			{
				Session->STAYFLAG = TRUE;
				ptr = strtok_s(NULL, " ", &Context);
			}
		}
	}

	if (ret & 0x8000)			// Disconnecting
	{
		Bufferptr += sprintf(Bufferptr, "Error - Port in use\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (PORT->PORTINTERLOCK)
	{
		// Check Interlocked Ports

		int p;
		
		p = CHECKINTERLOCK(PORT);
	
		if (p)
		{
			Bufferptr += sprintf(Bufferptr, "Sorry, interlocked port %d is in use\r", p);
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}
	}

	if (EXTPORT->ATTACHEDSESSIONS[sess])
	{
		// In use

		Bufferptr += sprintf(Bufferptr, "Error - Port in use\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	
	//	GET CIRCUIT TABLE ENTRY FOR OTHER END OF LINK
	
	NewSess = SetupNewSession(Session, Bufferptr);
	if (NewSess == NULL)
		return;

	EXTPORT->ATTACHEDSESSIONS[sess] = NewSess;

	NewSess->KAMSESSION = sess;
				
	//	Set paclen to lower of incoming and outgoing

	Paclen = Session->SESSPACLEN;	// Incoming PACLEN

	if (Paclen == 0)
		Paclen = 256;				// 0 = 256

	PortPaclen = PORT->PORTPACLEN;

	if (PortPaclen == 0)
		PortPaclen = 256;				// 0 = 256

	if (PortPaclen < Paclen)
		Paclen = PortPaclen;

	NewSess->SESSPACLEN = Paclen;
	Session->SESSPACLEN = Paclen;
	NewSess->L4STATE = 5;
	NewSess->L4CIRCUITTYPE = DOWNLINK + PACTOR;
	NewSess->L4TARGET.PORT = PORT;

checkattachandcall:

	if (ptr)
	{
		// we have a call to connect to

		char Callstring[80];
		int len;

		Buffer = REPLYBUFFER;
		Buffer->PORT = sess;
		Buffer->PID = 0xf0;

		len = sprintf(Callstring,"C %s", ptr);

		ptr = strtok_s(NULL, " ", &Context);

		while (ptr)				// if any other params (such as digis) copy them
		{
			if (strcmp(ptr, "S") == 0)
			{
				Session->STAYFLAG = TRUE;
			}
			else
				len += sprintf(&Callstring[len], " %s", ptr);
			
			ptr = strtok_s(NULL, " ", &Context);
		}
	
		Callstring[len++] = 13;
		Callstring[len] = 0;
						
		Buffer->LENGTH = len + 8;
		memcpy(Buffer->L2DATA, Callstring, len);
		C_Q_ADD(&PORT->PORTTX_Q, (UINT *)Buffer);
						
		return;
	}

	memcpy(Bufferptr, OKMSG, 3);
	Bufferptr += 3;
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);

	return;
}

//	SYSOP COMMANDS

CMDX COMMANDS[] =
{
	"SAVENODES   ",8, SAVENODES, 0,
	"REBOOT      ",6, REBOOT, 0,
	"RECONFIG    ",8 , RECONFIG, 0,
	"RESTART     ",7,RESTART,0,
	"SENDNODES   ",8,SENDNODES,0,
	"EXTRESTART  ",10, EXTPORTVAL,&DP.EXTRESTART,
	"TXDELAY     ",3, PORTVAL,&DP.PORTCONTROL.PORTTXDELAY,
	"MAXFRAME    ",3, PORTVAL,&DP.PORTCONTROL.PORTWINDOW,
	"RETRIES     ",3, PORTVAL,&DP.PORTCONTROL.PORTN2,
	"FRACK       ",3,PORTVAL,&DP.PORTCONTROL.PORTT1,
	"RESPTIME    ",3,PORTVAL,&DP.PORTCONTROL.PORTT2,
	"PPACLEN     ",3,PORTVAL,&DP.PORTCONTROL.PORTPACLEN,
	"QUALITY     ",3,PORTVAL,&DP.PORTCONTROL.PORTQUALITY,
	"PERSIST     ",2,PORTVAL,&DP.PORTCONTROL.PORTPERSISTANCE,
	"TXTAIL      ",3,PORTVAL,&DP.PORTCONTROL.PORTTAILTIME,
	"XMITOFF     ",7,PORTVAL,&DP.PORTCONTROL.PORTDISABLED,
	"DIGIFLAG    ",5,PORTVAL,&DP.PORTCONTROL.DIGIFLAG,
	"DIGIPORT    ",5,PORTVAL,&DP.PORTCONTROL.DIGIPORT,
	"MAXUSERS    ",4,PORTVAL,&DP.PORTCONTROL.USERS,
	"L3ONLY      ",6,PORTVAL,&DP.PORTCONTROL.PORTL3FLAG,
	"BBSALIAS    ",4,PORTVAL,&DP.PORTCONTROL.PORTBBSFLAG,
	"VALIDCALL   ",5,VALNODES,0,
	"WL2KSYSOP   ",5,WL2KSYSOP,0,
	"STOPPORT    ",4,STOPPORT,0,
	"STARTPORT   ",5,STARTPORT,0,
	"FINDBUFFS   ",4,FINDBUFFS,0,

#ifdef EXCLUDEBITS

	"EXCLUDE     ",4,ListExcludedCalls,0,

#endif

	"FULLDUP     ",4,PORTVAL,&DP.PORTCONTROL.FULLDUPLEX,
	"SOFTDCD     ",4,PORTVAL,&DP.PORTCONTROL.SOFTDCDFLAG,
	"OBSINIT     ",7,SWITCHVAL,&OBSINIT,
	"OBSMIN      ",6,SWITCHVAL,&OBSMIN,
	"NODESINT    ",8,SWITCHVAL,&L3INTERVAL,
	"L3TTL       ",5,SWITCHVAL,&L3LIVES,
	"L4RETRIES   ",5,SWITCHVAL,&L4N2,
	"L4TIMEOUT   ",5,SWITCHVALW,&L4T1,
	"T3          ",2,SWITCHVALW,&T3,
	"NODEIDLETIME",8,SWITCHVALW,&L4LIMIT,
	"LINKEDFLAG  ",10,SWITCHVAL,&LINKEDFLAG,
	"IDINTERVAL  ",5,SWITCHVAL,&IDINTERVAL,
	"MINQUAL     ",7,SWITCHVAL,&MINQUAL,
	"FULLCTEXT   ",6,SWITCHVAL,&FULL_CTEXT,
	"HIDENODES   ",8,SWITCHVAL,&HIDENODES,
	"L4DELAY     ",7,SWITCHVAL,&L4DELAY,
	"L4WINDOW    ",6,SWITCHVAL,&L4DEFAULTWINDOW,
	"BTINTERVAL  ",5,SWITCHVAL,&BTINTERVAL,
	"PASSWORD    ", 8, PWDCMD, 0,

	"************", 12, APPLCMD, 0,
	"************", 12, APPLCMD, 0,
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,		
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,		
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,	
	"************", 12, APPLCMD, 0,			// Apppl 32 is internal Terminal
	"*** LINKED  ",10,LINKCMD,0,
	"CQ          ",2,CQCMD,0,
	"CONNECT     ",1,CMDC00,0,
	"BYE         ",1,BYECMD,0,
	"QUIT        ",1,BYECMD,0,
	"INFO        ",1,CMDI00,0,
	"VERSION     ",1,CMDV00,0,
	"NODES       ",1,CMDN00,0,
	"LINKS       ",1,CMDL00,0,
	"LISTEN      ",3,LISTENCMD,0,
	"L4T1        ",2,CMDT00,0,
	"PORTS       ",1,CMDP00,0,
	"PACLEN      ",3,CMDPAC,0,
	"IDLETIME    ",4,CMDIDLE,0,
	"ROUTES      ",1,CMDR00,0,
	"STATS       ",1,CMDSTATS,0,
	"USERS       ",1,CMDS00,0,
	"UNPROTO     ",2,UNPROTOCMD,0,
	"?           ",1,CMDQUERY,0,
//	"DUMP        ",4,DUMPCMD,0,
	"MHV         ",3,MHCMD,0,
	"MHEARD      ",1,MHCMD,0,
	"APRSMH      ",2,APRSMHCMD,0,
	"ATTACH      ",1,ATTACHCMD,0,
	"RADIO       ",3,RADIOCMD,0,
	"AXRESOLVER  ",3,AXRESOLVER,0,
	"AXMHEARD    ",3,AXMHEARD,0,
	"TELSTATUS   ",3,SHOWTELNET,0,
	"NRR         ",1,NRRCMD,0,
	"PING        ",2,PING,0,
	"AGWSTATUS   ",3,SHOWAGW,0,
	"ARP         ",3,SHOWARP,0,
	"NAT         ",3,SHOWNAT,0,
	"IPROUTE     ",3,SHOWIPROUTE,0,
	"..FLMSG     ",7,FLMSG,0
};

CMDX * CMD = NULL;

int NUMBEROFCOMMANDS = sizeof(COMMANDS)/sizeof(CMDX);

char * ReplyPointer;			// Pointer into reply buffer

int DecodeNodeName(char * NodeName, char * ptr)
{
	// NodeName is TABLE ENTRY WITH AX25 CALL AND ALIAS

	// Copyies 20 byte 20 DECODED NAME IN FORM ALIAS:CALL to ptr
	// Returns significant length of string

	int len;
	char Normcall[10];
	char * alias = &NodeName[7];
	int n = 6;
	char * start = ptr;

	memset(ptr, ' ', 20);

	len = ConvFromAX25(NodeName, Normcall);

	if (*(alias) > ' ')		// Does alias start with a null or a space ?
	{
		while (*(alias) > ' ' && n--)
		{
			*ptr++ = *alias++;
		}
		*ptr++ = ':';
	}

	memcpy(ptr, Normcall, len);
	ptr += len;

	return (ptr - start);
}

char * SetupNodeHeader(struct DATAMESSAGE * Buffer)
{
	char Header[20];
	int len;

	char * ptr = &Buffer->L2DATA[0];

	len = DecodeNodeName(MYCALLWITHALIAS, Header);

	memcpy (ptr, Header, len);
	ptr += len;

	(*ptr++) = HEADERCHAR;
	(*ptr++) = ' ';
	
	return ptr;
}

VOID SendCommandReply(TRANSPORTENTRY * Session, struct DATAMESSAGE * Buffer, int Len)
{
	if (Len == 8)			// Null Packet
	{
		ReleaseBuffer((UINT *)Buffer);
		return;
	}

	Buffer->LENGTH = Len;

	C_Q_ADD(&Session->L4TX_Q, (UINT *)Buffer);

	PostDataAvailable(Session);
}


VOID CommandHandler(TRANSPORTENTRY * Session, struct DATAMESSAGE * Buffer)
{
	// ignore frames with single NULL (Keepalive)

	if (Buffer->LENGTH == 9 && Buffer->L2DATA[0] == 0)
	{
		ReleaseBuffer(Buffer);
		return;
	}

	if (Buffer->LENGTH > 100)
	{
//		Debugprintf("BPQ32 command too long %s", Buffer->L2DATA);
		ReleaseBuffer(Buffer);
		return;
	}

InnerLoop:

	InnerCommandHandler(Session, Buffer);
	
//	See if any more commands in buffer

	if (Session->PARTCMDBUFFER)
	{	
		char * ptr1, * ptr2;
		int len;
		
		Buffer = Session->PARTCMDBUFFER;

		//	Check that message has a CR, if not save buffer and exit

		len = Buffer->LENGTH - 8;
		ptr1 = &Buffer->L2DATA[0];

		ptr2 = memchr(ptr1, 13, len);
	
		if (ptr2 == NULL)
			return;

		Session->PARTCMDBUFFER = NULL;

		goto InnerLoop;
	}
}


VOID InnerCommandHandler(TRANSPORTENTRY * Session, struct DATAMESSAGE * Buffer)
{
	char * ptr1, * ptr2, *ptr3;
	int len, oldlen, newlen, rest, n;
	struct DATAMESSAGE * OldBuffer;
	struct DATAMESSAGE * SaveBuffer;
	char c;

	//	If a partial command is stored, append this data to it.
	
	if (Session->PARTCMDBUFFER)
	{
		len = Buffer->LENGTH - 8;
		ptr1 = &Buffer->L2DATA[0];
	
		OldBuffer = Session->PARTCMDBUFFER;			// Old Data

		if (OldBuffer == Buffer)
		{
			// something has gone horribly wrong

			Session->PARTCMDBUFFER = NULL;
			return;
		}

		oldlen = OldBuffer->LENGTH;

		newlen = len + oldlen;

		if (newlen > 200)
		{
			// Command far too long - ignore previous
	
			OldBuffer->LENGTH = 0;
			oldlen = 8;
		}

		OldBuffer->LENGTH += len;
		memcpy(&OldBuffer->L2DATA[oldlen - 8], Buffer->L2DATA, len);
	
		ReleaseBuffer((UINT *)Buffer);
		
		Buffer = OldBuffer;
		
		Session->PARTCMDBUFFER = NULL;
	}

	//	Check that message has a CR, if not save buffer and exit

	len = Buffer->LENGTH - 8;
	ptr1 = &Buffer->L2DATA[0];

	ptr2 = memchr(ptr1, 13, len);
	
	if (ptr2 == 0)
	{
		//	No newline

		Session->PARTCMDBUFFER = Buffer;
		return;
	}

	ptr2++;

	rest = len - (ptr2 - ptr1);

	if (rest)
	{
		// there are chars beyond the cr in the buffer

		// see if LF after CR
	
		if ((*ptr2) == 10)			// LF
		{
			ptr2++;
			rest--;
		}

		//	Get a new buffer, and copy extra data to it.

		SaveBuffer = (struct DATAMESSAGE *)GetBuff();
	
		if (SaveBuffer)
		{
			SaveBuffer->LENGTH = rest + 8;
			SaveBuffer->PID = 0xf0;
			memcpy(&SaveBuffer->L2DATA[0], ptr2, rest);
			Session->PARTCMDBUFFER = SaveBuffer;
		}
		//`Just ignore if no buffers
	}

	//	GET PACLEN FOR THIS CONNECTION

	CMDPACLEN = Session->SESSPACLEN;

	if (CMDPACLEN == 0)
		CMDPACLEN = PACLEN;		// Use default if no Session PACLEN

	// If sesion is in UNPROTO Mode, send message as a UI message

	if (Session->UNPROTO)
	{
		DIGIMESSAGE Msg;
		int Port = Session->UNPROTO;
		int Len = Buffer->LENGTH - 6;		// CTL, PID and Data

		//	First check for UNPROTO exit - ctrl/z or /ex

		if (Buffer->L2DATA[0] == 26 || (Len == 6 && _memicmp(&Buffer->L2DATA[0], "/ex", 3) == 0))		// CTRL/Z or /ex
		{
			REPLYBUFFER = Buffer;
	
			Session->UNPROTO = 0;
			memset(Session->UADDRESS, 0, 64);

			// SET UP HEADER

			Buffer->PID = 0xf0;
			ptr1 = SetupNodeHeader(Buffer);
			memcpy(ptr1, OKMSG, 3);
			ptr1 += 3;
			SendCommandReply(Session, Buffer, ptr1 - (char *)Buffer);

			return;
		}
		
		memset(&Msg, 0, sizeof(Msg));

		Msg.PORT = Port;
		Msg.CTL = 3;			// UI
		memcpy(Msg.DEST, Session->UADDRESS, 7);
		memcpy(Msg.ORIGIN, Session->L4MYCALL, 7);
		memcpy(Msg.DIGIS, &Session->UADDRESS[7], Session->UAddrLen - 7);
		memcpy(&Msg.PID, &Buffer->PID, Len);
	
		Send_AX_Datagram(&Msg, Len, Port);		// Len is Payload - CTL, PID and Data
	
//		SendUIModeFrame(Session, (PMESSAGE)Buffer, Session->UNPROTO);

		ReleaseBuffer((UINT *)Buffer);			// Not using buffer for reply
	
		return;
	}

	memset(COMMANDBUFFER, 32, 80);		// Clear to spaces
	
	ptr1 = &Buffer->L2DATA[0];
	ptr2 = &COMMANDBUFFER[0];
	ptr3 = &OrigCmdBuffer[0];

	memset(OrigCmdBuffer, 0, 80);
	n = 80;
	
	while (n--)
	{
		c = *(ptr1++) & 0x7f;			// Mask paritu
		
		if (c == 13)
			break;						// CR
	
		*(ptr3++) = c;					// Original Case	
	
		c = toupper(c);
		*(ptr2++) = c;
	}

	
	//	USE INPUT MESSAGE _BUFFER FOR REPLY

	REPLYBUFFER = Buffer;

	// SET UP HEADER

	Buffer->PID = 0xf0;
	ptr1 = SetupNodeHeader(Buffer);

	ReplyPointer = ptr1;

	ALIASINVOKED = 0;				// Clear "Invoked by APPL ALIAS flag"

	DoTheCommand(Session);					// We also call DotheCommand when we need to reprocess - eg for alias handling
}

VOID DoTheCommand(TRANSPORTENTRY * Session)
{
	struct DATAMESSAGE * Buffer = REPLYBUFFER;
	char * ptr1, * ptr2;
	int n;

	ptr1 = &COMMANDBUFFER[0];		//

	n = 10;
	
	while ((*ptr1 == ' ' || *ptr1 == 0) && n--)
		ptr1++;						// STRIP LEADING SPACES and nulls (from keepalive)

	if (n == -1)
	{
		// Null command

		ReleaseBuffer((UINT *)Buffer);
		return;
	}

	ptr2 = ptr1;				// Save


	CMD = &COMMANDS[0];
	n = 0;
	
	for (n = 0; n < NUMBEROFCOMMANDS; n++)
	{
		int CL = CMD->CMDLEN;

		ptr1 = ptr2;

		CMDPTR = CMD;

		if (n == APPL1)		// First APPL command
		{
			APPLMASK = 1;			// FOR APPLICATION ATTACH REQUESTS
			ALIASPTR = &CMDALIAS[0][0];
		}

		// ptr1 is input command

		if (memcmp(CMD->String, ptr1, CL) == 0)
		{
			// Found match so far - check rest
		
			char * ptr2 = &CMD->String[CL];
			
			ptr1 += CL;

			if (*(ptr1) != ' ')
			{
				while(*(ptr1) == *ptr2 && *(ptr1) != ' ')
				{
					ptr1++;
					ptr2++;
				}
			}

			if (*(ptr1) == ' ')
			{
				Session->BADCOMMANDS = 0;			 // RESET ERROR COUNT
	
				// SEE IF SYSOP COMMAND, AND IF SO IF PASSWORD HAS BEEN ENTERED

				if (n < PASSCMD)
				{
					//NEEDS PASSWORD FOR SYSOP COMMANDS
					
					if (Session->PASSWORD  != 0xFFFF)
					{
						ptr1 = ReplyPointer;

						memcpy(ptr1, PASSWORDMSG, LPASSMSG);
						ptr1 += LPASSMSG;

						SendCommandReply(Session, Buffer, ptr1 - (char *)Buffer);
						return;
					}
				}
//				VALNODESFLAG = 0;				//  NOT VALID NODES COMMAND

				ptr1++;						// Skip space

				CMD->CMDPROC(Session, ReplyPointer, ptr1, CMD);
				return;
			}
		}
		
		APPLMASK <<= 1;
		ALIASPTR += ALIASLEN;

		CMD++;
	
	}
	Session->BADCOMMANDS++;

	if (Session->BADCOMMANDS > 6)			// TOO MANY ERRORS
	{
		ReleaseBuffer((UINT *)Buffer);
		Session->STAYFLAG = 0;
		CLOSECURRENTSESSION(Session);
		return;
	}

	ptr1 = ReplyPointer;

	memcpy(ptr1, CMDERRMSG, CMDERRLEN);
	ptr1 += CMDERRLEN;

	SendCommandReply(Session, Buffer, ptr1 - (char *)Buffer);
}


VOID StatsTimer()
{
	struct PORTCONTROL * PORT = PORTTABLE;
	int sum;

	while(PORT)
	{
		sum = PORT->SENDING / 11;
		PORT->AVSENDING = sum;

		sum = (PORT->SENDING + PORT->ACTIVE) /11;
		PORT->AVACTIVE = sum;

		PORT->SENDING = 0;
		PORT->ACTIVE = 0;

		PORT = PORT->PORTPOINTER;
	}
}



extern struct AXIPPORTINFO * Portlist[];

#define TCPConnected 4


VOID AXRESOLVER(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	DISPLAY AXIP Resolver info
	
	int Port = 0, index =0;
	char * ptr, *Context;
	struct PORTCONTROL * PORT = NULL;
	struct AXIPPORTINFO * AXPORT;
	char Normcall[11];
	char Flags[10];
	struct arp_table_entry * arp;

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
		Port = atoi(ptr);

	if (Port)
		PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	AXPORT = Portlist[Port];

	if (AXPORT == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Not an AXIP port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Bufferptr += sprintf(Bufferptr, "AXIP Resolver info for Port %d\r", Port);

	while (index < AXPORT->arp_table_len)
	{
		arp = &AXPORT->arp_table[index];

		Bufferptr = CHECKBUFFER(Session, Bufferptr);

		if (arp->ResolveFlag && arp->error != 0)
		{
				// resolver error - Display Error Code
			sprintf(AXPORT->hostaddr, "Error %d", arp->error);
		}
		else
		{
			if (arp->IPv6)	
				Format_Addr((unsigned char *)&arp->destaddr6.sin6_addr, AXPORT->hostaddr, TRUE);
			else
				Format_Addr((unsigned char *)&arp->destaddr.sin_addr, AXPORT->hostaddr, FALSE);
		}
				
		ConvFromAX25(arp->callsign, Normcall);

		Flags[0] = 0;
		
		if (arp->BCFlag)
			strcat(Flags, "B ");

		if (arp->TCPState == TCPConnected)
			strcat(Flags, "C ");

		if (arp->AutoAdded)
			strcat(Flags, "A");
								
		if (arp->port == arp->SourcePort)
			Bufferptr += sprintf(Bufferptr,"%.10s = %.64s %d = %-.30s %s\r",
				Normcall,
				arp->hostname,
				arp->port,
				AXPORT->hostaddr,
				Flags);

		else
			Bufferptr += sprintf(Bufferptr,"%.10s = %.64s %d<%d = %-.30s %s\r",
				Normcall,
				arp->hostname,
				arp->port,
				arp->SourcePort,
				AXPORT->hostaddr,
				Flags);

		index++;
	}
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID AXMHEARD(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	DISPLAY AXIP Mheard info
	
	int Port = 0, index =0;
	char * ptr, *Context;
	struct PORTCONTROL * PORT = NULL;
	struct AXIPPORTINFO * AXPORT;
	int n = MHENTRIES;
	char Normcall[11];

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
		Port = atoi(ptr);

	if (Port)
		PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	AXPORT = Portlist[Port];

	if (AXPORT == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Not an AXIP port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Bufferptr += sprintf(Bufferptr, "AXIP Mheard for Port %d\r", Port);
	
	while (index < MaxMHEntries)
	{	
		if (AXPORT->MHTable[index].proto != 0)
		{
			char Addr[80];

			Bufferptr = CHECKBUFFER(Session, Bufferptr);
				
			Format_Addr((unsigned char *)&AXPORT->MHTable[index].ipaddr6, Addr, AXPORT->MHTable[index].IPv6);

			Normcall[ConvFromAX25(AXPORT->MHTable[index].callsign, Normcall)] = 0;

			Bufferptr += sprintf(Bufferptr, "%-10s%-15s %c %-6d %-25s%c\r", Normcall,
						Addr,
						AXPORT->MHTable[index].proto,
						AXPORT->MHTable[index].port,
						asctime(gmtime( &AXPORT->MHTable[index].LastHeard )),
						(AXPORT->MHTable[index].Keepalive == 0) ? ' ' : 'K');

			Bufferptr[-2] =  ' ';			// Clear CR returned by asctime
		}

		index++;
	}
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

#pragma pack()

struct TNCINFO * TNCInfo[34];	

VOID SHOWTELNET(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	DISPLAY Telnet Server Status Mheard
	
	int Port = 0, index =0;
	char * ptr, *Context;
	struct PORTCONTROL * PORT = NULL;
	int txlen = 0, n;
	struct TNCINFO * TNC;
	char msg[80];
	struct ConnectionInfo * sockptr;
	int i;
	char CMS[] = "CMS Disabled";

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
		Port = atoi(ptr);

	if (Port)
		PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == NULL)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	TNC = TNCInfo[Port];

	if (TNC == NULL || TNC->Hardware != H_TELNET)
	{
		Bufferptr += sprintf(Bufferptr, "Not a Telnet port\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (TNC->TCPInfo->CMS)
		if (TNC->TCPInfo->CMSOK)
			strcpy(CMS, "CMS Ok");
		else
			strcpy(CMS, "No CMS");
	
	Bufferptr += sprintf(Bufferptr, "Telnet Status for Port %d %s\r", Port, CMS);

	for (n = 1; n <= TNC->TCPInfo->CurrentSockets; n++)
	{
		sockptr=TNC->Streams[n].ConnectionInfo;

		Bufferptr = CHECKBUFFER(Session, Bufferptr);

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
					sockptr->UserPointer->UserName,sockptr->Callsign,sockptr->BPQStream);
			}
		}
		Bufferptr += sprintf(Bufferptr, "%s\r", msg);
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

extern char WL2KCall[10];
extern char WL2KLoc[7];

BOOL GetWL2KSYSOPInfo(char * Call, char * _REPLYBUFFER);
BOOL UpdateWL2KSYSOPInfo(char * Call, char * SQL);

VOID WL2KSYSOP(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	char _REPLYBUFFER[1000] = "";

	char LastUpdated[100];
	char Name[100] = "";
	char Addr1[100] = "";
	char Addr2[100] = "";
	char City[100] = "";
	char State[100] = "";
	char Country[100] = "";
	char PostCode[100] = "";
	char Email[100] = "";
	char Website[100] = "";
	char Phone[100] = "";
	char Data[100] = "";
	char LOC[100] = "";
	BOOL Exists = TRUE;
	time_t LastUpdateSecs = 0;
	char * ptr1, * ptr2;

	SOCKET sock;
			
	int Len;
	char Message[2048];

	if (WL2KCall[0] < 33)
	{
		Bufferptr += sprintf(Bufferptr, "Winlink reporting is not configured\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}


	if (GetWL2KSYSOPInfo(WL2KCall, _REPLYBUFFER) == 0)
	{
		Bufferptr += sprintf(Bufferptr, "Failed to connect to WL2K Database\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (strstr(_REPLYBUFFER, "\"ErrorMessage\":"))
		Exists = FALSE;

	GetJSONValue(_REPLYBUFFER, "\"SysopName\":", Name);	
	GetJSONValue(_REPLYBUFFER, "\"StreetAddress1\":", Addr1);	
	GetJSONValue(_REPLYBUFFER, "\"StreetAddress2\":", Addr2);	
	GetJSONValue(_REPLYBUFFER, "\"City\":", City);	
	GetJSONValue(_REPLYBUFFER, "\"State\":", State);	
	GetJSONValue(_REPLYBUFFER, "\"Country\":", Country);	
	GetJSONValue(_REPLYBUFFER, "\"PostalCode\":", PostCode);	
	GetJSONValue(_REPLYBUFFER, "\"Email\":", Email);	
	GetJSONValue(_REPLYBUFFER, "\"Website\":", Website);	
	GetJSONValue(_REPLYBUFFER, "\"Phones\":", Phone);	
	GetJSONValue(_REPLYBUFFER, "\"Comments\":", Data);	
	GetJSONValue(_REPLYBUFFER, "\"GridSquare\":", LOC);	
	GetJSONValue(_REPLYBUFFER, "\"Timestamp\":", LastUpdated);	

	ptr1 = strchr(LastUpdated, '(');

	if (ptr1)
	{
		ptr2 = strchr(++ptr1, ')');

		if (ptr2)
		{
			*(ptr2 - 3) = 0; // remove millisecs
			LastUpdateSecs = atoi(ptr1);

			FormatTime2(LastUpdated, LastUpdateSecs);
		}
	}

	if (_memicmp(CmdTail, "SET ", 4) == 0)
	{
		if (Exists)
		{
			Bufferptr += sprintf(Bufferptr, "Record already exists in WL2K Database\r");
			SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
			return;
		}

	// Set New Values. Any other params are values to set, separated by |

//		ptr1 = strtok_s(&CmdTail[4], ",", &Context);

//		if (ptr1 == NULL)
//			goto DoReplace;

//		strcpy(Name, ptr1);

//DoReplace:

		Len = sprintf(Message,
				"\"Callsign\":\"%s\","
				"\"GridSquare\":\"%s\","
				"\"SysopName\":\"%s\","
				"\"StreetAddress1\":\"%s\","
				"\"StreetAddress2\":\"%s\","
				"\"City\":\"%s\","
				"\"State\":\"%s\","
				"\"Country\":\"%s\","
				"\"PostalCode\":\"%s\","
				"\"Email\":\"%s\","
				"\"Phones\":\"%s\","
				"\"Website\":\"%s\","
				"\"Comments\":\"%s\",",

			WL2KCall, WL2KLoc, Name, Addr1, Addr2, City, State, Country, PostCode, Email, Phone, Website, Data);
		
		Debugprintf("Sending %s", Message);

		sock = OpenWL2KHTTPSock();

		if (sock)
			SendHTTPRequest(sock, "server.winlink.org", 8085, 
				"/sysop/add", Message, Len, NULL);
	
		closesocket(sock);

		Bufferptr += sprintf(Bufferptr, "Database Updated\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (Exists)
	{
		Bufferptr += sprintf(Bufferptr, "\rWL2K SYSOP Info for %s\r", WL2KCall);
		Bufferptr += sprintf(Bufferptr, "Grid Square: %s\r", LOC);
		Bufferptr += sprintf(Bufferptr, "Name: %s\r", Name);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "Addr Line 1: %s\r", Addr1);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "Addr Line 2: %s\r", Addr2);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "City: %s\r", City);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "State: %s\r", State);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "Country: %s\r", Country);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "PostCode: %s\r", PostCode);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "Email Address: %s\r", Email);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "Website: %s\r", Website);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "Phone: %s\r", Phone);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "Additional Data: %s\r", Data);
		Bufferptr = CHECKBUFFER(Session, Bufferptr);		// ENSURE ROOM
		Bufferptr += sprintf(Bufferptr, "Last Updated: %s\r", LastUpdated);
	}
	else
		Bufferptr += sprintf(Bufferptr, "No SYSOP record for %s\r", WL2KCall);


	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;
}

VOID CloseKISSPort(struct PORTCONTROL * PortVector);
int OpenConnection(struct PORTCONTROL * PortVector, int port);

VOID STOPPORT(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	char _REPLYBUFFER[1000] = "";
	char * ptr, * Context;

	int portno;
	struct PORTCONTROL * PORT = PORTTABLE;
	int n = NUMBEROFPORTS;

	// Get port number

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
	{
		portno = atoi (ptr);

		if (portno)
		{
			while (n--)
			{
				if (PORT->PORTNUMBER == portno)
				{
					struct KISSINFO * KISS;

					if (PORT->PORTTYPE != 0)
					{
						Bufferptr += sprintf(Bufferptr, "Not a KISS Port\r");
						SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
						return;
					}

					KISS = (struct KISSINFO *) PORT;

					if (KISS->FIRSTPORT != KISS)
					{
						Bufferptr += sprintf(Bufferptr, "Not first port of a Multidrop Set\r");
						SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
						return;
					}

					CloseKISSPort(PORT); 
					Bufferptr += sprintf(Bufferptr, "Port Closed\r");
					SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);

					return;
				}
				PORT = PORT->PORTPOINTER;
			}
		}
	}

	// Bad port

	strcpy(Bufferptr, BADPORT);
	Bufferptr += strlen(BADPORT);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;
}

VOID STARTPORT(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	char _REPLYBUFFER[1000] = "";
	char * ptr, * Context;

	int portno;
	struct PORTCONTROL * PORT = PORTTABLE;
	int n = NUMBEROFPORTS;

	// Get port number

	ptr = strtok_s(CmdTail, " ", &Context);

	if (ptr)
	{
		portno = atoi (ptr);

		if (portno)
		{
			while (n--)
			{
				if (PORT->PORTNUMBER == portno)
				{
					struct KISSINFO * KISS;

					if (PORT->PORTTYPE != 0)
					{
						Bufferptr += sprintf(Bufferptr, "Not a KISS Port\r");
						SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
						return;
					}

					KISS = (struct KISSINFO *) PORT;

					if (KISS->FIRSTPORT != KISS)
					{
						Bufferptr += sprintf(Bufferptr, "Not first port of a Multidrop Set\r");
						SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
						return;
					}

					if (OpenConnection(PORT, PORT->IOBASE))
						Bufferptr += sprintf(Bufferptr, "Port Opened\r");
					else
						Bufferptr += sprintf(Bufferptr, "Port Open Failed\r");
						
					SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
					return;
				}
				PORT = PORT->PORTPOINTER;
			}
		}
	}

	// Bad port

	strcpy(Bufferptr, BADPORT);
	Bufferptr += strlen(BADPORT);
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	return;
}


VOID FINDBUFFS(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	FindLostBuffers();

#ifdef WIN32
	Bufferptr += sprintf(Bufferptr, "Lost buffer info dumped to Debugview\r");
#else
	Bufferptr += sprintf(Bufferptr, "Lost buffer info dumped to syslog\r");
#endif
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID FLMSG(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * UserCMD)
{
	// Telnet Connection from FLMSG
	CLOSECURRENTSESSION(Session);		// Kills any crosslink, plus local link
	ReleaseBuffer((UINT *)REPLYBUFFER);
}

BOOL CheckExcludeList(UCHAR * Call)
{
	UCHAR * ptr1 = ExcludeList;

	while (*ptr1)
	{
		if (memcmp(Call, ptr1, 6) == 0)
			return FALSE;

		ptr1 += 7;
	}

	return TRUE;
}


void ListExcludedCalls(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{

	UCHAR * ptr = ExcludeList;
	char Normcall[10] = "";
	UCHAR AXCall[8] = "";

	if (*CmdTail == ' ')
		goto DISPLIST;

	if (*CmdTail == 'Z')
	{
		// CLEAR LIST

		memset(ExcludeList, 0, 70);
		goto DISPLIST;
	}

	ConvToAX25(CmdTail, AXCall);

	if (strlen(ExcludeList) < 70)
		strcat(ExcludeList, AXCall);
	
DISPLIST:

	while (*ptr)
	{
		Normcall[ConvFromAX25(ptr, Normcall)] = 0;
		Bufferptr += sprintf(Bufferptr, "%s ", Normcall);
		ptr += 7;
	}

	*(Bufferptr++) = '\r';
	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}





