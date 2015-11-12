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

//
//	DLL to provide BPQEther support for G8BPQ switch in a Linux environment,

// Normally uses a Raw socket, but that can't send to other apps on same machine.
// so can use a TAP device instead (or maybe as well??)

#include <stdio.h>

#include "CHeaders.h"

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

extern char * PortConfig[33];

extern int tap_fd;

typedef struct PCAPStruct
{
   UCHAR	EthSource[6];
   UCHAR	EthDest[6];
   short	EtherType;
   BOOL		RLITX;
   BOOL		RLIRX;
   BOOL		Promiscuous;
	
   int s; /*socketdescriptor*/
	struct sockaddr_ll socket_address;	/*target address*/

} PCAPINFO, *PPCAPINFO ;

PCAPINFO PCAPInfo[32];

// on linux default to broadcast

short udpport=0;

unsigned int OurInst = 0;

BOOL GotMsg;

DWORD n;

char Adapter[256];

static BOOL ReadConfigFile(int Port);
static int ProcessLine(char * buf,int Port, BOOL CheckPort);


int ExtProc(int fn, int port,unsigned char * buff)
{
	int len,txlen=0,res;
	char txbuff[500];
	unsigned char rxbuff[1600];
	PCAPINFO * IF = &PCAPInfo[port];

	if (IF->s == 0)
		return 0;

	switch (fn)
	{
	case 1:				// poll

		res = recvfrom(IF->s, rxbuff, ETH_FRAME_LEN, 0, NULL, NULL);

		if (res == -1)
		{
			if (errno == 11)
				return 0;	//Resource temporarily unavailable
		
			perror("Eth RX");
			return 0;
		}

		if (res == 0)
			/* Timeout elapsed */
			return 0;

		if (rxbuff[13] != 0xff)
			return 0;

		if (IF->RLIRX)
		
		//	RLI MODE - An extra 3 bytes before len, seem to be 00 00 41

		{
			len=rxbuff[18]*256 + rxbuff[17];

			if ((len < 16) || (len > 320)) return 0; // Probably BPQ Mode Frame

			len-=3;
		
			memcpy(&buff[7],&rxbuff[19],len);
		
			len+=5;
		}
		else
		{
			len=rxbuff[15]*256 + rxbuff[14];

			if ((len < 16) || (len > 320)) return 0; // Probably RLI Mode Frame

			len-=3;
		
			memcpy(&buff[7],&rxbuff[16],len);
		
			len+=5;
		}

		buff[5]=(len & 0xff);
		buff[6]=(len >> 8);
		
		return 1;

		
	case 2:				// send
		
 		if (IF->RLITX)
		
		//	RLI MODE - An extra 3 bytes before len, seem to be 00 00 41

		{
			txlen=(buff[6]<<8) + buff[5];		// BPQEther is DOS-based - chain word is 2 bytes

			txlen-=2;
			txbuff[16]=0x41;
			txbuff[17]=(txlen & 0xff);
			txbuff[18]=(txlen >> 8);

			if (txlen < 1 || txlen > 400)
				return 0;
			
			memcpy(&txbuff[19],&buff[7],txlen);

		}
		else
		{
			txlen=(buff[6]<<8) + buff[5];		// BPQEther is DOS-based - chain word is 2 bytes

			txlen-=2;

			txbuff[14]=(txlen & 0xff);
			txbuff[15]=(txlen >> 8);

			if (txlen < 1 || txlen > 400)
				return 0;


			memcpy(&txbuff[16],&buff[7],txlen);
		}

		memcpy(&txbuff[0], &IF->EthDest[0],6);
		memcpy(&txbuff[6], &IF->EthSource[0],6);
		memcpy(&txbuff[12], &IF->EtherType,2);

		txlen+=14;
		
		if (txlen < 60) txlen = 60;

		// Send down the packet 

		res = sendto(IF->s, txbuff, txlen, 0, 
	      (const struct sockaddr *)&IF->socket_address, sizeof(struct sockaddr_ll));

		if (res < 0)
		{
			perror("Eth Send");	
			return 3;
		}

//		if (tap_fd)
//			write(tap_fd, txbuff, txlen);

		return (0);

	case 3:				// CHECK IF OK TO SEND

		return (0);		// OK	

	case 4:				// reinit

		return 0;

	case 5:				// reinit

		return 0;
	}

	return (0);
}


