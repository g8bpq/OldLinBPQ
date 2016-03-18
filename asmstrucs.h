// C equivalents of ASM STRUCS

// Aug 2010 Extend Applmask to 32 bit

#ifndef _ASMSTRUCS_
#define _ASMSTRUCS_

#ifdef WIN32

#define _USE_32BIT_TIME_T	// Until the ASM code switches to 64 bit time
#include "winsock2.h"
#include "ws2tcpip.h"

#endif

#define BPQICON 2

#define BUFFLEN	360

//#define ApplStringLen 48	// Length of each config entry
#define NumberofAppls 32	// Max APPLICATIONS= values
#define ALIASLEN 48
#define MHENTRIES 30		// Entries in MH List

typedef int (FAR *FARPROCY)();

#define NRPID 0xcf			// NETROM PID

#define L4BUSY	0x80		// BNA - DONT SEND ANY MORE
#define L4NAK	0x40		// NEGATIVE RESPONSE FLAG
#define L4MORE	0x20		// MORE DATA FOLLOWS - FRAGMENTATION FLAG

#define L4CREQ	1		// CONNECT REQUEST
#define L4CACK	2		// CONNECT ACK
#define L4DREQ	3		// DISCONNECT REQUEST
#define L4DACK	4		// DISCONNECT ACK
#define L4INFO	5		// INFORMATION
#define L4IACK	6		// INFORMATION ACK


extern char MYCALL[];	// 7 chars, ax.25 format
extern char MYALIASTEXT[];	// 6 chars, not null terminated
extern UCHAR L3RTT[7];	// 7 chars, ax.25 format
extern UCHAR L3KEEP[7];	// 7 chars, ax.25 format
//extern int SENDNETFRAME();
extern struct _DATABASE * DataBase;
//extern APPLCALLS  APPLCALLTABLE[8];

extern int MAXDESTS;

extern VOID * GETBUFF();
extern VOID SETUPNODEHEADER();
extern VOID POSTDATAAVAIL();


extern int DATABASE;
extern int ENDOFDATA;
extern int L3LIVES;
extern int NUMBEROFNODES;

// Length of config Buffer = 100000

#define BridgeMapOffset 70000
#define ApplOffset 80000			// Applications offset in config buffer
#define InfoOffset 85000			// Infomsg offset in  buffer
#define InfoMax	2000				// Max Info
#define HFCTextOffset 88000;		// HF modes CTEXT

#define C_IDMSG	512 
#define C_ROUTES 90000				// Allow 2500
#define C_CTEXT	2048
#define C_PORTS	2560
#define C_INFOMSG 85000

typedef struct _CMDX
{
	char String[12];			// COMMAND STRING
	UCHAR CMDLEN;				// SIGNIFICANT LENGTH
	VOID (* CMDPROC)();			// COMMAND PROCESSOR
	VOID * CMDFLAG;				// FLAG/VALUE ADDRESS
} CMDX;


struct APPLCONFIG
{
	char Command[12];
	char CommandAlias[48];
	char ApplCall[10];
	char ApplAlias[10];
	char L2Alias[10];
	int ApplQual;
};

typedef struct _MHSTRUC
{
	UCHAR MHCALL[7];
	UCHAR MHDIGIS[7][8];
	time_t MHTIME;
	int MHCOUNT;
	BYTE MHDIGI;
	char MHFreq[12];
	char MHLocator[6];
} MHSTRUC, *PMHSTRUC;

//
//	Session Record
//

typedef struct _TRANSPORTENTRY
{
	UCHAR	L4USER[7];				// CALL OF ORIGINATING USER

	union						// POINTER TO TARGET HOST/LINK/DEST/PORT (if Pactor)
	{
		struct PORTCONTROL * PORT;
		struct _LINKTABLE * LINK;	
		struct DEST_LIST * DEST;
		struct _BPQVECSTRUC * HOST;
		struct _EXTPORTDATA * EXTPORT;
	}	L4TARGET;	
		
	UCHAR	L4MYCALL[7];			// CALL WE ARE USING

	UCHAR	CIRCUITINDEX;			// OUR CIRCUIT INFO
	UCHAR	CIRCUITID;			//

	UCHAR	FARINDEX;			//
	UCHAR	FARID;			//	OTHER END'S INFO

	UCHAR	L4WINDOW;			// WINDOW SIZE
	UCHAR	L4WS;				// WINDOW START - NEXT FRAME TO ACK
	UCHAR	TXSEQNO;			//
	UCHAR	RXSEQNO;			// TRANSPORT LEVEL SEQUENCE INFO
	UCHAR	L4LASTACKED;		// LAST SEQUENCE ACKED

	UCHAR	FLAGS;				// TRANSPORT LEVEL FLAGS
	UCHAR	NAKBITS;			// NAK & CHOKE BITS TO BE SENT
	struct _TRANSPORTENTRY * L4CROSSLINK; // POINTER TO LINKED L4 SESSION ENTRY
	UCHAR	L4CIRCUITTYPE;		// BIT SIGNIFICANT - SEE BELOW
	UCHAR	KAMSESSION;			// Session Number on KAM Host Mode TNC
	struct DATAMESSAGE * L4TX_Q;
	struct _L3MESSAGEBUFFER * L4RX_Q;	
	struct DATAMESSAGE * L4HOLD_Q;				// FRAMES WAITING TO BE ACKED
	struct _L3MESSAGEBUFFER * L4RESEQ_Q;		// FRAMES RECEIVED OUT OF SEQUENCE

	UCHAR	L4STATE;
	USHORT	L4TIMER;
	UCHAR	L4ACKREQ;			// DATA ACK NEEDED
	UCHAR	L4RETRIES;			// RETRY COUNTER
	USHORT	L4KILLTIMER;		// IDLE CIRCUIT TIMER 
	USHORT	SESSIONT1;			// TIMEOUT FOR SESSIONS FROM HERE
	UCHAR	SESSPACLEN;			// PACLEN FOR THIS SESSION
	UCHAR	BADCOMMANDS;		// SUCCESSIVE BAD COMMANDS
	UCHAR	STAYFLAG;			// STAY CONNECTED FLAG
	UCHAR	SPYFLAG;			// SPY - CONNECT TO NODE VIA BBS CALLSIGN
		
	UCHAR	RTT_SEQ;			// SEQUENCE NUMBER BEING TIMED
	ULONG	RTT_TIMER;			// TIME ABOVE SEQUENCE WAS SENT

	USHORT	PASSWORD;			// AUTHORISATION CODE FOR REMOTE SYSOP

	UCHAR	SESS_APPLFLAGS;		// APPL FLAGS FOR THIS SESSION

	UCHAR	Secure_Session;		// Set if Host session from BPQTerminal or BPQMailChat

	VOID *	DUMPPTR;			// POINTER FOR REMOTnE DUMP MODE
	VOID *	PARTCMDBUFFER;		//  Save area for incomplete commmand

	int Frequency;				// If known - for CMS Reporting Hz
	char RMSCall[10];
	UCHAR Mode;					// ditto

	int UNPROTO;				// Unproto Mode flag - port number if in unproto mode
	int UAddrLen;				//
	char UADDRESS[64];			// Unproto Address String - Dest + Digis

	int LISTEN;					// Port if in Listen Mode

	char APPL[16];				// Set if session initiated by an APPL
	int L4LIMIT;				// Idle time for this Session

} TRANSPORTENTRY;

