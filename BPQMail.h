#ifndef WINVER				// Allow use of features specific to Windows XP or later.
#define WINVER 0x0501		// Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifndef _WIN32_WINDOWS		// Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE			// Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600	// Change this to the appropriate value to target other versions of IE.
#endif


#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#define LIBCONFIG_STATIC
#include "libconfig.h"

#include "compatbits.h"

#ifndef LINBPQ
#include "bpq32.h"
#include "BPQMailrc.h"
#else
#include "CHeaders.h"
#endif

#include "asmstrucs.h"


#define NEWROUTING



// Standard __except handler for try/except

VOID CheckProgramErrors();

extern int ProgramErrors;

extern struct _EXCEPTION_POINTERS exinfox;

#ifdef WIN32
Dump_Process_State(struct _EXCEPTION_POINTERS * exinfo, char * Msg);

#define My__except_Routine(Message) \
__except(memcpy(&exinfo, GetExceptionInformation(), sizeof(struct _EXCEPTION_POINTERS)), EXCEPTION_EXECUTE_HANDLER)\
{\
	Debugprintf("MAILCHAT *** Program Error %x at %x in %s EAX %x EBX %x ECX %x EDX %x ESI %x EDI %x",\
		exinfo.ExceptionRecord->ExceptionCode, exinfo.ExceptionRecord->ExceptionAddress, Message,\
		exinfo.ContextRecord->Eax, exinfo.ContextRecord->Ebx, exinfo.ContextRecord->Ecx,\
		exinfo.ContextRecord->Edx, exinfo.ContextRecord->Esi, exinfo.ContextRecord->Edi);\
		CheckProgramErrors();\
}


/*
#define My__except_Routine(Message) \
__except(memcpy(&exinfox, GetExceptionInformation(), sizeof(struct _EXCEPTION_POINTERS)), EXCEPTION_EXECUTE_HANDLER)\
{\
	Dump_Process_State(&exinfox, Message);\
	CheckProgramErrors();\
}

#define My__except_RoutineWithDisconnect(Message) \
__except(memcpy(&exinfo, GetExceptionInformation(), sizeof(struct _EXCEPTION_POINTERS)), EXCEPTION_EXECUTE_HANDLER)\
{\
	Debugprintf("MAILCHAT *** Program Error %x at %x in %s EAX %x EBX %x ECX %x EDX %x ESI %x EDI %x",\
		exinfo.ExceptionRecord->ExceptionCode, exinfo.ExceptionRecord->ExceptionAddress, Message,\
		exinfo.ContextRecord->Eax, exinfo.ContextRecord->Ebx, exinfo.ContextRecord->Ecx,\
		exinfo.ContextRecord->Edx, exinfo.ContextRecord->Esi, exinfo.ContextRecord->Edi);\
	FreeSemaphore(&ChatSemaphore);\
	if (conn->BPQStream <  0)\
		CloseConsole(conn->BPQStream);\
	else\
		Disconnect(conn->BPQStream);\
}
*/
#define My_except_RoutineWithDiscBBS(Message) \
__except(memcpy(&exinfo, GetExceptionInformation(), sizeof(struct _EXCEPTION_POINTERS)), EXCEPTION_EXECUTE_HANDLER)\
{\
	Debugprintf("MAILCHAT *** Program Error %x at %x in %s EAX %x EBX %x ECX %x EDX %x ESI %x EDI %x",\
		exinfo.ExceptionRecord->ExceptionCode, exinfo.ExceptionRecord->ExceptionAddress, Message,\
		exinfo.ContextRecord->Eax, exinfo.ContextRecord->Ebx, exinfo.ContextRecord->Ecx,\
		exinfo.ContextRecord->Edx, exinfo.ContextRecord->Esi, exinfo.ContextRecord->Edi);\
	if (conn->BPQStream <  0)\
		CloseConsole(conn->BPQStream);\
	else\
		Disconnect(conn->BPQStream);\
	CheckProgramErrors();\
}
#endif
#define MAXUSERNAMELEN 6

#define WSA_ACCEPT WM_USER + 1
#define WSA_CONNECT WM_USER + 2
#define WSA_DATA WM_USER + 3
#define NNTP_ACCEPT WM_USER + 4
#define NNTP_DATA WM_USER + 5

#ifdef _DEBUG

VOID * _malloc_dbg_trace(int len, int type, char * file, int line);

#define   malloc(s)             _malloc_dbg(s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define   calloc(c, s)          _calloc_dbg(c, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define   realloc(p, s)         _realloc_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define   _recalloc(p, c, s)    _recalloc_dbg(p, c, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define   _expand(p, s)         _expand_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define   free(p)               _free_dbg(p, _NORMAL_BLOCK)
#define   _strdup(s)			_strdup_dbg(s, _NORMAL_BLOCK, __FILE__, __LINE__)


#define   zalloc(s)             _zalloc_dbg(s, _NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define   zalloc(s)             _zalloc(s)
#endif

#ifdef LINBPQ

#undef   zalloc
#define  zalloc _zalloc

#endif

VOID * _zalloc_dbg(int len, int type, char * file, int line);

#define LOG_BBS 0
#define LOG_CHAT 1
#define LOG_TCP 2
#define LOG_DEBUG_X 3


//Chat Duplicate suppression Code

#define MAXDUPS 10			// Number to keep
#define DUPSECONDS 5		// TIme to Keep

struct DUPINFO
{
	time_t DupTime;
	char  DupUser[10];
	char  DupText[100];
};


struct UserRec
{
	char * Callsign;
	char * UserName;
	char * Password;
};



