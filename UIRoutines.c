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
// UI Handling Routines

#include "BPQMail.h"


char UIDEST[10] = "FBB";
char UIMAIL[10] = "MAIL";
char AXDEST[7];
char AXMAIL[7];
static char MAILMYCALL[7];

#pragma pack(1)

UINT UIPortMask = 0;
BOOL UIEnabled[33];
BOOL UIMF[33];
BOOL UIHDDR[33];
BOOL UINull[33];
char * UIDigi[33];
char * UIDigiAX[33];		// ax.25 version of digistring
int UIDigiLen[33];			// Length of AX string



#pragma pack()

PMESSAGEX DG_Q;					// Queue of messages to be sent to node

struct SEM DGSemaphore = {0, 0}; // For locking access to DG_Q;

VOID UnQueueRaw(UINT Param);
unsigned long _beginthread(void(*start_address),
				unsigned stack_size, int Param);

static VOID Send_AX_Datagram(UCHAR * Msg, DWORD Len, UCHAR Port, UCHAR * HWADDR, BOOL Queue);


VOID SetupUIInterface()
{
	int i, NumPorts = GetNumberofPorts();
#ifndef LINBPQ
	struct _EXCEPTION_POINTERS exinfo;
#endif

	ConvToAX25(GetApplCall(BBSApplNum), MAILMYCALL);
	ConvToAX25(UIDEST, AXDEST);
	ConvToAX25(UIMAIL, AXMAIL);

	UIPortMask = 0;

	for (i = 1; i <= NumPorts; i++)
	{
		if (UIEnabled[i])
		{
			char DigiString[100], * DigiLeft;

			UIPortMask |= 1 << (i-1);
			UIDigiLen[i] = 0;

			if (UIDigi[i])
			{
				UIDigiAX[i] = zalloc(100);
				strcpy(DigiString, UIDigi[i]);
				DigiLeft = strlop(DigiString,',');

				while(DigiString[0])
				{
					ConvToAX25(DigiString, &UIDigiAX[i][UIDigiLen[i]]);
					UIDigiLen[i] += 7;

					if (DigiLeft)
					{
						memmove(DigiString, DigiLeft, strlen(DigiLeft) + 1);
						DigiLeft = strlop(DigiString,',');
					}
					else
						DigiString[0] = 0;
				}
			}
		}
	}

	_beginthread(UnQueueRaw, 0, 0);

	if (EnableUI)
#ifdef LINBPQ
	SendLatestUI(0);
#else
	__try
	{
		SendLatestUI(0);
	}
	My__except_Routine("SendLatestUI");
#endif

}

VOID Free_UI()
{
	int i;
	PMESSAGEX AXMSG;

	for (i = 1; i <= 32; i++)
	{
		if (UIDigi[i])
		{
			free(UIDigi[i]);
			UIDigi[i] = NULL;
		}

		if (UIDigiAX[i])
		{
			free(UIDigiAX[i]);
			UIDigiAX[i] = NULL;
		}
	}

	if (DG_Q)
	{
		AXMSG = DG_Q;
		DG_Q = AXMSG->CHAIN;
		free(AXMSG);
	}
}

VOID QueueRaw(int Port, PMESSAGEX AXMSG, int Len)
{
	PMESSAGEX AXCopy = zalloc(400);
	PMESSAGEX AXNext;

	AXMSG->PORT = Port;
	AXMSG->LENGTH = Len;
	AXMSG->CHAIN = 0;					// Clear chain in new buffer

	memcpy(AXCopy, AXMSG, Len + 10);

	GetSemaphore(&DGSemaphore, 0);

	if (DG_Q == 0)						// Empty
	{
		DG_Q = AXCopy;
		FreeSemaphore(&DGSemaphore);
		return;
	}

	AXNext = DG_Q;

	while (AXNext->CHAIN)
		AXNext = AXNext->CHAIN;			// Chain to end of queue

	AXNext->CHAIN = AXCopy;				// New one on end

	FreeSemaphore(&DGSemaphore);
}