//
//	CIRCUITTYPE EQUATES
//

#define L2LINK		1
#define SESSION		2
#define UPLINK		4
#define DOWNLINK	8
#define BPQHOST		32
#define PACTOR		64

typedef struct ROUTE
{
	// Adjacent Nodes

	UCHAR NEIGHBOUR_CALL[7];	// AX25 CALLSIGN	
	UCHAR NEIGHBOUR_DIGI1[7];	// DIGIS EN ROUTE (MAX 2 - ?? REMOVE)
	UCHAR NEIGHBOUR_DIGI2[7];	// DIGIS EN ROUTE (MAX 2 - ?? REMOVE)

	UCHAR NEIGHBOUR_PORT;
	UCHAR NEIGHBOUR_QUAL;
	UCHAR NEIGHBOUR_FLAG;		// SET IF 'LOCKED' ROUTE

	struct _LINKTABLE * NEIGHBOUR_LINK;		// POINTER TO LINK FOR THIS NEIGHBOUR

	USHORT NEIGHBOUR_TIME;		// TIME LAST HEARD (HH MM)

	int NBOUR_IFRAMES;			// FRAMES SENT/RECEIVED
	int NBOUR_RETRIES;			// RETRASMISSIONS

	UCHAR NBOUR_MAXFRAME;		// FOR OPTIMISATION CODE
	UCHAR NBOUR_FRACK;
	UCHAR NBOUR_PACLEN;

	BOOL INP3Node;
	BOOL NoKeepAlive;			// Suppress Keepalive Processing
	int	LastConnectAttempt;		// To stop us trying too often

	int Status;			// 
	int LastRTT;			// Last Value Reported
	int RTT;				// Current	
	int SRTT;				// Smoothed RTT
	int NeighbourSRTT;		// Other End SRTT
//	int RTTIncrement;		// Average of Ours and Neighbours SRTT in 10 ms
	int BCTimer;			// Time to next L3RTT Broadcast
	int Timeout;			// Lost Response Timer
	int Retries;			// Lost Response Count
	struct _L3MESSAGEBUFFER * Msg;	// RIF being built

	int	OtherendsRouteQual;	//	Route quality used by other end.
	int	OtherendLocked;		//	Route quality locked by ROUTES entry.
	int	FirstTimeFlag;		//	Set once quality has been set by direct receive

} *PROUTE;

// Status Equates

#define GotRTTRequest 1		// Other end has sent us a RTT Packet
#define GotRTTResponse 2	// Other end has sent us a RTT Response
#define GotRIF 4			// Other end has sent RIF, so is probably an INP3 Node
							//	(could just be monitoring RTT for some reason
#define SentOurRIF 16		// Set when we have sent a rif for our Call and any ApplCalls
							//  (only sent when we have seen both a request and response)

#pragma pack(1)

typedef struct _L3MESSAGEBUFFER
{
//
//	NETROM LEVEL 3 MESSAGE with Buffer Header 
//
	struct _L3MESSAGEBUFFER * Next;
	UCHAR	Port;
	SHORT	LENGTH;
	UCHAR	L3PID;				// PID

	UCHAR	L3SRCE[7];			// ORIGIN NODE
	UCHAR	L3DEST[7];			// DEST NODE
	UCHAR	L3TTL;				// TX MONITOR FIELD - TO PREVENT MESSAGE GOING
								// ROUND THE NETWORK FOR EVER DUE TO ROUTING LOOP
//
//	NETROM LEVEL 4 DATA
//
	UCHAR	L4INDEX;			// TRANSPORT SESSION INDEX
	UCHAR	L4ID;				// TRANSPORT SESSION ID
	UCHAR	L4TXNO;				// TRANSMIT SEQUENCE NUMBER
	UCHAR	L4RXNO;				// RECEIVE (ACK) SEQ NUMBER
	UCHAR	L4FLAGS;			// FRAGMENTATION, ACK/NAK, FLOW CONTROL AND MSG TYPE BITS

	UCHAR	L4DATA[236]	;		//DATA

} L3MESSAGEBUFFER, *PL3MESSAGEBUFFER;


typedef struct _L3MESSAGE
{
//
//	NETROM LEVEL 3 MESSAGE - WITHOUT L2 INFO 
//
	UCHAR	L3SRCE[7];			// ORIGIN NODE
	UCHAR	L3DEST[7];			// DEST NODE
	UCHAR	L3TTL;				// TX MONITOR FIELD - TO PREVENT MESSAGE GOING								// ROUND THE NETWORK FOR EVER DUE TO ROUTING LOOP
//
//	NETROM LEVEL 4 DATA
//
	UCHAR	L4INDEX;			// TRANSPORT SESSION INDEX
	UCHAR	L4ID;				// TRANSPORT SESSION ID
	UCHAR	L4TXNO;				// TRANSMIT SEQUENCE NUMBER
	UCHAR	L4RXNO;				// RECEIVE (ACK) SEQ NUMBER
	UCHAR	L4FLAGS;			// FRAGMENTATION, ACK/NAK, FLOW CONTROL AND MSG TYPE BITS

	UCHAR	L4DATA[236]	;		//DATA

} L3MESSAGE, *PL3MESSAGE;