typedef struct ConnectionInfo_S
{
	struct ConnectionInfo_S *next;
	PROC *proc;
	UCHAR rtcflags;             // p_linked or p_user.
	int s;                 // Socket.
//	char buf[ln_ibuf];      // Line of incoming text.

	int Number;					// Number of record - for Connections display
//    SOCKET socket;
//	SOCKADDR_IN sin;  
	BOOL Active;
    int BPQStream;
	int paclen;
	UCHAR Callsign[11];			// Station call including SSID
    BOOL GotHeader;
	UCHAR InputMode;			// Line by Line or Binary

    UCHAR * InputBuffer;
    int InputBufferLen;
    int InputLen;				// Data we have already = Offset of end of an incomplete packet;

	struct UserInfo * UserPointer;
    int Retries;
	int	LoginState;				// 1 = user ok, 2 = password ok
	int Flags;

	// Data to the user is kept in a malloc'd buffer. This can be appended to,
	// and data sucked out under both terminal and system flow control. PACLEN is
	// enfored when sending to node.

	UCHAR * OutputQueue;		// Messages to user
	int OutputQueueLength;		// Total Malloc'ed size. Also Put Pointer for next Message
	int OutputGetPointer;		// Next byte to send. When Getpointer = Queue Length all is sent - free the buffer and start again.

	int CloseAfterFlush;		// Close session when all sent. Set to 100ms intervals to wait.
	
	BOOL Paging;				// Set if user wants paging
	int LinesSent;				// Count when paging
	int PageLen;				// Lines per page

	UCHAR * MailBuffer;			// Mail Message being received
	UCHAR * CopyBuffer;			// Mail Message being forwarded
	int MailBufferSize;			// Total Malloc'ed size. Actual size in in Msg Struct

	long lastmsg;				// Last Listed. Stored here, updated in user record only on clean close
	BOOL sysop;					// Set if user is authenticated as a sysop
	BOOL Secure_Session;		// Set if Local Terminal, or Telnet connect with SYSOP status
	UINT BBSFlags;						// Set if defined as a bbs and SID received
	struct MsgInfo * TempMsg;			// Header while message is being received
	struct MsgInfo * FwdMsg;			// Header while message is being forwarded

	char ** To;							// May be several Recipients
	int ToCount;

	int BBSNumber;						// The BBS number (offset into bitlist of BBSes to forward a message to
	int NextMessagetoForward;			// Next index to check in forward cycle
	BOOL BPQBBS;						// Set if SID indicates other end is BPQ
	char MSGTYPES[20];					// Any MSGTYPEFLAGS
	BOOL SendT;							// Send T messages
	BOOL SendP;							// Send P messages
	BOOL SendB;							// Send Bulls
	int MaxBLen;						// Max Size for this session
	int MaxPLen;						// Max Size for this session
	int MaxTLen;						// Max Size for this session
	BOOL DoReverse;						// Request Reverse Forward
	char LastForwardType;				// Last type of messages forwarded
	struct FBBHeaderLine * FBBHeaders;	// The Headers from an FFB forward block
	char FBBReplyChars[36];				//The +-=!nnnn chars for the 5 proposals
	int FBBReplyIndex;					// current Reply Pointer
	int FBBIndex;						// current propopsal number
	int RestartFrom;					// Restart position
	BOOL NeedRestartHeader;				// Set if waiting for 6 byte restart header
	BOOL DontSaveRestartData;			// Set if corrupt data received
	BOOL FBBMsgsSent;					// Messages need to be maked as complete when next command received
	UCHAR FBBChecksum;					// Header Checksum
	BOOL OpenBCM;						// OpenBCM mode (escape -xFF chars)
	BOOL InTelnetExcape;				// Last Char was 0xff
	BOOL LocalMsg;						// Set if current Send command is for a local user
	BOOL NewUser;						// Set if first time user has accessed BBS
	BOOL Paclink;						// Set if receiving messages from Paclink
	BOOL RMSExpress;					// Set if receiving messages from RMS Express
	char ** PacLinkCalls;				// Calls we are getting messages for
	BOOL SkipPrompt;					// Set if a remote node sends a > at the end of his CTEXT
	BOOL SkipConn;						// Node sends "connected" in its CTEXT
	int Watchdog;						// Hung Circuit Detect.
	int SessType;						// BPQ32 sesstype bits

#define Sess_L2LINK 1
#define Sess_SESSION	2
#define Sess_UPLINK	4
#define Sess_DOWNLINK 8
#define Sess_BPQHOST 0x20
#define Sess_PACTOR	0x40

	HANDLE DebugHandle;					// File Handle for session-based debugging

	char ARQFilename[256];				// Filename from ARQ:FILE:: Header
	int ARQClearCount;					// To make sure queues are flushed when sending

	int SIDResponseTimer;				// Used to detect incomplete handshake

	char PQChallenge[20];				// Secure User logon challange
	char SecureMsg[20];					// CMS Secure Signon Response

} ConnectionInfo, CIRCUIT;

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

// BBSFlags Equates

#define BBS 1
#define FBBForwarding 2
#define FBBCompressed 4
#define FBBB1Mode 8
#define FBBB2Mode 16
#define RunningConnectScript 32
#define MBLFORWARDING 64				// MBL Style Frwarding- waiting for OK/NO or Prompt following message
#define TEXTFORWARDING 128				// Plain Text forwarding
#define OUTWARDCONNECT 256				// We connected to them
#define FLARQMODE 512					// Message from FLARQ
#define FLARQMAIL 1024					// Sending FLARQ Format Message
#define ARQMAILACK 2048					// Waiting for all data to be acked
#define	NEEDLF 4096						// Add LF to forward script commands (fro Telnet

struct FBBRestartData
{
	struct MsgInfo * TempMsg;		// Header while message is being received
	struct UserInfo * UserPointer;
	UCHAR * MailBuffer;				// Mail Message being received
	int MailBufferSize;				// Total Malloc'ed size. Actual size in in Msg Struct
	int Count;						// Give up if too many restarts
};

//	We need to keep the B2Message file for B2 messages we are sending until the messages is acked, so
//	we can restart it. Otherwise the file may change, resulting in a checksum error


struct B2RestartData
{
	int CSize;						// Compresses Size (B2 proto)
	UCHAR * CompressedMsg;			// Compressed Body fo B2
	struct MsgInfo * FwdMsg;
	struct UserInfo * UserPointer;
	int Count;						// Give up if too many restarts
};

#pragma pack(1)

struct TempUserInfo
{
	int LastAuthCode;				// Protect against playback attack

	// Fields used to allow interrupting and resuming a paged listing

	BOOL ListActive;				// Doing a list
	BOOL ListSuspended;				// Paused doing a list
	int LastListedInPagedMode;
	char LastListCommand[80];
	char LastListParams[80];
	int LinesSent;
	int LLCount;					// Number still to send in List Last N

};

#define PMSG 1
#define BMSG 2
#define TMSG 3

struct OldUserInfo
{	
	// Old format - without message type specific traffic counts 

	char	Call[10];			//	Connected call without SSID	
//	indicat relai[8];			/* 64 Digis path */
	int		lastmsg;			/* 4  Last L number */
	int		ConnectsIn;				/* 4  Number of connexions in*/
	time_t	TimeLastConnected;  //Last connexion date */
//	long	lastyap __a2__  ;	/* 4  Last YN date */
	ULONG	flags    ;	/* 4  Flags */

	UCHAR	PageLen;			// Lines Per Page
	UCHAR	lang      ;	/* 1  Language */

	int		Xnewbanner;	/* 4  Last Banner date */
	short 	Xdownload  ;	/* 2  download size (KB) = 100 */
	char	POP3Locked ;		// Nonzero if POP3 server has locked this user (stops other pop3 connections, or BBS user killing messages)
	char	BBSNumber;			// BBS Bitmap Index Number
	struct	BBSForwardingInfo * ForwardingInfo;
	struct  UserInfo * BBSNext;	// links BBS record
	struct  TempUserInfo * Temp;	// Working Fields - not saved in user file
	char	xfree[6];			/* 6 Spare */
	char	Xtheme;				/* 1  Current topic */

	char	Name[18];			/* 18 1st Name */
	char	Address[61];		/* 61 Address */

	// Stats. Was City[31];			/* 31 City */

