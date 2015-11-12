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

/*
	Windows uses PCAP to send to both the local host (the machine running BPQ) and 
	to other machines on the same LAN. I may be able to add the 44/8 route but
	dont at the moment.

	On Linux, the local machine doesn't see packets sent via pcap, so it uses a TAP
	device for the local host, and pcap for other addresses on the LAN. The TAP is 
	created dynamically - it doesn't have to be predefined. A route to 44/8 via the
	TAP and an ARP entry for it are also added. The TAP runs unnumbered

	44 addresses can be NAT'ed to the local LAN address, so hosts don't have to have
	both an ISP and a 44 address. You can run your local LAN as 44, but I would expect
	most uses to prefer to keep their LAN with its normal (usually 192.168) addresses.

	If the PC address isn't the same as the IPGateway IPAddr a NAT entry is created
	automaticaly.

	In these cases the NAT line for jnos should have TAP appended to tell
	LinBPQ it is reached over the TAP.

	NAT 44.131.11.x 192.168.x.y TAP

*/


/*

ShellExecute( NULL, 
    "runas",  
    "c:\\windows\\notepad.exe",  
    " c:\\temp\\report.txt",     
    NULL,                        // default dir 
    SW_SHOWNORMAL  
); 

*/


#pragma data_seg("_BPQDATA")

#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <stdio.h>
#include <time.h>

#include "CHeaders.h"

#include "IPCode.h"

#ifdef WIN32
#include <iphlpapi.h>
// Link with Iphlpapi.lib
#pragma comment(lib, "IPHLPAPI.lib")
#endif

//#ifdef WIN32
#include <pcap.h>
//#endif

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
VOID AddToRoutes(PARPDATA Arp, UINT IPAddr, char Type);

VOID ProcessTunnelMsg(PIPMSG IPptr);
VOID ProcessRIP44Message(PIPMSG IPptr);
PROUTEENTRY LookupRoute(ULONG IPADDR, ULONG Mask, BOOL Add, BOOL * Found);
BOOL ProcessROUTELine(char * buf, BOOL Locked);
VOID DoRouteTimer();
PROUTEENTRY FindRoute(ULONG IPADDR);
VOID SendIPtoEncap(PIPMSG IPptr, ULONG Encap);
USHORT Generate_CHECKSUM(VOID * ptr1, int Len);
VOID RecalcTCPChecksum(PIPMSG IPptr);
VOID RecalcUDPChecksum(PIPMSG IPptr);
BOOL Send_ETH(VOID * Block, DWORD len, BOOL SendToTAP);
VOID ProcessEthARPMsg(PETHARP arpptr, BOOL FromTAP);
VOID WriteIPRLine(PROUTEENTRY RouteRecord, FILE * file);
int CountBits(unsigned long in);
VOID SendARPMsg(PARPDATA ARPptr, BOOL ToTAP);;
BOOL DecodeCallString(char * Calls, BOOL * Stay, BOOL * Spy, UCHAR * AXCalls);

#define ARPTIMEOUT 3600

//       ARP REQUEST (AX.25)

AXARP AXARPREQMSG = {0};

//		ARP REQUEST/REPLY (Eth)

ETHARP ETHARPREQMSG = {0};

ARPDATA ** ARPRecords = NULL;				// ARP Table - malloc'ed as needed

int NumberofARPEntries = 0;

ROUTEENTRY ** RouteRecords = NULL;

int NumberofRoutes = 0;

time_t LastRIP44Msg = 0;

//HANDLE hBPQNET = INVALID_HANDLE_VALUE;

ULONG OurIPAddr = 0;
char IPAddrText[20];			// Text form of Our Address

ULONG EncapAddr = INADDR_NONE;	// Virtual Host for IPIP PCAP Mode.
char EncapAddrText[20];			// Text form of Our Address

UCHAR RouterMac[6] = {0};		// Mac Address of our Internet Gateway.

//ULONG HostIPAddr = INADDR_NONE;	// Makes more sense to use same addr for host

ULONG HostNATAddr = INADDR_NONE;	// LAN address (not 44 net) of our host
char HostNATAddrText[20];

ULONG OurNetMask = 0xffffffff;

BOOL WantTAP = FALSE;
BOOL WantEncap = 0;				// Run RIP44 and Net44 Encap
BOOL NoDefaultRoute = FALSE;	// Don't add route to 44/8

SOCKET EncapSock = 0;

BOOL UDPEncap = FALSE;

BOOL IPv6 = FALSE;
int UDPPort = 4473;			// RX Port, Send on +1

BOOL BPQSNMP = FALSE;		// If set process SNMP in BPQ, else pass to host

int IPTTL = 128;

int tap_fd = 0;

int FramesForwarded = 0;
int FramesDropped = 0;
int ARPTimeouts = 0;
int SecTimer = 10;

extern char * PortConfig[];

int baseline=0;

unsigned char  hostaddr[64];

static int nat_table_len = 0;

static struct nat_table_entry nat_table[MAX_ENTRIES];


ULONG UCSD44;		// 44.0.0.1


// Following two fields used by stats to get round shared memmory problem

ARPDATA Arp={0};
int ARPFlag = -1;

// Following Buffer is used for msgs from WinPcap. Put the Enet message part way down the buffer, 
//	so there is room for ax.25 header instead of Enet header when we route the frame to ax.25
//	Enet Header ia 14 bytes, AX.25 UI is 16

// Also used to reassemble NOS Fragmented ax.25 packets

static UCHAR Buffer[4096] = {0};

#define EthOffset 30				// Should be plenty

DWORD IPLen = 0;

UCHAR QST[7]={'Q'+'Q','S'+'S','T'+'T',0x40,0x40,0x40,0xe0};		//QST IN AX25

#ifdef WIN32
UCHAR ourMACAddr[6] = {02,'B','P','Q',2,2};
#else
UCHAR ourMACAddr[6] = {02,'B','P','Q',1,1};
#endif

UCHAR RealMacAddress[6];

int IPPortMask = 0;

IPSTATS IPStats = {0};

UCHAR BPQDirectory[260];

char ARPFN[MAX_PATH];
char IPRFN[MAX_PATH];

HANDLE handle;

//#ifdef WIN32
pcap_t *adhandle = 0;
pcap_t * (FAR * pcap_open_livex)(const char *, int, int, int, char *);

int pcap_reopen_delay;
//#endif

static char Adapter[256];

int Promiscuous = 1;			// Default to Promiscuous

#ifdef WIN32

HINSTANCE PcapDriver=0;

typedef int (FAR *FARPROCX)();

int (FAR * pcap_sendpacketx)();

FARPROCX pcap_findalldevsx;

FARPROCX pcap_compilex;
FARPROCX pcap_setfilterx;
FARPROCX pcap_datalinkx;
FARPROCX pcap_next_exx;
FARPROCX pcap_geterrx;
FARPROCX pcap_closex;


char Dllname[6]="wpcap";

FARPROCX GetAddress(char * Proc);

#else
#define pcap_findalldevsx pcap_findalldevs
#define pcap_compilex pcap_compile
#define pcap_open_livex pcap_open_live
#define pcap_setfilterx pcap_setfilter
#define pcap_datalinkx pcap_datalink
#define pcap_next_exx pcap_next_ex
#define pcap_geterrx pcap_geterr
#define pcap_sendpacketx pcap_sendpacket
#define pcap_closex pcap_close
#endif
VOID __cdecl Debugprintf(const char * format, ...);

#ifdef WIN32

// Routine to check if a route to 44.0.0.0/8 exists and points to us

BOOL Check44Route(int Interface)
{
	PMIB_IPFORWARDTABLE pIpForwardTable = NULL;
	PMIB_IPFORWARDROW Row;
	int Size = 0;
	DWORD n;

	//	First call gets the required size

	n = GetIpForwardTable(pIpForwardTable, &Size, FALSE);

	pIpForwardTable = malloc(Size);
	
	n  = GetIpForwardTable(pIpForwardTable, &Size, FALSE);

	if (n)
		return FALSE;			// Couldnt read table

	Row = pIpForwardTable->table;

	for (n = 0; n < pIpForwardTable->dwNumEntries; n++)
	{
		if (Row->dwForwardDest == 44 && Row->dwForwardMask == 255 && Row->dwForwardIfIndex == Interface)
		{
			free(pIpForwardTable);
			return TRUE;
		}
		Row++;
	}

	free(pIpForwardTable);

	return FALSE;
}

BOOL Setup44Route(int Interface, char * Gateway)
{
	//	Better just to call route.exe, so we can set -p flag and use runas

	char Params[256];

	sprintf(Params, " -p add 44.0.0.0 mask 255.0.0.0 %s if %d", Gateway, Interface);

	ShellExecute(NULL, "runas", "c:\\windows\\system32\\route.exe", Params, NULL, SW_SHOWNORMAL); 
/*
	MIB_IPFORWARDROW Row = {0};
	int ret;

	Row.dwForwardDest = 44;
	Row.dwForwardMask = 255;
	Row.dwForwardIfIndex = 24;
	Row.dwForwardMetric1 = 100;
	Row.dwForwardProto = MIB_IPPROTO_NETMGMT;
	ret = CreateIpForwardEntry(&Row);
*/

	return 1;
}

#endif

char FormatIPWork[20];