#define MSGHDDRLEN 7 //sizeof(VOID *) + sizeof(UCHAR) + sizeof(USHORT)

typedef struct _MESSAGE
{
//	BASIC LINK LEVEL MESSAGE BUFFER LAYOUT

	struct _MESSAGE * CHAIN;

	UCHAR	PORT;
	USHORT	LENGTH;

	UCHAR	DEST[7];
	UCHAR	ORIGIN[7];

//	 MAY BE UP TO 56 BYTES OF DIGIS

	UCHAR	CTL;
	UCHAR	PID; 

	union 
	{                   /*  array named screen */
		UCHAR L2DATA[256];
		struct _L3MESSAGE L3MSG;
	};
		
	UCHAR Padding[BUFFLEN - sizeof(time_t) - sizeof(unsigned short) - sizeof(VOID *) - 256 - MSGHDDRLEN - 16];

	unsigned short Process;
	time_t Timestamp;
	VOID * Linkptr;		// For ACKMODE processing

}MESSAGE, *PMESSAGE;


typedef struct HDDRWITHDIGIS
{
//	BASIC LINK LEVEL MESSAGE BUFFER LAYOUT

	struct _MESSAGE * CHAIN;

	UCHAR	PORT;
	USHORT	LENGTH;

	UCHAR	DEST[7];
	UCHAR	ORIGIN[7];
	UCHAR	DIGIS[8][7];

	UCHAR	CTL;
	UCHAR	PID; 

	union 
	{                   /*  array named screen */
		UCHAR L2DATA[256];
		struct _L3MESSAGE L3MSG;

	};

} DIGIMESSAGE, *PDIGIMESSAGE;


typedef struct DATAMESSAGE
{
//	BASIC LINK LEVEL MESSAGE HEADERLAYOUT

	struct DATAMESSAGE * CHAIN;

	UCHAR	PORT;
	USHORT	LENGTH;

	UCHAR	PID; 

	UCHAR L2DATA[256];

} *PDATAMESSAGE;

//
//	BPQHOST MODE VECTOR STRUC
//

#pragma pack()

typedef struct _BPQVECSTRUC
{
	struct _TRANSPORTENTRY * HOSTSESSION;	// Pointer to Session
	UCHAR	HOSTFLAGS;		// ALLOCATED AND STATE CHANGE FLAGS
	ULONG	HOSTAPPLMASK;
	UCHAR	HOSTAPPLFLAGS;
	UCHAR	HOSTSTREAM;		// STREAM NUMBER
	PMESSAGE	HOSTTRACEQ;
	HWND	HOSTHANDLE;		// HANDLE FOR POSTING MSGS TO
	ULONG	HOSTAPPLNUM;	// Application Number
	ULONG	STREAMOWNER;	//	PID of Process owning stream
	char	PgmName[32];	// Program Name;


}  BPQVECSTRUC, *PBPQVECSTRUC;

typedef struct _APPLCALLS
{

//	Application Calls/Alias Supports multiple L4 application calls

	UCHAR APPLCALL[7];				// ax.25
	char APPLALIAS_TEXT[10];		// TEXT, WITH APPENDED SPACE

	char APPLCALL_TEXT[10];
	UCHAR APPLALIAS[6];
	char Filler;					// So we can use ConvtoAX25 on Alias
	USHORT APPLQUAL;
	struct DEST_LIST * NODEPOINTER;	// Pointer to "NODES" entry for this App (if L4)

	char APPLCMD[13];				//
	BOOL APPLHASALIAS;
	int APPLPORT;					// Port used if APPL has an Alias
	char APPLALIASVAL[48];				// Alias if defined
	UCHAR L2ALIAS[7];				// Additional Alias foe L2 connects

} APPLCALLS;

//
//	We store the Time Received from our neighbour. Before using it we add our current route SRTT
//  This way our times adjust to changes of neighbour SRTT. We can't cater for changes to other hop RTTs,
//	But if these are significant (say 25% or 100 ms) they will be retransmitted

typedef struct NR_DEST_ROUTE_ENTRY
{
	struct ROUTE * ROUT_NEIGHBOUR;	// POINTER TO NEXT NODE IN PATH
	UCHAR ROUT_QUALITY;		// QUALITY
	UCHAR ROUT_OBSCOUNT;
	UCHAR Padding[5];		// SO Entries are the same length
} *PNR_DEST_ROUTE_ENTRY;

typedef struct DEST_ROUTE_ENTRY
{
	struct ROUTE * ROUT_NEIGHBOUR;	// POINTER TO NEXT NODE IN PATH
	USHORT LastRTT;					// Last Value Reported
	USHORT RTT;						// Current	
	USHORT SRTT;					// Smoothed RTT
	UCHAR Hops;
} *PDEST_ROUTE_ENTRY;

typedef struct DEST_LIST
{
	//	L4 Destinations (NODES)

	struct DEST_LIST * DEST_CHAIN;	// SORTED LIST CHAIN

	UCHAR DEST_CALL[7];			// DESTINATION CALLSIGN (AX25 FORMAT)
	UCHAR DEST_ALIAS[6];	

	UCHAR DEST_STATE;			// CONTROL BITS - SETTING UP, ACTIVE ETC	

	UCHAR DEST_ROUTE;			// CURRENTY ACTIVE DESTINATION
	UCHAR INP3FLAGS;

	struct NR_DEST_ROUTE_ENTRY NRROUTE[3];// Best 3 NR neighbours for this dest

	struct DEST_ROUTE_ENTRY ROUTE[3];	// Best 3 INP neighbours for this dest

	long DEST_Q;				// QUEUE OF FRAMES FOR THIS DESTINATION

	int DEST_RTT;				// SMOOTHED ROUND TRIP TIMER
	int DEST_COUNT;				//  FRAMES SENT

} dest_list;

// IMNP3FLAGS Equates

#define NewNode 1			// Just added to table, so need to pass on

struct XDIGI
{
	struct XDIGI * Next;	//  Chain

	UCHAR Call[7];
	UCHAR Alias[7];
	int Port;
	BOOL UIOnly;
};

struct WL2KInfo
{
	struct WL2KInfo * Next;

	char * Host;
	short WL2KPort;

	char RMSCall[10];
	char BaseCall[10];
	char GridSquare[7];
	char Times[80];
	char ServiceCode[17];

