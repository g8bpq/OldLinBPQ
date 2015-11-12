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


// Module to provide a basic Gateway between IP over AX.25 and the Internet.

// Uses WinPcap on Windows, TAP Driver on Linux

// Basically operates as a mac level bridge, with headers converted between ax.25 and Ethernet.
// ARP frames are also reformatted, and monitored to build a simple routing table.
// Apps may not use ARP (MSYS is configured with an ax.25 default route, rather than an IP one),
// so the default route must be configured. 

// Intended as a gateway for legacy apps, rather than a full function ip over ax.25 router.
// Suggested config is to use the Internet Ethernet Adapter, behind a NAT/PAT Router.
// The ax.25 applications will appear as single addresses on the Ethernet LAN

// The code can also switch packets between ax.25 interfaces

// First Version, July 2008

// Version 1.2.1 January 2009

//	Add IP Address Mapping option

//	June 2014. Convert to Router instead of MAC Bridge, and include a RIP44 decoder
//	so packets can be routed from RF to/from encapsulated 44 net subnets.
//	Routes may also be learned from received RF packets, or added from config file

/*
TODo	?Multiple Adapters
*/


// ip tuntap add dev bpqtap mode tap
// ifconfig bpqtap 44.131.4.19 mtu 256 up



#pragma data_seg("_BPQDATA")

#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <stdio.h>
#include <time.h>

#include "CHeaders.h"

#include "IPCode.h"

#ifdef WIN32
#include "pcap.h"
#endif

#ifndef LINBPQ
#include "kernelresource.h"
LRESULT CALLBACK ResWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
#endif

//#define s_addr  S_un.S_addr

extern BPQVECSTRUC * IPHOSTVECTORPTR;

BOOL APIENTRY  Send_AX(PMESSAGE Block, DWORD Len, UCHAR Port);
VOID SENDSABM(struct _LINKTABLE * LINK);
BOOL FindLink(UCHAR * LinkCall, UCHAR * OurCall, int Port, struct _LINKTABLE ** REQLINK);
BOOL ProcessConfig();
VOID RemoveARP(PARPDATA Arp);

VOID ProcessTunnelMsg(PIPMSG IPptr);
VOID ProcessRIP44Message(PIPMSG IPptr);
PROUTEENTRY LookupRoute(ULONG IPADDR, ULONG Mask, BOOL Add, BOOL * Found);
BOOL ProcessROUTELine(char * buf, BOOL Locked);
VOID DoRouteTimer();
PROUTEENTRY FindRoute(ULONG IPADDR);
VOID SendIPtoEncap(PIPMSG IPptr, ULONG Encap);
USHORT Generate_CHECKSUM(VOID * ptr1, int Len);

static VOID MapRouteIPMsg(PIPMSG IPptr);
BOOL Check_Checksum(VOID * ptr1, int Len);

static BOOL Send_ETH(VOID * Block, DWORD len);

VOID ProcessEthARPMsg(PETHARP arpptr);
static VOID SendARPMsg(PARPDATA Arp);

#define ARPTIMEOUT 3600


//		ARP REQUEST/REPLY (Eth)

static ETHARP ETHARPREQMSG = {0};

static ARPDATA ** ARPRecords = NULL;				// ARP Table - malloc'ed as needed

static int NumberofARPEntries = 0;

static ROUTEENTRY ** RouteRecords = NULL;

static int NumberofRoutes = 0;

//HANDLE hBPQNET = INVALID_HANDLE_VALUE;

static ULONG OurIPAddr = 0;

static ULONG OurIPBroadcast = 0;
static ULONG OurNetMask = 0xffffffff;

static BOOL WantTAP = FALSE;
static BOOL WantEncap = 0;			// Run RIP44 and Net44 Encap

static int IPTTL = 128;

static int FramesForwarded = 0;
static int FramesDropped = 0;
static int ARPTimeouts = 0;
static int SecTimer = 10;

static BOOL NeedResolver = FALSE;

static HMENU hMenu;
extern HMENU hWndMenu;
static HMENU hPopMenu;

extern HKEY REGTREE;

extern int OffsetH, OffsetW;

extern HMENU hMainFrameMenu, hBaseMenu;
extern HWND ClientWnd, FrameWnd;

static int map_table_len = 0;
//int index=0;					// pointer for table search
static int ResolveIndex=-1;			// pointer to entry being resolved

static struct map_table_entry map_table[MAX_ENTRIES];

static int Windowlength, WindowParam;


static time_t ltime,lasttime;

static char ConfigClassName[]="CONFIG";

HWND hIPResWnd = 0;

BOOL IPMinimized;

extern char * PortConfig[];

static int baseline=0;

static unsigned char  hostaddr[64];


// Following two fields used by stats to get round shared memmory problem

static ARPDATA Arp={0};
static int ARPFlag = -1;

// Following Buffer is used for msgs from WinPcap. Put the Enet message part way down the buffer, 
//	so there is room for ax.25 header instead of Enet header when we route the frame to ax.25
//	Enet Header ia 14 bytes, AX.25 UI is 16

// Also used to reassemble NOS Fragmented ax.25 packets

static UCHAR Buffer[4096] = {0};

#define EthOffset 30				// Should be plenty

static DWORD IPLen = 0;


#ifdef WIN32
static UCHAR ourMACAddr[6] = {02,'B','P','Q',3,48};
#else
UCHAR ourMACAddr[6] = {02,'B','P','Q',0,1};
#endif

static LONG DefaultIPAddr = 0;

static IPSTATS IPStats = {0};

static UCHAR BPQDirectory[260];

static char ARPFN[MAX_PATH];

static HANDLE handle;

#ifdef WIN32
static pcap_t *adhandle = 0;
static pcap_t * (FAR * pcap_open_livex)(const char *, int, int, int, char *);

static int pcap_reopen_delay;
#endif

static char Adapter[256];

static int Promiscuous = 1;			// Default to Promiscuous

#ifdef WIN32

static HINSTANCE PcapDriver=0;

typedef int (FAR *FARPROCX)();

static int (FAR * pcap_sendpacketx)();

static FARPROCX pcap_compilex;
static FARPROCX pcap_setfilterx;
static FARPROCX pcap_datalinkx;
static FARPROCX pcap_next_exx;
static FARPROCX pcap_geterrx;


