
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include "compatbits.h"

#ifndef LINBPQ

#include "asmstrucs.h"
#include "bpq32.h"
#include "chatrc.h"
#else

#define WCHAR  wchar_t
#endif

#ifdef LINBPQ
#ifdef WIN32
#include "C:\Program Files (X86)\GnuWin32\include\iconv.h"
#else
#include <iconv.h>
#endif
#endif

#define IDC_STATIC -1
#define IDS_APP_TITLE 103
#define IDR_MAINFRAME 128
#define IDD_CONFIG 102
#define IDD_ABOUTBOX 103
#define IDM_ABOUT 104
#define IDM_EXIT 105
#define IDM_CONSOLE 120
#define IDM_MONITOR 121
#define IDC_BPQMailChat 109
#define IDM_DISCONNECT 2000
#define IDM_LOGGING 2100
#define IDM_CONFIG 110
#define IDC_MYICON 2
#define IDC_BBSCall 1001
#define IDC_BaseDir 1002
#define IDC_BBSAppl 1003
#define IDC_BBSStreams 1004
#define IDC_POP3Port 1005
#define IDC_SMTPPort 1006
#define IDC_HRoute 1007
#define IDC_SYSOPCALL 1008
#define IDC_REMOTEEMAIL 1009
#define IDC_BBSSAVE 1100
#define IDC_ChatAppl 2001
#define IDC_ChatNodes 2002
#define SAVENODES 2100


#define BPQBASE 1024
#define BPQMTX 1040
#define BPQMCOM 1041
#define BPQCOPYMON 1042
#define BPQCOPYOUT 1043
#define BPQCLEARMON 1044
#define BPQCLEAROUT 1045
#define BPQBELLS 1046
#define BPQCHAT 1047
#define BPQHELP 1048
#define BPQStripLF 1049
#define BPQLogOutput 1050
#define BPQLogMonitor 1051
#define BPQSendDisconnected 1052
#define BPQFLASHONBELL 1053

#define MONBBS 1060
#define MONCHAT 1061
#define MONTCP 1062

#define IDC_NODES 501
#define IDC_USERS 502
#define IDC_LINKS 503
#define IDC_SYSOPMSGS 504
#define IDC_FWDINT 505
#define IDC_UTC 506
#define IDC_LOCAL 507
#define IDC_MSGS 508 
#define IDC_HELD 509

#define IDD_USEREDIT 200
#define IDD_FORWARDING 201
#define IDD_MSGEDIT 202


#define IDD_USERADDED_BOX 5051
#define CHAT_CONFIG 9013
#define IDC_ChatNodes 2002

// Standard __except handler for try/except

VOID CheckProgramErrors();

extern int ProgramErrors;

struct _EXCEPTION_POINTERS;

int Dump_Process_State(struct _EXCEPTION_POINTERS * exinfo, char * Msg);