	int MsgsReceived;
	int MsgsSent;
	int MsgsRejectedIn;			// Messages we reject
	int MsgsRejectedOut;		// Messages Rejectd by other end
	int BytesForwardedIn;
	int BytesForwardedOut;
	int ConnectsOut;			// Forwarding Connects Out

	USHORT RMSSSIDBits;			// SSID's to poll in RMS

	char Spare1;

	char	HomeBBS[41];		/* 41 home BBS */
	char	QRA[7];				/* 7  Qth Locator */
	char	pass[13];			/* 13 Password */
	char	ZIP[9];				/* 9  Zipcode */
	BOOL	spare;
} ;                /* Total : 360 bytes */

struct MsgStats
{
	int	ConnectsIn;				/* 4  Number of connexions in*/
	int ConnectsOut;			// Forwarding Connects Out

	// Stats saveed by message type

	int MsgsReceived[4];
	int MsgsSent[4];
	int MsgsRejectedIn[4];		// Messages we reject
	int MsgsRejectedOut[4];		// Messages Rejectd by other end
	int BytesForwardedIn[4];
	int BytesForwardedOut[4];
};

struct UserInfo
{
	// New Format - with stats maintained by message type and unused fields removed.

	char	Call[10];			//	Connected call without SSID	

	int		Length;				// To make subsequent format changes easier

	int		lastmsg;			/* 4  Last L number */
	time_t	TimeLastConnected;  //Last connexion date */
	ULONG	flags    ;	/* 4  Flags */

	UCHAR	PageLen;			// Lines Per Page

	char	POP3Locked ;		// Nonzero if POP3 server has locked this user (stops other pop3 connections, or BBS user killing messages)
	char	BBSNumber;			// BBS Bitmap Index Number
	struct	BBSForwardingInfo * ForwardingInfo;
	struct  UserInfo * BBSNext;	// links BBS record
	struct  TempUserInfo * Temp;	// Working Fields - not saved in user file
	char	Name[18];			/* 18 1st Name */
	char	Address[61];		/* 61 Address */

	USHORT RMSSSIDBits;			// SSID's to poll in RMS

	char	HomeBBS[41];		/* 41 home BBS */
	char	QRA[7];				/* 7  Qth Locator */
	char	pass[13];			/* 13 Password */
	char	ZIP[9];				/* 9  Zipcode */

	struct MsgStats Total;
	struct MsgStats	Last;
	
	char CMSPass[16];			// For Secure Signon
	char Filler[48];			// So we can add a few fields wirhout another resize
};

// flags equates

#define F_Excluded   0x0001
#define F_LOC        0x0002
#define F_Expert     0x0004
#define F_SYSOP      0x0008
#define F_BBS        0x0010
#define F_PAG        0x0020
#define F_GST        0x0040
#define F_MOD        0x0080
#define F_PRV        0x0100
#define F_UNP        0x0200
#define F_NEW        0x0400
#define F_PMS        0x0800
#define F_EMAIL      0x1000
#define F_HOLDMAIL   0x2000
#define F_POLLRMS	 0x4000
#define F_SYSOP_IN_LM 0x8000
#define F_Temp_B2_BBS 0x10000
#define F_NOWINLINK	 0x20000			// Don't add Winlink.org
#define F_NOBULLS	 0x40000	
#define F_NTSMPS	 0x80000	

/* #define F_PWD        0x1000 */


struct Override
{
	char * Call;
	int Days;
};
	
struct ALIAS
{
	char * Alias;
	char * Dest;
};

typedef struct _MESSAGEX
{
//	BASIC LINK LEVEL MESSAGE BUFFER LAYOUT

	struct _MESSAGEX * CHAIN;

	UCHAR	PORT;
	USHORT	LENGTH;

	UCHAR	DEST[7];
	UCHAR	ORIGIN[7];

//	 MAY BE UP TO 56 BYTES OF DIGIS

	UCHAR	CTL;
	UCHAR	PID;
	UCHAR	DATA[256];
	UCHAR	DIGIS[56];				// Padding in case we have digis

}MESSAGEX, *PMESSAGEX;


#pragma pack()

// Message Database Entry. Designed to be compatible with FBB

#define NBBBS 160			// Max BBSes we can forward to. Must be Multiple of 8, and must be 80 for FBB compatibliliy
#define NBMASK NBBBS/8		// Number of bytes in Forward bitlists.

#pragma pack(1)

struct OldMsgInfo
{
	char	type ;
	char	status ;
	int		number ;
	int		length ;
	int		datereceived;
	char	bbsfrom[7] ;			// ? BBS we got it from ?
	char	via[41] ;
	char	from[7] ;
	char	to[7] ;
	char	bid[13] ;
	char	title[61] ;
	char	bin;
	int		nntpnum;			// Number within topic (ie Bull TO Addr) - used for nntp

	UCHAR	B2Flags;

	#define B2Msg 1				// Set if Message File is a formatted B2 message
	#define Attachments 2		// Set if B2 message has attachments
	#define FromPaclink 4
	#define FromRMS 8
	#define FromRMSExpress 16 

	char	free[4];
	unsigned short	nblu;
	int		theme  ;
	time_t	datecreated ;
	time_t	datechanged ;
	char	fbbs[10] ;
	char	forw[10] ;
	char	emailfrom[41];
} ;


struct MsgInfo
{
	char	type ;
	char	status ;
	int		number ;
	int		length ;
	time_t	datereceived;
	char	bbsfrom[7] ;			// ? BBS we got it from ?
	char	via[41] ;
	char	from[7] ;
	char	to[7] ;
	char	bid[13] ;
	char	title[61] ;
	int		nntpnum;			// Number within topic (ie Bull TO Addr) - used for nntp

	UCHAR	B2Flags;

	#define B2Msg 1				// Set if Message File is a formatted B2 message
	#define Attachments 2		// Set if B2 message has attachments
	#define FromPaclink 4
	#define FromRMS 8
	#define FromRMSExpress 16 

	time_t	datecreated ;
	time_t	datechanged ;
	char	fbbs[NBMASK] ;
	char	forw[NBMASK] ;
	char	emailfrom[41];
	char	Locked;				//	Set if selected for sending (NTS Pickup)
	char	Defered;			//	FBB response '=' received
	char	Spare[62];			// For future use
} ;

#define MSGTYPE_B 0
#define MSGTYPE_P 1

#define MSGSTATUS_N 0
#define MSGSTATUS_Y 1
#define MSGSTATUS_F 2
#define MSGSTATUS_K 3
#define MSGSTATUS_H 4
#define MSGSTATUS_D 5
#define MSGSTATUS_$ 6

struct NNTPRec
{
	// Used for NNTP access to Bulls

	struct NNTPRec * Next;	// Record held in chain, so can be held sorted
	char NewsGroup[64];		// = Bull TO.at field
	int	FirstMsg;			// Lowest Number
	int LastMsg;			// Highest Number
	int Count;				// Active Msgs
	time_t DateCreated;		// COntains Creation Date of First Bull in Group
};


