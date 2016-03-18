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
//	Housekeeping Module

#include "BPQMail.h"

UCHAR * APIENTRY GetBPQDirectory();

int LogAge = 7;

BOOL DeletetoRecycleBin = FALSE;
BOOL SuppressMaintEmail = FALSE;
BOOL SaveRegDuringMaint = FALSE;
BOOL OverrideUnsent = FALSE;
BOOL SendNonDeliveryMsgs = TRUE;
VOID UpdateWP();

int PR = 30;
int PUR = 30;
int PF = 30;
int PNF = 30;
int BF = 30;
int BNF = 30;
//int AP;
//int AB;
int NTSD = 30;
int NTSF = 30;
int NTSU = 30;

char LTFROMString[2048];
char LTTOString[2048];
char LTATString[2048];

struct Override ** LTFROM;
struct Override ** LTTO;
struct Override ** LTAT;

int DeleteLogFiles();

VOID SendNonDeliveryMessage(struct MsgInfo * OldMsg, BOOL Forwarded, int Age);
int CreateWPMessage();

int LastHouseKeepingTime;

int LastTrafficTime;

void DeletetoRecycle(char * FN)
{
#ifdef WIN32
	SHFILEOPSTRUCT FileOp;

	FileOp.hwnd = NULL;
	FileOp.wFunc = FO_DELETE;
	FileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR | FOF_ALLOWUNDO;
	FileOp.pFrom = FN;
	FileOp.pTo = NULL;

	SHFileOperation(&FileOp);
#endif
}

VOID FreeOverride(struct Override ** Hddr)
{
	struct Override ** Save;
	
	if (Hddr)
	{
		Save = Hddr;
		while(Hddr[0])
		{
			free(Hddr[0]->Call);
			free(Hddr[0]);
			Hddr++;
		}
		
		free(Save);
	}

}

VOID FreeOverrides()
{
	FreeOverride(LTFROM);
	FreeOverride(LTTO);
	FreeOverride(LTAT);
}

VOID * GetOverrides(config_setting_t * group, char * ValueName)
{
	char * ptr1;
	char * MultiString = NULL;
	char * ptr;
	int Count = 0;
	struct Override ** Value;
	char * Val;

	config_setting_t *setting;

	Value = zalloc(4);				// always NULL entry on end even if no values
	Value[0] = NULL;

	setting = config_setting_get_member (group, ValueName);
	
	if (setting)
	{
		ptr =  (char *)config_setting_get_string (setting);
	
		while (ptr && strlen(ptr))
		{
			ptr1 = strchr(ptr, '|');
			
			if (ptr1)
				*(ptr1++) = 0;

			Value = realloc(Value, (Count+2)*4);
			Value[Count] = zalloc(sizeof(struct Override));
			Val = strlop(ptr, ',');
			if (Val == NULL)
				break;

			Value[Count]->Call = _strupr(_strdup(ptr));
			Value[Count++]->Days = atoi(Val);
			ptr = ptr1;
		}
	}

	Value[Count] = NULL;
	return Value;
}

VOID * RegGetOverrides(HKEY hKey, char * ValueName)
{
#ifdef LINBPQ
	return NULL;
#else
	int retCode,Type,Vallen;
	char * MultiString;
	int ptr, len;
	int Count = 0;
	struct Override ** Value;
	char * Val;


	Value = zalloc(4);				// always NULL entry on end even if no values

	Value[0] = NULL;

	Vallen=0;

	retCode = RegQueryValueEx(hKey, ValueName, 0, (ULONG *)&Type, NULL, (ULONG *)&Vallen);

	if ((retCode != 0)  || (Vallen == 0)) 
		return FALSE;

	MultiString = malloc(Vallen);

	retCode = RegQueryValueEx(hKey, ValueName, 0,			
			(ULONG *)&Type,(UCHAR *)MultiString,(ULONG *)&Vallen);

	ptr=0;

	while (MultiString[ptr])
	{
		len=strlen(&MultiString[ptr]);

		Value = realloc(Value, (Count+2)*4);
		Value[Count] = zalloc(sizeof(struct Override));
		Val = strlop(&MultiString[ptr], ',');
		if (Val == NULL)
			break;

		Value[Count]->Call = _strupr(_strdup(&MultiString[ptr]));
		Value[Count++]->Days = atoi(Val);
		ptr+= (len + 1);
	}

	Value[Count] = NULL;

	free(MultiString);

	return Value;
#endif
}

