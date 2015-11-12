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

#include "BPQMail.h"

int APIENTRY ChangeSessionIdletime(int Stream, int idletime);
struct MsgInfo * GetMsgFromNumber(int msgno);
BOOL ForwardMessagetoFile(struct MsgInfo * Msg, HANDLE Handle);

static char seps[] = " \t\r";

VOID DoAuthCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context)
{
	int AuthInt = 0;
	
	if (!(user->flags & F_SYSOP))
	{
		nodeprintf(conn, "AUTH can only be used by SYSOPs\r");
		SendPrompt(conn, user);
		return;
	}

	if (Arg1)
		AuthInt = atoi(Arg1);
	else
	{
		nodeprintf(conn, "AUTH Code missing\r");
		SendPrompt(conn, user);
		return;
	}

	if (user->Temp->LastAuthCode == AuthInt)
	{
		nodeprintf(conn, "AUTH Code already used\r");
		SendPrompt(conn, user);
		return;
	}

	if (Arg1 && CheckOneTimePassword(Arg1, user->pass))
	{
		conn->sysop = TRUE;
		nodeprintf(conn, "Ok\r");
		user->Temp->LastAuthCode = atoi(Arg1);
	}
	else
		nodeprintf(conn, "AUTH Failed\r");

	SendPrompt(conn, user);
	return;
}

VOID DoEditUserCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context)
{
	char Line[200] = "User Flags:";
	struct UserInfo * EUser = user;

	if (conn->sysop == 0)
	{
		nodeprintf(conn, "Edit User needs SYSOP status\r");
		SendPrompt(conn, user);
		return;
	}

	if (Arg1 == 0 || _stricmp(Arg1, "HELP") == 0)
	{
		nodeprintf(conn, "EDITUSER CALLSIGN to Display\r");
		nodeprintf(conn, "EDITUSER CALLSIGN FLAG1 FLAG2 ...  to set, -FLAG1 -FLAG2 ...  to clear\r");
		nodeprintf(conn, "EDITUSER: Flags are: EXC(luded) EXP(ert) SYSOP BBS PMS EMAIL HOLD RMS(Express User)\r");

		SendPrompt(conn, user);
		return;
	}

	EUser = LookupCall(Arg1);

	if (EUser == 0)
	{
		nodeprintf(conn, "User %s not found\r", Arg1);
		SendPrompt(conn, user);
		return;
	}

	Arg1 = strtok_s(NULL, seps, &Context);

	if (Arg1 == NULL)
		goto UDisplay;
					
	// A set of flags to change +Flag or -Flag
		
	while(Arg1 && strlen(Arg1) > 2)
	{
		_strupr(Arg1);

		if (strstr(Arg1, "EXC"))
			if (Arg1[0] != '-') EUser->flags |= F_Excluded; else EUser->flags &= ~F_Excluded;
		if (strstr(Arg1, "EXP"))
			if (Arg1[0] != '-') EUser->flags |= F_Expert; else EUser->flags &= ~F_Expert;
		if (strstr(Arg1, "SYS"))
			if (Arg1[0] != '-') EUser->flags |= F_SYSOP; else EUser->flags &= ~F_SYSOP;
		if (strstr(Arg1, "BBS"))
			if (Arg1[0] != '-') EUser->flags |= F_BBS; else EUser->flags &= ~F_BBS;
		if (strstr(Arg1, "PMS"))
			if (Arg1[0] != '-') EUser->flags |= F_PMS; else EUser->flags &= ~F_PMS;
		if (strstr(Arg1, "EMAIL"))
			if (Arg1[0] != '-') EUser->flags |= F_EMAIL; else EUser->flags &= ~F_EMAIL;
		if (strstr(Arg1, "HOLD"))
			if (Arg1[0] != '-') EUser->flags |= F_HOLDMAIL; else EUser->flags &= ~F_HOLDMAIL;
		if (strstr(Arg1, "RMS"))
			if (Arg1[0] != '-') EUser->flags |= F_Temp_B2_BBS; else EUser->flags &= ~F_Temp_B2_BBS;

		Arg1 = strtok_s(NULL, seps, &Context);
	}

	SaveUserDatabase();

	// Drop through to display
UDisplay:

	if (EUser->flags & F_Excluded)
		strcat(Line, " EXC");

	if (EUser->flags & F_Expert)
		strcat(Line, " EXP");

	if (EUser->flags & F_SYSOP)
		strcat(Line, " SYSOP");

	if (EUser->flags & F_BBS)
		strcat(Line, " BBS");

	if (EUser->flags & F_PMS)
		strcat(Line, " PMS");

	if (EUser->flags & F_EMAIL)
		strcat(Line, " EMAIL");

	if (EUser->flags & F_HOLDMAIL)
		strcat(Line, " HOLD");

	if (EUser->flags & F_Temp_B2_BBS)
		strcat(Line, " RMS");

	
	strcat(Line, "\r");	
	nodeprintf(conn, Line);
	
	SendPrompt(conn, user);
	return;
}