typedef struct {
	char	mode;
	char	BID[13];
	union 
	{                   /*  array named screen */
		struct    
		{ 
			unsigned short msgno;
			unsigned short timestamp;
		};
		CIRCUIT * conn;
	} u;
} BIDRec, *BIDRecP;


/* Structures fichiers WP */

typedef struct WPREC {	/* 108 bytes */
	long last;
	short  local;
	char source;
	char callsign[7];
	char homebbs[41];
	char zip[9];
	char name[13];
	char qth[31];
} WPMsgRec, * WPMsgRecP;

typedef struct WPDBASE{	/* 194 bytes */
	char callsign[7];
	char name[13];
	unsigned char Type;
	unsigned char changed;
	unsigned short seen;
	long last_modif;
	long last_seen;
	char first_homebbs[41];
	char secnd_homebbs[41];
	char first_zip[9];
	char secnd_zip[9];
	char first_qth[31];
	char secnd_qth[31];
} WPRec, * WPRecP;


#pragma pack()

struct FWDBAND
{
	time_t FWDStartBand;
	time_t FWDEndBand;
};



struct BBSForwardingInfo
{
	// Holds info for forwarding

	BOOL Enabled;					// Forwarding Enabled
	char ** ConnectScript;			// Main Connect Script
	char ** TempConnectScript;		// Used with FWD command.
	int ScriptIndex;				// Next line in script
	BOOL MoreLines;					// Set until script is finsihed

	char ** TOCalls;				// Calls in to field
	char ** ATCalls;				// Calls in ATBBS field
	char ** HaddressesP;			// Heirarchical Addresses for Personals to forward to (as stored)
	char *** HADDRSP;				// Heirarchical Addresses for Personals to forward to
	char ** Haddresses;				// Heirarchical Addresses to forward to (as stored)
	char *** HADDRS;				// Heirarchical Addresses to forward to
	int * HADDROffet;				// Elements added to complete the HR. At least n+1 must match to forward
	char ** FWDTimes;				// Time bands to forward
	struct FWDBAND ** FWDBands;
	int MsgCount;					// Messages for this BBS
	BOOL ReverseFlag;				// Set if BBS wants to poll for reverse forwarding
	BOOL Forwarding;				// Forward in progress
	int MaxFBBBlockSize;
	BOOL AllowCompressed;			// Allow FBB COmpressed
	BOOL AllowB1;					// Enable B1
	BOOL AllowB2;					// Enable B2 
	BOOL SendCTRLZ;					// Send Ctrl/z instead of /ex
	BOOL PersonalOnly;				// Only Forward Personals
	BOOL SendNew;					// Forward new messages immediately
	int FwdInterval;
	int RevFwdInterval;
	int FwdTimer;
	time_t LastReverseForward;
	char *BBSHA;					// HA of BBS
	char ** BBSHAElements;			// elements of HA of BBS
//	char UserCall[10];				// User we are forwarding on behalf of (Currently only for RMS)
//	int UserIndex;					// index of User we are forwarding on behalf of (Currently only for RMS)
};


struct FBBHeaderLine
{
	//	Holds the info from the (up to) 5 headers presented at the start of a Forward Block

	char Format;					// Ascii or Binary
	char MsgType;					// P B etc 
	char From[7];					// Sender
	char ATBBS[41];					// BBS of recipient (@ Field)
	char To[7];						// Recipient
	char BID[13];
	int Size;
	int CSize;						// Compresses Size (B2 proto)
	BOOL B2Message;					// Set if an FC type
	UCHAR * CompressedMsg;			// Compressed Body fo B2
	struct MsgInfo * FwdMsg;		// Header so we can mark as complete 
};

#define MAXSTACK 20
//#define MAXLINE 10000
#define INPUTLEN 512

#define MAXLINES 1000
#define LINELEN 200

char RTFHeader[4000];

int RTFHddrLen;

struct ConsoleInfo 
{
	struct ConsoleInfo * next;
	CIRCUIT * Console;
	int BPQStream;
	WNDPROC wpOrigInputProc; 
	HWND hConsole;
	HWND hwndInput;
	HWND hwndOutput;
	HMENU hMenu;		// handle of menu 
	RECT ConsoleRect;
	RECT OutputRect;

	int Height, Width, LastY;

	int ClientHeight, ClientWidth;
	char kbbuf[INPUTLEN];
	int kbptr;

	char * readbuff;		// Malloc'ed
	int readbufflen;		// Current Length
	char * KbdStack[MAXSTACK];

	int StackIndex;

	BOOL Bells;
	BOOL FlashOnBell;		// Flash instead of Beep
	BOOL StripLF;

	BOOL WarnWrap;
	BOOL FlashOnConnect;
	BOOL WrapInput;
	BOOL CloseWindowOnBye;

	unsigned int WrapLen;
	int WarnLen;
	int maxlinelen;

	int PartLinePtr;
	int PartLineIndex;		// Listbox index of (last) incomplete line

	DWORD dwCharX;      // average width of characters 
	DWORD dwCharY;      // height of characters 
	DWORD dwClientX;    // width of client area 
	DWORD dwClientY;    // height of client area 
	DWORD dwLineLen;    // line length 
	int nCaretPosX; // horizontal position of caret 
	int nCaretPosY; // vertical position of caret 

	COLORREF FGColour;		// Text Colour
	COLORREF BGColour;		// Background Colour
	COLORREF DefaultColour;	// Default Text Colour

	int CurrentLine;				// Line we are writing to in circular buffer.

	int Index;
	BOOL SendHeader;
	BOOL Finished;

	char OutputScreen[MAXLINES][LINELEN];

	int Colourvalue[MAXLINES];
	int LineLen[MAXLINES];

	int CurrentColour;
	int Thumb;
	int FirstTime;
	BOOL Scrolled;				// Set if scrolled back
	int RTFHeight;				// Height of RTF control in pixels 

};


VOID __cdecl nprintf(CIRCUIT * conn, const char * format, ...);
char * strlop(char * buf, char delim);
int rt_cmd(CIRCUIT *circuit, char * Buffer);
CIRCUIT *circuit_new(CIRCUIT *circuit, int flags);
VOID BBSputs(CIRCUIT * conn, char * buf);
VOID FBBputs(CIRCUIT * conn, char * buf);
void makelinks(void);
VOID * _zalloc(int len);
VOID FreeChatMemory();
VOID ChatTimer();
VOID nputs(CIRCUIT * conn, char * buf);
VOID node_close();
VOID removelinks();
VOID SetupChat();
VOID SendChatLinkStatus();
VOID ClearChatLinkStatus();
VOID Send_MON_Datagram(UCHAR * Msg, DWORD Len);

#define Connect(stream) SessionControl(stream,1,0)
#define Disconnect(stream) SessionControl(stream,2,0)
#define ReturntoNode(stream) SessionControl(stream,3,0)
#define ConnectUsingAppl(stream, appl) SessionControl(stream, 0, appl)

int EncryptPass(char * Pass, char * Encrypt);
VOID DecryptPass(char * Encrypt, unsigned char * Pass, unsigned int len);