char * FormatIP(ULONG Addr)
{
	unsigned char work[4];

	memcpy(work, &Addr, 4);
	sprintf(FormatIPWork, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

	return FormatIPWork;
}

int CompareRoutes (const VOID * a, const VOID * b)
{
	PROUTEENTRY x;
	PROUTEENTRY y;

	unsigned long r1, r2;

	x = * (PROUTEENTRY const *) a;
	y = * (PROUTEENTRY const *) b;

	r1 = x->NETWORK;
	r2 = y->NETWORK;

	r1 = htonl(r1);
	r2 = htonl(r2);

	if (r1 < r2 ) return -1;
	if (r1 == r2 ) return 0;
	return 1;
}


int CompareMasks (const VOID * a, const VOID * b)
{
	PROUTEENTRY x;
	PROUTEENTRY y;

	unsigned long m1, m2;
	unsigned long r1, r2;

	x = * (PROUTEENTRY const *) a;
	y = * (PROUTEENTRY const *) b;

	r1 = x->NETWORK;
	r2 = y->NETWORK;

	m1 = x->SUBNET;
	m2 = y->SUBNET;

	m1 = htonl(m1);
	m2 = htonl(m2);

	r1 = htonl(r1);
	r2 = htonl(r2);

	if (m1 > m2) return -1;
	if (m1 == m2)
	{
		if (r1 < r2) return -1;
		if (r1 == r2 ) return 0;
	}
	return 1;
}



void OpenTAP();

BOOL GetPCAP()
{
#ifdef WIN32

	PcapDriver=LoadLibrary(Dllname);

	if (PcapDriver == NULL) return(FALSE);
	
	if ((pcap_findalldevsx=GetAddress("pcap_findalldevs")) == 0 ) return FALSE;

	if ((pcap_sendpacketx=GetAddress("pcap_sendpacket")) == 0 ) return FALSE;

	if ((pcap_datalinkx=GetAddress("pcap_datalink")) == 0 ) return FALSE;

	if ((pcap_compilex=GetAddress("pcap_compile")) == 0 ) return FALSE;

	if ((pcap_setfilterx=GetAddress("pcap_setfilter")) == 0 ) return FALSE;
	
	pcap_open_livex = (pcap_t * (__cdecl *)(const char *, int, int, int, char *)) GetProcAddress(PcapDriver,"pcap_open_live");

	if (pcap_open_livex == NULL) return FALSE;

	if ((pcap_geterrx = GetAddress("pcap_geterr")) == 0 ) return FALSE;

	if ((pcap_next_exx = GetAddress("pcap_next_ex")) == 0 ) return FALSE;

	if ((pcap_closex = GetAddress("pcap_close")) == 0 ) return FALSE;


#endif
	return TRUE;
}
Dll BOOL APIENTRY Init_IP()
{
	if (BPQDirectory[0] == 0)
	{
		strcpy(ARPFN,"BPQARP.dat");
	}
	else
	{
		strcpy(ARPFN,BPQDirectory);
		strcat(ARPFN,"/");
		strcat(ARPFN,"BPQARP.dat");
	}
	
	if (BPQDirectory[0] == 0)
	{
		strcpy(IPRFN,"BPQIPR.dat");
	}
	else
	{
		strcpy(IPRFN,BPQDirectory);
		strcat(IPRFN,"/");
		strcat(IPRFN,"BPQIPR.dat");
	}
	
	//	Clear fields in case of restart

	ARPRecords = NULL;				// ARP Table - malloc'ed as needed
	NumberofARPEntries=0;

	RouteRecords = NULL;
	NumberofRoutes = 0;

	nat_table_len = 0;

	ReadConfigFile();

	ourMACAddr[5] = (UCHAR)(OurIPAddr >> 24) & 255;
	
	// Clear old packets

	IPHOSTVECTORPTR->HOSTAPPLFLAGS = 0x80;			// Request IP frames from Node

	// Set up static fields in ARP messages

	AXARPREQMSG.HWTYPE=0x0300;				//	AX25
	memcpy(AXARPREQMSG.MSGHDDR.DEST, QST, 7);
	memcpy(AXARPREQMSG.MSGHDDR.ORIGIN, MYCALL, 7);
	AXARPREQMSG.MSGHDDR.ORIGIN[6] |= 1;		// Set End of Call
	AXARPREQMSG.MSGHDDR.PID = 0xcd;			// ARP
	AXARPREQMSG.MSGHDDR.CTL = 03;			// UI

	AXARPREQMSG.PID=0xcc00;					// TYPE
	AXARPREQMSG.HWTYPE=0x0300;	
	AXARPREQMSG.HWADDRLEN = 7;
	AXARPREQMSG.IPADDRLEN = 4;

	memcpy(AXARPREQMSG.SENDHWADDR, MYCALL, 7);
	AXARPREQMSG.SENDIPADDR = OurIPAddr;

	memset(ETHARPREQMSG.MSGHDDR.DEST, 255, 6);
	memcpy(ETHARPREQMSG.MSGHDDR.SOURCE, ourMACAddr, 6);
	ETHARPREQMSG.MSGHDDR.ETYPE = 0x0608;			// ARP

	ETHARPREQMSG.HWTYPE=0x0100;				//	Eth
	ETHARPREQMSG.PID=0x0008;	
	ETHARPREQMSG.HWADDRLEN = 6;
	ETHARPREQMSG.IPADDRLEN = 4;

//#ifdef WIN32

	if (Adapter[0])
		GetPCAP();

	// on Windows create a NAT entry for IPADDR.
	// on linux enable the TAP device (on Linux you can't use pcap to talk to 
	// the local host, whereas on Windows you can.

#ifndef MACBPQ
	{
		pcap_if_t * ifs, * saveifs;
		char Line[80];

		// Find IP Addr of Adapter Interface
		
		pcap_findalldevsx(&ifs, "");	

		saveifs = ifs;		// Save for release

		while (ifs)
		{
			if (strcmp(ifs->name, Adapter) == 0)
				break;

			ifs = ifs->next;
		}

		if (ifs)
		{
			struct pcap_addr *address;

			address = ifs->addresses;

			while (address)
			{
				if (address->addr->sa_family == 2)
					break;

				address = address->next;
			}

			if (address)
			{
				memcpy(&HostNATAddr, &address->addr->sa_data[2], 4);

				sprintf(HostNATAddrText, "%d.%d.%d.%d", (UCHAR)address->addr->sa_data[2],
						(UCHAR)address->addr->sa_data[3],
						(UCHAR)address->addr->sa_data[4],
						(UCHAR)address->addr->sa_data[5]);			
			}
			
			//	We need to create a NAT entry.

			//  For now do for both Windows and Linux

			sprintf(Line, "NAT %s %s", IPAddrText, HostNATAddrText);
			Debugprintf("Generated NAT %s\n", Line);
			ProcessLine(Line);
#ifdef WIN32
#else
			// Linux, need TAP

			WantTAP = TRUE;

#endif
			}
		}
				
#endif

    //
    // Open PCAP Driver

	if (Adapter[0])					// Don't have to have ethernet, if used just as ip over ax.25 switch 
	{
		char buf[80];

		if (OpenPCAP())
			sprintf(buf,"IP Using %s\n", Adapter);
		else
			sprintf(buf," IP Unable to open %s\n", Adapter);
	
		WritetoConsoleLocal(buf);

		if (adhandle == NULL)
		{
			WritetoConsoleLocal("Failed to open pcap device - IP Support Disabled\n");
			return FALSE;
		} 
	}

//#else

	// Linux - if TAP requested, open it

#ifndef WIN32
#ifndef MACBPQ

	if (WantTAP)
		OpenTAP();

#endif
#endif

	ReadARP();
	ReadIPRoutes();

	// if we are running as Net44 encap, open a socket for IPIP

	if (WantEncap)
	{
		union
		{
			struct sockaddr_in sinx; 
			struct sockaddr_in6 sinx6; 
		} sinx = {0};
		u_long param = 1;
		int err, ret;
		char Msg[80];
				
		UCSD44 = inet_addr("44.0.0.1");

#ifdef WIN32

		// Find Interface number for PCAP Device

		{
			UINT ulOutBufLen;
			PIP_ADAPTER_INFO pAdapterInfo;
			PIP_ADAPTER_INFO pAdapter = NULL;
			DWORD dwRetVal = 0;
			int Interface = 0;

			// Make an initial call to GetAdaptersInfo to get
			// the necessary size into the ulOutBufLen variable

			GetAdaptersInfo(NULL, &ulOutBufLen);
				
			pAdapterInfo = (IP_ADAPTER_INFO *) malloc(ulOutBufLen);
		
			if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
			{
				pAdapter = pAdapterInfo;
				while (pAdapter)
				{
					if (strstr(Adapter, pAdapter->AdapterName))
					{
						Interface = pAdapter->Index;
						break;
					}
					pAdapter = pAdapter->Next;
				}
				free(pAdapterInfo);
			}
			if (NoDefaultRoute == FALSE)
			{
				// Check Route to 44 and if not there add

				if (Check44Route(Interface))	
					WritetoConsoleLocal("Route to 44/8 found\n");
				else
				{
					
#pragma warning(push)
#pragma warning(disable : 4996)
					if (_winver >= 0x0600)
#pragma warning(pop)
						Setup44Route(Interface, "0.0.0.0");
					else
						Setup44Route(Interface, EncapAddrText);
					
					Sleep(2000);
					if (Check44Route(Interface))	
						WritetoConsoleLocal("Route to 44/8 added\n");
					else				
						WritetoConsoleLocal("Adding route to 44/8 Failed\n");
				}
			}
		}			
#endif

		if (EncapAddr != INADDR_NONE)
		{
			// Using Virtual Host on PCAP Adapter (Windows)

			WritetoConsoleLocal("Net44 Tunnel opened on PCAP device\n");
			WritetoConsoleLocal("IP Support Enabled\n");
			return TRUE;
		}

		if (UDPEncap)
		{
			// Open UDP Socket

			if (IPv6)
				EncapSock = socket(AF_INET6,SOCK_DGRAM,0);
			else
				EncapSock = socket(AF_INET,SOCK_DGRAM,0);

			sinx.sinx.sin_port = htons(UDPPort);
		}
		else
		{
			// Open Raw Socket

			EncapSock = socket(AF_INET, SOCK_RAW, 4);
			sinx.sinx.sin_port = 0;
		}

		if (EncapSock == INVALID_SOCKET)
		{
			err = WSAGetLastError();
			sprintf(Msg, "Failed to create socket for IPIP Encap - error code = %d\n", err);
			WritetoConsoleLocal(Msg);
		}
		else
		{
			
			ioctl (EncapSock,FIONBIO,&param);

			if (IPv6)
			{
				sinx.sinx.sin_family = AF_INET6;
				memset (&sinx.sinx6.sin6_addr, 0, 16);
				ret = bind(EncapSock, (struct sockaddr *) &sinx.sinx, sizeof(sinx.sinx6));
			}
			else
			{
				sinx.sinx.sin_family = AF_INET;
				sinx.sinx.sin_addr.s_addr = INADDR_ANY;
				ret = bind(EncapSock, (struct sockaddr *) &sinx.sinx, sizeof(sinx.sinx));
			}
	
			if (ret)
			{
				//	Bind Failed

				err = WSAGetLastError();
				sprintf(Msg, "Bind Failed  for IPIP Encap socket - error code = %d\n", err);
				WritetoConsoleLocal(Msg);
			}
			else
			{
				WritetoConsoleLocal("Net44 Tunnel opened\n");
			}
		}
	}

	WritetoConsoleLocal("IP Support Enabled\n");

	return TRUE;

}

VOID IPClose()
{
	SaveIPRoutes();

	if (adhandle)
		pcap_closex(adhandle);
	
#ifdef LINBPQ
	if (WantTAP && tap_fd)
		close(tap_fd);
#endif

	if (EncapSock)
		closesocket(EncapSock);
}

union
{
	struct sockaddr_in rxaddr;
	struct sockaddr_in6 rxaddr6;
} RXaddr;


Dll BOOL APIENTRY Poll_IP()
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

//#ifdef WIN32

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
				ProcessEthARPMsg((PETHARP)ethptr, FALSE);
				goto Pollloop;
			}

			// Ignore anything else

			goto Pollloop;
		}
		else
		{
			if (res < 0)
			{
				char * error  = (char *)pcap_geterrx(adhandle) ;
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

//#endif

#ifdef LINBPQ

PollTAPloop:

	if (WantTAP && tap_fd)
	{
		int nread;

		nread = read(tap_fd, &Buffer[EthOffset], 1600);

		if (nread > 0)
		{
			PETHMSG ethptr = (PETHMSG)&Buffer[EthOffset];

			if (ethptr->ETYPE == 0x0008)
			{
				ProcessEthIPMsg((PETHMSG)&Buffer[EthOffset]);
				goto PollTAPloop;
			}

			if (ethptr->ETYPE == 0x0608)
			{
				ProcessEthARPMsg((PETHARP)ethptr, TRUE);
				goto PollTAPloop;
			}

			// if 08FF pass to BPQETHER Driver
/*
			if (ethptr->ETYPE == 0xFF08)
			{
				PBUFFHEADER axmsg;
				PBUFFHEADER savemsg;
				int len;
			
				// BPQEther Encap

				len = Buffer[EthOffset + 15]*256 + Buffer[EthOffset + 14];

				axmsg = (PBUFFHEADER)&Buffer[EthOffset + 9];
				axmsg->LENGTH = len;
				axmsg->PORT = 99;			// Dummy for IP Gate

				printf("BPQ Eth Len %d PID %d\n", len, axmsg->PID);

				if ((len < 16) || (len > 320))
					goto PollTAPloop; // Probably RLI Mode Frame


				//len-=3;
		
			//memcpy(&buff[7],&pkt_data[16],len);
		
			//		len+=5;

				savemsg=axmsg;

				// Packet from AX.25

				if (CompareCalls(axmsg->ORIGIN, MYCALL))
					return 0;				// Echoed packet

				switch (axmsg->PID)
				{
				case  0xcc:

				// IP Message

				{
					PIPMSG ipptr = (PIPMSG)++axmsg;
					axmsg--;
					ProcessIPMsg(ipptr, axmsg->ORIGIN, (axmsg->CTL == 3) ? 'D' : 'V', axmsg->PORT);
					break;
				}

				case 0xcd:
		
				// ARP Message

					ProcessAXARPMsg((PAXARP)axmsg);
					SaveARP();
					break;

	//		case 0x08:
				}

				goto PollTAPloop;
			}
*/
			// Ignore anything else

//			printf("TAP ype %X\n", ntohs(ethptr->ETYPE));

			goto PollTAPloop;
		}
	}

#endif

PollEncaploop:

	if (EncapSock)
	{
		int nread;
		int addrlen = sizeof(struct sockaddr_in6);

		nread = recvfrom(EncapSock, &Buffer[EthOffset], 1600, 0, (struct sockaddr *)&RXaddr.rxaddr,&addrlen);

		if (nread > 0)
		{
			PIPMSG IPptr = (PIPMSG)&Buffer[EthOffset];

			if (IPptr->IPPROTOCOL == 4)		// AMPRNET Tunnelled Packet
			{
				ProcessTunnelMsg(IPptr);
			}

			goto PollEncaploop;
		}
		else
			res = GetLastError();
	}


	if (IPHOSTVECTORPTR->HOSTTRACEQ != 0)
	{
		PBUFFHEADER axmsg;
		PBUFFHEADER savemsg;

		axmsg = (PBUFFHEADER)IPHOSTVECTORPTR->HOSTTRACEQ;

		IPHOSTVECTORPTR->HOSTTRACEQ = axmsg->CHAIN;

		savemsg=axmsg;

		// Packet from AX.25

		if (axmsg->DEST[0] == 0xCF)		// Netrom
		{
			//	Could we use the More bit for fragmentation??
			//	Seems a good idea, but would'nt be compatible with NOS

			// Have to move message down buffer

			UCHAR TEMP[7];

			memmove(TEMP, &axmsg->DEST[1], 7);
			memmove(axmsg->DEST, &axmsg->ORIGIN[1], 7);
			memmove(axmsg->ORIGIN, TEMP, 7);
			axmsg->PID = 0xCC;

			memmove(&axmsg->PID + 1, &axmsg->PID + 6, axmsg->LENGTH);
			axmsg->PORT = 0;
		}

		if (CompareCalls(axmsg->ORIGIN, MYCALL))
		{
			ReleaseBuffer(axmsg);
			return 0;				// Echoed packet
		}

		switch (axmsg->PID)
		{
		case  0xcc:

			// IP Message, 

			{
				PIPMSG ipptr = (PIPMSG)++axmsg;
				axmsg--;
				ProcessIPMsg(ipptr, axmsg->ORIGIN, (axmsg->CTL == 3) ? 'D' : 'V', axmsg->PORT);
				break;
			}

		case 0xcd:
		
			// ARP Message

			ProcessAXARPMsg((PAXARP)axmsg);
			SaveARP();
			break;

		case 0x08:

			// Fragmented message

			// The L2 code ensures that the last fragment is present before passing the
			// message up to us. It is just possible that bits are missing
			{
				UCHAR * ptr = &axmsg->PID;
				UCHAR * nextfrag;

				int frags;
				int len;

				ptr++;

				if (!(*ptr & 0x80))
					break;					// Not first fragment???
				
				frags=*ptr++ & 0x7f;

				len = axmsg->LENGTH;

				len-= sizeof(BUFFHEADER);
				len--;						// Remove Frag Control Byte

				memcpy(&Buffer[EthOffset], ptr, len);

				nextfrag = &Buffer[EthOffset]+len;

				// Release Buffer
fragloop:
				ReleaseBuffer(savemsg);

				if (IPHOSTVECTORPTR->HOSTTRACEQ == 0)	goto Pollloop;		// Shouldn't happen

				axmsg = (PBUFFHEADER)IPHOSTVECTORPTR->HOSTTRACEQ;
				IPHOSTVECTORPTR->HOSTTRACEQ = axmsg->CHAIN;
				savemsg=axmsg;

				ptr = &axmsg->PID;
				ptr++;
				
				if (--frags != (*ptr++ & 0x7f))
					break;					// Out of sequence

				len = axmsg->LENGTH;

				len-= sizeof(BUFFHEADER);
				len--;						// Remove Frag Control Byte

				memcpy(nextfrag, ptr, len);

				nextfrag+=len;

				if (frags != 0) goto fragloop;

				ProcessIPMsg((PIPMSG)&Buffer[EthOffset+1],axmsg->ORIGIN, (axmsg->CTL == 3) ? 'D' : 'V', axmsg->PORT);

				break;
			}

		}
		// Release the buffer

		ReleaseBuffer(savemsg);

		goto Pollloop;
	}
	return TRUE;
}


BOOL Send_ETH(VOID * Block, DWORD len, BOOL SendtoTAP)
{
	// On Windows we don't use TAP so everything goes to pcap

#ifndef WIN32
	if (SendtoTAP)
	{
		if (tap_fd)
			write(tap_fd, Block, len);

		return TRUE;
	}
#endif

	// On Windows we don't use TAP so everything goes to pcap

	if (adhandle)
	{
//		if (len < 60) len = 60;

		// Send down the packet 

		pcap_sendpacketx(adhandle,	// Adapter
			Block,				// buffer with the packet
			len);				// size
	}
    return TRUE;
}

#define AX25_P_SEGMENT  0x08
#define SEG_REM         0x7F
#define SEG_FIRST       0x80

static VOID Send_AX_Datagram(PMESSAGE Block, DWORD Len, UCHAR Port, UCHAR * HWADDR)
{
	//	Can't use API SENDRAW, as that tries to get the semaphore, which we already have

	// Block includes the Msg Header (7 bytes), Len Does not!

	memcpy(Block->DEST, HWADDR, 7);
	memcpy(Block->ORIGIN, MYCALL, 7);
	Block->DEST[6] &= 0x7e;						// Clear End of Call
	Block->ORIGIN[6] |= 1;						// Set End of Call
	Block->CTL = 3;		//UI

#ifdef LINBPQ

	if (Port == 99)				// BPQETHER over BPQTUN Port
	{
		// Add BPQETHER Header

		int txlen = Block->LENGTH;
		UCHAR txbuff[1600];
		
		if (txlen < 1 || txlen > 400)
			return;

		// Length field is little-endian

		// BPQEther Header is 14 bytes before the Length 

		txbuff[14]=(txlen & 0xff);
		txbuff[15]=(txlen >> 8);


		memcpy(&txbuff[16],&Block[7],txlen);
		
		//memcpy(&txbuff[0],&EthDest[0],6);
		//memcpy(&txbuff[6],&EthSource[0],6);
		//memcpy(&txbuff[12],&EtherType,2);
	
		write(tap_fd, Block, Len);
		return;
	}

#endif

	Send_AX(Block, Len, Port);	
	return;

}
VOID Send_AX_Connected(VOID * Block, DWORD Len, UCHAR Port, UCHAR * HWADDR)
{
	DWORD PACLEN = 256;
	int first = 1, fragno, txlen;
	UCHAR * p;

	// Len includes the 16 byte ax header (Addr CTL PID)

	// if Len - 16 is greater than PACLEN, then fragment (only possible on Virtual Circuits,
	//	as fragmentation relies upon reliable delivery


	if ((Len - 16) <= PACLEN)		// No need to fragment
	{
		SendNetFrame(HWADDR, MYCALL, Block, Len, Port);
		return;
	}

	Len = Len-16;			// Back to real length

	PACLEN-=2;				// Allow for fragment control info)

	fragno = Len / PACLEN;

	if (Len % PACLEN == 0) fragno--;

	p = Block;
	p += 20;
	
	while (Len > 0)
	{
		*p++ = AX25_P_SEGMENT;

		*p = fragno--;
	
		if (first)
			*p |= SEG_FIRST;

		txlen = (PACLEN > Len) ? Len : PACLEN;

		Debugprintf("Send IP to VC Fragment, Len = %d", txlen);

		// Sobsequent fragments only add one byte (the PID is left in place)

		if (first)
			SendNetFrame(HWADDR, MYCALL, p-23, txlen+18, Port); 
		else
		{
			SendNetFrame(HWADDR, MYCALL, p-23, txlen+17, Port);  // only one frag byte
			p--;
		}
		first = 0;
		p += (txlen);
		Len -= txlen;
	}
	return;
}

#define MAXDATA BUFFLEN-16


static VOID SendNetFrame(UCHAR * ToCall, UCHAR * FromCall, UCHAR * Block, DWORD Len, UCHAR Port)
{
//	ATTACH FRAME TO OUTBOUND L3 QUEUES (ONLY USED FOR IP ROUTER)

	struct DATAMESSAGE * buffptr;
	struct _LINKTABLE * LINK;
	struct PORTCONTROL * PORT;

	if (Len > MAXDATA)
		return;
	
	if (QCOUNT < 100)
		return;

	memcpy(&Block[7],ToCall, 7);
	memcpy(&Block[14],FromCall, 7);

	buffptr = GetBuff();

	if (buffptr == 0)
		return;			// No buffers

	Len -= 15;			// We added 16 before (for UI Header) but L2 send includes the PID, so is one more than datalength

	buffptr->LENGTH = (USHORT)Len + MSGHDDRLEN;

//	SEE IF L2 OR L3

	if (Port == 0)				// L3
	{
		struct DEST_LIST * DEST;
		L3MESSAGE * L3Header;

		if (FindDestination(ToCall, &DEST) == 0)
		{
			ReleaseBuffer(buffptr);
			return;
		}

		// We have to build the Netrom Header

		L3Header = (L3MESSAGE *)&buffptr->L2DATA[0];

		buffptr->PID = 0xCF;			// NETROM

		memcpy(L3Header->L3SRCE, FromCall, 7);
		memcpy(L3Header->L3DEST, ToCall, 7);
		L3Header->L3TTL = L3LIVES;
		L3Header->L4FLAGS = 0;			// Opcode
		L3Header->L4ID = 0x0C;			// IP
		L3Header->L4INDEX = 0x0C;

		memcpy(L3Header->L4DATA, &Block[23], Len);	// Dond Send PID - implied by OCOC above

		buffptr->LENGTH += 20;			// Netrom Header

		C_Q_ADD(&DEST->DEST_Q, buffptr);
		return;
	}

//	SEND FRAME TO L2 DEST, CREATING A LINK ENTRY IF NECESSARY

	memcpy(&buffptr->PID, &Block[22], Len);

	if (FindLink(ToCall, FromCall, Port, &LINK))
	{
		// Have a link

		C_Q_ADD(&LINK->TX_Q, buffptr);		
		return;
	}

	if (LINK == NULL)				// No spare space
	{
		ReleaseBuffer(buffptr);
		return;
	}
	
	LINK->LINKPORT = PORT = GetPortTableEntryFromPortNum(Port);

	if (PORT == NULL)
		return;						// maybe port has been deleted

	LINK->L2TIME = PORT->PORTT1;			// SET TIMER VALUE

	if (ToCall[7])
		LINK->L2TIME += PORT->PORTT1;		// Extend Timer for Digis

	LINK->LINKWINDOW = PORT->PORTWINDOW;

	LINK->L2STATE = 2;

	memcpy(LINK->LINKCALL, ToCall, 7);
	memcpy(LINK->OURCALL, FromCall, 7);
	memcpy(LINK->DIGIS, &ToCall[7], 56);

	LINK->LINKTYPE = 2;						// Dopwnlink

	SENDSABM(LINK);

	C_Q_ADD(&LINK->TX_Q, buffptr);		
	return;
}

VOID ProcessEthIPMsg(PETHMSG Buffer)
{
	PIPMSG ipptr = (PIPMSG)&Buffer[1];
	struct nat_table_entry * NAT = NULL;
	int index;

	if (memcmp(Buffer, ourMACAddr,6 ) != 0) 
		return;		// Not for us

	if (memcmp(&Buffer[6], ourMACAddr,6 ) == 0) 
		return;		// Discard our sends

	// See if from a NAT'ed address

	for (index=0; index < nat_table_len; index++)
	{
		NAT = &nat_table[index];
					
		if (NAT->mappedipaddr == ipptr->IPSOURCE.addr)
		{
			ipptr->IPSOURCE.addr = NAT->origipaddr;

			ipptr->IPCHECKSUM = 0;		// to force cksum recalc below
			break;
		}
	}

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

	if (ipptr->IPDEST.addr == EncapAddr)
	{
		if (ipptr->IPPROTOCOL == 4)		// AMPRNET Tunnelled Packet
		{
			memcpy(RouterMac, Buffer->SOURCE, 6);
			ProcessTunnelMsg(ipptr);
		}
		return;							// Ignore Others
	}

	ProcessIPMsg(ipptr, Buffer->SOURCE, 'E', 255);
}

VOID ProcessEthARPMsg(PETHARP arpptr, BOOL FromTAP)
{
	int i=0, Mask=IPPortMask;
	PARPDATA Arp;
	PROUTEENTRY Route;
	BOOL Found;

	if (memcmp(&arpptr->MSGHDDR.SOURCE, ourMACAddr,6 ) == 0 ) 
		return;		// Discard our sends

	switch (arpptr->ARPOPCODE)
	{
	case 0x0100:

		//	Is it for our ENCAP (not on our 44 LAN)

//		printf("ARP Request for %08x Tell %08x\n",
//			arpptr->TARGETIPADDR, arpptr->SENDIPADDR);

		//  Process anything for 44 from TAP
		// (or should the be from either..???)

//		if (FromTAP && (arpptr->TARGETIPADDR & 0xff) == 44)
		if ((arpptr->TARGETIPADDR & 0xff) == 44)
			goto ARPOk;
	
		if (arpptr->TARGETIPADDR != EncapAddr)
		{
			// We should only accept requests from our subnet - we might have more than one net on iterface

			if ((arpptr->SENDIPADDR & OurNetMask) != (OurIPAddr & OurNetMask))
			{
				// Discard Unless it is from a NAT'ed Host
				
				struct nat_table_entry * NAT = NULL;
				int index;

				for (index=0; index < nat_table_len; index++)
				{
					NAT = &nat_table[index];
					
					if (NAT->mappedipaddr == arpptr->SENDIPADDR)
						break;
				}
				
				if (index >= nat_table_len)
					return;

				// Also check it is for a 44. address or we send all LAN ARPS to 
				//	RF

				if ((arpptr->TARGETIPADDR & 0xff) != 44)
					return;

			}

			if (arpptr->TARGETIPADDR == 0)		// Request for 0.0.0.0
				return;
	
		}

		// Add to our table, as we will almost certainly want to send back to it
ARPOk:
//		Debugprintf("ARP Request for %08x Tell %08x\n",
//			arpptr->TARGETIPADDR, arpptr->SENDIPADDR);


		Arp = LookupARP(arpptr->SENDIPADDR, TRUE, &Found);

		if (Found)
			goto AlreadyThere;				// Already there

		if (Arp == NULL) return;	// No point if table full
			
		Arp->IPADDR = arpptr->SENDIPADDR;
		Arp->ARPTYPE = 'E';
		Arp->ARPINTERFACE = 255;
		memcpy(Arp->HWADDR, arpptr->SENDHWADDR ,6);
		Arp->ARPVALID = TRUE;
		Arp->ARPTIMER =  ARPTIMEOUT;

		// Also add to routes

		AddToRoutes(Arp,arpptr->SENDIPADDR, 'E');

		SaveARP();
	
AlreadyThere:

		if (arpptr->TARGETIPADDR == OurIPAddr || arpptr->TARGETIPADDR == EncapAddr)
		{
			ULONG Save = arpptr->TARGETIPADDR;
 
			arpptr->ARPOPCODE = 0x0200;
			memcpy(arpptr->TARGETHWADDR, arpptr->SENDHWADDR ,6);
			memcpy(arpptr->SENDHWADDR, ourMACAddr ,6);

			arpptr->TARGETIPADDR = arpptr->SENDIPADDR;
			arpptr->SENDIPADDR = Save;

			memcpy(arpptr->MSGHDDR.DEST, arpptr->MSGHDDR.SOURCE ,6); 
			memcpy(arpptr->MSGHDDR.SOURCE, ourMACAddr ,6); 

			Debugprintf("Forus ARP Reply for %08x Targ %08x HNAT %08x\n",
				arpptr->SENDIPADDR, arpptr->TARGETIPADDR, HostNATAddr);

			Send_ETH(arpptr,42, FromTAP);

			return;
		}

		// If for our Ethernet Subnet, Ignore or we send loads of unnecessary msgs to ax.25

		// Actually our subnet could be subnetted further

		// So respond for NAT'ed addresses

		// Why not just see if we have a route first??

		Route = FindRoute(arpptr->TARGETIPADDR);

		if (Route)
		{
			if (Route->TYPE == 'T')
				goto ProxyARPReply;			// Assume we can always reach via tunnel

			Arp = LookupARP(Route->GATEWAY, FALSE, &Found);

			if (Arp)
			{
				if(Arp->ARPVALID && (Arp->ARPTYPE == 'E'))
					return;				// On LAN, so should reply direct

				goto ProxyARPReply;
			}
		}

		if ((arpptr->TARGETIPADDR & OurNetMask) == (OurIPAddr & OurNetMask))
		{
			// Unless for a NAT'ed address, in which case we reply with our virtual MAC

			struct nat_table_entry * NAT = NULL;
			int index;

			for (index=0; index < nat_table_len; index++)
			{
				NAT = &nat_table[index];
					
				if (NAT->origipaddr == arpptr->TARGETIPADDR)
					break;
			}
				
			if (index >= nat_table_len)
				return;
		
			goto ProxyARPReply;

		}
		// Should't we just reply if we know it ?? (Proxy ARP)

		//	Maybe, but that may mean dowstream nodes dont learnit

		// Sould we look in routes table, as we may have a gateway to it.

		Route = FindRoute(arpptr->TARGETIPADDR);

		if (Route)
		{
			if (Route->TYPE == 'T')
				goto ProxyARPReply;			// Assume we can always reach via tunnel

			Arp = LookupARP(Route->GATEWAY, FALSE, &Found);

			if (Arp)
			{
				if(Arp->ARPVALID && (Arp->ARPTYPE == 'E'))
					return;				// On LAN, so should reply direct
ProxyARPReply:
					ETHARPREQMSG.TARGETIPADDR = arpptr->SENDIPADDR;
					ETHARPREQMSG.ARPOPCODE = 0x0200;	// Reply
					ETHARPREQMSG.SENDIPADDR = arpptr->TARGETIPADDR;
					
					memcpy(ETHARPREQMSG.SENDHWADDR,ourMACAddr, 6);
					memcpy(ETHARPREQMSG.MSGHDDR.DEST, arpptr->SENDHWADDR, 6);

//					Debugprintf("Proxy ARP Reply for %08x Targ %08x HNAT %08x\n",
//						ETHARPREQMSG.SENDIPADDR, ETHARPREQMSG.TARGETIPADDR, HostNATAddr);
	
					//	We send to TAP if request from TAP
//					Send_ETH(&ETHARPREQMSG, 42, ETHARPREQMSG.TARGETIPADDR == HostNATAddr);
					Send_ETH(&ETHARPREQMSG, 42, FromTAP);
					return;
			}
		}

		// Not in our cache, so send to all other ports enabled for IP, reformatting as necessary

		AXARPREQMSG.TARGETIPADDR = arpptr->TARGETIPADDR;
		AXARPREQMSG.SENDIPADDR = arpptr->SENDIPADDR;
		memset(AXARPREQMSG.TARGETHWADDR, 0, 7);
		AXARPREQMSG.ARPOPCODE = 0x0100;

		for (i=1; i<=NUMBEROFPORTS; i++)
		{
			if (Mask & 1)
				Send_AX_Datagram((PMESSAGE)&AXARPREQMSG, 46, i, QST);

			Mask>>=1;
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

		// Also add to routes

		AddToRoutes(Arp, arpptr->SENDIPADDR, 'E');
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

		if (Arp && arpptr->TARGETIPADDR == OurIPAddr)		// Reply to our request?
		{
			struct DATAMESSAGE * buffptr;
			PIPMSG IPptr;

			while (Arp->ARP_Q)
			{
				buffptr = Q_REM(&Arp->ARP_Q);
				IPptr = (PIPMSG)&buffptr->L2DATA[30];
				RouteIPMsg(IPptr);
				free(buffptr);
			}

			break;
		}

		Arp = LookupARP(arpptr->TARGETIPADDR, FALSE, &Found);
				
		if (Found)
		{
			if (Arp->ARPINTERFACE == 255)
			{
				ETHARPREQMSG.TARGETIPADDR = arpptr->TARGETIPADDR;
				ETHARPREQMSG.ARPOPCODE = 0x0200;	// Reply
				ETHARPREQMSG.SENDIPADDR = arpptr->SENDIPADDR;
					
				memcpy(ETHARPREQMSG.SENDHWADDR,ourMACAddr, 6);
				memcpy(ETHARPREQMSG.MSGHDDR.DEST, Arp->HWADDR, 6);

				Send_ETH(&ETHARPREQMSG, 42, ETHARPREQMSG.TARGETIPADDR == HostNATAddr);
				return;
			}
			else
			{
				AXARPREQMSG.TARGETIPADDR = arpptr->TARGETIPADDR;
				AXARPREQMSG.ARPOPCODE = 0x0200;		// Reply
				AXARPREQMSG.SENDIPADDR = arpptr->SENDIPADDR;

				memcpy(AXARPREQMSG.SENDHWADDR, MYCALL, 7);
				memcpy(AXARPREQMSG.TARGETHWADDR, Arp->HWADDR, 7);
					
				Send_AX_Datagram((PMESSAGE)&AXARPREQMSG, 46, Arp->ARPINTERFACE, Arp->HWADDR);

				return;
			}
		}
		break;

	default:
		break;
	}
	return;
}

VOID ProcessAXARPMsg(PAXARP arpptr)
{
	int i=0, Mask=IPPortMask;
	PARPDATA Arp;
	PROUTEENTRY Route;

	BOOL Found;

	arpptr->MSGHDDR.ORIGIN[6] &= 0x7e;			// Clear end of Call

	if (memcmp(arpptr->MSGHDDR.ORIGIN, MYCALL, 7) == 0)	// ?Echoed packet?
		return;

	switch (arpptr->ARPOPCODE)
	{
	case 0x0100:
	{
		// Add to our table, as we will almost certainly want to send back to it

		if (arpptr->TARGETIPADDR == 0)
			return;							// Ignore 0.0.0.0

		Arp = LookupARP(arpptr->SENDIPADDR, TRUE, &Found);

		if (Found)
			goto AlreadyThere;				// Already there
				
		if (Arp != NULL)
		{
			//   ENTRY NOT FOUND - IF ANY SPARE ENTRIES, USE ONE

			Arp->IPADDR = arpptr->SENDIPADDR;

			memcpy(Arp->HWADDR, arpptr->SENDHWADDR ,7);

			Arp->ARPTYPE = 'D';
			Arp->ARPINTERFACE = arpptr->MSGHDDR.PORT;
			Arp->ARPVALID = TRUE;
			Arp->ARPTIMER =  ARPTIMEOUT;

			// Also add to routes

			AddToRoutes(Arp, arpptr->SENDIPADDR, 'D');
		}

AlreadyThere:

		if (arpptr->TARGETIPADDR == OurIPAddr)
		{	
			arpptr->ARPOPCODE = 0x0200;
			memcpy(arpptr->TARGETHWADDR, arpptr->SENDHWADDR, 7);
			memcpy(arpptr->SENDHWADDR, MYCALL, 7);

			arpptr->TARGETIPADDR = arpptr->SENDIPADDR;
			arpptr->SENDIPADDR = OurIPAddr;

			Send_AX_Datagram((PMESSAGE)arpptr, 46, arpptr->MSGHDDR.PORT, arpptr->MSGHDDR.ORIGIN);

			return;
		}

		// Should't we just reply if we know it ?? (Proxy ARP)

		//	Maybe, but that may mean dowstream nodes dont learnit

		// Sould we look in routes table, as we may have a gateway to it.

		Route = FindRoute(arpptr->TARGETIPADDR);

		if (Route)
		{
			if (Route->TYPE == 'T')
				goto AXProxyARPReply;			// Assume we can always reach via tunnel


		Arp = LookupARP(arpptr->TARGETIPADDR, FALSE, &Found);

		if (Found)
		{
			// if Trarget is the station we got the request from, there is a loop
			// KIll the ARP entry, and ignore

			if (memcmp(Arp->HWADDR, arpptr->SENDHWADDR, 7) == 0)
			{
				RemoveARP(Arp);
				return;
			}

AXProxyARPReply:

			AXARPREQMSG.ARPOPCODE = 0x0200;		// Reply
			AXARPREQMSG.TARGETIPADDR = arpptr->SENDIPADDR;
			AXARPREQMSG.SENDIPADDR = arpptr->TARGETIPADDR;

			memcpy(AXARPREQMSG.SENDHWADDR, MYCALL, 7);
			memcpy(AXARPREQMSG.TARGETHWADDR, arpptr->SENDHWADDR, 7);

			Send_AX_Datagram((PMESSAGE)&AXARPREQMSG, 46, arpptr->MSGHDDR.PORT, arpptr->SENDHWADDR);

			return;
		}
		}

		// Not in our cache, so send to all other ports enabled for IP, reformatting as necessary
	
		AXARPREQMSG.ARPOPCODE = 0x0100;
		AXARPREQMSG.TARGETIPADDR = arpptr->TARGETIPADDR;
		AXARPREQMSG.SENDIPADDR = arpptr->SENDIPADDR;

		for (i=1; i<=NUMBEROFPORTS; i++)
		{
			if (i != arpptr->MSGHDDR.PORT)
				if (Mask & 1)
					Send_AX_Datagram((PMESSAGE)&AXARPREQMSG, 46, i, QST);

			Mask>>=1;
		}

		memset(ETHARPREQMSG.MSGHDDR.DEST, 0xff, 6);
		memcpy(ETHARPREQMSG.MSGHDDR.SOURCE, ourMACAddr, 6);

		ETHARPREQMSG.ARPOPCODE = 0x0100;
		ETHARPREQMSG.TARGETIPADDR = arpptr->TARGETIPADDR;
		ETHARPREQMSG.SENDIPADDR = arpptr->SENDIPADDR;
		memcpy(ETHARPREQMSG.SENDHWADDR, ourMACAddr, 6);

		Send_ETH(&ETHARPREQMSG, 42, FALSE);

		break;
	}

	case 0x0200:

		// Update ARP Cache
	
		Arp = LookupARP(arpptr->SENDIPADDR, TRUE, &Found);
		
		if (Found)
			goto Update;

		if (Arp == NULL)
			goto SendBack;

		// Also add to routes

		AddToRoutes(Arp, arpptr->SENDIPADDR, 'D');
Update:
		Arp->IPADDR = arpptr->SENDIPADDR;

		memcpy(Arp->HWADDR, arpptr->SENDHWADDR ,7);
		Arp->ARPTYPE = 'D';
		Arp->ARPINTERFACE = arpptr->MSGHDDR.PORT;
		Arp->ARPVALID = TRUE;
		Arp->ARPTIMER =  ARPTIMEOUT;

SendBack:

		//  Send Back to Originator of ARP Request

		if (arpptr->TARGETIPADDR == OurIPAddr)		// Reply to our request?
			break;

		Arp = LookupARP(arpptr->TARGETIPADDR, FALSE, &Found);

		if (Found)
		{
			if (Arp->ARPINTERFACE == 255)
			{
				ETHARPREQMSG.ARPOPCODE = 0x0200;	// Reply

				ETHARPREQMSG.TARGETIPADDR = arpptr->TARGETIPADDR;
				ETHARPREQMSG.SENDIPADDR = arpptr->SENDIPADDR;

				memcpy(ETHARPREQMSG.SENDHWADDR,ourMACAddr,6);
				memcpy(ETHARPREQMSG.TARGETHWADDR, Arp->HWADDR, 6);
				memcpy(ETHARPREQMSG.MSGHDDR.DEST, Arp->HWADDR, 6);

				Send_ETH(&ETHARPREQMSG, 42, ETHARPREQMSG.TARGETIPADDR == HostNATAddr);
				return;
			}
			else
			{
				AXARPREQMSG.ARPOPCODE = 0x0200;		// Reply

				AXARPREQMSG.TARGETIPADDR = arpptr->TARGETIPADDR;
				AXARPREQMSG.SENDIPADDR = arpptr->SENDIPADDR;

				memcpy(AXARPREQMSG.SENDHWADDR, MYCALL, 7);
				memcpy(AXARPREQMSG.TARGETHWADDR, Arp->HWADDR, 7);

				Send_AX_Datagram((PMESSAGE)&AXARPREQMSG, 46, Arp->ARPINTERFACE, Arp->HWADDR);

				return;
			}
		}
		
		break;

	
	default:

		return;
	}
}

VOID ProcessIPMsg(PIPMSG IPptr, UCHAR * MACADDR, char Type, UCHAR Port)
{
	ULONG Dest;
	PARPDATA Arp;
	PROUTEENTRY Route;
	BOOL Found;
	PUDPMSG UDPptr;

	if (IPptr->VERLEN != 0x45)
	{
		Dest = IPptr->IPDEST.addr;
		Debugprintf("IP Pkt not 45 for %s\n", FormatIP(Dest));
		return;  // Only support Type = 4, Len = 20
	}
	if (!CheckIPChecksum(IPptr))
	{
		Dest = IPptr->IPDEST.addr;
		Debugprintf("IP Pkt Bad CKSUM for %s\n", FormatIP(Dest));
		return;
	}
	// Make sure origin ia routable. If not, add to ARP

	Route = FindRoute(IPptr->IPSOURCE.addr);

	if (Route == NULL)
	{
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

				// Also add to routes

				AddToRoutes(Arp, IPptr->IPSOURCE.addr, Type);

				SaveARP();
			}
		}
	}

	if (Route && Route->ARP)
		Route->ARP->ARPTIMER =  ARPTIMEOUT;				// Refresh

	// See if for us - if not pass to router

	Dest = IPptr->IPDEST.addr;
//	Debugprintf("IP Pkt for %s\n", FormatIP(Dest));

	if (Dest == OurIPAddr)
		goto ForUs;

	RouteIPMsg(IPptr);

	return;

ForUs:
	
	//	We now pass stuff addressed to us to the host, unless it is a reponse
	//	to our ping request

	//	Not sure what to do with snmp (maybe option)

//	if (IPptr->IPPROTOCOL == 4)		// AMPRNET Tunnelled Packet
//	{
//		ProcessTunnelMsg(IPptr);
//		return;
//	}

	if (IPptr->IPPROTOCOL == 1)		// ICMP
	{
		int Len;
		PICMPMSG ICMPptr = (PICMPMSG)&IPptr->Data;

		Len = ntohs(IPptr->IPLENGTH);
		Len-=20;

		Debugprintf("FORUS ICMP Type %d", ICMPptr->ICMPTYPE);

		if (Len == 28 && ICMPptr->ICMPTYPE == 0 && memcmp(ICMPptr->ICMPData, "*BPQ", 4) == 0)
		{
			// Probably our response
			
			ProcessICMPMsg(IPptr);
			return;
		}
	}

	// Support UDP for SNMP

	if (BPQSNMP && IPptr->IPPROTOCOL == 17)		// UDP
	{
		UDPptr = (PUDPMSG)&IPptr->Data;

		if (UDPptr->DESTPORT == htons(161))
		{
			ProcessSNMPMessage(IPptr);
			return;
		}
	}

	// Pass rest to host
	
	RouteIPMsg(IPptr);
	return;
}