int Removed;
int Killed;
int BIDSRemoved;

#ifndef LINBPQ

INT_PTR CALLBACK HKDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	int Command;
		
	switch (message)
	{
	case WM_INITDIALOG:

		SetDlgItemInt(hDlg, IDC_REMOVED, Removed, FALSE);
		SetDlgItemInt(hDlg, IDC_KILLED, Killed, FALSE);
		SetDlgItemInt(hDlg, IDC_LIVE, NumberofMessages - Killed, FALSE);
		SetDlgItemInt(hDlg, IDC_TOTAL, NumberofMessages, FALSE);
		SetDlgItemInt(hDlg, IDC_BIDSREMOVED, BIDSRemoved, FALSE);
		SetDlgItemInt(hDlg, IDC_BIDSLEFT, NumberofBIDs, FALSE);

		return (INT_PTR)TRUE;

	case WM_COMMAND:

		Command = LOWORD(wParam);

		switch (Command)
		{
		case IDOK:
		case IDCANCEL:

			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;

		}
		break;
	}
	
	return 0;
}
#endif

VOID DoHouseKeeping(BOOL Manual)
{
	time_t NOW;

	CreateUserReport();

	UpdateWP();

	DeleteLogFiles();

	RemoveKilledMessages();
	ExpireMessages();
	
	GetSemaphore(&AllocSemaphore, 0);
	ExpireBIDs();
	FreeSemaphore(&AllocSemaphore);

	if (LatestMsg > MaxMsgno)
	{
		GetSemaphore(&MsgNoSemaphore, 0);
		GetSemaphore(&AllocSemaphore, 0);

		Renumber_Messages();
	
		FreeSemaphore(&AllocSemaphore);
		FreeSemaphore(&MsgNoSemaphore);
	}

	if (!SuppressMaintEmail)
		MailHousekeepingResults();
	
	LastHouseKeepingTime = NOW = time(NULL);

#ifndef LINBPQ

	if (Manual)
		DialogBox(hInst, MAKEINTRESOURCE(IDD_MAINTRESULTS), hWnd, HKDialogProc);

#endif

	if (SendWP)
		CreateWPMessage();

	return;
}