// TCP Connections. FOr the moment SMTP or POP3

typedef struct SocketConnectionInfo
{
	struct SocketConnectionInfo * Next;
	int Number;					// Number of record - for Connections display
    SOCKET socket;
	SOCKADDR_IN sin; 
	int Type;					// SMTP or POP3
	BOOL AMPR;					// Set if sending to an AMPR.ORG server
	char FromDomain[50];		// Domain we are sending from
	struct UserInfo * bbs;		// BBS dor forwarding to AMPR
	int State;					// Transaction State Machine
    UCHAR CallSign[10];
    UCHAR TCPBuffer[3000];		// For converting byte stream to messages
    int InputLen;				// Data we have alreasdy = Offset of end of an incomplete packet;

	char * MailFrom;			// Envelope Sender and Receiver
	char ** RecpTo;				// May be several Recipients
	int Recipients;

	UCHAR * MailBuffer;			// Mail Message being received. malloc'ed as needed
	int MailBufferSize;			// Total Malloc'ed size. Actual size is in MailSize
	int MailSize;
	int Flags;

	struct UserInfo * POP3User;
	struct MsgInfo ** POP3Msgs;	// Header List of messages for this uaer
	int POP3MsgCount;			// No of Messages
	int POP3MsgNum;				// Sequence number of message being received

	struct MsgInfo * SMTPMsg;	// message for this SMTP connection

	UCHAR * SendBuffer;			// Message being sent if socket is busy. malloc'ed as needed
	int SendBufferSize;			// Total Malloc'ed size. Actual size is in MailSize
	int SendSize;				// Bytes in buffer
	int SendPtr;				// next byte to send when ready

	struct NNTPRec * NNTPGroup;	// Currently Selected Group
	int NNTPNum;				// Currenrly Selected Msg Number

} SocketConn;

#define SMTPServer 1
#define POP3SLAVE 2
#define SMTPClient 3
#define POP3Client 4
#define NNTPServer 5

// State Values

#define GettingUser 1
#define GettingPass 2
#define Authenticated 4

#define Connecting 8

// SMTP Master

#define WaitingForGreeting 16
#define WaitingForHELOResponse 32
#define WaitingForFROMResponse 64
#define WaitingForTOResponse 128
#define WaitingForDATAResponse 256
#define WaitingForBodyResponse 512
#define WaitingForAUTHResponse 1024

// POP3 Master

#define WaitingForUSERResponse 32
#define WaitingForPASSResponse 64
#define WaitingForSTATResponse 128
#define WaitingForUIDLResponse 256
#define WaitingForLISTResponse 512
#define WaitingForRETRResponse 512
#define WaitingForDELEResponse 1024
#define WaitingForQUITResponse 2048


#define SE 240 // End of subnegotiation parameters
#define NOP 241 //No operation
//#define DM 242 //Data mark Indicates the position of a Synch event within the data stream. This should always be accompanied by a TCP urgent notification.
#define BRK 243 //Break Indicates that the "break" or "attention" key was hi.
#define IP 244 //Suspend Interrupt or abort the process to which the NVT is connected.
#define AO 245 //Abort output Allows the current process to run to completion but does not send its output to the user.
#define AYT 246 //Are you there Send back to the NVT some visible evidence that the AYT was received.
#define EC 247 //Erase character The receiver should delete the last preceding undeleted character from the data stream.
#define EL 248 //Erase line Delete characters from the data stream back to but not including the previous CRLF.
#define GA 249 //Go ahead Under certain circumstances used to tell the other end that it can transmit.
#define SB 250 //Subnegotiation Subnegotiation of the indicated option follows.
#define WILL 251 //will Indicates the desire to begin performing, or confirmation that you are now performing, the indicated option.
#define WONT 252 //wont Indicates the refusal to perform, or continue performing, the indicated option.
#define DOx 253 //do Indicates the request that the other party perform, or confirmation that you are expecting the other party to perform, the indicated option.
#define DONT 254 //dont Indicates the demand that the other party stop performing, or confirmation that you are no longer expecting the other party to perform, the indicated option.
#define IAC  255

#define suppressgoahead 3 //858
//#define Status 5 //859
//#define echo 1 //857
#define timingmark 6 //860
#define terminaltype 24 //1091
#define windowsize 31 //1073
#define terminalspeed 32 //1079
#define remoteflowcontrol 33 //1372
#define linemode 34 //1184
#define environmentvariables 36 //1408

BOOL Initialise();
#ifdef WIN32
INT_PTR CALLBACK ConfigWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
#endif
int DisplaySessions();
int DoStateChange(int Stream);
int DoReceivedData(int Stream);
int DoBBSMonitorData(int Stream);
int Connected(int Stream);
int Disconnected(int Stream);
//int Socket_Accept(int SocketId);
//int Socket_Data(int SocketId,int error, int eventcode);
int DataSocket_Read(SocketConn * sockptr, SOCKET sock);
int DataSocket_Write(SocketConn * sockptr, SOCKET sock);
int DataSocket_Disconnect(SocketConn * sockptr);
int RefreshMainWindow();
int Terminate();
int SendtoSocket(SOCKET sock,char * Msg);
int WriteLog(char * msg);
int ConnectState(int Stream);
UCHAR * EncodeCall(UCHAR * Call);
int ParseIniFile(char * fn);
struct UserInfo * AllocateUserRecord(char * Call);
struct MsgInfo * AllocateMsgRecord();
BIDRec * AllocateBIDRecord();
BIDRec * AllocateTempBIDRecord();
struct UserInfo * LookupCall(char * Call);
BIDRec  * LookupBID(char * BID);
BIDRec  * LookupTempBID(char * BID);
VOID RemoveTempBIDS(CIRCUIT * conn);
VOID SaveUserDatabase();
VOID GetUserDatabase();
VOID GetMessageDatabase();
VOID SaveMessageDatabase();
VOID GetBIDDatabase();
VOID SaveBIDDatabase();
VOID GetWPDatabase();
VOID CopyWPDatabase();
VOID SaveWPDatabase();
VOID GetBadWordFile();
WPRec * LookupWP(char * Call);
VOID SendWelcomeMsg(int Stream, ConnectionInfo * conn, struct UserInfo * user);
VOID ProcessLine(ConnectionInfo * conn, struct UserInfo * user, char* Buffer, int len);
VOID ProcessChatLine(ConnectionInfo * conn, struct UserInfo * user, char* Buffer, int len);
VOID SendPrompt(ConnectionInfo * conn, struct UserInfo * user);
int QueueMsg(	ConnectionInfo * conn, char * msg, int len);
VOID SendUnbuffered(int stream, char * msg, int len);
//int GetFileList(char * Dir);
BOOL ListMessage(struct MsgInfo * Msg, ConnectionInfo * conn, BOOL SendFullFrom);
void DoDeliveredCommand(CIRCUIT * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context);
void DoKillCommand(ConnectionInfo * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context);
void DoListCommand(ConnectionInfo * conn, struct UserInfo * user, char * Cmd, char * Arg1, BOOL Resuming);
void DoReadCommand(ConnectionInfo * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context);
void KillMessage(ConnectionInfo * conn, struct UserInfo * user, int msgno);
int KillMessagesTo(ConnectionInfo * conn, struct UserInfo * user, char * Call);
int KillMessagesFrom(ConnectionInfo * conn, struct UserInfo * user, char * Call);
void DoUnholdCommand(CIRCUIT * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context);