VOID DoShowRMSCmd(CIRCUIT * conn, struct UserInfo * inuser, char * Arg1, char * Context)
{
	int i, s;
	char FWLine[10000] = "";
	struct UserInfo * user;
	char RMSCall[20];

	if (conn->sysop == 0)
	{
		nodeprintf(conn, "Command needs SYSOP status\r");
		SendPrompt(conn, inuser);
		return;
	}
			
	for (i = 0; i <= NumberofUsers; i++)
	{
		user = UserRecPtr[i];

		if (user->flags & F_POLLRMS)
		{
			if (user->RMSSSIDBits == 0) user->RMSSSIDBits = 1;

			for (s = 0; s < 16; s++)
			{
				if (user->RMSSSIDBits & (1 << s))
				{
					strcat(FWLine, " ");
					if (s)
					{
						sprintf(RMSCall, "%s-%d", user->Call, s);
						strcat(FWLine, RMSCall);
					}
					else
						strcat(FWLine, user->Call);
							
				}
			}
		}
	}
			
	strcat(FWLine, "\r");	

	nodeprintf(conn, FWLine);
}



VOID DoPollRMSCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context)
{
	char RMSLine[200];
	char RMSCall[10];
	struct UserInfo * RMSUser = user;
	int s;
Loop:
	if (Arg1)
	{
		// Update	
		if (_stricmp(Arg1, "Enable") == 0)
		{
			RMSUser->flags |= F_POLLRMS;
			Arg1 = strtok_s(NULL, seps, &Context);
			goto Display;
		}
		else if (_stricmp(Arg1, "Disable") == 0)
		{
			RMSUser->flags &= ~F_POLLRMS;
			Arg1 = strtok_s(NULL, seps, &Context);
			goto Display;
		}
		else if (strlen(Arg1) > 2)
		{
			// Callsign - if SYSOP, following commands apply to selected user

			if (conn->sysop == 0)
			{
				nodeprintf(conn, "Changing RMS Poll params for another user needs SYSOP status\r");
				SendPrompt(conn, user);
				return;
			}
			RMSUser = LookupCall(Arg1);

			if (RMSUser == NULL)
			{
				nodeprintf(conn, "User %s not found\r", Arg1);
				SendPrompt(conn, user);
				return;
			}

			Arg1 = strtok_s(NULL, seps, &Context);

			if (Arg1 == NULL)
				goto Display;
				
			if (_stricmp(Arg1, "Enable") == 0 || _stricmp(Arg1, "Disable") == 0 || (strlen(Arg1) < 3))
				goto Loop;

			goto Display;
		}
	
		// A list of SSID's to poll

		RMSUser->RMSSSIDBits = 0;

		while(Arg1 && strlen(Arg1) < 3)
		{
			s = atoi(Arg1);
			if (s < 16)
				RMSUser->RMSSSIDBits |= (1 << (s));

			Arg1 = strtok_s(NULL, seps, &Context);
		}
	}

	// Drop through to display

Display:
	strcpy(RMSLine, "Polling for calls");

	if (RMSUser->flags & F_POLLRMS)
	{
		if (RMSUser->RMSSSIDBits == 0) RMSUser->RMSSSIDBits = 1;
		{
			for (s = 0; s < 16; s++)
			{
				if (RMSUser->RMSSSIDBits & (1 << s))
				{
					strcat(RMSLine, " ");
					if (s)
					{
						sprintf(RMSCall, "%s-%d", RMSUser->Call, s);
						strcat(RMSLine, RMSCall);
					}
					else
						strcat(RMSLine, RMSUser->Call);
						
				}
			}
		}
		strcat(RMSLine, "\r");	
		nodeprintf(conn, RMSLine);
	}
	else
		nodeprintf(conn, "RMS Polling for %s disabled\r", RMSUser->Call);

	if (Arg1)
		goto Loop;
	
	SaveUserDatabase();
	SendPrompt(conn, user);
}

