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
//	Utility Routines

#include "BPQMail.h"
#ifdef WIN32
#include "Winspool.h"
#endif

BOOL Bells;
BOOL FlashOnBell;		// Flash instead of Beep
BOOL StripLF;

BOOL WarnWrap;
BOOL FlashOnConnect;
BOOL WrapInput;
BOOL CloseWindowOnBye;

RECT ConsoleRect;

BOOL OpenConsole;
BOOL OpenMon;

//#define BBSIDLETIME 120
//#define USERIDLETIME 300


#define BBSIDLETIME 900
#define USERIDLETIME 900

unsigned long _beginthread( void( *start_address )(VOID * DParam),
				unsigned stack_size, VOID * DParam);


int APIENTRY GetRaw(int stream, char * msg, int * len, int * count);
void GetSemaphore(struct SEM * Semaphore, int ID);
void FreeSemaphore(struct SEM * Semaphore);
int EncryptPass(char * Pass, char * Encrypt);
VOID DecryptPass(char * Encrypt, unsigned char * Pass, unsigned int len);
void DeletetoRecycle(char * FN);
VOID DoImportCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context);
VOID DoExportCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context);
VOID TidyPrompts();
char * ReadMessageFileEx(struct MsgInfo * MsgRec);
UCHAR * APIENTRY GetBPQDirectory();
BOOL SendARQMail(CIRCUIT * conn);
int APIENTRY ChangeSessionIdletime(int Stream, int idletime);
int APIENTRY GetApplNum(int Stream);
VOID FormatTime(char * Time, time_t cTime);
BOOL CheckifPacket(char * Via);
UCHAR * APIENTRY GetVersionString();
void ListFiles(ConnectionInfo * conn, struct UserInfo * user, char * filename);
void ReadBBSFile(ConnectionInfo * conn, struct UserInfo * user, char * filename);
int GetCMSHash(char * Challenge, char * Password);
BOOL SendAMPRSMTP(CIRCUIT * conn);

config_t cfg;
config_setting_t * group;

extern ULONG BBSApplMask;

//static int SEMCLASHES = 0;

char SecureMsg[80] = "";			// CMS Secure Signon Response

int	NumberofStreams;

extern char VersionStringWithBuild[50];

#define MaxSockets 64

extern struct SEM OutputSEM;

extern ConnectionInfo Connections[MaxSockets+1];

extern struct UserInfo ** UserRecPtr;
extern int NumberofUsers;

extern struct UserInfo * BBSChain;					// Chain of users that are BBSes

extern struct MsgInfo ** MsgHddrPtr;
extern int NumberofMessages;

extern int FirstMessageIndextoForward;					// Lowest Message wirh a forward bit set - limits search

extern char UserDatabaseName[MAX_PATH];
extern char UserDatabasePath[MAX_PATH];

extern char MsgDatabasePath[MAX_PATH];
extern char MsgDatabaseName[MAX_PATH];

extern char BIDDatabasePath[MAX_PATH];
extern char BIDDatabaseName[MAX_PATH];

extern char WPDatabasePath[MAX_PATH];
extern char WPDatabaseName[MAX_PATH];

extern char BadWordsPath[MAX_PATH];
extern char BadWordsName[MAX_PATH];

extern char BaseDir[MAX_PATH];
extern char BaseDirRaw[MAX_PATH];			// As set in registry - may contain %NAME%
extern char ProperBaseDir[MAX_PATH];		// BPQ Directory/BPQMailChat


extern char MailDir[MAX_PATH];

extern BIDRec ** BIDRecPtr;
extern int NumberofBIDs;

extern BIDRec ** TempBIDRecPtr;
extern int NumberofTempBIDs;

extern WPRec ** WPRecPtr;
extern int NumberofWPrecs;

extern char ** BadWords;
extern int NumberofBadWords;
extern char * BadFile;

extern int LatestMsg;
extern struct SEM MsgNoSemaphore;					// For locking updates to LatestMsg
extern int HighestBBSNumber;

extern int MaxMsgno;
extern int BidLifetime;
extern int MaxAge;
extern int MaintInterval;
extern int MaintTime;

extern int ProgramErrors;

extern BOOL MonBBS;
extern BOOL MonCHAT;
extern BOOL MonTCP;

BOOL SendNewUserMessage = TRUE;
BOOL AllowAnon = FALSE;

#define BPQHOSTSTREAMS	64

// Although externally streams are numbered 1 to 64, internally offsets are 0 - 63

extern BPQVECSTRUC BPQHOSTVECTOR[BPQHOSTSTREAMS + 5];

FILE * LogHandle[4] = {NULL, NULL, NULL, NULL};

char FilesNames[4][100] = {"", "", "", ""};

char * Logs[4] = {"BBS", "CHAT", "TCP", "DEBUG"};

BOOL OpenLogfile(int Flags)
{
	UCHAR FN[MAX_PATH];
	time_t LT;
	struct tm * tm;

	LT = time(NULL);
	tm = gmtime(&LT);	

	sprintf(FN,"%s/logs/log_%02d%02d%02d_%s.txt", GetBPQDirectory(), tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, Logs[Flags]);

	LogHandle[Flags] = fopen(FN, "ab");
		
#ifndef WIN32

	if (strcmp(FN, &FilesNames[Flags][0]))
	{
		UCHAR SYMLINK[MAX_PATH];

		sprintf(SYMLINK,"%s/logLatest_%s.txt", GetBPQDirectory(), Logs[Flags]);
		unlink(SYMLINK); 
		strcpy(&FilesNames[Flags][0], FN);
		symlink(FN, SYMLINK);
	}

#endif

	return (LogHandle[Flags] != NULL);
}

struct SEM LogSEM = {0, 0};

void WriteLogLine(CIRCUIT * conn, int Flag, char * Msg, int MsgLen, int Flags)
{
	char CRLF[2] = {0x0d,0x0a};
	struct tm * tm;
	char Stamp[20];
	time_t LT;
//	struct _EXCEPTION_POINTERS exinfo;

#ifndef LINBPQ
	__try
	{
#endif

#ifndef LINBPQ

	if (hMonitor)
	{
		if (Flags == LOG_TCP && MonTCP)
		{	
			WritetoMonitorWindow((char *)&Flag, 1);
			WritetoMonitorWindow(Msg, MsgLen);
			WritetoMonitorWindow(CRLF , 1);
		}
		else if (Flags == LOG_CHAT && MonCHAT)
		{	
			WritetoMonitorWindow((char *)&Flag, 1);

			if (conn && conn->Callsign[0])
			{
				char call[20];
				sprintf(call, "%s          ", conn->Callsign);
				WritetoMonitorWindow(call, 10);
			}
			else
				WritetoMonitorWindow("          ", 10);

			WritetoMonitorWindow(Msg, MsgLen);
			if (Msg[MsgLen-1] != '\r')
				WritetoMonitorWindow(CRLF , 1);
		}
		else if (Flags == LOG_BBS  && MonBBS)
		{	
			WritetoMonitorWindow((char *)&Flag, 1);
			if (conn && conn->Callsign[0])
			{
				char call[20];
				sprintf(call, "%s          ", conn->Callsign);
				WritetoMonitorWindow(call, 10);
			}
			else
				WritetoMonitorWindow("          ", 10);

			WritetoMonitorWindow(Msg, MsgLen);
			WritetoMonitorWindow(CRLF , 1);
		}
		else if (Flags == LOG_DEBUG_X)
		{	
			WritetoMonitorWindow((char *)&Flag, 1);
			WritetoMonitorWindow(Msg, MsgLen);
			WritetoMonitorWindow(CRLF , 1);
		}

	}
#endif

	if (Flags == LOG_TCP && !LogTCP)
		return;
	if (Flags == LOG_BBS && !LogBBS)
		return;
	if (Flags == LOG_CHAT && !LogCHAT)
		return;

	GetSemaphore(&LogSEM, 0);

	if (LogHandle[Flags] == NULL) OpenLogfile(Flags);

	if (LogHandle[Flags] == NULL) 
	{
		FreeSemaphore(&LogSEM);
		return;
	}
	LT = time(NULL);
	tm = gmtime(&LT);	
	
	sprintf(Stamp,"%02d%02d%02d %02d:%02d:%02d %c",
				tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, Flag);

	fwrite(Stamp, 1, strlen(Stamp), LogHandle[Flags]);

	if (conn && conn->Callsign[0])
	{
		char call[20];
		sprintf(call, "%s          ", conn->Callsign);
		fwrite(call, 1, 10, LogHandle[Flags]);
	}
	else
		fwrite("          ", 1, 10, LogHandle[Flags]);

	fwrite(Msg, 1, MsgLen, LogHandle[Flags]);

	if (Flags == LOG_CHAT && Msg[MsgLen-1] == '\r')
		fwrite(&CRLF[1], 1, 1, LogHandle[Flags]);
	else
		fwrite(CRLF, 1, 2, LogHandle[Flags]);
		
	if (LogHandle[Flags])
		fclose(LogHandle[Flags]);

	LogHandle[Flags] = NULL;
	FreeSemaphore(&LogSEM);
	
#ifndef LINBPQ
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
#endif
}

int	CriticalErrorHandler(char * error)
{
	Debugprintf("Critical Error %s", error);
	ProgramErrors = 25;
	CheckProgramErrors();				// Force close
	return 0;
}


VOID __cdecl Debugprintf(const char * format, ...)
{
	char Mess[1000];
	va_list(arglist);int Len;

	va_start(arglist, format);
	Len = vsprintf(Mess, format, arglist);
#ifndef LINBPQ
	WriteLogLine(NULL, '!',Mess, Len, LOG_DEBUG_X);
#endif
	//	#ifdef _DEBUG 
	strcat(Mess, "\r\n");
	OutputDebugString(Mess);

//	#endif
	return;
}

VOID __cdecl Logprintf(int LogMode, CIRCUIT * conn, int InOut, const char * format, ...)
{
	char Mess[1000];
	va_list(arglist);int Len;

	va_start(arglist, format);
	Len = vsprintf(Mess, format, arglist);
	WriteLogLine(conn, InOut, Mess, Len, LogMode);

	return;
}

struct MsgInfo * GetMsgFromNumber(int msgno)
{
	if (msgno < 1 || msgno > 999999)
		return NULL;
	
	return MsgnotoMsg[msgno];
}
		
struct UserInfo * AllocateUserRecord(char * Call)
{
	struct UserInfo * User = zalloc(sizeof (struct UserInfo));
		
	strcpy(User->Call, Call);
	User->Length = sizeof (struct UserInfo);

	GetSemaphore(&AllocSemaphore, 0);

	UserRecPtr=realloc(UserRecPtr,(++NumberofUsers+1)*4);
	UserRecPtr[NumberofUsers]= User;

	FreeSemaphore(&AllocSemaphore);

	return User;
}

struct MsgInfo * AllocateMsgRecord()
{
	struct MsgInfo * Msg = zalloc(sizeof (struct MsgInfo));

	GetSemaphore(&AllocSemaphore, 0);

	MsgHddrPtr=realloc(MsgHddrPtr,(++NumberofMessages+1)*4);
	MsgHddrPtr[NumberofMessages] = Msg;

	FreeSemaphore(&AllocSemaphore);

	return Msg;
}

BIDRec * AllocateBIDRecord()
{
	BIDRec * BID = zalloc(sizeof (BIDRec));
	
	GetSemaphore(&AllocSemaphore, 0);

	BIDRecPtr=realloc(BIDRecPtr,(++NumberofBIDs+1)*4);
	BIDRecPtr[NumberofBIDs] = BID;

	FreeSemaphore(&AllocSemaphore);

	return BID;
}

BIDRec * AllocateTempBIDRecord()
{
	BIDRec * BID = zalloc(sizeof (BIDRec));
	
	GetSemaphore(&AllocSemaphore, 0);

	TempBIDRecPtr=realloc(TempBIDRecPtr,(++NumberofTempBIDs+1)*4);
	TempBIDRecPtr[NumberofTempBIDs] = BID;

	FreeSemaphore(&AllocSemaphore);

	return BID;
}

struct UserInfo * LookupCall(char * Call)
{
	struct UserInfo * ptr = NULL;
	int i;

	for (i=1; i <= NumberofUsers; i++)
	{
		ptr = UserRecPtr[i];

		if (_stricmp(ptr->Call, Call) == 0) return ptr;

	}

	return NULL;
}

VOID GetUserDatabase()
{
	struct UserInfo UserRec;

	FILE * Handle;
	int ReadLen;
	struct UserInfo * user;
	time_t UserLimit = time(NULL) - (UserLifetime * 86400); // Oldest user to keep

	Handle = fopen(UserDatabasePath, "rb");

	if (Handle == NULL)
	{
		// Initialise a new File

		UserRecPtr=malloc(4);
		UserRecPtr[0]= malloc(sizeof (struct UserInfo));
		memset(UserRecPtr[0], 0, sizeof (struct UserInfo));
		UserRecPtr[0]->Length = sizeof (struct UserInfo);

		NumberofUsers = 0;

		return;
	}


	// Get First Record
		
	ReadLen = fread(&UserRec, 1, sizeof (UserRec), Handle);
	
	if (ReadLen == 0)
	{
		// Duff file

		memset(&UserRec, 0, sizeof (struct UserInfo));
		UserRec.Length = sizeof (struct UserInfo);
	}
	else
	{
		// See if format has changed

		if (UserRec.Length == 0)
		{
			// Old format without a Length field

			struct OldUserInfo * OldRec = (struct OldUserInfo *)&UserRec;
			int Users = OldRec->ConnectsIn;		// User Count in control record
			char Backup1[MAX_PATH];

			//  Create a backup in case reversion is needed and Reposition to first User record

			fclose(Handle);

			strcpy(Backup1, UserDatabasePath);
			strcat(Backup1, ".oldformat");

			CopyFile(UserDatabasePath, Backup1, FALSE);	 // Copy to .bak

			Handle = fopen(UserDatabasePath, "rb");
			
			ReadLen = fread(&UserRec, 1, sizeof (struct OldUserInfo), Handle);	// Skip Control Record

			// Set up control record

			UserRecPtr=malloc(4);
			UserRecPtr[0]= malloc(sizeof (struct UserInfo));
			memcpy(UserRecPtr[0], &UserRec,  sizeof (UserRec));
			UserRecPtr[0]->Length = sizeof (UserRec);

			NumberofUsers = 0;
		
		OldNext:

			ReadLen = fread(&UserRec, 1, sizeof (struct OldUserInfo), Handle);

			if (ReadLen > 0)
			{
				if (OldRec->Call[0] < '0')
					goto OldNext;					// Blank record
			
				user = AllocateUserRecord(OldRec->Call);
				user->Temp = zalloc(sizeof (struct TempUserInfo));

				// Copy info from Old record

				user->lastmsg = OldRec->lastmsg;
				user->Total.ConnectsIn = OldRec->ConnectsIn;
				user->TimeLastConnected = OldRec->TimeLastConnected;
				user->flags = OldRec->flags;
				user->PageLen = OldRec->PageLen;
				user->BBSNumber = OldRec->BBSNumber;
				memcpy(user->Name, OldRec->Name, 18);
				memcpy(user->Address, OldRec->Address, 61);
				user->Total.MsgsReceived[0] = OldRec->MsgsReceived;
				user->Total.MsgsSent[0] = OldRec->MsgsSent;
				user->Total.MsgsRejectedIn[0] = OldRec->MsgsRejectedIn;			// Messages we reject
				user->Total.MsgsRejectedOut[0] = OldRec->MsgsRejectedOut;		// Messages Rejectd by other end
				user->Total.BytesForwardedIn[0] = OldRec->BytesForwardedIn;
				user->Total.BytesForwardedOut[0] = OldRec->BytesForwardedOut;
				user->Total.ConnectsOut = OldRec->ConnectsOut;			// Forwarding Connects Out
				user->RMSSSIDBits = OldRec->RMSSSIDBits;			// SSID's to poll in RMS
				memcpy(user->HomeBBS, OldRec->HomeBBS, 41);
				memcpy(user->QRA, OldRec->QRA, 7);
				memcpy(user->pass, OldRec->pass, 13);
				memcpy(user->ZIP, OldRec->ZIP, 9);

				//	Read any forwarding info, even if not a BBS.
				//	This allows a BBS to be temporarily set as a
				//	normal user without loosing forwarding info

				SetupForwardingStruct(user);

				if (user->flags & F_BBS)
				{
					// Defined as BBS - allocate and initialise forwarding structure

					// Add to BBS Chain;
	
					user->BBSNext = BBSChain;
					BBSChain = user;

					// Save Highest BBS Number

					if (user->BBSNumber > HighestBBSNumber) HighestBBSNumber = user->BBSNumber;
				}
				goto OldNext;
			}

			SortBBSChain();
			fclose(Handle);	

			return;
		}
	}
			
	// Set up control record

	UserRecPtr=malloc(4);
	UserRecPtr[0]= malloc(sizeof (struct UserInfo));
	memcpy(UserRecPtr[0], &UserRec,  sizeof (UserRec));
	UserRecPtr[0]->Length = sizeof (UserRec);

	NumberofUsers = 0;

Next:

	ReadLen = fread(&UserRec, 1, sizeof (UserRec), Handle);

	if (ReadLen > 0)
	{
		if (UserRec.Call[0] < '0')
			goto Next;					// Blank record

		if ((UserRec.flags & F_BBS) == 0)		// Not BBS - Check Age
			if (UserLifetime)					// if limit set
				if (UserRec.TimeLastConnected)	// Dont delete manually added Users that havent yet connected
					if (UserRec.TimeLastConnected < UserLimit)
						goto Next;			// Too Old - ignore
			
		user = AllocateUserRecord(UserRec.Call);
		memcpy(user, &UserRec,  sizeof (UserRec));
		user->Temp = zalloc(sizeof (struct TempUserInfo));

		user->ForwardingInfo = NULL;	// In case left behind on crash
		user->BBSNext = NULL;
		user->POP3Locked = FALSE;

		if (user->lastmsg < 0 || user->lastmsg > LatestMsg)
			user->lastmsg = LatestMsg;

		//	Read any forwarding info, even if not a BBS.
		//	This allows a BBS to be temporarily set as a
		//	normal user without loosing forwarding info

			SetupForwardingStruct(user);

		if (user->flags & F_BBS)
		{
			// Add to BBS Chain;

			user->BBSNext = BBSChain;
			BBSChain = user;

			// Save Highest BBS Number

			if (user->BBSNumber > HighestBBSNumber) HighestBBSNumber = user->BBSNumber;
		}
		goto Next;
	}

	SortBBSChain();

	fclose(Handle);	
}

VOID CopyUserDatabase()
{
	char Backup1[MAX_PATH];
	char Backup2[MAX_PATH];

	// Keep 4 Generations

	strcpy(Backup2, UserDatabasePath);
	strcat(Backup2, ".bak.3");

	strcpy(Backup1, UserDatabasePath);
	strcat(Backup1, ".bak.2");

	DeleteFile(Backup2);			// Remove old .bak.3
	MoveFile(Backup1, Backup2);		// Move .bak.2 to .bak.3

	strcpy(Backup2, UserDatabasePath);
	strcat(Backup2, ".bak.1");

	MoveFile(Backup2, Backup1);		// Move .bak.1 to .bak.2

	strcpy(Backup1, UserDatabasePath);
	strcat(Backup1, ".bak");

	MoveFile(Backup1, Backup2);		//Move .bak to .bak.1

	CopyFile(UserDatabasePath, Backup1, FALSE);	 // Copy to .bak

}

VOID CopyConfigFile(char * ConfigName)
{
	char Backup1[MAX_PATH];
	char Backup2[MAX_PATH];

	// Keep 4 Generations

	strcpy(Backup2, ConfigName);
	strcat(Backup2, ".bak.3");

	strcpy(Backup1, ConfigName);
	strcat(Backup1, ".bak.2");

	DeleteFile(Backup2);			// Remove old .bak.3
	MoveFile(Backup1, Backup2);		// Move .bak.2 to .bak.3

	strcpy(Backup2, ConfigName);
	strcat(Backup2, ".bak.1");

	MoveFile(Backup2, Backup1);		// Move .bak.1 to .bak.2

	strcpy(Backup1, ConfigName);
	strcat(Backup1, ".bak");

	MoveFile(Backup1, Backup2);		// Move .bak to .bak.1

	CopyFile(ConfigName, Backup1, FALSE);	// Copy to .bak
}


VOID SaveUserDatabase()
{
	FILE * Handle;
	int WriteLen;
	int i;

	Handle = fopen(UserDatabasePath, "wb");

	UserRecPtr[0]->Total.ConnectsIn = NumberofUsers;

	for (i=0; i <= NumberofUsers; i++)
	{
		WriteLen = fwrite(UserRecPtr[i], 1, sizeof (struct UserInfo), Handle);
	}

	fclose(Handle);

	return;
}

VOID GetMessageDatabase()
{
	struct MsgInfo MsgRec;
	FILE * Handle;
	int ReadLen;
	struct MsgInfo * Msg;
	char * MsgBytes;
	int FileRecsize = sizeof(struct MsgInfo);	// May be changed if reformating
	BOOL Reformatting = FALSE;

	Handle = fopen(MsgDatabasePath, "rb");

	if (Handle == NULL)
	{
		// Initialise a new File

		MsgHddrPtr=malloc(4);
		MsgHddrPtr[0]= zalloc(sizeof (MsgRec));
		NumberofMessages = 0;
		MsgHddrPtr[0]->status = 1;

		return;
	}

	// Get First Record
		
	ReadLen = fread(&MsgRec, 1, FileRecsize, Handle); 

	if (ReadLen == 0)
	{
		// Duff file

		memset(&MsgRec, 0, sizeof (MsgRec));
		MsgRec.status = 1;
	}

	// Set up control record

	MsgHddrPtr=malloc(4);
	MsgHddrPtr[0]= malloc(sizeof (MsgRec));
	memcpy(MsgHddrPtr[0], &MsgRec,  sizeof (MsgRec));

	LatestMsg=MsgHddrPtr[0]->length;

	NumberofMessages = 0;

	if (MsgRec.status == 1)		// Used as file format version
								// 0 = original, 1 = Extra email from addr, 2 = More BBS's.
	{
		char Backup1[MAX_PATH];

			//  Create a backup in case reversion is needed and Reposition to first User record

			fclose(Handle);

			strcpy(Backup1, MsgDatabasePath);
			strcat(Backup1, ".oldformat");

			CopyFile(MsgDatabasePath, Backup1, FALSE);	 // Copy to .oldformat

			Handle = fopen(MsgDatabasePath, "rb");

			FileRecsize = sizeof(struct OldMsgInfo);
			
			ReadLen = fread(&MsgRec, 1, FileRecsize, Handle); 

			MsgHddrPtr[0]->status = 2;
	}

Next: 

	ReadLen = fread(&MsgRec, 1, FileRecsize, Handle); 

	if (ReadLen > 0)
	{
		// Validate Header

		if (FileRecsize == sizeof(struct MsgInfo))
		{
			if (MsgRec.type == 0 || MsgRec.number == 0)
				goto Next;

			MsgBytes = ReadMessageFileEx(&MsgRec);

			if (MsgBytes)
			{
	//			MsgRec.length = strlen(MsgBytes);
				free(MsgBytes);
			}
			else
				goto Next;

			Msg = AllocateMsgRecord();

			memcpy(Msg, &MsgRec, +sizeof (MsgRec));
		}
		else
		{
			// Resizing - record from file is an OldRecInfo
			
			struct OldMsgInfo * OldMessage = (struct OldMsgInfo *) &MsgRec;

			if (OldMessage->type == 0)
				goto Next;

			Msg = AllocateMsgRecord();


			Msg->B2Flags = OldMessage->B2Flags;
			memcpy(Msg->bbsfrom, OldMessage->bbsfrom, 7);
			memcpy(Msg->bid, OldMessage->bid, 13);
			Msg->datechanged = OldMessage->datechanged;
			Msg->datecreated = OldMessage->datecreated;
			Msg->datereceived = OldMessage->datereceived;
			memcpy(Msg->emailfrom, OldMessage->emailfrom, 41);
			memcpy(Msg->fbbs , OldMessage->fbbs, 10);
			memcpy(Msg->forw , OldMessage->forw, 10);
			memcpy(Msg->from, OldMessage->from, 7);
			Msg->length = OldMessage->length;
			Msg->nntpnum = OldMessage->nntpnum;
			Msg->number = OldMessage->number;
			Msg->status = OldMessage->status;
			memcpy(Msg->title, OldMessage->title, 61);
			memcpy(Msg->to, OldMessage->to, 7);
			Msg->type = OldMessage->type;
			memcpy(Msg->via, OldMessage->via, 41);
		}

		MsgnotoMsg[Msg->number] = Msg;

		// Fix Corrupted NTS Messages

		if (Msg->type == 'N')
			Msg->type = 'T';

		// Look for corrupt FROM address (ending in @)

		strlop(Msg->from, '@');

		BuildNNTPList(Msg);				// Build NNTP Groups list

		Msg->Locked = 0;				// In case left locked
		Msg->Defered = 0;				// In case left set.

		// If any forward bits are set, increment count on corresponding BBS record.

		if (memcmp(Msg->fbbs, zeros, NBMASK) != 0)
		{
			if (FirstMessageIndextoForward == 0)
				FirstMessageIndextoForward = NumberofMessages;			// limit search
		}

		goto Next;
	}

	if (FirstMessageIndextoForward == 0)
		FirstMessageIndextoForward = NumberofMessages;			// limit search

	fclose(Handle);
}

VOID CopyMessageDatabase()
{
	char Backup1[MAX_PATH];
	char Backup2[MAX_PATH];

	// Keep 4 Generations

	strcpy(Backup2, MsgDatabasePath);
	strcat(Backup2, ".bak.3");

	strcpy(Backup1, MsgDatabasePath);
	strcat(Backup1, ".bak.2");

	DeleteFile(Backup2);			// Remove old .bak.3
	MoveFile(Backup1, Backup2);		// Move .bak.2 to .bak.3

	strcpy(Backup2, MsgDatabasePath);
	strcat(Backup2, ".bak.1");

	MoveFile(Backup2, Backup1);		// Move .bak.1 to .bak.2

	strcpy(Backup1, MsgDatabasePath);
	strcat(Backup1, ".bak");

	MoveFile(Backup1, Backup2);		//Move .bak to .bak.1

	strcpy(Backup2, MsgDatabasePath);
	strcat(Backup2, ".bak");

	CopyFile(MsgDatabasePath, Backup2, FALSE);	// Copy to .bak

}

VOID SaveMessageDatabase()
{
	FILE * Handle;
	int WriteLen;
	int i;

	Handle = fopen(MsgDatabasePath, "wb");

	MsgHddrPtr[0]->number = NumberofMessages;
	MsgHddrPtr[0]->length = LatestMsg;

	for (i=0; i <= NumberofMessages; i++)
	{
		WriteLen = fwrite(MsgHddrPtr[i], 1, sizeof (struct MsgInfo), Handle);
	}

	fclose(Handle);
	return;
}

VOID GetBIDDatabase()
{
	BIDRec BIDRec;
	FILE * Handle;
	int ReadLen;
	BIDRecP BID;

	Handle = fopen(BIDDatabasePath, "rb");

	if (Handle == NULL)
	{
		// Initialise a new File

		BIDRecPtr=malloc(4);
		BIDRecPtr[0]= malloc(sizeof (BIDRec));
		memset(BIDRecPtr[0], 0, sizeof (BIDRec));
		NumberofBIDs = 0;

		return;
	}


	// Get First Record
		
	ReadLen = fread(&BIDRec, 1, sizeof (BIDRec), Handle); 

	if (ReadLen == 0)
	{
		// Duff file

		memset(&BIDRec, 0, sizeof (BIDRec));
	}

	// Set up control record

	BIDRecPtr=malloc(4);
	BIDRecPtr[0]= malloc(sizeof (BIDRec));
	memcpy(BIDRecPtr[0], &BIDRec,  sizeof (BIDRec));

	NumberofBIDs = 0;

Next:

	ReadLen = fread(&BIDRec, 1, sizeof (BIDRec), Handle); 

	if (ReadLen > 0)
	{
		BID = AllocateBIDRecord();
		memcpy(BID, &BIDRec,  sizeof (BIDRec));

		if (BID->u.timestamp == 0) 	
			BID->u.timestamp = LOWORD(time(NULL)/86400);

		goto Next;
	}

	fclose(Handle);
}

VOID CopyBIDDatabase()
{
	char Backup[MAX_PATH];

	strcpy(Backup, BIDDatabasePath);
	strcat(Backup, ".bak");

	CopyFile(BIDDatabasePath, Backup, FALSE);
}

VOID SaveBIDDatabase()
{
	FILE * Handle;
	int WriteLen;
	int i;

	Handle = fopen(BIDDatabasePath, "wb");

	BIDRecPtr[0]->u.msgno = NumberofBIDs;			// First Record has file size

	for (i=0; i <= NumberofBIDs; i++)
	{
		WriteLen = fwrite(BIDRecPtr[i], 1, sizeof (BIDRec), Handle);
	}

	fclose(Handle);

	return;
}

BIDRec * LookupBID(char * BID)
{
	BIDRec * ptr = NULL;
	int i;

	for (i=1; i <= NumberofBIDs; i++)
	{
		ptr = BIDRecPtr[i];

		if (_stricmp(ptr->BID, BID) == 0)
			return ptr;
	}

	return NULL;
}

BIDRec * LookupTempBID(char * BID)
{
	BIDRec * ptr = NULL;
	int i;

	for (i=1; i <= NumberofTempBIDs; i++)
	{
		ptr = TempBIDRecPtr[i];

		if (_stricmp(ptr->BID, BID) == 0) return ptr;
	}

	return NULL;
}

VOID RemoveTempBIDS(CIRCUIT * conn)
{
	// Remove any Temp BID records for conn. Called when connection closes - Msgs will be complete or failed
	
	if (NumberofTempBIDs == 0)
		return;
	else
	{
		BIDRec * ptr = NULL;
		BIDRec ** NewTempBIDRecPtr = zalloc((NumberofTempBIDs+1) * 4);
		int i = 0, n;

		GetSemaphore(&AllocSemaphore, 0);

		for (n = 1; n <= NumberofTempBIDs; n++)
		{
			ptr = TempBIDRecPtr[n];

			if (ptr)
			{
				if (ptr->u.conn == conn)
					// Remove this entry 
					free(ptr);
				else
					NewTempBIDRecPtr[++i] = ptr;
			}
		}

		NumberofTempBIDs = i;

		free(TempBIDRecPtr);

		TempBIDRecPtr = NewTempBIDRecPtr;
		FreeSemaphore(&AllocSemaphore);
	}

}

VOID GetBadWordFile()
{
	FILE * Handle;
	DWORD FileSize;
	char * ptr1, * ptr2;
	struct stat STAT;

	if (stat(BadWordsPath, &STAT) == -1)
		return;

	FileSize = STAT.st_size;

	Handle = fopen(BadWordsPath, "rb");

	if (Handle == NULL)
		return;

	BadFile = malloc(FileSize+1);

	fread(BadFile, 1, FileSize, Handle); 

	fclose(Handle);

	BadFile[FileSize]=0;

	_strlwr(BadFile);								// Compares are case-insensitive

	ptr1 = BadFile;

	while (ptr1)
	{
		if (*ptr1 == '\n') ptr1++;

		ptr2 = strtok_s(NULL, "\r\n", &ptr1);
		if (ptr2)
		{
			if (*ptr2 != '#')
			{
				BadWords = realloc(BadWords,(++NumberofBadWords+1)*4);
				BadWords[NumberofBadWords] = ptr2;
			}
		}
		else
			break;
	}
}

BOOL CheckBadWord(char * Word, char * Msg)
{
	char * ptr1 = Msg, * ptr2;
	int len = strlen(Word);

	while (*ptr1)					// Stop at end
	{
		ptr2 = strstr(ptr1, Word);

		if (ptr2 == NULL)
			return FALSE;				// OK

		// Only bad if it ia not part of a longer word

		if ((ptr2 == Msg) || !(isalpha(*(ptr2 - 1))))	// No alpha before
			if (!(isalpha(*(ptr2 + len))))			// No alpha after
				return TRUE;					// Bad word
	
		// Keep searching

		ptr1 = ptr2 + len;
	}

	return FALSE;					// OK
}

BOOL CheckBadWords(char * Msg)
{
	char * dupMsg = _strlwr(_strdup(Msg));
	int i;

	for (i = 1; i <= NumberofBadWords; i++)
	{
		if (CheckBadWord(BadWords[i], dupMsg))
		{
			free(dupMsg);
			return TRUE;			// Bad
		}
	}

	free(dupMsg);
	return FALSE;					// OK

}

VOID SendWelcomeMsg(int Stream, ConnectionInfo * conn, struct UserInfo * user)
{
	if (user->flags & F_Expert)
		ExpandAndSendMessage(conn, ExpertWelcomeMsg, LOG_BBS);
	else if (conn->NewUser)
		ExpandAndSendMessage(conn, NewWelcomeMsg, LOG_BBS);
	else
		ExpandAndSendMessage(conn, WelcomeMsg, LOG_BBS);

	if (user->HomeBBS[0] == 0 && !DontNeedHomeBBS)
		BBSputs(conn, "Please enter your Home BBS using the Home command.\rYou may also enter your QTH and ZIP/Postcode using qth and zip commands.\r");

//	if (user->flags & F_Temp_B2_BBS)
//		nodeprintf(conn, "%s CMS >\r", BBSName);
//	else
		SendPrompt(conn, user);
}

VOID SendPrompt(ConnectionInfo * conn, struct UserInfo * user)
{
	if (user->Temp->ListSuspended)
		return;						// Dont send prompt if pausing a liting
		
	if (user->flags & F_Expert)
		ExpandAndSendMessage(conn, ExpertPrompt, LOG_BBS);
	else if (conn->NewUser)
		ExpandAndSendMessage(conn, NewPrompt, LOG_BBS);
	else
		ExpandAndSendMessage(conn, Prompt, LOG_BBS);

//	if (user->flags & F_Expert)
//		nodeprintf(conn, "%s\r", ExpertPrompt);
//	else if (conn->NewUser)
//		nodeprintf(conn, "%s\r", NewPrompt);
//	else
//		nodeprintf(conn, "%s\r", Prompt);
}



VOID * _zalloc(int len)
{
	// ?? malloc and clear

	void * ptr;

	ptr=malloc(len);
	memset(ptr, 0, len);

	return ptr;
}


struct UserInfo * FindAMPR()
{
	struct UserInfo * bbs;
	
	for (bbs = BBSChain; bbs; bbs = bbs->BBSNext)
	{		
		if (strcmp(bbs->Call, "AMPR") == 0)
			return bbs;
	}
	
	return NULL;
}

struct UserInfo * FindRMS()
{
	struct UserInfo * bbs;
	
	for (bbs = BBSChain; bbs; bbs = bbs->BBSNext)
	{		
		if (strcmp(bbs->Call, "RMS") == 0)
			return bbs;
	}
	
	return NULL;
}

int CountConnectionsOnPort(int CheckPort)
{
	int n, Count = 0;
	CIRCUIT * conn;
	int port, sesstype, paclen, maxframe, l4window;
	char callsign[11];

	for (n = 0; n < NumberofStreams; n++)
	{
		conn = &Connections[n];
		
		if (conn->Active)
		{
			GetConnectionInfo(conn->BPQStream, callsign, &port, &sesstype, &paclen, &maxframe, &l4window);
			if (port == CheckPort)
				Count++;
		}
	}

	return Count;
}


BOOL CheckRejFilters(char * From, char * To, char * ATBBS)
{
	char ** Calls;

	if (RejFrom && From)
	{
		Calls = RejFrom;

		while(Calls[0])
		{
			if (_stricmp(Calls[0], From) == 0)	
				return TRUE;

			Calls++;
		}
	}

	if (RejTo && To)
	{
		Calls = RejTo;

		while(Calls[0])
		{
			if (_stricmp(Calls[0], To) == 0)	
				return TRUE;

			Calls++;
		}
	}

	if (RejAt && ATBBS)
	{
		Calls = RejAt;

		while(Calls[0])
		{
			if (_stricmp(Calls[0], ATBBS) == 0)	
				return TRUE;

			Calls++;
		}
	}

	return FALSE;		// Ok to accept
}

BOOL CheckHoldFilters(char * From, char * To, char * ATBBS)
{
	char ** Calls;

	if (HoldFrom && From)
	{
		Calls = HoldFrom;

		while(Calls[0])
		{
			if (_stricmp(Calls[0], From) == 0)	
				return TRUE;

			Calls++;
		}
	}

	if (HoldTo && To)
	{
		Calls = HoldTo;

		while(Calls[0])
		{
			if (_stricmp(Calls[0], To) == 0)	
				return TRUE;

			Calls++;
		}
	}

	if (HoldAt && ATBBS)
	{
		Calls = HoldAt;

		while(Calls[0])
		{
			if (_stricmp(Calls[0], ATBBS) == 0)	
				return TRUE;

			Calls++;
		}
	}

	return FALSE;		// Ok to accept
}

BOOL CheckifLocalRMSUser(char * FullTo)
{
	struct UserInfo * user = LookupCall(FullTo);

	if (user)
		if (user->flags & F_POLLRMS)
			return TRUE;

	return FALSE;
		
}



int check_fwd_bit(char *mask, int bbsnumber)
{
	if (bbsnumber)
		return (mask[(bbsnumber - 1) / 8] & (1 << ((bbsnumber - 1) % 8)));
	else
		return 0;
}


void set_fwd_bit(char *mask, int bbsnumber)
{
	if (bbsnumber)
		mask[(bbsnumber - 1) / 8] |= (1 << ((bbsnumber - 1) % 8));
}


void clear_fwd_bit (char *mask, int bbsnumber)
{
	if (bbsnumber)
		mask[(bbsnumber - 1) / 8] &= (~(1 << ((bbsnumber - 1) % 8)));
}

VOID BBSputs(CIRCUIT * conn, char * buf)
{
	// Sends to user and logs

	WriteLogLine(conn, '>',buf,  strlen(buf) -1, LOG_BBS);

	QueueMsg(conn, buf, strlen(buf));
}

VOID __cdecl nodeprintf(ConnectionInfo * conn, const char * format, ...)
{
	char Mess[1000];
	int len;
	va_list(arglist);

	
	va_start(arglist, format);
	len = vsprintf(Mess, format, arglist);

	QueueMsg(conn, Mess, len);

	WriteLogLine(conn, '>',Mess, len-1, LOG_BBS);

	return;
}

// nodeprintfEx add a LF if NEEFLF is set

VOID __cdecl nodeprintfEx(ConnectionInfo * conn, const char * format, ...)
{
	char Mess[1000];
	int len;
	va_list(arglist);

	
	va_start(arglist, format);
	len = vsprintf(Mess, format, arglist);

	QueueMsg(conn, Mess, len);

	WriteLogLine(conn, '>',Mess, len-1, LOG_BBS);

	if (conn->BBSFlags & NEEDLF)
		QueueMsg(conn, "\r", 1);

	return;
}


int compare( const void *arg1, const void *arg2 );

VOID SortBBSChain()
{
	struct UserInfo * user;
	struct UserInfo * users[125]; 
	int i = 0, n;

	// Get array of addresses

	for (user = BBSChain; user; user = user->BBSNext)
	{
		users[i++] = user;
		if (i > 124) break;
	}

	qsort((void *)users, i, 4, compare );

	BBSChain = NULL;

	// Rechain (backwards, as entries ate put on front of chain)

	for (n = i-1; n >= 0; n--)
	{
		users[n]->BBSNext = BBSChain;
		BBSChain = users[n];
	}
}

int compare(const void *arg1, const void *arg2)
{
   // Compare Calls. Fortunately call is at start of stuct

   return _stricmp(*(char**)arg1 , *(char**)arg2);
}

int CountMessagesTo(struct UserInfo * user, int * Unread)
{
	int i, Msgs = 0;
	UCHAR * Call = user->Call;

	*Unread = 0;

	for (i = NumberofMessages; i > 0; i--)
	{
		if (MsgHddrPtr[i]->status == 'K')
			continue;

		if (_stricmp(MsgHddrPtr[i]->to, Call) == 0)
		{
			Msgs++;
			if (MsgHddrPtr[i]->status == 'N')
				*Unread = *Unread + 1;

		}
	}
	
	return(Msgs);
}



// Costimised message handling routines.
/*
	Variables - a subset of those used by FBB

 $C : Number of the next message.
 $I : First name of the connected user.
 $L : Number of the latest message.
 $N : Number of active messages.
 $U : Callsign of the connected user.
 $W : Inserts a carriage return.
 $Z : Last message read by the user (L command).
 %X : Number of messages for the user.
 %x : Number of new messages for the user.
*/

VOID ExpandAndSendMessage(CIRCUIT * conn, char * Msg, int LOG)
{
	char NewMessage[10000];
	char * OldP = Msg;
	char * NewP = NewMessage;
	char * ptr, * pptr;
	int len;
	char Dollar[] = "$";
	char CR[] = "\r";
	char num[20];
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

		case 'L': // Number of the latest message.

			sprintf(num, "%d", LatestMsg);
			pptr = num;
			break;

		case 'N': // Number of active messages.

			sprintf(num, "%d", NumberofMessages);
			pptr = num;
			break;


		case 'U': // Callsign of the connected user.

			pptr = conn->UserPointer->Call;
			break;

		case 'W': // Inserts a carriage return.

			pptr = CR;
			break;

		case 'Z': // Last message read by the user (L command).

			sprintf(num, "%d", conn->UserPointer->lastmsg);
			pptr = num;
			break;

		case 'X': // Number of messages for the user.

			Msgs = CountMessagesTo(conn->UserPointer, &Unread);
			sprintf(num, "%d", Msgs);
			pptr = num;
			break;

		case 'x': // Number of new messages for the user.

			Msgs = CountMessagesTo(conn->UserPointer, &Unread);
			sprintf(num, "%d", Unread);
			pptr = num;
			break;

		case 'F': // Number of new messages to forward to this BBS.

			Msgs = CountMessagestoForward(conn->UserPointer);
			sprintf(num, "%d", Msgs);
			pptr = num;
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
	QueueMsg(conn, NewMessage, len);
}

BOOL isdigits(char * string)
{
	// Returns TRUE id sting is decimal digits

	int i, n = strlen(string);
	
	for (i = 0; i < n; i++)
	{
		if (isdigit(string[i]) == FALSE) return FALSE;
	}
	return TRUE;
}

BOOL wildcardcompare(char * Target, char * Match)
{
	// Do a compare with string *string string* *string*

	// Strings should all be UC

	char Pattern[100];
	char * firststar;

	strcpy(Pattern, Match);
	firststar = strchr(Pattern,'*');

	if (firststar)
	{
		int Len = strlen(Pattern);

		if (Pattern[0] == '*' && Pattern[Len - 1] == '*')		// * at start and end
		{
			Pattern[Len - 1] = 0;
			return (BOOL)(strstr(Target, &Pattern[1]));
		}
		if (Pattern[0] == '*')		// * at start
		{
			// Compare the last len - 1 chars of Target

			int Targlen = strlen(Target);
			int Comparelen = Targlen - (Len - 1);

			if (Len == 1)			// Just *
				return TRUE;

			if (Comparelen < 0)	// Too Short
				return FALSE;

			return (memcmp(&Target[Comparelen], &Pattern[1], Len - 1) == 0);
		}

		// Must be * at end - compare first Len-1 char

		return (memcmp(Target, Pattern, Len - 1) == 0);

	}

	// No WildCards - straight strcmp
	return (strcmp(Target, Pattern) == 0);
}

#ifndef LINBPQ

PrintMessage(HDC hDC, struct MsgInfo * Msg);

PrintMessages(HWND hDlg, int Count, int * Indexes)
{
	int i, CurrentMsgIndex;
	char MsgnoText[10];
	int Msgno;
	struct MsgInfo * Msg;
	int Len = MAX_PATH;
	BOOL hResult;
    PRINTDLG pdx = {0};
	HDC hDC;

//	CHOOSEFONT cf; 
	LOGFONT lf; 
    HFONT hFont; 
 
 
    //  Initialize the PRINTDLG structure.

    pdx.lStructSize = sizeof(PRINTDLG);
    pdx.hwndOwner = hWnd;
    pdx.hDevMode = NULL;
    pdx.hDevNames = NULL;
    pdx.hDC = NULL;
    pdx.Flags = PD_RETURNDC | PD_COLLATE;
    pdx.nMinPage = 1;
    pdx.nMaxPage = 1000;
    pdx.nCopies = 1;
    pdx.hInstance = 0;
    pdx.lpPrintTemplateName = NULL;
    
    //  Invoke the Print property sheet.
    
    hResult = PrintDlg(&pdx);

	memset(&lf, 0, sizeof(LOGFONT));

 /*
 
	// Initialize members of the CHOOSEFONT structure.  
 
    cf.lStructSize = sizeof(CHOOSEFONT); 
    cf.hwndOwner = (HWND)NULL; 
    cf.hDC = pdx.hDC; 
    cf.lpLogFont = &lf; 
    cf.iPointSize = 0; 
    cf.Flags = CF_PRINTERFONTS | CF_FIXEDPITCHONLY; 
    cf.rgbColors = RGB(0,0,0); 
    cf.lCustData = 0L; 
    cf.lpfnHook = (LPCFHOOKPROC)NULL; 
    cf.lpTemplateName = (LPSTR)NULL; 
    cf.hInstance = (HINSTANCE) NULL; 
    cf.lpszStyle = (LPSTR)NULL; 
    cf.nFontType = PRINTER_FONTTYPE; 
    cf.nSizeMin = 0; 
    cf.nSizeMax = 0; 
 
    // Display the CHOOSEFONT common-dialog box.  
 
    ChooseFont(&cf); 
 
    // Create a logical font based on the user's  
    // selection and return a handle identifying  
    // that font. 
*/

	lf.lfHeight =  -56;
	lf.lfWeight = 600;
	lf.lfOutPrecision = 3;
	lf.lfClipPrecision = 2;
	lf.lfQuality = 1;
	lf.lfPitchAndFamily = '1';
	strcpy (lf.lfFaceName, "Courier New");

    hFont = CreateFontIndirect(&lf); 

    if (hResult)
    {
        // User clicked the Print button, so use the DC and other information returned in the 
        // PRINTDLG structure to print the document.

		DOCINFO pdi;

		pdi.cbSize = sizeof(DOCINFO);
		pdi.lpszDocName = "BBS Message Print";
		pdi.lpszOutput = NULL;
		pdi.lpszDatatype = "RAW";
		pdi.fwType = 0;

		hDC = pdx.hDC;

		SelectObject(hDC, hFont);

		StartDoc(hDC, &pdi);
		StartPage(hDC);

		for (i = 0; i < Count; i++)
		{
			SendDlgItemMessage(hDlg, 0, LB_GETTEXT, Indexes[i], (LPARAM)(LPCTSTR)&MsgnoText);
	
			Msgno = atoi(MsgnoText);

			for (CurrentMsgIndex = 1; CurrentMsgIndex <= NumberofMessages; CurrentMsgIndex++)
			{
				Msg = MsgHddrPtr[CurrentMsgIndex];
	
				if (Msg->number == Msgno)
				{
					PrintMessage(hDC, Msg);
					break;
				}
			}
		}
		
		EndDoc(hDC);
	}

    if (pdx.hDevMode != NULL) 
        GlobalFree(pdx.hDevMode); 
    if (pdx.hDevNames != NULL) 
        GlobalFree(pdx.hDevNames); 

    if (pdx.hDC != NULL) 
        DeleteDC(pdx.hDC);

	return 0;
}

PrintMessage(HDC hDC, struct MsgInfo * Msg)
{
	int Len = MAX_PATH;
	char * MsgBytes;
	char * Save;
  	int Msglen;
 
	StartPage(hDC);

	Save = MsgBytes = ReadMessageFile(Msg->number);

	Msglen = Msg->length;

	if (MsgBytes)
	{
		char Hddr[1000];
		char FullTo[100];
		int HRes, VRes;
		char * ptr1, * ptr2;
		int LineLen;

		RECT Rect;

		if (_stricmp(Msg->to, "RMS") == 0)
			 sprintf(FullTo, "RMS:%s", Msg->via);
		else
		if (Msg->to[0] == 0)
			sprintf(FullTo, "smtp:%s", Msg->via);
		else
			strcpy(FullTo, Msg->to);


		sprintf(Hddr, "From: %s%s\r\nTo: %s\r\nType/Status: %c%c\r\nDate/Time: %s\r\nBid: %s\r\nTitle: %s\r\n\r\n",
			Msg->from, Msg->emailfrom, FullTo, Msg->type, Msg->status, FormatDateAndTime(Msg->datecreated, FALSE), Msg->bid, Msg->title);


		if (Msg->B2Flags)
		{
			// Remove B2 Headers (up to the File: Line)
			
			char * ptr;
			ptr = strstr(MsgBytes, "Body:");
			if (ptr)
			{
				Msglen = atoi(ptr + 5);
				ptr = strstr(ptr, "\r\n\r\n");
			}
			if (ptr)
				MsgBytes = ptr + 4;
		}

		HRes = GetDeviceCaps(hDC, HORZRES) - 50;
		VRes = GetDeviceCaps(hDC, VERTRES) - 50;

		Rect.top = 50;
		Rect.left = 50;
		Rect.right = HRes;
		Rect.bottom = VRes;

		DrawText(hDC, Hddr, strlen(Hddr), &Rect, DT_CALCRECT | DT_WORDBREAK);
		DrawText(hDC, Hddr, strlen(Hddr), &Rect, DT_WORDBREAK);

		// process message a line at a time. When page is full, output a page break

		ptr1 = MsgBytes;
		ptr2 = ptr1;

		while (Msglen-- > 0)
		{	
			if (*ptr1++ == '\r')
			{
				// Output this line

				// First check if it will fit

				Rect.top = Rect.bottom;
				Rect.right = HRes;
				Rect.bottom = VRes;

				LineLen = ptr1 - ptr2 - 1;
			
				if (LineLen == 0)		// Blank line
					Rect.bottom = Rect.top + 40;
				else
					DrawText(hDC, ptr2, ptr1 - ptr2 - 1, &Rect, DT_CALCRECT | DT_WORDBREAK);

				if (Rect.bottom >= VRes)
				{
					EndPage(hDC);
					StartPage(hDC);

					Rect.top = 50;
					Rect.bottom = VRes;
					if (LineLen == 0)		// Blank line
						Rect.bottom = Rect.top + 40;
					else
						DrawText(hDC, ptr2, ptr1 - ptr2 - 1, &Rect, DT_CALCRECT | DT_WORDBREAK);
				}

				if (LineLen == 0)		// Blank line
					Rect.bottom = Rect.top + 40;
				else
					DrawText(hDC, ptr2, ptr1 - ptr2 - 1, &Rect, DT_WORDBREAK);

				if (*(ptr1) == '\n')
				{
					ptr1++;
					Msglen--;
				}

				ptr2 = ptr1;
			}
		}
	
		free(Save);
	
		EndPage(hDC);

		}
		return 0;
}

#endif


int ImportMessages(CIRCUIT * conn, char * FN, BOOL Nopopup)
{
	char FileName[MAX_PATH] = "Messages.in";
	int Files = 0;
	int WriteLen=0;
	FILE *in;
	CIRCUIT dummyconn;
	struct UserInfo User;	
	int Index = 0;
			
	char Buffer[100000];
	char *buf = Buffer;

	if (FN[0])			// Name supplled
		strcpy(FileName, FN);

	else
	{
#ifndef LINBPQ
		OPENFILENAME Ofn; 

		memset(&Ofn, 0, sizeof(Ofn));
 
		Ofn.lStructSize = sizeof(OPENFILENAME); 
		Ofn.hInstance = hInst;
		Ofn.hwndOwner = MainWnd; 
		Ofn.lpstrFilter = NULL; 
		Ofn.lpstrFile= FileName; 
		Ofn.nMaxFile = sizeof(FileName)/ sizeof(*FileName); 
		Ofn.lpstrFileTitle = NULL; 
		Ofn.nMaxFileTitle = 0; 
		Ofn.lpstrInitialDir = BaseDir; 
		Ofn.Flags = OFN_SHOWHELP | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST; 
		Ofn.lpstrTitle = NULL;//; 

		if (!GetOpenFileName(&Ofn))
			return 0;
#endif
	}

	in = fopen(FileName, "rb");

	if (!(in))
	{
		char msg[500];
		sprintf_s(msg, sizeof(msg), "Failed to open %s", FileName);
		if (conn)
			nodeprintf(conn, "%s\r", msg);
#ifdef WIN32
		else
			if (Nopopup == FALSE)
				MessageBox(NULL, msg, "BPQMailChat", MB_OK);
#endif
		return 0;
	}

	memset(&dummyconn, 0, sizeof(CIRCUIT));
	memset(&User, 0, sizeof(struct UserInfo));

	if (conn == 0)
	{	
		conn = &dummyconn;

		dummyconn.UserPointer = &User;	// Was SYSOPCall, but I think that is wrong.
		strcpy(User.Call, "IMPORT");
		dummyconn.sysop = TRUE;
		dummyconn.BBSFlags = BBS;
		strcpy(dummyconn.Callsign, "IMPORT");
	}

	while(fgets(Buffer, 99999, in))
	{
		// First line should start SP/SB ?ST?

		char * From = NULL;
		char * BID = NULL;
		char * ATBBS = NULL;
		char seps[] = " \t\r";
		struct MsgInfo * Msg;
		char SMTPTO[100]= "";
		int msglen;
		char * Context;
		char * Arg1, * Cmd;

NextMessage:

		Sleep(100);

		strlop(Buffer, 10);
		strlop(Buffer, 13);				// Remove cr and/or lf

		if (Buffer[0] == 0)			//Blank Line
			continue;

		WriteLogLine(conn, '>', Buffer, strlen(Buffer), LOG_BBS);

		if (dummyconn.sysop == 0)
		{
			nodeprintf(conn, "%s\r", Buffer);
			Flush(conn);
		}
 
		Cmd = strtok_s(Buffer, seps, &Context);

		if (Cmd == NULL)
		{
			fclose(in);
			return Files;
		}

		Arg1 = strtok_s(NULL, seps, &Context);

		if (Arg1 == NULL)
		{
			if (dummyconn.sysop)
				Debugprintf("Bad Import Line %s", Buffer);
			else
				nodeprintf(conn, "Bad Import Line %s\r", Buffer);

			fclose(in);
			return Files;
		}

		if (DecodeSendParams(conn, Context, &From, &Arg1, &ATBBS, &BID))
		{
			if (CreateMessage(conn, From, Arg1, ATBBS, toupper(Cmd[1]), BID, NULL))
			{
				Msg = conn->TempMsg;

				// SP is Ok, read message;

				ClearQueue(conn);

				fgets(Buffer, 99999, in);
				strlop(Buffer, 10);
				strlop(Buffer, 13);				// Remove cr and/or lf
				strcpy(Msg->title, Buffer);

				// Read the lines
		
				conn->Flags |= GETTINGMESSAGE;

				Buffer[0] = 0;

				fgets(Buffer, 99999, in);

				while ((conn->Flags & GETTINGMESSAGE) && Buffer[0])
				{
					strlop(Buffer, 10);
					strlop(Buffer, 13);				// Remove cr and/or lf
					msglen = strlen(Buffer);
					Buffer[msglen++] = 13;
					ProcessMsgLine(conn, conn->UserPointer,Buffer, msglen);
	
					Buffer[0] = 0;
					fgets(Buffer, 99999, in);
				}

				// Message completed (or off end of file)

				Files ++;

				ClearQueue(conn);
		
				if (Buffer[0])
					goto NextMessage;		// We have read the SP/SB line;
				else
				{
					fclose(in);
					return Files;
				}
			}
			else
			{
				// Create failed
			
				Flush(conn);
			}
		}

		// Search for next message 

		Buffer[0] = 0;
		fgets(Buffer, 99999, in);

		while (Buffer[0])
		{
			strlop(Buffer, 10);
			strlop(Buffer, 13);				// Remove cr and/or lf

			if (_stricmp(Buffer, "/EX") == 0)
			{
				// Found end

				Buffer[0] = 0;
				fgets(Buffer, 99999, in);
			
				if (dummyconn.sysop)
					ClearQueue(conn);
				else
					Flush(conn);

				if (Buffer[0])
					goto NextMessage;		// We have read the SP/SB line;
			}
			
			Buffer[0] = 0;
			fgets(Buffer, 99999, in);
		}
	}

	fclose(in);

	if (dummyconn.sysop)
		ClearQueue(conn);
	else
		Flush(conn);

	return Files;
}
char * ReadMessageFileEx(struct MsgInfo * MsgRec)
{
	// Sets Message Size from File Size

	int msgno = MsgRec->number;
	int FileSize;
	char MsgFile[MAX_PATH];
	FILE * hFile;
	char * MsgBytes;
	struct stat STAT;
 
	sprintf_s(MsgFile, sizeof(MsgFile), "%s/m_%06d.mes", MailDir, msgno);

	if (stat(MsgFile, &STAT) == -1)
		return NULL;

	FileSize = STAT.st_size;

	hFile = fopen(MsgFile, "rb");

	if (hFile == NULL)
		return NULL;

	MsgBytes=malloc(FileSize+1);

	fread(MsgBytes, 1, FileSize, hFile); 

	fclose(hFile);

	MsgBytes[FileSize]=0;
	MsgRec->length = FileSize;

	return MsgBytes;
}

char * ReadMessageFile(int msgno)
{
	int FileSize;
	char MsgFile[MAX_PATH];
	FILE * hFile;
	char * MsgBytes;
	struct stat STAT;
 
	sprintf_s(MsgFile, sizeof(MsgFile), "%s/m_%06d.mes", MailDir, msgno);

	if (stat(MsgFile, &STAT) == -1)
		return NULL;

	FileSize = STAT.st_size;

	hFile = fopen(MsgFile, "rb");

	if (hFile == NULL)
		return NULL;

	MsgBytes=malloc(FileSize+1);

	fread(MsgBytes, 1, FileSize, hFile); 

	fclose(hFile);

	MsgBytes[FileSize]=0;

	return MsgBytes;
}


int QueueMsg(ConnectionInfo * conn, char * msg, int len)
{
	// Add Message to queue for this connection

	//	UCHAR * OutputQueue;		// Messages to user
	//	int OutputQueueLength;		// Total Malloc'ed size. Also Put Pointer for next Message
	//	int OutputGetPointer;		// Next byte to send. When Getpointer = Quele Length all is sent - free the buffer and start again.

	// Create or extend buffer

	GetSemaphore(&OutputSEM, 0);

	conn->OutputQueue=realloc(conn->OutputQueue, conn->OutputQueueLength + len);

	if (conn->OutputQueue == NULL)
	{
		// relloc failed - should never happen, but clean up

		CriticalErrorHandler("realloc failed to expand output queue");
		FreeSemaphore(&OutputSEM);
		return 0;
	}

	memcpy(&conn->OutputQueue[conn->OutputQueueLength], msg, len);
	conn->OutputQueueLength += len;
	FreeSemaphore(&OutputSEM);

	return len;
}

void TrytoSend()
{
	// call Flush on any connected streams with queued data

	ConnectionInfo * conn;
	struct ConsoleInfo * Cons;

	int n;

	for (n = 0; n < NumberofStreams; n++)
	{
		conn = &Connections[n];
		
		if (conn->Active == TRUE)
		{
			Flush(conn);

			// if an FLARQ mail has been sent see if queues have cleared

			if (conn->OutputQueue == NULL && (conn->BBSFlags & ARQMAILACK))
			{
				int n = TXCount(conn->BPQStream);		// All Sent and Acked?
	
				if (n == 0)
				{
					struct MsgInfo * Msg = conn->FwdMsg;

					conn->ARQClearCount--;

					if (conn->ARQClearCount <= 0)
					{
						Logprintf(LOG_BBS, conn, '>', "ARQ Send Complete");

						// Mark mail as sent, and look for more
	
						clear_fwd_bit(Msg->fbbs, conn->UserPointer->BBSNumber);
						set_fwd_bit(Msg->forw, conn->UserPointer->BBSNumber);

						//  Only mark as forwarded if sent to all BBSs that should have it
			
						if (memcmp(Msg->fbbs, zeros, NBMASK) == 0)
						{
							Msg->status = 'F';			// Mark as forwarded
							Msg->datechanged=time(NULL);
						}
	
						conn->BBSFlags &= ~ARQMAILACK;
						conn->UserPointer->ForwardingInfo->MsgCount--;

						SendARQMail(conn);				// See if any more - close if not
					}
				}
				else
					conn->ARQClearCount = 10;
			}
		}
	}
#ifndef LINBPQ
	for (Cons = ConsHeader[0]; Cons; Cons = Cons->next)
	{
		if (Cons->Console)
			Flush(Cons->Console);
	}
#endif
}


void Flush(CIRCUIT * conn)
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
		if (TXCount(conn->BPQStream) > 15)
			return;						// Busy

		if (conn->Paging && (conn->LinesSent >= conn->PageLen))
			return;

		if (tosend <= conn->paclen)
			len=tosend;
		else
			len=conn->paclen;

		GetSemaphore(&OutputSEM, 0);

		if (conn->Paging)
		{
			// look for CR chars in message to send. Increment LinesSent, and stop if at limit

			UCHAR * ptr1 = &conn->OutputQueue[conn->OutputGetPointer];
			UCHAR * ptr2;
			int lenleft = len;

			ptr2 = memchr(ptr1, 0x0d, len);

			while (ptr2)
			{
				conn->LinesSent++;
				ptr2++;
				lenleft = len - (ptr2 - ptr1);

				if (conn->LinesSent >= conn->PageLen)
				{
					len = ptr2 - &conn->OutputQueue[conn->OutputGetPointer];
					
					SendUnbuffered(conn->BPQStream, &conn->OutputQueue[conn->OutputGetPointer], len);
					conn->OutputGetPointer+=len;
					tosend-=len;
					SendUnbuffered(conn->BPQStream, "<A>bort, <CR> Continue..>", 25);
					FreeSemaphore(&OutputSEM);
					return;

				}
				ptr2 = memchr(ptr2, 0x0d, lenleft);
			}
		}

		SendUnbuffered(conn->BPQStream, &conn->OutputQueue[conn->OutputGetPointer], len);

		conn->OutputGetPointer+=len;

		FreeSemaphore(&OutputSEM);

		tosend-=len;	
		sent++;

		if (sent > 15)
			return;
	}

	// All Sent. Free buffers and reset pointers

	conn->LinesSent = 0;

	ClearQueue(conn);
}