VOID ExpireMessages()
{
	struct MsgInfo * Msg;
	int n;
	int PRLimit;
	int PURLimit;
	int PFLimit;
	int PNFLimit;
	int BFLimit;
	int BNFLimit;
	int BLimit;
	int NTSDLimit;
	int NTSULimit;
	int NTSFLimit;

	struct Override ** Calls;

	int now=time(NULL);
	int Future = now + (7 * 86400);

	Killed = 0;

	PRLimit = now - PR*86400;
	PURLimit = now - PUR*86400;
	PFLimit = now - PF*86400;
	PNFLimit = now - PNF*86400;
	BFLimit = now - BF*86400;
	BNFLimit = now - BNF*86400;

	if (NTSU == 0)
	{
		// Assume all unset

		NTSD = 30;
		NTSU = 30;
		NTSF = 30;
	}

	NTSDLimit = now - NTSD*86400;
	NTSULimit = now - NTSU*86400;
	NTSFLimit = now - NTSF*86400;

	for (n = 1; n <= NumberofMessages; n++)
	{
		Msg = MsgHddrPtr[n];

		// If from the future, Kill it

		if (Msg->datecreated > Future)
		{
			KillMsg(Msg);
			continue;
		}

		switch (Msg->type)
		{
		case 'P':

			switch (Msg->status)
			{
			case 'N':
			case 'H':

				// Is it unforwarded or unread?

				if (memcmp(Msg->fbbs, zeros, NBMASK) == 0)
				{
					if (Msg->datecreated < PURLimit)
					{
						if (SendNonDeliveryMsgs) 
							SendNonDeliveryMessage(Msg, TRUE, PUR);

						KillMsg(Msg);
					}
				}
				else
				{
					if (Msg->datecreated < PNFLimit)
					{
						if (SendNonDeliveryMsgs) 
							SendNonDeliveryMessage(Msg, FALSE, PNF);

						KillMsg(Msg);
					}
				}
				continue;	
	
			case 'F':

				if (Msg->datechanged < PFLimit) KillMsg(Msg);

				continue;	

			case 'Y':

				if (Msg->datechanged < PRLimit) KillMsg(Msg);

				continue;

			default:

				continue;

			}			
			
		case 'T':

			switch (Msg->status)
			{	
			case 'F':

				if (Msg->datechanged < NTSFLimit)
					KillMsg(Msg);

				continue;	

			case 'D':

				if (Msg->datechanged < NTSDLimit)
					KillMsg(Msg);

				continue;

			default:

				if (Msg->datecreated < NTSULimit)
				{
					if (SendNonDeliveryMsgs) 
						SendNonDeliveryMessage(Msg, TRUE, PUR);

					KillMsg(Msg);
				}

				continue;

			}			

		case 'B':

			BLimit = BF;
			BNFLimit = now - BNF*86400;

			// Check FROM Overrides

			if (LTFROM)
			{
				Calls = LTFROM;

				while(Calls[0])
				{
					if (strcmp(Calls[0]->Call, Msg->from) == 0)
					{
						BLimit = Calls[0]->Days;
						goto gotit;
					}
					Calls++;
				}
			}

			// Check TO Overrides

			if (LTTO)
			{
				Calls = LTTO;

				while(Calls[0])
				{
					if (strcmp(Calls[0]->Call, Msg->to) == 0)
					{
						BLimit = Calls[0]->Days;
						goto gotit;
					}
					Calls++;
				}
			}

			// Check AT Overrides

			if (LTAT)
			{
				Calls = LTAT;

				while(Calls[0])
				{
					if (strcmp(Calls[0]->Call, Msg->via) == 0)
					{
						BLimit = Calls[0]->Days;
						goto gotit;
					}
					Calls++;
				}
			}

		gotit:

			BFLimit = now - BLimit*86400;

			if (OverrideUnsent)
				if (BLimit != BF)		// Have we an override?
					BNFLimit = BFLimit;

			switch (Msg->status)
			{
			case '$':
			case 'N':
			case ' ':
			case 'H':


				if (Msg->datecreated < BNFLimit)
					KillMsg(Msg);
				break;	

			case 'F':
			case 'Y':

				if (Msg->datecreated < BFLimit) 
					KillMsg(Msg);
				break;	
			}			
		}
	}
}


VOID KillMsg(struct MsgInfo * Msg)
{
	FlagAsKilled(Msg);
	Killed++;
}

BOOL RemoveKilledMessages()
{
	struct MsgInfo * Msg;
	struct MsgInfo ** NewMsgHddrPtr;
	char MsgFile[MAX_PATH];
	int i, n;

	Removed = 0;

	GetSemaphore(&MsgNoSemaphore, 0);
	GetSemaphore(&AllocSemaphore, 0);

	FirstMessageIndextoForward = 0;

	NewMsgHddrPtr = zalloc((NumberofMessages+1) * 4);
	NewMsgHddrPtr[0] = MsgHddrPtr[0];		// Copy Control Record

	i = 0;

	for (n = 1; n <= NumberofMessages; n++)
	{
		Msg = MsgHddrPtr[n];

		if (Msg->status == 'K')
		{
			sprintf_s(MsgFile, sizeof(MsgFile), "%s/m_%06d.mes%c", MailDir, Msg->number, 0);
			if (DeletetoRecycleBin)
				DeletetoRecycle(MsgFile);
			else
				DeleteFile(MsgFile);

			MsgnotoMsg[Msg->number] = NULL;	
			free(Msg);

			Removed++;
		}
		else
		{
			NewMsgHddrPtr[++i] = Msg;
			if (memcmp(Msg->fbbs, zeros, NBMASK) != 0)
			{
				if (FirstMessageIndextoForward == 0)
					FirstMessageIndextoForward = i;
			}
		}
	}

	NumberofMessages = i;
	NewMsgHddrPtr[0]->number = i;

	if (FirstMessageIndextoForward == 0)
		FirstMessageIndextoForward = NumberofMessages;

	free(MsgHddrPtr);

	MsgHddrPtr = NewMsgHddrPtr;

	FreeSemaphore(&MsgNoSemaphore);
	FreeSemaphore(&AllocSemaphore);

	SaveMessageDatabase();

	return TRUE;

}