#define My__except_Routine(Message) \
__except(memcpy(&exinfo, GetExceptionInformation(), sizeof(struct _EXCEPTION_POINTERS)), EXCEPTION_EXECUTE_HANDLER)\
{\
	Debugprintf("CHAT *** Program Error %x at %x in %s EAX %x EBX %x ECX %x EDX %x ESI %x EDI %x",\
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
	Debugprintf("CHAT *** Program Error %x at %x in %s EAX %x EBX %x ECX %x EDX %x ESI %x EDI %x",\
		exinfo.ExceptionRecord->ExceptionCode, exinfo.ExceptionRecord->ExceptionAddress, Message,\
		exinfo.ContextRecord->Eax, exinfo.ContextRecord->Ebx, exinfo.ContextRecord->Ecx,\
		exinfo.ContextRecord->Edx, exinfo.ContextRecord->Esi, exinfo.ContextRecord->Edi);\
	if (conn->BPQStream <  0)\
		CloseConsole(conn->BPQStream);\
	else\
		Disconnect(conn->BPQStream);\
	CheckProgramErrors();\
}

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

VOID * _zalloc_dbg(int len, int type, char * file, int line);

#define LOG_CHAT 1
#define LOG_DEBUGx 3


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

//#define ln_ibuf 128
#define deftopic "General"


// Protocol version.

#define FORMAT       1	 // Ctrl/A
#define FORMAT_O     0   // Offset in frame to format byte.
#define TYPE_O       1   // Offset in frame to kind byte.
#define DATA_O       2   // Offset in frame to data.

// Protocol Frame Types.

#define id_join   'J'    // User joins RT.
#define id_leave  'L'    // User leaves RT.
#define id_link   'N'    // Node joins RT.
#define id_unlink 'Q'    // Node leaves RT.
#define id_data   'D'    // Data for all users.
#define id_send   'S'    // Data for one user.
#define id_topic  'T'    // User changes topic.
#define id_user   'I'    // User login information.
#define id_keepalive   'K'    // Node-Node Keepalive.

#define o_all    1  // To all users.
#define o_one    2  // To a specific user.
#define o_topic  3  // To all users in a specific topic.


// RT protocol version 1.
// First two bytes are FORMAT and Frame Type.
// These are followed by text fields delimited by blanks.
// Note that "node", "to", "from", "user" are callsigns.

// ^AD<node> <user> <text>        - Data for all users.
// ^AI<node> <user> <name> <qth>  - User information.
// ^AJ<node> <user> <name> <qth>  - User joins.
// ^AL<node> <user> <name> <qth>  - User leaves.
// ^AN<node> <node> <alias>       - Node joins.
// ^AQ<node> <node>               - Node leaves.
// ^AS<node> <from> <to>   <text> - Data for one user.
// ^AT<node> <user> <topic>       - User changes topic.

// Connect protocol:

// 1. Connect to node.
// 2. Send *RTL
// 3. Receive OK. Will get disconnect if link is not allowed.
// 4. Go to it.

// Disconnect protocol:

// 1. If there are users on this node, send an id_leave for each user,
//    to each node you are disconnecting from.
// 2. Disconnect.

// Other RT systems to link with. Flags can be p_linked, p_linkini.

typedef struct link_t
{
	struct link_t *next;
	char *alias;
	char *call;
	int  flags; // See circuit flags.
	int delay;	// Limit connects when failing

} LINK;


typedef struct knownnode_t
{
	struct knownnode_t *next;
	char *call;
	time_t LastHeard;

} KNOWNNODE;


// Topics.

typedef struct topic_t
{
	struct topic_t *next;
	char  *name;
	int  refcnt;
} TOPIC;

// Nodes.

typedef struct node_t
{
	struct node_t *next;
	char *alias;
	char *call;
	char * Version;
	int refcnt;
} CHATNODE;


// Topics in use at each circuit.

typedef struct ct_t
{
	struct ct_t *next;
	TOPIC *topic;
	int  refcnt;
} CT;

// Nodes reached on each circuit.

typedef struct cn_t
{
	struct cn_t *next;
	CHATNODE *node;
	int refcnt;
} CN;

// Circuits.
// A circuit may be used by one local user, or one link.
// If it is used by a link, there may be many users on that link.

// Bits for circuit flags and link flags.

#define p_nil     0x00    // Circuit is being shut down.
#define p_user    0x01    // User connected.
#define p_linked  0x02    // Active link with another RT.
#define p_linkini 0x04    // Outgoing link setup with another RT.
#define p_linkwait 0x08   // Incoming link setup - waiting for *RTL


// Users. Could be connected at any node.

#define u_echo 0x0002		// User wants his text echoed to him.
#define u_bells 0x0004		// User wants bell when other users join.
#define u_colour 0x0008		// User wants BPQTerminal colour codes.
#define u_keepalive 0x0010	// User wants Keepalive Messages.
#define u_shownames 0x0020	// User wants name as well as call on each message.
#define u_showtime 0x0040	// User wants time on each message.
#define u_auto 0x0080		// Determine UTF-8 Mode automatically.
#define u_noUTF8 0x0100		// Terminal is not using UTF-8.

struct UserInfo{

	char	Call[10];			//	Connected call without SSID	
	char	Name[18];			/* 18 1st Name */

}; 

typedef struct ChatConnectionInfo_S
{
	struct ChatConnectionInfo_S *next;
	PROC *proc;
	UCHAR rtcflags;             // p_linked or p_user.
	int s;                 // Socket.
//	char buf[ln_ibuf];      // Line of incoming text.
	union
	{
		struct user_t *user;  // Associated user if local.
		LINK *link;           // Associated link if link.
	} u;
	int refcnt;            // If link, # of users on that link.
	CN   *hnode;            // Nodes heard from this link.
	CT   *topic;            // Out this circuit if from these topics.

	int Number;					// Number of record - for Connections display
	BOOL Active;
    int BPQStream;
	int paclen;
	UCHAR Callsign[11];			// Station call including SSID
    BOOL GotHeader;

	char FBBReplyChars[80];		// Version from other end

    UCHAR InputBuffer[10000];
    int InputLen;				// Data we have already = Offset of end of an incomplete packet;

	struct UserInfo * UserPointer;
    int Retries;
	int	LoginState;				// 1 = user ok, 2 = password ok
	int Flags;

	// Data to the user is kept in a static buffer. This can be appended to,
	// and data sucked out under both terminal and system flow control. PACLEN is
	// enfored when sending to node.

	UCHAR OutputQueue[10000];	// Messages to user
	int OutputQueueLength;		// Total Malloc'ed size. Also Put Pointer for next Message
	int OutputGetPointer;		// Next byte to send. When Getpointer = Queue Length all is sent - free the buffer and start again.

	int CloseAfterFlush;		// Close session when all sent. Set to 100ms intervals to wait.
	
	BOOL sysop;					// Set if user is authenticated as a sysop
	BOOL Secure_Session;		// Set if Local Terminal, or Telnet connect with SYSOP status

	BOOL NewUser;						// Set if first time user has accessed BBS
	int Watchdog;						// Hung Circuit Detect.
	int SessType;						// BPQ32 sesstype bits

#define Sess_L2LINK 1
#define Sess_SESSION	2
#define Sess_UPLINK	4
#define Sess_DOWNLINK 8
#define Sess_BPQHOST 0x20
#define Sess_PACTOR	0x40

	HANDLE DebugHandle;					// File Handle for session-based debugging

} ChatConnectionInfo, ChatCIRCUIT;



// Flags Equates

#define GETTINGUSER 1
#define CHATMODE 4
#define CHATLINK 32					// Link to another Chat Node

// BBSFlags Equates


#pragma pack(1)

struct TempUserInfo
{
	int LastAuthCode;				// Protect against playback attack
};


typedef struct user_t
{
	struct  user_t *next;
	char    *call;
	char    *name;
	char    *qth;
	CHATNODE    *node;          // Node user logged into.
	ChatCIRCUIT *circuit;       // Circuit user is on, local or link.
	TOPIC   *topic;         // Topic user is in.
	int     rtflags;
	time_t	lastmsgtime;	// Time of last input from user
	time_t	lastsendtime;	// Time of last output to user
	int Colour;				// For Console Display
#ifdef LINBPQ
	char Codepage[80];		// For Converting UTF8 to local char set for non-utf-8 terminals
	iconv_t iconv_toUTF8;	// per-uswer converison handles
	iconv_t iconv_fromUTF8;
#else
	int Codepage;
#endif
} USER;

#pragma pack()


#pragma pack(1)


#pragma pack()


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
	ChatCIRCUIT * Console;
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

	WCHAR * readbuff;		// Malloc'ed
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

	WCHAR OutputScreen[MAXLINES][LINELEN];

	int Colourvalue[MAXLINES];
	int LineLen[MAXLINES];

	int CurrentColour;
	int Thumb;
	int FirstTime;
	BOOL Scrolled;				// Set if scrolled back
	int RTFHeight;				// Height of RTF control in pixels 

};