	BOOL UseRigCtrlFreqs;
	char WL2KFreq[12];
	char WL2KMode;				// WL2K reporting mode
	char WL2KModeChar;			// W or N
	BOOL DontReportNarrowOnWideFreqs;

//	char NARROWMODE;
//	char WIDEMODE;				// Mode numbers to report to WL2K

//	struct WL2KInfo WL2KInfoList[MAXFREQS];		// Freqs for sending to WL2K

	int Freq;
	char Bandwidth;
//	char * TimeList;		// eg 06-10,12-15
	int mode;              // see below (an integer)
	int baud;              // see below (an integer)
	int power;             // actual power if known, default to 100 for HF, 30 for VHF/UHF (an integer)
	int height;            // antenna height in feet if known, default to 25
	int gain;              // antenna gain if known, default to 0
	int direction;         // primary antenna direction in degrees if known, use 000 for omni (an integer)
	BOOL RPonPTC;			// Set if scanning for Robust Packet on a PTC
};


typedef struct PORTCONTROL
{
	UCHAR PORTCALL[7];
	UCHAR PORTALIAS[7];		//USED FOR UPLINKS ONLY
	char PORTNUMBER;
	char PROTOCOL;	// PORT PROTOCOL
					// 0 = KISS, 2 = NETROM, 4 = BPQKISS
					//; 6 = HDLC, 8 = L2

	struct PORTCONTROL * PORTPOINTER; // NEXT IN CHAIN

	UCHAR PORTQUALITY;		// 'STANDARD' QUALITY FOR THIS PORT
	UINT PORTRX_Q;			// FRAMES RECEIVED ON THIS PORT
	UINT PORTTX_Q;			// FRAMES TO BE SENT ON THIS PORT

	int (FAR * PORTTXROUTINE)();	// POINTER TO TRANSMIT ROUTINE FOR THIS PORT
	int (FAR * PORTRXROUTINE)();	// POINTER TO RECEIVE ROUTINE FOR THIS PORT
	int (FAR * PORTINITCODE)();		// INITIALISATION ROUTINE
	int (FAR * PORTTIMERCODE)();	//
	int (FAR * PORTCLOSECODE)();	// CLOSE ROUTINE
	int (FAR * PORTTXCHECKCODE)();	// OK to TX Check

	char PORTDESCRIPTION[30];// TEXT DESCRIPTION OF FREQ/SPEED ETC

	char PORTBBSFLAG;		// NZ MEANS PORT CALL/ALIAS ARE FOR BBS
	char PORTL3FLAG;			// NZ RESTRICTS OUTGOING L2 CONNECTS
//
//	CWID FIELDS
//

	USHORT CWID[9];			// 8 ELEMENTS + FLAG
	USHORT ELEMENT;			// REMAINING BITS OF CURRENT CHAR
	USHORT * CWPOINTER;		// POINTER TO NEXT CHAR
	USHORT CWIDTIMER;		// TIME TO NEXT ID
	char CWSTATE;			// STATE MACHINE FOR CWID
	char CWTYPE;				// SET TO USE ON/OFF KEYING INSTEAD OF
							// FSK (FOR RUH MODEMS)
	UCHAR PORTMINQUAL;		// MIN QUAL TO BRAOCAST ON THIS PORT

//	STATS COUNTERS

	int L2DIGIED;
	int L2FRAMES;
	int L2FRAMESFORUS;
	int L2FRAMESSENT;
	int L2TIMEOUTS;
	int L2ORUNC;			// OVERRUNS
	int L2URUNC;			// UNDERRUNS
	int L1DISCARD;			// FRAMES DISCARDED (UNABLE TO TX DUE TO DCD)
	int L2FRMRRX;
	int L2FRMRTX;
	int RXERRORS;			// RECEIVE ERRORS
	int L2REJCOUNT;			// REJ FRAMES RECEIVED
	int L2OUTOFSEQ;			// FRAMES RECEIVED OUT OF SEQUENCE
	int L2RESEQ;			// FRAMES RESEQUENCED

	USHORT SENDING;			// LINK STATUS BITS
	USHORT ACTIVE;

	UCHAR AVSENDING;			// LAST MINUTE
	UCHAR AVACTIVE;

	char PORTTYPE;	// H/W TYPE
					// 0 = ASYNC, 2 = PC120, 4 = DRSI
					// 6 = TOSH, 8 = QUAD, 10 = RLC100
					// 12 = RLC400 14 = INTERNAL 16 = EXTERNAL


	USHORT IOBASE;		// CONFIG PARAMS FOR HARDWARE DRIVERS 

	char INTLEVEL;		// NEXT 4 SAME FOR ALL H/W TYPES
	int	BAUDRATE;		// SPEED
	char CHANNELNUM;	// ON MULTICHANNEL H/W
	struct PORTCONTROL * INTCHAIN; // POINTER TO NEXT PORT USING THIS LEVEL
	UCHAR PORTWINDOW;	// L2 WINDOW FOR THIS PORT
	USHORT PORTTXDELAY;	// TX DELAY FOR THIS PORT
	UCHAR PORTPERSISTANCE;	// PERSISTANCE VALUE FOR THIS PORT
	UCHAR FULLDUPLEX;	// FULL DUPLEX IF SET
	UCHAR SOFTDCDFLAG;	// IF SET USE 'SOFT DCD' - IF MODEM CANT GIVE A REAL ONE
	UCHAR PORTSLOTTIME;	// SLOT TIME
	UCHAR PORTTAILTIME;	// TAIL TIME
	UCHAR BBSBANNED;	// SET IF PORT CAN'T ACCEPT L2 CALLS TO BBS CALLSIGN 
	UCHAR PORTT1;		// L2 TIMEOUT
	UCHAR PORTT2;		// L2 DELAYED ACK TIMER
	UCHAR PORTN2;		// RETRIES
	UCHAR PORTPACLEN;	// DEFAULT PACLEN FOR INCOMING SESSIONS

	UINT * PORTINTERRUPT; // ADDRESS OF INTERRUPT HANDLER

	UCHAR QUAL_ADJUST;	// % REDUCTION IN QUALITY IF ON SAME PORT

	char * PERMITTEDCALLS;	// POINTER TO PERMITED CALLS LIST
	char * PORTUNPROTO;		// POINTER TO UI DEST AND DIGI LIST
	UCHAR PORTDISABLED;		// PORT TX DISABLE FLAG
	char DIGIFLAG;			// ENABLE/DISABLE/UI ONLY
	UCHAR DIGIPORT;			// CROSSBAND DIGI PORT
	USHORT DIGIMASK;			// CROSSBAND DIGI MASK
	UCHAR USERS;			// MAX USERS ON PORT
	USHORT KISSFLAGS;		// KISS SPECIAL MODE BITS
	UCHAR PORTINTERLOCK;	// TO DEFINE PORTS WHICH CANT TX AT SAME TIME
	UCHAR NODESPACLEN;		// MAX LENGTH OF 'NODES' MSG 
	UCHAR TXPORT;			// PORT FOR SHARED TX OPERATION
	MHSTRUC * PORTMHEARD;		// POINTER TO MH DATA

	USHORT PARAMTIMER;		// MOVED FROM HW DATA FOR SYSOPH
	UCHAR PORTMAXDIGIS;		// DIGIS ALLOWED ON THIS PORT
	UCHAR PORTALIAS2[7];		// 2ND ALIAS FOR DIGIPEATING FOR APRS
	UCHAR PORTBCALL[7];		// Source call for Beacon
	char PortNoKeepAlive;	// Default to no Keepalives
	char PortUIONLY;		// UI only port - no connects
	char UICAPABLE;			// Pactor-style port that can do UI

	struct WL2KInfo WL2KInfo;	// WL2K Report for this Port
	struct in_addr PORTIPADDR;	// IP address for "KISS over UDP"
	int	ListenPort;				// For KISS over UDP, if Different TX and RX Ports needed
	BOOL KISSTCP;				// TCP instead of UDP for KISS
	BOOL KISSSLAVE;				// TCP KISS is Salve

	char * SerialPortName;		//	Serial Port Name for Unix
	struct XDIGI * XDIGIS;		// Cross port digi setup

	BOOL NormalizeQuality;		// Normalise Node Qualities
	BOOL IgnoreUnlocked;		// Ignore Unlocked routes
	BOOL INP3ONLY;				// Default to INP3 and disallow NODES

	FARPROCY UIHook;			// Used for KISSARQ
	struct PORTCONTROL * HookPort;

}	PORTCONTROLX, *PPORTCONTROL;

