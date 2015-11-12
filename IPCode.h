// Header file for BPQ32 IP over ax.25 support

#define BPQREREAD						403
#define BPQADDARP						404

//extern struct PORTCONTROL * PORTTABLE;

#define IDI_ICON2                       123

unsigned long _beginthread( void *, unsigned stack_size, void * arglist);

#pragma pack(1) 

typedef struct _ETHMSG
{
	UCHAR	DEST[6];
	UCHAR	SOURCE[6];
	USHORT	ETYPE;

} ETHMSG, *PETHMSG;

#define ICMP 1

typedef struct _BUFFHEADER
{
//	BASIC LINK LEVEL HEADER BUFFER LAYOUT

	struct _MESSAGE * CHAIN;

	UCHAR	PORT;
	USHORT	LENGTH;

	UCHAR	DEST[7];
	UCHAR	ORIGIN[7];

//	 MAY BE UP TO 56 BYTES OF DIGIS

	UCHAR	CTL;
	UCHAR	PID; 

}BUFFHEADER, *PBUFFHEADER;


typedef struct _AXARP
{
	struct _BUFFHEADER MSGHDDR;
	
	USHORT	HWTYPE;      //    DB      0,3             ; AX.25
	USHORT	PID;			//	DB      0,0CCH          ; PID

	UCHAR	HWADDRLEN;       //      7
	UCHAR	IPADDRLEN;      //      4

	USHORT	ARPOPCODE;       //      200H            ; REQUEST/REPLY

	UCHAR	SENDHWADDR[7];
	ULONG	SENDIPADDR;

	UCHAR	TARGETHWADDR[7];
	ULONG	TARGETIPADDR;

} AXARP, *PAXARP;


typedef struct _ETHARP
{
	struct _ETHMSG MSGHDDR;

	USHORT	HWTYPE;      //    DB      0,1             ; Eth
	USHORT	PID;			//	DB      8,0          ; PID

	UCHAR	HWADDRLEN;       //      6
	UCHAR	IPADDRLEN;      //      4

	USHORT	ARPOPCODE;       //      200H            ; REQUEST/REPLY

	UCHAR	SENDHWADDR[6];
	ULONG	SENDIPADDR;

	UCHAR	TARGETHWADDR[6];
	ULONG	TARGETIPADDR;

} ETHARP, *PETHARP;

typedef struct _RIP2HDDR
{
	UCHAR	Command;
	UCHAR	Version;
	USHORT	Padding;
} RIP2HDDR, *PRIP2HDDR;

typedef struct _RIP2ENTRY
{
	USHORT	AddrFamily;
	USHORT	RouteTAG;
	ULONG	IPAddress;
	ULONG	Mask;
	ULONG	NextHop;
	// Metric Defined as 32 bits, but sent in network order and limited to 16, so just use last byte
	UCHAR	Pad1;
	UCHAR	Pad2;
	UCHAR	Pad3;
	UCHAR	Metric;
} RIP2ENTRY, *PRIP2ENTRY;

#pragma pack()

//       ARP DATA

//       USED TO TRANSLATE IP ADDRESSES TO MAC (Ether or ax.25) ADDDRESSES

typedef struct _ARPDATA
{
//       KEEP IP ADDR AT FRONT

	ULONG	IPADDR;
	UCHAR	HWADDR[64];				// ETHERNET/ax.25 ADDR, maybe with digis
	BOOL	ARPVALID;				// NONZERO IF ADDRESS HAS BEEN RESOLVED
	ULONG	ARPTIMER;
	UCHAR	ARPINTERFACE;			// Port to use. 0= NETROM, 0xff Ethernet
	UCHAR	ARPTYPE;				// NETROM/VC/DG/ETH
	BOOL	LOCKED;					// Locked entry from config file
	struct _MESSAGE * ARP_Q;		// CHAIN OF DATAGRAMS WAITING FOR RESOLUTION
	struct _ROUTEENTRY * ARPROUTE;	// Route Entry for this ARP entry
 
} ARPDATA, *PARPDATA;