extern USER *user_hd;

static PROC *Rt_Control;
static int  rtrun = FALSE;

//#define rtjoin  "*** Joined"
#define rtleave "*** Left"

KNOWNNODE *knownnode_find(char *call);
static void cn_dec(ChatCIRCUIT *circuit, CHATNODE *node);
static CHATNODE *cn_inc(ChatCIRCUIT *circuit, char *call, char *alias, char * Version);
CHATNODE *node_find(char *call);
static CHATNODE *node_inc(char *call, char *alias, char * Version);
static int cn_find(ChatCIRCUIT *circuit, CHATNODE *node);
static void text_xmit(USER *user, USER *to, char *text);
void text_tellu(USER *user, char *text, char *to, int who);
void text_tellu_Joined(USER *user);
static void topic_xmit(USER *user, ChatCIRCUIT *circuit);
static void node_xmit(CHATNODE *node, char kind, ChatCIRCUIT *circuit);
static void node_tell(CHATNODE *node, char kind);
static void user_xmit(USER *user, char kind, ChatCIRCUIT *circuit);
static void user_tell(USER *user, char kind);
USER *user_find(char *call, char * node);
static void user_leave(USER *user);
static BOOL topic_chg(USER *user, char *s);
static USER *user_join(ChatCIRCUIT *circuit, char *ucall, char *ncall, char *nalias, BOOL Local);
void link_drop(ChatCIRCUIT *circuit);
static void echo(ChatCIRCUIT *fc, CHATNODE *node, char * Buffer);
void state_tell(ChatCIRCUIT *circuit, char * Version);
int ct_find(ChatCIRCUIT *circuit, TOPIC *topic);
int rtlink (char * Call);
int rtloginl (ChatCIRCUIT *conn, char * call);
void chkctl(ChatCIRCUIT *ckt_from, char * Buffer, int Len);
int rtloginu (ChatCIRCUIT *circuit, BOOL Local);
void logout(ChatCIRCUIT *circuit);
void show_users(ChatCIRCUIT *circuit);
#ifdef LINBPQ
static VOID __cdecl nprintf(ChatCIRCUIT * conn, const char * format, ...);
static VOID nputs(ChatCIRCUIT * conn, char * buf);
#else
VOID __cdecl nprintf(ChatCIRCUIT * conn, const char * format, ...);
VOID nputs(ChatCIRCUIT * conn, char * buf);
#endif
BOOL matchi(char * p1, char * p2);
char * strlop(char * buf, char delim);
int rt_cmd(ChatCIRCUIT *circuit, char * Buffer);
ChatCIRCUIT *circuit_new(ChatCIRCUIT *circuit, int flags);
void makelinks(void);
VOID * _zalloc(int len);
VOID FreeChatMemory();
VOID ChatTimer();
char * lookupuser(char * call);
VOID node_close();
VOID removelinks();
VOID SetupChat();
int rtlink (char * Call);
VOID SendChatLinkStatus();
VOID ClearChatLinkStatus();
void rduser(USER *user);
void upduser(USER *user);
VOID Send_MON_Datagram(UCHAR * Msg, DWORD Len);