#define MESSAGE_NUMBER_MAX 65536

VOID Renumber_Messages()
{
	int * NewNumber = (int *)0;
	struct MsgInfo * Msg;
	struct UserInfo * user = NULL;
	char OldMsgFile[MAX_PATH];
	char NewMsgFile[MAX_PATH];
	int j, lastmsg, result;

	int i, n, s;

	s = sizeof(int)* MESSAGE_NUMBER_MAX;

	NewNumber = malloc(s);

	if (!NewNumber) return;

    memset(NewNumber, 0, s);

	for (i = 0; i < 100000; i++)
	{
		MsgnotoMsg[i] = NULL;
	}

	i = 0;		// New Message Number

	for (n = 1; n <= NumberofMessages; n++)
	{
		Msg = MsgHddrPtr[n];

		NewNumber[Msg->number] = ++i;		// Save so we can update users' last listed count

		// New will always be >= old unless somethnig hasgon horribly wrong,
		// so can rename in place without risk of losing a message

		if (Msg->number < i)
		{
#ifndef LINBPQ
			MessageBox(MainWnd, "Invalid message number detected, quitting", "BPQMailChat", MB_OK);
#else
			Debugprintf("Invalid message number detected, quitting");
#endif
			SaveMessageDatabase();
			if (NewNumber) free(NewNumber);

			return;
		}

		if (Msg->number != i)
		{
			sprintf(OldMsgFile, "%s/m_%06d.mes", MailDir, Msg->number);
			sprintf(NewMsgFile, "%s/m_%06d.mes", MailDir, i);
			result = rename(OldMsgFile, NewMsgFile);
			if (result)
			{
				char Errmsg[100];
				sprintf(Errmsg, "Could not rename message no %d to %d, quitting", Msg->number, i);
#ifndef LINBPQ
				MessageBox(MainWnd,Errmsg , "BPQMailChat", MB_OK);
#else
				Debugprintf(Errmsg);
#endif
				SaveMessageDatabase();
				if (NewNumber) free(NewNumber);

				return;
			}
			Msg->number = i;
			MsgnotoMsg[i] = Msg;
		}
		
	}

	for (n = 0; n <= NumberofUsers; n++)
	{
		user = UserRecPtr[n];
		lastmsg = user->lastmsg;

		if (lastmsg <= 0)
			user->lastmsg = 0;
		else
		{
			j = NewNumber[lastmsg];

			if (j == 0)
			{
				// Last listed has gone. Find next above

				while(++lastmsg < 65536)
				{
					if (NewNumber[lastmsg] != 0)
					{
						user->lastmsg = NewNumber[lastmsg];
						break;
					}
				}

				// Not found, so use latest

				user->lastmsg = i;
				break;
			}
			user->lastmsg = NewNumber[lastmsg];
		}
	}

	MsgHddrPtr[0]->length = LatestMsg = i;

	SaveMessageDatabase();
	SaveUserDatabase();

	if (NewNumber) free(NewNumber);

	return;

}