unsigned short cksum(unsigned short *ip, int len)
{
	long sum = 0;  /* assume 32 bit long, 16 bit short */

	// if not word aligned copy

	unsigned short copy [1024];

//	if (&ip & 1)
	{
		memcpy(copy, ip, len);
		ip = copy;
	}

	while(len > 1)
	{
		sum += *(ip++);
		if(sum & 0x80000000)   /* if high order bit set, fold */
		sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	if(len)       /* take care of left over byte */
		sum += (unsigned short) *(unsigned char *)ip;
          
	while(sum>>16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return (unsigned short)sum;
}

BOOL CheckIPChecksum(PIPMSG IPptr)
{
	USHORT checksum=0;

	if (IPptr->IPCHECKSUM == 0)
		return TRUE; //Not used

	checksum = cksum((unsigned short *)IPptr, 20);

	if (checksum == 0xffff) return TRUE; else return FALSE;

}
BOOL Check_Checksum(VOID * ptr1, int Len)
{
	USHORT checksum;

	checksum = cksum((unsigned short *)ptr1, Len);

	if (checksum == 0xffff) return TRUE; else return FALSE;

}
USHORT Generate_CHECKSUM(VOID * ptr1, int Len)
{
	USHORT checksum=0;

	checksum = cksum((unsigned short *)ptr1, Len);

	return ~checksum ;
}

VOID ProcessTunnelMsg(PIPMSG IPptr)
{
	UCHAR * ptr;
	PIPMSG Outer = IPptr;			// Save tunnel header
	int Origlen;
//	int InnerLen;

	// Check header length - for now drop any message with options

	if (IPptr->VERLEN != 0x45)
		return;

	Origlen = htons(Outer->IPLENGTH);

	ptr = (UCHAR *)IPptr;
	ptr += 20;						// Skip IPIP Header
	IPptr = (PIPMSG) ptr;

	//	If we are relaying it from a DMZ host there will be antoher header

	if (IPptr->IPPROTOCOL == 4)		// IPIP
	{
		Outer = IPptr;	
		ptr = (UCHAR *)IPptr;
		ptr += 20;						// Skip IPIP Header
		IPptr = (PIPMSG) ptr;
		Origlen -= 20;
	}
		
	// First check for RIP44 Messages

	if (IPptr->IPPROTOCOL == 17)		// UDP
	{
		PUDPMSG UDPptr = (PUDPMSG)&IPptr->Data;

		if (IPptr->IPSOURCE.addr == UCSD44 && UDPptr->DESTPORT == htons(520))
		{
			ProcessRIP44Message(IPptr);
			return;
		}
	}

	// for now drop anything not from a 44 address. 

	if (IPptr->IPSOURCE.S_un_b.s_b1 != 44)
	{
		// Reply to a ping - pretty safe!

		if (IPptr->IPPROTOCOL == 1)
		{
			int Len;

			int addrlen = sizeof(struct sockaddr_in);

			PICMPMSG ICMPptr = (PICMPMSG)&IPptr->Data;

			Len = ntohs(IPptr->IPLENGTH);
			Len-=20;

			Check_Checksum(ICMPptr, Len);

			if (ICMPptr->ICMPTYPE == 8)
			{
				//	ICMP_ECHO

				ULONG Temp;
				
				ICMPptr->ICMPTYPE = 0;		// Convert to Reply

				ICMPptr->ICMPCHECKSUM = 0;

				// CHECKSUM IT
	
				ICMPptr->ICMPCHECKSUM = Generate_CHECKSUM(ICMPptr, Len);

				// Swap Dest to Origin

				Temp = IPptr->IPDEST.addr;
				IPptr->IPDEST = IPptr->IPSOURCE;
				IPptr->IPSOURCE.addr = Temp;
				IPptr->IPTTL = IPTTL;

				IPptr->IPCHECKSUM = 0;
				IPptr->IPCHECKSUM = Generate_CHECKSUM(IPptr, 20);

	//			SendIPtoEncap(IPptr, Outer->IPSOURCE.addr);

			}
			return;
		}

		return;
	}

	//	See if for us

	//	Handle Pings, our Ping responses and SNMP in BPQ
	//	pass rest to host

	if (IPptr->IPDEST.addr == OurIPAddr)
	{
		if (IPptr->IPPROTOCOL == 1)		// ICMP
		{
			int Len;
			PICMPMSG ICMPptr = (PICMPMSG)&IPptr->Data;

			Len = ntohs(IPptr->IPLENGTH);
			Len-=20;

			if (Len == 28 && ICMPptr->ICMPTYPE == 0 && memcmp(ICMPptr->ICMPData, "*BPQ", 4) == 0)
			{
				// Probably ours
			
				ProcessICMPMsg(IPptr);
				return;
			}
		}

		if (IPptr->IPPROTOCOL == 1)		// ICMP
		{
			PICMPMSG ICMPptr = (PICMPMSG)&IPptr->Data;

			if (ICMPptr->ICMPTYPE == 8)
			{
				ProcessICMPMsg(IPptr);
				return;
			}
		}

		// Support UDP for SNMP

		if (IPptr->IPPROTOCOL == 17)		// UDP
		{
			PUDPMSG UDPptr = (PUDPMSG)&IPptr->Data;

			if (UDPptr->DESTPORT == htons(161))
			{
				ProcessSNMPMessage(IPptr);
				return;
			}
		}

		// Others should be passed to our host

		// I think we can just drop through to RouteIPMsg

	}

	// I think anything else is just passed to the router

	RouteIPMsg(IPptr);		
}

VOID ProcessRIP44Message(PIPMSG IPptr)
{
	int Len;
	PUDPMSG UDPptr = (PUDPMSG)&IPptr->Data;
	PRIP2HDDR HDDR = (PRIP2HDDR)&UDPptr->UDPData;
	PRIP2ENTRY RIP2;

	Len = ntohs(IPptr->IPLENGTH);
	Len -= 20;

	if (UDPptr->CHECKSUM)
		if (Check_Checksum(UDPptr, Len) == FALSE)
			return;

	if (HDDR->Command != 2 || HDDR->Version != 2)
		return;

	RIP2 = (PRIP2ENTRY) ++HDDR;

	Len -= 12;				// UDP and RIP Headers

	if (RIP2->AddrFamily == 0xFFFF)
	{
		//	Authentication Entry

		Len -= 20;
		RIP2++;
	}

	while (Len >= 20)		// Entry LengtH
	{
		// See if already in table

		PROUTEENTRY Route;
		BOOL Found;

		// if for our subnet, ignore

		//	Actually don't need to, as we won't overwrite the preconfigured
		//	interface route

		Route = LookupRoute(RIP2->IPAddress, RIP2->Mask, TRUE, &Found);

		if (!Found)
		{
			// Add if possible

			if (Route != NULL && RIP2->Metric < 16)
			{
				Route->NETWORK = RIP2->IPAddress;
				Route->SUBNET = RIP2->Mask;
				Route->METRIC = RIP2->Metric;
				Route->Encap = RIP2->NextHop;
				Route->TYPE = 'T';
				Route->RIPTIMOUT = 3600;		// 1 hour for now
			}
		}
		else
		{
			//	Already in table

			//	Should we replace an RF route with an ENCAP route??
			//	For now, no. May make an option later
			//	Should never replace our interface routes

			if (Route->TYPE == 'T')
			{
				//	See if same Encap, and if not, is this better metric?

				//	Is this possible with RIP44??

				if (Route->Encap != RIP2->NextHop)
				{
					if (Route->METRIC >= RIP2->Metric)
					{
						// Should also change if equal, as dynamic address could have changed
				
						Route->METRIC = RIP2->Metric;
						Route->Encap = RIP2->NextHop;
					}
				}

				Route->METRIC = RIP2->Metric;

				if (RIP2->Metric >= 15)
				{
					//	HE IS TELLING US ITS UNREACHABLE - START DELETE TIMER
	
					Route->RIPTIMOUT = 0;
					if (Route->GARTIMOUT == 0)
						Route ->GARTIMOUT = 4;
				}
				else
				{
					Route->RIPTIMOUT = 3600;	// 1 hour for now
					Route->GARTIMOUT = 0;		// In case started to delete
				}
			}
		}

		Len -= 20;
		RIP2++;
	}

	SaveIPRoutes();
	qsort(RouteRecords, NumberofRoutes, 4, CompareMasks);
	LastRIP44Msg = time(NULL);
}


VOID ProcessICMPMsg(PIPMSG IPptr)
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

		// RouteIPMsg redoes checksum

		RouteIPMsg(IPptr);			// Send Back
		return;
	}

	if (ICMPptr->ICMPTYPE == 0)
	{
		//	ICMP_REPLY:

		//	It could be a reply to our request
		//	or from a our host pc

		UCHAR * BUFFER = GetBuff();
		UCHAR * ptr1;
		struct _MESSAGE * Msg;
		TRANSPORTENTRY * Session = L4TABLE;
		char IP[20];
		unsigned char work[4];

		// Internal Pings have Length 28 and Circuit Index as ID
	
		if (Len > 28 || ICMPptr->ICMPID >= MAXCIRCUITS)
		{
			// For Host

			ReleaseBuffer(BUFFER);
			RouteIPMsg(IPptr);
			return;
		}

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


VOID SendICMPMessage(PIPMSG IPptr, int Type, int Code, int P2)
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

	RouteIPMsg(IPptr);
}

VOID SendICMPTimeExceeded(PIPMSG IPptr)
{
	SendICMPMessage(IPptr, 11, 0, 0);
	return;
}

VOID SendIPtoEncap(PIPMSG IPptr, ULONG Encap)
{
	union
	{
		struct sockaddr_in txaddr;
		struct sockaddr_in6 txaddr6;
	} TXaddr = {0};

	int sent;
	int addrlen = sizeof(struct sockaddr_in6);
	int Origlen;

	TXaddr.txaddr.sin_family = AF_INET;
	Origlen = htons(IPptr->IPLENGTH);

	//	If we are using PCAP interface we have to add IPIP and MAC Headers.

	if (EncapAddr != INADDR_NONE)
	{
		UCHAR IPCopy[2048];				// Need Space to add headers
		PETHMSG Ethptr = (PETHMSG)IPCopy;
		PIPMSG Outer = (PIPMSG)&IPCopy[14];

		memset(IPCopy, 0, 34);			// Eth + IP Headers

		Outer->VERLEN = 0x45;
		Outer->IPDEST.addr = Encap;
		Outer->IPSOURCE.addr = EncapAddr;
		Outer->IPPROTOCOL = 4;
		Outer->IPTTL = IPTTL;
		Outer->IPID = IPptr->IPID;
		Outer->IPLENGTH = htons(Origlen + 20);
		memcpy(&Outer->Data, IPptr, Origlen);

		Outer->IPCHECKSUM = 0;
		Outer->IPCHECKSUM = Generate_CHECKSUM(Outer, 20);

		memcpy(Ethptr->DEST, RouterMac, 6);
		memcpy(Ethptr->SOURCE, ourMACAddr, 6);
		Ethptr->ETYPE= 0x0008;
		Send_ETH(Ethptr, Origlen + 34, FALSE);

		return;

	}
	if (UDPEncap)
	{
		UCHAR * ptr;
	
		memcpy(&TXaddr, &RXaddr, sizeof(struct sockaddr_in6));
		TXaddr.txaddr.sin_port = htons(UDPPort);

		// UDP Processor Needs the Encap Address, but we don't need the IPIP hearer
		//	as that is added by the raw send later. Just stick it on the end.


		ptr = (UCHAR *)IPptr;
		memcpy(ptr + Origlen, &Encap, 4);
		Origlen += 4;
	}
	else
		memcpy(&TXaddr.txaddr.sin_addr, &Encap, 4);
	

	sent = sendto(EncapSock, (char *)IPptr, Origlen, 0, (struct sockaddr *)&TXaddr, addrlen);
	sent = GetLastError();

}


BOOL RouteIPMsg(PIPMSG IPptr)
{
	PARPDATA Arp, ARPptr;
	PROUTEENTRY Route;
	BOOL Found;
	struct nat_table_entry * NAT = NULL;
	int index;
	BOOL SendtoTAP = FALSE;		// used on LinBPQ for NAT to This Host

	//	Decremnent TTL and Recalculate header checksum

	IPptr->IPTTL--;

	if (IPptr->IPTTL == 0)
	{
		SendICMPTimeExceeded(IPptr);
		return FALSE;	
	}

	// See if for a NATed Address

//	Debugprintf("RouteIPFrame IP %s\n", FormatIP(IPptr->IPDEST.addr));
				
	for (index=0; index < nat_table_len; index++)
	{
		NAT = &nat_table[index];
					
		// for the moment only map all ports

		if (NAT->origipaddr == IPptr->IPDEST.addr)
		{
			char Msg[80];
			int ptr;
			IPLen =  htons(IPptr->IPLENGTH);
			
			ptr = sprintf(Msg, "NAT %s to ", FormatIP(IPptr->IPDEST.addr));
			sprintf(&Msg[ptr], "%s\n", FormatIP(NAT->mappedipaddr));

			Debugprintf("%s", Msg);

			IPptr->IPDEST.addr = NAT->mappedipaddr;

			if (IPptr->IPPROTOCOL == 6)
				RecalcTCPChecksum(IPptr);
			else if (IPptr->IPPROTOCOL == 17)
				RecalcUDPChecksum(IPptr);
			else if (IPptr->IPPROTOCOL == 1)
			{
				// ICMP. If it has an inner packet (Time Exceeded or Need to Fragment)
				// we also need to un-NAT the inner packet.

				PICMPMSG ICMPptr = (PICMPMSG)&IPptr->Data;
			
				if (ICMPptr->ICMPTYPE == 3 || ICMPptr->ICMPTYPE == 11)
				{
					PIPMSG IPptr = (PIPMSG)ICMPptr->ICMPData;

					Debugprintf("NAT ICMP Unreachable or Time Exceeded %d", ICMPptr->ICMPTYPE); 
					IPptr->IPSOURCE.addr = NAT->mappedipaddr;

					// Dest could also need to be de-natted

					for (index=0; index < nat_table_len; index++)
					{
						NAT = &nat_table[index];
		
						if (NAT->mappedipaddr == IPptr->IPDEST.addr)
						{
							IPptr->IPDEST.addr = NAT->origipaddr;
							break;
						}
					}

					IPptr->IPCHECKSUM = 0;
					IPptr->IPCHECKSUM = Generate_CHECKSUM(IPptr, 20);
					ICMPptr->ICMPCHECKSUM = 0;
					ICMPptr->ICMPCHECKSUM = Generate_CHECKSUM(ICMPptr, IPLen - 20);
				}
			}


			SendtoTAP = NAT->ThisHost;
			break;
		}
	}

	if (IPptr->IPDEST.addr == HostNATAddr)
		SendtoTAP = TRUE;

	IPptr->IPCHECKSUM = 0;
	IPptr->IPCHECKSUM = Generate_CHECKSUM(IPptr, 20);

	// Everything is in the routes table, even arp-derived routes. so just look there

	Route = FindRoute(IPptr->IPDEST.addr);

	if (Route == NULL)
		return FALSE;				// ?? Dest unreachable ??

	Route->FRAMECOUNT++;

	Arp = Route->ARP;

	if (Arp == NULL)
	{
		if (Route->TYPE == 'T')
		{
			SendIPtoEncap(IPptr, Route->Encap);
			return TRUE;
		}

		// Look up target address in ARP 

		if (Route->GATEWAY)
			Arp = LookupARP(Route->GATEWAY, FALSE, &Found);
		else
			Arp = LookupARP(IPptr->IPDEST.addr, FALSE, &Found);

		if (!Found)
		{
			if (Route->GATEWAY == 0)	// Interace Route
			{	
				ARPptr = AllocARPEntry();
				if (ARPptr != NULL)
				{
					struct DATAMESSAGE * buffptr;
					Route->ARP = ARPptr;
					ARPptr->ARPROUTE = Route;
					ARPptr->ARPINTERFACE = 255;
					ARPptr->ARPTIMER = 5;
					ARPptr->ARPTYPE = 'E';
					ARPptr->IPADDR = IPptr->IPDEST.addr;

					// Save a copy to send on if ARP reply received
					
					buffptr = malloc(2048);
					
					if (buffptr)
					{
						if (ntohs(IPptr->IPLENGTH) > 1600)
						{
							Debugprintf("Overlength IP Packet %d" , ntohs(IPptr->IPLENGTH));
							return TRUE;
						}
						memcpy(&buffptr->L2DATA[30], IPptr, ntohs(IPptr->IPLENGTH));
						C_Q_ADD(&ARPptr->ARP_Q, buffptr);
					}

					SendARPMsg(ARPptr, SendtoTAP);

					return TRUE;
				}
			}
			return FALSE;
		}
	}

	if (Arp->ARPVALID)
	{
		if (Arp->ARPTYPE == 'T')
			SendIPtoEncap(IPptr, Route->Encap);
		else
			if (Arp->ARPTYPE == 'E')
				SendIPtoEther(IPptr, Arp->HWADDR, SendtoTAP);
			else
				SendIPtoAX25(IPptr, Arp->HWADDR, Arp->ARPINTERFACE, Arp->ARPTYPE);

		return TRUE;
	}
	return FALSE;	
}

VOID SendIPtoEther(PIPMSG IPptr, UCHAR * HWADDR, BOOL SendtoTAP)
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

	Send_ETH(Ethptr,Len, SendtoTAP);

	return;
}

