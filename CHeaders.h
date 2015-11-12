//
// Prototypes for BPQ32 Node Functions
//

#define DllImport

#include "compatbits.h"

#include "asmstrucs.h"

Dll int ConvFromAX25(unsigned char * incall,unsigned char * outcall);
Dll BOOL ConvToAX25(unsigned char * callsign, unsigned char * ax25call);
DllExport BOOL ConvToAX25Ex(unsigned char * callsign, unsigned char * ax25call);
int WritetoConsoleLocal(char * buff);
VOID Consoleprintf(const char * format, ...);
VOID FreeConfig();

UINT InitializeExtDriver(PEXTPORTDATA PORTVEC);

VOID PutLengthinBuffer(UCHAR * buff, int datalen);			// Neded for arm5 portability
int GetLengthfromBuffer(UCHAR * buff);	


#define GetBuff() _GetBuff(__FILE__, __LINE__)
#define ReleaseBuffer(s) _ReleaseBuffer(s, __FILE__, __LINE__)

#define Q_REM(s) _Q_REM(s, __FILE__, __LINE__)

#define C_Q_ADD(s, b) _C_Q_ADD(s, b, __FILE__, __LINE__)

VOID * _Q_REM(VOID *Q, char * File, int Line);

int _C_Q_ADD(VOID *Q, VOID *BUFF, char * File, int Line);

UINT _ReleaseBuffer(VOID *BUFF, char * File, int Line);

VOID * _GetBuff(char * File, int Line);

int C_Q_COUNT(VOID *Q);

DllExport char * APIENTRY GetApplCall(int Appl);
DllExport char * APIENTRY GetApplAlias(int Appl);
DllExport int APIENTRY FindFreeStream();
DllExport int APIENTRY DeallocateStream(int stream);
DllExport int APIENTRY SessionState(int stream, int * state, int * change);
DllExport int APIENTRY SetAppl(int stream, int flags, int mask);
DllExport int APIENTRY GetMsg(int stream, char * msg, int * len, int * count );
DllExport int APIENTRY GetConnectionInfo(int stream, char * callsign,
										 int * port, int * sesstype, int * paclen,
										 int * maxframe, int * l4window);


struct config_setting_t;

int GetIntValue(struct config_setting_t * group, char * name);
BOOL GetStringValue(struct config_setting_t * group, char * name, char * value);
VOID SaveIntValue(struct config_setting_t * group, char * name, int value);
VOID SaveStringValue(struct config_setting_t * group, char * name, char * value);

int EncryptPass(char * Pass, char * Encrypt);
VOID DecryptPass(char * Encrypt, unsigned char * Pass, unsigned int len);
Dll VOID APIENTRY CreateOneTimePassword(char * Password, char * KeyPhrase, int TimeOffset);
Dll BOOL APIENTRY CheckOneTimePassword(char * Password, char * KeyPhrase);

DllExport int APIENTRY TXCount(int stream);
DllExport int APIENTRY RXCount(int stream);
DllExport int APIENTRY MONCount(int stream);

VOID ReadNodes();
int BPQTRACE(MESSAGE * Msg, BOOL APRS);

VOID CommandHandler(TRANSPORTENTRY * Session, struct DATAMESSAGE * Buffer);

VOID PostStateChange(TRANSPORTENTRY * Session);

VOID InnerCommandHandler(TRANSPORTENTRY * Session, struct DATAMESSAGE * Buffer);
VOID DoTheCommand(TRANSPORTENTRY * Session);
char * MOVEANDCHECK(TRANSPORTENTRY * Session, char * Bufferptr, char * Source, int Len);
VOID DISPLAYCIRCUIT(TRANSPORTENTRY * L4, char * Buffer);
char * FormatUptime(int Uptime);
char * strlop(char * buf, char delim);
BOOL CompareCalls(UCHAR * c1, UCHAR * c2);

VOID PostDataAvailable(TRANSPORTENTRY * Session);
int WritetoConsoleLocal(char * buff);
char * CHECKBUFFER(TRANSPORTENTRY * Session, char * Bufferptr);
VOID CLOSECURRENTSESSION(TRANSPORTENTRY * Session);

VOID SendCommandReply(TRANSPORTENTRY * Session, struct DATAMESSAGE * Buffer, int Len);

struct PORTCONTROL * APIENTRY GetPortTableEntryFromPortNum(int portnum);

int cCOUNT_AT_L2(struct _LINKTABLE * LINK);
VOID SENDL4CONNECT(TRANSPORTENTRY * Session);

VOID CloseSessionPartner(TRANSPORTENTRY * Session);
int COUNTNODES();
int DecodeNodeName(char * NodeName, char * ptr);;
VOID DISPLAYCIRCUIT(TRANSPORTENTRY * L4, char * Buffer);
int cCOUNT_AT_L2(struct _LINKTABLE * LINK);
void * zalloc(int len);
BOOL FindDestination(UCHAR * Call, struct DEST_LIST ** REQDEST);