VOID ClearQueue(ConnectionInfo * conn)
{
	if (conn->OutputQueue == NULL)
		return;

	GetSemaphore(&OutputSEM, 0);
	
	free(conn->OutputQueue);

	conn->OutputQueue=NULL;
	conn->OutputGetPointer=0;
	conn->OutputQueueLength=0;

	FreeSemaphore(&OutputSEM);
}



VOID FlagAsKilled(struct MsgInfo * Msg)
{
	struct UserInfo * user;

	Msg->status='K';
	Msg->datechanged=time(NULL);

	// Remove any forwarding references

	if (memcmp(Msg->fbbs, zeros, NBMASK) != 0)
	{	
		for (user = BBSChain; user; user = user->BBSNext)
		{
			if (check_fwd_bit(Msg->fbbs, user->BBSNumber))
			{
				user->ForwardingInfo->MsgCount--;
				clear_fwd_bit(Msg->fbbs, user->BBSNumber);
			}
		}
	}
	SaveMessageDatabase();
}

void DoDeliveredCommand(CIRCUIT * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context)
{
	int msgno=-1;
	struct MsgInfo * Msg;
	
	while (Arg1)
	{
		msgno = atoi(Arg1);
				
		if (msgno > 100000)
		{
			BBSputs(conn, "Message Number too high\r");
			return;
		}

		Msg = GetMsgFromNumber(msgno);

		if (Msg == NULL)
		{
			nodeprintf(conn, "Message %d not found\r", msgno);
			goto Next;
		}

		if (Msg->type != 'T')
		{
			nodeprintf(conn, "Message %d not an NTS Message\r", msgno);
			goto Next;
		}

		if (Msg->status == 'N')
		{
			nodeprintf(conn, "Message %d has not been read\r", msgno);
			goto Next;
		}

		Msg->status = 'D';
		Msg->datechanged=time(NULL);

		nodeprintf(conn, "Message #%d Flagged as Delivered\r", msgno);
	Next:
		Arg1 = strtok_s(NULL, " \r", &Context);
	}

	return;
}

void DoUnholdCommand(CIRCUIT * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context)
{
	int msgno=-1;
	int i;
	struct MsgInfo * Msg;
	
	// Param is either ALL or a list of numbers

	if (Arg1 == NULL)
	{
		nodeprintf(conn, "No message number\r");
		return;
	}

	if (_stricmp(Arg1, "ALL") == 0)
	{
		for (i=NumberofMessages; i>0; i--)
		{
			Msg = MsgHddrPtr[i];

			if (Msg->status == 'H')
			{
				if (Msg->type == 'B' && memcmp(Msg->fbbs, zeros, NBMASK) != 0)
					Msg->status = '$';				// Has forwarding
				else
					Msg->status = 'N';
				
				nodeprintf(conn, "Message #%d Unheld\r", Msg->number);
			}
		}
		return;
	}

	while (Arg1)
	{
		msgno = atoi(Arg1);
		Msg = GetMsgFromNumber(msgno);
		
		if (Msg)
		{
			if (Msg->status == 'H')
			{
				if (Msg->type == 'B' && memcmp(Msg->fbbs, zeros, NBMASK) != 0)
					Msg->status = '$';				// Has forwarding
				else
					Msg->status = 'N';

				nodeprintf(conn, "Message #%d Unheld\r", msgno);
			}
			else
			{
				nodeprintf(conn, "Message #%d was not held\r", msgno);
			}
		}
		else
				nodeprintf(conn, "Message #%d not found\r", msgno);
		
		Arg1 = strtok_s(NULL, " \r", &Context);
	}

	return;
}

void DoKillCommand(CIRCUIT * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context)
{
	int msgno=-1;
	int i;
	struct MsgInfo * Msg;
	
	switch (toupper(Cmd[1]))
	{

	case 0:					// Just K

		while (Arg1)
		{
			msgno = atoi(Arg1);
			KillMessage(conn, user, msgno);

			Arg1 = strtok_s(NULL, " \r", &Context);
		}

		return;

	case 'M':					// Kill Mine

		for (i=NumberofMessages; i>0; i--)
		{
			Msg = MsgHddrPtr[i];

			if ((_stricmp(Msg->to, user->Call) == 0) || (conn->sysop && _stricmp(Msg->to, "SYSOP") == 0 && user->flags & F_SYSOP_IN_LM))
			{
				if (Msg->type == 'P' && Msg->status == 'Y')
				{
					FlagAsKilled(Msg);
					nodeprintf(conn, "Message #%d Killed\r", Msg->number);
				}
			}
		}
		return;

	case 'H':					// Kill Held

		if (conn->sysop)
		{
			for (i=NumberofMessages; i>0; i--)
			{
				Msg = MsgHddrPtr[i];

				if (Msg->status == 'H')
				{
					FlagAsKilled(Msg);
					nodeprintf(conn, "Message #%d Killed\r", Msg->number);
				}
			}
		}
		return;

	case '>':			// K> - Kill to 

		if (conn->sysop)
		{
			if (Arg1)
				if (KillMessagesTo(conn, user, Arg1) == 0)
				BBSputs(conn, "No Messages found\r");
		
			return;
		}

	case '<':

		if (conn->sysop)
		{
			if (Arg1)
				if (KillMessagesFrom(conn, user, Arg1) == 0);
					BBSputs(conn, "No Messages found\r");

					return;
		}
	}

	nodeprintf(conn, "*** Error: Invalid Kill option %c\r", Cmd[1]);

	return;

}

int KillMessagesTo(ConnectionInfo * conn, struct UserInfo * user, char * Call)
{
	int i, Msgs = 0;
	struct MsgInfo * Msg;

	for (i=NumberofMessages; i>0; i--)
	{
		Msg = MsgHddrPtr[i];
		if (Msg->status != 'K' && _stricmp(Msg->to, Call) == 0)
		{
			Msgs++;
			KillMessage(conn, user, MsgHddrPtr[i]->number);
		}
	}
	
	return(Msgs);
}

int KillMessagesFrom(ConnectionInfo * conn, struct UserInfo * user, char * Call)
{
	int i, Msgs = 0;
	struct MsgInfo * Msg;


	for (i=NumberofMessages; i>0; i--)
	{
		Msg = MsgHddrPtr[i];
		if (Msg->status != 'K' && _stricmp(Msg->from, Call) == 0)
		{
			Msgs++;
			KillMessage(conn, user, MsgHddrPtr[i]->number);
		}
	}
	
	return(Msgs);
}

BOOL OkToKillMessage(BOOL SYSOP, char * Call, struct MsgInfo * Msg)
{	
	if (SYSOP || Msg->type == 'T') return TRUE;
	
	if (Msg->type == 'P')
		if ((_stricmp(Msg->to, Call) == 0) || (_stricmp(Msg->from, Call) == 0))
			return TRUE;

	if (Msg->type == 'B')
		if (_stricmp(Msg->from, Call) == 0)
			return TRUE;

	return FALSE;
}

void KillMessage(ConnectionInfo * conn, struct UserInfo * user, int msgno)
{
	struct MsgInfo * Msg;

	Msg = GetMsgFromNumber(msgno);

	if (Msg == NULL || Msg->status == 'K')
	{
		nodeprintf(conn, "Message %d not found\r", msgno);
		return;
	}

	if (OkToKillMessage(conn->sysop, user->Call, Msg))
	{
		FlagAsKilled(Msg);
		nodeprintf(conn, "Message #%d Killed\r", msgno);
	}
	else
		nodeprintf(conn, "Not your message\r");
}


BOOL ListMessage(struct MsgInfo * Msg, ConnectionInfo * conn, BOOL SendFullFrom)
{
	char FullFrom[80];
	char FullTo[80];
	struct TempUserInfo * Temp = conn->UserPointer->Temp;

	strcpy(FullFrom, Msg->from);

	if ((_stricmp(Msg->from, "RMS:") == 0) || (_stricmp(Msg->from, "SMTP:") == 0) || 
		SendFullFrom || (_stricmp(Msg->emailfrom, "@winlink.org") == 0))
		strcat(FullFrom, Msg->emailfrom);

	if (_stricmp(Msg->to, "RMS") == 0)
	{
		sprintf(FullTo, "RMS:%s", Msg->via);
		nodeprintf(conn, "%-6d %s %c%c   %5d %-7s %-6s %-s\r",
				Msg->number, FormatDateAndTime(Msg->datecreated, TRUE), Msg->type, Msg->status, Msg->length, FullTo, FullFrom, Msg->title);
	}
	else

	if (Msg->to[0] == 0 && Msg->via[0] != 0)
	{
		sprintf(FullTo, "smtp:%s", Msg->via);
		nodeprintf(conn, "%-6d %s %c%c   %5d %-7s %-6s %-s\r",
				Msg->number, FormatDateAndTime(Msg->datecreated, TRUE), Msg->type, Msg->status, Msg->length, FullTo, FullFrom, Msg->title);
	}

	else
		if (Msg->via[0] != 0)
		{
			char Via[80];
			strcpy(Via, Msg->via);
			strlop(Via, '.');			// Only show first part of via
			nodeprintf(conn, "%-6d %s %c%c   %5d %-7s@%-6s %-6s %-s\r",
				Msg->number, FormatDateAndTime(Msg->datecreated, TRUE), Msg->type, Msg->status, Msg->length, Msg->to, Via, FullFrom, Msg->title);
		}
		else
		nodeprintf(conn, "%-6d %s %c%c   %5d %-7s        %-6s %-s\r",
				Msg->number, FormatDateAndTime(Msg->datecreated, TRUE), Msg->type, Msg->status, Msg->length, Msg->to, FullFrom, Msg->title);
	
	//	if paging, stop two before page lengh. This lets us send the continue prompt, save status
	//	and exit without triggering the system paging code. We can then read a message then resume listing

	if (Temp->ListActive && conn->Paging)
	{
		Temp->LinesSent++;

		if ((Temp->LinesSent + 1) >= conn->PageLen)
		{
			nodeprintf(conn, "<A>bort, <R Msg(s)>, <CR> = Continue..>");
			Temp->LastListedInPagedMode = Msg->number;
			Temp->ListSuspended = TRUE;
			return TRUE;
		}
	}

	return FALSE;
}