static char Dllname[6]="wpcap";

FARPROCX GetAddress(char * Proc);

#else

#define pcap_compilex pcap_compile
#define pcap_open_livex pcap_open_live
#define pcap_setfilterx pcap_setfilter
#define pcap_datalinkx pcap_datalink
#define pcap_next_exx pcap_next_ex
#define pcap_geterrx pcap_geterr
#define pcap_sendpacketx pcap_sendpacket
#endif
VOID __cdecl Debugprintf(const char * format, ...);

static HANDLE hInstance;




void OpenTAP();

Dll BOOL APIENTRY Init_PM()
{
	ARPDATA * ARPptr;

	if (hIPResWnd)
	{
		PostMessage(hIPResWnd, WM_CLOSE,0,0);
//		DestroyWindow(hIPResWnd);

		Debugprintf("IP Init Destroying IP Resolver");
	}
	
	hIPResWnd= NULL;

	ARPRecords = NULL;				// ARP Table - malloc'ed as needed
	NumberofARPEntries=0;

	RouteRecords = NULL;
	NumberofRoutes = 0;

	ReadConfigFile();
	
	// Clear old packets

	memset(ETHARPREQMSG.MSGHDDR.DEST, 255, 6);
	memcpy(ETHARPREQMSG.MSGHDDR.SOURCE, ourMACAddr, 6);
	ETHARPREQMSG.MSGHDDR.ETYPE = 0x0608;			// ARP

	ETHARPREQMSG.HWTYPE=0x0100;				//	Eth
	ETHARPREQMSG.PID=0x0008;	
	ETHARPREQMSG.HWADDRLEN = 6;
	ETHARPREQMSG.IPADDRLEN = 4;

#ifdef WIN32

    //
    // Open PCAP Driver

	if (Adapter[0])					// Don't have to have ethernet, if used just as ip over ax.25 switch 
	{
		char buf[80];

		if (OpenPCAP())
			sprintf(buf,"Portmapper Using %s\n", Adapter);
		else
			sprintf(buf," Portmapper Unable to open %s\n", Adapter);
	
		WritetoConsoleLocal(buf);

		if (adhandle == NULL)
		{
			WritetoConsoleLocal("Failed to open pcap device - Portmapper Disabled\n");
			return FALSE;
		} 

		// Allocate ARP Entry for Default Gateway, and send ARP for it

		if (DefaultIPAddr)
		{
			ARPptr = AllocARPEntry();

			if (ARPptr != NULL)
			{
				ARPptr->ARPINTERFACE = 255;
				ARPptr->ARPTYPE = 'E';
				ARPptr->IPADDR = DefaultIPAddr;
				ARPptr->LOCKED = TRUE;

				SendARPMsg(ARPptr);
			}
		}
	}

#else

	// Linux - if TAP requested, open it
#ifndef MACBPQ

	if (WantTAP)
		OpenTAP();

#endif
#endif


#ifndef LINBPQ

	if (NeedResolver)
	{
		WNDCLASS  wc;
		int i;
		char WindowTitle[100];
		int retCode, Type, Vallen;
		HKEY hKey;
		char Size[80];
		RECT Rect = {0,0,0,0};

		retCode = RegOpenKeyEx (REGTREE, "SOFTWARE\\G8BPQ\\BPQ32", 0, KEY_QUERY_VALUE, &hKey);

		if (retCode == ERROR_SUCCESS)
		{
			Vallen=80;

			retCode = RegQueryValueEx(hKey,"IPResSize",0,			
				(ULONG *)&Type,(UCHAR *)&Size,(ULONG *)&Vallen);

			if (retCode == ERROR_SUCCESS)
				sscanf(Size,"%d,%d,%d,%d,%d",&Rect.left,&Rect.right,&Rect.top,&Rect.bottom, &IPMinimized);

			if (Rect.top < - 500 || Rect.left < - 500)
			{
				Rect.left = 0;
				Rect.top = 0;
				Rect.right = 600;
				Rect.bottom = 400;
			}

			RegCloseKey(hKey);
		}

		// Fill in window class structure with parameters that describe
		// the main window.

        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE;
        wc.lpfnWndProc   = (WNDPROC)ResWndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hInstance;
        wc.hIcon         = LoadIcon (hInstance, MAKEINTRESOURCE(BPQICON));
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
		wc.lpszMenuName =  NULL ;
        wc.lpszClassName = "IPAppName";

        // Register the window classes

		RegisterClass(&wc);

		i=GetLastError();
 
		Windowlength=(map_table_len)*14+100;
		WindowParam=WS_OVERLAPPEDWINDOW | WS_VSCROLL;

		sprintf(WindowTitle,"PortM Resolver");

		hIPResWnd = CreateMDIWindow("IPAppName", WindowTitle, WindowParam,
			  Rect.left - (OffsetW /2), Rect.top - OffsetH, Rect.right - Rect.left, Rect.bottom - Rect.top,
			  ClientWnd, hInstance, 1234);

		hPopMenu = CreatePopupMenu();
		AppendMenu(hPopMenu, MF_STRING, BPQREREAD, "ReRead Config");

		SetScrollRange(hIPResWnd,SB_VERT,0, map_table_len,TRUE);

		if (IPMinimized)
			ShowWindow(hIPResWnd, SW_SHOWMINIMIZED);
		else
			ShowWindow(hIPResWnd, SW_RESTORE);

		_beginthread(IPResolveNames, 0, NULL );
	}
#endif

	WritetoConsoleLocal("Portmapper Enabled\n");

	return TRUE;

}

VOID PMClose()
{
}

union
{
	struct sockaddr_in rxaddr;
	struct sockaddr_in6 rxaddr6;
} RXaddr;