char * FormatAPRSMH(APRSSTATIONRECORD * MH);
BOOL ProcessConfig();

VOID PUT_ON_PORT_Q(struct PORTCONTROL * PORT, MESSAGE * Buffer);
VOID CLEAROUTLINK(struct _LINKTABLE * LINK);
VOID TellINP3LinkGone(struct ROUTE * Route);
VOID CLEARACTIVEROUTE(struct ROUTE * ROUTE, int Reason);

// Reason Equates

#define NORMALCLOSE 0
#define RETRIEDOUT 1
#define SETUPFAILED 2
#define LINKLOST 3
#define LINKSTUCK 4

int COUNT_AT_L2(struct _LINKTABLE * LINK);
VOID SENDIDMSG();
VOID SENDBTMSG();
VOID INP3TIMER();
VOID REMOVENODE(dest_list * DEST);
BOOL ACTIVATE_DEST(struct DEST_LIST * DEST);
VOID TellINP3LinkSetupFailed(struct ROUTE * Route);
BOOL FindNeighbour(UCHAR * Call, int Port, struct ROUTE ** REQROUTE);
VOID PROCROUTES(struct DEST_LIST * DEST, struct ROUTE * ROUTE, int Qual);
BOOL L2SETUPCROSSLINK(PROUTE ROUTE);
VOID REMOVENODE(dest_list * DEST);
char * SetupNodeHeader(struct DATAMESSAGE * Buffer);
VOID L4CONNECTFAILED(TRANSPORTENTRY * L4);
int CountFramesQueuedOnSession(TRANSPORTENTRY * Session);
VOID CLEARSESSIONENTRY(TRANSPORTENTRY * Session);
VOID __cdecl Debugprintf(const char * format, ...);

int APIENTRY Restart();
int APIENTRY Reboot();
int APIENTRY Reconfig();
Dll int APIENTRY SaveNodes ();

struct SEM;

void GetSemaphore(struct SEM * Semaphore, int ID);
void FreeSemaphore(struct SEM * Semaphore);

void MySetWindowText(HWND hWnd, char * Msg);

Dll int APIENTRY SessionControl(int stream, int command, int Mask);

HANDLE OpenCOMPort(VOID * pPort, int speed, BOOL SetDTR, BOOL SetRTS, BOOL Quiet, int Stopbits);
int ReadCOMBlock(HANDLE fd, char * Block, int MaxLength);
BOOL WriteCOMBlock(HANDLE fd, char * Block, int BytesToWrite);
VOID CloseCOMPort(HANDLE fd);

#define CMD_TO_APPL	1	// PASS COMMAND TO APPLICATION
#define MSG_TO_USER	2	// SEND 'CONNECTED' TO USER
#define MSG_TO_APPL	4	//	SEND 'CONECTED' TO APPL

#define	UI	3
#define	SABM 0x2F
#define	DISC 0x43
#define	DM	0x0F
#define	UA	0x63
#define	FRMR 0x87
#define	RR	1
#define	RNR	5
#define	REJ	9

#define BPQHOSTSTREAMS	64

extern TRANSPORTENTRY * L4TABLE;
extern unsigned char NEXTID;
extern int MAXCIRCUITS;
extern int L4DEFAULTWINDOW;
extern int L4T1;
extern APPLCALLS APPLCALLTABLE[];
extern char * APPLS;
extern int NEEDMH;
extern int RFOnly;

extern char SESSIONHDDR[];

extern UCHAR NEXTID;

extern struct ROUTE * NEIGHBOURS;
extern int  MAXNEIGHBOURS;

extern struct ROUTE * NEIGHBOURS;
extern int  ROUTE_LEN;
extern int  MAXNEIGHBOURS;

extern struct DEST_LIST * DESTS;				// NODE LIST
extern struct DEST_LIST * ENDDESTLIST;
extern int  DEST_LIST_LEN;
extern int  MAXDESTS;			// MAX NODES IN SYSTEM

extern struct _LINKTABLE * LINKS;
extern int	LINK_TABLE_LEN; 
extern int	MAXLINKS;



extern char	MYCALL[]; //		DB	7 DUP (0)	; NODE CALLSIGN (BIT SHIFTED)
extern char	MYALIASTEXT[]; //	{"      "	; NODE ALIAS (KEEP TOGETHER)

extern UCHAR	MYCALLWITHALIAS[13];
extern APPLCALLS APPLCALLTABLE[NumberofAppls];

extern UCHAR MYNODECALL[];				// NODE CALLSIGN (ASCII)
extern UCHAR MYNETROMCALL[];			// NETROM CALLSIGN (ASCII)

extern UCHAR NETROMCALL[];				// NETORM CALL (AX25)

extern UINT	FREE_Q;

extern struct PORTCONTROL * PORTTABLE;
extern int	NUMBEROFPORTS;