VOID FlagAsKilled(struct MsgInfo * Msg);
int ListMessagesFrom(ConnectionInfo * conn, struct UserInfo * user, char * Call, BOOL SendFullFrom, int Start);
int ListMessagesTo(ConnectionInfo * conn, struct UserInfo * user, char * Call, BOOL SendFullFrom, int Start);
int ListMessagesAT(ConnectionInfo * conn, struct UserInfo * user, char * Call, BOOL SendFullFrom, int Start);
void ListMessagesInRange(ConnectionInfo * conn, struct UserInfo * user, char * Call, int Start, int End, BOOL SendFullFrom );
void ListMessagesInRangeForwards(ConnectionInfo * conn, struct UserInfo * user, char * Call, int Start, int End, BOOL SendFullFrom );
int GetUserMsg(int m, char * Call, BOOL SYSOP);
void Flush(ConnectionInfo * conn);
VOID ClearQueue(ConnectionInfo * conn);
void TrytoSend();
void ReadMessage(ConnectionInfo * conn, struct UserInfo * user, int msgno);
struct MsgInfo * FindMessage(char * Call, int msgno, BOOL sysop);
char * ReadMessageFile(int msgno);
char * ReadInfoFile(char * File);
char * FormatDateAndTime(time_t Datim, BOOL DateOnly);
int	CriticalErrorHandler(char * error);
BOOL DoSendCommand(ConnectionInfo * conn, struct UserInfo * user, char * Cmd, char * Arg1, char * Context);
BOOL CreateMessage(ConnectionInfo * conn, char * From, char * ToCall, char * ATBBS, char MsgType, char * BID, char * Title);
VOID ProcessMsgTitle(ConnectionInfo * conn, struct UserInfo * user, char* Buffer, int len);
VOID ProcessMsgLine(CIRCUIT * conn, struct UserInfo * user, char* Buffer, int len);
VOID CreateMessageFile(ConnectionInfo * conn, struct MsgInfo * Msg);
int ProcessConnecting(CIRCUIT * circuit, char * Buffer, int Len);
VOID SaveConfig(char * ConfigName);
BOOL GetConfig(char * ConfigName);
int GetIntValue(config_setting_t * group, char * name);
BOOL GetStringValue(config_setting_t * group, char * name, char * value);
BOOL GetConfigFromRegistry();
VOID Parse_SID(CIRCUIT * conn, char * SID, int len);
VOID ProcessMBLLine(CIRCUIT * conn, struct UserInfo * user, UCHAR* Buffer, int len);
VOID ProcessFBBLine(ConnectionInfo * conn, struct UserInfo * user, UCHAR * Buffer, int len);
VOID SetupNextFBBMessage(CIRCUIT * conn);
BOOL DecodeSendParams(CIRCUIT * conn, char * Context, char ** From, char ** To, char ** ATBBS, char ** BID);
int PrintMessages(HWND hDlg, int Count, int * Indexes);
int check_fwd_bit(char *mask, int bbsnumber);
void set_fwd_bit(char *mask, int bbsnumber);
void clear_fwd_bit (char *mask, int bbsnumber);
VOID SetupForwardingStruct(struct UserInfo * user);
BOOL Forward_Message(struct UserInfo * user, struct MsgInfo * Msg);
VOID StartForwarding (int BBSNumber, char ** TempScript);
BOOL Reverse_Forward();
int ProcessBBSConnectScript(CIRCUIT * conn, char * Buffer, int len);
BOOL FBBDoForward(CIRCUIT * conn);
BOOL FindMessagestoForward(CIRCUIT * conn);
BOOL SeeifMessagestoForward(int BBSNumber, CIRCUIT * Conn);
int CountMessagestoForward(struct UserInfo * user);

VOID * GetMultiLineDialogParam(HWND hDialog, int DLGItem);

#define LIBCONFIG_STATIC
#include "libconfig.h"
VOID * GetMultiStringValue(config_setting_t * hKey, char * ValueName);
VOID * RegGetMultiStringValue(HKEY hKey, char * ValueName);

int MultiLineDialogToREG_MULTI_SZ(HWND hWnd, int DLGItem, HKEY hKey, char * ValueName);
int Do_BBS_Sel_Changed(HWND hDlg);
VOID FreeForwardingStruct(struct UserInfo * user);
VOID FreeList(char ** Hddr);
int Do_User_Sel_Changed(HWND hDlg);
int Do_Msg_Sel_Changed(HWND hDlg);
VOID Do_Save_Msg();
VOID Do_Add_User(HWND hDlg);
VOID Do_Delete_User(HWND hDlg);
VOID FlagSentMessages(CIRCUIT * conn, struct UserInfo * user);
VOID Do_Save_User(HWND hDlg, BOOL ShowBox);
VOID DeleteBBS();
VOID AddBBS();
VOID SaveBBSConfig();
BOOL GetChatConfig();
VOID SaveChatConfig();
VOID SaveISPConfig();
VOID SaveFWDConfig();
VOID SaveMAINTConfig();
VOID SaveWelcomeMsgs();
VOID SavePrompts();
VOID ReinitializeFWDStruct(struct UserInfo * user);
VOID CopyBIDDatabase();
VOID CopyMessageDatabase();
VOID CopyUserDatabase();
VOID FWDTimerProc();
VOID CreateMessageFromBuffer(CIRCUIT * conn);
VOID __cdecl nodeprintf(ConnectionInfo * conn, const char * format, ...);
VOID __cdecl nodeprintfEx(ConnectionInfo * conn, const char * format, ...);
VOID FreeOverrides();
VOID SendMessageToSYSOP(char * Title, char * MailBuffer, int Length);
struct UserInfo * FindRMS();
VOID FindNextRMSUser(struct BBSForwardingInfo * FWDInfo);
BOOL ConnecttoBBS (struct UserInfo * user);
BOOL SetupNewBBS(struct UserInfo * user);
VOID CreateRegBackup();
VOID SaveFilters(HWND hDlg);
BOOL CheckRejFilters(char * From, char * To, char * ATBBS);
BOOL CheckHoldFilters(char * From, char * To, char * ATBBS);
BOOL CheckifLocalRMSUser(char * FullTo);
VOID DoWPLookup(ConnectionInfo * conn, struct UserInfo * user, char Type, char *Context);
BOOL wildcardcompare(char * Target, char * Match);
VOID SendWarningToSYSOP(struct MsgInfo * Msg);
VOID DoEditUserCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context);
VOID DoPollRMSCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context);
VOID DoShowRMSCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context);
VOID DoSetIdleTime(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context);
VOID DoFwdCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context);
VOID SaveFwdParams(char * Call, struct BBSForwardingInfo * ForwardingInfo);
VOID DoAuthCmd(CIRCUIT * conn, struct UserInfo * user, char * Arg1, char * Context);
VOID ProcessSuspendedListCommand(CIRCUIT * conn, struct UserInfo * user, char* Buffer, int len);