VOID SendIPtoAX25(PIPMSG IPptr, UCHAR * HWADDR, int Port, char Mode)
{
	PBUFFHEADER Msgptr = (PBUFFHEADER)IPptr;
	int Len;
	USHORT FRAGWORD = ntohs(IPptr->FRAGWORD);
	USHORT OrigFragWord = FRAGWORD;
	int PACLEN = 256;


	(UCHAR *)Msgptr--;
	Msgptr->PID = 0xcc;		//IP

	if (Port == 0)			// NETROM
		PACLEN = 235;

	Len = ntohs(IPptr->IPLENGTH);

	// Don't fragment VC stuff

	if (Mode == 'V')		// Virtual Circuit
	{
		Send_AX_Connected((PMESSAGE)Msgptr, Len + 16, Port, HWADDR);
		return;
	}

	while (Len > PACLEN)
	{
		// Need to Frgament
		
		USHORT Fraglen;				// Max Fragment Size (PACLEN rounded down to 8 boundary))
		USHORT Datalen;				// Data Content))

		UCHAR * ptr1 = &IPptr->Data;

		//Bit 0: reserved, must be zero
		//Bit 1: (DF) 0 = May Fragment,  1 = Don't Fragment.
		//Bit 2: (MF) 0 = Last Fragment, 1 = More Fragments.
		//Fragment Offset:  13 bits

//		FRAGWORD &= 0x3fff;		// Clear Dont Fragment bit

		if (FRAGWORD & (1 << 14))
		{
			SendICMPMessage(IPptr, 3, 4, PACLEN);  // type 3 (dest unreachable), code 4 (frag needed but don't-fragment bit set))
			return;	
		}

		FRAGWORD |= (1 << 13);				// Set More Fragments bit
		IPptr->FRAGWORD = htons(FRAGWORD);

		Datalen = (PACLEN - 20) & 0xFFF8;	// Must be multiple of 8 bytes
		Fraglen = Datalen + 20;

		IPptr->IPLENGTH = htons(Fraglen);

		IPptr->IPCHECKSUM = 0;
		IPptr->IPCHECKSUM = Generate_CHECKSUM(IPptr, 20);

		// Send First Fragment

		if (Mode == 'D')		// Datagram
			Send_AX_Datagram((PMESSAGE)Msgptr, Fraglen + 16, Port, HWADDR);
		else
			Send_AX_Connected((PMESSAGE)Msgptr, Fraglen + 16, Port, HWADDR);

		// Update Header

		FRAGWORD += Datalen / 8;

		// Move Data Down the buffer

		Len -= Datalen;	
		memmove(ptr1, ptr1 + (Datalen), Len);
	}

	//	Reset Header in case we've messed with it

	IPptr->IPLENGTH = htons(Len);

	// if this started out as a fragment before we split it more,
	// we need to leave the MF bit set

	if ((OrigFragWord & 0x2000) == 0)
		FRAGWORD &= 0x5fff;		// Clear More Fragments bit

	IPptr->FRAGWORD = htons(FRAGWORD);

	IPptr->IPCHECKSUM = 0;
	IPptr->IPCHECKSUM = Generate_CHECKSUM(IPptr, 20);

	Len+=16;

	if (Mode == 'D')		// Datagram
	{
		Send_AX_Datagram((PMESSAGE)Msgptr, Len, Port, HWADDR);
		return;
	}

	Send_AX_Connected((PMESSAGE)Msgptr, Len, Port, HWADDR);
}