typedef struct FULLPORTDATA
{
	struct PORTCONTROL PORTCONTROL;	
	UCHAR HARDWAREDATA[200];			// WORK AREA FOR HARDWARE DRIVERS
} *PFULLPORTDATA;

// KISS Mapping of HARDWAREDATA

typedef struct KISSINFO
{
	struct PORTCONTROL  PORT;	

	int LINKSTS;			// CURRENT STATE
	UINT * CURALP;			// CURRENT BUFFER
	UINT * NEXTCHR;			// 	
	UINT ASYNCMSG_Q;		//  RECEIVED MESSAGES
	UINT KISSTX_Q	;		// MESSAGES TO SEND
	int ESCFLAG	;			// 		; SET IF LAST RX CHAR WAS DLE
	int ESCTXCHAR;			// 	; CHAR TO SEND FOLLOWING DLE IF NZ

	struct KISSINFO * FIRSTPORT;			// 		; FIRST PORT DEFINED FOR THIS IO ADDR
	struct KISSINFO * SUBCHAIN;			// 	; NEXT SUBCHANNEL FOR SAME PHYSICAL PORT

	int OURCTRL;			// 	; CONTROL BYTE FOR THIS PORT

	int XCTRL;			//  CONTROL BYTE TO SEND
	int REALKISSFLAGS;			// 	; KISS FLAGS FOR ACTIVE SUBPORT

	USHORT TXCCC;			// 	; NETROM/BPQKISS CHECKSUMS
	USHORT RXCCC;			// 

	int TXACTIVE;			// TIMER TO DETECT 'HUNG' SENDS

	int POLLFLAG;			// POLL OUTSTANDING FOR MULTIKISS

	struct KISSINFO * POLLPOINTER;			// LAST GROUP POLLED
	int POLLED;					// SET WHEN POLL RECEIVED

//	UCHAR WIN32INFO[16];		//	FOR WINDOWS DRIVER
} *PKISSINFO;

// EXT Driver Mapping of HARDWAREDATA


typedef struct _EXTPORTDATA
{
	struct PORTCONTROL PORTCONTROL	;	// REMAP HARDWARE INFO

	int (FAR * PORT_EXT_ADDR) ();		// ADDR OF RESIDENT ROUTINE
	char PORT_DLL_NAME[16];	
	UCHAR EXTRESTART;					// FLAG FOR DRIVER REINIT
	HINSTANCE DLLhandle;
	int MAXHOSTMODESESSIONS;			// Max Host Sessions supported (Used for KAM Pactor + ax.25 support)
	struct _TRANSPORTENTRY * ATTACHEDSESSIONS[27];	// For PACTOR. etc
	BOOL PERMITGATEWAY;				//  Set if ax.25 ports can change callsign (ie SCS, not KAM
	int SCANCAPABILITIES;			//Type of scan control Controller supports (None, Simple, Connect Lock)
#define NONE 0
#define SIMPLE 1
#define CONLOCK 2

	UINT UI_Q;						// Unproto Frames for Session Mode Drivers (TRK, etc)
	int	FramesQueued;				// TX Frames queued in Driver

} EXTPORTDATA, *PEXTPORTDATA;