BOOL ExpireBIDs()
{
	BIDRec * BID;
	BIDRec ** NewBIDRecPtr;
	unsigned short now=LOWORD(time(NULL)/86400);

	int i, n;

	NewBIDRecPtr = zalloc((NumberofBIDs+1) * 4);
	NewBIDRecPtr[0] = BIDRecPtr[0];		// Copy Control Record

	i = 0;

	for (n = 1; n <= NumberofBIDs; n++)
	{
		BID = BIDRecPtr[n];

//		Debugprintf("%d %d", BID->u.timestamp, now - BID->u.timestamp);

		if ((now - BID->u.timestamp) < BidLifetime)
			NewBIDRecPtr[++i] = BID;
	}

	BIDSRemoved = NumberofBIDs - i;

	NumberofBIDs = i;
	NewBIDRecPtr[0]->u.msgno = i;

	free(BIDRecPtr);

	BIDRecPtr = NewBIDRecPtr;

	SaveBIDDatabase();

	return TRUE;

}

VOID MailHousekeepingResults()
{
	int Length=0;
	char * MailBuffer = malloc(10000);

	Length += sprintf(&MailBuffer[Length], "Killed Messsages Removed %d\r\n", Removed);
	Length += sprintf(&MailBuffer[Length], "Messages Killed          %d\r\n", Killed);
	Length += sprintf(&MailBuffer[Length], "Live Messages            %d\r\n", NumberofMessages - Killed);
	Length += sprintf(&MailBuffer[Length], "Total Messages           %d\r\n", NumberofMessages);
	Length += sprintf(&MailBuffer[Length], "BIDs Removed             %d\r\n", BIDSRemoved);
	Length += sprintf(&MailBuffer[Length], "BIDs Left                %d\r\n", NumberofBIDs);

	SendMessageToSYSOP("Housekeeping Results", MailBuffer, Length);
}

#ifdef WIN32

int DeleteLogFiles()
{
   WIN32_FIND_DATA ffd;

   char szDir[MAX_PATH];
   char File[MAX_PATH];
   HANDLE hFind = INVALID_HANDLE_VALUE;
   DWORD dwError=0;
   LARGE_INTEGER ft;
   time_t now = time(NULL);
   int Age;

   // Prepare string for use with FindFile functions.  First, copy the
   // string to a buffer, then append '\*' to the directory name.

   strcpy(szDir, GetBPQDirectory());
   strcat(szDir, "\\logs\\Log_*.txt");

   // Find the first file in the directory.

   hFind = FindFirstFile(szDir, &ffd);

   if (INVALID_HANDLE_VALUE == hFind) 
   {
      return dwError;
   } 
   
   // List all the files in the directory with some info about them.

   do
   {
      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
         OutputDebugString(ffd.cFileName);
      }
      else
      {
         ft.HighPart = ffd.ftCreationTime.dwHighDateTime;
         ft.LowPart = ffd.ftCreationTime.dwLowDateTime;

		 ft.QuadPart -=  116444736000000000;
		 ft.QuadPart /= 10000000;

		 Age = (now - ft.LowPart) / 86400; 

		 if (Age > LogAge)
		 {
			 sprintf(File, "%s/logs/%s%c", GetBPQDirectory(), ffd.cFileName, 0);
			 if (DeletetoRecycleBin)
				DeletetoRecycle(File);
			 else
				 DeleteFile(File);
		 }
      }
   }
   while (FindNextFile(hFind, &ffd) != 0);
 
   dwError = GetLastError();

   FindClose(hFind);
   return dwError;
}

#else

#include <dirent.h>

int Filter(const struct dirent * dir)
{
	return memcmp(dir->d_name, "log", 3) == 0 && strstr(dir->d_name, ".txt");
}

int DeleteLogFiles()
{
	struct dirent **namelist;
    int n;
	struct stat STAT;
	time_t now = time(NULL);
	int Age = 0, res;
	char FN[256];
     	
    n = scandir("logs", &namelist, Filter, alphasort);

	if (n < 0) 
		perror("scandir");
	else  
	{ 
		while(n--)
		{
			sprintf(FN, "logs/%s", namelist[n]->d_name);
			if (stat(FN, &STAT) == 0)
			{
				Age = (now - STAT.st_mtime) / 86400;
				
				if (Age > LogAge)
				{
					printf("Deleting  %s\n", FN);
					unlink(FN);
				}
			}
			free(namelist[n]);
		}
		free(namelist);
    }
	return 0;
}
#endif