VOID DoSetIdleTime(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context)
{
	int IdleTime;

	if (Arg1)
		IdleTime = atoi(Arg1);
	else
	{
		nodeprintf(conn, "Format is IDLETIME nnn\r");
		SendPrompt(conn, user);
		return;
	}

	if (IdleTime < 60 || IdleTime > 900)
	{
		nodeprintf(conn, "Time out of range (60 to 900 seconds)\r");
		SendPrompt(conn, user);
		return;
	}

	if (conn->BPQStream >= 0)
		ChangeSessionIdletime(conn->BPQStream, IdleTime);
	else
	{
		nodeprintf(conn, "Can't set Idle Time on Console)\r");
		SendPrompt(conn, user);
		return;
	}

	nodeprintf(conn, "Idle Tine set to %d\r", IdleTime);
	SendPrompt(conn, user);
	return;
}

VOID DoExportCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context)
{
	int msgno;
	char * FN;
	FILE * Handle = NULL;
	struct MsgInfo * Msg;


	if (conn->sysop == 0)
	{
		nodeprintf(conn, "EXPORT command needs SYSOP status\r");
		SendPrompt(conn, user);
		return;
	}

	if (Arg1 == 0 || _stricmp(Arg1, "HELP") == 0)
	{
		nodeprintf(conn, "EXPORT nnn FILENAME - Export Message nnn to file FILENAME\r");
		SendPrompt(conn, user);
		return;
	}

	msgno = atoi(Arg1);
	
	FN = strtok_s(NULL, " \r", &Context);

	if (FN == NULL)
	{
		nodeprintf(conn, "Missong Filename");
		SendPrompt(conn, user);
		return;
	}


	Msg = GetMsgFromNumber(msgno);

	if (Msg == NULL)
	{
		nodeprintf(conn, "Message %d not found\r", msgno);		
		SendPrompt(conn, user);
		return;
	}

	conn->BBSFlags |= BBS;

	Handle = fopen(FN, "ab");

	if (Handle == NULL)
	{
		nodeprintf(conn, "File %s could not be opened\r", FN);		
		SendPrompt(conn, user);
		return;
	}

//	SetFilePointer(Handle, 0, 0, FILE_END);

	ForwardMessagetoFile(Msg, Handle);

	fclose(Handle);

	nodeprintf(conn, "%Message %d Exported\r", msgno);
	SendPrompt(conn, user);

	return;

}


VOID DoImportCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context)
{
	int count;

	if (conn->sysop == 0)
	{
		nodeprintf(conn, "IMPORT command needs SYSOP status\r");
		SendPrompt(conn, user);
		return;
	}

	if (Arg1 == 0 || _stricmp(Arg1, "HELP") == 0)
	{
		nodeprintf(conn, "IMPORT FILENAME - Import Messages from file FILENAME\r");
		SendPrompt(conn, user);
		return;
	}

	conn->BBSFlags |= BBS;

	count = ImportMessages(conn, Arg1, TRUE);

	conn->BBSFlags &= ~BBS;
	conn->Flags = 0;

	nodeprintf(conn, "%d Messages Processed\r", count);
	SendPrompt(conn, user);

	return;

}