Dll BOOL APIENTRY Poll_PM()
{
	int res;
	struct pcap_pkthdr *header;
	const u_char *pkt_data;

	// Entered every 100 mS
	
	// if ARPFlag set, copy requested ARP record (For BPQStatus GUI)

	if (ARPFlag != -1)
	{
		memcpy(&Arp, ARPRecords[ARPFlag], sizeof (ARPDATA));
		ARPFlag = -1;
	}

	SecTimer--;

	if (SecTimer == 0)
	{
		SecTimer = 10;
		DoARPTimer();
		DoRouteTimer();
	}

Pollloop:

#ifdef WIN32

	if (adhandle)
	{
		res = pcap_next_exx(adhandle, &header, &pkt_data);

		if (res > 0)
		{
			PETHMSG ethptr = (PETHMSG)&Buffer[EthOffset];

			if (header->len > 1514)
			{
//				Debugprintf("Ether Packet Len = %d", header->len);
				goto Pollloop;
			}

			memcpy(&Buffer[EthOffset],pkt_data, header->len);

			if (ethptr->ETYPE == 0x0008)
			{
				ProcessEthIPMsg((PETHMSG)&Buffer[EthOffset]);
			//	PIPMSG ipptr = (PIPMSG)&Buffer[EthOffset+14];
			//	ProcessIPMsg(ipptr, ethptr->SOURCE, 'E', 255);
				goto Pollloop;
			}

			if (ethptr->ETYPE == 0x0608)
			{
				ProcessEthARPMsg((PETHARP)ethptr);
				goto Pollloop;
			}

			// Ignore anything else

			goto Pollloop;
		}
		else
		{
			if (res < 0)
			{
				char * error  = pcap_geterrx(adhandle) ;
				Debugprintf(error);
				if (OpenPCAP() == FALSE)
					pcap_reopen_delay = 300;
			}
		}
	}
	else
	{
		// No handle. 
		
		if (Adapter[0])					// Don't have to have ethernet, if used just as ip over ax.25 switch 
		{
			// Try reopening periodically
			
			pcap_reopen_delay --;
			
			if (pcap_reopen_delay < 0)
				if (OpenPCAP() == FALSE)
					pcap_reopen_delay = 300;	// Retry every 30 seconds
		}
	}

#endif


	return TRUE;
}


static BOOL Send_ETH(VOID * Block, DWORD len)
{
#ifdef WIN32
	if (adhandle)
	{
//		if (len < 60) len = 60;

		// Send down the packet 

		pcap_sendpacketx(adhandle,	// Adapter
			Block,				// buffer with the packet
			len);				// size
	}
#endif			
    return TRUE;
}


static VOID SendIPtoBPQDEV(PIPMSG IPptr, UCHAR * HWADDR)
{	
	// AX.25 headers are bigger, so there will always be room in buffer for enet header
	
	PETHMSG Ethptr = (PETHMSG)IPptr;
	int Len;

	(UCHAR *)Ethptr--;

	Len = ntohs(IPptr->IPLENGTH);

	Len+=14;			// Add eth Header

	memcpy(Ethptr->DEST, HWADDR, 6);
	memcpy(Ethptr->SOURCE, ourMACAddr, 6);
	Ethptr->ETYPE= 0x0008;

	Send_ETH(Ethptr,Len);

	return;
}

static VOID ProcessEthIPMsg(PETHMSG Buffer)

{
	PIPMSG ipptr = (PIPMSG)&Buffer[1];

	if (memcmp(Buffer, ourMACAddr,6 ) != 0) 
		return;		// Not for us

	if (memcmp(&Buffer[6], ourMACAddr,6 ) == 0) 
		return;		// Discard our sends

	// if Checkum offload is active we get the packet before the NIC sees it (from PCAP)

	if (ipptr->IPCHECKSUM == 0)				// Windows seems to do this
	{
		// Generate IP and TCP/UDP checksums

		int Len = ntohs(ipptr->IPLENGTH);
		Len-=20;

		ipptr->IPCHECKSUM = Generate_CHECKSUM(ipptr, 20);

		if (ipptr->IPPROTOCOL == 6)		// TCP	
		{
			PTCPMSG TCP = (PTCPMSG)&ipptr->Data;
			PHEADER PH = {0};	
	
			PH.IPPROTOCOL = 6;
			PH.LENGTH = htons(Len);
			memcpy(&PH.IPSOURCE, &ipptr->IPSOURCE, 4);
			memcpy(&PH.IPDEST, &ipptr->IPDEST, 4);

			TCP->CHECKSUM = ~Generate_CHECKSUM(&PH, 12);
			TCP->CHECKSUM = Generate_CHECKSUM(TCP, Len);
		}
	}
	ProcessIPMsg(ipptr, Buffer->SOURCE, 'E', 255);
}

static VOID ProcessEthARPMsg(PETHARP arpptr)
{
	int i=0;
	PARPDATA Arp;
	BOOL Found;

	if (memcmp(&arpptr->MSGHDDR.SOURCE, ourMACAddr,6 ) == 0 ) 
		return;		// Discard our sends

	switch (arpptr->ARPOPCODE)
	{
	case 0x0100:

		// We should only accept requests from our subnet - we might have more than one net on iterface

		if ((arpptr->SENDIPADDR & OurNetMask) != (OurIPAddr & OurNetMask))
			return;

		if (arpptr->TARGETIPADDR == 0)		// Request for 0.0.0.0
			return;
	
		// Add to our table, as we will almost certainly want to send back to it
		
		Arp = LookupARP(arpptr->SENDIPADDR, TRUE, &Found);

		if (Found)
			goto AlreadyThere;				// Already there

		if (Arp == NULL) return;				// No point of table full
				
		Arp->IPADDR = arpptr->SENDIPADDR;
		Arp->ARPTYPE = 'E';
		Arp->ARPINTERFACE = 255;
		Arp->ARPTIMER =  ARPTIMEOUT;

		SaveARP();
	
AlreadyThere:

		memcpy(Arp->HWADDR, arpptr->SENDHWADDR ,6);
		Arp->ARPVALID = TRUE;

		if (arpptr->TARGETIPADDR == OurIPAddr)
		{
			ULONG Save = arpptr->TARGETIPADDR;
 
			arpptr->ARPOPCODE = 0x0200;
			memcpy(arpptr->TARGETHWADDR, arpptr->SENDHWADDR ,6);
			memcpy(arpptr->SENDHWADDR, ourMACAddr ,6);

			arpptr->TARGETIPADDR = arpptr->SENDIPADDR;
			arpptr->SENDIPADDR = Save;

			memcpy(arpptr->MSGHDDR.DEST, arpptr->MSGHDDR.SOURCE ,6); 
			memcpy(arpptr->MSGHDDR.SOURCE, ourMACAddr ,6); 

			Send_ETH(arpptr,42);

			return;

		}

	break;

	
	case 0x0200:

		if (memcmp(&arpptr->MSGHDDR.DEST, ourMACAddr,6 ) != 0 ) 
			return;		// Not for us

		// Update ARP Cache

		Arp = LookupARP(arpptr->SENDIPADDR, TRUE, &Found);

		if (Found)
			goto Update;

		if (Arp == NULL)
			goto SendBack;

Update:
		Arp->IPADDR = arpptr->SENDIPADDR;

		memcpy(Arp->HWADDR, arpptr->SENDHWADDR ,6);
		Arp->ARPTYPE = 'E';
		Arp->ARPINTERFACE = 255;
		Arp->ARPVALID = TRUE;
		Arp->ARPTIMER =  ARPTIMEOUT;
		SaveARP();

SendBack:
		
		//  Send Back to Originator of ARP Request

		if (arpptr->TARGETIPADDR == OurIPAddr)		// Reply to our request?
			break;

	default:
		break;
	}
	return;
}