PROUTEENTRY AllocRouteEntry()
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


PARPDATA AllocARPEntry()
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

 VOID SendARPMsg(PARPDATA Arp, BOOL ToTAP)
 {
	//	Send ARP. Initially used only to find default gateway

	Arp->ARPTIMER =  5;							// Retry periodically

	if (Arp->ARPINTERFACE == 255)				
	{
		ETHARPREQMSG.ARPOPCODE = 0x0100;		//             ; REQUEST

		ETHARPREQMSG.TARGETIPADDR = Arp->IPADDR;						
		memset(ETHARPREQMSG.TARGETHWADDR, 0, 6);

		ETHARPREQMSG.SENDIPADDR = OurIPAddr;
		memcpy(ETHARPREQMSG.SENDHWADDR,ourMACAddr, 6);

		memcpy(ETHARPREQMSG.MSGHDDR.SOURCE, ourMACAddr, 6);
		memset(ETHARPREQMSG.MSGHDDR.DEST, 255, 6);

		Send_ETH(&ETHARPREQMSG, 42, ToTAP);

		return;
	}
	else
	{
		AXARPREQMSG.TARGETIPADDR = Arp->IPADDR;
		memset(AXARPREQMSG.TARGETHWADDR, 0, 7);
		AXARPREQMSG.ARPOPCODE = 0x0100;		//             ; REQUEST
		AXARPREQMSG.SENDIPADDR = OurIPAddr;

		memcpy(AXARPREQMSG.SENDHWADDR, MYCALL, 7);
		memcpy(AXARPREQMSG.TARGETHWADDR, Arp->HWADDR, 7);

		Send_AX_Datagram((PMESSAGE)&AXARPREQMSG, 46, Arp->ARPINTERFACE, QST);

		return;

	}
 }

PROUTEENTRY FindRoute(ULONG IPADDR)
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



PROUTEENTRY LookupRoute(ULONG IPADDR, ULONG Mask, BOOL Add, BOOL * Found)
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

PARPDATA LookupARP(ULONG IPADDR, BOOL Add, BOOL * Found)
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
VOID RemoveARP(PARPDATA Arp);

VOID RemoveRoute(PROUTEENTRY Route)
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
				if (Arp->LOCKED == 0)
					RemoveARP(Arp);
			}

			free(Route);
			NumberofRoutes--;
			return;
		}
	}
}


VOID RemoveARP(PARPDATA Arp)
{
	int i;

	while (Arp->ARP_Q)
		free(Q_REM(&Arp->ARP_Q));

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
			
				if (Route->LOCKED == 0)
					RemoveRoute(Route);
			}

			free(Arp);
			NumberofARPEntries--;
			return;
		}
	}
}

	
Dll int APIENTRY GetIPInfo(VOID * ARPRecs, VOID * IPStatsParam, int index)
{
	IPStats.ARPEntries = NumberofARPEntries;
	
	ARPFlag = index;
#ifndef LINBPQ 
	_asm {
		
		mov esi, ARPRecs
		mov DWORD PTR[ESI], offset Arp
	
		mov esi, IPStatsParam
		mov DWORD PTR[ESI], offset IPStats 
	}
#endif
	return ARPFlag;
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
	PROUTEENTRY Route;
	BOOL Found;
	char buf[256],errbuf[256];

	Config = PortConfig[33];		// Config fnom bpq32.cfg

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

			strlop(buf, ';');
			strlop(buf, '#');

			strcpy(errbuf,buf);			// save in case of error
	
			if (!ProcessLine(buf))
			{
				WritetoConsoleLocal("IP Gateway bad config record ");
				strcat(errbuf, "\n");
				WritetoConsoleLocal(errbuf);
			}
		}

		// Add an Interface Route to our LAN

		Route = LookupRoute(OurIPAddr & OurNetMask, OurNetMask, TRUE, &Found);

		if (!Found)
		{
			// Add if possible

			if (Route != NULL)
			{
				Route->NETWORK = OurIPAddr & OurNetMask;
				Route->SUBNET = OurNetMask;
			 	Route->GATEWAY = 0;
				Route->LOCKED = 1;
				Route->TYPE = 'E';
			}
		}
	}
	return TRUE;
}