UINT ETHERExtInit(struct PORTCONTROL *  PortEntry)
{
	//	Can have multiple ports, each mapping to a different Ethernet Adapter
	
	//	The Adapter number is in IOADDR
	//

	int i=0;
	u_int netmask;
	char buf[256];
	int n;
	struct ifreq ifr;
	size_t if_name_len;
	PCAPINFO * IF;
	int port = PortEntry->PORTNUMBER;
	u_long param=1;
    struct ifreq buffer;

	WritetoConsole("BPQEther ");

	//
	//	Read config 
	//

	if (!ReadConfigFile(port))
		return (FALSE);

	if_name_len = strlen(Adapter);

	IF = &PCAPInfo[port];

	IF->s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_BPQ));

	if (IF->s == -1)
	{
		perror("Open Ether Socket");
		IF->s = 0;
	}
	else
	{
		ioctl(IF->s, FIONBIO, &param);
		memcpy(ifr.ifr_name, Adapter, if_name_len);
		ifr.ifr_name[if_name_len] = 0;

		if (ioctl(IF->s, SIOCGIFINDEX,&ifr) == -1)
			perror("Get IF Number");

		// Get MAC Address

		memset(&buffer, 0x00, sizeof(buffer));

		strcpy(buffer.ifr_name, Adapter);
	    ioctl(IF->s, SIOCGIFHWADDR, &buffer);
		memcpy(IF->EthSource, buffer.ifr_hwaddr.sa_data, 6);
	}

	n=sprintf(buf,"Using %s = Interface %d\n", Adapter, ifr.ifr_ifindex);
	WritetoConsole(buf);

	/*prepare sockaddr_ll*/

	/*RAW communication*/
	IF->socket_address.sll_family = PF_PACKET;	

	/*we don't use a protocoll above ethernet layer ->just use anything here*/
	IF->socket_address.sll_protocol = htons(ETH_P_IP);	

	//index of the network device

	IF->socket_address.sll_ifindex = ifr.ifr_ifindex;

	/*ARP hardware identifier is ethernet*/
	IF->socket_address.sll_hatype   = ARPHRD_ETHER;
	
	/*target is another host*/
	IF->socket_address.sll_pkttype  = PACKET_BROADCAST;

	/*address length*/
	IF->socket_address.sll_halen    = ETH_ALEN;		
	/*MAC - begin*/

	memcpy(IF->socket_address.sll_addr, IF->EthDest, 6);
	IF->socket_address.sll_addr[6]  = 0x00;/*not used*/
	IF->socket_address.sll_addr[7]  = 0x00;/*not used*/


//	n=sprintf(buf,"Using %s Adapter = Interface %d\r", ifr.ifr_ifindex);
//	WritetoConsole(buf);

	return ((int) ExtProc);
}



static BOOL ReadConfigFile(int Port)
{
//TYPE	1	08FF                            # Ethernet Type
//ETH	1	FF:FF:FF:FF:FF:FF				#	Target Ethernet AddrMAP G8BPQ-7 10.2.77.1                  # IP 93 for compatibility
//ADAPTER	1	\Device\NPF_{21B601E8-8088-4F7D-96 29-EDE2A9243CF4}	# Adapter Name

	char buf[256],errbuf[256];
	char * Config;

	Config = PortConfig[Port];

	PCAPInfo[Port].Promiscuous = 1;				// Default
	PCAPInfo[Port].EtherType=htons(0x08FF);		// Default
	memset(PCAPInfo[Port].EthDest, 0xff, 6); 


	if (Config)
	{
		// Using config from bpq32.cfg

		char * ptr1 = Config, * ptr2;

		ptr2 = strchr(ptr1, 13);
		while(ptr2)
		{
			memcpy(buf, ptr1, ptr2 - ptr1);
			buf[ptr2 - ptr1] = 0;
			ptr1 = ptr2 + 2;
			ptr2 = strchr(ptr1, 13);

			strcpy(errbuf,buf);			// save in case of error
	
			if (!ProcessLine(buf, Port, FALSE))
			{
				WritetoConsole("BPQEther - Bad config record ");
				WritetoConsole(errbuf);
				WritetoConsole("\n");
			}
		}
		return (TRUE);
	}
		
	n=sprintf(buf,"No config info found in bpq32.cfg\n");
	WritetoConsole(buf);

	return (FALSE);
}