static int CheckSumAndSend(PIPMSG IPptr, PTCPMSG TCPmsg, USHORT Len)
{
	struct _IPMSG PH = {0};	
	IPptr->IPCHECKSUM = 0;

	PH.IPPROTOCOL = 6;
	PH.IPLENGTH = htons(Len);
	memcpy(&PH.IPSOURCE, &IPptr->IPSOURCE, 4);
	memcpy(&PH.IPDEST, &IPptr->IPDEST, 4);

	TCPmsg->CHECKSUM = ~Generate_CHECKSUM(&PH, 20);
	TCPmsg->CHECKSUM = Generate_CHECKSUM(TCPmsg, Len);

	// No need to do IP checksum as RouteIPMessage doesit
	
//	CHECKSUM IT

//	IPptr->IPCHECKSUM = Generate_CHECKSUM(IPptr, 20);

	MapRouteIPMsg(IPptr);
	return 0;
}


static VOID ProcessIPMsg(PIPMSG IPptr, UCHAR * MACADDR, char Type, UCHAR Port)
{
	ULONG Dest;
	PARPDATA Arp;
	BOOL Found;
	int index, Len;
	PTCPMSG TCPptr;
	PUDPMSG UDPptr;


	if (IPptr->VERLEN != 0x45) return;  // Only support Type = 4, Len = 20

	if (!CheckIPChecksum(IPptr)) return;

	// Make sure origin ia in ARP Table

	Arp = LookupARP(IPptr->IPSOURCE.addr, TRUE, &Found);

	if (!Found)
	{
		// Add if possible

		if (Arp != NULL)
		{
			Arp->IPADDR = IPptr->IPSOURCE.addr;

			if (Type == 'E')
			{
				memcpy(Arp->HWADDR, MACADDR, 6);
			}
			else
			{
				memcpy(Arp->HWADDR, MACADDR, 7);
				Arp->HWADDR[6] &= 0x7e;
			}
			Arp->ARPTYPE = Type;
			Arp->ARPINTERFACE = Port;
			Arp->ARPVALID = TRUE;
			Arp->ARPTIMER =  ARPTIMEOUT;

			SaveARP();
		}
	}
	else
		Arp->ARPTIMER =  ARPTIMEOUT;				// Refresh

	// See if for us - if not pass to router

	Dest = IPptr->IPDEST.addr;

	if (Dest == OurIPAddr || Dest == 0xffffffff || Dest == OurIPBroadcast)
		goto ForUs;

	return;

ForUs:

//	if (IPptr->IPPROTOCOL == 4)		// AMPRNET Tunnelled Packet
//	{
//		ProcessTunnelMsg(IPptr);
//		return;
//	}

	if (IPptr->IPPROTOCOL == 1)		// ICMP
	{
		ProcessICMPMsg(IPptr);
		return;
	}

	// Support UDP for SNMP

	if (IPptr->IPPROTOCOL == 17)		// UDP
	{
		UDPptr = (PUDPMSG)&IPptr->Data;

		if (UDPptr->DESTPORT == htons(161))
		{
			ProcessSNMPMessage(IPptr);
			return;
		}
	}

	// See if for a mapped Address

	if (IPptr->IPPROTOCOL != 6) return; // Only TCP

	TCPptr = (PTCPMSG)&IPptr->Data;

	Len = ntohs(IPptr->IPLENGTH);
	Len-=20;

	for (index=0; index < map_table_len; index++)
	{
		if ((map_table[index].sourceport == TCPptr->DESTPORT) &&
			map_table[index].sourceipaddr == IPptr->IPSOURCE.addr)
		{
			//	Outgoing Message - replace Dest IP address and Port. Source Port remains unchanged

			IPptr->IPSOURCE.addr = OurIPAddr;
			IPptr->IPDEST.addr = map_table[index].mappedipaddr;
			TCPptr->DESTPORT = map_table[index].mappedport;
			CheckSumAndSend(IPptr, TCPptr, Len);
			return;
		}

		if ((map_table[index].mappedport == TCPptr->SOURCEPORT) &&
			map_table[index].mappedipaddr == IPptr->IPSOURCE.addr)
		{
			//	Incomming Message - replace Dest IP address and Source Port

			IPptr->IPSOURCE.addr = OurIPAddr;
			IPptr->IPDEST.addr = map_table[index].sourceipaddr;
			TCPptr->SOURCEPORT = map_table[index].sourceport;
			CheckSumAndSend(IPptr, TCPptr, Len);
			return;
		}
	}
}