VOID SendNonDeliveryMessage(struct MsgInfo * OldMsg, BOOL Unread, int Age)
{
	struct MsgInfo * Msg = AllocateMsgRecord();
	BIDRec * BIDRec;
	char MailBuffer[1000];
	char MsgFile[MAX_PATH];
	FILE * hFile;
	int WriteLen=0;
	char From[100];
	char * Via;
	struct UserInfo * FromUser;

	// Try to create a from Address. ( ? check RMS)

	strcpy(From, OldMsg->from);

	if (strcmp(From, "SYSTEM") == 0)
		return;							// Don't send non-deliverys SYSTEM messages

	FromUser = LookupCall(OldMsg->from);

	if (FromUser)
	{
		if (FromUser->HomeBBS[0])
			sprintf(From, "%s@%s", OldMsg->from, FromUser->HomeBBS);
		else
			sprintf(From, "%s@%s", OldMsg->from, BBSName);
	}
	else
	{
		WPRecP WP = LookupWP(OldMsg->from);

		if (WP)
			sprintf(From, "%s@%s", OldMsg->from, WP->first_homebbs);
	}

	GetSemaphore(&MsgNoSemaphore, 0);
	Msg->number = ++LatestMsg;
	MsgnotoMsg[Msg->number] = Msg;

	FreeSemaphore(&MsgNoSemaphore);
 
	strcpy(Msg->from, SYSOPCall);

	Via = strlop(From, '@');

	strcpy(Msg->to, From);
	if (Via)
		strcpy(Msg->via, Via);

	if (strcmp(From, "RMS:") == 0)
	{
		strcpy(Msg->to, "RMS");
		strcpy(Msg->via, OldMsg->emailfrom);
	}

	if (strcmp(From, "smtp:") == 0)
	{
		Msg->to[0] = 0;
		strcpy(Msg->via, OldMsg->emailfrom);
	}

	if (Msg->to[0] == 0)
		return;

	strcpy(Msg->title, "Non-delivery Notification");
	
	if (Unread)
		Msg->length = sprintf(MailBuffer, "Your Message ID %s Subject %s to %s has not been read for %d days.\r\nMessage had been deleted.\r\n", OldMsg->bid, OldMsg->title, OldMsg->to, Age);
	else
		Msg->length = sprintf(MailBuffer, "Your Message ID %s Subject %s to %s could not be delivered in %d days.\r\nMessage had been deleted.\r\n", OldMsg->bid, OldMsg->title, OldMsg->to, Age);


	Msg->type = 'P';
	Msg->status = 'N';
	Msg->datereceived = Msg->datechanged = Msg->datecreated = time(NULL);

	sprintf_s(Msg->bid, sizeof(Msg->bid), "%d_%s", LatestMsg, BBSName);

	BIDRec = AllocateBIDRecord();
	strcpy(BIDRec->BID, Msg->bid);
	BIDRec->mode = Msg->type;
	BIDRec->u.msgno = LOWORD(Msg->number);
	BIDRec->u.timestamp = LOWORD(time(NULL)/86400);

	sprintf_s(MsgFile, sizeof(MsgFile), "%s/m_%06d.mes", MailDir, Msg->number);
	
	hFile = fopen(MsgFile, "wb");

	if (hFile)
	{
		fwrite(MailBuffer, 1, Msg->length, hFile);
		fclose(hFile);
	}

	MatchMessagetoBBSList(Msg, NULL);
}