typedef struct _HDLCDATA
{
	struct PORTCONTROL PORTCONTROL	;	// REMAP HARDWARE INFO
//
//	Mapping of VXD fields (mainly to simplify debugging
//

	ULONG ASIOC;			// A CHAN ADDRESSES
	ULONG SIO;				// OUR ADDRESSES (COULD BE A OR B) 
	ULONG SIOC;
	ULONG BSIOC;			// B CHAN CONTROL

	struct _HDLCDATA * A_PTR; // PORT ENTRY FOR A CHAN
	struct _HDLCDATA * B_PTR; // PORT ENTRY FOR B CHAN

	VOID (FAR * VECTOR[4]) (); // INTERRUPT VECTORS

//	UINT * IOTXCA;				// INTERRUPT VECTORS
//	UINT * IOTXEA;
//	UINT * IORXCA;
//	UINT * IORXEA;	

	UCHAR LINKSTS;

	UINT * SDRNEXT;
	UINT * SDRXCNT;
	UINT * CURALP;
	UCHAR OLOADS;				// LOCAL COUNT OF BUFFERS SHORTAGES
	USHORT FRAMELEN;
	UINT * SDTNEXT;				// POINTER to NEXT BYTE to TRANSMIT
	USHORT SDTXCNT;				// CHARS LEFT TO SEND
	UCHAR RR0;					// CURRENT RR0
	UINT * TXFRAME;				// ADDRESS OF FRAME BEING SENT

	UCHAR SDFLAGS;				// GENERAL FLAGS

	UINT * PCTX_Q;				// HDLC HOLDING QUEUE
	UINT * RXMSG_Q;				// RX INTERRUPT TO SDLC BG


//;SOFTDCD		DB	0		; RX ACTIVE FLAG FOR 'SOFT DC
	UCHAR TXDELAY;				// TX KEYUP DELAY TIMER
	UCHAR SLOTTIME;				// TIME TO WAIT IF WE DONT SEND
	UCHAR FIRSTCHAR;			// CHAR TO SEND FOLLOWING TXDELAY
	USHORT L1TIMEOUT;			// UNABLE TO TX TIMEOUT
	UCHAR PORTSLOTIMER;

	USHORT TXBRG;				// FOR CARDS WITHOUT /32 DIVIDER
	USHORT RXBRG;	

	UCHAR WR10	;				// NRZ/NRZI FLAG

	int	IRQHand;

	ULONG IOLEN;				// Number of bytes in IO Space

	struct PORTCONTROL * DRIVERPORTTABLE;	// ADDR OF PORT TABLE ENTRY IN VXD
											// Used in NT Driver for Kernel Device Pointer

}HDLCDATA, * PHDLCDATA;


extern struct ROUTE * NEIGHBOURS;
extern int  ROUTE_LEN;
extern int  MAXNEIGHBOURS;

extern struct DEST_LIST * DESTS;				// NODE LIST
extern int  DEST_LIST_LEN;
extern int  MAXDESTS;			// MAX NODES IN SYSTEM

extern struct _LINKTABLE * LINKS;
extern int	LINK_TABLE_LEN; 
extern int	MAXLINKS;
/*
L4TABLE		DD	0
MAXCIRCUITS	DW	50		; NUMBER OF L4 CIRCUITS

NUMBEROFPORTS	DW	0

TNCTABLE	DD	0
NUMBEROFSTREAMS	DW	0

ENDDESTLIST	DD	0		; NODE LIST+1
*/


typedef struct _APRSSTATIONRECORD
{
	UCHAR MHCALL[10];				// Stored with space padding
	time_t MHTIME;					// Time last heard
 	time_t LASTMSG;					// Time last message sent from this station (via IS)
	int Port;						// Port last heard on (zero for APRS-IS)
	BOOL IGate;						// Set if station is an IGate;
//	BYTE MHDIGI[56];				// Not sure if we need this
	struct STATIONRECORD * Station;	// Info previously held by APRS Application

} APRSSTATIONRECORD, *PAPRSSTATIONRECORD;

typedef struct _LINKTABLE
{
//;
//;	LEVEL 2 LINK CONTROL TABLE
//;

	UCHAR	LINKCALL[7];	// CALLSIGN OF STATION
	UCHAR	OURCALL[7];		// CALLSIGN OF OUR END
	UCHAR	DIGIS[56];		// LEVEL 2 DIGIS IN PATH

	PPORTCONTROL	LINKPORT;		// PORT NUMBER
	UCHAR	LINKTYPE;		// 1 = UP, 2= DOWN, 3 = INTERNODE

	UCHAR	LINKNR;
	UCHAR	LINKNS;			// LEV 2 SEQUENCE COUNTS
	UCHAR	LINKWS;			// WINDOW START
	UCHAR	LINKOWS;		// OLD (LAST ACKED) WINDOW START
	UCHAR	LINKWINDOW;		// LEVEL 2 WINDOW SIZE

	UCHAR	L2FLAGS;		// CONTROL BITS
	UCHAR	VER1FLAG;		// SET IF OTHER END RUNNING VERSION 1
  
	VOID *	RX_Q;			// PACKETS RECEIVED ON THIS LINK
	VOID *	TX_Q;			// PACKETS TO SEND
	VOID *	FRAMES[8];		// FRAMES WAITING ACK
	VOID *	RXFRAMES[8];	// Frames received out of sequence

	UCHAR	L2STATE;		// PROCESSING STATE
	USHORT	L2TIMER;		// FRAME RETRY TIMER
	UCHAR	L2TIME;			// RETRY TIMER INITIAL VALUE
	USHORT	L2SLOTIM;		// DELAY FOR LINK VALIDATION POLL
	UCHAR	L2ACKREQ;		// DELAYED TEXT ACK TIMER
	UCHAR	REJTIMER;		// TO TIME OUT REJ  IN VERSION 1
	USHORT	LAST_F_TIME;	// TIME LAST R(F) SENT
	UCHAR	SDREJF;			// FLAGS FOR FRMR
	UCHAR	SDRBYTE;		// SAVED CONTROL BYTE FOR FRMR

	UCHAR	SDTSLOT	;		// POINTER TO NEXT TXSLOT TO USE

	UCHAR	L2RETRIES;		// RETRY COUNTER

	UCHAR	SESSACTIVE;		// SET WHEN WE ARE SURE SESSION IS UP

	USHORT	KILLTIMER;		// TIME TO KILL IDLE LINK

	VOID *	CIRCUITPOINTER;	// POINTER TO L4 CIRCUIT TABLE ENTRY
							// (IF UP/DOWN)
	PROUTE	NEIGHBOUR;		// POINTER TO NEIGHBOUR (IF CROSSLINK)

	VOID *	L2FRAG_Q;		// DEFRAGMENTATION QUEUE

} LINKTABLE;

#pragma pack(1)

struct myin_addr {
        union {
                struct { u_char s_b1,s_b2,s_b3,s_b4; } S_un_b;
                struct { u_short s_w1,s_w2; } S_un_w;
                u_long addr;
        };
};