static VOID ProcessICMPMsg(PIPMSG IPptr)
{
	int Len;
	PICMPMSG ICMPptr = (PICMPMSG)&IPptr->Data;

	Len = ntohs(IPptr->IPLENGTH);
	Len-=20;

	Check_Checksum(ICMPptr, Len);

	if (ICMPptr->ICMPTYPE == 8)
	{
		//	ICMP_ECHO

		ICMPptr->ICMPTYPE = 0;		// Convert to Reply

		// CHECKSUM IT

		ICMPptr->ICMPCHECKSUM = 0;
		ICMPptr->ICMPCHECKSUM = Generate_CHECKSUM(ICMPptr, Len);

		// Swap Dest to Origin

		IPptr->IPDEST = IPptr->IPSOURCE;
		IPptr->IPSOURCE.addr = OurIPAddr;
		IPptr->IPTTL = IPTTL;

//		IPptr->IPCHECKSUM = 0;
//		IPptr->IPCHECKSUM = Generate_CHECKSUM(IPptr, 20);		// RouteIPMsg redoes checksum

		MapRouteIPMsg(IPptr);			// Send Back
	}

	if (ICMPptr->ICMPTYPE == 0)
	{
		//	ICMP_REPLY:

		UCHAR * BUFFER = GetBuff();
		UCHAR * ptr1;
		struct _MESSAGE * Msg;
		TRANSPORTENTRY * Session = L4TABLE;
		char IP[20];
		unsigned char work[4];

		Session += ICMPptr->ICMPID;

		if (BUFFER == NULL)
			return;

		ptr1 = &BUFFER[7];

		memcpy(work, &IPptr->IPSOURCE, 4);
		sprintf(IP, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

		*ptr1++ = 0xf0;			// PID

		ptr1 += sprintf(ptr1, "Ping Response from %s", IP);

		*ptr1++ = 0x0d;			// CR

		Len = ptr1 - BUFFER;

		Msg = (struct _MESSAGE *)BUFFER;
		Msg->LENGTH = Len;
		Msg->CHAIN = NULL;

		C_Q_ADD(&Session->L4TX_Q, (UINT *)BUFFER);

		PostDataAvailable(Session);

		return;
	}
}


static VOID SendICMPMessage(PIPMSG IPptr, int Type, int Code, int P2)
{
	PICMPMSG ICMPptr = (PICMPMSG)&IPptr->Data;
	UCHAR * ptr;

	if (OurIPAddr == 0)
		return;					// Can't do much without one

	if (IPptr->IPPROTOCOL == ICMP && ICMPptr->ICMPTYPE == 11)
		return;					// Don't send Time Exceeded for TimeExceded

	// Copy the Original IP Header and first 8 bytes of packet down the buffer

	ptr = (UCHAR *) ICMPptr;

	memmove(ptr + 8, IPptr, 28);		// IP header plus 8 data

//	We swap Souce to Dest, Convert to ICMP 11 and send back first 8 bytes of packet after header

	IPptr->IPDEST = IPptr->IPSOURCE;
	IPptr->IPSOURCE.addr = OurIPAddr;
	IPptr->IPPROTOCOL = ICMP;
	IPptr->IPTTL = IPTTL;
	IPptr->FRAGWORD = 0;
	IPptr->IPLENGTH = htons(56);			// IP Header ICMP Header IP Header 8 Data

	memset (ICMPptr, 0, 8);
	ICMPptr->ICMPTYPE = Type;
	ICMPptr->ICMPCODE = Code; 
	ICMPptr->ICMPSEQUENCE = htons(P2);
	ICMPptr->ICMPCHECKSUM = Generate_CHECKSUM(ICMPptr, 36);

	MapRouteIPMsg(IPptr);
}

static VOID MapRouteIPMsg(PIPMSG IPptr)
{
	PARPDATA Arp;
	BOOL Found;

	// We rely on the ARP messages generated by either end to route frames.
	//	If address is not in ARP cache (say call originated by MSYS), send to our default route

	//	Decremnent TTL and Recalculate header checksum

	IPptr->IPTTL--;

	if (IPptr->IPTTL == 0)
	{
		SendICMPTimeExceeded(IPptr);
		return;					// Should we send time exceeded????
	}

	IPptr->IPCHECKSUM = 0;
	IPptr->IPCHECKSUM = Generate_CHECKSUM(IPptr, 20);

	// Look up ARP

	Arp = LookupARP(IPptr->IPDEST.addr, FALSE, &Found);

	// If enabled, look in Net44 Encap Routes

	if (!Found && DefaultIPAddr)
		Arp = LookupARP(DefaultIPAddr, FALSE, &Found);

	if (!Found)
		return;				// No route or default
		
	if (Arp == NULL)
		return;				// Should we try to ARP it?
	
	if (Arp->ARPVALID)
	{
		SendIPtoBPQDEV(IPptr, Arp->HWADDR);
	}
	
	return;	
}

static PROUTEENTRY AllocRouteEntry()
{
	PROUTEENTRY Routeptr;

	if (NumberofRoutes == 0)

		RouteRecords = malloc(4);
	else
		RouteRecords = realloc(RouteRecords,(NumberofRoutes + 1) * 4);

	Routeptr = zalloc(sizeof(ROUTEENTRY));

	if (Routeptr == NULL) return NULL;
	
	RouteRecords[NumberofRoutes++] = Routeptr;
 
	return Routeptr;
}


static PARPDATA AllocARPEntry()
{
	ARPDATA * ARPptr;

	if (NumberofARPEntries == 0)

		ARPRecords = malloc(4);
	else
		ARPRecords = realloc(ARPRecords, (NumberofARPEntries+1)*4);

	ARPptr = malloc(sizeof(ARPDATA));

	if (ARPptr == NULL) return NULL;

	memset(ARPptr, 0, sizeof(ARPDATA));
	
	ARPRecords[NumberofARPEntries++] = ARPptr;
 
	return ARPptr;
}

 static VOID SendARPMsg(PARPDATA Arp)
 {
	//	Send ARP. Initially used only to find default gateway

	Arp->ARPTIMER =  5;							// Retry periodically

	ETHARPREQMSG.ARPOPCODE = 0x0100;		//             ; REQUEST

	ETHARPREQMSG.TARGETIPADDR = Arp->IPADDR;						
	memset(ETHARPREQMSG.TARGETHWADDR, 0, 6);

	ETHARPREQMSG.SENDIPADDR = OurIPAddr;
	memcpy(ETHARPREQMSG.SENDHWADDR,ourMACAddr, 6);

	memcpy(ETHARPREQMSG.MSGHDDR.SOURCE, ourMACAddr, 6);
	memset(ETHARPREQMSG.MSGHDDR.DEST, 255, 6);

	Send_ETH(&ETHARPREQMSG, 42);

	return;
 }

static PROUTEENTRY FindRoute(ULONG IPADDR)
{
	PROUTEENTRY Route = NULL;
	int i;

	for (i = 0; i < NumberofRoutes; i++)
	{
		Route = RouteRecords[i];

		if ((IPADDR & Route->SUBNET) == Route->NETWORK)
			return Route;
	}
	return NULL;
}



static PROUTEENTRY LookupRoute(ULONG IPADDR, ULONG Mask, BOOL Add, BOOL * Found)
{
	PROUTEENTRY Route = NULL;
	int i;

	for (i = 0; i < NumberofRoutes; i++)
	{
		Route = RouteRecords[i];

		if (Route->NETWORK == IPADDR && Route->SUBNET == Mask)
		{
			*Found = TRUE;
			return Route;
		}
	}

	// Not Found

	*Found = FALSE;

	if (Add)
	{
		Route = AllocRouteEntry();
		return Route;
	}
	else
		return NULL;
}

static PARPDATA LookupARP(ULONG IPADDR, BOOL Add, BOOL * Found)
{
	PARPDATA Arp = NULL;
	int i;

	for (i=0; i < NumberofARPEntries; i++)
	{
		Arp = ARPRecords[i];

		if (Arp->IPADDR == IPADDR)
		{
			*Found = TRUE;
			return Arp;
		}
	}

	// Not Found

	*Found = FALSE;

	if (Add)
	{
		Arp = AllocARPEntry();
		return Arp;
	}
	else
		return NULL;
}
static VOID RemoveARP(PARPDATA Arp);

static VOID RemoveRoute(PROUTEENTRY Route)
{
	int i;

	for (i=0; i < NumberofRoutes; i++)
	{
		if (Route == RouteRecords[i])
		{
			while (i < NumberofRoutes)
			{
				RouteRecords[i] = RouteRecords[i+1];
				i++;
			}
			
			if (Route->ARP)
			{
				PARPDATA Arp = Route->ARP;
				Route->ARP->ARPROUTE = NULL;			// Avoid recursion
				RemoveARP(Arp);
			}

			free(Route);
			NumberofRoutes--;
			return;
		}
	}
}


static VOID RemoveARP(PARPDATA Arp)
{
	int i;

	if (Arp->IPADDR == DefaultIPAddr)
	{
		// Dont remove Default Gateway. Set to re-resolve

		Arp->ARPVALID = FALSE;
		Arp->ARPTIMER = 5;
		return;
	}

	for (i=0; i < NumberofARPEntries; i++)
	{
		if (Arp == ARPRecords[i])
		{
			while (i < NumberofARPEntries)
			{
				ARPRecords[i] = ARPRecords[i+1];
				i++;
			}

			// Remove linked route

			if (Arp->ARPROUTE)
			{
				PROUTEENTRY Route = Arp->ARPROUTE;
				Arp->ARPROUTE->ARP = NULL;		// Avoid recursion
				RemoveRoute(Route);
			}

			free(Arp);
			NumberofARPEntries--;
			return;
		}
	}
}

	
static BOOL ReadConfigFile()
{

// IPAddr 192.168.0.129
// IPBroadcast 192.168.0.255
// IPGateway 192.168.0.1
// IPPorts 1,4 

// MAP 192.168.0.100 1001 n9pmo.dyndns.org 1000

	char * Config;
	char * ptr1, * ptr2;

	char buf[256],errbuf[256];

	map_table_len = 0;				// For reread

	Config = PortConfig[35];		// Config fnom bpq32.cfg

	if (Config)
	{
		// Using config from bpq32.cfg

		ptr1 = Config;

		ptr2 = strchr(ptr1, 13);
		while(ptr2)
		{
			memcpy(buf, ptr1, ptr2 - ptr1);
			buf[ptr2 - ptr1] = 0;
			ptr1 = ptr2 + 2;
			ptr2 = strchr(ptr1, 13);

			strcpy(errbuf,buf);			// save in case of error
	
			if (!ProcessLine(buf))
			{
				WritetoConsoleLocal("PortMapper bad config record ");
				strcat(errbuf, "\n");
				WritetoConsoleLocal(errbuf);
			}
		}
	}
	return (TRUE);
}


static ProcessLine(char * buf)
{
	char * ptr, * p_value, * p_origport, * p_host, * p_port;
	int port, mappedport, ipad;

	ptr = strtok(buf, " \t\n\r");
	p_value = strtok(NULL, " \t\n\r");


	if(ptr == NULL) return (TRUE);

	if(*ptr =='#') return (TRUE);			// comment

	if(*ptr ==';') return (TRUE);			// comment

	if(_stricmp(ptr,"ADAPTER") == 0)
	{
#ifndef WIN32
		WritetoConsoleLocal("IPGating to Ethernet is not supported in this build\n");
		return TRUE;
#endif
		strcpy(Adapter,p_value);
		return (TRUE);
	}

	if(_stricmp(ptr,"promiscuous") == 0)
	{
		Promiscuous = atoi(p_value);
		return (TRUE);
	}

	if (_stricmp(ptr,"IPAddr") == 0)
	{
		OurIPAddr = inet_addr(p_value);

		if (OurIPAddr == INADDR_NONE) return (FALSE);

		return (TRUE);
	}
	if (_stricmp(ptr,"IPBroadcast") == 0)
	{
		OurIPBroadcast = inet_addr(p_value);

		if (OurIPBroadcast == INADDR_NONE) return (FALSE);

		return (TRUE);
	}

	if (_stricmp(ptr,"IPNetMask") == 0)
	{
		OurNetMask = inet_addr(p_value);

		if (OurNetMask == INADDR_NONE) return (FALSE);

		return (TRUE);
	}


	if (_stricmp(ptr,"IPGateway") == 0)
	{
		DefaultIPAddr = inet_addr(p_value);

		if (DefaultIPAddr == INADDR_NONE) return (FALSE);

		return (TRUE);
	}

// ARP 44.131.4.18 GM8BPQ-7 1 D

	if (_stricmp(ptr,"MAP") == 0)
	{
#ifdef LINBPQ

		WritetoConsoleLocal("MAP not supported in LinBPQ IP Gateway\n");
		return TRUE;
#endif
		if (!p_value) return FALSE;

		p_origport = strtok(NULL, " ,\t\n\r");
		if (!p_origport) return FALSE;

		p_host = strtok(NULL, " ,\t\n\r");
		if (!p_host) return FALSE;

		p_port = strtok(NULL, " ,\t\n\r");
		if (!p_port) return FALSE;

		port=atoi(p_origport);
		if (port == 0) return FALSE;

		mappedport=atoi(p_port);
		if (mappedport == 0) return FALSE;

		ipad = inet_addr(p_value);

		map_table[map_table_len].sourceipaddr = ipad;
		strcpy(map_table[map_table_len].hostname, p_host);
		map_table[map_table_len].sourceport = ntohs(port);
		map_table[map_table_len++].mappedport = ntohs(mappedport);
	
		NeedResolver = TRUE;

		return (TRUE);
	}
	
	//
	//	Bad line
	//
	return (FALSE);
	
}

static VOID DoARPTimer()
{
	PARPDATA Arp = NULL;
	int i;

	for (i=0; i < NumberofARPEntries; i++)
	{
		Arp = ARPRecords[i];

		if (!Arp->ARPVALID)
		{
			Arp->ARPTIMER--;
			
			if (Arp->ARPTIMER == 0)
			{
				// Retry Request

				SendARPMsg(Arp);
			}
			continue;
		}

		// Time out active entries

		if (Arp->LOCKED == 0)
		{
			Arp->ARPTIMER--;
			
			if (Arp->ARPTIMER == 0)
			{
				// Remove Entry
				
				RemoveARP(Arp);
				SaveARP();
			}
		}
	}
}

static VOID DoRouteTimer()
{
	int i;
	PROUTEENTRY Route;

	for (i=0; i < NumberofRoutes; i++)
	{
		Route = RouteRecords[i];
		if (Route->RIPTIMOUT)
			Route->RIPTIMOUT--;
	}
}


// PCAP Support Code


#ifdef WIN32

static FARPROCX GetAddress(char * Proc)
{
	FARPROCX ProcAddr;
	int err=0;
	char buf[256];
	int n;


	ProcAddr=(FARPROCX) GetProcAddress(PcapDriver,Proc);

	if (ProcAddr == 0)
	{
		err=GetLastError();

		n=sprintf(buf,"Error finding %s - %d", Proc,err);
		WritetoConsoleLocal(buf);
	
		return(0);
	}

	return ProcAddr;
}


static void packet_handler(u_char *param, const struct pcap_pkthdr *header, const u_char *pkt_data);

static int OpenPCAP()
{
	u_long param=1;
	BOOL bcopt=TRUE;
	int i=0;
	char errbuf[PCAP_ERRBUF_SIZE];
	u_int netmask;
	char packet_filter[64];
	struct bpf_program fcode;
	char buf[256];
	int n;


	PcapDriver=LoadLibrary(Dllname);

	if (PcapDriver == NULL) return(FALSE);
	
	if ((pcap_sendpacketx=GetAddress("pcap_sendpacket")) == 0 ) return FALSE;

	if ((pcap_datalinkx=GetAddress("pcap_datalink")) == 0 ) return FALSE;

	if ((pcap_compilex=GetAddress("pcap_compile")) == 0 ) return FALSE;

	if ((pcap_setfilterx=GetAddress("pcap_setfilter")) == 0 ) return FALSE;
	
	pcap_open_livex = (pcap_t * (__cdecl *)(const char *, int, int, int, char *)) GetProcAddress(PcapDriver,"pcap_open_live");

	if (pcap_open_livex == NULL) return FALSE;

	if ((pcap_geterrx=GetAddress("pcap_geterr")) == 0 ) return FALSE;

	if ((pcap_next_exx=GetAddress("pcap_next_ex")) == 0 ) return FALSE;

	/* Open the adapter */

	adhandle = pcap_open_livex(Adapter,	// name of the device
							 65536,			// portion of the packet to capture. 
											// 65536 grants that the whole packet will be captured on all the MACs.
							 Promiscuous,	// promiscuous mode (nonzero means promiscuous)
							 1,				// read timeout
							 errbuf			// error buffer
							 );
	
	if (adhandle == NULL)
		return FALSE;
	
	/* Check the link layer. We support only Ethernet for simplicity. */
	
	if(pcap_datalinkx(adhandle) != DLT_EN10MB)
	{
		n=sprintf(buf,"\nThis program works only on Ethernet networks.\n");
		WritetoConsoleLocal(buf);
	
		adhandle = 0;
		return FALSE;
	}

	netmask=0xffffff; 

//	sprintf(packet_filter,"ether[12:2]=0x0800 or ether[12:2]=0x0806");

	sprintf(packet_filter,"ether broadcast or ether dst %02x:%02x:%02x:%02x:%02x:%02x",
		ourMACAddr[0], ourMACAddr[1], ourMACAddr[2],
		ourMACAddr[3], ourMACAddr[4], ourMACAddr[5]);
		
	//compile the filter

	if (pcap_compilex(adhandle, &fcode, packet_filter, 1, netmask) <0 )
	{	
		n=sprintf(buf,"\nUnable to compile the packet filter. Check the syntax.\n");
		WritetoConsoleLocal(buf);

		adhandle = 0;
		return FALSE;
	}
	
	//set the filter

	if (pcap_setfilterx(adhandle, &fcode)<0)
	{
		n=sprintf(buf,"\nError setting the filter.\n");
		WritetoConsoleLocal(buf);

		adhandle = 0;
		return FALSE;
	}
	
	return TRUE;
}
#endif


int CompareMasks (const VOID * a, const VOID * b);


#ifndef LINBPQ

extern HFONT hFont;
struct tagMSG Msg;
char buf[1024];

static LRESULT CALLBACK ResWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	HFONT    hOldFont ;
	struct hostent * hostptr;
	struct in_addr ipad;
	char line[100];
	int index,displayline;
	MINMAXINFO * mmi;

	int i=1;

	int nScrollCode,nPos;

	switch (message)
	{
	case WM_GETMINMAXINFO:

 		mmi = (MINMAXINFO *)lParam;
		mmi->ptMaxSize.x = 400;
		mmi->ptMaxSize.y = Windowlength;
		mmi->ptMaxTrackSize.x = 400;
		mmi->ptMaxTrackSize.y = Windowlength;
		break;

	case WM_USER+199:

		i=WSAGETASYNCERROR(lParam);

		map_table[ResolveIndex].error=i;

		if (i ==0)
		{
			// resolved ok

			hostptr=(struct hostent *)&buf;
			memcpy(&map_table[ResolveIndex].mappedipaddr,hostptr->h_addr,4);
		}

  		InvalidateRect(hWnd,NULL,FALSE);

		while (ResolveIndex < map_table_len)
		{
			ResolveIndex++;
			
			WSAAsyncGetHostByName (hWnd,WM_USER+199,
						map_table[ResolveIndex].hostname,
						buf,MAXGETHOSTSTRUCT);	
			
			break;
		}
		break;

	case WM_MDIACTIVATE:
	{ 
		// Set the system info menu when getting activated
			 
		if (lParam == (LPARAM) hWnd)
		{
			// Activate

			RemoveMenu(hBaseMenu, 1, MF_BYPOSITION);
			AppendMenu(hBaseMenu, MF_STRING + MF_POPUP, (WPARAM)hPopMenu, "Actions");

			SendMessage(ClientWnd, WM_MDISETMENU, (WPARAM) hBaseMenu, (LPARAM)hWndMenu);
		}
		else
			SendMessage(ClientWnd, WM_MDISETMENU, (WPARAM)hMainFrameMenu, (LPARAM)NULL);
			
		DrawMenuBar(FrameWnd);

		return DefMDIChildProc(hWnd, message, wParam, lParam);

	}

	case WM_COMMAND:

		wmId    = LOWORD(wParam); // Remember, these are...
		wmEvent = HIWORD(wParam); // ...different for Win32!



		if (wmId == BPQREREAD)
		{
			ProcessConfig();
			FreeConfig();

			ReadConfigFile();
			PostMessage(hIPResWnd, WM_TIMER,0,0);
			InvalidateRect(hWnd,NULL,TRUE);

			return 0;
		}
/*
		if (wmId == BPQADDARP)
		{
			if (ConfigWnd == 0)
			{		
				ConfigWnd=CreateDialog(hInstance,ConfigClassName,0,NULL);
    
				if (!ConfigWnd)
				{
					i=GetLastError();
					return (FALSE);
				}
				ShowWindow(ConfigWnd, SW_SHOW);  
				UpdateWindow(ConfigWnd); 
  			}

			SetForegroundWindow(ConfigWnd);

			return(0);
		}
		return 0;
*/
	case WM_SYSCOMMAND:

		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		
		switch (wmId)
		{ 
		case SC_RESTORE:

			IPMinimized = FALSE;
			SendMessage(ClientWnd, WM_MDIRESTORE, (WPARAM)hWnd, 0);
			break;

		case SC_MINIMIZE: 

			IPMinimized = TRUE;
			break;
		}
		return DefMDIChildProc(hWnd, message, wParam, lParam);

	case WM_VSCROLL:
		
		nScrollCode = (int) LOWORD(wParam); // scroll bar value 
		nPos = (short int) HIWORD(wParam);  // scroll box position 

		//hwndScrollBar = (HWND) lParam;      // handle of scroll bar 

		if (nScrollCode == SB_LINEUP || nScrollCode == SB_PAGEUP)
		{
			baseline--;
			if (baseline <0)
				baseline=0;
		}

		if (nScrollCode == SB_LINEDOWN || nScrollCode == SB_PAGEDOWN)
		{
			baseline++;
			if (baseline > map_table_len)
				baseline = map_table_len;
		}

		if (nScrollCode == SB_THUMBTRACK)
		{
			baseline=nPos;
		}

		SetScrollPos(hWnd,SB_VERT,baseline,TRUE);

		InvalidateRect(hWnd,NULL,TRUE);
		break;


	case WM_PAINT:

		hdc = BeginPaint (hWnd, &ps);
		
		hOldFont = SelectObject( hdc, hFont) ;
			
		index = baseline;
		displayline=0;

		while (index < map_table_len)
		{
			if (map_table[index].ResolveFlag && map_table[index].error != 0)
			{
					// resolver error - Display Error Code
				sprintf(hostaddr,"Error %d",map_table[index].error);
			}
			else
			{
				memcpy(&ipad,&map_table[index].mappedipaddr,4);
				strncpy(hostaddr,inet_ntoa(ipad),16);
			}
				
			memcpy(&ipad,&map_table[index].mappedipaddr,4);
								
			i=sprintf(line,"%.64s = %-.30s",
				map_table[index].hostname,
				hostaddr);

			TextOut(hdc,0,(displayline++)*14+2,line,i);

			index++;
		}

		SelectObject( hdc, hOldFont ) ;
		EndPaint (hWnd, &ps);
	
		break;        

	case WM_DESTROY:
		

//		PostQuitMessage(0);
			
		break;


	case WM_TIMER:
			
		for (ResolveIndex=0; ResolveIndex < map_table_len; ResolveIndex++)
		{	
			WSAAsyncGetHostByName (hWnd,WM_USER+199,
						map_table[ResolveIndex].hostname,
						buf,MAXGETHOSTSTRUCT);
			break;	
		}

	default:
		break;
	}
			return DefMDIChildProc(hWnd, message, wParam, lParam);
}