VOID CreateBBSTrafficReport()
{
	struct UserInfo * User;
	int i, n;
	char Line[200];
	int len;
	char File[MAX_PATH];
	FILE * hFile;
	time_t NOW = time(NULL);

	int	ConnectsIn;
	int ConnectsOut;
//	int MsgsReceived;
//	int MsgsSent;
//	int MsgsRejectedIn;
//	int MsgsRejectedOut;
//	int BytesForwardedIn;
//	int BytesForwardedOut;
	int TotMsgsReceived[4] = {0,0,0,0};
	int TotMsgsSent[4] = {0,0,0,0};

	int TotBytesForwardedIn[4] = {0,0,0,0};
	int TotBytesForwardedOut[4] = {0,0,0,0};

	char MsgsIn[80];
	char MsgsOut[80];
	char BytesIn[80];
	char BytesOut[80];
	char RejIn[80];
	char RejOut[80];

	struct tm tm;
	struct tm last;

	memcpy(&tm, gmtime(&NOW), sizeof(tm));	
	memcpy(&last, gmtime((const time_t *)&LastTrafficTime), sizeof(tm));

	sprintf(File, "%s/Traffic_%02d%02d%02d.txt", BaseDir, tm.tm_year-100, tm.tm_mon+1, tm.tm_mday);
	
	hFile = fopen(File, "wb");

	if (hFile == NULL)
	{
		Debugprintf("Failed to create traffic.txt");
		return;
	}

	len = sprintf(Line, "    Traffic Report for %s From: %04d/%02d/%02d %02d:%02dz To: %04d/%02d/%02d %02d:%02dz\r\n",
		BBSName, last.tm_year+1900, last.tm_mon+1, last.tm_mday, last.tm_hour, last.tm_min,
		tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min);

	fwrite(Line, 1, len, hFile);

	len = sprintf(Line, "    Call    Connects  Connects    Messages        Messages           Bytes           Bytes        Rejected        Rejected\r\n");
	fwrite(Line, 1, len, hFile);
	len = sprintf(Line, "               In        Out      Rxed(P/B/T)       Sent             Rxed            Sent            In              Out\r\n\r\n");

	fwrite(Line, 1, len, hFile);
		
	for (i=1; i <= NumberofUsers; i++)
	{
		User = UserRecPtr[i];

		ConnectsIn = User->Total.ConnectsIn - User->Last.ConnectsIn;
		ConnectsOut = User->Total.ConnectsOut - User->Last.ConnectsOut;

/*
		MsgsReceived = MsgsSent = MsgsRejectedIn = MsgsRejectedOut = BytesForwardedIn = BytesForwardedOut = 0;

		for (n = 0; n < 4; n++)
		{
			MsgsReceived +=	User->Total.MsgsReceived[n] - User->Last.MsgsReceived[n];	
			MsgsSent += User->Total.MsgsSent[n] - User->Last.MsgsSent[n];
			BytesForwardedIn += User->Total.BytesForwardedIn[n] - User->Last.BytesForwardedIn[n];
			BytesForwardedOut += User->Total.BytesForwardedOut[n] - User->Last.BytesForwardedOut[n];
			MsgsRejectedIn += User->Total.MsgsRejectedIn[n] - User->Last.MsgsRejectedIn[n];
			MsgsRejectedOut += User->Total.MsgsRejectedOut[n] - User->Last.MsgsRejectedOut[n];
		}

		len = sprintf(Line, "%s %-7s %5d %8d %10d %10d %10d %10d %10d %10d\r\n",
			(User->flags & F_BBS)? "(B)": "   ",
			User->Call, ConnectsIn,
			ConnectsOut,
			MsgsReceived,
			MsgsSent, 
			BytesForwardedIn,
			BytesForwardedOut,
			MsgsRejectedIn,
			MsgsRejectedOut);
*/

		for (n = 0; n < 4; n++)
		{
			TotMsgsReceived[n] += User->Total.MsgsReceived[n] - User->Last.MsgsReceived[n];
			TotMsgsSent[n] += User->Total.MsgsSent[n] - User->Last.MsgsSent[n];

			TotBytesForwardedIn[n] += User->Total.BytesForwardedIn[n] - User->Last.BytesForwardedIn[n];
			TotBytesForwardedOut[n] += User->Total.BytesForwardedOut[n] - User->Last.BytesForwardedOut[n];
		}

		sprintf(MsgsIn,"%d/%d/%d", User->Total.MsgsReceived[1] - User->Last.MsgsReceived[1],
			User->Total.MsgsReceived[2] - User->Last.MsgsReceived[2],
			User->Total.MsgsReceived[3] - User->Last.MsgsReceived[3]);

		sprintf(MsgsOut,"%d/%d/%d", User->Total.MsgsSent[1] - User->Last.MsgsSent[1],
			User->Total.MsgsSent[2] - User->Last.MsgsSent[2],
			User->Total.MsgsSent[3] - User->Last.MsgsSent[3]);

		sprintf(BytesIn,"%d/%d/%d", User->Total.BytesForwardedIn[1] - User->Last.BytesForwardedIn[1],
			User->Total.BytesForwardedIn[2] - User->Last.BytesForwardedIn[2],
			User->Total.BytesForwardedIn[3] - User->Last.BytesForwardedIn[3]);

		sprintf(BytesOut,"%d/%d/%d", User->Total.BytesForwardedOut[1] - User->Last.BytesForwardedOut[1],
			User->Total.BytesForwardedOut[2] - User->Last.BytesForwardedOut[2],
			User->Total.BytesForwardedOut[3] - User->Last.BytesForwardedOut[3]);

		sprintf(RejIn,"%d/%d/%d", User->Total.MsgsRejectedIn[1] - User->Last.MsgsRejectedIn[1],
			User->Total.MsgsRejectedIn[2] - User->Last.MsgsRejectedIn[2],
			User->Total.MsgsRejectedIn[3] - User->Last.MsgsRejectedIn[3]);

		sprintf(RejOut,"%d/%d/%d", User->Total.MsgsRejectedOut[1] - User->Last.MsgsRejectedOut[1],
			User->Total.MsgsRejectedOut[2] - User->Last.MsgsRejectedOut[2],
			User->Total.MsgsRejectedOut[3] - User->Last.MsgsRejectedOut[3]);

		len = sprintf(Line, "%s %-7s %5d %8d%16s%16s%16s%16s%16s%16s\r\n",
			(User->flags & F_BBS)? "(B)": "   ",
			User->Call, ConnectsIn,
			ConnectsOut,
			MsgsIn,
			MsgsOut, 
			BytesIn,
			BytesOut,
			RejIn,
			RejOut);

		fwrite(Line, 1, len, hFile);

		User->Last.ConnectsIn = User->Total.ConnectsIn;
		User->Last.ConnectsOut = User->Total.ConnectsOut;

		for (n = 0; n < 4; n++)
		{
			User->Last.MsgsReceived[n] = User->Total.MsgsReceived[n];	
			User->Last.MsgsSent[n] = User->Total.MsgsSent[n];
			User->Last.BytesForwardedIn[n] = User->Total.BytesForwardedIn[n];
			User->Last.BytesForwardedOut[n] = User->Total.BytesForwardedOut[n];
			User->Last.MsgsRejectedIn[n] = User->Total.MsgsRejectedIn[n];
			User->Last.MsgsRejectedOut[n] = User->Total.MsgsRejectedOut[n];
		}

	}

	sprintf(MsgsIn,"%d/%d/%d", TotMsgsReceived[1], TotMsgsReceived[2], TotMsgsReceived[3]);

	sprintf(MsgsOut,"%d/%d/%d", TotMsgsSent[1], TotMsgsSent[2], TotMsgsSent[3]);

	sprintf(BytesIn,"%d/%d/%d", TotBytesForwardedIn[1], TotBytesForwardedIn[2], TotBytesForwardedIn[3]);

	sprintf(BytesOut,"%d/%d/%d", TotBytesForwardedOut[1], TotBytesForwardedOut[2], TotBytesForwardedOut[3]);

	len = sprintf(Line, "\r\n Totals    %s Messages In        %s Messages Out       %s"
						" Bytes In        %s Bytes Out\r\n", MsgsIn, MsgsOut, BytesIn, BytesOut);

	fwrite(Line, 1, len, hFile);

	SaveConfig(ConfigName);
	GetConfig(ConfigName);

	SaveUserDatabase();
	fclose(hFile);
}