extern int OBSINIT;				// INITIAL OBSOLESCENCE VALUE
extern int OBSMIN;					// MINIMUM TO BROADCAST
extern int L3INTERVAL;			// "NODES" INTERVAL IN MINS
extern int IDINTERVAL;			// "ID" BROADCAST INTERVAL
extern int BTINTERVAL;			// "BT" BROADCAST INTERVAL
extern int MINQUAL;				// MIN QUALITY FOR AUTOUPDATES
extern int HIDENODES;				// N * COMMAND SWITCH
extern int BBSQUAL;				// QUALITY OF BBS RELATIVE TO NODE

extern int NUMBEROFBUFFERS;		// PACKET BUFFERS
extern int PACLEN;				//MAX PACKET SIZE

//	L2 SYSTEM TIMER RUNS AT 3 HZ

extern int T3;				// LINK VALIDATION TIMER (3 MINS) (+ a bit to reduce RR collisions)

extern int L2KILLTIME;		// IDLE LINK TIMER (16 MINS)	
extern int L3LIVES;				// MAX L3 HOPS
extern int L4N2;					// LEVEL 4 RETRY COUNT
extern int L4LIMIT;			// IDLE SESSION LIMIT - 15 MINS
extern int L4DELAY;				// L4 DELAYED ACK TIMER
	
extern int BBS;					// INCLUDE BBS SUPPORT
extern int NODE;					// INCLUDE SWITCH SUPPORT

extern int FULL_CTEXT;				// CTEXT ON ALL CONNECTS IF NZ


// Although externally streams are numbered 1 to 64, internally offsets are 0 - 63

extern BPQVECSTRUC DUMMY;					// Needed to force correct order of following

extern BPQVECSTRUC BPQHOSTVECTOR[BPQHOSTSTREAMS + 5];

extern int NODEORDER;
extern UCHAR LINKEDFLAG;

extern UCHAR UNPROTOCALL[80];


extern char * INFOMSG;
extern int INFOLEN;

extern char * CTEXTMSG;
extern int CTEXTLEN;

extern UCHAR MYALIAS[7];				// ALIAS IN AX25 FORM
extern UCHAR BBSALIAS[7];

extern UINT TRACE_Q;				// TRANSMITTED FRAMES TO BE TRACED

extern char HEADERCHAR;				// CHAR FOR _NODE HEADER MSGS

extern int AUTOSAVE;				// AUTO SAVE NODES ON EXIT FLAG
extern int L4APPL;					// Application for BBSCALL/ALIAS connects
extern int CFLAG;					// C =HOST Command

extern VOID * IDMSG_Q;				// ID/BEACONS WAITING TO BE SENT

extern struct DATAMESSAGE BTHDDR;
extern struct _MESSAGE IDHDDR;

extern VOID * IDMSG;

extern int	L3TIMER;					// TIMER FOR 'NODES' MESSAGE
extern int	IDTIMER;					// TIMER FOR ID MESSAGE
extern int	BTTIMER;					// TIMER FOR BT MESSAGE

extern int STATSTIME;


extern BOOL IPRequired;
extern int MaxHops;
extern int MAXRTT;
extern USHORT CWTABLE[];
extern TRANSPORTENTRY * L4TABLE;
extern UCHAR ROUTEQUAL;
extern UINT BPQMsg;


extern APPLCALLS APPLCALLTABLE[];

extern char VersionStringWithBuild[];
extern char VersionString[];

extern int MAXHEARDENTRIES;
extern int MHLEN;
extern APRSSTATIONRECORD * MHDATA;

extern int APPL1;
extern int PASSCMD;
extern int NUMBEROFCOMMANDS;

extern char * ConfigBuffer;

extern char * RigConfigMsg[];
extern char * WL2KReportLine[];

extern CMDX COMMANDS[];

extern int QCOUNT, MAXBUFFS, MAXCIRCUITS, L4DEFAULTWINDOW, L4T1, CMDXLEN;
extern char CMDALIAS[ALIASLEN][NumberofAppls];

extern int SEMGETS;
extern int SEMRELEASES;
extern int SEMCLASHES;
extern int MINBUFFCOUNT;

extern UCHAR BPQDirectory[];
extern UCHAR BPQProgramDirectory[];

extern char WINMOR[];
extern char PACTORCALL[]; 

extern UCHAR MCOM;
extern UCHAR MUIONLY;
extern UCHAR MTX;
extern ULONG MMASK;

extern UCHAR NODECALL[];			//  NODES in ax.25

extern int L4CONNECTSOUT;
extern int L4CONNECTSIN;
extern int L4FRAMESTX;
extern int L4FRAMESRX;
extern int L4FRAMESRETRIED;
extern int OLDFRAMES;
extern int L3FRAMES;

extern char * PortConfig[];
extern struct SEM Semaphore;
extern UCHAR AuthorisedProgram;			// Local Variable. Set if Program is on secure list

extern int REALTIMETICKS;

// SNMP Variables

int InOctets[32];
int OutOctets[32];