VOID SendMsgUI(struct MsgInfo * Msg)
{
	char msg[200];
	int len, i;
	int Mask = UIPortMask;
	int NumPorts = GetNumberofPorts();

	//12345 B 2053 TEST@ALL F6FBB 920325 This is the subject

	struct tm *tm = gmtime(&Msg->datecreated);	
	
	len = sprintf_s(msg, sizeof(msg),"%-6d %c %6d %-13s %-6s %02d%02d%02d %s\r",
		Msg->number, Msg->type, Msg->length, Msg->to,
		Msg->from, tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, Msg->title);

	for (i=1; i <= NumPorts; i++)
	{
		if ((Mask & 1) && UIHDDR[i])
			Send_AX_Datagram(msg, len, i, AXDEST, TRUE);
		
		Mask>>=1;
	}
}

VOID SendHeaders(int Number, int Port)
{
	// Send headers in response to a resync request

	char msg[256];
	unsigned  len=0;
	struct tm *tm;
	struct MsgInfo * Msg;

	//12345 B 2053 TEST@ALL F6FBB 920325 This is the subject

	while (Number <= LatestMsg)
	{
		Msg = FindMessageByNumber(Number);
	
		if (Msg)
		{
			if (len > (200 - strlen(Msg->title)))
			{
				Send_AX_Datagram(msg, len, Port, AXDEST, FALSE);
				len=0;
			}

			tm = gmtime(&Msg->datecreated);	
	
			len += sprintf(&msg[len], "%-6d %c %6d %-13s %-6s %02d%02d%02d %s\r",
				Msg->number, Msg->type, Msg->length, Msg->to,
				Msg->from, tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, Msg->title);
		}
		else
		{
			if (len > 230)
			{
				Send_AX_Datagram(msg, len, Port, AXDEST, FALSE);
				len=0;
			}
			len += sprintf(&msg[len], "%-6d #\r", Number);
		}

		Number++;
	}

	Send_AX_Datagram(msg, len, Port, AXDEST, FALSE);

}
VOID SendDummyUI(int num)
{
	char msg[100];
	int len, i;
	int Mask = UIPortMask;
	int NumPorts = GetNumberofPorts()
;
	len = sprintf_s(msg, sizeof(msg),"%-6d #\r", num);

	for (i=1; i <= NumPorts; i++)
	{
		if (Mask & 1)
			Send_AX_Datagram(msg, len, i, AXDEST, TRUE);
		
		Mask>>=1;
	}
}

VOID SendLatestUI(int Port)
{
	char msg[20];
	int len, i;
	int Mask = UIPortMask;
	int NumPorts = GetNumberofPorts();
	
	len = sprintf_s(msg, sizeof(msg),"%-6d !!\r", LatestMsg);

	if (Port)
	{
		Send_AX_Datagram(msg, len, Port, AXDEST, FALSE);
		return;
	}

	for (i=1; i <= NumPorts; i++)
	{
		if ((Mask & 1) && UIHDDR[i])
			Send_AX_Datagram(msg, len, i, AXDEST, TRUE);
		
		Mask>>=1;
	}
}

static VOID Send_AX_Datagram(UCHAR * Msg, DWORD Len, UCHAR Port, UCHAR * HWADDR, BOOL Queue)
{
	MESSAGEX AXMSG;

	PMESSAGEX AXPTR = &AXMSG;

	// Block includes the Msg Header (7 bytes), Len Does not!

	memcpy(AXPTR->DEST, HWADDR, 7);
	memcpy(AXPTR->ORIGIN, MAILMYCALL, 7);
	AXPTR->DEST[6] &= 0x7e;			// Clear End of Call
	AXPTR->DEST[6] |= 0x80;			// set Command Bit

	if (UIDigi[Port])
	{
		// This port has a digi string

		int DigiLen = UIDigiLen[Port];
		UCHAR * ptr;

		memcpy(&AXPTR->CTL, UIDigiAX[Port], DigiLen);
		
		ptr = (UCHAR *)AXPTR;
		ptr += DigiLen;
		AXPTR = (PMESSAGEX)ptr;

		Len += DigiLen;

	}
	AXPTR->ORIGIN[6] |= 1;			// Set End of Call
	AXPTR->CTL = 3;		//UI
	AXPTR->PID = 0xf0;
	memcpy(AXPTR->DATA, Msg, Len);

	if (Queue)
		QueueRaw(Port, &AXMSG, Len + 16);
	else
		SendRaw(Port, (char *)&AXMSG.DEST, Len + 16);

	return;

}