void DoListCommand(ConnectionInfo * conn, struct UserInfo * user, char * Cmd, char * Arg1, BOOL Resuming)
{
	BOOL SendFullFrom = Cmd[2];
	int Start = 0;
	struct  TempUserInfo * Temp = user->Temp;
	char ListType = 0;

	if (conn->Paging)
	{
		if (Temp->ListSuspended)
		{
			// We have reentered list command after a pause. The next message to list is in Temp->LastListedInPagedMode

			Start = Temp->LastListedInPagedMode;
			Temp->ListSuspended = FALSE;
		}


		user->Temp->ListActive = TRUE;
		memcpy(user->Temp->LastListCommand, Cmd, 79);
		if (Arg1)
			memcpy(user->Temp->LastListParams, Arg1, 79);
		else
			user->Temp->LastListParams[0] = 0;
		
		user->Temp->LinesSent = 0;
	}

	// Allow compound selection, eg LTN or LFP

	if (Cmd[1] == 0)
		Cmd[1] = '*';

	_strupr(Cmd);

	if (strchr(Cmd, 'T'))
	{
		ListType = 'T';
		if (Cmd[1] == 'T')		// We want LNT not LTN
			memmove(&Cmd[1], &Cmd[2], strlen(&Cmd[1]));  // Need Null
	}	
	else
	if (strchr(Cmd, 'P'))
	{
		ListType = 'P';
		if (Cmd[1] == 'P')		// We want LNT not LTN
			memmove(&Cmd[1], &Cmd[2], strlen(&Cmd[1]));  // Need Null
	}	
	else
	if (strchr(Cmd, 'B'))
	{
		ListType = 'B';
		if (Cmd[1] =='B')		// We want LNT not LTN
			memmove(&Cmd[1], &Cmd[2], strlen(&Cmd[1]));  // Need Null
	}	
	
	switch (Cmd[1])
	{

	case '*':					// Just L
	case 'R':				// LR = List Reverse

		if (Arg1)
		{
			// Range nnn-nnn or single value

			char * Arg2, * Arg3;
			char * Context;
			char seps[] = " -\t\r";
			UINT From=LatestMsg, To=0;
			char * Range = strchr(Arg1, '-');
			
			Arg2 = strtok_s(Arg1, seps, &Context);
			Arg3 = strtok_s(NULL, seps, &Context);

			if (Arg2)
				To = From = atoi(Arg2);

			if (Arg3)
				From = atoi(Arg3);
			else
				if (Range)
					From = LatestMsg;

			if (From > 100000 || To > 100000)
			{
				BBSputs(conn, "Message Number too high\r");
				return;
			}

			if (Cmd[1] == 'R')
			{
				if (Start)
					To = Start + 1;

				ListMessagesInRangeForwards(conn, user, user->Call, From, To, SendFullFrom);
			}
			else
			{
				if (Start)
					From = Start - 1;

				ListMessagesInRange(conn, user, user->Call, From, To, SendFullFrom);
			}
		}
		else

			if (LatestMsg == conn->lastmsg)
				BBSputs(conn, "No New Messages\r");
			else if (Cmd[1] == 'R')
				ListMessagesInRangeForwards(conn, user, user->Call, LatestMsg, conn->lastmsg + 1, SendFullFrom);
			else
				ListMessagesInRange(conn, user, user->Call, LatestMsg, conn->lastmsg + 1, SendFullFrom);

			conn->lastmsg = LatestMsg;

		return;


	case 'L':				// List Last

		if (Arg1)
		{
			int i = atoi(Arg1);
			int m = NumberofMessages;

			if (Resuming)
				i = Temp->LLCount;
			else
				Temp->LLCount = i;
				
			for (; i>0 && m != 0; i--)
			{
				m = GetUserMsg(m, user->Call, conn->sysop);

				if (m > 0)
				{
					if (Start && MsgHddrPtr[m]->number >= Start)
					{
						m--;
						i++;
						continue;
					}

					Temp->LLCount--;
					
					if (ListMessage(MsgHddrPtr[m], conn, SendFullFrom))
						return;			// Hit page limit
					m--;
				}
			}
		}
		return;

	case 'M':			// LM - List Mine

		if (ListMessagesTo(conn, user, user->Call, SendFullFrom, Start) == 0)
			BBSputs(conn, "No Messages found\r");
		return;

	case '>':			// L> - List to 

		if (Arg1)
			if (ListMessagesTo(conn, user, Arg1, SendFullFrom, Start) == 0)
				BBSputs(conn, "No Messages found\r");
		
		
		return;

	case '<':

		if (Arg1)
			if (ListMessagesFrom(conn, user, Arg1, SendFullFrom, Start) == 0)
				BBSputs(conn, "No Messages found\r");
		
		return;

	case '@':

		if (Arg1)
			if (ListMessagesAT(conn, user, Arg1, SendFullFrom, Start) == 0)
				BBSputs(conn, "No Messages found\r");
		
		return;

	case 'N':
	case 'Y':
	case 'F':
	case '$':
	case 'D':			// Delivered NTS Traffic can be listed by anyone
		{
			int m = NumberofMessages;
				
			while (m > 0)
			{
				m = GetUserMsg(m, user->Call, conn->sysop);

				if (m > 0)
				{
					if (Start && MsgHddrPtr[m]->number >= Start)
					{
						m--;
						continue;
					}
			
					if (ListType)
					{
						if (MsgHddrPtr[m]->status == Cmd[1] && MsgHddrPtr[m]->type == ListType)
							if (ListMessage(MsgHddrPtr[m], conn, SendFullFrom))
								return;			// Hit page limit
					}
					else
					{
						if (MsgHddrPtr[m]->status == toupper(Cmd[1]))
							if (ListMessage(MsgHddrPtr[m], conn, SendFullFrom))
								return;			// Hit page limit
					}
					m--;
				}
			}
		}
		return;

	case 'K':
	case 'H':				// List Status

		if (conn->sysop)
		{
			int i, Msgs = Start;

			for (i=NumberofMessages; i>0; i--)
			{
				if (Start && MsgHddrPtr[i]->number >= Start)
					continue;

				if (MsgHddrPtr[i]->status == toupper(Cmd[1]))
				{
					Msgs++;
					if (ListMessage(MsgHddrPtr[i], conn, SendFullFrom))
						return;			// Hit page limit

				}
			}

			if (Msgs == 0)
				BBSputs(conn, "No Messages found\r");
		}
		else
				BBSputs(conn, "LH or LK can only be used by SYSOP\r");

		return;

	case 'C':
	{
		struct NNTPRec * ptr = FirstNNTPRec;
		char Cat[100];
		char NextCat[100];
		int Line = 0;
		int Count;

		while (ptr)
		{
			// if the next name is the same, combine  counts
			
			strcpy(Cat, ptr->NewsGroup);
			strlop(Cat, '.');
			Count = ptr->Count;
		Catloop:
			if (ptr->Next)
			{
				strcpy(NextCat, ptr->Next->NewsGroup);
				strlop(NextCat, '.');
				if (strcmp(Cat, NextCat) == 0)
				{
					ptr = ptr->Next;
					Count += ptr->Count;
					goto Catloop;
				}
			}

			nodeprintf(conn, "%-6s %-3d", Cat, Count);
			Line += 10;
			if (Line > 80)
			{
				Line = 0;
				nodeprintf(conn, "\r");
			}
			
			ptr = ptr->Next;
		}

		if (Line)
			nodeprintf(conn, "\r\r");
		else
			nodeprintf(conn, "\r");

		return;
	}
	}
	
	// Could be P B or T if specified without a status

	switch (ListType)
	{
	case 'P':
	case 'B':
	case 'T':			// NTS Traffic can be listed by anyone
	{
			int m = NumberofMessages;
							
			while (m > 0)
			{
				m = GetUserMsg(m, user->Call, conn->sysop);

				if (m > 0)
				{
					if (Start && MsgHddrPtr[m]->number >= Start)
					{
						m--;
						continue;
					}

					if (MsgHddrPtr[m]->type == ListType)
						if (ListMessage(MsgHddrPtr[m], conn, SendFullFrom))
							return;			// Hit page limit
					m--;
				}
			}

			return;
		}
	}

	
	nodeprintf(conn, "*** Error: Invalid List option %c\r", Cmd[1]);


}	
int ListMessagesTo(ConnectionInfo * conn, struct UserInfo * user, char * Call, BOOL SendFullFrom, int Start)
{
	int i, Msgs = Start;

	for (i=NumberofMessages; i>0; i--)
	{
		if (MsgHddrPtr[i]->status == 'K')
			continue;

		if (Start && MsgHddrPtr[i]->number >= Start)
			continue;

		if ((_stricmp(MsgHddrPtr[i]->to, Call) == 0) ||
			((conn->sysop) && _stricmp(Call, SYSOPCall) == 0 &&
			_stricmp(MsgHddrPtr[i]->to, "SYSOP") == 0 && (user->flags & F_SYSOP_IN_LM)))
		{
			Msgs++;
			if (ListMessage(MsgHddrPtr[i], conn, SendFullFrom))
				break;			// Hit page limit
		}
	}
	
	return(Msgs);
}

int ListMessagesFrom(ConnectionInfo * conn, struct UserInfo * user, char * Call, BOOL SendFullFrom, int Start)
{
	int i, Msgs = 0;

	for (i=NumberofMessages; i>0; i--)
	{
		if (MsgHddrPtr[i]->status == 'K')
			continue;

		if (MsgHddrPtr[i]->number <= Start)
			continue;

		if (_stricmp(MsgHddrPtr[i]->from, Call) == 0)
		{
			Msgs++;
			if (ListMessage(MsgHddrPtr[i], conn, SendFullFrom))
				return Msgs;			// Hit page limit

		}
	}
	
	return(Msgs);
}

int ListMessagesAT(ConnectionInfo * conn, struct UserInfo * user, char * Call, BOOL SendFullFrom,int Start)
{
	int i, Msgs = 0;

	for (i=NumberofMessages; i>0; i--)
	{
		if (MsgHddrPtr[i]->status == 'K')
			continue;

		if (MsgHddrPtr[i]->number <= Start)
			continue;

		if (_memicmp(MsgHddrPtr[i]->via, Call, strlen(Call)) == 0 ||
			(_stricmp(Call, "SMTP:") == 0 && MsgHddrPtr[i]->to[0] == 0))
		{
			Msgs++;
			if (ListMessage(MsgHddrPtr[i], conn, SendFullFrom))
				break;			// Hit page limit
		}
	}
	
	return(Msgs);
}
int GetUserMsg(int m, char * Call, BOOL SYSOP)
{
	struct MsgInfo * Msg;
	
	// Get Next (usually backwards) message which should be shown to this user
	//	ie Not Deleted, and not Private unless to or from Call

	do
	{
		Msg=MsgHddrPtr[m];

		if (SYSOP) return m;			// Sysop can list or read anything
		
		if (Msg->status != 'K')
		{
	
			if (Msg->status != 'H')
			{
				if (Msg->type == 'B' || Msg->type == 'T') return m;

				if (Msg->type == 'P')
				{
					if ((_stricmp(Msg->to, Call) == 0) || (_stricmp(Msg->from, Call) == 0))
						return m;
				}
			}
		}

		m--;

	} while (m> 0);

	return 0;
}

BOOL CheckUserMsg(struct MsgInfo * Msg, char * Call, BOOL SYSOP)
{
	// Return TRUE if user is allowed to read message
	
	if (SYSOP) return TRUE;			// Sysop can list or read anything

	if ((Msg->status != 'K') && (Msg->status != 'H'))
	{
		if (Msg->type == 'B' || Msg->type == 'T') return TRUE;

		if (Msg->type == 'P')
		{
			if ((_stricmp(Msg->to, Call) == 0) || (_stricmp(Msg->from, Call) == 0))
				return TRUE;
		}
	}

	return FALSE;
}

int GetUserMsgForwards(int m, char * Call, BOOL SYSOP)
{
	struct MsgInfo * Msg;
	
	// Get Next (usually backwards) message which should be shown to this user
	//	ie Not Deleted, and not Private unless to or from Call

	do
	{
		Msg=MsgHddrPtr[m];
		
		if (Msg->status != 'K')
		{
			if (SYSOP) return m;			// Sysop can list or read anything

			if (Msg->status != 'H')
			{
				if (Msg->type == 'B' || Msg->type == 'T') return m;

				if (Msg->type == 'P')
				{
					if ((_stricmp(Msg->to, Call) == 0) || (_stricmp(Msg->from, Call) == 0))
						return m;
				}
			}
		}

		m++;

	} while (m <= NumberofMessages);

	return 0;

}


void ListMessagesInRange(ConnectionInfo * conn, struct UserInfo * user, char * Call, int Start, int End, BOOL SendFullFrom)
{
	int m;
	struct MsgInfo * Msg;

	for (m = Start; m >= End; m--)
	{
		Msg = GetMsgFromNumber(m);
		
		if (Msg && CheckUserMsg(Msg, user->Call, conn->sysop))
			if (ListMessage(Msg, conn, SendFullFrom))
				return;			// Hit page limit

	}
}


void ListMessagesInRangeForwards(ConnectionInfo * conn, struct UserInfo * user, char * Call, int End, int Start, BOOL SendFullFrom)
{
	int m;
	struct MsgInfo * Msg;

	for (m = Start; m <= End; m++)
	{
		Msg = GetMsgFromNumber(m);
		
		if (Msg && CheckUserMsg(Msg, user->Call, conn->sysop))
			if (ListMessage(Msg, conn, SendFullFrom))
				return;			// Hit page limit
	}
}


void DoReadCommand(CIRCUIT * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context)
{
	int msgno=-1;
	int i;
	struct MsgInfo * Msg;

	
	switch (toupper(Cmd[1]))
	{
	case 0:					// Just R

		while (Arg1)
		{
			msgno = atoi(Arg1);
			if (msgno > 100000)
			{
				BBSputs(conn, "Message Number too high\r");
				return;
			}

			ReadMessage(conn, user, msgno);
			Arg1 = strtok_s(NULL, " \r", &Context);
		}

		return;

	case 'M':					// Read Mine (Unread Messages)

		for (i=NumberofMessages; i>0; i--)
		{
			Msg = MsgHddrPtr[i];

			if ((_stricmp(Msg->to, user->Call) == 0) || (conn->sysop && _stricmp(Msg->to, "SYSOP") == 0 && user->flags & F_SYSOP_IN_LM))
				if (Msg->status == 'N')
					ReadMessage(conn, user, Msg->number);
		}

		return;
	}
	
	nodeprintf(conn, "*** Error: Invalid Read option %c\r", Cmd[1]);
	
	return;
}

int RemoveLF(char * Message, int len)
{
	// Remove lf chars

	char * ptr1, * ptr2;

	ptr1 = ptr2 = Message;

	while (len-- > 0)
	{
		*ptr2 = *ptr1;
	
		if (*ptr1 == '\r')
			if (*(ptr1+1) == '\n')
			{
				ptr1++;
				len--;
			}
		ptr1++;
		ptr2++;
	}

	return (ptr2 - Message);
}

void ReadMessage(ConnectionInfo * conn, struct UserInfo * user, int msgno)
{
	struct MsgInfo * Msg;
	char * MsgBytes, * Save;
	char FullTo[100];
	int Index;

	Msg = GetMsgFromNumber(msgno);

	if (Msg == NULL)
	{
		nodeprintf(conn, "Message %d not found\r", msgno);
		return;
	}

	if (!CheckUserMsg(Msg, user->Call, conn->sysop))
	{
		nodeprintf(conn, "Message %d not for you\r", msgno);
		return;
	}

	if (_stricmp(Msg->to, "RMS") == 0)
		 sprintf(FullTo, "RMS:%s", Msg->via);
	else
	if (Msg->to[0] == 0)
		sprintf(FullTo, "smtp:%s", Msg->via);
	else
		strcpy(FullTo, Msg->to);


	nodeprintf(conn, "From: %s%s\rTo: %s\rType/Status: %c%c\rDate/Time: %s\rBid: %s\rTitle: %s\r\r",
		Msg->from, Msg->emailfrom, FullTo, Msg->type, Msg->status, FormatDateAndTime(Msg->datecreated, FALSE), Msg->bid, Msg->title);

	MsgBytes = Save = ReadMessageFile(msgno);

	if (Msg->type == 'P')
		Index = PMSG;
	else if (Msg->type == 'B')
		Index = BMSG;
	else if (Msg->type == 'T')
		Index = TMSG;

	if (MsgBytes)
	{
		int Length;

		if (Msg->B2Flags)
		{
			char * ptr;
	
			// if message has attachments, display them if plain text

			if (Msg->B2Flags & Attachments)
			{
				char * FileName[100];
				int FileLen[100];
				int Files = 0;
				int BodyLen, NewLen;
				int i;
				char *ptr2;		
				char Msg[512];
				int Len;
		
				ptr = MsgBytes;
	
				Len = sprintf(Msg, "Message has Attachments\r\r");
				QueueMsg(conn, Msg, Len);

				while(*ptr != 13)
				{
					ptr2 = strchr(ptr, 10);	// Find CR

					if (memcmp(ptr, "Body: ", 6) == 0)
					{
						BodyLen = atoi(&ptr[6]);
					}

					if (memcmp(ptr, "File: ", 6) == 0)
					{
						char * ptr1 = strchr(&ptr[6], ' ');	// Find Space

						FileLen[Files] = atoi(&ptr[6]);

						FileName[Files++] = &ptr1[1];
						*(ptr2 - 1) = 0;
					}
				
					ptr = ptr2;
					ptr++;
				}

				ptr += 2;			// Over Blank Line and Separator 

				NewLen = RemoveLF(ptr, BodyLen);

				QueueMsg(conn, ptr, NewLen);		// Display Body

				ptr += BodyLen + 2;		// to first file

				for (i = 0; i < Files; i++)
				{
					char Msg[512];
					int Len, n;
					char * p = ptr;
					char c;

					// Check if message is probably binary

					int BinCount = 0;

					NewLen = RemoveLF(ptr, FileLen[i]);		// Removes LF agter CR but not on its own

					for (n = 0; n < NewLen; n++)
					{
						c = *p;
						
						if (c == 10)
							*p = 13;

						if (c==0 || (c & 128))
							BinCount++;

						p++;

					}

					if (BinCount > NewLen/10)
					{
						// File is probably Binary

						Len = sprintf(Msg, "\rAttachment %s is a binary file\r", FileName[i]);
						QueueMsg(conn, Msg, Len);
					}
					else
					{
						Len = sprintf(Msg, "\rAttachment %s\r\r", FileName[i]);
						QueueMsg(conn, Msg, Len);

						user->Total.MsgsSent[Index] ++;
						user->Total.BytesForwardedOut[Index] += NewLen;

						QueueMsg(conn, ptr, NewLen);
					}
				
					ptr += FileLen[i];
					ptr +=2;				// Over separator
				}
				return;
			}
			
			// Remove B2 Headers (up to the File: Line)
			
			ptr = strstr(MsgBytes, "Body:");

			if (ptr)
				MsgBytes = ptr;
		}

		// Remove lf chars

		Length = RemoveLF(MsgBytes, strlen(MsgBytes));

		user->Total.MsgsSent[Index] ++;
		user->Total.BytesForwardedOut[Index] += Length;

		QueueMsg(conn, MsgBytes, Length);
		free(Save);

		nodeprintf(conn, "\r\r[End of Message #%d from %s]\r", msgno, Msg->from);

		if ((_stricmp(Msg->to, user->Call) == 0) || ((conn->sysop) && (_stricmp(Msg->to, "SYSOP") == 0)))
		{
			if ((Msg->status != 'K') && (Msg->status != 'H') && (Msg->status != 'F') && (Msg->status != 'D'))
			{
				if (Msg->status != 'Y')
				{
					Msg->status = 'Y';
					Msg->datechanged=time(NULL);
				}
			}
		}
	}
	else
	{
		nodeprintf(conn, "File for Message %d not found\r", msgno);
	}

}
 struct MsgInfo * FindMessage(char * Call, int msgno, BOOL sysop)
 {
	int m=NumberofMessages;

	struct MsgInfo * Msg;

	do
	{
		m = GetUserMsg(m, Call, sysop);

		if (m == 0)
			return NULL;

		Msg=MsgHddrPtr[m];

		if (Msg->number == msgno)
			return Msg;

		m--;

	} while (m> 0);

	return NULL;

}


char * ReadInfoFile(char * File)
{
	int FileSize;
	char MsgFile[MAX_PATH];
	FILE * hFile;
	char * MsgBytes;
	struct stat STAT;
	char * ptr1 = 0;
 
	sprintf_s(MsgFile, sizeof(MsgFile), "%s/%s", BaseDir, File);

	if (stat(MsgFile, &STAT) == -1)
		return NULL;

	FileSize = STAT.st_size;

	hFile = fopen(MsgFile, "rb");

	if (hFile == NULL)
		return NULL;

	MsgBytes=malloc(FileSize+1);

	fread(MsgBytes, 1, FileSize, hFile); 

	fclose(hFile);

	MsgBytes[FileSize]=0;

#ifndef WIN32

	// Replace LF with CR

	// Remove lf chars

	ptr1 = MsgBytes;

	while (*ptr1)
	{
		if (*ptr1 == '\n')
			*(ptr1) = '\r';

		ptr1++;
	}
#endif

	return MsgBytes;
}

char * FormatDateAndTime(time_t Datim, BOOL DateOnly)
{
	struct tm *tm;
	static char Date[]="xx-xxx hh:mmZ";

	tm = gmtime(&Datim);
	
	if (tm)
		sprintf_s(Date, sizeof(Date), "%02d-%3s %02d:%02dZ",
					tm->tm_mday, month[tm->tm_mon], tm->tm_hour, tm->tm_min);

	if (DateOnly)
	{
		Date[6]=0;
		return Date;
	}
	
	return Date;
}

BOOL DecodeSendParams(CIRCUIT * conn, char * Context, char ** From, char ** To, char ** ATBBS, char ** BID);


BOOL DoSendCommand(CIRCUIT * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context)
{
	// SB WANT @ ALLCAN < N6ZFJ $4567_N0ARY
	
	char * From = NULL;
	char * BID = NULL;
	char * ATBBS = NULL;
	char seps[] = " \t\r";
	struct MsgInfo * OldMsg;
	char OldTitle[62];
	char NewTitle[62];
	char SMTPTO[100]= "";
	int msgno;

	if (Cmd[1] == 0) Cmd[1] ='P'; // Just S means SP

	switch (toupper(Cmd[1]))
	{
	case 'B':

		if (RefuseBulls)
		{
			nodeprintf(conn, "*** Error: This system doesn't allow sending Bulls\r");
			return FALSE;
		}

		if (user->flags & F_NOBULLS)
		{
			nodeprintf(conn, "*** Error: You are not allowed to send Bulls\r");
			return FALSE;
		}
		

	case 'P':
	case 'T':
				
		if (Arg1 == NULL)
		{
			nodeprintf(conn, "*** Error: The 'TO' callsign is missing\r");
			return FALSE;
		}

		if (!DecodeSendParams(conn, Context, &From, &Arg1, &ATBBS, &BID))
			return FALSE;

		return CreateMessage(conn, From, Arg1, ATBBS, toupper(Cmd[1]), BID, NULL);	

	case 'R':
				
		if (Arg1 == NULL)
		{
			nodeprintf(conn, "*** Error: Message Number is missing\r");
			return FALSE;
		}

		msgno = atoi(Arg1);

		if (msgno > 100000)
		{
			BBSputs(conn, "Message Number too high\r");
			return FALSE;
		}

		OldMsg = FindMessage(user->Call, msgno, conn->sysop);

		if (OldMsg == NULL)
		{
			nodeprintf(conn, "Message %d not found\r", msgno);
			return FALSE;
		}

		Arg1=&OldMsg->from[0];

		if (_stricmp(Arg1, "SMTP:") == 0 || _stricmp(Arg1, "RMS:") == 0 || OldMsg->emailfrom)
		{
			// SMTP message. Need to get the real sender from the message

			sprintf(SMTPTO, "%s%s", Arg1, OldMsg->emailfrom);

			Arg1 = SMTPTO;
		}

		if (!DecodeSendParams(conn, "", &From, &Arg1, &ATBBS, &BID))
			return FALSE;

		strcpy(OldTitle, OldMsg->title);

		if (strlen(OldTitle) > 57) OldTitle[57] = 0;

		strcpy(NewTitle, "Re:");
		strcat(NewTitle, OldTitle);

		return CreateMessage(conn, From, Arg1, ATBBS, 'P', BID, NewTitle);	

		return TRUE;

	case 'C':
				
		if (Arg1 == NULL)
		{
			nodeprintf(conn, "*** Error: Message Number is missing\r");
			return FALSE;
		}

		msgno = atoi(Arg1);

		if (msgno > 100000)
		{
			BBSputs(conn, "Message Number too high\r");
			return FALSE;
		}

		Arg1 = strtok_s(NULL, seps, &Context);

		if (Arg1 == NULL)
		{
			nodeprintf(conn, "*** Error: The 'TO' callsign is missing\r");
			return FALSE;
		}

		if (!DecodeSendParams(conn, Context, &From, &Arg1, &ATBBS, &BID))
			return FALSE;
	
		OldMsg = FindMessage(user->Call, msgno, conn->sysop);

		if (OldMsg == NULL)
		{
			nodeprintf(conn, "Message %d not found\r", msgno);
			return FALSE;
		}

		strcpy(OldTitle, OldMsg->title);

		if (strlen(OldTitle) > 56) OldTitle[56] = 0;

		strcpy(NewTitle, "Fwd:");
		strcat(NewTitle, OldTitle);

		conn->CopyBuffer = ReadMessageFile(msgno);

		return CreateMessage(conn, From, Arg1, ATBBS, 'P', BID, NewTitle);	
	}


	nodeprintf(conn, "*** Error: Invalid Send option %c\r", Cmd[1]);

	return FALSE;
}

char * CheckToAddress(CIRCUIT * conn, char * Addr)
{
	// Check one element of Multiple Address

	if (!(conn->BBSFlags & BBS))
	{
		// if a normal user, check that TO and/or AT are known and warn if not.

		if (_stricmp(Addr, "SYSOP") == 0)
		{
			return _strdup(Addr);
		}

		if (SendBBStoSYSOPCall)
			if (_stricmp(Addr, BBSName) == 0)
				return _strdup(SYSOPCall);
	

		if (strchr(Addr, '@') == 0)
		{
			// No routing, if not a user and not known to forwarding or WP warn

			struct UserInfo * ToUser = LookupCall(Addr);

			if (ToUser)
			{
				// Local User. If Home BBS is specified, use it

				if (ToUser->HomeBBS[0])
				{
					char * NewAddr = malloc(250);
					nodeprintf(conn, "Address %s - @%s added from HomeBBS\r", Addr, ToUser->HomeBBS);
					sprintf(NewAddr, "%s@%s", Addr, ToUser->HomeBBS);
					return NewAddr;
				}
			}
			else
			{
				WPRecP WP = LookupWP(Addr);

				if (WP)
				{
					char * NewAddr = malloc(250);

					nodeprintf(conn, "Address %s - @%s added from WP\r", Addr, WP->first_homebbs);
					sprintf(NewAddr, "%s@%s", Addr, WP->first_homebbs);
					return NewAddr;
				}
			}
		}
	}

	// Check SMTP and RMS Addresses

	if ((_memicmp(Addr, "rms:", 4) == 0) || (_memicmp(Addr, "rms/", 4) == 0))
	{
		Addr[3] = ':';			// Replace RMS/ with RMS:
			
		if (!FindRMS())
		{
			nodeprintf(conn, "*** Error - Forwarding via RMS is not configured on this BBS\r");
			return FALSE;
		}
	}
	else if ((_memicmp(Addr, "smtp:", 5) == 0) || (_memicmp(Addr, "smtp/", 5) == 0))
	{
		Addr[4] = ':';			// Replace smpt/ with smtp:

		if (ISP_Gateway_Enabled)
		{
			if ((conn->UserPointer->flags & F_EMAIL) == 0)
			{
				nodeprintf(conn, "*** Error - You need to ask the SYSOP to allow you to use Internet Mail\r");
				return FALSE;
			}
		}
		else
		{
			nodeprintf(conn, "*** Error - Sending mail to smtp addresses is disabled\r");
			return FALSE;
		}
	}

	return _strdup(Addr);
}


BOOL DecodeSendParams(CIRCUIT * conn, char * Context, char ** From, char ** To, char ** ATBBS, char ** BID)
{
	char * ptr;
	char seps[] = " \t\r";
	WPRecP WP;
	char * Addr = *To;
	int Len;

	conn->ToCount = 0;

	// SB WANT @ ALLCAN < N6ZFJ $4567_N0ARY

	if (strchr(Context, ';') || strchr(Addr, ';'))
	{
		// Multiple Addresses - put address list back together

		Addr[strlen(Addr)] = ' ';
		Context = Addr;

		while (strchr(Context, ';'))
		{
			// Multiple Addressees

			Addr = strtok_s(NULL, ";", &Context);
			Len = strlen(Addr);
			conn->To = realloc(conn->To, (conn->ToCount+1)*4);
			if (conn->To[conn->ToCount] = CheckToAddress(conn, Addr))
				conn->ToCount++;
		}

		Addr = strtok_s(NULL, seps, &Context);

		Len = strlen(Addr);
		conn->To=realloc(conn->To, (conn->ToCount+1)*4);
		if (conn->To[conn->ToCount] = CheckToAddress(conn, Addr))
			conn->ToCount++;
	}
	else
	{
		// Single Call

		// Accept call@call (without spaces) - but check for smtp addresses

		if (_memicmp(*To, "smtp:", 5) != 0 && _memicmp(*To, "rms:", 4) != 0  && _memicmp(*To, "rms/", 4) != 0)
		{
			ptr = strchr(*To, '@');

			if (ptr)
			{
				*ATBBS = strlop(*To, '@');
			}
		}
	}

	// Look for Optional fields;

	ptr = strtok_s(NULL, seps, &Context);

	while (ptr)
	{
		if (strcmp(ptr, "@") == 0)
		{
			*ATBBS = _strupr(strtok_s(NULL, seps, &Context));
		}
		else if(strcmp(ptr, "<") == 0)
		{			
			*From = strtok_s(NULL, seps, &Context);
		}
		else if (ptr[0] == '$')
			*BID = &ptr[1];
		else
		{
			nodeprintf(conn, "*** Error: Invalid Format\r");
			return FALSE;
		}
		ptr = strtok_s(NULL, seps, &Context);
	}

	// Only allow < from a BBS

	if (*From)
	{
		if (!(conn->BBSFlags & BBS))
		{
			nodeprintf(conn, "*** < can only be used by a BBS\r");
			return FALSE;
		}
	}

	if (!*From)
		*From = conn->UserPointer->Call;

	if (!(conn->BBSFlags & BBS))
	{
		// if a normal user, check that TO and/or AT are known and warn if not.

		if (_stricmp(*To, "SYSOP") == 0)
		{
			conn->LocalMsg = TRUE;
			return TRUE;
		}

		if (!*ATBBS && conn->ToCount == 0)
		{
			// No routing, if not a user and not known to forwarding or WP warn

			struct UserInfo * ToUser = LookupCall(*To);

			if (ToUser)
			{
				// Local User. If Home BBS is specified, use it

				if (ToUser->HomeBBS[0])
				{
					*ATBBS = ToUser->HomeBBS;
					nodeprintf(conn, "Address @%s added from HomeBBS\r", *ATBBS);
				}
				else
				{
					conn->LocalMsg = TRUE;
				}
			}
			else
			{
				conn->LocalMsg = FALSE;
				WP = LookupWP(*To);

				if (WP)
				{
					*ATBBS = WP->first_homebbs;
					nodeprintf(conn, "Address @%s added from WP\r", *ATBBS);
				}
			}
		}
	}
	return TRUE;
}