typedef struct _IPMSG
{
//       FORMAT OF IP HEADER
//
//       NOTE THESE FIELDS ARE STORED HI ORDER BYTE FIRST (NOT NORMAL 8086 FORMAT)

	UCHAR	VERLEN;          // 4 BITS VERSION, 4 BITS LENGTH
	UCHAR	TOS;             // TYPE OF SERVICE
	USHORT	IPLENGTH;        // DATAGRAM LENGTH
	USHORT	IPID;            // IDENTIFICATION
	USHORT	FRAGWORD;        // 3 BITS FLAGS, 13 BITS OFFSET
	UCHAR	IPTTL;
	UCHAR	IPPROTOCOL;      // HIGHER LEVEL PROTOCOL
	USHORT	IPCHECKSUM;      // HEADER CHECKSUM
	struct myin_addr IPSOURCE;
	struct myin_addr IPDEST;

	UCHAR	Data;

} IPMSG, *PIPMSG;

typedef struct _PSEUDOHEADER
{
	struct myin_addr IPSOURCE;
	struct myin_addr IPDEST;
	UCHAR	Reserved;
	UCHAR	IPPROTOCOL;      // HIGHER LEVEL PROTUDP/TCP Length
	USHORT	LENGTH;        // DATAGRAM LENGTH

} PHEADER;

typedef struct _TCPMSG
{

//	FORMAT OF TCP HEADER WITHIN AN IP DATAGRAM

//	NOTE THESE FIELDS ARE STORED HI ORDER BYTE FIRST (NOT NORMAL 8086 FORMAT)

	USHORT	SOURCEPORT;
	USHORT	DESTPORT;

	ULONG	SEQNUM;
	ULONG	ACKNUM;

	UCHAR	TCPCONTROL;			// 4 BITS DATA OFFSET 4 RESERVED
	UCHAR	TCPFLAGS;			// (2 RESERVED) URG ACK PSH RST SYN FIN

	USHORT	WINDOW;
	USHORT	CHECKSUM;
	USHORT	URGPTR;


} TCPMSG, *PTCPMSG;

typedef struct _UDPMSG
{

//	FORMAT OF UDP HEADER WITHIN AN IP DATAGRAM

//	NOTE THESE FIELDS ARE STORED HI ORDER BYTE FIRST (NOT NORMAL 8086 FORMAT)

	USHORT	SOURCEPORT;
	USHORT	DESTPORT;
	USHORT	LENGTH;
	USHORT	CHECKSUM;
	UCHAR	UDPData[0];

} UDPMSG, *PUDPMSG;

//		ICMP MESSAGE STRUCTURE

typedef struct _ICMPMSG
{
	//	FORMAT OF ICMP HEADER WITHIN AN IP DATAGRAM

	//	NOTE THESE FIELDS ARE STORED HI ORDER BYTE FIRST (NOT NORMAL 8086 FORMAT)

	UCHAR	ICMPTYPE;
	UCHAR	ICMPCODE;
	USHORT	ICMPCHECKSUM;

	USHORT	ICMPID;
	USHORT	ICMPSEQUENCE;
	UCHAR	ICMPData[0];

} ICMPMSG, *PICMPMSG;

#pragma pack()

struct SEM
{
	UINT Flag;
	int Clashes;
	int	Gets;
	int Rels;
	DWORD SemProcessID;
	DWORD SemThreadID;
};


#define TNCBUFFLEN 1024
#define MAXSTREAMS 32

// DED Emulator Stream Info

struct StreamInfo
{ 
	UCHAR * Chan_TXQ;		//	!! Leave at front so ASM Code Finds it
							// FRAMES QUEUED TO NODE
    int BPQStream;
	BOOL Connected;					// Set if connected to Node
	int CloseTimer;					// Used to close session after connect failure
	UCHAR MYCall[30];
};


struct TNCDATA
{
	struct TNCDATA * Next;
	unsigned int Mode;				// 0 = TNC2, others may follow

	UCHAR RXBUFFER[TNCBUFFLEN];		// BUFFER TO USER
	UCHAR TXBUFFER[300];			// BUFFER TO NODE

	char PORTNAME[80];				// for Linux Port Names
	int ComPort;
	BOOL VCOM;
	int RTS;
	int CTS;
	int DCD;
	int DTR;
	int DSR;
    int BPQPort;
	int HostPort;
	int Speed;
	char PortLabel[20];
	char TypeFlag[2];
	BOOL PortEnabled;
	HANDLE hDevice;
	BOOL NewVCOM;			// Setif User Mode VCOM Port

	UCHAR VMSR;						// VIRTUAL MSR

//        BIT 7 - Receive Line Signal Detect (DCD)
//            6 - Ring Indicator
//            5 - Data Set Ready
//            4 - Clear To Send
//            3 - Delta  RLSD ( ie state has changed since last
//                                 access)
//            2 - Trailing Edge Ring Detect
//            1 - Delta DSR
//            0 - Delta CTS


	BOOL	RTSFLAG;		// BIT 0 SET IF RTS/DTR UP
	UCHAR	VLSR;			// LAST RECEIVED LSR VALUE

	int RXCOUNT;			// BYTES IN RX BUFFER
	UCHAR * PUTPTR;			// POINTER FOR LOADING BUFFER
	UCHAR * GETPTR;			// POINTER FOR UNLOADING BUFFER

	UCHAR * CURSOR;			// POSTION IN KEYBOARD BUFFER
	struct _TRANSPORTENTRY * KBSESSION;	// POINTER TO L4 SESSION ENTRY FOR CONSOLE

	int	MSGLEN;
	int TRANSTIMER;			// TRANPARENT MODE SEND TIMOUT
	BOOL AUTOSENDFLAG;		// SET WHEN TRANSMODE TIME EXPIRES

	int CMDTMR;				// TRANSARENT MODE ESCAPE TIMER
	int COMCOUNT;			// NUMBER OF COMMAND CHARS RECEIVED
	int CMDTIME ;			// GUARD TIME FOR TRANS MODE EACAPE
	int COMCHAR;			// CHAR TO LEAVE CONV MODE 

	char ECHOFLAG;			// ECHO ENABLED
	BOOL TRACEFLAG;			//  MONITOR ON/OFF
	BOOL FLOWFLAG;			//  FLOW OFF/ON