VOID UnQueueRaw(UINT Param)
{
	PMESSAGEX AXMSG;

	while (TRUE)
	{
		GetSemaphore(&DGSemaphore, 0);

		if (DG_Q)
		{
			AXMSG = DG_Q;
			DG_Q = AXMSG->CHAIN;

			SendRaw(AXMSG->PORT, (char *)&AXMSG->DEST, AXMSG->LENGTH);
			free(AXMSG);
		}
	
		FreeSemaphore(&DGSemaphore);

		Sleep(5000);
	}
}

VOID ProcessUItoMe(char * msg, int len)
{
	msg[len] = 0;
	return;
}

VOID ProcessUItoFBB(char * msg, int len, int Port)
{
	// ? 0000006464
	// The first 8 digits are the hexadecimal number of the requested start of the list
	// (here 00002EE0 -> 12000) and the last two digits are the sum of the four bytes anded with FF (0E).

	int Number, Sum, Sent = 0;
	char cksum[3];
	
	if (msg[0] == '?')
	{
		memcpy(cksum, &msg[10], 2);
		msg[10]=0;

		sscanf(&msg[1], "%X", &Number);
		sscanf(cksum, "%X", &Sum);

		if (Number >= LatestMsg)
		{
			SendLatestUI(Port);
			return;
		}
		
		SendHeaders(Number+1, Port);
	}
	
	return;
}

UCHAR * AdjustForDigis(PMESSAGEX * buff, int * len)
{
	PMESSAGEX buff1 = *(buff);
	UCHAR * ptr, * ptr1;

	if ((buff1->ORIGIN[6] & 1) == 1)
	{
		// End of Call Set
	
		return 0;				// No Digis
	}

	ptr1 = &buff1->ORIGIN[6];		// End of add 
	ptr = (UCHAR *)*buff;

	while((*ptr1 & 1) == 0)			// End of address bit
	{
		ptr1 += 7;
		ptr+= 7;
	}

	*buff = (PMESSAGEX)ptr;
	return (&buff1->CTL);		// Start of Digi String
}
VOID SeeifBBSUIFrame(PMESSAGEX buff, int len)
{
	UCHAR * Digis;
	UCHAR From[7], To[7];
	int Port = buff->PORT;
	
	if (Port > 128)
		return;									// Only look at received frames

	memcpy(From, buff->ORIGIN, 7);				// Save Origin and Dest before adjucting for Digis
	memcpy(To, buff->DEST, 7);

	Digis = AdjustForDigis(&buff, &len);

	if (Digis)
	{
		// Make sure all are actioned
	
	DigiLoop:
	
		if ((Digis[6] & 0x80) == 0)
			return;								// Not repeated
		
		if ((Digis[6] & 1) == 0)				// Not end of list
		{
			Digis +=7;
			goto DigiLoop;
		}
	}

	if (buff->CTL != 3)
		return;

	if (buff->PID != 0xf0)
		return;

//	if (memcmp(buff->ORIGIN, MAILMYCALL,6) == 0)		// From me?
//		if (buff->ORIGIN[6] == (MAILMYCALL[6] | 1))		// Set End of Call
//			return;

	From[6] &= 0x7e;
	To[6] &= 0x7e;

	if (memcmp(To, MAILMYCALL, 7) == 0)
	{
		ProcessUItoFBB(buff->DATA, len-23, Port);
		return;
	}

	if (memcmp(To, AXDEST, 7) == 0)
	{
		ProcessUItoFBB(buff->DATA, len-23, Port);
		return;
	}

	len++;

	return;
}

//	ConvToAX25(MYNODECALL, MAILMYCALL);
//				len=ConvFromAX25(Routes->NEIGHBOUR_DIGI1,Portcall);
//			Portcall[len]=0;

char MailForHeader[] = "Mail For:";

char MailForExpanded[100];