static ProcessLine(char * buf)
{
	char * ptr, * p_value, * p_origport, * p_host, * p_port;
	int port, mappedport, ipad, mappedipad;
	BOOL NATTAP = FALSE;
	int i;

	ptr = strtok(buf, " \t\n\r");
	p_value = strtok(NULL, " \t\n\r");


	if(ptr == NULL) return (TRUE);

	if(*ptr =='#') return (TRUE);			// comment

	if(*ptr ==';') return (TRUE);			// comment

	if(_stricmp(ptr,"ADAPTER") == 0)
	{
//#ifndef WIN32
//		WritetoConsoleLocal("IPGating to Ethernet is not supported in this build\n");
//		return TRUE;
//#endif
		strcpy(Adapter,p_value);
		return (TRUE);
	}

	if(_stricmp(ptr,"promiscuous") == 0)
	{
		Promiscuous = atoi(p_value);
		return (TRUE);
	}

//	if(_stricmp(ptr,"USEBPQTAP") == 0)
//	{
//		WantTAP = TRUE;
//		return (TRUE);
//	}

	if (_stricmp(ptr,"44Encap") == 0)			// Enable Net44 IPIP Tunnel
	{
		WantEncap = TRUE;

		if (p_value == NULL)
			return TRUE;

		EncapAddr = inet_addr(p_value);

		strcpy(EncapAddrText, p_value);

		if (EncapAddr != INADDR_NONE)
		{
			int a,b,c,d,e,f,num;
		
			// See if MAC Specified

			p_value = strtok(NULL, " \t\n\r");

			if (p_value == NULL)
				return TRUE;
		
			num=sscanf(p_value,"%x-%x-%x-%x-%x-%x",&a,&b,&c,&d,&e,&f);

			if (num != 6) return FALSE;

			RouterMac[0]=a;
			RouterMac[1]=b;
			RouterMac[2]=c;
			RouterMac[3]=d;
			RouterMac[4]=e;
			RouterMac[5]=f;

			return TRUE;					// Normal IPIP
		}

		if (_stricmp(p_value, "UDP") == 0)
		{
			UDPEncap = TRUE;

			// look for options PORT and/or IPv6

			p_value = strtok(NULL, " \t\n\r");

			while (p_value)
			{
				if (_stricmp(p_value, "IPv6") == 0)
					IPv6 = TRUE;
				else
					if (_memicmp(p_value, "PORT=", 5) == 0)
						UDPPort = atoi(&p_value[5]);
			
				p_value = strtok(NULL, " \t\n\r");
			}
		}

		return (TRUE);
	}

	if (_stricmp(ptr,"IPAddr") == 0)
	{
		//	accept /xx as a netmask

		char * 	p_mask = strlop(p_value, '/');

		if (p_mask)
		{
			ULONG IPMask;
			int Bits = atoi(p_mask);

			if (Bits > 32)
				Bits = 32;

			if (Bits == 0)
				IPMask = 0;
			else
				IPMask = (0xFFFFFFFF) << (32 - Bits);

			OurNetMask = htonl(IPMask);			// Needs to be Network order
		}

		OurIPAddr = inet_addr(p_value);

		if (OurIPAddr == INADDR_NONE) return (FALSE);

		strcpy(IPAddrText, p_value);
		return (TRUE);
	}

//	if (_stricmp(ptr,"HostIPAddr") == 0)
//	{
//		HostIPAddr = inet_addr(p_value);

//		if (HostIPAddr == INADDR_NONE) return (FALSE);

//		strcpy(HostIPAddrText, p_value);

//		return (TRUE);
//	}

	if (_stricmp(ptr,"IPNetMask") == 0)
	{
		OurNetMask = inet_addr(p_value);

		if (strcmp(p_value, "255.255.255.255") == 0)
			return TRUE;
		
		if (OurNetMask == INADDR_NONE) return (FALSE);

		return (TRUE);
	}

	if (_stricmp(ptr,"IPPorts") == 0)
	{
		p_port = strtok(p_value, " ,\t\n\r");
		
		while (p_port != NULL)
		{
			i=atoi(p_port);
			if (i == 0) return FALSE;
			if (i > NUMBEROFPORTS) return FALSE;

			IPPortMask |= 1 << (i-1);
			p_port = strtok(NULL, " ,\t\n\r");
		}
		return (TRUE);
	}

	if (_stricmp(ptr,"NoDefaultRoute") == 0)
	{
		NoDefaultRoute = TRUE;
		return TRUE;
	}

	// ARP 44.131.4.18 GM8BPQ-7 1 D

	if (_stricmp(ptr,"ARP") == 0)
	{
		p_value[strlen(p_value)] = ' ';		// put back together
		return ProcessARPLine(p_value, TRUE);
	}

	if (_stricmp(ptr,"ROUTE") == 0)
	{
		p_value[strlen(p_value)] = ' ';		// put back together
		ProcessROUTELine(p_value, TRUE);
		return TRUE;
	}

	if (_stricmp(ptr,"NAT") == 0)
	{
		PROUTEENTRY Route;
		BOOL Found;

		if (OurIPAddr == 0)
		{
			WritetoConsoleLocal("NAT lines should follow IPAddr\n");
			return FALSE;
		}

		if (!p_value) return FALSE;
		ipad = inet_addr(p_value);
		
		if (ipad == INADDR_NONE)
			return FALSE;

		p_host = strtok(NULL, " ,\t\n\r");
		if (!p_host) return FALSE;

		mappedipad = inet_addr(p_host);
		if (mappedipad == INADDR_NONE)
			return FALSE;

		p_origport = strtok(NULL, " ,\t\n\r");

		if (p_origport && strcmp(p_origport, "TAP") == 0)
			NATTAP = TRUE;

		//		
//		//	Default is all ports
//
//		if (p_origport)
//		{
//			p_port = strtok(NULL, " ,\t\n\r");
//			if (!p_port) return FALSE;
//
//			port = atoi(p_origport);
//			mappedport=atoi(p_port);
//		}
//		else

		port = mappedport = 0;
	
#ifndef WIN32
		// on Linux, we send stuff for our host to TAP

		if (ipad == OurIPAddr || NATTAP)
			nat_table[nat_table_len].ThisHost = TRUE;
#endif
		nat_table[nat_table_len].origipaddr = ipad;
		nat_table[nat_table_len].origport = ntohs(port);
		nat_table[nat_table_len].mappedipaddr = mappedipad;
		nat_table[nat_table_len++].mappedport = ntohs(mappedport);
	
		//	Add a Host Route

		Route = LookupRoute(mappedipad, 0xffffffff, TRUE, &Found);

		if (!Found)
		{
			// Add if possible

			if (Route != NULL)
			{
				Route->NETWORK = mappedipad ;
				Route->SUBNET = 0xffffffff;
			 	Route->GATEWAY = 0;
				Route->LOCKED = 1;
				Route->TYPE = 'E';
			}
		}
		return TRUE;
	}

	if (_stricmp(ptr,"ENABLESNMP") == 0)
	{
		BPQSNMP = TRUE;
		return TRUE;
	}
	//
	//	Bad line
	//
	return FALSE;
	
}

VOID DoARPTimer()
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

//				SendARPMsg(Arp);
				RemoveARP(Arp);
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

VOID DoRouteTimer()
{
	int i;
	PROUTEENTRY Route;
	time_t NOW = time(NULL);

	for (i=0; i < NumberofRoutes; i++)
	{
		Route = RouteRecords[i];
		if (Route->RIPTIMOUT)
			Route->RIPTIMOUT--;

		if (Route->TYPE == 'T' && Route->RIPTIMOUT == 0 && Route->LOCKED == FALSE)
		{
			// Only remove Encap routes if we are still getting RIP44 messages,
			// so we can keep going if UCSD stops sending updates, but can time 
			// out entries that are removed

			if ((NOW - LastRIP44Msg) < 3600)
			{
				RemoveRoute(Route);
				return;					// Will remove all eventually
			}
		}
	}
}

// PCAP Support Code


#ifdef WIN32

FARPROCX GetAddress(char * Proc)
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

#endif

void packet_handler(u_char *param, const struct pcap_pkthdr *header, const u_char *pkt_data);