// FBB Routines

VOID SendCompressed(CIRCUIT * conn, struct MsgInfo * FwdMsg);
VOID SendCompressedB2(CIRCUIT * conn, struct FBBHeaderLine * FBBHeader);
VOID UnpackFBBBinary(CIRCUIT * conn);
void Decode(CIRCUIT * conn) ;
//long Encode(char * in, char * out, long inlen, BOOL B1Protocol);

BOOL CreateB2Message(CIRCUIT * conn, struct FBBHeaderLine * FBBHeader, char * Rline);
VOID SaveFBBBinary(CIRCUIT * conn);
BOOL LookupRestart(CIRCUIT * conn, struct FBBHeaderLine * FBBHeader);
BOOL DoWeWantIt(CIRCUIT * conn, struct FBBHeaderLine * FBBHeader);

// Console Routines

BOOL CreateConsole(int Stream);
int WritetoConsoleWindow(int Stream, char * Msg, int len);
int ToggleParam(HMENU hMenu, HWND hWnd, BOOL * Param, int Item);
void CopyRichTextToClipboard(HWND hWnd);
void CopyToClipboard(HWND hWnd);
VOID CloseConsole(int Stream);

// Monitor Routines

BOOL CreateMonitor();
int WritetoMonitorWindow(char * Msg, int len);

BOOL CreateDebugWindow();
VOID WritetoDebugWindow(char * Msg, int len);
VOID ClearDebugWindow();
int RemoveLF(char * Message, int len);

// Utilities

BOOL isdigits(char * string);
void GetSemaphore(struct SEM * Semaphore, int ID);
void FreeSemaphore(struct SEM * Semaphore);

VOID __cdecl Debugprintf(const char * format, ...);
VOID __cdecl Logprintf(int LogMode, CIRCUIT * conn, int InOut, const char * format, ...);

VOID SortBBSChain();
VOID ExpandAndSendMessage(CIRCUIT * conn, char * Msg, int LOG);
int ImportMessages(CIRCUIT * conn, char * FN, BOOL Nopopup);

// TCP Routines

BOOL InitialiseTCP();
VOID SetupListenSet();
VOID TCPTimer();
VOID TCPFastTimer();
int Socket_Data(int sock, int error, int eventcode);
int Socket_Accept(int SocketId);
int Socket_Connect(SOCKET sock, int Error);
VOID ProcessSMTPServerMessage(SocketConn * sockptr, char * Buffer, int Len);
int CreateSMTPMessage(SocketConn * sockptr, int i, char * MsgTitle, time_t Date, char * MsgBody, int Msglen, BOOL B2Flag);
BOOL CreateSMTPMessageFile(char * Message, struct MsgInfo * Msg);
SOCKET CreateListeningSocket(int Port);
int TidyString(char * MailFrom);
VOID ProcessPOP3ServerMessage(SocketConn * sockptr, char * Buffer, int Len);
char *str_base64_encode(char *str);
int b64decode(char *str);
SocketConn * SMTPConnect(char * Host, int Port, BOOL AMPR, struct MsgInfo * Msg, char * MsgBody);
BOOL POP3Connect(char * Host, int Port);
VOID ProcessSMTPClientMessage(SocketConn * sockptr, char * Buffer, int Len);
VOID ProcessPOP3ClientMessage(SocketConn * sockptr, char * Buffer, int Len);
int CreatePOP3Message(char * From, char * To, char * MsgTitle, time_t Date, char * MsgBody, int MsgLen, BOOL B2Flag);
void WriteLogLine(CIRCUIT * conn, int Flag, char * Msg, int MsgLen, int Flags);
int SendSock(SocketConn * sockptr, char * msg);
VOID __cdecl sockprintf(SocketConn * sockptr, const char * format, ...);
VOID SendFromQueue(SocketConn * sockptr);
VOID SendMultiPartMessage(SocketConn * sockptr, struct MsgInfo * Msg, UCHAR * msgbytes);
int CountMessagesTo(struct UserInfo * user, int * Unread);

BOOL SendtoISP();

// NNTP ROutines

VOID InitialiseNNTP();
VOID BuildNNTPList(struct MsgInfo * Msg);
int NNTP_Data(int sock, int error, int eventcode);
int NNTP_Accept(int SocketId);

VOID * GetOverrides(config_setting_t * group, char * ValueName);
VOID * RegGetOverrides(HKEY hKey, char * ValueName);

VOID DoHouseKeeping(BOOL Mainual);
VOID ExpireMessages();
VOID KillMsg(struct MsgInfo * Msg);
BOOL RemoveKilledMessages();
VOID Renumber_Messages();
BOOL ExpireBIDs();
VOID MailHousekeepingResults();
VOID CreateBBSTrafficReport();
VOID CreateWPReport();

// WP Routines

VOID ProcessWPMsg(char * MailBuffer, int Size, char * FisrtRLine);
VOID GetWPInfoFromRLine(char * From, char * FirstRLine, time_t RLineTime);
VOID UpdateWPWithUserInfo(struct UserInfo * user);
VOID GetWPBBSInfo(char * Rline);

// UI Routines

VOID SetupUIInterface();
VOID Free_UI();
VOID SendLatestUI(int Port);
VOID SendMsgUI(struct MsgInfo * Msg);
static VOID Send_AX_Datagram(UCHAR * Msg, DWORD Len, UCHAR Port, UCHAR * HWADDR, BOOL Queue);
VOID SeeifBBSUIFrame(struct _MESSAGEX * buff, int len);
struct MsgInfo * FindMessageByNumber(int msgno);
int CountConnectionsOnPort(int CheckPort);

// Message Routing Routtines

VOID SetupHAElements(struct BBSForwardingInfo * ForwardingInfo);
VOID SetupHAddreses(struct	BBSForwardingInfo * ForwardingInfo);
VOID SetupHAddresesP(struct	BBSForwardingInfo * ForwardingInfo);
VOID SetupMyHA();
VOID SetupFwdAliases();
struct Continent * FindContinent(char * Name);
int MatchMessagetoBBSList(struct MsgInfo * Msg, CIRCUIT * conn);
BOOL CheckABBS(struct MsgInfo * Msg, struct UserInfo * bbs, struct	BBSForwardingInfo * ForwardingInfo, char * ATBBS, char * HRoute);
BOOL CheckBBSToList(struct MsgInfo * Msg, struct UserInfo * bbs, struct	BBSForwardingInfo * ForwardingInfo);
BOOL CheckBBSAtList(struct MsgInfo * Msg, struct BBSForwardingInfo * ForwardingInfo, char * ATBBS);
BOOL CheckBBSHList(struct MsgInfo * Msg, struct UserInfo * bbs, struct	BBSForwardingInfo * ForwardingInfo, char * ATBBS, char * HRoute);
BOOL CheckBBSHElements(struct MsgInfo * Msg, struct UserInfo * bbs, struct	BBSForwardingInfo * ForwardingInfo, char * ATBBS, char ** HElements);
BOOL CheckBBSHElementsFlood(struct MsgInfo * Msg, struct UserInfo * bbs, struct	BBSForwardingInfo * ForwardingInfo, char * ATBBS, char ** HElements);
int CheckBBSToForNTS(struct MsgInfo * Msg, struct BBSForwardingInfo * ForwardingInfo);
int CheckBBSATListWildCarded(struct MsgInfo * Msg, struct BBSForwardingInfo * ForwardingInfo, char * ATBBS);