BOOL CreateMessage(CIRCUIT * conn, char * From, char * ToCall, char * ATBBS, char MsgType, char * BID, char * Title)
{
	struct MsgInfo * Msg, * TestMsg;
	char * via = NULL;
	char * FromHA;

	// Create a temp msg header entry

	if (conn->ToCount)
	{
	}
	else
	{
		if (CheckRejFilters(From, ToCall, ATBBS))
		{	
			if ((conn->BBSFlags & BBS))
			{
				nodeprintf(conn, "NO - REJECTED\r");
				if (conn->BBSFlags & OUTWARDCONNECT)
					nodeprintf(conn, "F>\r");				// if Outward connect must be reverse forward
				else
					nodeprintf(conn, ">\r");
			}
			else
				nodeprintf(conn, "*** Error - Message Filters prevent sending this message\r");

			return FALSE;
		}
	}

	Msg = malloc(sizeof (struct MsgInfo));

	if (Msg == 0)
	{
		CriticalErrorHandler("malloc failed for new message header");
		return FALSE;
	}
	
	memset(Msg, 0, sizeof (struct MsgInfo));

	conn->TempMsg = Msg;

	Msg->type = MsgType;
	
	if (conn->UserPointer->flags & F_HOLDMAIL)
		Msg->status = 'H';
	else
		Msg->status = 'N';
	
	Msg->datereceived = Msg->datechanged = Msg->datecreated = time(NULL);

	if (BID)
	{
		BIDRec * TempBID;

		// If P Message, dont immediately reject on a Duplicate BID. Check if we still have the message
		//	If we do, reject  it. If not, accept it again. (do we need some loop protection ???)

		TempBID = LookupBID(BID);
	
		if (TempBID)
		{
			if (MsgType == 'B')
			{
				// Duplicate bid
	
				if ((conn->BBSFlags & BBS))
				{
					nodeprintf(conn, "NO - BID\r");
					if (conn->BBSFlags & OUTWARDCONNECT)
						nodeprintf(conn, "F>\r");				// if Outward connect must be reverse forward
					else
						nodeprintf(conn, ">\r");
				}
				else
					nodeprintf(conn, "*** Error- Duplicate BID\r");

				return FALSE;
			}

			TestMsg = GetMsgFromNumber(TempBID->u.msgno);
 
			// if the same TO we will assume the same message

			if (TestMsg && strcmp(TestMsg->to, ToCall) == 0)
			{
				// We have this message. If we have already forwarded it, we should accept it again

				if ((TestMsg->status == 'N') || (TestMsg->status == 'Y')|| (TestMsg->status == 'H'))
				{
					// Duplicate bid
	
					if ((conn->BBSFlags & BBS))
					{
						nodeprintf(conn, "NO - BID\r");
						if (conn->BBSFlags & OUTWARDCONNECT)
							nodeprintf(conn, "F>\r");				// if Outward connect must be reverse forward
						else
							nodeprintf(conn, ">\r");
					}
					else
						nodeprintf(conn, "*** Error- Duplicate BID\r");

					return FALSE;
				}
			}
		}
		
		if (strlen(BID) > 12) BID[12] = 0;
		strcpy(Msg->bid, BID);

		// Save BID in temp list in case we are offered it again before completion
			
		TempBID = AllocateTempBIDRecord();
		strcpy(TempBID->BID, BID);
		TempBID->u.conn = conn;

	}

	if (conn->ToCount)
	{
	}
	else
	{
		if (_memicmp(ToCall, "rms:", 4) == 0)
		{
			if (!FindRMS())
			{
				nodeprintf(conn, "*** Error - Forwarding via RMS is not configured on this BBS\r");
				return FALSE;
			}

			via=strlop(ToCall, ':');
			_strupr(ToCall);
		}
		else if (_memicmp(ToCall, "rms/", 4) == 0)
		{
			if (!FindRMS())
			{
				nodeprintf(conn, "*** Error - Forwarding via RMS is not configured on this BBS\r");
				return FALSE;
			}

			via=strlop(ToCall, '/');
			_strupr(ToCall);
		}
		else if (_memicmp(ToCall, "smtp:", 5) == 0)
		{
			if (ISP_Gateway_Enabled)
			{
				if ((conn->UserPointer->flags & F_EMAIL) == 0)
				{
					nodeprintf(conn, "*** Error - You need to ask the SYSOP to allow you to use Internet Mail\r");
					return FALSE;
				}
				via=strlop(ToCall, ':');
				ToCall[0] = 0;
			}
			else
			{
				nodeprintf(conn, "*** Error - Sending mail to smtp addresses is disabled\r");
				return FALSE;
			}
		}
		else
		{
			_strupr(ToCall);
			if (ATBBS)
				via=_strupr(ATBBS);
		}

		strlop(ToCall, '-');						// Remove any (illegal) ssid
		if (strlen(ToCall) > 6) ToCall[6] = 0;
	
		strcpy(Msg->to, ToCall);
		
		if (SendBBStoSYSOPCall)
			if (_stricmp(ToCall, BBSName) == 0)
				strcpy(Msg->to, SYSOPCall);

		if (via)
		{
			if (strlen(via) > 40) via[40] = 0;

			strcpy(Msg->via, via);
		}

	}		// End of Multiple Dests

	// Look for HA in From (even if we shouldn't be getting it!)

	FromHA = strlop(From, '@');


	strlop(From, '-');						// Remove any (illegal) ssid
	if (strlen(From) > 6) From[6] = 0;
	strcpy(Msg->from, From);

	if (FromHA)
	{
		if (strlen(FromHA) > 39) FromHA[39] = 0;
		Msg->emailfrom[0] = '@';
		strcpy(&Msg->emailfrom[1], _strupr(FromHA));
	}

	if (Title)					// Only used by SR and SC
	{
		strcpy(Msg->title, Title);
		conn->Flags |= GETTINGMESSAGE;

		// Create initial buffer of 10K. Expand if needed later

		conn->MailBuffer=malloc(10000);
		conn->MailBufferSize=10000;

		nodeprintf(conn, "Enter Message Text (end with /ex or ctrl/z)\r");
		return TRUE;
	}

	if (conn->BBSFlags & FLARQMODE)
		return TRUE;

	if (!(conn->BBSFlags & FBBCompressed))
		conn->Flags |= GETTINGTITLE;

	if (!(conn->BBSFlags & BBS))
		nodeprintf(conn, "Enter Title (only):\r");
	else
		if (!(conn->BBSFlags & FBBForwarding))
			nodeprintf(conn, "OK\r");

	return TRUE;
}

VOID ProcessMsgTitle(ConnectionInfo * conn, struct UserInfo * user, char* Buffer, int msglen)
{
		
	conn->Flags &= ~GETTINGTITLE;

	if (msglen == 1)
	{
		nodeprintf(conn, "*** Message Cancelled\r");
		SendPrompt(conn, user);
		return;
	}

	if (msglen > 60) msglen = 60;

	Buffer[msglen-1] = 0;

	strcpy(conn->TempMsg->title, Buffer);

	// Create initial buffer of 10K. Expand if needed later

	conn->MailBuffer=malloc(10000);
	conn->MailBufferSize=10000;

	if (conn->MailBuffer == NULL)
	{
		nodeprintf(conn, "Failed to create Message Buffer\r");
		return;
	}

	conn->Flags |= GETTINGMESSAGE;

	if (!conn->BBSFlags & BBS)
		nodeprintf(conn, "Enter Message Text (end with /ex or ctrl/z)\r");

}

VOID ProcessMsgLine(CIRCUIT * conn, struct UserInfo * user, char* Buffer, int msglen)
{
	char * ptr2 = NULL;

	if (((msglen < 3) && (Buffer[0] == 0x1a)) || ((msglen == 4) && (_memicmp(Buffer, "/ex", 3) == 0)))
	{
		int Index = 0;
			
		if (conn->TempMsg->type == 'P')
			Index = PMSG;
		else if (conn->TempMsg->type == 'B')
			Index = BMSG;
		else if (conn->TempMsg->type == 'T')
			Index = TMSG;
		
		conn->Flags &= ~GETTINGMESSAGE;

		user->Total.MsgsReceived[Index]++;
		user->Total.BytesForwardedIn[Index] += conn->TempMsg->length;

		if (conn->ToCount)
		{
			// Multiple recipients

			struct MsgInfo * Msg = conn->TempMsg;
			int i;
			struct MsgInfo * SaveMsg = Msg;
			char * SaveBody = conn->MailBuffer;
			int SaveMsgLen = Msg->length; 
			BOOL SentToRMS = FALSE;
			int ToLen = 0;
			char * ToString = zalloc(conn->ToCount * 100);

			// If no BID provided, allocate one
			
			if (Msg->bid[0] == 0)
				sprintf_s(Msg->bid, sizeof(Msg->bid), "%d_%s", LatestMsg + 1, BBSName);

			for (i = 0; i < conn->ToCount; i++)
			{
				char * Addr = conn->To[i];
				char * Via;

				if (_memicmp (Addr, "SMTP:", 5) == 0)
				{
					// For Email

					conn->TempMsg = Msg = malloc(sizeof(struct MsgInfo));
					memcpy(Msg, SaveMsg, sizeof(struct MsgInfo));
	
					conn->MailBuffer = malloc(SaveMsgLen + 10);
					memcpy(conn->MailBuffer, SaveBody, SaveMsgLen);

					Msg->to[0] = 0;
					strcpy(Msg->via, &Addr[5]);

					CreateMessageFromBuffer(conn);
					continue;
				}

				if (_memicmp (Addr, "RMS:", 4) == 0)
				{
					// Add to B2 Message for RMS

					Addr+=4;
					
					Via = strlop(Addr, '@');
				
					if (Via && _stricmp(Via, "winlink.org") == 0)
					{
						if (CheckifLocalRMSUser(Addr))
						{
							// Local RMS - Leave Here
	
							Via = 0;							// Drop Through
							goto PktMsg;
						}
						else
						{
							ToLen = sprintf(ToString, "%sTo: %s\r\n", ToString, Addr);
							continue;
						}
					}

					ToLen = sprintf(ToString, "%sTo: %s@%s\r\n", ToString, Addr, Via);
					continue;
				}

				_strupr(Addr);
				
				Via = strlop(Addr, '@');

				if (Via && _stricmp(Via, "winlink.org") == 0)
				{
					if (CheckifLocalRMSUser(Addr))
					{
						// Local RMS - Leave Here

						Via = 0;							// Drop Through
					}
					else
					{
						ToLen = sprintf(ToString, "%sTo: %s\r\n", ToString, Addr);

						// Add to B2 Message for RMS

						continue;
					}
				}

			PktMsg:		
				
				conn->LocalMsg = FALSE;

				// Normal BBS Message

				if (_stricmp(Addr, "SYSOP") == 0)
					conn->LocalMsg = TRUE;
				else
				{
					struct UserInfo * ToUser = LookupCall(Addr);

					if (ToUser)
						conn->LocalMsg = TRUE;
				}

				conn->TempMsg = Msg = malloc(sizeof(struct MsgInfo));
				memcpy(Msg, SaveMsg, sizeof(struct MsgInfo));
	
				conn->MailBuffer = malloc(SaveMsgLen + 10);
				memcpy(conn->MailBuffer, SaveBody, SaveMsgLen);

				strcpy(Msg->to, Addr);

				if (Via)
				{
					Msg->bid[0] = 0;					// if we are forwarding it, we must change BID to be safe
					strcpy(Msg->via, Via);
				}

				CreateMessageFromBuffer(conn);
			}

			if (ToLen)
			{
				char * B2Hddr = zalloc(ToLen + 1000);
				int B2HddrLen;
				char DateString[80];
				struct tm * tm;
				time_t Date = time(NULL);

				tm = gmtime(&Date);	
	
				sprintf(DateString, "%04d/%02d/%02d %02d:%02d",
					tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);

				conn->TempMsg = Msg = malloc(sizeof(struct MsgInfo));
				memcpy(Msg, SaveMsg, sizeof(struct MsgInfo));
	
				conn->MailBuffer = malloc(SaveMsgLen + 1000 + ToLen);

				Msg->B2Flags = B2Msg;

				B2HddrLen = sprintf(B2Hddr,
					"MID: %s\r\nDate: %s\r\nType: %s\r\nFrom: %s\r\n%sSubject: %s\r\nMbo: %s\r\nBody: %d\r\n\r\n",
					SaveMsg->bid, DateString, "Private",
					SaveMsg->from, ToString, SaveMsg->title, BBSName, SaveMsgLen);

				memcpy(conn->MailBuffer, B2Hddr, B2HddrLen);
				memcpy(&conn->MailBuffer[B2HddrLen], SaveBody, SaveMsgLen);

				Msg->length += B2HddrLen;

				strcpy(Msg->to, "RMS");

				CreateMessageFromBuffer(conn);

				free(B2Hddr);
			}

			free(SaveMsg);
			free(SaveBody);
			conn->MailBuffer = NULL;
			conn->MailBufferSize=0;

			if (!(conn->BBSFlags & BBS))
				SendPrompt(conn, conn->UserPointer);
			else
				if (!(conn->BBSFlags & FBBForwarding))
				{
					if (conn->BBSFlags & OUTWARDCONNECT)
						BBSputs(conn, "F>\r");				// if Outward connect must be reverse forward
					else
						BBSputs(conn, ">\r");
				}

			/*
			// From a client - Create one copy with all RMS recipients, and another for each packet recipient	

			// Merge all RMS To: lines 

			ToLen = 0;
			ToString[0] = 0;

			for (i = 0; i < Recipients; i++)
			{
				if (LocalMsg[i])
					continue;						// For a local RMS user
				
				if (_stricmp(Via[i], "WINLINK.ORG") == 0 || _memicmp (&HddrTo[i][4], "SMTP:", 5) == 0 ||
					_stricmp(RecpTo[i], "RMS") == 0)
				{
					ToLen += strlen(HddrTo[i]);
					strcat(ToString, HddrTo[i]);
				}
			}

			if (ToLen)
			{
				conn->TempMsg = Msg = malloc(sizeof(struct MsgInfo));
				memcpy(Msg, SaveMsg, sizeof(struct MsgInfo));
	
				conn->MailBuffer = malloc(SaveMsgLen + 1000);
				memcpy(conn->MailBuffer, SaveBody, SaveMsgLen);


				memmove(&conn->MailBuffer[B2To + ToLen], &conn->MailBuffer[B2To], count);
				memcpy(&conn->MailBuffer[B2To], ToString, ToLen); 

				conn->TempMsg->length += ToLen;

				strcpy(Msg->to, "RMS");
				strcpy(Msg->via, "winlink.org");

				// Must Change the BID

				Msg->bid[0] = 0;

				CreateMessageFromBuffer(conn);
			}

			}

			free(ToString);

			for (i = 0; i < Recipients; i++)
			{
				// Only Process Non - RMS Dests or local RMS Users

				if (LocalMsg[i] == 0)
					if (_stricmp (Via[i], "WINLINK.ORG") == 0 ||
						_memicmp (&HddrTo[i][4], "SMTP:", 5) == 0 ||
						_stricmp(RecpTo[i], "RMS") == 0)		
					continue;

				conn->TempMsg = Msg = malloc(sizeof(struct MsgInfo));
				memcpy(Msg, SaveMsg, sizeof(struct MsgInfo));
	
				conn->MailBuffer = malloc(SaveMsgLen + 1000);
				memcpy(conn->MailBuffer, SaveBody, SaveMsgLen);

				// Add our To: 

				ToLen = strlen(HddrTo[i]);

				if (_memicmp(HddrTo[i], "CC", 2) == 0)	// Replace CC: with TO:
					memcpy(HddrTo[i], "To", 2);

				memmove(&conn->MailBuffer[B2To + ToLen], &conn->MailBuffer[B2To], count);
				memcpy(&conn->MailBuffer[B2To], HddrTo[i], ToLen); 

				conn->TempMsg->length += ToLen;

				strcpy(Msg->to, RecpTo[i]);
				strcpy(Msg->via, Via[i]);
				
				Msg->bid[0] = 0;

				CreateMessageFromBuffer(conn);
			}
			}	// End not from RMS

			free(SaveMsg);
			free(SaveBody);
			conn->MailBuffer = NULL;
			conn->MailBufferSize=0;

			SetupNextFBBMessage(conn);
			return;
	
			} My__except_Routine("Process Multiple Destinations");

			BBSputs(conn, "*** Program Error Processing Multiple Destinations\r");
			Flush(conn);
			conn->CloseAfterFlush = 20;			// 2 Secs

			return;
*/

			conn->ToCount = 0;

			return;
		}


		CreateMessageFromBuffer(conn);
		return;

	}

	Buffer[msglen++] = 0x0a;

	if ((conn->TempMsg->length + msglen) > conn->MailBufferSize)
	{
		conn->MailBufferSize += 10000;
		conn->MailBuffer = realloc(conn->MailBuffer, conn->MailBufferSize);
	
		if (conn->MailBuffer == NULL)
		{
			nodeprintf(conn, "Failed to extend Message Buffer\r");

			conn->Flags &= ~GETTINGMESSAGE;
			return;
		}
	}

	memcpy(&conn->MailBuffer[conn->TempMsg->length], Buffer, msglen);

	conn->TempMsg->length += msglen;
}

VOID CreateMessageFromBuffer(CIRCUIT * conn)
{
	struct MsgInfo * Msg;
	BIDRec * BIDRec;
	char * ptr1, * ptr2 = NULL;
	char * ptr3, * ptr4;
	int FWDCount;
	char OldMess[] = "\r\n\r\nOriginal Message:\r\n\r\n";
	int Age, OurCount;
	char * HoldReason = "User has Hold Messages flag set";


#ifndef LINBPQ
	struct _EXCEPTION_POINTERS exinfo;
#endif

	// If doing SC, Append Old Message

	if (conn->CopyBuffer)
	{
		if ((conn->TempMsg->length + (int) strlen(conn->CopyBuffer) + 80 )> conn->MailBufferSize)
		{
			conn->MailBufferSize += strlen(conn->CopyBuffer) + 80;
			conn->MailBuffer = realloc(conn->MailBuffer, conn->MailBufferSize);
	
			if (conn->MailBuffer == NULL)
			{
				nodeprintf(conn, "Failed to extend Message Buffer\r");

				conn->Flags &= ~GETTINGMESSAGE;
				return;
			}
		}

		memcpy(&conn->MailBuffer[conn->TempMsg->length], OldMess, strlen(OldMess));

		conn->TempMsg->length += strlen(OldMess);

		memcpy(&conn->MailBuffer[conn->TempMsg->length], conn->CopyBuffer, strlen(conn->CopyBuffer));

		conn->TempMsg->length += strlen(conn->CopyBuffer);

		free(conn->CopyBuffer);
		conn->CopyBuffer = NULL;
	}

		// Allocate a message Record slot

		Msg = AllocateMsgRecord();
		memcpy(Msg, conn->TempMsg, sizeof(struct MsgInfo));

		free(conn->TempMsg);

		// Set number here so they remain in sequence
		
		GetSemaphore(&MsgNoSemaphore, 0);
		Msg->number = ++LatestMsg;
		FreeSemaphore(&MsgNoSemaphore);
		MsgnotoMsg[Msg->number] = Msg;

		// Create BID if non supplied

		if (Msg->bid[0] == 0)
			sprintf_s(Msg->bid, sizeof(Msg->bid), "%d_%s", LatestMsg, BBSName);

		// if message body had R: lines, get date created from last (not very accurate, but best we can do)

		// Also check if we have had message before to detect loops

		ptr1 = conn->MailBuffer;
		OurCount = 0;

		// If it is a B2 Message, Must Skip B2 Header

		if (Msg->B2Flags & B2Msg)
		{
			ptr1 = strstr(ptr1, "\r\n\r\n");
			if (ptr1)
				ptr1 += 4;
			else
				ptr1 = conn->MailBuffer;
		}

nextline:

		if (memcmp(ptr1, "R:", 2) == 0)
		{
			// Is if ours?

			// BPQ RLINE Format R:090920/1041Z 6542@N4JOA.#WPBFL.FL.USA.NOAM BPQ1.0.2

			ptr3 = strchr(ptr1, '@');
			ptr4 = strchr(ptr1, '.');

			if (ptr3 && ptr4 && (ptr4 > ptr3))
			{
				if (memcmp(ptr3+1, BBSName, ptr4-ptr3-1) == 0)
					OurCount++;
			}

			GetWPBBSInfo(ptr1);		// Create WP /I record from R: Line
			
			// see if another

			ptr2 = ptr1;			// save
			ptr1 = strchr(ptr1, '\r');
			if (ptr1 == 0)
			{
				Debugprintf("Corrupt Message %s from %s - truncated within R: line", Msg->bid, Msg->from);
				return;
			}
			ptr1++;
			if (*ptr1 == '\n') ptr1++;

			goto nextline;
		}

		// ptr2 points to last R: line (if any)

		if (ptr2)
		{
			struct tm rtime;
			time_t result;

			memset(&rtime, 0, sizeof(struct tm));

			if (ptr2[10] == '/')
			{
				// Dodgy 4 char year
			
				sscanf(&ptr2[2], "%04d%02d%02d/%02d%02d",
					&rtime.tm_year, &rtime.tm_mon, &rtime.tm_mday, &rtime.tm_hour, &rtime.tm_min);
				rtime.tm_year -= 1900;
				rtime.tm_mon--;
			}
			else if (ptr2[8] == '/')
			{
				sscanf(&ptr2[2], "%02d%02d%02d/%02d%02d",
					&rtime.tm_year, &rtime.tm_mon, &rtime.tm_mday, &rtime.tm_hour, &rtime.tm_min);

				if (rtime.tm_year < 90)
					rtime.tm_year += 100;		// Range 1990-2089
				rtime.tm_mon--;
			}

			// Otherwise leave date as zero, which should be rejected

//			result = _mkgmtime(&rtime);

			if ((result = mktime(&rtime)) != (time_t)-1 )
			{
				result -= (time_t)_MYTIMEZONE;

				Msg->datecreated =  result;	
				Age = (time(NULL) - result)/86400;

				if ( Age < -7)
				{
					Msg->status = 'H';
					HoldReason = "Suspect Date Sent";
				}
				else if (Age > BidLifetime || Age > MaxAge)
				{
					Msg->status = 'H';
					HoldReason = "Message too old";

				}
				else
					GetWPInfoFromRLine(Msg->from, ptr2, result);
			}
			else
			{
				// Can't decode R: Datestamp

				Msg->status = 'H';
				HoldReason = "Corrupt R: Line - can't determine age";
			}

			if (OurCount > 1)
			{
				// Message is looping 

				Msg->status = 'H';
				HoldReason = "Message may be looping";

			}

			if (Msg->status == 'N' && strcmp(Msg->to, "WP") == 0)
			{
				ProcessWPMsg(conn->MailBuffer, Msg->length, ptr2);
	
				if (Msg->type == 'P')			// Kill any processed private WP messages.
					Msg->status = 'K';

			}
		}

		conn->MailBuffer[Msg->length] = 0;

		if (CheckBadWords(Msg->title) || CheckBadWords(conn->MailBuffer))
		{
			Msg->status = 'H';
			HoldReason = "Bad word in title or body";
		}

		if (CheckHoldFilters(Msg->from, Msg->to, Msg->via))
		{
			Msg->status = 'H';
			HoldReason = "Matched Hold Filters";
		}

		CreateMessageFile(conn, Msg);

		BIDRec = AllocateBIDRecord();

		strcpy(BIDRec->BID, Msg->bid);
		BIDRec->mode = Msg->type;
		BIDRec->u.msgno = LOWORD(Msg->number);
		BIDRec->u.timestamp = LOWORD(time(NULL)/86400);

		if (Msg->length > MaxTXSize)
		{
			Msg->status = 'H';
			HoldReason = "Message too long";

			if (!(conn->BBSFlags & BBS))
				nodeprintf(conn, "*** Warning Message length exceeds sysop-defined maximum of %d - Message will be held\r", MaxTXSize);
		}

		if (Msg->to[0])
			FWDCount = MatchMessagetoBBSList(Msg, conn);
		else
		{
			// If addressed @winlink.org, and to a local user, Keep here.
			
			char * Call;
			char * AT;

			Call = _strupr(_strdup(Msg->via));
			AT = strlop(Call, '@');

			if (AT && _stricmp(AT, "WINLINK.ORG") == 0)
			{
				struct UserInfo * user = LookupCall(Call);

				if (user)
				{
					if (user->flags & F_POLLRMS)
					{
						Logprintf(LOG_BBS, conn, '?', "SMTP Message @ winlink.org, but local RMS user - leave here");
						strcpy(Msg->to, Call);
						strcpy(Msg->via, AT);
						if (user->flags & F_BBS)	// User is a BBS, so set FWD bit so he can get it
							set_fwd_bit(Msg->fbbs, user->BBSNumber);

					}
				}
			}
			free(Call);
		}

		// Warn SYSOP if P or T forwarded in, and has nowhere to go

		if ((conn->BBSFlags & BBS) && Msg->type != 'B' && FWDCount == 0 && WarnNoRoute &&
			strcmp(Msg->to, "SYSOP") && strcmp(Msg->to, "WP"))
		{
			if (Msg->via[0])
			{	
				if (_stricmp(Msg->via, BBSName))		// Not for our BBS a
					if (_stricmp(Msg->via, AMPRDomain))	// Not for our AMPR Address
						SendWarningToSYSOP(Msg);
			}
			else
			{
				// No via - is it for a local user?
				
				if (LookupCall(Msg->to) == 0)
					SendWarningToSYSOP(Msg);
			}
		}

		if (!(conn->BBSFlags & BBS))
		{
			nodeprintf(conn, "Message: %d Bid:  %s Size: %d\r", Msg->number, Msg->bid, Msg->length);

			if (Msg->via[0])
			{
				if (_stricmp(Msg->via, BBSName))		// Not for our BBS a
					if (_stricmp(Msg->via, AMPRDomain))	// Not for our AMPR Address

				if (FWDCount ==  0 &&  Msg->to[0] != 0)		// unless smtp msg
					nodeprintf(conn, "@BBS specified, but no forwarding info is available - msg may not be delivered\r");
			}
			else
			{
				if (FWDCount ==  0 && conn->LocalMsg == 0 && Msg->type != 'B')
					// Not Local and no forward route
					nodeprintf(conn, "Message is not for a local user, and no forwarding info is available - msg may not be delivered\r");
			}
			if (conn->ToCount == 0)
				SendPrompt(conn, conn->UserPointer);
		}
		else
		{
			if (!(conn->BBSFlags & FBBForwarding))
			{
				if (conn->ToCount == 0)
					if (conn->BBSFlags & OUTWARDCONNECT)
						nodeprintf(conn, "F>\r");				// if Outward connect must be reverse forward
					else
						nodeprintf(conn, ">\r");
			}					
		}
		if(Msg->to[0] == 0)
			SMTPMsgCreated=TRUE;

		if (Msg->status != 'H' && Msg->type == 'B' && memcmp(Msg->fbbs, zeros, NBMASK) != 0)
			Msg->status = '$';				// Has forwarding

		if (Msg->status == 'H')
		{
			int Length=0;
			char * MailBuffer = malloc(100);
			char Title[100];

			Length += sprintf(MailBuffer, "Message %d Held\r\n", Msg->number);
			sprintf(Title, "Message %d Held - %s", Msg->number, HoldReason);
			SendMessageToSYSOP(Title, MailBuffer, Length);
		}

		BuildNNTPList(Msg);				// Build NNTP Groups list

		SaveMessageDatabase();
		SaveBIDDatabase();

		if (EnableUI)
#ifdef LINBPQ
			SendMsgUI(Msg);	
#else
		__try
		{
			SendMsgUI(Msg);
		}
		My__except_Routine("SendMsgUI");
#endif
		return;
}

VOID CreateMessageFile(ConnectionInfo * conn, struct MsgInfo * Msg)
{
	char MsgFile[MAX_PATH];
	FILE * hFile;
	int WriteLen=0;
	char Mess[255];
	int len;
	BOOL AutoImport = FALSE;

	sprintf_s(MsgFile, sizeof(MsgFile), "%s/m_%06d.mes", MailDir, Msg->number);
	
	//	If title is "Batched messages for AutoImport from BBS xxxxxx and first line is S? and it is
	//  for this BBS, Import file and set message as Killed. May need to strip B2 Header and R: lines

	if (strcmp(Msg->to, BBSName) == 0 && strstr(Msg->title, "Batched messages for AutoImport from BBS "))
	{
		UCHAR * ptr = conn->MailBuffer;

		// If it is a B2 Message, Must Skip B2 Header

		if (Msg->B2Flags & B2Msg)
		{
			ptr = strstr(ptr, "\r\n\r\n");
			if (ptr)
				ptr += 4;
			else
				ptr = conn->MailBuffer;
		}

		if (memcmp(ptr, "R:", 2) == 0)
		{
			ptr = strstr(ptr, "\r\n\r\n");		//And remove R: Lines
			if (ptr)
				ptr += 4;
		}

		if (*(ptr) == 'S' && ptr[2] == ' ')
		{
			int HeaderLen = ptr - conn->MailBuffer;
			Msg->length -= HeaderLen;
			memmove(conn->MailBuffer, ptr, Msg->length);
			Msg->status = 'K';
			AutoImport = TRUE;
		}
	}

	hFile = fopen(MsgFile, "wb");

	if (hFile)
	{
		WriteLen = fwrite(conn->MailBuffer, 1, Msg->length, hFile);
		fclose(hFile);
	}

	if (AutoImport)
		ImportMessages(NULL, MsgFile, TRUE);

	free(conn->MailBuffer);
	conn->MailBufferSize=0;
	conn->MailBuffer=0;

	if (WriteLen != Msg->length)
	{
		len = sprintf_s(Mess, sizeof(Mess), "Failed to create Message File\r");
		QueueMsg(conn, Mess, len);
		Debugprintf(Mess);
	}
	return;
}




VOID SendUnbuffered(int stream, char * msg, int len)
{
#ifndef LINBPQ
	if (stream < 0)
		WritetoConsoleWindow(stream, msg, len);
	else
#endif
		SendMsg(stream, msg, len);
}

BOOL FindMessagestoForwardLoop(CIRCUIT * conn, char Type, int MaxLen);

BOOL FindMessagestoForward (CIRCUIT * conn)
{
	struct UserInfo * user = conn->UserPointer;

#ifndef LINBPQ

	struct _EXCEPTION_POINTERS exinfo;

	__try {
#endif

	if ((user->flags & F_Temp_B2_BBS) || conn->RMSExpress)
	{
		if (conn->PacLinkCalls == NULL)
		{
			// create a list with just the user call

			char * ptr1;

			conn->PacLinkCalls = zalloc(30);

			ptr1 = (char *)conn->PacLinkCalls;
			ptr1 += 10;
			strcpy(ptr1, user->Call);

			conn->PacLinkCalls[0] = ptr1;
		}
	}

	if (conn->SendT && FindMessagestoForwardLoop(conn, 'T', conn->MaxTLen))
	{
		conn->LastForwardType = 'T';
		return TRUE;
	}

	if (conn->LastForwardType == 'T')
		conn->NextMessagetoForward = FirstMessageIndextoForward;

	if (conn->SendP && FindMessagestoForwardLoop(conn, 'P', conn->MaxPLen))
	{
		conn->LastForwardType = 'P';
		return TRUE;
	}

	if (conn->LastForwardType == 'P')
		conn->NextMessagetoForward = FirstMessageIndextoForward;

	if (conn->SendB && FindMessagestoForwardLoop(conn, 'B', conn->MaxBLen))
	{
		conn->LastForwardType = 'B';
		return TRUE;
	}

	conn->LastForwardType = 0;
	return FALSE;
#ifndef LINBPQ
	} My__except_Routine("FindMessagestoForward");
#endif
	return FALSE;

}


BOOL FindMessagestoForwardLoop(CIRCUIT * conn, char Type, int MaxLen)
{
	// See if any messages are queued for this BBS

	int m;
	struct MsgInfo * Msg;
	struct UserInfo * user = conn->UserPointer;
	struct FBBHeaderLine * FBBHeader;
	BOOL Found = FALSE;
	char RLine[100];
	int TotalSize = 0;
	time_t NOW = time(NULL);

//	Debugprintf("FMTF entered Call %s Type %c Maxlen %d NextMsg = %d BBSNo = %d",
//		conn->Callsign, Type, MaxLen, conn->NextMessagetoForward, user->BBSNumber);

	if (conn->PacLinkCalls || (conn->UserPointer->flags & F_NTSMPS))	// Looking for all messages, so reset 
		conn->NextMessagetoForward = 1;

	conn->FBBIndex = 0;

	for (m = conn->NextMessagetoForward; m <= NumberofMessages; m++)
	{
		Msg=MsgHddrPtr[m];

		//	If an NTS MPS, see if anything matches

		if (Type == 'T' && (conn->UserPointer->flags & F_NTSMPS))
		{
			struct BBSForwardingInfo * ForwardingInfo = conn->UserPointer->ForwardingInfo;
			int depth;
				
			if (Msg->type == 'T' && Msg->status == 'N' && Msg->length <= MaxLen && ForwardingInfo)
			{
				depth = CheckBBSToForNTS(Msg, ForwardingInfo);

				if (depth > -1 && Msg->Locked == 0)
					goto Forwardit;
						
				depth = CheckBBSAtList(Msg, ForwardingInfo, Msg->via);

				if (depth && Msg->Locked == 0)
					goto Forwardit;

				depth = CheckBBSATListWildCarded(Msg, ForwardingInfo, Msg->via);

				if (depth > -1 && Msg->Locked == 0)
					goto Forwardit;
			}
		}

		// If forwarding to Paclink or RMS Express, look for any message matching the requested call list with statis 'N'

		if (conn->PacLinkCalls)
		{
			int index = 1;

			char * Call = conn->PacLinkCalls[0];

			while (Call)
			{
				if (Msg->type == Type && Msg->status == 'N')
				{
//				Debugprintf("Comparing RMS Call %s %s", Msg->to, Call);
				if (_stricmp(Msg->to, Call) == 0)
					if (Msg->status == 'N' && Msg->type == Type && Msg->length <= MaxLen) 
						goto Forwardit;
					else
						Debugprintf("Call Match but Wrong Type/Len %c %d", Msg->type, Msg->length);
				}
				Call = conn->PacLinkCalls[index++];
			}
//			continue;
		}

		if (Msg->type == Type && Msg->length <= MaxLen && (Msg->status != 'H')
			&& (Msg->status != 'D') && Msg->type && check_fwd_bit(Msg->fbbs, user->BBSNumber))
		{
			// Message to be sent - do a consistancy check (State, etc)

		Forwardit:

			if (Msg->Defered)			// = response received
			{
				Msg->Defered--;
				Debugprintf("Message %d deferred", Msg->number);
				continue;
			}

			if ((Msg->from[0] == 0) || (Msg->to[0] == 0))
			{
				int Length=0;
				char * MailBuffer = malloc(100);
				char Title[100];

				Length += sprintf(MailBuffer, "Message %d Held\r\n", Msg->number);
				sprintf(Title, "Message %d Held - %s", Msg->number, "Missing From: or To: field");
				SendMessageToSYSOP(Title, MailBuffer, Length);
			
				Msg->status = 'H';
				continue;
			}

			conn->NextMessagetoForward = m + 1;			// So we don't offer again if defered

			Msg->Locked = 1;				// So other MPS can't pick it up

			// if FBB forwarding add to list, eise save pointer

			if (conn->BBSFlags & FBBForwarding)
			{
				struct tm *tm;

				FBBHeader = &conn->FBBHeaders[conn->FBBIndex++];

				FBBHeader->FwdMsg = Msg;
				FBBHeader->MsgType = Msg->type;
				FBBHeader->Size = Msg->length;
				TotalSize += Msg->length;
				strcpy(FBBHeader->From, Msg->from);
				strcpy(FBBHeader->To, Msg->to);
				strcpy(FBBHeader->ATBBS, Msg->via);
				strcpy(FBBHeader->BID, Msg->bid);

				// Set up R:Line, so se can add its length to the sise

				tm = gmtime(&Msg->datereceived);	
	
				FBBHeader->Size += sprintf_s(RLine, sizeof(RLine),"R:%02d%02d%02d/%02d%02dZ %d@%s.%s %s\r\n",
					tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min,
					Msg->number, BBSName, HRoute, RlineVer);

				// If using B2 forwarding we need the message size and Compressed size for FC proposal

				if (conn->BBSFlags & FBBB2Mode)
				{
					if (CreateB2Message(conn, FBBHeader, RLine) == FALSE)
					{
						char * MailBuffer = malloc(100);
						char Title[100];
						int Length;
						
						// Corrupt B2 Message
						
						Debugprintf("Corrupt B2 Message found - Message %d will be held", Msg->number);
						Msg->status = 'H';
						SaveMessageDatabase();

						conn->FBBIndex--;
						TotalSize -= Msg->length;
						memset(&conn->FBBHeaders[conn->FBBIndex], 0, sizeof(struct FBBHeaderLine));

						Length = sprintf(MailBuffer, "Message %d Held\r\n", Msg->number);
						sprintf(Title, "Message %d Held - %s", Msg->number, "Corrupt B2 Message");
						SendMessageToSYSOP(Title, MailBuffer, Length);
			
						continue;
					}
				}

				if (conn->FBBIndex == 5  || TotalSize > user->ForwardingInfo->MaxFBBBlockSize)
					return TRUE;							// Got max number or too big

				Found = TRUE;								// Remember we have some
			}
			else
			{
				conn->FwdMsg = Msg;
				return TRUE;
			}
		}
	}

	return Found;
}

BOOL SeeifMessagestoForward (int BBSNumber, CIRCUIT * conn)
{
	// See if any messages are queued for this BBS

	// Conn is not NULL, also check Msg Type

	int m;
	struct MsgInfo * Msg;

	for (m = FirstMessageIndextoForward; m <= NumberofMessages; m++)
	{
		Msg=MsgHddrPtr[m];

		if ((Msg->status != 'H') && Msg->type && check_fwd_bit(Msg->fbbs, BBSNumber))
		{
			if (conn)
			{
				char Type = Msg->type;

				if ((conn->SendB && Type == 'B') || (conn->SendP && Type == 'P') || (conn->SendT && Type == 'T'))
				{
//					Debugprintf("SeeifMessagestoForward BBSNo %d Msg %d", BBSNumber, Msg->number);
					return TRUE;
				}
			}
			else
			{
//				Debugprintf("SeeifMessagestoForward BBSNo %d Msg %d", BBSNumber, Msg->number);	
				return TRUE;
			}
		}
	}

	return FALSE;
}