int OpenPCAP()
{
	u_long param=1;
	BOOL bcopt=TRUE;
	int i=0;
	char errbuf[PCAP_ERRBUF_SIZE];
	u_int netmask;
	char packet_filter[256];
	struct bpf_program fcode;
	char buf[256];
	int n;

#ifndef MACBPQ

	/* Open the adapter */

	adhandle= pcap_open_livex(Adapter,	// name of the device
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

	n = sprintf(packet_filter,"ether broadcast or ether dst %02x:%02x:%02x:%02x:%02x:%02x",
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
#endif	
	return TRUE;
}


VOID ReadARP()
{
	FILE *file;
	char buf[256],errbuf[256];
	
	if ((file = fopen(ARPFN,"r")) == NULL) return;
	
	while(fgets(buf, 255, file) != NULL)
	{
		strcpy(errbuf,buf);			// save in case of error
	
		if (!ProcessARPLine(buf, FALSE))
		{
			WritetoConsoleLocal("IP Gateway bad ARP record ");
			WritetoConsoleLocal(errbuf);
		}
				
	}
	
	fclose(file);

	return;
}

BOOL ProcessARPLine(char * buf, BOOL Locked)
{
	char * p_ip, * p_mac, * p_port, * p_type;
	int Port;
	char Mac[7];
	char AXCall[64];
	BOOL Stay, Spy;
	ULONG IPAddr;
	int a,b,c,d,e,f,num;		
	struct PORTCONTROL * PORT;
	
	PARPDATA Arp;
	BOOL Found;

	_strupr(buf);			// calls should be upper case

//	192.168.0.131 GM8BPQ-13 1 D

	p_ip = strtok(buf, " \t\n\r");
	p_mac = strtok(NULL, " \t\n\r");
	p_port = strtok(NULL, " \t\n\r");
	p_type = strtok(NULL, " \t\n\r");

	if(p_ip == NULL) return (TRUE);

	if(*p_ip =='#') return (TRUE);			// comment

	if(*p_ip ==';') return (TRUE);			// comment

	if (p_mac == NULL) return FALSE;

	if (p_port == NULL) return FALSE;

	if (p_type == NULL) return FALSE;

	IPAddr = inet_addr(p_ip);

	if (IPAddr == INADDR_NONE) return FALSE;

	_strupr(p_type);

	// Don't restore Eth addresses from the save file

	if (*p_type == 'E' && Locked == FALSE)
		return TRUE;

	if (!((*p_type == 'D') || (*p_type == 'E') || (*p_type =='V'))) return FALSE;

	Port=atoi(p_port);

	if (p_mac == NULL) return (FALSE);

	if (*p_type == 'E')
	{
		num=sscanf(p_mac,"%x:%x:%x:%x:%x:%x",&a,&b,&c,&d,&e,&f);

		if (num != 6) return FALSE;

		Mac[0]=a;
		Mac[1]=b;
		Mac[2]=c;
		Mac[3]=d;
		Mac[4]=e;
		Mac[5]=f;

		if (Port != 255) return FALSE;
	}
	else
	{
		if (DecodeCallString(p_mac, &Stay, &Spy, &AXCall[0]) == 0)
			return FALSE;

		if (Port == 0 && *p_type !='V')		// Port 0 for NETROM
			return FALSE;
	
		if (Port)
		{
			PORT = GetPortTableEntryFromPortNum(Port);
	
			if (PORT == NULL)
				return FALSE;
		}
	}

	Arp = LookupARP(IPAddr, TRUE, &Found);

	if (!Found)
	{
		// Add if possible

		if (Arp != NULL)
		{
			Arp->IPADDR = IPAddr;

			if (*p_type == 'E')
			{
				memcpy(Arp->HWADDR, Mac, 6);
			}
			else
			{
				memcpy(Arp->HWADDR, AXCall, 64);
				Arp->HWADDR[6] &= 0x7e;
			}
			Arp->ARPTYPE = *p_type;
			Arp->ARPINTERFACE = Port;
			Arp->ARPVALID = TRUE;
			Arp->ARPTIMER =  (Arp->ARPTYPE == 'E')? 300 : ARPTIMEOUT;
			Arp->LOCKED = Locked;

			// Also add to Routes

			AddToRoutes(Arp, IPAddr, *p_type);
			Arp->ARPROUTE->LOCKED = Locked;

		}
	}

	return TRUE;
}

VOID AddToRoutes(PARPDATA Arp, UINT IPAddr, char Type)
{
	PROUTEENTRY Route;
	BOOL Found;
	UINT IPMask = 0xffffffff;		// All ARP rerived routes are Host Routes

	Route = LookupRoute(Arp->IPADDR, IPMask, TRUE, &Found);

	if (!Found)
	{
		// Add if possible

		if (Route != NULL)
		{
			Route->NETWORK = IPAddr;
			Route->SUBNET = IPMask;
			Route->GATEWAY = IPAddr;		// Host Route
			Route->TYPE = Type;
		}
	}

	Arp->ARPROUTE = Route;
	Route->ARP = Arp;				// Crosslink Arp<>Routee

	//	Sort into reverse mask order

	qsort(RouteRecords, NumberofRoutes, 4, CompareMasks);
}

// ROUTE 44.131.4.18/32 D GM8BPQ-7 1// Datagram?netrom/VC via Call	!!!! No - this sohuld be an ARP entry

// ROUTE 44.131.4.18/32 T n.n.n.n			// Vis Tunnel Endpoint n.n.n.n
// ROUTE 44.131.4.18/32 E n.n.n.n			// Via IP address over Ethernet


int CountDots(char * Addr)
{
	int Dots = 0;

	while(*Addr)
		if (*(Addr++) == '.')
			Dots++;

	return Dots;
}


BOOL ProcessROUTELine(char * buf, BOOL Locked)
{
	char * p_ip, * p_type, * p_mask, * p_gateway;
	ULONG IPAddr, IPMask, IPGateway;
	char Type =  ' ';
	int n;	
	PROUTEENTRY Route;
	BOOL Found;

//	 ROUTE 44.131.4.18/31 44.131.4.1	// Normal Route
//	 ROUTE 44.131.4.18/31 1.2.3.4 T		// Tunnel via 1.2.3.4


	p_ip = strtok(buf, " \t\n\r");
	p_gateway = strtok(NULL, " \t\n\r");
	p_type = strtok(NULL, " \t\n\r");

	if (_stricmp(p_ip, "addprivate") == 0)
	{
		// From Encap.txt file
	
		p_ip = p_gateway;
		
		p_gateway = strtok(NULL, " \t\n\r");
		Type = 'T';
		p_type = NULL;
	}

	if(p_ip == NULL) return (TRUE);

	if(*p_ip =='#') return (TRUE);			// comment

	if(*p_ip ==';') return (TRUE);			// comment

	if (p_gateway == NULL) return FALSE;

	if (p_type)
	{
		Type = *p_type;
		if (Type == 't') Type = 'T';
	}

	p_mask = strchr(p_ip, '/');

	if (p_mask)
	{
		int Bits = atoi(p_mask + 1);

		if (Bits > 32)
			Bits = 32;

		if (Bits == 0)
			IPMask = 0;
		else
			IPMask = (0xFFFFFFFF) << (32 - Bits);
		
		*p_mask = 0;
	}
	else
		IPMask = 32;			// No mask means Host route

	IPMask = htonl(IPMask);			// Needs to be Network order

	IPGateway = inet_addr(p_gateway);

	if (IPGateway == INADDR_NONE) return FALSE;

	// The encap.txt format omits trailing zeros.
		
	n = CountDots(p_ip);
		
	if (n == 2)
		strcat(p_ip, ".0");
	else if (n == 1)
		strcat(p_ip, ".0.0");
	else if (n == 0)
		strcat(p_ip, ".0.0.0");

	IPAddr = inet_addr(p_ip);

	if (IPAddr == INADDR_NONE) return FALSE;


	Route = LookupRoute(IPAddr, IPMask, TRUE, &Found);

	if (!Found)
	{
		// Add if possible

		if (Route != NULL)
		{
			Route->NETWORK = IPAddr;
			Route->SUBNET = IPMask;

			if (Type == 'T')
				Route->Encap = IPGateway;
			else
			 	Route->GATEWAY = IPGateway;

			Route->TYPE = Type;
			Route->LOCKED = Locked;

			// Link to ARP

			Route->ARP = LookupARP(Route->GATEWAY, FALSE, &Found);
		}
	}
	return TRUE;
}


VOID SaveARP ()
{
	PARPDATA Arp;
	int i;
	FILE * file;

	if ((file = fopen(ARPFN, "w")) == NULL)
		return;

	for (i=0; i < NumberofARPEntries; i++)
	{
		Arp = ARPRecords[i];
		if (Arp->ARPVALID && !Arp->LOCKED) 
			WriteARPLine(Arp, file);
	}

 	fclose(file);
	
	return ;
}

VOID WriteARPLine(PARPDATA ARPRecord, FILE * file)
{
	int SSID, Len, j;
	char Mac[20];
	char Call[7];
	char IP[20];
	char Line[100];
	unsigned char work[4];

	memcpy(work, &ARPRecord->IPADDR, 4);

	sprintf(IP, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

	if(ARPRecord->ARPINTERFACE == 255)		// Ethernet
	{
		sprintf(Mac," %02x:%02x:%02x:%02x:%02x:%02x", 
				ARPRecord->HWADDR[0],
				ARPRecord->HWADDR[1],
				ARPRecord->HWADDR[2],
				ARPRecord->HWADDR[3],
				ARPRecord->HWADDR[4],
				ARPRecord->HWADDR[5]);
	}
	else
	{
		for (j=0; j< 6; j++)
		{
			Call[j] = ARPRecord->HWADDR[j]/2;
			if (Call[j] == 32) Call[j] = 0;
		}
		Call[6] = 0;
		SSID = (ARPRecord->HWADDR[6] & 31)/2;
			
		sprintf(Mac,"%s-%d", Call, SSID);
	}

	Len = sprintf(Line,"%s %s %d %c\n",
			IP, Mac, ARPRecord->ARPINTERFACE, ARPRecord->ARPTYPE);

	fputs(Line, file);
	
	return;
}

VOID ReadIPRoutes()
{
	PROUTEENTRY Route;
	FILE * file;
	char * Net;
	char * Nexthop;
	char * Encap;
	char * Context;
	char * Type;
	char * p_mask;

	char Line[256];

	ULONG IPAddr, IPMask = 0xffffffff, IPGateway;

	BOOL Found;

	if ((file = fopen(IPRFN, "r")) == NULL)
		return;

//	44.0.0.1/32 0.0.0.0 T 1 8 encap 169.228.66.251

	while(fgets(Line, 255, file) != NULL)
	{
		Net = strtok_s(Line, " \n", &Context);
		if (Net == 0) continue;

		if (strcmp(Net, "ENCAPMAC") == 0)
		{
			int a,b,c,d,e,f,num;
		
			num=sscanf(Context,"%x:%x:%x:%x:%x:%x",&a,&b,&c,&d,&e,&f);

			if (num == 6)
			{
				RouterMac[0]=a;
				RouterMac[1]=b;
				RouterMac[2]=c;
				RouterMac[3]=d;
				RouterMac[4]=e;
				RouterMac[5]=f;
			}
			continue;;
		}

		Nexthop = strtok_s(NULL, " \n", &Context);
		if (Nexthop == 0) continue;

		Type = strtok_s(NULL, " \n", &Context);
		if (Type == 0) continue;

		p_mask = strlop(Net, '/');

		if (p_mask)
		{
			int Bits = atoi(p_mask);

			if (Bits > 32)
				Bits = 32;

			if (Bits == 0)
				IPMask = 0;
			else
				IPMask = (0xFFFFFFFF) << (32 - Bits);

			IPMask = htonl(IPMask);			// Needs to be Network order
		}

		IPAddr = inet_addr(Net);
		if (IPAddr == INADDR_NONE) continue;

		IPGateway = inet_addr(Nexthop);
		if (IPGateway == INADDR_NONE) continue;

		if (Type[0] == 'T')
		{
			// Skip Metric, Time and "encap", get encap addr

			Encap = strtok_s(NULL, " \n", &Context);
			Encap = strtok_s(NULL, " \n", &Context);
			Encap = strtok_s(NULL, " \n", &Context);	
			Encap = strtok_s(NULL, " \n", &Context);
			
			if (Encap == 0) continue;

			IPGateway = inet_addr(Encap);
			if (IPGateway == INADDR_NONE) continue;

		}

		Route = LookupRoute(IPAddr, IPMask, TRUE, &Found);

		if (!Found)
		{
			// Add if possible

			if (Route != NULL)
			{
				Route->NETWORK = IPAddr;
				Route->SUBNET = IPMask;

				if (Type[0] == 'T')
					Route->Encap = IPGateway;
				else
			 		Route->GATEWAY = IPGateway;

				Route->TYPE = Type[0];
				Route->RIPTIMOUT = 900;

				// Link to ARP

				if (Type[0] != 'T')
					Route->ARP = LookupARP(Route->GATEWAY, FALSE, &Found);
			}
		}
	}

	fclose(file);
	return;
}

VOID SaveIPRoutes ()
{
	PROUTEENTRY Route;
	int i;
	FILE * file;
	char Line[128];

	if ((file = fopen(IPRFN, "w")) == NULL)
		return;

	// Save Gateway MAC

	sprintf(Line,"ENCAPMAC %02x:%02x:%02x:%02x:%02x:%02x\n", 
					RouterMac[0],
					RouterMac[1],
					RouterMac[2],
					RouterMac[3],
					RouterMac[4],
					RouterMac[5]);

	fputs(Line, file);

	for (i=0; i < NumberofRoutes; i++)
	{
		Route = RouteRecords[i];
		if (!Route->LOCKED) 
			WriteIPRLine(Route, file);
	}

 	fclose(file);
	
	return ;
}

VOID WriteIPRLine(PROUTEENTRY RouteRecord, FILE * file)
{
	int Len;
	char Net[20];
	char Nexthop[20];
	char Encap[20];

	char Line[100];
	unsigned char work[4];

	memcpy(work, &RouteRecord->NETWORK, 4);
	sprintf(Net, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

	memcpy(work, &RouteRecord->GATEWAY, 4);
	sprintf(Nexthop, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

	memcpy(work, &RouteRecord->Encap, 4);
	sprintf(Encap, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

	if (RouteRecord->TYPE == 'T')
		Len = sprintf(Line, "%s/%d %s %c %d %d encap %s\n",
					Net, CountBits(RouteRecord->SUBNET),
					Nexthop, RouteRecord->TYPE,
					RouteRecord->METRIC, RouteRecord->RIPTIMOUT, Encap);
	else
		Len = sprintf(Line, "%s/%d %s %c %d %d\n",
					Net, CountBits(RouteRecord->SUBNET),
					Nexthop, RouteRecord->TYPE,
					RouteRecord->METRIC, RouteRecord->RIPTIMOUT);

	fputs(Line, file);
	
	return;
}





int CheckSumAndSend(PIPMSG IPptr, PTCPMSG TCPmsg, USHORT Len)
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

	RouteIPMsg(IPptr);
	return 0;
}

int CheckSumAndSendUDP(PIPMSG IPptr, PUDPMSG UDPmsg, USHORT Len)
{
	struct _IPMSG PH = {0};
		
	IPptr->IPCHECKSUM = 0;

	PH.IPPROTOCOL = 17;
	PH.IPLENGTH = htons(Len);
	memcpy(&PH.IPSOURCE, &IPptr->IPSOURCE, 4);
	memcpy(&PH.IPDEST, &IPptr->IPDEST, 4);

	UDPmsg->CHECKSUM = ~Generate_CHECKSUM(&PH, 20);

	UDPmsg->CHECKSUM = Generate_CHECKSUM(UDPmsg, Len);

	// No need to do IP checksum as ROuteIPMessage doesit

	// CHECKSUM IT

//	IPptr->IPCHECKSUM = Generate_CHECKSUM(IPptr, 20);

	RouteIPMsg(IPptr);
	return 0;
}

VOID RecalcTCPChecksum(PIPMSG IPptr)
{
	PTCPMSG	TCPptr = (PTCPMSG)&IPptr->Data;
	PHEADER PH = {0};
	USHORT Len = ntohs(IPptr->IPLENGTH);
	Len-=20;

	PH.IPPROTOCOL = 6;
	PH.LENGTH = htons(Len);
	memcpy(&PH.IPSOURCE, &IPptr->IPSOURCE, 4);
	memcpy(&PH.IPDEST, &IPptr->IPDEST, 4);

	TCPptr->CHECKSUM = ~Generate_CHECKSUM(&PH, 12);
	TCPptr->CHECKSUM = Generate_CHECKSUM(TCPptr, Len);
}

VOID RecalcUDPChecksum(PIPMSG IPptr)
{
	PUDPMSG UDPmsg = (PUDPMSG)&IPptr->Data;
	PHEADER PH = {0};
	USHORT Len = ntohs(IPptr->IPLENGTH);
	Len-=20;

	PH.IPPROTOCOL = 17;
	PH.LENGTH = htons(Len);
	memcpy(&PH.IPSOURCE, &IPptr->IPSOURCE, 4);
	memcpy(&PH.IPDEST, &IPptr->IPDEST, 4);

	UDPmsg->CHECKSUM = ~Generate_CHECKSUM(&PH, 12);
	UDPmsg->CHECKSUM = Generate_CHECKSUM(UDPmsg, Len);
}

#ifndef WIN32
#ifndef MACBPQ

#include <net/if.h>
#include <linux/if_tun.h>

/* buffer for reading from tun/tap interface, must be >= 1500 */


#define BUFSIZE 2000   

/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            must reserve enough space in *dev.                          *
 **************************************************************************/
int tun_alloc(char *dev, int flags) {

  struct ifreq ifr;
  int fd, err;
  char *clonedev = "/dev/net/tun";

  if( (fd = open(clonedev , O_RDWR)) < 0 ) {
    perror("Opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;

  if (*dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
    perror("ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);
  return fd;
}

#include <net/if_arp.h>
#include <net/route.h> 
 

void OpenTAP()
{
	int flags = IFF_TAP;
	char if_name[IFNAMSIZ] = "LinBPQTAP";
	struct arpreq arpreq;
	int s;
	struct ifreq ifr;
	int sockfd;
	struct rtentry rm;
	int err;
	int n;

	uint nread, nwrite, plength;
	char buffer[BUFSIZE];

	int optval = 1;

	struct nat_table_entry * NAT = NULL;
	int index;

	/* initialize tun/tap interface */

	if ((tap_fd = tun_alloc(if_name, flags | IFF_NO_PI)) < 0 )
	{
		printf("Error connecting to tun/tap interface %s!\n", if_name);
		tap_fd = 0;
		return;
	}
	
	printf("Successfully connected to TAP interface %s\n", if_name);

	ioctl(tap_fd, FIONBIO, &optval);

	// Bring it up

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd < 0)
	{
		perror ("Socket");
		return;
	}

	memset(&ifr, 0, sizeof ifr);
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_flags |= IFF_UP;

	if ((err = ioctl(sockfd, SIOCSIFFLAGS, &ifr)) < 0)
	{
		perror("SIOCSIFFLAGS");
		printf("SIOCSIFFLAGS failed , ret->%d\n",err);
		return;
	}

	printf("TAP brought up\n");

	// Set MTU to 256

	memset(&ifr, 0, sizeof ifr);
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_addr.sa_family = AF_INET;
	ifr.ifr_mtu = 256;
	
	if (ioctl(sockfd, SIOCSIFMTU, (caddr_t)&ifr) < 0)
		perror("Set MTU");
	else
		printf("TAP MTU set to 256\n");

	if (NoDefaultRoute == FALSE)
	{
		// Add a Route for 44/8 via TAP

		memset(&rm, 0, sizeof(rm));

		(( struct sockaddr_in*)&rm.rt_dst)->sin_family = AF_INET;
		(( struct sockaddr_in*)&rm.rt_dst)->sin_addr.s_addr = inet_addr("44.0.0.0");
		(( struct sockaddr_in*)&rm.rt_dst)->sin_port = 0;

		(( struct sockaddr_in*)&rm.rt_genmask)->sin_family = AF_INET;
		(( struct sockaddr_in*)&rm.rt_genmask)->sin_addr.s_addr = inet_addr("255.0.0.0");
		(( struct sockaddr_in*)&rm.rt_genmask)->sin_port = 0;
	
		(( struct sockaddr_in*)&rm.rt_gateway)->sin_family = AF_INET;
		(( struct sockaddr_in*)&rm.rt_gateway)->sin_addr.s_addr = 0; //inet_addr("192.168.17.1");
		(( struct sockaddr_in*)&rm.rt_gateway)->sin_port = 0;
	
		rm.rt_dev = if_name;

		rm.rt_flags = RTF_UP; // | RTF_GATEWAY;
	
		if ((err = ioctl(sockfd, SIOCADDRT, &rm)) < 0)
		{
			perror("SIOCADDRT");
			printf("SIOCADDRT failed , ret->%d\n",err);
			return;
		}
		printf("Route to 44/8 added via LinBPQTAP\n");
	}

	// Set up ARP entries for any virtual hosts (eg jnos)

	bzero((caddr_t)&arpreq, sizeof(arpreq));

	for (index=0; index < nat_table_len; index++)
	{
	    struct sockaddr_in *psin;
		psin = (struct sockaddr_in *)&arpreq.arp_pa;
		NAT = &nat_table[index];
					
		if (NAT->ThisHost && OurIPAddr != NAT->origipaddr)
		{
			printf("Adding ARP for %s\n", FormatIP(NAT->mappedipaddr));
	
			psin->sin_family = AF_INET;
		    psin->sin_addr.s_addr = NAT->mappedipaddr;

			arpreq.arp_flags =  ATF_PERM | ATF_COM | ATF_PUBL;
			strcpy(arpreq.arp_dev, "LinBPQTAP");

			if (ioctl(sockfd, SIOCSARP, (caddr_t)&arpreq) < 0)
				perror("ARP IOCTL");
		}
	}
	
	//	Create LinBPQ ARP entry for real IP Address

	//	Get Address

	struct ifreq xbuffer;

    memset(&xbuffer, 0x00, sizeof(xbuffer));

    strcpy(xbuffer.ifr_name, "LinBPQTAP");

    ioctl(sockfd, SIOCGIFHWADDR, &xbuffer);

	PARPDATA Arp;
	PROUTEENTRY Route;
	BOOL Found;

	Arp = LookupARP(HostNATAddr, TRUE, &Found);

	if (Arp != NULL)
	{
		Arp->IPADDR = HostNATAddr;
		memcpy(Arp->HWADDR, xbuffer.ifr_hwaddr.sa_data, 6);

		Arp->ARPTYPE = 'E';
		Arp->ARPINTERFACE = 255;
		Arp->ARPVALID = TRUE;
		Arp->ARPTIMER = 0;
		Arp->LOCKED = TRUE;

		// Also add to Routes

		AddToRoutes(Arp, HostNATAddr, 'E');
		Arp->ARPROUTE->LOCKED = TRUE;
	}

	close(sockfd);
}
#endif
#endif

extern struct DATAMESSAGE * REPLYBUFFER;

VOID PING(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	// Send ICMP Echo Request

	ULONG PingAddr;
	UCHAR Msg[120] = "";
	PIPMSG IPptr = (PIPMSG)&Msg[40];		// Space for frame header (not used)
	PICMPMSG ICMPptr = (PICMPMSG)&IPptr->Data;
	time_t NOW = time(NULL);

	Bufferptr += sprintf(Bufferptr, "\r");

	if (IPRequired == FALSE)
	{
		Bufferptr += sprintf(Bufferptr, "IP Gateway is not enabled\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	PingAddr = inet_addr(CmdTail);

	if (PingAddr == INADDR_NONE)
	{
		Bufferptr += sprintf(Bufferptr, "Invalid Address\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	// We keep the message pretty short in case running over RF
	// Send "*BPQPINGID*, then Timestamp (Secs), then padding to 20 bytes
	// So we can use same address for host we examine ping responses for 
	// the pattern, and intercept ours

	IPptr->VERLEN = 0x45;
	IPptr->IPDEST.addr = PingAddr;		
	IPptr->IPSOURCE.addr = OurIPAddr;
	IPptr->IPPROTOCOL = ICMP;
	IPptr->IPTTL = IPTTL;
	IPptr->FRAGWORD = 0;
	IPptr->IPLENGTH = htons(48);			// IP Header ICMP Header 20 Data

	ICMPptr->ICMPTYPE = 8;
	ICMPptr->ICMPID = Session->CIRCUITINDEX;
	strcpy(ICMPptr->ICMPData, "*BPQPINGID*");	// 12 including null
	memcpy(&ICMPptr->ICMPData[12], &NOW, 4);

	ICMPptr->ICMPCHECKSUM = Generate_CHECKSUM(ICMPptr, 28);

	if (RouteIPMsg(IPptr))	
		Bufferptr += sprintf(Bufferptr, "OK\r");
	else
		Bufferptr += sprintf(Bufferptr, "No Route to Host\r");

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
	
	return;
}

VOID SHOWARP(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	DISPLAY IP Gateway ARP status or Clear
	
	int i;
	PARPDATA ARPRecord, Arp;
	int SSID, j, n;
	char Mac[128];
	char Call[7];
	char IP[20];
	unsigned char work[4];

	Bufferptr += sprintf(Bufferptr, "\r");

	if (IPRequired == FALSE)
	{
		Bufferptr += sprintf(Bufferptr, "IP Gateway is not enabled\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	if (memcmp(CmdTail, "CLEAR ", 6) == 0)
	{
		int n = NumberofARPEntries;
		int rec = 0;

		for (i=0; i < n; i++)
		{
			Arp = ARPRecords[rec];
			if (Arp->LOCKED)
				rec++;
			else
				RemoveARP(Arp);
		}

		Bufferptr += sprintf(Bufferptr, "OK\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		SaveARP();

		return;
	}

	for (i=0; i < NumberofARPEntries; i++)
	{
		ARPRecord = ARPRecords[i];

//		if (ARPRecord->ARPVALID)
		{
			Bufferptr = CHECKBUFFER(Session, Bufferptr);	// ENSURE ROOM
			memcpy(work, &ARPRecord->IPADDR, 4);
			sprintf(IP, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

			if(ARPRecord->ARPINTERFACE == 255)		// Ethernet
			{
				sprintf(Mac," %02x:%02x:%02x:%02x:%02x:%02x", 
					ARPRecord->HWADDR[0],
					ARPRecord->HWADDR[1],
					ARPRecord->HWADDR[2],
					ARPRecord->HWADDR[3],
					ARPRecord->HWADDR[4],
					ARPRecord->HWADDR[5]);
			}
			else
			{
				UCHAR * AXCall = &ARPRecord->HWADDR[0];
				n = 0;

				while (AXCall[0])
				{

					for (j=0; j< 6; j++)
					{
						Call[j] = AXCall[j]/2;
						if (Call[j] == 32) Call[j] = 0;
					}

					Call[j] = 0;
					SSID = (AXCall[6] & 31)/2;
			
					if (SSID)
						n += sprintf(&Mac[n], " %s-%d", Call, SSID);
					else
						n += sprintf(&Mac[n], " %s", Call);
				
					AXCall += 7;
				}
			}
			Bufferptr += sprintf(Bufferptr, "%s%s %d %c %d %s\r",
				IP, Mac, ARPRecord->ARPINTERFACE, ARPRecord->ARPTYPE,
				(int)ARPRecord->ARPTIMER, ARPRecord->LOCKED?"Locked":"");
		}
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

VOID SHOWNAT(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	DISPLAY IP Gateway ARP status or Clear
	
	struct nat_table_entry * NAT = NULL;
	int index;
	char From[20];
	char To[20];
				
	Bufferptr += sprintf(Bufferptr, "\r");

	if (IPRequired == FALSE)
	{
		Bufferptr += sprintf(Bufferptr, "IP Gateway is not enabled\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	for (index=0; index < nat_table_len; index++)
	{
		NAT = &nat_table[index];
					
		strcpy(From, FormatIP(NAT->origipaddr));
		strcpy(To, FormatIP(NAT->mappedipaddr));
		
		Bufferptr = CHECKBUFFER(Session, Bufferptr);	// ENSURE ROOM

#ifdef LINBPQ
		Bufferptr += sprintf(Bufferptr, "%s to %s %s\r", From, To,
			NAT->ThisHost?"via TAP":"");
#else
		Bufferptr += sprintf(Bufferptr, "%s to %s\r", From, To);
#endif
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
}

int CountBits(unsigned long in)
{
	int n = 0;
	while (in)
	{
		if (in & 1) n ++;
		in >>=1;
	}
	return n;
}

VOID SHOWIPROUTE(TRANSPORTENTRY * Session, char * Bufferptr, char * CmdTail, CMDX * CMD)
{
	//	DISPLAY IP Gateway ARP status or Clear

	int i;
	PROUTEENTRY RouteRecord;
	char Net[20];
	char Nexthop[20];
	char Encap[20];
	char *Context;
	char Reply[128];
	char UCReply[128];

	unsigned char work[4];

	if (IPRequired == FALSE)
	{
		Bufferptr += sprintf(Bufferptr, "\rIP Gateway is not enabled\r");
		SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);
		return;
	}

	Bufferptr += sprintf(Bufferptr, "%d Entries\r", NumberofRoutes);

	if (NumberofRoutes)
		qsort(RouteRecords, NumberofRoutes, 4, CompareRoutes);

	for (i=0; i < NumberofRoutes; i++)
	{
		RouteRecord = RouteRecords[i];

//		if (RouteRecord->ARPVALID)
		{
			Bufferptr = CHECKBUFFER(Session, Bufferptr);	// ENSURE ROOM
			memcpy(work, &RouteRecord->NETWORK, 4);
			sprintf(Net, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

			memcpy(work, &RouteRecord->GATEWAY, 4);
			sprintf(Nexthop, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

			memcpy(work, &RouteRecord->Encap, 4);
			sprintf(Encap, "%d.%d.%d.%d", work[0], work[1], work[2], work[3]);

			if (RouteRecord->TYPE == 'T')
				sprintf(Reply, "%s/%d %d %c %d %d encap %s\r",
					Net, CountBits(RouteRecord->SUBNET),
					RouteRecord->FRAMECOUNT, RouteRecord->TYPE,
					RouteRecord->METRIC, RouteRecord->RIPTIMOUT, Encap);
			else
				sprintf(Reply, "%s/%d %d %s %c %d %d %s\r",
					Net, CountBits(RouteRecord->SUBNET),
					RouteRecord->FRAMECOUNT, Nexthop, RouteRecord->TYPE,
					RouteRecord->METRIC, RouteRecord->RIPTIMOUT,
					RouteRecord->LOCKED?"Locked":"");
		}

		// Treat any parameter as a "Find Filter"

		CmdTail = strtok_s(CmdTail, " ", &Context);
		strcpy(UCReply, Reply);
		_strupr(UCReply);

		if (CmdTail && CmdTail[0] && strstr(UCReply, CmdTail) == 0)
			continue;
	
		Bufferptr += sprintf(Bufferptr, "%s", Reply);
	}

	SendCommandReply(Session, REPLYBUFFER, Bufferptr - (char *)REPLYBUFFER);

	if (NumberofRoutes)
		qsort(RouteRecords, NumberofRoutes, 4, CompareMasks);		// Back to Maks order

}

// SNMP Support Code. Pretty limited - basically just for MRTG

/*
Primitive ASN.1 Types	Identifier in hex
INTEGER	02
BIT STRING	03
OCTET STRING	04
NULL	05
OBJECT IDENTIFIER	06

Constructed ASN.1 type	Identifier in hex
SEQUENCE	30

Primitive SNMP application types	Identifier in hex
IpAddress	40
Opaque	44
NsapAddress	45
Counter64 (available only in SNMPv2)	46
Uinteger32 (available only in SNMPv2)	47

Context-specific types within an SNMP Message	Identifier in hex
GetRequest-PDU	A0
GetNextRequestPUD	A1
GetResponse-PDU (Response-PDU in SNMPv 2)	A2
SetRequest-PDU	A3
Trap-PDU (obsolete in SNMPv 2)	A4
GetBulkRequest-PDU (added in SNMPv 2)	A5
InformRequest-PDU (added in SNMPv 2)	A6
SNMPv2-Trap-PDU (added in SNMPv 2)	A7

*/

#define Counter32 0x41
#define Gauge32 0x42
#define TimeTicks	0x43



UCHAR ifInOctets[] = {'+',6,1,2,1,2,2,1,10};
UCHAR ifOutOctets[] = {'+',6,1,2,1,2,2,1,16};

int ifInOctetsLen = 9;		// Not Inc  Port
int ifOutOctetsLen = 9;		// Not Inc  Port

UCHAR sysUpTime[] = {'+', 6,1,2,1,1,3,0};
int sysUpTimeLen = 8;

UCHAR sysName[] = {'+', 6,1,2,1,1,5,0};
int sysNameLen = 8;

extern time_t TimeLoaded;

int InOctets[32] = {0};
int OutOctets[32] = {0};

//	ASN PDUs have to be constructed backwards, as each header included a length

//	This code assumes we have enough space in front of the buffer.

int ASNGetInt(UCHAR * Msg, int Len)
{
	int Val = 0;

	while(Len)
	{
		Val = (Val << 8) + *(Msg++);
		Len --;
	}

	return Val;
}


int ASNPutInt(UCHAR * Buffer, int Offset, unsigned int Val, int Type)
{
	int Len = 0;
	
	// Encode in minimum space. But servers seem to sign-extend top byte, so if top bit set add another zero;

	while(Val)
	{
		Buffer[--Offset] = Val & 255;	// Value
		Val = Val >> 8;
		Len++;
	}

	if (Len < 4 && (Buffer[Offset] & 0x80))	// Negative
	{
		Buffer[--Offset] = 0;
		Len ++;
	}

	Buffer[--Offset] = Len;				// Len
	Buffer[--Offset] = Type;

	return Len + 2;
}


int AddHeader(UCHAR * Buffer, int Offset, UCHAR Type, int Length)
{
	Buffer[Offset - 2] = Type;
	Buffer[Offset - 1] = Length;

	return 2;
}

int BuildReply(UCHAR * Buffer, int Offset, UCHAR * OID, int OIDLen, UCHAR * Value, int ReqID)
{
	int IDLen;
	int ValLen = Value[1] + 2;

	// Value is pre-encoded = type, len, data

	// Contruct the Varbindings. Sequence OID Value

	Offset -= ValLen;
	memcpy(&Buffer[Offset], Value, ValLen);
	Offset -= OIDLen;
	memcpy(&Buffer[Offset], OID, OIDLen);
	Buffer[--Offset] = OIDLen;
	Buffer[--Offset] = 6;				// OID Type

	Buffer[--Offset] = OIDLen + ValLen + 2;
	Buffer[--Offset] = 48;				// Sequence

	Buffer[--Offset] = OIDLen + ValLen + 4;
	Buffer[--Offset] = 48;				// Sequence


	// Add the error fields (two zero ints

	Buffer[--Offset] = 0;				// Value
	Buffer[--Offset] = 1;				// Len
	Buffer[--Offset] = 2;				// Int

	Buffer[--Offset] = 0;				// Value
	Buffer[--Offset] = 1;				// Len
	Buffer[--Offset] = 2;				// Int

	// ID

	IDLen = ASNPutInt(Buffer, Offset, ReqID, 2);
	Offset -= IDLen;

	// PDU Type

	Buffer[--Offset] = OIDLen + ValLen + 12 + IDLen;
	Buffer[--Offset] = 0xA2;				// Len

	return OIDLen + ValLen + 14 + IDLen;
}



//   snmpget -v1 -c jnos [ve4klm.ampr.org | www.langelaar.net] 1.3.6.1.2.1.2.2.1.16.5


VOID ProcessSNMPMessage(PIPMSG IPptr)
{
	int Len;
	PUDPMSG UDPptr = (PUDPMSG)&IPptr->Data;
	char Community[256];
	UCHAR OID[256];
	int OIDLen;
	UCHAR * Msg;
	int Type;
	int Length, ComLen;
	int  IntVal;
	int ReqID;
	int RequestType;

	Len = ntohs(IPptr->IPLENGTH);
	Len-=20;

	Check_Checksum(UDPptr, Len);

	// 4 bytes version
	// Null Terminated Community

	Msg = (char *) UDPptr;

	Msg += 8;				// Over UDP Header
	Len -= 8;

	// ASN 1 Encoding - Type, Len, Data

	while (Len > 0)
	{
		Type = *(Msg++);
		Length = *(Msg++);

		// First should be a Sequence

		if (Type != 0x30)
			return;

		Len -= 2;

		Type = *(Msg++);
		Length = *(Msg++);
		IntVal =  *(Msg++);

		// Should be Integer - SNMP Version - We support V1, identified by zero

		if (Type != 2 || Length != 1 || IntVal != 0)
			return;

		Len -= 3;

		Type = *(Msg++);
		ComLen = *(Msg++);

		// Should  Be String (community)

		if (Type != 4)
			return;

		memcpy(Community, Msg, ComLen);
		Community[ComLen] = 0;

		Len -=2;				// Header
		Len -= ComLen;

		Msg += (ComLen);

		// A Complex Data Types - GetRequest PDU etc

		RequestType = *(Msg);
		*(Msg++) = 0xA2;
		Length = *(Msg++);

		Len -= 2; 

		// A 2 byte value requestid

		// Next is integer requestid

		Type = *(Msg++);
		Length = *(Msg++);

		if (Type != 2)
			return;

		ReqID = ASNGetInt(Msg, Length);
 
		Len -= (2 + Length);
		Msg += Length;

		// Two more Integers - error status, error index
	
		Type = *(Msg++);
		Length = *(Msg++);

		if (Type != 2)
			return;

		ASNGetInt(Msg, Length);
 
		Len -= (2 + Length);
		Msg += Length;

		Type = *(Msg++);
		Length = *(Msg++);

		if (Type != 2)
			return;

		ASNGetInt(Msg, Length);
 
		Len -= (2 + Length);
		Msg += Length;

		// Two Variable-bindings structs - another Sequence

		Type = *(Msg++);
		Length = *(Msg++);

		Len -= 2;

		if (Type != 0x30)
			return;

		Type = *(Msg++);
		Length = *(Msg++);

		Len -= 2;

		if (Type != 0x30)
			return;

		// Next is OID 

		Type = *(Msg++);
		Length = *(Msg++);

		if (Type != 6)				// Object ID
			return;

		memcpy(OID, Msg, Length);
		OID[Length] = 0;

		OIDLen = Length;

		Len -=2;				// Header
		Len -= Length;
	
		Msg += Length;

		// Should Just have a null value left
		
		Type = *(Msg++);
		Length = *(Msg++);

		if (Type != 5 || Length != 0)
			return;

		Len -=2;				// Header

		// Should be nothing left
	}

	if (RequestType = 160)
	{
		UCHAR Reply[256];
		int Offset = 255;
		int PDULen, SendLen;
		char Value[256];
		int ValLen;
		
		//	Only Support Get

		if (memcmp(OID, sysName, sysNameLen) == 0)
		{
			ValLen = strlen(MYNODECALL);;
			Value[0] = 4;		// String
			Value[1] = ValLen;
			memcpy(&Value[2], MYNODECALL, ValLen);  
			
			PDULen = BuildReply(Reply, Offset, sysName, sysNameLen, Value, ReqID);
		}
		else if (memcmp(OID, sysUpTime, sysUpTimeLen) == 0)
		{
			int ValOffset = 10;
			ValLen = ASNPutInt(Value, ValOffset, (time(NULL) - TimeLoaded) * 100, TimeTicks);
			ValOffset -= ValLen;

			PDULen = BuildReply(Reply, Offset, sysUpTime, sysUpTimeLen, &Value[ValOffset], ReqID);
		}
		else if (memcmp(OID, ifOutOctets, ifOutOctetsLen) == 0)
		{
			int Port = OID[9];
			int ValOffset = 10;
			ValLen = ASNPutInt(Value, ValOffset, OutOctets[Port], Counter32);
			ValOffset -= ValLen;
			PDULen = BuildReply(Reply, Offset, OID, OIDLen, &Value[ValOffset], ReqID);

		}
		else if (memcmp(OID, ifInOctets, ifInOctetsLen) == 0)
		{
			int Port = OID[9];
			int ValOffset = 10;
			ValLen = ASNPutInt(Value, ValOffset, InOctets[Port], Counter32);
			ValOffset -= ValLen;
			PDULen = BuildReply(Reply, Offset, OID, OIDLen, &Value[ValOffset], ReqID);

		}
		else
			return;

		Offset -= PDULen;
		Offset -= ComLen;

		memcpy(&Reply[Offset], Community, ComLen);
		Reply[--Offset] = ComLen;
		Reply[--Offset] = 4;

		// Version

		Reply[--Offset] = 0;
		Reply[--Offset] = 1;
		Reply[--Offset] = 2;

		Reply[--Offset] = PDULen + ComLen + 5;
		Reply[--Offset] = 48;

		SendLen = PDULen + ComLen + 7;

		memcpy(UDPptr->UDPData, &Reply[Offset], SendLen);

		// Swap Dest to Origin

		IPptr->IPDEST = IPptr->IPSOURCE;

		IPptr->IPSOURCE.addr = OurIPAddr;

		UDPptr->DESTPORT = UDPptr->SOURCEPORT;
		UDPptr->SOURCEPORT = htons(161);
		SendLen += 8;			// UDP Header
		UDPptr->LENGTH = htons(SendLen);
		IPptr->IPLENGTH = htons(SendLen + 20);

		CheckSumAndSendUDP(IPptr, UDPptr, SendLen);
	}

	// Ingnore others
}