VOID ReRouteMessages();

extern int _MYTIMEZONE;

extern HKEY REGTREE;	
extern char * REGTREETEXT;

extern HBRUSH bgBrush;
extern BOOL cfgMinToTray;

extern CIRCUIT * Console;

extern ULONG ChatApplMask;
extern char Verstring[];

extern char SignoffMsg[];
extern char AbortedMsg[];
extern char InfoBoxText[];			// Text to display in Config Info Popup

extern int LastVer[4];				// In case we need to do somthing the first time a version is run

extern HWND MainWnd;
extern char BaseDir[];
extern char BaseDirRaw[];
extern char MailDir[];
extern char WPDatabasePath[];
extern char RlineVer[50];

extern BOOL LogBBS;
extern BOOL LogCHAT;
extern BOOL LogTCP;
extern BOOL ForwardToMe;

extern BOOL AllowAnon;
extern BOOL DontNeedHomeBBS;

extern int LatestMsg;
extern char BBSName[];
extern char SYSOPCall[];
extern char BBSSID[];
extern char NewUserPrompt[];

extern char * WelcomeMsg;
extern char * NewWelcomeMsg;
extern char * ChatWelcomeMsg;
extern char * NewChatWelcomeMsg;
extern char * ExpertWelcomeMsg;

extern char * Prompt;
extern char * NewPrompt;
extern char * ExpertPrompt;

// Filter Params

extern char ** RejFrom;					// Reject on FROM Call
extern char ** RejTo;						// Reject on TO Call
extern char ** RejAt;						// Reject on AT Call

extern char ** HoldFrom;					// Hold on FROM Call
extern char ** HoldTo;						// Hold on TO Call
extern char ** HoldAt;						// Hold on AT Call

// Send WP Params

extern BOOL SendWP;
extern char SendWPVIA[81];
extern char SendWPTO[11];
extern int SendWPType;

extern int Ver[4];

extern struct MsgInfo ** MsgHddrPtr;

extern BIDRec ** BIDRecPtr;
extern int NumberofBIDs;

extern struct NNTPRec * FirstNNTPRec;
//extern int NumberofNNTPRecs;


extern int NumberofMessages;
extern int FirstMessageIndextoForward;

extern WPRec ** WPRecPtr;
extern int NumberofWPrecs;

extern struct SEM AllocSemaphore;
extern struct SEM ConSemaphore;
extern struct SEM MsgNoSemaphore;

extern struct MsgInfo * MsgnotoMsg[];	// Message Number to Message Slot List.


extern char hostname[];
extern char RtUsr[];
extern char RtUsrTemp[];
extern char RtKnown[];
extern int AXIPPort;
extern BOOL NeedStatus;

extern BOOL ISP_Gateway_Enabled;
extern BOOL SMTPAuthNeeded;


extern int MaxMsgno;
extern int BidLifetime;
extern int MaxAge;
extern int MaintInterval;
extern int MaintTime;
extern int UserLifetime;

extern int MaxRXSize;
extern int MaxTXSize;

extern char OurNode[];
extern char OurAlias[];
extern BOOL SMTPMsgCreated;

extern HINSTANCE hInst;
extern HWND hWnd;
extern RECT MainRect;

extern char BBSName[];
extern char HRoute[];
extern char AMPRDomain[];
extern BOOL SendAMPRDirect;
extern int BBSApplNum;
extern int SMTPInPort;
extern int POP3InPort;
extern int NNTPInPort;
extern BOOL RemoteEmail;

extern int MaxStreams;
extern UCHAR * OtherNodes;
extern struct UserInfo * BBSChain;		// Chain of users that are BBSes
extern struct UserInfo ** UserRecPtr;
extern int NumberofUsers;
extern struct MsgInfo ** MsgHddrPtr;
extern int NumberofMessages;
extern int HighestBBSNumber;
extern HMENU hFWDMenu;									// Forward Menu Handle
extern char zeros[];						// For forward bitmask tests
extern BOOL EnableUI;
extern BOOL RefuseBulls;
extern BOOL SendSYStoSYSOPCall;
extern BOOL SendBBStoSYSOPCall;
extern BOOL DontHoldNewUsers;
extern BOOL UIEnabled[];
extern BOOL UINull[];
extern BOOL UIMF[];
extern BOOL UIHDDR[];
extern char * UIDigi[];
extern int MailForInterval;
extern char MailForText[];

extern BOOL ISP_Gateway_Enabled;

extern char MyDomain[];			// Mail domain for BBS<>Internet Mapping

extern char ISPSMTPName[];
extern int ISPSMTPPort;

extern char ISPPOP3Name[];
extern int ISPPOP3Port;
extern int ISPPOP3Interval;

extern char ISPAccountName[];
extern char ISPAccountPass[];
extern char EncryptedISPAccountPass[];
extern int EncryptedPassLen;
extern char *month[]; 

extern HWND hDebug;
extern RECT MonitorRect;
extern RECT DebugRect;
extern HWND hMonitor;
//extern HWND hConsole;
//extern RECT ConsoleRect;
extern int LogAge;
extern BOOL DeletetoRecycleBin;
extern BOOL SuppressMaintEmail;
extern BOOL SaveRegDuringMaint;
extern BOOL SendWP;
extern BOOL OverrideUnsent;
extern BOOL SendNonDeliveryMsgs;

extern int PR;
extern int PUR;
extern int PF;
extern int PNF;
extern int BF;
extern int BNF;
extern int NTSD;
extern int NTSU;
extern int NTSF;
extern int AP;
extern int AB;

extern struct Override ** LTFROM;
extern struct Override ** LTTO;
extern struct Override ** LTAT;

extern int LastHouseKeepingTime;
extern int LastTrafficTime;

extern char * MyElements[];
extern char ** AliasText;
extern struct ALIAS ** Aliases;

extern BOOL ReaddressLocal;
extern BOOL ReaddressReceived;
extern BOOL WarnNoRoute;
extern BOOL Localtime;

struct ConsoleInfo * ConsHeader[2];

extern BOOL NeedHomeBBS;
extern char ConfigName[250];
extern BOOL UsingingRegConfig;