static ProcessLine(char * buf, int Port, BOOL CheckPort)
{
	char * ptr;
	char * p_port;
	char * p_mac;
	char * p_Adapter;
	char * p_type;

	int	port;
	int a,b,c,d,e,f,num;

	ptr = strtok(buf, " \t\n\r");

	if(ptr == NULL) return (TRUE);

	if(*ptr =='#') return (TRUE);			// comment

	if(*ptr ==';') return (TRUE);			// comment

	if (CheckPort)
	{
		p_port = strtok(NULL, " \t\n\r");
			
		if (p_port == NULL) return (FALSE);

		port = atoi(p_port);

		if (Port != port) return TRUE;		// Not for us
	}

	if(_stricmp(ptr,"ADAPTER") == 0)
	{
		p_Adapter = strtok(NULL, " \t\n\r");
		
		strcpy(Adapter,p_Adapter);
		return (TRUE);
	}

	if(_stricmp(ptr,"TYPE") == 0)
	{
		p_type = strtok(NULL, " \t\n\r");
		
		if (p_type == NULL) return (FALSE);

		num=sscanf(p_type,"%x",&a);

		if (num != 1) return FALSE;

		PCAPInfo[Port].EtherType=htons(a);
		return (TRUE);

	}

	if(_stricmp(ptr,"promiscuous") == 0)
	{
		ptr = strtok(NULL, " \t\n\r");
		
		if (ptr == NULL) return (FALSE);

		PCAPInfo[Port].Promiscuous = atoi(ptr);

		return (TRUE);

	}

	if(_stricmp(ptr,"RXMODE") == 0)
	{
		p_port = strtok(NULL, " \t\n\r");
			
		if (p_port == NULL) return (FALSE);

		if(_stricmp(p_port,"RLI") == 0)
		{
			PCAPInfo[Port].RLIRX=TRUE;
			return (TRUE);
		}

		if(_stricmp(p_port,"BPQ") == 0)
		{
			PCAPInfo[Port].RLIRX=FALSE;
			return (TRUE);
		}

		return FALSE;
	
	}

	if(_stricmp(ptr,"TXMODE") == 0)
	{
		p_port = strtok(NULL, " \t\n\r");
			
		if (p_port == NULL) return (FALSE);

		if(_stricmp(p_port,"RLI") == 0)
		{
			PCAPInfo[Port].RLITX=TRUE;
			return (TRUE);
		}

		if(_stricmp(p_port,"BPQ") == 0)
		{
			PCAPInfo[Port].RLITX=FALSE;
			return (TRUE);
		}

		return FALSE;

	}

	if(_stricmp(ptr,"DEST") == 0)
	{
		p_mac = strtok(NULL, " \t\n\r");
		
		if (p_mac == NULL) return (FALSE);

		num=sscanf(p_mac,"%x-%x-%x-%x-%x-%x",&a,&b,&c,&d,&e,&f);

		if (num != 6) return FALSE;

		PCAPInfo[Port].EthDest[0]=a;
		PCAPInfo[Port].EthDest[1]=b;
		PCAPInfo[Port].EthDest[2]=c;
		PCAPInfo[Port].EthDest[3]=d;
		PCAPInfo[Port].EthDest[4]=e;
		PCAPInfo[Port].EthDest[5]=f;


	//	strcpy(Adapter,p_Adapter);

		return (TRUE);
	}

	if(_stricmp(ptr,"SOURCE") == 0)		// not used, but ignore
		return (TRUE);

	//
	//	Bad line
	//
	return (FALSE);
	
}
	