int CountMessagestoForward (struct UserInfo * user)
{
	// See if any messages are queued for this BBS

	int m, n=0;
	struct MsgInfo * Msg;
	int BBSNumber = user->BBSNumber;
	int FirstMessage = FirstMessageIndextoForward;

	if ((user->flags & F_NTSMPS))
		FirstMessage = 1;

	for (m = FirstMessage; m <= NumberofMessages; m++)
	{
		Msg=MsgHddrPtr[m];

		if ((Msg->status != 'H') && (Msg->status != 'D') && Msg->type && check_fwd_bit(Msg->fbbs, BBSNumber))
		{
			n++;
			continue;			// So we dont count twice in Flag set and NTS MPS
		}

		// if an NTS MPS, also check for any matches

		if (Msg->type == 'T' && (user->flags & F_NTSMPS))
		{
			struct BBSForwardingInfo * ForwardingInfo = user->ForwardingInfo;
			int depth;
				
			if (Msg->status == 'N' && ForwardingInfo)
			{
				depth = CheckBBSToForNTS(Msg, ForwardingInfo);

				if (depth > -1 && Msg->Locked == 0)
				{
					n++;
					continue;
				}						
				depth = CheckBBSAtList(Msg, ForwardingInfo, Msg->via);

				if (depth && Msg->Locked == 0)
				{
					n++;
					continue;
				}						

				depth = CheckBBSATListWildCarded(Msg, ForwardingInfo, Msg->via);

				if (depth > -1 && Msg->Locked == 0)
				{
					n++;
					continue;
				}
			}
		}
	}

	return n;
}
VOID SendWarningToSYSOP(struct MsgInfo * Msg)
{
	int Length=0;
	char * MailBuffer = malloc(100);
	char Title[100];

	Length += sprintf(MailBuffer, "Warning - Message %d has nowhere to go", Msg->number);
	sprintf(Title, "Warning - Message %d has nowhere to go", Msg->number);
	SendMessageToSYSOP(Title, MailBuffer, Length);
}



VOID SendMessageToSYSOP(char * Title, char * MailBuffer, int Length)
{
	struct MsgInfo * Msg = AllocateMsgRecord();
	BIDRec * BIDRec;

	char MsgFile[MAX_PATH];
	FILE * hFile;
	int WriteLen=0;

	Msg->length = Length;

	GetSemaphore(&MsgNoSemaphore, 0);
	Msg->number = ++LatestMsg;
	MsgnotoMsg[Msg->number] = Msg;

	FreeSemaphore(&MsgNoSemaphore);
 
	strcpy(Msg->from, "SYSTEM");
	if (SendSYStoSYSOPCall)
		strcpy(Msg->to, SYSOPCall);
	else
		strcpy(Msg->to, "SYSOP");

	strcpy(Msg->title, Title);

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
		WriteLen = fwrite(MailBuffer, 1, Msg->length, hFile);
		fclose(hFile);
	}

	MatchMessagetoBBSList(Msg, NULL);
	free(MailBuffer);
}

int FindFreeBBSNumber()
{
	// returns the lowest number not used by any bbs or message.

	struct MsgInfo * Msg;
	struct UserInfo * user;
	int i, m;

	for (i = 1; i<= NBBBS; i++)
	{
		for (user = BBSChain; user; user = user->BBSNext)
		{
			if (user->BBSNumber == i)
				goto nexti;				// In use
		}

		// Not used by BBS - check messages

		for (m = 1; m <= NumberofMessages; m++)
		{
			Msg=MsgHddrPtr[m];

			if (check_fwd_bit(Msg->fbbs, i))
				goto nexti;				// In use

			if (check_fwd_bit(Msg->forw, i))
				goto nexti;				// In use
		}

		// Not in Use

		return i;
	
nexti:;

	}

	return 0;		// All used
}

BOOL SetupNewBBS(struct UserInfo * user)
{
	user->BBSNumber = FindFreeBBSNumber();

	if (user->BBSNumber == 0)
		return FALSE;

	user->BBSNext = BBSChain;
	BBSChain = user;

	SortBBSChain();

	ReinitializeFWDStruct(user);

	return TRUE;
}

VOID DeleteBBS(struct UserInfo * user)
{
	struct UserInfo * BBSRec, * PrevBBS = NULL;

#ifndef LINBPQ
	RemoveMenu(hFWDMenu, IDM_FORWARD_ALL + user->BBSNumber, MF_BYCOMMAND); 
#endif
	for (BBSRec = BBSChain; BBSRec; PrevBBS = BBSRec, BBSRec = BBSRec->BBSNext)
	{
		if (user == BBSRec)
		{
			if (PrevBBS == NULL)		// First in chain;
			{
				BBSChain = BBSRec->BBSNext;
				break;
			}
			PrevBBS->BBSNext = BBSRec->BBSNext;
			break;
		}
	}
}


VOID SetupFwdTimes(struct BBSForwardingInfo * ForwardingInfo);

VOID SetupForwardingStruct(struct UserInfo * user)
{
	struct	BBSForwardingInfo * ForwardingInfo;

	char Key[100] =  "BBSForwarding.";
	char Temp[100];

	HKEY hKey=0;
	int retCode,Type,Vallen;
	char RegKey[100] =  "SOFTWARE\\G8BPQ\\BPQ32\\BPQMailChat\\BBSForwarding\\";

	int m;
	struct MsgInfo * Msg;

	ForwardingInfo = user->ForwardingInfo = zalloc(sizeof(struct BBSForwardingInfo));

	if (UsingingRegConfig == 0)
	{
		//	Config from file

		if (isdigit(user->Call[0]))
			strcat(Key, "*");

		strcat(Key, user->Call);

		group = config_lookup (&cfg, Key);

		if (group == NULL)			// No info
			return;
		else
		{
			ForwardingInfo->TOCalls = GetMultiStringValue(group,  "TOCalls");
			ForwardingInfo->ConnectScript = GetMultiStringValue(group,  "ConnectScript");
			ForwardingInfo->ATCalls = GetMultiStringValue(group,  "ATCalls");
			ForwardingInfo->Haddresses = GetMultiStringValue(group,  "HRoutes");
			ForwardingInfo->HaddressesP = GetMultiStringValue(group,  "HRoutesP");
			ForwardingInfo->FWDTimes = GetMultiStringValue(group,  "FWDTimes");

			ForwardingInfo->Enabled = GetIntValue(group, "Enabled");
			ForwardingInfo->ReverseFlag = GetIntValue(group, "RequestReverse");
			ForwardingInfo->AllowCompressed = GetIntValue(group, "AllowCompressed");
			ForwardingInfo->AllowB1 = GetIntValue(group, "UseB1Protocol");
			ForwardingInfo->AllowB2 = GetIntValue(group, "UseB2Protocol");
			ForwardingInfo->SendCTRLZ = GetIntValue(group, "SendCTRLZ");

			if (ForwardingInfo->AllowB1 || ForwardingInfo->AllowB2)
				ForwardingInfo->AllowCompressed = TRUE;

			ForwardingInfo->PersonalOnly = GetIntValue(group, "FWDPersonalsOnly");
			ForwardingInfo->SendNew = GetIntValue(group, "FWDNewImmediately");
			ForwardingInfo->FwdInterval = GetIntValue(group, "FwdInterval");
			ForwardingInfo->RevFwdInterval = GetIntValue(group, "RevFWDInterval");
			ForwardingInfo->MaxFBBBlockSize = GetIntValue(group, "MaxFBBBlock");

			if (ForwardingInfo->MaxFBBBlockSize == 0)
				ForwardingInfo->MaxFBBBlockSize = 10000;

			if (ForwardingInfo->FwdInterval == 0)
				ForwardingInfo->FwdInterval = 3600;

			GetStringValue(group, "BBSHA", Temp);
		
			if (Temp[0])
				ForwardingInfo->BBSHA = _strdup(Temp);
			else
				ForwardingInfo->BBSHA = _strdup("");
		}
	}
	else
	{
#ifndef	LINBPQ
		strcat(RegKey, user->Call);
		retCode = RegOpenKeyEx (REGTREE, RegKey, 0, KEY_QUERY_VALUE, &hKey);

		if (retCode != ERROR_SUCCESS)
			return;
		else
		{
			ForwardingInfo->ConnectScript = RegGetMultiStringValue(hKey,  "Connect Script");
			ForwardingInfo->TOCalls = RegGetMultiStringValue(hKey,  "TOCalls");
			ForwardingInfo->ATCalls = RegGetMultiStringValue(hKey,  "ATCalls");
			ForwardingInfo->Haddresses = RegGetMultiStringValue(hKey,  "HRoutes");
			ForwardingInfo->HaddressesP = RegGetMultiStringValue(hKey,  "HRoutesP");
			ForwardingInfo->FWDTimes = RegGetMultiStringValue(hKey,  "FWD Times");

			Vallen=4;
			retCode += RegQueryValueEx(hKey, "Enabled", 0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->Enabled,(ULONG *)&Vallen);
				
			Vallen=4;
			retCode += RegQueryValueEx(hKey, "RequestReverse", 0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->ReverseFlag,(ULONG *)&Vallen);

			Vallen=4;
			retCode += RegQueryValueEx(hKey, "AllowCompressed", 0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->AllowCompressed,(ULONG *)&Vallen);

			Vallen=4;
			retCode += RegQueryValueEx(hKey, "Use B1 Protocol", 0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->AllowB1,(ULONG *)&Vallen);

			Vallen=4;
			retCode += RegQueryValueEx(hKey, "Use B2 Protocol", 0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->AllowB2,(ULONG *)&Vallen);

			Vallen=4;
			retCode += RegQueryValueEx(hKey, "SendCTRLZ", 0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->SendCTRLZ,(ULONG *)&Vallen);

			if (ForwardingInfo->AllowB1 || ForwardingInfo->AllowB2)
				ForwardingInfo->AllowCompressed = TRUE;
	
			Vallen=4;
			retCode += RegQueryValueEx(hKey, "FWD Personals Only", 0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->PersonalOnly,(ULONG *)&Vallen);

			Vallen=4;
			retCode += RegQueryValueEx(hKey, "FWD New Immediately", 0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->SendNew,(ULONG *)&Vallen);

			Vallen=4;
			RegQueryValueEx(hKey,"FWDInterval",0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->FwdInterval,(ULONG *)&Vallen);

			Vallen=4;
			RegQueryValueEx(hKey,"RevFWDInterval",0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->RevFwdInterval,(ULONG *)&Vallen);

			RegQueryValueEx(hKey,"MaxFBBBlock",0,			
				(ULONG *)&Type,(UCHAR *)&ForwardingInfo->MaxFBBBlockSize,(ULONG *)&Vallen);

			if (ForwardingInfo->MaxFBBBlockSize == 0)
				ForwardingInfo->MaxFBBBlockSize = 10000;

			if (ForwardingInfo->FwdInterval == 0)
				ForwardingInfo->FwdInterval = 3600;

			Vallen=0;
			retCode = RegQueryValueEx(hKey,"BBSHA",0 , (ULONG *)&Type,NULL, (ULONG *)&Vallen);

			if (retCode != 0)
			{
				// No Key - Get from WP??
				
				WPRec * ptr = LookupWP(user->Call);

				if (ptr)
				{
					if (ptr->first_homebbs)
					{
						ForwardingInfo->BBSHA = _strdup(ptr->first_homebbs);
					}
				}
			}

			if (Vallen)
			{
				ForwardingInfo->BBSHA = malloc(Vallen);
				RegQueryValueEx(hKey, "BBSHA", 0, (ULONG *)&Type, ForwardingInfo->BBSHA,(ULONG *)&Vallen);
			}

			RegCloseKey(hKey);
		}
#endif
	}

	// Convert FWD Times and H Addresses

	if (ForwardingInfo->FWDTimes)
			SetupFwdTimes(ForwardingInfo);

	if (ForwardingInfo->Haddresses)
			SetupHAddreses(ForwardingInfo);

	if (ForwardingInfo->HaddressesP)
			SetupHAddresesP(ForwardingInfo);

	if (ForwardingInfo->BBSHA)
	{
			if (ForwardingInfo->BBSHA[0])
				SetupHAElements(ForwardingInfo);
			else
			{
				free(ForwardingInfo->BBSHA);
				ForwardingInfo->BBSHA = NULL;
			}
	}

	for (m = FirstMessageIndextoForward; m <= NumberofMessages; m++)
	{
		Msg=MsgHddrPtr[m];

		// If any forward bits are set, increment count on  BBS record.

		if (memcmp(Msg->fbbs, zeros, NBMASK) != 0)
		{
			if (Msg->type && check_fwd_bit(Msg->fbbs, user->BBSNumber))
			{
				user->ForwardingInfo->MsgCount++;
			}
		}
	}
}

VOID * GetMultiStringValue(config_setting_t * group, char * ValueName)
{
	char * ptr1;
	char * MultiString = NULL;
	const char * ptr;
	int Count = 0;
	char ** Value;
	config_setting_t *setting;
	char * Save;

	Value = zalloc(4);				// always NULL entry on end even if no values
	Value[0] = NULL;

	setting = config_setting_get_member (group, ValueName);
	
	if (setting)
	{
		ptr =  config_setting_get_string (setting);

		Save = _strdup(ptr);			// DOnt want to change config string
		ptr = Save;
	
		while (ptr && strlen(ptr))
		{
			ptr1 = strchr(ptr, '|');
			
			if (ptr1)
				*(ptr1++) = 0;

			Value = realloc(Value, (Count+2)*4);
			
			Value[Count++] = _strdup(ptr);
			
			ptr = ptr1;
		}
		free(Save);
	}

	Value[Count] = NULL;
	return Value;
}


VOID * RegGetMultiStringValue(HKEY hKey, char * ValueName)
{
#ifdef LINBPQ
	return NULL;
#else
	int retCode,Type,Vallen;
	char * MultiString = NULL;
	int ptr, len;
	int Count = 0;
	char ** Value;

	Value = zalloc(4);				// always NULL entry on end even if no values

	Value[0] = NULL;

	Vallen=0;


	retCode = RegQueryValueEx(hKey, ValueName, 0, (ULONG *)&Type, NULL, (ULONG *)&Vallen);

	if ((retCode != 0)  || (Vallen < 3))		// Two nulls means empty multistring
	{
		free(Value);
		return FALSE;
	}

	MultiString = malloc(Vallen);

	retCode = RegQueryValueEx(hKey, ValueName, 0,			
			(ULONG *)&Type,(UCHAR *)MultiString,(ULONG *)&Vallen);

	ptr=0;

	while (MultiString[ptr])
	{
		len=strlen(&MultiString[ptr]);

		Value = realloc(Value, (Count+2)*4);
		Value[Count++] = _strdup(&MultiString[ptr]);
		ptr+= (len + 1);
	}

	Value[Count] = NULL;

	free(MultiString);

	return Value;
#endif
}

VOID FreeForwardingStruct(struct UserInfo * user)
{
	struct	BBSForwardingInfo * ForwardingInfo;
	int i;


	ForwardingInfo = user->ForwardingInfo;

	FreeList(ForwardingInfo->TOCalls);
	FreeList(ForwardingInfo->ATCalls);
	FreeList(ForwardingInfo->Haddresses);
	FreeList(ForwardingInfo->HaddressesP);

	i=0;
	if(ForwardingInfo->HADDRS)
	{
		while(ForwardingInfo->HADDRS[i])
		{
			FreeList(ForwardingInfo->HADDRS[i]);
			i++;
		}
		free(ForwardingInfo->HADDRS);
		free(ForwardingInfo->HADDROffet);
	}

	i=0;
	if(ForwardingInfo->HADDRSP)
	{
		while(ForwardingInfo->HADDRSP[i])
		{
			FreeList(ForwardingInfo->HADDRSP[i]);
			i++;
		}
		free(ForwardingInfo->HADDRSP);
	}

	FreeList(ForwardingInfo->ConnectScript);
	FreeList(ForwardingInfo->FWDTimes);
	if (ForwardingInfo->FWDBands)
	{
		i=0;
		while(ForwardingInfo->FWDBands[i])
		{
			free(ForwardingInfo->FWDBands[i]);
			i++;
		}
		free(ForwardingInfo->FWDBands);
	}
	if (ForwardingInfo->BBSHAElements)
	{
		i=0;
		while(ForwardingInfo->BBSHAElements[i])
		{
			free(ForwardingInfo->BBSHAElements[i]);
			i++;
		}
		free(ForwardingInfo->BBSHAElements);
	}
	free(ForwardingInfo->BBSHA);

}

VOID FreeList(char ** Hddr)
{
	VOID ** Save;
	
	if (Hddr)
	{
		Save = (void **)Hddr;
		while(Hddr[0])
		{
			free(Hddr[0]);
			Hddr++;
		}	
		free(Save);
	}
}


VOID ReinitializeFWDStruct(struct UserInfo * user)
{
	if (user->ForwardingInfo)
	{
		FreeForwardingStruct(user);
		free(user->ForwardingInfo); 
	}

	SetupForwardingStruct(user);

}

VOID SetupFwdTimes(struct BBSForwardingInfo * ForwardingInfo)
{
	char ** Times = ForwardingInfo->FWDTimes;
	int Start, End;
	int Count = 0;

	ForwardingInfo->FWDBands = zalloc(sizeof(struct FWDBAND));

	if (Times)
	{
		while(Times[0])
		{
			ForwardingInfo->FWDBands = realloc(ForwardingInfo->FWDBands, (Count+2)* sizeof(struct FWDBAND));
			ForwardingInfo->FWDBands[Count] = zalloc(sizeof(struct FWDBAND));

			Start = atoi(Times[0]);
			End = atoi(&Times[0][5]);

			ForwardingInfo->FWDBands[Count]->FWDStartBand =  (time_t)(Start / 100) * 3600 + (Start % 100) * 60; 
			ForwardingInfo->FWDBands[Count]->FWDEndBand =  (time_t)(End / 100) * 3600 + (End % 100) * 60 + 59; 

			Count++;
			Times++;
		}
		ForwardingInfo->FWDBands[Count] = NULL;
	}
}
void StartForwarding(int BBSNumber, char ** TempScript)
{
	struct UserInfo * user;
	struct	BBSForwardingInfo * ForwardingInfo ;
	time_t NOW = time(NULL);


	for (user = BBSChain; user; user = user->BBSNext)
	{
		// See if any messages are queued for this BBS

		ForwardingInfo = user->ForwardingInfo;

		if ((BBSNumber == 0) || (user->BBSNumber == BBSNumber))
			if (ForwardingInfo)
				if (ForwardingInfo->Enabled || BBSNumber)		// Menu Command overrides enable
					if (ForwardingInfo->ConnectScript  && (ForwardingInfo->Forwarding == 0) && ForwardingInfo->ConnectScript[0])
						if (BBSNumber || SeeifMessagestoForward(user->BBSNumber, NULL) ||
							(ForwardingInfo->ReverseFlag && ((NOW - ForwardingInfo->LastReverseForward) >= ForwardingInfo->RevFwdInterval))) // Menu Command overrides Reverse
						{
							user->ForwardingInfo->ScriptIndex = -1;	// Incremented before being used
						
							// See if TempScript requested
							
							if (user->ForwardingInfo->TempConnectScript)
								FreeList(user->ForwardingInfo->TempConnectScript);

							user->ForwardingInfo->TempConnectScript = TempScript;
						
							if (ConnecttoBBS(user))
								ForwardingInfo->Forwarding = TRUE;
						}
	}

	return;
}

size_t fwritex(CIRCUIT * conn, void * _Str, size_t _Size, size_t _Count, FILE * _File)
{
	if (_File)
		return fwrite(_Str, _Size, _Count, _File);

	// Appending to MailBuffer

	memcpy(&conn->MailBuffer[conn->InputLen], _Str, _Count);
	conn->InputLen += _Count;

	return _Count;
}


BOOL ForwardMessagestoFile(CIRCUIT * conn, char * FN)
{
	BOOL AddCRLF = FALSE;
	BOOL AutoImport = FALSE;
	FILE * Handle = NULL;
	char * Context;
	BOOL Email = FALSE;
	time_t now = time(NULL);
	char * param;

	FN = strtok_s(FN, " ,", &Context); 

	param = strtok_s(NULL, " ,", &Context); 

	if (param)
	{
		if (_stricmp(param, "ADDCRLF") == 0)
			AddCRLF = TRUE;

		if (_stricmp(param, "AutoImport") == 0)
			AutoImport = TRUE;

		param = strtok_s(NULL, " ,", &Context); 

		if (param)
		{
			if (_stricmp(param, "ADDCRLF") == 0)
				AddCRLF = TRUE;

			if (_stricmp(param, "AutoImport") == 0)
				AutoImport = TRUE;

		}
	}
	// If FN is an email address, write to a temp file, and send via rms or emali gateway
	
	if (strchr(FN, '@') || _memicmp(FN, "RMS:", 4) == 0)
	{
		Email = TRUE;
		AddCRLF =TRUE;
		conn->MailBuffer=malloc(100000);
		conn->MailBufferSize=100000;
		conn->InputLen = 0;
	}
	else
	{
		Handle = fopen(FN, "ab");

		if (Handle == NULL)
		{
			int err = GetLastError();
			Logprintf(LOG_BBS, conn, '!', "Failed to open Export File %s", FN);
			return FALSE;
		}
	}

	while (FindMessagestoForward(conn))
	{
		struct MsgInfo * Msg;
		struct tm * tm;
		char * MsgBytes = ReadMessageFile(conn->FwdMsg->number);
		int MsgLen;
		char * MsgPtr;
		char Line[256];
		int len;
		struct UserInfo * user = conn->UserPointer;
		int Index = 0;

		Msg = conn->FwdMsg;

		if (Email)
			if (conn->InputLen + Msg->length + 500 > conn->MailBufferSize)
				break;

		if (Msg->type == 'P')
			Index = PMSG;
		else if (Msg->type == 'B')
			Index = BMSG;
		else if (Msg->type == 'T')
			Index = TMSG;


		if (Msg->via[0])
			len = sprintf(Line, "S%c %s @ %s < %s $%s\r\n", Msg->type, Msg->to,
						Msg->via, Msg->from, Msg->bid);
		else
			len = sprintf(Line, "S%c %s < %s $%s\r\n", Msg->type, Msg->to, Msg->from, Msg->bid);
	
		fwritex(conn, Line, 1, len, Handle);

		len = sprintf(Line, "%s\r\n", Msg->title);
		fwritex(conn, Line, 1, len, Handle);
		
		if (MsgBytes == 0)
		{
			MsgBytes = _strdup("Message file not found\r\n");
			conn->FwdMsg->length = strlen(MsgBytes);
		}

		MsgPtr = MsgBytes;
		MsgLen = conn->FwdMsg->length;

		// If a B2 Message, remove B2 Header

		if (conn->FwdMsg->B2Flags)
		{		
			// Remove all B2 Headers, and all but the first part.
					
			MsgPtr = strstr(MsgBytes, "Body:");
			
			if (MsgPtr)
			{
				MsgLen = atoi(&MsgPtr[5]);
				MsgPtr = strstr(MsgBytes, "\r\n\r\n");		// Blank Line after headers
	
				if (MsgPtr)
					MsgPtr +=4;
				else
					MsgPtr = MsgBytes;
			
			}
			else
				MsgPtr = MsgBytes;
		}

		tm = gmtime(&Msg->datereceived);	

		len = sprintf(Line, "R:%02d%02d%02d/%02d%02dZ %d@%s.%s %s\r\n",
				tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min,
				conn->FwdMsg->number, BBSName, HRoute, RlineVer);

		fwritex(conn, Line, 1, len, Handle);

		if (memcmp(MsgPtr, "R:", 2) != 0)    // No R line, so must be our message - put blank line after header
			fwritex(conn, "\r\n", 1, 2, Handle);

		fwritex(conn, MsgPtr, 1, MsgLen, Handle);

		if (MsgPtr[MsgLen - 2] == '\r')
			fwritex(conn, "/EX\r\n", 1, 5, Handle);
		else
			fwritex(conn, "\r\n/EX\r\n", 1, 7, Handle);

		if (AddCRLF)
			fwritex(conn, "\r\n", 1, 2, Handle);

		free(MsgBytes);
			
		user->Total.MsgsSent[Index]++;
		user->Total.BytesForwardedOut[Index] += MsgLen;

		Msg->datechanged = now;
			
		clear_fwd_bit(conn->FwdMsg->fbbs, user->BBSNumber);
		set_fwd_bit(conn->FwdMsg->forw, user->BBSNumber);

		//  Only mark as forwarded if sent to all BBSs that should have it
			
		if (memcmp(conn->FwdMsg->fbbs, zeros, NBMASK) == 0)
		{
			conn->FwdMsg->status = 'F';			// Mark as forwarded
			conn->FwdMsg->datechanged=time(NULL);
		}

		conn->UserPointer->ForwardingInfo->MsgCount--;
	}

	if (Email)
	{
		struct MsgInfo * Msg;
		BIDRec * BIDRec;

		if (conn->InputLen == 0)
		{
			free(conn->MailBuffer);
			conn->MailBufferSize=0;
			conn->MailBuffer=0;

			return TRUE;
		}

		// Allocate a message Record slot

		Msg = AllocateMsgRecord();

		// Set number here so they remain in sequence
		
		GetSemaphore(&MsgNoSemaphore, 0);
		Msg->number = ++LatestMsg;
		FreeSemaphore(&MsgNoSemaphore);
		MsgnotoMsg[Msg->number] = Msg;

		Msg->type = 'P';
		Msg->status = 'N';
		Msg->datecreated = Msg->datechanged = Msg->datereceived = now;

		strcpy(Msg->from, BBSName);

		sprintf_s(Msg->bid, sizeof(Msg->bid), "%d_%s", LatestMsg, BBSName);

		if (AutoImport)
			sprintf(Msg->title, "Batched messages for AutoImport from BBS %s",  BBSName);
		else
			sprintf(Msg->title, "Batched messages from BBS %s",  BBSName);

		Msg->length = conn->InputLen; 
		CreateMessageFile(conn, Msg);

		BIDRec = AllocateBIDRecord();

		strcpy(BIDRec->BID, Msg->bid);
		BIDRec->mode = Msg->type;
		BIDRec->u.msgno = LOWORD(Msg->number);
		BIDRec->u.timestamp = LOWORD(time(NULL)/86400);

		if (_memicmp(FN, "SMTP:", 5) == 0)
		{
			strcpy(Msg->via, &FN[5]);
			SMTPMsgCreated=TRUE;
		}
		else
		{
			strcpy(Msg->to, "RMS");
			if (_memicmp(FN, "RMS:", 4) == 0)
				strcpy(Msg->via, &FN[4]);
			else
				strcpy(Msg->via, FN);
		}

		MatchMessagetoBBSList(Msg, conn);

		SaveMessageDatabase();
		SaveBIDDatabase();
	}
	else
		fclose(Handle);

	return TRUE;
}

BOOL ForwardMessagetoFile(struct MsgInfo * Msg, FILE * Handle)
{
	struct tm * tm;
	char * MsgBytes = ReadMessageFile(Msg->number);
	char * MsgPtr;
	char Line[256];
	int len;
	int MsgLen = Msg->length;

	if (Msg->via[0])
		len = sprintf(Line, "S%c %s @ %s < %s $%s\r\n", Msg->type, Msg->to,
			Msg->via, Msg->from, Msg->bid);
	else
		len = sprintf(Line, "S%c %s < %s $%s\r\n", Msg->type, Msg->to, Msg->from, Msg->bid);
	
	fwrite(Line, 1, len, Handle);

	len = sprintf(Line, "%s\r\n", Msg->title);
	fwrite(Line, 1, len, Handle);
		
	if (MsgBytes == 0)
	{
		MsgBytes = _strdup("Message file not found\r\n");
		MsgLen = strlen(MsgBytes);
		}

	MsgPtr = MsgBytes;

	// If a B2 Message, remove B2 Header

	if (Msg->B2Flags)
	{		
		// Remove all B2 Headers, and all but the first part.
					
		MsgPtr = strstr(MsgBytes, "Body:");
			
		if (MsgPtr)
		{
			MsgLen = atoi(&MsgPtr[5]);
			
			MsgPtr= strstr(MsgBytes, "\r\n\r\n");		// Blank Line after headers
	
			if (MsgPtr)
				MsgPtr +=4;
			else
				MsgPtr = MsgBytes;
			
		}
		else
			MsgPtr = MsgBytes;
	}

	tm = gmtime(&Msg->datereceived);	

	len = sprintf(Line, "R:%02d%02d%02d/%02d%02dZ %d@%s.%s %s\r\n",
			tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min,
			Msg->number, BBSName, HRoute, RlineVer);

	fwrite(Line, 1, len, Handle);

	if (memcmp(MsgPtr, "R:", 2) != 0)    // No R line, so must be our message - put blank line after header
		fwrite("\r\n", 1, 2, Handle);

	fwrite(MsgPtr, 1, MsgLen, Handle);

	if (MsgPtr[MsgLen - 2] == '\r')
		fwrite("/EX\r\n", 1, 5, Handle);
	else
		fwrite("\r\n/EX\r\n", 1, 7, Handle);

	free(MsgBytes);
		
	return TRUE;

}

BOOL ConnecttoBBS (struct UserInfo * user)
{
	int n, p;
	CIRCUIT * conn;
	struct	BBSForwardingInfo * ForwardingInfo = user->ForwardingInfo;

/*
if (_memicmp(ForwardingInfo->ConnectScript[0], "FILE ", 5) == 0)
	{
		// Forward to File

		CIRCUIT conn;

		memset(&conn, 0, sizeof(conn));

		conn.UserPointer = user;

		ForwardMessagestoFile(&conn, &ForwardingInfo->ConnectScript[0][5]);

		return FALSE;
	}
*/
	for (n = NumberofStreams-1; n >= 0 ; n--)
	{
		conn = &Connections[n];
		
		if (conn->Active == FALSE)
		{
			p = conn->BPQStream;
			memset(conn, 0, sizeof(ConnectionInfo));		// Clear everything
			conn->BPQStream = p;

			conn->Active = TRUE;
			strcpy(conn->Callsign, user->Call); 
			conn->BBSFlags |= (RunningConnectScript | OUTWARDCONNECT);
			conn->UserPointer = user;

			Logprintf(LOG_BBS, conn, '|', "Connecting to BBS %s", user->Call);

			ForwardingInfo->MoreLines = TRUE;
			
			ConnectUsingAppl(conn->BPQStream, BBSApplMask);

#ifdef LINBPQ
			{
				BPQVECSTRUC * SESS;	
				SESS = &BPQHOSTVECTOR[conn->BPQStream - 1];

				if (SESS->HOSTSESSION == NULL)
				{
					Logprintf(LOG_BBS, NULL, '|', "No L4 Sessions for connect to BBS %s", user->Call);
					return FALSE;
				}

				SESS->HOSTSESSION->Secure_Session = 1;
			}
#endif

			strcpy(conn->Callsign, user->Call);

			//	Connected Event will trigger connect to remote system

			RefreshMainWindow();

			return TRUE;
		}
	}

	Logprintf(LOG_BBS, NULL, '|', "No Free Streams for connect to BBS %s", user->Call);

	return FALSE;
	
}

struct DelayParam
{
	struct UserInfo * User;
	CIRCUIT * conn;
	int Delay;
};

struct DelayParam DParam;		// Not 100% safe, but near enough

VOID ConnectDelayThread(struct DelayParam * DParam)
{
	struct UserInfo * User = DParam->User;
	int Delay = DParam->Delay;

	User->ForwardingInfo->Forwarding = TRUE;		// Minimize window for two connects

	Sleep(Delay);

	User->ForwardingInfo->Forwarding = TRUE;
	ConnecttoBBS(User);
	
	return;
}

VOID ConnectPauseThread(struct DelayParam * DParam)
{
	CIRCUIT * conn = DParam->conn;
	int Delay = DParam->Delay;
	char Msg[] = "Pause Ok\r    ";

	Sleep(Delay);

	ProcessBBSConnectScript(conn, Msg, 9);
	
	return;
}


/*
BOOL ProcessBBSConnectScriptInner(CIRCUIT * conn, char * Buffer, int len);


BOOL ProcessBBSConnectScript(CIRCUIT * conn, char * Buffer, int len)
{
	BOOL Ret;
	GetSemaphore(&ScriptSEM);
	Ret = ProcessBBSConnectScriptInner(conn, Buffer, len);
	FreeSemaphore(&ScriptSEM);

	return Ret;
}
*/