	BOOL CONOK;
	BOOL CBELL;
	BOOL NOMODE;			//  MODE CHANGE FLAGS
	BOOL NEWMODE;
	BOOL CONMODEFLAG;		//  CONNECT MODE - CONV OR TRANS
	BOOL LFIGNORE;
	BOOL MCON;				//  TRACE MODE FLAGS 
	BOOL MCOM;
	BOOL MALL;
	BOOL AUTOLF;			//  Add LF after CR
	BOOL BBSMON;			//  SPECIAL SHORT MONITOR FOR BBS
	BOOL MTX;				//  MONITOR TRANSMITTED FRAMES
	BOOL MTXFORCE;			//  MONITOR TRANSMITTED FRAMES EVEN IF M OFF
	UINT MMASK;				// MONITOR PORT MASK
	BOOL HEADERLN;			//  PUT MONITORED DATA ON NEW LINE FLAG
	
	BOOL MODEFLAG;			//  COMMAND/DATA MODE

	UINT APPLICATION;		// APPLMASK
	
	UINT APPLFLAGS;		// FLAGS TO CONTROL APPL SYSTEM

	UINT SENDPAC;			//  SEND PACKET CHAR
	BOOL CPACTIME;			// USE PACTIME IN CONV MODE
	BOOL CRFLAG	;			// APPEND SENDPAC FLAG

	int TPACLEN	;			// MAX PACKET SIZE FOR TNC GENERATED PACKETS
	UCHAR UNPROTO[64];		// UNPROTO DEST AND DIGI STRING

	char MYCALL[10];

	// DED Mode Fields

	int PollDelay;			// Used by VCOM to slow down continuous reads on real port 

	struct StreamInfo * Channels[MAXSTREAMS+1];
	char MODE;				// INITIALLY TERMINAL MODE
	char HOSTSTATE;			// HOST STATE MACHINE 
	int MSGCOUNT;			// LENGTH OF MESSAGE EXPECTED
	int MSGLENGTH;
	char MSGTYPE;
	unsigned char MSGCHANNEL;
	char DEDMODE;			// CLUSTER MODE - DONT ALLOW DUP CONNECTS
	int HOSTSTREAMS;		// Default Streams

	UCHAR DEDTXBUFFER[256];
	UCHAR * DEDCURSOR;

	unsigned char MONBUFFER[258]; //="\x6";
	int MONLENGTH;
	int MONFLAG;

	// Kantronics Fields

	int	RXBPtr;	
	char nextMode;			// Mode after RESET

	// SCS Fields

	BOOL Term4Mode;			// Used by Airmail
	BOOL PACMode;			// SCS in Packet Mode
	BOOL Toggle;				// SCS Sequence Toggle

	char MyCall[10];

};

// Emulatiom mode equates

#define TNC2 0
#define DED 1
#define KANTRONICS 2		// For future use
#define SCS 3


#define MAX_ENTRIES 128
#define MaxMHEntries 100
#define MAX_BROADCASTS 8
#define MAXUDPPORTS 30 

#ifndef MAXGETHOSTSTRUCT
#define MAXGETHOSTSTRUCT        1024
#endif


struct arp_table_entry
{
	unsigned char callsign[7];
	unsigned char len;			// bytes to compare (6 or 7)
	BOOL IPv6;

//	union
//	{
//		struct in_addr in_addr;
//		unsigned int ipaddr;
//		struct in6_addr in6_addr;
//	};

	unsigned short port;
	unsigned char hostname[64];
	unsigned int error;
	BOOL ResolveFlag;			// True if need to resolve name
	unsigned int keepalive;
	unsigned int keepaliveinit;
	BOOL BCFlag;				// True if we want broadcasts to got to this call
	BOOL AutoAdded;				// Set if Entry created as a result of AUTOADDMAP
	SOCKET TCPListenSock;			// Listening socket if slave
	SOCKET TCPSock;
	int  TCPMode;				// TCPMaster ot TCPSlave
	UCHAR * TCPBuffer;			// Area for building TCP message from byte stream
	int InputLen;				// Bytes in TCPBuffer

	union
	{
		struct sockaddr_in6 destaddr6;  
		struct sockaddr_in destaddr;
	};

	BOOL TCPState;
	UINT TCPThreadID;			// Thread ID if TCP Master
	UINT TCPOK; 				// Cleared when Message RXed . Incremented by timer
	int SourcePort;				// Used to select socket, hence from port.
//	SOCKET SourceSocket;
	struct AXIPPORTINFO * PORT;
};


struct broadcast_table_entry
{
	unsigned char callsign[7];
	unsigned char len;			// bytes to compare (6 or 7)
};


struct MHTableEntry
{
	unsigned char callsign[7];
	char proto;
	short port;
	union
	{
		struct in_addr ipaddr;
		struct in6_addr ipaddr6;
	};
	time_t	LastHeard;		// Time last packet received
	int Keepalive;
	BOOL IPv6;
};


struct AXIPPORTINFO
{
	int Port;

	struct MHTableEntry MHTable[MaxMHEntries];
	struct broadcast_table_entry BroadcastAddresses[MAX_BROADCASTS];

	int NumberofBroadcastAddreses;
	BOOL Checkifcanreply;
	
	int arp_table_len;
	int ResolveIndex;			// pointer to entry being resolved

	struct arp_table_entry arp_table[MAX_ENTRIES];

	struct arp_table_entry default_arp;

	BOOL MHEnabled;
	BOOL MHAvailable;			// Enabled with config file directive

	BOOL AutoAddARP;
	BOOL AutoAddBC;				// Broadcast flag for autoaddmap

	unsigned char  hostaddr[64];

	HWND hResWnd, hMHWnd, ConfigWnd;

	BOOL GotMsg;

	int udpport[MAXUDPPORTS+2];
	BOOL IPv6[MAXUDPPORTS+2];

	int NumberofUDPPorts;

	BOOL needip;
	BOOL NeedResolver;
	BOOL NeedTCP;

	SOCKET sock;						// IP 93 Sock
	SOCKET udpsock[MAXUDPPORTS+2];

	time_t ltime,lasttime;
	int baseline;
	int mhbaseline;
	int CurrentMHEntries;

	char buf[MAXGETHOSTSTRUCT];

	int MaxMHWindowlength;
	int MaxResWindowlength;

	BOOL ResMinimized;
	BOOL MHMinimized;

	HMENU hResMenu;
	HMENU hMHMenu;

	pthread_t ResolveNamesThreadId;

};


#define Disconnect(stream) SessionControl(stream,2,0)
#define Connect(stream) SessionControl(stream,1,0)

#endif