VOID ExpandMailFor()
{
	char * OldP = MailForText;
	char * NewP = MailForExpanded;
	char * ptr, * pptr;
	int len;
	char Dollar[] = "\\";
	char CR[] = "\r";

	ptr = strchr(OldP, '\\');

	while (ptr)
	{
		len = ptr - OldP;		// Chars before Backslash
		memcpy(NewP, OldP, len);
		NewP += len;

		switch (*++ptr)
		{
		case 'r': // Inserts a carriage return.
		case 'R': // Inserts a carriage return.

			pptr = CR;
			break;

		default:

			pptr = Dollar;		// Just Copy Backslash
		}

		len = strlen(pptr);
		memcpy(NewP, pptr, len);
		NewP += len;

		OldP = ++ptr;
		ptr = strchr(OldP, '\\');
	}

	strcpy(NewP, OldP);
}

	
VOID SendMailFor(char * Msg, BOOL HaveCalls)
{
	int Mask = UIPortMask;
	int NumPorts = GetNumberofPorts();
	int i;

	if (!HaveCalls)
		strcat(Msg, "None ");

	Sleep(1000);
	
	for (i=1; i <= NumPorts; i++)
	{
		if (Mask & 1)
		{
			if (UIMF[i] && (HaveCalls || UINull[i]))
			{
				Send_AX_Datagram(Msg, strlen(Msg) - 1, i, AXDEST, TRUE);
			}
		}
		Mask>>=1;
	}
}

VOID SendMailForThread(VOID * Param)
{
	struct UserInfo * user;
	char MailForMessage[256] = "";
	BOOL HaveMailFor;
	struct UserInfo * ptr = NULL;
	int i, Unread;

	while (MailForInterval)
	{
		ExpandMailFor();

		if (MailForText[0])				// User supplied header
			strcpy(MailForMessage, MailForExpanded);
		else
			strcpy(MailForMessage, MailForHeader);

		HaveMailFor = FALSE;

		for (i=1; i <= NumberofUsers; i++)
		{
			user = UserRecPtr[i];

			CountMessagesTo(user, &Unread);
	
			if (Unread)
			{
				if (strlen(MailForMessage) > 240)
				{
					SendMailFor(MailForMessage, TRUE);

					if (MailForText[0])				// User supplied header
						strcpy(MailForMessage, MailForExpanded);
					else
						strcpy(MailForMessage, MailForHeader);
				}
				strcat(MailForMessage, user->Call);
				strcat(MailForMessage, " ");
				HaveMailFor = TRUE;
			}
		}

		SendMailFor(MailForMessage, HaveMailFor);
		Sleep(MailForInterval * 60000);
	}
}








/*
20:09:00R GM8BPQ-10>FBB Port=1 <UI C>:
103    !!
20:10:06R GM8BPQ-10>FBB Port=1 <UI C>:
19-Jul 21:08 <<< Mailbox GM8BPQ Skigersta >>> 2 active messages.
Messages for
 ALL

20:11:11R GM8BPQ-10>FBB Port=1 <UI C>:
104    P      5 G8BPQ         GM8BPQ 090719 ***
20:12:17R GM8BPQ-10>FBB Port=1 <UI C>:
105    B      5 ALL           GM8BPQ 090719 test

12345 B 2053 TEST@ALL F6FBB 920325 This is the subject



20:13:23R GM8BPQ-10>FBB Port=1 <UI C>:
? 0000006464

20:15:34R GM8BPQ-10>FBB Port=1 <UI C>:
105    !!
20:15:45T GM8BPQ-10>MAIL Port=2 <UI C>:

20:16:40R GM8BPQ-10>FBB Port=1 <UI C>:
19-Jul 21:15 <<< Mailbox GM8BPQ Skigersta >>> 4 active messages.
Messages for
 ALL G8BPQ
20:17:46R GM8BPQ-10>FBB Port=1 <UI C>:
106    P      5 GM8BPQ        GM8BPQ 090719 ***
20:20:54R GM8BPQ-10>FBB Port=1 <UI C>:
? 0000006464
20:21:05T GM8BPQ-10>FBB Port=2 <UI C>:
? 0000006464
*/