BOOL ProcessBBSConnectScript(CIRCUIT * conn, char * Buffer, int len)
{
	struct	BBSForwardingInfo * ForwardingInfo = conn->UserPointer->ForwardingInfo;
	char ** Scripts;
	char callsign[10];
	int port, sesstype, paclen, maxframe, l4window;
	char * ptr, * ptr2;

	WriteLogLine(conn, '<',Buffer, len-1, LOG_BBS);

	Buffer[len]=0;
	_strupr(Buffer);

	if (ForwardingInfo->TempConnectScript)
		Scripts = ForwardingInfo->TempConnectScript;
	else
		Scripts = ForwardingInfo->ConnectScript;	

	if (ForwardingInfo->ScriptIndex == -1)
	{
		// First Entry - if first line is TIMES, check and skip forward if necessary
	
		int n = 0;
		int Start, End;
		time_t now = time(NULL), StartSecs, EndSecs;
		char * Line;

		if (Localtime)
			now -= (time_t)_MYTIMEZONE; 

		now %= 86400;
		Line = Scripts[n];

		if (_memicmp(Line, "TIMES", 5) == 0)
		{
		NextBand:
			Start = atoi(&Line[6]);
			End = atoi(&Line[11]);

			StartSecs =  (time_t)(Start / 100) * 3600 + (Start % 100) * 60; 
			EndSecs =  (time_t)(End / 100) * 3600 + (End % 100) * 60 + 59;

			if ((StartSecs <= now) && (EndSecs >= now))
				goto InBand;	// In band

			// Look for next TIME
		NextLine:
			Line = Scripts[++n];

			if (Line == NULL)
			{
				// No more lines - Disconnect
			
				Disconnect(conn->BPQStream);
				return FALSE;
			}

			if (_memicmp(Line, "TIMES", 5) != 0)
				goto NextLine;
			else
				goto NextBand;
InBand:
			ForwardingInfo->ScriptIndex = n;	
		}

	}
	else
	{
		// Dont check first time through

		if (strcmp(Buffer, "*** CONNECTED  ") != 0)
		{
			if (Scripts[ForwardingInfo->ScriptIndex] == NULL ||
				_memicmp(Scripts[ForwardingInfo->ScriptIndex], "TIMES", 5) == 0	||		// Only Check until script is finished
				_memicmp(Scripts[ForwardingInfo->ScriptIndex], "ELSE", 4) == 0)			// Only Check until script is finished
			{
				ForwardingInfo->MoreLines = FALSE;
			}
			if (!ForwardingInfo->MoreLines)
				goto CheckForSID;
			}
	}

	if (strstr(Buffer, "BUSY") || strstr(Buffer, "FAILURE") ||
		(strstr(Buffer, "DOWNLINK") && strstr(Buffer, "ATTEMPTING") == 0) ||
		strstr(Buffer, "SORRY") || strstr(Buffer, "INVALID") || strstr(Buffer, "RETRIED") ||
		strstr(Buffer, "NO CONNECTION TO") || strstr(Buffer, "ERROR - ") ||
		strstr(Buffer, "UNABLE TO CONNECT") ||  strstr(Buffer, "DISCONNECTED") ||
		strstr(Buffer, "FAILED TO CONNECT"))
	{
		// Connect Failed

		char * Cmd = Scripts[++ForwardingInfo->ScriptIndex];
		int Delay = 1000;
	
		// Look for an alternative connect block (Starting with ELSE)

	ElseLoop:

		if (Cmd == 0 || _memicmp(Cmd, "TIMES", 5) == 0)			// Only Check until script is finished
		{
			Disconnect(conn->BPQStream);
			return FALSE;
		}

		if (_memicmp(Cmd, "ELSE", 4) != 0)
		{
			Cmd = Scripts[++ForwardingInfo->ScriptIndex];
			goto ElseLoop;
		}

		if (_memicmp(&Cmd[5], "DELAY", 5) == 0)
			Delay = atoi(&Cmd[10]) * 1000;
		else
			Delay = 1000;

		Disconnect(conn->BPQStream);

		DParam.Delay = Delay;
		DParam.User = conn->UserPointer;

		_beginthread(ConnectDelayThread, 0, &DParam);
		
		return FALSE;
	}

	// The pointer is only updated when we get the connect, so we can tell when the last line is acked
	// The first entry is always from Connected event, so don't have to worry about testing entry -1 below


	// NETROM to  KA node returns

	//c 1 milsw
	//WIRAC:N9PMO-2} Connected to MILSW
	//###CONNECTED TO NODE MILSW(N9ZXS) CHANNEL A
	//You have reached N9ZXS's KA-Node MILSW
	//ENTER COMMAND: B,C,J,N, or Help ?

	//C KB9PRF-7
	//###LINK MADE
	//###CONNECTED TO NODE KB9PRF-7(KB9PRF-4) CHANNEL A

	// Look for (Space)Connected so we aren't fooled by ###CONNECTED TO NODE, which is not
	// an indication of a connect.


	if (strstr(Buffer, " CONNECTED") || strstr(Buffer, "PACLEN") || strstr(Buffer, "IDLETIME") ||
			strstr(Buffer, "OK") || strstr(Buffer, "###LINK MADE") || strstr(Buffer, "VIRTUAL CIRCUIT ESTABLISHED"))
	{
		char * Cmd;

		if (conn->SkipConn)
		{
			conn->SkipConn = FALSE;
			return TRUE;
		}

	LoopBack:

		Cmd = Scripts[++ForwardingInfo->ScriptIndex];

		// Only Check until script is finished
		
		if (Cmd && (strcmp(Cmd, " ") == 0 || Cmd[0] == ';' || Cmd[0] == '#'))
			goto LoopBack;			// Blank line 

		if (Cmd && _memicmp(Cmd, "TIMES", 5) != 0 && _memicmp(Cmd, "ELSE", 4) != 0)			// Only Check until script is finished
		{
			if (_memicmp(Cmd, "MSGTYPE", 7) == 0)
			{
				char * ptr;
				
				// Select Types to send. Only send types in param. Only reverse if R in param

				_strupr(Cmd);
				
				Logprintf(LOG_BBS, conn, '?', "Script %s", Cmd);

				conn->SendB = conn->SendP = conn->SendT = conn->DoReverse = FALSE;

				strcpy(conn->MSGTYPES, &Cmd[8]);

				if (strchr(&Cmd[8], 'R')) conn->DoReverse = TRUE;

				ptr = strchr(&Cmd[8], 'B');
	
				if (ptr)
				{
					conn->SendB = TRUE;
					conn->MaxBLen = atoi(++ptr);
					if (conn->MaxBLen == 0) conn->MaxBLen = 99999999;
				}

				ptr = strchr(&Cmd[8], 'T');
	
				if (ptr)
				{
					conn->SendT = TRUE;
					conn->MaxTLen = atoi(++ptr);
					if (conn->MaxTLen == 0) conn->MaxTLen = 99999999;
				}
				ptr = strchr(&Cmd[8], 'P');

				if (ptr)
				{
					conn->SendP = TRUE;
					conn->MaxPLen = atoi(++ptr);
					if (conn->MaxPLen == 0) conn->MaxPLen = 99999999;
				}

				// If nothing to do, terminate script

				if (conn->DoReverse || SeeifMessagestoForward(conn->UserPointer->BBSNumber, conn))
					goto LoopBack;

				Logprintf(LOG_BBS, conn, '?', "Nothing to do - quitting");
				Disconnect(conn->BPQStream);
				return FALSE;
			}

			if (_memicmp(Cmd, "INTERLOCK ", 10) == 0)
			{
				// Used to limit connects on a port to 1

				int Port;
				char Option[80];

				Logprintf(LOG_BBS, conn, '?', "Script %s", Cmd);

				sscanf(&Cmd[10], "%d %s", &Port, &Option[0]);

				if (CountConnectionsOnPort(Port))
				{								
					Logprintf(LOG_BBS, conn, '?', "Interlocked Port is busy - quitting");
					Disconnect(conn->BPQStream);
					return FALSE;
				}

				goto LoopBack;
			}

			if (_memicmp(Cmd, "RADIO AUTH", 10) == 0)
			{
				// Generate a Password to enable RADIO commands on a remote node
				char AuthCommand[80];

				_strupr(Cmd);
				strcpy(AuthCommand, Cmd);

				CreateOneTimePassword(&AuthCommand[11], &Cmd[11], 0); 

				nodeprintf(conn, "%s\r", AuthCommand);
				return TRUE;
			}

			if (_memicmp(Cmd, "SKIPCON", 7) == 0)
			{
				// Remote Node sends Connected in CTEXT - we need to swallow it

				conn->SkipConn = TRUE;
				goto CheckForEnd;
			}

			if (_memicmp(Cmd, "SKIPPROMPT", 10) == 0)
			{
				// Remote Node sends > at end of CTEXT - we need to swallow it

				conn->SkipPrompt++;
				goto CheckForEnd;
			}

			if (_memicmp(Cmd, "TEXTFORWARDING", 10) == 0)
			{
				conn->BBSFlags |= TEXTFORWARDING;			
				goto CheckForEnd;
			}

			if (_memicmp(Cmd, "NEEDLF", 6) == 0)
			{
				conn->BBSFlags |= NEEDLF;			
				goto CheckForEnd;
			}

			if (_memicmp(Cmd, "FLARQ", 5) == 0)
			{
				conn->BBSFlags |= FLARQMAIL;

		CheckForEnd:
				if (Scripts[ForwardingInfo->ScriptIndex + 1] == NULL ||
						memcmp(Scripts[ForwardingInfo->ScriptIndex +1], "TIMES", 5) == 0	||		// Only Check until script is finished
						memcmp(Scripts[ForwardingInfo->ScriptIndex + 1], "ELSE", 4) == 0)			// Only Check until script is finished
					ForwardingInfo->MoreLines = FALSE;
			
				goto LoopBack;
			}
			if (_memicmp(Cmd, "PAUSE", 5) == 0)
			{
				// Pause script

				Logprintf(LOG_BBS, conn, '?', "Script %s", Cmd);

				DParam.Delay = atoi(&Cmd[6]) * 1000;
				DParam.conn = conn;

				_beginthread(ConnectPauseThread, 0, &DParam);

				return TRUE;
			}

			if (_memicmp(Cmd, "FILE", 4) == 0)
			{
				ForwardMessagestoFile(conn, &Cmd[5]);
				Disconnect(conn->BPQStream);
				return FALSE;
			}

			if (_memicmp(Cmd, "SMTP", 4) == 0)
			{
				conn->NextMessagetoForward = FirstMessageIndextoForward;
				conn->UserPointer->Total.ConnectsOut++;

				SendAMPRSMTP(conn);
				Disconnect(conn->BPQStream);
				return FALSE;
			}


			if (_memicmp(Cmd, "IMPORT", 6) == 0)
			{
				char * File, * Context;
				int Num;

				File = strtok_s(&Cmd[6], " ", &Context);

				if (File && File[0]) 
				{
					Num = ImportMessages(NULL, File, TRUE);

					Logprintf(LOG_BBS, NULL, '|', "Imported %d Message(s)", Num);

					if (Context && _stricmp(Context, "delete") == 0)
						DeleteFile(File);
				}
				Disconnect(conn->BPQStream);
				return FALSE;
			}

			nodeprintfEx(conn, "%s\r", Cmd);
			return TRUE;
		}

		// End of script.

		if (conn->BBSFlags & FLARQMAIL)
		{
			// FLARQ doesnt send a prompt - Just send message(es)

			conn->UserPointer->Total.ConnectsOut++;
			conn->BBSFlags &= ~RunningConnectScript;
			ForwardingInfo->LastReverseForward = time(NULL);

			//	Update Paclen
					
			GetConnectionInfo(conn->BPQStream, callsign, &port, &sesstype, &paclen, &maxframe, &l4window);
		
			if (paclen > 0)
				conn->paclen = paclen;

			SendARQMail(conn);
			return TRUE;
		}


		return TRUE;
	}

	ptr = strchr(Buffer, '}');

	if (ptr && ForwardingInfo->MoreLines) // Beware it could be part of ctext
	{
		// Could be respsonse to Node Command 

		ptr+=2;
		
		ptr2 = strchr(&ptr[0], ' ');

		if (ptr2)
		{
			if (_memicmp(ptr, Scripts[ForwardingInfo->ScriptIndex], ptr2-ptr) == 0)	// Reply to last sscript command
			{
				ForwardingInfo->ScriptIndex++;
		
				if (Scripts[ForwardingInfo->ScriptIndex])
					if (_memicmp(Scripts[ForwardingInfo->ScriptIndex], "TIMES", 5) != 0)	
					nodeprintf(conn, "%s\r", Scripts[ForwardingInfo->ScriptIndex]);

				return TRUE;
			}
		}
	}

	// Not Success or Fail. If last line is still outstanding, wait fot Response
	//		else look for SID or Prompt

	if (conn->SkipPrompt && Buffer[len-2] == '>')
	{
		conn->SkipPrompt--;
		return TRUE;
	}

	if (ForwardingInfo->MoreLines)
		return TRUE;

	// No more steps, Look for SID or Prompt

CheckForSID:

	if (strstr(Buffer, "SORRY, NO"))			// URONODE
	{
		Disconnect(conn->BPQStream);
		return FALSE;
	}

		if (memcmp(Buffer, ";PQ: ", 5) == 0)
		{
			// Secure CMS challenge

			int Len;
			struct UserInfo * User = conn->UserPointer;
			char * Pass = User->CMSPass;
			int Response ;
			char RespString[12];

			if (Pass[0] == 0)
			{
				Pass = User->pass;		// Old Way
				if (Pass[0] == 0)
				{
					User = LookupCall(BBSName);
					if (User)
					Pass = User->CMSPass;
				}
			}

			// 

			Response = GetCMSHash(&Buffer[5], Pass);

			sprintf(RespString, "%010d", Response);

			Len = sprintf(conn->SecureMsg, ";PR: %s\r", &RespString[2]);

			// Save challengs in case needed for FW lines

			strcpy(conn->PQChallenge, &Buffer[5]);

			return FALSE;
		}


	if (Buffer[0] == '[' && Buffer[len-2] == ']')		// SID
	{
		// Update PACLEN

		GetConnectionInfo(conn->BPQStream, callsign, &port, &sesstype, &paclen, &maxframe, &l4window);

		if (paclen > 0)
			conn->paclen = paclen;

		
		Parse_SID(conn, &Buffer[1], len-4);
			
		if (conn->BBSFlags & FBBForwarding)
		{
			conn->FBBIndex = 0;		// ready for first block;
			memset(&conn->FBBHeaders[0], 0, 5 * sizeof(struct FBBHeaderLine));
			conn->FBBChecksum = 0;
		}

		return TRUE;
	}

	if (memcmp(Buffer, "[PAKET ", 7) == 0)
	{
		conn->BBSFlags |= BBS;
		conn->BBSFlags |= MBLFORWARDING;
	}

	if (Buffer[len-2] == '>')
	{
		if (conn->SkipPrompt)
		{
			conn->SkipPrompt--;
			return TRUE;
		}

		conn->NextMessagetoForward = FirstMessageIndextoForward;
		conn->UserPointer->Total.ConnectsOut++;
		conn->BBSFlags &= ~RunningConnectScript;
		ForwardingInfo->LastReverseForward = time(NULL);

		if (memcmp(Buffer, "[AEA PK", 7) == 0 || (conn->BBSFlags & TEXTFORWARDING))
		{
			// PK232. Don't send a SID, and switch to Text Mode

			conn->BBSFlags |= (BBS | TEXTFORWARDING);
			conn->Flags |= SENDTITLE;

			// Send Message. There is no mechanism for reverse forwarding

			if (FindMessagestoForward(conn))
			{
				struct MsgInfo * Msg;
				
				// Send S line and wait for response - SB WANT @ USA < W8AAA $1029_N0XYZ 

				Msg = conn->FwdMsg;
		
				nodeprintf(conn, "S%c %s @ %s < %s $%s\r", Msg->type, Msg->to,
						(Msg->via[0]) ? Msg->via : conn->UserPointer->Call, 
						Msg->from, Msg->bid);
			}
			else
			{
					Disconnect(conn->BPQStream);
					return FALSE;
			}
			
			return TRUE;
		}

		if (strcmp(conn->Callsign, "RMS") == 0)
		{
			// Build a ;FW: line with all calls with PollRMS Set

			// According to Lee if you use secure login the first
			// must be the BBS call
			//	Actually I don't think we need the first,
			//	as that is implied

			//	If a secure password is available send the new 
			//	call|response format.

			int i, s;
			char FWLine[10000] = ";FW: ";
			struct UserInfo * user;
			char RMSCall[20];

			strcat (FWLine, BBSName);
			
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
							if (s)
							{
								sprintf(RMSCall, "%s-%d", user->Call, s);
								if (strcmp(RMSCall, BBSName) != 0)
								{
									strcat(FWLine, " ");
									strcat(FWLine, RMSCall);
								}
							}
							else
							{
								if (strcmp(user->Call, BBSName) == 0)
								{
									// Dont include BBS call - was put on front
									goto NoPass;
								}
								
								strcat(FWLine, " ");
								strcat(FWLine, user->Call);
							}

							if (user->CMSPass[0])
							{
								int Response = GetCMSHash(conn->PQChallenge, user->CMSPass);
								char RespString[12];

								sprintf(RespString, "%010d", Response);
								strcat(FWLine, "|");
								strcat(FWLine, &RespString[2]);
							}
NoPass:;
						}
					}
				}
			}
			
			strcat(FWLine, "\r");	

			nodeprintf(conn, FWLine);
		}

		// Only declare B1 and B2 if other end did, and we are configued for it

		nodeprintfEx(conn, BBSSID, "BPQ-",
			Ver[0], Ver[1], Ver[2], Ver[3],
			(conn->BBSFlags & FBBCompressed) ? "B" : "", 
			(conn->BBSFlags & FBBB1Mode && !(conn->BBSFlags & FBBB2Mode)) ? "1" : "",
			(conn->BBSFlags & FBBB2Mode) ? "2" : "",
			(conn->BBSFlags & FBBForwarding) ? "F" : ""); 

		if (conn->SecureMsg[0])
		{
			BBSputs(conn, conn->SecureMsg);
			conn->SecureMsg[0] = 0;
		}
					
		if (conn->BPQBBS && conn->MSGTYPES[0])

			// Send a ; MSGTYPES to control what he sends us

			nodeprintf(conn, "; MSGTYPES %s\r", conn->MSGTYPES);

		if (conn->BBSFlags & FBBForwarding)
		{
			if (!FBBDoForward(conn))				// Send proposal if anthing to forward
			{
				if (conn->DoReverse)
					FBBputs(conn, "FF\r");
				else
				{
					FBBputs(conn, "FQ\r");
					conn->CloseAfterFlush = 20;			// 2 Secs
				}
			}

			return TRUE;
		}

		return TRUE;
	}

	return TRUE;
}

VOID Parse_SID(CIRCUIT * conn, char * SID, int len)
{
	ChangeSessionIdletime(conn->BPQStream, BBSIDLETIME);		// Default Idletime for BBS Sessions

	// scan backwards for first '-'

	if (strstr(SID, "BPQCHATSERVER"))
	{
		Disconnect(conn->BPQStream);
		return;
	}

	if (strstr(SID, "RMS Ex"))
	{
		conn->RMSExpress = TRUE;
		conn->Paclink = FALSE;

		// Set new RMS Users as RMS User

		if (conn->NewUser)
			conn->UserPointer->flags |= F_Temp_B2_BBS;
	}

	if (strstr(SID, "Paclink"))
	{
		conn->RMSExpress = FALSE;
		conn->Paclink = TRUE;
	}

	if (_memicmp(SID, "OpenBCM", 7) == 0)
	{
		conn->OpenBCM = TRUE;
	}


	// See if BPQ for selective forwarding 

	if (strstr(SID, "BPQ"))
		conn->BPQBBS = TRUE;

	while (len > 0)
	{
		switch (SID[len--])
		{
		case '-':

			len=0;
			break;

		case '$':

			conn->BBSFlags |= BBS | MBLFORWARDING;
			conn->Paging = FALSE;

			break;

		case 'F':			// FBB Blocked Forwarding
		
			if (conn->UserPointer->flags & F_PMS)
			{
				// We need to allocate a forwarding structure

					conn->UserPointer->ForwardingInfo = zalloc(sizeof(struct BBSForwardingInfo));
					conn->UserPointer->ForwardingInfo->AllowCompressed = TRUE;
					conn->UserPointer->BBSNumber = NBBBS;
			}

			if (conn->UserPointer->ForwardingInfo->AllowCompressed)
			{
				conn->BBSFlags |= FBBForwarding | BBS;
				conn->BBSFlags &= ~MBLFORWARDING;
		
				conn->Paging = FALSE;

				// Allocate a Header Block

				conn->FBBHeaders = zalloc(5 * sizeof(struct FBBHeaderLine));
			}
			break;

		case 'B':

			if (conn->UserPointer->ForwardingInfo->AllowCompressed)
			{
				conn->BBSFlags |= FBBCompressed;
				conn->DontSaveRestartData = FALSE;		// Alow restarts

//				if (conn->UserPointer->ForwardingInfo->AllowB1) // !!!!! Testing !!!!
//					conn->BBSFlags |= FBBB1Mode;

				
				// Look for 1 or 2 or 12 as next 2 chars

				if (SID[len+2] == '1')
				{
					if (conn->UserPointer->ForwardingInfo->AllowB1 ||
						conn->UserPointer->ForwardingInfo->AllowB2)		// B2 implies B1
						conn->BBSFlags |= FBBB1Mode;

					if (SID[len+3] == '2')
						if (conn->UserPointer->ForwardingInfo->AllowB2)
							conn->BBSFlags |= FBBB1Mode | FBBB2Mode;	// B2 uses B1 mode (crc on front of file)

					break;
				}

				if (SID[len+2] == '2')
				{
					if (conn->UserPointer->ForwardingInfo->AllowB2)
							conn->BBSFlags |= FBBB1Mode | FBBB2Mode;	// B2 uses B1 mode (crc on front of file)
	
					if (conn->UserPointer->ForwardingInfo->AllowB1)
							conn->BBSFlags |= FBBB1Mode;				// B2 should allow fallback to B1 (but RMS doesnt!)

				}
				break;
			}

			break;
		}
	}
	return;
}

VOID BBSSlowTimer()
{
	ConnectionInfo * conn;
	int n;

	for (n = 0; n < NumberofStreams; n++)
	{
		conn = &Connections[n];
		
		if (conn->Active == TRUE)
		{
			//	Check SIDTImers - used to detect failure to compete SID Handshake

			if (conn->SIDResponseTimer)
			{
				conn->SIDResponseTimer--;
				if (conn->SIDResponseTimer == 0)
				{
					// Disconnect Session

					Disconnect(conn->BPQStream);
				}
			}
		}
	}
}


VOID FWDTimerProc()
{
	struct UserInfo * user;
	struct	BBSForwardingInfo * ForwardingInfo ;
	time_t NOW = time(NULL);

	for (user = BBSChain; user; user = user->BBSNext)
	{
		// See if any messages are queued for this BBS

		ForwardingInfo = user->ForwardingInfo;
		ForwardingInfo->FwdTimer+=10;

		if (ForwardingInfo->FwdTimer >= ForwardingInfo->FwdInterval)
		{
			ForwardingInfo->FwdTimer=0;

			if (ForwardingInfo->FWDBands && ForwardingInfo->FWDBands[0])
			{
				// Check Timebands

				struct FWDBAND ** Bands = ForwardingInfo->FWDBands;
				int Count = 0;
				time_t now = time(NULL);
						
				if (Localtime)
					now -= (time_t)_MYTIMEZONE; 

				now %= 86400;		// Secs in day

				while(Bands[Count])
				{
					if ((Bands[Count]->FWDStartBand < now) && (Bands[Count]->FWDEndBand >= now))
						goto FWD;	// In band

				Count++;
				}
				continue;				// Out of bands
			}
		FWD:	

				if (ForwardingInfo->Enabled)
					if (ForwardingInfo->ConnectScript  && (ForwardingInfo->Forwarding == 0) && ForwardingInfo->ConnectScript[0])
						if (SeeifMessagestoForward(user->BBSNumber, NULL) ||
							(ForwardingInfo->ReverseFlag && ((NOW - ForwardingInfo->LastReverseForward) >= ForwardingInfo->RevFwdInterval))) // Menu Command overrides Reverse

						{
							user->ForwardingInfo->ScriptIndex = -1;			 // Incremented before being used


							// remove any old TempScript

							if (user->ForwardingInfo->TempConnectScript)
							{
								FreeList(user->ForwardingInfo->TempConnectScript);
								user->ForwardingInfo->TempConnectScript = NULL;
							}

							if (ConnecttoBBS(user))
								ForwardingInfo->Forwarding = TRUE;					
						}
		}
	}
}

// R:090209/0128Z 33040@N4JOA.#WPBFL.FL.USA.NOAM [164113] FBB7.01.35 alpha


char * DateAndTimeForHLine(time_t Datim)
{
	struct tm *newtime;
    char * Time;
	static char Date[]="yymmdd/hhmmZ";
  
	newtime = gmtime(&Datim);
	Time = asctime(newtime);
	Date[0]=Time[22];
	Date[1]=Time[23];
	Date[3]=Time[4];
	Date[4]=Time[5];
	Date[5]=Time[6];
	
	return Date;
}




VOID * _zalloc_dbg(int len, int type, char * file, int line)
{
	// ?? malloc and clear

	void * ptr;

#ifdef WIN32
	ptr=_malloc_dbg(len, type, file, line);
#else
	ptr = malloc(len);
#endif
	if (ptr)
		memset(ptr, 0, len);

	return ptr;
}


struct MsgInfo * FindMessageByNumber(int msgno)
 {
	int m=NumberofMessages;

	struct MsgInfo * Msg;

	do
	{
		Msg=MsgHddrPtr[m];

		if (Msg->number == msgno)
			return Msg;

		if (Msg->number < msgno)
			return NULL;			// Not found

		m--;

	} while (m > 0);

	return NULL;
}

VOID DecryptPass(char * Encrypt, unsigned char * Pass, unsigned int len)
{
	unsigned char hash[50];
	unsigned char key[100];
	unsigned int i, j = 0, val1, val2;
	unsigned char hostname[100]="";

	gethostname(hostname, 100);

	strcpy(key, hostname);
	strcat(key, ISPPOP3Name);

	md5(key, hash);
	memcpy(&hash[16], hash, 16);	// in case very long password

	// String is now encoded as hex pairs, but still need to decode old format

	for (i=0; i < len; i++)
	{
		if (Encrypt[i] < '0' || Encrypt[i] > 'F')
			goto OldFormat;
	}

	// Only '0' to 'F'

	for (i=0; i < len; i++)
	{
		val1 = Encrypt[i++];
		val1 -= '0';
		if (val1 > 9)
			val1 -= 7;

		val2 = Encrypt[i];
		val2 -= '0';
		if (val2 > 9)
			val2 -= 7;

		Pass[j] =  (val1 << 4) | val2;
		Pass[j] ^= hash[j];
		j++;
	}

	return;

OldFormat:

	for (i=0; i < len; i++)
	{
		Pass[i] =  Encrypt[i] ^ hash[i];
	}

	return;
}

int EncryptPass(char * Pass, char * Encrypt)
{
	unsigned char hash[50];
	unsigned char key[100];
	unsigned int i, val;
	unsigned char hostname[100];
	unsigned char extendedpass[100];
	unsigned int passlen;
	unsigned char * ptr;

	gethostname(hostname, 100);

	strcpy(key, hostname);
	strcat(key, ISPPOP3Name);

	md5(key, hash);
	memcpy(&hash[16], hash, 16);	// in case very long password

	// if password is less than 16 chars, extend with zeros

	passlen=strlen(Pass);

	strcpy(extendedpass, Pass);

	if (passlen < 16)
	{
		for  (i=passlen+1; i <= 16; i++)
		{
			extendedpass[i] = 0;
		}

		passlen = 16;
	}

	ptr = Encrypt;
	Encrypt[0] = 0;

	for (i=0; i < passlen; i++)
	{
		val = extendedpass[i] ^ hash[i];
		ptr += sprintf(ptr, "%02X", val);
	}

	return passlen * 2;
}



VOID SaveIntValue(config_setting_t * group, char * name, int value)
{
	config_setting_t *setting;
	
	setting = config_setting_add(group, name, CONFIG_TYPE_INT);
	if(setting)
		config_setting_set_int(setting, value);
}

VOID SaveStringValue(config_setting_t * group, char * name, char * value)
{
	config_setting_t *setting;

	setting = config_setting_add(group, name, CONFIG_TYPE_STRING);
	if (setting)
		config_setting_set_string(setting, value);

}


VOID SaveOverride(config_setting_t * group, char * name, struct Override ** values)
{
	config_setting_t *setting;
	struct Override ** Calls;
	char Multi[10000];
	char * ptr = &Multi[1];

	*ptr = 0;

	if (values)
	{
		Calls = values;

		while(Calls[0])
		{
			ptr += sprintf(ptr, "%s, %d|", Calls[0]->Call, Calls[0]->Days);
			Calls++;
		}
		*(--ptr) = 0;
	}

	setting = config_setting_add(group, name, CONFIG_TYPE_STRING);
	if (setting)
		config_setting_set_string(setting, &Multi[1]);

}


VOID SaveMultiStringValue(config_setting_t * group, char * name, char ** values)
{
	config_setting_t *setting;
	char ** Calls;
	char Multi[10000];
	char * ptr = &Multi[1];

	*ptr = 0;

	if (values)
	{
		Calls = values;

		while(Calls[0])
		{
			strcpy(ptr, Calls[0]);
			ptr += strlen(Calls[0]);
			*(ptr++) = '|';
			Calls++;
		}
		*(--ptr) = 0;
	}

	setting = config_setting_add(group, name, CONFIG_TYPE_STRING);
	if (setting)
		config_setting_set_string(setting, &Multi[1]);

}


VOID SaveConfig(char * ConfigName)
{
	struct UserInfo * user;
	struct	BBSForwardingInfo * ForwardingInfo ;
	config_setting_t *root, *group, *bbs;
	int i;
	char Size[80];
	struct BBSForwardingInfo DummyForwardingInfo;
	
	memset(&DummyForwardingInfo, 0, sizeof(struct BBSForwardingInfo));

	//	Get rid of old config before saving
	
	memset((void *)&cfg, 0, sizeof(config_t));

	config_init(&cfg);

	root = config_root_setting(&cfg);

	group = config_setting_add(root, "main", CONFIG_TYPE_GROUP);

	SaveIntValue(group, "Streams", MaxStreams);
	SaveIntValue(group, "BBSApplNum", BBSApplNum);
	SaveStringValue(group, "BBSName", BBSName);
	SaveStringValue(group, "SYSOPCall", SYSOPCall);
	SaveStringValue(group, "H-Route", HRoute);
	SaveStringValue(group, "AMPRDomain", AMPRDomain);
	SaveIntValue(group, "EnableUI", EnableUI);
	SaveIntValue(group, "RefuseBulls", RefuseBulls);
	SaveIntValue(group, "SendSYStoSYSOPCall", SendSYStoSYSOPCall);
	SaveIntValue(group, "SendBBStoSYSOPCall", SendBBStoSYSOPCall);
	SaveIntValue(group, "DontHoldNewUsers", DontHoldNewUsers);
	SaveIntValue(group, "AllowAnon", AllowAnon);
	SaveIntValue(group, "DontNeedHomeBBS", DontNeedHomeBBS);

	SaveIntValue(group, "ForwardToMe", ForwardToMe);
	SaveIntValue(group, "SMTPPort", SMTPInPort);
	SaveIntValue(group, "POP3Port", POP3InPort);
	SaveIntValue(group, "NNTPPort", NNTPInPort);
	SaveIntValue(group, "RemoteEmail", RemoteEmail);
	SaveIntValue(group, "SendAMPRDirect", SendAMPRDirect);

	SaveIntValue(group, "MailForInterval", MailForInterval);
	SaveStringValue(group, "MailForText", MailForText);

	EncryptedPassLen = EncryptPass(ISPAccountPass, EncryptedISPAccountPass);

	SaveIntValue(group, "AuthenticateSMTP", SMTPAuthNeeded);

	SaveIntValue(group, "Log_BBS", LogBBS);
	SaveIntValue(group, "Log_TCP", LogTCP);

	SaveIntValue(group, "SMTPGatewayEnabled", ISP_Gateway_Enabled);
	SaveIntValue(group, "ISPSMTPPort", ISPSMTPPort);
	SaveIntValue(group, "ISPPOP3Port", ISPPOP3Port);
	SaveIntValue(group, "POP3PollingInterval", ISPPOP3Interval);

	SaveStringValue(group, "MyDomain", MyDomain);
	SaveStringValue(group, "ISPSMTPName", ISPSMTPName);
	SaveStringValue(group, "ISPPOP3Name", ISPPOP3Name);
	SaveStringValue(group, "ISPAccountName", ISPAccountName);
	SaveStringValue(group, "ISPAccountPass", EncryptedISPAccountPass);


	//	Save Window Sizes
	
#ifndef LINBPQ

	if (ConsoleRect.right)
	{
		sprintf(Size,"%d,%d,%d,%d",ConsoleRect.left, ConsoleRect.right,
			ConsoleRect.top, ConsoleRect.bottom);

		SaveStringValue(group, "ConsoleSize", Size);
	}
	
	sprintf(Size,"%d,%d,%d,%d,%d",MonitorRect.left,MonitorRect.right,MonitorRect.top,MonitorRect.bottom, hMonitor ? 1 : 0);
	SaveStringValue(group, "MonitorSize", Size);

	sprintf(Size,"%d,%d,%d,%d",MainRect.left,MainRect.right,MainRect.top,MainRect.bottom);
	SaveStringValue(group, "WindowSize", Size);

	SaveIntValue(group, "Bells", Bells);
	SaveIntValue(group, "FlashOnBell", FlashOnBell);
	SaveIntValue(group, "StripLF", StripLF);
	SaveIntValue(group, "WarnWrap", WarnWrap);
	SaveIntValue(group, "WrapInput", WrapInput);
	SaveIntValue(group, "FlashOnConnect", FlashOnConnect);
	SaveIntValue(group, "CloseWindowOnBye", CloseWindowOnBye);

#endif

	SaveIntValue(group, "Log_BBS", LogBBS);
	SaveIntValue(group, "Log_TCP", LogTCP);

	sprintf(Size,"%d,%d,%d,%d", Ver[0], Ver[1], Ver[2], Ver[3]);
	SaveStringValue(group, "Version", Size);

	// Save Welcome Messages and prompts

	SaveStringValue(group, "WelcomeMsg", WelcomeMsg);
	SaveStringValue(group, "NewUserWelcomeMsg", NewWelcomeMsg);
	SaveStringValue(group, "ExpertWelcomeMsg", ExpertWelcomeMsg);
	
	SaveStringValue(group, "Prompt", Prompt);
	SaveStringValue(group, "NewUserPrompt", NewPrompt);
	SaveStringValue(group, "ExpertPrompt", ExpertPrompt);
	SaveStringValue(group, "SignoffMsg", SignoffMsg);

	SaveMultiStringValue(group,  "RejFrom", RejFrom);
	SaveMultiStringValue(group,  "RejTo", RejTo);
	SaveMultiStringValue(group,  "RejAt", RejAt);

	SaveMultiStringValue(group,  "HoldFrom", HoldFrom);
	SaveMultiStringValue(group,  "HoldTo", HoldTo);
	SaveMultiStringValue(group,  "HoldAt", HoldAt);

	SaveIntValue(group, "SendWP", SendWP);

	SaveStringValue(group, "SendWPTO", SendWPTO);
	SaveStringValue(group, "SendWPVIA", SendWPVIA);
	SaveIntValue(group, "SendWPType", SendWPType);

	// Save Forwarding Config

	// Interval and Max Sizes and Aliases are not user specific

	SaveIntValue(group, "MaxTXSize", MaxTXSize);
	SaveIntValue(group, "MaxRXSize", MaxRXSize);
	SaveIntValue(group, "ReaddressLocal", ReaddressLocal);
	SaveIntValue(group, "ReaddressReceived", ReaddressReceived);
	SaveIntValue(group, "WarnNoRoute", WarnNoRoute);
	SaveIntValue(group, "Localtime", Localtime);
	SaveIntValue(group, "WarnNoRoute", WarnNoRoute);

	SaveMultiStringValue(group, "FWDAliases", AliasText);

	bbs = config_setting_add(root, "BBSForwarding", CONFIG_TYPE_GROUP);

	for (i=1; i <= NumberofUsers; i++)
	{
		user = UserRecPtr[i];
		ForwardingInfo = user->ForwardingInfo;

		if (ForwardingInfo == NULL)
			continue;

		if (memcmp(ForwardingInfo, &DummyForwardingInfo, sizeof(struct BBSForwardingInfo)) == 0)
			continue;		// Ignore empty records;

		if (isdigit(user->Call[0]))
		{
			char Key[20] = "*";
			strcat (Key, user->Call); 
			group = config_setting_add(bbs, Key, CONFIG_TYPE_GROUP);
		}
		else
			group = config_setting_add(bbs, user->Call, CONFIG_TYPE_GROUP);

		SaveMultiStringValue(group, "TOCalls", ForwardingInfo->TOCalls);
		SaveMultiStringValue(group, "ConnectScript", ForwardingInfo->ConnectScript);
		SaveMultiStringValue(group, "ATCalls", ForwardingInfo->ATCalls);
		SaveMultiStringValue(group, "HRoutes", ForwardingInfo->Haddresses);
		SaveMultiStringValue(group, "HRoutesP", ForwardingInfo->HaddressesP);
		SaveMultiStringValue(group, "FWDTimes", ForwardingInfo->FWDTimes);
	
		SaveIntValue(group, "Enabled", ForwardingInfo->Enabled);
		SaveIntValue(group, "RequestReverse", ForwardingInfo->ReverseFlag);
		SaveIntValue(group, "AllowCompressed", ForwardingInfo->AllowCompressed);
		SaveIntValue(group, "UseB1Protocol", ForwardingInfo->AllowB1);
		SaveIntValue(group, "UseB2Protocol", ForwardingInfo->AllowB2);
		SaveIntValue(group, "SendCTRLZ", ForwardingInfo->SendCTRLZ);
				
		SaveIntValue(group, "FWDPersonalsOnly", ForwardingInfo->PersonalOnly);
		SaveIntValue(group, "FWDNewImmediately", ForwardingInfo->SendNew);
		SaveIntValue(group, "FwdInterval", ForwardingInfo->FwdInterval);
		SaveIntValue(group, "RevFWDInterval", ForwardingInfo->RevFwdInterval);
		SaveIntValue(group, "MaxFBBBlock", ForwardingInfo->MaxFBBBlockSize);

		SaveStringValue(group, "BBSHA", ForwardingInfo->BBSHA);
	}


	// Save Housekeeping config

	group = config_setting_add(root, "Housekeeping", CONFIG_TYPE_GROUP);

	SaveIntValue(group, "LastHouseKeepingTime", LastHouseKeepingTime);
	SaveIntValue(group, "LastTrafficTime", LastTrafficTime);
	SaveIntValue(group, "MaxMsgno", MaxMsgno);
	SaveIntValue(group, "BidLifetime", BidLifetime);
	SaveIntValue(group, "MaxAge", MaxAge);
	SaveIntValue(group, "LogLifetime", LogAge);
	SaveIntValue(group, "LogLifetime", LogAge);
	SaveIntValue(group, "MaintInterval", MaintInterval);
	SaveIntValue(group, "UserLifetime", UserLifetime);
	SaveIntValue(group, "MaintTime", MaintTime);
	SaveIntValue(group, "PR", PR);
	SaveIntValue(group, "PUR", PUR);
	SaveIntValue(group, "PF", PF);
	SaveIntValue(group, "PNF", PNF);
	SaveIntValue(group, "BF", BF);
	SaveIntValue(group, "BNF", BNF);
	SaveIntValue(group, "NTSD", NTSD);
	SaveIntValue(group, "NTSF", NTSF);
	SaveIntValue(group, "NTSU", NTSU);
//	SaveIntValue(group, "AP", AP);
//	SaveIntValue(group, "AB", AB);
	SaveIntValue(group, "DeletetoRecycleBin", DeletetoRecycleBin);
	SaveIntValue(group, "SuppressMaintEmail", SuppressMaintEmail);
	SaveIntValue(group, "MaintSaveReg", SaveRegDuringMaint);
	SaveIntValue(group, "OverrideUnsent", OverrideUnsent);
	SaveIntValue(group, "SendNonDeliveryMsgs", SendNonDeliveryMsgs);

	SaveOverride(group, "LTFROM", LTFROM);
	SaveOverride(group, "LTTO", LTTO);
	SaveOverride(group, "LTAT", LTAT);

	// Save UI config

	for (i=1; i<=32; i++)
	{
		char Key[100];
			
		sprintf(Key, "UIPort%d", i);

		group = config_setting_add(root, Key, CONFIG_TYPE_GROUP);

		if (group)
		{
			SaveIntValue(group, "Enabled", UIEnabled[i]);
			SaveIntValue(group, "SendMF", UIMF[i]);
			SaveIntValue(group, "SendHDDR", UIHDDR[i]);
			SaveIntValue(group, "SendNull", UINull[i]);
	
			if (UIDigi[i])
				SaveStringValue(group, "Digis", UIDigi[i]);
		}
	}

	if(! config_write_file(&cfg, ConfigName))
	{
		fprintf(stderr, "Error while writing file.\n");
		config_destroy(&cfg);
		return;
	}
	config_destroy(&cfg);

/*

#ifndef LINBPQ

	//	Save a copy with current Date/Time Stamp for debugging

	{
		char Backup[MAX_PATH];
		time_t LT;
		struct tm * tm;

		LT = time(NULL);
		tm = gmtime(&LT);	

		sprintf(Backup,"%s.%02d%02d%02d%02d%02d.save", ConfigName, tm->tm_year-100, tm->tm_mon+1,
			tm->tm_mday, tm->tm_hour, tm->tm_min);

		CopyFile(ConfigName, Backup, FALSE);	// Copy to .bak
	}
#endif
*/
}