#define Connect(stream) SessionControl(stream,1,0)
#define Disconnect(stream) SessionControl(stream,2,0)
#define ReturntoNode(stream) SessionControl(stream,3,0)
#define ConnectUsingAppl(stream, appl) SessionControl(stream, 0, appl)

BOOL Initialise();
#ifndef LINBPQ
INT_PTR CALLBACK ConfigWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
#endif
int DisplaySessions();
int DoStateChange(int Stream);
int DoReceivedData(int Stream);
int DoMonitorData(int Stream);
int Connected(int Stream);
int Disconnected(int Stream);
//int DeleteConnection(con);
//int Socket_Accept(int SocketId);
//int Socket_Data(int SocketId,int error, int eventcode);
int RefreshMainWindow();
int Terminate();
int WriteLog(char * msg);
int ConnectState(int Stream);
UCHAR * EncodeCall(UCHAR * Call);
int ParseIniFile(char * fn);
VOID SendWelcomeMsg(int Stream, ChatCIRCUIT * conn, struct UserInfo * user);
VOID ProcessLine(ChatCIRCUIT * conn, struct UserInfo * user, char* Buffer, int len);
VOID ProcessChatLine(ChatCIRCUIT * conn, struct UserInfo * user, char* Buffer, int len);
VOID SendPrompt(ChatCIRCUIT * conn, struct UserInfo * user);
int ChatQueueMsg(ChatCIRCUIT * conn, char * msg, int len);
VOID SendUnbuffered(int stream, char * msg, int len);
void WriteLogLine(ChatCIRCUIT * conn, int Flag, char * Msg, int MsgLen, int Flags);