VOID DoFwdCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context)
{
	char Line[200];
	struct UserInfo * FwdBBS;
	struct	BBSForwardingInfo * ForwardingInfo;

	if (conn->sysop == 0)
	{
		nodeprintf(conn, "FWD command needs SYSOP status\r");
		SendPrompt(conn, user);
		return;
	}

	if (Arg1 == 0 || _stricmp(Arg1, "HELP") == 0)
	{
		nodeprintf(conn, "FWD BBSCALL - Display settings\r");
		nodeprintf(conn, "FWD BBSCALL interval - Set forwarding interval\r");
		nodeprintf(conn, "FWD BBSCALL REV interval - Set reverse forwarding interval\r");
		nodeprintf(conn, "FWD BBSCALL +- Flags (Flags are EN(able) RE(verse Poll) SE(Send Immediately)\r");
		nodeprintf(conn, "FWD BBSCALL NOW - Start a forwarding cycle now\r");
		nodeprintf(conn, "FWD QUEUE - List BBS's with queued messages\r");
		nodeprintf(conn, "FWD NOW can specify a Connect Script to use, overriding the configured script.\r");
		nodeprintf(conn, "Elements are separated by | chars. eg FWD RMS NOW ATT 7|C GM8BPQ-10\r");

		SendPrompt(conn, user);
		return;
	}


	if (_stricmp(Arg1, "QUEUE") == 0)
	{
		struct UserInfo * xuser;
		int Msgs;

		for (xuser = BBSChain; xuser; xuser = xuser->BBSNext)
		{
			Msgs = CountMessagestoForward(xuser);

			if (Msgs)
				nodeprintf(conn, "%s %d Msgs\r", xuser->Call, Msgs);

		}
		SendPrompt(conn, user);
		return;
	}

	FwdBBS = LookupCall(Arg1);

	if (FwdBBS == 0 || (FwdBBS->flags & F_BBS) == 0)
	{
		nodeprintf(conn, "BBS %s not found\r", Arg1);
		SendPrompt(conn, user);
		return;
	}

	ForwardingInfo = FwdBBS->ForwardingInfo;

	Arg1 = strtok_s(NULL, seps, &Context);

	if (Arg1 == NULL)
		goto FDisplay;

	if (_stricmp(Arg1, "NOW") == 0)
	{
		char ** Value = NULL;

		if (ForwardingInfo->Forwarding)
		{
			BBSputs(conn, "Already Connected\r");
			SendPrompt(conn, user);
			return;
		}

		while (Context[0] == ' ')
			Context++;

		if (Context)
			strlop(Context, 13);

		if (Context && Context[0])
		{
			// Temp Connect Script to use

			char * ptr1;
			char * MultiString = NULL;
			const char * ptr;
			int Count = 0;

			Value = zalloc(4);				// always NULL entry on end even if no values
			Value[0] = NULL;

			ptr = Context;

			while (ptr && strlen(ptr))
			{
				ptr1 = strchr(ptr, '|');
			
				if (ptr1)
					*(ptr1++) = 0;

				Value = realloc(Value, (Count+2)*4);
			
				Value[Count++] = _strdup(ptr);
			
				ptr = ptr1;
			}
	
			Value[Count] = NULL;
		}

		StartForwarding(FwdBBS->BBSNumber, Value);
		
		if (ForwardingInfo->Forwarding)
			nodeprintf(conn, "Forwarding Started\r");
		else
			nodeprintf(conn, "Start Forwarding failed\r");
		
		SendPrompt(conn, user);
		return;
	}

	if (_stricmp(Arg1, "REV") == 0)
	{
		Arg1 = strtok_s(NULL, seps, &Context);
		ForwardingInfo->RevFwdInterval = atoi(Arg1);
	}
	else
	{
		while(Arg1)
		{
			_strupr(Arg1);

			if (isdigits(Arg1))
				ForwardingInfo->FwdInterval = atoi(Arg1);
			else if (strstr(Arg1, "EN"))
				if (Arg1[0] == '-') ForwardingInfo->Enabled = FALSE; else ForwardingInfo->Enabled = TRUE;
			else if (strstr(Arg1, "RE"))
				if (Arg1[0] == '-') ForwardingInfo->ReverseFlag = FALSE; else ForwardingInfo->ReverseFlag = TRUE;
			else if (strstr(Arg1, "SE"))
				if (Arg1[0] == '-') ForwardingInfo->SendNew = FALSE; else ForwardingInfo->SendNew = TRUE;

			Arg1 = strtok_s(NULL, seps, &Context);
		}
	}

	SaveConfig(ConfigName);
	GetConfig(ConfigName);

FDisplay:

	sprintf(Line, "%s Fwd Interval %d Rev Interval %d Fwd %s, Reverse Poll %s, Send Immediately %s\r",
		FwdBBS->Call, ForwardingInfo->FwdInterval, ForwardingInfo->RevFwdInterval,
		(ForwardingInfo->Enabled)? "Enabled" : "Disabled", (ForwardingInfo->ReverseFlag) ? "Enabled": "Disabled",
		(ForwardingInfo->SendNew) ? "Enabled": "Disabled");

	nodeprintf(conn, Line);
	SendPrompt(conn, user);

	return;
}