typedef struct _ROUTEENTRY
{
	ULONG	NETWORK;	// NETWORK 
	ULONG	SUBNET;		// SUBNET MASK
	ULONG	GATEWAY;	// GATEWAY IP ADDRESS
	ULONG	Encap;		// Encap if a Tunnelled 44 address
	int		FRAMECOUNT; // FRAMES SENT TO THIS NETWORK
	UCHAR	TYPE;		// TYPE (NETROM/VC/DG/ETH)
	UCHAR	METRIC;		// FOR RIP 
	UCHAR	ROUTEINFO;  // TYPE (RIP44, LEARNED, SYSOP Config, ETC)
	UCHAR	ROUTECHANGED;// CHANGED RECENTLY FLAG
	int		RIPTIMOUT;  // FOR REMOVING ACTIVE ENTRIES
	UCHAR	GARTIMOUT;  // FOR REMOVING DEAD ENTRIES
	PARPDATA ARP;		// Pointer to ARP entry if resolved
	BOOL	LOCKED;

} ROUTEENTRY, *PROUTEENTRY;


typedef struct _IPSTATS
{
	int	ARPEntries;
	int FramesForwared;
	int FramesDropped;
	int ARPTimeouts;

} IPSTATS, PIPSTATS;


#define MAX_ENTRIES 128

struct map_table_entry
{
	unsigned int sourceipaddr;
	unsigned short sourceport;
	unsigned int mappedipaddr;
	unsigned short mappedport;
	unsigned char hostname[64];
	unsigned int error;
	BOOL ResolveFlag;			// True if need to resolve name
};

struct nat_table_entry
{
	unsigned int origipaddr;
	unsigned short origport;
	unsigned int mappedipaddr;
	unsigned short mappedport;
	BOOL ThisHost;				//	Needed by LinBPQ to direct host mesgs to TAP
};

struct ipv6_header
{
    unsigned int
        version : 4,
        traffic_class : 8,
        flow_label : 20;
    unsigned short length;
    unsigned char  next_header;
    unsigned char  hop_limit;
    struct in6_addr src;
    struct in6_addr dst;
};

static HANDLE hInstance;

//unsigned long _beginthread( void( *start_address )( void *), unsigned stack_size, char * arglist);

Dll BOOL APIENTRY Init_IP();
Dll BOOL APIENTRY Poll_IP();  
VOID ProcessEthIPMsg(PETHMSG Buffer);
VOID ProcessAXARPMsg(PAXARP arpptr);
VOID ProcessIPMsg(PIPMSG IPptr, UCHAR * MACADDR, char Type, UCHAR Port);
BOOL CheckIPChecksum(PIPMSG IPptr);
VOID ProcessICMPMsg(PIPMSG IPptr);
VOID ProcessSNMPMessage(PIPMSG IPptr);
BOOL RouteIPMsg(PIPMSG IPptr);
VOID SendIPtoBPQDEV(PIPMSG IPptr, UCHAR * HWADDR);
VOID SendIPtoAX25(PIPMSG IPptr, UCHAR * HWADDR, int Port, char Mode);
PARPDATA AllocARPEntry();
PARPDATA LookupARP(ULONG IPADDR, BOOL Add, BOOL * Found);
static BOOL ReadConfigFile();
static int ProcessLine(char * buf);
VOID DoARPTimer();
UINT SENDNETFRAME;
static VOID SendNetFrame(UCHAR * ToCall, UCHAR * FromCall, UCHAR * Block, DWORD Len, UCHAR Port);
VOID ReadARP();
VOID ReadIPRoutes();
BOOL ProcessARPLine(char * buf, BOOL Locked);
void IPResolveNames(void *dummy);
int CheckSumAndSend(PIPMSG IPptr, PTCPMSG TCPmsg, USHORT Len);
int CheckSumAndSendUDP(PIPMSG IPptr, PUDPMSG UDPmsg, USHORT Len);
VOID SaveARP();
VOID SaveIPRoutes();
VOID ReadIPRoutes();
VOID WriteARPLine(PARPDATA ArpRecord, FILE * file);
VOID SendIPtoEther(PIPMSG IPptr, UCHAR * HWADDR, BOOL SendtoTAP);

int InitPCAP(void);
int OpenPCAP(void);