void ChatFlush(ChatCIRCUIT * conn);
VOID ChatClearQueue(ChatCIRCUIT * conn);
void TrytoSend();
int	CriticalErrorHandler(char * error);
void chat_link_out (LINK *link);
int ProcessConnecting(ChatCIRCUIT * circuit, char * Buffer, int Len);
BOOL SaveConfig();
VOID SaveWindowConfig();
VOID __cdecl nodeprintf(ChatCIRCUIT * conn, const char * format, ...);

// Console Routines

BOOL CreateConsole(int Stream);
int WritetoConsoleWindow(int Stream, UCHAR * Msg, int len);
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

struct SEM;

BOOL isdigits(char * string);
void GetSemaphore(struct SEM * Semaphore, int ID);
void FreeSemaphore(struct SEM * Semaphore);

VOID __cdecl Debugprintf(const char * format, ...);
VOID __cdecl Logprintf(int LogMode, ChatCIRCUIT * conn, int InOut, const char * format, ...);
int DeleteLogFiles();
VOID ExpandAndSendMessage(ChatCIRCUIT * conn, char * Msg, int LOG);

extern char Session[];

extern HBRUSH bgBrush;
extern BOOL cfgMinToTray;

extern ChatCIRCUIT * Console;

extern ULONG ChatApplMask;
extern char Verstring[];

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
extern BOOL LogTCP;


extern int LatestMsg;
extern char ChatSYSOPCall[];
extern char ChatSID[];
extern char NewUserPrompt[];


extern int Ver[4];

extern struct SEM AllocSemaphore;
extern struct SEM ConSemaphore;
extern struct SEM MsgNoSemaphore;


extern char hostname[];
extern char RtUsr[];
extern char RtUsrTemp[];
extern char RtKnown[];
extern int AXIPPort;
extern BOOL NeedStatus;

extern LINK *link_hd;
extern ChatCIRCUIT *circuit_hd ;			// This is a chain of RT circuits. There may be others
extern char OurNode[];
extern char OurAlias[];
extern BOOL SMTPMsgCreated;

extern HINSTANCE hInst;
extern HWND hWnd;
extern RECT MainRect;

extern int ChatApplNum;

extern int MaxStreams;
extern UCHAR * OtherNodes;
								// Forward Menu Handle
extern char zeros[];						// For forward bitmask tests
extern char *month[]; 

extern HWND hDebug;
extern RECT MonitorRect;
extern RECT DebugRect;
extern HWND hMonitor;
//extern HWND hConsole;
//extern RECT ConsoleRect;

extern BOOL DeletetoRecycleBin;
extern BOOL SuppressMaintEmail;
extern BOOL SaveRegDuringMaint;
extern BOOL SendWP;
extern BOOL OverrideUnsent;
extern BOOL SendNonDeliveryMsgs;

struct ConsoleInfo * ConsHeader[2];

extern BOOL LogCHAT;