static void IPResolveNames( void *dummy )
{
	SetTimer(hIPResWnd,1,15*60*1000,0);	

	PostMessage(hIPResWnd, WM_TIMER,0,0);

	while (GetMessage(&Msg, hIPResWnd, 0, 0)) 
	{
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
	}		
}

#endif

/*
;	DO PSEUDO HEADER FIRST
;
	MOV	DX,600H			; PROTOCOL (REVERSED)
	MOV	AX,TCPLENGTH		; TCP LENGTH
	XCHG	AH,AL
	ADD	DX,AX
	MOV	AX,WORD PTR LOCALADDR[BX]
	ADC	DX,AX
	MOV	AX,WORD PTR LOCALADDR+2[BX]
	ADC	DX,AX
	MOV	AX,WORD PTR REMOTEADDR[BX]
	ADC	DX,AX
	MOV	AX,WORD PTR REMOTEADDR+2[BX]
	ADC	DX,AX
	ADC	DX,0

	MOV	PHSUM,DX

	PUSH	BX

	MOV	BX,TXBUFFER		; HEADER

	MOV	CX,TCPLENGTH		; PUT LENGTH INTO HEADER
	MOV	BUFFLEN[BX],CX
;
	MOV	SI,BUFFPTR[BX]

	INC	CX			; ROUND UP
	SHR	CX,1			; WORD COUNT

	CALL	DO_CHECKSUM

	ADD	DX,PHSUM
	ADC	DX,0
	NOT	DX

	MOV	SI,BUFFPTR[BX]
	MOV	CHECKSUM[SI],DX


*/