int GetIntValue(config_setting_t * group, char * name)
{
	config_setting_t *setting;

	setting = config_setting_get_member (group, name);
	if (setting)
		return config_setting_get_int (setting);

	return 0;
}

int GetIntValueWithDefault(config_setting_t * group, char * name, int Default)
{
	config_setting_t *setting;

	setting = config_setting_get_member (group, name);
	if (setting)
		return config_setting_get_int (setting);

	return Default;
}


BOOL GetStringValue(config_setting_t * group, char * name, char * value)
{
	const char * str;
	config_setting_t *setting;

	setting = config_setting_get_member (group, name);
	if (setting)
	{
		str =  config_setting_get_string (setting);
		strcpy(value, str);
		return TRUE;
	}
	return FALSE;
}

BOOL GetConfig(char * ConfigName)
{
	int i;
	char Size[80];
	config_setting_t *setting;
	const char * ptr;

	config_init(&cfg);

	/* Read the file. If there is an error, report it and exit. */
	
	if(! config_read_file(&cfg, ConfigName))
	{
		char Msg[256];
		sprintf(Msg, "Config FIle Line %d - %s\n",
			config_error_line(&cfg), config_error_text(&cfg));
#ifdef WIN32
		MessageBox(NULL, Msg, "BPQMail", MB_ICONSTOP);
#else
		printf("%s", Msg);
#endif
		config_destroy(&cfg);
		return(EXIT_FAILURE);
	}

	group = config_lookup (&cfg, "main");

	if (group == NULL)
		return EXIT_FAILURE;

	SMTPInPort =  GetIntValue(group, "SMTPPort");
	POP3InPort =  GetIntValue(group, "POP3Port");
	NNTPInPort =  GetIntValue(group, "NNTPPort");
	RemoteEmail =  GetIntValue(group, "RemoteEmail");
	MaxStreams =  GetIntValue(group, "Streams");
	BBSApplNum =  GetIntValue(group, "BBSApplNum");
	EnableUI =  GetIntValue(group, "EnableUI");
	MailForInterval =  GetIntValue(group, "MailForInterval");
	RefuseBulls =  GetIntValue(group, "RefuseBulls");
	SendSYStoSYSOPCall =  GetIntValue(group, "SendSYStoSYSOPCall");
	SendBBStoSYSOPCall =  GetIntValue(group, "SendBBStoSYSOPCall");
	DontHoldNewUsers =  GetIntValue(group, "DontHoldNewUsers");
	ForwardToMe =  GetIntValue(group, "ForwardToMe");
	AllowAnon =  GetIntValue(group, "AllowAnon");
	DontNeedHomeBBS =  GetIntValue(group, "DontNeedHomeBBS");
	MaxTXSize =  GetIntValue(group, "MaxTXSize");
	MaxRXSize =  GetIntValue(group, "MaxRXSize");
	ReaddressLocal =  GetIntValue(group, "ReaddressLocal");
	ReaddressReceived =  GetIntValue(group, "ReaddressReceived");
	WarnNoRoute =  GetIntValue(group, "WarnNoRoute");
	Localtime =  GetIntValue(group, "Localtime");
	AliasText = GetMultiStringValue(group, "FWDAliases");
	GetStringValue(group, "BBSName", BBSName);
	GetStringValue(group, "MailForText", MailForText);
	GetStringValue(group, "SYSOPCall", SYSOPCall);
	GetStringValue(group, "H-Route", HRoute);
	GetStringValue(group, "AMPRDomain", AMPRDomain);
	SendAMPRDirect = GetIntValue(group, "SendAMPRDirect");
	ISP_Gateway_Enabled =  GetIntValue(group, "SMTPGatewayEnabled");
	ISPPOP3Interval =  GetIntValue(group, "POP3PollingInterval");
	GetStringValue(group, "MyDomain", MyDomain);
	GetStringValue(group, "ISPSMTPName", ISPSMTPName);
	GetStringValue(group, "ISPPOP3Name", ISPPOP3Name);
	ISPSMTPPort = GetIntValue(group, "ISPSMTPPort");
	ISPPOP3Port = GetIntValue(group, "ISPPOP3Port");
	GetStringValue(group, "ISPAccountName", ISPAccountName);
	GetStringValue(group, "ISPAccountPass", EncryptedISPAccountPass);
	GetStringValue(group, "ISPAccountName", ISPAccountName);

	sprintf(SignoffMsg, "73 de %s\r", BBSName);					// Default
	GetStringValue(group, "SignoffMsg", SignoffMsg);

	DecryptPass(EncryptedISPAccountPass, ISPAccountPass, strlen(EncryptedISPAccountPass));

	SMTPAuthNeeded = GetIntValue(group, "AuthenticateSMTP");
	LogBBS = GetIntValue(group, "Log_BBS");
	LogTCP = GetIntValue(group, "Log_TCP");

#ifndef LINBPQ

	GetStringValue(group, "MonitorSize", Size);
	sscanf(Size,"%d,%d,%d,%d,%d",&MonitorRect.left,&MonitorRect.right,&MonitorRect.top,&MonitorRect.bottom,&OpenMon);
	
	GetStringValue(group, "WindowSize", Size);
	sscanf(Size,"%d,%d,%d,%d",&MainRect.left,&MainRect.right,&MainRect.top,&MainRect.bottom);

	Bells = GetIntValue(group, "Bells");

	FlashOnBell = GetIntValue(group, "FlashOnBell");			

	StripLF	 = GetIntValue(group, "StripLF");	
	CloseWindowOnBye = GetIntValue(group, "CloseWindowOnBye");			
	WarnWrap = GetIntValue(group, "WarnWrap");
	WrapInput = GetIntValue(group, "WrapInput");			
	FlashOnConnect = GetIntValue(group, "FlashOnConnect");			
	
	GetStringValue(group, "ConsoleSize", Size);
	sscanf(Size,"%d,%d,%d,%d", &ConsoleRect.left, &ConsoleRect.right,
			&ConsoleRect.top, &ConsoleRect.bottom,&OpenConsole);

#endif

	// Get Welcome Messages

	setting = config_setting_get_member (group, "WelcomeMsg");

	if (setting && setting->value.sval[0])
	{
		ptr =  config_setting_get_string (setting);
		WelcomeMsg = _strdup(ptr);
	}
	else
		WelcomeMsg = _strdup("Hello $I. Latest Message is $L, Last listed is $Z\r\n");


	setting = config_setting_get_member (group, "NewUserWelcomeMsg");
	
	if (setting && setting->value.sval[0])
	{
		ptr =  config_setting_get_string (setting);
		NewWelcomeMsg = _strdup(ptr);
	}
	else
		NewWelcomeMsg = _strdup("Hello $I. Latest Message is $L, Last listed is $Z\r\n");


	setting = config_setting_get_member (group, "ExpertWelcomeMsg");
	
	if (setting && setting->value.sval[0])
	{
		ptr =  config_setting_get_string (setting);
		ExpertWelcomeMsg = _strdup(ptr);
	}
	else
		ExpertWelcomeMsg = _strdup("");

	// Get Prompts

	setting = config_setting_get_member (group, "Prompt");
	
	if (setting && setting->value.sval[0])
	{
		ptr =  config_setting_get_string (setting);
		Prompt = _strdup(ptr);
	}
	else
	{
		Prompt = malloc(20);
		sprintf(Prompt, "de %s>\r\n", BBSName);
	}

	setting = config_setting_get_member (group, "NewUserPrompt");
	
	if (setting && setting->value.sval[0])
	{
		ptr =  config_setting_get_string (setting);
		NewPrompt = _strdup(ptr);
	}
	else
	{
		NewPrompt = malloc(20);
		sprintf(NewPrompt, "de %s>\r\n", BBSName);
	}

	setting = config_setting_get_member (group, "ExpertPrompt");
	
	if (setting && setting->value.sval[0])
	{
		ptr =  config_setting_get_string (setting);
		ExpertPrompt = _strdup(ptr);
	}
	else
	{
		ExpertPrompt = malloc(20);
		sprintf(ExpertPrompt, "de %s>\r\n", BBSName);
	}

	TidyPrompts();

	RejFrom = GetMultiStringValue(group,  "RejFrom");
	RejTo = GetMultiStringValue(group,  "RejTo");
	RejAt = GetMultiStringValue(group,  "RejAt");

	HoldFrom = GetMultiStringValue(group,  "HoldFrom");
	HoldTo = GetMultiStringValue(group,  "HoldTo");
	HoldAt = GetMultiStringValue(group,  "HoldAt");

	// Send WP Params
	
	SendWP = GetIntValue(group, "SendWP");

	GetStringValue(group, "SendWPTO", SendWPTO);
	GetStringValue(group, "SendWPVIA", SendWPVIA);
	SendWPType = GetIntValue(group, "SendWPType");


	GetStringValue(group, "Version", Size);
	sscanf(Size,"%d,%d,%d,%d", &LastVer[0], &LastVer[1], &LastVer[2], &LastVer[3]);


	for (i=1; i<=32; i++)
	{
		char Key[100];
			
		sprintf(Key, "UIPort%d", i);

		group = config_lookup (&cfg, Key);

		if (group)
		{
			UIEnabled[i] = GetIntValue(group, "Enabled");
			UIMF[i] = GetIntValueWithDefault(group, "SendMF", UIEnabled[i]);
			UIHDDR[i] = GetIntValueWithDefault(group, "SendHDDR", UIEnabled[i]);
			UINull[i] = GetIntValue(group, "SendNull");
			Size[0] = 0;
			GetStringValue(group, "Digis", Size);
			if (Size[0])
				UIDigi[i] = _strdup(Size);
		}
	}

	 group = config_lookup (&cfg, "Housekeeping");

	 if (group)
	 {
		 LastHouseKeepingTime = GetIntValue(group, "LastHouseKeepingTime");
		 LastTrafficTime = GetIntValue(group, "LastTrafficTime");
		 MaxMsgno = GetIntValue(group, "MaxMsgno");
		 LogAge = GetIntValue(group, "LogLifetime");
		 BidLifetime = GetIntValue(group, "BidLifetime");
		 MaxAge = GetIntValue(group, "MaxAge");
		 if (MaxAge == 0)
			 MaxAge = 30;
		 UserLifetime = GetIntValue(group, "UserLifetime");
		 MaintInterval = GetIntValue(group, "MaintInterval");
		 MaintTime = GetIntValue(group, "MaintTime");
	
		 PR = GetIntValue(group, "PR");
		 PUR = GetIntValue(group, "PUR");
		 PF = GetIntValue(group, "PF");
		 PNF = GetIntValue(group, "PNF");
		 BF = GetIntValue(group, "BF");
		 BNF = GetIntValue(group, "BNF");
		 NTSD = GetIntValue(group, "NTSD");
		 NTSU = GetIntValue(group, "NTSU");
		 NTSF = GetIntValue(group, "NTSF");
//		 AP = GetIntValue(group, "AP");
//		 AB = GetIntValue(group, "AB");
		 DeletetoRecycleBin = GetIntValue(group, "DeletetoRecycleBin");
		 SuppressMaintEmail = GetIntValue(group, "SuppressMaintEmail");
		 SaveRegDuringMaint = GetIntValue(group, "MaintSaveReg");
		 OverrideUnsent = GetIntValue(group, "OverrideUnsent");
		 SendNonDeliveryMsgs = GetIntValue(group, "SendNonDeliveryMsgs");
		 OverrideUnsent = GetIntValue(group, "OverrideUnsent");
	
		 LTFROM = GetOverrides(group,  "LTFROM");
		 LTTO = GetOverrides(group,  "LTTO");
		 LTAT = GetOverrides(group,  "LTAT");
	}
	 return EXIT_SUCCESS;
}

#ifdef LINBPQ
extern BPQVECSTRUC ** BPQHOSTVECPTR;
#else
__declspec(dllimport) BPQVECSTRUC ** BPQHOSTVECPTR;
#endif

int Connected(int Stream)
{
	int n, Mask;
	CIRCUIT * conn;
	struct UserInfo * user = NULL;
	char callsign[10];
	int port, paclen, maxframe, l4window;
	char ConnectedMsg[] = "*** CONNECTED    ";
	char Msg[100];
	char Title[100];

	for (n = 0; n < NumberofStreams; n++)
	{
  		conn = &Connections[n];
		
		if (Stream == conn->BPQStream)
		{

			if (conn->Active)
			{
				// Probably an outgoing connect
		
				ChangeSessionIdletime(Stream, USERIDLETIME);		// Default Idletime for BBS Sessions
				conn->SendB = conn->SendP = conn->SendT = conn->DoReverse = TRUE;
				conn->MaxBLen = conn->MaxPLen = conn->MaxTLen = 99999999;

				if (conn->BBSFlags & RunningConnectScript)
				{
					// BBS Outgoing Connect

					conn->paclen = 236;

					// Run first line of connect script

					ChangeSessionIdletime(Stream, BBSIDLETIME);		// Default Idletime for BBS Sessions
					ProcessBBSConnectScript(conn, ConnectedMsg, 15);
					return 0;
				}
			}
	
			memset(conn, 0, sizeof(ConnectionInfo));		// Clear everything
			conn->Active = TRUE;
			conn->BPQStream = Stream;
			ChangeSessionIdletime(Stream, USERIDLETIME);			// Default Idletime for BBS Sessions

			conn->SendB = conn->SendP = conn->SendT = conn->DoReverse = TRUE;
			conn->MaxBLen = conn->MaxPLen = conn->MaxTLen = 99999999;

			conn->Secure_Session = GetConnectionInfo(Stream, callsign,
				&port, &conn->SessType, &paclen, &maxframe, &l4window);

			strlop(callsign, ' ');		// Remove trailing spaces

			memcpy(conn->Callsign, callsign, 10);

			strlop(callsign, '-');		// Remove any SSID

			user = LookupCall(callsign);

			if (user == NULL)
			{
				int Length=0;
				char * MailBuffer = malloc(100);

				user = AllocateUserRecord(callsign);
				user->Temp = zalloc(sizeof (struct TempUserInfo));

				if (SendNewUserMessage)
				{
					// Try to find port, freq, mode, etc

					int Freq = 0;
					int Mode = 0;

#ifdef LINBPQ
					BPQVECSTRUC * SESS = &BPQHOSTVECTOR[0];
#else
					BPQVECSTRUC * SESS = (BPQVECSTRUC *)BPQHOSTVECPTR;
#endif
					TRANSPORTENTRY * Sess1 = NULL, * Sess2;	
					
					SESS +=(Stream - 1);
				
					if (SESS)
						Sess1 = SESS->HOSTSESSION;

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

					Length += sprintf(MailBuffer, "New User %s Connected to Mailbox on Port %d Freq %d Mode %d\r\n", callsign, port, Freq, Mode);

					sprintf(Title, "New User %s", callsign);

					SendMessageToSYSOP(Title, MailBuffer, Length);
				}

				if (user == NULL) return 0; //		Cant happen??

				if (!DontHoldNewUsers)
					user->flags |= F_HOLDMAIL;

				conn->NewUser = TRUE;
			}

			time(&user->TimeLastConnected);
			user->Total.ConnectsIn++;

			conn->UserPointer = user;

			conn->lastmsg = user->lastmsg;

			conn->NextMessagetoForward = FirstMessageIndextoForward;

			if (paclen == 0)
			{
				paclen = 236;
	
				if (conn->SessType & Sess_PACTOR)
					paclen = 100;
			}

			conn->paclen = paclen;

			//	Set SYSOP flag if user is defined as SYSOP and Host Session 
			
			if (((conn->SessType & Sess_BPQHOST) == Sess_BPQHOST) && (user->flags & F_SYSOP))
				conn->sysop = TRUE;

			if (conn->Secure_Session && (user->flags & F_SYSOP))
				conn->sysop = TRUE;

			Mask = 1 << (GetApplNum(Stream) - 1);

			if (user->flags & F_Excluded)
			{
				n=sprintf_s(Msg, sizeof(Msg), "Incoming Connect from %s Rejected by Exclude Flag", user->Call);
				WriteLogLine(conn, '|',Msg, n, LOG_BBS);
				Disconnect(Stream);
				return 0;
			}

			n=sprintf_s(Msg, sizeof(Msg), "Incoming Connect from %s", user->Call);
			
			// Send SID and Prompt

			conn->SIDResponseTimer =  12;				// Allow a couple of minutes for response to SID

			{
				BOOL B1 = FALSE, B2 = FALSE, BIN = FALSE;
				struct	BBSForwardingInfo * ForwardingInfo;

				conn->PageLen = user->PageLen;				// No paging for chat
				conn->Paging = (user->PageLen > 0);

				if ((user->flags & F_Temp_B2_BBS) && (user->ForwardingInfo == NULL))
				{
					// An RMS Express user that needs a temporary BBS struct

					ForwardingInfo = user->ForwardingInfo = zalloc(sizeof(struct BBSForwardingInfo));

					ForwardingInfo->AllowCompressed = TRUE;
					B2 = ForwardingInfo->AllowB2 = TRUE;
					user->BBSNumber = NBBBS;
				}

				if (conn->NewUser)
				{
					BIN = TRUE;
					B2 = TRUE;
				}

				if (user->ForwardingInfo)
				{
					BIN = user->ForwardingInfo->AllowCompressed;
					B1 = user->ForwardingInfo->AllowB1;
					B2 = user->ForwardingInfo->AllowB2;
				}

				WriteLogLine(conn, '|',Msg, n, LOG_BBS);

				nodeprintf(conn, BBSSID, "BPQ-",
					Ver[0], Ver[1], Ver[2], Ver[3],
					BIN ? "B" : "", B1 ? "1" : "", B2 ? "2" : "", BIN ? "FW": "");

//				 if (user->flags & F_Temp_B2_BBS)
//					 nodeprintf(conn,";PQ: 66427529\r");

	//			nodeprintf(conn,"[WL2K-BPQ.1.0.4.39-B2FWIHJM$]\r");
			}

			if ((user->Name[0] == 0) & AllowAnon)
				strcpy(user->Name, user->Call);

			if (user->Name[0] == 0)
			{
				conn->Flags |= GETTINGUSER;
				BBSputs(conn, NewUserPrompt);
			}
			else
				SendWelcomeMsg(Stream, conn, user);

			RefreshMainWindow();
			
			return 0;
		}
	}

	return 0;
}

int Disconnected (Stream)
{
	struct UserInfo * user = NULL;
	CIRCUIT * conn;
	int n;
	char Msg[255];
	int len;

	for (n = 0; n <= NumberofStreams-1; n++)
	{
		conn=&Connections[n];

		if (Stream == conn->BPQStream)
		{
			if (conn->Active == FALSE)
			{
				return 0;
			}

			ClearQueue(conn);

			if (conn->PacLinkCalls)
				free(conn->PacLinkCalls);

			if (conn->InputBuffer)
			{
				free(conn->InputBuffer);
				conn->InputBuffer = NULL;
				conn->InputBufferLen = 0;
			}

			if (conn->InputMode == 'B')
			{
				// Save partly received message for a restart
						
				if (conn->BBSFlags & FBBB1Mode)
					if (conn->Paclink == 0)			// Paclink doesn't do restarts
						if (strcmp(conn->Callsign, "RMS") != 0)	// Neither does RMS Packet.
							if (conn->DontSaveRestartData == FALSE)
								SaveFBBBinary(conn);		
			}

			conn->Active = FALSE;

			if (conn->FwdMsg)
				conn->FwdMsg->Locked = 0;	// Unlock

			RefreshMainWindow();

			RemoveTempBIDS(conn);

			len=sprintf_s(Msg, sizeof(Msg), "%s Disconnected", conn->Callsign);
			WriteLogLine(conn, '|',Msg, len, LOG_BBS);

			if (conn->FBBHeaders)
			{
				struct FBBHeaderLine * FBBHeader;
				int n;

				for (n = 0; n < 5; n++)
				{
					FBBHeader = &conn->FBBHeaders[n];
					
					if (FBBHeader->FwdMsg)
						FBBHeader->FwdMsg->Locked = 0;	// Unlock

				}

				free(conn->FBBHeaders);
				conn->FBBHeaders = NULL;
			}

			if (conn->UserPointer)
			{
				struct	BBSForwardingInfo * FWDInfo = conn->UserPointer->ForwardingInfo;

				if (FWDInfo)
				{
					FWDInfo->Forwarding = FALSE;

//					if (FWDInfo->UserCall[0])			// Will be set if RMS
//					{
//						FindNextRMSUser(FWDInfo);
//					}
//					else
						FWDInfo->FwdTimer = 0;
				}
			}
			
			conn->BBSFlags = 0;				// Clear ARQ Mode

			return 0;
		}
	}
	return 0;
}

int DoReceivedData(int Stream)
{
	int count, InputLen;
	UINT MsgLen;
	int n;
	CIRCUIT * conn;
	struct UserInfo * user;
	char * ptr, * ptr2;
	char * Buffer;

	for (n = 0; n < NumberofStreams; n++)
	{
		conn = &Connections[n];

		if (Stream == conn->BPQStream)
		{
			conn->SIDResponseTimer = 0;		// Got a message, so cancel timeout.

			do
			{ 
				// May have several messages per packet, or message split over packets

			OuterLoop:
				if (conn->InputLen + 1000 > conn->InputBufferLen )	// Shouldnt have lines longer  than this in text mode
				{
					conn->InputBufferLen += 1000;
					conn->InputBuffer = realloc(conn->InputBuffer, conn->InputBufferLen);
				}
				
				GetMsg(Stream, &conn->InputBuffer[conn->InputLen], &InputLen, &count);

				if (InputLen == 0) return 0;

				conn->Watchdog = 900;				// 15 Minutes

				conn->InputLen += InputLen;

				if (conn->InputMode == 'B')
				{
					// if in OpenBCM mode, remove FF transparency

					if (conn->OpenBCM)			// Telnet, so escape any 0xFF
					{
						unsigned char * ptr1 = conn->InputBuffer;
						unsigned char * ptr2;
						int Len;
						unsigned char c;

						// We can come through here again for the
						// same data as we wait for a full packet
						// So only check last InputLen bytes

						ptr1 += (conn->InputLen - InputLen);
						ptr2 = ptr1;
						Len = InputLen;

						while (Len--)
						{
							c = *(ptr1++);

							if (conn->InTelnetExcape)	// Last char was ff
							{
								conn->InTelnetExcape = FALSE;
								continue;
							}

							*(ptr2++) = c;

							if (c == 0xff)		// 
								conn->InTelnetExcape = TRUE;
						}

						conn->InputLen = ptr2 - conn->InputBuffer;
					}

					UnpackFBBBinary(conn);
					goto OuterLoop;
				}
				else
				{

			loop:

				if (conn->InputLen == 1 && conn->InputBuffer[0] == 0)		// Single Null
				{
					conn->InputLen = 0;
					return 0;
				}

				ptr = memchr(conn->InputBuffer, '\r', conn->InputLen);
				ptr2 = memchr(conn->InputBuffer, '\n', conn->InputLen);
				
				if ((ptr2 && ptr2 < ptr) || ptr == 0)		// LF before CR, or no CR
					ptr = ptr2;								// Use LF

				if (ptr)				// CR ot LF in buffer
				{
					*(ptr) = '\r';		// In case was LF

					user = conn->UserPointer;
				
					ptr2 = &conn->InputBuffer[conn->InputLen];
					
					if (++ptr == ptr2)
					{
						// Usual Case - single meg in buffer

							if (conn->BBSFlags & RunningConnectScript)
								ProcessBBSConnectScript(conn, conn->InputBuffer, conn->InputLen);
							else
								ProcessLine(conn, user, conn->InputBuffer, conn->InputLen);
					
						conn->InputLen=0;
					}
					else
					{
						// buffer contains more that 1 message

						MsgLen = conn->InputLen - (ptr2-ptr);

						Buffer = malloc(MsgLen + 100);

						memcpy(Buffer, conn->InputBuffer, MsgLen);
					
							if (conn->BBSFlags & RunningConnectScript)
								ProcessBBSConnectScript(conn, Buffer, MsgLen);
							else
								ProcessLine(conn, user, Buffer, MsgLen);
						

						free(Buffer);

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
				}
			} while (count > 0);

			return 0;
		}
	}

	// Socket not found

	return 0;

}
int DoBBSMonitorData(int Stream)
{
//	UCHAR Buffer[1000];
	UCHAR buff[500];

	int len = 0,count=0;
	int stamp;
	
		do
		{ 
			stamp=GetRaw(Stream, buff,&len,&count);

			if (len == 0) return 0;

			SeeifBBSUIFrame((struct _MESSAGEX *)buff, len);	
		}
		
		while (count > 0);	
 		

	return 0;

}

VOID ProcessFLARQLine(ConnectionInfo * conn, struct UserInfo * user, char * Buffer, int MsgLen)
{
	Buffer[MsgLen] = 0;

	if (MsgLen == 1 && Buffer[0] == 13)
		return;

	if (strcmp(Buffer, "ARQ::ETX\r") == 0)
	{
		// Decode it. 

		UCHAR * ptr1, * ptr2, * ptr3;
		int len, linelen;
		struct MsgInfo * Msg = conn->TempMsg;
		time_t Date;
		char FullTo[100];
		char FullFrom[100];
		char ** RecpTo = NULL;				// May be several Recipients
		char ** HddrTo = NULL;				// May be several Recipients
		char ** Via = NULL;					// May be several Recipients
		int LocalMsg[1000]	;				// Set if Recipient is a local wl2k address

		int B2To;							// Offset to To: fields in B2 header
		int Recipients = 0;
		int RMSMsgs = 0, BBSMsgs = 0;

//		Msg->B2Flags |= B2Msg;
				

		ptr1 = conn->MailBuffer;
		len = Msg->length;
		ptr1[len] = 0;

		if (strstr(ptr1, "ARQ:ENCODING::"))
		{
			// a file, not a message. If is called  "BBSPOLL" do a reverse forward else Ignore for now

			_strupr(conn->MailBuffer);
			if (strstr(conn->MailBuffer, "BBSPOLL"))
			{
				SendARQMail(conn);
			}

			free(conn->MailBuffer);
			conn->MailBuffer = NULL;
			conn->MailBufferSize = 0;

			return;
		}
	Loop:
		ptr2 = strchr(ptr1, '\r');

		linelen = ptr2 - ptr1;

		if (_memicmp(ptr1, "From:", 5) == 0 && linelen > 6)			// Can have empty From:
		{
			char SaveFrom[100];
			char * FromHA;

			memcpy(FullFrom, ptr1, linelen);
			FullFrom[linelen] = 0;

			// B2 From may now contain an @BBS 

			strcpy(SaveFrom, FullFrom);
				
			FromHA = strlop(SaveFrom, '@');

			if (strlen(SaveFrom) > 12) SaveFrom[12] = 0;
			
			strcpy(Msg->from, &SaveFrom[6]);

			if (FromHA)
			{
				if (strlen(FromHA) > 39) FromHA[39] = 0;
				Msg->emailfrom[0] = '@';
				strcpy(&Msg->emailfrom[1], _strupr(FromHA));
			}

			// Remove any SSID

			ptr3 = strchr(Msg->from, '-');
				if (ptr3) *ptr3 = 0;
		
		}
		else if (_memicmp(ptr1, "To:", 3) == 0 || _memicmp(ptr1, "cc:", 3) == 0)
		{
			HddrTo=realloc(HddrTo, (Recipients+1)*4);
			HddrTo[Recipients] = zalloc(100);

			memset(FullTo, 0, 99);
			memcpy(FullTo, &ptr1[4], linelen-4);
			memcpy(HddrTo[Recipients], ptr1, linelen+2);
			LocalMsg[Recipients] = FALSE;

			_strupr(FullTo);

			B2To = ptr1 - conn->MailBuffer;

			if (_memicmp(FullTo, "RMS:", 4) == 0)
			{
				// remove RMS and add @winlink.org

				strcpy(FullTo, "RMS");
				strcpy(Msg->via, &FullTo[4]);
			}
			else
			{
				ptr3 = strchr(FullTo, '@');

				if (ptr3)
				{
					*ptr3++ = 0;
					strcpy(Msg->via, ptr3);
				}
				else
					Msg->via[0] = 0;
			}
		
			if (_memicmp(&ptr1[4], "SMTP:", 5) == 0)
			{
				// Airmail Sends MARS messages as SMTP
					
				if (CheckifPacket(Msg->via))
				{
					// Packet Message

					memmove(FullTo, &FullTo[5], strlen(FullTo) - 4);
					_strupr(FullTo);
					_strupr(Msg->via);
						
					// Update the saved to: line (remove the smtp:)

					strcpy(&HddrTo[Recipients][4], &HddrTo[Recipients][9]);
					BBSMsgs++;
					goto BBSMsg;
				}

				// If a winlink.org address we need to convert to call

				if (_stricmp(Msg->via, "winlink.org") == 0)
				{
					memmove(FullTo, &FullTo[5], strlen(FullTo) - 4);
					_strupr(FullTo);
					LocalMsg[Recipients] = CheckifLocalRMSUser(FullTo);
				}
				else
				{
					memcpy(Msg->via, &ptr1[9], linelen);
					Msg->via[linelen - 9] = 0;
					strcpy(FullTo,"RMS");
				}
//					FullTo[0] = 0;

		BBSMsg:		
				_strupr(FullTo);
				_strupr(Msg->via);
			}

			if (memcmp(FullTo, "RMS:", 4) == 0)
			{
				// remove RMS and add @winlink.org

				memmove(FullTo, &FullTo[4], strlen(FullTo) - 3);
				strcpy(Msg->via, "winlink.org");
				sprintf(HddrTo[Recipients], "To: %s\r\n", FullTo);
			}

			if (strcmp(Msg->via, "RMS") == 0)
			{
				// replace RMS with @winlink.org

				strcpy(Msg->via, "winlink.org");
				sprintf(HddrTo[Recipients], "To: %s@winlink.org\r\n", FullTo);
			}

			if (strlen(FullTo) > 6)
				FullTo[6] = 0;

			strlop(FullTo, '-');

			strcpy(Msg->to, FullTo);

			if (SendBBStoSYSOPCall)
				if (_stricmp(FullTo, BBSName) == 0)
					strcpy(Msg->to, SYSOPCall);

			if ((Msg->via[0] == 0 || strcmp(Msg->via, "BPQ") == 0 || strcmp(Msg->via, "BBS") == 0))
			{
				// No routing - check @BBS and WP

				struct UserInfo * ToUser = LookupCall(FullTo);

				Msg->via[0] = 0;				// In case BPQ and not found

				if (ToUser)
				{
					// Local User. If Home BBS is specified, use it

					if (ToUser->HomeBBS[0])
					{
						strcpy(Msg->via, ToUser->HomeBBS); 
					}
				}
				else
				{
					WPRecP WP = LookupWP(FullTo);

					if (WP)
					{
						strcpy(Msg->via, WP->first_homebbs);
			
					}
				}

				// Fix To: address in B2 Header

				if (Msg->via[0])
					sprintf(HddrTo[Recipients], "To: %s@%s\r\n", FullTo, Msg->via);
				else
					sprintf(HddrTo[Recipients], "To: %s\r\n", FullTo);

			}

			RecpTo=realloc(RecpTo, (Recipients+1)*4);
			RecpTo[Recipients] = zalloc(10);

			Via=realloc(Via, (Recipients+1)*4);
			Via[Recipients] = zalloc(50);

			strcpy(Via[Recipients], Msg->via);
			strcpy(RecpTo[Recipients++], FullTo);

			// Remove the To: Line from the buffer
			
		}
		else if (_memicmp(ptr1, "Type:", 4) == 0)
		{
			if (ptr1[6] == 'N')
				Msg->type = 'T';				// NTS
			else
				Msg->type = ptr1[6];
		}
		else if (_memicmp(ptr1, "Subject:", 8) == 0)
		{
			int Subjlen = ptr2 - &ptr1[9];
			if (Subjlen > 60) Subjlen = 60;
			memcpy(Msg->title, &ptr1[9], Subjlen);

			goto ProcessBody;
		}
//		else if (_memicmp(ptr1, "Body:", 4) == 0)
//		{
//			MsgLen = atoi(&ptr1[5]);
//			StartofMsg = ptr1;
//		}
		else if (_memicmp(ptr1, "File:", 5) == 0)
		{
			Msg->B2Flags |= Attachments;
		}
		else if (_memicmp(ptr1, "Date:", 5) == 0)
		{
			struct tm rtime;
			char seps[] = " ,\t\r";

			memset(&rtime, 0, sizeof(struct tm));

			// Date: 2009/07/25 10:08
	
			sscanf(&ptr1[5], "%04d/%02d/%02d %02d:%02d:%02d",
					&rtime.tm_year, &rtime.tm_mon, &rtime.tm_mday, &rtime.tm_hour, &rtime.tm_min, &rtime.tm_sec);

			sscanf(&ptr1[5], "%02d/%02d/%04d %02d:%02d:%02d",
					&rtime.tm_mday, &rtime.tm_mon, &rtime.tm_year, &rtime.tm_hour, &rtime.tm_min, &rtime.tm_sec);

			rtime.tm_year -= 1900;

			Date = mktime(&rtime) - (time_t)_MYTIMEZONE;
	
			if (Date == (time_t)-1)
				Date = time(NULL);

			Msg->datecreated = Date;

		}

		if (linelen)			// Not Null line
		{
			ptr1 = ptr2 + 2;		// Skip cr
			goto Loop;
		}
	
		
		// Processed all headers
ProcessBody:

		ptr2 +=2;					// skip crlf

		Msg->length = &conn->MailBuffer[Msg->length] - ptr2;

		memmove(conn->MailBuffer, ptr2, Msg->length);

		CreateMessageFromBuffer(conn);

		conn->BBSFlags = 0;				// Clear ARQ Mode
		return;
	}

	// File away the data

	Buffer[MsgLen++] = 0x0a;			// BBS Msgs stored with crlf

	if ((conn->TempMsg->length + MsgLen) > conn->MailBufferSize)
	{
		conn->MailBufferSize += 10000;
		conn->MailBuffer = realloc(conn->MailBuffer, conn->MailBufferSize);
	
		if (conn->MailBuffer == NULL)
		{
			BBSputs(conn, "*** Failed to extend Message Buffer\r");
			conn->CloseAfterFlush = 20;			// 2 Secs

			return;
		}
	}

	memcpy(&conn->MailBuffer[conn->TempMsg->length], Buffer, MsgLen);

	conn->TempMsg->length += MsgLen;

	return;

	// Not sure what to do yet with files, but will process emails (using text style forwarding

/*
ARQ:FILE::flarqmail-1.eml
ARQ:EMAIL::
ARQ:SIZE::96
ARQ::STX
//FLARQ COMPOSER
Date: 16/01/2014 22:26:06
To: g8bpq
From: 
Subject: test message

Hello
Hello

ARQ::ETX
*/

	return;
}

VOID ProcessTextFwdLine(ConnectionInfo * conn, struct UserInfo * user, char * Buffer, int len)
{
	Buffer[len] = 0;
//	Debugprintf(Buffer);

	if (len == 1 && Buffer[0] == 13)
		return;

	if (conn->Flags & SENDTITLE)
	{	
		// Waiting for Subject: prompt

		struct MsgInfo * Msg = conn->FwdMsg;
		
		nodeprintf(conn, "%s\r", Msg->title);
		
		conn->Flags &= ~SENDTITLE;
		conn->Flags |= SENDBODY;
		return;

	}
	
	if (conn->Flags & SENDBODY)
	{
		// Waiting for Enter Message Prompt

		struct tm * tm;
		char * MsgBytes = ReadMessageFile(conn->FwdMsg->number);
		char * MsgPtr;
		int MsgLen;
		int Index = 0;

		if (MsgBytes == 0)
		{
			MsgBytes = _strdup("Message file not found\r");
			conn->FwdMsg->length = strlen(MsgBytes);
		}

		MsgPtr = MsgBytes;
		MsgLen = conn->FwdMsg->length;

		// If a B2 Message, remove B2 Header

		if (conn->FwdMsg->B2Flags)
		{		
			// Remove all B2 Headers, and all but the first part.
					
			MsgPtr = strstr(MsgBytes, "Body:");
			
			if (MsgPtr)
			{
				MsgLen = atoi(&MsgPtr[5]);
				MsgPtr= strstr(MsgBytes, "\r\n\r\n");		// Blank Line after headers
	
				if (MsgPtr)
					MsgPtr +=4;
				else
					MsgPtr = MsgBytes;
			
			}
			else
				MsgPtr = MsgBytes;
		}

		tm = gmtime(&conn->FwdMsg->datereceived);	

		nodeprintf(conn, "R:%02d%02d%02d/%02d%02dZ %d@%s.%s %s\r",
				tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min,
				conn->FwdMsg->number, BBSName, HRoute, RlineVer);

		if (memcmp(MsgPtr, "R:", 2) != 0)    // No R line, so must be our message - put blank line after header
			BBSputs(conn, "\r");

		MsgLen = RemoveLF(MsgPtr, MsgLen);

		QueueMsg(conn, MsgPtr, MsgLen);

		if (user->ForwardingInfo->SendCTRLZ)
			nodeprintf(conn, "\r\x1a");
		else
			nodeprintf(conn, "\r/ex\r");

		free(MsgBytes);
			
		conn->FBBMsgsSent = TRUE;

		
		if (conn->FwdMsg->type == 'P')
			Index = PMSG;
		else if (conn->FwdMsg->type == 'B')
			Index = BMSG;
		else if (conn->FwdMsg->type == 'T')
			Index = TMSG;

		user->Total.MsgsSent[Index]++;
		user->Total.BytesForwardedOut[Index] += MsgLen;
			
		conn->Flags &= ~SENDBODY;
		conn->Flags |= WAITPROMPT;

		return;
	}

	if (conn->Flags & WAITPROMPT)
	{
		if (Buffer[len-2] != '>')
			return;

		conn->Flags &= ~WAITPROMPT;

		clear_fwd_bit(conn->FwdMsg->fbbs, user->BBSNumber);
		set_fwd_bit(conn->FwdMsg->forw, user->BBSNumber);

		//  Only mark as forwarded if sent to all BBSs that should have it
			
		if (memcmp(conn->FwdMsg->fbbs, zeros, NBMASK) == 0)
		{
			conn->FwdMsg->status = 'F';			// Mark as forwarded
			conn->FwdMsg->datechanged=time(NULL);
		}

		conn->UserPointer->ForwardingInfo->MsgCount--;

		// See if any more to forward

		if (FindMessagestoForward(conn))
		{
			struct MsgInfo * Msg;
				
			// Send S line and wait for response - SB WANT @ USA < W8AAA $1029_N0XYZ 

			conn->Flags |= SENDTITLE;
			Msg = conn->FwdMsg;
		
			nodeprintf(conn, "S%c %s @ %s < %s $%s\r", Msg->type, Msg->to,
						(Msg->via[0]) ? Msg->via : conn->UserPointer->Call, 
						Msg->from, Msg->bid);
		}
		else
		{
			Disconnect(conn->BPQStream);
		}
		return;
	}
}



VOID ProcessLine(CIRCUIT * conn, struct UserInfo * user, char* Buffer, int len)
{
	char * Cmd, * Arg1;
	char * Context;
	char seps[] = " \t\r";
	int CmdLen;

	WriteLogLine(conn, '<',Buffer, len-1, LOG_BBS);

	if (conn->BBSFlags & FBBForwarding)
	{
		ProcessFBBLine(conn, user, Buffer, len);
		return;
	}

	if (conn->BBSFlags & FLARQMODE)
	{
		ProcessFLARQLine(conn, user, Buffer, len);
		return;
	}

	if (conn->BBSFlags & TEXTFORWARDING)
	{
		ProcessTextFwdLine(conn, user, Buffer, len);
		return;
	}

	if (conn->Flags & GETTINGMESSAGE)
	{
		ProcessMsgLine(conn, user, Buffer, len);
		return;
	}
	if (conn->Flags & GETTINGTITLE)
	{
		ProcessMsgTitle(conn, user, Buffer, len);
		return;
	}

	if (conn->BBSFlags & MBLFORWARDING)
	{
			ProcessMBLLine(conn, user, Buffer, len);
		return;
	}
	if (conn->Flags & GETTINGUSER || conn->NewUser)		// Could be new user but dont nned name
	{
		if (memcmp(Buffer, ";FW:", 4) == 0 || Buffer[0] == '[')
		{
			struct	BBSForwardingInfo * ForwardingInfo;
			
			conn->Flags &= ~GETTINGUSER;

			// New User is a BBS - create a temp struct for it

			if ((user->flags & (F_BBS | F_Temp_B2_BBS)) == 0)			// It could already be a BBS without a user name
			{
				// Not defined as BBS - allocate and initialise forwarding structure
		
				user->flags |= F_Temp_B2_BBS;

				// An RMS Express user that needs a temporary BBS struct

				ForwardingInfo = user->ForwardingInfo = zalloc(sizeof(struct BBSForwardingInfo));

				ForwardingInfo->AllowCompressed = TRUE;
				conn->UserPointer->ForwardingInfo->AllowB2 = TRUE;
			}
			SaveUserDatabase();
		}
		else
		{
			if (conn->Flags & GETTINGUSER)
			{
				conn->Flags &= ~GETTINGUSER;
				memcpy(user->Name, Buffer, len-1);
				SendWelcomeMsg(conn->BPQStream, conn, user);
				SaveUserDatabase();
				UpdateWPWithUserInfo(user);
				return;
			}
		}
	}

	// Process Command

	if (conn->Paging && (conn->LinesSent >= conn->PageLen))
	{
		// Waiting for paging prompt

		if (len > 1)
		{
			if (_memicmp(Buffer, "Abort", 1) == 0)
			{
				ClearQueue(conn);
				conn->LinesSent = 0;

				nodeprintf(conn, AbortedMsg);

				if (conn->UserPointer->Temp->ListSuspended)
					nodeprintf(conn, "<A>bort, <R Msg(s)>, <CR> = Continue..>");

				SendPrompt(conn, user);
				return;
			}
		}

		conn->LinesSent = 0;
		return;
	}

	if (user->Temp->ListSuspended)
	{
		// Paging limit hit when listing. User may about, continue, or read one or more messages

		ProcessSuspendedListCommand(conn, user, Buffer, len);
		return;
	}
	if (len == 1)
	{
		SendPrompt(conn, user);
		return;
	}

	Buffer[len] = 0;

	if (strstr(Buffer, "ARQ:FILE:"))
	{
		// Message from FLARQ

		conn->BBSFlags |= FLARQMODE;
		strcpy(conn->ARQFilename, &Buffer[10]);			// Will need name when we decide what to do with files

		// Create a Temp Messge Stucture

		CreateMessage(conn, conn->Callsign, "", "", 'P', NULL, NULL);

		Buffer[len++] = 0x0a;			// BBS Msgs stored with crlf

		if ((conn->TempMsg->length + len) > conn->MailBufferSize)
		{
			conn->MailBufferSize += 10000;
			conn->MailBuffer = realloc(conn->MailBuffer, conn->MailBufferSize);
	
			if (conn->MailBuffer == NULL)
			{
				BBSputs(conn, "*** Failed to extend Message Buffer\r");
				conn->CloseAfterFlush = 20;			// 2 Secs

				return;
			}
		}

		memcpy(&conn->MailBuffer[conn->TempMsg->length], Buffer, len);

		conn->TempMsg->length += len;

		return;
	}

	if (memcmp(Buffer, ";FW:", 4) == 0)
	{
		// Paclink User Select (poll for list)
		
		char * ptr1,* ptr2, * ptr3;
		int index=0;

		// Convert string to Multistring

		Buffer[len-1] = 0;

		conn->PacLinkCalls = zalloc(len*3);

		ptr1 = &Buffer[5];
		ptr2 = (char *)conn->PacLinkCalls;
		ptr2 += (len * 2);
		strcpy(ptr2, ptr1);

		while (ptr2)
		{
			ptr3 = strlop(ptr2, ' ');

			if (strlen(ptr2))
				conn->PacLinkCalls[index++] = ptr2;
		
			ptr2 = ptr3;
		}
	
		return;	
	}

	if (memcmp(Buffer, ";FR:", 4) == 0)
	{
		// New Message from TriMode - Just igonre till I know what to do with it

		return;
	}

	if (Buffer[0] == '[' && Buffer[len-2] == ']')		// SID
	{
		// If a BBS, set BBS Flag

		if (user->flags & ( F_BBS | F_Temp_B2_BBS))
		{
			if (user->ForwardingInfo)
			{
				if (user->ForwardingInfo->Forwarding && ((conn->BBSFlags & OUTWARDCONNECT) == 0))
				{
					BBSputs(conn, "Already Connected\r");
					Flush(conn);
					Sleep(500);
					Disconnect(conn->BPQStream);
					return;
				}
			}

			if (user->ForwardingInfo)
			{
				user->ForwardingInfo->Forwarding = TRUE;
				user->ForwardingInfo->FwdTimer = 0;			// So we dont send to immediately
			}
		}

		if (user->flags & ( F_BBS | F_PMS | F_Temp_B2_BBS))
		{
			Parse_SID(conn, &Buffer[1], len-4);
			
			if (conn->BBSFlags & FBBForwarding)
			{
				conn->FBBIndex = 0;		// ready for first block;
				conn->FBBChecksum = 0;
				memset(&conn->FBBHeaders[0], 0, 5 * sizeof(struct FBBHeaderLine));
			}
			else
				FBBputs(conn, ">\r");

		}

		return;
	}

	Cmd = strtok_s(Buffer, seps, &Context);

	if (Cmd == NULL)
	{
		BBSputs(conn, "Invalid Command\r");
		SendPrompt(conn, user);
		return;
	}

	Arg1 = strtok_s(NULL, seps, &Context);
	CmdLen = strlen(Cmd);

	// Check List first. If any other, save last listed to user record.

	if (_memicmp(Cmd, "L", 1) == 0 && _memicmp(Cmd, "LISTFILES", 3) != 0)
	{
		DoListCommand(conn, user, Cmd, Arg1, FALSE);
		SendPrompt(conn, user);
		return;
	}

	if (conn->lastmsg > user->lastmsg)
	{
		user->lastmsg = conn->lastmsg;
		SaveUserDatabase();
	}

	if (_stricmp(Cmd, "SHOWRMSPOLL") == 0)
	{
		DoShowRMSCmd(conn, user, Arg1, Context);
		return;
	}

	if (_stricmp(Cmd, "AUTH") == 0)
	{
		DoAuthCmd(conn, user, Arg1, Context);
		return;
	}

	if (_memicmp(Cmd, "Abort", 1) == 0)
	{
		ClearQueue(conn);
		conn->LinesSent = 0;

		nodeprintf(conn, AbortedMsg);

		if (conn->UserPointer->Temp->ListSuspended)
			nodeprintf(conn, "<A>bort, <R Msg(s)>, <CR> = Continue..>");

		SendPrompt(conn, user);
		return;
	}
	if (_memicmp(Cmd, "Bye", CmdLen) == 0)
	{
		ExpandAndSendMessage(conn, SignoffMsg, LOG_BBS);
		Flush(conn);
		Sleep(1000);

		if (conn->BPQStream > 0)
			Disconnect(conn->BPQStream);
#ifndef LINBPQ
		else
			CloseConsole(conn->BPQStream);
#endif
		return;
	}

	if (_memicmp(Cmd, "Node", 4) == 0)
	{
		ExpandAndSendMessage(conn, SignoffMsg, LOG_BBS);
		Flush(conn);
		Sleep(1000);
			
		if (conn->BPQStream > 0)
			ReturntoNode(conn->BPQStream);
#ifndef LINBPQ
		else
			CloseConsole(conn->BPQStream);
#endif

		return;						
	}

	if (_memicmp(Cmd, "IDLETIME", 4) == 0)
	{
		DoSetIdleTime(conn, user, Arg1, Context);
		return;
	}

	if (_memicmp(Cmd, "D", 1) == 0)
	{
		DoDeliveredCommand(conn, user, Cmd, Arg1, Context);
		SendPrompt(conn, user);
		return;
	}

	if (_memicmp(Cmd, "K", 1) == 0)
	{
		DoKillCommand(conn, user, Cmd, Arg1, Context);
		SendPrompt(conn, user);
		return;
	}


	if (_memicmp(Cmd, "LISTFILES", 3) == 0 || _memicmp(Cmd, "FILES", 5) == 0)
	{
		ListFiles(conn, user, Arg1);
		SendPrompt(conn, user);
		return;
	}

	if (_memicmp(Cmd, "READFILE", 4) == 0)
	{
		ReadBBSFile(conn, user, Arg1);
		SendPrompt(conn, user);
		return;
	}

	if (_memicmp(Cmd, "UH", 2) == 0 && conn->sysop)
	{
		DoUnholdCommand(conn, user, Cmd, Arg1, Context);
		SendPrompt(conn, user);
		return;
	}

	if (_stricmp(Cmd, "IMPORT") == 0)
	{
		DoImportCmd(conn, user, Arg1, Context);
		return;
	}

	if (_stricmp(Cmd, "EXPORT") == 0)
	{
		DoExportCmd(conn, user, Arg1, Context);
		return;
	}

	if (_memicmp(Cmd, "I", 1) == 0)
	{
		char * Save;
		char * MsgBytes;

		if (Arg1)
		{
			// User WP lookup

			DoWPLookup(conn, user, Cmd[1], Arg1);
			SendPrompt(conn, user);
			return;	
		}


		MsgBytes = Save = ReadInfoFile("info.txt");
		if (MsgBytes)
		{
			int Length;

			// Remove lf chars

			Length = RemoveLF(MsgBytes, strlen(MsgBytes));

			QueueMsg(conn, MsgBytes, Length);
			free(Save);
		}
		else
			BBSputs(conn, "SYSOP has not created an INFO file\r");


		SendPrompt(conn, user);
		return;	
	}


	if (_memicmp(Cmd, "Name", CmdLen) == 0)
	{
		if (Arg1)
		{
			if (strlen(Arg1) > 17)
				Arg1[17] = 0;

			strcpy(user->Name, Arg1);
			UpdateWPWithUserInfo(user);

		}

		SendWelcomeMsg(conn->BPQStream, conn, user);
		SaveUserDatabase();

		return;
	}

	if (_memicmp(Cmd, "OP", 2) == 0)
	{
		int Lines;
		
		// Paging Control. Param is number of lines per page

		if (Arg1)
		{
			Lines = atoi(Arg1);
			
			if (Lines)				// Sanity Check
			{
				if (Lines < 10)
				{
					nodeprintf(conn,"Page Length %d is too short\r", Lines);
					SendPrompt(conn, user);
					return;
				}
			}

			user->PageLen = Lines;
			conn->PageLen = Lines;
			conn->Paging = (Lines > 0);
			SaveUserDatabase();
		}
		
		nodeprintf(conn,"Page Length is %d\r", user->PageLen);
		SendPrompt(conn, user);

		return;
	}

	if (_memicmp(Cmd, "QTH", CmdLen) == 0)
	{
		if (Arg1)
		{
			// QTH may contain spaces, so put back together, and just split at cr
			
			Arg1[strlen(Arg1)] = ' ';
			strtok_s(Arg1, "\r", &Context);

			if (strlen(Arg1) > 60)
				Arg1[60] = 0;

			strcpy(user->Address, Arg1);
			UpdateWPWithUserInfo(user);

		}

		nodeprintf(conn,"QTH is %s\r", user->Address);
		SendPrompt(conn, user);

		SaveUserDatabase();

		return;
	}

	if (_memicmp(Cmd, "ZIP", CmdLen) == 0)
	{
		if (Arg1)
		{
			if (strlen(Arg1) > 8)
				Arg1[8] = 0;

			strcpy(user->ZIP, _strupr(Arg1));
			UpdateWPWithUserInfo(user);
		}

		nodeprintf(conn,"ZIP is %s\r", user->ZIP);
		SendPrompt(conn, user);

		SaveUserDatabase();

		return;
	}

	if (_memicmp(Cmd, "CMSPASS", 7) == 0)
	{
		if (Arg1 == 0)
		{
			nodeprintf(conn,"Must specify a password\r");
		}
		else
		{
			if (strlen(Arg1) > 15)
				Arg1[15] = 0;

			strcpy(user->CMSPass, _strupr(Arg1));
			UpdateWPWithUserInfo(user);
			nodeprintf(conn,"CMS Password Set\r");
			SaveUserDatabase();
		}

		SendPrompt(conn, user);

		return;
	}

	if (_memicmp(Cmd, "PASS", CmdLen) == 0)
	{
		if (Arg1 == 0)
		{
			nodeprintf(conn,"Must specify a password\r");
		}
		else
		{
			if (strlen(Arg1) > 12)
				Arg1[12] = 0;

			strcpy(user->pass, Arg1);
			UpdateWPWithUserInfo(user);
			nodeprintf(conn,"BBS Password Set\r");
			SaveUserDatabase();
		}

		SendPrompt(conn, user);

		return;
	}


	if (_memicmp(Cmd, "R", 1) == 0)
	{
		DoReadCommand(conn, user, Cmd, Arg1, Context);
		SendPrompt(conn, user);
		return;
	}

	if (_memicmp(Cmd, "S", 1) == 0)
	{
		if (!DoSendCommand(conn, user, Cmd, Arg1, Context))
			SendPrompt(conn, user);
		return;
	}

	if ((_memicmp(Cmd, "Help", CmdLen) == 0) || (_memicmp(Cmd, "?", 1) == 0))
	{
		char * Save;
		char * MsgBytes = Save = ReadInfoFile("help.txt");

		if (MsgBytes)
		{
			int Length;

			// Remove lf chars

			Length = RemoveLF(MsgBytes, strlen(MsgBytes));

			QueueMsg(conn, MsgBytes, Length);
			free(Save);
		}
		else
		{
			BBSputs(conn, "A - Abort Output\r");
			BBSputs(conn, "B - Logoff\r");
			BBSputs(conn, "CMSPASS Password - Set CMS Password\r");
			BBSputs(conn, "D - Flag NTS Message(s) as Delivered - D num\r");
			BBSputs(conn, "HOMEBBS - Display or get HomeBBS\r");
			BBSputs(conn, "INFO - Display information about this BBS\r");
			BBSputs(conn, "I CALL - Lookup CALL in WP Allows *CALL CALL* *CALL* wildcards\r");
			BBSputs(conn, "I@ PARAM - Lookup @BBS in WP\r");
			BBSputs(conn, "IZ PARAM - Lookup Zip Codes in WP\r");
			BBSputs(conn, "IH PARAM - Lookup HA elements in WP - eg USA EU etc\r");

			BBSputs(conn, "K - Kill Message(s) - K num, KM (Kill my read messages)\r");
			BBSputs(conn, "L - List Message(s) - L = List New, LR = List New (Oldest first)\r");
			BBSputs(conn, "                      LM = List Mine L> Call, L< Call = List to or from\r");
			BBSputs(conn, "                      LL num = List Last, L num-num = List Range\r");
			BBSputs(conn, "                      LN LY LH LK LF L$ LD - List Message with corresponding Status\r");
			BBSputs(conn, "                      LB LP LT - List Mesaage with corresponding Type\r");
			BBSputs(conn, "                      LC List TO fields of all active bulletins\r");
			BBSputs(conn, "LISTFILES or FILES - List files available for download\r");

			BBSputs(conn, "N Name - Set Name\r");
			BBSputs(conn, "NODE - Return to Node\r");
			BBSputs(conn, "OP n - Set Page Length (Output will pause every n lines)\r");
			BBSputs(conn, "PASS Password - Set BBS Password\r");
			BBSputs(conn, "POLLRMS - Manage Polling for messages from RMS \r");
			BBSputs(conn, "Q QTH - Set QTH\r");
			BBSputs(conn, "R - Read Message(s) - R num, RM (Read new messages to me)\r");
			BBSputs(conn, "READ Name - Read File\r");

			BBSputs(conn, "S - Send Message - S or SP Send Personal, SB Send Bull, ST Send NTS,\r");
			BBSputs(conn, "                   SR Num - Send Reply, SC Num - Send Copy\r");
			BBSputs(conn, "X - Toggle Expert Mode\r");
			if (conn->sysop)
			{
				BBSputs(conn, "EU - Edit User Flags - Type EU for Help\r");
				BBSputs(conn, "EXPORT - Export messages to file - Type EXPORT for Help\r");
				BBSputs(conn, "FWD - Control Forwarding - Type FWD for Help\r");
				BBSputs(conn, "IMPORT - Import messages from file - Type IMPORT for Help\r");
				BBSputs(conn, "SHOWRMSPOLL - Displays your RMS polling list\r");
				BBSputs(conn, "UH - Unhold Message(s) - UH ALL or UH num num num...\r");
			}
		}

		SendPrompt(conn, user);

		return;

	}

	if (_memicmp(Cmd, "Ver", CmdLen) == 0)
	{
		nodeprintf(conn, "BBS Version %s\rNode Version %s\r", VersionStringWithBuild, GetVersionString());

		SendPrompt(conn, user);

		return;
	}

	if (_memicmp(Cmd, "HOMEBBS", CmdLen) == 0)
	{
		if (Arg1)
		{
			if (strlen(Arg1) > 40) Arg1[40] = 0;

			strcpy(user->HomeBBS, _strupr(Arg1));
			UpdateWPWithUserInfo(user);
	
			if (!strchr(Arg1, '.'))
				BBSputs(conn, "Please enter HA with HomeBBS eg g8bpq.gbr.eu - this will help message routing\r");
		}

		nodeprintf(conn,"HomeBBS is %s\r", user->HomeBBS);
		SendPrompt(conn, user);

		SaveUserDatabase();

		return;
	}

	if ((_memicmp(Cmd, "EDITUSER", 5) == 0) || (_memicmp(Cmd, "EU", 2) == 0))
	{
		DoEditUserCmd(conn, user, Arg1, Context);
		return;
	}

	if (_stricmp(Cmd, "POLLRMS") == 0)
	{
		DoPollRMSCmd(conn, user, Arg1, Context);
		return;
	}

	if (_stricmp(Cmd, "FWD") == 0)
	{
		DoFwdCmd(conn, user, Arg1, Context);
		return;
	}

	if (_memicmp(Cmd, "X", 1) == 0)
	{
		user->flags ^= F_Expert;

		if (user->flags & F_Expert)
			BBSputs(conn, "Expert Mode\r");
		else
			BBSputs(conn, "Expert Mode off\r");

		SaveUserDatabase();
		SendPrompt(conn, user);
		return;
	}

	if (conn->Flags == 0)
	{
		BBSputs(conn, "Invalid Command\r");
		SendPrompt(conn, user);
	}

	//	Send if possible

	Flush(conn);
}

VOID __cdecl nprintf(CIRCUIT * conn, const char * format, ...)
{
	// seems to be printf to a socket

	char buff[600];
	va_list(arglist);
	
	va_start(arglist, format);
	vsprintf(buff, format, arglist);

	BBSputs(conn, buff);
}

// Code to delete obsolete files from Mail folder

#ifdef WIN32

int DeleteRedundantMessages()
{
   WIN32_FIND_DATA ffd;

   char szDir[MAX_PATH];
   char File[MAX_PATH];
   HANDLE hFind = INVALID_HANDLE_VALUE;
   int Msgno;

   // Prepare string for use with FindFile functions.  First, copy the
   // string to a buffer, then append '\*' to the directory name.

   strcpy(szDir, MailDir);
   strcat(szDir, "\\*.mes");



   // Find the first file in the directory.

   hFind = FindFirstFile(szDir, &ffd);

   if (INVALID_HANDLE_VALUE == hFind) 
   {
      return 0;
   } 
   
   do
   {
      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
         OutputDebugString(ffd.cFileName);
      }
      else
      {
		 Msgno = atoi(&ffd.cFileName[2]);

		 if (MsgnotoMsg[Msgno] == 0)
		 {
			 sprintf(File, "%s/%s%c", MailDir, ffd.cFileName, 0);
			 printf("Delete %s\n", File);

//			 if (DeletetoRecycleBin)
				DeletetoRecycle(File);
//			 else
//				 DeleteFile(File);
		 }
      }
   }
   while (FindNextFile(hFind, &ffd) != 0);
 
   FindClose(hFind);
   return 0;
}

#else

#include <dirent.h>

int MsgFilter(const struct dirent * dir)
{
	return (int)strstr(dir->d_name, ".mes");
}

int DeleteRedundantMessages()
{
	struct dirent **namelist;
    int n;
	struct stat STAT;
	int Msgno = 0, res;
	char File[100];
     	
    n = scandir("Mail", &namelist, MsgFilter, alphasort);

	if (n < 0) 
		perror("scandir");
	else  
	{ 
		while(n--)
		{
			if (stat(namelist[n]->d_name, &STAT) == 0);
			{
				Msgno = atoi(&namelist[n]->d_name[2]);

				if (MsgnotoMsg[Msgno] == 0)
				{
					sprintf(File, "Mail/%s", namelist[n]->d_name);
					printf("Deleting %s\n", File);
					unlink(File);
				}
			}
			free(namelist[n]);
		}
		free(namelist);
    }
	return 0;
}
#endif

VOID TidyPrompt(char ** pPrompt)
{
	// Make sure prompt ends > CR LF

	char * Prompt = *pPrompt;

	int i = strlen(Prompt) - 1;

	*pPrompt = realloc(Prompt, i + 5);	// In case we need to expand it

	Prompt = *pPrompt;

	while (Prompt[i] == 10 || Prompt[i] == 13)
	{
		Prompt[i--] = 0;
	}

	if (Prompt[i] != '>')
		strcat(Prompt, ">");

	strcat(Prompt, "\r\n");
}

VOID TidyPrompts()
{
	TidyPrompt(&Prompt);
	TidyPrompt(&NewPrompt);
	TidyPrompt(&ExpertPrompt);
}

BOOL SendARQMail(CIRCUIT * conn)
{
	conn->NextMessagetoForward = FirstMessageIndextoForward;

	// Send Message. There is no mechanism for reverse forwarding

	if (FindMessagestoForward(conn))
	{
		struct MsgInfo * Msg;
		char MsgHddr[512];
		int HddrLen;
		char TimeString[64];
		char * WholeMessage;

		char * MsgBytes = ReadMessageFile(conn->FwdMsg->number);
		int MsgLen;
		
		if (MsgBytes == 0)
		{
			MsgBytes = _strdup("Message file not found\r");
			conn->FwdMsg->length = strlen(MsgBytes);
		}

		Msg = conn->FwdMsg;
		WholeMessage = malloc(Msg->length + 512);

		FormatTime(TimeString, Msg->datecreated);

/*
ARQ:FILE::flarqmail-1.eml
ARQ:EMAIL::
ARQ:SIZE::96
ARQ::STX
//FLARQ COMPOSER
Date: 16/01/2014 22:26:06
To: g8bpq
From: 
Subject: test message

Hello
Hello

ARQ::ETX		
*/
		Logprintf(LOG_BBS, conn, '>', "ARQ Send Msg %d From %s To %s", Msg->number, Msg->from, Msg->to);

		HddrLen = sprintf(MsgHddr, "Date: %s\nTo: %s\nFrom: %s\nSubject %s\n\n",
			TimeString, Msg->to, Msg->from, Msg->title);
				
		MsgLen = sprintf(WholeMessage, "ARQ:FILE::Msg%s_%d\nARQ:EMAIL::\nARQ:SIZE::%d\nARQ::STX\n%s%s\nARQ::ETX\n",
			BBSName, Msg->number, HddrLen + strlen(MsgBytes), MsgHddr, MsgBytes);

		WholeMessage[MsgLen] = 0;
		QueueMsg(conn,WholeMessage, MsgLen);

		free(WholeMessage);
		free(MsgBytes);

		// FLARQ doesn't ACK the message, so set flag to look for all acked
				
		conn->BBSFlags |= ARQMAILACK;
		conn->ARQClearCount = 10;		// To make sure clear isn't reported too soon

		return TRUE;
	}

	// Nothing to send - close

	Logprintf(LOG_BBS, conn, '>', "ARQ Send -  Nothing to Send - Closing");

	conn->CloseAfterFlush = 20;
	return FALSE;
}

char *stristr (char *ch1, char *ch2)
{
	char	*chN1, *chN2;
	char	*chNdx;
	char	*chRet				= NULL;

	chN1 = _strdup (ch1);
	chN2 = _strdup (ch2);
	if (chN1 && chN2)
	{
		chNdx = chN1;
		while (*chNdx)
		{
			*chNdx = (char) tolower (*chNdx);
			chNdx ++;
		}
		chNdx = chN2;
		while (*chNdx)
		{
			*chNdx = (char) tolower (*chNdx);
			chNdx ++;
		}
		chNdx = strstr (chN1, chN2);
		if (chNdx)
			chRet = ch1 + (chNdx - chN1);
	}
	free (chN1);
	free (chN2);
	return chRet;
}

#ifdef WIN32

void ListFiles(ConnectionInfo * conn, struct UserInfo * user, char * filename)
{

   WIN32_FIND_DATA ffd;

   char szDir[MAX_PATH];
   HANDLE hFind = INVALID_HANDLE_VALUE;
 
   // Prepare string for use with FindFile functions.  First, copy the
   // string to a buffer, then append '\*' to the directory name.

   strcpy(szDir, GetBPQDirectory());
   strcat(szDir, "\\BPQMailChat\\Files\\*.*");

   // Find the first file in the directory.

   hFind = FindFirstFile(szDir, &ffd);

   if (INVALID_HANDLE_VALUE == hFind) 
   {
      nodeprintf(conn, "No Files\r");
	  return;
   } 
   
   // List all the files in the directory with some info about them.

	do
	{
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{}
		else
		{
			if (filename == NULL || stristr(ffd.cFileName, filename))
				nodeprintf(conn, "%s %d\r", ffd.cFileName, ffd.nFileSizeLow);
		}
	}
	while (FindNextFile(hFind, &ffd) != 0);
	
	FindClose(hFind);
}

#else

#include <dirent.h>

void ListFiles(ConnectionInfo * conn, struct UserInfo * user, char * filename)
{
	struct dirent **namelist;
    int n, i;
	struct stat STAT;
	time_t now = time(NULL);
	int Age = 0, res;
	char FN[256];
     	
    n = scandir("Files", &namelist, NULL, alphasort);

	if (n < 0) 
		perror("scandir");
	else  
	{ 
		for (i = 0; i < n; i++)
		{
			sprintf(FN, "Files/%s", namelist[i]->d_name);

			if (filename == NULL || stristr(namelist[i]->d_name, filename))
				if (FN[6] != '.' && stat(FN, &STAT) == 0)
					nodeprintf(conn, "%s %d\r", namelist[i]->d_name, STAT.st_size);
			
			free(namelist[i]);
		}
		free(namelist);
    }
	return;
}
#endif

void ReadBBSFile(ConnectionInfo * conn, struct UserInfo * user, char * filename)
{
	char * MsgBytes;
	
	int FileSize;
	char MsgFile[MAX_PATH];
	FILE * hFile;
	struct stat STAT;

	if (strstr(filename, "..") || strchr(filename, '/') || strchr(filename, '\\'))
	{
		nodeprintf(conn, "Invalid filename\r");
		return;
	}

	if (BaseDir[0])
		sprintf_s(MsgFile, sizeof(MsgFile), "%s/Files/%s", BaseDir, filename);
	else
		sprintf_s(MsgFile, sizeof(MsgFile), "Files/%s", filename);

	if (stat(MsgFile, &STAT) != -1)
	{
		FileSize = STAT.st_size;

		hFile = fopen(MsgFile, "rb");

		if (hFile)
		{
			int Length;
	
			MsgBytes=malloc(FileSize+1);
			fread(MsgBytes, 1, FileSize, hFile); 
			fclose(hFile);

			MsgBytes[FileSize]=0;

			// Remove lf chars

			Length = RemoveLF(MsgBytes, strlen(MsgBytes));

			QueueMsg(conn, MsgBytes, Length);
			free(MsgBytes);

			nodeprintf(conn, "\r\r[End of File %s]\r", filename);
			return;
		}
	}

	nodeprintf(conn, "File %s not found\r", filename);
}

VOID ProcessSuspendedListCommand(CIRCUIT * conn, struct UserInfo * user, char* Buffer, int len)
{
	struct  TempUserInfo * Temp = user->Temp;

	Buffer[len] = 0;

	//	Command entered during listing pause. May be A R or C (or <CR>)

	if (Buffer[0] == 'A' || Buffer[0] == 'a')
	{
		// Abort

		Temp->ListActive = Temp->ListSuspended = FALSE;
		SendPrompt(conn, user);
		return;
	}

	if (_memicmp(Buffer, "R ", 2) == 0)
	{
		// Read Message(es)

		int msgno;
		char * ptr;
		char * Context;

		ptr = strtok_s(&Buffer[2], " ", &Context);

		while (ptr)
		{
			msgno = atoi(ptr);
			ReadMessage(conn, user, msgno);

			ptr = strtok_s(NULL, " ", &Context);
		}

		nodeprintf(conn, "<A>bort, <R Msg(s)>, <CR> = Continue..>");
		return;
	}

	if (Buffer[0] == 'C' || Buffer[0] == 'c' || Buffer[0] == '\r' )
	{
		//	Resume Listing from where we left off

		DoListCommand(conn, user, Temp->LastListCommand, Temp->LastListParams, TRUE);
		SendPrompt(conn, user);
		return;
	}

	nodeprintf(conn, "<A>bort, <R Message>, <CR> = Continue..>");

}
/*
CreateMessageWithAttachments()
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

*/
